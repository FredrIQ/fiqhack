/* NetHack may be freely redistributed.  See license for details. */

/* this header defines the interface between libnethack and window ports
 * it should never include any private headers, as those are not on the
 * search path for window ports. */

#ifndef NETHACK_API_H
#define NETHACK_API_H

#include "nethack_types.h"

#if defined (nethack_EXPORTS)/* defined by cmake while building libnethack */
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

extern EXPORT struct instance_flags2 iflags2;
extern EXPORT void (*decgraphics_mode_callback)(void);    /* defined in drawing.c */

/* allmain.c */
extern EXPORT void nh_init(int, struct window_procs *, char **);
extern EXPORT boolean nh_restore_save(char *, int, int);
extern EXPORT void nh_start_game(char*, int, int);
extern EXPORT void moveloop(void);
extern EXPORT const char **nh_get_copyright_banner(void);

/* display.c */
extern EXPORT void row_refresh(int,int,int);
extern EXPORT void swallowed(int);
extern EXPORT void docrt(void);
extern EXPORT void switch_graphics(int);

/* files.c */
extern EXPORT void clearlocks(void);

/* hack.c */
extern EXPORT boolean check_swallowed(void);

/* mapglyph.c */
extern EXPORT void mapglyph(int, int *, int *, unsigned *, int, int);

/* options.c */
extern EXPORT boolean nh_set_option(const char *name, union nh_optvalue value, boolean isstr);
extern EXPORT struct nh_option_desc *nh_get_options(boolean birth);
extern EXPORT void nh_setup_ui_options(struct nh_option_desc *options,
			 struct nh_boolopt_map *boolmap,
			 boolean(*callback)(struct nh_option_desc *));

/* role.c */
extern EXPORT int nh_get_valid_roles(int, int, int, struct nh_listitem*, int);
extern EXPORT int nh_get_valid_races(int, int, int, struct nh_listitem*, int);
extern EXPORT int nh_get_valid_genders(int, int, int, struct nh_listitem*, int);
extern EXPORT int nh_get_valid_aligns(int, int, int, struct nh_listitem*, int);
extern EXPORT boolean nh_validrole(int);
extern EXPORT boolean nh_validrace(int, int);
extern EXPORT boolean nh_validgend(int, int, int);
extern EXPORT boolean nh_validalign(int, int, int);
extern EXPORT int nh_str2role(char *);
extern EXPORT int nh_str2race(char *);
extern EXPORT int nh_str2gend(char *);
extern EXPORT int nh_str2align(char *);
extern EXPORT void nh_set_role(int);
extern EXPORT void nh_set_race(int);
extern EXPORT void nh_set_gend(int);
extern EXPORT void nh_set_align(int);
extern EXPORT void nh_set_random_player(void);
extern EXPORT char *nh_build_plselection_prompt(char *, int, int, int, int, int);
extern EXPORT char *nh_root_plselection_prompt(char *, int, int, int, int, int);

/* topten.c */
extern EXPORT void prscore(char*,int,char**);

#endif
