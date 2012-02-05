/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <time.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/time.h>

static FILE *logfile;
static int startup_pid;


void log_msg(const char *fmt, ...)
{
    char msgbuf[512], timestamp[32], *last;
    struct tm *tm_local;
    struct timeval tv;
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
    va_end(args);

    /* eliminate spaces and newlines at the end of msgbuf.
     * There should be only exactly one newline, which gets added later. */
    last = &msgbuf[strlen(msgbuf) - 1];
    while (isspace(*last))
	*last-- ='\0';

    /* make a timestamp like "2011-11-30 18:45:59" */
    gettimeofday(&tv, NULL);
    tm_local = localtime(&tv.tv_sec);
    if (!tm_local ||
	!strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_local))
	strcpy(timestamp, "???");
    fprintf(logfile, "%s.%06ld [%d] %s\n", timestamp, tv.tv_usec, getpid(), msgbuf);
    fflush(logfile);
    
    if (settings.nodaemon)
	/* stdout is still open, lets print some stuff */
	fprintf(stdout, "%s.%06ld [%d] %s\n", timestamp, tv.tv_usec, getpid(), msgbuf);
}


int begin_logging(void)
{
    logfile = fopen(settings.logfile, "a");
    if (!logfile) {
	fprintf(stderr, "Error opening/creating %s: %s.\n", settings.logfile, strerror(errno));
	return FALSE;
    }
    
    return TRUE;
}


const char *addr2str(const void *sockaddr)
{
    static char buf[INET6_ADDRSTRLEN+1];
    const struct sockaddr_in *sa4 = sockaddr;
    const struct sockaddr_in6 *sa6 = sockaddr;
    
    switch (sa4->sin_family) {
	case AF_UNIX:
	    return settings.bind_addr_unix.sun_path;

	case AF_INET:
	    inet_ntop(AF_INET, &sa4->sin_addr, buf, INET6_ADDRSTRLEN);
	    return buf;

	case AF_INET6:
	    inet_ntop(AF_INET6, &sa6->sin6_addr, buf, INET6_ADDRSTRLEN);
	    return buf;
    }

    return "(none)";
}


/* Print startup messages into the logfile.
 * This function is not part of begin_logging() because begin_logging is called
 * before the process detaches from the terminal (via daemon()), which involves
 * a fork() and changes the pid.
 * This function can be alled after detaching so that the logged pid will match
 * the actual pid. */
void report_startup(void)
{
    log_msg("----- Server startup. -----");
    
    log_msg("current configuration:");
    log_msg("  logfile = %s", settings.logfile);
    log_msg("  workdir = %s", settings.workdir);
    if (settings.nodaemon)
	log_msg("  nodaemon = true");
    else
	log_msg("  pidfile = %s", settings.pidfile);
    
    if (settings.disable_ipv4)
	log_msg("  disable_family = v4");
    else if (settings.disable_ipv6)
	log_msg("  disable_family = v6");
	
    log_msg("  ipv4addr = %s", addr2str(&settings.bind_addr_4));
    log_msg("  ipv6addr = %s", addr2str(&settings.bind_addr_6));
    log_msg("  unixsocket = %s", addr2str(&settings.bind_addr_unix));
    log_msg("  port = %d", settings.port);
    log_msg("  client_timeout = %d", settings.client_timeout);
    
    /* database settings */
    log_msg("  dbhost = %s", settings.dbhost ? settings.dbhost : "(not set)");
    log_msg("  dbport = %s", settings.dbport ? settings.dbport : "(not set)");
    log_msg("  dbuser = %s", settings.dbuser ? settings.dbuser : "(not set)");
    log_msg("  dbpass = %s", settings.dbpass ? "(not shown)" : "(not set)");
    log_msg("  dbname = %s", settings.dbname ? settings.dbname : "(not set)");
    
    startup_pid = getpid();
}


void end_logging(void)
{
    if (startup_pid == getpid())
	log_msg("----- Server shutdown. -----");
    fclose(logfile);
    logfile = NULL;
}

/* log.c */
