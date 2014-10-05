/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-07-07 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <signal.h>

int sigsegv_flag;

static void
signal_quit(int ignored)
{
    /* the main event loop tests this flag and will exit in response */
    termination_flag = 1;
}


static void
signal_usr2(int ignored)
{
    char filename[1024], databuf[8192];
    struct stat statbuf;
    int ret, fd;

    snprintf(filename, sizeof(filename), "%s/message", settings.workdir);
    ret = stat(filename, &statbuf);
    if (ret == -1) {
        log_msg("Failed to read the message file %s: %s", filename,
                strerror(errno));
        return;
    }

    fd = open(filename, O_RDONLY);
    ret = read(fd, databuf, 8191);
    close(fd);
    if (ret <= 0) {
        log_msg("No data read from %s. No message will be sent.");
        return;
    }
    if (ret == 8191)
        log_msg("Large message file found. Only the first 8kb will be sent.");
    databuf[ret] = '\0';

    log_msg("Message sent.");
    srv_display_buffer(databuf, FALSE);
}


static void
signal_segv(int ignored)
{
    sigsegv_flag++;
    log_msg("BUG: caught SIGSEGV! Exit.");
    if (user_info.uid)
        exit_client("Fatal: Programming error on the server. Sorry about that.",
                    SIGSEGV);

    /* Die via recursive SIGSEGV, so as to leave a useful core dump. */
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}


void
setup_signals(void)
{
    struct sigaction quitaction;
    struct sigaction usr2action;
    struct sigaction segvaction;
    struct sigaction ignoreaction;
    sigset_t set;

    sigfillset(&set);

    quitaction.sa_handler = signal_quit;
    quitaction.sa_mask = set;
    quitaction.sa_flags = 0;
    usr2action.sa_handler = signal_usr2;
    usr2action.sa_mask = set;
    usr2action.sa_flags = 0;
    segvaction.sa_handler = signal_segv;
    segvaction.sa_mask = set;
    segvaction.sa_flags = 0;
    memset(&ignoreaction, 0, sizeof (struct sigaction));
    ignoreaction.sa_handler = SIG_IGN;

    /* terminate safely in response to SIGINT and SIGTERM */
    sigaction(SIGINT, &quitaction, NULL);
    sigaction(SIGTERM, &quitaction, NULL);

    /* SIGUSR2 sends a message to connected clients */
    sigaction(SIGUSR2, &usr2action, NULL);
    sigaction(SIGUSR1, &usr2action, NULL);      /* extra */

    /* catch SIGSEGV to log an error message before exiting */
    sigaction(SIGSEGV, &segvaction, NULL);

    /* don't need SIGPIPE, all return values from read+write are checked */
    sigaction(SIGPIPE, &ignoreaction, NULL);
}


static int
create_dir(const char *path)
{
    int ret;

    ret = mkdir(path, 0700);
    if (ret == -1 && errno != EEXIST) {
        fprintf(stderr, "Error: Could not create work directory %s: %s.\n",
                path, strerror(errno));
        return FALSE;
    }
    return TRUE;
}


int
init_workdir(void)
{
    char dirbuf[1024];

    if (!create_dir(settings.workdir))
        return FALSE;

    snprintf(dirbuf, sizeof(dirbuf), "%s/completed/", settings.workdir);
    if (!create_dir(dirbuf))
        return FALSE;

    snprintf(dirbuf, sizeof(dirbuf), "%s/save/", settings.workdir);
    if (!create_dir(dirbuf))
        return FALSE;

    return TRUE;
}

/* miscsetup.c */
