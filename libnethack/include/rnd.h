/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-31 */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef RND_H
# define RND_H

#define RNG_SEEDSPACE 2508
#define RNG_SEED_SIZE_BYTES 12

#define RNG_SEED_SIZE_BASE64 (RNG_SEED_SIZE_BYTES * 4 / 3)

# include "nethack_types.h"
# include "global.h"

/* We have many RNGs. Things that couldn't reasonably be expected to line up
   between games are on rng_main (gameplay-affecting) or rng_display
   (non-gameplay-affecting); things can't be expected to line up if they depend
   strongly on player actions (e.g. turn boundary or combat), or if how much
   they help/hurt the player is heavily context-dependent and there's no control
   over the context (e.g. helplessness duration).  Bones generation is also on
   rng_main, because there's no sensible reason to keep it consistent between
   games.

   For things that could, we have specific RNGs (typically highly specific, so
   that you can't use up unwanted random numbers destined for one purpose via
   using them on something else).  These mostly correspond to ways of generating
   things (monsters, items, etc.).  These also include random durations with
   /strategic/ effects, e.g. prayer timeout.

   One special case is the level generation RNGs. There is one of these for
   each level, which handles all the aspects of the generation of that level
   that aren't affected by the character. It also handles the /species/ of
   randomly generating monsters on the level after that (but the other details
   are on the main RNG, to avoid desyncing it). As of the time of writing,
   the total number of RNGs needed for this is:

   29 (Dungeons) + 24 (Gehennom) + 9 (Mines) + 6 (Quest) + 4 (Sokoban) +
   1 (Ludios) + 3 (Vlad's) + 6 (Planes, including 1 dummy level) = 82.

   To allow for future expansion, we preserve 110 RNGs for this purpose.  (If
   a level ledger number goes above 109, we don't crash, but rather, recycle
   some of the existing RNGs. This will lead to games being played on the
   same seed no longer having identical levels, but that's forgivable.)

   Don't worry about save compatibility when adding new RNGs; nothing will break
   if they start using each other's seedspaces.  Games are only really
   comparable if played on the same version anyway.
*/
#define NUMBER_OF_LEVEL_RNGS 110
enum rng {
    rng_display = -1,    /* not saved in save file */
    rng_initialseed = 0, /* never used, serves to remember the initial seed */
    rng_main = 1,        /* general-purpose */
    rng_intervention,    /* intervene(), timeout just after intervene() */
    rng_cursed_unihorn,  /* cursed unihorn result */
    rng_eucalyptus,      /* number of uses of blessed eucalyptus */
    rng_horn_of_plenty,  /* horn of plenty results */
    rng_artifact_invoke, /* artifact invoke timeout */
    rng_strength_gain,   /* "standard" strength gain from gainstr(otmp, 0) */
    rng_charstats_role,  /* role-dependent character creation and level up */
    rng_charstats_race,  /* race-dependent character creation and level up */
    rng_survive_dbridge, /* chance of you surviving drawbridge explosion */
    rng_crystal_ball,    /* crystal ball results */
    rng_boulder_bridge,  /* boulders forming a bridge over water */
    rng_unfix_crysknife, /* crysknives reverting */
    rng_mysterious_force,/* what happens when you go upwards in Gehennom */
    rng_helm_alignment,  /* helm of opposite alignment effect on a neutral */
    rng_u_create_monster,/* "create monster" effects used by player */
    rng_m_create_monster,/* "create monster" effects used by a monster */
    rng_t_create_monster,/* "create monster" effects that create pets */
    rng_polyform_hostile,/* polyform for hostile monsters */
    rng_polyform_tame,   /* polyform for tame monsters */
    rng_figurine_effect, /* figurine tameness */
    rng_dog_untame,      /* untaming when off-level or revived: low is good */
    rng_throne_loot,     /* throne kicking effects */
    rng_sink_kick,       /* sink kicking effects */
    rng_sink_ring,       /* what ring is generated from a sink */
    rng_sink_quaff,      /* effects of quaffing a sink */
    rng_fountain_result, /* fountain result: shared by quaff and dip */
    rng_fountain_magic,  /* magic fountains have a different set of results */
    rng_mjollnir_return, /* Mjollnir's returning effect */
    rng_throne_result,   /* sitting on a throne */
    rng_dungeon_gen,     /* dungeon generation independent of the character */
    rng_intrinsic_fire,  /* chance of gaining fire resistance intrinsic */
    rng_intrinsic_sleep, /* ditto sleep resistance */
    rng_intrinsic_cold,  /* ditto cold resistance */
    rng_intrinsic_shock, /* ditto shock resistance */
    rng_intrinsic_poison,/* ditto poison resistance */
    rng_intrinsic_itis,  /* ditto teleportitis */
    rng_intrinsic_tc,    /* ditto teleport control */
    rng_newt_pw_boost,   /* Pw boost from newts */
    rng_50percent_a_int, /* 50% chance of gaining intelligence */
    rng_ddeath_d10p9,    /* d10+9 rolls for delayed instadeath timeouts */
    rng_ddeath_dconp20,  /* d(constitution)+20 rolls, likewise */
    rng_intrinsic_ring,  /* gain an intrinsic from eating a ring */
    rng_intrinsic_amulet,/* or from eating an amulet */
    rng_poly_engrave,    /* results of polymorphing an engraving */
    rng_wish_80,         /* 80% wish source, e.g. blessed magic lamp */
    rng_wish_20,         /* 20% wish source, e.g. uncursed magic lamp */
    rng_wish_15,         /* 15% wish source, e.g. fountain on dlvl 5 */
    rng_wish_10,         /* 10% wish source, e.g. fountain on dlvl 10 */
    rng_wish_5,          /*  5% wish source, e.g. cursed magic lamp */
    rng_smoky_potion,    /* smoky potion effects */
    rng_death_drop_s,    /* death drop RNG (small monsters) */
    rng_death_drop_l,    /* death drop RNG (large monsters) */
    rng_poly_obj,        /* object creatd by polymorph */
    rng_random_gem,      /* gems generated post-level-gen via rnd_class */
    rng_excalibur,       /* getting the special weapon effect of a fountain */
    rng_slow_stoning,    /* d10+d3=2 chance of being slow-stoned */
    rng_eel_drowning,    /* 10% chance of a 1-turn drown timer */
    rng_deathtouch,      /* touch of death used by Death */
    rng_foocubus_results,/* d35, d5, d25: result of foocubus sedution */
    rng_dragonscales,    /* chance of a dragon leaving scales */
    rng_permattrdmg_8,   /* Permanent attribute damage, permanent = 8 */
    rng_permattrdmg_10,  /* Permanent attribute damage, permanent = 10 */
    rng_permattrdmg_15,  /* Permanent attribute damage, permanent = 15 */
    rng_permattrdmg_30,  /* Permanent attribute damage, permanent = 30 */
    rng_artifact_wish,   /* wishing for an artifact */
    rng_wish_quantity,   /* quantity of stackable items wished for */
    rng_wish_quality,    /* spe of wished-for weapon/armor */
    rng_poly_level_adj,  /* level adjustment on polymorph */
    rng_system_shock,    /* chance of system shock death */
    rng_alchemic_blast,  /* chance of alchemy failing */
    rng_god_anger,       /* god anger result */
    rng_prayer_timeout,  /* prayer timeout */
    rng_first_protection,/* d3+1 for the first purchase of protection */
    rng_altar_convert,   /* altar conversion */
    rng_altar_gift,      /* chance and identity of an altar gift */
    rng_spellbook_gift,  /* identity of gifted spellbook */
    rng_armor_ench_4_5,  /* whether armor enchants to +4 or +5 */
    rng_id_count,        /* number of items to ID */
    rng_rndcurse,        /* rndcurse(): Magicbane absorption, count, saddle */
    rng_levport_results, /* distance to get level-teleported */
    rng_trapdoor_result, /* distance to get trapdoored */
    first_level_rng,
    last_rng = first_level_rng + NUMBER_OF_LEVEL_RNGS
};

static_assert(last_rng * RNG_SEED_SIZE_BYTES <= RNG_SEEDSPACE,
              "too many RNGs defined");

/* Not in extern.h, because we can't guarantee that that's included before this
   header is. (And in general, moving things out of extern.h is a Good Idea
   anyway.) */
extern int rn2_on_rng(int, enum rng);
extern int rnl(int);
extern int rn2_on_display_rng(int);

extern void seed_rng_from_entropy(void);
extern boolean seed_rng_from_base64(const char [static RNG_SEED_SIZE_BASE64]);
extern void get_initial_rng_seed(char [static RNG_SEED_SIZE_BASE64]);

/* 0 <= rn2(x) < x */
static inline int
rn2(int x)
{
    return rn2_on_rng(x, rng_main);
}

/* 1 <= rnd(x) <= x */
static inline int
rnd(int x)
{
    return rn2(x) + 1;
}

/* n <= d(n,x) <= (n*x) */
static inline int
dice(int n, int x)
{
    int tmp = n; /* start with number of dices, since rn2(x) is 0 to x-1 */

    while (n--)
        tmp += rn2(x);
    return tmp; /* Alea iacta est. -- J.C. */
}

static inline int
rne_on_rng(int x, enum rng rng)
{
    int tmp;

    tmp = 1;
    while (tmp < 10 && !rn2_on_rng(x, rng))
        tmp++;
    return tmp;
}

static inline int
rne(int x)
{
    return rne_on_rng(x, rng_main);
}

static inline int
rnz_on_rng(int i, enum rng rng)
{
    long x = i;
    long tmp = 1000;

    tmp += rn2_on_rng(1000, rng);
    tmp *= rne_on_rng(4, rng);
    if (rn2(2)) {
        x *= tmp;
        x /= 1000;
    } else {
        x *= 1000;
        x /= tmp;
    }
    return (int)x;
}

static inline int
rnz(int i)
{
    return rnz_on_rng(i, rng_main);
}

#endif

/*rnd.h*/

