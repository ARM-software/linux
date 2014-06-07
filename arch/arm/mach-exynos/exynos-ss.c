/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot for debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <plat/cpu.h>
#include <mach/regs-pmu.h>
#include <mach/exynos-ss.h>

#ifdef CONFIG_EXYNOS_SNAPSHOT

/*  Size of Domain */
#define ESS_HEADER_SZ			SZ_4K
#define ESS_MMU_REG_SZ			SZ_4K
#define ESS_CORE_REG_SZ			SZ_4K
#define ESS_HEADER_TOTAL_SZ		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ)
#define ESS_LOG_MEM_SZ			SZ_4M
#define ESS_HOOK_LOGBUF_SZ		SZ_2M

#define ESS_HOOK_LOGGER_MAIN_SZ		(SZ_2M + SZ_1M)
#define ESS_HOOK_LOGGER_SYSTEM_SZ	SZ_1M
#define ESS_HOOK_LOGGER_EVENTS_SZ	0
#define ESS_HOOK_LOGGER_RADIO_SZ	0

#define ESS_LOG_STRING_LENGTH		SZ_128
#define ESS_MMU_REG_OFFSET		SZ_256
#define ESS_CORE_REG_OFFSET		SZ_256
#define ESS_LOG_MAX_NUM			SZ_2K

/*  Virtual Address Information */
#define S5P_VA_SS_BASE			S3C_ADDR(0x03000000)
#define S5P_VA_SS_LOGMEM		(S5P_VA_SS_BASE)

#define S5P_VA_SS_HEADER		(S5P_VA_SS_LOGMEM)
#define S5P_VA_SS_SCRATCH		(S5P_VA_SS_LOGMEM + 0x10)
#define S5P_VA_SS_LAST_LOGBUF		(S5P_VA_SS_LOGMEM + 0x14)
#define S5P_VA_SS_MMU_REG		(S5P_VA_SS_HEADER + ESS_HEADER_SZ)
#define S5P_VA_SS_CORE_REG		(S5P_VA_SS_MMU_REG + ESS_MMU_REG_SZ)

/*  logger mandotory */
#define S5P_VA_SS_LOGBUF		(S5P_VA_SS_LOGMEM + ESS_LOG_MEM_SZ)
#define S5P_VA_SS_LOGGER_MAIN		(S5P_VA_SS_LOGBUF + ESS_HOOK_LOGBUF_SZ)
#define S5P_VA_SS_LOGGER_SYSTEM		(S5P_VA_SS_LOGGER_MAIN + ESS_HOOK_LOGGER_MAIN_SZ)

/*  logger option */
#define S5P_VA_SS_LOGGER_RADIO		(S5P_VA_SS_LOGGER_SYSTEM + ESS_HOOK_LOGGER_SYSTEM_SZ)
#define S5P_VA_SS_LOGGER_EVENTS		(S5P_VA_SS_LOGGER_RADIO + ESS_HOOK_LOGGER_RADIO_SZ)

/*  Physical Address Information */
#define S5P_PA_SS_BASE(x)		(x)
#define S5P_PA_SS_LOGMEM(x)		(S5P_PA_SS_BASE(x))
#define S5P_PA_SS_HEADER(x)		(S5P_PA_SS_LOGMEM(x))
#define S5P_PA_SS_MMU_REG(x)		(S5P_PA_SS_HEADER(x) + ESS_HEADER_SZ)
#define S5P_PA_SS_CORE_REG(x)		(S5P_PA_SS_MMU_REG(x) + ESS_MMU_REG_SZ)

/*  logger mandotory */
#define S5P_PA_SS_LOGBUF(x)		(S5P_PA_SS_LOGMEM(x) + ESS_LOG_MEM_SZ)
#define S5P_PA_SS_LOGGER_MAIN(x)	(S5P_PA_SS_LOGBUF(x) + ESS_HOOK_LOGBUF_SZ)
#define S5P_PA_SS_LOGGER_SYSTEM(x)	(S5P_PA_SS_LOGGER_MAIN(x) + ESS_HOOK_LOGGER_MAIN_SZ)

/*  logger option */
#define S5P_PA_SS_LOGGER_RADIO(x)	(S5P_PA_SS_LOGGER_SYSTEM(x) + ESS_HOOK_LOGGER_SYSTEM_SZ)
#define S5P_PA_SS_LOGGER_EVENTS(x)	(S5P_PA_SS_LOGGER_RADIO(x) + ESS_HOOK_LOGGER_RADIO_SZ)

struct exynos_ss_hook_item {
	unsigned char *head_ptr;
	unsigned char *curr_ptr;
	size_t bufsize;
};

struct exynos_ss_hook {
	struct exynos_ss_hook_item logbuf;
	struct exynos_ss_hook_item logger_main;
	struct exynos_ss_hook_item logger_system;
	struct exynos_ss_hook_item logger_radio;
	struct exynos_ss_hook_item logger_events;
};

struct exynos_ss_log {
	struct task_log {
		unsigned long long time;
		char comm[TASK_COMM_LEN];
		pid_t pid;
	} task[NR_CPUS][ESS_LOG_MAX_NUM];
	struct irq_log {
		unsigned long long time;
		int irq;
		void *fn;
		int en;
	} irq[NR_CPUS][ESS_LOG_MAX_NUM];
	struct work_log {
		unsigned long long time;
		struct worker *worker;
		struct work_struct *work;
		work_func_t f;
		int en;
	} work[NR_CPUS][ESS_LOG_MAX_NUM];
	struct reg_log {
		unsigned long long time;
		int read;
		unsigned int val;
		unsigned int reg;
		int en;
	} reg[NR_CPUS][ESS_LOG_MAX_NUM];
	struct printkl_log {
		unsigned long long time;
		int cpu;
		unsigned int msg;
		unsigned int val;
	} printkl[ESS_LOG_MAX_NUM * 2];
	struct printk_log {
		unsigned long long time;
		int cpu;
		char log[ESS_LOG_STRING_LENGTH];
	} printk[ESS_LOG_MAX_NUM / 2];
	struct hrtimer_log {
		unsigned long long time;
		unsigned long long now;
		struct hrtimer *timer;
		enum hrtimer_restart (*fn)(struct hrtimer *);
		int en;
	} hrtimers[NR_CPUS][ESS_LOG_MAX_NUM];

	atomic_t task_log_idx[NR_CPUS];
	atomic_t irq_log_idx[NR_CPUS];
	atomic_t work_log_idx[NR_CPUS];
	atomic_t reg_log_idx[NR_CPUS];
	atomic_t hrtimer_log_idx[NR_CPUS];
	atomic_t printkl_log_idx;
	atomic_t printk_log_idx;
};

struct exynos_ss_mmu_reg {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};

struct exynos_ss_core_reg {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;

	/* SVC */
	unsigned int r13_svc;
	unsigned int r14_svc;
	unsigned int spsr_svc;

	/* PC & CPSR */
	unsigned int pc;
	unsigned int cpsr;

	/* USR/SYS */
	unsigned int r13_usr;
	unsigned int r14_usr;

	/* FIQ */
	unsigned int r8_fiq;
	unsigned int r9_fiq;
	unsigned int r10_fiq;
	unsigned int r11_fiq;
	unsigned int r12_fiq;
	unsigned int r13_fiq;
	unsigned int r14_fiq;
	unsigned int spsr_fiq;

	/* IRQ */
	unsigned int r13_irq;
	unsigned int r14_irq;
	unsigned int spsr_irq;

	/* MON */
	unsigned int r13_mon;
	unsigned int r14_mon;
	unsigned int spsr_mon;

	/* ABT */
	unsigned int r13_abt;
	unsigned int r14_abt;
	unsigned int spsr_abt;

	/* UNDEF */
	unsigned int r13_und;
	unsigned int r14_und;
	unsigned int spsr_und;
};

struct exynos_ss_interface {
	struct exynos_ss_log *info_log;
	struct exynos_ss_hook info_hook;
};

extern void (*arm_pm_restart)(char str, const char *cmd);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
extern void register_hook_logbuf(void (*)(const char));
#else
extern void register_hook_logbuf(void (*)(const char *, u64, size_t));
#endif
extern void register_hook_logger(void (*)(const char *, const char *, size_t));
static struct exynos_ss_log *ess_log = NULL;
static struct exynos_ss_hook ess_hook;

/*  External Interface Variable For T32 debugging */
static struct exynos_ss_interface ess_info;

/*  internal interface variable */
static int ess_enable = 0;
static unsigned int ess_phy_addr = 0;
static unsigned int ess_virt_addr = 0;
static unsigned int ess_size = 0;

DEFINE_PER_CPU(struct exynos_ss_core_reg *, ess_core_reg);
DEFINE_PER_CPU(struct exynos_ss_mmu_reg *, ess_mmu_reg);
DEFINE_PER_CPU(enum ess_cause_emerg_events, ess_cause_emerg);

static void exynos_ss_save_core(struct exynos_ss_core_reg *core_reg)
{
	asm("str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
	    "mov r0, %0\n\t"		/* R0 will be alias for core_reg */
	    "str r1, [r0,#4]\n\t"	/* R1 */
	    "str r2, [r0,#8]\n\t"	/* R2 */
	    "str r3, [r0,#12]\n\t"	/* R3 */
	    "str r4, [r0,#16]\n\t"	/* R4 */
	    "str r5, [r0,#20]\n\t"	/* R5 */
	    "str r6, [r0,#24]\n\t"	/* R6 */
	    "str r7, [r0,#28]\n\t"	/* R7 */
	    "str r8, [r0,#32]\n\t"	/* R8 */
	    "str r9, [r0,#36]\n\t"	/* R9 */
	    "str r10, [r0,#40]\n\t"	/* R10 */
	    "str r11, [r0,#44]\n\t"	/* R11 */
	    "str r12, [r0,#48]\n\t"	/* R12 */
	    /* SVC */
	    "str r13, [r0,#52]\n\t"	/* R13_SVC */
	    "str r14, [r0,#56]\n\t"	/* R14_SVC */
	    "mrs r1, spsr\n\t"		/* SPSR_SVC */
	    "str r1, [r0,#60]\n\t"
	    /* PC and CPSR */
	    "sub r1, r15, #0x4\n\t"	/* PC */
	    "str r1, [r0,#64]\n\t"
	    "mrs r1, cpsr\n\t"		/* CPSR */
	    "str r1, [r0,#68]\n\t"
	    /* SYS/USR */
	    "mrs r1, cpsr\n\t"		/* switch to SYS mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1f\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#72]\n\t"	/* R13_USR */
	    "str r14, [r0,#76]\n\t"	/* R14_USR */
	    /* FIQ */
	    "mrs r1, cpsr\n\t"		/* switch to FIQ mode */
	    "and r1,r1,#0xFFFFFFE0\n\t"
	    "orr r1,r1,#0x11\n\t"
	    "msr cpsr,r1\n\t"
	    "str r8, [r0,#80]\n\t"	/* R8_FIQ */
	    "str r9, [r0,#84]\n\t"	/* R9_FIQ */
	    "str r10, [r0,#88]\n\t"	/* R10_FIQ */
	    "str r11, [r0,#92]\n\t"	/* R11_FIQ */
	    "str r12, [r0,#96]\n\t"	/* R12_FIQ */
	    "str r13, [r0,#100]\n\t"	/* R13_FIQ */
	    "str r14, [r0,#104]\n\t"	/* R14_FIQ */
	    "mrs r1, spsr\n\t"		/* SPSR_FIQ */
	    "str r1, [r0,#108]\n\t"
	    /* IRQ */
	    "mrs r1, cpsr\n\t"		/* switch to IRQ mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x12\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#112]\n\t"	/* R13_IRQ */
	    "str r14, [r0,#116]\n\t"	/* R14_IRQ */
	    "mrs r1, spsr\n\t"		/* SPSR_IRQ */
	    "str r1, [r0,#120]\n\t"
	    /* MON */
	    "mrs r1, cpsr\n\t"		/* switch to monitor mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x16\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#124]\n\t"	/* R13_MON */
	    "str r14, [r0,#128]\n\t"	/* R14_MON */
	    "mrs r1, spsr\n\t"		/* SPSR_MON */
	    "str r1, [r0,#132]\n\t"
	    /* ABT */
	    "mrs r1, cpsr\n\t"		/* switch to Abort mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x17\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#136]\n\t"	/* R13_ABT */
	    "str r14, [r0,#140]\n\t"	/* R14_ABT */
	    "mrs r1, spsr\n\t"		/* SPSR_ABT */
	    "str r1, [r0,#144]\n\t"
	    /* UND */
	    "mrs r1, cpsr\n\t"		/* switch to undef mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1B\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#148]\n\t"	/* R13_UND */
	    "str r14, [r0,#152]\n\t"	/* R14_UND */
	    "mrs r1, spsr\n\t"		/* SPSR_UND */
	    "str r1, [r0,#156]\n\t"
	    /* restore to SVC mode */
	    "mrs r1, cpsr\n\t"		/* switch to SVC mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x13\n\t"
	    "msr cpsr,r1\n\t" :		/* output */
	    : "r"(core_reg)		/* input */
	    : "%r0", "%r1"		/* clobbered registers */
	);
	return;
}

static void exynos_ss_save_mmu(struct exynos_ss_mmu_reg *mmu_reg)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
}

int exynos_ss_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	exynos_ss_save_mmu(per_cpu(ess_mmu_reg, smp_processor_id()));
	exynos_ss_save_core(per_cpu(ess_core_reg, smp_processor_id()));
	pr_emerg("exynos-snapshot: context saved(CPU:%d)\n", smp_processor_id());
	local_irq_restore(flags);
	flush_cache_all();
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_context);

static inline int exynos_ss_check_rb(struct exynos_ss_hook_item *hook,
						size_t size)
{
	unsigned int max, cur;

	max = (unsigned int)(hook->head_ptr + hook->bufsize);
	cur = (unsigned int)(hook->curr_ptr + size);

	if (cur > max)
		return -1;
	else
		return 0;
}

static inline void exynos_ss_hook_logger(const char *name,
					 const char *buf, size_t size)
{
	struct exynos_ss_hook_item *hook;

	if (!strcmp(name, "log_main"))
		hook = &ess_hook.logger_main;
	else if (!strcmp(name, "log_system"))
		hook = &ess_hook.logger_system;
#if ESS_HOOK_LOGGER_MAIN_SZ <= SZ_1M
	else if (!strcmp(name, "log_radio"))
		hook = &ess_hook.logger_radio;
	else if (!strcmp(name, "log_events"))
		hook = &ess_hook.logger_events;
#endif
	else
		return;

	if ((exynos_ss_check_rb(hook, size)))
		hook->curr_ptr = hook->head_ptr;

	memcpy(hook->curr_ptr, buf, size);
	hook->curr_ptr += size;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
static inline void exynos_ss_hook_logbuf(const char buf)
{
	unsigned int last_buf;
	if (ess_hook.logbuf.head_ptr && buf) {
		if (exynos_ss_check_rb(&ess_hook.logbuf, 1))
			ess_hook.logbuf.curr_ptr = ess_hook.logbuf.head_ptr;

		ess_hook.logbuf.curr_ptr[0] = buf;
		ess_hook.logbuf.curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (unsigned int)ess_hook.logbuf.curr_ptr;
		__raw_writel((last_buf & (SZ_16M - 1)) | ess_phy_addr, S5P_VA_SS_LAST_LOGBUF);
	}
}
#else
static inline void exynos_ss_hook_logbuf(const char *buf, u64 ts_nsec, size_t size)
{
	if (ess_hook.logbuf.head_ptr && buf && size) {
		unsigned long rem_nsec;
		unsigned int last_buf;
		size_t timelen = 0;

		if (exynos_ss_check_rb(&ess_hook.logbuf, size + 32))
			ess_hook.logbuf.curr_ptr = ess_hook.logbuf.head_ptr;

		rem_nsec = do_div(ts_nsec, 1000000000);

		/*  fixed exact size */
		timelen = snprintf(ess_hook.logbuf.curr_ptr, 32,
				"[%5lu.%06lu] ", (unsigned long)ts_nsec, rem_nsec / 1000);

		ess_hook.logbuf.curr_ptr += timelen;
		memcpy(ess_hook.logbuf.curr_ptr, buf, size);
		ess_hook.logbuf.curr_ptr += size;
		ess_hook.logbuf.curr_ptr[0] = '\n';
		ess_hook.logbuf.curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (unsigned int)ess_hook.logbuf.curr_ptr;
		__raw_writel((last_buf & (SZ_16M - 1)) | ess_phy_addr, S5P_VA_SS_LAST_LOGBUF);
	}
}
#endif

enum ess_cause_emerg_events {
	CAUSE_INVALID_DUMP = 0x00000000,
	CAUSE_KERNEL_PANIC = 0x00000001,
	CAUSE_FORCE_DUMP   = 0x0000000D,
	CAUSE_FORCE_REBOOT = 0x000000FF,
};

static void exynos_ss_scratch_reg(unsigned int val)
{
	__raw_writel(val, S5P_VA_SS_SCRATCH);
}

#if defined(CONFIG_EXYNOS_SNAPSHOT_FORCE_DUMP_MODE) || defined(CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT)
static void exynos_ss_report_cause_emerg(enum ess_cause_emerg_events event)
{
	per_cpu(ess_cause_emerg, smp_processor_id()) = event;
}
#endif

static int exynos_ss_reboot_handler(struct notifier_block *nb,
				    unsigned long l, void *p)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_FORCE_DUMP_MODE
	local_irq_disable();
	pr_emerg("exynos-snapshot: forced reboot [%s]\n", __func__);
	exynos_ss_report_cause_emerg(CAUSE_FORCE_DUMP);
	exynos_ss_save_context();
	flush_cache_all();
	arm_pm_restart(0, "reset");
	while(1);
#else
	pr_emerg("exynos-snapshot: normal reboot [%s]\n", __func__);
	exynos_ss_scratch_reg(CAUSE_INVALID_DUMP);
	flush_cache_all();
#endif
	return 0;
}

static int exynos_ss_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
	local_irq_disable();
	exynos_ss_report_cause_emerg(CAUSE_KERNEL_PANIC);
	pr_emerg("exynos-snapshot: panic - forced ramdump mode [%s]\n", __func__);
	exynos_ss_save_context();
	flush_cache_all();
	arm_pm_restart(0, "reset");
	while(1);
#else
	pr_emerg("exynos-snapshot: panic [%s]\n", __func__);
	flush_cache_all();
#endif
	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = exynos_ss_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = exynos_ss_panic_handler,
};

static unsigned int __init exynos_ss_remap(unsigned int base, unsigned int size)
{
	static struct map_desc ess_iodesc[] __initdata = {
		{
			.virtual	= (unsigned long)S5P_VA_SS_LOGMEM,
			.length		= ESS_LOG_MEM_SZ,
			.type		= MT_DEVICE,
		}, {
			.virtual	= (unsigned long)S5P_VA_SS_LOGBUF,
			.length		= ESS_HOOK_LOGBUF_SZ,
			.type		= MT_DEVICE,
		},
	};
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	static struct map_desc ess_iodesc_logger[] __initdata = {
		{
			.virtual        = (unsigned long)S5P_VA_SS_LOGGER_MAIN,
			.length         = ESS_HOOK_LOGGER_MAIN_SZ,
			.type           = MT_DEVICE,
		}, {
			.virtual        = (unsigned long)S5P_VA_SS_LOGGER_SYSTEM,
			.length         = ESS_HOOK_LOGGER_SYSTEM_SZ,
			.type           = MT_DEVICE,
#if ESS_HOOK_LOGGER_MAIN_SZ <= SZ_1M
		}, {
			.virtual        = (unsigned long)S5P_VA_SS_LOGGER_RADIO,
			.length         = ESS_HOOK_LOGGER_RADIO_SZ,
			.type           = MT_DEVICE,
		}, {
			.virtual        = (unsigned long)S5P_VA_SS_LOGGER_EVENTS,
			.length         = ESS_HOOK_LOGGER_EVENTS_SZ,
			.type           = MT_DEVICE,
#endif
		},
	};
#endif
	ess_iodesc[0].pfn = __phys_to_pfn(S5P_PA_SS_LOGMEM(base));
	ess_iodesc[1].pfn = __phys_to_pfn(S5P_PA_SS_LOGBUF(base));

	iotable_init(ess_iodesc, ARRAY_SIZE(ess_iodesc));
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	ess_iodesc_logger[0].pfn = __phys_to_pfn(S5P_PA_SS_LOGGER_MAIN(base));
	ess_iodesc_logger[1].pfn = __phys_to_pfn(S5P_PA_SS_LOGGER_SYSTEM(base));
#if ESS_HOOK_LOGGER_MAIN_SZ <= SZ_1M
	ess_iodesc_logger[2].pfn = __phys_to_pfn(S5P_PA_SS_LOGGER_RADIO(base));
	ess_iodesc_logger[3].pfn = __phys_to_pfn(S5P_PA_SS_LOGGER_EVENTS(base));
#endif
	iotable_init(ess_iodesc_logger, ARRAY_SIZE(ess_iodesc_logger));
#endif
	return (unsigned int)S5P_VA_SS_BASE;
}

static int __init exynos_ss_setup(char *str)
{
	unsigned int size = 0;
	unsigned long base = 0;

	if (kstrtoul(str, 0, &base))
		goto out;

	size = sizeof(struct exynos_ss_log);

	if (ESS_LOG_MEM_SZ < size)
		goto out;

	size = ESS_LOG_MEM_SZ + ESS_HOOK_LOGBUF_SZ;
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	size += ESS_HOOK_LOGGER_MAIN_SZ;
	size += ESS_HOOK_LOGGER_SYSTEM_SZ;
	size += ESS_HOOK_LOGGER_RADIO_SZ;
	size += ESS_HOOK_LOGGER_EVENTS_SZ;
#endif
	/*  allow only align 2Mbyte */
	if ((size & SZ_1M) != 0)
		goto out;

	if (!(reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE))) {
		ess_phy_addr = base;
		ess_virt_addr = exynos_ss_remap(base,size);
		ess_size = size;

		pr_info("exynos-snapshot: memory reserved complete - base:%08X, size: %08X\n",
				(unsigned int)base, (unsigned int)size);
		return 0;
	}
out:
	pr_err("exynos-snapshot: buffer reserved failed base:%08X, size:%08X\n",
			(unsigned int)base, (unsigned int)size);
	return -1;
}
__setup("ess_setup=", exynos_ss_setup);

/*
 *  Normally, exynos-snapshot has 2-types debug buffer - log and hook.
 *  hooked buffer is for log_buf of kernel and loggers of platform.
 *  Each buffer has 2Mbyte memory except loggers. Loggers is consist of 4
 *  division. Each logger has 1Mbytes.
 *  ---------------------------------------------------------------------
 *  - dummy data(4K):phy_addr, virtual_addr, buffer_size, magic_key	-
 *  ---------------------------------------------------------------------
 *  -		Cores MMU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		Cores CPU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		log buffer(3Mbyte - Headers(12K))			-
 *  ---------------------------------------------------------------------
 *  -		Hooked buffer of kernel's log_buf(2Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked main logger buffer of platform(3Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked system logger buffer of platform(1Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked radio logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked events logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 */
static int __init exynos_ss_output(void)
{
	pr_info("Exynos-SnapShot physical / virtual memoy layout-(mandotory):"
		"\n\theader        : phys:0x%08x virt:0x%08x"
		"\n\tmmu_reg       : phys:0x%08x virt:0x%08x"
		"\n\tcore_reg      : phys:0x%08x virt:0x%08x"
		"\n\tlog           : phys:0x%08x virt:0x%08x"
		"\n\tlogbuf        : phys:0x%08x virt:0x%08x\n",
	(unsigned int)S5P_PA_SS_HEADER(ess_phy_addr), (unsigned int)S5P_VA_SS_HEADER,
	(unsigned int)S5P_PA_SS_MMU_REG(ess_phy_addr), (unsigned int)S5P_VA_SS_MMU_REG,
	(unsigned int)S5P_PA_SS_CORE_REG(ess_phy_addr), (unsigned int)S5P_VA_SS_CORE_REG,
	(unsigned int)S5P_PA_SS_LOGMEM(ess_phy_addr), (unsigned int)S5P_VA_SS_LOGMEM,
	(unsigned int)S5P_PA_SS_LOGBUF(ess_phy_addr), (unsigned int)S5P_VA_SS_LOGBUF);
#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	pr_info("Exynos-SnapShot physical / virtual memoy layout-(option):"
		"\n\tlogger-main   : phys:0x%08x virt:0x%08x"
		"\n\tlogger-sys    : phys:0x%08x virt:0x%08x"
		"\n\tlogger-radio  : phys:0x%08x virt:0x%08x"
		"\n\tlogger-events  : phys:0x%08x virt:0x%08x\n",
	(unsigned int)S5P_PA_SS_LOGGER_MAIN(ess_phy_addr), (unsigned int)S5P_VA_SS_LOGGER_MAIN,
	(unsigned int)S5P_PA_SS_LOGGER_SYSTEM(ess_phy_addr), (unsigned int)S5P_VA_SS_LOGGER_SYSTEM,
#if ESS_HOOK_LOGGER_MAIN_SZ <= SZ_1M
	(unsigned int)S5P_PA_SS_LOGGER_RADIO(ess_phy_addr), (unsigned int)S5P_VA_SS_LOGGER_RADIO,
	(unsigned int)S5P_PA_SS_LOGGER_EVENTS(ess_phy_addr), (unsigned int)S5P_VA_SS_LOGGER_EVENTS);
#else
	0,0,0,0);
#endif
#endif
	return 0;
}

/*	Header dummy data(4K)
 *	-------------------------------------------------------------------------
 *		0		4		8		C
 *	-------------------------------------------------------------------------
 *	0	virt_addr	phy_addr	size		magic_code
 *	4	Scratch_val	logbuf_addr	0		0
 *	-------------------------------------------------------------------------
*/
static void __init exynos_ss_fixmap_header(void)
{
	/*  fill 0 to next to header */
	int i;
	unsigned int *addr = (unsigned int *)ess_virt_addr;

	*addr = (unsigned int)ess_virt_addr;
	addr++;
	*addr = (unsigned int)ess_phy_addr;
	addr++;
	*addr = (unsigned int)ess_size;
	addr++;
	*addr = 0xDBDBDBDB;

	/*  kernel log buf */
	ess_log = (struct exynos_ss_log *)(S5P_VA_SS_LOGMEM + ESS_HEADER_TOTAL_SZ);
	/*  set fake translation to virtual address to debug trace */
	ess_info.info_log = (struct exynos_ss_log *)(CONFIG_PAGE_OFFSET |
			    (0x0FFFFFFF & (S5P_PA_SS_LOGMEM(ess_phy_addr) +
					   ESS_HEADER_TOTAL_SZ)));

	atomic_set(&(ess_log->printk_log_idx), -1);
	atomic_set(&(ess_log->printkl_log_idx), -1);
	for (i = 0; i < NR_CPUS; i++) {
		atomic_set(&(ess_log->task_log_idx[i]), -1);
		atomic_set(&(ess_log->irq_log_idx[i]), -1);
		atomic_set(&(ess_log->work_log_idx[i]), -1);
		atomic_set(&(ess_log->reg_log_idx[i]), -1);
		atomic_set(&(ess_log->hrtimer_log_idx[i]), -1);

		per_cpu(ess_mmu_reg, i) = (struct exynos_ss_mmu_reg *)
					  (S5P_VA_SS_MMU_REG + i * ESS_MMU_REG_OFFSET);
		per_cpu(ess_core_reg, i) = (struct exynos_ss_core_reg *)
					   (S5P_VA_SS_CORE_REG + i * ESS_CORE_REG_OFFSET);
	}
	/*  initialize Logmem to 0 except only header */
	memset((unsigned int *)(S5P_VA_SS_LOGMEM + ESS_HEADER_SZ),
				0, ESS_LOG_MEM_SZ - ESS_HEADER_SZ);
}

static int __init exynos_ss_fixmap(void)
{
	unsigned int last_buf, align;

	/*  fixmap to header first */
	exynos_ss_fixmap_header();

	/*  load last_buf address */
	last_buf = (unsigned int)readl(S5P_VA_SS_LAST_LOGBUF);
	align = (last_buf & 0xFF000000);

	/*  check physical address offset */
	if (align == ((S5P_PA_SS_LOGBUF(ess_phy_addr) & 0xFF000000))) {
		/*  assumed valid address */
		ess_hook.logbuf.curr_ptr = (unsigned char*)
			((last_buf & (SZ_16M - 1)) |
			 (unsigned int)S5P_VA_SS_LOGBUF);
	}
	else {	/*  invalid address, set to first line */
		ess_hook.logbuf.curr_ptr = (unsigned char *)S5P_VA_SS_LOGBUF;

		/*  initialize logbuf to 0 */
		memset((unsigned int *)S5P_VA_SS_LOGBUF, 0, ESS_HOOK_LOGBUF_SZ);
	}

	ess_hook.logbuf.head_ptr = (unsigned char *)S5P_VA_SS_LOGBUF;
	ess_hook.logbuf.bufsize = ESS_HOOK_LOGBUF_SZ;

	ess_info.info_hook.logbuf.head_ptr =
			(unsigned char *)(CONFIG_PAGE_OFFSET |
			(0x0FFFFFFF & S5P_PA_SS_LOGBUF(ess_phy_addr)));
	ess_info.info_hook.logbuf.curr_ptr = NULL;
	ess_info.info_hook.logbuf.bufsize = ESS_HOOK_LOGBUF_SZ;

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
	/* logger - main */
	ess_hook.logger_main.head_ptr = (unsigned char *)S5P_VA_SS_LOGGER_MAIN;
	ess_hook.logger_main.curr_ptr = (unsigned char *)S5P_VA_SS_LOGGER_MAIN;
	ess_hook.logger_main.bufsize = ESS_HOOK_LOGGER_MAIN_SZ;
	ess_info.info_hook.logger_main.head_ptr =
			(unsigned char *)(CONFIG_PAGE_OFFSET |
			(0x0FFFFFFF & S5P_PA_SS_LOGGER_MAIN(ess_phy_addr)));
	ess_info.info_hook.logger_main.curr_ptr = NULL;
	ess_info.info_hook.logger_main.bufsize = ESS_HOOK_LOGGER_MAIN_SZ;

	/*  initialize logger main to 0 */
	memset((unsigned int *)S5P_VA_SS_LOGGER_MAIN, 0, ESS_HOOK_LOGGER_MAIN_SZ);

	/*  logger - system */
	ess_hook.logger_system.head_ptr = (unsigned char *)S5P_VA_SS_LOGGER_SYSTEM;
	ess_hook.logger_system.curr_ptr = (unsigned char *)S5P_VA_SS_LOGGER_SYSTEM;
	ess_hook.logger_system.bufsize = ESS_HOOK_LOGGER_SYSTEM_SZ;
	ess_info.info_hook.logger_system.head_ptr =
			(unsigned char *)(CONFIG_PAGE_OFFSET |
			(0x0FFFFFFF & S5P_PA_SS_LOGGER_SYSTEM(ess_phy_addr)));
	ess_info.info_hook.logger_system.curr_ptr = NULL;
	ess_info.info_hook.logger_system.bufsize = ESS_HOOK_LOGGER_SYSTEM_SZ;

	/*  initialize logger system to 0 */
	memset((unsigned int *)S5P_VA_SS_LOGGER_SYSTEM, 0, ESS_HOOK_LOGGER_SYSTEM_SZ);

#if ESS_HOOK_LOGGER_MAIN_SZ <= SZ_1M
	/*  logger - radio */
	ess_hook.logger_radio.head_ptr = (unsigned char *)S5P_VA_SS_LOGGER_RADIO;
	ess_hook.logger_radio.curr_ptr = (unsigned char *)S5P_VA_SS_LOGGER_RADIO;
	ess_hook.logger_radio.bufsize = ESS_HOOK_LOGGER_RADIO_SZ;
	ess_info.info_hook.logger_radio.head_ptr =
			(unsigned char *)(CONFIG_PAGE_OFFSET |
			(0x0FFFFFFF & S5P_PA_SS_LOGGER_RADIO(ess_phy_addr)));
	ess_info.info_hook.logger_radio.curr_ptr = NULL;
	ess_info.info_hook.logger_radio.bufsize = ESS_HOOK_LOGGER_RADIO_SZ;

	/*  initialize logger radio to 0 */
	memset((unsigned int *)S5P_VA_SS_LOGGER_RADIO, 0, ESS_HOOK_LOGGER_RADIO_SZ);

	/*  logger - events */
	ess_hook.logger_events.head_ptr = (unsigned char *)S5P_VA_SS_LOGGER_EVENTS;
	ess_hook.logger_events.curr_ptr = (unsigned char *)S5P_VA_SS_LOGGER_EVENTS;
	ess_hook.logger_events.bufsize = ESS_HOOK_LOGGER_EVENTS_SZ;
	ess_info.info_hook.logger_events.head_ptr =
			(unsigned char *)(CONFIG_PAGE_OFFSET |
			(0x0FFFFFFF & S5P_PA_SS_LOGGER_EVENTS(ess_phy_addr)));
	ess_info.info_hook.logger_events.curr_ptr = NULL;
	ess_info.info_hook.logger_events.bufsize = ESS_HOOK_LOGGER_EVENTS_SZ;

	/*  initialize logger events to 0 */
	memset((unsigned int *)S5P_VA_SS_LOGGER_EVENTS, 0, ESS_HOOK_LOGGER_EVENTS_SZ);
#else
	memset(&ess_hook.logger_radio, 0, sizeof(ess_hook.logger_radio));
	memset(&ess_hook.logger_events, 0, sizeof(ess_hook.logger_events));
#endif
#endif
	exynos_ss_output();
	return 0;
}

static int __init exynos_ss_init(void)
{
	if (ess_virt_addr && ess_phy_addr) {
	/*
	 *  for debugging when we don't know the virtual address of pointer,
	 *  In just privous the debug buffer, It is added 16byte dummy data.
	 *  start address(dummy 16bytes)
	 *  --> @virtual_addr | @phy_addr | @buffer_size | @magic_key(0xDBDBDBDB)
	 *  And then, the debug buffer is shown.
	 */
		exynos_ss_fixmap();

		register_hook_logbuf(exynos_ss_hook_logbuf);

#ifdef CONFIG_EXYNOS_SNAPSHOT_HOOK_LOGGER
		register_hook_logger(exynos_ss_hook_logger);
#endif
		register_reboot_notifier(&nb_reboot_block);
		atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

		exynos_ss_scratch_reg(CAUSE_FORCE_DUMP);

		ess_enable = 1;
	} else
		pr_err("Exynos-SnapShot: %s failed\n", __func__);

	return 0;
}
early_initcall(exynos_ss_init);

void __exynos_ss_task(int cpu, struct task_struct *task)
{
	unsigned i;

	if (!ess_enable || !ess_log)
		return;

	i = atomic_inc_return(&ess_log->task_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->task[0]) - 1);
	ess_log->task[cpu][i].time = cpu_clock(cpu);
	strncpy(ess_log->task[cpu][i].comm, task->comm, TASK_COMM_LEN - 1);
	ess_log->task[cpu][i].pid = task->pid;
}

void __exynos_ss_irq(unsigned int irq, void *fn, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (!ess_enable || !ess_log)
		return;

	i = atomic_inc_return(&ess_log->irq_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->irq[0]) - 1);
	ess_log->irq[cpu][i].time = cpu_clock(cpu);
	ess_log->irq[cpu][i].irq = irq;
	ess_log->irq[cpu][i].fn = (void *)fn;
	ess_log->irq[cpu][i].en = en;
}

void __exynos_ss_work(struct worker *worker, struct work_struct *work,
			work_func_t f, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (!ess_enable || !ess_log)
		return;

	i = atomic_inc_return(&ess_log->work_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->work[0]) - 1);
	ess_log->work[cpu][i].time = cpu_clock(cpu);
	ess_log->work[cpu][i].worker = worker;
	ess_log->work[cpu][i].work = work;
	ess_log->work[cpu][i].f = f;
	ess_log->work[cpu][i].en = en;
}

void __exynos_ss_hrtimer(struct hrtimer *timer, s64 *now,
		     enum hrtimer_restart (*fn) (struct hrtimer *), int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (!ess_enable || !ess_log)
		return;

	i = atomic_inc_return(&ess_log->hrtimer_log_idx[cpu]) &
	    (ARRAY_SIZE(ess_log->hrtimers[0]) - 1);
	ess_log->hrtimers[cpu][i].time = cpu_clock(cpu);
	ess_log->hrtimers[cpu][i].now = *now;
	ess_log->hrtimers[cpu][i].timer = timer;
	ess_log->hrtimers[cpu][i].fn = fn;
	ess_log->hrtimers[cpu][i].en = en;
}

void __exynos_ss_reg(unsigned int read, unsigned int val,
			unsigned int reg, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (!ess_enable || !ess_log)
		return;

	i = atomic_inc_return(&ess_log->reg_log_idx[cpu]) &
		(ARRAY_SIZE(ess_log->reg[0]) - 1);

	ess_log->reg[cpu][i].time =
		(unsigned long long)(jiffies - INITIAL_JIFFIES) *
		(NSEC_PER_SEC / HZ);

	ess_log->reg[cpu][i].read = read;
	ess_log->reg[cpu][i].val = val;
	ess_log->reg[cpu][i].reg = reg;
	ess_log->reg[cpu][i].en = en;
}

void exynos_ss_printk(char *fmt, ...)
{
	va_list args;
	char buf[ESS_LOG_STRING_LENGTH];
	unsigned i;
	int cpu = raw_smp_processor_id();

	if (!ess_enable || !ess_log)
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	i = atomic_inc_return(&ess_log->printk_log_idx) &
	    (ARRAY_SIZE(ess_log->printk) - 1);
	ess_log->printk[i].time = cpu_clock(cpu);
	ess_log->printk[i].cpu = cpu;
	strncpy(ess_log->printk[i].log, buf, ESS_LOG_STRING_LENGTH - 1);
}

void exynos_ss_printkl(unsigned int msg, unsigned int val)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (!ess_enable || !ess_log)
		return;

	i = atomic_inc_return(&ess_log->printkl_log_idx) &
	    (ARRAY_SIZE(ess_log->printkl) - 1);

	ess_log->printkl[i].time = cpu_clock(cpu);
	ess_log->printkl[i].cpu = cpu;
	ess_log->printkl[i].msg = msg;
	ess_log->printkl[i].val = val;
}
#endif
