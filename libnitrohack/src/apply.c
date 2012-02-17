/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "edog.h"

static const char tools[] = { TOOL_CLASS, WEAPON_CLASS, WAND_CLASS, 0 };
static const char tools_too[] = { ALL_CLASSES, TOOL_CLASS, POTION_CLASS,
				  WEAPON_CLASS, WAND_CLASS, GEM_CLASS, 0 };

static int use_camera(struct obj *);
static int use_towel(struct obj *);
static boolean its_dead(int,int,int *);
static int use_stethoscope(struct obj *);
static void use_whistle(struct obj *);
static void use_magic_whistle(struct obj *);
static void use_leash(struct obj *);
static int use_mirror(struct obj *);
static void use_bell(struct obj **);
static void use_candelabrum(struct obj *);
static void use_candle(struct obj **);
static void use_lamp(struct obj *);
static void light_cocktail(struct obj *);
static void use_tinning_kit(struct obj *);
static boolean use_figurine(struct obj *obj);
static void use_grease(struct obj *);
static void use_trap(struct obj *);
static void use_stone(struct obj *);
static int set_trap(void);		/* occupation callback */
static int use_whip(struct obj *);
static int use_pole(struct obj *);
static int use_cream_pie(struct obj *);
static int use_grapple(struct obj *);
static boolean figurine_location_checks(struct obj *, coord *, boolean);
static boolean uhave_graystone(void);
static void add_class(char *, char);

static const char no_elbow_room[] = "You don't have enough elbow-room to maneuver.";

static int use_camera(struct obj *obj)
{
	struct monst *mtmp;
	schar dx, dy, dz;

	if (Underwater) {
		pline("Using your camera underwater would void the warranty.");
		return 0;
	}
	if (!getdir(NULL, &dx, &dy, &dz))
	    return 0;

	if (obj->spe <= 0) {
		pline("Nothing happens.");
		return 1;
	}
	consume_obj_charge(obj, TRUE);

	if (obj->cursed && !rn2(2)) {
		zapyourself(obj, TRUE);
	} else if (u.uswallow) {
		pline("You take a picture of %s %s.", s_suffix(mon_nam(u.ustuck)),
		    mbodypart(u.ustuck, STOMACH));
	} else if (dz) {
		pline("You take a picture of the %s.",
			(dz > 0) ? surface(u.ux, u.uy) : ceiling(u.ux, u.uy));
	} else if (!dx && !dy) {
		zapyourself(obj, TRUE);
	} else if ((mtmp = beam_hit(dx, dy, COLNO, FLASHED_LIGHT,
				NULL, NULL, obj, NULL)) != 0) {
		obj->ox = u.ux,  obj->oy = u.uy;
		flash_hits_mon(mtmp, obj);
	}
	return 1;
}

static int use_towel(struct obj *obj)
{
	if (!freehand()) {
		pline("You have no free %s!", body_part(HAND));
		return 0;
	} else if (obj->owornmask) {
		pline("You cannot use it while you're wearing it!");
		return 0;
	} else if (obj->cursed) {
		long old;
		switch (rn2(3)) {
		case 2:
		    old = Glib;
		    Glib += rn1(10, 3);
		    pline("Your %s %s!", makeplural(body_part(HAND)),
			(old ? "are filthier than ever" : "get slimy"));
		    return 1;
		case 1:
		    if (!ublindf) {
			old = u.ucreamed;
			u.ucreamed += rn1(10, 3);
			pline("Yecch! Your %s %s gunk on it!", body_part(FACE),
			      (old ? "has more" : "now has"));
			make_blinded(Blinded + (long)u.ucreamed - old, TRUE);
		    } else {
			const char *what = (ublindf->otyp == LENSES) ?
					    "lenses" : "blindfold";
			if (ublindf->cursed) {
			    pline("You push your %s %s.", what,
				rn2(2) ? "cock-eyed" : "crooked");
			} else {
			    struct obj *saved_ublindf = ublindf;
			    pline("You push your %s off.", what);
			    Blindf_off(ublindf);
			    dropx(saved_ublindf);
			}
		    }
		    return 1;
		case 0:
		    break;
		}
	}

	if (Glib) {
		Glib = 0;
		pline("You wipe off your %s.", makeplural(body_part(HAND)));
		return 1;
	} else if (u.ucreamed) {
		Blinded -= u.ucreamed;
		u.ucreamed = 0;

		if (!Blinded) {
			pline("You've got the glop off.");
			Blinded = 1;
			make_blinded(0L,TRUE);
		} else {
			pline("Your %s feels clean now.", body_part(FACE));
		}
		return 1;
	}

	pline("Your %s and %s are already clean.",
		body_part(FACE), makeplural(body_part(HAND)));

	return 0;
}

/* maybe give a stethoscope message based on floor objects */
static boolean its_dead(int rx, int ry, int *resp)
{
	struct obj *otmp;
	struct trap *ttmp;

	if (!can_reach_floor()) return FALSE;

	/* additional stethoscope messages from jyoung@apanix.apana.org.au */
	if (Hallucination && sobj_at(CORPSE, level, rx, ry)) {
	    /* (a corpse doesn't retain the monster's sex,
	       so we're forced to use generic pronoun here) */
	    You_hear("a voice say, \"It's dead, Jim.\"");
	    *resp = 1;
	    return TRUE;
	} else if (Role_if (PM_HEALER) && ((otmp = sobj_at(CORPSE, level, rx, ry)) != 0 ||
				    (otmp = sobj_at(STATUE, level, rx, ry)) != 0)) {
	    /* possibly should check uppermost {corpse,statue} in the pile
	       if both types are present, but it's not worth the effort */
	    if (vobj_at(rx, ry)->otyp == STATUE) otmp = vobj_at(rx, ry);
	    if (otmp->otyp == CORPSE) {
		pline("You determine that %s unfortunate being is dead.",
		    (rx == u.ux && ry == u.uy) ? "this" : "that");
	    } else {
		ttmp = t_at(level, rx, ry);
		pline("%s appears to be in %s health for a statue.",
		      The(mons[otmp->corpsenm].mname),
		      (ttmp && ttmp->ttyp == STATUE_TRAP) ?
			"extraordinary" : "excellent");
	    }
	    return TRUE;
	}
	return FALSE;
}

static const char hollow_str[] = "a hollow sound.  This must be a secret %s!";

/* Strictly speaking it makes no sense for usage of a stethoscope to
   not take any time; however, unless it did, the stethoscope would be
   almost useless.  As a compromise, one use per turn is free, another
   uses up the turn; this makes curse status have a tangible effect. */
static int use_stethoscope(struct obj *obj)
{
	struct monst *mtmp;
	struct rm *loc;
	int rx, ry, res;
	schar dx, dy, dz;
	boolean interference = (u.uswallow && is_whirly(u.ustuck->data) &&
				!rn2(Role_if (PM_HEALER) ? 10 : 3));

	if (nohands(youmonst.data)) {	/* should also check for no ears and/or deaf */
		pline("You have no hands!");	/* not `body_part(HAND)' */
		return 0;
	} else if (!freehand()) {
		pline("You have no free %s.", body_part(HAND));
		return 0;
	}
	if (!getdir(NULL, &dx, &dy, &dz))
	    return 0;

	res = (moves == stetho_last_used_move) &&
	      (youmonst.movement == stetho_last_used_movement);
	stetho_last_used_move = moves;
	stetho_last_used_movement = youmonst.movement;

	if (u.usteed && dz > 0) {
		if (interference) {
			pline("%s interferes.", Monnam(u.ustuck));
			mstatusline(u.ustuck);
		} else
			mstatusline(u.usteed);
		return res;
	} else if (u.uswallow && (dx || dy || dz)) {
		mstatusline(u.ustuck);
		return res;
	} else if (u.uswallow && interference) {
		pline("%s interferes.", Monnam(u.ustuck));
		mstatusline(u.ustuck);
		return res;
	} else if (dz) {
		if (Underwater)
		    You_hear("faint splashing.");
		else if (dz < 0 || !can_reach_floor())
		    pline("You can't reach the %s.",
			(dz > 0) ? surface(u.ux, u.uy) : ceiling(u.ux, u.uy));
		else if (its_dead(u.ux, u.uy, &res))
		    ;	/* message already given */
		else if (Is_stronghold(&u.uz))
		    You_hear("the crackling of hellfire.");
		else
		    pline("The %s seems healthy enough.", surface(u.ux, u.uy));
		return res;
	} else if (obj->cursed && !rn2(2)) {
		You_hear("your heart beat.");
		return res;
	}
	if (Stunned || (Confusion && !rn2(5)))
	    confdir(&dx, &dy);
	if (!dx && !dy) {
		ustatusline();
		return res;
	}
	
	rx = u.ux + dx;
	ry = u.uy + dy;
	if (!isok(rx, ry)) {
		You_hear("a faint typing noise.");
		return 0;
	}
	if ((mtmp = m_at(level, rx, ry)) != 0) {
		mstatusline(mtmp);
		if (mtmp->mundetected) {
			mtmp->mundetected = 0;
			if (cansee(rx, ry)) newsym(mtmp->mx, mtmp->my);
		}
		if (!canspotmon(mtmp))
			map_invisible(rx, ry);
		return res;
	}
	if (level->locations[rx][ry].mem_invis) {
		unmap_object(rx, ry);
		newsym(rx, ry);
		pline("The invisible monster must have moved.");
	}
	loc = &level->locations[rx][ry];
	switch(loc->typ) {
	case SDOOR:
		You_hear(hollow_str, "door");
		cvt_sdoor_to_door(loc, &u.uz);		/* ->typ = DOOR */
		if (Blind) feel_location(rx,ry);
		else newsym(rx,ry);
		return res;
	case SCORR:
		You_hear(hollow_str, "passage");
		loc->typ = CORR;
		unblock_point(rx,ry);
		if (Blind) feel_location(rx,ry);
		else newsym(rx,ry);
		return res;
	}

	if (!its_dead(rx, ry, &res))
	    pline("You hear nothing special.");	/* not You_hear()  */
	return res;
}

static const char whistle_str[] = "You produce a %s whistling sound.";

static void use_whistle(struct obj *obj)
{
	pline(whistle_str, obj->cursed ? "shrill" : "high");
	wake_nearby();
}

static void use_magic_whistle(struct obj *obj)
{
	struct monst *mtmp, *nextmon;

	if (obj->cursed && !rn2(2)) {
		pline("You produce a high-pitched humming noise.");
		wake_nearby();
	} else {
		int pet_cnt = 0;
		pline(whistle_str, Hallucination ? "normal" : "strange");
		for (mtmp = level->monlist; mtmp; mtmp = nextmon) {
		    nextmon = mtmp->nmon; /* trap might kill mon */
		    if (DEADMONSTER(mtmp)) continue;
		    if (mtmp->mtame) {
			if (mtmp->mtrapped) {
			    /* no longer in previous trap (affects mintrap) */
			    mtmp->mtrapped = 0;
			    fill_pit(level, mtmp->mx, mtmp->my);
			}
			mnexto(mtmp);
			if (canspotmon(mtmp)) ++pet_cnt;
			if (mintrap(mtmp) == 2) change_luck(-1);
		    }
		}
		if (pet_cnt > 0) makeknown(obj->otyp);
	}
}

boolean um_dist(xchar x, xchar y, xchar n)
{
	return (boolean)(abs(u.ux - x) > n  || abs(u.uy - y) > n);
}

int number_leashed(void)
{
	int i = 0;
	struct obj *obj;

	for (obj = invent; obj; obj = obj->nobj)
		if (obj->otyp == LEASH && obj->leashmon != 0) i++;
	return i;
}

void o_unleash(struct obj *otmp)	/* otmp is about to be destroyed or stolen */
{
	struct monst *mtmp;

	for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
		if (mtmp->m_id == (unsigned)otmp->leashmon)
			mtmp->mleashed = 0;
	otmp->leashmon = 0;
}

void m_unleash(struct monst *mtmp, boolean feedback)	/* mtmp is about to die, or become untame */
{
	struct obj *otmp;

	if (feedback) {
	    if (canseemon(mtmp))
		pline("%s pulls free of %s leash!", Monnam(mtmp), mhis(mtmp));
	    else
		pline("Your leash falls slack.");
	}
	for (otmp = invent; otmp; otmp = otmp->nobj)
		if (otmp->otyp == LEASH &&
				otmp->leashmon == (int)mtmp->m_id)
			otmp->leashmon = 0;
	mtmp->mleashed = 0;
}

void unleash_all(void)		/* player is about to die (for bones) */
{
	struct obj *otmp;
	struct monst *mtmp;

	for (otmp = invent; otmp; otmp = otmp->nobj)
		if (otmp->otyp == LEASH) otmp->leashmon = 0;
	for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
		mtmp->mleashed = 0;
}

#define MAXLEASHED	2

/* ARGSUSED */
static void use_leash(struct obj *obj)
{
	coord cc;
	struct monst *mtmp;
	int spotmon;
	schar dz;

	if (!obj->leashmon && number_leashed() >= MAXLEASHED) {
		pline("You cannot leash any more pets.");
		return;
	}

	if (!get_adjacent_loc(NULL, NULL, u.ux, u.uy, &cc, &dz)) return;

	if ((cc.x == u.ux) && (cc.y == u.uy)) {
		if (u.usteed && dz > 0) {
		    mtmp = u.usteed;
		    spotmon = 1;
		    goto got_target;
		}
		pline("Leash yourself?  Very funny...");
		return;
	}

	if (!(mtmp = m_at(level, cc.x, cc.y))) {
		pline("There is no creature there.");
		return;
	}

	spotmon = canspotmon(mtmp);
got_target:

	if (!mtmp->mtame) {
	    if (!spotmon)
		pline("There is no creature there.");
	    else
		pline("%s %s leashed!", Monnam(mtmp), (!obj->leashmon) ?
				"cannot be" : "is not");
	    return;
	}
	if (!obj->leashmon) {
		if (mtmp->mleashed) {
			pline("This %s is already leashed.",
			      spotmon ? l_monnam(mtmp) : "monster");
			return;
		}
		pline("You slip the leash around %s%s.",
		    spotmon ? "your " : "", l_monnam(mtmp));
		mtmp->mleashed = 1;
		obj->leashmon = (int)mtmp->m_id;
		mtmp->msleeping = 0;
		return;
	}
	if (obj->leashmon != (int)mtmp->m_id) {
		pline("This leash is not attached to that creature.");
		return;
	} else {
		if (obj->cursed) {
			pline("The leash would not come off!");
			obj->bknown = TRUE;
			return;
		}
		mtmp->mleashed = 0;
		obj->leashmon = 0;
		pline("You remove the leash from %s%s.",
		    spotmon ? "your " : "", l_monnam(mtmp));
	}
	return;
}

struct obj *get_mleash(struct monst *mtmp)	/* assuming mtmp->mleashed has been checked */
{
	struct obj *otmp;

	otmp = invent;
	while (otmp) {
		if (otmp->otyp == LEASH && otmp->leashmon == (int)mtmp->m_id)
			return otmp;
		otmp = otmp->nobj;
	}
	return NULL;
}


boolean next_to_u(void)
{
	struct monst *mtmp;
	struct obj *otmp;

	for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
		if (DEADMONSTER(mtmp)) continue;
		if (mtmp->mleashed) {
			if (distu(mtmp->mx,mtmp->my) > 2) mnexto(mtmp);
			if (distu(mtmp->mx,mtmp->my) > 2) {
			    for (otmp = invent; otmp; otmp = otmp->nobj)
				if (otmp->otyp == LEASH &&
					otmp->leashmon == (int)mtmp->m_id) {
				    if (otmp->cursed) return FALSE;
				    pline("You feel %s leash go slack.",
					(number_leashed() > 1) ? "a" : "the");
				    mtmp->mleashed = 0;
				    otmp->leashmon = 0;
				}
			}
		}
	}
	/* no pack mules for the Amulet */
	if (u.usteed && mon_has_amulet(u.usteed))
	    return FALSE;
	return TRUE;
}


void check_leash(xchar x, xchar y)
{
	struct obj *otmp;
	struct monst *mtmp;

	for (otmp = invent; otmp; otmp = otmp->nobj) {
	    if (otmp->otyp != LEASH || otmp->leashmon == 0) continue;
	    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
		if (DEADMONSTER(mtmp)) continue;
		if ((int)mtmp->m_id == otmp->leashmon) break; 
	    }
	    if (!mtmp) {
		impossible("leash in use isn't attached to anything?");
		otmp->leashmon = 0;
		continue;
	    }
	    if (dist2(u.ux,u.uy,mtmp->mx,mtmp->my) >
		    dist2(x,y,mtmp->mx,mtmp->my)) {
		if (!um_dist(mtmp->mx, mtmp->my, 3)) {
		    ;	/* still close enough */
		} else if (otmp->cursed && !breathless(mtmp->data)) {
		    if (um_dist(mtmp->mx, mtmp->my, 5) ||
			    (mtmp->mhp -= rnd(2)) <= 0) {
			long save_pacifism = u.uconduct.killer;

			pline("Your leash chokes %s to death!", mon_nam(mtmp));
			/* hero might not have intended to kill pet, but
			   that's the result of his actions; gain experience,
			   lose pacifism, take alignment and luck hit, make
			   corpse less likely to remain tame after revival */
			xkilled(mtmp, 0);	/* no "you kill it" message */
			/* life-saving doesn't ordinarily reset this */
			if (mtmp->mhp > 0) u.uconduct.killer = save_pacifism;
		    } else {
			pline("%s chokes on the leash!", Monnam(mtmp));
			/* tameness eventually drops to 1 here (never 0) */
			if (mtmp->mtame && rn2(mtmp->mtame)) mtmp->mtame--;
		    }
		} else {
		    if (um_dist(mtmp->mx, mtmp->my, 5)) {
			pline("%s leash snaps loose!", s_suffix(Monnam(mtmp)));
			m_unleash(mtmp, FALSE);
		    } else {
			pline("You pull on the leash.");
			if (mtmp->data->msound != MS_SILENT)
			    switch (rn2(3)) {
			    case 0:  growl(mtmp);   break;
			    case 1:  yelp(mtmp);    break;
			    default: whimper(mtmp); break;
			    }
		    }
		}
	    }
	}
}


#define WEAK	3	/* from eat.c */

static const char look_str[] = "You look %s.";

static int use_mirror(struct obj *obj)
{
	struct monst *mtmp;
	char mlet;
	boolean vis;
	schar dx, dy, dz;

	if (!getdir(NULL, &dx, &dy, &dz))
	    return 0;
	
	if (obj->cursed && !rn2(2)) {
		if (!Blind)
			pline("The mirror fogs up and doesn't reflect!");
		return 1;
	}
	if (!dx && !dy && !dz) {
		if (!Blind && !Invisible) {
		    if (u.umonnum == PM_FLOATING_EYE) {
			if (!Free_action) {
			    pline(Hallucination ?
				"Yow!  The mirror stares back!" :
				"Yikes!  You've frozen yourself!");
			    nomul(-rnd((MAXULEV+6) - u.ulevel), "gazing into a mirror");
			} else pline("You stiffen momentarily under your gaze.");
		    } else if (youmonst.data->mlet == S_VAMPIRE)
			pline("You don't have a reflection.");
		    else if (u.umonnum == PM_UMBER_HULK) {
			pline("Huh?  That doesn't look like you!");
			make_confused(HConfusion + dice(3,4), FALSE);
		    } else if (Hallucination)
			pline(look_str, hcolor(NULL));
		    else if (Sick)
			pline(look_str, "peaked");
		    else if (u.uhs >= WEAK)
			pline(look_str, "undernourished");
		    else pline("You look as %s as ever.",
				ACURR(A_CHA) > 14 ?
				(poly_gender()==1 ? "beautiful" : "handsome") :
				"ugly");
		} else {
			pline("You can't see your %s %s.",
				ACURR(A_CHA) > 14 ?
				(poly_gender()==1 ? "beautiful" : "handsome") :
				"ugly",
				body_part(FACE));
		}
		return 1;
	}
	if (u.uswallow) {
		if (!Blind) pline("You reflect %s %s.", s_suffix(mon_nam(u.ustuck)),
		    mbodypart(u.ustuck, STOMACH));
		return 1;
	}
	if (Underwater) {
		pline(Hallucination ?
		    "You give the fish a chance to fix their makeup." :
		    "You reflect the murky water.");
		return 1;
	}
	if (dz) {
		if (!Blind)
		    pline("You reflect the %s.",
			(dz > 0) ? surface(u.ux,u.uy) : ceiling(u.ux,u.uy));
		return 1;
	}
	mtmp = beam_hit(dx, dy, COLNO, INVIS_BEAM, NULL, NULL, obj, NULL);
	if (!mtmp || !haseyes(mtmp->data))
		return 1;

	vis = canseemon(mtmp);
	mlet = mtmp->data->mlet;
	if (mtmp->msleeping) {
		if (vis)
		    pline ("%s is too tired to look at your mirror.",
			    Monnam(mtmp));
	} else if (!mtmp->mcansee) {
	    if (vis)
		pline("%s can't see anything right now.", Monnam(mtmp));
	/* some monsters do special things */
	} else if (mlet == S_VAMPIRE || mlet == S_GHOST) {
	    if (vis)
		pline ("%s doesn't have a reflection.", Monnam(mtmp));
	} else if (!mtmp->mcan && !mtmp->minvis &&
					mtmp->data == &mons[PM_MEDUSA]) {
		if (mon_reflects(mtmp, "The gaze is reflected away by %s %s!"))
			return 1;
		if (vis)
			pline("%s is turned to stone!", Monnam(mtmp));
		stoned = TRUE;
		killed(mtmp);
	} else if (!mtmp->mcan && !mtmp->minvis &&
					mtmp->data == &mons[PM_FLOATING_EYE]) {
		int tmp = dice((int)mtmp->m_lev, (int)mtmp->data->mattk[0].damd);
		if (!rn2(4)) tmp = 120;
		if (vis)
			pline("%s is frozen by its reflection.", Monnam(mtmp));
		else You_hear("something stop moving.");
		mtmp->mcanmove = 0;
		if ( (int) mtmp->mfrozen + tmp > 127)
			mtmp->mfrozen = 127;
		else mtmp->mfrozen += tmp;
	} else if (!mtmp->mcan && !mtmp->minvis &&
					mtmp->data == &mons[PM_UMBER_HULK]) {
		if (vis)
			pline ("%s confuses itself!", Monnam(mtmp));
		mtmp->mconf = 1;
	} else if (!mtmp->mcan && !mtmp->minvis && (mlet == S_NYMPH
				     || mtmp->data==&mons[PM_SUCCUBUS])) {
		if (vis) {
		    pline ("%s admires herself in your mirror.", Monnam(mtmp));
		    pline ("She takes it!");
		} else pline ("It steals your mirror!");
		setnotworn(obj); /* in case mirror was wielded */
		freeinv(obj);
		mpickobj(mtmp,obj);
		if (!tele_restrict(mtmp)) rloc(mtmp, FALSE);
	} else if (!is_unicorn(mtmp->data) && !humanoid(mtmp->data) &&
			(!mtmp->minvis || perceives(mtmp->data)) && rn2(5)) {
		if (vis)
		    pline("%s is frightened by its reflection.", Monnam(mtmp));
		monflee(mtmp, dice(2,4), FALSE, FALSE);
	} else if (!Blind) {
		if (mtmp->minvis && !See_invisible)
		    ;
		else if ((mtmp->minvis && !perceives(mtmp->data))
			 || !haseyes(mtmp->data))
		    pline("%s doesn't seem to notice its reflection.",
			Monnam(mtmp));
		else
		    pline("%s ignores %s reflection.",
			  Monnam(mtmp), mhis(mtmp));
	}
	return 1;
}

static void use_bell(struct obj **optr)
{
	struct obj *obj = *optr;
	struct monst *mtmp;
	boolean wakem = FALSE, learno = FALSE,
		ordinary = (obj->otyp != BELL_OF_OPENING || !obj->spe),
		invoking = (obj->otyp == BELL_OF_OPENING &&
			 invocation_pos(&u.uz, u.ux, u.uy) && !On_stairs(u.ux, u.uy));

	pline("You ring %s.", the(xname(obj)));

	if (Underwater || (u.uswallow && ordinary)) {
	    pline("But the sound is muffled.");

	} else if (invoking && ordinary) {
	    /* needs to be recharged... */
	    pline("But it makes no sound.");
	    learno = TRUE;	/* help player figure out why */

	} else if (ordinary) {
	    if (obj->cursed && !rn2(4) &&
		    /* note: once any of them are gone, we stop all of them */
		    !(mvitals[PM_WOOD_NYMPH].mvflags & G_GONE) &&
		    !(mvitals[PM_WATER_NYMPH].mvflags & G_GONE) &&
		    !(mvitals[PM_MOUNTAIN_NYMPH].mvflags & G_GONE) &&
		    (mtmp = makemon(mkclass(&u.uz, S_NYMPH, 0), level,
					u.ux, u.uy, NO_MINVENT)) != 0) {
		pline("You summon %s!", a_monnam(mtmp));
		if (!obj_resists(obj, 93, 100)) {
		    pline("%s shattered!", Tobjnam(obj, "have"));
		    useup(obj);
		    *optr = 0;
		} else switch (rn2(3)) {
			default:
				break;
			case 1:
				mon_adjust_speed(mtmp, 2, NULL);
				break;
			case 2: /* no explanation; it just happens... */
				nomovemsg = "";
				nomul(-rnd(2), NULL);
				break;
		}
	    }
	    wakem = TRUE;

	} else {
	    /* charged Bell of Opening */
	    consume_obj_charge(obj, TRUE);

	    if (u.uswallow) {
		if (!obj->cursed)
		    openit();
		else
		    pline("Nothing happens.");

	    } else if (obj->cursed) {
		coord mm;

		mm.x = u.ux;
		mm.y = u.uy;
		mkundead(level, &mm, FALSE, NO_MINVENT);
		wakem = TRUE;

	    } else  if (invoking) {
		pline("%s an unsettling shrill sound...",
		      Tobjnam(obj, "issue"));
		obj->age = moves;
		learno = TRUE;
		wakem = TRUE;

	    } else if (obj->blessed) {
		int res = 0;

		if (uchain) {
		    unpunish();
		    res = 1;
		}
		res += openit();
		switch (res) {
		  case 0:  pline("Nothing happens."); break;
		  case 1:  pline("Something opens...");
			   learno = TRUE; break;
		  default: pline("Things open around you...");
			   learno = TRUE; break;
		}

	    } else {  /* uncursed */
		if (findit() != 0) learno = TRUE;
		else pline("Nothing happens.");
	    }

	}	/* charged BofO */

	if (learno) {
	    makeknown(BELL_OF_OPENING);
	    obj->known = 1;
	}
	if (wakem) wake_nearby();
}

static void use_candelabrum(struct obj *obj)
{
	const char *s = (obj->spe != 1) ? "candles" : "candle";

	if (Underwater) {
		pline("You cannot make fire under water.");
		return;
	}
	if (obj->lamplit) {
		pline("You snuff the %s.", s);
		end_burn(obj, TRUE);
		return;
	}
	if (obj->spe <= 0) {
		pline("This %s has no %s.", xname(obj), s);
		return;
	}
	if (u.uswallow || obj->cursed) {
		if (!Blind)
		    pline("The %s %s for a moment, then %s.",
			      s, vtense(s, "flicker"), vtense(s, "die"));
		return;
	}
	if (obj->spe < 7) {
		pline("There %s only %d %s in %s.",
		      vtense(s, "are"), obj->spe, s, the(xname(obj)));
		if (!Blind)
		    pline("%s lit.  %s dimly.",
			  obj->spe == 1 ? "It is" : "They are",
			  Tobjnam(obj, "shine"));
	} else {
		pline("%s's %s burn%s", The(xname(obj)), s,
			(Blind ? "." : " brightly!"));
	}
	if (!invocation_pos(&u.uz, u.ux, u.uy)) {
		pline("The %s %s being rapidly consumed!", s, vtense(s, "are"));
		obj->age /= 2;
	} else {
		if (obj->spe == 7) {
		    if (Blind)
		      pline("%s a strange warmth!", Tobjnam(obj, "radiate"));
		    else
		      pline("%s with a strange light!", Tobjnam(obj, "glow"));
		}
		obj->known = 1;
	}
	begin_burn(obj, FALSE);
}

static void use_candle(struct obj **optr)
{
	struct obj *obj = *optr;
	struct obj *otmp;
	const char *s = (obj->quan != 1) ? "candles" : "candle";
	char qbuf[QBUFSZ];

	if (u.uswallow) {
		pline(no_elbow_room);
		return;
	}
	
	otmp = carrying(CANDELABRUM_OF_INVOCATION);
	if (!otmp || otmp->spe == 7) {
		if (Underwater) {
		    pline("Sorry, fire and water don't mix.");
		    return;
		}
		use_lamp(obj);
		return;
	}

	sprintf(qbuf, "Attach %s", the(xname(obj)));
	sprintf(eos(qbuf), " to %s?",
		safe_qbuf(qbuf, sizeof(" to ?"), the(xname(otmp)),
			the(simple_typename(otmp->otyp)), "it"));
	if (yn(qbuf) == 'n') {
		if (Underwater) {
		    pline("Sorry, fire and water don't mix.");
		    return;
		}
		if (!obj->lamplit)
		    pline("You try to light %s...", the(xname(obj)));
		use_lamp(obj);
		return;
	} else {
		if ((long)otmp->spe + obj->quan > 7L)
		    obj = splitobj(obj, 7L - (long)otmp->spe);
		else *optr = 0;
		pline("You attach %ld%s %s to %s.",
		    obj->quan, !otmp->spe ? "" : " more",
		    s, the(xname(otmp)));
		if (!otmp->spe || otmp->age > obj->age)
		    otmp->age = obj->age;
		otmp->spe += (int)obj->quan;
		if (otmp->lamplit && !obj->lamplit)
		    pline("The new %s magically %s!", s, vtense(s, "ignite"));
		else if (!otmp->lamplit && obj->lamplit)
		    pline("%s out.", (obj->quan > 1L) ? "They go" : "It goes");
		if (obj->unpaid)
		    verbalize("You %s %s, you bought %s!",
			      otmp->lamplit ? "burn" : "use",
			      (obj->quan > 1L) ? "them" : "it",
			      (obj->quan > 1L) ? "them" : "it");
		if (obj->quan < 7L && otmp->spe == 7)
		    pline("%s now has seven%s candles attached.",
			  The(xname(otmp)), otmp->lamplit ? " lit" : "");
		/* candelabrum's light range might increase */
		if (otmp->lamplit) obj_merge_light_sources(otmp, otmp);
		/* candles are no longer a separate light source */
		if (obj->lamplit) end_burn(obj, TRUE);
		/* candles are now gone */
		useupall(obj);
	}
}

boolean snuff_candle(struct obj *otmp)  /* call in drop, throw, and put in box, etc. */
{
	boolean candle = Is_candle(otmp);

	if ((candle || otmp->otyp == CANDELABRUM_OF_INVOCATION) &&
		otmp->lamplit) {
	    char buf[BUFSZ];
	    xchar x, y;
	    boolean many = candle ? otmp->quan > 1L : otmp->spe > 1;

	    get_obj_location(otmp, &x, &y, 0);
	    if (otmp->where == OBJ_MINVENT ? cansee(x,y) : !Blind)
		pline("%s %scandle%s flame%s extinguished.",
		      Shk_Your(buf, otmp),
		      (candle ? "" : "candelabrum's "),
		      (many ? "s'" : "'s"), (many ? "s are" : " is"));
	   end_burn(otmp, TRUE);
	   return TRUE;
	}
	return FALSE;
}

/* called when lit lamp is hit by water or put into a container or
   you've been swallowed by a monster; obj might be in transit while
   being thrown or dropped so don't assume that its location is valid */
boolean snuff_lit(struct obj *obj)
{
	xchar x, y;

	if (obj->lamplit) {
	    if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
		    obj->otyp == BRASS_LANTERN || obj->otyp == POT_OIL) {
		get_obj_location(obj, &x, &y, 0);
		if (obj->where == OBJ_MINVENT ? cansee(x,y) : !Blind)
		    pline("%s %s out!", Yname2(obj), otense(obj, "go"));
		end_burn(obj, TRUE);
		return TRUE;
	    }
	    if (snuff_candle(obj)) return TRUE;
	}
	return FALSE;
}

/* Called when potentially lightable object is affected by fire_damage().
   Return TRUE if object was lit and FALSE otherwise --ALI */
boolean catch_lit(struct obj *obj)
{
	xchar x, y;

	if (!obj->lamplit && (obj->otyp == MAGIC_LAMP || ignitable(obj))) {
	    if ((obj->otyp == MAGIC_LAMP ||
		 obj->otyp == CANDELABRUM_OF_INVOCATION) &&
		obj->spe == 0)
		return FALSE;
	    else if (obj->otyp != MAGIC_LAMP && obj->age == 0)
		return FALSE;
	    if (!get_obj_location(obj, &x, &y, 0))
		return FALSE;
	    if (obj->otyp == CANDELABRUM_OF_INVOCATION && obj->cursed)
		return FALSE;
	    if ((obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
		 obj->otyp == BRASS_LANTERN) && obj->cursed && !rn2(2))
		return FALSE;
	    if (obj->where == OBJ_MINVENT ? cansee(x,y) : !Blind)
		pline("%s %s light!", Yname2(obj), otense(obj, "catch"));
	    if (obj->otyp == POT_OIL) makeknown(obj->otyp);
	    if (obj->unpaid && costly_spot(u.ux, u.uy) && (obj->where == OBJ_INVENT)) {
	        /* if it catches while you have it, then it's your tough luck */
		check_unpaid(obj);
	        verbalize("That's in addition to the cost of %s %s, of course.",
				Yname2(obj), obj->quan == 1 ? "itself" : "themselves");
		bill_dummy_object(obj);
	    }
	    begin_burn(obj, FALSE);
	    return TRUE;
	}
	return FALSE;
}

static void use_lamp(struct obj *obj)
{
	char buf[BUFSZ];

	if (Underwater) {
		pline("This is not a diving lamp.");
		return;
	}
	if (obj->lamplit) {
		if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
				obj->otyp == BRASS_LANTERN)
		    pline("%s lamp is now off.", Shk_Your(buf, obj));
		else
		    pline("You snuff out %s.", yname(obj));
		end_burn(obj, TRUE);
		return;
	}
	/* magic lamps with an spe == 0 (wished for) cannot be lit */
	if ((!Is_candle(obj) && obj->age == 0)
			|| (obj->otyp == MAGIC_LAMP && obj->spe == 0)) {
		if (obj->otyp == BRASS_LANTERN)
			pline("Your lamp has run out of power.");
		else pline("This %s has no oil.", xname(obj));
		return;
	}
	if (obj->cursed && !rn2(2)) {
		pline("%s for a moment, then %s.",
		      Tobjnam(obj, "flicker"), otense(obj, "die"));
	} else {
		if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
				obj->otyp == BRASS_LANTERN) {
		    check_unpaid(obj);
		    pline("%s lamp is now on.", Shk_Your(buf, obj));
		} else {	/* candle(s) */
		    pline("%s flame%s %s%s",
			s_suffix(Yname2(obj)),
			plur(obj->quan), otense(obj, "burn"),
			Blind ? "." : " brightly!");
		    if (obj->unpaid && costly_spot(u.ux, u.uy) &&
			  obj->age == 20L * (long)objects[obj->otyp].oc_cost) {
			const char *ithem = obj->quan > 1L ? "them" : "it";
			verbalize("You burn %s, you bought %s!", ithem, ithem);
			bill_dummy_object(obj);
		    }
		}
		begin_burn(obj, FALSE);
	}
}

static void light_cocktail(struct obj *obj) /* obj is a potion of oil */
{
	char buf[BUFSZ];

	if (u.uswallow) {
	    pline(no_elbow_room);
	    return;
	}

	if (obj->lamplit) {
	    pline("You snuff the lit potion.");
	    end_burn(obj, TRUE);
	    /*
	     * Free & add to re-merge potion.  This will average the
	     * age of the potions.  Not exactly the best solution,
	     * but its easy.
	     */
	    freeinv(obj);
	    addinv(obj);
	    return;
	} else if (Underwater) {
	    pline("There is not enough oxygen to sustain a fire.");
	    return;
	}

	pline("You light %s potion.%s", shk_your(buf, obj),
	    Blind ? "" : "  It gives off a dim light.");
	if (obj->unpaid && costly_spot(u.ux, u.uy)) {
	    /* Normally, we shouldn't both partially and fully charge
	     * for an item, but (Yendorian Fuel) Taxes are inevitable...
	     */
	    check_unpaid(obj);
	    verbalize("That's in addition to the cost of the potion, of course.");
	    bill_dummy_object(obj);
	}
	makeknown(obj->otyp);

	if (obj->quan > 1L) {
	    obj = splitobj(obj, 1L);
	    begin_burn(obj, FALSE);	/* burn before free to get position */
	    obj_extract_self(obj);	/* free from inv */

	    /* shouldn't merge */
	    hold_another_object(obj, "You drop %s!", doname(obj), NULL);
	} else
	    begin_burn(obj, FALSE);
}

static const char cuddly[] = { TOOL_CLASS, GEM_CLASS, 0 };

int dorub(struct obj *obj)
{
	if (obj && !validate_object(obj, cuddly, "rub"))
	    return 0;
	else if (!obj)
	    obj = getobj(cuddly, "rub");

        if (!obj) return 0;

	if (obj->oclass == GEM_CLASS) {
	    if (is_graystone(obj)) {
		use_stone(obj);
		return 1;
	    } else {
		pline("Sorry, I don't know how to use that.");
		return 0;
	    }
	}

	if (obj->otyp == MAGIC_LAMP) {
            if (!wield_tool(obj, "rub")) return 0;
	    if (uwep->spe > 0 && !rn2(3)) {
		check_unpaid_usage(uwep, TRUE);		/* unusual item use */
		djinni_from_bottle(uwep);
		makeknown(MAGIC_LAMP);
		uwep->otyp = OIL_LAMP;
		uwep->spe = 0; /* for safety */
		uwep->age = rn1(500,1000);
		if (uwep->lamplit) begin_burn(uwep, TRUE);
		update_inventory();
	    } else if (rn2(2) && !Blind)
		pline("You see a puff of smoke.");
	    else pline("Nothing happens.");
	} else if (obj->otyp == BRASS_LANTERN) {
            if (!wield_tool(obj, "rub")) return 0;
	    /* message from Adventure */
	    pline("Rubbing the electric lamp is not particularly rewarding.");
	    pline("Anyway, nothing exciting happens.");
        } else if (obj->otyp == OIL_LAMP) {
            if (!wield_tool(obj,"rub")) return 0;
            pline("Nothing happens.");
	} else pline("Nothing happens.");
	return 1;
}

int dojump(void)
{
	/* Physical jump */
	return jump(0);
}

int jump(
    int magic /* 0=Physical, otherwise skill level */
    )
{
	coord cc;

	if (!magic && (nolimbs(youmonst.data) || slithy(youmonst.data))) {
		/* normally (nolimbs || slithy) implies !Jumping,
		   but that isn't necessarily the case for knights */
		pline("You can't jump; you have no legs!");
		return 0;
	} else if (!magic && !Jumping) {
		pline("You can't jump very far.");
		return 0;
	} else if (u.uswallow) {
		if (magic) {
			pline("You bounce around a little.");
			return 1;
		}
		pline("You've got to be kidding!");
		return 0;
	} else if (u.uinwater) {
		if (magic) {
			pline("You swish around a little.");
			return 1;
		}
		pline("This calls for swimming, not jumping!");
		return 0;
	} else if (u.ustuck) {
		if (u.ustuck->mtame && !Conflict && !u.ustuck->mconf) {
		    pline("You pull free from %s.", mon_nam(u.ustuck));
		    u.ustuck = 0;
		    return 1;
		}
		if (magic) {
			pline("You writhe a little in the grasp of %s!", mon_nam(u.ustuck));
			return 1;
		}
		pline("You cannot escape from %s!", mon_nam(u.ustuck));
		return 0;
	} else if (Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)) {
		if (magic) {
			pline("You flail around a little.");
			return 1;
		}
		pline("You don't have enough traction to jump.");
		return 0;
	} else if (!magic && near_capacity() > UNENCUMBERED) {
		pline("You are carrying too much to jump!");
		return 0;
	} else if (!magic && (u.uhunger <= 100 || ACURR(A_STR) < 6)) {
		pline("You lack the strength to jump!");
		return 0;
	} else if (Wounded_legs) {
		long wl = (Wounded_legs & BOTH_SIDES);
		const char *bp = body_part(LEG);

		if (wl == BOTH_SIDES) bp = makeplural(bp);
		if (u.usteed)
		    pline("%s is in no shape for jumping.", Monnam(u.usteed));
		else
		    pline("Your %s%s %s in no shape for jumping.",
		     (wl == LEFT_SIDE) ? "left " :
			(wl == RIGHT_SIDE) ? "right " : "",
		     bp, (wl == BOTH_SIDES) ? "are" : "is");
		return 0;
	}
	else if (u.usteed && u.utrap) {
		pline("%s is stuck in a trap.", Monnam(u.usteed));
		return 0;
	}

	pline("Where do you want to jump?");
	cc.x = u.ux;
	cc.y = u.uy;
	if (getpos(&cc, TRUE, "the desired position") < 0)
		return 0;	/* user pressed ESC */
	if (!magic && !(HJumping & ~INTRINSIC) && !EJumping &&
			distu(cc.x, cc.y) != 5) {
		/* The Knight jumping restriction still applies when riding a
		 * horse.  After all, what shape is the knight piece in chess?
		 */
		pline("Illegal move!");
		return 0;
	} else if (distu(cc.x, cc.y) > (magic ? 6+magic*3 : 9)) {
		pline("Too far!");
		return 0;
	} else if (!cansee(cc.x, cc.y)) {
		pline("You cannot see where to land!");
		return 0;
	} else if (!isok(cc.x, cc.y)) {
		pline("You cannot jump there!");
		return 0;
	} else {
	    coord uc;
	    int range, temp;

	    if (u.utrap)
		switch(u.utraptype) {
		case TT_BEARTRAP: {
		    long side = rn2(3) ? LEFT_SIDE : RIGHT_SIDE;
		    pline("You rip yourself free of the bear trap!  Ouch!");
		    losehp(rnd(10), "jumping out of a bear trap", KILLED_BY);
		    set_wounded_legs(side, rn1(1000,500));
		    break;
		  }
		case TT_PIT:
		    pline("You leap from the pit!");
		    break;
		case TT_WEB:
		    pline("You tear the web apart as you pull yourself free!");
		    deltrap(t_at(level, u.ux,u.uy));
		    break;
		case TT_LAVA:
		    pline("You pull yourself above the lava!");
		    u.utrap = 0;
		    return 1;
		case TT_INFLOOR:
		    pline("You strain your %s, but you're still stuck in the floor.",
			makeplural(body_part(LEG)));
		    set_wounded_legs(LEFT_SIDE, rn1(10, 11));
		    set_wounded_legs(RIGHT_SIDE, rn1(10, 11));
		    return 1;
		}

	    /*
	     * Check the path from uc to cc, calling hurtle_step at each
	     * location.  The final position actually reached will be
	     * in cc.
	     */
	    uc.x = u.ux;
	    uc.y = u.uy;
	    /* calculate max(abs(dx), abs(dy)) as the range */
	    range = cc.x - uc.x;
	    if (range < 0) range = -range;
	    temp = cc.y - uc.y;
	    if (temp < 0) temp = -temp;
	    if (range < temp)
		range = temp;
	    walk_path(&uc, &cc, hurtle_step, &range);

	    /* A little Sokoban guilt... */
	    if (In_sokoban(&u.uz))
		change_luck(-1);

	    teleds(cc.x, cc.y, TRUE);
	    nomul(-1, "jumping around");
	    nomovemsg = "";
	    morehungry(rnd(25));
	    return 1;
	}
}

boolean tinnable(struct obj *corpse)
{
	if (corpse->oeaten) return 0;
	if (!mons[corpse->corpsenm].cnutrit) return 0;
	return 1;
}

static void use_tinning_kit(struct obj *obj)
{
	struct obj *corpse, *can;

	/* This takes only 1 move.  If this is to be changed to take many
	 * moves, we've got to deal with decaying corpses...
	 */
	if (obj->spe <= 0) {
		pline("You seem to be out of tins.");
		return;
	}
	if (!(corpse = floorfood("tin", 2))) return;
	if (corpse->oeaten) {
		pline("You cannot tin something which is partly eaten.");
		return;
	}
	if (touch_petrifies(&mons[corpse->corpsenm])
		&& !Stone_resistance && !uarmg) {
	    char kbuf[BUFSZ];

	    if (poly_when_stoned(youmonst.data))
		pline("You tin %s without wearing gloves.",
			an(mons[corpse->corpsenm].mname));
	    else {
		pline("Tinning %s without wearing gloves is a fatal mistake...",
			an(mons[corpse->corpsenm].mname));
		sprintf(kbuf, "trying to tin %s without gloves",
			an(mons[corpse->corpsenm].mname));
	    }
	    instapetrify(kbuf);
	}
	if (is_rider(&mons[corpse->corpsenm])) {
		revive_corpse(corpse);
		verbalize("Yes...  But War does not preserve its enemies...");
		return;
	}
	if (mons[corpse->corpsenm].cnutrit == 0) {
		pline("That's too insubstantial to tin.");
		return;
	}
	consume_obj_charge(obj, TRUE);

	if ((can = mksobj(level, TIN, FALSE, FALSE)) != 0) {
	    static const char you_buy_it[] = "You tin it, you bought it!";

	    can->corpsenm = corpse->corpsenm;
	    can->cursed = obj->cursed;
	    can->blessed = obj->blessed;
	    can->owt = weight(can);
	    can->known = 1;
	    can->spe = -1;  /* Mark tinned tins. No spinach allowed... */
	    if (carried(corpse)) {
		if (corpse->unpaid)
		    verbalize(you_buy_it);
		useup(corpse);
	    } else {
		if (costly_spot(corpse->ox, corpse->oy) && !corpse->no_charge)
		    verbalize(you_buy_it);
		useupf(corpse, 1L);
	    }
	    hold_another_object(can, "You make, but cannot pick up, %s.",
				doname(can), NULL);
	} else impossible("Tinning failed.");
}

void use_unicorn_horn(struct obj *obj)
{
#define PROP_COUNT 6		/* number of properties we're dealing with */
#define ATTR_COUNT (A_MAX*3)	/* number of attribute points we might fix */
	int idx, val, val_limit,
	    trouble_count, unfixable_trbl, did_prop, did_attr;
	int trouble_list[PROP_COUNT + ATTR_COUNT];

	if (obj && obj->cursed) {
	    long lcount = (long) rnd(100);

	    switch (rn2(6)) {
	    case 0: make_sick(Sick ? Sick/3L + 1L : (long)rn1(ACURR(A_CON),20),
			xname(obj), TRUE, SICK_NONVOMITABLE);
		    break;
	    case 1: make_blinded(Blinded + lcount, TRUE);
		    break;
	    case 2: if (!Confusion)
			pline("You suddenly feel %s.",
			    Hallucination ? "trippy" : "confused");
		    make_confused(HConfusion + lcount, TRUE);
		    break;
	    case 3: make_stunned(HStun + lcount, TRUE);
		    break;
	    case 4: adjattrib(rn2(A_MAX), -1, FALSE);
		    break;
	    case 5: make_hallucinated(HHallucination + lcount, TRUE, 0L);
		    break;
	    }
	    return;
	}

/*
 * Entries in the trouble list use a very simple encoding scheme.
 */
#define prop2trbl(X)	((X) + A_MAX)
#define attr2trbl(Y)	(Y)
#define prop_trouble(X) trouble_list[trouble_count++] = prop2trbl(X)
#define attr_trouble(Y) trouble_list[trouble_count++] = attr2trbl(Y)

	trouble_count = did_prop = did_attr = 0;

	/* collect property troubles */
	if (Sick) prop_trouble(SICK);
	if (Blinded > (long)u.ucreamed) prop_trouble(BLINDED);
	if (HHallucination) prop_trouble(HALLUC);
	if (Vomiting) prop_trouble(VOMITING);
	if (HConfusion) prop_trouble(CONFUSION);
	if (HStun) prop_trouble(STUNNED);

	unfixable_trbl = unfixable_trouble_count(TRUE);

	/* collect attribute troubles */
	for (idx = 0; idx < A_MAX; idx++) {
	    val_limit = AMAX(idx);
	    /* don't recover strength lost from hunger */
	    if (idx == A_STR && u.uhs >= WEAK) val_limit--;
	    /* don't recover more than 3 points worth of any attribute */
	    if (val_limit > ABASE(idx) + 3) val_limit = ABASE(idx) + 3;

	    for (val = ABASE(idx); val < val_limit; val++)
		attr_trouble(idx);
	    /* keep track of unfixed trouble, for message adjustment below */
	    unfixable_trbl += (AMAX(idx) - val_limit);
	}

	if (trouble_count == 0) {
	    pline("Nothing happens.");
	    return;
	} else if (trouble_count > 1) {		/* shuffle */
	    int i, j, k;

	    for (i = trouble_count - 1; i > 0; i--)
		if ((j = rn2(i + 1)) != i) {
		    k = trouble_list[j];
		    trouble_list[j] = trouble_list[i];
		    trouble_list[i] = k;
		}
	}

	/*
	 *		Chances for number of troubles to be fixed
	 *		 0	1      2      3      4	    5	   6	  7
	 *   blessed:  22.7%  22.7%  19.5%  15.4%  10.7%   5.7%   2.6%	 0.8%
	 *  uncursed:  35.4%  35.4%  22.9%   6.3%    0	    0	   0	  0
	 */
	val_limit = rn2( dice(2, (obj && obj->blessed) ? 4 : 2) );
	if (val_limit > trouble_count) val_limit = trouble_count;

	/* fix [some of] the troubles */
	for (val = 0; val < val_limit; val++) {
	    idx = trouble_list[val];

	    switch (idx) {
	    case prop2trbl(SICK):
		make_sick(0L, NULL, TRUE, SICK_ALL);
		did_prop++;
		break;
	    case prop2trbl(BLINDED):
		make_blinded((long)u.ucreamed, TRUE);
		did_prop++;
		break;
	    case prop2trbl(HALLUC):
		make_hallucinated(0L, TRUE, 0L);
		did_prop++;
		break;
	    case prop2trbl(VOMITING):
		make_vomiting(0L, TRUE);
		did_prop++;
		break;
	    case prop2trbl(CONFUSION):
		make_confused(0L, TRUE);
		did_prop++;
		break;
	    case prop2trbl(STUNNED):
		make_stunned(0L, TRUE);
		did_prop++;
		break;
	    default:
		if (idx >= 0 && idx < A_MAX) {
		    ABASE(idx) += 1;
		    did_attr++;
		} else
		    panic("use_unicorn_horn: bad trouble? (%d)", idx);
		break;
	    }
	}

	if (did_attr)
	    pline("This makes you feel %s!",
		  (did_prop + did_attr) == (trouble_count + unfixable_trbl) ?
		  "great" : "better");
	else if (!did_prop)
	    pline("Nothing seems to happen.");

	iflags.botl = (did_attr || did_prop);
#undef PROP_COUNT
#undef ATTR_COUNT
#undef prop2trbl
#undef attr2trbl
#undef prop_trouble
#undef attr_trouble
}

/*
 * Timer callback routine: turn figurine into monster
 */
void fig_transform(void *arg, long timeout)
{
	struct obj *figurine = (struct obj *)arg;
	struct monst *mtmp;
	coord cc;
	boolean cansee_spot, silent, okay_spot;
	boolean redraw = FALSE;
	char monnambuf[BUFSZ], carriedby[BUFSZ];

	if (!figurine)
	    return;

	silent = (timeout != moves); /* happened while away */
	okay_spot = get_obj_location(figurine, &cc.x, &cc.y, 0);
	if (figurine->where == OBJ_INVENT ||
	    figurine->where == OBJ_MINVENT)
		okay_spot = enexto(&cc, level, cc.x, cc.y,
				   &mons[figurine->corpsenm]);
	if (!okay_spot ||
	    !figurine_location_checks(figurine,&cc, TRUE)) {
		/* reset the timer to try again later */
		start_timer(figurine->olev, (long)rnd(5000), TIMER_OBJECT,
				FIG_TRANSFORM, figurine);
		return;
	}

	cansee_spot = cansee(cc.x, cc.y);
	mtmp = make_familiar(figurine, cc.x, cc.y, TRUE);
	if (mtmp) {
	    sprintf(monnambuf, "%s",an(m_monnam(mtmp)));
	    switch (figurine->where) {
		case OBJ_INVENT:
		    if (Blind)
			pline("You feel something %s from your pack!",
			    locomotion(mtmp->data,"drop"));
		    else
			pline("You see %s %s out of your pack!",
			    monnambuf,
			    locomotion(mtmp->data,"drop"));
		    break;

		case OBJ_FLOOR:
		    if (cansee_spot && !silent) {
			pline("You suddenly see a figurine transform into %s!",
				monnambuf);
			redraw = TRUE;	/* update figurine's map location */
		    }
		    break;

		case OBJ_MINVENT:
		    if (cansee_spot && !silent) {
			struct monst *mon;
			mon = figurine->ocarry;
			/* figurine carring monster might be invisible */
			if (canseemon(figurine->ocarry)) {
			    sprintf(carriedby, "%s pack",
				     s_suffix(a_monnam(mon)));
			}
			else if (is_pool(level, mon->mx, mon->my))
			    strcpy(carriedby, "empty water");
			else
			    strcpy(carriedby, "thin air");
			pline("You see %s %s out of %s!", monnambuf,
			    locomotion(mtmp->data, "drop"), carriedby);
		    }
		    break;

		default:
		    impossible("figurine came to life where? (%d)",
				(int)figurine->where);
		break;
	    }
	}
	/* free figurine now */
	obj_extract_self(figurine);
	obfree(figurine, NULL);
	if (redraw) newsym(cc.x, cc.y);
}


static boolean figurine_location_checks(struct obj *obj, coord *cc, boolean quietly)
{
	xchar x,y;

	if (carried(obj) && u.uswallow) {
		if (!quietly)
			pline("You don't have enough room in here.");
		return FALSE;
	}
	x = cc->x; y = cc->y;
	if (!isok(x,y)) {
		if (!quietly)
			pline("You cannot put the figurine there.");
		return FALSE;
	}
	if (IS_ROCK(level->locations[x][y].typ) &&
	    !(passes_walls(&mons[obj->corpsenm]) && may_passwall(level, x,y))) {
		if (!quietly)
		    pline("You cannot place a figurine in %s!",
			IS_TREE(level->locations[x][y].typ) ? "a tree" : "solid rock");
		return FALSE;
	}
	if (sobj_at(BOULDER, level, x,y) && !passes_walls(&mons[obj->corpsenm])
			&& !throws_rocks(&mons[obj->corpsenm])) {
		if (!quietly)
			pline("You cannot fit the figurine on the boulder.");
		return FALSE;
	}
	return TRUE;
}


static boolean use_figurine(struct obj *obj)
{
	xchar x, y;
	coord cc;
	schar dx, dy, dz;

	if (u.uswallow) {
		/* can't activate a figurine while swallowed */
		if (!figurine_location_checks(obj, NULL, FALSE))
			return FALSE;
	}
	if (!getdir(NULL, &dx, &dy, &dz)) {
		flags.move = multi = 0;
		return FALSE;
	}
	
	x = u.ux + dx;
	y = u.uy + dy;
	cc.x = x;
	cc.y = y;
	
	/* Passing FALSE arg here will result in messages displayed */
	if (!figurine_location_checks(obj, &cc, FALSE))
	    return FALSE;
	pline("You %s and it transforms.",
	    (dx || dy) ? "set the figurine beside you" :
	    (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
	     is_pool(level, cc.x, cc.y)) ?
		"release the figurine" :
	    (dz < 0 ?
		"toss the figurine into the air" :
		"set the figurine on the ground"));
	make_familiar(obj, cc.x, cc.y, FALSE);
	stop_timer(obj->olev, FIG_TRANSFORM, obj);
	useup(obj);
	
	return TRUE;
}

static const char lubricables[] = { ALL_CLASSES, ALLOW_NONE, 0 };
static const char need_to_remove_outer_armor[] =
			"You need to remove your %s to grease your %s.";

static void use_grease(struct obj *obj)
{
	struct obj *otmp;
	char buf[BUFSZ];

	if (Glib) {
	    pline("%s from your %s.", Tobjnam(obj, "slip"),
		  makeplural(body_part(FINGER)));
	    dropx(obj);
	    return;
	}

	if (obj->spe > 0) {
		if ((obj->cursed || Fumbling) && !rn2(2)) {
			consume_obj_charge(obj, TRUE);

			pline("%s from your %s.", Tobjnam(obj, "slip"),
			      makeplural(body_part(FINGER)));
			dropx(obj);
			return;
		}
		otmp = getobj(lubricables, "grease");
		if (!otmp) return;
		if ((otmp->owornmask & WORN_ARMOR) && uarmc) {
			strcpy(buf, xname(uarmc));
			pline(need_to_remove_outer_armor, buf, xname(otmp));
			return;
		}
		if ((otmp->owornmask & WORN_SHIRT) && (uarmc || uarm)) {
			strcpy(buf, uarmc ? xname(uarmc) : "");
			if (uarmc && uarm) strcat(buf, " and ");
			strcat(buf, uarm ? xname(uarm) : "");
			pline(need_to_remove_outer_armor, buf, xname(otmp));
			return;
		}
		consume_obj_charge(obj, TRUE);

		if (otmp != &zeroobj) {
			pline("You cover %s with a thick layer of grease.",
			    yname(otmp));
			otmp->greased = 1;
			if (obj->cursed && !nohands(youmonst.data)) {
			    incr_itimeout(&Glib, rnd(15));
			    pline("Some of the grease gets all over your %s.",
				makeplural(body_part(HAND)));
			}
		} else {
			Glib += rnd(15);
			pline("You coat your %s with grease.",
			    makeplural(body_part(FINGER)));
		}
	} else {
	    if (obj->known)
		pline("%s empty.", Tobjnam(obj, "are"));
	    else
		pline("%s to be empty.", Tobjnam(obj, "seem"));
	}
	update_inventory();
}

static struct trapinfo {
	struct obj *tobj;
	xchar tx, ty;
	int time_needed;
	boolean force_bungle;
} trapinfo;

void reset_trapset(void)
{
	trapinfo.tobj = 0;
	trapinfo.force_bungle = 0;
}

/* touchstones - by Ken Arnold */
static void use_stone(struct obj *tstone)
{
    struct obj *obj;
    boolean do_scratch;
    const char *streak_color, *choices;
    char stonebuf[QBUFSZ];
    static const char scritch[] = "\"scritch, scritch\"";
    static const char allowall[3] = { ALL_CLASSES, 0 };
    static const char justgems[3] = { ALLOW_NONE, GEM_CLASS, 0 };

    /* in case it was acquired while blinded */
    if (!Blind) tstone->dknown = 1;
    /* when the touchstone is fully known, don't bother listing extra
       junk as likely candidates for rubbing */
    choices = (tstone->otyp == TOUCHSTONE && tstone->dknown &&
		objects[TOUCHSTONE].oc_name_known) ? justgems : allowall;
    sprintf(stonebuf, "rub on the stone%s", plur(tstone->quan));
    if ((obj = getobj(choices, stonebuf)) == 0)
	return;

    if (obj == tstone && obj->quan == 1) {
	pline("You can't rub %s on itself.", the(xname(obj)));
	return;
    }

    if (tstone->otyp == TOUCHSTONE && tstone->cursed &&
	    obj->oclass == GEM_CLASS && !is_graystone(obj) &&
	    !obj_resists(obj, 80, 100)) {
	if (Blind)
	    pline("You feel something shatter.");
	else if (Hallucination)
	    pline("Oh, wow, look at the pretty shards.");
	else
	    pline("A sharp crack shatters %s%s.",
		  (obj->quan > 1) ? "one of " : "", the(xname(obj)));
	useup(obj);
	return;
    }

    if (Blind) {
	pline(scritch);
	return;
    } else if (Hallucination) {
	pline("Oh wow, man: Fractals!");
	return;
    }

    do_scratch = FALSE;
    streak_color = 0;

    switch (obj->oclass) {
    case GEM_CLASS:	/* these have class-specific handling below */
    case RING_CLASS:
	if (tstone->otyp != TOUCHSTONE) {
	    do_scratch = TRUE;
	} else if (obj->oclass == GEM_CLASS && (tstone->blessed ||
		(!tstone->cursed &&
		    (Role_if (PM_ARCHEOLOGIST) || Race_if(PM_GNOME))))) {
	    makeknown(TOUCHSTONE);
	    makeknown(obj->otyp);
	    prinv(NULL, obj, 0L);
	    return;
	} else {
	    /* either a ring or the touchstone was not effective */
	    if (objects[obj->otyp].oc_material == GLASS) {
		do_scratch = TRUE;
		break;
	    }
	}
	streak_color = c_obj_colors[objects[obj->otyp].oc_color];
	break;		/* gem or ring */

    default:
	switch (objects[obj->otyp].oc_material) {
	case CLOTH:
	    pline("%s a little more polished now.", Tobjnam(tstone, "look"));
	    return;
	case LIQUID:
	    if (!obj->known)		/* note: not "whetstone" */
		pline("You must think this is a wetstone, do you?");
	    else
		pline("%s a little wetter now.", Tobjnam(tstone, "are"));
	    return;
	case WAX:
	    streak_color = "waxy";
	    break;		/* okay even if not touchstone */
	case WOOD:
	    streak_color = "wooden";
	    break;		/* okay even if not touchstone */
	case GOLD:
	    do_scratch = TRUE;	/* scratching and streaks */
	    streak_color = "golden";
	    break;
	case SILVER:
	    do_scratch = TRUE;	/* scratching and streaks */
	    streak_color = "silvery";
	    break;
	default:
	    /* Objects passing the is_flimsy() test will not
	       scratch a stone.  They will leave streaks on
	       non-touchstones and touchstones alike. */
	    if (is_flimsy(obj))
		streak_color = c_obj_colors[objects[obj->otyp].oc_color];
	    else
		do_scratch = (tstone->otyp != TOUCHSTONE);
	    break;
	}
	break;		/* default oclass */
    }

    sprintf(stonebuf, "stone%s", plur(tstone->quan));
    if (do_scratch)
	pline("You make %s%sscratch marks on the %s.",
	      streak_color ? streak_color : (const char *)"",
	      streak_color ? " " : "", stonebuf);
    else if (streak_color)
	pline("You see %s streaks on the %s.", streak_color, stonebuf);
    else
	pline(scritch);
    return;
}

/* Place a landmine/bear trap.  Helge Hafting */
static void use_trap(struct obj *otmp)
{
	int ttyp, tmp;
	const char *what = NULL;
	char buf[BUFSZ];
	const char *occutext = "setting the trap";

	if (nohands(youmonst.data))
	    what = "without hands";
	else if (Stunned)
	    what = "while stunned";
	else if (u.uswallow)
	    what = is_animal(u.ustuck->data) ? "while swallowed" :
			"while engulfed";
	else if (Underwater)
	    what = "underwater";
	else if (Levitation)
	    what = "while levitating";
	else if (is_pool(level, u.ux, u.uy))
	    what = "in water";
	else if (is_lava(level, u.ux, u.uy))
	    what = "in lava";
	else if (On_stairs(u.ux, u.uy))
	    what = (u.ux == level->dnladder.sx || u.ux == level->upladder.sx) ?
			"on the ladder" : "on the stairs";
	else if (IS_FURNITURE(level->locations[u.ux][u.uy].typ) ||
		IS_ROCK(level->locations[u.ux][u.uy].typ) ||
		closed_door(level, u.ux, u.uy) || t_at(level, u.ux, u.uy))
	    what = "here";
	if (what) {
	    pline("You can't set a trap %s!",what);
	    reset_trapset();
	    return;
	}
	ttyp = (otmp->otyp == LAND_MINE) ? LANDMINE : BEAR_TRAP;
	if (otmp == trapinfo.tobj &&
		u.ux == trapinfo.tx && u.uy == trapinfo.ty) {
	    pline("You resume setting %s %s.",
		shk_your(buf, otmp),
		trapexplain[what_trap(ttyp)-1]);
	    set_occupation(set_trap, occutext, 0);
	    return;
	}
	trapinfo.tobj = otmp;
	trapinfo.tx = u.ux,  trapinfo.ty = u.uy;
	tmp = ACURR(A_DEX);
	trapinfo.time_needed = (tmp > 17) ? 2 : (tmp > 12) ? 3 :
				(tmp > 7) ? 4 : 5;
	if (Blind) trapinfo.time_needed *= 2;
	tmp = ACURR(A_STR);
	if (ttyp == BEAR_TRAP && tmp < 18)
	    trapinfo.time_needed += (tmp > 12) ? 1 : (tmp > 7) ? 2 : 4;
	/*[fumbling and/or confusion and/or cursed object check(s)
	   should be incorporated here instead of in set_trap]*/
	if (u.usteed && P_SKILL(P_RIDING) < P_BASIC) {
	    boolean chance;

	    if (Fumbling || otmp->cursed) chance = (rnl(10) > 3);
	    else  chance = (rnl(10) > 5);
	    pline("You aren't very skilled at reaching from %s.",
		mon_nam(u.usteed));
	    sprintf(buf, "Continue your attempt to set %s?",
		the(trapexplain[what_trap(ttyp)-1]));
	    if (yn(buf) == 'y') {
		if (chance) {
			switch(ttyp) {
			    case LANDMINE:	/* set it off */
			    	trapinfo.time_needed = 0;
			    	trapinfo.force_bungle = TRUE;
				break;
			    case BEAR_TRAP:	/* drop it without arming it */
				reset_trapset();
				pline("You drop %s!",
			  the(trapexplain[what_trap(ttyp)-1]));
				dropx(otmp);
				return;
			}
		}
	    } else {
	    	reset_trapset();
		return;
	    }
	}
	
	pline("You begin setting %s %s.",
	    shk_your(buf, otmp),
	    trapexplain[what_trap(ttyp)-1]);
	set_occupation(set_trap, occutext, 0);
	return;
}


static int set_trap(void)
{
	struct obj *otmp = trapinfo.tobj;
	struct trap *ttmp;
	int ttyp;

	if (!otmp || !carried(otmp) ||
		u.ux != trapinfo.tx || u.uy != trapinfo.ty) {
	    /* ?? */
	    reset_trapset();
	    return 0;
	}

	if (--trapinfo.time_needed > 0) return 1;	/* still busy */

	ttyp = (otmp->otyp == LAND_MINE) ? LANDMINE : BEAR_TRAP;
	ttmp = maketrap(level, u.ux, u.uy, ttyp);
	if (ttmp) {
	    ttmp->tseen = 1;
	    ttmp->madeby_u = 1;
	    newsym(u.ux, u.uy); /* if our hero happens to be invisible */
	    if (*in_rooms(level, u.ux, u.uy, SHOPBASE)) {
		add_damage(u.ux, u.uy, 0L);		/* schedule removal */
	    }
	    if (!trapinfo.force_bungle)
		pline("You finish arming %s.",
			the(trapexplain[what_trap(ttyp)-1]));
	    if (((otmp->cursed || Fumbling) && (rnl(10) > 5)) || trapinfo.force_bungle)
		dotrap(ttmp,
			(unsigned)(trapinfo.force_bungle ? FORCEBUNGLE : 0));
	} else {
	    /* this shouldn't happen */
	    pline("Your trap setting attempt fails.");
	}
	useup(otmp);
	reset_trapset();
	return 0;
}


static int use_whip(struct obj *obj)
{
    char buf[BUFSZ];
    struct monst *mtmp;
    struct obj *otmp;
    int rx, ry, proficient, res = 0;
    const char *msg_slipsfree = "The bullwhip slips free.";
    const char *msg_snap = "Snap!";
    schar dx, dy, dz;

    if (obj != uwep) {
	if (!wield_tool(obj, "lash")) return 0;
	else res = 1;
    }
    
    if (!getdir(NULL, &dx, &dy, &dz))
	return res;

    if (Stunned || (Confusion && !rn2(5)))
	confdir(&dx, &dy);
    rx = u.ux + dx;
    ry = u.uy + dy;
    mtmp = m_at(level, rx, ry);

    /* fake some proficiency checks */
    proficient = 0;
    if (Role_if (PM_ARCHEOLOGIST)) ++proficient;
    if (ACURR(A_DEX) < 6) proficient--;
    else if (ACURR(A_DEX) >= 14) proficient += (ACURR(A_DEX) - 14);
    if (Fumbling) --proficient;
    if (proficient > 3) proficient = 3;
    if (proficient < 0) proficient = 0;

    if (u.uswallow && attack(u.ustuck, dx, dy)) {
	pline("There is not enough room to flick your bullwhip.");

    } else if (Underwater) {
	pline("There is too much resistance to flick your bullwhip.");

    } else if (dz < 0) {
	pline("You flick a bug off of the %s.",ceiling(u.ux,u.uy));

    } else if ((!dx && !dy) || (dz > 0)) {
	int dam;

	/* Sometimes you hit your steed by mistake */
	if (u.usteed && !rn2(proficient + 2)) {
	    pline("You whip %s!", mon_nam(u.usteed));
	    kick_steed();
	    return 1;
	}

	if (Levitation || u.usteed) {
	    /* Have a shot at snaring something on the floor */
	    otmp = level->objects[u.ux][u.uy];
	    if (otmp && otmp->otyp == CORPSE && otmp->corpsenm == PM_HORSE) {
		pline("Why beat a dead horse?");
		return 1;
	    }
	    if (otmp && proficient) {
		pline("You wrap your bullwhip around %s on the %s.",
		    an(singular(otmp, xname)), surface(u.ux, u.uy));
		if (rnl(6) || pickup_object(otmp, 1L, TRUE) < 1)
		    pline(msg_slipsfree);
		return 1;
	    }
	}
	dam = rnd(2) + dbon() + obj->spe;
	if (dam <= 0)
	    dam = 1;
	pline("You hit your %s with your bullwhip.", body_part(FOOT));
	sprintf(buf, "killed %sself with %s bullwhip", uhim(), uhis());
	losehp(dam, buf, NO_KILLER_PREFIX);
	iflags.botl = 1;
	return 1;

    } else if ((Fumbling || Glib) && !rn2(5)) {
	pline("The bullwhip slips out of your %s.", body_part(HAND));
	dropx(obj);

    } else if (u.utrap && u.utraptype == TT_PIT) {
	/*
	 *     Assumptions:
	 *
	 *	if you're in a pit
	 *		- you are attempting to get out of the pit
	 *		- or, if you are applying it towards a small
	 *		  monster then it is assumed that you are
	 *		  trying to hit it.
	 *	else if the monster is wielding a weapon
	 *		- you are attempting to disarm a monster
	 *	else
	 *		- you are attempting to hit the monster
	 *
	 *	if you're confused (and thus off the mark)
	 *		- you only end up hitting.
	 *
	 */
	const char *wrapped_what = NULL;

	if (mtmp) {
	    if (bigmonst(mtmp->data)) {
		wrapped_what = strcpy(buf, mon_nam(mtmp));
	    } else if (proficient) {
		if (attack(mtmp, dx, dy)) return 1;
		else pline(msg_snap);
	    }
	}
	if (!wrapped_what) {
	    if (IS_FURNITURE(level->locations[rx][ry].typ))
		wrapped_what = "something";
	    else if (sobj_at(BOULDER, level, rx, ry))
		wrapped_what = "a boulder";
	}
	if (wrapped_what) {
	    coord cc;

	    cc.x = rx; cc.y = ry;
	    pline("You wrap your bullwhip around %s.", wrapped_what);
	    if (proficient && rn2(proficient + 2)) {
		if (!mtmp || enexto(&cc, level, rx, ry, youmonst.data)) {
		    pline("You yank yourself out of the pit!");
		    teleds(cc.x, cc.y, TRUE);
		    u.utrap = 0;
		    vision_full_recalc = 1;
		}
	    } else {
		pline(msg_slipsfree);
	    }
	    if (mtmp) wakeup(mtmp);
	} else pline(msg_snap);

    } else if (mtmp) {
	if (!canspotmon(mtmp) &&
		!level->locations[rx][ry].mem_invis) {
	   pline("A monster is there that you couldn't see.");
	   map_invisible(rx, ry);
	}
	otmp = MON_WEP(mtmp);	/* can be null */
	if (otmp) {
	    char onambuf[BUFSZ];
	    const char *mon_hand;
	    boolean gotit = proficient && (!Fumbling || !rn2(10));

	    strcpy(onambuf, cxname(otmp));
	    if (gotit) {
		mon_hand = mbodypart(mtmp, HAND);
		if (bimanual(otmp)) mon_hand = makeplural(mon_hand);
	    } else
		mon_hand = 0;	/* lint suppression */

	    pline("You wrap your bullwhip around %s %s.",
		s_suffix(mon_nam(mtmp)), onambuf);
	    if (gotit && otmp->cursed) {
		pline("%s welded to %s %s%c",
		      (otmp->quan == 1L) ? "It is" : "They are",
		      mhis(mtmp), mon_hand,
		      !otmp->bknown ? '!' : '.');
		otmp->bknown = 1;
		gotit = FALSE;	/* can't pull it free */
	    }
	    if (gotit) {
		obj_extract_self(otmp);
		possibly_unwield(mtmp, FALSE);
		setmnotwielded(mtmp,otmp);

		switch (rn2(proficient + 1)) {
		case 2:
		    /* to floor near you */
		    pline("You yank %s %s to the %s!", s_suffix(mon_nam(mtmp)),
			onambuf, surface(u.ux, u.uy));
		    place_object(otmp, level, u.ux, u.uy);
		    stackobj(otmp);
		    break;
		case 3:
		    /* right to you */
		    /* right into your inventory */
		    pline("You snatch %s %s!", s_suffix(mon_nam(mtmp)), onambuf);
		    if (otmp->otyp == CORPSE &&
			    touch_petrifies(&mons[otmp->corpsenm]) &&
			    !uarmg && !Stone_resistance &&
			    !(poly_when_stoned(youmonst.data) &&
				polymon(PM_STONE_GOLEM))) {
			char kbuf[BUFSZ];

			sprintf(kbuf, "%s corpse",
				an(mons[otmp->corpsenm].mname));
			pline("Snatching %s is a fatal mistake.", kbuf);
			instapetrify(kbuf);
		    }
		    hold_another_object(otmp, "You drop %s!",
					       doname(otmp), NULL);
		    break;
		default:
		    /* to floor beneath mon */
		    pline("You yank %s from %s %s!", the(onambuf),
			s_suffix(mon_nam(mtmp)), mon_hand);
		    obj_no_longer_held(otmp);
		    place_object(otmp, level, mtmp->mx, mtmp->my);
		    stackobj(otmp);
		    break;
		}
	    } else {
		pline(msg_slipsfree);
	    }
	    wakeup(mtmp);
	} else {
	    if (mtmp->m_ap_type &&
		!Protection_from_shape_changers && !sensemon(mtmp))
		stumble_onto_mimic(mtmp, dx, dy);
	    else pline("You flick your bullwhip towards %s.", mon_nam(mtmp));
	    if (proficient) {
		if (attack(mtmp, dx, dy))
		    return 1;
		else
		    pline(msg_snap);
	    }
	}

    } else if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)) {
	    /* it must be air -- water checked above */
	    pline("You snap your whip through thin air.");

    } else {
	pline(msg_snap);

    }
    return 1;
}


static const char
	not_enough_room[] = "There's not enough room here to use that.",
	where_to_hit[] = "Where do you want to hit?",
	cant_see_spot[] = "You won't hit anything if you can't see that spot.",
	cant_reach[] = "You can't reach that spot from here.";

/* Distance attacks by pole-weapons */
static int use_pole (struct obj *obj)
{
	int res = 0, typ, max_range = 4, min_range = 4;
	coord cc;
	struct monst *mtmp;


	/* Are you allowed to use the pole? */
	if (u.uswallow) {
	    pline(not_enough_room);
	    return 0;
	}
	if (obj != uwep) {
	    if (!wield_tool(obj, "swing")) return 0;
	    else res = 1;
	}
     /* assert(obj == uwep); */

	/* Prompt for a location */
	pline(where_to_hit);
	cc.x = u.ux;
	cc.y = u.uy;
	if (getpos(&cc, TRUE, "the spot to hit") < 0)
	    return 0;	/* user pressed ESC */

	/* Calculate range */
	typ = uwep_skill_type();
	if (typ == P_NONE || P_SKILL(typ) <= P_BASIC) max_range = 4;
	else if (P_SKILL(typ) == P_SKILLED) max_range = 5;
	else max_range = 8;
	if (distu(cc.x, cc.y) > max_range) {
	    pline("Too far!");
	    return res;
	} else if (distu(cc.x, cc.y) < min_range) {
	    pline("Too close!");
	    return res;
	} else if (!cansee(cc.x, cc.y) &&
		   ((mtmp = m_at(level, cc.x, cc.y)) == NULL ||
		    !canseemon(mtmp))) {
	    pline(cant_see_spot);
	    return res;
	} else if (!couldsee(cc.x, cc.y)) { /* Eyes of the Overworld */
	    pline(cant_reach);
	    return res;
	}

	/* Attack the monster there */
	if ((mtmp = m_at(level, cc.x, cc.y)) != NULL) {
	    int oldhp = mtmp->mhp;

	    bhitpos = cc;
	    check_caitiff(mtmp);
	    thitmonst(mtmp, uwep);
	    /* check the monster's HP because thitmonst() doesn't return
	     * an indication of whether it hit.  Not perfect (what if it's a
	     * non-silver weapon on a shade?)
	     */
	    if (mtmp->mhp < oldhp)
		u.uconduct.weaphit++;
	} else
	    /* Now you know that nothing is there... */
	    pline("Nothing happens.");
	return 1;
}


static int use_cream_pie(struct obj *obj)
{
	boolean wasblind = Blind;
	boolean wascreamed = u.ucreamed;
	boolean several = FALSE;

	if (obj->quan > 1L) {
		several = TRUE;
		obj = splitobj(obj, 1L);
	}
	if (Hallucination)
		pline("You give yourself a facial.");
	else
		pline("You immerse your %s in %s%s.", body_part(FACE),
			several ? "one of " : "",
			several ? makeplural(the(xname(obj))) : the(xname(obj)));
	if (can_blnd(NULL, &youmonst, AT_WEAP, obj)) {
		int blindinc = rnd(25);
		u.ucreamed += blindinc;
		make_blinded(Blinded + (long)blindinc, FALSE);
		if (!Blind || (Blind && wasblind))
			pline("There's %ssticky goop all over your %s.",
				wascreamed ? "more " : "",
				body_part(FACE));
		else /* Blind  && !wasblind */
			pline("You can't see through all the sticky goop on your %s.",
				body_part(FACE));
	}
	if (obj->unpaid) {
		verbalize("You used it, you bought it!");
		bill_dummy_object(obj);
	}
	obj_extract_self(obj);
	delobj(obj);
	return 0;
}


static int use_grapple (struct obj *obj)
{
	int res = 0, typ, max_range = 4, tohit;
	coord cc;
	struct monst *mtmp;
	struct obj *otmp;

	/* Are you allowed to use the hook? */
	if (u.uswallow) {
	    pline(not_enough_room);
	    return 0;
	}
	if (obj != uwep) {
	    if (!wield_tool(obj, "cast")) return 0;
	    else res = 1;
	}
     /* assert(obj == uwep); */

	/* Prompt for a location */
	pline(where_to_hit);
	cc.x = u.ux;
	cc.y = u.uy;
	if (getpos(&cc, TRUE, "the spot to hit") < 0)
	    return 0;	/* user pressed ESC */

	/* Calculate range */
	typ = uwep_skill_type();
	if (typ == P_NONE || P_SKILL(typ) <= P_BASIC) max_range = 4;
	else if (P_SKILL(typ) == P_SKILLED) max_range = 5;
	else max_range = 8;
	if (distu(cc.x, cc.y) > max_range) {
	    pline("Too far!");
	    return res;
	} else if (!cansee(cc.x, cc.y)) {
	    pline(cant_see_spot);
	    return res;
	} else if (!couldsee(cc.x, cc.y)) { /* Eyes of the Overworld */
	    pline(cant_reach);
	    return res;
	}

	/* What do you want to hit? */
	tohit = rn2(5);
	if (typ != P_NONE && P_SKILL(typ) >= P_SKILLED) {
	    struct nh_menuitem items[3];
	    int selected[3];
	    
	    sprintf(items[0].caption, "an object on the %s", surface(cc.x, cc.y));
	    set_menuitem(&items[0], 1, MI_NORMAL, "", 0, FALSE);
	    
	    set_menuitem(&items[1], 2, MI_NORMAL, "a monster", 0, FALSE);
	    
	    sprintf(items[2].caption, "the %s", surface(cc.x, cc.y));
	    set_menuitem(&items[2], 3, MI_NORMAL, "a monster", 0, FALSE);
	    
	    if (display_menu(items, 3, "Aim for what?", PICK_ONE, selected)&&
			rn2(P_SKILL(typ) > P_SKILLED ? 20 : 2))
		tohit = selected[0] - 1;
	}

	/* What did you hit? */
	switch (tohit) {
	case 0:	/* Trap */
	    /* FIXME -- untrap needs to deal with non-adjacent traps */
	    break;
	case 1:	/* Object */
	    if ((otmp = level->objects[cc.x][cc.y]) != 0) {
		pline("You snag an object from the %s!", surface(cc.x, cc.y));
		pickup_object(otmp, 1L, FALSE);
		/* If pickup fails, leave it alone */
		newsym(cc.x, cc.y);
		return 1;
	    }
	    break;
	case 2:	/* Monster */
	    if ((mtmp = m_at(level, cc.x, cc.y)) == NULL) break;
	    if (verysmall(mtmp->data) && !rn2(4) &&
			enexto(&cc, level, u.ux, u.uy, NULL)) {
		pline("You pull in %s!", mon_nam(mtmp));
		mtmp->mundetected = 0;
		rloc_to(mtmp, cc.x, cc.y);
		return 1;
	    } else if ((!bigmonst(mtmp->data) && !strongmonst(mtmp->data)) ||
		       rn2(4)) {
		thitmonst(mtmp, uwep);
		return 1;
	    }
	    /* FALL THROUGH */
	case 3:	/* Surface */
	    if (IS_AIR(level->locations[cc.x][cc.y].typ) || is_pool(level, cc.x, cc.y))
		pline("The hook slices through the %s.", surface(cc.x, cc.y));
	    else {
		pline("You are yanked toward the %s!", surface(cc.x, cc.y));
		hurtle(sgn(cc.x-u.ux), sgn(cc.y-u.uy), 1, FALSE);
		spoteffects(TRUE);
	    }
	    return 1;
	default:	/* Yourself (oops!) */
	    if (P_SKILL(typ) <= P_BASIC) {
		pline("You hook yourself!");
		losehp(rn1(10,10), "a grappling hook", KILLED_BY);
		return 1;
	    }
	    break;
	}
	pline("Nothing happens.");
	return 1;
}


#define BY_OBJECT	(NULL)

/* return 1 if the wand is broken, hence some time elapsed */
int do_break_wand(struct obj *obj)
{
    static const char nothing_else_happens[] = "But nothing else happens...";
    int i, x, y;
    struct monst *mon;
    int dmg, damage;
    boolean affects_objects;
    boolean shop_damage = FALSE;
    int expltype = EXPL_MAGICAL;
    char confirm[QBUFSZ], the_wand[BUFSZ], buf[BUFSZ];

    strcpy(the_wand, yname(obj));
    sprintf(confirm, "Are you really sure you want to break %s?",
	safe_qbuf("", sizeof("Are you really sure you want to break ?"),
				the_wand, ysimple_name(obj), "the wand"));
    if (yn(confirm) == 'n' ) return 0;

    if (nohands(youmonst.data)) {
	pline("You can't break %s without hands!", the_wand);
	return 0;
    } else if (ACURR(A_STR) < 10) {
	pline("You don't have the strength to break %s!", the_wand);
	return 0;
    }
    pline("Raising %s high above your %s, you break it in two!",
	  the_wand, body_part(HEAD));

    /* [ALI] Do this first so that wand is removed from bill. Otherwise,
     * the freeinv() below also hides it from setpaid() which causes problems.
     */
    if (obj->unpaid) {
	check_unpaid(obj);		/* Extra charge for use */
	bill_dummy_object(obj);
    }

    current_wand = obj;		/* destroy_item might reset this */
    freeinv(obj);		/* hide it from destroy_item instead... */
    setnotworn(obj);		/* so we need to do this ourselves */

    if (obj->spe <= 0) {
	pline(nothing_else_happens);
	goto discard_broken_wand;
    }
    obj->ox = u.ux;
    obj->oy = u.uy;
    dmg = obj->spe * 4;
    affects_objects = FALSE;

    switch (obj->otyp) {
    case WAN_WISHING:
    case WAN_NOTHING:
    case WAN_LOCKING:
    case WAN_PROBING:
    case WAN_ENLIGHTENMENT:
    case WAN_OPENING:
    case WAN_SECRET_DOOR_DETECTION:
	pline(nothing_else_happens);
	goto discard_broken_wand;
    case WAN_DEATH:
    case WAN_LIGHTNING:
	dmg *= 4;
	goto wanexpl;
    case WAN_FIRE:
	expltype = EXPL_FIERY;
    case WAN_COLD:
	if (expltype == EXPL_MAGICAL) expltype = EXPL_FROSTY;
	dmg *= 2;
    case WAN_MAGIC_MISSILE:
    wanexpl:
	explode(u.ux, u.uy,
		(obj->otyp - WAN_MAGIC_MISSILE), dmg, WAND_CLASS, expltype);
	makeknown(obj->otyp);	/* explode described the effect */
	goto discard_broken_wand;
    case WAN_STRIKING:
	/* we want this before the explosion instead of at the very end */
	pline("A wall of force smashes down around you!");
	dmg = dice(1 + obj->spe,6);	/* normally 2d12 */
    case WAN_CANCELLATION:
    case WAN_POLYMORPH:
    case WAN_TELEPORTATION:
    case WAN_UNDEAD_TURNING:
	affects_objects = TRUE;
	break;
    default:
	break;
    }

    /* magical explosion and its visual effect occur before specific effects */
    explode(obj->ox, obj->oy, 0, rnd(dmg), WAND_CLASS, EXPL_MAGICAL);

    /* this makes it hit us last, so that we can see the action first */
    for (i = 0; i <= 8; i++) {
	bhitpos.x = x = obj->ox + xdir[i];
	bhitpos.y = y = obj->oy + ydir[i];
	if (!isok(x,y)) continue;

	if (obj->otyp == WAN_DIGGING) {
	    if (dig_check(BY_OBJECT, FALSE, x, y)) {
		if (IS_WALL(level->locations[x][y].typ) ||
		    IS_DOOR(level->locations[x][y].typ)) {
		    /* normally, pits and holes don't anger guards, but they
		     * do if it's a wall or door that's being dug */
		    watch_dig(NULL, x, y, TRUE);
		    if (*in_rooms(level, x, y, SHOPBASE)) shop_damage = TRUE;
		}		    
		digactualhole(x, y, BY_OBJECT,
			      (rn2(obj->spe) < 3 || !can_dig_down(level)) ?
			       PIT : HOLE);
	    }
	    continue;
	} else if (obj->otyp == WAN_CREATE_MONSTER) {
	    /* u.ux,u.uy creates it near you--x,y might create it in rock */
	    makemon(NULL, level, u.ux, u.uy, NO_MM_FLAGS);
	    continue;
	} else {
	    if (x == u.ux && y == u.uy) {
		/* teleport objects first to avoid race with tele control and
		   autopickup.  Other wand/object effects handled after
		   possible wand damage is assessed */
		if (obj->otyp == WAN_TELEPORTATION &&
		    affects_objects && level->objects[x][y]) {
		    bhitpile(obj, bhito, x, y);
		    if (iflags.botl) bot();		/* potion effects */
		}
		damage = zapyourself(obj, FALSE);
		if (damage) {
		    sprintf(buf, "killed %sself by breaking a wand", uhim());
		    losehp(damage, buf, NO_KILLER_PREFIX);
		}
		if (iflags.botl) bot();		/* blindness */
	    } else if ((mon = m_at(level, x, y)) != 0) {
		bhitm(mon, obj);
	     /* if (iflags.botl) bot(); */
	    }
	    if (affects_objects && level->objects[x][y]) {
		bhitpile(obj, bhito, x, y);
		if (iflags.botl) bot();		/* potion effects */
	    }
	}
    }

    /* Note: if player fell thru, this call is a no-op.
       Damage is handled in digactualhole in that case */
    if (shop_damage) pay_for_damage("dig into", FALSE);

    if (obj->otyp == WAN_LIGHT)
	litroom(TRUE, obj);	/* only needs to be done once */

 discard_broken_wand:
    obj = current_wand;		/* [see dozap() and destroy_item()] */
    current_wand = 0;
    if (obj)
	delobj(obj);
    nomul(0, NULL);
    return 1;
}

static boolean uhave_graystone(void)
{
	struct obj *otmp;

	for (otmp = invent; otmp; otmp = otmp->nobj)
		if (is_graystone(otmp))
			return TRUE;
	return FALSE;
}

static void add_class(char *cl, char class)
{
	char tmp[2];
	tmp[0] = class;
	tmp[1] = '\0';
	strcat(cl, tmp);
}

int doapply(struct obj *obj)
{
	int res = 1;
	char class_list[MAXOCLASSES+2];

	if (check_capacity(NULL)) return 0;

	if (carrying(POT_OIL) || uhave_graystone())
		strcpy(class_list, tools_too);
	else
		strcpy(class_list, tools);
	if (carrying(CREAM_PIE) || carrying(EUCALYPTUS_LEAF))
		add_class(class_list, FOOD_CLASS);

	if (obj && !validate_object(obj, class_list, "use or apply"))
	    return 0;
	else if (!obj)
	    obj = getobj(class_list, "use or apply");
	if (!obj) return 0;

	if (obj->oartifact && !touch_artifact(obj, &youmonst))
	    return 1;	/* evading your grasp costs a turn; just be
			   grateful that you don't drop it as well */

	if (obj->oclass == WAND_CLASS)
	    return do_break_wand(obj);

	switch(obj->otyp){
	case BLINDFOLD:
	case LENSES:
		if (obj == ublindf) {
		    if (!cursed(obj)) Blindf_off(obj);
		} else if (!ublindf)
		    Blindf_on(obj);
		else pline("You are already %s.",
			ublindf->otyp == TOWEL ?     "covered by a towel" :
			ublindf->otyp == BLINDFOLD ? "wearing a blindfold" :
						     "wearing lenses");
		break;
	case CREAM_PIE:
		res = use_cream_pie(obj);
		break;
	case BULLWHIP:
		res = use_whip(obj);
		break;
	case GRAPPLING_HOOK:
		res = use_grapple(obj);
		break;
	case LARGE_BOX:
	case CHEST:
	case ICE_BOX:
	case SACK:
	case BAG_OF_HOLDING:
	case OILSKIN_SACK:
		res = use_container(obj, 1);
		break;
	case BAG_OF_TRICKS:
		bagotricks(obj);
		break;
	case CAN_OF_GREASE:
		use_grease(obj);
		break;
	case LOCK_PICK:
	case CREDIT_CARD:
	case SKELETON_KEY:
		pick_lock(obj);
		break;
	case PICK_AXE:
	case DWARVISH_MATTOCK:
		res = use_pick_axe(obj);
		break;
	case TINNING_KIT:
		use_tinning_kit(obj);
		break;
	case LEASH:
		use_leash(obj);
		break;
	case SADDLE:
		res = use_saddle(obj);
		break;
	case MAGIC_WHISTLE:
		use_magic_whistle(obj);
		break;
	case TIN_WHISTLE:
		use_whistle(obj);
		break;
	case EUCALYPTUS_LEAF:
		/* MRKR: Every Australian knows that a gum leaf makes an */
		/*	 excellent whistle, especially if your pet is a  */
		/*	 tame kangaroo named Skippy.			 */
		if (obj->blessed) {
		    use_magic_whistle(obj);
		    /* sometimes the blessing will be worn off */
		    if (!rn2(49)) {
			if (!Blind) {
			    char buf[BUFSZ];

			    pline("%s %s %s.", Shk_Your(buf, obj),
				  aobjnam(obj, "glow"), hcolor("brown"));
			    obj->bknown = 1;
			}
			unbless(obj);
		    }
		} else {
		    use_whistle(obj);
		}
		break;
	case STETHOSCOPE:
		res = use_stethoscope(obj);
		break;
	case MIRROR:
		res = use_mirror(obj);
		break;
	case BELL:
	case BELL_OF_OPENING:
		use_bell(&obj);
		break;
	case CANDELABRUM_OF_INVOCATION:
		use_candelabrum(obj);
		break;
	case WAX_CANDLE:
	case TALLOW_CANDLE:
		use_candle(&obj);
		break;
	case OIL_LAMP:
	case MAGIC_LAMP:
	case BRASS_LANTERN:
		use_lamp(obj);
		break;
	case POT_OIL:
		light_cocktail(obj);
		break;
	case EXPENSIVE_CAMERA:
		res = use_camera(obj);
		break;
	case TOWEL:
		res = use_towel(obj);
		break;
	case CRYSTAL_BALL:
		use_crystal_ball(obj);
		break;
	case MAGIC_MARKER:
		res = dowrite(obj);
		break;
	case TIN_OPENER:
		if (!carrying(TIN)) {
			pline("You have no tin to open.");
			goto xit;
		}
		pline("You cannot open a tin without eating or discarding its contents.");
		if (flags.verbose)
			pline("In order to eat, use the 'e' command.");
		if (obj != uwep)
    pline("Opening the tin will be much easier if you wield the tin opener.");
		goto xit;

	case FIGURINE:
		if (use_figurine(obj))
		    obj = NULL; /* used up */
		break;
	case UNICORN_HORN:
		use_unicorn_horn(obj);
		break;
	case WOODEN_FLUTE:
	case MAGIC_FLUTE:
	case TOOLED_HORN:
	case FROST_HORN:
	case FIRE_HORN:
	case WOODEN_HARP:
	case MAGIC_HARP:
	case BUGLE:
	case LEATHER_DRUM:
	case DRUM_OF_EARTHQUAKE:
		res = do_play_instrument(obj);
		break;
	case HORN_OF_PLENTY:	/* not a musical instrument */
		if (obj->spe > 0) {
		    struct obj *otmp;
		    const char *what;

		    consume_obj_charge(obj, TRUE);
		    if (!rn2(13)) {
			otmp = mkobj(level, POTION_CLASS, FALSE);
			if (objects[otmp->otyp].oc_magic) do {
			    otmp->otyp = rnd_class(POT_BOOZE, POT_WATER);
			} while (otmp->otyp == POT_SICKNESS);
			what = "A potion";
		    } else {
			otmp = mkobj(level, FOOD_CLASS, FALSE);
			if (otmp->otyp == FOOD_RATION && !rn2(7))
			    otmp->otyp = LUMP_OF_ROYAL_JELLY;
			what = "Some food";
		    }
		    pline("%s spills out.", what);
		    otmp->blessed = obj->blessed;
		    otmp->cursed = obj->cursed;
		    otmp->owt = weight(otmp);
		    hold_another_object(otmp, u.uswallow ?
				       "Oops!  %s out of your reach!" :
					(Is_airlevel(&u.uz) ||
					 Is_waterlevel(&u.uz) ||
					 level->locations[u.ux][u.uy].typ < IRONBARS ||
					 level->locations[u.ux][u.uy].typ >= ICE) ?
					       "Oops!  %s away from you!" :
					       "Oops!  %s to the floor!",
					       The(aobjnam(otmp, "slip")),
					       NULL);
		    makeknown(HORN_OF_PLENTY);
		} else
		    pline("Nothing happens.");
		break;
	case LAND_MINE:
	case BEARTRAP:
		use_trap(obj);
		break;
	case FLINT:
	case LUCKSTONE:
	case LOADSTONE:
	case TOUCHSTONE:
		use_stone(obj);
		break;
	default:
		/* Pole-weapons can strike at a distance */
		if (is_pole(obj)) {
			res = use_pole(obj);
			break;
		} else if (is_pick(obj) || is_axe(obj)) {
			res = use_pick_axe(obj);
			break;
		}
		pline("Sorry, I don't know how to use that.");
	xit:
		nomul(0, NULL);
		return 0;
	}
	if (res && obj && obj->oartifact) arti_speak(obj);
	nomul(0, NULL);
	return res;
}

/* Keep track of unfixable troubles for purposes of messages saying you feel
 * great.
 */
int unfixable_trouble_count(boolean is_horn)
{
	int unfixable_trbl = 0;

	if (Stoned) unfixable_trbl++;
	if (Strangled) unfixable_trbl++;
	if (Wounded_legs && !u.usteed) unfixable_trbl++;
	if (Slimed) unfixable_trbl++;
	/* lycanthropy is not desirable, but it doesn't actually make you feel
	   bad */

	/* we'll assume that intrinsic stunning from being a bat/stalker
	   doesn't make you feel bad */
	if (!is_horn) {
	    if (Confusion) unfixable_trbl++;
	    if (Sick) unfixable_trbl++;
	    if (HHallucination) unfixable_trbl++;
	    if (Vomiting) unfixable_trbl++;
	    if (HStun) unfixable_trbl++;
	}
	return unfixable_trbl;
}

/*apply.c*/
