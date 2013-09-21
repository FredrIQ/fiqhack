/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
#ifndef NHCLIENT_H
# define NHCLIENT_H

# define NETHACK_CLIENT_H_IN_LIBNETHACK_CLIENT
# include "nethack.h"
# include "nethack_client.h"

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <ctype.h>
# include <setjmp.h>
# include <sys/types.h>

/* We don't have nethack_types to help us out with OS detection when
   calculating dependencies, so redo the calculation here. */
# if defined(UNIX) || (!defined(WIN32) && !defined(AIMAKE_BUILDOS_MSWin32))
#  include <unistd.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <netdb.h>
#  include <sys/un.h>
# else
#  include <Winsock2.h>
#  include <Ws2def.h>
#  include <Ws2tcpip.h>

#  define snprintf(buf, len, fmt, ...) _snprintf_s(buf, len, len-1, fmt, __VA_ARGS__)
#  define close closesocket

/* TODO: Find the header file this is defined in, and include it. */
extern int _snprintf_s(char *, size_t, size_t, const char *, ...);

# endif
# include <jansson.h>

# define DEFAULT_PORT 53421     /* matches the definition in nhserver.h */

extern struct nh_window_procs AIMAKE_REVERSE_EXPORT(windowprocs);
extern struct nh_window_procs alt_windowprocs;
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
extern json_t *send_receive_msg(const char *msgtype, json_t * jmsg);
extern int restart_connection(void);

/* netcmd.c */
extern json_t *handle_netcmd(const char *key, json_t * jmsg);
extern void handle_display_list(json_t * display_list);

/* xmalloc.c */
extern void *xmalloc(int size);
extern void xmalloc_cleanup(void);

# define api_entry() \
    (!conn_err && (ex_jmp_buf_valid++ ? 1 : setjmp(ex_jmp_buf) ? 0 : 1))
# define api_exit()  do {--ex_jmp_buf_valid; } while(0)

#endif
