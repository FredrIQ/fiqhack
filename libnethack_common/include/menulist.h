/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-10 */
/* Copyright (c) Alex Smith, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MENULIST_H
# define MENULIST_H

# include "nethack_types.h"

extern void set_menuitem(struct nh_menuitem *, int, enum nh_menuitem_role,
                         const char *, char, nh_bool);
extern void add_menu_item(struct nh_menulist *, int,
                          const char *, char, nh_bool);
extern void add_menu_txt(struct nh_menulist *, const char *,
                         enum nh_menuitem_role);
extern void init_menulist(struct nh_menulist *);
extern void dealloc_menulist(struct nh_menulist *);

extern void init_objmenulist(struct nh_objlist *);
extern void dealloc_objmenulist(struct nh_objlist *);

extern void null_menu_callback(const int *, int, void *);

#endif
