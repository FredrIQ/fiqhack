/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"	/* save & restore info */

static void setgemprobs(d_level*);
static void shuffle(int,int,boolean);
static void shuffle_all(void);
static boolean interesting_to_discover(int);


static short disco[NUM_OBJECTS] = DUMMY;

#ifdef USE_TILES
static void shuffle_tiles(void);
extern short glyph2tile[];	/* from tile.c */

/* Shuffle tile assignments to match descriptions, so a red potion isn't
 * displayed with a blue tile and so on.
 *
 * Tile assignments are not saved, and shouldn't be so that a game can
 * be resumed on an otherwise identical non-tile-using binary, so we have
 * to reshuffle the assignments from oc_descr_idx information when a game
 * is restored.  So might as well do that the first time instead of writing
 * another routine.
 */
static void shuffle_tiles(void)
{
	int i;
	short tmp_tilemap[NUM_OBJECTS];

	for (i = 0; i < NUM_OBJECTS; i++)
		tmp_tilemap[i] =
			glyph2tile[objects[i].oc_descr_idx + GLYPH_OBJ_OFF];

	for (i = 0; i < NUM_OBJECTS; i++)
		glyph2tile[i + GLYPH_OBJ_OFF] = tmp_tilemap[i];
}
#endif	/* USE_TILES */

static void setgemprobs(d_level *dlev)
{
	int j, first, lev;

	if (dlev)
	    lev = (ledger_no(dlev) > maxledgerno())
				? maxledgerno() : ledger_no(dlev);
	else
	    lev = 0;
	first = bases[GEM_CLASS];

	for(j = 0; j < 9-lev/3; j++)
		objects[first+j].oc_prob = 0;
	first += j;
	if (first > LAST_GEM || objects[first].oc_class != GEM_CLASS ||
	    OBJ_NAME(objects[first]) == (char *)0) {
		raw_printf("Not enough gems? - first=%d j=%d LAST_GEM=%d",
			first, j, LAST_GEM);
		wait_synch();
	    }
	for (j = first; j <= LAST_GEM; j++)
		objects[j].oc_prob = (171+j-first)/(LAST_GEM+1-first);
}

/* shuffle descriptions on objects o_low to o_high */
static void shuffle(int o_low, int o_high, boolean domaterial)
{
	int i, j, num_to_shuffle;
	short sw;
	int color;

	for (num_to_shuffle = 0, j=o_low; j <= o_high; j++)
		if (!objects[j].oc_name_known) num_to_shuffle++;
	if (num_to_shuffle < 2) return;

	for (j=o_low; j <= o_high; j++) {
		if (objects[j].oc_name_known) continue;
		do
			i = j + rn2(o_high-j+1);
		while (objects[i].oc_name_known);
		sw = objects[j].oc_descr_idx;
		objects[j].oc_descr_idx = objects[i].oc_descr_idx;
		objects[i].oc_descr_idx = sw;
		sw = objects[j].oc_tough;
		objects[j].oc_tough = objects[i].oc_tough;
		objects[i].oc_tough = sw;
		color = objects[j].oc_color;
		objects[j].oc_color = objects[i].oc_color;
		objects[i].oc_color = color;

		/* shuffle material */
		if (domaterial) {
			sw = objects[j].oc_material;
			objects[j].oc_material = objects[i].oc_material;
			objects[i].oc_material = sw;
		}
	}
}


# define COPY_OBJ_DESCR(o_dst,o_src) \
			o_dst.oc_descr_idx = o_src.oc_descr_idx,\
			o_dst.oc_color = o_src.oc_color

void init_objects(void)
{
	int i, first, last, sum;
	char oclass;

	/* bug fix to prevent "initialization error" abort on Intel Xenix.
	 * reported by mikew@semike
	 */
	for (i = 0; i < MAXOCLASSES; i++)
		bases[i] = 0;
	/* initialize object descriptions */
	for (i = 0; i < NUM_OBJECTS; i++)
		objects[i].oc_name_idx = objects[i].oc_descr_idx = i;
	/* init base; if probs given check that they add up to 1000,
	   otherwise compute probs */
	first = 0;
	while( first < NUM_OBJECTS ) {
		oclass = objects[first].oc_class;
		last = first+1;
		while (last < NUM_OBJECTS && objects[last].oc_class == oclass) last++;
		bases[(int)oclass] = first;

		if (oclass == GEM_CLASS) {
			setgemprobs((d_level *)0);

			if (rn2(2)) { /* change turquoise from green to blue? */
			    COPY_OBJ_DESCR(objects[TURQUOISE],objects[SAPPHIRE]);
			}
			if (rn2(2)) { /* change aquamarine from green to blue? */
			    COPY_OBJ_DESCR(objects[AQUAMARINE],objects[SAPPHIRE]);
			}
			switch (rn2(4)) { /* change fluorite from violet? */
			    case 0:  break;
			    case 1:	/* blue */
				COPY_OBJ_DESCR(objects[FLUORITE],objects[SAPPHIRE]);
				break;
			    case 2:	/* white */
				COPY_OBJ_DESCR(objects[FLUORITE],objects[DIAMOND]);
				break;
			    case 3:	/* green */
				COPY_OBJ_DESCR(objects[FLUORITE],objects[EMERALD]);
				break;
			}
		}
	check:
		sum = 0;
		for(i = first; i < last; i++) sum += objects[i].oc_prob;
		if(sum == 0) {
			for(i = first; i < last; i++)
			    objects[i].oc_prob = (1000+i-first)/(last-first);
			goto check;
		}
		if(sum != 1000) {
			char buf[BUFSZ];
			sprintf(buf, "init-prob error for class %d (%d%%)", oclass, sum);
			putstr(WIN_ERR, 0, buf);
		}
		first = last;
	}
	/* shuffle descriptions */
	shuffle_all();
#ifdef USE_TILES
	shuffle_tiles();
#endif
}

static void shuffle_all(void)
{
	int first, last, oclass;

	for (oclass = 1; oclass < MAXOCLASSES; oclass++) {
		first = bases[oclass];
		last = first+1;
		while (last < NUM_OBJECTS && objects[last].oc_class == oclass)
			last++;

		if (OBJ_DESCR(objects[first]) != (char *)0 &&
				oclass != TOOL_CLASS &&
				oclass != WEAPON_CLASS &&
				oclass != ARMOR_CLASS &&
				oclass != GEM_CLASS) {
			int j = last-1;

			if (oclass == POTION_CLASS)
			    j -= 1;  /* only water has a fixed description */
			else if (oclass == AMULET_CLASS ||
				 oclass == SCROLL_CLASS ||
				 oclass == SPBOOK_CLASS) {
			    while (!objects[j].oc_magic || objects[j].oc_unique)
				j--;
			}

			/* non-magical amulets, scrolls, and spellbooks
			 * (ex. imitation Amulets, blank, scrolls of mail)
			 * and one-of-a-kind magical artifacts at the end of
			 * their class in objects[] have fixed descriptions.
			 */
			shuffle(first, j, TRUE);
		}
	}

	/* shuffle the helmets */
	shuffle(HELMET, HELM_OF_TELEPATHY, FALSE);

	/* shuffle the gloves */
	shuffle(LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY, FALSE);

	/* shuffle the cloaks */
	shuffle(CLOAK_OF_PROTECTION, CLOAK_OF_DISPLACEMENT, FALSE);

	/* shuffle the boots [if they change, update find_skates() below] */
	shuffle(SPEED_BOOTS, LEVITATION_BOOTS, FALSE);
}

/* find the object index for snow boots; used [once] by slippery ice code */
int find_skates(void)
{
    int i;
    const char *s;

    for (i = SPEED_BOOTS; i <= LEVITATION_BOOTS; i++)
	if ((s = OBJ_DESCR(objects[i])) != 0 && !strcmp(s, "snow boots"))
	    return i;

    impossible("snow boots not found?");
    return -1;	/* not 0, or caller would try again each move */
}

void oinit(void)	/* level dependent initialization */
{
	setgemprobs(&u.uz);
}

void savenames(int fd, int mode)
{
	int i;
	unsigned int len;

	if (perform_bwrite(mode)) {
	    bwrite(fd, (void *)bases, sizeof bases);
	    bwrite(fd, (void *)disco, sizeof disco);
	    bwrite(fd, (void *)objects,
		   sizeof(struct objclass) * NUM_OBJECTS);
	}
	/* as long as we use only one version of Hack we
	   need not save oc_name and oc_descr, but we must save
	   oc_uname for all objects */
	for (i = 0; i < NUM_OBJECTS; i++)
	    if (objects[i].oc_uname) {
		if (perform_bwrite(mode)) {
		    len = strlen(objects[i].oc_uname)+1;
		    bwrite(fd, (void *)&len, sizeof len);
		    bwrite(fd, (void *)objects[i].oc_uname, len);
		}
		if (release_data(mode)) {
		    free((void *)objects[i].oc_uname);
		    objects[i].oc_uname = 0;
		}
	    }
}

void restnames(int fd)
{
	int i;
	unsigned int len;

	mread(fd, (void *) bases, sizeof bases);
	mread(fd, (void *) disco, sizeof disco);
	mread(fd, (void *) objects, sizeof(struct objclass) * NUM_OBJECTS);
	for (i = 0; i < NUM_OBJECTS; i++)
	    if (objects[i].oc_uname) {
		mread(fd, (void *) &len, sizeof len);
		objects[i].oc_uname = (char *) alloc(len);
		mread(fd, (void *)objects[i].oc_uname, len);
	    }
#ifdef USE_TILES
	shuffle_tiles();
#endif
}

void discover_object(int oindx, boolean mark_as_known, boolean credit_hero)
{
    if (!objects[oindx].oc_name_known) {
	int dindx, acls = objects[oindx].oc_class;

	/* Loop thru disco[] 'til we find the target (which may have been
	   uname'd) or the next open slot; one or the other will be found
	   before we reach the next class...
	 */
	for (dindx = bases[acls]; disco[dindx] != 0; dindx++)
	    if (disco[dindx] == oindx) break;
	disco[dindx] = oindx;

	if (mark_as_known) {
	    objects[oindx].oc_name_known = 1;
	    if (credit_hero) exercise(A_WIS, TRUE);
	}
	if (moves > 1L) update_inventory();
    }
}

/* if a class name has been cleared, we may need to purge it from disco[] */
void undiscover_object(int oindx)
{
    if (!objects[oindx].oc_name_known) {
	int dindx, acls = objects[oindx].oc_class;
	boolean found = FALSE;

	/* find the object; shift those behind it forward one slot */
	for (dindx = bases[acls];
	      dindx < NUM_OBJECTS && disco[dindx] != 0
		&& objects[dindx].oc_class == acls; dindx++)
	    if (found)
		disco[dindx-1] = disco[dindx];
	    else if (disco[dindx] == oindx)
		found = TRUE;

	/* clear last slot */
	if (found) disco[dindx-1] = 0;
	else impossible("named object not in disco");
	update_inventory();
    }
}

static boolean interesting_to_discover(int i)
{
	/* Pre-discovered objects are now printed with a '*' */
    return((boolean)(objects[i].oc_uname != (char *)0 ||
	    (objects[i].oc_name_known && OBJ_DESCR(objects[i]) != (char *)0)));
}

/* items that should stand out once they're known */
static short uniq_objs[] = {
	AMULET_OF_YENDOR,
	SPE_BOOK_OF_THE_DEAD,
	CANDELABRUM_OF_INVOCATION,
	BELL_OF_OPENING,
};

int dodiscovered(void)
{
    int i, dis;
    int	ct = 0;
    char *s, oclass, prev_class, classes[MAXOCLASSES];
    winid tmpwin;
	char buf[BUFSZ];

    tmpwin = create_nhwindow(NHW_MENU);
    putstr(tmpwin, 0, "Discoveries");
    putstr(tmpwin, 0, "");

    /* gather "unique objects" into a pseudo-class; note that they'll
       also be displayed individually within their regular class */
    for (i = dis = 0; i < SIZE(uniq_objs); i++)
	if (objects[uniq_objs[i]].oc_name_known) {
	    if (!dis++)
		putstr(tmpwin, iflags.menu_headings, "Unique Items");
		sprintf(buf, "  %s", OBJ_NAME(objects[uniq_objs[i]]));
	    putstr(tmpwin, 0, buf);
	    ++ct;
	}
    /* display any known artifacts as another pseudo-class */
    ct += disp_artifact_discoveries(tmpwin);

    /* several classes are omitted from packorder; one is of interest here */
    strcpy(classes, flags.inv_order);
    if (!index(classes, VENOM_CLASS)) {
	s = eos(classes);
	*s++ = VENOM_CLASS;
	*s = '\0';
    }

    for (s = classes; *s; s++) {
	oclass = *s;
	prev_class = oclass + 1;	/* forced different from oclass */
	for (i = bases[(int)oclass];
	     i < NUM_OBJECTS && objects[i].oc_class == oclass; i++) {
	    if ((dis = disco[i]) && interesting_to_discover(dis)) {
		ct++;
		if (oclass != prev_class) {
		    putstr(tmpwin, iflags.menu_headings, let_to_name(oclass, FALSE));
		    prev_class = oclass;
		}
		sprintf(buf, "%s %s",(objects[dis].oc_pre_discovered ? "*" : " "),
				obj_typename(dis));
		putstr(tmpwin, 0, buf);
	    }
	}
    }
    if (ct == 0) {
	You("haven't discovered anything yet...");
    } else
	display_nhwindow(tmpwin, TRUE);
    destroy_nhwindow(tmpwin);

    return 0;
}

/*o_init.c*/
