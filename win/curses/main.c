/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
#include <signal.h>

#include "nhcurses.h"

static void process_args(int, char **);
void append_slash(char *name);

struct settings settings;
struct interface_flags ui_flags;
char *hackdir;
boolean interrupt_multi = FALSE;
boolean game_is_running = FALSE;
int initrole = ROLE_NONE, initrace = ROLE_NONE;
int initgend = ROLE_NONE, initalign = ROLE_NONE;
boolean random_player = FALSE;

enum menuitems {
    NEWGAME = 1,
    LOAD,
    TOPTEN,
    EXITGAME
};

struct nh_menuitem mainmenu_items[] = {
    {NEWGAME, MI_NORMAL, "new game", 'n'},
    {LOAD, MI_NORMAL, "load game", 'l'},
    {TOPTEN, MI_NORMAL, "show score list", 's'},
    {EXITGAME, MI_NORMAL, "quit", 'q', 'x'}
};


const char *nhlogo[] = { /* this _beautiful_ logo was created by exporting an
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
	nh_exit(EXIT_FORCE_SAVE);
}

static void sigint_handler(int signum)
{
    if (!game_is_running)
	return;
    
    if (settings.ignintr)
	interrupt_multi = TRUE;
    else
	nh_exit(EXIT_REQUEST_QUIT);
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
    
    dir = getenv("NETHACKDIR");
    if (!dir)
	dir = getenv("HACKDIR");
    
    if (!dir)
	dir = hackdir;

    if (!dir)
	dir = NETHACKDIR;
    
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
	wclear(stdscr);
	wattron(stdscr, A_BOLD | COLOR_PAIR(4));
	for (i = 0; i < logoheight; i++) {
	    wmove(stdscr, i, (COLS - strlen(nhlogo[0])) / 2);
	    waddstr(stdscr, nhlogo[i]);
	}
	wattroff(stdscr, A_BOLD | COLOR_PAIR(4));
	mvwaddstr(stdscr, LINES-3, 0, copybanner[0]);
	mvwaddstr(stdscr, LINES-2, 0, copybanner[1]);
	mvwaddstr(stdscr, LINES-1, 0, copybanner[2]);
	wrefresh(stdscr);

	menuresult[0] = EXITGAME; /* default action */
	n = curses_display_menu_core(mainmenu_items, 4, NULL, PICK_ONE,
				     menuresult, 0, logoheight, COLS, ROWNO+3, NULL);
	
	wclear(stdscr);
	wrefresh(stdscr);
	
	switch (menuresult[0]) {
	    case NEWGAME:
		rungame();
		break;
		
	    case LOAD:
		loadgame();
		break;
		
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
    nh_init(&curses_windowprocs, gamepaths);
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
    nh_exit(0);
    free_displaychars();

    exit(EXIT_SUCCESS);
    /*NOTREACHED*/
    return 0;
}


static void process_args(int argc, char *argv[])
{
    int i;

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
		if ((i = nh_str2role(&argv[0][2])) >= 0)
		    initrole = i;
	    } else if (argc > 1) {
		    argc--;
		    argv++;
		if ((i = nh_str2role(argv[0])) >= 0)
		    initrole = i;
	    }
	    break;
	    
	case 'r': /* race */
	    if (argv[0][2]) {
		if ((i = nh_str2race(&argv[0][2])) >= 0)
		    initrace = i;
	    } else if (argc > 1) {
		    argc--;
		    argv++;
		if ((i = nh_str2race(argv[0])) >= 0)
		    initrace = i;
	    }
	    break;
	    
	case '@':
	    random_player = TRUE;
	    break;
	    
	default:
	    if ((i = nh_str2role(&argv[0][1])) >= 0) {
		initrole = i;
		    break;
	    }
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
