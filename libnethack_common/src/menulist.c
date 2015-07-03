/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-10 */
/* Copyright (c) Alex Smith, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

#include "menulist.h"
#include <stdlib.h>
#include <string.h>

/* Ensures that the given menu list has space for at least one more item. */
static void
expand_menulist(struct nh_menulist *ml)
{
    if (!ml->size && ml->items)
        return;  /* memory is managed statically */

    if (ml->icount >= ml->size) {
        if (ml->size < 2)
            ml->size = 2;
        ml->size *= 2;
        ml->items = realloc(ml->items, ml->size * sizeof *(ml->items));
        if (!ml->items) /* out of memory */
            abort();
    }
}

void
init_menulist(struct nh_menulist *ml)
{
    ml->items = 0;
    ml->size = 0;
    ml->icount = 0;
}

void
dealloc_menulist(struct nh_menulist *ml)
{
    if (!ml->size && ml->items)
        return;  /* memory is managed statically */

    free(ml->items);
    init_menulist(ml);
}


void
init_objmenulist(struct nh_objlist *ml)
{
    ml->items = 0;
    ml->size = 0;
    ml->icount = 0;
}

void
dealloc_objmenulist(struct nh_objlist *ml)
{
    if (!ml->size && ml->items)
        return;  /* memory is managed statically */

    free(ml->items);
    init_objmenulist(ml);
}


void
set_menuitem(struct nh_menuitem *item, int id, enum nh_menuitem_role role,
             const char *caption, char accel, nh_bool selected)
{
    item->id = id;
    item->role = role;
    item->accel = accel;
    item->group_accel = 0;
    item->selected = selected;

    strncpy(item->caption, caption, (sizeof item->caption) - 1);
    item->caption[(sizeof item->caption) - 1] = '\0';
}

void
add_menu_item(struct nh_menulist *ml, int id, const char *caption,
              char accel, nh_bool selected)
{
    expand_menulist(ml);
    set_menuitem(ml->items + ml->icount, id, MI_NORMAL,
                 caption, accel, selected);
    ml->icount++;
}

void
add_menu_txt(struct nh_menulist *ml, const char *caption,
             enum nh_menuitem_role role)
{
    expand_menulist(ml);
    set_menuitem(ml->items + ml->icount, 0, role, caption, 0, FALSE);
    ml->icount++;
}


void
null_menu_callback(const int *results, int nresults, void *arg)
{
    (void)results;
    (void)nresults;
    (void)arg;
}
