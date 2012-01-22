/* Copyright (c) Daniel Thaler, 2011.                             */
/* NitroHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(NETCLIENT)
# define allow_timetest() (!nhnet_active())
#else
# define allow_timetest() (1)
#endif

static void dummy_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO], int ux, int uy) {}
static void dummy_delay_output(void) {}

static struct nh_window_procs curses_replay_windowprocs = {
    curses_pause,
    curses_display_buffer,
    curses_update_status_silent,
    curses_print_message_nonblocking,
    NULL,
    NULL,
    curses_list_items_nonblocking,
    dummy_update_screen,
    curses_raw_print,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    dummy_delay_output,
    curses_notify_level_changed,
    curses_outrip,
};


#if defined(WIN32)
#define TIMETEST_OK

typedef __int64 hp_time;

static void gettime(hp_time *t)
{
    QueryPerformanceCounter((LARGE_INTEGER*)t);
}

static long clock_delta_ms(hp_time *t_start, hp_time *t_end)
{
    __int64 freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    
    return (*t_end - *t_start) * 1000 / freq;
}

#elif defined(UNIX)
#include <sys/time.h>
#define TIMETEST_OK

typedef struct timeval hp_time;

static void gettime(hp_time *t)
{
    gettimeofday(t, NULL);
}

static long clock_delta_ms(hp_time *t_start, hp_time *t_end)
{
    return (t_end->tv_sec - t_start->tv_sec) * 1000 +
           (t_end->tv_usec - t_start->tv_usec) / 1000;
}

#endif


static void draw_replay_info(struct nh_replay_info *rinfo)
{
    char buf[BUFSZ];
    
    sprintf(buf, "REPLAY action %d/%d", rinfo->actions, rinfo->max_actions);
    if (*rinfo->nextcmd)
	sprintf(buf + strlen(buf), "; next command: %s.", rinfo->nextcmd);
    
    if (ui_flags.draw_frame) {
	/* draw the replay state on top of the frame under the map */
	mvwhline(basewin, 2 + ui_flags.msgheight + ROWNO, 1, ACS_HLINE, COLNO);
	wattron(basewin, COLOR_PAIR(4) | A_BOLD);
	mvwaddstr(basewin, 2 + ui_flags.msgheight + ROWNO, 2, buf);
	wattroff(basewin, COLOR_PAIR(4) | A_BOLD);
    } else {
	/* try to draw under the status */
	int h = ui_flags.msgheight + ROWNO + (ui_flags.status3 ? 3 : 2);
	if (h < LINES) {
	    wattron(basewin, COLOR_PAIR(4) | A_BOLD);
	    mvwaddstr(basewin, h, 0, buf);
	    wattroff(basewin, COLOR_PAIR(4) | A_BOLD);
	    wclrtoeol(basewin);
	} /* else: make do without replay info */
    }
    wnoutrefresh(basewin);
    /* refresh on basewin erases the windows on top of it ... */
    touchwin(msgwin);
    wnoutrefresh(msgwin);
    touchwin(mapwin);
    wnoutrefresh(mapwin);
}


#if defined(TIMETEST_OK)
static void timetest(int fd, struct nh_replay_info *rinfo)
{
    char buf[BUFSZ];
    hp_time t_start, t_end;
    long ms;
    int mmax, initial, revpos;
    
    initial = rinfo->moves;
    nh_view_replay_step(rinfo, REPLAY_GOTO, 0);
    
    /* run forward */
    gettime(&t_start);
    while (rinfo->actions < rinfo->max_actions) {
	nh_view_replay_step(rinfo, REPLAY_FORWARD, 1);
	curses_update_status(NULL);
	draw_replay_info(rinfo);
	doupdate();
    }
    draw_msgwin();
    gettime(&t_end);
    ms = clock_delta_ms(&t_start, &t_end);
    snprintf(buf, BUFSZ, "%d actions replayed with display in %ld ms. (%ld actions/sec)",
	     rinfo->max_actions, ms, rinfo->max_actions * 1000 / ms);
    curses_msgwin(buf);
    
    /* reset the entire replay state to delete checkpoints */
    mmax = rinfo->moves; /* max_moves may not be available if this is a replay of a crashed game */
    nh_view_replay_finish();
    nh_view_replay_start(fd, &curses_replay_windowprocs, rinfo);
    
    /* run forward without showing the map etc. */
    gettime(&t_start);
    nh_view_replay_step(rinfo, REPLAY_GOTO, mmax);
    gettime(&t_end);
    ms = clock_delta_ms(&t_start, &t_end);
    snprintf(buf, BUFSZ, "%d actions replayed without display in %ld ms. (%ld actions/sec)",
	     rinfo->actions, ms, rinfo->actions * 1000 / ms);
    curses_msgwin(buf);
    
    /* run backward */
    revpos = rinfo->actions;
    gettime(&t_start);
    while (rinfo->actions > 0 && revpos < rinfo->actions + 1000) {
	nh_view_replay_step(rinfo, REPLAY_BACKWARD, 1);
	curses_update_status(NULL);
	draw_msgwin();
	draw_replay_info(rinfo);
	doupdate();
    }
    gettime(&t_end);
    ms = clock_delta_ms(&t_start, &t_end);
    snprintf(buf, BUFSZ, "%d actions replayed backward with display in %ld ms. (%ld actions/sec)",
	     revpos - rinfo->actions, ms, (revpos - rinfo->actions) * 1000 / ms);
    curses_msgwin(buf);
    
    nh_view_replay_step(rinfo, REPLAY_GOTO, initial);
}
#else
static void timetest(int fd, struct nh_replay_info *rinfo) {}
#endif


static void show_replay_help(void)
{
    static struct nh_menuitem items[] = {
	{0, MI_TEXT, "KEY_RIGHT or SPACE\t- advance one move"},
	{0, MI_TEXT, "KEY_LEFT\t- go back one move"},
	{0, MI_TEXT, "g\t- go to any move"},
	{0, MI_TEXT, "ESC\t- leave the replay"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, "Informational game commands like \"inventory\" or \"discoveries\" will also work"}
    };
    curses_display_menu(items, 6, "Replay help:", PICK_NONE, NULL);
}


void replay_commandloop(int fd)
{
    int key, move, count;
    char buf[BUFSZ], qbuf[BUFSZ];
    nh_bool ret, firsttime = TRUE;
    struct nh_replay_info rinfo;
    struct nh_cmd_arg noarg;
    struct nh_cmd_desc *cmd;
    
    create_game_windows();
    if (!nh_view_replay_start(fd, &curses_replay_windowprocs, &rinfo))
	return;
    load_keymap();
    
    while (1) {
	draw_msgwin();
	curses_update_status(NULL);
	draw_sidebar();
	draw_replay_info(&rinfo);
	if (firsttime)
	    show_replay_help();
	firsttime = FALSE;

	key = get_cmdkey();
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
		nh_view_replay_step(&rinfo, REPLAY_BACKWARD, 1);
		draw_replay_info(&rinfo);
		break;
		
	    case KEY_ESC:
		goto out;

	    case 'g':
		strncpy(qbuf, "What move do you want to jump to?", BUFSZ);
		if (rinfo.max_moves > 0)
		    sprintf(qbuf + strlen(qbuf), " (Max: %d)", rinfo.max_moves);
		
		curses_getline(qbuf, buf);
		if (buf[0] == '\033' || !(move = atoi(buf)))
		    break;
		nh_view_replay_step(&rinfo, REPLAY_GOTO, move);
		break;

	    case KEY_F(12): /* timetest! */
		if (allow_timetest())
		    timetest(fd, &rinfo);
		break;
		
	    default:
		count = 0;
		noarg.argtype = CMD_ARG_NONE;
		cmd = keymap[key];
		if (!cmd)
		    break;
		if (cmd->flags & CMD_UI)
		    handle_internal_cmd(&cmd, &noarg, &count);
		if (cmd)
		    nh_command(cmd->name, count, &noarg);
		break;
	}
    }
    
out:
    nh_view_replay_finish();
    free_keymap();
    destroy_game_windows();
    cleanup_messages();
}


void replay(void)
{
    char buf[BUFSZ];
    fnchar logdir[BUFSZ], savedir[BUFSZ], filename[1024], *dir, **files;
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
	    fd = sys_open(files[i], O_RDWR, 0660);
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
	filename[0] = '\0';
	if (n > 0 && pick[0] != -1)
	    fnncat(filename, files[pick[0]-1], sizeof(filename)/sizeof(fnchar)-1);
	
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
    
    fd = sys_open(filename, O_RDWR, 0660);
    replay_commandloop(fd);
    close(fd);
}
