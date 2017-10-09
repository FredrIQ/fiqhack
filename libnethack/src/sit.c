/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-09 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

void
take_gold(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    struct obj *otmp, *nobj;
    int lost_money = 0;

    for (otmp = mon->minvent; otmp; otmp = nobj) {
        nobj = otmp->nobj;
        if (otmp->oclass == COIN_CLASS) {
            lost_money = 1;
            delobj(otmp);
        }
    }
    if (!lost_money) {
        if (you)
            pline(msgc_noconsequence, "You feel a strange sensation.");
        /* no noticed effect on monsters */
    } else {
        if (you)
            pline(msgc_itemloss, "You notice you have no money!");
        else if (vis)
            pline(msgc_monneutral,
                  "%s money disappears!", s_suffix(Monnam(mon)));
    }
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
        pline(msgc_cancelled, "You are already sitting on %s.",
              mon_nam(u.usteed));
        return 0;
    }

    if (!can_reach_floor()) {
        if (Levitation)
            pline(msgc_cancelled, "You tumble in place.");
        else
            pline(msgc_cancelled, "You are sitting on air.");
        return 0;
    } else if (is_pool(level, u.ux, u.uy) && !Underwater) {
        /* water walking */
        goto in_water;
    }

    if ((trap = t_at(level, u.ux, u.uy)) != 0 && !u.utrap &&
        (trap->ttyp == HOLE || trap->ttyp == TRAPDOOR || trap->ttyp == PIT ||
         trap->ttyp == SPIKED_PIT) && trap->tseen) {
        pline(msgc_yafm, "You sit on the edge of the %s.",
              trap->ttyp == HOLE ? "hole" : trap->ttyp ==
              TRAPDOOR ? "trapdoor" : "pit");
        return 1;
    }

    if (OBJ_AT(u.ux, u.uy)) {
        struct obj *obj;

        obj = level->objects[u.ux][u.uy];
        pline(msgc_yafm, "You sit on %s.", the(xname(obj)));
        if (!(Is_box(obj) || objects[obj->otyp].oc_material == CLOTH))
            pline_implied(msgc_yafm, "It's not very comfortable...");

    } else if ((trap = t_at(level, u.ux, u.uy)) != 0 ||
               (u.utrap && (u.utraptype >= TT_LAVA))) {

        if (u.utrap) {
            exercise(A_WIS, FALSE);     /* you're getting stuck longer */
            if (u.utraptype == TT_BEARTRAP) {
                pline(msgc_badidea,
                      "You can't sit down with your %s in the bear trap.",
                      body_part(FOOT));
                u.utrap++;
            } else if (u.utraptype == TT_PIT) {
                if (trap->ttyp == SPIKED_PIT) {
                    pline(msgc_badidea, "You sit down on a spike.  Ouch!");
                    losehp(1, killer_msg(DIED, "sitting on an iron spike"));
                    exercise(A_STR, FALSE);
                } else
                    pline(msgc_badidea, "You sit down in the pit.");
                u.utrap += rn2(5);
            } else if (u.utraptype == TT_WEB) {
                pline(msgc_badidea,
                      "You sit in the spider web and get entangled further!");
                u.utrap += rn1(10, 5);
            } else if (u.utraptype == TT_LAVA) {
                /* Must have fire resistance or they'd be dead already */
                pline(msgc_badidea, "You sit in the lava!");
                u.utrap += rnd(4);
                losehp(dice(2, 10), killer_msg(DIED, "sitting in lava"));
            } else if (u.utraptype == TT_INFLOOR) {
                pline(msgc_badidea, "You can't maneuver to sit!");
                u.utrap++;
            }
        } else {
            pline(msgc_actionok, "You sit down.");
            dotrap(trap, 0);
        }
    } else if (Underwater || Is_waterlevel(&u.uz)) {
        if (Is_waterlevel(&u.uz))
            pline(msgc_cancelled1, "There are no cushions floating nearby.");
        else
            pline(msgc_yafm, "You sit down on the muddy bottom.");
    } else if (is_pool(level, u.ux, u.uy)) {
    in_water:
        pline(msgc_badidea, "You sit in the water.");
        if (!rn2(10) && uarm)
            water_damage(uarm, "armor", TRUE);
        if (!rn2(10) && uarmf && uarmf->otyp != WATER_WALKING_BOOTS)
            water_damage(uarm, "armor", TRUE);
    } else if (IS_SINK(typ)) {

        pline(msgc_yafm, sit_message, defexplain[S_sink]);
        pline(msgc_yafm, "Your %s gets wet.",
              humanoid(youmonst.data) ? "rump" : "underside");
    } else if (IS_ALTAR(typ)) {

        pline(msgc_badidea, sit_message, defexplain[S_altar]);
        altar_wrath(u.ux, u.uy);

    } else if (IS_GRAVE(typ)) {

        pline(msgc_yafm, sit_message, defexplain[S_grave]);

    } else if (typ == STAIRS) {

        pline(msgc_yafm, sit_message, "stairs");

    } else if (typ == LADDER) {

        pline(msgc_yafm, sit_message, "ladder");

    } else if (is_lava(level, u.ux, u.uy)) {

        /* must be WWalking */
        pline(msgc_yafm, sit_message, "lava");
        burn_away_slime(&youmonst);
        if (likes_lava(youmonst.data)) {
            pline(msgc_yafm, "The lava feels warm.");
            return 1;
        }
        pline(msgc_badidea, "The lava burns you!");
        losehp(dice((Fire_resistance ? 2 : 10), 10),
               killer_msg(DIED, "sitting on lava"));

    } else if (is_ice(level, u.ux, u.uy)) {

        pline(msgc_yafm, sit_message, defexplain[S_ice]);
        if (!Cold_resistance)
            pline(msgc_yafm, "The ice feels cold.");

    } else if (typ == DRAWBRIDGE_DOWN) {

        pline(msgc_yafm, sit_message, "drawbridge");

    } else if (IS_THRONE(typ)) {

        pline(msgc_actionok, sit_message, defexplain[S_throne]);
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
                pline(Shock_resistance ? msgc_notresisted : msgc_nonmonbad,
                      "A%s electric shock shoots through your body!",
                      (Shock_resistance) ? "n" : " massive");
                losehp(Shock_resistance ? rnd(6) : rnd(30),
                       killer_msg(DIED, "an electric chair"));
                exercise(A_CON, FALSE);
                break;
            case 4:
                if (Upolyd) {
                    if (u.mh >= (u.mhmax - 5))
                        u.mhmax += 4;
                    u.mh = u.mhmax;
                }
                /* TODO: Different wording in these two cases, not just
                   different channel */
                if (u.uhp >= (u.uhpmax - 5)) {
                    pline(msgc_intrgain, "You feel much, much better!");
                    u.uhpmax += 4;
                } else {
                    pline(msgc_statusheal, "You feel much, much better!");
                }
                u.uhp = u.uhpmax;
                set_property(&youmonst, BLINDED, -2, FALSE);
                make_sick(&youmonst, 0L, NULL, FALSE, SICK_ALL);
                if (leg_hurt(&youmonst))
                    heal_legs(&youmonst, leg_hurtsides(&youmonst));
                break;
            case 5:
                take_gold(&youmonst);
                break;
            case 6:
                /* TODO: Set this up on the throne RNG? */
                if (u.uluck + rn2(5) < 0) {
                    pline(msgc_statusheal, "You feel your luck is changing.");
                    change_luck(1);
                } else
                    makewish();
                break;
            case 7:
            {
                int cnt = rn2_on_rng(10, rng_throne_result);
                
                pline(msgc_npcvoice, "A voice echoes:");
                verbalize(msgc_levelwarning,
                          "Thy audience hath been summoned, %s!",
                          u.ufemale ? "Dame" : "Sire");
                while (cnt--)
                    makemon(courtmon(&u.uz, rng_main), level, u.ux, u.uy,
                            NO_MM_FLAGS);
                break;
            }
            case 8:
                pline(msgc_npcvoice, "A voice echoes:");
                verbalize(msgc_intrgain, "By thy Imperious order, %s...",
                          u.ufemale ? "Dame" : "Sire");
                do_genocide(&youmonst, 5, FALSE); /* REALLY|ONTHRONE, see do_genocide() */
                break;
            case 9:
                pline(msgc_npcvoice, "A voice echoes:");
                verbalize(Luck > 0 ? msgc_statusbad : msgc_itemloss,
                          "A curse upon thee for sitting upon this most holy "
                          "throne!");
                if (Luck > 0)
                    inc_timeout(&youmonst, BLINDED, rn1(100, 250), FALSE);
                else
                    rndcurse(&youmonst, NULL);
                break;
            case 10:
                if (Luck < 0 || see_invisible(&youmonst)) {
                    if (level->flags.nommap) {
                        pline(msgc_statusbad,
                              "A terrible drone fills your head!");
                        inc_timeout(&youmonst, CONFUSION, rnd(30), TRUE);
                    } else {
                        pline(msgc_youdiscover, "An image forms in your mind.");
                        do_mapping();
                    }
                } else {
                    pline(msgc_intrgain, "Your vision becomes clear.");
                    set_property(&youmonst, SEE_INVIS, 0, FALSE);
                    newsym(u.ux, u.uy);
                }
                break;
            case 11:
                if (Luck < 0) {
                    pline(msgc_statusbad, "You feel threatened.");
                    aggravate();
                } else {
                    pline(msgc_nonmonbad, "You feel a wrenching sensation.");
                    tele();     /* teleport him */
                }
                break;
            case 12:
                pline(msgc_youdiscover, "You are granted an insight!");
                if (youmonst.minvent)
                    /* rn2(5) agrees w/seffects() */
                    identify_pack(&youmonst, rn2_on_rng(5, rng_throne_result),
                                  P_EXPERT);
                else
                    rn2_on_rng(5, rng_throne_result); /* to match */
                break;
            case 13:
                pline(msgc_statusbad, "Your mind turns into a pretzel!");
                inc_timeout(&youmonst, CONFUSION, rn1(7, 16), TRUE);
                break;
            default:
                impossible("throne effect");
                break;
            }
        } else {
            if (is_prince(youmonst.data))
                pline(msgc_failrandom, "You feel very comfortable here.");
            else
                pline(msgc_failrandom, "You feel somehow out of place...");
        }

        if (!rn2_on_rng(3, rng_throne_result) &&
            IS_THRONE(level->locations[u.ux][u.uy].typ)) {
            /* may have teleported */
            level->locations[u.ux][u.uy].typ = ROOM;
            pline(msgc_consequence,
                  "The throne vanishes in a puff of logic.");
            newsym(u.ux, u.uy);
        }

    } else if (lays_eggs(youmonst.data)) {
        struct obj *uegg;

        if (!u.ufemale) {
            pline(msgc_cancelled, "Males can't lay eggs!");
            return 0;
        }

        if (u.uhunger < (int)objects[EGG].oc_nutrition) {
            pline(msgc_cancelled,
                  "You don't have enough energy to lay an egg.");
            return 0;
        }

        uegg = mksobj(level, EGG, FALSE, FALSE, rng_main);
        uegg->spe = 1;
        uegg->quan = 1;
        uegg->owt = weight(uegg);
        uegg->corpsenm = egg_type_from_parent(u.umonnum, FALSE);
        uegg->known = uegg->dknown = 1;
        attach_egg_hatch_timeout(uegg);
        pline(msgc_actionok, "You lay an egg.");
        dropy(uegg);
        stackobj(uegg);
        morehungry((int)objects[EGG].oc_nutrition);
    } else if (Engulfed)
        pline(msgc_cancelled1, "There are no seats in here!");
    else
        pline(msgc_yafm, "Having fun sitting on the %s?", surface(u.ux, u.uy));
    return 1;
}

void
rndcurse(struct monst *mtmp, struct monst *magr)
{       /* curse a few inventory items at random */
    int nobj = 0;
    int cnt, onum;
    struct obj *otmp;
    static const char mal_aura[] = "You feel a malignant aura surround %s.";
    boolean saddle;
    boolean you = (mtmp == &youmonst);
    boolean vis = canseemon(mtmp);
    enum rng rng = (you ? rng_rndcurse : rng_main);
    cnt = rn2_on_rng(18, rng);
    saddle = (you && !rn2_on_rng(4, rng));

    if ((otmp = m_mwep(mtmp)) && otmp->oartifact == ART_MAGICBANE) {
        if (you || vis)
            pline(combat_msgc(magr, mtmp, cr_miss), mal_aura,
                  "the magic-absorbing blade");
        cnt = rn2_on_rng(6, rng);
    }
    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
        /* gold isn't subject to being cursed or blessed */
        if (otmp->oclass == COIN_CLASS)
            continue;
        nobj++;
    }

    /*
     * With nothing:                          cnt is 1-18
     * With one of MR or half spell damage:   cnt is 1-9
     * With Magicbane or MR+half spell:       cnt is 1-6
     * With Magicbane and MR or half spell:   cnt is 1-3
     * With Magicbane, MR and half spell:     cnt is 1-2
     */
    cnt /= (!!resists_magm(mtmp) + !!half_spell_dam(mtmp) + 1);
    cnt++;

    if (nobj) {
        for (; cnt > 0; cnt--) {
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
                pline(combat_msgc(magr, mtmp, cr_miss), "%s!",
                      Tobjnam(otmp, "resist"));
                continue;
            }

            if (otmp->blessed)
                unbless(otmp);
            else
                curse(otmp);
        }
        update_inventory();
    }

    /* treat steed's saddle as extended part of inventory */
    if (saddle && u.usteed && (otmp = which_armor(u.usteed, os_saddle)) != 0 &&
        !otmp->cursed) { /* skip if already cursed */
        if (otmp->blessed)
            unbless(otmp);
        else
            curse(otmp);
        if (!Blind) {
            pline(msgc_itemloss, "%s %s %s.", s_suffix(
                      msgupcasefirst(y_monnam(u.usteed))),
                  aobjnam(otmp, "glow"),
                  hcolor(otmp->cursed ? "black" : "brown"));
            otmp->bknown = TRUE;
        }
    }
}

/*sit.c*/
