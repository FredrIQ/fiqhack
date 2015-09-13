/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "edog.h"

static int pet_type(struct newgame_options *);

void
initedog(struct monst *mtmp)
{
    mtmp->mtame = is_domestic(mtmp->data) ? 10 : 5;
    mtmp->mpeaceful = 1;
    mtmp->mavenge = 0;
    set_malign(mtmp);   /* recalc alignment now that it's tamed */
    mtmp->mleashed = 0;
    mtmp->meating = 0;
    EDOG(mtmp)->droptime = 0;
    EDOG(mtmp)->dropdist = 10000;
    EDOG(mtmp)->apport = 10;
    EDOG(mtmp)->whistletime = 0;
    EDOG(mtmp)->hungrytime = 1000 + moves;
    EDOG(mtmp)->save_compat_bytes[0] = -1;
    EDOG(mtmp)->save_compat_bytes[1] = -1;
    EDOG(mtmp)->abuse = 0;
    EDOG(mtmp)->revivals = 0;
    EDOG(mtmp)->mhpmax_penalty = 0;
    EDOG(mtmp)->killed_by_u = 0;
}

static int
pet_type(struct newgame_options *ngo)
{
    int which_pet = rn2(2);

    if (urole.petnum != NON_PM)
        return urole.petnum;
    else if (ngo && ngo->preferred_pet == 'c')
        return PM_KITTEN;
    else if (ngo && ngo->preferred_pet == 'd')
        return PM_LITTLE_DOG;
    else
        return which_pet ? PM_KITTEN : PM_LITTLE_DOG;
}

struct monst *
make_familiar(struct obj *otmp, xchar x, xchar y, boolean quietly)
{
    const struct permonst *pm;
    struct monst *mtmp = 0;
    int chance, trycnt = 100;

    do {
        if (otmp) {     /* figurine; otherwise spell */
            int mndx = otmp->corpsenm;

            pm = &mons[mndx];
            /* activating a figurine provides one way to exceed the maximum
               number of the target critter created--unless it has a special
               limit (erinys, Nazgul) */
            if ((mvitals[mndx].mvflags & G_EXTINCT) &&
                mbirth_limit(mndx) != MAXMONNO) {
                if (!quietly)
                    /* have just been given "You <do something with> the
                       figurine and it transforms." message */
                    pline("... into a pile of dust.");
                break;  /* mtmp is null */
            }
        } else if (!rn2(3)) {
            pm = &mons[pet_type(NULL)];
        } else {
            pm = rndmonst(&u.uz, rng_t_create_monster);
            if (!pm) {
                if (!quietly)
                    pline
                        ("There seems to be nothing available for a familiar.");
                break;
            }
        }

        mtmp = makemon(pm, level, x, y, MM_EDOG | MM_IGNOREWATER |
                       (otmp ? 0 : (MM_CREATEMONSTER | MM_CMONSTER_T)));
        if (otmp && !mtmp) {    /* monster was genocided or square occupied */
            if (!quietly)
                pline("The figurine writhes and then shatters into pieces!");
            break;
        }
    } while (!mtmp && --trycnt > 0);

    if (!mtmp)
        return NULL;

    if (is_pool(level, mtmp->mx, mtmp->my) && minliquid(mtmp))
        return NULL;

    initedog(mtmp);
    mtmp->msleeping = 0;
    if (otmp) { /* figurine; resulting monster might not become a pet */
        chance = rn2_on_rng(10, rng_figurine_effect);
        /* 0: tame, 1: peaceful, 2: hostile, 3+: matching BCU; this gives
           an 80% chance of the desired effect, 10% of each other effect */
        if (chance > 2)
            chance = otmp->blessed ? 0 : !otmp->cursed ? 1 : 2;
        if (chance > 0) {
            mtmp->mtame = 0;    /* not tame after all */
            if (chance == 2) {  /* hostile (cursed figurine) */
                if (!quietly)
                    pline("You get a bad feeling about this.");
                msethostility(mtmp, TRUE, TRUE);
            }
        }
        /* if figurine has been named, give same name to the monster */
        if (otmp->onamelth)
            mtmp = christen_monst(mtmp, ONAME(otmp));
    }
    set_malign(mtmp);   /* more alignment changes */
    newsym(mtmp->mx, mtmp->my);

    /* must wield weapon immediately since pets will otherwise drop it */
    if (mtmp->mtame && attacktype(mtmp->data, AT_WEAP)) {
        mtmp->weapon_check = NEED_HTH_WEAPON;
        mon_wield_item(mtmp);
    }
    return mtmp;
}

/*
 * This function is only called from newgame, so we know:
 *   - pets aren't genocided so makemon will always work
 *   - the petname has not been used yet
 * This is called very late in the newgame sequence, and so can safely clobber
 * the charstats RNGs.
 */
struct monst *
makedog(struct newgame_options *ngo)
{
    struct monst *mtmp;
    struct obj *otmp;
    const char *petname;
    int pettype;

    if (ngo->preferred_pet == 'n')
        return NULL;

    pettype = pet_type(ngo);
    if (pettype == PM_LITTLE_DOG)
        petname = ngo->dogname;
    else if (pettype == PM_PONY)
        petname = ngo->horsename;
    else
        petname = ngo->catname;

    /* default pet names */
    if (!*petname && pettype == PM_LITTLE_DOG) {
        /* All of these names were for dogs. */
        if (Role_if(PM_CAVEMAN))
            petname = "Slasher";        /* The Warrior */
        if (Role_if(PM_SAMURAI))
            petname = "Hachi";  /* Shibuya Station */
        if (Role_if(PM_BARBARIAN))
            petname = "Idefix"; /* Obelix */
        if (Role_if(PM_RANGER))
            petname = "Sirius"; /* Orion's dog */
    }

    /* ideally we'd use rng_charstats_role for this, but... */
    mtmp = makemon(&mons[pettype], level, u.ux, u.uy, MM_EDOG);

    /* Horses already wear a saddle */
    if (pettype == PM_PONY &&
        ((otmp = mksobj(level, SADDLE, TRUE, FALSE, rng_charstats_role)))) {
        if (mpickobj(mtmp, otmp))
            panic("merged saddle?");
        mtmp->misc_worn_check |= W_MASK(os_saddle);
        otmp->dknown = otmp->bknown = otmp->rknown = 1;
        otmp->owornmask = W_MASK(os_saddle);
        otmp->leashmon = mtmp->m_id;
        update_mon_intrinsics(mtmp, otmp, TRUE, TRUE);
    }

    if (*petname)
        mtmp = christen_monst(mtmp, petname);

    initedog(mtmp);
    return mtmp;
}

/* record `last move time' for all monsters prior to level save so that
   mon_arrive() can catch up for lost time when they're restored later */
void
update_mlstmv(void)
{
    struct monst *mon;

    /* monst->mlstmv used to be updated every time `monst' actually moved, but
       that is no longer the case so we just do a blanket assignment */
    for (mon = level->monlist; mon; mon = mon->nmon)
        if (!DEADMONSTER(mon))
            mon->mlstmv = moves;
}

void
losedogs(void)
{
    struct monst *mtmp, *mtmp0 = 0, *mtmp2;

    while ((mtmp = turnstate.migrating_pets) != 0) {
        turnstate.migrating_pets = mtmp->nmon;
        mon_arrive(mtmp, TRUE);
    }

    for (mtmp = migrating_mons; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;
        if (mtmp->mux == u.uz.dnum && mtmp->muy == u.uz.dlevel) {
            if (mtmp == migrating_mons)
                migrating_mons = mtmp->nmon;
            else
                mtmp0->nmon = mtmp->nmon;
            mon_arrive(mtmp, FALSE);
        } else
            mtmp0 = mtmp;
    }
}

/* called from resurrect() in addition to losedogs() */
void
mon_arrive(struct monst *mtmp, boolean with_you)
{
    struct trap *t;
    struct obj *otmp;
    xchar xlocale, ylocale, xyloc, xyflags, wander;
    int num_segs;

    mtmp->dlevel = level;
    mtmp->nmon = level->monlist;
    level->monlist = mtmp;
    if (mtmp->isshk)
        set_residency(mtmp, FALSE);

    num_segs = mtmp->wormno;
    /* baby long worms have no tail so don't use is_longworm() */
    if ((mtmp->data == &mons[PM_LONG_WORM]) &&
        (mtmp->wormno = get_wormno(mtmp->dlevel)) != 0) {
        initworm(mtmp, num_segs);
        /* tail segs are not yet initialized or displayed */
    } else
        mtmp->wormno = 0;

    /* some monsters might need to do something special upon arrival _after_
       the current level has been fully set up; see dochug() */
    mtmp->mstrategy |= STRAT_ARRIVE;

    xyloc = mtmp->xyloc;
    xyflags = mtmp->xyflags;
    xlocale = mtmp->xlocale;
    ylocale = mtmp->ylocale;

    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
        set_obj_level(mtmp->dlevel, otmp);

    if (mtmp == u.usteed)
        return; /* don't place steed on the map */
    if (with_you) {
        /* When a monster accompanies you, sometimes it will arrive at your
           intended destination and you'll end up next to that spot.  This code
           doesn't control the final outcome; goto_level(do.c) decides who ends
           up at your target spot when there is a monster there too. */
        if (!MON_AT(level, u.ux, u.uy) &&
            !rn2(mtmp->mtame ? 10 : mtmp->mpeaceful ? 5 : 2))
            rloc_to(mtmp, u.ux, u.uy);
        else
            mnexto(mtmp);
        return;
    }
    /*
     * The monster arrived on this level independently of the player.
     * Its coordinate fields were overloaded for use as flags that
     * specify its final destination.
     */

    if (mtmp->mlstmv < moves - 1L) {
        /* heal monster for time spent in limbo */
        long nmv = moves - 1L - mtmp->mlstmv;

        mon_catchup_elapsed_time(mtmp, nmv);
        mtmp->mlstmv = moves - 1L;

        /* let monster move a bit on new level (see placement code below) */
        wander = (xchar) min(nmv, 8);
    } else
        wander = 0;

    switch (xyloc) {
    case MIGR_APPROX_XY:       /* {x,y}locale set above */
        break;
    case MIGR_EXACT_XY:
        wander = 0;
        break;
    case MIGR_NEAR_PLAYER:
        xlocale = u.ux, ylocale = u.uy;
        break;
    case MIGR_STAIRS_UP:
        xlocale = level->upstair.sx, ylocale = level->upstair.sy;
        break;
    case MIGR_STAIRS_DOWN:
        xlocale = level->dnstair.sx, ylocale = level->dnstair.sy;
        break;
    case MIGR_LADDER_UP:
        xlocale = level->upladder.sx, ylocale = level->upladder.sy;
        break;
    case MIGR_LADDER_DOWN:
        xlocale = level->dnladder.sx, ylocale = level->dnladder.sy;
        break;
    case MIGR_SSTAIRS:
        xlocale = level->sstairs.sx, ylocale = level->sstairs.sy;
        break;
    case MIGR_PORTAL:
        if (In_endgame(&u.uz)) {
            /* there is no arrival portal for endgame levels */
            /* BUG[?]: for simplicity, this code relies on the fact that we
               know that the current endgame levels always build upwards and
               never have any exclusion subregion inside their TELEPORT_REGION
               settings. */
            xlocale =
                rn1(level->updest.hx - level->updest.lx + 1, level->updest.lx);
            ylocale =
                rn1(level->updest.hy - level->updest.ly + 1, level->updest.ly);
            break;
        }
        /* find the arrival portal */
        for (t = level->lev_traps; t; t = t->ntrap)
            if (t->ttyp == MAGIC_PORTAL)
                break;
        if (t) {
            xlocale = t->tx, ylocale = t->ty;
            break;
        } else {
            impossible("mon_arrive: no corresponding portal?");
        }
     /*FALLTHRU*/ default:
    case MIGR_RANDOM:
        xlocale = COLNO;
        ylocale = ROWNO;
        break;
    }

    if ((xlocale != COLNO) && wander) {
        /* monster moved a bit; pick a nearby location */
        /* mnearto() deals w/stone, et al */
        char *r = in_rooms(level, xlocale, ylocale, 0);

        if (r && *r) {
            coord c;

            /* somexy() handles irregular level->rooms */
            if (somexy(level, &level->rooms[*r - ROOMOFFSET], &c, rng_main))
                xlocale = c.x, ylocale = c.y;
            else {
                xlocale = COLNO;
                ylocale = ROWNO;
            }
        } else {        /* not in a room */
            int i, j;

            i = max(0, xlocale - wander);
            j = min(COLNO - 1, xlocale + wander);
            xlocale = rn1(j - i, i);
            i = max(0, ylocale - wander);
            j = min(ROWNO - 1, ylocale + wander);
            ylocale = rn1(j - i, i);
        }
    }
    /* moved a bit */
    mtmp->mx = COLNO;       /* (already is 0) */
    mtmp->my = xyflags;
    if (xlocale != COLNO)
        mnearto(mtmp, xlocale, ylocale, FALSE);
    else {
        if (!rloc(mtmp, TRUE)) {
            /* Failed to place migrating monster, probably because the level is
               full.  Dump the monster's cargo and leave the monster dead. */
            struct obj *obj;

            while ((obj = mtmp->minvent) != 0) {
                obj_extract_self(obj);
                obj_no_longer_held(obj);
                if (obj->owornmask & W_MASK(os_wep))
                    setmnotwielded(mtmp, obj);
                obj->owornmask = 0L;
                if (xlocale != COLNO && ylocale != ROWNO)
                    place_object(obj, level, xlocale, ylocale);
                else {
                    rloco(obj);
                    get_obj_location(obj, &xlocale, &ylocale, 0);
                }
            }
            mkcorpstat(CORPSE, NULL, mtmp->data, level, xlocale, ylocale,
                       FALSE, rng_main);
            mongone(mtmp);
        }
    }

    mtmp->mux = COLNO;
    mtmp->muy = ROWNO;
}

/* heal monster for time spent elsewhere */
void
mon_catchup_elapsed_time(struct monst *mtmp, long nmv)
{
    int imv = 0;        /* avoid zillions of casts and lint warnings */

    /* Without this, if a monster is deleted during level creation
       (e.g. statue trap) and the player comes to the level later, it'll
       have healed itself out of the "deleted" state, thoroughly confusing
       dmonsfree(). */
    if (DEADMONSTER(mtmp))
        return;

    if (nmv >= LARGEST_INT)     /* paranoia */
        imv = LARGEST_INT - 1;
    else
        imv = (int)nmv;
    if (!imv)
        return;

    /* might stop being afraid, blind or frozen */
    /* set to 1 and allow final decrement in movemon() */
    if (mtmp->mblinded) {
        if (imv >= (int)mtmp->mblinded)
            mtmp->mblinded = 1;
        else
            mtmp->mblinded -= imv;
    }
    if (mtmp->mfrozen) {
        if (imv >= (int)mtmp->mfrozen)
            mtmp->mfrozen = 1;
        else
            mtmp->mfrozen -= imv;
    }
    if (mtmp->mfleetim) {
        if (imv >= (int)mtmp->mfleetim)
            mtmp->mfleetim = 1;
        else
            mtmp->mfleetim -= imv;
    }

    /* might recover from temporary trouble */
    if (mtmp->mtrapped && rn2(imv + 1) > 40 / 2)
        mtmp->mtrapped = 0;
    if (mtmp->mconf && rn2(imv + 1) > 50 / 2)
        mtmp->mconf = 0;
    if (mtmp->mstun && rn2(imv + 1) > 10 / 2)
        mtmp->mstun = 0;

    /* might finish eating or be able to use special ability again */
    if (imv > mtmp->meating)
        mtmp->meating = 0;
    else
        mtmp->meating -= imv;
    if (imv > mtmp->mspec_used)
        mtmp->mspec_used = 0;
    else
        mtmp->mspec_used -= imv;

    /* reduce tameness for every 150 moves you are separated */
    if (mtmp->mtame) {
        int wilder = (imv + 75) / 150;

        /* The rng_dog_untame RNG is only semi-synched, because the argument
           changes.  This gives better results than rng_main, and we can't match
           exactly due to different pet-wrangling habits.

           Note: not msethostility; we're off-level right now. */
        if (mtmp->mtame > wilder)
            mtmp->mtame -= wilder;      /* less tame */
        else if (mtmp->mtame > rn2_on_rng(wilder, rng_dog_untame))
            mtmp->mtame = 0;    /* untame */
        else
            mtmp->mtame = mtmp->mpeaceful = 0;  /* hostile! */
    }
    /* check to see if it would have died as a pet; if so, go wild instead of
       dying the next time we call dog_move() */
    if (mtmp->mtame && !mtmp->isminion &&
        (carnivorous(mtmp->data) || herbivorous(mtmp->data))) {
        const struct edog *edog = CONST_EDOG(mtmp);

        if ((moves > edog->hungrytime + 500 && mtmp->mhp < 3) ||
            (moves > edog->hungrytime + 750))
            mtmp->mtame = mtmp->mpeaceful = 0;
    }

    if (!mtmp->mtame && mtmp->mleashed) {
        /* leashed monsters should always be with hero, consequently never
           losing any time to be accounted for later */
        impossible("catching up for leashed monster?");
        m_unleash(mtmp, FALSE);
    }

    /* recover lost hit points */
    if (!regenerates(mtmp->data))
        imv /= 20;
    if (mtmp->mhp + imv >= mtmp->mhpmax)
        mtmp->mhp = mtmp->mhpmax;
    else
        mtmp->mhp += imv;
}


/* called when you move to another level
 * pets_only: true for ascension or final escape */
void
keepdogs(boolean pets_only)
{
    struct monst *mtmp, *mtmp2;
    struct obj *obj;
    int num_segs;
    boolean stay_behind;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;
        if (DEADMONSTER(mtmp))
            continue;
        if (pets_only && !mtmp->mtame)
            continue;
        if (((monnear(mtmp, u.ux, u.uy) && levl_follower(mtmp)) ||
             (mtmp == u.usteed) ||
             /* the wiz will level t-port from anywhere to chase the amulet; if
                you don't have it, will chase you only if in range. -3. */
             (Uhave_amulet && mtmp->iswiz))
            && ((!mtmp->msleeping && mtmp->mcanmove)
                /* eg if level teleport or new trap, steed has no control to
                   avoid following */
                || (mtmp == u.usteed))
            /* monster won't follow if it hasn't noticed you yet */
            && !(mtmp->mstrategy & STRAT_WAITFORU)) {
            stay_behind = FALSE;
            if (!pets_only && mtmp->mtame && mtmp->meating) {
                if (canseemon(mtmp))
                    pline("%s is still eating.", Monnam(mtmp));
                stay_behind = TRUE;
            } else if (mon_has_amulet(mtmp)) {
                if (canseemon(mtmp))
                    pline("%s seems very disoriented for a moment.",
                          Monnam(mtmp));
                stay_behind = TRUE;
            } else if (!pets_only && mtmp->mtame && mtmp->mtrapped) {
                if (canseemon(mtmp))
                    pline("%s is still trapped.", Monnam(mtmp));
                stay_behind = TRUE;
            }
            if (mtmp == u.usteed)
                stay_behind = FALSE;

            if (stay_behind) {
                if (mtmp->mleashed) {
                    pline("%s leash suddenly comes loose.", humanoid(mtmp->data)
                          ? (mtmp->female ? "Her" : "His")
                          : "Its");
                    m_unleash(mtmp, FALSE);
                }
                continue;
            }
            if (mtmp->isshk)
                set_residency(mtmp, TRUE);

            if (mtmp->wormno) {
                int cnt;

                /* NOTE: worm is truncated to # segs = max wormno size */
                cnt = count_wsegs(mtmp);
                num_segs = min(cnt, MAX_NUM_WORMS - 1);
                wormgone(mtmp);
            } else
                num_segs = 0;

            /* set minvent's obj->no_charge to 0 */
            for (obj = mtmp->minvent; obj; obj = obj->nobj) {
                if (Has_contents(obj))
                    picked_container(obj);      /* does the right thing */
                obj->no_charge = 0;
            }

            relmon(mtmp);
            newsym(mtmp->mx, mtmp->my);
            mtmp->mx = COLNO;    /* avoid mnexto()/MON_AT() problem */
            mtmp->my = ROWNO;
            mtmp->wormno = num_segs;
            mtmp->mlstmv = moves;
            mtmp->nmon = turnstate.migrating_pets;
            turnstate.migrating_pets = mtmp;
        } else if (mtmp->iswiz) {
            /* we want to be able to find him when his next resurrection chance
               comes up, but have him resume his present location if player
               returns to this level before that time */
            migrate_to_level(mtmp, ledger_no(&u.uz), MIGR_EXACT_XY, NULL);
        } else if (mtmp->mleashed) {
            /* this can happen if your quest leader ejects you from the "home"
               level while a leashed pet isn't next to you */
            pline("%s leash goes slack.", s_suffix(Monnam(mtmp)));
            m_unleash(mtmp, FALSE);
        }
    }
}


void
migrate_to_level(struct monst *mtmp, xchar tolev,       /* destination level */
                 xchar xyloc,   /* MIGR_xxx destination xy location: */
                 coord * cc)
{       /* optional destination coordinates */
    struct obj *obj;
    d_level new_lev;
    xchar xyflags;
    int num_segs = 0;   /* count of worm segments */

    if (mtmp->isshk)
        set_residency(mtmp, TRUE);

    if (mtmp->wormno) {
        int cnt;

        /* **** NOTE: worm is truncated to # segs = max wormno size **** */
        cnt = count_wsegs(mtmp);
        num_segs = min(cnt, MAX_NUM_WORMS - 1);
        wormgone(mtmp);
    }

    /* set minvent's obj->no_charge to 0 */
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (Has_contents(obj))
            picked_container(obj);      /* does the right thing */
        obj->no_charge = 0;
    }

    if (mtmp->mleashed) {
        mtmp->mtame--;
        m_unleash(mtmp, TRUE);
    }
    relmon(mtmp);
    mtmp->nmon = migrating_mons;
    migrating_mons = mtmp;
    if (mtmp->dlevel == level)
        newsym(mtmp->mx, mtmp->my);

    /* The dlevel pointer is meaningless for a migrating monster. Set it to NULL
       so that any uses of it are detected quickly via the resulting
       segfault. */
    mtmp->dlevel = NULL;

    new_lev.dnum = ledger_to_dnum((xchar) tolev);
    new_lev.dlevel = ledger_to_dlev((xchar) tolev);

    /* set migration data */
    xyflags = (depth(&new_lev) < depth(&u.uz)); /* 1 => up */
    if (In_W_tower(mtmp->mx, mtmp->my, &u.uz))
        xyflags |= 2;
    mtmp->wormno = num_segs;
    mtmp->mlstmv = moves;
    mtmp->xlocale = cc ? cc->x : mtmp->mx;
    mtmp->ylocale = cc ? cc->y : mtmp->my;
    mtmp->xyloc = xyloc;
    mtmp->xyflags = xyflags;
    mtmp->mux = new_lev.dnum;
    mtmp->muy = new_lev.dlevel;
    mtmp->mx = COLNO;
    mtmp->my = ROWNO;    /* this implies migration */
}


/* return quality of food; the lower the better */
/* fungi will eat even tainted food */
int
dogfood(const struct monst *mon, struct obj *obj)
{
    boolean carni = carnivorous(mon->data);
    boolean herbi = herbivorous(mon->data);
    const struct permonst *fptr = &mons[obj->corpsenm];
    boolean starving;

    if (is_quest_artifact(obj) || obj_resists(obj, 0, 95))
        return obj->cursed ? TABU : APPORT;

    switch (obj->oclass) {
    case FOOD_CLASS:
        if (obj->otyp == CORPSE &&
            ((touch_petrifies(&mons[obj->corpsenm]) && !resists_ston(mon))
             || is_rider(fptr)))
            return TABU;

        /* Ghouls only eat old corpses... yum! */
        if (mon->data == &mons[PM_GHOUL])
            return (obj->otyp == CORPSE &&
                    peek_at_iced_corpse_age(obj) + 50L <=
                    moves) ? DOGFOOD : TABU;

        if (!carni && !herbi)
            return obj->cursed ? UNDEF : APPORT;

        /* a starving pet will eat almost anything */
        starving = (mon->mtame && !mon->isminion &&
                    CONST_EDOG(mon)->mhpmax_penalty);

        switch (obj->otyp) {
        case TRIPE_RATION:
        case MEATBALL:
        case MEAT_RING:
        case MEAT_STICK:
        case HUGE_CHUNK_OF_MEAT:
            return carni ? DOGFOOD : MANFOOD;
        case EGG:
            if (touch_petrifies(&mons[obj->corpsenm]) && !resists_ston(mon))
                return POISON;
            return carni ? CADAVER : MANFOOD;
        case CORPSE:
            if ((peek_at_iced_corpse_age(obj) + 50L <= moves &&
                 obj->corpsenm != PM_LIZARD && obj->corpsenm != PM_LICHEN &&
                 mon->data->mlet != S_FUNGUS) ||
                (acidic(&mons[obj->corpsenm]) && !resists_acid(mon)) ||
                (poisonous(&mons[obj->corpsenm]) && !resists_poison(mon)))
                return POISON;
            else if (vegan(fptr))
                return herbi ? CADAVER : MANFOOD;
            else
                return carni ? CADAVER : MANFOOD;
        case CLOVE_OF_GARLIC:
            return (is_undead(mon->data) ? TABU
                    : ((herbi || starving) ? ACCFOOD : MANFOOD));
        case TIN:
            return metallivorous(mon->data) ? ACCFOOD : MANFOOD;
        case APPLE:
        case CARROT:
            return herbi ? DOGFOOD : starving ? ACCFOOD : MANFOOD;
        case BANANA:
            return ((mon->data->mlet ==
                     S_YETI) ? DOGFOOD : ((herbi ||
                                           starving) ? ACCFOOD : MANFOOD));

        case K_RATION:
        case C_RATION:
        case CRAM_RATION:
        case LEMBAS_WAFER:
        case FOOD_RATION:
            if (is_human(mon->data) || is_elf(mon->data) || is_dwarf(mon->data)
                || is_gnome(mon->data) || is_orc(mon->data))
                return ACCFOOD;

        default:
            if (starving)
                return ACCFOOD;
            return (obj->otyp >
                    SLIME_MOLD ? (carni ? ACCFOOD : MANFOOD) :
                    (herbi ? ACCFOOD : MANFOOD));
        }
    default:
        if (obj->otyp == AMULET_OF_STRANGULATION ||
            obj->otyp == RIN_SLOW_DIGESTION)
            return TABU;
        if (hates_silver(mon->data) && objects[obj->otyp].oc_material == SILVER)
            return TABU;
        if (mon->data == &mons[PM_GELATINOUS_CUBE] && is_organic(obj))
            return ACCFOOD;
        if (metallivorous(mon->data) && is_metallic(obj) &&
            (is_rustprone(obj) || mon->data != &mons[PM_RUST_MONSTER])) {
            /* Non-rustproofed ferrous based metals are preferred. */
            return (is_rustprone(obj) && !obj->oerodeproof) ? DOGFOOD : ACCFOOD;
        }
        if (!obj->cursed && obj->oclass != BALL_CLASS &&
            obj->oclass != CHAIN_CLASS)
            return APPORT;
        /* fall into next case */
    case ROCK_CLASS:
        return UNDEF;
    }
}


struct monst *
tamedog(struct monst *mtmp, struct obj *obj)
{
    struct monst *mtmp2;

    /* The Wiz, Medusa and the quest nemeses aren't even made peaceful. */
    if (mtmp->iswiz || mtmp->data == &mons[PM_MEDUSA]
        || (mtmp->data->mflags3 & M3_WANTSARTI))
        return NULL;

    /* worst case, at least it'll be peaceful; this uses the main RNG because
       realtime effects means that this won't really sync anyway; this also
       calls set_malign (thus there's no need for the caller to call it after
       calling tamedog()) */
    msethostility(mtmp, FALSE, TRUE);
    if (flags.moonphase == FULL_MOON && night() && rn2(6) && obj &&
        mtmp->data->mlet == S_DOG)
        return NULL;

    /* If we cannot tame it, at least it's no longer afraid. */
    mtmp->mflee = 0;
    mtmp->mfleetim = 0;

    /* make grabber let go now, whether it becomes tame or not */
    if (mtmp == u.ustuck) {
        if (Engulfed)
            expels(mtmp, mtmp->data, TRUE);
        else if (!(Upolyd && sticks(youmonst.data)))
            unstuck(mtmp);
    }

    /* feeding it treats makes it tamer */
    if (mtmp->mtame && obj) {
        int tasty;

        if (mtmp->mcanmove && !mtmp->mconf && !mtmp->meating &&
            ((tasty = dogfood(mtmp, obj)) == DOGFOOD ||
             (tasty <= ACCFOOD && CONST_EDOG(mtmp)->hungrytime <= moves))) {
            /* pet will "catch" and eat this thrown food */
            if (canseemon(mtmp)) {
                boolean big_corpse = (obj->otyp == CORPSE &&
                                      obj->corpsenm >= LOW_PM &&
                                      mons[obj->corpsenm].msize >
                                      mtmp->data->msize);
                pline("%s catches %s%s", Monnam(mtmp), the(xname(obj)),
                      !big_corpse ? "." : ", or vice versa!");
            } else if (cansee(mtmp->mx, mtmp->my))
                pline("%s.", Tobjnam(obj, "stop"));
            /* dog_eat expects a floor object */
            place_object(obj, level, mtmp->mx, mtmp->my);
            dog_eat(mtmp, obj, mtmp->mx, mtmp->my, FALSE);
            /* eating might have killed it, but that doesn't matter here; a
               non-null result suppresses "miss" message for thrown food and
               also implies that the object has been deleted */
            return mtmp;
        } else
            return NULL;
    }

    if (mtmp->mtame || !mtmp->mcanmove ||
        /* monsters with conflicting structures cannot be tamed */
        mtmp->isshk || mtmp->isgd || mtmp->ispriest || mtmp->isminion ||
        is_covetous(mtmp->data) || is_human(mtmp->data) ||
        (is_demon(mtmp->data) && !is_demon(youmonst.data)) ||
        (obj && dogfood(mtmp, obj) >= MANFOOD))
        return NULL;

    if (mtmp->m_id == u.quest_status.leader_m_id)
        return NULL;

    /* make a new monster which has the pet extension */
    mtmp2 = newmonst(MX_EDOG, mtmp->mnamelth);
    *mtmp2 = *mtmp;
    mtmp2->mxtyp = MX_EDOG;
    mtmp2->mxlth = sizeof (struct edog);
    if (mtmp->mnamelth)
        strcpy(NAME_MUTABLE(mtmp2), NAME(mtmp));
    initedog(mtmp2);
    replmon(mtmp, mtmp2);
    /* `mtmp' is now obsolete */

    if (obj) {  /* thrown food */
        /* defer eating until the edog extension has been set up */
        place_object(obj, level, mtmp2->mx, mtmp2->my); /* put on floor */
        /* devour the food (might grow into larger, genocided monster) */
        if (dog_eat(mtmp2, obj, mtmp2->mx, mtmp2->my, TRUE) == 2)
            return mtmp2;       /* oops, it died... */
        /* `obj' is now obsolete */
    }

    if (mtmp2->dlevel == level)
        newsym(mtmp2->mx, mtmp2->my);
    if (attacktype(mtmp2->data, AT_WEAP)) {
        mtmp2->weapon_check = NEED_HTH_WEAPON;
        mon_wield_item(mtmp2);
    }
    return mtmp2;
}

/*
 * Called during pet revival or pet life-saving.
 * If you killed the pet, it revives wild.
 * If you abused the pet a lot while alive, it revives wild.
 * If you abused the pet at all while alive, it revives untame.
 * If the pet wasn't abused and was very tame, it might revive tame.
 */
void
wary_dog(struct monst *mtmp, boolean was_dead)
{
    struct edog *edog;
    boolean quietly = was_dead;

    mtmp->meating = 0;
    if (!mtmp->mtame)
        return;
    edog = !mtmp->isminion ? EDOG(mtmp) : 0;

    /* if monster was starving when it died, undo that now */
    if (edog && edog->mhpmax_penalty) {
        mtmp->mhpmax += edog->mhpmax_penalty;
        mtmp->mhp += edog->mhpmax_penalty;      /* heal it */
        edog->mhpmax_penalty = 0;
    }

    if (edog && (edog->killed_by_u == 1 || edog->abuse > 2)) {
        msethostility(mtmp, TRUE, FALSE);
        if (edog->abuse >= 0 && edog->abuse < 10)
            if (!rn2_on_rng(edog->abuse + 1, rng_dog_untame))
                msethostility(mtmp, FALSE, FALSE);
        if (!quietly && cansee(mtmp->mx, mtmp->my)) {
            if (haseyes(youmonst.data)) {
                if (haseyes(mtmp->data))
                    pline("%s %s to look you in the %s.", Monnam(mtmp),
                          mtmp->mpeaceful ? "seems unable" : "refuses",
                          body_part(EYE));
                else
                    pline("%s avoids your gaze.", Monnam(mtmp));
            }
        }
    } else {
        /* chance it goes wild anyway - Pet Semetary */
        if (rn2_on_rng(mtmp->mtame, rng_dog_untame) == mtmp->mtame - 1)
            msethostility(mtmp, TRUE, FALSE);
    }
    if (!mtmp->mtame) {
        newsym(mtmp->mx, mtmp->my);
        /* a life-saved monster might be leashed; don't leave it that way if
           it's no longer tame */
        if (mtmp->mleashed)
            m_unleash(mtmp, TRUE);
    }

    /* if its still a pet, start a clean pet-slate now */
    if (edog && mtmp->mtame) {
        edog->revivals++;
        edog->killed_by_u = 0;
        edog->abuse = 0;
        if (was_dead || edog->hungrytime < moves + 500L)
            edog->hungrytime = moves + 500L;
        if (was_dead) {
            edog->droptime = 0L;
            edog->dropdist = 10000;
            edog->whistletime = 0L;
            edog->apport = 5;
        }       /* else lifesaved, so retain current values */
    }
}

void
abuse_dog(struct monst *mtmp)
{
    if (!mtmp->mtame)
        return;

    if (Aggravate_monster || Conflict)
        mtmp->mtame /= 2;
    else
        mtmp->mtame--;

    if (mtmp->mtame && !mtmp->isminion)
        EDOG(mtmp)->abuse++;

    if (!mtmp->mtame && mtmp->mleashed)
        m_unleash(mtmp, TRUE);

    /* don't make a sound if pet is in the middle of leaving the level */
    /* newsym isn't necessary in this case either */
    if (mtmp->mx != COLNO) {
        if (mtmp->mtame && rn2(mtmp->mtame))
            yelp(mtmp);
        else
            growl(mtmp);        /* give them a moment's worry */

        if (!mtmp->mtame && mtmp->dlevel == level)
            newsym(mtmp->mx, mtmp->my);
    }
}

/*dog.c*/
