/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Base EXYNOS G2D resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fimg2d.h>
#include <mach/map.h>

#define S5P_PA_FIMG2D_OFFSET	0x02000000
#define S5P_PA_FIMG2D_3X	(S5P_PA_FIMG2D+S5P_PA_FIMG2D_OFFSET)

static void __iomem *sysreg_g2d_base;

#ifndef CONFIG_OF
static struct resource s5p_fimg2d_resource[] = {
	[0] = DEFINE_RES_MEM(S5P_PA_FIMG2D, SZ_4K),
	[1] = DEFINE_RES_IRQ(IRQ_2D),
};

struct platform_device s5p_device_fimg2d = {
	.name		= "s5p-fimg2d",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_fimg2d_resource),
	.resource	= s5p_fimg2d_resource
};

static struct fimg2d_platdata default_fimg2d_data __initdata = {
	.parent_clkname	= "mout_g2d0",
	.clkname	= "sclk_fimg2d",
	.gate_clkname	= "fimg2d",
	.clkrate	= 200 * MHZ,
};

void __init s5p_fimg2d_set_platdata(struct fimg2d_platdata *pd)
{
	struct fimg2d_platdata *npd;

	if (soc_is_exynos4210()) {
		s5p_fimg2d_resource[0].start = S5P_PA_FIMG2D_3X;
		s5p_fimg2d_resource[0].end = S5P_PA_FIMG2D_3X + SZ_4K - 1;
	}

	if (!pd)
		pd = &default_fimg2d_data;

	npd = s3c_set_platdata(pd, sizeof(struct fimg2d_platdata),
			&s5p_device_fimg2d);
	if (!npd)
		pr_err("%s: fail s3c_set_platdata()\n", __func__);
}
#endif

int g2d_cci_snoop_init(int ip_ver)
{
	switch (ip_ver) {
	case IP_VER_G2D_5R:
		sysreg_g2d_base = ioremap(EXYNOS5430_PA_SYSREG_G2D, 0x2000);
		if (!sysreg_g2d_base) {
			pr_err("syrreg_g2d_base ioremap is failed\n");
			return -ENOMEM;
		}
		break;

	case IP_VER_G2D_5H:
		sysreg_g2d_base = ioremap(EXYNOS5430_PA_SYSREG_G2D, 0x2000);
		if (!sysreg_g2d_base) {
			pr_err("syrreg_g2d_base ioremap is failed\n");
			return -ENOMEM;
		}
		break;

	case IP_VER_G2D_5AR:
		break;

	case IP_VER_G2D_5AR2:
		sysreg_g2d_base = ioremap(EXYNOS5422_PA_SYSREG_G2D, 0x1000);
		if (!sysreg_g2d_base) {
			pr_err("syrreg_g2d_base ioremap is failed\n");
			return -ENOMEM;
		}
		break;

	default:
		pr_err("syrreg_g2d_base ioremap is failed\n");
		break;
	}
	return 0;
}

void g2d_cci_snoop_remove(int ip_ver)
{
	switch (ip_ver) {
	case IP_VER_G2D_5R:
	case IP_VER_G2D_5H:
	case IP_VER_G2D_5AR:
	case IP_VER_G2D_5AR2:
		iounmap(sysreg_g2d_base);
		break;
	default:
		pr_err("syrreg_g2d_base iounmap is failed\n");
		break;
	}
}

int g2d_cci_snoop_control(int ip_ver
		, enum g2d_shared_val val, enum g2d_shared_sel sel)
{
	void __iomem *control_reg;
	unsigned int cfg;

	if ((val >= SHAREABLE_VAL_END) || (sel >= SHAREABLE_SEL_END)) {
		pr_err("g2d val or sel are out of range. val:%d, sel:%d\n"
				, val, sel);
		return -EINVAL;
	}

	switch (ip_ver) {
	case IP_VER_G2D_5R:

		control_reg = sysreg_g2d_base + EXYNOS5260_G2D_USER_CON;

		if (sel == SHARED_G2D_SEL) {
			/* disable cci path */
			cfg = (EXYNOS5260_G2D_ARUSER_SEL |
					EXYNOS5260_G2D_AWUSER_SEL);
			writel(cfg, control_reg);
	}

		control_reg = sysreg_g2d_base + EXYNOS5260_G2D_AXUSER_SEL;
		if (val == NON_SHAREABLE_PATH) {
			cfg = EXYNOS5260_G2D_SEL;
			writel(cfg, control_reg);
		}
		break;

	case IP_VER_G2D_5H:

		control_reg = sysreg_g2d_base + EXYNOS5430_G2D_USER_CON;

		cfg = readl(control_reg);
		cfg &= ~EXYNOS5430_G2DX_SHARED_VAL_MASK;
		cfg |= val << EXYNOS5430_G2DX_SHARED_VAL_SHIFT;

		cfg &= ~EXYNOS5430_G2DX_SHARED_SEL_MASK;
		cfg |= sel << EXYNOS5430_G2DX_SHARED_SEL_SHIFT;

		pr_debug("[%s:%d] control_reg :0x%p cfg:0x%x\n"
				,  __func__, __LINE__, control_reg, cfg);

		writel(cfg, control_reg);
		break;

	case IP_VER_G2D_5AR:
		/* G2D CCI off */
		/* cci.c : cci_init() : G2D_BACKBONE_SEL: 0x1 - CCIby-pass ON */
		break;

	case IP_VER_G2D_5AR2:
		control_reg = sysreg_g2d_base;
		/* G2D CCI off */
		/* G2D_BACKBONE_SEL: 0x1 - CCIby-pass ON */
		cfg = 0x1;
	writel(cfg, control_reg);
		break;
	default:
		pr_err("syrreg_g2d_base ioremap is failed\n");
		break;
	}

	return 0;
}

int g2d_dynamic_clock_gating(int ip_ver)
{
	void __iomem *control_reg;

	switch (ip_ver) {
	case IP_VER_G2D_5H:
		control_reg = sysreg_g2d_base + EXYNOS5430_G2D_NOC_DCG_EN;
		/* Enable ACLK_G2DND_400, ACLK_G2DNP_133 */
		writel(0x3, control_reg);

		control_reg = sysreg_g2d_base + EXYNOS5430_G2D_XIU_TOP_DCG_EN;
		/* Enable G2DX */
		writel(0x1, control_reg);

		control_reg = sysreg_g2d_base + EXYNOS5430_G2D_AXI_US_DCG_EN;
		/* Enable G2DX_S0 */
		writel(0x1, control_reg);

		control_reg = sysreg_g2d_base + EXYNOS5430_G2D_XIU_ASYNC_DCG_EN;
		/* Enable G2DX_S0 */
		writel(0x1, control_reg);

		control_reg = sysreg_g2d_base + EXYNOS5430_G2D_DYN_CLKGATE_DISABLE;
		/* Enable G2D */
		writel(0x0, control_reg);

		break;
	}

	return 0;
}
