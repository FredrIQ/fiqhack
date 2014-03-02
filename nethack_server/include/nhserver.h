/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-03-01 */
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

# include "nethack.h"
# include "nethack_client.h"    /* for enum authresult */

# define DEFAULT_PORT 53421     /* different from NitroHack */

/* If using aimake, take directory options from there */
# ifndef STRINGIFY_OPTION
#  define STRINGIFY_OPTION(x) STRINGIFY_OPTION_1(x)
#  define STRINGIFY_OPTION_1(x) #x
# endif

# ifdef AIMAKE_OPTION_configdir
#  define DEFAULT_CONFIG_FILE STRINGIFY_OPTION(AIMAKE_OPTION_configdir) \
    "/nethack4.conf"
# endif
# ifdef AIMAKE_OPTION_logdir
#  define DEFAULT_LOG_FILE STRINGIFY_OPTION(AIMAKE_OPTION_logdir) \
    "/nethack4.log"
# endif
# ifdef AIMAKE_OPTION_lockdir
#  define DEFAULT_PID_FILE STRINGIFY_OPTION(AIMAKE_OPTION_lockdir) \
    "/nethack4.pid"
# endif
# ifdef AIMAKE_OPTION_gamesstatedir
#  define DEFAULT_WORK_DIR STRINGIFY_OPTION(AIMAKE_OPTION_gamesstatedir)
# endif

# if !defined(DEFAULT_CONFIG_FILE)
#  define DEFAULT_CONFIG_FILE ""
# endif

# if !defined(DEFAULT_LOG_FILE)
#  define DEFAULT_LOG_FILE "/var/log/nhserver.log"
# endif

# if !defined(DEFAULT_PID_FILE)
#  define DEFAULT_PID_FILE "/var/run/nhserver.pid"
# endif

# if !defined(DEFAULT_WORK_DIR)
#  define DEFAULT_WORK_DIR "/var/lib/NetHack4/"
# endif

# if !defined(DEFAULT_CLIENT_TIMEOUT)
#  define DEFAULT_CLIENT_TIMEOUT (15 * 60)      /* 15 minutes */
# endif


struct settings {
    char *logfile;
    char *workdir;
    char *pidfile;
    struct sockaddr_in bind_addr_4;
    struct sockaddr_in6 bind_addr_6;
    struct sockaddr_un bind_addr_unix;
    int port;
    int client_timeout;
    char nodaemon;
    char disable_ipv4;
    char disable_ipv6;
    char *dbhost, *dbname, *dbport, *dbuser, *dbpass;
};

# define SUN_PATH_MAX (sizeof(settings.bind_addr_unix.sun_path))


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
    const char *filename;
    const char *username;
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
extern int auth_user(char *authbuf, const char *peername, int *is_reg,
                     int *reconnect_id);
extern void auth_send_result(int sockfd, enum authresult, int is_reg,
                             int connid);

/* clientmain.c */
extern void client_main(int userid, int infd, int outfd);
extern void exit_client(const char *err);
extern void client_msg(const char *key, json_t * value);
extern json_t *read_input(void);

/* config.c */
extern int read_config(char *confname);
extern void setup_defaults(void);
extern void free_config(void);
extern int parse_ip_addr(const char *str, struct sockaddr *out, int want_v4);

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
extern int db_get_game_filename(int uid, int gid, char *namebuf, int buflen);
extern void db_delete_game(int uid, int gid);
extern struct gamefile_info *db_list_games(int completed, int uid, int limit,
                                           int *count);
extern void db_add_topten_entry(int gid, int points, int hp, int maxhp,
                                int deaths, int end_how, const char *death,
                                const char *entrytxt);

/* kill.c */
extern int create_pidfile(void);
extern void remove_pidfile(void);
extern void kill_server(void);
extern void signal_message(void);

/* log.c */
extern void log_msg(const char *fmt, ...);
extern int begin_logging(void);
extern void end_logging(void);
extern void report_startup(void);
extern const char *addr2str(const void *sockaddr);

/* miscsetup.c */
extern void setup_signals(void);
extern int init_workdir(void);
extern int remove_unix_socket(void);

/* server.c */
extern int runserver(void);

/* winprocs.c */
extern json_t *get_display_data(void);
extern void reset_cached_diplaydata(void);
extern void srv_display_buffer(const char *buf, nh_bool trymove);
extern char srv_yn_function(const char *query, const char *rset,
                            char defchoice);


#endif
