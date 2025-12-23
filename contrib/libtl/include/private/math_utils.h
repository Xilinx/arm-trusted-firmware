/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>

/**
 * @brief Rounds up a given value to the nearest multiple of a boundary.
 *
 * @param value The value to round up.
 * @param boundary The alignment boundary (must be a power of two).
 * @return The smallest multiple of `boundary` that is greater than or equal to `value`.
 */
static inline uintptr_t libtl_align_up(uintptr_t value, uintptr_t boundary)
{
	return (value + boundary - 1) & ~(boundary - 1);
}

/**
 * @brief Checks whether `value` is aligned to the specified `boundary`.
 *
 * @param value The value to check.
 * @param boundary The alignment boundary (should be a power of two for efficiency).
 * @return `1` if `value` is aligned, `0` otherwise.
 */
static inline int libtl_is_aligned(uintptr_t value, uintptr_t boundary)
{
	return (value % boundary) == 0;
}

/**
 * @brief Safely adds `a` and `b`, detecting overflow.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param res Pointer to store the result if no overflow occurs.
 * @return `1` if overflow occurs, `0` otherwise.
 */
#define libtl_add_overflow(a, b, res) __builtin_add_overflow((a), (b), (res))

/**
 * @brief Rounds up `v` to the nearest multiple of `size`, detecting overflow.
 *
 * @param v The value to round up.
 * @param size The alignment boundary (must be a power of two).
 * @param res Pointer to store the rounded-up result.
 * @return `1` if an overflow occurs, `0` otherwise.
 */
#define libtl_round_up_overflow(v, size, res)                               \
	(__extension__({                                                    \
		typeof(res) __res = res;                                    \
		typeof(*(__res)) __roundup_tmp = 0;                         \
		typeof(v) __roundup_mask = (typeof(v))(size) - 1;           \
                                                                            \
		libtl_add_overflow((v), __roundup_mask, &__roundup_tmp) ?   \
			1 :                                                 \
			(void)(*(__res) = __roundup_tmp & ~__roundup_mask), \
			0;                                                  \
	}))

/**
  * @brief Adds `a` and `b`, then rounds up the result to the nearest multiple
  * of `size`, detecting overflow.
  *
  * @param a First operand for addition.
  * @param b Second operand for addition.
  * @param size The alignment boundary (must be positive).
  * @param res Pointer to store the final rounded result.
  * @return `1` if an overflow occurs during addition or rounding, `0` otherwise.
  */
#define libtl_add_with_round_up_overflow(a, b, size, res)               \
	(__extension__({                                                \
		typeof(a) __a = (a);                                    \
		typeof(__a) __add_res = 0;                              \
                                                                        \
		libtl_add_overflow((__a), (b), &__add_res)	  ? 1 : \
		libtl_round_up_overflow(__add_res, (size), (res)) ? 1 : \
								    0;  \
	}))

#endif /* MATH_UTILS_H */
