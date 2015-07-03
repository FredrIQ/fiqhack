/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-05-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* Contains code for 't' (throw) */

#include "hack.h"
#include "edog.h"

static int throw_obj(struct obj *, const struct nh_cmd_arg *, boolean);
static void autoquiver(void);
static int gem_accept(struct monst *, struct obj *);
static void tmiss(struct obj *, struct monst *);
static int throw_gold(struct obj *, schar, schar, schar);
static void check_shop_obj(struct obj *, xchar, xchar, boolean);
static void breakobj(struct obj *, xchar, xchar, boolean, boolean);
static void breakmsg(struct obj *, boolean);
static boolean toss_up(struct obj *, boolean);
static void sho_obj_return_to_u(struct obj *obj, schar, schar);
static boolean mhurtle_step(void *, int, int);


static const char toss_objs[] =
    { ALLOW_COUNT, ALL_CLASSES, COIN_CLASS, WEAPON_CLASS, 0 };
/* different default choices when wielding a sling (gold must be included) */
static const char bullets[] =
    { ALLOW_COUNT, ALL_CLASSES, COIN_CLASS, GEM_CLASS, 0 };

struct obj *thrownobj = 0; /* tracks an object until it lands */


/* Throw the selected object, taking direction and maximum multishot from
   the provided argument */
static int
throw_obj(struct obj *obj, const struct nh_cmd_arg *arg,
          boolean cancel_unquivers)
{
    struct obj *otmp;
    int multishot = 1;
    schar skill;
    long wep_mask;
    boolean twoweap;
    schar dx, dy, dz;

    /* ask "in what direction?" if necessary */
    if (!getargdir(arg, NULL, &dx, &dy, &dz)) {
        /* obj might need to be merged back into the singular gold object */
        if (obj->owornmask == 0) {
            freeinv(obj);
            addinv(obj);
        }
        if (cancel_unquivers) {
            pline("You now have no ammunition readied.");
            setuqwep(NULL);
        }
        return 0;
    }

    /* 
       Throwing money is usually for getting rid of it when a leprechaun
       approaches, or for bribing an oncoming angry monster.  So throw the
       whole object.

       If the money is in quiver, throw one coin at a time, possibly using a
       sling. */
    if (obj->oclass == COIN_CLASS && obj != uquiver)
        return throw_gold(obj, dx, dy, dz);

    if (!canletgo(obj, "throw"))
        return 0;
    if (obj->oartifact == ART_MJOLLNIR && obj != uwep) {
        pline("%s must be wielded before it can be thrown.", The(xname(obj)));
        return 0;
    }
    if ((obj->oartifact == ART_MJOLLNIR && ACURR(A_STR) < STR19(25))
        || (obj->otyp == BOULDER && !throws_rocks(youmonst.data))) {
        pline("It's too heavy.");
        return 1;
    }
    if (!dx && !dy && !dz) {
        pline("You cannot throw an object at yourself.");
        return 0;
    }
    u_wipe_engr(2);
    if (!uarmg && !Stone_resistance &&
        (obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm]))) {
        pline("You throw the %s corpse with your bare %s.",
              mons[obj->corpsenm].mname, body_part(HAND));
        instapetrify(killer_msg(STONING,
            msgprintf("throwing %s corpse without gloves",
                      an(mons[obj->corpsenm].mname))));
    }
    if (welded(obj)) {
        weldmsg(obj);
        return 1;
    }

    /* Multishot calculations */
    skill = objects[obj->otyp].oc_skill;
    if ((ammo_and_launcher(obj, uwep) || skill == P_DAGGER || skill == -P_DART
         || skill == -P_SHURIKEN) && !(Confusion || Stunned)) {
        /* Bonus if the player is proficient in this weapon... */
        switch (P_SKILL(weapon_type(obj))) {
        default:
            break;      /* No bonus */
        case P_SKILLED:
            multishot++;
            break;
        case P_EXPERT:
            multishot += 2;
            break;
        }
        /* ...or is using a special weapon for their role... */
        switch (Role_switch) {
        case PM_RANGER:
            multishot++;
            break;
        case PM_ROGUE:
            if (skill == P_DAGGER)
                multishot++;
            break;
        case PM_SAMURAI:
            if (obj->otyp == YA && uwep && uwep->otyp == YUMI)
                multishot++;
            break;
        default:
            break;      /* No bonus */
        }
        /* ...or using their race's special bow */
        switch (Race_switch) {
        case PM_ELF:
            if (obj->otyp == ELVEN_ARROW && uwep && uwep->otyp == ELVEN_BOW)
                multishot++;
            break;
        case PM_ORC:
            if (obj->otyp == ORCISH_ARROW && uwep && uwep->otyp == ORCISH_BOW)
                multishot++;
            break;
        default:
            break;      /* No bonus */
        }
    }

    if ((long)multishot > obj->quan)
        multishot = (int)obj->quan;
    multishot = rnd(multishot);
    if (arg->argtype & CMD_ARG_LIMIT && multishot > arg->limit)
        multishot = arg->limit;
    if (multishot < 1)
        multishot = 1;

    m_shot.s = ammo_and_launcher(obj, uwep) ? TRUE : FALSE;
    /* give a message if shooting more than one, or if player attempted to
       specify a count */
    if (multishot > 1 || (arg->argtype & CMD_ARG_LIMIT)) {
        /* "You shoot N arrows." or "You throw N daggers." */
        pline("You %s %d %s.", m_shot.s ? "shoot" : "throw",
              multishot, /* (might be 1 if player gave shotlimit) */
              (multishot == 1) ? singular(obj, xname) : xname(obj));
    }

    wep_mask = obj->owornmask;
    m_shot.o = obj->otyp;
    m_shot.n = multishot;
    for (m_shot.i = 1; m_shot.i <= m_shot.n; m_shot.i++) {
        twoweap = u.twoweap;
        /* split this object off from its slot if necessary */
        if (obj->quan > 1L) {
            otmp = splitobj(obj, 1L);
        } else {
            otmp = obj;
            if (otmp->owornmask)
                remove_worn_item(otmp, FALSE);
        }
        freeinv(otmp);
        throwit(otmp, wep_mask, twoweap, dx, dy, dz);
    }
    m_shot.n = m_shot.i = 0;
    m_shot.o = STRANGE_OBJECT;
    m_shot.s = FALSE;

    return 1;
}


int
dothrow(const struct nh_cmd_arg *arg)
{
    struct obj *obj;

    if (notake(youmonst.data) || nohands(youmonst.data)) {
        pline("You are physically incapable of throwing anything.");
        return 0;
    }

    if (check_capacity(NULL))
        return 0;

    obj = getargobj(arg, uslinging() ? bullets : toss_objs, "throw");
    /* it is also possible to throw food */
    /* (or jewels, or iron balls... ) */
    if (!obj)
        return 0;

    return throw_obj(obj, arg, FALSE);
}


/* KMH -- Automatically fill quiver */
/* Suggested by Jeffrey Bay <jbay@convex.hp.com> */
static void
autoquiver(void)
{
    struct obj *otmp, *oammo = 0, *omissile = 0, *omisc = 0, *altammo = 0;

    if (uquiver)
        return;

    /* Scan through the inventory */
    for (otmp = invent; otmp; otmp = otmp->nobj) {
        if (otmp->owornmask || otmp->oartifact || !otmp->dknown) {
            ;   /* Skip it */
        } else if (otmp->otyp == ROCK ||
                   /* seen rocks or known flint or known glass */
                   (objects[otmp->otyp].oc_name_known && otmp->otyp == FLINT) ||
                   (objects[otmp->otyp].oc_name_known &&
                    otmp->oclass == GEM_CLASS &&
                    objects[otmp->otyp].oc_material == GLASS)) {
            if (uslinging())
                oammo = otmp;
            else if (ammo_and_launcher(otmp, uswapwep))
                altammo = otmp;
            else if (!omisc)
                omisc = otmp;
        } else if (otmp->oclass == GEM_CLASS) {
            ;   /* skip non-rock gems--they're ammo but player has to select
                   them explicitly */
        } else if (is_ammo(otmp)) {
            if (ammo_and_launcher(otmp, uwep))
                /* Ammo matched with launcher (bow and arrow, crossbow and
                   bolt) */
                oammo = otmp;
            else if (ammo_and_launcher(otmp, uswapwep))
                altammo = otmp;
            else
                /* Mismatched ammo (no better than an ordinary weapon) */
                omisc = otmp;
        } else if (is_missile(otmp)) {
            /* Missile (dart, shuriken, etc.) */
            omissile = otmp;
        } else if (otmp->oclass == WEAPON_CLASS && throwing_weapon(otmp)) {
            /* Ordinary weapon */
            if (objects[otmp->otyp].oc_skill == P_DAGGER && !omissile)
                omissile = otmp;
            else
                omisc = otmp;
        }
    }

    /* Pick the best choice */
    if (oammo)
        setuqwep(oammo);
    else if (omissile)
        setuqwep(omissile);
    else if (altammo)
        setuqwep(altammo);
    else if (omisc)
        setuqwep(omisc);

    return;
}


/* Throw from the quiver */
int
dofire(const struct nh_cmd_arg *arg)
{
    boolean cancel_unquivers = FALSE;

    if (notake(youmonst.data)) {
        pline("You are physically incapable of doing that.");
        return 0;
    }

    if (check_capacity(NULL))
        return 0;
    if (!uquiver) {
        if (!flags.autoquiver) {
            /* Don't automatically fill the quiver */
            pline("You have no ammunition readied!");
            dowieldquiver(&(struct nh_cmd_arg){.argtype = 0});
            /* Allow this quiver to be unset if the throw is cancelled, so
               vi-keys players don't have to do it manually after typo-ing an
               object when entering a firing direction. */
            cancel_unquivers = TRUE;
            if (!uquiver)
                return dothrow(arg);
        }
        autoquiver();
        if (!uquiver) {
            pline("You have nothing appropriate for your quiver!");
            return dothrow(arg);
        } else {
            pline("You fill your quiver:");
            prinv(NULL, uquiver, 0L);
        }
    }

    return throw_obj(uquiver, arg, cancel_unquivers);
}


/*
 * Object hits floor at hero's feet.  Called from drop() and throwit().
 */
void
hitfloor(struct obj *obj)
{
    if (IS_SOFT(level->locations[u.ux][u.uy].typ) || u.uinwater) {
        dropy(obj);
        return;
    }
    if (IS_ALTAR(level->locations[u.ux][u.uy].typ))
        doaltarobj(obj);
    else
        pline("%s hit%s the %s.", Doname2(obj), (obj->quan == 1L) ? "s" : "",
              surface(u.ux, u.uy));

    if (hero_breaks(obj, u.ux, u.uy, TRUE))
        return;
    if (ship_object(obj, u.ux, u.uy, FALSE))
        return;
    dropy(obj);
    if (!Engulfed)
        container_impact_dmg(obj);
}

/*
 * Walk a path from src_cc to dest_cc, calling a proc for each location
 * except the starting one.  If the proc returns FALSE, stop walking
 * and return FALSE.  If stopped early, dest_cc will be the location
 * before the failed callback.
 */
boolean
walk_path(coord * src_cc, coord * dest_cc,
          boolean(*check_proc) (void *, int, int), void *arg)
{
    int x, y, dx, dy, x_change, y_change, err, i, prev_x, prev_y;
    boolean keep_going = TRUE;

    /* Use Bresenham's Line Algorithm to walk from src to dest */
    dx = dest_cc->x - src_cc->x;
    dy = dest_cc->y - src_cc->y;
    prev_x = x = src_cc->x;
    prev_y = y = src_cc->y;

    if (dx < 0) {
        x_change = -1;
        dx = -dx;
    } else
        x_change = 1;
    if (dy < 0) {
        y_change = -1;
        dy = -dy;
    } else
        y_change = 1;

    i = err = 0;
    if (dx < dy) {
        while (i++ < dy) {
            prev_x = x;
            prev_y = y;
            y += y_change;
            err += dx;
            if (err >= dy) {
                x += x_change;
                err -= dy;
            }
            /* check for early exit condition */
            if (!(keep_going = (*check_proc) (arg, x, y)))
                break;
        }
    } else {
        while (i++ < dx) {
            prev_x = x;
            prev_y = y;
            x += x_change;
            err += dy;
            if (err >= dx) {
                y += y_change;
                err -= dx;
            }
            /* check for early exit condition */
            if (!(keep_going = (*check_proc) (arg, x, y)))
                break;
        }
    }

    if (keep_going)
        return TRUE;    /* successful */

    dest_cc->x = prev_x;
    dest_cc->y = prev_y;
    return FALSE;
}

/*
 * Single step for the hero flying through the air from jumping, flying,
 * etc.  Called from hurtle() and jump() via walk_path().  We expect the
 * argument to be a pointer to an integer -- the range -- which is
 * used in the calculation of points off if we hit something.
 *
 * Bumping into monsters won't cause damage but will wake them and make
 * them angry.  Auto-pickup isn't done, since you don't have control over
 * your movements at the time.
 *
 * Possible additions/changes:
 *      o really attack monster if we hit one
 *      o set stunned if we hit a wall or door
 *      o reset nomul when we stop
 *      o creepy feeling if pass through monster (if ever implemented...)
 *      o bounce off walls
 *      o let jumps go over boulders
 */
boolean
hurtle_step(void *arg, int x, int y)
{
    int ox, oy, *range = (int *)arg;
    struct obj *obj;
    struct monst *mon;
    boolean may_pass = TRUE;
    struct trap *ttmp;

    if (!isok(x, y)) {
        pline("You feel the spirits holding you back.");
        return FALSE;
    } else if (!in_out_region(level, x, y)) {
        return FALSE;
    } else if (*range == 0) {
        return FALSE;   /* previous step wants to stop now */
    }

    if (!Passes_walls || !(may_pass = may_passwall(level, x, y))) {
        if (IS_ROCK(level->locations[x][y].typ) || closed_door(level, x, y)) {
            const char *s;

            pline("Ouch!");
            if (IS_TREE(level->locations[x][y].typ))
                s = "bumping into a tree";
            else if (IS_ROCK(level->locations[x][y].typ))
                s = "bumping into a wall";
            else
                s = "bumping into a door";
            losehp(rnd(2 + *range), killer_msg(DIED, s));
            return FALSE;
        }
        if (level->locations[x][y].typ == IRONBARS) {
            pline("You crash into some iron bars.  Ouch!");
            losehp(rnd(2 + *range), killer_msg(DIED, "crashing into iron bars"));
            return FALSE;
        }
        if ((obj = sobj_at(BOULDER, level, x, y)) != 0) {
            pline("You bump into a %s.  Ouch!", xname(obj));
            losehp(rnd(2 + *range), killer_msg(DIED, "bumping into a boulder"));
            return FALSE;
        }
        if (!may_pass) {
            /* did we hit a no-dig non-wall position? */
            pline("You smack into something!");
            losehp(rnd(2 + *range),
                   killer_msg(DIED, "touching the edge of the universe"));
            return FALSE;
        }
        if ((u.ux - x) && (u.uy - y) && bad_rock(youmonst.data, u.ux, y) &&
            bad_rock(youmonst.data, x, u.uy)) {
            boolean too_much = (invent && (inv_weight() + weight_cap() > 600));

            /* Move at a diagonal. */
            if (bigmonst(youmonst.data) || too_much) {
                pline("You %sget forcefully wedged into a crevice.",
                      too_much ? "and all your belongings " : "");
                losehp(rnd(2 + *range),
                       killer_msg(DIED, "wedging into a narrow crevice"));
                return FALSE;
            }
        }
    }

    if ((mon = m_at(level, x, y)) != 0) {
        pline("You bump into %s.", a_monnam(mon));
        wakeup(mon, TRUE);
        return FALSE;
    }
    if ((u.ux - x) && (u.uy - y) && bad_rock(youmonst.data, u.ux, y) &&
        bad_rock(youmonst.data, x, u.uy)) {
        /* Move at a diagonal. */
        if (In_sokoban(&u.uz)) {
            pline("You come to an abrupt halt!");
            return FALSE;
        }
    }

    ox = u.ux;
    oy = u.uy;
    u.ux = x;
    u.uy = y;
    newsym(ox, oy);     /* update old position */
    vision_recalc(1);   /* update for new position */
    flush_screen();
    /* FIXME: Each trap should really trigger on the recoil if it would trigger 
       during normal movement. However, not all the possible side-effects of
       this are tested [as of 3.4.0] so we trigger those that we have tested,
       and offer a message for the ones that we have not yet tested. */
    if ((ttmp = t_at(level, x, y)) != 0) {
        if (ttmp->ttyp == MAGIC_PORTAL) {
            dotrap(ttmp, 0);
            return FALSE;
        } else if (ttmp->ttyp == VIBRATING_SQUARE) {
            pline("The ground vibrates as you pass it.");
            dotrap(ttmp, 0);    /* doesn't print messages */
        } else if (ttmp->ttyp == FIRE_TRAP) {
            dotrap(ttmp, 0);
        } else if ((ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT ||
                    ttmp->ttyp == HOLE || ttmp->ttyp == TRAPDOOR) &&
                   In_sokoban(&u.uz)) {
            /* Air currents overcome the recoil */
            dotrap(ttmp, 0);
            *range = 0;
            return TRUE;
        } else if (ttmp->tseen) {
            if (ttmp->ttyp == ROCKTRAP)
                pline("You avoid triggering %s.",
                        an(trapexplain[ttmp->ttyp - 1]));
            else
                pline("You pass right over %s.",
                        an(trapexplain[ttmp->ttyp - 1]));
        }
    }
    if (--*range < 0)   /* make sure our range never goes negative */
        *range = 0;
    if (*range != 0)
        win_delay_output();
    return TRUE;
}

static boolean
mhurtle_step(void *arg, int x, int y)
{
    struct monst *mon = (struct monst *)arg;

    /* TODO: Treat walls, doors, iron bars, pools, lava, etc. specially rather
       than just stopping before. */
    if (goodpos(level, x, y, mon, 0) && m_in_out_region(mon, x, y)) {
        remove_monster(level, mon->mx, mon->my);
        newsym(mon->mx, mon->my);
        place_monster(mon, x, y);
        newsym(mon->mx, mon->my);
        set_apparxy(mon);
        mintrap(mon);
        return TRUE;
    }
    return FALSE;
}

/*
 * The player moves through the air for a few squares as a result of
 * throwing or kicking something.
 *
 * dx and dy should be the direction of the hurtle, not of the original
 * kick or throw and be only.
 */
void
hurtle(int dx, int dy, int range, boolean verbose)
{
    coord uc, cc;

    /* The chain is stretched vertically, so you shouldn't be able to move very 
       far diagonally.  The premise that you should be able to move one spot
       leads to calculations that allow you to only move one spot away from the 
       ball, if you are levitating over the ball, or one spot towards the ball, 
       if you are at the end of the chain.  Rather than bother with all of
       that, assume that there is no slack in the chain for diagonal movement,
       give the player a message and return. */
    if (Punished && !carried(uball)) {
        pline("You feel a tug from the iron ball.");
        action_completed();
        return;
    } else if (u.utrap) {
        pline("You are anchored by the %s.",
              u.utraptype == TT_WEB ? "web" :
              u.utraptype == TT_LAVA ? "lava" :
              u.utraptype == TT_INFLOOR ? surface(u.ux, u.uy) : "trap");
        action_completed();
        return;
    }

    /* make sure dx and dy are [-1,0,1] */
    dx = sgn(dx);
    dy = sgn(dy);

    if (!range || (!dx && !dy) || u.ustuck)
        return; /* paranoia */

    helpless(range, hr_moving, "moving through the air", NULL);
    if (verbose)
        pline("You %s in the opposite direction.",
              range > 1 ? "hurtle" : "float");
    /* if we're in the midst of shooting multiple projectiles, stop */
    if (m_shot.i < m_shot.n) {
        /* last message before hurtling was "you shoot N arrows" */
        pline("You stop %sing after the first %s.",
              m_shot.s ? "shoot" : "throw", m_shot.s ? "shot" : "toss");
        m_shot.n = m_shot.i;    /* make current shot be the last */
    }
    if (In_sokoban(&u.uz))
        change_luck(-1);        /* Sokoban guilt */
    uc.x = u.ux;
    uc.y = u.uy;
    /* this setting of cc is only correct if dx and dy are [-1,0,1] only */
    cc.x = u.ux + (dx * range);
    cc.y = u.uy + (dy * range);
    walk_path(&uc, &cc, hurtle_step, &range);
}

/* Move a monster through the air for a few squares.
 */
void
mhurtle(struct monst *mon, int dx, int dy, int range)
{
    coord mc, cc;

    /* At the very least, debilitate the monster */
    if (!mon->mfrozen)
        mon->mfrozen = 1;
    mon->mstun = 1;

    /* Is the monster stuck or too heavy to push? (very large monsters have too 
       much inertia, even floaters and flyers) */
    if (mon->data->msize >= MZ_HUGE || mon == u.ustuck || mon->mtrapped)
        return;

    /* Make sure dx and dy are [-1,0,1] */
    dx = sgn(dx);
    dy = sgn(dy);
    if (!range || (!dx && !dy))
        return; /* paranoia */

    /* Send the monster along the path */
    mc.x = mon->mx;
    mc.y = mon->my;
    cc.x = mon->mx + (dx * range);
    cc.y = mon->my + (dy * range);
    walk_path(&mc, &cc, mhurtle_step, mon);
    return;
}

static void
check_shop_obj(struct obj *obj, xchar x, xchar y, boolean broken)
{
    struct monst *shkp = shop_keeper(level, *u.ushops);

    if (!shkp)
        return;

    if (broken) {
        if (obj->unpaid) {
            stolen_value(obj, u.ux, u.uy, (boolean) shkp->mpeaceful, FALSE);
            subfrombill(obj, shkp);
        }
        obj->no_charge = 1;
        return;
    }

    if (!costly_spot(x, y) || *in_rooms(level, x, y, SHOPBASE) != *u.ushops) {
        /* thrown out of a shop or into a different shop */
        if (obj->unpaid) {
            stolen_value(obj, u.ux, u.uy, (boolean) shkp->mpeaceful, FALSE);
            subfrombill(obj, shkp);
        }
    } else {
        if (costly_spot(u.ux, u.uy) && costly_spot(x, y)) {
            if (obj->unpaid)
                subfrombill(obj, shkp);
            else if (!(x == shkp->mx && y == shkp->my))
                sellobj(obj, x, y);
        }
    }
}

/*
 * Hero tosses an object upwards with appropriate consequences.
 *
 * Returns FALSE if the object is gone.
 */
static boolean
toss_up(struct obj *obj, boolean hitsroof)
{
    const char *almost;

    /* note: obj->quan == 1 */

    if (hitsroof) {
        if (breaktest(obj)) {
            pline("%s hits the %s.", Doname2(obj), ceiling(u.ux, u.uy));
            breakmsg(obj, !Blind);
            breakobj(obj, u.ux, u.uy, TRUE, TRUE);
            return FALSE;
        }
        almost = "";
    } else {
        almost = " almost";
    }
    pline("%s%s hits the %s, then falls back on top of your %s.", Doname2(obj),
          almost, ceiling(u.ux, u.uy), body_part(HEAD));

    /* object now hits you */

    if (obj->oclass == POTION_CLASS) {
        potionhit(&youmonst, obj, TRUE);
    } else if (breaktest(obj)) {
        int otyp = obj->otyp, ocorpsenm = obj->corpsenm;
        int blindinc;

        /* need to check for blindness result prior to destroying obj */
        blindinc = (otyp == CREAM_PIE || otyp == BLINDING_VENOM) &&
            /* AT_WEAP is ok here even if attack type was AT_SPIT */
            can_blnd(&youmonst, &youmonst, AT_WEAP, obj) ? rnd(25) : 0;

        breakmsg(obj, !Blind);
        breakobj(obj, u.ux, u.uy, TRUE, TRUE);
        obj = NULL;     /* it's now gone */
        switch (otyp) {
        case EGG:
            if (!uarmh && touched_monster(ocorpsenm))
                goto petrify;
        case CREAM_PIE:
        case BLINDING_VENOM:
            pline("You've got it all over your %s!", body_part(FACE));
            if (blindinc) {
                if (otyp == BLINDING_VENOM && !Blind)
                    pline("It blinds you!");
                u.ucreamed += blindinc;
                make_blinded(Blinded + (long)blindinc, FALSE);
                if (!Blind)
                    pline("Your vision quickly clears.");
                else if (flags.verbose)
                    pline("Use the command #wipe to clean your %s.",
                          body_part(FACE));
            }
            break;
        default:
            break;
        }
        return FALSE;
    } else {    /* neither potion nor other breaking object */
        boolean less_damage = uarmh && is_metallic(uarmh), artimsg = FALSE;
        int dmg = dmgval(obj, &youmonst);

        if (obj->oartifact)
            /* need a fake die roll here; rn1(18,2) avoids 1 and 20 */
            artimsg = artifact_hit(NULL, &youmonst, obj, &dmg, rn1(18, 2));

        if (!dmg) {     /* probably wasn't a weapon; base damage on weight */
            dmg = (int)obj->owt / 100;
            if (dmg < 1)
                dmg = 1;
            else if (dmg > 6)
                dmg = 6;
            if (youmonst.data == &mons[PM_SHADE] &&
                objects[obj->otyp].oc_material != SILVER)
                dmg = 0;
        }
        if (dmg > 1 && less_damage)
            dmg = 1;
        if (dmg > 0)
            dmg += u.udaminc;
        if (dmg < 0)
            dmg = 0;    /* beware negative rings of increase damage */
        if (Half_physical_damage)
            dmg = (dmg + 1) / 2;

        if (uarmh) {
            if (less_damage && dmg < (Upolyd ? u.mh : u.uhp)) {
                if (!artimsg)
                    pline("Fortunately, you are wearing a hard helmet.");
            } else if (flags.verbose &&
                       !(obj->otyp == CORPSE &&
                         touch_petrifies(&mons[obj->corpsenm])))
                pline("Your %s does not protect you.", xname(uarmh));
        } else if (obj->otyp == CORPSE && touched_monster(obj->corpsenm)) {
        petrify:
            /* "what goes up..." */
            pline("You turn to stone.");
            if (obj)
                dropy(obj); /* bypass most of hitfloor() */
            done(STONING, killer_msg(STONING, "elementary physics"));
            return obj ? TRUE : FALSE;
        }
        hitfloor(obj);
        losehp(dmg, killer_msg(DIED, "a falling object"));
    }
    return TRUE;
}

/* return true for weapon meant to be thrown; excludes ammo */
boolean
throwing_weapon(const struct obj * obj)
{
    return (is_missile(obj) || is_spear(obj) ||
            /* daggers and knife (excludes scalpel) */
            (is_blade(obj) && !is_sword(obj) &&
             (objects[obj->otyp].oc_dir & PIERCE)) ||
            /* special cases [might want to add AXE] */
            obj->otyp == WAR_HAMMER || obj->otyp == AKLYS);
}

/* the currently thrown object is returning to you (not for boomerangs) */
static void
sho_obj_return_to_u(struct obj *obj, schar dx, schar dy)
{
    /* might already be our location (bounced off a wall) */
    if (bhitpos.x != u.ux || bhitpos.y != u.uy) {
        int x = bhitpos.x - dx;
        int y = bhitpos.y - dy;

        struct tmp_sym *tsym = tmpsym_initobj(obj);

        while (x != u.ux || y != u.uy) {
            tmpsym_at(tsym, x, y);
            win_delay_output();
            x -= dx;
            y -= dy;
        }
        tmpsym_end(tsym);
    }
}

void
throwit(struct obj *obj, long wep_mask, /* used to re-equip returning boomerang 
                                         */
        boolean twoweap,        /* used to restore twoweapon mode if wielded
                                   weapon returns */
        schar dx, schar dy, schar dz)
{
    struct monst *mon;
    int range, urange;
    boolean impaired = (Confusion || Stunned || Blind || Hallucination ||
                        Fumbling);

    obj->was_thrown = 1;
    if ((obj->cursed || obj->greased) && (dx || dy) && !rn2(7)) {
        boolean slipok = TRUE;

        if (ammo_and_launcher(obj, uwep))
            pline("%s!", Tobjnam(obj, "misfire"));
        else {
            /* only slip if it's greased or meant to be thrown */
            if (obj->greased || throwing_weapon(obj))
                /* BUG: this message is grammatically incorrect if obj has a
                   plural name; greased gloves or boots for instance. */
                pline("%s as you throw it!", Tobjnam(obj, "slip"));
            else
                slipok = FALSE;
        }
        if (slipok) {
            dx = rn2(3) - 1;
            dy = rn2(3) - 1;
            if (!dx && !dy)
                dz = 1;
            impaired = TRUE;
        }
    }

    if ((dx || dy || (dz < 1)) && calc_capacity((int)obj->owt) > SLT_ENCUMBER &&
        (Upolyd ? (u.mh < 5 && u.mh != u.mhmax)
         : (u.uhp < 10 && u.uhp != u.uhpmax)) &&
        obj->owt > (unsigned)((Upolyd ? u.mh : u.uhp) * 2) &&
        !Is_airlevel(&u.uz)) {
        pline("You have so little stamina, %s drops from your grasp.",
              the(xname(obj)));
        exercise(A_CON, FALSE);
        dx = dy = 0;
        dz = 1;
    }

    thrownobj = obj;

    if (Engulfed) {
        mon = u.ustuck;
        bhitpos.x = mon->mx;
        bhitpos.y = mon->my;
    } else if (dz) {
        if (dz < 0 && Role_if(PM_VALKYRIE) && obj->oartifact == ART_MJOLLNIR &&
            !impaired) {
            pline("%s the %s and returns to your hand!", Tobjnam(obj, "hit"),
                  ceiling(u.ux, u.uy));
            obj = addinv(obj);
            encumber_msg();
            setuwep(obj);
            u.twoweap = twoweap;
        } else if (dz < 0 && !Is_airlevel(&u.uz) && !Underwater &&
                   !Is_waterlevel(&u.uz)) {
            toss_up(obj, rn2(5));
        } else {
            hitfloor(obj);
        }
        thrownobj = NULL;
        return;

    } else if (obj->otyp == BOOMERANG && !Underwater) {
        if (Is_airlevel(&u.uz) || Levitation)
            hurtle(-dx, -dy, 1, TRUE);
        mon = boomhit(dx, dy);
        if (mon == &youmonst) { /* the thing was caught */
            exercise(A_DEX, TRUE);
            obj = addinv(obj);
            encumber_msg();
            if (wep_mask && !(obj->owornmask & wep_mask)) {
                setworn(obj, wep_mask);
                u.twoweap = twoweap;
            }
            thrownobj = NULL;
            return;
        }
    } else {
        boolean obj_destroyed;

        urange = (int)(ACURRSTR) / 2;
        /* balls are easy to throw or at least roll */
        /* also, this insures the maximum range of a ball is greater than 1, so 
           the effects from throwing attached balls are actually possible */
        if (obj->otyp == HEAVY_IRON_BALL)
            range = urange - (int)(obj->owt / 100);
        else
            range = urange - (int)(obj->owt / 40);
        if (obj == uball) {
            if (u.ustuck)
                range = 1;
            else if (range >= 5)
                range = 5;
        }
        if (range < 1)
            range = 1;

        if (is_ammo(obj)) {
            if (ammo_and_launcher(obj, uwep))
                range++;
            else if (obj->oclass != GEM_CLASS)
                range /= 2;
        }

        if (Is_airlevel(&u.uz) || Levitation) {
            /* action, reaction... */
            urange -= range;
            if (urange < 1)
                urange = 1;
            range -= urange;
            if (range < 1)
                range = 1;
        }

        if (obj->otyp == BOULDER)
            range = 20; /* you must be giant */
        else if (obj->oartifact == ART_MJOLLNIR)
            range = (range + 1) / 2;    /* it's heavy */
        else if (obj == uball && u.utrap && u.utraptype == TT_INFLOOR)
            range = 1;

        if (Underwater)
            range = 1;

        obj_destroyed = FALSE;
        mon =
            beam_hit(dx, dy, range, THROWN_WEAPON, NULL, NULL, obj,
                     &obj_destroyed);

        /* have to do this after bhit() so u.ux & u.uy are correct */
        if (Is_airlevel(&u.uz) || Levitation)
            hurtle(-dx, -dy, urange, TRUE);

        if (obj_destroyed)
            return;
    }

    if (mon) {
        boolean obj_gone;

        if (mon->isshk && obj->where == OBJ_MINVENT && obj->ocarry == mon) {
            thrownobj = NULL;
            return;     /* alert shk caught it */
        }
        snuff_candle(obj);
        notonhead = (bhitpos.x != mon->mx || bhitpos.y != mon->my);
        obj_gone = thitmonst(mon, obj);
        /* Monster may have been tamed; this frees old mon */
        mon = m_at(level, bhitpos.x, bhitpos.y);

        /* [perhaps this should be moved into thitmonst or hmon] */
        if (mon && mon->isshk &&
            (!inside_shop(level, u.ux, u.uy) ||
             !strchr(in_rooms(level, mon->mx, mon->my, SHOPBASE), *u.ushops)))
            hot_pursuit(mon);

        if (obj_gone)
            return;
    }

    if (Engulfed) {
        /* ball is not picked up by monster */
        if (obj != uball)
            mpickobj(u.ustuck, obj);
    } else {
        /* the code following might become part of dropy() */
        if (obj->oartifact == ART_MJOLLNIR && Role_if(PM_VALKYRIE) &&
            rn2_on_rng(100, rng_mjollnir_return)) {
            /* we must be wearing Gauntlets of Power to get here */
            sho_obj_return_to_u(obj, dx, dy);   /* display its flight */

            int dmg = rn2_on_rng(2, rng_mjollnir_return);

            if (rn2_on_rng(100, rng_mjollnir_return) && !impaired) {
                pline("%s to your hand!", Tobjnam(obj, "return"));
                obj = addinv(obj);
                encumber_msg();
                setuwep(obj);
                u.twoweap = twoweap;
                if (cansee(bhitpos.x, bhitpos.y))
                    newsym(bhitpos.x, bhitpos.y);
            } else {
                if (!dmg) {
                    pline(Blind ? "%s lands %s your %s." :
                          "%s back to you, landing %s your %s.",
                          Blind ? "Something" : Tobjnam(obj, "return"),
                          Levitation ? "beneath" : "at",
                          makeplural(body_part(FOOT)));
                } else {
                    dmg += rnd(3);
                    pline(Blind ? "%s your %s!" :
                          "%s back toward you, hitting your %s!",
                          Tobjnam(obj, Blind ? "hit" : "fly"),
                          body_part(ARM));
                    artifact_hit(NULL, &youmonst, obj, &dmg, 0);
                    losehp(dmg, killer_msg_obj(DIED, obj));
                }
                if (ship_object(obj, u.ux, u.uy, FALSE)) {
                    thrownobj = NULL;
                    return;
                }
                dropy(obj);
            }
            thrownobj = NULL;
            return;
        }

        if (!IS_SOFT(level->locations[bhitpos.x][bhitpos.y].typ) &&
            breaktest(obj)) {
            struct tmp_sym *tsym = tmpsym_initobj(obj);

            tmpsym_at(tsym, bhitpos.x, bhitpos.y);
            win_delay_output();
            tmpsym_end(tsym);
            breakmsg(obj, cansee(bhitpos.x, bhitpos.y));
            breakobj(obj, bhitpos.x, bhitpos.y, TRUE, TRUE);
            return;
        }
        if (flooreffects(obj, bhitpos.x, bhitpos.y, "fall"))
            return;
        obj_no_longer_held(obj);
        if (mon && mon->isshk && is_pick(obj)) {
            if (cansee(bhitpos.x, bhitpos.y))
                pline("%s snatches up %s.", Monnam(mon), the(xname(obj)));
            if (*u.ushops)
                check_shop_obj(obj, bhitpos.x, bhitpos.y, FALSE);
            mpickobj(mon, obj); /* may merge and free obj */
            thrownobj = NULL;
            return;
        }
        snuff_candle(obj);
        if (!mon && ship_object(obj, bhitpos.x, bhitpos.y, FALSE)) {
            thrownobj = NULL;
            return;
        }
        thrownobj = NULL;
        place_object(obj, level, bhitpos.x, bhitpos.y);
        if (*u.ushops && obj != uball)
            check_shop_obj(obj, bhitpos.x, bhitpos.y, FALSE);

        stackobj(obj);
        if (obj == uball)
            drop_ball(bhitpos.x, bhitpos.y, dx, dy);
        if (cansee(bhitpos.x, bhitpos.y))
            newsym(bhitpos.x, bhitpos.y);
        if (obj_sheds_light(obj))
            turnstate.vision_full_recalc = TRUE;

        /* Lead autoexplore back over thrown object if it's seen again.
           Technically the player may not see where it lands, but they could
           probably guess it anyway. */
        level->locations[bhitpos.x][bhitpos.y].mem_stepped = 0;

        if (!IS_SOFT(level->locations[bhitpos.x][bhitpos.y].typ))
            container_impact_dmg(obj);
    }
}

/* an object may hit a monster; various factors adjust the chance of hitting */
int
omon_adj(struct monst *mon, struct obj *obj, boolean mon_notices)
{
    int tmp = 0;

    /* size of target affects the chance of hitting */
    tmp += (mon->data->msize - MZ_MEDIUM);      /* -2..+5 */
    /* sleeping target is more likely to be hit */
    if (mon->msleeping) {
        tmp += 2;
        if (mon_notices)
            mon->msleeping = 0;
    }
    /* ditto for immobilized target */
    if (!mon->mcanmove || !mon->data->mmove) {
        tmp += 4;
        if (mon_notices && mon->data->mmove && !rn2(10)) {
            mon->mcanmove = 1;
            mon->mfrozen = 0;
        }
    }
    /* some objects are more likely to hit than others */
    switch (obj->otyp) {
    case HEAVY_IRON_BALL:
        if (obj != uball)
            tmp += 2;
        break;
    case BOULDER:
        tmp += 6;
        break;
    default:
        if (obj->oclass == WEAPON_CLASS || is_weptool(obj) ||
            obj->oclass == GEM_CLASS)
            tmp += hitval(obj, mon);
        break;
    }
    return tmp;
}

/* thrown object misses target monster */
static void
tmiss(struct obj *obj, struct monst *mon)
{
    const char *missile = mshot_xname(obj);

    /* If the target can't be seen or doesn't look like a valid target, avoid
       "the arrow misses it," or worse, "the arrows misses the mimic." An
       attentive player will still notice that this is different from an arrow
       just landing short of any target (no message in that case), so will
       realize that there is a valid target here anyway. */
    if (!canseemon(mon))
        pline("%s %s.", The(missile), otense(obj, "miss"));
    else
        miss(missile, mon);
    if (!rn2(3))
        wakeup(mon, TRUE);
    return;
}

#define quest_arti_hits_leader(obj,mon) \
  (obj->oartifact && is_quest_artifact(obj) && (mon->data->msound == MS_LEADER))

/*
 * Object thrown by player arrives at monster's location.
 * Return 1 if obj has disappeared or otherwise been taken care of,
 * 0 if caller must take care of it.
 */
int
thitmonst(struct monst *mon, struct obj *obj)
{
    int tmp;    /* Base chance to hit */
    int disttmp;        /* distance modifier */
    int otyp = obj->otyp;
    boolean guaranteed_hit = (Engulfed && mon == u.ustuck);

    /* Differences from melee weapons: Dex still gives a bonus, but strength
       does not. Polymorphed players lacking attacks may still throw. There's a 
       base -1 to hit. No bonuses for fleeing or stunned targets (they don't
       dodge melee blows as readily, but dodging arrows is hard anyway). Not
       affected by traps, etc. Certain items which don't in themselves do
       damage ignore tmp. Distance and monster size affect chance to hit. */
    tmp =
        -1 + Luck + find_mac(mon) + u.uhitinc +
        maybe_polyd(youmonst.data->mlevel, u.ulevel);
    if (ACURR(A_DEX) < 4)
        tmp -= 3;
    else if (ACURR(A_DEX) < 6)
        tmp -= 2;
    else if (ACURR(A_DEX) < 8)
        tmp -= 1;
    else if (ACURR(A_DEX) >= 14)
        tmp += (ACURR(A_DEX) - 14);

    /* Modify to-hit depending on distance; but keep it sane. Polearms get a
       distance penalty even when wielded; it's hard to hit at a distance. */
    disttmp = 3 - distmin(u.ux, u.uy, mon->mx, mon->my);
    if (disttmp < -4)
        disttmp = -4;
    tmp += disttmp;
    
    /* gloves are a hinderance to proper use of bows */
    if (uarmg && uwep && objects[uwep->otyp].oc_skill == P_BOW) {
        switch (uarmg->otyp) {
        case GAUNTLETS_OF_POWER:       /* metal */
            tmp -= 2;
            break;
        case GAUNTLETS_OF_FUMBLING:
            tmp -= 3;
            break;
        case LEATHER_GLOVES:
        case GAUNTLETS_OF_DEXTERITY:
            break;
        default:
            impossible("Unknown type of gloves (%d)", uarmg->otyp);
            break;
        }
    }

    tmp += omon_adj(mon, obj, TRUE);
    if (is_orc(mon->data) &&
        maybe_polyd(is_elf(youmonst.data), Race_if(PM_ELF)))
        tmp++;
    if (guaranteed_hit) {
        tmp += 1000;    /* Guaranteed hit */
    }

    if (obj->oclass == GEM_CLASS && is_unicorn(mon->data) && mon->mcanmove &&
        !mon->msleeping && !mon->mburied) {
        if (mon->mtame) {
            pline("%s catches and drops %s.", Monnam(mon), the(xname(obj)));
            return 0;
        } else {
            pline("%s catches %s.", Monnam(mon), the(xname(obj)));
            return gem_accept(mon, obj);
        }
    }

    /* don't make game unwinnable if naive player throws artifact at leader.... 
     */
    if (quest_arti_hits_leader(obj, mon)) {
        /* not wakeup(), which angers non-tame monsters */
        mon->msleeping = 0;
        mon->mstrategy &= ~STRAT_WAITMASK;

        if (mon->mcanmove) {
            pline("%s catches %s.", Monnam(mon), the(xname(obj)));
            if (mon->mpeaceful) {
                boolean next2u = monnear(mon, u.ux, u.uy);

                finish_quest(obj);      /* acknowledge quest completion */
                pline("%s %s %s back to you.", Monnam(mon),
                      (next2u ? "hands" : "tosses"), the(xname(obj)));
                if (!next2u) {
                    schar dx = sgn(mon->mx - u.ux);
                    schar dy = sgn(mon->my - u.uy);

                    sho_obj_return_to_u(obj, dx, dy);
                }
                addinv(obj);    /* back into your inventory */
                encumber_msg();
            } else {
                /* angry leader caught it and isn't returning it */
                mpickobj(mon, obj);
            }
            return 1;   /* caller doesn't need to place it */
        }
        return 0;
    }

    if (obj->oclass == WEAPON_CLASS || is_weptool(obj) ||
        obj->oclass == GEM_CLASS) {
        if (is_ammo(obj)) {
            if (!ammo_and_launcher(obj, uwep)) {
                tmp -= 4;
            } else {
                tmp += uwep->spe - greatest_erosion(uwep);
                tmp += weapon_hit_bonus(uwep);
                if (uwep->oartifact)
                    tmp += spec_abon(uwep, mon);
                /* 
                 * Elves and Samurais are highly trained w/bows,
                 * especially their own special types of bow.
                 * Polymorphing won't make you a bow expert.
                 */
                if ((Race_if(PM_ELF) || Role_if(PM_SAMURAI)) &&
                    (!Upolyd || your_race(youmonst.data)) &&
                    objects[uwep->otyp].oc_skill == P_BOW) {
                    tmp++;
                    if (Race_if(PM_ELF) && uwep->otyp == ELVEN_BOW)
                        tmp++;
                    else if (Role_if(PM_SAMURAI) && uwep->otyp == YUMI)
                        tmp++;
                }
            }
        } else {
            if (otyp == BOOMERANG)      /* arbitrary */
                tmp += 4;
            else if (throwing_weapon(obj))      /* meant to be thrown */
                tmp += 2;
            else        /* not meant to be thrown */
                tmp -= 2;
            /* we know we're dealing with a weapon or weptool handled by
               WEAPON_SKILLS once ammo objects have been excluded */
            tmp += weapon_hit_bonus(obj);
        }

        if (tmp >= rnd(20)) {
            if (hmon(mon, obj, 1)) {    /* mon still alive */
                cutworm(mon, bhitpos.x, bhitpos.y, obj);
            }
            exercise(A_DEX, TRUE);
            /* projectiles other than magic stones sometimes disappear when
               thrown */
            if (objects[otyp].oc_skill < P_NONE &&
                objects[otyp].oc_skill > -P_BOOMERANG &&
                !objects[otyp].oc_magic) {
                /* we were breaking 2/3 of everything unconditionally. we still 
                   don't want anything to survive unconditionally, but we need
                   ammo to stay around longer on average. */
                int broken, chance;

                chance = 3 + greatest_erosion(obj) - obj->spe;
                if (chance > 1)
                    broken = rn2(chance);
                else
                    broken = !rn2(4);
                if (obj->blessed && !rnl(4))
                    broken = 0;

                if (broken) {
                    if (*u.ushops)
                        check_shop_obj(obj, bhitpos.x, bhitpos.y, TRUE);
                    obfree(obj, NULL);
                    return 1;
                }
            }
            passive_obj(mon, obj, NULL);
        } else {
            tmiss(obj, mon);
        }

    } else if (otyp == HEAVY_IRON_BALL) {
        exercise(A_STR, TRUE);
        if (tmp >= rnd(20)) {
            int was_swallowed = guaranteed_hit;

            exercise(A_DEX, TRUE);
            if (!hmon(mon, obj, 1)) {   /* mon killed */
                if (was_swallowed && !Engulfed && obj == uball)
                    return 1;   /* already did placebc() */
            }
        } else {
            tmiss(obj, mon);
        }

    } else if (otyp == BOULDER) {
        exercise(A_STR, TRUE);
        if (tmp >= rnd(20)) {
            exercise(A_DEX, TRUE);
            hmon(mon, obj, 1);
        } else {
            tmiss(obj, mon);
        }

    } else if ((otyp == EGG || otyp == CREAM_PIE || otyp == BLINDING_VENOM ||
                otyp == ACID_VENOM)) {
        if ((guaranteed_hit || ACURR(A_DEX) > rnd(25))) {
            hmon(mon, obj, 1);
            return 1;   /* hmon used it up */
        }
        tmiss(obj, mon);
        return 0;

    } else if (obj->oclass == POTION_CLASS) {
        if ((guaranteed_hit || ACURR(A_DEX) > rnd(25))) {
            potionhit(mon, obj, TRUE);
            return 1;
        }
        tmiss(obj, mon);
        return 0;

    } else if (befriend_with_obj(mon->data, obj) ||
               (mon->mtame && dogfood(mon, obj) <= ACCFOOD)) {
        if (tamedog(mon, obj))
            return 1;   /* obj is gone */
        else {
            /* not tmiss(), which angers non-tame monsters */
            miss(xname(obj), mon);
            mon->msleeping = 0;
            mon->mstrategy &= ~STRAT_WAITMASK;
        }
    } else if (guaranteed_hit) {
        /* this assumes that guaranteed_hit is due to swallowing */
        wakeup(mon, TRUE);
        if (obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm])) {
            if (is_animal(u.ustuck->data)) {
                minstapetrify(u.ustuck, TRUE);
                /* Don't leave a cockatrice corpse available in a statue */
                if (!Engulfed) {
                    delobj(obj);
                    return 1;
                }
            }
        }
        pline("%s into %s %s.", Tobjnam(obj, "vanish"), s_suffix(mon_nam(mon)),
              is_animal(u.ustuck->data) ? "entrails" : "currents");
    } else {
        tmiss(obj, mon);
    }

    return 0;
}

static int
gem_accept(struct monst *mon, struct obj *obj)
{
    boolean is_buddy = sgn(mon->data->maligntyp) == sgn(u.ualign.type);
    boolean is_gem = objects[obj->otyp].oc_material == GEMSTONE;
    int ret = 0;
    static const char nogood[] = " is not interested in your junk.";
    static const char acceptgift[] = " accepts your gift.";
    static const char maybeluck[] = " hesitatingly";
    static const char noluck[] = " graciously";
    static const char addluck[] = " gratefully";
    const char *buf = Monnam(mon);

    msethostility(mon, FALSE, FALSE);
    mon->mavenge = 0;

    if (obj->dknown && objects[obj->otyp].oc_name_known) {
        /* object properly identified */
        if (is_gem) {
            if (is_buddy) {
                buf = msgcat(buf, addluck);
                change_luck(5);
            } else {
                buf = msgcat(buf, maybeluck);
                change_luck(rn2(7) - 3);
            }
        } else {
            buf = msgcat(buf, nogood);
            goto nopick;
        }
    } else if (obj->onamelth || objects[obj->otyp].oc_uname) {
        /* making guesses */
        if (is_gem) {
            if (is_buddy) {
                buf = msgcat(buf, addluck);
                change_luck(2);
            } else {
                buf = msgcat(buf, maybeluck);
                change_luck(rn2(3) - 1);
            }
        } else {
            buf = msgcat(buf, nogood);
            goto nopick;
        }
        /* value completely unknown to @ */
    } else {
        if (is_gem) {
            if (is_buddy) {
                buf = msgcat(buf, addluck);
                change_luck(1);
            } else {
                buf = msgcat(buf, maybeluck);
                change_luck(rn2(3) - 1);
            }
        } else {
            buf = msgcat(buf, noluck);
        }
    }
    buf = msgcat(buf, acceptgift);
    if (*u.ushops)
        check_shop_obj(obj, mon->mx, mon->my, TRUE);
    mpickobj(mon, obj); /* may merge and free obj */
    ret = 1;

nopick:
    if (!Blind)
        pline("%s", buf);
    if (!tele_restrict(mon))
        rloc(mon, TRUE);
    return ret;
}

/*
 * Comments about the restructuring of the old breaks() routine.
 *
 * There are now three distinct phases to object breaking:
 *     breaktest() - which makes the check/decision about whether the
 *                   object is going to break.
 *     breakmsg()  - which outputs a message about the breakage,
 *                   appropriate for that particular object. Should
 *                   only be called after a positve breaktest().
 *                   on the object and, if it going to be called,
 *                   it must be called before calling breakobj().
 *                   Calling breakmsg() is optional.
 *     breakobj()  - which actually does the breakage and the side-effects
 *                   of breaking that particular object. This should
 *                   only be called after a positive breaktest() on the
 *                   object.
 *
 * Each of the above routines is currently static to this source module.
 * There are two routines callable from outside this source module which
 * perform the routines above in the correct sequence.
 *
 *   hero_breaks() - called when an object is to be broken as a result
 *                   of something that the hero has done. (throwing it,
 *                   kicking it, etc.)
 *   breaks()      - called when an object is to be broken for some
 *                   reason other than the hero doing something to it.
 */

/*
 * The hero causes breakage of an object (throwing, dropping it, etc.)
 * Return 0 if the object didn't break, 1 if the object broke.
 */
int
hero_breaks(struct obj *obj,
            xchar x, xchar y, /* object location (ox, oy may be wrong) */
            boolean from_invent)
{       /* thrown or dropped by player; maybe on shop bill */
    boolean in_view = !Blind;

    if (!breaktest(obj))
        return 0;
    breakmsg(obj, in_view);
    breakobj(obj, x, y, TRUE, from_invent);
    return 1;
}

/*
 * The object is going to break for a reason other than the hero doing
 * something to it.
 * Return 0 if the object doesn't break, 1 if the object broke.
 */
int
breaks(struct obj *obj, xchar x, xchar y)
{       /* object location (ox, oy may not be right) */
    boolean in_view = Blind ? FALSE : cansee(x, y);

    if (!breaktest(obj))
        return 0;
    breakmsg(obj, in_view);
    breakobj(obj, x, y, FALSE, FALSE);
    return 1;
}

/*
 * Unconditionally break an object. Assumes all resistance checks
 * and break messages have been delivered prior to getting here.
 */
static void
breakobj(struct obj *obj,
         xchar x, xchar y, /* object location (ox, oy may be wrong) */
         boolean hero_caused,   /* is this the hero's fault? */
         boolean from_invent)
{
    boolean lamplit;

    /* Remove any timers attached to the object (lit potion of oil). */
    lamplit = (obj->otyp == POT_OIL && obj->lamplit);
    if (obj->timed)
        obj_stop_timers(obj);

    switch (obj->oclass == POTION_CLASS ? POT_WATER : obj->otyp) {
    case MIRROR:
        if (hero_caused)
            change_luck(-2);
        break;
    case POT_WATER:    /* really, all potions */
        if (obj->otyp == POT_OIL && lamplit) {
            splatter_burning_oil(x, y);
        } else if (distu(x, y) <= 2) {
            if (!breathless(youmonst.data) || haseyes(youmonst.data)) {
                if (obj->otyp != POT_WATER) {
                    if (!breathless(youmonst.data))
                        /* [what about "familiar odor" when known?] */
                        pline("You smell a peculiar odor...");
                    else {
                        int numeyes = eyecount(youmonst.data);

                        pline("Your %s water%s.",
                              (numeyes ==
                               1) ? body_part(EYE) : makeplural(body_part(EYE)),
                              (numeyes == 1) ? "s" : "");
                    }
                }
                potionbreathe(obj);
            }
        }
        /* monster breathing isn't handled... [yet?] */
        break;
    case EGG:
        /* breaking your own eggs is bad luck */
        if (hero_caused && obj->spe && obj->corpsenm >= LOW_PM)
            change_luck((schar) - min(obj->quan, 5L));
        break;
    }
    if (hero_caused) {
        if (from_invent) {
            if (*u.ushops)
                check_shop_obj(obj, x, y, TRUE);
        } else if (!obj->no_charge && costly_spot(x, y)) {
            /* it is assumed that the obj is a floor-object */
            char *o_shop = in_rooms(level, x, y, SHOPBASE);
            struct monst *shkp = shop_keeper(level, *o_shop);

            if (shkp) { /* (implies *o_shop != '\0') */
                static long lastmovetime = 0L;
                static boolean peaceful_shk = FALSE;

                /* We want to base shk actions on her peacefulness at start of
                   this turn, so that "simultaneous" multiple breakage isn't
                   drastically worse than single breakage.  (ought to be done
                   via ESHK) */
                if (moves != lastmovetime)
                    peaceful_shk = shkp->mpeaceful;
                if (stolen_value(obj, x, y, peaceful_shk, FALSE) > 0L &&
                    (*o_shop != u.ushops[0] || !inside_shop(level, u.ux, u.uy))
                    && moves != lastmovetime)
                    make_angry_shk(shkp, x, y);
                lastmovetime = moves;
            }
        }
    }
    delobj(obj);
}

/*
 * Check to see if obj is going to break, but don't actually break it.
 * Return 0 if the object isn't going to break, 1 if it is.
 */
boolean
breaktest(struct obj *obj)
{
    /* Venom can never resist destruction. */
    if (obj->otyp == ACID_VENOM || obj->otyp == BLINDING_VENOM)
        return 1;

    if (obj_resists(obj, 1, 99))
        return 0;

    /* All glass items break (includes all potions) */
    if ((objects[obj->otyp].oc_material == GLASS && !obj->oartifact &&
         obj->oclass != GEM_CLASS))
        return 1;

    switch (obj->otyp) {
    case EXPENSIVE_CAMERA:
    case EGG:
    case CREAM_PIE:
    case MELON:
        return 1;
    default:
        return 0;
    }
}

static void
breakmsg(struct obj *obj, boolean in_view)
{
    const char *to_pieces;

    to_pieces = "";
    switch (obj->oclass == POTION_CLASS ? POT_WATER : obj->otyp) {
    default:   /* glass or crystal wand */
        if (obj->oclass != WAND_CLASS)
            impossible("breaking odd object?");
    case CRYSTAL_PLATE_MAIL:
    case LENSES:
    case MIRROR:
    case CRYSTAL_BALL:
    case EXPENSIVE_CAMERA:
        to_pieces = " into a thousand pieces";
     /*FALLTHRU*/ case POT_WATER:      /* really, all potions */
        if (!in_view)
            You_hear("something shatter!");
        else
            pline("%s shatter%s%s!", Doname2(obj), (obj->quan == 1) ? "s" : "",
                  to_pieces);
        break;
    case EGG:
    case MELON:
        pline("Splat!");
        break;
    case CREAM_PIE:
        if (in_view)
            pline("What a mess!");
        break;
    case ACID_VENOM:
    case BLINDING_VENOM:
        pline("Splash!");
        break;
    }
}

static int
throw_gold(struct obj *obj, schar dx, schar dy, schar dz)
{
    int range, odx, ody;
    struct monst *mon;

    if (!dx && !dy && !dz) {
        pline("You cannot throw gold at yourself.");
        return 0;
    }
    unwield_silently(obj);
    freeinv(obj);

    if (Engulfed) {
        pline(is_animal(u.ustuck->data) ? "%s in the %s's entrails." :
              "%s into %s.", "The money disappears", mon_nam(u.ustuck));
        add_to_minv(u.ustuck, obj);
        return 1;
    }

    if (dz) {
        if (dz < 0 && !Is_airlevel(&u.uz) && !Underwater &&
            !Is_waterlevel(&u.uz)) {
            pline("The gold hits the %s, then falls back on top of your %s.",
                  ceiling(u.ux, u.uy), body_part(HEAD));
            /* some self damage? */
            if (uarmh)
                pline("Fortunately, you are wearing a %s!", helmet_name(uarmh));
        }
        bhitpos.x = u.ux;
        bhitpos.y = u.uy;
    } else {
        /* consistent with range for normal objects */
        range = (int)((ACURRSTR) / 2 - obj->owt / 40);

        /* see if the gold has a place to move into */
        odx = u.ux + dx;
        ody = u.uy + dy;
        if (!ZAP_POS(level->locations[odx][ody].typ) ||
            closed_door(level, odx, ody)) {
            bhitpos.x = u.ux;
            bhitpos.y = u.uy;
        } else {
            mon = beam_hit(dx, dy, range, THROWN_WEAPON, NULL, NULL, obj, NULL);
            if (mon) {
                if (ghitm(mon, obj))    /* was it caught? */
                    return 1;
            } else {
                if (ship_object(obj, bhitpos.x, bhitpos.y, FALSE))
                    return 1;
            }
        }
    }

    if (flooreffects(obj, bhitpos.x, bhitpos.y, "fall"))
        return 1;
    if (dz > 0)
        pline("The gold hits the %s.", surface(bhitpos.x, bhitpos.y));
    place_object(obj, level, bhitpos.x, bhitpos.y);
    if (*u.ushops)
        sellobj(obj, bhitpos.x, bhitpos.y);
    stackobj(obj);
    newsym(bhitpos.x, bhitpos.y);
    return 1;
}

/*dothrow.c*/

