/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nhcurses.h"

struct extcmd_hook_args {
    const char **namelist;
    const char **desclist;
    int listlen;
};

static boolean ext_cmd_getlin_hook(char *, void *);


static void buf_insert(char *buf, int pos, char key)
{
    int len = strlen(buf);
    
    while (len >= pos) {
	buf[len+1] = buf[len];
	len--;
    }
    buf[pos] = key;
}


static void buf_delete(char *buf, int pos)
{
    int len = strlen(buf);
    
    while (len >= pos) {
	buf[pos] = buf[pos+1];
	pos++;
    }
}


void draw_getline(struct gamewin *gw)
{
    struct win_getline *glw = (struct win_getline *)gw->extra;
    int width, height, i, offset = 0;
    int len = strlen(glw->buf);
    
    wclear(gw->win);
    wattron(gw->win, FRAME_ATTRS);
    box(gw->win, 0 , 0);
    wattroff(gw->win, FRAME_ATTRS);
    
    getmaxyx(gw->win, height, width);
    mvwaddnstr(gw->win, 1, 2, glw->query, width - 4);
    if (glw->pos > width - 4)
	offset = glw->pos - (width - 4);
    
    wmove(gw->win, 2, 2);
    wattron(gw->win, A_UNDERLINE);
    waddnstr(gw->win, &glw->buf[offset], width - 4);
    for (i = len; i < width - 4; i++)
	wprintw(gw->win, "%c", ' ');
    wattroff(gw->win, A_UNDERLINE);
    wmove(gw->win, 2, glw->pos - offset + 2);
    wrefresh(gw->win);
}


static void resize_getline(struct gamewin *gw)
{
    int height, width;
    
    getmaxyx(gw->win, height, width);
    if (height > LINES) height = LINES;
    if (width > COLS) width = COLS;
    
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    
    mvwin(gw->win, starty, startx);
    wresize(gw->win, height, width);
}


/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
void hooked_curses_getlin(const char *query, char *buf,
		       getlin_hook_proc hook, void *hook_proc_arg)
{
    struct gamewin *gw;
    struct win_getline *gldat;
    int height, width, key, len = 0;
    boolean done = FALSE;
    boolean autocomplete = FALSE;
    
    height = 4;
    width = COLS - 2;
    gw = alloc_gamewin(sizeof(struct win_getline));
    gw->win = newdialog(height, width);
    gw->draw = draw_getline;
    gw->resize = resize_getline;
    gldat = (struct win_getline *)gw->extra;
    gldat->buf = buf;
    gldat->query = query;
    
    curs_set(1);
    
    buf[0] = 0;
    while (!done) {
	draw_getline(gw);
	key = nh_wgetch(gw->win);
	
	switch (key) {
	    case KEY_ESC:
		buf[0] = (char)key;
		buf[1] = 0;
		done = TRUE;
		break;
		
	    case '\n':
		done = TRUE;
		break;
		
	    case KEY_BACKDEL:
		if (gldat->pos == 0) continue;
		gldat->pos--;
		/* fall through */
	    case KEY_DC:
		if (len == 0) continue;
		len--;
		buf_delete(buf, gldat->pos);
		break;
		
	    case KEY_LEFT:
		if (gldat->pos > 0) gldat->pos--;
		break;
		
	    case KEY_RIGHT:
		if (gldat->pos < len) gldat->pos++;
		break;
		
	    case KEY_HOME:
		gldat->pos = 0;
		break;
		
	    case KEY_END:
		gldat->pos = len;
		break;
		
	    default:
		if (' ' > (unsigned) key || (unsigned)key >= 128 || 
		    key == KEY_BACKDEL || gldat->pos >= BUFSZ-2)
		    continue;
		buf_insert(buf, gldat->pos, key);
		gldat->pos++;
		len++;
		
		if (hook) {
		    if (autocomplete)
			/* discard previous completion before looking for a new one */
			buf[gldat->pos] = '\0';
		    
		    autocomplete = (*hook)(buf, hook_proc_arg);
		    len = strlen(buf); /* (*hook) may modify buf */
		} else
		    autocomplete = FALSE;
		
		break;
	}
    }
    
    curs_set(0);
    delwin(gw->win);
    delete_gamewin(gw);
    redraw_game_windows();
}


void curses_getline(const char *query, char *buffer)
{
    hooked_curses_getlin(query, buffer, NULL, NULL);
}


/*
 * Implement extended command completion by using this hook into
 * tty_getlin.  Check the characters already typed, if they uniquely
 * identify an extended command, expand the string to the whole
 * command.
 *
 * Return TRUE if we've extended the string at base.  Otherwise return FALSE.
 * Assumptions:
 *
 *	+ we don't change the characters that are already in base
 *	+ base has enough room to hold our string
 */
static boolean ext_cmd_getlin_hook(char *base, void *hook_arg)
{
    int oindex, com_index;
    struct extcmd_hook_args *hpa = hook_arg;

    com_index = -1;
    for (oindex = 0; oindex < hpa->listlen; oindex++) {
	if (!strncmp(base, hpa->namelist[oindex], strlen(base))) {
	    if (com_index == -1)	/* no matches yet */
		com_index = oindex;
	    else			/* more than 1 match */
		return FALSE;
	}
    }
    if (com_index >= 0) {
	strcpy(base, hpa->namelist[com_index]);
	return TRUE;
    }

    return FALSE;	/* didn't match anything */
}

/* remove excess whitespace from a string buffer (in place) */
char *mungspaces(char *bp)
{
    char c, *p, *p2;
    boolean was_space = TRUE;

    for (p = p2 = bp; (c = *p) != '\0'; p++) {
	if (c == '\t') c = ' ';
	if (c != ' ' || !was_space) *p2++ = c;
	was_space = (c == ' ');
    }
    if (was_space && p2 > bp) p2--;
    *p2 = '\0';
    return bp;
}


static int extcmd_via_menu(const char **namelist, const char **desclist, int listlen)
{
    struct nh_menuitem *items;
    int icount, size, *pick_list;
    int choices[listlen+1];
    char buf[BUFSZ];
    char cbuf[QBUFSZ], prompt[QBUFSZ], fmtstr[20];
    int i, n, nchoices, acount;
    int ret,  biggest;
    int accelerator, prevaccelerator;
    int matchlevel = 0;
    
    size = 10;
    items = malloc(sizeof(struct nh_menuitem) * size);
    
    ret = 0;
    cbuf[0] = '\0';
    biggest = 0;
    while (!ret) {
	nchoices = n = 0;
	/* populate choices */
	for (i = 0; i < listlen; i++) {
	    if (!matchlevel || !strncmp(namelist[i], cbuf, matchlevel)) {
		choices[nchoices++] = i;
		if (strlen(desclist[i]) > biggest) {
		    biggest = strlen(desclist[i]);
		    sprintf(fmtstr,"%%-%ds", biggest + 15);
		}
	    }
	}
	choices[nchoices] = -1;
	
	/* if we're down to one, we have our selection so get out of here */
	if (nchoices == 1) {
	    for (i = 0; i < listlen; i++)
		if (!strncmp(namelist[i], cbuf, matchlevel)) {
		    ret = i;
		    break;
		}
	    break;
	}

	/* otherwise... */
	icount = 0;
	prevaccelerator = 0;
	acount = 0;
	for (i = 0; i < listlen && choices[i] >= 0; ++i) {
	    accelerator = namelist[choices[i]][matchlevel];
	    if (accelerator != prevaccelerator || nchoices < (ROWNO - 3)) {
		if (acount) {
		    /* flush the extended commands for that letter already in buf */
		    sprintf(buf, fmtstr, prompt);
		    add_menu_item(items, size, icount, prevaccelerator, buf,
				  prevaccelerator, FALSE);
		    acount = 0;
		}
	    }
	    prevaccelerator = accelerator;
	    if (!acount || nchoices < (ROWNO - 3)) {
		sprintf(prompt, "%s [%s]", namelist[choices[i]],
			    desclist[choices[i]]);
	    } else if (acount == 1) {
		sprintf(prompt, "%s or %s", namelist[choices[i-1]],
			    namelist[choices[i]]);
	    } else {
		strcat(prompt," or ");
		strcat(prompt, namelist[choices[i]]);
	    }
	    ++acount;
	}
	if (acount) {
	    /* flush buf */
	    sprintf(buf, fmtstr, prompt);
	    add_menu_item(items, size, icount, prevaccelerator, buf,
			  prevaccelerator, FALSE);
	}
	pick_list = malloc(sizeof(int) * icount);
	sprintf(prompt, "Extended Command: %s", cbuf);
	n = curses_display_menu(items, icount, prompt, PICK_ONE, pick_list);
	
	if (n==1) {
	    if (matchlevel > (QBUFSZ - 2)) {
		ret = -1;
	    } else {
		cbuf[matchlevel++] = (char)pick_list[0];
		cbuf[matchlevel] = '\0';
	    }
	} else {
	    if (matchlevel) {
		ret = 0;
		matchlevel = 0;
	    } else
		ret = -1;
	}
	free(pick_list);
    }
    
    free(items);
    return ret;
}


/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
int curses_get_ext_cmd(const char **namelist, const char **desclist, int listlen)
{
	int i;
	char buf[BUFSZ];
	struct extcmd_hook_args hpa = {namelist, desclist, listlen};

	if (settings.extmenu)
	    return extcmd_via_menu(namelist, desclist, listlen);

	/* maybe a runtime option? */
	hooked_curses_getlin("extended command: (? for help)", buf,
			     ext_cmd_getlin_hook, &hpa);
	mungspaces(buf);
	if (buf[0] == 0 || buf[0] == '\033')
	    return -1;
	
	for (i = 0; i < listlen; i++)
	    if (!strcmp(buf, namelist[i])) break;


	if (i == listlen) {
	    char msg[BUFSZ];
	    sprintf(msg, "%s: unknown extended command.", buf);
	    curses_msgwin(msg);
	    i = -1;
	}

	return i;
}

/*getline.c*/
