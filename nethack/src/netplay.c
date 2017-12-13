/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
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

    ret = playgame(id, FM_PLAY);

    destroy_game_windows();
    discard_message_history(0);

    game_ended(ret, NULL, TRUE);
}

void
net_replay(void)
{
    char buf[BUFSZ];
    struct nhnet_game *gamelist;
    struct nh_menulist menu;
    int pick[1];
    int i, gamecount, gameid, want_done, show_all;

    want_done = FALSE;
    show_all = TRUE;
    while (1) {
        gamelist = nhnet_list_games(want_done, show_all, &gamecount);

        init_menulist(&menu);

        if (!gamecount)
            add_menu_txt(&menu, "(No games in this list)",
                         MI_NORMAL);

        /* add all the files to the menu */
        for (i = 0; i < gamecount; i++) {
            describe_game(buf, gamelist[i].status, &gamelist[i].i);
            add_menu_item(&menu, gamelist[i].gameid, buf, 0,
                          FALSE);
        }

        add_menu_txt(&menu, "", MI_NORMAL);
        if (want_done)
            add_menu_item(&menu, -1, show_all ? "Watch current games" :
                          "Replay your unfinished games", '!', FALSE);
        else
            add_menu_item(&menu, -1,
                          "Replay a completed game", '!', FALSE);

        if (show_all)
            add_menu_item(&menu, -2, want_done ? "View your completed games" :
                          "View your saved games", '#', FALSE);
        else
            add_menu_item(&menu, -2, "View games from other players",
                          '#', FALSE);

        curses_display_menu(&menu, want_done ? show_all ?
                            "Completed games by other players" :
                            "Your completed games" : show_all ?
                            "Pick a current game to watch" :
                            "Replay your saved games", PICK_ONE,
                            PLHINT_ANYWHERE, pick, curses_menu_callback);
        if (pick[0] == CURSES_MENU_CANCELLED)
            return;

        if (pick[0] == -1) {
            want_done = !want_done;
            if (want_done)
                show_all = FALSE; /* will normally be intended */
            continue;
        } else if (pick[0] == -2) {
            show_all = !show_all;
            continue;
        } else
            gameid = pick[0];

        break;
    }

    create_game_windows();

    /* If the game is over, we want to replay (nothing to watch). If it's our
       game, we want to replay (not much point in watching yourself). Otherwise,
       we start in watch mode; the player can change to replay mode from the
       watch menu. */
    int ret = playgame(gameid, show_all && !want_done ? FM_WATCH : FM_REPLAY);

    destroy_game_windows();
    discard_message_history(0);

    game_ended(ret, NULL, TRUE);
}
