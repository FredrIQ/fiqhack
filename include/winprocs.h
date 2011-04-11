/* Copyright (c) David Cohrs, 1992				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINPROCS_H
#define WINPROCS_H

#include "wincap.h"

extern struct window_procs windowprocs;

/*
 * If you wish to only support one window system and not use procedure
 * pointers, add the appropriate #ifdef below.
 */

extern void nh_delay_output(void);

#define init_nhwindows (*windowprocs.win_init_nhwindows)
#define player_selection (*windowprocs.win_player_selection)
#define askname (*windowprocs.win_askname)
#define get_nh_event (*windowprocs.win_get_nh_event)
#define exit_nhwindows (*windowprocs.win_exit_nhwindows)
#define suspend_nhwindows (*windowprocs.win_suspend_nhwindows)
#define resume_nhwindows (*windowprocs.win_resume_nhwindows)
#define create_nhwindow (*windowprocs.win_create_nhwindow)
#define clear_nhwindow (*windowprocs.win_clear_nhwindow)
#define display_nhwindow (*windowprocs.win_display_nhwindow)
#define destroy_nhwindow (*windowprocs.win_destroy_nhwindow)
#define curs (*windowprocs.win_curs)
#define putstr (*windowprocs.win_putstr)
#define display_file (*windowprocs.win_display_file)
#define start_menu (*windowprocs.win_start_menu)
#define add_menu (*windowprocs.win_add_menu)
#define end_menu (*windowprocs.win_end_menu)
#define select_menu (*windowprocs.win_select_menu)
#define message_menu (*windowprocs.win_message_menu)
#define update_inventory (*windowprocs.win_update_inventory)
#define mark_synch (*windowprocs.win_mark_synch)
#define wait_synch (*windowprocs.win_wait_synch)
#define print_glyph (*windowprocs.win_print_glyph)
#define raw_print (*windowprocs.win_raw_print)
#define raw_print_bold (*windowprocs.win_raw_print_bold)
#define nhgetch (*windowprocs.win_nhgetch)
#define nh_poskey (*windowprocs.win_nh_poskey)
#define nhbell (*windowprocs.win_nhbell)
#define nh_doprev_message (*windowprocs.win_doprev_message)
#define getlin (*windowprocs.win_getlin)
#define get_ext_cmd (*windowprocs.win_get_ext_cmd)
#define number_pad (*windowprocs.win_number_pad)
#define delay_output nh_delay_output
#ifdef CHANGE_COLOR
#define change_color (*windowprocs.win_change_color)
#define get_color_string (*windowprocs.win_get_color_string)
#endif

/* 3.4.2: There is a real yn_function() in the core now, which does
 *        some buffer length validation on the parameters prior to
 *        invoking the window port routine. yn_function() is in cmd.c
 */
/* #define yn_function (*windowprocs.win_yn_function) */

/* other defs that really should go away (they're tty specific) */
#define start_screen (*windowprocs.win_start_screen)
#define end_screen (*windowprocs.win_end_screen)

#define outrip (*windowprocs.win_outrip)
#define preference_update (*windowprocs.win_preference_update)


#define ALIGN_LEFT	1
#define ALIGN_RIGHT	2
#define ALIGN_TOP	3
#define ALIGN_BOTTOM	4

/* player_selection */
#define VIA_DIALOG	0
#define VIA_PROMPTS	1

/* map_mode settings - deprecated */
#define MAP_MODE_TILES		0
#define MAP_MODE_ASCII4x6	1
#define MAP_MODE_ASCII6x8	2
#define MAP_MODE_ASCII8x8	3
#define MAP_MODE_ASCII16x8	4
#define MAP_MODE_ASCII7x12	5
#define MAP_MODE_ASCII8x12	6
#define MAP_MODE_ASCII16x12	7
#define MAP_MODE_ASCII12x16	8
#define MAP_MODE_ASCII10x18	9
#define MAP_MODE_ASCII_FIT_TO_SCREEN 10
#define MAP_MODE_TILES_FIT_TO_SCREEN 11


struct wc_Opt {
	const char *wc_name;
	unsigned long wc_bit;
};

#endif /* WINPROCS_H */
