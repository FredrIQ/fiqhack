/* Copyright (c) David Cohrs, 1991				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINTYPE_H
#define WINTYPE_H

/* select_menu() "how" argument types */
#define PICK_NONE 0	/* user picks nothing (display only) */
#define PICK_ONE  1	/* only pick one */
#define PICK_ANY  2	/* can pick any amount */

/* window types */
/* any additional port specific types should be defined in win*.h */
#define NHW_MESSAGE 1
#define NHW_STATUS  2
#define NHW_MAP     3
#define NHW_MENU    4
#define NHW_TEXT    5

/* nh_poskey() modifier types */
#define CLICK_1     1
#define CLICK_2     2

/* invalid winid */
#define WIN_ERR ((winid) -1)


#endif /* WINTYPE_H */
