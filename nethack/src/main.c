/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <signal.h>


static void process_args(int, char **);
void append_slash(char *name);

struct settings settings;
struct interface_flags ui_flags;
nh_bool interrupt_multi = FALSE;
nh_bool game_is_running = FALSE;
int initrole = ROLE_NONE, initrace = ROLE_NONE;
int initgend = ROLE_NONE, initalign = ROLE_NONE;
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
    {NEWGAME, MI_NORMAL, "new game", 'n'},
    {LOAD, MI_NORMAL, "load game", 'l'},
    {REPLAY, MI_NORMAL, "view replay", 'v'},
    {OPTIONS, MI_NORMAL, "set options", 'o'},
    {TOPTEN, MI_NORMAL, "show score list", 's'},
#if defined(NETCLIENT)
    {NETWORK, MI_NORMAL, "connect to server", 'c'},
#endif
    {EXITGAME, MI_NORMAL, "quit", 'q', 'x'}
};

static struct nh_menuitem mainmenu_items_noclient[] = {
    {NEWGAME, MI_NORMAL, "new game", 'n'},
    {LOAD, MI_NORMAL, "load game", 'l'},
    {REPLAY, MI_NORMAL, "view replay", 'v'},
    {OPTIONS, MI_NORMAL, "set options", 'o'},
    {TOPTEN, MI_NORMAL, "show score list", 's'},
    {EXITGAME, MI_NORMAL, "quit", 'q', 'x'}
};

const char *nhlogo_small[12] = { /* created using pbmtoascii */
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
#ifdef UNIX

/* the terminal went away - do not pass go, etc. */
static void sighup_handler(int signum)
{
    if (!ui_flags.done_hup++)
	nh_exit_game(EXIT_FORCE_SAVE);
    nh_lib_exit();
    exit(0);
}

static void sigint_handler(int signum)
{
    if (!game_is_running)
	return;
    
    nh_exit_game(EXIT_REQUEST_SAVE);
}

static void setup_signals(void)
{
    signal(SIGHUP, sighup_handler); 
    signal(SIGTERM, sighup_handler);
# ifdef SIGXCPU
    signal(SIGXCPU, sighup_handler);
# endif
    signal(SIGQUIT,SIG_IGN);
    
    
    signal(SIGINT, sigint_handler);
}

#else
static void setup_signals(void) {}
#endif

static char** init_game_paths(const char *argv0)
{
#ifdef WIN32
    char dirbuf[1024], docpath[MAX_PATH], *pos;
#endif
    char **pathlist = malloc(sizeof(char*) * PREFIX_COUNT);
    char *dir = NULL;
    int i;
    
#if defined(UNIX)
    if (getgid() == getegid()) {
	dir = getenv("NETHACKDIR");
	if (!dir)
	    dir = getenv("HACKDIR");
    } else
	dir = NULL;
    
    if (!dir)
	dir = NETHACKDIR;
    
    for (i = 0; i < PREFIX_COUNT; i++)
	pathlist[i] = dir;

#ifndef STRINGIFY_OPTION
#define STRINGIFY_OPTION(x) STRINGIFY_OPTION_1(x)
#define STRINGIFY_OPTION_1(x) #x
#endif
#ifdef AIMAKE_OPTION_statedir
    pathlist[BONESPREFIX] = STRINGIFY_OPTION(AIMAKE_OPTION_statedir);
    pathlist[SCOREPREFIX] = STRINGIFY_OPTION(AIMAKE_OPTION_statedir);
    pathlist[TROUBLEPREFIX] = STRINGIFY_OPTION(AIMAKE_OPTION_statedir);
#endif
#ifdef AIMAKE_OPTION_specificlockdir
    pathlist[LOCKPREFIX] = STRINGIFY_OPTION(AIMAKE_OPTION_specificlockdir);
#endif
    /* and leave HACKDIR to provide the data */

    pathlist[DUMPPREFIX] = malloc(BUFSZ);
    if (!get_gamedir(DUMP_DIR, pathlist[DUMPPREFIX])) {
      free(pathlist[DUMPPREFIX]);
      pathlist[DUMPPREFIX] = getenv("HOME");
      if (!pathlist[DUMPPREFIX])
	pathlist[DUMPPREFIX] = "./";
    }
    
#elif defined(WIN32)
    dir = getenv("NETHACKDIR");
    if (!dir) {
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
    /* get the actual, localized path to the Documents folder */
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docpath)))
	pathlist[DUMPPREFIX] = docpath;
    else
	pathlist[DUMPPREFIX] = ".\\";
#else
    /* avoid a trap for people trying to port this */
#error You must run NetHack 4 under Win32 or Linux.
#endif

    /* if given an override directory, use it (unless we're running
       setgid */
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
            /* config and save are also player-specific, but we don't
               pass those as filenames to the engine; rather,
               get_gamedir looks at them */
        }
#ifdef UNIX
    }
#endif
    
    /* alloc memory for the paths and append slashes as required */
    for (i = 0; i < PREFIX_COUNT; i++) {
	char *tmp = pathlist[i];
	pathlist[i] = malloc(strlen(tmp) + 2);
	strcpy(pathlist[i], tmp);
	append_slash(pathlist[i]);
    }
    
    return pathlist;
}

#define str_macro(val) #val
static void mainmenu(void)
{
    int menuresult[1];
    int n = 1, logoheight, i;
    const char * const *copybanner = nh_get_copyright_banner();
    const char **nhlogo;
    char verstr[32];
    sprintf(verstr, "Version %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);

    if (ui_flags.connection_only) {
        netgame();
        return;
    }
    
    while (n > 0) {
	if (COLS >= 100) {
	    nhlogo = nhlogo_large;
	    logoheight = sizeof(nhlogo_large) / sizeof(nhlogo_large[0]);
	} else {
	    nhlogo = nhlogo_small;
	    logoheight = sizeof(nhlogo_small) / sizeof(nhlogo_small[0]);
        }
	wclear(basewin);
	wattron(basewin, A_BOLD | COLOR_PAIR(4));
	for (i = 0; i < logoheight; i++) {
	    wmove(basewin, i, (COLS - strlen(nhlogo[0])) / 2);
	    waddstr(basewin, nhlogo[i]);
	}
	wattroff(basewin, A_BOLD | COLOR_PAIR(4));
	mvwaddstr(basewin, LINES-3, 0, copybanner[0]);
	mvwaddstr(basewin, LINES-2, 0, copybanner[1]);
	mvwaddstr(basewin, LINES-1, 0, copybanner[2]);
	mvwaddstr(basewin, LINES-4, COLS - strlen(verstr), verstr);
	wrefresh(basewin);

	menuresult[0] = EXITGAME; /* default action */
        if (!override_hackdir)
            n = curses_display_menu_core(mainmenu_items, ARRAY_SIZE(mainmenu_items),
                                         NULL, PICK_ONE, menuresult, 0, logoheight-1,
                                         COLS, LINES-3, NULL);
        else
            n = curses_display_menu_core(mainmenu_items_noclient,
                                         ARRAY_SIZE(mainmenu_items_noclient),
                                         NULL, PICK_ONE, menuresult, 0, logoheight-1,
                                         COLS, LINES-3, NULL);

	
	switch (menuresult[0]) {
	    case NEWGAME:
		rungame();
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
		netgame();
		break;
#endif
		
	    case TOPTEN:
		show_topten(NULL, -1, FALSE, FALSE);
		break;
		
	    case EXITGAME:
		return;
	}
    }
}


int main(int argc, char *argv[])
{
    char **gamepaths;
    int i;
    
    umask(0777 & ~FCMASK);

    process_args(argc, argv);	/* grab -U, -H, -k early */
    
    init_options();
    
    gamepaths = init_game_paths(argv[0]);
    nh_lib_init(&curses_windowprocs, gamepaths);
    for (i = 0; i < PREFIX_COUNT; i++)
	free(gamepaths[i]);
    free(gamepaths);
    
    setup_signals();
    init_curses_ui();
    read_nh_config();

    process_args(argc, argv);	/* other command line options */
    init_displaychars();
    
    mainmenu();
    
    exit_curses_ui();
    nh_lib_exit();
    free_displaychars();

    exit(EXIT_SUCCESS);
    /*NOTREACHED*/
    return 0;
}


static int str2role(const struct nh_roles_info *ri, const char *str)
{
    int i, role = -1;
    
    for (i = 0; i < ri->num_roles && role == -1; i++)
	if (!strncasecmp(ri->rolenames_m[i], str, strlen(str)) ||
	    (ri->rolenames_f[i] && !strncasecmp(ri->rolenames_f[i], str, strlen(str))))
	    role = i;

    return role;
}


static int str2race(const struct nh_roles_info *ri, const char *str)
{
    int i, race = -1;
    
    for (i = 0; i < ri->num_races && race == -1; i++)
	if (!strncasecmp(ri->racenames[i], str, strlen(str)))
	    race = i;

    return race;
}


static void process_args(int argc, char *argv[])
{
    int i;
    const struct nh_roles_info *ri = nh_get_roles();

    /*
     * Process options.
     */
    while (argc > 1 && argv[1][0] == '-'){
	argv++;
	argc--;
	switch(argv[0][1]){
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
		strncpy(settings.plname, argv[0]+2, sizeof(settings.plname)-1);
	    else if (argc > 1) {
		argc--;
		argv++;
		strncpy(settings.plname, argv[0], sizeof(settings.plname)-1);
	    } else
		    printf("Player name expected after -u");
	    break;
	    
	case 'p': /* profession (role) */
	    if (argv[0][2]) {
		i = str2role(ri, &argv[0][2]);
		if (i >= 0)
		    initrole = i;
	    } else if (argc > 1) {
		argc--;
		argv++;
		i = str2role(ri, argv[0]);
		if (i >= 0)
		    initrole = i;
	    }
	    break;
	    
	case 'r': /* race */
	    if (argv[0][2]) {
		i = str2race(ri, &argv[0][2]);
		if (i >= 0)
		    initrace = i;
	    } else if (argc > 1) {
		argc--;
		argv++;
		i = str2race(ri, argv[0]);
		if (i >= 0)
		    initrace = i;
	    }
	    break;
	    
	case '@':
	    random_player = TRUE;
	    break;

        case 'H':
            if (argv[0][2]) {
                override_hackdir = argv[0] + 2;
            } else if (argc > 1) {
                argc--;
                argv++;
                override_hackdir = argv[0];
            }
            break;

        case 'U':
            if (argv[0][2]) {
                override_userdir = argv[0] + 2;
            } else if (argc > 1) {
                argc--;
                argv++;
                override_userdir = argv[0];
            }
            break;

        case 'Z':
            ui_flags.no_stop = true;
            break;
	    
	default:
	    i = str2role(ri, argv[0]);
	    if (i >= 0)
		initrole = i;
	}
    }
}


/*
* Add a slash to any name not ending in /. There must
* be room for the /
*/
void append_slash(char *name)
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

/* main.c */
