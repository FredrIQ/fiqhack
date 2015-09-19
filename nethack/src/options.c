/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-08-22 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include "common_options.h"
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

enum option_lists {
    NO_LIST,
    BIRTH_OPTS,
    CREATION_OPTS,
    GAME_OPTS,
    UI_OPTS
};

enum optstyles {
    OPTSTYLE_DESC,
    OPTSTYLE_NAME,
    OPTSTYLE_FULL
};

enum extra_opttypes {
    OPTTYPE_KEYMAP = 1000
};

static void show_autopickup_menu(struct nh_option_desc *opt);



/*----------------------------------------------------------------------------*/

#define listlen(list) (sizeof(list)/sizeof(list[0]))

static struct nh_listitem menu_headings_list[] = {
    {A_NORMAL, "none"},
    {A_REVERSE, "inverse"},
    {A_BOLD, "bold"},
    {A_UNDERLINE, "underline"}
};
static struct nh_enum_option menu_headings_spec =
    { menu_headings_list, listlen(menu_headings_list) };

static struct nh_listitem palette_list[] = {
    {PALETTE_NONE,      "terminal default (may require new window)"},
    {PALETTE_DEFAULT,   "uncursed default"},
    {PALETTE_SATURATED, "saturated bold"},
    {PALETTE_TERTIARY,  "tertiary colors"},
    {PALETTE_EQUILUM,   "equiluminous"},
    {PALETTE_DKPASTELS, "dark pastels"},
    {PALETTE_TTERMINAL, "typical X11 terminal"},
    {PALETTE_LCONSOLE,  "Linux console"},
    {PALETTE_REDMOND,   "Windows console"},
    {PALETTE_ALT1,      "alternative 1"},
    {PALETTE_ALT2,      "alternative 2"},
    {PALETTE_ALT3,      "alternative 3"}
};
static struct nh_enum_option palette_spec =
    { palette_list, listlen(palette_list) };

static struct nh_listitem animation_list[] = {
    {ANIM_INSTANT, "instant"},
    {ANIM_INTERRUPTIBLE, "interruptible"},
    {ANIM_ALL, "everything"},
    {ANIM_SLOW, "slow"},
};
static struct nh_enum_option animation_spec =
    { animation_list, listlen(animation_list) };

static struct nh_listitem menupaging_list[] = {
    {MP_LINES, "by lines"},
    {MP_PAGES, "by pages"},
};
static struct nh_enum_option menupaging_spec =
    { menupaging_list, listlen(menupaging_list) };

static struct nh_listitem optstyle_list[] = {
    {OPTSTYLE_DESC, "description only"},
    {OPTSTYLE_NAME, "name only"},
    {OPTSTYLE_FULL, "name + description"}
};
static struct nh_enum_option optstyle_spec =
    { optstyle_list, listlen(optstyle_list) };

static struct nh_listitem networkmotd_list[] = {
    {MOTD_TRUE, "on"},
    {MOTD_FALSE, "off"},
    {MOTD_ASK, "ask"},
};
static struct nh_enum_option networkmotd_spec =
    { networkmotd_list, listlen(networkmotd_list) };

static struct nh_listitem autoable_boolean_list[] = {
    {AB_FALSE, "never"},
    {AB_TRUE,  "always"},
    {AB_AUTO,  "auto"},
};
static struct nh_enum_option autoable_boolean_spec =
    { autoable_boolean_list, listlen(autoable_boolean_list) };

static struct nh_listitem frame_list[] = {
    {FRAME_ALL,   "screen and menus"},
    {FRAME_MENUS, "menus only"},
    {FRAME_NONE,  "nowhere"},
};
static struct nh_enum_option frame_spec =
    { frame_list, listlen(frame_list) };

static const char *const bucnames[] =
    { "unknown", "blessed", "uncursed", "cursed", "all" };

static struct nh_option_desc curses_options[] = {
    {"alt_is_esc", "interpret Alt-letter as ESC letter",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"animation", "what to animate, and how fast",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = ANIM_ALL}},
    {"border", "what to draw borders around",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = FRAME_ALL}},
    {"comment", "has no effect",
     nh_birth_ingame, OPTTYPE_STRING, {.s = NULL}},
    {"darkgray", "try to show 'black' as dark gray instead of dark blue",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"draw_branch", "use different renderings for different branches",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"draw_detected", "mark detected monsters",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"draw_rock", "make the walls of corridors visible",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"draw_stepped", "mark squares you have walked on",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"draw_tame", "mark tame and peaceful monsters",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"draw_terrain", "mark concealed traps and stairs",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"extmenu", "use a menu for selecting extended commands (#)",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"invweight", "show item weights in the inventory",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"keymap", "alter the key to command mapping",
     nh_birth_ingame, (enum nh_opttype)OPTTYPE_KEYMAP, {0}},
    {"menu_headings", "display style for menu headings",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = A_REVERSE}},
    {"mouse", "accept mouse input (where supported)",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"menupaging", "scrolling behaviour of menus",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = MP_LINES}},
    {"msgheight", "message window height",
     nh_birth_ingame, OPTTYPE_INT, {.i = 8}},
    {"msghistory", "number of messages saved for prevmsg",
     nh_birth_ingame, OPTTYPE_INT, {.i = 256}},
    {"networkmotd", "get tips and announcements from the Internet",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = MOTD_ASK}},
    {"optstyle", "option menu display style",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = OPTSTYLE_FULL}},
    {"palette", "color palette used for text",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = PALETTE_DEFAULT}},
    {"prompt_inline", "place prompts in the message window",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"scores_own", "show all your own scores in the list",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = FALSE}},
    {"scores_top", "how many top scores to show",
     nh_birth_ingame, OPTTYPE_INT, {.i = 3}},
    {"scores_around", "the number of scores shown around your score",
     nh_birth_ingame, OPTTYPE_INT, {.i = 2}},
    {"sidebar", "when to draw the inventory sidebar",
     nh_birth_ingame, OPTTYPE_ENUM, {.e = AB_AUTO}},
    {"status3", "3 line status display",
     nh_birth_ingame, OPTTYPE_BOOL, {.b = TRUE}},
    {"tileset", "text or graphics for map drawing",
     nh_birth_ingame, OPTTYPE_STRING, {.s = NULL}},
    {NULL, NULL, nh_birth_ingame, OPTTYPE_BOOL, {NULL}}};

static struct nhlib_boolopt_map boolopt_map[] = {
    {"alt_is_esc", &settings.alt_is_esc},
    {"draw_branch", &settings.dungeoncolor},
    {"draw_detected", &settings.use_inverse},
    {"draw_rock", &settings.visible_rock},
    {"draw_stepped", &settings.floorcolor},
    {"draw_tame", &settings.hilite_pet},
    {"draw_terrain", &settings.bgbranding},
    {"darkgray", &settings.darkgray},
    {"extmenu", &settings.extmenu},
    {"invweight", &settings.invweight},
    {"mouse", &settings.mouse},
    {"prompt_inline", &settings.prompt_inline},
    {"scores_own", &settings.end_own},
    {"status3", &settings.status3},
    {NULL, NULL}
};

/*
 * When editing game options, there are two scenarios:
 *  - A game in progress, in which case changes apply only to that game, and
 *    we let the game engine save them.
 *  - No game in progress, in which case changes apply to new games and are
 *    saved by the interface.
 *
 * This pointer is to the client's local copy of the game options, which are
 * edited when no game is in progress, and used to set the startup options for a
 * new game. They have no effect on already-started games.
 *
 * This pointer should not be used directly in the options menu code. Instead,
 * curses_get_nh_opts will retrieve a copy of the game options to use at that
 * point in time. The returned option list should then be passed to
 * curses_free_nh_opts, which will free the option list if it was obtained anew
 * from the game, or do nothing if it was just this pointer. In this way, we
 * abstract away the notion of the current option list.
 */
static struct nh_option_desc *nh_options = NULL;


struct nh_option_desc *
curses_get_nh_opts(void)
{
    if (game_is_running)
        return nh_get_options();
    else
        return nh_options;
}


void
curses_free_nh_opts(struct nh_option_desc *opts)
{
    if (game_is_running) {
        if (opts == nh_options)
            curses_impossible("Freeing non-game options during a game!");
        else
            nhlib_free_optlist(opts);
    } else if (opts != nh_options) {
        curses_impossible("Freeing game options outside a game!");
        nhlib_free_optlist(opts);
    }
}


nh_bool
curses_set_option(const char *name, union nh_optvalue value)
{
    nh_bool game_option = FALSE;
    struct nh_option_desc *option = nhlib_find_option(curses_options, name);

    if (!option) {
        if (game_is_running)
            return nh_set_option(name, value);

        /* If the game is not running, update our local copy of options. */
        if (!nh_options || !(option = nhlib_find_option(nh_options, name))) {
            return FALSE;
        }
        game_option = TRUE;
    }

    if ((int)option->type == OPTTYPE_KEYMAP) {
        return FALSE;
    }

    if (!nhlib_option_value_ok(option, value))
        return FALSE;

    nhlib_copy_option_value(option, value);

    if (game_option)
        return TRUE;

    /* In case the option affects graphics; this is pretty cheap if we don't do
       it every turn */
    mark_mapwin_for_full_refresh();

    if (option->type == OPTTYPE_BOOL) {
        nh_bool *var = nhlib_find_boolopt(boolopt_map, option->name);
        if (!var) {
            curses_impossible("missing boolean option");
            return FALSE;
        }

        *var = value.b;

        if (!strcmp(option->name, "status3")) {
            rebuild_ui();
        } else if (!strcmp(option->name, "darkgray")) {
            set_darkgray();
            draw_map(player.x, player.y);
        } else if (!strcmp(option->name, "mouse")) {
            uncursed_enable_mouse(option->value.b);
        }
    } else if (!strcmp(option->name, "comment")) {
        /* do nothing */
    } else if (!strcmp(option->name, "tileset")) {
        if (settings.tileset)
            free(settings.tileset);
        settings.tileset = malloc(strlen(option->value.s) + 1);
        strcpy(settings.tileset, option->value.s);
        rebuild_ui();
    } else if (!strcmp(option->name, "border")) {
        settings.whichframes = option->value.e;
        rebuild_ui();
    } else if (!strcmp(option->name, "menu_headings")) {
        settings.menu_headings = option->value.e;
    } else if (!strcmp(option->name, "palette")) {
        settings.palette = option->value.e;
        setup_palette();

        if (ui_flags.initialized) {
            /*
             * - We don't want to install a palette as a result of the default
             *   setting of "palette", because some terminals cannot handle a
             *   palette reset, and thus we need to ensure that we've loaded
             *   the user's palette setting before palette initialization.
             *
             * - Besides, clear() will crash with an uninitialized libuncursed.
             *   So we have to delay this anyway.
             */
            clear();
            refresh();
            rebuild_ui();
        }
    } else if (!strcmp(option->name, "animation")) {
        settings.animation = option->value.e;
    } else if (!strcmp(option->name, "sidebar")) {
        settings.sidebar = option->value.e;
        rebuild_ui();
    } else if (!strcmp(option->name, "scores_top")) {
        settings.end_top = option->value.i;
    } else if (!strcmp(option->name, "scores_around")) {
        settings.end_around = option->value.i;
    } else if (!strcmp(option->name, "networkmotd")) {
        settings.show_motd = option->value.e;
    } else if (!strcmp(option->name, "menupaging")) {
        settings.menupaging = option->value.e;
    } else if (!strcmp(option->name, "optstyle")) {
        settings.optstyle = option->value.e;
    } else if (!strcmp(option->name, "msgheight")) {
        settings.msgheight = option->value.i;
        rebuild_ui();
    } else if (!strcmp(option->name, "msghistory")) {
        settings.msghistory = option->value.i;
        alloc_hist_array();
    }
    else
        return FALSE;

    return TRUE;
}


static struct nh_option_desc *
find_option(const char *name)
{
    int i;

    for (i = 0; curses_options[i].name; i++)
        if (!strcmp(name, curses_options[i].name))
            return &curses_options[i];

    return NULL;
}


void
init_options(void)
{
    int i;

    find_option("border")->e = frame_spec;
    find_option("comment")->s.maxlen = BUFSZ;
    find_option("tileset")->s.maxlen = BUFSZ;
    find_option("tileset")->value.s = malloc(sizeof "textunicode");
    strcpy(find_option("tileset")->value.s, "textunicode");
    find_option("menu_headings")->e = menu_headings_spec;
    find_option("msgheight")->i.min = 1;
    find_option("msgheight")->i.max = 40;
    find_option("msghistory")->i.min = 20;      /* arbitrary min/max values */
    find_option("msghistory")->i.max = 20000;
    find_option("animation")->e = animation_spec;
    find_option("networkmotd")->e = networkmotd_spec;
    find_option("optstyle")->e = optstyle_spec;
    find_option("menupaging")->e = menupaging_spec;
    find_option("palette")->e = palette_spec;
    find_option("scores_top")->i.max = 10000;
    find_option("scores_around")->i.max = 100;
    find_option("sidebar")->e = autoable_boolean_spec;

    /* set up option defaults; this is necessary for options that are not
       specified in the config file */
    for (i = 0; curses_options[i].name; i++)
        curses_set_option(curses_options[i].name, curses_options[i].value);

    read_ui_config();
}


static char *
get_display_string(struct nh_option_desc *option)
{
    char *str;

    switch ((int)option->type) {
    default:
    case OPTTYPE_BOOL:
    case OPTTYPE_ENUM:
    case OPTTYPE_INT:
    case OPTTYPE_STRING:
        return nhlib_optvalue_to_string(option);

    case OPTTYPE_AUTOPICKUP_RULES:
    case OPTTYPE_KEYMAP:
        str = malloc(1 + sizeof "submenu");
        strcpy(str, "submenu");
        return str;
    }
}


static void
print_option_string(struct nh_option_desc *option, char *buf)
{
    const char *opttxt;
    char *valstr = get_display_string(option);

    switch (settings.optstyle) {
    case OPTSTYLE_DESC:
        opttxt = option->helptxt;
        if (!opttxt || strlen(opttxt) < 2)
            opttxt = option->name;

        snprintf(buf, BUFSZ, "%.*s\t[%s]", COLS - 21, opttxt, valstr);
        break;

    case OPTSTYLE_NAME:
        snprintf(buf, BUFSZ, "%.*s\t[%s]", COLS - 21, option->name, valstr);
        break;

    default:
    case OPTSTYLE_FULL:
        snprintf(buf, BUFSZ, "%s\t[%s]\t%.*s", option->name, valstr,
                 COLS - 42, option->helptxt);
        break;
    }

    free(valstr);
}


/* add a list of options to the given selection menu */
static int
menu_add_options(struct nh_menulist *menu, int listid,
                 struct nh_option_desc *options, enum nh_optbirth birth,
                 nh_bool read_only)
{
    int i, id;
    char optbuf[256];

    for (i = 0; options[i].name; i++) {
        id = (listid << 10) | i;
        if (options[i].birth_option != birth)
            continue;

        print_option_string(&options[i], optbuf);
        if (read_only)
            add_menu_txt(menu, optbuf, MI_TEXT);
        else
            add_menu_item(menu, id, optbuf, 0, 0);
    }

    /* add an empty line */
    add_menu_txt(menu, "", MI_TEXT);

    return i;
}


/* display a selecton menu for boolean options */
static void
select_boolean_value(union nh_optvalue *value, struct nh_option_desc *option)
{
    struct nh_menulist menu;
    int pick_list[1];

    init_menulist(&menu);

    add_menu_txt(&menu, option->helptxt, MI_TEXT);
    add_menu_txt(&menu, "", MI_TEXT);
    add_menu_item(&menu, 1, option->value.b ? "true (set)" : "true", 't', 0);
    add_menu_item(&menu, 2, option->value.b ? "false" : "false (set)", 'f', 0);

    curses_display_menu(&menu, option->name, PICK_ONE, PLHINT_RIGHT,
                        pick_list, curses_menu_callback);

    value->b = option->value.b; /* in case of ESC */
    if (pick_list[0] != CURSES_MENU_CANCELLED)
        value->b = pick_list[0] == 1;
}


/* display a selection menu for enum options */
static void
select_enum_value(union nh_optvalue *value, struct nh_option_desc *option)
{
    struct nh_menulist menu;
    int i, selectidx;

    init_menulist(&menu);

    add_menu_txt(&menu, option->helptxt, MI_TEXT);
    add_menu_txt(&menu, "", MI_TEXT);

    for (i = 0; i < option->e.numchoices; i++) {
        char capbuf[QBUFSZ];
        const char *cap;

        if (option->value.e == option->e.choices[i].id) {
            snprintf(capbuf, QBUFSZ, "%s (set)", option->e.choices[i].caption);
            cap = capbuf;
        } else {
            cap = option->e.choices[i].caption;
        }
        /* don't use the choice ids directly, 0 is a valid value for those */
        add_menu_item(&menu, i + 1, cap, 0, 0);
    }

    int pick_list[1];
    curses_display_menu(&menu, option->name, PICK_ONE, PLHINT_RIGHT, pick_list,
                        curses_menu_callback);

    value->e = option->value.e; /* in case of ESC */
    if (pick_list[0] != CURSES_MENU_CANCELLED) {
        selectidx = pick_list[0] - 1;
        value->e = option->e.choices[selectidx].id;
    }
}

/* display a selection menu for tilesets */
static void
select_tileset_value(union nh_optvalue *value, struct nh_option_desc *option)
{
    struct nh_menulist menu;
    int i;

    int tc;
    struct tileset_description *tilesets = get_tileset_descriptions(&tc);
    struct tileset_description tilesets_copy[tc];
    memcpy(tilesets_copy, tilesets, sizeof tilesets_copy);
    free(tilesets);

    init_menulist(&menu);

    add_menu_txt(&menu, option->helptxt, MI_TEXT);
    add_menu_txt(&menu, "", MI_TEXT);

    for (i = 0; i < tc; i++)
        add_menu_item(&menu, i + 1, tilesets_copy[i].desc, 0, 0);

    int pick_list[1];
    curses_display_menu(&menu, option->name, PICK_ONE, PLHINT_RIGHT, pick_list,
                        curses_menu_callback);

    const char *newname = option->value.s;
    if (pick_list[0] != CURSES_MENU_CANCELLED)
        newname = tilesets_copy[pick_list[0] - 1].basename;

    value->s = malloc(strlen(newname) + 1);
    strcpy(value->s, newname);
}

static void
getlin_option_callback(const char *str, void *option_void)
{
    struct nh_option_desc **option_p = option_void;
    struct nh_option_desc *option = *option_p;

    if ((!*str && option->type != OPTTYPE_STRING) || *str == '\033') {
        /* The user cancelled. */
        *option_p = NULL;
        return;
    }

    if (option->type == OPTTYPE_STRING) {
        option->value.s = strdup(str);
    } else if (option->type == OPTTYPE_INT) {
        errno = 0;
        long l = strtol(str, NULL, 0);
        if (!errno && l >= option->i.min && l <= option->i.max)
            option->value.i = (int)l;
        else
            *option_p = NULL;
    }
}

/* get a new value of the appropriate type for the given option */
static nh_bool
query_new_value(struct win_menu *mdat, int idx)
{
    struct nh_option_desc *option, *optlist;
    struct nh_option_desc optioncopy;
    struct nh_option_desc *optioncopy_p = &optioncopy;
    int listid = mdat->items[idx].id >> 10;
    int id = mdat->items[idx].id & 0x1ff;
    int prev_optstyle = settings.optstyle;
    enum nh_menupaging prev_menupaging = settings.menupaging;
    nh_bool prev_menuborder = settings.whichframes != FRAME_NONE;
    nh_bool ret = FALSE;

    switch (listid) {
    case BIRTH_OPTS:
    case CREATION_OPTS:
    case GAME_OPTS:
        optlist = curses_get_nh_opts();
        break;
    case UI_OPTS:
        optlist = curses_options;
        break;

    default:
        return FALSE;
    }

    option = &optlist[id];
    /* optioncopy holds the new option we're planning to set */
    optioncopy = *option;

    switch ((int)optioncopy.type) {
    case OPTTYPE_BOOL:
        select_boolean_value(&optioncopy.value, &optioncopy);
        break;

    case OPTTYPE_INT:
        if (optioncopy.i.min >= -2147483647-1 &&
            optioncopy.i.max <= 2147483647)
        {
            /* Maximum length of a number as text is 11 chars */
            char query[11 + 1 + strlen(optioncopy.name) +
                       sizeof "New value for  (number from  to )"];
            sprintf(query, "New value for %s (number from %d to %d)",
                    optioncopy.name, optioncopy.i.min, optioncopy.i.max);
            curses_getline(query, &optioncopy_p, getlin_option_callback);
        }
        break;

    case OPTTYPE_ENUM:
        select_enum_value(&optioncopy.value, option);
        break;

    case OPTTYPE_STRING:
        if (!strcmp(optioncopy.name, "tileset")) {
            select_tileset_value(&optioncopy.value, option);
        } else {
            char query[strlen(optioncopy.name) + 1 +
                       sizeof "New value for  (text)"];
            sprintf(query, "New value for %s (text)", optioncopy.name);
            optioncopy.value.s = NULL;
            curses_getline(query, &optioncopy_p, getlin_option_callback);
        }
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        show_autopickup_menu(option);
        goto free;

    case OPTTYPE_KEYMAP:
        show_keymap_menu(FALSE);
        goto free;

    default:
        goto free;
    }

    /* getlin_option_callback NULLs out optioncopy_p to indicate that setting
       was cancelled */
    if (optioncopy_p && !curses_set_option(optioncopy.name, optioncopy.value)) {
        char query[strlen(optioncopy.name) + 1 +
                   sizeof "new value for  rejected"];
        sprintf(query, "new value for %s rejected", optioncopy.name);
        curses_msgwin(query, krc_notification);
    } else if (optioncopy_p) {
        if (listid != UI_OPTS) {
            curses_free_nh_opts(optlist);
            optlist = curses_get_nh_opts();
            option = &optlist[id];
        }

        print_option_string(option, mdat->items[idx].caption);
    }

    /* We need to deallocate any string that might have been allocated by
       the getlin callback. */
    if (optioncopy.type == OPTTYPE_STRING && optioncopy.value.s)
        free(optioncopy.value.s);

    /* If we're changing the option menu appearance, or if we changed game
       options, we need to reload and redraw the menu. */
    if (settings.optstyle != prev_optstyle ||
        settings.menupaging != prev_menupaging ||
        (settings.whichframes != FRAME_NONE) != prev_menuborder ||
        (game_is_running && listid != UI_OPTS))
        ret = TRUE;

free:
    if (listid != UI_OPTS)
        curses_free_nh_opts(optlist);

    return ret;
}


/* display the option dialog */
void
display_options(nh_bool change_birth_opt)
{
    struct nh_menulist menu;
    struct nh_option_desc *options;
    int selected[1];

    do {
        init_menulist(&menu);
        options = curses_get_nh_opts();

        if (!change_birth_opt) {
            /* add general game options */
            add_menu_txt(&menu, "Game options:", MI_HEADING);
            menu_add_options(&menu, GAME_OPTS, options, nh_birth_ingame, FALSE);

            /* add or display birth options */
            add_menu_txt(&menu, "Birth options for this game:", MI_HEADING);
            menu_add_options(&menu, BIRTH_OPTS, options,
                             nh_birth_lasting, TRUE);
        } else {
            /* add or display birth options */
            add_menu_txt(&menu, "Birth options:", MI_HEADING);
            menu_add_options(&menu, BIRTH_OPTS, options,
                             nh_birth_lasting, FALSE);

            add_menu_txt(&menu, "Game creation options:", MI_HEADING);
            menu_add_options(&menu, CREATION_OPTS, options,
                             nh_birth_creation, FALSE);

            add_menu_txt(&menu, "Game options:", MI_HEADING);
            menu_add_options(&menu, GAME_OPTS, options, nh_birth_ingame, FALSE);
        }

        /* add UI specific options */
        add_menu_txt(&menu, "Interface options:", MI_HEADING);
        menu_add_options(&menu, UI_OPTS, curses_options, FALSE, FALSE);

        curses_display_menu_core(
            &menu, "Set what options?", PICK_ONE, selected,
            curses_menu_callback, 0, 0, -1, -1, FALSE, query_new_value, TRUE);

        curses_free_nh_opts(options);
    } while (*selected != CURSES_MENU_CANCELLED);

    write_ui_config();
    if (!game_is_running)
        write_nh_config();
}


void
print_options(void)
{
    struct nh_menulist menu;
    int i;
    char buf[BUFSZ];
    struct nh_option_desc *options = curses_get_nh_opts();

    init_menulist(&menu);

    add_menu_txt(&menu, "Birth options:", MI_HEADING);
    for (i = 0; options[i].name; i++) {
        if (options[i].birth_option != nh_birth_lasting)
            continue;
        snprintf(buf, BUFSZ, "%s\t%s", options[i].name, options[i].helptxt);
        add_menu_txt(&menu, buf, MI_TEXT);
    }
    add_menu_txt(&menu, "", MI_TEXT);

    add_menu_txt(&menu, "Game creation options:", MI_HEADING);
    for (i = 0; options[i].name; i++) {
        if (options[i].birth_option != nh_birth_creation)
            continue;
        snprintf(buf, BUFSZ, "%s\t%s", options[i].name, options[i].helptxt);
        add_menu_txt(&menu, buf, MI_TEXT);
    }
    add_menu_txt(&menu, "", MI_TEXT);

    add_menu_txt(&menu, "Game options:", MI_HEADING);
    for (i = 0; options[i].name; i++) {
        if (options[i].birth_option != nh_birth_ingame)
            continue;
        snprintf(buf, BUFSZ, "%s\t%s", options[i].name, options[i].helptxt);
        add_menu_txt(&menu, buf, MI_TEXT);
    }
    add_menu_txt(&menu, "", MI_TEXT);

    /* add UI specific options */
    add_menu_txt(&menu, "Interface options:", MI_HEADING);
    for (i = 0; curses_options[i].name; i++) {
        snprintf(buf, BUFSZ, "%s\t%s", curses_options[i].name,
                 curses_options[i].helptxt);
        add_menu_txt(&menu, buf, MI_TEXT);
    }

    curses_display_menu(&menu, "Available options:", PICK_NONE,
                        PLHINT_ANYWHERE, NULL, null_menu_callback);

    curses_free_nh_opts(options);
}

/*----------------------------------------------------------------------------*/

static void
autopickup_rules_help(void)
{
    struct nh_menuitem items[] = {
        {0, MI_TEXT,
         "The autopickup rules are only active if autopickup is on."},
        {0, MI_TEXT,
         "If autopickup is on and you walk onto an item, the item is compared"},
        {0, MI_TEXT, "to each rule in turn until a matching rule is found."},
        {0, MI_TEXT,
         "If a match is found, the action specified in that rule is taken"},
        {0, MI_TEXT, "and no other rules are considered."},
        {0, MI_TEXT, ""},
        {0, MI_TEXT,
         "Each rule may match any combination of object name, object type,"},
        {0, MI_TEXT, "and object blessing (including unknown)."},
        {0, MI_TEXT, "A rule that specifies none of these matches everything."},
        {0, MI_TEXT, ""},
        {0, MI_TEXT, "Suppose your rules look like this:"},
        {0, MI_TEXT,
         " 1. IF name matches \"*lizard*\" AND type is \"food\": < GRAB"},
        {0, MI_TEXT,
         " 2. IF name matches \"*corpse*\" AND type is \"food\":   LEAVE >"},
        {0, MI_TEXT,
         " 3. IF type is \"food\":                             < GRAB"},
        {0, MI_TEXT, ""},
        {0, MI_TEXT,
         "A newt corpse will not match rule 1, but rule 2 applies, so"},
        {0, MI_TEXT, "it won't be picked up and rule 3 won't be considered."},
        {0, MI_TEXT,
         "(Strictly speaking, the \"type is food\" part of these rules is not"},
        {0, MI_TEXT, "necessary; it's purpose here is to make the example more "
         "interesting.)"},
        {0, MI_TEXT, ""},
        {0, MI_TEXT,
         "A dagger will not match any of these rules and so it won't"},
        {0, MI_TEXT, "be picked up either."},
        {0, MI_TEXT, ""},
        {0, MI_TEXT,
         "You may select any existing rule to edit it, change its position"},
        {0, MI_TEXT, "in the list, or delete it."},
    };
    curses_display_menu(STATIC_MENULIST(items), "Autopickup rules help:",
                        PICK_NONE, PLHINT_LEFT, NULL, null_menu_callback);
}


static enum nh_bucstatus
get_autopickup_buc(enum nh_bucstatus cur)
{
    struct nh_menuitem items[] = {
        {B_DONT_CARE + 1, MI_NORMAL, "all", 'a'},
        {B_BLESSED + 1, MI_NORMAL, "blessed", 'b'},
        {B_CURSED + 1, MI_NORMAL, "cursed", 'c'},
        {B_UNCURSED + 1, MI_NORMAL, "uncursed", 'u'},
        {B_UNKNOWN + 1, MI_NORMAL, "unknown", 'U'}
    };
    int selected[1];

    curses_display_menu(STATIC_MENULIST(items), "Beatitude match:",
                        PICK_ONE, PLHINT_RIGHT, selected, curses_menu_callback);
    if (*selected == CURSES_MENU_CANCELLED)
        return cur;
    return selected[0] - 1;
}


static int
get_autopickup_oclass(struct nh_autopick_option *desc, int cur)
{
    int i, selected[1];
    struct nh_menulist menu;

    init_menulist(&menu);

    for (i = 0; i < desc->numclasses; i++)
        add_menu_item(&menu, desc->classes[i].id, desc->classes[i].caption,
                      (char)desc->classes[i].id, 0);

    curses_display_menu(&menu, "Object class match:", PICK_ONE,
                        PLHINT_RIGHT, selected, curses_menu_callback);

    if (*selected == CURSES_MENU_CANCELLED)
        return cur;
    return selected[0];
}


/* pos holds maximum position on call, chosen position or -1 on return */
static void
rule_position_callback(const char *str, void *pos_void)
{
    int *pos = pos_void;
    long l;

    if (!*str || *str == '\033') {
        *pos = -1;
        return;
    }

    l = strtol(str, NULL, 0);
    if (l >= 1 && l <= *pos)
        *pos = (int)l;
    else {
        curses_msgwin("Invalid rule position.", krc_notification);
        *pos = -1;
    }
}

static void
rule_pattern_callback(const char *str, void *pat_void)
{
    char *pat = pat_void;

    if (*str == '\033')
        return;
    if (strlen(str) >= AUTOPICKUP_PATTERNSZ) {
        curses_msgwin("That pattern is too long.", krc_notification);
        return;
    }

    memset(pat, 0, AUTOPICKUP_PATTERNSZ);
    strcpy(pat, str);
}

static void
edit_ap_rule(struct nh_autopick_option *desc, struct nh_autopickup_rules *ar,
             int ruleno)
{
    struct nh_autopickup_rule *r = &ar->rules[ruleno];
    struct nh_autopickup_rule tmprule;
    struct nh_menulist menu;
    int i, selected[1], newpos, allocsize;
    char query[BUFSZ], buf[BUFSZ];
    const char *classname;

    do {
        init_menulist(&menu);

        snprintf(buf, ARRAY_SIZE(buf), "rule position:\t[%d]", ruleno + 1);
        add_menu_item(&menu, 1, buf, 0, 0);

        snprintf(buf, ARRAY_SIZE(buf), "name pattern:\t[%s]", r->pattern);
        add_menu_item(&menu, 2, buf, 0, 0);

        classname = NULL;
        for (i = 0; i < desc->numclasses && !classname; i++)
            if (desc->classes[i].id == r->oclass)
                classname = desc->classes[i].caption;
        snprintf(buf, ARRAY_SIZE(buf), "object type:\t[%s]", classname);
        add_menu_item(&menu, 3, buf, 0, 0);

        snprintf(buf, ARRAY_SIZE(buf), "beatitude:\t[%s]", bucnames[r->buc]);
        add_menu_item(&menu, 4, buf, 0, 0);

        snprintf(buf, ARRAY_SIZE(buf), "action:\t[%s]", r->action == AP_GRAB ? "GRAB" : "LEAVE");
        add_menu_item(&menu, 5, buf, 0, 0);
        add_menu_txt(&menu, "", MI_TEXT);
        add_menu_item(&menu, 6, "delete this rule", 'x', 0);

        curses_display_menu(&menu, "Edit rule:", PICK_ONE, PLHINT_RIGHT,
                            selected, curses_menu_callback);
        if (*selected == CURSES_MENU_CANCELLED)
            return;

        switch (selected[0]) {
            /* move this rule */
        case 1:
            snprintf(query, ARRAY_SIZE(query), "New rule position: (1 - %d), currently: %d",
                    ar->num_rules, ruleno + 1);
            newpos = ar->num_rules;
            curses_getline(query, &newpos, rule_position_callback);
            newpos--;
            if (newpos == ruleno || newpos < 0)
                break;

            tmprule = ar->rules[ruleno];
            /* shift the rules around */
            if (newpos > ruleno) {
                for (i = ruleno; i < newpos; i++)
                    ar->rules[i] = ar->rules[i + 1];
            } else {
                for (i = ruleno; i > newpos; i--)
                    ar->rules[i] = ar->rules[i - 1];
            }
            ar->rules[newpos] = tmprule;
            return;

            /* edit the pattern */
        case 2:
            snprintf(query, ARRAY_SIZE(query), "New name pattern (empty matches everything):");
            curses_getline(query, r->pattern, rule_pattern_callback);
            break;

            /* edit object class match */
        case 3:
            r->oclass = get_autopickup_oclass(desc, r->oclass);
            break;

            /* edit beatitude match */
        case 4:
            r->buc = get_autopickup_buc(r->buc);
            break;

            /* toggle action */
        case 5:
            if (r->action == AP_GRAB)
                r->action = AP_LEAVE;
            else
                r->action = AP_GRAB;
            break;

            /* delete */
        case 6:
            for (i = ruleno; i < ar->num_rules - 1; i++)
                ar->rules[i] = ar->rules[i + 1];
            ar->num_rules--;
            allocsize = ar->num_rules;
            if (allocsize < 1)
                allocsize = 1;
            ar->rules =
                realloc(ar->rules,
                        allocsize * sizeof (struct nh_autopickup_rule));
            return;
        }

    } while (1);
}


static void
show_autopickup_menu(struct nh_option_desc *opt)
{
    struct nh_menulist menu;
    int i, j, parts, selected[1], id;
    struct nh_autopickup_rule *r;
    char buf[BUFSZ];
    struct nh_autopickup_rule *rule;
    union nh_optvalue value;

    /* clone autopickup rules */
    value.ar = malloc(sizeof (struct nh_autopickup_rules));
    value.ar->num_rules = 0;
    value.ar->rules = NULL;
    if (opt->value.ar) {
        int size;
        value.ar->num_rules = opt->value.ar->num_rules;
        size = value.ar->num_rules * sizeof (struct nh_autopickup_rule);
        value.ar->rules = malloc(size);
        memcpy(value.ar->rules, opt->value.ar->rules, size);
    }

    do {
        init_menulist(&menu);

        add_menu_txt(&menu, "Pos\tRule\tAction", MI_HEADING);

        /* list the rules in human-readable form */
        for (i = 0; i < value.ar->num_rules; i++) {
            r = &value.ar->rules[i];
            parts = 0;
            snprintf(buf, ARRAY_SIZE(buf), "%2d.\tIF ", i + 1);

            if (strlen(r->pattern)) {
                parts++;
                sprintf(buf + strlen(buf), "name matches \"%s\"", r->pattern);
            }

            if (r->oclass != OCLASS_ANY) {
                const char *classname = NULL;

                for (j = 0; j < opt->a.numclasses && !classname; j++)
                    if (opt->a.classes[j].id == r->oclass)
                        classname = opt->a.classes[j].caption;

                if (parts++)
                    strcat(buf, " AND ");
                sprintf(buf + strlen(buf), "type is \"%s\"", classname);
            }

            if (r->buc != B_DONT_CARE) {
                if (parts++)
                    strcat(buf, " AND ");
                sprintf(buf + strlen(buf), "beatitude is %s", bucnames[r->buc]);
            }

            if (!parts)
                snprintf(buf, ARRAY_SIZE(buf), "%2d.\teverything", i + 1);

            if (r->action == AP_GRAB)
                sprintf(buf + strlen(buf), ":\t< GRAB");
            else
                sprintf(buf + strlen(buf), ":\t  LEAVE >");

            add_menu_item(&menu, i + 1, buf, 0, 0);
        }

        add_menu_txt(&menu, "", MI_TEXT);
        add_menu_item(&menu, -1, "add a new rule", '!', 0);
        add_menu_item(&menu, -2, "help", '?', 0);

        /* TODO */
        curses_display_menu(&menu, "Autopickup rules:", PICK_ONE,
                            PLHINT_RIGHT, selected, curses_menu_callback);
        if (*selected == CURSES_MENU_CANCELLED)
            break;

        /* add or edit a rule */
        id = selected[0];
        if (id == -1) {
            int size;

            /* create a new rule */
            id = value.ar->num_rules;
            value.ar->num_rules++;
            size = value.ar->num_rules * sizeof (struct nh_autopickup_rule);
            value.ar->rules = realloc(value.ar->rules, size);

            rule = &value.ar->rules[id];
            memset(rule->pattern, 0, sizeof(rule->pattern));
            rule->oclass = OCLASS_ANY;
            rule->buc = B_DONT_CARE;
            rule->action = AP_GRAB;
        } else if (id == -2) {
            autopickup_rules_help();
            continue;
        } else
            id--;

        edit_ap_rule(&opt->a, value.ar, id);
    } while (1);

    curses_set_option(opt->name, value);

    free(value.ar->rules);
    free(value.ar);
}

/*----------------------------------------------------------------------------*/

/* parse a single line from the config file and set the option */
static void
read_config_line(char *line)
{
    char *comment, *delim, *name, *value;
    struct nh_option_desc *option;
    struct nh_option_desc *optlist = NULL;
    union nh_optvalue optval;

    comment = strchr(line, '#');
    if (comment)
        *comment = '\0';
    delim = strchr(line, '=');
    if (!delim)
        return; /* could whine about junk chars in the config, but why bother */

    name = line;
    value = delim + 1;
    *delim-- = '\0';

    /* remove space around name */
    while (isspace(*name))
        name++;
    while (isspace(*delim))
        *delim-- = '\0';

    /* remove spaces around value */
    delim = value;
    while (*delim)
        delim++;
    delim--;

    while (isspace(*value))
        value++;
    while (isspace(*delim))
        *delim-- = '\0';

    /* value may be enclosed with double quotes (") */
    if (*value == '"' && *delim == '"') {
        value++;
        *delim = '\0';
    }

    /* Find an options list with which to parse this option name. */
    option = nhlib_find_option(curses_options, name);
    if (!option) {
        optlist = curses_get_nh_opts();
        if (optlist)
            option = nhlib_find_option(optlist, name);
    }

    if (!option) {
#if 0
        curses_msgwin("Unknown option in config file:", krc_notification);
        curses_msgwin(name, krc_notification);
#endif
        return;
    }

    optval = nhlib_string_to_optvalue(option, value);
    curses_set_option(name, optval);
    if (option->type == OPTTYPE_AUTOPICKUP_RULES) {
        free(optval.ar->rules);
        free(optval.ar);
    }

    if (optlist)
        curses_free_nh_opts(optlist);
}


/* open a config file and separate it into lines for read_config_line() */
static void
read_config_file(const fnchar * filename)
{
    FILE *fp;
    int fsize;
    char *line;

    fp = fopen(filename, "rb");
    if (!fp)
        return;

    /* obtain file size. */
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    rewind(fp);

    if (!fsize) {       /* truncated config file */
        fclose(fp);
        return;
    }

    char buf[fsize + 1];

    if (fread(buf, 1, fsize, fp) < fsize) {
        /* This can only happen if the file shrinks while it's open; let's just
           not read the file at all, because it's probably corrupted */
        curses_msgwin("warning: corrupted configuration file",
                      krc_notification);
        fclose(fp);
        return;
    }
    fclose(fp);

    buf[fsize] = '\0';

    /* each option is expected to have the following format: * name=value\n */
    line = strtok(buf, "\n");
    do {
        read_config_line(line);

        line = strtok(NULL, "\n");
    } while (line);
}


/* determine the correct filename for the config file */
static int
get_config_name(fnchar * buf, nh_bool ui)
{
    buf[0] = '\0';

    /* If running in connection-only mode, we can't get the options until we're
       already logged into the server. */
    if (ui_flags.connection_only && !*ui_flags.username)
        return 0;

#if defined(UNIX)
    char *envval;

    if (!ui) {
        /* check for env override first */
        envval = getenv("NETHACK4OPTIONS");
        if (envval) {
            strncpy(buf, envval, BUFSZ);
            return 1;
        }
    }
#endif

    /* look in regular location */
    if (!get_gamedir(CONFIG_DIR, buf))
        return 0;

#ifdef WIN32
    wchar_t usernamew[BUFSZ];
    int i = 0;

    while (i < BUFSZ - 2 && ui_flags.username[i]) {
        usernamew[i] = ui_flags.username[i];
        i++;
    }
    usernamew[i] = 0;
#else
    /* This is to avoid putting a directive inside the arguments to fnncat,
     * which is a macro. */
    char *usernamew = ui_flags.username;
#endif

    fnncat(buf, ui_flags.connection_only ? usernamew :
                ui ? FN("curses.conf") :
                FN("NetHack4.conf"),
           BUFSZ - fnlen(buf) - 1);
    if (ui_flags.connection_only)
        fnncat(buf, ui ? FN(".curses.rc") : FN(".NetHack4.rc"),
               BUFSZ - fnlen(buf) - 1);

    return 1;
}


nh_bool
read_nh_config(void)
{
    fnchar filename[BUFSZ];

    struct nh_option_desc *opts = nh_get_options();
    if (!opts)
        return FALSE;

    nhlib_free_optlist(nh_options);
    nh_options = opts;

    get_config_name(filename, FALSE);
    read_config_file(filename);

    return TRUE;
}

void
read_ui_config(void)
{
    fnchar uiconfname[BUFSZ];

    /* If running in connection-only mode, we won't know the file to look at
       the first time we call get_config_name. So instead, we put this off
       until after we're already logged in. */
    if (get_config_name(uiconfname, TRUE))
        read_config_file(uiconfname);
}


static FILE *
open_config_file(fnchar * filename)
{
    FILE *fp;

    fp = fopen(filename, "w");
    if (!fp && (errno == ENOTDIR || errno == ENOENT)) {
        fp = fopen(filename, "w");
    }

    if (!fp) {
        fprintf(stderr, "could not open " FN_FMT ": %s", filename,
                strerror(errno));
        return NULL;
    }

    fprintf(fp,
            "# note: this file is rewritten whenever options are\n"
            "# changed and a game is not running\n");

    return fp;
}


static void
write_config_options(FILE * fp, struct nh_option_desc *options)
{
    int i;
    char *optval;

    for (i = 0; options[i].name; i++) {
        optval = nhlib_optvalue_to_string(&options[i]);
        if (options[i].type == OPTTYPE_STRING ||
            options[i].type == OPTTYPE_ENUM)
            fprintf(fp, "%s=\"%s\"\n", options[i].name, optval);
        else
            fprintf(fp, "%s=%s\n", options[i].name, optval);
        free(optval);
    }
}


void
write_nh_config(void)
{
    FILE *fp;
    fnchar filename[BUFSZ] = {0};

    if (get_config_name(filename, FALSE) &&
        (fp = open_config_file(filename))) {
        write_config_options(fp, nh_options);
        fclose(fp);
    } else {
#ifndef WIN32
        char buf[BUFSZ * 2];
        snprintf(buf, sizeof buf, "warning: could not write options (to \"%s\"",
                 filename);
        curses_msgwin(buf, krc_notification);
#endif
    }
}


void
write_ui_config(void)
{
    FILE *fp;
    fnchar filename[BUFSZ] = {0};

    if (get_config_name(filename, TRUE) &&
        (fp = open_config_file(filename))) {
        write_config_options(fp, curses_options);
        fclose(fp);
    } else {
#ifndef WIN32
        char buf[BUFSZ * 2];
        snprintf(buf, sizeof buf, "warning: could not write options (to \"%s\"",
                 filename);
        curses_msgwin(buf, krc_notification);
#endif
    }
}
