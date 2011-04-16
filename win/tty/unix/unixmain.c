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
#ifdef CHDIR
static void chdirx(const char *,boolean);
#endif /* CHDIR */
static boolean whoami(void);
static void process_options(int, char **);

#ifdef __linux__
extern void check_linux_console(void);
extern void init_linux_cons(void);
#endif

static boolean wiz_error_flag = FALSE;
int locknum = 0;		/* max num of simultaneous users */
static char plname[PL_NSIZ] = "\0";

int EXPORT main(int argc, char *argv[])
{
#ifdef CHDIR
	char *dir;
#endif
	boolean exact_username;

#if defined(__APPLE__)
	/* special hack to change working directory to a resource fork when
	   running from finder --sam */
#define MAC_PATH_VALUE ".app/Contents/MacOS/"
	char mac_cwd[1024], *mac_exe = argv[0], *mac_tmp;
	int arg0_len = strlen(mac_exe), mac_tmp_len, mac_lhs_len=0;
	getcwd(mac_cwd, 1024);
	if(mac_exe[0] == '/' && !strcmp(mac_cwd, "/")) {
	    if((mac_exe = strrchr(mac_exe, '/')))
		mac_exe++;
	    else
		mac_exe = argv[0];
	    mac_tmp_len = (strlen(mac_exe) * 2) + strlen(MAC_PATH_VALUE);
	    if(mac_tmp_len <= arg0_len) {
		mac_tmp = malloc(mac_tmp_len + 1);
		sprintf(mac_tmp, "%s%s%s", mac_exe, MAC_PATH_VALUE, mac_exe);
		if(!strcmp(argv[0] + (arg0_len - mac_tmp_len), mac_tmp)) {
		    mac_lhs_len = (arg0_len - mac_tmp_len) + strlen(mac_exe) + 5;
		    if(mac_lhs_len > mac_tmp_len - 1)
			mac_tmp = realloc(mac_tmp, mac_lhs_len);
		    strncpy(mac_tmp, argv[0], mac_lhs_len);
		    mac_tmp[mac_lhs_len] = '\0';
		    chdir(mac_tmp);
		}
		free(mac_tmp);
	    }
	}
#endif

	hackpid = getpid();
	(void) umask(0777 & ~FCMASK);

	win_tty_init();
	nethack_init(&tty_procs);

#ifdef CHDIR			/* otherwise no chdir() */
	/*
	 * See if we must change directory to the playground.
	 * (Perhaps hack runs suid and playground is inaccessible
	 *  for the player.)
	 * The environment variable HACKDIR is overridden by a
	 *  -d command line option (must be the first option given)
	 */
	dir = nh_getenv("NETHACKDIR");
	if (!dir) dir = nh_getenv("HACKDIR");
#endif
	if(argc > 1) {
#ifdef CHDIR
	    if (!strncmp(argv[1], "-d", 2) && argv[1][2] != 'e') {
		/* avoid matching "-dec" for DECgraphics; since the man page
		 * says -d directory, hope nobody's using -desomething_else
		 */
		argc--;
		argv++;
		dir = argv[0]+2;
		if(*dir == '=' || *dir == ':') dir++;
		if(!*dir && argc > 1) {
			argc--;
			argv++;
			dir = argv[0];
		}
		if(!*dir)
		    error("Flag -d must be followed by a directory name.");
	    }
	    if (argc > 1)
#endif /* CHDIR */

	    /*
	     * Now we know the directory containing 'record' and
	     * may do a prscore().  Exclude `-style' - it's a Qt option.
	     */
	    if (!strncmp(argv[1], "-s", 2) && strncmp(argv[1], "-style", 6)) {
#ifdef CHDIR
		chdirx(dir,0);
#endif
		prscore(argv[0],argc, argv);
		exit(EXIT_SUCCESS);
	    }
	}

	/*
	 * Change directories before we initialize the window system so
	 * we can find the tile file.
	 */
#ifdef CHDIR
	chdirx(dir,1);
#endif

#ifdef __linux__
	check_linux_console();
#endif
	initoptions();
	tty_init_nhwindows(&argc,argv);
	exact_username = whoami();
#ifdef __linux__
	init_linux_cons();
#endif

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
			  (void) strncpy(plname, argv[0]+2, sizeof(plname)-1);
			else if(argc > 1) {
			  argc--;
			  argv++;
			  (void) strncpy(plname, argv[0], sizeof(plname)-1);
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

#ifdef CHDIR
static void chdirx(const char *dir, boolean wr)
{
	if (dir					/* User specified directory? */
# ifdef HACKDIR
	       && strcmp(dir, HACKDIR)		/* and not the default? */
# endif
		) {
# ifdef SECURE
	    (void) setgid(getgid());
	    (void) setuid(getuid());		/* Ron Wessels */
# endif
	} else {
	    /* non-default data files is a sign that scores may not be
	     * compatible, or perhaps that a binary not fitting this
	     * system's layout is being used.
	     */
# ifdef VAR_PLAYGROUND
	    int len = strlen(VAR_PLAYGROUND);

	    fqn_prefix[SCOREPREFIX] = (char *)alloc(len+2);
	    strcpy(fqn_prefix[SCOREPREFIX], VAR_PLAYGROUND);
	    if (fqn_prefix[SCOREPREFIX][len-1] != '/') {
		fqn_prefix[SCOREPREFIX][len] = '/';
		fqn_prefix[SCOREPREFIX][len+1] = '\0';
	    }
# endif
	}

# ifdef HACKDIR
	if (dir == (const char *)0)
	    dir = HACKDIR;
# endif

	if (dir && chdir(dir) < 0) {
	    perror(dir);
	    error("Cannot chdir to %s.", dir);
	}

	/* warn the player if we can't write the record file */
	/* perhaps we should also test whether . is writable */
	/* unfortunately the access system-call is worthless */
	if (wr) {
# ifdef VAR_PLAYGROUND
	    fqn_prefix[LEVELPREFIX] = fqn_prefix[SCOREPREFIX];
	    fqn_prefix[SAVEPREFIX] = fqn_prefix[SCOREPREFIX];
	    fqn_prefix[BONESPREFIX] = fqn_prefix[SCOREPREFIX];
	    fqn_prefix[LOCKPREFIX] = fqn_prefix[SCOREPREFIX];
	    fqn_prefix[TROUBLEPREFIX] = fqn_prefix[SCOREPREFIX];
# endif
	    check_recordfile(dir);
	}
}
#endif /* CHDIR */

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
		(void) strncpy(plname, s, sizeof(plname)-1);
	if(!*plname && (s = nh_getenv("LOGNAME")))
		(void) strncpy(plname, s, sizeof(plname)-1);
	if(!*plname && (s = getlogin()))
		(void) strncpy(plname, s, sizeof(plname)-1);
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
