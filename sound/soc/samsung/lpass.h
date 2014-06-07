/* sound/soc/samsung/lpass.h
 *
 * ALSA SoC Audio Layer - Samsung Audio Subsystem driver
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SAMSUNG_LPASS_H
#define __SND_SOC_SAMSUNG_LPASS_H

/* SFR */
#define LPASS_VERSION		(0x00)
#define LPASS_CA5_SW_RESET	(0x04)
#define LPASS_CORE_SW_RESET	(0x08)
#define LPASS_MIF_POWER		(0x10)
#define LPASS_CA5_BOOTADDR	(0x20)
#define LPASS_CA5_DBG		(0x30)
#define LPASS_SW_INTR_CA5	(0x40)
#define LPASS_INTR_CA5_STATUS	(0x44)
#define LPASS_INTR_CA5_MASK	(0x48)
#define LPASS_SW_INTR_CPU	(0x50)
#define LPASS_INTR_CPU_STATUS	(0x54)
#define LPASS_INTR_CPU_MASK	(0x58)

/* SW_RESET */
#define LPASS_SW_RESET_CA5	(1 << 0)
#define LPASS_SW_RESET_SB	(1 << 11)
#define LPASS_SW_RESET_UART	(1 << 10)
#define LPASS_SW_RESET_PCM	(1 << 9)
#define LPASS_SW_RESET_I2S	(1 << 8)
#define LPASS_SW_RESET_TIMER	(1 << 2)
#define LPASS_SW_RESET_MEM	(1 << 1)
#define LPASS_SW_RESET_DMA	(1 << 0)

/* Interrupt mask */
#define LPASS_INTR_APM		(1 << 9)
#define LPASS_INTR_MIF		(1 << 8)
#define LPASS_INTR_TIMER	(1 << 7)
#define LPASS_INTR_DMA		(1 << 6)
#define LPASS_INTR_GPIO		(1 << 5)
#define LPASS_INTR_I2S		(1 << 4)
#define LPASS_INTR_PCM		(1 << 3)
#define LPASS_INTR_SB		(1 << 2)
#define LPASS_INTR_UART		(1 << 1)
#define LPASS_INTR_SFR		(1 << 0)

extern void __iomem *lpass_get_regs(void);
extern void __iomem *lpass_get_mem(void);

extern struct clk *lpass_get_i2s_opclk(int clk_id);
extern void lpass_reg_dump(void);

#endif /* __SND_SOC_SAMSUNG_LPASS_H */
