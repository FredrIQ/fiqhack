/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-21 */
/* Copyright (c) Steve Creps, 1988.                               */
/* Copyright (c) Alex Smith, 2013.                                */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef HACKLIB_H
# define HACKLIB_H

# include "config.h"

extern char *tabexpand(char *); /* TODO: Bounds check */
extern boolean letter(char);
extern boolean digit(char);
extern char lowc(char);
extern char highc(char);
extern char *mungspaces(char *);
extern char *xcrypt(const char *, char *);
extern int base85enclen(int);
extern int base85declen(int);
extern int base85enc(const unsigned char *, int, char *);
extern int base85dec(const char *, unsigned char *);
extern boolean onlyspace(const char *);
extern boolean onlynul(const void *, int);
extern const char *ordin(int);
extern int sgn(int);
extern int rounddiv(long, int);
extern long long isqrt(long long);
extern long long ilog2(long long);
extern int popcount(unsigned long long);
extern int nextprime(int);
extern int dist2(int, int, int, int);
extern int distmin(int, int, int, int);
extern boolean online2(int, int, int, int);
extern boolean pmatch(const char *, const char *);

# ifndef STRSTRI
extern const char *strstri(const char *, const char *);
# endif
extern char *strstri_mutable(char *, const char *);

extern boolean fuzzymatch(const char *, const char *, const char *, boolean);

#endif

