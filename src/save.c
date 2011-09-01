/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include "quest.h"

#if !defined(LSC) && !defined(O_WRONLY)
#include <fcntl.h>
#endif

extern int logfile;

static void savelevchn(int,int);
static void savedamage(int fd, struct level *lev, int mode);
static void saveobjchn(int,struct obj *,int);
static void savemonchn(int,struct monst *,int);
static void savetrapchn(int,struct trap *,int);
static void savegamestate(int,int);


int dosave(void)
{
	if (yn("Really save?") == 'n') {
		if (multi > 0) nomul(0);
	} else {
		pline("Saving...");
		if (dosave0(FALSE)) {
			program_state.something_worth_saving = 0;
			u.uhp = -1;		/* universal game's over indicator */
			/* make sure they see the Saving message */
			win_pause(P_MESSAGE);
			terminate();
		} else doredraw();
	}
	return 0;
}


/* returns 1 if save successful */
int dosave0(boolean emergency)
{
	int fd, count = 0;
	xchar ltmp;

	fd = logfile;
	
	log_finish(LS_SAVED);
	vision_recalc(2);	/* shut down vision to prevent problems
				   in the event of an impossible() call */
	store_version(fd);
	
	/* store dungeon layout */
	save_dungeon(fd, TRUE, FALSE);
	savelevchn(fd, WRITE_SAVE);
	
	/* store levels */
	for (ltmp = 1; ltmp <= maxledgerno(); ltmp++)
	    if (levels[ltmp])
		count++;
	bwrite(fd, &count, sizeof(count));
	for (ltmp = 1; ltmp <= maxledgerno(); ltmp++) {
		if (!levels[ltmp])
		    continue;
		bwrite(fd, &ltmp, sizeof ltmp); /* level number*/
		savelev(fd, ltmp, WRITE_SAVE); /* actual level*/
	}
	
	savegamestate(fd, WRITE_SAVE | FREE_SAVE);
	freedynamicdata();

	return TRUE;
}


static void savegamestate(int fd, int mode)
{
	unsigned ustuck_id = (u.ustuck ? u.ustuck->m_id : 0);
	unsigned usteed_id = (u.usteed ? u.usteed->m_id : 0);

	bwrite(fd, &flags, sizeof(struct flag));
	bwrite(fd, &u, sizeof(struct you));
	bwrite(fd, &youmonst, sizeof(youmonst));

	/* must come before migrating_objs and migrating_mons are freed */
	save_timers(fd, level, mode, RANGE_GLOBAL);
	save_light_sources(fd, level, mode, RANGE_GLOBAL);

	saveobjchn(fd, invent, mode);
	saveobjchn(fd, migrating_objs, mode);
	savemonchn(fd, migrating_mons, mode);
	if (release_data(mode)) {
	    invent = 0;
	    migrating_objs = 0;
	    migrating_mons = 0;
	}
	bwrite(fd, mvitals, sizeof(mvitals));

	bwrite(fd, &moves, sizeof moves);
	bwrite(fd, &quest_status, sizeof(struct q_score));
	bwrite(fd, spl_book, sizeof(struct spell) * (MAXSPELL + 1));
	save_artifacts(fd);
	save_oracles(fd, mode);
	if (ustuck_id)
	    bwrite(fd, &ustuck_id, sizeof ustuck_id);
	if (usteed_id)
	    bwrite(fd, &usteed_id, sizeof usteed_id);
	bwrite(fd, pl_character, sizeof pl_character);
	bwrite(fd, pl_fruit, sizeof pl_fruit);
	bwrite(fd, &current_fruit, sizeof current_fruit);
	savefruitchn(fd, mode);
	savenames(fd, mode);
	save_waterlevel(fd, mode);
	bwrite(fd, &lastinvnr, sizeof(lastinvnr));
	save_mt_state(fd);
	save_track(fd);
	save_food(fd);
}


void savelev(int fd, xchar levnum, int mode)
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

	if (levnum >= 0 && levnum <= maxledgerno())
	    level_info[levnum].flags |= VISITED;
	bwrite(fd,&levnum,sizeof(levnum));
	bwrite(fd,&lev->z,sizeof(lev->z));
	bwrite(fd,lev->locations,sizeof(lev->locations));
	bwrite(fd,&lev->lastmoves,sizeof(lev->lastmoves));
	bwrite(fd,&lev->upstair,sizeof(stairway));
	bwrite(fd,&lev->dnstair,sizeof(stairway));
	bwrite(fd,&lev->upladder,sizeof(stairway));
	bwrite(fd,&lev->dnladder,sizeof(stairway));
	bwrite(fd,&lev->sstairs,sizeof(stairway));
	bwrite(fd,&lev->updest,sizeof(dest_area));
	bwrite(fd,&lev->dndest,sizeof(dest_area));
	bwrite(fd,&lev->flags,sizeof(lev->flags));
	bwrite(fd, lev->doors, sizeof(lev->doors));
	save_rooms(fd, lev);	/* no dynamic memory to reclaim */

	/* from here on out, saving also involves allocated memory cleanup */
skip_lots:
	/* must be saved before mons, objs, and buried objs */
	save_timers(fd, lev, mode, RANGE_LEVEL);
	save_light_sources(fd, lev, mode, RANGE_LEVEL);

	savemonchn(fd, lev->monlist, mode);
	save_worm(fd, lev, mode);	/* save worm information */
	savetrapchn(fd, lev->lev_traps, mode);
	saveobjchn(fd, lev->objlist, mode);
	saveobjchn(fd, lev->buriedobjlist, mode);
	saveobjchn(fd, lev->billobjs, mode);
	if (release_data(mode)) {
	    lev->monlist = NULL;
	    lev->lev_traps = NULL;
	    lev->objlist = NULL;
	    lev->buriedobjlist = NULL;
	    lev->billobjs = NULL;
	}
	save_engravings(fd, lev, mode);
	savedamage(fd, lev, mode);
	save_regions(fd, lev, mode);
	
	if (release_data(mode)) {
	    free(lev);
	    levels[levnum] = NULL;
	}
}


void bwrite(int fd, void *loc, unsigned num)
{
    boolean failed;

    failed = (write(fd, loc, num) != num);
    if (failed)
	panic("cannot write %u bytes to file #%d", num, fd);
}


static void savelevchn(int fd, int mode)
{
	s_level	*tmplev, *tmplev2;
	int cnt = 0;

	for (tmplev = sp_levchn; tmplev; tmplev = tmplev->next) cnt++;
	if (perform_bwrite(mode))
	    bwrite(fd, &cnt, sizeof(int));

	for (tmplev = sp_levchn; tmplev; tmplev = tmplev2) {
	    tmplev2 = tmplev->next;
	    if (perform_bwrite(mode))
		bwrite(fd, tmplev, sizeof(s_level));
	    if (release_data(mode))
		free(tmplev);
	}
	if (release_data(mode))
	    sp_levchn = 0;
}


static void savedamage(int fd, struct level *lev, int mode)
{
	struct damage *damageptr, *tmp_dam;
	unsigned int xl = 0;

	damageptr = lev->damagelist;
	for (tmp_dam = damageptr; tmp_dam; tmp_dam = tmp_dam->next)
	    xl++;
	if (perform_bwrite(mode))
	    bwrite(fd, &xl, sizeof(xl));

	while (xl--) {
	    if (perform_bwrite(mode))
		bwrite(fd, damageptr, sizeof(*damageptr));
	    tmp_dam = damageptr;
	    damageptr = damageptr->next;
	    if (release_data(mode))
		free(tmp_dam);
	}
	if (release_data(mode))
	    lev->damagelist = NULL;
}


static void saveobjchn(int fd, struct obj *otmp, int mode)
{
	struct obj *otmp2;
	unsigned int xl;
	int minusone = -1;

	while (otmp) {
	    otmp2 = otmp->nobj;
	    if (perform_bwrite(mode)) {
		xl = otmp->oxlth + otmp->onamelth;
		bwrite(fd, &xl, sizeof(int));
		bwrite(fd, otmp, xl + sizeof(struct obj));
	    }
	    if (Has_contents(otmp))
		saveobjchn(fd,otmp->cobj,mode);
	    if (release_data(mode)) {
		otmp->where = OBJ_FREE;	/* set to free so dealloc will work */
		otmp->timed = 0;	/* not timed any more */
		otmp->lamplit = 0;	/* caller handled lights */
		dealloc_obj(otmp);
	    }
	    otmp = otmp2;
	}
	if (perform_bwrite(mode))
	    bwrite(fd, &minusone, sizeof(int));
}

static void savemonchn(int fd, struct monst *mtmp, int mode)
{
	struct monst *mtmp2;
	unsigned int xl;
	int minusone = -1;
	const struct permonst *monbegin = &mons[0];

	if (perform_bwrite(mode))
	    bwrite(fd, &monbegin, sizeof(monbegin));

	while (mtmp) {
	    mtmp2 = mtmp->nmon;
	    if (perform_bwrite(mode)) {
		xl = mtmp->mxlth + mtmp->mnamelth;
		bwrite(fd, &xl, sizeof(int));
		bwrite(fd, mtmp, xl + sizeof(struct monst));
	    }
	    if (mtmp->minvent)
		saveobjchn(fd,mtmp->minvent,mode);
	    if (release_data(mode))
		dealloc_monst(mtmp);
	    mtmp = mtmp2;
	}
	if (perform_bwrite(mode))
	    bwrite(fd, &minusone, sizeof(int));
}

static void savetrapchn(int fd, struct trap *trap, int mode)
{
	struct trap *trap2;

	while (trap) {
	    trap2 = trap->ntrap;
	    if (perform_bwrite(mode))
		bwrite(fd, trap, sizeof(struct trap));
	    if (release_data(mode))
		dealloc_trap(trap);
	    trap = trap2;
	}
	if (perform_bwrite(mode))
	    bwrite(fd, (void*)nul, sizeof(struct trap));
}

/* save all the fruit names and ID's; this is used only in saving whole games
 * (not levels) and in saving bones levels.  When saving a bones level,
 * we only want to save the fruits which exist on the bones level; the bones
 * level routine marks nonexistent fruits by making the fid negative.
 */
void savefruitchn(int fd, int mode)
{
	struct fruit *f2, *f1;

	f1 = ffruit;
	while (f1) {
	    f2 = f1->nextf;
	    if (f1->fid >= 0 && perform_bwrite(mode))
		bwrite(fd, f1, sizeof(struct fruit));
	    if (release_data(mode))
		dealloc_fruit(f1);
	    f1 = f2;
	}
	if (perform_bwrite(mode))
	    bwrite(fd, (void *)nul, sizeof(struct fruit));
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

#ifdef AUTOPICKUP_EXCEPTIONS
	free_autopickup_exceptions();
#endif
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
