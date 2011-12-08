/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include "quest.h"

#if !defined(LSC) && !defined(O_WRONLY)
#include <fcntl.h>
#endif

static void savelevchn(struct memfile *mf, int);
static void savedamage(struct memfile *mf, struct level *lev, int mode);
static void saveobjchn(struct memfile *mf, struct obj *,int);
static void savemonchn(struct memfile *mf, struct monst *,int);
static void savetrapchn(struct memfile *mf, struct trap *,int);
static void savegamestate(struct memfile *mf, int);


int dosave(void)
{
	if (yn("Really save?") == 'n') {
		if (multi > 0) nomul(0, NULL);
	} else {
		pline("Saving...");
		if (dosave0(FALSE)) {
			program_state.something_worth_saving = 0;
			u.uhp = -1;		/* universal game's over indicator */
			terminate();
		} else doredraw();
	}
	return 0;
}


/* returns 1 if save successful */
int dosave0(boolean emergency)
{
	int fd;
	struct memfile mf = {NULL, 0, 0};

	fd = logfile;
	
	log_finish(LS_SAVED);
	vision_recalc(2);	/* shut down vision to prevent problems
				   in the event of an impossible() call */
	
	savegame(&mf);
	store_mf(fd, &mf);
	
	freedynamicdata();

	return TRUE;
}


void savegame(struct memfile *mf)
{
	int count = 0;
	xchar ltmp;
	
	store_version(mf);
	
	/* Place flags, player info & moves at the beginning of the save.
	 * This makes it possible to read them in nh_get_savegame_status without
	 * parsing all the dungeon and level data */
	mwrite(mf, &flags, sizeof(struct flag));
	mwrite(mf, &u, sizeof(struct you));
	mwrite(mf, &youmonst, sizeof(youmonst));
	mwrite(mf, &moves, sizeof moves);
	
	/* store dungeon layout */
	save_dungeon(mf, TRUE, FALSE);
	savelevchn(mf, WRITE_SAVE);
	
	/* store levels */
	for (ltmp = 1; ltmp <= maxledgerno(); ltmp++)
	    if (levels[ltmp])
		count++;
	mwrite(mf, &count, sizeof(count));
	for (ltmp = 1; ltmp <= maxledgerno(); ltmp++) {
		if (!levels[ltmp])
		    continue;
		mwrite(mf, &ltmp, sizeof ltmp); /* level number*/
		savelev(mf, ltmp, WRITE_SAVE); /* actual level*/
	}
	savegamestate(mf, WRITE_SAVE);
}


static void savegamestate(struct memfile *mf, int mode)
{
	unsigned ustuck_id = (u.ustuck ? u.ustuck->m_id : 0);
	unsigned usteed_id = (u.usteed ? u.usteed->m_id : 0);
	unsigned book_id;

	/* must come before migrating_objs and migrating_mons are freed */
	save_timers(mf, level, mode, RANGE_GLOBAL);
	save_light_sources(mf, level, mode, RANGE_GLOBAL);

	saveobjchn(mf, invent, mode);
	saveobjchn(mf, migrating_objs, mode);
	savemonchn(mf, migrating_mons, mode);
	if (release_data(mode)) {
	    invent = 0;
	    migrating_objs = 0;
	    migrating_mons = 0;
	}
	mwrite(mf, mvitals, sizeof(mvitals));

	mwrite(mf, &quest_status, sizeof(struct q_score));
	mwrite(mf, spl_book, sizeof(struct spell) * (MAXSPELL + 1));
	save_artifacts(mf);
	save_oracles(mf, mode);
	if (ustuck_id)
	    mwrite(mf, &ustuck_id, sizeof ustuck_id);
	if (usteed_id)
	    mwrite(mf, &usteed_id, sizeof usteed_id);
	mwrite(mf, pl_character, sizeof pl_character);
	mwrite(mf, pl_fruit, sizeof pl_fruit);
	mwrite(mf, &current_fruit, sizeof current_fruit);
	savefruitchn(mf, mode);
	savenames(mf, mode);
	save_waterlevel(mf, mode);
	mwrite(mf, &lastinvnr, sizeof(lastinvnr));
	save_mt_state(mf);
	save_track(mf);
	save_food(mf);
	book_id = book ? book->o_id : 0;
	mwrite(mf, &book_id, sizeof(book_id));
	mwrite(mf, &multi, sizeof(multi));
	save_rndmonst_state(mf);
}


void savelev(struct memfile *mf, xchar levnum, int mode)
{
	struct level *lev = levels[levnum];
	
	/* if we're tearing down the current level without saving anything
	   (which happens upon entrance to the endgame or after an aborted
	   restore attempt) then we don't want to do any actual I/O */
	if (mode == FREE_SAVE) goto skip_lots;
	if (iflags.purge_monsters) {
		/* purge any dead monsters (necessary if we're starting
		 * a panic save rather than a normal one, or sometimes
		 * when changing levels without taking time -- e.g.
		 * create statue trap then immediately level teleport) */
		dmonsfree(lev);
	}

	mwrite(mf, &levnum, sizeof(levnum));
	mwrite(mf, &lev->z, sizeof(lev->z));
	mwrite(mf, lev->levname, sizeof(lev->levname));
	mwrite(mf, lev->locations, sizeof(lev->locations));
	mwrite(mf, &lev->lastmoves, sizeof(lev->lastmoves));
	mwrite(mf, &lev->upstair, sizeof(stairway));
	mwrite(mf, &lev->dnstair, sizeof(stairway));
	mwrite(mf, &lev->upladder, sizeof(stairway));
	mwrite(mf, &lev->dnladder, sizeof(stairway));
	mwrite(mf, &lev->sstairs, sizeof(stairway));
	mwrite(mf, &lev->updest, sizeof(dest_area));
	mwrite(mf, &lev->dndest, sizeof(dest_area));
	mwrite(mf, &lev->flags, sizeof(lev->flags));
	mwrite(mf, lev->doors, sizeof(lev->doors));
	save_rooms(mf, lev);	/* no dynamic memory to reclaim */

	/* from here on out, saving also involves allocated memory cleanup */
skip_lots:
	/* must be saved before mons, objs, and buried objs */
	save_timers(mf, lev, mode, RANGE_LEVEL);
	save_light_sources(mf, lev, mode, RANGE_LEVEL);

	savemonchn(mf, lev->monlist, mode);
	save_worm(mf, lev, mode);	/* save worm information */
	savetrapchn(mf, lev->lev_traps, mode);
	saveobjchn(mf, lev->objlist, mode);
	saveobjchn(mf, lev->buriedobjlist, mode);
	saveobjchn(mf, lev->billobjs, mode);
	if (release_data(mode)) {
	    lev->monlist = NULL;
	    lev->lev_traps = NULL;
	    lev->objlist = NULL;
	    lev->buriedobjlist = NULL;
	    lev->billobjs = NULL;
	}
	save_engravings(mf, lev, mode);
	savedamage(mf, lev, mode);
	save_regions(mf, lev, mode);
	
	if (release_data(mode)) {
	    free(lev);
	    levels[levnum] = NULL;
	}
}


void mwrite(struct memfile *mf, const void *buf, unsigned int num)
{
	boolean do_realloc = FALSE;
	while (mf->len < mf->pos + num) {
	    mf->len += 4096;
	    do_realloc = TRUE;
	}
	
	if (do_realloc)
	    mf->buf = realloc(mf->buf, mf->len);
	memcpy(&mf->buf[mf->pos], buf, num);
	mf->pos += num;
}


void store_mf(int fd, struct memfile *mf)
{
	int len, left, ret;
	
	len = left = mf->pos;
	while (left) {
	    ret = write(fd, &mf->buf[len - left], left);
	    if (ret == -1) /* error */
		goto out;
	    left -= ret;
	}

out:
	free(mf->buf);
	mf->buf = NULL;
	mf->pos = mf->len = 0;
}


static void savelevchn(struct memfile *mf, int mode)
{
	s_level	*tmplev, *tmplev2;
	int cnt = 0;

	for (tmplev = sp_levchn; tmplev; tmplev = tmplev->next) cnt++;
	if (perform_mwrite(mode))
	    mwrite(mf, &cnt, sizeof(int));

	for (tmplev = sp_levchn; tmplev; tmplev = tmplev2) {
	    tmplev2 = tmplev->next;
	    if (perform_mwrite(mode))
		mwrite(mf, tmplev, sizeof(s_level));
	    if (release_data(mode))
		free(tmplev);
	}
	if (release_data(mode))
	    sp_levchn = 0;
}


static void savedamage(struct memfile *mf, struct level *lev, int mode)
{
	struct damage *damageptr, *tmp_dam;
	unsigned int xl = 0;

	damageptr = lev->damagelist;
	for (tmp_dam = damageptr; tmp_dam; tmp_dam = tmp_dam->next)
	    xl++;
	if (perform_mwrite(mode))
	    mwrite(mf, &xl, sizeof(xl));

	while (xl--) {
	    if (perform_mwrite(mode))
		mwrite(mf, damageptr, sizeof(*damageptr));
	    tmp_dam = damageptr;
	    damageptr = damageptr->next;
	    if (release_data(mode))
		free(tmp_dam);
	}
	if (release_data(mode))
	    lev->damagelist = NULL;
}


static void saveobjchn(struct memfile *mf, struct obj *otmp, int mode)
{
	struct obj *otmp2;
	unsigned int xl;
	int minusone = -1;

	while (otmp) {
	    otmp2 = otmp->nobj;
	    if (perform_mwrite(mode)) {
		xl = otmp->oxlth + otmp->onamelth;
		mwrite(mf, &xl, sizeof(int));
		mwrite(mf, otmp, xl + sizeof(struct obj));
	    }
	    if (Has_contents(otmp))
		saveobjchn(mf, otmp->cobj,mode);
	    if (release_data(mode)) {
		otmp->where = OBJ_FREE;	/* set to free so dealloc will work */
		otmp->timed = 0;	/* not timed any more */
		otmp->lamplit = 0;	/* caller handled lights */
		dealloc_obj(otmp);
	    }
	    otmp = otmp2;
	}
	if (perform_mwrite(mode))
	    mwrite(mf, &minusone, sizeof(int));
}

static void savemonchn(struct memfile *mf, struct monst *mtmp, int mode)
{
	struct monst *mtmp2;
	unsigned int xl;
	int minusone = -1;
	const struct permonst *monbegin = &mons[0];

	if (perform_mwrite(mode))
	    mwrite(mf, &monbegin, sizeof(monbegin));

	while (mtmp) {
	    mtmp2 = mtmp->nmon;
	    if (perform_mwrite(mode)) {
		xl = mtmp->mxlth + mtmp->mnamelth;
		mwrite(mf, &xl, sizeof(int));
		mwrite(mf, mtmp, xl + sizeof(struct monst));
	    }
	    if (mtmp->minvent)
		saveobjchn(mf,mtmp->minvent,mode);
	    if (release_data(mode))
		dealloc_monst(mtmp);
	    mtmp = mtmp2;
	}
	if (perform_mwrite(mode))
	    mwrite(mf, &minusone, sizeof(int));
}

static void savetrapchn(struct memfile *mf, struct trap *trap, int mode)
{
	struct trap *trap2;

	while (trap) {
	    trap2 = trap->ntrap;
	    if (perform_mwrite(mode))
		mwrite(mf, trap, sizeof(struct trap));
	    if (release_data(mode))
		dealloc_trap(trap);
	    trap = trap2;
	}
	if (perform_mwrite(mode))
	    mwrite(mf, (void*)nul, sizeof(struct trap));
}

/* save all the fruit names and ID's; this is used only in saving whole games
 * (not levels) and in saving bones levels.  When saving a bones level,
 * we only want to save the fruits which exist on the bones level; the bones
 * level routine marks nonexistent fruits by making the fid negative.
 */
void savefruitchn(struct memfile *mf, int mode)
{
	struct fruit *f2, *f1;

	f1 = ffruit;
	while (f1) {
	    f2 = f1->nextf;
	    if (f1->fid >= 0 && perform_mwrite(mode))
		mwrite(mf, f1, sizeof(struct fruit));
	    if (release_data(mode))
		dealloc_fruit(f1);
	    f1 = f2;
	}
	if (perform_mwrite(mode))
	    mwrite(mf, (void *)nul, sizeof(struct fruit));
	if (release_data(mode))
	    ffruit = 0;
}

/* also called by prscore(); this probably belongs in dungeon.c... */
void free_dungeons(void)
{
	savelevchn(0, FREE_SAVE);
	save_dungeon(0, FALSE, TRUE);
	return;
}

void freedynamicdata(void)
{
	int i;
	struct level *lev;
	
	if (!objects)
	    return; /* no cleanup necessary */
	
	unload_qtlist();
	free_invbuf();	/* let_to_name (invent.c) */
	free_youbuf();	/* You_buf,&c (pline.c) */
	tmp_at(DISP_FREEMEM, 0);	/* temporary display effects */
# define freeobjchn(X)	(saveobjchn(0, X, FREE_SAVE),  X = 0)
# define freemonchn(X)	(savemonchn(0, X, FREE_SAVE),  X = 0)
# define freetrapchn(X)	(savetrapchn(0, X, FREE_SAVE), X = 0)
# define freefruitchn()	 savefruitchn(0, FREE_SAVE)
# define freenames()	 savenames(0, FREE_SAVE)
# define free_oracles()	save_oracles(0, FREE_SAVE)
# define free_waterlevel() save_waterlevel(0, FREE_SAVE)
# define free_worm(lev)	 save_worm(0, lev, FREE_SAVE)
# define free_timers(lev, R)	 save_timers(0, lev, FREE_SAVE, R)
# define free_light_sources(lev, R) save_light_sources(0, lev, FREE_SAVE, R);
# define free_engravings(lev) save_engravings(0, lev, FREE_SAVE)
# define freedamage(lev)	 savedamage(0, lev, FREE_SAVE)
# define free_animals()	 mon_animal_list(FALSE)

	for (i = 0; i < MAXLINFO; i++) {
	    lev = levels[i];
	    if (!lev) continue;
	    
	    /* level-specific data */
	    dmonsfree(lev);	/* release dead monsters */
	    free_timers(lev, RANGE_LEVEL);
	    free_light_sources(lev, RANGE_LEVEL);
	    free_timers(lev, RANGE_GLOBAL);
	    free_light_sources(lev, RANGE_GLOBAL);
	    freemonchn(lev->monlist);
	    free_worm(lev);		/* release worm segment information */
	    freetrapchn(lev->lev_traps);
	    freeobjchn(lev->objlist);
	    freeobjchn(lev->buriedobjlist);
	    freeobjchn(lev->billobjs);
	    free_engravings(lev);
	    freedamage(lev);
	    
	    free(lev);
	}

	/* game-state data */
	freeobjchn(invent);
	freeobjchn(migrating_objs);
	freemonchn(migrating_mons);
	freemonchn(mydogs);		/* ascension or dungeon escape */
     /* freelevchn();	[folded into free_dungeons()] */
	free_animals();
	free_oracles();
	freefruitchn();
	freenames();
	free_waterlevel();
	free_dungeons();

	if (iflags.ap_rules) {
	    free(iflags.ap_rules->rules);
	    iflags.ap_rules->rules = NULL;
	    free(iflags.ap_rules);
	}
	iflags.ap_rules = NULL;
	free(artilist);
	free(objects);
	objects = NULL;
	artilist = NULL;
	
	if (active_birth_options)
	    free_optlist(active_birth_options);
	active_birth_options = NULL;
	
	return;
}

/*save.c*/
