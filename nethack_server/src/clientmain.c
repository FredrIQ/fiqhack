/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-21 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

/* For POLLRDHUP */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include "nhserver.h"
#include <ctype.h>

#define DEFAULT_NETHACKDIR "/usr/share/NetHack4/"

#define COMMBUF_SIZE (1024 * 1024)

static int infd, outfd;
int gamefd;
long gameid;    /* id in the database */
struct user_info user_info;
static int can_send_msg;
static volatile sig_atomic_t currently_sending_message;
static volatile sig_atomic_t send_server_cancel;

static char **
init_game_paths(void)
{
    const char *pathlist[PREFIX_COUNT];
    char **pathlist_copy = malloc(sizeof (char *) * PREFIX_COUNT);
    const char *dir = NULL;
    int i, len;

    if (getgid() == getegid()) {
        dir = getenv("NETHACKDIR");
        if (!dir)
            dir = getenv("HACKDIR");
    }

    if (!dir || !*dir)
        dir = aimake_get_option("gamesdatadir");

    if (!dir || !*dir)
        dir = DEFAULT_NETHACKDIR;

    for (i = 0; i < PREFIX_COUNT; i++)
        pathlist[i] = dir;

    /* If the build system gave us more specific directories, use them. */
    const char *temp_path;

    temp_path = aimake_get_option("gamesstatedir");
    if (temp_path) {
        pathlist[BONESPREFIX] = temp_path;
        pathlist[SCOREPREFIX] = temp_path;
        pathlist[TROUBLEPREFIX] = temp_path;
        pathlist[DUMPPREFIX] = temp_path;
    }

    temp_path = aimake_get_option("specificlockdir");
    if (temp_path)
        pathlist[LOCKPREFIX] = temp_path;
    /* and leave NETHACKDIR to provide the data */

    /* alloc memory for the paths and append slashes as required */
    for (i = 0; i < PREFIX_COUNT; i++) {
        len = strlen(pathlist[i]);
        pathlist_copy[i] = malloc(len + (i == DUMPPREFIX ? 8 : 2));

        strcpy(pathlist_copy[i], pathlist[i]);
        if (pathlist_copy[i][len - 1] != '/') {
            pathlist_copy[i][len] = '/';
            pathlist_copy[i][len + 1] = '\0';
            len++;
        }
        if (i == DUMPPREFIX) {
            sprintf(pathlist_copy[i] + len, "dumps/");
        }
    }

    return pathlist_copy;
}

/* The low-level function responsible for doing the actual sending. This is
   async-signal-safe if the second argument is TRUE (this happens in signal
   handlers; also during exits for any reason, to prevent the exit code running
   recursively). */
void
send_string_to_client(const char *jsonstr, int defer_errors)
{
    int len = strlen(jsonstr);
    int pos = 0;
    int ret;

    currently_sending_message++;
    do {
        /* For NetHack 4.3, we separate the messages we send with NUL characters
           (which are not legal in JSON), so that the client can more easily
           find the boundary between messages. (NitroHack relied on separating
           messages using the boundary between packets, which doesn't work in
           practice.) The NUL is added using the terminating NUL of jsonstr. */
        ret = write(outfd, jsonstr + pos, len + 1 - pos);
        if (ret == -1 && (errno == EINTR || errno == EAGAIN))
            continue;
        else if (ret == -1 || ret == 0) {   /* bad news */
            if (defer_errors) {
                currently_sending_message--;
                return; /* handle the error later */
            }

            /* since we just found we can't write output to the pipe,
               prevent any more tries */
            close(infd);
            close(outfd);
            infd = outfd = -1;
            exit_client(NULL, 0);      /* Goodbye. */
        }
        pos += ret;
    } while (pos < len);
    currently_sending_message--;
}

/* Server cancels work differently from other messages; they can be sent out of
   sequence, can be sent from signal handlers, and don't have a response.

   This function runs async-signal! It can't touch globals, unless they're
   volatile; it can't allocate memory on the heap (which is why the JSON is
   hard-coded); and it can't call any function in the libc, except those
   specifially marked as safe (such as "write"). */
void
client_server_cancel_msg(void)
{
    if (currently_sending_message) {
        /* send it later, we don't want one message inside another */
        send_server_cancel = 1;
        return;
    }

    int save_errno = errno;

    send_string_to_client("{\"server_cancel\":{}}", TRUE);

    errno = save_errno;
}

static void
client_msg_core(const char *key, json_t *value, nh_bool from_exit)
{
    char *jsonstr;
    json_t *jval, *display_data;

    currently_sending_message = 1;

    jval = json_object();

    /* send out display data whenever anything else goes out */
    display_data = get_display_data();
    if (display_data) {
        json_object_set_new(jval, "display", display_data);
        display_data = NULL;
    }

    /* actual message content */
    json_object_set_new(jval, key, value);
    jsonstr = json_dumps(jval, JSON_COMPACT);
    json_decref(jval);

    if (can_send_msg)
        send_string_to_client(jsonstr, from_exit);

    /* this message is sent; don't send another */
    can_send_msg = FALSE;

    free(jsonstr);

    currently_sending_message = 0;

    if (send_server_cancel) {
        client_server_cancel_msg();
        send_server_cancel = 0;
    }
}

void
client_msg(const char *key, json_t *value)
{
    client_msg_core(key, value, FALSE);
}

noreturn void
exit_client(const char *err, int coredumpsignal)
{
    const char *msg = err ? err : "";
    json_t *exit_obj;

    if (err)
        log_msg("Client error: %s. Exit.", err);

    if (outfd != -1) {
        exit_obj = json_object();

        json_object_set_new(exit_obj, "error",
                            err ? json_true() : json_false());
        json_object_set_new(exit_obj, "message", json_string(msg));
        if (can_send_msg)
            client_msg_core("server_error", exit_obj, TRUE);
        else
            json_decref(exit_obj);

        close(infd);
        close(outfd);
        infd = outfd = -1;
    }

    termination_flag = 3;       /* make sure the command loop exits if
                                   nh_exit_game jumps there */
    if (!sigsegv_flag)
        nh_exit_game(EXIT_SAVE);  /* might not return here */
    nh_lib_exit();

    if (user_info.username)
        free(user_info.username);
    free_config();
    reset_cached_displaydata();

    exit_server(err == NULL ? EXIT_SUCCESS : EXIT_FAILURE,
                coredumpsignal);
}


json_t *
read_input(void)
{
    int ret, datalen, done;
    static char commbuf[COMMBUF_SIZE];
    char *bp;
    json_t *jval = NULL;
    json_error_t err;
    struct pollfd pfd[1] =
        { {infd, POLLIN | POLLRDHUP | POLLERR | POLLHUP, 0} };

    done = FALSE;
    datalen = 0;
    while (!done && !termination_flag) {
        ret = poll(pfd, 1, settings.client_timeout * 1000);
        if (ret == 0)
            exit_client("Inactivity timeout", 0);

        ret = read(infd, &commbuf[datalen], COMMBUF_SIZE - datalen - 1);
        if (ret == -1)
            continue;   /* sone signals will set termination_flag, others won't 
                         */
        else if (ret == 0)
            exit_client("Input pipe lost", 0);
        datalen += ret;

        if (commbuf[datalen - ret] == '\033') {
            /* this is a request to reset the buffer when recovering from a
               connection error. After such an error it simply isn't possible
               to know what data actually arrived. */
            /* do a memmove in case there was already some new legitimate data
               queued after the '\033' reset request. */
            memmove(commbuf, &commbuf[datalen - ret + 1], ret - 1);
            datalen = ret - 1;
            /* also reset the cached display data to make sure all display
               state is re-sent */
            continue;
        }

        commbuf[datalen] = '\0';        /* terminate the string */
        bp = &commbuf[datalen - 1];
        while (isspace(*bp))
            bp--;

        jval = NULL;
        if (*bp == '}') {       /* possibly the end of the json object */
            jval = json_loads(commbuf, JSON_REJECT_DUPLICATES, &err);
            if (jval)
                done = TRUE;
            else if (err.position < datalen)
                exit_client("Bad JSON data received", 0);
        }

        if (!jval && datalen >= COMMBUF_SIZE - 1)
            exit_client("Max allowed input length exceeded", 0);
        /* too much data received */
    }
    /* message received; now it's our turn to send */
    can_send_msg = TRUE;
    return jval;
}


static void
client_main_loop(void)
{
    json_t *obj, *value;
    const char *key;
    void *iter;
    int i;

    while (!termination_flag) {
        obj = read_input();
        if (termination_flag) {
            if (obj)
                json_decref(obj);
            return;
        }
        db_update_user_ts(user_info.uid);

        iter = json_object_iter(obj);
        if (!iter)
            exit_client("Empty command object received.", 0);

        /* find a command function to call */
        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);
        for (i = 0; clientcmd[i].name; i++)
            if (!strcmp(clientcmd[i].name, key)) {
                clientcmd[i].func(value);
                break;
            }

        if (!clientcmd[i].name)
            exit_client("Unknown command", 0);

        iter = json_object_iter_next(obj, iter);
        if (iter)
            exit_client(
                "More than one command received. This is unsupported.", 0);

        json_decref(obj);
    }
}


/*
 * This is the start of the client handling code.
 * The server process has accepted a connection and authenticated it. Data from
 * the client will arrive here via infd and data that should be sent back goes
 * through outfd.
 * An instance of NetHack will run in this process under the control of the
 * remote player. 
 */
noreturn void
client_main(int userid, int _infd, int _outfd)
{
    char **gamepaths;
    int i;

    infd = _infd;
    outfd = _outfd;
    gamefd = -1;

    if (!db_get_user_info(userid, &user_info)) {
        log_msg("get_user_info error for uid %d!", userid);
        exit_client("database error", SIGABRT);
    }

    gamepaths = init_game_paths();
    nh_lib_init(&server_windowprocs, (const char *const *)gamepaths);
    for (i = 0; i < PREFIX_COUNT; i++)
        free(gamepaths[i]);
    free(gamepaths);

    client_main_loop();

    exit_client(NULL, 0);
}
