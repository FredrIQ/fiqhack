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


boolean get_gamedir(enum game_dirs dirtype, char *buf)
{
    char *envval, *subdir;
    mode_t mask;
    
    switch (dirtype) {
	case CONFIG_DIR: subdir = ""; break;
	case SAVE_DIR: subdir = "save/"; break;
	case LOG_DIR: subdir = "log/"; break;
    }
    
    /* look in regular location */
    envval = getenv("XDG_CONFIG_HOME");
    if (envval)
	snprintf(buf, BUFSZ, "%s/NetHack/%s", envval, subdir);
    else {
	envval = getenv("HOME");
	if (!envval) /* HOME not set? just give up... */
	    return FALSE;
	snprintf(buf, BUFSZ, "%s/.config/NetHack/%s", envval, subdir);
    }
    
    mask = umask(0);
    if (mkdir(buf, 0755) == -1 && errno != EEXIST) {
	/* try to create the parent directory too. This ist the only problem we
	 * can fix here - permission problems etc. all requre user intervention */
	char dirbuf[BUFSZ], *basedir;
	strcpy(dirbuf, buf);
	basedir = dirname(dirbuf);
	
	mkdir(basedir, 0755); /* no need to check the return value: if it doesn't work, we're screwed */
	if (mkdir(buf, 0755) == -1) {
	    umask(mask);
	    return FALSE;
	}
    }
    umask(mask);
    
    return TRUE;
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
	gamestate = nh_command(cmd, count, &cmdarg);
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
    int ret, role = initrole, race = initrace, gend = initgend, align = initalign;
    int fd = -1;
    char filename[1024];
    char savedir[BUFSZ];
    long t;
    
    if (!get_gamedir(SAVE_DIR, savedir)) {
	curses_raw_print("Could not find where to put the logfile for a new game.");
	return;
    }
    
    if (!player_selection(&role, &race, &gend, &align, random_player))
	return;
    
    /* The player name is set to "wizard" (again) in nh_start_game, so setting
     * it here just prevents wizmode player from being asked for a name. */
    if (ui_flags.playmode == MODE_WIZARD)
	strcpy(settings.plname, "wizard");
    
    while (!settings.plname[0])
	curses_getline("what is your name?", settings.plname);
    if (settings.plname[0] == '\033') /* canceled */
	return;

    t = (long)time(NULL);
    snprintf(filename, sizeof(filename), "%s%ld_%s.nhgame", savedir,
	     t, settings.plname);
    fd = open(filename, O_TRUNC | O_CREAT | O_RDWR, 0660);
    if (fd == -1) {
	curses_raw_print("Could not create the logfile.");
	return;
    }
    
    create_game_windows();
    if (!nh_start_game(fd, settings.plname, role, race, gend, align, ui_flags.playmode))
	return;
    
    load_keymap(); /* need to load the keymap after the game has been started */
    ret = commandloop();
    free_keymap();
    
    destroy_game_windows();
    cleanup_messages();
    game_ended(ret, filename);
}


static void describe_game(char *buf, enum nh_log_status status, struct nh_game_info *gi)
{
    const char *mode_desc[] = {"", "\t[explore]", "\t[wizard]"};
    switch (status) {
	case LS_CRASHED:
	    snprintf(buf, BUFSZ, "%s\t%3.3s-%3.3s-%3.3s-%3.3s  (crashed)\t%s",
		    gi->name, gi->plrole, gi->plrace, gi->plgend, gi->plalign,
		    mode_desc[gi->playmode]);
	    break;
	    
	case LS_IN_PROGRESS:
	    snprintf(buf, BUFSZ, "    %s\t%3.3s-%3.3s-%3.3s-%3.3s  (in progress)\t%s",
		    gi->name, gi->plrole, gi->plrace, gi->plgend, gi->plalign,
		    mode_desc[gi->playmode]);
	    break;
	    
	case LS_SAVED:
	    snprintf(buf, BUFSZ, "%s\t%3.3s-%3.3s-%3.3s-%3.3s %s%s\tafter %d moves%s",
		    gi->name, gi->plrole, gi->plrace, gi->plgend, gi->plalign,
		    gi->level_desc, gi->has_amulet ? " with the amulet" : "",
		    gi->moves, mode_desc[gi->playmode]);
	    break;
	    
	case LS_DONE:
	    snprintf(buf, BUFSZ, "%s\t%3.3s-%3.3s-%3.3s-%3.3s %s\tafter %d moves%s",
		    gi->name, gi->plrole, gi->plrace, gi->plgend, gi->plalign,
		    gi->death, gi->moves, mode_desc[gi->playmode]);
	    break;
	    
	    
	default:
	    buf[0] = '\0';
	    break;
    }
}


int compare_filetime(const void *arg1, const void *arg2)
{
    const char *file1 = *(const char**)arg1;
    const char *file2 = *(const char**)arg2;
    
    struct stat s1, s2;
    
    stat(file1, &s1);
    stat(file2, &s2);
    
    return s2.st_mtime - s1.st_mtime;
}


char **list_gamefiles(char *dir, int *count)
{
    char filename[1024], **files;
    DIR *dirp;
    struct dirent *dp;
    int namelen, fd;
    enum nh_log_status status;
    
    files = NULL;
    *count = 0;
    
    dirp = opendir(dir);
    while ((dp = readdir(dirp)) != NULL) {
	namelen = strlen(dp->d_name);
	if (namelen > 7 && /* ".nhgame" */
	    !strcmp(&dp->d_name[namelen-7], ".nhgame")) {
	    snprintf(filename, sizeof(filename), "%s%s", dir, dp->d_name);
	    
	    fd = open(filename, O_RDWR, 0660);
	    status = nh_get_savegame_status(fd, NULL);
	    close(fd);
	    
	    if (status == LS_INVALID)
		continue;
	    
	    (*count)++;
	    files = realloc(files, (*count) * sizeof(char*));
	    files[*count - 1] = strdup(filename);
	}
    }
    closedir(dirp);
    
    qsort(files, *count, sizeof(char*), compare_filetime);
    
    return files;
}


boolean loadgame(void)
{
    char buf[BUFSZ], savedir[BUFSZ], filename[1024], **files;
    struct nh_menuitem *items;
    int size, icount, fd, i, n, ret, pick[1];
    enum nh_log_status status;
    struct nh_game_info gi;
    
    if (!get_gamedir(SAVE_DIR, savedir)) {
	curses_raw_print("Could not find or create the save directory.");
	return FALSE;
    }
    
    files = list_gamefiles(savedir, &size);
    if (!size) {
	curses_msgwin("No saved games found.");
	return FALSE;
    }
    
    icount = 0;
    items = malloc(size * sizeof(struct nh_menuitem));

    for (i = 0; i < size; i++) {
	fd = open(files[i], O_RDWR, 0660);
	status = nh_get_savegame_status(fd, &gi);
	close(fd);
	
	describe_game(buf, status, &gi);
	add_menu_item(items, size, icount, (status == LS_IN_PROGRESS) ? 0 : icount + 1,
			buf, 0, FALSE);
    }

    n = curses_display_menu(items, icount, "saved games", PICK_ONE, pick);    
    free(items);
    if (n > 0)
	strncpy(filename, files[pick[0]-1], sizeof(filename));

    for (i = 0; i < icount; i++)
	free(files[i]);
    free(files);
    if (n <= 0)
	return FALSE;
    
    fd = open(filename, O_RDWR, 0660);
    create_game_windows();
    if (nh_restore_game(fd, NULL, TRUE) != GAME_RESTORED) {
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


static void draw_replay_info(struct nh_replay_info *rinfo)
{
    char buf[BUFSZ];
    
    sprintf(buf, "REPLAY action %d/%d", rinfo->actions, rinfo->max_actions);
    if (*rinfo->last_command)
	sprintf(buf + strlen(buf), "; last command: %s.", rinfo->last_command);
    
    mvwhline(stdscr, 2 + ui_flags.msgheight + ROWNO, 1, ACS_HLINE, COLNO);
    wattron(stdscr, COLOR_PAIR(4) | A_BOLD);
    mvwaddstr(stdscr, 2 + ui_flags.msgheight + ROWNO, 2, buf);
    wattroff(stdscr, COLOR_PAIR(4) | A_BOLD);
    redraw_game_windows();
}


static void replay_commandloop(int fd)
{
    int key, move;
    char buf[BUFSZ], qbuf[BUFSZ];
    boolean ret;
    struct nh_replay_info rinfo;
    
    create_game_windows();
    nh_view_replay_start(fd, &curses_windowprocs, &rinfo);
    draw_replay_info(&rinfo);
    while (1) {
	key = nh_wgetch(stdscr);
	switch (key) {
	    /* step forward */
	    case KEY_RIGHT:
	    case ' ':
		ret = nh_view_replay_step(&rinfo, REPLAY_FORWARD, 1);
		draw_replay_info(&rinfo);
		if (ret == FALSE) {
		    key = curses_msgwin("You have reached the end of this game. "
		                        "Go back or press ESC to exit.");
		    if (key == KEY_ESC)
			goto out;
		}
		break;
		
	    /* step backward */
	    case KEY_LEFT:
		ret = nh_view_replay_step(&rinfo, REPLAY_BACKWARD, 1);
		draw_replay_info(&rinfo);
		if (ret == FALSE) {
		    key = curses_msgwin("You have reached the beginning of this game. "
		                        "Go forward or press ESC to exit.");
		    if (key == KEY_ESC)
			goto out;
		}
		break;
		
	    case KEY_ESC:
		goto out;

	    case 'g':
		strncpy(qbuf, "What move do you want to go to?", BUFSZ);
		if (rinfo.max_moves > 0)
		    sprintf(qbuf + strlen(qbuf), " (Max: %d)", rinfo.max_moves);
		
		curses_getline(qbuf, buf);
		if (buf[0] == '\033' || !(move = atoi(buf)))
		    break;
		ret = nh_view_replay_step(&rinfo, REPLAY_GOTO, move);
		draw_replay_info(&rinfo);
		if (ret == FALSE) {
		    sprintf(buf, "You tried to go to move %d but move %d is the last.",
			    move, rinfo.moves);
		    curses_msgwin(buf);
		}
		
		break;
	}
    }
    
out:
    nh_view_replay_finish();
    destroy_game_windows();
    cleanup_messages();
}


void replay(void)
{
    char buf[BUFSZ], logdir[BUFSZ], savedir[BUFSZ], filename[1024], *dir, **files;
    struct nh_menuitem *items;
    int i, n, fd, icount, size, filecount, pick[1];
    enum nh_log_status status;
    struct nh_game_info gi;
    
    if (!get_gamedir(LOG_DIR, logdir))	logdir[0] = '\0';
    if (!get_gamedir(SAVE_DIR, savedir))savedir[0] = '\0';
    
    if (*logdir)	dir = logdir;
    else if (*savedir)	dir = savedir;
    else {
	curses_msgwin("There are no games to replay.");
	return;
    }
    
    while (1) {
	filename[0] = '\0';
	files = list_gamefiles(dir, &filecount);
	/* make sure there are some files to show */
	if (!filecount) {
	    if (dir == savedir) {
		curses_msgwin("There are no saved games to replay.");
		savedir[0] = '\0';
	    } else {
		curses_msgwin("There are no completed games to replay.");
		logdir[0] = '\0';
	    }
	    
	    dir = (dir == savedir) ? logdir : savedir;
	    if (!*dir) return;
	    continue;
	}
	
	icount = 0;
	size = filecount + 2;
	items = malloc(size * sizeof(struct nh_menuitem));
	
	/* add all the files to the menu */
	for (i = 0; i < filecount; i++) {
	    fd = open(files[i], O_RDWR, 0660);
	    status = nh_get_savegame_status(fd, &gi);
	    close(fd);
	    
	    describe_game(buf, status, &gi);
	    add_menu_item(items, size, icount,
			    (status == LS_IN_PROGRESS) ? 0 : icount + 1,
			    buf, 0, FALSE);
	}
	
	if (dir == logdir && *savedir) {
	    add_menu_item(items, size, icount, -1, "View saved games instead", 0, FALSE);
	} else if (dir == savedir && *logdir) {
	    add_menu_item(items, size, icount, -1, "View saved games instead", 0, FALSE);
	}
	
	n = curses_display_menu(items, icount, "Pick a game to view", PICK_ONE, pick);    
	free(items);
	if (n > 0 && pick[0] != -1)
	    strncpy(filename, files[pick[0]-1], sizeof(filename));
	
	for (i = 0; i < filecount; i++)
	    free(files[i]);
	free(files);
	
	if (n <= 0)
	    return;
	
	if (pick[0] == -1) {
	    dir = (dir == savedir) ? logdir : savedir;
	    continue;
	}
	
	/* we have a valid filename */
	break;
    }
    
    fd = open(filename, O_RDWR, 0660);
    replay_commandloop(fd);
    close(fd);
}
