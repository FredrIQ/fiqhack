/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

/* save more of long messages than could be displayed in COLNO chars */
#define MSGLEN 119

struct message {
    char msg[MSGLEN + 1];
    int turn;
};

#define MAX_MSGLINES 40

static struct message *msghistory;
static int histsize, histpos;
static char msglines[MAX_MSGLINES][COLNO+1];
static int curline;
static int start_of_turn_curline;
static int last_redraw_curline;
static nh_bool stopprint;
static int prevturn, action, prevaction;

static void newline(void);


static void store_message(int turn, const char *msg)
{
    if (!*msg)
	return;
    
    histpos++;
    if (histpos >= histsize)
	histpos = 0;
    
    msghistory[histpos].turn = turn;
    strncpy(msghistory[histpos].msg, msg, MSGLEN);
    msghistory[histpos].msg[MSGLEN] = '\0';
}


void alloc_hist_array(void)
{
    struct message *oldhistory = msghistory;
    int i, oldsize = histsize, oldpos = histpos;
    
    msghistory = calloc(settings.msghistory, sizeof(struct message));
    histsize = settings.msghistory;
    histpos = -1; /* histpos is incremented before any message is stored */
    
    if (!oldhistory)
	return;
    
    for (i = 0; i < oldsize; i++) {
	int pos = oldpos + i + 1;
	if (pos >= oldsize) /* wrap around eventually */
	    pos -= oldsize;
	
	if (oldhistory[pos].turn)
	    store_message(oldhistory[pos].turn, oldhistory[pos].msg);
    }
    
    free(oldhistory);
}


static void newline(void)
{
    if (msglines[curline][0]) { /* do nothing if the line is empty */
	curline++;
	if (curline >= MAX_MSGLINES)
	    curline = 0;
	
	msglines[curline][0] = '\0';
    }
}


static void prune_messages(int maxturn)
{
    int i, pos;
    const char *msg;
    
    if (!msghistory)
	return;
    
    /* remove future messages from the history */
    while (msghistory[histpos].turn >= maxturn) {
	msghistory[histpos].turn = 0;
	msghistory[histpos].msg[0] = '\0';
	histpos--;
	if (histpos < 0)
	    histpos += histsize;
    }
    
    /* rebuild msglines */
    curline = 0;
    for (i = 0; i < MAX_MSGLINES; i++)
	msglines[i][0] = '\0';
    for (i = 0; i < histsize; i++) {
	pos = histpos + i + 1;
	if (pos >= histsize)
	    pos -= histsize;
	
	msg = msghistory[pos].msg;
	if (!*msg)
	    continue;
        if (msghistory[pos].turn > prevturn)
            start_of_turn_curline = last_redraw_curline = curline;
	prevturn = msghistory[pos].turn;
	
	if (strlen(msglines[curline]) + strlen(msg) + 2 < COLNO) {
	    if (msglines[curline][0])
		strcat(msglines[curline], "  ");
	    strcat(msglines[curline], msg);
	} else {
	    if (strlen(msglines[curline]) > 0)
		newline();
	    strcpy(msglines[curline], msg);
	}
    }
}


void draw_msgwin(void)
{
    int i, pos;
    
    werase(msgwin);
    
    for (i = getmaxy(msgwin) - 1; i >= 0; i--) {
	pos = curline - getmaxy(msgwin) + 1 + i;
	if (pos < 0)
	    pos += MAX_MSGLINES;
        if (pos == start_of_turn_curline)
            wattron(msgwin, curses_color_attr(COLOR_BLACK, 0));

	wmove(msgwin, i, 0);
	waddstr(msgwin, msglines[pos]);
        /* Only clear the remainder of the line if the cursor did not wrap. */
	if(getcurx(msgwin)) wclrtoeol(msgwin);
    }
    wattroff(msgwin, curses_color_attr(COLOR_BLACK, 0));
    wnoutrefresh(msgwin);
}


static void more(void)
{
    int key, attr = A_NORMAL;
    int cursx, cursy;
    
    if (settings.standout)
	attr = A_STANDOUT;
    
    if (getmaxy(msgwin) == 1) {
	wmove(msgwin, getmaxy(msgwin)-1, COLNO-8);
	wattron(msgwin, attr);
	waddstr(msgwin, "--More--");
	wattroff(msgwin, attr);
	wrefresh(msgwin);
    } else {
	newline();
	draw_msgwin();
	wmove(msgwin, getmaxy(msgwin)-1, COLNO/2 - 4);
	wattron(msgwin, attr);
	waddstr(msgwin, "--More--");
	wattroff(msgwin, attr);
	wrefresh(msgwin);
    }
    
    getyx(msgwin, cursy, cursx);
    wtimeout(msgwin, 666); /* enable blinking */
    do {
	key = nh_wgetch(msgwin);
	draw_map(player.x, player.y);
	wmove(msgwin, cursy, cursx);
	doupdate();
    } while (key != '\n' && key != '\r' && key != ' ' && key != KEY_ESC);
    wtimeout(msgwin, -1);
    
    if (getmaxy(msgwin) == 1)
	newline();
    draw_msgwin();
    
    if (key == KEY_ESC)
	stopprint = TRUE;

    /* we want to --more-- by screenfuls, not lines */
    last_redraw_curline = curline;
}


/* called from get_command */
void new_action(void)
{
    action++;
    start_of_turn_curline = last_redraw_curline = curline;
    draw_msgwin();
}


static void curses_print_message_core(int turn, const char *inmsg, nh_bool canblock)
{
    char msg[COLNO+1];
    int maxlen;
    nh_bool died;
    
    /* guard against malformed input strings */
    strncpy(msg, inmsg, COLNO);
    msg[COLNO] = '\0';
    
    if (!msghistory)
	alloc_hist_array();
    
    if (turn != prevturn)
        start_of_turn_curline = last_redraw_curline = curline;

    if (turn < prevturn) /* going back in time can happen during replay */
	prune_messages(turn);

    if (!*msg)
	return; /* empty message. done. */
    
    if (turn > prevturn || action > prevaction) {
	/* re-enable output if it was stopped and start a new line */
	stopprint = FALSE;
	newline();
    }
    prevturn = turn;
    prevaction = action;

    store_message(turn, inmsg);
    
    if (stopprint)
	return;
    
    /* 
     * generally we want to put as many messages on one line as possible to
     * maximize space usage. A new line is begun after each player turn or if
     * more() is called via pause_messages(). "You die" also deserves its own line.
     * 
     * If the message area is only one line high, space for "--More--" must be
     * reserved at the end of the line, otherwise  --More-- is shown on a new line.
     */
    maxlen = COLNO - 1;
    if (getmaxy(msgwin) == 1)
	maxlen -= 8; /* for "--More--" */
    
    died = !strncmp(msg, "You die", 7);
    if (strlen(msglines[curline]) + strlen(msg) + 1 < maxlen && !died) {
	if (msglines[curline][0])
	    strcat(msglines[curline], "  ");
	strcat(msglines[curline], msg);
    } else {
	if (strlen(msglines[curline]) > 0) {
            if (canblock) {
                /* If we would scroll a message off the screen that
                   the user hasn't had a chance to look at this
                   redraw, then run more(), else newline(). Because
                   the --more-- takes up a line by itself, we need to
                   offset that by one line. Thus, a message window of
                   height 2 requires us to always show a --more-- even
                   if we're on the first message of the redraw; with
                   height 3, we can safely newline after the first
                   line of messages but must --more-- after the
                   second, etc. getmaxy gives the height of the window
                   minus 1, which is why we only subtract 2 not 3. */
                if ((curline + MAX_MSGLINES - last_redraw_curline) %
                    MAX_MSGLINES > getmaxy(msgwin) - 2)
                  more();
                else
                  newline();
	    } else
		newline();
	}
	if (!stopprint) /* may get set in more() */
	    strcpy(msglines[curline], msg);
    }
    
    draw_msgwin();
}


void curses_print_message(int turn, const char *inmsg)
{
    curses_print_message_core(turn, inmsg, TRUE);
}


void curses_print_message_nonblocking(int turn, const char *inmsg)
{
    curses_print_message_core(turn, inmsg, FALSE);
}



void pause_messages(void)
{
    draw_msgwin();
    if (msglines[curline][0]) /* new text printed this turn */
	more();
}


void doprev_message(void)
{
    struct nh_menuitem *items;
    char buf[MSGLEN+1];
    int icount, size, i;
    
    icount = 0;
    size = 10;
    items = malloc(size * sizeof(struct nh_menuitem));
    
    for (i = 0; i < histsize; i++) {
	int pos = histpos + i + 1;
	if (pos >= histsize) /* wrap around eventually */
	    pos -= histsize;
	
	if (!msghistory[pos].turn)
	    continue;
	
	snprintf(buf, MSGLEN+1, "T:%d\t%s", msghistory[pos].turn, msghistory[pos].msg);
	add_menu_txt(items, size, icount, buf, MI_TEXT);
    }
    
    curses_display_menu(items, icount, "Message history:", PICK_NONE, NULL);
    free(items);
}


void cleanup_messages(void)
{
    int i;
    free(msghistory);
    prevturn = 0;
    
    /* extra cleanup to prevent old messages from appearing in a new game */
    msghistory = NULL;
    curline = last_redraw_curline = start_of_turn_curline = 0;
    histsize = histpos = 0;
    for (i = 0; i < MAX_MSGLINES; i++)
	msglines[i][0] = '\0';
}

