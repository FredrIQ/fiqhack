/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include <ctype.h>

#include "nhcurses.h"

/* save more of long messages than could be displayed in COLNO chars */
#define MSGLEN 119

struct message {
    char *msg;
    int turn;
};

#define MAX_MSGLINES 40

static struct message *msghistory;
static int histsize, histpos;
static char msglines[MAX_MSGLINES][COLNO + 1];
static int curline;
static int start_of_turn_curline;
static int last_redraw_curline;
static nh_bool stopprint, blockafter = TRUE;
static int prevturn, action, prevaction;

static void newline(void);

static void wrap_text(int width, const char *input, int *output_count,
                      char ***output);
static void free_wrap(char **wrap_output);

static void
store_message(int turn, const char *msg)
{
    if (!*msg)
        return;

    histpos++;
    if (histpos >= histsize)
        histpos = 0;

    msghistory[histpos].turn = turn;
    if (msghistory[histpos].msg)
        free(msghistory[histpos].msg);
    msghistory[histpos].msg = strdup(msg);
}


void
alloc_hist_array(void)
{
    struct message *oldhistory = msghistory;
    int i, oldsize = histsize, oldpos = histpos;

    msghistory = calloc(settings.msghistory, sizeof (struct message));
    histsize = settings.msghistory;
    histpos = -1;       /* histpos is incremented before any message is stored */

    for (i = 0; i < histsize; i++) {
        msghistory[i].msg = NULL;
        msghistory[i].turn = 0;
    }

    if (!oldhistory)
        return;

    for (i = 0; i < oldsize; i++) {
        int pos = oldpos + i + 1;

        if (pos >= oldsize)     /* wrap around eventually */
            pos -= oldsize;

        if (oldhistory[pos].turn)
            store_message(oldhistory[pos].turn, oldhistory[pos].msg);
        if (oldhistory[pos].msg)
            free(oldhistory[pos].msg);
    }

    free(oldhistory);
}


static void
newline(void)
{
    if (msglines[curline][0]) { /* do nothing if the line is empty */
        curline++;
        if (curline >= MAX_MSGLINES)
            curline = 0;

        msglines[curline][0] = '\0';
    }
}


static void
prune_messages(int maxturn)
{
    int i, pos;
    const char *msg;

    if (!msghistory)
        return;

    /* remove future messages from the history */
    while (msghistory[histpos].turn >= maxturn) {
        msghistory[histpos].turn = 0;
        if (msghistory[histpos].msg) {
            free(msghistory[histpos].msg);
            msghistory[histpos].msg = NULL;
        }
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
        if (!msg)
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


void
draw_msgwin(void)
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
        if (getcurx(msgwin))
            wclrtoeol(msgwin);
    }
    wattroff(msgwin, curses_color_attr(COLOR_BLACK, 0));
    wnoutrefresh(msgwin);
}


static void
more(void)
{
    int key, attr = A_NORMAL;
    int cursx, cursy;

    draw_msgwin();

    if (settings.standout)
        attr = A_STANDOUT;

    if (getmaxy(msgwin) == 1) {
        wmove(msgwin, getmaxy(msgwin) - 1, COLNO - 8);
        wattron(msgwin, attr);
        waddstr(msgwin, "--More--");
        wattroff(msgwin, attr);
        wrefresh(msgwin);
    } else {
        newline();
        draw_msgwin();
        wmove(msgwin, getmaxy(msgwin) - 1, COLNO / 2 - 4);
        wattron(msgwin, attr);
        waddstr(msgwin, "--More--");
        wattroff(msgwin, attr);
        wrefresh(msgwin);
    }

    getyx(msgwin, cursy, cursx);
    wtimeout(msgwin, 666);      /* enable blinking */
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
void
new_action(void)
{
    action++;
    start_of_turn_curline = last_redraw_curline = curline;
    draw_msgwin();
}

/* Make sure the bottom message line is empty; if this would scroll something
   off the screen, do a --More-- first if necessary */
void
fresh_message_line(nh_bool canblock)
{
    /* If we would scroll a message off the screen that the user hasn't had a
       chance to look at this redraw, then run more(), else newline(). Because
       the --more-- takes up a line by itself, we need to offset that by one
       line. Thus, a message window of height 2 requires us to always show a
       --more-- even if we're on the first message of the redraw; with height
       3, we can safely newline after the first line of messages but must
       --more-- after the second, etc. getmaxy gives the height of the window
       minus 1, which is why we only subtract 2 not 3. */
    if (!blockafter || action > prevaction)
        newline();
    else if ((curline + MAX_MSGLINES - last_redraw_curline) % MAX_MSGLINES >
             getmaxy(msgwin) - 2)
        more();
    else
        newline();
    blockafter = canblock;
}

static void
curses_print_message_core(int turn, const char *msg, nh_bool canblock)
{
    int hsize, vsize, maxlen;
    nh_bool died;

    if (!msghistory)
        alloc_hist_array();

    if (turn != prevturn)
        /* If the current line is empty, we won't advance past it until
         * something is written there, so go to the previous line in that
         * case. */
        start_of_turn_curline = last_redraw_curline =
            strlen(msglines[curline]) ? curline : curline - 1;

    if (turn < prevturn)        /* going back in time can happen during replay */
        prune_messages(turn);

    if (!*msg)
        return; /* empty message. done. */

    if (action > prevaction) {
        /* re-enable output if it was stopped and start a new line */
        stopprint = FALSE;
        newline();
    }
    prevturn = turn;
    prevaction = action;

    store_message(turn, msg);

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

    getmaxyx(msgwin, vsize, hsize);

    maxlen = hsize;
    if (maxlen >= COLNO)
        maxlen = COLNO - 1;
    if (vsize == 1)
        maxlen -= 8;    /* for "--More--" */

    died = !strncmp(msg, "You die", 7);
    if (strlen(msglines[curline]) + strlen(msg) + 1 < maxlen && !died) {
        if (msglines[curline][0])
            strcat(msglines[curline], "  ");
        strcat(msglines[curline], msg);
    } else {
        int idx, output_count;
        char **output;

        wrap_text(maxlen, msg, &output_count, &output);

        for (idx = 0; idx < output_count; idx++) {
            if (strlen(msglines[curline]) > 0)
                fresh_message_line(canblock);
            if (stopprint)
                break;  /* may get set in more() */
            strcpy(msglines[curline], output[idx]);
        }

        free_wrap(output);
    }

    draw_msgwin();
}


/* The meaning of nonblocking has changed from AceHack. A nonblocking message
   is now one we don't block /after/. */
void
curses_print_message(int turn, const char *inmsg)
{
    curses_print_message_core(turn, inmsg, TRUE);
}

void
curses_print_message_nonblocking(int turn, const char *inmsg)
{
    curses_print_message_core(turn, inmsg, FALSE);
}


void
pause_messages(void)
{
    draw_msgwin();
    if (msglines[curline][0])   /* new text printed this turn */
        more();
}


void
doprev_message(void)
{
    struct nh_menuitem *items;
    char buf[MSGLEN + 1];
    int icount, size, i;

    icount = 0;
    size = 10;
    items = malloc(size * sizeof (struct nh_menuitem));

    for (i = 0; i < histsize; i++) {
        int pos = histpos + i + 1;

        if (pos >= histsize)    /* wrap around eventually */
            pos -= histsize;

        if (!msghistory[pos].turn)
            continue;

        snprintf(buf, MSGLEN + 1, "T:%d\t%s", msghistory[pos].turn,
                 msghistory[pos].msg);
        add_menu_txt(items, size, icount, buf, MI_TEXT);
    }

    curses_display_menu(items, icount, "Message history:", PICK_NONE,
                        PLHINT_ANYWHERE, NULL);
    free(items);
}


void
cleanup_messages(void)
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

/* Given the string "input", generate a series of strings of the given
 * maximum width, wrapping lines at spaces in the text.  The number of
 * lines will be stored into *output_count, and an array of the output
 * lines will be stored in *output.  The memory for both the output strings
 * and the output array is obtained via malloc and should be freed when
 * no longer needed. */
static void
wrap_text(int width, const char *input, int *output_count, char ***output)
{
    const int min_width = 20, max_wrap = 20;
    int len = strlen(input);
    int input_idx, input_lidx;
    int idx, outcount;

    *output = malloc(max_wrap * sizeof (char *));
    for (idx = 0; idx < max_wrap; idx++)
        (*output)[idx] = NULL;

    input_idx = 0;
    outcount = 0;
    do {
        if (len - input_idx <= width) {
            (*output)[outcount] = strdup(input + input_idx);
            outcount++;
            break;
        }

        for (input_lidx = input_idx + width;
             !isspace(input[input_lidx]) && input_lidx - input_idx > min_width;
             input_lidx--) {
        }
        if (!isspace(input[input_lidx]))
            input_lidx = input_idx + width;

        (*output)[outcount] =
            strndup(input + input_idx, input_lidx - input_idx);
        outcount++;

        for (input_idx = input_lidx; isspace(input[input_idx]); input_idx++) {
        }
    } while (input[input_idx] && outcount < max_wrap);

    *output_count = outcount;
}

static void
free_wrap(char **wrap_output)
{
    const int max_wrap = 20;
    int idx;

    for (idx = 0; idx < max_wrap; idx++) {
        if (!wrap_output[idx])
            break;
        free(wrap_output[idx]);
    }
    free(wrap_output);
}
