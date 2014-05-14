/*
 * ARM System Profiler driver
 *
 * Copyright (C) 2014 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) DRIVER_NAME ": " fmt

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/trace_seq.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "arm-system-profiler"
#define DEBUGFS_DIR_NAME "arm-system-profiler"

#define DEBUGFS_README_NAME "README"


/* register offsets for registers directly accessed by the driver */
#define SP_MONITOR_SIZE			0x10000
#define SP_MCTLR_OFFSET			(SP_MONITOR_SIZE * 0xf)
#define MCTLR_CTRL_OFFSET		4
#define MCTLR_MTR_EN_OFFSET		0xc
#define MCTLR_INT_STATUS_OFFSET		0x8014

/* MCTLR.CTRL bit definitions */
#define CTRL_SWACK			(1 << 2)

/* MCTLR.INT_* bit definitions */
#define INT_CAP_ACK			(1 << 3)


/*
 * Global top-level debugfs directory used for all system profiler
 * instances.
 */
static struct dentry *arm_system_profiler_debugfs_dir;


/* iterate over the elements of an array */
#define for_each(p, array)						\
	for ((p) = (array); (p) < (array) + ARRAY_SIZE(array); ++(p))

/*
 * Wrapper to export a string via debugfs_create_blob().
 *
 * The blob_wrapper is defined non-const because debugfs_create_blob
 * expects a non-const pointer, even though we don't expect this to
 * be modified and create the debugfs file read-only.
 *
 * Similarly, we cast off const from string to satisfy the debugfs: we
 * make sure the user perms don't allow write anyway when creating the
 * blob.
 */
#define DEFINE_BLOB(name, string)			\
	static struct debugfs_blob_wrapper name = {	\
		.data = (void *)(string),		\
		.size = ARRAY_SIZE(string) - 1,		\
	}


/* register definitions */

/* reg_desc describes one register for a specific monitor type */
struct reg_desc {
	unsigned int offset;	/* address offset from monitor register block */
	char const *name;	/* name for debugfs export and printk */
	uint32_t mask;		/* valid bits mask: zeroed bits are reserved */
	int flags;		/* permitted access type(s) (see REG_*) */
};

/* register access flags for reg_desc.flags */
#define REG_R (1 << 0)
#define REG_W (1 << 1)
#define REG_RW (REG_R | REG_W)

#define REGM(_name, _offset, _access, _mask) {	\
	.name = #_name,				\
	.offset = _offset,			\
	.flags = REG_##_access,			\
	.mask = _mask,				\
}

#define REG(name, offset, access)			\
	REGM(name, offset, access, ~(uint32_t)0)

static const struct reg_desc mctlr_regs[] = {
	REGM(CFG,	0x0000,	RW,	0x0000000f),
	REGM(CTRL,	MCTLR_CTRL_OFFSET,
				RW,	0x00000007),
	REGM(SP_STATUS,	0x0008,	R,	0x00000003),
	REGM(MTR_EN,	MCTLR_MTR_EN_OFFSET,
				RW,	0x8000ffff),
	REG(WIN_DUR,	0x0010,	RW),	/* mask is dynamic */
	REG(WIN_STATUS,	0x0014,	R),
	REGM(SECURE,	0x8000,	RW,	0x00000003),
	REGM(SECURE_STATUS,	0x8004,	R, 0x00000003),
	REGM(MTR_IM,	0x8008,	R,	0x0000ffff),
	REGM(TCFG,	0x800c,	RW,	0x0000007f),
	REGM(INT_EN,	0x8010,	RW,	0x0000000f),
	REGM(INT_STATUS,	MCTLR_INT_STATUS_OFFSET,
				RW,	0x0000000f),
	REGM(ITATBCTR0,	0xfefc,	W,	0x00000001),
	REGM(ITCTRL,	0xff00,	RW,	0x00000001),
	REGM(CLAIMSET,	0xffa0,	RW,	0x00000001),
	REGM(CLAIMCLR,	0xffa4,	RW,	0x00000001),
	REGM(AUTHSTATUS,	0xffb8,	R,	0x000000ff),
	REGM(DEVID,	0xffc8,	R,	0x000fffff),
	REGM(DEVTYPE,	0xffcc,	R,	0x000000ff),
	REG(PID4,	0xffd0, R),
	REG(PID5,	0xffd4, R),
	REG(PID6,	0xffd8, R),
	REG(PID7,	0xffdc, R),
	REG(PID0,	0xffe0, R),
	REG(PID1,	0xffe4, R),
	REG(PID2,	0xffe8, R),
	REG(PID3,	0xffec, R),
	REG(ID0,	0xfff0, R),
	REG(ID1,	0xfff4, R),
	REG(ID2,	0xfff8, R),
	REG(ID3,	0xfffc, R),
};

static const struct reg_desc abm_regs[] = {
	REGM(SR0,	0x0000,	R,	0x000fffff),
	REG(SR1,	0x0004,	R),
	REG(SR2,	0x0008,	R),
	REG(SR3,	0x000c,	R),
	REG(SR4,	0x0010,	R),
	REG(SR5,	0x0014,	R),
	REG(SR6,	0x0018,	R),
	REG(SR7,	0x001c,	R),
	REG(SR8,	0x0020,	R),
	REG(SR9,	0x0024,	R),
	REG(SR10,	0x0028,	R),
	REG(SR11,	0x002c,	R),
	/* SR12-SR31 currently reserved */
	REGM(CR1SEL,	0x0400, RW,	0x0000001f),
	REGM(CR2SEL,	0x0404, RW,	0x0000001f),
	REGM(CR3SEL,	0x0408, RW,	0x0000001f),
	REGM(CR4SEL,	0x040c, RW,	0x0000001f),
	REGM(CW1SEL,	0x0410, RW,	0x0000001f),
	REGM(CW2SEL,	0x0414, RW,	0x0000001f),
	REGM(CW3SEL,	0x0418, RW,	0x0000001f),
	REGM(CW4SEL,	0x041c, RW,	0x0000001f),
	REGM(AC1SEL,	0x0420, RW,	0x0000000f),
	REGM(AC2SEL,	0x0424, RW,	0x0000000f),
	REGM(AC3SEL,	0x0428, RW,	0x0000000f),
	REGM(ABM_CAP,	0x0d80,	R,	0xfcffffff),
	REGM(ABM_MODE,	0x0d84,	RW,	0x00000001),
	REGM(SEEE,	0x0d88,	R,	0x0000003f),
	REG(VNFLTCFG,	0x0d8c,	RW),	/* mask is dynamic */
	REGM(FILTCFG,	0x0d90,	RW,	0x7fffffff),
	REGM(FILTMSK,	0x0d94,	RW,	0x7ffe0fff),
	REGM(ACFILTCFG,	0x0d98,	RW,	0x00ffffff),
	REGM(FLTEN,	0x0d9c,	RW,	0x0007ffff),
	REG(IDFILT,	0x0da0,	RW),	/* mask is dynamic */
	REG(IDFILTM,	0x0da4,	RW),	/* mask is dynamic */
	REGM(TLAT,	0x0da8,	RW,	0x0001ffff),
	REGM(ATLAT,	0x0dac,	RW,	0x0001ffff),
	REG(ADDRH_UP,	0x0db0,	RW),	/* mask is dynamic */
	REG(ADDRH_LO,	0x0db4, RW),	/* mask is dynamic */
	REG(ADDRL_UP,	0x0db8,	RW),	/* mask is dynamic */
	REG(ADDRL_LO,	0x0dbc, RW),	/* mask is dynamic */
	REG(ACADDRH_UP,	0x0dc0,	RW),	/* mask is dynamic */
	REG(ACADDRH_LO,	0x0dc4, RW),	/* mask is dynamic */
	REG(ACADDRL_UP,	0x0dc8,	RW),	/* mask is dynamic */
	REG(ACADDRL_LO,	0x0dcc, RW),	/* mask is dynamic */
	REGM(EVT_ENABLE,	0x0dd0,	R,	0x00000003),
	REGM(MCFGR,	0x0e00,	R,	0x000fffff), /* mask is dynamic */
	REGM(MTR_CAP,	0x0fc8,	R,	0xff0000ff),
	REG(MID4,	0x0fd0,	R),
	REG(MID5,	0x0fd4,	R),
	REG(MID6,	0x0fd8,	R),
	REG(MID7,	0x0fdc,	R),
	REG(MID0,	0x0fe0,	R),
	REG(MID1,	0x0fe4,	R),
	REG(MID2,	0x0fe8,	R),
	REG(MID3,	0x0fec,	R),
};

static const struct reg_desc cpm_regs[] = {
	REGM(SR0,	0x0000,	R,	0x000fffff),
	REG(SR1,	0x0004,	R),
	REG(SR2,	0x0008,	R),
	REG(SR3,	0x000c,	R),
	REG(SR4,	0x0010,	R),
	REG(SR5,	0x0014,	R),
	REG(SR6,	0x0018,	R),
	REG(SR7,	0x001c,	R),
	REG(SR8,	0x0020,	R),
	REG(SR9,	0x0024,	R),
	REG(SR10,	0x0028,	R),
	REG(SR11,	0x002c,	R),
	/* SR12-SR31 currently reserved */
	REGM(EVSEL0,	0x0400,	RW,	0x0000ffff),
	REGM(EVSEL1,	0x0404,	RW,	0x0000ffff),
	REGM(EVSEL2,	0x0408,	RW,	0x0000ffff),
	REGM(EVSEL3,	0x040c,	RW,	0x0000ffff),
	REGM(EVSEL4,	0x0410,	RW,	0x0000ffff),
	REGM(EVSEL5,	0x0414,	RW,	0x0000ffff),
	REGM(EVSEL6,	0x0418,	RW,	0x0000ffff),
	REGM(EVSEL7,	0x041c,	RW,	0x0000ffff),
	REGM(EVSEL8,	0x0420,	RW,	0x0000ffff),
	REGM(EVSEL9,	0x0424,	RW,	0x0000ffff),
	REGM(EVSEL10,	0x0428,	RW,	0x0000ffff),
	REGM(EVSEL11,	0x042c,	RW,	0x0000ffff),
	REGM(CPM_CAP,	0x0d80,	R,	0x00000000),
	REGM(CPM_MODE,	0x0d84,	RW,	0x00000001),
	REGM(MCFGR,	0x0e00,	R,	0x0009ffff),
	REGM(MTR_CAP,	0x0fc8,	R,	0xff0000ff),
	REG(MID4,	0x0fd0,	R),
	REG(MID5,	0x0fd4,	R),
	REG(MID6,	0x0fd8,	R),
	REG(MID7,	0x0fdc,	R),
	REG(MID0,	0x0fe0,	R),
	REG(MID1,	0x0fe4,	R),
	REG(MID2,	0x0fe8,	R),
	REG(MID3,	0x0fec,	R),
};

static const struct reg_desc mpm_regs[] = {
	REG(SR0,	0x0000,	R),
	REG(SR1,	0x0004,	R),
	REG(SR2,	0x0008,	R),
	REG(SR3,	0x000c,	R),
	REG(SR4,	0x0010,	R),
	REG(SR5,	0x0014,	R),
	REG(SR6,	0x0018,	R),
	REG(SR7,	0x001c,	R),
	REG(SR8,	0x0020,	R),
	REG(SR9,	0x0024,	R),
	REG(SR10,	0x0028,	R),
	REG(SR11,	0x002c,	R),
	REG(SR12,	0x0030,	R),
	/* SR13-SR31 currently reserved */
	REGM(EVSEL0,	0x0400,	RW,	0x0000ffff),
	REGM(EVSEL1,	0x0404,	RW,	0x0000ffff),
	REGM(EVSEL2,	0x0408,	RW,	0x0000ffff),
	REGM(EVSEL3,	0x040c,	RW,	0x0000ffff),
	REGM(EVSEL4,	0x0410,	RW,	0x0000ffff),
	REGM(EVSEL5,	0x0414,	RW,	0x0000ffff),
	REGM(EVSEL6,	0x0418,	RW,	0x0000ffff),
	REGM(EVSEL7,	0x041c,	RW,	0x0000ffff),
	REGM(EVSEL8,	0x0420,	RW,	0x0000ffff),
	REGM(EVSEL9,	0x0424,	RW,	0x0000ffff),
	REGM(EVSEL10,	0x0428,	RW,	0x0000ffff),
	REGM(EVSEL11,	0x042c,	RW,	0x0000ffff),
	REG(MPM_CAP,	0x0d80,	R),
	REG(FCR0,	0x0d88,	RW),
	REG(FCR1,	0x0d8c,	RW),
	REG(FCR2,	0x0d90,	RW),
	REG(FCR3,	0x0d94,	RW),
	REG(FCR4,	0x0d98,	RW),
	REG(FCR5,	0x0d9c,	RW),
	REG(FCR6,	0x0da0,	RW),
	REG(FCR7,	0x0da4,	RW),
	REG(FCR8,	0x0da8,	RW),
	REG(FCR9,	0x0dac,	RW),
	REG(FCR10,	0x0db0,	RW),
	REG(FCR11,	0x0db4,	RW),
	REGM(MCFGR,	0x0e00,	R,	0x0009ffff),
	REGM(MTR_CAP,	0x0fc8,	R,	0xff0000ff),
	REG(MID4,	0x0fd0,	R),
	REG(MID5,	0x0fd4,	R),
	REG(MID6,	0x0fd8,	R),
	REG(MID7,	0x0fdc,	R),
	REG(MID0,	0x0fe0,	R),
	REG(MID1,	0x0fe4,	R),
	REG(MID2,	0x0fe8,	R),
	REG(MID3,	0x0fec,	R),
};


/* monitor type definitions */

/*
 * monitor_type_desc describes a single monitor type, binding a name
 * and description to the appropriate register set.
 */
struct monitor_type_desc {
	char const *name;	/* monitor type name for debugfs and printk */
	struct debugfs_blob_wrapper *name_blob;

	char const *dt_type;		/* device tree "type" property */

	char const *description;	/* verbose description for debugfs */
	struct debugfs_blob_wrapper *description_blob;

	unsigned int num_capture_regs; /* number of regs captured via ftrace */

	struct reg_desc const *regs;	/* register set for this monitor */
	unsigned int num_regs;		/* number of registers in regs */
};

#define MONITOR_TYPE_BLOBS(identifier, _regs, _name, _dt_type, _description, \
			   _num_capture_regs)				\
	static const char identifier##__name[] = _name;			\
	DEFINE_BLOB(identifier##__name_blob, identifier##__name);	\
									\
	static const char identifier##__description[] = _description;	\
	DEFINE_BLOB(identifier##__description_blob, identifier##__description);

#define MONITOR_TYPE_DEFINE(identifier, _regs, _name, _dt_type, _description, \
		     _num_capture_regs)					\
	{								\
		.name = _name,						\
		.name_blob = &identifier##__name_blob,			\
									\
		.dt_type = _dt_type,					\
									\
		.description = _description,				\
		.description_blob = &identifier##__description_blob,	\
		.num_capture_regs = _num_capture_regs,			\
									\
		.regs = _regs,						\
		.num_regs = ARRAY_SIZE(_regs),				\
	},

#define MONITOR_TYPE(_, identifier, regs, name, dt_type, description,	\
		     num_capture_regs)					\
	MONITOR_TYPE_##_(identifier, regs, name, dt_type, description,	\
			 num_capture_regs)

/* mctlr must be first in the list.  See configure_ports(). */
#define MONITORS_MCTLR_INDEX 0

#define MONITOR_TYPES(_)						\
	MONITOR_TYPE(_, montype_mctlr, mctlr_regs,			\
		    "mctlr", NULL, "Monitor Controller", 6)		\
	MONITOR_TYPE(_, montype_abm, abm_regs,				\
		    "abm", "axi", "AXI Bus Monitor", 12)		\
	MONITOR_TYPE(_, montype_cpm, cpm_regs,				\
		    "cpm", "cci", "CCI Performance Monitor", 13)	\
	MONITOR_TYPE(_, montype_mpm, mpm_regs,				\
		    "mpm", "memory", "Memory Controller Performance Monitor", \
		    13)
/*
 * MCTLR is not a kind of monitor, but it is convenient to describe it
 * in the same way.
 */

MONITOR_TYPES(BLOBS)

static const struct monitor_type_desc monitors[] = {
	MONITOR_TYPES(DEFINE)
};


/* port definitions */

/*
 * port_desc associates a numbered port on the system profiler with a
 * monitor type and a description of what it monitors.
 */
struct port_desc {
	int id;				/* port index on the profiler */
	struct monitor_type_desc const *type;	/* attached monitor type */
	unsigned int offset;		/* offset of monitor's register block */
	char const *description;	/* description of what is monitored */
	struct debugfs_blob_wrapper *description_blob;
};


/* runtime */

#define NUM_PORTS 16

/*
 * system_profiler_desc describes runtime context for a single system
 * profiler instance
 */
struct system_profiler_desc {
	struct platform_device *pdev;
	void __iomem *iomem;
	struct dentry *debugfs_dir;
	uint32_t serial; /* serial number to correlate parallal trace events */

	struct port_desc ports[NUM_PORTS];
	unsigned int num_ports;	/* number of ports actually connected */

	char *strings;	/* slab block containing all target descriptions */

	/* debugfs blob wrappers for the description strings: */
	struct debugfs_blob_wrapper blobs[NUM_PORTS];
};
/*
 * iomem points to the virtual mapping of all the system profiler's
 * registers, including all monitors.
 *
 * serial associates the multiple trace events that result from a single
 * capture.
 */


/* irq handling and trace */

#ifdef CONFIG_TRACEPOINTS
/*
 * print out capture data for an arm_system_profiler_capture trace event,
 * in the form {0x<value>,...}.
 */
static const char *arm_system_profiler_print_u32_array(
	struct trace_seq *p, u32 const *array, size_t count)
{
	const char *ret = p->buffer + p->len;
	const char *prefix = "";

	trace_seq_putc(p, '{');

	while (count--) {
		trace_seq_printf(p, "%s0x%x", prefix, (unsigned int)*array++);
		prefix = ",";
	}

	trace_seq_putc(p, '}');
	trace_seq_putc(p, 0);

	return ret;
}

/*
 * This function is called by the tracepoint to capture an individual
 * port's registers.  See <trace/events/arm-system-profiler.h>.
 */
static void capture_port(struct system_profiler_desc const *sp,
			 struct port_desc const *port,
			 u32 *data, unsigned int data_count)
{
	/* Assuming that capture registers start at offset 0 */
	uint32_t __iomem *p = sp->iomem + port->offset;
	unsigned int i;

	BUG_ON(port->type->num_capture_regs != data_count);

	for (i = 0; i < data_count; ++i)
		data[i] = readl(&p[i]);
}
#endif

#define CREATE_TRACE_POINTS
#include <trace/events/arm-system-profiler.h>

static irqreturn_t arm_system_profiler_interrupt(int irq, void *dev_id)
{
	struct system_profiler_desc *sp = dev_id;
	uint32_t status;
	uint32_t enabled_monitors_mask;
	struct port_desc const *port;

	status = readl(sp->iomem + SP_MCTLR_OFFSET + MCTLR_INT_STATUS_OFFSET);
	enabled_monitors_mask = readl(sp->iomem + SP_MCTLR_OFFSET +
				      MCTLR_MTR_EN_OFFSET);

	if (!status)
		return IRQ_NONE;

	/*
	 * Clear any pending interrupts.  If there was a capture
	 * (INT_CAP_ACK set), then we don't race with the next capture
	 * because it cannot begin until CTRL_SWACK is written to
	 * MCTLR.CTRL to acknowledge the capture.
	 */
	writel(status, sp->iomem + SP_MCTLR_OFFSET + MCTLR_INT_STATUS_OFFSET);

	/*
	 * Do nothing for trigger interrupts for now.  ftrace will still
	 * capture PID etc. when INT_CAP_ACK occurs, but it will lag the
	 * actual system profiler snapshot window a bit.  Too bad.
	 */
	if (!(status & INT_CAP_ACK))
		return IRQ_HANDLED;

	/*
	 * Process the capture data for each enabled monitor.  In
	 * theory, this can race with user modifications to
	 * MCTLR.MTR_EN, but changing MCTLR.MTR_EN requires the profiler
	 * to be disabled anyway -- protecting against this race alone
	 * is not worthwhile.
	 */
	for (port = sp->ports; port < sp->ports + sp->num_ports; ++port)
		if (port->id < 0 || enabled_monitors_mask & (1 << port->id))
			trace_arm_system_profiler_capture(sp, port);

	++sp->serial;
	/*
	 * Ensure the update is observable to any recipient of the next
	 * interrupt, but forcing the serial update to be observed before the
	 * SWACK write:
	 */
	smp_wmb();

	writel(CTRL_SWACK, sp->iomem + SP_MCTLR_OFFSET + MCTLR_CTRL_OFFSET);

	return IRQ_HANDLED;
}


/* register I/O for debugfs */

/*
 * amount of pending data we can buffer for an open file handle: This
 * needs to be enough to format any single hex word for read, or to
 * accept a reasonable representation of any single word for write.
 * Numbers with a redundant leading zeroes beyond the range of a word
 * are considered unreasonable.
 */
#define REG_FILE_BUF_SIZE 16

/*
 * reg_file_desc describes the static properties of a single file in
 * debugfs representing a register.
 */
struct reg_file_desc {
	struct system_profiler_desc const *sp;	/* which system profiler */
	struct port_desc const *port;	/* which port on the profiler */
	struct reg_desc const *reg;	/* which register */
};

/* reg_file_state holds the extra state for an open file handle */
struct reg_file_state {
	struct reg_file_desc const *reg_file;	/* which debugfs entry */
	char buf[REG_FILE_BUF_SIZE];	/* queued I/O data */
	size_t len;			/* number of valid characters in buf */
	int errno;			/* cached error code */
};
/*
 * errno caches any buffer overflow error so that we can report it in
 * release()
 */

/* file_operations for the register files */

static int reg_file_open(struct inode *i, struct file *f)
{
	int ret;
	struct reg_file_state *state;
	struct reg_file_desc const *reg_desc = i->i_private;
	struct reg_desc const *reg = reg_desc->reg;
	struct device *dev = &reg_desc->sp->pdev->dev;
	int acc_mode;

	/* Protect against module/driver unload while the file is open: */
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	get_device(dev);

	ret = -ENOMEM;
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		goto error;

	state->reg_file = i->i_private;
	f->private_data = state;

	dev_dbg(&reg_desc->sp->pdev->dev, "open with flags=0x%x\n", f->f_flags);

	acc_mode = f->f_flags & O_ACCMODE;
	if (acc_mode == O_RDONLY) {
		/*
		 * For files opened for read, read the underlying
		 * register immediately and format it for reading by the
		 * user process.
		 */
		uint32_t value = readl(reg_desc->sp->iomem +
				       reg_desc->port->offset + reg->offset);
		/*
		 * Don't police reserved bits for now, but at least
		 * print a warning:
		 */
		if (value & ~reg_desc->reg->mask)
			dev_info(dev,
				 "%s/%s: reserved bits nonzero on read (valid mask: 0x%08X, value read: 0x%08X)\n",
				 reg_desc->port->type->name,
				 reg_desc->reg->name,
				 (unsigned int)reg_desc->reg->mask,
				 (unsigned int)value);

		state->len = sprintf(state->buf, "0x%08x\n",
				     (unsigned int)value);

		dev_dbg(&reg_desc->sp->pdev->dev,
			"%s[%d]/%s = 0x%08x (len=%u)\n",
			reg_desc->port->type->name,
			reg_desc->port->id,
			reg_desc->reg->name,
			(unsigned int)value,
			(unsigned int)state->len);
	} else if (acc_mode != O_WRONLY) {
		/*
		 * Don't support simultaneous read/write.  It would add
		 * complexity and is rather useless because accessing a
		 * register is logically atomic.
		 */
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	put_device(dev);
	module_put(THIS_MODULE);

	/* See comment in reg_file_release(). */

	return ret;
}

/*
 * For files opened for write, reg_do_write performs the actual write to
 * the underlying register, once the data to write has been received
 * from userspace.
 */
static int reg_do_write(struct file *f)
{
	int ret;
	struct reg_file_state *state = f->private_data;
	struct reg_file_desc const *reg_desc = state->reg_file;
	struct device *dev = &reg_desc->sp->pdev->dev;
	unsigned long long val;

	if (state->errno)
		return state->errno;

	BUG_ON(state->len >= ARRAY_SIZE(state->buf));
	state->buf[state->len] = '\0';

	ret = kstrtoull(state->buf, 0, &val);
	if (ret)
		return ret;

	if (val > 0xFFFFFFFFU)
		return -ERANGE;

	if (val & ~reg_desc->reg->mask)
		dev_info(dev,
			 "%s/%s: reserved bits nonzero on write (valid mask: 0x%08X, value written: 0x%08X)\n",
			 reg_desc->port->type->name,
			 reg_desc->reg->name,
			 (unsigned int)reg_desc->reg->mask,
			 (unsigned int)val);

	writel(val, reg_desc->sp->iomem + reg_desc->port->offset +
			reg_desc->reg->offset);

	dev_dbg(dev, "Write 0x%08lx at 0x%08x\n",
		(unsigned long)val,
		reg_desc->port->offset + reg_desc->reg->offset);

	return 0;
}

static ssize_t reg_file_write(struct file *f, char __user const *up,
			      size_t size, loff_t *offset)
{
	ssize_t ret;
	struct reg_file_state *state = f->private_data;
	size_t count;

	if (!(state->reg_file->reg->flags & REG_W))
		return -EPERM;

	if (*offset >= REG_FILE_BUF_SIZE - 1) {
		state->errno = -EIO;
		return state->errno;
	}

	count = REG_FILE_BUF_SIZE - *offset;

	if (size > count)
		return -EIO;

	if (size < count)
		count = size;

	ret = copy_from_user(state->buf + *offset, up, count);
	if (ret < 0)
		return ret;

	*offset += count;
	if (*offset > state->len)
		state->len = *offset;

	return count;
}

static ssize_t reg_file_read(struct file *f, char __user *up,
			     size_t size, loff_t *offset)
{
	ssize_t ret;
	struct reg_file_state *state = f->private_data;
	size_t count;

	if (!(state->reg_file->reg->flags & REG_R))
		return -EPERM;

	if (*offset > state->len)
		return -EIO;

	count = state->len - *offset;

	if (size < count)
		count = size;

	ret = copy_to_user(up, state->buf + *offset, count);
	if (ret < 0)
		return ret;

	*offset += count;
	return count;
}

static int reg_file_release(struct inode *i, struct file *f)
{
	int ret = 0;
	struct reg_file_state *state = f->private_data;
	struct device *dev = &state->reg_file->sp->pdev->dev;

	if ((f->f_flags & O_ACCMODE) == O_WRONLY)
		ret = reg_do_write(f);

	kfree(f->private_data);
	put_device(dev);
	module_put(THIS_MODULE);

	/*
	 * Hope that the module doesn't get unloaded between here and
	 * the point when this function actually returns.  VFS doesn't
	 * (yet) provide a convenient solution for this, so this is the
	 * best we can do without disallowing module unload completely.
	 *
	 * Only root has access by default, so this is not a serious
	 * concern.  Hardened systems should not be mounting debugfs.
	 */

	return ret;
}

static const struct file_operations reg_file_fops = {
	.open = reg_file_open,
	.release = reg_file_release,
	.read = reg_file_read,
	.write = reg_file_write,
};


/*
 * FIXME: hack to enable event bus export from the CCI-400
 *
 * It would be cleaner for the arm-cci driver to provide an interface for
 * doing this, and we should only do this if the CCI is actually connected
 * to the system profiler.  Otherwise, this is harmless but unnecessary.
 * Note that this should not affect any other aspect of CCI operation.
 *
 * This will only work if the NIDEN input to CCI-400 is high.  For now,
 * assume that this is fixed in the hardware or will be pre-enabled by
 * firmware if appropriate: Linux running Non-secure will not have the
 * necessary access to enable it directly in most systems.
 */
#define CCI_PMCR_OFFSET 0x100
#define CCI_PMCR_EX	(1 << 4)

static int try_enable_cci_event_bus(struct device *dev)
{
	int ret = -ENODEV;
	struct device_node *np = of_find_compatible_node(
		NULL, NULL, "arm,cci-400");
	void __iomem *cci_base;

	if (!np) {
		dev_info(dev, "CCI-400 not found in device tree\n");
		goto error;
	}

	cci_base = of_iomap(np, 0);
	if (!cci_base) {
		dev_info(dev, "CCI-400 present, but cannot map registers\n");
		goto error;
	}

	writel(readl(cci_base + CCI_PMCR_OFFSET) | CCI_PMCR_EX,
	       cci_base + CCI_PMCR_OFFSET);

	ret = 0;
	dev_dbg(dev, "CCI PMCR = %08X\n",
		(unsigned int)readl(cci_base + CCI_PMCR_OFFSET));

	goto out;

error:
	dev_info(dev,
		 "CCI-400 event bus export failed.  CCI Performance Monitor may not work.\n");

out:
	if (cci_base)
		iounmap(cci_base);
	of_node_put(np);

	return ret;
}


/* per-instance debugfs setup */

static int debugfs_create_reg(struct dentry *parent,
			      struct system_profiler_desc const *sp,
			      struct port_desc const *port,
			      struct reg_desc const *reg)
{
	struct device *dev = &sp->pdev->dev;
	struct reg_file_desc *reg_desc =
		devm_kmalloc(dev, sizeof(*reg_desc), GFP_KERNEL);

	if (!reg_desc)
		return -ENOMEM;

	reg_desc->sp = sp;
	reg_desc->port = port;
	reg_desc->reg = reg;

	if (!debugfs_create_file(reg->name,
				 (reg->flags & REG_R ? S_IRUSR : 0) |
				 (reg->flags & REG_W ? S_IWUSR : 0),
				 parent, reg_desc, &reg_file_fops))
		return -ENOSPC;

	return 0;
}

static int debugfs_create_port(struct system_profiler_desc const *sp,
			       struct port_desc const *port)
{
	int ret = -ENOSPC;
	struct dentry *dir;
	struct reg_desc const *reg;

	if (port->id >= 0) {
		char name[16];

		sprintf(name, "port%d", port->id); /* FIXME */
		dir = debugfs_create_dir(name, sp->debugfs_dir);
		if (!dir)
			goto error;
	} else
		dir = sp->debugfs_dir;

	if (!debugfs_create_blob("type", S_IRUSR, dir,
				 port->type->name_blob) ||
	    !debugfs_create_blob("description", S_IRUSR, dir,
				 port->type->description_blob))
		goto error;

	if (port->description_blob)
		if (!debugfs_create_blob("target", S_IRUSR, dir,
					 port->description_blob))
			goto error;

	for (reg = port->type->regs;
	     reg < port->type->regs + port->type->num_regs; ++reg) {

		ret = debugfs_create_reg(dir, sp, port, reg);
		if (ret)
			goto error;
	}

	return 0;

error:
	return ret;
}

static int debugfs_create_profiler_instance(struct system_profiler_desc *sp)
{
	int ret;
	struct port_desc const *port;

	sp->debugfs_dir = debugfs_create_dir(sp->pdev->name,
					     arm_system_profiler_debugfs_dir);
	if (!sp->debugfs_dir)
		return -ENOSPC;

	for (port = sp->ports; port < sp->ports + sp->num_ports; ++port) {
		ret = debugfs_create_port(sp, port);
		if (ret)
			goto error;
	}

	return 0;

error:
	debugfs_remove_recursive(sp->debugfs_dir);

	return ret;
}


/* Resizable buffer for storing string data fetched from device tree: */
struct buf {
	char *p;
	size_t offset;	/* offset for appending */
	size_t size;	/* total size of buffer */
};

#define DEFINE_BUF(name) struct buf name = { NULL, 0, 0 }

#define BUF_MIN 64		/* initial guess for how much space we need */
#define BUF_MAX (1 << 10)	/* sanity limit on the size */

/* Append a string to the buffer, growing as required: */
static int buf_append(struct buf *b, char const *string)
{
	size_t len = strlen(string);
	size_t size = b->size;
	void *p;

	while (b->offset >= size || len >= size - b->offset) {
		if (size == BUF_MAX)
			return -ENOMEM;

		if (size == 0)
			size = BUF_MIN;
		else if (size > BUF_MAX / 2)
			size = BUF_MAX;
		else
			size *= 2;
	};

	if (size != b->size) {
		p = krealloc(b->p, size, GFP_KERNEL);
		if (!p)
			return -ENOMEM;

		b->p = p;
		b->size = size;
	}

	BUG_ON(b->offset > b->size || len >= b->size - b->offset);
	strcpy(b->p + b->offset, string);
	b->offset += len + 1;

	return 0;
}

/* Attempt to free unused space: */
static void buf_shrink(struct buf *b)
{
	void *p = krealloc(b->p, b->offset, GFP_KERNEL);

	if (p)
		b->p = p;
}

/* Free the buffer: */
static void buf_free(struct buf *b)
{
	kfree(b->p);
	b->p = NULL;
	b->offset = b->size = 0;
}


/* Parse a monitor node's "reg" property to get its ID: */
static int get_monitor_id(struct device_node const *np, int na)
{
	void const *prop;
	int len;
	u64 id;

	prop = of_get_property(np, "reg", &len);
	if (!prop)
		goto error;

	if (len != 4 * na)
		goto error;

	id = of_read_number(prop, na);
	if (id >= (u64)1 << 32)
		goto error;

	return id;

error:
	return -EINVAL;
}

/* Find the monitor type descriptor based on the monitor's type property: */
static struct monitor_type_desc const *find_monitor(char const *type)
{
	struct monitor_type_desc const *monitor;

	if (!type)
		return NULL;

	for_each (monitor, monitors) {
		if (!monitor->dt_type)
			continue;

		if (!strcmp(monitor->dt_type, type))
			return monitor;
	}

	return NULL;
}

/*
 * Add a monitor port to the list of known ports, based on the
 * monitor's device tree node:
*/
static int configure_next_port(struct system_profiler_desc *sp,
			       struct buf *strings, size_t *description_offset,
			       struct device_node *np)
{
	int ret = -EINVAL;
	int id;
	struct device *dev = &sp->pdev->dev;
	char const *type, *target;
	struct monitor_type_desc const *monitor;
	int na = of_n_addr_cells(np);

	id = get_monitor_id(np, na);
	if (id < 0) {
		dev_info(dev, "Bad or missing reg property on node %s\n",
			 np->full_name);
		goto error;
	}

	type = NULL;
	of_property_read_string(np, "type", &type);
	monitor = find_monitor(type);
	if (!monitor) {
		dev_info(dev,
			 "Missing or unknown type property on monitor node %s\n",
			 np->full_name);
		goto error;
	}

	if (!of_property_read_string(np, "target", &target)) {
		*description_offset = strings->offset;
		ret = buf_append(strings, target);
		if (ret) {
			dev_info(dev, "Out of memory\n");
			ret = -ENOMEM;
		}
	} else
		*description_offset = 0;

	sp->ports[sp->num_ports] = (struct port_desc){
		.id = id,
		.type = monitor,
		.offset = id * SP_MONITOR_SIZE,
		/*
		 * description and description_blob are initialised later,
		 * in configure_ports().
		 */
	};

	return 0;

error:
	return -EINVAL;
}

/*
 * Configure the list of ports on the profiler, based on the contents
 * of the profiler's device tree node:
 */
static int configure_ports(struct system_profiler_desc *sp)
{
	int ret;
	struct device_node *sp_node = sp->pdev->dev.of_node;
	struct device_node *np;
	size_t description_offsets[NUM_PORTS];
	DEFINE_BUF(strings);
	struct device *dev = &sp->pdev->dev;
	unsigned int port;

	BUILD_BUG_ON(NUM_PORTS < 1);

	sp->ports[0] = (struct port_desc){
		.id = -1,
		.type = &monitors[MONITORS_MCTLR_INDEX], /* MCTLR */
		.offset = SP_MCTLR_OFFSET,
		.description = NULL,
		.description_blob = NULL,
	};

	sp->num_ports = 1;

	ret = buf_append(&strings, "");
	if (ret) {
		dev_info(dev, "Out of memory\n");
		goto error;
	}

	for_each_child_of_node (sp_node, np) {
		if (strcmp(np->name, "monitor"))
			continue;

		if (sp->num_ports >= NUM_PORTS) {
			dev_info(dev, "Too many ports, ignoring node %s\n",
				 np->full_name);
			continue;
		}

		ret = configure_next_port(sp, &strings,
					  &description_offsets[sp->num_ports],
					  np);
		if (ret) {
			dev_info(dev, "Failed to configure port for node %s\n",
				 np->full_name);
			goto error;
		}

		++sp->num_ports;
	}

	if (sp->num_ports < 2) {
		dev_info(&sp->pdev->dev, "No usable ports found\n");
		ret = -ENOENT;
		goto error;
	}

	/* Free unused string memory: */
	if (strings.offset > 1)
		buf_shrink(&strings);
	else
		buf_free(&strings);

	/* Now that the strings won't move around, set up string pointers: */
	for (port = 1; port < sp->num_ports; ++port) {
		if (!description_offsets[port])
			continue;

		sp->ports[port].description =
			strings.p + description_offsets[port];
		sp->blobs[port] = (struct debugfs_blob_wrapper){
			/* cast off const: see DEFINE_BLOB() for explanation */
			.data = (char *)sp->ports[port].description,
			.size = strlen(sp->ports[port].description),
		};
		sp->ports[port].description_blob = &sp->blobs[port];
	}

	sp->strings = strings.p;

	return 0;

error:
	buf_free(&strings);
	return ret;
}


/* Helper to free resources not tracked by the device model: */
static void system_profiler_cleanup(struct system_profiler_desc *sp)
{
	if (!sp)
		return;

	kfree(sp->strings);
}


/* platform device hooks */

static int arm_system_profiler_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
	struct system_profiler_desc *sp;
	struct device *dev = &pdev->dev;

	sp = devm_kzalloc(dev, sizeof(*sp), GFP_KERNEL);
	if (!sp) {
		return -ENOMEM;
		goto error;
	}
	sp->pdev = pdev;

	ret = configure_ports(sp);
	if (ret)
		goto error;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sp->iomem = devm_ioremap_resource(dev, r);
	if (IS_ERR(sp->iomem))
		return PTR_ERR(sp->iomem);

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		dev_info(dev, "No IRQ found\n");
	else {
		ret = devm_request_irq(dev, ret, arm_system_profiler_interrupt,
				       IRQF_SHARED, pdev->name, sp);
		if (!ret)
			goto irq_ok;

		dev_info(dev, "request_irq failed (%d)\n", ret);
	}

	/*
	 * If we can't get an IRQ then the user can still poke
	 * registers.  This is less useful, but non-fatal.
	 */
	dev_info(dev, "Tracing unavailable.\n");

irq_ok:
	platform_set_drvdata(pdev, sp);

	ret = debugfs_create_profiler_instance(sp);
	if (ret)
		goto error;

	dev_info(dev, "probed with PA 0x%08lX-0x%08lX, VA 0x%08lX\n",
		 (unsigned long)r->start, (unsigned long)r->end,
		 (unsigned long)sp->iomem);

	try_enable_cci_event_bus(dev);

	return 0;

error:
	system_profiler_cleanup(sp);
	return ret;
}

static int arm_system_profiler_remove(struct platform_device *pdev)
{
	struct system_profiler_desc *sp = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "remove\n");

	debugfs_remove_recursive(sp->debugfs_dir);

	system_profiler_cleanup(sp);
	return 0;
}


/*
 * This help text is placed in DEBUGFS_DIR_NAME/DEBUGFS_README_NAME.  It
 * contains hints only -- complete documentation would be way too big
 * here.
 */
#define README_TEXT							\
"ARM System Profiler debugfs interface\n"				\
"\n"									\
"To access a system profiler register:\n"				\
"\n"									\
"# cd /sys/kernel/debug/arm-system-profiler\n"				\
"# cat <register>\n"							\
"# echo <value> ><register>\n"						\
"\n"									\
"<register> is either:\n"						\
"\t<device>/<REGISTER_NAME> (for MCTLR registers), or\n"		\
"\t<device>/port<X>/<REGISTER_NAME> (for monitor registers)\n"		\
"\n"									\
"<value> can be any 32-bit C integer literal generally accepted by\n"	\
"strtoul(3).  Note that when reading a register the output is always in\n" \
"hex with the conventional \"0x\" prefix, but values written are not\n"	\
"required to be expressed in hex.\n"					\
"\n"									\
"You can also read some files to discover how the profiler is configured:\n" \
"\t.../name: short name for the monitor type\n"				\
"\t.../description: descriptive name for the monitor type\n"		\
"\t.../target: described what the monitor is connected to, where\n"	\
"\t\tmeaningful.\n"							\
"\n"									\
"If you enable INT_CAP_ACK in INT_EN, then trace events will be\n"	\
"generated containing the captured data after each capture\n"		\
"automatically.  This is most useful when WT_EN and WT_CAP are\n"	\
"enabled in CFG and WIN_DUR is set to a suitable period, to\n"		\
"enable periodic capture using the window timer.  This will\n"		\
"generate a stream of periodic captures via ftrace.\n"

DEFINE_BLOB(arm_system_profiler_readme_blob, README_TEXT);


/* driver skeleton */

static const struct of_device_id arm_system_profiler_dt_match[] = {
	{ .compatible = "arm,system-profiler" },
	{}
};
MODULE_DEVICE_TABLE(of, arm_system_profiler_dt_match);

static struct platform_driver arm_system_profiler_driver = {
	.probe = arm_system_profiler_probe,
	.remove = arm_system_profiler_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(arm_system_profiler_dt_match),
	},
};


static int __init arm_system_profiler_init_module(void)
{
	static struct dentry *arm_system_profiler_debugfs_readme_file;
	int ret;

	arm_system_profiler_debugfs_dir =
		debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (!arm_system_profiler_debugfs_dir) {
		pr_info("Cannot create directory in debugfs\n");
		return -ENOSPC;
	}

	arm_system_profiler_debugfs_readme_file =
		debugfs_create_blob("README", S_IRUGO,
				    arm_system_profiler_debugfs_dir,
				    &arm_system_profiler_readme_blob);
	if (!arm_system_profiler_debugfs_readme_file) {
		ret = -ENOSPC;
		goto error_debugfs_dir;
	}

	ret = platform_driver_register(&arm_system_profiler_driver);
	if (!ret) {
		pr_debug("Module loaded successfully\n");
		return 0;
	}

error_debugfs_dir:
	debugfs_remove(arm_system_profiler_debugfs_dir);

	return ret;
}
module_init(arm_system_profiler_init_module);

static void __exit arm_system_profiler_exit_module(void)
{
	pr_debug("Module exiting\n");

	platform_driver_unregister(&arm_system_profiler_driver);
	debugfs_remove_recursive(arm_system_profiler_debugfs_dir);
}
module_exit(arm_system_profiler_exit_module);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dave Martin");
MODULE_DESCRIPTION("ARM System Profiler driver");
