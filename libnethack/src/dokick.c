/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Izchak Miller, Mike Stephenson, Steve Linhart, 1989. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "eshk.h"

#define is_bigfoot(x)   ((x) == &mons[PM_SASQUATCH])
#define martial()       (martial_bonus() || is_bigfoot(youmonst.data) || \
                         (uarmf && uarmf->otyp == KICKING_BOOTS))

static struct rm *maploc;
static const char *gate_str;

static void kickdmg(struct monst *, boolean, schar, schar);
static enum attack_check_status kick_monster(xchar, xchar, schar, schar);
static int kick_object(xchar, xchar, schar, schar, struct obj **);
static const char *kickstr(struct obj *);
static void otransit_msg(struct obj *, boolean, long);
static void drop_to(coord *, schar);

static const char kick_passes_thru[] = "kick passes harmlessly through";

static void
kickdmg(struct monst *mon, boolean clumsy, schar dx, schar dy)
{
    int mdx, mdy;
    int dmg = (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 15;
    int kick_skill = P_NONE;
    int blessed_foot_damage = 0;
    boolean trapkilled = FALSE;

    if (uarmf && uarmf->otyp == KICKING_BOOTS)
        dmg += 5;

    /* excessive wt affects dex, so it affects dmg */
    if (clumsy)
        dmg /= 2;

    /* kicking a dragon or an elephant will not harm it */
    if (thick_skinned(mon->data))
        dmg = 0;

    /* attacking a shade is useless */
    if (mon->data == &mons[PM_SHADE])
        dmg = 0;

    if ((is_undead(mon->data) || is_demon(mon->data)) && uarmf &&
        uarmf->blessed)
        blessed_foot_damage = 1;

    if (mon->data == &mons[PM_SHADE] && !blessed_foot_damage) {
        pline(msgc_combatimmune, "The %s.", kick_passes_thru);
        /* doesn't exercise skill or abuse alignment or frighten pet, and
           shades have no passive counterattack */
        return;
    }

    /* in AceHack, floating eyes are immune to kicks if their passive would
       fire, just like they're immune to other melee damage. "it" here because
       the floating eye has been named already in the previous message. */
    if (mon->data == &mons[PM_FLOATING_EYE] && canseemon(mon) && !Free_action &&
        !Reflecting && mon->mcansee) {
        pline(msgc_combatimmune,
              "But it glares at you, making your kick go wild!");
        return;
    }


    if (mon->m_ap_type)
        seemimic(mon);

    check_caitiff(mon);

    /* squeeze some guilt feelings... */
    if (mon->mtame) {
        abuse_dog(mon);
        if (mon->mtame)
            monflee(mon, (dmg ? rnd(dmg) : 1), FALSE, FALSE);
        else
            mon->mflee = 0;
    }

    if (dmg > 0) {
        /* convert potential damage to actual damage */
        dmg = rnd(dmg);
        if (martial()) {
            if (dmg > 1)
                kick_skill = P_MARTIAL_ARTS;
            dmg += rn2(ACURR(A_DEX) / 2 + 1);
        }
        /* a good kick exercises your dex */
        exercise(A_DEX, TRUE);
    }
    if (blessed_foot_damage)
        dmg += rnd(4);
    if (uarmf)
        dmg += uarmf->spe;
    dmg += u.udaminc;   /* add ring(s) of increase damage */

    /* Do passive counterattacks before damaging the monster. Otherwise, we get
       a dmonsfree crash if the monster and player kill each other
       simultaneously. */
    if (dmg < 0)
        dmg = 0;

    int newmhp = mon->mhp - dmg;

    passive(mon, TRUE, newmhp > 0, AT_KICK);

    if (newmhp > 0 && martial() && !bigmonst(mon->data) && !rn2(3) &&
        mon->mcanmove && mon != u.ustuck && !mon->mtrapped) {
        /* see if the monster has a place to move into */
        mdx = mon->mx + dx;
        mdy = mon->my + dy;
        mon->mhp = newmhp;

        if (goodpos(level, mdx, mdy, mon, 0)) {
            pline(msgc_combatalert, "%s reels from the blow.", Monnam(mon));
            if (m_in_out_region(mon, mdx, mdy)) {
                remove_monster(level, mon->mx, mon->my);
                newsym(mon->mx, mon->my);
                place_monster(mon, mdx, mdy);
                newsym(mon->mx, mon->my);
                set_apparxy(mon);
                if (mintrap(mon) == 2)
                    trapkilled = TRUE;
            }
            if (uarmf && uarmf->otyp == KICKING_BOOTS)
                makeknown(KICKING_BOOTS);
        }

        newmhp = mon->mhp;
    }

    mon->mhp = newmhp;
    if (mon->mhp <= 0 && !trapkilled)
        killed(mon);

    /* may bring up a dialog, so put this after all messages */
    if (kick_skill != P_NONE)   /* exercise proficiency */
        use_skill(kick_skill, 1);
}

static enum attack_check_status
kick_monster(xchar x, xchar y, schar dx, schar dy)
{
    boolean clumsy = FALSE;
    struct monst *mon = m_at(level, x, y);
    int i, j, attack_status;
    coord bypos;
    boolean canmove = TRUE;

    bhitpos.x = x;
    bhitpos.y = y;

    if (resolve_uim(flags.interaction_mode, TRUE, u.ux + turnstate.intended_dx,
                    u.uy + turnstate.intended_dy) == uia_halt)
        return ac_cancel;

    attack_status = ac_continue;

    setmangry(mon);

    /* Kick attacks by kicking monsters are normal attacks, not special. This
       is almost always worthless, since you can either take one turn and do
       all your kicks, or else take one turn and attack the monster normally,
       getting all your attacks _including_ all your kicks. If you have >1 kick
       attack, you get all of them. */
    if (Upolyd && attacktype(youmonst.data, AT_KICK)) {
        const struct attack *uattk;
        int sum;
        schar tmp = find_roll_to_hit(mon);

        for (i = 0; i < NATTK; i++) {
            /* first of two kicks might have provoked counterattack that has
               incapacitated the hero (ie, floating eye) */
            if (u_helpless(hm_all))
                break;

            uattk = &youmonst.data->mattk[i];
            /* we only care about kicking attacks here */
            if (uattk->aatyp != AT_KICK)
                continue;

            if (mon->data == &mons[PM_SHADE] && (!uarmf || !uarmf->blessed)) {
                /* doesn't matter whether it would have hit or missed, and
                   shades have no passive counterattack */
                pline(msgc_combatimmune, "Your %s %s.", kick_passes_thru,
                      mon_nam(mon));
                break;  /* skip any additional kicks */
            } else if (tmp > rnd(20)) {
                pline(msgc_combatgood, "You kick %s.", mon_nam(mon));
                attack_status = ac_monsterhit;
                sum = damageum(mon, uattk);
                passive(mon, (boolean) (sum > 0), (sum != 2), AT_KICK);
                if (sum == 2)
                    break;      /* Defender died */
            } else {
                missum(mon, uattk);
                passive(mon, 0, 1, AT_KICK);
            }
        }
        return attack_status;
    }

    if (Levitation && !rn2(3) && verysmall(mon->data) && !is_flyer(mon->data)) {
        pline(msgc_failrandom, "Floating in the air, you miss wildly!");
        exercise(A_DEX, FALSE);
        passive(mon, FALSE, 1, AT_KICK);
        return attack_status;
    }

    i = -inv_weight();
    j = weight_cap();

    if (i < (j * 3) / 10) {
        if (!rn2((i < j / 10) ? 2 : (i < j / 5) ? 3 : 4)) {
            if (martial() && !rn2(2))
                goto doit;
            pline(msgc_failrandom, "Your clumsy kick does no damage.");
            passive(mon, FALSE, 1, AT_KICK);
            return attack_status;
        }
        if (i < j / 10)
            clumsy = TRUE;
        else if (!rn2((i < j / 5) ? 2 : 3))
            clumsy = TRUE;
    }

    if (Fumbling)
        clumsy = TRUE;
    else if (uarm && !uskin() && objects[uarm->otyp].oc_bulky &&
             ACURR(A_DEX) < rnd(25))
        clumsy = TRUE;

doit:
    if (!enexto(&bypos, level, u.ux, u.uy, mon->data) ||
        !((can_teleport(mon->data) && !level->flags.noteleport) ||
          ((abs(bypos.x - u.ux) <= 1) && (abs(bypos.y - u.uy) <= 1))))
        canmove = FALSE;
    if (!rn2(clumsy ? 3 : 4) && (clumsy || !bigmonst(mon->data)) && mon->mcansee
        && !mon->mtrapped && !thick_skinned(mon->data) &&
        mon->data->mlet != S_EEL && haseyes(mon->data) && mon->mcanmove &&
        !mon->mstun && !mon->mconf && !mon->msleeping &&
        mon->data->mmove >= 12) {
        if (!canmove || (!nohands(mon->data) && !rn2(martial()? 5 : 3))) {
            pline(msgc_failrandom, "%s blocks your %skick.", Monnam(mon),
                  clumsy ? "clumsy " : "");
            passive(mon, FALSE, 1, AT_KICK);
            return ac_somethingelse;
        } else {
            rloc_to(mon, bypos.x, bypos.y);
            if (mon->mx != x || mon->my != y) {
                reveal_monster_at(x, y, TRUE);
                /* TODO: This should probably use locomotion(). */
                pline(msgc_failrandom,
                      "%s %s, %s evading your %skick.", Monnam(mon),
                      (can_teleport(mon->data) &&
                       !level->flags.noteleport ? "teleports" :
                       is_floater(mon->data) ? "floats" :
                       is_flyer(mon->data) ? "swoops" :
                       (nolimbs(mon->data) || slithy(mon->data)) ? "slides" :
                       "jumps"), clumsy ? "easily" : "nimbly",
                      clumsy ? "clumsy " : "");
                passive(mon, FALSE, 1, AT_KICK);
                return ac_continue;
            }
        }
    }
    pline(msgc_combatgood, "You kick %s.", mon_nam(mon));
    kickdmg(mon, clumsy, dx, dy);

    return ac_monsterhit;
}

/*
 *  Return TRUE if caught (the gold taken care of), FALSE otherwise.
 *  The gold object is *not* attached to the level->objlist chain!
 */
boolean
ghitm(struct monst * mtmp, struct obj * gold)
{
    boolean msg_given = FALSE;

    if (!likes_gold(mtmp->data) && !mtmp->isshk && !mtmp->ispriest &&
        !is_mercenary(mtmp->data)) {
        wakeup(mtmp, FALSE);
    } else if (!mtmp->mcanmove) {
        /* too light to do real damage */
        if (canseemon(mtmp)) {
            pline(msgc_yafm, "The %s harmlessly %s %s.", xname(gold),
                  otense(gold, "hit"), mon_nam(mtmp));
            msg_given = TRUE;
        }
    } else {
        long value = gold->quan * objects[gold->otyp].oc_cost;

        mtmp->msleeping = 0;
        mtmp->meating = 0;
        if (!rn2(4))
            setmangry(mtmp);    /* not always pleasing */

        /* greedy monsters catch gold */
        if (cansee(mtmp->mx, mtmp->my))
            pline(msgc_actionok, "%s catches the gold.", Monnam(mtmp));
        if (mtmp->isshk) {
            long robbed = ESHK(mtmp)->robbed;

            if (robbed) {
                robbed -= value;
                if (robbed < 0)
                    robbed = 0;
                pline(msgc_actionok, "The amount %scovers %s recent losses.",
                      !robbed ? "" : "partially ", mhis(mtmp));
                ESHK(mtmp)->robbed = robbed;
                if (!robbed)
                    make_happy_shk(mtmp, FALSE);
            } else {
                if (mtmp->mpeaceful) {
                    ESHK(mtmp)->credit += value;
                    pline(msgc_actionok, "You have %ld %s in credit.",
                          (long)ESHK(mtmp)->credit,
                          currency(ESHK(mtmp)->credit));
                } else
                    verbalize(msgc_badidea, "Thanks, scum!");
            }
        } else if (mtmp->ispriest) {
            if (mtmp->mpeaceful)
                verbalize(msgc_actionok, "Thank you for your contribution.");
            else
                verbalize(msgc_badidea, "Thanks, scum!");
        } else if (is_mercenary(mtmp->data)) {
            long goldreqd = 0L;

            if (rn2(3)) {
                if (mtmp->data == &mons[PM_SOLDIER])
                    goldreqd = 100L;
                else if (mtmp->data == &mons[PM_SERGEANT])
                    goldreqd = 250L;
                else if (mtmp->data == &mons[PM_LIEUTENANT])
                    goldreqd = 500L;
                else if (mtmp->data == &mons[PM_CAPTAIN])
                    goldreqd = 750L;

                if (goldreqd) {
                    if (value >
                        goldreqd + (money_cnt(invent) +
                                    u.ulevel * rn2(5)) / ACURR(A_CHA))
                        msethostility(mtmp, FALSE, FALSE);
                }
            }
            if (mtmp->mpeaceful)
                verbalize(msgc_actionok, "That should do.  Now beat it!");
            else
                verbalize(msgc_failrandom, "That's not enough, coward!");
        }

        add_to_minv(mtmp, gold);
        return TRUE;
    }

    if (!msg_given)
        miss(xname(gold), mtmp, &youmonst);
    return FALSE;
}

/* container is kicked, dropped, thrown or otherwise impacted by player.
 * Assumes container is on floor.  Checks contents for possible damage. */
void
container_impact_dmg(struct obj *obj)
{
    struct monst *shkp;
    struct obj *otmp, *otmp2;
    long loss = 0L;
    boolean costly, insider;
    xchar x = obj->ox, y = obj->oy;

    /* only consider normal containers */
    if (!Is_container(obj) || Is_mbag(obj))
        return;

    costly = ((shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE))) &&
              costly_spot(x, y));
    insider = (*u.ushops && inside_shop(level, u.ux, u.uy) &&
               *in_rooms(level, x, y, SHOPBASE) == *u.ushops);

    for (otmp = obj->cobj; otmp; otmp = otmp2) {
        const char *result = NULL;

        otmp2 = otmp->nobj;
        if (objects[otmp->otyp].oc_material == GLASS &&
            otmp->oclass != GEM_CLASS && !obj_resists(otmp, 33, 100)) {
            result = "shatter";
        } else if (otmp->otyp == EGG && !rn2(3)) {
            result = "cracking";
        }
        if (result) {
            if (otmp->otyp == MIRROR)
                change_luck(-2);

            /* eggs laid by you.  penalty is -1 per egg, max 5, but it's always
               exactly 1 that breaks */
            if (otmp->otyp == EGG && otmp->spe && otmp->corpsenm >= LOW_PM)
                change_luck(-1);
            You_hear(msgc_itemloss, "a muffled %s.", result);
            if (costly)
                loss +=
                    stolen_value(otmp, x, y, (boolean) shkp->mpeaceful, TRUE);
            if (otmp->quan > 1L)
                useup(otmp);
            else {
                obj_extract_self(otmp);
                obfree(otmp, NULL);
            }
        }
    }
    if (costly && loss) {
        if (!insider) {
            pline(msgc_npcanger, "You caused %ld %s worth of damage!", loss,
                  currency(loss));
            make_angry_shk(shkp, x, y);
        } else {
            pline(msgc_unpaid, "You owe %s %ld %s for objects destroyed.",
                  mon_nam(shkp), loss, currency(loss));
        }
    }
}

static int
kick_object(xchar x, xchar y, schar dx, schar dy, struct obj **kickobj_p)
{
    int range;
    struct monst *mon, *shkp;
    struct trap *trap;
    char bhitroom;
    boolean costly, isgold, slide = FALSE, air = FALSE;

    /* if a pile, the "top" object gets kicked */
    struct obj *kickobj = level->objects[x][y];
    *kickobj_p = kickobj;

    /* *kickobj_p should always be set due to conditions of call */
    if (!kickobj || kickobj->otyp == BOULDER || kickobj == uball ||
        kickobj == uchain)
        return 0;

    if ((trap = t_at(level, x, y)) != 0 &&
        (((trap->ttyp == PIT || trap->ttyp == SPIKED_PIT) && !Passes_walls) ||
         trap->ttyp == WEB)) {
        if (!trap->tseen)
            find_trap(trap);
        pline(msgc_cancelled1, "You can't kick something that's in a %s!",
              Hallucination ? "tizzy" : (trap->ttyp == WEB) ? "web" : "pit");
        return 1;
    }

    if (Fumbling && !rn2(3)) {
        pline(msgc_failrandom, "Your clumsy kick missed.");
        return 1;
    }

    if (kickobj->otyp == CORPSE && !uarmf &&
        touched_monster(kickobj->corpsenm)) {
        pline(msgc_badidea, "You kick the %s with your bare %s.",
              corpse_xname(kickobj, TRUE), makeplural(body_part(FOOT)));
        pline(msgc_fatal_predone, "You turn to stone...");
        /* KMH -- otmp should be kickobj */
        done(STONING, killer_msg(STONING,
                                 msgprintf("kicking %s without boots",
                                           killer_xname(kickobj))));
    }

    /* range < 2 means the object will not move. */
    /* maybe dexterity should also figure here.  */
    range = (int)((ACURRSTR) / 2 - kickobj->owt / 40);

    if (martial())
        range += rnd(3);

    if (is_pool(level, x, y)) {
        /* you're in the water too; significantly reduce range */
        range = range / 3 + 1;  /* {1,2}=>1, {3,4,5}=>2, {6,7,8}=>3 */
    } else {
        if (is_ice(level, x, y)) {
            range += rnd(3);
            slide = TRUE;
        }
        if ((air = IS_AIR(level->locations[x][y].typ))) {
            range += rnd(5) + 1;
            slide = TRUE;
        } else if (kickobj->greased) {
            /* The greased bonus should not apply in air. */
            range += rnd(3);
            slide = TRUE;
        }
    }

    /* Mjollnir is magically too heavy to kick */
    if (kickobj->oartifact == ART_MJOLLNIR)
        range = 1;

    /* see if the object has a place to move into */
    if (!ZAP_POS(level->locations[x + dx][y + dy].typ) ||
        closed_door(level, x + dx, y + dy))
        range = 1;

    costly = ((shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE))) &&
              costly_spot(x, y));
    isgold = (kickobj->oclass == COIN_CLASS);

    if (IS_ROCK(level->locations[x][y].typ) || closed_door(level, x, y)) {
        if ((!martial() && rn2(20) > ACURR(A_DEX)) ||
            IS_ROCK(level->locations[u.ux][u.uy].typ) ||
            closed_door(level, u.ux, u.uy)) {
            if (Blind)
                pline(msgc_failrandom, "It doesn't come loose.");
            else
                pline(msgc_failrandom, "%s %sn't come loose.",
                      The(distant_name(kickobj, xname)),
                      otense(kickobj, "do"));
            return !rn2(3) || martial();
        }
        if (Blind)
            pline(msgc_actionok, "It comes loose.");
        else
            pline(msgc_actionok, "%s %s loose.",
                  The(distant_name(kickobj, xname)), otense(kickobj, "come"));
        obj_extract_self(kickobj);
        newsym(x, y);
        if (costly &&
            (!costly_spot(u.ux, u.uy) ||
             !strchr(u.urooms, *in_rooms(level, x, y, SHOPBASE))))
            addtobill(kickobj, FALSE, FALSE, FALSE);
        if (!flooreffects(kickobj, u.ux, u.uy, "fall")) {
            place_object(kickobj, level, u.ux, u.uy);
            stackobj(kickobj);
            newsym(u.ux, u.uy);
        }
        return 1;
    }

    /* a box gets a chance of breaking open here */
    if (Is_box(kickobj)) {
        boolean otrp = kickobj->otrapped;

        if (range < 2)
            pline_implied(msgc_failrandom, "THUD!");

        container_impact_dmg(kickobj);

        if (kickobj->olocked) {
            if (!rn2(5) || (martial() && !rn2(2))) {
                pline(msgc_actionok, "You break open the lock!");
                kickobj->olocked = 0;
                kickobj->obroken = 1;
                if (otrp)
                    chest_trap(kickobj, LEG, FALSE);
                return 1;
            }
        } else {
            if (!rn2(3) || (martial() && !rn2(2))) {
                pline(msgc_actionok, "The lid slams open, then falls shut.");
                if (otrp)
                    chest_trap(kickobj, LEG, FALSE);
                return 1;
            }
        }
        if (range < 2)
            return 1;
        /* else let it fall through to the next cases... */
    }

    /* fragile objects should not be kicked */
    if (hero_breaks(kickobj, kickobj->ox, kickobj->oy, FALSE))
        return 1;

    /* too heavy to move.  range is calculated as potential distance from
       player, so range == 2 means the object may move up to one square from
       its current position */
    if (range < 2 || (isgold && kickobj->quan > 300L)) {
        if (!Is_box(kickobj))
            pline(msgc_failcurse, "Thump!");
        return !rn2(3) || martial();
    }

    if (kickobj->quan > 1L && !isgold)
        kickobj = splitobj(kickobj, 1L);

    if (slide && !Blind)
        pline(msgc_actionok, "Whee!  %s %s %s the %s.", Doname2(kickobj),
              otense(kickobj, air ? "fly" : "slide"),
              air ? "through" : "across", air &&
              Is_waterlevel(&u.uz) ? "bubble" : surface(x, y));

    obj_extract_self(kickobj);
    snuff_candle(kickobj);
    newsym(x, y);
    mon = beam_hit(dx, dy, range, KICKED_WEAPON, NULL, NULL, kickobj, NULL);

    if (mon) {
        if (mon->isshk && kickobj->where == OBJ_MINVENT &&
            kickobj->ocarry == mon)
            return 1;   /* alert shk caught it */
        notonhead = (mon->mx != bhitpos.x || mon->my != bhitpos.y);
        if (isgold ? ghitm(mon, kickobj) :      /* caught? */
            thitmonst(mon, kickobj))    /* hit && used up? */
            return 1;
    }

    /* the object might have fallen down a hole */
    if (kickobj->olev != level) {
        if (costly) {
            if (isgold)
                costly_gold(x, y, kickobj->quan);
            else
                stolen_value(kickobj, x, y, (boolean) shkp->mpeaceful, FALSE);
        }
        return 1;
    }

    bhitroom = *in_rooms(level, bhitpos.x, bhitpos.y, SHOPBASE);
    if (costly &&
        (!costly_spot(bhitpos.x, bhitpos.y) ||
         *in_rooms(level, x, y, SHOPBASE) != bhitroom)) {
        if (isgold)
            costly_gold(x, y, kickobj->quan);
        else
            stolen_value(kickobj, x, y, (boolean) shkp->mpeaceful, FALSE);
    }

    if (flooreffects(kickobj, bhitpos.x, bhitpos.y, "fall"))
        return 1;
    place_object(kickobj, level, bhitpos.x, bhitpos.y);
    stackobj(kickobj);
    newsym(kickobj->ox, kickobj->oy);
    return 1;
}

static const char *
kickstr(struct obj *kickobj)
{
    const char *what;

    if (kickobj)
        what = killer_xname(kickobj);
    else if (IS_DOOR(maploc->typ))
        what = "a door";
    else if (IS_TREE(maploc->typ))
        what = "a tree";
    else if (IS_STWALL(maploc->typ))
        what = "a wall";
    else if (IS_ROCK(maploc->typ))
        what = "a rock";
    else if (IS_THRONE(maploc->typ))
        what = "a throne";
    else if (IS_FOUNTAIN(maploc->typ))
        what = "a fountain";
    else if (IS_GRAVE(maploc->typ))
        what = "a headstone";
    else if (IS_SINK(maploc->typ))
        what = "a sink";
    else if (IS_ALTAR(maploc->typ))
        what = "an altar";
    else if (IS_DRAWBRIDGE(maploc->typ))
        what = "a drawbridge";
    else if (maploc->typ == STAIRS)
        what = "the stairs";
    else if (maploc->typ == LADDER)
        what = "a ladder";
    else if (maploc->typ == IRONBARS)
        what = "an iron bar";
    else
        what = "something weird";
    return msgcat("kicking ", what);
}

int
dokick(const struct nh_cmd_arg *arg)
{
    int x, y;
    int avrg_attrib;
    struct monst *mtmp;
    boolean no_kick = FALSE;
    schar dx, dy, dz;
    struct obj *kickobj = NULL;

    if (nolimbs(youmonst.data) || slithy(youmonst.data)) {
        pline(msgc_cancelled, "You have no legs to kick with.");
        no_kick = TRUE;
    } else if (verysmall(youmonst.data)) {
        pline(msgc_cancelled, "You are too small to do any kicking.");
        no_kick = TRUE;
    } else if (u.usteed) {
        if (yn_function("Kick your steed?", ynchars, 'y') == 'y') {
            pline(msgc_occstart, "You kick %s.", mon_nam(u.usteed));
            kick_steed();
            return 1;
        } else {
            return 0;
        }
    } else if (Wounded_legs) {
        /* note: jump() has similar code */
        const char *bp = body_part(LEG);

        if (LWounded_legs && RWounded_legs)
            bp = makeplural(bp);
        pline(msgc_cancelled, "Your %s%s %s in no shape for kicking.",
              (!RWounded_legs) ? "left " :
              (!LWounded_legs) ? "right " : "",
              bp, (LWounded_legs && RWounded_legs) ? "are" : "is");
        no_kick = TRUE;
    } else if (near_capacity() > SLT_ENCUMBER) {
        pline(msgc_cancelled,
              "Your load is too heavy to balance yourself for a kick.");
        no_kick = TRUE;
    } else if (youmonst.data->mlet == S_LIZARD) {
        pline(msgc_cancelled, "Your legs cannot kick effectively.");
        no_kick = TRUE;
    } else if (u.uinwater && !rn2(2)) {
        pline(msgc_failrandom, "Your slow motion kick doesn't hit anything.");
        no_kick = TRUE;
    } else if (u.utrap) {
        switch (u.utraptype) {
        case TT_PIT:
            pline(msgc_cancelled, "There's not enough room to kick down here.");
            break;
        case TT_WEB:
        case TT_BEARTRAP:
            pline(msgc_cancelled, "You can't move your %s!", body_part(LEG));
            break;
        default:
            break;
        }
        no_kick = TRUE;
    }

    /* TODO: Prompt for a direction before the error message? */
    if (no_kick) {
        /* ignore direction typed before player notices kick failed */
        win_pause_output(P_MESSAGE);    /* --More-- */
        return 0;
    }

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;
    if (!dx && !dy)
        return 0;

    x = u.ux + dx;
    y = u.uy + dy;

    /* KMH -- Kicking boots always succeed */
    if (uarmf && uarmf->otyp == KICKING_BOOTS)
        avrg_attrib = 99;
    else
        avrg_attrib = (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3;

    if (Engulfed) {
        switch (rn2(3)) {
        case 0:
            pline(msgc_cancelled1, "You can't move your %s!", body_part(LEG));
            break;
        case 1:
            if (is_animal(u.ustuck->data)) {
                pline(msgc_cancelled1, "%s burps loudly.", Monnam(u.ustuck));
                break;
            }
        default:
            pline(msgc_cancelled1, "Your feeble kick has no effect.");
            break;
        }
        return 1;
    }
    if (!isok(x, y)) {
        pline(msgc_cancelled, "Now is not the time to think outside the box.");
        return 0;
    }
    if (Levitation) {
        int xx, yy;

        xx = u.ux - dx;
        yy = u.uy - dy;
        /* doors can be opened while levitating, so they must be reachable for
           bracing purposes. Possible extension: allow bracing against stuff on
           the side? */
        if (isok(xx, yy) && !IS_ROCK(level->locations[xx][yy].typ) &&
            !IS_DOOR(level->locations[xx][yy].typ) &&
            (!Is_airlevel(&u.uz) || !OBJ_AT(xx, yy))) {
            pline(msgc_cancelled,
                  "You have nothing to brace yourself against.");
            return 0;
        }
    }

    maploc = &level->locations[x][y];

    /* The next five tests should stay in */
    /* their present order: monsters, pools, */
    /* objects, non-doors, doors.  */

    if (MON_AT(level, x, y)) {
        const struct permonst *mdat;
        boolean ret;

        mtmp = m_at(level, x, y);
        mdat = mtmp->data;
        ret = kick_monster(x, y, dx, dy);
        if (ret != ac_continue)
            return ret != ac_cancel;

        wake_nearby(FALSE);
        u_wipe_engr(2);

        reveal_monster_at(x, y, TRUE);

        if (Is_airlevel(&u.uz) || Levitation) {
            int range;

            range = ((int)youmonst.data->cwt + (weight_cap() + inv_weight()));
            if (range < 1)
                range = 1;      /* divide by zero avoidance */
            range = (3 * (int)mdat->cwt) / range;

            if (range < 1)
                range = 1;
            hurtle(-dx, -dy, range, TRUE);
        }
        return 1;
    }

    wake_nearby(FALSE);
    u_wipe_engr(2);

    reveal_monster_at(x, y, TRUE);

    if (is_pool(level, x, y) ^ !!u.uinwater) {
        /* objects normally can't be removed from water by kicking */
        pline(msgc_cancelled1, "You splash some water around.");
        return 1;
    }

    if (OBJ_AT(x, y) &&
        (!Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)
         || sobj_at(BOULDER, level, x, y))) {
        if (kick_object(x, y, dx, dy, &kickobj)) {
            if (Is_airlevel(&u.uz))
                hurtle(-dx, -dy, 1, TRUE);      /* assume it's light */
            return 1;
        }
        goto ouch;
    }

    if (!IS_DOOR(maploc->typ)) {
        if (maploc->typ == SDOOR) {
            if (!Levitation && rn2(30) < avrg_attrib) {
                cvt_sdoor_to_door(maploc, &u.uz);       /* ->typ = DOOR */
                pline(msgc_youdiscover, "Crash!  %s a secret door!",
                      /* don't "kick open" when it's locked unless it also
                         happens to be trapped */
                      (maploc->doormask & (D_LOCKED | D_TRAPPED)) ==
                      D_LOCKED ? "Your kick uncovers" : "You kick open");
                exercise(A_DEX, TRUE);
                if (maploc->doormask & D_TRAPPED) {
                    maploc->doormask = D_NODOOR;
                    b_trapped("door", FOOT);
                } else if (maploc->doormask != D_NODOOR &&
                           !(maploc->doormask & D_LOCKED))
                    maploc->doormask = D_ISOPEN;
                if (Blind)
                    feel_location(x, y);        /* we know it's gone */
                else
                    newsym(x, y);
                if (maploc->doormask == D_ISOPEN ||
                    maploc->doormask == D_NODOOR)
                    unblock_point(x, y);        /* vision */
                return 1;
            } else
                goto ouch;
        }
        if (maploc->typ == SCORR) {
            if (!Levitation && rn2(30) < avrg_attrib) {
                pline(msgc_youdiscover,
                      "Crash!  You kick open a secret passage!");
                exercise(A_DEX, TRUE);
                maploc->typ = CORR;
                if (Blind)
                    feel_location(x, y);        /* we know it's gone */
                else
                    newsym(x, y);
                unblock_point(x, y);    /* vision */
                return 1;
            } else
                goto ouch;
        }
        if (IS_THRONE(maploc->typ)) {
            int i;

            boolean kickedloose = !rn2_on_rng(3, rng_throne_loot);
            int goldamt = rn2_on_rng(200, rng_throne_loot);
            boolean trapdoor = !rn2_on_rng(4, rng_throne_loot);

            if (Levitation)
                goto dumb;
            if ((Luck < 0 || maploc->doormask) && kickedloose) {
                maploc->typ = ROOM;
                maploc->doormask = 0;   /* don't leave loose ends.. */
                mkgold(goldamt, level, x, y, rng_main);
                if (Blind)
                    pline(msgc_substitute, "CRASH!  You destroy it.");
                else {
                    pline(msgc_substitute, "CRASH!  You destroy the throne.");
                    newsym(x, y);
                }
                exercise(A_DEX, TRUE);
                return 1;
            } else if (Luck > 0 && kickedloose && !maploc->looted) {
                mkgold(goldamt + 301, level, x, y, rng_main);
                i = Luck + 1;
                if (i > 6)
                    i = 6;
                while (i--)
                    mksobj_at(rnd_class(DILITHIUM_CRYSTAL, LUCKSTONE - 1,
                                        rng_main),
                              level, x, y, FALSE, TRUE, rng_main);
                if (Blind)
                    pline(msgc_actionok, "You kick something loose!");
                else {
                    pline(msgc_actionok,
                          "You kick loose some ornamental coins and gems!");
                    newsym(x, y);
                }
                /* prevent endless milking */
                maploc->looted = T_LOOTED;
                return 1;
            } else if (trapdoor) {
                if (dunlev(&u.uz) < dunlevs_in_dungeon(&u.uz)) {
                    fall_through(FALSE);
                    return 1;
                } else
                    goto ouch;
            }
            goto ouch;
        }
        if (IS_ALTAR(maploc->typ)) {
            if (Levitation)
                goto dumb;
            pline(msgc_badidea, "You kick %s.",
                  (Blind ? "something" : "the altar"));
            if (!rn2(3))
                goto ouch;
            altar_wrath(x, y);
            exercise(A_DEX, TRUE);
            return 1;
        }
        if (IS_FOUNTAIN(maploc->typ)) {
            if (Levitation)
                goto dumb;
            pline(msgc_badidea, "You kick %s.",
                  (Blind ? "something" : "the fountain"));
            if (!rn2(3))
                goto ouch;
            /* make metal boots rust */
            if (uarmf && rn2(3))
                if (!water_damage(uarmf, "metal boots", TRUE)) {
                    pline(msgc_badidea, "Your boots get wet.");
                    /* could cause short-lived fumbling here */
                }
            exercise(A_DEX, TRUE);
            return 1;
        }
        if (IS_GRAVE(maploc->typ) || maploc->typ == IRONBARS)
            goto ouch;
        if (IS_TREE(maploc->typ)) {
            struct obj *treefruit;

            /* nothing, fruit or trouble? 75:23.5:1.5% */
            if (rn2(3)) {
                if (!rn2(6) && !(mvitals[PM_KILLER_BEE].mvflags & G_GONE))
                    You_hear(msgc_levelwarning, "a low buzzing.");
                goto ouch;
            }
            if (rn2(15) && !(maploc->looted & TREE_LOOTED) &&
                (treefruit = rnd_treefruit_at(x, y))) {
                long nfruit = 8L - rnl(7), nfall;
                short frtype = treefruit->otyp;

                treefruit->quan = nfruit;
                if (is_plural(treefruit))
                    pline(msgc_actionok, "Some %s fall from the tree!",
                          xname(treefruit));
                else
                    pline(msgc_actionok, "%s falls from the tree!",
                          An(xname(treefruit)));
                nfall = scatter(x, y, 2, MAY_HIT, treefruit);
                if (nfall != nfruit) {
                    /* scatter left some in the tree, but treefruit may not
                       refer to the correct object */
                    treefruit = mksobj(level, frtype, TRUE, FALSE, rng_main);
                    treefruit->quan = nfruit - nfall;
                    pline(msgc_actionok, "%ld %s got caught in the branches.",
                          nfruit - nfall, xname(treefruit));
                    dealloc_obj(treefruit);
                }
                exercise(A_DEX, TRUE);
                exercise(A_WIS, TRUE);  /* discovered a new food source! */
                newsym(x, y);
                maploc->looted |= TREE_LOOTED;
                return 1;
            } else if (!(maploc->looted & TREE_SWARM)) {
                int cnt = rnl(4) + 2;
                int made = 0;
                coord mm;

                mm.x = x;
                mm.y = y;
                while (cnt--) {
                    if (enexto(&mm, level, mm.x, mm.y, &mons[PM_KILLER_BEE])
                        && makemon(&mons[PM_KILLER_BEE], level, mm.x, mm.y,
                                   MM_ANGRY))
                        made++;
                }
                if (made)
                    pline(msgc_substitute,
                          "You've attracted the tree's former occupants!");
                else
                    pline(msgc_substitute, "You smell stale honey.");
                maploc->looted |= TREE_SWARM;
                return 1;
            }
            goto ouch;
        }
        if (IS_SINK(maploc->typ)) {
            int gend = poly_gender();
            short washerndx = (gend == 1 ||
                               (gend == 2 &&
                                rn2(2))) ? PM_INCUBUS : PM_SUCCUBUS;

            boolean pudding_available = !rn2_on_rng(3, rng_sink_kick);
            boolean dishwasher_available = !rn2_on_rng(3, rng_sink_kick);
            boolean ring_available = !rn2_on_rng(3, rng_sink_kick);

            if (Levitation)
                goto dumb;
            if (rn2_on_rng(5, rng_sink_kick)) {
                if (canhear())
                    pline(msgc_failrandom,
                          "Klunk!  The pipes vibrate noisily.");
                else
                    pline(msgc_failrandom, "Klunk!");
                exercise(A_DEX, TRUE);
                return 1;
            } else if (!(maploc->looted & S_LPUDDING) && pudding_available) {
                if (!(mvitals[PM_BLACK_PUDDING].mvflags & G_GONE)) {
                    if (Blind)
                        You_hear(msgc_levelwarning, "a gushing sound.");
                    else
                        pline(msgc_levelwarning,
                              "A %s ooze gushes up from the drain!",
                              hcolor("black"));
                    makemon(&mons[PM_BLACK_PUDDING], level, x, y, NO_MM_FLAGS);
                    exercise(A_DEX, TRUE);
                    newsym(x, y);
                    maploc->looted |= S_LPUDDING;
                } else {
                    /* this message works even if blind */
                    pline(msgc_noconsequence,
                          "A %s ooze fails to gush up from the drain.",
                          hcolor("black"));
                }
                return 1;
            } else if (!(maploc->looted & S_LDWASHER) && dishwasher_available) {
                if (!(mvitals[washerndx].mvflags & G_GONE)) {
                    /* can't resist... */
                    pline(msgc_levelwarning, "%s returns!",
                          (Blind ? "Something" : "The dish washer"));
                    if (makemon(&mons[washerndx], level, x, y, NO_MM_FLAGS))
                        newsym(x, y);
                    maploc->looted |= S_LDWASHER;
                    exercise(A_DEX, TRUE);
                } else {
                    pline(msgc_noconsequence,
                          "You wonder if the sink's owners would approve.");
                }
                return 1;
            } else if (ring_available) {
                if (!(maploc->looted & S_LRING)) {      /* once per sink */
                    if (!Blind)
                        pline(msgc_youdiscover, "Muddy waste backs up.  "
                              "There's a ring in its midst!");
                    else
                        pline(msgc_youdiscover,
                              "Flupp!  Rattle rattle rattle!");
                    mkobj_at(RING_CLASS, level, x, y, TRUE, rng_sink_ring);
                    newsym(x, y);
                    exercise(A_DEX, TRUE);
                    exercise(A_WIS, TRUE);      /* a discovery! */
                    maploc->looted |= S_LRING;
                } else {
                    pline(msgc_noconsequence, "Flupp!  %s.",
                          (Blind ? "You hear a sloshing sound" :
                           "Muddy waste pops up from the drain"));
                }
                return 1;
            }
            goto ouch;
        }

        if (maploc->typ == STAIRS || maploc->typ == LADDER ||
            IS_STWALL(maploc->typ)) {
            if (!IS_STWALL(maploc->typ) && maploc->ladder == LA_DOWN)
                goto dumb;
        ouch:
            pline(msgc_badidea, "Ouch!  That hurts!");
            exercise(A_DEX, FALSE);
            exercise(A_STR, FALSE);
            if (Blind)
                feel_location(x, y);    /* we know we hit it */
            if (is_drawbridge_wall(x, y) >= 0) {
                pline(msgc_cancelled1, "The drawbridge is unaffected.");
                /* update maploc to refer to the drawbridge */
                find_drawbridge(&x, &y);
                maploc = &level->locations[x][y];
            }
            if (!rn2(3))
                set_wounded_legs(RIGHT_SIDE, 5 + rnd(5));
            losehp(rnd(ACURR(A_CON) > 15 ? 3 : 5),
                   killer_msg(DIED, kickstr(kickobj)));
            if (Is_airlevel(&u.uz) || Levitation)
                hurtle(-dx, -dy, rn1(2, 4), TRUE);      /* assume it's heavy */
            return 1;
        }
        goto dumb;
    }

    if (maploc->doormask == D_ISOPEN || maploc->doormask == D_BROKEN ||
        maploc->doormask == D_NODOOR) {
    dumb:
        exercise(A_DEX, FALSE);
        if (martial() || ACURR(A_DEX) >= 16 || rn2(3)) {
            pline(msgc_yafm, "You kick at empty space.");
            if (Blind)
                feel_location(x, y);
        } else {
            pline(msgc_badidea, "Dumb move!  You strain a muscle.");
            exercise(A_STR, FALSE);
            set_wounded_legs(RIGHT_SIDE, 5 + rnd(5));
        }
        if ((Is_airlevel(&u.uz) || Levitation) && rn2(2))
            hurtle(-dx, -dy, 1, TRUE);

        return 1;       /* you moved, so use up a turn */
    }

    /* not enough leverage to kick open doors while levitating */
    if (Levitation)
        goto ouch;

    exercise(A_DEX, TRUE);
    /* door is known to be CLOSED or LOCKED */
    if (rnl(35) < avrg_attrib + (!martial()? 0 : ACURR(A_DEX))) {
        boolean shopdoor = *in_rooms(level, x, y, SHOPBASE) ? TRUE : FALSE;

        /* break the door */
        if (maploc->doormask & D_TRAPPED) {
            pline(msgc_actionboring, "You kick the door.");
            exercise(A_STR, FALSE);
            maploc->doormask = D_NODOOR;
            b_trapped("door", FOOT);
        } else if (ACURR(A_STR) > 18 && !rn2(5) && !shopdoor) {
            pline(msgc_actionok,
                  "As you kick the door, it shatters to pieces!");
            exercise(A_STR, TRUE);
            maploc->doormask = D_NODOOR;
        } else {
            pline(msgc_actionok, "As you kick the door, it crashes open!");
            exercise(A_STR, TRUE);
            maploc->doormask = D_BROKEN;
        }
        if (Blind)
            feel_location(x, y);        /* we know we broke it */
        else
            newsym(x, y);
        unblock_point(x, y);    /* vision */
        if (shopdoor) {
            add_damage(x, y, 400L);
            pay_for_damage("break", FALSE);
        }
        if (in_town(x, y))
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                if ((mtmp->data == &mons[PM_WATCHMAN] ||
                     mtmp->data == &mons[PM_WATCH_CAPTAIN]) &&
                    couldsee(mtmp->mx, mtmp->my) && mtmp->mpeaceful) {
                    if (canspotmon(mtmp))
                        pline(msgc_npcvoice, "%s yells:", Amonnam(mtmp));
                    else
                        You_hear(msgc_npcvoice, "someone yell:");
                    verbalize(msgc_npcanger,
                              "Halt, thief!  You're under arrest!");
                    angry_guards(FALSE);
                    break;
                }
            }
    } else {
        if (Blind)
            feel_location(x, y);        /* we know we hit it */
        exercise(A_STR, TRUE);
        pline(msgc_failrandom, "WHAMMM!!!");
        if (in_town(x, y))
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                if ((mtmp->data == &mons[PM_WATCHMAN] ||
                     mtmp->data == &mons[PM_WATCH_CAPTAIN]) && mtmp->mpeaceful
                    && couldsee(mtmp->mx, mtmp->my)) {
                    if (canspotmon(mtmp))
                        pline(msgc_npcvoice, "%s yells:", Amonnam(mtmp));
                    else
                        You_hear(msgc_npcvoice, "someone yell:");
                    if (level->locations[x][y].looted & D_WARNED) {
                        verbalize(msgc_npcanger,
                                  "Halt, vandal!  You're under arrest!");
                        angry_guards(FALSE);
                    } else {
                        verbalize(msgc_levelwarning,
                                  "Hey, stop damaging that door!");
                        level->locations[x][y].looted |= D_WARNED;
                    }
                    break;
                }
            }
    }
    return 1;
}

static void
drop_to(coord * cc, schar loc)
{
    /* cover all the MIGR_xxx choices generated by down_gate() */
    switch (loc) {
    case MIGR_RANDOM:  /* trap door or hole */
        if (Is_stronghold(&u.uz)) {
            cc->x = valley_level.dnum;
            cc->y = valley_level.dlevel;
            break;
        } else if (In_endgame(&u.uz) || Is_botlevel(&u.uz)) {
            cc->y = cc->x = -1;
            break;
        }       /* else fall to the next cases */
    case MIGR_STAIRS_UP:
    case MIGR_LADDER_UP:
        cc->x = u.uz.dnum;
        cc->y = u.uz.dlevel + 1;
        break;
    case MIGR_SSTAIRS:
        cc->x = level->sstairs.tolev.dnum;
        cc->y = level->sstairs.tolev.dlevel;
        break;
    default:
    case MIGR_NOWHERE:
        /* y==0 means "nowhere", in which case x doesn't matter */
        cc->y = cc->x = -1;
        break;
    }
}

void
impact_drop(struct obj *missile, xchar x, xchar y, xchar dlev)
{
    schar toloc;
    struct obj *obj, *obj2;
    struct monst *shkp;
    long oct, dct, price, debit, robbed;
    boolean angry, costly, isrock;
    coord cc;

    if (!OBJ_AT(x, y))
        return;

    toloc = down_gate(x, y);
    drop_to(&cc, toloc);
    if (!cc.y)
        return;

    if (dlev) {
        /* send objects next to player falling through trap door. checked in
           obj_delivery(). */
        toloc = MIGR_NEAR_PLAYER;
        cc.y = dlev;
    }

    costly = costly_spot(x, y);
    price = debit = robbed = 0L;
    angry = FALSE;
    shkp = NULL;
    /* if 'costly', we must keep a record of ESHK(shkp) before it undergoes
       changes through the calls to stolen_value. the angry bit must be reset,
       if needed, in this fn, since stolen_value is called under the 'silent'
       flag to avoid unsavory pline repetitions. */
    if (costly) {
        if ((shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE)))
                != 0) {
            debit = ESHK(shkp)->debit;
            robbed = ESHK(shkp)->robbed;
            angry = !shkp->mpeaceful;
        }
    }

    isrock = (missile && missile->otyp == ROCK);
    oct = dct = 0L;
    for (obj = level->objects[x][y]; obj; obj = obj2) {
        obj2 = obj->nexthere;
        if (obj == missile)
            continue;
        /* number of objects in the pile */
        oct += obj->quan;
        if (obj == uball || obj == uchain)
            continue;
        /* boulders can fall too, but rarely & never due to rocks */
        if ((isrock && obj->otyp == BOULDER) ||
            rn2(obj->otyp == BOULDER ? 30 : 3))
            continue;
        obj_extract_self(obj);

        if (costly) {
            price +=
                stolen_value(obj, x, y,
                             (costly_spot(u.ux, u.uy) &&
                              strchr(u.urooms,
                                     *in_rooms(level, x, y, SHOPBASE))), TRUE);
            /* set obj->no_charge to 0 */
            if (Has_contents(obj))
                picked_container(obj);  /* does the right thing */
            if (obj->oclass != COIN_CLASS)
                obj->no_charge = 0;
        }

        /* number of fallen objects.  Note that deliver_object may change
           obj->quan. */
        dct += obj->quan;

        deliver_object(obj, cc.x, cc.y, toloc);
    }

    if (dct && cansee(x, y)) {  /* at least one object fell */
        const char *what = (dct == 1L ? "object falls" : "objects fall");

        if (missile)
            pline(msgc_consequence, "From the impact, %sother %s.",
                  dct == oct ? "the " : dct == 1L ? "an" : "", what);
        else if (oct == dct)
            pline(msgc_consequence,
                  "%s adjacent %s %s.", dct == 1L ? "The" : "All the", what,
                  gate_str);
        else
            pline(msgc_consequence, "%s adjacent %s %s.",
                  dct == 1L ? "One of the" : "Some of the",
                  dct == 1L ? "objects falls" : what, gate_str);
    }

    if (costly && shkp && price) {
        if (ESHK(shkp)->robbed > robbed) {
            pline(msgc_unpaid, "You removed %ld %s worth of goods!", price,
                  currency(price));
            if (cansee(shkp->mx, shkp->my)) {
                if (ESHK(shkp)->customer[0] == 0)
                    strncpy(ESHK(shkp)->customer, u.uplname, PL_NSIZ);
                if (angry)
                    pline(msgc_npcanger, "%s is infuriated!", Monnam(shkp));
                else
                    pline(msgc_npcanger, "\"%s, you are a thief!\"", u.uplname);
            } else
                You_hear(msgc_npcanger, "a scream, \"Thief!\"");
            hot_pursuit(shkp);
            angry_guards(FALSE);
            return;
        }
        if (ESHK(shkp)->debit > debit) {
            long amt = (ESHK(shkp)->debit - debit);

            pline(msgc_unpaid, "You owe %s %ld %s for goods lost.",
                  Monnam(shkp), amt, currency(amt));
        }
    }

}

/* NOTE: ship_object assumes otmp was FREED from level->objlist or invent.
 * <x,y> is the point of drop.  otmp is _not_ an <x,y> resident:
 * otmp is either a kicked, dropped, or thrown object.
 */
boolean
ship_object(struct obj *otmp, xchar x, xchar y, boolean shop_floor_obj)
{
    schar toloc;
    xchar ox, oy;
    coord cc;
    struct obj *obj;
    struct trap *t;
    boolean nodrop, unpaid, container, impact = FALSE;
    long n = 0L;

    if (!otmp)
        return FALSE;
    if ((toloc = down_gate(x, y)) == MIGR_NOWHERE)
        return FALSE;
    drop_to(&cc, toloc);
    if (!cc.y)
        return FALSE;

    /* objects other than attached iron ball always fall down ladder, but have
       a chance of staying otherwise */
    nodrop = (otmp == uball) || (otmp == uchain) ||
        (toloc != MIGR_LADDER_UP && rn2(3));

    container = Has_contents(otmp);
    unpaid = (otmp->unpaid || (container && count_unpaid(otmp->cobj)));

    if (OBJ_AT(x, y)) {
        for (obj = level->objects[x][y]; obj; obj = obj->nexthere)
            if (obj != otmp)
                n += obj->quan;
        if (n)
            impact = TRUE;
    }
    /* boulders never fall through trap doors, but they might knock other
       things down before plugging the hole */
    if (otmp->otyp == BOULDER && ((t = t_at(level, x, y)) != 0) &&
        (t->ttyp == TRAPDOOR || t->ttyp == HOLE)) {
        if (impact)
            impact_drop(otmp, x, y, 0);
        return FALSE;   /* let caller finish the drop */
    }

    if (cansee(x, y))
        otransit_msg(otmp, nodrop, n);

    if (nodrop) {
        if (impact)
            impact_drop(otmp, x, y, 0);
        return FALSE;
    }

    if (unpaid || shop_floor_obj) {
        if (unpaid) {
            subfrombill(otmp, shop_keeper(level, *u.ushops));
            stolen_value(otmp, u.ux, u.uy, TRUE, FALSE);
        } else {
            ox = otmp->ox;
            oy = otmp->oy;
            stolen_value(otmp, ox, oy,
                         (costly_spot(u.ux, u.uy) &&
                          strchr(u.urooms, *in_rooms(level, ox, oy, SHOPBASE))),
                         FALSE);
        }
        /* set otmp->no_charge to 0 */
        if (container)
            picked_container(otmp);     /* happens to do the right thing */
        if (otmp->oclass != COIN_CLASS)
            otmp->no_charge = 0;
    }

    unwield_silently(otmp);

    /* some things break rather than ship */
    if (breaktest(otmp)) {
        const char *result;

        if (objects[otmp->otyp].oc_material == GLASS ||
            otmp->otyp == EXPENSIVE_CAMERA) {
            if (otmp->otyp == MIRROR)
                change_luck(-2);
            result = "crash";
        } else {
            /* penalty for breaking eggs laid by you */
            if (otmp->otyp == EGG && otmp->spe && otmp->corpsenm >= LOW_PM)
                change_luck((schar) - min(otmp->quan, 5L));
            result = "splat";
        }
        You_hear(msgc_itemloss, "a muffled %s.", result);
        obj_extract_self(otmp);
        obfree(otmp, NULL);
        return TRUE;
    }

    deliver_object(otmp, cc.x, cc.y, toloc);
    /* boulder from rolling boulder trap, no longer part of the trap */
    if (otmp->otyp == BOULDER)
        otmp->otrapped = 0;

    if (impact) {
        /* the objs impacted may be in a shop other than the one in which the
           hero is located.  another check for a shk is made in impact_drop. it
           is, e.g., possible to kick/throw an object belonging to one shop
           into another shop through a gap in the wall, and cause objects
           belonging to the other shop to fall down a trap door--thereby
           getting two shopkeepers angry at the hero in one shot. */
        impact_drop(otmp, x, y, 0);
        newsym(x, y);
    }
    return TRUE;
}


void
deliver_object(struct obj *obj, xchar dnum, xchar dlevel, int where)
{
    struct level *lev;
    d_level levnum = { dnum, dlevel };
    int nx, ny;

    lev = levels[ledger_no(&levnum)];
    if (!lev) { /* this can go away if we pre-generate all levels */
        /* Reset the rndmonst state so that it will generate correct monsters
           for the level being created. */
        reset_rndmonst(NON_PM);
        lev = mklev(&levnum);
        reset_rndmonst(NON_PM);
    }

    obj_extract_self(obj);

    switch (where) {
    case MIGR_STAIRS_UP:
        nx = lev->upstair.sx, ny = lev->upstair.sy;
        break;
    case MIGR_LADDER_UP:
        nx = lev->upladder.sx, ny = lev->upladder.sy;
        break;
    case MIGR_SSTAIRS:
        nx = lev->sstairs.sx, ny = lev->sstairs.sy;
        break;
    case MIGR_NEAR_PLAYER:
        /* the player has fallen through a trapdoor or hole and obj fell
           through too [impact_drop in goto_level] */
        if (level == lev) {
            nx = u.ux;
            ny = u.uy;
        } else {
            extract_nobj(obj, &turnstate.floating_objects,
                         &turnstate.migrating_objs, OBJ_MIGRATING);
            return;
        }
        break;

    default:
    case MIGR_RANDOM:
        /* set dummy coordinates because there's no current position for
           rloco() to update */
        obj->ox = obj->oy = -1;
        rloco_pos(lev, obj, &nx, &ny);
        break;
    }

    place_object(obj, lev, nx, ny);
    stackobj(obj);
    scatter(nx, ny, rnd(2), 0, obj);
}


static void
otransit_msg(struct obj *otmp, boolean nodrop, long num)
{
    const char *obuf = msgprintf("The %s", xname(otmp));

    if (num) {  /* means: other objects are impacted */
        obuf = msgprintf("%s %s %s object%s", obuf, otense(otmp, "hit"),
                num == 1L ? "another" : "other", num > 1L ? "s" : "");
        if (nodrop)
            obuf = msgcat(obuf, ".");
        else
            obuf = msgprintf("%s and %s %s.", obuf,
                             otense(otmp, "fall"), gate_str);
        pline(msgc_consequence, "%s", obuf);
    } else if (!nodrop)
        pline(msgc_consequence, "%s %s %s.", obuf, otense(otmp, "fall"),
              gate_str);
}

/* migration destination for objects which fall down to next level */
schar
down_gate(xchar x, xchar y)
{
    struct trap *ttmp;

    gate_str = 0;
    /* this matches the player restriction in goto_level() */
    if (on_level(&u.uz, &qstart_level) && !ok_to_quest(FALSE))
        return MIGR_NOWHERE;

    if ((level->dnstair.sx == x && level->dnstair.sy == y) ||
        (level->sstairs.sx == x && level->sstairs.sy == y &&
         !level->sstairs.up)) {
        gate_str = "down the stairs";
        return (level->dnstair.sx == x &&
                level->dnstair.sy == y) ? MIGR_STAIRS_UP : MIGR_SSTAIRS;
    }
    if (level->dnladder.sx == x && level->dnladder.sy == y) {
        gate_str = "down the ladder";
        return MIGR_LADDER_UP;
    }

    if (((ttmp = t_at(level, x, y)) != 0 && ttmp->tseen) &&
        (ttmp->ttyp == TRAPDOOR || ttmp->ttyp == HOLE)) {
        gate_str =
            (ttmp->ttyp ==
             TRAPDOOR) ? "through the trap door" : "through the hole";
        return MIGR_RANDOM;
    }
    return MIGR_NOWHERE;
}

/*dokick.c*/

