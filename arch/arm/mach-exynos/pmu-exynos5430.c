/* linux/arch/arm/mach-exynos/pmu.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/bug.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include "common.h"

#define REG_CPU_STATE_ADDR     (S5P_VA_SYSRAM_NS + 0x28)

static struct exynos_pmu_conf *exynos_pmu_config;

static struct exynos_pmu_conf exynos5430_pmu_config[] = {
	/* { .reg = address, .val = { AFTR, LPD, LPA, ALPA, DSTOP, DSTOP_PSR, SLEEP } */
	{ EXYNOS5_ARM_CORE0_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1000 */
	{ EXYNOS5_DIS_IRQ_ARM_CORE0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1008 */
	{ EXYNOS5_ARM_CORE1_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1010 */
	{ EXYNOS5_DIS_IRQ_ARM_CORE1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1018 */
	{ EXYNOS54XX_ARM_CORE2_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1020 */
	{ EXYNOS54XX_DIS_IRQ_ARM_CORE2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1028 */
	{ EXYNOS54XX_ARM_CORE3_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1030 */
	{ EXYNOS54XX_DIS_IRQ_ARM_CORE3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1038 */
	{ EXYNOS54XX_KFC_CORE0_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1040 */
	{ EXYNOS54XX_DIS_IRQ_KFC_CORE0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1048 */
	{ EXYNOS54XX_KFC_CORE1_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1050 */
	{ EXYNOS54XX_DIS_IRQ_KFC_CORE1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1058 */
	{ EXYNOS54XX_KFC_CORE2_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1060 */
	{ EXYNOS54XX_DIS_IRQ_KFC_CORE2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1068 */
	{ EXYNOS54XX_KFC_CORE3_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1070 */
	{ EXYNOS54XX_DIS_IRQ_KFC_CORE3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1078 */
	{ EXYNOS5430_EAGLE_NONCPU_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1080 */
	{ EXYNOS5430_KFC_NONCPU_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8} }, /* 1084 */
	{ EXYNOS5430_A5IS_SYS_PWR_REG,				{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 10B0 */
	{ EXYNOS5430_DIS_IRQ_A5IS_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 10B4 */
	{ EXYNOS5430_DIS_IRQ_A5IS_CENTRAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 10B8 */
	{ EXYNOS5_ARM_L2_SYS_PWR_REG,				{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7} }, /* 10C0 */
	{ EXYNOS5430_KFC_L2_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7} }, /*_10C4 */
	{ EXYNOS5430_CLKSTOP_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1100 */
	{ EXYNOS5430_CLKRUN_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1104 */
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	{ EXYNOS5430_RESET_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 110C */
	{ EXYNOS5430_CLKRUN_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1124 */
	{ EXYNOS5430_RESET_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 112C */
#else
	{ EXYNOS5430_RESET_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 110C */
	{ EXYNOS5430_CLKRUN_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 1124 */
	{ EXYNOS5430_RESET_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 112C */
#endif
	{ EXYNOS5430_RESET_CPUCLKSTOP_SYS_PWR_REG,		{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 111C */
	{ EXYNOS5430_CLKSTOP_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1120 */
	{ EXYNOS5_DDRPHY_DLLLOCK_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} }, /* 1138 */
	{ EXYNOS5430_DISABLE_PLL_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1140 */
	{ EXYNOS5430_DISABLE_PLL_AUD_PLL_SYS_PWR_REG,		{ 0x1, 0x0, 0x1, 0x1, 0x0, 0x0, 0x0} }, /* 1144 */
	{ EXYNOS5430_DISABLE_PLL_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1160 */
	{ EXYNOS5_TOP_BUS_SYS_PWR_REG,				{ 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1180 */
	{ EXYNOS5_TOP_RETENTION_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1} }, /* 1184 */
	{ EXYNOS5_TOP_PWR_SYS_PWR_REG,				{ 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3} }, /* 1188 */
	{ EXYNOS5430_TOP_BUS_MIF_SYS_PWR_REG,			{ 0x7, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1190 */
	{ EXYNOS5430_TOP_RETENTION_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x1} }, /* 1194 */
	{ EXYNOS5430_TOP_PWR_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x3} }, /* 1198 */
	{ EXYNOS5_LOGIC_RESET_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 11A0 */
	{ EXYNOS5_OSCCLK_GATE_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1} }, /* 11A4 */
	{ EXYNOS5430_SLEEP_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 11A8 */
	{ EXYNOS5430_LOGIC_RESET_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 11B0 */
	{ EXYNOS5430_OSCCLK_GATE_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x1} }, /* 11B4 */
	{ EXYNOS5430_SLEEP_RESET_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 11B8 */
	{ EXYNOS5430_MEMORY_TOP_SYS_PWR_REG,			{ 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 11C0 */
	{ EXYNOS5_PAD_RETENTION_DRAM_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1200 */
	{ EXYNOS5430_PAD_RETENTION_JTAG_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1208 */
	{ EXYNOS5430_PAD_RETENTION_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1220 */
	{ EXYNOS5430_PAD_RETENTION_UART_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1224 */
	{ EXYNOS5430_PAD_RETENTION_EBIA_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1230 */
	{ EXYNOS5430_PAD_RETENTION_EBIB_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1234 */
	{ EXYNOS5430_PAD_RETENTION_SPI_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1238 */
	{ EXYNOS5430_PAD_RETENTION_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 123C */
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	{ EXYNOS5_PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1240 */
	{ EXYNOS5430_PAD_ISOLATION_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1250 */
#else
	{ EXYNOS5_PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1} }, /* 1240 */
	{ EXYNOS5430_PAD_ISOLATION_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x1} }, /* 1250 */
#endif
	{ EXYNOS5430_PAD_RETENTION_USBXTI_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1244 */
	{ EXYNOS5430_PAD_RETENTION_BOOTLDO_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1248 */
	{ EXYNOS5430_PAD_RETENTION_FSYSGENIO_SYS_PWR_REG,	{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1254 */
	{ EXYNOS5_PAD_ALV_SEL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1260 */
	{ EXYNOS5_XXTI_SYS_PWR_REG,				{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 1284 */
	{ EXYNOS5430_XXTI26_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1288 */
	{ EXYNOS5_EXT_REGULATOR_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 12C0 */
	{ EXYNOS5_GPIO_MODE_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1300 */
	{ EXYNOS5430_GPIO_MODE_FSYS_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1304 */
	{ EXYNOS5430_GPIO_MODE_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1320 */
	{ EXYNOS5430_GPIO_MODE_AUD_SYS_PWR_REG,			{ 0x1, 0x0, 0x1, 0x1, 0x0, 0x0, 0x0} }, /* 1340 */
	{ EXYNOS5430_GSCL_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1400 */
	{ EXYNOS5430_CAM0_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1404 */
	{ EXYNOS5430_MSCL_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1408 */
	{ EXYNOS5430_G3D_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 140C */
	{ EXYNOS5430_DISP_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0xF, 0x0} }, /* 1410 */
	{ EXYNOS5430_CAM1_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1414 */
	{ EXYNOS5430_AUD_SYS_PWR_REG,				{ 0xF, 0x0, 0xF, 0xF, 0x0, 0x0, 0x0} }, /* 1418 */
	{ EXYNOS5430_FSYS_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 141C */
	{ EXYNOS5430_BUS2_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1420 */
	{ EXYNOS5430_G2D_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1424 */
	{ EXYNOS5430_ISP_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1428 */
	{ EXYNOS5430_MFC0_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1430 */
	{ EXYNOS5430_MFC1_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1434 */
	{ EXYNOS5430_HEVC_SYS_PWR_REG,				{ 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} }, /* 1438 */
	{ EXYNOS5430_RESET_SLEEP_FSYS_SYS_PWR_REG,		{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 15DC */
	{ EXYNOS5430_RESET_SLEEP_BUS2_SYS_PWR_REG,		{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} }, /* 15E0 */
	{ PMU_TABLE_END,},
};

void __iomem *exynos5_list_disable_pmu_reg[] = {
	EXYNOS5_CMU_SYSCLK_ISP_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_ISP_SYS_PWR_REG,
};

void __iomem *exynos_list_feed[] = {
	EXYNOS_ARM_CORE_OPTION(0),
	EXYNOS_ARM_CORE_OPTION(1),
	EXYNOS_ARM_CORE_OPTION(2),
	EXYNOS_ARM_CORE_OPTION(3),
	EXYNOS_ARM_CORE_OPTION(4),
	EXYNOS_ARM_CORE_OPTION(5),
	EXYNOS_ARM_CORE_OPTION(6),
	EXYNOS_ARM_CORE_OPTION(7),
	EXYNOS5430_EAGLE_NONCPU_OPTION,
	EXYNOS5430_KFC_NONCPU_OPTION,
	EXYNOS5_TOP_PWR_OPTION,
	EXYNOS5_TOP_PWR_SYSMEM_OPTION,
	EXYNOS5430_GSCL_OPTION,
	EXYNOS5430_CAM0_OPTION,
	EXYNOS5430_MSCL_OPTION,
	EXYNOS5430_G3D_OPTION,
	EXYNOS5430_DISP_OPTION,
	EXYNOS5430_CAM1_OPTION,
	EXYNOS5430_AUD_OPTION,
	EXYNOS5430_FSYS_OPTION,
	EXYNOS5430_BUS2_OPTION,
	EXYNOS5430_G2D_OPTION,
	EXYNOS5430_ISP_OPTION,
	EXYNOS5430_MFC0_OPTION,
	EXYNOS5430_MFC1_OPTION,
	EXYNOS5430_HEVC_OPTION,
};

void set_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);
	addr = REG_CPU_STATE_ADDR + 4 * (core + cluster * 4);

	tmp = __raw_readl(addr);

	if (mode & BOOT_MODE_MASK)
		tmp &= ~BOOT_MODE_MASK;
	else
		BUG_ON(mode == 0);

	tmp |= mode;
	__raw_writel(tmp, addr);
}

void clear_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	BUG_ON(mode == 0);

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);
	addr = REG_CPU_STATE_ADDR + 4 * (core + cluster * 4);

	tmp = __raw_readl(addr);
	tmp &= ~mode;
	__raw_writel(tmp, addr);
}

void exynos5430_secondary_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_INIT_WAKEUP_FROM_LOWPWR | EXYNOS_CORE_PWR_EN;

	__raw_writel(tmp, addr);
}

void exynos5430_cpu_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_PWR_EN;
	__raw_writel(tmp, addr);
}

void exynos5430_cpu_down(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp &= ~(EXYNOS_CORE_PWR_EN);
	__raw_writel(tmp, addr);
}

unsigned int exynos5430_cpu_state(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core, cluster, val;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	val = __raw_readl(EXYNOS_ARM_CORE_STATUS(core + (4 * cluster)))
						& EXYNOS_CORE_PWR_EN;

	return val == EXYNOS_CORE_PWR_EN;
}

unsigned int exynos5430_cluster_state(unsigned int cluster)
{
	unsigned int cpu_start, cpu_end, ret;

	BUG_ON(cluster > 2);

	cpu_start = (cluster) ? 4 : 0;
	cpu_end = cpu_start + 4;

	for (;cpu_start < cpu_end; cpu_start++) {
		ret = exynos5430_cpu_state(cpu_start);
		if (ret)
			break;
	}

	return ret ? 1 : 0;
}

extern struct cpumask hmp_slow_cpu_mask;
extern struct cpumask hmp_fast_cpu_mask;

#define cpu_online_hmp(cpu, mask)      cpumask_test_cpu((cpu), mask)

bool exynos5430_is_last_core(unsigned int cpu)
{
	unsigned int cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	unsigned int cpu_id;
	struct cpumask mask, mask_and_online;

	if (cluster)
		cpumask_copy(&mask, &hmp_slow_cpu_mask);
	else
		cpumask_copy(&mask, &hmp_fast_cpu_mask);

	cpumask_and(&mask_and_online, &mask, cpu_online_mask);

	for_each_cpu(cpu_id, &mask) {
		if (cpu_id == cpu)
			continue;
		if (cpu_online_hmp(cpu_id, &mask_and_online))
			return false;
	}

	return true;
}

unsigned int exynos5430_l2_status(unsigned int cluster)
{
	unsigned int state;

	BUG_ON(cluster > 2);

	state = __raw_readl(EXYNOS_L2_STATUS(cluster));

	return state & EXYNOS_L2_PWR_EN;
}

void exynos5430_l2_up(unsigned int cluster)
{
	unsigned int tmp = (EXYNOS_L2_PWR_EN << 8) | EXYNOS_L2_PWR_EN;

	if (exynos5430_l2_status(cluster))
		return;

	tmp |= __raw_readl(EXYNOS_L2_CONFIGURATION(cluster));
	__raw_writel(tmp, EXYNOS_L2_CONFIGURATION(cluster));

	/* wait for turning on */
	while (exynos5430_l2_status(cluster));
}

void exynos5430_l2_down(unsigned int cluster)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS_L2_CONFIGURATION(cluster));
	tmp &= ~(EXYNOS_L2_PWR_EN);

	__raw_writel(tmp, EXYNOS_L2_CONFIGURATION(cluster));
}

static void exynos_use_feedback(void)
{
	unsigned int i;
	unsigned int tmp;

	/*
	 * Enable only SC_FEEDBACK
	 */
	for (i = 0; i < ARRAY_SIZE(exynos_list_feed); i++) {
		tmp = __raw_readl(exynos_list_feed[i]);
		tmp &= ~EXYNOS5_USE_SC_COUNTER;
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		__raw_writel(tmp, exynos_list_feed[i]);
	}
}

void exynos_sys_powerdown_conf(enum sys_powerdown mode)
{
	unsigned int i;
#ifdef CONFIG_SOC_EXYNOS5430_REV_1
	unsigned int tmp;

	/* Enable non retention flip-flop reset */
	tmp = __raw_readl(EXYNOS_PMU_SPARE0);
	tmp |= EXYNOS_EN_NONRET_RESET;
	__raw_writel(tmp, EXYNOS_PMU_SPARE0);
#endif

	for (i = 0; (exynos_pmu_config[i].reg != PMU_TABLE_END) ; i++)
		__raw_writel(exynos_pmu_config[i].val[mode],
				exynos_pmu_config[i].reg);
}

void exynos_xxti_sys_powerdown(bool enable)
{
	unsigned int value;

	value = __raw_readl(EXYNOS5_XXTI_SYS_PWR_REG);

	if (enable)
		value |= EXYNOS_SYS_PWR_CFG;
	else
		value &= ~EXYNOS_SYS_PWR_CFG;

	__raw_writel(value, EXYNOS5_XXTI_SYS_PWR_REG);
}

void exynos_cpu_sequencer_ctrl(bool enable)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5430_EAGLE_CPUSEQ_OPTION);
	if (enable)
		tmp |= ENABLE_CPUSEQ;
	else
		tmp &= ~ENABLE_CPUSEQ;
	__raw_writel(tmp, EXYNOS5430_EAGLE_CPUSEQ_OPTION);
}

static void exynos_cpu_reset_assert_ctrl(bool on, enum cpu_type cluster)
{
	unsigned int i;
	unsigned int option;
	unsigned int cpu_s, cpu_f;

	if (cluster == KFC) {
		cpu_s = CPUS_PER_CLUSTER;
		cpu_f = cpu_s + CPUS_PER_CLUSTER - 1;
	} else {
		cpu_s = 0;
		cpu_f = CPUS_PER_CLUSTER - 1;
	}

	for (i = cpu_s; i <= cpu_f; i++) {
		option = __raw_readl(EXYNOS_ARM_CORE_OPTION(i));
		option = on ? (option | EXYNOS_USE_DELAYED_RESET_ASSERTION) :
				   (option & ~EXYNOS_USE_DELAYED_RESET_ASSERTION);
		__raw_writel(option, EXYNOS_ARM_CORE_OPTION(i));
	}

}

void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type)
{
	unsigned int value;
	unsigned int wdt_auto_reset_dis;
	unsigned int wdt_reset_mask;

	/*
	 * When SYS_WDTRESET is set, watchdog timer reset request is ignored
	 * by power management unit.
	 */
	if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE0) {
		wdt_auto_reset_dis = EXYNOS_SYS_WDTRESET;
		wdt_reset_mask = EXYNOS_SYS_WDTRESET;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE1) {
		wdt_auto_reset_dis = EXYNOS5410_SYS_WDTRESET;
		wdt_reset_mask = EXYNOS5410_SYS_WDTRESET;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE2) {
		wdt_auto_reset_dis = EXYNOS5430_SYS_WDTRESET_EGL |
			EXYNOS5430_SYS_WDTRESET_KFC;
		wdt_reset_mask = EXYNOS5430_SYS_WDTRESET_EGL;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE3) {
		wdt_auto_reset_dis = EXYNOS5430_SYS_WDTRESET_EGL |
			EXYNOS5430_SYS_WDTRESET_KFC;
		wdt_reset_mask = EXYNOS5430_SYS_WDTRESET_KFC;
	} else {
		pr_err("Failed to %s pmu wdt reset\n",
				on ? "enable" : "disable");
		return;
	}

	value = __raw_readl(EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	if (on)
		value &= ~wdt_auto_reset_dis;
	else
		value |= wdt_auto_reset_dis;
	__raw_writel(value, EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	value = __raw_readl(EXYNOS_MASK_WDT_RESET_REQUEST);
	if (on)
		value &= ~wdt_reset_mask;
	else
		value |= wdt_reset_mask;
	__raw_writel(value, EXYNOS_MASK_WDT_RESET_REQUEST);

	return;
}

#define EXYNOS5430_PRINT_PMU(name) \
	pr_info("  - %s  CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n", \
			#name, \
			__raw_readl(EXYNOS5430_##name##_CONFIGURATION), \
			__raw_readl(EXYNOS5430_##name##_STATUS), \
			__raw_readl(EXYNOS5430_##name##_OPTION))

void show_exynos_pmu(void)
{
	int i;
	pr_info("\n");
	pr_info(" -----------------------------------------------------------------------------------\n");
	pr_info(" **** CPU PMU register dump ****\n");
	for (i=0; i < NR_CPUS; i++) {
		printk("[%d]   CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n", i,
			__raw_readl(EXYNOS_ARM_CORE0_CONFIGURATION + i * 0x80),
			__raw_readl(EXYNOS_ARM_CORE0_STATUS + i * 0x80),
			__raw_readl(EXYNOS_ARM_CORE0_OPTION + i * 0x80));
	}
	pr_info(" **** EGL NONCPU CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_COMMON_CONFIGURATION(0)),
			__raw_readl(EXYNOS_COMMON_STATUS(0)),
			__raw_readl(EXYNOS_COMMON_OPTION(0)));
	pr_info(" **** EGL L2 CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_L2_CONFIGURATION(0)),
			__raw_readl(EXYNOS_L2_STATUS(0)),
			__raw_readl(EXYNOS_L2_OPTION(0)));
	pr_info(" **** KFC NONCPU CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_COMMON_CONFIGURATION(1)),
			__raw_readl(EXYNOS_COMMON_STATUS(1)),
			__raw_readl(EXYNOS_COMMON_OPTION(1)));
	pr_info(" **** KFC L2 CONFIG : 0x%x  STATUS : 0x%x  OPTION : 0x%x\n",
			__raw_readl(EXYNOS_L2_CONFIGURATION(1)),
			__raw_readl(EXYNOS_L2_STATUS(1)),
			__raw_readl(EXYNOS_L2_OPTION(1)));
	pr_info(" **** LOCAL BLOCK POWER ****\n");
	EXYNOS5430_PRINT_PMU(GSCL);
	EXYNOS5430_PRINT_PMU(CAM0);
	EXYNOS5430_PRINT_PMU(CAM1);
	EXYNOS5430_PRINT_PMU(MSCL);
	EXYNOS5430_PRINT_PMU(G3D);
	EXYNOS5430_PRINT_PMU(DISP);
	EXYNOS5430_PRINT_PMU(AUD);
	EXYNOS5430_PRINT_PMU(FSYS);
	EXYNOS5430_PRINT_PMU(BUS2);
	EXYNOS5430_PRINT_PMU(G2D);
	EXYNOS5430_PRINT_PMU(ISP);
	EXYNOS5430_PRINT_PMU(MFC0);
	EXYNOS5430_PRINT_PMU(MFC1);
	EXYNOS5430_PRINT_PMU(HEVC);
	pr_info(" -----------------------------------------------------------------------------------\n");
}

int __init exynos5430_pmu_init(void)
{
	unsigned int tmp;
	/*
	 * Set measure power on/off duration
	 * Use SC_USE_FEEDBACK
	 */
	exynos_use_feedback();

	/* Enable USE_STANDBY_WFI for all CORE */
	__raw_writel(EXYNOS5_USE_STANDBY_WFI_ALL |
		EXYNOS_USE_PROLOGNED_LOGIC_RESET, EXYNOS_CENTRAL_SEQ_OPTION);

	exynos_cpu_reset_assert_ctrl(true, ARM);

	/* L2 use retention disable */
	tmp = __raw_readl(EXYNOS_L2_OPTION(0));
	tmp &= ~USE_RETENTION;
	tmp |= USE_STANDBYWFIL2 | USE_DEACTIVATE_ACP | USE_DEACTIVATE_ACE;
	__raw_writel(tmp, EXYNOS_L2_OPTION(0));

	tmp = __raw_readl(EXYNOS_L2_OPTION(1));
	tmp &= ~USE_RETENTION;
	__raw_writel(tmp, EXYNOS_L2_OPTION(1));

	/* UP Scheduler Enable */
	tmp = __raw_readl(EXYNOS5_UP_SCHEDULER);
	tmp |= ENABLE_EAGLE_CPU;
	__raw_writel(tmp, EXYNOS5_UP_SCHEDULER);

	/*
	 * Set PSHOLD port for output high
	 */
	tmp = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
	tmp |= EXYNOS_PS_HOLD_OUTPUT_HIGH;
	__raw_writel(tmp, EXYNOS_PS_HOLD_CONTROL);

	/*
	 * Enable signal for PSHOLD port
	 */
	tmp = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
	tmp |= EXYNOS_PS_HOLD_EN;
	__raw_writel(tmp, EXYNOS_PS_HOLD_CONTROL);

#ifdef CONFIG_SOC_EXYNOS5430_REV_1
	/* Enable non retention flip-flop reset */
	tmp = __raw_readl(EXYNOS_PMU_SPARE0);
	tmp |= EXYNOS_EN_NONRET_RESET;
	__raw_writel(tmp, EXYNOS_PMU_SPARE0);
#endif

	exynos_pmu_config = exynos5430_pmu_config;
	exynos_cpu.power_up = exynos5430_secondary_up;
	exynos_cpu.power_state = exynos5430_cpu_state;
	exynos_cpu.power_down = exynos5430_cpu_down;
	exynos_cpu.cluster_down = exynos5430_l2_down;
	exynos_cpu.cluster_state = exynos5430_cluster_state;
	exynos_cpu.is_last_core = exynos5430_is_last_core;

	if (exynos_pmu_config != NULL)
		pr_info("EXYNOS5430 PMU Initialize\n");
	else
		pr_info("EXYNOS: PMU not supported\n");

	return 0;
}
