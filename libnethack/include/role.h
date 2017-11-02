/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-02 */
#ifndef ROLE_H
# define ROLE_H

# include "global.h"
# include "attrib.h"
# include "align.h"

/* Flags to control pick_[race,role,gend,align] routines in role.c */
# define PICK_RANDOM    0
# define PICK_RIGID     1

/* used during initialization for race, gender, and alignment
   as well as for character class */
# define ROLE_GENDERS   2       /* number of permitted player genders */
                                /* increment to 3 if you allow neuter roles */
# define ROLE_ALIGNS    3       /* number of permitted player alignments */


struct RoleName {
    const char *m;      /* name when character is male */
    const char *f;      /* when female; null if same as male */
};

struct RoleAdvance {
    /* "fix" is the fixed amount, "rnd" is the random amount */
    xchar infix, inrnd; /* at character initialization */
    xchar lofix, lornd; /* gained per level < urole.xlev */
    xchar hifix, hirnd; /* gained per level >= urole.xlev */
};

/*** Unified structure containing role information ***/
struct Role {
        /*** Strings that name various things ***/
    struct RoleName name;       /* the role's name (from u_init.c) */
    struct RoleName rank[9];    /* names for experience levels (from botl.c) */
    const char *lgod, *ngod, *cgod;     /* god names (from pray.c) */
    const char *filecode;       /* abbreviation for use in file names */
    const char *homebase;       /* quest leader's location (from questpgr.c) */
    const char *intermed;       /* quest intermediate goal (from questpgr.c) */

        /*** Indices of important monsters and objects ***/
    short num,          /* index (PM_) for the role (botl.c) */
          petnum,       /* PM_ of preferred pet (NON_PM == random) */
          ldrnum,       /* PM_ of quest leader (questpgr.c) */
          guardnum,     /* PM_ of quest guardians (questpgr.c) */
          neminum,      /* PM_ of quest nemesis (questpgr.c) */
          enemy1num,    /* specific quest enemies (NON_PM == random) */
          enemy2num;
    char enemy1sym,     /* quest enemies by class (S_) */
         enemy2sym;
    short questarti;    /* index (ART_) of quest artifact (questpgr.c) */

        /*** Bitmasks ***/
    short allow;        /* bit mask of allowed variations */
# define ROLE_RACEMASK  0x0ff8  /* allowable races */
# define ROLE_GENDMASK  0xf000  /* allowable genders */
# define ROLE_MALE      0x1000
# define ROLE_FEMALE    0x2000
# define ROLE_NEUTER    0x4000
# define ROLE_ALIGNMASK AM_MASK /* allowable alignments */
# define ROLE_LAWFUL    AM_LAWFUL
# define ROLE_NEUTRAL   AM_NEUTRAL
# define ROLE_CHAOTIC   AM_CHAOTIC

        /*** Attributes (from attrib.c and exper.c) ***/
    xchar attrbase[A_MAX];      /* lowest initial attributes */
    xchar attrdist[A_MAX];      /* distribution of initial attributes */
    struct RoleAdvance hpadv;   /* hit point advancement */
    struct RoleAdvance enadv;   /* energy advancement */
    xchar xlev; /* cutoff experience level */
    xchar initrecord;   /* initial alignment record */

        /*** Spell statistics (from spell.c) ***/
    int spelbase;       /* base spellcasting penalty */
    int spelheal;       /* penalty (-bonus) for healing spells */
    int spelshld;       /* penalty for wearing any shield */
    int spelarmr;       /* penalty for wearing metal armor */
    int spelstat;       /* which stat (A_) is used */
    int spelspec;       /* spell (SPE_) the class excels at */
    int spelsbon;       /* penalty (-bonus) for that spell */

        /*** Properties in variable-length arrays ***/
    /* intrinsics (see attrib.c) */
    /* initial inventory (see u_init.c) */
    /* skills (see u_init.c) */

        /*** Don't forget to add... ***/
    /* quest leader, guardians, nemesis (monst.c) */
    /* quest artifact (artilist.h) */
    /* quest dungeon definition (dat/Xyz.dat) */
    /* quest text (dat/quest.txt) */
    /* dictionary entries (dat/data.bas) */
};

extern const struct Role roles[];       /* table of available roles */
extern struct Role urole;

# define Role_if(X)     (urole.num == (X))
# define Role_switch    (urole.num)

/*** Unified structure specifying race information ***/

struct Race {
        /*** Strings that name various things ***/
    const char *noun;   /* noun ("human", "elf") */
    const char *adj;    /* adjective ("human", "elven") */
    const char *coll;   /* collective ("humanity", "elvenkind") */
    const char *filecode;       /* code for filenames */
    struct RoleName individual; /* individual as a noun ("man", "elf") */

        /*** Indices of important monsters and objects ***/
    short num,      /* PM_ normally */
          mummynum,     /* PM_ as a mummy */
          zombienum;    /* PM_ as a zombie */

        /*** Bitmasks ***/
    short allow;        /* bit mask of allowed variations */
    short selfmask,     /* your own race's bit mask */
          lovemask,     /* bit mask of always peaceful */
          hatemask;     /* bit mask of always hostile */

        /*** Attributes ***/
    xchar attrmin[A_MAX];       /* minimum allowable attribute */
    xchar attrmax[A_MAX];       /* maximum allowable attribute */
    struct RoleAdvance hpadv;   /* hit point advancement */
    struct RoleAdvance enadv;   /* energy advancement */

        /*** Properties in variable-length arrays ***/
    /* intrinsics (see attrib.c) */

        /*** Don't forget to add... ***/
    /* quest leader, guardians, nemesis (monst.c) */
    /* quest dungeon definition (dat/Xyz.dat) */
    /* quest text (dat/quest.txt) */
    /* dictionary entries (dat/data.bas) */
};

extern const struct Race races[];       /* Table of available races */
extern struct Race urace;

# define Race_if(X)     (urace.num == (X))
# define Race_switch    (urace.num)

/*** Unified structure specifying gender information ***/
struct Gender {
    const char *adj;    /* male/female/neuter */
    const char *he;     /* he/she/it */
    const char *him;    /* him/her/it */
    const char *his;    /* his/her/its */
    const char *filecode;       /* file code */
    short allow;        /* equivalent ROLE_ mask */
};

extern const struct Gender genders[];   /* table of available genders */

# define uhe()          (genders[u.ufemale ? 1 : 0].he)
# define uhim()         (genders[u.ufemale ? 1 : 0].him)
# define uhis()         (genders[u.ufemale ? 1 : 0].his)
# define umhe(mon)      (genders[pronoun_gender(mon)].he)
# define umhim(mon)     (genders[pronoun_gender(mon)].him)
# define umhis(mon)     (genders[pronoun_gender(mon)].his)
# define mhe(mon)       ((mon) == &youmonst ? "you" : umhe(mon))
# define mhim(mon)      ((mon) == &youmonst ? "you" : umhim(mon))
# define mhis(mon)      ((mon) == &youmonst ? "your" : umhis(mon))
# define mfeel(mon)     ((mon) == &youmonst ? "feel" : "looks")

/*** Unified structure specifying alignment information ***/
struct Align {
    const char *noun;   /* law/balance/chaos */
    const char *adj;    /* lawful/neutral/chaotic */
    const char *filecode;       /* file code */
    short allow;        /* equivalent ROLE_ mask */
    aligntyp value;     /* equivalent A_ value */
};

extern const struct Align aligns[];     /* table of available alignments */

#endif /* ROLE_H */
