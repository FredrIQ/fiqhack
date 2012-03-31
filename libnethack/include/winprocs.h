/* Copyright (c) David Cohrs, 1992				  */
/* NitroHack may be freely redistributed.  See license for details. */

#ifndef WINPROCS_H
#define WINPROCS_H

extern struct nh_window_procs windowprocs;

#define win_pause_output (*windowprocs.win_pause)
#define display_buffer (*windowprocs.win_display_buffer)
#define update_status (*windowprocs.win_update_status)
#define print_message (*windowprocs.win_print_message)
#define update_screen (*windowprocs.win_update_screen)
#define raw_print (*windowprocs.win_raw_print)
#define outrip (*windowprocs.win_outrip)
#define level_changed (*windowprocs.win_level_changed)
#define win_delay_output (*windowprocs.win_delay)

#endif /* WINPROCS_H */
