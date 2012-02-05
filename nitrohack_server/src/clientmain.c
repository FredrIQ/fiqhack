/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include <poll.h>
#include <ctype.h>

#define COMMBUF_SIZE (1024 * 1024)


static int infd, outfd;
int gamefd;
long gameid; /* id in the database */
struct user_info user_info;
int can_send_msg;


static char** init_game_paths(void)
{
    char **pathlist = malloc(sizeof(char*) * PREFIX_COUNT);
    char *dir = NULL, *tmp;
    int i, len;
    
    if (getgid() == getegid()) {
	dir = getenv("NITROHACKDIR");
	if (!dir)
	    dir = getenv("HACKDIR");
    }
    
    if (!dir)
	dir = NITROHACKDIR;
    
    for (i = 0; i < PREFIX_COUNT; i++)
	pathlist[i] = dir;
    
    /* alloc memory for the paths and append slashes as required */
    for (i = 0; i < PREFIX_COUNT; i++) {
	tmp = pathlist[i];
	len = strlen(tmp);
	pathlist[i] = malloc(len + 2);
	
	strcpy(pathlist[i], tmp);
	if (pathlist[i][len-1] != '/') {
	    pathlist[i][len] = '/';
	    pathlist[i][len+1] = '\0';
	}
    }
    
    strcpy(pathlist[DUMPPREFIX], "./");
    
    return pathlist;
}


void client_msg(const char *key, json_t *value)
{
    int len, ret, pos;
    char *jsonstr;
    json_t *jval, *display_data;
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
    
    if (can_send_msg) {
	len = strlen(jsonstr);
	pos = 0;
	do {
	    ret = write(outfd, &jsonstr[pos], len - pos);
	    if (ret == -1 && (errno == EINTR || errno == EAGAIN))
		continue;
	    else if (ret == -1 || ret == 0) { /* bad news */
		/* since we just found we can't write output to the pipe,
		 * prevent any more tries */
		close(infd);
		close(outfd);
		infd = outfd = -1;
		exit_client(NULL); /* Goodbye. */
	    }
	    pos += ret;
	} while(pos < len);
    }
    /* this message is sent; don't send another */
    can_send_msg = FALSE;
    
    free(jsonstr);
}

void exit_client(const char *err)
{
    const char *msg = err ? err : "";
    json_t *exit_obj;
    
    if (err)
	log_msg("Client error: %s. Exit.", err);
    
    if (outfd != -1) {
	exit_obj = json_object();
	
	json_object_set_new(exit_obj, "error", err ? json_true() : json_false());
	json_object_set_new(exit_obj, "message", json_string(msg));
	if (can_send_msg)
	    client_msg("server_error", exit_obj);
	else
	    json_decref(exit_obj);
	
	usleep(100); /* try to make sure the server process handles write() before close(). */
	close(infd);
	close(outfd);
	infd = outfd = -1;
    }
    
    termination_flag = 3; /* make sure the command loop exits if nh_exit_game jumps there */
    if (!sigsegv_flag)
	nh_exit_game(EXIT_FORCE_SAVE); /* might not return here */
    nh_lib_exit();
    close_database();
    if (user_info.username)
	free(user_info.username);
    free_config();
    reset_cached_diplaydata();
    end_logging();
    exit(err != NULL);
}


json_t *read_input(void)
{
    int ret, datalen, done;
    static char commbuf[COMMBUF_SIZE];
    char *bp;
    json_t *jval = NULL;
    json_error_t err;
    struct pollfd pfd[1] = {{infd, POLLIN | POLLRDHUP | POLLERR | POLLHUP, 0}};
    
    done = FALSE;
    datalen = 0;
    while (!done && !termination_flag) {
	ret = poll(pfd, 1, settings.client_timeout * 1000);
	if (ret == 0)
	    exit_client("Inactivity timeout");
	
	ret = read(infd, &commbuf[datalen], COMMBUF_SIZE - datalen - 1);
	if (ret == -1)
	    continue; /* sone signals will set termination_flag, others won't */
	else if (ret == 0)
	    exit_client("Input pipe lost");
	datalen += ret;
	
	if (commbuf[datalen-ret] == '\033') {
	    /* this is a request to reset the buffer when recovering from a
	     * connection error. After such an error it simply isn't possible
	     * to know what data actually arrived. */
	    /* do a memmove in case there was already some new legitimate data
	     * queued after the '\033' reset request. */
	    memmove(commbuf, &commbuf[datalen-ret+1], ret - 1);
	    datalen = ret - 1;
	    /* also reset the cached display data to make sure all display state is re-sent */
	    continue;
	}
	
	commbuf[datalen] = '\0'; /* terminate the string */
	bp = &commbuf[datalen - 1];
	while (isspace(*bp))
	    bp--;

	jval = NULL;
	if (*bp == '}') { /* possibly the end of the json object */
	    jval = json_loads(commbuf, JSON_REJECT_DUPLICATES, &err);
	    if (jval)
		done = TRUE;
	    else if (err.position < datalen)
		exit_client("Bad JSON data received");
	}
	
	if (!jval && datalen >= COMMBUF_SIZE - 1)
	    exit_client("Max allowed input length exceeded"); /* too much data received */
    }
    /* message received; mow it's our turn to send */
    can_send_msg = TRUE;
    return jval;
}


static void client_main_loop(void)
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
	    exit_client("Empty command object received.");
	
	/* find a command function to call */
	key = json_object_iter_key(iter);
	value = json_object_iter_value(iter);
	for (i = 0; clientcmd[i].name; i++)
	    if (!strcmp(clientcmd[i].name, key)) {
		clientcmd[i].func(value);
		break;
	    }
	
	if (!clientcmd[i].name)
	    exit_client("Unknown command");
	
	iter = json_object_iter_next(obj, iter);
	if (iter)
	    exit_client("More than one command received. This is unsupported.");
	
	json_decref(obj);
    }
}


/*
 * This is the start of the client handling code.
 * The server process has accepted a connection and authenticated it. Data from
 * the client will arrive here via infd and data that should be sent back goes
 * through outfd.
 * An instance of NitroHack will run in this process under the control of the
 * remote player. 
 */
void client_main(int userid, int _infd, int _outfd)
{
    char **gamepaths;
    int i;
    
    infd = _infd;
    outfd = _outfd;
    gamefd = -1;
    
    init_database();
    if (!db_get_user_info(userid, &user_info)) {
	log_msg("get_user_info error for uid %d!", userid);
	exit_client("database error");
    }
    
    gamepaths = init_game_paths();
    nh_lib_init(&server_windowprocs, gamepaths);
    for (i = 0; i < PREFIX_COUNT; i++)
	free(gamepaths[i]);
    free(gamepaths);

    db_restore_options(userid);
    
    client_main_loop();
    
    exit_client(NULL);
    /*NOTREACHED*/
    
    return;
}
