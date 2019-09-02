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

#endif
