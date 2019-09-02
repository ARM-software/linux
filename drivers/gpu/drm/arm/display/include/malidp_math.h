/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Tiannan Zhu <tiannan.zhu@arm.com>
 *
 */

#ifndef _MALIDP_MATH_
#define _MALIDP_MATH_

#include <linux/types.h>

#define float32		__u32

#define FLOAT32_2		0x40000000
#define FLOAT32_1		0x3f800000
#define FLOAT32_NEG_1		0xbf800000
#define FLOAT32_0		0x00000000
#define FLOAT32_EPSILON		0x322bcc77

struct round_exception
{
	__u32 exception;
};

enum FLOAT32_EXCEPTION {
	float32_exception_inexact   = 1 << 0,
	float32_exception_invalid   = 1 << 1,
	float32_exception_overflow  = 1 << 2,
	float32_exception_underflow = 1 << 3,
};

float32 float32_sub(float32 a, float32 b, struct round_exception *extra_data);
float32 float32_add(float32 a, float32 b, struct round_exception *extra_data);
float32 float32_mul(float32 a, float32 b, struct round_exception *extra_data);
float32 float32_div(float32 a, float32 b, struct round_exception *extra_data);

int float32_eq(float32 a, float32 b, struct round_exception *extra_data);
int float32_lt(float32 a, float32 b, struct round_exception *extra_data);
int float32_ge(float32 a, float32 b, struct round_exception *extra_data);

/* fixed-point representation
 * qm.f:  signed fixedpoint, and m integer bits, m fractional bits
 * |sign| integer | fractional|
 *   1b    m bits     f bits
 */
__u32 to_q1_30(float32 a, struct round_exception *extra_data);

struct malidp_matrix4 {
	float32 data[16];
};

#endif /* _MALIDP_MATH_ */
