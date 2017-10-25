/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MONST_H
# define MONST_H

# include "global.h"
# include "coord.h"
# include "monuse.h"
# include "prop.h"

/* The weapon_check flag is used two ways:
 * 1) When calling mon_wield_item, is 2-6 depending on what is desired.
 * 2) Between calls to mon_wield_item, is 0 or 1 depending on whether or not
 *    the weapon is known by the monster to be cursed (so it shouldn't bother
 *    trying for another weapon).
 * I originally planned to also use 0 if the monster already had its best
 * weapon, to avoid the overhead of a call to mon_wield_item, but it turns out
 * that there are enough situations which might make a monster change its
 * weapon that this is impractical.  --KAA
 */
# define NO_WEAPON_WANTED 0
# define NEED_WEAPON 1
# define NEED_RANGED_WEAPON 2
# define NEED_HTH_WEAPON 3
# define NEED_PICK_AXE 4
# define NEED_AXE 5
# define NEED_PICK_OR_AXE 6

/* The following flags are used for the second argument to display_minventory
 * in invent.c:
 *
 * MINV_NOLET  If set, don't display inventory letters on monster's inventory.
 * MINV_ALL    If set, display all items in monster's inventory, otherwise
 *             just display wielded weapons and worn items.
 */
# define MINV_NOLET 0x01
# define MINV_ALL   0x02

# ifndef MEXTRA_H
#  include "mextra.h"
# endif

/* Monster strategies */
enum monstrat {
    st_none,
    st_close,                           /* idle until player adjacant */
    st_waiting,                         /* idle until player in LOS */
    st_firstwait = st_close,
    st_lastwait = st_waiting,
    st_mon,                             /* chasing a monster */
    st_obj,                             /* take an item */
    st_trap,                            /* going to a trap */
    st_square,                          /* chasing a square in general */
    st_ascend,                          /* monster is trying to ascend */
    st_wander,                          /* wandering aimlessy */
    st_firsttarget = st_mon,
    st_lasttarget = st_wander,
    st_escape,                          /* escaping from STRAT_GOAL[XY] */
    st_heal,                            /* heal up self (covetous) */
    st_lineup,                          /* find position in line of STRAT_GOAL */
    st_nolineup,                        /* same as above, but avoid it */
};

struct monst {
    struct monst *nmon; /* next monster in the level monster list */
    const struct permonst *data;
    struct level *dlevel;       /* pointer to the level this monster is on */
    struct obj *minvent;
    struct obj *meminvent;
    struct obj *mw;             /* weapon */
    unsigned int mstuck;        /* is the monster stuck to another monster */
    struct mextra *mextra;      /* extended data for some monsters (or names) */
    unsigned int m_id;
    int mhp, mhpmax;
    int pw, pwmax;
    int mspec_used;             /* monster's special ability attack timeout */
    unsigned int mtrapseen;     /* bitmap of traps we've been trapped in */
    unsigned int mlstmv;        /* for catching up with lost time */

    int mstrategy;              /* Current monster strategy */
    xchar sx, sy;               /* Monster strategy coordinates */
    xchar mstratprio;           /* strategy priority -- what the monster is willing to afford */

    /* migration */
    uchar xyloc, xyflags, xlocale, ylocale;

    short orig_mnum;            /* monster ID pre-polyself */
    int orig_hp;                /* monster HP pre-polyself */
    int orig_hpmax;             /* monster maxHP pre-polyself */
    uchar polyself_timer;       /* polyelf timer (0-255, decreases at 1/4 turnspeed) */
    uchar m_lev;                /* adjusted difficulty level of monster */
    xchar mx, my;               /* monster location */
    xchar dx, dy;               /* monster's displaced image, COLNO/ROWNO if none */
    xchar mux, muy;             /* where the monster thinks you are; if it doesn't know
                                   where you are, this is (COLNO, ROWNO) */
    /* TODO: find a saner name for malign */
    aligntyp malign;            /* alignment of this monster, relative to the player
                                   (positive = good to kill) */
    aligntyp maligntyp;         /* monster alignment */
    aligntyp maligntyp_temp;    /* temporary alignment from opposite alignment */
    short moveoffset;           /* how this monster's actions map onto turns */
    schar mtame;                /* level of tameness, implies peaceful */
    uchar m_ap_type;            /* what mappearance is describing: */
# define M_AP_NOTHING   0       /* mappearance is unused -- monster appears as
                                   itself */
# define M_AP_FURNITURE 1       /* stairs, a door, an altar, etc. */
# define M_AP_OBJECT    2       /* an object */
# define M_AP_MONSTER   3       /* a monster */

    uchar mfrozen;

    unsigned int mappearance;   /* for undetected mimics and the wiz */

    /* Whether a delayed instadeath for a monster was caused by you */
    unsigned usicked:1;
    unsigned uslimed:1;
    unsigned ustoned:1;
    unsigned uzombied:1;
    unsigned levi_wary:1;       /* worried about levi timing out */
    unsigned female:1;          /* is female */
    unsigned cham:3;            /* shape-changer */
/* note: lycanthropes are handled elsewhere */
# define CHAM_ORDINARY          0       /* not a shapechanger */
# define CHAM_CHAMELEON         1       /* animal */
# define CHAM_DOPPELGANGER      2       /* demi-human */
# define CHAM_SANDESTIN         3       /* demon */
# define CHAM_MAX_INDX          CHAM_SANDESTIN
    unsigned mundetected:1;     /* when S_EEL: underwater
                                   for hiding monsters: currently hidden
                                   otherwise: unused, always 0 */
    /* implies one of M1_CONCEAL or M1_HIDE, but not mimic (that is, snake,
       spider, trapper, piercer, eel) */

    unsigned mburied:1;         /* has been buried */
    unsigned mrevived:1;        /* has been revived from the dead */
    unsigned mavenge:1;         /* did something to deserve retaliation */

    unsigned mflee:1;           /* fleeing */
    unsigned mcanmove:1;        /* paralysis, similar to mblinded */
    unsigned msleeping:1;       /* asleep until woken */
    unsigned mpeaceful:1;       /* does not attack unprovoked */
    unsigned mtrapped:1;        /* trapped in a pit, web or bear trap */

    unsigned mleashed:1;        /* monster is on a leash */
    unsigned msuspicious:1;     /* monster is suspicious of the player */
    unsigned iswiz:1;           /* is the Wizard of Yendor */
    uint64_t mspells;           /* known monster spells */
    uint64_t spells_maintained; /* maintained spells */

    /* intrinsic format: os_outside:1, os_timeout:15 */
    short mintrinsic[LAST_PROP + 1]; /* monster intrinsics */
    int mintrinsic_cache[LAST_PROP + 1]; /* cached from above */

    /* turnstate; doesn't count against bitfield bit count */
    unsigned deadmonster:1;     /* always 0 at neutral turnstate */

    uchar mfleetim;     /* timeout for mflee */
    uchar wormno;       /* at most 31 worms on any level */
# define MAX_NUM_WORMS  32      /* wormno could hold larger worm ids, but 32 is
                                   (still) fine */
    xchar weapon_check;
    int misc_worn_check;

    short former_player; /* info about this being the ghost or whatnot
                            of a former player, from a bones file */
    int meating;        /* monster is eating timeout */
    schar mhitinc;      /* monster intrinsic to-hit bonus/penalty */
    schar mdaminc;      /* monster intrinsic damage bonus/penalty */
    schar msearchinc;   /* monster intrinsic search bonus/penalty */
    schar mac;          /* monster AC bonus/penalty */
};

# define MON_WEP(mon)     (m_mwep(mon))
# define MON_NOWEP(mon)   ((mon)->mw = NULL)

# define DEADMONSTER(mon) ((mon)->deadmonster)

# define onmap(mon) (isok((mon)->mx, (mon)->my))

/* player/monster symmetry; eventually we'll just access the fields of youmonst
   directly, but for now, having accessor macros is a good compromise

   DEFERRED: mburied appears to be a deferred feature, it's not set anywhere in
   the code. */
# define m_mburied(mon) ((mon) == &youmonst ? u.uburied : (mon)->mburied)
/* Iterator helper that includes youmonst */
# define monnext(mon) (!(mon) ? NULL :                                  \
                       (mon) == &youmonst ? NULL :                      \
                       (mon)->nmon ? (mon)->nmon :                      \
                       (mon)->dlevel && (mon)->dlevel == level ? &youmonst : \
                       NULL)
# define m_mundetected(mon) ((mon) == &youmonst ? u.uundetected :       \
                             (mon)->mundetected)
# define m_mhiding(mon) ((mon) == &youmonst ? u.uundetected :   \
                         is_hider(mon->data) && (mon)->mundetected)
# define m_underwater(mon) ((mon) == &youmonst ? Underwater :           \
                            (mon)->data->mlet == S_EEL && (mon)->mundetected)
/* TODO: does the hero have a true mon->dlevel equavilent? */
# define m_dlevel(mon) ((mon) == &youmonst ? level : (mon)->dlevel)
# define m_mx(mon) ((mon) == &youmonst ? u.ux : (mon)->mx)
# define m_my(mon) ((mon) == &youmonst ? u.uy : (mon)->my)
# define m_mz(mon) ((mon) == &youmonst ? &u.uz : &((mon)->dlevel->z))
/* mhp(max) will not work for polymorphed player, when monster polymorph
   is implemented, mhp will always point to current hp (umh/umhmax "equavilent"
   will be orig_hp(max)) */
# define m_mhp(mon) ((mon) == &youmonst ? u.uhp : (mon)->mhp)
# define m_mhpmax(mon) ((mon) == &youmonst ? u.uhpmax : (mon)->mhpmax)
# define m_mlev(mon) ((mon) == &youmonst ? u.ulevel : (mon)->m_lev)
# define m_mwep(mon) ((mon) == &youmonst ? uwep : (mon)->mw)
/* actually used for players and monsters alike now */
# define m_mspellprot(mon) ((property_timeout(mon, PROTECTION) + 9) / 10)

/* Does a monster know where the player character is? Does it think it does? */
# define engulfing_u(mon) (Engulfed && (mon) == u.ustuck)
# define knows_ux_uy(mon) (((mon)->mux == u.ux && (mon)->muy == u.uy) || \
                           engulfing_u(mon))
# define aware_of_u(mon)  (isok((mon)->mux, (mon)->muy) || engulfing_u(mon))

/* Strategy helpers */
# define idle(mon) ((mon)->mstrategy >= st_firstwait && \
                    (mon)->mstrategy <= st_lastwait)
# define st_target(mon) ((mon)->sx != COLNO && \
                         (mon)->mstrategy >= st_firsttarget && \
                         (mon)->mstrategy <= st_lasttarget)
# define is_goal(mon, x, y) (st_target(mon) && (mon)->sx == (x) && \
                             (mon)->sy == (y))

/* More detail on why a monster doesn't sense you: typically used for messages
   that describe where a monster is aiming; also used to determine whether a
   monster can specifically see a displaced image (if you're invisible and it
   doesn't have see invis, it can't even if you're Displaced) */
enum monster_awareness_reasons {
    mar_unaware,
    mar_guessing_invis,
    mar_guessing_displaced,
    mar_guessing_other,
    mar_aware
};
# define awareness_reason(mon) (!aware_of_u(mon) ? mar_unaware :        \
                                knows_ux_uy(mon) ? mar_aware :          \
                                ((Invis && !see_invisible(mon)) ||      \
                                 (blind(mon))) ?                        \
                                mar_guessing_invis :                    \
                                Displaced ? mar_guessing_displaced :    \
                                mar_guessing_other)

/* When a long worm is hit, is the hit on the head or thebody? */
extern boolean notonhead;

/* Extra return value for select_rwep() */
extern struct obj *propellor;

/* Polyform special abilities, UI code

   Perhaps eventually this will be part of the monster structure, but for
   now, it's generated as-needed via a hardcoded if statement */
struct polyform_ability {
    const char *description; /* infinitive without the 'to' */
    boolean directed;
    union {
        int (*handler_directed)(const struct musable *);
        int (*handler_undirected)(void);
    };
};

#endif /* MONST_H */
