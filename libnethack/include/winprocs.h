/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-29 */
/* Copyright (c) David Cohrs, 1992                                */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINPROCS_H
# define WINPROCS_H

# include "nethack_types.h"

extern struct nh_window_procs windowprocs;

# define win_pause_output (*windowprocs.win_pause)
# define display_buffer (*windowprocs.win_display_buffer)
# define update_status (*windowprocs.win_update_status)
# define print_message (*windowprocs.win_print_message)
# define update_screen (*windowprocs.win_update_screen)
# define raw_print (*windowprocs.win_raw_print)
# define outrip (*windowprocs.win_outrip)
# define level_changed (*windowprocs.win_level_changed)
# define win_delay_output (*windowprocs.win_delay)

struct display_menu_callback_data {
    const int *results;
    int nresults;
};
struct display_objects_callback_data {
    const struct nh_objresult *results;
    int nresults;
};

#endif /* WINPROCS_H */

