/* Copyright (c) Daniel Thaler, 2011. */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"

static FILE *dumpfp;
static struct nh_window_procs winprocs_original;

static void dump_status(void);
static int dump_display_menu(struct nh_menuitem*, int, const char*, int, int*);
static int dump_display_objects(struct nh_objitem*, int, const char*, int,
				struct nh_objresult*);
static void dump_outrip(struct nh_menuitem *items, int icount, boolean ts,
    const char *plname, int gold, const char *killbuf, int end_how, int year);

#if !defined(WIN32)
#define TIMESTAMP_FORMAT "%Y-%m-%d %H:%M"
#else
/* windows doesn't allow ':' in filenames */
#define TIMESTAMP_FORMAT "%Y-%m-%d %H_%M"
#endif

void begin_dump(int how)
{
    char dumpname[BUFSZ], timestamp[BUFSZ], *status;
    const char *rolename;
    time_t t;
    struct tm *tmp;
    
    /* back up the window procs */
    winprocs_original = windowprocs;

    /* make a timestamp like "2011-11-30 18:45" */
    t = time(NULL);
    tmp = localtime(&t);
    if (!tmp || !strftime(timestamp, sizeof(timestamp), TIMESTAMP_FORMAT, tmp))
	strcpy(timestamp, "???");

    switch(how) {
	case ASCENDED:	status = "ascended"; break;
	case QUIT:	status = "quit"; break;
	case ESCAPED:	status = "escaped"; break;
	default:	status = "died"; break;
    }
    
    sprintf(dumpname, "%s, %s-%s-%s-%s-%s, %s.txt", timestamp, plname,
	    urole.filecode, urace.filecode, genders[flags.female].filecode,
	    aligns[1-u.ualign.type].filecode, status);
    dumpfp = fopen_datafile(dumpname, "w+", DUMPPREFIX);
    if (!dumpfp)
	return;
    
    rolename = (flags.female && urole.name.f) ? urole.name.f : urole.name.m;
    fprintf(dumpfp, "%s, %s %s %s %s\n", plname, aligns[1-u.ualign.type].adj,
	    genders[flags.female].adj, urace.adj, rolename);
    
    dump_screen(dumpfp);
    dump_status();
}


static void dump_status(void)
{
    int hp;
    fprintf(dumpfp, "%s the %s\n", plname, rank_of(u.ulevel, Role_switch, flags.female));
    fprintf(dumpfp, "  Experience level: %d\n", u.ulevel);
    
    if (ACURR(A_STR) > 18) {
	if (ACURR(A_STR) < 118)
	    fprintf(dumpfp, "  Strength: 18/%02d\n", ACURR(A_STR) - 18);
	else if (ACURR(A_STR) == 118)
	    fprintf(dumpfp,"  Strength: 18/**\n");
	else
	    fprintf(dumpfp, "  Strength: %-1d\n", ACURR(A_STR) - 100);
    } else
	fprintf(dumpfp, "  Strength: %-1d\n", ACURR(A_STR));
    
    fprintf(dumpfp, "  Dexterity: %-1d\n  Constitution: %-1d\n", ACURR(A_DEX),
	    ACURR(A_CON));
    fprintf(dumpfp, "  Intelligence: %-1d\n  Wisdom: %-1d\n  Charisma: %-1d\n",
	    ACURR(A_INT), ACURR(A_WIS), ACURR(A_CHA));
    
    hp = Upolyd ? u.mh : u.uhp;
    fprintf(dumpfp, "  Health: %d(%d)\n", hp < 0 ? 0 : hp, Upolyd ? u.mhmax : u.uhpmax);
    fprintf(dumpfp, "  Energy: %d(%d)\n", u.uen, u.uenmax);
    fprintf(dumpfp, "  AC: %d\n", u.uac);
    fprintf(dumpfp, "  Gold: %ld\n", money_cnt(invent));
    fprintf(dumpfp, "  Moves: %u\n", moves);
    
    fprintf(dumpfp, "\n\n");
}


void end_dump(int how, char *killbuf, char *pbuf, long umoney)
{
    int saved_stopprint, i, line;
    if (!dumpfp)
	return;
    
    fprintf(dumpfp, "Latest messages:\n");
    for (i = 0; i < MSGCOUNT; i++) {
	line = (curline + i) % MSGCOUNT;
	if (toplines[line][0])
	    fprintf(dumpfp, "  %s\n", toplines[line]);
    }
    fprintf(dumpfp, "\n");
    
    dump_catch_menus(TRUE);
    saved_stopprint = program_state.stopprint;
    program_state.stopprint = 0;
    
    display_rip(how, killbuf, pbuf, umoney);
    
    program_state.stopprint = saved_stopprint;
    dump_catch_menus(FALSE);
    
    fclose(dumpfp);
}


void dump_catch_menus(boolean intercept)
{
    if (!intercept) {
	windowprocs = winprocs_original;
	return;
    }
	
    windowprocs.win_display_menu = dump_display_menu;
    windowprocs.win_display_objects = dump_display_objects;
    windowprocs.win_outrip = dump_outrip;
}


static int dump_display_menu(struct nh_menuitem *items, int icount,
			     const char *title, int how, int *result)
{
    int i, col, extra;
    int colwidth[10];
    char *start, *tab;
    
    if (!dumpfp)
	return 0;
    
    /* menus may have multiple cokumns separated by tabs */
    memset(colwidth, 0, sizeof(colwidth));
    for (i = 0; i < icount; i++) {
	tab = strchr(items[i].caption, '\t');
	if (!tab && items[i].role == MI_HEADING)
	    continue; /* some headings shouldn't be forced into column 1 */
	
	col = 0;
	start = items[i].caption;
	while ( (tab = strchr(start, '\t')) != NULL && col < 10) {
	    extra = (items[i].accel) ? 4 : 0; /* leave space for "a - " */
	    colwidth[col] = max(colwidth[col], tab - start + extra);
	    start = tab+1;
	    col++;
	}
	colwidth[col] = max(colwidth[col], strlen(start));
    }
    
    if (!title)
	title = "Menu:";
    
    /* print the menu content */
    fprintf(dumpfp, "%s\n", title);
    for (i = 0; i < icount; i++) {
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
	while ( (tab = strchr(start, '\t')) != NULL && col < 10) {
	    *tab++ = '\0';
	    fprintf(dumpfp, " %-*s", colwidth[col], start);
	    start = tab;
	    col++;
	}
	fprintf(dumpfp, " %-*s\n", colwidth[col], start);
    }
    
    fprintf(dumpfp, "\n");
    return 0;
}


static int dump_display_objects(struct nh_objitem *items, int icount,
			const char *title, int how, struct nh_objresult *result)
{
    int i;
    if (!dumpfp)
	return 0;
    
    if (!title)
	title = "Your Inventory:";
    
    fprintf(dumpfp, "%s\n", title);
    for (i = 0; i < icount; i++) {
	fprintf(dumpfp, "  ");
	if (items[i].accel)
	    fprintf(dumpfp, "%c - ", items[i].accel);
	fprintf(dumpfp, "%s\n", items[i].caption);
    }
    
    fprintf(dumpfp, "\n");
    return 0;
}


static void dump_outrip(struct nh_menuitem *items, int icount, boolean ts,
	const char *name, int gold, const char *killbuf, int end_how, int year)
{
    dump_display_menu(items, icount, "Final status:", PICK_NONE, NULL);
}
