/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This file contains the command routines dowhatis() and dohelp() and */
/* a few other help related facilities */

#include "hack.h"
#include "dlb.h"

static int append_str(char *buf, const char *new_str, int is_plur);
static void mon_vision_summary(const struct monst *mtmp, char *outbuf);
static void describe_bg(int x, int y, int bg, char *buf);
static int describe_object(int x, int y, int votyp, char *buf);
static void describe_mon(int x, int y, int monnum, char *buf);
static void checkfile(const char *inp, struct permonst *, boolean, boolean);
static int do_look(boolean);
static boolean help_menu(int *);

/* The explanations below are also used when the user gives a string
 * for blessed genocide, so no text should wholly contain any later
 * text.  They should also always contain obvious names (eg. cat/feline).
 */
const char * const monexplain[MAXMCLASSES] = {
    0,
    "ant or other insect",	"blob",			"cockatrice",
    "dog or other canine",	"eye or sphere",	"cat or other feline",
    "gremlin",			"humanoid",		"imp or minor demon",
    "jelly",			"kobold",		"leprechaun",
    "mimic",			"nymph",		"orc",
    "piercer",			"quadruped",		"rodent",
    "arachnid or centipede",	"trapper or lurker above", "unicorn or horse",
    "vortex",		"worm", "xan or other mythical/fantastic insect",
    "light",			"zruty",

    "angelic being",		"bat or bird",		"centaur",
    "dragon",			"elemental",		"fungus or mold",
    "gnome",			"giant humanoid",	0,
    "jabberwock",		"Keystone Kop",		"lich",
    "mummy",			"naga",			"ogre",
    "pudding or ooze",		"quantum mechanic",	"rust monster or disenchanter",
    "snake",			"troll",		"umber hulk",
    "vampire",			"wraith",		"xorn",
    "apelike creature",		"zombie",

    "human or elf",		"ghost",		"golem",
    "major demon",		"sea monster",		"lizard",
    "long worm tail",		"mimic"
};

const char invisexplain[] = "remembered, unseen, creature";

/* Object descriptions.  Used in do_look(). */
const char * const objexplain[] = {	/* these match def_oc_syms */
/* 0*/	0,
	"strange object",
	"weapon",
	"suit or piece of armor",
	"ring",
/* 5*/	"amulet",
	"useful item (pick-axe, key, lamp...)",
	"piece of food",
	"potion",
	"scroll",
/*10*/	"spellbook",
	"wand",
	"pile of coins",
	"gem or rock",
	"boulder or statue",
/*15*/	"iron ball",
	"iron chain",
	"splash of venom"
};

/*
 * Append new_str to the end of buf if new_str doesn't already exist as
 * a substring of buf.  Return 1 if the string was appended, 0 otherwise.
 * It is expected that buf is of size BUFSZ.
 */
static int append_str(char *buf, const char *new_str, int is_plur)
{
    int space_left;	/* space remaining in buf */

    if (!new_str || !new_str[0])
	return 0;
    
    space_left = BUFSZ - strlen(buf) - 1;
    if (buf[0]) {
	strncat(buf, " on ", space_left);
	space_left -= 4;
    }
    
    if (is_plur)
	strncat(buf, new_str, space_left);
    else
	strncat(buf, an(new_str), space_left);
    
    return 1;
}


static void mon_vision_summary(const struct monst *mtmp, char *outbuf)
{
    int ways_seen = 0, normal = 0, xraydist;
    boolean useemon = (boolean) canseemon(mtmp);
    
    outbuf[0] = '\0';

    xraydist = (u.xray_range<0) ? -1 : u.xray_range * u.xray_range;
    /* normal vision */
    if ((mtmp->wormno ? worm_known(mtmp) : cansee(mtmp->mx, mtmp->my)) &&
	    mon_visible(mtmp) && !mtmp->minvis) {
	ways_seen++;
	normal++;
    }
    /* see invisible */
    if (useemon && mtmp->minvis)
	ways_seen++;
    /* infravision */
    if ((!mtmp->minvis || See_invisible) && see_with_infrared(mtmp))
	ways_seen++;
    /* telepathy */
    if (tp_sensemon(mtmp))
	ways_seen++;
    /* xray */
    if (useemon && xraydist > 0 &&
	    distu(mtmp->mx, mtmp->my) <= xraydist)
	ways_seen++;
    if (Detect_monsters)
	ways_seen++;
    if (MATCH_WARN_OF_MON(mtmp))
	ways_seen++;

    if (ways_seen > 1 || !normal) {
	if (normal) {
	    strcat(outbuf, "normal vision");
	    /* can't actually be 1 yet here */
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
	if (useemon && mtmp->minvis) {
	    strcat(outbuf, "see invisible");
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
	if ((!mtmp->minvis || See_invisible) &&
		see_with_infrared(mtmp)) {
	    strcat(outbuf, "infravision");
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
	if (tp_sensemon(mtmp)) {
	    strcat(outbuf, "telepathy");
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
	if (useemon && xraydist > 0 &&
		distu(mtmp->mx, mtmp->my) <= xraydist) {
	    /* Eyes of the Overworld */
	    strcat(outbuf, "astral vision");
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
	if (Detect_monsters) {
	    strcat(outbuf, "monster detection");
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
	if (MATCH_WARN_OF_MON(mtmp)) {
	    char wbuf[BUFSZ];
	    if (Hallucination)
		    strcat(outbuf, "paranoid delusion");
	    else {
		    sprintf(wbuf, "warned of %s",
			    makeplural(mtmp->data->mname));
		    strcat(outbuf, wbuf);
	    }
	    if (ways_seen-- > 1) strcat(outbuf, ", ");
	}
    }
}


static void describe_bg(int x, int y, int bg, char *buf)
{
    if (!bg)
	return;
    
    switch(bg) {
	case S_altar:
	    if (!In_endgame(&u.uz))
		sprintf(buf, "%s altar",
		    align_str(Amask2align(level.locations[x][y].altarmask & ~AM_SHRINE)));
	    else
		sprintf(buf, "aligned altar");
	    break;
	    
	case S_ndoor:
	    if (is_drawbridge_wall(x, y) >= 0)
		strcpy(buf,"open drawbridge portcullis");
	    else if ((level.locations[x][y].doormask & ~D_TRAPPED) == D_BROKEN)
		strcpy(buf,"broken door");
	    else
		strcpy(buf,"doorway");
	    break;
	    
	case S_cloud:
	    strcpy(buf, Is_airlevel(&u.uz) ? "cloudy area" : "fog/vapor cloud");
	    break;
	    
	default:
	    strcpy(buf, defexplain[bg]);
	    break;
    }
}


static int describe_object(int x, int y, int votyp, char *buf)
{
    int num_objs = 0;
    if (votyp == -1)
	return -1;
    
    struct obj *otmp = vobj_at(x,y);
    
    if (!otmp || otmp->otyp != votyp) {
	if (votyp != STRANGE_OBJECT) {
	    otmp = mksobj(votyp, FALSE, FALSE);
	    if (otmp->oclass == COIN_CLASS)
		otmp->quan = 1L; /* to force pluralization off */
	    else if (otmp->otyp == SLIME_MOLD)
		otmp->spe = current_fruit;	/* give the fruit a type */
	    strcpy(buf, distant_name(otmp, xname));
	    dealloc_obj(otmp);
	}
    } else
	strcpy(buf, distant_name(otmp, xname));
    
    if (level.locations[x][y].typ == STONE || level.locations[x][y].typ == SCORR)
	strcat(buf, " embedded in stone");
    else if (IS_WALL(level.locations[x][y].typ) || level.locations[x][y].typ == SDOOR)
	strcat(buf, " embedded in a wall");
    else if (closed_door(x,y))
	strcat(buf, " embedded in a door");
    else if (is_pool(x,y))
	strcat(buf, " in water");
    else if (is_lava(x,y))
	strcat(buf, " in molten lava");	/* [can this ever happen?] */
    
    if (!cansee(x, y))
	return -1; /* don't disclose the number of objects for location out of LOS */
    
    if (!otmp)
	/* There is no object here. Since the player sees one it must be a mimic */
	return 1;
    
    if (otmp->otyp != votyp)
	/* Hero sees something other than the actual top object. Probably a mimic */
	num_objs++;
    
    for ( ; otmp; otmp = otmp->nexthere)
	num_objs++;
    
    return num_objs;
}


static void describe_mon(int x, int y, int monnum, char *buf)
{
    char race[QBUFSZ];
    char *name, monnambuf[BUFSZ];
    boolean accurate = !Hallucination;
    char steedbuf[BUFSZ];
    struct monst *mtmp;
    
    if (monnum == -1)
	return;
    
    if (u.ux == x && u.uy == y && senseself()) {
	/* if not polymorphed, show both the role and the race */
	race[0] = 0;
	if (!Upolyd)
	    sprintf(race, "%s ", urace.adj);

	sprintf(buf, "%s%s%s called %s",
		Invis ? "invisible " : "",
		race,
		mons[u.umonnum].mname,
		plname);
	
	if (u.usteed) {
	    sprintf(steedbuf, ", mounted on %s", y_monnam(u.usteed));
	    /* assert((sizeof buf >= strlen(buf)+strlen(steedbuf)+1); */
	    strcat(buf, steedbuf);
	}
	/* When you see yourself normally, no explanation is appended
	(even if you could also see yourself via other means).
	Sensing self while blind or swallowed is treated as if it
	were by normal vision (cf canseeself()). */
	if ((Invisible || u.uundetected) && !Blind && !u.uswallow) {
	    unsigned how = 0;

	    if (Infravision)	 how |= 1;
	    if (Unblind_telepat) how |= 2;
	    if (Detect_monsters) how |= 4;

	    if (how)
		sprintf(eos(buf), " [seen: %s%s%s%s%s]",
			(how & 1) ? "infravision" : "",
			/* add comma if telep and infrav */
			((how & 3) > 2) ? ", " : "",
			(how & 2) ? "telepathy" : "",
			/* add comma if detect and (infrav or telep or both) */
			((how & 7) > 4) ? ", " : "",
			(how & 4) ? "monster detection" : "");
	}
	
    } else if (monnum >= NUMMONS) {
	monnum -= NUMMONS;
	if (monnum < WARNCOUNT)
	    strcat(buf, warnexplain[monnum]);
	
    } else if ( (mtmp = m_at(x,y)) ) {
	bhitpos.x = x;
	bhitpos.y = y;

	if (mtmp->data == &mons[PM_COYOTE] && accurate)
	    name = coyotename(mtmp, monnambuf);
	else
	    name = distant_monnam(mtmp, ARTICLE_NONE, monnambuf);

	sprintf(buf, "%s%s%s",
		(mtmp->mx != x || mtmp->my != y) ?
		    ((mtmp->isshk && accurate)
			    ? "tail of " : "tail of a ") : "",
		(mtmp->mtame && accurate) ? "tame " :
		(mtmp->mpeaceful && accurate) ? "peaceful " : "",
		name);
	if (u.ustuck == mtmp)
	    strcat(buf, (Upolyd && sticks(youmonst.data)) ?
		    ", being held" : ", holding you");
	if (mtmp->mleashed)
	    strcat(buf, ", leashed to you");

	if (mtmp->mtrapped && cansee(mtmp->mx, mtmp->my)) {
	    struct trap *t = t_at(mtmp->mx, mtmp->my);
	    int tt = t ? t->ttyp : NO_TRAP;

	    /* newsym lets you know of the trap, so mention it here */
	    if (tt == BEAR_TRAP || tt == PIT ||
		    tt == SPIKED_PIT || tt == WEB)
		sprintf(eos(buf), ", trapped in %s", an(trapexplain[tt]));
	}

	char visionbuf[BUFSZ], temp_buf[BUFSZ];
	mon_vision_summary(mtmp, visionbuf);
	if (visionbuf[0]) {
	    sprintf(temp_buf, " [seen: %s]", visionbuf);
	    strncat(buf, temp_buf, BUFSZ-strlen(buf)-1);
	}
    }
}


void nh_describe_pos(int x, int y, struct nh_desc_buf *bufs)
{
    int monid = dbuf_get_mon(x, y);
    
    bufs->bgdesc[0] = '\0';
    bufs->trapdesc[0] = '\0';
    bufs->objdesc[0] = '\0';
    bufs->mondesc[0] = '\0';
    bufs->invisdesc[0] = '\0';
    bufs->effectdesc[0] = '\0';
    bufs->objcount = -1;
    
    describe_bg(x, y, level.locations[x][y].mem_bg, bufs->bgdesc);
    
    if (level.locations[x][y].mem_trap)
	strcpy(bufs->trapdesc, trapexplain[level.locations[x][y].mem_trap]);
    
    bufs->objcount = describe_object(x, y, level.locations[x][y].mem_obj - 1,
				     bufs->objdesc);
    
    describe_mon(x, y, monid - 1, bufs->mondesc);
    
    if (level.locations[x][y].mem_invis)
	strcpy(bufs->invisdesc, invisexplain);
    
    if (u.uswallow && (x != u.ux || y != u.uy)) {
	/* all locations when swallowed other than the hero are the monster */
	sprintf(bufs->effectdesc, "interior of %s", Blind ? "a monster" : a_monnam(u.ustuck));
    }
}

/*
 * Look in the "data" file for more info.  Called if the user typed in the
 * whole name (user_typed_name == TRUE), or we've found a possible match
 * with a character/glyph and flags.help is TRUE.
 */
static void checkfile(const char *inp, struct permonst *pm, boolean user_typed_name,
		      boolean without_asking)
{
    dlb *fp;
    char buf[BUFSZ], newstr[BUFSZ];
    char *ep, *dbase_str;
    long txt_offset;
    int chk_skip;
    boolean found_in_file = FALSE, skipping_entry = FALSE;

    fp = dlb_fopen(DATAFILE, "r");
    if (!fp) {
	pline("Cannot open data file!");
	return;
    }

    /* To prevent the need for entries in data.base like *ngel to account
     * for Angel and angel, make the lookup string the same for both
     * user_typed_name and picked name.
     */
    if (pm != NULL && !user_typed_name)
	dbase_str = strcpy(newstr, pm->mname);
    else dbase_str = strcpy(newstr, inp);
    lcase(dbase_str);

    if (!strncmp(dbase_str, "interior of ", 12))
	dbase_str += 12;
    if (!strncmp(dbase_str, "a ", 2))
	dbase_str += 2;
    else if (!strncmp(dbase_str, "an ", 3))
	dbase_str += 3;
    else if (!strncmp(dbase_str, "the ", 4))
	dbase_str += 4;
    if (!strncmp(dbase_str, "tame ", 5))
	dbase_str += 5;
    else if (!strncmp(dbase_str, "peaceful ", 9))
	dbase_str += 9;
    if (!strncmp(dbase_str, "invisible ", 10))
	dbase_str += 10;
    if (!strncmp(dbase_str, "statue of ", 10))
	dbase_str[6] = '\0';
    else if (!strncmp(dbase_str, "figurine of ", 12))
	dbase_str[8] = '\0';

    /* Make sure the name is non-empty. */
    if (*dbase_str) {
	/* adjust the input to remove "named " and convert to lower case */
	char *alt = 0;	/* alternate description */

	if ((ep = strstri(dbase_str, " named ")) != 0)
	    alt = ep + 7;
	else
	    ep = strstri(dbase_str, " called ");
	if (!ep) ep = strstri(dbase_str, ", ");
	if (ep && ep > dbase_str) *ep = '\0';

	/*
	 * If the object is named, then the name is the alternate description;
	 * otherwise, the result of makesingular() applied to the name is. This
	 * isn't strictly optimal, but named objects of interest to the user
	 * will usually be found under their name, rather than under their
	 * object type, so looking for a singular form is pointless.
	 */

	if (!alt)
	    alt = makesingular(dbase_str);
	else
	    if (user_typed_name)
		lcase(alt);

	/* skip first record; read second */
	txt_offset = 0L;
	if (!dlb_fgets(buf, BUFSZ, fp) || !dlb_fgets(buf, BUFSZ, fp)) {
	    impossible("can't read 'data' file");
	    dlb_fclose(fp);
	    return;
	} else if (sscanf(buf, "%8lx\n", &txt_offset) < 1 || txt_offset <= 0)
	    goto bad_data_file;

	/* look for the appropriate entry */
	while (dlb_fgets(buf,BUFSZ,fp)) {
	    if (*buf == '.') break;  /* we passed last entry without success */

	    if (digit(*buf)) {
		/* a number indicates the end of current entry */
		skipping_entry = FALSE;
	    } else if (!skipping_entry) {
		if (!(ep = index(buf, '\n'))) goto bad_data_file;
		*ep = 0;
		/* if we match a key that begins with "~", skip this entry */
		chk_skip = (*buf == '~') ? 1 : 0;
		if (pmatch(&buf[chk_skip], dbase_str) ||
			(alt && pmatch(&buf[chk_skip], alt))) {
		    if (chk_skip) {
			skipping_entry = TRUE;
			continue;
		    } else {
			found_in_file = TRUE;
			break;
		    }
		}
	    }
	}
    }

    if (found_in_file) {
	long entry_offset;
	int  entry_count;
	int  i;

	/* skip over other possible matches for the info */
	do {
	    if (!dlb_fgets(buf, BUFSZ, fp))
		goto bad_data_file;
	} while (!digit(*buf));
	
	if (sscanf(buf, "%ld,%d\n", &entry_offset, &entry_count) < 2) {
bad_data_file:	impossible("'data' file in wrong format");
		dlb_fclose(fp);
		return;
	}

	if (user_typed_name || without_asking || yn("More info?") == 'y') {
	    struct menulist menu;

	    if (dlb_fseek(fp, txt_offset + entry_offset, SEEK_SET) < 0) {
		pline("? Seek error on 'data' file!");
		dlb_fclose(fp);
		return;
	    }
	    
	    init_menulist(&menu);
	    for (i = 0; i < entry_count; i++) {
		if (!dlb_fgets(buf, BUFSZ, fp))
		    goto bad_data_file;
		if ((ep = index(buf, '\n')) != 0)
		    *ep = 0;
		if (index(buf+1, '\t') != 0)
		    tabexpand(buf+1);
		add_menutext(&menu, buf+1);
	    }
	    
	    display_menu(menu.items, menu.icount, NULL, FALSE, NULL);
	    free(menu.items);
	}
    } else if (user_typed_name)
	pline("I don't have any information on those things.");

    dlb_fclose(fp);
}


/* getpos() return values */
#define LOOK_TRADITIONAL	0	/* '.' -- ask about "more info?" */
#define LOOK_QUICK		1	/* ',' -- skip "more info?" */
#define LOOK_ONCE		2	/* ';' -- skip and stop looping */
#define LOOK_VERBOSE		3	/* ':' -- show more info w/o asking */

/* also used by getpos hack in do_name.c */
const char what_is_an_unknown_object[] = "an unknown object";

/* quick: use cursor && don't search for "more info" */
static int do_look(boolean quick)
{
    char out_str[BUFSZ];
    char firstmatch[BUFSZ];
    int i, ans = 0, objplur = 0;
    int found;		/* count of matching syms found */
    coord cc;		/* screen pos of unknown glyph */
    boolean save_verbose;	/* saved value of flags.verbose */
    boolean from_screen;	/* question from the screen */
    boolean need_to_look;	/* need to get explan. from glyph */
    int skipped_venom;		/* non-zero if we ignored "splash of venom" */
    struct nh_desc_buf descbuf;
    struct obj *otmp;

    if (quick) {
	from_screen = TRUE;	/* yes, we want to use the cursor */
    } else {
	i = ynq("Specify unknown object by cursor?");
	if (i == 'q') return 0;
	from_screen = (i == 'y');
    }

    if (from_screen) {
	cc.x = u.ux;
	cc.y = u.uy;
    } else {
	getlin("Specify what? (type the word)", out_str);
	if (out_str[0] == '\0' || out_str[0] == '\033')
	    return 0;

	/* the ability to specify symbols is gone: it is simply impossible to
	 * know how the window port is displaying things (tiles?) and even if
	 * charaters are used it may not be possible to type them (utf8)
	 */
	
	checkfile(out_str, NULL, TRUE, TRUE);
	return 0;
    }
    /* Save the verbose flag, we change it later. */
    save_verbose = flags.verbose;
    flags.verbose = flags.verbose && !quick;
    
    /*
     * we're identifying from the screen.
     */
    do {
	/* Reset some variables. */
	need_to_look = FALSE;
	skipped_venom = 0;
	found = 0;
	out_str[0] = '\0';

	if (flags.verbose)
	    pline("Please move the cursor to %s.",
		    what_is_an_unknown_object);
	else
	    pline("Pick an object.");

	ans = getpos(&cc, quick, what_is_an_unknown_object);
	if (ans < 0 || cc.x < 0) {
	    flags.verbose = save_verbose;
	    return 0;	/* done */
	}
	flags.verbose = FALSE;	/* only print long question once */

	nh_describe_pos(cc.x, cc.y, &descbuf);
	
	otmp = vobj_at(cc.x, cc.y);
	if (otmp && is_plural(otmp))
	    objplur = 1;
	
	out_str[0] = '\0';
	if (append_str(out_str, descbuf.effectdesc, 0))
	    if (++found == 1)
		strcpy (firstmatch, descbuf.effectdesc);
	
	if (append_str(out_str, descbuf.invisdesc, 0))
	    if (++found == 1)
		strcpy (firstmatch, descbuf.invisdesc);
	
	if (append_str(out_str, descbuf.mondesc, 0))
	    if (++found == 1)
		strcpy (firstmatch, descbuf.mondesc);
	
	if (append_str(out_str, descbuf.objdesc, objplur))
	    if (++found == 1)
		strcpy (firstmatch, descbuf.objdesc);
	
	if (append_str(out_str, descbuf.trapdesc, 0))
	    if (++found == 1)
		strcpy (firstmatch, descbuf.trapdesc);
	
	if (append_str(out_str, descbuf.bgdesc, 0))
	    if (!found) {
		found++; /* only increment found if nothing else was seen,
		so that checkfile can be called below */
		strcpy (firstmatch, descbuf.bgdesc);
	    }
	

	/* Finally, print out our explanation. */
	if (found) {
	    pline("%s", out_str);
	    /* check the data file for information about this thing */
	    if (found == 1 && ans != LOOK_QUICK && ans != LOOK_ONCE &&
			(ans == LOOK_VERBOSE || (flags.help && !quick))) {
		checkfile(firstmatch, NULL, FALSE, ans == LOOK_VERBOSE);
	    }
	} else {
	    pline("I've never heard of such things.");
	}
    } while (!quick && ans != LOOK_ONCE);

    flags.verbose = save_verbose;

    return 0;
}


int dowhatis(void)
{
	return do_look(FALSE);
}

int doquickwhatis(void)
{
	return do_look(TRUE);
}

int doidtrap(void)
{
	struct trap *trap;
	int x, y, tt;
	schar dx, dy, dz;

	if (!getdir(NULL, &dx, &dy, &dz))
	    return 0;
	
	x = u.ux + dx;
	y = u.uy + dy;
	for (trap = ftrap; trap; trap = trap->ntrap)
	    if (trap->tx == x && trap->ty == y) {
		if (!trap->tseen) break;
		tt = trap->ttyp;
		if (dz) {
		    if (dz < 0 ? (tt == TRAPDOOR || tt == HOLE) :
			    tt == ROCKTRAP) break;
		}
		tt = what_trap(tt);
		pline("That is %s%s%s.",
		      an(trapexplain[tt - 1]),
		      !trap->madeby_u ? "" : (tt == WEB) ? " woven" :
			  /* trap doors & spiked pits can't be made by
			     player, and should be considered at least
			     as much "set" as "dug" anyway */
			  (tt == HOLE || tt == PIT) ? " dug" : " set",
		      !trap->madeby_u ? "" : " by you");
		return 0;
	    }
	pline("I can't see a trap there.");
	return 0;
}

char *dowhatdoes_core(char q, char *cbuf)
{
	dlb *fp;
	char bufr[BUFSZ];
	char *buf = &bufr[6], *ep, ctrl, meta;

	fp = dlb_fopen(CMDHELPFILE, "r");
	if (!fp) {
		pline("Cannot open data file!");
		return 0;
	}

  	ctrl = ((q <= '\033') ? (q - 1 + 'A') : 0);
	meta = ((0x80 & q) ? (0x7f & q) : 0);
	while (dlb_fgets(buf,BUFSZ-6,fp)) {
	    if ((ctrl && *buf=='^' && *(buf+1)==ctrl) ||
		(meta && *buf=='M' && *(buf+1)=='-' && *(buf+2)==meta) ||
		*buf==q) {
		ep = index(buf, '\n');
		if (ep) *ep = 0;
		if (ctrl && buf[2] == '\t'){
			buf = bufr + 1;
			strncpy(buf, "^?      ", 8);
			buf[1] = ctrl;
		} else if (meta && buf[3] == '\t'){
			buf = bufr + 2;
			strncpy(buf, "M-?     ", 8);
			buf[2] = meta;
		} else if (buf[1] == '\t'){
			buf = bufr;
			buf[0] = q;
			strncpy(buf+1, "       ", 7);
		}
		dlb_fclose(fp);
		strcpy(cbuf, buf);
		return cbuf;
	    }
	}
	dlb_fclose(fp);
	return NULL;
}

int dowhatdoes(void)
{
	char bufr[BUFSZ];
	char q, *reslt;

	q = yn_function("What command?", NULL, '\0');

	reslt = dowhatdoes_core(q, bufr);
	if (reslt)
		pline("%s", reslt);
	else
		pline("I've never heard of such commands.");
	return 0;
}

/* data for help_menu() */
static const char *help_menu_items[] = {
/* 0*/	"Long description of the game and commands.",
/* 1*/	"List of game commands.",
/* 2*/	"Concise history of NetHack.",
/* 3*/	"Info on a character in the game display.",
/* 4*/	"Info on what a given key does.",
/* 5*/	"Longer explanation of game options.",
/* 6*/	"The NetHack license.",
#define WIZHLP_SLOT 7
	"List of wizard-mode commands.",
	"",
	NULL
};

static boolean help_menu(int *sel)
{
	struct nh_menuitem items[SIZE(help_menu_items)];
	int results[SIZE(help_menu_items)];
	int i, n;
	
	if (!wizard) help_menu_items[WIZHLP_SLOT] = "",
		     help_menu_items[WIZHLP_SLOT+1] = NULL;

	for (i = 0; help_menu_items[i]; i++)
	    set_menuitem(&items[i], (*help_menu_items[i]) ? i+1 : 0, MI_NORMAL,
			 help_menu_items[i], 0, FALSE);
	
	n = display_menu(items, i, "Select one item:", PICK_ONE, results);
	if (n > 0) {
	    *sel = results[0] - 1;
	    return TRUE;
	}
	return FALSE;
}

int dohelp(void)
{
	int sel = 0;

	if (help_menu(&sel)) {
		switch (sel) {
			case  0:  display_file(HELP, TRUE);  break;
			case  1:  display_file(SHELP, TRUE);  break;
			case  2:  dohistory();  break;
			case  3:  dowhatis();  break;
			case  4:  dowhatdoes();  break;
			case  5:  display_file(OPTIONFILE, TRUE);  break;
			case  6:  display_file(LICENSE, TRUE);  break;
			/* handle WIZHLP_SLOT */
			default: display_file(DEBUGHELP, TRUE);  break;
		}
	}
	return 0;
}

int dohistory(void)
{
	display_file(HISTORY, TRUE);
	return 0;
}

/*pager.c*/
