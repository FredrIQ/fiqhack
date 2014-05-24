/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-25 */
/* Copyright (c) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"


/* Returns TRUE if two ranges of integers have any elements in common. The
   algorithm is to find which range starts first, and then ensure it finishes
   before the other range starts. */
static nh_bool
range_overlap(int min1, int len1, int min2, int len2)
{
    if (min1 < min2)
        return min1 + len1 <= min2;
    else
        return min2 + len2 <= min1;
}

void
draw_extrawin(enum keyreq_context context)
{
    if (!ui_flags.ingame)
        return;

    /* Which window do we draw our hints onto? Nearly always, it's extrawin.
       However, during getpos() calls, the hints are particularly important
       (because this is how farlooking is done with keyboard controls), so we
       steal the space used by statuswin if we don't have an extrawin to draw
       onto. The y height is read from the appropriate field of ui_flags; both
       the windows are width ui_flags.mapwith, so we don't need to record
       that. */

    int y_remaining = ui_flags.extraheight;
    int y = 0;
    WINDOW *win = extrawin;
    if (!y_remaining && context == krc_getpos) {
        y_remaining = ui_flags.statusheight;
        win = statuswin;
    }

    if (!y_remaining)
        return;

    /* Modal windows are allowed to steal any space they feel like. Nearly
       always, this will avoid the extrawin, but it can overlap in some cases
       (e.g. an inventory window full of inventory on a terminal that is both
       very tall and very narrow). In such cases, drawing the extra window would
       be a) difficult (due to handling the overlap correctly while also
       handling mouse regions correctly), and b) pointless, because the user
       wouldn't be able to see the information it was trying to give.  So just
       leave it alone. */
    struct gamewin *gw;
    for (gw = firstgw; gw; gw = gw->next) {
        int t, l, h, w, t2, l2, h2, w2;
        getmaxyx(gw->win, h, w);
        getbegyx(gw->win, t, l);
        getmaxyx(win, h2, w2);
        getbegyx(win, t2, l2);

        if (range_overlap(t, h, t2, h2) || range_overlap(l, w, l2, w2))
            return;
    }

    /* We're OK to start drawing, and we can refresh win as much as we like
       without it overwriting anyone else's space. Because draw_extrawin is
       called from nh_wgetch (i.e. very very late), we can also set mouse
       regions without a modal dialog box turning them back off. */
    werase(win);

    /* We fit as many hints as we can into the space provided (that is, while
       y_remaining is stil positive), starting with the most important and
       moving down to progressively less important ones as time goes on. */

#define spend_one_line()                        \
    do {                                        \
        if (!y_remaining--) {                   \
            wnoutrefresh(win);                  \
            return;                             \
        }                                       \
        wmove(win, y++, 0);                     \
    } while(0)

    /* Most important: map hover (i.e. mouselook). This also handles keyboard
       look in the case of the getpos prompt (which basically hijacks the hover
       information, reporting whichever of the keyboard or mouse produced input
       last). Note that the mouse doesn't confirm a location in getpos unless
       you actually press it, nor move the cursor, so pressing . doesn't
       necessarily get the position that's being hinted. */
    if (ui_flags.maphoverx != -1) {

        spend_one_line();

        struct nh_desc_buf descbuf;
        nh_describe_pos(ui_flags.maphoverx, ui_flags.maphovery,
                        &descbuf, NULL);

        int x_remaining = ui_flags.mapwidth - 1;
        nh_bool first = TRUE;
        int l;

#define place_desc_message(s)                   \
        do {                                    \
            l = strlen(s);                      \
            if (l && !first) l += 2;            \
            if (l && l <= x_remaining) {        \
                if (!first)                     \
                    waddstr(win, "; ");         \
                waddstr(win, s);                \
                x_remaining -= l;               \
                first = FALSE;                  \
            }                                   \
        } while(0)

        place_desc_message(descbuf.effectdesc);
        place_desc_message(descbuf.invisdesc);
        place_desc_message(descbuf.mondesc);
        place_desc_message(descbuf.objdesc);
        place_desc_message(descbuf.trapdesc);
        place_desc_message(descbuf.bgdesc);

    }

    /* Next most important: keymaps for unusual contexts.

       These have to be kept under 80 characters long, and are written as two
       lines of 40 for easier counting. If using hintline(), as many lines from
       the start as will fit will be displayed. */

#define hintline(s) do { spend_one_line(); waddstr(win, s); } while(0)

    if (context == krc_getpos) {
        /* ------ 1234567890123456789012345678901234567890 */
        hintline("Move the cursor with the direction keys."
                 " When finished, confirm with . , : or ;" );
        hintline("Press the letter of a dungeon symbol to "
                 "select it or m/M to move to a monster."  );
    }

    wnoutrefresh(win);
}

/* This is a separate function mostly in case it expands in the future. */
void
clear_extrawin(void)
{
    if (extrawin && ui_flags.ingame)
        werase(extrawin);
    wnoutrefresh(extrawin);
}
