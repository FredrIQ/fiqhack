/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static void do_oname(struct obj *);

int getpos(coord *cc, boolean force, const char *goal)
{
	int rv = -1, x, y;
	
	x = cc->x;
	y = cc->y;
	
	flush_screen(0);
	
	while (rv == -1 || x < 1 || y < 1 || x > COLNO || y > ROWNO) {
	    rv = win_getpos(&x, &y, force, goal);
	}
	
	cc->x = x;
	cc->y = y;
	
	return rv;
}


struct monst *christen_monst(struct monst *mtmp, const char *name)
{
	int lth;
	struct monst *mtmp2;
	char buf[PL_PSIZ];

	/* dogname & catname are PL_PSIZ arrays; object names have same limit */
	lth = *name ? (int)(strlen(name) + 1) : 0;
	if(lth > PL_PSIZ){
		lth = PL_PSIZ;
		name = strncpy(buf, name, PL_PSIZ - 1);
		buf[PL_PSIZ - 1] = '\0';
	}
	if (lth == mtmp->mnamelth) {
		/* don't need to allocate a new monst struct */
		if (lth) strcpy(NAME(mtmp), name);
		return mtmp;
	}
	mtmp2 = newmonst(mtmp->mxlth + lth);
	*mtmp2 = *mtmp;
	memcpy(mtmp2->mextra,
		      mtmp->mextra, mtmp->mxlth);
	mtmp2->mnamelth = lth;
	if (lth) strcpy(NAME(mtmp2), name);
	replmon(mtmp,mtmp2);
	return mtmp2;
}


int do_mname(void)
{
	char buf[BUFSZ];
	coord cc;
	int cx,cy;
	struct monst *mtmp;
	char qbuf[QBUFSZ];

	if (Hallucination) {
		You("would never recognize it anyway.");
		return 0;
	}
	cc.x = u.ux;
	cc.y = u.uy;
	if (getpos(&cc, FALSE, "the monster you want to name") < 0 ||
			(cx = cc.x) < 0)
		return 0;
	cy = cc.y;

	if (cx == u.ux && cy == u.uy) {
	    if (u.usteed && canspotmon(u.usteed))
		mtmp = u.usteed;
	    else {
		pline("This %s creature is called %s and cannot be renamed.",
		ACURR(A_CHA) > 14 ?
		(flags.female ? "beautiful" : "handsome") :
		"ugly",
		plname);
		return 0;
	    }
	} else
	    mtmp = m_at(cx, cy);

	if (!mtmp || (!sensemon(mtmp) &&
			(!(cansee(cx,cy) || see_with_infrared(mtmp)) || mtmp->mundetected
			|| mtmp->m_ap_type == M_AP_FURNITURE
			|| mtmp->m_ap_type == M_AP_OBJECT
			|| (mtmp->minvis && !See_invisible)))) {
		pline("I see no monster there.");
		return 0;
	}
	/* special case similar to the one in lookat() */
	distant_monnam(mtmp, ARTICLE_THE, buf);
	sprintf(qbuf, "What do you want to call %s?", buf);
	getlin(qbuf,buf);
	if(!*buf || *buf == '\033') return 0;
	/* strip leading and trailing spaces; unnames monster if all spaces */
	mungspaces(buf);

	if (mtmp->data->geno & G_UNIQ)
	    pline("%s doesn't like being called names!", Monnam(mtmp));
	else
	    christen_monst(mtmp, buf);
	return 0;
}

/*
 * This routine changes the address of obj. Be careful not to call it
 * when there might be pointers around in unknown places. For now: only
 * when obj is in the inventory.
 */
static void do_oname(struct obj *obj)
{
	char buf[BUFSZ], qbuf[QBUFSZ];
	const char *aname;
	short objtyp;

	sprintf(qbuf, "What do you want to name %s %s?",
		is_plural(obj) ? "these" : "this", xname(obj));
	getlin(qbuf, buf);
	if(!*buf || *buf == '\033')	return;
	/* strip leading and trailing spaces; unnames item if all spaces */
	mungspaces(buf);

	/* relax restrictions over proper capitalization for artifacts */
	if ((aname = artifact_name(buf, &objtyp)) != 0 && objtyp == obj->otyp)
		strcpy(buf, aname);

	if (obj->oartifact) {
		pline_The("artifact seems to resist the attempt.");
		return;
	} else if (restrict_name(obj, buf) || exist_artifact(obj->otyp, buf)) {
		int n = rn2((int)strlen(buf));
		char c1, c2;

		c1 = lowc(buf[n]);
		do c2 = 'a' + rn2('z'-'a'); while (c1 == c2);
		buf[n] = (buf[n] == c1) ? c2 : highc(c2);  /* keep same case */
		pline("While engraving your %s slips.", body_part(HAND));
		display_nhwindow(NHW_MESSAGE, FALSE);
		You("engrave: \"%s\".",buf);
	}
	obj = oname(obj, buf);
}

/*
 * Allocate a new and possibly larger storage space for an obj.
 */
struct obj *realloc_obj(struct obj *obj, int oextra_size, void *oextra_src,
			int oname_size, const char *name)
{
	struct obj *otmp;

	otmp = newobj(oextra_size + oname_size);
	*otmp = *obj;	/* the cobj pointer is copied to otmp */
	if (oextra_size) {
	    if (oextra_src)
		memcpy(otmp->oextra, oextra_src,
							oextra_size);
	} else {
	    otmp->oattached = OATTACHED_NOTHING;
	}
	otmp->oxlth = oextra_size;

	otmp->onamelth = oname_size;
	otmp->timed = 0;	/* not timed, yet */
	otmp->lamplit = 0;	/* ditto */
	/* __GNUC__ note:  if the assignment of otmp->onamelth immediately
	   precedes this `if' statement, a gcc bug will miscompile the
	   test on vax (`insv' instruction used to store bitfield does
	   not set condition codes, but optimizer behaves as if it did).
	   gcc-2.7.2.1 finally fixed this. */
	if (oname_size) {
	    if (name)
		strcpy(ONAME(otmp), name);
	}

	if (obj->owornmask) {
		boolean save_twoweap = u.twoweap;
		/* unwearing the old instance will clear dual-wield mode
		   if this object is either of the two weapons */
		setworn(NULL, obj->owornmask);
		setworn(otmp, otmp->owornmask);
		u.twoweap = save_twoweap;
	}

	/* replace obj with otmp */
	replace_object(obj, otmp);

	/* fix ocontainer pointers */
	if (Has_contents(obj)) {
		struct obj *inside;

		for(inside = obj->cobj; inside; inside = inside->nobj)
			inside->ocontainer = otmp;
	}

	/* move timers and light sources from obj to otmp */
	if (obj->timed) obj_move_timers(obj, otmp);
	if (obj->lamplit) obj_move_light_source(obj, otmp);

	/* objects possibly being manipulated by multi-turn occupations
	   which have been interrupted but might be subsequently resumed */
	if (obj->oclass == FOOD_CLASS)
	    food_substitution(obj, otmp);	/* eat food or open tin */
	else if (obj->oclass == SPBOOK_CLASS)
	    book_substitution(obj, otmp);	/* read spellbook */

	/* obfree(obj, otmp);	now unnecessary: no pointers on bill */
	dealloc_obj(obj);	/* let us hope nobody else saved a pointer */
	return otmp;
}

struct obj *oname(struct obj *obj, const char *name)
{
	int lth;
	char buf[PL_PSIZ];

	lth = *name ? (int)(strlen(name) + 1) : 0;
	if (lth > PL_PSIZ) {
		lth = PL_PSIZ;
		name = strncpy(buf, name, PL_PSIZ - 1);
		buf[PL_PSIZ - 1] = '\0';
	}
	/* If named artifact exists in the game, do not create another.
	 * Also trying to create an artifact shouldn't de-artifact
	 * it (e.g. Excalibur from prayer). In this case the object
	 * will retain its current name. */
	if (obj->oartifact || (lth && exist_artifact(obj->otyp, name)))
		return obj;

	if (lth == obj->onamelth) {
		/* no need to replace entire object */
		if (lth) strcpy(ONAME(obj), name);
	} else {
		obj = realloc_obj(obj, obj->oxlth,
			      obj->oextra, lth, name);
	}
	if (lth) artifact_exists(obj, name, TRUE);
	if (obj->oartifact) {
	    /* can't dual-wield with artifact as secondary weapon */
	    if (obj == uswapwep) untwoweapon();
	    /* activate warning if you've just named your weapon "Sting" */
	    if (obj == uwep) set_artifact_intrinsic(obj, TRUE, W_WEP);
	}
	if (carried(obj)) update_inventory();
	return obj;
}

static const char callable[] = {
	SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS, AMULET_CLASS,
	GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS, 0 };


int ddocall(void)
{
	struct obj *obj;
	char allowall[2];

	switch(ynq("Name an individual object?")) {
	case 'q':
		break;
	case 'y':
		allowall[0] = ALL_CLASSES; allowall[1] = '\0';
		obj = getobj(allowall, "name");
		if(obj) do_oname(obj);
		break;
	default :
		obj = getobj(callable, "call");
		if (obj) {
			/* behave as if examining it in inventory;
			   this might set dknown if it was picked up
			   while blind and the hero can now see */
			xname(obj);

			if (!obj->dknown) {
				You("would never recognize another one.");
				return 0;
			}
			docall(obj);
		}
		break;
	}
	return 0;
}


void docall(struct obj *obj)
{
	char buf[BUFSZ], qbuf[QBUFSZ];
	struct obj otemp;
	char **str1;

	if (!obj->dknown) return; /* probably blind */
	otemp = *obj;
	otemp.quan = 1L;
	otemp.onamelth = 0;
	otemp.oxlth = 0;
	if (objects[otemp.otyp].oc_class == POTION_CLASS && otemp.fromsink)
	    /* kludge, meaning it's sink water */
	    sprintf(qbuf,"Call a stream of %s fluid:",
		    OBJ_DESCR(objects[otemp.otyp]));
	else
	    sprintf(qbuf, "Call %s:", an(xname(&otemp)));
	getlin(qbuf, buf);
	if(!*buf || *buf == '\033')
		return;

	/* clear old name */
	str1 = &(objects[obj->otyp].oc_uname);
	if(*str1) free(*str1);

	/* strip leading and trailing spaces; uncalls item if all spaces */
	mungspaces(buf);
	if (!*buf) {
	    if (*str1) {	/* had name, so possibly remove from disco[] */
		/* strip name first, for the update_inventory() call
		   from undiscover_object() */
		*str1 = NULL;
		undiscover_object(obj->otyp);
	    }
	} else {
	    *str1 = strcpy(malloc((unsigned)strlen(buf)+1), buf);
	    discover_object(obj->otyp, FALSE, TRUE); /* possibly add to disco[] */
	}
}


static const char * const ghostnames[] = {
	/* these names should have length < PL_NSIZ */
	/* Capitalize the names for aesthetics -dgk */
	"Adri", "Andries", "Andreas", "Bert", "David", "Dirk", "Emile",
	"Frans", "Fred", "Greg", "Hether", "Jay", "John", "Jon", "Karnov",
	"Kay", "Kenny", "Kevin", "Maud", "Michiel", "Mike", "Peter", "Robert",
	"Ron", "Tom", "Wilmar", "Nick Danger", "Phoenix", "Jiro", "Mizue",
	"Stephan", "Lance Braccus", "Shadowhawk"
};

/* ghost names formerly set by x_monnam(), now by makemon() instead */
const char * rndghostname(void)
{
    return rn2(7) ? ghostnames[rn2(SIZE(ghostnames))] : (const char *)plname;
}

/* Monster naming functions:
 * x_monnam is the generic monster-naming function.
 *		  seen	      unseen	   detected		  named
 * mon_nam:	the newt	it	the invisible orc	Fido
 * noit_mon_nam:the newt (as if detected) the invisible orc	Fido
 * l_monnam:	newt		it	invisible orc		dog called fido
 * Monnam:	The newt	It	The invisible orc	Fido
 * noit_Monnam: The newt (as if detected) The invisible orc	Fido
 * Adjmonnam:	The poor newt	It	The poor invisible orc	The poor Fido
 * Amonnam:	A newt		It	An invisible orc	Fido
 * a_monnam:	a newt		it	an invisible orc	Fido
 * m_monnam:	newt		xan	orc			Fido
 * y_monnam:	your newt     your xan	your invisible orc	Fido
 */

/* Bug: if the monster is a priest or shopkeeper, not every one of these
 * options works, since those are special cases.
 */
char *x_monnam(struct monst *mtmp,
	       int article, /* ARTICLE_NONE, ARTICLE_THE, ARTICLE_A: obvious
	                     * ARTICLE_YOUR: "your" on pets, "the" on everything
			     * else
			     * If the monster would be referred to as "it" or if the monster has a name
			     * _and_ there is no adjective, "invisible", "saddled", etc., override this
			     * and always use no article.
			     */
	       const char *adjective,
	       int suppress, /* SUPPRESS_IT, SUPPRESS_INVISIBLE, SUPPRESS_HALLUCINATION, SUPPRESS_SADDLE.
	                      * EXACT_NAME: combination of all the above
			      */
	       boolean called)
{
	static char buf[BUFSZ];

	struct permonst *mdat = mtmp->data;
	boolean do_hallu, do_invis, do_it, do_saddle;
	boolean name_at_start, has_adjectives;
	char *bp;

	if (program_state.gameover)
	    suppress |= SUPPRESS_HALLUCINATION;
	if (article == ARTICLE_YOUR && !mtmp->mtame)
	    article = ARTICLE_THE;

	do_hallu = Hallucination && !(suppress & SUPPRESS_HALLUCINATION);
	do_invis = mtmp->minvis && !(suppress & SUPPRESS_INVISIBLE);
	do_it = !canspotmon(mtmp) && 
	    article != ARTICLE_YOUR &&
	    !program_state.gameover &&
	    mtmp != u.usteed &&
	    !(u.uswallow && mtmp == u.ustuck) &&
	    !(suppress & SUPPRESS_IT);
	do_saddle = !(suppress & SUPPRESS_SADDLE);

	buf[0] = 0;

	/* unseen monsters, etc.  Use "it" */
	if (do_it) {
	    strcpy(buf, "it");
	    return buf;
	}

	/* priests and minions: don't even use this function */
	if (mtmp->ispriest || mtmp->isminion) {
	    char priestnambuf[BUFSZ];
	    char *name;
	    long save_prop = EHalluc_resistance;
	    unsigned save_invis = mtmp->minvis;

	    /* when true name is wanted, explicitly block Hallucination */
	    if (!do_hallu) EHalluc_resistance = 1L;
	    if (!do_invis) mtmp->minvis = 0;
	    name = priestname(mtmp, priestnambuf);
	    EHalluc_resistance = save_prop;
	    mtmp->minvis = save_invis;
	    if (article == ARTICLE_NONE && !strncmp(name, "the ", 4))
		name += 4;
	    return strcpy(buf, name);
	}

	/* Shopkeepers: use shopkeeper name.  For normal shopkeepers, just
	 * "Asidonhopo"; for unusual ones, "Asidonhopo the invisible
	 * shopkeeper" or "Asidonhopo the blue dragon".  If hallucinating,
	 * none of this applies.
	 */
	if (mtmp->isshk && !do_hallu) {
	    if (adjective && article == ARTICLE_THE) {
		/* pathological case: "the angry Asidonhopo the blue dragon"
		   sounds silly */
		strcpy(buf, "the ");
		strcat(strcat(buf, adjective), " ");
		strcat(buf, shkname(mtmp));
		return buf;
	    }
	    strcat(buf, shkname(mtmp));
	    if (mdat == &mons[PM_SHOPKEEPER] && !do_invis)
		return buf;
	    strcat(buf, " the ");
	    if (do_invis)
		strcat(buf, "invisible ");
	    strcat(buf, mdat->mname);
	    return buf;
	}

	/* Put the adjectives in the buffer */
	if (adjective)
	    strcat(strcat(buf, adjective), " ");
	if (do_invis)
	    strcat(buf, "invisible ");

	if (do_saddle && (mtmp->misc_worn_check & W_SADDLE) &&
	    !Blind && !Hallucination)
	    strcat(buf, "saddled ");

	if (buf[0] != 0)
	    has_adjectives = TRUE;
	else
	    has_adjectives = FALSE;

	/* Put the actual monster name or type into the buffer now */
	/* Be sure to remember whether the buffer starts with a name */
	if (do_hallu) {
	    strcat(buf, rndmonnam());
	    name_at_start = FALSE;
	} else if (mtmp->mnamelth) {
	    char *name = NAME(mtmp);

	    if (mdat == &mons[PM_GHOST]) {
		sprintf(eos(buf), "%s ghost", s_suffix(name));
		name_at_start = TRUE;
	    } else if (called) {
		sprintf(eos(buf), "%s called %s", mdat->mname, name);
		name_at_start = (boolean)type_is_pname(mdat);
	    } else if (is_mplayer(mdat) && (bp = strstri(name, " the ")) != 0) {
		/* <name> the <adjective> <invisible> <saddled> <rank> */
		char pbuf[BUFSZ];

		strcpy(pbuf, name);
		pbuf[bp - name + 5] = '\0'; /* adjectives right after " the " */
		if (has_adjectives)
		    strcat(pbuf, buf);
		strcat(pbuf, bp + 5);	/* append the rest of the name */
		strcpy(buf, pbuf);
		article = ARTICLE_NONE;
		name_at_start = TRUE;
	    } else {
		strcat(buf, name);
		name_at_start = TRUE;
	    }
	} else if (is_mplayer(mdat) && !In_endgame(&u.uz)) {
	    char pbuf[BUFSZ];
	    strcpy(pbuf, rank_of((int)mtmp->m_lev,
				 monsndx(mdat),
				 (boolean)mtmp->female));
	    strcat(buf, lcase(pbuf));
	    name_at_start = FALSE;
	} else {
	    strcat(buf, mdat->mname);
	    name_at_start = (boolean)type_is_pname(mdat);
	}

	if (name_at_start && (article == ARTICLE_YOUR || !has_adjectives)) {
	    if (mdat == &mons[PM_WIZARD_OF_YENDOR])
		article = ARTICLE_THE;
	    else
		article = ARTICLE_NONE;
	} else if ((mdat->geno & G_UNIQ) && article == ARTICLE_A) {
	    article = ARTICLE_THE;
	}

	{
	    char buf2[BUFSZ];

	    switch(article) {
		case ARTICLE_YOUR:
		    strcpy(buf2, "your ");
		    strcat(buf2, buf);
		    strcpy(buf, buf2);
		    return buf;
		case ARTICLE_THE:
		    strcpy(buf2, "the ");
		    strcat(buf2, buf);
		    strcpy(buf, buf2);
		    return buf;
		case ARTICLE_A:
		    return an(buf);
		case ARTICLE_NONE:
		default:
		    return buf;
	    }
	}
}


char *l_monnam(struct monst *mtmp)
{
	return(x_monnam(mtmp, ARTICLE_NONE, NULL, 
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, TRUE));
}


char *mon_nam(struct monst *mtmp)
{
	return(x_monnam(mtmp, ARTICLE_THE, NULL,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE));
}

/* print the name as if mon_nam() was called, but assume that the player
 * can always see the monster--used for probing and for monsters aggravating
 * the player with a cursed potion of invisibility
 */
char *noit_mon_nam(struct monst *mtmp)
{
	return(x_monnam(mtmp, ARTICLE_THE, NULL,
		mtmp->mnamelth ? (SUPPRESS_SADDLE|SUPPRESS_IT) :
		    SUPPRESS_IT, FALSE));
}

char *Monnam(struct monst *mtmp)
{
	char *bp = mon_nam(mtmp);

	*bp = highc(*bp);
	return bp;
}

char *noit_Monnam(struct monst *mtmp)
{
	char *bp = noit_mon_nam(mtmp);

	*bp = highc(*bp);
	return bp;
}

/* monster's own name */
char *m_monnam(struct monst *mtmp)
{
	return x_monnam(mtmp, ARTICLE_NONE, NULL, EXACT_NAME, FALSE);
}

/* pet name: "your little dog" */
char *y_monnam(struct monst *mtmp)
{
	int prefix, suppression_flag;

	prefix = mtmp->mtame ? ARTICLE_YOUR : ARTICLE_THE;
	/* "saddled" is redundant when mounted */
	suppression_flag = (mtmp->mnamelth || mtmp == u.usteed) ? SUPPRESS_SADDLE : 0;

	return x_monnam(mtmp, prefix, NULL, suppression_flag, FALSE);
}


char *Adjmonnam(struct monst *mtmp, const char *adj)
{
	char *bp = x_monnam(mtmp, ARTICLE_THE, adj,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE);

	*bp = highc(*bp);
	return bp;
}

char *a_monnam(struct monst *mtmp)
{
	return x_monnam(mtmp, ARTICLE_A, NULL,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE);
}

char *Amonnam(struct monst *mtmp)
{
	char *bp = a_monnam(mtmp);

	*bp = highc(*bp);
	return bp;
}

/* used for monster ID by the '/', ';', and 'C' commands to block remote
   identification of the endgame altars via their attending priests */
/* article: only ARTICLE_NONE and ARTICLE_THE are handled here */
char *distant_monnam(struct monst *mon, int article, char *outbuf)
{
    /* high priest(ess)'s identity is concealed on the Astral Plane,
       unless you're adjacent (overridden for hallucination which does
       its own obfuscation) */
    if (mon->data == &mons[PM_HIGH_PRIEST] && !Hallucination &&
	    Is_astralevel(&u.uz) && distu(mon->mx, mon->my) > 2) {
	strcpy(outbuf, article == ARTICLE_THE ? "the " : "");
	strcat(outbuf, mon->female ? "high priestess" : "high priest");
    } else {
	strcpy(outbuf, x_monnam(mon, article, NULL, 0, TRUE));
    }
    return outbuf;
}

static const char * const bogusmons[] = {
	"jumbo shrimp", "giant pigmy", "gnu", "killer penguin",
	"giant cockroach", "giant slug", "maggot", "pterodactyl",
	"tyrannosaurus rex", "basilisk", "beholder", "nightmare",
	"efreeti", "marid", "rot grub", "bookworm", "master lichen",
	"shadow", "hologram", "jester", "attorney", "sleazoid",
	"killer tomato", "amazon", "robot", "battlemech",
	"rhinovirus", "harpy", "lion-dog", "rat-ant", "Y2K bug",
						/* misc. */
	"grue", "Christmas-tree monster", "luck sucker", "paskald",
	"brogmoid", "dornbeast",		/* Quendor (Zork, &c.) */
	"Ancient Multi-Hued Dragon", "Evil Iggy",
						/* Moria */
	"emu", "kestrel", "xeroc", "venus flytrap",
						/* Rogue */
	"creeping coins",			/* Wizardry */
	"hydra", "siren",			/* Greek legend */
	"killer bunny",				/* Monty Python */
	"rodent of unusual size",		/* The Princess Bride */
	"Smokey the bear",	/* "Only you can prevent forest fires!" */
	"Luggage",				/* Discworld */
	"Ent",					/* Lord of the Rings */
	"tangle tree", "nickelpede", "wiggle",	/* Xanth */
	"white rabbit", "snark",		/* Lewis Carroll */
	"pushmi-pullyu",			/* Dr. Doolittle */
	"smurf",				/* The Smurfs */
	"tribble", "Klingon", "Borg",		/* Star Trek */
	"Ewok",					/* Star Wars */
	"Totoro",				/* Tonari no Totoro */
	"ohmu",					/* Nausicaa */
	"youma",				/* Sailor Moon */
	"nyaasu",				/* Pokemon (Meowth) */
	"Godzilla", "King Kong",		/* monster movies */
	"earthquake beast",			/* old L of SH */
	"Invid",				/* Robotech */
	"Terminator",				/* The Terminator */
	"boomer",				/* Bubblegum Crisis */
	"Dalek",				/* Dr. Who ("Exterminate!") */
	"microscopic space fleet", "Ravenous Bugblatter Beast of Traal",
						/* HGttG */
	"teenage mutant ninja turtle",		/* TMNT */
	"samurai rabbit",			/* Usagi Yojimbo */
	"aardvark",				/* Cerebus */
	"Audrey II",				/* Little Shop of Horrors */
	"witch doctor", "one-eyed one-horned flying purple people eater",
						/* 50's rock 'n' roll */
	"Barney the dinosaur",			/* saccharine kiddy TV */
	"Morgoth",				/* Angband */
	"Vorlon",				/* Babylon 5 */
	"questing beast",		/* King Arthur */
	"Predator",				/* Movie */
	"mother-in-law"				/* common pest */
};


/* Return a random monster name, for hallucination.
 * KNOWN BUG: May be a proper name (Godzilla, Barney), may not
 * (the Terminator, a Dalek).  There's no elegant way to deal
 * with this without radically modifying the calling functions.
 */
const char *rndmonnam(void)
{
	int name;

	do {
	    name = rn1(SPECIAL_PM + SIZE(bogusmons) - LOW_PM, LOW_PM);
	} while (name < SPECIAL_PM &&
	    (type_is_pname(&mons[name]) || (mons[name].geno & G_NOGEN)));

	if (name >= SPECIAL_PM) return bogusmons[name - SPECIAL_PM];
	return mons[name].mname;
}


const char *roguename(void) /* Name of a Rogue player */
{
	char *i, *opts;

	if ((opts = nh_getenv("ROGUEOPTS")) != 0) {
		for (i = opts; *i; i++)
			if (!strncmp("name=",i,5)) {
				char *j;
				if ((j = index(i+5,',')) != 0)
					*j = (char)0;
				return i+5;
			}
	}
	return rn2(3) ? (rn2(2) ? "Michael Toy" : "Kenneth Arnold")
		: "Glenn Wichman";
}

static const char * const hcolors[] = {
	"ultraviolet", "infrared", "bluish-orange",
	"reddish-green", "dark white", "light black", "sky blue-pink",
	"salty", "sweet", "sour", "bitter",
	"striped", "spiral", "swirly", "plaid", "checkered", "argyle",
	"paisley", "blotchy", "guernsey-spotted", "polka-dotted",
	"square", "round", "triangular",
	"cabernet", "sangria", "fuchsia", "wisteria",
	"lemon-lime", "strawberry-banana", "peppermint",
	"romantic", "incandescent"
};

const char *hcolor(const char *colorpref)
{
	return (Hallucination || !colorpref) ?
		hcolors[rn2(SIZE(hcolors))] : colorpref;
}

/* return a random real color unless hallucinating */
const char *rndcolor(void)
{
	int k = rn2(CLR_MAX);
	return Hallucination ? hcolor(NULL) : (k == NO_COLOR) ?
		"colorless" : c_obj_colors[k];
}

/* Aliases for road-runner nemesis
 */
static const char * const coynames[] = {
	"Carnivorous Vulgaris","Road-Runnerus Digestus",
	"Eatibus Anythingus"  ,"Famishus-Famishus",
	"Eatibus Almost Anythingus","Eatius Birdius",
	"Famishius Fantasticus","Eternalii Famishiis",
	"Famishus Vulgarus","Famishius Vulgaris Ingeniusi",
	"Eatius-Slobbius","Hardheadipus Oedipus",
	"Carnivorous Slobbius","Hard-Headipus Ravenus",
	"Evereadii Eatibus","Apetitius Giganticus",
	"Hungrii Flea-Bagius","Overconfidentii Vulgaris",
	"Caninus Nervous Rex","Grotesques Appetitus",
	"Nemesis Riduclii","Canis latrans"
};
	
char *coyotename(struct monst *mtmp, char *buf)
{
    if (mtmp && buf) {
	sprintf(buf, "%s - %s",
	    x_monnam(mtmp, ARTICLE_NONE, NULL, 0, TRUE),
	    mtmp->mcan ? coynames[SIZE(coynames)-1] : coynames[rn2(SIZE(coynames)-1)]);
    }
    return buf;
}

/*do_name.c*/
