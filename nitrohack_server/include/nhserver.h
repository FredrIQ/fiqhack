#ifndef NHSERVER_H
#define NHSERVER_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <jansson.h>

#include "nitrohack.h"
#include "nitrohack_client.h" /* for enum authresult */

#define DEFAULT_PORT 7114 /* chosen at random, not in use by anything else */

#if !defined(DEFAULT_CONFIG_FILE)
# define DEFAULT_CONFIG_FILE ""
#endif

#if !defined(DEFAULT_LOG_FILE)
# define DEFAULT_LOG_FILE "/var/log/nhserver.log"
#endif

#if !defined(DEFAULT_PID_FILE)
# define DEFAULT_PID_FILE "/var/run/nhserver.pid"
#endif

#if !defined(DEFAULT_WORK_DIR)
# define DEFAULT_WORK_DIR "/var/lib/NitroHack/"
#endif

#if !defined(DEFAULT_CLIENT_TIMEOUT)
# define DEFAULT_CLIENT_TIMEOUT (15 * 60) /* 15 minutes */
#endif


struct settings {
    char *logfile;
    char *workdir;
    char *pidfile;
    struct sockaddr_in  bind_addr_4;
    struct sockaddr_in6 bind_addr_6;
    int port;
    int client_timeout;
    char nodaemon;
    char disable_ipv4;
    char disable_ipv6;
    char *dbhost, *dbname, *dbport, *dbuser, *dbpass;
};


struct user_info {
    char *username;
    int uid;
    int can_debug;
};


struct client_command {
    const char *name;
    void (*func)(json_t *val);
};


struct gamefile_info {
    int gid;
    const char *filename;
    const char *username;
};


/*---------------------------------------------------------------------------*/

extern struct settings settings;
extern struct user_info user_info;
extern struct nh_window_procs server_windowprocs, server_alt_windowprocs;
extern int termination_flag;
extern int gamefd;
extern long gameid;
extern const struct client_command clientcmd[];

/*---------------------------------------------------------------------------*/

/* auth.c */
extern int auth_user(char *authbuf, char *peername, int *is_reg, int *reconnect_id);
extern void auth_send_result(int sockfd, enum authresult, int is_reg, int connid);

/* clientmain.c */
extern void client_main(int userid, int infd, int outfd);
extern void exit_client(const char *err);
extern void client_msg(const char *key, json_t *value);
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
extern int db_register_user(const char *name, const char *pass, const char*email);
extern int db_get_user_info(int uid, struct user_info *info);
extern void db_update_user_ts(int uid);
extern long db_add_new_game(int uid, const char *filename, int role, int race,
			    int gend, int align, int mode, const char *plname);
extern void db_set_game_done(int gameid, int end_how);
extern void db_update_game_ts(int gameid);
extern int db_get_game_filename(int uid, int gid, char *namebuf, int buflen);
extern void db_set_game_filename(int gid, const char *filename);
extern struct gamefile_info *db_list_games(int completed, int uid, int limit, int *count);
extern void db_set_option(int uid, const char *optname, int type, const char *optval);
extern void db_restore_options(int uid);

/* kill.c */
extern int create_pidfile(void);
extern void remove_pidfile(void);
extern void kill_server(void);

/* log.c */
extern void log_msg(const char *fmt, ...);
extern int begin_logging(void);
extern void end_logging(void);
extern void report_startup(void);
extern char *addr2str(const void *sockaddr);

/* miscsetup.c */
extern void setup_signals(void);
extern int init_workdir(void);

/* server.c */
extern int runserver(void);

/* winprocs.c */
extern json_t *get_display_data(void);
extern void reset_cached_diplaydata(void);

#endif
