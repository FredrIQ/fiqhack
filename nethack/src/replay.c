/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
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

void
replay(void)
{
    char buf[BUFSZ];
    fnchar logdir[BUFSZ], savedir[BUFSZ], filename[1024], *dir, **files;
    struct nh_menulist menu;
    int i, fd, filecount, pick[1];
    enum nh_log_status status;
    struct nh_game_info gi;

    if (!get_gamedir(LOG_DIR, logdir))
        logdir[0] = '\0';
    if (!get_gamedir(SAVE_DIR, savedir))
        savedir[0] = '\0';

    if (*logdir)
        dir = logdir;
    else if (*savedir)
        dir = savedir;
    else {
        curses_msgwin("There are no games to replay.", krc_notification);
        return;
    }

    while (1) {
        filename[0] = '\0';
        files = list_gamefiles(dir, &filecount);
        /* make sure there are some files to show */
        if (!filecount) {
            if (dir == savedir) {
                curses_msgwin("There are no saved games to replay.",
                              krc_notification);
                savedir[0] = '\0';
            } else {
                curses_msgwin("There are no completed games to replay.",
                              krc_notification);
                logdir[0] = '\0';
            }

            dir = (dir == savedir) ? logdir : savedir;
            if (!*dir)
                return;
            continue;
        }

        init_menulist(&menu);

        if (dir == logdir && *savedir) {
            add_menu_item(&menu, -1, "View current games instead",
                          '!', FALSE);
            add_menu_txt(&menu, "", MI_NORMAL);
        } else if (dir == savedir && *logdir) {
            add_menu_item(&menu, -1,
                          "View completed games instead", '!', FALSE);
            add_menu_txt(&menu, "", MI_NORMAL);
        }

        /* add all the files to the menu */
        for (i = 0; i < filecount; i++) {
            fd = sys_open(files[i], O_RDWR, 0660);
            status = nh_get_savegame_status(fd, &gi);
            close(fd);

            describe_game(buf, status, &gi);
            add_menu_item(&menu, i + 1, buf, 0, FALSE);
        }

        curses_display_menu(
            &menu, (dir == savedir ? "Pick a current game to view" :
                    "Pick a completed game to view"),
            PICK_ONE, PLHINT_ANYWHERE, pick, curses_menu_callback);

        filename[0] = '\0';
        if (pick[0] != -1 && pick[0] != CURSES_MENU_CANCELLED)
            fnncat(filename, files[pick[0] - 1],
                   sizeof (filename) / sizeof (fnchar) - fnlen(filename) - 1);

        for (i = 0; i < filecount; i++)
            free(files[i]);
        free(files);

        if (pick[0] == CURSES_MENU_CANCELLED)
            return;

        if (pick[0] == -1) {
            dir = (dir == savedir) ? logdir : savedir;
            continue;
        }

        /* we have a valid filename */
        break;
    }

    fd = sys_open(filename, O_RDWR, FILE_OPEN_MASK);

    create_game_windows();

    /* Watching isn't so useful locally (you could only watch yourself, due to
       the permissions on the save file). So load in replay mode.

       Note that a GAME_OVER response is possible and meaningful; it means that
       the server discovered that the game over sequence was incomplete, and
       automatically entered recoverquit mode. */
    enum nh_play_status ret = playgame(fd, FM_REPLAY);

    close(fd);

    destroy_game_windows();
    discard_message_history(0);
    game_ended(ret, filename, FALSE);
}
