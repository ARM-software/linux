/* drivers/video/decon_display/decon_pm_exynos.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_PM_HEADER__
#define __DECON_DISPLAY_PM_HEADER__

bool check_camera_is_running(void);
bool get_display_power_status(void);
void set_hw_trigger_mask(struct s3c_fb *sfb, bool mask);
void set_default_hibernation_mode(struct display_driver *dispdrv);
int get_display_line_count(struct display_driver *dispdrv);

#endif
