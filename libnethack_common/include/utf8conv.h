/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-18 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef COMMON_UTF8CONV_H
# define COMMON_UTF8CONV_H

# ifdef IN_LIBNETHACK_COMMON
#  define EXPORT(x) x
# else
#  define EXPORT(x) x
# endif

# include "nethack_types.h"
# include <stdlib.h>

extern unsigned long decode_one_utf8_character(const char **);
extern unsigned long EXPORT(utf8towc)(const char *);
extern void EXPORT(wctoutf8)(unsigned long, char [static 7]);
extern size_t EXPORT(utf8_mbstowcs)(wchar_t *, const char *, size_t);
extern int EXPORT(utf8_wcswidth)(const char *, size_t);

#endif
