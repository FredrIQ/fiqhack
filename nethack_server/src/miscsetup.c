/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <signal.h>

int sigsegv_flag;

static void signal_quit(int ignored)
{
    /* the main event loop tests this flag and will exit in response */
    termination_flag = 1;
}


static void signal_usr2(int ignored)
{
    char filename[1024], databuf[8192];
    struct stat statbuf;
    int ret, fd;
    
    sprintf(filename, "%s/message", settings.workdir);
    ret = stat(filename, &statbuf);
    if (ret == -1) {
	if (!user_info.uid) /* only show the error once from the main process */
	    log_msg("Failed to read the message file %s: %s", filename, strerror(errno));
	return;
    }
    
    if (!user_info.uid) {
	log_msg("Message sent.");
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
    
    srv_display_buffer(databuf, FALSE);
}


static void signal_segv(int ignored)
{
    sigsegv_flag++;
    log_msg("BUG: caught SIGSEGV! Exit.");
    if (user_info.uid)
	exit_client("Fatal: Programming error on the server. Sorry about that.");
    exit(1);
}


void setup_signals(void)
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
    memset(&ignoreaction, 0, sizeof(struct sigaction));
    ignoreaction.sa_handler = SIG_IGN;
    
    /* terminate safely in response to SIGINT and SIGTERM */
    sigaction(SIGINT, &quitaction, NULL);
    sigaction(SIGTERM, &quitaction, NULL);
    
    /* SIGUSR2 sends a message to connected clients */
    sigaction(SIGUSR2, &usr2action, NULL);
    sigaction(SIGUSR1, &usr2action, NULL); /* extra */

    /* catch SIGSEGV to log an error message before exiting */
    sigaction(SIGSEGV, &segvaction, NULL);
    
    /* don't need SIGPIPE, all return values from read+write are checked */
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
