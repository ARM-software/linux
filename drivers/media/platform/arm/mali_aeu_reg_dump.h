// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#ifndef _MALI_AEU_REG_DUMP_H_
#define _MALI_AEU_REG_DUMP_H_

/* current registers */
#define REG_FILE_CURR	0
/* just before compressing is enabled */
#define REG_FILE_BEF	1
/* the coppressing is done, or error is detected */
#define REG_FILE_AFT	2

struct mali_aeu_reg_file;

#ifdef CONFIG_VIDEO_ADV_DEBUG
struct mali_aeu_reg_file *mali_aeu_init_reg_file(void __iomem *b);
void mali_aeu_free_reg_file(struct mali_aeu_reg_file *reg_f);
int mali_aeu_reg_dump(struct mali_aeu_reg_file *reg_f, u32 table);
int mali_aeu_g_reg(struct mali_aeu_reg_file *reg_f, u32 table, u32 idx, u32 *v);
#else
static inline struct mali_aeu_reg_file *mali_aeu_init_reg_file(void __iomem *b)
{
	return NULL;
}

static inline void mali_aeu_free_reg_file(struct mali_aeu_reg_file *reg_f)
{
}

static inline int mali_aeu_reg_dump(struct mali_aeu_reg_file *reg_f, u32 table)
{
	return -EINVAL;
}

static inline
int mali_aeu_g_reg(struct mali_aeu_reg_file *reg_f, u32 table, u32 idx, u32 *v)
{
	return -EINVAL;
}
#endif /* CONFIG_VIDEO_ADV_DEBUG */

#endif /* !_MALI_AEU_REG_DUMP_H_ */
