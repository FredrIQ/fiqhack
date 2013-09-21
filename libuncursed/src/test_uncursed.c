/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
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

int main(int argc, char **argv) {
    initialize_uncursed(&argc, argv);
    initscr();
    /* Testing input */
    clear();
    mvprintw(0, 0, "Press a key ('q' to exit)");
    while (1) {
        wint_t k;
        int r = get_wch(&k);
        erase();
        if (r == KEY_CODE_YES || k < 32)
            mvprintw(0, 0, "%d %s", (int)k, keyname(k));
        else {
            cchar_t ct = {A_UNDERLINE, {k, 0}};
            mvprintw(0, 0, "%d '", (int)k);
            add_wch(&ct);
            addch('\'');
        }
        refresh();
        if (k == 'q') break;
    }
    endwin();
    return 0;
}
