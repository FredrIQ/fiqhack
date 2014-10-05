/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-06-06 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"

static void
topten_add_score(struct nh_topten_entry *entry, struct nh_menulist *menu,
                 int maxwidth)
{
    char line[BUFSZ], txt[BUFSZ], *txt2;
    char fmt[32], hpbuf[16], maxhpbuf[16];
    int txtfw, initialfw, hpfw; /* field widths */
    enum nh_menuitem_role role = entry->highlight ? MI_HEADING : MI_TEXT;

    initialfw = strlen(" No     Points   ");
    hpfw = strlen(" Hp [max] ");

    /* build padded hp strings */
    if (entry->hp <= 0) {
        hpbuf[0] = '-';
        hpbuf[1] = '\0';
    } else
        snprintf(hpbuf, ARRAY_SIZE(hpbuf), "%d", entry->hp);
    snprintf(maxhpbuf, ARRAY_SIZE(maxhpbuf), "[%d]", entry->maxhp);

    /* calc maximum text field width for the current terminal. maxwidth already 
       accounts for window borders and spacing. */
    txtfw = maxwidth - initialfw - hpfw - 1;

    if (strlen(entry->entrytxt) > txtfw) {      /* text needs to be split */
        strncpy(txt, entry->entrytxt, BUFSZ);
        txt2 = &txt[txtfw];
        while (*txt2 != ' ' && txt2 > txt)
            txt2--;
        /* special case: if about to wrap in the middle of maximum dungeon
           depth reached, wrap in front of it instead */
        if (txt2 > txt + 5 && !strncmp(txt2 - 5, " [max", 5))
            txt2 -= 5;
        *txt2++ = '\0';

        snprintf(fmt, ARRAY_SIZE(fmt), "%%4d %%10d  %%-%ds", maxwidth - initialfw);
        snprintf(line, ARRAY_SIZE(line), fmt, entry->rank, entry->points, txt);
        add_menu_txt(menu, line, role);

        snprintf(fmt, ARRAY_SIZE(fmt), "%%%ds%%-%ds %%3s %%5s ", initialfw, txtfw);
        snprintf(line, ARRAY_SIZE(line), fmt, "", txt2, hpbuf, maxhpbuf);
        add_menu_txt(menu, line, role);
    } else {
        snprintf(fmt, ARRAY_SIZE(fmt), "%%4d %%10d  %%-%ds %%3s %%5s ", txtfw);
        snprintf(line, ARRAY_SIZE(line), fmt, entry->rank, entry->points, entry->entrytxt, hpbuf,
                maxhpbuf);
        add_menu_txt(menu, line, role);
    }
}


static void
makeheader(char *linebuf)
{
    size_t i;

    strcpy(linebuf, " No     Points   Name");
    for (i = strlen(linebuf); i < min(BUFSZ, COLS) - strlen(" Hp [max] ") - 4;
         i++)
        linebuf[i] = ' ';
    strcpy(&linebuf[i], " Hp [max] ");
}


void
show_topten(char *you, int top, int around, nh_bool own)
{
    struct nh_topten_entry *scores;
    char buf[BUFSZ];
    int i, listlen = 0;
    struct nh_menulist menu;

    scores = nh_get_topten(&listlen, buf, you, top, around, own);

    if (listlen == 0) {
        curses_msgwin("There are no scores to show.", krc_notification);
        return;
    }

    /* show the score list on a blank screen */
    erase();
    wnoutrefresh(stdscr);

    init_menulist(&menu);

    /* buf has the topten status if there is one, eg: "you did not beat your
       previous score" */
    if (buf[0]) {
        add_menu_txt(&menu, buf, MI_TEXT);
        add_menu_txt(&menu, "", MI_TEXT);
    }

    makeheader(buf);
    add_menu_txt(&menu, buf, MI_HEADING);

    for (i = 0; i < listlen; i++)
        topten_add_score(&scores[i], &menu, min(COLS, BUFSZ) - 4);
    add_menu_txt(&menu, "", MI_TEXT);

    curses_display_menu(&menu, "Top scores:", PICK_NONE,
                        PLHINT_ANYWHERE, NULL, null_menu_callback);
}
