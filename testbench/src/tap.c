/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-23 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "tap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* Called when we start to test. (No special notation is needed for the end of
   the test.) */
void
tap_init(int testcount)
{
    printf("1..%d\n", testcount);
}

/* Outputs information to the test log. Typically used to report what went
   wrong, in the case that something goes wrong; but comments are not test
   failures in of themselves (so you could also report a random seed, etc.).

   Do not append a newline to the message. You can use printf formatting for
   it. */
void
tap_comment(const char *message, ...)
{
    va_list v;
    va_start(v, message);
    printf("# ");
    vprintf(message, v);
    printf("\n");
    va_end(v);
}

/* Called when a test passes or fails. The test name may not contain '#' or
   vertical whitespace; you can use printf formatting for it. "testnumber" is a
   pointer to an integer holding TAP's internal state; it should be initialized
   to 1, and thereafter not touched (you can look at it if you like though; it
   holds the number of the next test to run). */
void
tap_test(int *testnumber, bool passed, const char *name, ...)
{
    va_list v;
    va_start(v, name);
    printf("%s %d - ", passed ? "ok" : "not ok", (*testnumber)++);
    vprintf(name, v);
    printf("\n");
    va_end(v);
}

/* Called to skip a test. */
void
tap_skip(int *testnumber, const char *name, ...)
{
    va_list v;
    va_start(v, name);
    printf("ok %d - ", (*testnumber)++);
    vprintf(name, v);
    printf(" # skip\n");
    va_end(v);
}

/* Called when something goes badly wrong. Exits the program. Do not append a
   newline to the message. */
noreturn void
tap_bail(const char *message)
{
    printf("Bail out! %s\n", message);
    abort();
}

/* Like tap_bail, but also includes the contents of errno in the error. */
noreturn void
tap_bail_errno(const char *message)
{
    printf("Bail out! %s: %s\n", message, strerror(errno));
    abort();
}
