/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DUNGEON_H
# define DUNGEON_H

# include "global.h"

typedef struct d_flags {        /* dungeon/level type flags */
    unsigned town:1;    /* is this a town? (levels only) */
    unsigned hellish:1; /* is this part of hell? */
    unsigned maze_like:1;       /* is this a maze? */
    unsigned rogue_like:1;      /* is this an old-fashioned presentation? */
    unsigned align:3;   /* dungeon alignment. */
    unsigned unused:1;  /* etc... */
} d_flags;

typedef struct d_level {        /* basic dungeon level element */
    xchar dnum; /* dungeon number */
    xchar dlevel;       /* level number */
} d_level;

typedef struct s_level {        /* special dungeon level element */
    struct s_level *next;
    d_level dlevel;     /* dungeon & level numbers */
    char proto[15];     /* name of prototype file (eg. "tower") */
    char boneid;        /* character to id level in bones files */
    uchar rndlevs;      /* no. of randomly available similar levels */
    d_flags flags;      /* type flags */
} s_level;

typedef struct stairway {       /* basic stairway identifier */
    xchar sx, sy;       /* x / y location of the stair */
    d_level tolev;      /* where does it go */
    char up;    /* what type of stairway (up/down) */
} stairway;

/* level region types */
# define LR_DOWNSTAIR 0
# define LR_UPSTAIR 1
# define LR_PORTAL 2
# define LR_BRANCH 3
# define LR_TELE 4
# define LR_UPTELE 5
# define LR_DOWNTELE 6

typedef struct dest_area {      /* non-stairway level change indentifier */
    schar lx, ly;       /* "lower" left corner (near [0,0]) */
    schar hx, hy;       /* "upper" right corner (near [COLNO,ROWNO]) */
    schar nlx, nly;     /* outline of invalid area */
    schar nhx, nhy;     /* opposite corner of invalid area */
} dest_area;

typedef struct dungeon {        /* basic dungeon identifier */
    char dname[24];     /* name of the dungeon (eg. "Hell") */
    char proto[15];     /* name of prototype file (eg. "tower") */
    char boneid;        /* character to id dungeon in bones files */
    d_flags flags;      /* dungeon flags */
    xchar entry_lev;    /* entry level */
    xchar num_dunlevs;  /* number of levels in this dungeon */
    xchar dunlev_ureached;      /* how deep you have been in this dungeon */
    int ledger_start,   /* the starting depth in "real" terms */
        depth_start;    /* the starting depth in "logical" terms */
} dungeon;

/*
 * A branch structure defines the connection between two dungeons.  They
 * will be ordered by the dungeon number/level number of 'end1'.  Ties
 * are resolved by 'end2'.  'Type' uses 'end1' arbitrarily as the primary
 * point.
 */
typedef struct branch {
    struct branch *next;        /* next in the branch chain */
    int id;     /* branch identifier */
    int type;   /* type of branch */
    d_level end1;       /* "primary" end point */
    d_level end2;       /* other end point */
    boolean end1_up;    /* does end1 go up? */
} branch;

/* branch types */
# define BR_STAIR   0   /* "Regular" connection, 2 staircases. */
# define BR_NO_END1 1   /* "Regular" connection.  However, no stair from */
                        /* end1 to end2.  There is a stair from end2 */
                        /* to end1.  */
# define BR_NO_END2 2   /* "Regular" connection.  However, no stair from */
                        /* end2 to end1.  There is a stair from end1 */
                        /* to end2.  */
# define BR_PORTAL  3   /* Connection by magic portals (traps) */


/* dungeon overview information, generated and used by dooverview */
struct overview_info {
    char fountains, sinks, thrones, trees, temples;
    char altars;              /* number of non-high altars */
    char high_altars;         /* number of high altars */
    boolean lawful_altar;     /* lawful altar present */
    boolean neutral_altar;    /* neutral altar present */
    boolean chaotic_altar;    /* chaotic altar present */
    boolean unaligned_altar;  /* altar to Moloch present */
    char shopcount;
    schar shoptype;    /* -1: multiple shops */
    boolean branch, portal;     /* branch, magic portal on this level */
    struct d_level branch_dst, portal_dst;      /* where to? */
    boolean branch_dst_known, portal_dst_known; /* destination known? */
};


/* A particular dungeon contains num_dunlevs d_levels with dlevel 1..
 * num_dunlevs.  Ledger_start and depth_start are bases that are added
 * to the dlevel of a particular d_level to get the effective ledger_no
 * and depth for that d_level.
 *
 * Ledger_no is a bookkeeping number that gives a unique identifier for a
 * particular d_level (for level.?? files, e.g.).
 *
 * Depth corresponds to the number of floors below the surface.
 */
# define Is_astralevel(x)       (on_level(x, &astral_level))
# define Is_earthlevel(x)       (on_level(x, &earth_level))
# define Is_waterlevel(x)       (on_level(x, &water_level))
# define Is_firelevel(x)        (on_level(x, &fire_level))
# define Is_airlevel(x)         (on_level(x, &air_level))
# define Is_firstplane(x)       Is_earthlevel(x)
# define Is_medusa_level(x)     (on_level(x, &medusa_level))
# define Is_oracle_level(x)     (on_level(x, &oracle_level))
# define Is_valley(x)           (on_level(x, &valley_level))
# define Is_juiblex_level(x)    (on_level(x, &juiblex_level))
# define Is_asmo_level(x)       (on_level(x, &asmodeus_level))
# define Is_baal_level(x)       (on_level(x, &baalzebub_level))
# define Is_wiz1_level(x)       (on_level(x, &wiz1_level))
# define Is_wiz2_level(x)       (on_level(x, &wiz2_level))
# define Is_wiz3_level(x)       (on_level(x, &wiz3_level))
# define Is_sanctum(x)          (on_level(x, &sanctum_level))
# define Is_portal_level(x)     (on_level(x, &portal_level))
# define Is_rogue_level(x)      (on_level(x, &rogue_level))
# define Is_stronghold(x)       (on_level(x, &stronghold_level))
# define Is_bigroom(x)          (on_level(x, &bigroom_level))
# define Is_qstart(x)           (on_level(x, &qstart_level))
# define Is_qlocate(x)          (on_level(x, &qlocate_level))
# define Is_nemesis(x)          (on_level(x, &nemesis_level))
# define Is_knox(x)             (on_level(x, &knox_level))

# define In_sokoban(x)          ((x)->dnum == sokoban_dnum)
# define Inhell                 In_hell(&u.uz)  /* now gehennom */
# define In_endgame(x)          ((x)->dnum == astral_level.dnum)

# define within_bounded_area(X,Y,LX,LY,HX,HY)                   \
    ((X) >= (LX) && (X) <= (HX) && (Y) >= (LY) && (Y) <= (HY))

# define isok(x, y) ((x) >= 0 && (x) <= COLNO-1 && (y) >= 0 && (y) <= ROWNO-1)

/* monster and object migration codes */

# define MIGR_NOWHERE           (-1)    /* failure flag for down_gate() */
# define MIGR_RANDOM            0
# define MIGR_APPROX_XY         1       /* approximate coordinates */
# define MIGR_EXACT_XY          2       /* specific coordinates */
# define MIGR_STAIRS_UP         3
# define MIGR_STAIRS_DOWN       4
# define MIGR_LADDER_UP         5
# define MIGR_LADDER_DOWN       6
# define MIGR_SSTAIRS           7       /* dungeon branch */
# define MIGR_PORTAL            8       /* magic portal */
# define MIGR_NEAR_PLAYER       9       /* mon: followers; obj: trap door */

#endif /* DUNGEON_H */

