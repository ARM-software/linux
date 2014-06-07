/*
 * Exynos MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mipi-lli.h>

#include "exynos-mipi-lli.h"
#include "exynos-mipi-lli-mphy.h"

#define EXYNOS_LLI_LINK_START		(0x4000)
/*
 * 5ns, Default System Clock 200MHz
 * SYSTEM_CLOCK_PERIOD = 1000MHz / System Clock
 */
#define SYSTEM_CLOCK_PERIOD		(5)
#define SIG_INT_MASK0			(0xFFFFFFFF)
#define SIG_INT_MASK1			(0xFFFFFFFF)

static int poll_bit_set(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = readl(ptr);
		if (reg & val)
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

static u32 exynos_lli_cal_remap(u32 base_addr, unsigned long size)
{
	u32 remap_addr, bit_pos;

	if (!size)
		return 0;

	bit_pos = find_first_bit(&size, 32);

	/* check size is pow of 2 */
	if (size != (1 << bit_pos))
		return 0;

	remap_addr = (base_addr >> bit_pos) << LLI_REMAP_BASE_ADDR_SHIFT;
	remap_addr |= bit_pos;
	remap_addr |= LLI_REMAP_ENABLE;

	return remap_addr;
}

static int exynos_lli_get_clk_info(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	clks->aclk_cpif_200 = devm_clk_get(lli->dev, "aclk_cpif_200");
	/* To gate/ungate clocks */
	clks->gate_cpifnm_200 = devm_clk_get(lli->dev, "gate_cpifnm_200");
	clks->gate_mphy_pll = devm_clk_get(lli->dev, "gate_mphy_pll");
	clks->gate_lli_svc_loc = devm_clk_get(lli->dev, "gate_lli_svc_loc");
	clks->gate_lli_svc_rem = devm_clk_get(lli->dev, "gate_lli_svc_rem");
	clks->gate_lli_ll_init = devm_clk_get(lli->dev, "gate_lli_ll_init");
	clks->gate_lli_be_init = devm_clk_get(lli->dev, "gate_lli_be_init");
	clks->gate_lli_ll_targ = devm_clk_get(lli->dev, "gate_lli_ll_targ");
	clks->gate_lli_be_targ = devm_clk_get(lli->dev, "gate_lli_be_targ");
	clks->gate_lli_cmn_cfg = devm_clk_get(lli->dev, "gate_lli_cmn_cfg");
	clks->gate_lli_tx0_cfg = devm_clk_get(lli->dev, "gate_lli_tx0_cfg");
	clks->gate_lli_rx0_cfg = devm_clk_get(lli->dev, "gate_lli_rx0_cfg");
	clks->gate_lli_tx0_symbol = devm_clk_get(lli->dev, "gate_lli_tx0_symbol");
	clks->gate_lli_rx0_symbol = devm_clk_get(lli->dev, "gate_lli_rx0_symbol");

	/* For mux selection of clocks */
	clks->mout_phyclk_lli_tx0_symbol_user = devm_clk_get(lli->dev,
			"mout_phyclk_lli_tx0_symbol_user");
	clks->phyclk_lli_tx0_symbol = devm_clk_get(lli->dev,
			"phyclk_lli_tx0_symbol");
	clks->mout_phyclk_lli_rx0_symbol_user = devm_clk_get(lli->dev,
			"mout_phyclk_lli_rx0_symbol_user");
	clks->phyclk_lli_rx0_symbol = devm_clk_get(lli->dev,
			"phyclk_lli_rx0_symbol");
	clks->mout_mphy_pll = devm_clk_get(lli->dev, "mout_mphy_pll");
	clks->fout_mphy_pll = devm_clk_get(lli->dev, "fout_mphy_pll");

	if (IS_ERR(clks->aclk_cpif_200) ||
		IS_ERR(clks->gate_cpifnm_200) ||
		IS_ERR(clks->gate_mphy_pll) ||
		IS_ERR(clks->gate_lli_svc_loc) ||
		IS_ERR(clks->gate_lli_svc_rem) ||
		IS_ERR(clks->gate_lli_ll_init) ||
		IS_ERR(clks->gate_lli_be_init) ||
		IS_ERR(clks->gate_lli_ll_targ) ||
		IS_ERR(clks->gate_lli_be_targ) ||
		IS_ERR(clks->gate_lli_cmn_cfg) ||
		IS_ERR(clks->gate_lli_tx0_cfg) ||
		IS_ERR(clks->gate_lli_rx0_cfg) ||
		IS_ERR(clks->gate_lli_tx0_symbol) ||
		IS_ERR(clks->gate_lli_rx0_symbol) ||
		IS_ERR(clks->mout_phyclk_lli_tx0_symbol_user) ||
		IS_ERR(clks->phyclk_lli_tx0_symbol) ||
		IS_ERR(clks->mout_phyclk_lli_rx0_symbol_user) ||
		IS_ERR(clks->phyclk_lli_rx0_symbol) ||
		IS_ERR(clks->mout_mphy_pll) ||
		IS_ERR(clks->fout_mphy_pll)
	) {
		dev_err(lli->dev, "exynos_lli_get_clks - failed\n");
		return -ENODEV;
	}

	return 0;
}

static void exynos_lli_system_config(struct mipi_lli *lli)
{
	if (lli->sys_regs) {
		writel(SIG_INT_MASK0, lli->sys_regs + CPIF_LLI_SIG_INT_MASK0);
		writel(SIG_INT_MASK1, lli->sys_regs + CPIF_LLI_SIG_INT_MASK1);
	}
}

static int exynos_lli_init(struct mipi_lli *lli)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);
	u32 remap_addr;

	/* update lli_link_state as reset */
	lli->state = LLI_RESET;

	exynos_lli_system_config(lli);
	/* enable LLI_PHY_CONTROL */
	writel(1, lli->pmu_regs);
	/* software reset */
	writel(1, lli->regs + EXYNOS_DME_LLI_RESET);

	/* set_system clk period */
	writel(SYSTEM_CLOCK_PERIOD, lli->regs + EXYNOS_PA_SYSTEM_CLK_PERIOD);
	/* Set DriveTactiveDuration */
	writel(0xf, lli->regs + EXYNOS_PA_DRIVE_TACTIVATE_DURATION);

	remap_addr = exynos_lli_cal_remap(lli->phy_addr, lli->shdmem_size);
	if (!remap_addr) {
		dev_err(lli->dev, "remap calculation error\n");
		return -EINVAL;
	}

	/* Un-set LL_INIT REMAP Address */
	writel(remap_addr, lli->regs + EXYNOS_IAL_LL_INIT_ADDR_REMAP);
	/* Set BE_INIT REMAP Address */
	writel(remap_addr, lli->regs + EXYNOS_IAL_BE_INIT_ADDR_REMAP);

	/* LLI Interrupt enable */
	writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

	/* Set BE TC enable */
	writel(0, lli->regs + EXYNOS_DME_BE_TC_DISABLE);
	/* Set Breaking point enable */
	writel(0, lli->regs + EXYNOS_PA_DBG_PA_BREAK_POINT_ENABLE);
	/* Set Error count enable */
	writel(1, lli->regs + EXYNOS_PA_PHIT_ERR_COUNT_ENABLE);
	/* Set Receive count enable */
	writel(1, lli->regs + EXYNOS_PA_PHIT_RECEIVE_COUNT_ENABLE);

	/* Set Acti Tx/Rx count enable */
	writel(1, lli->regs + EXYNOS_PA_TX_COUNT);
	writel(1, lli->regs + EXYNOS_PA_RX_COUNT);

	/*
	 Set Rx Latch
	 Edge and number of latches from M-RX to PA.
	 [0:7] where,
	 [0] : Rising edge. 1 cycle.
	 [1] : Rising edge. 2 cycles.
	 [2] : No latch (Bypass)
	 [3-7] : Reserved
	 */
	writel(2, lli->regs + EXYNOS_PA_PA_DBG_RX_LATCH_MODE);

	writel(0x40, lli->regs + EXYNOS_PA_NACK_RTT);
	writel(0x1, lli->regs + EXYNOS_PA_MK0_INSERTION_ENABLE);
	writel((128<<0) | (1<<12), lli->regs + EXYNOS_PA_MK0_CONTROL);
	/* Set Scrambler enable */
	writel(1, lli->regs + EXYNOS_PA_USR_SCRAMBLER_ENABLE);

	/* MPHY configuration */
	if (phy->init)
		phy->init(phy);

	if (phy->cmn_init) {
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
		phy->cmn_init(phy);
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	}

	if (phy->ovtm_init) {
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
		phy->ovtm_init(phy);
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	}

	/* Set SNF FIFO for LL&BE */
	writel(((0x1F<<1) | 1), lli->regs + EXYNOS_IAL_LL_SNF_FIFO);
	writel(((0x1F<<1) | 1), lli->regs + EXYNOS_IAL_BE_SNF_FIFO);

	writel(0x1000, lli->regs + EXYNOS_PA_WORSTCASE_RTT);
	writel(0x1, lli->regs + EXYNOS_PA_SLAVE_AUTOMOUNT);

	return 0;
}

static int exynos_lli_set_master(struct mipi_lli *lli, bool is_master)
{
	lli->is_master = is_master;

	writel((is_master << 1), lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);

	return 0;
}

static int exynos_lli_link_startup_mount(struct mipi_lli *lli)
{
	u32 regs;
	u32 ret = 0;

	if (lli->is_master) {
		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
		if (regs & LLI_MOUNTED) {
			pr_debug("LLI master already mounted\n");
			return ret;
		}

		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
		writel((0x1 << 2) | regs,
			lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	} else {
		ret = poll_bit_set(lli->regs + EXYNOS_DME_LLI_INTR_STATUS,
			     INTR_MPHY_HIBERN8_EXIT_DONE,
			     1000);
		if (ret) {
			dev_err(lli->dev, "HIBERN8 Exit Failed\n");
			return ret;
		}

		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
		writel((0x1 << 2) | regs,
			lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	}

	return ret;
}

static int exynos_lli_get_status(struct mipi_lli *lli)
{
	u32 regs;

	regs = readl(lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	return regs;
}

static int exynos_lli_send_signal(struct mipi_lli *lli, u32 cmd)
{
	writel(cmd, lli->remote_regs + EXYNOS_TL_SIGNAL_SET_LSB);

	return 0;
}

static int exynos_lli_reset_signal(struct mipi_lli *lli)
{
	writel(0xFFFFFFFF, lli->regs + EXYNOS_TL_SIGNAL_CLR_LSB);
	writel(0xFFFFFFFF, lli->regs + EXYNOS_TL_SIGNAL_CLR_MSB);

	return 0;
}

static int exynos_lli_read_signal(struct mipi_lli *lli)
{
	u32 intr_lsb, intr_msb;

	intr_lsb = readl(lli->regs + EXYNOS_TL_SIGNAL_STATUS_LSB);
	intr_msb = readl(lli->regs + EXYNOS_TL_SIGNAL_STATUS_MSB);

	if (intr_lsb)
		writel(intr_lsb, lli->regs + EXYNOS_TL_SIGNAL_CLR_LSB);
	if (intr_msb)
		writel(intr_msb, lli->regs + EXYNOS_TL_SIGNAL_CLR_MSB);

	/* TODO: change to dev_dbg */
	dev_info(lli->dev, "LSB = %x, MSB = %x\n", intr_lsb, intr_msb);

	return intr_lsb;
}

static int exynos_lli_clock_init(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if(!clks)
		return -EINVAL;

	clk_set_parent(clks->mout_phyclk_lli_tx0_symbol_user,
			clks->phyclk_lli_tx0_symbol);
	clk_set_parent(clks->mout_phyclk_lli_rx0_symbol_user,
			clks->phyclk_lli_rx0_symbol);
	clk_set_parent(clks->mout_mphy_pll, clks->fout_mphy_pll);

	return 0;
}

static int exynos_lli_clock_gating(struct mipi_lli *lli, int is_gating)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if(!clks)
		return -EINVAL;

	if (is_gating){
		clk_disable_unprepare(clks->gate_lli_rx0_symbol);
		clk_disable_unprepare(clks->gate_lli_tx0_symbol);
		clk_disable_unprepare(clks->gate_lli_rx0_cfg);
		clk_disable_unprepare(clks->gate_lli_tx0_cfg);
		clk_disable_unprepare(clks->gate_lli_cmn_cfg);
		clk_disable_unprepare(clks->gate_lli_be_targ);
		clk_disable_unprepare(clks->gate_lli_ll_targ);
		clk_disable_unprepare(clks->gate_lli_be_init);
		clk_disable_unprepare(clks->gate_lli_ll_init);
		clk_disable_unprepare(clks->gate_lli_svc_rem);
		clk_disable_unprepare(clks->gate_lli_svc_loc);
		clk_disable_unprepare(clks->gate_mphy_pll);
		clk_disable_unprepare(clks->gate_cpifnm_200);
		/* it doesn't gate/ungate aclk_cpif_200
		   clk_disable_unprepare(clks->aclk_cpif_200);
		 */
	} else {
		/* it doesn't gate/ungate aclk_cpif_200
		   clk_prepare_enable(clks->aclk_cpif_200);
		 */
		clk_prepare_enable(clks->gate_cpifnm_200);
		clk_prepare_enable(clks->gate_mphy_pll);
		clk_prepare_enable(clks->gate_lli_svc_loc);
		clk_prepare_enable(clks->gate_lli_svc_rem);
		clk_prepare_enable(clks->gate_lli_ll_init);
		clk_prepare_enable(clks->gate_lli_be_init);
		clk_prepare_enable(clks->gate_lli_ll_targ);
		clk_prepare_enable(clks->gate_lli_be_targ);
		clk_prepare_enable(clks->gate_lli_cmn_cfg);
		clk_prepare_enable(clks->gate_lli_tx0_cfg);
		clk_prepare_enable(clks->gate_lli_rx0_cfg);
		clk_prepare_enable(clks->gate_lli_tx0_symbol);
		clk_prepare_enable(clks->gate_lli_rx0_symbol);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
/* exynos_lli_suspend must call by modem_if */
static int exynos_lli_suspend(struct mipi_lli *lli)
{
	/* masking all of lli interrupts */
	exynos_lli_system_config(lli);
	/* clearing all of lli sideband signal */
	exynos_lli_reset_signal(lli);
	/* disable LLI_PHY_CONTROL */
	writel(0, lli->pmu_regs);

	exynos_lli_clock_gating(lli, true);

	lli->is_suspended = true;

	return 0;
}

/* exynos_lli_resume must call by modem_if */
static int exynos_lli_resume(struct mipi_lli *lli)
{
	/* re-init clock mux selection for LLI & M-PHY */
	exynos_lli_clock_init(lli);
	exynos_lli_clock_gating(lli, false);

	/* re-init all of lli resource */
	exynos_lli_init(lli);

	lli->is_suspended = false;

	return 0;
}
#else
#define exynos_lli_suspend NULL
#define exynos_lli_resume NULL
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_RUNTIME
static int exynos_lli_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	lli->is_runtime_suspended = true;

	return 0;
}

static int exynos_lli_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	lli->is_runtime_suspended = false;

	return 0;
}
#else
#define exynos_lli_runtime_suspend NULL
#define exynos_lli_runtime_resume NULL
#endif /* CONFIG_PM_RUNTIME */

static void exynos_mipi_lli_set_automode(struct mipi_lli *lli, bool is_auto)
{
	if (is_auto)
		writel(CSA_AUTO_MODE, lli->regs + EXYNOS_PA_CSA_PA_SET);
	else
		writel(CSA_AUTO_MODE, lli->regs + EXYNOS_PA_CSA_PA_CLR);
}

const struct lli_driver exynos_lli_driver = {
	.init = exynos_lli_init,
	.set_master = exynos_lli_set_master,
	.link_startup_mount = exynos_lli_link_startup_mount,
	.get_status = exynos_lli_get_status,
	.send_signal = exynos_lli_send_signal,
	.reset_signal = exynos_lli_reset_signal,
	.read_signal = exynos_lli_read_signal,
	.suspend = exynos_lli_suspend,
	.resume = exynos_lli_resume,
};

static irqreturn_t exynos_mipi_lli_irq(int irq, void *_dev)
{
	struct device *dev = _dev;
	struct mipi_lli *lli = dev_get_drvdata(dev);
	struct exynos_mphy *phy;
	int status;

	status = readl(lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	if (status & INTR_MPHY_HIBERN8_EXIT_DONE)
		writel(LLI_MOUNT_CTRL, lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);

	if (status & INTR_LLI_MOUNT_DONE) {
		lli->state = LLI_MOUNTED;
		dev_dbg(dev, "Mount\n");
	}

	if (status & INTR_LLI_UNMOUNT_DONE) {
		lli->state = LLI_UNMOUNTED;
		dev_dbg(dev, "Unmount\n");
	}

	if (status & INTR_PA_PLU_DETECTED)
		dev_dbg(dev, "PLU_DETECT\n");

	if (status & INTR_PA_PLU_DONE) {
		dev_dbg(dev, "PLU_DONE\n");

		phy = dev_get_drvdata(lli->mphy);

		if (phy) {
			if (phy->is_shared_clk)
				exynos_mipi_lli_set_automode(lli, true);
			else
				exynos_mipi_lli_set_automode(lli, false);
		} else {
			dev_err(dev, "Failed to get exynos_mphy\n");
		}
	}

	if ((status & INTR_RESET_ON_ERROR_DETECTED)) {
		dev_err(dev, "Error detected\n");
		writel(1, lli->regs + EXYNOS_DME_LLI_RESET);
	}

	if (status & INTR_RESET_ON_ERROR_SENT) {
		dev_err(dev, "Error sent\n");
		writel(1, lli->regs + EXYNOS_DME_LLI_RESET);
	}

	if (status & INTR_PA_ERROR_INDICATION) {
		dev_err(dev, "PA_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_PA_INTR_REASON));
	}

	if (status & INTR_DL_ERROR_INDICATION) {
		dev_err(dev, "DL_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_DL_INTR_REASON));
	}

	if (status & INTR_TL_ERROR_INDICATION) {
		dev_err(dev, "TL_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_TL_INTR_REASON));
	}

	if (status & INTR_IAL_ERROR_INDICATION) {
		dev_err(dev, "IAL_REASON0 %x, REASON1 %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_IAL_INTR_REASON0),
			readl(lli->regs + EXYNOS_DME_LLI_IAL_INTR_REASON1));
	}

	writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	return IRQ_HANDLED;
}

static int exynos_mipi_lli_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct mipi_lli *lli;
	struct resource *res;
	struct clk *dout_aclk_cpif_200;
	struct clk *dout_mif_pre;
	void __iomem *regs, *remote_regs, *sysregs, *pmuregs;
	int irq, irq_sig;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find register resource 0\n");
		return -ENXIO;
	}

	regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "cannot find register resource 1\n");
		return -ENXIO;
	}

	remote_regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "cannot find register resource 2\n");
		return -ENXIO;
	}

	sysregs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(dev, "cannot find register resource 3\n");
		return -ENXIO;
	}

	pmuregs = devm_request_and_ioremap(dev, res);
	if (!pmuregs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	/* Request LLI IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return -EBUSY;
	}

	/* Request Signal IRQ */
	irq_sig = platform_get_irq(pdev, 1);
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return -EBUSY;
	}

	ret = mipi_lli_add_driver(dev, &exynos_lli_driver, irq_sig);
	if (ret < 0)
		return ret;

	lli = dev_get_drvdata(dev);

	lli->regs = regs;
	lli->remote_regs = remote_regs;
	lli->sys_regs = sysregs;
	lli->pmu_regs = pmuregs;
	lli->is_master = false;

	ret = request_irq(irq, exynos_mipi_lli_irq, 0, dev_name(dev), dev);
	if (ret < 0)
		return ret;

	if (node) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret)
			dev_err(dev, "failed to add mphy\n");
	} else {
		dev_err(dev, "no device node, failed to add mphy\n");
		ret = -ENODEV;
	}

	lli->mphy = exynos_get_mphy();
	if (!lli->mphy) {
		dev_err(dev, "failed get mphy\n");
		return -ENODEV;
	}

	exynos_lli_get_clk_info(lli);

	/* init clock mux selection for LLI & M-PHY */
	exynos_lli_clock_init(lli);

	/* When getting clock structure data at first, there is no way to
	   know whether the value of clock is enabled or disabled.
	   therefore, at first time, set the enable the clock value to
	   inform that clock is enabled */
	exynos_lli_clock_gating(lli, false);

	dout_aclk_cpif_200 = devm_clk_get(lli->dev, "dout_aclk_cpif_200");
	dout_mif_pre = devm_clk_get(lli->dev, "dout_mif_pre");

	ret = clk_set_rate(dout_aclk_cpif_200, 100000000);
	if (!ret)
		dev_err(dev, "failed clk_set_rate on dout_aclk_cpif_200 to 100000000");

	dev_err(dev, "dout_aclk_cpif_200 = %ld\n", dout_aclk_cpif_200->rate);
	dev_err(dev, "dout_mif_pre= %ld\n", dout_mif_pre->rate);

	dev_info(dev, "Registered MIPI-LLI interface\n");

	return ret;
}

static int exynos_mipi_lli_remove(struct platform_device *pdev)
{
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	mipi_lli_remove_driver(lli);

	return 0;
}

static const struct dev_pm_ops exynos_mipi_lli_pm = {
	/* suspend, resume functions must call by modem_if.
	SET_SYSTEM_SLEEP_PM_OPS(exynos_lli_suspend, exynos_lli_resume)
	*/
	SET_RUNTIME_PM_OPS(exynos_lli_runtime_suspend,
			   exynos_lli_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_mipi_lli_dt_match[] = {
	{
		.compatible = "samsung,exynos-mipi-lli"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mipi_lli_dt_match);
#endif

static struct platform_driver exynos_mipi_lli_driver = {
	.probe = exynos_mipi_lli_probe,
	.remove = exynos_mipi_lli_remove,
	.driver = {
		.name = "exynos-mipi-lli",
		.owner = THIS_MODULE,
		.pm = &exynos_mipi_lli_pm,
		.of_match_table = of_match_ptr(exynos_mipi_lli_dt_match),
	},
};

module_platform_driver(exynos_mipi_lli_driver);

MODULE_DESCRIPTION("Exynos MIPI LLI driver");
MODULE_AUTHOR("Yulgon Kim <yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
