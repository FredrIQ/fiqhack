/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-23 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "compilers.h"
#include <stdbool.h>

extern void tap_init(int);
extern void tap_comment(const char *, ...) PRINTFLIKE(1, 2);
extern void tap_test(int *, bool, const char *, ...) PRINTFLIKE(3, 4);
extern void tap_skip(int *, const char *, ...) PRINTFLIKE(2, 3);
extern noreturn void tap_bail(const char *);
extern noreturn void tap_bail_errno(const char *);
