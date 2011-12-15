/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#if !defined(WIN32)
# include <dirent.h>
# include <libgen.h>
#endif


#if defined(WIN32)

nh_bool get_gamedir(enum game_dirs dirtype, wchar_t *buf)
{
    wchar_t *subdir;
    wchar_t appPath[MAX_PATH], nhPath[MAX_PATH];
    
    /* Get the location of "AppData\Roaming" (Vista, 7) or "Application Data" (XP).
     * The returned Path does not include a trailing backslash. */
    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appPath)))
	return FALSE;
    
    switch (dirtype) {
	case CONFIG_DIR: subdir = L"\\"; break;
	case SAVE_DIR:   subdir = L"\\save\\"; break;
	case LOG_DIR:    subdir = L"\\log\\"; break;
    }
    
    snwprintf(nhPath, MAX_PATH, L"%s\\NetHack", appPath);
    _wmkdir(nhPath);
    
    snwprintf(buf, BUFSZ, L"%s%s", nhPath, subdir);
    _wmkdir(buf);
    
    return TRUE;
}

#else /* defined(WIN32) */

nh_bool get_gamedir(enum game_dirs dirtype, char *buf)
{
    char *envval, *subdir;
    mode_t mask;
    
    switch (dirtype) {
	case CONFIG_DIR: subdir = ""; break;
	case SAVE_DIR:   subdir = "save/"; break;
	case LOG_DIR:    subdir = "log/"; break;
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

#endif


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


static void game_ended(int status, fnchar *filename)
{
    fnchar fncopy[1024], logname[1024], savedir[BUFSZ], *fdir, *fname;
    
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
#if !defined(WIN32)
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
#endif
}


void rungame(void)
{
    int ret, role = initrole, race = initrace, gend = initgend, align = initalign;
    int fd = -1;
    fnchar filename[1024];
    fnchar savedir[BUFSZ];
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
#if defined(WIN32)
    snwprintf(filename, 1024, L"%ls%ld_%s.nhgame", savedir,
	     t, settings.plname);
#else
    snprintf(filename, 1024, "%s%ld_%s.nhgame", savedir,
	     t, settings.plname);
#endif
    fd = sys_open(filename, O_TRUNC | O_CREAT | O_RDWR, 0660);
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


void describe_game(char *buf, enum nh_log_status status, struct nh_game_info *gi)
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

#if defined(WIN32)
wchar_t **list_gamefiles(wchar_t *dir, int *count)
{
    return NULL;
}

#else

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
#endif


nh_bool loadgame(void)
{
    char buf[BUFSZ];
    fnchar savedir[BUFSZ], filename[1024], **files;
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
	fd = sys_open(files[i], O_RDWR, 0660);
	status = nh_get_savegame_status(fd, &gi);
	close(fd);
	
	describe_game(buf, status, &gi);
	add_menu_item(items, size, icount, (status == LS_IN_PROGRESS) ? 0 : icount + 1,
			buf, 0, FALSE);
    }

    n = curses_display_menu(items, icount, "saved games", PICK_ONE, pick);    
    free(items);
    filename[0] = '\0';
    if (n > 0)
	fnncat(filename, files[pick[0]-1], sizeof(filename)/sizeof(fnchar));

    for (i = 0; i < icount; i++)
	free(files[i]);
    free(files);
    if (n <= 0)
	return FALSE;
    
    fd = sys_open(filename, O_RDWR, 0660);
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
