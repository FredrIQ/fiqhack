/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <signal.h>
#include <sys/stat.h>


static void signal_quit(int ignored)
{
    /* the main event loop tests this flag and will exit in response */
    termination_flag = 1;
}


void setup_signals(void)
{
    struct sigaction quitaction;
    struct sigaction ignoreaction;
    sigset_t set;
    
    sigfillset(&set);
    
    quitaction.sa_handler = signal_quit;
    quitaction.sa_mask = set;
    quitaction.sa_flags = 0;
    ignoreaction.sa_handler = SIG_IGN;
    
    /* terminate safely in response to SIGINT and SIGTERM */
    sigaction(SIGINT, &quitaction, NULL);
    sigaction(SIGTERM, &quitaction, NULL);
    sigaction(SIGPIPE, &ignoreaction, NULL);
}


static int create_dir(const char *path)
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


int init_workdir(void)
{
    char dirbuf[1024];
    
    if (!create_dir(settings.workdir))
	return FALSE;
    
    sprintf(dirbuf, "%s/completed/", settings.workdir);
    if (!create_dir(dirbuf))
	return FALSE;
    
    sprintf(dirbuf, "%s/save/", settings.workdir);
    if (!create_dir(dirbuf))
	return FALSE;
    
    return TRUE;
}


int remove_unix_socket(void)
{
    struct stat statbuf;
    int ret;
    
    if (!settings.bind_addr_unix.sun_family)
	return TRUE;
    
    ret = stat(settings.bind_addr_unix.sun_path, &statbuf);
    if (ret == -1)
	/* file doesn't exist */
	return TRUE;
    
    if (!S_ISSOCK(statbuf.st_mode)) {
	log_msg("Error: %s already exists and is not a socket",
		settings.bind_addr_unix.sun_path);
	return FALSE;
    }
    
    return unlink(settings.bind_addr_unix.sun_path) == 0;
}

/* miscsetup.c */
