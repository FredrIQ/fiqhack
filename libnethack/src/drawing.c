/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-13 */
/* Copyright (c) NetHack Development Team 1992.                   */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "xmalloc.h"

#include <stdio.h>


/* Be very, very careful about what functions you call from this file. It's
   linked into the tiles ports, and other things that aren't part of libnethack;
   the basic rule is that this file serves the purpose of explaining to other
   parts of NetHack 4 (libnethack, nethack clients, tilesequence.c):

   * What exists in the game;
   * What the things that exist in the game are for.

   The current list of dependencies is:

   xmalloc.c
   objects.c
   symclass.c
   monst.c
   hacklib.c

   Apart from xmalloc.c, which is basically just memory allocation functions,
   these are basically just files like this one that declare arrays of strings
   and accessor functions. Please keep it that way; there should be no game
   logic reachable via this file at all. In particular, avoid any kind of state
   in the referenced files.
 */

static struct xmalloc_block *xm_drawing = 0;

/* Relevant header information in rm.h and objclass.h. */

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

const char *const invismonexplain = "invisible monster";

const struct nh_symdef warnsyms[WARNCOUNT] = {
    {'0', "warn1", CLR_WHITE},  /* white warning */
    {'1', "warn2", CLR_RED},    /* pink warning */
    {'2', "warn3", CLR_RED},    /* red warning */
    {'3', "warn4", CLR_RED},    /* ruby warning */
    {'4', "warn5", CLR_MAGENTA},        /* purple warning */
    {'5', "warn6", CLR_BRIGHT_MAGENTA}  /* black warning */
};

/* Note: these descriptions are used to name tiles, so should not be changed
   without a good reason. If two descriptions are the same, they must be
   consecutive in the list; and the list itself must be in the same order as
   the S_ symbols in rm.h. */
const char *const defexplain[] = {
     /* 0 */ "unexplored area",     /* unexplored */
             "solid rock",          /* stone */
             "wall",                /* vwall */
             "wall",                /* hwall */
             "wall",                /* tlcorn */
             "wall",                /* trcorn */
             "wall",                /* blcorn */
             "wall",                /* brcorn */
             "wall",                /* crwall */
             "wall",                /* tuwall */
    /* 10 */ "wall"                 /* tdwall */,
             "wall",                /* tlwall */
             "wall",                /* trwall */
             "corridor",            /* corr */
             "the floor of a room", /* room */
             "water",               /* pool */
             "air",                 /* open air */
             "cloud",               /* [part of] a cloud */
             "underwater",          /* under water */
             "ice",                 /* ice */
    /* 20 */ "molten lava",         /* lava */
             "doorway",             /* ndoor */
             /* "features" start here */
             "open door",           /* vodoor */
             "open door",           /* hodoor */
             "closed door",         /* vcdoor */
             "closed door",         /* hcdoor */
             "iron bars",           /* bars */
             "tree",                /* tree */
             "staircase up",        /* upstair */
             "staircase down",      /* dnstair */
    /* 30 */ "ladder up",           /* upladder */
             "ladder down",         /* dnladder */
             "long staircase up",   /* upsstair */
             "long staircase down", /* dnsstair */
             "altar",               /* altar */
             "lawful altar",        /* laltar */
             "neutral altar",       /* naltar */
             "chaotic altar",       /* caltar */
             "unaligned altar",     /* ualtar */
             "aligned altar",       /* aaltar */
             "grave",               /* grave */
             "opulent throne",      /* throne */
             "sink",                /* sink */
             "fountain",            /* fountain */
             "lowered drawbridge",  /* vodbridge */
    /* 40 */ "lowered drawbridge",  /* hodbridge */
             "raised drawbridge",   /* vcdbridge */
             "raised drawbridge",   /* hcdbridge */
};

/* Note: this starts at 1 not 0, there's a shift of 1 used for uses of it. */
const char *const trapexplain[] = {
/* 1*/ "arrow trap",
    "dart trap",
    "falling rock trap",
    "squeaky board",
    "bear trap",
    "land mine",
    "rolling boulder trap",
    "sleeping gas trap",
    "rust trap",
/*10*/ "fire trap",
    "pit",
    "spiked pit",
    "hole",
    "trap door",
    "vibrating square",
    "teleportation trap",
    "level teleporter",
    "magic portal",
    "web",
/*20*/ "statue trap",
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
    {'.', "room", CLR_GRAY},
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
    {'<', "upsstair", CLR_YELLOW | HI_ULINE},
    {'>', "dnsstair", CLR_YELLOW | HI_ULINE},
    {'_', "altar", CLR_GRAY},
    {'_', "laltar", CLR_WHITE},
    {'_', "naltar", CLR_GRAY},
    {'_', "caltar", CLR_BLACK},
    {'_', "ualtar", CLR_RED},
    {'_', "aaltar", CLR_BRIGHT_MAGENTA},
    {'|', "grave", CLR_BLACK},
    {'\\', "throne", HI_GOLD},
    {'#', "sink", CLR_GRAY},
/*40*/ {'{', "fountain", CLR_BRIGHT_BLUE},
    {'#', "vodbridge", CLR_BROWN},
    {'#', "hodbridge", CLR_BROWN},
    {'+', "vcdbridge", CLR_YELLOW},
    {'+', "hcdbridge", CLR_YELLOW}
};

const struct nh_symdef trapsyms[] = {
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


const struct nh_symdef explsyms[] = {
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

const struct nh_symdef expltypes[] = {
    {0, "dark", CLR_BLACK},
    {0, "noxious", CLR_GREEN},
    {0, "muddy", CLR_BROWN},
    {0, "wet", CLR_BLUE},
    {0, "magical", CLR_MAGENTA},
    {0, "fiery", CLR_ORANGE},
    {0, "frosty", CLR_WHITE}
};

const struct nh_symdef zapsyms[NUM_ZAP] = {
    {'|', "zap_v", HI_ZAP},
    {'-', "zap_h", HI_ZAP},
    {'\\', "zap_ld", HI_ZAP},
    {'/', "zap_rd", HI_ZAP}
};

/*
 *  This must be the same order as used for buzz() in zap.c.
 */
const struct nh_symdef zaptypes[NUM_ZAP] = {
    {0, "missile", HI_ZAP},
    {0, "fire", CLR_ORANGE},
    {0, "frost", CLR_WHITE},
    {0, "sleep", HI_ZAP},
    {0, "death", CLR_BLACK},
    {0, "lightning", CLR_WHITE},
    {0, "poison gas", CLR_YELLOW},
    {0, "acid", CLR_GREEN},
    {0, "stun", CLR_BROWN}
};

const struct nh_symdef effectsyms[] = {
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

const struct nh_symdef swallowsyms[] = {
    {'/', "swallow_top_l", CLR_ORANGE}, /* swallow top left */
    {'-', "swallow_top_c", CLR_ORANGE}, /* swallow top center */
    {'\\', "swallow_top_r", CLR_ORANGE},        /* swallow top right */
    {'|', "swallow_mid_l", CLR_ORANGE}, /* swallow middle left */
    {'|', "swallow_mid_r", CLR_ORANGE}, /* swallow middle right */
    {'\\', "swallow_bot_l", CLR_ORANGE},        /* swallow bottom left */
    {'-', "swallow_bot_c", CLR_ORANGE}, /* swallow bottom center */
    {'/', "swallow_bot_r", CLR_ORANGE}, /* swallow bottom right */
};


/* Make unique, non-null names for all objects. */
#define UNIQUE_OBJECT_NAME_LENGTH 80
/* We cache the names to ease allocation issues; there are only a finite number
   of them anyway. */
static char unique_object_names[NUM_OBJECTS][UNIQUE_OBJECT_NAME_LENGTH+1];
static boolean unique_object_names_initialized = FALSE;

static void
initialize_unique_object_names(void)
{
    const char *nameptr;
    char *target;
    int otyp, otyp2;
    int class;
    int object_matches_descr_of[NUM_OBJECTS];

    /* We do two passes over the list: first we name the objects, then we
       disambiguate names that would otherwise be the same. */
    for (otyp = 0; otyp < NUM_OBJECTS; otyp++) {
        class = (int)const_objects[otyp].oc_class;
        
        /* Name objects by their unidentified description, if we can, because
           that's what determines what colour they'll be in ASCII, and what tile
           will be used in tiles mode. */
        nameptr = obj_descr[otyp].oc_descr;
        if (!nameptr) nameptr = obj_descr[otyp].oc_name;
        target = unique_object_names[otyp];

        /* Add a suffix to some object types to disambiguate orange from orange
           potion from orange spellbook, etc.. */
        if (class == AMULET_CLASS && *nameptr != 'A')
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s amulet", nameptr);
        else if (class == WAND_CLASS)
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s wand", nameptr);
        else if (class == RING_CLASS)
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s ring", nameptr);
        else if (class == POTION_CLASS)
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s potion", nameptr);
        else if (class == SPBOOK_CLASS)
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s spellbook",
                     nameptr);
        else if (class == GEM_CLASS)
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s gem", nameptr);
        else
            snprintf(target, UNIQUE_OBJECT_NAME_LENGTH, "%s", nameptr);
        target[UNIQUE_OBJECT_NAME_LENGTH] = '\0'; /* cut off, don't overflow */
    }
    for (otyp = 0; otyp < NUM_OBJECTS; otyp++) {
        object_matches_descr_of[otyp] = 0;
        target = unique_object_names[otyp];
        for (otyp2 = 0; otyp2 < NUM_OBJECTS; otyp2++) {
            if (otyp == otyp2) continue;
            if (strcmp(target, unique_object_names[otyp2]) == 0) {
                object_matches_descr_of[otyp] = (otyp < otyp2 ? otyp : otyp2);
                break;
            }
        }
    }
    for (otyp = 0; otyp < NUM_OBJECTS; otyp++) {
        if (!object_matches_descr_of[otyp]) continue;
        target = unique_object_names[otyp];    
        /* Append 's ' and a number, just as is done to disambiguate tiles.
           This leads to some weird constructions sometimes (probably just
           "Amulet of Yendors 0"), but maximizes the compatibility with
           foreign tilesets. */
        int nth = 0;
        for (otyp2 = 0; otyp2 < otyp; otyp2++) {
            if (object_matches_descr_of[otyp] ==
                object_matches_descr_of[otyp2]) nth++;
        }
        snprintf(target + strlen(target),
                 UNIQUE_OBJECT_NAME_LENGTH - strlen(target),
                 "s %d", nth);
    }
}

char *
make_object_name(int otyp)
{
    if (!unique_object_names_initialized)
        initialize_unique_object_names();
    unique_object_names_initialized = TRUE;
    return unique_object_names[otyp];
}

const char *
make_mon_name(int mnum)
{
    char *name;

    if (mons[mnum].mlet == S_HUMAN && !strncmp(mons[mnum].mname, "were", 4)) {
        name = xmalloc(&xm_drawing,
                       strlen(mons[mnum].mname) + strlen("human ") + 1);
        sprintf(name, "human %s", mons[mnum].mname);
        return name;
    }
    return mons[mnum].mname;
}

void
populate_mon_symdef(int mnum, struct nh_symdef *rv)
{
    rv->ch = def_monsyms[(int)mons[mnum].mlet];
    rv->symname = make_mon_name(mnum);
    rv->color = mons[mnum].mcolor;
}

void
populate_obj_symdef(int otyp, struct nh_symdef *rv)
{
    rv->ch = def_oc_syms[(int)const_objects[otyp].oc_class];
    rv->symname = make_object_name(otyp);
    rv->color = const_objects[otyp].oc_color;
}

struct nh_drawing_info *
nh_get_drawing_info(void)
{
    int i;
    struct nh_symdef *tmp;
    struct nh_drawing_info *di;

    xmalloc_cleanup(&xm_drawing);

    di = xmalloc(&xm_drawing, sizeof (struct nh_drawing_info));

    di->num_bgelements = SIZE(defsyms);
    di->bgelements = defsyms;

    di->num_traps = SIZE(trapsyms);
    di->traps = trapsyms;

    di->num_objects = NUM_OBJECTS;
    tmp = xmalloc(&xm_drawing, sizeof (struct nh_symdef) * di->num_objects);
    for (i = 0; i < di->num_objects; i++)
        populate_obj_symdef(i, tmp + i);
    di->objects = tmp;

    tmp = xmalloc(&xm_drawing, sizeof (struct nh_symdef));
    tmp->ch = DEF_INVISIBLE;
    tmp->color = CLR_BRIGHT_BLUE;
    tmp->symname = invismonexplain;
    di->invis = tmp;

    di->num_monsters = NUMMONS;
    tmp = xmalloc(&xm_drawing, sizeof (struct nh_symdef) * di->num_monsters);
    for (i = 0; i < di->num_monsters; i++)
        populate_mon_symdef(i, tmp + i);
    di->monsters = tmp;

    di->num_warnings = SIZE(warnsyms);
    di->warnings = warnsyms;

    di->num_expltypes = SIZE(expltypes);
    di->expltypes = expltypes;
    di->explsyms = explsyms;

    di->num_zaptypes = SIZE(zaptypes);
    di->zaptypes = zaptypes;
    di->zapsyms = zapsyms;

    di->num_effects = SIZE(effectsyms);
    di->effects = effectsyms;

    di->swallowsyms = swallowsyms;

    di->bg_feature_offset = DUNGEON_FEATURE_OFFSET;

    return di;
}

/*drawing.c*/

