/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <arpa/inet.h>

struct settings settings;
int termination_flag;


static void print_usage(const char *progname);
static int read_parameters(int argc, char *argv[], char **conffile, int *request_kill);


int main(int argc, char *argv[])
{
    char *conffile = NULL;
    int request_kill = 0;
    
    if (!read_parameters(argc, argv, &conffile, &request_kill)) {
	print_usage(argv[0]);
	return 1;
    }
    
    /* read the config file; conffile = NULL means use the default. */
    if (!read_config(conffile))
	return 1; /* error reading the config file */
    setup_defaults();
    
    if (request_kill) {
	kill_server();
	return 0;
    }
    
    setup_signals();
    
    /* Init files and directories.
     * Start logging last, so that the log is only created if startup succeeds. */
    if (!init_workdir() ||
	(!settings.nodaemon && !create_pidfile()) ||
	!init_database() ||
	!check_database() ||
	!begin_logging())
	return 1;
    
    if (!settings.nodaemon)
    	daemon(0, 0);

    /* give this process and its children an own process group id for kill */
    setpgid(0, 0);
    report_startup();
    
    runserver();
    
    end_logging();
    close_database();
    free_config();
    
    return 0;
}


static void print_usage(const char *progname)
{
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("  -4 <ipv4 addr>   The ipv4 host address which should be used.\n");
    printf("                     Default: bind to all v4 host addresses.\n");
    printf("  -6 <ipv6 addr>   The ipv6 host address which should be used.\n");
    printf("                     Default: bind to all v6 host addresses.\n");
    printf("  -c <file name>   Config file to use insted of the default.\n");
    printf("                     Default: \"" DEFAULT_CONFIG_FILE "\"\n");
    printf("  -d <\"v4\"|\"v6\">   -d v4: disable ipv4; -d v6 disable ipv6.\n");
    printf("  -k               Kill a previously started server daemon.\n");
    printf("                     The -p parameter and the config file will be\n");
    printf("                     used to find the pid file of the server to kill.\n");
    printf("  -l <file name>   Alternate log file name. Default: \"" DEFAULT_LOG_FILE "\"\n");
    printf("  -n               Don't detach from the terminal.\n");
    printf("                     If this parameter is set, no pidfile is created.\n");
    printf("  -P <number>      Port number. Default: %d\n", DEFAULT_PORT);
    printf("  -p <file name>   Name of the file used to store the pid of a running\n");
    printf("                     server daemon.\n");
    printf("  -t <seconds>     Client timeout in seconds. Default: %d.\n", DEFAULT_CLIENT_TIMEOUT);
    printf("  -w <directory>   Working directory which will store user details,\n");
    printf("                     saved games, high score etc.\n");
    printf("\n");
    printf("  Database connection settings:\n");
    printf("  -H <string>      Hostname, ip address (v4 or v6) or unix socket name\n");
    printf("                     of the PostgreSQL database server.\n");
    printf("  -o <string>      Port number to connect to at the PostgreSQL server, or\n");
    printf("                     socket file name extension for Unix-domain connections.\n");
    printf("  -u <string>      PostgreSQL user name to connect as.\n");
    printf("  -a <string>      Password for the given user name.\n");
    printf("  -D <string>      Database name. Default: the same as the user name.\n");
    printf("\n");
    printf("  -h               Show this message.\n");
}


static int read_parameters(int argc, char *argv[], char **conffile, int *request_kill)
{
    int opt;
    
    while ((opt = getopt(argc, argv, "4:6:a:c:D:d:H:hk:l:no:P:p:t:u:w:")) != -1) {
	switch (opt) {
	    case '4': /* bind address */
		if (!parse_ip_addr(optarg, (struct sockaddr*)&settings.bind_addr_4, TRUE)) {
		    fprintf(stderr, "Error: \"%s\" was not recognized as an ipv4 address.", optarg);
		    return FALSE;
		}
		break;
		
	    case '6': /* bind address */
		if (!parse_ip_addr(optarg, (struct sockaddr*)&settings.bind_addr_6, FALSE)) {
		    fprintf(stderr, "Error: \"%s\" was not recognized as an ipv6 address.", optarg);
		    return FALSE;
		}
		break;
		
	    case 'a':
		settings.dbpass = strdup(optarg);
		break;
		
	    case 'c': /* config file */
		*conffile = optarg;
		break;
		
	    case 'D':
		settings.dbname = strdup(optarg);
		break;
		
	    case 'd': /* disable ipv4 / ipv6 */
		if (!strcmp(optarg, "v4"))
		    settings.disable_ipv4 = TRUE;
		else if (!strcmp(optarg, "v6"))
		    settings.disable_ipv6 = TRUE;
		else {
		    fprintf(stderr, "Error: \"v4"" or \"v6\" expected after parameter -d.");
		    return FALSE;
		}
		break;
		
	    case 'H':
		settings.dbhost = strdup(optarg);
		break;
		
	    case 'k': /* kill server */
		*request_kill = TRUE;
		break;
		
	    case 'l': /* log file */
		settings.logfile = strdup(optarg);
		break;
		
	    case 'n': /* don't daemonize */
		settings.nodaemon = TRUE;
		break;
		
	    case 'o':
		settings.dbport = strdup(optarg);
		break;
		
	    case 'P': /* port number */
		settings.port = atoi(optarg);
		if (settings.port < 1 || settings.port > 65535) {
		    fprintf(stderr, "Error: Port %d is outside the range of valid port numbers [1-65535].\n", settings.port);
		    return FALSE;
		}
		break;
		
	    case 'p': /* pid file */
		settings.pidfile = strdup(optarg);
		break;
		
	    case 't':
		settings.client_timeout = atoi(optarg);
		if (settings.client_timeout <= 30 ||
		    settings.client_timeout > (24 * 60 * 60)) {
		    fprintf(stderr, "Error: Silly value %s given as the client timeout.\n",
			    optarg);
		    return FALSE;
		}
		break;
		
	    case 'u':
		settings.dbuser = strdup(optarg);
		break;
		
	    case 'w': /* work dir */
		settings.workdir = strdup(optarg);
		break;
		
	    case 'h': /* help */
		return FALSE;
		
	    default: /* unrecognized */
		fprintf(stderr, "Error: unknown option '%c'.\n", optopt);
		return FALSE;
	}
    }
    
    return TRUE;
}


/* srvmain.c */
