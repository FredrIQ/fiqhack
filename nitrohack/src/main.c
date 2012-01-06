/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NitroHack may be freely redistributed.  See license for details. */

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
char *hackdir;
nh_bool interrupt_multi = FALSE;
nh_bool game_is_running = FALSE;
int initrole = ROLE_NONE, initrace = ROLE_NONE;
int initgend = ROLE_NONE, initalign = ROLE_NONE;
nh_bool random_player = FALSE;

enum menuitems {
    NEWGAME = 1,
    LOAD,
    REPLAY,
    OPTIONS,
    TOPTEN,
    NETWORK,
    EXITGAME
};

struct nh_menuitem mainmenu_items[] = {
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


const char *nhlogo[12] = { /* this _beautiful_ logo was created by exporting an
                            160x24 image from gimp using aalib */
"]QQQ,   ]QQf             _aa/    ]QQf    ]QQf                       ]QQf       ",
"]QQQm   ]QQf             ]QQf    ]QQf    ]QQf                       ]QQf       ",
"]QQQQ/  ]QQf             ]QQf    ]QQf    ]QQf                       ]QQf       ",
"]QQ@Qm  ]QQf   _yQQmw.  QQQQQQQf ]QQf    ]QQf  ]mQQQmw,    _wmQQg/  ]QQf  yQQ[ ",
"]QQf$Q/ ]QQf  jQQQQQWm. QQQQQQQf ]QQf    ]QQf  ]QQQQQQm   <QQQQQQf  ]QQf jQQ[  ",
"]QQf]Wm ]QQf .QW@' ]QQL  ]QQf    ]QQQQQQQQQQf       4QQ( .QQQ!  \"[  ]QQfjQQ(   ",
"]QQf $Qc]QQf ]QQ6aaaWQm  ]QQf    ]QQQQQQQQQQf  _wmQQQQQf ]QQF       ]QQQQQ(    ",
"]QQf ]Wh]QQf ]QQQQWWQQQ  ]QQf    ]QQf    ]QQf _QQQQQQQQf ]QQf       ]QQQQQ,    ",
"]QQf  $QmQQf ]QQF\"\"\"\"\"^  ]QQf    ]QQf    ]QQf ]QQP  jQQf ]WQk       ]QQPQQm,   ",
"]QQf  )WQQQf -QWQ,.._a/  ]QQf    ]QQf    ]QQf ]QQk _mQQf -QQQc  <r  ]QQf)WQm,  ",
"]QQf   4QQQf  ]QQQQQQQf  +QQQQQ  ]QQf    ]QQf -QQQQQ$QQf  )QQQQQQf  ]QQf )QQm, ",
"]QQf   )QQQf   \"9WQWV!    ?$QQQ  ]QQf    ]QQf  )$W@!]QQf   \"9$QWU'  ]QQf  ]QQm,",
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

static char** init_game_paths(void)
{
    char **pathlist = malloc(sizeof(char*) * PREFIX_COUNT);
    char *dir;
    int i;
    
    dir = getenv("NITROHACKDIR");
    if (!dir)
	dir = getenv("HACKDIR");
    
    if (!dir)
	dir = hackdir;

    if (!dir)
	dir = NITROHACKDIR;
    
    for (i = 0; i < PREFIX_COUNT; i++)
	pathlist[i] = dir;
    
    /* alloc memory for the paths and append slashes as required */
    for (i = 0; i < PREFIX_COUNT; i++) {
	char *tmp = pathlist[i];
	pathlist[i] = malloc(strlen(tmp) + 2);
	strcpy(pathlist[i], tmp);
	append_slash(pathlist[i]);
    }
    
    strcpy(pathlist[DUMPPREFIX], "./");
    
    return pathlist;
}


static void mainmenu(void)
{
    int menuresult[1];
    int n = 1, logoheight, i;
    const char * const *copybanner = nh_get_copyright_banner();
    
    while (n > 0) {
	logoheight = sizeof(nhlogo) / sizeof(nhlogo[0]);
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
	wrefresh(basewin);

	menuresult[0] = EXITGAME; /* default action */
	n = curses_display_menu_core(mainmenu_items, ARRAY_SIZE(mainmenu_items),
				     NULL, PICK_ONE, menuresult, 0, logoheight,
				     COLS, ROWNO+3, NULL);
	
	wclear(basewin);
	wrefresh(basewin);
	
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
    
    init_options();
    
    gamepaths = init_game_paths();
    nh_lib_init(&curses_windowprocs, gamepaths);
    for (i = 0; i < PREFIX_COUNT; i++)
	free(gamepaths[i]);
    free(gamepaths);
    
    setup_signals();
    init_curses_ui();
    read_nh_config();

    process_args(argc, argv);	/* command line options */
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
    char *ptr;

    if (!*name)
	return;
    ptr = name + (strlen(name) - 1);
    if (*ptr != '/') {
	*++ptr = '/';
	*++ptr = '\0';
    }
    return;
}

/*unixmain.c*/
