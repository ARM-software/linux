/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Tiannan Zhu <tiannan.zhu@arm.com>
 *
 */

#include "malidp_math.h"

static __s32 find_leading_zero(__u32 a)
{
	__s32 c = 0;
	if (a <= 0xffff) {
		a <<= 16;
		c += 16;
	}
	if (a <= 0xffffff) {
		a <<= 8;
		c += 8;
	}
	if (a <= 0xfffffff) {
		a <<= 4;
		c += 4;
	}
	if (a <= 0x3fffffff) {
		a <<= 2;
		c += 2;
	}

	if (a <= 0x7fffffff)
		c += 1;

	return c;
}

static __s32 find_u64_leading_zero(__u64 a)
{
	__s32 c = 0;
	if (a <= 0xffffffff) {
		c += 32;
		c += find_leading_zero(a & 0xffffffff);
	} else {
		c += find_leading_zero(a >> 32);
	}
	return c;
}

static inline __u32 get_float32_exp(float32 a)
{
	return (a >> 23) & 0xff;
}

static inline __u32 get_float32_frac(float32 a)
{
	return a & 0x7fffff;
}

static inline int get_float32_sign(float32 a)
{
	return (a >> 31) == 1;
}

static inline int is_float32_nan(float32 a)
{
	return (get_float32_exp(a) == 0xFF) && (get_float32_frac(a) != 0);
}

static inline int is_float32_signaling_nan(float32 a)
{
	/* signaling nan is a nan, and it's 22th bit (count from 0) is 0 */
	return (((a >> 22) & 0x1FF) == 0x1FE) && ( a & 0x003FFFFF );
}

static inline int is_float32_zero(float32 a)
{
	return (get_float32_exp(a) == 0) && (get_float32_frac(a) == 0);
}

static inline int is_float32_subnormal(float32 a)
{
	return (get_float32_exp(a) == 0) && (get_float32_frac(a) != 0);
}

static inline int is_float32_infinite(float32 a)
{
	return (get_float32_exp(a) == 0xFF) && (get_float32_frac(a) == 0);
}

static inline float32 negative(float32 a)
{
	return (a ^ 0x80000000);
}

static inline float32 float32_abs(float32 a)
{
	return (a & 0x7FFFFFFF);
}

static inline float32 create_float32(__u8 s, __u32 e, __u32 f)
{
	return ((s << 31) + ((e & 0xFF) << 23) + f);
}

static float32 set_float32_nan(float32 a, float32 b,
			       struct round_exception *extra_data)
{
	int a_singaling = is_float32_signaling_nan(a);
	int b_singaling = is_float32_signaling_nan(b);

	if(a_singaling || b_singaling)
		extra_data->exception |= float32_exception_invalid;

	if (is_float32_nan(a)) {
		/* disable singaling */
		return a | 0x400000;
	} else {
		return b | 0x400000;
	}
}

static int shift_right_float32(__u32 a, __s32 count, __u32 *c,
				struct round_exception *extra_data)
{
	__u32 ignore_part;
	__s32 leading_zero;
	int carrier = 0;

	if (count == 0) {
		*c = a;
		return carrier;
	}

	ignore_part = a & ((1 << count) - 1);
	if (ignore_part != 0)
		extra_data->exception |= float32_exception_inexact;

	if (ignore_part > (1 << (count-1))) {
		leading_zero = find_leading_zero(a);
		a += 1 << (count-1);
		carrier = (leading_zero != find_leading_zero(a));
	} else if (ignore_part == (1 << (count-1))) {
		if ((a & (1<<count)) != 0) {
			leading_zero = find_leading_zero(a);
			a += 1 << (count-1);
			carrier = (leading_zero != find_leading_zero(a));
		}
	}
	*c = (a >> count);

	return carrier;
}

static int shift_u64_right(__u64 a, __s32 count, __u64 *c,
			    struct round_exception *extra_data)
{
	__u64 ignore_part;
	__s32 leading_zero;
	int carrier = 0;

	if (count == 0) {
		*c = a;
	} else if (count > 0) {
		ignore_part = a & (((__u64)1 << count) - 1);
		if (ignore_part != 0)
			extra_data->exception |= float32_exception_inexact;
		/* we use nearest_tie_even strategy as default strategy */
		/* if ignore part is more (or less) than half, we add 1 (or 0) */
		if (ignore_part > ((__u64)1 << (count-1))) {
			leading_zero = find_u64_leading_zero(a);
			a += ((__u64)1 << (count-1));
			/* check whether there is carrier */
			carrier = (leading_zero != find_u64_leading_zero(a));
		} else if (ignore_part == ((__u64)1 << (count-1))){
			/* if the ignore part is equal to half, we tie the result to even */
			if ((a & ((__u64)1<<count)) != 0) {
				leading_zero = find_u64_leading_zero(a);
				a += ((__u64)1 << (count-1));
				carrier = (leading_zero != find_u64_leading_zero(a));
			}

		}
		*c = (a >> count) ;
	} else {
		*c = a << (-count);
	}
	return carrier;
}

/* a and b has same sign */
static float32 float32_add_same_sign(float32 a, float32 b,
				     struct round_exception *extra_data)
{
	__u64 a_frac, b_frac, c_frac, a_exp, b_exp, c_exp;
	int a_sign, c_sign, a_subnormal, b_subnormal;
	__s32 exp_diff, leading_zero, shift_len;

	a_frac = get_float32_frac(a);
	a_exp = get_float32_exp(a);
	a_sign = get_float32_sign(a);
	c_sign = a_sign;
	b_frac = get_float32_frac(b);
	b_exp = get_float32_exp(b);
	a_subnormal = is_float32_subnormal(a);
	b_subnormal = is_float32_subnormal(b);

	/* if a or b is infinite */
	if (is_float32_infinite(a) || is_float32_infinite(b))
		return create_float32(a_sign, 0xFF, 0);

	/* if a or b is zero */
	if (is_float32_zero(a))
		return b;
	else if (is_float32_zero(b))
		return a;

	if (a_subnormal && b_subnormal) {
		/* a and b are subnormal */
		c_frac = a_frac + b_frac;
		if (c_frac >= 0x800000) {
			/* result can be normalized */
			leading_zero = find_leading_zero(c_frac);
			/* shift count is 32 - 23 - 1 - leading_zero */
			shift_right_float32(c_frac, 8 - leading_zero, (__u32*)&c_frac, extra_data);
			c_exp = 8 - leading_zero + 1;
			/* remove leading 1 at 23th bit(count from 0th)*/
			c_frac &= 0x7fffff;
			return create_float32(a_sign, c_exp, c_frac);
		} else {
			/* c is still subnormal */
			return create_float32(a_sign, 0, c_frac);
		}
	} else {
		exp_diff = a_exp - b_exp;
		if (a_subnormal) {
			/* a is subnormal, and b is normal */
			exp_diff += 1;
			/* b is normalized number, add the leading 1 */
			b_frac |= 0x800000;
		} else if (b_subnormal) {
			/* b is subnormal, and a is normal */
			exp_diff -= 1;
			/* a is normalized number, add the leading 1 */
			a_frac |= 0x800000;
		} else {
			/* a and b are both normalized number */
			a_frac |= 0x800000;
			b_frac |= 0x800000;
		}
		if (exp_diff > 24) {
			extra_data->exception |= float32_exception_inexact;
			return a;
		} else if (exp_diff < -24) {
			extra_data->exception |= float32_exception_inexact;
			return b;
		} else {
			if (exp_diff > 0)
				a_frac <<= exp_diff;
			else if (exp_diff < 0)
				b_frac <<= (-exp_diff);

			c_frac = a_frac + b_frac;
			leading_zero = find_u64_leading_zero(c_frac);
			shift_len = 64 - leading_zero - 24;
			if (shift_u64_right(c_frac, shift_len, &c_frac, extra_data))
				shift_len += 1;

			if (a_exp >= b_exp)
				c_exp = a_exp + (shift_len - exp_diff);
			else
				c_exp = b_exp + (shift_len - (-exp_diff));

			if (c_exp >= 0xFF) {
				extra_data->exception |= float32_exception_overflow;
				/* return MAX number */
				return create_float32(c_sign, 0xFF, 0x0);
			} else {
				/* remove first 1 */
				c_frac &= 0x7FFFFF;
				return create_float32(c_sign, c_exp, c_frac);
			}
		}
	}
}

/* a - b and a and b are both postive */
static float32 float32_sub_both_pos(float32 a, float32 b,
				    struct round_exception *extra_data)
{
	__u64 a_frac, b_frac, c_frac, a_exp, b_exp;
	int c_sign, a_subnormal, b_subnormal;
	__s32 exp_diff, leading_zero, shift_len, c_exp;

	a_frac = get_float32_frac(a);
	a_exp = get_float32_exp(a);
	b_frac = get_float32_frac(b);
	b_exp = get_float32_exp(b);
	a_subnormal = is_float32_subnormal(a);
	b_subnormal = is_float32_subnormal(b);

	/* if a and b is postive infinite, a - b = NAN */
	if (is_float32_infinite(a) && is_float32_infinite(b)) {
		extra_data->exception |= float32_exception_invalid;
		return create_float32(0, 0xFF, 0x1);
	} else if (is_float32_infinite(a)) {
		return a;
	} else if (is_float32_infinite(b)) {
		return negative(b);
	}

	/* if a or b is zero */
	if (is_float32_zero(b))
		return a;
	else if (is_float32_zero(a))
		return negative(b);

	if (a_subnormal && b_subnormal) {
		/* a and b are subnormal */
		if (a_frac >= b_frac) {
			c_frac = a_frac - b_frac;
			c_sign = 0;
		} else {
			c_frac = b_frac - a_frac;
			c_sign = 1;
		}
		return create_float32(c_sign, 0, c_frac);
		c_frac = a_frac + b_frac;
	} else {
		exp_diff = a_exp - b_exp;
		if (a_subnormal) {
			/* a is subnormal, and b is normal */
			leading_zero = find_leading_zero(a_frac);
			leading_zero -= 9; /* 32-23 = 9 */
			exp_diff += 1;
			/* b is normalized number, add the leading 1 */
			b_frac |= 0x800000;
		} else if (b_subnormal) {
			/* b is subnormal, and a is normal */
			exp_diff -= 1;
			/* a is normalized number, add the leading 1 */
			a_frac |= 0x800000;
		} else {
			/* a and b are both normalized number */
			a_frac |= 0x800000;
			b_frac |= 0x800000;
		}
		if (exp_diff > 24) {
			/* compare with a, b is too small, return a */
			extra_data->exception |= float32_exception_inexact;
			return a;
		} else if (exp_diff < -24) {
			/* compare with b, a is too small, return -b*/
			extra_data->exception |= float32_exception_inexact;
			return negative(b);
		} else {
			if (exp_diff > 0)
				a_frac <<= exp_diff;
			else if (exp_diff < 0)
				b_frac <<= (-exp_diff);

			if (a_frac >= b_frac) {
				c_frac = a_frac - b_frac;
				c_sign = 0;
			} else {
				c_frac = b_frac - a_frac;
				c_sign = 1;
			}

			if (c_frac == 0)
				return create_float32(0, 0, 0);

			leading_zero = find_u64_leading_zero(c_frac);
			shift_len = 64 - leading_zero - 24;
			if (shift_u64_right(c_frac, shift_len, &c_frac, extra_data))
				shift_len += 1;

			if (c_sign == 0) {
				/* a >= b, so a_exp >= b_exp*/
				c_exp = a_exp - (exp_diff - shift_len);
			} else {
				/* a < b, so a_exp <= b_exp*/
				c_exp = b_exp - (-exp_diff - shift_len);
			}

			/* a - b, a and b are both postive
			 *  so result will not beyound largest limitation
			 */
			if (c_exp >= 1) {
				c_frac &= 0x7FFFFF;
				return create_float32(c_sign, c_exp, c_frac);
			} else if (c_exp >= -23){
				/* result is subnormal number */
				shift_right_float32(c_frac, 1 - c_exp, (__u32*)&c_frac, extra_data);
				return create_float32(c_sign, 0, c_frac);
			} else {
				/* result is little than mini value */
				extra_data->exception |= float32_exception_inexact |
										 float32_exception_underflow;
				return create_float32(c_sign, 0, 1);
			}
		}
	}
}

float32 float32_add(float32 a, float32 b, struct round_exception *extra_data)
{
	int a_sign, b_sign;

	/* if a or b is not a number */
	if (is_float32_nan(a) || is_float32_nan(b))
		return set_float32_nan(a, b, extra_data);

	a_sign = get_float32_sign(a);
	b_sign = get_float32_sign(b);

	if (!(a_sign ^ b_sign))
		return float32_add_same_sign(a, b, extra_data);
	else if(a_sign)
		return float32_sub_both_pos(b, negative(a), extra_data);
	else
		return float32_sub_both_pos(a, negative(b), extra_data);
}

float32 float32_sub(float32 a, float32 b, struct round_exception *extra_data)
{
	int a_sign, b_sign;

	/* if a or b is not a number */
	if (is_float32_nan(a) || is_float32_nan(b))
		return set_float32_nan(a, b, extra_data);

	a_sign = get_float32_sign(a);
	b_sign = get_float32_sign(b);

	if (a_sign ^ b_sign) {
		return float32_add_same_sign(a, negative(b), extra_data);
	} else {
		if (a_sign) {
			return float32_sub_both_pos(negative(b), negative(a), extra_data);
		} else {
			return float32_sub_both_pos(a, b, extra_data);
		}
	}
}

float32 float32_mul(float32 a, float32 b, struct round_exception *extra_data)
{
	__u64 a_frac, b_frac, c_frac;
	int a_sign, b_sign, c_sign, a_subnormal, b_subnormal;
	__s32 leading_zero, shift_len, a_exp, b_exp, c_exp;

	a_frac = get_float32_frac(a);
	a_exp = get_float32_exp(a);
	a_sign = get_float32_sign(a);
	b_frac = get_float32_frac(b);
	b_exp = get_float32_exp(b);
	b_sign = get_float32_sign(b);
	a_subnormal = is_float32_subnormal(a);
	b_subnormal = is_float32_subnormal(b);

	c_sign = a_sign ^ b_sign;

	/* if a or b is not a number */
	if (is_float32_nan(a) || is_float32_nan(b))
		return set_float32_nan(b, a, extra_data);

	/* if a and b are both infinite */
	if (is_float32_infinite(a) && is_float32_infinite(b))
		return create_float32(c_sign, 0xFF, 0);

	/* a is infinte */
	if (is_float32_infinite(a)) {
		if (is_float32_zero(b)) {
			/* b is 0, a*b = nan */
			extra_data->exception |= float32_exception_invalid;
			return create_float32(0, 0xFF, 1);
		} else {
			return create_float32(c_sign, 0xFF, 0);
		}
	} else if (is_float32_infinite(b)) {
		/* b is infinite */
		if (is_float32_zero(a)) {
			/* a is 0, a*b = nan */
			extra_data->exception |= float32_exception_invalid;
			return create_float32(0, 0xFF, 1);
		} else {
			return create_float32(c_sign, 0xFF, 0);
		}
	}

	/* if a or b is zero */
	if (is_float32_zero(a) || is_float32_zero(b))
		return create_float32(c_sign, 0, 0);

	if (a_subnormal) {
		/* normalize a at first */
		leading_zero = find_leading_zero(a_frac);
		a_exp = 0 - (leading_zero - (32 - 23));
		a_frac <<= (leading_zero - (32 - 23)) + 1;
	} else {
		a_frac |= 0x800000;
	}
	if (b_subnormal) {
		/* normalize b at first */
		leading_zero = find_leading_zero(b_frac);
		b_exp = 0 - (leading_zero - (32 - 23));
		b_frac <<= (leading_zero - (32 - 23)) + 1;
	} else {
		b_frac |= 0x800000;
	}

	c_frac = (__u64)a_frac * b_frac;
	c_exp = (a_exp - 23) + (b_exp - 23) - 127;

	leading_zero = find_u64_leading_zero(c_frac);
	shift_len = 64 - leading_zero - 24;

	c_exp += shift_len + 23;

	if (c_exp > 0xFE) {
		/* result is too large, return inf */
		extra_data->exception |= float32_exception_overflow;
		extra_data->exception |= float32_exception_inexact;
		return create_float32(c_sign, 0xFF, 0);
	} else if (c_exp >= 1) {
		if (shift_u64_right(c_frac, shift_len, &c_frac, extra_data))
			c_exp += 1;
		c_frac &= 0x7FFFFF;
		return create_float32(c_sign, c_exp, c_frac);
	} else if (c_exp >= -23) {
		/* 0 <= c_exp <= -22, c is subnormal */
		shift_len += 1 - c_exp;
		if (shift_u64_right(c_frac, shift_len, &c_frac, extra_data)) {
			c_exp += 1;
			if (c_exp == 1) {
				/* because of carrier, val is not a subnormal anymore */
				return create_float32(c_sign, 1, 1);
			}
		}
		return create_float32(c_sign, 0, c_frac);
	} else {
		/* smaller than subnormal limitation, return 0 */
		return create_float32(c_sign, 0, 0);
	}
}

float32 float32_div(float32 a, float32 b, struct round_exception *extra_data)
{
	__u64 a_frac, b_frac, c_frac;
	int a_sign, b_sign, c_sign, a_subnormal, b_subnormal;
	__s32 leading_zero, shift_len, a_exp, b_exp, c_exp;

	a_frac = get_float32_frac(a);
	a_exp = get_float32_exp(a);
	a_sign = get_float32_sign(a);
	b_frac = get_float32_frac(b);
	b_exp = get_float32_exp(b);
	b_sign = get_float32_sign(b);
	a_subnormal = is_float32_subnormal(a);
	b_subnormal = is_float32_subnormal(b);

	c_sign = a_sign ^ b_sign;

	/* if a or b is not a number */
	if (is_float32_nan(a) || is_float32_nan(b))
		return set_float32_nan(a, b, extra_data);

	/* if a and b are both infinite  result is nan*/
	if (is_float32_infinite(a) && is_float32_infinite(b))
		return create_float32(0, 0xFF, 1);

	/* infinite / valid num is infinite */
	if (is_float32_infinite(a))
		return create_float32(c_sign, 0xFF, 0);

	/* valid num / infinte = 0 */
	if (is_float32_infinite(b))
		return create_float32(c_sign, 0, 0);

	/* 0 / 0 is nan */
	if (is_float32_zero(a) && is_float32_zero(b)) {
		extra_data->exception |= float32_exception_invalid;
		return create_float32(0, 0xFF, 1);
	}

	/* 0 / valid num is 0 */
	if (is_float32_zero(a))
		return create_float32(c_sign, 0, 0);

	/* valid_num / 0 is infinite */
	if (is_float32_zero(b))
		return create_float32(c_sign, 0xFF, 0);

	/* extend a as much as possible */
	if (!a_subnormal)
		a_frac |= 0x800000;
	else
		a_exp = 1; /* 1 - 127 = -126 */

	leading_zero = find_u64_leading_zero(a_frac);
	a_frac <<= leading_zero;
	a_exp -= leading_zero;

	if (b_subnormal) {
		b_exp = 1; /* 1 - 127 = -126*/
		/* normalize b */
		leading_zero = find_leading_zero(b_frac);
		b_exp -= leading_zero - (32 - 24);
	} else {
		b_frac |= 0x800000;
	}

	leading_zero = find_u64_leading_zero(b_frac);
	b_exp -= (64 - leading_zero - 1);

	c_frac = a_frac / b_frac;
	if ((a_frac % b_frac) > (b_frac/2))
		c_frac += 1;

	c_exp = (a_exp - b_exp) + 127;

	leading_zero = find_u64_leading_zero(c_frac);
	shift_len = 64 - leading_zero - 24;
	c_exp += shift_len;
	if (c_exp > 0xFE) {
		/* result is too large, return inf */
		extra_data->exception |= float32_exception_overflow;
		extra_data->exception |= float32_exception_inexact;
		return create_float32(c_sign, 0xFF, 0);
	} else if (c_exp >= 1) {
		if (shift_u64_right(c_frac, shift_len, &c_frac, extra_data))
			c_exp += 1;
		c_frac &= 0x7FFFFF;
		return create_float32(c_sign, c_exp, c_frac);
	} else if (c_exp >= -23) {
		/* 0 <= c_exp <= -22, c is subnormal */
		shift_len += 1 - c_exp;
		if (shift_u64_right(c_frac, shift_len, &c_frac, extra_data)) {
			c_exp += 1;
			if (c_exp == 1) {
				/* because of carrier, val is not a subnormal anymore */
				return create_float32(c_sign, 1, 1);
			}
		}
		return create_float32(c_sign, 0, c_frac);
	} else {
		/* smaller than subnormal limitation, return 0 */
		return create_float32(c_sign, 0, 0);
	}
}

int float32_eq(float32 a, float32 b, struct round_exception *extra_data)
{
	float32 c;

	if (is_float32_nan(a) || is_float32_nan(b)) {
		extra_data->exception |= float32_exception_invalid;
		return 0;
	}

	c = float32_sub(a, b, extra_data);
	if (is_float32_zero(c))
		return 1;

	return 0;
}

int float32_lt(float32 a, float32 b, struct round_exception *extra_data)
{
	float32 c;
	if (is_float32_nan(a) || is_float32_nan(b)) {
		extra_data->exception |= float32_exception_invalid;
		return 0;
	}

	c = float32_sub(a, b, extra_data);
	if (is_float32_zero(c) || !get_float32_sign(c))
		return 0;

	return 1;
}

int float32_ge(float32 a, float32 b, struct round_exception *extra_data)
{
	return float32_lt(b, a, extra_data);
}

static __u32 to_fixedpoint(float32 a, int is_signed, int int_len, int frac_len,
			 struct round_exception *extra_data)
{
	int sign = get_float32_sign(a);
	__u32 exp, frac, fixedpoint;
	int shift;

	exp = get_float32_exp(a);
	frac = get_float32_frac(a);

	/* input is 0, ignore sign */
	if (exp == 0 && frac == 0)
		return 0;

	/* output is unsigned but input is negative or NaN */
	if ((!is_signed && sign) || is_float32_nan(a)) {
		extra_data->exception |= float32_exception_invalid;
		return 0;
	}

	if (is_float32_subnormal(a) || exp < 127 - (__u32)frac_len) {
		extra_data->exception |= float32_exception_inexact;
		return 0;
	}

	if (is_float32_infinite(a) || exp > 127 + ((__u32)int_len - 1)) {
		extra_data->exception |= float32_exception_overflow;
		fixedpoint = (1 << (int_len + frac_len)) - 1;
		if (sign)
			fixedpoint = ~fixedpoint + 1;
		return fixedpoint;
	}

	fixedpoint = 1 << 23 | frac;
	shift = (exp - 127) + 1 + frac_len - 24;
	if (shift >= 0) {
		fixedpoint <<= shift;
	} else {
		fixedpoint >>= -shift;
		if ((fixedpoint << -shift) != (1 << 23 | frac))
			extra_data->exception |= float32_exception_inexact;
	}

	if (is_signed && sign)
		fixedpoint = ~fixedpoint + 1;

	return fixedpoint;
}

__u32 to_q1_30(float32 a, struct round_exception *extra_data)
{
	return to_fixedpoint(a, 1, 1, 30, extra_data);
}

/************************* MATRIX ***********************************/
void matrix_4X4_to_3X3(struct malidp_matrix4 *a, struct malidp_matrix3 *res)
{
	int i, j, k = 0;
	for (i = 0; i < 4; i++) {
		if (i == 2)
			continue;
		for (j = 0; j < 4; j++) {
			if (j == 2)
				continue;
			res->data[k++] = a->data[i*4 + j];
		}
	}
}

int matrix_mul_4X4(struct malidp_matrix4 *a, struct malidp_matrix4 *b,
		   struct malidp_matrix4 *res)
{
	int i, j, k;
	struct round_exception extra_data = {.exception = 0};

	if (!a || !b || !res)
		return -1;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			res->data[i*4 + j] = 0;
			for (k = 0; k < 4; k++) {
				res->data[i*4 + j] =
					float32_add(res->data[i*4 + j],
						float32_mul(a->data[i*4 + k],
							    b->data[k*4 + j],
							    &extra_data),
						    &extra_data);
				if (extra_data.exception & float32_exception_invalid)
					return -1;
			}
		}
	return 0;
}

static float32 minor_matrix(float32 m1, float32 m2, float32 m3,
			    float32 m4, float32 m5, float32 m6,
			    float32 m7, float32 m8, float32 m9)
{
	float32 res;
	float32 val1, val2, val3,val4, val5, val6;
	struct round_exception extra_data = {.exception = 0};

	val1 = float32_mul(m1, m5, &extra_data);
	val1 = float32_mul(val1, m9, &extra_data);

	val2 = float32_mul(m1, m6, &extra_data);
	val2 = float32_mul(val2, m8, &extra_data);

	val3 = float32_mul(m4, m2, &extra_data);
	val3 = float32_mul(val3, m9, &extra_data);

	val4 = float32_mul(m4, m3, &extra_data);
	val4 = float32_mul(val4, m8, &extra_data);

	val5 = float32_mul(m7, m2, &extra_data);
	val5 = float32_mul(val5, m6, &extra_data);

	val6 = float32_mul(m7, m3, &extra_data);
	val6 = float32_mul(val6, m5, &extra_data);

	res = float32_sub(val1, val2, &extra_data);
	res = float32_sub(res, val3, &extra_data);
	res = float32_add(res, val4, &extra_data);
	res = float32_add(res, val5, &extra_data);
	res = float32_sub(res, val6, &extra_data);

	return res;
}

int inverse_matrix_4X4(struct malidp_matrix4 *m, struct malidp_matrix4 *res)
{
	float32 inv[16], det;
	float32 val1, val2, val3, val4;
	struct round_exception extra_data = {.exception = 0};
	int i;

	if (!m || !res)
		return -1;

	inv[0] = minor_matrix(m->data[5],  m->data[6],  m->data[7],
			      m->data[9],  m->data[10], m->data[11],
			      m->data[13], m->data[14], m->data[15]);

	inv[4] = negative(minor_matrix(m->data[4],  m->data[6],  m->data[7],
				       m->data[8],  m->data[10], m->data[11],
				       m->data[12], m->data[14], m->data[15]));

	inv[8] = minor_matrix(m->data[4], m->data[5], m->data[7],
			      m->data[8], m->data[9], m->data[11],
			      m->data[12], m->data[13], m->data[15]);

	inv[12] = negative(minor_matrix(m->data[4],  m->data[5],  m->data[6],
					m->data[8],  m->data[9],  m->data[10],
					m->data[12], m->data[13], m->data[14]));

	inv[1] = negative(minor_matrix(m->data[1],  m->data[2],  m->data[3],
				       m->data[9],  m->data[10], m->data[11],
				       m->data[13], m->data[14], m->data[15]));

	inv[5] = minor_matrix(m->data[0], m->data[2], m->data[3],
			      m->data[8], m->data[10], m->data[11],
			      m->data[12], m->data[14], m->data[15]);

	inv[9] = negative(minor_matrix(m->data[0],  m->data[1],  m->data[3],
				       m->data[8],  m->data[9],  m->data[11],
				       m->data[12], m->data[13], m->data[15]));

	inv[13] = minor_matrix(m->data[0],  m->data[1],  m->data[2],
			       m->data[8],  m->data[9],  m->data[10],
			       m->data[12], m->data[13], m->data[14]);

	inv[2] = minor_matrix(m->data[1],  m->data[2],  m->data[3],
			      m->data[5],  m->data[6],  m->data[7],
			      m->data[13], m->data[14], m->data[15]);

	inv[6] = negative(minor_matrix(m->data[0],  m->data[2],  m->data[3],
				       m->data[4],  m->data[6],  m->data[7],
				       m->data[12], m->data[14], m->data[15]));

	inv[10] = minor_matrix(m->data[0],  m->data[1],  m->data[3],
			       m->data[4],  m->data[5],  m->data[7],
			       m->data[12], m->data[13], m->data[15]);

	inv[14] = negative(minor_matrix(m->data[0],  m->data[1],  m->data[2],
					m->data[4],  m->data[5],  m->data[6],
					m->data[12], m->data[13], m->data[14]));

	inv[3] = negative(minor_matrix(m->data[1], m->data[2],  m->data[3],
				       m->data[5], m->data[6],  m->data[7],
				       m->data[9], m->data[10], m->data[11]));

	inv[7] = minor_matrix(m->data[0], m->data[2],  m->data[3],
			      m->data[4], m->data[6],  m->data[7],
			      m->data[8], m->data[10], m->data[11]);

	inv[11] = negative(minor_matrix(m->data[0], m->data[1], m->data[3],
					m->data[4], m->data[5], m->data[7],
					m->data[8], m->data[9], m->data[11]));

	inv[15] = minor_matrix(m->data[0], m->data[1], m->data[2],
			       m->data[4], m->data[5], m->data[6],
			       m->data[8], m->data[9], m->data[10]);
	/* det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12] */
	val1 = float32_mul(m->data[0], inv[0], &extra_data);
	val2 = float32_mul(m->data[1], inv[4], &extra_data);
	val3 = float32_mul(m->data[2], inv[8], &extra_data);
	val4 = float32_mul(m->data[3], inv[12], &extra_data);
	det = float32_add(val1, val2, &extra_data);
	det = float32_add(det, val3, &extra_data);
	det = float32_add(det, val4, &extra_data);

	if (float32_lt(float32_abs(det), FLOAT32_EPSILON, &extra_data))
		return -1;
	/* det = 1.0 / det */
	det = float32_div(FLOAT32_1, det, &extra_data);

	for (i = 0; i < 16; i++)
		res->data[i] = float32_mul(inv[i], det, &extra_data);
	return 0;
}

int normalize_matrix3(struct malidp_matrix3 *m,
			struct round_exception *extra_data)
{
	float32 max_num = 0;
	int i;

	extra_data->exception = 0;
	for (i = 0; i < 9; i++) {
		if (float32_ge(float32_abs(m->data[i]), max_num, extra_data))
			max_num = float32_abs(m->data[i]);
		extra_data->exception &= ~float32_exception_inexact;
		if (extra_data->exception)
			return -1;
	}

	if (is_float32_zero(max_num))
		return -1;

	for (i = 0; i < 9; i++) {
		m->data[i] = float32_div(m->data[i], max_num, extra_data);
		extra_data->exception &= ~float32_exception_inexact;
		if (extra_data->exception)
			return -1;
	}
	return 0;
}

void identity_matrix4(struct malidp_matrix4 *mat)
{
	int i, j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			mat->data[i * 4 + j] = (i == j) ? FLOAT32_1 : FLOAT32_0;
	}
}

void identity_matrix3(struct malidp_matrix3 *mat)
{
	int i, j;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			mat->data[i * 3 + j] = (i == j) ? FLOAT32_1 : FLOAT32_0;
	}
}

void quaternion_to_matrix(struct malidp_quaternion *q,
			  struct malidp_matrix4 *res)
{
	struct round_exception extra_data = {.exception = 0};
	float32 square_w, square_x, square_y, square_z;
	float32 xy, zw, xz, yw, yz, xw;

	if (!res)
		return;
	square_w = float32_mul(q->w, q->w, &extra_data);
	square_x = float32_mul(q->x, q->x, &extra_data);
	square_y = float32_mul(q->y, q->y, &extra_data);
	square_z = float32_mul(q->z, q->z, &extra_data);
	xy = float32_mul(q->x, q->y, &extra_data);
	zw = float32_mul(q->z, q->w, &extra_data);
	xz = float32_mul(q->x, q->z, &extra_data);
	yw = float32_mul(q->y, q->w, &extra_data);
	yz = float32_mul(q->y, q->z, &extra_data);
	xw = float32_mul(q->x, q->w, &extra_data);

	/* w2 + x2 - y2 - z2 */
	res->data[0] = float32_add(square_w, square_x, &extra_data);
	res->data[0] = float32_sub(res->data[0], square_y, &extra_data);
	res->data[0] = float32_sub(res->data[0], square_z, &extra_data);

	/* 2*xy - 2*zw */
	res->data[1] = float32_sub(xy, zw, &extra_data);
	res->data[1] = float32_mul(res->data[1], FLOAT32_2, &extra_data);

	/* 2*xz + 2*yw */
	res->data[2] = float32_add(xz, yw, &extra_data);
	res->data[2] = float32_mul(res->data[2], FLOAT32_2, &extra_data);

	res->data[3] = 0;

	/* 2*xy + 2*zw */
	res->data[4] = float32_add(xy, zw, &extra_data);
	res->data[4] = float32_mul(res->data[4], FLOAT32_2, &extra_data);

	/* w2 - x2 + y2 - z2 */
	res->data[5] = float32_sub(square_w, square_x, &extra_data);
	res->data[5] = float32_add(res->data[5], square_y, &extra_data);
	res->data[5] = float32_sub(res->data[5], square_z, &extra_data);

	/* 2*yz - 2*xw */
	res->data[6] = float32_sub(yz, xw, &extra_data);
	res->data[6] = float32_mul(res->data[6], FLOAT32_2, &extra_data);

	res->data[7] = 0;

	/* 2*xz - 2*yw */
	res->data[8] = float32_sub(xz, yw, &extra_data);
	res->data[8] = float32_mul(res->data[8], FLOAT32_2, &extra_data);

	/* 2*yz + 2*xw */
	res->data[9] = float32_add(yz, xw, &extra_data);
	res->data[9] = float32_mul(res->data[9], FLOAT32_2, &extra_data);

	/* w2 - x2 - y2 + z2 */
	res->data[10] = float32_sub(square_w, square_x, &extra_data);
	res->data[10] = float32_sub(res->data[10], square_y, &extra_data);
	res->data[10] = float32_add(res->data[10], square_z, &extra_data);

	res->data[11] = 0;
	res->data[12] = 0;
	res->data[13] = 0;
	res->data[14] = 0;

	/* w2 + x2 + y2 + z2 */
	res->data[15] = float32_add(square_w, square_x, &extra_data);
	res->data[15] = float32_add(res->data[15], square_y, &extra_data);
	res->data[15] = float32_add(res->data[15], square_z, &extra_data);
}

void inverse_quaternion(struct malidp_quaternion *q,
			struct malidp_quaternion *inv_q)
{
	if (!q || !inv_q)
		return;

	/* q should be unit quaternion, so inv_q = q_star */
	inv_q->x = negative(q->x);
	inv_q->y = negative(q->y);
	inv_q->z = negative(q->z);
	inv_q->w = q->w;
}

static int zero_matrix(struct malidp_matrix4 *mat)
{
	int i;

	if (!mat)
		return -1;

	for (i = 0; i < 16; i++)
		mat->data[i] = FLOAT32_0;

	return 0;

}

static int lower_half_identity_matrix(struct malidp_matrix4 *mat)
{
	int i, j;

	if (!mat)
		return -1;

	for(i = 2; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			if (i == j)
				mat->data[i * 4 + j] = FLOAT32_1;
			else
				mat->data[i * 4 + j] = FLOAT32_0;
		}
	}
	return 0;
}

static float32 vec3_dot(struct malidp_position *v1, struct malidp_position *v2,
			struct round_exception *extra_data)
{
	float32 tmp_x, tmp_y, tmp_z;

	tmp_x = float32_mul(v1->x, v2->x, extra_data);
	tmp_y = float32_mul(v1->y, v2->y, extra_data);
	tmp_z = float32_mul(v1->z, v2->z, extra_data);
	tmp_x = float32_add(tmp_x, tmp_y, extra_data);
	tmp_x = float32_add(tmp_x, tmp_z, extra_data);

	return tmp_x;
}

/* Here we calculate M * H * inv(H'). */
int update_projection_layer_transform_matrix(struct malidp_matrix4 *render_mat,
					     struct malidp_matrix4 *helmet_mat,
					     struct malidp_matrix4 *ref_mat,
					     struct malidp_matrix4 *inv_cur_mat,
					     struct malidp_matrix4 *output_mat)
{
	struct malidp_matrix4 inv_helmet_mat, pose_delta_mat;
	struct malidp_matrix4 m_delta;
	int ret;

	ret = inverse_matrix_4X4(helmet_mat, &inv_helmet_mat);
	if (ret)
		return ret;

	/* calculate pose delta of orientation here by reference_ori_matrix * inv(current_ori_matrix). */
	matrix_mul_4X4(ref_mat, inv_cur_mat, &pose_delta_mat);
	/* projection matrix contains the matrix M. here we calculate M * pose_delata. */
	matrix_mul_4X4(render_mat, &pose_delta_mat, &m_delta);
	/* projection matrix contains the matrix M. here we calculate M * pose_delata * inv(M'). */
	matrix_mul_4X4(&m_delta, &inv_helmet_mat, output_mat);

	return 0;
}

static int get_quad_layer_view_matrix(struct malidp_position *cur_pos,
				      struct malidp_matrix4  *inv_cur_mat,
				      struct malidp_position *ref_pos,
				      struct malidp_matrix4  *ref_mat,
				      struct malidp_matrix4  *view_mat,
				      struct round_exception *extra_data)
{
	struct malidp_matrix4 C, R, T, tmp_mat;
	struct malidp_position position_delta;

	identity_matrix4(&C);
	identity_matrix4(&R);
	identity_matrix4(&T);
	C.data[5] = FLOAT32_NEG_1;

	matrix_mul_4X4(ref_mat, inv_cur_mat, &R);

	position_delta.x = float32_sub(cur_pos->x, ref_pos->x, extra_data);
	position_delta.y = float32_sub(cur_pos->y, ref_pos->y, extra_data);
	position_delta.z = float32_sub(cur_pos->z, ref_pos->z, extra_data);

	T.data[3]  = float32_sub(T.data[3],  position_delta.x, extra_data);
	T.data[7]  = float32_sub(T.data[7],  position_delta.y, extra_data);
	T.data[11] = float32_sub(T.data[11], position_delta.z, extra_data);

	matrix_mul_4X4(&R, &T, &tmp_mat);
	matrix_mul_4X4(&C, &tmp_mat, view_mat);
	return 0;
}

static void
make_display_to_plane_projection(struct malidp_matrix4 *view_from_ndc,
				 struct malidp_matrix4 *projection_mat)
{
	struct malidp_quaternion u, v, n, q, z;
	float32 xx, yy, zz, xy, xz, yx, yz, zx, zy, tmp[5];
	struct round_exception extra_data;

	u.x = view_from_ndc->data[0];
	u.y = view_from_ndc->data[4];
	u.z = view_from_ndc->data[8];
	u.w = view_from_ndc->data[12];

	v.x = view_from_ndc->data[1];
	v.y = view_from_ndc->data[5];
	v.z = view_from_ndc->data[9];
	v.w = view_from_ndc->data[13];

	n.x = view_from_ndc->data[2];
	n.y = view_from_ndc->data[6];
	n.z = view_from_ndc->data[10];
	n.w = view_from_ndc->data[14];

	q.x = view_from_ndc->data[3];
	q.y = view_from_ndc->data[7];
	q.z = view_from_ndc->data[11];
	q.w = view_from_ndc->data[15];

	zero_matrix(projection_mat);

	xx = float32_mul(n.x, q.x, &extra_data);
	yy = float32_mul(n.y, q.y, &extra_data);
	zz = float32_mul(n.z, q.z, &extra_data);

	xy = float32_mul(n.x, q.y, &extra_data);
	xz = float32_mul(n.x, q.z, &extra_data);

	yx = float32_mul(n.y, q.x, &extra_data);
	yz = float32_mul(n.y, q.z, &extra_data);

	zx = float32_mul(n.z, q.x, &extra_data);
	zy = float32_mul(n.z, q.y, &extra_data);

	/************* row 0 ********************/
	/* -n.x*q.z*u.z - n.x*q.y*u.y + n.z*q.z*u.x + n.y*q.y*u.x; */
	tmp[0] = float32_mul(xz, u.z, &extra_data);
	tmp[1] = float32_mul(xy, u.y, &extra_data);
	tmp[2] = float32_mul(zz, u.x, &extra_data);
	tmp[3] = float32_mul(yy, u.x, &extra_data);
	tmp[4] = float32_add(tmp[3], tmp[2], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[1], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[0], &extra_data);
	projection_mat->data[0] = tmp[4];

	/* -n.y*q.z*u.z + n.z*q.z*u.y + n.x*q.x*u.y - n.y*q.x*u.x; */
	tmp[0] = float32_mul(yz, u.z, &extra_data);
	tmp[1] = float32_mul(zz, u.y, &extra_data);
	tmp[2] = float32_mul(xx, u.y, &extra_data);
	tmp[3] = float32_mul(yx, u.x, &extra_data);
	tmp[4] = float32_add(tmp[1], tmp[2], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[0], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[3], &extra_data);
	projection_mat->data[1] = tmp[4];

	/* n.y*q.y*u.z + n.x*q.x*u.z - n.z*q.y*u.y - n.z*q.x*u.x; */
	tmp[0] = float32_mul(yy, u.z, &extra_data);
	tmp[1] = float32_mul(xx, u.z, &extra_data);
	tmp[2] = float32_mul(zy, u.y, &extra_data);
	tmp[3] = float32_mul(zx, u.x, &extra_data);
	tmp[4] = float32_add(tmp[0], tmp[1], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[2], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[3], &extra_data);
	projection_mat->data[2] = tmp[4];

	/************* row 1 ********************/
	/* -n.x*q.z*v.z - n.x*q.y*v.y + n.z*q.z*v.x + n.y*q.y*v.x; */
	tmp[0] = float32_mul(xz, v.z, &extra_data);
	tmp[1] = float32_mul(xy, v.y, &extra_data);
	tmp[2] = float32_mul(zz, v.x, &extra_data);
	tmp[3] = float32_mul(yy, v.x, &extra_data);
	tmp[4] = float32_add(tmp[3], tmp[2], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[1], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[0], &extra_data);
	projection_mat->data[4] = tmp[4];

	/* -n.y*q.z*v.z + n.z*q.z*v.y + n.x*q.x*v.y - n.y*q.x*v.x; */
	tmp[0] = float32_mul(yz, v.z, &extra_data);
	tmp[1] = float32_mul(zz, v.y, &extra_data);
	tmp[2] = float32_mul(xx, v.y, &extra_data);
	tmp[3] = float32_mul(yx, v.x, &extra_data);
	tmp[4] = float32_add(tmp[1], tmp[2], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[0], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[3], &extra_data);
	projection_mat->data[5] = tmp[4];

	/* n.y*q.y*v.z + n.x*q.x*v.z - n.z*q.y*v.y - n.z*q.x*v.x; */
	tmp[0] = float32_mul(yy, v.z, &extra_data);
	tmp[1] = float32_mul(xx, v.z, &extra_data);
	tmp[2] = float32_mul(zy, v.y, &extra_data);
	tmp[3] = float32_mul(zx, v.x, &extra_data);
	tmp[4] = float32_add(tmp[0], tmp[1], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[2], &extra_data);
	tmp[4] = float32_sub(tmp[4], tmp[3], &extra_data);
	projection_mat->data[6] = tmp[4];

	projection_mat->data[12] = n.x;
	projection_mat->data[13] = n.y;
	projection_mat->data[14] = n.z;

	z.x = float32_sub(FLOAT32_0, projection_mat->data[2], &extra_data);
	z.y = float32_sub(FLOAT32_0, projection_mat->data[6], &extra_data);
	z.z = float32_sub(FLOAT32_0, projection_mat->data[10], &extra_data);
	z.w = float32_sub(FLOAT32_0, projection_mat->data[14], &extra_data);

	projection_mat->data[2]  = projection_mat->data[3];
	projection_mat->data[6]  = projection_mat->data[7];
	projection_mat->data[10] = projection_mat->data[11];
	projection_mat->data[14] = projection_mat->data[15];

	projection_mat->data[3]  = z.x;
	projection_mat->data[7]  = z.y;
	projection_mat->data[11] = z.z;
	projection_mat->data[15] = z.w;
}

/* Using Sam Martin's function PrepareQuadLayerMatrices to update projection matrix for quad layer */
int update_quad_layer_transform_matrix(struct malidp_position *cur_pos,
			struct malidp_matrix4  *inv_cur_mat,
			struct malidp_position *ref_pos,
			struct malidp_matrix4  *ref_mat,
			struct malidp_matrix4  *render_mat,
			struct malidp_matrix4  *helmet_mat,
			struct malidp_matrix4  *output_mat)
{
	struct malidp_matrix4 quad_view_world_scale;
	struct malidp_matrix4 display_to_plane_projection;
	struct malidp_matrix4 inv_helmet_projection;
	struct malidp_matrix4 quad_view;
	struct malidp_position quad_normal, quad_pos;
	struct round_exception extra_data;
	float32 result;
	int facing_camera = 1;

	get_quad_layer_view_matrix(cur_pos, inv_cur_mat, ref_pos, ref_mat, &quad_view, &extra_data);

	matrix_mul_4X4(&quad_view, render_mat, &quad_view_world_scale);
	/* quad surface normal in view space. */
	quad_normal.x = quad_view_world_scale.data[2];
	quad_normal.y = quad_view_world_scale.data[6];
	quad_normal.z = quad_view_world_scale.data[10];

	/* quad origin position in view space. */
	quad_pos.x = quad_view_world_scale.data[3];
	quad_pos.y = quad_view_world_scale.data[7];
	quad_pos.z = quad_view_world_scale.data[11];

	/* detect if the quad layer facing away from the camera, if it is,
	 *  we can clip the entire quad layer. */
	result = vec3_dot(&quad_normal, &quad_pos, &extra_data);
	facing_camera = float32_ge(result, FLOAT32_0, &extra_data);

	if (facing_camera) {
		/* A short explanation from reprojection_model project:
		 * We first construct a matrix that describes the quad plane in view space (so camera at the origin looking down
 		 * negative z). The quad can be scaled and positioned arbitrarily. The APIs allow you to specify this per-eye
 		 * (although that doesn't make much sense tbh). Once we have the quad in view space, we construct a homography
 		 * that maps from display NDC (x,y = -1..1, w=1) to the layer image, with -1..1 containing the image data. This
		 * mapping produces the same effect as tracing a view ray from each pixel on the display and intersecting it
		 * with the quad plane. This mapping is all done in MakeDisplayToPlaneProjection and you will need to consult
		 * other documentation (slides or maxima description) for how it works. Once we have this, we do not need the
		 * matrix sandwich. The calling code will insert the identity matrix for the rotational delta. The only thing we
		 * still need to correct for is the display fov and aspect ratio, which is handled by
		 * inv_helmet_projection. */

		make_display_to_plane_projection(&quad_view_world_scale, &display_to_plane_projection);

		if (inverse_matrix_4X4(helmet_mat, &inv_helmet_projection))
		{
			return -1;
		}

		lower_half_identity_matrix(&inv_helmet_projection);

		matrix_mul_4X4(&display_to_plane_projection, &inv_helmet_projection, output_mat);
	} else {
		zero_matrix(output_mat);
	}
	return 0;
}
