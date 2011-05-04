/* NetHack may be freely redistributed.  See license for details. */

/* this header defines the interface between libnethack and window ports
 * it should never include any private headers, as those are not on the
 * search path for window ports. */

#ifndef NETHACK_API_H
#define NETHACK_API_H

#include "nethack_types.h"

/* nethack_EXPORTS is defined by cmake while building libnethack */
#if nethack_EXPORTS
#define EXPORT __attribute__((__visibility__("default")))
#elif nethack_EXPORTS && defined _MSC_VER
#define EXPORT __declspec(dllexport)
#elif defined _MSC_VER
#define EXPORT __declspec(dllimport)
#else
#define EXPORT
#endif

extern EXPORT struct sinfo program_state;

extern EXPORT char lock[];
extern EXPORT int hackpid;

extern EXPORT long yn_number;

extern EXPORT struct instance_flags iflags;

/* window stuff */
extern EXPORT winid WIN_MESSAGE, WIN_STATUS;
extern EXPORT winid WIN_MAP, WIN_INVEN;
extern EXPORT char toplines[];
extern EXPORT boolean botl;
extern EXPORT boolean botlx;

extern EXPORT char mapped_menu_cmds[]; /* from options.c */

#ifdef TTY_GRAPHICS
extern EXPORT void (*decgraphics_mode_callback)(void);    /* defined in drawing.c */
#endif

/* allmain.c */
extern EXPORT void nethack_init(struct window_procs *);
extern EXPORT void nethack_start_game(char*,boolean,boolean,void(*)(void));
extern EXPORT void moveloop(void);

/* alloc.c, for util progs too */
extern EXPORT long *alloc(unsigned int);

/* botl.c */
extern EXPORT void bot(void);

/* cmd.c */
extern EXPORT char readchar(void);
extern EXPORT int extcmd_via_menu(void);

/* display.c */
extern EXPORT void row_refresh(int,int,int);
extern EXPORT void swallowed(int);
extern EXPORT void docrt(void);
extern EXPORT void switch_graphics(int);

/* end.c */
extern EXPORT void terminate(int);
extern EXPORT void panic(const char *,...);

/* files.c */
extern EXPORT void clearlocks(void);
extern EXPORT void check_recordfile(const char *);
extern EXPORT void set_levelfile_name(char *,int);
extern EXPORT const char *fqname(const char *, int, int);
extern EXPORT boolean lock_file(const char *,int,int);
extern EXPORT void unlock_file(const char *);
extern EXPORT void regularize(char *);

/* hack.c */
extern EXPORT boolean check_swallowed(void);
extern EXPORT void enter_discover_mode(void);
extern EXPORT void enter_wizard_mode(void);

/* hacklib.c */
extern EXPORT boolean digit(char);
extern EXPORT char *eos(char *);
extern EXPORT char lowc(char);
extern EXPORT char highc(char);
extern EXPORT char *mungspaces(char *);
extern EXPORT char *tabexpand(char *);
#ifndef STRNCMPI
extern EXPORT int strncmpi(const char *,const char *,int);
#endif

/* mapglyph.c */
extern EXPORT void mapglyph(int, int *, int *, unsigned *, int, int);

/* objnam.c */
extern EXPORT char *an(const char *);

/* options.c */
extern EXPORT void initoptions(void);
extern EXPORT void add_menu_cmd_alias(char, char);
extern EXPORT char map_menu_cmd(char);
extern EXPORT char *nh_getenv(const char *);
extern EXPORT boolean nh_set_option(const char *name, union optvalue value);
extern EXPORT struct nh_option_desc *nh_get_options(boolean birth);

/* pline.c */
extern EXPORT void pline(const char *,...);
extern EXPORT void impossible(const char *,...);

/* role.c */
extern EXPORT int str2role(char *);
extern EXPORT int str2race(char *);
extern EXPORT int str2gend(char *);
extern EXPORT int str2align(char *);
extern EXPORT void set_role(int);
extern EXPORT void set_race(int);
extern EXPORT void set_gend(int);
extern EXPORT void set_align(int);
extern EXPORT void set_random_player(void);
extern EXPORT boolean ok_role(int, int, int, int);
extern EXPORT boolean ok_race(int, int, int, int);
extern EXPORT boolean ok_gend(int, int, int, int);
extern EXPORT boolean ok_align(int, int, int, int);
extern EXPORT int pick_role(int, int, int, int);
extern EXPORT int pick_race(int, int, int, int);
extern EXPORT int pick_gend(int, int, int, int);
extern EXPORT int pick_align(int, int, int, int);
extern EXPORT boolean validrole(int);
extern EXPORT boolean validrace(int, int);
extern EXPORT boolean validgend(int, int, int);
extern EXPORT boolean validalign(int, int, int);
extern EXPORT int randrole(void);
extern EXPORT int randrace(int);
extern EXPORT int randgend(int, int);
extern EXPORT int randalign(int, int);
extern EXPORT void rigid_role_checks(void);
extern EXPORT char *build_plselection_prompt(char *, int, int, int, int, int);
extern EXPORT char *root_plselection_prompt(char *, int, int, int, int, int);

/* rip.c */
extern EXPORT void genl_outrip(winid,int);

/* topten.c */
extern EXPORT void prscore(char*,int,char**);

/* windows.c */
extern EXPORT char genl_message_menu(char,int,const char *);
extern EXPORT void genl_preference_update(const char *);

#endif
