/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef FLAG_H
# define FLAG_H

# include "nethack_types.h"
# include "global.h"
# include "coord.h"
# include "objclass.h"
# include "prop.h"
# include "youprop.h"
# include "xmalloc.h"
# include "dungeon.h"
# include "rnd.h"

/* Argument to functions that handle attack-like actions, that specifies the
   extent to which the player is trying to interact with monsters; and to
   movement functions, that specifies the extent to which the player is trying
   to interact with items. Not all appropriate commands use this mechanism yet.

   This is also used by the "movecommand" game option. As such, all the enum
   entries should start with a different letter (to simplify the user interface
   for options), and you need to update the list in options.c whenever you
   change this list.

   Commands are able to override the interaction mode downwards, but not
   upwards; overrides are often needed due to the semantics of the command
   (e.g. farmove shouldn't stay in place trying to attack monsters). The
   exception is the F command, which overrides absolutely anything.

   When changing this list, update resolve_uim in hack.c. */
enum u_interaction_mode {
    /* from approximate order of least interactive to most interactive: */
    uim_nointeraction,  /* Do not interact with anything, monster or item. Used
                           by most farmoves, travel, and the 'm' command. */
    uim_onlyitems,      /* Cancel the action if any monster is in the way;
                           interact with items as normal. User-specified only.
                           This mode was inspired by Tjr. */
    uim_displace,       /* Do not interact with monsters except via displacing
                           them. Interact with items as normal. Used by the more
                           aggressive farmoves (i.e. shift-direction), and
                           autoexplore. */
    uim_pacifist,       /* Cancel attacks on monsters. Anything other than an
                           attack is OK (e.g. chat, displace). User-specified
                           only. Intended for "normal" pacifist play. */
    uim_attackhostile,  /* Attack hostile monsters. Always cancel attacks on
                           peacefuls (always-peacefuls are chatted to, like
                           normal). Interact with items as normal. This is
                           user-specified only, and designed for regular use for
                           players who dislike attacking peacefuls, and for
                           anyone trying to achieve moral conduct. */
    uim_traditional,    /* Attack if hostile. Prompt about whether to attack
                           peacefuls. Interact with monsters only by attacking,
                           but with items as normal. This is user-specified only,
                           and intended for all those crazy foodless players who
                           liked to turn safe_pet off in previous versions. */
    uim_standard,       /* Attack hostiles, chat to always-peacefuls, interact
                           with items as normal, etc.. The default interaction
                           mode, so although it's user-specified only, most
                           users will in fact specify it. */
    uim_indiscriminate, /* Attack any monster that appears to be there, even if
                           peaceful, tame, etc.. Don't hit an unseen monster
                           unless there's an invisible monster marker there.
                           Interact with items as normal; move if nothing seems
                           to be there. This corresponds to "confirm" off in
                           3.4.3. User-specified only. */
    uim_forcefight,     /* Always attack the square, no matter what; used by the
                           F command (overriding user preferences). The regular
                           movement commands respect the user preferences, so
                           they can be set as high as this too. */
};

/* The various functions of the movement keys. The uim chooses which of these we
   use. This list is for what the player believes that they're doing. If
   they're impaired, or when monsters are hiding, the option listed here might
   not be possible. */
enum u_interaction_attempt {
    uia_move_nopickup, /* move to the square, don't do anything else */
    uia_move_pickup,   /* move to the square, and run autopickup as normal */
    uia_opendoor,      /* open a door on the square */
    uia_pushboulder,   /* push a boulder on the square */
    uia_chat,          /* chat to a monster on the square */
    uia_displace,      /* swap places with a monster on the square */
    uia_attack,        /* attack a monster on the square */
    uia_search,        /* search the square (e.g. checking mem_invis) */
    uia_halt,          /* do nothing / cancel the action */
};


enum pray_type {
    pty_invalid = 0,
    pty_too_soon,
    pty_anger,
    pty_conversion,
    pty_favour,
    pty_smite_undead
};

enum pray_trouble {
    ptr_invalid = 0,

    ptr_first_major,
    ptr_stoned = ptr_first_major,
    ptr_slimed,
    ptr_strangled,
    ptr_lava,
    ptr_sick,
    ptr_starving,
    ptr_hit,
    ptr_lycanthrope,
    ptr_collapsing,
    ptr_stuck,
    ptr_levitation,
    ptr_hands,
    ptr_blindfold,
    ptr_last_major = ptr_blindfold,

    ptr_first_minor,
    ptr_punished = ptr_first_minor,
    ptr_fumbling,
    ptr_cursed,
    ptr_saddle,
    ptr_blind,
    ptr_poisoned,
    ptr_wounded_legs,
    ptr_hungry,
    ptr_stunned,
    ptr_confused,
    ptr_hallucinating,
    ptr_last_minor = ptr_hallucinating,
};


/* Information that is reset every time the user has a chance to give commands
   (from default_turnstate).  This is never saved in a binary save; if it needs
   to be reconstructed, the reconstruction is done via replaying user actions
   from the last command (when it had a known value.) */
struct turnstate {
    /* Objects that might potentially be destroyed or otherwise changed during
       a turn. */
    struct obj *tracked[ttos_last_slot + 1];
    /* Messages (and a few other things, like menu selections) that are
       scheduled for deallocation at the end of the turn. */
    struct xmalloc_block *message_chain;

    /* TRUE if continuing a multi-turn action should print a message. */
    boolean continue_message;
    /* TRUE if we need to do a complete vision recalculation. */
    boolean vision_full_recalc;
    /* TRUE if we should avoid flushing the display buffer. */
    boolean delay_flushing;
    /* TRUE while generating bones. (This is allowed to consume random numbers
       that aren't logged in the save file, which is an exception to the normal
       logging discipline.) */
    boolean generating_bones;

    /* Pets migrating with their owner between levels. */
    struct monst *migrating_pets;
    /* Objects falling between levels. */
    struct obj *migrating_objs;
    /* Objects not otherwise on any chain. */
    struct obj *floating_objects;

    /* timers for helplessness */
    unsigned helpless_timers[hr_last + 1];
    /* causes of helplessness */
    char helpless_causes[hr_last + 1][BUFSZ];
    /* messages to print when helplessness ends */
    char helpless_endmsgs[hr_last + 1][BUFSZ];
    /* prayer state */
    struct prayer_info {
        aligntyp align;
        enum pray_trouble trouble;
        enum pray_type type;
    } pray;

    /* When the character is stunned or confused, they can end up going in a
       direction they didn't intend. In such cases, the direction-prompt
       commands will randomize their output. However, we record the direction
       that the character was /trying/ to move in for the purpose of things
       like working out whether they're trying to move or attack. */
    schar intended_dx;
    schar intended_dy;

    /* State for farmove-related stuff. */
    struct move_info {
        /* which direction we're going */
        schar dx, dy;
        /* Squares that should halt running if entered.  Initially, this array
           is all FALSE; individual entries are set to TRUE when the
           corresponding space is stepped on.  If running would enter a TRUE
           space, it stops.  This should prevent any infinite run loops. */
        boolean stepped_on[COLNO][ROWNO];
    } move;

    /* Level changes scheduled for the end of turn after all monsters move. */
    struct deferred_goto_info {
        d_level dlevel;
        uchar flags;
        char pre_msg[BUFSZ], post_msg[BUFSZ];
    } goto_info;
};

extern struct turnstate turnstate;

/* Persistent flags that are saved and restored with the game. */

struct flag {
    /* === GAME OPTIONS === */
    /* For historical reasons, there are some non-options mixed in with these.
       TODO: Move them elsewhere (turnstate is a good place, but they don't all
       fit there). */

    /* 8 bit values: enums */
    enum u_interaction_mode interaction_mode;
    enum {
        saveenc_unencoded = 0,  /* relative to 0 */
        saveenc_moverel = 1,    /* relative to moves */
        saveenc_levelrel = 2    /* relative to level->lastmoves */
    } save_encoding;    /* allows safe conversion of old saves */

# define DISCLOSE_PROMPT_DEFAULT_YES    'y'
# define DISCLOSE_PROMPT_DEFAULT_NO     'n'
# define DISCLOSE_YES_WITHOUT_PROMPT    '+'
# define DISCLOSE_NO_WITHOUT_PROMPT     '-'
    char end_disclose;  /* disclose various info upon exit */
    char menu_style;    /* User interface style setting */

    /* 8 bit values: numerical */
    xchar actions;      /* number of actions that have happened this turn */

    /* 1 bit values: booleans */
    boolean autodig;    /* MRKR: Automatically dig */
    boolean autodigdown;        /* autodigging works downwadrds */
    boolean autoquiver; /* Automatically fill quiver */
    boolean beginner;
    boolean bypasses;   /* bypass flag is set on at least one fobj */
    boolean cblock;
    boolean corridorbranch;     /* branching corridors are interesting */
    boolean debug;      /* in debugging mode */
# define wizard  flags.debug
    boolean desync;     /* set this to desync the save */
    boolean explore;    /* in exploration mode */
# define discover flags.explore
    boolean friday13;   /* it's Friday the 13th */
    boolean legacy;     /* print game entry "story" */
    boolean made_amulet;
    boolean mon_generation;     /* debug: control monster generaion */
    boolean mon_moving; /* monsters' turn to move */
    boolean mon_polycontrol;    /* debug: control monster polymorphs */
    boolean incomplete; /* the requested action continues into future turns */
    boolean interrupted;/* something happened to make long actions stop */
    boolean pickup;     /* whether you pickup or move and look */
    boolean pickup_thrown;      /* auto-pickup items you threw */
    boolean prayconfirm;        /* confirm before praying */
    boolean pushweapon; /* When wielding, push old weapon into second slot */
    boolean showrace;   /* show hero glyph by race rather than by role */
    boolean show_uncursed;      /* always show uncursed items as such */
    boolean sortpack;   /* sorted inventory */
    boolean sparkle;    /* show "resisting" special FX (Scott Bigham) */
    boolean tombstone;  /* print tombstone */
    boolean travel_interrupt;   /* Interrupt travel if there is a hostile *
                                   monster in sight. */
    boolean verbose;    /* max battle info */

    /* 32-bit values: integers, etc. */
    int timezone;       /* UTC plus this many seconds */
    int polyinit_mnum;  /* polyinit to this monster, -1 = no polyinit */
    unsigned ident;     /* social security number for each monster/object */
# define TEMPORARY_IDENT 1 /* identification number for temporary things */
# define FIRST_PERMANENT_IDENT 2 /* lower values are temporaries */
    unsigned moonphase;
# define NEW_MOON       0
# define FULL_MOON      4
    unsigned no_of_wizards;     /* 0, 1 or 2 (wizard and his shadow) */
    int djinni_count, ghost_count;      /* potion effect tuning */
    int pickup_burden;  /* maximum burden before prompt */
    int recently_broken_otyp;   /* object that broke recently */

    /* Weird-sized structures */
    struct nh_autopickup_rules *ap_rules;
    char inv_order[MAXOCLASSES];
    char setseed[RNG_SEED_SIZE_BASE64]; /* not NUL-terminated */

    /* Note: for backwards compatibility, this has its own section in the save
       file. */
    unsigned char rngstate[RNG_SEEDSPACE];

    /* === MULTI-TURN COMMAND STATE === */

    /* This state needs to be in the save file, because it persists to the start
       of the next command (rather than the end of the current command, like
       turnstate).

       last_cmd is always the last command that the user entered (and is set to
       the command currently being processed, while a command is being
       processed). last_arg is initially the argument that the user entered for
       that command, but it is modified in response to user input (i.e. if the
       server prompts for an argument, it's handled the same way as if the
       client had volunteered it.)

       Whenever last_arg.argtype & CMD_ARG_STR, last_arg.str must point to
       last_str_buf, in order to handle memory access for the arguments
       correctly. last_str_buf itself is always either NULL or malloc'ed; it
       does not necessarily have to be deallocated once last_arg.str is no
       longer relevant, although it can be. */
    int last_cmd;                             /* this or previous command */
    struct nh_cmd_arg last_arg;              /* this or previous argument */
    char *last_str_buf;       /* mutable last_arg.str, so it can be freed */
    enum occupation occupation; /* internal code for a multi-turn command */
    coord travelcc;                      /* previously traveled-to square */

    /* The current time is not part of the multi-turn command state, but has
       identical save properties (it persists to the start of the next
       command). */
    microseconds turntime;

    /* === BIRTH OPTIONS === */
    boolean elbereth_enabled;   /* should the E-word repel monsters? */
    boolean rogue_enabled;      /* create a rogue level */
    boolean seduce_enabled;     /* succubus seduction */
    boolean bones_enabled;      /* allow loading bones levels */
    boolean permablind;         /* stay permanently blind */
    boolean permahallu;         /* stay permanently hallucinating */
};


extern struct flag flags;

#endif /* FLAG_H */
