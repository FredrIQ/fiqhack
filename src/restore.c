/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

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
	mread(mf, &cnt, sizeof(int));
	for (; cnt > 0; cnt--) {

	    tmplev = malloc(sizeof(s_level));
	    mread(mf, tmplev, sizeof(s_level));
	    if (!sp_levchn) sp_levchn = tmplev;
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

	mread(mf, &counter, sizeof(counter));
	if (!counter)
	    return;
	tmp_dam = malloc(sizeof(struct damage));
	while (--counter >= 0) {
	    char damaged_shops[5], *shp = NULL;

	    mread(mf, tmp_dam, sizeof(*tmp_dam));
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
	int xl;

	while (1) {
		mread(mf, &xl, sizeof(xl));
		if (xl == -1) break;
		otmp = newobj(xl);
		if (!first) first = otmp;
		else otmp2->nobj = otmp;
		mread(mf, otmp, (unsigned) xl + sizeof(struct obj));
		otmp->olev = lev;
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
	struct monst *mtmp, *mtmp2 = 0;
	struct monst *first = NULL;
	int xl;
	struct permonst *monbegin;
	boolean moved;

	/* get the original base address */
	mread(mf, &monbegin, sizeof(monbegin));
	moved = (monbegin != mons);

	while (1) {
		mread(mf, &xl, sizeof(xl));
		if (xl == -1) break;
		mtmp = newmonst(xl);
		if (!first) first = mtmp;
		else mtmp2->nmon = mtmp;
		mread(mf, mtmp, (unsigned) xl + sizeof(struct monst));
		mtmp->dlevel = lev;
		if (ghostly) {
			unsigned nid = flags.ident++;
			add_id_mapping(mtmp->m_id, nid);
			mtmp->m_id = nid;
		}
		if (moved && mtmp->data) {
			int offset = mtmp->data - monbegin;	/*(ptrdiff_t)*/
			mtmp->data = mons + offset;  /* new permonst location */
		}
		if (ghostly) {
			int mndx = monsndx(mtmp->data);
			if (propagate(mndx, TRUE, ghostly) == 0) {
				/* cookie to trigger purge in getbones() */
				mtmp->mhpmax = DEFUNCT_MONSTER;	
			}
		}
		if (mtmp->minvent) {
			struct obj *obj;
			mtmp->minvent = restobjchn(mf, lev, ghostly, FALSE);
			/* restore monster back pointer */
			for (obj = mtmp->minvent; obj; obj = obj->nobj)
				obj->ocarry = mtmp;
		}
		if (mtmp->mw) {
			struct obj *obj;

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
	struct fruit *flist, *fnext;

	flist = 0;
	while (fnext = newfruit(),
	       mread(mf, fnext, sizeof *fnext),
	       fnext->fid != 0) {
		fnext->nextf = flist;
		flist = fnext;
	}
	dealloc_fruit(fnext);
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


static void restgamestate(struct memfile *mf)
{
	struct obj *otmp;
	unsigned int stuckid = 0, steedid = 0, bookid = 0;
	struct monst *mtmp;
	struct level *lev;

	lev = levels[ledger_no(&u.uz)];

	/* this stuff comes after potential aborted restore attempts */
	restore_timers(mf, lev, RANGE_GLOBAL, FALSE, 0L);
	restore_light_sources(mf, lev);
	invent = restobjchn(mf, lev, FALSE, FALSE);
	migrating_objs = restobjchn(mf, lev, FALSE, FALSE);
	migrating_mons = restmonchn(mf, lev, FALSE);
	mread(mf, mvitals, sizeof(mvitals));

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

	mread(mf, &quest_status, sizeof(struct q_score));
	mread(mf, spl_book, sizeof(struct spell) * (MAXSPELL + 1));
	restore_artifacts(mf);
	restore_oracles(mf);
	if (u.ustuck)
		mread(mf, &stuckid, sizeof (stuckid));
	if (u.usteed)
		mread(mf, &steedid, sizeof (steedid));
	mread(mf, pl_character, sizeof pl_character);

	mread(mf, pl_fruit, sizeof pl_fruit);
	mread(mf, &current_fruit, sizeof current_fruit);
	freefruitchn(ffruit);	/* clean up fruit(s) made by initoptions() */
	ffruit = loadfruitchn(mf);

	restnames(mf);
	restore_waterlevel(mf);
	mread(mf, &lastinvnr, sizeof(lastinvnr));
	restore_mt_state(mf);
	restore_track(mf);
	restore_food(mf);
	mread(mf, &bookid, sizeof(bookid));
	if (bookid)
	    book = find_oid(bookid);
	mread(mf, &multi, sizeof(multi));
	
	/* must come after all mons & objs are restored */
	relink_timers(FALSE, lev);
	relink_light_sources(FALSE, lev);
	
	if (stuckid) {
		for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
			if (mtmp->m_id == stuckid) break;
		if (!mtmp) panic("Cannot find the monster ustuck.");
		u.ustuck = mtmp;
	}
	if (steedid) {
		for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
			if (mtmp->m_id == steedid) break;
		if (!mtmp) panic("Cannot find the monster usteed.");
		u.usteed = mtmp;
		remove_monster(mtmp->mx, mtmp->my);
	}
}


int dorecover(struct memfile *mf)
{
	int count;
	xchar ltmp;
	struct obj *otmp;
	
	level = NULL; /* level restore must not use this pointer */
	
	if (!uptodate(mf, NULL))
	    return 0;
	
	mread(mf, &flags, sizeof(struct flag));
	flags.bypasses = 0;	/* never use the saved value of bypasses */

	mread(mf, &u, sizeof(struct you));
	role_init();	/* Reset the initial role, race, gender, and alignment */
	mread(mf, &youmonst, sizeof(youmonst));
	set_uasmon(); /* fix up youmonst.data */
	mread(mf, &moves, sizeof(moves));
	
	/* restore dungeon */
	restore_dungeon(mf);
	restlevchn(mf);
	
	/* restore levels */
	mread(mf, &count, sizeof(count));
	for ( ; count; count--) {
	    mread(mf, &ltmp, sizeof ltmp);
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

struct level *getlev(struct memfile *mf, xchar levnum, boolean ghostly)
{
	struct trap *trap;
	struct monst *mtmp;
	branch *br;
	xchar dlvl;
	int x, y;
	struct level *lev;
	
	if (ghostly)
	    clear_id_mapping();

	/* Load the old fruit info.  We have to do it first, so the
	 * information is available when restoring the objects.
	 */
	if (ghostly) oldfruit = loadfruitchn(mf);

	/* CHECK:  This may prevent restoration */
	mread(mf, &dlvl, sizeof(dlvl));
	if (levnum && dlvl != levnum) {
	    char trickbuf[BUFSZ];
	    sprintf(trickbuf, "This is level %d, not %d!", dlvl, levnum);
	    if (wizard)
		pline(trickbuf);
	    trickery(trickbuf);
	}
	
	if (levels[dlvl])
	    panic("Unsupported: trying to restore level %d which already exists.\n", dlvl);
	lev = levels[dlvl] = alloc_level(NULL);

	mread(mf, &lev->z,sizeof(lev->z));
	mread(mf, lev->levname, sizeof(lev->levname));
	mread(mf, lev->locations, sizeof(lev->locations));
	mread(mf, &lev->lastmoves, sizeof(lev->lastmoves));
	mread(mf, &lev->upstair, sizeof(stairway));
	mread(mf, &lev->dnstair, sizeof(stairway));
	mread(mf, &lev->upladder, sizeof(stairway));
	mread(mf, &lev->dnladder, sizeof(stairway));
	mread(mf, &lev->sstairs, sizeof(stairway));
	mread(mf, &lev->updest, sizeof(dest_area));
	mread(mf, &lev->dndest, sizeof(dest_area));
	mread(mf, &lev->flags, sizeof(lev->flags));
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
	lev->lev_traps = 0;
	while (trap = newtrap(),
	       mread(mf, trap, sizeof(struct trap)),
	       trap->tx != 0) {
		trap->ntrap = lev->lev_traps;
		lev->lev_traps = trap;
	}
	dealloc_trap(trap);
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

void mread(struct memfile *mf, void *buf, unsigned int len)
{
	int rlen = min(len, mf->len - mf->pos);

	memcpy(buf, &mf->buf[mf->pos], rlen);
	mf->pos += rlen;
	if ((unsigned)rlen != len){
		pline("Read %d instead of %u bytes.", rlen, len);
		if (restoring)
			raw_print("Error restoring old game.\n");
		
		panic("Error reading level file.");
	}
}


/*restore.c*/
