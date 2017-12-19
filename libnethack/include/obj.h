/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef OBJ_H
# define OBJ_H

# include "global.h"

/* New oextra defines */
# define OX_NAME  0x01
# define OX_MONST 0x02

/* Old oextra defines. Kept for save compatibility */
# define OATTACHED_NOTHING 0
# define OATTACHED_MONST   1
# define OATTACHED_M_ID    2
# define OATTACHED_NEW 3 /* Originally UNUSED3, now used to mark non-legacy save */

enum obj_where {
    OBJ_FREE,   /* object not attached to anything */
    OBJ_FLOOR,  /* object on floor */
    OBJ_CONTAINED,      /* object in a container */
    OBJ_INVENT, /* object in the hero's inventory */
    OBJ_MINVENT,        /* object in a monster inventory */
    OBJ_BURIED, /* object buried */
    OBJ_ONBILL, /* object on shk bill */
    OBJ_MIGRATING, /* object in limbo between levels */
    NOBJ_STATES
};

enum erode_type {
    ERODE_BURN,
    ERODE_RUST,
    ERODE_ROT,
    ERODE_CORRODE,
};

struct obj {
    struct obj *nobj;
    union {
        struct obj *nexthere;     /* floor location lists */
        struct obj *ocontainer;   /* point back to container */
        struct monst *ocarry;     /* point back to carrying monst */
    };

    struct obj *cobj;   /* contents list for containers */
    struct oextra *oextra; /* extra object data */
    unsigned int o_id;
    /* Points to the remembered object or vice versa */
    struct obj *mem_obj;
    /* Used during save/restore */
    unsigned int mem_o_id;
    unsigned int m_id; /* monster ID for temporary monsters and bones ghosts */
    struct level *olev; /* the level it is on */
    xchar ox, oy;
    short otyp; /* object class number */
    unsigned owt;
    int quan;   /* number of items */

    schar spe;  /*
                 * quality of weapon, armor or ring (+ or -)
                 * number of charges for wand ( >= -1 )
                 * marks your eggs, spinach tins
                 * royal coffers for a court ( == 2)
                 * tells which fruit a fruit is
                 * special for uball and amulet
                 * historic and gender for statues
                 * last used movement for a stethoscope
                 */
# define OPM_HISTORIC 0x01 /* statues only */
# define OPM_SPINACH  0x01 /* tins only */
# define OPM_YOULAID  0x01 /* eggs only */
# define OPM_MALE     0x02
# define OPM_FEMALE   0x04
# define OPM_NEUTER   0x06
# define OPM_HOMEMADE 0x08 /* tins only */
    char oclass;                /* object class */
    char invlet;                /* designation in inventory */
    unsigned char oartifact;    /* artifact array index */

    xchar where;                /* where the object thinks it is */
    xchar timed;                /* # of fuses (timers) attached to this obj */

    unsigned cursed:1;
    unsigned blessed:1;
    unsigned unpaid:1;          /* on some bill */
    unsigned no_charge:1;       /* if shk shouldn't charge for this */
    /* player knows ID */
    unsigned known:1;           /* exact nature known */
    unsigned dknown:1;          /* color or text known */
    unsigned bknown:1;          /* blessing or curse known */
    unsigned rknown:1;          /* rustproof or not known */
    /* monster knows ID (applies to all monsters) */
    unsigned mknown:1;          /* enchantment/charge/container content known */
    unsigned mbknown:1;         /* monster BUC/trapped known */

    unsigned oeroded:2;         /* rusted/burnt weapon/armor */
    unsigned oeroded2:2;        /* corroded/rotted weapon/armor */
# define greatest_erosion(otmp) \
    (int)((otmp)->oeroded > (otmp)->oeroded2 ? (otmp)->oeroded \
                                             : (otmp)->oeroded2)
# define MAX_ERODE 3
# define orotten oeroded        /* rotten food */
# define odiluted oeroded       /* diluted potions */
# define norevive oeroded2
    unsigned oerodeproof:1;     /* erodeproof weapon/armor */
    unsigned olocked:1;         /* object is locked */
    unsigned obroken:1;         /* lock has been broken */
    unsigned otrapped:1;        /* container is trapped */
    /* or accidental tripped rolling boulder trap */
# define opoisoned otrapped     /* object (weapon) is coated with poison */

    unsigned recharged:3;       /* number of times it's been recharged */
    unsigned lamplit:1;         /* a light-source -- can be lit */
    unsigned greased:1;         /* covered with grease */

    unsigned in_use:1;          /* for magic items before useup items */
    unsigned was_thrown:1;      /* thrown by the hero since last picked up */
    unsigned was_dropped:1;     /* last left inventory via d or D command */
    unsigned to_be_hit:1;       /* to be hit by bhitpile */
    unsigned memory:2;  /* object memory state */
# define OM_NO_MEMORY   0 /* not an object memory */
# define OM_MEMORY_OK   1 /* memory state is OK as far as we know */
# define OM_MEMORY_LOST 2 /* we lost it, deallocate when mem_obj is NULL */
    unsigned cknown:1;  /* container has been investigated */
    /* No free bits */

    union {
        int corpsenm;           /* type of corpse is mons[corpsenm] */
        int leashmon;           /* gets m_id of attached pet */
        int fromsink;           /* a potion from a sink */
        int lastused;           /* last time a tool was used */
    };
    unsigned oeaten;            /* nutrition left in food, if partly eaten */
    int thrown_time;            /* when an item was thrown to a pet */

    int age;                    /* creation date */
    int owornmask;
    unsigned nomerge:1;         /* always 0 at neutral turnstate */
    uint64_t oprops;            /* object properties */
    uint64_t oprops_known;      /* IDed properties from above */
};

#define opm_none         0x0000000000000000LLU
#define opm_reflects     0x0000000000000001LLU
#define opm_search       0x0000000000000002LLU
#define opm_hunger       0x0000000000000004LLU
#define opm_stealth      0x0000000000000008LLU
#define opm_telepat      0x0000000000000010LLU
#define opm_fumble       0x0000000000000020LLU
#define opm_warn         0x0000000000000040LLU
#define opm_aggravate    0x0000000000000080LLU
#define opm_fire         0x0000000000000100LLU
#define opm_frost        0x0000000000000200LLU
#define opm_shock        0x0000000000000400LLU
#define opm_vorpal       0x0000000000000800LLU
#define opm_drain        0x0000000000001000LLU
#define opm_detonate     0x0000000000002000LLU
#define opm_speed        0x0000000000004000LLU
#define opm_oilskin      0x0000000000008000LLU
#define opm_power        0x0000000000010000LLU
#define opm_dexterity    0x0000000000020000LLU
#define opm_brilliance   0x0000000000040000LLU
#define opm_displacement 0x0000000000080000LLU
#define opm_clairvoyant  0x0000000000100000LLU
#define opm_mercy        0x0000000000200000LLU
#define opm_carrying     0x0000000000400000LLU
#define opm_nasty        0x0000000000800000LLU
#define opm_all          0x0000000000ffffffLLU
/* Don't have any properties that depend on update_property in
   potential weapon slots, weapon properties aren't updated
   properly. */
#define opm_any (opm_reflects | opm_search | opm_hunger |       \
                 opm_stealth | opm_telepat | opm_warn |         \
                 opm_aggravate | opm_oilskin)
#define opm_aweparm (opm_fire | opm_frost | opm_shock | opm_drain)
#define opm_mwep (opm_any | opm_aweparm | opm_vorpal |  \
                  opm_mercy | opm_nasty)
#define opm_ammo (opm_aweparm | opm_detonate)
#define opm_spe (opm_dexterity | opm_brilliance)
#define opm_armor (opm_any | opm_aweparm | opm_spe |            \
                   opm_speed | opm_power | opm_displacement |   \
                   opm_clairvoyant | opm_fumble | opm_carrying)
#define opm_jewelry (opm_any | opm_spe)


/* Weapons and weapon-tools */
/* KMH -- now based on skill categories.  Formerly:
 *      #define is_sword(otmp)  (otmp->oclass == WEAPON_CLASS && \
 *                       objects[otmp->otyp].oc_wepcat == WEP_SWORD)
 *      #define is_blade(otmp)  (otmp->oclass == WEAPON_CLASS && \
 *                       (objects[otmp->otyp].oc_wepcat == WEP_BLADE || \
 *                        objects[otmp->otyp].oc_wepcat == WEP_SWORD))
 *      #define is_weptool(o)   ((o)->oclass == TOOL_CLASS && \
 *                       objects[(o)->otyp].oc_weptool)
 *      #define is_multigen(otyp) (otyp <= SHURIKEN)
 *      #define is_poisonable(otyp) (otyp <= BEC_DE_CORBIN)
 */
# define is_blade(otmp)      ((otmp->oclass == WEAPON_CLASS || \
                             otmp->oclass == TOOL_CLASS) &&\
                              objects[otmp->otyp].oc_skill >= P_DAGGER && \
                              objects[otmp->otyp].oc_skill <= P_SABER)
# define is_axe(otmp)        ((otmp->oclass == WEAPON_CLASS || \
                              otmp->oclass == TOOL_CLASS) && \
                              objects[otmp->otyp].oc_skill == P_AXE)
# define is_pick(otmp)       ((otmp->oclass == WEAPON_CLASS || \
                              otmp->oclass == TOOL_CLASS) && \
                              objects[otmp->otyp].oc_skill == P_PICK_AXE)
# define is_sword(otmp)      (otmp->oclass == WEAPON_CLASS && \
                              objects[otmp->otyp].oc_skill >= P_SHORT_SWORD && \
                              objects[otmp->otyp].oc_skill <= P_SABER)
# define is_pole(otmp)       ((otmp->oclass == WEAPON_CLASS || \
                             otmp->oclass == TOOL_CLASS) && \
                              (objects[otmp->otyp].oc_skill == P_POLEARMS || \
                              objects[otmp->otyp].oc_skill == P_LANCE))
# define is_spear(otmp)      (otmp->oclass == WEAPON_CLASS && \
                              objects[otmp->otyp].oc_skill >= P_SPEAR && \
                              objects[otmp->otyp].oc_skill <= P_JAVELIN)
# define is_launcher(otmp)   (otmp->oclass == WEAPON_CLASS && \
                              objects[otmp->otyp].oc_skill >= P_BOW && \
                              objects[otmp->otyp].oc_skill <= P_CROSSBOW)
# define is_ammo(otmp)       ((otmp->oclass == WEAPON_CLASS || \
                              otmp->oclass == GEM_CLASS) && \
                              objects[otmp->otyp].oc_skill >= -P_CROSSBOW && \
                              objects[otmp->otyp].oc_skill <= -P_BOW)
# define ammo_and_launcher(otmp,ltmp) \
                              (is_ammo(otmp) && (ltmp) && \
                              objects[(otmp)->otyp].oc_skill == \
                                -objects[(ltmp)->otyp].oc_skill)
# define is_missile(otmp)    ((otmp->oclass == WEAPON_CLASS || \
                              otmp->oclass == TOOL_CLASS) && \
                              objects[otmp->otyp].oc_skill >= -P_BOOMERANG && \
                              objects[otmp->otyp].oc_skill <= -P_DART)
# define is_weptool(o)       ((o)->oclass == TOOL_CLASS && \
                              objects[(o)->otyp].oc_skill != P_NONE)
# define bimanual(otmp)      ((otmp->oclass == WEAPON_CLASS || \
                              otmp->oclass == TOOL_CLASS) && \
                              objects[otmp->otyp].oc_bimanual)
# define is_multigen(otmp)   (otmp->oclass == WEAPON_CLASS && \
                              objects[otmp->otyp].oc_skill >= -P_SHURIKEN && \
                              objects[otmp->otyp].oc_skill <= -P_BOW)
# define is_poisonable(otmp) (otmp->oclass == WEAPON_CLASS && \
                              objects[otmp->otyp].oc_skill >= -P_SHURIKEN && \
                              objects[otmp->otyp].oc_skill <= -P_BOW)
# define uslinging()         (uwep && objects[uwep->otyp].oc_skill == P_SLING)
# define is_wep(otmp)        (((otmp)->oclass == WEAPON_CLASS) \
                                 ? !(is_launcher((otmp)) || is_ammo((otmp)) || \
                                     is_missile((otmp)) || \
                                     (is_pole((otmp)) && !u.usteed)) \
                                 : is_weptool((otmp)))

/* Armor */
# define is_shield(otmp)     (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_SHIELD)
# define is_helmet(otmp)     (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_HELM)
# define is_boots(otmp)      (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_BOOTS)
# define is_gloves(otmp)     (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_GLOVES)
# define is_cloak(otmp)      (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_CLOAK)
# define is_shirt(otmp)      (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_SHIRT)
# define is_suit(otmp)       (otmp->oclass == ARMOR_CLASS && \
                              objects[otmp->otyp].oc_armcat == ARM_SUIT)
# define is_elven_armor(otmp)    ((otmp)->otyp == ELVEN_LEATHER_HELM\
                                 || (otmp)->otyp == ELVEN_MITHRIL_COAT\
                                 || (otmp)->otyp == ELVEN_CLOAK\
                                 || (otmp)->otyp == ELVEN_SHIELD\
                                 || (otmp)->otyp == ELVEN_BOOTS)
# define is_orcish_armor(otmp)   ((otmp)->otyp == ORCISH_HELM\
                                 || (otmp)->otyp == ORCISH_CHAIN_MAIL\
                                 || (otmp)->otyp == ORCISH_RING_MAIL\
                                 || (otmp)->otyp == ORCISH_CLOAK\
                                 || (otmp)->otyp == URUK_HAI_SHIELD\
                                 || (otmp)->otyp == ORCISH_SHIELD)
# define is_dwarvish_armor(otmp) ((otmp)->otyp == DWARVISH_IRON_HELM\
                                 || (otmp)->otyp == DWARVISH_MITHRIL_COAT\
                                 || (otmp)->otyp == DWARVISH_CLOAK\
                                 || (otmp)->otyp == DWARVISH_ROUNDSHIELD)
# define is_gnomish_armor(otmp)  (FALSE)

# define helmet_name(otmp) \
    (is_metallic(otmp) && \
     (otmp)->otyp != DWARVISH_IRON_HELM ? "helmet" : "hat")
# define maybe_helmet_name(otmp) ((otmp) ? helmet_name((otmp)) : "helmet")


/* Eggs and other food */
# define MAX_EGG_HATCH_TIME 200 /* longest an egg can remain unhatched */
# define stale_egg(egg) ((moves - (egg)->age) > (2*MAX_EGG_HATCH_TIME))
# define ofood(o) ((o)->otyp == CORPSE || (o)->otyp == EGG || (o)->otyp == TIN)
# define polyfodder(obj) (ofood(obj) && \
                         pm_to_cham((obj)->corpsenm) != CHAM_ORDINARY)
# define mlevelgain(obj) (ofood(obj) && (obj)->corpsenm == PM_WRAITH)
# define mhealup(obj)   (ofood(obj) && (obj)->corpsenm == PM_NURSE)

/* Containers */
# define carried(o)     ((o)->where == OBJ_INVENT)
# define mcarried(o)    ((o)->where == OBJ_MINVENT)
# define Has_contents(o) (/* (Is_container(o) || (o)->otyp == STATUE) && */ \
                         (o)->cobj != NULL)
# define Is_container(o) ((o)->otyp >= LARGE_BOX && (o)->otyp <= BAG_OF_TRICKS)
# define Is_box(otmp)   (otmp->otyp == LARGE_BOX || otmp->otyp == CHEST)
# define Is_mbag(otmp)  (otmp->otyp == BAG_OF_HOLDING || \
                         otmp->otyp == BAG_OF_TRICKS)

/* dragon gear */
# define Is_dragon_scales(obj)  ((obj)->otyp >= GRAY_DRAGON_SCALES && \
                                 (obj)->otyp <= YELLOW_DRAGON_SCALES)
# define Is_dragon_mail(obj)    ((obj)->otyp >= GRAY_DRAGON_SCALE_MAIL && \
                                 (obj)->otyp <= YELLOW_DRAGON_SCALE_MAIL)
# define Is_dragon_armor(obj)   (Is_dragon_scales(obj) || Is_dragon_mail(obj))
# define Dragon_scales_to_pm(obj) &mons[PM_GRAY_DRAGON + (obj)->otyp \
                                       - GRAY_DRAGON_SCALES]
# define Dragon_mail_to_pm(obj) &mons[PM_GRAY_DRAGON + (obj)->otyp \
                                      - GRAY_DRAGON_SCALE_MAIL]
# define Dragon_to_scales(pm)   (GRAY_DRAGON_SCALES + (pm - mons))

/* Elven gear */
# define is_elven_weapon(otmp)  ((otmp)->otyp == ELVEN_ARROW\
                                || (otmp)->otyp == ELVEN_SPEAR\
                                || (otmp)->otyp == ELVEN_DAGGER\
                                || (otmp)->otyp == ELVEN_SHORT_SWORD\
                                || (otmp)->otyp == ELVEN_BROADSWORD\
                                || (otmp)->otyp == ELVEN_BOW)
# define is_elven_obj(otmp)     (is_elven_armor(otmp) || is_elven_weapon(otmp))

/* Orcish gear */
# define is_orcish_obj(otmp)    (is_orcish_armor(otmp)\
                                || (otmp)->otyp == ORCISH_ARROW\
                                || (otmp)->otyp == ORCISH_SPEAR\
                                || (otmp)->otyp == ORCISH_DAGGER\
                                || (otmp)->otyp == ORCISH_SHORT_SWORD\
                                || (otmp)->otyp == ORCISH_BOW)

/* Dwarvish gear */
# define is_dwarvish_obj(otmp)  (is_dwarvish_armor(otmp)\
                                || (otmp)->otyp == DWARVISH_SPEAR\
                                || (otmp)->otyp == DWARVISH_SHORT_SWORD\
                                || (otmp)->otyp == DWARVISH_MATTOCK)

/* Gnomish gear */
# define is_gnomish_obj(otmp)   (is_gnomish_armor(otmp))

/* Light sources */
# define Is_candle(otmp) (otmp->otyp == TALLOW_CANDLE || \
                         otmp->otyp == WAX_CANDLE)
# define MAX_OIL_IN_FLASK 400   /* maximum amount of oil in a potion of oil */

/* MAGIC_LAMP intentionally excluded below */
/* age field of this is relative age rather than absolute */
# define age_is_relative(otmp)  ((otmp)->otyp == BRASS_LANTERN\
                                || (otmp)->otyp == OIL_LAMP\
                                || (otmp)->otyp == CANDELABRUM_OF_INVOCATION\
                                || (otmp)->otyp == TALLOW_CANDLE\
                                || (otmp)->otyp == WAX_CANDLE\
                                || (otmp)->otyp == POT_OIL)
/* object can be ignited */
# define ignitable(otmp)        ((otmp)->otyp == BRASS_LANTERN\
                                || (otmp)->otyp == OIL_LAMP\
                                || (otmp)->otyp == CANDELABRUM_OF_INVOCATION\
                                || (otmp)->otyp == TALLOW_CANDLE\
                                || (otmp)->otyp == WAX_CANDLE\
                                || (otmp)->otyp == POT_OIL)

/* special stones */
# define is_graystone(obj)      ((obj)->otyp == LUCKSTONE || \
                                 (obj)->otyp == LOADSTONE || \
                                 (obj)->otyp == FLINT     || \
                                 (obj)->otyp == TOUCHSTONE)

/* misc */
# define is_flimsy(otmp)      (objects[(otmp)->otyp].oc_material <= LEATHER || \
                               (otmp)->otyp == RUBBER_HOSE)

/* helpers, simple enough to be macros */
# define is_plural(o)   ((o)->quan > 1 || \
                         (o)->oartifact == ART_EYES_OF_THE_OVERWORLD)

/* Flags for get_obj_location(). */
# define CONTAINED_TOO  0x1
# define BURIED_TOO     0x2


enum destroy_msg_type {
    destroy_msg_potion_cold,
    destroy_msg_potion_fire,
    destroy_msg_scroll_fire,
    destroy_msg_spellbook_fire,
    destroy_msg_wand_elec,
    num_destroy_msgs
};

struct destroy_message {
    const char *plural, *singular, *killer;
};

extern struct destroy_message destroy_messages[num_destroy_msgs];

/* used to track the thrown object to remove it from the bill if it kills a
 * shopkeeper */
extern struct obj *thrownobj;

#endif /* OBJ_H */
