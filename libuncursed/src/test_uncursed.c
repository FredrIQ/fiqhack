/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-11-13 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack general public license
 *  - the GNU General Public license v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This is a test file in order to check that uncursed is operating correctly. */

#include "uncursed.h"
#include <stdio.h>

#ifndef AIMAKE_BUILDOS_MSWin32
# include <signal.h>
#endif

static void
handle_sigusr1(int unused)
{
    (void)unused;
    uncursed_signal_getch();
}

int
main(int argc, char **argv)
{
    initialize_uncursed(&argc, argv);
    initscr();
    set_faketerm_font_file("./tilesets/dat/fonts/font14.png");

#ifndef AIMAKE_BUILDOS_MSWin32
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&(sa.sa_mask));

    sa.sa_handler = handle_sigusr1;
    sigaction(SIGUSR1, &sa, 0);    
#endif

    /* Test color pairs */
    start_color();
    init_pair(1, 10, 4);

    /* Testing input */
    clear();
    mvprintw(0, 0, "Press a key ('q' to exit)");
    refresh();

    while (1) {
        wint_t k;
        int r = get_wch(&k);

        if (r != KEY_CODE_YES && k == ('R' & 63))
            clear();
        erase();
        mvaddch(LINES - 2, COLS - 2, 'A');
        mvaddch(LINES - 2, COLS - 1, 'B');
        mvaddch(LINES - 1, COLS - 2, 'C');
        mvaddch(LINES - 1, COLS - 1, 'D'); /* ensure the bottom-right works */

        if (r == KEY_CODE_YES || k < 32) {
            mvprintw(0, 0, "%d %s", k, keyname(k));
            if (k == KEY_HANGUP)
                break;
        } else {
            cchar_t ct = { A_UNDERLINE | COLOR_PAIR(1), {k, 0} };
            mvprintw(0, 0, "%d '", k);
            add_wch(&ct);
            addch('\'');
        }

        refresh();
        if (k == 'q')
            break;
    }

    endwin();
    return 0;
}
