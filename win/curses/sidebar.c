/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"


static WINDOW *invwin, *objwin;
static struct nh_objitem *flooritems, *inventory;
static int floor_icount, inv_icount;


static nh_bool curses_list_items_core(struct nh_objitem *items, int icount,
			              nh_bool invent, nh_bool draw)
{
    struct nh_objitem **list;
    if (invent) {
	inv_icount = icount;
	list = &inventory;
    } else {
	floor_icount = icount;
	list = &flooritems;
    }
    
    free(*list);
    if (!icount)
	*list = NULL;
    else {
	*list = malloc(icount * sizeof(struct nh_objitem));
	memcpy(*list, items, icount * sizeof(struct nh_objitem));
    }
    
    if (draw)
	draw_sidebar();
    
    return ui_flags.draw_sidebar;
}


nh_bool curses_list_items(struct nh_objitem *items, int icount, nh_bool invent)
{
    return curses_list_items_core(items, icount, invent, TRUE);
}

nh_bool curses_list_items_nonblocking(struct nh_objitem *items, int icount, nh_bool invent)
{
    return curses_list_items_core(items, icount, invent, FALSE);
}


void draw_sidebar(void)
{
    int flheight = 0, invheight = 0, invwh;
    int sbwidth = getmaxx(sidebar);
    
    if (!ui_flags.draw_sidebar)
	return;
    
    /* re-create the subwindows every time; they only exist for use by draw_objlist */
    if (objwin)
	delwin(objwin);
    if (invwin)
	delwin(invwin);
    objwin = invwin = NULL;
    
    werase(sidebar);
    if (flooritems && inventory) {
	flheight = min(floor_icount+1, ui_flags.viewheight / 2);
	invheight = ui_flags.viewheight - flheight - 1;
	mvwhline(sidebar, flheight, 0, ACS_HLINE, sbwidth);
    } else if (flooritems)
	flheight = ui_flags.viewheight;
    else
	invheight = ui_flags.viewheight;
    
    if (flheight) {
	wattron(sidebar, A_UNDERLINE);
	mvwaddstr(sidebar, 0, 0, "Things that are here:");
	wattroff(sidebar, A_UNDERLINE);
	objwin = derwin(sidebar, flheight-1, sbwidth-1, 1, 0);
	draw_objlist(objwin, floor_icount, flooritems, NULL, PICK_NONE);
    }
    
    if (invheight) {
	wattron(sidebar, A_UNDERLINE);
	mvwaddstr(sidebar, flheight ? flheight + 1 : 0, 0, "Inventory:");
	wattroff(sidebar, A_UNDERLINE);
	
	invwh = invheight-1;
	if (invwh < inv_icount) {
	    invwh--;
	    mvwprintw(sidebar, ui_flags.viewheight - 1, 0, "(%d more omitted)",
		      inv_icount- invwh);
	}
	invwin = derwin(sidebar, invwh, sbwidth, flheight ? flheight + 2 : 1, 0);
	draw_objlist(invwin, inv_icount, inventory, NULL, PICK_NONE);
    }
    
    wrefresh(sidebar);
}


void cleanup_sidebar(nh_bool dealloc)
{
    if (dealloc) {
	free(flooritems);
	free(inventory);
	flooritems = inventory = NULL;
    }
    delwin(objwin);
    delwin(invwin);
    objwin = invwin = NULL;
}

