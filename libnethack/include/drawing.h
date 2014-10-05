/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) 2013 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

/* drawing.c is pretty self-contained in its own right, so it gets a
   self-contained header file. */

#ifndef DRAWING_H
# define DRAWING_H

# include "nethack_types.h"

extern const char *make_mon_name(int);
extern char *make_object_name(int);

extern void populate_mon_symdef(int, struct nh_symdef *);
extern void populate_obj_symdef(int, struct nh_symdef *);

extern const struct nh_symdef defsyms[];
extern const struct nh_symdef warnsyms[];
extern const struct nh_symdef trapsyms[];
extern const struct nh_symdef explsyms[];
extern const struct nh_symdef zapsyms[];
extern const struct nh_symdef effectsyms[];
extern const struct nh_symdef swallowsyms[];

extern const struct nh_symdef expltypes[];
extern const struct nh_symdef zaptypes[];

extern const char *const defexplain[];
extern const char *const warnexplain[];
extern const char *const trapexplain[];
extern const char *const invismonexplain;

#endif

