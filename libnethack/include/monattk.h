/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-23 */
/* NetHack may be freely redistributed.  See license for details. */
/* Copyright 1988, M. Stephenson */

#ifndef MONATTK_H
# define MONATTK_H

/* Monster attack types. Order affects experience, type > AT_BUTT gives more */
# define AT_ANY  (-1)   /* fake attack; dmgtype_fromattack wildcard */
# define AT_NONE 0      /* passive monster (ex. acid blob) */
# define AT_CLAW 1      /* claw (punch, hit, etc.) */
# define AT_BITE 2      /* bite */
# define AT_KICK 3      /* kick */
# define AT_BUTT 4      /* head butt (ex. a unicorn) */
# define AT_TUCH 5      /* touches */
# define AT_STNG 6      /* sting */
# define AT_HUGS 7      /* crushing bearhug */
# define AT_SPIT 10     /* spits substance - ranged */
# define AT_ENGL 11     /* engulf (swallow or by a cloud) */
# define AT_BREA 12     /* breath - ranged */
# define AT_EXPL 13     /* explodes - proximity */
# define AT_BOOM 14     /* explodes when killed */
# define AT_GAZE 15     /* gaze - ranged */
# define AT_TENT 16     /* tentacles */
# define AT_AREA 17     /* area of effect ability */

/* Special attack types, some of them aren't used on permonsts directly */
# define AT_WEAP 252    /* weapons */
# define AT_SCRL 253    /* scrolls */
# define AT_WAND 254    /* wands */
# define AT_SPEL 255    /* spells */
# define LAST_AT AT_SPEL

/*      Add new damage types below.
 *
 *      Note that 1-10 correspond to the types of attack used in buzz().
 *      Please don't disturb the order unless you rewrite the buzz() code.
 */
# define AD_ANY  (-1)   /* fake damage; attacktype_fordmg wildcard */
# define AD_PHYS 0      /* ordinary physical */
# define AD_MAGM 1      /* magic missiles */
# define AD_FIRE 2      /* fire damage */
# define AD_COLD 3      /* frost damage */
# define AD_SLEE 4      /* sleep ray */
# define AD_DISN 5      /* disintegration (death ray) */
# define AD_ELEC 6      /* shock damage */
# define AD_DRST 7      /* drains str (poison) */
# define AD_ACID 8      /* acid damage */
# define AD_STUN 9      /* stuns */
# define AD_SPC1 10     /* for extension of buzz() */
# define AD_BLND 11     /* blinds (yellow light) */
# define AD_SLOW 12     /* slows */
# define AD_PLYS 13     /* paralyses */
# define AD_DRLI 14     /* drains life levels (Vampire) */
# define AD_DREN 15     /* drains magic energy */
# define AD_LEGS 16     /* damages legs (xan) */
# define AD_STON 17     /* petrifies (Medusa, cockatrice) */
# define AD_STCK 18     /* sticks to you (mimic) */
# define AD_SGLD 19     /* steals gold (leppie) */
# define AD_SITM 20     /* steals item (nymphs) */
# define AD_SEDU 21     /* seduces & steals multiple items */
# define AD_TLPT 22     /* teleports you (Quantum Mech.) */
# define AD_RUST 23     /* rusts armor (Rust Monster) */
# define AD_CONF 24     /* confuses (Umber Hulk) */
# define AD_DGST 25     /* digests opponent (trapper, etc.) */
# define AD_HEAL 26     /* heals opponent's wounds (nurse) */
# define AD_WRAP 27     /* special "stick" for eels */
# define AD_WERE 28     /* confers lycanthropy */
# define AD_DRDX 29     /* drains dexterity (quasit) */
# define AD_DRCO 30     /* drains constitution */
# define AD_DRIN 31     /* drains intelligence (mind flayer) */
# define AD_DISE 32     /* confers diseases */
# define AD_DCAY 33     /* decays organics (brown Pudding) */
# define AD_SSEX 34     /* Succubus seduction (extended) */
# define AD_HALU 35     /* causes hallucination */
# define AD_DETH 36     /* for Death only */
# define AD_PEST 37     /* for Pestilence only */
# define AD_FAMN 38     /* for Famine only */
# define AD_SLIM 39     /* turns you into green slime */
# define AD_ENCH 40     /* remove enchantment (disenchanter) */
# define AD_CORR 41     /* corrode armor (black pudding) */
# define AD_ZOMB 42     /* zombifies + revives corpses (if AT_AREA) */

# define AD_RBRE 240    /* random breath weapon */

# define AD_SAMU 252    /* hits, may steal Amulet (Wizard) */
# define AD_CURS 253    /* random curse (ex. gremlin) */
# define LAST_AD AD_CURS


/*
 *  Monster to monster attacks.  When a monster attacks another (mattackm),
 *  any or all of the following can be returned.  See mattackm() for more
 *  details.
 */
# define MM_MISS        0x0     /* aggressor missed */
# define MM_HIT         0x1     /* aggressor hit defender */
# define MM_DEF_DIED    0x2     /* defender died */
# define MM_AGR_DIED    0x4     /* aggressor died */
# define MM_EXPELLED    0x8     /* defender was saved by slow digestion */

/* Return value from functions that handle attack-like actions (anything that
   can hit and damage an adjacent monster). Note that "ac_continue" varies a
   little in meaning; for a function that's checking if an attack is possible it
   means "sure, go ahead with the attack"; for a function that conditionally
   performs an attack it means "you swung your weapon past this square, and
   there weren't any monsters that got in the way". */
enum attack_check_status {
    ac_continue,        /* nothing prevents this action from happening */
    ac_cancel,          /* the attack-like action was cancelled */
    ac_somethingelse,   /* something else happened, which consumes time */
    ac_monsterhit,      /* the attack-like action hit a monster */
};

/* Argument to combat_msgc, describing what happened. */
enum combatresult {
    cr_miss,   /* an attack missed; or a passive attack hit an immunity */
    cr_hit,    /* an attack hit */
    cr_immune, /* an active attack hit, but is 100% resisted by the target */
    cr_resist, /* an attack hit but not for full effect */
    cr_kill,   /* an attack hit and killed the target, potentially petfatal */
    cr_kill0,  /* ditto, but never prints a petfatal (presumably because the
                  caller will in that case) */
};

/* Monster special abilities. In monattk.h since most are various kinds of
   attacks. Currently only properly handles having a single kind of
   gaze/breath/spit ability (each). */
enum monabil {
    abil_none, /* used as terminator */
    abil_pray,
    abil_turn,
    abil_tele,
    abil_jump,
    abil_spit,
    abil_remove_ball,
    abil_gaze,
    abil_weresummon,
    abil_web,
    abil_hide,
    abil_mindblast,
    abil_multiply,
    abil_unihorn,
    abil_shriek,
    abil_breathe,
    last_abil = abil_breathe,
};

#endif /* MONATTK_H */
