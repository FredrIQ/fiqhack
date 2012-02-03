/* Copyright (c) Daniel Thaler, 2011.                             */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"


int dohistory(void)
{
    struct menulist menu;
    boolean over = program_state.gameover;
    boolean showall = over || wizard;
    char buf[BUFSZ];
    int i;
    
    if (histcount < 2) {
	/* you get an automatic entry on turn 1 for being born.
	 * If it's the only one, there is nothing worth reporting */
	if (!over)
	    pline("History has not recorded anything about you.");
	return 0;
    }
    
    init_menulist(&menu);
    for (i = 0; i < histcount; i++) {
	if (histevents[i].hidden && !showall)
	    continue;
	snprintf(buf, BUFSZ, "On T:%u you %s", histevents[i].when, histevents[i].what);
	add_menutext(&menu, buf);
    }
    
    display_menu(menu.items, menu.icount, "History has recorded:", PICK_NONE, NULL);
    free(menu.items);
    
    return 0;
}


void historic_event(boolean hidden, const char *fmt, ...)
{
    char hbuf[BUFSZ];
    va_list vargs;
    
    va_start(vargs, fmt);
    vsnprintf(hbuf, BUFSZ, fmt, vargs);
    va_end(vargs);
    
    histevents = realloc(histevents, (histcount + 1) * sizeof(struct histevent));
    histevents[histcount].when = moves;
    histevents[histcount].hidden = hidden;
    strncpy(histevents[histcount].what, hbuf, BUFSZ);
    histevents[histcount].what[BUFSZ - 1] = '\0';
    histcount++;
}


void save_history(struct memfile *mf)
{
    int i, len;
    
    mfmagic_set(mf, HISTORY_MAGIC);
    mwrite32(mf, histcount);
    for (i = 0; i < histcount; i++) {
	mwrite32(mf, histevents[i].when);
	mwrite32(mf, histevents[i].hidden);
	len = strlen(histevents[i].what) + 1;
	mwrite32(mf, len);
	mwrite(mf, histevents[i].what, len);
    }
}


void restore_history(struct memfile *mf)
{
    int i, len;
    
    mfmagic_check(mf, HISTORY_MAGIC);
    histcount = mread32(mf);
    histevents = malloc(histcount * sizeof(struct histevent));
    for (i = 0; i < histcount; i++) {
	histevents[i].when = mread32(mf);
	histevents[i].hidden = mread32(mf);
	len = mread32(mf);
	mread(mf, histevents[i].what, len);
    }
}


void free_history(void)
{
    if (histevents)
	free(histevents);
    histevents = NULL;
    histcount = 0;
}

/* build a level name for historic events
 * in_or_on = TRUE: "On T:12345 you killed Kenny ..."
 *   - in The Castle
 *   - on level 3 of Sokoban
 *   - on level 45 in Gehennom
 * in_or_on = FALSE: "On T:12345 you reached ..."
 *   - The Castle
 *   - level 3 of The Quest
 *   - level 12 in The Dungeons of Doom
 * */
const char *hist_lev_name(const d_level *l, boolean in_or_on)
{
    static char hlnbuf[BUFSZ];
    char *bufptr;
    
    if (In_endgame(l)) {
	switch (l->dlevel) {
	    case -5: strcpy(hlnbuf, "on the Astral Plane"); break;
	    case -4: strcpy(hlnbuf, "on the Plane of Water"); break;
	    case -3: strcpy(hlnbuf, "on the Plane of Fire"); break;
	    case -2: strcpy(hlnbuf, "on the Plane of Air"); break;
	    case -1: strcpy(hlnbuf, "on the Plane of Earth"); break;
	}
    }
    else if (Is_knox(l))
	strcpy(hlnbuf, "in Fort Knox");
    else if (Is_stronghold(l))
	strcpy(hlnbuf, "in The Castle");
    else if (Is_valley(l))
	strcpy(hlnbuf, "in The Valley of the Dead");
    else
	sprintf(hlnbuf, "on level %d of %s", l->dlevel, dungeons[l->dnum].dname);
    
    bufptr = in_or_on? hlnbuf : (hlnbuf + 3);
    return bufptr;
}

/* history.c */