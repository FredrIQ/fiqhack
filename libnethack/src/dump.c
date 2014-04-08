/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-08 */
/* Copyright (c) Daniel Thaler, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static FILE *dumpfp;
static struct nh_window_procs winprocs_original;

static void dump_status(void);
static int dump_display_menu(struct nh_menulist *, const char *, int, int,
                             int *);
static int dump_display_objects(struct nh_objlist *, const char *, int,
                                int, struct nh_objresult *);
static void dump_outrip(struct nh_menulist *ml, boolean ts, const char *name,
                        int gold, const char *killbuf, int end_how, int year);

#if !defined(WIN32)
# define TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"
#else
/* windows doesn't allow ':' in filenames */
# define TIMESTAMP_FORMAT "%Y-%m-%d %H_%M_%S"
#endif

void
begin_dump(int how)
{
    const char *timestamp, *dumpname, *status, *rolename;
    time_t t;
    struct tm *tmp;

    /* back up the window procs */
    winprocs_original = windowprocs;

    /* Make a timestamp like "2011-11-30 18:45:00".  This now uses UTC time, in
       accordance with the timebase rules (in particular, we never look at the
       system timezone). This also avoids clashes when there are two games an
       hour apart and DST changed in between. (It doesn't help when there are
       two games in the same second, but that only happens as a result of
       extreme startscumming.) */
    t = (time_t)(utc_time() / 1000000LL);
    tmp = gmtime(&t);
    if (tmp)
        timestamp = msgstrftime(TIMESTAMP_FORMAT, tmp);
    else
        timestamp = "unknown time"; /* previously "???" but that's illegal
                                       on many filesystems */

    switch (how) {
    case ASCENDED:
        status = "ascended";
        break;
    case QUIT:
        status = "quit";
        break;
    case ESCAPED:
        status = "escaped";
        break;
    default:
        status = "died";
        break;
    }

    dumpname = msgprintf("%s, %s-%s-%s-%s-%s, %s.txt",
                         timestamp, u.uplname, urole.filecode, urace.filecode,
                         genders[u.ufemale].filecode,
                         aligns[1 - u.ualign.type].filecode, status);
    dumpfp = fopen_datafile(dumpname, "w+", DUMPPREFIX);
    if (!dumpfp)
        return;

    rolename = (u.ufemale && urole.name.f) ? urole.name.f : urole.name.m;
    fprintf(dumpfp, "%s, %s %s %s %s\n", u.uplname,
            aligns[1 - u.ualign.type].adj, genders[u.ufemale].adj,
            urace.adj, rolename);

    dump_screen(dumpfp);
    dump_status();
}


static void
dump_status(void)
{
    int hp;

    fprintf(dumpfp, "%s the %s\n", u.uplname,
            rank_of(u.ulevel, Role_switch, u.ufemale));
    fprintf(dumpfp, "  Experience level: %d\n", u.ulevel);

    if (ACURR(A_STR) > 18) {
        if (ACURR(A_STR) < 118)
            fprintf(dumpfp, "  Strength: 18/%02d\n", ACURR(A_STR) - 18);
        else if (ACURR(A_STR) == 118)
            fprintf(dumpfp, "  Strength: 18/**\n");
        else
            fprintf(dumpfp, "  Strength: %-1d\n", ACURR(A_STR) - 100);
    } else
        fprintf(dumpfp, "  Strength: %-1d\n", ACURR(A_STR));

    fprintf(dumpfp, "  Dexterity: %-1d\n  Constitution: %-1d\n", ACURR(A_DEX),
            ACURR(A_CON));
    fprintf(dumpfp, "  Intelligence: %-1d\n  Wisdom: %-1d\n  Charisma: %-1d\n",
            ACURR(A_INT), ACURR(A_WIS), ACURR(A_CHA));

    hp = Upolyd ? u.mh : u.uhp;
    fprintf(dumpfp, "  Health: %d(%d)\n", hp < 0 ? 0 : hp,
            Upolyd ? u.mhmax : u.uhpmax);
    fprintf(dumpfp, "  Energy: %d(%d)\n", u.uen, u.uenmax);
    fprintf(dumpfp, "  Def: %d\n", 10 - get_player_ac());
    fprintf(dumpfp, "  Gold: %ld\n", money_cnt(invent));
    fprintf(dumpfp, "  Moves: %u\n", moves);

    fprintf(dumpfp, "\n\n");
}


void
end_dump(int how, long umoney, unsigned long carried)
{
    int saved_stopprint, i, line;

    if (!dumpfp)
        return;

    fprintf(dumpfp, "Latest messages:\n");
    for (i = 0; i < MSGCOUNT; i++) {
        line = (curline + i) % MSGCOUNT;
        if (toplines[line][0]) {
            if (toplines_count[line] == 1) {
                fprintf(dumpfp, "  %s\n", toplines[line]);
            } else {
                fprintf(dumpfp, "  %s (%dx)\n", toplines[line],
                        toplines_count[line]);
            }
        }
    }
    fprintf(dumpfp, "\n");

    dump_catch_menus(TRUE);
    saved_stopprint = program_state.stopprint;
    program_state.stopprint = 0;

    display_rip(how, umoney, carried);

    program_state.stopprint = saved_stopprint;
    dump_catch_menus(FALSE);

    fclose(dumpfp);
}


void
dump_catch_menus(boolean intercept)
{
    if (!intercept) {
        windowprocs = winprocs_original;
        return;
    }

    windowprocs.win_display_menu = dump_display_menu;
    windowprocs.win_display_objects = dump_display_objects;
    windowprocs.win_outrip = dump_outrip;
}


static int
dump_display_menu(struct nh_menulist *menu, const char *title,
                  int how, int placement_hint, int *result)
{
    int i, col, extra;
    int colwidth[10];
    char *start, *tab;
    struct nh_menuitem *const items = menu->items;

    if (!dumpfp) {
        dealloc_menulist(menu);
        return 0;
    }

    /* menus may have multiple columns separated by tabs */
    memset(colwidth, 0, sizeof (colwidth));
    for (i = 0; i < menu->icount; i++) {
        tab = strchr(items[i].caption, '\t');
        if (!tab && items[i].role == MI_HEADING)
            continue;   /* some headings shouldn't be forced into column 1 */

        col = 0;
        start = items[i].caption;
        while ((tab = strchr(start, '\t')) != NULL && col < 10) {
            extra = (items[i].accel) ? 4 : 0;   /* leave space for "a - " */
            colwidth[col] = max(colwidth[col], tab - start + extra);
            start = tab + 1;
            col++;
        }
        colwidth[col] = max(colwidth[col], strlen(start));
    }

    if (!title)
        title = "Menu:";

    /* print the menu content */
    fprintf(dumpfp, "%s\n", title);
    for (i = 0; i < menu->icount; i++) {
        tab = strchr(items[i].caption, '\t');
        if (!tab && items[i].role == MI_HEADING) {
            fprintf(dumpfp, "  %s\n", items[i].caption);
            continue;
        }

        fprintf(dumpfp, " ");
        if (items[i].accel)
            fprintf(dumpfp, " %c -", items[i].accel);

        col = 0;
        start = items[i].caption;
        while ((tab = strchr(start, '\t')) != NULL && col < 10) {
            *tab++ = '\0';
            fprintf(dumpfp, " %-*s", colwidth[col], start);
            start = tab;
            col++;
        }
        fprintf(dumpfp, " %-*s\n", colwidth[col], start);
    }

    fprintf(dumpfp, "\n");

    dealloc_menulist(menu);

    return 0;
}


static int
dump_display_objects(struct nh_objlist *objects, const char *title,
                     int how, int placement_hint, struct nh_objresult *result)
{
    int i;

    if (!dumpfp) {
        dealloc_objmenulist(objects);
        return 0;
    }

    if (!title)
        title = "Your Inventory:";

    fprintf(dumpfp, "%s\n", title);
    for (i = 0; i < objects->icount; i++) {
        fprintf(dumpfp, "  ");
        if (objects->items[i].accel)
            fprintf(dumpfp, "%c - ", objects->items[i].accel);
        fprintf(dumpfp, "%s\n", objects->items[i].caption);
    }

    fprintf(dumpfp, "\n");

    dealloc_objmenulist(objects);

    return 0;
}


static void
dump_outrip(struct nh_menulist *menu, boolean ts, const char *name,
            int gold, const char *killbuf, int end_how, int year)
{
    dump_display_menu(menu, "Final status:", PICK_NONE, PLHINT_ANYWHERE, NULL);
}

