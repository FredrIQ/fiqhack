/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* Contains code for 't' (throw) */

#include "hack.h"

static int throw_obj(struct obj *, const struct musable *, boolean);
static void autoquiver(void);
static int gem_accept(struct monst *, struct monst *, struct obj *);
static void tmiss(struct monst *, struct monst *, struct obj *, int);
static void check_shop_obj(struct obj *, xchar, xchar, boolean);
static void breakobj(struct obj *, xchar, xchar, boolean, boolean);
static void breakmsg(struct obj *, boolean);
static boolean toss_up(struct monst *, struct obj *, boolean);
static void show_obj_flight(struct obj *obj, int, int, int, int);


static const char toss_objs[] =
    { ALLOW_COUNT, ALL_CLASSES, COIN_CLASS, WEAPON_CLASS, 0 };
/* different default choices when wielding a sling (gold must be included) */
static const char bullets[] =
    { ALLOW_COUNT, ALL_CLASSES, COIN_CLASS, GEM_CLASS, 0 };

struct obj *thrownobj = 0; /* tracks an object until it lands */


/* Throw the selected object, taking direction and maximum multishot from
   the provided argument */
static int
throw_obj(struct obj *obj, const struct musable *m,
          boolean cancel_unquivers)
{
    struct monst *mon = m->mon;
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    struct obj *otmp;
    int multishot = 1;
    schar skill;
    long wep_mask;
    boolean twoweap = FALSE;
    boolean shot = FALSE;
    schar dx, dy, dz;

    /* ask "in what direction?" if necessary */
    if (!mgetargdir(m, NULL, &dx, &dy, &dz)) {
        if (!you)
            impossible("Monster was going to throw something, but changed its mind?");
        /* obj might need to be merged back into the singular gold object */
        if (!obj->owornmask) {
            obj_extract_self(obj);
            /* TODO: Figure out the differences in these and merge them
               into one */
            if (you)
                addinv(obj);
            else
                mpickobj(mon, obj, NULL);
        }
        if (cancel_unquivers && you) {
            pline(msgc_cancelled, "You now have no ammunition readied.");
            setuqwep(NULL);
        }
        return 0;
    }

    /* Throwing money is usually for getting rid of it when a leprechaun
       approaches, or for bribing an oncoming angry monster.  So throw the
       whole object.

       If the money is in quiver, throw one coin at a time, possibly using a
       sling. */
    boolean throw_all = FALSE;
    if (obj->oclass == COIN_CLASS && obj != uquiver)
        throw_all = TRUE;

    /* Monsters are assumed to always be able to throw the given object */
    if (!canletgo(obj, "throw"))
        return 0;
    if (obj->oartifact == ART_MJOLLNIR && obj != m_mwep(mon)) {
        if (vis)
            pline(msgc_controlhelp,
                  "%s must be wielded before it can be thrown.",
                  The(xname(obj)));
        if (!you)
            impossible("Monster tried to throw non-welded Mjollnir?");
        return 0;
    }
    if ((obj->oartifact == ART_MJOLLNIR && acurr(mon, A_STR) < 25) ||
        (obj->otyp == BOULDER && !throws_rocks(mon->data))) {
        if (vis)
            pline(msgc_cancelled1, "It's too heavy.");
        if (!you)
            impossible("Non-boulder thrower tried to throw a boulder?");
        return 1;
    }
    if (!dx && !dy && !dz) {
        pline(msgc_cancelled, "You cannot throw an object at yourself.");
        return 0;
    }
    u_wipe_engr(2);
    if (!which_armor(mon, os_armg) && !resists_ston(mon) &&
        (obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm]))) {
        if (vis)
            pline(you ? msgc_badidea : mon->mtame ? msgc_petfatal :
                  msgc_monneutral, "%s the %s corpse with %s bare %s.",
                  M_verbs(mon, "throw"), opm_name(obj), mhis(mon),
                  mbodypart(mon, HAND));
        if (you)
            instapetrify(killer_msg(STONING,
                                    msgprintf("throwing %s corpse without "
                                              "gloves",
                                              an(opm_name(obj)))));
        else
            minstapetrify(mon, NULL);
    }

    /* Take time if we knew it was welded only */
    boolean knew_buc = you ? obj->bknown : obj->mbknown;
    if (welded(mon, obj)) {
        if (vis)
            weldmsg(knew_buc ? msgc_cancelled : msgc_cancelled1, mon,
                    obj);
        return !knew_buc;
    }

    /* Multishot calculations */
    skill = objects[obj->otyp].oc_skill;
    if ((ammo_and_launcher(obj, m_mwep(mon)) || skill == P_DAGGER ||
         skill == -P_DART || skill == -P_SHURIKEN) &&
        !(confused(mon) || stunned(mon))) {
        /* Bonus if the player is proficient in this weapon... */
        switch (MP_SKILL(mon, weapon_type(obj))) {
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
        switch (you ? Role_switch : monsndx(mon->data)) {
        case PM_RANGER:
            multishot++;
            break;
        case PM_ROGUE:
            if (skill == P_DAGGER)
                multishot++;
            break;
        case PM_NINJA:
            if (skill == P_SHURIKEN)
                multishot++;
            /* FALLTHROUGH */
        case PM_SAMURAI:
            if (obj->otyp == YA && m_mwep(mon) &&
                m_mwep(mon)->otyp == YUMI)
                multishot++;
            break;
        default:
            break;      /* No bonus */
        }
        /* ...or using their race's special bow */
        switch (race(mon, TRUE)) {
        case PM_ELF:
            if (obj->otyp == ELVEN_ARROW && m_mwep(mon) &&
                m_mwep(mon)->otyp == ELVEN_BOW)
                multishot++;
            break;
        case PM_ORC:
            if (obj->otyp == ORCISH_ARROW && m_mwep(mon) &&
                m_mwep(mon)->otyp == ORCISH_BOW)
                multishot++;
            break;
        default:
            break;      /* No bonus */
        }
    }

    if ((long)multishot > obj->quan)
        multishot = (int)obj->quan;
    multishot = rnd(multishot);
    if (m->limit && multishot > m->limit)
        multishot = m->limit;
    if (multishot < 1)
        multishot = 1;
    if (throw_all && multishot > 1)
        multishot = 1;

    shot = ammo_and_launcher(obj, m_mwep(mon)) ? TRUE : FALSE;
    /* give a message if shooting more than one, or if player attempted to
       specify a count */
    if (multishot > 1 || m->limit) {
        /* "You shoot N arrows." or "You throw N daggers." */
        if (vis)
            pline(you ? msgc_actionok : msgc_monneutral,
                  "%s %d %s.", M_verbs(mon, shot ? "shoot" : "throw"),
                  multishot, /* (might be 1 if player gave shotlimit) */
                  (multishot == 1) ? singular(obj, xname) : xname(obj));
    } else if (!you && vis)
        pline(msgc_monneutral, "%s %s!",
              M_verbs(mon, shot ? "shoot" : "throw"),
              singular(obj, doname));

    wep_mask = obj->owornmask;
    int i;
    for (i = 1; i <= multishot; i++) {
        if (you)
            twoweap = u.twoweap;
        /* split this object off from its slot if necessary */
        if (obj->quan > 1L && !throw_all) {
            otmp = splitobj(obj, 1L);
        } else {
            otmp = obj;
            if (otmp->owornmask) {
                if (you)
                    remove_worn_item(otmp, FALSE);
                else {
                    if (otmp->owornmask & W_MASK(os_wep))
                        MON_NOWEP(mon);
                    otmp->owornmask = 0;
                }
            }
            obj = NULL; /* there's no stack anymore */
        }

        obj_extract_self(otmp);
        if (throwit(mon, otmp, obj, multishot > 1 ? i : 0, wep_mask,
                    twoweap, dx, dy, dz)) {
            if (vis && multishot)
                pline(you ? msgc_failcurse : msgc_monneutral,
                      "Hurtling through the air, %s %sing.", m_verbs(mon, "stop"),
                      shot ? "shoot" : "throw");
            break;
        }
    }

    return 1;
}


int
dothrow(const struct musable *m)
{
    struct monst *mon = m->mon;
    struct obj *obj;

    if (notake(mon->data) || nohands(mon->data)) {
        pline(msgc_cancelled,
              "%s physically incapable of throwing anything.", M_verbs(mon, "are"));
        if (mon != &youmonst)
            impossible("Monster attempting to throw something while "
                       "unable to?");
        return 0;
    }

    if (check_capacity(NULL))
        return 0;

    obj = mgetargobj(m, uslinging() ? bullets : toss_objs, "throw");
    /* it is also possible to throw food */
    /* (or jewels, or iron balls... ) */
    if (!obj)
        return 0;

    return throw_obj(obj, m, FALSE);
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
    if (uquiver && is_ammo(uquiver) &&
        (uquiver->oclass != GEM_CLASS || carrying(SLING)) &&
        !ammo_and_launcher(uquiver, uwep)) {
        /* First however, check if we have the proper launcher in offhand and neither
           is (knowingly) cursed. */
        if (flags.autoswap && !u.twoweap && uswapwep &&
            ammo_and_launcher(uquiver, uswapwep) &&
            (!uwep || !uwep->cursed || !uwep->bknown) &&
            (!uswapwep->cursed || !uswapwep->bknown)) {
            if (uwep && uwep->cursed) {
                weldmsg(msgc_cancelled1, &youmonst, uwep);
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
                return dothrow(&m);
        } else
            autoquiver();
        if (!uquiver) {
            pline_implied(msgc_cancelled,
                          "You have nothing appropriate for your quiver!");
            return dothrow(&m);
        } else {
            pline(msgc_actionboring, "You fill your quiver:");
            prinv(NULL, uquiver, 0L);
        }
    }

    return throw_obj(uquiver, &m, cancel_unquivers);
}


/* Object hits floor at mon's feet.  Called from drop() and throwit(). */
void
hitfloor(struct monst *mon, struct obj *obj)
{
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    if (IS_SOFT(level->locations[mon->mx][mon->my].typ) ||
        m_underwater(mon)) {
        dropy(obj);
        return;
    }
    if (IS_ALTAR(level->locations[mon->mx][mon->my].typ))
        doaltarobj(mon, obj);
    else if (vis)
        /* easy to do this by mistake, use a moderately warny msgc */
        pline(you ? msgc_badidea : msgc_monneutral, "%s hit%s the %s.", Doname2(obj),
              (obj->quan == 1L) ? "s" : "", surface(youmonst.mx, youmonst.my));

    if (you ? hero_breaks(obj, mon->mx, mon->my, TRUE) :
        breaks(obj, mon->mx, mon->my))
        return;
    if (ship_object(obj, mon->mx, mon->my, FALSE))
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
        if ((youmonst.mx - x) && (youmonst.my - y) &&
            bad_rock(&youmonst, youmonst.mx, y) &&
            bad_rock(&youmonst, x, youmonst.my)) {
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
    if ((youmonst.mx - x) && (youmonst.my - y) && bad_rock(&youmonst, youmonst.mx, y) &&
        bad_rock(&youmonst, x, youmonst.my)) {
        /* Move at a diagonal. */
        if (In_sokoban(&u.uz)) {
            pline(msgc_cancelled1, "You come to an abrupt halt!");
            return FALSE;
        }
    }

    ox = youmonst.mx;
    oy = youmonst.my;
    youmonst.mx = x;
    youmonst.my = y;
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
              u.utraptype == TT_INFLOOR ? surface(youmonst.mx, youmonst.my) : "trap");
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
    if (In_sokoban(&u.uz))
        change_luck(-1);        /* Sokoban guilt */
    uc.x = youmonst.mx;
    uc.y = youmonst.my;
    /* this setting of cc is only correct if dx and dy are [-1,0,1] only */
    cc.x = youmonst.mx + (dx * range);
    cc.y = youmonst.my + (dy * range);
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
            stolen_value(obj, youmonst.mx, youmonst.my, (boolean) shkp->mpeaceful, FALSE);
            subfrombill(obj, shkp);
        }
        obj->no_charge = 1;
        return;
    }

    if (!costly_spot(x, y) || *in_rooms(level, x, y, SHOPBASE) != *u.ushops) {
        /* thrown out of a shop or into a different shop */
        if (obj->unpaid) {
            stolen_value(obj, youmonst.mx, youmonst.my, (boolean) shkp->mpeaceful, FALSE);
            subfrombill(obj, shkp);
        }
    } else {
        if (costly_spot(youmonst.mx, youmonst.my) && costly_spot(x, y)) {
            if (obj->unpaid)
                subfrombill(obj, shkp);
            else if (!(x == shkp->mx && y == shkp->my))
                sellobj(obj, x, y);
        }
    }
}

/* Monster tosses an object upwards with appropriate consequences.
   Returns FALSE if the object is gone. */
static boolean
toss_up(struct monst *mon, struct obj *obj, boolean hitsroof)
{
    const char *almost;
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));
    boolean delayed_stoning = FALSE;
    struct obj *armh = which_armor(mon, os_armh);

    /* note: obj->quan == 1 */

    if (hitsroof) {
        if (breaktest(obj)) {
            if (vis)
                pline(msgc_actionok, "%s hits the %s.", Doname2(obj),
                      ceiling(mon->mx, mon->my));
            if (vis)
                breakmsg(obj, !blind(&youmonst));
            breakobj(obj, mon->mx, mon->my, TRUE, TRUE);
            return FALSE;
        }
        almost = "";
    } else
        almost = " almost";
    if (vis)
        pline(msgc_badidea, "%s%s hits the %s, then falls back on top of your %s.",
              Doname2(obj), almost, ceiling(mon->mx, mon->my), mbodypart(mon, HEAD));

    /* object now hits monster */

    if (obj->oclass == POTION_CLASS) {
        potionhit(mon, obj, mon);
    } else if (breaktest(obj)) {
        int otyp = obj->otyp, ocorpsenm = obj->corpsenm;
        int blindinc;

        /* need to check for blindness result prior to destroying obj */
        blindinc = (otyp == CREAM_PIE || otyp == BLINDING_VENOM) &&
            /* AT_WEAP is ok here even if attack type was AT_SPIT */
            can_blnd(mon, mon, AT_WEAP, obj) ? rnd(25) : 0;

        breakmsg(obj, !Blind);
        breakobj(obj, youmonst.mx, youmonst.my, TRUE, TRUE);
        obj = NULL;     /* it's now gone */
        switch (otyp) {
        case EGG:
            if (!armh && touched_monster(ocorpsenm)) {
                delayed_stoning = TRUE; /* eggs only do gradual stiffening */
                goto petrify;
            }
        case CREAM_PIE:
        case BLINDING_VENOM:
            if (vis)
                pline(you ? msgc_statusbad : msgc_monneutral,
                      "%s got it all over %s %s!", M_verbs(mon, "have"),
                      you ? "your" : mhis(mon), mbodypart(mon, FACE));
            if (blindinc) {
                if (you && otyp == BLINDING_VENOM && !blind(mon))
                    pline_implied(msgc_statusbad, "It blinds you!");
                inc_timeout(mon, CREAMED, blindinc, TRUE);
                if (you && !blind(mon))
                    pline(msgc_statusheal, "Your vision quickly clears.");
                else if (you && flags.verbose)
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
        boolean less_damage = armh && is_metallic(armh), artimsg = FALSE;
        int dmg = dmgval(obj, mon);

        if (obj->oartifact || obj->oprops)
            /* need a fake die roll here; rn1(18,2) avoids 1 and 20 */
            artimsg = artifact_hit(NULL, mon, obj, &dmg, rn1(18, 2));

        if (!dmg) {     /* probably wasn't a weapon; base damage on weight */
            dmg = (int)obj->owt / 100;
            if (dmg < 1)
                dmg = 1;
            else if (dmg > 6)
                dmg = 6;
            if (mon->data == &mons[PM_SHADE] &&
                objects[obj->otyp].oc_material != SILVER)
                dmg = 0;
        }
        if (dmg > 1 && less_damage)
            dmg = 1;
        if (dmg > 0)
            dmg += dambon(mon);
        if (dmg < 0)
            dmg = 0;    /* beware negative rings of increase damage */
        if (half_phys_dam(mon))
            dmg = (dmg + 1) / 2;

        if (armh) {
            if (less_damage && dmg < (you && Upolyd ? u.mh : you ? u.uhp : mon->mhp)) {
                if (!artimsg && vis)
                    pline(combat_msgc(mon, mon, cr_immune),
                          "Fortunately, %s wearing a hard helmet.", m_verbs(mon, "are"));
            } else if (flags.verbose &&
                       !(obj->otyp == CORPSE &&
                         touch_petrifies(&mons[obj->corpsenm])))
                pline(combat_msgc(mon, mon, cr_hit),
                      "%s %s does not protect %s.", s_suffix(Monnam(mon)), xname(armh),
                      you ? "you" : mhim(mon));
        } else if (obj->otyp == CORPSE && touched_monster(obj->corpsenm)) {
        petrify:
            /* "what goes up..." */
            if (!delayed_stoning) {
                if (obj)
                    dropy(obj); /* bypass most of hitfloor() */
                if (you)
                    instapetrify(killer_msg(STONING, "elementary physics"));
                else
                    minstapetrify(mon, NULL);
            } else {
                if (!petrifying(mon)) {
                    if (you) {
                        set_property(&youmonst, STONING, 5, TRUE);
                        set_delayed_killer(STONING, "elementary physics");
                    } else
                        mstiffen(mon, mon);
                }
            }
            return obj ? TRUE : FALSE;
        }
        hitfloor(mon, obj);
        if (you)
            losehp(dmg, killer_msg(DIED, "a falling object"));
        else {
            mon->mhp -= dmg;
            if (mon->mhp <= 0)
                mondied(mon);
        }
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

/* Shows object flight paths. Currently only used for Mjollnir returns. Cannot be used for
   fire_obj due to how hits_bars/ship_obj works, and changing those just to use this isn't
   worth it. */
static void
show_obj_flight(struct obj *obj, int fx, int fy, int tx, int ty)
{
    /* might already be our location (bounced off a wall) */
    if (fx == tx && fy == ty)
        return;

    int dx = sgn(tx - fx);
    int dy = sgn(ty - fy);
    int x = fx + dx;
    int y = fy + dy;

    /* First, determine if we can see the action happen at any point. This avoids spoilers
       from monsters throwing things (judging the delay). Might be overkill, but the check
       is simple enough (the inherent delay of this logic is small enough to not be
       noticeable, unlike win_delay_output, and the additional complexity is simple enough
       to justify it, especially since it allows error-checking without weird displays) */
    boolean see_path = FALSE;
    while (x != tx || y != ty) {
        if (!isok(x, y)) {
            impossible("show_obj_flight: source/destination doesn't line up?");
            return;
        }

        if (cansee(x, y)) {
            see_path = TRUE;
            break;
        }
        x += dx;
        y += dy;
    }
    if (!see_path)
        return; /* can't see any part of the flight */

    x = fx + dx;
    y = fy + dy;
    struct tmp_sym *tsym = tmpsym_initobj(obj);
    while (x != tx || y != ty) {
        tmpsym_at(tsym, x, y);
        win_delay_output();
        x += dx;
        y += dy;
    }

    tmpsym_end(tsym);
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
fire_obj(struct monst *magr, int dx, int dy, int range, int weapon, struct obj *obj,
         boolean *obj_destroyed, coord *coords)
{
    struct monst *mdef = NULL;
    struct tmp_sym *tsym = NULL;
    boolean you = (magr == &youmonst);
    uchar typ;
    boolean point_blank = TRUE;

    if (obj_destroyed)
        *obj_destroyed = FALSE;

    int orig_x = magr->mx;
    int orig_y = magr->my;
    if (weapon == KICKED_WEAPON) {
        orig_x += dx;
        orig_y += dy;
        range--;
    }

    int x = orig_x;
    int y = orig_y;
    boolean see_path = FALSE;
    tsym = tmpsym_initobj(obj);
    while (range-- > 0) {
        if (isok(x + dx, y + dy)) {
            x += dx;
            y += dy;
        } else
            break;

        if (is_pick(obj) && inside_shop(level, x, y) &&
            (mdef = shkcatch(obj, x, y)))
            break;

        typ = level->locations[x][y].typ;

        /* iron bars will block anything big enough */
        if ((weapon == THROWN_WEAPON || weapon == KICKED_WEAPON) &&
            typ == IRONBARS &&
            hits_bars(&obj, x - dx, y - dy, point_blank ? 0 : !rn2(5), you ? 1 : 0)) {
            x -= dx;
            y -= dy;
            /* caveat: obj might now be null... */
            if (obj == NULL && obj_destroyed)
                *obj_destroyed = TRUE;
            break;
        }

        if ((mdef = um_at(m_dlevel(magr), x, y))) {
            notonhead = (x != mdef->mx || y != mdef->my);
            if (cansee(x, y) && !canspotmon(mdef) && mdef != &youmonst)
                map_invisible(x, y);
            break;
        }
        if (weapon == KICKED_WEAPON &&
            ((obj->oclass == COIN_CLASS && OBJ_AT(x, y)) ||
             ship_object(obj, x, y,
                         you && costly_spot(x, y))))
            break;

        if (!ZAP_POS(typ) || closed_door(level, x, y)) {
            x -= dx;
            y -= dy;
            break;
        }

        /* 'I' present but no monster: erase */
        /* do this before the tmpsym_at() */
        if (level->locations[x][y].mem_invis &&
            cansee(x, y)) {
            level->locations[x][y].mem_invis = FALSE;
            newsym(x, y);
        }
        tmpsym_at(tsym, x, y);
        if (!see_path)
            see_path = cansee(x, y);
        if (see_path) /* only delay if we saw part of the path (avoids mvm delays/etc) */
            win_delay_output();

        /* kicked objects fall in pools */
        if ((weapon == KICKED_WEAPON) &&
            (is_pool(level, x, y) ||
             is_lava(level, x, y)))
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
                if (!test_move(x - dx, y - dy, dx, dy, 0, TEST_MOVE,
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
    if (coords) {
        coords->x = x;
        coords->y = y;
    }
    return mdef;
}

/* TRUE iff thrown/kicked/rolled object doesn't pass through iron bars */
boolean
hits_bars(struct obj ** obj_p, /* *obj_p will be set to NULL if object breaks */
          int x, int y, int always_hit, /* caller can force a hit for items
                                           which would fit through */
          int whodidit)
{       /* 1==hero, 0=other, -1==just check whether it'll pass thru */
    struct obj *otmp = *obj_p;
    int obj_type = otmp->otyp;
    boolean hits = always_hit;

    if (!hits)
        switch (otmp->oclass) {
        case WEAPON_CLASS:
            {
                int oskill = objects[obj_type].oc_skill;

                hits = (oskill != -P_BOW && oskill != -P_CROSSBOW &&
                        oskill != -P_DART && oskill != -P_SHURIKEN &&
                        oskill != P_SPEAR && oskill != P_JAVELIN &&
                        oskill != P_KNIFE); /* but not dagger */
                break;
            }
        case ARMOR_CLASS:
            hits = (objects[obj_type].oc_armcat != ARM_GLOVES);
            break;
        case TOOL_CLASS:
            hits = (obj_type != SKELETON_KEY && obj_type != LOCK_PICK &&
                    obj_type != CREDIT_CARD && obj_type != TALLOW_CANDLE &&
                    obj_type != WAX_CANDLE && obj_type != LENSES &&
                    obj_type != TIN_WHISTLE && obj_type != MAGIC_WHISTLE);
            break;
        case ROCK_CLASS:       /* includes boulder */
            if (obj_type != STATUE || mons[otmp->corpsenm].msize > MZ_TINY)
                hits = TRUE;
            break;
        case FOOD_CLASS:
            if (obj_type == CORPSE && mons[otmp->corpsenm].msize > MZ_TINY)
                hits = TRUE;
            else
                hits = (obj_type == MEAT_STICK ||
                        obj_type == HUGE_CHUNK_OF_MEAT);
            break;
        case SPBOOK_CLASS:
        case WAND_CLASS:
        case BALL_CLASS:
        case CHAIN_CLASS:
            hits = TRUE;
            break;
        default:
            break;
        }

    if (hits && whodidit != -1)
        hits_bars_hit(obj_p, x, y, whodidit);

    return hits;
}

/* Split from hits_bars to allow additional bookkeeping before destroying the obj */
void
hits_bars_hit(struct obj **obj_p, int x, int y, int whodidit)
{
    struct obj *obj = *obj_p;
    if (whodidit ? hero_breaks(obj, x, y, FALSE) : breaks(obj, x, y))
        *obj_p = obj = NULL; /* object is now gone */
    else if (obj->otyp == BOULDER || obj->otyp == HEAVY_IRON_BALL)
        pline(msgc_levelsound, "Whang!");
    else if (obj->oclass == COIN_CLASS ||
             objects[obj->otyp].oc_material == GOLD ||
             objects[obj->otyp].oc_material == SILVER)
        pline(msgc_levelsound, "Clink!");
    else
        pline(msgc_levelsound, "Clonk!");
}

/* Returns the throwing range for given obj. If hurtle_range is non-NULL, set it to
   the range to be hurtled */
int
throwing_range(const struct monst *mon, const struct obj *obj, int *hurtle_range)
{
    int range, urange;
    /* TODO: acurrstr() for monsters */
    int str = acurr(mon, A_STR);
    if (str > 18 && str <= 121)
        str = (19 + str / 50); /* map to 19-21 */
    else if (str > 121)
        str -= 100;
    urange = str;

    /* balls are easy to throw or at least roll */
    /* also, this insures the maximum range of a ball is greater than 1, so
       the effects from throwing attached balls are actually possible */
    if (obj->otyp == HEAVY_IRON_BALL)
        range = urange - (int)(obj->owt / 100);
    else if (obj->otyp == BOULDER)
        range = 20; /* must be giant */
    else
        range = urange - (int)(obj->owt / 40);

    if (obj->oartifact == ART_MJOLLNIR)
        range = (range + 1) / 2; /* it's heavy */

    if (obj == uball) {
        if (mon != &youmonst)
            panic("Monster throwing your ball?");

        if (u.ustuck ||
            (u.utrap && u.utraptype == TT_INFLOOR))
            range = 1;
        else if (range >= 5)
            range = 5;
    }

    if (m_underwater(mon))
        range = 1;

    if (range < 1)
        range = 1;

    if (is_ammo(obj)) {
        if (ammo_and_launcher(obj, m_mwep(mon)))
            range++;
        else if (obj->oclass != GEM_CLASS)
            range /= 2;
    }

    if ((Is_airlevel(m_mz(mon)) && !flying(mon)) || levitates(mon)) {
        /* action, reaction... */
        urange -= range;
        if (urange < 1)
            urange = 1;
        range -= urange;
        if (range < 1)
            range = 1;
    }
    if (hurtle_range)
        *hurtle_range = urange;

    return range;
}

/* Throw an object. Returns TRUE if we should abort a potential multishot
   due to hurtling through air. */
boolean
throwit(struct monst *magr, struct obj *obj, struct obj *stack, int count,
        long wep_mask, /* used to re-equip returning boomerang */
        boolean twoweap, /* used to restore twoweapon mode if wielded
                            weapon returns */
        schar dx, schar dy, schar dz)
{
    struct monst *mdef;
    boolean uagr = (magr == &youmonst);
    boolean vis = (uagr || canseemon(magr));
    boolean hurtling = FALSE;
    int range, urange;
    int role = urole.num;
    if (magr != &youmonst)
        role = monsndx(magr->data);

    boolean impaired =
        (confused(magr) || stunned(magr) || blind(magr) ||
         hallucinating(magr) || fumbling(magr));
    boolean returning = FALSE;
    if ((role == PM_VALKYRIE && obj->oartifact == ART_MJOLLNIR) ||
        obj->otyp == BOOMERANG)
        returning = TRUE;

    obj->was_thrown = 1;
    if ((obj->cursed || obj->greased) && (dx || dy) && !rn2(7)) {
        boolean slipok = TRUE;

        if (ammo_and_launcher(obj, m_mwep(magr))) {
            if (vis) {
                pline(msgc_substitute, "%s!", Tobjnam(obj, "misfire"));
                if (!obj->greased) {
                    obj->bknown = TRUE;
                    if (stack)
                        stack->bknown = TRUE;
                }
            }
            if (!obj->greased && !uagr) {
                obj->mbknown = TRUE;
                if (stack)
                    stack->mbknown = TRUE;
            }
        } else {
            /* only slip if it's greased or meant to be thrown */
            if (obj->greased || throwing_weapon(obj)) {
                if (vis) {
                    pline(msgc_substitute,
                          "%s as %s %s!", Tobjnam(obj, "slip"),
                          m_verbs(magr, "throw"),
                          obj_isplural(obj) ? "them" : "it");
                    if (!obj->greased) {
                        obj->bknown = TRUE;
                        if (stack)
                            stack->bknown = TRUE;
                    }
                }
                if (!obj->greased && !uagr) {
                    obj->mbknown = TRUE;
                    if (stack)
                        stack->mbknown = TRUE;
                }
            } else
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

    if (uagr &&
        (dx || dy || (dz < 1)) && calc_capacity((int)obj->owt) > SLT_ENCUMBER &&
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
        mdef = u.ustuck;
        bhitpos.x = mdef->mx;
        bhitpos.y = mdef->my;
    } else if (dz) {
        if (dz < 0 && returning && !impaired) {
            if (vis)
                pline(msgc_yafm, "%s the %s and return%s to %s %s!",
                      Tobjnam(obj, "hit"), ceiling(magr->mx, magr->my),
                      obj_isplural(obj) ? "" : "s",
                      s_suffix(mon_nam(magr)), mbodypart(magr, HAND));
            if (uagr) {
                obj = pickinv(obj);
                setuwep(obj);
                u.twoweap = twoweap;
            } else {
                mpickobj(magr, obj, &obj);
                magr->mw = obj;
                obj->owornmask |= W_MASK(os_wep);
            }
        } else if (dz < 0 && !Is_airlevel(&u.uz) && !Underwater &&
                   !Is_waterlevel(&u.uz))
            toss_up(magr, obj, rn2(5));
        else
            hitfloor(magr, obj);
        thrownobj = NULL;
        return hurtling;
    } else {
        boolean obj_destroyed;

        urange = 0;
        range = throwing_range(magr, obj, &urange);

        obj_destroyed = FALSE;
        mdef = fire_obj(magr, dx, dy, range, THROWN_WEAPON, obj, &obj_destroyed,
                        &bhitpos);

        /* have to do this after fire_obj() so mx/my are correct */
        if ((Is_airlevel(m_mz(magr)) && !flying(magr)) || levitates(magr)) {
            if (uagr)
                hurtle(-dx, -dy, urange, TRUE);
            else
                mhurtle(magr, -dx, -dy, urange);
            hurtling = TRUE;
        }

        if (obj_destroyed)
            return hurtling;
    }

    if (mdef) {
        boolean obj_gone;

        if (mx_eshk(mdef) && obj->where == OBJ_MINVENT && obj->ocarry == mdef) {
            thrownobj = NULL;
            return hurtling; /* alert shk caught it */
        }
        snuff_candle(obj);
        notonhead = (bhitpos.x != mdef->mx || bhitpos.y != mdef->my);
        obj_gone = thitmonst(magr, mdef, obj, stack, count);

        /* [perhaps this should be moved into thitmonst or hmon] */
        if (mdef && mx_eshk(mdef) && uagr &&
            (!inside_shop(level, youmonst.mx, youmonst.my) ||
             !strchr(in_rooms(level, mdef->mx, mdef->my, SHOPBASE), *u.ushops)))
            hot_pursuit(mdef);

        if (obj_gone)
            return hurtling;
    }

    if (uagr && Engulfed) {
        /* ball is not picked up by monster */
        if (obj != uball)
            mpickobj(u.ustuck, obj, NULL);
    } else {
        /* the code following might become part of dropy() */
        if (returning) {
            show_obj_flight(obj, bhitpos.x, bhitpos.y, magr->mx, magr->my);

            if (!impaired) {
                if (vis)
                    pline(msgc_actionok, "%s to %s %s!", Tobjnam(obj, "return"),
                          s_suffix(mon_nam(magr)), mbodypart(magr, HAND));
                if (uagr) {
                    obj = pickinv(obj);
                    setuwep(obj);
                    u.twoweap = twoweap;
                } else {
                    mpickobj(magr, obj, &obj);
                    magr->mw = obj;
                    obj->owornmask |= W_MASK(os_wep);
                }
                if (cansee(bhitpos.x, bhitpos.y))
                    newsym(bhitpos.x, bhitpos.y);
            } else {
                int dmg = rn2_on_rng(2, uagr ? rng_mjollnir_return : rng_main);
                if (!dmg && vis) {
                    pline(msgc_substitute, Blind ? "%s lands %s %s %s." :
                          "%s back to you, landing %s %s %s.",
                          Blind ? "Something" : Tobjnam(obj, "return"),
                          levitates(magr) ? "beneath" : "at",
                          s_suffix(mon_nam(magr)), makeplural(mbodypart(magr, FOOT)));
                } else if (dmg) {
                    dmg += rnd(3);
                    if (uagr && Blind)
                        pline(msgc_substitute, "%s your %s!",
                              Tobjnam(obj, "hit"), body_part(ARM));
                    else if (vis)
                        pline(msgc_substitute, "%s back towards %s, hitting %s %s!",
                              Tobjnam(obj, Blind ? "hit" : "fly"), mon_nam(magr),
                              uagr ? "your" : mhis(magr), mbodypart(magr, ARM));
                    artifact_hit(NULL, magr, obj, &dmg, 0);
                    if (uagr)
                        losehp(dmg, killer_msg_obj(DIED, obj));
                    else {
                        magr->mhp -= dmg;
                        if (magr->mhp <= 0)
                            mondied(magr);
                    }
                }
                if (ship_object(obj, magr->mx, magr->my, FALSE)) {
                    thrownobj = NULL;
                    return hurtling;
                }
                dropy(obj);
            }
            thrownobj = NULL;
            return hurtling;
        }

        if (!IS_SOFT(level->locations[bhitpos.x][bhitpos.y].typ) &&
            breaktest(obj)) {
            struct tmp_sym *tsym = tmpsym_initobj(obj);

            tmpsym_at(tsym, bhitpos.x, bhitpos.y);
            win_delay_output();
            tmpsym_end(tsym);
            breakmsg(obj, cansee(bhitpos.x, bhitpos.y));
            breakobj(obj, bhitpos.x, bhitpos.y, uagr, uagr);
            return hurtling;
        }
        if (flooreffects(obj, bhitpos.x, bhitpos.y, "fall"))
            return hurtling;
        obj_no_longer_held(obj);
        if (mdef && mx_eshk(mdef) && is_pick(obj)) {
            if (cansee(bhitpos.x, bhitpos.y))
                pline(msgc_substitute, "%s snatches up %s.", Monnam(mdef),
                      the(xname(obj)));
            if (uagr && *u.ushops)
                check_shop_obj(obj, bhitpos.x, bhitpos.y, FALSE);
            mpickobj(mdef, obj, NULL); /* may merge and free obj */
            thrownobj = NULL;
            return hurtling;
        }
        snuff_candle(obj);
        if (!mdef && ship_object(obj, bhitpos.x, bhitpos.y, FALSE)) {
            thrownobj = NULL;
            return hurtling;
        }
        thrownobj = NULL;
        place_object(obj, level, bhitpos.x, bhitpos.y);
        if (uagr && *u.ushops && obj != uball)
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
        if (uagr)
            level->locations[bhitpos.x][bhitpos.y].mem_stepped = 0;

        if (!IS_SOFT(level->locations[bhitpos.x][bhitpos.y].typ))
            container_impact_dmg(obj);
    }
    return hurtling;
}

/* an object may hit a monster; various factors adjust the chance of hitting */
int
omon_adj(struct monst *mon, struct obj *obj, boolean mon_notices)
{
    boolean you = (mon == &youmonst);
    int tmp = 0;

    /* size of target affects the chance of hitting */
    tmp += (mon->data->msize - MZ_MEDIUM);      /* -2..+5 */
    /* sleeping target is more likely to be hit.
       This is sleep from being generated asleep, not from magic, so monster-only. */
    if (!you && m_helpless(mon, hm_asleep)) {
        tmp += 2;
        if (mon_notices)
            mon->msleeping = 0;
    }
    /* ditto for immobilized target */
    if (m_helpless(mon, hm_all) || !mon->data->mmove) {
        tmp += 4;
        if (mon_notices && mon->data->mmove && !rn2(10) &&
            !(property_timeout(mon, STONED) <= 2)) {
            if (you)
                cancel_helplessness(hm_unconscious,
                                    "Hit by something, you snap awake!");
            else {
                mon->mcanmove = 1;
                mon->mfrozen = 0;
            }
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
tmiss(struct monst *magr, struct monst *mdef, struct obj *obj, int count)
{
    const char *missile = mshot_xname(obj, count);

    if (magr == &youmonst || mdef == &youmonst || canseemon(magr) || canseemon(mdef))
        pline(combat_msgc(magr, mdef, cr_miss), "%s %s %s.", The(missile),
              otense(obj, "miss"), mon_nam(mdef));
    if (!rn2(3) && magr == &youmonst && mdef != &youmonst)
        wakeup(mdef, TRUE);
}

#define quest_arti_hits_leader(obj,mon) \
  (obj->oartifact && is_quest_artifact(obj) && (mon->data->msound == MS_LEADER))

/* Object thrown by player arrives at monster's location. Returns
   1 if obj has disappeared or otherwise been taken care of, 0 if
   caller must take care of it. count is used to pass (count)th
   shot in a multishot. */
int
thitmonst(struct monst *magr, struct monst *mdef, struct obj *obj,
          struct obj *stack, int count)
{
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef || canseemon(magr) || canseemon(mdef));
    int tmp;    /* Base chance to hit */
    int disttmp;        /* distance modifier */
    int otyp = obj->otyp;
    boolean guaranteed_hit = (uagr && Engulfed && mdef == u.ustuck);
    struct obj *mwep = m_mwep(magr);
    int dieroll = rnd(20);

    /* Differences from melee weapons: Dex still gives a bonus, but strength
       does not. Polymorphed players lacking attacks may still throw. There's a 
       base -1 to hit. No bonuses for fleeing or stunned targets (they don't
       dodge melee blows as readily, but dodging arrows is hard anyway). Not
       affected by traps, etc. Certain items which don't in themselves do
       damage ignore tmp. Distance and monster size affect chance to hit. */
    tmp = (-1 + find_mac(mdef) + hitbon(magr) +
           (uagr && Upolyd ? magr->data->mlevel : magr->m_lev));
    if (uagr)
        tmp += Luck;
    if (acurr(magr, A_DEX) < 4)
        tmp -= 3;
    else if (acurr(magr, A_DEX) < 6)
        tmp -= 2;
    else if (acurr(magr, A_DEX) < 8)
        tmp -= 1;
    else if (acurr(magr, A_DEX) >= 14)
        tmp += (ACURR(A_DEX) - 14);

    /* Modify to-hit depending on distance; but keep it sane. Polearms get a
       distance penalty even when wielded; it's hard to hit at a distance. */
    disttmp = 3 - distmin(magr->mx, magr->my, mdef->mx, mdef->my);
    if (disttmp < -4)
        disttmp = -4;
    tmp += disttmp;
    
    /* gloves are a hinderance to proper use of bows */
    struct obj *armg = which_armor(magr, os_armg);
    if (armg && mwep && objects[mwep->otyp].oc_skill == P_BOW) {
        switch (armg->otyp) {
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
            impossible("Unknown type of gloves (%d)", armg->otyp);
            break;
        }
    }

    tmp += omon_adj(mdef, obj, TRUE);
    if (race(magr, FALSE) == PM_ELF && race(mdef, FALSE) == PM_ORC)
        tmp++;

    if (obj->oclass == GEM_CLASS && is_unicorn(mdef->data) &&
        !m_helpless(mdef, hm_all)) {
        if ((uagr || magr->mtame) && (udef || mdef->mtame)) {
            /* Prevent abuse by you catching gems from tame monsters, or tame unicorns
               (which would eventually also drop them) */
            pline(msgc_yafm, "%s and drop%ss %s.", M_verbs(mdef, "catch"),
                  udef ? "" : "s", the(xname(obj)));
            return 0;
        } else {
            /* Fair game, even if they're throwing gems to *you*. */
            pline(msgc_actionok, "%s %s.", M_verbs(mdef, "catch"), the(xname(obj)));
            return gem_accept(magr, mdef, obj);
        }
    }

    /* don't make game unwinnable if naive player throws artifact at
       leader... */
    if (quest_arti_hits_leader(obj, mdef)) {
        /* not wakeup(), which angers non-tame monsters */
        if (!udef) {
            mdef->msleeping = 0;
            if (idle(mdef))
                mdef->mstrategy = st_none;
        }

        if (!m_helpless(mdef, udef ? hm_all : hm_paralyzed)) {
            if (vis)
                pline(msgc_actionok, "%s %s.", M_verbs(mdef, "catch"),
                      the(xname(obj)));
            if (udef || mdef->mpeaceful) {
                boolean next2u = monnear(mdef, magr->mx, magr->my);

                finish_quest(obj); /* acknowledge quest completion */
                if (vis)
                    pline_implied(msgc_consequence, "%s %s back to you.",
                                  M_verbs(mdef, next2u ? "hand" : "toss"),
                                  the(xname(obj)));
                if (!next2u)
                    show_obj_flight(obj, mdef->mx, mdef->my,
                                    magr->mx, magr->my);
                if (uagr)
                    pickinv(obj); /* back into your inventory */
                else
                    mpickobj(magr, obj, NULL);
            } else
                mpickobj(mdef, obj, NULL); /* angry leader wont return it */
            return 1; /* caller doesn't need to place it */
        }
        return 0;
    } else if (uagr && !udef && mdef->mtame && mdef->mcanmove &&
               !is_animal(mdef->data) && !mindless(mdef->data) &&
               !(uwep && ammo_and_launcher(obj, uwep))) {
        /* thrown item at intelligent pet to let it use it */
        pline(msgc_actionok, "%s %s.",
              M_verbs(mdef, "catch"), the(xname(obj)));
        obj->thrown_time = moves;
        obj_extract_self(obj);
        mpickobj(mdef, obj, NULL);
        if (attacktype(mdef->data, AT_WEAP) &&
            mdef->weapon_check == NEED_WEAPON) {
            mdef->weapon_check = NEED_HTH_WEAPON;
            mon_wield_item(mdef);
        }

        m_dowear(mdef, FALSE);
        newsym(mdef->mx, mdef->my);
        return 1;
    }

    if (obj->oclass == WEAPON_CLASS || is_weptool(obj) ||
        obj->oclass == GEM_CLASS) {
        if (is_ammo(obj)) {
            if (!ammo_and_launcher(obj, mwep)) {
                tmp -= 4;
            } else {
                tmp += mwep->spe - greatest_erosion(mwep);
                /* TODO: Monsters have ammo+launcher skills, so
                   make use of it here */
                if (uagr)
                    tmp += weapon_hit_bonus(mwep);
                if (mwep->oartifact)
                    tmp += spec_abon(mwep, mdef);
                /* 
                 * Elves and Samurais are highly trained w/bows,
                 * especially their own special types of bow.
                 * Polymorphing won't make you a bow expert.
                 */
                if ((race(magr, TRUE) == PM_ELF ||
                    (uagr ? u.umonster :
                     magr->orig_mnum ? magr->orig_mnum :
                     monsndx(magr->data)) == PM_SAMURAI) &&
                    objects[mwep->otyp].oc_skill == P_BOW) {
                    tmp++;
                    if (race(magr, TRUE) == PM_ELF &&
                        mwep->otyp == ELVEN_BOW)
                        tmp++;
                    else if (mwep->otyp == YUMI &&
                             (uagr ? u.umonster :
                              magr->orig_mnum ? magr->orig_mnum :
                              monsndx(magr->data)) == PM_SAMURAI)
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
            if (uagr)
                tmp += weapon_hit_bonus(obj);
        }

        if (guaranteed_hit || tmp >= dieroll) {
            if (mhmon(magr, mdef, obj, 1, count, dieroll) && !udef) /* mon still alive */
                cutworm(mdef, bhitpos.x, bhitpos.y, obj);
            if (uagr)
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
                if (obj->blessed && !mrnl(magr, 4))
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
                    if (stack && vis)
                        learn_oprop(stack, opm_detonate);
                }

                if (broken) {
                    if (uagr && *u.ushops)
                        check_shop_obj(obj, bhitpos.x, bhitpos.y, TRUE);
                    obfree(obj, NULL);
                    return 1;
                }
            }
            passive_obj(mdef, obj, NULL);
        } else
            tmiss(magr, mdef, obj, count);
    } else if (otyp == HEAVY_IRON_BALL ||
               otyp == BOULDER ||
               otyp == EGG ||
               otyp == CREAM_PIE ||
               otyp == BLINDING_VENOM ||
               otyp == ACID_VENOM ||
               obj->oclass == POTION_CLASS) {
        if (uagr &&
            (otyp == HEAVY_IRON_BALL ||
             otyp == BOULDER))
            exercise(A_STR, TRUE);
        if (guaranteed_hit ||
            ((otyp == HEAVY_IRON_BALL || otyp == BOULDER) ? dieroll :
             acurr(magr, A_DEX) > rnd(25))) {
            int was_swallowed = guaranteed_hit;

            if (uagr)
                exercise(A_DEX, TRUE);
            if (obj->oclass == POTION_CLASS) {
                potionhit(mdef, obj, magr);
                return 1; /* potionhit shatters the item */
            } else if (!mhmon(magr, mdef, obj, 1, count, dieroll) &&
                       otyp == HEAVY_IRON_BALL && uagr && was_swallowed &&
                       !Engulfed && obj == uball)
                return 1; /* already did placebc() */
        } else
            tmiss(magr, mdef, obj, count);
    } else if (!udef &&
               (befriend_with_obj(mdef->data, obj) ||
                (mdef->mtame && dogfood(mdef, obj) >= df_acceptable))) {
        /* TODO: hostile monsters untaming/making dogs hostile, not trivial since
           tamedog() messes with the obj */
        if ((uagr || magr->mtame) && tamedog(mdef, obj))
            return 1;   /* obj is gone */
        else {
            /* not tmiss(), which angers non-tame monsters */
            miss(xname(obj), mdef, &youmonst);
            mdef->msleeping = 0;
            if (idle(mdef))
                mdef->mstrategy = st_none;
        }
    } else if (guaranteed_hit) {
        /* this assumes that guaranteed_hit is due to swallowing */
        wakeup(mdef, TRUE);
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
              s_suffix(mon_nam(mdef)),
              is_animal(u.ustuck->data) ? "entrails" : "currents");
    } else
        tmiss(magr, mdef, obj, count);
    return 0;
}

static int
gem_accept(struct monst *magr, struct monst *mdef, struct obj *obj)
{
    boolean is_buddy = sgn(mdef->data->maligntyp) == malign(magr);
    boolean is_gem = objects[obj->otyp].oc_material == GEMSTONE;
    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (udef || canseemon(mdef));
    boolean pickup = TRUE;
    int ret = 0;
    int id_level = 0; /* not ID-ed */
    if (!uagr || (obj->dknown && objects[obj->otyp].oc_name_known))
        id_level = 2; /* fully ID-ed */
    else if (ox_name(obj) || objects[obj->otyp].oc_uname)
        id_level = 1; /* named (probably guessing) */

    /* Reverts unicorns hostile to the thrower (except if the unicorn is you) to being
       peaceful towards it. */
    if (!udef &&
        (((uagr || magr->mtame) && !mdef->mpeaceful) ||
         (mdef->mtame && !magr->mpeaceful))) {
        msethostility(mdef, FALSE, FALSE);
        if (uagr && !udef)
            mdef->mavenge = 0;
    }

    if (is_gem || !id_level) {
        if (vis)
            pline(is_gem ? msgc_nospoil : msgc_yafm, "%s %s accept%s %s gift.",
                  Monnam(mdef), !is_gem ? "graciously" : is_buddy ? "gratefully" :
                  "hestitatingly", udef ? "" : "s", s_suffix(mon_nam(magr)));
        /* TODO: luck for monsters */
        if (is_gem && uagr) {
            if (is_buddy)
                change_luck(id_level == 2 ? 5 : id_level == 1 ? 2 : 1);
            else if (id_level == 2)
                change_luck(rn2(7) - 3);
            else
                change_luck(rn2(3) - 1);
        }
    } else {
        if (vis)
            pline(msgc_yafm, "%s not interested in %s junk.", M_verbs(mdef, "are"),
                  s_suffix(mon_nam(magr)));
        pickup = FALSE;
    }

    if (pickup) {
        if (uagr && *u.ushops)
            check_shop_obj(obj, mdef->mx, mdef->my, TRUE);
        if (udef)
            hold_another_object(obj, "You drop %s.", doname(obj), NULL);
        else
            mpickobj(mdef, obj, NULL); /* may merge and free obj */
        ret = 1;
    }

    mon_tele(mdef, !!teleport_control(mdef));
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
                    (*o_shop != u.ushops[0] || !inside_shop(level, youmonst.mx, youmonst.my))
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

/*dothrow.c*/
