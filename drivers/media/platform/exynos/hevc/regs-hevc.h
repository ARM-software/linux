/*
 * Register definition file for Samsung HEVC Driver
 *
 * Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_HEVC_H
#define _REGS_HEVC_H

#define HEVC_REG_SIZE	(HEVC_END_ADDR - HEVC_START_ADDR)
#define HEVC_REG_COUNT	((HEVC_END_ADDR - HEVC_START_ADDR) / 4)

/* Number of bits that the buffer address should be shifted for particular
 * HEVC buffers.  */
#define HEVC_MEM_OFFSET		0

#define HEVC_START_ADDR		0x0000
#define HEVC_END_ADDR		0xfd80

#define HEVC_REG_CLEAR_BEGIN	0xf000
#define HEVC_REG_CLEAR_COUNT	1024

/* Codec Common Registers */
#define HEVC_RISC_ON			0x0000
#define HEVC_RISC2HOST_INT			0x003C
#define HEVC_HOST2RISC_INT			0x0044
#define HEVC_RISC_BASE_ADDRESS		0x0054

#define HEVC_DEC_RESET			0x1070

#define HEVC_HOST2RISC_CMD			0x1100
#define HEVC_H2R_CMD_EMPTY			0
#define HEVC_H2R_CMD_SYS_INIT		1
#define HEVC_H2R_CMD_OPEN_INSTANCE		2
#define HEVC_CH_SEQ_HEADER			3
#define HEVC_CH_INIT_BUFS			4
#define HEVC_CH_NAL_START			5
#define HEVC_CH_FRAME_START			HEVC_CH_NAL_START
#define HEVC_H2R_CMD_CLOSE_INSTANCE		6
#define HEVC_H2R_CMD_SLEEP			7
#define HEVC_H2R_CMD_WAKEUP			8
#define HEVC_CH_LAST_FRAME			9
#define HEVC_H2R_CMD_FLUSH			10
#define HEVC_CH_NAL_ABORT			11
/* RMVME: REALLOC used? */
#define HEVC_CH_FRAME_START_REALLOC		5

#define HEVC_RISC2HOST_CMD			0x1104
#define HEVC_R2H_CMD_EMPTY			0
#define HEVC_R2H_CMD_SYS_INIT_RET		1
#define HEVC_R2H_CMD_OPEN_INSTANCE_RET	2
#define HEVC_R2H_CMD_SEQ_DONE_RET		3
#define HEVC_R2H_CMD_INIT_BUFFERS_RET	4

#define HEVC_R2H_CMD_CLOSE_INSTANCE_RET	6
#define HEVC_R2H_CMD_SLEEP_RET		7
#define HEVC_R2H_CMD_WAKEUP_RET		8
#define HEVC_R2H_CMD_COMPLETE_SEQ_RET	9
#define HEVC_R2H_CMD_DPB_FLUSH_RET		10
#define HEVC_R2H_CMD_NAL_ABORT_RET		11
#define HEVC_R2H_CMD_FW_STATUS_RET		12
#define HEVC_R2H_CMD_FRAME_DONE_RET		13
#define HEVC_R2H_CMD_FIELD_DONE_RET		14
#define HEVC_R2H_CMD_SLICE_DONE_RET		15
#define HEVC_R2H_CMD_ERR_RET		32

#define HEVC_H2R_CMD_FW_STATUS		12	/* dummy H/W command */
#define R2H_BIT(x)	(((x) > 0) ? (1 << ((x) - 1)) : 0)
static inline unsigned int h2r_to_r2h_bits(int cmd)
{
	unsigned int mask = 0;

	switch (cmd) {
	case HEVC_H2R_CMD_FW_STATUS:
	case HEVC_H2R_CMD_SYS_INIT:
	case HEVC_H2R_CMD_OPEN_INSTANCE:
	case HEVC_CH_SEQ_HEADER:
	case HEVC_CH_INIT_BUFS:
	case HEVC_H2R_CMD_CLOSE_INSTANCE:
	case HEVC_H2R_CMD_SLEEP:
	case HEVC_H2R_CMD_WAKEUP:
	case HEVC_H2R_CMD_FLUSH:
	case HEVC_CH_NAL_ABORT:
		mask |= R2H_BIT(cmd);
		break;
	case HEVC_CH_FRAME_START:
		mask |= (R2H_BIT(HEVC_R2H_CMD_FRAME_DONE_RET) |
			 R2H_BIT(HEVC_R2H_CMD_FIELD_DONE_RET) |
			 R2H_BIT(HEVC_R2H_CMD_SLICE_DONE_RET));
		break;
	case HEVC_CH_LAST_FRAME:
		mask |= R2H_BIT(HEVC_R2H_CMD_FRAME_DONE_RET);
		break;
	}

	return (mask |= R2H_BIT(HEVC_R2H_CMD_ERR_RET));
}

static inline unsigned int r2h_bits(int cmd)
{
	unsigned int mask = R2H_BIT(cmd);

	if (cmd == HEVC_R2H_CMD_FRAME_DONE_RET)
		mask |= (R2H_BIT(HEVC_R2H_CMD_FIELD_DONE_RET) |
			 R2H_BIT(HEVC_R2H_CMD_SLICE_DONE_RET));
	/* FIXME: Temporal mask for S3D SEI processing */
	else if (cmd == HEVC_R2H_CMD_INIT_BUFFERS_RET)
		mask |= (R2H_BIT(HEVC_R2H_CMD_FIELD_DONE_RET) |
			 R2H_BIT(HEVC_R2H_CMD_SLICE_DONE_RET) |
			 R2H_BIT(HEVC_R2H_CMD_FRAME_DONE_RET));

	return (mask |= R2H_BIT(HEVC_R2H_CMD_ERR_RET));
}

#define HEVC_BUS_RESET_CTRL		0x2110
#define HEVC_FW_VERSION			0xF000

#define HEVC_INSTANCE_ID			0xF008
#define HEVC_CODEC_TYPE			0xF00C
#define HEVC_CONTEXT_MEM_ADDR		0xF014
#define HEVC_CONTEXT_MEM_SIZE		0xF018
#define HEVC_PIXEL_FORMAT			0xF020

#define HEVC_METADATA_ENABLE		0xF024
#define HEVC_DBG_BUFFER_ADDR		0xF030
#define HEVC_DBG_BUFFER_SIZE		0xF034

#define HEVC_HED_CONTROL		0xF038
#define HEVC_DEC_TIMEOUT_VALUE		0xF03C
#define HEVC_SHARED_MEM_ADDR		0xF040

#define HEVC_RET_INSTANCE_ID		0xF070
#define HEVC_ERROR_CODE			0xF074

#define HEVC_ERR_WARNINGS_START		160
#define HEVC_ERR_WARNINGS_END		222
#define HEVC_ERR_DEC_MASK			0xFFFF
#define HEVC_ERR_DEC_SHIFT			0
#define HEVC_ERR_DSPL_MASK			0xFFFF0000
#define HEVC_ERR_DSPL_SHIFT			16

#define HEVC_DBG_BUFFER_OUTPUT_SIZE		0xF078
#define HEVC_METADATA_STATUS		0xF07C
#define HEVC_METADATA_ADDR_MB_INFO		0xF080
#define HEVC_METADATA_SIZE_MB_INFO		0xF084
#define HEVC_DBG_INFO_STAGE_COUNTER		0xF088

/* Decoder Registers */
#define HEVC_D_CRC_CTRL			0xF0B0
#define HEVC_D_DEC_OPTIONS			0xF0B4
#define HEVC_D_OPT_DISPLAY_LINEAR_EN	11
#define HEVC_D_OPT_DISCARD_RCV_HEADER	7
#define HEVC_D_OPT_IDR_DECODING_SHFT	6
#define HEVC_D_OPT_FMO_ASO_CTRL_MASK	4
#define HEVC_D_OPT_DDELAY_EN_SHIFT		3
#define HEVC_D_OPT_LF_CTRL_SHIFT		1
#define HEVC_D_OPT_LF_CTRL_MASK		0x3
#define HEVC_D_OPT_TILE_MODE_SHIFT		0
#define HEVC_D_OPT_DYNAMIC_DPB_SET_SHIFT	3

#define HEVC_D_DISPLAY_DELAY		0xF0B8

#define HEVC_D_SET_FRAME_WIDTH		0xF0BC
#define HEVC_D_SET_FRAME_HEIGHT		0xF0C0

#define HEVC_D_SEI_ENABLE			0xF0C4
#define HEVC_D_SEI_NEED_INIT_BUFFER_SHIFT	1

/* Buffer setting registers */
/* Session return */
#define HEVC_D_MIN_NUM_DPB                                  0xF0F0
#define HEVC_D_MIN_FIRST_PLANE_DPB_SIZE                     0xF0F4
#define HEVC_D_MIN_SECOND_PLANE_DPB_SIZE                    0xF0F8
#define HEVC_D_MIN_THIRD_PLANE_DPB_SIZE                     0xF0FC
#define HEVC_D_MIN_NUM_MV                                   0xF100

/* Buffers */
#define HEVC_D_NUM_DPB                                      0xF130
#define HEVC_D_NUM_MV                                       0xF134
#define HEVC_D_FIRST_PLANE_DPB_STRIDE_SIZE                  0xF138
#define HEVC_D_SECOND_PLANE_DPB_STRIDE_SIZE                 0xF13C
#define HEVC_D_THIRD_PLANE_DPB_STRIDE_SIZE                  0xF140
#define HEVC_D_FIRST_PLANE_DPB_SIZE                         0xF144
#define HEVC_D_SECOND_PLANE_DPB_SIZE                        0xF148
#define HEVC_D_THIRD_PLANE_DPB_SIZE                         0xF14C
#define HEVC_D_MV_BUFFER_SIZE                               0xF150
#define HEVC_D_FIRST_PLANE_DPB0                             0xF160
#define HEVC_D_FIRST_PLANE_DPB1                             0xF164
#define HEVC_D_FIRST_PLANE_DPB2                             0xF168
#define HEVC_D_FIRST_PLANE_DPB3                             0xF16C
#define HEVC_D_FIRST_PLANE_DPB4                             0xF170
#define HEVC_D_FIRST_PLANE_DPB5                             0xF174
#define HEVC_D_FIRST_PLANE_DPB6                             0xF178
#define HEVC_D_FIRST_PLANE_DPB7                             0xF17C
#define HEVC_D_FIRST_PLANE_DPB8                             0xF180
#define HEVC_D_FIRST_PLANE_DPB9                             0xF184
#define HEVC_D_FIRST_PLANE_DPB10                            0xF188
#define HEVC_D_FIRST_PLANE_DPB11                            0xF18C
#define HEVC_D_FIRST_PLANE_DPB12                            0xF190
#define HEVC_D_FIRST_PLANE_DPB13                            0xF194
#define HEVC_D_FIRST_PLANE_DPB14                            0xF198
#define HEVC_D_FIRST_PLANE_DPB15                            0xF19C
#define HEVC_D_FIRST_PLANE_DPB16                            0xF1A0
#define HEVC_D_FIRST_PLANE_DPB17                            0xF1A4
#define HEVC_D_FIRST_PLANE_DPB18                            0xF1A8
#define HEVC_D_FIRST_PLANE_DPB19                            0xF1AC
#define HEVC_D_FIRST_PLANE_DPB20                            0xF1B0
#define HEVC_D_FIRST_PLANE_DPB21                            0xF1B4
#define HEVC_D_FIRST_PLANE_DPB22                            0xF1B8
#define HEVC_D_FIRST_PLANE_DPB23                            0xF1BC
#define HEVC_D_FIRST_PLANE_DPB24                            0xF1C0
#define HEVC_D_FIRST_PLANE_DPB25                            0xF1C4
#define HEVC_D_FIRST_PLANE_DPB26                            0xF1C8
#define HEVC_D_FIRST_PLANE_DPB27                            0xF1CC
#define HEVC_D_FIRST_PLANE_DPB28                            0xF1D0
#define HEVC_D_FIRST_PLANE_DPB29                            0xF1D4
#define HEVC_D_FIRST_PLANE_DPB30                            0xF1D8
#define HEVC_D_FIRST_PLANE_DPB31                            0xF1DC
#define HEVC_D_FIRST_PLANE_DPB32                            0xF1E0
#define HEVC_D_FIRST_PLANE_DPB33                            0xF1E4
#define HEVC_D_FIRST_PLANE_DPB34                            0xF1E8
#define HEVC_D_FIRST_PLANE_DPB35                            0xF1EC
#define HEVC_D_FIRST_PLANE_DPB36                            0xF1F0
#define HEVC_D_FIRST_PLANE_DPB37                            0xF1F4
#define HEVC_D_FIRST_PLANE_DPB38                            0xF1F8
#define HEVC_D_FIRST_PLANE_DPB39                            0xF1FC
#define HEVC_D_FIRST_PLANE_DPB40                            0xF200
#define HEVC_D_FIRST_PLANE_DPB41                            0xF204
#define HEVC_D_FIRST_PLANE_DPB42                            0xF208
#define HEVC_D_FIRST_PLANE_DPB43                            0xF20C
#define HEVC_D_FIRST_PLANE_DPB44                            0xF210
#define HEVC_D_FIRST_PLANE_DPB45                            0xF214
#define HEVC_D_FIRST_PLANE_DPB46                            0xF218
#define HEVC_D_FIRST_PLANE_DPB47                            0xF21C
#define HEVC_D_FIRST_PLANE_DPB48                            0xF220
#define HEVC_D_FIRST_PLANE_DPB49                            0xF224
#define HEVC_D_FIRST_PLANE_DPB50                            0xF228
#define HEVC_D_FIRST_PLANE_DPB51                            0xF22C
#define HEVC_D_FIRST_PLANE_DPB52                            0xF230
#define HEVC_D_FIRST_PLANE_DPB53                            0xF234
#define HEVC_D_FIRST_PLANE_DPB54                            0xF238
#define HEVC_D_FIRST_PLANE_DPB55                            0xF23C
#define HEVC_D_FIRST_PLANE_DPB56                            0xF240
#define HEVC_D_FIRST_PLANE_DPB57                            0xF244
#define HEVC_D_FIRST_PLANE_DPB58                            0xF248
#define HEVC_D_FIRST_PLANE_DPB59                            0xF24C
#define HEVC_D_FIRST_PLANE_DPB60                            0xF250
#define HEVC_D_FIRST_PLANE_DPB61                            0xF254
#define HEVC_D_FIRST_PLANE_DPB62                            0xF258
#define HEVC_D_FIRST_PLANE_DPB63                            0xF25C
#define HEVC_D_SECOND_PLANE_DPB0                            0xF260
#define HEVC_D_SECOND_PLANE_DPB1                            0xF264
#define HEVC_D_SECOND_PLANE_DPB2                            0xF268
#define HEVC_D_SECOND_PLANE_DPB3                            0xF26C
#define HEVC_D_SECOND_PLANE_DPB4                            0xF270
#define HEVC_D_SECOND_PLANE_DPB5                            0xF274
#define HEVC_D_SECOND_PLANE_DPB6                            0xF278
#define HEVC_D_SECOND_PLANE_DPB7                            0xF27C
#define HEVC_D_SECOND_PLANE_DPB8                            0xF280
#define HEVC_D_SECOND_PLANE_DPB9                            0xF284
#define HEVC_D_SECOND_PLANE_DPB10                           0xF288
#define HEVC_D_SECOND_PLANE_DPB11                           0xF28C
#define HEVC_D_SECOND_PLANE_DPB12                           0xF290
#define HEVC_D_SECOND_PLANE_DPB13                           0xF294
#define HEVC_D_SECOND_PLANE_DPB14                           0xF298
#define HEVC_D_SECOND_PLANE_DPB15                           0xF29C
#define HEVC_D_SECOND_PLANE_DPB16                           0xF2A0
#define HEVC_D_SECOND_PLANE_DPB17                           0xF2A4
#define HEVC_D_SECOND_PLANE_DPB18                           0xF2A8
#define HEVC_D_SECOND_PLANE_DPB19                           0xF2AC
#define HEVC_D_SECOND_PLANE_DPB20                           0xF2B0
#define HEVC_D_SECOND_PLANE_DPB21                           0xF2B4
#define HEVC_D_SECOND_PLANE_DPB22                           0xF2B8
#define HEVC_D_SECOND_PLANE_DPB23                           0xF2BC
#define HEVC_D_SECOND_PLANE_DPB24                           0xF2C0
#define HEVC_D_SECOND_PLANE_DPB25                           0xF2C4
#define HEVC_D_SECOND_PLANE_DPB26                           0xF2C8
#define HEVC_D_SECOND_PLANE_DPB27                           0xF2CC
#define HEVC_D_SECOND_PLANE_DPB28                           0xF2D0
#define HEVC_D_SECOND_PLANE_DPB29                           0xF2D4
#define HEVC_D_SECOND_PLANE_DPB30                           0xF2D8
#define HEVC_D_SECOND_PLANE_DPB31                           0xF2DC
#define HEVC_D_SECOND_PLANE_DPB32                           0xF2E0
#define HEVC_D_SECOND_PLANE_DPB33                           0xF2E4
#define HEVC_D_SECOND_PLANE_DPB34                           0xF2E8
#define HEVC_D_SECOND_PLANE_DPB35                           0xF2EC
#define HEVC_D_SECOND_PLANE_DPB36                           0xF2F0
#define HEVC_D_SECOND_PLANE_DPB37                           0xF2F4
#define HEVC_D_SECOND_PLANE_DPB38                           0xF2F8
#define HEVC_D_SECOND_PLANE_DPB39                           0xF2FC
#define HEVC_D_SECOND_PLANE_DPB40                           0xF300
#define HEVC_D_SECOND_PLANE_DPB41                           0xF304
#define HEVC_D_SECOND_PLANE_DPB42                           0xF308
#define HEVC_D_SECOND_PLANE_DPB43                           0xF30C
#define HEVC_D_SECOND_PLANE_DPB44                           0xF310
#define HEVC_D_SECOND_PLANE_DPB45                           0xF314
#define HEVC_D_SECOND_PLANE_DPB46                           0xF318
#define HEVC_D_SECOND_PLANE_DPB47                           0xF31C
#define HEVC_D_SECOND_PLANE_DPB48                           0xF320
#define HEVC_D_SECOND_PLANE_DPB49                           0xF324
#define HEVC_D_SECOND_PLANE_DPB50                           0xF328
#define HEVC_D_SECOND_PLANE_DPB51                           0xF32C
#define HEVC_D_SECOND_PLANE_DPB52                           0xF330
#define HEVC_D_SECOND_PLANE_DPB53                           0xF334
#define HEVC_D_SECOND_PLANE_DPB54                           0xF338
#define HEVC_D_SECOND_PLANE_DPB55                           0xF33C
#define HEVC_D_SECOND_PLANE_DPB56                           0xF340
#define HEVC_D_SECOND_PLANE_DPB57                           0xF344
#define HEVC_D_SECOND_PLANE_DPB58                           0xF348
#define HEVC_D_SECOND_PLANE_DPB59                           0xF34C
#define HEVC_D_SECOND_PLANE_DPB60                           0xF350
#define HEVC_D_SECOND_PLANE_DPB61                           0xF354
#define HEVC_D_SECOND_PLANE_DPB62                           0xF358
#define HEVC_D_SECOND_PLANE_DPB63                           0xF35C
#define HEVC_D_THIRD_PLANE_DPB0                             0xF360
#define HEVC_D_THIRD_PLANE_DPB1                             0xF364
#define HEVC_D_THIRD_PLANE_DPB2                             0xF368
#define HEVC_D_THIRD_PLANE_DPB3                             0xF36C
#define HEVC_D_THIRD_PLANE_DPB4                             0xF370
#define HEVC_D_THIRD_PLANE_DPB5                             0xF374
#define HEVC_D_THIRD_PLANE_DPB6                             0xF378
#define HEVC_D_THIRD_PLANE_DPB7                             0xF37C
#define HEVC_D_THIRD_PLANE_DPB8                             0xF380
#define HEVC_D_THIRD_PLANE_DPB9                             0xF384
#define HEVC_D_THIRD_PLANE_DPB10                            0xF388
#define HEVC_D_THIRD_PLANE_DPB11                            0xF38C
#define HEVC_D_THIRD_PLANE_DPB12                            0xF390
#define HEVC_D_THIRD_PLANE_DPB13                            0xF394
#define HEVC_D_THIRD_PLANE_DPB14                            0xF398
#define HEVC_D_THIRD_PLANE_DPB15                            0xF39C
#define HEVC_D_THIRD_PLANE_DPB16                            0xF3A0
#define HEVC_D_THIRD_PLANE_DPB17                            0xF3A4
#define HEVC_D_THIRD_PLANE_DPB18                            0xF3A8
#define HEVC_D_THIRD_PLANE_DPB19                            0xF3AC
#define HEVC_D_THIRD_PLANE_DPB20                            0xF3B0
#define HEVC_D_THIRD_PLANE_DPB21                            0xF3B4
#define HEVC_D_THIRD_PLANE_DPB22                            0xF3B8
#define HEVC_D_THIRD_PLANE_DPB23                            0xF3BC
#define HEVC_D_THIRD_PLANE_DPB24                            0xF3C0
#define HEVC_D_THIRD_PLANE_DPB25                            0xF3C4
#define HEVC_D_THIRD_PLANE_DPB26                            0xF3C8
#define HEVC_D_THIRD_PLANE_DPB27                            0xF3CC
#define HEVC_D_THIRD_PLANE_DPB28                            0xF3D0
#define HEVC_D_THIRD_PLANE_DPB29                            0xF3D4
#define HEVC_D_THIRD_PLANE_DPB30                            0xF3D8
#define HEVC_D_THIRD_PLANE_DPB31                            0xF3DC
#define HEVC_D_THIRD_PLANE_DPB32                            0xF3E0
#define HEVC_D_THIRD_PLANE_DPB33                            0xF3E4
#define HEVC_D_THIRD_PLANE_DPB34                            0xF3E8
#define HEVC_D_THIRD_PLANE_DPB35                            0xF3EC
#define HEVC_D_THIRD_PLANE_DPB36                            0xF3F0
#define HEVC_D_THIRD_PLANE_DPB37                            0xF3F4
#define HEVC_D_THIRD_PLANE_DPB38                            0xF3F8
#define HEVC_D_THIRD_PLANE_DPB39                            0xF3FC
#define HEVC_D_THIRD_PLANE_DPB40                            0xF400
#define HEVC_D_THIRD_PLANE_DPB41                            0xF404
#define HEVC_D_THIRD_PLANE_DPB42                            0xF408
#define HEVC_D_THIRD_PLANE_DPB43                            0xF40C
#define HEVC_D_THIRD_PLANE_DPB44                            0xF410
#define HEVC_D_THIRD_PLANE_DPB45                            0xF414
#define HEVC_D_THIRD_PLANE_DPB46                            0xF418
#define HEVC_D_THIRD_PLANE_DPB47                            0xF41C
#define HEVC_D_THIRD_PLANE_DPB48                            0xF420
#define HEVC_D_THIRD_PLANE_DPB49                            0xF424
#define HEVC_D_THIRD_PLANE_DPB50                            0xF428
#define HEVC_D_THIRD_PLANE_DPB51                            0xF42C
#define HEVC_D_THIRD_PLANE_DPB52                            0xF430
#define HEVC_D_THIRD_PLANE_DPB53                            0xF434
#define HEVC_D_THIRD_PLANE_DPB54                            0xF438
#define HEVC_D_THIRD_PLANE_DPB55                            0xF43C
#define HEVC_D_THIRD_PLANE_DPB56                            0xF440
#define HEVC_D_THIRD_PLANE_DPB57                            0xF444
#define HEVC_D_THIRD_PLANE_DPB58                            0xF448
#define HEVC_D_THIRD_PLANE_DPB59                            0xF44C
#define HEVC_D_THIRD_PLANE_DPB60                            0xF450
#define HEVC_D_THIRD_PLANE_DPB61                            0xF454
#define HEVC_D_THIRD_PLANE_DPB62                            0xF458
#define HEVC_D_THIRD_PLANE_DPB63                            0xF45C
#define HEVC_D_MV_BUFFER0                                   0xF460
#define HEVC_D_MV_BUFFER1                                   0xF464
#define HEVC_D_MV_BUFFER2                                   0xF468
#define HEVC_D_MV_BUFFER3                                   0xF46C
#define HEVC_D_MV_BUFFER4                                   0xF470
#define HEVC_D_MV_BUFFER5                                   0xF474
#define HEVC_D_MV_BUFFER6                                   0xF478
#define HEVC_D_MV_BUFFER7                                   0xF47C
#define HEVC_D_MV_BUFFER8                                   0xF480
#define HEVC_D_MV_BUFFER9                                   0xF484
#define HEVC_D_MV_BUFFER10                                  0xF488
#define HEVC_D_MV_BUFFER11                                  0xF48C
#define HEVC_D_MV_BUFFER12                                  0xF490
#define HEVC_D_MV_BUFFER13                                  0xF494
#define HEVC_D_MV_BUFFER14                                  0xF498
#define HEVC_D_MV_BUFFER15                                  0xF49C
#define HEVC_D_MV_BUFFER16                                  0xF4A0
#define HEVC_D_MV_BUFFER17                                  0xF4A4
#define HEVC_D_MV_BUFFER18                                  0xF4A8
#define HEVC_D_MV_BUFFER19                                  0xF4AC
#define HEVC_D_MV_BUFFER20                                  0xF4B0
#define HEVC_D_MV_BUFFER21                                  0xF4B4
#define HEVC_D_MV_BUFFER22                                  0xF4B8
#define HEVC_D_MV_BUFFER23                                  0xF4BC
#define HEVC_D_MV_BUFFER24                                  0xF4C0
#define HEVC_D_MV_BUFFER25                                  0xF4C4
#define HEVC_D_MV_BUFFER26                                  0xF4C8
#define HEVC_D_MV_BUFFER27                                  0xF4CC
#define HEVC_D_MV_BUFFER28                                  0xF4D0
#define HEVC_D_MV_BUFFER29                                  0xF4D4
#define HEVC_D_MV_BUFFER30                                  0xF4D8
#define HEVC_D_MV_BUFFER31                                  0xF4DC
#define HEVC_D_MV_BUFFER32                                  0xF4E0
#define HEVC_D_MV_BUFFER33                                  0xF4E4
#define HEVC_D_MV_BUFFER34                                  0xF4E8
#define HEVC_D_MV_BUFFER35                                  0xF4EC
#define HEVC_D_MV_BUFFER36                                  0xF4F0
#define HEVC_D_MV_BUFFER37                                  0xF4F4
#define HEVC_D_MV_BUFFER38                                  0xF4F8
#define HEVC_D_MV_BUFFER39                                  0xF4FC
#define HEVC_D_MV_BUFFER40                                  0xF500
#define HEVC_D_MV_BUFFER41                                  0xF504
#define HEVC_D_MV_BUFFER42                                  0xF508
#define HEVC_D_MV_BUFFER43                                  0xF50C
#define HEVC_D_MV_BUFFER44                                  0xF510
#define HEVC_D_MV_BUFFER45                                  0xF514
#define HEVC_D_MV_BUFFER46                                  0xF518
#define HEVC_D_MV_BUFFER47                                  0xF51C
#define HEVC_D_MV_BUFFER48                                  0xF520
#define HEVC_D_MV_BUFFER49                                  0xF524
#define HEVC_D_MV_BUFFER50                                  0xF528
#define HEVC_D_MV_BUFFER51                                  0xF52C
#define HEVC_D_MV_BUFFER52                                  0xF530
#define HEVC_D_MV_BUFFER53                                  0xF534
#define HEVC_D_MV_BUFFER54                                  0xF538
#define HEVC_D_MV_BUFFER55                                  0xF53C
#define HEVC_D_MV_BUFFER56                                  0xF540
#define HEVC_D_MV_BUFFER57                                  0xF544
#define HEVC_D_MV_BUFFER58                                  0xF548
#define HEVC_D_MV_BUFFER59                                  0xF54C
#define HEVC_D_MV_BUFFER60                                  0xF550
#define HEVC_D_MV_BUFFER61                                  0xF554
#define HEVC_D_MV_BUFFER62                                  0xF558
#define HEVC_D_MV_BUFFER63                                  0xF55C
#define HEVC_D_SCRATCH_BUFFER_ADDR                          0xF560
#define HEVC_D_SCRATCH_BUFFER_SIZE                          0xF564
#define HEVC_D_METADATA_BUFFER_ADDR                         0xF568
#define HEVC_D_METADATA_BUFFER_SIZE                         0xF56C

#define HEVC_D_FIRST_PLANE_DPB_PORT_ID_UPPER                0xF570
#define HEVC_D_FIRST_PLANE_DPB_PORT_ID_LOWER                0xF574
#define HEVC_D_SECOND_PLANE_DPB_PORT_ID_UPPER               0xF578
#define HEVC_D_SECOND_PLANE_DPB_PORT_ID_LOWER               0xF57C
#define HEVC_D_THIRD_PLANE_DPB_PORT_ID_UPPER                0xF580
#define HEVC_D_THIRD_PLANE_DPB_PORT_ID_LOWER                0xF584
#define HEVC_D_MV_BUFFER_PORT_ID_UPPER                      0xF588
#define HEVC_D_MV_BUFFER_PORT_ID_LOWER                      0xF58C
#define HEVC_D_OTHER_BUFFER_PORT_ID                         0xF590
#define HEVC_D_METADATA_BUFFER_PORT_ID                      0xF594
#define HEVC_D_INIT_BUFFER_OPTIONS                          0xF598
#define HEVC_D_DYNAMIC_DPB_FLAG_UPPER                       0xF5A0
#define HEVC_D_DYNAMIC_DPB_FLAG_LOWER                       0xF5A4

/* Nal cmd */
#define HEVC_D_CPB_BUFFER_ADDR                              0xF5B0
#define HEVC_D_CPB_BUFFER_SIZE                              0xF5B4
#define HEVC_D_AVAILABLE_DPB_FLAG_UPPER                     0xF5B8
#define HEVC_D_AVAILABLE_DPB_FLAG_LOWER                     0xF5BC
#define HEVC_D_CPB_BUFFER_OFFSET                            0xF5C0
#define HEVC_D_SLICE_IF_ENABLE                              0xF5C4
#define HEVC_D_PICTURE_TAG                                  0xF5C8
#define HEVC_D_CPB_BUFFER_PORT_ID                           0xF5CC
#define HEVC_D_STREAM_DATA_SIZE                             0xF5D0

/* Nal return */
#define HEVC_D_DISPLAY_FRAME_WIDTH                          0xF600
#define HEVC_D_DISPLAY_FRAME_HEIGHT                         0xF604
#define HEVC_D_DISPLAY_STATUS                               0xF608
#define HEVC_D_DISPLAY_FIRST_PLANE_ADDR                     0xF60C
#define HEVC_D_DISPLAY_SECOND_PLANE_ADDR                    0xF610
#define HEVC_D_DISPLAY_THIRD_PLANE_ADDR                     0xF614
#define HEVC_D_DISPLAY_FRAME_TYPE                           0xF618
#define HEVC_D_DISPLAY_CROP_INFO1                           0xF61C
#define HEVC_D_DISPLAY_CROP_INFO2                           0xF620
#define HEVC_D_DISPLAY_PICTURE_PROFILE                      0xF624
#define HEVC_D_DISPLAY_FIRST_PLANE_CRC                      0xF628
#define HEVC_D_DISPLAY_SECOND_PLANE_CRC                     0xF62C
#define HEVC_D_DISPLAY_THIRD_PLANE_CRC                      0xF630
#define HEVC_D_DISPLAY_ASPECT_RATIO                         0xF634
#define HEVC_D_DISPLAY_EXTENDED_AR                          0xF638
#define HEVC_D_DECODED_FRAME_WIDTH                          0xF63C
#define HEVC_D_DECODED_FRAME_HEIGHT                         0xF640
#define HEVC_D_DECODED_STATUS                               0xF644
#define HEVC_D_DECODED_FIRST_PLANE_ADDR                     0xF648
#define HEVC_D_DECODED_SECOND_PLANE_ADDR                    0xF64C
#define HEVC_D_DECODED_THIRD_PLANE_ADDR                     0xF650
#define HEVC_D_DECODED_FRAME_TYPE                           0xF654
#define HEVC_D_DECODED_CROP_INFO1                           0xF658
#define HEVC_D_DECODED_CROP_INFO2                           0xF65C
#define HEVC_D_DECODED_PICTURE_PROFILE                      0xF660
#define HEVC_D_DECODED_NAL_SIZE                             0xF664
#define HEVC_D_DECODED_FIRST_PLANE_CRC                      0xF668
#define HEVC_D_DECODED_SECOND_PLANE_CRC                     0xF66C
#define HEVC_D_DECODED_THIRD_PLANE_CRC                      0xF670
#define HEVC_D_RET_PICTURE_TAG_TOP                          0xF674
#define HEVC_D_RET_PICTURE_TAG_BOT                          0xF678
#define HEVC_D_RET_PICTURE_TIME_TOP                         0xF67C
#define HEVC_D_RET_PICTURE_TIME_BOT                         0xF680
#define HEVC_D_CHROMA_FORMAT                                0xF684

#define HEVC_D_HEVC_INFO                                    0xF6A0
#define HEVC_D_USED_DPB_FLAG_UPPER                          0xF6A4
#define HEVC_D_USED_DPB_FLAG_LOWER                          0xF6A8

#define HEVC_D_METADATA_ADDR_SEI_NAL                        0xF6C0
#define HEVC_D_METADATA_SIZE_SEI_NAL                        0xF6C4
#define HEVC_D_METADATA_ADDR_VUI                            0xF6C8
#define HEVC_D_METADATA_SIZE_VUI                            0xF6CC

#define HEVC_D_FRAME_PACK_SEI_AVAIL                         0xF6DC
#define HEVC_D_FRAME_PACK_ARRGMENT_ID                       0xF6E0
#define HEVC_D_FRAME_PACK_SEI_INFO                          0xF6E4
#define HEVC_D_FRAME_PACK_GRID_POS                          0xF6E8

/* Frame info dump */
#define HEVC_D_SUM_FORW_COUNT                               0xF700
#define HEVC_D_SUM_FORW_MV_X                                0xF704
#define HEVC_D_SUM_FORW_MV_Y                                0xF708
#define HEVC_D_SUM_BACK_CNT                                 0xF70C
#define HEVC_D_SUM_BACK_MV_X                                0xF710
#define HEVC_D_SUM_BACK_MV_Y                                0xF714
#define HEVC_D_SUM_INTRA                                    0xF718
#define HEVC_D_SUM_QP                                       0xF71C

/* Display status */
#define HEVC_DEC_STATUS_DECODING_ONLY		0
#define HEVC_DEC_STATUS_DECODING_DISPLAY		1
#define HEVC_DEC_STATUS_DISPLAY_ONLY		2
#define HEVC_DEC_STATUS_DECODING_EMPTY		3
#define HEVC_DEC_STATUS_DECODING_STATUS_MASK	7
#define HEVC_DEC_STATUS_PROGRESSIVE			(0<<3)
#define HEVC_DEC_STATUS_INTERLACE			(1<<3)
#define HEVC_DEC_STATUS_INTERLACE_MASK		(1<<3)
#define HEVC_DEC_STATUS_RESOLUTION_MASK		(3<<4)
#define HEVC_DEC_STATUS_RESOLUTION_INC		(1<<4)
#define HEVC_DEC_STATUS_RESOLUTION_DEC		(2<<4)
#define HEVC_DEC_STATUS_RESOLUTION_SHIFT		4
#define HEVC_DEC_STATUS_CRC_GENERATED		(1<<5)
#define HEVC_DEC_STATUS_CRC_NOT_GENERATED		(0<<5)
#define HEVC_DEC_STATUS_CRC_MASK			(1<<5)

#define HEVC_D_DISPLAY_LUMA_ADDR		0xF60C
#define HEVC_D_DISPLAY_CHROMA_ADDR		0xF610

#define HEVC_DISPLAY_FRAME_MASK		7
#define HEVC_DISPLAY_FRAME_NOT_CODED	0
#define HEVC_DISPLAY_FRAME_I		1
#define HEVC_DISPLAY_FRAME_P		2
#define HEVC_DISPLAY_FRAME_B		3
#define HEVC_SHARED_CROP_INFO_H		0x0020
#define HEVC_SHARED_CROP_LEFT_MASK		0xFFFF
#define HEVC_SHARED_CROP_LEFT_SHIFT		0
#define HEVC_SHARED_CROP_RIGHT_MASK		0xFFFF0000
#define HEVC_SHARED_CROP_RIGHT_SHIFT	16
#define HEVC_SHARED_CROP_INFO_V		0x0024
#define HEVC_SHARED_CROP_TOP_MASK		0xFFFF
#define HEVC_SHARED_CROP_TOP_SHIFT		0
#define HEVC_SHARED_CROP_BOTTOM_MASK	0xFFFF0000
#define HEVC_SHARED_CROP_BOTTOM_SHIFT	16


#define HEVC_DEC_CRC_GEN_MASK		0x1
#define HEVC_DEC_CRC_GEN_SHIFT		6

#define HEVC_DECODED_FRAME_MASK		7
#define HEVC_DECODED_FRAME_NOT_CODED	0
#define HEVC_DECODED_FRAME_I		1
#define HEVC_DECODED_FRAME_P		2
#define HEVC_DECODED_FRAME_B		3

#define HEVC_D_DECODED_LUMA_ADDR		0xF648
#define HEVC_D_DECODED_CHROMA_ADDR		0xF64C

/* Codec numbers  */
#define HEVC_FORMATS_NO_CODEC		-1

#define EXYNOS_CODEC_HEVC_DEC		0

#define HEVC_SI_DISPLAY_Y_ADR		HEVC_D_DISPLAY_LUMA_ADDR
#define HEVC_SI_DISPLAY_C_ADR		HEVC_D_DISPLAY_CHROMA_ADDR
#define HEVC_SI_DECODED_Y_ADR		HEVC_D_DECODED_LUMA_ADDR
#define HEVC_SI_DECODED_C_ADR		HEVC_D_DECODED_CHROMA_ADDR

#define HEVC_CRC_LUMA0			HEVC_D_DECODED_LUMA_CRC_TOP
#define HEVC_CRC_CHROMA0			HEVC_D_DECODED_CHROMA_CRC_TOP
#define HEVC_CRC_LUMA1			HEVC_D_DECODED_LUMA_CRC_BOT
#define HEVC_CRC_CHROMA1			HEVC_D_DECODED_CHROMA_CRC_BOT
#define HEVC_CRC_DISP_LUMA0			HEVC_D_DISPLAY_LUMA_CRC_TOP
#define HEVC_CRC_DISP_CHROMA0		HEVC_D_DISPLAY_CHROMA_CRC_TOP

#define HEVC_SI_DECODED_STATUS		HEVC_D_DECODED_STATUS
#define HEVC_SI_DISPLAY_STATUS		HEVC_D_DISPLAY_STATUS
#define HEVC_SHARED_SET_FRAME_TAG		HEVC_D_PICTURE_TAG
#define HEVC_SHARED_GET_FRAME_TAG_TOP	HEVC_D_RET_PICTURE_TAG_TOP
#define HEVC_CRC_DISP_STATUS		HEVC_D_DISPLAY_STATUS

/* SEI related information */
#define HEVC_FRAME_PACK_SEI_AVAIL		HEVC_D_FRAME_PACK_SEI_AVAIL
#define HEVC_FRAME_PACK_ARRGMENT_ID		HEVC_D_FRAME_PACK_ARRGMENT_ID
#define HEVC_FRAME_PACK_SEI_INFO		HEVC_D_FRAME_PACK_SEI_INFO
#define HEVC_FRAME_PACK_GRID_POS		HEVC_D_FRAME_PACK_GRID_POS

#define HEVC_VPS_ONLY_ERROR		42

#endif /* _REGS_HEVC_H */
