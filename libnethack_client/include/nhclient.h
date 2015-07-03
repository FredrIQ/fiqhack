/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-07-07 */
#ifndef NHCLIENT_H
# define NHCLIENT_H

# define NETHACK_CLIENT_H_IN_LIBNETHACK_CLIENT
# include "nethack.h"
# include "nethack_client.h"
# include "netconnect.h"

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <ctype.h>
# include <setjmp.h>

# include <jansson.h>

# define DEFAULT_PORT 53430

extern struct nh_window_procs client_windowprocs;
extern int current_game;
extern jmp_buf ex_jmp_buf;
extern int ex_jmp_buf_valid;
extern int conn_err;
extern int error_retry_ok;
extern char saved_password[];

/* connection.c */
extern void print_error(const char *msg);
extern json_t *send_receive_msg(const char *msgtype, json_t * jmsg);
extern int restart_connection(void);

/* netcmd.c */
extern json_t *handle_netcmd(const char *key, const char *expected,
                             json_t *jmsg);
extern void handle_display_list(json_t * display_list);

# define api_entry() \
    (!conn_err && (ex_jmp_buf_valid++ ? 1 : setjmp(ex_jmp_buf) ? 0 : 1))
# define api_exit()  do {--ex_jmp_buf_valid; } while(0)

#endif
