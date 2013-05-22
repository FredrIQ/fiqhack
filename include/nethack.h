/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* NetHack may be freely redistributed.  See license for details. */

/* this header defines the interface between libnethack and window ports
 * it should never include any private headers, as those are not on the
 * search path for window ports. */

#ifndef NETHACK_H
# define NETHACK_H

/* NetHack 4.2.1. Keep this consistent with patchlevel.h. */
/* (Note that the number in patchlevel.h may be older than the number
   here, if save/bones compatibility is not broken between versions.) */
# define VERSION_MAJOR  4
# define VERSION_MINOR  2
# define PATCHLEVEL     1

# include "nethack_types.h"

# if !defined(STATIC_BUILD)
#  if defined (libnethack_EXPORTS)      /* defined by cmake while building
                                           libnethack */
#   if defined (_MSC_VER)
#    define EXPORT __declspec(dllexport)
#   else
      /* gcc & clang with -fvisibility=hidden need this for exported syms */
#    define EXPORT __attribute__((__visibility__("default")))
#   endif

#  else
      /* building a window port */
#   if defined (_MSC_VER)
#    define EXPORT __declspec(dllimport)
#   else
#    define EXPORT
#   endif
#  endif
# else
#  define EXPORT
# endif

/* allmain.c */
extern EXPORT void nh_lib_init(const struct nh_window_procs *, char **paths);
extern EXPORT void nh_lib_exit(void);
extern EXPORT nh_bool nh_exit_game(int exit_type);
extern EXPORT enum nh_restore_status nh_restore_game(int fd, struct nh_window_procs
                                                     *rwinprocs,
                                                     nh_bool force_replay);

extern EXPORT nh_bool nh_start_game(int fd, const char *name, int role,
                                    int race, int gend, int align,
                                    enum nh_game_modes playmode);
extern EXPORT int nh_command(const char *cmd, int rep, struct nh_cmd_arg *arg);
extern EXPORT const char *const *nh_get_copyright_banner(void);

/* logreplay.c */
extern EXPORT nh_bool nh_view_replay_start(int fd,
                                           struct nh_window_procs *rwinprocs,
                                           struct nh_replay_info *info);
extern EXPORT nh_bool nh_view_replay_step(struct nh_replay_info *info,
                                          enum replay_control action,
                                          int count);
extern EXPORT void nh_view_replay_finish(void);
extern EXPORT enum nh_log_status nh_get_savegame_status(
  int fd, struct nh_game_info *si);

/* cmd.c */
extern EXPORT struct nh_cmd_desc *nh_get_commands(int *count);
extern EXPORT struct nh_cmd_desc *nh_get_object_commands(int *count,
                                                         char invlet);

/* drawing.c */
extern EXPORT struct nh_drawing_info *nh_get_drawing_info(void);

/* options.c */
extern EXPORT nh_bool nh_set_option(const char *name, union nh_optvalue value,
                                    nh_bool isstr);
extern EXPORT struct nh_option_desc *nh_get_options(enum nh_option_list list);
extern EXPORT void nh_setup_ui_options(
  struct nh_option_desc *options, struct nh_boolopt_map *boolmap,
  nh_bool(*callback) (struct nh_option_desc *));
extern EXPORT const char *nh_get_option_string(
  const struct nh_option_desc *opt);

/* pager.c */
extern EXPORT void nh_describe_pos(int x, int y, struct nh_desc_buf *bufs,
                                   int *is_in);

/* role.c */
extern EXPORT struct nh_roles_info *nh_get_roles(void);
extern EXPORT char *nh_build_plselection_prompt(char *, int, int, int, int,
                                                int);
extern EXPORT const char *nh_root_plselection_prompt(char *, int, int, int, int,
                                                     int);

/* topten.c */
extern EXPORT struct nh_topten_entry *nh_get_topten(int *out_len,
                                                    char *statusbuf,
                                                    const char *player, int top,
                                                    int around, nh_bool own);

# undef EXPORT

# define set_menuitem(it, i, r, cap, acc, sel) \
do {\
    struct nh_menuitem *_item_ = it;\
    (_item_)->id = i; (_item_)->role = r; (_item_)->accel = acc;\
    (_item_)->group_accel = 0; (_item_)->selected = sel;\
    strncpy((_item_)->caption, cap, BUFSZ - 1); \
    /* This should never do anything, but safety first! */ \
    (_item_)->caption[BUFSZ - 1] = '\0'; \
} while(0)

# define add_menu_item(items, size, icount, id, caption, accel, selected) \
do {\
    if (icount >= size) \
        {size *= 2; (items) = realloc((items), size * sizeof(struct nh_menuitem)); } \
    set_menuitem(&(items)[icount], id, MI_NORMAL, caption, accel, selected); \
    (icount)++;\
} while(0)

# define add_menu_txt(items, size, icount, caption, role) \
do {\
    if (icount >= size) \
        {size *= 2; (items) = realloc((items), size * sizeof(struct nh_menuitem)); } \
    set_menuitem(&(items)[icount], 0, role, caption, 0, FALSE); \
    (icount)++;\
} while(0)

#endif
