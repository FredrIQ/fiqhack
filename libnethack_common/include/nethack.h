/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-21 */
/* NetHack may be freely redistributed.  See license for details. */

/* this header defines the interface between libnethack and window ports
 * it should never include any private headers, as those are not on the
 * search path for window ports. */

#ifndef NETHACK_H
# define NETHACK_H

/* NetHack 4.3.0. Keep this consistent with patchlevel.h. */
/* (Note that the number in patchlevel.h may be older than the number
   here, if save/bones compatibility is not broken between versions.) */
# define VERSION_MAJOR  4
# define VERSION_MINOR  3
# define PATCHLEVEL     0

# include "nethack_types.h"

/* We want to export these symbols if in libnethack, import them otherwise.
   This is so much simpler than the code that was here before. Thanks,
   aimake! */
# ifdef NETHACK_H_IN_LIBNETHACK
#  define EXPORT(x) AIMAKE_EXPORT(x)
# else
#  define EXPORT(x) AIMAKE_IMPORT(x)
# endif

/* allmain.c */
extern void EXPORT(nh_lib_init) (const struct nh_window_procs *,
                                 const char *const *paths);
extern void EXPORT(nh_lib_exit) (void);
extern nh_bool EXPORT(nh_exit_game) (int exit_type);
extern enum nh_play_status EXPORT (nh_play_game) (int fd, enum nh_followmode);

extern enum nh_create_response EXPORT(nh_create_game) (
    int fd, struct nh_option_desc *opts);
extern const_char_p_const_p EXPORT(nh_get_copyright_banner) (void);

/* log.c */
extern enum nh_log_status EXPORT(nh_get_savegame_status) (
    int fd, struct nh_game_info *si);

/* cmd.c */
extern nh_cmd_desc_p EXPORT(nh_get_commands) (int *count);
extern nh_cmd_desc_p EXPORT(nh_get_object_commands) (int *count, char invlet);

/* drawing.c */
extern nh_drawing_info_p EXPORT(nh_get_drawing_info) (void);

/* options.c */
extern nh_bool EXPORT(nh_set_option) (
    const char *name, union nh_optvalue value);
extern nh_option_desc_p EXPORT(nh_get_options) (void);

/* pager.c */
extern void EXPORT(nh_describe_pos) (
    int x, int y, struct nh_desc_buf *bufs, int *is_in);

/* role.c */
extern nh_roles_info_p EXPORT(nh_get_roles) (void);
extern char_p EXPORT(nh_build_plselection_prompt) (
    char *, int, int, int, int, int);
extern const_char_p EXPORT(nh_root_plselection_prompt) (
    char *, int, int, int, int, int);

/* topten.c */
extern nh_topten_entry_p EXPORT(nh_get_topten) (
    int *out_len, char *statusbuf, const char *player, int top,
    int around, nh_bool own);

# undef EXPORT

#endif
