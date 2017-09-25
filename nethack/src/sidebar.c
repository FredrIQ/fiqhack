/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-09-25 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"


static WINDOW *invwin, *objwin;
static struct nh_objlist flooritems, inventory;

static void
curses_list_items_core(struct nh_objlist *objlist, nh_bool invent, nh_bool draw)
{
    struct nh_objlist *list;

    if (invent) {
        list = &inventory;
    } else {
        list = &flooritems;
    }

    dealloc_objmenulist(list);

    if (objlist->icount) {
        list->items = malloc(objlist->icount * sizeof (struct nh_objitem));
        list->size = objlist->icount;
        list->icount = objlist->icount;
        memcpy(list->items, objlist->items,
               objlist->icount * sizeof (struct nh_objitem));
    }

    dealloc_objmenulist(objlist);

    if (draw)
        draw_sidebar();
}


void
curses_list_items(struct nh_objlist *objlist, nh_bool invent)
{
    curses_list_items_core(objlist, invent, TRUE);
}

void
curses_list_items_nonblocking(struct nh_objlist *objlist, nh_bool invent)
{
    curses_list_items_core(objlist, invent, FALSE);
}


static int
count_omitted_items(struct nh_objlist *list, int pos)
{
    int omitted = 0;

    for (; pos < list->icount; pos++)
        if (list->items[pos].role == MI_NORMAL)
            omitted++;
    return omitted;
}

/* Returns nonzero if we have both an inventory and flooritem part in sidebar,
   for the purpose of joining borders. */
int
sidebar_delimiter_pos(void)
{
    if (!inventory.icount || !flooritems.icount)
        return 0;

    int viewheight = LINES - (ui_flags.draw_outer_frame_lines ? 2 : 0);
    int invheight = min(inventory.icount + 1, viewheight / 2);
    int flheight = min(flooritems.icount + 1, (viewheight - 1) / 2);
    /* if there is need and available space, expand the floor item list up */
    if (flooritems.icount + 1 > flheight &&
        invheight < (viewheight + 1) / 2)
        flheight =
            min(flooritems.icount + 1, viewheight - invheight - 1);
    /* assign all unused space to the inventory list whether it is needed
       or not */
    invheight = viewheight - flheight - 1;
    return invheight + !!ui_flags.draw_outer_frame_lines;
}

void
draw_sidebar(void)
{
    int flheight = 0, invheight = 0, invwh, flwh;
    int viewheight = LINES - (ui_flags.draw_outer_frame_lines ? 2 : 0);

    if (!ui_flags.sidebarwidth)
        return;

    int sbwidth = getmaxx(sidebar);

    /* re-create the subwindows every time; they only exist for use by
       draw_objlist */
    if (objwin)
        delwin(objwin);
    if (invwin)
        delwin(invwin);
    objwin = invwin = NULL;

    werase(sidebar);
    /* Inventory and floor item list sizing: Each list "owns" half the sidebar,
       but will expand into unused space in the other half. If unused space
       remains, the inventory list is expanded to push the floor item list to
       the bottom of the screen. */
    if (flooritems.icount && inventory.icount) {
        invheight = min(inventory.icount + 1, viewheight / 2);
        flheight = min(flooritems.icount + 1, (viewheight - 1) / 2);
        /* if there is need and available space, expand the floor item list up
           */
        if (flooritems.icount + 1 > flheight &&
            invheight < (viewheight + 1) / 2)
            flheight =
                min(flooritems.icount + 1, viewheight - invheight - 1);
        /* assign all unused space to the inventory list whether it is needed
           or not */
        invheight = viewheight - flheight - 1;
        nh_mvwhline(sidebar, invheight, 0, sbwidth);
    } else if (flooritems.icount)
        flheight = viewheight;
    else
        invheight = viewheight;

    if (invheight) {
        wattron(sidebar, A_UNDERLINE);
        mvwprintw(sidebar, 0, 0, "Inventory: %d/%dwt (%d/52 slots)",
                  player.wt, player.wtcap, player.invslots);
        wattroff(sidebar, A_UNDERLINE);

        if (!inventory.icount) {
            mvwprintw(sidebar, 1, 0, "Not carrying anything");
            wnoutrefresh(sidebar);
            return;
        }

        invwh = invheight - 1;
        if (invwh < inventory.icount) {
            invwh--;
            mvwprintw(sidebar, invheight - 1, 0, "(%d more omitted)",
                      count_omitted_items(&inventory, invwh));
        }
        invwin = derwin(sidebar, invwh, sbwidth, 1, 0);
        draw_objlist(invwin, &inventory, NULL, PICK_NONE);
    }

    if (flheight) {
        wattron(sidebar, A_UNDERLINE);
        mvwaddstr(sidebar, invheight ? invheight + 1 : 0, 0,
                  "Things that are here:");
        wattroff(sidebar, A_UNDERLINE);

        flwh = flheight - 1;
        if (flwh < flooritems.icount) {
            flwh--;
            mvwprintw(sidebar, viewheight - 1, 0, "(%d more omitted)",
                      count_omitted_items(&flooritems, flwh));
        }

        objwin =
            derwin(sidebar, flwh, sbwidth, invheight ? invheight + 2 : 1, 0);
        draw_objlist(objwin, &flooritems, NULL, PICK_NONE);
    }

    //draw_frame();
    wnoutrefresh(sidebar);
    //wnoutrefresh(basewin);
}

void
item_actions_from_sidebar(char accel)
{
    int i;
    for (i = 0; i < inventory.icount; i++) {
        if (inventory.items[i].accel == accel) {
            do_item_actions(inventory.items + i);
            break;
        }
    }
}

void
cleanup_sidebar(nh_bool dealloc)
{
    if (dealloc) {
        dealloc_objmenulist(&flooritems);
        dealloc_objmenulist(&inventory);
    }
    delwin(objwin);
    delwin(invwin);
    objwin = invwin = NULL;
}
