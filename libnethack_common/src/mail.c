/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-05 */
/* Copyright (c) Fredrik Ljungdahl, 2018. */
/* NetHack may be freely redistributed.  See license for details. */

#include "mail.h"
#include "nethack_types.h"
#include <stdlib.h>
#include <string.h>
#ifdef UNIX
# include <unistd.h>
# include <sys/types.h>
#endif

#ifndef MAILBOXENVVAR
/* Server admins: you can use -DMAILBOXENVVAR in CFLAGS to set the name of this
   environment variable.  The game will then check for the variable you specify
   in the environment at runtime to know where to look for the mailbox file. */
# define MAILBOXENVVAR "NHMAILBOX"
#endif

# define MAILERROR(str)                         \
    do {                                        \
        if (error) {                            \
            strncpy(error, (str), BUFSZ);       \
            error[BUFSZ - 1] = '\0';            \
        }                                       \
        return NULL;                            \
    } while (0)

/* Returns mail filename, or NULL if we encounter an error. If error isn't NULL,
   it is populated with an error message.
   error is assumed to be of size BUFSZ. */
const char *
mail_filename(char *error)
{
#ifndef UNIX
    MAILERROR("Mail isn't available on this operating system.");
#else
    if (getgid() != getegid() || getuid() != geteuid())
        MAILERROR("You're not allowed to send mail in this environment.");

    const char *box = getenv(MAILBOXENVVAR);
    if (!box)
        MAILERROR("Mail is disabled in this installation.");

    return box;
#endif
}
