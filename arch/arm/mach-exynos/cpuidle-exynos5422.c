/* linux/arch/arm/mach-exynos/cpuidle.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/fb.h>
#include <linux/cpu.h>
#include <linux/tick.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include "../../../drivers/clk/samsung/clk.h"

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/unified.h>
#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/tlbflush.h>
#include <asm/topology.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/cpufreq.h>
#include <mach/exynos-pm.h>
#include <mach/devfreq.h>
#ifdef CONFIG_SND_SAMSUNG_AUDSS
#include <sound/exynos.h>
#endif

#include <plat/pm.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-core.h>
#include <plat/usb-phy.h>

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
static cputime64_t cluster_off_time = 0;
static unsigned long long last_time = 0;
static bool cluster_off_flag = false;
#endif

#define C1_TARGET_RESIDENCY			500
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#define C2_TARGET_RESIDENCY			1000
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
#define CLUSTER_OFF_TARGET_RESIDENCY		5000
#endif
#endif
#define LOWPOWER_TARGET_RESIDENCY		5000

#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10
#define EXYNOS_CHECK_LPA	0xABAD0000
#define EXYNOS_CHECK_DSTOP	0xABAE0000

static int exynos_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			      int index);
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
static int exynos_enter_c2(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv,
				 int index);
#endif
static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index);

struct check_reg_lpa {
	void __iomem	*check_reg;
	unsigned int	check_bit;
};

/*
 * List of check power domain list for LPA mode
 * These register are have to power off to enter LPA mode
 */

static struct check_reg_lpa exynos5_power_domain[] = {
	{.check_reg = EXYNOS5422_SCALER_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5422_ISP_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5422_MFC_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5422_G3D_STATUS,	.check_bit = 0x7},
	{.check_reg = EXYNOS5422_DISP1_STATUS,	.check_bit = 0x7},
};

static struct check_reg_lpa exynos5_dstop_power_domain[] = {
	{.check_reg = EXYNOS5422_MAU_STATUS,	.check_bit = 0x7},
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */

static struct check_reg_lpa exynos5_clock_gating[] = {
	{.check_reg = EXYNOS5_CLK_GATE_IP_DISP1,	.check_bit = 0x00000008},
	{.check_reg = EXYNOS5_CLK_GATE_IP_MFC,		.check_bit = 0x00000001},
	{.check_reg = EXYNOS5_CLK_GATE_IP_GEN,		.check_bit = 0x0000001E},
	{.check_reg = EXYNOS5_CLK_GATE_BUS_FSYS0,	.check_bit = 0x00000006},
	{.check_reg = EXYNOS5_CLK_GATE_IP_PERIC,	.check_bit = 0x00077FC0},
};

static struct clk *clkm_phy0;
static struct clk *clkm_phy1;
static bool mif_max = false;

#ifdef CONFIG_SAMSUNG_USBPHY
extern int samsung_usbphy_check_op(void);
#endif

#if defined(CONFIG_MMC_DW)
extern int dw_mci_exynos_request_status(void);
#endif

#ifdef CONFIG_DEBUG_CPUIDLE
static inline void show_core_regs(int cpuid)
{
	unsigned int val_conf, val_stat, val_opt;
	val_conf = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpuid^0x4));
	val_stat = __raw_readl(EXYNOS_ARM_CORE_STATUS(cpuid^0x4));
	val_opt = __raw_readl(EXYNOS_ARM_CORE_OPTION(cpuid^0x4));
	printk("[%d] config(%x) status(%x) option(%x)\n",
			cpuid, val_conf, val_stat, val_opt);
}
#endif

static int exynos_check_reg_status(struct check_reg_lpa *reg_list,
				    unsigned int list_cnt)
{
	unsigned int i;
	unsigned int tmp;

	for (i = 0; i < list_cnt; i++) {
		tmp = __raw_readl(reg_list[i].check_reg);
		if (tmp & reg_list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

static int exynos_uart_fifo_check(void)
{
	unsigned int ret;
	unsigned int check_val;

	ret = 0;

	/* Check UART for console is empty */
	check_val = __raw_readl(S5P_VA_UART(CONFIG_S3C_LOWLEVEL_UART_PORT) +
				0x18);

	ret = ((check_val >> 16) & 0xff);

	return ret;
}

static int __maybe_unused exynos_check_enter_mode(void)
{
	/* Check power domain */
	if (exynos_check_reg_status(exynos5_power_domain,
			    ARRAY_SIZE(exynos5_power_domain)))
		return EXYNOS_CHECK_DIDLE;

	/* Check clock gating */
	if (exynos_check_reg_status(exynos5_clock_gating,
			    ARRAY_SIZE(exynos5_clock_gating)))
		return EXYNOS_CHECK_DIDLE;

#if defined(CONFIG_MMC_DW)
	if (dw_mci_exynos_request_status())
		return EXYNOS_CHECK_DIDLE;
#endif

#ifdef CONFIG_SAMSUNG_USBPHY
	if (samsung_usbphy_check_op())
		return EXYNOS_CHECK_DIDLE;
#endif
	/* Check audio power domain for Deep STOP */
	if (exynos_check_reg_status(exynos5_dstop_power_domain,
			    ARRAY_SIZE(exynos5_dstop_power_domain)))
		return EXYNOS_CHECK_LPA;

	return EXYNOS_CHECK_DIDLE;
}

static struct cpuidle_state exynos5_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= exynos_enter_idle,
		.exit_latency		= 1,
		.target_residency	= C1_TARGET_RESIDENCY,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM clock gating(WFI)",
	},
	[1] = {
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		.enter			= exynos_enter_c2,
		.exit_latency		= 30,
		.target_residency	= C2_TARGET_RESIDENCY,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C2",
		.desc			= "ARM power down",
	},
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	[2] = {
		.enter			= exynos_enter_c2,
		.exit_latency		= 300,
		.target_residency	= CLUSTER_OFF_TARGET_RESIDENCY,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C2-1",
		.desc			= "Cluster power down",
	},
	[3] = {
#else
	[2] = {
#endif
#endif
		.enter			= exynos_enter_lowpower,
		.exit_latency		= 300,
		.target_residency	= LOWPOWER_TARGET_RESIDENCY,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C3",
		.desc			= "System power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
static spinlock_t c2_state_lock;
static DEFINE_PER_CPU(int, in_c2_state);
#endif

static struct cpuidle_driver exynos_idle_driver = {
	.name		= "exynos_idle",
	.owner		= THIS_MODULE,
};

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(void)
{
	__raw_writel(0x40003ffe, EXYNOS5422_WAKEUP_MASK);
}

static void save_cpu_arch_register(void)
{
}

static void restore_cpu_arch_register(void)
{
}

static int idle_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);

	return 1;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
#define L2_OFF		(1 << 0)
#define L2_CCI_OFF	(1 << 1)
#endif

static int c2_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (flags == L2_CCI_OFF) {
		last_time = get_jiffies_64();
		cluster_off_flag = true;
		exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, flags);
	} else {
		exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	}
#else
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
#endif

	/*
	 * Secure monitor disables the SMP bit and takes the CPU out of the
	 * coherency domain.
	 */
	local_flush_tlb_all();

	return 1;
}
#endif

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
void exynos_enable_idle_clock_down(unsigned int cluster)
{
	unsigned int tmp;

	if (cluster) {
		/* For A15 core */
		tmp = __raw_readl(EXYNOS5422_PWR_CTRL);
		tmp &= ~((0x7 << 28) | (0x7 << 16) | (1 << 9) | (1 << 8));
		tmp |= (0x7 << 28) | (0x7 << 16) | 0x3ff;
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL);

		tmp = __raw_readl(EXYNOS5422_PWR_CTRL2);
		tmp &= ~((0x3 << 24) | (0xffff << 8) | (0x77));
		tmp |= (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0);
		tmp |= (1 << 25) | (1 << 24);
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL2);
	} else {
		/* For A7 core */
		tmp = __raw_readl(EXYNOS5422_PWR_CTRL_KFC);
		tmp &= ~((0x3F << 16) | (1 << 8));
		tmp |= (0x3F << 16) | 0x1ff;
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL_KFC);

		tmp = __raw_readl(EXYNOS5422_PWR_CTRL2_KFC);
		tmp &= ~((0x1 << 24) | (0xffff << 8) | (0x7));
		tmp |= (1 << 16) | (1 << 8) | (1 << 0);
		tmp |= 1 << 24;
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL2_KFC);
	}

	pr_debug("%s idle clock down is enabled\n", cluster ? "ARM" : "KFC");
}

void exynos_disable_idle_clock_down(unsigned int cluster)
{
	unsigned int tmp;

	if (cluster) {
		/* For A15 core */
		tmp = __raw_readl(EXYNOS5422_PWR_CTRL);
		tmp &= ~((0x7 << 28) | (0x7 << 16) | (1 << 9) | (1 << 8));
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL);

		tmp = __raw_readl(EXYNOS5422_PWR_CTRL2);
		tmp &= ~((0x3 << 24) | (0xffff << 8) | (0x77));
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL2);
	} else {
		/* For A7 core */
		tmp = __raw_readl(EXYNOS5422_PWR_CTRL_KFC);
		tmp &= ~((0x7 << 16) | (1 << 8));
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL_KFC);

		tmp = __raw_readl(EXYNOS5422_PWR_CTRL2_KFC);
		tmp &= ~((0x1 << 24) | (0xffff << 8) | (0x7));
		__raw_writel(tmp, EXYNOS5422_PWR_CTRL2_KFC);
	}

	pr_debug("%s idle clock down is disabled\n", cluster ? "ARM" : "KFC");
}
#endif

static struct sleep_save exynos5_lpa_save[] = {
	/* CMU side */
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_TOP),
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_GSCL),
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_DISP10),
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_MAUDIO),
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_PERIC0),
	SAVE_ITEM(EXYNOS5_CLK_SRC_MASK_PERIC1),
	SAVE_ITEM(EXYNOS5_CLK_SRC_TOP3),
	SAVE_ITEM(EXYNOS5_CLK_SRC_TOP5),
	SAVE_ITEM(EXYNOS5_CLK_DIV_G2D),
	SAVE_ITEM(EXYNOS5_CLK_GATE_IP_G2D),
};

static struct sleep_save exynos5_set_clksrc[] = {
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_GSCL		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_DISP10		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_MAUDIO		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_FSYS		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_PERIC0		, .val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_PERIC1		, .val = 0xffffffff, },
};

static int __maybe_unused exynos_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	unsigned long tmp;
	unsigned int ret = 0;
	unsigned int cpuid = smp_processor_id();

	local_irq_disable();
	do_gettimeofday(&before);

	exynos_set_wakeupmask();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_disable_idle_clock_down(ARM);
	exynos_disable_idle_clock_down(KFC);
#endif

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
	}

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_enable_idle_clock_down(ARM);
	exynos_enable_idle_clock_down(KFC);
#endif

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS5422_WAKEUP_STAT);

	do_gettimeofday(&after);

	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static int exynos_enter_core0_lpa(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int lp_mode, int index, int enter_mode)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned long tmp;
	unsigned int cpuid = smp_processor_id();
	unsigned int cpu_offset;

	/*
	 * Before enter central sequence mode, clock src register have to set
	 */
	s3c_pm_do_save(exynos5_lpa_save, ARRAY_SIZE(exynos5_lpa_save));

	s3c_pm_do_restore_core(exynos5_set_clksrc,
			       ARRAY_SIZE(exynos5_set_clksrc));

	local_irq_disable();
	do_gettimeofday(&before);
	/*
	 * Unmasking all wakeup source.
	 */
	__raw_writel(0x7FFFE000, EXYNOS5422_WAKEUP_MASK);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	/*
	 * If audio clock source is changed to others,
	 * set back audio clock to XXTI.
	 */
	tmp = __raw_readl(EXYNOS5_CLK_SRC_PERIC1);
	if(tmp&0x7000) {
		tmp &= ~((1<<12)|(1<<13)|(1<<14));
		__raw_writel(tmp, EXYNOS5_CLK_SRC_PERIC1);
	}

	/* Set value of power down register for low power mode */
	if(enter_mode == EXYNOS_CHECK_LPA)
		exynos_sys_powerdown_conf(SYS_LPA);
	else
		exynos_sys_powerdown_conf(SYS_AFTR);

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_disable_idle_clock_down(ARM);
	exynos_disable_idle_clock_down(KFC);
#endif

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	cpu_offset = cpuid ^ 0x4;
	tmp = __raw_readl(EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);

	do {
		/* Waiting for flushing UART fifo */
	} while (exynos_uart_fifo_check());

	set_boot_flag(cpuid, C2_STATE);

	__raw_writel(EXYNOS_CHECK_LPA, EXYNOS_PMU_SPARE1);

	tmp = __raw_readl(EXYNOS5422_SFR_AXI_CGDIS1_REG);
	tmp |= (EXYNOS5422_UFS | EXYNOS5422_ACE_KFC | EXYNOS5422_ACE_EAGLE);
	__raw_writel(tmp, EXYNOS5422_SFR_AXI_CGDIS1_REG);
#if defined(CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ)
	exynos5_mif_transition_disable(true);
#endif
	if((__clk_is_enabled(clkm_phy0))||(__clk_is_enabled(clkm_phy1))) {
		mif_max = true;
		clk_disable(clkm_phy0);
		clk_disable(clkm_phy1);
	}

	cpu_pm_enter();
	exynos_lpa_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);

		goto early_wakeup;
	}

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_GPIO_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MMCA_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MMCB_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MMCC_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_HSI_OPTION);

early_wakeup:
	if (mif_max) {
		clk_enable(clkm_phy0);
		clk_enable(clkm_phy1);
		mif_max = false;
	}

#if defined(CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ)
	exynos5_mif_transition_disable(false);
#endif
	__raw_writel(0, EXYNOS_PMU_SPARE1);

	tmp = __raw_readl(EXYNOS5422_SFR_AXI_CGDIS1_REG);
	tmp &= ~(EXYNOS5422_UFS | EXYNOS5422_ACE_KFC | EXYNOS5422_ACE_EAGLE);
	__raw_writel(tmp, EXYNOS5422_SFR_AXI_CGDIS1_REG);

#if defined(CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ)
	exynos5_int_nocp_resume();
#endif
	clear_boot_flag(cpuid, C2_STATE);

	exynos_lpa_exit();
	cpu_pm_exit();

	restore_cpu_arch_register();

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_enable_idle_clock_down(ARM);
	exynos_enable_idle_clock_down(KFC);
#endif

	s3c_pm_do_restore_core(exynos5_lpa_save,
			       ARRAY_SIZE(exynos5_lpa_save));

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS5422_WAKEUP_STAT);

	do_gettimeofday(&after);

	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;

	return index;
}

static int exynos_enter_lowpower(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int new_index = index;
	int enter_mode;

	/* This mode only can be entered when other core's are offline */
	if (num_online_cpus() > 1)
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		return exynos_enter_c2(dev, drv, 2);
#else
		return exynos_enter_c2(dev, drv, 1);
#endif
#else
		return exynos_enter_idle(dev, drv, 0);
#endif
	enter_mode = exynos_check_enter_mode();
	if (enter_mode == EXYNOS_CHECK_DIDLE)
		return exynos_enter_idle(dev, drv, 0);
#ifdef CONFIG_SND_SAMSUNG_AUDSS
	else if (exynos_check_aud_pwr() == AUD_PWR_AFTR)
		return exynos_enter_core0_aftr(dev, drv, new_index);
#endif
	else
		return exynos_enter_core0_lpa(dev, drv, SYS_LPA, new_index, enter_mode);
}

static int exynos_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;

	local_irq_disable();
	do_gettimeofday(&before);

	cpu_do_idle();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		(after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
#define CHECK_CCI_SNOOP		(1 << 7)

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
static bool disabled_c3 = false;

static void exynos_disable_c3_idle(bool disable)
{
	disabled_c3 = disable;
}
#endif

static int can_enter_cluster_off(int cpu_id)
{
#if defined(CONFIG_SCHED_HMP)
	ktime_t now = ktime_get();
	struct clock_event_device *dev;
	int cpu;

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	if (disabled_c3)
		return 0;
#endif

	for_each_cpu_and(cpu, cpu_possible_mask, cpu_coregroup_mask(cpu_id)) {
		if (cpu_id == cpu)
			continue;

		dev = per_cpu(tick_cpu_device, cpu).evtdev;
		if (!(per_cpu(in_c2_state, cpu)))
			return 0;

		if (ktime_to_us(ktime_sub(dev->next_event, now)) <
			CLUSTER_OFF_TARGET_RESIDENCY)
			return 0;
	}

	return 1;
#else
	return 0;
#endif
}
#endif
static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned int cpuid = smp_processor_id(), cpu_offset = 0;
	unsigned int value;
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	unsigned int cluster_id = read_cpuid(CPUID_MPIDR) >> 8 & 0xf;
	unsigned long flags;
#endif

	/* KFC don't use C2 state */
	if (cpuid < 4)
		return exynos_enter_idle(dev, drv, 0);

	local_irq_disable();
	do_gettimeofday(&before);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	cpu_offset = cpuid ^ 0x4;

	set_boot_flag(cpuid, C2_STATE);
	cpu_pm_enter();

	 __raw_writel(0x0, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));

	value = __raw_readl(EXYNOS5422_ARM_INTR_SPREAD_ENABLE);
	value &= ~(0x1 << cpu_offset);
	__raw_writel(value, EXYNOS5422_ARM_INTR_SPREAD_ENABLE);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (cluster_id == 0)
		set_boot_flag(cpuid, CHECK_CCI_SNOOP);

	flags = 0;
	if (index == 2) {
		spin_lock(&c2_state_lock);
		per_cpu(in_c2_state, cpuid) = 1;
		if (can_enter_cluster_off(cpuid))
			flags = L2_CCI_OFF;
		spin_unlock(&c2_state_lock);
	}

	if (flags)
		__raw_writel(0, EXYNOS_COMMON_CONFIGURATION(cluster_id));
	else
		__raw_writel(0x3, EXYNOS_COMMON_CONFIGURATION(cluster_id));

	ret = cpu_suspend(flags, c2_finisher);
	if (ret) {
		__raw_writel(0x3, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));

	if (flags)
		__raw_writel(0x3, EXYNOS_COMMON_CONFIGURATION(cluster_id));
	}

	if (cluster_off_flag && !disabled_c3) {
		cluster_off_time += get_jiffies_64() - last_time;
		cluster_off_flag = false;
	}

#else
	ret = cpu_suspend(0, c2_finisher);
	if (ret)
		__raw_writel(0x3, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));
#endif

	value = __raw_readl(EXYNOS5422_ARM_INTR_SPREAD_ENABLE);
	value |= (0x1 << cpu_offset);
	__raw_writel(value, EXYNOS5422_ARM_INTR_SPREAD_ENABLE);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (index == 2) {
		spin_lock(&c2_state_lock);
		per_cpu(in_c2_state, cpuid) = 0;
		spin_unlock(&c2_state_lock);
	}

	clear_boot_flag(cpuid, C2_STATE | CHECK_CCI_SNOOP);
#else
	clear_boot_flag(cpuid, C2_STATE);
#endif

	cpu_pm_exit();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;

	return index;
}

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
static struct dentry *cluster_off_time_debugfs;

static int cluster_off_time_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "CA15_cluster_off %llu\n",
			(unsigned long long) cputime64_to_clock_t(cluster_off_time));

	return 0;
}

static int cluster_off_time_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cluster_off_time_show, inode->i_private);
}

const static struct file_operations cluster_off_time_fops = {
	.open		= cluster_off_time_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif
#endif

static int exynos_cpuidle_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		cpu_idle_poll_ctrl(true);
		pr_debug("PM_SUSPEND_PREPARE for CPUIDLE\n");
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		cpu_idle_poll_ctrl(false);
		pr_debug("PM_POST_SUSPEND for CPUIDLE\n");
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block exynos_cpuidle_notifier = {
	.notifier_call = exynos_cpuidle_notifier_event,
};

static int __init exynos_init_cpuidle(void)
{
	int i, max_cpuidle_state, cpu_id, ret;
	struct cpuidle_device *device;
	struct cpuidle_driver *drv = &exynos_idle_driver;
	struct cpuidle_state *idle_set;
#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	u32 value = 0;

	value = __raw_readl(EXYNOS_COMMON_OPTION(0));
	value |= (1 << 30) | (1 << 29) | (1 << 9);
	__raw_writel(value, EXYNOS_COMMON_OPTION(0));
#endif

	/* Setup cpuidle driver */
	idle_set = exynos5_cpuidle_set;
	drv->state_count = ARRAY_SIZE(exynos5_cpuidle_set);

	max_cpuidle_state = drv->state_count;

	for (i = 0; i < max_cpuidle_state; i++) {
		memcpy(&drv->states[i], &idle_set[i],
				sizeof(struct cpuidle_state));
	}
	drv->safe_state_index = 0;
	ret = cpuidle_register_driver(&exynos_idle_driver);
	if (ret) {
		printk(KERN_ERR "CPUidle register device failed\n,");
		return ret;
	}

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

		device->state_count = max_cpuidle_state;

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		if (cpu_id < 4)
			device->state_count--;
		per_cpu(in_c2_state, cpu_id) = 0;
#endif

		if (cpuidle_register_device(device)) {
			printk(KERN_ERR "CPUidle register device failed\n,");
			return -EIO;
		}
	}

	clkm_phy0 = __clk_lookup("clkm_phy0");
	clkm_phy1 = __clk_lookup("clkm_phy1");

	if (IS_ERR(clkm_phy0)) {
		pr_err("Cannot get clock \"clkm_phy0\"\n");
		return PTR_ERR(clkm_phy0);
	}

	if (IS_ERR(clkm_phy1)) {
		pr_err("Cannot get clock \"clkm_phy1\"\n");
		return PTR_ERR(clkm_phy1);
	}

#if defined(CONFIG_EXYNOS_CLUSTER_POWER_DOWN) && defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
	disable_c3_idle = exynos_disable_c3_idle;
#endif

	register_pm_notifier(&exynos_cpuidle_notifier);

#if defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	cluster_off_time_debugfs =
		debugfs_create_file("cluster_off_time",
				S_IRUGO, NULL, NULL, &cluster_off_time_fops);
	if (IS_ERR_OR_NULL(cluster_off_time_debugfs)) {
		cluster_off_time_debugfs = NULL;
		pr_err("%s: debugfs_create_file() failed\n", __func__);
	}

	spin_lock_init(&c2_state_lock);
#endif

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_enable_idle_clock_down(ARM);
	exynos_enable_idle_clock_down(KFC);
#endif

	return 0;
}
device_initcall(exynos_init_cpuidle);
