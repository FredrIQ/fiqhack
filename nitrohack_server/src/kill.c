/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <signal.h>

static int read_pid(void)
{
    int pid;
    FILE *pidfile;
    
    pidfile = fopen(settings.pidfile, "r");
    if (!pidfile)
	return 0;
    
    fscanf(pidfile, "%d", &pid);
    if (!pid)
	return 0;
    
    /* kill with sig=0 only performs error checking without actually sending a
     * signal. Errno == ESRCH tells us that there is no process with the given pid */
    if (kill(pid, 0) == -1 && errno == ESRCH)
	return 0;
    
    return pid;
}


int create_pidfile(void)
{
    int pid;
    FILE *pidfile;
    
    if ( (pid = read_pid()) ) {
	fprintf(stderr, "Error: The server is already running as pid %d.", pid);
	return FALSE;
    }
    
    pidfile = fopen(settings.pidfile, "w");
    if (!pidfile) {
	fprintf(stderr, "Error: Could not create pid files %s: %s.\n",
		settings.pidfile, strerror(errno));
	return FALSE;
    }
    
    fprintf(pidfile, "%d\n", getpid());
    fclose(pidfile);
    
    return TRUE;
}


void kill_server(void)
{
    int err = 0;
    int pid = read_pid();
    
    if (!pid) {
	printf("There doesn't seem to be any NH server to kill.  Done.\n");
	return;
    }
    
    if (kill(pid, SIGTERM) == -1) {
	fprintf(stderr, "Error sending SIGTERM to pid %d: %s\n", pid, strerror(errno));
	err++;
    }
    
    if (kill(-pid, SIGTERM) == -1) {
	fprintf(stderr, "Error sending SIGTERM to pgid %d: %s\n", pid, strerror(errno));
	err++;
    }
    
    if (!err)
	printf("Signal sent.\n");
}
