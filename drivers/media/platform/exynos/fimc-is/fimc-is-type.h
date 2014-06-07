#ifndef FIMC_IS_TYPE_H
#define FIMC_IS_TYPE_H

#include <linux/v4l2-mediabus.h>

enum fimc_is_device_type {
	FIMC_IS_DEVICE_SENSOR,
	FIMC_IS_DEVICE_ISCHAIN
};

struct fimc_is_window {
	u32 o_width;
	u32 o_height;
	u32 width;
	u32 height;
	u32 offs_h;
	u32 offs_v;
	u32 otf_width;
	u32 otf_height;
};

struct fimc_is_fmt {
	char				*name;
	enum v4l2_mbus_pixelcode	mbus_code;
	u32				pixelformat;
	u32				field;
	u32				num_planes;
};

struct fimc_is_image {
	u32			framerate;
	u32			num_lanes;
	struct fimc_is_window 	window;
	struct fimc_is_fmt	format;
};

#define TO_WORD_OFFSET(byte_offset) ((byte_offset) >> 2)

#endif
