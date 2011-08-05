/* Copyright (c) David Cohrs, 1992				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINPROCS_H
#define WINPROCS_H

extern struct window_procs windowprocs;

extern void nh_delay_output(void);

#define player_selection (*windowprocs.win_player_selection)
#define clear_nhwindow (*windowprocs.win_clear_nhwindow)
#define display_nhwindow (*windowprocs.win_display_nhwindow)
#define display_buffer (*windowprocs.win_display_buffer)
#define update_status (*windowprocs.win_update_status)
#define print_message (*windowprocs.win_print_message)
#define display_menu (*windowprocs.win_display_menu)
#define display_objects (*windowprocs.win_display_objects)
#define update_inventory (*windowprocs.win_update_inventory)
#define update_screen (*windowprocs.win_update_screen)
#define raw_print (*windowprocs.win_raw_print)
#define query_key (*windowprocs.win_query_key)
#define win_getpos (*windowprocs.win_getpos)
#define getlin (*windowprocs.win_getlin)
#define delay_output nh_delay_output
#define outrip (*windowprocs.win_outrip)
#define level_changed (*windowprocs.win_level_changed)

#endif /* WINPROCS_H */
