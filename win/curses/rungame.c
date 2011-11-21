/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <string.h>
#ifndef O_RDONLY
#include <fcntl.h>
#endif
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <errno.h>

#include "nhcurses.h"


enum game_dirs {
    SAVE_DIR,
    LOG_DIR
};


static boolean get_gamedir(enum game_dirs dirtype, char *buf)
{
    char *envval, *subdir;
    mode_t mask;
    
    switch (dirtype) {
	case SAVE_DIR: subdir = "save"; break;
	case LOG_DIR: subdir = "log"; break;
    }
    
    /* look in regular location */
    envval = getenv("XDG_CONFIG_HOME");
    if (envval)
	snprintf(buf, BUFSZ, "%s/NetHack/%s/", envval, subdir);
    else {
	envval = getenv("HOME");
	if (!envval) /* HOME not set? just give up... */
	    return FALSE;
	snprintf(buf, BUFSZ, "%s/.config/NetHack/%s/", envval, subdir);
    }
    
    mask = umask(0);
    if (mkdir(buf, 0755) == -1 && errno != EEXIST) {
	/* try to create the parent directory too. This ist the only problem we
	 * can fix here - permission problems etc. all requre user intervention */
	char dirbuf[BUFSZ], *basedir;
	strcpy(dirbuf, buf);
	basedir = dirname(dirbuf);
	
	mkdir(dirbuf, 0755); /* no need to check the return value: if it doesn't work, we're screwed */
	if (mkdir(dirbuf, 0755) == -1) {
	    umask(mask);
	    return FALSE;
	}
    }
    umask(mask);
    
    return TRUE;
}


static void query_birth_options(void)
{
    char resp = 0;

    resp = curses_yn_function("Do you want to modify your birth options?", "yn", 'n');
    
    if (resp == 'y')
	display_options(TRUE);
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


static void game_ended(int status, char *filename)
{
    char fncopy[1024], logname[1024], savedir[BUFSZ], *fdir, *fname;
    
    if (status == GAME_SAVED) {
	printf("Be seeing you...");
	return;
    } else if (status != GAME_OVER)
	return;
    
    show_topten(player.plname, settings.end_top, settings.end_around,
		settings.end_own);
    
    /* 
     * The game ended terminally. Now it would be nice to move the saved game
     * out of the save/ dir and into the log/ dir, since it's only use now is as
     * a tropy.
     */
    
    /* dirname and basename may modify the input string, depending on the system */
    strncpy(fncopy, filename, sizeof(fncopy));
    fdir = dirname(fncopy);
    
    get_gamedir(SAVE_DIR, savedir);
    savedir[strlen(savedir)-1] = '\0'; /* remove the trailing '/' */
    if (strcmp(fdir, savedir) != 0)
	return; /* file was not in savedir, so don't touch it */
    
    get_gamedir(LOG_DIR, logname);
    strncpy(fncopy, filename, sizeof(fncopy));
    fname = basename(fncopy);
    strncat(logname, fname, sizeof(logname));
    
    /* don't care about errors: rename is nice to have, not essential */
    rename(filename, logname);
}


void rungame(void)
{
    int ret;
    int fd = -1;
    char filename[1024];
    char savedir[BUFSZ];
    long t;
    
    if (!get_gamedir(SAVE_DIR, savedir)) {
	curses_raw_print("Could not find where to put the logfile for a new game.");
	return;
    }
    
    query_birth_options();
    while (!settings.plname[0])
	curses_getline("what is your name?", settings.plname);

    t = (long)time(NULL);
    snprintf(filename, sizeof(filename), "%s%ld_%s.nhgame", savedir,
	     t, settings.plname);
    fd = open(filename, O_TRUNC | O_CREAT | O_RDWR, 0660);
    if (fd == -1) {
	curses_raw_print("Could not create the logfile.");
	return;
    }
    
    create_game_windows();
    if (!nh_start_game(fd, settings.plname, ui_flags.playmode))
	return;
    
    load_keymap(); /* need to load the keymap after the game has been started */
    ret = commandloop();
    free_keymap();
    
    destroy_game_windows();
    cleanup_messages();
    game_ended(ret, filename);
}


static void describe_save(char *buf, enum nh_log_status status, struct nh_save_info *si)
{
    if (status == LS_IN_PROGRESS)
	snprintf(buf, BUFSZ, "%s\t%3.3s-%3.3s-%3.3s-%3.3s  (crashed?)",
		 si->name, si->plrole, si->plrace, si->plgend, si->plalign);
    else
	snprintf(buf, BUFSZ, "%s\t%3.3s-%3.3s-%3.3s-%3.3s %s%s\tafter %d moves",
		 si->name, si->plrole, si->plrace, si->plgend, si->plalign,
		 si->level_desc, si->has_amulet ? " with the amulet" : "", si->moves);
}


boolean loadgame(void)
{
    char buf[BUFSZ], savedir[BUFSZ], filename[1024], **files;
    DIR *dirp;
    struct dirent *dp;
    struct nh_menuitem *items;
    int namelen, size, icount, fd, i, n, ret, pick[1];
    enum nh_log_status status;
    struct nh_save_info si;
    
    if (!get_gamedir(SAVE_DIR, savedir)) {
	curses_raw_print("Could not find or create the save directory.");
	return FALSE;
    }
    
    size = 10;
    icount = 0;
    items = malloc(size * sizeof(struct nh_menuitem));
    files = NULL;
    
    dirp = opendir(savedir);
    while ((dp = readdir(dirp)) != NULL) {
	namelen = strlen(dp->d_name);
	if (namelen > 7 && /* ".nhgame" */
	    !strcmp(&dp->d_name[namelen-7], ".nhgame")) {
	    snprintf(filename, sizeof(filename), "%s%s", savedir, dp->d_name);
	    
	    fd = open(filename, O_RDONLY);
	    status = nh_get_savegame_status(fd, &si);
	    close(fd);
	    
	    if (status == LS_SAVED || status == LS_IN_PROGRESS) {
		describe_save(buf, status, &si);
		add_menu_item(items, size, icount, icount + 1, buf, 0, FALSE);
		files = realloc(files, icount * sizeof(char*));
		files[icount-1] = strdup(dp->d_name);
	    }
	}
    }
    closedir(dirp);

    if (!icount) {
	free(items);
	curses_msgwin("No saved games found.");
	return FALSE;
    }
    
    n = curses_display_menu(items, icount, "saved games", PICK_ONE, pick);    
    free(items);
    if (n <= 0)
	return FALSE;

    snprintf(filename, sizeof(filename), "%s%s", savedir, files[pick[0]-1]);
    for (i = 0; i < icount; i++)
	free(files[i]);
    free(files);
    
    fd = open(filename, O_RDWR, 0660);
    create_game_windows();
    if (nh_restore_game(fd, NULL, FALSE) != GAME_RESTORED) {
	curses_msgwin("Failed to restore saved game.");
	destroy_game_windows();
	return FALSE;
    }
    
    load_keymap(); /* need to load the keymap after the game has been started */
    ret = commandloop();
    free_keymap();
    
    destroy_game_windows();
    cleanup_messages();
    game_ended(ret, filename);
    
    return TRUE;
}


