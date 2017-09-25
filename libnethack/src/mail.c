/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Jonadab the Unsightly One, 2015-09-30 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nethack_types.h"

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

static int mailhashname(const char *str);
static void delivermail(const char *from, const char *message);

static const char *const maildelivery[] = {
    "You feel as much as hear the booming deep bass rumble of %s belting out, \"%s\"\n",
    "You can clearly recall that %s once told you, \"%s\"\n",
    "In your imagination you can almost hear the voice of %s saying, \"%s\"\n",
    "Whither cometh this strange revelation?  Could it be from %s?  Thou art uncertain.  But lo, it sayeth, \"%s\"\n",
    "An airy, breathless whisper, scented intriguingly with lavendar, arrives fluttering on a light draft, delivering the words of %s to you:  \"%s\"\n",
    "Disrupting your game (and shattering the fourth wall in the process), %s says \"%s\"\n",
    "Your mind clouds over as your focus shifts to the voice of %s saying, \"%s\"\n",
    "Buried deep in the core of your consciousness is a nagging fixation on a hazy memory of something %s may once have said: \"%s\"\n",
    "A screaming banshee appears, wailing at the top of her lungs, delivering a message ostensibly from %s: \"%s\"\n",
    "No one but %s can be saying \"%s\"\n",
    "You can almost make out a barely-audible high-pitched whine.  It sounds a little like %s, whimpering, \"%s\"\n",
    "You summon all the strength of your considerable mental powers and deliver unto %s a stunning rebuke.  The only answer that comes back is a feeble \"%s\"\n",
    "Emblazoned upon your mind, like a brand on the hide of a cow but glowing like a hot iron, the hideous scrawled handwriting of %s says this: \"%s\"\n",
    "\"You must never forget,\" %s always reminds you, \"%s\"\n",
    "You have a message from %s.  \"%s\"\n",
    "Whether it is true, or whether it is untrue, you cannot say; but surely this is what %s is telling you:  \"%s\"\n",
};

static int
mailhashname(const char *str)
{
    /* This is only intended to provide variety and interest.
       It would not be useful in any security-related context.
       Mail from any given user will have consistent delivery. */
    int i;
    int mdsize = SIZE(maildelivery);
    int len = strlen(str);
    int sum = 0;
    for (i = 0; i < (int) len; i++) {
        char c = str[i];
        sum = ((int) (sum + (int) c)) % mdsize;
    }
    return sum;
}

static void
delivermail(const char *from, const char *message)
{
    pline(msgc_mail, maildelivery[mailhashname(from)], from, message);
}

void
checkformail(void)
{
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
        delivermail(who, thetext);

        fl.l_type = F_RDLCK;
        fcntl(fileno(mb), F_SETLKW, &fl);
    }

    fl.l_type = F_UNLCK;
    fcntl(fileno(mb), F_UNLCK, &fl);

    fclose(mb);
    unlink(box);
    return;
}

#endif
