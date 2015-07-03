/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static void dofiretrap(struct obj *);
static void domagictrap(void);
static boolean emergency_disrobe(boolean *);
static int untrap_prob(struct trap *ttmp);
static void move_into_trap(struct trap *);
static int try_disarm(struct trap *, boolean, schar, schar);
static void reward_untrap(struct trap *, struct monst *);
static int disarm_holdingtrap(struct trap *, schar, schar);
static int disarm_landmine(struct trap *, schar, schar);
static int disarm_squeaky_board(struct trap *, schar, schar);
static int disarm_shooting_trap(struct trap *, int, schar, schar);
static int try_lift(struct monst *, struct trap *, int, boolean);
static int help_monster_out(struct monst *, struct trap *);
static boolean thitm(int, struct monst *, struct obj *, int, boolean);
static int mkroll_launch(struct trap *, struct level *lev, xchar, xchar, short,
                         long, enum rng);
static boolean isclearpath(struct level *lev, coord *, int, schar, schar);
static int steedintrap(struct trap *, struct obj *);
static boolean keep_saddle_with_steedcorpse(unsigned, struct obj *,
                                            struct obj *);
static int launch_obj(short, int, int, int, int, int);


static const char *const a_your[2] = { "a", "your" };
static const char *const A_Your[2] = { "A", "Your" };

static const char tower_of_flame[] = "tower of flame";
static const char *const A_gush_of_water_hits = "A gush of water hits";
static const char *const blindgas[6] =
    { "humid", "odorless", "pungent", "chilling", "acrid", "biting" };


/* called when you're hit by fire (dofiretrap,buzz,zapyourself,explode) */
/* returns TRUE if hit on torso */
boolean
burnarmor(struct monst *victim)
{
    struct obj *item;

    if (!victim)
        return 0;
#define burn_dmg(obj,descr) erode_obj(obj, descr, ERODE_BURN, TRUE, FALSE)
    while (1) {
        switch (rn2(6)) {
        case 0:
            item = which_armor(victim, os_armh);
            if (!burn_dmg(item, item ?
                     msgcat_many(materialnm[objects[item->otyp].oc_material],
                                 " ", helmet_name(item), NULL) : "helmet"))
                continue;
            break;
        case 1:
            item = which_armor(victim, os_armc);
            if (item) {
                burn_dmg(item, cloak_simple_name(item));
                return TRUE;
            }
            item = which_armor(victim, os_arm);
            if (item) {
                burn_dmg(item, xname(item));
                return TRUE;
            }
            item = which_armor(victim, os_armu);
            if (item)
                burn_dmg(item, "shirt");
            return TRUE;
        case 2:
            if ((item = which_armor(victim, os_arms))) {
                if (!burn_dmg(item, "wooden shield"))
                    continue;
            } else if (victim == &youmonst && u.twoweap &&
                       (item = which_armor(victim, os_swapwep))) {
                if (!burn_dmg(item, xname(item)))
                    continue;
            }
            break;
        case 3:
            item = which_armor(victim, os_armg);
            if (!burn_dmg(item, "gloves"))
                continue;
            break;
        case 4:
            item = which_armor(victim, os_armf);
            if (!burn_dmg(item, "boots"))
                continue;
            break;
        case 5:
            item = which_armor(victim, os_wep);
            if (item && !burn_dmg(item, xname(item)))
                continue;
            break;
        }
        break;  /* Out of while loop */
    }
    return FALSE;
#undef burn_dmg
}

/*
 * Generic erode-item function.  Returns TRUE if any change in state occurred,
 * or if grease protected item.
 * "check_grease", if FALSE, means that grease is not checked for.
 * "print", if set, means to print a message even if no change occurs.
 */
boolean
erode_obj(struct obj * otmp, const char *ostr, enum erode_type type,
          boolean check_grease, boolean print)
{
    static const char *const action[] =
        { "smoulder", "rust", "rot", "corrode" };
    static const char *const msg[] =
        { "burnt", "rusted", "rotten", "corroded" };

    if (!otmp)
        return FALSE;

    boolean vulnerable = FALSE;
    boolean primary = TRUE;

    struct monst *victim =
        carried(otmp) ? &youmonst : mcarried(otmp) ? otmp->ocarry : NULL;
    boolean vismon = victim && (victim != &youmonst) && canseemon(victim);
    /* FIXME: bhitpos is stupid, but due to the throwing code, we must use it */
    boolean visobj = !victim && cansee(bhitpos.x, bhitpos.y);

    switch (type) {
    case ERODE_BURN:
        vulnerable = is_flammable(otmp);
        check_grease = FALSE;
        break;
    case ERODE_RUST:
        vulnerable = is_rustprone(otmp);
        break;
    case ERODE_ROT:
        vulnerable = is_rottable(otmp);
        check_grease = FALSE;
        primary = FALSE;
        break;
    case ERODE_CORRODE:
        vulnerable = is_corrodeable(otmp);
        primary = FALSE;
        break;
    default:
        impossible("Invalid erosion type in erode_obj");
        return FALSE;
    }

    int erosion = primary ? otmp->oeroded : otmp->oeroded2;
    if (!ostr)
        ostr = cxname(otmp);

    if (check_grease && otmp->greased) {
        grease_protect(otmp, ostr, victim);
        return TRUE;
    } else if (!vulnerable || (otmp->oerodeproof && otmp->rknown)) {
        if (print && flags.verbose) {
            if (victim == &youmonst)
                pline("Your %s %s not affected.", ostr, vtense(ostr, "are"));
            else if (vismon)
                pline("%s %s %s not affected.", s_suffix(Monnam(victim)), ostr,
                      vtense(ostr, "are"));
        }
        return FALSE;
    } else if (otmp->oerodeproof || (otmp->blessed && !rnl(4))) {
        if (flags.verbose && (print || otmp->oerodeproof)) {
            if (victim == &youmonst)
                pline("Somehow, your %s %s not affected.", ostr,
                      vtense(ostr, "are"));
            else if (vismon)
                pline("Somehow, %s %s %s not affected.",
                      s_suffix(mon_nam(victim)), ostr, vtense(ostr, "are"));
            else if (visobj)
                pline("Somehow, the %s %s not affected.", ostr,
                      vtense(ostr, "are"));
        }
        /* We assume here that if the object is protected because it is blessed,
         * it still shows some minor signs of wear, and the hero can distinguish
         * this from an object that is actually proof against damage.
         */
        if (otmp->oerodeproof)
            otmp->rknown = TRUE;
        return FALSE;
    } else if (erosion < MAX_ERODE) {
        const char *adverb = erosion + 1 == MAX_ERODE ?
            " completely" : erosion ? " further" : "";
        if (victim == &youmonst)
            pline("Your %s %s%s!", ostr, vtense(ostr, action[type]), adverb);
        else if (vismon)
            pline("%s %s %s%s!", s_suffix(Monnam(victim)), ostr,
                  vtense(ostr, action[type]), adverb);
        else if (visobj)
            pline("The %s %s%s!", ostr, vtense(ostr, action[type]), adverb);

        if (primary)
            otmp->oeroded++;
        else
            otmp->oeroded2++;

        if (otmp->unpaid)
            costly_damage_obj(otmp);
        update_inventory();
        return TRUE;
    } else {
        if (flags.verbose && print) {
            if (victim == &youmonst)
                pline("Your %s %s completely %s.", ostr,
                      vtense(ostr, Blind ? "feel" : "look"), msg[type]);
            else if (vismon)
                pline("%s %s %s completely %s.", s_suffix(Monnam(victim)), ostr,
                      vtense(ostr, "look"), msg[type]);
            else if (visobj)
                pline("The %s %s completely %s.", ostr, vtense(ostr, "look"),
                      msg[type]);
        }
        if (otmp->unpaid) {
            costly_damage_obj(otmp);
            update_inventory();
            return TRUE;
        }
        return FALSE;
    }
}

/* Protect an item from erosion with grease. Returns TRUE if the grease wears
   off. */
boolean
grease_protect(struct obj *otmp, const char *ostr, struct monst *victim)
{
    static const char txt[] = "protected by the layer of grease!";
    boolean vismon = victim && (victim != &youmonst) && canseemon(victim);

    if (ostr) {
        if (victim == &youmonst)
            pline("Your %s %s %s", ostr, vtense(ostr, "are"), txt);
        else if (vismon)
            pline("%s's %s %s %s", Monnam(victim), ostr, vtense(ostr, "are"),
                  txt);
    } else {
        if (victim == &youmonst)
            pline("Your %s %s", aobjnam(otmp, "are"), txt);
        else if (vismon)
            pline("%s's %s %s", Monnam(victim), aobjnam(otmp, "are"), txt);
    }
    if (!rn2(2)) {
        otmp->greased = 0;
        if (carried(otmp)) {
            pline("The grease dissolves.");
            update_inventory();
        }
        return TRUE;
    }
    return FALSE;
}

struct trap *
maketrap(struct level *lev, int x, int y, int typ, enum rng rng)
{
    struct trap *ttmp;
    struct rm *loc;
    boolean oldplace;

    if ((ttmp = t_at(lev, x, y)) != 0) {
        if (ttmp->ttyp == MAGIC_PORTAL)
            return NULL;
        if (ttmp->ttyp == VIBRATING_SQUARE)
            return NULL;
        oldplace = TRUE;
        if (u.utrap && (x == u.ux) && (y == u.uy) &&
            ((u.utraptype == TT_BEARTRAP && typ != BEAR_TRAP) ||
             (u.utraptype == TT_WEB && typ != WEB) ||
             (u.utraptype == TT_PIT && typ != PIT &&
              typ != SPIKED_PIT)))
            u.utrap = 0;
    } else {
        oldplace = FALSE;
        ttmp = newtrap();
        memset(ttmp, 0, sizeof (struct trap));
        ttmp->tx = x;
        ttmp->ty = y;
        ttmp->launch.x = -1;    /* force error if used before set */
        ttmp->launch.y = -1;
    }
    ttmp->ttyp = typ;
    switch (typ) {
    case STATUE_TRAP:  /* create a "living" statue */
        {
            struct monst *mtmp;
            struct obj *otmp, *statue;

            /* use the provided RNG for species but nothing else */
            statue = mkcorpstat(STATUE, NULL, &mons[rndmonnum(&lev->z, rng)],
                                lev, x, y, FALSE, rng_main);
            mtmp = makemon(&mons[statue->corpsenm], lev, COLNO, ROWNO,
                           NO_MM_FLAGS);
            if (!mtmp)
                break;  /* should never happen */
            while (mtmp->minvent) {
                otmp = mtmp->minvent;
                otmp->owornmask = 0;
                obj_extract_self(otmp);
                add_to_container(statue, otmp);
            }
            statue->owt = weight(statue);
            mongone(mtmp);
            break;
        }
    case ROLLING_BOULDER_TRAP: /* boulder will roll towards trigger */
        mkroll_launch(ttmp, lev, x, y, BOULDER, 1L, rng);
        break;
    case HOLE:
    case PIT:
    case SPIKED_PIT:
    case TRAPDOOR:
        loc = &lev->locations[x][y];
        if (*in_rooms(lev, x, y, SHOPBASE) &&
            (typ == HOLE || typ == TRAPDOOR || typ == PIT || IS_DOOR(loc->typ)
             || IS_WALL(loc->typ)))
            add_damage(x, y,    /* schedule repair */
                       ((IS_DOOR(loc->typ) || IS_WALL(loc->typ))
                        && !flags.mon_moving) ? 200L : 0L);
        loc->doormask = 0;      /* subsumes altarmask, icedpool... */
        if (IS_ROOM(loc->typ))  /* && !IS_AIR(loc->typ) */
            loc->typ = ROOM;

        /*
         * some cases which can happen when digging
         * down while phazing thru solid areas
         */
        else if (loc->typ == STONE || loc->typ == SCORR)
            loc->typ = CORR;
        else if (IS_WALL(loc->typ) || loc->typ == SDOOR)
            loc->typ =
                lev->flags.is_maze_lev ? ROOM : lev->
                flags.is_cavernous_lev ? CORR : DOOR;

        unearth_objs(lev, x, y);
        break;
    }
    if (ttmp->ttyp == HOLE)
        ttmp->tseen = 1;        /* You can't hide a hole */
    else
        ttmp->tseen = 0;
    ttmp->once = 0;
    ttmp->madeby_u = 0;
    ttmp->dst.dnum = -1;
    ttmp->dst.dlevel = -1;
    if (!oldplace) {
        ttmp->ntrap = lev->lev_traps;
        lev->lev_traps = ttmp;
    }
    return ttmp;
}

void
fall_through(boolean td)
{       /* td == TRUE : trap door or hole */
    d_level dtmp;
    const char *dont_fall = NULL;
    int newlevel = dunlev(&u.uz);
    const char *msgbuf;

    /* KMH -- You can't escape the Sokoban level traps */
    if (Blind && Levitation && !In_sokoban(&u.uz))
        return;

    do
        newlevel++;
    while (!rn2_on_rng(4, rng_trapdoor_result) &&
           newlevel < dunlevs_in_dungeon(&u.uz));

    if (td) {
        struct trap *t = t_at(level, u.ux, u.uy);

        seetrap(t);
        if (!In_sokoban(&u.uz)) {
            if (t->ttyp == TRAPDOOR)
                pline("A trap door opens up under you!");
            else
                pline("There's a gaping hole under you!");
        }
    } else
        pline("The %s opens up under you!", surface(u.ux, u.uy));

    if (In_sokoban(&u.uz) && can_fall_thru(level))
        ;
    /* KMH -- You can't escape the Sokoban level traps */
    else if (Levitation || u.ustuck || !can_fall_thru(level)
             || Flying || is_clinger(youmonst.data)
             || (Inhell && !u.uevent.invoked &&
                 newlevel == dunlevs_in_dungeon(&u.uz))) {
        dont_fall = "You don't fall in.";
    } else if (youmonst.data->msize >= MZ_HUGE) {
        dont_fall = "You don't fit through.";
    } else if (!next_to_u()) {
        dont_fall = "You are jerked back by your pet!";
    }
    if (dont_fall) {
        pline("%s", dont_fall);
        /* hero didn't fall through, but any objects here might */
        impact_drop(NULL, u.ux, u.uy, 0);
        if (!td) {
            win_pause_output(P_MESSAGE);
            pline("The opening under you closes up.");
        }
        return;
    }

    if (*u.ushops)
        shopdig(1);
    if (Is_stronghold(&u.uz)) {
        find_hell(&dtmp);
    } else {
        dtmp.dnum = u.uz.dnum;
        dtmp.dlevel = newlevel;
    }
    if (!td)
        msgbuf = msgprintf("The hole in the %s above you closes up.",
                           ceiling(u.ux, u.uy));
    else
        msgbuf = NULL;
    schedule_goto(&dtmp, FALSE, TRUE, 0, NULL, msgbuf);
}

/*
 * Animate the given statue.  May have been via shatter attempt, trap,
 * or stone to flesh spell.  Return a monster if successfully animated.
 * If the monster is animated, the object is deleted.  If fail_reason
 * is non-null, then fill in the reason for failure (or success).
 *
 * The cause of animation is:
 *
 *     ANIMATE_NORMAL  - hero "finds" the monster
 *     ANIMATE_SHATTER - hero tries to destroy the statue
 *      ANIMATE_SPELL   - stone to flesh spell hits the statue
 *
 * Perhaps x, y is not needed if we can use get_obj_location() to find
 * the statue's location... ???
 */
struct monst *
animate_statue(struct obj *statue, xchar x, xchar y, int cause,
               int *fail_reason)
{
    const struct permonst *mptr;
    struct monst *mon = 0;
    struct obj *item;
    coord cc;
    boolean historic = (Role_if(PM_ARCHEOLOGIST) && !flags.mon_moving &&
                        (statue->spe & STATUE_HISTORIC));
    const char *statuename = the(xname(statue));

    if (statue->oxlth && statue->oattached == OATTACHED_MONST) {
        cc.x = x, cc.y = y;
        mon = montraits(statue, &cc);
        if (mon && mon->mtame && !mon->isminion)
            wary_dog(mon, TRUE);
    } else {
        /* statue of any golem hit with stone-to-flesh becomes flesh golem */
        if (is_golem(&mons[statue->corpsenm]) && cause == ANIMATE_SPELL)
            mptr = &mons[PM_FLESH_GOLEM];
        else
            mptr = &mons[statue->corpsenm];
        /*
         * Guard against someone wishing for a statue of a unique monster
         * (which is allowed in normal play) and then tossing it onto the
         * [detected or guessed] location of a statue trap.  Normally the
         * uppermost statue is the one which would be activated.
         */
        if ((mptr->geno & G_UNIQ) && cause != ANIMATE_SPELL) {
            if (fail_reason)
                *fail_reason = AS_MON_IS_UNIQUE;
            return NULL;
        }
        if (cause == ANIMATE_SPELL &&
            ((mptr->geno & G_UNIQ) || mptr->msound == MS_GUARDIAN)) {
            /* Statues of quest guardians or unique monsters will not
               stone-to-flesh as the real thing. */
            mon = makemon(&mons[PM_DOPPELGANGER], level, x, y,
                          NO_MINVENT | MM_NOCOUNTBIRTH | MM_ADJACENTOK);
            if (mon) {
                /* makemon() will set mon->cham to CHAM_ORDINARY if hero is
                   wearing ring of protection from shape changers when
                   makemon() is called, so we have to check the field before
                   calling newcham(). */
                if (mon->cham == CHAM_DOPPELGANGER)
                    newcham(mon, mptr, FALSE, FALSE);
            }
        } else
            mon = makemon(mptr, level, x, y, (cause == ANIMATE_SPELL) ?
                          (NO_MINVENT | MM_ADJACENTOK) : NO_MINVENT);
    }

    if (!mon) {
        if (fail_reason)
            *fail_reason = AS_NO_MON;
        return NULL;
    }

    /* in case statue is wielded and hero zaps stone-to-flesh at self */
    if (statue->owornmask)
        remove_worn_item(statue, TRUE);

    /* allow statues to be of a specific gender */
    if (statue->spe & STATUE_MALE)
        mon->female = FALSE;
    else if (statue->spe & STATUE_FEMALE)
        mon->female = TRUE;
    /* if statue has been named, give same name to the monster */
    if (statue->onamelth)
        mon = christen_monst(mon, ONAME(statue));
    /* transfer any statue contents to monster's inventory */
    while ((item = statue->cobj) != 0) {
        obj_extract_self(item);
        add_to_minv(mon, item);
    }
    m_dowear(mon, TRUE);
    delobj(statue);

    /* mimic statue becomes seen mimic; other hiders won't be hidden */
    if (mon->m_ap_type)
        seemimic(mon);
    else
        mon->mundetected = FALSE;

    const char *comes_to_life =
        nonliving(mon->data) ? "moves" : "comes to life";

    if ((x == u.ux && y == u.uy) || cause == ANIMATE_SPELL) {
        if (cause == ANIMATE_SPELL)
            pline("%s %s!", msgupcasefirst(statuename),
                  canspotmon(mon) ? comes_to_life : "disappears");
        else
            pline("The statue %s!",
                  canspotmon(mon) ? comes_to_life : "disappears");
        if (historic) {
            pline("You feel guilty that the historic statue is now gone.");
            adjalign(-1);
        }
    } else if (cause == ANIMATE_SHATTER) {
        pline("Instead of shattering, the statue suddenly %s!",
              canspotmon(mon) ? comes_to_life : "disappears");
    } else {      /* cause == ANIMATE_NORMAL */
        pline("You find %s posing as a statue.",
              canspotmon(mon) ? a_monnam(mon) : "something");
        action_interrupted();
    }
    /* avoid hiding under nothing */
    if (x == u.ux && y == u.uy && Upolyd && hides_under(youmonst.data) &&
        !OBJ_AT(x, y))
        u.uundetected = 0;

    if (fail_reason)
        *fail_reason = AS_OK;
    return mon;
}

/* You've either stepped onto a statue trap's location or you've triggered a
   statue trap by searching next to it or by trying to break it with a wand or
   pick-axe. */
struct monst *
activate_statue_trap(struct trap *trap, xchar x, xchar y, boolean shatter)
{
    struct monst *mtmp = NULL;
    struct obj *otmp = sobj_at(STATUE, level, x, y);
    int fail_reason;

    /* Try to animate the first valid statue. Stop the loop when we actually
       create something or the failure cause is not because the mon was
       unique. */
    deltrap(level, trap);
    while (otmp) {
        mtmp =
            animate_statue(otmp, x, y,
                           shatter ? ANIMATE_SHATTER : ANIMATE_NORMAL,
                           &fail_reason);
        if (mtmp || fail_reason != AS_MON_IS_UNIQUE)
            break;

        while ((otmp = otmp->nexthere) != 0)
            if (otmp->otyp == STATUE)
                break;
    }

    if (Blind)
        feel_location(x, y);
    else
        newsym(x, y);
    return mtmp;
}


static boolean
keep_saddle_with_steedcorpse(unsigned steed_mid, struct obj *objchn,
                             struct obj *saddle)
{
    if (!saddle)
        return FALSE;
    while (objchn) {
        if (objchn->otyp == CORPSE && objchn->oattached == OATTACHED_MONST &&
            objchn->oxlth) {
            struct monst *mtmp = (struct monst *)objchn->oextra;

            if (mtmp->m_id == steed_mid) {
                /* move saddle */
                xchar x, y;

                if (get_obj_location(objchn, &x, &y, 0)) {
                    obj_extract_self(saddle);
                    place_object(saddle, level, x, y);
                    stackobj(saddle);
                }
                return TRUE;
            }
        }
        if (Has_contents(objchn) &&
            keep_saddle_with_steedcorpse(steed_mid, objchn->cobj, saddle))
            return TRUE;
        objchn = objchn->nobj;
    }
    return FALSE;
}


void
dotrap(struct trap *trap, unsigned trflags)
{
    int ttype = trap->ttyp;
    struct obj *otmp;
    boolean already_seen = trap->tseen;
    boolean webmsgok = (!(trflags & NOWEBMSG));
    boolean forcebungle = (trflags & FORCEBUNGLE);

    action_interrupted();

    /* KMH -- You can't escape the Sokoban level traps */
    if (In_sokoban(&u.uz) &&
        (ttype == PIT || ttype == SPIKED_PIT || ttype == HOLE ||
         ttype == TRAPDOOR)) {
        /* The "air currents" message is still appropriate -- even when the
           hero isn't flying or levitating -- because it conveys the reason why
           the player cannot escape the trap with a dexterity check, clinging
           to the ceiling, etc. */
        pline("Air currents pull you down into %s %s!", a_your[trap->madeby_u],
              trapexplain[ttype - 1]);
        /* then proceed to normal trap effect */
    } else if (already_seen) {
        if ((Levitation || Flying) &&
            (ttype == PIT || ttype == SPIKED_PIT || ttype == HOLE ||
             ttype == BEAR_TRAP)) {
            pline("You %s over %s %s.", Levitation ? "float" : "fly",
                  a_your[trap->madeby_u], trapexplain[ttype - 1]);
            return;
        }
        if (!Fumbling && ttype != MAGIC_PORTAL && ttype != VIBRATING_SQUARE &&
            ttype != ANTI_MAGIC && !forcebungle &&
            (!rn2(5) || ((ttype == PIT || ttype == SPIKED_PIT) &&
                         is_clinger(youmonst.data)))) {
            pline("You escape %s %s.",
                  (ttype == ARROW_TRAP &&
                   !trap->madeby_u) ? "an" : a_your[trap->madeby_u],
                  trapexplain[ttype - 1]);
            return;
        }
    }

    if (u.usteed)
        u.usteed->mtrapseen |= (1 << (ttype - 1));

    switch (ttype) {
    case ARROW_TRAP:
        if (trap->once && trap->tseen && !rn2(15)) {
            You_hear("a loud click!");
            deltrap(level, trap);
            newsym(u.ux, u.uy);
            break;
        }
        trap->once = 1;
        seetrap(trap);
        pline("An arrow shoots out at you!");
        otmp = mksobj(level, ARROW, TRUE, FALSE, rng_main);
        otmp->quan = 1L;
        otmp->owt = weight(otmp);
        otmp->opoisoned = 0;

        if (u.usteed && !rn2(2) && steedintrap(trap, otmp))     /* nothing */
            ;
        else if (thitu(8, dmgval(otmp, &youmonst), otmp, "arrow")) {
            obfree(otmp, NULL);
        } else {
            place_object(otmp, level, u.ux, u.uy);
            if (!Blind)
                otmp->dknown = 1;
            stackobj(otmp);
            newsym(u.ux, u.uy);
        }
        break;
    case DART_TRAP:
        if (trap->once && trap->tseen && !rn2(15)) {
            You_hear("a soft click.");
            deltrap(level, trap);
            newsym(u.ux, u.uy);
            break;
        }
        trap->once = 1;
        seetrap(trap);
        pline("A little dart shoots out at you!");
        otmp = mksobj(level, DART, TRUE, FALSE, rng_main);
        otmp->quan = 1L;
        otmp->owt = weight(otmp);
        if (!rn2(6))
            otmp->opoisoned = 1;
        if (u.usteed && !rn2(2) && steedintrap(trap, otmp))     /* nothing */
            ;
        else if (thitu(7, dmgval(otmp, &youmonst), otmp, "little dart")) {
            if (otmp->opoisoned)
                poisoned("dart", A_CON, killer_msg(POISONING, "a little dart"),
                         -10);
            obfree(otmp, NULL);
        } else {
            place_object(otmp, level, u.ux, u.uy);
            if (!Blind)
                otmp->dknown = 1;
            stackobj(otmp);
            newsym(u.ux, u.uy);
        }
        break;
    case ROCKTRAP:
        if (trap->once && trap->tseen && !rn2(15)) {
            pline("A trap door in %s opens, but nothing falls out!",
                  the(ceiling(u.ux, u.uy)));
            deltrap(level, trap);
            newsym(u.ux, u.uy);
        } else {
            int dmg = dice(2, 6);       /* should be std ROCK dmg? */

            trap->once = 1;
            seetrap(trap);
            otmp = mksobj_at(ROCK, level, u.ux, u.uy, TRUE, FALSE, rng_main);
            otmp->quan = 1L;
            otmp->owt = weight(otmp);

            pline("A trap door in %s opens and %s falls on your %s!",
                  the(ceiling(u.ux, u.uy)), an(xname(otmp)), body_part(HEAD));

            if (uarmh) {
                if (is_metallic(uarmh)) {
                    pline("Fortunately, you are wearing a hard %s.",
                          helmet_name(uarmh));
                    dmg = 2;
                } else if (flags.verbose) {
                    pline("Your %s does not protect you.", xname(uarmh));
                }
            }

            if (!Blind)
                otmp->dknown = 1;
            stackobj(otmp);
            newsym(u.ux, u.uy); /* map the rock */

            losehp(dmg, killer_msg(DIED, "a falling rock"));
            exercise(A_STR, FALSE);
        }
        break;

    case SQKY_BOARD:   /* stepped on a squeaky board */
        if (Levitation || Flying) {
            if (!Blind) {
                seetrap(trap);
                if (Hallucination)
                    pline("You notice a crease in the linoleum.");
                else
                    pline("You notice a loose board below you.");
            }
        } else {
            seetrap(trap);
            pline("A board beneath you squeaks loudly.");
            wake_nearby(FALSE);
        }
        break;

    case BEAR_TRAP:
        if (Levitation || Flying)
            break;
        seetrap(trap);
        if (amorphous(youmonst.data) || is_whirly(youmonst.data) ||
            unsolid(youmonst.data)) {
            pline("%s bear trap closes harmlessly through you.",
                  A_Your[trap->madeby_u]);
            break;
        }
        if (!u.usteed && youmonst.data->msize <= MZ_SMALL) {
            pline("%s bear trap closes harmlessly over you.",
                  A_Your[trap->madeby_u]);
            break;
        }
        u.utrap = rn1(4, 4);
        u.utraptype = TT_BEARTRAP;
        if (u.usteed) {
            pline("%s bear trap closes on %s %s!", A_Your[trap->madeby_u],
                  s_suffix(mon_nam(u.usteed)), mbodypart(u.usteed, FOOT));
        } else {
            pline("%s bear trap closes on your %s!", A_Your[trap->madeby_u],
                  body_part(FOOT));
            if (u.umonnum == PM_OWLBEAR || u.umonnum == PM_BUGBEAR)
                pline("You howl in anger!");
        }
        exercise(A_DEX, FALSE);
        break;

    case SLP_GAS_TRAP:
        seetrap(trap);
        if (Sleep_resistance || breathless(youmonst.data)) {
            pline("You are enveloped in a cloud of gas!");
        } else {
            pline("A cloud of gas puts you to sleep!");
            helpless(rnd(25), hr_asleep, "sleeping", NULL);
        }
        steedintrap(trap, NULL);
        break;

    case RUST_TRAP:
        seetrap(trap);

        /* Unlike monsters, traps cannot aim their rust attacks at you, so
           instead of looping through and taking either the first rustable one
           or the body, we take whatever we get, even if it is not rustable. */
        switch (rn2(5)) {
        case 0:
            pline("%s you on the %s!", A_gush_of_water_hits, body_part(HEAD));
            water_damage(uarmh, maybe_helmet_name(uarmh), TRUE);
            break;
        case 1:
            pline("%s your left %s!", A_gush_of_water_hits, body_part(ARM));
            if (water_damage(uarms, "shield", TRUE))
                break;
            if (u.twoweap || (uwep && bimanual(uwep)))
                water_damage(u.twoweap ? uswapwep : uwep, NULL, TRUE);
        glovecheck:
            water_damage(uarmg, "gauntlets", TRUE);
            /* Not "metal gauntlets" since it gets called even if it's leather
               for the message */
            break;
        case 2:
            pline("%s your right %s!", A_gush_of_water_hits, body_part(ARM));
            water_damage(uwep, NULL, TRUE);
            goto glovecheck;
        default:
            pline("%s you!", A_gush_of_water_hits);
            for (otmp = invent; otmp; otmp = otmp->nobj)
                snuff_lit(otmp);
            if (uarmc)
                water_damage(uarmc, cloak_simple_name(uarmc), TRUE);
            else if (uarm)
                water_damage(uarm, "armor", TRUE);
            else if (uarmu)
                water_damage(uarmu, "shirt", TRUE);
        }
        update_inventory();

        if (u.umonnum == PM_IRON_GOLEM) {
            int dam = u.mhmax;

            pline("You are covered with rust!");
            if (Half_physical_damage)
                dam = (dam + 1) / 2;
            losehp(dam, "rusted away by a rust trap");
            break;
        } else if (u.umonnum == PM_GREMLIN && rn2(3)) {
            split_mon(&youmonst, NULL);
            break;
        }
        break;

    case FIRE_TRAP:
        seetrap(trap);
        dofiretrap(NULL);
        break;

    case PIT:
    case SPIKED_PIT:
        /* KMH -- You can't escape the Sokoban level traps */
        if (!In_sokoban(&u.uz) && (Levitation || Flying))
            break;
        seetrap(trap);
        if (!In_sokoban(&u.uz) && is_clinger(youmonst.data)) {
            if (trap->tseen) {
                pline("You see %s %spit below you.", a_your[trap->madeby_u],
                      ttype == SPIKED_PIT ? "spiked " : "");
            } else {
                pline("%s pit %sopens up under you!", A_Your[trap->madeby_u],
                      ttype == SPIKED_PIT ? "full of spikes " : "");
                pline("You don't fall in!");
            }
            break;
        }
        if (!In_sokoban(&u.uz)) {
            const char *verbbuf;

            if (u.usteed) {
                if ((trflags & RECURSIVETRAP) != 0)
                    verbbuf = msgprintf(
                        "and %s fall",
                        x_monnam(u.usteed,
                                 u.usteed->mnamelth ? ARTICLE_NONE :
                                 ARTICLE_THE, NULL, SUPPRESS_SADDLE,
                                 FALSE));
                else
                    verbbuf = msgcat(
                        "lead ",
                        x_monnam(u.usteed,
                                 u.usteed->mnamelth ? ARTICLE_NONE :
                                 ARTICLE_THE, "poor", SUPPRESS_SADDLE,
                                 FALSE));
            } else
                verbbuf = "fall";
            pline("You %s into %s pit!", verbbuf, a_your[trap->madeby_u]);
        }
        /* wumpus reference */
        if (Role_if(PM_RANGER) && !trap->madeby_u && !trap->once &&
            In_quest(&u.uz) && Is_qlocate(&u.uz)) {
            pline("Fortunately it has a bottom after all...");
            trap->once = 1;
        } else if (u.umonnum == PM_PIT_VIPER || u.umonnum == PM_PIT_FIEND)
            pline("How pitiful.  Isn't that the pits?");
        if (ttype == SPIKED_PIT) {
            const char *predicament = "on a set of sharp iron spikes";

            if (u.usteed) {
                pline("%s lands %s!",
                      msgupcasefirst(
                          x_monnam(
                              u.usteed,
                              u.usteed->mnamelth ? ARTICLE_NONE : ARTICLE_THE,
                              "poor", SUPPRESS_SADDLE, FALSE)), predicament);
            } else
                pline("You land %s!", predicament);
        }
        if (!Passes_walls) {
            u.utrap = rn1(6, 2);
            u.utraptype = TT_PIT;
            turnstate.vision_full_recalc = TRUE;
        }
        if (!steedintrap(trap, NULL)) {
            if (ttype == SPIKED_PIT) {
                losehp(rnd(10), "fell into a pit of iron spikes");
                if (!rn2(6))
                    poisoned("spikes", A_STR,
                             killer_msg(DIED, "a fall onto poison spikes"),
                             8);
            } else
                losehp(rnd(6), "fell into a pit");
            if (Punished && !carried(uball)) {
                unplacebc();
                ballfall();
                placebc();
            }
            selftouch("Falling, you", "falling into a pit while wielding");
            turnstate.vision_full_recalc = TRUE;     /* vision limits change */
            exercise(A_STR, FALSE);
            exercise(A_DEX, FALSE);
        }
        break;
    case HOLE:
    case TRAPDOOR:
        if (!can_fall_thru(level)) {
            seetrap(trap);      /* normally done in fall_through */
            impossible("dotrap: %ss cannot exist on this level.",
                       trapexplain[ttype - 1]);
            break;      /* don't activate it after all */
        }
        fall_through(TRUE);
        break;

    case TELEP_TRAP:
        seetrap(trap);
        tele_trap(trap);
        break;
    case LEVEL_TELEP:
        seetrap(trap);
        level_tele_trap(trap);
        break;

    case WEB:  /* Our luckless player has stumbled into a web. */
        seetrap(trap);
        if (amorphous(youmonst.data) || is_whirly(youmonst.data) ||
            unsolid(youmonst.data)) {
            if (acidic(youmonst.data) || u.umonnum == PM_GELATINOUS_CUBE ||
                u.umonnum == PM_FIRE_ELEMENTAL) {
                if (webmsgok)
                    pline("You %s %s spider web!",
                          (u.umonnum ==
                           PM_FIRE_ELEMENTAL) ? "burn" : "dissolve",
                          a_your[trap->madeby_u]);
                deltrap(level, trap);
                newsym(u.ux, u.uy);
                break;
            }
            if (webmsgok)
                pline("You flow through %s spider web.",
                      a_your[trap->madeby_u]);
            break;
        }
        if (webmaker(youmonst.data)) {
            if (webmsgok)
                pline(trap->madeby_u ? "You take a walk on your web." :
                      "There is a spider web here.");
            break;
        }
        if (webmsgok) {
            const char *verbbuf;

            if (u.usteed)
                verbbuf = msgcat(
                    "lead ",
                    x_monnam(u.usteed,
                             u.usteed->
                             mnamelth ? ARTICLE_NONE : ARTICLE_THE, "poor",
                             SUPPRESS_SADDLE, FALSE));
            else
                verbbuf = Levitation ? (const char *)"float" :
                    locomotion(youmonst.data, "stumble");
            pline("You %s into %s spider web!", verbbuf,
                  a_your[trap->madeby_u]);
        }
        u.utraptype = TT_WEB;

        /* Time stuck in the web depends on your/steed strength. */
        {
            int str = ACURR(A_STR);

            /* If mounted, the steed gets trapped.  Use mintrap to do all the
               work.  If mtrapped is set as a result, unset it and set utrap
               instead.  In the case of a strongmonst and mintrap said it's
               trapped, use a short but non-zero trap time.  Otherwise,
               monsters have no specific strength, so use player strength. This
               gets skipped for webmsgok, which implies that the steed isn't a
               factor. */
            if (u.usteed && webmsgok) {
                /* mtmp location might not be up to date */
                u.usteed->mx = u.ux;
                u.usteed->my = u.uy;

                /* mintrap currently does not return 2(died) for webs */
                if (mintrap(u.usteed)) {
                    u.usteed->mtrapped = 0;
                    if (strongmonst(u.usteed->data))
                        str = 17;
                } else {
                    break;
                }

                webmsgok = FALSE;       /* mintrap printed the messages */
            }

            if (str <= 3)
                u.utrap = rn1(6, 6);
            else if (str < 6)
                u.utrap = rn1(6, 4);
            else if (str < 9)
                u.utrap = rn1(4, 4);
            else if (str < 12)
                u.utrap = rn1(4, 2);
            else if (str < 15)
                u.utrap = rn1(2, 2);
            else if (str < 18)
                u.utrap = rnd(2);
            else if (str < 69)
                u.utrap = 1;
            else {
                u.utrap = 0;
                if (webmsgok)
                    pline("You tear through %s web!", a_your[trap->madeby_u]);
                deltrap(level, trap);
                newsym(u.ux, u.uy);     /* get rid of trap symbol */
            }
        }
        break;

    case STATUE_TRAP:
        activate_statue_trap(trap, u.ux, u.uy, FALSE);
        break;

    case MAGIC_TRAP:   /* A magic trap. */
        seetrap(trap);
        if (!rn2(30)) {
            deltrap(level, trap);
            newsym(u.ux, u.uy); /* update position */
            pline("You are caught in a magical explosion!");
            losehp(rnd(10), killer_msg(DIED, "a magical explosion"));
            pline("Your body absorbs some of the magical energy!");
            u.uen = (u.uenmax += 2);
        } else
            domagictrap();
        steedintrap(trap, NULL);
        break;

    case ANTI_MAGIC:
        seetrap(trap);
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("You feel momentarily lethargic.");
        } else
            drain_en(rnd(u.ulevel) + 1);
        break;

    case POLY_TRAP:{
            const char *verbbuf;

            seetrap(trap);
            if (u.usteed)
                verbbuf = msgcat(
                    "lead ",
                    x_monnam(u.usteed,
                             u.usteed->
                             mnamelth ? ARTICLE_NONE : ARTICLE_THE, NULL,
                             SUPPRESS_SADDLE, FALSE));
            else
                verbbuf =
                    Levitation ? (const char *)"float" :
                    locomotion(youmonst.data, "step");
            pline("You %s onto a polymorph trap!", verbbuf);
            if (Antimagic || Unchanging) {
                shieldeff(u.ux, u.uy);
                pline("You feel momentarily different.");
                /* Trap did nothing; don't remove it --KAA */
            } else {
                steedintrap(trap, NULL);
                deltrap(level, trap);   /* delete trap before polymorph */
                newsym(u.ux, u.uy);     /* get rid of trap symbol */
                pline("You feel a change coming over you.");
                polyself(FALSE);
            }
            break;
        }
    case LANDMINE:{
            unsigned steed_mid = 0;
            struct obj *saddle = 0;

            if (Levitation || Flying) {
                if (!already_seen && rn2(3))
                    break;
                seetrap(trap);
                pline("%s %s in a pile of soil below you.",
                      already_seen ? "There is" : "You discover",
                      trap->madeby_u ? "the trigger of your mine" :
                      "a trigger");
                if (already_seen && rn2(3))
                    break;
                pline("KAABLAMM!!!  %s %s%s off!",
                      forcebungle ? "Your inept attempt sets" :
                      "The air currents set",
                      already_seen ? a_your[trap->madeby_u] : "",
                      already_seen ? " land mine" : "it");
            } else {
                /* prevent landmine from killing steed, throwing you to the
                   ground, and you being affected again by the same mine
                   because it hasn't been deleted yet */
                static boolean recursive_mine = FALSE;

                if (recursive_mine)
                    break;

                seetrap(trap);
                pline("KAABLAMM!!!  You triggered %s land mine!",
                      a_your[trap->madeby_u]);
                if (u.usteed)
                    steed_mid = u.usteed->m_id;
                recursive_mine = TRUE;
                steedintrap(trap, NULL);
                recursive_mine = FALSE;
                saddle = sobj_at(SADDLE, level, u.ux, u.uy);
                set_wounded_legs(LEFT_SIDE, rn1(35, 41));
                set_wounded_legs(RIGHT_SIDE, rn1(35, 41));
                exercise(A_DEX, FALSE);
            }
            blow_up_landmine(trap);
            if (steed_mid && saddle && !u.usteed)
                keep_saddle_with_steedcorpse(steed_mid, level->objlist, saddle);
            newsym(u.ux, u.uy); /* update trap symbol */
            losehp(rnd(16), killer_msg(DIED, "a land mine"));
            /* fall recursively into the pit... */
            if ((trap = t_at(level, u.ux, u.uy)) != 0) {
                dotrap(trap, RECURSIVETRAP);
                fill_pit(level, u.ux, u.uy);
            } else
                spoteffects(FALSE);
            break;
        }
    case ROLLING_BOULDER_TRAP:{
            int style = ROLL | (trap->tseen ? LAUNCH_KNOWN : 0);

            seetrap(trap);
            pline("Click! You trigger a rolling boulder trap!");
            if (!launch_obj
                (BOULDER, trap->launch.x, trap->launch.y, trap->launch2.x,
                 trap->launch2.y, style)) {
                deltrap(level, trap);
                newsym(u.ux, u.uy);     /* get rid of trap symbol */
                pline("Fortunately for you, no boulder was released.");
            }
            break;
        }
    case MAGIC_PORTAL:
        seetrap(trap);
        domagicportal(trap);
        break;

    case VIBRATING_SQUARE:
        seetrap(trap);
        /* messages handled elsewhere; the trap symbol is merely to mark the
           square for future reference */
        break;

    default:
        seetrap(trap);
        impossible("You hit a trap of type %u", trap->ttyp);
    }
}


static int
steedintrap(struct trap *trap, struct obj *otmp)
{
    struct monst *mtmp = u.usteed;
    const struct permonst *mptr;
    int tt;
    boolean in_sight;
    boolean trapkilled = FALSE;
    boolean steedhit = FALSE;

    if (!u.usteed || !trap)
        return 0;
    mptr = mtmp->data;
    tt = trap->ttyp;
    mtmp->mx = u.ux;
    mtmp->my = u.uy;

    in_sight = !Blind;
    switch (tt) {
    case ARROW_TRAP:
        if (!otmp) {
            impossible("steed hit by non-existant arrow?");
            return 0;
        }
        if (thitm(8, mtmp, otmp, 0, FALSE))
            trapkilled = TRUE;
        steedhit = TRUE;
        break;
    case DART_TRAP:
        if (!otmp) {
            impossible("steed hit by non-existant dart?");
            return 0;
        }
        if (thitm(7, mtmp, otmp, 0, FALSE))
            trapkilled = TRUE;
        steedhit = TRUE;
        break;
    case SLP_GAS_TRAP:
        if (!resists_sleep(mtmp) && !breathless(mptr) && !mtmp->msleeping &&
            mtmp->mcanmove) {
            mtmp->mcanmove = 0;
            mtmp->mfrozen = rnd(25);
            if (in_sight) {
                pline("%s suddenly falls asleep!", Monnam(mtmp));
            }
        }
        steedhit = TRUE;
        break;
    case LANDMINE:
        if (thitm(0, mtmp, NULL, rnd(16), FALSE))
            trapkilled = TRUE;
        steedhit = TRUE;
        break;
    case PIT:
    case SPIKED_PIT:
        if (mtmp->mhp <= 0 ||
            thitm(0, mtmp, NULL, rnd((tt == PIT) ? 6 : 10), FALSE))
            trapkilled = TRUE;
        steedhit = TRUE;
        break;
    case POLY_TRAP:
        if (!resists_magm(mtmp)) {
            if (!resist(mtmp, WAND_CLASS, 0, NOTELL)) {
                newcham(mtmp, NULL, FALSE, FALSE);
                if (!can_saddle(mtmp) || !can_ride(mtmp)) {
                    dismount_steed(DISMOUNT_POLY);
                } else {
                    pline("You have to adjust yourself in the saddle on %s.",
                          x_monnam(mtmp,
                                   mtmp->mnamelth ? ARTICLE_NONE : ARTICLE_A,
                                   NULL, SUPPRESS_SADDLE, FALSE));
                }

            }
            steedhit = TRUE;
            break;
    default:
            return 0;
        }
    }
    if (trapkilled) {
        dismount_steed(DISMOUNT_POLY);
        return 2;
    } else if (steedhit)
        return 1;
    else
        return 0;
}


/* some actions common to both player and monsters for triggered landmine */
void
blow_up_landmine(struct trap *trap)
{
    int x = trap->tx;
    int y = trap->ty;

    scatter(x, y, 4, MAY_DESTROY | MAY_HIT | MAY_FRACTURE | VIS_EFFECTS, NULL);
    del_engr_at(level, trap->tx, trap->ty);
    wake_nearto(trap->tx, trap->ty, 400);
    if (IS_DOOR(level->locations[trap->tx][trap->ty].typ))
        level->locations[trap->tx][trap->ty].doormask = D_BROKEN;
    if (!IS_DRAWBRIDGE(level->locations[trap->tx][trap->ty].typ)) {
        trap->ttyp = PIT;       /* explosion creates a pit */
        trap->madeby_u = FALSE; /* resulting pit isn't yours */
        seetrap(trap);  /* and it isn't concealed */
    } else {
        deltrap(level, trap);
    }
    if (find_drawbridge(&x, &y)) {
        destroy_drawbridge(x, y);
    }
}


/*
 * Move obj from (x1,y1) to (x2,y2)
 *
 * Return 0 if no object was launched.
 *        1 if an object was launched and placed somewhere.
 *        2 if an object was launched, but used up.
 */
int
launch_obj(short otyp, int x1, int y1, int x2, int y2, int style)
{
    struct monst *mtmp;
    struct obj *otmp, *otmp2;
    struct tmp_sym *tsym;
    int dx, dy;
    struct obj *singleobj;
    boolean used_up = FALSE;
    boolean otherside = FALSE;
    int dist;
    int tmp;
    int delaycnt = 0;
    int damage = 0;

    otmp = sobj_at(otyp, level, x1, y1);
    /* Try the other side too, for rolling boulder traps */
    if (!otmp && otyp == BOULDER) {
        otherside = TRUE;
        otmp = sobj_at(otyp, level, x2, y2);
    }
    if (!otmp)
        return 0;
    if (otherside) {    /* swap 'em */
        int tx, ty;

        tx = x1;
        ty = y1;
        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }

    if (otmp->quan == 1L) {
        obj_extract_self(otmp);
        singleobj = otmp;
        otmp = NULL;
    } else {
        singleobj = splitobj(otmp, 1L);
        obj_extract_self(singleobj);
    }
    newsym(x1, y1);

    dist = distmin(x1, y1, x2, y2);
    bhitpos.x = x1;
    bhitpos.y = y1;
    dx = sgn(x2 - x1);
    dy = sgn(y2 - y1);
    switch (style) {
    case ROLL | LAUNCH_UNSEEN:
        if (otyp == BOULDER) {
            You_hear(Hallucination ? "someone bowling." :
                     "rumbling in the distance.");
        }
        style &= ~LAUNCH_UNSEEN;
        goto roll;
    case ROLL | LAUNCH_KNOWN:
        /* use otrapped as a flag to ohitmon */
        singleobj->otrapped = 1;
        style &= ~LAUNCH_KNOWN;
        /* fall through */
    roll:
    case ROLL:
        delaycnt = 2;
    }

    if (!delaycnt)
        delaycnt = 1;
    if (!cansee(bhitpos.x, bhitpos.y))
        flush_screen();

    tsym = tmpsym_initobj(singleobj);
    tmpsym_at(tsym, bhitpos.x, bhitpos.y);

    /* Set the object in motion */
    while (dist-- > 0 && !used_up) {
        struct trap *t;

        tmpsym_at(tsym, bhitpos.x, bhitpos.y);
        tmp = delaycnt;

        /* dstage@u.washington.edu -- Delay only if hero sees it */
        if (cansee(bhitpos.x, bhitpos.y))
            while (tmp-- > 0)
                win_delay_output();

        bhitpos.x += dx;
        bhitpos.y += dy;
        t = t_at(level, bhitpos.x, bhitpos.y);

        if ((mtmp = m_at(level, bhitpos.x, bhitpos.y)) != 0) {
            if (otyp == BOULDER && throws_rocks(mtmp->data)) {
                if (rn2(3)) {
                    pline("%s snatches the boulder.", Monnam(mtmp));
                    singleobj->otrapped = 0;
                    mpickobj(mtmp, singleobj);
                    used_up = TRUE;
                    break;
                }
            }
            if (ohitmon(mtmp, singleobj, (style == ROLL) ? -1 : dist, FALSE)) {
                used_up = TRUE;
                break;
            }
        } else if (bhitpos.x == u.ux && bhitpos.y == u.uy) {
            action_interrupted();

            if (thitu(9 + singleobj->spe, 0, singleobj, NULL))
                damage = dmgval(singleobj, &youmonst);
        }
        if (style == ROLL) {
            if (down_gate(bhitpos.x, bhitpos.y) != -1) {
                if (ship_object(singleobj, bhitpos.x, bhitpos.y, FALSE)) {
                    used_up = TRUE;
                    break;
                }
            }
            if (t && otyp == BOULDER) {
                switch (t->ttyp) {
                case LANDMINE:
                    if (rn2(10) > 2) {
                        pline("KAABLAMM!!!%s",
                              cansee(bhitpos.x,
                                     bhitpos.y) ?
                              " The rolling boulder triggers a land mine." :
                              "");
                        deltrap(level, t);
                        del_engr_at(level, bhitpos.x, bhitpos.y);
                        place_object(singleobj, level, bhitpos.x, bhitpos.y);
                        singleobj->otrapped = 0;
                        fracture_rock(singleobj);
                        scatter(bhitpos.x, bhitpos.y, 4,
                                MAY_DESTROY | MAY_HIT | MAY_FRACTURE |
                                VIS_EFFECTS, NULL);
                        if (cansee(bhitpos.x, bhitpos.y))
                            newsym(bhitpos.x, bhitpos.y);
                        used_up = TRUE;
                    }
                    break;
                case LEVEL_TELEP:
                case TELEP_TRAP:
                    if (cansee(bhitpos.x, bhitpos.y))
                        pline("Suddenly the rolling boulder disappears!");
                    else
                        You_hear("a rumbling stop abruptly.");
                    singleobj->otrapped = 0;
                    if (t->ttyp == TELEP_TRAP)
                        rloco(singleobj);
                    else {
                        int newlev = random_teleport_level();
                        d_level dest;

                        if (newlev == depth(&u.uz) || In_endgame(&u.uz))
                            continue;
                        get_level(&dest, newlev);
                        deliver_object(singleobj, dest.dnum, dest.dlevel,
                                       MIGR_RANDOM);
                    }
                    seetrap(t);
                    used_up = TRUE;
                    break;
                case PIT:
                case SPIKED_PIT:
                case HOLE:
                case TRAPDOOR:
                    /* the boulder won't be used up if there is a monster in
                       the trap; stop rolling anyway */
                    x2 = bhitpos.x, y2 = bhitpos.y;     /* stops here */
                    if (flooreffects(singleobj, x2, y2, "fall"))
                        used_up = TRUE;
                    dist = -1;  /* stop rolling immediately */
                    break;
                }
                if (used_up || dist == -1)
                    break;
            }
            if (flooreffects(singleobj, bhitpos.x, bhitpos.y, "fall")) {
                used_up = TRUE;
                break;
            }
            if (otyp == BOULDER &&
                (otmp2 = sobj_at(BOULDER, level, bhitpos.x, bhitpos.y)) != 0) {
                const char *bmsg = " as one boulder sets another in motion";

                if (!isok(bhitpos.x + dx, bhitpos.y + dy) || !dist ||
                    IS_ROCK(level->
                            locations[bhitpos.x + dx][bhitpos.y + dy].typ))
                    bmsg = " as one boulder hits another";

                You_hear("a loud crash%s!",
                         cansee(bhitpos.x, bhitpos.y) ? bmsg : "");
                obj_extract_self(otmp2);
                /* pass off the otrapped flag to the next boulder */
                otmp2->otrapped = singleobj->otrapped;
                singleobj->otrapped = 0;
                place_object(singleobj, level, bhitpos.x, bhitpos.y);
                singleobj = otmp2;
                otmp2 = NULL;
                wake_nearto(bhitpos.x, bhitpos.y, 10 * 10);
            }
        }
        if (otyp == BOULDER && closed_door(level, bhitpos.x, bhitpos.y)) {
            if (cansee(bhitpos.x, bhitpos.y))
                pline("The boulder crashes through a door.");
            level->locations[bhitpos.x][bhitpos.y].doormask = D_BROKEN;
            if (dist)
                unblock_point(bhitpos.x, bhitpos.y);
        }

        /* if about to hit iron bars, do so now */
        if (dist > 0 && isok(bhitpos.x + dx, bhitpos.y + dy) &&
            level->locations[bhitpos.x + dx][bhitpos.y + dy].typ == IRONBARS) {
            x2 = bhitpos.x, y2 = bhitpos.y;     /* object stops here */
            if (hits_bars(&singleobj, x2, y2, !rn2(20), 0)) {
                if (!singleobj)
                    used_up = TRUE;
                break;
            }
        }
    }
    tmpsym_end(tsym);
    if (!used_up) {
        singleobj->otrapped = 0;
        place_object(singleobj, level, x2, y2);
        newsym(x2, y2);
    }

    if (damage)
        losehp(damage, killer_msg_obj(DIED, singleobj));

    if (!used_up) {
        return 1;
    } else
        return 2;
}


void
seetrap(struct trap *trap)
{
    if (!trap->tseen) {
        trap->tseen = 1;
        map_trap(trap, 1, TRUE);
        newsym(trap->tx, trap->ty);
    }
}


static int
mkroll_launch(struct trap *ttmp, struct level *lev, xchar x, xchar y,
              short otyp, long ocount, enum rng rng)
{
    struct obj *otmp;
    int tmp;
    schar dx, dy;
    int distance;
    coord cc;
    coord bcc;
    int trycount = 0;
    boolean success = FALSE;
    int mindist = 4;

    if (ttmp->ttyp == ROLLING_BOULDER_TRAP)
        mindist = 2;
    distance = 4 + rn2_on_rng(5, rng);       /* 4..8 away */
    tmp = rn2_on_rng(8, rng);     /* randomly pick a direction to try first */
    while (distance >= mindist) {
        dx = xdir[tmp];
        dy = ydir[tmp];
        cc.x = x;
        cc.y = y;
        /* Prevent boulder from being placed on water */
        if (ttmp->ttyp == ROLLING_BOULDER_TRAP &&
            (is_pool(lev, x + distance * dx, y + distance * dy) ||
             is_lava(lev, x + distance * dx, y + distance * dy)))
            success = FALSE;
        else
            success = isclearpath(lev, &cc, distance, dx, dy);
        if (ttmp->ttyp == ROLLING_BOULDER_TRAP) {
            boolean success_otherway;

            bcc.x = x;
            bcc.y = y;
            success_otherway = isclearpath(lev, &bcc, distance, -(dx), -(dy));
            if (!success_otherway)
                success = FALSE;
        }
        if (success)
            break;
        if (++tmp > 7)
            tmp = 0;
        if ((++trycount % 8) == 0)
            --distance;
    }
    if (!success) {
        /* create the trap without any ammo, launch pt at trap location */
        cc.x = bcc.x = x;
        cc.y = bcc.y = y;
    } else {
        otmp = mksobj(lev, otyp, TRUE, FALSE, rng_main);
        otmp->quan = ocount;
        otmp->owt = weight(otmp);
        place_object(otmp, lev, cc.x, cc.y);
        stackobj(otmp);
    }
    ttmp->launch.x = cc.x;
    ttmp->launch.y = cc.y;
    if (ttmp->ttyp == ROLLING_BOULDER_TRAP) {
        ttmp->launch2.x = bcc.x;
        ttmp->launch2.y = bcc.y;
    } else
        ttmp->launch_otyp = otyp;
    if (lev == level)
        newsym(ttmp->launch.x, ttmp->launch.y);
    return 1;
}

static boolean
isclearpath(struct level *lev, coord * cc, int distance, schar dx, schar dy)
{
    uchar typ;
    xchar x, y;

    x = cc->x;
    y = cc->y;
    while (distance-- > 0) {
        x += dx;
        y += dy;
        typ = lev->locations[x][y].typ;
        if (!isok(x, y) || !ZAP_POS(typ) || closed_door(lev, x, y))
            return FALSE;
    }
    cc->x = x;
    cc->y = y;
    return TRUE;
}


int
mintrap(struct monst *mtmp)
{
    struct level *lev = mtmp->dlevel;
    struct trap *trap = t_at(lev, mtmp->mx, mtmp->my);
    boolean trapkilled = FALSE;
    const struct permonst *mptr = mtmp->data;
    struct obj *otmp;

    if (!trap) {
        mtmp->mtrapped = 0;     /* perhaps teleported? */
    } else if (mtmp->mtrapped) {        /* is currently in the trap */
        if (!trap->tseen && cansee(mtmp->mx, mtmp->my) && canseemon(mtmp) &&
            (trap->ttyp == SPIKED_PIT || trap->ttyp == BEAR_TRAP ||
             trap->ttyp == HOLE || trap->ttyp == PIT || trap->ttyp == WEB)) {
            /* If you come upon an obviously trapped monster, then you must be
               able to see the trap it's in too. */
            seetrap(trap);
        }

        if (!rn2(40)) {
            if (sobj_at(BOULDER, lev, mtmp->mx, mtmp->my) &&
                (trap->ttyp == PIT || trap->ttyp == SPIKED_PIT)) {
                if (!rn2(2)) {
                    mtmp->mtrapped = 0;
                    if (canseemon(mtmp))
                        pline("%s pulls free...", Monnam(mtmp));
                    fill_pit(lev, mtmp->mx, mtmp->my);
                }
            } else {
                mtmp->mtrapped = 0;
            }
        } else if (metallivorous(mptr)) {
            if (trap->ttyp == BEAR_TRAP) {
                if (canseemon(mtmp))
                    pline("%s eats a bear trap!", Monnam(mtmp));
                deltrap(lev, trap);
                mtmp->meating = 5;
                mtmp->mtrapped = 0;
            } else if (trap->ttyp == SPIKED_PIT) {
                if (canseemon(mtmp))
                    pline("%s munches on some spikes!", Monnam(mtmp));
                trap->ttyp = PIT;
                mtmp->meating = 5;
            }
        }
    } else {
        int tt = trap->ttyp;
        boolean in_sight, tear_web, see_it, inescapable =
            ((tt == HOLE || tt == PIT) && In_sokoban(&u.uz) && !trap->madeby_u);
        const char *fallverb;

        /* true when called from dotrap, inescapable is not an option */
        if (mtmp == u.usteed)
            inescapable = TRUE;
        if (!inescapable &&
            ((mtmp->mtrapseen & (1 << (tt - 1))) != 0 ||
             (tt == HOLE && !mindless(mtmp->data)))) {
            /* it has been in such a trap - perhaps it escapes */
            if (rn2(4))
                return 0;
        } else {
            mtmp->mtrapseen |= (1 << (tt - 1));
        }
        /* Monster is aggravated by being trapped by you. Recognizing who made
           the trap isn't completely unreasonable; everybody has their own
           style. */
        if (trap->madeby_u && rnl(5))
            setmangry(mtmp);

        in_sight = canseemon(mtmp);
        see_it = cansee(mtmp->mx, mtmp->my);

        /* assume hero can tell what's going on for the steed */
        if (mtmp == u.usteed)
            in_sight = TRUE;
        switch (tt) {
        case ARROW_TRAP:
            if (trap->once && trap->tseen && !rn2(15)) {
                if (in_sight && see_it)
                    pline("%s triggers a trap but nothing happens.",
                          Monnam(mtmp));
                deltrap(lev, trap);
                if (lev == level)
                    newsym(mtmp->mx, mtmp->my);
                break;
            }
            trap->once = 1;
            otmp = mksobj(lev, ARROW, TRUE, FALSE, rng_main);
            otmp->quan = 1L;
            otmp->owt = weight(otmp);
            otmp->opoisoned = 0;
            if (in_sight)
                seetrap(trap);
            if (thitm(8, mtmp, otmp, 0, FALSE))
                trapkilled = TRUE;
            break;
        case DART_TRAP:
            if (trap->once && trap->tseen && !rn2(15)) {
                if (in_sight && see_it)
                    pline("%s triggers a trap but nothing happens.",
                          Monnam(mtmp));
                deltrap(lev, trap);
                if (lev == level)
                    newsym(mtmp->mx, mtmp->my);
                break;
            }
            trap->once = 1;
            otmp = mksobj(lev, DART, TRUE, FALSE, rng_main);
            otmp->quan = 1L;
            otmp->owt = weight(otmp);
            if (!rn2(6))
                otmp->opoisoned = 1;
            if (in_sight)
                seetrap(trap);
            if (thitm(7, mtmp, otmp, 0, FALSE))
                trapkilled = TRUE;
            break;
        case ROCKTRAP:
            if (trap->once && trap->tseen && !rn2(15)) {
                if (in_sight && see_it)
                    pline("A trap door above %s opens, but nothing falls out!",
                          mon_nam(mtmp));
                deltrap(lev, trap);
                if (lev == level)
                    newsym(mtmp->mx, mtmp->my);
                break;
            }
            trap->once = 1;
            otmp = mksobj(lev, ROCK, TRUE, FALSE, rng_main);
            otmp->quan = 1L;
            otmp->owt = weight(otmp);
            if (in_sight)
                seetrap(trap);
            if (thitm(0, mtmp, otmp, dice(2, 6), FALSE))
                trapkilled = TRUE;
            break;

        case SQKY_BOARD:
            if (is_flyer(mptr))
                break;
            /* stepped on a squeaky board */
            if (in_sight) {
                pline("A board beneath %s squeaks loudly.", mon_nam(mtmp));
                seetrap(trap);
            } else
                You_hear("a distant squeak.");
            /* wake up nearby monsters */
            wake_nearto(mtmp->mx, mtmp->my, 40);
            break;

        case BEAR_TRAP:
            if (mptr->msize > MZ_SMALL && !amorphous(mptr) && !is_flyer(mptr) &&
                !is_whirly(mptr) && !unsolid(mptr)) {
                mtmp->mtrapped = 1;
                if (in_sight) {
                    pline("%s is caught in %s bear trap!", Monnam(mtmp),
                          a_your[trap->madeby_u]);
                    seetrap(trap);
                } else {
                    if ((mptr == &mons[PM_OWLBEAR]
                         || mptr == &mons[PM_BUGBEAR]))
                        You_hear("the roaring of an angry bear!");
                }
            }
            break;

        case SLP_GAS_TRAP:
            if (!resists_sleep(mtmp) && !breathless(mptr) && !mtmp->msleeping &&
                mtmp->mcanmove) {
                mtmp->mcanmove = 0;
                mtmp->mfrozen = rnd(25);
                if (in_sight) {
                    pline("%s suddenly falls asleep!", Monnam(mtmp));
                    seetrap(trap);
                }
            }
            break;

        case RUST_TRAP:
            {
                struct obj *target;

                if (in_sight)
                    seetrap(trap);
                switch (rn2(5)) {
                case 0:
                    if (in_sight)
                        pline("%s %s on the %s!", A_gush_of_water_hits,
                              mon_nam(mtmp), mbodypart(mtmp, HEAD));
                    target = which_armor(mtmp, os_armh);
                    water_damage(target, maybe_helmet_name(target), TRUE);
                    break;
                case 1:
                    if (in_sight)
                        pline("%s %s's left %s!", A_gush_of_water_hits,
                              mon_nam(mtmp), mbodypart(mtmp, ARM));
                    target = which_armor(mtmp, os_arms);
                    if (water_damage(target, "shield", TRUE))
                        break;
                    target = MON_WEP(mtmp);
                    if (target && bimanual(target))
                        water_damage(target, NULL, TRUE);
                glovecheck:
                    target =
                        which_armor(mtmp, os_armg);
                    water_damage(target, "gauntlets", TRUE);
                    break;
                case 2:
                    if (in_sight)
                        pline("%s %s's right %s!", A_gush_of_water_hits,
                              mon_nam(mtmp), mbodypart(mtmp, ARM));
                    water_damage(MON_WEP(mtmp), NULL, TRUE);
                    goto glovecheck;
                default:
                    if (in_sight)
                        pline("%s %s!", A_gush_of_water_hits, mon_nam(mtmp));
                    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
                        snuff_lit(otmp);
                    target = which_armor(mtmp, os_armc);
                    if (target)
                        water_damage(target, cloak_simple_name(target), TRUE);
                    else {
                        target = which_armor(mtmp, os_arm);
                        if (target)
                            water_damage(target, "armor", TRUE);
                        else {
                            target = which_armor(mtmp, os_armu);
                            water_damage(target, "shirt", TRUE);
                        }
                    }
                }
                if (mptr == &mons[PM_IRON_GOLEM]) {
                    if (in_sight)
                        pline("%s falls to pieces!", Monnam(mtmp));
                    else if (mtmp->mtame)
                        pline("May %s rust in peace.", mon_nam(mtmp));
                    mondied(mtmp);
                    if (mtmp->mhp <= 0)
                        trapkilled = TRUE;
                } else if (mptr == &mons[PM_GREMLIN] && rn2(3)) {
                    split_mon(mtmp, NULL);
                }
                break;
            }
        case FIRE_TRAP:
        mfiretrap:
            if (in_sight)
                pline("A %s erupts from the %s under %s!", tower_of_flame,
                      surface(mtmp->mx, mtmp->my), mon_nam(mtmp));
            else if (see_it)    /* evidently `mtmp' is invisible */
                pline("You see a %s erupt from the %s!", tower_of_flame,
                      surface(mtmp->mx, mtmp->my));

            if (resists_fire(mtmp)) {
                if (in_sight) {
                    shieldeff(mtmp->mx, mtmp->my);
                    pline("%s is uninjured.", Monnam(mtmp));
                }
            } else {
                int num = dice(2, 4), alt;
                boolean immolate = FALSE;

                /* paper burns very fast, assume straw is tightly packed and
                   burns a bit slower */
                switch (monsndx(mtmp->data)) {
                case PM_PAPER_GOLEM:
                    immolate = TRUE;
                    alt = mtmp->mhpmax;
                    break;
                case PM_STRAW_GOLEM:
                    alt = mtmp->mhpmax / 2;
                    break;
                case PM_WOOD_GOLEM:
                    alt = mtmp->mhpmax / 4;
                    break;
                case PM_LEATHER_GOLEM:
                    alt = mtmp->mhpmax / 8;
                    break;
                default:
                    alt = 0;
                    break;
                }
                if (alt > num)
                    num = alt;

                if (thitm(0, mtmp, NULL, num, immolate))
                    trapkilled = TRUE;
                else
                    /* we know mhp is at least `num' below mhpmax, so no (mhp >
                       mhpmax) check is needed here */
                    mtmp->mhpmax -= rn2(num + 1);
            }
            if (burnarmor(mtmp) || rn2(3)) {
                destroy_mitem(mtmp, SCROLL_CLASS, AD_FIRE);
                destroy_mitem(mtmp, SPBOOK_CLASS, AD_FIRE);
                destroy_mitem(mtmp, POTION_CLASS, AD_FIRE);
            }
            if (burn_floor_paper(lev, mtmp->mx, mtmp->my, see_it, FALSE) &&
                !see_it && distu(mtmp->mx, mtmp->my) <= 3 * 3)
                pline("You smell smoke.");
            if (is_ice(mtmp->dlevel, mtmp->mx, mtmp->my))
                melt_ice(mtmp->dlevel, mtmp->mx, mtmp->my);
            if (see_it)
                seetrap(trap);
            break;

        case PIT:
        case SPIKED_PIT:
            fallverb = "falls";
            if (is_flyer(mptr) || is_floater(mptr) ||
                (mtmp->wormno && count_wsegs(mtmp) > 5) || is_clinger(mptr)) {
                if (!inescapable)
                    break;      /* avoids trap */
                fallverb = "is dragged";        /* sokoban pit */
            }
            if (!passes_walls(mptr))
                mtmp->mtrapped = 1;
            if (in_sight) {
                pline("%s %s into %s pit!", Monnam(mtmp), fallverb,
                      a_your[trap->madeby_u]);
                if (mptr == &mons[PM_PIT_VIPER] || mptr == &mons[PM_PIT_FIEND])
                    pline("How pitiful.  Isn't that the pits?");
                seetrap(trap);
            }
            mselftouch(mtmp, "Falling, ", FALSE);
            if (mtmp->mhp <= 0 ||
                thitm(0, mtmp, NULL, rnd((tt == PIT) ? 6 : 10), FALSE))
                trapkilled = TRUE;
            break;
        case HOLE:
        case TRAPDOOR:
            if (!can_fall_thru(lev)) {
                impossible("mintrap: %ss cannot exist on this level.",
                           trapexplain[tt - 1]);
                break;  /* don't activate it after all */
            }
            if (is_flyer(mptr) || is_floater(mptr) || mptr == &mons[PM_WUMPUS]
                || (mtmp->wormno && count_wsegs(mtmp) > 5) ||
                mptr->msize >= MZ_HUGE) {
                if (inescapable) {      /* sokoban hole */
                    if (in_sight) {
                        pline("%s seems to be yanked down!", Monnam(mtmp));
                        /* suppress message in mlevel_tele_trap() */
                        in_sight = FALSE;
                        seetrap(trap);
                    }
                } else
                    break;
            }
            /* Fall through */
        case LEVEL_TELEP:
        case MAGIC_PORTAL:
            {
                int mlev_res;

                mlev_res = mlevel_tele_trap(mtmp, trap, inescapable, in_sight);
                if (mlev_res)
                    return mlev_res;
            }
            break;

        case TELEP_TRAP:
            mtele_trap(mtmp, trap, in_sight);
            break;

        case WEB:
            /* Monster in a web. */
            if (webmaker(mptr))
                break;
            if (amorphous(mptr) || is_whirly(mptr) || unsolid(mptr)) {
                if (acidic(mptr) || mptr == &mons[PM_GELATINOUS_CUBE] ||
                    mptr == &mons[PM_FIRE_ELEMENTAL]) {
                    if (in_sight)
                        pline("%s %s %s spider web!", Monnam(mtmp),
                              (mptr ==
                               &mons[PM_FIRE_ELEMENTAL]) ? "burns" :
                              "dissolves", a_your[trap->madeby_u]);
                    deltrap(lev, trap);
                    if (lev == level)
                        newsym(mtmp->mx, mtmp->my);
                    break;
                }
                if (in_sight) {
                    pline("%s flows through %s spider web.", Monnam(mtmp),
                          a_your[trap->madeby_u]);
                    seetrap(trap);
                }
                break;
            }
            tear_web = FALSE;
            switch (monsndx(mptr)) {
            case PM_OWLBEAR:   /* Eric Backus */
            case PM_BUGBEAR:
                if (!in_sight) {
                    You_hear("the roaring of a confused bear!");
                    mtmp->mtrapped = 1;
                    break;
                }
                /* fall though */
            default:
                if (mptr->mlet == S_GIANT ||
                    (mptr->mlet == S_DRAGON &&
                     extra_nasty(mptr)) || /* excl. babies */
                    (mtmp->wormno && count_wsegs(mtmp) > 5)) {
                    tear_web = TRUE;
                } else if (in_sight) {
                    pline("%s is caught in %s spider web.", Monnam(mtmp),
                          a_your[trap->madeby_u]);
                    seetrap(trap);
                }
                mtmp->mtrapped = tear_web ? 0 : 1;
                break;
                /* this list is fairly arbitrary; it deliberately excludes
                   wumpus & giant/ettin zombies/mummies */
            case PM_TITANOTHERE:
            case PM_BALUCHITHERIUM:
            case PM_PURPLE_WORM:
            case PM_JABBERWOCK:
            case PM_IRON_GOLEM:
            case PM_BALROG:
            case PM_KRAKEN:
            case PM_MASTODON:
                tear_web = TRUE;
                break;
            }
            if (tear_web) {
                if (in_sight)
                    pline("%s tears through %s spider web!", Monnam(mtmp),
                          a_your[trap->madeby_u]);
                deltrap(lev, trap);
                if (lev == level)
                    newsym(mtmp->mx, mtmp->my);
            }
            break;

        case VIBRATING_SQUARE:
            if (see_it && !Blind) {
                if (in_sight)
                    pline("You see a %s vibration beneath %s %s.",
                          (Hallucination ? "normal" : "strange"),
                          s_suffix(mon_nam(mtmp)),
                          makeplural(mbodypart(mtmp, FOOT)));
                else
                    pline("You see the ground vibrate in the distance.");
                seetrap(trap);
            }
            break;

        case STATUE_TRAP:
            break;

        case MAGIC_TRAP:
            /* A magic trap.  Monsters usually immune. */
            if (!rn2(21))
                goto mfiretrap;
            break;
        case ANTI_MAGIC:
            break;

        case LANDMINE:
            {
                if (rn2(3))
                    break;      /* monsters usually don't set it off */
                if (is_flyer(mptr)) {
                    boolean already_seen = trap->tseen;

                    if (in_sight && !already_seen) {
                        pline("A trigger appears in a pile of soil below %s.",
                              mon_nam(mtmp));
                        seetrap(trap);
                    }
                    if (rn2(3))
                        break;
                    if (in_sight) {
                        if (lev == level)
                            newsym(mtmp->mx, mtmp->my);
                        pline("The air currents set %s off!",
                              already_seen ? "a land mine" : "it");
                    }
                } else if (in_sight) {
                    if (lev == level)
                        newsym(mtmp->mx, mtmp->my);
                    pline("KAABLAMM!!!  %s triggers %s land mine!",
                          Monnam(mtmp), a_your[trap->madeby_u]);
                }
                if (!in_sight)
                    pline("Kaablamm!  You hear an explosion in the distance!");
                blow_up_landmine(trap);
                if (DEADMONSTER(mtmp))
                    trapkilled = TRUE;
                else if (thitm(0, mtmp, NULL, rnd(16), FALSE))
                    trapkilled = TRUE;
                else {
                    /* monsters recursively fall into new pit */
                    if (mintrap(mtmp) == 2)
                        trapkilled = TRUE;
                }
                /* a boulder may fill the new pit, crushing monster */
                if (t_at(lev, mtmp->mx, mtmp->my))
                    fill_pit(lev, mtmp->mx, mtmp->my);
                else if (!DEADMONSTER(mtmp))
                    minliquid(mtmp);
                if (mtmp->mhp <= 0)
                    trapkilled = TRUE;
                cancel_helplessness(hm_unconscious,
                                    "The explosion awakens you!");
                break;
            }
        case POLY_TRAP:
            if (resists_magm(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
            } else if (!resist(mtmp, WAND_CLASS, 0, NOTELL)) {
                newcham(mtmp, NULL, FALSE, FALSE);
                if (in_sight)
                    seetrap(trap);
            }
            break;

        case ROLLING_BOULDER_TRAP:
            if (!is_flyer(mptr)) {
                int style = ROLL | (in_sight ? 0 : LAUNCH_UNSEEN);

                if (lev == level)
                    newsym(mtmp->mx, mtmp->my);
                if (in_sight)
                    pline("Click! %s triggers %s.", Monnam(mtmp),
                          trap->tseen ? "a rolling boulder trap" : "something");
                if (launch_obj
                    (BOULDER, trap->launch.x, trap->launch.y, trap->launch2.x,
                     trap->launch2.y, style)) {
                    if (in_sight)
                        trap->tseen = TRUE;
                    if (mtmp->mhp <= 0)
                        trapkilled = TRUE;
                } else {
                    deltrap(lev, trap);
                    if (lev == level)
                        newsym(mtmp->mx, mtmp->my);
                }
            }
            break;

        default:
            impossible("Some monster encountered a strange trap of type %d.",
                       tt);
        }
    }
    if (trapkilled)
        return 2;
    return mtmp->mtrapped;
}


/* Combine cockatrice checks into single functions to avoid repeating code. */
void
instapetrify(const char *str)
{
    if (Stone_resistance)
        return;
    if (poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM, TRUE))
        return;
    pline("You turn to stone...");
    done(STONING, str);
}

void
minstapetrify(struct monst *mon, boolean byplayer)
{
    if (resists_ston(mon))
        return;
    if (poly_when_stoned(mon->data)) {
        mon_to_stone(mon);
        return;
    }

    /* give a "<mon> is slowing down" message and also remove intrinsic speed
       (comparable to similar effect on the hero) */
    mon_adjust_speed(mon, -3, NULL);

    if (cansee(mon->mx, mon->my))
        pline("%s turns to stone.", Monnam(mon));
    if (byplayer) {
        stoned = TRUE;
        xkilled(mon, 0);
    } else
        monstone(mon);
}

void
selftouch(const char *arg, const char *deathtype)
{
    if (uwep && uwep->otyp == CORPSE && touch_petrifies(&mons[uwep->corpsenm])
        && !Stone_resistance) {
        pline("%s touch the %s corpse.", arg, mons[uwep->corpsenm].mname);
        instapetrify(killer_msg(STONING,
            msgprintf("%s %s corpse", deathtype,
                      an(mons[uwep->corpsenm].mname))));
        if (!Stone_resistance)
            uwepgone();
    }
    /* Or your secondary weapon, if wielded */
    if (u.twoweap && uswapwep && uswapwep->otyp == CORPSE &&
        touch_petrifies(&mons[uswapwep->corpsenm]) && !Stone_resistance) {
        pline("%s touch the %s corpse.", arg, mons[uswapwep->corpsenm].mname);
        instapetrify(killer_msg(STONING,
            msgprintf("%s %s corpse", deathtype,
                      an(mons[uswapwep->corpsenm].mname))));
        if (!Stone_resistance)
            uswapwepgone();
    }
}

void
mselftouch(struct monst *mon, const char *arg, boolean byplayer)
{
    struct obj *mwep = MON_WEP(mon);

    if (mwep && mwep->otyp == CORPSE &&
        touch_petrifies(&mons[mwep->corpsenm])) {
        if (cansee(mon->mx, mon->my)) {
            pline("%s%s touches the %s corpse.", arg ? arg : "",
                  arg ? mon_nam(mon) : Monnam(mon), mons[mwep->corpsenm].mname);
        }
        minstapetrify(mon, byplayer);
    }
}

void
float_up(void)
{
    if (u.utrap) {
        if (u.utraptype == TT_PIT) {
            u.utrap = 0;
            pline("You float up, out of the pit!");
            turnstate.vision_full_recalc = TRUE;     /* vision limits change */
            fill_pit(level, u.ux, u.uy);
        } else if (u.utraptype == TT_INFLOOR) {
            pline("Your body pulls upward, but your %s are still stuck.",
                  makeplural(body_part(LEG)));
        } else {
            pline("You float up, only your %s is still stuck.", body_part(LEG));
        }
    } else if (Is_waterlevel(&u.uz))
        pline("It feels as though you've lost some weight.");
    else if (u.uinwater)
        spoteffects(TRUE);
    else if (Engulfed)
        pline(is_animal(u.ustuck->data) ? "You float away from the %s." :
              "You spiral up into %s.",
              is_animal(u.ustuck->data) ? surface(u.ux, u.uy) :
              mon_nam(u.ustuck));
    else if (Hallucination)
        pline("Up, up, and awaaaay!  You're walking on air!");
    else if (Is_airlevel(&u.uz))
        pline("You gain control over your movements.");
    else
        pline("You start to float in the air!");
    if (u.usteed && !is_floater(u.usteed->data) && !is_flyer(u.usteed->data)) {
        if (Lev_at_will)
            pline("%s magically floats up!", Monnam(u.usteed));
        else {
            pline("You cannot stay on %s.", mon_nam(u.usteed));
            dismount_steed(DISMOUNT_GENERIC);
        }
    }
    return;
}

void
fill_pit(struct level *lev, int x, int y)
{
    struct obj *otmp;
    struct trap *t;

    if ((t = t_at(lev, x, y)) && ((t->ttyp == PIT) || (t->ttyp == SPIKED_PIT))
        && (otmp = sobj_at(BOULDER, lev, x, y))) {
        obj_extract_self(otmp);
        flooreffects(otmp, x, y, "settle");
    }
}

int
float_down(long hmask)
{       /* might cancel timeout */
    struct trap *trap = NULL;
    d_level current_dungeon_level;
    boolean no_msg = FALSE;

    HLevitation &= ~hmask;

    if (Levitation)
        return 0;       /* maybe another ring/potion/boots */
    if (Engulfed) {
        pline((Flying) ? "You feel less buoyant, but you are still %s." :
              "You float down, but you are still %s.",
              is_animal(u.ustuck->data) ? "swallowed" : "engulfed");
        return 1;
    }

    /* Don't print "you float gently..." if it's actually violent. */
    if (IS_SINK(level->locations[u.ux][u.uy].typ))
        no_msg = TRUE;

    if (Punished && !carried(uball) &&
        (is_pool(level, uball->ox, uball->oy) ||
         ((trap = t_at(level, uball->ox, uball->oy)) &&
          ((trap->ttyp == PIT) || (trap->ttyp == SPIKED_PIT) ||
           (trap->ttyp == TRAPDOOR) || (trap->ttyp == HOLE))))) {
        u.ux0 = u.ux;
        u.uy0 = u.uy;
        u.ux = uball->ox;
        u.uy = uball->oy;
        movobj(uchain, uball->ox, uball->oy);
        newsym(u.ux0, u.uy0);
        turnstate.vision_full_recalc = TRUE; /* in case the hero moved. */
    }
    /* check for falling into pool - added by GAN 10/20/86 */
    if (!Flying) {
        if (!Engulfed && u.ustuck) {
            if (sticks(youmonst.data))
                pline("You aren't able to maintain your hold on %s.",
                      mon_nam(u.ustuck));
            else
                pline("Startled, %s can no longer hold you!",
                      mon_nam(u.ustuck));
            u.ustuck = 0;
        }
        /* kludge alert: drown() and lava_effects() print various messages
           almost every time they're called which conflict with the "fall into"
           message below.  Thus, we want to avoid printing confusing, duplicate
           or out-of-order messages. Use knowledge of the two routines as a
           hack -- this should really be handled differently -dlc */
        if (is_pool(level, u.ux, u.uy) && !Wwalking && !Swimming && !u.uinwater)
            no_msg = drown();

        if (is_lava(level, u.ux, u.uy)) {
            lava_effects();
            no_msg = TRUE;
        }
    }
    if (!trap) {
        trap = t_at(level, u.ux, u.uy);
        if (Is_airlevel(&u.uz)) {
            if (Flying)
                pline("You feel less buoyant.");
            else
                pline("You begin to tumble in place.");
        } else if (Is_waterlevel(&u.uz) && !no_msg)
            pline("You feel heavier.");
        /* u.uinwater msgs already in spoteffects()/drown() */
        else if (!u.uinwater && !no_msg) {
            if (!in_steed_dismounting) {
                boolean sokoban_trap = (In_sokoban(&u.uz) && trap);

                if (Hallucination)
                    pline("Bummer!  You've %s.",
                          is_pool(level, u.ux,
                                  u.uy) ? "splashed down" : sokoban_trap ?
                          "crashed" : "hit the ground");
                else {
                    if (!sokoban_trap) {
                        if (Flying)
                            pline("You feel less buoyant.");
                        else
                            pline("You float gently to the %s.",
                                  surface(u.ux, u.uy));
                    } else {
                        /* Justification elsewhere for Sokoban traps is based
                           on air currents. This is consistent with that. The
                           unexpected additional force of the air currents once
                           leviation ceases knocks you off your feet. */
                        pline("You fall over.");
                        losehp(rnd(2), killer_msg(DIED, "dangerous winds"));
                        if (u.usteed)
                            dismount_steed(DISMOUNT_FELL);
                        selftouch("As you fall, you", "being blown into");
                    }
                }
            }
        }
        action_interrupted();
    }

    assign_level(&current_dungeon_level, &u.uz);

    if (trap)
        switch (trap->ttyp) {
        case STATUE_TRAP:
            break;
        case HOLE:
        case TRAPDOOR:
            if (!can_fall_thru(level) || u.ustuck)
                break;
            /* fall into next case */
        default:
            if (!u.utrap)       /* not already in the trap */
                dotrap(trap, 0);
        }

    if (!Is_airlevel(&u.uz) && !Is_waterlevel(&u.uz) && !Engulfed &&
        /* falling through trap door calls goto_level, and goto_level does its
           own pickup() call */
        on_level(&u.uz, &current_dungeon_level))
        pickup(1, flags.interaction_mode);
    return 1;
}


/* box: null for floor trap */
static void
dofiretrap(struct obj *box)
{
    struct level *lev = box ? box->olev : level;
    boolean see_it = !Blind;
    int num, alt;

/* Bug: for box case, the equivalent of burn_floor_paper() ought
 * to be done upon its contents.
 */

    if ((box && !carried(box)) ? is_pool(level, box->ox, box->oy)
                               : Underwater) {
        pline("A cascade of steamy bubbles erupts from %s!",
              the(box ? xname(box) : surface(u.ux, u.uy)));
        if (Fire_resistance)
            pline("You are uninjured.");
        else
            losehp(rnd(3), killer_msg(DIED, "boiling water"));
        return;
    }
    pline("A %s %s from %s!", tower_of_flame, box ? "bursts" : "erupts",
          the(box ? xname(box) : surface(u.ux, u.uy)));
    if (Fire_resistance) {
        shieldeff(u.ux, u.uy);
        num = rn2(2);
    } else if (Upolyd) {
        num = dice(2, 4);
        switch (u.umonnum) {
        case PM_PAPER_GOLEM:
            alt = u.mhmax;
            break;
        case PM_STRAW_GOLEM:
            alt = u.mhmax / 2;
            break;
        case PM_WOOD_GOLEM:
            alt = u.mhmax / 4;
            break;
        case PM_LEATHER_GOLEM:
            alt = u.mhmax / 8;
            break;
        default:
            alt = 0;
            break;
        }
        if (alt > num)
            num = alt;
        if (u.mhmax > mons[u.umonnum].mlevel)
            u.mhmax -= rn2(min(u.mhmax, num + 1));
    } else {
        num = dice(2, 4);
        if (u.uhpmax > u.ulevel)
            u.uhpmax -= rn2(min(u.uhpmax, num + 1));
    }
    if (!num)
        pline("You are uninjured.");
    else
        losehp(num, killer_msg(DIED, an(tower_of_flame)));
    burn_away_slime();

    if (burnarmor(&youmonst) || rn2(3)) {
        destroy_item(SCROLL_CLASS, AD_FIRE);
        destroy_item(SPBOOK_CLASS, AD_FIRE);
        destroy_item(POTION_CLASS, AD_FIRE);
    }
    if (!box && burn_floor_paper(level, u.ux, u.uy, see_it, TRUE) && !see_it)
        pline("You smell paper burning.");
    if (is_ice(lev, u.ux, u.uy)) {
        struct trap *tt = NULL;
        int xbak = 0, ybak = 0;

        if (!box) {
            tt = t_at(level, u.ux, u.uy);
            if (tt) {
                xbak = tt->tx;
                ybak = tt->ty;
                tt->tx = tt->ty = 0;
            } else {
                impossible("dofiretrap: no tt and no box?");
            }
        }
        melt_ice(lev, u.ux, u.uy);
        if (tt) {
            tt->tx = xbak;
            tt->ty = ybak;
        }
    }
}

static void
domagictrap(void)
{
    int fate = rnd(20);

    /* What happened to the poor sucker? */

    if (fate < 10) {
        /* Most of the time, it creates some monsters. */
        int cnt = rnd(4);

        if (!resists_blnd(&youmonst)) {
            pline("You are momentarily blinded by a flash of light!");
            make_blinded((long)rn1(5, 10), FALSE);
            if (!Blind)
                pline("Your vision quickly clears.");
        } else if (!Blind) {
            pline("You see a flash of light!");
        } else
            You_hear("a deafening roar!");
        /* Use the "create monster used by monster" RNG for the species; the
           other option is the "random monster generation on this level" RNG,
           but I don't like that one as much; it's rather about what we want to
           keep consistency with */
        while (cnt--)
            makemon(NULL, level, u.ux, u.uy, MM_CREATEMONSTER | MM_CMONSTER_M);
    } else
        switch (fate) {

        case 10:
        case 11:
            /* sometimes nothing happens */
            break;
        case 12:       /* a flash of fire */
            dofiretrap(NULL);
            break;

            /* odd feelings */
        case 13:
            pline("A shiver runs up and down your %s!", body_part(SPINE));
            break;
        case 14:
            You_hear(Hallucination ? "the moon howling at you." :
                     "distant howling.");
            break;
        case 15:
            if (on_level(&u.uz, &qstart_level))
                pline("You feel %slike the prodigal son.",
                      (u.ufemale ||
                       (Upolyd && is_neuter(youmonst.data))) ? "oddly " : "");
            else
                pline("You suddenly yearn for %s.",
                      Hallucination ? "Cleveland" :
                      (In_quest(&u.uz) || at_dgn_entrance(&u.uz, "The Quest")) ?
                      "your nearby homeland" : "your distant homeland");
            break;
        case 16:
            pline("Your pack shakes violently!");
            break;
        case 17:
            pline(Hallucination ? "You smell hamburgers." :
                  "You smell charred flesh.");
            break;
        case 18:
            pline("You feel tired.");
            break;

            /* very occasionally something nice happens. */

        case 19:
            /* tame nearby monsters */
            {
                int i, j;
                struct monst *mtmp;

                adjattrib(A_CHA, 1, FALSE);
                for (i = -1; i <= 1; i++)
                    for (j = -1; j <= 1; j++) {
                        if (!isok(u.ux + i, u.uy + j))
                            continue;
                        mtmp = m_at(level, u.ux + i, u.uy + j);
                        if (mtmp)
                            tamedog(mtmp, NULL);
                    }
                break;
            }

        case 20:
            /* uncurse stuff */
            {
                struct obj *pseudo = mktemp_sobj(NULL, SCR_REMOVE_CURSE);
                boolean dummy;
                long save_conf = HConfusion;

                HConfusion = 0L;
                seffects(pseudo, &dummy);
                HConfusion = save_conf;

                obfree(pseudo, NULL);
                break;
            }
        default:
            break;
        }
}

/*
 * Scrolls, spellbooks, potions, and flammable items
 * may get affected by the fire.
 *
 * Return number of objects destroyed. --ALI
 */
int
fire_damage(struct obj *chain, boolean force, boolean here, xchar x, xchar y)
{
    int chance;
    struct obj *obj, *otmp, *nobj, *ncobj;
    int retval = 0;
    int in_sight = !Blind && couldsee(x, y);    /* Don't care if it's lit */

    for (obj = chain; obj; obj = nobj) {
        nobj = here ? obj->nexthere : obj->nobj;

        if (obj->oerodeproof)
            continue;

        /* object might light in a controlled manner */
        if (catch_lit(obj))
            continue;

        if (Is_container(obj)) {
            switch (obj->otyp) {
            case ICE_BOX:
                continue;       /* Immune */
            case CHEST:
                chance = 40;
                break;
            case LARGE_BOX:
                chance = 30;
                break;
            default:
                chance = 20;
                break;
            }
            if (!force && (Luck + 5) > rn2(chance))
                continue;
            /* Container is burnt up - dump contents out */
            if (in_sight)
                pline("%s catches fire and burns.", Yname2(obj));
            if (Has_contents(obj)) {
                if (in_sight)
                    pline("Its contents fall out.");
                for (otmp = obj->cobj; otmp; otmp = ncobj) {
                    ncobj = otmp->nobj;
                    obj_extract_self(otmp);
                    if (!flooreffects(otmp, x, y, ""))
                        place_object(otmp, level, x, y);
                }
            }
            delobj(obj);
            retval++;
        } else if (!force && (Luck + 5) > rn2(20)) {
            /* chance per item of sustaining damage: max luck (full moon): 5%
               max luck (elsewhen): 10% avg luck (Luck==0): 75% awful luck
               (Luck<-4): 100% */
            continue;
        } else if (obj->oclass == SCROLL_CLASS || obj->oclass == SPBOOK_CLASS) {
            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                continue;
            if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                if (in_sight)
                    pline("Smoke rises from %s.", the(xname(obj)));
                continue;
            }
            enum destroy_msg_type dindx = (obj->oclass == SCROLL_CLASS) ?
                destroy_msg_scroll_fire : destroy_msg_spellbook_fire;
            if (in_sight)
                pline("%s %s.", Yname2(obj), (obj->quan > 1) ?
                          destroy_messages[dindx].singular :
                          destroy_messages[dindx].plural);
            delobj(obj);
            retval++;
        } else if (obj->oclass == POTION_CLASS) {
            if (in_sight)
                pline("%s %s.", Yname2(obj), (obj->quan > 1) ?
                          destroy_messages[destroy_msg_potion_fire].plural :
                          destroy_messages[destroy_msg_potion_fire].singular);
            delobj(obj);
            retval++;
        } else
            erode_obj(obj, NULL, ERODE_BURN, FALSE, TRUE);
    }

    if (retval && !in_sight)
        pline("You smell smoke.");
    return retval;
}


void
acid_damage(struct obj *obj)
{
    if (!obj)
        return;

    /* Scrolls (but not spellbooks) fade. There is no particular reason for this
       other than to preserve vanilla behaviour. See ticket #454. */
    struct monst *victim =
        carried(obj) ? &youmonst : mcarried(obj) ? obj->ocarry : NULL;
    boolean vismon = victim && (victim != &youmonst) && canseemon(victim);

    if (obj->greased)
        grease_protect(obj, NULL, victim);
    else if (obj->oclass == SCROLL_CLASS && obj->otyp != SCR_BLANK_PAPER) {
        if (obj->otyp != SCR_BLANK_PAPER) {
            if (!Blind) {
                if (victim == &youmonst)
                    pline("Your %s.", aobjnam(obj, "fade"));
                else if (vismon)
                    pline("%s's %s.", Monnam(victim), aobjnam(obj, "fade"));
            }
            obj->otyp = SCR_BLANK_PAPER;
            obj->spe = 0;
        }
    } else
        erode_obj(obj, NULL, ERODE_CORRODE, FALSE, TRUE);
}


/* returns:
 *  0 if obj is unaffected
 *  1 if obj is protected by grease
 *  2 if obj is changed but survived
 *  3 if obj is destroyed
 */
int
water_damage(struct obj * obj, const char *ostr, boolean force)
{
    if (!obj)
        return 0;

    if (snuff_lit(obj))
        return 2;

    if (obj->otyp == CAN_OF_GREASE && obj->spe > 0) {
        return 0;
    } else if (obj->greased) {
        if (!rn2(2))
            obj->greased = 0;
        if (carried(obj))
            update_inventory();
        return 1;
    } else if (Is_container(obj) && !Is_box(obj) &&
                (obj->otyp != OILSKIN_SACK || (obj->cursed && !rn2(3)))) {
        water_damage_chain(obj->cobj, FALSE);
        return 0;
    } else if (!force && (Luck + 5) > rn2(20)) {
        /* chance per item of sustaining damage: max luck (full moon): 5%
            max luck (elsewhen): 10% avg luck (Luck==0): 75% awful luck
            (Luck<-4): 100% */
        return 0;
    } else if (obj->oclass == SCROLL_CLASS) {
        obj->otyp = SCR_BLANK_PAPER;
        obj->spe = 0;
        if (carried(obj))
            update_inventory();
        return 2;
    } else if (obj->oclass == SPBOOK_CLASS) {
        if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
            pline("Steam rises from %s.", the(xname(obj)));
            return 0;
        } else {
            obj->otyp = SPE_BLANK_PAPER;
            if (carried(obj))
                update_inventory();
            return 2;
        }
    } else if (obj->oclass == POTION_CLASS) {
        if (obj->otyp == POT_ACID) {
            /* damage player/monster? */
            pline("A potion explodes!");
            boolean update = carried(obj);
            delobj(obj);
            if (update)
                update_inventory();
            return 3;
        } else if (obj->odiluted) {
            obj->otyp = POT_WATER;
            obj->blessed = obj->cursed = 0;
            obj->odiluted = 0;
            if (carried(obj))
                update_inventory();
            return 2;
        } else if (obj->otyp != POT_WATER) {
            obj->odiluted++;
            if (carried(obj))
                update_inventory();
            return 2;
        }
    } else {
        return erode_obj(obj, ostr, ERODE_RUST, FALSE, FALSE) ? 2 : 0;
    }
    
    return 0;
}

void
water_damage_chain(struct obj *obj, boolean here)
{
    struct obj *otmp;

    for (; obj; obj = otmp) {
        otmp = here ? obj->nexthere : obj->nobj;

        water_damage(obj, NULL, FALSE);
    }
}

/*
 * This function is potentially expensive - rolling
 * inventory list multiple times.  Luckily it's seldom needed.
 * Returns TRUE if disrobing made player unencumbered enough to
 * crawl out of the current predicament.
 */
static boolean
emergency_disrobe(boolean * lostsome)
{
    int invc = inv_cnt(FALSE);

    while (near_capacity() > (Punished ? UNENCUMBERED : SLT_ENCUMBER)) {
        struct obj *obj, *otmp = NULL;
        int i;

        /* Pick a random object */
        if (invc > 0) {
            i = rn2(invc);
            for (obj = invent; obj; obj = obj->nobj) {
                /* Undroppables are: body armor (including skin), boots, gloves,
                   amulets, and rings because of the time and effort in removing
                   them; and loadstones and other cursed stuff for obvious
                   reasons. */
                if (!
                    ((obj->otyp == LOADSTONE && obj->cursed) || obj == uamul ||
                     obj == uleft || obj == uright || obj == ublindf ||
                     obj == uarm || obj == uarmc || obj == uarmg || obj == uarmf
                     || obj == uarmu || (obj->cursed &&
                                         (obj == uarmh || obj == uarms)) ||
                     welded(obj)))
                    otmp = obj;
                /* reached the mark and found some stuff to drop? */
                if (--i < 0 && otmp)
                    break;

                /* else continue */
            }
        }
        if (!otmp)
            return FALSE;       /* nothing to drop! */

        if (otmp->owornmask)
            remove_worn_item(otmp, FALSE);
        *lostsome = TRUE;
        dropx(otmp);
        invc--;
    }
    return TRUE;
}

/*
 *  return(TRUE) == player relocated
 */
boolean
drown(void)
{
    boolean inpool_ok = FALSE, crawl_ok;
    int i, x, y;

    /* happily wading in the same contiguous pool */
    if (u.uinwater && (!u.umoved || is_pool(level, u.ux0, u.uy0)) &&
        (Swimming || Amphibious)) {
        /* water effects on objects every now and then */
        if (!rn2(5))
            inpool_ok = TRUE;
        else
            return FALSE;
    }

    if (!u.uinwater) {
        pline("You %s into the water%c",
              Is_waterlevel(&u.uz) ? "plunge" : "fall", Amphibious ||
              Swimming ? '.' : '!');
        if (!Swimming && !Is_waterlevel(&u.uz))
            pline("You sink like %s.",
                  Hallucination ? "the Titanic" : "a rock");
    }

    water_damage_chain(invent, FALSE);

    if (u.umonnum == PM_GREMLIN && rn2(3))
        split_mon(&youmonst, NULL);
    else if (u.umonnum == PM_IRON_GOLEM) {
        pline("You rust!");
        i = dice(2, 6);
        if (u.mhmax > i)
            u.mhmax -= i;
        losehp(i, killer_msg(DIED, msgcat("rusted away in ",
                                          Is_waterlevel(&u.uz) ? "water" :
                                          a_waterbody(u.ux, u.uy))));
    }
    if (inpool_ok)
        return FALSE;

    if ((i = number_leashed()) > 0) {
        pline("The leash%s slip%s loose.", (i > 1) ? "es" : "",
              (i > 1) ? "" : "s");
        unleash_all();
    }

    if (Amphibious || Swimming) {
        if (Amphibious) {
            if (flags.verbose)
                pline("But you aren't drowning.");
            if (!Is_waterlevel(&u.uz)) {
                if (Hallucination)
                    pline("Your keel hits the bottom.");
                else
                    pline("You touch bottom.");
            }
        }
        if (Punished) {
            unplacebc();
            placebc();
        }
        vision_recalc(2);       /* unsee old position */
        u.uinwater = 1;
        under_water(1);
        turnstate.vision_full_recalc = TRUE;
        return FALSE;
    }
    /* TODO: Isn't this u_helpless check backwards? */
    if ((Teleportation || can_teleport(youmonst.data)) &&
        u_helpless(hm_unconscious) && (Teleport_control || rn2(3) < Luck + 2)) {
        pline("You attempt a teleport spell."); /* utcsri!carroll */
        if (!level->flags.noteleport) {
            dotele(&(struct nh_cmd_arg){.argtype = 0});
            if (!is_pool(level, u.ux, u.uy))
                return TRUE;
        } else
            pline("The attempted teleport spell fails.");
    }
    if (u.usteed) {
        dismount_steed(DISMOUNT_GENERIC);
        if (!is_pool(level, u.ux, u.uy))
            return TRUE;
    }
    crawl_ok = FALSE;
    x = y = 0;  /* lint suppression */
    /* if sleeping, wake up now so that we don't crawl out of water while still
       asleep */
    cancel_helplessness(hm_unconscious, "Suddenly you wake up!");
    /* can't crawl if unable to move (crawl_ok flag stays false) */
    if (u_helpless(hm_all) || (Upolyd && !youmonst.data->mmove))
        goto crawl;
    /* look around for a place to crawl to */
    for (i = 0; i < 100; i++) {
        x = rn1(3, u.ux - 1);
        y = rn1(3, u.uy - 1);
        if (goodpos(level, x, y, &youmonst, 0)) {
            crawl_ok = TRUE;
            goto crawl;
        }
    }
    /* one more scan */
    for (x = u.ux - 1; x <= u.ux + 1; x++)
        for (y = u.uy - 1; y <= u.uy + 1; y++)
            if (goodpos(level, x, y, &youmonst, 0)) {
                crawl_ok = TRUE;
                goto crawl;
            }
crawl:
    if (crawl_ok) {
        boolean lost = FALSE;

        /* time to do some strip-tease... */
        boolean succ = Is_waterlevel(&u.uz) ? TRUE : emergency_disrobe(&lost);

        pline("You try to crawl out of the water.");
        if (lost)
            pline("You dump some of your gear to lose weight...");
        if (succ) {
            pline("Pheew!  That was close.");
            teleds(x, y, TRUE);
            return TRUE;
        }
        /* still too much weight */
        pline("But in vain.");
    }
    u.uinwater = 1;
    pline("You drown.");
    done(DROWNING,
         killer_msg(DROWNING,
                    Is_waterlevel(&u.uz) ? "the Plane of Water"
                                         : a_waterbody(u.ux, u.uy)));
    /* oops, we're still alive.  better get out of the water. */
    while (!safe_teleds(TRUE)) {
        pline("You're still drowning.");
        done(DROWNING,
             killer_msg(DROWNING,
                        msgcat(Is_waterlevel(&u.uz) ? "the Plane of Water"
                               : a_waterbody(u.ux, u.uy),
                               " despite being life-saved")));
    }
    if (u.uinwater) {
        u.uinwater = 0;
        pline("You find yourself back %s.",
              Is_waterlevel(&u.uz) ? "in an air bubble" : "on land");
    }
    return TRUE;
}

void
drain_en(int n)
{
    if (!u.uenmax)
        return;
    pline("You feel your magical energy drain away!");
    u.uen -= n;
    if (u.uen < 0) {
        u.uenmax += u.uen;
        if (u.uenmax < 0)
            u.uenmax = 0;
        u.uen = 0;
    }
}

/* disarm a trap */
int
dountrap(const struct nh_cmd_arg *arg)
{
    if (near_capacity() >= HVY_ENCUMBER) {
        pline("You're too strained to do that.");
        return 0;
    }
    if ((nohands(youmonst.data) && !webmaker(youmonst.data)) ||
        !youmonst.data->mmove) {
        pline("And just how do you expect to do that?");
        return 0;
    } else if (u.ustuck && sticks(youmonst.data)) {
        pline("You'll have to let go of %s first.", mon_nam(u.ustuck));
        return 0;
    }
    if (u.ustuck || (welded(uwep) && bimanual(uwep))) {
        pline("Your %s seem to be too busy for that.",
              makeplural(body_part(HAND)));
        return 0;
    }
    return untrap(arg, FALSE);
}


/* Probability of disabling a trap.  Helge Hafting */
static int
untrap_prob(struct trap *ttmp)
{
    int chance = 3;

    /* Only spiders know how to deal with webs reliably */
    if (ttmp->ttyp == WEB && !webmaker(youmonst.data))
        chance = 30;
    if (Confusion || Hallucination)
        chance++;
    if (Blind)
        chance++;
    if (Stunned)
        chance += 2;
    if (Fumbling)
        chance *= 2;
    /* Your own traps are better known than others. */
    if (ttmp->madeby_u)
        chance--;
    if (Role_if(PM_ROGUE)) {
        if (rn2(2 * MAXULEV) < u.ulevel)
            chance--;
        if (Uhave_questart && chance > 1)
            chance--;
    } else if (Role_if(PM_RANGER) && chance > 1)
        chance--;
    return rn2(chance);
}

/* Replace trap with object(s).  Helge Hafting */
void
cnv_trap_obj(struct level *lev, int otyp, int cnt, struct trap *ttmp)
{
    struct obj *otmp = mksobj(lev, otyp, TRUE, FALSE, rng_main);

    otmp->quan = cnt;
    otmp->owt = weight(otmp);
    /* Only dart traps are capable of being poisonous */
    if (otyp != DART)
        otmp->opoisoned = 0;
    place_object(otmp, lev, ttmp->tx, ttmp->ty);
    /* Sell your own traps only... */
    if (ttmp->madeby_u)
        sellobj(otmp, ttmp->tx, ttmp->ty);
    stackobj(otmp);
    if (lev == level)
        newsym(ttmp->tx, ttmp->ty);
    deltrap(lev, ttmp);
}

/* while attempting to disarm an adjacent trap, we've fallen into it */
static void
move_into_trap(struct trap *ttmp)
{
    int bc = 0;
    xchar x = ttmp->tx, y = ttmp->ty, bx = -1, by = -1, cx = -1, cy = -1;
    boolean unused;

    /* we know there's no monster in the way, and we're not trapped */
    if (!Punished || drag_ball(x, y, &bc, &bx, &by, &cx, &cy, &unused, TRUE)) {
        u.ux0 = u.ux, u.uy0 = u.uy;
        u.ux = x, u.uy = y;
        u.umoved = TRUE;
        newsym(u.ux0, u.uy0);
        vision_recalc(1);
        check_leash(u.ux0, u.uy0);
        if (Punished)
            move_bc(0, bc, bx, by, cx, cy);
        spoteffects(FALSE);     /* dotrap() */
        exercise(A_WIS, FALSE);
    }
}

/* 0: doesn't even try
 * 1: tries and fails
 * 2: succeeds
 */
static int
try_disarm(struct trap *ttmp, boolean force_failure, schar dx, schar dy)
{
    struct monst *mtmp = m_at(level, ttmp->tx, ttmp->ty);
    int ttype = ttmp->ttyp;
    boolean under_u = (!dx && !dy);
    boolean holdingtrap = (ttype == BEAR_TRAP || ttype == WEB);

    /* Test for monster first, monsters are displayed instead of trap. */
    if (mtmp && (!mtmp->mtrapped || !holdingtrap)) {
        pline("%s is in the way.", Monnam(mtmp));
        return 0;
    }
    /* We might be forced to move onto the trap's location. */
    if (sobj_at(BOULDER, level, ttmp->tx, ttmp->ty)
        && !Passes_walls && !under_u) {
        pline("There is a boulder in your way.");
        return 0;
    }
    /* duplicate tight-space checks from test_move */
    if (dx && dy && bad_rock(youmonst.data, u.ux, ttmp->ty) &&
        bad_rock(youmonst.data, ttmp->tx, u.uy)) {
        if ((invent && (inv_weight() + weight_cap() > 600)) ||
            bigmonst(youmonst.data)) {
            /* don't allow untrap if they can't get thru to it */
            pline("You are unable to reach the %s!", trapexplain[ttype - 1]);
            return 0;
        }
    }
    /* untrappable traps are located on the ground. */
    if (!can_reach_floor()) {
        if (u.usteed && P_SKILL(P_RIDING) < P_BASIC)
            pline("You aren't skilled enough to reach from %s.",
                  mon_nam(u.usteed));
        else
            pline("You are unable to reach the %s!", trapexplain[ttype - 1]);
        return 0;
    }

    /* Will our hero succeed? */
    if (force_failure || untrap_prob(ttmp)) {
        if (rnl(5)) {
            pline("Whoops...");
            if (mtmp) { /* must be a trap that holds monsters */
                if (ttype == BEAR_TRAP) {
                    if (mtmp->mtame)
                        abuse_dog(mtmp);
                    if ((mtmp->mhp -= rnd(4)) <= 0)
                        killed(mtmp);
                } else if (ttype == WEB) {
                    if (!webmaker(youmonst.data)) {
                        struct trap *ttmp2 =
                            maketrap(level, u.ux, u.uy, WEB, rng_main);

                        if (ttmp2) {
                            pline("The webbing sticks to you. You're caught "
                                  "too!");
                            dotrap(ttmp2, NOWEBMSG);
                            if (u.usteed && u.utrap) {
                                /* you, not steed, are trapped */
                                dismount_steed(DISMOUNT_FELL);
                            }
                        }
                    } else
                        pline("%s remains entangled.", Monnam(mtmp));
                }
            } else if (under_u) {
                dotrap(ttmp, 0);
            } else {
                move_into_trap(ttmp);
            }
        } else {
            pline("%s %s is difficult to %s.",
                  ttmp->madeby_u ? "Your" : under_u ? "This" : "That",
                  trapexplain[ttype - 1], (ttype == WEB) ? "remove" : "disarm");
        }
        return 1;
    }
    return 2;
}

static void
reward_untrap(struct trap *ttmp, struct monst *mtmp)
{
    if (!ttmp->madeby_u) {
        if (rnl(10) < 8 && !mtmp->mpeaceful && !mtmp->msleeping &&
            !mtmp->mfrozen && !mindless(mtmp->data) &&
            mtmp->data->mlet != S_HUMAN) {
            msethostility(mtmp, FALSE, TRUE);
            pline("%s is grateful.", Monnam(mtmp));
        }
        /* Helping someone out of a trap is a nice thing to do, A lawful may be
           rewarded, but not too often.  */
        if (!rn2(3) && !rnl(8) && u.ualign.type == A_LAWFUL) {
            adjalign(1);
            pline("You feel that you did the right thing.");
        }
    }
}

static int
disarm_holdingtrap(struct trap *ttmp, schar dx, schar dy)
{
    struct monst *mtmp;
    int fails = try_disarm(ttmp, FALSE, dx, dy);

    if (fails < 2)
        return fails;

    /* ok, disarm it. */

    /* untrap the monster, if any. There's no need for a cockatrice test, only
       the trap is touched */
    if ((mtmp = m_at(level, ttmp->tx, ttmp->ty)) != 0) {
        mtmp->mtrapped = 0;
        pline("You remove %s %s from %s.", the_your[ttmp->madeby_u],
              (ttmp->ttyp == BEAR_TRAP) ? "bear trap" : "webbing",
              mon_nam(mtmp));
        reward_untrap(ttmp, mtmp);
    } else {
        if (ttmp->ttyp == BEAR_TRAP) {
            pline("You disarm %s bear trap.", the_your[ttmp->madeby_u]);
            makeknown(BEARTRAP);
            cnv_trap_obj(level, BEARTRAP, 1, ttmp);
        } else {        /* if (ttmp->ttyp == WEB) */

            pline("You succeed in removing %s web.", the_your[ttmp->madeby_u]);
            deltrap(level, ttmp);
        }
    }
    level->locations[u.ux + dx][u.uy + dy].mem_trap = NO_TRAP;
    newsym(u.ux + dx, u.uy + dy);
    return 1;
}

static int
disarm_landmine(struct trap *ttmp, schar dx, schar dy)
{
    int fails = try_disarm(ttmp, FALSE, dx, dy);

    if (fails < 2)
        return fails;
    pline("You disarm %s land mine.", the_your[ttmp->madeby_u]);
    cnv_trap_obj(level, LAND_MINE, 1, ttmp);
    level->locations[u.ux + dx][u.uy + dy].mem_trap = NO_TRAP;
    newsym(u.ux + dx, u.uy + dy);
    makeknown(LAND_MINE);
    return 1;
}

/* getobj will filter down to cans of grease and known potions of oil */
static const char oil[] = { ALL_CLASSES, TOOL_CLASS, POTION_CLASS, 0 };

/* it may not make much sense to use grease on floor boards, but so what? */
static int
disarm_squeaky_board(struct trap *ttmp, schar dx, schar dy)
{
    struct obj *obj;
    boolean bad_tool;
    int fails;

    obj = getobj(oil, "untrap with", FALSE);
    if (!obj)
        return 0;

    bad_tool = (obj->cursed ||
                ((obj->otyp != POT_OIL || obj->lamplit) &&
                 (obj->otyp != CAN_OF_GREASE || !obj->spe)));

    fails = try_disarm(ttmp, bad_tool, dx, dy);
    if (fails < 2)
        return fails;

    /* successfully used oil or grease to fix squeaky board */
    if (obj->otyp == CAN_OF_GREASE) {
        consume_obj_charge(obj, TRUE);
    } else {
        useup(obj);     /* oil */
        makeknown(POT_OIL);
    }
    pline("You repair the squeaky board.");     /* no madeby_u */
    level->locations[u.ux + dx][u.uy + dy].mem_trap = NO_TRAP;
    deltrap(level, ttmp);
    newsym(u.ux + dx, u.uy + dy);
    more_experienced(1, 5);
    newexplevel();
    return 1;
}

/* removes traps that shoot arrows, darts, etc. */
static int
disarm_shooting_trap(struct trap *ttmp, int otyp, schar dx, schar dy)
{
    int fails = try_disarm(ttmp, FALSE, dx, dy);

    if (fails < 2)
        return fails;
    pline("You disarm %s trap.", the_your[ttmp->madeby_u]);
    cnv_trap_obj(level, otyp, 50 - rnl(50), ttmp);
    level->locations[u.ux + dx][u.uy + dy].mem_trap = NO_TRAP;
    newsym(u.ux + dx, u.uy + dy);
    return 1;
}

/* Is the weight too heavy?
 * Formula as in near_capacity() & check_capacity() */
static int
try_lift(struct monst *mtmp, struct trap *ttmp, int wt, boolean stuff)
{
    int wc = weight_cap();

    if (((wt * 2) / wc) >= HVY_ENCUMBER) {
        pline("%s is %s for you to lift.", Monnam(mtmp),
              stuff ? "carrying too much" : "too heavy");
        if (!ttmp->madeby_u && !mtmp->mpeaceful && mtmp->mcanmove &&
            !mindless(mtmp->data) && mtmp->data->mlet != S_HUMAN &&
            rnl(10) < 3) {
            msethostility(mtmp, FALSE, TRUE);
            pline("%s thinks it was nice of you to try.", Monnam(mtmp));
        }
        return 0;
    }
    return 1;
}

/* Help trapped monster (out of a (spiked) pit) */
static int
help_monster_out(struct monst *mtmp, struct trap *ttmp)
{
    int wt;
    struct obj *otmp;
    boolean uprob;

    /*
     * This works when levitating too -- consistent with the ability
     * to hit monsters while levitating.
     *
     * Should perhaps check that our hero has arms/hands at the
     * moment.  Helping can also be done by engulfing...
     *
     * Test the monster first - monsters are displayed before traps.
     */
    if (!mtmp->mtrapped) {
        pline("%s isn't trapped.", Monnam(mtmp));
        return 0;
    }
    /* Do you have the necessary capacity to lift anything? */
    if (check_capacity(NULL))
        return 1;

    if (Levitation) {
        pline("You cannot reach %s.", mon_nam(mtmp));
        return 0;
    }

    /* Will our hero succeed? */
    if ((uprob = untrap_prob(ttmp)) && !mtmp->msleeping && mtmp->mcanmove) {
        pline("You try to reach out your %s, but %s backs away skeptically.",
              makeplural(body_part(ARM)), mon_nam(mtmp));
        return 1;
    }


    /* is it a cockatrice?... */
    if (!uarmg && touched_monster(mtmp->data - mons)) {
        pline("You grab the trapped %s using your bare %s.", mtmp->data->mname,
              makeplural(body_part(HAND)));
        instapetrify(killer_msg(STONING,
                                msgprintf("trying to help %s out of a pit",
                                          an(mtmp->data->mname))));
        return 1;
    }
    /* need to do cockatrice check first if sleeping or paralyzed */
    if (uprob) {
        pline("You try to grab %s, but cannot get a firm grasp.",
              mon_nam(mtmp));
        if (mtmp->msleeping) {
            mtmp->msleeping = 0;
            pline("%s awakens.", Monnam(mtmp));
        }
        return 1;
    }

    pline("You reach out your %s and grab %s.", makeplural(body_part(ARM)),
          mon_nam(mtmp));

    if (mtmp->msleeping) {
        mtmp->msleeping = 0;
        pline("%s awakens.", Monnam(mtmp));
    } else if (mtmp->mfrozen && !rn2(mtmp->mfrozen)) {
        /* After such manhandling, perhaps the effect wears off */
        mtmp->mcanmove = 1;
        mtmp->mfrozen = 0;
        pline("%s stirs.", Monnam(mtmp));
    }

    /* is the monster too heavy? */
    wt = inv_weight() + mtmp->data->cwt;
    if (!try_lift(mtmp, ttmp, wt, FALSE))
        return 1;

    /* is the monster with inventory too heavy? */
    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
        wt += otmp->owt;
    if (!try_lift(mtmp, ttmp, wt, TRUE))
        return 1;

    pline("You pull %s out of the pit.", mon_nam(mtmp));
    mtmp->mtrapped = 0;
    fill_pit(level, mtmp->mx, mtmp->my);
    reward_untrap(ttmp, mtmp);
    return 1;
}

int
untrap(const struct nh_cmd_arg *arg, boolean force)
{
    struct obj *otmp;
    boolean confused = (Confusion > 0 || Hallucination > 0);
    int x, y;
    int ch;
    struct trap *ttmp;
    struct monst *mtmp;
    boolean trap_skipped = FALSE;
    boolean box_here = FALSE;
    boolean deal_with_floor_trap = FALSE;
    const char *the_trap, *qbuf;
    int containercnt = 0;
    schar dx, dy, dz;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;
    x = u.ux + dx;
    y = u.uy + dy;

    for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere) {
        if (Is_box(otmp) && !dx && !dy) {
            box_here = TRUE;
            containercnt++;
            if (containercnt > 1)
                break;
        }
    }

    if ((ttmp = t_at(level, x, y)) && ttmp->tseen) {
        deal_with_floor_trap = TRUE;
        the_trap = the(trapexplain[ttmp->ttyp - 1]);
        if (box_here) {
            if (ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT) {
                pline("You can't do much about %s%s.", the_trap,
                      u.utrap ? " that you're stuck in" :
                      " while standing on the edge of it");
                trap_skipped = TRUE;
                deal_with_floor_trap = FALSE;
            } else {
                qbuf = msgprintf("There %s and %s here. %s %s?",
                                 (containercnt == 1) ?
                                 "is a container" : "are containers",
                                 an(trapexplain[ttmp->ttyp - 1]),
                                 ttmp->ttyp == WEB ? "Remove" : "Disarm",
                                 the_trap);
                switch (ynq(qbuf)) {
                case 'q':
                    return 0;
                case 'n':
                    trap_skipped = TRUE;
                    deal_with_floor_trap = FALSE;
                    break;
                }
            }
        }
        if (deal_with_floor_trap) {
            if (u.utrap) {
                pline("You cannot deal with %s while trapped%s!", the_trap,
                      (x == u.ux && y == u.uy) ? " in it" : "");
                return 1;
            }
            switch (ttmp->ttyp) {
            case BEAR_TRAP:
            case WEB:
                return disarm_holdingtrap(ttmp, dx, dy);
            case LANDMINE:
                return disarm_landmine(ttmp, dx, dy);
            case SQKY_BOARD:
                return disarm_squeaky_board(ttmp, dx, dy);
            case DART_TRAP:
                return disarm_shooting_trap(ttmp, DART, dx, dy);
            case ARROW_TRAP:
                return disarm_shooting_trap(ttmp, ARROW, dx, dy);
            case PIT:
            case SPIKED_PIT:
                if (!dx && !dy) {
                    pline("You are already on the edge of the pit.");
                    return 0;
                }
                if (!(mtmp = m_at(level, x, y))) {
                    pline("Try filling the pit instead.");
                    return 0;
                }
                return help_monster_out(mtmp, ttmp);
            default:
                pline("You cannot disable %s trap.",
                      (dx || dy) ? "that" : "this");
                return 0;
            }
        }
    }
    /* end if */
    if (!dx && !dy) {
        for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
            if (Is_box(otmp)) {
                qbuf = msgprintf(
                    "There is %s here. Check it for traps?",
                    safe_qbuf("",
                              sizeof ("There is  here. Check it for traps?"),
                              doname(otmp), an(simple_typename(otmp->otyp)),
                              "a box"));
                switch (ynq(qbuf)) {
                case 'q':
                    return 0;
                case 'n':
                    continue;
                }
                if (u.usteed && P_SKILL(P_RIDING) < P_BASIC) {
                    pline("You aren't skilled enough to reach from %s.",
                          mon_nam(u.usteed));
                    return 0;
                }
                if ((otmp->otrapped &&
                     (force || (!confused && rn2(MAXULEV + 1 - u.ulevel) < 10)))
                    || (!force && confused && !rn2(3))) {
                    pline("You find a trap on %s!", the(xname(otmp)));
                    if (!confused)
                        exercise(A_WIS, TRUE);

                    switch (ynq("Disarm it?")) {
                    case 'q':
                        return 1;
                    case 'n':
                        trap_skipped = TRUE;
                        continue;
                    }

                    if (otmp->otrapped) {
                        exercise(A_DEX, TRUE);
                        ch = ACURR(A_DEX) + u.ulevel;
                        if (Role_if(PM_ROGUE))
                            ch *= 2;
                        if (!force &&
                            (confused || Fumbling ||
                             rnd(75 + level_difficulty(&u.uz) / 2) > ch)) {
                            chest_trap(otmp, FINGER, TRUE);
                        } else {
                            pline("You disarm it!");
                            otmp->otrapped = 0;
                        }
                    } else
                        pline("That %s was not trapped.", xname(otmp));
                    return 1;
                } else {
                    pline("You find no traps on %s.", the(xname(otmp)));
                    return 1;
                }
            }

        pline(trap_skipped ? "You find no other traps here." :
              "You know of no traps here.");
        return 0;
    }

    if ((mtmp = m_at(level, x, y)) && mtmp->m_ap_type == M_AP_FURNITURE &&
        (mtmp->mappearance == S_hcdoor || mtmp->mappearance == S_vcdoor) &&
        !Protection_from_shape_changers) {

        stumble_onto_mimic(mtmp, dx, dy);
        return 1;
    }

    if (!IS_DOOR(level->locations[x][y].typ)) {
        if ((ttmp = t_at(level, x, y)) && ttmp->tseen)
            pline("You cannot disable that trap.");
        else
            pline("You know of no traps there.");
        return 0;
    }

    switch (level->locations[x][y].doormask) {
    case D_NODOOR:
        pline("You %s no door there.", Blind ? "feel" : "see");
        return 0;
    case D_ISOPEN:
        pline("This door is safely open.");
        return 0;
    case D_BROKEN:
        pline("This door is broken.");
        return 0;
    }

    if ((level->locations[x][y].doormask & D_TRAPPED &&
         (force || (!confused && rn2(MAXULEV - u.ulevel + 11) < 10)))
        || (!force && confused && !rn2(3))) {
        pline("You find a trap on the door!");
        exercise(A_WIS, TRUE);
        if (ynq("Disarm it?") != 'y')
            return 1;
        if (level->locations[x][y].doormask & D_TRAPPED) {
            ch = 15 + (Role_if(PM_ROGUE) ? u.ulevel * 3 : u.ulevel);
            exercise(A_DEX, TRUE);
            if (!force &&
                (confused || Fumbling ||
                 rnd(75 + level_difficulty(&u.uz) / 2) > ch)) {
                pline("You set it off!");
                b_trapped("door", FINGER);
                level->locations[x][y].doormask = D_NODOOR;
                unblock_point(x, y);
                newsym(x, y);
                /* (probably ought to charge for this damage...) */
                if (*in_rooms(level, x, y, SHOPBASE))
                    add_damage(x, y, 0L);
            } else {
                pline("You disarm it!");
                level->locations[x][y].doormask &= ~D_TRAPPED;
            }
        } else
            pline("This door was not trapped.");
        return 1;
    } else {
        pline("You find no traps on the door.");
        return 1;
    }
}


/* only called when the player is doing something to the chest directly */
boolean
chest_trap(struct obj * obj, int bodypart, boolean disarm)
{
    struct obj *otmp = obj, *otmp2;
    char buf[80];
    const char *msg;
    coord cc;

    if (get_obj_location(obj, &cc.x, &cc.y, 0)) /* might be carried */
        obj->ox = cc.x, obj->oy = cc.y;

    otmp->otrapped = 0; /* trap is one-shot; clear flag first in case chest
                           kills you and ends up in bones file */
    pline(disarm ? "You set it off!" : "You trigger a trap!");
    win_pause_output(P_MESSAGE);
    if (Luck > -13 && rn2(13 + Luck) > 7) {     /* saved by luck */
        /* trap went off, but good luck prevents damage */
        switch (rn2(13)) {
        case 12:
        case 11:
            msg = "explosive charge is a dud";
            break;
        case 10:
        case 9:
            msg = "electric charge is grounded";
            break;
        case 8:
        case 7:
            msg = "flame fizzles out";
            break;
        case 6:
        case 5:
        case 4:
            msg = "poisoned needle misses";
            break;
        case 3:
        case 2:
        case 1:
        case 0:
            msg = "gas cloud blows away";
            break;
        default:
            impossible("chest disarm bug");
            msg = NULL;
            break;
        }
        if (msg)
            pline("But luckily the %s!", msg);
    } else {
        switch (rn2(20) ? ((Luck >= 13) ? 0 : rn2(13 - Luck)) : rn2(26)) {
        case 25:
        case 24:
        case 23:
        case 22:
        case 21:{
                struct monst *shkp = 0;
                long loss = 0L;
                boolean costly, insider;
                xchar ox = obj->ox, oy = obj->oy;

                /* the obj location need not be that of player */
                costly = (costly_spot(ox, oy) &&
                          (shkp =
                           shop_keeper(level,
                                       *in_rooms(level, ox, oy,
                                                 SHOPBASE))) != NULL);
                insider = (*u.ushops && inside_shop(level, u.ux, u.uy) &&
                           *in_rooms(level, ox, oy, SHOPBASE) == *u.ushops);

                pline("%s!", Tobjnam(obj, "explode"));
                snprintf(buf, SIZE(buf), "exploding %s", xname(obj));

                if (costly)
                    loss +=
                        stolen_value(obj, ox, oy, (boolean) shkp->mpeaceful,
                                     TRUE);
                delete_contents(obj);
                /* we're about to delete all things at this location, which
                   could include the ball & chain. If we attempt to call
                   unpunish() in the for-loop below we can end up with otmp2
                   being invalid once the chain is gone. Deal with ball & chain
                   right now instead. */
                if (Punished && !carried(uball) &&
                    ((uchain->ox == u.ux && uchain->oy == u.uy) ||
                     (uball->ox == u.ux && uball->oy == u.uy)))
                    unpunish();

                for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp2) {
                    otmp2 = otmp->nexthere;
                    if (costly)
                        loss +=
                            stolen_value(otmp, otmp->ox, otmp->oy,
                                         (boolean) shkp->mpeaceful, TRUE);
                    delobj(otmp);
                }
                wake_nearby(FALSE);
                losehp(dice(6, 6), killer_msg(DIED, an(buf)));
                exercise(A_STR, FALSE);
                if (costly && loss) {
                    if (insider)
                        pline("You owe %ld %s for objects destroyed.", loss,
                              currency(loss));
                    else {
                        pline("You caused %ld %s worth of damage!", loss,
                              currency(loss));
                        make_angry_shk(shkp, ox, oy);
                    }
                }
                return TRUE;
            }
        case 20:
        case 19:
        case 18:
        case 17:
            pline("A cloud of noxious gas billows from %s.", the(xname(obj)));
            poisoned("gas cloud", A_STR,
                     killer_msg(DIED, "a cloud of poison gas"), 15);
            exercise(A_CON, FALSE);
            break;
        case 16:
        case 15:
        case 14:
        case 13:
            pline("You feel a needle prick your %s.", body_part(bodypart));
            poisoned("needle", A_CON,
                     killer_msg(DIED, "a poisoned needle"), 10);
            exercise(A_CON, FALSE);
            break;
        case 12:
        case 11:
        case 10:
        case 9:
            dofiretrap(obj);
            break;
        case 8:
        case 7:
        case 6:{
                int dmg;

                pline("You are jolted by a surge of electricity!");
                if (Shock_resistance) {
                    shieldeff(u.ux, u.uy);
                    pline("You don't seem to be affected.");
                    dmg = 0;
                } else
                    dmg = dice(4, 4);
                destroy_item(RING_CLASS, AD_ELEC);
                destroy_item(WAND_CLASS, AD_ELEC);
                if (dmg)
                    losehp(dmg, killer_msg(DIED, "an electric shock"));
                break;
            }
        case 5:
        case 4:
        case 3:
            if (!Free_action) {
                pline("Suddenly you are frozen in place!");
                helpless(dice(5, 6), hr_paralyzed, "frozen by a trap", NULL);
                exercise(A_DEX, FALSE);
            } else
                pline("You momentarily stiffen.");
            break;
        case 2:
        case 1:
        case 0:
            pline("A cloud of %s gas billows from %s.",
                  Blind ? blindgas[rn2(SIZE(blindgas))] : rndcolor(),
                  the(xname(obj)));
            if (!Stunned) {
                if (Hallucination)
                    pline("What a groovy feeling!");
                else if (Blind)
                    pline("You %s and get dizzy...",
                          stagger(youmonst.data, "stagger"));
                else
                    pline("You %s and your vision blurs...",
                          stagger(youmonst.data, "stagger"));
            }
            make_stunned(HStun + rn1(7, 16), FALSE);
            make_hallucinated(HHallucination + rn1(5, 16), FALSE);
            break;
        default:
            impossible("bad chest trap");
            break;
        }
        bot();  /* to get immediate botl re-display */
    }

    return FALSE;
}


struct trap *
t_at(struct level *lev, int x, int y)
{
    struct trap *trap = lev->lev_traps;

    while (trap) {
        if (trap->tx == x && trap->ty == y)
            return trap;
        trap = trap->ntrap;
    }
    return NULL;
}


void
deltrap(struct level *lev, struct trap *trap)
{
    struct trap *ttmp;

    if (trap == lev->lev_traps)
        lev->lev_traps = lev->lev_traps->ntrap;
    else {
        for (ttmp = lev->lev_traps; ttmp->ntrap != trap; ttmp = ttmp->ntrap)
            ;
        ttmp->ntrap = trap->ntrap;
    }
    dealloc_trap(trap);
}

boolean
delfloortrap(struct level *lev, struct trap *ttmp)
{
    /* Destroy a trap that emanates from the floor. */
    /* some of these are arbitrary -dlc */
    if (ttmp &&
        ((ttmp->ttyp == SQKY_BOARD) || (ttmp->ttyp == BEAR_TRAP) ||
         (ttmp->ttyp == LANDMINE) || (ttmp->ttyp == FIRE_TRAP) ||
         (ttmp->ttyp == PIT) || (ttmp->ttyp == SPIKED_PIT) ||
         (ttmp->ttyp == HOLE) || (ttmp->ttyp == TRAPDOOR) ||
         (ttmp->ttyp == TELEP_TRAP) || (ttmp->ttyp == LEVEL_TELEP) ||
         (ttmp->ttyp == WEB) || (ttmp->ttyp == MAGIC_TRAP) ||
         (ttmp->ttyp == ANTI_MAGIC))) {
        struct monst *mtmp;

        if (ttmp->tx == u.ux && ttmp->ty == u.uy) {
            u.utrap = 0;
            u.utraptype = 0;
        } else if ((mtmp = m_at(lev, ttmp->tx, ttmp->ty)) != 0) {
            mtmp->mtrapped = 0;
        }
        deltrap(lev, ttmp);
        return TRUE;
    } else
        return FALSE;
}

/* used for doors (also tins).  can be used for anything else that opens. */
void
b_trapped(const char *item, int bodypart)
{
    int lvl = level_difficulty(&u.uz);
    int dmg = rnd(5 + (lvl < 5 ? lvl : 2 + lvl / 2));

    pline("KABOOM!!  %s was booby-trapped!", The(item));
    wake_nearby(FALSE);
    losehp(dmg, killer_msg(DIED, "an explosion"));
    exercise(A_STR, FALSE);
    if (bodypart)
        exercise(A_CON, FALSE);
    make_stunned(HStun + dmg, TRUE);
}

/* Monster is hit by trap. */
/* Note: doesn't work if both obj and d_override are null */
static boolean
thitm(int tlev, struct monst *mon, struct obj *obj, int d_override,
      boolean nocorpse)
{
    int strike;
    boolean trapkilled = FALSE;

    if (d_override)
        strike = 1;
    else if (obj)
        strike = (find_mac(mon) + tlev + obj->spe <= rnd(20));
    else
        strike = (find_mac(mon) + tlev <= rnd(20));

    /* Actually more accurate than thitu, which doesn't take obj->spe into
       account. */
    if (!strike) {
        if (obj && cansee(mon->mx, mon->my))
            pline("%s is almost hit by %s!", Monnam(mon), doname(obj));
    } else {
        int dam = 1;

        if (obj && cansee(mon->mx, mon->my))
            pline("%s is hit by %s!", Monnam(mon), doname(obj));
        if (d_override)
            dam = d_override;
        else if (obj) {
            dam = dmgval(obj, mon);
            if (dam < 1)
                dam = 1;
        }
        if ((mon->mhp -= dam) <= 0) {
            int xx = mon->mx;
            int yy = mon->my;

            monkilled(mon, "", nocorpse ? -AD_RBRE : AD_PHYS);
            if (mon->mhp <= 0) {
                newsym(xx, yy);
                trapkilled = TRUE;
            }
        }
    }
    if (obj && (!strike || d_override)) {
        place_object(obj, level, mon->mx, mon->my);
        stackobj(obj);
    } else if (obj)
        dealloc_obj(obj);

    return trapkilled;
}

static const char lava_killer[] = "molten lava";

boolean
lava_effects(void)
{
    struct obj *obj, *obj2;
    int dmg;

    burn_away_slime();
    if (likes_lava(youmonst.data))
        return FALSE;

    /* Check whether we should burn away boots *first* so we know whether to
       make the player sink into the lava.  Assumption: water walking only comes
       from boots. */
    if (Wwalking && uarmf && is_organic(uarmf) && !uarmf->oerodeproof) {
        /* save uarmf value because setequip sets uarmf to null */
        obj = uarmf;
        pline("Your %s into flame!", aobjnam(obj, "burst"));
        setequip(os_armf, NULL, em_silent);
        useupall(obj);
    }

    if (!Fire_resistance) {
        if (Wwalking) {
            dmg = dice(6, 6);
            pline("The lava here burns you!");
            if (dmg < u.uhp) {
                losehp(dmg, killer_msg(DIED, lava_killer));
                goto burn_stuff;
            }
        } else
            pline("You fall into the lava!");

        for (obj = invent; obj; obj = obj2) {
            obj2 = obj->nobj;
            /* 3.4.3 doesn't have a uskin check here. It's not clear what to do
               about embedded scales, but just leaving them alone is simplest,
               and this code may be unreachable for a draconic player anyway. */
            if (is_organic(obj) && !obj->oerodeproof && obj != uskin()) {
                if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                    if (!Blind)
                        pline("%s glows a strange %s, but remains intact.",
                              The(xname(obj)), hcolor("dark red"));
                    continue;
                }
                if (obj->owornmask) {
                    pline("Your %s into flame!", aobjnam(obj, "burst"));

                    unwield_silently(obj);
                    setunequip(obj);
                }
                useupall(obj);
            }
        }

        /* s/he died... */
        u.uhp = -1;
        pline("You burn to a crisp...");
        done(BURNING, killer_msg(BURNING, lava_killer));
        while (!safe_teleds(TRUE)) {
            pline("You're still burning.");
            done(BURNING, killer_msg(BURNING, lava_killer));
        }
        pline("You find yourself back on solid %s.", surface(u.ux, u.uy));
        return TRUE;
    } else if (!Wwalking && (!u.utrap || u.utraptype != TT_LAVA)) {
        u.utrap = rn1(4, 4) + (rn1(4, 12) << 8);
        u.utraptype = TT_LAVA;
        pline("You sink into the lava, but it only burns slightly!");
        if (u.uhp > 1)
            losehp(1, killer_msg(DIED, lava_killer));
    }

burn_stuff:
    destroy_item(SCROLL_CLASS, AD_FIRE);
    destroy_item(SPBOOK_CLASS, AD_FIRE);
    destroy_item(POTION_CLASS, AD_FIRE);
    return FALSE;
}

/*trap.c*/

