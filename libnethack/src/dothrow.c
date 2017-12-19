/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* Contains code for 't' (throw) */

#include "hack.h"

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
            pline(msgc_cancelled, "You now have no ammunition readied.");
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
        pline(msgc_controlhelp, "%s must be wielded before it can be thrown.",
              The(xname(obj)));
        return 0;
    }
    if ((obj->oartifact == ART_MJOLLNIR && ACURR(A_STR) < 25)
        || (obj->otyp == BOULDER && !throws_rocks(youmonst.data))) {
        pline(msgc_cancelled1, "It's too heavy.");
        return 1;
    }
    if (!dx && !dy && !dz) {
        pline(msgc_cancelled, "You cannot throw an object at yourself.");
        return 0;
    }
    u_wipe_engr(2);
    if (!uarmg && !Stone_resistance &&
        (obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm]))) {
        pline(msgc_badidea, "You throw the %s corpse with your bare %s.",
              opm_name(obj), body_part(HAND));
        instapetrify(killer_msg(STONING,
            msgprintf("throwing %s corpse without gloves",
                      an(opm_name(obj)))));
    }
    if (welded(obj)) {
        weldmsg(msgc_cancelled1, obj);
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
        pline(msgc_actionboring, "You %s %d %s.", m_shot.s ? "shoot" : "throw",
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
            obj = NULL;
        }
        freeinv(otmp);
        throwit(otmp, obj, wep_mask, twoweap, dx, dy, dz);
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
        pline(msgc_cancelled,
              "You are physically incapable of throwing anything.");
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
    for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj) {
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
    struct musable m = arg_to_musable(arg);

    if (u.spellquiver) {
        int sp_no;
        for (sp_no = 0;
             sp_no < MAXSPELL && spl_book[sp_no].sp_id != u.spellquiver;
             sp_no++)
            ;

        if (sp_no < MAXSPELL && spl_book[sp_no].sp_id == u.spellquiver &&
            spellknow(sp_no)) {
            m.spell = u.spellquiver;
            return spelleffects(FALSE, &m);
        }
    }

    /* Might have forgotten the spell, silently unready it */
    u.spellquiver = 0;

    if (notake(youmonst.data)) {
        pline(msgc_cancelled, "You are physically incapable of doing that.");
        return 0;
    }

    /* If we lack a quiver but are wielding a polearm, auto-apply it
       appropriately. */
    if (!uquiver && uwep && is_pole(uwep))
        return use_pole(uwep, arg);
    else if (flags.autoswap && !uquiver && uswapwep && is_pole(uswapwep) &&
             (!uswapwep->cursed || !uswapwep->bknown))
        return use_pole(uswapwep, arg);

    if (check_capacity(NULL))
        return 0;

    /* It can be reasonable to fire rocks without a sling. Otherwise,
       force the usage of 't' for ammo that needs a launcher when
       the proper launcher isn't wielded. */
    if (uquiver && is_ammo(uquiver) && uquiver->oclass != GEM_CLASS &&
        !ammo_and_launcher(uquiver, uwep)) {
        /* First however, check if we have the proper launcher in offhand and neither
           is (knowingly) cursed. */
        if (flags.autoswap && !u.twoweap && uswapwep &&
            ammo_and_launcher(uquiver, uswapwep) &&
            (!uwep || !uwep->cursed || !uwep->bknown) &&
            (!uswapwep->cursed || !uswapwep->bknown)) {
            if (uwep && uwep->cursed) {
                weldmsg(msgc_cancelled1, uwep);
                return 1;
            }

            int wtstatus = wield_tool(uswapwep, "preparing to wield something",
                                      occ_prepare, TRUE);
            if (wtstatus & 2)
                return 1;
            if (!(wtstatus & 1))
                return 0;
        } else {
            /* This is useless, but is here because players expect a direction prompt
               after firing, so avoid them taking a step they don't want to. */
            schar dx, dy, dz;
            getargdir(arg, NULL, &dx, &dy, &dz);

            pline(msgc_cancelled, "You aren't wielding the appropriate launcher.");
            pline(msgc_controlhelp, "(Use the 'throw' command to fire anyway.)");
            return 0;
        }
    }

    if (!uquiver) {
        if (!flags.autoquiver) {
            /* Don't automatically fill the quiver */
            pline_implied(msgc_cancelled, "You have no ammunition readied!");
            dowieldquiver(&(struct nh_cmd_arg){.argtype = 0});
            if (u.spellquiver)
                return dofire(arg);

            /* Allow this quiver to be unset if the throw is cancelled, so
               vi-keys players don't have to do it manually after typo-ing an
               object when entering a firing direction. */
            cancel_unquivers = TRUE;
            if (!uquiver)
                return dothrow(arg);
        } else
            autoquiver();
        if (!uquiver) {
            pline_implied(msgc_cancelled,
                          "You have nothing appropriate for your quiver!");
            return dothrow(arg);
        } else {
            pline(msgc_actionboring, "You fill your quiver:");
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
        doaltarobj(&youmonst, obj);
    else /* easy to do this by mistake, use a moderately warny msgc */
        pline(msgc_badidea, "%s hit%s the %s.", Doname2(obj),
              (obj->quan == 1L) ? "s" : "", surface(u.ux, u.uy));

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
        /* potential side effects before cancelling, so this isn't
           msgc_cancelled1 */
        pline(msgc_yafm, "You feel the spirits holding you back.");
        return FALSE;
    } else if (!in_out_region(level, x, y)) {
        return FALSE;
    } else if (*range == 0) {
        return FALSE;   /* previous step wants to stop now */
    }

    if (!Passes_walls || !(may_pass = may_passwall(level, x, y))) {
        if (IS_ROCK(level->locations[x][y].typ) || closed_door(level, x, y)) {
            const char *s;

            pline(msgc_badidea, "Ouch!");
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
            pline(msgc_badidea, "You crash into some iron bars.  Ouch!");
            losehp(rnd(2 + *range), killer_msg(DIED, "crashing into iron bars"));
            return FALSE;
        }
        if ((obj = sobj_at(BOULDER, level, x, y)) != 0) {
            pline(msgc_badidea, "You bump into a %s.  Ouch!", xname(obj));
            losehp(rnd(2 + *range), killer_msg(DIED, "bumping into a boulder"));
            return FALSE;
        }
        if (!may_pass) {
            /* did we hit a no-dig non-wall position? */
            pline(msgc_badidea, "You smack into something!");
            losehp(rnd(2 + *range),
                   killer_msg(DIED, "touching the edge of the universe"));
            return FALSE;
        }
        if ((u.ux - x) && (u.uy - y) && bad_rock(&youmonst, u.ux, y) &&
            bad_rock(&youmonst, x, u.uy)) {
            boolean too_much = (youmonst.minvent &&
                                (inv_weight_total() > 600));

            /* Move at a diagonal. */
            if (bigmonst(youmonst.data) || too_much) {
                pline(msgc_badidea,
                      "You %sget forcefully wedged into a crevice.",
                      too_much ? "and all your belongings " : "");
                losehp(rnd(2 + *range),
                       killer_msg(DIED, "wedging into a narrow crevice"));
                return FALSE;
            }
        }
    }

    if ((mon = m_at(level, x, y)) != 0) {
        pline(msgc_badidea, "You bump into %s.", a_monnam(mon));
        wakeup(mon, TRUE);
        return FALSE;
    }
    if ((u.ux - x) && (u.uy - y) && bad_rock(&youmonst, u.ux, y) &&
        bad_rock(&youmonst, x, u.uy)) {
        /* Move at a diagonal. */
        if (In_sokoban(&u.uz)) {
            pline(msgc_cancelled1, "You come to an abrupt halt!");
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
            pline(msgc_discoverportal, "The ground vibrates as you pass it.");
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
                pline(msgc_nonmongood, "You avoid triggering %s.",
                        an(trapexplain[ttmp->ttyp - 1]));
            else
                pline(msgc_nonmongood, "You pass right over %s.",
                      an(trapexplain[ttmp->ttyp - 1]));
        }
    }
    if (--*range < 0)   /* make sure our range never goes negative */
        *range = 0;
    if (*range != 0)
        win_delay_output();
    return TRUE;
}

boolean
mhurtle_step_ok(void *arg, int x, int y)
{
    struct monst *mon = arg;
    if (!goodpos(level, x, y, mon, 0) || !m_in_out_region(mon, x, y))
        return FALSE;
    return TRUE;
}

boolean
mhurtle_step(void *arg, int x, int y)
{
    if (!mhurtle_step_ok(arg, x, y))
        return FALSE;
    struct monst *mon = arg;

    /* TODO: Treat walls, doors, iron bars, pools, lava, etc. specially rather
       than just stopping before. */
    int ox = mon->mx;
    int oy = mon->my;
    remove_monster(level, mon->mx, mon->my);
    newsym(mon->mx, mon->my);
    place_monster(mon, x, y, TRUE);
    newsym(x, y);
    set_apparxy(mon);
    flush_screen();
    if (cansee(ox, oy) && cansee(x, y))
        win_delay_output();
    return TRUE;
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
        pline(msgc_yafm, "You feel a tug from the iron ball.");
        action_completed();
        return;
    } else if (u.utrap) {
        pline(msgc_yafm, "You are anchored by the %s.",
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
    /* Note: 3.4.3 had a flags.verbose check here; it's been removed because
       it's out of character with other uses of verbose */
    pline(msgc_substitute, "You %s in the opposite direction.",
          range > 1 ? "hurtle" : "float");
    /* if we're in the midst of shooting multiple projectiles, stop */
    if (m_shot.i < m_shot.n) {
        /* last message before hurtling was "you shoot N arrows" */
        pline_implied(msgc_consequence, "You stop %sing after the first %s.",
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
    mintrap(mon);
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
            pline(msgc_actionok, "%s hits the %s.", Doname2(obj),
                  ceiling(u.ux, u.uy));
            breakmsg(obj, !Blind);
            breakobj(obj, u.ux, u.uy, TRUE, TRUE);
            return FALSE;
        }
        almost = "";
    } else {
        almost = " almost";
    }
    pline(msgc_badidea, "%s%s hits the %s, then falls back on top of your %s.",
          Doname2(obj), almost, ceiling(u.ux, u.uy), body_part(HEAD));

    /* object now hits you */

    if (obj->oclass == POTION_CLASS) {
        potionhit(&youmonst, obj, &youmonst);
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
            pline(msgc_statusbad, "You've got it all over your %s!",
                  body_part(FACE));
            if (blindinc) {
                if (otyp == BLINDING_VENOM && !Blind)
                    pline_implied(msgc_statusbad, "It blinds you!");
                u.ucreamed += blindinc;
                if (!blind(&youmonst))
                    pline(msgc_statusheal, "Your vision quickly clears.");
                else if (flags.verbose)
                    pline(msgc_controlhelp,
                          "Use the command #wipe to clean your %s.",
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

        if (obj->oartifact || obj->oprops)
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
            dmg += dambon(&youmonst);
        if (dmg < 0)
            dmg = 0;    /* beware negative rings of increase damage */
        if (Half_physical_damage)
            dmg = (dmg + 1) / 2;

        if (uarmh) {
            if (less_damage && dmg < (Upolyd ? u.mh : u.uhp)) {
                if (!artimsg)
                    pline(msgc_playerimmune,
                          "Fortunately, you are wearing a hard helmet.");
            } else if (flags.verbose &&
                       !(obj->otyp == CORPSE &&
                         touch_petrifies(&mons[obj->corpsenm])))
                pline(msgc_notresisted,
                      "Your %s does not protect you.", xname(uarmh));
        } else if (obj->otyp == CORPSE && touched_monster(obj->corpsenm)) {
        petrify:
            /* "what goes up..." */
            pline(msgc_fatal_predone, "You turn to stone.");
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

/*
 *  Called for the following distance effects:
 *      when a weapon is thrown (weapon == THROWN_WEAPON)
 *      when an object is kicked (KICKED_WEAPON)
 *  A thrown/kicked object falls down at the end of its range or when a monster
 *  is hit.  The variable 'bhitpos' is set to the final position of the weapon
 *  thrown/kicked.
 *
 *  Check !Engulfed before calling fire_obj().
 *  This function reveals the absence of a remembered invisible monster in
 *  necessary cases (throwing or kicking weapons).  The presence of a real
 *  one is revealed for a weapon, but if not a weapon is left up to fhitm().
 */
struct monst *
fire_obj(int ddx, int ddy, int range,   /* direction and range */
         int weapon,    /* see values in hack.h */
         /* fns called when mon/obj hit */
         struct obj *obj,    /* object tossed/used */
         boolean * obj_destroyed /* has object been deallocated? may be NULL */
    )
{
    struct monst *mtmp;
    struct tmp_sym *tsym = NULL;
    uchar typ;
    boolean point_blank = TRUE;

    if (obj_destroyed)
        *obj_destroyed = FALSE;

    if (weapon == KICKED_WEAPON) {
        /* object starts one square in front of player */
        bhitpos.x = u.ux + ddx;
        bhitpos.y = u.uy + ddy;
        range--;
    } else {
        bhitpos.x = u.ux;
        bhitpos.y = u.uy;
    }

    tsym = tmpsym_initobj(obj);
    while (range-- > 0) {
        int x, y;

        bhitpos.x += ddx;
        bhitpos.y += ddy;
        x = bhitpos.x;
        y = bhitpos.y;

        if (!isok(x, y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        if (is_pick(obj) && inside_shop(level, x, y) &&
            (mtmp = shkcatch(obj, x, y))) {
            tmpsym_end(tsym);
            return mtmp;
        }

        typ = level->locations[bhitpos.x][bhitpos.y].typ;

        /* iron bars will block anything big enough */
        if ((weapon == THROWN_WEAPON || weapon == KICKED_WEAPON) &&
            typ == IRONBARS &&
            hits_bars(&obj, x - ddx, y - ddy, point_blank ? 0 : !rn2(5), 1)) {
            /* caveat: obj might now be null... */
            if (obj == NULL && obj_destroyed)
                *obj_destroyed = TRUE;
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        if ((mtmp = m_at(level, bhitpos.x, bhitpos.y))) {
            notonhead = (bhitpos.x != mtmp->mx || bhitpos.y != mtmp->my);
            tmpsym_end(tsym);
            if (cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp))
                map_invisible(bhitpos.x, bhitpos.y);
            return mtmp;
        }
        if (weapon == KICKED_WEAPON &&
            ((obj->oclass == COIN_CLASS && OBJ_AT(bhitpos.x, bhitpos.y)) ||
             ship_object(obj, bhitpos.x, bhitpos.y,
                         costly_spot(bhitpos.x, bhitpos.y)))) {
            tmpsym_end(tsym);
            return NULL;
        }
        if (!ZAP_POS(typ) || closed_door(level, bhitpos.x, bhitpos.y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        /* 'I' present but no monster: erase */
        /* do this before the tmpsym_at() */
        if (level->locations[bhitpos.x][bhitpos.y].mem_invis &&
            cansee(x, y)) {
            level->locations[bhitpos.x][bhitpos.y].mem_invis = FALSE;
            newsym(x, y);
        }
        tmpsym_at(tsym, bhitpos.x, bhitpos.y);
        win_delay_output();
        /* kicked objects fall in pools */
        if ((weapon == KICKED_WEAPON) &&
            (is_pool(level, bhitpos.x, bhitpos.y) ||
             is_lava(level, bhitpos.x, bhitpos.y)))
            break;
        if (IS_SINK(typ))
            break;  /* physical objects fall onto sink */

        /* limit range of ball so hero won't make an invalid move */
        if (weapon == THROWN_WEAPON && range > 0 &&
            obj->otyp == HEAVY_IRON_BALL) {
            struct obj *bobj;
            struct trap *t;

            if ((bobj = sobj_at(BOULDER, level, x, y)) != 0) {
                if (cansee(x, y))
                    pline(msgc_yafm, "%s hits %s.",
                          The(distant_name(obj, xname)), an(xname(bobj)));
                range = 0;
            } else if (obj == uball) {
                struct test_move_cache cache;
                init_test_move_cache(&cache);
                if (!test_move(x - ddx, y - ddy, ddx, ddy, 0, TEST_MOVE,
                               &cache)) {
                    /* nb: it didn't hit anything directly */
                    if (cansee(x, y))
                        pline(msgc_yafm, "%s jerks to an abrupt halt.",
                              The(distant_name(obj, xname))); /* lame */
                    range = 0;
                } else if (In_sokoban(&u.uz) && (t = t_at(level, x, y)) != 0 &&
                           (t->ttyp == PIT || t->ttyp == SPIKED_PIT ||
                            t->ttyp == HOLE || t->ttyp == TRAPDOOR)) {
                    /* hero falls into the trap, so ball stops */
                    range = 0;
                }
            }
        }

        /* thrown/kicked missile has moved away from its starting spot */
        point_blank = FALSE;    /* affects passing through iron bars */
    }
    tmpsym_end(tsym);
    return NULL;
}

struct monst *
boomhit(int dx, int dy)
{
    int i, ct;
    int boom = E_boomleft;      /* showsym[] index */
    struct monst *mtmp;
    struct tmp_sym *tsym;

    bhitpos.x = u.ux;
    bhitpos.y = u.uy;

    for (i = 0; i < 8; i++)
        if (xdir[i] == dx && ydir[i] == dy)
            break;
    tsym = tmpsym_init(DISP_FLASH, dbuf_effect(E_MISC, boom));
    for (ct = 0; ct < 10; ct++) {
        if (i == 8)
            i = 0;
        boom = (boom == E_boomleft) ? E_boomright : E_boomleft;
        tmpsym_change(tsym, dbuf_effect(E_MISC, boom)); /* change glyph */
        dx = xdir[i];
        dy = ydir[i];
        bhitpos.x += dx;
        bhitpos.y += dy;
        if (MON_AT(level, bhitpos.x, bhitpos.y)) {
            mtmp = m_at(level, bhitpos.x, bhitpos.y);
            m_respond(mtmp);
            tmpsym_end(tsym);
            return mtmp;
        }
        if (!ZAP_POS(level->locations[bhitpos.x][bhitpos.y].typ) ||
            closed_door(level, bhitpos.x, bhitpos.y)) {
            bhitpos.x -= dx;
            bhitpos.y -= dy;
            break;
        }
        if (bhitpos.x == u.ux && bhitpos.y == u.uy) {   /* ct == 9 */
            if (Fumbling || rn2(20) >= ACURR(A_DEX)) {
                /* we hit ourselves */
                thitu(10, rnd(10), NULL, "boomerang");
                break;
            } else {    /* we catch it */
                tmpsym_end(tsym);
                pline(msgc_actionok, "You skillfully catch the boomerang.");
                return &youmonst;
            }
        }
        tmpsym_at(tsym, bhitpos.x, bhitpos.y);
        win_delay_output();
        if (ct % 5 != 0)
            i++;
        if (IS_SINK(level->locations[bhitpos.x][bhitpos.y].typ))
            break;      /* boomerang falls on sink */
    }
    tmpsym_end(tsym);   /* do not leave last symbol */
    return NULL;
}

void
throwit(struct obj *obj, struct obj *stack,
        long wep_mask, /* used to re-equip returning boomerang 
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
            pline(msgc_substitute, "%s!", Tobjnam(obj, "misfire"));
        else {
            /* only slip if it's greased or meant to be thrown */
            if (obj->greased || throwing_weapon(obj))
                /* TODO: this message is grammatically incorrect if obj has a
                   plural name; greased gloves or boots for instance. */
                pline(msgc_substitute,
                      "%s as you throw it!", Tobjnam(obj, "slip"));
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
        pline(msgc_substitute,
              "You have so little stamina, %s drops from your grasp.",
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
            pline(msgc_yafm, "%s the %s and returns to your hand!",
                  Tobjnam(obj, "hit"), ceiling(u.ux, u.uy));
            obj = pickinv(obj);
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
            obj = pickinv(obj);
            if (wep_mask && !(obj->owornmask & wep_mask)) {
                setworn(obj, wep_mask);
                u.twoweap = twoweap;
            }
            thrownobj = NULL;
            return;
        }
    } else {
        boolean obj_destroyed;

        urange = (int)(ACURR(A_STR)) / 2;
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
        mon = fire_obj(dx, dy, range, THROWN_WEAPON, obj, &obj_destroyed);

        /* have to do this after fire_obj() so u.ux & u.uy are correct */
        if (Is_airlevel(&u.uz) || Levitation)
            hurtle(-dx, -dy, urange, TRUE);

        if (obj_destroyed)
            return;
    }

    if (mon) {
        boolean obj_gone;

        if (mx_eshk(mon) && obj->where == OBJ_MINVENT && obj->ocarry == mon) {
            thrownobj = NULL;
            return;     /* alert shk caught it */
        }
        snuff_candle(obj);
        notonhead = (bhitpos.x != mon->mx || bhitpos.y != mon->my);
        obj_gone = thitmonst(mon, obj, stack);
        /* Monster may have been tamed; this frees old mon */
        mon = m_at(level, bhitpos.x, bhitpos.y);

        /* [perhaps this should be moved into thitmonst or hmon] */
        if (mon && mx_eshk(mon) &&
            (!inside_shop(level, u.ux, u.uy) ||
             !strchr(in_rooms(level, mon->mx, mon->my, SHOPBASE), *u.ushops)))
            hot_pursuit(mon);

        if (obj_gone)
            return;
    }

    if (Engulfed) {
        /* ball is not picked up by monster */
        if (obj != uball)
            mpickobj(u.ustuck, obj, NULL);
    } else {
        /* the code following might become part of dropy() */
        if (obj->oartifact == ART_MJOLLNIR && Role_if(PM_VALKYRIE) &&
            rn2_on_rng(100, rng_mjollnir_return)) {
            /* we must be wearing Gauntlets of Power to get here */
            sho_obj_return_to_u(obj, dx, dy);   /* display its flight */

            int dmg = rn2_on_rng(2, rng_mjollnir_return);

            if (rn2_on_rng(100, rng_mjollnir_return) && !impaired) {
                pline(msgc_actionok, "%s to your hand!",
                      Tobjnam(obj, "return"));
                obj = pickinv(obj);
                setuwep(obj);
                u.twoweap = twoweap;
                if (cansee(bhitpos.x, bhitpos.y))
                    newsym(bhitpos.x, bhitpos.y);
            } else {
                if (!dmg) {
                    pline(msgc_substitute, Blind ? "%s lands %s your %s." :
                          "%s back to you, landing %s your %s.",
                          Blind ? "Something" : Tobjnam(obj, "return"),
                          Levitation ? "beneath" : "at",
                          makeplural(body_part(FOOT)));
                } else {
                    dmg += rnd(3);
                    pline(msgc_substitute, Blind ? "%s your %s!" :
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
        if (mon && mx_eshk(mon) && is_pick(obj)) {
            if (cansee(bhitpos.x, bhitpos.y))
                pline(msgc_substitute, "%s snatches up %s.", Monnam(mon),
                      the(xname(obj)));
            if (*u.ushops)
                check_shop_obj(obj, bhitpos.x, bhitpos.y, FALSE);
            mpickobj(mon, obj, NULL); /* may merge and free obj */
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
        if (mon_notices && mon->data->mmove && !rn2(10) &&
            !(property_timeout(mon, STONED) <= 3)) {
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
       "the arrow misses it," or worse, "the arrow misses the mimic." An
       attentive player will still notice that this is different from an arrow
       just landing short of any target (no message in that case), so will
       realize that there is a valid target here anyway. */
    if (!canseemon(mon))
        pline(msgc_failrandom, "%s %s.", The(missile), otense(obj, "miss"));
    else
        miss(missile, mon, &youmonst);
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
thitmonst(struct monst *mon, struct obj *obj, struct obj *stack)
{
    int tmp;    /* Base chance to hit */
    int disttmp;        /* distance modifier */
    int otyp = obj->otyp;
    boolean guaranteed_hit = (Engulfed && mon == u.ustuck);
    int dieroll = rnd(20);

    /* Differences from melee weapons: Dex still gives a bonus, but strength
       does not. Polymorphed players lacking attacks may still throw. There's a 
       base -1 to hit. No bonuses for fleeing or stunned targets (they don't
       dodge melee blows as readily, but dodging arrows is hard anyway). Not
       affected by traps, etc. Certain items which don't in themselves do
       damage ignore tmp. Distance and monster size affect chance to hit. */
    tmp =
        -1 + Luck + find_mac(mon) + hitbon(&youmonst) +
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
            pline(msgc_yafm, "%s catches and drops %s.", Monnam(mon),
                  the(xname(obj)));
            return 0;
        } else {
            /* assume this is what the player intended... */
            pline(msgc_actionok, "%s catches %s.", Monnam(mon),
                  the(xname(obj)));
            return gem_accept(mon, obj);
        }
    }

    /* don't make game unwinnable if naive player throws artifact at
       leader... */
    if (quest_arti_hits_leader(obj, mon)) {
        /* not wakeup(), which angers non-tame monsters */
        mon->msleeping = 0;
        if (idle(mon))
            mon->mstrategy = st_none;

        if (mon->mcanmove) {
            pline(msgc_actionok, "%s catches %s.", Monnam(mon),
                  the(xname(obj)));
            if (mon->mpeaceful) {
                boolean next2u = monnear(mon, u.ux, u.uy);

                finish_quest(obj);      /* acknowledge quest completion */
                pline_implied(msgc_consequence, "%s %s %s back to you.",
                              Monnam(mon), (next2u ? "hands" : "tosses"),
                              the(xname(obj)));
                if (!next2u) {
                    schar dx = sgn(mon->mx - u.ux);
                    schar dy = sgn(mon->my - u.uy);

                    sho_obj_return_to_u(obj, dx, dy);
                }
                pickinv(obj);    /* back into your inventory */
            } else {
                /* angry leader caught it and isn't returning it */
                mpickobj(mon, obj, NULL);
            }
            return 1;   /* caller doesn't need to place it */
        }
        return 0;
    } else if (mon->mtame && mon->mcanmove &&
               !is_animal(mon->data) && !mindless(mon->data) &&
               !(uwep && ammo_and_launcher(obj, uwep))) {
        /* thrown item at intelligent pet to let it use it */
        pline(msgc_actionok, "%s %s.",
              M_verbs(mon, "catch"), the(xname(obj)));
        obj->thrown_time = moves;
        obj_extract_self(obj);
        mpickobj(mon, obj, NULL);
        if (attacktype(mon->data, AT_WEAP) &&
            mon->weapon_check == NEED_WEAPON) {
            mon->weapon_check = NEED_HTH_WEAPON;
            mon_wield_item(mon);
        }

        m_dowear(mon, FALSE);
        newsym(mon->mx, mon->my);
        return 1;
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

        if (tmp >= dieroll) {
            if (hmon(mon, obj, 1, dieroll)) {
                /* mon still alive */
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

                uint64_t props = obj_properties(obj);
                if (props & opm_detonate) {
                    obj->in_use = TRUE;
                    explode(bhitpos.x, bhitpos.y,
                            ((props & opm_frost) ? AD_COLD :
                             (props & opm_shock) ? AD_ELEC :
                             AD_FIRE) - 1,
                            dice(3, 6), WEAPON_CLASS,
                            (props & (opm_frost | opm_shock)) ?
                            EXPL_FROSTY : EXPL_FIERY, NULL, 0);
                    broken = 1;
                    if (stack)
                        learn_oprop(stack, opm_detonate);
                }

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
        if (tmp >= dieroll) {
            int was_swallowed = guaranteed_hit;

            exercise(A_DEX, TRUE);
            if (!hmon(mon, obj, 1, dieroll)) {   /* mon killed */
                if (was_swallowed && !Engulfed && obj == uball)
                    return 1;   /* already did placebc() */
            }
        } else {
            tmiss(obj, mon);
        }

    } else if (otyp == BOULDER) {
        exercise(A_STR, TRUE);
        if (tmp >= dieroll) {
            exercise(A_DEX, TRUE);
            hmon(mon, obj, 1, dieroll);
        } else {
            tmiss(obj, mon);
        }

    } else if ((otyp == EGG || otyp == CREAM_PIE || otyp == BLINDING_VENOM ||
                otyp == ACID_VENOM)) {
        if ((guaranteed_hit || ACURR(A_DEX) > rnd(25))) {
            hmon(mon, obj, 1, dieroll);
            return 1;   /* hmon used it up */
        }
        tmiss(obj, mon);
        return 0;

    } else if (obj->oclass == POTION_CLASS) {
        if ((guaranteed_hit || ACURR(A_DEX) > rnd(25))) {
            potionhit(mon, obj, &youmonst);
            return 1;
        }
        tmiss(obj, mon);
        return 0;

    } else if (befriend_with_obj(mon->data, obj) ||
               (mon->mtame && dogfood(mon, obj) >= df_acceptable)) {
        if (tamedog(mon, obj))
            return 1;   /* obj is gone */
        else {
            /* not tmiss(), which angers non-tame monsters */
            miss(xname(obj), mon, &youmonst);
            mon->msleeping = 0;
            if (idle(mon))
                mon->mstrategy = st_none;
        }
    } else if (guaranteed_hit) {
        /* this assumes that guaranteed_hit is due to swallowing */
        wakeup(mon, TRUE);
        if (obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm])) {
            if (is_animal(u.ustuck->data)) {
                minstapetrify(&youmonst, u.ustuck);
                /* Don't leave a cockatrice corpse available in a statue */
                if (!Engulfed) {
                    delobj(obj);
                    return 1;
                }
            }
        }
        pline(msgc_yafm, "%s into %s %s.", Tobjnam(obj, "vanish"),
              s_suffix(mon_nam(mon)),
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
    enum msg_channel msgc = msgc_nospoil;

    msethostility(mon, FALSE, FALSE);
    mon->mavenge = 0;

    if (obj->dknown && objects[obj->otyp].oc_name_known) {
        /* object properly identified */
        if (is_gem) {
            if (is_buddy) {
                buf = msgcat(buf, addluck);
                msgc = msgc_statusgood;
                change_luck(5);
            } else {
                buf = msgcat(buf, maybeluck);
                change_luck(rn2(7) - 3);
            }
        } else {
            buf = msgcat(buf, nogood);
            msgc = msgc_yafm;
            goto nopick;
        }
    } else if (ox_name(obj) || objects[obj->otyp].oc_uname) {
        /* making guesses */
        if (is_gem) {
            if (is_buddy) {
                buf = msgcat(buf, addluck);
                msgc = msgc_statusgood;
                change_luck(2);
            } else {
                buf = msgcat(buf, maybeluck);
                change_luck(rn2(3) - 1);
            }
        } else {
            buf = msgcat(buf, nogood);
            msgc = msgc_yafm;
            goto nopick;
        }
        /* value completely unknown to @ */
    } else {
        if (is_gem) {
            if (is_buddy) {
                buf = msgcat(buf, addluck);
                msgc = msgc_statusgood;
                change_luck(1);
            } else {
                buf = msgcat(buf, maybeluck);
                change_luck(rn2(3) - 1);
            }
        } else {
            buf = msgcat(buf, noluck);
            /* technically msgc_failcurse, but that would look out of place */
            msgc = msgc_yafm;
        }
    }
    buf = msgcat(buf, acceptgift);
    if (*u.ushops)
        check_shop_obj(obj, mon->mx, mon->my, TRUE);
    mpickobj(mon, obj, NULL); /* may merge and free obj */
    ret = 1;

nopick:
    if (!Blind)
        pline(msgc, "%s", buf);
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
                        /* TODO: is this pline_implied? */
                        pline(msgc_levelsound, "You smell a peculiar odor...");
                    else {
                        int numeyes = eyecount(youmonst.data);

                        pline(msgc_levelsound, "Your %s water%s.",
                              (numeyes == 1) ? body_part(EYE) :
                              makeplural(body_part(EYE)),
                              (numeyes == 1) ? "s" : "");
                    }
                }
                potionbreathe(&youmonst, obj);
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
     /*FALLTHRU*/
    case POT_WATER:      /* really, all potions */
        if (!in_view)
            You_hear(msgc_levelsound, "something shatter!");
        else
            pline(msgc_itemloss, "%s shatter%s%s!", Doname2(obj),
                  (obj->quan == 1) ? "s" : "", to_pieces);
        break;
    case EGG:
    case MELON:
        pline(msgc_itemloss, "Splat!");
        break;
    case CREAM_PIE:
        if (in_view)
            pline(msgc_itemloss, "What a mess!");
        break;
    case ACID_VENOM:
    case BLINDING_VENOM:
        /* these items are temporary anyway, so not msgc_itemloss */
        pline(msgc_failrandom, "Splash!");
        break;
    }
}

static int
throw_gold(struct obj *obj, schar dx, schar dy, schar dz)
{
    int range, odx, ody;
    struct monst *mon;

    if (!dx && !dy && !dz) {
        pline(msgc_cancelled, "You cannot throw gold at yourself.");
        return 0;
    }
    unwield_silently(obj);
    freeinv(obj);

    if (Engulfed) {
        pline(msgc_yafm, is_animal(u.ustuck->data) ?
              "%s in the %s's entrails." : "%s into %s.",
              "The money disappears", mon_nam(u.ustuck));
        add_to_minv(u.ustuck, obj, NULL);
        return 1;
    }

    if (dz) {
        if (dz < 0 && !Is_airlevel(&u.uz) && !Underwater &&
            !Is_waterlevel(&u.uz)) {
            pline(msgc_yafm,
                  "The gold hits the %s, then falls back on top of your %s.",
                  ceiling(u.ux, u.uy), body_part(HEAD));
            /* some self damage? */
            if (uarmh)
                pline(msgc_playerimmune,
                      "Fortunately, you are wearing a %s!", helmet_name(uarmh));
        }
        bhitpos.x = u.ux;
        bhitpos.y = u.uy;
    } else {
        /* consistent with range for normal objects */
        range = (int)((ACURR(A_STR)) / 2 - obj->owt / 40);

        /* see if the gold has a place to move into */
        odx = u.ux + dx;
        ody = u.uy + dy;
        if (!ZAP_POS(level->locations[odx][ody].typ) ||
            closed_door(level, odx, ody)) {
            bhitpos.x = u.ux;
            bhitpos.y = u.uy;
        } else {
            mon = fire_obj(dx, dy, range, THROWN_WEAPON, obj, NULL);
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
        pline(msgc_actionboring, "The gold hits the %s.",
              surface(bhitpos.x, bhitpos.y));
    place_object(obj, level, bhitpos.x, bhitpos.y);
    if (*u.ushops)
        sellobj(obj, bhitpos.x, bhitpos.y);
    stackobj(obj);
    newsym(bhitpos.x, bhitpos.y);
    return 1;
}

/*dothrow.c*/

