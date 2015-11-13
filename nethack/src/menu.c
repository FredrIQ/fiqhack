/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include "utf8conv.h"
#include <limits.h>
#include <inttypes.h>

#define BORDERWIDTH  (settings.whichframes != FRAME_NONE ? 4 : 2)
#define BORDERHEIGHT (settings.whichframes != FRAME_NONE ? 2 : 0)

/* Functions for scrollable windows */

void
layout_scrollable(struct gamewin *gw)
{
    struct win_scrollable *s = (struct win_scrollable *)gw->extra;

    int x1, x2, y1, y2;
    int scrheight;
    int scrwidth;

    x1 = (s->x1 > 0) ? s->x1 : 0;
    y1 = (s->y1 > 0) ? s->y1 : 0;
    x2 = (s->x2 > 0 && s->x2 <= COLS) ? s->x2 : COLS;
    y2 = (s->y2 > 0 && s->y2 <= LINES) ? s->y2 : LINES;

    scrheight = y2 - y1;
    scrwidth = x2 - x1;

    /* calculate height */
    s->frameheight = BORDERHEIGHT;
    if (s->title && s->title[0])
        s->frameheight++;
    if (settings.menupaging == MP_PAGES)
        s->frameheight++;    /* (1 of 2) or (end) */

    s->height = s->frameheight + s->maxlinecount;
    if (s->height > scrheight)
        s->height = scrheight;
    s->innerheight = s->height - s->frameheight;

    /* calculate width */
    s->innerwidth = s->wanted_width;
    if (s->title && s->innerwidth < strlen(s->title))
        s->innerwidth = strlen(s->title);
    if (s->innerwidth > scrwidth - BORDERWIDTH)
        s->innerwidth = scrwidth - BORDERWIDTH;
    s->width = s->innerwidth + BORDERWIDTH; /* border + space */
}

static void
setup_scrollable_win2(WINDOW *win, WINDOW **win2,
                      int innerheight, int innerwidth)
{
    *win2 = derwin(win, innerheight, innerwidth,
                   getmaxy(win) - innerheight -
                   (settings.whichframes != FRAME_NONE) -
                   (settings.menupaging == MP_PAGES),
                   1 + (settings.whichframes != FRAME_NONE));
    wset_mouse_event(*win2, uncursed_mbutton_wheelup,
                     KEY_UP, KEY_CODE_YES);
    wset_mouse_event(*win2, uncursed_mbutton_wheeldown,
                     KEY_DOWN, KEY_CODE_YES);
}

void
initialize_scrollable_windows(struct gamewin *gw, int starty, int startx)
{
    struct win_scrollable *s = (struct win_scrollable *)gw->extra;
    gw->win = newwin_onscreen(s->height, s->width, starty, startx);
    keypad(gw->win, TRUE);
    if (settings.whichframes != FRAME_NONE)
        nh_window_border(gw->win, s->dismissable);
    setup_scrollable_win2(gw->win, &(gw->win2),
                          s->innerheight, s->innerwidth);
    leaveok(gw->win, TRUE);
    leaveok(gw->win2, TRUE);
}

/* Draws the border and title of a scrollable, clearing the interior. */
void
draw_scrollable_frame(struct gamewin *gw)
{
    struct win_scrollable *s = (struct win_scrollable *)gw->extra;

    werase(gw->win);
    if (settings.whichframes != FRAME_NONE)
        nh_window_border(gw->win, s->dismissable);
    if (s->title) {
        wattron(gw->win, A_UNDERLINE);
        mvwaddnstr(gw->win, (settings.whichframes != FRAME_NONE),
                   (settings.whichframes != FRAME_NONE) + 1,
                   s->title, s->width - 4);
        wattroff(gw->win, A_UNDERLINE);
    }
    werase(gw->win2);
}

/* Changes the offset inside the given win_scrollable so that the given line is
   onscreen. Ensures that we're scrolled into a legal position (i.e. a whole
   number of pages with MP_PAGES, and not beyond the end of the screen with
   MP_LINES). */
void
scroll_onscreen(struct win_scrollable *s, int onscreen_offset)
{
    s->offset = onscreen_offset;
    switch (settings.menupaging) {
    case MP_LINES:
        if (s->offset > s->linecount - s->innerheight)
            s->offset = s->linecount - s->innerheight;
        if (s->offset < 0)
            s->offset = 0;
        break;
    case MP_PAGES:
        s->offset -= s->offset % s->innerheight;
        break;
    }
}

/* Handles most of the work of resizing a scrollable. Does not perform layout
   (because the caller may want something more complex than layout_scrollable),
   and does not do rendering (a scrollable is an abstract concept, and in
   particular, there's no draw_scrollable). */
void
resize_scrollable_inner(struct gamewin *gw)
{
    struct win_scrollable *s = (struct win_scrollable *)gw->extra;
    int startx, starty;

    delwin(gw->win2);
    wresize(gw->win, s->height, s->width);
    setup_scrollable_win2(gw->win, &(gw->win2), s->innerheight, s->innerwidth);

    starty = (LINES - s->height) / 2;
    startx = (COLS - s->width) / 2;

    mvwin(gw->win, starty, startx);
    scroll_onscreen(s, s->offset);
}

void
draw_scrollbar(WINDOW *win, struct win_scrollable *s)
{
    switch (settings.menupaging) {

    case MP_LINES:
        if (s->linecount > s->innerheight) {

            int scrltop, scrlheight, scrlpos, attr, i;
            scrltop = !!s->title + (settings.whichframes != FRAME_NONE);
            scrlheight = s->innerheight * s->innerheight / s->linecount;
            scrlpos = s->offset * (s->innerheight - scrlheight) /
                (s->linecount - s->innerheight);
            for (i = 0; i < s->innerheight; i++) {
                char ch = ' ';
                attr = A_NORMAL;
                if (i >= scrlpos && i < scrlpos + scrlheight) {
                    if (ui_flags.asciiborders)
                        ch = '#';
                    else
                        attr = A_REVERSE;
                }
                wattron(win, attr);
                mvwaddch(win, i + scrltop, s->width -
                         (settings.whichframes != FRAME_NONE) - 1, ch);
                wattroff(win, attr);
            }

        }
        break;

    case MP_PAGES:
        if (s->linecount <= s->innerheight)
            mvwprintw(win, s->height -
                      (settings.whichframes != FRAME_NONE) - 1, 2, "(end)");
        else
            mvwprintw(win, s->height -
                      (settings.whichframes != FRAME_NONE) - 1, 2,
                      "(%lg of %d)",
                      (double)(s->offset) / (double)(s->innerheight) + 1.,
                      (s->linecount - 1) / s->innerheight + 1);
        /* note: the floating-point arithmetic here should always produce an
           integer; we do it with floating-point so that something obviously
           wrong is displayed in cases where something went wrong */
        break;
    }
}

/* If keycode is a key that has a meaning to scrollable windows, update the
   given win_scrollable accordingly, and return TRUE. Otherwise return FALSE.
   Scrolling off the end of the menu (or pressing Return) will set *done to TRUE
   and return TRUE. Does not handle Escape, server cancels, etc., even though
   those work ina consistent way between scrollables (because they need
   different error returns). */
nh_bool
scroll_using_key(struct win_scrollable *s, int keycode, nh_bool *done)
{
    switch (keycode) {
    case KEY_UP:        /* one line up */
        if (s->offset > 0 && settings.menupaging == MP_LINES)
            s->offset--;
        return TRUE;

    case KEY_DOWN:      /* one line down */
        if (s->offset < s->linecount - s->innerheight &&
            settings.menupaging == MP_LINES)
            s->offset++;
        return TRUE;

    case KEY_PPAGE:     /* page up */
    case '<':
        s->offset -= s->innerheight;
        if (s->offset < 0)
            s->offset = 0;
        return TRUE;

    case KEY_NPAGE:     /* page down */
    case '>':
    case ' ':
        s->offset += s->innerheight;
        if (s->offset >= s->linecount) {
            s->offset -= s->innerheight;
            if (keycode == ' ')
                *done = TRUE;
        }
        return TRUE;

    case KEY_HOME:      /* go to the top */
    case '^':
        s->offset = 0;
        return TRUE;

    /* Go to the end. This acts differently in line-at-a-time and
       page-at-a-time scrolling; in page-at-a-time, we want to go to the
       last page, in line-at-a-time, we want the last line at the bottom
       of the window. */
    case KEY_END:
    case '|':
        switch (settings.menupaging) {
        case MP_LINES:
            s->offset = max(s->linecount - s->innerheight, 0);
            break;
        case MP_PAGES:
            s->offset = (s->linecount - 1);
            s->offset -= s->offset % s->innerheight;
            if (s->offset < 0) /* 0-item menu */
                s->offset = 0;
            break;
        }
        return TRUE;

    case KEY_ENTER:     /* confirm */
    case '\r':
        *done = TRUE;
        return TRUE;

    default:
        return FALSE;
    }
}


/* Functions that are the "same" for object and non-object menus, but
   reference different structures and thus produce different code */

static void
menu_search_callback(const char *sbuf, void *mdat_void)
{
    struct win_menu *mdat = mdat_void;
    int i;

    for (i = 0; i < mdat->s.linecount; i++)
        if (strstr(mdat->visitems[i]->caption, sbuf))
            break;
    if (i < mdat->s.linecount)
        scroll_onscreen(&(mdat->s), i);
}
static void
objmenu_search_callback(const char *sbuf, void *mdat_void)
{
    struct win_objmenu *mdat = mdat_void;
    int i;

    for (i = 0; i < mdat->s.linecount; i++)
        if (strstr(mdat->items[i].caption, sbuf))
            break;
    if (i < mdat->s.linecount)
        scroll_onscreen(&(mdat->s), i);
}

static void
assign_menu_accelerators(struct win_menu *mdat)
{
    int i;
    char accel = 'a';

    for (i = 0; i < mdat->s.linecount; i++) {

        if (mdat->visitems[i]->accel ||
            mdat->visitems[i]->role != MI_NORMAL ||
            mdat->visitems[i]->id == 0)
            continue;

        mdat->visitems[i]->accel = accel;

        if (accel == 'z')
            accel = 'A';
        else if (accel == 'Z')
            accel = 'a';
        else
            accel++;
    }
}
static void
assign_objmenu_accelerators(struct win_objmenu *mdat)
{
    int i;
    char accel = 'a';

    for (i = 0; i < mdat->s.linecount; i++) {

        if (mdat->items[i].accel || mdat->items[i].role != MI_NORMAL ||
            mdat->items[i].id == 0)
            continue;

        mdat->items[i].accel = accel;

        if (accel == 'z')
            accel = 'A';
        else if (accel == 'Z')
            accel = 'a';
        else
            accel++;
    }
}

/* Functions for non-object menus (specifically) */

static int
calc_colwidths(char *menustr, int *colwidth, int level)
{
    char *start, *tab;
    int col = 0;

    start = menustr;
    while ((tab = strchr(start, '\t')) != NULL && col < MAXCOLS) {
        *tab = '\0';
        colwidth[col] = max(colwidth[col],
                            utf8_wcswidth(start, SIZE_MAX) + level * 2);
        *tab = '\t';
        start = tab + 1;
        col++;
        level = 0;
    }
    colwidth[col] = max(colwidth[col],
                        utf8_wcswidth(start, SIZE_MAX) + level * 2);
    return col;
}

static int
calc_menuwidth(int *colwidth, int *colpos, int maxcol)
{
    int i;

    for (i = 1; i <= maxcol; i++)
        colpos[i] = colpos[i - 1] + colwidth[i - 1] + 1;
    return colpos[maxcol] + colwidth[maxcol];
}

static void
layout_menu(struct gamewin *gw)
{
    struct win_menu *mdat = (struct win_menu *)gw->extra;

    int i, col, w;
    int colwidth[MAXCOLS];
    int singlewidth = 0;

    /* Calculate the width we're aiming for. */
    mdat->maxcol = 0;
    for (i = 0; i < MAXCOLS; i++)
        colwidth[i] = 0;

    for (i = 0; i < mdat->s.linecount; i++) {
        /* Headings without tabs are not fitted into columns, but headings with
           tabs are presumably column titles. */
        if (!strchr(mdat->visitems[i]->caption, '\t')) {
            w = strlen(mdat->visitems[i]->caption);
            if (mdat->visitems[i]->role == MI_NORMAL &&
                mdat->visitems[i]->id)
                w += 4;
            singlewidth = max(singlewidth, w);
        } else {
            col = calc_colwidths(mdat->visitems[i]->caption, colwidth,
                                 mdat->visitems[i]->level);
            mdat->maxcol = max(mdat->maxcol, col);
        }
    }
    if (mdat->how != PICK_NONE)
        colwidth[0] += 4;       /* "a - " */

    mdat->s.wanted_width =
        max(calc_menuwidth(colwidth, mdat->colpos, mdat->maxcol), singlewidth);

    layout_scrollable(gw);

    mdat->colpos[mdat->maxcol + 1] = mdat->s.innerwidth + 1;
}

void
draw_menu(struct gamewin *gw)
{
    struct win_menu *mdat = (struct win_menu *)gw->extra;
    struct nh_menuitem *item;
    char caption[BUFSZ];
    char *colstrs[MAXCOLS];
    int i, j, col;
    char *tab;

    draw_scrollable_frame(gw);

    /* Draw the menu items. It's possible to scroll beyond the bottom of the
       menu (e.g. by using PgDn when the menu isn't a whole number of pages),
       so ensure we don't render any nonexistent menu items. */
    item = mdat->visitems[mdat->s.offset];
    for (i = 0; i < mdat->s.innerheight &&
             (i + mdat->s.offset) < mdat->s.linecount; i++, item++) {
        strncpy(caption + (item->level * 2), item->caption,
                BUFSZ - 1 - (item->level * 2));
        for (j = 0; j < (item->level * 2); j++)
            caption[j] = ' ';
        caption[BUFSZ - 1] = '\0';

        col = 0;
        colstrs[col] = caption;
        while ((tab = strchr(colstrs[col], '\t')) != NULL) {
            col++;
            *tab = '\0';
            colstrs[col] = tab + 1;
        }

        if (item->role == MI_HEADING)
            wattron(gw->win2, settings.menu_headings);

        wmove(gw->win2, i, item->level * 2);

        if (mdat->how != PICK_NONE && item->role == MI_NORMAL && item->accel) {
            wset_mouse_event(gw->win2, uncursed_mbutton_left, item->accel, OK);
            wprintw(gw->win2, "%c %c ", item->accel,
                    *(mdat->visselected[mdat->s.offset + i]) ? '+' : '-');
        }

        /* TODO: This isn't quite correct, we're taking a number of codepoints
           from the string equal to the width of the output we want, and that
           doesn't work correctly with combining characters. */
        if (col) {
            for (j = 0; j <= col; j++) {
                wchar_t wcol[utf8_wcswidth(colstrs[j], SIZE_MAX) + 1];
                if (utf8_mbstowcs(wcol, colstrs[j], sizeof wcol) != (size_t) -1)
                    waddwnstr(gw->win2, wcol + (j ? 0 : item->level * 2),
                              mdat->colpos[j + 1] - mdat->colpos[j] -
                              1 - (j ? 0 : item->level * 2));

                if (j < col)
                    wmove(gw->win2, i, mdat->colpos[j + 1]);
            }
        } else {
            wchar_t wcap[utf8_wcswidth(caption, SIZE_MAX) + 1];
            if (utf8_mbstowcs(wcap, caption, sizeof wcap) != (size_t) -1)
                waddwnstr(gw->win2, wcap, mdat->s.innerwidth);
        }

        if (item->role == MI_HEADING)
            wattroff(gw->win2, settings.menu_headings);

        wset_mouse_event(gw->win2, uncursed_mbutton_left, 0, ERR);
    }

    draw_scrollbar(gw->win, &(mdat->s));
}

static void
resize_menu(struct gamewin *gw)
{
    layout_menu(gw);
    resize_scrollable_inner(gw);
    draw_menu(gw);
}


static int
find_accel(int accel, struct win_menu *mdat)
{
    int i, upper;

    /* Scan visible entries first: long lists (e.g. the options menu) might
       re-use accelerators, so that each is only unique among the visible menu
       items */
    upper = min(mdat->s.linecount, mdat->s.offset + mdat->s.innerheight);
    for (i = mdat->s.offset; i < upper; i++)
        if (mdat->visitems[i]->accel == accel)
            return i;

    /* It's not a regular accelerator; maybe there is a group accelerator? */
    for (i = mdat->s.offset; i < upper; i++)
        if (mdat->visitems[i]->group_accel == accel)
            return i;

    /* Extra effort: if the list is too long for one page search for the accel
       among those entries too, and scroll the changed item into view. */
    if (mdat->s.linecount > mdat->s.innerheight)
        for (i = 0; i < mdat->s.linecount; i++)
            if (mdat->visitems[i]->accel == accel) {
                scroll_onscreen(&(mdat->s), i);
                return i;
            }

    return -1;
}

void
curses_display_menu_core(struct nh_menulist *ml, const char *title, int how,
                         void *callbackarg,
                         void (*callback)(const int *, int, void *),
                         int x1, int y1, int x2, int y2, nh_bool bottom,
                         nh_bool(*changefn) (struct win_menu *, int),
                         nh_bool dismissable)
{
    struct gamewin *gw;
    struct win_menu *mdat;
    int i, key, idx, rv, startx, starty, prevcurs;
    nh_bool done, cancelled, servercancelled;
    char selected[ml->icount ? ml->icount : 1];
    int results[ml->icount ? ml->icount : 1];

    if (isendwin() || COLS < COLNO || LINES < ROWNO) {
        dealloc_menulist(ml);
        callback(results, -1, callbackarg);
        return;
    }

    /* Make a stack-allocated copy of the menulist, in case we end up longjmping
       out of here before we get a chance to deallocate it. */
    struct nh_menuitem item_copy[ml->icount ? ml->icount : 1];
    if (ml->icount)
        memcpy(item_copy, ml->items, sizeof item_copy);
    struct nh_menuitem *visitem_pointers[ml->icount ? ml->icount : 1];
    for (i = 0; i < ml->icount; i++)
        visitem_pointers[i] = item_copy + i;
    char *visselected_pointers[ml->icount ? ml->icount : 1];
    for (i = 0; i < ml->icount; i++)
        visselected_pointers[i] = selected + i;

    memset(selected, 0, sizeof selected);

    prevcurs = nh_curs_set(0);

    gw = alloc_gamewin(sizeof (struct win_menu), FALSE);
    gw->draw = draw_menu;
    gw->resize = resize_menu;

    mdat = (struct win_menu *)gw->extra;
    mdat->visitems = visitem_pointers;
    mdat->s.linecount = ml->icount;
    mdat->s.maxlinecount = ml->icount;
    mdat->s.title = title;
    mdat->how = how;
    mdat->visselected = visselected_pointers;
    mdat->s.x1 = x1;
    mdat->s.y1 = y1;
    mdat->s.x2 = x2;
    mdat->s.y2 = y2;
    mdat->s.dismissable = !!dismissable;
    mdat->s.offset = 0;
    if (mdat->how != PICK_ONE && mdat->s.dismissable)
        mdat->s.dismissable = 2;

    dealloc_menulist(ml);

    if (x1 < 0)
        x1 = COLS;
    if (y1 < 0)
        y1 = LINES;
    if (x2 < 0)
        x2 = COLS;
    if (y2 < 0)
        y2 = LINES;

    assign_menu_accelerators(mdat);
    layout_menu(gw);

    starty = (y2 - y1 - mdat->s.height) / 2 + y1;
    startx = (x2 - x1 - mdat->s.width) / 2 + x1;

    initialize_scrollable_windows(gw, starty, startx);
    done = FALSE;
    cancelled = servercancelled = FALSE;

    if (bottom && mdat->s.linecount - mdat->s.innerheight > 0)
        mdat->s.offset = mdat->s.linecount - mdat->s.innerheight;
    while (!done && !cancelled) {
        draw_menu(gw);

        key = nh_wgetch(gw->win, krc_menu);

        if (!scroll_using_key(&(mdat->s), key, &done))
            switch (key) {
            case KEY_ESCAPE:                /* cancel */
            case '\x1b':
                cancelled = TRUE;
                break;

            case KEY_SIGNAL:
                cancelled = TRUE;
                servercancelled = TRUE;
                break;

            case '.':                       /* select all (even hidden) */
                if (mdat->how == PICK_ANY)
                    for (i = 0; i < mdat->s.maxlinecount; i++)
                        if (item_copy[i].role == MI_NORMAL)
                            selected[i] = TRUE;
                break;

            case '-':                       /* select none (even hidden) */
                for (i = 0; i < mdat->s.maxlinecount; i++)
                    selected[i] = FALSE;
                break;

            case ':':                       /* search for a menu item */
                curses_getline("Search:", mdat, menu_search_callback);
                break;

            /* try to find an item for this key and, if one is found, select it */
            default:
                if (mdat->how == PICK_LETTER) {
                    if (key >= 'a' && key <= 'z') {
                        /* Since you can choose a letter outside of the menu range,
                           this needs to bypass the normal results-setting code. */
                        results[0] = key - 'a' + 1;
                        rv = 1;
                        goto cleanup_and_return;
                    } else if (key >= 'A' && key <= 'Z') {
                        results[0] = key - 'A' + 27;
                        rv = 1;
                        goto cleanup_and_return;
                    }
                }
                idx = find_accel(key, mdat);

                if (idx != -1 &&    /* valid accelerator */
                    (!changefn || changefn(mdat, idx))) {
                    *(mdat->visselected[idx]) = !*(mdat->visselected[idx]);

                    if (mdat->how == PICK_ONE)
                        done = TRUE;
                }
                break;
            }
    }

    if (cancelled)
        rv = servercancelled ? -2 : -1;
    else {
        rv = 0;
        for (i = 0; i < mdat->s.maxlinecount; i++) {
            if (selected[i]) {
                results[rv] = item_copy[i].id;
                rv++;
            }
        }
    }

cleanup_and_return:
    delete_gamewin(gw);
    redraw_game_windows();
    nh_curs_set(prevcurs);

    callback(results, rv, callbackarg);
}

void
curses_menu_callback(const int *results, int nresults, void *arg)
{
    if (nresults > 0)
        memcpy(arg, results, nresults * sizeof *results);
    else {
        /* Getting a server cancel while in one of our own menus is awkward.
           This could potentially happen if someone opens the options menu
           while watching a game, and the watched game starts taking turns.
           Our current approach is just to close the menu as if cancelled,
           and propagate the cancel forwards to the next server request.

           TODO: Reopen the menu again, if possible. (This is not easy.) */
        if (nresults == -2 && ui_flags.ingame)
            uncursed_signal_getch();
        *(int *)arg = CURSES_MENU_CANCELLED;
    }
}

void
curses_display_menu(struct nh_menulist *ml, const char *title,
                    int how, int placement_hint, void *callbackarg,
                    void (*callback)(const int *, int, void *))
{
    int x1 = 0, y1 = 0, x2 = -1, y2 = -1;

    if (msgwin)
        draw_messages_precover();
    redraw_popup_windows();

    /* Even while watching/replaying, these menus take input. */
    if (placement_hint == PLHINT_URGENT)
        ui_flags.in_zero_time_command = TRUE;

    if (placement_hint == PLHINT_ONELINER && game_is_running) {
        x1 = 0;
        if (ui_flags.sidebarwidth)
            x2 = getmaxx(sidebar);
        y1 = 0;
        y2 = getmaxy(msgwin);
    } else if (!ui_flags.sidebarwidth && game_is_running) {
        /* If we have no sidebar, place all menus but oneliners in the top-right
           corner. */
        x1 = -1;
        y2 = 0;
    } else if (placement_hint == PLHINT_INVENTORY ||
               placement_hint == PLHINT_CONTAINER) {
        if (ui_flags.sidebarwidth)
            x1 = COLS - getmaxx(sidebar);
        else
            x1 = COLS - 64;
    }

    curses_display_menu_core(ml, title, how, callbackarg, callback,
                             x1, y1, x2, y2, FALSE, NULL, TRUE);
}


/* Functions for object menus (specifically) */

static void
layout_objmenu(struct gamewin *gw)
{
    struct win_objmenu *mdat = (struct win_objmenu *)gw->extra;
    char weightstr[16];
    int i, maxwidth, itemwidth;

    /* calc width */
    maxwidth = 0;
    for (i = 0; i < mdat->s.linecount; i++) {
        itemwidth = strlen(mdat->items[i].caption);

        /* Add extra space for an object symbol.

           TODO: We don't need this any more, do we? */
        if (mdat->items[i].role == MI_NORMAL)
            itemwidth += 2;

        /* If the weight is known, leave space to show it. */
        if (settings.invweight && mdat->items[i].weight != -1) {
            snprintf(weightstr, ARRAY_SIZE(weightstr),
                     " {%d}", mdat->items[i].weight);
            itemwidth += strlen(weightstr);
        }
        if ((mdat->items[i].role == MI_NORMAL && mdat->items[i].accel) ||
            (mdat->items[i].role == MI_HEADING && mdat->items[i].group_accel))
            itemwidth += 4;     /* "a - " or " ')'" */
        maxwidth = max(maxwidth, itemwidth);
    }

    mdat->s.wanted_width = maxwidth;
    layout_scrollable(gw);
}

void
draw_objlist(WINDOW * win, struct nh_objlist *objlist, int *selected, int how)
{
    int i, maxitem, txtattr, width, pos;
    char buf[BUFSZ];

    width = getmaxx(win);
    werase(win);

    /* draw menu items */
    maxitem = min(getmaxy(win), objlist->icount);
    for (i = 0; i < maxitem; i++) {
        struct nh_objitem *const olii = objlist->items + i;

        wmove(win, i, 0);
        pos = 0;
        wattrset(win, 0);
        txtattr = A_NORMAL;

        if (olii->role != MI_NORMAL) {
            if (olii->role == MI_HEADING)
                txtattr = settings.menu_headings;
            wattron(win, txtattr);
            pos += snprintf(buf + pos, BUFSZ - pos, "%s", olii->caption);
            if (olii->group_accel)
                snprintf(buf + pos, BUFSZ - pos, " '%c'", olii->group_accel);
            waddnstr(win, buf, width);
            wattroff(win, txtattr);
            continue;
        }

        if (how != PICK_NONE && selected && olii->accel) {
            wset_mouse_event(win, uncursed_mbutton_left, olii->accel, OK);
            switch (selected[i]) {
            case -1:
                pos += snprintf(buf + pos, BUFSZ - pos, "%c + ", olii->accel);
                break;
            case 0:
                pos += snprintf(buf + pos, BUFSZ - pos, "%c - ", olii->accel);
                break;
            default:
                pos += snprintf(buf + pos, BUFSZ - pos, "%c # ", olii->accel);
                break;
            }
        } else if (olii->accel) {
            /* We use user-defined key codes in the range KEY_MAX + letter for
               itemactions. We check that the letter is valid to avoid issues
               due to inventory overflow slots. */
            if (!selected && ((olii->accel >= 'A' && olii->accel <= 'Z') ||
                              (olii->accel >= 'a' && olii->accel <= 'z') ||
                              olii->accel == '$'))
                wset_mouse_event(win, uncursed_mbutton_left,
                                 olii->accel + KEY_MAX, KEY_CODE_YES);
            pos += snprintf(buf + pos, BUFSZ - pos, "%c - ", olii->accel);
        }

        if (olii->worn)
            txtattr |= A_BOLD;
        switch (olii->buc) {
        case B_CURSED:
            txtattr |= COLOR_PAIR(2);
            break;
        case B_BLESSED:
            txtattr |= COLOR_PAIR(7);
            break;
        default:
            break;
        }
        wattron(win, txtattr);
        pos += snprintf(buf + pos, BUFSZ - pos, "%s", olii->caption);
        if (settings.invweight && olii->weight != -1) {
            pos += snprintf(buf + pos, BUFSZ - pos, " {%d}", olii->weight);
        }
        waddnstr(win, buf, width - 1);
        wattroff(win, txtattr);

        wset_mouse_event(win, uncursed_mbutton_left, 0, ERR);
    }
    wnoutrefresh(win);
}

static void
draw_objmenu(struct gamewin *gw)
{
    struct win_objmenu *mdat = (struct win_objmenu *)gw->extra;

    draw_scrollable_frame(gw);
    draw_objlist(gw->win2,
                 &(struct nh_objlist){
                     .icount = mdat->s.linecount - mdat->s.offset,
                     .items  = mdat->items       + mdat->s.offset},
                 mdat->selected + mdat->s.offset, mdat->how);

    if (mdat->selcount > 0 && settings.whichframes != FRAME_NONE) {
        wmove(gw->win, getmaxy(gw->win) - 1, 1);
        wprintw(gw->win, "Count: %d", mdat->selcount);
    }

    draw_scrollbar(gw->win, &(mdat->s));
}


static void
resize_objmenu(struct gamewin *gw)
{
    layout_objmenu(gw);
    resize_scrollable_inner(gw);
    draw_objmenu(gw);
}

static int
find_objaccel(int accel, struct win_objmenu *mdat)
{
    int i, upper;

    /* Scan visible entries first: long lists (e.g. the options menu) might
       re-use accelerators, so that each is only unique among the visible menu
       items */
    upper = min(mdat->s.linecount, mdat->s.offset + mdat->s.innerheight);
    for (i = mdat->s.offset; i < upper; i++)
        if (mdat->items[i].accel == accel)
            return i;

    /* Don't handle group accelerators here, because they need special handling
       for the item counts. */

    /* Extra effort: if the list is too long for one page, search for the accel
       among those entries too. */
    if (mdat->s.linecount > mdat->s.innerheight)
        for (i = 0; i < mdat->s.linecount; i++)
            if (mdat->items[i].accel == accel) {
                scroll_onscreen(&(mdat->s), i);
                return i;
            }

    return -1;
}

void
curses_display_objects(
    struct nh_objlist *objlist, const char *title, int how,
    int placement_hint, void *callbackarg,
    void (*callback)(const struct nh_objresult *, int, void *))
{
    struct gamewin *gw;
    struct win_objmenu *mdat;
    int i, key, idx, rv, startx, starty, prevcurs;
    nh_bool done, cancelled, servercancelled;
    nh_bool inventory_special = title && !!strstr(title, "Inventory") &&
        how == PICK_NONE;
    int selected[objlist->icount ? objlist->icount : 1];
    struct nh_objresult results[objlist->icount ? objlist->icount : 1];

    if (msgwin)
        draw_messages_precover();

    if (isendwin() || COLS < COLNO || LINES < ROWNO) {
        dealloc_objmenulist(objlist);
        callback(results, -1, callbackarg);
        return;
    }

    /* Make a stack-allocated copy of the objlist, in case we end up longjmping
       out of here before we get a chance to deallocate it. */
    struct nh_objitem item_copy[objlist->icount ? objlist->icount : 1];
    if (objlist->icount)
        memcpy(item_copy, objlist->items, sizeof item_copy);

    memset(selected, 0, sizeof selected);

    if (inventory_special)
        placement_hint = PLHINT_INVENTORY;

    prevcurs = nh_curs_set(0);

    gw = alloc_gamewin(sizeof (struct win_objmenu), FALSE);
    gw->draw = draw_objmenu;
    gw->resize = resize_objmenu;

    mdat = (struct win_objmenu *)gw->extra;
    mdat->items = item_copy;
    mdat->s.linecount = objlist->icount;
    mdat->s.maxlinecount = objlist->icount;
    mdat->s.title = title;
    mdat->how = inventory_special ? PICK_ONE : how;
    mdat->selcount = -1;
    mdat->selected = selected;
    mdat->s.x1 = 0;
    mdat->s.y1 = 0;
    mdat->s.x2 = 0;
    mdat->s.y2 = 0;
    mdat->s.dismissable = mdat->how == PICK_ONE ? 1 : 2;
    mdat->s.offset = 0;

    dealloc_objmenulist(objlist);

    if (how != PICK_NONE && !inventory_special)
        assign_objmenu_accelerators(mdat);
    layout_objmenu(gw);

    starty = (LINES - mdat->s.height) / 2;
    startx = (COLS - mdat->s.width) / 2;

    if (placement_hint == PLHINT_INVENTORY ||
        placement_hint == PLHINT_CONTAINER) {
        if (ui_flags.sidebarwidth)
            mdat->s.width = max(mdat->s.width, 2 + getmaxx(sidebar));
        starty = 0;
        startx = COLS - mdat->s.width;
    } else if (placement_hint == PLHINT_ONELINER && game_is_running) {
        starty = 0;
        startx = 0;
    }

    initialize_scrollable_windows(gw, starty, startx);
    done = FALSE;
    cancelled = servercancelled = FALSE;

    while (!done && !cancelled) {
        draw_objmenu(gw);

        key = nh_wgetch(gw->win, krc_objmenu);

        if (!scroll_using_key(&(mdat->s), key, &done))
            switch (key) {
            case KEY_ESCAPE:                       /* cancel */
            case '\x1b':
                cancelled = TRUE;
                break;

            case KEY_SIGNAL:
                cancelled = TRUE;
                servercancelled = TRUE;
                break;


            case '.':                              /* select all */
                if (mdat->how == PICK_ANY)
                    for (i = 0; i < mdat->s.linecount; i++)
                        if (mdat->items[i].oclass != -1)
                            mdat->selected[i] = -1;
                break;

            case '-':                              /* select none */
                for (i = 0; i < mdat->s.linecount; i++)
                    mdat->selected[i] = 0;
                break;

            case '@':                              /* invert all */
                if (mdat->how == PICK_ANY)
                    for (i = 0; i < mdat->s.linecount; i++)
                        if (mdat->items[i].oclass != -1)
                            mdat->selected[i] = mdat->selected[i] ? 0 : -1;
                                break;

            case ',':                              /* select page */
                if (mdat->how != PICK_ANY)
                    break;

                for (i = mdat->s.offset; i < mdat->s.linecount &&
                         i < mdat->s.offset + mdat->s.innerheight; i++)
                    if (mdat->items[i].oclass != -1)
                        mdat->selected[i] = -1;
                break;

            case '\\':                             /* deselect page */
                for (i = mdat->s.offset; i < mdat->s.linecount &&
                         i < mdat->s.offset + mdat->s.innerheight; i++)
                    if (mdat->items[i].oclass != -1)
                        mdat->selected[i] = 0;
                break;

            case '~':                              /* invert page */
                if (mdat->how != PICK_ANY)
                    break;

                for (i = mdat->s.offset; i < mdat->s.linecount &&
                         i < mdat->s.offset + mdat->s.innerheight; i++)
                    if (mdat->items[i].oclass != -1)
                        mdat->selected[i] = mdat->selected[i] ? 0 : -1;
                break;

            case ':':                              /* search for a menu item */
                curses_getline("Search:", mdat, objmenu_search_callback);
                break;

            case KEY_BACKSPACE:                    /* edit selection count */
                mdat->selcount /= 10;
                if (mdat->selcount == 0)
                    mdat->selcount = -1;    /* -1: select all */
                break;

            default:
                /* selection allows an item count */
                if (key >= '0' && key <= '9') {
                    if (mdat->selcount == -1)
                        mdat->selcount = 0;
                    mdat->selcount = mdat->selcount * 10 + (key - '0');
                    if (mdat->selcount > 0xffff)
                        mdat->selcount /= 10;

                    break;
                }

                /* try to find an item for this key and, if one is found, select
                   it */
                idx = find_objaccel(key, mdat);

                if (idx != -1) {    /* valid item accelerator */
                    if (mdat->selected[idx])
                        mdat->selected[idx] = 0;
                    else
                        mdat->selected[idx] = mdat->selcount;
                    mdat->selcount = -1;

                    /* inventory special case: show item actions menu */
                    if (inventory_special) {
                        mdat->selected[idx] = 0;
                        if (do_item_actions(&mdat->items[idx]))
                            done = TRUE;
                    } else if (mdat->how == PICK_ONE)
                        done = TRUE;

                } else if (mdat->how == PICK_ANY) { /* group accel? */
                    int grouphits = 0;

                    for (i = 0; i < mdat->s.linecount; i++) {
                        if (mdat->items[i].group_accel == key &&
                            mdat->items[i].oclass != -1) {
                            if (mdat->selected[i] == mdat->selcount)
                                mdat->selected[i] = 0;
                            else
                                mdat->selected[i] = mdat->selcount;
                            grouphits++;
                        }
                    }

                    if (grouphits)
                        mdat->selcount = -1;
                }
                break;
            }
    }

    if (cancelled)
        rv = servercancelled ? -2 : -1;
    else {
        rv = 0;
        for (i = 0; i < mdat->s.linecount; i++) {
            if (mdat->selected[i]) {
                results[rv].id = mdat->items[i].id;
                results[rv].count = mdat->selected[i];
                rv++;
            }
        }
    }

    delete_gamewin(gw);
    redraw_game_windows();
    nh_curs_set(prevcurs);

    callback(results, rv, callbackarg);
}


nh_bool
do_item_actions(const struct nh_objitem *item)
{
    int ccount = 0, i, selected[1];
    struct nh_cmd_desc *obj_cmd = nh_get_object_commands(&ccount, item->accel);
    char title[QBUFSZ];
    struct nh_menulist menu;
    struct nh_cmd_arg arg;

    if (!obj_cmd || !ccount)
        return FALSE;

    init_menulist(&menu);

    for (i = 0; i < ccount; i++)
        add_menu_item(&menu, i + 1, obj_cmd[i].desc, obj_cmd[i].defkey, FALSE);

    if (settings.invweight && item->weight != -1) {
        snprintf(title, QBUFSZ, "%c - %s {%d}", item->accel, item->caption,
                 item->weight);
    } else {
        snprintf(title, QBUFSZ, "%c - %s", item->accel, item->caption);
    }
    curses_display_menu(&menu, title, PICK_ONE, PLHINT_INVENTORY,
                        selected, curses_menu_callback);

    if (*selected == CURSES_MENU_CANCELLED)
        return FALSE;

    arg.argtype = CMD_ARG_OBJ;
    arg.invlet = item->accel;
    set_next_command(obj_cmd[selected[0] - 1].name, &arg);

    return TRUE;
}
