/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

/*
 * NetHack server core.
 * 
 * The code is designed to meet the following goals/constraints:
 *  - Each NetHack game MUST run in it's own process. The game is the
 *    anti-thesis of thread-safe. Writing a threaded server would require major
 *    work on the game code.
 * 
 *  - Clients should be able to disconnect from the server between commands.
 *    This is required to more easily support web clients. In that scenario
 *    Some javascript in a browser issues an XMLHttpRequest to a web-server,
 *    where a script performs the trivial transformation from request parameters
 *    to a JSON string which it sends on (via localhost) to the NetHack server.
 *    The NH server responds with some more JSON which represents game state.
 *    Since the request is now complete from their point of view, neither the
 *    web server nor the web client should be required to maintain a connection
 *    to the NH server.
 * 
 * In order to allow a client to reconnect to its running game, there are 2
 * alternatives:
 *   (1) performing each client connection over a new server socket on a
 *       different port.
 *   (2) forwarding the client connection from the master process which owns
 *       the listening socket to a client process which handles the game state.
 * Option (1) has been tried in FTP; this design is widely considered to be a
 * bad idea.
 * 
 * The design of the NH server was based on alternative (2):
 * The server listens for incoming connections.
 * When a connection is made, the client is allowed to send enough data to
 * authenticate.
 * Each authenticated client has its own game process.
 * Communication with this game process is always handled by the main server
 * process: data to and from the client is passed across a set of anonymous
 * pipes.
 * These pipes remain open when the client disconnects from the server; the
 * game process doesn't even notice the disconnection (it may time out from
 * inactivity, though).
 * When the connection is re-established, the client's requests get forwarded
 * again.
 */

#include "nhserver.h"

#include <ctype.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/time.h>

#if defined(OPEN_MAX)
static int
get_open_max(void)
{
    return OPEN_MAX;
}
#else
static int
get_open_max(void)
{
    return sysconf(_SC_OPEN_MAX);
}
#endif

/* How many epoll events do we want to process in one go? No idea, actually!
 * 16 seems like a reasonable value for now... */
#define MAX_EVENTS 16

/* a username + password with some fluff should always fit in 500 bytes */
#define AUTH_MAXLEN 500
/* make the buffer slightly bigger to detect when the client sends too much data */
#define AUTHBUFSIZE 512

enum comm_status {
    NEW_CONNECTION,
    CLIENT_DISCONNECTED,
    CLIENT_CONNECTED
};

/* Client communication data.
 * This structure tracks connected clients, ie remote users with open sockets
 * and also "disconnected clients", which refer to local games that continue to
 * run while waiting for the owner to reconnect and issue more commands.
 * 
 * Theoretically the auth info for new sockets could be split out, but it would
 * cause all sorts of headaches and saving a few bytes just isn't worth it. */
struct client_data {
    enum comm_status state;
    int pid;
    int userid; /* owner of this game */
    int connid;
    struct client_data *prev, *next;
    int pipe_out;       /* master -> game pipe */
    int pipe_in;        /* game -> master pipe */
    int sock;   /* master <-> client socket */
    int unsent_data_size;
    char *unsent_data;
};


/*---------------------------------------------------------------------------*/

static struct client_data new_connection_dummy = { NEW_CONNECTION, 0 
                                                   /* , 0 etc */  };

/* disconnected_list_head: list of games which are fully established, but the
 * client has disconnected. The client can reconnect to the running game later.
 * in these client_data structures, sockfd will be -1, but everything else is valid.*/
static struct client_data disconnected_list_head;

/* connected_list_head: list of games which are fully established and have a
 * connected client. */
static struct client_data connected_list_head;

static struct client_data **fd_to_client;
static int client_count, fd_to_client_max;

/*---------------------------------------------------------------------------*/


static void cleanup_game_process(struct client_data *client, int epfd);
static int init_server_socket(struct sockaddr *sa);
static int fork_client(struct client_data *client, int epfd);
static void handle_new_connection(int newfd, int epfd);


static void
link_client_data(struct client_data *client, struct client_data *list)
{
    client->prev = list;
    client->next = list->next;
    list->next = client;
    if (client->next)
        client->next->prev = client;

    client_count++;
}

static void
unlink_client_data(struct client_data *client)
{
    if (client->prev)
        client->prev->next = client->next;
    if (client->next)
        client->next->prev = client->prev;

    client_count--;
}

static struct client_data *
alloc_client_data(struct client_data *list_start)
{
    struct client_data *client = malloc(sizeof (struct client_data));

    memset(client, 0, sizeof (struct client_data));
    link_client_data(client, list_start);
    client->sock = client->pipe_in = client->pipe_out = -1;

    return client;
}


static void
map_fd_to_client(int fd, struct client_data *client)
{
    int size;

    while (fd >= fd_to_client_max) {
        size = fd_to_client_max * sizeof (struct client_data *);
        fd_to_client = realloc(fd_to_client, 2 * size);
        memset(&fd_to_client[fd_to_client_max], 0, size);
        fd_to_client_max *= 2;
    }
    fd_to_client[fd] = client;
}


/* full setup for both ipv4 and ipv6 server sockets */
static int
init_server_socket(struct sockaddr *sa)
{
    int fd, opt_enable = 1, defer_seconds = 20;
    socklen_t len;

    fd = socket(sa->sa_family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        log_msg("Error creating server socket: %s", strerror(errno));
        return -1;
    }

    /* Enable fast address re-use. Nice to have during development, probably
       irrelevant otherwise. */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof (int));

    switch (sa->sa_family) {
    case AF_INET:
        len = sizeof (struct sockaddr_in);
        break;
    case AF_INET6:
        len = sizeof (struct sockaddr_in6);
        /* Setting the IPV6_V6ONLY socket option allows the ipv6 socket to bind 
           to the same port as the ipv4 socket. Using one socket for each
           protocol is better than using one ipv4-compatible ipv6 socket, as
           that limits the possible ipv6 addresses to ipv4 compatible ones. */
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt_enable, sizeof (int))
            == -1)
            log_msg("Failed to set the IPV6_V6ONLY socket option: %s.",
                    strerror(errno));
        break;
    case AF_UNIX:
        len = sizeof (struct sockaddr_un);
        break;

    default:   /* shouldn't ever happen */
        close(fd);
        return -1;
    }

    if (sa->sa_family != AF_UNIX) {
        /* don't accept client connections until there is data */
        if (setsockopt
            (fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &defer_seconds,
             sizeof (int)) == -1)
            log_msg("Failed to set the DEFER_ACCEPT socket option: %s.",
                    strerror(errno));
    }

    if (bind(fd, sa, len) == -1) {
        close(fd);
        log_msg("Error binding server socket (%s): %s", addr2str(sa),
                strerror(errno));
        return -1;
    }

    if (listen(fd, 16) == -1) {
        close(fd);
        log_msg("Error listening on server socket (%s): %s", addr2str(sa),
                strerror(errno));
        return -1;
    }

    return fd;
}


/*
 * The client inherits sevaral things that aren't needed to run a game
 * Free them here.
 */
static void
post_fork_cleanup(void)
{
    int i;
    struct client_data *ccur, *cnext;

    /* forking doesn't actually close any of the CLOEXEC file descriptors.
       CLOEXEC is still nice to have and we can use it as a flag to get rid of
       lots of stuff here. */
    for (i = 0; i < get_open_max(); i++)
        if (fcntl(i, F_GETFD) & FD_CLOEXEC)
            close(i);

    for (ccur = disconnected_list_head.next; ccur; ccur = cnext) {
        cnext = ccur->next;
        free(ccur);
    }

    for (ccur = connected_list_head.next; ccur; ccur = cnext) {
        cnext = ccur->next;
        free(ccur);
    }

    free(fd_to_client);
}


/*
 * A new game process is needed.
 * Create the communication pipes, register them with epoll and fork the new
 * process.
 */
static int
fork_client(struct client_data *client, int epfd)
{
    int ret1, ret2, userid;
    int pipe_out_fd[2];
    int pipe_in_fd[2];
    struct epoll_event ev;

    ret1 = pipe2(pipe_out_fd, O_NONBLOCK);
    ret2 = pipe2(pipe_in_fd, O_NONBLOCK);
    if (ret1 == -1 || ret2 == -1) {
        if (!ret1) {
            close(pipe_out_fd[0]);
            close(pipe_out_fd[1]);
        }
        /* it's safe to use errno here, even though the second pipe2 call will
           erase the status from the first, because the second one will always
           fail with the same status as the first if the first call fails. */
        log_msg("Failed to create communication pipes for new connection: %s",
                strerror(errno));
        cleanup_game_process(client, epfd);
        return FALSE;
    }

    /* pipe[0] read side - pipe[1] write side */
    fcntl(pipe_in_fd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipe_out_fd[1], F_SETFD, FD_CLOEXEC); /* client does not need to
                                                   inherit this */

    client->pipe_out = pipe_out_fd[1];
    client->pipe_in = pipe_in_fd[0];
    map_fd_to_client(client->pipe_out, client);
    map_fd_to_client(client->pipe_in, client);

    client->pid = fork();
    if (client->pid > 0) {      /* parent */
    } else if (client->pid == 0) {      /* child */
        struct user_info info;

        userid = client->userid;
        db_get_user_info(userid, &info);
        setenv("NH4SERVERUSER", info.username, 1);
        post_fork_cleanup();
        client_main(userid, pipe_out_fd[0], pipe_in_fd[1]);
        exit(0);        /* shouldn't get here... client is done. */
    } else if (client->pid == -1) {     /* error */
        /* can't proceed, so clean up. The client side of the pipes needs to be
           closed here, this end gets handled in cleanup_game_process */
        close(pipe_out_fd[0]);
        close(pipe_in_fd[1]);
        cleanup_game_process(client, epfd);
        log_msg("Failed to fork a client process: %s", strerror(errno));
        return FALSE;
    }

    client->state = CLIENT_CONNECTED;
    unlink_client_data(client);
    link_client_data(client, &connected_list_head);

    /* register the pipe fds for monitoring by epoll */
    ev.data.ptr = NULL;
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    ev.data.fd = client->pipe_out;
    epoll_ctl(epfd, EPOLL_CTL_ADD, client->pipe_out, &ev);
    ev.data.fd = client->pipe_in;
    epoll_ctl(epfd, EPOLL_CTL_ADD, client->pipe_in, &ev);

    /* close the client side of the pipes */
    close(pipe_out_fd[0]);
    close(pipe_in_fd[1]);

    return TRUE;
}


/*
 * Accept and authenticate a new client connection on one of the listening sockets.
 */
static void
server_socket_event(int server_fd, int epfd)
{
    struct epoll_event ev;
    struct sockaddr_storage addr;
    int newfd, opt_enable = 1;
    socklen_t addrlen = sizeof (addr);

    memset(&addr, 0, addrlen);
    newfd =
        accept4(server_fd, (struct sockaddr *)&addr, &addrlen,
                SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (newfd == -1)    /* maybe the connection attempt was aborted early? */
        return;

    /* no need to complain if this setsockopt fails; TCP_NODELAY doesn't exist
       for AF_UNIX sockets. */
    setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &opt_enable, sizeof (int));

    if (addr.ss_family == AF_UNIX) {
        /* unix sockets don't have defer_accept, so the auth data might not be
           ready yet. Additionally, as the socket is local, it gets a higher
           level of trust. */
        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
        ev.data.ptr = NULL;
        ev.data.fd = newfd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev) == -1) {
            log_msg("Error in epoll_ctl for %s: %s", addr2str(&addr),
                    strerror(errno));
            close(newfd);
            return;
        }
        map_fd_to_client(newfd, &new_connection_dummy);
        return;
    } else
        handle_new_connection(newfd, epfd);
}


static void
handle_new_connection(int newfd, int epfd)
{
    struct epoll_event ev;
    struct client_data *client;
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof (addr);
    char authbuf[AUTHBUFSIZE];
    int pos, is_reg, reconnect_id, authlen, userid;
    static int connection_id = 1;

    if (fd_to_client_max > newfd &&
        fd_to_client[newfd] == &new_connection_dummy) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, newfd, &ev);
        fd_to_client[newfd] = NULL;
    }

    /* it should be possible to read immediately due to the "defer" sockopt */
    authlen = read(newfd, authbuf, AUTHBUFSIZE - 1);
    if (authlen <= 0) {
        close(newfd);
        return;
    }

    getpeername(newfd, (struct sockaddr *)&addr, &addrlen);
    log_msg("New connection from %s.", addr2str(&addr));

    authbuf[authlen] = '\0';    /* make it safe to use as a string */

    /* did we receive too much data? */
    if (authlen >= AUTH_MAXLEN) {
        log_msg("Auth buffer overrun attempt from %s? Peer disconnected.",
                addr2str(&addr));
        close(newfd);
        return;
    }

    /* check the end of the received auth data: a JSON object always ends with
       '}' */
    pos = authlen - 1;
    while (pos > 0 && isspace(authbuf[pos]))
        pos--;

    if (authbuf[pos] != '}') {  /* not the end of JSON auth data */
        log_msg("authentication for %s failed due to incomplete JSON",
                addr2str(&addr));
        return;
    }

    /* 
     * ready to authenticate the user here
     */
    userid = auth_user(authbuf, addr2str(&addr), &is_reg, &reconnect_id);
    if (userid <= 0) {
        if (!userid)
            auth_send_result(newfd, AUTH_FAILED_UNKNOWN_USER, is_reg, 0);
        else
            auth_send_result(newfd, AUTH_FAILED_BAD_PASSWORD, is_reg, 0);
        log_msg("authentication failed for %s", addr2str(&addr));
        close(newfd);
        return;
    }

    /* user ok, we'll keep this socket */
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    ev.data.ptr = NULL;
    ev.data.fd = newfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &ev) == -1) {
        log_msg("Error in epoll_ctl for %s: %s", addr2str(&addr),
                strerror(errno));
        close(newfd);
        return;
    }

    /* is the client re-establishing a connection to an existing, disconnected
       game? */
    for (client = disconnected_list_head.next; client; client = client->next)
        if (client->userid == userid &&
            (!reconnect_id || reconnect_id == client->connid))
            break;
    if (reconnect_id && !client) {
        /* now search through the active connections. The client might have a
           new IP address, which would leave the socket open and seemingly
           valid. */
        for (client = connected_list_head.next; client; client = client->next)
            if (client->userid == userid && reconnect_id == client->connid)
                break;
    }

    if (client) {
        /* there is a running, disconnected game process for this user */
        auth_send_result(newfd, AUTH_SUCCESS_RECONNECT, is_reg, client->connid);
        client->sock = newfd;
        map_fd_to_client(client->sock, client);
        client->state = CLIENT_CONNECTED;
        unlink_client_data(client);
        link_client_data(client, &connected_list_head);
        write(client->pipe_out, "\033", 1);     /* signal to reset the read
                                                   buffer */

        log_msg("Connection to game at pid %d reestablished for user %d",
                client->pid, client->userid);
        return;
    } else {
        client = alloc_client_data(&connected_list_head);
        client->state = CLIENT_CONNECTED;
        client->sock = newfd;
        map_fd_to_client(newfd, client);
        client->connid = connection_id++;
        client->userid = userid;
        /* there is no process yet */
        if (fork_client(client, epfd))
            auth_send_result(newfd, AUTH_SUCCESS_NEW, is_reg, client->connid);
        /* else: client communication is shutdown if fork_client errors out */
    }

    log_msg("There are now %d clients on the server", client_count);
}


/*
 * completely free a client_data struct and all its pointers
 */
static void
cleanup_game_process(struct client_data *client, int epfd)
{
    /* if the client didn't get a signal yet, send it one now. */
    if (client->pid)
        kill(client->pid, SIGTERM);

    /* close all file descriptors and free data structures */
    if (client->sock != -1) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->sock, NULL);
        shutdown(client->sock, SHUT_RDWR);
        close(client->sock);
        fd_to_client[client->sock] = NULL;
    }

    if (client->pipe_out != -1) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->pipe_out, NULL);
        close(client->pipe_out);
        fd_to_client[client->pipe_out] = NULL;
    }

    if (client->pipe_in != -1) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->pipe_in, NULL);
        close(client->pipe_in);
        fd_to_client[client->pipe_in] = NULL;
    }

    if (client->unsent_data)
        free(client->unsent_data);

    client->pipe_in = client->pipe_out = client->sock = -1;
    unlink_client_data(client);
    free(client);

    log_msg("There are now %d clients on the server", client_count);
}


/*
 * A game process closed one or both of its communication pipes. This only
 * happens when the process is about to exit, because doing so cuts it off from
 * the outside world.
 * This means the server side of both pipes and the client socket can be
 * closed, too.
 */
static void
close_client_pipe(struct client_data *client, int epfd)
{
    if (client->pipe_in != -1) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->pipe_in, NULL);
        close(client->pipe_in);
        fd_to_client[client->pipe_in] = NULL;
        client->pipe_in = -1;
    }

    if (client->pipe_out != -1) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->pipe_out, NULL);
        close(client->pipe_out);
        fd_to_client[client->pipe_out] = NULL;
        client->pipe_out = -1;
    }


    if (client->sock)
        /* allow a send to complete (incl retransmits). close() is too brutal. */
        shutdown(client->sock, SHUT_RDWR);
    else {
        /* don't try to send a signal in cleanup_game_process - the process may 
           be gone already */
        client->pid = 0;
        cleanup_game_process(client, epfd);
    }
}


static int
send_to_client(struct client_data *client, char *buffer, int sendlen)
{
    int sent, ret;
    char *newbuf;

    if (!buffer) {
        buffer = client->unsent_data;
        sendlen = client->unsent_data_size;
        if (!buffer)
            return 0;
    }

    sent = 0;
    do {
        ret = write(client->sock, &buffer[sent], sendlen - sent);
        if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* be careful, buffer == client->unsent_data is possible */
            newbuf = malloc(sendlen - sent);
            memcpy(newbuf, &buffer[sent], sendlen - sent);
            if (client->unsent_data)
                free(client->unsent_data);
            client->unsent_data = newbuf;
            client->unsent_data_size = sendlen - sent;
            return sent;
        } else if (ret == -1 && errno == EPIPE) {
            shutdown(client->sock, SHUT_RDWR);
            return -1;
        } else if (ret == -1 && errno != EINTR)
            return -1;
        sent += ret;
    } while (sent < sendlen);

    if (client->unsent_data)
        free(client->unsent_data);
    client->unsent_data = NULL;
    client->unsent_data_size = 0;

    return sent;
}


/*
 * handle an epoll event for a fully esablished communication channel, where
 * client->sock, client->pipe_in an client->pipe->out all exist.
 * 
 * The function determines what happened to which file descriptor and passes
 * data around accordingly.
 */
static void
handle_communication(int fd, int epfd, unsigned int event_mask)
{
    int ret, closed, write_count, errno_orig, read_ret, write_ret;
    struct client_data *client = fd_to_client[fd];
    struct epoll_event ev;
    char buf[16384];

    if (event_mask & EPOLLERR ||        /* fd error */
        event_mask & EPOLLHUP ||        /* fd closed */
        event_mask & EPOLLRDHUP)        /* fd closed */
        closed = TRUE;
    else
        closed = FALSE;

    if (fd == client->sock) {
        if (closed) {   /* peer gone. goodbye. */
            epoll_ctl(epfd, EPOLL_CTL_DEL, client->sock, NULL);
            close(client->sock);
            fd_to_client[client->sock] = NULL;
            client->sock = -1;
            if (client->pipe_in != -1 && client->pipe_out != -1) {
                log_msg("User %d has disconnected from a game", client->userid);
                client->state = CLIENT_DISCONNECTED;
                unlink_client_data(client);
                link_client_data(client, &disconnected_list_head);

                /* Maybe the destination vanished before sending completed...
                   unsent_data is likely to be an incomplete JSON object;
                   deleting it is the only sane option. */
                if (client->unsent_data)
                    free(client->unsent_data);
                client->unsent_data = NULL;
                client->unsent_data_size = 0;
            } else {
                log_msg("Shutdown completed for game at pid %d", client->pid);
                client->pid = 0;
                cleanup_game_process(client, epfd);
            }
        } else {        /* it is possible to receive or send data */
            if (event_mask & EPOLLIN) {
                do {
                    write_ret = -2;
                    read_ret = read(client->sock, buf, sizeof (buf));
                    if (read_ret == -1 && errno == EINTR)
                        continue;
                    else if (read_ret <= 0)
                        break;
                    write_count = 0;
                    do {
                        write_ret =
                            write(client->pipe_out, &buf[write_count],
                                  read_ret - write_count);
                        if (write_ret == -1 && errno == EINTR)
                            continue;
                        else if (write_ret == -1)
                            break;
                        write_count += write_ret;
                    } while (write_count < read_ret);
                } while (read_ret == sizeof (buf) && write_ret != -1);
                if (read_ret <= 0 || write_ret == -1) {
                    log_msg
                        ("data transfer error for game process %d (read = %d, write = %d): %s",
                         client->pid, read_ret, write_ret, strerror(errno));
                    cleanup_game_process(client, epfd);
                }
            }
            if ((event_mask & EPOLLOUT) && client->unsent_data) {
                write_count = send_to_client(client, NULL, 0);
                if (write_count == -1)
                    log_msg("error while sending: %s", strerror(errno));

                /* re-arm pipe_in notification: there may be more data to send
                   in the pipe for which an event was already received but not
                   acted upon */
                ev.data.ptr = NULL;
                ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
                ev.data.fd = client->pipe_in;
                epoll_ctl(epfd, EPOLL_CTL_MOD, client->pipe_in, &ev);
            }
        }

    } else if (fd == client->pipe_in) {
        if (closed)
            close_client_pipe(client, epfd);

        else {  /* there is data to send */
            if (client->unsent_data)
                return; /* socket isn't ready for sending */

            /* oddity alert: this code originally used splice for sending. That 
               would match the receive case above and no buffer would be
               required. Unfortunately sending that way is significantly
               slower. splice: 200ms - read+write: 0.2ms! Ouch! */
            do {
                /* read from the pipe */
            redo:
                ret = read(client->pipe_in, buf, sizeof (buf));
                errno_orig = errno;
                if (ret == -1 && errno == EINTR)
                    goto redo; /* not "continue", we don't want to check the
                                  loop condition */
                else if (ret > 0) {
                    /* write to the socket; be careful to write all of it... */
                    write_count = send_to_client(client, buf, ret);
                    /* write_count != ret if no more data could be sent and the
                       remainder buf was copied to client->unsent_data for later
                       sending */
                } else write_count = ret;
            } while (ret == write_count && ret == sizeof (buf));
            if (ret == -1)
                log_msg("error while reading from pipe: %s",
                        strerror(errno_orig));
            if (write_count == -1)
                log_msg("error while sending: %s", strerror(errno));
        }

    } else if (fd == client->pipe_out) {
        if (closed)
            close_client_pipe(client, epfd);

        else
            /* closed == FALSE doesn't happen for this fd: it's the write side,
               so there should NEVER be anything to read */
            log_msg("Impossible: data readable on a write pipe?!?");
    }
}


static int
setup_server_sockets(int *ipv4fd, int *ipv6fd, int *unixfd, int epfd)
{
    struct epoll_event ev;

    ev.data.ptr = NULL;
    ev.events = EPOLLIN | EPOLLET;      /* epoll_wait waits for EPOLLERR and
                                           EPOLLHUP as well */

    if (!settings.disable_ipv6) {
        settings.bind_addr_6.sin6_port = htons((unsigned short)settings.port);
        *ipv6fd = init_server_socket((struct sockaddr *)&settings.bind_addr_6);
        ev.data.fd = *ipv6fd;
        if (*ipv6fd != -1)
            epoll_ctl(epfd, EPOLL_CTL_ADD, *ipv6fd, &ev);
    } else
        *ipv6fd = -1;

    if (!settings.disable_ipv4) {
        settings.bind_addr_4.sin_port = htons((unsigned short)settings.port);
        *ipv4fd = init_server_socket((struct sockaddr *)&settings.bind_addr_4);
        ev.data.fd = *ipv4fd;
        if (*ipv4fd != -1)
            epoll_ctl(epfd, EPOLL_CTL_ADD, *ipv4fd, &ev);
    } else
        *ipv4fd = -1;

    if (settings.bind_addr_unix.sun_family && remove_unix_socket()) {
        int prevmask = umask(0);

        *unixfd =
            init_server_socket((struct sockaddr *)&settings.bind_addr_unix);
        ev.data.fd = *unixfd;
        if (*unixfd != -1)
            epoll_ctl(epfd, EPOLL_CTL_ADD, *unixfd, &ev);
        umask(prevmask);
    }

    if (*ipv4fd == -1 && *ipv6fd == -1) {
        log_msg
            ("Failed to create any listening socket. Nothing to do except shut down.");
        return FALSE;
    }

    return TRUE;
}


/*
 * The signal handler for SIGTERM and SIGINT ran and set termination_flag to 1
 * to indicate the shutdown request.
 * In response, this function is called from the epoll event loop to close the
 * server sockets and record the shutdown request time.
 */
static int
trigger_server_shutdown(struct timeval *tv, int *ipv4fd, int *ipv6fd,
                        int *unixfd)
{
    termination_flag = 2;

    gettimeofday(tv, NULL);
    log_msg("Shutdown request received; %d clients active.", client_count);
    if (*ipv4fd != -1) {
        close(*ipv4fd);
        *ipv4fd = -1;
    }
    if (*ipv6fd != -1) {
        close(*ipv6fd);
        *ipv6fd = -1;
    }
    if (*unixfd != -1) {
        close(*unixfd);
        *unixfd = -1;
    }
    if (client_count) {
        log_msg("Server sockets closed, will wait 5 seconds "
                "for clients to shut down.");
        /* because termination_flag is now set, the epoll_wait timeout * will
           be adjusted based on sigtime */
        return FALSE;
    } else
        return TRUE;
}


/*
 * The server's core. Creates the configured listening sockets and then
 * enters the server event loop from which all clients are served.
 */
int
runserver(void)
{
    int i, ipv4fd, ipv6fd, unixfd, epfd, nfds, timeout, fd, childstatus;
    struct epoll_event events[MAX_EVENTS];
    struct client_data *client;
    struct timeval sigtime, curtime, tmp;

    fd_to_client_max = 64;      /* will be doubled every time it becomes too
                                   small */
    fd_to_client = malloc(fd_to_client_max * sizeof (struct client_data *));

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        log_msg("Error in epoll_create1");
        return FALSE;
    }

    if (!setup_server_sockets(&ipv4fd, &ipv6fd, &unixfd, epfd))
        return FALSE;

    /* 
     * server event loop
     */
    while (1) { /* loop exit via "goto finally" */
        timeout = 10 * 60 * 1000;
        if (termination_flag) {
            if (termination_flag == 1)  /* signal didn't interrupt epoll_wait */
                trigger_server_shutdown(&sigtime, &ipv4fd, &ipv6fd, &unixfd);
            gettimeofday(&curtime, NULL);
            /* calculate the elapsed time since the quit request */
            timersub(&curtime, &sigtime, &tmp);
            timeout = 5000 - (1000 * tmp.tv_sec) - (tmp.tv_usec / 1000);
            if (timeout < 0)
                timeout = 0;
        }

        /* make sure child processes are cleaned up */
        waitpid(-1, &childstatus, WNOHANG);

        nfds = epoll_wait(epfd, events, MAX_EVENTS, timeout);
        if (nfds == -1) {
            if (errno != EINTR) {       /* serious problem */
                log_msg("Error from epoll_wait in main event loop: %s",
                        strerror(errno));
                goto finally;
            }

            /* some signal - SIGTERM? */
            if (!termination_flag)
                continue;

            /* begin server shutdown sequence */
            if (!trigger_server_shutdown(&sigtime, &ipv4fd, &ipv6fd, &unixfd))
                continue;
            else
                goto finally;
        } else if (nfds == 0) { /* timeout */
            if (!termination_flag)
                log_msg(" -- mark (no activity for 10 minutes) --");
            else        /* shutdown timer has run out */
                goto finally;
            continue;
        }

        for (i = 0; i < nfds; i++) {
            fd = events[i].data.fd;

            if (fd == ipv4fd || fd == ipv6fd || fd == unixfd) {
                /* server socket ready for accept */
                server_socket_event(fd, epfd);
                continue;
            }

            /* activity on a client socket or pipe */
            client = fd_to_client[fd];
            /* was this fd closed while handling a prior event? */
            if (!client)
                continue;

            switch (client->state) {
            case NEW_CONNECTION:
                if (events[i].events & EPOLLERR ||      /* error */
                    events[i].events & EPOLLHUP ||      /* connection closed */
                    events[i].events & EPOLLRDHUP)      /* connection closed */
                    close(fd);
                else if (events[i].events & EPOLLIN)
                    handle_new_connection(fd, epfd);
                break;

            case CLIENT_DISCONNECTED:
                /* When the client is disconnected, activity usually only
                   happens on the pipes: either the game process is closing
                   them because the idle timeout expired or shutdown was
                   requested via a signal. */
                if (events[i].events & EPOLLERR ||      /* error */
                    events[i].events & EPOLLHUP ||      /* connection closed */
                    events[i].events & EPOLLRDHUP)      /* connection closed */
                    cleanup_game_process(client, epfd);
                else if (events[i].events & EPOLLIN) {
                    /* Perhaps the game process was just writing data to the
                       pipe when the client disconnected. There is nothing we
                       can do with this data here, but we don't want to kill
                       the game either, so just read and discard the data. */
                    char buf[2048];
                    int ret;

                    do {
                        ret = read(fd, buf, 2048);
                    } while (ret == 2048);
                }
                break;

            case CLIENT_CONNECTED:
                handle_communication(fd, epfd, events[i].events);
                break;
            }
            if (termination_flag && client_count == 0)
                goto finally;
        }       /* for */
    }   /* while(1) */

finally:
    while (disconnected_list_head.next)
        cleanup_game_process(disconnected_list_head.next, epfd);
    while (connected_list_head.next)
        cleanup_game_process(connected_list_head.next, epfd);

    close(epfd);
    if (ipv4fd != -1)
        close(ipv4fd);
    if (ipv6fd != -1)
        close(ipv6fd);
    if (unixfd != -1)
        close(unixfd);
    free(fd_to_client);

    return TRUE;
}

/* server.c */
