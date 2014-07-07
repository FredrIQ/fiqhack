/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-07-07 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <ctype.h>


/* convert a string into either an ipv4 or an ipv6 address */
int
parse_ip_addr(const char *str, struct sockaddr *out, int want_v4)
{
    const struct addrinfo gai_hints = {
        .ai_flags = AI_PASSIVE | AI_NUMERICHOST,
        .ai_family = want_v4 ? AF_INET : AF_INET6,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
        .ai_addr = NULL,
        .ai_canonname = NULL,
        .ai_next = NULL
    };
    struct addrinfo *gai_res = NULL;

    if (getaddrinfo(str, NULL, &gai_hints, &gai_res) != 0)
        return FALSE;

    struct addrinfo *next;

    memcpy(out, gai_res->ai_addr, want_v4 ? sizeof (struct sockaddr_in) :
                                            sizeof (struct sockaddr_in6));
    do {
        next = gai_res->ai_next;
        free(gai_res);
        gai_res = next;
    } while (gai_res);

    return TRUE;
}


static char *
trim(char *str)
{
    char *end;

    /* remove the spaces around the string */
    while (isspace(*str))
        str++;
    if (*str == '\0')
        return str;

    end = &str[strlen(str) - 1];
    while (isspace(*end))
        *end-- = '\0';

    return str;
}


static int
parse_config_line(char *line)
{
    char *val;

    /* remove the spaces around the string */
    line = trim(line);

    /* empty line? OK, done */
    if (strlen(line) == 0)
        return TRUE;

    /* is it a comment? */
    if (*line == '#')
        return TRUE;

    val = strchr(line, '=');
    if (!val) {
        fprintf(stderr, "Error: bad configuration line: \"%s\".\n", line);
        return FALSE;
    }

    /* the string at line now only has the param name and val is the value
       string */
    *val++ = '\0';

    /* remove more spaces */
    line = trim(line);
    val = trim(val);

    /* 
     * set the actual option
     */

    if (!strcmp(line, "logfile")) {
        if (!settings.logfile)
            settings.logfile = strdup(val);
    }

    else if (!strcmp(line, "workdir")) {
        if (!settings.workdir)
            settings.workdir = strdup(val);
    }

    else if (!strcmp(line, "client_timeout")) {
        if (!settings.client_timeout)
            settings.client_timeout = atoi(val);

        if (settings.client_timeout < 30 ||
            settings.client_timeout > (24 * 60 * 60)) {
            fprintf(stderr,
                    "Error: the value for client_timeout must be in the"
                    " range [30, 86400].\n");
            return FALSE;
        }
    }

    else if (!strcmp(line, "dbhost")) {
        if (!settings.dbhost)
            settings.dbhost = strdup(val);
    }

    else if (!strcmp(line, "dbport")) {
        if (!settings.dbport)
            settings.dbport = strdup(val);
    }

    else if (!strcmp(line, "dbuser")) {
        if (!settings.dbuser)
            settings.dbuser = strdup(val);
    }

    else if (!strcmp(line, "dbpass")) {
        if (!settings.dbpass)
            settings.dbpass = strdup(val);
    }

    else if (!strcmp(line, "dbname")) {
        if (!settings.dbname)
            settings.dbname = strdup(val);
    }

    else
        /* it's a warning, no need to return FALSE */
        fprintf(stderr, "Warning: unrecognized option \"%s\".\n", line);

    return TRUE;
}


int
read_config(const char *confname)
{
    int fd, len, remain, ret;
    const char *filename = confname;
    char *data, *line, *newline;

    if (!filename)
        filename = DEFAULT_CONFIG_FILE;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        if (!confname) {
            fprintf(stderr, "Warning: Could not open %s. %s. Default settings "
                    "will be used.\n", filename, strerror(errno));
            return TRUE;        /* nonexistent default config need not be an
                                   error */
        }
        fprintf(stderr, "Error: Could not open %s. %s\n", filename,
                strerror(errno));
        return FALSE;   /* non-existent custom config file is always an error */
    }

    /* read the entire file into memory carefully */
    len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    data = malloc(len + 1);
    remain = len;
    while (remain) {
        ret = read(fd, data, remain);
        if (ret <= 0) { /* some error */
            free(data);
            close(fd);
            return FALSE;
        }
        remain -= ret;
    }
    close(fd);
    data[len] = '\0';

    /* break up the file data into lines using the '\n' char. */
    line = data;
    while ((newline = strchr(line, '\n'))) {
        *newline++ = '\0';
        if (!parse_config_line(line)) {
            free(data);
            return FALSE;
        }
        line = newline;
    }

    /* check for trailing junk */
    line = trim(line);
    if (strlen(line))
        fprintf(stderr, "Warning: trailing junk (\"%s\") after last config "
                "line ignored.\n", line);

    free(data);
    return TRUE;
}


void
setup_defaults(void)
{
    if (!settings.logfile)
        settings.logfile = strdup(DEFAULT_LOG_FILE);

    if (!settings.pidfile)
        settings.pidfile = strdup(DEFAULT_PID_FILE);

    if (!settings.workdir)
        settings.workdir = strdup(DEFAULT_WORK_DIR);

    if (!settings.client_timeout)
        settings.client_timeout = DEFAULT_CLIENT_TIMEOUT;
}


void
free_config(void)
{
    if (settings.logfile)
        free(settings.logfile);
    if (settings.pidfile)
        free(settings.pidfile);
    if (settings.workdir)
        free(settings.workdir);
    if (settings.dbhost)
        free(settings.dbhost);
    if (settings.dbport)
        free(settings.dbport);
    if (settings.dbname)
        free(settings.dbname);
    if (settings.dbuser)
        free(settings.dbuser);
    if (settings.dbpass)
        free(settings.dbpass);
    memset(&settings, 0, sizeof (settings));
}

/* config.c */
