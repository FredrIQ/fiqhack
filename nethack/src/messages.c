/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include <ctype.h>

#include "nhcurses.h"
#include "messagechannel.h"

/* Default colors for each message channel. We have eight possible message
   colours: the seven bold/bright colours that aren't dark gray, and light
   gray. CLRFLAG_* constants can also be ORed in. */
static const int channel_color[] = {
    /* CLR_GRAY: normal misses, cancelled actions, etc. */
    [msgc_nonmongood] = CLR_GRAY,
    [msgc_moncombatgood] = CLR_GRAY,
    [msgc_playerimmune] = CLR_GRAY,
    [msgc_petcombatbad] = CLR_GRAY,
    [msgc_cancelled] = CLR_GRAY,
    [msgc_failrandom] = CLR_GRAY,
    [msgc_trapescape] = CLR_GRAY,
    [msgc_notarget] = CLR_GRAY,
    [msgc_yafm] = CLR_GRAY,

    /* also CLR_GRAY: low-priority messages with no real implications */
    [msgc_alignchaos] = CLR_GRAY, /* e.g. chaotic action as a neutral */
    [msgc_monneutral] = CLR_GRAY,
    [msgc_npcvoice] = CLR_GRAY,
    [msgc_petneutral] = CLR_GRAY,
    [msgc_occstart] = CLR_GRAY,
    [msgc_levelsound] = CLR_GRAY,
    [msgc_branchchange] = CLR_GRAY,
    [msgc_rumor] = CLR_GRAY,
    [msgc_consequence] = CLR_GRAY,

    /* CLR_ORANGE: actions going wrong due to spoiler info; also warnings
       that are not important enough to be bright magenta */
    [msgc_notresisted] = CLR_ORANGE,
    [msgc_petwarning] = CLR_ORANGE,
    [msgc_substitute] = CLR_ORANGE,
    [msgc_failcurse] = CLR_ORANGE,
    [msgc_combatimmune] = CLR_ORANGE,
    [msgc_unpaid] = CLR_ORANGE,
    [msgc_cancelled1] = CLR_ORANGE, /* here because it wasted time */
    [msgc_levelwarning] = CLR_ORANGE,

    /* CLR_BRIGHT_GREEN: permanent non-spammy good things */
    [msgc_intrgain] = CLR_BRIGHT_GREEN,
    [msgc_outrogood] = CLR_BRIGHT_GREEN,

    /* CLR_YELLOW: spammy/common bad events (e.g. being hit in combat) */
    [msgc_moncombatbad] = CLR_YELLOW,
    [msgc_itemloss] = CLR_YELLOW,
    [msgc_statusbad] = CLR_YELLOW,
    [msgc_statusend] = CLR_YELLOW,
    [msgc_alignbad] = CLR_YELLOW,
    [msgc_interrupted] = CLR_YELLOW,
    [msgc_nonmonbad] = CLR_YELLOW,
    [msgc_badidea] = CLR_YELLOW,
    [msgc_outrobad] = CLR_YELLOW,

    /* CLR_BRIGHT_BLUE: metagame messages, hints, debug messages, etc. */
    [msgc_emergency] = CLR_BRIGHT_BLUE,
    [msgc_impossible] = CLR_BRIGHT_BLUE,
    [msgc_saveload] = CLR_BRIGHT_BLUE,
    [msgc_debug] = CLR_BRIGHT_BLUE,
    [msgc_noidea] = CLR_BRIGHT_BLUE,
    [msgc_intro] = CLR_BRIGHT_BLUE,
    [msgc_info] = CLR_BRIGHT_BLUE,
    [msgc_controlhelp] = CLR_BRIGHT_BLUE,
    [msgc_hint] = CLR_BRIGHT_BLUE,
    [msgc_uiprompt] = CLR_BRIGHT_BLUE,
    [msgc_curprompt] = CLR_BRIGHT_BLUE,
    [msgc_reminder] = CLR_BRIGHT_BLUE,    /* fades to dark blue immediately */

    /* CLR_BRIGHT_MAGENTA: permanent non-spammy bad things, urgent warnings */
    [msgc_fatal] = CLR_BRIGHT_MAGENTA | CLRFLAG_FORCETAB,
    [msgc_fatalavoid] = CLR_BRIGHT_MAGENTA | CLRFLAG_FORCEMORE,
    [msgc_petfatal] = CLR_BRIGHT_MAGENTA,
    [msgc_npcanger] = CLR_BRIGHT_MAGENTA,
    [msgc_intrloss] = CLR_BRIGHT_MAGENTA,

    /* CLR_BRIGHT_CYAN: healing and temporary status gains; unexpected good
       events that need to look different from CLR_WHITE; may be somewhat
       spammy */
    [msgc_statusgood] = CLR_BRIGHT_CYAN,
    [msgc_statusheal] = CLR_BRIGHT_CYAN,
    [msgc_itemrepair] = CLR_BRIGHT_CYAN,
    [msgc_aligngood] = CLR_BRIGHT_CYAN,
    [msgc_combatalert] = CLR_BRIGHT_CYAN,
    [msgc_discoverportal] = CLR_BRIGHT_CYAN | CLRFLAG_FORCEMORE,
    [msgc_youdiscover] = CLR_BRIGHT_CYAN,

    /* CLR_WHITE: spammy/common good events (e.g. hitting in combat) */
    [msgc_statusextend] = CLR_WHITE,
    [msgc_petkill] = CLR_WHITE,
    [msgc_petcombatgood] = CLR_WHITE,
    [msgc_kill] = CLR_WHITE,
    [msgc_combatgood] = CLR_WHITE,
    [msgc_actionok] = CLR_WHITE,
    [msgc_actionboring] = CLR_WHITE,
    [msgc_nospoil] = CLR_WHITE,

    /* Special handling, that violates normal rules */
    [msgc_intrloss_level] = 0, /* msgc_intrloss but never forces a more */
    [msgc_intrgain_level] = 0, /* msgc_intrgain but never forces a more */
    [msgc_fatal_predone] = 0,  /* msgc_fatal but never forces a more */
    [msgc_mispaste] = 0,       /* msgc_cancelled but triggers mispaste code */
    [msgc_offlevel] = 0,       /* should never reach the client */
    [msgc_mute] = 0,           /* should never reach the client */
};

/* these should be the same length */
static const char *const more_text = "--More--";   /* "press space or return" */
static const char *const tab_text  = "--Tab!--";   /* "press Tab" */


/* The message display works via using the concept of "chunks".  A chunk is one
   line high, and can potentially be as wide as the message buffer (but can be
   narrower); a longer message is broken into multiple chunks. At any given
   point, either all the messages, or all but one, are converted into chunk
   form; the last is kept around until we request a key, then chunked depending
   on whether the keypress is for a --More-- (i.e. we need to leave space) or
   for some other reason.

   Chunks conceptually form a coordinate system that scrolls vertically forever
   (positive x to the right, with 0 being the leftmost column, positive y going
   downwards, with arbitrary zero). However, once a chunk scrolls too far off
   the top of the screen (a distance of settings.msghistory), we deallocate
   it. The chunks are stored in a doubly linked list, sorted by increasing y and
   then by increasing x; we keep pointers to the first and last chunk in sort
   order.

   The chunks don't have to be seen in contiguous order (and often won't be due
   to reminder chunks and spacing chunks). However, we only try to keep unseen
   chunks visible to the user while they're actually on the screen; if one
   escapes the screen while it's unseen (say due to the window being resized),
   its unseen status is ignored unless it somehow gets back onto the screen
   again. This case should hardly ever come up (and we can assume that a user
   who's resizing the screen looked at the messages on it before the resize, so
   it doesn't harm anything). */
struct message_chunk;
struct message_chunk {
    char *message;                 /* text of the chunk */
    struct message_chunk *next;    /* next chunk */
    struct message_chunk *prev;    /* previous chunk */
    unsigned x;                    /* x-position of the chunk */
    unsigned y;                    /* y-position of the chunk */
    enum msg_channel channel;      /* message channel for this chunk */
    nh_bool seen;                  /* this chunk was on screen at a keypress */
};

static struct message_chunk *first_chunk = NULL;
static struct message_chunk *last_chunk = NULL;

/* We want to draw the last chunk at a position of ui_flags.msgheight - 1;
   adding this value of y_offset thus gives us the coordinates of a chunk. To
   handle wraparound correctly, we use unsigned arithmetic for everything, with
   the "in range" values being 0 to ui_flags.msgheight - 1 and the "out of
   range" values being everything higher. (In most cases, y_offset will be a
   "negative", very large, number, but that doesn't matter; it'll wrap back
   around when we add y_offset to an appropriately sized value.)

   Coded as a macro because without using gcc extensions, the version with a
   callback function is considerably harder to read. */
#define FOR_EACH_ONSCREEN_CHUNK()                                 \
    const unsigned y_offset = !last_chunk ? 0 :                   \
        (unsigned)ui_flags.msgheight - 1 - last_chunk->y;         \
    struct message_chunk *chunk;                                  \
    for (chunk = last_chunk;                                      \
         chunk && (chunk->y + y_offset < ui_flags.msgheight);     \
         chunk = chunk->prev)


/* We leave one message unchunkified untill just before we request a key (or
   perform a delay, which has similar message timing properties). At that point,
   some, none or all of the message may be fit on the screen. If all of it does,
   then we just chunkify it and move on (taking into account the reason that the
   key was requested; we may need to leave room for a --More-- in the case that
   the server sends us a message pause request). If some of it doesn't, then we
   chunkify as much as will fit beside a --More--, force a --More--, then go
   back to what we were doing (which may in extreme cases involve forcing
   another --More-- because the rest of the message won't fit).  */
static char *pending_message = NULL;
enum msg_channel pending_message_channel;

enum moreforce {mf_nomore, mf_more, mf_tab};

/* Someone pressed Esc recently: automatically dismiss all --More--s. */
static nh_bool stopmore = FALSE;
/* Is more_io currently running? Used for resizing the message window at a
   --More-- prompt.*/
static enum moreforce in_more_io = mf_nomore;

void
discard_message_history(int lines_to_keep)
{
    while (first_chunk && last_chunk->y - first_chunk->y >= lines_to_keep) {
        struct message_chunk *temp = first_chunk->next;
        free(first_chunk->message);

        if (!temp) { /* i.e. first_chunk == last_chunk */
            free(first_chunk);
            first_chunk = NULL;
            last_chunk = NULL;
            /* If we're freeing everything, get rid of the pending message
               too (if any). */
            free(pending_message);
            pending_message = NULL;
            return;
        }
        temp->prev = NULL;
        free(first_chunk);
        first_chunk = temp;
    }
}

/* Returns the appropriate color to use for the given message channel,
   perhaps including CLRFLAG_ constants (the caller can mask those out if it
   doesn't care). */
static int
resolve_channel_color(enum msg_channel msgc)
{
    const int no_forcing = ~(CLRFLAG_FORCEMORE | CLRFLAG_FORCETAB);
    if (msgc == msgc_intrloss_level)
        return channel_color[msgc_intrloss] & no_forcing;
    else if (msgc == msgc_intrgain_level)
        return channel_color[msgc_intrgain] & no_forcing;
    else if (msgc == msgc_fatal_predone)
        return channel_color[msgc_fatal] & no_forcing;
    else if (msgc == msgc_mispaste)
        return channel_color[msgc_cancelled];
    else if (channel_color[msgc] == 0)
        /* something's gone badly wrong here */
        return CLR_GREEN; /* the least common color in the message window */
    else
        return channel_color[msgc];
}

/* The lowest-level message window drawing function. Draws the message window up
   to and including the last chunk, and also renders a --More-- or --Tab!-- if
   requested. Does not do any asking for input, and does not chunkify the
   pending message. Should only be called in a state in which we've left room
   for the --More-- during chunkification, if one is required. */
static void
show_msgwin(enum moreforce more)
{
    wattrset(msgwin, 0);
    werase(msgwin);

    FOR_EACH_ONSCREEN_CHUNK()
    {
        if (!*(chunk->message))
            continue;

        int color = resolve_channel_color(chunk->channel) & 15;
        wattrset(msgwin, curses_color_attr(
                     !chunk->seen ? color :
                     color == CLR_GRAY || color == CLR_WHITE ?
                     CLR_DARK_GRAY : color & 7, 0));
        mvwaddstr(msgwin, chunk->y + y_offset, chunk->x, chunk->message);
    }

    /* Maybe draw a --More--. */
    if (more != mf_nomore) {
        wattrset(msgwin, curses_color_attr(0, 7));
        mvwaddstr(msgwin, ui_flags.msgheight - 1,
                  ui_flags.mapwidth - strlen(more_text),
                  more == mf_more ? more_text : tab_text);
    }

    wnoutrefresh(msgwin);
}


/* Implementation of --More--. Assumes that the rendering has already happened,
   and does the I/O parts. */
static void
more_io(nh_bool block_until_tab)
{
    in_more_io = block_until_tab ? mf_tab : mf_more;

    while (!stopmore || block_until_tab) {
        switch (get_map_key(FALSE, FALSE,
                            block_until_tab ? krc_moretab : krc_more)) {
        case KEY_SIGNAL:
            /* This happens when a watcher is stuck at a --More-- when the
               watchee gives a command. Just move on, so we don't end up behind
               forever; and repeat the signal, because it's still relevant.

               This mechanism is needed even when we don't think we're watching;
               disconnected processes get forced into implicit watch mode if the
               user reconnects before the process times out, and so a process
               that thought it was playing might unexpectely get a signal.

               This can also override block_until_tab; it has to, because a tab
               input by the watchee needs to move on for the watcher too. (Not
               to mention that the watcher might have a tab-block set on an
               action which the watchee doesn't even have a more on.) */
            uncursed_signal_getch();
            stopmore = 1;
            goto more_dismissed;

        case KEY_ESCAPE:
        case '\x1b':
            if (block_until_tab)
                break;
            stopmore = 1;
            goto more_dismissed;

        case ' ':
        case 10:
        case 13:
        case KEY_ENTER:
            if (block_until_tab)
                break;
            goto more_dismissed;

        case 9: /* tab: used to dismiss the most urgent force-mores */
            if (!block_until_tab)
                break;
            goto more_dismissed;
        }
    }
more_dismissed: /* multilevel break out of loop */
    draw_messages_postkey();

    in_more_io = mf_nomore;
}

/* Re-renders the message window, without trying to force a --More--. Used if
   you know it hasn't changed, or in cases like the message window being resized
   due to the screen being resized. This may cause messages at the top of the
   window to be lost. */
void
redraw_messages(void)
{
    show_msgwin(in_more_io);
}

/* Breaks off the first n characters of the pending message, or less if there's
   somewhere convenient to split at, like a space. Returns the first portion of
   the message, leaving the rest of the message (if any) in pending_message. */
static char *
split_pending_message_at(int n)
{
    char *rv;

    /* We didn't have to split it at all. How convenient. */
    if (strlen(pending_message) <= n) {
        rv = pending_message;
        pending_message = NULL;
        return rv;
    }

    /* Find the last space in the region we can split, and split there. */
    char *ptr;
    for (ptr = pending_message + n; ptr > pending_message; ptr--)
        if (*ptr == ' ') {
            /* Find the first space in this run of spaces. */
            char *preptr = ptr;
            while (preptr > pending_message && *preptr == ' ')
                preptr--;
            preptr++;

            /* And the last (this can be different from ptr if there happened to
               be a run of spaces right at pending_message + n). */
            char *postptr = ptr;
            while (*postptr == ' ')
                postptr++;

            /* Our return value will be the same buffer as pending_message, just
               with a '\0' inserted to shorten it. (Potential TODO: realloc rv
               shorter. Most likely it won't produce noticeable memory gains,
               though, unless someone is sending very very long messages.) */
            *preptr = '\0';
            rv = pending_message;

            if (*postptr == '\0')
                /* the message was too long, but not after trimming trailing
                   spaces */
                pending_message = NULL;
            else {
                /* duplicate the portion after the run of spaces back into
                   pending_message */
                pending_message = malloc(strlen(postptr) + 1);
                strcpy(pending_message, postptr);
            }
            return rv;
        }

    /* If we get here, there was a continuous string of nonspaces too wide for
       the requested width. Just break anywhere. As above, pending_message's
       existing memory is used for the return value (by inserting a '\0'), and
       we allocate a new pending_message. */
    rv = pending_message;
    ptr = pending_message + n;
    pending_message = malloc(strlen(ptr) + 1);
    strcpy(pending_message, ptr);
    *ptr = '\0'; /* trims rv down to width n */
    return rv;
}

/* We leave two spaces between chunks, so calculate the width of a chunk plus
   2. Exception: sometimes we use empty chunks to take up a blank line, and
   those don't need padding. */
static int
padded_xright(struct message_chunk *chunk)
{
    if (!*(chunk->message))
        return chunk->x;
    return strlen(chunk->message) + 2 + chunk->x;
}

/* Adds a new chunk to the end of the list of chunks. */
static void
alloc_chunk(char *contents, enum msg_channel channel, int x, int y)
{
    struct message_chunk *new_chunk = malloc(sizeof *new_chunk);
    new_chunk->message = contents;
    new_chunk->next = NULL;
    new_chunk->prev = last_chunk;
    new_chunk->x = x;
    new_chunk->y = y;
    new_chunk->channel = channel;
    new_chunk->seen = channel == msgc_reminder;
    if (last_chunk)
        last_chunk->next = new_chunk;
    last_chunk = new_chunk;
    if (!first_chunk)
        first_chunk = new_chunk;
}

/* If we're currently using more than the given amount of padded width on the
   last message line, add a new blank line below it (via placing a spacing chunk
   at the start of the line). The caller must check that there's room, i.e. no
   seen messages on the top line. */
static void
limit_last_line_x(int max_ok_x)
{
    if (last_chunk && padded_xright(last_chunk) > max_ok_x) {
        char *nullstring = malloc(1);
        *nullstring = '\0';
        alloc_chunk(nullstring, msgc_mute, 0, last_chunk->y + 1);
        last_chunk->seen = TRUE; /* spacing chunks aren't visible */
    }
}

/* Attempts to break off chunks from the start of the pending message, adding
   them to the end of the list of chunks; however, will not scroll an unseen
   message off the screen in the process. If entire_last_line is set, will leave
   one line blank unless the entire message can be chunkfied (so that a caller
   can leave room for a --More-- if and only if the entire message doesn't fit
   without it). Otherwise, might chunkify only part of the message. If
   room_for_more is set, will ensure that the --More-- region in the bottom
   right of the message area is left blank (via leaving a blank line if
   necessary). */
static void
chunkify_pending_message(nh_bool room_for_more, nh_bool entire_last_line)
{
    /* avoid a segfault on very narrow windows */
    if (ui_flags.mapwidth < strlen(more_text))
        room_for_more = FALSE;

    /* Do we have a "spacing chunk" at the start of the last line? If so, we can
       get rid of it safely for the time being (we might add it back later), and
       it's going to prevent us splitting a message on its line and the line
       after (because that would count as both spanning and sharing). However,
       this is pointless if there's no pending message, or if the pending
       message fits onto the line anyway, and potentially harmful because it
       might cause the message window to apparently scroll backwards. */
    if (last_chunk && last_chunk->x == 0 && !*(last_chunk->message) &&
        pending_message && strlen(pending_message) >
        ui_flags.mapwidth - (room_for_more ? strlen(more_text) + 2 : 0)) {
        struct message_chunk *temp = last_chunk->prev;
        free(last_chunk->message);
        free(last_chunk);
        last_chunk = temp;
        if (temp)
            temp->next = NULL;
        else
            first_chunk = NULL;
    }

    int spare_lines = ui_flags.msgheight;
    FOR_EACH_ONSCREEN_CHUNK() /* sets chunk, y_offset */
    {
        /* If the last unseen chunk is on line y, then we have y lines spare
           (if it's on line 0 we have 0 lines spare). */
        if (!chunk->seen)
            spare_lines = chunk->y + y_offset;
    }

    while (pending_message)
    {
        /* Are we allowed to split this message? Don't split it if there's any
           doubt about whether we can actually create a chunk (because then we
           clobber pending_message). Also don't let the message both split and
           share a line.

           Luckily, in all cases, we can determine in advance that either a
           split will definitely have somewhere to put the first part, or a
           split definitely won't help:

           - If we have two spare lines, then we can put the first part on the
             first of those lines without breaking any rules;

           - If we have one spare line and an entire_last_line requirement, then
             by definition we can only use the last line if we fit the entire
             message in; splitting it would occupy the last line with the first
             part, and thus there'd be nowhere else to put the rest;

           - If we have one spare line and no entire_last_line requirement, then
             we can just put the first part of the split onto that spare line;

           - If we have no spare lines, then the message will have to share a
             line, and thus can't also split.

           We calculate how much of the line is usable in the cases that we
           share, split, or do neither. (If we split, and we're using the last
           line for the first half, we'll definitely need room for a
           --More--, because the second half will be offscreen.) */
        nh_bool splitok = spare_lines > (entire_last_line ? 1 : 0);
        int usable_width_unsplit = (spare_lines > 1 || !room_for_more) ?
            ui_flags.mapwidth : ui_flags.mapwidth - strlen(more_text) - 2;
        int usable_width_split = (spare_lines > 1) ?
            ui_flags.mapwidth : ui_flags.mapwidth - strlen(more_text) - 2;
        int usable_width_share = (spare_lines > 0 || !room_for_more) ?
            ui_flags.mapwidth : ui_flags.mapwidth - strlen(more_text) - 2;

        /* Split it, or don't split it and try to use the whole thing if we
           can't. */
        char *firstpart = splitok ?
            split_pending_message_at(usable_width_split) : pending_message;
        if (!splitok)
            pending_message = NULL;

        /*
         * Can we fit this message alongside an existing message? The existing
         * message has to be not yet seen to make that possible.
         *
         * Note that this isn't quite perfect in the following case:
         * |abc  01234567890123456|
         * |012345678901  --More--|
         * (in which the numbers represent usable space). Imagine a 14
         * character message in this (admittedly implausible) situation. We'd
         * have room to fit the entire message on the line above, but we split
         * it at 12 characters in case we had to fit it next to a --More--, and
         * now it doesn't fit on either line because it's two lines high. The
         * result is that we'll get the first half of the message next to the
         * --More--, and the second half after the --More--.
         *
         * For now, we don't worry about this case because a) the result isn't
         * outright wrong (just less efficient than it could be), and b) it
         * requires a message that's 7 characters or less on the previous line,
         * and the game doesn't produce many if any of those.
         */
        if (last_chunk && (!last_chunk->seen || !*(last_chunk->message)) &&
            !pending_message && strlen(firstpart) + padded_xright(last_chunk) <
            usable_width_share) {
            /* We can. */
            alloc_chunk(firstpart, pending_message_channel,
                        padded_xright(last_chunk), last_chunk->y);
        } else if (spare_lines > 0 && strlen(firstpart) <=
                   (pending_message ?
                    usable_width_split : usable_width_unsplit)) {
            /* We can't, but we have a spare line it fits on; use that. */
            alloc_chunk(firstpart, pending_message_channel, 0,
                        last_chunk ? last_chunk->y + 1 : 0);
            spare_lines--;
        } else if (!splitok) {
            /* It doesn't fit; better put it back into pending_message
               and stop trying. */
            pending_message = firstpart;
            break;
        } else
            abort(); /* should be impossible */
    }

    /* We guarantee that we leave space for a --More-- if room_for_more is set.
       However, we might have used the entirety of the last line if there's a
       spare line (on the basis that we could put the --More-- onto the spare
       line). In this case we need to add a blank chunk at the start of the
       next line to force the screen up and leave room. */
    if (room_for_more)
        limit_last_line_x(ui_flags.mapwidth - strlen(more_text));
}

/* Ensures that the message window is in a ready state to read a key (i.e. with
   all messages so far showing). Can force a --More-- if one is required to fit
   all the messages so far onto the screen (which will happen before it returns
   and you can read your key, if it does). After it returns, the message window
   will be up to date. This should be called before any key input that has the
   message window visible.

   The room_for_more argument is used to handle a particularly awkward case: the
   game fills the message area exactly with messages, does a delay, and then
   forces a --More-- as part of the same turn without printing any further
   messages. If it's set to TRUE, the last portion of the message area (where
   the --More-- goes) will be left empty, in case you subsequently need to
   render a --More-- there. If it's set to FALSE, you're promising that after
   this function returns, you will call draw_messages_postkey() before the next
   event that might potentially force a --More--. */
void
draw_messages_prekey(nh_bool room_for_more)
{
    if (pending_message)
        /* Let's try to fit all of this onscreen at once, so we don't have to
           give a --More--. */
        chunkify_pending_message(room_for_more, TRUE);
    if (pending_message)
        /* OK, then, let's try to fit as much onscreen as possible. Leave
           room for our inevitable --More--. */
        chunkify_pending_message(TRUE, FALSE);
    if (pending_message) {
        /* We haven't shown all the messages yet, so we need to force a
           --More--. */
        show_msgwin(mf_more);
        more_io(FALSE);
        /* Now the user's had a chance to mark some chunks as seen, there
           will be more room, so try again. */
        draw_messages_prekey(room_for_more);
        return;
    }

    /* All the messages fit onscreen. It's just time to draw them. */
    show_msgwin(mf_nomore);
}

/* Marks all currently onscreen message chunks as having been read by the
   user, allowing them to scroll off the screen without issue. */
void
draw_messages_postkey(void)
{
    FOR_EACH_ONSCREEN_CHUNK()
    {
        chunk->seen = TRUE;
    }
    if (!in_more_io)
        stopmore = FALSE;

    redraw_messages();
}

/* Ensures that all messages so far can be read by the user, even with the
   bottom message line blank. This will fit them into the last-but-one lines of
   the message area without a --More--, if possible. Otherwise, it will force a
   --More-- using the entire space of the message area, and then scroll messages
   up to leave the last line blank. The intended use is before drawing a prompt
   that covers the bottom line of the message area. */
void
fresh_message_line(void)
{
    /* Get any pending message onto the screen, leaving space for a --More--
       (we'll need to print one if we don't have a spare line at the bottom;
       note that draw_message_prekey will use the entirety of the line before if
       it can, and in preference to splitting a message to make room for a
       --More--). This means that we don't need to handle the pending message
       ourself. */
    draw_messages_prekey(TRUE);

    /* Is the top line in use? Is the bottom line in use? If they both are,
       we'll need to force our --More--. */
    nh_bool top_line_in_use = FALSE;
    FOR_EACH_ONSCREEN_CHUNK()
    {
        if (chunk->y + y_offset == 0 && !chunk->seen)
            top_line_in_use = TRUE;
    }
    if (top_line_in_use /* implies last_chunk != NULL*/ &&
        padded_xright(last_chunk) != 0) {
        /* Yep, both in use. Force our --More--. */
        show_msgwin(mf_more);
        more_io(FALSE);
    }
    /* more_io marks every line on screen (thus at least the top line) as no
       longer in use. So we can now scroll the screen upwards safely; either we
       verified that it was safe, or more_io made it safe. */
    limit_last_line_x(0); /* i.e. require that the whole line is blank */
    redraw_messages();
}

/* Ensures that all messages so far have been read by the user. This is a more
   stringent requirement than on draw_messages_prekey; if there's even one
   unread message, there will be a --More-- even if there is plenty of room for
   more messages. The idea is to call this before the message window gets
   covered up by some other window, so that messages don't get stuck behind
   it. */
void
draw_messages_precover(void)
{
    /* Get all the messages onto the screen, leaving room for a --More--. */
    draw_messages_prekey(TRUE);

    /* Are there any unseen messages? */
    nh_bool any_unseen = FALSE;
    FOR_EACH_ONSCREEN_CHUNK()
    {
        if (!chunk->seen) {
            any_unseen = TRUE;
            break;
        }
    }

    /* Yep, force a --More-- for those too. */
    if (any_unseen) {
        show_msgwin(mf_more);
        more_io(FALSE);
    }
}

/* An even more stringent version of draw_messages_precover; waits for the user
   to dismiss a --More-- even if there are zero unread messages. If the argument
   is set, will use a --Tab!-- instead, that cannot be dismissed in the normal
   way. */
void
force_more(nh_bool require_tab)
{
    /* Get all messages onto the screen. */
    draw_messages_prekey(TRUE);

    /* Do the --More--. */
    show_msgwin(require_tab ? mf_tab : mf_more);
    more_io(require_tab);
}

/* Prints a message onto the screen, and into message history. The code will
   ensure that the user eventually sees the message (e.g. with --More--), unless
   the channel is msgc_reminder (or there are shenanigans with window
   resizing). */
void
curses_print_message(enum msg_channel msgc, const char *msg)
{
    /* Sanity: ignore blank messages */
    if (!*msg)
        return;

    /* Do we want to force a --More--? Or ignore the message altogether? Return
       now if the message is ignored, otherwise remember it for later. */
    int c = resolve_channel_color(msgc);
    if (c & CLRFLAG_HIDE)
        return;

    /* At all times (other than between draw_messages_prekey(FALSE) and
       draw_messages_postkey), we ensure that we have room to place a --More--
       on screen, in case it becomes necessary. We do this via working one
       message behind until such time as key input or delays are required,
       storing the most recent message in pending_message.

       Thus, we need to evict any message that's already there to make room for
       our new one, then store our new message there. We can evict the existing
       message using draw_messages_prekey(TRUE), which will leave space for a
       hypothetical --More-- (and there won't be any wasted space as a result;
       if the message area is full enough that the amount of space left for a
       --More-- becomes relevant, then there won't be enough room for this
       message unless it's very short, and so the --More-- will actually be
       required). */
    draw_messages_prekey(TRUE); /* forces pending_message to NULL */

    /* Now just take a copy of our message and store it in pending_message. */
    pending_message = malloc(strlen(msg) + 1);
    strcpy(pending_message, msg);
    pending_message_channel = msgc;

    /* Finally, do any More-forcing. */
    if (c & CLRFLAG_FORCETAB)
        force_more(TRUE);
    else if (c & CLRFLAG_FORCEMORE)
        force_more(FALSE);
}

/* Prints a message to the message history that can be erased later (assuming
   no messages are printed in the meantime). */
void
curses_temp_message(const char *msg)
{
    curses_print_message(msgc_curprompt, msg);
}

/* Clear the temporary messages from the buffer (via looking for the
   msgc_curprompt chunks they produce). Assumes that they are contiguous at the
   end (possibly intermixed with empty spacing chunks). */
void
curses_clear_temp_messages(void)
{
    while (last_chunk && (last_chunk->channel == msgc_curprompt ||
                          !*(last_chunk->message))) {
        struct message_chunk *temp = last_chunk->prev;
        free(last_chunk->message);
        free(last_chunk);
        if (temp)
            temp->next = NULL;
        else
            first_chunk = NULL;
        last_chunk = temp;
    }
}

/* Displays the message history. */
void
doprev_message(void)
{
    /* TODO */
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
