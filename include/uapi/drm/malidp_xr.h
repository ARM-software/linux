/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 */

#ifndef _MALIDP_XR_H_
#define _MALIDP_XR_H_

/* sensor buffer info:
 * dma_fd: file descriptor for sensor buffer
 * offset: buffer content offset from start
 * element_size: head pose size including timestamp, quarternion and position
 * element_num: the number of element in the ring
 * left_offset/right_offset: the ring address from buffer start address for
			     left/right eys.
 */

struct malidp_sensor_buffer_info {
	int dma_fd;
	__u32 offset;
	__u32 element_size;
	__u32 element_num;
	__u32 left_offset, right_offset;
};

/* quaternion describe object rotation
 * position describe ojbect transfrom
 * for usermode, x,y,z,w are float
 * for kernel driver, we use float32(__u32)
 */
#ifndef __KERNEL__
struct malidp_quaternion
{
	float x, y, z, w;
};

struct malidp_position
{
	float x, y, z;
};
#else
struct malidp_quaternion
{
	__u32 x, y, z, w;
};

struct malidp_position
{
	__u32 x, y, z;
};
#endif

struct malidp_sensor_data
{
	__u32 timestamp;
	struct malidp_position pos;
	struct malidp_quaternion quat;
};

#endif
