/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "config.h"
#include "nethack.h"
#include "dlb.h"

#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#ifndef O_RDONLY
#include <fcntl.h>
#endif

#include "wintty.h"

extern struct passwd *getpwuid(uid_t);
extern struct passwd *getpwnam(const char *);
static boolean whoami(void);
static void process_options(int, char **);
void append_slash(char *name);

#ifdef __linux__
extern void check_linux_console(void);
extern void init_linux_cons(void);
#endif

static boolean wiz_error_flag = FALSE;
int locknum = 0;		/* max num of simultaneous users */
static char plname[PL_NSIZ] = "\0";
char *hackdir, *var_playground;
int hackpid;


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
	
	if (var_playground) {
	    pathlist[SCOREPREFIX] = var_playground;
	    pathlist[LEVELPREFIX] = var_playground;
	    pathlist[SAVEPREFIX] = var_playground;
	    pathlist[BONESPREFIX] = var_playground;
	    pathlist[LOCKPREFIX] = var_playground;
	    pathlist[TROUBLEPREFIX] = var_playground;
	}

	/* alloc memory for the paths and append slashes as required */
	for (i = 0; i < PREFIX_COUNT; i++) {
	    char *tmp = pathlist[i];
	    pathlist[i] = malloc(strlen(tmp) + 1);
	    strcpy(pathlist[i], tmp);
	    append_slash(pathlist[i]);
	}
	
	return pathlist;
}


EXPORT int main(int argc, char *argv[])
{
	char **gamepaths;
	boolean exact_username;

	hackpid = getpid();
	umask(0777 & ~FCMASK);
	
	tty_init_options();
	gamepaths = init_game_paths();

	win_tty_init();
	nh_init(hackpid, &tty_procs, gamepaths);

	if(argc > 1) {
	    /*
	     * Now we know the directory containing 'record' and
	     * may do a prscore().  Exclude `-style' - it's a Qt option.
	     */
	    if (!strncmp(argv[1], "-s", 2) && strncmp(argv[1], "-style", 6)) {
		prscore(argv[0],argc, argv);
		exit(EXIT_SUCCESS);
	    }
	}

#ifdef __linux__
	check_linux_console();
#endif
	tty_init_nhwindows(&argc,argv);
	exact_username = whoami();
#ifdef __linux__
	init_linux_cons();
#endif
	read_config();

	process_options(argc, argv);	/* command line options */
	
	nethack_start_game(plname, exact_username, wiz_error_flag, getlock);

	moveloop();
	exit(EXIT_SUCCESS);
	/*NOTREACHED*/
	return 0;
}

static void process_options(int argc, char *argv[])
{
	int i;

	/*
	 * Process options.
	 */
	while(argc > 1 && argv[1][0] == '-'){
		argv++;
		argc--;
		switch(argv[0][1]){
		case 'D':
			{
			  char *user;
			  int uid;
			  struct passwd *pw = (struct passwd *)0;

			  uid = getuid();
			  user = getlogin();
			  if (user) {
			      pw = getpwnam(user);
			      if (pw && (pw->pw_uid != uid)) pw = 0;
			  }
			  if (pw == 0) {
			      user = nh_getenv("USER");
			      if (user) {
				  pw = getpwnam(user);
				  if (pw && (pw->pw_uid != uid)) pw = 0;
			      }
			      if (pw == 0) {
				  pw = getpwuid(uid);
			      }
			  }
			  if (pw && !strcmp(pw->pw_name,WIZARD)) {
			      enter_wizard_mode();
			      break;
			  }
			}
			/* otherwise fall thru to discover */
			wiz_error_flag = TRUE;
		case 'X':
			enter_discover_mode();
			break;
#ifdef NEWS
		case 'n':
			iflags.news = FALSE;
			break;
#endif
		case 'u':
			if(argv[0][2])
			  strncpy(plname, argv[0]+2, sizeof(plname)-1);
			else if(argc > 1) {
			  argc--;
			  argv++;
			  strncpy(plname, argv[0], sizeof(plname)-1);
			} else
				tty_raw_print("Player name expected after -u");
			break;
		case 'I':
		case 'i':
			if (!strncmpi(argv[0]+1, "IBM", 3))
				switch_graphics(IBM_GRAPHICS);
			break;
	    /*  case 'D': */
		case 'd':
			if (!strncmpi(argv[0]+1, "DEC", 3))
				switch_graphics(DEC_GRAPHICS);
			break;
		case 'p': /* profession (role) */
			if (argv[0][2]) {
			    if ((i = str2role(&argv[0][2])) >= 0)
			    	set_role(i);
			} else if (argc > 1) {
				argc--;
				argv++;
			    if ((i = str2role(argv[0])) >= 0)
			    	set_role(i);
			}
			break;
		case 'r': /* race */
			if (argv[0][2]) {
			    if ((i = str2race(&argv[0][2])) >= 0)
			    	set_race(i);
			} else if (argc > 1) {
				argc--;
				argv++;
			    if ((i = str2race(argv[0])) >= 0)
			    	set_race(i);
			}
			break;
		case '@':
			set_random_player();
			break;
		default:
			if ((i = str2role(&argv[0][1])) >= 0) {
			    set_role(i);
				break;
			}
			/* else raw_printf("Unknown option: %s", *argv); */
		}
	}

	if(argc > 1)
		locknum = atoi(argv[1]);
#ifdef MAX_NR_OF_PLAYERS
	if(!locknum || locknum > MAX_NR_OF_PLAYERS)
		locknum = MAX_NR_OF_PLAYERS;
#endif
}


static boolean whoami(void)
{
	/*
	 * Who am i? Algorithm: 1. Use name as specified in NETHACKOPTIONS
	 *			2. Use $USER or $LOGNAME	(if 1. fails)
	 *			3. Use getlogin()		(if 2. fails)
	 * The resulting name is overridden by command line options.
	 * If everything fails, or if the resulting name is some generic
	 * account like "games", "play", "player", "hack" then eventually
	 * we'll ask him.
	 * Note that we trust the user here; it is possible to play under
	 * somebody else's name.
	 */
	char *s;

	if (*plname) return FALSE;
	if(/* !*plname && */ (s = nh_getenv("USER")))
		strncpy(plname, s, sizeof(plname)-1);
	if(!*plname && (s = nh_getenv("LOGNAME")))
		strncpy(plname, s, sizeof(plname)-1);
	if(!*plname && (s = getlogin()))
		strncpy(plname, s, sizeof(plname)-1);
	return TRUE;
}

#ifdef PORT_HELP
void port_help(void)
{
	/*
	 * Display unix-specific help.   Just show contents of the helpfile
	 * named by PORT_HELP.
	 */
	display_file(PORT_HELP, TRUE);
}
#endif

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
