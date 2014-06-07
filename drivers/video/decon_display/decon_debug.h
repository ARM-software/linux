/* drivers/video/decon_display/decon_debug.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_DEBUG_HEADER__
#define __DECON_DISPLAY_DEBUG_HEADER__

void dump_driver_data(void);
void dump_s3c_fb_variant(struct s3c_fb_variant *p_fb_variant);
void dump_s3c_fb_win_variant(struct s3c_fb_win_variant *p_fb_win_variant);
void dump_s3c_fb_win_variants(struct s3c_fb_win_variant p_fb_win_variant[],
	int num);

void decon_dump_registers(struct display_driver *pdispdrv);
void decon_dump_underrun(struct display_driver *pdispdrv);

#endif
