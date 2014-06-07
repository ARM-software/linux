/*
 * arch/arm/mach-exynos/include/mach/exynos-tv.h
 *
 * Exynos TV driver core functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_TV_H
#define _EXYNOS_TV_H

#include <plat/devs.h>

/*
 * These functions are only for use with the core support code, such as
 * the CPU-specific initialization code.
 */

enum tv_ip_version {
	IP_VER_TV_5G_1,
	IP_VER_TV_5A_0,
	IP_VER_TV_5A_1,
	IP_VER_TV_5S,
	IP_VER_TV_5H,
	IP_VER_TV_5S2,
};

#define is_ip_ver_5g_1	(pdata->ip_ver == IP_VER_TV_5G_1)
#define is_ip_ver_5a_0	(pdata->ip_ver == IP_VER_TV_5A_0)
#define is_ip_ver_5a_1	(pdata->ip_ver == IP_VER_TV_5A_1)
#define is_ip_ver_5s	(pdata->ip_ver == IP_VER_TV_5S)
#define is_ip_ver_5h	(pdata->ip_ver == IP_VER_TV_5H)
#define is_ip_ver_5s2	(pdata->ip_ver == IP_VER_TV_5S2)
#define is_ip_ver_5g	(is_ip_ver_5g_1)
#define is_ip_ver_5a	(is_ip_ver_5a_0 || is_ip_ver_5a_1)
#define is_ip_ver_5	(is_ip_ver_5g || is_ip_ver_5a || is_ip_ver_5s || is_ip_ver_5h)

/* Re-define device name to differentiate the subsystem in various SoCs. */
static inline void s5p_hdmi_setname(char *name)
{
#ifdef CONFIG_S5P_DEV_TV
	s5p_device_hdmi.name = name;
#endif
}

static inline void s5p_mixer_setname(char *name)
{
#ifdef CONFIG_S5P_DEV_TV
	s5p_device_mixer.name = name;
#endif
}

struct s5p_hdmi_platdata {
	enum tv_ip_version ip_ver;
	void (*hdmiphy_enable)(struct platform_device *pdev, int en);
};

struct s5p_dex_platdata {
	enum tv_ip_version ip_ver;
};

struct s5p_mxr_platdata {
	enum tv_ip_version ip_ver;
};

extern void s5p_hdmi_set_platdata(struct s5p_hdmi_platdata *pd);

#endif /* _EXYNOS_TV_H */
