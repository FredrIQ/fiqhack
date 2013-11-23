/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2013-11-23 */
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

static struct nh_listitem graphics_list[] = {
    {ASCII_GRAPHICS, "Text (ASCII only)"},
    {UNICODE_GRAPHICS, "Text (Unicode)"},
    {TILESET_16, "Tiles (16x16)"},
/*    {TILESET_32, "Tiles (32x32)"},
      {TILESET_3D, "Tiles (3D effect)"}, */
};
static struct nh_enum_option graphics_spec =
    { graphics_list, listlen(graphics_list) };

static struct nh_listitem optstyle_list[] = {
    {OPTSTYLE_DESC, "description only"},
    {OPTSTYLE_NAME, "name only"},
    {OPTSTYLE_FULL, "name + description"}
};
static struct nh_enum_option optstyle_spec =
    { optstyle_list, listlen(optstyle_list) };


static const char *const bucnames[] =
    { "unknown", "blessed", "uncursed", "cursed", "all" };

struct nh_option_desc curses_options[] = {
    {"name", "name for new characters (blank = ask)", FALSE, OPTTYPE_STRING,
     {.s = NULL}},
    {"bgbranding", "use background colors to show hidden stairs and traps",
     FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"blink",
     "show multiple symbols for each location by switching between them", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"darkgray", "try to show 'black' as dark gray instead of dark blue", FALSE,
     OPTTYPE_BOOL, {.b = FALSE}},
    {"extmenu", "use a menu for selecting extended commands (#)", FALSE,
     OPTTYPE_BOOL,
     {.b = FALSE}},
    {"floorcolor", "change the color of the floor to show where you walked",
     FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"frame", "draw a frame around the window sections", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"graphics", "characters or tiles to use for the map", FALSE, OPTTYPE_ENUM,
     {.e = UNICODE_GRAPHICS}},
    {"hilite_pet", "use background colors to show monster attitude", FALSE,
     OPTTYPE_BOOL, {.b = TRUE}},
    {"invweight", "show item weights in the inventory", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"keymap", "alter the key to command mapping", FALSE,
     (enum nh_opttype)OPTTYPE_KEYMAP, {0}},
    {"menu_headings", "display style for menu headings", FALSE, OPTTYPE_ENUM,
     {.e = A_REVERSE}},
    {"msgheight", "message window height", FALSE, OPTTYPE_INT, {.i = 8}},
    {"msghistory", "number of messages saved for prevmsg", FALSE, OPTTYPE_INT,
     {.i = 256}},
    {"optstyle", "option menu display style", FALSE, OPTTYPE_ENUM,
     {.e = OPTSTYLE_FULL}},
    {"scores_own", "show all your own scores in the list", FALSE, OPTTYPE_BOOL,
     {.b = FALSE}},
    {"scores_top", "how many top scores to show", FALSE, OPTTYPE_INT, {.i = 3}},
    {"scores_around", "the number of scores shown around your score", FALSE,
     OPTTYPE_INT, {.i = 2}},
    {"showexp", "show experience points", FALSE, OPTTYPE_BOOL, {.b = TRUE}},
    {"showscore", "show your score in the status line", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"sidebar", "draw the inventory sidebar", FALSE, OPTTYPE_BOOL, {.b = TRUE}},
    {"standout", "use standout for --More--", FALSE, OPTTYPE_BOOL,
     {.b = FALSE}},
    {"status3", "3 line status display", FALSE, OPTTYPE_BOOL, {.b = TRUE}},
    {"time", "display elapsed game time, in moves", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},
    {"use_inverse", "use inverse video for some things", FALSE, OPTTYPE_BOOL,
     {.b = TRUE}},
#if defined(PDCURSES) && defined(WIN32)
    {"win_width", "window width", FALSE, OPTTYPE_INT, {.i = 130}},
    {"win_height", "window height", FALSE, OPTTYPE_INT, {.i = 40}},
#endif
    {NULL, NULL, FALSE, OPTTYPE_BOOL, {NULL}}
};

struct nhlib_boolopt_map boolopt_map[] = {
    {"bgbranding", &settings.bgbranding},
    {"blink", &settings.blink},
    {"darkgray", &settings.darkgray},
    {"extmenu", &settings.extmenu},
    {"floorcolor", &settings.floorcolor},
    {"frame", &settings.frame},
    {"hilite_pet", &settings.hilite_pet},
    {"invweight", &settings.invweight},
    {"scores_own", &settings.end_own},
    {"showexp", &settings.showexp},
    {"showscore", &settings.showscore},
    {"sidebar", &settings.sidebar},
    {"standout", &settings.standout},
    {"status3", &settings.status3},
    {"time", &settings.time},
    {"use_inverse", &settings.use_inverse},
    {NULL, NULL}
};


nh_bool
curses_set_option(const char *name, union nh_optvalue value, nh_bool isstring)
{
    struct nh_option_desc *option = nhlib_find_option(curses_options, name);
    if (!option)
        return nh_set_option(name, value, isstring);

    if ((int)option->type == OPTTYPE_KEYMAP)
        /* FIXME: impossible() */
        return FALSE;

    if (isstring)
        value = nhlib_string_to_optvalue(option, value.s);

    if (!nhlib_option_value_ok(option, value))
        return FALSE;

    nhlib_copy_option_value(option, value);

    /* We may have allocated a new copy of the autopickup rules. */
    if (isstring && option->type == OPTTYPE_AUTOPICKUP_RULES) {
        free(value.ar->rules);
        free(value.ar);
    }

    if (option->type == OPTTYPE_BOOL) {
        nh_bool *var = nhlib_find_boolopt(boolopt_map, option->name);
        if (!var)
            /* FIXME: impossible() */
            return FALSE;

        *var = value.b;
    }

    if (!strcmp(option->name, "frame") || !strcmp(option->name, "status3") ||
        !strcmp(option->name, "sidebar")) {
        rebuild_ui();
        return TRUE;
    } else if (!strcmp(option->name, "showexp") ||
               !strcmp(option->name, "showscore") ||
               !strcmp(option->name, "time")) {
        curses_update_status(NULL);
    } else if (!strcmp(option->name, "darkgray")) {
        set_darkgray();
        draw_map(player.x, player.y);
    } else if (!strcmp(option->name, "menu_headings")) {
        settings.menu_headings = option->value.e;
    } else if (!strcmp(option->name, "graphics")) {
        settings.graphics = option->value.e;
        switch_graphics(option->value.e);
        if (ui_flags.ingame) {
            rebuild_ui(); /* sets up tiles/ASCII mode */
            draw_map(player.x, player.y);
            redraw_game_windows();
        }
    } else if (!strcmp(option->name, "scores_top")) {
        settings.end_top = option->value.i;
    } else if (!strcmp(option->name, "scores_around")) {
        settings.end_around = option->value.i;
    } else if (!strcmp(option->name, "optstyle")) {
        settings.optstyle = option->value.e;
    } else if (!strcmp(option->name, "msgheight")) {
        settings.msgheight = option->value.i;
        rebuild_ui();
    } else if (!strcmp(option->name, "msghistory")) {
        settings.msghistory = option->value.i;
        alloc_hist_array();
    }
#if defined(PDCURSES) && defined(WIN32)
    else if (!strcmp(option->name, "win_width")) {
        settings.win_width = option->value.i;
        resize_term(settings.win_height, settings.win_width);
        handle_resize();
    } else if (!strcmp(option->name, "win_height")) {
        settings.win_height = option->value.i;
        resize_term(settings.win_height, settings.win_width);
        handle_resize();
    }
#endif
    else if (!strcmp(option->name, "name")) {
        if (option->value.s)
            strcpy(settings.plname, option->value.s);
        else
            settings.plname[0] = '\0';
    } else
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

    find_option("name")->s.maxlen = PL_NSIZ;
    find_option("menu_headings")->e = menu_headings_spec;
    find_option("msgheight")->i.min = 1;
    find_option("msgheight")->i.max = 40;
    find_option("msghistory")->i.min = 20;      /* arbitrary min/max values */
    find_option("msghistory")->i.max = 20000;
    find_option("graphics")->e = graphics_spec;
    find_option("optstyle")->e = optstyle_spec;
    find_option("scores_top")->i.max = 10000;
    find_option("scores_around")->i.max = 100;
#if defined(PDCURSES) && defined(WIN32)
    find_option("win_width")->i.min = COLNO;    /* can never be narrower than
                                                   COLNO */
    find_option("win_width")->i.max = 100 + COLNO;      /* 100 chars wide
                                                           sidebar already
                                                           looks pretty silly */
    find_option("win_height")->i.min = ROWNO + 3;
    find_option("win_height")->i.max = 70;      /* ROWNO + max msgheight +
                                                   extra for status and frame */
#endif
    /* set up option defaults; this is necessary for options that are not
       specified in the config file */
    for (i = 0; curses_options[i].name; i++)
        curses_set_option(curses_options[i].name, curses_options[i].value, FALSE);

    read_ui_config();
}


static const char *
get_display_string(struct nh_option_desc *option)
{
    switch ((int)option->type) {
    default:
    case OPTTYPE_BOOL:
    case OPTTYPE_ENUM:
    case OPTTYPE_INT:
    case OPTTYPE_STRING:
        return nh_get_option_string(option);

    case OPTTYPE_AUTOPICKUP_RULES:
    case OPTTYPE_KEYMAP:
        return "submenu";
    }
}


static void
print_option_string(struct nh_option_desc *option, char *buf)
{
    char fmt[16];
    const char *opttxt;
    const char *valstr = get_display_string(option);

    switch (settings.optstyle) {
    case OPTSTYLE_DESC:
        opttxt = option->helptxt;
        if (!opttxt || strlen(opttxt) < 2)
            opttxt = option->name;

        sprintf(fmt, "%%.%ds\t[%%s]", COLS - 21);
        snprintf(buf, BUFSZ, fmt, opttxt, valstr);
        break;

    case OPTSTYLE_NAME:
        sprintf(fmt, "%%.%ds\t[%%s]", COLS - 21);
        snprintf(buf, BUFSZ, fmt, option->name, valstr);
        break;

    default:
    case OPTSTYLE_FULL:
        sprintf(fmt, "%%s\t[%%s]\t%%.%ds", COLS - 42);
        snprintf(buf, BUFSZ, fmt, option->name, valstr, option->helptxt);
        break;
    }
}


/* add a list of options to the given selection menu */
static int
menu_add_options(struct nh_menuitem **items, int *size, int *icount, int listid,
                 struct nh_option_desc *options, nh_bool birth_only,
                 nh_bool read_only)
{
    int i, id;
    char optbuf[256];

    for (i = 0; options[i].name; i++) {
        id = (listid << 10) | i;
        if (options[i].birth_option != birth_only)
            continue;

        print_option_string(&options[i], optbuf);
        if (read_only)
            add_menu_txt(*items, *size, *icount, optbuf, MI_TEXT);
        else
            add_menu_item(*items, *size, *icount, id, optbuf, 0, 0);
    }

    /* add an empty line */
    add_menu_txt(*items, *size, *icount, "", MI_TEXT);

    return i;
}


/* display a selecton menu for boolean options */
static void
select_boolean_value(union nh_optvalue *value, struct nh_option_desc *option)
{
    struct nh_menuitem *items;
    int icount, size;
    int n, pick_list[2];

    icount = 0;
    size = 4;
    items = malloc(sizeof (struct nh_menuitem) * size);

    add_menu_txt(items, size, icount, option->helptxt, MI_TEXT);
    add_menu_txt(items, size, icount, "", MI_TEXT);
    add_menu_item(items, size, icount, 1,
                  option->value.b ? "true (set)" : "true", 't', 0);
    add_menu_item(items, size, icount, 2,
                  option->value.b ? "false" : "false (set)", 'f', 0);

    n = curses_display_menu(items, icount, option->name, PICK_ONE, PLHINT_RIGHT,
                            pick_list);
    free(items);

    value->b = option->value.b; /* in case of ESC */
    if (n == 1)
        value->b = pick_list[0] == 1;
}


/* display a selection menu for enum options */
static void
select_enum_value(union nh_optvalue *value, struct nh_option_desc *option)
{
    struct nh_menuitem *items;
    int icount, size;
    int i, n, selectidx, *pick_list;

    icount = 0;
    size = 10;
    items = malloc(sizeof (struct nh_menuitem) * size);

    add_menu_txt(items, size, icount, option->helptxt, MI_TEXT);
    add_menu_txt(items, size, icount, "", MI_TEXT);

    for (i = 0; i < option->e.numchoices; i++) {
        char capbuf[QBUFSZ];
        char *cap;

        if (option->value.e == option->e.choices[i].id) {
            snprintf(capbuf, QBUFSZ, "%s (set)", option->e.choices[i].caption);
            cap = capbuf;
        } else {
            cap = option->e.choices[i].caption;
        }
        /* don't use the choice ids directly, 0 is a valid value for those */
        add_menu_item(items, size, icount, i + 1, cap, 0, 0);
    }

    pick_list = malloc(sizeof (int) * icount);
    n = curses_display_menu(items, icount, option->name, PICK_ONE, PLHINT_RIGHT,
                            pick_list);
    free(items);

    value->e = option->value.e; /* in case of ESC */
    if (n == 1) {
        selectidx = pick_list[0] - 1;
        value->e = option->e.choices[selectidx].id;
    }
    free(pick_list);
}


/* get a new value of the appropriate type for the given option */
static nh_bool
get_option_value(struct win_menu *mdat, int idx)
{
    char buf[BUFSZ], query[BUFSZ];
    union nh_optvalue value;
    struct nh_option_desc *option, *optlist;
    int listid = mdat->items[idx].id >> 10;
    int id = mdat->items[idx].id & 0x1ff;
    char strbuf[BUFSZ];
    int prev_optstyle = settings.optstyle;

    switch (listid) {
    case BIRTH_OPTS:
    case GAME_OPTS:
        optlist = nh_get_options();
        break;
    case UI_OPTS:
        optlist = curses_options;
        break;

    default:
        return FALSE;
    }

    option = &optlist[id];
    value.s = strbuf;

    switch ((int)option->type) {
    case OPTTYPE_BOOL:
        select_boolean_value(&value, option);
        break;

    case OPTTYPE_INT:
        sprintf(query, "New value for %s (number from %d to %d)", option->name,
                option->i.min, option->i.max);
        sprintf(buf, "%d", value.i);
        curses_getline(query, buf);
        if (buf[0] == '\033')
            return FALSE;
        sscanf(buf, "%d", &value.i);
        break;

    case OPTTYPE_ENUM:
        select_enum_value(&value, option);
        break;

    case OPTTYPE_STRING:
        sprintf(query, "New value for %s (text)", option->name);
        curses_getline(query, value.s);
        if (value.s[0] == '\033')
            return FALSE;
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        show_autopickup_menu(option);
        return FALSE;

    case OPTTYPE_KEYMAP:
        show_keymap_menu(FALSE);
        return FALSE;

    default:
        return FALSE;
    }

    if (!curses_set_option(option->name, value, FALSE)) {
        sprintf(strbuf, "new value for %s rejected", option->name);
        curses_msgwin(strbuf);
    } else
        print_option_string(option, mdat->items[idx].caption);

    /* special case: directly redo option menu appearance */
    if (settings.optstyle != prev_optstyle)
        return TRUE;

    return FALSE;
}


/* display the option dialog */
void
display_options(nh_bool change_birth_opt)
{
    struct nh_menuitem *items;
    int icount, size;
    struct nh_option_desc *options = nh_get_options();
    int n;

    size = 10;
    items = malloc(sizeof (struct nh_menuitem) * size);

    do {
        icount = 0;
        if (!change_birth_opt) {
            /* add general game options */
            add_menu_txt(items, size, icount, "Game options:", MI_HEADING);
            menu_add_options(&items, &size, &icount, GAME_OPTS, options,
                             FALSE, FALSE);

            /* add or display birth options */
            add_menu_txt(items, size, icount, "Birth options for this game:",
                         MI_HEADING);
            menu_add_options(&items, &size, &icount, BIRTH_OPTS, options,
                             TRUE, TRUE);
        } else {
            /* add or display birth options */
            add_menu_txt(items, size, icount, "Birth options:", MI_HEADING);
            menu_add_options(&items, &size, &icount, BIRTH_OPTS, options,
                             TRUE, FALSE);

            add_menu_txt(items, size, icount, "Game options:", MI_HEADING);
            menu_add_options(&items, &size, &icount, GAME_OPTS, options,
                             FALSE, FALSE);
        }

        /* add UI specific options */
        add_menu_txt(items, size, icount, "Interface options:", MI_HEADING);
        menu_add_options(&items, &size, &icount, UI_OPTS, curses_options,
                         FALSE, FALSE);

        n = curses_display_menu_core(items, icount, "Set what options?",
                                     PICK_ONE, NULL, 0, 0, -1, -1,
                                     get_option_value);
    } while (n > 0);
    free(items);

    write_config();
}


void
print_options(void)
{
    struct nh_menuitem *items;
    int i, icount, size;
    char buf[BUFSZ];
    struct nh_option_desc *options = nh_get_options();

    icount = 0;
    size = 10;
    items = malloc(sizeof (struct nh_menuitem) * size);

    add_menu_txt(items, size, icount, "Birth options:", MI_HEADING);
    for (i = 0; options[i].name; i++) {
        if (!options[i].birth_option)
            continue;
        snprintf(buf, BUFSZ, "%s\t%s", options[i].name, options[i].helptxt);
        add_menu_txt(items, size, icount, buf, MI_TEXT);
    }
    add_menu_txt(items, size, icount, "", MI_TEXT);

    add_menu_txt(items, size, icount, "Game options:", MI_HEADING);
    for (i = 0; options[i].name; i++) {
        if (options[i].birth_option)
            continue;
        snprintf(buf, BUFSZ, "%s\t%s", options[i].name, options[i].helptxt);
        add_menu_txt(items, size, icount, buf, MI_TEXT);
    }
    add_menu_txt(items, size, icount, "", MI_TEXT);

    /* add UI specific options */
    add_menu_txt(items, size, icount, "Interface options:", MI_HEADING);
    for (i = 0; curses_options[i].name; i++) {
        snprintf(buf, BUFSZ, "%s\t%s", curses_options[i].name,
                 curses_options[i].helptxt);
        add_menu_txt(items, size, icount, buf, MI_TEXT);
    }

    curses_display_menu(items, icount, "Available options:", PICK_NONE,
                        PLHINT_ANYWHERE, NULL);
    free(items);
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
        {0, MI_TEXT,
         "necessary; it's purpose here is to make the example more interesting.)"},
        {0, MI_TEXT, ""},
        {0, MI_TEXT,
         "A dagger will not match any of these rules and so it won't"},
        {0, MI_TEXT, "be picked up either."},
        {0, MI_TEXT, ""},
        {0, MI_TEXT,
         "You may select any existing rule to edit it, change its position"},
        {0, MI_TEXT, "in the list, or delete it."},
    };
    curses_display_menu(items, listlen(items), "Autopickup rules help:",
                        PICK_NONE, PLHINT_LEFT, NULL);
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
    int n, selected[1];

    n = curses_display_menu(items, 5, "Beatitude match:", PICK_ONE,
                            PLHINT_RIGHT, selected);
    if (n <= 0)
        return cur;
    return selected[0] - 1;
}


static int
get_autopickup_oclass(struct nh_autopick_option *desc, int cur)
{
    int i, n, size, icount, selected[1];
    struct nh_menuitem *items;

    size = desc->numclasses;
    items = malloc(sizeof (struct nh_menuitem) * size);
    icount = 0;

    for (i = 0; i < desc->numclasses; i++)
        add_menu_item(items, size, icount, desc->classes[i].id,
                      desc->classes[i].caption, (char)desc->classes[i].id, 0);

    n = curses_display_menu(items, icount, "Object class match:", PICK_ONE,
                            PLHINT_RIGHT, selected);
    free(items);
    if (n <= 0)
        return cur;
    return selected[0];
}


static void
edit_ap_rule(struct nh_autopick_option *desc, struct nh_autopickup_rules *ar,
             int ruleno)
{
    struct nh_autopickup_rule *r = &ar->rules[ruleno];
    struct nh_autopickup_rule tmprule;
    struct nh_menuitem *items;
    int i, icount, size = 7, n, selected[1], newpos;
    char query[BUFSZ], buf[BUFSZ], *classname;

    items = malloc(sizeof (struct nh_menuitem) * size);

    do {
        icount = 0;
        sprintf(buf, "rule position:\t[%d]", ruleno + 1);
        add_menu_item(items, size, icount, 1, buf, 0, 0);

        sprintf(buf, "name pattern:\t[%s]", r->pattern);
        add_menu_item(items, size, icount, 2, buf, 0, 0);

        classname = NULL;
        for (i = 0; i < desc->numclasses && !classname; i++)
            if (desc->classes[i].id == r->oclass)
                classname = desc->classes[i].caption;
        sprintf(buf, "object type:\t[%s]", classname);
        add_menu_item(items, size, icount, 3, buf, 0, 0);

        sprintf(buf, "beatitude:\t[%s]", bucnames[r->buc]);
        add_menu_item(items, size, icount, 4, buf, 0, 0);

        sprintf(buf, "action:\t[%s]", r->action == AP_GRAB ? "GRAB" : "LEAVE");
        add_menu_item(items, size, icount, 5, buf, 0, 0);
        add_menu_txt(items, size, icount, "", MI_TEXT);
        add_menu_item(items, size, icount, 6, "delete this rule", 'x', 0);

        n = curses_display_menu(items, icount, "Edit rule:", PICK_ONE,
                                PLHINT_RIGHT, selected);
        if (n <= 0)
            break;

        switch (selected[0]) {
            /* move this rule */
        case 1:
            sprintf(query, "New rule position: (1 - %d), currently: %d",
                    ar->num_rules, ruleno + 1);
            buf[0] = '\0';
            curses_getline(query, buf);
            if (!*buf || *buf == '\033')
                break;
            newpos = atoi(buf);
            if (newpos <= 0 || newpos > ar->num_rules) {
                curses_msgwin("Invalid rule position.");
                break;
            }
            newpos--;
            if (newpos == ruleno)
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
            goto out;

            /* edit the pattern */
        case 2:
            sprintf(query, "New name pattern (empty matches everything):");
            buf[0] = '\0';
            curses_getline(query, buf);
            if (*buf != '\033')
                strncpy(r->pattern, buf, sizeof (r->pattern));
            r->pattern[sizeof (r->pattern) - 1] = '\0';
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
            ar->rules =
                realloc(ar->rules,
                        ar->num_rules * sizeof (struct nh_autopickup_rule));
            goto out;   /* break just beaks the switch .. doh */
        }

    } while (n > 0);
out:
    free(items);
}


static void
show_autopickup_menu(struct nh_option_desc *opt)
{
    struct nh_menuitem *items;
    int i, j, n, icount, size, menusize, parts, selected[1], id;
    struct nh_autopickup_rule *r;
    char buf[BUFSZ];
    struct nh_autopickup_rule *rule;
    union nh_optvalue value;

    /* clone autopickup rules */
    value.ar = malloc(sizeof (struct nh_autopickup_rules));
    value.ar->num_rules = 0;
    value.ar->rules = NULL;
    if (opt->value.ar) {
        value.ar->num_rules = opt->value.ar->num_rules;
        size = value.ar->num_rules * sizeof (struct nh_autopickup_rule);
        value.ar->rules = malloc(size);
        memcpy(value.ar->rules, opt->value.ar->rules, size);
    }

    menusize = value.ar->num_rules + 4;
    items = malloc(sizeof (struct nh_menuitem) * menusize);

    do {
        icount = 0;

        add_menu_txt(items, menusize, icount, "Pos\tRule\tAction", MI_HEADING);

        /* list the rules in human-readable form */
        for (i = 0; i < value.ar->num_rules; i++) {
            r = &value.ar->rules[i];
            parts = 0;
            sprintf(buf, "%2d.\tIF ", i + 1);

            if (strlen(r->pattern)) {
                parts++;
                sprintf(buf + strlen(buf), "name matches \"%s\"", r->pattern);
            }

            if (r->oclass != OCLASS_ANY) {
                char *classname = NULL;

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
                sprintf(buf, "%2d.\teverything", i + 1);

            if (r->action == AP_GRAB)
                sprintf(buf + strlen(buf), ":\t< GRAB");
            else
                sprintf(buf + strlen(buf), ":\t  LEAVE >");

            add_menu_item(items, menusize, icount, i + 1, buf, 0, 0);
        }

        add_menu_txt(items, menusize, icount, "", MI_TEXT);
        add_menu_item(items, menusize, icount, -1, "add a new rule", '!', 0);
        add_menu_item(items, menusize, icount, -2, "help", '?', 0);

        /* TODO */
        n = curses_display_menu(items, icount, "Autopickup rules:", PICK_ONE,
                                PLHINT_RIGHT, selected);
        if (n <= 0)
            break;

        /* add or edit a rule */
        id = selected[0];
        if (id == -1) {
            /* create a new rule */
            id = value.ar->num_rules;
            value.ar->num_rules++;
            size = value.ar->num_rules * sizeof (struct nh_autopickup_rule);
            value.ar->rules = realloc(value.ar->rules, size);

            rule = &value.ar->rules[id];
            rule->pattern[0] = '\0';
            rule->oclass = OCLASS_ANY;
            rule->buc = B_DONT_CARE;
            rule->action = AP_GRAB;
        } else if (id == -2) {
            autopickup_rules_help();
            continue;
        } else
            id--;

        edit_ap_rule(&opt->a, value.ar, id);
    } while (n > 0);

    curses_set_option(opt->name, value, FALSE);

    free(value.ar->rules);
    free(value.ar);
    free(items);
}

/*----------------------------------------------------------------------------*/

/* parse a single line from the config file and set the option */
static void
read_config_line(char *line)
{
    char *comment, *delim, *name, *value;
    union nh_optvalue optval;

    comment = strchr(line, '#');
    if (comment)
        comment = '\0';
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

    optval.s = value;
    curses_set_option(name, optval, TRUE);
}


/* open a config file and separate it into lines for read_config_line() */
static void
read_config_file(const fnchar * filename)
{
    FILE *fp;
    int fsize;
    char *buf, *line;

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

    buf = malloc(fsize + 1);
    if (!buf) {
        fclose(fp);
        return;
    }

    fread(buf, fsize, 1, fp);
    fclose(fp);

    buf[fsize] = '\0';

    /* each option is expected to have the following format: * name=value\n */
    line = strtok(buf, "\n");
    do {
        read_config_line(line);

        line = strtok(NULL, "\n");
    } while (line);

    free(buf);
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
#endif

    fnncat(buf, ui_flags.connection_only ?
#ifdef WIN32
           usernamew :
#else
           ui_flags.username:
#endif
           ui ? FN("curses.conf") : FN("NetHack4.conf"), BUFSZ);
    if (ui_flags.connection_only)
        fnncat(buf, FN(".rc"), BUFSZ);

    return 1;
}


void
read_nh_config(void)
{
    fnchar filename[BUFSZ];

    if (!ui_flags.connection_only) {
        get_config_name(filename, FALSE);
        read_config_file(filename);
    }
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
            "# note: this file is rewritten whenever options are changed ingame\n");

    return fp;
}


static void
write_config_options(FILE * fp, struct nh_option_desc *options)
{
    int i;
    const char *optval;

    for (i = 0; options[i].name; i++) {
        optval = nh_get_option_string(&options[i]);
        if (options[i].type == OPTTYPE_STRING ||
            options[i].type == OPTTYPE_ENUM)
            fprintf(fp, "%s=\"%s\"\n", options[i].name, optval);
        else
            fprintf(fp, "%s=%s\n", options[i].name, optval);
    }
}


void
write_config(void)
{
    FILE *fp;
    fnchar filename[BUFSZ];
    fnchar uiconfname[BUFSZ];

    if (!ui_flags.connection_only)
        get_config_name(filename, FALSE);
    if (!get_config_name(uiconfname, TRUE))
        return;

    if (!ui_flags.connection_only && !game_is_running) {
        fp = open_config_file(filename);
        if (fp) {
            write_config_options(fp, nh_get_options());
            fclose(fp);
        }
    }

    fp = open_config_file(uiconfname);
    if (fp) {
        write_config_options(fp, curses_options);
        fclose(fp);
    }
}
