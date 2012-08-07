/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *      This code was adapted from the code in end.c to run in a standalone
 *      mode for the makedefs / drg code.
 */

#include "config.h"

/*VARARGS1*/
boolean panicking;
void NORETURN panic(const char *, ...);

void
panic(const char *str, ...)
{
    va_list the_args;

    va_start(the_args, str);
    if (panicking++)
        abort();        /* avoid loops - this should never happen */

    fputs(" ERROR:  ", stderr);
    vfprintf(stderr, str, the_args);
    fflush(stderr);

    va_end(the_args);
    exit(EXIT_FAILURE); /* redundant */
}

/*panic.c*/
