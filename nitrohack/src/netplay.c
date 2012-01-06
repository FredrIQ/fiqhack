/* Copyright (c) Daniel Thaler, 2012 */
/* NitroHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"


void net_rungame(void)
{
    char plname[BUFSZ];
    int role = initrole, race = initrace, gend = initgend, align = initalign;
    int ret;
    
    if (!player_selection(&role, &race, &gend, &align, random_player))
	return;
    
    strncpy(plname, settings.plname, PL_NSIZ);
    /* The player name is set to "wizard" (again) in nh_start_game, so setting
     * it here just prevents wizmode player from being asked for a name. */
    if (ui_flags.playmode == MODE_WIZARD)
	strcpy(plname, "wizard");
    
    while (!plname[0])
	curses_getline("what is your name?", plname);
    if (plname[0] == '\033') /* canceled */
	return;
    
    create_game_windows();
    if (!nhnet_start_game(plname, role, race, gend, align, ui_flags.playmode)) {
	destroy_game_windows();
	return;
    }
    
    load_keymap(); /* need to load the keymap after the game has been started */
    ret = commandloop();
    free_keymap();
    
    destroy_game_windows();
    cleanup_messages();
    
    if (ret == GAME_OVER)
	show_topten(player.plname, settings.end_top, settings.end_around,
		    settings.end_own);
}


void net_loadgame(void)
{
    char buf[BUFSZ];
    struct nhnet_game *gamelist;
    struct nh_menuitem *items;
    int size, icount, id, i, n, ret, pick[1];
    
    gamelist = nhnet_list_games(FALSE, &size);
    if (!size) {
	curses_msgwin("No saved games found.");
	return;
    }
    
    icount = 0;
    items = malloc(size * sizeof(struct nh_menuitem));
    for (i = 0; i < size; i++) {
	if (gamelist[i].status == LS_DONE || gamelist[i].status == LS_INVALID)
	    continue;
	describe_game(buf, gamelist[i].status, &gamelist[i].i);
	id = (gamelist[i].status == LS_IN_PROGRESS) ? 0 : gamelist[i].gameid;
	add_menu_item(items, size, icount, id, buf, 0, FALSE);
    }

    n = curses_display_menu(items, icount, "saved games", PICK_ONE, pick);    
    free(items);
    if (n <= 0)
	return;
    
    id = pick[0];
    
    create_game_windows();
    if (nhnet_restore_game(id, NULL) != GAME_RESTORED) {
	curses_msgwin("Failed to restore saved game.");
	destroy_game_windows();
	return;
    }
    
    load_keymap(); /* need to load the keymap after the game has been started */
    ret = commandloop();
    free_keymap();
    
    destroy_game_windows();
    cleanup_messages();
    
    if (ret == GAME_OVER)
	show_topten(player.plname, settings.end_top, settings.end_around,
		    settings.end_own);
}

void net_replay(void)
{
    char buf[BUFSZ];
    struct nhnet_game *gamelist;
    struct nh_menuitem *items;
    int pick[1], allow_type[2];
    int i, n, icount, size, filecount, id, gameid, want_done;
    
    want_done = TRUE;
    allow_type[0] = allow_type[1] = 1;
    while (1) {
	gamelist = nhnet_list_games(want_done, &filecount);
	/* make sure there are some games to show */
	if (!filecount) {
	    if (!want_done) {
		curses_msgwin("There are no saved games to replay.");
		allow_type[0] = 0;
	    } else {
		curses_msgwin("There are no completed games to replay.");
		allow_type[1] = 0;
	    }
	    want_done = !want_done;
	    if (!allow_type[want_done]) return;
	    continue;
	}
	
	icount = 0;
	size = filecount + 2;
	items = malloc(size * sizeof(struct nh_menuitem));
	
	/* add all the files to the menu */
	for (i = 0; i < filecount; i++) {
	    describe_game(buf, gamelist[i].status, &gamelist[i].i);
	    id = (gamelist[i].status == LS_IN_PROGRESS) ? 0 : gamelist[i].gameid;
	    add_menu_item(items, size, icount, id, buf, 0, FALSE);
	}
	
	if (want_done == 1 && allow_type[0])
	    add_menu_item(items, size, icount, -1, "View saved games instead", 0, FALSE);
	else if (want_done == 0 && allow_type[1])
	    add_menu_item(items, size, icount, -1, "View saved games instead", 0, FALSE);
	
	n = curses_display_menu(items, icount, "Pick a game to view", PICK_ONE, pick);
	free(items);
	if (n <= 0)
	    return;
	
	if (pick[0] == -1) {
	    want_done = !want_done;
	    continue;
	} else
	    gameid = pick[0];
	
	break;
    }
    
    replay_commandloop(gameid);
}

