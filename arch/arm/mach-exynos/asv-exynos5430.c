/* linux/arch/arm/mach-exynos/asv-exynos5430.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5430 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos5430.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>

#define CHIP_ID2_REG			(S5P_VA_CHIPID + 0x4)
#define EXYNOS5430_IDS_OFFSET		(16)
#define EXYNOS5430_IDS_MASK		(0xFF)
#define ASV_MEM_SIZE_SHIFT		(4)
#define ASV_MEM_SIZE_MASK		(0x3)

#define ASV_ARM_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x10)
#define ASV_ARM_GRP_1_OFFSET		(0)
#define ASV_ARM_GRP_2_OFFSET		(4)
#define ASV_ARM_GRP_3_OFFSET		(8)
#define ASV_ARM_VOL_LOCK_OFFSET		(12)

#define ASV_KFC_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x14)
#define ASV_KFC_GRP_1_OFFSET		(0)
#define ASV_KFC_GRP_2_OFFSET		(4)
#define ASV_KFC_GRP_3_OFFSET		(8)
#define ASV_KFC_VOL_LOCK_OFFSET		(12)

#define ASV_G3D_MIF_SPEED_GRP_REG	(S5P_VA_CHIPID2 + 0x18)
#define ASV_G3D_GRP_1_OFFSET		(0)
#define ASV_G3D_GRP_2_OFFSET		(4)
#define ASV_G3D_GRP_3_OFFSET		(8)
#define ASV_G3D_VOL_LOCK_OFFSET		(12)
#define ASV_MIF_GRP_1_OFFSET		(16)
#define ASV_MIF_GRP_2_OFFSET		(20)
#define ASV_MIF_GRP_3_OFFSET		(24)
#define ASV_MIF_VOL_LOCK_OFFSET		(28)

#define ASV_INT_ISP_SPEED_GRP_REG	(S5P_VA_CHIPID2 + 0x1C)
#define ASV_INT_GRP_1_OFFSET		(0)
#define ASV_INT_GRP_2_OFFSET		(4)
#define ASV_INT_GRP_3_OFFSET		(8)
#define ASV_INT_VOL_LOCK_OFFSET		(12)
#define ASV_ISP_GRP_1_OFFSET		(16)
#define ASV_ISP_GRP_2_OFFSET		(20)
#define ASV_ISP_GRP_3_OFFSET		(24)
#define ASV_ISP_VOL_LOCK_OFFSET		(28)

#define ASV_SPEED_GRP_MASK		(0xF)
#define ASV_VOLT_LOCK_MASK		(0xF)

#define ASV_TBL_VER_EMA_REG		(S5P_VA_CHIPID2 + 0x20)
#define ASV_TBL_VER_OFFSET		(0)
#define ASV_TBL_VER_MASK		(0x3)
#define ASV_G3D_OPTION_OFFSET		(8)
#define ASV_G3D_OPTION_MASK		(0x1)
#define ASV_G3D_OFF_CORE_N_OFFSET	(9)
#define ASV_G3D_OFF_CORE_N_MASK		(0x7)

#define EXYNOS5430_GRP_MAX_NR		(3)

#define EXYNOS5430_GRP_L1               (0)
#define EXYNOS5430_GRP_L2               (1)
#define EXYNOS5430_GRP_L3               (2)

#define EXYNOS5430_VOL_NO_LOCK		(0x0)
#define EXYNOS5430_VOL_775_LOCK		(0x1)
#define EXYNOS5430_VOL_800_LOCK		(0x2)
#define EXYNOS5430_VOL_825_LOCK		(0x3)
#define EXYNOS5430_VOL_850_LOCK		(0x4)
#define EXYNOS5430_VOL_875_LOCK		(0x5)
#define EXYNOS5430_VOL_900_LOCK		(0x6)
#define EXYNOS5430_VOL_925_LOCK		(0x7)

#define VOL_775000			775000
#define VOL_800000			800000
#define VOL_825000			825000
#define VOL_850000			850000
#define VOL_875000			875000
#define VOL_900000			900000
#define VOL_925000			925000

#define EGL_GRP_L1_FREQ			1700000
#define EGL_GRP_L2_FREQ			1200000
#define KFC_GRP_L1_FREQ			1200000
#define KFC_GRP_L2_FREQ			700000
#define G3D_GRP_L1_FREQ			500000
#define G3D_GRP_L2_FREQ			350000
#define MIF_GRP_L1_FREQ			825000
#define MIF_GRP_L2_FREQ			413000
#define INT_GRP_L1_FREQ			413000
#define INT_GRP_L2_FREQ			206000
#define ISP_GRP_L1_FREQ			333000
#define ISP_GRP_L2_FREQ			222000

#define ASV_VER_050			(0x0)
#define ASV_VER_100			(0x1)
#define ASV_VER_200			(0x2)
#define ASV_VER_300			(0x3)

#define EGL_SPD_OPTION_REG		(S5P_VA_CHIPID2 + 0x24)
#define EGL_SPD_OPTION_FLAG_OFFSET	(0)
#define EGL_SPD_OPTION_FLAG_MASK	(0x1)
#define EGL_SPD_SEL_OFFSET		(1)
#define EGL_SPD_SEL_MASK		(0x3)

struct asv_reference {
	unsigned int asv_version;
	bool is_speedgroup;
};
static struct asv_reference asv_ref_info = {0, false};

struct asv_fused_info {
	unsigned int speed_grp[EXYNOS5430_GRP_MAX_NR];
	unsigned int voltage_lock;
};

static unsigned int asv_mem_size;
static unsigned int egl_speed_option;
static unsigned int egl_speed_sel;

static struct asv_fused_info egl_fused_info;
static struct asv_fused_info kfc_fused_info;
static struct asv_fused_info g3d_fused_info;
static struct asv_fused_info mif_fused_info;
static struct asv_fused_info int_fused_info;
static struct asv_fused_info isp_fused_info;

#ifdef CONFIG_ASV_MARGIN_TEST
static int set_arm_volt = 0;
static int set_kfc_volt = 0;
static int set_int_volt = 0;
static int set_mif_volt = 0;
static int set_g3d_volt = 0;
static int set_isp_volt = 0;

static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

static int __init get_kfc_volt(char *str)
{
	get_option(&str, &set_kfc_volt);
	return 0;
}
early_param("kfc", get_kfc_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_isp_volt(char *str)
{
	get_option(&str, &set_isp_volt);
	return 0;
}
early_param("isp", get_isp_volt);
#endif

unsigned int exynos5430_get_memory_size(void)
{
	switch (asv_mem_size) {
	case 0:
		return 2;
	case 1:
		return 3;
	}

	return 0;
}

void exynos5430_get_egl_speed_option(unsigned int *opt_flag, unsigned int *spd_sel)
{
	*opt_flag = egl_speed_option;
	*spd_sel = egl_speed_sel;
}

static unsigned int exynos5430_lock_voltage(unsigned int volt_lock)
{
	unsigned int lock_voltage;

	if (volt_lock == EXYNOS5430_VOL_775_LOCK)
		lock_voltage = VOL_775000;
	else if (volt_lock == EXYNOS5430_VOL_800_LOCK)
		lock_voltage = VOL_800000;
	else if (volt_lock == EXYNOS5430_VOL_825_LOCK)
		lock_voltage = VOL_825000;
	else if (volt_lock == EXYNOS5430_VOL_850_LOCK)
		lock_voltage = VOL_850000;
	else if (volt_lock == EXYNOS5430_VOL_875_LOCK)
		lock_voltage = VOL_875000;
	else if (volt_lock == EXYNOS5430_VOL_900_LOCK)
		lock_voltage = VOL_900000;
	else if (volt_lock == EXYNOS5430_VOL_925_LOCK)
		lock_voltage = VOL_925000;
	else
		lock_voltage = 0;

	return lock_voltage;
}

static unsigned int exynos5430_get_asv_group_arm(struct asv_common *asv_comm)
{
	if (asv_ref_info.is_speedgroup)
		return egl_fused_info.speed_grp[EXYNOS5430_GRP_L1];

	return 0;
}

static void exynos5430_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (asv_ref_info.asv_version == ASV_VER_050) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= EGL_GRP_L1_FREQ)
					target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= EGL_GRP_L2_FREQ)
					target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(egl_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= arm_asv_volt_info[i][target_asv_grp_nr + 1])
					arm_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				arm_asv_volt_info[i][target_asv_grp_nr + 1] + set_arm_volt;
#else
			asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		}
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info_v01[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= EGL_GRP_L1_FREQ)
					target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= EGL_GRP_L2_FREQ)
					target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(egl_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= arm_asv_volt_info_v01[i][target_asv_grp_nr + 1])
					arm_asv_volt_info_v01[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				arm_asv_volt_info_v01[i][target_asv_grp_nr + 1] + set_arm_volt;
#else
			asv_inform->asv_volt[i].asv_value = arm_asv_volt_info_v01[i][target_asv_grp_nr + 1];
#endif
		}
	} else {
		pr_err("%s: cannot support ASV verison (0x%x)\n",
				__func__, asv_ref_info.asv_version);
		return;
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_arm = {
	.get_asv_group	= exynos5430_get_asv_group_arm,
	.set_asv_info	= exynos5430_set_asv_info_arm,
};

static unsigned int exynos5430_get_asv_group_kfc(struct asv_common *asv_comm)
{
	if (asv_ref_info.is_speedgroup)
		return kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1];

	return 0;
}

static void exynos5430_set_asv_info_kfc(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (asv_ref_info.asv_version == ASV_VER_050) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = kfc_asv_volt_info[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= KFC_GRP_L1_FREQ)
					target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= KFC_GRP_L2_FREQ)
					target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(kfc_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= kfc_asv_volt_info[i][target_asv_grp_nr + 1])
					kfc_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				kfc_asv_volt_info[i][target_asv_grp_nr + 1] + set_kfc_volt;
#else
			asv_inform->asv_volt[i].asv_value = kfc_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		}
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = kfc_asv_volt_info_v01[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= KFC_GRP_L1_FREQ)
					target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= KFC_GRP_L2_FREQ)
					target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(kfc_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= kfc_asv_volt_info_v01[i][target_asv_grp_nr + 1])
					kfc_asv_volt_info_v01[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				kfc_asv_volt_info_v01[i][target_asv_grp_nr + 1] + set_kfc_volt;
#else
			asv_inform->asv_volt[i].asv_value = kfc_asv_volt_info_v01[i][target_asv_grp_nr + 1];
#endif
		}
	} else {
		pr_err("%s: cannot support ASV verison (0x%x)\n",
				__func__, asv_ref_info.asv_version);
		return;
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_kfc = {
	.get_asv_group	= exynos5430_get_asv_group_kfc,
	.set_asv_info	= exynos5430_set_asv_info_kfc,
};

static unsigned int exynos5430_get_asv_group_int(struct asv_common *asv_comm)
{
	if (asv_ref_info.is_speedgroup)
		return int_fused_info.speed_grp[EXYNOS5430_GRP_L1];

	return 0;
}

static void exynos5430_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (asv_ref_info.asv_version == ASV_VER_050) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = int_asv_volt_info[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= INT_GRP_L1_FREQ)
					target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= INT_GRP_L2_FREQ)
					target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(int_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= int_asv_volt_info[i][target_asv_grp_nr + 1])
					int_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				int_asv_volt_info[i][target_asv_grp_nr + 1] + set_int_volt;
#else
			asv_inform->asv_volt[i].asv_value = int_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		}
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = int_asv_volt_info_v01[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= INT_GRP_L1_FREQ)
					target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= INT_GRP_L2_FREQ)
					target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(int_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= int_asv_volt_info_v01[i][target_asv_grp_nr + 1])
					int_asv_volt_info_v01[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				int_asv_volt_info_v01[i][target_asv_grp_nr + 1] + set_int_volt;
#else
			asv_inform->asv_volt[i].asv_value = int_asv_volt_info_v01[i][target_asv_grp_nr + 1];
#endif
		}
	} else {
		pr_err("%s: cannot support ASV verison (0x%x)\n",
				__func__, asv_ref_info.asv_version);
		return;
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_int = {
	.get_asv_group	= exynos5430_get_asv_group_int,
	.set_asv_info	= exynos5430_set_asv_info_int,
};

static unsigned int exynos5430_get_asv_group_mif(struct asv_common *asv_comm)
{
	if (asv_ref_info.is_speedgroup)
		return mif_fused_info.speed_grp[EXYNOS5430_GRP_L1];

	return 0;
}

static void exynos5430_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (asv_ref_info.asv_version == ASV_VER_050) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= MIF_GRP_L1_FREQ)
					target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= MIF_GRP_L2_FREQ)
					target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(mif_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= mif_asv_volt_info[i][target_asv_grp_nr + 1])
					mif_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				mif_asv_volt_info[i][target_asv_grp_nr + 1] + set_mif_volt;
#else
			asv_inform->asv_volt[i].asv_value = mif_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		}
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info_v01[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= MIF_GRP_L1_FREQ)
					target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= MIF_GRP_L2_FREQ)
					target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(mif_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= mif_asv_volt_info_v01[i][target_asv_grp_nr + 1])
					mif_asv_volt_info_v01[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				mif_asv_volt_info_v01[i][target_asv_grp_nr + 1] + set_mif_volt;
#else
			asv_inform->asv_volt[i].asv_value = mif_asv_volt_info_v01[i][target_asv_grp_nr + 1];
#endif
		}
	} else {
		pr_err("%s: cannot support ASV verison (0x%x)\n",
				__func__, asv_ref_info.asv_version);
		return;
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_mif = {
	.get_asv_group	= exynos5430_get_asv_group_mif,
	.set_asv_info	= exynos5430_set_asv_info_mif,
};

static unsigned int exynos5430_get_asv_group_g3d(struct asv_common *asv_comm)
{
	if (asv_ref_info.is_speedgroup)
		return g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1];

	return 0;
}

static void exynos5430_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (asv_ref_info.asv_version == ASV_VER_050) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= G3D_GRP_L1_FREQ)
					target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= G3D_GRP_L2_FREQ)
					target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(g3d_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= g3d_asv_volt_info[i][target_asv_grp_nr + 1])
					g3d_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				g3d_asv_volt_info[i][target_asv_grp_nr + 1] + set_g3d_volt;
#else
			asv_inform->asv_volt[i].asv_value = g3d_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		}
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info_v01[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= G3D_GRP_L1_FREQ)
					target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= G3D_GRP_L2_FREQ)
					target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(g3d_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= g3d_asv_volt_info_v01[i][target_asv_grp_nr + 1])
					g3d_asv_volt_info_v01[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				g3d_asv_volt_info_v01[i][target_asv_grp_nr + 1] + set_g3d_volt;
#else
			asv_inform->asv_volt[i].asv_value = g3d_asv_volt_info_v01[i][target_asv_grp_nr + 1];
#endif
		}
	} else {
		pr_err("%s: cannot support ASV verison (0x%x)\n",
				__func__, asv_ref_info.asv_version);
		return;
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_g3d = {
	.get_asv_group	= exynos5430_get_asv_group_g3d,
	.set_asv_info	= exynos5430_set_asv_info_g3d,
};

static unsigned int exynos5430_get_asv_group_isp(struct asv_common *asv_comm)
{
	if (asv_ref_info.is_speedgroup)
		return isp_fused_info.speed_grp[EXYNOS5430_GRP_L1];

	return 0;
}

static void exynos5430_set_asv_info_isp(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (asv_ref_info.asv_version == ASV_VER_050) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = isp_asv_volt_info[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= ISP_GRP_L1_FREQ)
					target_asv_grp_nr = isp_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= ISP_GRP_L2_FREQ)
					target_asv_grp_nr = isp_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = isp_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(isp_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= isp_asv_volt_info[i][target_asv_grp_nr + 1])
					isp_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				isp_asv_volt_info[i][target_asv_grp_nr + 1] + set_isp_volt;
#else
			asv_inform->asv_volt[i].asv_value = isp_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		}
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			asv_inform->asv_volt[i].asv_freq = isp_asv_volt_info_v01[i][0];

			if (asv_ref_info.is_speedgroup) {
				if (asv_inform->asv_volt[i].asv_freq >= ISP_GRP_L1_FREQ)
					target_asv_grp_nr = isp_fused_info.speed_grp[EXYNOS5430_GRP_L1];
				else if (asv_inform->asv_volt[i].asv_freq >= ISP_GRP_L2_FREQ)
					target_asv_grp_nr = isp_fused_info.speed_grp[EXYNOS5430_GRP_L2];
				else
					target_asv_grp_nr = isp_fused_info.speed_grp[EXYNOS5430_GRP_L3];

				lock_voltage = exynos5430_lock_voltage(isp_fused_info.voltage_lock);
				if (lock_voltage &&
						lock_voltage >= isp_asv_volt_info_v01[i][target_asv_grp_nr + 1])
					isp_asv_volt_info_v01[i][target_asv_grp_nr + 1] = lock_voltage;
			}

#ifdef CONFIG_ASV_MARGIN_TEST
			asv_inform->asv_volt[i].asv_value =
				isp_asv_volt_info_v01[i][target_asv_grp_nr + 1] + set_isp_volt;
#else
			asv_inform->asv_volt[i].asv_value = isp_asv_volt_info_v01[i][target_asv_grp_nr + 1];
#endif
		}
	} else {
		pr_err("%s: cannot support ASV verison (0x%x)\n",
				__func__, asv_ref_info.asv_version);
		return;
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_isp = {
	.get_asv_group	= exynos5430_get_asv_group_isp,
	.set_asv_info	= exynos5430_set_asv_info_isp,
};

struct asv_info exynos5430_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5430_asv_ops_arm,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5430_asv_ops_kfc,
		.asv_group_nr	= ASV_GRP_NR(KFC),
		.dvfs_level_nr	= DVFS_LEVEL_NR(KFC),
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos5430_asv_ops_int,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos5430_asv_ops_mif,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos5430_asv_ops_g3d,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	}, {
		.asv_type	= ID_ISP,
		.name		= "VDD_ISP",
		.ops		= &exynos5430_asv_ops_isp,
		.asv_group_nr	= ASV_GRP_NR(ISP),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ISP),
		.max_volt_value = MAX_VOLT(ISP),
	},
};

unsigned int exynos5430_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos5430_asv_member); i++)
		add_asv_member(&exynos5430_asv_member[i]);

	return 0;
}

int exynos5430_init_asv(struct asv_common *asv_info)
{
#if defined(CONFIG_SOC_EXYNOS5430_REV_0)
	pr_err("EXYNOS5430 ASV : cannot support Rev0\n");
	return -EINVAL;
#else
	unsigned int arm_speed_grp, kfc_speed_grp;
	unsigned int g3d_mif_speed_grp, int_isp_speed_grp;
	unsigned int asv_tbl_ver_ema;
	unsigned int egl_speed_option_reg;

	asv_ref_info.is_speedgroup = true;

	asv_mem_size = (readl(S5P_VA_CHIPID + 0x0004) >> ASV_MEM_SIZE_SHIFT) & ASV_MEM_SIZE_MASK;

	pr_info("EXYNOS5430 ASV : LPDDR3 DRAM size %uGB\n", asv_mem_size + 2);

	arm_speed_grp = readl(ASV_ARM_SPEED_GRP_REG);
	kfc_speed_grp = readl(ASV_KFC_SPEED_GRP_REG);
	g3d_mif_speed_grp = readl(ASV_G3D_MIF_SPEED_GRP_REG);
	int_isp_speed_grp = readl(ASV_INT_ISP_SPEED_GRP_REG);
	asv_tbl_ver_ema = readl(ASV_TBL_VER_EMA_REG);
	egl_speed_option_reg = readl(EGL_SPD_OPTION_REG);

	egl_speed_option =
		(egl_speed_option_reg >> EGL_SPD_OPTION_FLAG_OFFSET) & EGL_SPD_OPTION_FLAG_MASK;
	egl_speed_sel =
		(egl_speed_option_reg >> EGL_SPD_SEL_OFFSET) & EGL_SPD_SEL_MASK;

	pr_info("EXYNOS5430 ASV : EGL Speed Option (%u), EGL Speed Select (0x%x)\n",
			egl_speed_option, egl_speed_sel);

	asv_ref_info.asv_version = (asv_tbl_ver_ema >> ASV_TBL_VER_OFFSET) & ASV_TBL_VER_MASK;

	pr_info("EXYNOS5430 ASV : ASV version (0x%x)\n", asv_ref_info.asv_version);

	if (asv_ref_info.asv_version == ASV_VER_050) {
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(arm_speed_grp >> ASV_ARM_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L2] = egl_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L3] = egl_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		egl_fused_info.voltage_lock =
			(arm_speed_grp >> ASV_ARM_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(kfc_speed_grp >> ASV_KFC_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L2] = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L3] = kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		kfc_fused_info.voltage_lock =
			(kfc_speed_grp >> ASV_KFC_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(g3d_mif_speed_grp >> ASV_G3D_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L2] = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L3] = g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		g3d_fused_info.voltage_lock =
			(g3d_mif_speed_grp >> ASV_G3D_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		mif_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(g3d_mif_speed_grp >> ASV_MIF_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		mif_fused_info.speed_grp[EXYNOS5430_GRP_L2] = mif_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		mif_fused_info.speed_grp[EXYNOS5430_GRP_L3] = mif_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		mif_fused_info.voltage_lock =
			(g3d_mif_speed_grp >> ASV_MIF_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		int_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(int_isp_speed_grp >> ASV_INT_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		int_fused_info.speed_grp[EXYNOS5430_GRP_L2] = int_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		int_fused_info.speed_grp[EXYNOS5430_GRP_L3] = int_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		int_fused_info.voltage_lock =
			(int_isp_speed_grp >> ASV_INT_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		/* if ASV version is 0, isp ASV group same with int ASV group */
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L1] = int_fused_info.speed_grp[EXYNOS5430_GRP_L1];
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L2] = int_fused_info.speed_grp[EXYNOS5430_GRP_L2];
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L3] = int_fused_info.speed_grp[EXYNOS5430_GRP_L3];
		isp_fused_info.voltage_lock = int_fused_info.voltage_lock;
	} else if (asv_ref_info.asv_version == ASV_VER_100) {
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(arm_speed_grp >> ASV_ARM_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L2] =
			(arm_speed_grp >> ASV_ARM_GRP_2_OFFSET) & ASV_SPEED_GRP_MASK;
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L3] =
			(arm_speed_grp >> ASV_ARM_GRP_3_OFFSET) & ASV_SPEED_GRP_MASK;
		egl_fused_info.voltage_lock =
			(arm_speed_grp >> ASV_ARM_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(kfc_speed_grp >> ASV_KFC_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L2] =
			(kfc_speed_grp >> ASV_KFC_GRP_2_OFFSET) & ASV_SPEED_GRP_MASK;
		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L3] =
			(kfc_speed_grp >> ASV_KFC_GRP_3_OFFSET) & ASV_SPEED_GRP_MASK;
		kfc_fused_info.voltage_lock =
			(kfc_speed_grp >> ASV_KFC_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(g3d_mif_speed_grp >> ASV_G3D_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L2] =
			(g3d_mif_speed_grp >> ASV_G3D_GRP_2_OFFSET) & ASV_SPEED_GRP_MASK;
		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L3] =
			(g3d_mif_speed_grp >> ASV_G3D_GRP_3_OFFSET) & ASV_SPEED_GRP_MASK;
		g3d_fused_info.voltage_lock =
			(g3d_mif_speed_grp >> ASV_G3D_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		mif_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(g3d_mif_speed_grp >> ASV_MIF_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		mif_fused_info.speed_grp[EXYNOS5430_GRP_L2] =
			(g3d_mif_speed_grp >> ASV_MIF_GRP_2_OFFSET) & ASV_SPEED_GRP_MASK;
		mif_fused_info.speed_grp[EXYNOS5430_GRP_L3] =
			(g3d_mif_speed_grp >> ASV_MIF_GRP_3_OFFSET) & ASV_SPEED_GRP_MASK;
		mif_fused_info.voltage_lock =
			(g3d_mif_speed_grp >> ASV_MIF_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		int_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(int_isp_speed_grp >> ASV_INT_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		int_fused_info.speed_grp[EXYNOS5430_GRP_L2] =
			(int_isp_speed_grp >> ASV_INT_GRP_2_OFFSET) & ASV_SPEED_GRP_MASK;
		int_fused_info.speed_grp[EXYNOS5430_GRP_L3] =
			(int_isp_speed_grp >> ASV_INT_GRP_3_OFFSET) & ASV_SPEED_GRP_MASK;
		int_fused_info.voltage_lock =
			(int_isp_speed_grp >> ASV_INT_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;

		isp_fused_info.speed_grp[EXYNOS5430_GRP_L1] =
			(int_isp_speed_grp >> ASV_ISP_GRP_1_OFFSET) & ASV_SPEED_GRP_MASK;
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L2] =
			(int_isp_speed_grp >> ASV_ISP_GRP_2_OFFSET) & ASV_SPEED_GRP_MASK;
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L3] =
			(int_isp_speed_grp >> ASV_ISP_GRP_3_OFFSET) & ASV_SPEED_GRP_MASK;
		isp_fused_info.voltage_lock =
			(int_isp_speed_grp >> ASV_ISP_VOL_LOCK_OFFSET) & ASV_VOLT_LOCK_MASK;
	} else {
		pr_err("EXYNOS5430 ASV : cannot support ASV version (0x%x)\n",
					asv_ref_info.asv_version);
	}

	pr_info("EXYNOS5430 ASV : EGL Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L1], egl_fused_info.speed_grp[EXYNOS5430_GRP_L2],
		egl_fused_info.speed_grp[EXYNOS5430_GRP_L3], egl_fused_info.voltage_lock);

	pr_info("EXYNOS5430 ASV : KFC Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L1], kfc_fused_info.speed_grp[EXYNOS5430_GRP_L2],
		kfc_fused_info.speed_grp[EXYNOS5430_GRP_L3], kfc_fused_info.voltage_lock);

	pr_info("EXYNOS5430 ASV : G3D Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L1], g3d_fused_info.speed_grp[EXYNOS5430_GRP_L2],
		g3d_fused_info.speed_grp[EXYNOS5430_GRP_L3], g3d_fused_info.voltage_lock);

	pr_info("EXYNOS5430 ASV : MIF Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
		mif_fused_info.speed_grp[EXYNOS5430_GRP_L1], mif_fused_info.speed_grp[EXYNOS5430_GRP_L2],
		mif_fused_info.speed_grp[EXYNOS5430_GRP_L3], mif_fused_info.voltage_lock);

	pr_info("EXYNOS5430 ASV : INT Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
		int_fused_info.speed_grp[EXYNOS5430_GRP_L1], int_fused_info.speed_grp[EXYNOS5430_GRP_L2],
		int_fused_info.speed_grp[EXYNOS5430_GRP_L3], int_fused_info.voltage_lock);

	pr_info("EXYNOS5430 ASV : ISP Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L1], isp_fused_info.speed_grp[EXYNOS5430_GRP_L2],
		isp_fused_info.speed_grp[EXYNOS5430_GRP_L3], isp_fused_info.voltage_lock);

	asv_info->regist_asv_member = exynos5430_regist_asv_member;

	return 0;
#endif
}
