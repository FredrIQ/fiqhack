/* Copyright (c) NetHack Development Team 1992.			  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Relevent header information in rm.h and objclass.h. */

/* Object class names.  Used in object_detect(). */
const char * const oclass_names[] = {
/* 0*/	0,
	"illegal objects",
	"weapons",
	"armor",
	"rings",
/* 5*/	"amulets",
	"tools",
	"food",
	"potions",
	"scrolls",
/*10*/	"spellbooks",
	"wands",
	"coins",
	"rocks",
	"large stones",
/*15*/	"iron balls",
	"chains",
	"venoms"
};


const char * const warnexplain[] = {
	"unknown creature causing you worry",
	"unknown creature causing you concern",
	"unknown creature causing you anxiety",
	"unknown creature causing you disquiet",
	"unknown creature causing you alarm",
	"unknown creature causing you dread"
};


const struct nh_symdef warnsyms[WARNCOUNT] = {
	{'0', "warn1", CLR_WHITE},  	/* white warning  */
	{'1', "warn2", CLR_RED},	/* pink warning   */
	{'2', "warn3", CLR_RED},	/* red warning    */
	{'3', "warn4", CLR_RED},	/* ruby warning   */
	{'4', "warn5", CLR_MAGENTA},	/* purple warning */
	{'5', "warn6", CLR_BRIGHT_MAGENTA}/* black warning  */
};

const char * const defexplain[] = {
/* 0*/	"unexplored area",	/* unexplored */
	"solid rock",		/* stone */
	"wall",			/* vwall */
	"wall",			/* hwall */
	"wall",			/* tlcorn */
	"wall",			/* trcorn */
	"wall",			/* blcorn */
	"wall",			/* brcorn */
	"wall",			/* crwall */
	"wall",			/* tuwall */
/*10*/	"wall",			/* tdwall */
	"wall",			/* tlwall */
	"wall",			/* trwall */
	"doorway",		/* ndoor */
	"open door",		/* vodoor */
	"open door",		/* hodoor */
	"closed door",		/* vcdoor */
	"closed door",		/* hcdoor */
	"iron bars",		/* bars */
	"tree",			/* tree */
/*20*/	"the floor of a room",	/* room */
	"dark part of a room",	/* darkroom */
	"corridor",		/* dark corr */
	"lit corridor",		/* lit corr */
	"staircase up",		/* upstair */
	"staircase down",	/* dnstair */
	"ladder up",		/* upladder */
	"ladder down",		/* dnladder */
	"altar",		/* altar */
	"grave",		/* grave */
/*30*/	"opulent throne",	/* throne */
	"sink",			/* sink */
	"fountain",		/* fountain */
	"water",		/* pool */
	"ice",			/* ice */
	"molten lava",		/* lava */
	"lowered drawbridge",	/* vodbridge */
	"lowered drawbridge",	/* hodbridge */
	"raised drawbridge",	/* vcdbridge */
	"raised drawbridge",	/* hcdbridge */
/*40*/	"air",			/* open air */
	"cloud",		/* [part of] a cloud */
	"water"			/* under water */
};


const char * const trapexplain[] = {
	"arrow trap",
	"dart trap",
	"falling rock trap",
	"squeaky board",
	"bear trap",
	"land mine",
	"rolling boulder trap",
	"sleeping gas trap",
	"rust trap",
/*50*/	"fire trap",
	"pit",
	"spiked pit",
	"hole",
	"trap door",
	"teleportation trap",
	"level teleporter",
	"magic portal",
	"web",
	"statue trap",
/*60*/	"magic trap",
	"anti-magic field",
	"polymorph trap"
};

/*
 *  Default screen symbols with explanations and colors.
 */
const struct nh_symdef defsyms[] = {
/* 0*/	{' ', "unexplored",	CLR_GRAY},
	{' ', "stone",		CLR_GRAY},
	{'|', "vwall",		CLR_GRAY},
	{'-', "hwall",		CLR_GRAY},
	{'-', "tlcorn",		CLR_GRAY},
	{'-', "trcorn",		CLR_GRAY},
	{'-', "blcorn",		CLR_GRAY},
	{'-', "brcorn",		CLR_GRAY},
	{'-', "crwall",		CLR_GRAY},
	{'-', "tuwall",		CLR_GRAY},
/*10*/	{'-', "tdwall",		CLR_GRAY},
	{'|', "tlwall",		CLR_GRAY},
	{'|', "trwall",		CLR_GRAY},
	{'.', "ndoor",		CLR_GRAY},
	{'-', "vodoor",		CLR_BROWN},
	{'|', "hodoor",		CLR_BROWN},
	{'+', "vcdoor",		CLR_BROWN},
	{'+', "hcdoor",		CLR_BROWN},
	{'#', "bars",		HI_METAL},
	{'#', "tree",		CLR_GREEN},
/*20*/	{'.', "room",		CLR_GRAY},
	{' ', "darkroom",	CLR_GRAY},
	{'#', "corr",		CLR_GRAY},
	{'#', "litcorr",	CLR_WHITE},
	{'<', "upstair",	CLR_GRAY},
	{'>', "dnstair",	CLR_GRAY},
	{'<', "upladder",	CLR_BROWN},
	{'>', "dnladder",	CLR_BROWN},
	{'_', "altar",		CLR_GRAY},
	{'|', "grave",		CLR_GRAY},
/*30*/	{'\\',"throne",		HI_GOLD},
	{'#', "sink",		CLR_GRAY},
	{'{', "fountain",	CLR_BLUE},
	{'}', "pool",		CLR_BLUE},
	{'.', "ice",		CLR_CYAN},
	{'}', "lava",		CLR_RED},
	{'.', "vodbridge",	CLR_BROWN},
	{'.', "hodbridge",	CLR_BROWN},
	{'#', "vcdbridge",	CLR_BROWN},
	{'#', "hcdbridge",	CLR_BROWN},
/*40*/	{' ', "air",		CLR_CYAN},
	{'#', "cloud",		CLR_GRAY},
	{'}', "water",		CLR_BLUE}
};

static const struct nh_symdef trapsyms[] = {
/* 0*/	{'^', "arrow trap",		HI_METAL},
	{'^', "dart trap",		HI_METAL},
	{'^', "falling rock trap",	CLR_GRAY},
	{'^', "squeaky board",		CLR_BROWN},
	{'^', "bear trap",		HI_METAL},
	{'^', "land mine",		CLR_RED},
	{'^', "rolling boulder trap",	CLR_GRAY},
	{'^', "sleeping gas trap",	HI_ZAP},
	{'^', "rust trap",		CLR_BLUE},
	{'^', "fire trap",		CLR_ORANGE},
/*10*/	{'^', "pit",			CLR_BLACK},
	{'^', "spiked pit",		CLR_BLACK},
	{'^', "hole",			CLR_BROWN},
	{'^', "trap door",		CLR_BROWN},
	{'^', "teleportation trap",	CLR_MAGENTA},
	{'^', "level teleporter",	CLR_MAGENTA},
	{'^', "magic portal",		CLR_BRIGHT_MAGENTA},
	{'"', "web",			CLR_GRAY},
	{'^', "statue trap",		CLR_GRAY},
	{'^', "magic trap",		HI_ZAP},
/*20*/	{'^', "anti-magic field",	HI_ZAP},
	{'^', "polymorph trap",		CLR_BRIGHT_GREEN}
};


static const struct nh_symdef explsyms[] = {
	{'/', "exp_top_l",	CLR_ORANGE},	/* explosion top left     */
	{'-', "exp_top_c",	CLR_ORANGE},	/* explosion top center   */
	{'\\',"exp_top_r",	CLR_ORANGE},	/* explosion top right    */
	{'|', "exp_mid_l",	CLR_ORANGE},	/* explosion middle left  */
	{' ', "exp_mid_c",	CLR_ORANGE},	/* explosion middle center*/
	{'|', "exp_mid_r",	CLR_ORANGE},	/* explosion middle right */
	{'\\',"exp_bot_l",	CLR_ORANGE},	/* explosion bottom left  */
	{'-', "exp_bot_c",	CLR_ORANGE},	/* explosion bottom center*/
	{'/', "exp_bot_r",	CLR_ORANGE},	/* explosion bottom right */
};

static const struct nh_symdef expltypes[] = {
	{0, "dark",	CLR_BLACK},
	{0, "noxious",	CLR_GREEN},
	{0, "muddy",	CLR_BROWN},
	{0, "wet",	CLR_BLUE},
	{0, "magical",	CLR_MAGENTA},
	{0, "fiery",	CLR_ORANGE},
	{0, "frosty",	CLR_WHITE}
};

static const struct nh_symdef zapsyms[NUM_ZAP] = {
	{'|', "zap_v",	HI_ZAP},
	{'-', "zap_h",	HI_ZAP},
	{'\\',"zap_ld",	HI_ZAP},
	{'/', "zap_rd",	HI_ZAP}
};

/*
 *  This must be the same order as used for buzz() in zap.c.
 */
static const struct nh_symdef zaptypes[NUM_ZAP] = {
	{0, "missile",	HI_ZAP},
	{0, "fire",	CLR_ORANGE},
	{0, "frost",	CLR_WHITE},
	{0, "sleep",	HI_ZAP},
	{0, "death",	CLR_BLACK},
	{0, "lightning",CLR_WHITE},
	{0, "poison gas",CLR_YELLOW},
	{0, "acid",	CLR_GREEN}
};

static const struct nh_symdef effectsyms[] = {
	{'*', "digbeam",	CLR_WHITE},	/* dig beam */
	{'!', "flashbeam",	CLR_WHITE},	/* camera flash beam */
	{')', "boomleft",	HI_WOOD},	/* boomerang open left */
	{'(', "boomright",	HI_WOOD},	/* boomerang open right */
	{'0', "shield1",	HI_ZAP},	/* 4 magic shield symbols */
	{'#', "shield2",	HI_ZAP},
	{'@', "shield3",	HI_ZAP},
	{'*', "shield4",	HI_ZAP},
	{'#', "gascloud",	CLR_GRAY}
};

static const struct nh_symdef swallowsyms[] = {
	{'/', "swallow_top_l",	CLR_ORANGE},	/* swallow top left     */
	{'-', "swallow_top_c",	CLR_ORANGE},	/* swallow top center   */
	{'\\',"swallow_top_r",	CLR_ORANGE},	/* swallow top right    */
	{'|', "swallow_mid_l",	CLR_ORANGE},	/* swallow middle left  */
	{'|', "swallow_mid_r",	CLR_ORANGE},	/* swallow middle right */
	{'\\',"swallow_bot_l",	CLR_ORANGE},	/* swallow bottom left  */
	{'-', "swallow_bot_c",	CLR_ORANGE},	/* swallow bottom center*/
	{'/', "swallow_bot_r",	CLR_ORANGE},	/* swallow bottom right */
};

static int unnamed_cnt[MAXOCLASSES];
/*
 * make unique, non-null names for all objects
 */
static char *make_object_name(int otyp)
{
    char buffer[41], buf2[41];
    const char *nameptr, *classname;
    char *ret;
    int class = (int)const_objects[otyp].oc_class;
    
    
    buffer[0] = buf2[0] = '\0';
    buffer[40] = buf2[0] = '\0';

    classname = oclass_names[class];
    nameptr = obj_descr[otyp].oc_name;
   
    /* catch dummy objects (scrolls, wands, ...) without names */
    if (!nameptr) {
	unnamed_cnt[(int)const_objects[otyp].oc_class]++;
	snprintf(buf2, 40, "unnamed %d", unnamed_cnt[class]);
    }
    
    if (class == AMULET_CLASS &&
	const_objects[otyp].oc_material == PLASTIC) {
	snprintf(buf2, 40, "fake amulet of yendor");
    } else if (class == GEM_CLASS &&
	const_objects[otyp].oc_material == GLASS) {
	snprintf(buf2, 40, "%s glass gem", obj_descr[otyp].oc_descr);
    }

    if (buf2[0])
	nameptr = buf2;
    
    /* add a prefix to some object types to disambiguate
     * (wand of) fire from (scroll of) fire */
    if (class == WAND_CLASS ||
	class == RING_CLASS ||
	class == POTION_CLASS ||
	class == SPBOOK_CLASS ||
	class == SCROLL_CLASS)
	snprintf(buffer, 40, "%3.3s %s", classname, nameptr);
    else
	snprintf(buffer, 40, "%s", nameptr);
    
    ret = xmalloc(strlen(buffer) + 1);
    strcpy(ret, buffer);
    
    return ret;
}

#include <stdio.h>
struct nh_drawing_info *nh_get_drawing_info(void)
{
    int i;
    struct nh_symdef *tmp;
    struct nh_drawing_info *di = xmalloc(sizeof(struct nh_drawing_info));
    
    di->num_bgelements = SIZE(defsyms);
    di->bgelements = (struct nh_symdef *)defsyms;
    
    di->num_traps = SIZE(trapsyms);
    di->traps = (struct nh_symdef *)trapsyms;
    
    di->num_objects = NUM_OBJECTS;
    tmp = xmalloc(sizeof(struct nh_symdef) * di->num_objects);
    for (i = 0; i < di->num_objects; i++) {
	tmp[i].ch = def_oc_syms[(int)const_objects[i].oc_class];
	tmp[i].symname = make_object_name(i);
	tmp[i].color = const_objects[i].oc_color;
    }
    di->objects = tmp;
    
    tmp = xmalloc(sizeof(struct nh_symdef));
    tmp->ch = DEF_INVISIBLE;
    tmp->color = NO_COLOR;
    tmp->symname = "invisible monster";
    di->invis = tmp;
    
    di->num_monsters = NUMMONS;
    tmp = xmalloc(sizeof(struct nh_symdef) * di->num_monsters);
    for (i = 0; i < di->num_monsters; i++) {
	tmp[i].ch = def_monsyms[(int)mons[i].mlet];
	tmp[i].symname = mons[i].mname;
	tmp[i].color = mons[i].mcolor;
    }
    di->monsters = tmp;
    
    di->num_warnings = SIZE(warnsyms);
    di->warnings = (struct nh_symdef *)warnsyms;
    
    di->num_expltypes = SIZE(expltypes);
    di->expltypes = (struct nh_symdef *)expltypes;
    di->explsyms = (struct nh_symdef *)explsyms;
    
    di->num_zaptypes = SIZE(zaptypes);
    di->zaptypes = (struct nh_symdef *)zaptypes;
    di->zapsyms = (struct nh_symdef *)zapsyms;
    
    di->num_effects = SIZE(effectsyms);
    di->effects = (struct nh_symdef *)effectsyms;
    
    di->swallowsyms = (struct nh_symdef *)swallowsyms;
    
    return di;
}

/*drawing.c*/
