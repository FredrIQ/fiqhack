/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-18 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "utf8conv.h"
#include <limits.h>
#include <inttypes.h>

#define ERRVAL ULONG_MAX

/* Returns the integer representing the first character of a UTF-8 string, and
   repositions that string to point at its second character (i.e. "removes the
   first character" of the string).

   Returns ULONG_MAX on error. */
unsigned long
decode_one_utf8_character(const char **in)
{
    int fc = (unsigned char)**in;
    (*in)++;

    if (fc <= 0x7f)
        return fc;

    int len = 0;
    int fcp = 0x80;
    while (fcp & fc) {
        len++;
        fcp >>= 1;
    }
    if (len < 2 || len > 6)
        return ERRVAL;

    unsigned long rv = fc & (fcp - 1);
    int i;
    for (i = 1; i < len; i++) {
        if ((((unsigned char) **in) & 0xc0) != 0x80)
            return ERRVAL;

        rv <<= 6;
        rv += ((unsigned char) **in) & 0x3f;
        (*in)++;
    }

    /* These aren't valid Unicode codepoints. */
    if ((rv >= 0xd800U && rv <= 0xdfffU) ||
        (rv >= 0xfffeU && rv <= 0xffffU) ||
        rv > 0x10ffffLU)
        return ERRVAL;

    return rv;
}

/* Convert a UTF-8 string representing a single codepoint to that codepoint.
   Returns ULONG_MAX on error. */
unsigned long
utf8towc(const char *in)
{
    return decode_one_utf8_character(&in);
}

/* The reverse operation: produce a string encoding a single UTF-8 codepoint. */
void
wctoutf8(unsigned long in, char out[static 7])
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

/* Like mbstowcs, but always uses UTF-8, rather than being locale-dependent.

   Specifically: returns the length in codepoints of mbs, or (size_t)-1 if mbs
   is invalid UTF-8 (note that (size_t)-1 is positive, so test the result
   against it for equality). If wcs is non-NULL, also stores up to wcs_allocsize
   - 1 wide characters from the start of the string in wcs, plus a wide NUL (the
   contents of wcs become undefined on error).

   In the case of astral-plane input on an OS that can't handle it (*glares at
   Windows*), currently substitutes REPLACEMENT CHARACTER. It's worth
   considering encoding the output as UTF-16 instead, which would remove one set
   of problems and introduce another set of problems. Given that we can't really
   rely on the downstream terminal handling non-BMP characters correctly (and
   libuncursed faketerm doesn't have those characters in its font, so wouldn't
   be able to render them for that reason).

   TODO: Currently doesn't check to ensure that the input UTF-8 is as short as
   possible. */
size_t
utf8_mbstowcs(wchar_t *wcs, const char *mbs, size_t wcs_allocsize)
{
    size_t outlen = 0;
    const char *p = mbs;

    while (*p) {
        unsigned long c = decode_one_utf8_character(&p);

        if (c == ERRVAL)
            return (size_t) -1;

        /* Replace characters outside the wchar_t range. */
        if (c > WCHAR_MAX)
            c = 0xfffdU;

        /* Add the character to the output, if we have one and it fits. */
        if (wcs && outlen < wcs_allocsize - 1)
            wcs[outlen] = (wchar_t) c;

        outlen++;
    }

    if (wcs && outlen < wcs_allocsize)
        wcs[outlen] = L'\0';
    else if (wcs)
        wcs[wcs_allocsize - 1] = L'\0';

    return outlen;
}

/* Determines the number of columns onscreen that the given string takes to
   render. (Wow, the API for this is weird; it's chosen to match wcswidth, but
   wcswidth is bizarre.)

   TODO: Currently mostly unimplemented (it supports ordinary width-1 characters
   only). */
int
utf8_wcswidth(const char *wcs, size_t maxreturn)
{
    size_t length = utf8_mbstowcs(NULL, wcs, -1);

    if (length == (size_t) -1)
        return -1;
    else if (length > maxreturn)
        return maxreturn;
    else
        return length;
}
