/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
/* #define DEBUG *//* uncomment for debugging */

#include <stdbool.h>
#include <string.h>

/*
 * Some systems may have getchar() return EOF for various reasons, and
 * we should not quit before seeing at least NR_OF_EOFS consecutive EOFs.
 */
#define NR_OF_EOFS      20

#define CMD_TRAVEL (char)0x90

static int doservercancel(const struct nh_cmd_arg *);
static int dotravel(const struct nh_cmd_arg *);
static int doautoexplore(const struct nh_cmd_arg *);
static int dowelcome(const struct nh_cmd_arg *);
static int dointerrupt(const struct nh_cmd_arg *);
static int doattributes(const struct nh_cmd_arg *);
static int doconduct(const struct nh_cmd_arg *);
static int doquit(const struct nh_cmd_arg *);
static int wiz_wish(const struct nh_cmd_arg *);
static int wiz_identify(const struct nh_cmd_arg *);
static int wiz_map(const struct nh_cmd_arg *);
static int wiz_genesis(const struct nh_cmd_arg *);
static int wiz_levelcide(const struct nh_cmd_arg *);
static int wiz_where(const struct nh_cmd_arg *);
static int wiz_detect(const struct nh_cmd_arg *);
static int wiz_desync(const struct nh_cmd_arg *);
static int wiz_panic(const struct nh_cmd_arg *);
static int wiz_impossible(const struct nh_cmd_arg *);
static int wiz_rewind(const struct nh_cmd_arg *);
static int wiz_polyself(const struct nh_cmd_arg *);
static int wiz_teleport(const struct nh_cmd_arg *);
static int wiz_hpset(const struct nh_cmd_arg *);
static int wiz_level_tele(const struct nh_cmd_arg *);
static int wiz_level_change(const struct nh_cmd_arg *);
static int wiz_show_seenv(const struct nh_cmd_arg *);
static int wiz_show_vision(const struct nh_cmd_arg *);
static int wiz_mon_polycontrol(const struct nh_cmd_arg *);
static int wiz_togglegen(const struct nh_cmd_arg *);
static int wiz_show_wmodes(const struct nh_cmd_arg *);
static int wiz_show_stats(const struct nh_cmd_arg *);
static void count_obj(struct obj *, long *, long *, boolean, boolean);
static void obj_chain(struct nh_menulist *, const char *, struct obj *, long *,
                      long *);
static void mon_invent_chain(struct nh_menulist *, const char *, struct monst *,
                             long *, long *);
static void mon_chain(struct nh_menulist *, const char *, struct monst *,
                      long *, long *);
static void contained(struct nh_menulist *, const char *, long *, long *);

#ifndef M
# define M(c)           ((c) - 128)
#endif
#ifndef C
# define C(c)           (0x1f & (c))
#endif

/* Note: command names must consist of lowercase letters only; the network API
   sanitizes them to that subset. Likewise, there's a maximum length of 59
   characters, because longer names are assumed to be malicious and rejected
   (mostly as a precaution; there are no fixed size buffers involved any
   more). */
const struct cmd_desc cmdlist[] = {
    /* "str", "", defkey, altkey, buried, func, flags */
    {"adjust", "adjust inventory letters", M('a'), 0, TRUE, doorganize,
     CMD_ARG_OBJ | CMD_EXT},
    {"annotate", "name the current level", 0, 0, TRUE, donamelevel,
     CMD_ARG_STR | CMD_EXT | CMD_MAINMENU},
    {"apply", "use a tool or container or dip into a potion", 'a', 0, FALSE,
     doapply, CMD_ARG_OBJ | CMD_ARG_POS | CMD_ARG_DIR | CMD_ARG_STR },
    {"attributes", "show status of your character", C('x'), 0, TRUE,
     doattributes, CMD_MAINMENU | CMD_NOTIME},
    {"autoexplore", "automatically explore until something happens", 'v', 0,
     FALSE, doautoexplore, 0},
    {"cast", "use a magical or supernatural ability", 'Z', 0, TRUE, docast,
     CMD_ARG_SPELL | CMD_ARG_DIR | CMD_ARG_POS},
    {"chat", "talk to someone", 'c', M('c'), TRUE, dotalk,
     CMD_ARG_DIR | CMD_EXT},     /* converse? */
    {"close", "close a door", 0, 0, FALSE, doclose, CMD_ARG_DIR},
    {"conduct", "list status of voluntary challenges", 0, 0, TRUE, doconduct,
     CMD_EXT | CMD_NOTIME},
    {"countgold", "show gold, debt, credit, and unpaid items", '$', 0, TRUE,
     doprgold, CMD_NOTIME},
    {"dip", "dip an object into something", M('d'), 0, FALSE, dodip,
     CMD_EXT | CMD_ARG_OBJ},
    {"discoveries", "show your knowledge about items", '\\', 0, TRUE,
     dodiscovered, CMD_NOTIME | CMD_MAINMENU},
    {"drink", "quaff a potion", 'q', 0, FALSE, dodrink,
     CMD_ARG_OBJ},
    {"drop", "drop one item", 'd', 0, FALSE, dodrop,
     CMD_ARG_OBJ},
    {"eat", "eat an item from inventory or the floor", 'e', 0, FALSE, doeat,
     CMD_ARG_OBJ},
    {"elbereth", "write an Elbereth in the dust", C('e'), 0, FALSE, doelbereth,
     0},
    {"enhance", "advance or check your character's skills", M('e'), 0, TRUE,
     enhance_weapon_skill, CMD_EXT | CMD_MAINMENU},
    {"engrave", "write on the floor", 'E', 0, FALSE, doengrave,
     CMD_ARG_OBJ | CMD_ARG_STR},
    {"equip", "change your equipment", 'A', '*', FALSE, doequip, CMD_MAINMENU},
    {"farlook", "say what is on a distant square", ';', 0, TRUE, doquickwhatis,
     CMD_NOTIME | CMD_ARG_POS},
    {"fight", "attack even if no hostile monster is visible", 'F', 0, FALSE,
     dofight, CMD_ARG_DIR},
    {"fire", "throw your quivered item", 'f', 0, FALSE, dofire,
     CMD_ARG_DIR | CMD_ARG_LIMIT},
    {"force", "force a lock", M('f'), 0, FALSE, doforce,
     CMD_EXT},
    {"grope", "feel around for things at your feet", C('g'), M('g'),
     FALSE, dofeel, 0},
    {"history", "show a list of your historic deeds", 0, 0, TRUE, dohistory,
     CMD_EXT | CMD_NOTIME},
    {"idtrap", "identify a trap", '^', 0, TRUE, doidtrap,
     CMD_ARG_DIR | CMD_NOTIME},
    {"inventory", "show your inventory", 'i', 0, TRUE, ddoinv,
     CMD_NOTIME},
    {"invoke", "invoke an object's powers", 'V', M('i'), TRUE, doinvoke,
     CMD_EXT | CMD_ARG_OBJ},
    {"jump", "jump to a location", M('j'), 'j', FALSE, dojump,
     CMD_ARG_POS | CMD_EXT},
    {"kick", "kick an adjacent object or monster", C('d'), 'k', FALSE, dokick,
     CMD_ARG_DIR},
    {"license", "show the NetHack license", 0, 0, TRUE, dolicense,
     CMD_HELP | CMD_NOTIME},
    {"lookhere", "describe the current square", ':', 0, TRUE, dolook,
     CMD_NOTIME},
    {"loot", "loot a bag or box on the floor", M('l'), 'l', FALSE, doloot,
     CMD_EXT},
    {"menuinv", "show a partial inventory", 'I', 0, TRUE, dotypeinv,
     CMD_NOTIME},
    {"monster", "use a monster's special ability", 'M', M('m'), TRUE,
     domonability, CMD_ARG_DIR | CMD_EXT},
    {"multidrop", "drop multiple items", 'D', 0, FALSE, doddrop, 0},
    {"name", "name a monster, item or type of object", M('n'), 'C', TRUE,
     do_naming, CMD_EXT},
    {"namemon", "christen a monster", 0, 0, TRUE, do_mname,
     CMD_ARG_POS | CMD_ARG_STR},
    {"nameitem", "name an item", 0, 0, TRUE, do_oname,
     CMD_ARG_OBJ | CMD_ARG_STR},
    {"nametype", "name a type of objects", 0, 0, TRUE, do_tname,
     CMD_ARG_OBJ | CMD_ARG_STR},
    {"offer", "offer a sacrifice to the gods", 0, 0, FALSE,
     dosacrifice, CMD_EXT | CMD_ARG_OBJ},
    {"open", "open, close or unlock a door", 'o', 0, FALSE, doopen,
     CMD_ARG_DIR},
    {"overview", "show an overview of the dungeon", C('o'), 0, TRUE, dooverview,
     CMD_EXT | CMD_NOTIME | CMD_MAINMENU},
    {"pay", "pay a shopkeeper", 'p', 0, FALSE, dopay,
     CMD_ARG_OBJ},
    {"pickup", "take items from the floor", ',', 0, FALSE, dopickup,
     CMD_ARG_LIMIT},
    {"pray", "pray to the gods for help", M('p'), 0, TRUE, dopray, CMD_EXT},
    {"quit", "don't use this, use 'save' instead", M('q'), 0, TRUE, doquit,
     CMD_EXT | CMD_NOTIME},
    {"quiver", "ready an item for firing", 'Q', 0, FALSE, dowieldquiver,
     CMD_ARG_OBJ},
    {"read", "read a scroll or spellbook", 'r', 0, FALSE, doread, CMD_ARG_OBJ},
    {"redraw", "redraw the screen", C('r'), 0, TRUE, doredrawcmd,
     CMD_NOTIME},
    {"repeat", "repeat the last command or continue an interrupted command",
     C('a'), 0, TRUE, NULL},
    {"ride", "ride (or stop riding) a monster", M('r'), 0, FALSE, doride,
     CMD_ARG_DIR | CMD_EXT},
    {"rub", "rub a lamp or a stone", 0, 0, FALSE, dorub,
     CMD_EXT | CMD_ARG_OBJ},
    {"sacrifice", "offer a sacrifice to the gods", M('o'), 0, FALSE,
     dosacrifice, CMD_EXT | CMD_ARG_OBJ},
    {"search", "search for hidden doors and traps", 's', 0, TRUE, dosearch,
     CMD_ARG_LIMIT},
    {"showamulets", "list your worn amulet", '"', 0, TRUE,
     dopramulet, CMD_NOTIME},
    {"showarmor", "list your worn armor", '[', 0, TRUE, doprarm,
     CMD_NOTIME},
    {"showrings", "list your worn rings", '=', 0, TRUE, doprring,
     CMD_NOTIME},
    {"showtools", "list your worn eyewear", '(', 0, TRUE, doprtool,
     CMD_NOTIME},
    {"showweapon", "list your wielded weapons", ')', 0, TRUE, doprwep,
     CMD_NOTIME},
    {"showworn", "list your equipment", 0, 0, TRUE, doprinuse,
     CMD_NOTIME},
    {"sit", "sit down", M('s'), 0, FALSE, dosit, CMD_EXT},
    {"spellbook", "display and organise your magical abilities", '+', 0, TRUE,
     dovspell, CMD_MAINMENU},
    {"swapweapon", "exchange wielded and alternate weapon", 'x', 0, FALSE,
     doswapweapon, 0},
    {"takeoff", "take off an item you are wearing", 'T', 'R', FALSE, dounequip,
     CMD_ARG_OBJ},
    {"teleport", "use intrinsic or magical teleportation ability", C('t'), 0,
     TRUE, dotele, 0},
    {"throw", "throw an item", 't', 0, FALSE, dothrow,
     CMD_ARG_OBJ | CMD_ARG_DIR | CMD_ARG_LIMIT},
    {"travel", "walk until a given square is reached", '_', 0, TRUE, dotravel,
     CMD_ARG_POS},
    {"turn", "turn undead", M('t'), 0, TRUE, doturn, CMD_EXT},
    {"twoweapon", "toggle two-weapon combat", M('2'), 'X', FALSE, dotwoweapon,
     CMD_EXT},
    {"untrap", "untrap something", M('u'), 'u', FALSE, dountrap,
     CMD_EXT},
    {"version", "displays the version number", 0, 0, TRUE, doversion,
     CMD_HELP | CMD_NOTIME | CMD_EXT},
    {"verhistory", "display the version history", 0, 0, TRUE, doverhistory,
     CMD_HELP | CMD_NOTIME | CMD_EXT},
    {"wait", "do nothing for one turn", '.', 0, TRUE, donull, CMD_ARG_LIMIT},
    {"wear", "wear clothing, armor, or accessories", 'W', 'P', FALSE, dowear,
     CMD_ARG_OBJ},
    {"wield", "hold an item in your hands", 'w', 0, FALSE, dowield,
     CMD_ARG_OBJ},
    {"wipe", "wipe off your face", M('w'), 0, FALSE, dowipe,
     CMD_EXT},
    {"whatis", "describe what a symbol means", '/', 0, TRUE, dowhatis,
     CMD_HELP | CMD_NOTIME | CMD_ARG_POS | CMD_ARG_STR | CMD_ARG_OBJ},
    {"zap", "zap a wand to use its magic", 'z', 0, FALSE, dozap,
     CMD_ARG_OBJ | CMD_ARG_DIR},

    {"move", "move one step", 0, 0, FALSE, domovecmd,
     CMD_ARG_DIR | CMD_MOVE | CMD_ARG_LIMIT},
    {"moveonly", "move, but don't fight or pick anything up", 'm', 0,
     FALSE, domovecmd_nopickup, CMD_ARG_DIR | CMD_MOVE | CMD_ARG_LIMIT},
    {"run", "run, interacting with things found on the way", 0, 0, FALSE, dorun,
     CMD_ARG_DIR | CMD_MOVE | CMD_ARG_LIMIT},
    {"go", "run until something happens, cautiously", 'g', 'G', FALSE, dogo,
     CMD_ARG_DIR | CMD_MOVE | CMD_ARG_LIMIT},

    {"welcome", "(internal) display the 'welcome back!' message", 0, 0, TRUE,
     dowelcome, CMD_INTERNAL},
    {"interrupt", "(internal) cancels a multi-turn command", 0, 0, TRUE,
     dointerrupt, CMD_INTERNAL},
    {"servercancel", "(internal) reread the replay, someone moved", 0, 0, TRUE,
     doservercancel, CMD_INTERNAL | CMD_NOTIME},

    {"desync", "(DEBUG) corrupt the save file", 0, 0, TRUE, wiz_desync,
     CMD_DEBUG | CMD_EXT},
    {"detect", "(DEBUG) detect monsters", 0, 0, TRUE, wiz_detect,
     CMD_DEBUG | CMD_EXT},
    {"genesis", "(DEBUG) create a monster", 0, 0, TRUE, wiz_genesis,
     CMD_DEBUG | CMD_ARG_LIMIT | CMD_ARG_STR | CMD_EXT},
    {"hpset", "(DEBUG) refill or change your current HP", 0, 0, TRUE,
     wiz_hpset, CMD_DEBUG | CMD_EXT | CMD_ARG_LIMIT},
    {"identify", "(DEBUG) identify all items in the inventory", C('i'), 0, TRUE,
     wiz_identify, CMD_DEBUG | CMD_EXT},
    {"impossible", "(DEBUG) test nonfatal error handling", 0, 0, TRUE,
     wiz_impossible, CMD_DEBUG | CMD_EXT},
    {"levelchange", "(DEBUG) change experience level", 0, 0, TRUE,
     wiz_level_change, CMD_DEBUG | CMD_EXT},
    {"levelcide", "(DEBUG) kill all other monsters on the level", 0, 0, TRUE,
     wiz_levelcide, CMD_DEBUG | CMD_EXT},
    {"lightsources", "(DEBUG) show mobile light sources", 0, 0, TRUE,
     wiz_light_sources, CMD_DEBUG | CMD_EXT | CMD_NOTIME},
    {"levelteleport", "(DEBUG) telport to a different level", C('v'), 0, TRUE,
     wiz_level_tele, CMD_DEBUG},
    {"monpolycontrol", "(DEBUG) control monster polymorphs", 0, 0, TRUE,
     wiz_mon_polycontrol, CMD_DEBUG | CMD_EXT},
    {"panic", "(DEBUG) test fatal error handling", 0, 0, TRUE,
     wiz_panic, CMD_DEBUG | CMD_EXT},
    {"polyself", "(DEBUG) polymorph self", 0, 0, TRUE, wiz_polyself,
     CMD_DEBUG | CMD_EXT},
    {"printdungeon", "(DEBUG) print dungeon structure", 0, 0, TRUE, wiz_where,
     CMD_DEBUG | CMD_NOTIME | CMD_EXT},
    {"rewind", "(DEBUG) permanently undo gamestate changes", 0, 0, TRUE,
     wiz_rewind, CMD_DEBUG | CMD_NOTIME | CMD_EXT},
    {"seenv", "(DEBUG) show seen vectors", 0, 0, TRUE, wiz_show_seenv,
     CMD_DEBUG | CMD_EXT | CMD_NOTIME},
    {"showmap", "(DEBUG) reveal the entire map", 0, 0, TRUE, wiz_map,
     CMD_DEBUG | CMD_EXT},
    {"stats", "(DEBUG) show memory statistics", 0, 0, TRUE, wiz_show_stats,
     CMD_DEBUG | CMD_EXT | CMD_NOTIME},
    {"timeout", "(DEBUG) look at timeout queue", 0, 0, TRUE, wiz_timeout_queue,
     CMD_DEBUG | CMD_EXT | CMD_NOTIME},
    {"togglegen", "(DEBUG) toggle monster generation", 0, 0, TRUE,
     wiz_togglegen, CMD_DEBUG | CMD_EXT},
    {"vision", "(DEBUG) show vision array", 0, 0, TRUE, wiz_show_vision,
     CMD_DEBUG | CMD_EXT | CMD_NOTIME},
    {"wish", "(DEBUG) wish for an item", C('w'), 0, TRUE, wiz_wish,
     CMD_DEBUG},
    {"wizport", "(DEBUG) teleport without fail", C('f'), 0, TRUE,
     wiz_teleport, CMD_DEBUG},
    {"wmode", "(DEBUG) show wall modes", 0, 0, TRUE, wiz_show_wmodes,
     CMD_DEBUG | CMD_EXT | CMD_NOTIME},

    {NULL, NULL, 0, 0, 0, 0, 0}
};

/* Several occupations cannot be meaningfully continued if you move (anything
   acting on a map square, for instance, because your orientation to it
   changes). Others can't be continued if the item is no longer in inventory
   (equipping), or if the item is no longer on the ground at your location
   (unlocking, forcing). */
void
reset_occupations(boolean u_changed_location)
{
    int i;

    /* Occupations that always reset if you move. */
    if (u_changed_location) {
        /* Setting traps. */
        u.uoccupation_progress[tos_trap] = 0;
        u.utracked[tos_trap] = 0;
        /* Digging. */
        u.uoccupation_progress[tos_dig] = 0; /* u.utracked[tos_dig] unused */
        /* Picking locks. */
        u.uoccupation_progress[tos_lock] = 0;
        u.utracked[tos_lock] = 0;
    }

    /* Occupations that reset if you don't have the item in inventory. */
    for (i = tos_first_equip; i <= tos_last_equip; i++) {
        /* Equipping. */
        if (u.utracked[i] && u.utracked[i] != &zeroobj &&
            u.utracked[i]->where != OBJ_INVENT) {
            u.utracked[i] = 0;
            u.uoccupation_progress[i] = 0;
        }
    }

    /* Occupations that require the item to be on the ground. */
    if (u.utracked[tos_lock] != &zeroobj) {
        if (!obj_with_u(u.utracked[tos_lock]) ||
            u.utracked[tos_lock]->where == OBJ_INVENT) {
            u.utracked[tos_lock] = 0;
            u.uoccupation_progress[tos_lock] = 0;
        }
    }

    /* Eating, reading, and opening tins can be resumed even if you lose the
       item in the meantime. */
}

static int
doservercancel(const struct nh_cmd_arg *arg)
{
    (void) arg;
    return 0;
}

/* ^W command - wish for something */
static int
wiz_wish(const struct nh_cmd_arg *arg)
{       /* Unlimited wishes for debug mode by Paul Polderman */
    boolean save_verbose = flags.verbose;

    (void) arg;
    /* TODO: We could implement CMD_ARG_STR for this to allow repeatedly
       wishing for the same item. */

    flags.verbose = FALSE;
    makewish();
    flags.verbose = save_verbose;
    encumber_msg();

    return 0;
}

/* ^I command - identify hero's inventory */
static int
wiz_identify(const struct nh_cmd_arg *arg)
{
    (void) arg;

    identify_pack(0);

    return 0;
}

/* ^F command - reveal the level map and any traps on it */
static int
wiz_map(const struct nh_cmd_arg *arg)
{
    struct trap *t;
    long save_Hconf = HConfusion, save_Hhallu = HHallucination;

    (void) arg;

    HConfusion = HHallucination = 0L;
    for (t = level->lev_traps; t != 0; t = t->ntrap) {
        t->tseen = 1;
        map_trap(t, TRUE, FALSE);
    }
    do_mapping();
    HConfusion = save_Hconf;
    HHallucination = save_Hhallu;

    return 0;
}

/* ^G command - generate monster(s); a count prefix will be honored */
static int
wiz_genesis(const struct nh_cmd_arg *arg)
{
    create_particular(arg);

    return 0;
}

/* #levelcide - kill all other monsters on the level */
static int
wiz_levelcide(const struct nh_cmd_arg *arg)
{
    (void) arg;

    do_level_genocide();

    return 0;
}

/* ^O command - display dungeon layout */
static int
wiz_where(const struct nh_cmd_arg *arg)
{
    (void) arg;

    print_dungeon(FALSE, NULL, NULL);

    return 0;
}

/* ^E command - detect unseen (secret doors, traps, hidden monsters) */
static int
wiz_detect(const struct nh_cmd_arg *arg)
{
    (void) arg;

    findit();

    return 0;
}

static int
wiz_desync(const struct nh_cmd_arg *arg)
{
    (void) arg;

    flags.desync = TRUE;

    return 0;
}

/* ^F command - teleport without fail */
static int
wiz_teleport(const struct nh_cmd_arg *arg)
{
    (void) arg;

    tele_impl(TRUE, FALSE);

    return 0;
}

/* ^V command - level teleport */
static int
wiz_level_tele(const struct nh_cmd_arg *arg)
{
    (void) arg;

    level_tele_impl(TRUE);

    return 0;
}

/* #monpolycontrol command - choose new form for shapechangers, polymorphees */
static int
wiz_mon_polycontrol(const struct nh_cmd_arg *arg)
{
    (void) arg;

    flags.mon_polycontrol = !flags.mon_polycontrol;

    pline("Monster polymorph control is %s.",
          flags.mon_polycontrol ? "on" : "off");

    return 0;
}

/* #togglegen command - toggle monster generation on/off */
static int
wiz_togglegen(const struct nh_cmd_arg *arg)
{
    (void) arg;
    flags.mon_generation = !flags.mon_generation;

    pline("Monster generation is %s.",
          flags.mon_generation ? "on" : "off");

    return 0;
}

/* #hpset command - refill current HP, or change it with a prefix */
static int
wiz_hpset(const struct nh_cmd_arg *arg)
{
    int *hp = Upolyd ? &u.mh : &u.uhp;
    int *hpmax = Upolyd ? &u.mhmax : &u.uhpmax;
    if (arg->argtype & CMD_ARG_LIMIT)
        *hp = arg->limit;
    else
        *hp = *hpmax;

    if (*hp < 1)
        *hp = 1;
    if (*hp >= *hpmax) {
        *hp = *hpmax;
        pline("You feel entirely healed.");
    } else {
        pline("You feel very precisely wounded.");
    }
    return 0;
}

/* #levelchange command - adjust hero's experience level */
static int
wiz_level_change(const struct nh_cmd_arg *arg)
{
    const char *buf;
    int newlevel = u.ulevel;
    int ret;

    buf = getarglin(arg, "To what experience level do you want to be set?");
    buf = msgmungspaces(buf);
    if (buf[0] == '\033' || buf[0] == '\0')
        ret = 0;
    else
        ret = sscanf(buf, "%d", &newlevel);

    if (ret != 1) {
        pline("Never mind.");
        return 0;
    }
    if (newlevel == u.ulevel) {
        pline("You are already that experienced.");
    } else if (newlevel < u.ulevel) {
        if (u.ulevel == 1) {
            pline("You are already as inexperienced as you can get.");
            return 0;
        }
        if (newlevel < 1)
            newlevel = 1;
        while (u.ulevel > newlevel)
            losexp(NULL, TRUE);
    } else {
        if (u.ulevel >= MAXULEV) {
            pline("You are already as experienced as you can get.");
            return 0;
        }
        if (newlevel > MAXULEV)
            newlevel = MAXULEV;
        while (u.ulevel < newlevel)
            pluslvl(FALSE);
    }
    u.ulevelmax = u.ulevel;
    return 0;
}

/* #panic command - test program's fatal error handling */
static int
wiz_panic(const struct nh_cmd_arg *arg)
{
    (void) arg;

    panic("Testing fatal error handling.");
    /* no return; panic() never returns */
}

/* #impossible command - test program's nonfatal error handling */
static int
wiz_impossible(const struct nh_cmd_arg *arg)
{
    (void) arg;

    impossible("Testing impossible().");
    return 0;
}

/* #rewind command - undo changes to a save */
static int
wiz_rewind(const struct nh_cmd_arg *arg)
{
    (void) arg;

    /* Grab write access to this game, even if we don't have the perms,
       even if we're not at the end. */
    program_state.followmode = FM_PLAY;

    log_recover_noreturn(program_state.end_of_gamestate_location,
                         "Manual rewind requested", __FILE__, __LINE__);
}

/* #polyself command - change hero's form */
static int
wiz_polyself(const struct nh_cmd_arg *arg)
{
    (void) arg;

    polyself(TRUE);
    return 0;
}

/* #seenv command */
static int
wiz_show_seenv(const struct nh_cmd_arg *arg)
{
    struct nh_menulist menu;
    int x, y, v, startx, stopx, curx;
    char row[COLNO + 1];

    (void) arg;

    init_menulist(&menu);
    /*
     * Each seenv description takes up 2 characters, so center
     * the seenv display around the hero.
     */
    startx = max(0, u.ux - (COLNO / 4));
    stopx = min(startx + (COLNO / 2), COLNO);
    /* can't have a line exactly 80 chars long */
    if (stopx - startx == COLNO / 2)
        startx++;

    for (y = 0; y < ROWNO; y++) {
        for (x = startx, curx = 0; x < stopx; x++, curx += 2) {
            if (x == u.ux && y == u.uy) {
                row[curx] = row[curx + 1] = '@';
            } else {
                v = level->locations[x][y].seenv & 0xff;
                if (v == 0)
                    row[curx] = row[curx + 1] = ' ';
                else
                    sprintf(&row[curx], "%02x", v);
            }
        }
        /* remove trailing spaces */
        for (x = curx - 1; x >= 0; x--)
            if (row[x] != ' ')
                break;
        row[x + 1] = '\0';

        add_menutext(&menu, row);
    }
    display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    return 0;
}

static const char hex[] = {
    ' ', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

/* #vision command */
static int
wiz_show_vision(const struct nh_cmd_arg *arg)
{
    struct nh_menulist menu;
    int x, y, v;
    char row[COLNO + 1];

    (void) arg;

    init_menulist(&menu);
    snprintf(row, SIZE(row), "Flags: 0x%x could see, 0x%x in sight, 0x%x temp lit, 0x8 tile lit",
            COULD_SEE, IN_SIGHT, TEMP_LIT);
    add_menutext(&menu, row);
    add_menutext(&menu, "");
    for (y = 0; y < ROWNO; y++) {
        for (x = 0; x < COLNO; x++) {
            if (x == u.ux && y == u.uy)
                row[x] = '@';
            else {
                v = viz_array[y][x];    /* data access should be hidden */
                v += 8 * level->locations[x][y].lit;
                row[x] = hex[v];
            }
        }
        /* remove trailing spaces */
        for (x = COLNO - 1; x >= 0; x--)
            if (row[x] != ' ')
                break;
        row[x + 1] = '\0';

        add_menutext(&menu, &row[1]);
    }
    display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    return 0;
}

/* #wmode command */
static int
wiz_show_wmodes(const struct nh_cmd_arg *arg)
{
    struct nh_menulist menu;
    int x, y;
    char row[COLNO + 1];
    struct rm *loc;

    (void) arg;

    init_menulist(&menu);
    for (y = 0; y < ROWNO; y++) {
        for (x = 0; x < COLNO; x++) {
            loc = &level->locations[x][y];
            if (x == u.ux && y == u.uy)
                row[x] = '@';
            else if (IS_WALL(loc->typ) || loc->typ == SDOOR)
                row[x] = '0' + (loc->wall_info & WM_MASK);
            else if (loc->typ == CORR)
                row[x] = '#';
            else if (IS_ROOM(loc->typ) || IS_DOOR(loc->typ))
                row[x] = '.';
            else
                row[x] = 'x';
        }
        row[COLNO] = '\0';
        add_menutext(&menu, row);
    }
    display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    return 0;
}

/* Shows legacy, "welcome [back] to NetHack" messages when a save file is
   restored; the client sends this command to record the times that the game was
   restored via voluntary action rather than via reconnecting a timed-out
   connection.

   It is important that this command does not affect anything other than
   messages (it may affect parts of the gamestate that affect only messages,
   such as u.uwelcomed). Otherwise, it would be possible to use a hacked client
   to send this command at inappropriate times in order to modify the gamestate
   in ways that would not be available to a normal player. Hacking in order to
   get welcome messages at inappropriate times is pointless :-) */
static int
dowelcome(const struct nh_cmd_arg *arg)
{
    const char *buf;
    boolean currentgend = Upolyd ? u.mfemale : u.ufemale;
    boolean new_game = !u.uwelcomed;

    (void) arg;

    u.uwelcomed = 1;

    if (new_game && flags.legacy) {
        flush_screen();
        com_pager(1);
    }

    /*
     * The "welcome back" message always describes your innate form
     * even when polymorphed or wearing a helm of opposite alignment.
     * Alignment is shown unconditionally for new games; for restores
     * it's only shown if it has changed from its original value.
     * Sex is shown for new games except when it is redundant; for
     * restores it's only shown if different from its original value.
     */
    buf = "";
    if (new_game || u.ualignbase[A_ORIGINAL] != u.ualignbase[A_CURRENT])
        buf = msgprintf("%s %s", buf, align_str(u.ualignbase[A_ORIGINAL]));
    if (!urole.name.f &&
        (new_game ? (urole.allow & ROLE_GENDMASK) ==
         (ROLE_MALE | ROLE_FEMALE) : currentgend != u.initgend))
        buf = msgprintf("%s %s", buf, genders[currentgend].adj);

    pline(new_game ? "%s %s, welcome to NetHack!  You are a%s %s %s." :
          "%s %s, the%s %s %s, welcome back to NetHack!", Hello(NULL),
          u.uplname, buf, urace.adj,
          (currentgend && urole.name.f) ? urole.name.f : urole.name.m);

    /* Realtime messages are no longer printed in dowelcome(); rather, there's a
       special case in pre_move_tasks to print them at the next realtime status
       update, which happens before the player gets any more chances to provide
       input (and thus /looks/ like it was part of the welcome). This allows for
       the situation where a game was saved on a new moon and reloaded on a full
       moon, or something similar; the player will get only one message (the one
       for the time when it's loaded), not two.*/

    if (discover)
        pline("You are in non-scoring discovery mode.");

    if (*level->levname)
        pline("You named this level: %s.", level->levname);

    return 0;
}

/* Normally, we don't want commands like "inventory" to change the save data.
   However, there's one respect in which they must: if you use them to break up
   a multi-turn action, you don't want the action immediately restarting after
   viewing your inventory, yet the action would continue if you didn't (and the
   client didn't intervene). Thus, we need to insert a command whose purpose is
   to capture this side effect of a notime action. It can also be sent
   explicitly by the client in order to print a "You stop running." message, or
   the like. */
static int
dointerrupt(const struct nh_cmd_arg *arg)
{
    (void) arg;

    action_interrupted();
    return 0;
}

/*
 * Courtesy function for non-debug, non-explorer mode players
 * to help refresh them about who/what they are.
 */
static int
doattributes(const struct nh_cmd_arg *arg)
{
    (void) arg;

    int genidx, n, wcu;
    const int *selected;
    long wc;
    const char *buf;
    static const char fmtstr[] = "%-10s: %-12s (originally %s)";
    static const char fmtstr_noorig[] = "%-10s: %s";
    static const char deity_fmtstr[] = "%-17s%s";
    struct nh_menulist menu;

    init_menulist(&menu);

    add_menuheading(&menu, "Stats");

    /* Starting and current name, race, role, gender, alignment, abilities */
    buf = msgprintf(fmtstr_noorig, "name", u.uplname);
    add_menutext(&menu, buf);
    buf = msgprintf(fmtstr, "race", Upolyd ? youmonst.data->mname : urace.noun,
                    urace.noun);
    add_menutext(&menu, buf);
    buf = msgprintf(fmtstr_noorig, "role",
                    ((Upolyd ? u.mfemale : u.ufemale) &&
                     urole.name.f) ? urole.name.f : urole.name.m);
    add_menutext(&menu, buf);
    genidx = is_neuter(youmonst.data) ? 2 : u.ufemale;
    buf = msgprintf(fmtstr, "gender", genders[genidx].adj,
                    genders[u.initgend].adj);
    add_menutext(&menu, buf);
    buf = msgprintf(fmtstr, "alignment", align_str(u.ualign.type),
                    align_str(u.ualignbase[A_ORIGINAL]));
    add_menutext(&menu, buf);
    if (ACURR(A_STR) > 18) {
        if (ACURR(A_STR) > STR18(100))
            buf = msgprintf("abilities : St:%2d ", ACURR(A_STR) - 100);
        else if (ACURR(A_STR) < STR18(100))
            buf = msgprintf("abilities : St:18/%02d ", ACURR(A_STR) - 18);
        else
            buf = msgprintf("abilities : St:18/** ");
    } else
        buf = msgprintf("abilities : St:%-1d ", ACURR(A_STR));

    buf = msgprintf("%s Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",
                    buf, ACURR(A_DEX), ACURR(A_CON),
                    ACURR(A_INT), ACURR(A_WIS), ACURR(A_CHA));
    add_menutext(&menu, buf);
    if (u.ulevel < 30)
        buf = msgprintf("%-10s: %d (exp: %d, %ld needed)", "level", u.ulevel,
                        u.uexp, newuexp(u.ulevel));
    else
        buf = msgprintf("%-10s: %d (exp: %d)", "level", u.ulevel, u.uexp);
    add_menutext(&menu, buf);

    wc = weight_cap();
    buf = msgprintf("%-10s: %ld (", "burden", wc + inv_weight());

    switch (calc_capacity(wc / 4)) {
    case UNENCUMBERED:
    case SLT_ENCUMBER:
        buf = msgcat(buf, "burdened at ");
        wcu = 2;
        break;
    case MOD_ENCUMBER:
        buf = msgcat(buf, "stressed at ");
        wcu = 3;
        break;
    case HVY_ENCUMBER:
        buf = msgcat(buf, "strained at ");
        wcu = 4;
        break;
    case EXT_ENCUMBER:
        buf = msgcat(buf, "overtaxed at ");
        wcu = 5;
        break;
    default:
        buf = msgcat(buf, "overloaded at ");
        wcu = 6;
        break;
    }
    buf = msgprintf("%s %ld)", buf, wc * wcu / 2 + 1);
    add_menutext(&menu, buf);

    /* Deity list */
    add_menutext(&menu, "");
    add_menuheading(&menu, "Deities");
    buf = msgprintf(deity_fmtstr, align_gname(A_CHAOTIC),
                    (u.ualignbase[A_ORIGINAL] == u.ualign.type &&
                     u.ualign.type == A_CHAOTIC) ? " (s,c)" :
                    (u.ualignbase[A_ORIGINAL] == A_CHAOTIC) ? " (s)" :
                    (u.ualign.type == A_CHAOTIC) ? " (c)" : "");
    buf = msgprintf(fmtstr_noorig, "Chaotic", buf);
    add_menutext(&menu, buf);

    buf = msgprintf(deity_fmtstr, align_gname(A_NEUTRAL),
                    (u.ualignbase[A_ORIGINAL] == u.ualign.type &&
                     u.ualign.type == A_NEUTRAL) ? " (s,c)" :
                    (u.ualignbase[A_ORIGINAL] == A_NEUTRAL) ? " (s)" :
                    (u.ualign.type == A_NEUTRAL) ? " (c)" : "");
    buf = msgprintf(fmtstr_noorig, "Neutral", buf);
    add_menutext(&menu, buf);

    buf = msgprintf(deity_fmtstr, align_gname(A_LAWFUL),
                    (u.ualignbase[A_ORIGINAL] == u.ualign.type &&
                     u.ualign.type == A_LAWFUL) ? " (s,c)" :
                    (u.ualignbase[A_ORIGINAL] == A_LAWFUL) ? " (s)" :
                    (u.ualign.type == A_LAWFUL) ? " (c)" : "");
    buf = msgprintf(fmtstr_noorig, "Lawful", buf);
    add_menutext(&menu, buf);

    add_menutext(&menu, "");
    add_menuheading(&menu, "Other Information");
    add_menuitem(&menu, 'i', "Inventory", 'i', FALSE);
    add_menuitem(&menu, 'a', "Intrinsic abilities", 'a', FALSE);
    if (num_vanquished() > 0)
        add_menuitem(&menu, 'v', "Vanquished creatures", 'v', FALSE);
    if (num_genocides() > 0 || num_extinctions() > 0)
        add_menuitem(&menu, 'g', "Genocided/extinct creatures", 'g', FALSE);
    add_menuitem(&menu, 'h', "Your character's history", 'h', FALSE);
    add_menuitem(&menu, 'c', "Conducts followed", 'c', FALSE);
    add_menuitem(&menu, 's', "Score breakdown", 's', FALSE);
    if (wizard || discover)
        add_menuitem(&menu, 'w', "Debug/explore mode spoilers", 'w', FALSE);

    n = display_menu(&menu, "Your Statistics", PICK_ONE,
                     PLHINT_ANYWHERE, &selected);

    if (n == 1) {
        switch (*selected) {
        case 'i':
            n = ddoinv(&(struct nh_cmd_arg){.argtype = 0});
            break;
        case 'a':
            unspoilered_intrinsics();
            break;
        case 'v':
            list_vanquished('y', FALSE);
            break;
        case 'g':
            list_genocided('y', FALSE);
            break;
        case 'h':
            n = dohistory(&(struct nh_cmd_arg){.argtype = 0});
            break;
        case 'c':
            n = doconduct(&(struct nh_cmd_arg){.argtype = 0});
            break;
        case 's':
            calc_score(-1, TRUE, money_cnt(invent) + hidden_gold());
            break;
        case 'w':
            if (wizard || discover)
                enlightenment(0);
            break;
        }
    }

    return 0;
}

/* KMH, #conduct
 * (shares enlightenment's tense handling)
 */
static int
doconduct(const struct nh_cmd_arg *arg)
{
    (void) arg;
    show_conduct(0);
    return 0;
}

struct nh_cmd_desc *
nh_get_commands(int *count)
{
    int i, j, cmdcount = 0;
    struct nh_cmd_desc *ui_cmd;

    xmalloc_cleanup(&api_blocklist);

    for (i = 0; cmdlist[i].name; i++)
        if (!(cmdlist[i].flags & CMD_INTERNAL))
            cmdcount++;

    ui_cmd = xmalloc(&api_blocklist, sizeof (struct nh_cmd_desc) * cmdcount);
    if (!ui_cmd)
        return NULL;
    memset(ui_cmd, 0, sizeof (struct nh_cmd_desc) * cmdcount);

    j = 0;
    for (i = 0; cmdlist[i].name; i++)
        if (!(cmdlist[i].flags & CMD_INTERNAL)) {
            strncpy(ui_cmd[j].name, cmdlist[i].name, sizeof (ui_cmd[j].name));
            strncpy(ui_cmd[j].desc, cmdlist[i].desc, sizeof (ui_cmd[j].desc));
            ui_cmd[j].defkey = cmdlist[i].defkey;
            ui_cmd[j].altkey = cmdlist[i].altkey;
            ui_cmd[j].flags = cmdlist[i].flags;
            j++;
        }

    *count = cmdcount;
    return ui_cmd;
}


/* for better readability below */
static void
set_obj_cmd(char caccel, struct nh_cmd_desc *obj_cmd, struct obj *obj, size_t i,
            const char *cname, const char *cdesc, bool singular)
{
    int _o_c_idx = get_command_idx(cname);
    char dbuf[80], nbuf[80];

    obj_cmd[i].defkey = caccel;
    strncpy(obj_cmd[i].name, cname, sizeof (obj_cmd[i].name));
    snprintf(nbuf, 80, "%s %s",
             obj->quan > 1 ? (singular ? "one of these" : "these") : "this",
             cxname(obj));
    snprintf(dbuf, 80, cdesc, nbuf);
    strncpy(obj_cmd[i].desc, dbuf, sizeof (obj_cmd[i].desc));
    obj_cmd[i].flags = cmdlist[_o_c_idx].flags;
}

#define SET_OBJ_CMD(caccel, cname, cdesc, singular)              \
    set_obj_cmd(caccel, obj_cmd, obj, i++, cname, cdesc, singular)

#define SET_OBJ_CMD2(caccel, cname, cdesc) \
do {\
    int _o_c_idx = get_command_idx(cname);\
    obj_cmd[i].defkey = caccel;\
    strncpy(obj_cmd[i].name, cname, sizeof(obj_cmd[i].name));\
    strncpy(obj_cmd[i].desc, cdesc, sizeof(obj_cmd[i].desc));\
    obj_cmd[i].flags = cmdlist[_o_c_idx].flags;\
    i++;\
} while (0)


struct nh_cmd_desc *
nh_get_object_commands(int *count, char invlet)
{
    int i, cmdcount = 0;
    struct nh_cmd_desc *obj_cmd;
    struct obj *obj;
    struct monst *shkp;

    xmalloc_cleanup(&api_blocklist);

    /* returning a list of commands when viewing doesn't hurt anything, but
       since they won't work there is no point. */
    if (program_state.followmode != FM_PLAY)
        return NULL;

    for (obj = invent; obj; obj = obj->nobj)
        if (obj->invlet == invlet)
            break;

    if (!obj)
        return NULL;

    for (i = 0; cmdlist[i].name; i++)
        if (cmdlist[i].flags & CMD_ARG_OBJ)
            cmdcount++;

    /* alloc space for all potential commands, even if fewer will apply to the
       given object. This greatly simplifies the counting above. */
    obj_cmd = xmalloc(&api_blocklist, sizeof (struct nh_cmd_desc) * cmdcount);
    if (!obj_cmd)
        return NULL;
    memset(obj_cmd, 0, sizeof (struct nh_cmd_desc) * cmdcount);

    i = 0;      /* incremented by the SET_OBJ_CMD macro */

    /* get information about item; this applies to all items if known */
    if (objects[obj->otyp].oc_name_known)
        SET_OBJ_CMD('i', "whatis", "Get information about %s", 0);

    /* apply item; this can mean almost anything depending on the item */
    if (obj->otyp == CREAM_PIE)
        SET_OBJ_CMD('a', "apply", "Hit yourself with %s", 0);
    else if (obj->otyp == BULLWHIP)
        SET_OBJ_CMD('a', "apply", "Lash out with %s", 0);
    else if (obj->otyp == GRAPPLING_HOOK)
        SET_OBJ_CMD('a', "apply", "Grapple something with %s", 0);
    else if (obj->otyp == BAG_OF_TRICKS && obj->known)
        SET_OBJ_CMD('a', "apply", "Reach into this %s", 0);
    else if (Is_container(obj) || obj->otyp == BAG_OF_TRICKS)
        SET_OBJ_CMD('a', "apply", "Open this %s", 0);
    else if (obj->otyp == CAN_OF_GREASE)
        SET_OBJ_CMD('a', "apply", "Use %s to grease an item", 0);
    else if (obj->otyp == LOCK_PICK || obj->otyp == CREDIT_CARD ||
             obj->otyp == SKELETON_KEY)
        SET_OBJ_CMD('a', "apply", "Use %s to pick a lock", 0);
    else if (obj->otyp == TINNING_KIT)
        SET_OBJ_CMD('a', "apply", "Use %s to tin a corpse", 0);
    else if (obj->otyp == LEASH)
        SET_OBJ_CMD('a', "apply", "Tie a pet to %s", 0);
    else if (obj->otyp == SADDLE)
        SET_OBJ_CMD('a', "apply", "Place %s on a pet", 0);
    else if (obj->otyp == MAGIC_WHISTLE || obj->otyp == TIN_WHISTLE)
        SET_OBJ_CMD('a', "apply", "Blow %s", 0);
    else if (obj->otyp == EUCALYPTUS_LEAF)
        SET_OBJ_CMD('a', "apply", "Use %s as a whistle", 0);
    else if (obj->otyp == STETHOSCOPE)
        SET_OBJ_CMD('a', "apply", "Listen through %s", 0);
    else if (obj->otyp == MIRROR)
        SET_OBJ_CMD2('a', "apply", "Show something its reflection");
    else if (obj->otyp == BELL || obj->otyp == BELL_OF_OPENING)
        SET_OBJ_CMD('a', "apply", "Ring %s", 0);
    else if (obj->otyp == CANDELABRUM_OF_INVOCATION)
        SET_OBJ_CMD('a', "apply", "Light or extinguish %s", 0);
    else if ((obj->otyp == WAX_CANDLE || obj->otyp == TALLOW_CANDLE) &&
             carrying(CANDELABRUM_OF_INVOCATION))
        SET_OBJ_CMD('a', "apply", "Attach %s to the candelabrum", 0);
    else if (obj->otyp == WAX_CANDLE || obj->otyp == TALLOW_CANDLE)
        SET_OBJ_CMD('a', "apply", "Light or extinguish %s", 0);
    else if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
             obj->otyp == BRASS_LANTERN)
        SET_OBJ_CMD('a', "apply", "Light or extinguish %s", 0);
    else if (obj->otyp == POT_OIL)
        SET_OBJ_CMD('a', "apply", "Light or extinguish %s", 0);
    else if (obj->otyp == EXPENSIVE_CAMERA)
        SET_OBJ_CMD2('a', "apply", "Take a photograph");
    else if (obj->otyp == TOWEL)
        SET_OBJ_CMD('a', "apply", "Clean yourself off with %s", 0);
    else if (obj->otyp == CRYSTAL_BALL)
        SET_OBJ_CMD('a', "apply", "Peer into %s", 0);
    else if (obj->otyp == MAGIC_MARKER)
        SET_OBJ_CMD('a', "apply", "Write on something with %s", 0);
    else if (obj->otyp == FIGURINE)
        SET_OBJ_CMD('a', "apply", "Make %s transform", 0);
    else if (obj->otyp == UNICORN_HORN)
        SET_OBJ_CMD('a', "apply", "Squeeze %s tightly", 0);
    else if ((obj->otyp >= WOODEN_FLUTE && obj->otyp <= DRUM_OF_EARTHQUAKE) ||
             (obj->otyp == HORN_OF_PLENTY && !obj->known))
        SET_OBJ_CMD('a', "apply", "Play %s", 0);
    else if (obj->otyp == HORN_OF_PLENTY)
        SET_OBJ_CMD('a', "apply", "Blow into %s", 0);
    else if (obj->otyp == LAND_MINE || obj->otyp == BEARTRAP)
        SET_OBJ_CMD('a', "apply", "Arm %s", 0);
    else if (is_pick(obj))
        SET_OBJ_CMD('a', "apply", "Dig with %s", 0);
    else if (is_axe(obj))
        SET_OBJ_CMD('a', "apply", "Chop a tree with %s", 0);
    else if (is_pole(obj))
        SET_OBJ_CMD('a', "apply", "Strike at a distance with %s", 0);

    /* drop item, works on almost everything */
    if (canletgo(obj, ""))
        SET_OBJ_CMD('d', "drop", "Drop %s", 0);

    /* dip */
    if (obj->oclass == POTION_CLASS)
        SET_OBJ_CMD('D', "dip", "Dip something into %s", 1);

    /* eat item; eat.c provides is_edible to check */
    if (obj->otyp == TIN && uwep && uwep->otyp == TIN_OPENER)
        SET_OBJ_CMD('e', "eat", "Open and eat %s with your tin opener", 1);
    else if (obj->otyp == TIN)
        SET_OBJ_CMD('e', "eat", "Open and eat %s", 1);
    else if (is_edible(obj, TRUE))
        SET_OBJ_CMD('e', "eat", "Eat %s", 1);

    /* engrave with item */
    if (obj->otyp == TOWEL)
        SET_OBJ_CMD('E', "engrave", "Wipe the floor with %s", 0);
    else if (obj->otyp == MAGIC_MARKER)
        SET_OBJ_CMD2('E', "engrave", "Scribble graffiti on the floor");
    else if (obj->oclass == WEAPON_CLASS || obj->oclass == WAND_CLASS ||
             obj->oclass == GEM_CLASS || obj->oclass == RING_CLASS)
        SET_OBJ_CMD('E', "engrave", "Write on the floor with %s", 0);

    /* pay for this item */
    if ((shkp = shop_keeper(level, *in_rooms(level, u.ux, u.uy, SHOPBASE))) &&
        inhishop(shkp) && obj->unpaid) {
        char desc[80];  /* == sizeof(obj_cmd[i].desc) */
        long price = unpaid_cost(obj);
        boolean unpaid_contents = Has_contents(obj) &&
            contained_cost(obj, shkp, 0L, FALSE, TRUE) > 0;
        snprintf(desc, 80, "pay (%ld %s%s)", price, currency(price),
                 unpaid_contents ? ", excluding contents" : "");
        SET_OBJ_CMD2('p', "pay", desc);
    }

    /* drink item; strangely, this one seems to have no exceptions */
    if (obj->oclass == POTION_CLASS)
        SET_OBJ_CMD('q', "drink", "Quaff %s", 1);

    /* quiver throwable item (Why are weapons not designed for throwing
       included, I wonder?) */
    if ((obj->oclass == GEM_CLASS || obj->oclass == WEAPON_CLASS) &&
        !obj->owornmask)
        SET_OBJ_CMD('Q', "quiver", "Quiver %s for easy throwing", 0);

    /* read item. Note: Fortune Cookies and T-shirt are intentionally omitted
       here, as getobj() also goes to some lengths to omit them from the list of
       items available for reading */
    if (obj->oclass == SCROLL_CLASS)
        SET_OBJ_CMD('r', "read", "Cast the spell on %s", 1);
    else if (obj->oclass == SPBOOK_CLASS)
        SET_OBJ_CMD('r', "read", "Study %s", 0);

    /* rub */
    if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
        obj->otyp == BRASS_LANTERN)
        SET_OBJ_CMD('V', "rub", "Rub %s", 0);
    else if (obj->oclass == GEM_CLASS && is_graystone(obj))
        SET_OBJ_CMD('V', "rub", "Rub something on %s", 0);

    /* throw item, works on almost everything */
    if (!(obj->owornmask & (W_WORN | W_MASK(os_saddle)))) {
        /* you automatically throw only 1 item - except for gold */
        if (obj->oclass == COIN_CLASS)
            SET_OBJ_CMD('t', "throw", "Throw %s", 0);
        else
            SET_OBJ_CMD('t', "throw", "Throw %s", 1);
    }

    /* unequip armor */
    if (obj->owornmask & W_WORN)
        SET_OBJ_CMD('T', "takeoff", "Take %s off", 0);

    /* invoke */
    if ((obj->otyp == FAKE_AMULET_OF_YENDOR && !obj->known) ||
        obj->oartifact || objects[obj->otyp].oc_unique ||
        obj->otyp == MIRROR)   /* deception, according to
                                  object_selection_checks */
        SET_OBJ_CMD('V', "invoke", "Try to invoke a unique power of %s", 0);
    else if (obj->oclass == WAND_CLASS)
        SET_OBJ_CMD('V', "invoke", "Break %s", 0);

    /* wield: hold in hands, works on everything but with different advice text;
       not mentioned for things that are already equipped, except for quivered
       items */
    if (obj->owornmask & ~W_MASK(os_quiver)) {
    } else if (obj->oclass == WEAPON_CLASS || obj->otyp == PICK_AXE ||
               obj->otyp == UNICORN_HORN)
        SET_OBJ_CMD('w', "wield", "Wield %s as your weapon", 0);
    else if (obj->otyp == TIN_OPENER)
        SET_OBJ_CMD('w', "wield", "Hold %s to open tins", 0);
    else
        SET_OBJ_CMD('w', "wield", "Hold %s in your hands", 0);

    /* wear: Equip this item */
    if (!obj->owornmask) {
        if (obj->oclass == ARMOR_CLASS)
            SET_OBJ_CMD('W', "wear", "Wear %s", 0);
        else if (obj->oclass == RING_CLASS || obj->otyp == MEAT_RING)
            SET_OBJ_CMD('W', "wear", "Put %s on", 0);
        else if (obj->oclass == AMULET_CLASS)
            SET_OBJ_CMD('W', "wear", "Put %s on", 0);
        else if (obj->otyp == TOWEL || obj->otyp == BLINDFOLD)
            SET_OBJ_CMD('W', "wear", "Use %s to blindfold yourself", 0);
        else if (obj->otyp == LENSES)
            SET_OBJ_CMD('W', "wear", "Put %s on", 0);
    }

    /* swap weapons: mentioned for wielded or offhand weapons */
    if (obj->owornmask == W_MASK(os_wep))
        SET_OBJ_CMD('x', "swapweapon", "Move %s to your offhand", 0);
    else if (obj->owornmask == W_MASK(os_swapwep))
        SET_OBJ_CMD('x', "swapweapon", "Move %s to your main hand", 0);

    /* zap wand */
    if (obj->oclass == WAND_CLASS)
        SET_OBJ_CMD('z', "zap", "Zap %s to release its magic", 0);

    /* sacrifice object */
    if (IS_ALTAR(level->locations[u.ux][u.uy].typ) && !Engulfed) {
        if (In_endgame(&u.uz) &&
            (obj->otyp == AMULET_OF_YENDOR ||
             obj->otyp == FAKE_AMULET_OF_YENDOR))
            SET_OBJ_CMD('S', "sacrifice", "Sacrifice %s at this altar", 0);
        else if (obj->otyp == CORPSE)
            SET_OBJ_CMD('S', "sacrifice", "Sacrifice %s at this altar", 1);
    }

    /* name object */
    if (obj->oclass != COIN_CLASS)
        SET_OBJ_CMD('N', "nameitem", "Name %s", 0);

    /* name type */
    if (obj->oclass != COIN_CLASS && obj->oclass != WEAPON_CLASS &&
        obj->oclass != ROCK_CLASS && obj->oclass != CHAIN_CLASS &&
        obj->oclass != BALL_CLASS && obj->oclass != VENOM_CLASS)
        set_obj_cmd('C', obj_cmd, obj, i++, "nametype",
                    "Name all objects of this type", false);

    *count = i;
    return obj_cmd;
}

#undef SET_OBJ_CMD


static const char template[] = "%-18s %4ld  %6ld";
static const char count_str[] = "                   count  bytes";
static const char separator[] = "------------------ -----  ------";

static void
count_obj(struct obj *chain, long *total_count, long *total_size, boolean top,
          boolean recurse)
{
    long count, size;
    struct obj *obj;

    for (count = size = 0, obj = chain; obj; obj = obj->nobj) {
        if (top) {
            count++;
            size += sizeof (struct obj) + obj->oxlth + obj->onamelth;
        }
        if (recurse && obj->cobj)
            count_obj(obj->cobj, total_count, total_size, TRUE, TRUE);
    }
    *total_count += count;
    *total_size += size;
}

static void
obj_chain(struct nh_menulist *menu, const char *src, struct obj *chain,
          long *total_count, long *total_size)
{
    const char *buf;
    long count = 0, size = 0;

    count_obj(chain, &count, &size, TRUE, FALSE);
    *total_count += count;
    *total_size += size;
    buf = msgprintf(template, src, count, size);
    add_menutext(menu, buf);
}

static void
mon_invent_chain(struct nh_menulist *menu, const char *src, struct monst *chain,
                 long *total_count, long *total_size)
{
    const char *buf;
    long count = 0, size = 0;
    struct monst *mon;

    for (mon = chain; mon; mon = mon->nmon)
        count_obj(mon->minvent, &count, &size, TRUE, FALSE);
    *total_count += count;
    *total_size += size;
    buf = msgprintf(template, src, count, size);
    add_menutext(menu, buf);
}

static void
contained(struct nh_menulist *menu, const char *src, long *total_count,
          long *total_size)
{
    const char *buf;
    long count = 0, size = 0;
    struct monst *mon;

    count_obj(invent, &count, &size, FALSE, TRUE);
    count_obj(level->objlist, &count, &size, FALSE, TRUE);
    count_obj(level->buriedobjlist, &count, &size, FALSE, TRUE);
    /* DEADMONSTER check not required in this loop since they have no
       inventory */
    for (mon = level->monlist; mon; mon = mon->nmon)
        count_obj(mon->minvent, &count, &size, FALSE, TRUE);
    for (mon = migrating_mons; mon; mon = mon->nmon)
        count_obj(mon->minvent, &count, &size, FALSE, TRUE);

    *total_count += count;
    *total_size += size;

    buf = msgprintf(template, src, count, size);
    add_menutext(menu, buf);
}

static void
mon_chain(struct nh_menulist *menu, const char *src, struct monst *chain,
          long *total_count, long *total_size)
{
    const char *buf;
    long count, size;
    struct monst *mon;

    for (count = size = 0, mon = chain; mon; mon = mon->nmon) {
        count++;
        size += sizeof (struct monst) + mon->mxlth + mon->mnamelth;
    }
    *total_count += count;
    *total_size += size;
    buf = msgprintf(template, src, count, size);
    add_menutext(menu, buf);
}

/*
 * Display memory usage of all monsters and objects on the level.
 */
static int
wiz_show_stats(const struct nh_cmd_arg *arg)
{
    const char *buf;
    struct nh_menulist menu;
    long total_obj_size = 0, total_obj_count = 0;
    long total_mon_size = 0, total_mon_count = 0;

    (void) arg;

    init_menulist(&menu);
    add_menutext(&menu, "Current memory statistics:");
    add_menutext(&menu, "");
    buf = msgprintf("Objects, size %d", (int)sizeof (struct obj));
    add_menutext(&menu, buf);
    add_menutext(&menu, "");
    add_menutext(&menu, count_str);

    obj_chain(&menu, "invent", invent, &total_obj_count, &total_obj_size);
    obj_chain(&menu, "level->objlist", level->objlist, &total_obj_count,
              &total_obj_size);
    obj_chain(&menu, "buried", level->buriedobjlist, &total_obj_count,
              &total_obj_size);
    mon_invent_chain(&menu, "minvent", level->monlist, &total_obj_count,
                     &total_obj_size);
    mon_invent_chain(&menu, "migrating minvent", migrating_mons,
                     &total_obj_count, &total_obj_size);

    contained(&menu, "contained", &total_obj_count, &total_obj_size);

    add_menutext(&menu, separator);
    buf = msgprintf(template, "Total", total_obj_count, total_obj_size);
    add_menutext(&menu, buf);

    add_menutext(&menu, "");
    add_menutext(&menu, "");
    buf = msgprintf("Monsters, size %d", (int)sizeof (struct monst));
    add_menutext(&menu, buf);
    add_menutext(&menu, "");

    mon_chain(&menu, "level->monlist", level->monlist, &total_mon_count,
              &total_mon_size);
    mon_chain(&menu, "migrating", migrating_mons, &total_mon_count,
              &total_mon_size);

    add_menutext(&menu, separator);
    buf = msgprintf(template, "Total", total_mon_count, total_mon_size);
    add_menutext(&menu, buf);

    display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    return 0;
}

boolean
dir_to_delta(enum nh_direction dir, schar * dx, schar * dy, schar * dz)
{
    if (dir == DIR_NONE)
        return FALSE;

    *dx = xdir[dir];
    *dy = ydir[dir];
    *dz = zdir[dir];

    return TRUE;
}

/* In case of invalid input, returns DIR_NONE. */
void
arg_from_delta(schar dx, schar dy, schar dz, struct nh_cmd_arg *arg)
{
    int i;
    arg->argtype = CMD_ARG_DIR;
    arg->dir = DIR_NONE;

    /* TODO: Bleh at the hardcoded 11. */
    for (i = 0; i < 11; i++) {
        if (dx == xdir[i] && dy == ydir[i] && dz == zdir[i])
            arg->dir = i;
    }
}

int
getargdir(const struct nh_cmd_arg *arg, const char *query,
          schar *dx, schar *dy, schar *dz)
{
    /* Is there a reasonable direction specified already? */
    if ((arg->argtype & CMD_ARG_DIR) &&
        dir_to_delta(arg->dir, dx, dy, dz) &&
        (!*dx || !*dy || u.umonnum != PM_GRID_BUG)) {

        turnstate.intended_dx = *dx;
        turnstate.intended_dy = *dy;

        /* getdir() has a stun/confusion check; replicate that here.

           (Doing things this way prevents a bug whereby command repeat would
           remember the direction chosen from stun/confusion rather than
           re-randomizing it each turn.) */
        if (!*dz && (Stunned || (Confusion && !rn2(5))))
            confdir(dx, dy);

        return 1;
    }

    /* Otherwise, ask. */
    return getdir(query, dx, dy, dz, TRUE);
}

int
getargpos(const struct nh_cmd_arg *arg, coord *cc, boolean force,
          const char *goal)
{
    /* Did the client specify an (in bounds) position? */
    if ((arg->argtype & CMD_ARG_POS) && isok(cc->x, cc->y)) {
        cc->x = arg->pos.x;
        cc->y = arg->pos.y;
        return NHCR_ACCEPTED;
    }

    /* Otherwise, ask. */
    return getpos(cc, force, goal, TRUE);
}

struct obj *
getargobj(const struct nh_cmd_arg *arg, const char *let, const char *word)
{
    struct obj *obj = NULL, *otmp;

    /* Did the client specify an inventory letter? */
    if (arg->argtype & CMD_ARG_OBJ)
        for (otmp = invent; otmp && !obj; otmp = otmp->nobj)
            if (otmp->invlet == arg->invlet)
                obj = otmp;

    /* If so, is that a valid object? (If not, ask for another.) */
    if (obj && validate_object(obj, let, word))
        return obj;

    /* What about '-' or ','? */
    if (arg->argtype & CMD_ARG_OBJ &&
        (arg->invlet == '-' || arg->invlet == ',') &&
        strchr(let, ALLOW_NONE))
        return &zeroobj;

    /* Otherwise, prompt the user. */
    return getobj(let, word, TRUE);
}

const char *
getarglin(const struct nh_cmd_arg *arg, const char *query)
{
    /* Did the user specify a string? Ensure that it isn't empty, nor a cancel
       string. */
    if ((arg->argtype & CMD_ARG_STR) &&
        *(arg->str) != '\033' && *(arg->str))
        return msg_from_string(arg->str);

    /* Otherwise, prompt for one. */
    return getlin(query, TRUE);
}

int
get_command_idx(const char *command)
{
    int i;

    if (!command || !command[0])
        return -1;

    for (i = 0; cmdlist[i].name; i++)
        if (!strcmp(command, cmdlist[i].name))
            return i;

    return -1;
}

/* Returns TRUE if the command that the user entered during the current turn (if
   it's their turn) or their previous turn (if it isn't) was the named command
   exactly, no synonyms. The main purpose of this is for commands that require a
   specific method of invoking before they will do something dangerous (e.g.
   opening a door needs the "open" in order to unlock it, which is
   timeconsuming). Except for this purpose, commands shouldn't care about how
   they were invoked. */
boolean
last_command_was(const char *command)
{
    return flags.last_cmd == get_command_idx(command);
}

enum nh_command_status
do_command(int command, struct nh_cmd_arg *arg)
{
    int res;

    /* protection from out-of-range values from the client */
    if (command < 0)
        return COMMAND_UNKNOWN;
    if (command >= (sizeof cmdlist / sizeof *cmdlist))
        return COMMAND_UNKNOWN;

    /* Although accessible to the user (because they may want to repeat/continue
       actions even if interrupted), "repeat" is special in that it's also sent
       spontaneously by the windowport to repeat actions or continue multi-turn
       actions. Note that a client is never forced to repeat; if do_command()
       runs at all, the user (if using an appropriate client) has full control
       over whether an action continues or not. */
    if (!cmdlist[command].func) {
        command = flags.last_cmd;
        arg = &flags.last_arg;
        turnstate.continue_message = FALSE;
    } else if (!(cmdlist[command].flags & (CMD_INTERNAL | CMD_NOTIME)) ||
               strcmp(cmdlist[command].name, "welcome") == 0) {
        flags.last_cmd = command;
        flags.last_arg = *arg;

        /* Make sure we have a locally allocated copy of the string argument,
           if necessary. */
        if (flags.last_str_buf)
            free(flags.last_str_buf);
        if (flags.last_arg.argtype & CMD_ARG_STR) {
            flags.last_str_buf = malloc(strlen(flags.last_arg.str) + 1);
            strcpy(flags.last_str_buf, flags.last_arg.str);
            flags.last_arg.str = flags.last_str_buf;
        } else {
            flags.last_arg.str = NULL; /* unnecessary right now, but... */
            flags.last_str_buf = NULL;
        }

        /* Zero out any unused fields; the value doesn't matter, but they're
           saved in the save file, thus must be defined. */
        if (!(flags.last_arg.argtype & CMD_ARG_DIR))
            flags.last_arg.dir = 0;
        if (!(flags.last_arg.argtype & CMD_ARG_POS)) {
            flags.last_arg.pos.x = 0;
            flags.last_arg.pos.y = 0;
        }
        if (!(flags.last_arg.argtype & CMD_ARG_OBJ))
            flags.last_arg.invlet = 0;
        if (!(flags.last_arg.argtype & CMD_ARG_SPELL))
            flags.last_arg.spelllet = 0;
        if (!(flags.last_arg.argtype & CMD_ARG_LIMIT))
            flags.last_arg.limit = 0;
    }

    /* Debug commands are now restricted to wizard mode here, rather than with
       a special case in each command */
    if (cmdlist[command].flags & CMD_DEBUG && !wizard)
        return COMMAND_DEBUG_ONLY;

    if (u.uburied && !cmdlist[command].can_if_buried) {
        pline("You can't do that while you are buried!");
        res = 0;
        action_completed();
    } else {
        res = (*cmdlist[command].func) (arg);
    }

    /* If the command takes no time, most likely it was cancelled, and there's
       probably not much sense in repeat-counting it anyway.

       Exception: server cancels are a true no-op, and mustn't tinker with
       the "completed" flag. */
    if (!res && !(cmdlist[command].flags & CMD_INTERNAL))
        action_completed();

    turnstate.continue_message = TRUE;

    return res ? COMMAND_OK : COMMAND_ZERO_TIME;
}


int
xytod(schar x, schar y)
{       /* convert an x,y pair into a direction code */
    int dd;

    for (dd = 0; dd < 8; dd++)
        if (x == xdir[dd] && y == ydir[dd])
            return dd;

    return -1;
}

void
dtoxy(coord * cc, int dd)
{       /* convert a direction code into an x,y pair */
    cc->x = xdir[dd];
    cc->y = ydir[dd];
    return;
}


/*
 * uses getdir() but unlike getdir() it specifically
 * produces coordinates using the direction from getdir()
 * and verifies that those coordinates are ok.
 *
 * If the call to getdir() returns 0, "Never mind." is displayed.
 * If the resulting coordinates are not okay, emsg is displayed.
 *
 * Returns non-zero if coordinates in cc are valid.
 */
int
get_adjacent_loc(const char *prompt, const char *emsg, xchar x, xchar y,
                 coord * cc, schar * dz)
{
    xchar new_x, new_y;
    schar dx, dy;

    if (!getdir(prompt, &dx, &dy, dz, FALSE)) {
        pline("Never mind.");
        return 0;
    }
    new_x = x + dx;
    new_y = y + dy;
    if (cc && isok(new_x, new_y)) {
        cc->x = new_x;
        cc->y = new_y;
    } else {
        if (emsg)
            pline("%s", emsg);
        return 0;
    }
    return 1;
}


void
confdir(schar * dx, schar * dy)
{
    int x = (u.umonnum == PM_GRID_BUG) ? 2 * rn2(4) : rn2(8);

    *dx = xdir[x];
    *dy = ydir[x];
    return;
}

static int
doquit(const struct nh_cmd_arg *arg)
{
    (void) arg;
    pline("To quit the game, use the 'save' command (typically on 'S').");

    return 0;
}

static int
doautoexplore(const struct nh_cmd_arg *arg)
{
    (void) arg;

    action_incomplete("exploring", occ_autoexplore);
    return domove(&(struct nh_cmd_arg){.argtype = CMD_ARG_DIR, .dir = DIR_SELF},
                  exploration_interaction_status(), occ_autoexplore);
}

static int
dotravel(const struct nh_cmd_arg *arg)
{
    /* Keyboard travel command */
    coord cc;

    cc.x = flags.travelcc.x;
    cc.y = flags.travelcc.y;
    if (cc.x == -1 && cc.y == -1) {
        /* No cached destination, start attempt from current position */
        cc.x = u.ux;
        cc.y = u.uy;
    }
    if (!(arg->argtype & CMD_ARG_POS))
        pline("Where do you want to travel to?");
    if (getargpos(arg, &cc, FALSE, "the desired destination") ==
        NHCR_CLIENT_CANCEL) {
        if (flags.verbose)
            pline("Never mind.");
        return 0;
    }
    flags.travelcc.x = u.tx = cc.x;
    flags.travelcc.y = u.ty = cc.y;

    action_incomplete("travelling", occ_travel);
    return domove(&(struct nh_cmd_arg){.argtype = CMD_ARG_DIR, .dir = DIR_SELF},
                  exploration_interaction_status(), occ_travel);
}

/*cmd.c*/
