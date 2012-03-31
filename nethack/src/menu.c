/* Copyright (c) Daniel Thaler, 2011 */
/* NitroHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"


static nh_bool do_item_actions(char invlet);

static int calc_colwidths(char *menustr, int *colwidth)
{
    char *start, *tab;
    int col = 0;
    
    start = menustr;
    while ( (tab = strchr(start, '\t')) != NULL && col < MAXCOLS) {
	colwidth[col] = max(colwidth[col], tab - start);
	start = tab+1;
	col++;
    }
    colwidth[col] = max(colwidth[col], strlen(start));
    return col;
}


static int calc_menuwidth(int *colwidth, int *colpos, int maxcol)
{
    int i;
    for (i = 1; i <= maxcol; i++)
	colpos[i] = colpos[i-1] + colwidth[i-1] + 1;
    return colpos[maxcol] + colwidth[maxcol];    
}


static void layout_menu(struct gamewin *gw)
{
    struct win_menu *mdat = (struct win_menu*)gw->extra;
    int i, col, w, x1, x2, y1, y2;
    int colwidth[MAXCOLS];
    int scrheight;
    int scrwidth;
    int singlewidth = 0;
    
    x1 = (mdat->x1 > 0) ? mdat->x1 : 0;
    y1 = (mdat->y1 > 0) ? mdat->y1 : 0;
    x2 = (mdat->x2 > 0 && mdat->x2 <= COLS) ? mdat->x2 : COLS;
    y2 = (mdat->y2 > 0 && mdat->y2 <= LINES) ? mdat->y2 : LINES;
    
    scrheight = y2 - y1;
    scrwidth = x2 - x1;
    
    /* calc height */
    mdat->frameheight = 2;
    if (mdat->title && mdat->title[0]) {
	mdat->frameheight += 2; /* window title + space */
    }
	
    mdat->height = mdat->frameheight + min(mdat->icount, 52);
    if (mdat->height > scrheight)
	mdat->height = scrheight;
    mdat->innerheight = mdat->height - mdat->frameheight;
    
    /* calc width */
    mdat->maxcol = 0;
    for (i = 0; i < MAXCOLS; i++)
	colwidth[i] = 0;
    
    for (i = 0; i < mdat->icount; i++) {
	/* headings without tabs are not fitted into columns, but headers with
	 * tabs are presumably column titles */
	if (!strchr(mdat->items[i].caption, '\t')) {
	    w = strlen(mdat->items[i].caption);
	    if (mdat->items[i].role == MI_NORMAL && mdat->items[i].id)
		w += 4;
	    singlewidth = max(singlewidth, w);
	} else {
	    col = calc_colwidths(mdat->items[i].caption, colwidth);
	    mdat->maxcol = max(mdat->maxcol, col);
	}
    }
    if (mdat->how != PICK_NONE)
	colwidth[0] += 4; /* "a - " */
    
    mdat->innerwidth = max(calc_menuwidth(colwidth, mdat->colpos, mdat->maxcol),
			   singlewidth);
    if (mdat->innerwidth > scrwidth - 4)/* make sure there is space for window borders */
	mdat->innerwidth = scrwidth - 4;
    mdat->width = mdat->innerwidth + 4; /* border + space */
    mdat->colpos[mdat->maxcol+1] = mdat->innerwidth+1;
    
    if (mdat->title && mdat->width < strlen(mdat->title) + 4) {
	mdat->innerwidth = strlen(mdat->title);
	mdat->width = mdat->innerwidth + 4;
    }
}


void draw_menu(struct gamewin *gw)
{
    struct win_menu *mdat = (struct win_menu*)gw->extra;
    struct nh_menuitem *item;
    char caption[BUFSZ];
    char *colstrs[MAXCOLS];
    int i, j, col, scrlheight, scrlpos, scrltop, attr;
    char *tab;
    
    wattron(gw->win, FRAME_ATTRS);
    box(gw->win, 0 , 0);
    wattroff(gw->win, FRAME_ATTRS);
    if (mdat->title) {
	wattron(gw->win, A_UNDERLINE);
	mvwaddnstr(gw->win, 1, 2, mdat->title, mdat->width - 4);
	wattroff(gw->win, A_UNDERLINE);
    }
    
    werase(mdat->content);
    /* draw menu items */
    item = &mdat->items[mdat->offset];
    for (i = 0; i < mdat->innerheight; i++, item++) {
	strncpy(caption, item->caption, BUFSZ - 1);
	caption[BUFSZ - 1] = '\0';
	
	col = 0;
	colstrs[col] = caption;
	while ( (tab = strchr(colstrs[col], '\t')) != NULL ) {
	    col++;
	    *tab = '\0';
	    colstrs[col] = tab+1;
	}
	
	if (item->role == MI_HEADING)
	    wattron(mdat->content, settings.menu_headings);
	
	wmove(mdat->content, i, 0);
	if (mdat->how != PICK_NONE && item->role == MI_NORMAL && item->accel)
	    wprintw(mdat->content, "%c %c ", item->accel,
		    mdat->selected[mdat->offset + i] ? '+' : '-');
	
	if (col)
	    for (j = 0; j <= col; j++) {
		wprintw(mdat->content, "%-*s", mdat->colpos[j+1]-mdat->colpos[j]-1, colstrs[j]);
		wmove(mdat->content, i, mdat->colpos[j+1]);
	    }
	else
	    waddstr(mdat->content, caption);
	
	if (item->role == MI_HEADING)
	    wattroff(mdat->content, settings.menu_headings);
    }
    
    if (mdat->icount <= mdat->innerheight)
	return;
    /* draw scroll bar */
    scrltop = mdat->title ? 3 : 1;
    scrlheight = mdat->innerheight * mdat->innerheight / mdat->icount;
    scrlpos = mdat->offset * (mdat->innerheight - scrlheight) / (mdat->icount - mdat->innerheight);
    for (i = 0; i < mdat->innerheight; i++) {
	attr = A_NORMAL;
	if (i >= scrlpos && i < scrlpos + scrlheight)
	    attr = A_REVERSE;
	wattron(gw->win, attr);
	mvwaddch(gw->win, i + scrltop, mdat->innerwidth + 2, ' ');
	wattroff(gw->win, attr);
    }
}


static void resize_menu(struct gamewin *gw)
{
    struct win_menu *mdat = (struct win_menu*)gw->extra;
    int startx, starty;
    
    layout_menu(gw);
    /* if the window got longer and the last line was already visible, draw_menu
     * would go past the end of mdat->items without this */
    if (mdat->offset > mdat->icount - mdat->innerheight)
	mdat->offset = mdat->icount - mdat->innerheight;

    delwin(mdat->content);
    wresize(gw->win, mdat->height, mdat->width);
    mdat->content = derwin(gw->win, mdat->innerheight, mdat->innerwidth,
			   mdat->frameheight-1, 2);
    
    starty = (LINES - mdat->height) / 2;
    startx = (COLS - mdat->width) / 2;
    
    mvwin(gw->win, starty, startx);
    
    draw_menu(gw);
}


static void assign_menu_accelerators(struct win_menu *mdat)
{
    int i;
    char accel = 'a';
    
    for (i = 0; i < mdat->icount; i++) {
	
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


static int find_accel(int accel, struct win_menu *mdat)
{
    int i, upper;
    
    /*
     * scan visible entries first: long list of (eg the options menu)
     * might re-use accelerators, so that each is only unique among the visible
     * menu items
     */
    upper = min(mdat->icount, mdat->offset + mdat->innerheight);
    for (i = mdat->offset; i < upper; i++)
	if (mdat->items[i].accel == accel)
	    return i;
    
    /* it's not a regular accelerator, maybe there is a group accel? */
    for (i = mdat->offset; i < upper; i++)
	if (mdat->items[i].group_accel == accel)
	    return i;
    /*
     * extra effort: if the list is too long for one page search for the accel
     * among those entries, too and scroll the changed item into view.
     */
    if (mdat->icount > mdat->innerheight)
	for (i = 0; i < mdat->icount; i++)
	    if (mdat->items[i].accel == accel) {
		if (i > mdat->offset)
		    mdat->offset = i - mdat->innerheight + 1;
		else
		    mdat->offset = i;
		return i;
	    }
    
    return -1;
}


int curses_display_menu_core(struct nh_menuitem *items, int icount,
			     const char *title, int how, int *results,
			     int x1, int y1, int x2, int y2,
			     nh_bool (*changefn)(struct win_menu*, int))
{
    struct gamewin *gw;
    struct win_menu *mdat;
    int i, key, idx, rv, startx, starty, prevcurs, prev_offset;
    nh_bool done, cancelled;
    char sbuf[BUFSZ];
    
    prevcurs = curs_set(0);
    
    gw = alloc_gamewin(sizeof(struct win_menu));
    gw->draw = draw_menu;
    gw->resize = resize_menu;
    mdat = (struct win_menu*)gw->extra;
    mdat->items = items;
    mdat->icount = icount;
    mdat->title = title;
    mdat->how = how;
    mdat->selected = calloc(icount, sizeof(nh_bool));
    mdat->x1 = x1; mdat->y1 = y1; mdat->x2 = x2; mdat->y2 = y2;
    
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 < 0) x2 = COLS;
    if (y2 < 0) y2 = LINES;
    
    
    assign_menu_accelerators(mdat);
    layout_menu(gw);
    
    starty = (y2 - y1 - mdat->height) / 2 + y1;
    startx = (x2 - x1 - mdat->width) / 2 + x1;
    
    gw->win = newwin(mdat->height, mdat->width, starty, startx);
    keypad(gw->win, TRUE);
    wattron(gw->win, FRAME_ATTRS);
    box(gw->win, 0 , 0);
    wattroff(gw->win, FRAME_ATTRS);
    mdat->content = derwin(gw->win, mdat->innerheight, mdat->innerwidth,
			   mdat->frameheight-1, 2);
    leaveok(gw->win, TRUE);
    leaveok(mdat->content, TRUE);
    done = FALSE;
    cancelled = FALSE;
    while (!done && !cancelled) {
	draw_menu(gw);
	
	key = nh_wgetch(gw->win);
	
	switch (key) {
	    /* one line up */
	    case KEY_UP:
		if (mdat->offset > 0)
		    mdat->offset--;
		break;
		
	    /* one line down */
	    case KEY_DOWN:
		if (mdat->offset < mdat->icount - mdat->innerheight)
		    mdat->offset++;
		break;
	    
	    /* page up */
	    case KEY_PPAGE:
	    case '<':
		mdat->offset -= mdat->innerheight;
		if (mdat->offset < 0)
		    mdat->offset = 0;
		break;
	    
	    /* page down */
	    case KEY_NPAGE:
	    case '>':
	    case ' ':
		prev_offset = mdat->offset;
		mdat->offset += mdat->innerheight;
		if (mdat->offset > mdat->icount - mdat->innerheight)
		    mdat->offset = mdat->icount - mdat->innerheight;
		if (key == ' ' && mdat->offset == prev_offset)
		    done = TRUE;
		break;
		
	    /* go to the top */
	    case KEY_HOME:
	    case '^':
		mdat->offset = 0;
		break;
		
	    /* go to the end */
	    case KEY_END:
	    case '|':
		mdat->offset = max(mdat->icount - mdat->innerheight, 0);
		break;
		
	    /* cancel */
	    case KEY_ESC:
		cancelled = TRUE;
		break;
		
	    /* confirm */
	    case KEY_ENTER:
	    case '\r':
		done = TRUE;
		break;
		
	    /* select all */
	    case '.':
		if (mdat->how == PICK_ANY)
		    for (i = 0; i < icount; i++)
			if (mdat->items[i].role == MI_NORMAL)
			    mdat->selected[i] = TRUE;
		break;
		
	    /* select none */
	    case '-':
		for (i = 0; i < icount; i++)
		    mdat->selected[i] = FALSE;
		break;
		
	    /* search for a menu item */
	    case ':':
		curses_getline("Search:", sbuf);
		for (i = 0; i < icount; i++)
		    if (strstr(mdat->items[i].caption, sbuf))
			break;
		if (i < icount) {
		    int end = max(mdat->icount - mdat->innerheight, 0);
		    mdat->offset = min(i, end);
		}
		break;
		
	    /* try to find an item for this key and, if one is found, select it */
	    default:
		idx = find_accel(key, mdat);
		
		if (idx != -1 && /* valid accelerator */
		    (!changefn || changefn(mdat, idx))) {
		    mdat->selected[idx] = !mdat->selected[idx];
		    
		    if (mdat->how == PICK_ONE)
			done = TRUE;
		}
		break;
	}
    }
    
    delwin(mdat->content);
    delwin(gw->win);
    
    if (cancelled)
	rv = -1;
    else {
	rv = 0;
	for (i = 0; i < icount; i++) {
	    if (mdat->selected[i]) {
		if (results)
		    results[rv] = items[i].id;
		rv++;
	    }
	}
    }
    
    free(mdat->selected);
    delete_gamewin(gw);    
    redraw_game_windows();
    curs_set(prevcurs);
	
    return rv;
}


int curses_display_menu(struct nh_menuitem *items, int icount,
			       const char *title, int how, int *results)
{
    int n;
    
    n = curses_display_menu_core(items, icount, title, how, results, 0, 0,
				 -1, -1, NULL);
    return n;
}

/******************************************************************************
 * object menu functions follow
 * they are _very_ similar to the functions for regular menus, but not identical.
 * I can't shake the feeling that it should be possible to share lots of code,
 * but I can't quite see how to get there...
 */

static void layout_objmenu(struct gamewin *gw)
{
    struct win_objmenu *mdat = (struct win_objmenu*)gw->extra;
    char weightstr[16];
    int i, maxwidth, itemwidth;
    int scrheight = LINES;
    int scrwidth = COLS;
    
    /* calc height */
    mdat->frameheight = 2;
    if (mdat->title && mdat->title[0]) {
	mdat->frameheight += 2; /* window title + space */
    }
	
    mdat->height = mdat->frameheight + min(mdat->icount, 52);
    if (mdat->height > scrheight)
	mdat->height = scrheight;
    mdat->innerheight = mdat->height - mdat->frameheight;
    
    /* calc width */
    maxwidth = 0;
    for (i = 0; i < mdat->icount; i++) {
	itemwidth = strlen(mdat->items[i].caption);
	
	/* add extra space for an object symbol */
	if (mdat->items[i].role == MI_NORMAL)
	    itemwidth += 2;
	
	/* if the weight is known, leave space to show it */
	if (settings.invweight && mdat->items[i].weight != -1) {
	    sprintf(weightstr, " {%d}", mdat->items[i].weight);
	    itemwidth += strlen(weightstr);
	}
	if ((mdat->items[i].role == MI_NORMAL && mdat->items[i].accel) ||
	    (mdat->items[i].role == MI_HEADING && mdat->items[i].group_accel))
	    itemwidth += 4; /* "a - " or " ')'" */
	maxwidth = max(maxwidth, itemwidth);
    }
    
    mdat->innerwidth = maxwidth;
    if (mdat->innerwidth > scrwidth - 4)/* make sure there is space for window borders */
	mdat->innerwidth = scrwidth - 4;
    mdat->width = mdat->innerwidth + 4; /* border + space */
    
    if (mdat->title && mdat->width < strlen(mdat->title) + 4) {
	mdat->innerwidth = strlen(mdat->title);
	mdat->width = mdat->innerwidth + 4;
    }
}


void draw_objlist(WINDOW *win, int icount, struct nh_objitem *items,
		  int *selected, int how)
{
    int i, maxitem, txtattr, width, pos;
    
    width = getmaxx(win);
    werase(win);
    
    /* draw menu items */
    maxitem = min(getmaxy(win), icount);
    for (i = 0; i < maxitem; i++) {
	pos = 4; /* assume that an accel will be added ("a - ") */
	wmove(win, i, 0);
	wattrset(win, 0);
	txtattr = A_NORMAL;
	
	if (items[i].role != MI_NORMAL) {
	    if (items[i].role == MI_HEADING)
		txtattr = settings.menu_headings;
	    wattron(win, txtattr);
	    waddstr(win, items[i].caption);
	    if (items[i].group_accel)
		wprintw(win, " '%c'", items[i].group_accel);
	    wattroff(win, txtattr);
	    continue;
	}	    
	
	if (how != PICK_NONE && selected && items[i].accel)
	    switch (selected[i]) {
		case -1: wprintw(win, "%c + ", items[i].accel); break;
		case  0: wprintw(win, "%c - ", items[i].accel); break;
		default: wprintw(win, "%c # ", items[i].accel); break;
	    }
	else if (items[i].accel)
	    wprintw(win, "%c - ", items[i].accel);
	else
	    pos = 0; /* no accel after all */
	
	if (items[i].otype) {
	    print_sym(win, &cur_drawing->objects[items[i].otype-1], A_NORMAL);
	    waddch(win, ' ');
	    pos += 2;
	}
	
	if (items[i].worn) txtattr |= A_BOLD;
	switch (items[i].buc) {
	    case B_CURSED: txtattr |= COLOR_PAIR(2); break;
	    case B_BLESSED: txtattr |= COLOR_PAIR(7); break;
	    default: break;
	}
	wattron(win, txtattr);
	wprintw(win, "%-.*s", width - pos, items[i].caption);
	pos += min(strlen(items[i].caption), width - pos);
	wattroff(win, txtattr);
	
	if (settings.invweight && items[i].weight != -1 && width > pos + 3)
	    wprintw(win, " {%d}", items[i].weight);
    }
    wnoutrefresh(win);
}


static void draw_objmenu(struct gamewin *gw)
{
    struct win_objmenu *mdat = (struct win_objmenu*)gw->extra;
    int i, scrlheight, scrlpos, scrltop, attr;
    
    wattron(gw->win, FRAME_ATTRS);
    box(gw->win, 0 , 0);
    wattroff(gw->win, FRAME_ATTRS);
    if (mdat->title) {
	wattron(gw->win, A_UNDERLINE);
	mvwaddnstr(gw->win, 1, 2, mdat->title, mdat->width - 4);
	wattroff(gw->win, A_UNDERLINE);
    }
    
    draw_objlist(mdat->content, mdat->icount - mdat->offset,
		 &mdat->items[mdat->offset], &mdat->selected[mdat->offset], mdat->how);
    
    if (mdat->selcount > 0) {
	wmove(gw->win, getmaxy(gw->win)-1, 1);
	wprintw(gw->win, "Count: %d", mdat->selcount);
    }
    
    if (mdat->icount <= mdat->innerheight)
	return;
    
    /* draw scroll bar */
    scrltop = mdat->title ? 3 : 1;
    scrlheight = mdat->innerheight * mdat->innerheight / mdat->icount;
    scrlpos = mdat->offset * (mdat->innerheight - scrlheight) / (mdat->icount - mdat->innerheight);
    for (i = 0; i < mdat->innerheight; i++) {
	attr = A_NORMAL;
	if (i >= scrlpos && i < scrlpos + scrlheight)
	    attr = A_REVERSE;
	wattron(gw->win, attr);
	mvwaddch(gw->win, i + scrltop, mdat->innerwidth + 2, ' ');
	wattroff(gw->win, attr);
    }
}


static void resize_objmenu(struct gamewin *gw)
{
    struct win_objmenu *mdat = (struct win_objmenu*)gw->extra;
    int startx, starty;
    
    layout_objmenu(gw);
    
    delwin(mdat->content);
    wresize(gw->win, mdat->height, mdat->width);
    mdat->content = derwin(gw->win, mdat->innerheight, mdat->innerwidth,
			   mdat->frameheight-1, 2);
     
    starty = (LINES - mdat->height) / 2;
    startx = (COLS - mdat->width) / 2;
    
    mvwin(gw->win, starty, startx);
    
    draw_objmenu(gw);
}


static void assign_objmenu_accelerators(struct win_objmenu *mdat)
{
    int i;
    char accel = 'a';
    
    for (i = 0; i < mdat->icount; i++) {
	
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


static int find_objaccel(int accel, struct win_objmenu *mdat)
{
    int i, upper;
    
    /*
     * scan visible entries first: long list of items (eg the options menu)
     * might re-use accelerators, so that each is only unique among the visible
     * menu items
     */
    upper = min(mdat->icount, mdat->offset + mdat->innerheight);
    for (i = mdat->offset; i < upper; i++)
	if (mdat->items[i].accel == accel)
	    return i;
    
    /*
     * extra effort: if the list is too long for one page search for the accel
     * among those entries, too
     */
    if (mdat->icount > mdat->innerheight)
	for (i = 0; i < mdat->icount; i++)
	    if (mdat->items[i].accel == accel)
		return i;
    
    return -1;
}


int curses_display_objects(struct nh_objitem *items, int icount,
		  const char *title, int how, struct nh_objresult *results)
{
    struct gamewin *gw;
    struct win_objmenu *mdat;
    int i, key, idx, rv, startx, starty, prevcurs, prev_offset;
    nh_bool done, cancelled;
    char sbuf[BUFSZ];
    nh_bool inventory_special = title && !!strstr(title, "Inventory") && how == PICK_NONE;
    
    prevcurs = curs_set(0);
    
    gw = alloc_gamewin(sizeof(struct win_objmenu));
    gw->draw = draw_objmenu;
    gw->resize = resize_objmenu;
    mdat = (struct win_objmenu*)gw->extra;
    mdat->items = items;
    mdat->icount = icount;
    mdat->title = title;
    mdat->how = how;
    mdat->selcount = -1;
    mdat->selected = calloc(icount, sizeof(int));
    
    if (how != PICK_NONE)
	assign_objmenu_accelerators(mdat);
    layout_objmenu(gw);
    
    starty = (LINES - mdat->height) / 2;
    startx = (COLS - mdat->width) / 2;
    
    gw->win = newwin(mdat->height, mdat->width, starty, startx);
    keypad(gw->win, TRUE);
    wattron(gw->win, FRAME_ATTRS);
    box(gw->win, 0 , 0);
    wattroff(gw->win, FRAME_ATTRS);
    mdat->content = derwin(gw->win, mdat->innerheight, mdat->innerwidth,
			   mdat->frameheight-1, 2);
    leaveok(gw->win, TRUE);
    leaveok(mdat->content, TRUE);
    
    done = FALSE;
    cancelled = FALSE;
    while (!done && !cancelled) {
	draw_objmenu(gw);
	
	key = nh_wgetch(gw->win);
	
	switch (key) {
	    /* one line up */
	    case KEY_UP:
		if (mdat->offset > 0)
		    mdat->offset--;
		break;
		
	    /* one line down */
	    case KEY_DOWN:
		if (mdat->offset < mdat->icount - mdat->innerheight)
		    mdat->offset++;
		break;
	    
	    /* page up */
	    case KEY_PPAGE:
	    case '<':
		mdat->offset -= mdat->innerheight;
		if (mdat->offset < 0)
		    mdat->offset = 0;
		break;
	    
	    /* page down */
	    case KEY_NPAGE:
	    case '>':
	    case ' ':
		prev_offset = mdat->offset;
		mdat->offset += mdat->innerheight;
		if (mdat->offset > mdat->icount - mdat->innerheight)
		    mdat->offset = mdat->icount - mdat->innerheight;
		if (key == ' ' && prev_offset == mdat->offset)
		    done = TRUE;
		break;
		
	    /* go to the top */
	    case KEY_HOME:
	    case '^':
		mdat->offset = 0;
		break;
		
	    /* go to the end */
	    case KEY_END:
	    case '|':
		mdat->offset = max(mdat->icount - mdat->innerheight, 0);
		break;
		
	    /* cancel */
	    case KEY_ESC:
		cancelled = TRUE;
		break;
		
	    /* confirm */
	    case KEY_ENTER:
	    case '\r':
		done = TRUE;
		break;
		
	    /* select all */
	    case '.':
		if (mdat->how == PICK_ANY)
		    for (i = 0; i < icount; i++)
			if (mdat->items[i].oclass != -1)
			    mdat->selected[i] = -1;
		break;
		
	    /* select none */
	    case '-':
		for (i = 0; i < icount; i++)
		    mdat->selected[i] = 0;
		break;
		
	    /* invert all */
	    case '@':
		if (mdat->how == PICK_ANY)
		    for (i = 0; i < icount; i++)
			if (mdat->items[i].oclass != -1)
			    mdat->selected[i] = mdat->selected[i] ? 0 : -1;
		break;
		
	    /* select page */
	    case ',':
		if (mdat->how != PICK_ANY)
		    break;
		
		for (i = mdat->offset;
		     i < icount && i < mdat->offset + mdat->innerheight; i++)
		    if (mdat->items[i].oclass != -1)
			mdat->selected[i] = -1;
		break;
		
	    /* deselect page */
	    case '\\':
		for (i = mdat->offset;
		     i < icount && i < mdat->offset + mdat->innerheight; i++)
		    if (mdat->items[i].oclass != -1)
			mdat->selected[i] = 0;
		break;
		
	    /* invert page */
	    case '~':
		if (mdat->how != PICK_ANY)
		    break;
		
		for (i = mdat->offset;
		     i < icount && i < mdat->offset + mdat->innerheight; i++)
		    if (mdat->items[i].oclass != -1)
			mdat->selected[i] = mdat->selected[i] ? 0 : -1;
		break;
		
	    /* search for a menu item */
	    case ':':
		curses_getline("Search:", sbuf);
		for (i = 0; i < icount; i++)
		    if (strstr(mdat->items[i].caption, sbuf))
			break;
		if (i < icount) {
		    int end = max(mdat->icount - mdat->innerheight, 0);
		    mdat->offset = min(i, end);
		}
		break;
		
	    /* edit selection count */
	    case KEY_BACKDEL:
		mdat->selcount /= 10;
		if (mdat->selcount == 0)
		    mdat->selcount = -1; /* -1: select all */
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
		
		/* try to find an item for this key and, if one is found, select it */
		idx = find_objaccel(key, mdat);
		
		if (idx != -1) { /* valid item accelerator */
		    if (mdat->selected[idx])
			mdat->selected[idx] = 0;
		    else
			mdat->selected[idx] = mdat->selcount;
		    mdat->selcount = -1;
		    
		    if (mdat->how == PICK_ONE)
			done = TRUE;
		    
		    /* inventory special case: show item actions menu */
		    else if (inventory_special)
			if (do_item_actions(mdat->items[idx].accel))
			    done = TRUE;
		    
		} else if (mdat->how == PICK_ANY) { /* maybe it's a group accel? */
		    int grouphits = 0;
		    for (i = 0; i < mdat->icount; i++) {
			if (items[i].group_accel == key && mdat->items[i].oclass != -1) {
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
    
    delwin(mdat->content);
    delwin(gw->win);
    
    if (cancelled)
	rv = -1;
    else {
	rv = 0;
	for (i = 0; i < icount; i++) {
	    if (mdat->selected[i]) {
		if (results) {
		    results[rv].id = items[i].id;
		    results[rv].count = mdat->selected[i];
		}
		rv++;
	    }
	}
    }
    
    free(mdat->selected);
    delete_gamewin(gw);
    redraw_game_windows();
    curs_set(prevcurs);
	
    return rv;
}


static nh_bool do_item_actions(char invlet)
{
    int ccount = 0, i, selected[1];
    struct nh_cmd_desc *obj_cmd = nh_get_object_commands(&ccount, invlet);
    struct nh_menuitem *items;
    struct nh_cmd_arg arg;
    
    if (!obj_cmd || !ccount)
	return FALSE;
    
    items = malloc(sizeof(struct nh_menuitem) * ccount);
    
    for (i = 0; i < ccount; i++)
	set_menuitem(&items[i], i+1, MI_NORMAL, obj_cmd[i].desc, 0, FALSE);
    
    i = curses_display_menu(items, ccount, "Item actions:", PICK_ONE, selected);
    free(items);
    
    if (i <= 0)
	return FALSE;
    
    arg.argtype = CMD_ARG_OBJ;
    arg.invlet = invlet;
    set_next_command(obj_cmd[selected[0]-1].name, &arg);
    
    return TRUE;
}

