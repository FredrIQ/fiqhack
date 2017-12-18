/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-18 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

#ifdef WIN32
void
sendmail(void)
{
    curses_print_message(player.moves, msgc_failcurse,
                         "Mail isn't available on this operating system.");
    return;
}
#else
# include <stdio.h>
# include <string.h>
# include <fcntl.h>

# ifndef MAILBOXENVVAR
/* Server admins: you can use -DMAILBOXENVVAR in CFLAGS to set the name of this
   environment variable.  The game will then check for the variable you specify
   in the environment at runtime to know where to look for the mailbox file. */
#  define MAILBOXENVVAR "NHMAILBOX"
# endif

static void
mailstr_callback(const char *str, void *vmsg)
{
    char *msg = vmsg;
    strcpy(msg, str);
}

void
sendmail(void)
{
    if (ui_flags.current_followmode != FM_WATCH) {
        curses_print_message(player.moves, msgc_failcurse,
                             "Mail can only be sent while watching.");
        return;
    }

    char *box;
    FILE* mb;

    box = getenv(MAILBOXENVVAR);
    if (!box) {
        curses_print_message(player.moves, msgc_failcurse,
                             "Mail is disabled in this installation.");
        return;
    }

    const char *who = getenv("NH4WATCHER");
    if (!who || !*who) {
        curses_print_message(player.moves, msgc_failcurse,
                             "You need to be logged in to send mail!");
        return;
    }

    char msg[BUFSZ] = {0};
    curses_getline("What do you want to send?", &msg, mailstr_callback);
    if (!msg[0] || msg[0] == '\033')
        return;

    mb = fopen(box, "a");
    if (!mb) {
        curses_print_message(player.moves, msgc_failcurse,
                             "Error sending mail.");
        return;
    }

    fprintf(mb, "%s:%s\n", who, msg);
    fclose(mb);

    curses_print_message(player.moves, msgc_actionok, "Mail sent!");
}

#endif
