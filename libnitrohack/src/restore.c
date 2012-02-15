/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"

static void find_lev_obj(struct level *lev);
static void restlevchn(struct memfile *mf);
static void restdamage(struct memfile *mf, struct level *lev, boolean ghostly);
static struct obj *restobjchn(struct memfile *mf, struct level *lev,
			      boolean ghostly, boolean frozen);
static struct monst *restmonchn(struct memfile *mf, struct level *lev, boolean ghostly);
static struct fruit *loadfruitchn(struct memfile *mf);
static void freefruitchn(struct fruit *);
static void ghostfruit(struct obj *);
static void restgamestate(struct memfile *mf);
static void reset_oattached_mids(boolean ghostly, struct level *lev);

/*
 * Save a mapping of IDs from ghost levels to the current level.  This
 * map is used by the timer routines when restoring ghost levels.
 */
#define N_PER_BUCKET 64
struct bucket {
    struct bucket *next;
    struct {
	unsigned gid;	/* ghost ID */
	unsigned nid;	/* new ID */
    } map[N_PER_BUCKET];
};

static void clear_id_mapping(void);
static void add_id_mapping(unsigned, unsigned);

static int n_ids_mapped = 0;
static struct bucket *id_map = 0;


#include "quest.h"

boolean restoring = FALSE;
static struct fruit *oldfruit;

#define Is_IceBox(o) ((o)->otyp == ICE_BOX ? TRUE : FALSE)

/* Recalculate lev->objects[x][y], since this info was not saved. */
static void find_lev_obj(struct level *lev)
{
	struct obj *fobjtmp = NULL;
	struct obj *otmp;
	int x,y;

	for (x=0; x<COLNO; x++) for(y=0; y<ROWNO; y++)
		lev->objects[x][y] = NULL;

	/*
	 * Reverse the entire lev->objlist chain, which is necessary so that we can
	 * place the objects in the proper order.  Make all obj in chain
	 * OBJ_FREE so place_object will work correctly.
	 */
	while ((otmp = lev->objlist) != 0) {
		lev->objlist = otmp->nobj;
		otmp->nobj = fobjtmp;
		otmp->where = OBJ_FREE;
		fobjtmp = otmp;
	}
	/* lev->objlist should now be empty */

	/* Set lev->objects (as well as reversing the chain back again) */
	while ((otmp = fobjtmp) != 0) {
		fobjtmp = otmp->nobj;
		place_object(otmp, lev, otmp->ox, otmp->oy);
	}
}

/* Things that were marked "in_use" when the game was saved (ex. via the
 * infamous "HUP" cheat) get used up here.
 */
void inven_inuse(boolean quietly)
{
	struct obj *otmp, *otmp2;

	for (otmp = invent; otmp; otmp = otmp2) {
	    otmp2 = otmp->nobj;
	    if (otmp->in_use) {
		if (!quietly) pline("Finishing off %s...", xname(otmp));
		useup(otmp);
	    }
	}
}

static void restlevchn(struct memfile *mf)
{
	int cnt;
	s_level	*tmplev, *x;

	sp_levchn = NULL;
	cnt = mread32(mf);
	for (; cnt > 0; cnt--) {
	    tmplev = malloc(sizeof(s_level));
	    tmplev->flags = restore_d_flags(mf);
	    mread(mf, &tmplev->dlevel, sizeof(tmplev->dlevel));
	    mread(mf, tmplev->proto, sizeof(tmplev->proto));
	    tmplev->boneid = mread8(mf);
	    tmplev->rndlevs = mread8(mf);
	    
	    if (!sp_levchn)
		sp_levchn = tmplev;
	    else {
		for (x = sp_levchn; x->next; x = x->next);
		x->next = tmplev;
	    }
	    tmplev->next = NULL;
	}
}


static void restdamage(struct memfile *mf, struct level *lev, boolean ghostly)
{
	int counter;
	struct damage *tmp_dam;

	counter = mread32(mf);
	if (!counter)
	    return;
	tmp_dam = malloc(sizeof(struct damage));
	while (--counter >= 0) {
	    char damaged_shops[5], *shp = NULL;

	    tmp_dam->when = mread32(mf);
	    tmp_dam->cost = mread32(mf);
	    tmp_dam->place.x = mread8(mf);
	    tmp_dam->place.y = mread8(mf);
	    tmp_dam->typ = mread8(mf);
	    
	    if (ghostly)
		tmp_dam->when += (moves - lev->lastmoves);
	    strcpy(damaged_shops,
		   in_rooms(lev, tmp_dam->place.x, tmp_dam->place.y, SHOPBASE));
	    
	    for (shp = damaged_shops; *shp; shp++) {
		struct monst *shkp = shop_keeper(lev, *shp);

		if (shkp && inhishop(shkp) &&
			repair_damage(lev, shkp, tmp_dam, TRUE))
		    break;
	    }
	    
	    if (!shp || !*shp) {
		tmp_dam->next = lev->damagelist;
		lev->damagelist = tmp_dam;
		tmp_dam = malloc(sizeof(*tmp_dam));
	    }
	}
	free(tmp_dam);
}


static struct obj *restobjchn(struct memfile *mf, struct level *lev,
			      boolean ghostly, boolean frozen)
{
	struct obj *otmp, *otmp2 = 0;
	struct obj *first = NULL;
	unsigned int count;

	mfmagic_check(mf, OBJCHAIN_MAGIC);
	count = mread32(mf);
	
	while (count--) {
	    otmp = restore_obj(mf);
	    otmp->olev = lev;
	    if (!first)
		first = otmp;
	    else
		otmp2->nobj = otmp;
	    
	    if (ghostly) {
		unsigned nid = flags.ident++;
		add_id_mapping(otmp->o_id, nid);
		otmp->o_id = nid;
	    }
	    if (ghostly && otmp->otyp == SLIME_MOLD) ghostfruit(otmp);
	    /* Ghost levels get object age shifted from old player's clock
		* to new player's clock.  Assumption: new player arrived
		* immediately after old player died.
		*/
	    if (ghostly && !frozen && !age_is_relative(otmp))
		otmp->age = moves - lev->lastmoves + otmp->age;

	    /* get contents of a container or statue */
	    if (Has_contents(otmp)) {
		struct obj *otmp3;
		otmp->cobj = restobjchn(mf, lev, ghostly, Is_IceBox(otmp));
		/* restore container back pointers */
		for (otmp3 = otmp->cobj; otmp3; otmp3 = otmp3->nobj)
		    otmp3->ocontainer = otmp;
	    }
	    if (otmp->bypass) otmp->bypass = 0;

	    otmp2 = otmp;
	}
	if (first && otmp2->nobj){
		impossible("Restobjchn: error reading objchn.");
		otmp2->nobj = 0;
	}

	return first;
}


static struct monst *restmonchn(struct memfile *mf, struct level *lev, boolean ghostly)
{
	struct monst *mtmp, *mtmp2 = NULL;
	struct monst *first = NULL;
	struct obj *obj;
	unsigned int count, mndx;

	/* get the original base address */
	mfmagic_check(mf, MONCHAIN_MAGIC);
	count = mread32(mf);

	while (count--) {
	    mtmp = restore_mon(mf);
	    if (!first)
		first = mtmp;
	    else
		mtmp2->nmon = mtmp;
	    mtmp->dlevel = lev;
	    
	    if (ghostly) {
		unsigned nid = flags.ident++;
		add_id_mapping(mtmp->m_id, nid);
		mtmp->m_id = nid;

		mndx = monsndx(mtmp->data);
		if (propagate(mndx, TRUE, ghostly) == 0) {
		    /* cookie to trigger purge in getbones() */
		    mtmp->mhpmax = DEFUNCT_MONSTER;	
		}
	    }
	    
	    if (mtmp->minvent) {
		mtmp->minvent = restobjchn(mf, lev, ghostly, FALSE);
		/* restore monster back pointer */
		for (obj = mtmp->minvent; obj; obj = obj->nobj)
		    obj->ocarry = mtmp;
	    }
	    
	    if (mtmp->mw) {
		for (obj = mtmp->minvent; obj; obj = obj->nobj)
		    if (obj->owornmask & W_WEP) break;
		if (obj) mtmp->mw = obj;
		else {
		    MON_NOWEP(mtmp);
		    impossible("bad monster weapon restore");
		}
	    }

	    if (mtmp->isshk) restshk(mtmp, ghostly);
	    if (mtmp->ispriest) restpriest(mtmp, ghostly);

	    mtmp2 = mtmp;
	}
	if (first && mtmp2->nmon){
	    impossible("Restmonchn: error reading monchn.");
	    mtmp2->nmon = 0;
	}
	
	return first;
}


static struct fruit *loadfruitchn(struct memfile *mf)
{
	struct fruit *flist = NULL, *fnext;
	unsigned int count;

	mfmagic_check(mf, FRUITCHAIN_MAGIC);
	count = mread32(mf);
	while (count--) {
	    fnext = newfruit();
	    mread(mf, fnext->fname, sizeof(fnext->fname));
	    fnext->fid = mread32(mf);
	    fnext->nextf = flist;
	    flist = fnext;
	}
	return flist;
}


static void freefruitchn(struct fruit *flist)
{
	struct fruit *fnext;

	while (flist) {
	    fnext = flist->nextf;
	    dealloc_fruit(flist);
	    flist = fnext;
	}
}


static void ghostfruit(struct obj *otmp)
{
	struct fruit *oldf;

	for (oldf = oldfruit; oldf; oldf = oldf->nextf)
		if (oldf->fid == otmp->spe) break;

	if (!oldf) impossible("no old fruit?");
	else otmp->spe = fruitadd(oldf->fname);
}


static void restore_mvitals(struct memfile *mf)
{
	int i;
	for (i = 0; i < NUMMONS; i++) {
	    mvitals[i].born = mread8(mf);
	    mvitals[i].died = mread8(mf);
	    mvitals[i].mvflags = mread8(mf);
	}
}


static void restore_quest_status(struct memfile *mf)
{
	unsigned int qflags;
	
	qflags = mread32(mf);
	quest_status.first_start	= (qflags >> 31) & 1;
	quest_status.met_leader		= (qflags >> 30) & 1;
	quest_status.not_ready		= (qflags >> 27) & 7;
	quest_status.pissed_off		= (qflags >> 26) & 1;
	quest_status.got_quest		= (qflags >> 25) & 1;
	quest_status.first_locate	= (qflags >> 24) & 1;
	quest_status.met_intermed	= (qflags >> 23) & 1;
	quest_status.got_final		= (qflags >> 22) & 1;
	quest_status.made_goal		= (qflags >> 19) & 7;
	quest_status.met_nemesis	= (qflags >> 18) & 1;
	quest_status.killed_nemesis	= (qflags >> 17) & 1;
	quest_status.in_battle		= (qflags >> 16) & 1;
	quest_status.cheater		= (qflags >> 15) & 1;
	quest_status.touched_artifact	= (qflags >> 14) & 1;
	quest_status.offered_artifact	= (qflags >> 13) & 1;
	quest_status.got_thanks		= (qflags >> 12) & 1;
	quest_status.leader_is_dead	= (qflags >> 11) & 1;
	
	quest_status.leader_m_id = mread32(mf);
}


static void restore_spellbook(struct memfile *mf)
{
	int i;
	for (i = 0; i < MAXSPELL + 1; i++) {
	    spl_book[i].sp_know = mread32(mf);
	    spl_book[i].sp_id = mread16(mf);
	    spl_book[i].sp_lev = mread8(mf);
	}
}


static void restgamestate(struct memfile *mf)
{
	struct obj *otmp;
	unsigned int bookid = 0;
	struct monst *mtmp;
	struct level *lev;
	
	mfmagic_check(mf, STATE_MAGIC);

	lev = levels[ledger_no(&u.uz)];

	/* this stuff comes after potential aborted restore attempts */
	restore_timers(mf, lev, RANGE_GLOBAL, FALSE, 0L);
	restore_light_sources(mf, lev);
	invent = restobjchn(mf, lev, FALSE, FALSE);
	migrating_objs = restobjchn(mf, lev, FALSE, FALSE);
	migrating_mons = restmonchn(mf, lev, FALSE);
	restore_mvitals(mf);

	/* this comes after inventory has been loaded */
	for (otmp = invent; otmp; otmp = otmp->nobj)
		if (otmp->owornmask)
			setworn(otmp, otmp->owornmask);
	/* reset weapon so that player will get a reminder about "bashing"
	   during next fight when bare-handed or wielding an unconventional
	   item; for pick-axe, we aren't able to distinguish between having
	   applied or wielded it, so be conservative and assume the former */
	otmp = uwep;	/* `uwep' usually init'd by setworn() in loop above */
	uwep = 0;	/* clear it and have setuwep() reinit */
	setuwep(otmp);	/* (don't need any null check here) */
	if (!uwep || uwep->otyp == PICK_AXE || uwep->otyp == GRAPPLING_HOOK)
	    unweapon = TRUE;

	restore_quest_status(mf);
	restore_spellbook(mf);
	restore_artifacts(mf);
	restore_oracles(mf);
	mread(mf, pl_character, sizeof(pl_character));

	mread(mf, pl_fruit, sizeof pl_fruit);
	current_fruit = mread32(mf);
	freefruitchn(ffruit);	/* clean up fruit(s) made by initoptions() */
	/* set it to NULL before loadfruitchn, otherwise loading a faulty fruit
	 * chain will crash in terminate -> freedynamicdata -> freefruitchn */
	ffruit = NULL;
	ffruit = loadfruitchn(mf);

	restnames(mf);
	restore_waterlevel(mf, lev);
	lastinvnr = mread32(mf);
	restore_mt_state(mf);
	restore_track(mf);
	restore_food(mf);
	restore_steal(mf);
	restore_dig_status(mf);
	bookid = mread32(mf);
	if (bookid)
	    book = find_oid(bookid);
	stetho_last_used_move = mread32(mf);
	stetho_last_used_movement = mread32(mf);
	multi = mread32(mf);
	restore_rndmonst_state(mf);
	restore_history(mf);
	
	/* must come after all mons & objs are restored */
	relink_timers(FALSE, lev);
	relink_light_sources(FALSE, lev);
	
	if (u.ustuck) {
		for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
			if (mtmp->m_id == (long)u.ustuck) break;
		if (!mtmp) panic("Cannot find the monster ustuck.");
		u.ustuck = mtmp;
	}
	if (u.usteed) {
		for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
			if (mtmp->m_id == (long)u.usteed) break;
		if (!mtmp) panic("Cannot find the monster usteed.");
		u.usteed = mtmp;
		remove_monster(lev, mtmp->mx, mtmp->my);
	}
}


void restore_flags(struct memfile *mf, struct flag *f)
{
	memset(f, 0, sizeof(struct flag));
	
	f->ident = mread32(mf);
	f->moonphase = mread32(mf);
	f->no_of_wizards = mread32(mf);
	f->init_role = mread32(mf);
	f->init_race = mread32(mf);
	f->init_gend = mread32(mf);
	f->init_align = mread32(mf);
	f->randomall = mread32(mf);
	f->pantheon = mread32(mf);
	f->run = mread32(mf);
        f->warntype = mread32(mf);
	f->warnlevel = mread32(mf);
	f->djinni_count = mread32(mf);
	f->ghost_count = mread32(mf);
	f->pickup_burden = mread32(mf);
	
	f->autodig = mread8(mf);
	f->autoquiver = mread8(mf);
	f->beginner = mread8(mf);
	f->confirm = mread8(mf);
	f->debug = mread8(mf);
	f->explore = mread8(mf);
	f->female = mread8(mf);
	f->forcefight = mread8(mf);
	f->friday13 = mread8(mf);
	f->legacy = mread8(mf);
	f->lit_corridor = mread8(mf);
	f->made_amulet = mread8(mf);
	f->mon_moving = mread8(mf);
	f->move = mread8(mf);
	f->mv = mread8(mf);
	f->nopick = mread8(mf);
	f->null = mread8(mf);
	f->pickup = mread8(mf);
	f->pushweapon = mread8(mf);
	f->rest_on_space = mread8(mf);
	f->safe_dog = mread8(mf);
	f->silent = mread8(mf);
	f->sortpack = mread8(mf);
	f->soundok = mread8(mf);
	f->sparkle = mread8(mf);
	f->tombstone = mread8(mf);
	f->verbose = mread8(mf);
	f->prayconfirm = mread8(mf);
	f->travel = mread8(mf);
	f->end_disclose = mread8(mf);
	f->menu_style = mread8(mf);
	f->elbereth_enabled = mread8(mf);
	f->rogue_enabled = mread8(mf);
	f->seduce_enabled = mread8(mf);
	f->bones_enabled = mread8(mf);
	
	mread(mf, f->inv_order, sizeof(f->inv_order));
}


int dorecover(struct memfile *mf)
{
	int count;
	xchar ltmp;
	struct obj *otmp;
	struct monst *mtmp;
	
	level = NULL; /* level restore must not use this pointer */
	
	if (!uptodate(mf, NULL))
	    return 0;
	
	restore_flags(mf, &flags);
	flags.bypasses = 0;	/* never use a saved value of bypasses */

	restore_you(mf, &u);
	role_init();	/* Reset the initial role, race, gender, and alignment */
	
	moves = mread32(mf);
	
	mtmp = restore_mon(mf);
	youmonst = *mtmp;
	dealloc_monst(mtmp);
	set_uasmon(); /* fix up youmonst.data */
	
	/* restore dungeon */
	restore_dungeon(mf);
	restlevchn(mf);
	
	/* restore levels */
	count = mread32(mf);
	for ( ; count; count--) {
	    ltmp = mread8(mf);
	    getlev(mf, ltmp, FALSE);
	}
	
	restgamestate(mf);

	/* all data has been read, prepare for player */
	level = levels[ledger_no(&u.uz)];

	max_rank_sz(); /* to recompute mrank_sz (botl.c) */
	/* take care of iron ball & chain */
	for (otmp = level->objlist; otmp; otmp = otmp->nobj)
		if (otmp->owornmask)
			setworn(otmp, otmp->owornmask);

	/* in_use processing must be after:
	 *    + The inventory has been read so that freeinv() works.
	 *    + The current level has been restored so billing information
	 *	is available.
	 */
	inven_inuse(FALSE);

	load_qtlist();	/* re-load the quest text info */
	/* Set up the vision internals, after level->locations[] data is loaded */
	/* but before doredraw().					    */
	vision_reset();
	vision_full_recalc = 1;	/* recompute vision (not saved) */

	/* help the window port get it's display charset/tiles sorted out */
	notify_levelchange();

	run_timers();	/* expire all timers that have gone off while away */
	doredraw();
	restoring = FALSE;
	program_state.something_worth_saving++;	/* useful data now exists */
	
	check_special_room(FALSE);
	flags.move = 0;

	/* Success! */
	return 1;
}


/* wrapper for dorecover so that data can come from a file */
int dorecover_fd(int infd)
{
	int ret;
	struct memfile mf = {NULL, 0, 0};
	long initial_pos;

	restoring = TRUE;

	initial_pos = lseek(infd, 0, SEEK_CUR);
	mf.buf = loadfile(infd, &mf.len);
	if (!mf.buf)
	    return 0;
	
	ret = dorecover(&mf);
	free(mf.buf);
	
	if (ret) {
	    /* erase the binary portion of the logfile */
	    lseek(infd, initial_pos, SEEK_SET);
	    ftruncate(infd, initial_pos);
	}
	
	return ret;
}


void trickery(char *reason)
{
	pline("Strange, this map is not as I remember it.");
	pline("Somebody is trying some trickery here...");
	pline("This game is void.");
	killer = reason;
	done(TRICKED);
}


static void restore_location(struct memfile *mf, struct rm *loc)
{
	unsigned int lflags1;
	unsigned short lflags2;
	
	lflags1 = mread32(mf);
	loc->typ = mread8(mf);
	loc->seenv = mread8(mf);
	lflags2 = mread16(mf);
	loc->mem_bg	= (lflags1 >> 26) & 63;
	loc->mem_trap	= (lflags1 >> 21) & 31;
	loc->mem_obj	= (lflags1 >> 11) & 1023;
	loc->mem_obj_mn	= (lflags1 >> 2) & 511;
	loc->mem_invis	= (lflags1 >> 1) & 1;
	loc->flags	= (lflags2 >> 11) & 31;
	loc->horizontal	= (lflags2 >> 10) & 1;
	loc->lit	= (lflags2 >> 9) & 1;
	loc->waslit	= (lflags2 >> 8) & 1;
	loc->roomno	= (lflags2 >> 2) & 63;
	loc->edge	= (lflags2 >> 1) & 1;
}


static struct trap *restore_traps(struct memfile *mf)
{
	struct trap *trap, *first = NULL;
	unsigned int count, tflags;
	
	mfmagic_check(mf, TRAPCHAIN_MAGIC);
	count = mread32(mf);
	
	while (count--) {
	    trap = newtrap();
	    
	    trap->tx = mread8(mf);
	    trap->ty = mread8(mf);
	    trap->dst.dnum = mread8(mf);
	    trap->dst.dlevel = mread8(mf);
	    trap->launch.x = mread8(mf);
	    trap->launch.y = mread8(mf);
	    
	    tflags = mread16(mf);
	    trap->ttyp	= (tflags >> 11) & 31;
	    trap->tseen	= (tflags >> 10) & 1;
	    trap->once	= (tflags >> 9) & 1;
	    trap->madeby_u = (tflags >> 8) & 1;
	    
	    trap->vl.v_launch_otyp = mread16(mf);

	    trap->ntrap = first;
	    first = trap;
	}
	
	return first;
}


struct level *getlev(struct memfile *mf, xchar levnum, boolean ghostly)
{
	struct monst *mtmp;
	branch *br;
	int x, y;
	unsigned int lflags;
	struct level *lev;
	
	if (ghostly)
	    clear_id_mapping();

	/* Load the old fruit info.  We have to do it first, so the
	 * information is available when restoring the objects.
	 */
	if (ghostly) oldfruit = loadfruitchn(mf);

	/* for bones files, there is fruit chain data before the level data */
	mfmagic_check(mf, LEVEL_MAGIC);
	
	if (levels[levnum])
	    panic("Unsupported: trying to restore level %d which already exists.\n", levnum);
	lev = levels[levnum] = alloc_level(NULL);

	lev->z.dnum = mread8(mf);
	lev->z.dlevel = mread8(mf);
	mread(mf, lev->levname, sizeof(lev->levname));
	for (x = 0; x < COLNO; x++)
	    for (y = 0; y < ROWNO; y++)
		restore_location(mf, &lev->locations[x][y]);
	
	lev->lastmoves = mread32(mf);
	mread(mf, &lev->upstair, sizeof(stairway));
	mread(mf, &lev->dnstair, sizeof(stairway));
	mread(mf, &lev->upladder, sizeof(stairway));
	mread(mf, &lev->dnladder, sizeof(stairway));
	mread(mf, &lev->sstairs, sizeof(stairway));
	mread(mf, &lev->updest, sizeof(dest_area));
	mread(mf, &lev->dndest, sizeof(dest_area));
	
	lev->flags.nfountains = mread8(mf);
	lev->flags.nsinks = mread8(mf);
	
	lflags = mread32(mf);
	lev->flags.has_shop	= (lflags >> 31) & 1;
	lev->flags.has_vault	= (lflags >> 30) & 1;
	lev->flags.has_zoo	= (lflags >> 29) & 1;
	lev->flags.has_court	= (lflags >> 28) & 1;
	lev->flags.has_morgue	= (lflags >> 27) & 1;
	lev->flags.has_beehive = (lflags >> 26) & 1;
	lev->flags.has_barracks = (lflags >> 25) & 1;
	lev->flags.has_temple	= (lflags >> 24) & 1;
	lev->flags.has_swamp	= (lflags >> 23) & 1;
	lev->flags.noteleport	= (lflags >> 22) & 1;
	lev->flags.hardfloor	= (lflags >> 21) & 1;
	lev->flags.nommap	= (lflags >> 20) & 1;
	lev->flags.hero_memory = (lflags >> 19) & 1;
	lev->flags.shortsighted = (lflags >> 18) & 1;
	lev->flags.graveyard	= (lflags >> 17) & 1;
	lev->flags.is_maze_lev = (lflags >> 16) & 1;
	lev->flags.is_cavernous_lev = (lflags >> 15) & 1;
	lev->flags.arboreal	= (lflags >> 14) & 1;
	lev->flags.forgotten	= (lflags >> 13) & 1;
	
	mread(mf, lev->doors, sizeof(lev->doors));
	rest_rooms(mf, lev);	/* No joke :-) */
	if (lev->nroom)
	    lev->doorindex = lev->rooms[lev->nroom - 1].fdoor +
	                lev->rooms[lev->nroom - 1].doorct;
	else
	    lev->doorindex = 0;

	restore_timers(mf, lev, RANGE_LEVEL, ghostly, moves - lev->lastmoves);
	restore_light_sources(mf, lev);
	lev->monlist = restmonchn(mf, lev, ghostly);

	if (ghostly) {
	    struct monst *mtmp2;

	    for (mtmp = lev->monlist; mtmp; mtmp = mtmp2) {
		mtmp2 = mtmp->nmon;
		/* reset peaceful/malign relative to new character */
		if (!mtmp->isshk)
			/* shopkeepers will reset based on name */
			mtmp->mpeaceful = peace_minded(mtmp->data);
		set_malign(mtmp);

		/* update shape-changers in case protection against
		   them is different now than when the level was saved */
		restore_cham(mtmp);
	    }
	}

	rest_worm(mf, lev);	/* restore worm information */
	lev->lev_traps = restore_traps(mf);
	lev->objlist = restobjchn(mf, lev, ghostly, FALSE);
	find_lev_obj(lev);
	/* restobjchn()'s `frozen' argument probably ought to be a callback
	   routine so that we can check for objects being buried under ice */
	lev->buriedobjlist = restobjchn(mf, lev, ghostly, FALSE);
	lev->billobjs = restobjchn(mf, lev, ghostly, FALSE);
	rest_engravings(mf, lev);

	/* reset level->monsters for new level */
	for (x = 0; x < COLNO; x++)
	    for (y = 0; y < ROWNO; y++)
		lev->monsters[x][y] = NULL;
	for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon) {
	    if (mtmp->isshk)
		set_residency(mtmp, FALSE);
	    place_monster(mtmp, mtmp->mx, mtmp->my);
	    if (mtmp->wormno) place_wsegs(mtmp);
	}
	restdamage(mf, lev, ghostly);

	rest_regions(mf, lev, ghostly);
	if (ghostly) {
	    /* assert(lev->z == u.uz); */
	    
	    /* Now get rid of all the temp fruits... */
	    freefruitchn(oldfruit),  oldfruit = 0;

	    if (levnum > ledger_no(&medusa_level) &&
		levnum < ledger_no(&stronghold_level) && lev->dnstair.sx == 0) {
		coord cc;

		mazexy(lev, &cc);
		lev->dnstair.sx = cc.x;
		lev->dnstair.sy = cc.y;
		lev->locations[cc.x][cc.y].typ = STAIRS;
	    }

	    br = Is_branchlev(&lev->z);
	    if (br && lev->z.dlevel == 1) {
		d_level ltmp;

		if (on_level(&lev->z, &br->end1))
		    assign_level(&ltmp, &br->end2);
		else
		    assign_level(&ltmp, &br->end1);

		switch(br->type) {
		case BR_STAIR:
		case BR_NO_END1:
		case BR_NO_END2: /* OK to assign to sstairs if it's not used */
		    assign_level(&lev->sstairs.tolev, &ltmp);
		    break;		
		case BR_PORTAL: /* max of 1 portal per level */
		    {
			struct trap *ttmp;
			for (ttmp = lev->lev_traps; ttmp; ttmp = ttmp->ntrap)
			    if (ttmp->ttyp == MAGIC_PORTAL)
				break;
			if (!ttmp) panic("getlev: need portal but none found");
			assign_level(&ttmp->dst, &ltmp);
		    }
		    break;
		}
	    } else if (!br) {
		/* Remove any dangling portals. */
		struct trap *ttmp;
		for (ttmp = lev->lev_traps; ttmp; ttmp = ttmp->ntrap)
		    if (ttmp->ttyp == MAGIC_PORTAL) {
			deltrap(ttmp);
			break; /* max of 1 portal/level */
		    }
	    }
	}

	/* must come after all mons & objs are restored */
	relink_timers(ghostly, lev);
	relink_light_sources(ghostly, lev);
	reset_oattached_mids(ghostly, lev);

	if (ghostly)
	    clear_id_mapping();
	
	return lev;
}


/* Clear all structures for object and monster ID mapping. */
static void clear_id_mapping(void)
{
    struct bucket *curr;

    while ((curr = id_map) != 0) {
	id_map = curr->next;
	free(curr);
    }
    n_ids_mapped = 0;
}

/* Add a mapping to the ID map. */
static void add_id_mapping(unsigned gid, unsigned nid)
{
    int idx;

    idx = n_ids_mapped % N_PER_BUCKET;
    /* idx is zero on first time through, as well as when a new bucket is */
    /* needed */
    if (idx == 0) {
	struct bucket *gnu = malloc(sizeof(struct bucket));
	gnu->next = id_map;
	id_map = gnu;
    }

    id_map->map[idx].gid = gid;
    id_map->map[idx].nid = nid;
    n_ids_mapped++;
}

/*
 * Global routine to look up a mapping.  If found, return TRUE and fill
 * in the new ID value.  Otherwise, return false and return -1 in the new
 * ID.
 */
boolean lookup_id_mapping(unsigned gid, unsigned *nidp)
{
    int i;
    struct bucket *curr;

    if (n_ids_mapped)
	for (curr = id_map; curr; curr = curr->next) {
	    /* first bucket might not be totally full */
	    if (curr == id_map) {
		i = n_ids_mapped % N_PER_BUCKET;
		if (i == 0) i = N_PER_BUCKET;
	    } else
		i = N_PER_BUCKET;

	    while (--i >= 0)
		if (gid == curr->map[i].gid) {
		    *nidp = curr->map[i].nid;
		    return TRUE;
		}
	}

    return FALSE;
}

static void reset_oattached_mids(boolean ghostly, struct level *lev)
{
    struct obj *otmp;
    unsigned oldid, nid;
    for (otmp = lev->objlist; otmp; otmp = otmp->nobj) {
	if (ghostly && otmp->oattached == OATTACHED_MONST && otmp->oxlth) {
	    struct monst *mtmp = (struct monst *)otmp->oextra;

	    mtmp->m_id = 0;
	    mtmp->mpeaceful = mtmp->mtame = 0;	/* pet's owner died! */
	}
	if (ghostly && otmp->oattached == OATTACHED_M_ID) {
	    memcpy(&oldid, (void *)otmp->oextra, sizeof(oldid));
	    if (lookup_id_mapping(oldid, &nid))
		memcpy(otmp->oextra, (void *)&nid, sizeof(nid));
	    else
		otmp->oattached = OATTACHED_NOTHING;
	}
    }
}


/*restore.c*/
