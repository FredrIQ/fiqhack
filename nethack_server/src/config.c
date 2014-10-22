/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-22 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include "netconnect.h"

#include <ctype.h>
#include <stddef.h>

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

#define SETTINGS_MAP_ENTRY(x) {#x, offsetof(struct settings, x)}
struct { const char *const name; size_t offset; } settings_map[] = {
    SETTINGS_MAP_ENTRY(logfile),
    SETTINGS_MAP_ENTRY(workdir),
    SETTINGS_MAP_ENTRY(dbhost),
    SETTINGS_MAP_ENTRY(pidfile),
    SETTINGS_MAP_ENTRY(dbport),
    SETTINGS_MAP_ENTRY(dbuser),
    SETTINGS_MAP_ENTRY(dbpass),
    SETTINGS_MAP_ENTRY(dbname)
};

static int
parse_config_line(char *line)
{
    char *val;
    int i;

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

    /* set the actual option */

    for (i = 0; i < sizeof settings_map / sizeof *settings_map; i++) {
        if (!strcmp(line, settings_map[i].name)) {
            char **optptr = (char **)(settings_map[i].offset +
                                      (char *)&settings);
            if (!*optptr)
                *optptr = strdup(val);

            return TRUE;
        }
    }

    if (!strcmp(line, "client_timeout")) {
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

    const char *configdir = aimake_get_option("configdir");
    char default_filename[strlen(configdir) + strlen("/nethack4.conf") + 1];
    strcpy(default_filename, configdir);
    strcat(default_filename, "/nethack4.conf");

    if (!filename)
        filename = default_filename;

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


static char *
construct_server_filename(const char *option, const char *name)
{
    const char *dir = aimake_get_option(option);
    char *rv = malloc(strlen(dir) + strlen(name) + 1);
    strcpy(rv, dir);
    strcat(rv, name);
    return rv;
}


void
setup_defaults(void)
{
    if (!settings.logfile)
        settings.logfile =
            construct_server_filename("logdir", "/nethack4.log");

    if (!settings.pidfile)
        settings.pidfile =
            construct_server_filename("lockdir", "/nethack4.pid");

    if (!settings.workdir)
        settings.workdir =
            construct_server_filename("gamesstatedir", "");

    if (!settings.client_timeout)
        settings.client_timeout = DEFAULT_CLIENT_TIMEOUT;
}


void
free_config(void)
{
    int i;

    for (i = 0; i < sizeof settings_map / sizeof *settings_map; i++)
        free(*(char **)(settings_map[i].offset + (char *)&settings));

    memset(&settings, 0, sizeof (settings));
}

/* config.c */
