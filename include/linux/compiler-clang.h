/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPILER_TYPES_H
#error "Please don't include <linux/compiler-clang.h> directly, include <linux/compiler.h> instead."
#endif

/* Some compiler specific definitions are overwritten here
 * for Clang compiler
 */

#ifdef uninitialized_var
#undef uninitialized_var
#define uninitialized_var(x) x = *(&(x))
#endif

/* same as gcc, this was present in clang-2.6 so we can assume it works
 * with any version that can compile the kernel
 */
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

/* Clang doesn't have a way to turn it off per-function, yet. */
#ifdef __noretpoline
#undef __noretpoline
#endif

/*
 * Not all versions of clang implement the the type-generic versions
 * of the builtin overflow checkers. Fortunately, clang implements
 * __has_builtin allowing us to avoid awkward version
 * checks. Unfortunately, we don't know which version of gcc clang
 * pretends to be, so the macro may or may not be defined.
 */
#undef COMPILER_HAS_GENERIC_BUILTIN_OVERFLOW
#if __has_builtin(__builtin_mul_overflow) && \
    __has_builtin(__builtin_add_overflow) && \
    __has_builtin(__builtin_sub_overflow)
#define COMPILER_HAS_GENERIC_BUILTIN_OVERFLOW 1
#endif

#ifdef CONFIG_LTO_CLANG
#ifdef CONFIG_FTRACE_MCOUNT_RECORD
#define __norecordmcount \
	__attribute__((__section__(".text..ftrace")))
#define __nocfi		__attribute__((no_sanitize("cfi")))
#endif
