/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-07-07 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <ctype.h>
#include <sys/select.h>

#define AUTH_MAXLEN 4096
#define AUTHBUFSIZE (AUTH_MAXLEN + 2)

static int outfd = 1; /* stdout */
static int infd = 0;  /* stdin */

/* Used for reading from the socket before the connection is fully authed.  This
   uses a smaller timeout, and requires the auth to be a single packet (TODO:
   Does this make sense?) */
static int
timeouted_read(int fd, void *buf, int buflen)
{
    struct timeval timeout = {.tv_sec = 15, .tv_usec = 0};
    fd_set readfds;
    int ready_fds;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    do {
        ready_fds = select(fd + 1, &readfds, NULL, NULL, &timeout);
    } while (ready_fds < 0 && errno == EINTR);

    if (ready_fds == 1) {
        return read(fd, buf, buflen);
    } else if (ready_fds == 0) {
        /* Timeout. This is a silent disconnection; we want the other end of
           the connection to stay dormant until it tries to send something. */
        log_msg("Timeout during authentication. Disconnecting.");
        exit_server(EXIT_SUCCESS, 0);
    } else {
        /* Error. */
        log_msg("Failed to read from socket, error %s", strerror(errno));
        exit_server(EXIT_FAILURE, 0);
    }
}

static noreturn void
newclient(int userid)
{
    struct user_info info;

    db_get_user_info(userid, &info);
    setenv("NH4SERVERUSER", info.username, 1);
    client_main(userid, outfd, infd);
}

static int
auth_connection(void)
{
    int pos, is_reg, authlen, userid;
    char authbuf[AUTHBUFSIZE];

    authlen = timeouted_read(infd, authbuf, AUTHBUFSIZE - 1);
    if (authlen <= 0) {
        return -1;
    }

    log_msg("Auth data received.");

    authbuf[authlen] = '\0';    /* make it safe to use as a string */

    /* did we receive too much data? */
    if (authlen >= AUTH_MAXLEN) {
        log_msg("Auth buffer overrun attempt? Peer disconnected.");
        return -1;
    }

    /* check the end of the received auth data: a JSON object always ends with
       '}'; TODO: we're assuming the auth arrives in a single packet here,
       which is silly and unwarranted */
    pos = authlen - 1;
    while (pos > 0 && isspace(authbuf[pos]))
        pos--;

    if (authbuf[pos] != '}') {  /* not the end of JSON auth data */
        log_msg("authentication failed due to incomplete JSON");
        return -1;
    }

    /* ready to authenticate the user here */
    userid = auth_user(authbuf, &is_reg);
    if (userid <= 0) {
        if (!userid) {
            log_msg("Authentication failed: unknown user");
            auth_send_result(outfd, AUTH_FAILED_UNKNOWN_USER, is_reg);
        } else {
            log_msg("Authentication failed: wrong password");
            auth_send_result(outfd, AUTH_FAILED_BAD_PASSWORD, is_reg);
        }
        return -1;
    }

    auth_send_result(outfd, AUTH_SUCCESS_NEW, is_reg);

    return userid;
}


/* Authenticate the new connection, then call into clientmain. */

noreturn void
runserver(void)
{
    int userid = auth_connection();
    if (userid >= 0)
        newclient(userid);
    else
        exit_server(EXIT_FAILURE, 0);
}

noreturn void
exit_server(int exitstatus, int coredumpsignal)
{
    if (coredumpsignal) {
        /* don't bother cleaning up in this case */

        signal(coredumpsignal, SIG_DFL);
        raise(coredumpsignal);
    }

    /* shutdown */
    end_logging();
    close_database();
    free_config();

    exit(exitstatus);
}

/* server.c */
