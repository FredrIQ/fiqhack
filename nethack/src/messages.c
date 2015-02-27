/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-02-27 */
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
    nh_bool temp;           /* true if the message is temporary (to be erased) */
};

static struct msghist_entry *histlines; /* circular buffer */
static int histlines_alloclen;          /* allocated length of histlines */
static int histlines_pointer;           /* next unused histline entry */
static int first_unseen = -1;           /* first unseen histline entry */
static int first_new = -1;              /* first non-"old" histline entry */
static nh_bool stopmore = 0;            /* stop doing input at --More-- */

static struct msghist_entry *showlines; /* lines to be displayed; noncircular.
                                           showlines[0] is bottom message. */
static int num_showlines;               /* number of lines in the message buf */

static const char* more_text = " --More--";   /* The string to use in more
                                                 prompts */

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

    if (showlines)
        for (i = 0; i < num_showlines; i++)
            free(showlines[i].message);
    free(showlines);
    num_showlines = 0;
}

/* Allocate showlines. */
void
setup_showlines(void)
{
    num_showlines = getmaxy(msgwin);
    showlines = calloc((num_showlines + 1), sizeof *showlines);
    int i;
    for (i = 0; i < num_showlines; i++) {
        showlines[i].turn = -1;
        showlines[i].message = NULL;
        showlines[i].old = FALSE;
        showlines[i].unseen = FALSE;
        showlines[i].nomerge = FALSE;
        showlines[i].temp = FALSE;
    }
}

/* Reallocate showlines (preserving existing messages where possible) if the
   window gets resized. */
void
redo_showlines(void)
{
    int new_num_showlines = getmaxy(msgwin);
    static struct msghist_entry *new_showlines;
    new_showlines = calloc((new_num_showlines + 1), sizeof *new_showlines);
    int i;
    for (i = 0; i < new_num_showlines && i < num_showlines; i++) {
        new_showlines[i].turn = showlines[i].turn;
        new_showlines[i].message = showlines[i].message;
        new_showlines[i].old = showlines[i].old;
        new_showlines[i].unseen = showlines[i].unseen;
        new_showlines[i].nomerge = showlines[i].nomerge;
        new_showlines[i].temp = showlines[i].temp;
    }
    /* At most one of the following loops will execute at all. */
    for (; i < new_num_showlines; i++) {
        new_showlines[i].turn = -1;
        new_showlines[i].message = NULL;
        new_showlines[i].old = TRUE;
        new_showlines[i].unseen = FALSE;
        new_showlines[i].nomerge = FALSE;
        new_showlines[i].temp = FALSE;
    }
    for (; i < num_showlines; i++) {
        free(showlines[i].message);
    }
    free(showlines);
    showlines = new_showlines;
    num_showlines = new_num_showlines;
}

/* Appends second to the end of first, which will be reallocated larger to fit
   if necessary. The allocated length of first is stored in first_alloclen.
   *first can be NULL, in which case *first_alloclen must be 0; first itself
   must point to a valid char pointer. If second is NULL, the function returns
   without doing anything. */
static void
realloc_strcat(char **first, int *first_alloclen, const char *second)
{
    int strlen_first = *first ? strlen(*first) : 0;
    int strlen_second = second ? strlen(second) : 0;

    if (!second)
        return;

    if (strlen_first + strlen_second >= *first_alloclen) {
        int first_was_null = !*first;

        *first_alloclen = ((strlen_first + strlen_second + 1) / 256)
                          * 256 + 256;
        *first = realloc(*first, *first_alloclen);

        if (first_was_null)
            **first = 0;
    }

    strcat(*first, second);
}

static void
show_msgwin(nh_bool more)
{
    werase(msgwin);
    int i;
    for (i = num_showlines - 1; i >= 0; i--) {
        wmove(msgwin, num_showlines - 1 - i, 0);
        if(!showlines[i].message)
            continue;
        const char *p = showlines[i].message;
        attr_t color_attr = showlines[i].old ?
            curses_color_attr(COLOR_BLACK, 0) :
            curses_color_attr(COLOR_WHITE, 0);
        while (*p)
            waddch(msgwin, *p++ | color_attr);
        if (i == 0 && more) {
            p = more_text;
            while (*p)
                waddch(msgwin, *p++ | curses_color_attr(COLOR_WHITE + 8, 0));
        }
    }
    wnoutrefresh(msgwin);
}

static void
mark_all_seen(nh_bool mark_old)
{
    int i;
    for (i = 0; i < num_showlines; i++) {
        showlines[i].unseen = FALSE;
        showlines[i].nomerge = TRUE;
        if (mark_old)
            showlines[i].old = TRUE;
    }
}

static void
keypress_at_more(void)
{
    int continue_looping = 1;
    /* Well, we've at least tried to give a --More--.  Any failure to see
       the currently-visible messages is the players own fault. */
    mark_all_seen(FALSE);
    if (stopmore)
        return;

    while (continue_looping) {
        switch (get_map_key(FALSE, FALSE, krc_more)) {
        case KEY_SIGNAL:
            /* This happens when a watcher is stuck at a --More-- when the
               watchee gives a command. Just move on, so we don't end up behind
               forever; and repeat the signal, because it's still relevant.

               This mechanism is needed even when we don't think we're watching;
               disconnected processes get forced into implicit watch mode if the
               user reconnects before the process times out, and so a process
               that thought it was playing might unexpectely get a signal. */
            uncursed_signal_getch();
            stopmore = 1;
            continue_looping = 0;
            break;

        case KEY_ESCAPE:
        case '\x1b':
            stopmore = 1;
            continue_looping = 0;
            break;

        case ' ':
        case 10:
        case 13:
        case KEY_ENTER:
            continue_looping = 0;
            break;
        }
    }
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
    show_msgwin(FALSE);
}


/* When called, previous messages should be blued out. Assumes that there has
   been user input with the message window visible since the last message was
   written. */
void
new_action(void)
{
    mark_all_seen(TRUE);
    draw_msgwin();

    int hp = first_new;
    int last_hp = hp;
    if (hp == -1)
        return;

    if (!histlines)
        alloc_hist_array();

    /* Don't merge histlines from different actions. */
    while (hp != histlines_pointer) {
        last_hp = hp;
        hp++;
        hp %= histlines_alloclen;
    }
    histlines[last_hp].nomerge = 1;

    first_new = -1;
    stopmore = 0;
}

/* Accepts negative numbers */
static void
move_lines_upward(int num_to_bump)
{
    int i;
    if (num_to_bump > 0) {
        for (i = num_showlines - 1; i >= num_showlines - num_to_bump; i--)
            free(showlines[i].message);
        for (i = num_showlines - 1; i >= num_to_bump; i--) {
            showlines[i].message = showlines[i - num_to_bump].message;
            showlines[i].turn = showlines[i - num_to_bump].turn;
            showlines[i].old = showlines[i - num_to_bump].old;
            showlines[i].unseen = showlines[i - num_to_bump].unseen;
            showlines[i].nomerge = showlines[i - num_to_bump].nomerge;
            showlines[i].temp = showlines[i - num_to_bump].temp;
        }
        for (; i >= 0; i--) {
            showlines[i].message = NULL;
            showlines[i].turn = -1;
            showlines[i].old = FALSE;
            showlines[i].unseen = FALSE;
            showlines[i].nomerge = FALSE;
            showlines[i].temp = FALSE;
        }
    } else if (num_to_bump < 0) {
        num_to_bump = -num_to_bump;
        for (i = 0; i < num_to_bump; ++i)
            free(showlines[i].message);
        for (i = 0; i < num_showlines - num_to_bump; ++i) {
            showlines[i].message = showlines[i + num_to_bump].message;
            showlines[i].turn = showlines[i + num_to_bump].turn;
            showlines[i].old = showlines[i + num_to_bump].old;
            showlines[i].nomerge = showlines[i + num_to_bump].nomerge;
            showlines[i].unseen = showlines[i + num_to_bump].unseen;
            showlines[i].temp = showlines[i + num_to_bump].temp;
        }
        for (; i < num_showlines; ++i) {
            showlines[i].message = NULL;
            showlines[i].turn = -1;
            showlines[i].old = FALSE;
            showlines[i].unseen = FALSE;
            showlines[i].nomerge = FALSE;
            showlines[i].temp = FALSE;
        }
    }
}

/* Update the showlines array with new string text from intermediate.
   Returns TRUE if we're going to need a --More-- and another pass. */
static nh_bool
update_showlines(char **intermediate, int *length, nh_bool force_more,
                 nh_bool important, nh_bool temporary)
{
    /*
     * Each individual step in this can be ugly, but the overall logic isn't
     * terribly complicated.
     * STEP 1: Determine whether the string already present in showlines[0]
     *         (that is, the one at the bottom of the message window) should be
     *         merged with the text in intermediate.  Create a new buffer, buf,
     *         out of the combination of (possibly) the text from showlines[0]
     *         and the text from intermediate.
     * STEP 2: Wrap the buffer we got in Step 1.  Count how many showlines we
     *         can and should bump upward to make room for the new text.  If
     *         we can't make enough room to fit all of the wrapped lines from
     *         buf, make a note that we're going to need another pass/more.
     * STEP 3: Shift the showlines messages, freeing the ones that fall off the
     *         end, and put the wrapped lines in the freed slots.
     * STEP 4: Eliminate the first part of intermediate as needed by traversing
     *         the buffer.  Check each of the newly-displayed showlines' length,
     *         and for each one have a char pointer called "marker" jump that
     *         many characters ahead.
     * STEP 5: If we need another pass, strip tokens off the end of showlines[0]
     *         and simultaneously rewind marker one token at a time until we
     *         have room for a more prompt.
     */

    /* Step 1 begins here. */
    int messagelen = 0;
    nh_bool merging = FALSE;
    /* Unimportant messages are not marked as unseen, unless the previous
     * message was also unseen. This is so that the unseen messages always form
     * a contiguous block at the end of the message array.
     *
     * TODO: Replace msghist_entry.unseen with a num_unseen variable.
     */
    nh_bool mark_unseen = important || showlines[0].unseen;
    nh_bool need_more = force_more && showlines[0].unseen;
    if (showlines[0].message)
        messagelen = strlen(showlines[0].message);

    char buf[strlen(*intermediate) + messagelen + 3];

    if (!showlines[0].nomerge && showlines[0].message &&
        /* Compare negations because 2 != 1 but !2 == !1 */
        (!showlines[0].temp == !temporary)) {
        strcpy(buf, showlines[0].message);
        strcat(buf, "  ");
        strcat(buf, *intermediate);
        merging = TRUE;
    }
    else if (!showlines[0].message) {
        /* Setting merging to TRUE means showlines[0].message will be freed,
           but free(NULL) is legal. */
        strcpy(buf, *intermediate);
        merging = TRUE;
    }
    else
        strcpy(buf, *intermediate);

    /* Step 2 begins here. */
    char **wrapped_buf = NULL;
    int num_buflines = 0;
    wrap_text(getmaxx(msgwin), buf, &num_buflines, &wrapped_buf);
    /* Sometimes, this function will be called with an empty string to format
       properly for a --More--.  This avoids any resulting awkwardness. */
    if(strlen(wrapped_buf[0]) == 0) {
        free_wrap(wrapped_buf);
        wrapped_buf = NULL;
        num_buflines = 0;
        merging = FALSE;
    }
    /* Determine the number of entries in showlines to bump off the top and
       into the gaping maw of free().  It is bounded above by:
       1: num_buflines
       2: the number of showlines that have been seen and can legally be
          bumped. */
    int num_can_bump = 0;
    int i;
    for (i = 0; i < num_showlines; i++)
        if (!showlines[i].unseen)
            num_can_bump++;

    int num_to_bump = num_can_bump;
    if (num_to_bump >= num_buflines)
        num_to_bump = num_buflines;
    //XXX: num_to_bump is sometimes negative, particularly when quitting
    //XXX: FIX THIS
    if (merging && num_to_bump > 0)
        num_to_bump--;

    /* If we're merging, we'll need a --More-- if num_to_bump is strictly
       smaller than num_buflines - 1.
       If we're not merging, we'll need a --More-- if num_to_bump is strictly
       smaller than num_buflines. */
    if ((num_to_bump < num_buflines - 1) ||
        (!merging && num_to_bump < num_buflines))
        need_more = TRUE;

    if (merging)
        free(showlines[0].message);

    /* Step 3 begins here. */
    move_lines_upward(num_to_bump);

    if (!merging) {
        for (i = num_to_bump - 1; i >= 0; i--)
        {
            showlines[i].message =
                malloc(strlen(wrapped_buf[num_to_bump - 1 - i]) + 1);
            strcpy(showlines[i].message, wrapped_buf[num_to_bump - 1 - i]);
            showlines[i].unseen = mark_unseen;
            showlines[i].nomerge = FALSE;
            showlines[i].temp = temporary;
        }
    }
    else {
        for (i = num_to_bump; i >= 0; i--)
        {
            showlines[i].message =
                malloc(strlen(wrapped_buf[num_to_bump - i]) + 1);
            strcpy(showlines[i].message, wrapped_buf[num_to_bump - i]);
            showlines[i].unseen = mark_unseen;
            showlines[i].nomerge = FALSE;
            showlines[i].old = FALSE;
            showlines[i].temp = temporary;
        }
    }

    /* Step 4 begins here. */
    /* marker will walk across buf until it reaches text that wasn't printed */
    char* marker = buf;
    for (i = 0; i < (merging ? num_to_bump + 1 : num_to_bump); i++) {
        marker += strlen(wrapped_buf[i]);
        while(*marker == ' ')
            marker++; /* Traverse forward past whitespace */
    }

    /* Step 5 begins here. */
    while (showlines[0].message && need_more &&
           strlen(showlines[0].message) > getmaxx(msgwin) - strlen(more_text)) {
        /* Find the last space in the current showlines[0]. */
        char *last;
        last = strrchr(showlines[0].message, ' ');
        if (last) {
            /* rewind marker so the token will wind up in the buffer again.
               n.b.: to_rewind will always be at least two if there's two
               spaces to go back (end of a sentence), because of the ending
               punctuation mark. */
            while (marker > buf && *(marker - 1) != ' ')
                marker--; /* might've been more than one space */
            int to_rewind = strlen(last);
            marker -= to_rewind;
            /* NULL out the space in showlines[0] */
            *last = '\0';
        }
        else {
            /* If the showlines[0] string doesn't *have* any whitespace, just
               kind of split it up anyway.  This case will usually come up on
               squares with dozens of Elbereths on them and probably nowhere
               else. */
            last = showlines[0].message + getmaxx(msgwin) - strlen(more_text);
            marker -= strlen(last);
            *last = '\0';
        }
    }
    /* At this point, *marker might be NULL if we printed everything in buf.
       Doesn't matter, though. */
    strcpy(*intermediate, "");
    realloc_strcat(intermediate, length, marker);

    free_wrap(wrapped_buf);
    return need_more;
}

/* Marks all showlines as seen.  Called at the start of each turn to ensure that
   no extraneous --More-- stuff gets printed if the player opens a Count: window
   or something similar. */
void
mark_showlines_seen(void)
{
    int i;
    for (i = 0; i < num_showlines; i++) {
        showlines[0].unseen = FALSE;
    }
}

/* Guarantee the player sees the current message buffer by forcing a more prompt
   if this is legal. */
static void
force_seen(void)
{
    /* dummy is just "" initially, but forcing a more in update_showlines might
       lop a few tokens off the end of showlines[0].message and put them into
       dummy.  That's why we need to call update_showlines in a loop. */
    char* dummy = malloc(1);
    int dummy_length = 1;
    strcpy(dummy, "");
    nh_bool keep_going = TRUE;
    showlines[0].nomerge = FALSE;
    while (keep_going) {
        keep_going = update_showlines(&dummy, &dummy_length, TRUE, FALSE, FALSE);
        show_msgwin(keep_going);
        if (keep_going)
            keypress_at_more();
    }
    free(dummy);
}

/* Make sure the bottom message line is empty. If this would scroll something
   off the screen, do a --More-- first if necessary. */
void
fresh_message_line(nh_bool canblock)
{
    /* If the top line is already seen, just bump the messages without
       calling force_seen. */
    if (showlines[num_showlines - 1].unseen)
        force_seen();
    if (showlines[0].message)
        move_lines_upward(1);
}

static void
curses_print_message_core(int turn, const char *msg, nh_bool important)
{
    /* First, add the message to the message history.  Do this before deciding
       whether to print it; "unimportant" messages always show up in ^P. */
    struct msghist_entry *h;
    nh_bool temporary = turn < 0;

    if (!histlines)
        alloc_hist_array();

    if (!temporary) {
        h = histlines + histlines_pointer;

        free(h->message); /* in case there was something there */
        h->turn = turn;
        h->message = malloc(strlen(msg)+1);
        strcpy(h->message, msg);
        h->nomerge = 0;

        if (first_new == -1)
            first_new = histlines_pointer;

        histlines_pointer++;
        histlines_pointer %= histlines_alloclen;

        free(histlines[histlines_pointer].message);
        histlines[histlines_pointer].message = 0;
    }

    /* If we're in a small terminal, suppress certain messages, like the one
       asking in which direction to kick. */
    if (!important && num_showlines == 1)
        return;

    /* Now actually print the message. */
    nh_bool keep_going = TRUE;

    char *intermediate;
    intermediate = calloc(strlen(msg) + 1, sizeof (char));
    int intermediate_size = strlen(msg) + 1;
    strcpy(intermediate, msg);
    while (keep_going) {
        keep_going = update_showlines(&intermediate, &intermediate_size, FALSE,
                                      important, temporary);
        show_msgwin(keep_going);
        if (keep_going)
            keypress_at_more();
    }
    free(intermediate);
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

/* Prints a message to the message history with the expectation it will be
   erased later. */
void
curses_temp_message(const char *msg)
{
    curses_print_message_core(-1, msg, TRUE);
}

/* Clear the temporary messages from the buffer. Assumes that they are
   contiguous at the end. */
void
curses_clear_temp_messages(void)
{
    int i = 0;
    while (i < num_showlines && showlines[i].temp)
        ++i;

    move_lines_upward(-i);
}

/* Ensure that the user has seen all messages printed so far, before
   continuing to run the rest of the program. */
void
pause_messages(void)
{
    stopmore = 0;
    force_seen();
}

/* Displays the message history. */
void
doprev_message(void)
{
    int i, j, curlinelen = 0, lines = 0;
    struct nh_menulist menu;
    char *curline = NULL;
    char **buf = NULL;

    init_menulist(&menu);

    if (!histlines)
        alloc_hist_array();

    i = histlines_pointer;
    do {
        if (!histlines[i].message) {
            i = (i + 1) % settings.msghistory;
            continue;
        }
        realloc_strcat(&curline, &curlinelen, histlines[i].message);
        /* This'll mean the string always has two spaces at the end, but
           wrap_text will take care of them for us. */
        realloc_strcat(&curline, &curlinelen, "  ");
        /* If either the next line is where we wrap around or the next line
           is the start of a new turn's worth of messages, quit appending.
           Otherwise, make another append pass. */
        if (!histlines[i].nomerge &&
            ((i + 1) % settings.msghistory != histlines_pointer)) {
            i = (i + 1) % settings.msghistory;
            continue;
        }
        /* Subtracting 3 is necessary to prevent curses_display_menu in
           smallterm games from eating the last part of the message here.
           Subtracting 4 allows slight indentation where appropriate. */
        wrap_text(getmaxx(msgwin) - 4, curline, &lines, &buf);
        free(curline);
        curline = NULL;
        curlinelen = 0;
        for (j = 0; j < lines; j++) {
            /* If a message wraps, very slightly indent the additional lines
               to make them obvious. */
            char tempstr[getmaxx(msgwin)];
            sprintf(tempstr, "%s%s", j == 0 ? "" : " ", buf[j]);
            add_menu_txt(&menu, tempstr, MI_TEXT);
        }
        free_wrap(buf);
        buf = NULL;
        i = (i + 1) % settings.msghistory;
    } while (i != histlines_pointer);

    curses_display_menu_core(
        &menu, "Previous messages:", PICK_NONE, NULL,
        null_menu_callback, 0, 0, -1, -1, TRUE, NULL, TRUE);
}

/* Given the string "input", generate a series of strings of the given maximum
   width, wrapping lines at spaces in the text. The number of lines will be
   stored into *output_count, and an array of the output lines will be stored in
   *output. The memory for both the output strings and the output array is
   obtained via malloc and should be freed when no longer needed. */
void
wrap_text(int width, const char *input, int *output_count, char ***output)
{
    const int min_width = 20;
    int wrap_alloclen = 2;
    int len = strlen(input);
    int input_idx, input_lidx;
    int idx, outcount;

    *output = malloc(wrap_alloclen * sizeof (char *));
    for (idx = 0; idx < wrap_alloclen; idx++)
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

        if (outcount == wrap_alloclen - 1) {
            idx = wrap_alloclen;
            wrap_alloclen *= 2;
            *output = realloc(*output, wrap_alloclen * sizeof (char *));
            for (; idx < wrap_alloclen; idx++)
                (*output)[idx] = NULL;
        }
    } while (input[input_idx]);

    (*output)[outcount] = NULL;
    *output_count = outcount;
}


void
free_wrap(char **wrap_output)
{
    if (!wrap_output)
        return;

    int idx;
    for (idx = 0; wrap_output[idx]; idx++) {
        free(wrap_output[idx]);
    }
    free(wrap_output);
}
