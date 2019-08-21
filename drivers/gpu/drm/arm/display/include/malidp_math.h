/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018 ARM Limited. All rights reserved.
 * Author: Tiannan Zhu <tiannan.zhu@arm.com>
 *
 */

#ifndef _MALIDP_MATH_
#define _MALIDP_MATH_

typedef __u32 float32;
struct malidp_matrix4 {
	float32 data[16];
};

#endif /* _MALIDP_MATH_ */