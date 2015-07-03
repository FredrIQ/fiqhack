/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) NetHack Development Team 1992.                   */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Default object class symbols.  See objclass.h. */
const char def_oc_syms[MAXOCLASSES] = {
    /* 0 */ '\0',
    /* placeholder for the "random class" */
    ILLOBJ_SYM,
    WEAPON_SYM,
    ARMOR_SYM,
    RING_SYM,
/* 5*/ AMULET_SYM,
    TOOL_SYM,
    FOOD_SYM,
    POTION_SYM,
    SCROLL_SYM,
/*10*/ SPBOOK_SYM,
    WAND_SYM,
    GOLD_SYM,
    GEM_SYM,
    ROCK_SYM,
/*15*/ BALL_SYM,
    CHAIN_SYM,
    VENOM_SYM
};

/* Default monster class symbols.  See monsym.h. */
const char def_monsyms[MAXMCLASSES] = {
    '\0',       /* holder */
    DEF_ANT,
    DEF_BLOB,
    DEF_COCKATRICE,
    DEF_DOG,
    DEF_EYE,
    DEF_FELINE,
    DEF_GREMLIN,
    DEF_HUMANOID,
    DEF_IMP,
    DEF_JELLY,  /* 10 */
    DEF_KOBOLD,
    DEF_LEPRECHAUN,
    DEF_MIMIC,
    DEF_NYMPH,
    DEF_ORC,
    DEF_PIERCER,
    DEF_QUADRUPED,
    DEF_RODENT,
    DEF_SPIDER,
    DEF_TRAPPER,        /* 20 */
    DEF_UNICORN,
    DEF_VORTEX,
    DEF_WORM,
    DEF_XAN,
    DEF_LIGHT,
    DEF_ZRUTY,
    DEF_ANGEL,
    DEF_BAT,
    DEF_CENTAUR,
    DEF_DRAGON, /* 30 */
    DEF_ELEMENTAL,
    DEF_FUNGUS,
    DEF_GNOME,
    DEF_GIANT,
    '\0',
    DEF_JABBERWOCK,
    DEF_KOP,
    DEF_LICH,
    DEF_MUMMY,
    DEF_NAGA,   /* 40 */
    DEF_OGRE,
    DEF_PUDDING,
    DEF_QUANTMECH,
    DEF_RUSTMONST,
    DEF_SNAKE,
    DEF_TROLL,
    DEF_UMBER,
    DEF_VAMPIRE,
    DEF_WRAITH,
    DEF_XORN,   /* 50 */
    DEF_YETI,
    DEF_ZOMBIE,
    DEF_HUMAN,
    DEF_GOLEM,
    DEF_DEMON,
    DEF_EEL,
    DEF_LIZARD,
    DEF_WORM_TAIL,
    DEF_MIMIC_DEF,      /* 60 */
};


/*
 * Convert the given character to an object class.  If the character is not
 * recognized, then MAXOCLASSES is returned.  Used in detect.c invent.c,
 * options.c, pickup.c, sp_lev.c, and lev_main.c.
 */
int
def_char_to_objclass(char ch)
{
    int i;

    for (i = 1; i < MAXOCLASSES; i++)
        if (ch == def_oc_syms[i])
            break;
    return i;
}


/*
 * Convert a character into a monster class.  This returns the _first_
 * match made.  If there are are no matches, return MAXMCLASSES.
 */
int
def_char_to_monclass(char ch)
{
    int i;

    for (i = 1; i < MAXMCLASSES; i++)
        if (def_monsyms[i] == ch)
            break;
    return i;
}

