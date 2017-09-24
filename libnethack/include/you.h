/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-09-24 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef YOU_H
# define YOU_H

# include "global.h"
# include "attrib.h"
# include "monst.h"
# include "prop.h"     /* for makedefs, enum objslot */
# include "skills.h"
# include "dungeon.h"
# include "quest.h"
# include "youprop.h"  /* for conducts */

# include <time.h>


/*** Substructures ***/

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

/*** Information about the player ***/
struct you {
    /* NOW UNUSED */
    int unused_oldcap;  /* carry cap on previous turn */
    int unused_uen, unused_uenmax; /* magical energy (Pw) */

    xchar ux, uy;
    xchar tx, ty;       /* destination of travel */
    xchar ux0, uy0;     /* initial position of a move */
    d_level uz;    /* your level on this and the previous turn */
    boolean umoved;     /* changed map location (post-move) */
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

    struct obj *utracked[tos_last_slot + 1];      /* occupation objects */
    int uoccupation_progress[tos_last_slot + 1];  /* time spent on occupation */
    coord utracked_location[tl_last_slot + 1];    /* occupation locations */

    unsigned umconf;

    /* uwhybusy is the reason for an occupation occuring (e.g.  "You stop
       uwhybusy.") This is undefined when flags.occupation is zero.
    
       TODO: This needs a better memory allocation scheme. */
    char uwhybusy[BUFSZ];

# define SICK_VOMITABLE 0x01
# define SICK_NONVOMITABLE 0x02
# define SICK_ALL 0x03

    /* This range can never be more than MAX_RANGE (vision.h). */
    int nv_range;       /* current night vision range */

    /* These variables are valid globally only when punished and blind. */
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
    unsigned ufemale:1;         /* no neutral gender for players normally */
    unsigned mfemale:1;         /* saved value of ufemale */
    unsigned uinvulnerable:1;   /* you're invulnerable (praying) */
    unsigned uburied:1;         /* you're buried */
    unsigned uedibility:1;      /* blessed food detection; sense unsafe food */

    unsigned uwelcomed:1;       /* you've seen the "Welcome to NetHack!"
                                   message (and legacy if appropriate) */
    unsigned usick_type:2;

    unsigned udg_cnt;   /* how long you have been demigod */
    struct u_event uevent;      /* certain events have happened */
    int uconduct[num_conducts];       /* KMH, conduct */
    int uconduct_time[num_conducts];  /* when each conduct was first broken */

    /* Track which properties the character has /ever/ had, for the xlog */
    uchar ever_extrinsic[(LAST_PROP + 7) / 8];
    uchar ever_intrinsic[(LAST_PROP + 7) / 8];
    uchar ever_temporary[(LAST_PROP + 7) / 8];

    struct attribs acurr,       /* your current attributes (eg. str) */
            aexe,       /* for gain/loss via "exercise" */
            amax,       /* your max attributes (eg. str) */
            atemp,      /* used for temporary loss/gain */
            atime;      /* used for loss/gain countdown */
    int next_attr_check; /* number of turns until exerchk can run again */
    align ualign;       /* character alignment */
# define CONVERT        2
# define A_ORIGINAL     1
# define A_CURRENT      0
    aligntyp ualignbase[CONVERT];       /* for ualign conversion record */
    int upantheon;
    schar uluck, moreluck;      /* luck and luck bonus */
# define Luck   (u.uluck + u.moreluck)
# define LUCKADD        3
    /* added value when carrying luck stone */
# define LUCKMAX        10
    /* on moonlit nights 11 */
# define LUCKMIN        (-10)
    int uhp, uhpmax;
    int ugangr; /* if the gods are angry at you */
    int ugifts; /* number of artifacts bestowed */
    int ublessed, ublesscnt;    /* blessing/duration from #pray */
    int umoney0; /* Starting gold, for score calculation purposes */
    int uexp, urexp;
    int ucleansed;      /* to record moves when player was cleansed */
    int uinvault;
    struct monst *ustuck;
    struct monst *usteed;
    int ugallop;
    int urideturns;
    xchar moveamt;      /* your movement ration for this turn */
    int umortality;     /* how many times you died */
    int ugrave_arise;   /* you die and become something aside from a ghost */
    microseconds ubirthday;         /* real world UTC time when game began */

    int weapon_slots;   /* unused skill slots */
    int skills_advanced;        /* # of advances made so far */
    xchar skill_record[P_SKILL_LIMIT];  /* skill advancements */
    struct skills weapon_skills[P_NUM_SKILLS];
    boolean twoweap;    /* KMH -- Using two-weapon combat */
    boolean bashmsg;    /* control for the "begin bashing mosnters" message */

    int initrole;       /* starting role (index into roles[]) */
    int initrace;       /* starting race (index into races[]) */
    int initgend;       /* starting gender (index into genders[]) */
    int initalign;      /* starting alignment (index into aligns[]) */

    struct q_score quest_status;

    char uplname[PL_NSIZ];       /* player name */

    /* These pointers own the strings pointed to since they persist across turns
       on occasion (allocation is via malloc). Only the code in end.c, as well
       as save/restore code, should manipulate these directly. */
    struct {
        char *stoning;
        char *sliming;
        char *illness;
        char *genocide;
        char *zombie;
    } delayed_killers;

    int lastinvnr;
    int spellquiver;

    /* SAVEBREAK: for avoiding desyncs with old saves */
    unsigned char save_compat_bytes[3];

};      /* end of `struct you' */

# define Upolyd (u.umonnum != u.umonster)

#endif /* YOU_H */
