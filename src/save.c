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
static void savedamage(int,int);
static void saveobjchn(int,struct obj *,int);
static void savemonchn(int,struct monst *,int);
static void savetrapchn(int,struct trap *,int);
static void savegamestate(int,int);


/* need to preserve these during save to avoid accessing freed memory */
static unsigned ustuck_id = 0, usteed_id = 0;

int dosave(void)
{
	clear_nhwindow(NHW_MESSAGE);
	if (yn("Really save?") == 'n') {
		clear_nhwindow(NHW_MESSAGE);
		if (multi > 0) nomul(0);
	} else {
		clear_nhwindow(NHW_MESSAGE);
		pline("Saving...");
		if (dosave0(FALSE)) {
			program_state.something_worth_saving = 0;
			u.uhp = -1;		/* universal game's over indicator */
			/* make sure they see the Saving message */
			display_nhwindow(NHW_MESSAGE, TRUE);
			terminate();
		} else doredraw();
	}
	return 0;
}


/* returns 1 if save successful */
int dosave0(boolean emergency)
{
	int fd, ofd;
	xchar ltmp;
	d_level uz_save;
	char whynot[BUFSZ];

	fd = logfile;
	
	log_finish(LS_SAVED);
	vision_recalc(2);	/* shut down vision to prevent problems
				   in the event of an impossible() call */
	store_version(fd);
	ustuck_id = (u.ustuck ? u.ustuck->m_id : 0);
	usteed_id = (u.usteed ? u.usteed->m_id : 0);
	savelev(fd, ledger_no(&u.uz), WRITE_SAVE | FREE_SAVE);
	savegamestate(fd, WRITE_SAVE | FREE_SAVE);
	save_mt_state(fd);

	/* While copying level files around, zero out u.uz to keep
	 * parts of the restore code from completely initializing all
	 * in-core data structures, since all we're doing is copying.
	 * This also avoids at least one nasty core dump.
	 */
	uz_save = u.uz;
	u.uz.dnum = u.uz.dlevel = 0;
	/* these pointers are no longer valid, and at least u.usteed
	 * may mislead place_monster() on other levels
	 */
	u.ustuck = NULL;
	u.usteed = NULL;

	for (ltmp = (xchar)1; ltmp <= maxledgerno(); ltmp++) {
		if (ltmp == ledger_no(&uz_save)) continue;
		if (!(level_info[ltmp].flags & LFILE_EXISTS)) continue;
		ofd = open_levelfile(ltmp, whynot);
		if (ofd < 0) {
		    pline("%s", whynot);
		    killer = whynot;
		    done(TRICKED);
		    return 0;
		}
		getlev(ofd, hackpid, ltmp, FALSE);
		close(ofd);
		bwrite(fd, &ltmp, sizeof ltmp); /* level number*/
		savelev(fd, ltmp, WRITE_SAVE | FREE_SAVE);     /* actual level*/
		delete_levelfile(ltmp);
	}
	close(fd);

	u.uz = uz_save;

	/* get rid of current level --jgm */
	delete_levelfile(ledger_no(&u.uz));
	delete_levelfile(0);
	return TRUE;
}

static void savegamestate(int fd, int mode)
{
	bwrite(fd, &flags, sizeof(struct flag));
	bwrite(fd, &u, sizeof(struct you));

	/* must come before migrating_objs and migrating_mons are freed */
	save_timers(fd, mode, RANGE_GLOBAL);
	save_light_sources(fd, mode, RANGE_GLOBAL);

	saveobjchn(fd, invent, mode);
	saveobjchn(fd, migrating_objs, mode);
	savemonchn(fd, migrating_mons, mode);
	if (release_data(mode)) {
	    invent = 0;
	    migrating_objs = 0;
	    migrating_mons = 0;
	}
	bwrite(fd, mvitals, sizeof(mvitals));

	save_dungeon(fd, (boolean)!!perform_bwrite(mode),
			 (boolean)!!release_data(mode));
	savelevchn(fd, mode);
	bwrite(fd, &moves, sizeof moves);
	bwrite(fd, &monstermoves, sizeof monstermoves);
	bwrite(fd, &quest_status, sizeof(struct q_score));
	bwrite(fd, spl_book,
				sizeof(struct spell) * (MAXSPELL + 1));
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
}


void savelev(int fd, xchar lev, int mode)
{
	/* if we're tearing down the current level without saving anything
	   (which happens upon entrance to the endgame or after an aborted
	   restore attempt) then we don't want to do any actual I/O */
	if (mode == FREE_SAVE) goto skip_lots;
	if (iflags.purge_monsters) {
		/* purge any dead monsters (necessary if we're starting
		 * a panic save rather than a normal one, or sometimes
		 * when changing levels without taking time -- e.g.
		 * create statue trap then immediately level teleport) */
		dmonsfree();
	}

	if (fd < 0) panic("Save on bad file!");	/* impossible */
	if (lev >= 0 && lev <= maxledgerno())
	    level_info[lev].flags |= VISITED;
	bwrite(fd,&hackpid,sizeof(hackpid));
	bwrite(fd,&lev,sizeof(lev));
	bwrite(fd,level.locations,sizeof(level.locations));
	bwrite(fd,&monstermoves,sizeof(monstermoves));
	bwrite(fd,&upstair,sizeof(stairway));
	bwrite(fd,&dnstair,sizeof(stairway));
	bwrite(fd,&upladder,sizeof(stairway));
	bwrite(fd,&dnladder,sizeof(stairway));
	bwrite(fd,&sstairs,sizeof(stairway));
	bwrite(fd,&updest,sizeof(dest_area));
	bwrite(fd,&dndest,sizeof(dest_area));
	bwrite(fd,&level.flags,sizeof(level.flags));
	bwrite(fd, doors, sizeof(doors));
	save_rooms(fd);	/* no dynamic memory to reclaim */

	/* from here on out, saving also involves allocated memory cleanup */
 skip_lots:
	/* must be saved before mons, objs, and buried objs */
	save_timers(fd, mode, RANGE_LEVEL);
	save_light_sources(fd, mode, RANGE_LEVEL);

	savemonchn(fd, level.monlist, mode);
	save_worm(fd, mode);	/* save worm information */
	savetrapchn(fd, ftrap, mode);
	saveobjchn(fd, level.objlist, mode);
	saveobjchn(fd, level.buriedobjlist, mode);
	saveobjchn(fd, billobjs, mode);
	if (release_data(mode)) {
	    level.monlist = 0;
	    ftrap = 0;
	    level.objlist = 0;
	    level.buriedobjlist = 0;
	    billobjs = 0;
	}
	save_engravings(fd, mode);
	savedamage(fd, mode);
	save_regions(fd, mode);
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

static void savedamage(int fd, int mode)
{
	struct damage *damageptr, *tmp_dam;
	unsigned int xl = 0;

	damageptr = level.damagelist;
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
	    level.damagelist = 0;
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
		if (otmp->oclass == FOOD_CLASS) food_disappears(otmp);
		if (otmp->oclass == SPBOOK_CLASS) book_disappears(otmp);
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
# define free_worm()	 save_worm(0, FREE_SAVE)
# define free_timers(R)	 save_timers(0, FREE_SAVE, R)
# define free_light_sources(R) save_light_sources(0, FREE_SAVE, R);
# define free_engravings() save_engravings(0, FREE_SAVE)
# define freedamage()	 savedamage(0, FREE_SAVE)
# define free_animals()	 mon_animal_list(FALSE)

	/* move-specific data */
	dmonsfree();		/* release dead monsters */

	/* level-specific data */
	free_timers(RANGE_LEVEL);
	free_light_sources(RANGE_LEVEL);
	freemonchn(level.monlist);
	free_worm();		/* release worm segment information */
	freetrapchn(ftrap);
	freeobjchn(level.objlist);
	freeobjchn(level.buriedobjlist);
	freeobjchn(billobjs);
	free_engravings();
	freedamage();

	/* game-state data */
	free_timers(RANGE_GLOBAL);
	free_light_sources(RANGE_GLOBAL);
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
