/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright 1994, Dean Luick                                     */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef TIMEOUT_H
# define TIMEOUT_H

# include "global.h"

/* generic timeout function */
typedef void (*timeout_proc) (void *, long);

/* kind of timer */
# define TIMER_LEVEL    0       /* event specific to level */
# define TIMER_GLOBAL   1       /* event follows current play */
# define TIMER_OBJECT   2       /* event follows a object */

/* save/restore timer ranges */
# define RANGE_LEVEL  0 /* save/restore timers staying on level */
# define RANGE_GLOBAL 1 /* save/restore timers following global play */

/*
 * Timeout functions.  Add a define here, then put it in the table
 * in timeout.c.  "One more level of indirection will fix everything."
 */
# define ROT_ORGANIC    0       /* for buried organics */
# define ROT_CORPSE     1
# define REVIVE_MON     2
# define BURN_OBJECT    3
# define HATCH_EGG      4
# define FIG_TRANSFORM  5
# define NUM_TIME_FUNCS 6

/* used in timeout.c */
typedef struct timer_element {
    struct timer_element *next; /* next item in chain */
    void *arg;  /* pointer to timeout argument */
    unsigned int timeout;       /* when we time out */
    unsigned int tid;   /* timer ID */
    short kind; /* kind of use */
    uchar func_index;   /* what to call when we time out */
    unsigned needs_fixup:1;     /* does arg need to be patched? */
} timer_element;

#endif /* TIMEOUT_H */

