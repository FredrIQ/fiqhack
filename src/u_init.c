/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"

struct trobj {
	short trotyp;
	schar trspe;
	char trclass;
	unsigned trquan:6;
	unsigned trbless:2;
};

static void ini_inv(const struct trobj *, short nocreate[4]);
static void knows_object(int);
static void knows_class(char);
static boolean restricted_spell_discipline(int);

#define UNDEF_TYP	0
#define UNDEF_SPE	'\177'
#define UNDEF_BLESS	2

/*
 *	Initial inventory for the various roles.
 */

static const struct trobj Archeologist[] = {
	/* if adventure has a name...  idea from tan@uvm-gen */
	{ BULLWHIP, 2, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ LEATHER_JACKET, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ FEDORA, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ FOOD_RATION, 0, FOOD_CLASS, 3, 0 },
	{ PICK_AXE, UNDEF_SPE, TOOL_CLASS, 1, UNDEF_BLESS },
	{ TINNING_KIT, UNDEF_SPE, TOOL_CLASS, 1, UNDEF_BLESS },
	{ TOUCHSTONE, 0, GEM_CLASS, 1, 0 },
	{ SACK, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Barbarian[] = {
#define B_MAJOR	0	/* two-handed sword or battle-axe  */
#define B_MINOR	1	/* matched with axe or short sword */
	{ TWO_HANDED_SWORD, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ AXE, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ RING_MAIL, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ FOOD_RATION, 0, FOOD_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Cave_man[] = {
#define C_AMMO	2
	{ CLUB, 1, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ SLING, 2, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ FLINT, 0, GEM_CLASS, 15, UNDEF_BLESS },	/* quan is variable */
	{ ROCK, 0, GEM_CLASS, 3, 0 },			/* yields 18..33 */
	{ LEATHER_ARMOR, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Healer[] = {
	{ SCALPEL, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ LEATHER_GLOVES, 1, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ STETHOSCOPE, 0, TOOL_CLASS, 1, 0 },
	{ POT_HEALING, 0, POTION_CLASS, 4, UNDEF_BLESS },
	{ POT_EXTRA_HEALING, 0, POTION_CLASS, 4, UNDEF_BLESS },
	{ WAN_SLEEP, UNDEF_SPE, WAND_CLASS, 1, UNDEF_BLESS },
	/* always blessed, so it's guaranteed readable */
	{ SPE_HEALING, 0, SPBOOK_CLASS, 1, 1 },
	{ SPE_EXTRA_HEALING, 0, SPBOOK_CLASS, 1, 1 },
	{ SPE_STONE_TO_FLESH, 0, SPBOOK_CLASS, 1, 1 },
	{ APPLE, 0, FOOD_CLASS, 5, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Knight[] = {
	{ LONG_SWORD, 1, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ LANCE, 1, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ RING_MAIL, 1, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ HELMET, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ SMALL_SHIELD, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ LEATHER_GLOVES, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ APPLE, 0, FOOD_CLASS, 10, 0 },
	{ CARROT, 0, FOOD_CLASS, 10, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Monk[] = {
#define M_BOOK		2
	{ LEATHER_GLOVES, 2, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ ROBE, 1, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ UNDEF_TYP, UNDEF_SPE, SPBOOK_CLASS, 1, 1 },
	{ UNDEF_TYP, UNDEF_SPE, SCROLL_CLASS, 1, UNDEF_BLESS },
	{ POT_HEALING, 0, POTION_CLASS, 3, UNDEF_BLESS },
	{ FOOD_RATION, 0, FOOD_CLASS, 3, 0 },
	{ APPLE, 0, FOOD_CLASS, 5, UNDEF_BLESS },
	{ ORANGE, 0, FOOD_CLASS, 5, UNDEF_BLESS },
	/* Yes, we know fortune cookies aren't really from China.  They were
	 * invented by George Jung in Los Angeles, California, USA in 1916.
	 */
	{ FORTUNE_COOKIE, 0, FOOD_CLASS, 3, UNDEF_BLESS },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Priest[] = {
	{ MACE, 1, WEAPON_CLASS, 1, 1 },
	{ ROBE, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ SMALL_SHIELD, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ POT_WATER, 0, POTION_CLASS, 4, 1 },	/* holy water */
	{ CLOVE_OF_GARLIC, 0, FOOD_CLASS, 1, 0 },
	{ SPRIG_OF_WOLFSBANE, 0, FOOD_CLASS, 1, 0 },
	{ UNDEF_TYP, UNDEF_SPE, SPBOOK_CLASS, 2, UNDEF_BLESS },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Ranger[] = {
#define RAN_BOW			1
#define RAN_TWO_ARROWS	2
#define RAN_ZERO_ARROWS	3
	{ DAGGER, 1, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ BOW, 1, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ ARROW, 2, WEAPON_CLASS, 50, UNDEF_BLESS },
	{ ARROW, 0, WEAPON_CLASS, 30, UNDEF_BLESS },
	{ CLOAK_OF_DISPLACEMENT, 2, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ CRAM_RATION, 0, FOOD_CLASS, 4, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Rogue[] = {
#define R_DAGGERS	1
	{ SHORT_SWORD, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ DAGGER, 0, WEAPON_CLASS, 10, 0 },	/* quan is variable */
	{ LEATHER_ARMOR, 1, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ POT_SICKNESS, 0, POTION_CLASS, 1, 0 },
	{ LOCK_PICK, 9, TOOL_CLASS, 1, 0 },
	{ SACK, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Samurai[] = {
#define S_ARROWS	3
	{ KATANA, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ SHORT_SWORD, 0, WEAPON_CLASS, 1, UNDEF_BLESS }, /* wakizashi */
	{ YUMI, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ YA, 0, WEAPON_CLASS, 25, UNDEF_BLESS }, /* variable quan */
	{ SPLINT_MAIL, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Tourist[] = {
#define T_DARTS		0
	{ DART, 2, WEAPON_CLASS, 25, UNDEF_BLESS },	/* quan is variable */
	{ UNDEF_TYP, UNDEF_SPE, FOOD_CLASS, 10, 0 },
	{ POT_EXTRA_HEALING, 0, POTION_CLASS, 2, UNDEF_BLESS },
	{ SCR_MAGIC_MAPPING, 0, SCROLL_CLASS, 4, UNDEF_BLESS },
	{ HAWAIIAN_SHIRT, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ EXPENSIVE_CAMERA, UNDEF_SPE, TOOL_CLASS, 1, 0 },
	{ CREDIT_CARD, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Valkyrie[] = {
	{ LONG_SWORD, 1, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ DAGGER, 0, WEAPON_CLASS, 1, UNDEF_BLESS },
	{ SMALL_SHIELD, 3, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ FOOD_RATION, 0, FOOD_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Wizard[] = {
#define W_MULTSTART	2
#define W_MULTEND	6
	{ QUARTERSTAFF, 1, WEAPON_CLASS, 1, 1 },
	{ CLOAK_OF_MAGIC_RESISTANCE, 0, ARMOR_CLASS, 1, UNDEF_BLESS },
	{ UNDEF_TYP, UNDEF_SPE, WAND_CLASS, 1, UNDEF_BLESS },
	{ UNDEF_TYP, UNDEF_SPE, RING_CLASS, 2, UNDEF_BLESS },
	{ UNDEF_TYP, UNDEF_SPE, POTION_CLASS, 3, UNDEF_BLESS },
	{ UNDEF_TYP, UNDEF_SPE, SCROLL_CLASS, 3, UNDEF_BLESS },
	{ SPE_FORCE_BOLT, 0, SPBOOK_CLASS, 1, 1 },
	{ UNDEF_TYP, UNDEF_SPE, SPBOOK_CLASS, 1, UNDEF_BLESS },
	{ 0, 0, 0, 0, 0 }
};

/*
 *	Optional extra inventory items.
 */

static const struct trobj Tinopener[] = {
	{ TIN_OPENER, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Magicmarker[] = {
	{ MAGIC_MARKER, UNDEF_SPE, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Lamp[] = {
	{ OIL_LAMP, 1, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Blindfold[] = {
	{ BLINDFOLD, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Instrument[] = {
	{ WOODEN_FLUTE, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Xtra_food[] = {
	{ UNDEF_TYP, UNDEF_SPE, FOOD_CLASS, 2, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Leash[] = {
	{ LEASH, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Towel[] = {
	{ TOWEL, 0, TOOL_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Wishing[] = {
	{ WAN_WISHING, 3, WAND_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};
static const struct trobj Money[] = {
	{ GOLD_PIECE, 0 , COIN_CLASS, 1, 0 },
	{ 0, 0, 0, 0, 0 }
};

/* race-based substitutions for initial inventory;
   the weaker cloak for elven rangers is intentional--they shoot better */
static const struct inv_sub { short race_pm, item_otyp, subs_otyp; } inv_subs[] = {
    { PM_ELF,	DAGGER,			ELVEN_DAGGER	      },
    { PM_ELF,	SPEAR,			ELVEN_SPEAR	      },
    { PM_ELF,	SHORT_SWORD,		ELVEN_SHORT_SWORD     },
    { PM_ELF,	BOW,			ELVEN_BOW	      },
    { PM_ELF,	ARROW,			ELVEN_ARROW	      },
    { PM_ELF,	HELMET,			ELVEN_LEATHER_HELM    },
 /* { PM_ELF,	SMALL_SHIELD,		ELVEN_SHIELD	      }, */
    { PM_ELF,	CLOAK_OF_DISPLACEMENT,	ELVEN_CLOAK	      },
    { PM_ELF,	CRAM_RATION,		LEMBAS_WAFER	      },
    { PM_ORC,	DAGGER,			ORCISH_DAGGER	      },
    { PM_ORC,	SPEAR,			ORCISH_SPEAR	      },
    { PM_ORC,	SHORT_SWORD,		ORCISH_SHORT_SWORD    },
    { PM_ORC,	BOW,			ORCISH_BOW	      },
    { PM_ORC,	ARROW,			ORCISH_ARROW	      },
    { PM_ORC,	HELMET,			ORCISH_HELM	      },
    { PM_ORC,	SMALL_SHIELD,		ORCISH_SHIELD	      },
    { PM_ORC,	RING_MAIL,		ORCISH_RING_MAIL      },
    { PM_ORC,	CHAIN_MAIL,		ORCISH_CHAIN_MAIL     },
    { PM_DWARF, SPEAR,			DWARVISH_SPEAR	      },
    { PM_DWARF, SHORT_SWORD,		DWARVISH_SHORT_SWORD  },
    { PM_DWARF, HELMET,			DWARVISH_IRON_HELM    },
 /* { PM_DWARF, SMALL_SHIELD,		DWARVISH_ROUNDSHIELD  }, */
 /* { PM_DWARF, PICK_AXE,		DWARVISH_MATTOCK      }, */
    { PM_GNOME, BOW,			CROSSBOW	      },
    { PM_GNOME, ARROW,			CROSSBOW_BOLT	      },
    { NON_PM,	STRANGE_OBJECT,		STRANGE_OBJECT	      }
};

static const struct def_skill Skill_A[] = {
    { P_DAGGER, P_BASIC },		{ P_KNIFE,  P_BASIC },
    { P_PICK_AXE, P_EXPERT },		{ P_SHORT_SWORD, P_BASIC },
    { P_SCIMITAR, P_SKILLED },		{ P_SABER, P_EXPERT },
    { P_CLUB, P_SKILLED },		{ P_QUARTERSTAFF, P_SKILLED },
    { P_SLING, P_SKILLED },		{ P_DART, P_BASIC },
    { P_BOOMERANG, P_EXPERT },		{ P_WHIP, P_EXPERT },
    { P_UNICORN_HORN, P_SKILLED },
    { P_ATTACK_SPELL, P_BASIC },	{ P_HEALING_SPELL, P_BASIC },
    { P_DIVINATION_SPELL, P_EXPERT},	{ P_MATTER_SPELL, P_BASIC},
    { P_RIDING, P_BASIC },
    { P_TWO_WEAPON_COMBAT, P_BASIC },
    { P_BARE_HANDED_COMBAT, P_EXPERT },
    { P_NONE, 0 }
};

static const struct def_skill Skill_B[] = {
    { P_DAGGER, P_BASIC },		{ P_AXE, P_EXPERT },
    { P_PICK_AXE, P_SKILLED },	{ P_SHORT_SWORD, P_EXPERT },
    { P_BROAD_SWORD, P_SKILLED },	{ P_LONG_SWORD, P_SKILLED },
    { P_TWO_HANDED_SWORD, P_EXPERT },	{ P_SCIMITAR, P_SKILLED },
    { P_SABER, P_BASIC },		{ P_CLUB, P_SKILLED },
    { P_MACE, P_SKILLED },		{ P_MORNING_STAR, P_SKILLED },
    { P_FLAIL, P_BASIC },		{ P_HAMMER, P_EXPERT },
    { P_QUARTERSTAFF, P_BASIC },	{ P_SPEAR, P_SKILLED },
    { P_TRIDENT, P_SKILLED },		{ P_BOW, P_BASIC },
    { P_ATTACK_SPELL, P_SKILLED },
    { P_RIDING, P_BASIC },
    { P_TWO_WEAPON_COMBAT, P_BASIC },
    { P_BARE_HANDED_COMBAT, P_MASTER },
    { P_NONE, 0 }
};

static const struct def_skill Skill_C[] = {
    { P_DAGGER, P_BASIC },		{ P_KNIFE,  P_SKILLED },
    { P_AXE, P_SKILLED },		{ P_PICK_AXE, P_BASIC },
    { P_CLUB, P_EXPERT },		{ P_MACE, P_EXPERT },
    { P_MORNING_STAR, P_BASIC },	{ P_FLAIL, P_SKILLED },
    { P_HAMMER, P_SKILLED },		{ P_QUARTERSTAFF, P_EXPERT },
    { P_POLEARMS, P_SKILLED },		{ P_SPEAR, P_EXPERT },
    { P_JAVELIN, P_SKILLED },		{ P_TRIDENT, P_SKILLED },
    { P_BOW, P_SKILLED },		{ P_SLING, P_EXPERT },
    { P_ATTACK_SPELL, P_BASIC },	{ P_MATTER_SPELL, P_SKILLED },
    { P_BOOMERANG, P_EXPERT },		{ P_UNICORN_HORN, P_BASIC },
    { P_BARE_HANDED_COMBAT, P_MASTER },
    { P_NONE, 0 }
};

static const struct def_skill Skill_H[] = {
    { P_DAGGER, P_SKILLED },		{ P_KNIFE, P_EXPERT },
    { P_SHORT_SWORD, P_SKILLED },	{ P_SCIMITAR, P_BASIC },
    { P_SABER, P_BASIC },		{ P_CLUB, P_SKILLED },
    { P_MACE, P_BASIC },		{ P_QUARTERSTAFF, P_EXPERT },
    { P_POLEARMS, P_BASIC },		{ P_SPEAR, P_BASIC },
    { P_JAVELIN, P_BASIC },		{ P_TRIDENT, P_BASIC },
    { P_SLING, P_SKILLED },		{ P_DART, P_EXPERT },
    { P_SHURIKEN, P_SKILLED },		{ P_UNICORN_HORN, P_EXPERT },
    { P_HEALING_SPELL, P_EXPERT },
    { P_BARE_HANDED_COMBAT, P_BASIC },
    { P_NONE, 0 }
};

static const struct def_skill Skill_K[] = {
    { P_DAGGER, P_BASIC },		{ P_KNIFE, P_BASIC },
    { P_AXE, P_SKILLED },		{ P_PICK_AXE, P_BASIC },
    { P_SHORT_SWORD, P_SKILLED },	{ P_BROAD_SWORD, P_SKILLED },
    { P_LONG_SWORD, P_EXPERT },	{ P_TWO_HANDED_SWORD, P_SKILLED },
    { P_SCIMITAR, P_BASIC },		{ P_SABER, P_SKILLED },
    { P_CLUB, P_BASIC },		{ P_MACE, P_SKILLED },
    { P_MORNING_STAR, P_SKILLED },	{ P_FLAIL, P_BASIC },
    { P_HAMMER, P_BASIC },		{ P_POLEARMS, P_SKILLED },
    { P_SPEAR, P_SKILLED },		{ P_JAVELIN, P_SKILLED },
    { P_TRIDENT, P_BASIC },		{ P_LANCE, P_EXPERT },
    { P_BOW, P_BASIC },			{ P_CROSSBOW, P_SKILLED },
    { P_ATTACK_SPELL, P_SKILLED },	{ P_HEALING_SPELL, P_SKILLED },
    { P_CLERIC_SPELL, P_SKILLED },
    { P_RIDING, P_EXPERT },
    { P_TWO_WEAPON_COMBAT, P_SKILLED },
    { P_BARE_HANDED_COMBAT, P_EXPERT },
    { P_NONE, 0 }
};

static const struct def_skill Skill_Mon[] = {
    { P_QUARTERSTAFF, P_BASIC },    { P_SPEAR, P_BASIC },
    { P_JAVELIN, P_BASIC },		    { P_CROSSBOW, P_BASIC },
    { P_SHURIKEN, P_BASIC },
    { P_ATTACK_SPELL, P_BASIC },    { P_HEALING_SPELL, P_EXPERT },
    { P_DIVINATION_SPELL, P_BASIC },{ P_ENCHANTMENT_SPELL, P_BASIC },
    { P_CLERIC_SPELL, P_SKILLED },  { P_ESCAPE_SPELL, P_BASIC },
    { P_MATTER_SPELL, P_BASIC },
    { P_MARTIAL_ARTS, P_GRAND_MASTER },
    { P_NONE, 0 }
};

static const struct def_skill Skill_P[] = {
    { P_CLUB, P_EXPERT },		{ P_MACE, P_EXPERT },
    { P_MORNING_STAR, P_EXPERT },	{ P_FLAIL, P_EXPERT },
    { P_HAMMER, P_EXPERT },		{ P_QUARTERSTAFF, P_EXPERT },
    { P_POLEARMS, P_SKILLED },		{ P_SPEAR, P_SKILLED },
    { P_JAVELIN, P_SKILLED },		{ P_TRIDENT, P_SKILLED },
    { P_LANCE, P_BASIC },		{ P_BOW, P_BASIC },
    { P_SLING, P_BASIC },		{ P_CROSSBOW, P_BASIC },
    { P_DART, P_BASIC },		{ P_SHURIKEN, P_BASIC },
    { P_BOOMERANG, P_BASIC },		{ P_UNICORN_HORN, P_SKILLED },
    { P_HEALING_SPELL, P_EXPERT },	{ P_DIVINATION_SPELL, P_EXPERT },
    { P_CLERIC_SPELL, P_EXPERT },
    { P_BARE_HANDED_COMBAT, P_BASIC },
    { P_NONE, 0 }
};

static const struct def_skill Skill_R[] = {
    { P_DAGGER, P_EXPERT },		{ P_KNIFE,  P_EXPERT },
    { P_SHORT_SWORD, P_EXPERT },	{ P_BROAD_SWORD, P_SKILLED },
    { P_LONG_SWORD, P_SKILLED },	{ P_TWO_HANDED_SWORD, P_BASIC },
    { P_SCIMITAR, P_SKILLED },		{ P_SABER, P_SKILLED },
    { P_CLUB, P_SKILLED },		{ P_MACE, P_SKILLED },
    { P_MORNING_STAR, P_BASIC },	{ P_FLAIL, P_BASIC },
    { P_HAMMER, P_BASIC },		{ P_POLEARMS, P_BASIC },
    { P_SPEAR, P_BASIC },		{ P_CROSSBOW, P_EXPERT },
    { P_DART, P_EXPERT },		{ P_SHURIKEN, P_SKILLED },
    { P_DIVINATION_SPELL, P_SKILLED },	{ P_ESCAPE_SPELL, P_SKILLED },
    { P_MATTER_SPELL, P_SKILLED },
    { P_RIDING, P_BASIC },
    { P_TWO_WEAPON_COMBAT, P_EXPERT },
    { P_BARE_HANDED_COMBAT, P_EXPERT },
    { P_NONE, 0 }
};

static const struct def_skill Skill_Ran[] = {
    { P_DAGGER, P_EXPERT },		 { P_KNIFE,  P_SKILLED },
    { P_AXE, P_SKILLED },	 { P_PICK_AXE, P_BASIC },
    { P_SHORT_SWORD, P_BASIC },	 { P_MORNING_STAR, P_BASIC },
    { P_FLAIL, P_SKILLED },	 { P_HAMMER, P_BASIC },
    { P_QUARTERSTAFF, P_BASIC }, { P_POLEARMS, P_SKILLED },
    { P_SPEAR, P_SKILLED },	 { P_JAVELIN, P_EXPERT },
    { P_TRIDENT, P_BASIC },	 { P_BOW, P_EXPERT },
    { P_SLING, P_EXPERT },	 { P_CROSSBOW, P_EXPERT },
    { P_DART, P_EXPERT },	 { P_SHURIKEN, P_SKILLED },
    { P_BOOMERANG, P_EXPERT },	 { P_WHIP, P_BASIC },
    { P_HEALING_SPELL, P_BASIC },
    { P_DIVINATION_SPELL, P_EXPERT },
    { P_ESCAPE_SPELL, P_BASIC },
    { P_RIDING, P_BASIC },
    { P_BARE_HANDED_COMBAT, P_BASIC },
    { P_NONE, 0 }
};

static const struct def_skill Skill_S[] = {
    { P_DAGGER, P_BASIC },		{ P_KNIFE,  P_SKILLED },
    { P_SHORT_SWORD, P_EXPERT },	{ P_BROAD_SWORD, P_SKILLED },
    { P_LONG_SWORD, P_EXPERT },		{ P_TWO_HANDED_SWORD, P_EXPERT },
    { P_SCIMITAR, P_BASIC },		{ P_SABER, P_BASIC },
    { P_FLAIL, P_SKILLED },		{ P_QUARTERSTAFF, P_BASIC },
    { P_POLEARMS, P_SKILLED },		{ P_SPEAR, P_BASIC },
    { P_JAVELIN, P_BASIC },		{ P_LANCE, P_SKILLED },
    { P_BOW, P_EXPERT },		{ P_SHURIKEN, P_EXPERT },
    { P_ATTACK_SPELL, P_SKILLED },	{ P_CLERIC_SPELL, P_SKILLED },
    { P_RIDING, P_SKILLED },
    { P_TWO_WEAPON_COMBAT, P_EXPERT },
    { P_MARTIAL_ARTS, P_MASTER },
    { P_NONE, 0 }
};

static const struct def_skill Skill_T[] = {
    { P_DAGGER, P_EXPERT },		{ P_KNIFE,  P_SKILLED },
    { P_AXE, P_BASIC },			{ P_PICK_AXE, P_BASIC },
    { P_SHORT_SWORD, P_EXPERT },	{ P_BROAD_SWORD, P_BASIC },
    { P_LONG_SWORD, P_BASIC },		{ P_TWO_HANDED_SWORD, P_BASIC },
    { P_SCIMITAR, P_SKILLED },		{ P_SABER, P_SKILLED },
    { P_MACE, P_BASIC },		{ P_MORNING_STAR, P_BASIC },
    { P_FLAIL, P_BASIC },		{ P_HAMMER, P_BASIC },
    { P_QUARTERSTAFF, P_BASIC },	{ P_POLEARMS, P_BASIC },
    { P_SPEAR, P_BASIC },		{ P_JAVELIN, P_BASIC },
    { P_TRIDENT, P_BASIC },		{ P_LANCE, P_BASIC },
    { P_BOW, P_BASIC },			{ P_SLING, P_BASIC },
    { P_CROSSBOW, P_BASIC },		{ P_DART, P_EXPERT },
    { P_SHURIKEN, P_BASIC },		{ P_BOOMERANG, P_BASIC },
    { P_WHIP, P_BASIC },		{ P_UNICORN_HORN, P_SKILLED },
    { P_DIVINATION_SPELL, P_BASIC },	{ P_ENCHANTMENT_SPELL, P_BASIC },
    { P_ESCAPE_SPELL, P_SKILLED },
    { P_RIDING, P_BASIC },
    { P_TWO_WEAPON_COMBAT, P_SKILLED },
    { P_BARE_HANDED_COMBAT, P_SKILLED },
    { P_NONE, 0 }
};

static const struct def_skill Skill_V[] = {
    { P_DAGGER, P_EXPERT },		{ P_AXE, P_EXPERT },
    { P_PICK_AXE, P_SKILLED },		{ P_SHORT_SWORD, P_SKILLED },
    { P_BROAD_SWORD, P_SKILLED },	{ P_LONG_SWORD, P_EXPERT },
    { P_TWO_HANDED_SWORD, P_EXPERT },	{ P_SCIMITAR, P_BASIC },
    { P_SABER, P_BASIC },		{ P_HAMMER, P_EXPERT },
    { P_QUARTERSTAFF, P_BASIC },	{ P_POLEARMS, P_SKILLED },
    { P_SPEAR, P_SKILLED },		{ P_JAVELIN, P_BASIC },
    { P_TRIDENT, P_BASIC },		{ P_LANCE, P_SKILLED },
    { P_SLING, P_BASIC },
    { P_ATTACK_SPELL, P_BASIC },	{ P_ESCAPE_SPELL, P_BASIC },
    { P_RIDING, P_SKILLED },
    { P_TWO_WEAPON_COMBAT, P_SKILLED },
    { P_BARE_HANDED_COMBAT, P_EXPERT },
    { P_NONE, 0 }
};

static const struct def_skill Skill_W[] = {
    { P_DAGGER, P_EXPERT },		{ P_KNIFE,  P_SKILLED },
    { P_AXE, P_SKILLED },		{ P_SHORT_SWORD, P_BASIC },
    { P_CLUB, P_SKILLED },		{ P_MACE, P_BASIC },
    { P_QUARTERSTAFF, P_EXPERT },	{ P_POLEARMS, P_SKILLED },
    { P_SPEAR, P_BASIC },		{ P_JAVELIN, P_BASIC },
    { P_TRIDENT, P_BASIC },		{ P_SLING, P_SKILLED },
    { P_DART, P_EXPERT },		{ P_SHURIKEN, P_BASIC },
    { P_ATTACK_SPELL, P_EXPERT },	{ P_HEALING_SPELL, P_SKILLED },
    { P_DIVINATION_SPELL, P_EXPERT },	{ P_ENCHANTMENT_SPELL, P_SKILLED },
    { P_CLERIC_SPELL, P_SKILLED },	{ P_ESCAPE_SPELL, P_EXPERT },
    { P_MATTER_SPELL, P_EXPERT },
    { P_RIDING, P_BASIC },
    { P_BARE_HANDED_COMBAT, P_BASIC },
    { P_NONE, 0 }
};


static struct trobj *copy_trobj_list(const struct trobj *list)
{
	struct trobj *copy;
	int len = 0;
	
	while (list[len].trotyp || list[len].trclass)
	    len++;
	len++; /* list is terminated by an entry of zeros */
	copy = malloc(len * sizeof(struct trobj));
	memcpy(copy, list, len * sizeof(struct trobj));
	
	return copy;
}

static void knows_object(int obj)
{
	discover_object(obj,TRUE,FALSE);
	objects[obj].oc_pre_discovered = 1;	/* not a "discovery" */
}

/* Know ordinary (non-magical) objects of a certain class,
 * like all gems except the loadstone and luckstone.
 */
static void knows_class(char sym)
{
	int ct;
	for (ct = 1; ct < NUM_OBJECTS; ct++)
		if (objects[ct].oc_class == sym && !objects[ct].oc_magic)
			knows_object(ct);
}

void u_init(void)
{
	int i;
	struct trobj *trobj_list = NULL;
	short nclist[4] = {STRANGE_OBJECT, STRANGE_OBJECT, STRANGE_OBJECT, STRANGE_OBJECT};

	flags.female = u.initgend;
	flags.beginner = 1;
	
	u.ustuck = NULL;

	u.uz.dlevel = 1;
	u.uz0.dlevel = 0;
	u.utolev = u.uz;

	u.umoved = FALSE;
	u.umortality = 0;
	u.ugrave_arise = NON_PM;

	u.umonnum = u.umonster = (flags.female &&
			urole.femalenum != NON_PM) ? urole.femalenum :
			urole.malenum;
	set_uasmon();

	u.ulevel = 0;	/* set up some of the initial attributes */
	u.uhp = u.uhpmax = newhp();
	u.uenmax = urole.enadv.infix + urace.enadv.infix;
	if (urole.enadv.inrnd > 0)
	    u.uenmax += rnd(urole.enadv.inrnd);
	if (urace.enadv.inrnd > 0)
	    u.uenmax += rnd(urace.enadv.inrnd);
	u.uen = u.uenmax;
	u.uspellprot = 0;
	adjabil(0,1);
	u.ulevel = u.ulevelmax = 1;

	init_uhunger();
	for (i = 0; i <= MAXSPELL; i++) spl_book[i].sp_id = NO_SPELL;
	u.ublesscnt = 300;			/* no prayers just yet */
	u.ualignbase[A_CURRENT] = u.ualignbase[A_ORIGINAL] = u.ualign.type =
			aligns[u.initalign].value;
	u.ulycn = NON_PM;

	u.ubirthday = turntime;

	/*
	 *  For now, everyone starts out with a night vision range of 1 and
	 *  their xray range disabled.
	 */
	u.nv_range   =  1;
	u.xray_range = -1;
	u.next_attr_check = 600; /* arbitrary initial setting */


	/*** Role-specific initializations ***/
	switch (Role_switch) {
	/* rn2(100) > 50 necessary for some choices because some
	 * random number generators are bad enough to seriously
	 * skew the results if we use rn2(2)...  --KAA
	 */
	case PM_ARCHEOLOGIST:
		ini_inv(Archeologist, nclist);
		if (!rn2(10)) ini_inv(Tinopener, nclist);
		else if (!rn2(4)) ini_inv(Lamp, nclist);
		else if (!rn2(10)) ini_inv(Magicmarker, nclist);
		knows_object(SACK);
		knows_object(TOUCHSTONE);
		skill_init(Skill_A);
		break;
	case PM_BARBARIAN:
		trobj_list = copy_trobj_list(Barbarian);
		if (rn2(100) >= 50) {	/* see above comment */
		    trobj_list[B_MAJOR].trotyp = BATTLE_AXE;
		    trobj_list[B_MINOR].trotyp = SHORT_SWORD;
		}
		ini_inv(trobj_list, nclist);
		if (!rn2(6)) ini_inv(Lamp, nclist);
		knows_class(WEAPON_CLASS);
		knows_class(ARMOR_CLASS);
		skill_init(Skill_B);
		break;
	case PM_CAVEMAN:
		trobj_list = copy_trobj_list(Cave_man);
		trobj_list[C_AMMO].trquan = rn1(11, 10);	/* 10..20 */
		ini_inv(trobj_list, nclist);
		skill_init(Skill_C);
		break;
	case PM_HEALER:
		u.umoney0 = rn1(1000, 1001);
		ini_inv(Healer, nclist);
		if (!rn2(25)) ini_inv(Lamp, nclist);
		knows_object(POT_FULL_HEALING);
		skill_init(Skill_H);
		break;
	case PM_KNIGHT:
		ini_inv(Knight, nclist);
		knows_class(WEAPON_CLASS);
		knows_class(ARMOR_CLASS);
		/* give knights chess-like mobility
		 * -- idea from wooledge@skybridge.scl.cwru.edu */
		HJumping |= FROMOUTSIDE;
		skill_init(Skill_K);
		break;
	case PM_MONK:
		trobj_list = copy_trobj_list(Monk);
		switch (rn2(90) / 30) {
		    case 0: trobj_list[M_BOOK].trotyp = SPE_HEALING; break;
		    case 1: trobj_list[M_BOOK].trotyp = SPE_PROTECTION; break;
		    case 2: trobj_list[M_BOOK].trotyp = SPE_SLEEP; break;
		}
		ini_inv(trobj_list, nclist);
		if (!rn2(5)) ini_inv(Magicmarker, nclist);
		else if (!rn2(10)) ini_inv(Lamp, nclist);
		knows_class(ARMOR_CLASS);
		skill_init(Skill_Mon);
		break;
	case PM_PRIEST:
		ini_inv(Priest, nclist);
		if (!rn2(10)) ini_inv(Magicmarker, nclist);
		else if (!rn2(10)) ini_inv(Lamp, nclist);
		knows_object(POT_WATER);
		skill_init(Skill_P);
		/* KMH, conduct --
		 * Some may claim that this isn't agnostic, since they
		 * are literally "priests" and they have holy water.
		 * But we don't count it as such.  Purists can always
		 * avoid playing priests and/or confirm another player's
		 * role in their YAAP.
		 */
		break;
	case PM_RANGER:
		trobj_list = copy_trobj_list(Ranger);
		trobj_list[RAN_TWO_ARROWS].trquan = rn1(10, 50);
		trobj_list[RAN_ZERO_ARROWS].trquan = rn1(10, 30);
		ini_inv(trobj_list, nclist);
		skill_init(Skill_Ran);
		break;
	case PM_ROGUE:
		trobj_list = copy_trobj_list(Rogue);
		trobj_list[R_DAGGERS].trquan = rn1(10, 6);
		u.umoney0 = 0;
		ini_inv(trobj_list, nclist);
		if (!rn2(5)) ini_inv(Blindfold, nclist);
		knows_object(SACK);
		skill_init(Skill_R);
		break;
	case PM_SAMURAI:
		trobj_list = copy_trobj_list(Samurai);
		trobj_list[S_ARROWS].trquan = rn1(20, 26);
		ini_inv(trobj_list, nclist);
		if (!rn2(5)) ini_inv(Blindfold, nclist);
		knows_class(WEAPON_CLASS);
		knows_class(ARMOR_CLASS);
		skill_init(Skill_S);
		break;
	case PM_TOURIST:
		trobj_list = copy_trobj_list(Tourist);
		trobj_list[T_DARTS].trquan = rn1(20, 21);
		u.umoney0 = rnd(1000);
		ini_inv(trobj_list, nclist);
		if (!rn2(25)) ini_inv(Tinopener, nclist);
		else if (!rn2(25)) ini_inv(Leash, nclist);
		else if (!rn2(25)) ini_inv(Towel, nclist);
		else if (!rn2(25)) ini_inv(Magicmarker, nclist);
		skill_init(Skill_T);
		break;
	case PM_VALKYRIE:
		ini_inv(Valkyrie, nclist);
		if (!rn2(6)) ini_inv(Lamp, nclist);
		knows_class(WEAPON_CLASS);
		knows_class(ARMOR_CLASS);
		skill_init(Skill_V);
		break;
	case PM_WIZARD:
		ini_inv(Wizard, nclist);
		if (!rn2(5)) ini_inv(Magicmarker, nclist);
		if (!rn2(5)) ini_inv(Blindfold, nclist);
		skill_init(Skill_W);
		break;

	default:	/* impossible */
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
	    /*
	     * Elves are people of music and song, or they are warriors.
	     * Non-warriors get an instrument.  We use a kludge to
	     * get only non-magic instruments.
	     */
	    if (Role_if (PM_PRIEST) || Role_if(PM_WIZARD)) {
		static const int trotyp[] = {
		    WOODEN_FLUTE, TOOLED_HORN, WOODEN_HARP,
		    BELL, BUGLE, LEATHER_DRUM
		};
		trobj_list = copy_trobj_list(Instrument);
		trobj_list[0].trotyp = trotyp[rn2(SIZE(trotyp))];
		ini_inv(trobj_list, nclist);
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
	    if (!Role_if (PM_WIZARD))
		ini_inv(Xtra_food, nclist);
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

	default:	/* impossible */
		break;
	}

	if (trobj_list)
	    free(trobj_list);

	if (discover)
		ini_inv(Wishing, nclist);

	if (u.umoney0) ini_inv(Money, nclist);
	u.umoney0 += hidden_gold();	/* in case sack has gold in it */

	find_ac();			/* get initial ac value */
	init_attr(75);			/* init attribute values */
	max_rank_sz();			/* set max str size for class ranks */
/*
 *	Do we really need this?
 */
	for (i = 0; i < A_MAX; i++)
	    if (!rn2(20)) {
		int xd = rn2(7) - 2;	/* biased variation */
		adjattrib(i, xd, TRUE);
		if (ABASE(i) < AMAX(i)) AMAX(i) = ABASE(i);
	    }

	/* make sure you can carry all you have - especially for Tourists */
	while (inv_weight() > 0) {
		if (adjattrib(A_STR, 1, TRUE)) continue;
		if (adjattrib(A_CON, 1, TRUE)) continue;
		/* only get here when didn't boost strength or constitution */
		break;
	}

	return;
}

/* skills aren't initialized, so we use the role-specific skill lists */
static boolean restricted_spell_discipline(int otyp)
{
    const struct def_skill *skills;
    int this_skill = spell_skilltype(otyp);

    switch (Role_switch) {
     case PM_ARCHEOLOGIST:	skills = Skill_A; break;
     case PM_BARBARIAN:		skills = Skill_B; break;
     case PM_CAVEMAN:		skills = Skill_C; break;
     case PM_HEALER:		skills = Skill_H; break;
     case PM_KNIGHT:		skills = Skill_K; break;
     case PM_MONK:		skills = Skill_Mon; break;
     case PM_PRIEST:		skills = Skill_P; break;
     case PM_RANGER:		skills = Skill_Ran; break;
     case PM_ROGUE:		skills = Skill_R; break;
     case PM_SAMURAI:		skills = Skill_S; break;
     case PM_TOURIST:		skills = Skill_T; break;
     case PM_VALKYRIE:		skills = Skill_V; break;
     case PM_WIZARD:		skills = Skill_W; break;
     default:			skills = 0; break;	/* lint suppression */
    }

    while (skills->skill != P_NONE) {
	if (skills->skill == this_skill) return FALSE;
	++skills;
    }
    return TRUE;
}

static void ini_inv(const struct trobj *trop, short nocreate[4])
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
			obj = mksobj(level, otyp, TRUE, FALSE);
		} else {	/* UNDEF_TYP */
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
			obj = mkobj(level, trop->trclass, FALSE);
			otyp = obj->otyp;
			while (otyp == WAN_WISHING
				|| otyp == nocreate[0]
				|| otyp == nocreate[1]
				|| otyp == nocreate[2]
				|| otyp == nocreate[3]
				|| (otyp == RIN_LEVITATION && flags.elbereth_enabled)
				/* 'useless' items */
				|| otyp == POT_HALLUCINATION
				|| otyp == POT_ACID
				|| otyp == SCR_AMNESIA
				|| otyp == SCR_FIRE
				|| otyp == SCR_BLANK_PAPER
				|| otyp == SPE_BLANK_PAPER
				|| otyp == RIN_AGGRAVATE_MONSTER
				|| otyp == RIN_HUNGER
				|| otyp == WAN_NOTHING
				/* Monks don't use weapons */
				|| (otyp == SCR_ENCHANT_WEAPON &&
				    Role_if (PM_MONK))
				/* wizard patch -- they already have one */
				|| (otyp == SPE_FORCE_BOLT &&
				    Role_if (PM_WIZARD))
				/* powerful spells are either useless to
				   low level players or unbalancing; also
				   spells in restricted skill categories */
				|| (obj->oclass == SPBOOK_CLASS &&
				    (objects[otyp].oc_level > 3 ||
				    restricted_spell_discipline(otyp)))
							) {
				dealloc_obj(obj);
				obj = mkobj(level, trop->trclass, FALSE);
				otyp = obj->otyp;
			}

			/* Don't start with +0 or negative rings */
			if (objects[otyp].oc_charged && obj->spe <= 0)
				obj->spe = rne(3);

			/* Heavily relies on the fact that 1) we create wands
			 * before rings, 2) that we create rings before
			 * spellbooks, and that 3) not more than 1 object of a
			 * particular symbol is to be prohibited.  (For more
			 * objects, we need more nocreate variables...)
			 */
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
			if (obj->oclass == RING_CLASS ||
			    obj->oclass == SPBOOK_CLASS)
				nocreate[3] = otyp;
		}

		if (trop->trclass == COIN_CLASS) {
			/* no "blessed" or "identified" money */
			obj->quan = u.umoney0;
		} else {
			obj->dknown = obj->bknown = obj->rknown = 1;
			if (objects[otyp].oc_uses_known) obj->known = 1;
			obj->cursed = 0;
			if (obj->opoisoned && u.ualign.type != A_CHAOTIC)
			    obj->opoisoned = 0;
			if (obj->oclass == WEAPON_CLASS ||
				obj->oclass == TOOL_CLASS) {
			    obj->quan = trquan;
			    trquan = 1;
			} else if (obj->oclass == GEM_CLASS &&
				is_graystone(obj) && obj->otyp != FLINT) {
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
			discover_object(otyp, TRUE, FALSE);
		if (otyp == OIL_LAMP)
			discover_object(POT_OIL, TRUE, FALSE);

		if (obj->oclass == ARMOR_CLASS){
			if (is_shield(obj) && !uarms) {
				setworn(obj, W_ARMS);
				if (uswapwep) setuswapwep(NULL);
			} else if (is_helmet(obj) && !uarmh)
				setworn(obj, W_ARMH);
			else if (is_gloves(obj) && !uarmg)
				setworn(obj, W_ARMG);
			else if (is_shirt(obj) && !uarmu)
				setworn(obj, W_ARMU);
			else if (is_cloak(obj) && !uarmc)
				setworn(obj, W_ARMC);
			else if (is_boots(obj) && !uarmf)
				setworn(obj, W_ARMF);
			else if (is_suit(obj) && !uarm)
				setworn(obj, W_ARM);
		}

		if (obj->oclass == WEAPON_CLASS || is_weptool(obj) ||
			otyp == TIN_OPENER || otyp == FLINT || otyp == ROCK) {
		    if (is_ammo(obj) || is_missile(obj)) {
			if (!uquiver) setuqwep(obj);
		    } else if (!uwep) setuwep(obj);
		    else if (!uswapwep) setuswapwep(obj);
		}
		if (obj->oclass == SPBOOK_CLASS &&
				obj->otyp != SPE_BLANK_PAPER)
		    initialspell(obj);

		if (--trquan) continue;	/* make a similar object */
		trop++;
		trquan = trop->trquan;
	}
}


void restore_you(struct memfile *mf, struct you *y)
{
	int i;
	unsigned int yflags, eflags, hflags;

	memset(y, 0, sizeof(struct you));
	
	yflags = mread32(mf);
	eflags = mread32(mf);
	hflags = mread32(mf);
	
	y->uswallow	= (yflags >> 31) & 1;
	y->uinwater	= (yflags >> 30) & 1;
	y->uundetected	= (yflags >> 29) & 1;
	y->mfemale	= (yflags >> 28) & 1;
	y->uinvulnerable= (yflags >> 27) & 1;
	y->uburied	= (yflags >> 26) & 1;
	y->uedibility	= (yflags >> 25) & 1;
	y->usick_type	= (yflags >> 23) & 3;
	
	y->uevent.minor_oracle		= (eflags >> 31) & 1;
	y->uevent.major_oracle		= (eflags >> 30) & 1;
	y->uevent.qcalled		= (eflags >> 29) & 1;
	y->uevent.qexpelled		= (eflags >> 28) & 1;
	y->uevent.qcompleted		= (eflags >> 27) & 1;
	y->uevent.uheard_tune		= (eflags >> 25) & 3;
	y->uevent.uopened_dbridge	= (eflags >> 24) & 1;
	y->uevent.invoked		= (eflags >> 23) & 1;
	y->uevent.gehennom_entered	= (eflags >> 22) & 1;
	y->uevent.uhand_of_elbereth	= (eflags >> 20) & 3;
	y->uevent.udemigod		= (eflags >> 19) & 1;
	y->uevent.ascended		= (eflags >> 18) & 1;
	
	y->uhave.amulet		= (hflags >> 31) & 1;
	y->uhave.bell		= (hflags >> 30) & 1;
	y->uhave.book		= (hflags >> 29) & 1;
	y->uhave.menorah	= (hflags >> 28) & 1;
	y->uhave.questart	= (hflags >> 27) & 1;
	
	y->uhp = mread32(mf);
	y->uhpmax = mread32(mf);
	y->uen = mread32(mf);
	y->uenmax = mread32(mf);
	y->ulevel = mread32(mf);
	y->umoney0 = mread32(mf);
	y->uexp = mread32(mf);
	y->urexp = mread32(mf);
	y->ulevelmax = mread32(mf);
	y->umonster = mread32(mf);
	y->umonnum = mread32(mf);
	y->mh = mread32(mf);
	y->mhmax = mread32(mf);
	y->mtimedone = mread32(mf);
	y->ulycn = mread32(mf);
	y->last_str_turn = mread32(mf);
	y->utrap = mread32(mf);
	y->utraptype = mread32(mf);
	y->uhunger = mread32(mf);
	y->uhs = mread32(mf);
	y->umconf = mread32(mf);
	y->nv_range = mread32(mf);
	y->xray_range = mread32(mf);
	y->bglyph = mread32(mf);
	y->cglyph = mread32(mf);
	y->bc_order = mread32(mf);
	y->bc_felt = mread32(mf);
	y->ucreamed = mread32(mf);
	y->uswldtim = mread32(mf);
	y->udg_cnt = mread32(mf);
	y->next_attr_check = mread32(mf);
	y->ualign.record = mread32(mf);
	y->ugangr = mread32(mf);
	y->ugifts = mread32(mf);
	y->ublessed = mread32(mf);
	y->ublesscnt = mread32(mf);
	y->ucleansed = mread32(mf);
	y->usleep = mread32(mf);
	y->uinvault = mread32(mf);
	y->ugallop = mread32(mf);
	y->urideturns = mread32(mf);
	y->umortality = mread32(mf);
	y->ugrave_arise = mread32(mf);
	y->weapon_slots = mread32(mf);
	y->skills_advanced = mread32(mf);
	y->initrole = mread32(mf);
	y->initrace = mread32(mf);
	y->initgend = mread32(mf);
	y->initalign = mread32(mf);
	y->uconduct.unvegetarian = mread32(mf);
	y->uconduct.unvegan = mread32(mf);
	y->uconduct.food = mread32(mf);
	y->uconduct.gnostic = mread32(mf);
	y->uconduct.weaphit = mread32(mf);
	y->uconduct.killer = mread32(mf);
	y->uconduct.literate = mread32(mf);
	y->uconduct.polypiles = mread32(mf);
	y->uconduct.polyselfs = mread32(mf);
	y->uconduct.wishes = mread32(mf);
	y->uconduct.wisharti = mread32(mf);
	
	y->ux = mread8(mf);
	y->uy = mread8(mf);
	y->dx = mread8(mf);
	y->dy = mread8(mf);
	y->tx = mread8(mf);
	y->ty = mread8(mf);
	y->ux0 = mread8(mf);
	y->uy0 = mread8(mf);
	y->uz.dnum = mread8(mf);
	y->uz.dlevel = mread8(mf);
	y->uz0.dnum = mread8(mf);
	y->uz0.dlevel = mread8(mf);
	y->utolev.dnum = mread8(mf);
	y->utolev.dlevel = mread8(mf);
	y->utotype = mread8(mf);
	y->umoved = mread8(mf);
	y->ualign.type = mread8(mf);
	y->ualignbase[0] = mread8(mf);
	y->ualignbase[1] = mread8(mf);
	y->uluck = mread8(mf);
	y->moreluck = mread8(mf);
	y->uhitinc = mread8(mf);
	y->udaminc = mread8(mf);
	y->uac = mread8(mf);
	y->uspellprot = mread8(mf);
	y->usptime = mread8(mf);
	y->uspmtime = mread8(mf);
	y->twoweap = mread8(mf);

	mread(mf, y->usick_cause, sizeof(y->usick_cause));
	mread(mf, y->urooms, sizeof(y->urooms));
	mread(mf, y->urooms0, sizeof(y->urooms0));
	mread(mf, y->uentered, sizeof(y->uentered));
	mread(mf, y->ushops, sizeof(y->ushops));
	mread(mf, y->ushops0, sizeof(y->ushops0));
	mread(mf, y->ushops_entered, sizeof(y->ushops_entered));
	mread(mf, y->ushops_left, sizeof(y->ushops_left));
	mread(mf, y->macurr.a, sizeof(y->macurr.a));
	mread(mf, y->mamax.a, sizeof(y->mamax.a));
	mread(mf, y->acurr.a, sizeof(y->acurr.a));
	mread(mf, y->aexe.a, sizeof(y->aexe.a));
	mread(mf, y->abon.a, sizeof(y->abon.a));
	mread(mf, y->amax.a, sizeof(y->amax.a));
	mread(mf, y->atemp.a, sizeof(y->atemp.a));
	mread(mf, y->atime.a, sizeof(y->atime.a));
	mread(mf, y->skill_record, sizeof(y->skill_record));
    
	for (i = 0; i <= LAST_PROP; i++) {
	    y->uprops[i].extrinsic = mread32(mf);
	    y->uprops[i].blocked = mread32(mf);
	    y->uprops[i].intrinsic = mread32(mf);
	}
	for (i = 0; i < P_NUM_SKILLS; i++) {
	    y->weapon_skills[i].skill = mread8(mf);
	    y->weapon_skills[i].max_skill = mread8(mf);
	    y->weapon_skills[i].advance = mread16(mf);
	}
}


void save_you(struct memfile *mf, struct you *y)
{
	int i;
	unsigned int yflags, eflags, hflags;
	yflags = (y->uswallow << 31) | (y->uinwater << 30) | (y->uundetected << 29) |
		(y->mfemale << 28) | (y->uinvulnerable << 27) | (y->uburied << 26) |
		(y->uedibility << 25) | (y->usick_type << 23);
	eflags = (y->uevent.minor_oracle << 31) | (y->uevent.major_oracle << 30) |
		(y->uevent.qcalled << 29) | (y->uevent.qexpelled << 28) |
		(y->uevent.qcompleted << 27) | (y->uevent.uheard_tune << 25) |
		(y->uevent.uopened_dbridge << 24) | (y->uevent.invoked << 23) |
		(y->uevent.gehennom_entered << 22) | (y->uevent.uhand_of_elbereth << 20) |
		(y->uevent.udemigod << 19) | (y->uevent.ascended << 18);
	hflags = (y->uhave.amulet << 31) | (y->uhave.bell << 30) |
		(y->uhave.book << 29) | (y->uhave.menorah << 28) |
		(y->uhave.questart << 27);

	mwrite32(mf, yflags);
	mwrite32(mf, eflags);
	mwrite32(mf, hflags);
	mwrite32(mf, y->uhp);
	mwrite32(mf, y->uhpmax);
	mwrite32(mf, y->uen);
	mwrite32(mf, y->uenmax);
	mwrite32(mf, y->ulevel);
	mwrite32(mf, y->umoney0);
	mwrite32(mf, y->uexp);
	mwrite32(mf, y->urexp);
	mwrite32(mf, y->ulevelmax);
	mwrite32(mf, y->umonster);
	mwrite32(mf, y->umonnum);
	mwrite32(mf, y->mh);
	mwrite32(mf, y->mhmax);
	mwrite32(mf, y->mtimedone);
	mwrite32(mf, y->ulycn);
	mwrite32(mf, y->last_str_turn);
	mwrite32(mf, y->utrap);
	mwrite32(mf, y->utraptype);
	mwrite32(mf, y->uhunger);
	mwrite32(mf, y->uhs);
	mwrite32(mf, y->umconf);
	mwrite32(mf, y->nv_range);
	mwrite32(mf, y->xray_range);
	mwrite32(mf, y->bglyph);
	mwrite32(mf, y->cglyph);
	mwrite32(mf, y->bc_order);
	mwrite32(mf, y->bc_felt);
	mwrite32(mf, y->ucreamed);
	mwrite32(mf, y->uswldtim);
	mwrite32(mf, y->udg_cnt);
	mwrite32(mf, y->next_attr_check);
	mwrite32(mf, y->ualign.record);
	mwrite32(mf, y->ugangr);
	mwrite32(mf, y->ugifts);
	mwrite32(mf, y->ublessed);
	mwrite32(mf, y->ublesscnt);
	mwrite32(mf, y->ucleansed);
	mwrite32(mf, y->usleep);
	mwrite32(mf, y->uinvault);
	mwrite32(mf, y->ugallop);
	mwrite32(mf, y->urideturns);
	mwrite32(mf, y->umortality);
	mwrite32(mf, y->ugrave_arise);
	mwrite32(mf, y->weapon_slots);
	mwrite32(mf, y->skills_advanced);
	mwrite32(mf, y->initrole);
	mwrite32(mf, y->initrace);
	mwrite32(mf, y->initgend);
	mwrite32(mf, y->initalign);
	mwrite32(mf, y->uconduct.unvegetarian);
	mwrite32(mf, y->uconduct.unvegan);
	mwrite32(mf, y->uconduct.food);
	mwrite32(mf, y->uconduct.gnostic);
	mwrite32(mf, y->uconduct.weaphit);
	mwrite32(mf, y->uconduct.killer);
	mwrite32(mf, y->uconduct.literate);
	mwrite32(mf, y->uconduct.polypiles);
	mwrite32(mf, y->uconduct.polyselfs);
	mwrite32(mf, y->uconduct.wishes);
	mwrite32(mf, y->uconduct.wisharti);
	
	mwrite8(mf, y->ux);
	mwrite8(mf, y->uy);
	mwrite8(mf, y->dx);
	mwrite8(mf, y->dy);
	mwrite8(mf, y->tx);
	mwrite8(mf, y->ty);
	mwrite8(mf, y->ux0);
	mwrite8(mf, y->uy0);
	mwrite8(mf, y->uz.dnum);
	mwrite8(mf, y->uz.dlevel);
	mwrite8(mf, y->uz0.dnum);
	mwrite8(mf, y->uz0.dlevel);
	mwrite8(mf, y->utolev.dnum);
	mwrite8(mf, y->utolev.dlevel);
	mwrite8(mf, y->utotype);
	mwrite8(mf, y->umoved);
	mwrite8(mf, y->ualign.type);
	mwrite8(mf, y->ualignbase[0]);
	mwrite8(mf, y->ualignbase[1]);
	mwrite8(mf, y->uluck);
	mwrite8(mf, y->moreluck);
	mwrite8(mf, y->uhitinc);
	mwrite8(mf, y->udaminc);
	mwrite8(mf, y->uac);
	mwrite8(mf, y->uspellprot);
	mwrite8(mf, y->usptime);
	mwrite8(mf, y->uspmtime);
	mwrite8(mf, y->twoweap);

	mwrite(mf, y->usick_cause, sizeof(y->usick_cause));
	mwrite(mf, y->urooms, sizeof(y->urooms));
	mwrite(mf, y->urooms0, sizeof(y->urooms0));
	mwrite(mf, y->uentered, sizeof(y->uentered));
	mwrite(mf, y->ushops, sizeof(y->ushops));
	mwrite(mf, y->ushops0, sizeof(y->ushops0));
	mwrite(mf, y->ushops_entered, sizeof(y->ushops_entered));
	mwrite(mf, y->ushops_left, sizeof(y->ushops_left));
	mwrite(mf, y->macurr.a, sizeof(y->macurr.a));
	mwrite(mf, y->mamax.a, sizeof(y->mamax.a));
	mwrite(mf, y->acurr.a, sizeof(y->acurr.a));
	mwrite(mf, y->aexe.a, sizeof(y->aexe.a));
	mwrite(mf, y->abon.a, sizeof(y->abon.a));
	mwrite(mf, y->amax.a, sizeof(y->amax.a));
	mwrite(mf, y->atemp.a, sizeof(y->atemp.a));
	mwrite(mf, y->atime.a, sizeof(y->atime.a));
	mwrite(mf, y->skill_record, sizeof(y->skill_record));
    
	for (i = 0; i <= LAST_PROP; i++) {
	    mwrite32(mf, y->uprops[i].extrinsic);
	    mwrite32(mf, y->uprops[i].blocked);
	    mwrite32(mf, y->uprops[i].intrinsic);
	}
	for (i = 0; i < P_NUM_SKILLS; i++) {
	    mwrite8(mf, y->weapon_skills[i].skill);
	    mwrite8(mf, y->weapon_skills[i].max_skill);
	    mwrite16(mf, y->weapon_skills[i].advance);
	}
}

/*u_init.c*/
