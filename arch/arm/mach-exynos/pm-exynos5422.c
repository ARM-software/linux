/* linux/arch/arm/mach-exynos/pm.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <asm/cacheflush.h>
#include <asm/smp_scu.h>
#include <asm/cputype.h>
#include <asm/firmware.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/pll.h>
#include <plat/regs-srom.h>
#include <plat/gpio-cfg.h>

#include <mach/bts.h>
#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>

#define PM_PREFIX	"PM: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef PM_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

#define REG_INFORM0            (S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1            (S5P_VA_SYSRAM_NS + 0xC)

#define EXYNOS_I2C_CFG		(S3C_VA_SYS + 0x234)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

#define CCI_PA			0x10d20000
#define SECURE_ACCESS_REG	0x8
#define CHECK_CCI_SNOOP		(1 << 7)
extern void cci_snoop_enable(unsigned int sif);

static unsigned int read_mpidr(void)
{
	unsigned int id;
	asm volatile ("mrc\tp15, 0, %0, c0, c0, 5" : "=r" (id));
	return id;
}

static struct sleep_save exynos5422_set_clksrc[] = {
	{ .reg = EXYNOS5_CLK_SRC_MASK_CPERI,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP0,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP1,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP2,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP7,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_DISP10,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_MAUDIO,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_FSYS,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_PERIC0,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_PERIC1,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_ISP,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLK_GATE_BUS_DISP1,		.val = 0xffffffff, },
};

static struct sleep_save exynos5422_enable_xxti[] = {
	{ .reg = EXYNOS5422_XXTI_SYS_PWR_REG,		.val = 0x1, },
};

static struct sleep_save exynos_core_save[] = {
	/* SROM side */
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),

	/* I2C CFG */
	SAVE_ITEM(EXYNOS_I2C_CFG),
};

static struct sleep_save exynos5422_core_save[] = {
	SAVE_ITEM(S3C_VA_SYS + 0x400),
	SAVE_ITEM(S3C_VA_SYS + 0x404),
};

static int exynos_cpu_suspend(unsigned long arg)
{
	unsigned int cluster_id = (read_mpidr() >> 8) & 0xf;
	unsigned int i, tmp, cpu_offset = ((cluster_id == 0) ? 0 : 4);
	int value = 0, loops = 0;

	/* flush cache back to ram */
	flush_cache_all();

	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020028), value, 0);

	/* W/A for kfc */
	for (i = 0; i < 4; i++) {
		if (i == 0)
			continue;

		__raw_writel(0x3, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset + i));

		/* Wait until changing core status during 5ms */
		loops = msecs_to_loops(5);
		do {
			if (--loops == 0)
				BUG();
			tmp = __raw_readl(EXYNOS_ARM_CORE_STATUS(cpu_offset + i));
		} while ((tmp & 0x3) != 0x3);
	}

	/* issue the standby signal into the pm unit. */
	if (call_firmware_op(do_idle))
		cpu_do_idle();
	pr_info("sleep resumed to originator?");

	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020028), 0xfcba0d10, 0);

	/* W/A for kfc */
	for (i = 0; i < 4; i++) {
		if (i == 0)
			continue;

		__raw_writel(0x0, EXYNOS_ARM_CORE_CONFIGURATION(cpu_offset + i));

		/* Wait until changing core status during 5ms */
		loops = msecs_to_loops(5);
		do {
			if (--loops == 0)
				BUG();
			tmp = __raw_readl(EXYNOS_ARM_CORE_STATUS(cpu_offset + i));
		} while (tmp & 0x3);
	}

	return 1; /* abort suspend */
}

static void exynos_pm_prepare(void)
{
	unsigned int tmp;

	/* Set value of power down register for sleep mode */
	exynos_sys_powerdown_conf(SYS_SLEEP);

	if (!(__raw_readl(EXYNOS5422_PMU_DEBUG) & 0x1))
		s3c_pm_do_restore_core(exynos5422_enable_xxti,
				ARRAY_SIZE(exynos5422_enable_xxti));

	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	s3c_pm_do_restore_core(exynos5422_set_clksrc, ARRAY_SIZE(exynos5422_set_clksrc));

	tmp = __raw_readl(EXYNOS5422_ARM_L2_OPTION);
	tmp &= ~EXYNOS5_USE_RETENTION;
	__raw_writel(tmp, EXYNOS5422_ARM_L2_OPTION);

	tmp = __raw_readl(EXYNOS5422_SFR_AXI_CGDIS1);
	tmp |= (EXYNOS5422_UFS | EXYNOS5422_ACE_KFC | EXYNOS5422_ACE_EAGLE);
	__raw_writel(tmp, EXYNOS5422_SFR_AXI_CGDIS1);

	tmp = __raw_readl(EXYNOS5422_ARM_COMMON_OPTION);
	tmp &= ~(1<<3);
	__raw_writel(tmp, EXYNOS5422_ARM_COMMON_OPTION);

	tmp = __raw_readl(EXYNOS5422_FSYS2_OPTION);
	tmp |= EXYNOS5422_EMULATION;
	__raw_writel(tmp, EXYNOS5422_FSYS2_OPTION);

	tmp = __raw_readl(EXYNOS5422_PSGEN_OPTION);
	tmp |= EXYNOS5422_EMULATION;
	__raw_writel(tmp, EXYNOS5422_PSGEN_OPTION);
}

static int exynos_pm_suspend(void)
{
	unsigned long tmp;
	unsigned int count = 10000;

	do {
		tmp = __raw_readl(EXYNOS5422_ARM_COMMON_STATUS) & 0x3;
		udelay(10);
		count--;
	} while (tmp && count);

	if (count == 0) {
		pr_err("Non-cpu block of A15 cluster is powered on\n");
		return -EAGAIN;
	}

	s3c_pm_do_save(exynos5422_core_save, ARRAY_SIZE(exynos5422_core_save));

	s3c_pm_do_save(exynos_core_save, ARRAY_SIZE(exynos_core_save));

	__raw_writel(0x00F00F00, EXYNOS5422_CENTRAL_SEQ_OPTION);

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);

	return 0;
}

static void exynos_pm_resume(void)
{
	unsigned long tmp;
	int i;

	__raw_writel(EXYNOS5422_USE_STANDBY_WFI_ALL,
		EXYNOS5422_CENTRAL_SEQ_OPTION);

	/* HACK */
	exynos_smc(SMC_CMD_REG,
		   SMC_REG_ID_SFR_W(CCI_PA + SECURE_ACCESS_REG),
		   1,
		   0);
	cci_snoop_enable(3);
	for_each_cpu(i, cpu_coregroup_mask(4))
		__raw_writel(HOTPLUG | CHECK_CCI_SNOOP, S5P_VA_SYSRAM_NS + 0x18 + 4 * i);

	/*
	 * If PMU failed while entering sleep mode, WFI will be
	 * ignored by PMU and then exiting cpu_do_idle().
	 * S5P_CENTRAL_LOWPWR_CFG bit will not be set automatically
	 * in this situation.
	 */
	tmp = __raw_readl(EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
	if (!(tmp & EXYNOS_CENTRAL_LOWPWR_CFG)) {
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS5422_CENTRAL_SEQ_CONFIGURATION);
		/* No need to perform below restore code */
		goto early_wakeup;
	}

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_DRAM_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MAU_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_JTAG_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_GPIO_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MMCA_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MMCB_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_MMCC_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_HSI_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS5422_PAD_RETENTION_DRAM_COREBLK_OPTION);

	s3c_pm_do_restore_core(exynos_core_save, ARRAY_SIZE(exynos_core_save));

	s3c_pm_do_restore_core(exynos5422_core_save, ARRAY_SIZE(exynos5422_core_save));

	bts_initialize(NULL, true);

early_wakeup:

	tmp = __raw_readl(EXYNOS5422_SFR_AXI_CGDIS1);
	tmp &= ~(EXYNOS5422_UFS | EXYNOS5422_ACE_KFC | EXYNOS5422_ACE_EAGLE);
	__raw_writel(tmp, EXYNOS5422_SFR_AXI_CGDIS1);

	tmp = __raw_readl(EXYNOS5422_FSYS2_OPTION);
	tmp &= ~EXYNOS5422_EMULATION;
	__raw_writel(tmp, EXYNOS5422_FSYS2_OPTION);

	tmp = __raw_readl(EXYNOS5422_PSGEN_OPTION);
	tmp &= ~EXYNOS5422_EMULATION;
	__raw_writel(tmp, EXYNOS5422_PSGEN_OPTION);

	return;
}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep = exynos_pm_prepare;
	pm_cpu_sleep = exynos_cpu_suspend;

	return 0;
}

static struct subsys_interface exynos_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos_subsys,
	.add_dev	= exynos_pm_add,
};

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_drvinit(void)
{
	s3c_pm_init();

	return subsys_interface_register(&exynos_pm_interface);
}
arch_initcall(exynos_pm_drvinit);

static __init int exynos_pm_syscore_init(void)
{
	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
