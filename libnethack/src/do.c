/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* Contains code for 'd', 'D' (drop), '>', '<' (up, down) */

#include "hack.h"
#include "lev.h"

static void dosinkring(struct obj *);
static int drop(struct obj *);

static int menu_drop(int);
static void final_level(void);

/* static boolean badspot(XCHAR_P,XCHAR_P); */

static const char drop_types[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

/* 'd' command: drop one inventory item */
int
dodrop(const struct nh_cmd_arg *arg)
{
    int result, i = (invent) ? 0 : (SIZE(drop_types) - 1);
    struct obj *obj;

    if (*u.ushops)
        sellobj_state(SELL_DELIBERATE);
    obj = getargobj(arg, &drop_types[i], "drop");
    if (!obj)
        return 0;
    obj->was_dropped = 1;
    result = drop(obj);
    if (*u.ushops)
        sellobj_state(SELL_NORMAL);

    return result;
}


/* Called when a boulder is dropped, thrown, or pushed.  If it ends up
 * in a pool, it either fills the pool up or sinks away.  In either case,
 * it's gone for good...  If the destination is not a pool, returns FALSE.
 */
boolean
boulder_hits_pool(struct obj * otmp, int rx, int ry, boolean pushing)
{
    if (!otmp || otmp->otyp != BOULDER)
        impossible("Not a boulder?");
    else if (!Is_waterlevel(&u.uz) &&
             (is_pool(level, rx, ry) || is_lava(level, rx, ry))) {
        boolean lava = is_lava(level, rx, ry), fills_up;
        const char *what = waterbody_name(rx, ry);
        schar ltyp = level->locations[rx][ry].typ;
        int chance = rn2_on_rng(10, rng_boulder_bridge);

        fills_up = lava ? chance == 0 : chance != 9; /* water: 90%; lava: 10% */

        if (fills_up) {
            struct trap *ttmp = t_at(level, rx, ry);

            if (ltyp == DRAWBRIDGE_UP) {
                /* clear lava */
                level->locations[rx][ry].drawbridgemask &= ~DB_UNDER;

                level->locations[rx][ry].drawbridgemask |= DB_FLOOR;
            } else
                level->locations[rx][ry].typ = ROOM;

            if (ttmp)
                delfloortrap(level, ttmp);
            bury_objs(level, rx, ry);

            newsym(rx, ry);
            if (pushing) {
                if (u.usteed) {
                    const char *bp = y_monnam(u.usteed);

                    pline("%s pushes %s into the %s.",
                          msgupcasefirst(bp), the(xname(otmp)), what);
                } else
                    pline("You push %s into the %s.", the(xname(otmp)), what);
                if (flags.verbose && !Blind)
                    pline("Now you can cross it!");
                /* no splashing in this case */
            }
        }
        if (!fills_up || !pushing) {    /* splashing occurs */
            if (!u.uinwater) {
                if (pushing ? !Blind : cansee(rx, ry)) {
                    pline("There is a large splash as %s %s the %s.",
                          the(xname(otmp)), fills_up ? "fills" : "falls into",
                          what);
                } else
                    You_hear("a%s splash.", lava ? " sizzling" : "");
                wake_nearto(rx, ry, 40);
            }

            if (fills_up && u.uinwater && distu(rx, ry) == 0) {
                u.uinwater = 0;
                doredraw();
                turnstate.vision_full_recalc = TRUE;
                pline("You find yourself on dry land again!");
            } else if (lava && distu(rx, ry) <= 2) {
                pline("You are hit by molten lava%c",
                      Fire_resistance ? '.' : '!');
                burn_away_slime();
                losehp(dice((Fire_resistance ? 1 : 3), 6),
                       killer_msg(DIED, "molten lava"));
            } else if (!fills_up && flags.verbose &&
                       (pushing ? !Blind : cansee(rx, ry)))
                pline("It sinks without a trace!");
        }

        /* boulder is now gone */
        if (pushing)
            delobj(otmp);
        else
            obfree(otmp, NULL);
        return TRUE;
    }
    return FALSE;
}

/* Used for objects which sometimes do special things when dropped; must be
 * called with the object not in any chain.  Returns TRUE if the object goes
 * away.
 */
boolean
flooreffects(struct obj * obj, int x, int y, const char *verb)
{
    struct level *lev = level;
    struct trap *t;
    struct monst *mtmp;

    if (obj->where != OBJ_FREE)
        panic("flooreffects: obj not free");

    /* We can't set obj->nobj to NULL like is done in 3.4.3; that has the effect
       of breaking the floating_objects chain. Instead, we set nexthere to NULL,
       and tell fire_damage and water_damage to follow the nexthere chain. */
    obj->nexthere = NULL;

    if (obj->otyp == BOULDER && boulder_hits_pool(obj, x, y, FALSE))
        return TRUE;
    else if (obj->otyp == BOULDER && (t = t_at(lev, x, y)) != 0 &&
             (t->ttyp == PIT || t->ttyp == SPIKED_PIT || t->ttyp == TRAPDOOR ||
              t->ttyp == HOLE)) {
        if (((mtmp = m_at(lev, x, y)) && mtmp->mtrapped) ||
            (u.utrap && u.ux == x && u.uy == y)) {
            if (*verb)
                pline("The boulder %s into the pit%s.", vtense(NULL, verb),
                      (mtmp) ? "" : " with you");
            if (mtmp) {
                if (!passes_walls(mtmp->data) && !throws_rocks(mtmp->data)) {
                    if (hmon(mtmp, obj, TRUE) && !is_whirly(mtmp->data))
                        return FALSE;   /* still alive */
                }
                mtmp->mtrapped = 0;
            } else {
                if (!Passes_walls && !throws_rocks(youmonst.data)) {
                    losehp(rnd(15), "squished under a boulder");
                    return FALSE;       /* player remains trapped */
                } else
                    u.utrap = 0;
            }
        }
        if (*verb) {
            if (Blind && couldsee(x, y)) {
                if ((x == u.ux) && (y == u.uy))
                    You_hear("a CRASH! beneath you.");
                else
                    You_hear("a nearby CRASH!");
            } else if (cansee(x, y))
                pline("The boulder %s%s.", t->tseen ? "" : "triggers and ",
                      t->ttyp == TRAPDOOR ? "plugs a trap door" : t->ttyp ==
                      HOLE ? "plugs a hole" : "fills a pit");
            else
                You_hear("a distant CRASH!");
        }
        deltrap(lev, t);
        obfree(obj, NULL);
        bury_objs(lev, x, y);
        if (lev == level)
            newsym(x, y);
        return TRUE;
    } else if (is_lava(lev, x, y)) {
        return fire_damage(obj, FALSE, TRUE, x, y);
    } else if (is_pool(lev, x, y)) {
        /* Reasonably bulky objects (arbitrary) splash when dropped. If you're
           floating above the water even small things make noise. Stuff dropped 
           near fountains always misses */
        if ((Blind || (Levitation || Flying)) && canhear() &&
            ((x == u.ux) && (y == u.uy))) {
            if (!Underwater) {
                if (weight(obj) > 9) {
                    pline("Splash!");
                } else if (Levitation || Flying) {
                    pline("Plop!");
                }
            }
            map_background(x, y, 0);
            if (lev == level)
                newsym(x, y);
        }
        return water_damage(obj, NULL, FALSE) == 3;
    } else if (u.ux == x && u.uy == y && (!u.utrap || u.utraptype != TT_PIT) &&
               (t = t_at(lev, x, y)) != 0 && t->tseen &&
               (t->ttyp == PIT || t->ttyp == SPIKED_PIT)) {
        /* you escaped a pit and are standing on the precipice */
        if (Blind)
            You_hear("%s tumble downwards.", the(xname(obj)));
        else
            pline("%s %s into %s pit.", The(xname(obj)), otense(obj, "tumble"),
                  the_your[t->madeby_u]);
    }
    return FALSE;
}


void
doaltarobj(struct obj *obj)
{       /* obj is an object dropped on an altar */
    if (Blind)
        return;

    /* KMH, conduct */
    break_conduct(conduct_gnostic);

    if ((obj->blessed || obj->cursed) && obj->oclass != COIN_CLASS) {
        pline("There is %s flash as %s %s the altar.",
              an(hcolor(obj->blessed ? "amber" : "black")), doname(obj),
              otense(obj, "hit"));
        if (!Hallucination)
            obj->bknown = 1;
    } else {
        pline("%s %s on the altar.", Doname2(obj), otense(obj, "land"));
        obj->bknown = 1;
    }
    /* Also BCU one level deep inside containers */
    if (Has_contents(obj)) {
        int bcucount = 0;
        struct obj *otmp;

        for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
            if (otmp->blessed || otmp->cursed)
                bcucount++;
            if (!Hallucination)
                otmp->bknown = 1;
        }
        if (bcucount == 1) {
            pline("Looking inside %s, you see a colored flash.",
                  the(xname(obj)));
        } else if (bcucount > 1) {
            pline("Looking inside %s, you see colored flashes.",
                  the(xname(obj)));
        }
    }
}

static void
dosinkring(struct obj *obj)  /* obj is a ring being dropped over a sink */
{
    struct obj *otmp, *otmp2;
    boolean ideed = TRUE;

    pline("You drop %s down the drain.", doname(obj));
    obj->in_use = TRUE; /* block free identification via interrupt */
    switch (obj->otyp) {        /* effects that can be noticed without eyes */
    case RIN_SEARCHING:
        pline("You thought your %s got lost in the sink, but there it is!",
              xname(obj));
        goto giveback;
    case RIN_SLOW_DIGESTION:
        pline("The ring is regurgitated!");
    giveback:
        obj->in_use = FALSE;
        /* this is safe, the ring was unwielded by the caller */
        dropx(obj);
        makeknown(obj->otyp);
        return;
    case RIN_LEVITATION:
        pline("The sink quivers upward for a moment.");
        break;
    case RIN_POISON_RESISTANCE:
        pline("You smell rotten %s.", makeplural(fruitname(FALSE)));
        break;
    case RIN_AGGRAVATE_MONSTER:
        pline("Several flies buzz angrily around the sink.");
        break;
    case RIN_SHOCK_RESISTANCE:
        pline("Static electricity surrounds the sink.");
        break;
    case RIN_CONFLICT:
        You_hear("loud noises coming from the drain.");
        break;
    case RIN_SUSTAIN_ABILITY:  /* KMH */
        pline("The water flow seems fixed.");
        break;
    case RIN_GAIN_STRENGTH:
        pline("The water flow seems %ser now.",
              (obj->spe < 0) ? "weak" : "strong");
        break;
    case RIN_GAIN_CONSTITUTION:
        pline("The water flow seems %ser now.",
              (obj->spe < 0) ? "less" : "great");
        break;
    case RIN_INCREASE_ACCURACY:        /* KMH */
        pline("The water flow %s the drain.",
              (obj->spe < 0) ? "misses" : "hits");
        break;
    case RIN_INCREASE_DAMAGE:
        pline("The water's force seems %ser now.",
              (obj->spe < 0) ? "small" : "great");
        break;
    case RIN_HUNGER:
        ideed = FALSE;
        for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp2) {
            otmp2 = otmp->nexthere;
            if (otmp != uball && otmp != uchain && !obj_resists(otmp, 1, 99)) {
                if (!Blind) {
                    pline("Suddenly, %s %s from the sink!", doname(otmp),
                          otense(otmp, "vanish"));
                    ideed = TRUE;
                }
                delobj(otmp);
            }
        }
        break;
    case MEAT_RING:
        /* Not the same as aggravate monster; besides, it's obvious. */
        pline("Several flies buzz around the sink.");
        break;
    default:
        ideed = FALSE;
        break;
    }
    if (!Blind && !ideed && obj->otyp != RIN_HUNGER) {
        ideed = TRUE;
        switch (obj->otyp) {    /* effects that need eyes */
        case RIN_ADORNMENT:
            pline("The faucets flash brightly for a moment.");
            break;
        case RIN_REGENERATION:
            pline("The sink looks as good as new.");
            break;
        case RIN_INVISIBILITY:
            pline("You don't see anything happen to the sink.");
            break;
        case RIN_FREE_ACTION:
            pline("You see the ring slide right down the drain!");
            break;
        case RIN_SEE_INVISIBLE:
            pline("You see some air in the sink.");
            break;
        case RIN_STEALTH:
            pline("The sink seems to blend into the floor for a moment.");
            break;
        case RIN_FIRE_RESISTANCE:
            pline("The hot water faucet flashes brightly for a moment.");
            break;
        case RIN_COLD_RESISTANCE:
            pline("The cold water faucet flashes brightly for a moment.");
            break;
        case RIN_PROTECTION_FROM_SHAPE_CHANGERS:
            pline("The sink looks nothing like a fountain.");
            break;
        case RIN_PROTECTION:
            pline("The sink glows %s for a moment.",
                  hcolor((obj->spe < 0) ? "black" : "silver"));
            break;
        case RIN_WARNING:
            pline("The sink glows %s for a moment.", hcolor("white"));
            break;
        case RIN_TELEPORTATION:
            pline("The sink momentarily vanishes.");
            break;
        case RIN_TELEPORT_CONTROL:
            pline("The sink looks like it is being beamed aboard somewhere.");
            break;
        case RIN_POLYMORPH:
            pline("The sink momentarily looks like a fountain.");
            break;
        case RIN_POLYMORPH_CONTROL:
            pline ("The sink momentarily looks like a regularly erupting "
                   "geyser.");
            break;
        }
    }
    if (ideed)
        makeknown(obj->otyp);
    else
        You_hear("the ring bouncing down the drainpipe.");

    /* A custom RNG would be nice here, but I can't see a way to make it work
       without having a different RNG for each class of ring, which would be
       overkill. */
    if (!rn2(20)) {
        pline("The sink backs up, leaving %s.", doname(obj));
        obj->in_use = FALSE;
        /* the caller has checked this is safe */
        dropx(obj);
    } else
        useup(obj);
}


/* some common tests when trying to drop or throw items */
boolean
canletgo(struct obj *obj, const char *word)
{
    if (obj->owornmask & W_WORN) {
        if (*word)
            pline_once("You cannot %s something you are wearing.", word);
        return FALSE;
    }
    if (obj->otyp == LOADSTONE && obj->cursed) {
        /* getobj() kludge sets corpsenm to user's specified count when
           refusing to split a stack of cursed loadstones */
        if (*word) {
            /* getobj() ignores a count for throwing since that is implicitly
               forced to be 1; replicate its kludge... */
            if (!strcmp(word, "throw") && obj->quan > 1L)
                obj->corpsenm = 1;
            pline("For some reason, you cannot %s%s the stone%s!", word,
                  obj->corpsenm ? " any of" : "", plur(obj->quan));
        }
        obj->corpsenm = 0;      /* reset */
        obj->bknown = 1;
        return FALSE;
    }
    if (obj->otyp == LEASH && obj->leashmon != 0) {
        if (*word)
            pline("The leash is tied around your %s.", body_part(HAND));
        return FALSE;
    }
    if (obj->owornmask & W_MASK(os_saddle)) {
        if (*word)
            pline("You cannot %s something you are sitting on.", word);
        return FALSE;
    }
    return TRUE;
}

static int
drop(struct obj *obj)
{
    if (!obj)
        return 0;
    if (!canletgo(obj, "drop"))
        return 0;
    if (obj == uwep) {
        if (welded(uwep)) {
            weldmsg(obj);
            return 0;
        }
    }
    unwield_silently(obj);

    if (Engulfed) {
        /* barrier between you and the floor */
        if (flags.verbose) {
            pline("You drop %s into %s %s.", doname(obj),
                  s_suffix(mon_nam(u.ustuck)), mbodypart(u.ustuck, STOMACH));
        }
    } else {
        if ((obj->oclass == RING_CLASS || obj->otyp == MEAT_RING) &&
            IS_SINK(level->locations[u.ux][u.uy].typ)) {
            dosinkring(obj);
            return 1;
        }
        if (!can_reach_floor()) {
            if (flags.verbose)
                pline("You drop %s.", doname(obj));

            freeinv(obj);
            hitfloor(obj);
            return 1;
        }
        if (!IS_ALTAR(level->locations[u.ux][u.uy].typ) && flags.verbose)
            pline("You drop %s.", doname(obj));
    }
    dropx(obj);
    return 1;
}

/* Called in several places - may produce output */
/* eg ship_object() and dropy() -> sellobj() both produce output */
void
dropx(struct obj *obj)
{
    freeinv(obj);
    if (!Engulfed) {
        if (ship_object(obj, u.ux, u.uy, FALSE))
            return;
        if (IS_ALTAR(level->locations[u.ux][u.uy].typ))
            doaltarobj(obj);    /* set bknown */
    }
    dropy(obj);
}

void
dropy(struct obj *obj)
{
    unwield_silently(obj);

    if (!Engulfed && flooreffects(obj, u.ux, u.uy, "drop"))
        return;
    /* Engulfed check done by GAN 01/29/87 */
    if (Engulfed) {
        boolean could_petrify = FALSE;
        boolean could_poly = FALSE;
        boolean could_slime = FALSE;
        boolean could_grow = FALSE;
        boolean could_heal = FALSE;

        if (obj != uball) {     /* mon doesn't pick up ball */
            if (obj->otyp == CORPSE) {
                could_petrify = touch_petrifies(&mons[obj->corpsenm]);
                could_poly = polyfodder(obj);
                could_slime = (obj->corpsenm == PM_GREEN_SLIME);
                could_grow = (obj->corpsenm == PM_WRAITH);
                could_heal = (obj->corpsenm == PM_NURSE);
            }
            mpickobj(u.ustuck, obj);
            if (is_animal(u.ustuck->data)) {
                if (could_poly || could_slime) {
                    newcham(u.ustuck, could_poly ? NULL : &mons[PM_GREEN_SLIME],
                            FALSE, could_slime);
                    delobj(obj);        /* corpse is digested */
                } else if (could_petrify) {
                    minstapetrify(u.ustuck, TRUE);
                    /* Don't leave a cockatrice corpse in a statue */
                    if (!Engulfed)
                        delobj(obj);
                } else if (could_grow) {
                    grow_up(u.ustuck, NULL);
                    delobj(obj);        /* corpse is digested */
                } else if (could_heal) {
                    u.ustuck->mhp = u.ustuck->mhpmax;
                    delobj(obj);        /* corpse is digested */
                }
            }
        }
    } else {
        place_object(obj, level, u.ux, u.uy);
        if (obj == uball)
            drop_ball(u.ux, u.uy, 0, 0);
        else
            sellobj(obj, u.ux, u.uy);
        stackobj(obj);
        if (Blind && Levitation)
            map_object(obj, 0, FALSE);
        newsym(u.ux, u.uy);     /* remap location under self */
    }
}

/* things that must change when not held; recurse into containers.
   Called for both player and monsters */
void
obj_no_longer_held(struct obj *obj)
{
    if (!obj) {
        return;
    } else if ((Is_container(obj) || obj->otyp == STATUE) && obj->cobj) {
        struct obj *contents;

        for (contents = obj->cobj; contents; contents = contents->nobj)
            obj_no_longer_held(contents);
    }
    switch (obj->otyp) {
    case CRYSKNIFE:
        /* KMH -- Fixed crysknives have only 10% chance of reverting */
        /* only changes when not held by player or monster */
        if (!obj->oerodeproof || !rn2_on_rng(10, rng_unfix_crysknife)) {
            obj->otyp = WORM_TOOTH;
            obj->oerodeproof = 0;
        }
        break;
    }
}

/* 'D' command: drop several things */
int
doddrop(const struct nh_cmd_arg *arg)
{
    int result = 0;

    (void) arg;

    add_valid_menu_class(0);    /* clear any classes already there */
    if (*u.ushops)
        sellobj_state(SELL_DELIBERATE);
    result = menu_drop(result);
    if (*u.ushops)
        sellobj_state(SELL_NORMAL);

    return result;
}

/* Drop things from the hero's inventory, using a menu. */
static int
menu_drop(int retry)
{
    int n, i, n_dropped = 0;
    long cnt;
    struct obj *otmp, *otmp2;
    const int *pick_list;
    struct object_pick *obj_pick_list;
    boolean all_categories = TRUE;
    boolean drop_everything = FALSE;

    if (retry) {
        all_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
        all_categories = FALSE;
        n = query_category("Drop what type of items?", invent,
                           UNPAID_TYPES | ALL_TYPES | CHOOSE_ALL | UNIDENTIFIED
                           | BUC_BLESSED | BUC_CURSED | BUC_UNCURSED |
                           BUC_UNKNOWN, &pick_list, PICK_ANY);
        if (!n)
            goto drop_done;
        for (i = 0; i < n; i++) {
            if (pick_list[i] == ALL_TYPES_SELECTED)
                all_categories = TRUE;
            else if (pick_list[i] == 'A')
                drop_everything = TRUE;
            else
                add_valid_menu_class(pick_list[i]);
        }
    }

    if (drop_everything) {
        for (otmp = invent; otmp; otmp = otmp2) {
            otmp2 = otmp->nobj;
            otmp->was_dropped = 1;
            n_dropped += drop(otmp);
        }
    } else {
        /* should coordinate with perm invent, maybe not show worn items */
        n = query_objlist("What would you like to drop?", invent,
                          USE_INVLET | INVORDER_SORT, &obj_pick_list, PICK_ANY,
                          all_categories ? allow_all : allow_category);
        if (n > 0) {
            for (i = 0; i < n; i++) {
                otmp = obj_pick_list[i].obj;
                cnt = obj_pick_list[i].count;
                if (cnt < otmp->quan) {
                    if (welded(otmp)) {
                        ;       /* don't split */
                    } else if (otmp->otyp == LOADSTONE && otmp->cursed) {
                        /* same kludge as getobj(), for canletgo()'s use */
                        otmp->corpsenm = (int)cnt;      /* don't split */
                    } else
                        otmp = splitobj(otmp, cnt);
                }
                otmp->was_dropped = 1;
                n_dropped += drop(otmp);
            }
            free(obj_pick_list);
        }
    }

drop_done:
    return n_dropped;
}


/* on a ladder, used in goto_level */
static boolean at_ladder = FALSE;

int
dodown(boolean autodig_ok)
{
    struct trap *trap = 0;
    boolean stairs_down =
        ((u.ux == level->dnstair.sx && u.uy == level->dnstair.sy) ||
         (u.ux == level->sstairs.sx && u.uy == level->sstairs.sy &&
          !level->sstairs.up));
    boolean ladder_down = (u.ux == level->dnladder.sx &&
                           u.uy == level->dnladder.sy);

    if (u.usteed && !u.usteed->mcanmove) {
        pline("%s won't move!", Monnam(u.usteed));
        return 0;
    } else if (u.usteed && u.usteed->meating) {
        pline("%s is still eating.", Monnam(u.usteed));
        return 0;
    } else if (Levitation) {
        unsigned controlled_lev = u_have_property(
            LEVITATION, W_MASK(os_invoked) | W_MASK(os_special), FALSE);
        if (controlled_lev) {
            /* end controlled levitation */
            if (controlled_lev & W_MASK(os_invoked)) {
                struct obj *obj;
                
                for (obj = invent; obj; obj = obj->nobj) {
                    if (obj->oartifact && artifact_has_invprop(obj, LEVITATION))
                        uninvoke_artifact(obj);
                }
            }
            if (float_down(I_SPECIAL | TIMEOUT))
                return 1;       /* came down, so moved */
        }
        if (level->locations[u.ux][u.uy].seenv &&
            level->locations[u.ux][u.uy].typ != S_stone) {
            boolean known_stairs = stairs_down &&
                level->locations[u.ux][u.uy].typ == S_dnstair;
            boolean known_ladder = ladder_down &&
                level->locations[u.ux][u.uy].typ == S_dnladder;
            floating_above(known_stairs ? "stairs" : known_ladder ? "ladder" :
                           surface(u.ux, u.uy));
        } else {
            pline("You are floating high in the air.");
        }
        return 0;       /* didn't move */
    }
    if (!stairs_down && !ladder_down) {
        boolean can_fall;

        trap = t_at(level, u.ux, u.uy);
        can_fall = trap && (trap->ttyp == TRAPDOOR || trap->ttyp == HOLE);
        if (!trap ||
            (trap->ttyp != TRAPDOOR && trap->ttyp != HOLE && trap->ttyp != PIT
             && trap->ttyp != SPIKED_PIT)
            || (!can_fall_thru(level) && can_fall) || !trap->tseen) {

            if (flags.autodig && autodig_ok && flags.autodigdown &&
                flags.occupation == occ_none && uwep && is_pick(uwep)) {
                struct nh_cmd_arg arg;
                arg_from_delta(0, 0, 1, &arg);
                return use_pick_axe(uwep, &arg);
            } else {
                pline("You can't go down here.");
                return 0;
            }
        }
    }
    if (u.ustuck) {
        pline("You are %s, and cannot go down.",
              !Engulfed ? "being held" :
              is_animal(u.ustuck->data) ? "swallowed" : "engulfed");
        return 1;
    }
    if (on_level(&valley_level, &u.uz) && !u.uevent.gehennom_entered) {
        pline("You are standing at the gate to Gehennom.");
        pline("Unspeakable cruelty and harm lurk down there.");
        if (yn("Are you sure you want to enter?") != 'y')
            return 0;
        else {
            if (Hallucination)
                pline("Please insert Disc 2.");
            else
                pline("So be it.");
        }
        u.uevent.gehennom_entered = 1;  /* don't ask again */
    }

    if (!next_to_u()) {
        pline("You are held back by your pet!");
        return 0;
    }

    if (trap) {
        if (trap->ttyp == PIT || trap->ttyp == SPIKED_PIT) {
            if (u.utrap && (u.utraptype == TT_PIT)) {
                if (flags.autodig && autodig_ok && flags.autodigdown &&
                    flags.occupation == occ_none && uwep && is_pick(uwep)) {
                    struct nh_cmd_arg arg;
                    arg_from_delta(0, 0, 1, &arg);
                    return use_pick_axe(uwep, &arg);
                } else {
                    pline("You are already in the pit.");      /* YAFM needed */
                }
            } else {
                u.utrap = 1;
                u.utraptype = TT_PIT;
                pline("You %s down into the pit.",
                      locomotion(youmonst.data, "go"));
                turnstate.vision_full_recalc = TRUE;
            }
            return 0;
        } else {
            pline("You %s %s.", locomotion(youmonst.data, "jump"),
                  trap->ttyp ==
                  HOLE ? "down the hole" : "through the trap door");
        }
    }

    if (trap && Is_stronghold(&u.uz)) {
        goto_hell(FALSE, TRUE);
    } else {
        at_ladder = (boolean) (level->locations[u.ux][u.uy].typ == LADDER);
        next_level(!trap);
        at_ladder = FALSE;
    }
    return 1;
}

int
doup(void)
{
    if ((u.ux != level->upstair.sx || u.uy != level->upstair.sy)
        && (u.ux != level->upladder.sx || u.uy != level->upladder.sy)
        && (u.ux != level->sstairs.sx || u.uy != level->sstairs.sy ||
            !level->sstairs.up)) {
        pline("You can't go up here.");
        return 0;
    }
    if (u.usteed && !u.usteed->mcanmove) {
        pline("%s won't move!", Monnam(u.usteed));
        return 0;
    } else if (u.usteed && u.usteed->meating) {
        pline("%s is still eating.", Monnam(u.usteed));
        return 0;
    } else if (u.ustuck) {
        pline("You are %s, and cannot go up.",
              !Engulfed ? "being held" :
              is_animal(u.ustuck->data) ? "swallowed" : "engulfed");
        return 1;
    }
    if (near_capacity() > SLT_ENCUMBER) {
        /* No levitation check; inv_weight() already allows for it */
        pline("Your load is too heavy to climb the %s.",
              level->locations[u.ux][u.uy].typ == STAIRS ? "stairs" : "ladder");
        return 1;
    }
    if (ledger_no(&u.uz) == 1) {
        if (yn("Beware, there will be no return! Still climb?") != 'y')
            return 0;
    }
    if (!next_to_u()) {
        pline("You are held back by your pet!");
        return 0;
    }
    at_ladder = (boolean) (level->locations[u.ux][u.uy].typ == LADDER);
    prev_level(TRUE);
    at_ladder = FALSE;
    return 1;
}


void
notify_levelchange(const d_level * dlev)
{
    int mode;
    const d_level *z = dlev ? dlev : &u.uz;

    if (In_hell(z))
        mode = LDM_HELL;
    else if (Is_qstart(z))
        mode = LDM_QUESTHOME;
    else if (Is_qlocate(z))
        mode = LDM_QUESTLOCATE;
    else if (Is_nemesis(z))
        mode = LDM_QUESTGOAL;
    else if (In_quest(z) && z->dlevel > qlocate_level.dlevel)
        mode = LDM_QUESTFILL2;
    else if (In_quest(z))
        mode = LDM_QUESTFILL1;
    else if (In_mines(z))
        mode = LDM_MINES;
    else if (In_sokoban(z))
        mode = LDM_SOKOBAN;
    else if (Is_rogue_level(z))
        mode = LDM_ROGUE;
    else if (Is_knox(z))
        mode = LDM_KNOX;
    else if (Is_astralevel(z))
        mode = LDM_ASTRAL;
    else
        mode = LDM_DEFAULT;
    /* TODO: Vlad's tower */

    level_changed(mode);
}


void
goto_level(d_level * newlevel, boolean at_stairs, boolean falling,
           boolean portal)
{
    xchar new_ledger;
    boolean up = (depth(newlevel) < depth(&u.uz)), newdungeon =
        (u.uz.dnum != newlevel->dnum), was_in_W_tower =
        In_W_tower(u.ux, u.uy, &u.uz), familiar = FALSE;
    boolean new = FALSE;        /* made a new level? */
    struct monst *mtmp, *mtmp2;
    struct obj *otmp;
    struct level *origlev;
    boolean at_trapdoor = ((t_at(level, u.ux, u.uy)) &&
                           (t_at(level, u.ux, u.uy))->ttyp == TRAPDOOR);
    d_level orig_d;

    if (dunlev(newlevel) > dunlevs_in_dungeon(newlevel))
        newlevel->dlevel = dunlevs_in_dungeon(newlevel);
    if (newdungeon && In_endgame(newlevel)) {   /* 1st Endgame Level !!! */
        if (Uhave_amulet)
            assign_level(newlevel, &earth_level);
        else
            return;
    }
    new_ledger = ledger_no(newlevel);
    if (new_ledger <= 0) {
        /* The caller should be doing this themselves. */
        impossible("Escaping via goto_level?");
        done(ESCAPED, NULL);
    }

    /*
     * If you have the amulet and are trying to get out of Gehennom, going up a
     * set of stairs sometimes does some very strange things! Biased against
     * law and towards chaos, but not nearly as strongly as it used to be
     * (prior to 3.2.0). Additionally, the documentation as of 3.2.0 didn't
     * match the actual distribution:
     *
     *  pre-3.20 (documented)      3.2.0 (documented)         3.2.0 (actual)
     * "up"   L     N     C     "up"   L     N     C     "up"   L     N     C
     *  +1  75.00 75.00 75.00    +1  75.00 75.00 75.00    +1  75.00 75.00 75.00
     *   0   0.00 12.50 25.00     0   6.25  8.33 12.50     0   6.25  8.33 12.50
     *  -1   8.33  4.17  0.00    -1   6.25  8.33 12.50    -1  11.45 12.50 12.50
     *  -2   8.33  4.17  0.00    -2   6.25  8.33  0.00    -2   5.21  4.17  0.00
     *  -3   8.33  4.17  0.00    -3   6.25  0.00  0.00    -3   2.08  0.00  0.00
     *
     * 4.3.0 reproduces the actual distribution from 3.2.0 and later (i.e. the
     * rightmost table above).
     */
    if (Inhell && up && Uhave_amulet && !newdungeon && !portal &&
        (dunlev(&u.uz) < dunlevs_in_dungeon(&u.uz) - 3)) {
        if (!rn2_on_rng(4, rng_mysterious_force)) {
            /* Note: to keep the RNG behaviour the same between different
               games on the same seed (with potentially different alignments),
               we generate a distance to send back in the range 0 to 11, then
               scale down to 0..1, 0..2, or 0..3 as appropriate;
               assign_rnd_level uses one RNG, so burn an RNG to match if we're
               not calling it */
            int odds = 3 + (int)u.ualign.type,  /* 2..4 */
                diff = odds <= 1 ? 0 /* paranoia */
                : rn2_on_rng(12, rng_mysterious_force) / (12 / odds);

            if (diff != 0) {
                assign_rnd_level(newlevel, &u.uz, diff);
                /* if inside the tower, stay inside */
                if (was_in_W_tower && !On_W_tower_level(newlevel))
                    diff = 0;
            } else
                rn2_on_rng(12, rng_mysterious_force); /* burn a random number */

            if (diff == 0)
                assign_level(newlevel, &u.uz);

            new_ledger = ledger_no(newlevel);

            pline("A mysterious force momentarily surrounds you...");
            if (on_level(newlevel, &u.uz)) {
                safe_teleds(FALSE);
                next_to_u();
                return;
            } else
                at_stairs = at_ladder = FALSE;
        }
    }

    /* Prevent the player from going past the first quest level unless (s)he
       has been given the go-ahead by the leader. */
    if (on_level(&u.uz, &qstart_level) && !newdungeon && !ok_to_quest(TRUE))
        return;

    if (on_level(newlevel, &u.uz))
        return; /* this can happen */

    if (falling)        /* assuming this is only trap door or hole */
        impact_drop(NULL, u.ux, u.uy, newlevel->dlevel);

    check_special_room(TRUE);   /* probably was a trap door */
    if (Punished)
        unplacebc();
    u.utrap = 0;        /* needed in level_tele */
    fill_pit(level, u.ux, u.uy);
    u.ustuck = 0;       /* idem */
    u.uinwater = 0;
    u.uundetected = 0;  /* not hidden, even if means are available */
    keepdogs(FALSE);
    if (Engulfed)     /* idem */
        u.uswldtim = Engulfed = 0;
    /* 
     *  We no longer see anything on the level->  Make sure that this
     *  follows Engulfed set to null since it overrides all normal vision.
     */
    vision_recalc(2);

    if (level->flags.purge_monsters) {
        /* purge any dead monsters */
        dmonsfree(level);
    }
    update_mlstmv();    /* current monsters are becoming inactive */


    assign_level(&orig_d, &u.uz);
    assign_level(&u.uz, newlevel);

    /* If the entry level is the top level, then the dungeon goes down.
       Otherwise it goes up. */
    if (find_dungeon(&u.uz).entry_lev == 1) {
        if (dunlev_reached(&u.uz) < dunlev(&u.uz))
            dunlev_reached(&u.uz) = dunlev(&u.uz);
    } else {
        if (dunlev_reached(&u.uz) > dunlev(&u.uz) || !dunlev_reached(&u.uz))
            dunlev_reached(&u.uz) = dunlev(&u.uz);
    }

    reset_rndmonst(NON_PM);     /* u.uz change affects monster generation */

    origlev = level;
    level = NULL;

    if (!levels[new_ledger]) {
        /* entering this level for first time; make it now */
        historic_event(FALSE, "reached %s.", hist_lev_name(&u.uz, FALSE));
        level = mklev(&u.uz);
        new = TRUE;     /* made the level */
    } else {
        /* returning to previously visited level */
        level = levels[new_ledger];

        /* regenerate animals while on another level */
        for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon;
            if (moves > level->lastmoves)
                mon_catchup_elapsed_time(mtmp, moves - level->lastmoves);

            /* update shape-changers in case protection against them is
               different now */
            restore_cham(mtmp);
        }
    }

    /* some timers and lights might need to be transferred to the new level if
       they are attached to objects the hero is carrying */
    transfer_timers(origlev, level, 0);
    transfer_lights(origlev, level, 0);

    /* do this prior to level-change pline messages */
    vision_reset();     /* clear old level's line-of-sight */
    /* don't let that reenable vision yet */
    turnstate.vision_full_recalc = FALSE; 
    flush_screen_disable();     /* ensure all map flushes are postponed */

    if (portal && !In_endgame(&u.uz)) {
        /* find the portal on the new level */
        struct trap *ttrap;

        for (ttrap = level->lev_traps; ttrap; ttrap = ttrap->ntrap)
            if (ttrap->ttyp == MAGIC_PORTAL)
                break;

        if (!ttrap)
            panic("goto_level: no corresponding portal!");
        seetrap(ttrap);
        u_on_newpos(ttrap->tx, ttrap->ty);
    } else if (at_stairs && !In_endgame(&u.uz)) {
        if (up) {
            if (at_ladder) {
                u_on_newpos(level->dnladder.sx, level->dnladder.sy);
            } else {
                if (newdungeon) {
                    if (Is_stronghold(&u.uz)) {
                        xchar x, y;

                        do {
                            x = (COLNO - 2 - rnd(5));
                            y = rn1(ROWNO - 4, 3);
                        } while (occupied(level, x, y) ||
                                 IS_WALL(level->locations[x][y].typ));
                        u_on_newpos(x, y);
                    } else
                        u_on_sstairs();
                } else
                    u_on_dnstairs();
            }
            /* Remove bug which crashes with levitation/punishment KAA */
            if (Punished && !Levitation) {
                pline("With great effort you climb the %s.",
                      at_ladder ? "ladder" : "stairs");
            }
        } else {        /* down */
            if (at_ladder) {
                u_on_newpos(level->upladder.sx, level->upladder.sy);
            } else {
                if (newdungeon)
                    u_on_sstairs();
                else
                    u_on_upstairs();
            }
            if (at_stairs && Flying)
                pline("You fly down along the %s.",
                      at_ladder ? "ladder" : "stairs");
            else if (at_stairs &&
                     (near_capacity() > UNENCUMBERED || Punished || Fumbling)) {
                pline("You fall down the %s.", at_ladder ? "ladder" : "stairs");
                if (Punished) {
                    drag_down();
                    if (carried(uball)) {
                        unwield_silently(uball);
                        freeinv(uball);
                    }
                }
                /* falling off steed has its own losehp() call */
                if (u.usteed)
                    dismount_steed(DISMOUNT_FELL);
                else
                    losehp(rnd(3), killer_msg(DIED, "falling downstairs"));
                selftouch("Falling, you", "falling downstairs while wielding");
            }
        }
    } else {    /* trap door or level_tele or In_endgame */
        if (was_in_W_tower && On_W_tower_level(&u.uz))
            /* Stay inside the Wizard's tower when feasible. */
            /* Note: up vs down doesn't really matter in this case. */
            place_lregion(level, level->dndest.nlx, level->dndest.nly,
                          level->dndest.nhx, level->dndest.nhy, COLNO, ROWNO, COLNO, ROWNO,
                          LR_DOWNTELE, NULL);
        else if (up)
            place_lregion(level, level->updest.lx, level->updest.ly,
                          level->updest.hx, level->updest.hy, level->updest.nlx,
                          level->updest.nly, level->updest.nhx,
                          level->updest.nhy, LR_UPTELE, NULL);
        else
            place_lregion(level, level->dndest.lx, level->dndest.ly,
                          level->dndest.hx, level->dndest.hy, level->dndest.nlx,
                          level->dndest.nly, level->dndest.nhx,
                          level->dndest.nhy, LR_DOWNTELE, NULL);
        if (falling) {
            const char *kbuf;

            kbuf = msgprintf("falling through a %s while wielding",
                             at_trapdoor ? "trap door" : "hole");
            if (Punished)
                ballfall();
            selftouch("Falling, you", kbuf);
        }
    }

    if (Punished)
        placebc();

    /* only matters if falling; place objects that fell with the player nearby
       */
    for (otmp = turnstate.migrating_objs; otmp; ) {
        struct obj *otmp2 = otmp;
        otmp = otmp2->nobj;

        deliver_object(otmp2, u.uz.dnum, u.uz.dlevel, MIGR_NEAR_PLAYER);
    }

    for (otmp = invent; otmp; otmp = otmp->nobj)
        set_obj_level(level, otmp);
    losedogs();
    kill_genocided_monsters();  /* for those wiped out while in limbo */
    /* 
     * Expire all timers that have gone off while away.  Must be
     * after migrating monsters and objects are delivered
     * (losedogs and obj_delivery).
     */
    run_timers();

    initrack();

    if ((mtmp = m_at(level, u.ux, u.uy)) != 0 && mtmp != u.usteed) {
        /* There's a monster at your target destination; it might be one which
           accompanied you--see mon_arrive(dogmove.c)--or perhaps it was
           already here.  Randomly move you to an adjacent spot or else the
           monster to any nearby location.  Prior to 3.3.0 the latter was done
           unconditionally. */
        coord cc;

        if (!rn2(2) && enexto(&cc, level, u.ux, u.uy, youmonst.data) &&
            distu(cc.x, cc.y) <= 2)
            u_on_newpos(cc.x, cc.y);    /* [maybe give message here?] */
        else
            mnexto(mtmp);

        if ((mtmp = m_at(level, u.ux, u.uy)) != 0) {
            /* there was an unconditional impossible("mnearto failed")
                here, but it's not impossible and we're prepared to cope
                with the situation, so only say something when debugging */
            if (wizard) pline("(monster in hero's way)");

            if (!rloc(mtmp, TRUE))
                /* no room to move it; send it away, to return later */
                migrate_to_level(mtmp, ledger_no(&u.uz), MIGR_RANDOM, NULL);
        }
    }

    /* Stop autoexplore revisiting the entrance stairs. */
    level->locations[u.ux][u.uy].mem_stepped = 1;

    /* initial movement of bubbles just before vision_recalc */
    if (Is_waterlevel(&u.uz))
        movebubbles();

    if (level->flags.forgotten) {
        familiar = TRUE;
        level->flags.forgotten = FALSE;
    }

    notify_levelchange(NULL);   /* inform window code of the level change */

    /* Reset the screen. */
    vision_reset();     /* reset the blockages */
    doredraw(); /* does a full vision recalc */
    flush_screen_enable();
    flush_screen();

    /* 
     *  Move all plines beyond the screen reset.
     */

    /* give room entrance message, if any */
    check_special_room(FALSE);

    /* Check whether we just entered Gehennom. */
    if (!In_hell(&orig_d) && Inhell) {
        if (Is_valley(&u.uz)) {
            pline("You arrive at the Valley of the Dead...");
            pline("The odor of burnt flesh and decay pervades the air.");
            You_hear("groans and moans everywhere.");
        } else
            pline("It is hot here.  You smell smoke...");
    }

    if (familiar) {
        static const char *const fam_msgs[4] = {
            "You have a sense of deja vu.",
            "You feel like you've been here before.",
            "This place %s familiar...",
            0   /* no message */
        };
        static const char *const halu_fam_msgs[4] = {
            "Whoa!  Everything %s different.",
            "You are surrounded by twisty little passages, all alike.",
            "Gee, this %s like uncle Conan's place...",
            0   /* no message */
        };
        
        const char *mesg;
        int which = rn2(4);

        if (Hallucination)
            mesg = halu_fam_msgs[which];
        else
            mesg = fam_msgs[which];
        if (mesg && strchr(mesg, '%')) {
            mesg = (msgprintf)(mesg, !Blind ? "looks" : "seems");
        }
        if (mesg)
            pline("%s", mesg);
    }

    if (new && Is_rogue_level(&u.uz))
        pline("You enter what seems to be an older, more primitive world.");
    /* Final confrontation */
    if (In_endgame(&u.uz) && newdungeon && Uhave_amulet)
        resurrect();
    if (newdungeon && In_V_tower(&u.uz) && In_hell(&orig_d))
        pline("The heat and smoke are gone.");

    /* the message from your quest leader */
    if (!In_quest(&orig_d) && at_dgn_entrance(&u.uz, "The Quest") &&
        !(u.uevent.qexpelled || u.uevent.qcompleted ||
          u.quest_status.leader_is_dead)) {

        if (u.uevent.qcalled) {
            com_pager(Role_if(PM_ROGUE) ? 4 : 3);
        } else {
            com_pager(2);
            u.uevent.qcalled = TRUE;
        }
    }

    /* once Croesus is dead, his alarm doesn't work any more */
    if (Is_knox(&u.uz) && (new || !mvitals[PM_CROESUS].died)) {
        pline("You penetrated a high security area!");
        pline("An alarm sounds!");
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
            if (!DEADMONSTER(mtmp) && mtmp->msleeping)
                mtmp->msleeping = 0;
    }

    if (on_level(&u.uz, &astral_level))
        final_level();
    else
        onquest(&orig_d);

    if (*level->levname)
        pline("You named this level: %s.", level->levname);

    /* assume this will always return TRUE when changing level */
    reset_occupations(TRUE);    /* you moved */
    in_out_region(level, u.ux, u.uy);
    pickup(1, flags.interaction_mode);
}


/* This runs as the player arrives on Astral. At this point, the Astral level
   RNG is in a consistent state between all games. We use it as far as it keeps
   a consistent number of seeds, then switch to the main RNG. */
static void
final_level(void)
{
    struct monst *mtmp;
    struct obj *otmp;
    coord mm;
    int i;
    enum rng astral_rng = rng_for_level(&level->z);

    /* reset monster hostility relative to player */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp))
            reset_hostility(mtmp);

    /* create some player-monsters */
    create_mplayers(rn2_on_rng(4, astral_rng) + 3, TRUE);

    /* generate angel count and guardian xlvl unconditionally */
    int angel_count = rn2_on_rng(4, astral_rng) + 1;
    int angel_xlvl  = rn2_on_rng(8, astral_rng) + 15;

    /* create a guardian angel next to player, if worthy */
    if (Conflict) {
        /* unworthy: generate one angel on astral_rng (for consistency), others
           on the main RNG */
        pline("A voice booms: \"Thy desire for conflict shall be fulfilled!\"");
        for (i = angel_count; i > 0; --i) {
            mm.x = u.ux;
            mm.y = u.uy;
            if (enexto(&mm, level, mm.x, mm.y, &mons[PM_ANGEL]))
                mk_roamer(&mons[PM_ANGEL], u.ualign.type, level, mm.x, mm.y,
                          FALSE, i == angel_count ? MM_ALLLEVRNG : NO_MM_FLAGS);
        }

    } else if (u.ualign.record > 8) {   /* fervent */
        /* worthy: generate one angel on astral_rng and buff it using the main
           RNG (except for m_lev which is relevant to balance), no others */
        pline("A voice whispers: \"Thou hast been worthy of me!\"");
        mm.x = u.ux;
        mm.y = u.uy;
        if (enexto(&mm, level, mm.x, mm.y, &mons[PM_ANGEL])) {
            if (((mtmp = mk_roamer(&mons[PM_ANGEL], u.ualign.type, level,
                                   mm.x, mm.y, TRUE, MM_ALLLEVRNG)))) {
                if (!Blind)
                    pline("An angel appears near you.");
                else
                    pline
                        ("You feel the presence of a friendly angel near you.");
                /* guardian angel -- the one case mtame doesn't imply an edog
                   structure, so we don't want to call tamedog(). */
                mtmp->mtame = 10;
                /* make him strong enough vs. endgame foes */
                mtmp->m_lev = angel_xlvl;
                mtmp->mhp = mtmp->mhpmax =
                    dice((int)mtmp->m_lev, 10) + 30 + rnd(30);
                if ((otmp = select_hwep(mtmp)) == 0) {
                    otmp = mksobj(level, SILVER_SABER, FALSE, FALSE, rng_main);
                    if (mpickobj(mtmp, otmp))
                        panic("merged weapon?");
                }
                bless(otmp);
                if (otmp->spe < 4)
                    otmp->spe += rnd(4);
                if ((otmp = which_armor(mtmp, os_arms)) == 0 ||
                    otmp->otyp != SHIELD_OF_REFLECTION) {
                    mongets(mtmp, AMULET_OF_REFLECTION, rng_main);
                    m_dowear(mtmp, TRUE);
                }
            }
        }
    }
}

/* change levels at the end of this turn, after monsters finish moving */
void
schedule_goto(d_level * tolev, boolean at_stairs, boolean falling,
              int portal_flag, const char *pre_msg, const char *post_msg)
{
    int typmask = 0100; /* non-zero triggers `deferred_goto' */

    /* destination flags (`goto_level' args) */
    if (at_stairs)
        typmask |= 1;
    if (falling)
        typmask |= 2;
    if (portal_flag)
        typmask |= 4;
    if (portal_flag < 0)
        typmask |= 0200;        /* flag for portal removal */
    turnstate.goto_info.flags = typmask;
    /* destination level */
    assign_level(&turnstate.goto_info.dlevel, tolev);

    if (pre_msg)
        strncpy(turnstate.goto_info.pre_msg, pre_msg, BUFSZ);
    if (post_msg)
        strncpy(turnstate.goto_info.post_msg, post_msg, BUFSZ);
}

/* handle something like portal ejection */
boolean
deferred_goto(void)
{
    if (!turnstate.goto_info.flags)
        return FALSE;

    boolean retval = FALSE;
    d_level dest;
    assign_level(&dest, &turnstate.goto_info.dlevel);

    if (!on_level(&u.uz, &dest)) {
        int typmask = turnstate.goto_info.flags;

        if (*turnstate.goto_info.pre_msg)
            pline("%s", turnstate.goto_info.pre_msg);

        goto_level(&dest, ! !(typmask & 1), ! !(typmask & 2), ! !(typmask & 4));
        retval = TRUE;

        if (typmask & 0200) {   /* remove portal */
            struct trap *t = t_at(level, u.ux, u.uy);

            if (t) {
                deltrap(level, t);
                newsym(u.ux, u.uy);
            }
        }

        if (*turnstate.goto_info.post_msg)
            pline("%s", turnstate.goto_info.post_msg);
    }

    turnstate.goto_info.dlevel.dnum = turnstate.goto_info.dlevel.dlevel = -1;
    turnstate.goto_info.flags = 0;
    turnstate.goto_info.pre_msg[0] = '\0';
    turnstate.goto_info.post_msg[0] = '\0';

    return retval;
}


/*
 * Return TRUE if we created a monster for the corpse.  If successful, the
 * corpse is gone.
 */
boolean
revive_corpse(struct obj *corpse)
{
    struct monst *mtmp, *mcarry;
    boolean is_uwep, chewed;
    xchar where;
    const char *cname;
    struct obj *container = NULL;
    int container_where = 0;

#ifdef INVISIBLE_OBJECTS
    boolean vis = !corpse->oinvis || See_invisible;
#endif

    where = corpse->where;
    is_uwep = corpse == uwep;
    cname = corpse_xname(corpse, TRUE); 
    mcarry = (where == OBJ_MINVENT) ? corpse->ocarry : 0;

    if (where == OBJ_CONTAINED) {
        struct monst *mtmp2 = NULL;

        container = corpse->ocontainer;
        mtmp2 = get_container_location(container, &container_where, NULL);
        /* container_where is the outermost container's location even if nested 
         */
        if (container_where == OBJ_MINVENT && mtmp2)
            mcarry = mtmp2;
    }
    mtmp = revive(corpse);      /* corpse is gone if successful */

    if (mtmp) {
        chewed = (mtmp->mhp < mtmp->mhpmax);
        if (chewed)
            cname = msgcat("bite-covered ", cname);
        switch (where) {
        case OBJ_INVENT:
            if (is_uwep)
                pline("The %s writhes out of your grasp!", cname);
            else
                pline("You feel squirming in your backpack!");
            break;

        case OBJ_FLOOR:
            if (cansee(mtmp->mx, mtmp->my)
#ifdef INVISIBLE_OBJECTS
                && vis
#endif
                )
                pline("%s rises from the dead!",
                      chewed ? Adjmonnam(mtmp, "bite-covered") : Monnam(mtmp));
            break;

        case OBJ_MINVENT:      /* probably a nymph's */
            if (cansee(mtmp->mx, mtmp->my)) {
                /* OK for long worms because we checked the location already */
                if (canseemon(mcarry))
                    pline("Startled, %s drops %s as it revives!",
                          mon_nam(mcarry), an(cname));
                else
                    pline("%s suddenly appears!",
                          chewed ? Adjmonnam(mtmp,
                                             "bite-covered") : Monnam(mtmp));
            }
            break;
        case OBJ_CONTAINED:
            if (container_where == OBJ_MINVENT && cansee(mtmp->mx, mtmp->my) &&
                mcarry && canseemon(mcarry) && container) {
                const char *sackname;

                sackname = msgprintf("%s %s", s_suffix(mon_nam(mcarry)),
                                     xname(container));
                pline("%s writhes out of %s!", Amonnam(mtmp), sackname);
            } else if (container_where == OBJ_INVENT && container) {
                pline("%s %s out of %s in your pack!",
                      Blind ? "Something" : Amonnam(mtmp),
                      locomotion(mtmp->data, "writhes"), an(xname(container)));
            } else if (container_where == OBJ_FLOOR && container &&
                       cansee(mtmp->mx, mtmp->my)) {
                pline("%s escapes from %s!", Amonnam(mtmp),
                      an(xname(container)));
            }
            break;
        default:
            /* we should be able to handle the other cases... */
            impossible("revive_corpse: lost corpse @ %d", where);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* Revive the corpse via a timeout. */
void
revive_mon(void *arg, long timeout)
{
    struct obj *body = (struct obj *)arg;

    (void) timeout;

    /* if we succeed, the corpse is gone, otherwise, rot it away */
    if (!revive_corpse(body)) {
        if (is_rider(&mons[body->corpsenm]))
            pline("You feel less hassled.");
        start_timer(body->olev, 250L - (moves - body->age), TIMER_OBJECT,
                    ROT_CORPSE, arg);
    }
}

int
donull(const struct nh_cmd_arg *arg)
{
    limited_turns(arg, occ_wait);
    return 1;   /* Do nothing, but let other things happen */
}

int
dowipe(const struct nh_cmd_arg *arg)
{
    (void) arg;

    if (!u.ucreamed) {
        pline("Your %s is already clean.", body_part(FACE));
        return 0;
    }

    if (u.ucreamed < 4)
        u.ucreamed = 0;
    else
        u.ucreamed -= 4;
    if (Blinded < 4)
        Blinded = 0;
    else
        Blinded -= 4;

    if (!Blinded) {

        pline("You've got the glop off.");
        u.ucreamed = 0;
        Blinded = 1;
        make_blinded(0L, TRUE);
        return 1;

    } else if (!u.ucreamed) {

        pline("Your %s feels clean now.", body_part(FACE));
        return 1;

    } else {

        const char *buf = msgprintf("wiping off your %s", body_part(FACE));
        action_incomplete(buf, occ_wipe);
        return 1;
    }
}

void
set_wounded_legs(long side, int timex)
{
    /* KMH If you are riding, your steed gets the wounded legs instead. You
       still call this function, but don't lose hp. Caller is also responsible
       for adjusting messages. */

    if (!Wounded_legs)
        ATEMP(A_DEX)--;

    if (side & LEFT_SIDE)
        if (!LWounded_legs || (LWounded_legs & TIMEOUT))
            LWounded_legs = timex;

    if (side & RIGHT_SIDE)
        if (!RWounded_legs || (RWounded_legs & TIMEOUT))
            RWounded_legs = timex;

    encumber_msg();
}

/* Note: call only if the legs are/were actually wounded. This function can
   be called after the LWounded_legs/RWounded_legs functions time out to
   zero, so it can't check the variables themselves; the caller has to do
   that, if it didn't just decrement them.

   Argument is LEFT_SIDE, RIGHT_SIDE, or LEFT_SIDE|RIGHT_SIDE. */
void
heal_legs(int side)
{
    if ((!(side & LEFT_SIDE) && LWounded_legs) ||
        (!(side & RIGHT_SIDE) && RWounded_legs)) {
        /* Heal one leg. */
        if (!u.usteed)
            pline("One of your %s feels somewhat better.",
                  makeplural(body_part(LEG)));
        if (side == LEFT_SIDE) LWounded_legs = 0;
        if (side == RIGHT_SIDE) RWounded_legs = 0;
        return;
    }

    /* Heal both legs. */

    if (ATEMP(A_DEX) < 0)
        ATEMP(A_DEX)++;

    if (!u.usteed) {
        /* KMH, intrinsics patch */
        if (side == (LEFT_SIDE | RIGHT_SIDE)) {
            pline("Your %s feel somewhat better.",
                  makeplural(body_part(LEG)));
        } else {
            pline("Your %s feels somewhat better.", body_part(LEG));
        }
    }
    LWounded_legs = RWounded_legs = 0;
    encumber_msg();
}
