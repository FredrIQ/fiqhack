/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>

#define sgn(x) ((x) >= 0 ? 1 : -1)

struct coord {
    int x, y;
};

static struct nh_dbuf_entry (*display_buffer)[COLNO] = NULL;
static const int xdir[8] = { -1,-1, 0, 1, 1, 1, 0,-1 };
static const int ydir[8] = {  0,-1,-1,-1, 0, 1, 1, 1 };


void curses_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO])
{
    display_buffer = dbuf;
    draw_map(0);
}


void draw_map(int frame)
{
    int x, y, symcount, attr;
    struct curses_symdef syms[4];
    
    if (!display_buffer)
	return;
       
    for (y = 0; y < ROWNO; y++) {
	for (x = 1; x < COLNO; x++) {
	    /* set the position for each character to prevent incorrect
	     * positioning due to charset issues (IBM chars on a unicode term
	     * or vice versa) */
	    wmove(mapwin, y, x-1);
	    
	    symcount = mapglyph(&display_buffer[y][x], syms);
	    attr = A_NORMAL;
	    if (((display_buffer[y][x].monflags & MON_TAME) && settings.hilite_pet) ||
		((display_buffer[y][x].monflags & MON_DETECTED) && settings.use_inverse))
		attr |= A_REVERSE;

	    print_sym(mapwin, &syms[frame % symcount], attr);
	}
    }

    wrefresh(mapwin);
}


static int compare_coord_dist(const void *p1, const void *p2)
{
    const struct coord *c1 = p1;
    const struct coord *c2 = p2;
    int dx, dy, dist1, dist2;
    
    dx = player.x - c1->x; dy = player.y - c1->y;
    dist1 = dx * dx + dy * dy;
    dx = player.x - c2->x; dy = player.y - c2->y;
    dist2 = dx * dx + dy * dy;
    
    return dist1 - dist2;
}


int curses_getpos(int *x, int *y, nh_bool force, const char *goal)
{
    int result = 0;
    int cx, cy;
    int key, dx, dy;
    int sidx;
    static const char pick_chars[] = ".,;:";
    const char *cp;
    char printbuf[BUFSZ];
    char *matching = NULL;
    enum nh_direction dir;
    struct coord *monpos = NULL;
    int moncount, monidx;
    
    werase(statuswin);
    mvwaddstr(statuswin, 0, 0, "Move the cursor with the direction keys. Press "
                               "the letter of a dungeon symbol");
    mvwaddstr(statuswin, 1, 0, "to select it or use m to move to a nearby "
                               "monster. Finish with one of .,;:");
    wrefresh(statuswin);

    curs_set(1);
    
    cx = *x >= 1 ? *x : 1;
    cy = *y >= 0 ? *y : 0;
    wmove(mapwin, cy, cx-1);
    
    while (1) {
	dx = dy = 0;
	key = nh_wgetch(mapwin);
	if (key == KEY_ESC) {
	    cx = cy = -10;
	    result = -1;
	    break;
	}
	
	if ((cp = strchr(pick_chars, (char)key)) != 0) {
	    /* '.' => 0, ',' => 1, ';' => 2, ':' => 3 */
	    result = cp - pick_chars;
	    break;
	}

	dir = key_to_dir(key);
	if (dir != DIR_NONE) {
	    dx = xdir[dir];
	    dy = ydir[dir];
	} else if ( (dir = key_to_dir(tolower((char)key))) != DIR_NONE ) {
	    /* a shifted movement letter */
	    dx = xdir[dir] * 8;
	    dy = ydir[dir] * 8;
	}
	
	if (dx || dy) {
	    /* truncate at map edge */
	    if (cx + dx < 1 || cx + dx > COLNO-1)
		dx = 0;
	    if (cy + dy < 0 || cy + dy > ROWNO-1)
		dy = 0;
	    cx += dx;
	    cy += dy;
	    goto nxtc;
	}

	if (key == 'm') {
	    if (!monpos) {
		int i, j;
		moncount = 0;
		for (i = 0; i < ROWNO; i++)
		    for (j = 0; j < COLNO; j++)
			if (display_buffer[i][j].mon && (j != player.x || i != player.y))
			    moncount++;
		monpos = malloc(moncount * sizeof(struct coord));
		monidx = 0;
		for (i = 0; i < ROWNO; i++)
		    for (j = 0; j < COLNO; j++)
			if (display_buffer[i][j].mon && (j != player.x || i != player.y)) {
			    monpos[monidx].x = j;
			    monpos[monidx].y = i;
			    monidx++;
			}
		monidx = 0;
		qsort(monpos, moncount, sizeof(struct coord), compare_coord_dist);
	    }
	    
	    if (moncount) { /* there is at least one monster to move to */
		cx = monpos[monidx].x;
		cy = monpos[monidx].y;
		monidx = (monidx + 1) % moncount;
	    }
	    goto nxtc;
	} else if (!strchr(quitchars, key)) {
	    int k = 0, tx, ty;
	    int pass, lo_x, lo_y, hi_x, hi_y;
	    matching = malloc(default_drawing->num_bgelements);
	    memset(matching, 0, default_drawing->num_bgelements);
	    for (sidx = 1; sidx < default_drawing->num_bgelements; sidx++)
		if (key == default_drawing->bgelements[sidx].ch)
		    matching[sidx] = (char) ++k;
	    if (k) {
		for (pass = 0; pass <= 1; pass++) {
		    /* pass 0: just past current pos to lower right;
			pass 1: upper left corner to current pos */
		    lo_y = (pass == 0) ? cy : 0;
		    hi_y = (pass == 0) ? ROWNO - 1 : cy;
		    for (ty = lo_y; ty <= hi_y; ty++) {
			lo_x = (pass == 0 && ty == lo_y) ? cx + 1 : 1;
			hi_x = (pass == 1 && ty == hi_y) ? cx : COLNO - 1;
			for (tx = lo_x; tx <= hi_x; tx++) {
			    k = display_buffer[ty][tx].bg;
			    if (k && matching[k]) {
				cx = tx;
				cy = ty;
				goto nxtc;
			    }
			}	/* column */
		    }	/* row */
		}		/* pass */
		sprintf(printbuf, "Can't find dungeon feature '%c'.", (char)key);
		curses_msgwin(printbuf);
		goto nxtc;
	    } else {
		sprintf(printbuf, "Unknown direction%s.", 
			!force ? " (aborted)" : "");
		curses_msgwin(printbuf);
	    }
	} /* !quitchars */
	if (force) goto nxtc;
	cx = -1;
	cy = 0;
	result = 0;	/* not -1 */
	break;
	    
nxtc:
	wmove(mapwin, cy, cx-1);
	wrefresh(mapwin);
    }
    
    curs_set(0);
    *x = cx;
    *y = cy;
    if (monpos)
	free(monpos);
    if (matching)
	free(matching);
    curses_update_status(NULL); /* clear the help message */
    return result;
}
