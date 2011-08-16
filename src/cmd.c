/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
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

static int timed_occupation(void);
static int domonability(void);
static int dotravel(int x,int y);
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


#ifndef M
# define M(c)		((c) - 128)
#endif
#ifndef C
#define C(c)		(0x1f & (c))
#endif

const struct cmd_desc cmdlist[] = {
	/* "str", "", defkey, altkey, wiz, buried, func, arg*/
	{"kick", "", C('d'), 'k', FALSE, dokick, CMD_ARG_NONE}, /* "D" is for door!...?  Msg is in dokick.c */
	{"wiz detect", "", C('e'), 0, TRUE, wiz_detect, CMD_ARG_NONE | CMD_DEBUG},
	{"wiz map", "", C('f'), 0, TRUE, wiz_map, CMD_ARG_NONE | CMD_DEBUG},
	{"wiz create monster", "", C('g'), 0, TRUE, wiz_genesis, CMD_ARG_NONE | CMD_DEBUG},
	{"wiz itentify", "", C('i'), 0, TRUE, wiz_identify, CMD_ARG_NONE | CMD_DEBUG},
	{"wiz print dungeon", "", C('o'), 0, TRUE, wiz_where, CMD_ARG_NONE | CMD_DEBUG},
	{"redraw", "", C('r'), C('l'), TRUE, doredraw, CMD_ARG_NONE},
	{"teleport", "", C('t'), 0, TRUE, dotele, CMD_ARG_NONE},
	{"wiz level teleport", "", C('v'), 0, TRUE, wiz_level_tele, CMD_ARG_NONE | CMD_DEBUG},
	{"wiz wish", "", C('w'), 0, TRUE, wiz_wish, CMD_ARG_NONE | CMD_DEBUG},
	{"attributes", "", C('x'), 0, TRUE, doattributes, CMD_ARG_NONE},
	{"apply", "", 'a', 0, FALSE, doapply, CMD_ARG_NONE},
	{"removearm", "", 'A', 0, FALSE, doddoremarm, CMD_ARG_NONE},
	{"organize", "", M('a'), 0, TRUE, doorganize, CMD_ARG_NONE},
	{"close", "", 'c', 0, FALSE, doclose, CMD_ARG_NONE},
	{"name mon", "", 'C', 0, TRUE, do_mname, CMD_ARG_NONE},
	{"talk", "", M('c'), 0, TRUE, dotalk, CMD_ARG_NONE},
	{"drop", "", 'd', 0, FALSE, dodrop, CMD_ARG_NONE},
	{"menudrop", "", 'D', 0, FALSE, doddrop, CMD_ARG_NONE},
	{"dip", "", M('d'), 0, FALSE, dodip, CMD_ARG_NONE},
	{"eat", "", 'e', 0, FALSE, doeat, CMD_ARG_NONE},
	{"engrave", "", 'E', 0, FALSE, doengrave, CMD_ARG_NONE},
	{"enhance", "", M('e'), 0, TRUE, enhance_weapon_skill, CMD_ARG_NONE},
	{"fire", "", 'f', 0, FALSE, dofire, CMD_ARG_NONE},
	{"fight", "", 'F', 0, FALSE, dofight, CMD_ARG_DIR},
	{"force", "", M('f'), 0, FALSE, doforce, CMD_ARG_NONE},
	
	{"move", "", 0, 0, FALSE, domovecmd, CMD_ARG_DIR | CMD_MOVE},
	{"move nopickup", "", 'm', 0, FALSE, domovecmd_nopickup, CMD_ARG_DIR | CMD_MOVE},
	{"run", "", 0, 0, FALSE, dorun, CMD_ARG_DIR | CMD_MOVE},
	{"run nopickup", "", 'M', 0, FALSE, dorun_nopickup, CMD_ARG_DIR | CMD_MOVE},
	{"go", "", 'g', 0, FALSE, dogo, CMD_ARG_DIR | CMD_MOVE},
	{"go2", "", 'G', 0, FALSE, dogo2, CMD_ARG_DIR | CMD_MOVE},
	
	{"help", "", '?', 'h', TRUE, dohelp, CMD_ARG_NONE},
	{"inventory", "", 'i', 0, TRUE, ddoinv, CMD_ARG_NONE},
	{"menuinv", "", 'I', 0, TRUE, dotypeinv, CMD_ARG_NONE},
	{"invoke", "", M('i'), 0, TRUE, doinvoke, CMD_ARG_NONE},
	{"jump", "", M('j'), 'j', FALSE, dojump, CMD_ARG_NONE},
	{"loot", "", M('l'), 'l', FALSE, doloot, CMD_ARG_NONE},
	{"monability", "", M('m'), 0, TRUE, domonability, CMD_ARG_NONE},
	{"name", "", M('n'), 'N', TRUE, ddocall, CMD_ARG_NONE},
	{"open", "", 'o', 0, FALSE, doopen, CMD_ARG_NONE},
	{"sacrifice", "", M('o'), 0, FALSE, dosacrifice, CMD_ARG_NONE},
	{"pay", "", 'p', 0, FALSE, dopay, CMD_ARG_NONE},
	{"put on", "", 'P', 0, FALSE, doputon, CMD_ARG_NONE},
	{"pray", "", M('p'), 0, TRUE, dopray, CMD_ARG_NONE},
	{"drink", "", 'q', 0, FALSE, dodrink, CMD_ARG_NONE},
	{"quiver", "", 'Q', 0, FALSE, dowieldquiver, CMD_ARG_NONE},
	{"quit", "", M('q'), 0, TRUE, done2, CMD_ARG_NONE},
	{"read", "", 'r', 0, FALSE, doread, CMD_ARG_NONE},
	{"remove", "", 'R', 0, FALSE, doremring, CMD_ARG_NONE},
	{"rub", "", M('r'), 0, FALSE, dorub, CMD_ARG_NONE},
	{"search", "", 's', 0, TRUE, dosearch, CMD_ARG_NONE, "searching"},
	{"save", "", 'S', 0, TRUE, dosave, CMD_ARG_NONE},
	{"sit", "", M('s'), 0, FALSE, dosit, CMD_ARG_NONE},
	{"throw", "", 't', 0, FALSE, dothrow, CMD_ARG_NONE},
	{"takeoff", "", 'T', 0, FALSE, dotakeoff, CMD_ARG_NONE},
	{"turn", "", M('t'), 0, TRUE, doturn, CMD_ARG_NONE},
	{"untrap", "", M('u'), 'u', FALSE, dountrap, CMD_ARG_NONE},
	{"version", "", 'v', 0, TRUE, doversion, CMD_ARG_NONE},
	{"verhistory", "", 'V', 0, TRUE, dohistory, CMD_ARG_NONE},
	{"version", "", M('v'), 0, TRUE, doextversion, CMD_ARG_NONE},
	{"wield", "", 'w', 0, FALSE, dowield, CMD_ARG_NONE},
	{"wear", "", 'W', 0, FALSE, dowear, CMD_ARG_NONE},
	{"wipe", "", M('w'), 0, FALSE, dowipe, CMD_ARG_NONE},
	{"swapweapon", "", 'x', 0, FALSE, doswapweapon, CMD_ARG_NONE},
	{"explore mode", "", 'X', 0, TRUE, enter_explore_mode, CMD_ARG_NONE},
	{"zap", "", 'z', 0, FALSE, dozap, CMD_ARG_NONE},
	{"cast", "", 'Z', 0, TRUE, docast, CMD_ARG_NONE},
	{"go up", "", '<', 0, FALSE, doup, CMD_ARG_NONE},
	{"go down", "", '>', 0, FALSE, dodown, CMD_ARG_NONE},
	{"whatis", "", '/', 0, TRUE, dowhatis, CMD_ARG_NONE},
	{"whatdoes", "", '&', 0, TRUE, dowhatdoes, CMD_ARG_NONE},
	{"wait", "", '.', ' ', TRUE, donull, CMD_ARG_NONE, "waiting"},
	{"pickup", "", ',', 0, FALSE, dopickup, CMD_ARG_NONE},
	{"look", "", ':', 0, TRUE, dolook, CMD_ARG_NONE},
	{"quickwhatis", "", ';', 0, TRUE, doquickwhatis, CMD_ARG_NONE},
	{"idtrap", "", '^', 0, TRUE, doidtrap, CMD_ARG_NONE},
	{"showdiscovered", "", '\\', 0, TRUE, dodiscovered, CMD_ARG_NONE},
	{"togglepickup", "", '@', 0, TRUE, dotogglepickup, CMD_ARG_NONE},
	{"twoweapon", "", M('2'), 0, FALSE, dotwoweapon, CMD_ARG_NONE},
	{"showweapon", "", WEAPON_SYM, 0, TRUE, doprwep, CMD_ARG_NONE},
	{"showarmor", "", ARMOR_SYM, 0, TRUE, doprarm, CMD_ARG_NONE},
	{"showrings", "", RING_SYM, 0, TRUE, doprring, CMD_ARG_NONE},
	{"showamulets", "", AMULET_SYM, 0, TRUE, dopramulet, CMD_ARG_NONE},
	{"showtools", "", TOOL_SYM, 0, TRUE, doprtool, CMD_ARG_NONE},
	{"showworn", "", '*', 0, TRUE, doprinuse, CMD_ARG_NONE},
	{"showgold", "", GOLD_SYM, 0, TRUE, doprgold, CMD_ARG_NONE},
	{"showspells", "", SPBOOK_SYM, 0, TRUE, dovspell, CMD_ARG_NONE},
	{"travel", "", '_', 0, TRUE, dotravel, CMD_ARG_NONE | CMD_ARG_POS},
	
	{"adjust", "adjust inventory letters", 0, 0, TRUE, doorganize, CMD_ARG_NONE | CMD_EXT},
	{"chat", "talk to someone", 0, 0, TRUE, dotalk, CMD_ARG_NONE | CMD_EXT},	/* converse? */
	{"conduct", "list which challenges you have adhered to", 0, 0, TRUE, doconduct, CMD_ARG_NONE | CMD_EXT},
	{"dip", "dip an object into something", 0, 0, FALSE, dodip, CMD_ARG_NONE | CMD_EXT},
	{"enhance", "advance or check weapons skills", 0, 0, TRUE, enhance_weapon_skill, CMD_ARG_NONE | CMD_EXT},
	{"force", "force a lock", 0, 0, FALSE, doforce, CMD_ARG_NONE | CMD_EXT},
	{"invoke", "invoke an object's powers", 0, 0, TRUE, doinvoke, CMD_ARG_NONE | CMD_EXT},
	{"jump", "jump to a location", 0, 0, FALSE, dojump, CMD_ARG_NONE | CMD_EXT},
	{"loot", "loot a box on the floor", 0, 0, FALSE, doloot, CMD_ARG_NONE | CMD_EXT},
	{"monster", "use a monster's special ability", 0, 0, TRUE, domonability, CMD_ARG_NONE | CMD_EXT},
	{"name", "name an item or type of object", 0, 0, TRUE, ddocall, CMD_ARG_NONE | CMD_EXT},
	{"offer", "offer a sacrifice to the gods", 0, 0, FALSE, dosacrifice, CMD_ARG_NONE | CMD_EXT},
	{"pray", "pray to the gods for help", 0, 0, TRUE, dopray, CMD_ARG_NONE | CMD_EXT},
	{"quit", "exit without saving current game", 0, 0, TRUE, done2, CMD_ARG_NONE | CMD_EXT},
	{"ride", "ride (or stop riding) a monster", 0, 0, FALSE, doride, CMD_ARG_NONE | CMD_EXT},
	{"rub", "rub a lamp or a stone", 0, 0, FALSE, dorub, CMD_ARG_NONE | CMD_EXT},
	{"sit", "sit down", 0, 0, FALSE, dosit, CMD_ARG_NONE | CMD_EXT},
	{"turn", "turn undead", 0, 0, TRUE, doturn, CMD_ARG_NONE | CMD_EXT},
	{"twoweapon", "toggle two-weapon combat", 0, 0, FALSE, dotwoweapon, CMD_ARG_NONE | CMD_EXT},
	{"untrap", "untrap something", 0, 0, FALSE, dountrap, CMD_ARG_NONE | CMD_EXT},
	{"wipe", "wipe off your face", 0, 0, FALSE, dowipe, CMD_ARG_NONE | CMD_EXT},
	
	{"levelchange", "change experience level", 0, 0, TRUE, wiz_level_change, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"lightsources", "show mobile light sources", 0, 0, TRUE, wiz_light_sources, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
#ifdef DEBUG_MIGRATING_MONS
	{"migratemons", "migrate n random monsters", 0, 0, TRUE, wiz_migrate_mons, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
#endif
	{"monpolycontrol", "control monster polymorphs", 0, 0, TRUE, wiz_mon_polycontrol, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"panic", "test panic routine (fatal to game)", 0, 0, TRUE, wiz_panic, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"polyself", "polymorph self", 0, 0, TRUE, wiz_polyself, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"seenv", "show seen vectors", 0, 0, TRUE, wiz_show_seenv, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"stats", "show memory statistics", 0, 0, TRUE, wiz_show_stats, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"timeout", "look at timeout queue", 0, 0, TRUE, wiz_timeout_queue, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{"vision", "show vision array", 0, 0, TRUE, wiz_show_vision, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
#ifdef DEBUG
	{"wizdebug", "wizard debug command", 0, 0, TRUE, wiz_debug_cmd, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
#endif
	{"wmode", "show wall modes", 0, 0, TRUE, wiz_show_wmodes, CMD_ARG_NONE | CMD_DEBUG | CMD_EXT},
	{NULL, NULL, 0, 0, 0, 0, 0, NULL}
};


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
	    if (IS_FOUNTAIN(level.locations[u.ux][u.uy].typ)) {
		if (split_mon(&youmonst, NULL))
		    dryup(u.ux, u.uy, TRUE);
	    } else There("is no fountain here.");
	} else if (is_unicorn(youmonst.data)) {
	    use_unicorn_horn(NULL);
	    return 1;
	} else if (youmonst.data->msound == MS_SHRIEK) {
	    You("shriek.");
	    if (u.uburied)
		pline("Unfortunately sound does not carry well through rock.");
	    else aggravate();
	} else if (Upolyd)
		pline("Any special ability you may have is purely reflexive.");
	else You("don't have a special ability in your normal form!");
	return 0;
}

static int enter_explore_mode(void)
{
	if (!discover && !wizard) {
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
	if (wizard)  findit();
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
	pline("Never mind.");
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
		    v = level.locations[x][y].seenv & 0xff;
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
		lev = &level.locations[x][y];
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
#define you_have_X(menu,something)	enl_msg(menu,You_,have,(const char *)"","something")

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
			"something");
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

	    if (uleft && uleft->otyp == RIN_ADORNMENT) adorn += uleft->spe;
	    if (uright && uright->otyp == RIN_ADORNMENT) adorn += uright->spe;
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

	    if (uleft && uleft->otyp == RIN_PROTECTION) prot += uleft->spe;
	    if (uright && uright->otyp == RIN_PROTECTION) prot += uright->spe;
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


struct nh_cmd_desc *nh_get_commands(int *count, boolean include_debug)
{
	int i, j, cmdcount = 0;
	struct nh_cmd_desc *ui_cmd;
	
	for (i = 0; cmdlist[i].name; i++)
	    if (include_debug || !(cmdlist[i].flags & CMD_DEBUG))
		cmdcount++;
    
	ui_cmd = xmalloc(sizeof(struct nh_cmd_desc) * cmdcount);
	if (!ui_cmd)
	    return NULL;

	j = 0;
	for (i = 0; cmdlist[i].name; i++)
	    if (include_debug || !(cmdlist[i].flags & CMD_DEBUG)) {
		ui_cmd[j].name = cmdlist[i].name;
		ui_cmd[j].desc = cmdlist[i].desc;
		ui_cmd[j].defkey = cmdlist[i].defkey;
		ui_cmd[j].altkey = cmdlist[i].altkey;
		ui_cmd[j].flags = cmdlist[i].flags;
		j++;
	    }
    
	*count = cmdcount;
	return ui_cmd;
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
	count_obj(level.objlist, &count, &size, FALSE, TRUE);
	count_obj(level.buriedobjlist, &count, &size, FALSE, TRUE);
	count_obj(migrating_objs, &count, &size, FALSE, TRUE);
	/* DEADMONSTER check not required in this loop since they have no inventory */
	for (mon = level.monlist; mon; mon = mon->nmon)
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
	obj_chain(&menu, "level.objlist", level.objlist, &total_obj_count, &total_obj_size);
	obj_chain(&menu, "buried", level.buriedobjlist,
				&total_obj_count, &total_obj_size);
	obj_chain(&menu, "migrating obj", migrating_objs,
				&total_obj_count, &total_obj_size);
	mon_invent_chain(&menu, "minvent", level.monlist,
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

	mon_chain(&menu, "level.monlist", level.monlist,
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


boolean dir_to_delta(enum nh_direction dir, schar *dx, schar *dy, schar *dz)
{
	if (dir == DIR_NONE)
	    return FALSE;
	
	*dx = xdir[dir];
	*dy = ydir[dir];
	*dz = zdir[dir];
	
	return TRUE;
}


int get_command_idx(const char *command)
{
	int i;
	
	if (!command || !command[0])
	    return -1;
	
	for (i = 0; cmdlist[i].name; i++)
	    if (!strcmp(command, cmdlist[i].name))
		return i;
	
	return -1;
}

static int prev_command;
static struct nh_cmd_arg prev_arg = {CMD_ARG_NONE};
static int prev_repcount = 0;

int do_command(int command, int repcount, boolean firsttime, struct nh_cmd_arg *arg)
{
	schar dx, dy, dz;
	int x, y;
	int res, (*func)(void), (*func_dir)(int, int, int), (*func_pos)(int,int);
	struct nh_cmd_arg noarg = {CMD_ARG_NONE};
	int argtype, functype;
	
	/* for multi-turn movement, we use re-invocation of do_command rather
	 * than set_occupation, so the relevant command must be saved and restored */
	if (command == -1) {
	    command = prev_command;
	    arg = &prev_arg;
	    repcount = prev_repcount;
	}
	
	prev_command = command;
	prev_arg = *arg;
	prev_repcount = repcount;
	
	/* NULL arg is synonymous to CMD_ARG_NONE */
	if (!arg)
	    arg = &noarg;
	
	flags.move = FALSE;
	multi = 0;
	
	if (firsttime) {
	    u.last_str_turn = 0; /* for flags.run > 1 movement */
	    flags.nopick = 0;
	}
	
	if (command == -1)
	    return COMMAND_UNKNOWN;
	
	/* in some cases, a command function will accept either it's proper argument
	 * type or no argument; we're looking for the possible type of the argument here */
	functype = (cmdlist[command].flags & CMD_ARG_FLAGS);
	if (!functype)
	    functype = CMD_ARG_NONE;
	    
	argtype = (arg->argtype & cmdlist[command].flags);
	if (!argtype)
	    return COMMAND_BAD_ARG;
	
	if (u.uburied && !cmdlist[command].can_if_buried) {
	    You_cant("do that while you are buried!");
	    res = 0;
	} else {
	    flags.move = TRUE;
	    multi = repcount;
	    
	    switch (functype) {
		case CMD_ARG_NONE:
		    func = cmdlist[command].func;
		    if (cmdlist[command].text && !occupation && multi > 1)
			set_occupation(func, cmdlist[command].text, multi - 1);
		    res = (*func)();		/* perform the command */
		    break;
		
		case CMD_ARG_DIR:
		    func_dir = cmdlist[command].func;
		    if (argtype == CMD_ARG_DIR) {
			if (!dir_to_delta(arg->d, &dx, &dy, &dz))
			    return COMMAND_BAD_ARG;
		    } else {
			 /* invalid direction deltas indicate that no arg was given */
			dx = -2; dy = -2; dz = -2;
		    }
		    res = func_dir(dx, dy, dz);
		    break;
		
		case CMD_ARG_POS:
		    func_pos = cmdlist[command].func;
		    if (argtype == CMD_ARG_POS) {
			x = arg->pos.x;
			y = arg->pos.y;
		    } else {
			x = -1;
			y = -1;
		    }
		    func_pos(x, y);
		    break;
		    
		default:
		    multi = 0;
		    return COMMAND_BAD_ARG;
	    }
	    
	    if (multi > 0)
		--multi;
	}
	
	if (!res) {
	    flags.move = FALSE;
	    multi = 0;
	}
	
	return COMMAND_OK;
}



int xytod(schar x, schar y)	/* convert an x,y pair into a direction code */
{
	int dd;

	for (dd = 0; dd < 8; dd++)
	    if (x == xdir[dd] && y == ydir[dd]) return dd;

	return -1;
}

void dtoxy(coord *cc, int dd)	/* convert a direction code into an x,y pair */
{
	cc->x = xdir[dd];
	cc->y = ydir[dd];
	return;
}


/*
 * uses getdir() but unlike getdir() it specifically
 * produces coordinates using the direction from getdir()
 * and verifies that those coordinates are ok.
 *
 * If the call to getdir() returns 0, "Never mind." is displayed.
 * If the resulting coordinates are not okay, emsg is displayed.
 *
 * Returns non-zero if coordinates in cc are valid.
 */
int get_adjacent_loc(const char *prompt, const char *emsg, xchar x, xchar y,
		     coord *cc, schar *dz)
{
	xchar new_x, new_y;
	schar dx, dy;
	
	if (!getdir(prompt, &dx, &dy, dz)) {
		pline("Never mind.");
		return 0;
	}
	new_x = x + dx;
	new_y = y + dy;
	if (cc && isok(new_x, new_y)) {
		cc->x = new_x;
		cc->y = new_y;
	} else {
		if (emsg)
		    pline(emsg);
		return 0;
	}
	return 1;
}


void confdir(schar *dx, schar *dy)
{
	int x = (u.umonnum == PM_GRID_BUG) ? 2*rn2(4) : rn2(8);
	*dx = xdir[x];
	*dy = ydir[x];
	return;
}


int isok(int x, int y)
{
	/* x corresponds to curx, so x==1 is the first column. Ach. %% */
	return x >= 1 && x <= COLNO-1 && y >= 0 && y <= ROWNO-1;
}


static int dotravel(int x, int y)
{
	/* Keyboard travel command */
	coord cc;

	if (!iflags.travelcmd)
	    return 0;
	
	if (x == -1 && y == -1) {
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
	} else {
	    cc.x = x;
	    cc.y = y;
	}
	iflags.travelcc.x = u.tx = cc.x;
	iflags.travelcc.y = u.ty = cc.y;

	flags.travel = 1;
	iflags.travel1 = 1;
	flags.run = 8;
	flags.nopick = 1;
	if (!multi)
	    multi = max(COLNO,ROWNO);
	u.last_str_turn = 0;
	flags.mv = TRUE;
	
	domove(0, 0, 0);
	
	return 1;
}

/*cmd.c*/
