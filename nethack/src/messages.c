/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Derrick Sund, 2014-03-04 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include <ctype.h>

#include "nhcurses.h"

struct msghist_entry {
    int turn;               /* for displaying in the UI */
    char *message;          /* the message; can be NULL if this slot is empty */
    nh_bool old;            /* true if the user has acted since seeing it */
    nh_bool unseen;         /* true if the user must see it but has not, yet */
    nh_bool nomerge;        /* don't merge this message with the next one */
};

static struct msghist_entry *histlines; /* circular buffer */
static int histlines_alloclen;          /* allocated length of histlines */
static int histlines_pointer;           /* next unused histline entry */
static nh_bool last_line_reserved;      /* keep the last line of msgwin blank */
static int first_unseen = -1;           /* first unseen histline entry */
static int first_new = -1;              /* first non-"old" histline entry */
static nh_bool stopmore = 0;            /* stop doing input at --More-- */

/* Allocates space for settings.msghistory lines of message history, or adjusts
   the message history to the given amount of space if it's already been
   allocated. We actually allocate an extra line, so that histlines_pointer can
   always point to a blank line; this simplifies the code. */
void
alloc_hist_array(void)
{
    static struct msghist_entry *newhistlines;
    int i, j;

    newhistlines = calloc((settings.msghistory + 1), sizeof *newhistlines);

    if (!newhistlines)
        return;                            /* just keep using the old array */

    i = histlines_pointer + 1;
    j = 0;

    first_unseen = -1;
    first_new = -1;

    if (histlines) {
        while (i != histlines_pointer) {
            if (histlines[i].message) {
                newhistlines[j] = histlines[i];

                if (histlines[j].unseen && first_unseen == -1)
                    first_unseen = j;
                if (!histlines[j].old && first_new == -1)
                    first_new = j;

                j++;
                j %= settings.msghistory + 1;
            }
            i++;
            i %= histlines_alloclen;
        }
        free(histlines);
    }
    histlines = newhistlines;
    histlines_pointer = j;
    histlines_alloclen = settings.msghistory + 1;
}

/* Deallocate and reset state used by the messages code. */
void
cleanup_messages(void)
{
    int i;
    if (histlines)
        for (i = 0; i < histlines_alloclen; i++)
            free(histlines[i].message);            /* free(NULL) is legal */
    free(histlines);
    histlines = 0;
    histlines_alloclen = 0;
    histlines_pointer = 0;
    first_unseen = -1;
    first_new = -1;
}

/* Appends second to the end of first, which will be reallocated larger to fit
   if necessary. The allocated length of first is stored in first_alloclen.
   *first can be NULL, in which case *first_alloclen must be 0; first itself
   must point to a valid char pointer. If second is NULL, the function returns
   without doing anything. */
static void
realloc_strcat(char **first, int *first_alloclen, char *second)
{
    int strlen_first = *first ? strlen(*first) : 0;
    int strlen_second = second ? strlen(second) : 0;

    if (!second)
        return;

    if (strlen_first + strlen_second >= *first_alloclen) {
        int first_was_null = !*first;

        *first_alloclen = ((strlen_first + strlen_second + 1) & 256) + 256;
        *first = realloc(*first, *first_alloclen);

        if (first_was_null)
            **first = 0;
    }

    strcat(*first, second);
}

/* Calculates how many messages fit into the message area, and optionally draws
   them (controlled by "dodraw"). The last "offset" lines on screen will not be
   shown (allowing you to scroll back the message buffer in order to print
   earlier parts of a long message, or to add a --More-- when previously none
   was needed). The return value is TRUE if the first character of the first
   unseen message line fits onto the screen under the given settings (or if it
   doesn't exist), FALSE otherwise. This respects last_line_reserved with
   space_for_more FALSE, and draws a --More-- otherwise. */
static nh_bool
layout_msgwin(nh_bool dodraw, int offset, nh_bool more, nh_bool mark_seen)
{
    int ypos = getmaxy(msgwin) - 1;
    int hp = histlines_pointer;
    int last_on_this_line = hp;
    int chars_on_this_line = more ? 9 : 0;
    int rv = first_unseen == -1;

    if (!histlines)
        alloc_hist_array();

    if (dodraw)
        werase(msgwin);

    /* Sometimes we don't want to write to the last line. */
    if (last_line_reserved && !more)
        ypos--;

    /* If we're reserving the /only/ line on a 1-line message buffer, we
       obviously don't print anything. */
    if (ypos < 0) {
        wnoutrefresh(msgwin);
        return rv;
    }

    ypos += offset;

    /* Run backwards through the messages, printing as many as will fit. */
    do {
        int hp_len = 0;

        /* Find the previous message line. */
        if (hp == 0)
            hp = histlines_alloclen - 1;
        else
            hp--;

        if (histlines[hp].message)
            hp_len = strlen(histlines[hp].message);

        /* Does this message need to end a new line? (Note: "end" rather than
           "start" because we're looping backwards.) */
        if (!histlines[hp].message || histlines[hp].nomerge ||
            hp_len + chars_on_this_line + 2 > getmaxx(msgwin)) {

            /* Yep; lay out the subsequent messages. */
            char *nextline = NULL;
            int nextlinelen = 0;
            int hp2 = hp;
            int anyold = 0;
            int first_unseen_seen = 0;

            do {
                hp2++;
                hp2 %= histlines_alloclen;

                if (histlines[hp2].message && nextlinelen) {
                    realloc_strcat(&nextline, &nextlinelen, "  ");
                }
                realloc_strcat(&nextline, &nextlinelen, histlines[hp2].message);

                if (histlines[hp2].message && histlines[hp2].old)
                    anyold = 1;

                if (hp2 == first_unseen)
                    first_unseen_seen = 1;

            } while (hp2 != last_on_this_line);

            /* It could be that we have nothing to print. */
            if (nextline) {

                /* If there's a single subsequent message and it's very long, we
                   need to wrap it. It's simplest to always call the wrapping
                   code. */
                char **wrapped_nextline;
                int wrap_linecount;
                attr_t colorattr = anyold ?
                    curses_color_attr(COLOR_BLACK, 0) :
                    curses_color_attr(COLOR_WHITE, 0);

                wrap_text(getmaxx(msgwin) - (more ? 9 : 0), nextline,
                          &wrap_linecount, &wrapped_nextline);
                free(nextline);

                while (wrap_linecount--) {
                    if (dodraw && ypos >= 0 && ypos < getmaxy(msgwin)) {
                        char *p = wrapped_nextline[wrap_linecount];

                        wmove(msgwin, ypos, 0);

                        while (*p)
                            waddch(msgwin, *p++ | colorattr);

                        if (more) {
                            p = " --More--";
                            while (*p)
                            waddch(msgwin, *p++ |
                                   curses_color_attr(COLOR_WHITE + 8, 0));
                        }

                    }
                    if (ypos == getmaxy(msgwin) - 1)
                        more = 0;
                    ypos--;
                }

                if (ypos >= -1 && ypos < getmaxy(msgwin) - 1 && mark_seen) {
                    /* The first characters of each of these lines were written
                       onto the screen. Mark them as seen. (If we have ypos <
                       -1, then we only drew part of the line, so it's not
                       properly "seen". If we have ypos >= getmaxy(msgwin), they
                       were excluded by the offset, and still aren't seen.)

                       Code using offsets to suppress later parts of a long
                       message line will need to ensure by itself that the later
                       parts of the long message are printed. */
                    hp2 = hp;

                    do {
                        hp2++;
                        hp2 %= histlines_alloclen;

                        histlines[hp2].unseen = 0;
                    } while (hp2 != last_on_this_line);
                }

                if (ypos >= -1 && first_unseen_seen) {
                    /* We went to or past the first unseen line. (Going past can
                       happen in some obscure cases involving --More--.) Return
                       TRUE, and find the new first unseen line (which may be
                       the same as the old one. */
                    rv = 1;
                    if (mark_seen && ypos < getmaxy(msgwin) - 1) {

                        /* Find the new first unseen line, if any. */
                        hp2 = histlines_pointer;
                        first_unseen = -1;

                        while (hp2 != hp) {
                            if (hp2 == 0)
                                hp2 = histlines_alloclen - 1;
                            else
                                hp2--;

                            if (histlines[hp2].message &&
                                histlines[hp2].unseen)
                                first_unseen = hp2;
                        }
                    }
                }

                free_wrap(wrapped_nextline);
            }

            last_on_this_line = hp;
            chars_on_this_line = more ? 10 : 0;
        }

        chars_on_this_line += hp_len;

        /* If we just printed the last message line in the buffer, or that
           fits onto the screen, exit. */
    } while (ypos >= 0 && histlines[hp].message);

    if (dodraw)
        wnoutrefresh(msgwin);

    return rv;
}

static void
keypress_at_more(void)
{
    int continue_looping = 1;
    if (stopmore)
        return;

    while (continue_looping) {
        switch (get_map_key(FALSE)) {
        case KEY_ESCAPE:
            stopmore = 1;
            continue_looping = 0;
            break;
        case ' ':
        case 10:
        case 13:
            continue_looping = 0;
            break;
        }
    }
}

/* Ensure that the user has seen all the messages that they're required to see
   (via displaying them, with --More-- if necessary), finally leaving the last
   onscreen. If more is set, draw a --More-- after the last set, too. */
static void
force_seen(nh_bool more, nh_bool mark_last_screenful_seen) {
    if (!layout_msgwin(0, 0, more, 0)) {
        /* The text so far doesn't fit onto the screen. Draw it, followed by a
           --More--. */
        int offset = 1;
        while (!layout_msgwin(0, offset, 1, 0))
            offset++;
        while (offset > 0) {
            layout_msgwin(1, offset, 1, 1); /* sets unseen to 0 */
            keypress_at_more();
            offset -= getmaxy(msgwin);
        }
    }
    layout_msgwin(1, 0, more, mark_last_screenful_seen);
}

/* Draws messages on the screen. Any messages drawn since the last call to
   new_action() are in white; others are in blue. This routine adapts to the
   size of the message buffer.

   This never asks for user input; it's more low-level than that. When adding a
   new message line, the caller will need to arrange for --More-- to be shown
   appropriately by itself. Typically, it's used to redraw the message window
   if it hasn't changed since the last time it was drawn. */
void
draw_msgwin(void)
{
    layout_msgwin(1, 0, 0, 0);
}

/* When called, previous messages should be blued out. Assumes that there has
   been user input with the message window visible since the last message was
   written. */
void
new_action(void)
{
    int hp = first_new;
    int last_hp = hp;
    if (hp == -1)
        return;

    if (!histlines)
        alloc_hist_array();

    while (hp != histlines_pointer) {
        histlines[hp].old = 1;
        last_hp = hp;
        hp++;
        hp %= histlines_alloclen;
    }

    /* Don't merge histlines from different actions. */
    histlines[last_hp].nomerge = 1;

    first_new = -1;
    stopmore = 0;

    layout_msgwin(1, 0, 0, 1);
}

/* Make sure the bottom message line is empty. If this would scroll something
   off the screen, do a --More-- first if necessary. */
void
fresh_message_line(nh_bool canblock)
{
    force_seen(0, 0);
    last_line_reserved = 1;
    if (!layout_msgwin(0, 0, 0, 0)) {
        layout_msgwin(1, 0, 1, 1);
        keypress_at_more();
    }
    layout_msgwin(1, 0, 0, 1);
}

static void
curses_print_message_core(int turn, const char *msg, nh_bool canblock)
{
    struct msghist_entry *h;

    if (!histlines)
        alloc_hist_array();

    h = histlines + histlines_pointer;

    last_line_reserved = 0;

    free(h->message); /* in case there was something there */
    h->turn = turn;
    h->message = malloc(strlen(msg)+1);
    strcpy(h->message, msg);
    h->old = 0;
    h->unseen = canblock;
    h->nomerge = 0;

    if (first_new == -1)
        first_new = histlines_pointer;
    if (first_unseen == -1 && canblock)
        first_unseen = histlines_pointer;

    histlines_pointer++;
    histlines_pointer %= histlines_alloclen;

    free(histlines[histlines_pointer].message);
    histlines[histlines_pointer].message = 0;

    if (!layout_msgwin(0, 0, 0, 0))
        force_seen(0, 0); /* print a --More-- at the appropriate point */
    else
        layout_msgwin(1, 0, 0, 0);
}

/* Prints a message onto the screen, and into message history. The code will
   ensure that the user sees the message (e.g. with --More--). */
void
curses_print_message(int turn, const char *inmsg)
{
    curses_print_message_core(turn, inmsg, TRUE);
}

/* Prints a message into message history, and shows it onscreen unless there are
   more important messages to show. */
void
curses_print_message_nonblocking(int turn, const char *inmsg)
{
    curses_print_message_core(turn, inmsg, FALSE);
}

/* Ensure that the user has seen all messages printed so far, before
   continuing to run the rest of the program. */
void
pause_messages(void)
{
    if (first_unseen != -1) {
        force_seen(1, 1);
        stopmore = 0;
        keypress_at_more();
    }
    draw_msgwin();
}

/* Displays the message history. */
void
doprev_message(void)
{
    int i, j, lines = 0;
    struct nh_menulist menu;
    char **buf;

    init_menulist(&menu);

    if (!histlines)
        alloc_hist_array();

    i = histlines_pointer;
    do {
        if (!histlines[i].message) {
            i = (i + 1) % settings.msghistory;
            continue;
        }
        /* Subtracting 3 is necessary to prevent curses_display_menu in
           smallterm games from eating the last part of the message here.
           Subtracting 4 allows slight indentation where appropriate. */
        wrap_text(getmaxx(msgwin) - 4, histlines[i].message, &lines, &buf);
        for (j = 0; j < lines; j++) {
            /* If a message wraps, very slightly indent the additional lines
               to make them obvious. */
            char tempstr[getmaxx(msgwin)];
            sprintf(tempstr, "%s%s", j == 0 ? "" : " ", buf[j]);
            add_menu_txt(&menu, tempstr, MI_TEXT);
        }
        i = (i + 1) % settings.msghistory;
    } while (i != histlines_pointer);

    curses_display_menu_core(&menu, "Previous messages:", PICK_NONE, NULL, 0, 0,
                             -1, -1, TRUE, NULL);
}

/* Given the string "input", generate a series of strings of the given maximum
   width, wrapping lines at spaces in the text. The number of lines will be
   stored into *output_count, and an array of the output lines will be stored in
   *output. The memory for both the output strings and the output array is
   obtained via malloc and should be freed when no longer needed. */
void
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
             input_lidx--) {}

        if (!isspace(input[input_lidx]))
            input_lidx = input_idx + width;

        (*output)[outcount] = malloc(input_lidx - input_idx + 1);
        strncpy((*output)[outcount], input + input_idx, input_lidx - input_idx);
        (*output)[outcount][input_lidx - input_idx] = '\0';
        outcount++;

        for (input_idx = input_lidx; isspace(input[input_idx]); input_idx++) {}
    } while (input[input_idx] && outcount < max_wrap);

    *output_count = outcount;
}


void
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
