/* drivers/video/decon_display/tv/regs-decon_tv.h
 *
 * Copyright 2013 Samsung Electronics
 *     Haowei Li <haowei.li@samsung.com>
 *
 * Display and Enhancement Controller Television(DECON_TV) register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_DECON_TV_H
#define _REGS_DECON_TV_H

/* VIDCON0 */
#define VIDCON0					(0x00)

#define VIDCON0_SWRESET				(1 << 28)
#define VIDCON0_CLKVALUP			(1 << 14)
#define VIDCON0_CLKVAL_F(_x)			((_x) << 6)
#define VIDCON0_CLKVAL_F_MASK			(0xff << 6)
#define VIDCON0_VLCKFREE			(1 << 5)
#define VIDCON0_DECON_STOP_STATUS		(1 << 2)
#define VIDCON0_ENVID				(1 << 1)
#define VIDCON0_ENVID_F				(1 << 0)

/* VIDOUTCON0 */
#define VIDOUTCON0				(0x10)

#define VIDOUTCON0_INTERLACE_EN_F		(0x1 << 28)
#define VIDOUTCON0_PROGRESSIVE_EN_F		(0x0 << 28)
#define VIDOUTCON0_MODE_MASK			(0x1 << 28)
#define VIDOUTCON0_LCD_F			(0x1 << 24)
#define VIDOUTCON0_LCD_F_SHIFT			(24)
#define VIDOUTCON0_IF_SHIFT			(20)
#define VIDOUTCON0_IF_MASK			(0x3 << 20)
#define VIDOUTCON0_RGBIF_F			(0x0 << 20)
#define VIDOUTCON0_I80IF_F			(0x2 << 20)
#define VIDOUTCON0_WB_F				(0x1 << 16)

/* WINCONx */
#define WINCON(_win)				(0x20 + ((_win) * 4))

#define WINCONx_BUFSTATUS			(0x3 << 30)
#define WINCONx_BUFSEL_MASK			(0x3 << 28)
#define WINCONx_BUFSEL_SHIFT			(28)
#define WINCONx_LOCALSEL_0_F			(0x0 << 21)
#define WINCONx_LOCALSEL_1_F			(0x1 << 21)
#define WINCONx_LOCALSEL_2_F			(0x2 << 21)
#define WINCONx_ENLOCAL_F			(0x1 << 20)
#define WINCONx_BUFAUTOEN			(0x1 << 19)
#define WINCONx_BITSWP				(0x1 << 18)
#define WINCONx_BYTSWP				(0x1 << 17)
#define WINCONx_HAWSWP				(0x1 << 16)
#define WINCONx_WSWP				(0x1 << 15)
#define WINCONx_TRIPLE_BUF_MODE			(0x1 << 13)
#define WINCONx_DOUBLE_BUF_MODE			(0x0 << 13)
#define WINCONx_BURSTLEN_16WORD			(0x0 << 10)
#define WINCONx_BURSTLEN_8WORD			(0x1 << 10)
#define WINCONx_BURSTLEN_4WORD			(0x2 << 10)
#define WINCONx_BURSTLEN_MASK			(0x3 << 10)
#define WINCONx_BURSTLEN_SHIFT			(10)
#define WINCONx_BPPORDER_ARGB_F			(0 << 9)
#define WINCONx_BPPORDER_RGBA_F			(1 << 9)
#define WINCONx_BPPORDER_RGB_F			(0 << 8)
#define WINCONx_BPPORDER_BGR_F			(1 << 8)
#define WINCONx_BLD_PLANE			(0 << 6)
#define WINCONx_BLD_PIX				(1 << 6)
#define WINCONx_ALPHA_MUL			(1 << 7)

#define WINCONx_BPPMODE_MASK			(0xf << 2)
#define WINCONx_BPPMODE_SHIFT			(2)
#define WINCONx_BPPMODE_8BPP_1232		(0x4 << 2)
#define WINCONx_BPPMODE_16BPP_565		(0x5 << 2)
#define WINCONx_BPPMODE_16BPP_A1555		(0x6 << 2)
#define WINCONx_BPPMODE_16BPP_I1555		(0x7 << 2)
#define WINCONx_BPPMODE_18BPP_666		(0x8 << 2)
#define WINCONx_BPPMODE_18BPP_A1665		(0x9 << 2)
#define WINCONx_BPPMODE_19BPP_A1666		(0xa << 2)
#define WINCONx_BPPMODE_24BPP_888		(0xb << 2)
#define WINCONx_BPPMODE_24BPP_A1887		(0xc << 2)
#define WINCONx_BPPMODE_25BPP_A1888		(0xd << 2)
#define WINCONx_BPPMODE_32BPP_A8888		(0xd << 2)
#define WINCONx_BPPMODE_13BPP_A1444		(0xe << 2)
#define WINCONx_BPPMODE_16BPP_A4444		(0xe << 2)
#define WINCONx_BPPMODE_15BPP_A4444		(0xf << 2)
#define WINCONx_ALPHA_SEL			(1 << 1)
#define WINCONx_ENWIN				(1 << 0)

#define WINCONx_BPPORDER_C_F_RGB	(1 << 8)
#define WINCONx_BPPORDER_C_F_BGR	(0 << 8)

#define WINCON1_ALPHA_MUL_F			(1 << 7)
#define WINCON2_ALPHA_MUL_F			(1 << 7)
#define WINCON3_ALPHA_MUL_F			(1 << 7)
#define WINCON4_ALPHA_MUL_F			(1 << 7)

#define WINCON3_DWSWP				(1 << 14)

/*  VIDOSDxH: The height for the OSD image(READ ONLY)*/
#define VIDOSD_H(_x)				(0x80 + ((_x) * 4))

/* SHADOWCON */
#define SHADOWCON				(0xA0)

#define SHADOWCON_WINx_PROTECT(_win)		(1 << (10 + (_win)))
#define SHADOWCON_STANDALONE_UPDATE_ALWAYS	(1 << 0)
#define SHADOWCON_CHx_ENABLE(_win)		(1 << (_win))

/* VIDOSDxA ~ VIDOSDxE */
#define VIDOSD_BASE				(0xB0)

#define OSD_STRIDE				(0x20)

#define VIDOSD_A(_win)				(VIDOSD_BASE + ((_win) * OSD_STRIDE) + 0x00)
#define VIDOSD_B(_win)				(VIDOSD_BASE + ((_win) * OSD_STRIDE) + 0x04)
#define VIDOSD_C(_win)				(VIDOSD_BASE + ((_win) * OSD_STRIDE) + 0x08)
#define VIDOSD_D(_win)				(VIDOSD_BASE + ((_win) * OSD_STRIDE) + 0x0C)
#define VIDOSD_E(_win)				(VIDOSD_BASE + ((_win) * OSD_STRIDE) + 0x10)

#define VIDOSDxA_TOPLEFT_X_MASK			(0xfff << 12)
#define VIDOSDxA_TOPLEFT_X_SHIFT		(12)
#define VIDOSDxA_TOPLEFT_X_LIMIT		(0xfff)
#define VIDOSDxA_TOPLEFT_X(_x)			(((_x) & 0xfff) << 12)

#define VIDOSDxA_TOPLEFT_Y_MASK			(0xfff << 0)
#define VIDOSDxA_TOPLEFT_Y_SHIFT		(0)
#define VIDOSDxA_TOPLEFT_Y_LIMIT		(0xfff)
#define VIDOSDxA_TOPLEFT_Y(_x)			(((_x) & 0xfff) << 0)

#define VIDOSDxB_BOTRIGHT_X_MASK		(0xfff << 12)
#define VIDOSDxB_BOTRIGHT_X_SHIFT		(12)
#define VIDOSDxB_BOTRIGHT_X_LIMIT		(0xfff)
#define VIDOSDxB_BOTRIGHT_X(_x)			(((_x) & 0xfff) << 12)

#define VIDOSDxB_BOTRIGHT_Y_MASK		(0xfff << 0)
#define VIDOSDxB_BOTRIGHT_Y_SHIFT		(0)
#define VIDOSDxB_BOTRIGHT_Y_LIMIT		(0xfff)
#define VIDOSDxB_BOTRIGHT_Y(_x)			(((_x) & 0xfff) << 0)

#define VIDOSDxC_ALPHA0_R_F(_x)			(((_x) & 0xFF) << 16)
#define VIDOSDxC_ALPHA0_G_F(_x)			(((_x) & 0xFF) << 8)
#define VIDOSDxC_ALPHA0_B_F(_x)			(((_x) & 0xFF) << 0)

#define VIDOSDxD_ALPHA1_R_F(_x)			(((_x) & 0xFF) << 16)
#define VIDOSDxD_ALPHA1_G_F(_x)			(((_x) & 0xFF) << 8)
#define VIDOSDxD_ALPHA1_B_F(_x)			(((_x) & 0xFF) >> 0)

/* Frame buffer start addresses: VIDWxxADD0n */
#define VIDW_BUF_START(_win)			(0x150 + ((_win) * 0x10))
#define VIDW_BUF_START1(_win)			(0x154 + ((_win) * 0x10))
#define VIDW_BUF_START2(_win)			(0x158 + ((_win) * 0x10))

/* Frame buffer end addresses: VIDWxxADD1n */
#define VIDW_BUF_END(_win)			(0x1A0 + ((_win) * 0x10))
#define VIDW_BUF_END1(_win)			(0x1A4 + ((_win) * 0x10))
#define VIDW_BUF_END2(_win)			(0x1A4 + ((_win) * 0x10))

/* VIDWxxADD2 */
#define VIDW_BUF_SIZE(_win)			(0x200 + ((_win) * 0x4))

#define VIDW_BUF_SIZE_OFFSET_MASK		(0x3fff << 14)
#define VIDW_BUF_SIZE_OFFSET_SHIFT		(14)
#define VIDW_BUF_SIZE_OFFSET_LIMIT		(0x3fff)
#define VIDW_BUF_SIZE_OFFSET(_x)		(((_x) & 0x3fff) << 15)

#define VIDW_BUF_SIZE_PAGEWIDTH_MASK		(0x3fff << 0)
#define VIDW_BUF_SIZE_PAGEWIDTH_SHIFT		(0)
#define VIDW_BUF_SIZE_PAGEWIDTH_LIMIT		(0x3fff)
#define VIDW_BUF_SIZE_PAGEWIDTH(_x)		(((_x) & 0x3fff) << 0)

/* LOCAL PATH SIZE */
#define LOCAL_SIZE(_x)				(0x214 + (_x) * 0x4)

/* Interrupt control register */
#define VIDINTCON0				(0x220)

#define VIDINTCON0_WAKEUP_MASK			(0xf << 28)
#define VIDINTCON0_INTEXTRAEN			(1 << 21)
#define VIDINTCON0_INT_I80IFDONE		(1 << 17)

#define VIDINTCON0_FRAMESEL0_SHIFT		(15)
#define VIDINTCON0_FRAMESEL0_MASK		(0x3 << 15)
#define VIDINTCON0_FRAMESEL0_BACKPORCH		(0x0 << 15)
#define VIDINTCON0_FRAMESEL0_VSYNC		(0x1 << 15)
#define VIDINTCON0_FRAMESEL0_ACTIVE		(0x2 << 15)
#define VIDINTCON0_FRAMESEL0_FRONTPORCH		(0x3 << 15)

#define VIDINTCON0_INT_FRAME			(1 << 12)

#define VIDINTCON0_FIFOSEL_MAIN_EN		(1 << 5)
#define VIDINTCON0_FIFOLEVEL_MASK		(0x7 << 2)
#define VIDINTCON0_FIFOLEVEL_SHIFT		(2)
#define VIDINTCON0_FIFOLEVEL_EMPTY		(0x0 << 2)
#define VIDINTCON0_FIFOLEVEL_TO25PC		(0x1 << 2)
#define VIDINTCON0_FIFOLEVEL_TO50PC		(0x2 << 2)
#define VIDINTCON0_FIFOLEVEL_FULL		(0x4 << 2)

#define VIDINTCON0_INT_FIFO			(1 << 1)

#define VIDINTCON0_INT_ENABLE			(1 << 0)
#define VIDINTCON0_INT_I80_EN			(1 << 17)
#define VIDINTCON0_INT_FIFO_MASK		(0x3 << 0)
#define VIDINTCON0_INT_FIFO_SHIFT		(0)

#define VIDINTCON0_INT_MASK			(0x209023)

/* Interrupt controls and status register */
#define VIDINTCON1				(0x224)

#define VIDINTCON1_INT_EXTRA			(1 << 3)
#define VIDINTCON1_INT_I80			(1 << 2)
#define VIDINTCON1_INT_FRAME			(1 << 1)
#define VIDINTCON1_INT_FIFO			(1 << 0)

/* Interrupt controls register */
#define VIDINTCON2				(0x228)

#define VIDINTCON1_INTEXTRA1_EN			(1 << 1)
#define VIDINTCON1_INTEXTRA0_EN			(1 << 0)

/* Interrupt controls and status register */
#define VIDINTCON3				(0x22C)

#define VIDINTCON1_INTEXTRA1_PEND		(1 << 1)
#define VIDINTCON1_INTEXTRA0_PEND		(1 << 0)

/* Window colour-key control registers */
#define WKEYCON					(0x230)

#define WKEYCON0				(0x00)
#define WKEYCON1				(0x04)
#define WxKEYCON0_KEYBL_EN			(1 << 26)
#define WxKEYCON0_KEYEN_F			(1 << 25)
#define WxKEYCON0_DIRCON			(1 << 24)
#define WxKEYCON0_COMPKEY_MASK			(0xffffff << 0)
#define WxKEYCON0_COMPKEY_SHIFT			(0)
#define WxKEYCON0_COMPKEY_LIMIT			(0xffffff)
#define WxKEYCON0_COMPKEY(_x)			((_x) << 0)
#define WxKEYCON1_COLVAL_MASK			(0xffffff << 0)
#define WxKEYCON1_COLVAL_SHIFT			(0)
#define WxKEYCON1_COLVAL_LIMIT			(0xffffff)
#define WxKEYCON1_COLVAL(_x)			((_x) << 0)

/* Window KEY Alpha value */
#define WxKEYALPHA(_win)			(0x250 + (((_win) - 1) * 0x4))

#define Wx_KEYALPHA_R_F_SHIFT			(16)
#define Wx_KEYALPHA_G_F_SHIFT			(8)
#define Wx_KEYALPHA_B_F_SHIFT			(0)

/* Window MAP (Color map) */
#define WINxMAP(_win)				(0x270 + ((_win) * 4))

#define WINxMAP_MAP				(1 << 24)
#define WINxMAP_MAP_COLOUR_MASK			(0xffffff << 0)
#define WINxMAP_MAP_COLOUR_SHIFT		(0)
#define WINxMAP_MAP_COLOUR_LIMIT		(0xffffff)
#define WINxMAP_MAP_COLOUR(_x)			((_x) << 0)

/* QoSLUT07_00 */
#define QOSLUT07_00				(0x2C0)

/* QoSLUT15_08 */
#define QOSLUT15_08				(0x2C4)

/* QoSCtrl */
#define QOSCTRL					(0x2C8)

/* Blending equation */
#define BLENDEQ(_x)				(0x300 + (_x) * 4)
#define BLENDEQ_COEF_ZERO			0x0
#define BLENDEQ_COEF_ONE			0x1
#define BLENDEQ_COEF_ALPHA_A			0x2
#define BLENDEQ_COEF_ONE_MINUS_ALPHA_A		0x3
#define BLENDEQ_COEF_ALPHA_B			0x4
#define BLENDEQ_COEF_ONE_MINUS_ALPHA_B		0x5
#define BLENDEQ_COEF_ALPHA0			0x6
#define BLENDEQ_COEF_A				0xA
#define BLENDEQ_COEF_ONE_MINUS_A		0xB
#define BLENDEQ_COEF_B				0xC
#define BLENDEQ_COEF_ONE_MINUS_B		0xD
#define BLENDEQ_Q_FUNC(_x)			((_x) << 18)
#define BLENDEQ_Q_FUNC_MASK			BLENDEQ_Q_FUNC(0xF)
#define BLENDEQ_P_FUNC(_x)			((_x) << 12)
#define BLENDEQ_P_FUNC_MASK			BLENDEQ_P_FUNC(0xF)
#define BLENDEQ_B_FUNC(_x)			((_x) << 6)
#define BLENDEQ_B_FUNC_MASK			BLENDEQ_B_FUNC(0xF)
#define BLENDEQ_A_FUNC(_x)			((_x) << 0)
#define BLENDEQ_A_FUNC_MASK			BLENDEQ_A_FUNC(0xF)

/* Blending equation control */
#define BLENDCON				(0x310)
#define BLENDCON_NEW_MASK			(1 << 0)
#define BLENDCON_NEW_8BIT_ALPHA_VALUE		(1 << 0)
#define BLENDCON_NEW_4BIT_ALPHA_VALUE		(0 << 0)

/* STEREO Control */
#define W013DSTEREOCON				(0x320)
#define w233DSTEREOCON				(0x324)

/* Buffer start/end addresses(Shadow, R_ONLY) */
#define SHD_VIDW_BUF_START(_buff)		(0x400 + ((_buff) * 4))
#define SHD_VIDW_BUF_END(_buff)			(0x414 + ((_buff) * 4))

#define FRAMEFIFO_REG0				(0x500)
#define FRAMEFIFO_REG7				(0x51C)
#define FRAMEFIFO_REG8				(0x520)
#define FRAMEFIFO_STATUS			(0x524)

#define DECON_MODECON				(0x1400)

#define DECON_MODE_STAND_ALONE_F		(0 << 0)
#define DECON_MODE_HIERARCHY_F			(1 << 0)

#define DECON_CMU				(0x1404)

#define DECON_CMU_CLKGATE_MODE_SE_F_SHIFT	(2)
#define DECON_CMU_CLKGATE_MODE_SFR_F_SHIFT	(1)
#define DECON_CMU_CLKGATE_MODE_MEM_F_SHIFT	(0)
#define DECON_CMU_CLKGATE_MODE_SFR_F		(1 << 1)
#define DECON_CMU_CLKGATE_MODE_MEM_F		(1 << 0)
#define DECON_CMU_CLKGATE_MASK			(0x3)

#define DECON_UPDATE				(0x1410)

#define DECON_UPDATE_SLAVE_SYNC			(1 << 4)
#define DECON_UPDATE_STANDALONE_F		(1 << 0)
#define DECON_UPDATE_MASK			(0x11)

#define DECON_CRFMID				(0x1414)

#define DECON_RRFRMID				(0x1418)

/* VICON1 */
#define VIDCON1					(0x2000)

#define VIDCON1_LINECNT_MASK			(0xfff << 16)
#define VIDCON1_LINECNT_SHIFT			(16)
#define VIDCON1_LINECNT_GET(_v)			(((_v) >> 16) & 0xfff)
#define VIDCON1_VSTATUS_MASK			(0x3 << 13)
#define VIDCON1_VSTATUS_SHIFT			(13)
#define VIDCON1_VSTATUS_VSYNC			(0x0 << 13)
#define VIDCON1_VSTATUS_BACKPORCH		(0x1 << 13)
#define VIDCON1_VSTATUS_ACTIVE			(0x2 << 13)
#define VIDCON1_VSTATUS_FRONTPORCH		(0x3 << 13)
#define VIDCON1_VCLK_MASK			(0x3 << 9)
#define VIDCON1_VCLK_HOLD			(0x0 << 9)
#define VIDCON1_VCLK_RUN			(0x1 << 9)

#define VIDCON1_INV_VCLK			(1 << 7)
#define VIDCON1_INV_HSYNC			(1 << 6)
#define VIDCON1_INV_VSYNC			(1 << 5)
#define VIDCON1_INV_VDEN			(1 << 4)

/* VIDCON2 */

#define VIDCON2					(0x2004)

#define VIDCON2_EN601				(1 << 23)
#define VIDCON2_RGB_ORDER_O_MASK	(0x7 << 16)
#define VIDCON2_RGB_ORDER_O_RGB		(0x0 << 16)
#define VIDCON2_RGB_ORDER_O_GBR		(0x1 << 16)
#define VIDCON2_RGB_ORDER_O_BRG		(0x2 << 16)
#define VIDCON2_RGB_ORDER_O_BGR		(0x4 << 16)
#define VIDCON2_RGB_ORDER_O_RBG		(0x5 << 16)
#define VIDCON2_RGB_ORDER_O_GRB		(0x6 << 16)

/* VIDCON3 */
#define VIDCON3					(0x2008)

/* VIDCON4 */
#define VIDCON4					(0x200C)
#define VIDCON4_FIFOCNT_START_EN		(1 << 0)

/* VIDTCON0 */
#define VIDTCON0				(0x2020)

#define VIDTCON0_VBPD(_x)			((_x) << 20)
#define VIDTCON0_VFPD(_x)			((_x) << 12)
#define VIDTCON0_VSPW(_x)			((_x) << 0)

/* VIDTCON1 */
#define VIDTCON1				(0x2024)

#define VIDTCON1_HBPD(_x)			((_x) << 20)
#define VIDTCON1_HFPD(_x)			((_x) << 12)
#define VIDTCON1_HSPW(_x)			((_x) << 0)

/* VIDTCON2 */
#define VIDTCON2				(0x2028)

#define VIDTCON2_LINEVAL_MASK			(0xfff << 16)
#define VIDTCON2_LINEVAL_SHIFT			(16)
#define VIDTCON2_LINEVAL_LIMIT			(0xfff)
#define VIDTCON2_LINEVAL(_x)			(((_x) & 0xfff) << 16)

#define VIDTCON2_HOZVAL_MASK			(0xfff << 0)
#define VIDTCON2_HOZVAL_SHIFT			(0)
#define VIDTCON2_HOZVAL_LIMIT			(0xfff)
#define VIDTCON2_HOZVAL(_x)			(((_x) & 0xfff) << 0)

/* FRAME SIZE */
#define FRAME_SIZE				(0x2038)

/* LINECNT OP THRSHOLD*/
#define LINECNT_OP_THRESHOLD			(0x203C)

/* TRIGCON */
#define TRIGCON					(0x2040)

#define TRIGCON_TRIGEN_PER_I80_RGB_F		(1 << 31)
#define TRIGCON_TRIGEN_I80_RGB_F		(1 << 30)
#define TRIGCON_SWTRIGCMD_W4BUF			(1 << 26)
#define TRIGCON_TRIGMODE_W4BUF			(1 << 25)
#define TRIGCON_SWTRIGCMD_W3BUF			(1 << 21)
#define TRIGCON_TRIGMODE_W3BUF			(1 << 20)
#define TRIGCON_SWTRIGCMD_W2BUF			(1 << 16)
#define TRIGCON_TRIGMODE_W2BUF			(1 << 15)
#define TRIGCON_SWTRIGCMD_W1BUF			(1 << 11)
#define TRIGCON_TRIGMODE_W1BUF			(1 << 10)
#define TRIGCON_SWTRIGCMD_W0BUF			(1 << 6)
#define TRIGCON_TRIGMODE_W0BUF			(1 << 5)
#define TRIGCON_HWTRIGMASK_I80_RGB		(1 << 4)
#define TRIGCON_HWTRIGEN_I80_RGB		(1 << 3)
#define TRIGCON_HWTRIG_INV_I80_RGB		(1 << 2)
#define TRIGCON_SWTRIGCMD_I80_RGB		(1 << 1)
#define TRIGCON_SWTRIGEN_I80_RGB		(1 << 0)
#define TRIGCON_MASK				(0xC0000018)

#define CRCRDATA				(0x20B0)

#define CRCCTRL					(0x20B4)
#define CRCCTRL_CRCCLKEN			(0x1 << 2)
#define CRCCTRL_CRCSTART_F			(0x1 << 1)
#define CRCCTRL_CRCEN				(0x1 << 0)
#define CRCCTRL_MASK				(0x5)

/* ENHANCER_CTRL */
#define ENHANCER_CTRL				(0x2100)

#define DITHMODE_R_POS_MASK			(0x3 << 6)
#define DITHMODE_R_POS_SHIFT			(6)
#define DITHMODE_R_POS_8BIT			(0x0 << 6)
#define DITHMODE_R_POS_6BIT			(0x1 << 6)
#define DITHMODE_R_POS_5BIT			(0x2 << 6)

#define DITHMODE_G_POS_MASK			(0x3 << 4)
#define DITHMODE_G_POS_SHIFT			(4)
#define DITHMODE_G_POS_8BIT			(0x0 << 4)
#define DITHMODE_G_POS_6BIT			(0x1 << 4)
#define DITHMODE_G_POS_5BIT			(0x2 << 4)

#define DITHMODE_B_POS_MASK			(0x3 << 2)
#define DITHMODE_B_POS_SHIFT			(2)
#define DITHMODE_B_POS_8BIT			(0x0 << 2)
#define DITHMODE_B_POS_6BIT			(0x1 << 2)
#define DITHMODE_B_POS_5BIT			(0x2 << 2)

#define DITHMODE_DITH_OFF			(0 << 0)
#define DITHMODE_DITH_SIMPLE_ENGINE_ON		(0x1 << 0)
#define DITHMODE_DITH_ADVANCED_ENGINE_ON	(0x3 << 0)

#define WINCON_SHADOW(x)			(WINCON(x) + 0x4000)
#define DECON_UPDATE_SHADOW			(DECON_UPDATE + 0x4000)
#endif /* _REGS_DECON_TV_H */
