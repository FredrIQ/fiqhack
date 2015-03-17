/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-17 */
/* Copyright (c) Daniel Thaler, 2012. */
/* The NetHack client lib may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhclient.h"
#include "netconnect.h"

struct nhnet_server_version nhnet_server_ver;

static int sockfd = -1;
/* We use a limited-size buffer for the unread messages to avoid/reduce the risk
   of DOS attacks, and to detect mistakes where the server is continuously
   sending characters that don't form valid messages. startptr is the start of
   the unread data; endptr the end of the received data. Whenever endptr
   catches startptr, they both reset to the start of the buffer. */
static char unread_messages[1024 * 1024 * 16];
static char *unread_message_startptr = unread_messages;
static char *unread_message_endptr = unread_messages;
static int net_active;
int conn_err, error_retry_ok;

/* Prevent automatic retries during connection setup or teardown. When the
   connection is being set up, it is better to report a failure immediately;
   when the connection is being closed it doesn't matter if it already is */
static int in_connect_disconnect;

/* saved connection details */
static char saved_hostname[256];
static int saved_port;
static char saved_username[200];        /* max 50 multibyte chars */
char saved_password[200];

jmp_buf ex_jmp_buf;
int ex_jmp_buf_valid;

static jmp_buf playgame_jmp_buf;
static int playgame_jmp_buf_valid = 0;
static json_t *send_receive_recent_response;

void
print_error(const char *msg)
{
    client_windowprocs.win_raw_print(msg);
}

/* Test for a closed network connection and try to reopen it.
 * Not all errors can be detected without sending data first.
 * 
 * Possible network errors:
 * - the client handling process on the server timed out or the server was
 *   restarted. Result: closed socket; will be fixed here
 * - the connection may become invalid due to outside interference
 *   (a reconnect + IP address change triggered by the ISP, for example)
 *   This case is especially evil, as it will only become detectable after a
 *   write (which will get us an RST packet from the server)
 * - network down, bad wifi signal, etc. - nothing we can do about these
 */
static int
test_restore_connection(void)
{
    int ret;
    fd_set rfds;
    struct timeval tv = { 0, 0 };
    char testbuf[1];

    if (sockfd < 0)
        return restart_connection();

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    ret = select(1, &rfds, NULL, NULL, &tv);
    /* ret < 0: select error. Lets hope for the best and continue */
    /* ret == 0: select timeout, no events on sockfd. The desired result. */
    if (ret > 0) {      /* sockfd is "ready"; check whether recv returns EOF as
                           we're not expecting any data */
        ret = recv(sockfd, testbuf, 1, MSG_PEEK);
        if (ret <= 0)
            return restart_connection();
        return TRUE;
    }
    return TRUE;
}


static int
send_json_msg(json_t * jmsg)
{
    char *msgstr;
    int msglen, datalen, ret;

    msgstr = json_dumps(jmsg, JSON_COMPACT);
    msglen = strlen(msgstr);
    datalen = 0;
    do {
        ret = send(sockfd, &msgstr[datalen], msglen - datalen, 0);
        if (ret == -1 && errno == EINTR)
            continue;
        else if (ret == -1) {
            free(msgstr);
            return FALSE;
        }
        datalen += ret;
    } while (datalen < msglen);

    free(msgstr);
    return TRUE;
}


/* receive one JSON object from the server.
 * Returns: - NULL after a network error OR
 *          - an empty JSON object if there is a parsing error OR
 *          - the parsed response from the server */
static json_t *
receive_json_msg(void)
{
    int datalen, ret;
    json_t *recv_msg;
    json_error_t err;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    recv_msg = NULL;
    datalen = 0;
    while (!recv_msg) {
        /* Do we have unread messages to return? We have a complete unread
           message if there's a NUL byte anywhere in the unread region. */
        char *eom = memchr(unread_message_startptr, '\0',
                           unread_message_endptr - unread_message_startptr);
        if (eom) {
            recv_msg = json_loads(unread_message_startptr,
                                  JSON_REJECT_DUPLICATES, &err);
            unread_message_startptr = eom+1;

            if (!recv_msg && err.position < datalen) {
                unread_message_startptr = unread_message_endptr =
                    unread_messages;
                print_error("Broken response received from server");
                return json_object();
            }

            if (unread_message_endptr == unread_message_startptr)
                unread_message_startptr = unread_message_endptr =
                    unread_messages;
        } else {
            /* select before reading so that we get a timeout. Otherwise the
               program might hang indefinitely in read if the connection has
               failed. */
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            do {
                ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);
            } while (ret == -1 && errno == EINTR);
            
            if (ret <= 0) {
                unread_message_startptr = unread_message_endptr =
                    unread_messages;
                return NULL;
            }

            ret = recv(sockfd, unread_message_endptr,
                       unread_messages + sizeof unread_messages -
                       unread_message_endptr, 0);
            if (ret == -1 && errno == EINTR)
                continue;
            else if (ret <= 0)
                return NULL;
            if (unread_message_endptr >
                unread_messages + sizeof unread_messages - 2) {
                unread_message_startptr = unread_message_endptr =
                    unread_messages;
                print_error("The server is sending messages too quickly.");
                return json_object();
            }
            unread_message_endptr += ret;
            /* loop back and see if we have a complete message yet */
        }
    }

    return recv_msg;
}


/* send a command to the server, handle callbacks (menus, getline, etc) and
   display data (map data, pline, player status, ...) and finally return the
   response to the command.

   A play_game response will longjmp() to the matching play_game request, so
   beware! */
json_t *
send_receive_msg(const char *const volatile msgtype, json_t *volatile jmsg)
{
    const char *volatile sendkey;
    char key[BUFSZ];
    char oldkey[BUFSZ];
    void *iter;
    volatile int retry_count = 3;

    if (conn_err && ex_jmp_buf_valid)
        longjmp(ex_jmp_buf, 1);

    /* quick connection sanity check and restoration attempt */
    if (!test_restore_connection())
        return NULL;

    if (!jmsg)
        return NULL;

    sendkey = msgtype;
    while (1) {
        /* send the message; keep the reference to jmsg */
        json_t *send_msg = json_pack("{sO}", sendkey, jmsg);
        if (!send_json_msg(send_msg)) {
            json_decref(send_msg);
            if (retry_count-- > 0 && restart_connection())
                continue;
            goto error;
        }
        json_decref(send_msg);

    receive_without_sending:
        ;
        /* receive the response */
        json_t *recv_msg = receive_json_msg();
        if (!recv_msg) {
            /* If no data is received, there must have been a network error.
               Presumably the send didn't succeed either and the sent data
               vanished, so reconnect. restart_connection() can longjmp back to
               play_game; if it doesn't, retry both send and receive. */
            if (retry_count-- > 0 && restart_connection())
                continue;
            goto error;
        }

        json_t *jdisplay = json_object_get(recv_msg, "display");
        if (jdisplay) {
            if (json_is_array(jdisplay))
                handle_display_list(jdisplay);
            else
                print_error
                    ("New display list doesn't have the right data type.");
            json_object_del(recv_msg, "display");
        }

        iter = json_object_iter(recv_msg);
        if (!iter) {
            print_error("Empty return object.");
            json_decref(recv_msg);
            json_decref(jmsg);
            return json_object();
        }

        /* The string returned by json_object_iter_key is only valid while
           recv_msg exists. Since we still want the value afterwards, it must
           be copied. */
        strncpy(key, json_object_iter_key(iter), BUFSZ - 1);

        if (!strcmp(key, "server_cancel")) {
            /* This message is special in that it can be called out of
               sequence, and has no response. */
            json_decref(recv_msg); /* free it */
            client_windowprocs.win_server_cancel();
            goto receive_without_sending;
        }
        if (!strcmp(key, "load_progress")) {
            /* This message is only called in-sequence, but it still has no
               response. */
            int progress;
            if (json_unpack(json_object_iter_value(iter), "{si!}",
                            "progress", &progress) != -1)
                client_windowprocs.win_load_progress(progress);
            json_decref(recv_msg); /* free it */
            goto receive_without_sending;
        }

        send_receive_recent_response = json_object_iter_value(iter);

        if (json_object_iter_next(recv_msg, iter))
            print_error("Too many JSON objects in response data.");

        /* keep only the core of the response and throw away the wrapper */
        json_incref(send_receive_recent_response);
        json_decref(recv_msg);

        if (strcmp(sendkey, "play_game") == 0) {
            /* We might need to longjmp back here. */
            if (setjmp(playgame_jmp_buf) == 0) {
                playgame_jmp_buf_valid = 1;
            } else {
                playgame_jmp_buf_valid = 0;
                /* key, sendkey might have any value right now, but we know what
                   they should be from the position in the control flow */
                sendkey = "play_game";
                memset(key, 0, sizeof key);
                strcpy(key, "play_game");
            }
        }

        /* If the response type doesn't match the request type then either:
           - this is a callback that needs to be handled first;
           - this is a request to longjmp() back to nhnet_play_game.

           To simplify the control flow, our longjmp back upon receiving a
           play_game response is unconditional, and ends up cancelling itself
           out if a play_game message gets a play_game response. This also
           guarantees that playgame_jmp_buf_valid is only set while
           playgame_jmp_buf is actually on the call stack. */
        if (strcmp(key, "play_game") == 0 && playgame_jmp_buf_valid)
            longjmp(playgame_jmp_buf, 1);

        if (strcmp(key, msgtype)) {
            json_t *srvmsg = send_receive_recent_response;
            /* The next line is unneccessary, but makes the control flow easier
               to follow in a debugger. */
            send_receive_recent_response = 0;
            json_t *newmsg = handle_netcmd(key, msgtype, srvmsg);
            if (!newmsg) {     /* server error */
                if (error_retry_ok && retry_count-- > 0 && restart_connection())
                    continue;  /* jmsg is still alive, use it again */
                goto error;
            }

            json_decref(jmsg);
            jmsg = newmsg;
            strcpy(oldkey, key);
            sendkey = oldkey;

            /* send the callback data to the server and get a new response */
            continue;
        }

        json_decref(jmsg);
        break;  /* only loop via continue */
    }

    json_t *response = send_receive_recent_response;
    send_receive_recent_response = 0;

    return response;

error:
    json_decref(jmsg);
    close(sockfd);
    sockfd = -1;
    conn_err = TRUE;
    playgame_jmp_buf_valid = 0;
    if (ex_jmp_buf_valid)
        longjmp(ex_jmp_buf, 1);
    return NULL;
}


int
nhnet_get_socket_fd(void)
{
    return sockfd;
}


/* Should only be called when the return value of nhnet_get_socket_fd refers to
   a readable file descriptor (otherwise it may well hang); and should be called
   in that situation. The client library will check for server cancels, and call
   win_server_cancel if it finds one; it will error out if it finds any other
   sort of message (because the server shouldn't be sending asynchronously
   otherwise).

   If the connection has dropped, this function will attempt to restart the
   connection; this may involve longjmping back to play_game in the process.

   This function is not async-signal-safe. (Most functions aren't, but it's less
   obvious with this one because its main purpose is to call win_server_cancel,
   which is signal-safe, and it's not unreasonable to want to call it directly
   from a SIGIO handler. You should handle the signal in the normal way instead,
   via bouncing it off your event loop.) */
void 
nhnet_check_socket_fd(void)
{
    json_t *j = receive_json_msg();
    void *iter;

    if (j == NULL) {
        /* connection failure */
        if (restart_connection()) /* might longjmp or return normally */
            return;

        /* we couldn't restart it */
        close(sockfd);
        sockfd = -1;
        conn_err = TRUE;
        playgame_jmp_buf_valid = 0;
        if (ex_jmp_buf_valid)
            longjmp(ex_jmp_buf, 1);
        /* otherwise we can't do anything about the error */
        return;
    }

    iter = json_object_iter(j);
    if (!iter || strcmp(json_object_iter_key(iter), "server_cancel") != 0) {
        /* Misbehaving server, it shouldn't be sending out of sequence */
        print_error("Server sent a JSON object out of sequence");
        if (ex_jmp_buf_valid) {
            playgame_jmp_buf_valid = 0;
            longjmp(ex_jmp_buf, 1);
        }
        /* otherwise we can't do anything about the error */
        return;
    }

    json_decref(j);
    client_windowprocs.win_server_cancel();
}


static int
do_connect(const char *host, int port, const char *user, const char *pass,
           const char *email, int reg_user)
{
    int fd = -1, authresult;
    char ipv6_error[120], ipv4_error[120], errmsg[256];
    json_t *jmsg, *jarr;

    /* try ipv6 */
    if (fd == -1)
        fd = connect_server(host, port, FALSE, ipv6_error, 120);

    if (fd == -1)
        /* no ipv6 connection, try ipv4 */
        fd = connect_server(host, port, TRUE, ipv4_error, 120);

    if (fd == -1) {
        /* both connection attempts failed: error message */
        if (!ipv6_error[0] && !ipv4_error[0]) {
            snprintf(errmsg, 256, "No address for \"%s\" found!", host);
            print_error(errmsg);
        }

        if (ipv6_error[0]) {
            snprintf(errmsg, 256, "IPv6: %s", ipv6_error);
            print_error(errmsg);
        }

        if (ipv4_error[0]) {
            snprintf(errmsg, 256, "IPv4: %s", ipv4_error);
            print_error(errmsg);
        }

        sockfd = -1;
        net_active = FALSE;
        return NO_CONNECTION;
    }

    in_connect_disconnect = TRUE;
    sockfd = fd;
    jmsg = json_pack("{ss,ss}", "username", user, "password", pass);
    if (reg_user) {
        if (email)
            json_object_set_new(jmsg, "email", json_string(email));
        jmsg = send_receive_msg("register", jmsg);
    } else {
        jmsg = send_receive_msg("auth", jmsg);
    }
    in_connect_disconnect = FALSE;

    if (!jmsg ||
        json_unpack(jmsg, "{si*}", "return", &authresult) == -1) {
        if (jmsg)
            json_decref(jmsg);
        close(fd);
        net_active = FALSE;
        sockfd = -1;
        return NO_CONNECTION;
    }
    /* the "version" field in the response is optional */
    if (json_unpack(jmsg, "{so*}", "version", &jarr) != -1 &&
        json_is_array(jarr) && json_array_size(jarr) >= 3) {
        nhnet_server_ver.major = json_integer_value(json_array_get(jarr, 0));
        nhnet_server_ver.minor = json_integer_value(json_array_get(jarr, 1));
        nhnet_server_ver.patchlevel =
            json_integer_value(json_array_get(jarr, 2));
    }
    json_decref(jmsg);

    if (host != saved_hostname)
        strncpy(saved_hostname, host, sizeof (saved_hostname));
    if (user != saved_username)
        strncpy(saved_username, user, sizeof (saved_username));
    if (pass != saved_password)
        strncpy(saved_password, pass, sizeof (saved_password));
    saved_port = port;

    if (authresult == AUTH_SUCCESS_NEW) {
        conn_err = FALSE;
        net_active = TRUE;
    } else {
        net_active = FALSE;
        sockfd = -1;
    }

    return authresult;
}


int
nhnet_connect(const char *host, int port, const char *user, const char *pass,
              const char *email, int reg_user)
{
    if (port == 0)
        port = DEFAULT_PORT;

    return do_connect(host, port, user, pass, email, reg_user);
}


void
nhnet_disconnect(void)
{
    json_t *msg;

    if (sockfd != -1) {
        in_connect_disconnect = TRUE;
        msg = send_receive_msg("shutdown", json_object());
        in_connect_disconnect = FALSE;
        if (msg)
            json_decref(msg);
        close(sockfd);
    }
    sockfd = -1;
    conn_err = FALSE;
    net_active = FALSE;
    memset(&nhnet_server_ver, 0, sizeof (nhnet_server_ver));
}


/* returns true between calls to nhnet_connect and nhnet_disconnect, even if
 * the connection is lost due to some network error. */
int
nhnet_active(void)
{
    return net_active;
}


int
nhnet_connected(void)
{
    return sockfd != -1;
}


/* An error has happened and the connection referred to by sockfd is no longer
   OK; close the old connection and try to open a new one. */
int
restart_connection(void)
{
    int ret;

    if (sockfd != -1)
        close(sockfd);
    sockfd = -1;

    if (in_connect_disconnect)
        return FALSE;

    ret =
        do_connect(saved_hostname, saved_port, saved_username, saved_password,
                   NULL, 0);
    if (ret != AUTH_SUCCESS_NEW)
        return FALSE;

    /* If we were in a game when the connection went down, we want to reload the
       game. This is done using by longjmping out of any prompts that might be
       open at the time (yay, disconnection works correctly at prompts now!),
       and getting the client's main loop to reconnect. */
    if (ret == AUTH_SUCCESS_NEW && playgame_jmp_buf_valid) {
        send_receive_recent_response =
            json_pack("{si}", "return", RESTART_PLAY);
        longjmp(playgame_jmp_buf, 1);
    }

    return TRUE;
}

/* connection.c */
