/* Copyright (c) David Cohrs, 1992				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINPROCS_H
#define WINPROCS_H

extern struct window_procs windowprocs;

extern void nh_delay_output(void);

#define player_selection (*windowprocs.win_player_selection)
#define exit_nhwindows (*windowprocs.win_exit_nhwindows)
#define create_game_windows (*windowprocs.win_create_game_windows)
#define destroy_game_windows (*windowprocs.win_destroy_game_windows)
#define clear_nhwindow (*windowprocs.win_clear_nhwindow)
#define display_nhwindow (*windowprocs.win_display_nhwindow)
#define display_buffer (*windowprocs.win_display_buffer)
#define update_status (*windowprocs.win_update_status)
#define print_message (*windowprocs.win_print_message)

#define display_menu (*windowprocs.win_display_menu)
#define display_objects (*windowprocs.win_display_objects)

#define message_menu (*windowprocs.win_message_menu)
#define update_inventory (*windowprocs.win_update_inventory)
#define mark_synch (*windowprocs.win_mark_synch)
#define wait_synch (*windowprocs.win_wait_synch)
#define print_glyph (*windowprocs.win_print_glyph)
#define raw_print (*windowprocs.win_raw_print)
#define raw_print_bold (*windowprocs.win_raw_print_bold)
#define nhgetch (*windowprocs.win_nhgetch)
#define win_getpos (*windowprocs.win_getpos)
#define nhbell (*windowprocs.win_nhbell)
#define nh_doprev_message (*windowprocs.win_doprev_message)
#define getlin (*windowprocs.win_getlin)
#define delay_output nh_delay_output

#define outrip (*windowprocs.win_outrip)


#define ALIGN_LEFT	1
#define ALIGN_RIGHT	2
#define ALIGN_TOP	3
#define ALIGN_BOTTOM	4

/* player_selection */
#define VIA_DIALOG	0
#define VIA_PROMPTS	1

#endif /* WINPROCS_H */
