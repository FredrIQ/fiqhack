/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-23 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "compilers.h"
#include <stdbool.h>

extern void init_test_system(unsigned long long, const char[static 4], int);
extern void shutdown_test_system(void);
extern void play_test_game(const char *, bool);
extern void skip_test_game(const char *, bool);
