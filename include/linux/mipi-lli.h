/*
 * MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MIPI_LLI_H
#define __MIPI_LLI_H

#include <linux/miscdevice.h>
#include <linux/clk.h>

enum mipi_lli_link_status {
	LLI_RESET,
	LLI_MOUNTED,
	LLI_UNMOUNTED,
};

struct mipi_lli_ipc_handler {
	void *data;
	void (*handler)(void *, u32);
};

struct mipi_lli_clks {
	/* for gate/ungate clock */
	struct clk	*aclk_cpif_200;
	struct clk	*gate_cpifnm_200;
	struct clk	*gate_mphy_pll;
	struct clk	*gate_lli_svc_loc;
	struct clk	*gate_lli_svc_rem;
	struct clk	*gate_lli_ll_init;
	struct clk	*gate_lli_be_init;
	struct clk	*gate_lli_ll_targ;
	struct clk	*gate_lli_be_targ;
	struct clk	*gate_lli_cmn_cfg;
	struct clk	*gate_lli_tx0_cfg;
	struct clk	*gate_lli_rx0_cfg;
	struct clk	*gate_lli_tx0_symbol;
	struct clk	*gate_lli_rx0_symbol;

	/* for clk_set_parents */
	struct clk	*mout_phyclk_lli_tx0_symbol_user;
	struct clk	*phyclk_lli_tx0_symbol;
	struct clk	*mout_phyclk_lli_rx0_symbol_user;
	struct clk	*phyclk_lli_rx0_symbol;
	struct clk	*mout_mphy_pll;
	struct clk	*fout_mphy_pll;
};

struct mipi_lli {
	const struct lli_driver *driver;	/* hw-specific hooks */

	struct miscdevice misc_dev;

	/* Flags that need to be manipulated atomically because they can
	 * change while the host controller is running.  Always use
	 * set_bit() or clear_bit() to change their values.
	 */
	unsigned long		flags;

	struct mipi_lli_clks	clks;

	unsigned int		irq;		/* irq allocated */
	unsigned int		irq_sig;	/* irq_sig allocated */
	void __iomem		*regs;		/* device memory/io */
	void __iomem		*remote_regs;	/* device memory/io */
	void __iomem		*sys_regs;	/* device memory/io */
	void __iomem		*pmu_regs;	/* device memory/io */

	struct mutex		*lli_mutex;
	struct spin_lock	*lock;
	struct device		*dev;
	struct device		*mphy;

	void __iomem		*shdmem_addr;
	u32			shdmem_size;
	dma_addr_t		phy_addr;
	int			state;
	bool			is_master;
	bool			is_suspended;
	bool			is_runtime_suspended;

	struct mipi_lli_ipc_handler hd;
};

struct lli_driver {
	int	flags;

	int	(*init)(struct mipi_lli *lli);
	int	(*exit)(struct mipi_lli *lli);
	int	(*set_master)(struct mipi_lli *lli, bool is_master);
	int	(*link_startup_mount)(struct mipi_lli *lli);
	int	(*get_status)(struct mipi_lli *lli);

	int	(*send_signal)(struct mipi_lli *lli, u32 cmd);
	int	(*reset_signal)(struct mipi_lli *lli);
	int	(*read_signal)(struct mipi_lli *lli);

	int	(*suspend)(struct mipi_lli *lli);
	int	(*resume)(struct mipi_lli *lli);
};

extern int mipi_lli_add_driver(struct device *dev,
			       const struct lli_driver *lli_driver,
			       int irq);
extern void mipi_lli_remove_driver(struct mipi_lli *lli);

extern void __iomem *mipi_lli_request_sh_region(u32 sh_addr, u32 size);
extern void mipi_lli_release_sh_region(void *rgn);
extern unsigned long mipi_lli_get_phys_base(void);
extern unsigned long mipi_lli_get_phys_size(void);
extern int mipi_lli_get_link_status(void);
extern int mipi_lli_register_handler(void (*handler)(void *, u32), void *data);
extern int mipi_lli_unregister_handler(void (*handler)(void *, u32));
extern void mipi_lli_send_interrupt(u32 cmd);
extern void mipi_lli_reset_interrupt(void);
extern u32 mipi_lli_read_interrupt(void);
extern void mipi_lli_reload(void);
extern void mipi_lli_suspend(void);
extern void mipi_lli_resume(void);

#endif /* __MIPI_LLI_H */
