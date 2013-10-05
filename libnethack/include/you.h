/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef YOU_H
# define YOU_H

# include "attrib.h"
# include "monst.h"
# ifndef PROP_H
#  include "prop.h"     /* (needed here for util/makedefs.c) */
# endif
# include "skills.h"

/*** Substructures ***/

struct u_have {
    unsigned amulet:1;  /* carrying Amulet */
    unsigned bell:1;    /* carrying Bell */
    unsigned book:1;    /* carrying Book */
    unsigned menorah:1; /* carrying Candelabrum */
    unsigned questart:1;        /* carrying the Quest Artifact */
    unsigned unused:3;
};

struct u_event {
    unsigned minor_oracle:1;    /* received at least 1 cheap oracle */
    unsigned major_oracle:1;    /* " expensive oracle */
    unsigned qcalled:1; /* called by Quest leader to do task */
    unsigned qexpelled:1;       /* expelled from the Quest dungeon */
    unsigned qcompleted:1;      /* successfully completed Quest task */
    unsigned uheard_tune:2;     /* 1=know about, 2=heard passtune */
    unsigned uopened_dbridge:1; /* opened the drawbridge */

    unsigned invoked:1; /* invoked Gate to the Sanctum level */
    unsigned gehennom_entered:1;        /* entered Gehennom via Valley */
    unsigned uhand_of_elbereth:2;       /* became Hand of Elbereth */
    unsigned udemigod:1;        /* killed the wiz */
    unsigned ascended:1;        /* has offered the Amulet */
};

/* KMH, conduct --
 * These are voluntary challenges.  Each field denotes the number of
 * times a challenge has been violated.
 */
struct u_conduct {      /* number of times... */
    unsigned int unvegetarian;  /* eaten any animal */
    unsigned int unvegan;       /* ... or any animal byproduct */
    unsigned int food;  /* ... or any comestible */
    unsigned int gnostic;       /* used prayer, priest, or altar */
    unsigned int weaphit;       /* hit a monster with a weapon */
    unsigned int killer;        /* killed a monster yourself */
    unsigned int literate;      /* read something (other than BotD) */
    unsigned int polypiles;     /* polymorphed an object */
    unsigned int polyselfs;     /* transformed yourself */
    unsigned int wishes;        /* used a wish */
    unsigned int wisharti;      /* wished for an artifact */
    /* genocides already listed at end of game */
    unsigned int elbereths;     /* wrote an elbereth */
    unsigned int puddings;      /* split a pudding */
};


/*** Information about the player ***/
struct you {
    xchar ux, uy;
    schar dx, dy;       /* direction of the ongoing multi-turn move */
    xchar tx, ty;       /* destination of travel */
    xchar ux0, uy0;     /* initial position of a move */
    d_level uz, uz0;    /* your level on this and the previous turn */
    d_level utolev;     /* level monster teleported you to, or uz */
    uchar utotype;      /* bitmask of goto_level() flags for utolev */
    boolean umoved;     /* changed map location (post-move) */
    int last_str_turn;  /* 0: none, 1: half turn, 2: full turn */
    /* +: turn right, -: turn left */
    int ulevel;         /* 1 to MAXULEV */
    int ulevelmax;
    unsigned utrap;     /* trap timeout */
    unsigned utraptype; /* defined if utrap nonzero */
# define TT_BEARTRAP    0
# define TT_PIT         1
# define TT_WEB         2
# define TT_LAVA        3
# define TT_INFLOOR     4
    char urooms[5];     /* rooms (roomno + 3) occupied now */
    char urooms0[5];    /* ditto, for previous position */
    char uentered[5];   /* rooms (roomno + 3) entered this turn */
    char ushops[5];     /* shop rooms (roomno + 3) occupied now */
    char ushops0[5];    /* ditto, for previous position */
    char ushops_entered[5];     /* ditto, shops entered this turn */
    char ushops_left[5];        /* ditto, shops exited this turn */

    int uhunger;        /* refd only in eat.c and shk.c */
    unsigned uhs;       /* hunger state - see eat.c */

    struct prop uprops[LAST_PROP + 1];

    unsigned umconf;
    char usick_cause[PL_PSIZ + 20];     /* sizeof "unicorn horn named "+1 */
    unsigned usick_type:2;
# define SICK_VOMITABLE 0x01
# define SICK_NONVOMITABLE 0x02
# define SICK_ALL 0x03

    /* These ranges can never be more than MAX_RANGE (vision.h). */
    int nv_range;       /* current night vision range */
    int xray_range;     /* current xray vision range */

    /* 
     * These variables are valid globally only when punished and blind.
     */
# define BC_BALL  0x01  /* bit mask for ball in 'bc_felt' below */
# define BC_CHAIN 0x02  /* bit mask for chain in 'bc_felt' below */
    int bglyph; /* glyph under the ball */
    int cglyph; /* glyph under the chain */
    int bc_order;       /* ball & chain order [see bc_order() in ball.c] */
    int bc_felt;        /* mask for ball/chain being felt */

    int umonster;       /* hero's "real" monster num */
    int umonnum;        /* current monster number */

    int mh, mhmax, mtimedone;   /* for polymorph-self */
    struct attribs macurr,      /* for monster attribs */
            mamax;      /* for monster attribs */
    int ulycn;          /* lycanthrope type */

    unsigned ucreamed;
    unsigned uswldtim;  /* time you have been swallowed */

    unsigned uswallow:1;        /* true if swallowed */
    unsigned uinwater:1;        /* if you're currently in water (only
                                   underwater possible currently) */
    unsigned uundetected:1;     /* if you're a hiding monster/piercer */
    unsigned mfemale:1; /* saved human value of flags.female */
    unsigned uinvulnerable:1;   /* you're invulnerable (praying) */
    unsigned uburied:1; /* you're buried */
    unsigned uedibility:1;      /* blessed food detection; sense unsafe food */
    /* 1 free bit! */

    unsigned udg_cnt;   /* how long you have been demigod */
    struct u_event uevent;      /* certain events have happened */
    struct u_have uhave;        /* you're carrying special objects */
    struct u_conduct uconduct;  /* KMH, conduct */
    struct attribs acurr,       /* your current attributes (eg. str) */
            aexe,       /* for gain/loss via "exercise" */
            abon,       /* your bonus attributes (eg. str) */
            amax,       /* your max attributes (eg. str) */
            atemp,      /* used for temporary loss/gain */
            atime;      /* used for loss/gain countdown */
    int next_attr_check;        /* number of turns until exerchk can run again */
    align ualign;       /* character alignment */
# define CONVERT        2
# define A_ORIGINAL     1
# define A_CURRENT      0
    aligntyp ualignbase[CONVERT];       /* for ualign conversion record */
    schar uluck, moreluck;      /* luck and luck bonus */
# define Luck   (u.uluck + u.moreluck)
# define LUCKADD        3
    /* added value when carrying luck stone */
# define LUCKMAX        10
    /* on moonlit nights 11 */
# define LUCKMIN        (-10)
    schar uhitinc;
    schar udaminc;
    schar uac;
    uchar uspellprot;   /* protection by SPE_PROTECTION */
    uchar usptime;      /* #moves until uspellprot-- */
    uchar uspmtime;     /* #moves between uspellprot-- */
    int uhp, uhpmax;
    int uen, uenmax;    /* magical energy - M. Stephenson */
    int ugangr; /* if the gods are angry at you */
    int ugifts; /* number of artifacts bestowed */
    int ublessed, ublesscnt;    /* blessing/duration from #pray */
    int umoney0;
    int uexp, urexp;
    int ucleansed;      /* to record moves when player was cleansed */
    int usleep; /* sleeping; monstermove you last started */
    int uinvault;
    struct monst *ustuck;
    struct monst *usteed;
    int ugallop;
    int urideturns;
    int umortality;     /* how many times you died */
    int ugrave_arise;   /* you die and become something aside from a ghost */
    time_t ubirthday;   /* real world time when game began */

    int weapon_slots;   /* unused skill slots */
    int skills_advanced;        /* # of advances made so far */
    xchar skill_record[P_SKILL_LIMIT];  /* skill advancements */
    struct skills weapon_skills[P_NUM_SKILLS];
    boolean twoweap;    /* KMH -- Using two-weapon combat */

    int initrole;       /* starting role (index into roles[]) */
    int initrace;       /* starting race (index into races[]) */
    int initgend;       /* starting gender (index into genders[]) */
    int initalign;      /* starting alignment (index into aligns[]) */

};      /* end of `struct you' */

# define Upolyd (u.umonnum != u.umonster)

#endif /* YOU_H */
