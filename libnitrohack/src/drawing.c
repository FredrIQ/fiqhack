/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) NetHack Development Team 1992.                   */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Relevent header information in rm.h and objclass.h. */

/* Object class names.  Used in object_detect(). */
const char *const oclass_names[] = {
/* 0*/ 0,
    "illegal objects",
    "weapons",
    "armor",
    "rings",
/* 5*/ "amulets",
    "tools",
    "food",
    "potions",
    "scrolls",
/*10*/ "spellbooks",
    "wands",
    "coins",
    "rocks",
    "large stones",
/*15*/ "iron balls",
    "chains",
    "venoms"
};


const char *const warnexplain[] = {
    "unknown creature causing you worry",
    "unknown creature causing you concern",
    "unknown creature causing you anxiety",
    "unknown creature causing you disquiet",
    "unknown creature causing you alarm",
    "unknown creature causing you dread"
};


const struct nh_symdef warnsyms[WARNCOUNT] = {
    {'0', "warn1", CLR_WHITE},  /* white warning */
    {'1', "warn2", CLR_RED},    /* pink warning */
    {'2', "warn3", CLR_RED},    /* red warning */
    {'3', "warn4", CLR_RED},    /* ruby warning */
    {'4', "warn5", CLR_MAGENTA},        /* purple warning */
    {'5', "warn6", CLR_BRIGHT_MAGENTA}  /* black warning */
};

const char *const defexplain[] = {
    /* 0 */ "unexplored area",
    /* unexplored */
    "solid rock",       /* stone */
    "wall",     /* vwall */
    "wall",     /* hwall */
    "wall",     /* tlcorn */
    "wall",     /* trcorn */
    "wall",     /* blcorn */
    "wall",     /* brcorn */
    "wall",     /* crwall */
    "wall",     /* tuwall */
    /* 10 */ "wall",
    /* tdwall */
    "wall",     /* tlwall */
    "wall",     /* trwall */
    "corridor", /* dark corr */
    "lit corridor",     /* lit corr */
    "the floor of a room",      /* room */
    "dark part of a room",      /* darkroom */
    "water",    /* pool */
    "air",      /* open air */
    "cloud",    /* [part of] a cloud */
    /* 20 */ "water",
    /* under water */
    "ice",      /* ice */
    "molten lava",      /* lava */
    "doorway",  /* ndoor */
    /* "features" start here */
    "open door",        /* vodoor */
    "open door",        /* hodoor */
    "closed door",      /* vcdoor */
    "closed door",      /* hcdoor */
    "iron bars",        /* bars */
    "tree",     /* tree */
    /* 30 */ "staircase up",
    /* upstair */
    "staircase down",   /* dnstair */
    "ladder up",        /* upladder */
    "ladder down",      /* dnladder */
    "long staircase up",        /* upsstair */
    "long staircase down",      /* dnsstair */
    "altar",    /* altar */
    "grave",    /* grave */
    "opulent throne",   /* throne */
    "sink",     /* sink */
    /* 40 */ "fountain",
    /* fountain */
    "lowered drawbridge",       /* vodbridge */
    "lowered drawbridge",       /* hodbridge */
    "raised drawbridge",        /* vcdbridge */
    "raised drawbridge",        /* hcdbridge */
};


const char *const trapexplain[] = {
    "arrow trap",
    "dart trap",
    "falling rock trap",
    "squeaky board",
    "bear trap",
    "land mine",
    "rolling boulder trap",
    "sleeping gas trap",
    "rust trap",
/*50*/ "fire trap",
    "pit",
    "spiked pit",
    "hole",
    "trap door",
    "vibrating square",
    "teleportation trap",
    "level teleporter",
    "magic portal",
    "web",
/*60*/ "statue trap",
    "magic trap",
    "anti-magic field",
    "polymorph trap"
};

/*
 *  Default screen symbols with explanations and colors.
 */
const struct nh_symdef defsyms[] = {
/* 0*/ {' ', "unexplored", CLR_GRAY},
    {' ', "stone", CLR_GRAY},
    {'|', "vwall", CLR_GRAY},
    {'-', "hwall", CLR_GRAY},
    {'-', "tlcorn", CLR_GRAY},
    {'-', "trcorn", CLR_GRAY},
    {'-', "blcorn", CLR_GRAY},
    {'-', "brcorn", CLR_GRAY},
    {'-', "crwall", CLR_GRAY},
    {'-', "tuwall", CLR_GRAY},
/*10*/ {'-', "tdwall", CLR_GRAY},
    {'|', "tlwall", CLR_GRAY},
    {'|', "trwall", CLR_GRAY},
    {'#', "corr", CLR_GRAY},
    {'#', "litcorr", CLR_WHITE},
    {'.', "room", CLR_GRAY},
    {'.', "darkroom", CLR_BLACK},
    {'}', "pool", CLR_BLUE},
    {' ', "air", CLR_CYAN},
    {'#', "cloud", CLR_GRAY},
/*20*/ {'}', "water", CLR_BLUE},
    {'.', "ice", CLR_CYAN},
    {'}', "lava", CLR_RED},
    {'.', "ndoor", CLR_GRAY},

    {'-', "vodoor", CLR_BROWN},
    {'|', "hodoor", CLR_BROWN},
    {'+', "vcdoor", CLR_BROWN},
    {'+', "hcdoor", CLR_BROWN},
    {'#', "bars", HI_METAL},
    {'#', "tree", CLR_GREEN},
/*30*/ {'<', "upstair", CLR_WHITE},
    {'>', "dnstair", CLR_WHITE},
    {'<', "upladder", CLR_YELLOW},
    {'>', "dnladder", CLR_YELLOW},
    {'<', "upsstair", CLR_YELLOW},
    {'>', "dnsstair", CLR_YELLOW},
    {'_', "altar", CLR_GRAY},
    {'|', "grave", CLR_GRAY},
    {'\\', "throne", HI_GOLD},
    {'#', "sink", CLR_GRAY},
/*40*/ {'{', "fountain", CLR_BLUE},
    {'#', "vodbridge", CLR_BROWN},
    {'#', "hodbridge", CLR_BROWN},
    {'+', "vcdbridge", CLR_YELLOW},
    {'+', "hcdbridge", CLR_YELLOW}
};

static const struct nh_symdef trapsyms[] = {
/* 0*/ {'^', "arrow trap", HI_METAL},
    {'^', "dart trap", HI_METAL},
    {'^', "falling rock trap", CLR_GRAY},
    {'^', "squeaky board", CLR_BROWN},
    {'^', "bear trap", HI_METAL},
    {'^', "land mine", CLR_RED},
    {'^', "rolling boulder trap", CLR_GRAY},
    {'^', "sleeping gas trap", HI_ZAP},
    {'^', "rust trap", CLR_BLUE},
    {'^', "fire trap", CLR_ORANGE},
/*10*/ {'^', "pit", CLR_BLACK},
    {'^', "spiked pit", CLR_BLACK},
    {'^', "hole", CLR_BROWN},
    {'^', "trap door", CLR_BROWN},
    {'^', "vibrating square", CLR_YELLOW},
    {'^', "teleportation trap", CLR_MAGENTA},
    {'^', "level teleporter", CLR_MAGENTA},
    {'^', "magic portal", CLR_BRIGHT_MAGENTA},
    {'^', "web", CLR_GRAY},
    {'^', "statue trap", CLR_GRAY},
/*20*/ {'^', "magic trap", HI_ZAP},
    {'^', "anti-magic field", HI_ZAP},
    {'^', "polymorph trap", CLR_BRIGHT_GREEN}
};


static const struct nh_symdef explsyms[] = {
    {'/', "exp_top_l", CLR_ORANGE},     /* explosion top left */
    {'-', "exp_top_c", CLR_ORANGE},     /* explosion top center */
    {'\\', "exp_top_r", CLR_ORANGE},    /* explosion top right */
    {'|', "exp_mid_l", CLR_ORANGE},     /* explosion middle left */
    {' ', "exp_mid_c", CLR_ORANGE},     /* explosion middle center */
    {'|', "exp_mid_r", CLR_ORANGE},     /* explosion middle right */
    {'\\', "exp_bot_l", CLR_ORANGE},    /* explosion bottom left */
    {'-', "exp_bot_c", CLR_ORANGE},     /* explosion bottom center */
    {'/', "exp_bot_r", CLR_ORANGE},     /* explosion bottom right */
};

static const struct nh_symdef expltypes[] = {
    {0, "dark", CLR_BLACK},
    {0, "noxious", CLR_GREEN},
    {0, "muddy", CLR_BROWN},
    {0, "wet", CLR_BLUE},
    {0, "magical", CLR_MAGENTA},
    {0, "fiery", CLR_ORANGE},
    {0, "frosty", CLR_WHITE}
};

static const struct nh_symdef zapsyms[NUM_ZAP] = {
    {'|', "zap_v", HI_ZAP},
    {'-', "zap_h", HI_ZAP},
    {'\\', "zap_ld", HI_ZAP},
    {'/', "zap_rd", HI_ZAP}
};

/*
 *  This must be the same order as used for buzz() in zap.c.
 */
static const struct nh_symdef zaptypes[NUM_ZAP] = {
    {0, "missile", HI_ZAP},
    {0, "fire", CLR_ORANGE},
    {0, "frost", CLR_WHITE},
    {0, "sleep", HI_ZAP},
    {0, "death", CLR_BLACK},
    {0, "lightning", CLR_WHITE},
    {0, "poison gas", CLR_YELLOW},
    {0, "acid", CLR_GREEN}
};

static const struct nh_symdef effectsyms[] = {
    {'*', "digbeam", CLR_WHITE},        /* dig beam */
    {'!', "flashbeam", CLR_WHITE},      /* camera flash beam */
    {')', "boomleft", HI_WOOD}, /* boomerang open left */
    {'(', "boomright", HI_WOOD},        /* boomerang open right */
    {'0', "shield1", HI_ZAP},   /* 4 magic shield symbols */
    {'#', "shield2", HI_ZAP},
    {'@', "shield3", HI_ZAP},
    {'*', "shield4", HI_ZAP},
    {'#', "gascloud", CLR_GRAY}
};

static const struct nh_symdef swallowsyms[] = {
    {'/', "swallow_top_l", CLR_ORANGE}, /* swallow top left */
    {'-', "swallow_top_c", CLR_ORANGE}, /* swallow top center */
    {'\\', "swallow_top_r", CLR_ORANGE},        /* swallow top right */
    {'|', "swallow_mid_l", CLR_ORANGE}, /* swallow middle left */
    {'|', "swallow_mid_r", CLR_ORANGE}, /* swallow middle right */
    {'\\', "swallow_bot_l", CLR_ORANGE},        /* swallow bottom left */
    {'-', "swallow_bot_c", CLR_ORANGE}, /* swallow bottom center */
    {'/', "swallow_bot_r", CLR_ORANGE}, /* swallow bottom right */
};

static int unnamed_cnt[MAXOCLASSES];

/*
 * make unique, non-null names for all objects
 */
static char *
make_object_name(int otyp)
{
    char buffer[41], buf2[41];
    const char *nameptr;
    char *ret;
    int class = (int)const_objects[otyp].oc_class;


    buffer[0] = buf2[0] = '\0';
    buffer[40] = buf2[0] = '\0';

    nameptr = obj_descr[otyp].oc_name;

    /* catch dummy objects (scrolls, wands, ...) without names */
    if (!nameptr) {
        unnamed_cnt[(int)const_objects[otyp].oc_class]++;
        snprintf(buf2, 40, "unnamed %d", unnamed_cnt[class]);
    }

    if (class == AMULET_CLASS && const_objects[otyp].oc_material == PLASTIC) {
        snprintf(buf2, 40, "fake amulet of yendor");
    } else if (class == GEM_CLASS && const_objects[otyp].oc_material == GLASS) {
        snprintf(buf2, 40, "%s glass gem", obj_descr[otyp].oc_descr);
    }

    if (buf2[0])
        nameptr = buf2;

    /* add a prefix to some object types to disambiguate (wand of) fire from
       (scroll of) fire */
    if (class == WAND_CLASS)
        snprintf(buffer, 40, "wand of %s", nameptr);
    else if (class == RING_CLASS)
        snprintf(buffer, 40, "ring of %s", nameptr);
    else if (class == POTION_CLASS)
        snprintf(buffer, 40, "potion of %s", nameptr);
    else if (class == SPBOOK_CLASS)
        snprintf(buffer, 40, "spellbook of %s", nameptr);
    else if (class == SCROLL_CLASS)
        snprintf(buffer, 40, "scroll of %s", nameptr);
    else
        snprintf(buffer, 40, "%s", nameptr);

    ret = xmalloc(strlen(buffer) + 1);
    strcpy(ret, buffer);

    return ret;
}


static const char *
make_mon_name(int mnum)
{
    char *name;

    if (mons[mnum].mlet == S_HUMAN && !strncmp(mons[mnum].mname, "were", 4)) {
        name = xmalloc(strlen(mons[mnum].mname) + strlen("human ") + 1);
        sprintf(name, "human %s", mons[mnum].mname);
        return name;
    }
    return mons[mnum].mname;
}


#include <stdio.h>
struct nh_drawing_info *
nh_get_drawing_info(void)
{
    int i;
    struct nh_symdef *tmp;
    struct nh_drawing_info *di = xmalloc(sizeof (struct nh_drawing_info));

    di->num_bgelements = SIZE(defsyms);
    di->bgelements = (struct nh_symdef *)defsyms;

    di->num_traps = SIZE(trapsyms);
    di->traps = (struct nh_symdef *)trapsyms;

    di->num_objects = NUM_OBJECTS;
    tmp = xmalloc(sizeof (struct nh_symdef) * di->num_objects);
    for (i = 0; i < di->num_objects; i++) {
        tmp[i].ch = def_oc_syms[(int)const_objects[i].oc_class];
        tmp[i].symname = make_object_name(i);
        tmp[i].color = const_objects[i].oc_color;
    }
    di->objects = tmp;

    tmp = xmalloc(sizeof (struct nh_symdef));
    tmp->ch = DEF_INVISIBLE;
    tmp->color = CLR_BRIGHT_BLUE;
    tmp->symname = "invisible monster";
    di->invis = tmp;

    di->num_monsters = NUMMONS;
    tmp = xmalloc(sizeof (struct nh_symdef) * di->num_monsters);
    for (i = 0; i < di->num_monsters; i++) {
        tmp[i].ch = def_monsyms[(int)mons[i].mlet];
        tmp[i].symname = make_mon_name(i);
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

    di->bg_feature_offset = DUNGEON_FEATURE_OFFSET;

    return di;
}

/*drawing.c*/
