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
