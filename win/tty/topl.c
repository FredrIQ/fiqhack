/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nethack.h"

#include "tcap.h"
#include "wintty.h"
#include <ctype.h>

#ifndef C	/* this matches src/cmd.c */
#define C(c)	(0x1f & (c))
#endif

static void redotoplin(const char*);
static void topl_putsym(char);
static void remember_topl(void);


int tty_doprev_message(void)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    winid prevmsg_win;
    int i;
    if ((ui_flags.prevmsg_window != 's') && !ttyDisplay->inread) { /* not single */
        if (ui_flags.prevmsg_window == 'f') { /* full */
            prevmsg_win = tty_create_nhwindow(NHW_MENU);
            tty_putstr(prevmsg_win, 0, "Message History");
            tty_putstr(prevmsg_win, 0, "");
            cw->maxcol = cw->maxrow;
            i = cw->maxcol;
            do {
                if (cw->data[i] && strcmp(cw->data[i], "") )
                    tty_putstr(prevmsg_win, 0, cw->data[i]);
                i = (i + 1) % cw->rows;
            } while (i != cw->maxcol);
            tty_putstr(prevmsg_win, 0, toplines);
            display_nhwindow(prevmsg_win, TRUE);
            tty_destroy_nhwindow(prevmsg_win);
        } else if (ui_flags.prevmsg_window == 'c') {		/* combination */
            do {
                morc = 0;
                if (cw->maxcol == cw->maxrow) {
                    ttyDisplay->dismiss_more = C('p');	/* <ctrl/P> allowed at --More-- */
                    redotoplin(toplines);
                    cw->maxcol--;
                    if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
                    if (!cw->data[cw->maxcol])
                        cw->maxcol = cw->maxrow;
                } else if (cw->maxcol == (cw->maxrow - 1)){
                    ttyDisplay->dismiss_more = C('p');	/* <ctrl/P> allowed at --More-- */
                    redotoplin(cw->data[cw->maxcol]);
                    cw->maxcol--;
                    if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
                    if (!cw->data[cw->maxcol])
                        cw->maxcol = cw->maxrow;
                } else {
                    prevmsg_win = tty_create_nhwindow(NHW_MENU);
                    tty_putstr(prevmsg_win, 0, "Message History");
                    tty_putstr(prevmsg_win, 0, "");
                    cw->maxcol = cw->maxrow;
                    i = cw->maxcol;
                    do {
                        if (cw->data[i] && strcmp(cw->data[i], "") )
                            tty_putstr(prevmsg_win, 0, cw->data[i]);
                        i = (i + 1) % cw->rows;
                    } while (i != cw->maxcol);
                    tty_putstr(prevmsg_win, 0, toplines);
                    display_nhwindow(prevmsg_win, TRUE);
                    tty_destroy_nhwindow(prevmsg_win);
                }

            } while (morc == C('p'));
            ttyDisplay->dismiss_more = 0;
        } else { /* reversed */
            morc = 0;
            prevmsg_win = tty_create_nhwindow(NHW_MENU);
            tty_putstr(prevmsg_win, 0, "Message History");
            tty_putstr(prevmsg_win, 0, "");
            tty_putstr(prevmsg_win, 0, toplines);
            cw->maxcol=cw->maxrow-1;
            if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
            do {
                tty_putstr(prevmsg_win, 0, cw->data[cw->maxcol]);
                cw->maxcol--;
                if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
                if (!cw->data[cw->maxcol])
                    cw->maxcol = cw->maxrow;
            } while (cw->maxcol != cw->maxrow);

            display_nhwindow(prevmsg_win, TRUE);
            tty_destroy_nhwindow(prevmsg_win);
            cw->maxcol = cw->maxrow;
            ttyDisplay->dismiss_more = 0;
        }
    } else if (ui_flags.prevmsg_window == 's') { /* single */
        ttyDisplay->dismiss_more = C('p');  /* <ctrl/P> allowed at --More-- */
        do {
            morc = 0;
            if (cw->maxcol == cw->maxrow)
                redotoplin(toplines);
            else if (cw->data[cw->maxcol])
                redotoplin(cw->data[cw->maxcol]);
            cw->maxcol--;
            if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
            if (!cw->data[cw->maxcol])
                cw->maxcol = cw->maxrow;
        } while (morc == C('p'));
        ttyDisplay->dismiss_more = 0;
    }
    return 0;
}


static void redotoplin(const char *str)
{
	int otoplin = ttyDisplay->toplin;
	home();
	if (*str & 0x80) {
		/* kludge for the / command, the only time we ever want a */
		/* graphics character on the top line */
		g_putch((int)*str++);
		ttyDisplay->curx++;
	}
	end_glyphout();	/* in case message printed during graphics output */
	putsyms(str);
	cl_end();
	ttyDisplay->toplin = 1;
	if (ttyDisplay->cury && otoplin != 3)
		more();
}

static void remember_topl(void)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];
    int idx = cw->maxrow;
    unsigned len = strlen(toplines) + 1;

    if (len > (unsigned)cw->datlen[idx]) {
	if (cw->data[idx]) free(cw->data[idx]);
	len += (8 - (len & 7));		/* pad up to next multiple of 8 */
	cw->data[idx] = malloc(len);
	cw->datlen[idx] = (short)len;
    }
    strcpy(cw->data[idx], toplines);
    cw->maxcol = cw->maxrow = (idx + 1) % cw->rows;
}

void addtopl(const char *s)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    move_cursor(BASE_WINDOW,cw->curx+1,cw->cury);
    putsyms(s);
    cl_end();
    ttyDisplay->toplin = 1;
}


void more(void)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    /* avoid recursion -- only happens from interrupts */
    if (ttyDisplay->inmore++)
	return;

    if (ttyDisplay->toplin) {
	move_cursor(BASE_WINDOW, cw->curx+1, cw->cury);
	if (cw->curx >= CO - 8) topl_putsym('\n');
    }

    if (ui_flags.standout)
	standoutbeg();
    putsyms(defmorestr);
    if (ui_flags.standout)
	standoutend();

    xwaitforspace("\033 ");

    if (morc == '\033')
	cw->flags |= WIN_STOP;

    if (ttyDisplay->toplin && cw->cury) {
	docorner(1, cw->cury+1);
	cw->curx = cw->cury = 0;
	home();
    } else if (morc == '\033') {
	cw->curx = cw->cury = 0;
	home();
	cl_end();
    }
    ttyDisplay->toplin = 0;
    ttyDisplay->inmore = 0;
}


void update_topl(const char *bp)
{
	char *tl, *otl;
	int n0;
	int notdied = 1;
	struct WinDesc *cw = wins[WIN_MESSAGE];

	/* If there is room on the line, print message on same line */
	/* But messages like "You die..." deserve their own line */
	n0 = strlen(bp);
	if ((ttyDisplay->toplin == 1 || (cw->flags & WIN_STOP)) &&
	    cw->cury == 0 &&
	    n0 + (int)strlen(toplines) + 3 < CO-8 &&  /* room for --More-- */
	    (notdied = strncmp(bp, "You die", 7))) {
		strcat(toplines, "  ");
		strcat(toplines, bp);
		cw->curx += 2;
		if (!(cw->flags & WIN_STOP))
		    addtopl(bp);
		return;
	} else if (!(cw->flags & WIN_STOP)) {
	    if (ttyDisplay->toplin == 1) more();
	    else if (cw->cury) {	/* for when flags.toplin == 2 && cury > 1 */
		docorner(1, cw->cury+1); /* reset cury = 0 if redraw screen */
		cw->curx = cw->cury = 0;/* from home--cls() & docorner(1,n) */
	    }
	}
	remember_topl();
	strncpy(toplines, bp, TBUFSZ);
	toplines[TBUFSZ - 1] = 0;

	for (tl = toplines; n0 >= CO; ){
	    otl = tl;
	    for (tl+=CO-1; tl != otl && !isspace(*tl); --tl) ;
	    if (tl == otl) {
		/* Eek!  A huge token.  Try splitting after it. */
		tl = index(otl, ' ');
		if (!tl) break;    /* No choice but to spit it out whole. */
	    }
	    *tl++ = '\n';
	    n0 = strlen(tl);
	}
	if (!notdied) cw->flags &= ~WIN_STOP;
	if (!(cw->flags & WIN_STOP)) redotoplin(toplines);
}


static void topl_putsym(char c)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    switch(c) {
    case '\b':
	if (ttyDisplay->curx == 0 && ttyDisplay->cury > 0)
	    move_cursor(BASE_WINDOW, CO, (int)ttyDisplay->cury-1);
	backsp();
	ttyDisplay->curx--;
	cw->curx = ttyDisplay->curx;
	return;
    case '\n':
	cl_end();
	ttyDisplay->curx = 0;
	ttyDisplay->cury++;
	cw->cury = ttyDisplay->cury;
#ifdef WIN32CON
    putchar(c);
#endif
	break;
    default:
	if (ttyDisplay->curx == CO-1)
	    topl_putsym('\n'); /* 1 <= curx <= CO; avoid CO */
#ifdef WIN32CON
    putchar(c);
#endif
	ttyDisplay->curx++;
    }
    cw->curx = ttyDisplay->curx;
    if (cw->curx == 0) cl_end();
#ifndef WIN32CON
    putchar(c);
#endif
}

void putsyms(const char *str)
{
    while (*str)
	topl_putsym(*str++);
}

extern char erase_char;		/* from xxxtty.c; don't need kill_char */

char tty_yn_function(const char *query, const char *resp, char def)
/*
 *   Generic yes/no function. 'def' is the default (returned by space or
 *   return; 'esc' returns 'q', or 'n', or the default, depending on
 *   what's in the string. The 'query' string is printed before the user
 *   is asked about the string.
 *   Resp may not be NULL and only characters in it are allowed (exceptions: 
 *   the quitchars are always allowed); if it includes an <esc>, anything
 *   beyond that won't be shown in the prompt to the user but will be
 *   acceptable as input.
 */
{
	char q;
	char rtmp[40];
	struct WinDesc *cw = wins[WIN_MESSAGE];
	boolean doprev = 0;
	char prompt[QBUFSZ];
	char *rb, respbuf[QBUFSZ];
	
	if (!resp) {
	    tty_raw_print("invalid use of the tty_yn_function API.\n");
	    return 0;
	}

	if (ttyDisplay->toplin == 1 && !(cw->flags & WIN_STOP)) more();
	cw->flags &= ~WIN_STOP;
	ttyDisplay->toplin = 3; /* special prompt state */
	ttyDisplay->inread++;

	strcpy(respbuf, resp);
	/* any acceptable responses that follow <esc> aren't displayed */
	if ((rb = index(respbuf, '\033')) != 0)
	    *rb = '\0';
	sprintf(prompt, "%s [%s] ", query, respbuf);
	if (def)
	    sprintf(prompt + strlen(prompt), "(%c) ", def);
	tty_print_message(prompt);

	do {	/* loop until we get valid input */
	    q = tolower(tty_nhgetch());
	    if (q == '\020') { /* ctrl-P */
		if (ui_flags.prevmsg_window != 's') {
		    int sav = ttyDisplay->inread;
		    ttyDisplay->inread = 0;
		    tty_doprev_message();
		    ttyDisplay->inread = sav;
		    clear_nhwindow(WIN_MESSAGE);
		    cw->maxcol = cw->maxrow;
		    addtopl(prompt);
		} else {
		    if (!doprev)
			tty_doprev_message(); /* need two initially */
		    tty_doprev_message();
		    doprev = 1;
		}
		q = '\0';	/* force another loop iteration */
		continue;
	    } else if (doprev) {
		/* BUG[?]: this probably ought to check whether the
		   character which has just been read is an acceptable
		   response; if so, skip the reprompt and use it. */
		clear_nhwindow(WIN_MESSAGE);
		cw->maxcol = cw->maxrow;
		doprev = 0;
		addtopl(prompt);
		q = '\0';	/* force another loop iteration */
		continue;
	    }
	    if (q == '\033') {
		if (index(resp, 'q'))
		    q = 'q';
		else if (index(resp, 'n'))
		    q = 'n';
		else
		    q = def;
		break;
	    } else if (index(quitchars, q)) {
		q = def;
		break;
	    }
	    if (!index(resp, q)) {
		tty_nhbell();
		q = (char)0;
	    }
	} while (!q);

	if (q != '#') {
		sprintf(rtmp, "%c", q);
		addtopl(rtmp);
	}

	ttyDisplay->inread--;
	ttyDisplay->toplin = 2;
	if (ttyDisplay->intr) ttyDisplay->intr--;
	if (wins[WIN_MESSAGE]->cury)
	    clear_nhwindow(WIN_MESSAGE);

	return q;
}

char tty_query_key(const char *query, int *count)
{
	char key;
	int cnt = 0;
	boolean hascount = FALSE;
	struct WinDesc *cw = wins[WIN_MESSAGE];
	
	if (ttyDisplay->toplin == 1 && !(cw->flags & WIN_STOP)) more();
	cw->flags &= ~WIN_STOP;
	ttyDisplay->toplin = 3; /* special prompt state */
	ttyDisplay->inread++;
	
	tty_print_message(query);
	key = tty_nhgetch();
	while (isdigit(key) && count != NULL) {
	    cnt = 10*cnt + (key - '0');
	    key = tty_nhgetch();
	    hascount = TRUE;
	}
	
	if (count != NULL) {
	    if (!hascount && !cnt)
		cnt = -1; /* signal to caller that no count was given */
	    *count = cnt;
	}
	
	ttyDisplay->inread--;
	ttyDisplay->toplin = 2;
	if (ttyDisplay->intr) ttyDisplay->intr--;
	if (wins[WIN_MESSAGE]->cury)
	    clear_nhwindow(WIN_MESSAGE);
	
	return key;    
}

/*topl.c*/
