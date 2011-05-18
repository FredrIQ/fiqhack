/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "func_tab.h"
/* #define DEBUG */	/* uncomment for debugging */

/*
 * Some systems may have getchar() return EOF for various reasons, and
 * we should not quit before seeing at least NR_OF_EOFS consecutive EOFs.
 */
#define NR_OF_EOFS	20

#define CMD_TRAVEL (char)0x90

#ifdef DEBUG
/*
 * only one "wiz_debug_cmd" routine should be available (in whatever
 * module you are trying to debug) or things are going to get rather
 * hard to link :-)
 */
extern int wiz_debug_cmd(void);
#endif

static int (*timed_occ_fn)(void);

static int doprev_message(void);
static int timed_occupation(void);
static int doextcmd(void);
static int domonability(void);
static int dotravel(void);
static int wiz_wish(void);
static int wiz_identify(void);
static int wiz_map(void);
static int wiz_genesis(void);
static int wiz_where(void);
static int wiz_detect(void);
static int wiz_panic(void);
static int wiz_polyself(void);
static int wiz_level_tele(void);
static int wiz_level_change(void);
static int wiz_show_seenv(void);
static int wiz_show_vision(void);
static int wiz_mon_polycontrol(void);
static int wiz_show_wmodes(void);
#ifdef DEBUG_MIGRATING_MONS
static int wiz_migrate_mons(void);
#endif
static void count_obj(struct obj *, long *, long *, boolean, boolean);
static void obj_chain(struct menulist *, const char *, struct obj *, long *, long *);
static void mon_invent_chain(struct menulist *, const char *, struct monst *,
			     long *, long *);
static void mon_chain(struct menulist *, const char *, struct monst *, long *, long *);
static void contained(struct menulist *, const char *, long *, long *);
static int wiz_show_stats(void);
static int enter_explore_mode(void);
static int doattributes(void);
static int doconduct(void); /**/
static boolean minimal_enlightenment(void);

static void enlght_line(struct menulist *,const char *,const char *,const char *);
static char *enlght_combatinc(const char *,int,int,char *);
#ifdef UNIX
static void end_of_input(void);
#endif

static const char* readchar_queue="";

static boolean help_dir(char, const char *);


static int doprev_message(void)
{
    return nh_doprev_message();
}

/* Count down by decrementing multi */
static int timed_occupation(void)
{
	(*timed_occ_fn)();
	if (multi > 0)
		multi--;
	return multi > 0;
}

/* If you have moved since initially setting some occupations, they
 * now shouldn't be able to restart.
 *
 * The basic rule is that if you are carrying it, you can continue
 * since it is with you.  If you are acting on something at a distance,
 * your orientation to it must have changed when you moved.
 *
 * The exception to this is taking off items, since they can be taken
 * off in a number of ways in the intervening time, screwing up ordering.
 *
 *	Currently:	Take off all armor.
 *			Picking Locks / Forcing Chests.
 *			Setting traps.
 */
void reset_occupations(void)
{
	reset_remarm();
	reset_pick();
	reset_trapset();
}

/* If a time is given, use it to timeout this function, otherwise the
 * function times out by its own means.
 */
void set_occupation(int (*fn)(void), const char *txt, int xtime)
{
	if (xtime) {
		occupation = timed_occupation;
		timed_occ_fn = fn;
	} else
		occupation = fn;
	occtxt = txt;
	occtime = 0;
	return;
}


static int doextcmd(void) /* here after # - now read a full-word command */
{
	int idx, size, retval;
	const char **namelist, **desclist;
	
	size = 0;
	while (extcmdlist[size].ef_txt)
	    size++;
	namelist = malloc((size+1) * sizeof(char*));
	desclist = malloc((size+1) * sizeof(char*));
	
	for (idx = 0; idx < size; idx++) {
	    namelist[idx] = extcmdlist[idx].ef_txt;
	    desclist[idx] = extcmdlist[idx].ef_desc;
	}
	    

	/* keep repeating until we don't run help or quit */
	do {
	    idx = get_ext_cmd(namelist, desclist, size);
	    if (idx < 0) return 0;	/* quit */

	    retval = (*extcmdlist[idx].ef_funct)();
	} while (extcmdlist[idx].ef_funct == doextlist);

	free(namelist);
	free(desclist);
	
	return retval;
}

int doextlist(void)	/* here after #? - now list all full-word commands */
{
	const struct ext_func_tab *efp;
	char	 buf[BUFSZ];
	struct menulist menu;
	init_menulist(&menu);
	
	add_menutext(&menu, "");
	add_menutext(&menu, "            Extended Commands List");
	add_menutext(&menu, "");
	add_menutext(&menu, "    Press '#', then type:");
	add_menutext(&menu, "");

	for(efp = extcmdlist; efp->ef_txt; efp++) {
		sprintf(buf, "    %-15s - %s.", efp->ef_txt, efp->ef_desc);
		add_menutext(&menu, buf);
	}
	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return 0;
}


/* #monster command - use special monster ability while polymorphed */
static int domonability(void)
{
	if (can_breathe(youmonst.data)) return dobreathe();
	else if (attacktype(youmonst.data, AT_SPIT)) return dospit();
	else if (youmonst.data->mlet == S_NYMPH) return doremove();
	else if (attacktype(youmonst.data, AT_GAZE)) return dogaze();
	else if (is_were(youmonst.data)) return dosummon();
	else if (webmaker(youmonst.data)) return dospinweb();
	else if (is_hider(youmonst.data)) return dohide();
	else if (is_mind_flayer(youmonst.data)) return domindblast();
	else if (u.umonnum == PM_GREMLIN) {
	    if(IS_FOUNTAIN(levl[u.ux][u.uy].typ)) {
		if (split_mon(&youmonst, NULL))
		    dryup(u.ux, u.uy, TRUE);
	    } else There("is no fountain here.");
	} else if (is_unicorn(youmonst.data)) {
	    use_unicorn_horn(NULL);
	    return 1;
	} else if (youmonst.data->msound == MS_SHRIEK) {
	    You("shriek.");
	    if(u.uburied)
		pline("Unfortunately sound does not carry well through rock.");
	    else aggravate();
	} else if (Upolyd)
		pline("Any special ability you may have is purely reflexive.");
	else You("don't have a special ability in your normal form!");
	return 0;
}

static int enter_explore_mode(void)
{
	if(!discover && !wizard) {
		pline("Beware!  From explore mode there will be no return to normal game.");
		if (yn("Do you want to enter explore mode?") == 'y') {
			clear_nhwindow(NHW_MESSAGE);
			You("are now in non-scoring explore mode.");
			discover = TRUE;
		}
		else {
			clear_nhwindow(NHW_MESSAGE);
			pline("Resuming normal game.");
		}
	}
	return 0;
}


/* ^W command - wish for something */
static int wiz_wish(void)	/* Unlimited wishes for debug mode by Paul Polderman */
{
	if (wizard) {
	    boolean save_verbose = flags.verbose;

	    flags.verbose = FALSE;
	    makewish();
	    flags.verbose = save_verbose;
	    encumber_msg();
	} else
	    pline("Unavailable command '^W'.");
	return 0;
}

/* ^I command - identify hero's inventory */
static int wiz_identify(void)
{
	if (wizard)	identify_pack(0);
	else		pline("Unavailable command '^I'.");
	return 0;
}

/* ^F command - reveal the level map and any traps on it */
static int wiz_map(void)
{
	if (wizard) {
	    struct trap *t;
	    long save_Hconf = HConfusion,
		 save_Hhallu = HHallucination;

	    HConfusion = HHallucination = 0L;
	    for (t = ftrap; t != 0; t = t->ntrap) {
		t->tseen = 1;
		map_trap(t, TRUE);
	    }
	    do_mapping();
	    HConfusion = save_Hconf;
	    HHallucination = save_Hhallu;
	} else
	    pline("Unavailable command '^F'.");
	return 0;
}

/* ^G command - generate monster(s); a count prefix will be honored */
static int wiz_genesis(void)
{
	if (wizard)	create_particular();
	else		pline("Unavailable command '^G'.");
	return 0;
}

/* ^O command - display dungeon layout */
static int wiz_where(void)
{
	if (wizard) print_dungeon(FALSE, NULL, NULL);
	else	    pline("Unavailable command '^O'.");
	return 0;
}

/* ^E command - detect unseen (secret doors, traps, hidden monsters) */
static int wiz_detect(void)
{
	if(wizard)  findit();
	else	    pline("Unavailable command '^E'.");
	return 0;
}

/* ^V command - level teleport */
static int wiz_level_tele(void)
{
	if (wizard)	level_tele();
	else		pline("Unavailable command '^V'.");
	return 0;
}

/* #monpolycontrol command - choose new form for shapechangers, polymorphees */
static int wiz_mon_polycontrol(void)
{
    iflags.mon_polycontrol = !iflags.mon_polycontrol;
    pline("Monster polymorph control is %s.",
	  iflags.mon_polycontrol ? "on" : "off");
    return 0;
}

/* #levelchange command - adjust hero's experience level */
static int wiz_level_change(void)
{
    char buf[BUFSZ];
    int newlevel;
    int ret;

    getlin("To what experience level do you want to be set?", buf);
    mungspaces(buf);
    if (buf[0] == '\033' || buf[0] == '\0') ret = 0;
    else ret = sscanf(buf, "%d", &newlevel);

    if (ret != 1) {
	pline(Never_mind);
	return 0;
    }
    if (newlevel == u.ulevel) {
	You("are already that experienced.");
    } else if (newlevel < u.ulevel) {
	if (u.ulevel == 1) {
	    You("are already as inexperienced as you can get.");
	    return 0;
	}
	if (newlevel < 1) newlevel = 1;
	while (u.ulevel > newlevel)
	    losexp("#levelchange");
    } else {
	if (u.ulevel >= MAXULEV) {
	    You("are already as experienced as you can get.");
	    return 0;
	}
	if (newlevel > MAXULEV) newlevel = MAXULEV;
	while (u.ulevel < newlevel)
	    pluslvl(FALSE);
    }
    u.ulevelmax = u.ulevel;
    return 0;
}

/* #panic command - test program's panic handling */
static int wiz_panic(void)
{
	if (yn("Do you want to call panic() and end your game?") == 'y')
		panic("crash test.");
        return 0;
}

/* #polyself command - change hero's form */
static int wiz_polyself(void)
{
        polyself(TRUE);
        return 0;
}

/* #seenv command */
static int wiz_show_seenv(void)
{
	struct menulist menu;
	int x, y, v, startx, stopx, curx;
	char row[COLNO+1];

	init_menulist(&menu);
	/*
	 * Each seenv description takes up 2 characters, so center
	 * the seenv display around the hero.
	 */
	startx = max(1, u.ux-(COLNO/4));
	stopx = min(startx+(COLNO/2), COLNO);
	/* can't have a line exactly 80 chars long */
	if (stopx - startx == COLNO/2) startx++;

	for (y = 0; y < ROWNO; y++) {
	    for (x = startx, curx = 0; x < stopx; x++, curx += 2) {
		if (x == u.ux && y == u.uy) {
		    row[curx] = row[curx+1] = '@';
		} else {
		    v = levl[x][y].seenv & 0xff;
		    if (v == 0)
			row[curx] = row[curx+1] = ' ';
		    else
			sprintf(&row[curx], "%02x", v);
		}
	    }
	    /* remove trailing spaces */
	    for (x = curx-1; x >= 0; x--)
		if (row[x] != ' ') break;
	    row[x+1] = '\0';

	    add_menutext(&menu, row);
	}
	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return 0;
}

/* #vision command */
static int wiz_show_vision(void)
{
	struct menulist menu;
	int x, y, v;
	char row[COLNO+1];

	init_menulist(&menu);
	sprintf(row, "Flags: 0x%x could see, 0x%x in sight, 0x%x temp lit",
		COULD_SEE, IN_SIGHT, TEMP_LIT);
	add_menutext(&menu, row);
	add_menutext(&menu, "");
	for (y = 0; y < ROWNO; y++) {
	    for (x = 1; x < COLNO; x++) {
		if (x == u.ux && y == u.uy)
		    row[x] = '@';
		else {
		    v = viz_array[y][x]; /* data access should be hidden */
		    if (v == 0)
			row[x] = ' ';
		    else
			row[x] = '0' + viz_array[y][x];
		}
	    }
	    /* remove trailing spaces */
	    for (x = COLNO-1; x >= 1; x--)
		if (row[x] != ' ') break;
	    row[x+1] = '\0';

	    add_menutext(&menu, &row[1]);
	}
	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return 0;
}

/* #wmode command */
static int wiz_show_wmodes(void)
{
	struct menulist menu;
	int x,y;
	char row[COLNO+1];
	struct rm *lev;

	init_menulist(&menu);
	for (y = 0; y < ROWNO; y++) {
	    for (x = 0; x < COLNO; x++) {
		lev = &levl[x][y];
		if (x == u.ux && y == u.uy)
		    row[x] = '@';
		else if (IS_WALL(lev->typ) || lev->typ == SDOOR)
		    row[x] = '0' + (lev->wall_info & WM_MASK);
		else if (lev->typ == CORR)
		    row[x] = '#';
		else if (IS_ROOM(lev->typ) || IS_DOOR(lev->typ))
		    row[x] = '.';
		else
		    row[x] = 'x';
	    }
	    row[COLNO] = '\0';
	    add_menutext(&menu, row);
	}
	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return 0;
}


/* -enlightenment and conduct- */
static const char
	You_[] = "You ",
	are[]  = "are ",  were[]  = "were ",
	have[] = "have ", had[]   = "had ",
	can[]  = "can ",  could[] = "could ";
static const char
	have_been[]  = "have been ",
	have_never[] = "have never ", never[] = "never ";

#define enl_msg(menu,prefix,present,past,suffix) \
			enlght_line(menu,prefix, final ? past : present, suffix)
#define you_are(menu,attr)	enl_msg(menu,You_,are,were,attr)
#define you_have(menu,attr)	enl_msg(menu,You_,have,had,attr)
#define you_can(menu,attr)	enl_msg(menu,You_,can,could,attr)
#define you_have_been(menu,goodthing) enl_msg(menu,You_,have_been,were,goodthing)
#define you_have_never(menu,badthing) enl_msg(menu,You_,have_never,never,badthing)
#define you_have_X(menu,something)	enl_msg(menu,You_,have,(const char *)"",something)

static void enlght_line(struct menulist *menu,
			const char *start, const char *middle, const char *end)
{
	char buf[BUFSZ];

	sprintf(buf, "%s%s%s.", start, middle, end);
	add_menutext(menu, buf);
}

/* format increased damage or chance to hit */
static char *enlght_combatinc(const char *inctyp, int incamt, int final, char *outbuf)
{
	char numbuf[24];
	const char *modif, *bonus;

	if (final || wizard) {
	    sprintf(numbuf, "%s%d",
		    (incamt > 0) ? "+" : "", incamt);
	    modif = (const char *) numbuf;
	} else {
	    int absamt = abs(incamt);

	    if (absamt <= 3) modif = "small";
	    else if (absamt <= 6) modif = "moderate";
	    else if (absamt <= 12) modif = "large";
	    else modif = "huge";
	}
	bonus = (incamt > 0) ? "bonus" : "penalty";
	/* "bonus to hit" vs "damage bonus" */
	if (!strcmp(inctyp, "damage")) {
	    const char *ctmp = inctyp;
	    inctyp = bonus;
	    bonus = ctmp;
	}
	sprintf(outbuf, "%s %s %s", an(modif), bonus, inctyp);
	return outbuf;
}


void enlightenment(int final)
    /* final: 0 => still in progress; 1 => over, survived; 2 => dead */
{
	int ltmp;
	char buf[BUFSZ];
	struct menulist menu;

	init_menulist(&menu);
	add_menutext(&menu, final ? "Final Attributes:" : "Current Attributes:");
	add_menutext(&menu, "");

	if (flags.elbereth_enabled && u.uevent.uhand_of_elbereth) {
	    static const char * const hofe_titles[3] = {
				"the Hand of Elbereth",
				"the Envoy of Balance",
				"the Glory of Arioch"
	    };
	    you_are(&menu, hofe_titles[u.uevent.uhand_of_elbereth - 1]);
	}

	/* note: piousness 20 matches MIN_QUEST_ALIGN (quest.h) */
	if (u.ualign.record >= 20)	you_are(&menu, "piously aligned");
	else if (u.ualign.record > 13)	you_are(&menu, "devoutly aligned");
	else if (u.ualign.record > 8)	you_are(&menu, "fervently aligned");
	else if (u.ualign.record > 3)	you_are(&menu, "stridently aligned");
	else if (u.ualign.record == 3)	you_are(&menu, "aligned");
	else if (u.ualign.record > 0)	you_are(&menu, "haltingly aligned");
	else if (u.ualign.record == 0)	you_are(&menu, "nominally aligned");
	else if (u.ualign.record >= -3)	you_have(&menu, "strayed");
	else if (u.ualign.record >= -8)	you_have(&menu, "sinned");
	else you_have(&menu, "transgressed");
	if (wizard) {
		sprintf(buf, " %d", u.ualign.record);
		enl_msg(&menu, "Your alignment ", "is", "was", buf);
	}

	/*** Resistances to troubles ***/
	if (Fire_resistance) you_are(&menu, "fire resistant");
	if (Cold_resistance) you_are(&menu, "cold resistant");
	if (Sleep_resistance) you_are(&menu, "sleep resistant");
	if (Disint_resistance) you_are(&menu, "disintegration-resistant");
	if (Shock_resistance) you_are(&menu, "shock resistant");
	if (Poison_resistance) you_are(&menu, "poison resistant");
	if (Drain_resistance) you_are(&menu, "level-drain resistant");
	if (Sick_resistance) you_are(&menu, "immune to sickness");
	if (Antimagic) you_are(&menu, "magic-protected");
	if (Acid_resistance) you_are(&menu, "acid resistant");
	if (Stone_resistance)
		you_are(&menu, "petrification resistant");
	if (Invulnerable) you_are(&menu, "invulnerable");
	if (u.uedibility) you_can(&menu, "recognize detrimental food");

	/*** Troubles ***/
	if (Halluc_resistance)
		enl_msg(&menu, "You resist", "", "ed", " hallucinations");
	if (final) {
		if (Hallucination) you_are(&menu, "hallucinating");
		if (Stunned) you_are(&menu, "stunned");
		if (Confusion) you_are(&menu, "confused");
		if (Blinded) you_are(&menu, "blinded");
		if (Sick) {
			if (u.usick_type & SICK_VOMITABLE)
				you_are(&menu, "sick from food poisoning");
			if (u.usick_type & SICK_NONVOMITABLE)
				you_are(&menu, "sick from illness");
		}
	}
	if (Stoned) you_are(&menu, "turning to stone");
	if (Slimed) you_are(&menu, "turning into slime");
	if (Strangled) you_are(&menu, (u.uburied) ? "buried" : "being strangled");
	if (Glib) {
		sprintf(buf, "slippery %s", makeplural(body_part(FINGER)));
		you_have(&menu, buf);
	}
	if (Fumbling) enl_msg(&menu, "You fumble", "", "d", "");
	if (Wounded_legs && !u.usteed) {
		sprintf(buf, "wounded %s", makeplural(body_part(LEG)));
		you_have(&menu, buf);
	}
	if (Wounded_legs && u.usteed && wizard) {
	    strcpy(buf, x_monnam(u.usteed, ARTICLE_YOUR, NULL, 
		    SUPPRESS_SADDLE | SUPPRESS_HALLUCINATION, FALSE));
	    *buf = highc(*buf);
	    enl_msg(&menu, buf, " has", " had", " wounded legs");
	}
	
	if (Sleeping) enl_msg(&menu, "You ", "fall", "fell", " asleep");
	if (Hunger) enl_msg(&menu, "You hunger", "", "ed", " rapidly");

	/*** Vision and senses ***/
	if (See_invisible) enl_msg(&menu, You_, "see", "saw", " invisible");
	if (Blind_telepat) you_are(&menu, "telepathic");
	if (Warning) you_are(&menu, "warned");
	if (Warn_of_mon && flags.warntype) {
		sprintf(buf, "aware of the presence of %s",
			(flags.warntype & M2_ORC) ? "orcs" :
			(flags.warntype & M2_DEMON) ? "demons" :
			something); 
		you_are(&menu, buf);
	}
	if (Undead_warning) you_are(&menu, "warned of undead");
	if (Searching) you_have(&menu, "automatic searching");
	if (Clairvoyant) you_are(&menu, "clairvoyant");
	if (Infravision) you_have(&menu, "infravision");
	if (Detect_monsters) you_are(&menu, "sensing the presence of monsters");
	if (u.umconf) you_are(&menu, "going to confuse monsters");

	/*** Appearance and behavior ***/
	if (Adornment) {
	    int adorn = 0;

	    if(uleft && uleft->otyp == RIN_ADORNMENT) adorn += uleft->spe;
	    if(uright && uright->otyp == RIN_ADORNMENT) adorn += uright->spe;
	    if (adorn < 0)
		you_are(&menu, "poorly adorned");
	    else
		you_are(&menu, "adorned");
	}
	if (Invisible) you_are(&menu, "invisible");
	else if (Invis) you_are(&menu, "invisible to others");
	/* ordinarily "visible" is redundant; this is a special case for
	   the situation when invisibility would be an expected attribute */
	else if ((HInvis || EInvis || pm_invisible(youmonst.data)) && BInvis)
	    you_are(&menu, "visible");
	if (Displaced) you_are(&menu, "displaced");
	if (Stealth) you_are(&menu, "stealthy");
	if (Aggravate_monster) enl_msg(&menu, "You aggravate", "", "d", " monsters");
	if (Conflict) enl_msg(&menu, "You cause", "", "d", " conflict");

	/*** Transportation ***/
	if (Jumping) you_can(&menu, "jump");
	if (Teleportation) you_can(&menu, "teleport");
	if (Teleport_control) you_have(&menu, "teleport control");
	if (Lev_at_will) you_are(&menu, "levitating, at will");
	else if (Levitation) you_are(&menu, "levitating");	/* without control */
	else if (Flying) you_can(&menu, "fly");
	if (Wwalking) you_can(&menu, "walk on water");
	if (Swimming) you_can(&menu, "swim");        
	if (Breathless) you_can(&menu, "survive without air");
	else if (Amphibious) you_can(&menu, "breathe water");
	if (Passes_walls) you_can(&menu, "walk through walls");
	
	/* If you die while dismounting, u.usteed is still set.  Since several
	 * places in the done() sequence depend on u.usteed, just detect this
	 * special case. */
	if (u.usteed && (final < 2 || strcmp(killer, "riding accident"))) {
	    sprintf(buf, "riding %s", y_monnam(u.usteed));
	    you_are(&menu, buf);
	}
	if (u.uswallow) {
	    sprintf(buf, "swallowed by %s", a_monnam(u.ustuck));
	    if (wizard) sprintf(eos(buf), " (%u)", u.uswldtim);
	    you_are(&menu, buf);
	} else if (u.ustuck) {
	    sprintf(buf, "%s %s",
		    (Upolyd && sticks(youmonst.data)) ? "holding" : "held by",
		    a_monnam(u.ustuck));
	    you_are(&menu, buf);
	}

	/*** Physical attributes ***/
	if (u.uhitinc)
	    you_have(&menu, enlght_combatinc("to hit", u.uhitinc, final, buf));
	if (u.udaminc)
	    you_have(&menu, enlght_combatinc("damage", u.udaminc, final, buf));
	if (Slow_digestion) you_have(&menu, "slower digestion");
	if (Regeneration) enl_msg(&menu, "You regenerate", "", "d", "");
	if (u.uspellprot || Protection) {
	    int prot = 0;

	    if(uleft && uleft->otyp == RIN_PROTECTION) prot += uleft->spe;
	    if(uright && uright->otyp == RIN_PROTECTION) prot += uright->spe;
	    if (HProtection & INTRINSIC) prot += u.ublessed;
	    prot += u.uspellprot;

	    if (prot < 0)
		you_are(&menu, "ineffectively protected");
	    else
		you_are(&menu, "protected");
	}
	if (Protection_from_shape_changers)
		you_are(&menu, "protected from shape changers");
	if (Polymorph) you_are(&menu, "polymorphing");
	if (Polymorph_control) you_have(&menu, "polymorph control");
	if (u.ulycn >= LOW_PM) {
		strcpy(buf, an(mons[u.ulycn].mname));
		you_are(&menu, buf);
	}
	if (Upolyd) {
	    if (u.umonnum == u.ulycn) strcpy(buf, "in beast form");
	    else sprintf(buf, "polymorphed into %s", an(youmonst.data->mname));
	    if (wizard) sprintf(eos(buf), " (%d)", u.mtimedone);
	    you_are(&menu, buf);
	}
	if (Unchanging) you_can(&menu, "not change from your current form");
	if (Fast) you_are(&menu, Very_fast ? "very fast" : "fast");
	if (Reflecting) you_have(&menu, "reflection");
	if (Free_action) you_have(&menu, "free action");
	if (Fixed_abil) you_have(&menu, "fixed abilities");
	if (Lifesaved)
		enl_msg(&menu, "Your life ", "will be", "would have been", " saved");
	if (u.twoweap) you_are(&menu, "wielding two weapons at once");

	/*** Miscellany ***/
	if (Luck) {
	    ltmp = abs((int)Luck);
	    sprintf(buf, "%s%slucky",
		    ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
		    Luck < 0 ? "un" : "");
	    if (wizard) sprintf(eos(buf), " (%d)", Luck);
	    you_are(&menu, buf);
	}
	 else if (wizard) enl_msg(&menu, "Your luck ", "is", "was", " zero");
	if (u.moreluck > 0) you_have(&menu, "extra luck");
	else if (u.moreluck < 0) you_have(&menu, "reduced luck");
	if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
	    ltmp = stone_luck(FALSE);
	    if (ltmp <= 0)
		enl_msg(&menu, "Bad luck ", "does", "did", " not time out for you");
	    if (ltmp >= 0)
		enl_msg(&menu, "Good luck ", "does", "did", " not time out for you");
	}

	if (u.ugangr) {
	    sprintf(buf, " %sangry with you",
		    u.ugangr > 6 ? "extremely " : u.ugangr > 3 ? "very " : "");
	    if (wizard) sprintf(eos(buf), " (%d)", u.ugangr);
	    enl_msg(&menu, u_gname(), " is", " was", buf);
	} else
	    /*
	     * We need to suppress this when the game is over, because death
	     * can change the value calculated by can_pray(), potentially
	     * resulting in a false claim that you could have prayed safely.
	     */
	  if (!final) {
	    sprintf(buf, "%ssafely pray", can_pray(FALSE) ? "" : "not ");
	    if (wizard) sprintf(eos(buf), " (%d)", u.ublesscnt);
	    you_can(&menu, buf);
	}

    {
	const char *p;

	buf[0] = '\0';
	if (final < 2) {    /* still in progress, or quit/escaped/ascended */
	    p = "survived after being killed ";
	    switch (u.umortality) {
	    case 0:  p = !final ? NULL : "survived";  break;
	    case 1:  strcpy(buf, "once");  break;
	    case 2:  strcpy(buf, "twice");  break;
	    case 3:  strcpy(buf, "thrice");  break;
	    default: sprintf(buf, "%d times", u.umortality);
		     break;
	    }
	} else {		/* game ended in character's death */
	    p = "are dead";
	    switch (u.umortality) {
	    case 0:  impossible("dead without dying?");
	    case 1:  break;			/* just "are dead" */
	    default: sprintf(buf, " (%d%s time!)", u.umortality,
			     ordin(u.umortality));
		     break;
	    }
	}
	if (p) enl_msg(&menu, You_, "have been killed ", p, buf);
    }

	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return;
}

/*
 * Courtesy function for non-debug, non-explorer mode players
 * to help refresh them about who/what they are.
 * Returns FALSE if menu cancelled (dismissed with ESC), TRUE otherwise.
 */
static boolean minimal_enlightenment(void)
{
	int genidx, n, i = 0;
	char buf[BUFSZ], buf2[BUFSZ];
	static const char untabbed_fmtstr[] = "%-15s: %-12s";
	static const char untabbed_deity_fmtstr[] = "%-17s%s";
	static const char tabbed_fmtstr[] = "%s:\t%-12s";
	static const char tabbed_deity_fmtstr[] = "%s\t%s";
	static const char *fmtstr;
	static const char *deity_fmtstr;
	struct nh_menuitem items[18];

	fmtstr = iflags.menu_tab_sep ? tabbed_fmtstr : untabbed_fmtstr;
	deity_fmtstr = iflags.menu_tab_sep ?
			tabbed_deity_fmtstr : untabbed_deity_fmtstr; 

	buf[0] = buf2[0] = '\0';
	set_menuitem(&items[i++], 0, MI_HEADING, "Starting", 0, FALSE);

	/* Starting name, race, role, gender */
	sprintf(buf, fmtstr, "name", plname);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	sprintf(buf, fmtstr, "race", urace.noun);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	sprintf(buf, fmtstr, "role",
		(flags.initgend && urole.name.f) ? urole.name.f : urole.name.m);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	sprintf(buf, fmtstr, "gender", genders[flags.initgend].adj);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);

	/* Starting alignment */
	sprintf(buf, fmtstr, "alignment", align_str(u.ualignbase[A_ORIGINAL]));
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);

	/* Current name, race, role, gender */
	set_menuitem(&items[i++], 0, MI_NORMAL, "", 0, FALSE);
	set_menuitem(&items[i++], 0, MI_HEADING, "Current", 0, FALSE);
	sprintf(buf, fmtstr, "race", Upolyd ? youmonst.data->mname : urace.noun);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	if (Upolyd)
	    sprintf(buf, fmtstr, "role (base)",
		(u.mfemale && urole.name.f) ? urole.name.f : urole.name.m);
	else
	    sprintf(buf, fmtstr, "role",
		(flags.female && urole.name.f) ? urole.name.f : urole.name.m);
	
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	/* don't want poly_gender() here; it forces `2' for non-humanoids */
	genidx = is_neuter(youmonst.data) ? 2 : flags.female;
	sprintf(buf, fmtstr, "gender", genders[genidx].adj);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	if (Upolyd && (int)u.mfemale != genidx) {
	    sprintf(buf, fmtstr, "gender (base)", genders[u.mfemale].adj);
	    set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);
	}

	/* Current alignment */
	sprintf(buf, fmtstr, "alignment", align_str(u.ualign.type));
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);

	/* Deity list */
	set_menuitem(&items[i++], 0, MI_NORMAL, "", 0, FALSE);
	set_menuitem(&items[i++], 0, MI_HEADING, "Deities", 0, FALSE);
	sprintf(buf2, deity_fmtstr, align_gname(A_CHAOTIC),
	    (u.ualignbase[A_ORIGINAL] == u.ualign.type
		&& u.ualign.type == A_CHAOTIC) ? " (s,c)" :
	    (u.ualignbase[A_ORIGINAL] == A_CHAOTIC)       ? " (s)" :
	    (u.ualign.type   == A_CHAOTIC)       ? " (c)" : "");
	sprintf(buf, fmtstr, "Chaotic", buf2);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);

	sprintf(buf2, deity_fmtstr, align_gname(A_NEUTRAL),
	    (u.ualignbase[A_ORIGINAL] == u.ualign.type
		&& u.ualign.type == A_NEUTRAL) ? " (s,c)" :
	    (u.ualignbase[A_ORIGINAL] == A_NEUTRAL)       ? " (s)" :
	    (u.ualign.type   == A_NEUTRAL)       ? " (c)" : "");
	sprintf(buf, fmtstr, "Neutral", buf2);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);

	sprintf(buf2, deity_fmtstr, align_gname(A_LAWFUL),
	    (u.ualignbase[A_ORIGINAL] == u.ualign.type &&
		u.ualign.type == A_LAWFUL)  ? " (s,c)" :
	    (u.ualignbase[A_ORIGINAL] == A_LAWFUL)        ? " (s)" :
	    (u.ualign.type   == A_LAWFUL)        ? " (c)" : "");
	sprintf(buf, fmtstr, "Lawful", buf2);
	set_menuitem(&items[i++], 0, MI_NORMAL, buf, 0, FALSE);

	n = display_menu(items, i, "Base Attributes", PICK_NONE, NULL);
	return n != -1;
}

static int doattributes(void)
{
	if (!minimal_enlightenment())
		return 0;
	if (wizard || discover)
		enlightenment(0);
	return 0;
}

/* KMH, #conduct
 * (shares enlightenment's tense handling)
 */
static int doconduct(void)
{
	show_conduct(0);
	return 0;
}

void show_conduct(int final)
{
	char buf[BUFSZ];
	int ngenocided;
	struct menulist menu;

	/* Create the conduct window */
	init_menulist(&menu);
	add_menutext(&menu, "Voluntary challenges:");
	add_menutext(&menu, "");

	if (!u.uconduct.food)
	    enl_msg(&menu, You_, "have gone", "went", " without food");
	    /* But beverages are okay */
	else if (!u.uconduct.unvegan)
	    you_have_X(&menu, "followed a strict vegan diet");
	else if (!u.uconduct.unvegetarian)
	    you_have_been(&menu, "vegetarian");

	if (!u.uconduct.gnostic)
	    you_have_been(&menu, "an atheist");

	if (!u.uconduct.weaphit)
	    you_have_never(&menu, "hit with a wielded weapon");
	else if (wizard) {
	    sprintf(buf, "used a wielded weapon %ld time%s",
		    u.uconduct.weaphit, plur(u.uconduct.weaphit));
	    you_have_X(&menu, buf);
	}
	if (!u.uconduct.killer)
	    you_have_been(&menu, "a pacifist");

	if (!u.uconduct.literate)
	    you_have_been(&menu, "illiterate");
	else if (wizard) {
	    sprintf(buf, "read items or engraved %ld time%s",
		    u.uconduct.literate, plur(u.uconduct.literate));
	    you_have_X(&menu, buf);
	}

	ngenocided = num_genocides();
	if (ngenocided == 0) {
	    you_have_never(&menu, "genocided any monsters");
	} else {
	    sprintf(buf, "genocided %d type%s of monster%s",
		    ngenocided, plur(ngenocided), plur(ngenocided));
	    you_have_X(&menu, buf);
	}

	if (!u.uconduct.polypiles)
	    you_have_never(&menu, "polymorphed an object");
	else if (wizard) {
	    sprintf(buf, "polymorphed %ld item%s",
		    u.uconduct.polypiles, plur(u.uconduct.polypiles));
	    you_have_X(&menu, buf);
	}

	if (!u.uconduct.polyselfs)
	    you_have_never(&menu, "changed form");
	else if (wizard) {
	    sprintf(buf, "changed form %ld time%s",
		    u.uconduct.polyselfs, plur(u.uconduct.polyselfs));
	    you_have_X(&menu, buf);
	}

	if (!u.uconduct.wishes)
	    you_have_X(&menu, "used no wishes");
	else {
	    sprintf(buf, "used %ld wish%s",
		    u.uconduct.wishes, (u.uconduct.wishes > 1L) ? "es" : "");
	    you_have_X(&menu, buf);

	    if (!u.uconduct.wisharti)
		enl_msg(&menu, You_, "have not wished", "did not wish",
			" for any artifacts");
	}

	/* Pop up the window and wait for a key */
	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
}


#ifndef M
# define M(c)		((c) - 128)
#endif
#ifndef C
#define C(c)		(0x1f & (c))
#endif

static const struct func_tab cmdlist[] = {
	{C('d'), FALSE, dokick}, /* "D" is for door!...?  Msg is in dokick.c */
	{C('e'), TRUE, wiz_detect},
	{C('f'), TRUE, wiz_map},
	{C('g'), TRUE, wiz_genesis},
	{C('i'), TRUE, wiz_identify},
	{C('l'), TRUE, doredraw}, /* if number_pad is set */
	{C('o'), TRUE, wiz_where},
	{C('p'), TRUE, doprev_message},
	{C('r'), TRUE, doredraw},
	{C('t'), TRUE, dotele},
	{C('v'), TRUE, wiz_level_tele},
	{C('w'), TRUE, wiz_wish},
	{C('x'), TRUE, doattributes},
	{'a', FALSE, doapply},
	{'A', FALSE, doddoremarm},
	{M('a'), TRUE, doorganize},
/*	'b', 'B' : go sw */
	{'c', FALSE, doclose},
	{'C', TRUE, do_mname},
	{M('c'), TRUE, dotalk},
	{'d', FALSE, dodrop},
	{'D', FALSE, doddrop},
	{M('d'), FALSE, dodip},
	{'e', FALSE, doeat},
	{'E', FALSE, doengrave},
	{M('e'), TRUE, enhance_weapon_skill},
	{'f', FALSE, dofire},
/*	'F' : fight (one time) */
	{M('f'), FALSE, doforce},
/*	'g', 'G' : multiple go */
/*	'h', 'H' : go west */
	{'h', TRUE, dohelp}, /* if number_pad is set */
	{'i', TRUE, ddoinv},
	{'I', TRUE, dotypeinv},		/* Robert Viduya */
	{M('i'), TRUE, doinvoke},
/*	'j', 'J', 'k', 'K', 'l', 'L', 'm', 'M', 'n', 'N' : move commands */
	{'j', FALSE, dojump}, /* if number_pad is on */
	{M('j'), FALSE, dojump},
	{'k', FALSE, dokick}, /* if number_pad is on */
	{'l', FALSE, doloot}, /* if number_pad is on */
	{M('l'), FALSE, doloot},
/*	'n' prefixes a count if number_pad is on */
	{M('m'), TRUE, domonability},
	{'N', TRUE, ddocall}, /* if number_pad is on */
	{M('n'), TRUE, ddocall},
	{M('N'), TRUE, ddocall},
	{'o', FALSE, doopen},
	{M('o'), FALSE, dosacrifice},
	{'p', FALSE, dopay},
	{'P', FALSE, doputon},
	{M('p'), TRUE, dopray},
	{'q', FALSE, dodrink},
	{'Q', FALSE, dowieldquiver},
	{M('q'), TRUE, done2},
	{'r', FALSE, doread},
	{'R', FALSE, doremring},
	{M('r'), FALSE, dorub},
	{'s', TRUE, dosearch, "searching"},
	{'S', TRUE, dosave},
	{M('s'), FALSE, dosit},
	{'t', FALSE, dothrow},
	{'T', FALSE, dotakeoff},
	{M('t'), TRUE, doturn},
/*	'u', 'U' : go ne */
	{'u', FALSE, dountrap}, /* if number_pad is on */
	{M('u'), FALSE, dountrap},
	{'v', TRUE, doversion},
	{'V', TRUE, dohistory},
	{M('v'), TRUE, doextversion},
	{'w', FALSE, dowield},
	{'W', FALSE, dowear},
	{M('w'), FALSE, dowipe},
	{'x', FALSE, doswapweapon},
	{'X', TRUE, enter_explore_mode},
/*	'y', 'Y' : go nw */
	{'z', FALSE, dozap},
	{'Z', TRUE, docast},
	{'<', FALSE, doup},
	{'>', FALSE, dodown},
	{'/', TRUE, dowhatis},
	{'&', TRUE, dowhatdoes},
	{'?', TRUE, dohelp},
	{M('?'), TRUE, doextlist},
	{'.', TRUE, donull, "waiting"},
	{' ', TRUE, donull, "waiting"},
	{',', FALSE, dopickup},
	{':', TRUE, dolook},
	{';', TRUE, doquickwhatis},
	{'^', TRUE, doidtrap},
	{'\\', TRUE, dodiscovered},		/* Robert Viduya */
	{'@', TRUE, dotogglepickup},
	{M('2'), FALSE, dotwoweapon},
	{WEAPON_SYM,  TRUE, doprwep},
	{ARMOR_SYM,  TRUE, doprarm},
	{RING_SYM,  TRUE, doprring},
	{AMULET_SYM, TRUE, dopramulet},
	{TOOL_SYM, TRUE, doprtool},
	{'*', TRUE, doprinuse},	/* inventory of all equipment in use */
	{GOLD_SYM, TRUE, doprgold},
	{SPBOOK_SYM, TRUE, dovspell},			/* Mike Stephenson */
	{'#', TRUE, doextcmd},
	{'_', TRUE, dotravel},
	{0,0,0,0}
};

struct ext_func_tab extcmdlist[] = {
	{"adjust", "adjust inventory letters", doorganize, TRUE},
	{"chat", "talk to someone", dotalk, TRUE},	/* converse? */
	{"conduct", "list which challenges you have adhered to", doconduct, TRUE},
	{"dip", "dip an object into something", dodip, FALSE},
	{"enhance", "advance or check weapons skills", enhance_weapon_skill,
							TRUE},
	{"force", "force a lock", doforce, FALSE},
	{"invoke", "invoke an object's powers", doinvoke, TRUE},
	{"jump", "jump to a location", dojump, FALSE},
	{"loot", "loot a box on the floor", doloot, FALSE},
	{"monster", "use a monster's special ability", domonability, TRUE},
	{"name", "name an item or type of object", ddocall, TRUE},
	{"offer", "offer a sacrifice to the gods", dosacrifice, FALSE},
	{"pray", "pray to the gods for help", dopray, TRUE},
	{"quit", "exit without saving current game", done2, TRUE},
	{"ride", "ride (or stop riding) a monster", doride, FALSE},
	{"rub", "rub a lamp or a stone", dorub, FALSE},
	{"sit", "sit down", dosit, FALSE},
	{"turn", "turn undead", doturn, TRUE},
	{"twoweapon", "toggle two-weapon combat", dotwoweapon, FALSE},
	{"untrap", "untrap something", dountrap, FALSE},
	{"version", "list compile time options for this version of NetHack",
		doextversion, TRUE},
	{"wipe", "wipe off your face", dowipe, FALSE},
	{"?", "get this list of extended commands", doextlist, TRUE},

	/*
	 * There must be a blank entry here for every entry in the table
	 * below.
	 */
	{NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE},
#ifdef DEBUG_MIGRATING_MONS
	{NULL, NULL, donull, TRUE},
#endif
	{NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE},
        {NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE},
#ifdef DEBUG
	{NULL, NULL, donull, TRUE},
#endif
	{NULL, NULL, donull, TRUE},
	{NULL, NULL, donull, TRUE}	/* sentinel */
};


static const struct ext_func_tab debug_extcmdlist[] = {
	{"levelchange", "change experience level", wiz_level_change, TRUE},
	{"lightsources", "show mobile light sources", wiz_light_sources, TRUE},
#ifdef DEBUG_MIGRATING_MONS
	{"migratemons", "migrate n random monsters", wiz_migrate_mons, TRUE},
#endif
	{"monpolycontrol", "control monster polymorphs", wiz_mon_polycontrol, TRUE},
	{"panic", "test panic routine (fatal to game)", wiz_panic, TRUE},
	{"polyself", "polymorph self", wiz_polyself, TRUE},
	{"seenv", "show seen vectors", wiz_show_seenv, TRUE},
	{"stats", "show memory statistics", wiz_show_stats, TRUE},
	{"timeout", "look at timeout queue", wiz_timeout_queue, TRUE},
	{"vision", "show vision array", wiz_show_vision, TRUE},
#ifdef DEBUG
	{"wizdebug", "wizard debug command", wiz_debug_cmd, TRUE},
#endif
	{"wmode", "show wall modes", wiz_show_wmodes, TRUE},
	{NULL, NULL, donull, TRUE}
};

/*
 * Insert debug commands into the extended command list.  This function
 * assumes that the last entry will be the help entry.
 *
 * You must add entries in ext_func_tab every time you add one to the
 * debug_extcmdlist().
 */
void add_debug_extended_commands(void)
{
	int i, j, k, n;

	/* count the # of help entries */
	for (n = 0; extcmdlist[n].ef_txt[0] != '?'; n++)
	    ;

	for (i = 0; debug_extcmdlist[i].ef_txt; i++) {
	    for (j = 0; j < n; j++)
		if (strcmp(debug_extcmdlist[i].ef_txt, extcmdlist[j].ef_txt) < 0) break;

	    /* insert i'th debug entry into extcmdlist[j], pushing down  */
	    for (k = n; k >= j; --k)
		extcmdlist[k+1] = extcmdlist[k];
	    extcmdlist[j] = debug_extcmdlist[i];
	    n++;	/* now an extra entry */
	}
}


static const char template[] = "%-18s %4ld  %6ld";
static const char count_str[] = "                   count  bytes";
static const char separator[] = "------------------ -----  ------";

static void count_obj(struct obj *chain, long *total_count, long *total_size,
		      boolean top, boolean recurse)
{
	long count, size;
	struct obj *obj;

	for (count = size = 0, obj = chain; obj; obj = obj->nobj) {
	    if (top) {
		count++;
		size += sizeof(struct obj) + obj->oxlth + obj->onamelth;
	    }
	    if (recurse && obj->cobj)
		count_obj(obj->cobj, total_count, total_size, TRUE, TRUE);
	}
	*total_count += count;
	*total_size += size;
}

static void obj_chain(struct menulist *menu, const char *src, struct obj *chain,
		      long *total_count, long *total_size)
{
	char buf[BUFSZ];
	long count = 0, size = 0;

	count_obj(chain, &count, &size, TRUE, FALSE);
	*total_count += count;
	*total_size += size;
	sprintf(buf, template, src, count, size);
	add_menutext(menu, buf);
}

static void mon_invent_chain(struct menulist *menu, const char *src, struct monst *chain,
			     long *total_count, long *total_size)
{
	char buf[BUFSZ];
	long count = 0, size = 0;
	struct monst *mon;

	for (mon = chain; mon; mon = mon->nmon)
	    count_obj(mon->minvent, &count, &size, TRUE, FALSE);
	*total_count += count;
	*total_size += size;
	sprintf(buf, template, src, count, size);
	add_menutext(menu, buf);
}

static void contained(struct menulist *menu, const char *src, long *total_count, long *total_size)
{
	char buf[BUFSZ];
	long count = 0, size = 0;
	struct monst *mon;

	count_obj(invent, &count, &size, FALSE, TRUE);
	count_obj(fobj, &count, &size, FALSE, TRUE);
	count_obj(level.buriedobjlist, &count, &size, FALSE, TRUE);
	count_obj(migrating_objs, &count, &size, FALSE, TRUE);
	/* DEADMONSTER check not required in this loop since they have no inventory */
	for (mon = fmon; mon; mon = mon->nmon)
	    count_obj(mon->minvent, &count, &size, FALSE, TRUE);
	for (mon = migrating_mons; mon; mon = mon->nmon)
	    count_obj(mon->minvent, &count, &size, FALSE, TRUE);

	*total_count += count; *total_size += size;

	sprintf(buf, template, src, count, size);
	add_menutext(menu, buf);
}

static void mon_chain(struct menulist *menu, const char *src, struct monst *chain,
		      long *total_count, long *total_size)
{
	char buf[BUFSZ];
	long count, size;
	struct monst *mon;

	for (count = size = 0, mon = chain; mon; mon = mon->nmon) {
	    count++;
	    size += sizeof(struct monst) + mon->mxlth + mon->mnamelth;
	}
	*total_count += count;
	*total_size += size;
	sprintf(buf, template, src, count, size);
	add_menutext(menu, buf);
}

/*
 * Display memory usage of all monsters and objects on the level.
 */
static int wiz_show_stats(void)
{
	char buf[BUFSZ];
	struct menulist menu;
	long total_obj_size = 0, total_obj_count = 0;
	long total_mon_size = 0, total_mon_count = 0;

	init_menulist(&menu);
	add_menutext(&menu, "Current memory statistics:");
	add_menutext(&menu, "");
	sprintf(buf, "Objects, size %d", (int) sizeof(struct obj));
	add_menutext(&menu, buf);
	add_menutext(&menu, "");
	add_menutext(&menu, count_str);

	obj_chain(&menu, "invent", invent, &total_obj_count, &total_obj_size);
	obj_chain(&menu, "fobj", fobj, &total_obj_count, &total_obj_size);
	obj_chain(&menu, "buried", level.buriedobjlist,
				&total_obj_count, &total_obj_size);
	obj_chain(&menu, "migrating obj", migrating_objs,
				&total_obj_count, &total_obj_size);
	mon_invent_chain(&menu, "minvent", fmon,
				&total_obj_count,&total_obj_size);
	mon_invent_chain(&menu, "migrating minvent", migrating_mons,
				&total_obj_count, &total_obj_size);

	contained(&menu, "contained",
				&total_obj_count, &total_obj_size);

	add_menutext(&menu, separator);
	sprintf(buf, template, "Total", total_obj_count, total_obj_size);
	add_menutext(&menu, buf);

	add_menutext(&menu, "");
	add_menutext(&menu, "");
	sprintf(buf, "Monsters, size %d", (int) sizeof(struct monst));
	add_menutext(&menu, buf);
	add_menutext(&menu, "");

	mon_chain(&menu, "fmon", fmon,
				&total_mon_count, &total_mon_size);
	mon_chain(&menu, "migrating", migrating_mons,
				&total_mon_count, &total_mon_size);

	add_menutext(&menu, separator);
	sprintf(buf, template, "Total", total_mon_count, total_mon_size);
	add_menutext(&menu, buf);

	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return 0;
}

void sanity_check(void)
{
	obj_sanity_check();
	timer_sanity_check();
}

#ifdef DEBUG_MIGRATING_MONS
static int wiz_migrate_mons(void)
{
	int mcount = 0;
	char inbuf[BUFSZ];
	struct permonst *ptr;
	struct monst *mtmp;
	d_level tolevel;
	getlin("How many random monsters to migrate? [0]", inbuf);
	if (*inbuf == '\033') return 0;
	mcount = atoi(inbuf);
	if (mcount < 0 || mcount > (COLNO * ROWNO) || Is_botlevel(&u.uz))
		return 0;
	while (mcount > 0) {
		if (Is_stronghold(&u.uz))
		    assign_level(&tolevel, &valley_level);
		else
		    get_level(&tolevel, depth(&u.uz) + 1);
		ptr = rndmonst();
		mtmp = makemon(ptr, 0, 0, NO_MM_FLAGS);
		if (mtmp) migrate_to_level(mtmp, ledger_no(&tolevel),
				MIGR_RANDOM, NULL);
		mcount--;
	}
	return 0;
}
#endif


#define unctrl(c)	((c) <= C('z') ? (0x60 | (c)) : (c))
#define unmeta(c)	(0x7f & (c))


void rhack(char *cmd)
{
	boolean do_walk, do_rush, prefix_seen, bad_command,
		firsttime = (cmd == 0);

	iflags.menu_requested = FALSE;
	if (cmd == NULL) {
	    /* shouldn't happen */
	    impossible("cmd is NULL in rhack");
	    return;
	}
	
	if (*cmd == '\033') {
		flags.move = FALSE;
		return;
	}
	
	if(!*cmd || *cmd == (char)0377 || (!flags.rest_on_space && *cmd == ' '))
	{
		nhbell();
		flags.move = FALSE;
		return;		/* probably we just had an interrupt */
	}
	/* handle most movement commands */
	do_walk = do_rush = prefix_seen = FALSE;
	flags.travel = iflags.travel1 = 0;
	switch (*cmd) {
	 case 'g':  if (movecmd(cmd[1])) {
			flags.run = 2;
			do_rush = TRUE;
		    } else
			prefix_seen = TRUE;
		    break;
	 case '5':  if (!iflags2.num_pad) break;	/* else FALLTHRU */
	 case 'G':  if (movecmd(lowc(cmd[1]))) {
			flags.run = 3;
			do_rush = TRUE;
		    } else
			prefix_seen = TRUE;
		    break;
	 case '-':  if (!iflags2.num_pad) break;	/* else FALLTHRU */
	/* Effects of movement commands and invisible monsters:
	 * m: always move onto space (even if 'I' remembered)
	 * F: always attack space (even if 'I' not remembered)
	 * normal movement: attack if 'I', move otherwise
	 */
	 case 'F':  if (movecmd(cmd[1])) {
			flags.forcefight = 1;
			do_walk = TRUE;
		    } else
			prefix_seen = TRUE;
		    break;
	 case 'm':  if (movecmd(cmd[1]) || u.dz) {
			flags.run = 0;
			flags.nopick = 1;
			if (!u.dz) do_walk = TRUE;
			else cmd[0] = cmd[1];	/* "m<" or "m>" */
		    } else
			prefix_seen = TRUE;
		    break;
	 case 'M':  if (movecmd(lowc(cmd[1]))) {
			flags.run = 1;
			flags.nopick = 1;
			do_rush = TRUE;
		    } else
			prefix_seen = TRUE;
		    break;
	 case '0':  if (!iflags2.num_pad) break;
		    ddoinv(); /* a convenience borrowed from the PC */
		    flags.move = FALSE;
		    multi = 0;
		    return;
	 case CMD_TRAVEL:
		    if (iflags.travelcmd) {
			    flags.travel = 1;
			    iflags.travel1 = 1;
			    flags.run = 8;
			    flags.nopick = 1;
			    do_rush = TRUE;
			    break;
		    }
		    /*FALLTHRU*/
	 default:   if (movecmd(*cmd)) {	/* ordinary movement */
			flags.run = 0;	/* only matters here if it was 8 */
			do_walk = TRUE;
		    } else if (movecmd(iflags2.num_pad ?
				       unmeta(*cmd) : lowc(*cmd))) {
			flags.run = 1;
			do_rush = TRUE;
		    } else if (movecmd(unctrl(*cmd))) {
			flags.run = 3;
			do_rush = TRUE;
		    }
		    break;
	}

	/* some special prefix handling */
	/* overload 'm' prefix for ',' to mean "request a menu" */
	if (prefix_seen && cmd[1] == ',') {
		iflags.menu_requested = TRUE;
		++cmd;
	}

	if (do_walk) {
	    if (multi) flags.mv = TRUE;
	    domove();
	    flags.forcefight = 0;
	    return;
	} else if (do_rush) {
	    if (firsttime) {
		if (!multi) multi = max(COLNO,ROWNO);
		u.last_str_turn = 0;
	    }
	    flags.mv = TRUE;
	    domove();
	    return;
	} else if (prefix_seen && cmd[1] == '\033') {	/* <prefix><escape> */
	    /* don't report "unknown command" for change of heart... */
	    bad_command = FALSE;
	} else if (*cmd == ' ' && !flags.rest_on_space) {
	    bad_command = TRUE;		/* skip cmdlist[] loop */

	/* handle all other commands */
	} else {
	    const struct func_tab *tlist;
	    int res, (*func)(void);

	    for (tlist = cmdlist; tlist->f_char; tlist++) {
		if ((*cmd & 0xff) != (tlist->f_char & 0xff)) continue;

		if (u.uburied && !tlist->can_if_buried) {
		    You_cant("do that while you are buried!");
		    res = 0;
		} else {
		    /* we discard 'const' because some compilers seem to have
		       trouble with the pointer passed to set_occupation() */
		    func = ((struct func_tab *)tlist)->f_funct;
		    if (tlist->f_text && !occupation && multi)
			set_occupation(func, tlist->f_text, multi);
		    res = (*func)();		/* perform the command */
		}
		if (!res) {
		    flags.move = FALSE;
		    multi = 0;
		}
		return;
	    }
	    /* if we reach here, cmd wasn't found in cmdlist[] */
	    bad_command = TRUE;
	}

	if (bad_command) {
	    char expcmd[10];
	    char *cp = expcmd;

	    while (*cmd && (int)(cp - expcmd) < (int)(sizeof expcmd - 3)) {
		if (*cmd >= 040 && *cmd < 0177) {
		    *cp++ = *cmd++;
		} else if (*cmd & 0200) {
		    *cp++ = 'M';
		    *cp++ = '-';
		    *cp++ = *cmd++ &= ~0200;
		} else {
		    *cp++ = '^';
		    *cp++ = *cmd++ ^ 0100;
		}
	    }
	    *cp = '\0';
	    if (!prefix_seen || !iflags.cmdassist ||
		!help_dir(0, "Invalid direction key!"))
		Norep("Unknown command '%s'.", expcmd);
	}
	/* didn't move */
	flags.move = FALSE;
	multi = 0;
	return;
}

int xytod(schar x, schar y)	/* convert an x,y pair into a direction code */
{
	int dd;

	for(dd = 0; dd < 8; dd++)
	    if(x == xdir[dd] && y == ydir[dd]) return dd;

	return -1;
}

void dtoxy(coord *cc, int dd)	/* convert a direction code into an x,y pair */
{
	cc->x = xdir[dd];
	cc->y = ydir[dd];
	return;
}

int movecmd(char sym)	/* also sets u.dz, but returns false for <> */
{
	const char *dp;
	const char *sdp;
	if(iflags2.num_pad) sdp = ndir; else sdp = sdir;	/* DICE workaround */

	u.dz = 0;
	if(!(dp = index(sdp, sym))) return 0;
	u.dx = xdir[dp-sdp];
	u.dy = ydir[dp-sdp];
	u.dz = zdir[dp-sdp];
	if (u.dx && u.dy && u.umonnum == PM_GRID_BUG) {
		u.dx = u.dy = 0;
		return 0;
	}
	return !u.dz;
}

/*
 * uses getdir() but unlike getdir() it specifically
 * produces coordinates using the direction from getdir()
 * and verifies that those coordinates are ok.
 *
 * If the call to getdir() returns 0, Never_mind is displayed.
 * If the resulting coordinates are not okay, emsg is displayed.
 *
 * Returns non-zero if coordinates in cc are valid.
 */
int get_adjacent_loc(const char *prompt, const char *emsg, xchar x, xchar y, coord *cc)
{
	xchar new_x, new_y;
	if (!getdir(prompt)) {
		pline(Never_mind);
		return 0;
	}
	new_x = x + u.dx;
	new_y = y + u.dy;
	if (cc && isok(new_x,new_y)) {
		cc->x = new_x;
		cc->y = new_y;
	} else {
		if (emsg) pline(emsg);
		return 0;
	}
	return 1;
}

int getdir(const char *s)
{
	char dirsym;

	dirsym = yn_function ((s && *s != '^') ? s : "In what direction?",
					NULL, '\0');
	if(dirsym == '.' || dirsym == 's')
		u.dx = u.dy = u.dz = 0;
	else if(!movecmd(dirsym) && !u.dz) {
		boolean did_help = FALSE;
		if(!index(quitchars, dirsym)) {
		    if (iflags.cmdassist) {
			did_help = help_dir((s && *s == '^') ? dirsym : 0,
					    "Invalid direction key!");
		    }
		    if (!did_help) pline("What a strange direction!");
		}
		return 0;
	}
	if(!u.dz && (Stunned || (Confusion && !rn2(5)))) confdir();
	return 1;
}

static boolean help_dir(char sym, const char *msg)
{
	char ctrl;
	struct menulist menu;
	static const char wiz_only_list[] = "EFGIOVW";
	char buf[BUFSZ], buf2[BUFSZ], *expl;

	init_menulist(&menu);

	if (msg) {
		sprintf(buf, "cmdassist: %s", msg);
		add_menutext(&menu, buf);
		add_menutext(&menu, "");
	}
	if (letter(sym)) { 
	    sym = highc(sym);
	    ctrl = (sym - 'A') + 1;
	    if ((expl = dowhatdoes_core(ctrl, buf2))
		&& (!index(wiz_only_list, sym) || wizard)) {
		sprintf(buf, "Are you trying to use ^%c%s?", sym,
			index(wiz_only_list, sym) ? "" :
			" as specified in the Guidebook");
		add_menutext(&menu, buf);
		add_menutext(&menu, "");
		add_menutext(&menu, expl);
		add_menutext(&menu, "");
		add_menutext(&menu, "To use that command, you press");
		sprintf(buf,
			"the <Ctrl> key, and the <%c> key at the same time.", sym);
		add_menutext(&menu, buf);
		add_menutext(&menu, "");
	    }
	}
	if (iflags2.num_pad && u.umonnum == PM_GRID_BUG) {
	    add_menutext(&menu, "Valid direction keys in your current form (with number_pad on) are:");
	    add_menutext(&menu, "             8   ");
	    add_menutext(&menu, "             |   ");
	    add_menutext(&menu, "          4- . -6");
	    add_menutext(&menu, "             |   ");
	    add_menutext(&menu, "             2   ");
	} else if (u.umonnum == PM_GRID_BUG) {
	    add_menutext(&menu, "Valid direction keys in your current form are:");
	    add_menutext(&menu, "             k   ");
	    add_menutext(&menu, "             |   ");
	    add_menutext(&menu, "          h- . -l");
	    add_menutext(&menu, "             |   ");
	    add_menutext(&menu, "             j   ");
	} else if (iflags2.num_pad) {
	    add_menutext(&menu, "Valid direction keys (with number_pad on) are:");
	    add_menutext(&menu, "          7  8  9");
	    add_menutext(&menu, "           \\ | / ");
	    add_menutext(&menu, "          4- . -6");
	    add_menutext(&menu, "           / | \\ ");
	    add_menutext(&menu, "          1  2  3");
	} else {
	    add_menutext(&menu, "Valid direction keys are:");
	    add_menutext(&menu, "          y  k  u");
	    add_menutext(&menu, "           \\ | / ");
	    add_menutext(&menu, "          h- . -l");
	    add_menutext(&menu, "           / | \\ ");
	    add_menutext(&menu, "          b  j  n");
	};
	add_menutext(&menu, "");
	add_menutext(&menu, "          <  up");
	add_menutext(&menu, "          >  down");
	add_menutext(&menu, "          .  direct at yourself");
	add_menutext(&menu, "");
	add_menutext(&menu, "(Suppress this message with !cmdassist in config file.)");
	display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
	free(menu.items);
	return TRUE;
}


void confdir(void)
{
	int x = (u.umonnum == PM_GRID_BUG) ? 2*rn2(4) : rn2(8);
	u.dx = xdir[x];
	u.dy = ydir[x];
	return;
}


int isok(int x, int y)
{
	/* x corresponds to curx, so x==1 is the first column. Ach. %% */
	return x >= 1 && x <= COLNO-1 && y >= 0 && y <= ROWNO-1;
}

static int last_multi;

/*
 * convert a MAP window position into a movecmd
 */
const char *click_to_cmd(int x, int y, int mod)
{
    int dir;
    static char cmd[4];
    cmd[1]=0;

    x -= u.ux;
    y -= u.uy;

    if (iflags.travelcmd) {
        if (abs(x) <= 1 && abs(y) <= 1 ) {
            x = sgn(x), y = sgn(y);
        } else {
            u.tx = u.ux+x;
            u.ty = u.uy+y;
            cmd[0] = CMD_TRAVEL;
            return cmd;
        }

        if(x == 0 && y == 0) {
            /* here */
            if(IS_FOUNTAIN(levl[u.ux][u.uy].typ) || IS_SINK(levl[u.ux][u.uy].typ)) {
                cmd[0]=mod == CLICK_1 ? 'q' : M('d');
                return cmd;
            } else if(IS_THRONE(levl[u.ux][u.uy].typ)) {
                cmd[0]=M('s');
                return cmd;
            } else if((u.ux == xupstair && u.uy == yupstair)
                      || (u.ux == sstairs.sx && u.uy == sstairs.sy && sstairs.up)
                      || (u.ux == xupladder && u.uy == yupladder)) {
                return "<";
            } else if((u.ux == xdnstair && u.uy == ydnstair)
                      || (u.ux == sstairs.sx && u.uy == sstairs.sy && !sstairs.up)
                      || (u.ux == xdnladder && u.uy == ydnladder)) {
                return ">";
            } else if(OBJ_AT(u.ux, u.uy)) {
                cmd[0] = Is_container(level.objects[u.ux][u.uy]) ? M('l') : ',';
                return cmd;
            } else {
                return "."; /* just rest */
            }
        }

        /* directional commands */

        dir = xytod(x, y);

	if (!m_at(u.ux+x, u.uy+y) && !test_move(u.ux, u.uy, x, y, TEST_MOVE)) {
            cmd[1] = (iflags2.num_pad ? ndir[dir] : sdir[dir]);
            cmd[2] = 0;
            if (IS_DOOR(levl[u.ux+x][u.uy+y].typ)) {
                /* slight assistance to the player: choose kick/open for them */
                if (levl[u.ux+x][u.uy+y].doormask & D_LOCKED) {
                    cmd[0] = C('d');
                    return cmd;
                }
                if (levl[u.ux+x][u.uy+y].doormask & D_CLOSED) {
                    cmd[0] = 'o';
                    return cmd;
                }
            }
            if (levl[u.ux+x][u.uy+y].typ <= SCORR) {
                cmd[0] = 's';
                cmd[1] = 0;
                return cmd;
            }
        }
    } else {
        /* convert without using floating point, allowing sloppy clicking */
        if(x > 2*abs(y))
            x = 1, y = 0;
        else if(y > 2*abs(x))
            x = 0, y = 1;
        else if(x < -2*abs(y))
            x = -1, y = 0;
        else if(y < -2*abs(x))
            x = 0, y = -1;
        else
            x = sgn(x), y = sgn(y);

        if(x == 0 && y == 0)	/* map click on player to "rest" command */
            return ".";

        dir = xytod(x, y);
    }

    /* move, attack, etc. */
    cmd[1] = 0;
    if(mod == CLICK_1) {
	cmd[0] = (iflags2.num_pad ? ndir[dir] : sdir[dir]);
    } else {
	cmd[0] = (iflags2.num_pad ? M(ndir[dir]) :
		(sdir[dir] - 'a' + 'A')); /* run command */
    }

    return cmd;
}

char *parse(void)
{
	static char in_line[COLNO];
	int foo;
	boolean prezero = FALSE;

	multi = 0;
	flags.move = 1;
	flush_screen(1); /* Flush screen buffer. Put the cursor on the hero. */

	if (!iflags2.num_pad || (foo = readchar()) == 'n')
	    for (;;) {
		foo = readchar();
		if (foo >= '0' && foo <= '9') {
		    multi = 10 * multi + foo - '0';
		    if (multi < 0 || multi >= LARGEST_INT) multi = LARGEST_INT;
		    if (multi > 9) {
			clear_nhwindow(NHW_MESSAGE);
			sprintf(in_line, "Count: %d", multi);
			pline(in_line);
			mark_synch();
		    }
		    last_multi = multi;
		    if (!multi && foo == '0') prezero = TRUE;
		} else break;	/* not a digit */
	    }

	if (foo == '\033') {   /* esc cancels count (TH) */
	    clear_nhwindow(NHW_MESSAGE);
	    multi = last_multi = 0;
	}

	if (multi) {
	    multi--;
	    save_cm = in_line;
	} else {
	    save_cm = NULL;
	}
	in_line[0] = foo;
	in_line[1] = '\0';
	if (foo == 'g' || foo == 'G' || foo == 'm' || foo == 'M' ||
	    foo == 'F' || (iflags2.num_pad && (foo == '5' || foo == '-'))) {
	    foo = readchar();
	    in_line[1] = foo;
	    in_line[2] = 0;
	}
	clear_nhwindow(NHW_MESSAGE);
	if (prezero) in_line[0] = '\033';
	return in_line;
}


#ifdef UNIX
static void end_of_input(void)
{
#ifndef NOSAVEONHANGUP
	if (!program_state.done_hup++ && program_state.something_worth_saving)
	    dosave0();
#endif
	exit_nhwindows(NULL);
	clearlocks();
	terminate(EXIT_SUCCESS);
}
#endif


char readchar(void)
{
	int sym;
	int x = u.ux, y = u.uy, mod = 0;

	if ( *readchar_queue )
	    sym = *readchar_queue++;
	else
	    sym = nhgetch();

#ifdef UNIX
# ifdef NR_OF_EOFS
	if (sym == EOF) {
	    int cnt = NR_OF_EOFS;
	  /*
	   * Some SYSV systems seem to return EOFs for various reasons
	   * (?like when one hits break or for interrupted systemcalls?),
	   * and we must see several before we quit.
	   */
	    do {
		clearerr(stdin);	/* omit if clearerr is undefined */
		sym = nhgetch();
	    } while (--cnt && sym == EOF);
	}
# endif /* NR_OF_EOFS */
	if (sym == EOF)
	    end_of_input();
#endif /* UNIX */

	if(sym == 0) {
	    /* click event */
	    readchar_queue = click_to_cmd(x, y, mod);
	    sym = *readchar_queue++;
	}
	return (char) sym;
}

static int dotravel(void)
{
	/* Keyboard travel command */
	static char cmd[2];
	coord cc;

	if (!iflags.travelcmd) return 0;
	cmd[1]=0;
	cc.x = iflags.travelcc.x;
	cc.y = iflags.travelcc.y;
	if (cc.x == -1 && cc.y == -1) {
	    /* No cached destination, start attempt from current position */
	    cc.x = u.ux;
	    cc.y = u.uy;
	}
	pline("Where do you want to travel to?");
	if (getpos(&cc, TRUE, "the desired destination") < 0) {
		/* user pressed ESC */
		return 0;
	}
	iflags.travelcc.x = u.tx = cc.x;
	iflags.travelcc.y = u.ty = cc.y;
	cmd[0] = CMD_TRAVEL;
	readchar_queue = cmd;
	return 0;
}


/*
 *   Parameter validator for generic yes/no function to prevent
 *   the core from sending too long a prompt string to the
 *   window port causing a buffer overflow there.
 */
char yn_function(const char *query,const char *resp, char def)
{
	char qbuf[QBUFSZ];
	unsigned truncspot, reduction = sizeof(" [N]  ?") + 1;

	if (resp)
	    reduction += strlen(resp) + sizeof(" () ");
	if (strlen(query) < (QBUFSZ - reduction))
		return (*windowprocs.win_yn_function)(query, resp, def, &yn_number);
	paniclog("Query truncated: ", query);
	reduction += sizeof("...");
	truncspot = QBUFSZ - reduction;
	strncpy(qbuf, query, (int)truncspot);
	qbuf[truncspot] = '\0';
	strcat(qbuf,"...");
	return (*windowprocs.win_yn_function)(qbuf, resp, def, &yn_number);
}

/*cmd.c*/
