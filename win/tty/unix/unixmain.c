/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#ifndef O_RDONLY
#include <fcntl.h>
#endif
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#include "nethack.h"
#include "wintty.h"

extern struct passwd *getpwuid(uid_t);
extern struct passwd *getpwnam(const char *);
static void whoami(char *name);
static void process_options(int, char **);
void append_slash(char *name);
int playmode = MODE_NORMAL;

#ifdef __linux__
extern void check_linux_console(void);
extern void init_linux_cons(void);
#endif

int locknum = 0;		/* max num of simultaneous users */
static char plname[PL_NSIZ] = "\0";
char *hackdir, *var_playground;
int hackpid;
static boolean interrupt_multi = FALSE;
static boolean game_is_running = FALSE;

#ifdef UNIX

static void sighup_handler(int signum)
{
	if (!ui_flags.done_hup++)
	    nh_exit(EXIT_FORCE_SAVE);
}

static void sigint_handler(int signum)
{
	if (!game_is_running)
	    return;
	
	if (ui_flags.ignintr)
	    interrupt_multi = TRUE;
	else
	    nh_exit(EXIT_REQUEST_QUIT);
	
	/* the question "Really quit?" moves the cursor off the player*/
	tty_curs(player.x, player.y);
}

static void setup_signals(void)
{
	signal(SIGHUP, sighup_handler);
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
	    pathlist[i] = malloc(strlen(tmp) + 2);
	    strcpy(pathlist[i], tmp);
	    append_slash(pathlist[i]);
	}
	
	return pathlist;
}


static void topten_print(struct nh_topten_entry *entry)
{
	char linebuf[BUFSZ];
	char *bp, hpbuf[24], linebuf3[BUFSZ];
	int hppos, lngr;
	
	sprintf(linebuf, "%4d %10d  %s", entry->rank, entry->points, entry->entrytxt);

	lngr = (int)strlen(linebuf);
	if (entry->hp <= 0) hpbuf[0] = '-', hpbuf[1] = '\0';
	else sprintf(hpbuf, "%d", entry->hp);
	/* beginning of hp column after padding (not actually padded yet) */
	hppos = COLNO - (sizeof("  Hp [max]")-1); /* sizeof(str) includes \0 */
	while (lngr >= hppos) {
	    bp = linebuf + strlen(linebuf);
	    while (!(*bp == ' ' && (bp-linebuf < hppos)))
		bp--;
	    /* special case: if about to wrap in the middle of maximum
		dungeon depth reached, wrap in front of it instead */
	    if (bp > linebuf + 5 && !strncmp(bp - 5, " [max", 5)) bp -= 5;
	    strcpy(linebuf3, bp+1);
	    *bp = 0;
	    
	    if (entry->highlight) {
		while (bp < linebuf + (COLNO-1)) *bp++ = ' ';
		*bp = 0;
		tty_raw_print_bold(linebuf);
	    } else
		tty_raw_print(linebuf);
	    sprintf(linebuf, "%16s %s", "", linebuf3);
	    lngr = strlen(linebuf);
	}
	/* beginning of hp column not including padding */
	hppos = COLNO - 7 - (int)strlen(hpbuf);
	bp = linebuf + strlen(linebuf);

	if (bp <= linebuf + hppos) {
	    /* pad any necessary blanks to the hit point entry */
	    while (bp < linebuf + hppos) *bp++ = ' ';
	    strcpy(bp, hpbuf);
	    sprintf(bp + strlen(bp), " %s[%d]",
		    (entry->maxhp < 10) ? "  " : (entry->maxhp < 100) ? " " : "",
		    entry->maxhp);
	}

	if (entry->highlight) {
	    bp = linebuf + strlen(linebuf);
	    while (bp < linebuf + (COLNO-1)) *bp++ = ' ';
	    *bp = 0;
	    tty_raw_print_bold(linebuf);
	} else
	    tty_raw_print(linebuf);
}


static void outheader(void)
{
	char linebuf[BUFSZ];
	char *bp;

	strcpy(linebuf, " No   Points     Name");
	bp = linebuf + strlen(linebuf);
	while (bp < linebuf + COLNO - 9) *bp++ = ' ';
	strcpy(bp, "Hp [max]");
	tty_raw_print(linebuf);
}


static void show_topten(char *player, int top, int around, boolean own)
{
	struct nh_topten_entry *scores;
	char status[BUFSZ];
	int i, listlen = 0;
	
	scores = nh_get_topten(&listlen, status, player, top, around, own);
	
	if (listlen == 0) {
	    return;
	}
	
	if (status[0])
	    tty_raw_print(status);
	
	tty_raw_print("");
	outheader();
	
	for (i = 0; i < listlen; i++)
	    topten_print(&scores[i]);
}


static void prscore(int argc, char **argv)
{
	char *plname = NULL;

	if (argc < 2 || strncmp(argv[1], "-s", 2)) {
		printf("prscore: bad arguments (%d)", argc);
		return;
	}
	
	if (argc >= 3)
	    plname = argv[2];
	
	show_topten(plname, 10, 0, plname != 0);
}


static void query_birth_options(void)
{
	char *prompt, resp = 0;
	
	move_cursor(BASE_WINDOW, 1, 5);
	prompt = "Do you want to modify your birth options? [yn] ";
	tty_putstr(BASE_WINDOW, 0, prompt);
	
	while (resp != 'y' && resp != 'n')
	    resp = tolower(tty_nhgetch());
	
	if (resp == 'y') {
	    display_options(TRUE);
	    clear_screen();
	    
	    move_cursor(BASE_WINDOW, 1, 0);
	    tty_putstr(BASE_WINDOW, 0, prompt);
	}
	putchar(resp);
}


static int commandloop(void)
{
	const char *cmd;
	int gamestate, count;
	struct nh_cmd_arg cmdarg;
	
	gamestate = READY_FOR_INPUT;
	game_is_running = TRUE;
	
	while (gamestate < GAME_OVER) {
	    count = 0;
	    cmd = NULL;
	    
	    if (gamestate == READY_FOR_INPUT)
		cmd = get_command(&count, &cmdarg);
	    else if (gamestate == MULTI_IN_PROGRESS && interrupt_multi)
		count = -1;
	    
	    interrupt_multi = FALSE; /* could have been set while no multi was in progress */
	    gamestate = nh_do_move(cmd, count, &cmdarg);
	}
	
	game_is_running = FALSE;
	return gamestate;
}


int main(int argc, char *argv[])
{
	char **gamepaths;
	int ret;

	/* idea from rpick%ucqais@uccba.uc.edu
	 * prevent automated rerolling of characters
	 * test input (fd0) so that tee'ing output to get a screen dump still
	 * works
	 * also incidentally prevents development of any hack-o-matic programs
	 */
	if (!isatty(0))
	    error("You must play from a terminal.");

	hackpid = getpid();
	umask(0777 & ~FCMASK);
	
	tty_init_options();
	win_tty_init();
	
	gamepaths = init_game_paths();
	nh_init(hackpid, &tty_procs, gamepaths);
	free(gamepaths);
	
	setup_signals();

	if (argc > 1) {
	    /* Now we know the directory containing 'record' and
	     * may do a prscore().
	     */
	    if (!strncmp(argv[1], "-s", 2)) {
		prscore(argc, argv);
		exit(EXIT_SUCCESS);
	    }
	}

#ifdef __linux__
	check_linux_console();
	init_linux_cons();
#endif
	
	tty_init_nhwindows();
	whoami(plname);
	read_config();

	process_options(argc, argv);	/* command line options */
	load_keymap(playmode == MODE_WIZARD);
	init_displaychars();
	
	while (!plname[0])
	    tty_askname(plname);
	
	tty_create_game_windows();
	
	if (!nh_restore_save(plname, locknum, playmode)) {
	    query_birth_options();
	    nh_start_game(plname, locknum, playmode);
	}
	
	ret = commandloop();
	
	tty_destroy_game_windows();
	tty_exit_nhwindows(NULL);
	
	if (ret == GAME_OVER)
	    show_topten(plname, ui_flags.end_top, ui_flags.end_around, ui_flags.end_own);
	else if (ret == GAME_SAVED)
	    tty_raw_print("Be seeing you...");
	
	nh_exit(0);
	free_displaychars();
	
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
	while (argc > 1 && argv[1][0] == '-'){
		argv++;
		argc--;
		switch(argv[0][1]){
		case 'D':
			playmode = MODE_WIZARD;
			break;
			
		case 'X':
			playmode = MODE_EXPLORE;
			break;
#ifdef NEWS
		case 'n':
			iflags2.news = FALSE;
			break;
#endif
		case 'u':
			if (argv[0][2])
			  strncpy(plname, argv[0]+2, sizeof(plname)-1);
			else if (argc > 1) {
			  argc--;
			  argv++;
			  strncpy(plname, argv[0], sizeof(plname)-1);
			} else
				tty_raw_print("Player name expected after -u");
			break;
		case 'I':
		case 'i':
			if (!strncmp(argv[0]+1, "IBM", 3))
				switch_graphics(IBM_GRAPHICS);
			break;
	    /*  case 'D': */
		case 'd':
			if (!strncmp(argv[0]+1, "DEC", 3))
				switch_graphics(DEC_GRAPHICS);
			break;
		case 'p': /* profession (role) */
			if (argv[0][2]) {
			    if ((i = nh_str2role(&argv[0][2])) >= 0)
			    	nh_set_role(i);
			} else if (argc > 1) {
				argc--;
				argv++;
			    if ((i = nh_str2role(argv[0])) >= 0)
			    	nh_set_role(i);
			}
			break;
		case 'r': /* race */
			if (argv[0][2]) {
			    if ((i = nh_str2race(&argv[0][2])) >= 0)
			    	nh_set_race(i);
			} else if (argc > 1) {
				argc--;
				argv++;
			    if ((i = nh_str2race(argv[0])) >= 0)
			    	nh_set_race(i);
			}
			break;
		case '@':
			nh_set_random_player();
			break;
		default:
			if ((i = nh_str2role(&argv[0][1])) >= 0) {
			    nh_set_role(i);
				break;
			}
			/* else raw_printf("Unknown option: %s", *argv); */
		}
	}

	if (argc > 1)
		locknum = atoi(argv[1]);
#ifdef MAX_NR_OF_PLAYERS
	if (!locknum || locknum > MAX_NR_OF_PLAYERS)
		locknum = MAX_NR_OF_PLAYERS;
#endif
}


static void whoami(char *name)
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

	if (*name)
	    return;
	
	if (/* !*name && */ (s = getenv("USER")))
		strncpy(name, s, PL_NSIZ);
	if (!*name && (s = getenv("LOGNAME")))
		strncpy(name, s, PL_NSIZ);
	if (!*name && (s = getlogin()))
		strncpy(name, s, PL_NSIZ);
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
