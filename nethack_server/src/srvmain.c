/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

/* For daemon */
#ifndef _BSD_SOURCE
# define _BSD_SOURCE
#endif

#include "nhserver.h"

struct settings settings;
int termination_flag;


static void print_usage(const char *progname);
static int read_parameters(int argc, char *argv[], char **conffile,
                           int *request_kill, int *show_message);


int
main(int argc, char *argv[])
{
    char *conffile = NULL;
    int request_kill = 0, show_message = 0;

    if (!read_parameters(argc, argv, &conffile, &request_kill, &show_message)) {
        print_usage(argv[0]);
        return 1;
    }

    /* read the config file; conffile = NULL means use the default. */
    if (!read_config(conffile))
        return 1;       /* error reading the config file */
    setup_defaults();

    if (request_kill) {
        fprintf(stderr, "There is no longer a centralized server daemon.\n");
        fprintf(stderr, "Send SIGTERM to the server processes manually.\n");
        return 0;
    }

    if (show_message) {
        fprintf(stderr, "There is no longer a centralized server daemon.\n");
        fprintf(stderr, "Send SIGUSR2 to the server processes manually.\n");
        return 0;
    }

    setup_signals();

    /* Init files and directories. Start logging last, so that the log is only
       created if startup succeeds. */
    if (!init_workdir() || !init_database() || !check_database() ||
        !begin_logging())
        return 1;

    log_msg("new process spawned");

    runserver(); /* does not return */
}

static void
print_usage(const char *progname)
{
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("  -c <file name>   Config file to use insted of the default.\n");
    printf("  -l <file name>   Alternate log file name.\n");
    printf("  -t <seconds>     Client timeout in seconds. Default: %d.\n",
           DEFAULT_CLIENT_TIMEOUT);
    printf("  -w <directory>   Working directory which will store user\n");
    printf("                     details, saved games, high score etc.\n");
    printf("\n");
    printf("  Database connection settings:\n");
    printf("  -H <string>      Hostname, ip address (v4 or v6) or unix socket\n");
    printf("                     name of the PostgreSQL database server.\n");
    printf("  -o <string>      Port number to connect to at the PostgreSQL\n");
    printf("                     server, or socket file name extension for\n");
    printf("                     Unix-domain connections.\n");
    printf("  -u <string>      PostgreSQL user name to connect as.\n");
    printf("  -a <string>      Password for the given user name.\n");
    printf("  -D <string>      Database name. Default: the same as the user\n");
    printf("                     name.\n");
    printf("\n");
    printf("  -h               Show this message.\n");
}


static int
read_parameters(int argc, char *argv[], char **conffile, int *request_kill,
                int *show_message)
{
    int opt;

    while ((opt =
            getopt(argc, argv, "a:c:D:H:kl:mo:t:u:w:")) != -1) {
        switch (opt) {
        case 'a':
            settings.dbpass = strdup(optarg);
            break;

        case 'c':      /* config file */
            *conffile = optarg;
            break;

        case 'D':
            settings.dbname = strdup(optarg);
            break;

        case 'H':
            settings.dbhost = strdup(optarg);
            break;

        case 'k':      /* formerly 'kill server'; now an undocumented option
                          that tells people to kill the processes manually */
            *request_kill = TRUE;
            break;

        case 'l':      /* log file */
            settings.logfile = strdup(optarg);
            break;

        case 'm':      /* ditto, but with SIGUSR2 not SIGTERM */
            *show_message = TRUE;
            break;

        case 'o':
            settings.dbport = strdup(optarg);
            break;

        case 't':
            settings.client_timeout = atoi(optarg);
            if (settings.client_timeout <= 30 ||
                settings.client_timeout > (24 * 60 * 60)) {
                fprintf(stderr,
                        "Error: Silly value %s given as the client timeout.\n",
                        optarg);
                return FALSE;
            }
            break;

        case 'u':
            settings.dbuser = strdup(optarg);
            break;

        case 'w':      /* work dir */
            settings.workdir = strdup(optarg);
            break;

        case 'h':      /* help */
            return FALSE;

        default:       /* unrecognized */
            fprintf(stderr, "Error: unknown option '%c'.\n", optopt);
            return FALSE;
        }
    }

    return TRUE;
}


/* srvmain.c */
