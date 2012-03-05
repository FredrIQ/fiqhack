#ifndef NHCLIENT_H
#define NHCLIENT_H

#include "nitrohack.h"
#include "nitrohack_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <setjmp.h>
#include <sys/types.h>
#if defined UNIX
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <sys/un.h>
#else
#include <Winsock2.h>
#include <Ws2def.h>
#include <Ws2tcpip.h>

# define snprintf(buf, len, fmt, ...) _snprintf_s(buf, len, len-1, fmt, __VA_ARGS__)
# define close closesocket
#endif
#include <jansson.h>

#define DEFAULT_PORT 7116 /* matches the definition in nhserver.h */

extern struct nh_window_procs windowprocs, alt_windowprocs;
extern int current_game;
extern jmp_buf ex_jmp_buf;
extern int ex_jmp_buf_valid;
extern int conn_err;
extern int error_retry_ok;
extern char saved_password[];

/* clientapi.c */
extern void free_option_lists(void);

/* connection.c */
extern void print_error(const char *msg);
extern json_t *send_receive_msg(const char *msgtype, json_t *jmsg);
extern int restart_connection(void);

/* netcmd.c */
extern json_t *handle_netcmd(const char *key, json_t *jmsg);
extern void handle_display_list(json_t *display_list);

/* xmalloc.c */
extern void *xmalloc(int size);
extern void xmalloc_cleanup(void);

#define api_entry() \
    (!conn_err && (ex_jmp_buf_valid++ ? 1 : setjmp(ex_jmp_buf) ? 0 : 1))
#define api_exit()  do {--ex_jmp_buf_valid; } while(0)

#endif