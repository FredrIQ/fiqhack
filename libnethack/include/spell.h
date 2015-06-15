/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-15 */
/* Copyright 1986, M. Stephenson                                  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef SPELL_H
# define SPELL_H

# include "global.h"

/* special sp_id values for divine and supernatural abilities */
#define SPID_PRAY -1 /* #pray: available to everyone */
#define SPID_TURN -2 /* #turn: available to certain classes */
#define SPID_RLOC -3 /* teleport at will; requires two properties */
#define SPID_JUMP -4 /* #jump: from jumping boots, or the Knight ability */
#define SPID_MONS -5 /* #monster: from polyself */
#define SPID_COUNT 5 /* number of SPID_ constants */
#define SPID_PREFERRED_LETTER "WXVYZ" /* must be at the end of the alphabet */
/* debug mode commands sort-of belong here, but it'd be a UI nightmare and
   would probably overflow the list */

static_assert(sizeof SPID_PREFERRED_LETTER == SPID_COUNT + 1,
              "each supernatural spell has a preferred letter");

#define SPELL_IS_FROM_SPELLBOOK(spell) (spl_book[spell].sp_id > 0)

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

