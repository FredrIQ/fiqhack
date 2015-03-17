/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-17 */
#ifndef NHSERVER_H
# define NHSERVER_H

/* The server code simply doesn't work on Windows; too many Linux/UNIX-specific
   constructs are used. So mark it as inapplicable. */
# ifdef AIMAKE_BUILDOS_MSWin32
#  undef WIN32
#  define WIN32
# endif

# ifdef WIN32
#  error !AIMAKE_FAIL_SILENTLY! \
    The server code does not currently work on Windows.
# endif


# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <sys/stat.h>
# include <sys/wait.h>
# include <arpa/inet.h>
# include <netinet/tcp.h>
# include <sys/epoll.h>
# include <netinet/in.h>
# include <netdb.h>
# include <poll.h>
# include <fcntl.h>
# include <unistd.h>
# include <errno.h>

# include <jansson.h>

# include "compilers.h"
# include "nethack.h"
# include "nethack_client.h"    /* for enum authresult */


# if !defined(DEFAULT_CLIENT_TIMEOUT)
#  define DEFAULT_CLIENT_TIMEOUT (15 * 60)      /* 15 minutes */
# endif


enum getgame_result {
    GGR_NOT_FOUND,
    GGR_COMPLETED,
    GGR_INCOMPLETE,
};

struct settings {
    char *logfile;
    char *workdir;
    char *pidfile;
    int client_timeout;
    char *dbhost, *dbname, *dbport, *dbuser, *dbpass;
};


struct user_info {
    char *username;
    int uid;
    int can_debug;
};


struct client_command {
    const char *name;
    void (*func) (json_t * val);
    int can_run_async;
};


struct gamefile_info {
    int gid;
    char *filename;
};


/*---------------------------------------------------------------------------*/

extern struct settings settings;
extern struct user_info user_info;
extern struct nh_window_procs server_windowprocs;
extern int termination_flag, sigsegv_flag;
extern int gamefd;
extern long gameid;
extern const struct client_command clientcmd[];
extern struct nh_player_info player_info;

/*---------------------------------------------------------------------------*/

/* auth.c */
extern int auth_user(char *authbuf, int *reconnect_id);
extern void auth_send_result(int sockfd, enum authresult, int is_reg);

/* clientmain.c */
extern noreturn void client_main(int userid, int infd, int outfd);
extern noreturn void exit_client(const char *err, int coredumpsignal);
extern void client_server_cancel_msg(void);
extern void client_msg(const char *key, json_t * value);
extern json_t *read_input(void);
extern void send_string_to_client(const char *jsonstr, int defer_errors);

/* config.c */
extern int read_config(const char *confname);
extern void setup_defaults(void);
extern void free_config(void);

/* db.c */
extern int init_database(void);
extern int check_database(void);
extern void close_database(void);
extern int db_auth_user(const char *name, const char *pass);
extern int db_register_user(const char *name, const char *pass,
                            const char *email);
extern int db_get_user_info(int uid, struct user_info *info);
extern void db_update_user_ts(int uid);
extern int db_set_user_email(int uid, const char *email);
extern int db_set_user_password(int uid, const char *password);
extern long db_add_new_game(int uid, const char *filename, const char *role,
                            const char *race, const char *gend,
                            const char *align, int mode, const char *plname,
                            const char *levdesc);
extern void db_update_game(int gameid, int moves, int depth,
                           const char *levdesc);
extern enum getgame_result db_get_game_filename(
    int gid, char *filenamebuf, int buflen);
extern void db_delete_game(int uid, int gid);
extern struct gamefile_info *db_list_games(int completed, int uid, int limit,
                                           int *count);
extern void db_add_topten_entry(int gid, int points, int hp, int maxhp,
                                int deaths, int end_how, const char *death,
                                const char *entrytxt);

/* log.c */
extern void log_msg(const char *fmt, ...);
extern int begin_logging(void);
extern void end_logging(void);
extern const char *addr2str(const void *sockaddr);

/* miscsetup.c */
extern void setup_signals(void);
extern int init_workdir(void);

/* server.c */
extern noreturn void runserver(void);
extern noreturn void exit_server(int exitstatus, int coredumpsignal);

/* winprocs.c */
extern json_t *get_display_data(void);
extern void reset_cached_displaydata(void);
extern void srv_display_buffer(const char *buf, nh_bool trymove);
extern char srv_yn_function(const char *query, const char *rset,
                            char defchoice);


#endif
