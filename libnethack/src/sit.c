/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

void
take_gold(void)
{
    struct obj *otmp, *nobj;
    int lost_money = 0;

    for (otmp = invent; otmp; otmp = nobj) {
        nobj = otmp->nobj;
        if (otmp->oclass == COIN_CLASS) {
            lost_money = 1;
            delobj(otmp);
        }
    }
    if (!lost_money)
        pline("You feel a strange sensation.");
    else
        pline("You notice you have no money!");
}

int
dosit(const struct nh_cmd_arg *arg)
{
    static const char sit_message[] = "You sit on the %s.";
    struct trap *trap;
    int typ = level->locations[u.ux][u.uy].typ;
    int attrib;

    (void) arg;

    if (u.usteed) {
        pline("You are already sitting on %s.", mon_nam(u.usteed));
        return 0;
    }

    if (!can_reach_floor()) {
        if (Levitation)
            pline("You tumble in place.");
        else
            pline("You are sitting on air.");
        return 0;
    } else if (is_pool(level, u.ux, u.uy) && !Underwater) {
        /* water walking */
        goto in_water;
    }

    if ((trap = t_at(level, u.ux, u.uy)) != 0 && !u.utrap &&
        (trap->ttyp == HOLE || trap->ttyp == TRAPDOOR || trap->ttyp == PIT ||
         trap->ttyp == SPIKED_PIT) && trap->tseen) {
        pline("You sit on the edge of the %s.",
              trap->ttyp == HOLE ? "hole" : trap->ttyp ==
              TRAPDOOR ? "trapdoor" : "pit");
        return 1;
    }

    if (OBJ_AT(u.ux, u.uy)) {
        struct obj *obj;

        obj = level->objects[u.ux][u.uy];
        pline("You sit on %s.", the(xname(obj)));
        if (!(Is_box(obj) || objects[obj->otyp].oc_material == CLOTH))
            pline("It's not very comfortable...");

    } else if ((trap = t_at(level, u.ux, u.uy)) != 0 ||
               (u.utrap && (u.utraptype >= TT_LAVA))) {

        if (u.utrap) {
            exercise(A_WIS, FALSE);     /* you're getting stuck longer */
            if (u.utraptype == TT_BEARTRAP) {
                pline("You can't sit down with your %s in the bear trap.",
                      body_part(FOOT));
                u.utrap++;
            } else if (u.utraptype == TT_PIT) {
                if (trap->ttyp == SPIKED_PIT) {
                    pline("You sit down on a spike.  Ouch!");
                    losehp(1, killer_msg(DIED, "sitting on an iron spike"));
                    exercise(A_STR, FALSE);
                } else
                    pline("You sit down in the pit.");
                u.utrap += rn2(5);
            } else if (u.utraptype == TT_WEB) {
                pline("You sit in the spider web and get entangled further!");
                u.utrap += rn1(10, 5);
            } else if (u.utraptype == TT_LAVA) {
                /* Must have fire resistance or they'd be dead already */
                pline("You sit in the lava!");
                u.utrap += rnd(4);
                losehp(dice(2, 10), killer_msg(DIED, "sitting in lava"));
            } else if (u.utraptype == TT_INFLOOR) {
                pline("You can't maneuver to sit!");
                u.utrap++;
            }
        } else {
            pline("You sit down.");
            dotrap(trap, 0);
        }
    } else if (Underwater || Is_waterlevel(&u.uz)) {
        if (Is_waterlevel(&u.uz))
            pline("There are no cushions floating nearby.");
        else
            pline("You sit down on the muddy bottom.");
    } else if (is_pool(level, u.ux, u.uy)) {
    in_water:
        pline("You sit in the water.");
        if (!rn2(10) && uarm)
            water_damage(uarm, "armor", TRUE);
        if (!rn2(10) && uarmf && uarmf->otyp != WATER_WALKING_BOOTS)
            water_damage(uarm, "armor", TRUE);
    } else if (IS_SINK(typ)) {

        pline(sit_message, defexplain[S_sink]);
        pline("Your %s gets wet.",
              humanoid(youmonst.data) ? "rump" : "underside");
    } else if (IS_ALTAR(typ)) {

        pline(sit_message, defexplain[S_altar]);
        altar_wrath(u.ux, u.uy);

    } else if (IS_GRAVE(typ)) {

        pline(sit_message, defexplain[S_grave]);

    } else if (typ == STAIRS) {

        pline(sit_message, "stairs");

    } else if (typ == LADDER) {

        pline(sit_message, "ladder");

    } else if (is_lava(level, u.ux, u.uy)) {

        /* must be WWalking */
        pline(sit_message, "lava");
        burn_away_slime();
        if (likes_lava(youmonst.data)) {
            pline("The lava feels warm.");
            return 1;
        }
        pline("The lava burns you!");
        losehp(dice((Fire_resistance ? 2 : 10), 10),
               killer_msg(DIED, "sitting on lava"));

    } else if (is_ice(level, u.ux, u.uy)) {

        pline(sit_message, defexplain[S_ice]);
        if (!Cold_resistance)
            pline("The ice feels cold.");

    } else if (typ == DRAWBRIDGE_DOWN) {

        pline(sit_message, "drawbridge");

    } else if (IS_THRONE(typ)) {

        pline(sit_message, defexplain[S_throne]);
        if (!rn2_on_rng(3, rng_throne_result)) {
            switch (1 + rn2_on_rng(13, rng_throne_result)) {
            case 1:
                attrib = rn2_on_rng(A_MAX, rng_throne_result);
                adjattrib(attrib, -rn1(4, 3), FALSE);
                losehp(rnd(10), killer_msg(DIED, "a cursed throne"));
                break;
            case 2:
                adjattrib(rn2_on_rng(A_MAX, rng_throne_result), 1, FALSE);
                break;
            case 3:
                pline("A%s electric shock shoots through your body!",
                      (Shock_resistance) ? "n" : " massive");
                losehp(Shock_resistance ? rnd(6) : rnd(30),
                       killer_msg(DIED, "an electric chair"));
                exercise(A_CON, FALSE);
                break;
            case 4:
                pline("You feel much, much better!");
                if (Upolyd) {
                    if (u.mh >= (u.mhmax - 5))
                        u.mhmax += 4;
                    u.mh = u.mhmax;
                }
                if (u.uhp >= (u.uhpmax - 5))
                    u.uhpmax += 4;
                u.uhp = u.uhpmax;
                make_blinded(0L, TRUE);
                make_sick(0L, NULL, FALSE, SICK_ALL);
                if (LWounded_legs || RWounded_legs)
                    heal_legs(Wounded_leg_side);
                break;
            case 5:
                take_gold();
                break;
            case 6:
                if (u.uluck + rn2(5) < 0) {
                    pline("You feel your luck is changing.");
                    change_luck(1);
                } else
                    makewish();
                break;
            case 7:
            {
                int cnt = rn2_on_rng(10, rng_throne_result);
                
                pline("A voice echoes:");
                verbalize("Thy audience hath been summoned, %s!",
                          u.ufemale ? "Dame" : "Sire");
                while (cnt--)
                    makemon(courtmon(&u.uz, rng_main), level, u.ux, u.uy,
                            NO_MM_FLAGS);
                break;
            }
            case 8:
                pline("A voice echoes:");
                verbalize("By thy Imperious order, %s...",
                          u.ufemale ? "Dame" : "Sire");
                do_genocide(5); /* REALLY|ONTHRONE, see do_genocide() */
                break;
            case 9:
                pline("A voice echoes:");
                verbalize("A curse upon thee for sitting upon this most holy "
                          "throne!");
                if (Luck > 0) {
                    make_blinded(Blinded + rn1(100, 250), TRUE);
                } else
                    rndcurse();
                break;
            case 10:
                if (Luck < 0 || (HSee_invisible & INTRINSIC)) {
                    if (level->flags.nommap) {
                        pline("A terrible drone fills your head!");
                        make_confused(HConfusion + rnd(30), FALSE);
                    } else {
                        pline("An image forms in your mind.");
                        do_mapping();
                    }
                } else {
                    pline("Your vision becomes clear.");
                    HSee_invisible |= FROMOUTSIDE;
                    newsym(u.ux, u.uy);
                }
                break;
            case 11:
                if (Luck < 0) {
                    pline("You feel threatened.");
                    aggravate();
                } else {

                    pline("You feel a wrenching sensation.");
                    tele();     /* teleport him */
                }
                break;
            case 12:
                pline("You are granted an insight!");
                if (invent)
                    /* rn2(5) agrees w/seffects() */
                    identify_pack(rn2_on_rng(5, rng_throne_result));
                else
                    rn2_on_rng(5, rng_throne_result); /* to match */
                break;
            case 13:
                pline("Your mind turns into a pretzel!");
                make_confused(HConfusion + rn1(7, 16), FALSE);
                break;
            default:
                impossible("throne effect");
                break;
            }
        } else {
            if (is_prince(youmonst.data))
                pline("You feel very comfortable here.");
            else
                pline("You feel somehow out of place...");
        }

        if (!rn2_on_rng(3, rng_throne_result) &&
            IS_THRONE(level->locations[u.ux][u.uy].typ)) {
            /* may have teleported */
            level->locations[u.ux][u.uy].typ = ROOM;
            pline("The throne vanishes in a puff of logic.");
            newsym(u.ux, u.uy);
        }

    } else if (lays_eggs(youmonst.data)) {
        struct obj *uegg;

        if (!u.ufemale) {
            pline("Males can't lay eggs!");
            return 0;
        }

        if (u.uhunger < (int)objects[EGG].oc_nutrition) {
            pline("You don't have enough energy to lay an egg.");
            return 0;
        }

        uegg = mksobj(level, EGG, FALSE, FALSE, rng_main);
        uegg->spe = 1;
        uegg->quan = 1;
        uegg->owt = weight(uegg);
        uegg->corpsenm = egg_type_from_parent(u.umonnum, FALSE);
        uegg->known = uegg->dknown = 1;
        attach_egg_hatch_timeout(uegg);
        pline("You lay an egg.");
        dropy(uegg);
        stackobj(uegg);
        morehungry((int)objects[EGG].oc_nutrition);
    } else if (Engulfed)
        pline("There are no seats in here!");
    else
        pline("Having fun sitting on the %s?", surface(u.ux, u.uy));
    return 1;
}

/* curse a few inventory items at random! */
void
rndcurse(void)
{
    int nobj = 0;
    int cnt, onum;
    struct obj *otmp;
    static const char mal_aura[] = "You feel a malignant aura surround %s.";
    boolean saddle;

    cnt = rn2_on_rng(6, rng_rndcurse);
    saddle = !rn2_on_rng(4, rng_rndcurse);
    if (rn2_on_rng(20, rng_rndcurse) &&
        uwep && (uwep->oartifact == ART_MAGICBANE)) {
        pline(mal_aura, "the magic-absorbing blade");
        return;
    }

    if (Antimagic) {
        shieldeff(u.ux, u.uy);
        pline(mal_aura, "you");
    }

    for (otmp = invent; otmp; otmp = otmp->nobj) {
        /* gold isn't subject to being cursed or blessed */
        if (otmp->oclass == COIN_CLASS)
            continue;
        nobj++;
    }

    /*
     * With neither MR nor half spell damage: cnt is 1-6
     * With either MR or half spell damage:   cnt is 1-3
     * With both MR and half spell damage:    cnt is 1-2
     */
    cnt /= (!!Antimagic + !!Half_spell_damage + 1);
    cnt++;

    if (nobj) {
        for (; cnt > 0; cnt--) {
            onum = rnd(nobj);
            for (otmp = invent; otmp; otmp = otmp->nobj) {
                /* as above */
                if (otmp->oclass == COIN_CLASS)
                    continue;
                if (--onum == 0)
                    break;      /* found the target */
            }
            /* the !otmp case should never happen; picking an already cursed
               item happens--avoid "resists" message in that case */
            if (!otmp || otmp->cursed)
                continue;       /* next target */

            if (otmp->oartifact && spec_ability(otmp, SPFX_INTEL) &&
                rn2(10) < 8) {
                pline("%s!", Tobjnam(otmp, "resist"));
                continue;
            }

            if (otmp->blessed)
                unbless(otmp);
            else
                curse(otmp);
        }
        update_inventory();
    }

    /* treat steed's saddle as extended part of hero's inventory */
    if (saddle && u.usteed && (otmp = which_armor(u.usteed, os_saddle)) != 0 &&
        !otmp->cursed) { /* skip if already cursed */
        if (otmp->blessed)
            unbless(otmp);
        else
            curse(otmp);
        if (!Blind) {
            pline("%s %s %s.", s_suffix(
                      msgupcasefirst(y_monnam(u.usteed))),
                  aobjnam(otmp, "glow"),
                  hcolor(otmp->cursed ? "black" : "brown"));
            otmp->bknown = TRUE;
        }
    }
}

void
mrndcurse(struct monst *mtmp)
{       /* curse a few inventory items at random */
    int nobj = 0;
    int cnt, onum;
    struct obj *otmp;
    static const char mal_aura[] = "You feel a malignant aura surround %s.";

    boolean resists = resist(mtmp, 0, 0, FALSE);

    if (MON_WEP(mtmp) && (MON_WEP(mtmp)->oartifact == ART_MAGICBANE) &&
        rn2(20)) {
        pline(mal_aura, "the magic-absorbing blade");
        return;
    }

    if (resists) {
        shieldeff(mtmp->mx, mtmp->my);
        pline(mal_aura, mon_nam(mtmp));
    }

    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
        /* gold isn't subject to being cursed or blessed */
        if (otmp->oclass == COIN_CLASS)
            continue;
        nobj++;
    }
    if (nobj) {
        for (cnt = rnd(6 / ((! !resists) + 1)); cnt > 0; cnt--) {
            onum = rnd(nobj);
            for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
                /* as above */
                if (otmp->oclass == COIN_CLASS)
                    continue;
                if (--onum == 0)
                    break;      /* found the target */
            }
            /* the !otmp case should never happen; picking an already cursed
               item happens--avoid "resists" message in that case */
            if (!otmp || otmp->cursed)
                continue;       /* next target */

            if (otmp->oartifact && spec_ability(otmp, SPFX_INTEL) &&
                rn2(10) < 8) {
                pline("%s!", Tobjnam(otmp, "resist"));
                continue;
            }

            if (otmp->blessed)
                unbless(otmp);
            else
                curse(otmp);
        }
        update_inventory();
    }
}

/* remove a random INTRINSIC ability */
void
attrcurse(void)
{
    /* probably too rare to benefit from a custom RNG */
    switch (rnd(11)) {
    case 1:
        if (HFire_resistance & INTRINSIC) {
            HFire_resistance &= ~INTRINSIC;
            pline("You feel warmer.");
            break;
        }
    case 2:
        if (HTeleportation & INTRINSIC) {
            HTeleportation &= ~INTRINSIC;
            pline("You feel less jumpy.");
            update_supernatural_abilities();
            break;
        }
    case 3:
        if (HPoison_resistance & INTRINSIC) {
            HPoison_resistance &= ~INTRINSIC;
            pline("You feel a little sick!");
            break;
        }
    case 4:
        if (HTelepat & INTRINSIC) {
            HTelepat &= ~INTRINSIC;
            if (Blind && !Blind_telepat)
                see_monsters(FALSE); /* Can't sense mons anymore! */
            pline("Your senses fail!");
            break;
        }
    case 5:
        if (HCold_resistance & INTRINSIC) {
            HCold_resistance &= ~INTRINSIC;
            pline("You feel cooler.");
            break;
        }
    case 6:
        if (HInvis & INTRINSIC) {
            HInvis &= ~INTRINSIC;
            pline("You feel paranoid.");
            break;
        }
    case 7:
        if (HSee_invisible & INTRINSIC) {
            HSee_invisible &= ~INTRINSIC;
            pline("You %s!",
                  Hallucination ? "tawt you taw a puttie tat" :
                  "thought you saw something");
            break;
        }
    case 8:
        if (HFast & INTRINSIC) {
            HFast &= ~INTRINSIC;
            pline("You feel slower.");
            break;
        }
    case 9:
        if (HStealth & INTRINSIC) {
            HStealth &= ~INTRINSIC;
            pline("You feel clumsy.");
            break;
        }
    case 10:
        if (HProtection & INTRINSIC) {
            HProtection &= ~INTRINSIC;
            pline("You feel vulnerable.");
            u.ublessed = 0;
            break;
        }
    case 11:
        if (HAggravate_monster & INTRINSIC) {
            HAggravate_monster &= ~INTRINSIC;
            pline("You feel less attractive.");
            break;
        }
    default:
        break;
    }
}

/*sit.c*/

