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
#include <linux/tick.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/debugfs.h>

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/unified.h>
#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/tlbflush.h>
#include <asm/cpuidle.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/cpufreq.h>
#include <mach/exynos-pm.h>
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
#include <plat/clock.h>

#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
static cputime64_t cluster_off_time = 0;
static unsigned long long last_time = 0;
static bool cluster_off_flag = false;

static spinlock_t cluster_ctrl_lock;
static DEFINE_PER_CPU(int, in_c2_state);

#define CLUSTER_OFF_TARGET_RESIDENCY	5000
#endif

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
	{.check_reg = EXYNOS5430_GSCL_STATUS,	.check_bit = 0x7},	/* 0x4004 */
	{.check_reg = EXYNOS5430_ISP_STATUS,	.check_bit = 0x7},	/* 0x4144 */
	{.check_reg = EXYNOS5430_CAM0_STATUS,	.check_bit = 0x7},	/* 0x4024 */
	{.check_reg = EXYNOS5430_CAM1_STATUS,	.check_bit = 0x7},	/* 0x40A4 */
	{.check_reg = EXYNOS5430_MFC0_STATUS,	.check_bit = 0x7},	/* 0x4184 */
	{.check_reg = EXYNOS5430_MFC1_STATUS,	.check_bit = 0x7},	/* 0x41A4 */
	{.check_reg = EXYNOS5430_HEVC_STATUS,	.check_bit = 0x7},	/* 0x41C4 */
	{.check_reg = EXYNOS5430_G3D_STATUS,	.check_bit = 0x7},	/* 0x4064 */
	{.check_reg = EXYNOS5430_DISP_STATUS,	.check_bit = 0x7},	/* 0x4084 */
};

static struct check_reg_lpa exynos5_dstop_power_domain[] = {
	{.check_reg = EXYNOS5430_AUD_STATUS,	.check_bit = 0xF},	/* 0x40C4 */
};

/*
 * List of check clock gating list for LPA mode
 * If clock of list is not gated, system can not enter LPA mode.
 */

static struct check_reg_lpa exynos5_clock_gating[] = {
#ifdef CONFIG_EXYNOS_MIPI_LLI
	{.check_reg = EXYNOS5430_ENABLE_IP_CPIF0,	.check_bit = 0xFFE},
#endif
};

#ifdef CONFIG_SAMSUNG_USBPHY
extern int samsung_usbphy_check_op(void);
extern void samsung_usb_lpa_resume(void);
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

static int exynos_check_enter_mode(void)
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

	/* Check power domain for DSTOP */
	if (exynos_check_reg_status(exynos5_dstop_power_domain,
			    ARRAY_SIZE(exynos5_dstop_power_domain)))
		return EXYNOS_CHECK_LPA;

	return EXYNOS_CHECK_DSTOP;
}

static struct cpuidle_state exynos5_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= exynos_enter_idle,
		.exit_latency		= 1,
		.target_residency	= 500,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM clock gating(WFI)",
	},
	[1] = {
#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
		.enter                  = exynos_enter_c2,
		.exit_latency           = 30,
		.target_residency       = 1000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C2",
		.desc                   = "ARM power down",
	},
#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	[2] = {
		.enter                  = exynos_enter_c2,
		.exit_latency           = 300,
		.target_residency       = CLUSTER_OFF_TARGET_RESIDENCY,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C2-1",
		.desc                   = "Cluster power down",
	},
	[3] = {
#else
	[2] = {
#endif
#endif
		.enter                  = exynos_enter_lowpower,
		.exit_latency           = 300,
		.target_residency       = 5000,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C3",
		.desc                   = "System power down",
	},
};

static DEFINE_PER_CPU(struct cpuidle_device, exynos_cpuidle_device);

static struct cpuidle_driver exynos_idle_driver = {
	.name                   = "exynos_idle",
	.owner                  = THIS_MODULE,
};

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(void)
{
	__raw_writel(0x1010fe06, EXYNOS5430_WAKEUP_MASK);
	__raw_writel(0x00000000, EXYNOS5430_WAKEUP_MASK1);
	__raw_writel(0x00000000, EXYNOS5430_WAKEUP_MASK2);
}

#if defined(CONFIG_APPLY_ARM_TRUSTZONE) || defined(CONFIG_SOC_EXYNOS5430)
static void save_cpu_arch_register(void)
{
}

static void restore_cpu_arch_register(void)
{
}
#else
static unsigned int g_pwr_ctrl, g_diag_reg;

static void save_cpu_arch_register(void)
{
	/*read power control register*/
	asm("mrc p15, 0, %0, c15, c0, 0" : "=r"(g_pwr_ctrl) : : "cc");
	/*read diagnostic register*/
	asm("mrc p15, 0, %0, c15, c0, 1" : "=r"(g_diag_reg) : : "cc");
	return;
}

static void restore_cpu_arch_register(void)
{
	/*write power control register*/
	asm("mcr p15, 0, %0, c15, c0, 0" : : "r"(g_pwr_ctrl) : "cc");
	/*write diagnostic register*/
	asm("mcr p15, 0, %0, c15, c0, 1" : : "r"(g_diag_reg) : "cc");
	return;
}
#endif

static int idle_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, 0);

	return 1;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
#define L2_OFF		(1 << 0)
#define L2_CCI_OFF	(1 << 1)
#endif
static int c2_finisher(unsigned long flags)
{
	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);

#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (flags == L2_CCI_OFF) {
		exynos_cpu_sequencer_ctrl(true);

		spin_lock(&cluster_ctrl_lock);
		cluster_off_flag = true;
		spin_unlock(&cluster_ctrl_lock);
		last_time = get_jiffies_64();

		exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, flags);
	} else
#endif
		exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CORE, SMC_POWERSTATE_IDLE, 0);

	/*
	 * Secure monitor disables the SMP bit and takes the CPU out of the
	 * coherency domain.
	 */
	local_flush_tlb_all();

	return 1;
}
#endif

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
void exynos_idle_clock_down(bool on, enum cpu_type cluster)
{
	void __iomem *reg_pwr_ctrl1, *reg_pwr_ctrl2;
	unsigned int tmp;

	if (cluster == KFC) {
		reg_pwr_ctrl1 = EXYNOS5430_KFC_PWR_CTRL;
		reg_pwr_ctrl2 = EXYNOS5430_KFC_PWR_CTRL2;
	} else {
		reg_pwr_ctrl1 = EXYNOS5430_EGL_PWR_CTRL;
		reg_pwr_ctrl2 = EXYNOS5430_EGL_PWR_CTRL2;
	}

	if (on) {
		/*
		 * EGL_PWR_CTRL and KFC_PWR_CTRL, both register's
		 * control bits are same. So even in the case of the KFC,
		 * use the EGL_PWR_CTRL register definition.
		 */
		tmp = __raw_readl(reg_pwr_ctrl1);
		tmp &= ~(EXYNOS5430_USE_EGL_CLAMPCOREOUT |
			EXYNOS5430_USE_STANDBYWFIL2_EGL);
		tmp |= (EXYNOS5430_EGL2_RATIO |
			EXYNOS5430_EGL1_RATIO |
			EXYNOS5430_DIVEGL2_DOWN_ENABLE |
			EXYNOS5430_DIVEGL1_DOWN_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl1);

		tmp = __raw_readl(reg_pwr_ctrl2);
		tmp &= ~(EXYNOS5430_DUR_STANDBY2 |
			EXYNOS5430_DUR_STANDBY1);
		tmp |= (EXYNOS5430_DUR_STANDBY2_VALUE |
			EXYNOS5430_DUR_STANDBY1_VALUE |
			EXYNOS5430_UP_EGL2_RATIO |
			EXYNOS5430_UP_EGL1_RATIO |
			EXYNOS5430_DIVEGL2_UP_ENABLE |
			EXYNOS5430_DIVEGL1_UP_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl2);
	} else {
		tmp = __raw_readl(reg_pwr_ctrl1);
		tmp &= ~(EXYNOS5430_DIVEGL2_DOWN_ENABLE |
			EXYNOS5430_DIVEGL1_DOWN_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl1);

		tmp = __raw_readl(reg_pwr_ctrl1);
		tmp &= ~(EXYNOS5430_DIVEGL2_UP_ENABLE |
			EXYNOS5430_DIVEGL1_UP_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl1);
	}

	pr_debug("%s idle clock down is %s\n", (cluster == KFC ? "KFC" : "EAGLE"),
					(on ? "enabled" : "disabled"));
}
#endif

static int exynos_enter_core0_aftr(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	unsigned long tmp;
	unsigned int ret = 0;
	unsigned int cpuid = smp_processor_id();

	local_fiq_disable();
	do_gettimeofday(&before);

	exynos_set_wakeupmask();

	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(false, ARM);
	exynos_idle_clock_down(false, KFC);
#endif

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	}

	clear_boot_flag(cpuid, C2_STATE);

	cpu_pm_exit();

	restore_cpu_arch_register();

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);
#endif

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS5430_WAKEUP_STAT);
	__raw_writel(0x0, EXYNOS5430_WAKEUP_STAT1);
	__raw_writel(0x0, EXYNOS5430_WAKEUP_STAT2);

	do_gettimeofday(&after);

	local_fiq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

static struct sleep_save exynos5_lpa_save[] = {
	/* CMU side */
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_TOP),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_FSYS0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_PERIC0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_PERIC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_EGL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_KFC1),

	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MIF1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CPIF0),

	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP3),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_CAM1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_DISP),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_FSYS0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_FSYS1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_PERIC0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_PERIC1),

	SAVE_ITEM(EXYNOS5430_ISP_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_ISP_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_AUD_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_AUD_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_AUD_PLL_CON2),
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	SAVE_ITEM(EXYNOS5430_AUD_DPLL_CON0),
	SAVE_ITEM(EXYNOS5430_AUD_DPLL_CON1),
	SAVE_ITEM(EXYNOS5430_AUD_DPLL_CON2),
#endif

	SAVE_ITEM(EXYNOS5430_DIV_TOP0),
	SAVE_ITEM(EXYNOS5430_DIV_TOP1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP2),
	SAVE_ITEM(EXYNOS5430_DIV_TOP3),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_CAM10),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_CAM11),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_FSYS0),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_FSYS1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_FSYS2),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC0),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC1),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC2),
	SAVE_ITEM(EXYNOS5430_DIV_TOP_PERIC3),

#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	SAVE_ITEM(EXYNOS5430_BUS_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_BUS_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_MEM_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MEM_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_BUS_DPLL_CON0),
	SAVE_ITEM(EXYNOS5430_BUS_DPLL_CON1),
	SAVE_ITEM(EXYNOS5430_BUS_DPLL_CON2),
#else
	SAVE_ITEM(EXYNOS5430_MEM0_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MEM0_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_MEM1_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MEM1_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_BUS_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_BUS_PLL_CON1),
#endif
	SAVE_ITEM(EXYNOS5430_MFC_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_MFC_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF3),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF4),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF5),
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	SAVE_ITEM(EXYNOS5430_DIV_MIF0),
#endif
	SAVE_ITEM(EXYNOS5430_DIV_MIF1),
	SAVE_ITEM(EXYNOS5430_DIV_MIF2),
	SAVE_ITEM(EXYNOS5430_DIV_MIF3),
	SAVE_ITEM(EXYNOS5430_DIV_MIF4),
	SAVE_ITEM(EXYNOS5430_DIV_MIF5),

	SAVE_ITEM(EXYNOS5430_SRC_SEL_EGL0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_EGL1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_EGL2),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_KFC0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_KFC1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_KFC2),

#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	SAVE_ITEM(EXYNOS5430_SRC_SEL_BUS1),
#endif
	SAVE_ITEM(EXYNOS5430_SRC_SEL_BUS2),

	SAVE_ITEM(EXYNOS5430_DIV_EGL0),
	SAVE_ITEM(EXYNOS5430_DIV_EGL1),

	SAVE_ITEM(EXYNOS5430_DIV_KFC0),
	SAVE_ITEM(EXYNOS5430_DIV_KFC1),
	SAVE_ITEM(EXYNOS5430_DIV_BUS1),
	SAVE_ITEM(EXYNOS5430_DIV_BUS2),
	/* CLK_SRC */
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP0),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP2),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP3),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP4),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP_CAM1),
	SAVE_ITEM(EXYNOS5430_SRC_ENABLE_TOP_DISP),
};

static struct sleep_save exynos5_set_clksrc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_FSYS0,	.val = 0x00007dfb, },
	{ .reg = EXYNOS5430_ENABLE_IP_PERIC0,	.val = 0x1fffffff, },
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	{ .reg = EXYNOS5430_SRC_SEL_TOP_PERIC1,	.val = 0x00000033, },
#else
	{ .reg = EXYNOS5430_SRC_SEL_TOP_PERIC1,	.val = 0x00000011, },
#endif
	{ .reg = EXYNOS5430_ENABLE_IP_EGL1,	.val = 0x00000fff, },
	{ .reg = EXYNOS5430_ENABLE_IP_KFC1,	.val = 0x00000fff, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF1,	.val = 0x01fffff7, },
	{ .reg = EXYNOS5430_ENABLE_IP_CPIF0,	.val = 0x000FF000, },
};

static void exynos_set_mif_mux_status(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5430_SRC_ENABLE_MIF1);

	/* 1. If 12bit of CLK_MUX_ENABLE_MIF1 is '1', save '1' */
	if (tmp & EXYNOS5430_CLKM_PHY_C_ENABLE)
		__raw_writel(0x1, EXYNOS5430_DREX_CALN2);
	else
		__raw_writel(0x0, EXYNOS5430_DREX_CALN2);

	/* 2. Write 0 to 12bit of CLK_MUX_ENABLE_MIF1 before LPA/DSTOP */
	tmp &= ~EXYNOS5430_CLKM_PHY_C_ENABLE;
	__raw_writel(tmp, EXYNOS5430_SRC_ENABLE_MIF1);
}

static void exynos_restore_mif_mux_status(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5430_SRC_ENABLE_MIF1);
	if (__raw_readl(EXYNOS5430_DREX_CALN2))
		tmp |= EXYNOS5430_CLKM_PHY_C_ENABLE;
	else
		tmp &= ~EXYNOS5430_CLKM_PHY_C_ENABLE;

	__raw_writel(tmp, EXYNOS5430_SRC_ENABLE_MIF1);
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

	local_fiq_disable();
	do_gettimeofday(&before);

	/*
	 * Unmasking all wakeup source.
	 */
	__raw_writel(0x00001000, EXYNOS5430_WAKEUP_MASK);
	__raw_writel(0xFFFF0000, EXYNOS5430_WAKEUP_MASK1);
	__raw_writel(0xFFFF0000, EXYNOS5430_WAKEUP_MASK2);

	/* Configure GPIO Power down control register */
#ifdef MUST_MODIFY
	exynos_set_lpa_pdn(S3C_GPIO_END);
#endif

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	/* Set value of power down register for aftr mode */
	if (enter_mode == EXYNOS_CHECK_LPA)
		exynos_sys_powerdown_conf(SYS_LPA);
	else
		exynos_sys_powerdown_conf(SYS_DSTOP);

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(false, ARM);
	exynos_idle_clock_down(false, KFC);
#endif

	save_cpu_arch_register();

	/* Setting Central Sequence Register for power down mode */
	cpu_offset = cpuid ^ 0x4;
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	do {
		/* Waiting for flushing UART fifo */
	} while (exynos_uart_fifo_check());

	set_boot_flag(cpuid, C2_STATE);

	cpu_pm_enter();
	exynos_lpa_enter();

	if (lp_mode == SYS_ALPA)
		__raw_writel(0x1, EXYNOS5430_PMU_SYNC_CTRL);

	/* This is W/A for gating Mclk during LPA/DSTOP */
	/* Save flag to confirm if current mode is LPA or DSTOP */
	__raw_writel(EXYNOS_CHECK_DIRECTGO, EXYNOS5430_DREX_CALN1);
	exynos_set_mif_mux_status();

	ret = cpu_suspend(0, idle_finisher);
	if (ret) {
		tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

		goto early_wakeup;
	}

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS_PAD_RET_DRAM_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MAUDIO_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_JTAG_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_MMC2_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_TOP_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_MMC0_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_MMC1_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_MIF_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_USBXTI_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_BOOTLDO_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_UFS_OPTION);
	__raw_writel((1 << 28), EXYNOS5430_PAD_RETENTION_FSYSGENIO_OPTION);

early_wakeup:
	samsung_usb_lpa_resume();

	if (lp_mode == SYS_ALPA)
		__raw_writel(0x0, EXYNOS5430_PMU_SYNC_CTRL);

	/* This is W/A for gating Mclk during LPA/DSTOP */
	/* Clear flag  */
	__raw_writel(0, EXYNOS5430_DREX_CALN1);
	exynos_restore_mif_mux_status();

	clear_boot_flag(cpuid, C2_STATE);

	exynos_lpa_exit();
	cpu_pm_exit();

	restore_cpu_arch_register();

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);
#endif

	s3c_pm_do_restore_core(exynos5_lpa_save,
			       ARRAY_SIZE(exynos5_lpa_save));

	/* Clear wakeup state register */
	__raw_writel(0x0, EXYNOS5430_WAKEUP_STAT);
	__raw_writel(0x0, EXYNOS5430_WAKEUP_STAT1);
	__raw_writel(0x0, EXYNOS5430_WAKEUP_STAT2);

	do_gettimeofday(&after);

	local_fiq_enable();
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
#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		return exynos_enter_c2(dev, drv, 2);
#else
		return exynos_enter_c2(dev, drv, 1);
#endif
#else
		return exynos_enter_idle(dev, drv, 0);
#endif

	enter_mode = exynos_check_enter_mode();
	if (enter_mode == EXYNOS_CHECK_DIDLE)
		return exynos_enter_core0_aftr(dev, drv, new_index);
#ifdef CONFIG_SND_SAMSUNG_AUDSS
	else if (exynos_check_aud_pwr() == AUD_PWR_ALPA)
		return exynos_enter_core0_lpa(dev, drv, SYS_ALPA, new_index, enter_mode);
	else
#endif
		return exynos_enter_core0_lpa(dev, drv, SYS_LPA, new_index, enter_mode);
}

static int exynos_enter_idle(struct cpuidle_device *dev,
                                struct cpuidle_driver *drv,
                                int index)
{
	struct timeval before, after;
	int idle_time;

	local_fiq_disable();
	do_gettimeofday(&before);

	cpu_do_idle();

	do_gettimeofday(&after);
	local_fiq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		(after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;
	return index;
}

#if defined (CONFIG_EXYNOS_CPUIDLE_C2)
#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
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

	if (disabled_c3)
		return 0;

	for_each_cpu_and(cpu, cpu_possible_mask, cpu_coregroup_mask(cpu_id)) {
		if (cpu_id == cpu)
			continue;

		dev = per_cpu(tick_cpu_device, cpu).evtdev;
		if (!(per_cpu(in_c2_state, cpu)))
			return 0;

		if(ktime_to_us(ktime_sub(dev->next_event, now)) < CLUSTER_OFF_TARGET_RESIDENCY)
			return 0;
	}
	return 1;
#else
	return 0;
#endif
}
#endif

#ifdef CONFIG_CPUIDLE_TEST_SYSFS
unsigned int enter_counter[8] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int early_wakeup_counter[8] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int enter_residency[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int test_start = 0;
#endif

static int exynos_enter_c2(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time, ret = 0;
	unsigned int cpuid = smp_processor_id(), cpu_offset = 0;
	unsigned int temp;
	unsigned long flags = 0;

	if (!(cpuid & 0x4))
		return exynos_enter_idle(dev, drv, 0);

	local_fiq_disable();
	do_gettimeofday(&before);

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	set_boot_flag(cpuid, C2_STATE);
	cpu_pm_enter();

#ifdef CONFIG_CPUIDLE_TEST_SYSFS
	if (test_start == 'r') {
		int i;
		test_start = 0;
		for (i=0; i<8; i++) {
			enter_counter[i] = 0;
			early_wakeup_counter[i] = 0;
			enter_residency[i] = 0;
		}
	}

	if (test_start != 0)
		enter_counter[cpuid]++;
#endif

	cpu_offset = cpuid ^ 0x4;
	temp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));
	temp &= 0xfffffff0;
	__raw_writel(temp, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));

#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	if (index == 2) {
		per_cpu(in_c2_state, cpuid) = 1;
		if (can_enter_cluster_off(cpuid))
			flags = L2_CCI_OFF;
	}
#endif

	ret = cpu_suspend(flags, c2_finisher);
	if (ret) {
		temp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));
		temp |= 0xf;
		__raw_writel(temp, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset));
#ifdef CONFIG_CPUIDLE_TEST_SYSFS
		if (test_start != 0)
			early_wakeup_counter[cpuid]++;
#endif
	}

#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	exynos_cpu_sequencer_ctrl(false);

	if (cluster_off_flag && !disabled_c3) {
		cluster_off_time += get_jiffies_64() - last_time;

		spin_lock(&cluster_ctrl_lock);
		cluster_off_flag = false;
		spin_unlock(&cluster_ctrl_lock);
	}

	if (index == 2)
		per_cpu(in_c2_state, cpuid) = 0;
#endif

	clear_boot_flag(cpuid, C2_STATE);
	cpu_pm_exit();

	do_gettimeofday(&after);
	local_fiq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
		    (after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time;

#ifdef CONFIG_CPUIDLE_TEST_SYSFS
	if (test_start != 0)
		enter_residency[cpuid] += idle_time;

#if 0
	do {
		static unsigned int pre_div = 0, cur_div = 0;
		cur_div = __raw_readl(EXYNOS5430_DIV_EGL1);
		if (pre_div != cur_div) {
			printk("############################# %08x\n", __raw_readl(EXYNOS5430_DIV_EGL1));
			pre_div = cur_div;
		}
	while (0);
#endif

	if (!(enter_counter[cpuid] % 20) && (test_start == 's')) {
		printk("ver4 %d\n", idle_time);
		printk("%u %u %u %u\n",	enter_counter[4],
					enter_counter[5],
					enter_counter[6],
					enter_counter[7]);
		printk("%u %u %u %u\n",	early_wakeup_counter[4],
					early_wakeup_counter[5],
					early_wakeup_counter[6],
					early_wakeup_counter[7]);
		printk("%u %u %u %u\n",	enter_residency[4],
					enter_residency[5],
					enter_residency[6],
					enter_residency[7]);
	}
#endif

	return index;
}

#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
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

static struct dentry *lp_debugfs;

static int lp_debug_show(struct seq_file *s, void *unused)
{
	int i;

	seq_printf(s, "[ devices status for LP mode ]\n");

	seq_printf(s, "power domain status\n");
	for (i = 0; i < ARRAY_SIZE(exynos5_power_domain); i++)
		seq_printf(s, "\tpower_domain : (%d), status : (0x%08x)\n",
				i, __raw_readl(exynos5_power_domain[i].check_reg) &
				exynos5_power_domain[i].check_bit);

	seq_printf(s, "clock gating status\n");
	for (i = 0; i < ARRAY_SIZE(exynos5_clock_gating); i++)
		seq_printf(s, "\tclock_gating : (%d), status : (0x%08x)\n",
				i, __raw_readl(exynos5_clock_gating[i].check_reg) &
				exynos5_clock_gating[i].check_bit);

#if defined(CONFIG_MMC_DW)
	seq_printf(s, "dw_mci status\n");
	seq_printf(s, "\tdw_mci operation : (%d)\n", dw_mci_exynos_request_status());
#endif

#ifdef CONFIG_SAMSUNG_USBPHY
	seq_printf(s, "usbphy status\n");
	seq_printf(s, "\tusbphy operation : (%d)\n", samsung_usbphy_check_op());
#endif

	seq_printf(s, "dstop power domain status\n");
	for (i = 0; i < ARRAY_SIZE(exynos5_dstop_power_domain); i++)
		seq_printf(s, "\tdstop power_domain : (%d), status : (0x%08x)\n",
				i, __raw_readl(exynos5_dstop_power_domain[i].check_reg) &
				exynos5_dstop_power_domain[i].check_bit);

	return 0;
}

static int lp_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, lp_debug_show, inode->i_private);
}

const static struct file_operations lp_cdev_status_fops = {
	.open           = lp_debug_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

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
	int i, cpu_id, ret;
	struct cpuidle_device *device;

	exynos_idle_driver.state_count = ARRAY_SIZE(exynos5_cpuidle_set);

	for (i = 0; i < exynos_idle_driver.state_count; i++) {
		memcpy(&exynos_idle_driver.states[i],
				&exynos5_cpuidle_set[i],
				sizeof(struct cpuidle_state));
	}

	exynos_idle_driver.safe_state_index = 0;

	ret = cpuidle_register_driver(&exynos_idle_driver);
	if (ret) {
		printk(KERN_ERR "CPUidle register device failed\n,");
		return ret;
	}

	for_each_cpu(cpu_id, cpu_online_mask) {
		device = &per_cpu(exynos_cpuidle_device, cpu_id);
		device->cpu = cpu_id;

		device->state_count = exynos_idle_driver.state_count;
#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
		per_cpu(in_c2_state, cpu_id) = 0;
#endif

		if (cpuidle_register_device(device)) {
			printk(KERN_ERR "CPUidle register device failed\n,");
			return -EIO;
		}
	}

#if defined(CONFIG_EXYNOS_CLUSTER_POWER_DOWN) && defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
	disable_c3_idle = exynos_disable_c3_idle;
#endif

	register_pm_notifier(&exynos_cpuidle_notifier);

#if defined (CONFIG_SOC_EXYNOS5430_REV_1) && defined (CONFIG_EXYNOS_CLUSTER_POWER_DOWN)
	cluster_off_time_debugfs =
		debugfs_create_file("cluster_off_time",
				S_IRUGO, NULL, NULL, &cluster_off_time_fops);
	if (IS_ERR_OR_NULL(cluster_off_time_debugfs)) {
		cluster_off_time_debugfs = NULL;
		pr_err("%s: debugfs_create_file() failed\n", __func__);
	}

	spin_lock_init(&cluster_ctrl_lock);
#endif

	lp_debugfs =
		debugfs_create_file("lp_cdev_status",
				S_IRUGO, NULL, NULL, &lp_cdev_status_fops);
	if (IS_ERR_OR_NULL(lp_debugfs)) {
		lp_debugfs = NULL;
		pr_err("%s: debugfs_create_file() failed\n", __func__);
	}

#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
	exynos_idle_clock_down(true, ARM);
	exynos_idle_clock_down(true, KFC);
#endif

	return 0;
}
device_initcall(exynos_init_cpuidle);
