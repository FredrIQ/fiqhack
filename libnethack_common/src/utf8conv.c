/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-10 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "utf8conv.h"
#include <limits.h>

#define ERRVAL ULONG_MAX

unsigned long
utf8towc(const char *in)
{
    int fc = *(unsigned char *)in;

    if (fc <= 0x7f)
        return fc;

    int len = 0;
    int fcp = 0x80;
    while (fcp & fc) {
        len++;
        fcp >>= 1;
    }
    if (len > 6)
        return ERRVAL;

    unsigned long rv = fc & (fcp - 1);
    int i;
    for (i = 1; i < len; i++) {
        if ((((unsigned char *)in)[i] & 0xc0) != 0x80)
            return ERRVAL;

        rv <<= 6;
        rv += ((unsigned char *)in)[i] & 0x3f;
    }

    return rv;
}

void wctoutf8(unsigned long in, char out[static 7])
{
    if (in < 0x7f) {
        out[0] = (unsigned char)in;
        out[1] = 0;
        return;
    }

    int len;
    if (in <= 0x7ff)
        len = 2;
    else if (in <= 0xffff)
        len = 3;
    else if (in <= 0x1fffff)
        len = 4;
    else if (in <= 0x3ffffff)
        len = 5;
    else if (in <= 0x7fffffff)
        len = 6;
    else {
        /* error: out of range */
        out[0] = 0;
        return;
    }

    int out0 = 0x80;
    out[len] = '\0';
    int i;
    for (i = len - 1; i >= 1; i--) {
        out[i] = 0x80 | (in & 0x3f);
        in >>= 6;
        out0 = 0x80 | (out0 >> 1);
    }
    out[0] = (char)(unsigned char)(out0 | in);
}


