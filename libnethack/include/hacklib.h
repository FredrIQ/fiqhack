/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* Copyright (c) Steve Creps, 1988.                               */
/* Copyright (c) Alex Smith, 2013.                                */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef HACKLIB_H
# define HACKLIB_H

# include "config.h"

extern char *tabexpand(char *);
extern boolean letter(char);
extern boolean digit(char);
extern char *eos(char *);
extern char lowc(char);
extern char highc(char);
extern char *lcase(char *);
extern char *upstart(char *);
extern char *mungspaces(char *);
extern char *s_suffix(const char *);
extern char *xcrypt(const char *, char *);
extern boolean onlyspace(const char *);
extern const char *ordin(int);
extern char *sitoa(int);
extern int sgn(int);
extern int rounddiv(long, int);
extern int dist2(int, int, int, int);
extern int distmin(int, int, int, int);
extern boolean online2(int, int, int, int);
extern boolean pmatch(const char *, const char *);

# ifndef STRSTRI
extern char *strstri(const char *, const char *);
# endif
extern boolean fuzzymatch(const char *, const char *, const char *, boolean);
extern unsigned int get_seedval(void);

#endif
