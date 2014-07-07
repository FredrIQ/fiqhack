/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-07-07 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <time.h>
#include <ctype.h>
#include <sys/time.h>

static FILE *logfile;


void
log_msg(const char *fmt, ...)
{
    char msgbuf[512], timestamp[32], *last;
    struct tm *tm_local;
    struct timeval tv;
    va_list args;

    va_start(args, fmt);
    vsnprintf(msgbuf, sizeof (msgbuf), fmt, args);
    va_end(args);

    /* eliminate spaces and newlines at the end of msgbuf. There should be only 
       exactly one newline, which gets added later. */
    last = &msgbuf[strlen(msgbuf) - 1];
    while (isspace(*last))
        *last-- = '\0';

    /* make a timestamp like "2011-11-30 18:45:59" */
    gettimeofday(&tv, NULL);
    tm_local = localtime(&tv.tv_sec);
    if (!tm_local ||
        !strftime(timestamp, sizeof (timestamp), "%Y-%m-%d %H:%M:%S", tm_local))
        strcpy(timestamp, "???");
    fprintf(logfile, "%s.%06ld [%d] %s\n", timestamp, tv.tv_usec, getpid(),
            msgbuf);
    fflush(logfile);
}


int
begin_logging(void)
{
    logfile = fopen(settings.logfile, "a");
    if (!logfile) {
        fprintf(stderr, "Error opening/creating %s: %s.\n", settings.logfile,
                strerror(errno));
        return FALSE;
    }

    return TRUE;
}

const char *
addr2str(const void *sockaddr)
{
    static char buf[INET6_ADDRSTRLEN + 1];
    const struct sockaddr_in *sa4 = sockaddr;
    const struct sockaddr_in6 *sa6 = sockaddr;

    switch (sa4->sin_family) {
    case AF_INET:
        inet_ntop(AF_INET, &sa4->sin_addr, buf, INET6_ADDRSTRLEN);
        return buf;

    case AF_INET6:
        inet_ntop(AF_INET6, &sa6->sin6_addr, buf, INET6_ADDRSTRLEN);
        return buf;
    }

    return "(none)";
}

void
end_logging(void)
{
    fclose(logfile);
    logfile = NULL;
}

/* log.c */
