/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-25 */
/* Copyright (c) Daniel Thaler, 2012 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

void
net_loadgame(void)
{
    char buf[BUFSZ];
    struct nhnet_game *gamelist;
    struct nh_menulist menu;
    int id, size, i, ret, pick[1];

    gamelist = nhnet_list_games(FALSE, FALSE, &size);
    if (!gamelist) {
        curses_msgwin("Failed to retrieve the list of saved games.",
                      krc_notification);
        return;
    }
    if (!size) {
        curses_msgwin("No saved games found.", krc_notification);
        return;
    }

    init_menulist(&menu);

    for (i = 0; i < size; i++) {
        if (gamelist[i].status == LS_DONE || gamelist[i].status == LS_INVALID)
            continue;
        describe_game(buf, gamelist[i].status, &gamelist[i].i);
        id = (gamelist[i].status == LS_IN_PROGRESS) ? 0 : gamelist[i].gameid;
        add_menu_item(&menu, id, buf, 0, FALSE);
    }

    curses_display_menu(&menu, "saved games", PICK_ONE, PLHINT_ANYWHERE, pick,
                        curses_menu_callback);

    if (*pick == CURSES_MENU_CANCELLED)
        return;

    id = pick[0];

    create_game_windows();

    ret = playgame(id);

    destroy_game_windows();
    cleanup_messages();

    game_ended(ret, NULL, TRUE);
}

void
net_replay(void)
{
    /* TODO */
}
