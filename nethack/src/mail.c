/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "mail.h"
#include "nhcurses.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

static void
mailstr_callback(const char *str, void *vmsg)
{
    char *msg = vmsg;
    strcpy(msg, str);
}

const char *
watcher_username(void)
{
    const char *who = getenv("NH4WATCHER");
    if (!who || !*who)
        return NULL;
    return who;
}

void
sendmail(void)
{
    if (ui_flags.current_followmode != FM_WATCH &&
        ui_flags.current_followmode != FM_PLAY) {
        curses_print_message(msgc_cancelled,
                             "Mail can't be sent while replaying.");
        return;
    }

    char error[BUFSZ];
    const char *box = mail_filename(error);
    if (!box) {
        curses_print_message(msgc_cancelled, error);
        return;
    }

    FILE* mb;
    const char *who = watcher_username();
    if (!who) {
        curses_print_message(msgc_cancelled,
                             "You need to be logged in to send mail!");
        return;
    }

    char msg[BUFSZ] = {0};
    curses_getline("What do you want to send?", &msg, mailstr_callback);
    if (!msg[0] || msg[0] == '\033')
        return;

    mb = fopen(box, "a");
    if (!mb) {
        curses_print_message(msgc_cancelled,
                             "Error sending mail.");
        return;
    }

    fprintf(mb, "%s:%s\n", who, msg);
    fclose(mb);

    curses_print_message(msgc_actionok, "Mail sent!");
}
