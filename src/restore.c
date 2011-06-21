/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"

#ifdef USE_TILES
extern void substitute_tiles(d_level *);       /* from tile.c */
#endif

static void find_lev_obj(void);
static void restlevchn(int);
static void restdamage(int,boolean);
static struct obj *restobjchn(int,boolean,boolean);
static struct monst *restmonchn(int,boolean);
static struct fruit *loadfruitchn(int);
static void freefruitchn(struct fruit *);
static void ghostfruit(struct obj *);
static boolean restgamestate(int, unsigned int *, unsigned int *);
static void restlevelstate(unsigned int, unsigned int);
static int restlevelfile(xchar);
static void reset_oattached_mids(boolean);

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
static long omoves;

#define Is_IceBox(o) ((o)->otyp == ICE_BOX ? TRUE : FALSE)

/* Recalculate level.objects[x][y], since this info was not saved. */
static void find_lev_obj(void)
{
	struct obj *fobjtmp = NULL;
	struct obj *otmp;
	int x,y;

	for (x=0; x<COLNO; x++) for(y=0; y<ROWNO; y++)
		level.objects[x][y] = NULL;

	/*
	 * Reverse the entire fobj chain, which is necessary so that we can
	 * place the objects in the proper order.  Make all obj in chain
	 * OBJ_FREE so place_object will work correctly.
	 */
	while ((otmp = fobj) != 0) {
		fobj = otmp->nobj;
		otmp->nobj = fobjtmp;
		otmp->where = OBJ_FREE;
		fobjtmp = otmp;
	}
	/* fobj should now be empty */

	/* Set level.objects (as well as reversing the chain back again) */
	while ((otmp = fobjtmp) != 0) {
		fobjtmp = otmp->nobj;
		place_object(otmp, otmp->ox, otmp->oy);
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
#ifndef GOLDOBJ
	    if (otmp->oclass == COIN_CLASS) {
		/* in_use gold is created by some menu operations */
		if (!otmp->in_use) {
		    impossible("inven_inuse: !in_use gold in inventory");
		}
		extract_nobj(otmp, &invent);
		otmp->in_use = FALSE;
		dealloc_obj(otmp);
	    } else
#endif /* GOLDOBJ */
	    if (otmp->in_use) {
		if (!quietly) pline("Finishing off %s...", xname(otmp));
		useup(otmp);
	    }
	}
}

static void restlevchn(int fd)
{
	int cnt;
	s_level	*tmplev, *x;

	sp_levchn = NULL;
	mread(fd, &cnt, sizeof(int));
	for (; cnt > 0; cnt--) {

	    tmplev = malloc(sizeof(s_level));
	    mread(fd, tmplev, sizeof(s_level));
	    if (!sp_levchn) sp_levchn = tmplev;
	    else {

		for (x = sp_levchn; x->next; x = x->next);
		x->next = tmplev;
	    }
	    tmplev->next = NULL;
	}
}


static void restdamage(int fd, boolean ghostly)
{
	int counter;
	struct damage *tmp_dam;

	mread(fd, &counter, sizeof(counter));
	if (!counter)
	    return;
	tmp_dam = malloc(sizeof(struct damage));
	while (--counter >= 0) {
	    char damaged_shops[5], *shp = NULL;

	    mread(fd, tmp_dam, sizeof(*tmp_dam));
	    if (ghostly)
		tmp_dam->when += (monstermoves - omoves);
	    strcpy(damaged_shops,
		   in_rooms(tmp_dam->place.x, tmp_dam->place.y, SHOPBASE));
	    if (u.uz.dlevel) {
		/* when restoring, there are two passes over the current
		 * level.  the first time, u.uz isn't set, so neither is
		 * shop_keeper().  just wait and process the damage on
		 * the second pass.
		 */
		for (shp = damaged_shops; *shp; shp++) {
		    struct monst *shkp = shop_keeper(*shp);

		    if (shkp && inhishop(shkp) &&
			    repair_damage(shkp, tmp_dam, TRUE))
			break;
		}
	    }
	    if (!shp || !*shp) {
		tmp_dam->next = level.damagelist;
		level.damagelist = tmp_dam;
		tmp_dam = malloc(sizeof(*tmp_dam));
	    }
	}
	free(tmp_dam);
}


static struct obj *restobjchn(int fd, boolean ghostly, boolean frozen)
{
	struct obj *otmp, *otmp2 = 0;
	struct obj *first = NULL;
	int xl;

	while (1) {
		mread(fd, &xl, sizeof(xl));
		if (xl == -1) break;
		otmp = newobj(xl);
		if (!first) first = otmp;
		else otmp2->nobj = otmp;
		mread(fd, otmp,
					(unsigned) xl + sizeof(struct obj));
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
		    otmp->age = monstermoves - omoves + otmp->age;

		/* get contents of a container or statue */
		if (Has_contents(otmp)) {
		    struct obj *otmp3;
		    otmp->cobj = restobjchn(fd, ghostly, Is_IceBox(otmp));
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


static struct monst *restmonchn(int fd, boolean ghostly)
{
	struct monst *mtmp, *mtmp2 = 0;
	struct monst *first = NULL;
	int xl;
	struct permonst *monbegin;
	boolean moved;

	/* get the original base address */
	mread(fd, &monbegin, sizeof(monbegin));
	moved = (monbegin != mons);

	while (1) {
		mread(fd, &xl, sizeof(xl));
		if (xl == -1) break;
		mtmp = newmonst(xl);
		if (!first) first = mtmp;
		else mtmp2->nmon = mtmp;
		mread(fd, mtmp, (unsigned) xl + sizeof(struct monst));
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
			mtmp->minvent = restobjchn(fd, ghostly, FALSE);
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


static struct fruit *loadfruitchn(int fd)
{
	struct fruit *flist, *fnext;

	flist = 0;
	while (fnext = newfruit(),
	       mread(fd, fnext, sizeof *fnext),
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


static boolean restgamestate(int fd, unsigned int *stuckid, unsigned int *steedid)
{
	/* discover is actually flags.explore */
	boolean remember_discover = discover;
	struct obj *otmp;
	int uid;

	mread(fd, &uid, sizeof uid);
	if (uid != getuid()) {		/* strange ... */
	    /* for wizard mode, issue a reminder; for others, treat it
	       as an attempt to cheat and refuse to restore this file */
	    pline("Saved game was not yours.");
	    if (!wizard)
		return FALSE;
	}

	mread(fd, &flags, sizeof(struct flag));
	flags.bypasses = 0;	/* never use the saved value of bypasses */
	if (remember_discover) discover = remember_discover;

	role_init();	/* Reset the initial role, race, gender, and alignment */
	mread(fd, &u, sizeof(struct you));
	set_uasmon();
	if (u.uhp <= 0 && (!Upolyd || u.mh <= 0)) {
	    u.ux = u.uy = 0;	/* affects pline() [hence You()] */
	    You("were not healthy enough to survive restoration.");
	    /* wiz1_level.dlevel is used by mklev.c to see if lots of stuff is
	     * uninitialized, so we only have to set it and not the other stuff.
	     */
	    wiz1_level.dlevel = 0;
	    u.uz.dnum = 0;
	    u.uz.dlevel = 1;
	    return FALSE;
	}

	/* this stuff comes after potential aborted restore attempts */
	restore_timers(fd, RANGE_GLOBAL, FALSE, 0L);
	restore_light_sources(fd);
	invent = restobjchn(fd, FALSE, FALSE);
	migrating_objs = restobjchn(fd, FALSE, FALSE);
	migrating_mons = restmonchn(fd, FALSE);
	mread(fd, mvitals, sizeof(mvitals));

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

	restore_dungeon(fd);
	restlevchn(fd);
	mread(fd, &moves, sizeof moves);
	mread(fd, &monstermoves, sizeof monstermoves);
	mread(fd, &quest_status, sizeof(struct q_score));
	mread(fd, spl_book,
				sizeof(struct spell) * (MAXSPELL + 1));
	restore_artifacts(fd);
	restore_oracles(fd);
	if (u.ustuck)
		mread(fd, stuckid, sizeof (*stuckid));
	if (u.usteed)
		mread(fd, steedid, sizeof (*steedid));
	mread(fd, pl_character, sizeof pl_character);

	mread(fd, pl_fruit, sizeof pl_fruit);
	mread(fd, &current_fruit, sizeof current_fruit);
	freefruitchn(ffruit);	/* clean up fruit(s) made by initoptions() */
	ffruit = loadfruitchn(fd);

	restnames(fd);
	restore_waterlevel(fd);
	/* must come after all mons & objs are restored */
	relink_timers(FALSE);
	relink_light_sources(FALSE);
	return TRUE;
}

/* update game state pointers to those valid for the current level (so we
 * don't dereference a wild u.ustuck when saving the game state, for instance)
 */
static void restlevelstate(unsigned int stuckid, unsigned int steedid)
{
	struct monst *mtmp;

	if (stuckid) {
		for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
			if (mtmp->m_id == stuckid) break;
		if (!mtmp) panic("Cannot find the monster ustuck.");
		u.ustuck = mtmp;
	}
	if (steedid) {
		for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
			if (mtmp->m_id == steedid) break;
		if (!mtmp) panic("Cannot find the monster usteed.");
		u.usteed = mtmp;
		remove_monster(mtmp->mx, mtmp->my);
	}
}

static int restlevelfile(xchar ltmp)
{
	int nfd;
	char whynot[BUFSZ];

	nfd = create_levelfile(ltmp, whynot);
	if (nfd < 0) {
		/* BUG: should suppress any attempt to write a panic
		   save file if file creation is now failing... */
		panic("restlevelfile: %s", whynot);
	}

	bufon(nfd);
	savelev(nfd, ltmp, WRITE_SAVE | FREE_SAVE);
	bclose(nfd);
	return 2;
}

int dorecover(int fd)
{
	unsigned int stuckid = 0, steedid = 0;
	xchar ltmp;
	int rtmp;
	struct obj *otmp;

#ifdef STORE_PLNAME_IN_FILE
	mread(fd, plname, PL_NSIZ);
#endif

	restoring = TRUE;
	getlev(fd, 0, (xchar)0, FALSE);
	if (!restgamestate(fd, &stuckid, &steedid)) {
		display_nhwindow(NHW_MESSAGE, TRUE);
		savelev(-1, 0, FREE_SAVE);	/* discard current level */
		close(fd);
		delete_savefile();
		restoring = FALSE;
		return 0;
	}
	restlevelstate(stuckid, steedid);
#ifdef INSURANCE
	savestateinlock();
#endif
	rtmp = restlevelfile(ledger_no(&u.uz));
	if (rtmp < 2) return rtmp;  /* dorecover called recursively */

	/* these pointers won't be valid while we're processing the
	 * other levels, but they'll be reset again by restlevelstate()
	 * afterwards, and in the meantime at least u.usteed may mislead
	 * place_monster() on other levels
	 */
	u.ustuck = NULL;
	u.usteed = NULL;

	while (1) {
		if (read(fd, &ltmp, sizeof ltmp) != sizeof ltmp)
			break;
		getlev(fd, 0, ltmp, FALSE);
		rtmp = restlevelfile(ltmp);
		if (rtmp < 2) return rtmp;  /* dorecover called recursively */
	}

	lseek(fd, (off_t)0, 0);
	uptodate(fd, NULL);		/* skip version info */
#ifdef STORE_PLNAME_IN_FILE
	mread(fd, plname, PL_NSIZ);
#endif
	getlev(fd, 0, (xchar)0, FALSE);
	close(fd);

	if (!wizard && !discover)
		delete_savefile();
	if (Is_rogue_level(&u.uz))
		assign_rogue_graphics(TRUE);
#ifdef USE_TILES
	substitute_tiles(&u.uz);
#endif
	restlevelstate(stuckid, steedid);
	max_rank_sz(); /* to recompute mrank_sz (botl.c) */
	/* take care of iron ball & chain */
	for (otmp = fobj; otmp; otmp = otmp->nobj)
		if (otmp->owornmask)
			setworn(otmp, otmp->owornmask);

	/* in_use processing must be after:
	 *    + The inventory has been read so that freeinv() works.
	 *    + The current level has been restored so billing information
	 *	is available.
	 */
	inven_inuse(FALSE);

	load_qtlist();	/* re-load the quest text info */
	reset_attribute_clock();
	/* Set up the vision internals, after levl[] data is loaded */
	/* but before docrt().					    */
	vision_reset();
	vision_full_recalc = 1;	/* recompute vision (not saved) */

	run_timers();	/* expire all timers that have gone off while away */
	docrt();
	restoring = FALSE;
	clear_nhwindow(NHW_MESSAGE);
	program_state.something_worth_saving++;	/* useful data now exists */

	/* Success! */
	welcome(FALSE);
	return 1;
}

void trickery(char *reason)
{
	pline("Strange, this map is not as I remember it.");
	pline("Somebody is trying some trickery here...");
	pline("This game is void.");
	killer = reason;
	done(TRICKED);
}

void getlev(int fd, int pid, xchar lev, boolean ghostly)
{
	struct trap *trap;
	struct monst *mtmp;
	branch *br;
	int hpid;
	xchar dlvl;
	int x, y;

	if (ghostly)
	    clear_id_mapping();

	/* Load the old fruit info.  We have to do it first, so the
	 * information is available when restoring the objects.
	 */
	if (ghostly) oldfruit = loadfruitchn(fd);

	/* First some sanity checks */
	mread(fd, &hpid, sizeof(hpid));
/* CHECK:  This may prevent restoration */
	mread(fd, &dlvl, sizeof(dlvl));
	if ((pid && pid != hpid) || (lev && dlvl != lev)) {
	    char trickbuf[BUFSZ];

	    if (pid && pid != hpid)
		sprintf(trickbuf, "PID (%d) doesn't match saved PID (%d)!",
			hpid, pid);
	    else
		sprintf(trickbuf, "This is level %d, not %d!", dlvl, lev);
	    if (wizard)
		pline(trickbuf);
	    trickery(trickbuf);
	}

	mread(fd, levl, sizeof(levl));
	mread(fd, &omoves, sizeof(omoves));
	mread(fd, &upstair, sizeof(stairway));
	mread(fd, &dnstair, sizeof(stairway));
	mread(fd, &upladder, sizeof(stairway));
	mread(fd, &dnladder, sizeof(stairway));
	mread(fd, &sstairs, sizeof(stairway));
	mread(fd, &updest, sizeof(dest_area));
	mread(fd, &dndest, sizeof(dest_area));
	mread(fd, &level.flags, sizeof(level.flags));
	mread(fd, doors, sizeof(doors));
	rest_rooms(fd);		/* No joke :-) */
	if (nroom)
	    doorindex = rooms[nroom - 1].fdoor + rooms[nroom - 1].doorct;
	else
	    doorindex = 0;

	restore_timers(fd, RANGE_LEVEL, ghostly, monstermoves - omoves);
	restore_light_sources(fd);
	fmon = restmonchn(fd, ghostly);

	/* regenerate animals while on another level */
	if (u.uz.dlevel) {
	    struct monst *mtmp2;

	    for (mtmp = fmon; mtmp; mtmp = mtmp2) {
		mtmp2 = mtmp->nmon;
		if (ghostly) {
			/* reset peaceful/malign relative to new character */
			if (!mtmp->isshk)
				/* shopkeepers will reset based on name */
				mtmp->mpeaceful = peace_minded(mtmp->data);
			set_malign(mtmp);
		} else if (monstermoves > omoves)
			mon_catchup_elapsed_time(mtmp, monstermoves - omoves);

		/* update shape-changers in case protection against
		   them is different now than when the level was saved */
		restore_cham(mtmp);
	    }
	}

	rest_worm(fd);	/* restore worm information */
	ftrap = 0;
	while (trap = newtrap(),
	       mread(fd, trap, sizeof(struct trap)),
	       trap->tx != 0) {	/* need "!= 0" to work around DICE 3.0 bug */
		trap->ntrap = ftrap;
		ftrap = trap;
	}
	dealloc_trap(trap);
	fobj = restobjchn(fd, ghostly, FALSE);
	find_lev_obj();
	/* restobjchn()'s `frozen' argument probably ought to be a callback
	   routine so that we can check for objects being buried under ice */
	level.buriedobjlist = restobjchn(fd, ghostly, FALSE);
	billobjs = restobjchn(fd, ghostly, FALSE);
	rest_engravings(fd);

	/* reset level.monsters for new level */
	for (x = 0; x < COLNO; x++)
	    for (y = 0; y < ROWNO; y++)
		level.monsters[x][y] = NULL;
	for (mtmp = level.monlist; mtmp; mtmp = mtmp->nmon) {
	    if (mtmp->isshk)
		set_residency(mtmp, FALSE);
	    place_monster(mtmp, mtmp->mx, mtmp->my);
	    if (mtmp->wormno) place_wsegs(mtmp);
	}
	restdamage(fd, ghostly);

	rest_regions(fd, ghostly);
	if (ghostly) {
	    /* Now get rid of all the temp fruits... */
	    freefruitchn(oldfruit),  oldfruit = 0;

	    if (lev > ledger_no(&medusa_level) &&
			lev < ledger_no(&stronghold_level) && xdnstair == 0) {
		coord cc;

		mazexy(&cc);
		xdnstair = cc.x;
		ydnstair = cc.y;
		levl[cc.x][cc.y].typ = STAIRS;
	    }

	    br = Is_branchlev(&u.uz);
	    if (br && u.uz.dlevel == 1) {
		d_level ltmp;

		if (on_level(&u.uz, &br->end1))
		    assign_level(&ltmp, &br->end2);
		else
		    assign_level(&ltmp, &br->end1);

		switch(br->type) {
		case BR_STAIR:
		case BR_NO_END1:
		case BR_NO_END2: /* OK to assign to sstairs if it's not used */
		    assign_level(&sstairs.tolev, &ltmp);
		    break;		
		case BR_PORTAL: /* max of 1 portal per level */
		    {
			struct trap *ttmp;
			for (ttmp = ftrap; ttmp; ttmp = ttmp->ntrap)
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
		for (ttmp = ftrap; ttmp; ttmp = ttmp->ntrap)
		    if (ttmp->ttyp == MAGIC_PORTAL) {
			deltrap(ttmp);
			break; /* max of 1 portal/level */
		    }
	    }
	}

	/* must come after all mons & objs are restored */
	relink_timers(ghostly);
	relink_light_sources(ghostly);
	reset_oattached_mids(ghostly);

	if (ghostly)
	    clear_id_mapping();
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

static void reset_oattached_mids(boolean ghostly)
{
    struct obj *otmp;
    unsigned oldid, nid;
    for (otmp = fobj; otmp; otmp = otmp->nobj) {
	if (ghostly && otmp->oattached == OATTACHED_MONST && otmp->oxlth) {
	    struct monst *mtmp = (struct monst *)otmp->oextra;

	    mtmp->m_id = 0;
	    mtmp->mpeaceful = mtmp->mtame = 0;	/* pet's owner died! */
	}
	if (ghostly && otmp->oattached == OATTACHED_M_ID) {
	    memcpy(&oldid, (void *)otmp->oextra,
								sizeof(oldid));
	    if (lookup_id_mapping(oldid, &nid))
		memcpy(otmp->oextra, (void *)&nid,
								sizeof(nid));
	    else
		otmp->oattached = OATTACHED_NOTHING;
	}
    }
}

void mread(int fd, void * buf, unsigned int len)
{
	int rlen;

	rlen = read(fd, buf, (unsigned) len);
	if ((unsigned)rlen != len){
		pline("Read %d instead of %u bytes.", rlen, len);
		if (restoring) {
			close(fd);
			delete_savefile();
			raw_print("Error restoring old game.");
		}
		panic("Error reading level file.");
	}
}

/*restore.c*/
