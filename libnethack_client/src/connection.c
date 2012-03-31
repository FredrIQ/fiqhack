/* Copyright (c) Daniel Thaler, 2012. */
/* The NitroHack client lib may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhclient.h"

struct nhnet_server_version nhnet_server_ver;

static int sockfd = -1;
static int connection_id;
static int net_active;
int conn_err, error_retry_ok;

/* Prevent automatic retries during connection setup or teardown.
 * When the connection is being set up, it is better to report a failure
 * immediately; when the connection is being closed it doesn't matter if it
 * already is */
static int in_connect_disconnect;

/* saved connection details */
static char saved_hostname[256];
static int saved_port;
static char saved_username[200]; /* max 50 multibyte chars */
char saved_password[200];

jmp_buf ex_jmp_buf;
int ex_jmp_buf_valid;

void print_error(const char *msg)
{
    windowprocs.win_raw_print(msg);
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
static int test_restore_connection(void)
{
    int ret;
    fd_set rfds;
    struct timeval tv = {0, 0};
    char testbuf[1];
    
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    
    ret = select(1, &rfds, NULL, NULL, &tv);
    /* ret < 0: select error. Lets hope for the best and continue */
    /* ret == 0: select timeout, no events on sockfd. The desired result. */
    if (ret > 0) { /* sockfd is "ready"; check whether recv returns EOF as
	            * we're not expecting any data */
	ret = recv(sockfd, testbuf, 1, MSG_PEEK);
	if (ret <= 0)
	    return restart_connection();
	return TRUE;
    }
    return TRUE;
}


static int send_json_msg(json_t *jmsg)
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
static json_t *receive_json_msg(void)
{
    char *rbuf, *bp;
    int datalen, ret, rbufsize;
    json_t *recv_msg;
    json_error_t err;
    fd_set rfds;
    struct timeval tv;
    
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    
    rbufsize = 1024 * 1024; /* initial size: 1MB */
    rbuf = malloc(rbufsize);
    memset(rbuf, 0, rbufsize);
    recv_msg = NULL;
    datalen = 0;
    while (!recv_msg) {
	/* select before reading so that we get a timeout. Otherwise the
	 * program might hang indefinitely in read if the connection has failed */
	tv.tv_sec = 10; /* 10s * 3 retries results in a long wait on failed connections... */
	tv.tv_usec = 0;
	ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);
	if (ret <= 0) {
	    /* we aren't expecting any signals, so it seems ok to abort even if
	     * ret == -1 && errno == EINTR */
	    free(rbuf);
	    return NULL;
	}
	
	/* leave the last byte in the buffer free for the '\0' */
	ret = recv(sockfd, &rbuf[datalen], rbufsize - datalen - 1, 0);
	if (ret == -1 && errno == EINTR)
	    continue;
	else if (ret <= 0) {
	    free(rbuf);
	    return NULL;
	}
	datalen += ret;
	
	rbuf[datalen] = '\0'; /* terminate the string */
	bp = &rbuf[datalen - 1];
	while (isspace(*bp))
	    bp--;

	recv_msg = NULL;
	if (*bp == '}') { /* possibly the end of the json object */
	    recv_msg = json_loads(rbuf, JSON_REJECT_DUPLICATES, &err);
	    if (!recv_msg && err.position < datalen) {
		print_error("Broken response received from server.");
		free(rbuf);
		return json_object();
	    }
	}
	
	/* allow the receive buffer to grow to 16MB. Growing larger than 1MB is
	 * extremely unlikely (I can't imagine how it would happen); 16MB or
	 * more is clearly an error. */
	if (!recv_msg && datalen >= rbufsize - 1) {
	    if (datalen < 16 * 1024 * 1024) {
		rbufsize *= 2;
		rbuf = realloc(rbuf, rbufsize);
	    } else {
		print_error("Too much incoming data. Server error?");
		free(rbuf);
		return json_object();
	    }
	}
    }
    
    free(rbuf);    
    return recv_msg;
}


/* send a command to the server, handle callbacks (menus, getline, etc) and
 * display data (map data, pline, player status, ...) and finally return the
 * response to the command. */
json_t *send_receive_msg(const char *msgtype, json_t *jmsg)
{
    json_t *recv_msg, *jobj, *jdisplay;
    const char *sendkey;
    char key[BUFSZ];
    void *iter;
    int retry_count = 3;
    
    if (conn_err && ex_jmp_buf_valid)
	longjmp(ex_jmp_buf, 1);
    
    /* quick connection sanity check and restoration attempt */
    if (!test_restore_connection())
	return NULL;
    
    if (!jmsg)
	return NULL;
    
    sendkey = msgtype;
    while (1) {
	/* send the message */
	jobj = json_pack("{sO}", sendkey, jmsg); /* keep the reference to jmsg */
	if (!send_json_msg(jobj)) {
	    json_decref(jobj);
	    if (retry_count-- > 0 && restart_connection())
		continue;
	    goto error;
	}
	
	/* receive the response */
	recv_msg = receive_json_msg();
	if (!recv_msg) {
	    json_decref(jobj);
	    /* If no data is received, there must have been a network error.
	     * Presumably the send didn't succeed either and the sent data
	     * vanished, so reconnect and retry both send and receive. */
	    if (retry_count-- > 0 && restart_connection())
		continue;
	    goto error;
	}
	
	json_decref(jobj);
	jdisplay = json_object_get(recv_msg, "display");
	if (jdisplay) {
	    if (json_is_array(jdisplay))
		handle_display_list(jdisplay);
	    else
		print_error("New display list doesn't have the right data type.");
	    json_object_del(recv_msg, "display");
	}
	
	iter = json_object_iter(recv_msg);
	if (!iter) {
	    print_error("Empty return object.");
	    json_decref(recv_msg);
	    json_decref(jmsg);
	    return json_object();
	}
	
	/* The string returned by json_object_iter_key is only valid while recv_msg
	 * exists. Since we still want the value afterwards, it must be copied */
	strncpy(key, json_object_iter_key(iter), BUFSZ - 1);
	jobj = json_object_iter_value(iter);
	
	if (json_object_iter_next(recv_msg, iter))
	    print_error("Too many JSON objects in response data.");
	
	/* keep only the core of the response and throw away the wrapper */
	json_incref(jobj);
	json_decref(recv_msg);
	
	/* if the response type doesn't match the request type this must be a
	 * callback that needs to be handled first. */
	if (strcmp(key, msgtype)) {
	    jobj = handle_netcmd(key, jobj);
	    if (!jobj) {/* this only happens after server errors */
		if (error_retry_ok && retry_count-- > 0 && restart_connection())
		    continue;
		
		json_decref(jmsg);
		return NULL;
	    }
	    
	    json_decref(jmsg);
	    jmsg = jobj;
	    sendkey = key;
	    
	    /* send the callback data to the server and get a new response */
	    continue;
	}
	json_decref(jmsg);
	break; /* only loop via continue */
    }
    
    return jobj;
    
error:
    json_decref(jmsg);
    close(sockfd);
    sockfd = -1;
    conn_err = TRUE;
    if (ex_jmp_buf_valid)
	longjmp(ex_jmp_buf, 1);
    return NULL;
}


/* convert a string (address or hosname) into an address */
static int parse_ip_addr(const char *host, int port, int want_v4,
			 struct sockaddr_storage *out)
{
    int res;
    char portstr[16];
    struct addrinfo *gai_res = NULL;
    struct addrinfo *next;
    struct addrinfo gai_hints;
    memset(&gai_hints, 0, sizeof(gai_hints));
    gai_hints.ai_flags = AI_NUMERICSERV;
    gai_hints.ai_family = want_v4 ? AF_INET : AF_INET6;
    gai_hints.ai_socktype = SOCK_STREAM;

    sprintf(portstr, "%d", port);
    
    res = getaddrinfo(host, portstr, &gai_hints, &gai_res);
    if (res != 0)
	return FALSE;
    if (want_v4)
	memcpy(out, gai_res->ai_addr,sizeof(struct sockaddr_in));
    else
	memcpy(out, gai_res->ai_addr,sizeof(struct sockaddr_in6));
    
#ifndef WIN32 /* it seems the result structures should not be free'd on Windows */
    do {
	next = gai_res->ai_next;
	free(gai_res);
	gai_res = next;
    } while (gai_res);
#endif

    return TRUE;
}


static int connect_server(const char *host, int port, int want_v4, char *errmsg, int msglen)
{
    struct sockaddr_storage sa;
    int fd = -1;
    
    errmsg[0] = '\0';
    if (parse_ip_addr(host, port, want_v4, &sa)) {
	fd = socket(sa.ss_family, SOCK_STREAM, 0);
	if (fd == -1) {
	    snprintf(errmsg, msglen, "failed to create a socket: %s\n", strerror(errno));
	    return -1;
	}
	
	if (connect(fd, (struct sockaddr*)&sa, want_v4 ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) == -1) {
	    snprintf(errmsg, msglen, "could not connect: %s\n", strerror(errno));
	    close(fd);
	    return -1;
	}
    }
    
    return fd;
}


static int do_connect(const char *host, int port, const char *user, const char *pass,
		      const char *email, int reg_user, int connid)
{
    int fd = -1, authresult, copylen;
    char ipv6_error[120], ipv4_error[120], errmsg[256];
    json_t *jmsg, *jarr;
    
#ifdef UNIX
    /* try to connect to a local unix socket */
    struct sockaddr_un sun;
    sun.sun_family = AF_UNIX;
    
    copylen = strlen(host) + 1;
    if (copylen > sizeof(sun.sun_path) - 1)
	copylen = sizeof(sun.sun_path) - 1;
    memcpy(sun.sun_path, host, copylen);
    sun.sun_path[sizeof(sun.sun_path) - 1] = '\0';
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd >= 0) {
	if (connect(fd, (struct sockaddr*)&sun, sizeof(sun)) == -1) {
	    close(fd);
	    fd = -1;
	}
    }
#endif

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
	if (connid)
	    json_object_set_new(jmsg, "reconnect", json_integer(connid));
	jmsg = send_receive_msg("auth", jmsg);
    }
    in_connect_disconnect = FALSE;
    
    if (!jmsg || json_unpack(jmsg, "{si,si*}", "return", &authresult,
	"connection", &connection_id) == -1) {
	if (jmsg)
	    json_decref(jmsg);
	close(fd);
	return NO_CONNECTION;
    }
    /* the "version" field in the response is optional */
    if (json_unpack(jmsg, "{so*}", "version", &jarr) != -1 &&
	json_is_array(jarr) && json_array_size(jarr) >= 3) {
	nhnet_server_ver.major = json_integer_value(json_array_get(jarr, 0));
	nhnet_server_ver.minor = json_integer_value(json_array_get(jarr, 1));
	nhnet_server_ver.patchlevel = json_integer_value(json_array_get(jarr, 2));
    }
    json_decref(jmsg);
    
    if (host != saved_hostname)
	strncpy(saved_hostname, host, sizeof(saved_hostname));
    if (user != saved_username)
	strncpy(saved_username, user, sizeof(saved_username));
    if (pass != saved_password)
	strncpy(saved_password, pass, sizeof(saved_password));
    saved_port = port;
    conn_err = FALSE;
    net_active = TRUE;
    
    return authresult;
}


int nhnet_connect(const char *host, int port, const char *user, const char *pass,
		  const char *email, int reg_user)
{
    if (port == 0)
	port = DEFAULT_PORT;
    
    return do_connect(host, port, user, pass, email, reg_user, 0);
}


void nhnet_disconnect(void)
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
    connection_id = 0;
    current_game = 0;
    conn_err = FALSE;
    net_active = FALSE;
    xmalloc_cleanup();
    free_option_lists();
    memset(&nhnet_server_ver, 0, sizeof(nhnet_server_ver));
}


/* returns true between calls to nhnet_connect and nhnet_disconnect, even if
 * the connection is lost due to some network error. */
int nhnet_active(void)
{
    return net_active;
}


int nhnet_connected(void)
{
    return sockfd != -1;
}


/* an error has happened and the connection referred to by sockfd is no longer OK.
 * close the old connection and try to open a new one.
 */
int restart_connection(void)
{
    int ret;
    
    if (sockfd != -1)
	close(sockfd);
    sockfd = -1;
    
    if (in_connect_disconnect)
	return FALSE;
    
    ret = do_connect(saved_hostname, saved_port, saved_username, saved_password,
		     NULL, 0, connection_id);
    if (ret != AUTH_SUCCESS_NEW && ret != AUTH_SUCCESS_RECONNECT)
	return FALSE;
    
    if (ret == AUTH_SUCCESS_NEW && current_game) {
	ret = nhnet_restore_game(current_game, NULL);
	if (ret == GAME_RESTORED)
	    return TRUE;
	return FALSE;
    }
    
    return TRUE;
}

/* connection.c */
