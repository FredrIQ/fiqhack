/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-18 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#ifdef WIN32
# include "extern.h"
void
checkformail(void)
{
    return;
}
#else
# include <stdio.h>
# include <string.h>
# include <fcntl.h>
# include "extern.h"

# ifndef MAILBOXENVVAR
/* Server admins: you can use -DMAILBOXENVVAR in CFLAGS to set the name of this
   environment variable.  The game will then check for the variable you specify
   in the environment at runtime to know where to look for the mailbox file. */
#  define MAILBOXENVVAR "NHMAILBOX"
# endif

static char prev_who[PL_NSIZ] = {0};
static char prev_text[BUFSZ] = {0};

static int mailhashname(const char *str);
static void delivermail(const char *from, const char *message);

static void
delivermail(const char *from, const char *message)
{
    pline(msgc_mail, "Mail from %s: %s", from, message);
}

void
checkformail(void)
{
    if (program_state.followmode != FM_PLAY &&
        program_state.followmode != FM_WATCH)
        return;

    char *box, *msg;
    FILE* mb;
    char curline[102];
    struct flock fl = { 0 };

    box = nh_getenv(MAILBOXENVVAR);
    if (!box)
        return;

    mb = fopen(box, "r");
    if (!mb)
        return;

    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    /* Allow this call to block. */
    if (fcntl (fileno (mb), F_SETLKW, &fl) == -1)
        return;

    boolean first = TRUE;
    while (fgets(curline, 102, mb) != NULL) {
        const char *who;
        const char *thetext;

        fl.l_type = F_UNLCK;
        fcntl (fileno(mb), F_UNLCK, &fl);

        msg = strchr(curline, ':');
        if (!msg)
            return;

        *msg = '\0';
        msg++;
        who = msgprintf("%s", curline);

        if (!flags.servermail) {
# ifdef MAILOVERRIDE
            if (strcmpi(who, MAILOVERRIDE))
# endif
                return;
        }

        msg[strlen(msg) - 1] = '\0'; /* kill newline */
        thetext = msgprintf("%s", msg);
        if (first) {
            first = FALSE;
            if (prev_who[0] && !strncmp(who, prev_who, PL_NSIZ) &&
                prev_text[0] && !strncmp(thetext, prev_text, BUFSZ)) {
                fclose(mb);
                return;
            }

            strncpy(prev_who, who, PL_NSIZ);
            strncpy(prev_text, thetext, BUFSZ);
        }
        delivermail(who, thetext);

        fl.l_type = F_RDLCK;
        fcntl(fileno(mb), F_SETLKW, &fl);
    }

    fl.l_type = F_UNLCK;
    fcntl(fileno(mb), F_UNLCK, &fl);

    fclose(mb);

    if (program_state.followmode == FM_PLAY)
        unlink(box);
    return;
}

#endif
