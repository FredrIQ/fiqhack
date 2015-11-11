/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#ifdef UNIX
# include <unistd.h>
#endif

#define DEFAULT_NETHACKDIR "/usr/share/NetHack4/"

static void process_args(int, char **);
void append_slash(char *name);

struct settings settings;
struct interface_flags ui_flags;
nh_bool interrupt_multi = FALSE;
nh_bool game_is_running = FALSE;
int cmdline_role = ROLE_NONE, cmdline_race = ROLE_NONE;
int cmdline_gend = ROLE_NONE, cmdline_align = ROLE_NONE;
char cmdline_name[BUFSZ] = {0};
nh_bool random_player = FALSE;

char *override_hackdir, *override_userdir;

enum menuitems {
    NEWGAME = 1,
    LOAD,
    REPLAY,
    OPTIONS,
    TOPTEN,
    NETWORK,
    EXITGAME
};

static struct nh_menuitem mainmenu_items[] = {
    {NEWGAME, MI_NORMAL, 0, "new game", 'n'},
    {LOAD, MI_NORMAL, 0, "load game", 'l'},
    {REPLAY, MI_NORMAL, 0, "view replay", 'v'},
    {OPTIONS, MI_NORMAL, 0, "set options", 'o'},
    {TOPTEN, MI_NORMAL, 0, "show score list", 's'},
#if defined(NETCLIENT)
    {NETWORK, MI_NORMAL, 0, "connect to server", 'c'},
#endif
    {EXITGAME, MI_NORMAL, 0, "quit", 'q', 'x'}
};

static struct nh_menuitem mainmenu_items_noclient[] = {
    {NEWGAME, MI_NORMAL, 0, "new game", 'n'},
    {LOAD, MI_NORMAL, 0, "load game", 'l'},
    {REPLAY, MI_NORMAL, 0, "view replay", 'v'},
    {OPTIONS, MI_NORMAL, 0, "set options", 'o'},
    {TOPTEN, MI_NORMAL, 0, "show score list", 's'},
    {EXITGAME, MI_NORMAL, 0, "quit", 'q', 'x'}
};

const char *nhlogo_small[11] = {        /* created using pbmtoascii */
    "                                                       oo               _d#,   ",
    "MMM,   MM            oo    MM     MM                   MM             ,HMMP'   ",
    "MMMM   MM            MM    MM     MM                   MM           ,HMMMM     ",
    "MMMM|  MM    ,oo,   oMMoo  MM     MM   ,ooo      ,ooo  MM   oo,   .dMM*MMM     ",
    "MM`MH  MM   dMMMMb  #MM##  MM     MM  |MMMMM,   dMM#M| MM  dMP   ,MMH' MMM     ",
    "MM TM, MM  |MM' 9Mb  MM    MM#####MM  `'  `Mb  |MH' `' MM dMP  ,HMM?   ]MM##HMH",
    "MM `Mb MM  MMboodMM  MM    MM\"\"\"\"\"MM    ,ooMM  MM'     MMMMP  ?MMMH##MMMMMH*\"'\"",
    "MM  HM.MM  MMH#####  MM    MM     MM  .HMH#MM  MM      MMMMb  *###\"\"\"\"\"9MM|    ",
    "MM  |MMMM  9Mb       MM    MM     MM  HM' .MM  9M?     MM 9Mb          |MM|    ",
    "MM   MMMM  `MMbood|  MMoo  MM     MM  HMbdMMM  `MMb_d| MM `MMb          MMb    ",
    "MM   `MMM   `9MMMP'  `MMM  MM     MM  `*MM'MM   `9MMM' MM  `MMb         ?MH    ",
};

const char *nhlogo_large[14] = {
    "                                                                     ooo                   .##.    ",
    "####     ##b                      ###     ###                        MMM                 dMMMM'    ",
    "MMMMb    MMM              MMM     MMM     MMM                        MMM              .dHMMMM'     ",
    "MMMMM.   MMM              MMM     MMM     MMM                        MMM             oHMMMMMT      ",
    "MMMMM|   MMM     ooo,    oMMMoo,  MMM     MMM    ,oooo        oooo,  MMM   ooo,    .dMMH*MMM|      ",
    "MMM\"MH|  MMM   JMMMMMM.  MMMMMM|  MMM     MMM   dMMMMMM_    _MMMMMM  MMM  JMM|    JMMMH  MMM|      ",
    "MMM 9M|  MMM  |MMM\"\"MMH   MMM     MMMoooooMMM  |MP\"\"\"TMM   |MMP' \"9  MMM |MMT   oHMMP\"   9MMbooo##b",
    "MMM |MM  MMM  dMM'  |MMM  MMM     MMMMMMMMMMM         MMH  MMH'      MMMdMM\"  .dMMMR:_,ooHMMMMMMM##",
    "MMM `MM| MMM  MMM###HMMM  MMM     MMM\"\"\"\"\"MMM    .o###MMM  MM|       MMMMMT  ,dMMMMMMMMMMMMMH*'\"\"  ",
    "MMM  HML MMM  MMM#######  MMM     MMM     MMM   ,MMM##MMM  MM|       MMMMMb  `*###*\"\"\"'  9MMM      ",
    "MMM  |MMMMMM  HMM.        MMM     MMM     MMM  ,MM*'  MMM  MMH       MMM|HMM.            |MMM|     ",
    "MMM   |MMMMM  |MMH    ?b  MMM     MMM     MMM  |MM\\ ,dMMM  TMMb,  d  MMM |MMH            `MMM|     ",
    "MMM   `MMMMM   9MMMMMMMH  `MMMM|  MMM     MMM  `9MMMMPMMM   \"MMM#MM  MMM  9MMb,           #MMM     ",
    "###    ?####   `\"#####'    ?###|  ###     ###    `##' ###    \"####?  ###  `###b            *#*     ",
};


static char **
init_game_paths(const char *argv0)
{
#ifdef WIN32
    char dirbuf[1024], *pos;
#endif
    const char *pathlist[PREFIX_COUNT];
    char **pathlist_copy = malloc(sizeof (char *) * PREFIX_COUNT);
    const char *dir = NULL;
    char *tmp = 0;
    int i;

#if defined(UNIX)
    if (getgid() == getegid()) {
        dir = getenv("NETHACKDIR");
        if (!dir)
            dir = getenv("HACKDIR");
    } else
        dir = NULL;

    if (!dir || !*dir)
        dir = aimake_get_option("gamesdatadir");

    if (!dir || !*dir)
        dir = DEFAULT_NETHACKDIR;

    for (i = 0; i < PREFIX_COUNT; i++)
        pathlist[i] = dir;

    pathlist[DUMPPREFIX] = tmp = malloc(BUFSZ);
    if (!get_gamedir(DUMP_DIR, tmp)) {
        pathlist[DUMPPREFIX] = getenv("HOME");
        if (!pathlist[DUMPPREFIX])
            pathlist[DUMPPREFIX] = "./";
    }
#elif defined(WIN32)
    dir = getenv("NETHACKDIR");
    if (!dir || !*dir)
        dir = aimake_get_option("gamesdatadir");
    if (!dir || !*dir) {
        strncpy(dirbuf, argv0, 1023);
        pos = strrchr(dirbuf, '\\');
        if (!pos)
            pos = strrchr(dirbuf, '/');
        if (!pos) {
            /* argv0 doesn't contain a path */
            strcpy(dirbuf, ".\\");
            pos = &dirbuf[1];
        }
        *(++pos) = '\0';
        dir = dirbuf;
    }

    for (i = 0; i < PREFIX_COUNT; i++)
        pathlist[i] = dir;


    pathlist[DUMPPREFIX] = tmp = malloc(MAX_PATH);
    if (!get_gamedirA(DUMP_DIR, tmp)) {
        /* get the actual, localized path to the Documents folder */
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, tmp)))
            pathlist[DUMPPREFIX] = tmp;
        else
            pathlist[DUMPPREFIX] = ".\\";
    }

#else
    /* WIN32 / UNIX is set by the header files, /but/ those aren't included
       during dependency calculation. We want to error out if neither is set,
       but not in a way that will confuse dependencies. Thus, we want something
       here that the preprocessor is OK with but the compiler will reject. A
       negative-size array is the standard "portable" way to do a static assert,
       like is needed here (it works in practice, albeit possibly not in
       theory). */

    int please_define_UNIX_or_WIN32[-1];
#endif

    /* If the build system gave us more specific directories, use them. */
    const char *temp_path;

    temp_path = aimake_get_option("gamesstatedir");
    if (temp_path) {
        pathlist[BONESPREFIX] = temp_path;
        pathlist[SCOREPREFIX] = temp_path;
        pathlist[TROUBLEPREFIX] = temp_path;
    }

    temp_path = aimake_get_option("specificlockdir");
    if (temp_path)
        pathlist[LOCKPREFIX] = temp_path;
    /* and leave NETHACKDIR to provide the data */

    /* if given an override directory, use it (unless we're running setgid) */
#ifdef UNIX
    if (getgid() == getegid()) {
#endif
        if (override_hackdir) {
            /* player-nonspecific */
            pathlist[BONESPREFIX] = override_hackdir;
            pathlist[DATAPREFIX] = override_hackdir;
            pathlist[SCOREPREFIX] = override_hackdir;
            pathlist[LOCKPREFIX] = override_hackdir;
            pathlist[TROUBLEPREFIX] = override_hackdir;
        }
        if (override_userdir) {
            /* player-specific */
            pathlist[DUMPPREFIX] = override_userdir;
            /* config and save are also player-specific, but we don't pass
               those as filenames to the engine; rather, get_gamedir looks at
               them */
        }
#ifdef UNIX
    }
#endif

    /* alloc memory for the paths and append slashes as required */
    for (i = 0; i < PREFIX_COUNT; i++) {
        pathlist_copy[i] = malloc(strlen(pathlist[i]) + 2);
        strcpy(pathlist_copy[i], pathlist[i]);
        append_slash(pathlist_copy[i]);
    }

    free(tmp);

    return pathlist_copy;
}

#define str_macro(val) #val
static void
mainmenu(void)
{
    int menuresult[1];
    int n = 1, logoheight, i;
    const char *const *copybanner = nh_get_copyright_banner();
    const char **nhlogo;
    char verstr[32];
    nh_bool first = TRUE;

    snprintf(verstr, ARRAY_SIZE(verstr), "Version %d.%d.%d", VERSION_MAJOR, VERSION_MINOR,
            PATCHLEVEL);

#if defined(NETCLIENT)
    if (ui_flags.connection_only) {
        netgame();
        return;
    }
#endif

    load_keymap(); /* netgame() assumes the keymap isn't loaded */

    while (n >= 0) {
        if (COLS >= 100) {
            nhlogo = nhlogo_large;
            logoheight = sizeof (nhlogo_large) / sizeof (nhlogo_large[0]);
        } else {
            nhlogo = nhlogo_small;
            logoheight = sizeof (nhlogo_small) / sizeof (nhlogo_small[0]);
        }
        wclear(basewin);
        wattron(basewin, A_BOLD | COLOR_PAIR(4));
        for (i = 0; i < logoheight; i++) {
            wmove(basewin, i, (COLS - strlen(nhlogo[0])) / 2);
            if (nhlogo[i])
                waddstr(basewin, nhlogo[i]);
        }
        wattroff(basewin, A_BOLD | COLOR_PAIR(4));
        mvwaddstr(basewin, LINES - 3, 0, copybanner[0]);
        mvwaddstr(basewin, LINES - 2, 0, copybanner[1]);
        mvwaddstr(basewin, LINES - 1, 0, copybanner[2]);
        mvwaddstr(basewin, LINES - 4, COLS - strlen(verstr), verstr);
        wnoutrefresh(basewin);

        if (first) {
            network_motd();
            first = FALSE;
        }

        menuresult[0] = EXITGAME;       /* default action */
        if (!override_hackdir)
            curses_display_menu_core(
                STATIC_MENULIST(mainmenu_items), NULL, PICK_ONE,
                menuresult, curses_menu_callback,
                0, logoheight - 1, COLS, LINES - 3, FALSE, NULL, FALSE);
        else
            curses_display_menu_core(
                STATIC_MENULIST(mainmenu_items_noclient), NULL, PICK_ONE,
                menuresult, curses_menu_callback,
                0, logoheight - 1, COLS, LINES - 3, FALSE, NULL, FALSE);

        if (*menuresult == CURSES_MENU_CANCELLED && !ui_flags.done_hup)
            continue;

        switch (menuresult[0]) {
        case NEWGAME:
            rungame(FALSE);
            break;

        case LOAD:
            loadgame();
            break;

        case REPLAY:
            replay();
            break;

        case OPTIONS:
            display_options(TRUE);
            break;

#if defined(NETCLIENT)
        case NETWORK:
            free_keymap(); /* don't use the local keymap for server play */
            netgame();
            load_keymap();
            break;
#endif

        case TOPTEN:
            show_topten(NULL, -1, FALSE, FALSE);
            break;

        case EXITGAME:
        case CURSES_MENU_CANCELLED: /* in case of hangup */
            n = -1;     /* simulate menu cancel */
            break;
        }
    }

    free_keymap();
}


int
main(int argc, char *argv[])
{
    srand(time(NULL));

    char **gamepaths;
    int i;
    nh_bool init_ok;

    umask(0777 & ~FCMASK);

    /* this can change argc and *argv, so must come first */
    initialize_uncursed(&argc, argv);

    process_args(argc, argv);   /* grab -U, -H, -k, --help early */
    init_options();
    gamepaths = init_game_paths(argv[0]);

    nh_lib_init(&curses_windowprocs, (const char * const*)gamepaths);
    init_curses_ui(gamepaths[DATAPREFIX]);
    init_ok = read_nh_config();
    for (i = 0; i < PREFIX_COUNT; i++)
        free(gamepaths[i]);
    free(gamepaths);


    process_args(argc, argv);   /* other command line options */
    init_displaychars();

    if (init_ok)
        mainmenu();
    else
        curses_msgwin("Could not initialize game options!", krc_notification);

    exit_curses_ui();
    nh_lib_exit();
    free_displaychars();

    exit(EXIT_SUCCESS);
     /*NOTREACHED*/ return 0;
}


static int
str2role(const struct nh_roles_info *ri, const char *str)
{
    int i, role = -1;

    for (i = 0; i < ri->num_roles && role == -1; i++)
        if (!strncasecmp(ri->rolenames_m[i], str, strlen(str)) ||
            (ri->rolenames_f[i] &&
             !strncasecmp(ri->rolenames_f[i], str, strlen(str))))
            role = i;

    return role;
}


static int
str2race(const struct nh_roles_info *ri, const char *str)
{
    int i, race = -1;

    for (i = 0; i < ri->num_races && race == -1; i++)
        if (!strncasecmp(ri->racenames[i], str, strlen(str)))
            race = i;

    return race;
}


static void
process_args(int argc, char *argv[])
{
    int i;
    const struct nh_roles_info *ri = nh_get_roles();

    /*
     * Process options.
     */
    while (argc > 1 && argv[1][0] == '-') {
        argv++;
        argc--;
        switch (argv[0][1]) {
        case '-':
            if (!strcmp(argv[0], "--help")) {
                puts("Usage: nethack4 [--interface PLUGIN] [OPTIONS]");
                puts("");
                puts("-k          connection-only mode");
                puts("-D          start games in wizard mode");
                puts("-X          start games in explore mode");
                puts("-u name     specify player name");
                puts("-p role     specify role");
                puts("-r race     specify race");
                puts("-@          specify a random character");
                puts("-H dir      override the playfield location");
                puts("-U dir      override the user directory");
                puts("-Z          disable suspending the process");
                puts("");
                puts("PLUGIN can be any libuncursed plugin that is installed");
                puts("on your system; examples may include 'tty' and 'sdl'.");
                exit(0);
            } else if (!strcmp(argv[0], "--version")) {
                printf("NetHack 4 version %d.%d.%d\n",
                       VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
                exit(0);
            }
            break;

        case 'k':
            ui_flags.connection_only = 1;
            break;

        case 'D':
            ui_flags.playmode = MODE_WIZARD;
            break;

        case 'X':
            ui_flags.playmode = MODE_EXPLORE;
            break;

        case 'u':
            if (argv[0][2])
                strncpy(cmdline_name, argv[0] + 2,
                        sizeof (cmdline_name) - 1);
            else if (argc > 1) {
                argc--;
                argv++;
                strncpy(cmdline_name, argv[0], sizeof (cmdline_name) - 1);
            } else
                printf("Player name expected after -u");
            break;

        case 'p':      /* profession (role) */
            if (argv[0][2]) {
                i = str2role(ri, &argv[0][2]);
                if (i >= 0)
                    cmdline_role = i;
            } else if (argc > 1) {
                argc--;
                argv++;
                i = str2role(ri, argv[0]);
                if (i >= 0)
                    cmdline_role = i;
            }
            break;

        case 'r':      /* race */
            if (argv[0][2]) {
                i = str2race(ri, &argv[0][2]);
                if (i >= 0)
                    cmdline_race = i;
            } else if (argc > 1) {
                argc--;
                argv++;
                i = str2race(ri, argv[0]);
                if (i >= 0)
                    cmdline_race = i;
            }
            break;

        case '@':
            random_player = TRUE;
            break;

        case 'H':
#ifdef UNIX
            if (setregid(getgid(), getgid()) < 0)
                exit(14);
#endif
            if (argv[0][2]) {
                override_hackdir = argv[0] + 2;
            } else if (argc > 1) {
                argc--;
                argv++;
                override_hackdir = argv[0];
            }
            break;

        case 'U':
#ifdef UNIX
            if (setregid(getgid(), getgid()) < 0)
                exit(14);
#endif
            if (argv[0][2]) {
                override_userdir = argv[0] + 2;
            } else if (argc > 1) {
                argc--;
                argv++;
                override_userdir = argv[0];
            }
            break;

        case 'Z':
            ui_flags.no_stop = 1;
            break;

        default:
            i = str2role(ri, argv[0]);
            if (i >= 0)
                cmdline_role = i;
        }
    }
}


/*
* Add a slash to any name not ending in /. There must
* be room for the /
*/
void
append_slash(char *name)
{
#if defined(WIN32)
    static const char dirsep = '\\';
#else
    static const char dirsep = '/';
#endif
    char *ptr;

    if (!*name)
        return;
    ptr = name + (strlen(name) - 1);
    if (*ptr != dirsep) {
        *++ptr = dirsep;
        *++ptr = '\0';
    }
    return;
}


void
curses_impossible(const char *msg)
{
    curses_msgwin("Impossible state detected in the client.", krc_notification);
    curses_msgwin(msg, krc_notification);
    curses_msgwin("You may wish to save and restart the client.",
                  krc_notification);
}

/* main.c */
