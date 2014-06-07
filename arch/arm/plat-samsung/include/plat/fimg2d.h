/* linux/arch/arm/plat-samsung/include/plat/fimg2d.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Platform Data Structure for Samsung Graphics 2D Hardware
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_FIMG2D_H
#define __ASM_ARCH_FIMG2D_H __FILE__

#define FIMG2D_SET_CLK_NAME		"aclk_333_g2d_dout"

#if	defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ)
#define CONFIG_FIMG2D_USE_BUS_DEVFREQ
#endif

#define EXYNOS5260_PA_SYSREG_G2D                0x10A20000
#define EXYNOS5422_PA_SYSREG_G2D		0x10CE0000
#define EXYNOS5430_PA_SYSREG_G2D		0x124C0000

#define EXYNOS5430_G2D_USER_CON                 0x1000
#define EXYNOS5430_G2DX_SHARED_VAL_MASK         (1 << 8)
#define EXYNOS5430_G2DX_SHARED_VAL_SHIFT        (8)
#define EXYNOS5430_G2DX_SHARED_SEL_MASK         (1 << 0)
#define EXYNOS5430_G2DX_SHARED_SEL_SHIFT        (0)

#define EXYNOS5430_G2D_NOC_DCG_EN		0x0200
#define EXYNOS5430_G2D_XIU_TOP_DCG_EN		0x0204
#define EXYNOS5430_G2D_AXI_US_DCG_EN		0x0208
#define EXYNOS5430_G2D_XIU_ASYNC_DCG_EN		0x020C
#define EXYNOS5430_G2D_DYN_CLKGATE_DISABLE	0x0500

#define EXYNOS5260_G2D_USER_CON                 0x0000
#define EXYNOS5260_G2D_AXUSER_SEL               0x0004

#define EXYNOS5260_G2D_ARUSER_SEL               (0x2)
#define EXYNOS5260_G2D_AWUSER_SEL               (0x1)
#define EXYNOS5260_G2D_SEL                      0x4000

enum fimg2d_ip_version {
	IP_VER_G2D_4P,
	IP_VER_G2D_5G,
	IP_VER_G2D_5A,
	IP_VER_G2D_5AR,
	IP_VER_G2D_5R,
	IP_VER_G2D_5V,
	IP_VER_G2D_5H,
	IP_VER_G2D_5AR2,
};

enum g2d_shared_val {
	NON_SHAREABLE_PATH = 0,
	SHAREABLE_PATH,
	SHAREABLE_VAL_END,
};

enum g2d_shared_sel {
	SHARED_FROM_SYSMMU = 0,
	SHARED_G2D_SEL,
	SHAREABLE_SEL_END,
};

struct fimg2d_platdata {
	int ip_ver;
	int hw_ver;
	const char *parent_clkname;
	const char *clkname;
	const char *gate_clkname;
	const char *gate_clkname2;
	unsigned long clkrate;
	int  cpu_min;
	int  kfc_min;
	int  mif_min;
	int  int_min;
};

extern void __init s5p_fimg2d_set_platdata(struct fimg2d_platdata *pd);
extern int g2d_cci_snoop_init(int ip_ver);
extern void g2d_cci_snoop_remove(int ip_ver);
extern int g2d_cci_snoop_control(int ip_ver
		, enum g2d_shared_val val, enum g2d_shared_sel sel);
extern int g2d_dynamic_clock_gating(int ip_ver);
#endif /* __ASM_ARCH_FIMG2D_H */
