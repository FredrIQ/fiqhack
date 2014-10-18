/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-10-17 */
/* Copyright 1986, M. Stephenson                                  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef SPELL_H
# define SPELL_H

# include "global.h"

struct spell {
    short sp_id;        /* spell id (== object.otyp) */
    xchar sp_lev;       /* power level */
    int sp_know;        /* knowlege of spell */
};

/* levels of memory destruction with a scroll of amnesia */
# define ALL_MAP          0x1
# define ALL_SPELLS       0x2

# define decrnknow(spell) spl_book[spell].sp_know--
# define spellid(spell)   spl_book[spell].sp_id
# define spellknow(spell) spl_book[spell].sp_know

extern const char *const flash_types[];

#endif /* SPELL_H */

