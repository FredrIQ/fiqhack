/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-18 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) Alex Smith 2014. */
/* NetHack may be freely redistributed.  See license for details. */

/* This file contains compatibility macros that translate between compilers. */

# include <assert.h>
# include <setjmp.h> /* must be included before we redefine noreturn on mingw */

/* C11 compatibility. */

/* <assert.h> should define static_assert in C11. If it doesn't (say due to a
   pre-C11 compiler), we can construct a static assert on our own like this: */
# ifndef static_assert
#  define static_assert(cond, msg) \
    extern const int static_assertion_##__LINE__[cond ? 1 : -1]
# endif

/* noreturn is defined in stdnoreturn.h, but that's not available on pre-C11
   systems.  So instead, we ask aimake. */
# define noreturn AIMAKE_NORETURN

# undef PURE /* collision with mingw headers */

/* Optimization and warning markers. noreturn is standard, but there are a bunch
   of useful nonstandard markers too (unlike the standard "noreturn", these are
   allcaps): */
# ifndef __GNUC__
/*
 * These macros indicate functions that have no side effects:
 *
 * - USE_RETVAL indicates that the function may change global memory in minor
 *   ways (e.g. allocating onto an xmalloc chain), but has no useful effect
 *   apart from calculating the return values. It can thus be seen as meaning
 *   "this function may be implemented in an impure way, but is pure from the
 *   point of view of the caller".
 *
 * - PURE means that the function is effectively an accessor function; it can
 *   look at globals and any storage it's given access to via pointer arguments,
 *   but cannot modify memory at all (besides its own locals).
 *
 * - VERYPURE is a stronger PURE, and means that the function is effectively
 *   arithmetic-only (typically a lookup table); it must always produce the same
 *   result with the same arguments, even if those arguments are pointers and
 *   the memory they point to has changed.  In other words, it does not access
 *   any storage at all (apart from constants and its own locals).
 *
 * Additionally, functions marked PURE or VERYPURE must return normally (no
 * infinite loops, longjmps, etc.). In particular, they must not be able to
 * panic.
 */
#  define USE_RETVAL
#  define PURE
#  define VERYPURE

/* These macros indicate that a function uses the same varargs convention as
   printf, scanf, or strftime respectively; "f" is the index of the format
   string, and "a" is the index of the ... argument, or 0 if the function
   takes a va_list instead.

   Using them when appropriate means that compilers that understand them will
   warn about mistakes in the format string (and can also help catch common
   mistakes like adding an extra argument to msgprintf; it has one fewer
   argument than the similar sprintf does). */
#  define PRINTFLIKE(f,a)
#  define SCANFLIKE(f,a)
#  define STRFTIMELIKE(f,a)
/* This macro indicates that the function requires a sentinel on the arguments;
 * that is, the last variadic argument must be a null pointer.
 */
#  define SENTINEL
/* This macro indicates that an enumerator is intended to be used as a flag
 * type.
 */
#  define FLAG_ENUM
# else
#  define USE_RETVAL __attribute__((warn_unused_result))
#  define PURE __attribute__((pure))
#  define VERYPURE __attribute__((const))
#  ifdef AIMAKE_BUILDOS_MSWin32
#   define PRINTFLIKE(f,a) __attribute__((format (ms_printf, f, a)))
#   define SCANFLIKE(f,a) __attribute__((format (ms_scanf, f, a)))
#   define STRFTIMELIKE(f,a) __attribute__((format (strftime, f, a)))
#  else
#   define PRINTFLIKE(f,a) __attribute__((format (printf, f, a)))
#   define SCANFLIKE(f,a) __attribute__((format (scanf, f, a)))
#   define STRFTIMELIKE(f,a) __attribute__((format (strftime, f, a)))
#  endif
#  define SENTINEL __attribute__((sentinel))
#  if defined(__clang__)
#   if __has_attribute(flag_enum)
#    define FLAG_ENUM __attribute__((flag_enum))
#   else
#    define FLAG_ENUM
#   endif
#  else
#   define FLAG_ENUM
#  endif
# endif

/* MemorySanitizer stuff */
#ifdef __clang__
# if __has_feature(memory_sanitizer)
#  include <sanitizer/msan_interface.h>
#  define MARK_INITIALIZED(mem, size) __msan_unpoison(mem, size)
# else
#  define MARK_INITIALIZED(mem, size)
# endif
#else
# define MARK_INITIALIZED(mem, size)
#endif
