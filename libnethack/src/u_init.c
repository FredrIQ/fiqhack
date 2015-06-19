/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include <stdint.h>

struct trobj {
    short trotyp;
    schar trspe;
    char trclass;
    unsigned trquan:6;
    unsigned trbless:2;
};

static void ini_inv(const struct trobj *, short nocreate[4], enum rng);
static void role_ini_inv(const struct trobj *, short nocreate[4]);
static void race_ini_inv(const struct trobj *, short nocreate[4]);
static void knows_object(int);
static void knows_class(char);
static boolean restricted_spell_discipline(int);

#define UNDEF_TYP       0
#define UNDEF_SPE       '\177'
#define UNDEF_BLESS     2

static int
rolern2(int x)
{
    return rn2_on_rng(x, rng_charstats_role);
}
static int
racern2(int x)
{
    return rn2_on_rng(x, rng_charstats_race);
}

/*
 * Initial inventory for the various roles.
 */

static const struct trobj Archeologist[] = {
    /* if adventure has a name...  idea from tan@uvm-gen */
    {BULLWHIP, 2, WEAPON_CLASS, 1, UNDEF_BLESS},
    {LEATHER_JACKET, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {FEDORA, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {FOOD_RATION, 0, FOOD_CLASS, 3, 0},
    {PICK_AXE, UNDEF_SPE, TOOL_CLASS, 1, UNDEF_BLESS},
    {TINNING_KIT, UNDEF_SPE, TOOL_CLASS, 1, UNDEF_BLESS},
    {TOUCHSTONE, 0, GEM_CLASS, 1, 0},
    {SACK, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Barbarian[] = {
#define B_MAJOR 0       /* two-handed sword or battle-axe */
#define B_MINOR 1       /* matched with axe or short sword */
    {TWO_HANDED_SWORD, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {AXE, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {RING_MAIL, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {FOOD_RATION, 0, FOOD_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Cave_man[] = {
#define C_AMMO  2
    {CLUB, 1, WEAPON_CLASS, 1, UNDEF_BLESS},
    {SLING, 2, WEAPON_CLASS, 1, UNDEF_BLESS},
    {FLINT, 0, GEM_CLASS, 15, UNDEF_BLESS},     /* quan is variable */
    {ROCK, 0, GEM_CLASS, 3, 0}, /* yields 18..33 */
    {LEATHER_ARMOR, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {0, 0, 0, 0, 0}
};

static const struct trobj Healer[] = {
    {SCALPEL, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {LEATHER_GLOVES, 1, ARMOR_CLASS, 1, UNDEF_BLESS},
    {STETHOSCOPE, 0, TOOL_CLASS, 1, 0},
    {POT_HEALING, 0, POTION_CLASS, 4, UNDEF_BLESS},
    {POT_EXTRA_HEALING, 0, POTION_CLASS, 4, UNDEF_BLESS},
    {WAN_SLEEP, UNDEF_SPE, WAND_CLASS, 1, UNDEF_BLESS},
    /* always blessed, so it's guaranteed readable */
    {SPE_HEALING, 0, SPBOOK_CLASS, 1, 1},
    {SPE_EXTRA_HEALING, 0, SPBOOK_CLASS, 1, 1},
    {SPE_STONE_TO_FLESH, 0, SPBOOK_CLASS, 1, 1},
    {APPLE, 0, FOOD_CLASS, 5, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Knight[] = {
    {LONG_SWORD, 1, WEAPON_CLASS, 1, UNDEF_BLESS},
    {LANCE, 1, WEAPON_CLASS, 1, UNDEF_BLESS},
    {RING_MAIL, 1, ARMOR_CLASS, 1, UNDEF_BLESS},
    {HELMET, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {SMALL_SHIELD, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {LEATHER_GLOVES, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {APPLE, 0, FOOD_CLASS, 10, 0},
    {CARROT, 0, FOOD_CLASS, 10, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Monk[] = {
#define M_BOOK          2
    {LEATHER_GLOVES, 2, ARMOR_CLASS, 1, UNDEF_BLESS},
    {ROBE, 1, ARMOR_CLASS, 1, UNDEF_BLESS},
    {UNDEF_TYP, UNDEF_SPE, SPBOOK_CLASS, 1, 1},
    {UNDEF_TYP, UNDEF_SPE, SCROLL_CLASS, 1, UNDEF_BLESS},
    {POT_HEALING, 0, POTION_CLASS, 3, UNDEF_BLESS},
    {FOOD_RATION, 0, FOOD_CLASS, 3, 0},
    {APPLE, 0, FOOD_CLASS, 5, UNDEF_BLESS},
    {ORANGE, 0, FOOD_CLASS, 5, UNDEF_BLESS},
    /* Yes, we know fortune cookies aren't really from China.  They were
       invented by George Jung in Los Angeles, California, USA in 1916. */
    {FORTUNE_COOKIE, 0, FOOD_CLASS, 3, UNDEF_BLESS},
    {0, 0, 0, 0, 0}
};

static const struct trobj Priest[] = {
    {MACE, 1, WEAPON_CLASS, 1, 1},
    {ROBE, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {SMALL_SHIELD, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {POT_WATER, 0, POTION_CLASS, 4, 1}, /* holy water */
    {CLOVE_OF_GARLIC, 0, FOOD_CLASS, 1, 0},
    {SPRIG_OF_WOLFSBANE, 0, FOOD_CLASS, 1, 0},
    {UNDEF_TYP, UNDEF_SPE, SPBOOK_CLASS, 2, UNDEF_BLESS},
    {0, 0, 0, 0, 0}
};

static const struct trobj Ranger[] = {
#define RAN_BOW         1
#define RAN_TWO_ARROWS  2
#define RAN_ZERO_ARROWS 3
    {DAGGER, 1, WEAPON_CLASS, 1, UNDEF_BLESS},
    {BOW, 1, WEAPON_CLASS, 1, UNDEF_BLESS},
    {ARROW, 2, WEAPON_CLASS, 50, UNDEF_BLESS},
    {ARROW, 0, WEAPON_CLASS, 30, UNDEF_BLESS},
    {CLOAK_OF_DISPLACEMENT, 2, ARMOR_CLASS, 1, UNDEF_BLESS},
    {CRAM_RATION, 0, FOOD_CLASS, 4, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Rogue[] = {
#define R_DAGGERS       1
    {SHORT_SWORD, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {DAGGER, 0, WEAPON_CLASS, 10, 0},   /* quan is variable */
    {LEATHER_ARMOR, 1, ARMOR_CLASS, 1, UNDEF_BLESS},
    {POT_SICKNESS, 0, POTION_CLASS, 1, 0},
    {LOCK_PICK, 9, TOOL_CLASS, 1, 0},
    {SACK, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Samurai[] = {
#define S_ARROWS        3
    {KATANA, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {SHORT_SWORD, 0, WEAPON_CLASS, 1, UNDEF_BLESS},     /* wakizashi */
    {YUMI, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {YA, 0, WEAPON_CLASS, 25, UNDEF_BLESS},     /* variable quan */
    {SPLINT_MAIL, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {0, 0, 0, 0, 0}
};

static const struct trobj Tourist[] = {
#define T_DARTS         0
    {DART, 2, WEAPON_CLASS, 25, UNDEF_BLESS},   /* quan is variable */
    {UNDEF_TYP, UNDEF_SPE, FOOD_CLASS, 10, 0},
    {POT_EXTRA_HEALING, 0, POTION_CLASS, 2, UNDEF_BLESS},
    {SCR_MAGIC_MAPPING, 0, SCROLL_CLASS, 4, UNDEF_BLESS},
    {HAWAIIAN_SHIRT, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {EXPENSIVE_CAMERA, UNDEF_SPE, TOOL_CLASS, 1, 0},
    {CREDIT_CARD, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Valkyrie[] = {
    {LONG_SWORD, 1, WEAPON_CLASS, 1, UNDEF_BLESS},
    {DAGGER, 0, WEAPON_CLASS, 1, UNDEF_BLESS},
    {SMALL_SHIELD, 3, ARMOR_CLASS, 1, UNDEF_BLESS},
    {FOOD_RATION, 0, FOOD_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Wizard[] = {
#define W_MULTSTART     2
#define W_MULTEND       6
    {QUARTERSTAFF, 1, WEAPON_CLASS, 1, 1},
    {CLOAK_OF_MAGIC_RESISTANCE, 0, ARMOR_CLASS, 1, UNDEF_BLESS},
    {UNDEF_TYP, UNDEF_SPE, WAND_CLASS, 1, UNDEF_BLESS},
    {UNDEF_TYP, UNDEF_SPE, RING_CLASS, 2, UNDEF_BLESS},
    {UNDEF_TYP, UNDEF_SPE, POTION_CLASS, 3, UNDEF_BLESS},
    {UNDEF_TYP, UNDEF_SPE, SCROLL_CLASS, 3, UNDEF_BLESS},
    {SPE_FORCE_BOLT, 0, SPBOOK_CLASS, 1, 1},
    {UNDEF_TYP, UNDEF_SPE, SPBOOK_CLASS, 1, UNDEF_BLESS},
    {0, 0, 0, 0, 0}
};

/*
 * Optional extra inventory items.
 */

static const struct trobj Tinopener[] = {
    {TIN_OPENER, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Magicmarker[] = {
    {MAGIC_MARKER, UNDEF_SPE, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Lamp[] = {
    {OIL_LAMP, 1, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Blindfold[] = {
    {BLINDFOLD, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Instrument[] = {
    {WOODEN_FLUTE, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Xtra_food[] = {
    {UNDEF_TYP, UNDEF_SPE, FOOD_CLASS, 2, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Leash[] = {
    {LEASH, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Towel[] = {
    {TOWEL, 0, TOOL_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Wishing[] = {
    {WAN_WISHING, 3, WAND_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

static const struct trobj Money[] = {
    {GOLD_PIECE, 0, COIN_CLASS, 1, 0},
    {0, 0, 0, 0, 0}
};

/* race-based substitutions for initial inventory;
   the weaker cloak for elven rangers is intentional--they shoot better */
static const struct inv_sub {
    short race_pm, item_otyp, subs_otyp;
} inv_subs[] = {
    {PM_ELF, DAGGER, ELVEN_DAGGER},
    {PM_ELF, SPEAR, ELVEN_SPEAR},
    {PM_ELF, SHORT_SWORD, ELVEN_SHORT_SWORD},
    {PM_ELF, BOW, ELVEN_BOW},
    {PM_ELF, ARROW, ELVEN_ARROW},
    {PM_ELF, HELMET, ELVEN_LEATHER_HELM},
    /* { PM_ELF, SMALL_SHIELD, ELVEN_SHIELD }, */
    {PM_ELF, CLOAK_OF_DISPLACEMENT, ELVEN_CLOAK},
    {PM_ELF, CRAM_RATION, LEMBAS_WAFER},
    {PM_ORC, DAGGER, ORCISH_DAGGER},
    {PM_ORC, SPEAR, ORCISH_SPEAR},
    {PM_ORC, SHORT_SWORD, ORCISH_SHORT_SWORD},
    {PM_ORC, BOW, ORCISH_BOW},
    {PM_ORC, ARROW, ORCISH_ARROW},
    {PM_ORC, HELMET, ORCISH_HELM},
    {PM_ORC, SMALL_SHIELD, ORCISH_SHIELD},
    {PM_ORC, RING_MAIL, ORCISH_RING_MAIL},
    {PM_ORC, CHAIN_MAIL, ORCISH_CHAIN_MAIL},
    {PM_DWARF, SPEAR, DWARVISH_SPEAR},
    {PM_DWARF, SHORT_SWORD, DWARVISH_SHORT_SWORD},
    {PM_DWARF, HELMET, DWARVISH_IRON_HELM},
    /* { PM_DWARF, SMALL_SHIELD, DWARVISH_ROUNDSHIELD }, */
    /* { PM_DWARF, PICK_AXE, DWARVISH_MATTOCK }, */
    {PM_GNOME, BOW, CROSSBOW},
    {PM_GNOME, ARROW, CROSSBOW_BOLT},
    {NON_PM, STRANGE_OBJECT, STRANGE_OBJECT}
};

static const struct def_skill Skill_A[] = {
    {P_DAGGER, P_BASIC}, {P_KNIFE, P_BASIC},
    {P_PICK_AXE, P_EXPERT}, {P_SHORT_SWORD, P_BASIC},
    {P_SCIMITAR, P_SKILLED}, {P_SABER, P_EXPERT},
    {P_CLUB, P_SKILLED}, {P_QUARTERSTAFF, P_SKILLED},
    {P_SLING, P_SKILLED}, {P_DART, P_BASIC},
    {P_BOOMERANG, P_EXPERT}, {P_WHIP, P_EXPERT},
    {P_UNICORN_HORN, P_SKILLED},
    {P_ATTACK_SPELL, P_BASIC}, {P_HEALING_SPELL, P_BASIC},
    {P_DIVINATION_SPELL, P_EXPERT}, {P_MATTER_SPELL, P_BASIC},
    {P_RIDING, P_BASIC},
    {P_TWO_WEAPON_COMBAT, P_BASIC},
    {P_BARE_HANDED_COMBAT, P_EXPERT},
    {P_NONE, 0}
};

static const struct def_skill Skill_B[] = {
    {P_DAGGER, P_BASIC}, {P_AXE, P_EXPERT},
    {P_PICK_AXE, P_SKILLED}, {P_SHORT_SWORD, P_EXPERT},
    {P_BROAD_SWORD, P_SKILLED}, {P_LONG_SWORD, P_SKILLED},
    {P_TWO_HANDED_SWORD, P_EXPERT}, {P_SCIMITAR, P_SKILLED},
    {P_SABER, P_BASIC}, {P_CLUB, P_SKILLED},
    {P_MACE, P_SKILLED}, {P_MORNING_STAR, P_SKILLED},
    {P_FLAIL, P_BASIC}, {P_HAMMER, P_EXPERT},
    {P_QUARTERSTAFF, P_BASIC}, {P_SPEAR, P_SKILLED},
    {P_TRIDENT, P_SKILLED}, {P_BOW, P_BASIC},
    {P_ATTACK_SPELL, P_SKILLED},
    {P_RIDING, P_BASIC},
    {P_TWO_WEAPON_COMBAT, P_BASIC},
    {P_BARE_HANDED_COMBAT, P_MASTER},
    {P_NONE, 0}
};

static const struct def_skill Skill_C[] = {
    {P_DAGGER, P_BASIC}, {P_KNIFE, P_SKILLED},
    {P_AXE, P_SKILLED}, {P_PICK_AXE, P_BASIC},
    {P_CLUB, P_EXPERT}, {P_MACE, P_EXPERT},
    {P_MORNING_STAR, P_BASIC}, {P_FLAIL, P_SKILLED},
    {P_HAMMER, P_SKILLED}, {P_QUARTERSTAFF, P_EXPERT},
    {P_POLEARMS, P_SKILLED}, {P_SPEAR, P_EXPERT},
    {P_JAVELIN, P_SKILLED}, {P_TRIDENT, P_SKILLED},
    {P_BOW, P_SKILLED}, {P_SLING, P_EXPERT},
    {P_ATTACK_SPELL, P_BASIC}, {P_MATTER_SPELL, P_SKILLED},
    {P_BOOMERANG, P_EXPERT}, {P_UNICORN_HORN, P_BASIC},
    {P_BARE_HANDED_COMBAT, P_MASTER},
    {P_NONE, 0}
};

static const struct def_skill Skill_H[] = {
    {P_DAGGER, P_SKILLED}, {P_KNIFE, P_EXPERT},
    {P_SHORT_SWORD, P_SKILLED}, {P_SCIMITAR, P_BASIC},
    {P_SABER, P_BASIC}, {P_CLUB, P_SKILLED},
    {P_MACE, P_BASIC}, {P_QUARTERSTAFF, P_EXPERT},
    {P_POLEARMS, P_BASIC}, {P_SPEAR, P_BASIC},
    {P_JAVELIN, P_BASIC}, {P_TRIDENT, P_BASIC},
    {P_SLING, P_SKILLED}, {P_DART, P_EXPERT},
    {P_SHURIKEN, P_SKILLED}, {P_UNICORN_HORN, P_EXPERT},
    {P_HEALING_SPELL, P_EXPERT},
    {P_BARE_HANDED_COMBAT, P_BASIC},
    {P_NONE, 0}
};

static const struct def_skill Skill_K[] = {
    {P_DAGGER, P_BASIC}, {P_KNIFE, P_BASIC},
    {P_AXE, P_SKILLED}, {P_PICK_AXE, P_BASIC},
    {P_SHORT_SWORD, P_SKILLED}, {P_BROAD_SWORD, P_SKILLED},
    {P_LONG_SWORD, P_EXPERT}, {P_TWO_HANDED_SWORD, P_SKILLED},
    {P_SCIMITAR, P_BASIC}, {P_SABER, P_SKILLED},
    {P_CLUB, P_BASIC}, {P_MACE, P_SKILLED},
    {P_MORNING_STAR, P_SKILLED}, {P_FLAIL, P_BASIC},
    {P_HAMMER, P_BASIC}, {P_POLEARMS, P_SKILLED},
    {P_SPEAR, P_SKILLED}, {P_JAVELIN, P_SKILLED},
    {P_TRIDENT, P_BASIC}, {P_LANCE, P_EXPERT},
    {P_BOW, P_BASIC}, {P_CROSSBOW, P_SKILLED},
    {P_ATTACK_SPELL, P_SKILLED}, {P_HEALING_SPELL, P_SKILLED},
    {P_CLERIC_SPELL, P_SKILLED},
    {P_RIDING, P_EXPERT},
    {P_TWO_WEAPON_COMBAT, P_SKILLED},
    {P_BARE_HANDED_COMBAT, P_EXPERT},
    {P_NONE, 0}
};

static const struct def_skill Skill_Mon[] = {
    {P_QUARTERSTAFF, P_BASIC}, {P_SPEAR, P_BASIC},
    {P_JAVELIN, P_BASIC}, {P_CROSSBOW, P_BASIC},
    {P_SHURIKEN, P_BASIC},
    {P_ATTACK_SPELL, P_BASIC}, {P_HEALING_SPELL, P_EXPERT},
    {P_DIVINATION_SPELL, P_BASIC}, {P_ENCHANTMENT_SPELL, P_BASIC},
    {P_CLERIC_SPELL, P_SKILLED}, {P_ESCAPE_SPELL, P_BASIC},
    {P_MATTER_SPELL, P_BASIC},
    {P_MARTIAL_ARTS, P_GRAND_MASTER},
    {P_NONE, 0}
};

static const struct def_skill Skill_P[] = {
    {P_CLUB, P_EXPERT}, {P_MACE, P_EXPERT},
    {P_MORNING_STAR, P_EXPERT}, {P_FLAIL, P_EXPERT},
    {P_HAMMER, P_EXPERT}, {P_QUARTERSTAFF, P_EXPERT},
    {P_POLEARMS, P_SKILLED}, {P_SPEAR, P_SKILLED},
    {P_JAVELIN, P_SKILLED}, {P_TRIDENT, P_SKILLED},
    {P_LANCE, P_BASIC}, {P_BOW, P_BASIC},
    {P_SLING, P_BASIC}, {P_CROSSBOW, P_BASIC},
    {P_DART, P_BASIC}, {P_SHURIKEN, P_BASIC},
    {P_BOOMERANG, P_BASIC}, {P_UNICORN_HORN, P_SKILLED},
    {P_HEALING_SPELL, P_EXPERT}, {P_DIVINATION_SPELL, P_EXPERT},
    {P_CLERIC_SPELL, P_EXPERT},
    {P_BARE_HANDED_COMBAT, P_BASIC},
    {P_NONE, 0}
};

static const struct def_skill Skill_R[] = {
    {P_DAGGER, P_EXPERT}, {P_KNIFE, P_EXPERT},
    {P_SHORT_SWORD, P_EXPERT}, {P_BROAD_SWORD, P_SKILLED},
    {P_LONG_SWORD, P_SKILLED}, {P_TWO_HANDED_SWORD, P_BASIC},
    {P_SCIMITAR, P_SKILLED}, {P_SABER, P_SKILLED},
    {P_CLUB, P_SKILLED}, {P_MACE, P_SKILLED},
    {P_MORNING_STAR, P_BASIC}, {P_FLAIL, P_BASIC},
    {P_HAMMER, P_BASIC}, {P_POLEARMS, P_BASIC},
    {P_SPEAR, P_BASIC}, {P_CROSSBOW, P_EXPERT},
    {P_DART, P_EXPERT}, {P_SHURIKEN, P_SKILLED},
    {P_DIVINATION_SPELL, P_SKILLED}, {P_ESCAPE_SPELL, P_SKILLED},
    {P_MATTER_SPELL, P_SKILLED},
    {P_RIDING, P_BASIC},
    {P_TWO_WEAPON_COMBAT, P_EXPERT},
    {P_BARE_HANDED_COMBAT, P_EXPERT},
    {P_NONE, 0}
};

static const struct def_skill Skill_Ran[] = {
    {P_DAGGER, P_EXPERT}, {P_KNIFE, P_SKILLED},
    {P_AXE, P_SKILLED}, {P_PICK_AXE, P_BASIC},
    {P_SHORT_SWORD, P_BASIC}, {P_MORNING_STAR, P_BASIC},
    {P_FLAIL, P_SKILLED}, {P_HAMMER, P_BASIC},
    {P_QUARTERSTAFF, P_BASIC}, {P_POLEARMS, P_SKILLED},
    {P_SPEAR, P_SKILLED}, {P_JAVELIN, P_EXPERT},
    {P_TRIDENT, P_BASIC}, {P_BOW, P_EXPERT},
    {P_SLING, P_EXPERT}, {P_CROSSBOW, P_EXPERT},
    {P_DART, P_EXPERT}, {P_SHURIKEN, P_SKILLED},
    {P_BOOMERANG, P_EXPERT}, {P_WHIP, P_BASIC},
    {P_HEALING_SPELL, P_BASIC},
    {P_DIVINATION_SPELL, P_EXPERT},
    {P_ESCAPE_SPELL, P_BASIC},
    {P_RIDING, P_BASIC},
    {P_BARE_HANDED_COMBAT, P_BASIC},
    {P_NONE, 0}
};

static const struct def_skill Skill_S[] = {
    {P_DAGGER, P_BASIC}, {P_KNIFE, P_SKILLED},
    {P_SHORT_SWORD, P_EXPERT}, {P_BROAD_SWORD, P_SKILLED},
    {P_LONG_SWORD, P_EXPERT}, {P_TWO_HANDED_SWORD, P_EXPERT},
    {P_SCIMITAR, P_BASIC}, {P_SABER, P_BASIC},
    {P_FLAIL, P_SKILLED}, {P_QUARTERSTAFF, P_BASIC},
    {P_POLEARMS, P_SKILLED}, {P_SPEAR, P_BASIC},
    {P_JAVELIN, P_BASIC}, {P_LANCE, P_SKILLED},
    {P_BOW, P_EXPERT}, {P_SHURIKEN, P_EXPERT},
    {P_ATTACK_SPELL, P_SKILLED}, {P_CLERIC_SPELL, P_SKILLED},
    {P_RIDING, P_SKILLED},
    {P_TWO_WEAPON_COMBAT, P_EXPERT},
    {P_MARTIAL_ARTS, P_MASTER},
    {P_NONE, 0}
};

static const struct def_skill Skill_T[] = {
    {P_DAGGER, P_EXPERT}, {P_KNIFE, P_SKILLED},
    {P_AXE, P_BASIC}, {P_PICK_AXE, P_BASIC},
    {P_SHORT_SWORD, P_EXPERT}, {P_BROAD_SWORD, P_BASIC},
    {P_LONG_SWORD, P_BASIC}, {P_TWO_HANDED_SWORD, P_BASIC},
    {P_SCIMITAR, P_SKILLED}, {P_SABER, P_SKILLED},
    {P_MACE, P_BASIC}, {P_MORNING_STAR, P_BASIC},
    {P_FLAIL, P_BASIC}, {P_HAMMER, P_BASIC},
    {P_QUARTERSTAFF, P_BASIC}, {P_POLEARMS, P_BASIC},
    {P_SPEAR, P_BASIC}, {P_JAVELIN, P_BASIC},
    {P_TRIDENT, P_BASIC}, {P_LANCE, P_BASIC},
    {P_BOW, P_BASIC}, {P_SLING, P_BASIC},
    {P_CROSSBOW, P_BASIC}, {P_DART, P_EXPERT},
    {P_SHURIKEN, P_BASIC}, {P_BOOMERANG, P_BASIC},
    {P_WHIP, P_BASIC}, {P_UNICORN_HORN, P_SKILLED},
    {P_DIVINATION_SPELL, P_BASIC}, {P_ENCHANTMENT_SPELL, P_BASIC},
    {P_ESCAPE_SPELL, P_SKILLED},
    {P_RIDING, P_BASIC},
    {P_TWO_WEAPON_COMBAT, P_SKILLED},
    {P_BARE_HANDED_COMBAT, P_SKILLED},
    {P_NONE, 0}
};

static const struct def_skill Skill_V[] = {
    {P_DAGGER, P_EXPERT}, {P_AXE, P_EXPERT},
    {P_PICK_AXE, P_SKILLED}, {P_SHORT_SWORD, P_SKILLED},
    {P_BROAD_SWORD, P_SKILLED}, {P_LONG_SWORD, P_EXPERT},
    {P_TWO_HANDED_SWORD, P_EXPERT}, {P_SCIMITAR, P_BASIC},
    {P_SABER, P_BASIC}, {P_HAMMER, P_EXPERT},
    {P_QUARTERSTAFF, P_BASIC}, {P_POLEARMS, P_SKILLED},
    {P_SPEAR, P_SKILLED}, {P_JAVELIN, P_BASIC},
    {P_TRIDENT, P_BASIC}, {P_LANCE, P_SKILLED},
    {P_SLING, P_BASIC},
    {P_ATTACK_SPELL, P_BASIC}, {P_ESCAPE_SPELL, P_BASIC},
    {P_RIDING, P_SKILLED},
    {P_TWO_WEAPON_COMBAT, P_SKILLED},
    {P_BARE_HANDED_COMBAT, P_EXPERT},
    {P_NONE, 0}
};

static const struct def_skill Skill_W[] = {
    {P_DAGGER, P_EXPERT}, {P_KNIFE, P_SKILLED},
    {P_AXE, P_SKILLED}, {P_SHORT_SWORD, P_BASIC},
    {P_CLUB, P_SKILLED}, {P_MACE, P_BASIC},
    {P_QUARTERSTAFF, P_EXPERT}, {P_POLEARMS, P_SKILLED},
    {P_SPEAR, P_BASIC}, {P_JAVELIN, P_BASIC},
    {P_TRIDENT, P_BASIC}, {P_SLING, P_SKILLED},
    {P_DART, P_EXPERT}, {P_SHURIKEN, P_BASIC},
    {P_ATTACK_SPELL, P_EXPERT}, {P_HEALING_SPELL, P_SKILLED},
    {P_DIVINATION_SPELL, P_EXPERT}, {P_ENCHANTMENT_SPELL, P_SKILLED},
    {P_CLERIC_SPELL, P_SKILLED}, {P_ESCAPE_SPELL, P_EXPERT},
    {P_MATTER_SPELL, P_EXPERT},
    {P_RIDING, P_BASIC},
    {P_BARE_HANDED_COMBAT, P_BASIC},
    {P_NONE, 0}
};


static struct trobj *
copy_trobj_list(const struct trobj *list)
{
    struct trobj *copy;
    int len = 0;
    
    while (list[len].trotyp || list[len].trclass)
        len++;
    len++;      /* list is terminated by an entry of zeros */
    copy = malloc(len * sizeof (struct trobj));
    memcpy(copy, list, len * sizeof (struct trobj));

    return copy;
}

static void
knows_object(int obj)
{
    discover_object(obj, TRUE, FALSE, FALSE);
    objects[obj].oc_pre_discovered = 1; /* not a "discovery" */
}

/* Know ordinary (non-magical) objects of a certain class,
 * like all gems except the loadstone and luckstone.
 */
static void
knows_class(char sym)
{
    int ct;

    for (ct = 1; ct < NUM_OBJECTS; ct++)
        if (objects[ct].oc_class == sym && !objects[ct].oc_magic)
            knows_object(ct);
}

void
u_init(microseconds birthday)
{
    int i;

    u.ufemale = u.initgend;
    flags.beginner = 1;

    u.ustuck = NULL;

    u.uz.dlevel = 1;

    u.umoved = FALSE;
    u.umortality = 0;
    u.ugrave_arise = NON_PM;

    u.umonnum = u.umonster = (u.ufemale && urole.femalenum != NON_PM) ?
        urole.femalenum : urole.malenum;

    u.lastinvnr = 51;

    set_uasmon();

    u.ulevel = 0;       /* set up some of the initial attributes */
    u.uhp = u.uhpmax = newhp();
    u.uenmax = urole.enadv.infix + urace.enadv.infix;
    if (urole.enadv.inrnd > 0)
        u.uenmax += 1 + rolern2(urole.enadv.inrnd);
    if (urace.enadv.inrnd > 0)
        u.uenmax += 1 + racern2(urace.enadv.inrnd);
    u.uen = u.uenmax;
    u.uspellprot = 0;
    adjabil(0, 1);
    u.ulevel = u.ulevelmax = 1;
    u.uac = 10;

    u.urexp = -1;       /* indicates that score is calculated not remembered */

    init_uhunger();

    for (i = 0; i <= MAXSPELL; i++)
        spl_book[i].sp_id = NO_SPELL;
    update_supernatural_abilities();
    
    u.ublesscnt = 300;  /* no prayers just yet */
    u.ualignbase[A_CURRENT] = u.ualignbase[A_ORIGINAL] = u.ualign.type =
        aligns[u.initalign].value;
    u.ulycn = NON_PM;
    
    u.ubirthday = birthday;
    
    /* For now, everyone starts out with a night vision range of 1. */
    u.nv_range = 1;
    u.next_attr_check = 600;    /* arbitrary initial setting */
    
    u.delayed_killers.genocide = u.delayed_killers.illness =
        u.delayed_killers.stoning = u.delayed_killers.sliming = NULL;
}


void
u_init_inv_skills(void)
{
    int i;
    struct trobj *trobj_list = NULL;
    short nclist[4] =
        { STRANGE_OBJECT, STRANGE_OBJECT, STRANGE_OBJECT, STRANGE_OBJECT };

        /*** Role-specific initializations ***/
    switch (Role_switch) {
    case PM_ARCHEOLOGIST:
        role_ini_inv(Archeologist, nclist);
        if (!rolern2(10))
            role_ini_inv(Tinopener, nclist);
        else if (!rolern2(4))
            role_ini_inv(Lamp, nclist);
        else if (!rolern2(10))
            role_ini_inv(Magicmarker, nclist);
        knows_object(SACK);
        knows_object(TOUCHSTONE);
        skill_init(Skill_A);
        break;
    case PM_BARBARIAN:
        trobj_list = copy_trobj_list(Barbarian);
        if (!rolern2(2)) {
            trobj_list[B_MAJOR].trotyp = BATTLE_AXE;
            trobj_list[B_MINOR].trotyp = SHORT_SWORD;
        }
        role_ini_inv(trobj_list, nclist);
        if (!rolern2(6))
            role_ini_inv(Lamp, nclist);
        knows_class(WEAPON_CLASS);
        knows_class(ARMOR_CLASS);
        skill_init(Skill_B);
        break;
    case PM_CAVEMAN:
        trobj_list = copy_trobj_list(Cave_man);
        trobj_list[C_AMMO].trquan = 10 + rolern2(11);
        role_ini_inv(trobj_list, nclist);
        skill_init(Skill_C);
        break;
    case PM_HEALER:
        u.umoney0 = 1001 + rolern2(1000);
        role_ini_inv(Healer, nclist);
        if (!rolern2(25))
            role_ini_inv(Lamp, nclist);
        knows_object(POT_FULL_HEALING);
        skill_init(Skill_H);
        break;
    case PM_KNIGHT:
        role_ini_inv(Knight, nclist);
        knows_class(WEAPON_CLASS);
        knows_class(ARMOR_CLASS);
        /* give knights chess-like mobility -- idea from
           wooledge@skybridge.scl.cwru.edu */
        HJumping |= FROMOUTSIDE;
        skill_init(Skill_K);
        break;
    case PM_MONK:
        trobj_list = copy_trobj_list(Monk);
        switch (rolern2(3)) {
        case 0:
            trobj_list[M_BOOK].trotyp = SPE_HEALING;
            break;
        case 1:
            trobj_list[M_BOOK].trotyp = SPE_PROTECTION;
            break;
        case 2:
            trobj_list[M_BOOK].trotyp = SPE_SLEEP;
            break;
        }
        role_ini_inv(trobj_list, nclist);
        if (!rolern2(5))
            role_ini_inv(Magicmarker, nclist);
        else if (!rolern2(10))
            role_ini_inv(Lamp, nclist);
        knows_class(ARMOR_CLASS);
        skill_init(Skill_Mon);
        break;
    case PM_PRIEST:
        role_ini_inv(Priest, nclist);
        if (!rolern2(10))
            role_ini_inv(Magicmarker, nclist);
        else if (!rolern2(10))
            role_ini_inv(Lamp, nclist);
        knows_object(POT_WATER);
        skill_init(Skill_P);
        /* KMH, conduct -- Some may claim that this isn't agnostic, since they
           are literally "priests" and they have holy water. But we don't count 
           it as such.  Purists can always avoid playing priests and/or confirm 
           another player's role in their YAAP. */
        break;
    case PM_RANGER:
        trobj_list = copy_trobj_list(Ranger);
        trobj_list[RAN_TWO_ARROWS].trquan = 50 + rolern2(10);
        trobj_list[RAN_ZERO_ARROWS].trquan = 30 + rolern2(10);
        role_ini_inv(trobj_list, nclist);
        skill_init(Skill_Ran);
        break;
    case PM_ROGUE:
        trobj_list = copy_trobj_list(Rogue);
        trobj_list[R_DAGGERS].trquan = 6 + rolern2(10);
        u.umoney0 = 0;
        role_ini_inv(trobj_list, nclist);
        if (!rolern2(5))
            role_ini_inv(Blindfold, nclist);
        knows_object(SACK);
        skill_init(Skill_R);
        break;
    case PM_SAMURAI:
        trobj_list = copy_trobj_list(Samurai);
        trobj_list[S_ARROWS].trquan = 26 + rolern2(20);
        role_ini_inv(trobj_list, nclist);
        if (!rolern2(5))
            role_ini_inv(Blindfold, nclist);
        knows_class(WEAPON_CLASS);
        knows_class(ARMOR_CLASS);
        skill_init(Skill_S);
        break;
    case PM_TOURIST:
        trobj_list = copy_trobj_list(Tourist);
        trobj_list[T_DARTS].trquan = 21 + rolern2(20);
        u.umoney0 = 1 + rolern2(1000);
        role_ini_inv(trobj_list, nclist);
        if (!rolern2(25))
            role_ini_inv(Tinopener, nclist);
        else if (!rolern2(25))
            role_ini_inv(Leash, nclist);
        else if (!rolern2(25))
            role_ini_inv(Towel, nclist);
        else if (!rolern2(25))
            role_ini_inv(Magicmarker, nclist);
        skill_init(Skill_T);
        break;
    case PM_VALKYRIE:
        role_ini_inv(Valkyrie, nclist);
        if (!rolern2(6))
            role_ini_inv(Lamp, nclist);
        knows_class(WEAPON_CLASS);
        knows_class(ARMOR_CLASS);
        skill_init(Skill_V);
        break;
    case PM_WIZARD:
        role_ini_inv(Wizard, nclist);
        if (!rolern2(5))
            role_ini_inv(Magicmarker, nclist);
        if (!rolern2(5))
            role_ini_inv(Blindfold, nclist);
        skill_init(Skill_W);
        break;

    default:   /* impossible */
        break;
    }

    if (trobj_list)
        free(trobj_list);
    trobj_list = NULL;

        /*** Race-specific initializations ***/
    switch (Race_switch) {
    case PM_HUMAN:
        /* Nothing special */
        break;

    case PM_ELF:
    {
        /* Elves are people of music and song, or they are warriors.
           Non-warriors get an instrument.  We use a kludge to get only
           non-magic instruments. */
        static const int trotyp[] = {
            WOODEN_FLUTE, TOOLED_HORN, WOODEN_HARP, BELL, BUGLE, LEATHER_DRUM
        };
        if (Role_if(PM_PRIEST) || Role_if(PM_WIZARD)) {
            trobj_list = copy_trobj_list(Instrument);
            trobj_list[0].trotyp = trotyp[rn2(SIZE(trotyp))];
            ini_inv(trobj_list, nclist, rng_main);
        }

        /* Elves can recognize all elvish objects */
        knows_object(ELVEN_SHORT_SWORD);
        knows_object(ELVEN_ARROW);
        knows_object(ELVEN_BOW);
        knows_object(ELVEN_SPEAR);
        knows_object(ELVEN_DAGGER);
        knows_object(ELVEN_BROADSWORD);
        knows_object(ELVEN_MITHRIL_COAT);
        knows_object(ELVEN_LEATHER_HELM);
        knows_object(ELVEN_SHIELD);
        knows_object(ELVEN_BOOTS);
        knows_object(ELVEN_CLOAK);
        break;
    }
    case PM_DWARF:
        /* Dwarves can recognize all dwarvish objects */
        knows_object(DWARVISH_SPEAR);
        knows_object(DWARVISH_SHORT_SWORD);
        knows_object(DWARVISH_MATTOCK);
        knows_object(DWARVISH_IRON_HELM);
        knows_object(DWARVISH_MITHRIL_COAT);
        knows_object(DWARVISH_CLOAK);
        knows_object(DWARVISH_ROUNDSHIELD);
        break;

    case PM_GNOME:
        break;

    case PM_ORC:
        /* compensate for generally inferior equipment */
        if (!Role_if(PM_WIZARD))
            ini_inv(Xtra_food, nclist, rng_main);
        /* Orcs can recognize all orcish objects */
        knows_object(ORCISH_SHORT_SWORD);
        knows_object(ORCISH_ARROW);
        knows_object(ORCISH_BOW);
        knows_object(ORCISH_SPEAR);
        knows_object(ORCISH_DAGGER);
        knows_object(ORCISH_CHAIN_MAIL);
        knows_object(ORCISH_RING_MAIL);
        knows_object(ORCISH_HELM);
        knows_object(ORCISH_SHIELD);
        knows_object(URUK_HAI_SHIELD);
        knows_object(ORCISH_CLOAK);
        break;

    default:   /* impossible */
        break;
    }

    if (trobj_list)
        free(trobj_list);

    if (discover)
        race_ini_inv(Wishing, nclist);

    if (u.umoney0)
        race_ini_inv(Money, nclist);
    u.umoney0 += hidden_gold(); /* in case sack has gold in it */

    init_attr(75);      /* init attribute values */
    max_rank_sz();      /* set max str size for class ranks */

    /* Do we really need this? */
    for (i = 0; i < A_MAX; i++)
        if (!racern2(20)) {
            int xd = racern2(7) - 2;        /* biased variation */

            adjattrib(i, xd, TRUE);
            if (ABASE(i) < AMAX(i))
                AMAX(i) = ABASE(i);
        }

    /* make sure you can carry all you have - especially for Tourists */
    while (inv_weight() > 0) {
        if (adjattrib(A_STR, 1, TRUE))
            continue;
        if (adjattrib(A_CON, 1, TRUE))
            continue;
        /* only get here when didn't boost strength or constitution */
        break;
    }

    return;
}

/* skills aren't initialized, so we use the role-specific skill lists */
static boolean
restricted_spell_discipline(int otyp)
{
    const struct def_skill *skills;
    int this_skill = spell_skilltype(otyp);

    switch (Role_switch) {
    case PM_ARCHEOLOGIST:
        skills = Skill_A;
        break;
    case PM_BARBARIAN:
        skills = Skill_B;
        break;
    case PM_CAVEMAN:
        skills = Skill_C;
        break;
    case PM_HEALER:
        skills = Skill_H;
        break;
    case PM_KNIGHT:
        skills = Skill_K;
        break;
    case PM_MONK:
        skills = Skill_Mon;
        break;
    case PM_PRIEST:
        skills = Skill_P;
        break;
    case PM_RANGER:
        skills = Skill_Ran;
        break;
    case PM_ROGUE:
        skills = Skill_R;
        break;
    case PM_SAMURAI:
        skills = Skill_S;
        break;
    case PM_TOURIST:
        skills = Skill_T;
        break;
    case PM_VALKYRIE:
        skills = Skill_V;
        break;
    case PM_WIZARD:
        skills = Skill_W;
        break;
    default:
        impossible("checking spells for nonexistent role");
        return FALSE;
    }

    while (skills->skill != P_NONE) {
        if (skills->skill == this_skill)
            return FALSE;
        ++skills;
    }
    return TRUE;
}

static void
role_ini_inv(const struct trobj *trop, short nocreate[4])
{
    return ini_inv(trop, nocreate, rng_charstats_role);
}
static void
race_ini_inv(const struct trobj *trop, short nocreate[4])
{
    return ini_inv(trop, nocreate, rng_charstats_race);
}
static void
ini_inv(const struct trobj *trop, short nocreate[4], enum rng rng)
{
    struct obj *obj;
    int otyp, i;
    long trquan = trop->trquan;

    while (trop->trclass) {
        if (trop->trotyp != UNDEF_TYP) {
            otyp = (int)trop->trotyp;
            if (urace.malenum != PM_HUMAN) {
                /* substitute specific items for generic ones */
                for (i = 0; inv_subs[i].race_pm != NON_PM; ++i)
                    if (inv_subs[i].race_pm == urace.malenum &&
                        otyp == inv_subs[i].item_otyp) {
                        otyp = inv_subs[i].subs_otyp;
                        break;
                    }
            }
            obj = mksobj(level, otyp, TRUE, FALSE, rng);

            /* lacquered armour */
            if (obj->otyp == SPLINT_MAIL && Role_if(PM_SAMURAI))
                obj->oerodeproof = obj->rknown = 1;
        } else {        /* UNDEF_TYP */
            /* 
             * For random objects, do not create certain overly powerful
             * items: wand of wishing, ring of levitation, or the
             * polymorph/polymorph control combination.  Specific objects,
             * i.e. the discovery wishing, are still OK.
             * Also, don't get a couple of really useless items.  (Note:
             * punishment isn't "useless".  Some players who start out with
             * one will immediately read it and use the iron ball as a
             * weapon.)
             */
            obj = mkobj(level, trop->trclass, FALSE, rng);
            otyp = obj->otyp;
            while (otyp == WAN_WISHING || otyp == nocreate[0]
                   || otyp == nocreate[1]
                   || otyp == nocreate[2]
                   || otyp == nocreate[3]
                   || (otyp == RIN_LEVITATION && flags.elbereth_enabled)
                   /* 'useless' items */
                   || otyp == POT_HALLUCINATION || otyp == POT_ACID ||
                   otyp == SCR_AMNESIA || otyp == SCR_FIRE ||
                   otyp == SCR_BLANK_PAPER || otyp == SPE_BLANK_PAPER ||
                   otyp == RIN_AGGRAVATE_MONSTER || otyp == RIN_HUNGER ||
                   otyp == WAN_NOTHING
                   /* Monks don't use weapons */
                   || (otyp == SCR_ENCHANT_WEAPON && Role_if(PM_MONK))
                   /* wizard patch -- they already have one */
                   || (otyp == SPE_FORCE_BOLT && Role_if(PM_WIZARD))
                   /* powerful spells are either useless to low level players
                      or unbalancing; also spells in restricted skill
                      categories */
                   || (obj->oclass == SPBOOK_CLASS &&
                       (objects[otyp].oc_level > 3 ||
                        restricted_spell_discipline(otyp)))
                ) {
                dealloc_obj(obj);
                obj = mkobj(level, trop->trclass, FALSE, rng);
                otyp = obj->otyp;
            }

            /* Don't start with +0 or negative rings */
            if (objects[otyp].oc_charged && obj->spe <= 0)
                obj->spe = rne_on_rng(3, rng);

            /* Heavily relies on the fact that 1) we create wands before rings, 
               2) that we create rings before spellbooks, and that 3) not more
               than 1 object of a particular symbol is to be prohibited.  (For
               more objects, we need more nocreate variables...) */
            switch (otyp) {
            case WAN_POLYMORPH:
            case RIN_POLYMORPH:
            case POT_POLYMORPH:
                nocreate[0] = RIN_POLYMORPH_CONTROL;
                break;
            case RIN_POLYMORPH_CONTROL:
                nocreate[0] = RIN_POLYMORPH;
                nocreate[1] = SPE_POLYMORPH;
                nocreate[2] = POT_POLYMORPH;
            }
            /* Don't have 2 of the same ring or spellbook */
            if (obj->oclass == RING_CLASS || obj->oclass == SPBOOK_CLASS)
                nocreate[3] = otyp;
        }

        if (trop->trclass == COIN_CLASS) {
            /* no "blessed" or "identified" money */
            obj->quan = u.umoney0;
        } else {
            obj->dknown = obj->bknown = obj->rknown = 1;
            if (objects[otyp].oc_uses_known)
                obj->known = 1;
            obj->cursed = 0;
            if (obj->opoisoned && u.ualign.type != A_CHAOTIC)
                obj->opoisoned = 0;
            if (obj->oclass == WEAPON_CLASS || obj->oclass == TOOL_CLASS) {
                obj->quan = trquan;
                trquan = 1;
            } else if (obj->oclass == GEM_CLASS && is_graystone(obj) &&
                       obj->otyp != FLINT) {
                obj->quan = 1L;
            }
            if (trop->trspe != UNDEF_SPE)
                obj->spe = trop->trspe;
            if (trop->trbless != UNDEF_BLESS)
                obj->blessed = trop->trbless;
        }
        /* defined after setting otyp+quan + blessedness */
        obj->owt = weight(obj);
        obj = addinv(obj);

        /* Make the type known if necessary */
        if (OBJ_DESCR(objects[otyp]) && obj->known)
            knows_object(otyp);

        /* pre-ID oil as it's easy to check anyway */
        knows_object(POT_OIL);

        if (obj->oclass == ARMOR_CLASS) {
            long mask;
            if (is_shield(obj) && uswapwep)
                setuswapwep(NULL);

            /* The TRUE for cblock allows armor to be equipped out of order.
               Just in case we generate it like that. This relies on the fact
               that we don't give the player cursed items in starting
               inventory.
            
               TODO: Does this provide numerical extrinsics, like brilliance?
               The situation nonetheless probably can't currently come up. */
            if (canwearobj(obj, &mask, FALSE, TRUE, TRUE) && mask & W_ARMOR)
                setworn(obj, mask);
        }

        if (obj->oclass == WEAPON_CLASS || is_weptool(obj) || otyp == TIN_OPENER
            || otyp == FLINT || otyp == ROCK) {
            if (is_ammo(obj) || is_missile(obj) ||
                (obj->oclass == WEAPON_CLASS && obj->quan > 1)) {
                if (!uquiver)
                    setuqwep(obj);
            } else if (!uwep)
                setuwep(obj);
            else if (!uswapwep)
                setuswapwep(obj);
        }
        if (obj->oclass == SPBOOK_CLASS && obj->otyp != SPE_BLANK_PAPER)
            initialspell(obj);

        if (--trquan)
            continue;   /* make a similar object */
        trop++;
        trquan = trop->trquan;
    }
}



/*u_init.c*/

