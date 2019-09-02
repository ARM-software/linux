/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Tiannan Zhu <tiannan.zhu@arm.com>
 *
 */

#ifndef _MALIDP_MATH_
#define _MALIDP_MATH_

#include <linux/types.h>
#include "uapi/drm/malidp_xr.h"

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

/* matrix related operation */
struct malidp_matrix3 {
	float32 data[9];
};

struct malidp_matrix4 {
	float32 data[16];
};

/* convert a 4x4 matrix to a 3x3 matrix */
void matrix_4X4_to_3X3(struct malidp_matrix4 *a, struct malidp_matrix3 *res);

/* normalize 3x3 matrix */
int normalize_matrix3(struct malidp_matrix3  *m,
		      struct round_exception *extra_data);
/* normalize 4x4 matrix */
int normalize_matrix4(struct malidp_matrix4  *m,
		      struct round_exception *extra_data);

void identity_matrix4(struct malidp_matrix4 *mat);
void identity_matrix3(struct malidp_matrix3 *mat);

int  matrix_mul_4X4(struct malidp_matrix4 *a, struct malidp_matrix4 *b,
		    struct malidp_matrix4 *res);
int  inverse_matrix_4X4(struct malidp_matrix4 *m, struct malidp_matrix4 *res);

void inverse_quaternion(struct malidp_quaternion *q,
			struct malidp_quaternion *inv_q);
void quaternion_to_matrix(struct malidp_quaternion *q,
			  struct malidp_matrix4 *res);
#endif /* _MALIDP_MATH_ */
