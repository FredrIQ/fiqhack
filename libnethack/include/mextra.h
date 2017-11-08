/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-08 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) Izchak Miller, 1989.                             */
/* Copyright (c) Fredrik Ljungdahl, 2015.                         */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MEXTRA_H
# define MEXTRA_H

/* This file handles extended monster data. Used for tame monsters,
   names, shopkeepers, temple priests, etc. This used to be split
   into vault.h, epri.h, eshk.h, emin.h and edog.h.
   Note: emin.h had one purpose -- containing an aligntyp for a
   monster. Since all monsters now have this, it is now redundant. */

# include "align.h"
# include "coord.h"
# include "dungeon.h"
# include "flag.h"
# include "global.h"
# include "nethack_types.h"


/* Vault guards */
# define FCSIZ (ROWNO+COLNO)
struct fakecorridor {
    xchar fx, fy, ftyp;
};

struct egd {
    int fcbeg, fcend;   /* fcend: first unused pos */
    int vroom;          /* room number of the vault */
    xchar gdx, gdy;     /* goal of guard's walk */
    xchar ogx, ogy;     /* guard's last position */
    d_level gdlevel;    /* level (& dungeon) guard was created in */
    xchar warncnt;      /* number of warnings to follow */
    unsigned gddone:1;  /* true iff guard has released player */
    struct fakecorridor fakecorr[FCSIZ];
};


/* Priests */
struct epri {
    schar shroom;       /* index in rooms */
    coord shrpos;       /* position of shrine */
    d_level shrlevel;   /* level (& dungeon) of shrine */
};

# define ispriest(mon) (mx_epri(mon) && mx_epri(mon)->shroom)
# define isminion(mon) (mx_epri(mon) && !ispriest(mon))

/* Shopkeepers */
# define REPAIR_DELAY   5       /* minimum delay between shop damage & repair */
# define BILLSZ         200

struct bill_x {
    unsigned bo_id;
    boolean useup;
    int price;          /* price per unit */
    int bquan;          /* amount used up */
};

struct eshk {
    struct bill_x bill[BILLSZ];
    /* SAVEBREAK (4.3-beta1 -> 4.3-beta2): make this one byte; no
       longer needs to hold dummy data. Also flip the meaning. */
    int bill_inactive;
    coord shk;          /* usual position shopkeeper */
    coord shd;          /* position shop door */
    int robbed;         /* amount stolen by most recent customer */
    int credit;         /* amount credited to customer */
    int debit;          /* amount of debt for using unpaid items */
    int loan;           /* shop-gold picked (part of debit) */
    short shoptype;     /* the value of rooms[shoproom].rtype */
    short billct;       /* no. of entries of bill[] in use */
    short visitct;      /* nr of visits by most recent customer */
    d_level shoplevel;  /* level (& dungeon) of his shop */
    schar shoproom;     /* index in rooms; set by inshop() */
    boolean following;  /* following customer since he owes us sth */
    boolean surcharge;  /* angry shk inflates prices */
    char customer[PL_NSIZ];     /* most recent customer */
};


/* Pets (generally -- minions are special and lacks edog) */

/* Dog food usability. Higher is better. */
enum dogfood {
    df_nofood,
    df_tabu,
    df_harmful,
    df_apport,
    df_manfood,
    df_acceptable,
    df_good,
    df_treat,
};

struct edog {
    unsigned int droptime;      /* moment dog dropped object */
    unsigned int dropdist;      /* dist of drpped obj from @ */
    int apport; /* amount of training */
    unsigned int whistletime;   /* last time he whistled */
    unsigned int hungrytime;    /* will get hungry at this time */
    int abuse;  /* track abuses to this pet */
    int revivals;       /* count pet deaths */
    int mhpmax_penalty; /* while starving, points reduced */
    unsigned killed_by_u:1;     /* you attempted to kill him */
};

struct pet_weapons {
    struct monst *mon;
    int uses_weapons;
    const struct obj *wep;
    const struct obj *hwep;
    const struct obj *rwep;
    const struct obj *proj;
    const struct obj *pick;
    const struct obj *unihorn;
};

/* New "u" replacement. */
struct eyou {
    int last_pray_action; /* prayer/artifact/etc turncount */
    enum pray_type prayed_result;
};

/* mextra struct itself */
struct mextra {
    char *name;
    struct egd *egd;
    struct epri *epri;
    struct eshk *eshk;
    struct emin *emin;
    struct edog *edog;
    struct eyou *eyou;
};

struct oextra {
    char *name;
    struct monst *monst;
};

#endif /* MEXTRA_H */
