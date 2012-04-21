/* NetHack may be freely redistributed.  See license for details. */

#ifndef NETHACK_CLIENT_H
#define NETHACK_CLIENT_H

#include "nethack_types.h"

#if !defined(STATIC_BUILD)
#if defined (libnethack_client_EXPORTS)/* defined by cmake */
# if defined (_MSC_VER)
#  define EXPORT __declspec(dllexport)
# else /* gcc & clang with -fvisibility=hidden need this for exported syms */
#  define EXPORT __attribute__((__visibility__("default")))
# endif

#else /* building a window port */
# if defined (_MSC_VER)
#  define EXPORT __declspec(dllimport)
# else
#  define EXPORT
# endif
#endif
#else
#define EXPORT
#endif

/* extra return status for nhnet_command and nhnet_restore_game */
#define ERR_NETWORK_ERROR (20000)

enum authresult {
    NO_CONNECTION,
    AUTH_FAILED_UNKNOWN_USER,
    AUTH_FAILED_BAD_PASSWORD,
    AUTH_SUCCESS_NEW,
    AUTH_SUCCESS_RECONNECT
};


struct nhnet_game {
    int gameid;
    enum nh_log_status status;
    struct nh_game_info i;
};


struct nhnet_server_version {
    int major, minor, patchlevel;
};

extern EXPORT struct nhnet_server_version nhnet_server_ver;


extern EXPORT int nhnet_connect(const char *host, int port, const char *user,
				const char *pass, const char *email, int reg_user);
extern EXPORT void nhnet_disconnect(void);
extern EXPORT int nhnet_connected(void);
extern EXPORT int nhnet_active(void);
extern EXPORT struct nhnet_game *nhnet_list_games(int done, int show_all, int *count);

extern EXPORT void nhnet_lib_init(const struct nh_window_procs *);
extern EXPORT void nhnet_lib_exit(void);
extern EXPORT nh_bool nhnet_exit_game(int exit_type);
extern EXPORT int nhnet_restore_game(int gid, struct nh_window_procs *rwinprocs);
extern EXPORT nh_bool nhnet_start_game(const char *name, int role, int race,
				    int gend, int align, enum nh_game_modes playmode);
extern EXPORT int nhnet_command(const char *cmd, int rep, struct nh_cmd_arg *arg);
extern EXPORT const char *const *nhnet_get_copyright_banner(void);
extern EXPORT nh_bool nhnet_view_replay_start(int fd, struct nh_window_procs *rwinprocs,
					   struct nh_replay_info *info);
extern EXPORT nh_bool nhnet_view_replay_step(struct nh_replay_info *info,
					  enum replay_control action, int count);
extern EXPORT void nhnet_view_replay_finish(void);
extern EXPORT struct nh_cmd_desc *nhnet_get_commands(int *count);
extern EXPORT struct nh_cmd_desc *nhnet_get_object_commands(int *count, char invlet);
extern EXPORT struct nh_drawing_info *nhnet_get_drawing_info(void);
extern EXPORT nh_bool nhnet_set_option(const char *name, union nh_optvalue value, nh_bool isstr);
extern EXPORT struct nh_option_desc *nhnet_get_options(enum nh_option_list list);
extern EXPORT const char *nhnet_get_option_string(const struct nh_option_desc *opt);
extern EXPORT void nhnet_describe_pos(int x, int y, struct nh_desc_buf *bufs, int *is_in);
extern EXPORT struct nh_roles_info *nhnet_get_roles(void);
extern EXPORT char *nhnet_build_plselection_prompt(char *, int, int, int, int, int);
extern EXPORT const char *nhnet_root_plselection_prompt(char *, int, int, int, int, int);
extern EXPORT struct nh_topten_entry *nhnet_get_topten(int *out_len, char *statusbuf,
				      const char *player, int top, int around, nh_bool own);
extern EXPORT int nhnet_change_email(const char *email);
extern EXPORT int nhnet_change_password(const char *password);

#undef EXPORT

#if defined(NHNET_TRANSPARENT) && !defined(libnethack_client_EXPORTS)
# define nh_command		nhnet_command
# define nh_exit_game		nhnet_exit_game
# define nh_view_replay_start	nhnet_view_replay_start
# define nh_view_replay_step	nhnet_view_replay_step
# define nh_view_replay_finish	nhnet_view_replay_finish
# define nh_get_commands	nhnet_get_commands
# define nh_get_object_commands	nhnet_get_object_commands
# define nh_get_drawing_info	nhnet_get_drawing_info
# define nh_set_option		nhnet_set_option
# define nh_get_options		nhnet_get_options
# define nh_describe_pos	nhnet_describe_pos
# define nh_get_roles		nhnet_get_roles
# define nh_build_plselection_prompt	nhnet_build_plselection_prompt
# define nh_root_plselection_prompt	nhnet_root_plselection_prompt
# define nh_get_topten		nhnet_get_topten
#endif

#endif
