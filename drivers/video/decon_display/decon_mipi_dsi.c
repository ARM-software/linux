/* linux/drivers/video/decon_display/decon_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * Haowei Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>

#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/exynos5-mipiphy.h>

#include "decon_display_driver.h"
#include "decon_mipi_dsi_lowlevel.h"
#include "decon_mipi_dsi.h"
#include "regs-mipidsim.h"
#include "decon_dt.h"
#include "decon_pm.h"

#ifdef CONFIG_SOC_EXYNOS5430
#include "decon_fb.h"
#else
#include <mach/regs-pmu.h>
#include "fimd_fb.h"
#endif

static DEFINE_MUTEX(dsim_rd_wr_mutex);
static DECLARE_COMPLETION(dsim_wr_comp);
static DECLARE_COMPLETION(dsim_rd_comp);

#define MIPI_WR_TIMEOUT msecs_to_jiffies(250)
#define MIPI_RD_TIMEOUT msecs_to_jiffies(250)

static unsigned int dpll_table[15] = {
	100, 120, 170, 220, 270,
	320, 390, 450, 510, 560,
	640, 690, 770, 870, 950 };

unsigned int dphy_timing[][10] = {
	/* bps, clk_prepare, clk_zero, clk_post, clk_trail, hs_prepare, hs_zero, hs_trail, lpx */
	{1500, 13, 65, 17, 13, 16, 24, 16, 11, 18},
	{1490, 13, 65, 17, 13, 16, 24, 16, 11, 18},
	{1480, 13, 64, 17, 13, 16, 24, 16, 11, 18},
	{1470, 13, 64, 17, 13, 16, 24, 16, 11, 18},
	{1460, 13, 63, 17, 13, 16, 24, 16, 10, 18},
	{1450, 13, 63, 17, 13, 16, 23, 16, 10, 18},
	{1440, 13, 63, 17, 13, 15, 23, 16, 10, 18},
	{1430, 12, 62, 17, 13, 15, 23, 16, 10, 17},
	{1420, 12, 62, 17, 13, 15, 23, 16, 10, 17},
	{1410, 12, 61, 16, 13, 15, 23, 16, 10, 17},
	{1400, 12, 61, 16, 13, 15, 23, 16, 10, 17},
	{1390, 12, 60, 16, 12, 15, 22, 15, 10, 17},
	{1380, 12, 60, 16, 12, 15, 22, 15, 10, 17},
	{1370, 12, 59, 16, 12, 15, 22, 15, 10, 17},
	{1360, 12, 59, 16, 12, 15, 22, 15, 10, 17},
	{1350, 12, 59, 16, 12, 14, 22, 15, 10, 16},
	{1340, 12, 58, 16, 12, 14, 21, 15, 10, 16},
	{1330, 11, 58, 16, 12, 14, 21, 15, 9, 16},
	{1320, 11, 57, 16, 12, 14, 21, 15, 9, 16},
	{1310, 11, 57, 16, 12, 14, 21, 15, 9, 16},
	{1300, 11, 56, 16, 12, 14, 21, 15, 9, 16},
	{1290, 11, 56, 16, 12, 14, 21, 15, 9, 16},
	{1280, 11, 56, 15, 11, 14, 20, 14, 9, 16},
	{1270, 11, 55, 15, 11, 14, 20, 14, 9, 15},
	{1260, 11, 55, 15, 11, 13, 20, 14, 9, 15},
	{1250, 11, 54, 15, 11, 13, 20, 14, 9, 15},
	{1240, 11, 54, 15, 11, 13, 20, 14, 9, 15},
	{1230, 11, 53, 15, 11, 13, 19, 14, 9, 15},
	{1220, 10, 53, 15, 11, 13, 19, 14, 9, 15},
	{1210, 10, 52, 15, 11, 13, 19, 14, 9, 15},
	{1200, 10, 52, 15, 11, 13, 19, 14, 9, 15},
	{1190, 10, 52, 15, 11, 13, 19, 14, 8, 14},
	{1180, 10, 51, 15, 11, 13, 19, 13, 8, 14},
	{1170, 10, 51, 15, 10, 12, 18, 13, 8, 14},
	{1160, 10, 50, 15, 10, 12, 18, 13, 8, 14},
	{1150, 10, 50, 15, 10, 12, 18, 13, 8, 14},
	{1140, 10, 49, 14, 10, 12, 18, 13, 8, 14},
	{1130, 10, 49, 14, 10, 12, 18, 13, 8, 14},
	{1120, 10, 49, 14, 10, 12, 17, 13, 8, 14},
	{1110, 9, 48, 14, 10, 12, 17, 13, 8, 13},
	{1100, 9, 48, 14, 10, 12, 17, 13, 8, 13},
	{1090, 9, 47, 14, 10, 12, 17, 13, 8, 13},
	{1080, 9, 47, 14, 10, 11, 17, 13, 8, 13},
	{1070, 9, 46, 14, 10, 11, 17, 12, 8, 13},
	{1060, 9, 46, 14, 10, 11, 16, 12, 7, 13},
	{1050, 9, 45, 14, 9, 11, 16, 12, 7, 13},
	{1040, 9, 45, 14, 9, 11, 16, 12, 7, 13},
	{1030, 9, 45, 14, 9, 11, 16, 12, 7, 12},
	{1020, 9, 44, 14, 9, 11, 16, 12, 7, 12},
	{1010, 8, 44, 13, 9, 11, 15, 12, 7, 12},
	{1000, 8, 43, 13, 9, 11, 15, 12, 7, 12},
	{990, 8, 43, 13, 9, 10, 15, 12, 7, 12},
	{980, 8, 42, 13, 9, 10, 15, 12, 7, 12},
	{970, 8, 42, 13, 9, 10, 15, 12, 7, 12},
	{960, 8, 42, 13, 9, 10, 15, 11, 7, 12},
	{950, 8, 41, 13, 9, 10, 14, 11, 7, 11},
	{940, 8, 41, 13, 8, 10, 14, 11, 7, 11},
	{930, 8, 40, 13, 8, 10, 14, 11, 6, 11},
	{920, 8, 40, 13, 8, 10, 14, 11, 6, 11},
	{910, 8, 39, 13, 8, 9, 14, 11, 6, 11},
	{900, 7, 39, 13, 8, 9, 13, 11, 6, 11},
	{890, 7, 38, 13, 8, 9, 13, 11, 6, 11},
	{880, 7, 38, 12, 8, 9, 13, 11, 6, 11},
	{870, 7, 38, 12, 8, 9, 13, 11, 6, 10},
	{860, 7, 37, 12, 8, 9, 13, 11, 6, 10},
	{850, 7, 37, 12, 8, 9, 13, 10, 6, 10},
	{840, 7, 36, 12, 8, 9, 12, 10, 6, 10},
	{830, 7, 36, 12, 8, 9, 12, 10, 6, 10},
	{820, 7, 35, 12, 7, 8, 12, 10, 6, 10},
	{810, 7, 35, 12, 7, 8, 12, 10, 6, 10},
	{800, 7, 35, 12, 7, 8, 12, 10, 6, 10},
	{790, 6, 34, 12, 7, 8, 11, 10, 5, 9},
	{780, 6, 34, 12, 7, 8, 11, 10, 5, 9},
	{770, 6, 33, 12, 7, 8, 11, 10, 5, 9},
	{760, 6, 33, 12, 7, 8, 11, 10, 5, 9},
	{750, 6, 32, 12, 7, 8, 11, 9, 5, 9},
	{740, 6, 32, 11, 7, 8, 11, 9, 5, 9},
	{730, 6, 31, 11, 7, 7, 10, 9, 5, 9},
	{720, 6, 31, 11, 7, 7, 10, 9, 5, 9},
	{710, 6, 31, 11, 6, 7, 10, 9, 5, 8},
	{700, 6, 30, 11, 6, 7, 10, 9, 5, 8},
	{690, 5, 30, 11, 6, 7, 10, 9, 5, 8},
	{680, 5, 29, 11, 6, 7, 9, 9, 5, 8},
	{670, 5, 29, 11, 6, 7, 9, 9, 5, 8},
	{660, 5, 28, 11, 6, 7, 9, 9, 4, 8},
	{650, 5, 28, 11, 6, 7, 9, 9, 4, 8},
	{640, 5, 28, 11, 6, 6, 9, 8, 4, 8},
	{630, 5, 27, 11, 6, 6, 9, 8, 4, 7},
	{620, 5, 27, 11, 6, 6, 8, 8, 4, 7},
	{610, 5, 26, 10, 6, 6, 8, 8, 4, 7},
	{600, 5, 26, 10, 6, 6, 8, 8, 4, 7},
	{590, 5, 25, 10, 5, 6, 8, 8, 4, 7},
	{580, 4, 25, 10, 5, 6, 8, 8, 4, 7},
	{570, 4, 24, 10, 5, 6, 7, 8, 4, 7},
	{560, 4, 24, 10, 5, 6, 7, 8, 4, 7},
	{550, 4, 24, 10, 5, 5, 7, 8, 4, 6},
	{540, 4, 23, 10, 5, 5, 7, 8, 4, 6},
	{530, 4, 23, 10, 5, 5, 7, 7, 3, 6},
	{520, 4, 22, 10, 5, 5, 7, 7, 3, 6},
	{510, 4, 22, 10, 5, 5, 6, 7, 3, 6},
	{500, 4, 21, 10, 5, 5, 6, 7, 3, 6},
	{490, 4, 21, 10, 5, 5, 6, 7, 3, 6},
	{480, 4, 21, 9, 4, 5, 6, 7, 3, 6},
};

#ifdef CONFIG_OF
static const struct of_device_id exynos5_dsim[] = {
	{ .compatible = "samsung,exynos5-dsim" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_dsim);
#endif

struct mipi_dsim_device *dsim_for_decon;
EXPORT_SYMBOL(dsim_for_decon);


#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int s5p_mipi_dsi_hibernation_power_on(struct display_driver *dispdrv);
int s5p_mipi_dsi_hibernation_power_off(struct display_driver *dispdrv);
#endif

int s5p_dsim_init_d_phy(struct mipi_dsim_device *dsim, unsigned int enable)
{

#ifdef CONFIG_SOC_EXYNOS5430
	exynos5_dism_phy_enable(0, enable);
#else
	unsigned int reg;

	reg = readl(S5P_MIPI_DPHY_CONTROL(1)) & ~(1 << 0);
	reg |= (enable << 0);
	writel(reg, S5P_MIPI_DPHY_CONTROL(1));

	reg = readl(S5P_MIPI_DPHY_CONTROL(1)) & ~(1 << 2);
	reg |= (enable << 2);
	writel(reg, S5P_MIPI_DPHY_CONTROL(1));
#endif
	return 0;
}

#ifdef CONFIG_DECON_MIC
static void decon_mipi_dsi_config_mic(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_enable_mic(dsim, true);
	s5p_mipi_dsi_set_3d_off_mic_on_h_size(dsim);
}
#endif

static void s5p_mipi_dsi_set_packet_ctrl(struct mipi_dsim_device *dsim)
{
	writel(0xffff, dsim->reg_base + S5P_DSIM_MULTI_PKT);
}

static void s5p_mipi_dsi_long_data_wr(struct mipi_dsim_device *dsim, unsigned int data0, unsigned int data1)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < data1; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((data1 - data_cnt) < 4) {
			if ((data1 - data_cnt) == 3) {
				payload = *(u8 *)(data0 + data_cnt) |
				    (*(u8 *)(data0 + (data_cnt + 1))) << 8 |
					(*(u8 *)(data0 + (data_cnt + 2))) << 16;
			dev_dbg(dsim->dev, "count = 3 payload = %x, %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)));
			} else if ((data1 - data_cnt) == 2) {
				payload = *(u8 *)(data0 + data_cnt) |
					(*(u8 *)(data0 + (data_cnt + 1))) << 8;
			dev_dbg(dsim->dev,
				"count = 2 payload = %x, %x %x\n", payload,
				*(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)));
			} else if ((data1 - data_cnt) == 1) {
				payload = *(u8 *)(data0 + data_cnt);
			}

			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		/* send 4bytes per one time. */
		} else {
			payload = *(u8 *)(data0 + data_cnt) |
				(*(u8 *)(data0 + (data_cnt + 1))) << 8 |
				(*(u8 *)(data0 + (data_cnt + 2))) << 16 |
				(*(u8 *)(data0 + (data_cnt + 3))) << 24;

			dev_dbg(dsim->dev,
				"count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)),
				*(u8 *)(data0 + (data_cnt + 3)));

			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		}
	}
}

int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim, unsigned int data_id,
	unsigned int data0, unsigned int data1)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_add_refcount(get_display_driver());
#endif

	if (dsim->enabled == false || dsim->state != DSIM_STATE_HSCLKEN) {
		dev_dbg(dsim->dev, "MIPI DSIM is not ready.\n");
		return -EINVAL;
	}

	mutex_lock(&dsim_rd_wr_mutex);
	switch (data_id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data0, data1);
		break;

	/* general command */
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data0, data1);
		break;

	/* packet types for video data */
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_END_OF_TRANSMISSION:
		break;

	/* short and response packet types for command */
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		s5p_mipi_dsi_clear_all_interrupt(dsim);
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data0, data1);
		/* process response func should be implemented. */
		break;

	/* long packet type and null packet */
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
		break;

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		unsigned int size, data_cnt = 0, payload = 0;

		size = data1 * 4;
		INIT_COMPLETION(dsim_wr_comp);
		/* if data count is less then 4, then send 3bytes data.  */
		if (data1 < 4) {
			payload = *(u8 *)(data0) |
				*(u8 *)(data0 + 1) << 8 |
				*(u8 *)(data0 + 2) << 16;

			s5p_mipi_dsi_wr_tx_data(dsim, payload);

			dev_dbg(dsim->dev, "count = %d payload = %x,%x %x %x\n",
				data1, payload,
				*(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)));
		/* in case that data count is more then 4 */
		} else
			s5p_mipi_dsi_long_data_wr(dsim, data0, data1);

		/* put data into header fifo */
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data1 & 0xff,
			(data1 & 0xff00) >> 8);

		if (!wait_for_completion_interruptible_timeout(&dsim_wr_comp,
			MIPI_WR_TIMEOUT)) {
				dev_err(dsim->dev, "MIPI DSIM write Timeout!\n");
				mutex_unlock(&dsim_rd_wr_mutex);
				return -1;
		}
		break;
	}

	/* packet typo for video data */
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		break;
	default:
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n",
			data_id);

		mutex_unlock(&dsim_rd_wr_mutex);
		return -EINVAL;
	}
	mutex_unlock(&dsim_rd_wr_mutex);

	return 0;
}

static void s5p_mipi_dsi_rx_err_handler(struct mipi_dsim_device *dsim,
	u32 rx_fifo)
{
	/* Parse error report bit*/
	if (rx_fifo & (1 << 8))
		dev_err(dsim->dev, "SoT error!\n");
	if (rx_fifo & (1 << 9))
		dev_err(dsim->dev, "SoT sync error!\n");
	if (rx_fifo & (1 << 10))
		dev_err(dsim->dev, "EoT error!\n");
	if (rx_fifo & (1 << 11))
		dev_err(dsim->dev, "Escape mode entry command error!\n");
	if (rx_fifo & (1 << 12))
		dev_err(dsim->dev, "Low-power transmit sync error!\n");
	if (rx_fifo & (1 << 13))
		dev_err(dsim->dev, "HS receive timeout error!\n");
	if (rx_fifo & (1 << 14))
		dev_err(dsim->dev, "False control error!\n");
	/* Bit 15 is reserved*/
	if (rx_fifo & (1 << 16))
		dev_err(dsim->dev, "ECC error, single-bit(detected and corrected)!\n");
	if (rx_fifo & (1 << 17))
		dev_err(dsim->dev, "ECC error, multi-bit(detected, not corrected)!\n");
	if (rx_fifo & (1 << 18))
		dev_err(dsim->dev, "Checksum error(long packet only)!\n");
	if (rx_fifo & (1 << 19))
		dev_err(dsim->dev, "DSI data type not recognized!\n");
	if (rx_fifo & (1 << 20))
		dev_err(dsim->dev, "DSI VC ID invalid!\n");
	if (rx_fifo & (1 << 21))
		dev_err(dsim->dev, "Invalid transmission length!\n");
	/* Bit 22 is reserved */
	if (rx_fifo & (1 << 23))
		dev_err(dsim->dev, "DSI protocol violation!\n");
}

int s5p_mipi_dsi_rd_data(struct mipi_dsim_device *dsim, u32 data_id,
	 u32 addr, u32 count, u8 *buf)
{
	u32 rx_fifo, txhd, rx_size;
	int i, j;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_add_refcount(get_display_driver());
#endif

	if (dsim->enabled == false || dsim->state != DSIM_STATE_HSCLKEN) {
		dev_dbg(dsim->dev, "MIPI DSIM is not ready.\n");
		return -EINVAL;
	}

	mutex_lock(&dsim_rd_wr_mutex);
	INIT_COMPLETION(dsim_rd_comp);

	/* Set the maximum packet size returned */
	txhd = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE | count << 8;
	writel(txhd, dsim->reg_base + S5P_DSIM_PKTHDR);

	 /* Read request */
	txhd = data_id | addr << 8;
	writel(txhd, dsim->reg_base + S5P_DSIM_PKTHDR);

	if (!wait_for_completion_interruptible_timeout(&dsim_rd_comp,
		MIPI_RD_TIMEOUT)) {
		dev_err(dsim->dev, "MIPI DSIM read Timeout!\n");
		mutex_unlock(&dsim_rd_wr_mutex);
		return -ETIMEDOUT;
	}

	rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);

	/* Parse the RX packet data types */
	switch (rx_fifo & 0xff) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		s5p_mipi_dsi_rx_err_handler(dsim, rx_fifo);
		goto rx_error;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dev_dbg(dsim->dev, "EoTp was received from LCD module.\n");
		break;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		dev_dbg(dsim->dev, "Short Packet was received from LCD module.\n");
		for (i = 0; i <= count; i++)
			buf[i] = (rx_fifo >> (8 + i * 8)) & 0xff;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		dev_dbg(dsim->dev, "Long Packet was received from LCD module.\n");
		rx_size = (rx_fifo & 0x00ffff00) >> 8;
		/* Read data from RX packet payload */
		for (i = 0; i < rx_size >> 2; i++) {
			rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			buf[0 + i] = (u8)(rx_fifo >> 0) & 0xff;
			buf[1 + i] = (u8)(rx_fifo >> 8) & 0xff;
			buf[2 + i] = (u8)(rx_fifo >> 16) & 0xff;
			buf[3 + i] = (u8)(rx_fifo >> 24) & 0xff;
		}
		if (rx_size % 4) {
			rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			for (j = 0; j < rx_size % 4; j++)
				buf[4 * i + j] =
					(u8)(rx_fifo >> (j * 8)) & 0xff;
		}
		break;
	default:
		dev_err(dsim->dev, "Packet format is invaild.\n");
		goto rx_error;
	}

	rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
	if (rx_fifo != DSIM_RX_FIFO_READ_DONE) {
		dev_info(dsim->dev, "[DSIM:WARN]:%s Can't find RX FIFO READ DONE FLAG : %x\n",
			__func__, rx_fifo);
		goto clear_rx_fifo;
	}
	mutex_unlock(&dsim_rd_wr_mutex);
	return 0;

clear_rx_fifo:
	i = 0;
	while (1) {
		rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
		if ((rx_fifo == DSIM_RX_FIFO_READ_DONE) ||
				(i > DSIM_MAX_RX_FIFO))
			break;
		dev_info(dsim->dev, "[DSIM:INFO] : %s clear rx fifo : %08x\n",
			__func__, rx_fifo);
		i++;
	}
	mutex_unlock(&dsim_rd_wr_mutex);
	return 0;

rx_error:
	s5p_mipi_dsi_force_dphy_stop_state(dsim, 1);
	usleep_range(3000, 4000);
	s5p_mipi_dsi_force_dphy_stop_state(dsim, 0);
	mutex_unlock(&dsim_rd_wr_mutex);
	return -1;
}

int s5p_mipi_dsi_pll_on(struct mipi_dsim_device *dsim, unsigned int enable)
{
	int sw_timeout;

	if (enable) {
		sw_timeout = 1000;

		s5p_mipi_dsi_clear_interrupt(dsim, INTSRC_PLL_STABLE);
		s5p_mipi_dsi_enable_pll(dsim, 1);
		while (1) {
			sw_timeout--;
			if (s5p_mipi_dsi_is_pll_stable(dsim))
				return 0;
			if (sw_timeout == 0)
				return -EINVAL;
		}
	} else
		s5p_mipi_dsi_enable_pll(dsim, 0);

	return 0;
}

unsigned long s5p_mipi_dsi_change_pll(struct mipi_dsim_device *dsim,
	unsigned int pre_divider, unsigned int main_divider,
	unsigned int scaler)
{
	unsigned long dfin_pll, dfvco, dpll_out;
	unsigned int i, freq_band = 0xf;

	dfin_pll = (FIN_HZ / pre_divider);

	if (soc_is_exynos5250()) {
		if (dfin_pll < DFIN_PLL_MIN_HZ || dfin_pll > DFIN_PLL_MAX_HZ) {
			dev_warn(dsim->dev, "fin_pll range should be 6MHz ~ 12MHz\n");
			s5p_mipi_dsi_enable_afc(dsim, 0, 0);
		} else {
			if (dfin_pll < 7 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x1);
			else if (dfin_pll < 8 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x0);
			else if (dfin_pll < 9 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x3);
			else if (dfin_pll < 10 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x2);
			else if (dfin_pll < 11 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x5);
			else
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x4);
		}
	}
	dfvco = dfin_pll * main_divider;
	dev_dbg(dsim->dev, "dfvco = %lu, dfin_pll = %lu, main_divider = %d\n",
				dfvco, dfin_pll, main_divider);

	if (soc_is_exynos5250()) {
		if (dfvco < DFVCO_MIN_HZ || dfvco > DFVCO_MAX_HZ)
			dev_warn(dsim->dev, "fvco range should be 500MHz ~ 1000MHz\n");
	}

	dpll_out = dfvco / (1 << scaler);
	dev_dbg(dsim->dev, "dpll_out = %lu, dfvco = %lu, scaler = %d\n",
		dpll_out, dfvco, scaler);

	if (soc_is_exynos5250()) {
		for (i = 0; i < ARRAY_SIZE(dpll_table); i++) {
			if (dpll_out < dpll_table[i] * MHZ) {
				freq_band = i;
				break;
			}
		}
	}

	dev_dbg(dsim->dev, "freq_band = %d\n", freq_band);
	s5p_mipi_dsi_pll_freq(dsim, pre_divider, main_divider, scaler);

	s5p_mipi_dsi_hs_zero_ctrl(dsim, 0);
	s5p_mipi_dsi_prep_ctrl(dsim, 0);

	if (soc_is_exynos5250()) {
		/* Freq Band */
		s5p_mipi_dsi_pll_freq_band(dsim, freq_band);
	}
	/* Stable time */
	s5p_mipi_dsi_pll_stable_time(dsim, dsim->dsim_config->pll_stable_time);

	/* Enable PLL */
	dev_dbg(dsim->dev, "FOUT of mipi dphy pll is %luMHz\n",
		(dpll_out / MHZ));

	return dpll_out;
}

static u32 decon_mipi_dsi_calc_pms(struct mipi_dsim_device *dsim)
{
	u32 p_div, m_div, s_div;
	u32 target_freq, fin_pll, voc_out, fout_cal;
	u32 fin = 24;
	/*
	 * One clk lane consists of 2 lines.
	 * HS clk freq = line rate X 2
	 * Here, we calculate the freq of ONE line(fout_cal is the freq of ONE line).
	 * Thus, target_freq = dsim->lcd_info->hs_clk/2.
	 */
	target_freq = dsim->lcd_info->hs_clk / 2;

	for (p_div = 1; p_div <= 33; p_div++)
		for (m_div = 25; m_div <= 125; m_div++)
			for (s_div = 0; s_div <= 3; s_div++) {

				fin_pll = fin / p_div;
				voc_out = (m_div * fin) / p_div;
				fout_cal = (m_div * fin) / (p_div * (1 << s_div));

				if ((fin_pll < 6) || (fin_pll > 12))
					continue;
				if ((voc_out < 300) || (voc_out > 750))
					continue;
				if (fout_cal < target_freq)
					continue;
				if ((target_freq == fout_cal) && (fout_cal <= 750))
					goto calculation_success;
			}

	for (p_div = 1; p_div <= 33; p_div++)
		for (m_div = 25; m_div <= 125; m_div++)
			for (s_div = 0; s_div <= 3; s_div++) {

				fin_pll = fin / p_div;
				voc_out = (m_div * fin) / p_div;
				fout_cal = (m_div * fin) / (p_div * (1 << s_div));

				if ((fin_pll < 6) || (fin_pll > 12))
					continue;
				if ((voc_out < 300) || (voc_out > 750))
					continue;
				if (fout_cal < target_freq)
					continue;
				/* target_freq < fout_cal, here is different from the above */
				if ((target_freq < fout_cal) && (fout_cal <= 750))
					goto calculation_success;
			}

	dev_err(dsim->dev, "Failed to calculate PMS values\n");
	return -EINVAL;

calculation_success:
	dsim->dsim_config->p = p_div;
	dsim->dsim_config->m = m_div;
	dsim->dsim_config->s = s_div;
	dev_dbg(dsim->dev, "High Speed Clock rate = %dMhz, P = %d, M = %d, S = %d\n",
			fout_cal * 2, p_div, m_div, s_div);

	return fout_cal * 2;
}

static int decon_mipi_dsi_get_dphy_timing(struct mipi_dsim_device *dsim)
{
	bool loop = true;
	/* The index of the last element in array */
	int i = sizeof(dphy_timing) / sizeof(dphy_timing[0]) - 1;

	/* In case of resume, don't run the following code */
	if (dsim->timing.bps)
		return 1;

	while (loop) {
		if (dphy_timing[i][0] < dsim->hs_clk) {
			i--;
			continue;
		} else {
			dsim->timing.bps = dsim->hs_clk;
			dsim->timing.clk_prepare = dphy_timing[i][1];
			dsim->timing.clk_zero = dphy_timing[i][2];
			dsim->timing.clk_post = dphy_timing[i][3];
			dsim->timing.clk_trail = dphy_timing[i][4];
			dsim->timing.hs_prepare = dphy_timing[i][5];
			dsim->timing.hs_zero = dphy_timing[i][6];
			dsim->timing.hs_trail = dphy_timing[i][7];
			dsim->timing.lpx = dphy_timing[i][8];
			dsim->timing.hs_exit = dphy_timing[i][9];
			loop = false;
		}
	}

	switch (dsim->dsim_config->esc_clk) {
	/* Mhz */
	case 20:
		dsim->timing.b_dphyctl = 0x1f4;
		break;
	case 19:
		dsim->timing.b_dphyctl = 0x1db;
		break;
	case 18:
		dsim->timing.b_dphyctl = 0x1c2;
		break;
	case 17:
		dsim->timing.b_dphyctl = 0x1a9;
		break;
	case 16:
		dsim->timing.b_dphyctl = 0x190;
		break;
	case 15:
		dsim->timing.b_dphyctl = 0x177;
		break;
	case 14:
		dsim->timing.b_dphyctl = 0x15e;
		break;
	case 13:
		dsim->timing.b_dphyctl = 0x145;
		break;
	case 12:
		dsim->timing.b_dphyctl = 0x12c;
		break;
	case 11:
		dsim->timing.b_dphyctl = 0x113;
		break;
	case 10:
		dsim->timing.b_dphyctl = 0xfa;
		break;
	case 9:
		dsim->timing.b_dphyctl = 0xe1;
		break;
	case 8:
		dsim->timing.b_dphyctl = 0xc8;
		break;
	case 7:
		dsim->timing.b_dphyctl = 0xaf;
		break;
	default:
		dev_err(dsim->dev, "Escape clock is set too low\n");
		break;
	}

	return 1;
}

static int decon_mipi_dsi_set_dphy_timing_value(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_set_timing_register0(dsim, dsim->timing.lpx,
			dsim->timing.hs_exit);
	s5p_mipi_dsi_set_timing_register1(dsim, dsim->timing.clk_prepare,
			dsim->timing.clk_zero,	dsim->timing.clk_post,
			dsim->timing.clk_trail);
	s5p_mipi_dsi_set_timing_register2(dsim, dsim->timing.hs_prepare,
			dsim->timing.hs_zero, dsim->timing.hs_trail);
	s5p_mipi_dsi_set_b_dphyctrl(dsim, dsim->timing.b_dphyctl);
	return 1;
}

int s5p_mipi_dsi_set_clock(struct mipi_dsim_device *dsim,
	unsigned int byte_clk_sel, unsigned int enable)
{
	unsigned int esc_div;
	unsigned long esc_clk_error_rate;
	struct decon_lcd *lcd_info  = dsim->lcd_info;

	if (enable) {
		dsim->e_clk_src = byte_clk_sel;
		dsim->dsim_config->esc_clk = lcd_info->esc_clk;

		/* Escape mode clock and byte clock source */
		s5p_mipi_dsi_set_byte_clock_src(dsim, byte_clk_sel);

		/* DPHY, DSIM Link : D-PHY clock out */
		if (byte_clk_sel == DSIM_PLL_OUT_DIV8) {
			dsim->hs_clk = decon_mipi_dsi_calc_pms(dsim);
			if (dsim->hs_clk == -EINVAL) {
				dev_err(dsim->dev,
					"failed to get hs clock.\n");
				return -EINVAL;
			}

			decon_mipi_dsi_get_dphy_timing(dsim);
			s5p_mipi_dsi_change_pll(dsim,
				dsim->dsim_config->p, dsim->dsim_config->m,
				dsim->dsim_config->s);
			if (!soc_is_exynos5250())
				decon_mipi_dsi_set_dphy_timing_value(dsim);
			dsim->byte_clk = dsim->hs_clk / 8;
			s5p_mipi_dsi_enable_pll_bypass(dsim, 0);
			s5p_mipi_dsi_pll_on(dsim, 1);
		/* DPHY : D-PHY clock out, DSIM link : external clock out */
		} else if (byte_clk_sel == DSIM_EXT_CLK_DIV8)
			dev_warn(dsim->dev,
				"this project is not support \
				external clock source for MIPI DSIM\n");
		else if (byte_clk_sel == DSIM_EXT_CLK_BYPASS)
			dev_warn(dsim->dev,
				"this project is not support \
				external clock source for MIPI DSIM\n");

		/* escape clock divider */
		esc_div = dsim->byte_clk / (dsim->dsim_config->esc_clk);
		dev_dbg(dsim->dev,
			"esc_div = %d, byte_clk = %lu, esc_clk = %lu\n",
			esc_div, dsim->byte_clk, dsim->dsim_config->esc_clk);

		if (soc_is_exynos5250()) {
			if ((dsim->byte_clk / esc_div) >= (20 * MHZ) ||
					(dsim->byte_clk / esc_div) >
						dsim->dsim_config->esc_clk)
				esc_div += 1;
		} else {
			if ((dsim->byte_clk / esc_div) >= (10 * MHZ) ||
				(dsim->byte_clk / esc_div) >
					dsim->dsim_config->esc_clk)
				esc_div += 1;
		}
		dsim->escape_clk = dsim->byte_clk / esc_div;
		dev_dbg(dsim->dev,
			"escape_clk = %lu, byte_clk = %lu, esc_div = %d\n",
			dsim->escape_clk, dsim->byte_clk, esc_div);

		/* enable byte clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_ON);

		/* enable escape clock */
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 1, esc_div);
		/* escape clock on lane */
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 1);

		dev_dbg(dsim->dev, "byte clock is %luMHz\n",
			(dsim->byte_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock that user's need is %lu\n",
			(dsim->dsim_config->esc_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock divider is %x\n", esc_div);
		dev_dbg(dsim->dev, "escape clock is %luMHz\n",
			((dsim->byte_clk / esc_div) / MHZ));

		if ((dsim->byte_clk / esc_div) > dsim->escape_clk) {
			esc_clk_error_rate = dsim->escape_clk /
				(dsim->byte_clk / esc_div);
			dev_warn(dsim->dev, "error rate is %lu over.\n",
				(esc_clk_error_rate / 100));
		} else if ((dsim->byte_clk / esc_div) < (dsim->escape_clk)) {
			esc_clk_error_rate = (dsim->byte_clk / esc_div) /
				dsim->escape_clk;
			dev_warn(dsim->dev, "error rate is %lu under.\n",
				(esc_clk_error_rate / 100));
		}
	} else {
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 0, 0);

		/* disable escape clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_OFF);

		if (byte_clk_sel == DSIM_PLL_OUT_DIV8)
			s5p_mipi_dsi_pll_on(dsim, 0);
	}

	return 0;
}

void s5p_mipi_dsi_d_phy_onoff(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	/*
	if (dsim->pd->init_d_phy)
		dsim->pd->init_d_phy(dsim, enable);
	*/
	s5p_dsim_init_d_phy(dsim, enable);
}

int s5p_mipi_dsi_init_dsim(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	switch (dsim->dsim_config->e_no_data_lane) {
	case DSIM_DATA_LANE_1:
		dsim->data_lane = DSIM_LANE_DATA0;
		break;
	case DSIM_DATA_LANE_2:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1;
		break;
	case DSIM_DATA_LANE_3:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2;
		break;
	case DSIM_DATA_LANE_4:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2 | DSIM_LANE_DATA3;
		break;
	default:
		dev_info(dsim->dev, "data lane is invalid.\n");
		return -EINVAL;
	};

	s5p_mipi_dsi_sw_reset(dsim);
	s5p_mipi_dsi_dp_dn_swap(dsim, 0);

	return 0;
}

int s5p_mipi_dsi_enable_frame_done_int(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	/* enable only frame done interrupt */
	s5p_mipi_dsi_set_interrupt_mask(dsim, INTMSK_FRAME_DONE, enable);

	return 0;
}

int s5p_mipi_dsi_set_display_mode(struct mipi_dsim_device *dsim,
	struct mipi_dsim_config *dsim_config)
{
	unsigned int width = 0, height = 0;
	u32 vbp, vfp, hbp, hfp, vsync_len, hsync_len;

	width = dsim->lcd_info->xres;
	height = dsim->lcd_info->yres;

	vbp = dsim->lcd_info->vbp;
	vfp = dsim->lcd_info->vfp;
	hbp = dsim->lcd_info->hbp;
	hfp = dsim->lcd_info->hfp;

	vsync_len = dsim->lcd_info->vsa;
	hsync_len = dsim->lcd_info->hsa;

	/* in case of VIDEO MODE (RGB INTERFACE) */
	if (dsim->lcd_info->mode == VIDEO_MODE) {
		s5p_mipi_dsi_set_main_disp_vporch(dsim,
				2, /* cmd allow */
				1, /* stable vfp */
				vbp);
		s5p_mipi_dsi_set_main_disp_hporch(dsim,	hfp, hbp);
		s5p_mipi_dsi_set_main_disp_sync_area(dsim, vsync_len, hsync_len);
	}
#ifdef CONFIG_DECON_MIC
	width = s5p_mipi_dsi_calc_bs_size(dsim);
#endif
	s5p_mipi_dsi_set_main_disp_resol(dsim, height, width);
	s5p_mipi_dsi_display_config(dsim);
	return 0;
}

int s5p_mipi_dsi_init_link(struct mipi_dsim_device *dsim)
{
	unsigned int time_out = 100;
	unsigned int id;
	id = dsim->id;
	switch (dsim->state) {
	case DSIM_STATE_INIT:
		/* dsi configuration */
		s5p_mipi_dsi_init_config(dsim);
		s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 1);
		s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 1);

		/* set clock configuration */
		s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 1);

		/* check clock and data lane state are stop state */
		while (!(s5p_mipi_dsi_is_lane_state(dsim))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"DSI Master is not stop state.\n");
				dev_err(dsim->dev,
					"Check initialization process\n");

				return -EINVAL;
			}
		}

		if (time_out != 0) {
			dev_dbg(dsim->dev,
				"DSI Master driver has been completed.\n");
			dev_dbg(dsim->dev, "DSI Master state is stop state\n");
		}

		dsim->state = DSIM_STATE_STOP;

		/* BTA sequence counters */
		s5p_mipi_dsi_set_stop_state_counter(dsim,
			dsim->dsim_config->stop_holding_cnt);
		s5p_mipi_dsi_set_bta_timeout(dsim,
			dsim->dsim_config->bta_timeout);
		s5p_mipi_dsi_set_lpdr_timeout(dsim,
			dsim->dsim_config->rx_timeout);
		s5p_mipi_dsi_set_packet_ctrl(dsim);
		return 0;
	default:
		dev_info(dsim->dev, "DSI Master is already init.\n");
		return 0;
	}

	return 0;
}

int s5p_mipi_dsi_set_hs_enable(struct mipi_dsim_device *dsim)
{
	unsigned int time_out = 1000;
	if (dsim->state == DSIM_STATE_STOP) {
		if (dsim->e_clk_src != DSIM_EXT_CLK_BYPASS) {
			dsim->state = DSIM_STATE_HSCLKEN;

			 /* set LCDC and CPU transfer mode to HS. */
			s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
			s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);

			s5p_mipi_dsi_enable_hs_clock(dsim, 1);

			while (!(s5p_mipi_dsi_is_hs_state(dsim))) {
				time_out--;
				if (time_out == 0) {
					dev_err(dsim->dev,
							"DSI Master is not HS state.\n");
					return -EBUSY;
				}
				usleep_range(10, 10);
			}

			return 0;
		} else
			dev_warn(dsim->dev,
				"clock source is external bypass.\n");
	} else
		dev_warn(dsim->dev, "DSIM is not stop state.\n");

	return 0;
}

int s5p_mipi_dsi_set_data_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int mode)
{
	if (mode) {
		if (dsim->state != DSIM_STATE_HSCLKEN) {
			dev_err(dsim->dev, "HS Clock lane is not enabled.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
	} else {
		if (dsim->state == DSIM_STATE_INIT || dsim->state ==
			DSIM_STATE_ULPS) {
			dev_err(dsim->dev,
				"DSI Master is not STOP or HSDT state.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);
	}
	return 0;
}

int s5p_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim)
{
	return _s5p_mipi_dsi_get_frame_done_status(dsim);
}

int s5p_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim)
{
	_s5p_mipi_dsi_clear_frame_done(dsim);

	return 0;
}

static int s5p_mipi_dsi_set_interrupt(struct mipi_dsim_device *dsim, bool enable)
{
	unsigned int int_msk;

	int_msk = SFR_PL_FIFO_EMPTY | RX_DAT_DONE | MIPI_FRAME_DONE | ERR_RX_ECC;

	if (enable) {
		/* clear interrupt */
		s5p_mipi_dsi_clear_all_interrupt(dsim);
		s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 0);
	} else {
		s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 1);
	}

	return 0;
}

static irqreturn_t s5p_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	unsigned int int_src;
	struct mipi_dsim_device *dsim = dev_id;
	int framedone = 0;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();
#endif

	spin_lock(&dsim->slock);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	if (dispdrv->platform_status > DISP_STATUS_PM0 &&
			!dispdrv->pm_status.clock_enabled) {
		dev_err(dsim->dev, "IRQ occured during clock-gating!\n");
		spin_unlock(&dsim->slock);
		return IRQ_HANDLED;
	}
#endif
	s5p_mipi_dsi_set_interrupt_mask(dsim, 0xffffffff, 1);
	int_src = readl(dsim->reg_base + S5P_DSIM_INTSRC);

	/* Test bit */
	if (int_src & SFR_PL_FIFO_EMPTY)
		complete(&dsim_wr_comp);
	if (int_src & RX_DAT_DONE)
		complete(&dsim_rd_comp);
	if (int_src & MIPI_FRAME_DONE)
		framedone = 1;
	if (int_src & ERR_RX_ECC)
		dev_err(dsim->dev, "RX ECC Multibit error was detected!\n");
	s5p_mipi_dsi_clear_interrupt(dsim, int_src);

	s5p_mipi_dsi_set_interrupt_mask(dsim, 0xffffffff, 0);
	spin_unlock(&dsim->slock);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	/* tiggering power event for PM */
	if (framedone)
		disp_pm_dec_refcount(dispdrv);
#endif

	return IRQ_HANDLED;
}

int s5p_mipi_dsi_enable(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;
	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (dsim->enabled == true)
		return 0;

	GET_DISPDRV_OPS(dispdrv).enable_display_driver_clocks(dsim->dev);
	GET_DISPDRV_OPS(dispdrv).enable_display_driver_power(dsim->dev);

	if (dsim->dsim_lcd_drv->resume)
		dsim->dsim_lcd_drv->resume(dsim);
	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);
	dsim->enabled = true;

#ifdef CONFIG_DECON_MIC
	decon_mipi_dsi_config_mic(dsim);
#endif
	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	/* enable interrupts */
	s5p_mipi_dsi_set_interrupt(dsim, true);

	usleep_range(1000, 1500);
	dsim->dsim_lcd_drv->displayon(dsim);

	return 0;
}

int s5p_mipi_dsi_disable(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;
	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (dsim->enabled == false)
		return 0;

	/* disable interrupts */
	s5p_mipi_dsi_set_interrupt(dsim, false);

	dsim->enabled = false;
	dsim->dsim_lcd_drv->suspend(dsim);
	dsim->state = DSIM_STATE_SUSPEND;
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	GET_DISPDRV_OPS(dispdrv).disable_display_driver_power(dsim->dev);

	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int s5p_mipi_dsi_lcd_off(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;
	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	dsim->enabled = false;
	dsim->dsim_lcd_drv->suspend(dsim);
	dsim->state = DSIM_STATE_SUSPEND;

	GET_DISPDRV_OPS(dispdrv).disable_display_driver_power(dsim->dev);

	return 0;
}
#endif

int s5p_mipi_dsi_ulps_enable(struct mipi_dsim_device *dsim,
	unsigned int mode)
{
	int ret = 0;
	unsigned int time_out = 1000;

	if (mode == false) {
		if (dsim->state == DSIM_STATE_STOP)
			return ret;

		/* Exit ULPS clock and data lane */
		s5p_mipi_dsi_enable_ulps_exit_clk_data(dsim, 1);

		/* Check ULPS Exit request for data lane */
		while (!(s5p_mipi_dsi_is_ulps_lane_state(dsim, 0))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"%s: DSI Master is not stop state.\n", __func__);
				return -EBUSY;
			}
			usleep_range(10, 10);
		}

		/* Clear ULPS enter & exit state */
		s5p_mipi_dsi_enable_ulps_exit_clk_data(dsim, 0);

               dsim->state = DSIM_STATE_STOP;
	} else {
               if (dsim->state == DSIM_STATE_ULPS)
                       return ret;

		/* Disable TxRequestHsClk */
		s5p_mipi_dsi_enable_hs_clock(dsim, 0);

		/* Enable ULPS clock and data lane */
		s5p_mipi_dsi_enable_ulps_clk_data(dsim, 1);

		/* Check ULPS request for data lane */
		while (!(s5p_mipi_dsi_is_ulps_lane_state(dsim, 1))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"%s: DSI Master is not ULPS state.\n", __func__);

				/* Enable ULPS clock and data lane */
				s5p_mipi_dsi_enable_ulps_clk_data(dsim, 1);

				/* Disable TxRequestHsClk */
				s5p_mipi_dsi_enable_hs_clock(dsim, 0);
				return -EBUSY;
			}
			usleep_range(10, 10);
		}

		/* Clear ULPS enter & exit state */
		s5p_mipi_dsi_enable_ulps_clk_data(dsim, 0);

		dsim->state = DSIM_STATE_ULPS;
	}

	return ret;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int s5p_mipi_dsi_hibernation_power_on(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	if (dsim->enabled == true)
		return 0;

	GET_DISPDRV_OPS(dispdrv).enable_display_driver_clocks(dsim->dev);

	/* PPI signal disable + D-PHY reset */
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);
	/* Stable time */
	s5p_mipi_dsi_pll_stable_time(dsim, dsim->dsim_config->pll_stable_time);

	/* Enable PHY PLL */
	s5p_mipi_dsi_pll_on(dsim, 1);

	/* Exit ULPS mode clk & data */
	s5p_mipi_dsi_ulps_enable(dsim, false);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);
	dsim->enabled = true;

#ifdef CONFIG_DECON_MIC
	decon_mipi_dsi_config_mic(dsim);
#endif
	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	s5p_mipi_dsi_set_interrupt(dsim, true);

	return 0;
}

int s5p_mipi_dsi_hibernation_power_off(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	if (dsim->enabled == false)
		return 0;

	s5p_mipi_dsi_set_interrupt(dsim, false);

	/* Enter ULPS mode clk & data */
	s5p_mipi_dsi_ulps_enable(dsim, true);

	/* DSIM STOP SEQUENCE */
	/* Set main stand-by off */
	s5p_mipi_dsi_enable_main_standby(dsim, 0);

	/* CLK and LANE disable */
	s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 0);
	s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 0);

	/* escape clock on lane */
	s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);

	/* Disable byte clock */
	s5p_mipi_dsi_enable_byte_clock(dsim, 0);

	/* Disable PHY PLL */
	s5p_mipi_dsi_pll_on(dsim, 0);

	/* S/W reset */
	s5p_mipi_dsi_sw_reset(dsim);

	/* PPI signal disable + D-PHY reset */
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	dsim->enabled = false;

	return 0;
}
#endif

int create_mipi_dsi_controller(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = NULL;
	struct display_driver *dispdrv;
	int ret = -1;

	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (!dsim)
		dsim = kzalloc(sizeof(struct mipi_dsim_device),
			GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -EFAULT;
	}

	dispdrv->dsi_driver.dsim = dsim;

	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;

	spin_lock_init(&dsim->slock);

	dsim->dsim_config = dispdrv->dt_ops.get_display_dsi_drvdata();

	dsim->lcd_info = decon_get_lcd_info();

	dsim->reg_base = devm_request_and_ioremap(&pdev->dev, dispdrv->dsi_driver.regs);
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "mipi-dsi: failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/*
	 * it uses frame done interrupt handler
	 * only in case of MIPI Video mode.
	 */
	dsim->irq = dispdrv->dsi_driver.dsi_irq_no;
	if (request_irq(dsim->irq, s5p_mipi_dsi_interrupt_handler,
			IRQF_DISABLED, "mipi-dsi", dsim)) {
		dev_err(&pdev->dev, "request_irq failed.\n");
		goto err_irq;
	}

	dsim->dsim_lcd_drv = dsim->dsim_config->dsim_ddi_pd;

	dsim->timing.bps = 0;

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);
	dsim->dsim_lcd_drv->probe(dsim);

	GET_DISPDRV_OPS(dispdrv).enable_display_driver_power(&pdev->dev);

	dsim->enabled = true;
#ifdef CONFIG_DECON_MIC
	decon_mipi_dsi_config_mic(dsim);
#endif
	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);
	dsim->dsim_lcd_drv->displayon(dsim);
	dsim_for_decon = dsim;
	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim->dsim_config->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");

	dispdrv->dsi_driver.dsim = dsim;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	if (dispdrv->dsi_driver.ops) {
		dispdrv->dsi_driver.ops->pwr_on = s5p_mipi_dsi_hibernation_power_on;
		dispdrv->dsi_driver.ops->pwr_off = s5p_mipi_dsi_hibernation_power_off;
	}
#endif

	/* enable interrupts */
	s5p_mipi_dsi_set_interrupt(dsim, true);

	mutex_init(&dsim_rd_wr_mutex);
	return 0;

err_irq:
	release_resource(dispdrv->dsi_driver.regs);
	kfree(dispdrv->dsi_driver.regs);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	kfree(dsim);
	return ret;

}

MODULE_AUTHOR("Haowei li <haowei.li@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSI driver");
MODULE_LICENSE("GPL");
