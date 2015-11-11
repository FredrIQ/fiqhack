/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include "common_options.h"

#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#define NO_KEYMAP find_command("(nothing)")

enum internal_commands {
    /* implicitly include enum nh_direction */
    UICMD_OPTIONS = DIR_SELF + 1,
    UICMD_EXTCMD,
    UICMD_HELP,
    UICMD_MAINMENU,
    UICMD_DETACH,
    UICMD_STOP,
    UICMD_PREVMSG,
    UICMD_WHATDOES,
    UICMD_TOGGLEPICKUP,
    UICMD_REPEATCOUNT,
    UICMD_NOTHING,
    UICMD_SERVERCANCEL,
};

enum select_cmd {
    SELECT_CMD_ALL,        /* Select all commands */
    SELECT_CMD_DEBUG,      /* Select commands with CMD_DEBUG flags */
    SELECT_CMD_DIRECTION,  /* Select direction commands */
    SELECT_CMD_UI,         /* Select UI commands  */
    SELECT_CMD_GAME        /* Select commands that are neither debug, direction or UI */
};

enum order_cmd {
    ORDER_CMD_NONE,   /* do not order the commands */
    ORDER_CMD_NAME    /* sort commands by names */
};

static_assert(UICMD_SERVERCANCEL < CMD_UI, "CMD_UI too small");
static_assert(UICMD_SERVERCANCEL < CMD_INTERNAL, "CMD_INTERNAL too small");

#define RESET_BINDINGS_ID (-10000)
#define KEYMAP_ACTIONS_ID (-1000)

/* Special actions, found in the Keymap menus */
enum keymap_action {
    KEYMAP_ACTION_BEGIN = -10000 ,   /* === Dummy action! Should be first === */

    /* Unbinding */
    KEYMAP_ACTION_RESET_ALL,         /* Reset all keys to defaults */

    /* Mass rebinds */
    KEYMAP_ACTION_MASSREBIND_VI,     /* Manage the vi directions */
    KEYMAP_ACTION_MASSREBIND_DIGITS, /* Manage the digits */
    KEYMAP_ACTION_MASSREBIND_MODIF,  /* Manage Ctrl-/Shift-direction */

    /* Individual commands */
    KEYMAP_ACTION_ALL_SUBMENU,       /* Manage each individual command */
    KEYMAP_ACTION_DEBUG_SUBMENU,     /* Manage debug commands */
    KEYMAP_ACTION_UI_SUBMENU,        /* Manage UI commands */
    KEYMAP_ACTION_GAME_SUBMENU,      /* Manage game commands */
    KEYMAP_ACTION_END,               /* === Dummy action! Should be last === */
};


/* Try to add the Ctrl modifier to the specified 'key' code.

   Since the behavior of the Ctrl modifier is system and keyboard dependent, it
   is not always possible to calculate exactly what will happen.

   A result of 0 indicates that the input key is known to be problematic with
   Ctrl. A result equal to 'key' indicates that the key already has the Ctrl
   modifier. */
static int
guess_ctrl_key(int key)
{
    if (key == 0)
        return 0;
    else if ('a' <= key && key <= 'z')
        return key - 'a' + 1;   /* 'a'..'z' into 1..26 */
    else if ('A' <= key && key <= 'Z')
        return key - 'A' + 1;   /* Ctrl-Shift-letter looks like Ctrl-letter */
    else if (1 <= key && key <= 26)
        return key;             /* Letter, already has Ctrl modifier */
    else if (key < 256)
        return 0;               /* KEY_CTRL doesn't work in the ASCII range */
    else
        return KEY_CTRL | key;
}

/* Try to add the Shift modifier to the specified 'key' code.

   Since the behavior of the SHIFT modifier is system and keyboard dependent,
   it is not always possible to calculate what will happen.

   A result of 0 indicates that the input key is known to be problematic with
   Shift. A result equal to 'key' indicates that the key already has the Shift
   modifier. */
static int
guess_shift_key(int key)
{
    if (key == 0)
        return 0;
    else if ('a' <= key && key <= 'z')
        return key - 'a' + 'A'; /* from 'a'..'z' to 'A'..'Z' */
    else if ('A' <= key && key <= 'Z')
        return key;
    else if (key < 256)
        return 0;               /* KEY_SHIFT doesn't work in the ASCII range */
    else
        return KEY_SHIFT | key;
}

/* The order is important: adding 4 should be a 45 degree rotation clockwise */
static const char *const all_directions[8] = {
    "north",
    "south",
    "west",
    "east",
    "north_east",
    "south_west",
    "north_west",
    "south_east",
};

/* Provide the 8 run_ commands in the same order as in all_directions */
static const char *const all_run_directions[8] = {
    "run_north",
    "run_south",
    "run_west",
    "run_east",
    "run_north_east",
    "run_south_west",
    "run_north_west",
    "run_south_east",
};

/* Provide the 8 go_ commands in the same order as in all_directions */
static const char *const all_go_directions[8] = {
    "go_north",
    "go_south",
    "go_west",
    "go_east",
    "go_north_east",
    "go_south_west",
    "go_north_west",
    "go_south_east",
};

#ifndef Ctrl
# define Ctrl(c)        (0x1f & (c))
#endif

#define DIRCMD          (1U << 28)
#define DIRCMD_RUN      (1U << 29)
#define DIRCMD_GO       (1U << 30)

static struct nh_cmd_desc builtin_commands[] = {
    {"east", "move, fight or interact to the east", 'l', 0,
     CMD_UI | DIRCMD | DIR_E},
    {"north", "move, fight or interact to the north", 'k', 0,
     CMD_UI | DIRCMD | DIR_N},
    {"north_east", "move, fight or interact to the northeast", 'u', 0,
     CMD_UI | DIRCMD | DIR_NE},
    {"north_west", "move, fight or interact to the northwest", 'y', 0,
     CMD_UI | DIRCMD | DIR_NW},
    {"south", "move, fight or interact to the south", 'j', 0,
     CMD_UI | DIRCMD | DIR_S},
    {"south_east", "move, fight or interact to the southeast", 'n', 0,
     CMD_UI | DIRCMD | DIR_SE},
    {"south_west", "move, fight or interact to the southwest", 'b', 0,
     CMD_UI | DIRCMD | DIR_SW},
    {"west", "move, fight or interact to the west", 'h', 0,
     CMD_UI | DIRCMD | DIR_W},
    {"up", "climb stairs or ladders", '<', 0, CMD_UI | DIRCMD | DIR_UP},
    {"down", "go down stairs or ladders or jump into holes", '>', 0,
     CMD_UI | DIRCMD | DIR_DOWN},

    {"run_east", "go east until you run into something", 'L', 0,
     CMD_UI | DIRCMD_RUN | DIR_E},
    {"run_north", "go north until you run into something", 'K', 0,
     CMD_UI | DIRCMD_RUN | DIR_N},
    {"run_north_east", "go northeast until you run into something", 'U', 0,
     CMD_UI | DIRCMD_RUN | DIR_NE},
    {"run_north_west", "go northwest until you run into something", 'Y', 0,
     CMD_UI | DIRCMD_RUN | DIR_NW},
    {"run_south", "go south until you run into something", 'J', 0,
     CMD_UI | DIRCMD_RUN | DIR_S},
    {"run_south_east", "go southeast until you run into something", 'N', 0,
     CMD_UI | DIRCMD_RUN | DIR_SE},
    {"run_south_west", "go southwest until you run into something", 'B', 0,
     CMD_UI | DIRCMD_RUN | DIR_SW},
    {"run_west", "go west until you run into something", 'H', 0,
     CMD_UI | DIRCMD_RUN | DIR_W},

    {"go_east", "run east until something interesting is seen", Ctrl('l'), 0,
     CMD_UI | DIRCMD_GO | DIR_E},
    {"go_north", "run north until something interesting is seen", Ctrl('k'), 0,
     CMD_UI | DIRCMD_GO | DIR_N},
    {"go_north_east", "run northeast until something interesting is seen",
     Ctrl('u'), 0, CMD_UI | DIRCMD_GO | DIR_NE},
    {"go_north_west", "run northwest until something interesting is seen",
     Ctrl('y'), 0, CMD_UI | DIRCMD_GO | DIR_NW},
    {"go_south", "run south until something interesting is seen", Ctrl('j'), 0,
     CMD_UI | DIRCMD_GO | DIR_S},
    {"go_south_east", "run southeast until something interesting is seen",
     Ctrl('n'), 0, CMD_UI | DIRCMD_GO | DIR_SE},
    {"go_south_west", "run southwest until something interesting is seen",
     Ctrl('b'), 0, CMD_UI | DIRCMD_GO | DIR_SW},
    {"go_west", "run west until something interesting is seen", Ctrl('h'), 0,
     CMD_UI | DIRCMD_GO | DIR_W},

    {"extcommand", "perform an extended command", '#', 0,
     CMD_UI | UICMD_EXTCMD},
    {"help", "show the help menu", '?', 0, CMD_UI | UICMD_HELP},
    {"mainmenu", "show the main menu", '!', Ctrl('c'), CMD_UI | UICMD_MAINMENU},
    {"options", "show or change option settings", 'O', 0,
     CMD_UI | UICMD_OPTIONS},
    {"prevmsg", "list previously displayed messages", Ctrl('p'), 0,
     CMD_UI | UICMD_PREVMSG},
    {"save", "save or abandon the game", 'S', 0, CMD_UI | UICMD_DETACH},
    {"stop", "suspend to shell", Ctrl('z'), 0, CMD_UI | UICMD_STOP},
    {"togglepickup", "toggle the autopickup option", '@', 0,
     CMD_UI | UICMD_TOGGLEPICKUP},
    {"whatdoes", "describe what a key does", '&', 0, CMD_UI | UICMD_WHATDOES},

    {"repeatcount", "enter a number of turns to perform a command", 0,
     0, CMD_UI | UICMD_REPEATCOUNT},
    {"(nothing)", "bind keys to this command to suppress \"Bad command\"", 0,
     0, CMD_UI | UICMD_NOTHING},

    {"servercancel", "(internal use only) the server already has a command",
     0, 0, CMD_UI | CMD_INTERNAL | UICMD_SERVERCANCEL},
};

struct nh_cmd_desc *keymap[KEY_MAX + 1];
static struct nh_cmd_desc *unknown_keymap[KEY_MAX + 1];
static struct nh_cmd_desc *commandlist, *unknown_commands;
static int cmdcount, unknown_count, unknown_size;
static struct nh_cmd_arg next_command_arg;

static int current_cmd_key;

int repeats_remaining;

static nh_bool have_next_command = FALSE;
static char next_command_name[32];

static void show_whatdoes(void);
static struct nh_cmd_desc *show_help(void);
static struct nh_cmd_desc *show_mainmenu(nh_bool inside_another_command,
                                         nh_bool include_debug_commands);
static void save_menu(void);
static void instant_replay(void);
static void init_keymap(void);
static void write_keymap(void);
static struct nh_cmd_desc *doextcmd(nh_bool);
static void dostop(void);
static void dotogglepickup(void);

static struct nh_cmd_desc *
find_command(const char *cmdname)
{
    int i, count;

    for (i = 0; i < cmdcount; i++)
        if (!strcmp(commandlist[i].name, cmdname))
            return &commandlist[i];

    count = sizeof (builtin_commands) / sizeof (struct nh_cmd_desc);
    for (i = 0; i < count; i++)
        if (!strcmp(builtin_commands[i].name, cmdname))
            return &builtin_commands[i];

    return NULL;
}

void
handle_internal_cmd(struct nh_cmd_desc **cmd, struct nh_cmd_arg *arg,
                    nh_bool include_debug)
{
    int id = (*cmd)->flags & ~(CMD_UI | DIRCMD | DIRCMD_RUN | DIRCMD_GO);
    nh_bool cancel_yskip = TRUE;

    ui_flags.in_zero_time_command = TRUE;

    switch (id) {
    case DIR_NW:
    case DIR_N:
    case DIR_NE:
    case DIR_E:
    case DIR_W:
    case DIR_SW:
    case DIR_S:
    case DIR_SE:
    case DIR_UP:
    case DIR_DOWN:
        arg->argtype |= CMD_ARG_DIR;
        arg->dir = id;
        if ((*cmd)->flags & DIRCMD)
            *cmd = find_command("move");
        else if ((*cmd)->flags & DIRCMD_RUN)
            *cmd = find_command("run");
        else if ((*cmd)->flags & DIRCMD_GO)
            *cmd = find_command("go");
        break;

    case UICMD_OPTIONS:
        display_options(FALSE);
        draw_map(player.x, player.y);
        *cmd = find_command("interrupt");
        break;

    case UICMD_EXTCMD:
        *cmd = doextcmd(include_debug);
        break;

    case UICMD_HELP:
        arg->argtype = 0;
        *cmd = show_help();
        break;

    case UICMD_MAINMENU:
        arg->argtype = 0;
        *cmd = show_mainmenu(FALSE, include_debug);
        break;

    case UICMD_DETACH:
        save_menu();
        *cmd = NULL;
        break;

    case UICMD_STOP:
        dostop();
        *cmd = NULL;
        break;

    case UICMD_PREVMSG:
        doprev_message();
        cancel_yskip = FALSE;
        *cmd = NULL;
        break;

    case UICMD_WHATDOES:
        show_whatdoes();
        *cmd = NULL;
        break;

    case UICMD_TOGGLEPICKUP:
        dotogglepickup();
        *cmd = find_command("interrupt");
        break;

    case UICMD_NOTHING:
        *cmd = NULL;
        break;
    }
    ui_flags.in_zero_time_command = FALSE;

    /* We cancel the yskip on all commands exept UICMD_PREVMSG. */
    if (cancel_yskip)
        ui_flags.msghistory_yskip = 0;
}

void
get_command(void *callbackarg,
            void (*callback) (const struct nh_cmd_and_arg *, void *),
            nh_bool include_debug)
{
    int key, key2, multi;
    char line[BUFSZ];
    struct nh_cmd_desc *cmd, *cmd2;
    struct nh_cmd_and_arg ncaa;

    int save_repeats = repeats_remaining;
    repeats_remaining = 0;

    ui_flags.in_zero_time_command = FALSE;

    /* inventory item actions may set the next command */
    if (have_next_command) {
        have_next_command = FALSE;
        callback(&(struct nh_cmd_and_arg){next_command_name, next_command_arg},
                 callbackarg);
        return;
    }

    do {
        multi = 0;
        cmd = NULL;
        ncaa.arg.argtype = 0;

        draw_messages_prekey(FALSE);
        key = get_map_key(TRUE, TRUE, krc_get_command);
        /* TODO: If we get a /spurious/ server cancel, we should avoid greying
           messages as a result (problematic because a non-spurious server
           cancel will quite possibly display messages). */
        draw_messages_postkey();

        if (key <= KEY_MAX && keymap[key] == find_command("repeatcount")) {
            do {
                if (key == KEY_BACKSPACE)
                    multi /= 10;
                else if (key >= '0' && key <= '9') {
                    multi = 10 * multi + key - '0';
                    if (multi > 0xffff)
                        multi /= 10;
                }
                snprintf(line, ARRAY_SIZE(line), "Count: %d", multi);
                key = curses_msgwin(line, krc_count);
            } while ((key >= '0' && key <= '9') ||
                     (multi > 0 && key == KEY_BACKSPACE));
        }

        if (key == '\x1b' || key == KEY_ESCAPE) {
            continue;
        }

        if (key == KEY_SIGNAL) {
            cmd = find_command("servercancel");
        } else if (key <= KEY_MAX) {
            cmd = keymap[key];
            current_cmd_key = key;
        } else
            cmd = NULL;

        if (key > KEY_MAX && key < KEY_MAX + 128) {
            /* This range of user-defined keys is used for mouse callbacks from
               the inventory sidebar. */
            item_actions_from_sidebar(key - KEY_MAX);
            if (have_next_command) {
                have_next_command = FALSE;
                callback(&(struct nh_cmd_and_arg) {
                         next_command_name, next_command_arg}, callbackarg);
                return;
            }
            continue;
        }

        if (key >= KEY_MAX + 256) {
            /* This range of user-defined keys is used for clicks on the map. */

            /* For now, these don't do anything, and don't cause a
               draw_messages_postkey() either (safe beacuse they're no-ops). */
            continue;
        }

        if (cmd != NULL) {
            /* handle internal commands. The command handler may alter *cmd,
               and arg (although not all this functionality is currently used) */
            if (cmd->flags & CMD_UI) {
                handle_internal_cmd(&cmd, &ncaa.arg, include_debug);
                if (!cmd)       /* command was fully handled internally */
                    continue;
            }

            /* When we know for certain that the command wasn't a prevmsg
               command, cancel any prevmsg scroll in progress. */
            ui_flags.msghistory_yskip = 0;

            if (multi && cmd->flags & CMD_ARG_LIMIT) {
                ncaa.arg.argtype |= CMD_ARG_LIMIT;
                ncaa.arg.limit = multi;
            } else {
                repeats_remaining = multi;
            }

            if (cmd == find_command("redraw")) {
                /* This needs special handling locally in addition to sending
                   it to the server */
                clear();
                refresh();
                rebuild_ui();
            }

            if (cmd == find_command("repeat")) {
                repeats_remaining = save_repeats;
            }

            /* if the command requres an arg AND the arg isn't set yet (by
               handle_internal_cmd) */
            if (cmd->flags & CMD_ARG_DIR && cmd->flags & CMD_MOVE &&
                !(ncaa.arg.argtype & CMD_ARG_DIR)) {
                key2 = get_map_key(TRUE, FALSE, krc_get_movecmd_direction);

                if (key2 == '\x1b' || key2 == KEY_ESCAPE) /* cancel silently */
                    continue;
                if (key2 == KEY_SIGNAL) {
                    cmd = find_command("servercancel");
                } else if (key <= KEY_MAX) {

                    cmd2 = keymap[key2];
                    if (cmd2 && (cmd2->flags & CMD_UI) &&
                        (cmd2->flags & DIRCMD)) {
                        ncaa.arg.argtype |= CMD_ARG_DIR;
                        ncaa.arg.dir = (enum nh_direction)
                            (cmd2->flags & ~(CMD_UI | DIRCMD));
                    } else
                        cmd = NULL;
                } else
                    cmd = NULL; /* paranoia */
            }
        }

        if (!cmd) {
            snprintf(line, ARRAY_SIZE(line), "Bad command: '%s'.",
                     friendly_keyname(key));
            curses_print_message(msgc_cancelled, line);
        }
    } while (!cmd);

    ui_flags.in_zero_time_command = !!(cmd->flags & CMD_NOTIME);

    wmove(mapwin, player.y, player.x);

    ncaa.cmd = cmd->name;
    callback(&ncaa, callbackarg);
}

void
handle_nested_key(int key)
{
    if (key < 0 || key > KEY_MAX)
        return;

    int save_zero_time = ui_flags.in_zero_time_command;
    ui_flags.in_zero_time_command = TRUE;

    if (keymap[key] == find_command("save"))
        save_menu();
    if (keymap[key] == find_command("mainmenu"))
        show_mainmenu(TRUE, FALSE);

    /* Perhaps we should support various other commands that are either
       entirely client-side, or else zero-time and can be supported via
       dropping into replay mode temporarily. That could easily be confusing,
       though. */

    ui_flags.in_zero_time_command = save_zero_time;
}

void
set_next_command(const char *cmd, struct nh_cmd_arg *arg)
{
    have_next_command = TRUE;
    next_command_arg = *arg;
    strncpy(next_command_name, cmd, sizeof (next_command_name));
}

/* If range is NULL, only accept unshifted directions.

   Otherwise, set *range to 1/4/8 for nothing/go/run modifiers. */
enum nh_direction
key_to_dir(int key, int *range)
{
    struct nh_cmd_desc *cmd;

    if (key <= 0 || key > KEY_MAX)
        return DIR_NONE;

    cmd = keymap[key];

    if (cmd && (!strcmp(cmd->name, "wait") || !strcmp(cmd->name, "search")) &&
        range)
        return DIR_SELF;

    if (!cmd || !(cmd->flags & (DIRCMD | DIRCMD_RUN | DIRCMD_GO)))
        return DIR_NONE;

    if (cmd->flags & (DIRCMD_RUN | DIRCMD_GO) && !range)
        return DIR_NONE;

    if (range)
        *range = 1;
    if (cmd->flags & DIRCMD_GO)
        *range = 4;
    if (cmd->flags & DIRCMD_RUN)
        *range = 8;

    return (enum nh_direction)cmd->flags &
        ~(CMD_UI | DIRCMD | DIRCMD_RUN | DIRCMD_GO);
}

/* here after #? - now list all full-word commands */
static int
doextlist(const char **namelist, const char **desclist, int listlen)
{
    char buf[BUFSZ];
    int i;
    struct nh_menulist menu;

    init_menulist(&menu);

    for (i = 0; i < listlen; i++) {
        snprintf(buf, ARRAY_SIZE(buf), " %s\t- %s.", namelist[i], desclist[i]);
        add_menu_txt(&menu, buf, MI_TEXT);
    }

    curses_display_menu(&menu, "Extended Commands List", PICK_NONE,
                        PLHINT_ANYWHERE, NULL, null_menu_callback);

    return 0;
}

static void
doextcmd_callback(const char *cmdname, void *retval_void)
{
    struct nh_cmd_desc **retval = retval_void;

    if (*cmdname == '\033' || !*cmdname) {
        *retval = NULL; /* break out of the loop */
        return;
    }

    if (!strcmp(cmdname, "?"))  /* help */
        return; /* unchanged *retval = do help */

    *retval = find_command(cmdname);

    /* don't allow ui commands: they wouldn't be handled properly later on */
    if (!*retval || ((*retval)->flags & CMD_UI)) {
        char msg[strlen(cmdname) + 1 + sizeof ": unknown extended command."];

        sprintf(msg, "%s: unknown extended command.", cmdname);
        curses_msgwin(msg, krc_notification);
        *retval = NULL; /* break out of the loop */
    }
}

/* here after # - now read a full-word command */
static struct nh_cmd_desc *
doextcmd(nh_bool include_debug)
{
    int i, idx, size;
    struct nh_cmd_desc *retval = NULL;
    struct nh_cmd_desc retval_for_help; /* only the address matters */
    static const char exthelp[] = "?";

    size = 0;
    for (i = 0; i < cmdcount; i++)
        if ((commandlist[i].flags & CMD_EXT) &&
            (include_debug || !(commandlist[i].flags & CMD_DEBUG)))
            size++;

    const char *namelist[size + 1];
    const char *desclist[size + 1];

    /* add help */
    namelist[size] = exthelp;
    desclist[size] = "get this list of extended commands";

    idx = 0;
    for (i = 0; i < cmdcount; i++) {
        if ((commandlist[i].flags & CMD_EXT) &&
            (include_debug || !(commandlist[i].flags & CMD_DEBUG))) {
            namelist[idx] = commandlist[i].name;
            desclist[idx] = commandlist[i].desc;
            idx++;
        }
    }

    /* keep repeating until we don't run help */
    do {
        retval = &retval_for_help;
        curses_get_ext_cmd(namelist, desclist, size + 1, &retval,
                           doextcmd_callback);
        if (retval == &retval_for_help)
            doextlist(namelist, desclist, size + 1);
    } while (retval == &retval_for_help);

    return retval;
}

static void
show_whatdoes(void)
{
    char buf[BUFSZ];
    int key = curses_msgwin("What command?", krc_keybinding);

    if (key > KEY_MAX || !keymap[key])
        snprintf(buf, BUFSZ, "'%s' is not bound to any command.",
                 friendly_keyname(key));
    else
        snprintf(buf, BUFSZ, "'%s': %s - %s", friendly_keyname(key),
                 keymap[key]->name, keymap[key]->desc);
    curses_msgwin(buf, krc_notification);
}

static struct nh_cmd_desc *
show_help(void)
{
    struct nh_menulist menu;
    int i, selected[1];

    init_menulist(&menu);

    add_menu_item(&menu, 1, "list of game commands", 0, FALSE);
    add_menu_item(&menu, 2, "explain what a key does", 0, FALSE);
    add_menu_item(&menu, 3, "list of options", 0, FALSE);

    for (i = 0; i < cmdcount; i++)
        if (commandlist[i].flags & CMD_HELP)
            add_menu_item(&menu, 100 + i, commandlist[i].desc, 0, FALSE);

    curses_display_menu(&menu, "Help topics:", PICK_ONE, PLHINT_ANYWHERE,
                        selected, curses_menu_callback);

    if (*selected == CURSES_MENU_CANCELLED)
        return NULL;

    switch (selected[0]) {
    case 1:
        show_keymap_menu(TRUE);
        break;

    case 2:
        show_whatdoes();
        break;

    case 3:
        print_options();
        break;

    default:
        if (selected[0] >= 100 && selected[0] < cmdcount + 100)
            return &commandlist[selected[0] - 100];
        break;
    }

    return NULL;
}

static struct nh_cmd_desc *
show_mainmenu(nh_bool inside_another_command, nh_bool include_debug_commands)
{
    struct nh_menulist menu;
    int i, selected[1];

    init_menulist(&menu);

    if (!inside_another_command)
        for (i = 0; i < cmdcount; i++)
            if (commandlist[i].flags & CMD_MAINMENU &&
                (ui_flags.current_followmode == FM_PLAY ||
                 commandlist[i].flags & CMD_NOTIME))
                add_menu_item(&menu, 100 + i, commandlist[i].desc, 0, FALSE);

    if (!inside_another_command)
        add_menu_item(&menu, 1, ui_flags.current_followmode == FM_PLAY ?
                      "set options" : "set interface options", 0, FALSE);
    if (ui_flags.current_followmode != FM_REPLAY)
        add_menu_item(&menu, 2, "view a replay of this game", 0, FALSE);
    add_menu_item(&menu, 3, ui_flags.current_followmode == FM_PLAY ?
                  "save or abandon the game" : "stop viewing", 0, FALSE);
    if (include_debug_commands)
        add_menu_item(&menu, 4, "(debug) crash the client", 0, FALSE);

    curses_display_menu(&menu, "Main menu", PICK_ONE, PLHINT_ANYWHERE, selected,
                        curses_menu_callback);

    if (*selected == CURSES_MENU_CANCELLED)
        return NULL;

    if (selected[0] >= 100 && selected[0] < cmdcount + 100)
        return &commandlist[selected[0] - 100];

    if (selected[0] == 1) {
        display_options(FALSE);
        draw_map(player.x, player.y);
        return find_command("interrupt");
    } else if (selected[0] == 2) {
        instant_replay();
    } else if (selected[0] == 3) {
        save_menu();
    } else if (selected[0] == 4) {
        raise(SIGSEGV);
    }

    return NULL;
}

static void
instant_replay(void)
{
    ui_flags.current_followmode = FM_REPLAY;
    if (ui_flags.available_followmode == FM_WATCH)
        ui_flags.gameload_message =
            "You are now in replay mode.  To return to watching the game "
            "live, use the 'save' command.";
    else
        ui_flags.gameload_message =
            "You are now in replay mode.  To return to playing the game "
            "live, use the 'save' command.";
    nh_exit_game(EXIT_RESTART);
}

static void
save_menu(void)
{
    struct nh_menulist menu;
    int selected[1];

    /* No need for a confirmation if we're just watching. */
    if (ui_flags.current_followmode != FM_PLAY)
        nh_exit_game(EXIT_SAVE);

    init_menulist(&menu);

    add_menu_item(&menu, 1, "Close the game.", 'y', FALSE);
    add_menu_txt(&menu, "Your save file will remain stored on disk, and",
                 MI_NORMAL);
    add_menu_txt(&menu, "you can resume the game later.", MI_NORMAL);
    add_menu_txt(&menu, "", MI_NORMAL);

    add_menu_item(&menu, 2, "Abandon the game.", '!', FALSE);
    add_menu_txt(&menu, "You will see your statistics, as if you had died;",
                 MI_NORMAL);
    add_menu_txt(&menu, "the save file will be deleted (although a replay",
                 MI_NORMAL);
    add_menu_txt(&menu, "will be kept). You will not be able to resume the",
                 MI_NORMAL);
    add_menu_txt(&menu, "game, not even from an earlier save file.", MI_NORMAL);
    add_menu_txt(&menu, "", MI_NORMAL);

    add_menu_item(&menu, 3, "Keep playing.", 'n', FALSE);

    curses_display_menu(&menu, "Do you want to stop playing?", PICK_ONE,
                        PLHINT_URGENT, selected, curses_menu_callback);

    switch (*selected) {
    case CURSES_MENU_CANCELLED:
    case 3:
        return;
    case 1:
        /* We've already got the confirmation just now, so... */
        nh_exit_game(EXIT_SAVE);
        return;
    case 2:
        /* Ask for a second confirmation, this is really dangerous! */
        init_menulist(&menu);
        add_menu_item(&menu, 1, "Yes, delete the save file", 'y', FALSE);
        add_menu_item(&menu, 2, "No, I want to keep playing", 'n', FALSE);
        curses_display_menu(&menu, "Really delete the save file?", PICK_ONE,
                            PLHINT_URGENT, selected, curses_menu_callback);

        if (*selected == 1)
            nh_exit_game(EXIT_QUIT);

        return;
    }

    /* should be unreachable */
    return;
}

static void
dostop(void)
{
#ifndef WIN32
    if (ui_flags.no_stop) {
#endif
        curses_msgwin("Process suspension is disabled on this instance.",
                      krc_notification);
#ifndef WIN32
        return;
    }

    kill(getpid(), SIGTSTP);
#endif
}

void
dotogglepickup(void)
{
    union nh_optvalue val;
    struct nh_option_desc *options = nh_get_options(),
        *option = nhlib_find_option(options, "autopickup");

    if (!option) {
        curses_msgwin("Error: No autopickup option found.", krc_notification);
        nhlib_free_optlist(options);
        return;
    }

    val.b = !option->value.b;
    curses_set_option("autopickup", val);

    curses_msgwin(val.b ? "Autopickup now ON" : "Autopickup now OFF",
                  krc_notification);
    nhlib_free_optlist(options);
}

/*----------------------------------------------------------------------------*/

/* read the user-configured keymap from keymap.conf.
 * Return TRUE if this succeeds, FALSE otherwise */
static nh_bool
read_keymap(void)
{
    fnchar filename[BUFSZ];
    char *line, *endptr;
    int fd, size, pos, key, i;
    struct nh_cmd_desc *cmd;
    nh_bool unknown;
    struct nh_cmd_desc *unknown_commands_prev;
    long ptrdiff;

    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
        return FALSE;
    if (ui_flags.connection_only) {
#ifdef WIN32
        wchar_t usernamew[BUFSZ];
        int i = 0;

        while (i < BUFSZ - 2 && ui_flags.username[i]) {
            usernamew[i] = ui_flags.username[i];
            i++;
        }
        usernamew[i] = 0;
        fnncat(filename, usernamew, BUFSZ - fnlen(filename) - 1);
#else
        fnncat(filename, ui_flags.username, BUFSZ - fnlen(filename) - 1);
#endif
        fnncat(filename, FN(".keymap"), BUFSZ - fnlen(filename) - 1);
    } else
        fnncat(filename, FN("keymap.conf"), BUFSZ - fnlen(filename) - 1);

    fd = sys_open(filename, O_RDONLY, 0);
    if (fd == -1)
        return FALSE;

    size = lseek(fd, 0, SEEK_END);

    char data[size + 1];

    while (1) {
        lseek(fd, 0, SEEK_SET);
        errno = 0;
        int rcount = read(fd, data, size);

        if (rcount == size)
            break;
        else if (rcount != -1 || errno != EINTR)
            return FALSE;
    }
    data[size] = '\0';
    close(fd);

    unknown_count = 0;
    memset(unknown_keymap, 0, sizeof (unknown_keymap));

    /* read the file */
    line = strtok(data, "\r\n");
    while (line) {
        /* find the first non-space after the first space (ie the second word) */
        pos = 0;
        while (line[pos] && !isspace(line[pos]))
            pos++;
        while (line[pos] && isspace(line[pos]))
            pos++;

        unknown = FALSE;
        cmd = find_command(&line[pos]);
        if (line[pos] == '-') {
            /* old version of the keymap, with dangerously wrong keybindings */
            curses_msgwin
                ("keymap.conf has changed format. Your keybindings have "
                 "reverted to defaults.", krc_notification);
            init_keymap();
            write_keymap();
            return FALSE;
        }
        /* record unknown commands in the keymap: these may simply be valid,
           but unavailable in the current game. For example, the file might
           contain mappings for wizard mode commands. */
        if (!cmd && line[pos] != '-') {
            unknown = TRUE;
            if (unknown_count >= unknown_size) {
                unknown_size = max(unknown_size * 2, 16);
                unknown_commands_prev = unknown_commands;
                unknown_commands =
                    realloc(unknown_commands,
                            unknown_size * sizeof (struct nh_cmd_desc));
                memset(&unknown_commands[unknown_count], 0,
                       sizeof (struct nh_cmd_desc) *
                       (unknown_size - unknown_count));

                /* since unknown_commands has been realloc'd, pointers must be
                   adjusted to point to the new list rather than free'd memory */
                ptrdiff =
                    (char *)unknown_commands - (char *)unknown_commands_prev;
                for (i = 0; i <= KEY_MAX; i++) {
                    if (!unknown_keymap[i])
                        continue;

                    unknown_keymap[i] =
                        (void *)((char *)unknown_keymap[i] + ptrdiff);
                }
            }
            unknown_count++;
            cmd = &unknown_commands[unknown_count - 1];
            strncpy(cmd->name, &line[pos], sizeof (cmd->name));
            cmd->name[sizeof (cmd->name) - 1] = '\0';
        }

        if (cmd) {
            if (!strncmp(line, "EXT", 3))
                cmd->flags |= CMD_EXT;
            else if (!strncmp(line, "NOEXT", 5))
                cmd->flags &= ~CMD_EXT;
            else {
                key = strtol(line, &endptr, 16);
                if (key == 0 || endptr == line)
                    goto badmap;

                if (key < 0 || key > KEY_MAX)   /* manual edit or version
                                                   difference */
                    goto nextline;      /* nothing we can do with this, except
                                           perhaps complain */

                if (!unknown)
                    keymap[key] = cmd;
                else
                    unknown_keymap[key] = cmd;
            }
        }
    nextline:
        line = strtok(NULL, "\r\n");
    }

    return TRUE;

badmap:
    curses_msgwin("Bad/damaged keymap.conf. Reverting to defaults.",
                  krc_notification);
    init_keymap();
    return FALSE;
}

/* Like write(), but EINTR-safe, and closes the fd on error. */
static nh_bool
write_keymap_write(int fd, const void *buffer, int len)
{
    errno = 0;
    int written = 0;

    while (written < len) {
        int rv = write(fd, ((char *)buffer) + written, len - written);

        if (rv < 0 && errno == EINTR)
            continue;
        if (rv <= 0) {
            close(fd);
            return FALSE;
        }
        written += rv;
    }
    return TRUE;
}

/* store the keymap in keymap.conf */
static void
write_keymap(void)
{
    int fd, i;
    unsigned int key;
    fnchar filename[BUFSZ];
    char buf[BUFSZ];
    const char *name;

    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
        return;
    if (ui_flags.connection_only) {
#ifdef WIN32
        wchar_t usernamew[BUFSZ];
        int i = 0;

        while (i < BUFSZ - 2 && ui_flags.username[i]) {
            usernamew[i] = ui_flags.username[i];
            i++;
        }
        usernamew[i] = 0;
        fnncat(filename, usernamew, BUFSZ - fnlen(filename) - 1);
#else
        fnncat(filename, ui_flags.username, BUFSZ - fnlen(filename) - 1);
#endif
        fnncat(filename, FN(".keymap"), BUFSZ - fnlen(filename) - 1);
    } else
        fnncat(filename, FN("keymap.conf"), BUFSZ - fnlen(filename) - 1);

    fd = sys_open(filename, O_TRUNC | O_CREAT | O_RDWR, 0660);
    if (fd == -1)
        return;

    for (key = 1; key <= KEY_MAX; key++) {
        name =
            keymap[key] ? keymap[key]->name : (unknown_keymap[key] ?
                                               unknown_keymap[key]->name : "-");
        snprintf(buf, ARRAY_SIZE(buf), "%x %s\n", key, name);
        if (strcmp(name, "-"))
            if (!write_keymap_write(fd, buf, strlen(buf)))
                return;
    }

    for (i = 0; i < cmdcount; i++) {
        if (commandlist[i].flags & CMD_EXT) {
            snprintf(buf, ARRAY_SIZE(buf), "EXT %s\n", commandlist[i].name);
            if (!write_keymap_write(fd, buf, strlen(buf)))
                return;
        } else {
            snprintf(buf, ARRAY_SIZE(buf), "NOEXT %s\n", commandlist[i].name);
            if (!write_keymap_write(fd, buf, strlen(buf)))
                return;
        }
    }

    for (i = 0; i < unknown_count; i++) {
        if (unknown_commands[i].flags & CMD_EXT) {
            snprintf(buf, ARRAY_SIZE(buf), "EXT %s\n",
                     unknown_commands[i].name);
            if (!write_keymap_write(fd, buf, strlen(buf)))
                return;
        } else {
            snprintf(buf, ARRAY_SIZE(buf), "NOEXT %s\n",
                     unknown_commands[i].name);
            if (!write_keymap_write(fd, buf, strlen(buf)))
                return;
        }
    }

    close(fd);
}

/* initialize the keymap with the default keys suggested by NetHack */
static void
init_keymap(void)
{
    int i;
    int count = sizeof (builtin_commands) / sizeof (struct nh_cmd_desc);

    memset(keymap, 0, sizeof (keymap));

    /* num pad direction keys */
    keymap[KEY_UP] = find_command("north");
    keymap[KEY_DOWN] = find_command("south");
    keymap[KEY_LEFT] = find_command("west");
    keymap[KEY_RIGHT] = find_command("east");
    /* if the terminal gives us sufficient control over the numpad, we can do
       this */
    keymap[KEY_A2] = find_command("north");
    keymap[KEY_C2] = find_command("south");
    keymap[KEY_B1] = find_command("west");
    keymap[KEY_B3] = find_command("east");
    keymap[KEY_A1] = find_command("north_west");
    keymap[KEY_A3] = find_command("north_east");
    keymap[KEY_C1] = find_command("south_west");
    keymap[KEY_C3] = find_command("south_east");
    keymap[KEY_B2] = find_command("go");
    keymap[KEY_D1] = find_command("inventory");
    /* otherwise we have to do it like this */
    keymap[KEY_HOME] = find_command("north_west");
    keymap[KEY_PPAGE] = find_command("north_east");
    keymap[KEY_END] = find_command("south_west");
    keymap[KEY_NPAGE] = find_command("south_east");
    keymap['\r'] = NO_KEYMAP;
    keymap[' '] = NO_KEYMAP;

    /* main keyboard numbers are command repeat by default */
    for (i = '1'; i <= '9'; i++)
        keymap[i] = find_command("repeatcount");

    /* every command automatically gets its default key */
    for (i = 0; i < cmdcount; i++)
        if (commandlist[i].defkey)
            keymap[commandlist[i].defkey] = &commandlist[i];

    for (i = 0; i < count; i++)
        if (builtin_commands[i].defkey)
            keymap[builtin_commands[i].defkey] = &builtin_commands[i];

    /* alt keys are assigned if the key is not in use */
    for (i = 0; i < cmdcount; i++) {
        if (commandlist[i].altkey && !keymap[commandlist[i].altkey])
            keymap[commandlist[i].altkey] = &commandlist[i];
    }

    for (i = 0; i < count; i++) {
        if (builtin_commands[i].altkey && !keymap[builtin_commands[i].altkey])
            keymap[builtin_commands[i].altkey] = &builtin_commands[i];
    }

    /* if we have meta+key combinations assigned, assign alt+key too */
    for (i = 128; i < 256; i++) {
        keymap[KEY_ALT | (i - 128)] = keymap[i];
    }
}

void
load_keymap(void)
{
    struct nh_cmd_desc *cmdlist = nh_get_commands(&cmdcount);

    commandlist = malloc(cmdcount * sizeof (struct nh_cmd_desc));
    memcpy(commandlist, cmdlist, cmdcount * sizeof (struct nh_cmd_desc));

    /* always init the keymap - read keymap might not set up every mapping */
    init_keymap();
    read_keymap();
}

void
free_keymap(void)
{
    free(commandlist);
    commandlist = NULL;
    cmdcount = 0;

    if (unknown_commands) {
        free(unknown_commands);
        unknown_commands = NULL;
        unknown_size = unknown_count = 0;
    }
}

static void
add_keymap_action(struct nh_menulist *menu, enum keymap_action id,
                  char shortcut, const char *text)
{
    add_menu_item(menu, id, text, shortcut, FALSE);
}

/* add the description of a command to the keymap menu */
static void
add_keylist_command(struct nh_menulist *menu, struct nh_cmd_desc *cmd, int id)
{
    char buf[BUFSZ];
    char keys[23 + 1];
    int i, kl;
    const char *keyname;
    int dots = 0;       /* future position of trailing '...' */

    if (cmd->flags & CMD_INTERNAL)
        return;

    keys[0] = '\0';
    for (i = 0; i <= KEY_MAX; i++) {
        /* Don't display Alt-combinations with alt_is_esc turned on */
        if (settings.alt_is_esc && i == (KEY_ALT | (i & 0xff)))
            continue;
        if (keymap[i] == cmd) {
            keyname = friendly_keyname(i);
            kl = strlen(keys);
            if (kl + strlen(keyname) + 1 > sizeof (keys)) {
                /* Not enough room for all keynames so emit dots */
                if (kl)
                    strcpy(keys + dots, " ...");
                else
                    strcpy(keys + dots, "...");
                break;
            }
            if (kl) {
                keys[kl++] = ' ';
                keys[kl] = '\0';
            }
            strcat(keys, friendly_keyname(i));
            if (strlen(keys) + 4 + 1 < sizeof (keys)) {
                dots = strlen(keys);
            }
        }
    }

    snprintf(buf, ARRAY_SIZE(buf), "%s%.15s\t%.51s\t%.23s",
             cmd->flags & CMD_EXT ? "#" : "", cmd->name, cmd->desc, keys);
    add_menu_item(menu, id, buf, 0, FALSE);

}

/* display a menu to alter the key bindings for the given command */
static void
command_settings_menu(struct nh_cmd_desc *cmd)
{
    char buf[BUFSZ];
    int i, selection[1];
    struct nh_menulist menu;

    do {
        init_menulist(&menu);

        for (i = 0; i <= KEY_MAX; i++) {
            if (keymap[i] == cmd) {
                snprintf(buf, ARRAY_SIZE(buf), "delete key %s",
                         friendly_keyname(i));
                add_menu_item(&menu, i, buf, 0, FALSE);
            }
        }

        if (menu.icount > 0)
            add_menu_txt(&menu, "", MI_NORMAL);

        add_menu_item(&menu, -1, "Add a new key", '+', FALSE);
        if (!(cmd->flags & CMD_UI)) {
            if (cmd->flags & CMD_EXT)
                add_menu_item(&menu, -2, "Don't use as an extended command", 0,
                              FALSE);
            else
                add_menu_item(&menu, -2, "Use as an extended command", 0,
                              FALSE);
        }

        snprintf(buf, ARRAY_SIZE(buf), "Key bindings for %s", cmd->name);
        curses_display_menu(&menu, buf, PICK_ONE, PLHINT_ANYWHERE, selection,
                            curses_menu_callback);

        if (*selection == CURSES_MENU_CANCELLED)
            break;

        /* int this menu, ids > 0 are used for "delete key" items and id is the
           actual key. Negative ids are used for the 2 static menu items and for
           CURSES_MENU_CANCELLED */
        if (selection[0] > 0)   /* delete a key */
            keymap[selection[0]] = NO_KEYMAP;
        else if (selection[0] == -1) {  /* add a key */
            snprintf(buf, ARRAY_SIZE(buf),
                     "Press the key you want to use for \"%s\"", cmd->name);
            i = curses_msgwin(buf, krc_keybinding);
            if (i == KEY_ESCAPE || i > KEY_MAX)
                continue;
            if (keymap[i]) {
                snprintf(buf, ARRAY_SIZE(buf),
                         "That key is already in use by \"%s\"! Replace?",
                         keymap[i]->name);
                if ('y' != curses_yn_function_internal(buf, "yn", 'n'))
                    continue;
            }
            keymap[i] = cmd;

        } else if (selection[0] == -2) {        /* toggle extended command
                                                   status */
            cmd->flags = (cmd->flags ^ CMD_EXT);
        }

    } while (1);
}

static nh_bool
do_select_cmd(struct nh_cmd_desc *cmd, enum select_cmd sel)
{
    switch (sel) {
    case SELECT_CMD_DEBUG:
        return (cmd->flags & CMD_DEBUG) != 0;
    case SELECT_CMD_DIRECTION:
        return (cmd->flags & CMD_UI)
            && (cmd->flags & (DIRCMD | DIRCMD_RUN | DIRCMD_GO))
            /* Ignore UP and DOWN */
            && (cmd->flags != (CMD_UI | DIRCMD | DIR_UP))
            && (cmd->flags != (CMD_UI | DIRCMD | DIR_DOWN));
    case SELECT_CMD_UI:
        return (cmd->flags & CMD_UI)
            && !(cmd->flags & (DIRCMD | DIRCMD_RUN | DIRCMD_GO));
    case SELECT_CMD_GAME:
        return !do_select_cmd(cmd, SELECT_CMD_DEBUG)
            && !do_select_cmd(cmd, SELECT_CMD_DIRECTION)
            && !do_select_cmd(cmd, SELECT_CMD_UI);
    case SELECT_CMD_ALL:
        return TRUE;
    default:
        return FALSE;
    }

}

/*
 * Add the selected commands to a keylist menu.
 *
 * - 'sel' specifies the method used to select the commands
 * - 'order'  specifies the method used to sort the commands
 *
 */
static void
add_keylist_selection(struct nh_menulist *menu, enum select_cmd sel,
                      enum order_cmd order, nh_bool readonly)
{
    int nb_builtin_commands = ARRAY_SIZE(builtin_commands);
    int max_nb_commands = nb_builtin_commands + cmdcount;

    struct entry {
        struct nh_cmd_desc *cmd;
        int id;
    };

    int i, j;
    int len;
    struct entry *list;

    list = (struct entry *)malloc(sizeof (struct entry) * max_nb_commands);
    len = 0;

    /* add builtin commands to list (with negative ids) */
    for (i = 0; i < nb_builtin_commands; i++) {
        struct nh_cmd_desc *cmd = &builtin_commands[i];

        if (do_select_cmd(cmd, sel)) {
            list[len].cmd = cmd;
            list[len].id = readonly ? 0 : -(i + 1);
            len++;
        }
    }

    /* add Nethack commands to list (with positive ids) */
    for (i = 0; i < cmdcount; i++) {
        struct nh_cmd_desc *cmd = &commandlist[i];

        if (do_select_cmd(cmd, sel)) {
            list[len].cmd = cmd;
            list[len].id = readonly ? 0 : (i + 1);
            len++;
        }
    }

/* utility macro to swap 2 variables of a given type */
#define SWAP(type, a, b) do { type tmp = a ; a = b ; b = tmp ; } while (0)

    /* Sort entries as requested */
    switch (order) {
    case ORDER_CMD_NAME:
        /* A stupid n^2 sort should not really matter here */
        for (i = 0; i < len; i++)
            for (j = i + 1; j < len; j++)
                if (strcmp(list[i].cmd->name, list[j].cmd->name) > 0)
                    SWAP(struct entry, list[i], list[j]);

        break;

    case ORDER_CMD_NONE:
    default:
        break;
    }

    /* And add them all to the menu */
    for (i = 0; i < len; i++) {
        add_keylist_command(menu, list[i].cmd, list[i].id);
    }

    free(list);
}

/*
 * A function to make a simple dialog box
 *
 * Argument 'title' specifies the title of the dialog.
 *
 * Argument 'desc' is the array of lines that compose the dialog.
 *
 * - a line starting with '#' is a heading( and so will be highlighted)
 * - a line starting with ':' is a regular text
 * - a line stating with '[x]' where 'x' is a single character
 *   specifies a possible choice associated to the key 'x'
 * - the last line shall be NULL
 *
 * If the dialog was cancelled then return CURSES_MENU_CANCELLED
 * else return the key of the selected choice (one of the 'x' above)
 */
static int
simple_dialog(const char *title, const char *const *desc)
{
    struct nh_menulist menu;
    const char *line;
    int i, result;

    init_menulist(&menu);

    for (i = 0; desc[i] != NULL; i++) {
        line = desc[i];
        if (line[0] == '#') {
            add_menu_txt(&menu, line + 1, MI_HEADING);
        } else if (line[0] == '[' && strlen(line) >= 3 && line[2] == ']') {
            add_menu_item(&menu, line[1], line + 3, line[1], FALSE);
        } else if (line[0] == ':') {
            add_menu_txt(&menu, line + 1, MI_TEXT);
        } else {
            add_menu_txt(&menu, line, MI_TEXT);
        }
    }

    curses_display_menu(&menu, title, PICK_ONE, PLHINT_ANYWHERE, &result,
                        curses_menu_callback);

    return result;
}

static void
keymap_action_reset_all(void)
{
    int i, count = 0;
    struct nh_cmd_desc *cmd, *cmdlist;
    int res;

    const char *const dialog[] = {
        ":",
        "#Warning: All key bindings will be restored",
        "#         to their default settings!",
        ":",
        "[r]Reset all keybindings",
        "[c]Cancel",
        NULL
    };

    res = simple_dialog("Reset all key bindings", dialog);
    if (res == 'r') {

        init_keymap();  /* fully reset the keymap */

        /* reset extcmds */
        cmdlist = nh_get_commands(&count);
        for (i = 0; i < count; i++) {
            cmd = find_command(cmdlist[i].name);
            if (cmd)
                cmd->flags = cmdlist[i].flags;
        }
    }
}

/* Remove the mapping of cmdname to the specified key. Do nothing if that
   command is not mapped to that key. */
static void
remove_keymap_if(int key, const char *cmdname)
{
    struct nh_cmd_desc *cmd = find_command(cmdname);

    if (cmd)
        if (keymap[key] == cmd)
            keymap[key] = NO_KEYMAP;
}

/* Remove the vi direction keys, including their SHIFT and CTRL variants, from
   run_ or go_ commands. */
static void
remove_all_vi_directions(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        struct nh_cmd_desc *dir = find_command(all_directions[i]);
        char key, shift_key, ctrl_key;

        key = dir->defkey;
        remove_keymap_if(key, dir->name);

        /* Assume that SHIFT may be mapped to either RUN_ or GO_ */
        shift_key = guess_shift_key(key);
        if (shift_key != 0 && shift_key != key) {
            remove_keymap_if(shift_key, all_run_directions[i]);
            remove_keymap_if(shift_key, all_go_directions[i]);
        }

        /* Same for CTRL */
        ctrl_key = guess_ctrl_key(key);
        if (ctrl_key != 0 && ctrl_key != key) {
            remove_keymap_if(ctrl_key, all_run_directions[i]);
            remove_keymap_if(ctrl_key, all_go_directions[i]);
        }

    }
}

static void
keymap_action_massrebind_vi(void)
{
    int i, res;

    const char *const dialog[] = {
        ":The letter keys \"hjklyubn\" are used as direction keys in many",
        ":configurations of NetHack (\"vi-keys\" configuration):",
        ":",
        ":    y k u ",
        ":    h   l ",
        ":    b j n ",
        ":",
        ":In other configurations, they are used as abbreviations for",
        ":other commands:",
        ":",
        ":h  help             (typically on ?)",
        ":j  jump             (typically on #jump or Meta-j)",
        ":k  kick             (typically on Ctrl-d)",
        ":l  loot             (typically on #loot or Meta-l)",
        ":n  command repeat   (typically on main keyboard digits)",
        ":u  untrap           (typically on #untrap)",
        ":",
        "[v]Bind hjklyubn to direction keys",
        "[V]Bind hjklyubn and Ctrl- and Shift- variants to direction keys",
        "[c]Bind hjklnu to commands, unbind yb",
        "[u]Unbind hjklyubn and Ctrl- and Shift- variants from direction keys",
        "[q]Cancel",
        NULL
    };

    res = simple_dialog("Mass rebind: hjklyubn", dialog);

    /* The vikeys are in the default command mappings, so we can install them
       via resetting direction commands to defaults. */

    if (res == 'v') {
        for (i = 0; i < 8; i++) {
            struct nh_cmd_desc *dir = find_command(all_directions[i]);

            keymap[dir->defkey] = dir;
        }
    } else if (res == 'V') {
        for (i = 0; i < 8; i++) {
            struct nh_cmd_desc *dir;

            dir = find_command(all_directions[i]);
            keymap[dir->defkey] = dir;
            dir = find_command(all_run_directions[i]);
            keymap[dir->defkey] = dir;
            dir = find_command(all_go_directions[i]);
            keymap[dir->defkey] = dir;
        }
    } else if (res == 'u') {
        remove_all_vi_directions();
    } else if (res == 'c') {
        keymap['h'] = find_command("help");
        keymap['j'] = find_command("jump");
        keymap['k'] = find_command("kick");
        keymap['l'] = find_command("loot");
        keymap['n'] = find_command("repeatcount");
        keymap['u'] = find_command("untrap");
        keymap['y'] = NO_KEYMAP;
        keymap['b'] = NO_KEYMAP;
    }
}

static void
keymap_action_massrebind_digits(void)
{
    int res;

    const char *const dialog[] = {
        ":The digits \"0123456789\" are used as direction keys in many",
        ":configurations of NetHack (\"numpad\" configuration):",
        ":",
        ":    7 8 9",
        ":    4   6   (5 = farmove, 0 = inventory, . = wait)",
        ":    1 2 3",
        ":",
        ":In other configurations, they are used for command repeat,",
        ":specifying how many turns to perform a command for.",
        ":",
        ":Some terminals can distinguish the numeric keypad keys from the main",
        ":keyboard keys and the cursor movement keys. Some terminals cannot,",
        ":producing main keyboard numbers with NumLock off, and cursor",
        ":movement keys (Left, Home, etc.) with NumLock on.",
        ":",
        "[k]Bind numeric keypad numbers to directions (terminal-dependent)",
        "[m]Bind main keypad numbers to directions",
        "[c]Bind cursor movement keys to directions",
        "[a]Bind both numeric keypad and cursor movement keys to directions",
        "[r]Bind main keypad numbers to command repeat",
        "[v]Bind numeric keypad numbers to command repeat",
        "[q]Cancel",
        NULL
    };

    res = simple_dialog("Mass rebind: 123456789", dialog);

    if (res == 'k' || res == 'a') {
        keymap[KEY_A2] = find_command("north");
        keymap[KEY_C2] = find_command("south");
        keymap[KEY_B1] = find_command("west");
        keymap[KEY_B3] = find_command("east");
        keymap[KEY_A1] = find_command("north_west");
        keymap[KEY_A3] = find_command("north_east");
        keymap[KEY_C1] = find_command("south_west");
        keymap[KEY_C3] = find_command("south_east");

        keymap[KEY_B2] = find_command("go");
        keymap[KEY_D1] = find_command("inventory");
        keymap[KEY_D3] = find_command("wait");
    }

    if (res == 'm') {
        keymap['8'] = find_command("north");
        keymap['2'] = find_command("south");
        keymap['4'] = find_command("west");
        keymap['6'] = find_command("east");
        keymap['7'] = find_command("north_west");
        keymap['9'] = find_command("north_east");
        keymap['1'] = find_command("south_west");
        keymap['3'] = find_command("south_east");

        keymap['5'] = find_command("go");
        keymap['0'] = find_command("inventory");
        /* . is not a digit */
    }

    if (res == 'c' || res == 'a') {
        keymap[KEY_UP] = find_command("north");
        keymap[KEY_DOWN] = find_command("south");
        keymap[KEY_LEFT] = find_command("west");
        keymap[KEY_RIGHT] = find_command("east");
        keymap[KEY_HOME] = find_command("north_west");
        keymap[KEY_PPAGE] = find_command("north_east");
        keymap[KEY_END] = find_command("south_west");
        keymap[KEY_NPAGE] = find_command("south_east");

        keymap[KEY_B2] = find_command("go"); /* produces a unique code */
        keymap[KEY_IC] = find_command("inventory");
        keymap[KEY_DC] = find_command("wait");
    }

    if (res == 'r') {
        int i;
        for (i = '0'; i <= '9'; i++)
            keymap[i] = find_command("repeatcount");
    }

    if (res == 'v') {
        keymap[KEY_A2] = find_command("repeatcount");
        keymap[KEY_C2] = find_command("repeatcount");
        keymap[KEY_B1] = find_command("repeatcount");
        keymap[KEY_B3] = find_command("repeatcount");
        keymap[KEY_A1] = find_command("repeatcount");
        keymap[KEY_A3] = find_command("repeatcount");
        keymap[KEY_C1] = find_command("repeatcount");
        keymap[KEY_C3] = find_command("repeatcount");

        keymap[KEY_B2] = find_command("repeatcount");
        keymap[KEY_D1] = find_command("repeatcount");
    }
}

static void
keymap_action_massrebind_modif(void)
{
    int i, imax, res, key, mkey;
    struct nh_cmd_desc *direction_commands[8];

    const char *const dialog[] = {
        ":In addition to moving a single space with the direction keys, there",
        ":are also commands for moving multiple spaces: 'run' which will",
        ":interact with, e.g., doors found on the way, and 'go' which is more",
        ":cautious, stopping upon finding such features. One common binding",
        ":for these is Shift- or Ctrl-direction. (You can also bind a key like",
        ":'g' or '5' as a prefix via the game command binding menu.)",
        ":",
        ":Some players may prefer to use Shift-/Ctrl-direction for diagonal",
        ":movement.",
        ":",
        "[g]Map Ctrl-direction to the 'go' command",
        "[r]Map Ctrl-direction to the 'run' command",
        "[d]Map Ctrl-direction to diagonals (rotate 45 degrees clockwise)",
        "[u]Unmap Ctrl-direction",
        "[G]Map Shift-direction to the 'go' command",
        "[R]Map Shift-direction to the 'run' command",
        "[D]Map Shift-direction to diagonals (rotate 45 degrees clockwise)",
        "[U]Unmap Shift-direction",
        "[q]Cancel",
        NULL
    };

    res = simple_dialog("Mass rebind: Shift/Ctrl-direction", dialog);

    if (res == 'q' || res == CURSES_MENU_CANCELLED)
        return;

    for (i = 0; i < 8; i++)
        direction_commands[i] = find_command(all_directions[i]);

    imax = (res == 'd' || res == 'D') ? 4 : 8;

    for (key = 0; key <= KEY_MAX; key++) {
        for (i = 0; i < imax; i++) {
            if (keymap[key] == direction_commands[i]) {
                if (res == 'g' || res == 'r' || res == 'u')
                    mkey = guess_ctrl_key(key);
                else
                    mkey = guess_shift_key(key);

                if (mkey == key || mkey == 0)
                    ; /* be silent about this: the user might not care about
                         a failure to rebind, say, shift-main-keyboard-2,
                         and we've already warned about terminal problems */
                else if (res == 'g' || res == 'G')
                    keymap[mkey] = find_command(all_go_directions[i]);
                else if (res == 'r' || res == 'R')
                    keymap[mkey] = find_command(all_run_directions[i]);
                else if (res == 'u' || res == 'U')
                    keymap[mkey] = NO_KEYMAP;
                else if (res == 'd' || res == 'D')
                    keymap[mkey] = find_command(all_directions[i + 4]);
            }
        }
    }
}

static nh_bool set_command_keys(struct win_menu *mdat, int idx);

/* Sub-menu to manage the debug keymaps */
static void
keymap_action_debug_submenu(void)
{
    int selected[1];
    struct nh_menulist menu;
    nh_bool readonly = FALSE;

    do {
        init_menulist(&menu);

        add_menu_txt(&menu, "Command\tDescription\tKey", MI_HEADING);

        add_keylist_selection(&menu, SELECT_CMD_DEBUG, ORDER_CMD_NAME,
                              readonly);

        curses_display_menu_core(&menu, "Rebind keys: Debug commands",
                                 readonly ? PICK_NONE : PICK_ONE, selected,
                                 curses_menu_callback, 0, 0, COLS, LINES, FALSE,
                                 set_command_keys, TRUE);

    } while (*selected != CURSES_MENU_CANCELLED);
}

/* Sub-menu to manage the UI keymaps */
static void
keymap_action_ui_submenu(void)
{
    int selected[1];
    struct nh_menulist menu;
    nh_bool readonly = FALSE;

    do {
        init_menulist(&menu);

        add_menu_txt(&menu, "Command\tDescription\tKey", MI_HEADING);

        add_keylist_selection(&menu, SELECT_CMD_UI, ORDER_CMD_NAME, readonly);

        curses_display_menu_core(&menu, "Rebind keys: UI commands",
                                 readonly ? PICK_NONE : PICK_ONE, selected,
                                 curses_menu_callback, 0, 0, COLS, LINES, FALSE,
                                 set_command_keys, TRUE);

    } while (*selected != CURSES_MENU_CANCELLED);
}

/* Sub-menu to manage/show all keymaps at once. */
static void
keymap_action_all_submenu(nh_bool readonly)
{
    int selected[1];
    struct nh_menulist menu;

    do {
        init_menulist(&menu);

        add_menu_txt(&menu, "", MI_HEADING);
        add_menu_txt(&menu, "UI Command\tDescription\tKey", MI_HEADING);
        add_keylist_selection(&menu, SELECT_CMD_UI, ORDER_CMD_NAME, readonly);

        add_menu_txt(&menu, "", MI_HEADING);
        add_menu_txt(&menu, "Game Command\tDescription\tKey", MI_HEADING);
        add_keylist_selection(&menu, SELECT_CMD_GAME, ORDER_CMD_NAME, readonly);
        add_menu_txt(&menu, "", MI_HEADING);
        add_keylist_selection(&menu, SELECT_CMD_DIRECTION, ORDER_CMD_NONE,
                              readonly);

        add_menu_txt(&menu, "", MI_HEADING);
        add_menu_txt(&menu, "Debug Command\tDescription\tKey", MI_HEADING);
        add_keylist_selection(&menu, SELECT_CMD_DEBUG, ORDER_CMD_NAME,
                              readonly);

        curses_display_menu_core(&menu, "Rebind keys: All commands",
                                 readonly ? PICK_NONE : PICK_ONE, selected,
                                 curses_menu_callback, 0, 0, COLS, LINES, FALSE,
                                 set_command_keys, TRUE);

    } while (*selected != CURSES_MENU_CANCELLED);
}

/* Sub-menu to manage the game command keymaps */
static void
keymap_action_game_submenu(void)
{
    int selected[1];
    struct nh_menulist menu;
    nh_bool readonly = FALSE;

    do {
        init_menulist(&menu);

        add_menu_txt(&menu, "Command\tDescription\tKey", MI_HEADING);

        add_keylist_selection(&menu, SELECT_CMD_GAME, ORDER_CMD_NAME, readonly);
        add_menu_txt(&menu, "", MI_HEADING);
        add_keylist_selection(&menu, SELECT_CMD_DIRECTION, ORDER_CMD_NONE,
                              readonly);

        curses_display_menu_core(&menu, "Rebind keys: Game commands",
                                 readonly ? PICK_NONE : PICK_ONE, selected,
                                 curses_menu_callback, 0, 0, COLS, LINES, FALSE,
                                 set_command_keys, TRUE);

    } while (*selected != CURSES_MENU_CANCELLED);
}

static nh_bool
set_command_keys(struct win_menu *mdat, int idx)
{
    int id = mdat->visitems[idx]->id;
    struct nh_cmd_desc *cmd;

    if (id == RESET_BINDINGS_ID) {
        keymap_action_reset_all();
        return TRUE;
    }

    if (KEYMAP_ACTION_BEGIN < id && id < KEYMAP_ACTION_END) {
        switch (id) {
        case KEYMAP_ACTION_RESET_ALL:
            keymap_action_reset_all();
            break;
        case KEYMAP_ACTION_MASSREBIND_VI:
            keymap_action_massrebind_vi();
            break;
        case KEYMAP_ACTION_MASSREBIND_DIGITS:
            keymap_action_massrebind_digits();
            break;
        case KEYMAP_ACTION_MASSREBIND_MODIF:
            keymap_action_massrebind_modif();
            break;
        case KEYMAP_ACTION_ALL_SUBMENU:
            keymap_action_all_submenu(FALSE);
            break;
        case KEYMAP_ACTION_DEBUG_SUBMENU:
            keymap_action_debug_submenu();
            break;
        case KEYMAP_ACTION_UI_SUBMENU:
            keymap_action_ui_submenu();
            break;
        case KEYMAP_ACTION_GAME_SUBMENU:
            keymap_action_game_submenu();
            break;
        default:
            return FALSE;
        }
        return TRUE;
    }

    if (id < 0)
        cmd = &builtin_commands[-(id + 1)];
    else
        cmd = &commandlist[id - 1];

    command_settings_menu(cmd);

    return TRUE;
}

void
show_keymap_menu(nh_bool readonly)
{
    if (readonly) {
        keymap_action_all_submenu(TRUE);
    } else {
        int selected[1];
        struct nh_menulist menu;

        do {
            init_menulist(&menu);

            add_keymap_action(&menu, KEYMAP_ACTION_MASSREBIND_VI, 0,
                              "Mass rebind: hjklyubn");
            add_keymap_action(&menu, KEYMAP_ACTION_MASSREBIND_DIGITS, 0,
                              "Mass rebind: digits and direction keys");
            add_keymap_action(&menu, KEYMAP_ACTION_MASSREBIND_MODIF, 0,
                              "Mass rebind: Ctrl- and Shift-direction");
            add_menu_txt(&menu, "", MI_HEADING);
            add_keymap_action(&menu, KEYMAP_ACTION_UI_SUBMENU, 0,
                              "Rebind UI commands");
            add_keymap_action(&menu, KEYMAP_ACTION_GAME_SUBMENU, 0,
                              "Rebind game commands");
            add_keymap_action(&menu, KEYMAP_ACTION_DEBUG_SUBMENU, 0,
                              "Rebind debug commands");
            add_keymap_action(&menu, KEYMAP_ACTION_ALL_SUBMENU, 0,
                              "Rebind all commands");
            add_menu_txt(&menu, "", MI_HEADING);
            add_keymap_action(&menu, KEYMAP_ACTION_RESET_ALL, '!',
                              "Reset all key bindings to built-in defaults");
            curses_display_menu_core(&menu, "Keymap",
                                     readonly ? PICK_NONE : PICK_ONE, selected,
                                     curses_menu_callback, 0, 0, COLS, LINES,
                                     FALSE, set_command_keys, TRUE);

        } while (*selected != CURSES_MENU_CANCELLED);
    }
    write_keymap();
}
