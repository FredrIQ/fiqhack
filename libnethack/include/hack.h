/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef HACK_H
# define HACK_H

/* This is needed because we redefine yn */
# include <math.h>

# ifndef CONFIG_H
#  include "config.h"
# endif
# define NETHACK_H_IN_LIBNETHACK
# include "nethack.h"
# include "menulist.h"

/*      For debugging beta code.        */
# ifdef BETA
#  define Dpline        pline
# endif

# define TELL           1
# define NOTELL         0
# define BOLT_LIM       8       /* from this distance ranged attacks will be
                                   made */
# define MAX_CARR_CAP   1000    /* so that boulders can be heavier */

/* symbolic names for capacity levels */
# define UNENCUMBERED   0
# define SLT_ENCUMBER   1       /* Burdened */
# define MOD_ENCUMBER   2       /* Stressed */
# define HVY_ENCUMBER   3       /* Strained */
# define EXT_ENCUMBER   4       /* Overtaxed */
# define OVERLOADED     5       /* Overloaded */

/* Macros for how a rumor was delivered in outrumor() */
# define BY_ORACLE      0
# define BY_COOKIE      1
# define BY_PAPER       2
# define BY_OTHER       9

/* Macros for why you are no longer riding */
# define DISMOUNT_GENERIC       0
# define DISMOUNT_FELL          1
# define DISMOUNT_THROWN        2
# define DISMOUNT_POLY          3
# define DISMOUNT_ENGULFED      4
# define DISMOUNT_BONES         5
# define DISMOUNT_BYCHOICE      6

/* sellobj_state() states */
# define SELL_NORMAL            (0)
# define SELL_DELIBERATE        (1)
# define SELL_DONTSELL          (2)

/* This is the way the game ends.  If these are rearranged or new entries are
   added, the arrays in end.c will need to be changed. */
# define DIED            0
# define FIRST_ENDING    (DIED)
# define FIRST_KILLER    (DIED)
# define CHOKING         1
/* POISONING is overloaded; it's also used for illness */
# define POISONING       2
# define STARVING        3
# define DROWNING        4
# define SUFFOCATION     5
# define BURNING         6
/* DISSOLVED is for dissolving in a pool of lava */
# define DISSOLVED       7
/* CRUSHING is specifically for drawbridge-related deaths, even if they aren't
 * technically a death by crushing */
# define CRUSHING        8
# define STONING         9
# define TURNED_SLIME   10
# define EXPLODED       11
# define GENOCIDED      12
# define LAST_KILLER    (GENOCIDED)
# define NUM_KILLERS    (LAST_KILLER + 1)
# define TRICKED        13
# define QUIT           14
# define ESCAPED        15
# define ASCENDED       16
# define LAST_ENDING    (ASCENDED)
# define NUM_ENDINGS    (LAST_ENDING + 1)

# include "align.h"
# include "dungeon.h"
# include "message.h"
# include "monsym.h"
# include "mkroom.h"
# include "objclass.h"
# include "youprop.h"
# include "decl.h"
# include "timeout.h"

extern coord bhitpos;   /* place where throw or zap hits or stops */

/* types of calls to bhit() */
# define ZAPPED_WAND    0
# define THROWN_WEAPON  1
# define KICKED_WEAPON  2
# define FLASHED_LIGHT  3
# define INVIS_BEAM     4

# define MATCH_WARN_OF_MON(mon)  (worn_warntype() & (mon)->data->mflags2)

# include "trap.h"
# include "flag.h"
# include "rm.h"
# include "vision.h"
# include "display.h"
# include "engrave.h"
# include "rect.h"
# include "region.h"
# include "monuse.h"
# include "extern.h"
# include "magic.h"
# include "winprocs.h"
# include "rnd.h"

# define NO_SPELL         0

/* internal state of distmap; caller allocates so that it can be reused by
   multiple distmap calls */
struct distmap_state {
    int onmap[COLNO][ROWNO];
    xchar travelstepx[2][COLNO * ROWNO];
    xchar travelstepy[2][COLNO * ROWNO];
    int curdist;
    int tslen;
    struct monst *mon;
    int mmflags;
};

/* flags to control makemon() and/or goodpos() */
# define NO_MM_FLAGS      0x0000 /* use this rather than plain 0 */
# define NO_MINVENT       0x0001 /* suppress minvent when creating mon */
# define MM_NOWAIT        0x0002 /* don't set STRAT_WAITMASK flags */
# define MM_EDOG          0x0004 /* add edog structure */
# define MM_EMIN          0x0008 /* add emin structure */
# define MM_ANGRY         0x0010 /* monster is created angry */
# define MM_NONAME        0x0020 /* monster is not christened */
# define MM_NOCOUNTBIRTH  0x0040 /* don't increment born counter (for
                                    revival) */
# define MM_IGNOREWATER   0x0080 /* ignore water when positioning */
# define MM_ADJACENTOK    0x0100 /* it is acceptable to use adjacent
                                    coordinates */
# define MM_IGNOREMONST   0x0200 /* this location can contain a monster already;
                                    don't use in makemon() for obvious
                                    reasons */
# define MM_IGNOREDOORS   0x0400 /* assume all doors are open and boulders don't
                                    block squares; goodpos() only for now */
# define MM_SPECIESLEVRNG 0x0800 /* generate monster species on the level's
                                    generation RNG */
# define MM_ALLLEVRNG     0x1000 /* generate all aspects of the monster on the
                                    level's generation RNG (and do not take
                                    any aspects of the player into account) */
# define MM_CREATEMONSTER 0x2000 /* created by a "create monster" effect;
                                    must be ORed with... */
# define MM_CMONSTER_M    0x0800 /* ...created by monster, */
# define MM_CMONSTER_U    0x1000 /* created by player, or */
# define MM_CMONSTER_T    0x1800 /* either, but will be tamed after creation */
# define MM_CHEWROCK      0x4000 /* allow placement on diggable walls */

/* special mhpmax value when loading bones monster to flag as extinct or
   genocided */
# define DEFUNCT_MONSTER  (-100)

/* flags for special ggetobj status returns */
# define ALL_FINISHED     0x01  /* called routine already finished the job */

/* flags to control query_objlist() */
# define BY_NEXTHERE      0x1   /* follow objlist by nexthere field */
# define AUTOSELECT_SINGLE 0x2  /* if only 1 object, don't ask */
# define USE_INVLET       0x4   /* use object's invlet */
# define INVORDER_SORT    0x8   /* sort objects by packorder */
# define SIGNAL_NOMENU    0x10  /* return -1 rather than 0 if none allowed */
# define SIGNAL_ESCAPE	  0x20  /* return -2 rather than 0 if menu escaped */
# define FEEL_COCKATRICE 0x40  /* engage cockatrice checks and react */

/* Flags to control query_category() */
/* BY_NEXTHERE used by query_category() too, so skip 0x01 */
# define UNPAID_TYPES     0x02
# define GOLD_TYPES       0x04
# define WORN_TYPES       0x08
# define ALL_TYPES        0x10
# define BILLED_TYPES     0x20
# define CHOOSE_ALL       0x40
# define BUC_BLESSED      0x80
# define BUC_CURSED       0x100
# define BUC_UNCURSED     0x200
# define BUC_UNKNOWN      0x400
# define UNIDENTIFIED     0x800
# define BUC_ALLBKNOWN (BUC_BLESSED|BUC_CURSED|BUC_UNCURSED)
# define ALL_TYPES_SELECTED -2

/* Flags to control find_mid() */
# define FM_FMON          0x01  /* search the level->monlist chain */
# define FM_MIGRATE       0x02  /* search the migrating monster chain */
# define FM_MYDOGS        0x04  /* search the migrating pet chain */
# define FM_EVERYWHERE  (FM_FMON | FM_MIGRATE | FM_MYDOGS)

/* Flags to control dotrap() in trap.c */
# define NOWEBMSG         0x01  /* suppress stumble into web message */
# define FORCEBUNGLE      0x02  /* adjustments appropriate for bungling */
# define RECURSIVETRAP    0x04  /* trap changed into another type this same
                                   turn */

/* Flags to control test_move in hack.c */
# define DO_MOVE          0    /* really doing the move */
# define TEST_MOVE        1    /* test a normal move (move there next) */
# define TEST_TRAV        2    /* test a future travel location */
# define TEST_SLOW        3    /* test if we can travel through the location,
                                  but don't want to (trap, door, etc.) */

struct test_move_cache {
    boolean blind;
    boolean stunned;
    boolean fumbling;
    boolean halluc;
    boolean passwall;
    boolean grounded;

    /* This is never set by init_test_move_cache. Setting it manually changes
       the messages that are produced for squeezing onto a boulder. */
    boolean instead_of_pushing_boulder;
};

/*** some utility macros ***/

/* POSIX specifies that yn() is a Bessel function of the second kind. */
# ifdef yn
#  undef yn
# endif

# define yn(query) yn_function(query,ynchars, 'n')
# define ynq(query) yn_function(query,ynqchars, 'q')
# define ynaq(query) yn_function(query,ynaqchars, 'y')
# define nyaq(query) yn_function(query,ynaqchars, 'n')

/* Macros for scatter */
# define VIS_EFFECTS      0x01  /* display visual effects */
# define MAY_HITMON       0x02  /* objects may hit monsters */
# define MAY_HITYOU       0x04  /* objects may hit you */
# define MAY_HIT          (MAY_HITMON|MAY_HITYOU)
# define MAY_DESTROY      0x08  /* objects may be destroyed at random */
# define MAY_FRACTURE     0x10  /* boulders & statues may fracture */

/* Macros for launching objects */
# define ROLL             0x01  /* the object is rolling */
# define FLING            0x02  /* the object is flying thru the air */
# define LAUNCH_UNSEEN    0x40  /* hero neither caused nor saw it */
# define LAUNCH_KNOWN     0x80  /* the hero caused this by explicit action */

/* Macros for messages referring to hands, eyes, feet, etc... */
# define ARM              0
# define EYE              1
# define FACE             2
# define FINGER           3
# define FINGERTIP        4
# define FOOT             5
# define HAND             6
# define HANDED           7
# define HEAD             8
# define LEG              9
# define LIGHT_HEADED     10
# define NECK             11
# define SPINE            12
# define TOE              13
# define HAIR             14
# define BLOOD            15
# define LUNG             16
# define NOSE             17
# define STOMACH          18
# define HIDE             19

/* Flags to control menus */
# define MENU_PARTIAL     2
# define MENU_FULL        3

/* sizes for base64 string buffers */
/* these are given some extra space to allow for potential compression
   overhead */
# define ENCBUFSZ         512
# define EQBUFSZ          256

# ifndef max
#  define max(a,b) ((a) > (b) ? (a) : (b))
# endif
# ifndef min
#  define min(x,y) ((x) < (y) ? (x) : (y))
# endif
# define plur(x) (((x) == 1) ? "" : "s")

# define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

# define ARM_BONUS(obj)   (objects[(obj)->otyp].a_ac + (obj)->spe \
             - min((int)greatest_erosion(obj),objects[(obj)->otyp].a_ac))

# define makeknown(x)     discover_object((x),TRUE,TRUE,FALSE)
# define distu(xx,yy)     dist2((int)(xx),(int)(yy),(int)u.ux,(int)u.uy)
# define onlineu(xx,yy)   online2((int)(xx),(int)(yy),(int)u.ux,(int)u.uy)

# define rn1(x,y)         (rn2(x)+(y))

/* negative armor class is randomly weakened to prevent invulnerability */
# define AC_VALUE(AC)     ((AC) >= 0 ? (AC) : -rnd(-(AC)))

/* Byte swapping macros for the savefile code to make endian-independent saves.
   Many systems have such macros, but using different names every time.
   Defining our own is easier than figuring that out. */
# define _byteswap16(x)   ((((x) & 0x00ffU) << 8) | \
                           (((x) & 0xff00U) >> 8))

# define _byteswap32(x)   ((((x) & 0x000000ffLU) << 24) | \
                           (((x) & 0x0000ff00LU) <<  8) | \
                           (((x) & 0x00ff0000LU) >>  8) | \
                           (((x) & 0xff000000LU) >> 24))

# define _byteswap64(x)   ((((x) & 0x00000000000000ffLLU) << 56) | \
                           (((x) & 0x000000000000ff00LLU) << 40) | \
                           (((x) & 0x0000000000ff0000LLU) << 24) | \
                           (((x) & 0x00000000ff000000LLU) <<  8) | \
                           (((x) & 0x000000ff00000000LLU) <<  8) | \
                           (((x) & 0x0000ff0000000000LLU) << 24) | \
                           (((x) & 0x00ff000000000000LLU) << 40) | \
                           (((x) & 0xff00000000000000LLU) << 56))

/* If endian.h exists (on Linux for example and perhaps on other UNIX) and is
   indirectly included via the system headers, we may be able to find out what
   the endianness is.  Otherwise define IS_BIG_ENDIAN in config.h */
# if defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
    __BYTE_ORDER == __BIG_ENDIAN
#  define IS_BIG_ENDIAN
# endif

/* An API checkpoint lets us simulate an exception in the case that we need to
   cause an exceptional control flow pattern. The basic rule is that
   API_ENTRY_CHECKPOINT() is called at every entry point to the API (including
   the ones that shouldn't be called out of sequence; being able to use
   exceptions to handle out-of-sequence calls is good!), with the main
   interesting call being that in nh_play_game. API_EXIT() is the opposite, and
   should match API_ENTRY_CHECKPOINT: it specifies the end of scopes that
   API_ENTRY_CHECKPOINT creates.

   API_ENTRY_CHECKPOINT, being setjmp-based, needs an awkward calling convention
   (setjmp can't be moved into a stack frame of its own); additionally, it needs
   an awkward API because it can't be used in the middle of an expression. As
   such, this introdues a block (using "switch", the only legal way to unpack
   the return value of setjmp), using IF_API_EXCEPTION to generate the case
   labels. (The parameter to IF_API_EXCEPTION is an enum nh_play_status.)
 */
# define API_ENTRY_CHECKPOINT()                 \
    if (!(exit_jmp_buf_valid++))                \
        switch(nh_setjmp(exit_jmp_buf))                                 

# define API_ENTRY_OFFSET 5
# define IF_API_EXCEPTION(x) case ((x)+API_ENTRY_OFFSET)
# define IF_ANY_API_EXCEPTION() case 0: break; default

# define API_ENTRY_CHECKPOINT_RETURN_ON_ERROR(ret)      \
    do {                                                \
        API_ENTRY_CHECKPOINT() {                        \
        IF_ANY_API_EXCEPTION():                         \
            return (ret);                               \
        }                                               \
    } while (0)
# define API_ENTRY_CHECKPOINT_RETURN_VOID_ON_ERROR()    \
    do {                                                \
        API_ENTRY_CHECKPOINT() {                        \
        IF_ANY_API_EXCEPTION():                         \
            return;                                     \
        }                                               \
    } while (0)


# define API_EXIT() do {--exit_jmp_buf_valid; } while(0)

#endif /* HACK_H */

