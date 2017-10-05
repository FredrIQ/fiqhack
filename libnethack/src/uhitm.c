/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static boolean known_hitum(struct monst *, int *, const struct attack *, schar,
                           schar, int);
static void steal_it(struct monst *, const struct attack *);
static boolean hitum(struct monst *, int, const struct attack *, schar, schar);
static boolean hmon_hitmon(struct monst *, struct obj *, int,
                           int);
static int joust(struct monst *, struct obj *);
static boolean m_slips_free(struct monst *mtmp, const struct attack *mattk);
static int explum(struct monst *, const struct attack *);
static void start_engulf(struct monst *);
static void end_engulf(struct monst *);
static int gulpum(struct monst *, const struct attack *);
static boolean hmonas(struct monst *, int, schar, schar);
static void nohandglow(struct monst *);
static boolean shade_aware(struct obj *);

#define PROJECTILE(obj) ((obj) && is_ammo(obj))

/*
 * It is unchivalrous for a knight to attack the defenseless or from behind.
 */
void
check_caitiff(struct monst *mtmp)
{
    if (Role_if(PM_KNIGHT) && u.ualign.type == A_LAWFUL &&
        (!mtmp->mcanmove || mtmp->msleeping || (mtmp->mflee && !mtmp->mavenge))
        && u.ualign.record > -10) {
        pline(msgc_alignbad, "You caitiff!");
        adjalign(-1);
    }
}

schar
find_roll_to_hit(struct monst *mtmp)
{
    schar tmp;
    int tmp2;

    tmp =
        1 + Luck + abon() + find_mac(mtmp) + hitbon(&youmonst) +
        maybe_polyd(youmonst.data->mlevel, u.ulevel);

    check_caitiff(mtmp);

    /* attacking peaceful creatures is bad for the samurai's giri */
    if (Role_if(PM_SAMURAI) && mtmp->mpeaceful && u.ualign.record > -10) {
        pline(msgc_alignbad, "You dishonorably attack the innocent!");
        adjalign(-1);
    }

    /* Adjust vs. (and possibly modify) monster state. */

    if (stunned(mtmp))
        tmp += 2;
    if (mtmp->mflee)
        tmp += 2;

    if (mtmp->msleeping) {
        mtmp->msleeping = 0;
        tmp += 2;
    }
    if (!mtmp->mcanmove) {
        tmp += 4;
        if (!rn2(10)) {
            mtmp->mcanmove = 1;
            mtmp->mfrozen = 0;
        }
    }
    if (is_orc(mtmp->data) &&
        maybe_polyd(is_elf(youmonst.data), Race_if(PM_ELF)))
        tmp++;
    if (Role_if(PM_MONK) && !Upolyd) {
        if (uarm && !uskin()) {
            /* give people two different ways to turn this off without having
               to suppress msgc_hint altogether */
            if (flags.verbose)
                pline_implied(msgc_hint, "Your armor is rather cumbersome...");
            tmp -= urole.spelarmr;
        } else if (!uwep && !uarms) {
            tmp += (u.ulevel / 3) + 2;
        }
    }

/* with a lot of luggage, your agility diminishes */
    if ((tmp2 = near_capacity()) != 0)
        tmp -= (tmp2 * 2) - 1;
    if (u.utrap)
        tmp -= 3;
/* Some monsters have a combination of weapon attacks and non-weapon attacks.
   It is therefore wrong to add hitval to tmp; we must add it only for the
   specific attack (in hmonas()). */
    if (!Upolyd) {
        tmp += hitval(uwep, mtmp);
        tmp += weapon_hit_bonus(uwep); /* picks up bare-handed bonus */
    }
    return tmp;
}

/*
 * Called when the user does something that might attack a monster with uwep.
 * "confirmed" is TRUE if the caller checked that the character wants to attack;
 * FALSE if this function needs to do the check. (This is mostly to reduce code
 * duplication in the callers; most but not all will want a standard check
 * against using flags.u_interaction_mode and weird_attack == TRUE.)
 *
 * Return values:
 * ac_continue = The monster dodged the attack; uwep should continue moving past
 *     the monster to do whatever it would do anyway.
 * ac_cancel = The user cancelled the attack; the entire action should be
 *     cancelled, if that makes sense in context (or if it would be plausible
 *     for the character to attack only a subset of monsters in an area attack,
 *     simply avoid that specific monster).
 * ac_somethingelse = Something else happened entirely; the turn should end
 *     here.
 * ac_monsterhit = uwep hit the monster, that's it for the turn.
 */
enum attack_check_status
attack(struct monst *mtmp, schar dx, schar dy, boolean confirmed)
{
    schar tmp;
    const struct permonst *mdat = mtmp->data;

    /* Checks that prevent attacking altogether: do this to avoid a
       map_invisible call (the player doesn't try to attack, so won't detect a
       monster via collision). */
    if (Upolyd) {
        /* certain "pacifist" monsters don't attack */
        if (noattacks(youmonst.data)) {
            pline(msgc_cancelled,
                  "You have no way to attack monsters physically.");
            if (idle(mtmp))
                mtmp->mstrategy = st_none;
            return ac_cancel;
        }
    }

    if (check_capacity("You cannot fight while so heavily loaded."))
        return ac_cancel;

    /* if the caller didn't check to see if we even want to attack, do that
       now */
    if (!confirmed) {
        if (resolve_uim(flags.interaction_mode, TRUE, u.ux + dx, u.uy + dy) ==
            uia_halt)
            return ac_cancel;
    }

    if (u.twoweap && !can_twoweapon())
        untwoweapon();

    if (u.bashmsg) {
        u.bashmsg = FALSE;
        if (uwep)
            pline(msgc_fatalavoid, "You begin bashing monsters with your %s.",
                  aobjnam(uwep, NULL));
        else if (!cantwield(youmonst.data))
            pline(Role_if(PM_MONK) ? msgc_occstart : msgc_fatalavoid,
                  "You begin %sing monsters with your %s %s.",
                  Role_if(PM_MONK) ? "strik" : "bash",
                  uarmg ? "gloved" : "bare",      /* Del Lamb */
                  makeplural(body_part(HAND)));
    }
    exercise(A_STR, TRUE);      /* you're exercising muscles */
    /* andrew@orca: prevent unlimited pick-axe attacks */
    u_wipe_engr(3);

    /* Is the "it died" check actually correct? */
    if (mdat->mlet == S_LEPRECHAUN &&
        !mtmp->mfrozen && !mtmp->msleeping && !confused(mtmp) &&
        !blind(mtmp) && !rn2(7) && (m_move(mtmp, 0) == 2 || /* it died */
                                     mtmp->mx != u.ux + dx ||
                                     mtmp->my != u.uy + dy)) /* it moved */
        return ac_continue;

    tmp = find_roll_to_hit(mtmp);
    if (Upolyd)
        hmonas(mtmp, tmp, dx, dy);
    else
        hitum(mtmp, tmp, youmonst.data->mattk, dx, dy);
    if (idle(mtmp))
        mtmp->mstrategy = st_none;

    return ac_monsterhit;
}

/* returns TRUE if monster still lives */
static boolean
known_hitum(struct monst *mon, int *mhit, const struct attack *uattk, schar dx,
            schar dy, int dieroll)
{
    boolean malive = TRUE;

    if (!*mhit) {
        missum(mon, uattk);
    } else {
        int oldhp = mon->mhp;
        int x = u.ux + dx;
        int y = u.uy + dy;

        /* Save current conduct state in case we revert it later on a forced
           miss. */

        int curr_weaphit = u.uconduct[conduct_weaphit];
        int turn = u.uconduct_time[conduct_weaphit];

        /* KMH, conduct */
        if (uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep)))
            break_conduct(conduct_weaphit);

        /* we hit the monster; be careful: it might die or be knocked into a
           different location */
        notonhead = (mon->mx != x || mon->my != y);
        malive = hmon(mon, uwep, 0, dieroll);
        /* this assumes that Stormbringer was uwep not uswapwep */
        if (malive && u.twoweap && m_at(level, x, y) == mon)
            malive = hmon(mon, uswapwep, 0, dieroll);
        if (malive) {
            /* monster still alive */
            if (!rn2(25) && mon->mhp < mon->mhpmax / 2 &&
                !(Engulfed && mon == u.ustuck)) {
                /* maybe should regurgitate if swallowed? */
                if (!rn2(3)) {
                    monflee(mon, rnd(100), FALSE, TRUE);
                } else
                    monflee(mon, 0, FALSE, TRUE);

                if (u.ustuck == mon && !Engulfed && !sticks(youmonst.data))
                    u.ustuck = 0;
            }
            /* Vorpal Blade hit converted to miss */
            /* could be headless monster or worm tail */
            if (mon->mhp == oldhp) {
                *mhit = 0;
                /* a miss does not break conduct */
                if (uwep && (uwep->oclass == WEAPON_CLASS ||
                    is_weptool(uwep))) {
                    u.uconduct[conduct_weaphit] = curr_weaphit;
                    u.uconduct_time[conduct_weaphit] = turn;
                }
            }
            if (mon->wormno && *mhit)
                cutworm(mon, x, y, uwep);
        }
    }
    return malive;
}

/* returns TRUE if monster still lives */
static boolean
hitum(struct monst *mon, int tmp, const struct attack *uattk, schar dx,
      schar dy)
{
    boolean malive;
    int dieroll;
    int mhit;
    if (uwep && uwep->oartifact == ART_CLEAVER && !Engulfed &&
        (!Upolyd || youmonst.data != &mons[PM_GRID_BUG])) {
        /* cleaves */
        int i = 0;
        int x = m_mx(&youmonst);
        int y = m_my(&youmonst);
        struct monst *mtmp;
        while (i < 8) {
            if (xdir[i] == dx && ydir[i] == dy)
                break;
            i++;
        }

        if (i == 8)
            panic("hitum: failed to find target monster?");
        i += 2;
        if (i >= 8)
            i -= 8;

        int count = 3;
        boolean result;
        while (count-- && uwep) {
            if (!i)
                i = 7;
            else
                i--;

            mtmp = NULL;
            if (isok(x + xdir[i], y + ydir[i]))
                mtmp = um_at(level, x + xdir[i], y + ydir[i]);
            if (!mtmp)
                continue;

            /* Don't hit stuff in an arc which we can't see */
            if (mon != mtmp && !canspotmon(mtmp) &&
                !level->locations[x + xdir[i]][y + ydir[i]].mem_invis)
                continue;

            /* Unless the player was hitting a peaceful primarily,
               let the player avoid noconfirm attacks vs peacefuls. */
            if (!mon->mpeaceful && !mon->mtame &&
                (mtmp->mpeaceful || mtmp->mtame))
                continue;

            dieroll = rnd(20);
            mhit = (tmp > dieroll);
            result = known_hitum(mtmp, &mhit, uattk, xdir[i], ydir[i], dieroll);
            passive(mon, mhit, DEADMONSTER(mtmp), AT_WEAP);
            if (mon == mtmp)
                malive = result;
        }

        return malive;
    }

    dieroll = rnd(20);
    mhit = (tmp > dieroll || Engulfed);

    if (tmp > dieroll)
        exercise(A_DEX, TRUE);
    malive = known_hitum(mon, &mhit, uattk, dx, dy, dieroll);
    passive(mon, mhit, malive, AT_WEAP);
    return malive;
}

/* general "damage monster" routine */
/* return TRUE if mon still alive */
boolean
hmon(struct monst * mon, struct obj * obj, int thrown,
     int dieroll)
{
    boolean result, anger_guards;

    anger_guards = (mon->mpeaceful &&
                    (ispriest(mon) || mx_eshk(mon) ||
                     mon->data == &mons[PM_WATCHMAN] ||
                     mon->data == &mons[PM_WATCH_CAPTAIN]));
    result = hmon_hitmon(mon, obj, thrown, dieroll);
    if (ispriest(mon) && !rn2(2))
        ghod_hitsu(mon);
    if (anger_guards)
        angry_guards(!canhear());
    return result;
}

/* guts of hmon() */
static boolean
hmon_hitmon(struct monst *mon, struct obj *obj, int thrown,
            int dieroll)
{
    int tmp;
    const struct permonst *mdat = mon->data;
    int barehand_silver_rings = 0;

    /* The basic reason we need all these booleans is that we don't want a
       "hit" message when a monster dies, so we have to know how much damage it
       did _before_ outputting a hit message, but any messages associated with
       the damage don't come out until _after_ outputting a hit message. */
    boolean hittxt = FALSE, destroyed = FALSE, already_killed = FALSE;
    boolean get_dmg_bonus = TRUE;
    boolean ispoisoned = FALSE;
    boolean silvermsg = FALSE, silverobj = FALSE;
    boolean valid_weapon_attack = FALSE;
    boolean unarmed = !uwep && (!uarm || uskin()) && !uarms;
    boolean mercy = FALSE;
    int jousting = 0;
    int wtype;
    struct obj *monwep;
    const char *unconventional, *saved_oname, *poistxt;

    unconventional = "";
    saved_oname = "";

    wakeup(mon, TRUE);
    if (!obj) { /* attack with bare hands */
        if (mdat == &mons[PM_SHADE])
            tmp = 0;
        else if (martial_bonus())
            tmp = rnd(4);       /* bonus for martial arts */
        else
            tmp = rnd(2);
        valid_weapon_attack = (martial_bonus() || tmp > 1 || rn2(8));
        /* blessed gloves give bonuses when fighting 'bare-handed' */
        if (uarmg) {
            if (uarmg->blessed && (is_undead(mdat) || is_demon(mdat)))
                tmp += rnd(4);
            if (martial_bonus() && uarmg->spe > 0)
                tmp += uarmg->spe;
        }
        /* So do silver rings.  Note: rings are worn under gloves, so you don't
           get both bonuses. */
        if (!uarmg) {
            if (((uleft && uleft->blessed) || (uright && uright->blessed)) &&
                (is_undead(mdat) || is_demon(mdat)))
                tmp += rnd(4); /* blessed damage */
            if (uleft && objects[uleft->otyp].oc_material == SILVER)
                barehand_silver_rings++;
            if (uright && objects[uright->otyp].oc_material == SILVER)
                barehand_silver_rings++;
            if (barehand_silver_rings && hates_silver(mdat)) {
                tmp += rnd(20);
                silvermsg = TRUE;
            }
        }
    } else {
        if (!thrown && (obj_properties(obj) & opm_nasty)) {
            pline(combat_msgc(NULL, &youmonst, cr_hit),
                  "The nasty weapon hurts you!");
            learn_oprop(obj, opm_nasty);
            losehp(rnd(6), killer_msg(DIED, "a nasty weapon"));
        }

        saved_oname = cxname(obj);
        if (obj->oclass == WEAPON_CLASS || is_weptool(obj) ||
            obj->oclass == GEM_CLASS) {

            /* is it not a melee weapon? */
            if (        /* if you strike with a bow... */
                   is_launcher(obj) ||
                   /* or strike with a missile in your hand... */
                   (!thrown && (is_missile(obj) || is_ammo(obj))) ||
                   /* or use a pole at short range and not mounted... */
                   (!thrown && !u.usteed && is_pole(obj)) ||
                   /* or throw a missile without the proper bow... */
                   (is_ammo(obj) && !ammo_and_launcher(obj, uwep))) {
                /* then do only 1-2 points of damage */
                if (mdat == &mons[PM_SHADE] && obj->otyp != SILVER_ARROW)
                    tmp = 0;
                else
                    tmp = rnd(2);

                /* need to duplicate this check for silver arrows: they aren't
                   caught below as they're weapons, and aren't caught in dmgval
                   as they aren't melee weapons. */
                if (objects[obj->otyp].oc_material == SILVER &&
                    hates_silver(mdat)) {
                    silvermsg = TRUE;
                    silverobj = TRUE;
                    tmp += rnd(20);
                }
                if (!thrown && obj == uwep && obj->otyp == BOOMERANG &&
                    rnl(4) == 4 - 1) {
                    boolean more_than_1 = (obj->quan > 1L);

                    pline(msgc_itemloss,
                          "As you hit %s, %s%s %s breaks into splinters.",
                          mon_nam(mon), more_than_1 ? "one of " : "",
                          shk_your(obj), xname(obj));
                    if (!more_than_1)
                        uwepgone();     /* set bashmsg */
                    useup(obj);
                    if (!more_than_1)
                        obj = NULL;
                    hittxt = TRUE;
                    if (mdat != &mons[PM_SHADE])
                        tmp++;
                }
            } else {
                tmp = dmgval(obj, mon);
                /* a minimal hit doesn't exercise proficiency */
                valid_weapon_attack = (tmp > 1);
                if (!valid_weapon_attack || mon == u.ustuck || u.twoweap) {
                    ;   /* no special bonuses */
                } else if (mon->mflee && Role_if(PM_ROGUE) && !Upolyd) {
                    pline(msgc_combatalert, "You strike %s from behind!",
                          mon_nam(mon));
                    tmp += rnd(u.ulevel);
                    hittxt = TRUE;
                } else if (dieroll == 2 && obj == uwep &&
                           obj->oclass == WEAPON_CLASS &&
                           (bimanual(obj) ||
                            (Role_if(PM_SAMURAI) && obj->otyp == KATANA &&
                             !uarms)) &&
                           ((wtype = uwep_skill_type()) != P_NONE &&
                            P_SKILL(wtype) >= P_SKILLED) &&
                           ((monwep = MON_WEP(mon)) != 0 &&
                            !is_flimsy(monwep) &&
                            !obj_resists(monwep,
                                         50 + 15 * greatest_erosion(obj),
                                         100))) {
                    /*
                     * 2.5% chance of shattering defender's weapon when
                     * using a two-handed weapon; less if uwep is rusted.
                     * [dieroll == 2 is most successful non-beheading or
                     * -bisecting hit, in case of special artifact damage;
                     * the percentage chance is (1/20)*(50/100).]
                     */
                    setmnotwielded(mon, monwep);
                    MON_NOWEP(mon);
                    mon->weapon_check = NEED_WEAPON;
                    pline(msgc_combatalert,
                          "%s %s shatters from the force of your blow!",
                          s_suffix(Monnam(mon)), cxname2(monwep));
                    m_useup(mon, monwep);
                    /* If someone just shattered MY weapon, I'd flee! */
                    if (rn2(4)) {
                        monflee(mon, dice(2, 3), TRUE, TRUE);
                    }
                    hittxt = TRUE;
                }

                if ((obj->oartifact || obj->oprops) &&
                    artifact_hit(&youmonst, mon, obj, &tmp, dieroll)) {
                    if (DEADMONSTER(mon))  /* artifact killed monster */
                        return FALSE;
                    if (tmp == 0)
                        return TRUE;
                    hittxt = TRUE;
                }
                if (!thrown &&
                    (obj_properties(obj) & opm_mercy))
                    mercy = TRUE;

                if (objects[obj->otyp].oc_material == SILVER &&
                    hates_silver(mdat)) {
                    silvermsg = TRUE;
                    silverobj = TRUE;
                }
                if (u.usteed && !thrown && tmp > 0 &&
                    weapon_type(obj) == P_LANCE && mon != u.ustuck) {
                    jousting = joust(mon, obj);
                    /* exercise skill even for minimal damage hits */
                    if (jousting)
                        valid_weapon_attack = TRUE;
                }
                if (thrown && (is_ammo(obj) || is_missile(obj))) {
                    if (ammo_and_launcher(obj, uwep)) {
                        /* Elves and Samurai do extra damage using their
                           bows&arrows; they're highly trained. */
                        if (Role_if(PM_SAMURAI) && obj->otyp == YA &&
                            uwep->otyp == YUMI)
                            tmp++;
                        else if (Race_if(PM_ELF) && obj->otyp == ELVEN_ARROW &&
                                 uwep->otyp == ELVEN_BOW)
                            tmp++;
                    }
                    if (obj->opoisoned && is_poisonable(obj)) {
                        ispoisoned = TRUE;
                        poistxt = xname(obj);
                    }
                }
            }
        } else if (obj->oclass == POTION_CLASS) {
            if (obj->quan > 1L)
                obj = splitobj(obj, 1L);
            else if (obj == uwep)
                setuwep(NULL);
            obj_extract_self(obj);
            potionhit(mon, obj, &youmonst);
            if (DEADMONSTER(mon))
                return FALSE;   /* killed */
            hittxt = TRUE;
            /* in case potion effect causes transformation */
            mdat = mon->data;
            tmp = (mdat == &mons[PM_SHADE]) ? 0 : 1;
        } else {
            if (mdat == &mons[PM_SHADE] && !shade_aware(obj)) {
                tmp = 0;
                unconventional = cxname(obj);
            } else {
                switch (obj->otyp) {
                case BOULDER:  /* 1d20 */
                case HEAVY_IRON_BALL:  /* 1d25 */
                case IRON_CHAIN:       /* 1d4+1 */
                    tmp = dmgval(obj, mon);
                    break;
                case MIRROR:
                    if (breaktest(obj)) {
                        pline(msgc_statusbad,
                              "You break %s %s.  That's bad luck!",
                              shk_your(obj), simple_typename(obj->otyp));
                        change_luck(-2);
                        useup(obj);
                        obj = NULL;
                        unarmed = FALSE;        /* avoid obj==0 confusion */
                        get_dmg_bonus = FALSE;
                        hittxt = TRUE;
                    }
                    tmp = 1;
                    break;
                case EXPENSIVE_CAMERA:
                    pline(msgc_itemloss,
                          "You succeed in destroying %s camera.  "
                          "Congratulations!", shk_your(obj));
                    useup(obj);
                    return TRUE;
                case CORPSE:   /* fixed by polder@cs.vu.nl */
                    if (touch_petrifies(&mons[obj->corpsenm])) {
                        static const char withwhat[] = "corpse";

                        tmp = 1;
                        hittxt = TRUE;
                        pline_implied(combat_msgc(&youmonst, mon, cr_hit),
                                      "You hit %s with %s %s.", mon_nam(mon),
                                      obj->dknown ?
                                      the(mons[obj->corpsenm].mname) :
                                      an(mons[obj->corpsenm].mname),
                                      (obj->quan > 1) ?
                                      makeplural(withwhat) : withwhat);
                        mstiffen(mon, &youmonst);
                        return !DEADMONSTER(mon);
                    }
                    tmp = (obj->corpsenm >=
                           LOW_PM ? mons[obj->corpsenm].msize : 0) + 1;
                    break;
                case EGG:
                    {
#define useup_eggs(o) { if (thrown) obfree(o,NULL); \
                        else useupall(o); \
                        o = NULL; }     /* now gone */
                        long cnt = obj->quan;

                        tmp = 1;        /* nominal physical damage */
                        get_dmg_bonus = FALSE;
                        hittxt = TRUE;  /* message always given */
                        /* egg is always either used up or transformed, so next
                           hand-to-hand attack should yield a "bashing" mesg */
                        if (obj == uwep)
                            u.bashmsg = FALSE;
                        if (obj->spe && obj->corpsenm >= LOW_PM) {
                            if (obj->quan < 5)
                                change_luck((schar) - (obj->quan));
                            else
                                change_luck(-5);
                        }

                        if (touch_petrifies(&mons[obj->corpsenm])) {
                            /* learn_egg_type(obj->corpsenm); */
                            pline(combat_msgc(&youmonst, mon, cr_hit),
                                  "Splat! You hit %s with %s %s egg%s!",
                                  mon_nam(mon),
                                  obj->known ? "the" : cnt > 1L ? "some" : "a",
                                  obj->known ? mons[obj->corpsenm].
                                  mname : "petrifying", plur(cnt));
                            obj->known = 1;     /* (not much point...) */
                            useup_eggs(obj);
                            mstiffen(mon, &youmonst);
                            return !DEADMONSTER(mon);
                        } else {        /* ordinary egg(s) */
                            const char *eggp =
                                (obj->corpsenm != NON_PM &&
                                 obj->known) ? the(mons[obj->corpsenm].mname) :
                                (cnt > 1L) ? "some" : "an";

                            pline(combat_msgc(&youmonst, mon, cr_hit),
                                  "You hit %s with %s egg%s.", mon_nam(mon),
                                  eggp, plur(cnt));
                            if (touch_petrifies(mdat) && !stale_egg(obj)) {
                                pline(msgc_consequence,
                                      "The egg%s %s alive any more...",
                                      plur(cnt),
                                      (cnt == 1L) ? "isn't" : "aren't");
                                if (obj->timed)
                                    obj_stop_timers(obj);
                                obj->otyp = ROCK;
                                obj->oclass = GEM_CLASS;
                                obj->oartifact = 0;
                                obj->spe = 0;
                                obj->known = obj->dknown = obj->bknown = 0;
                                obj->owt = weight(obj);
                                if (thrown)
                                    place_object(obj, level, mon->mx, mon->my);
                            } else {
                                pline(msgc_yafm, "Splat!");
                                useup_eggs(obj);
                                exercise(A_WIS, FALSE);
                            }
                        }
                        break;
#undef useup_eggs
                    }
                case CLOVE_OF_GARLIC:  /* no effect against demons */
                    if (is_undead(mdat)) {
                        monflee(mon, dice(2, 4), FALSE, TRUE);
                    }
                    tmp = 1;
                    break;
                case CREAM_PIE:
                case BLINDING_VENOM:
                    mon->msleeping = 0;
                    if (can_blnd(&youmonst, mon, (uchar)
                                 (obj->otyp ==
                                  BLINDING_VENOM ? AT_SPIT : AT_WEAP), obj)) {
                        if (Blind) {
                            pline(msgc_levelsound, obj->otyp == CREAM_PIE ?
                                  "Splat!" : "Splash!");
                        } else if (obj->otyp == BLINDING_VENOM) {
                            pline(combat_msgc(&youmonst, mon, cr_hit),
                                  "The venom blinds %s%s!", mon_nam(mon),
                                  !blind(mon) ? "" : " further");
                        } else {
                            const char *whom = mon_nam(mon);
                            const char *what = The(xname(obj));

                            if (!thrown && obj->quan > 1)
                                what = An(singular(obj, xname));
                            /* note: s_suffix returns a modifiable buffer */
                            if (haseyes(mdat)
                                && mdat != &mons[PM_FLOATING_EYE])
                                whom = msgcat_many(
                                    s_suffix(whom), " ",
                                    mbodypart(mon, FACE), NULL);
                            pline(combat_msgc(&youmonst, mon, cr_hit),
                                  "%s %s over %s!", what,
                                  vtense(what, "splash"), whom);
                        }
                        setmangry(mon);
                        set_property(mon, BLINDED, rnd(25), FALSE);
                    } else {
                        pline(combat_msgc(&youmonst, mon, cr_hit),
                              obj->otyp == CREAM_PIE ? "Splat!" : "Splash!");
                        setmangry(mon);
                    }
                    if (thrown)
                        obfree(obj, NULL);
                    else
                        useup(obj);
                    hittxt = TRUE;
                    get_dmg_bonus = FALSE;
                    tmp = 0;
                    break;
                case ACID_VENOM:       /* thrown (or spit) */
                    if (resists_acid(mon)) {
                        pline(combat_msgc(&youmonst, mon, cr_immune),
                              "Your venom hits %s harmlessly.", mon_nam(mon));
                        tmp = 0;
                    } else {
                        pline(combat_msgc(&youmonst, mon, cr_hit),
                              "Your venom burns %s!", mon_nam(mon));
                        tmp = dmgval(obj, mon);
                    }
                    if (thrown)
                        obfree(obj, NULL);
                    else
                        useup(obj);
                    hittxt = TRUE;
                    get_dmg_bonus = FALSE;
                    break;
                default:
                    /* non-weapons can damage because of their weight */
                    /* (but not too much) */
                    tmp = obj->owt / 100;
                    if (tmp < 1)
                        tmp = 1;
                    else
                        tmp = rnd(tmp);
                    if (tmp > 6)
                        tmp = 6;
                    /*
                     * Things like silver wands can arrive here so
                     * so we need another silver check.
                     */
                    if (objects[obj->otyp].oc_material == SILVER &&
                        hates_silver(mdat)) {
                        tmp += rnd(20);
                        silvermsg = TRUE;
                        silverobj = TRUE;
                    }
                }
            }
        }
    }

        /****** NOTE: perhaps obj is undefined!! (if !thrown && BOOMERANG)
         *      *OR* if attacking bare-handed!! */

    if (get_dmg_bonus && tmp > 0) {
        tmp += dambon(&youmonst);
        /* If you throw using a propellor, you don't get a strength bonus but
           you do get an increase-damage bonus. */
        if (!thrown || !obj || !uwep || !ammo_and_launcher(obj, uwep))
            tmp += dbon();
    }

    if (valid_weapon_attack) {
        struct obj *wep;

        /* to be valid a projectile must have had the correct projector */
        wep = PROJECTILE(obj) ? uwep : obj;
        tmp += weapon_dam_bonus(wep);
        /* [this assumes that `!thrown' implies wielded...] */
        wtype = thrown ? weapon_type(wep) : uwep_skill_type();
        use_skill(wtype, 1);
        if (wep && !wep->known && P_SKILL(wtype) >= P_BASIC &&
            (P_SKILL(wtype) >= P_EXPERT ||
             !rn2(P_SKILL(wtype) == P_SKILLED ? 10 : 100))) {
            pline(msgc_info,
                  "You have successfully appraised this weapon's enchantment.");
            wep->known = TRUE;
        }
    }

    if (ispoisoned) {
        int nopoison = (10 - (obj->owt / 10));

        if (nopoison < 2)
            nopoison = 2;
        if (Role_if(PM_SAMURAI)) {
            pline(msgc_alignchaos,
                  "You dishonorably use a poisoned weapon!");
            adjalign(-sgn(u.ualign.type));
        } else if ((u.ualign.type == A_LAWFUL) && (u.ualign.record > -10)) {
            pline(msgc_alignbad,
                  "You feel like an evil coward for using a poisoned weapon.");
            adjalign(-1);
        }
        if (obj && !rn2(nopoison)) {
            obj->opoisoned = FALSE;
            pline(msgc_itemloss, "Your %s %s no longer poisoned.",
                  xname(obj), otense(obj, "are"));
        }
    }
    if (tmp < 1) {
        /* make sure that negative damage adjustment can't result in
           inadvertently boosting the victim's hit points */
        tmp = 0;
        if (mdat == &mons[PM_SHADE]) {
            if (!hittxt) {
                const char *what =
                    *unconventional ? unconventional : "attack";
                pline(combat_msgc(&youmonst, mon, cr_immune),
                      "Your %s %s harmlessly through %s.", what,
                      vtense(what, "pass"), mon_nam(mon));
                hittxt = TRUE;
            }
        } else {
            if (get_dmg_bonus)
                tmp = 1;
        }
    }

    if (jousting) {
        tmp += dice(2, (obj == uwep) ? 10 : 2); /* [was in dmgval()] */
        pline(msgc_combatalert, "You joust %s%s", mon_nam(mon),
              canseemon(mon) ? exclam(tmp) : ".");
        if (jousting < 0) {
            pline(msgc_itemloss, "Your %s shatters on impact!", xname(obj));
            /* (must be either primary or secondary weapon to get here) */
            u.twoweap = FALSE;  /* untwoweapon() is too verbose here */
            if (obj == uwep)
                uwepgone();     /* set bashmsg */
            /* minor side-effect: broken lance won't split puddings */
            useup(obj);
            obj = 0;
        }
        /* avoid migrating a dead monster */
        if (mon->mhp > tmp || mercy) {
            mhurtle(mon, mon->mx - u.ux, mon->my - u.uy, 1);
            mdat = mon->data;   /* in case of a polymorph trap */
            if (DEADMONSTER(mon))
                already_killed = TRUE;
        }
        hittxt = TRUE;
    } else
        /* VERY small chance of stunning opponent if unarmed. */
    if (unarmed && tmp > 1 && !thrown && !obj && !Upolyd) {
        if (rnd(100) < P_SKILL(P_BARE_HANDED_COMBAT) && !bigmonst(mdat) &&
            !thick_skinned(mdat)) {
            if (canspotmon(mon))
                pline(msgc_combatalert, "%s %s from your powerful strike!",
                      Monnam(mon), makeplural(stagger(mon->data, "stagger")));
            /* avoid migrating a dead monster */
            if (mon->mhp > tmp) {
                mhurtle(mon, mon->mx - u.ux, mon->my - u.uy, 1);
                mdat = mon->data;       /* in case of a polymorph trap */
                if (DEADMONSTER(mon))
                    already_killed = TRUE;
            }
            hittxt = TRUE;
        }
    }

    if (!already_killed) {
        if (mercy) {
            if (mon->mhp < mon->mhpmax) {
                mon->mhp += tmp;
                if (mon->mhp > mon->mhpmax)
                    mon->mhp = mon->mhpmax;
                pline(combat_msgc(&youmonst, mon, cr_immune),
                      "%s healed!", M_verbs(mon, "are"));
            }

            if (obj) {
                obj->mbknown = 1;
                obj->mknown = 1;

                /* autocurse */
                if (!obj->cursed) {
                    if (Blind)
                        pline(msgc_statusbad, "%s for a moment.",
                              Tobjnam(obj, "vibrate"));
                    else
                        pline(msgc_statusbad, "%s %s for a moment.",
                              Tobjnam(obj, "glow"), hcolor("black"));
                    curse(obj);
                    obj->bknown = TRUE;
                }
                learn_oprop(obj, opm_mercy);
            }
        } else
            mon->mhp -= tmp;
    }

    /* adjustments might have made tmp become less than what a level draining
       artifact has already done to max HP */
    if (mon->mhp > mon->mhpmax)
        mon->mhp = mon->mhpmax;
    if (mon->mhp <= 0)
        destroyed = TRUE;
    if (!mercy && mon->mtame &&
        (!mon->mflee || mon->mfleetim) && tmp > 0) {
        abuse_dog(mon);
        monflee(mon, 10 * rnd(tmp), FALSE, FALSE);
    }
    if ((mdat == &mons[PM_BLACK_PUDDING] || mdat == &mons[PM_BROWN_PUDDING])
        && obj && obj == uwep && objects[obj->otyp].oc_material == IRON &&
        mon->mhp >= 2 && !thrown && !cancelled(mon)
        /* && !destroyed -- guaranteed by mhp > 1 */ ) {
        if (clone_mon(mon, 0, 0)) {
            pline(msgc_consequence, "%s divides as you hit it!", Monnam(mon));
            hittxt = TRUE;
            break_conduct(conduct_puddingsplit);
        }
    }

    if (!hittxt &&      /* ( thrown => obj exists ) */
        (!destroyed || (thrown && m_shot.n > 1 && m_shot.o == obj->otyp))) {
        if (thrown)
            hit(mshot_xname(obj), mon, exclam(tmp), &youmonst);
        else if (!flags.verbose)
            pline(combat_msgc(&youmonst, mon, cr_hit), "You hit it.");
        else
            pline(combat_msgc(&youmonst, mon, cr_hit), "You %s %s%s",
                  Role_if(PM_BARBARIAN) ? "smite" : "hit",
                  mon_nam(mon), canseemon(mon) ? exclam(tmp) : ".");
    }

    if (silvermsg) {
        const char *fmt;
        const char *whom = mon_nam(mon);

        if (canseemon(mon)) {
            if (barehand_silver_rings == 1)
                fmt = "Your silver ring sears %s!";
            else if (barehand_silver_rings == 2)
                fmt = "Your silver rings sear %s!";
            else if (silverobj && *saved_oname) {
                fmt = msgprintf("Your %s%s %s %%s!",
                                strstri(saved_oname, "silver") ? "" : "silver ",
                                saved_oname, vtense(saved_oname, "sear"));
            } else
                fmt = "The silver sears %s!";
        } else {
            whom = msgupcasefirst(whom);       /* "it" -> "It" */
            fmt = "%s is seared!";
        }
        if (!noncorporeal(mdat))
            whom = msgcat(s_suffix(whom), " flesh");
        pline(combat_msgc(&youmonst, mon, cr_hit), fmt, whom);
    }

    if (ispoisoned && !already_killed) {
        poisoned(mon, poistxt, A_STR,
                 "???SHOULD NOT HAPPEN???", thrown ? -10 : 10);
        if (DEADMONSTER(mon))
            destroyed = TRUE;
    } else if (destroyed) {
        if (!already_killed)
            killed(mon);        /* takes care of most messages */
    } else if (u.umconf && !thrown) {
        nohandglow(mon);
        if (!resist(&youmonst, mon, SPBOOK_CLASS, NOTELL, 0)) {
            set_property(mon, CONFUSION, dice(3, 8), TRUE);
            if (!stunned(mon) && mon->mcanmove && !mon->msleeping &&
                canseemon(mon))
                pline(combat_msgc(&youmonst, mon, cr_hit),
                      "%s appears confused.", Monnam(mon));
        }
    }

    return (boolean) (destroyed ? FALSE : TRUE);
}

static boolean
shade_aware(struct obj *obj)
{
    if (!obj)
        return FALSE;
    /*
     * The things in this list either
     * 1) affect shades.
     *  OR
     * 2) are dealt with properly by other routines
     *    when it comes to shades.
     */
    if (obj->otyp == BOULDER || obj->otyp == HEAVY_IRON_BALL ||
        obj->otyp == IRON_CHAIN /* dmgval handles those first three */
        || obj->otyp == MIRROR  /* silver in the reflective surface */
        || obj->otyp == CLOVE_OF_GARLIC /* causes shades to flee */
        || objects[obj->otyp].oc_material == SILVER)
        return TRUE;
    return FALSE;
}

/* check whether slippery clothing protects from hug or wrap attack */
/* [currently assumes that you are the attacker] */
static boolean
m_slips_free(struct monst *mdef, const struct attack *mattk)
{
    struct obj *obj;

    if (mattk->adtyp == AD_DRIN) {
        /* intelligence drain attacks the head */
        obj = which_armor(mdef, os_armh);
    } else {
        /* grabbing attacks the body */
        obj = which_armor(mdef, os_armc);        /* cloak */
        if (!obj)
            obj = which_armor(mdef, os_arm);     /* suit */
        if (!obj)
            obj = which_armor(mdef, os_armu);    /* shirt */
    }

    uint64_t props = 0;
    if (obj)
        props = obj_properties(obj);

    /* if your cloak/armor is greased, monster slips off; this protection might
       fail (33% chance) when the armor is cursed */
    if (obj && (obj->greased || obj->otyp == OILSKIN_CLOAK ||
                (props & opm_oilskin)) &&
        (!obj->cursed || rn2(3))) {
        pline(combat_msgc(&youmonst, mdef, cr_miss), "You %s %s %s %s!",
              mattk->adtyp == AD_WRAP ?
              "slip off of" : "grab, but cannot hold onto",
              s_suffix(mon_nam(mdef)), obj->greased ? "greased" : "slippery",
              /* avoid "slippery slippery cloak" for undiscovered oilskin */
              (obj->greased || objects[obj->otyp].oc_name_known ||
               obj->otyp != OILSKIN_CLOAK) ?
              xname(obj) : cloak_simple_name(obj));

        if (obj->greased && !rn2(2)) {
            pline(msgc_consequence, "The grease wears off.");
            obj->greased = 0;
        }
        return TRUE;
    }
    return FALSE;
}

/* used when hitting a monster with a lance while mounted */
/* 1: joust hit; 0: ordinary hit; -1: joust but break lance */
static int
joust(struct monst *mon,        /* target */
      struct obj *obj)
{       /* weapon */
    int skill_rating, joust_dieroll;

    if (Fumbling || Stunned)
        return 0;
    /* sanity check; lance must be wielded in order to joust */
    if (obj != uwep && (obj != uswapwep || !u.twoweap))
        return 0;

    /* if using two weapons, use worse of lance and two-weapon skills */
    skill_rating = P_SKILL(weapon_type(obj));   /* lance skill */
    if (u.twoweap && P_SKILL(P_TWO_WEAPON_COMBAT) < skill_rating)
        skill_rating = P_SKILL(P_TWO_WEAPON_COMBAT);
    if (skill_rating == P_ISRESTRICTED)
        skill_rating = P_UNSKILLED;     /* 0=>1 */

    /* odds to joust are expert:80%, skilled:60%, basic:40%, unskilled:20% */
    if ((joust_dieroll = rn2(5)) < skill_rating) {
        if (joust_dieroll == 0 && rnl(50) == (50 - 1) && !unsolid(mon->data) &&
            !obj_resists(obj, 0, 100))
            return -1;  /* hit that breaks lance */
        return 1;       /* successful joust */
    }
    return 0;   /* no joust bonus; revert to ordinary attack */
}

/*
 * Send in a demon pet for the hero.  Exercise wisdom.
 *
 * This function used to be inline to damageum(), but the Metrowerks compiler
 * (DR4 and DR4.5) screws up with an internal error 5 "Expression Too Complex."
 * Pulling it out makes it work.
 */
void
demonpet(void)
{
    int i;
    const struct permonst *pm;
    struct monst *dtmp;

    pline(msgc_youdiscover, "Some hell-p has arrived!");
    i = (!rn2(6) || !is_demon(youmonst.data))
        ? ndemon(&u.uz, u.ualign.type) : NON_PM;
    pm = i != NON_PM ? &mons[i] : youmonst.data;
    if ((dtmp = makemon(pm, level, u.ux, u.uy,
                        MM_CREATEMONSTER | MM_CMONSTER_T)) != 0)
        tamedog(dtmp, NULL);
    exercise(A_WIS, TRUE);
}

/*
 * Player uses theft attack against monster.
 *
 * If the target is wearing body armor, take all of its possesions;
 * otherwise, take one object.  [Is this really the behavior we want?]
 *
 * This routine implicitly assumes that there is no way to be able to
 * resist petrification (ie, be polymorphed into a xorn or golem) at the
 * same time as being able to steal (poly'd into nymph or succubus).
 * If that ever changes, the check for touching a cockatrice corpse
 * will need to be smarter about whether to break out of the theft loop.
 */
static void
steal_it(struct monst *mdef, const struct attack *mattk)
{
    struct obj *otmp, *stealoid, **minvent_ptr;
    long unwornmask;

    if (!mdef->minvent)
        return; /* nothing to take */

    /* look for worn body armor */
    stealoid = NULL;
    if (could_seduce(&youmonst, mdef, mattk)) {
        /* find armor, and move it to end of inventory in the process */
        minvent_ptr = &mdef->minvent;
        while ((otmp = *minvent_ptr) != 0)
            if (otmp->owornmask & W_MASK(os_arm)) {
                if (stealoid)
                    panic("steal_it: multiple worn suits");
                *minvent_ptr = otmp->nobj;      /* take armor out of minvent */
                stealoid = otmp;
                stealoid->nobj = NULL;
            } else {
                minvent_ptr = &otmp->nobj;
            }
        *minvent_ptr = stealoid;        /* put armor back into minvent */
    }

    if (stealoid) {     /* we will be taking everything */
        if (gender(mdef) == (int)u.mfemale && youmonst.data->mlet == S_NYMPH)
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "You charm %s.  She gladly hands over her possessions.",
                  mon_nam(mdef));
        else
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "You seduce %s and %s starts to take off %s clothes.",
                  mon_nam(mdef), mhe(mdef), mhis(mdef));
    }

    while ((otmp = mdef->minvent) != 0) {
        if (!Upolyd)
            break;      /* no longer have ability to steal */
        /* take the object away from the monster */
        obj_extract_self(otmp);
        if ((unwornmask = otmp->owornmask) != 0L) {
            mdef->misc_worn_check &= ~unwornmask;
            if (otmp->owornmask & W_MASK(os_wep)) {
                setmnotwielded(mdef, otmp);
                MON_NOWEP(mdef);
            }
            otmp->owornmask = 0L;
            update_property(mdef, objects[otmp->otyp].oc_oprop, which_slot(otmp));
            update_property_for_oprops(mdef, otmp,
                                       which_slot(otmp));

            if (otmp == stealoid)       /* special message for final item */
                pline(msgc_actionok, "%s finishes taking off %s suit.",
                      Monnam(mdef), mhis(mdef));
        }
        /* give the object to the character */
        otmp =
            hold_another_object(otmp, "You snatched but dropped %s.",
                                doname(otmp), "You steal: ");
        if (otmp->where != OBJ_INVENT)
            continue;
        if (otmp->otyp == CORPSE && touch_petrifies(&mons[otmp->corpsenm]) &&
            !uarmg) {
            instapetrify(killer_msg(STONING,
                msgprintf("stealing %s corpse",
                          an(mons[otmp->corpsenm].mname))));
            break;      /* stop the theft even if hero survives */
        }
        /* more take-away handling, after theft message */
        if (unwornmask & W_MASK(os_wep)) {         /* stole wielded weapon */
            possibly_unwield(mdef, FALSE);
        } else if (unwornmask & W_MASK(os_armg)) {    /* stole worn gloves */
            mselftouch(mdef, NULL, &youmonst);
            if (DEADMONSTER(mdef)) /* it's now a statue */
                return; /* can't continue stealing */
        }

        if (!stealoid)
            break;      /* only taking one item */
    }
}

int
damageum(struct monst *mdef, const struct attack *mattk)
{
    const struct permonst *pd = mdef->data;
    int tmp = dice((int)mattk->damn, (int)mattk->damd);
    int armpro;
    int zombie_timer;
    int ptmp;
    boolean negated;

    armpro = magic_negation(mdef);
    /* since hero can't be cancelled, only defender's armor applies */
    negated = !(!cancelled(&youmonst) && (rn2(3) >= armpro || !rn2(50)));

    if (is_demon(youmonst.data) && !rn2(13) && !uwep && u.umonnum != PM_SUCCUBUS
        && u.umonnum != PM_INCUBUS && u.umonnum != PM_BALROG) {
        demonpet();
        return 0;
    }
    switch (mattk->adtyp) {
    case AD_STUN:
        if (!Blind)
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s %s for a moment.", Monnam(mdef),
                  makeplural(stagger(mdef->data, "stagger")));
        set_property(mdef, STUNNED, tmp, TRUE);
        goto physical;
    case AD_LEGS:
        /* if (u.ucancelled) { */
        /* tmp = 0; */
        /* break; */
        /* } */
        goto physical;
    case AD_WERE:      /* no special effect on monsters */
    case AD_HEAL:      /* likewise */
    case AD_PHYS:
    physical:
        if (mattk->aatyp == AT_WEAP) {
            if (uwep)
                tmp = 0;
        } else if (mattk->aatyp == AT_KICK) {
            if (thick_skinned(mdef->data))
                tmp = 0;
            if (mdef->data == &mons[PM_SHADE]) {
                if (!(uarmf && uarmf->blessed)) {
                    impossible("bad shade attack function flow?");
                    tmp = 0;
                } else
                    tmp = rnd(4);       /* bless damage */
            }
        }
        break;
    case AD_MAGM:
        if (cancelled(&youmonst)) {
            tmp = 0;
            break;
        }
        if (!Blind)
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s is hit by a shower of missiles!", Monnam(mdef));
        if (resists_magm(mdef)) {
            shieldeff(mdef->mx, mdef->my);
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_immune),
                      "The missiles bounce off!");
            tmp = 0;
        }
        break;
    case AD_FIRE:
        if (negated) {
            tmp = 0;
            break;
        }
        if (!Blind && !resists_fire(mdef))
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s is %s!", Monnam(mdef), on_fire(mdef->data, mattk));
        if (pd == &mons[PM_STRAW_GOLEM] || pd == &mons[PM_PAPER_GOLEM]) {
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_kill),
                      "%s burns completely!", Monnam(mdef));
            xkilled(mdef, 2);
            tmp = 0;
            break;
            /* Don't return yet; keep hp<1 and tmp=0 for pet msg */
        }
        burn_away_slime(mdef);
        tmp += destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
        tmp += destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
        if (resists_fire(mdef)) {
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_immune),
                      "%s is on fire, but doesn't heat up!", Monnam(mdef));
            golemeffects(mdef, AD_FIRE, tmp);
            shieldeff(mdef->mx, mdef->my);
            tmp = 0;
        }
        /* only potions damage resistant players in destroy_item */
        tmp += destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
        break;
    case AD_COLD:
        if (negated) {
            tmp = 0;
            break;
        }
        if (resists_cold(mdef)) {
            shieldeff(mdef->mx, mdef->my);
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_immune),
                      "%s is covered in frost, but not chilly!",
                      Monnam(mdef));
            golemeffects(mdef, AD_COLD, tmp);
            tmp = 0;
        } else if (!Blind)
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s is covered in frost!", Monnam(mdef));
        tmp += destroy_mitem(mdef, POTION_CLASS, AD_COLD);
        break;
    case AD_ELEC:
        if (negated) {
            tmp = 0;
            break;
        }
        if (!Blind && !resists_elec(mdef))
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s is zapped!", Monnam(mdef));
        tmp += destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
        if (resists_elec(mdef)) {
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_immune),
                      "%s is zapped, but not shocked!", Monnam(mdef));
            golemeffects(mdef, AD_ELEC, tmp);
            shieldeff(mdef->mx, mdef->my);
            tmp = 0;
        }
        break;
    case AD_ACID:
        if (resists_acid(mdef))
            tmp = 0;
        break;
    case AD_STON:
        if (cancelled(&youmonst))
            pline(msgc_failcurse, "You cough!");
        else
            mstiffen(mdef, &youmonst);
        break;
    case AD_SSEX:
    case AD_SEDU:
    case AD_SITM:
        if (!cancelled(&youmonst))
            steal_it(mdef, mattk);
        tmp = 0;
        break;
    case AD_SGLD:
        /* This you as a leprechaun, so steal real gold only, no lesser coins */
        if (!cancelled(&youmonst)) {
            struct obj *mongold = findgold(mdef->minvent);

            if (mongold) {
                obj_extract_self(mongold);
                if (can_hold(mongold)) {
                    pickinv(mongold);
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "Your purse feels heavier.");
                } else {
                    pline(msgc_substitute, "You grab %s's gold, but find no "
                          "room in your knapsack.", mon_nam(mdef));
                    dropy(mongold);
                }
            }
            exercise(A_DEX, TRUE);
        }
        tmp = 0;
        break;
    case AD_TLPT:
        if (tmp <= 0)
            tmp = 1;
        if (!negated && tmp < mdef->mhp) {
            const char *nambuf;
            boolean u_saw_mon = canseemon(mdef) ||
                (Engulfed && u.ustuck == mdef);
            /* record the name before losing sight of monster */
            nambuf = Monnam(mdef);
            if (u_teleport_mon(mdef, FALSE) && u_saw_mon) {
                boolean can_see_mon = canseemon(mdef) ||
                    (Engulfed && u.ustuck == mdef);
                if (!can_see_mon)
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "%s suddenly disappears!", nambuf);
            }
        }
        break;
    case AD_BLND:
        if (can_blnd(&youmonst, mdef, mattk->aatyp, NULL)) {
            if (!blind(&youmonst) && !blind(mdef))
                pline(combat_msgc(&youmonst, mdef, cr_hit),
                      "%s is blinded.", Monnam(mdef));
            inc_timeout(mdef, BLINDED, tmp, TRUE); /* avoid messages */
        }
        tmp = 0;
        break;
    case AD_CURS:
        if (night() && !rn2(10) && !cancelled(&youmonst)) {
            if (mdef->data == &mons[PM_CLAY_GOLEM]) {
                if (!blind(&youmonst))
                    pline(combat_msgc(&youmonst, mdef, cr_kill),
                          "Some writing vanishes from %s head!",
                          s_suffix(mon_nam(mdef)));
                xkilled(mdef, 0);
                /* Don't return yet; keep hp<1 and tmp=0 for pet msg */
            } else {
                pline(combat_msgc(&youmonst, mdef, cr_hit),
                      "You chuckle.");
                gremlin_curse(mdef);
            }
        }
        tmp = 0;
        break;
    case AD_DRLI:
        if (!cancelled(&youmonst) && !rn2(3) && !resists_drli(mdef))
            mlosexp(&youmonst, mdef, "", FALSE);
        if (DEADMONSTER(mdef))
            tmp = 0;
        break;
    case AD_RUST:
        if (pd == &mons[PM_IRON_GOLEM]) {
            pline(combat_msgc(&youmonst, mdef, cr_kill),
                  "%s falls to pieces!", Monnam(mdef));
            xkilled(mdef, 0);
        }
        hurtarmor(mdef, ERODE_RUST);
        tmp = 0;
        break;
    case AD_CORR:
        hurtarmor(mdef, ERODE_CORRODE);
        tmp = 0;
        break;
    case AD_DCAY:
        if (pd == &mons[PM_WOOD_GOLEM] || pd == &mons[PM_LEATHER_GOLEM]) {
            pline(combat_msgc(&youmonst, mdef, cr_kill),
                  "%s falls to pieces!", Monnam(mdef));
            xkilled(mdef, 0);
        }
        hurtarmor(mdef, ERODE_ROT);
        tmp = 0;
        break;
    case AD_DRST:
        ptmp = A_STR;
        goto dopois;
    case AD_DRDX:
        ptmp = A_DEX;
        goto dopois;
    case AD_DRCO:
        ptmp = A_CON;
    dopois:
        if (!negated && !rn2(8))
            poisoned(mdef, msgprintf("Your %s", mpoisons_subj(&youmonst, mattk)), ptmp,
                     killer_msg_mon(POISONING, &youmonst), 30);
        break;
    case AD_DRIN:
        if (notonhead || !has_head(mdef->data)) {
            pline(combat_msgc(&youmonst, mdef, cr_immune),
                  "%s doesn't seem harmed.", Monnam(mdef));
            tmp = 0;
            if (!Unchanging && !unsolid(youmonst.data) &&
                mdef->data == &mons[PM_GREEN_SLIME]) {
                if (!sliming(&youmonst) && level->locations[u.ux][u.uy].typ != LAVAPOOL) {
                    pline(msgc_fatal,
                          "You suck in some slime and don't feel very well.");
                    set_property(&youmonst, SLIMED, 10, TRUE);
                    set_delayed_killer(TURNED_SLIME,
                                       killer_msg(TURNED_SLIME, "sucking in some slime"));
                }
            }
            break;
        }
        if (m_slips_free(mdef, mattk))
            break;

        if ((mdef->misc_worn_check & W_MASK(os_armh)) && rn2(8)) {
            pline(combat_msgc(&youmonst, mdef, cr_resist),
                  "%s %s blocks your attack to %s head.",
                  s_suffix(Monnam(mdef)),
                  helmet_name(which_armor(mdef, os_armh)), mhis(mdef));
            break;
        }

        if (!mindless(mdef->data))
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "You eat %s brain!", s_suffix(mon_nam(mdef)));
        break_conduct(conduct_food);
        if (touch_petrifies(mdef->data) && !resists_ston(&youmonst) &&
            !petrifying(&youmonst)) {
            set_property(&youmonst, STONED, 5, TRUE);
            set_delayed_killer(STONING,
                               killer_msg(STONING,
                                          msgcat("eating the brain of ",
                                                 k_monnam(mdef))));
        }
        if (!vegan(mdef->data))
            break_conduct(conduct_vegan);
        if (!vegetarian(mdef->data))
            break_conduct(conduct_vegetarian);
        if (mindless(mdef->data)) {
            pline(combat_msgc(&youmonst, mdef, cr_immune),
                  "You eat %s brain, but it doesn't notice.",
                  s_suffix(mon_nam(mdef)));
            break;
        }
        tmp += rnd(10);
        morehungry(-rnd(30));   /* cannot choke */
        if (ABASE(A_INT) < AMAX(A_INT)) {
            ABASE(A_INT) += rnd(4);
            if (ABASE(A_INT) > AMAX(A_INT))
                ABASE(A_INT) = AMAX(A_INT);
        }
        exercise(A_WIS, TRUE);
        break;
    case AD_STCK:
        if (!negated && !sticks(mdef->data))
            u.ustuck = mdef;    /* it's now stuck to you */
        break;
    case AD_WRAP:
        if (!sticks(mdef->data) && !cancelled(&youmonst)) {
            if (!u.ustuck && !rn2(10)) {
                if (m_slips_free(mdef, mattk)) {
                    tmp = 0;
                } else {
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "You swing yourself around %s!", mon_nam(mdef));
                    u.ustuck = mdef;
                }
            } else if (u.ustuck == mdef) {
                if (is_pool(level, u.ux, u.uy) && !swims(mdef) &&
                    !unbreathing(mdef)) {
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "You drown %s...", mon_nam(mdef));
                    tmp = mdef->mhp;
                } else if (mattk->aatyp == AT_HUGS)
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "%s is being crushed.", Monnam(mdef));
            } else {
                tmp = 0;
                pline(combat_msgc(&youmonst, mdef, cr_miss),
                      "You brush against %s %s.", s_suffix(mon_nam(mdef)),
                      mbodypart(mdef, LEG));
            }
        } else
            tmp = 0;
        break;
    case AD_PLYS:
        if (!negated && mdef->mcanmove && !rn2(3) && tmp < mdef->mhp) {
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_hit),
                      "%s is frozen by you!", Monnam(mdef));
            mdef->mcanmove = 0;
            mdef->mfrozen = rnd(10);
        }
        break;
    case AD_SLEE:
        if (!negated && !mdef->msleeping && sleep_monst(&youmonst, mdef, rnd(10), -1)) {
            if (!Blind)
                pline(combat_msgc(&youmonst, mdef, cr_hit),
                      "%s is put to sleep by you!", Monnam(mdef));
            slept_monst(mdef);
        }
        break;
    case AD_SLIM:
        if (negated)
            break;      /* physical damage only */
        if (!rn2(4) && !flaming(mdef->data) && !unsolid(mdef->data) &&
            mdef->data != &mons[PM_GREEN_SLIME] && !unchanging(mdef)) {
            set_property(mdef, SLIMED, 10, FALSE);
            tmp = 0;
        }
        break;
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        if (negated)
            break;
        struct obj *obj = some_armor(mdef);

        if (drain_item(obj)) {
            pline(msgc_itemloss, "%s %s less effective.",
                  s_suffix(Monnam(mdef)), aobjnam(obj, "seem"));
        }
        tmp = 0;
        break;
    case AD_ZOMB:
        if (nonliving(mdef->data) || izombie(mdef))
            break;
        zombie_timer = property_timeout(mdef, ZOMBIE);
        if (!zombie_timer)
            mdef->uzombied = TRUE;
        set_property(mdef, ZOMBIE,
                     !zombie_timer ? 100 :
                     zombie_timer <= 10 ? 1 :
                     (zombie_timer - 10), FALSE);
        break;
    case AD_SLOW:
        if (!negated)
            inc_timeout(mdef, SLOW, tmp, FALSE);
        break;
    case AD_CONF:
        if (!cancelled(&youmonst))
            inc_timeout(mdef, CONFUSION, tmp, FALSE);
        break;
    default:
        tmp = 0;
        break;
    }

    if (DEADMONSTER(mdef)) {
        /* Can happen from poison which runs its own logic */
        return 2;
    }

    /* in case player is fast */
    if (mdef->mstrategy == st_waiting)
        mdef->mstrategy = st_none;
    if ((mdef->mhp -= tmp) <= 0) {
        if (mdef->mtame && !cansee(mdef->mx, mdef->my)) {
            /* not combat_msgc; that will avoid spoiling the fact that it's your
               pet you're attacking, but we want to spoil that fact now */
            pline(msgc_petfatal, "You feel embarrassed for a moment.");
            if (tmp)
                xkilled(mdef, 0);       /* !tmp but hp<1: already killed */
        } else if (!flags.verbose) {
            pline(combat_msgc(&youmonst, mdef, cr_kill), "You destroy it!");
            if (tmp)
                xkilled(mdef, 0);
        } else if (tmp)
            killed(mdef);
        return 2;
    }
    return 1;
}

static int
explum(struct monst *mdef, const struct attack *mattk)
{
    int tmp = dice((int)mattk->damn, (int)mattk->damd);
    boolean resistance;     /* only for cold/fire/elec */

    pline(msgc_statusend, "You explode!");
    switch (mattk->adtyp) {
    case AD_BLND:
        if (!resists_blnd(mdef)) {
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s is blinded by your flash of light!", Monnam(mdef));
            set_property(mdef, BLINDED, tmp, TRUE);
        }
        break;
    case AD_HALU:
        if (haseyes(mdef->data) && !blind(mdef) && !resists_hallu(mdef)) {
            pline(combat_msgc(&youmonst, mdef, cr_hit),
                  "%s is affected by your flash of light!", Monnam(mdef));
            set_property(mdef, CONFUSION, tmp, TRUE);
        }
        break;
    case AD_COLD:
        resistance = resists_cold(mdef);
        goto common;
    case AD_FIRE:
        resistance = resists_fire(mdef);
        goto common;
    case AD_ELEC:
        resistance = resists_elec(mdef);
    common:
        if (!resistance) {
            if (acurr(mdef, A_DEX) > rnd(20)) {
                pline(combat_msgc(&youmonst, mdef, cr_resist),
                      "%s ducks some of the blast.", Monnam(mdef));
                tmp = tmp / 2 + 1;
            } else
                pline(combat_msgc(&youmonst, mdef, cr_hit),
                      "%s gets blasted!", Monnam(mdef));
            mdef->mhp -= tmp;
            if (mdef->mhp <= 0) {
                killed(mdef);
                return 2;
            }
        } else {
            shieldeff(mdef->mx, mdef->my);
            if (is_golem(mdef->data))
                golemeffects(mdef, (int)mattk->adtyp, tmp);
            else
                pline(combat_msgc(&youmonst, mdef, cr_immune),
                      "The blast doesn't seem to affect %s.", mon_nam(mdef));
        }
        break;
    default:
        break;
    }
    return 1;
}

static void
start_engulf(struct monst *mdef)
{
    int x, y;

    if (!Invisible) {
        x = mdef->mx;
        y = mdef->my;
        map_location(u.ux, u.uy, TRUE, 0);

        dbuf_set(x, y, level->locations[x][y].mem_bg,
                 level->locations[x][y].mem_trap,
                 level->locations[x][y].mem_obj,
                 level->locations[x][y].mem_obj_mn, 0,
                 dbuf_monid((&youmonst), x, y, rn2),
                 0, 0, dbuf_branding(level, x, y));
    }
    pline(combat_msgc(&youmonst, mdef, cr_hit),
          "You engulf %s!", mon_nam(mdef));
    win_delay_output();
    win_delay_output();
}

static void
end_engulf(struct monst *mdef)
{
    if (!Invisible) {
        newsym(mdef->mx, mdef->my);
        newsym(u.ux, u.uy);
    }
}

static int
gulpum(struct monst *mdef, const struct attack *mattk)
{
    int tmp;
    int dam = dice((int)mattk->damn, (int)mattk->damd);
    struct obj *otmp;

    /* Not totally the same as for real monsters.  Specifically, these don't
       take multiple moves.  (It's just too hard, for too little result, to
       program monsters which attack from inside you, which would be necessary
       if done accurately.) Instead, we arbitrarily kill the monster
       immediately for AD_DGST and we regurgitate them after exactly 1 round of
       attack otherwise.  -KAA */

    if (mdef->data->msize >= MZ_HUGE)
        return 0;

    if (u.uhunger < 1500 && !Engulfed) {
        for (otmp = mdef->minvent; otmp; otmp = otmp->nobj)
            snuff_lit(otmp);

        /* KMH, conduct */
        if (mattk->adtyp == AD_DGST) {
            break_conduct(conduct_food);
            if (!vegan(mdef->data))
                break_conduct(conduct_vegan);
            if (!vegetarian(mdef->data))
                break_conduct(conduct_vegetarian);
        }

        if (!touch_petrifies(mdef->data) || Stone_resistance) {
            start_engulf(mdef);
            switch (mattk->adtyp) {
            case AD_DGST:
                /* eating a Rider or its corpse is fatal */
                if (is_rider(mdef->data)) {
                    pline(msgc_fatal_predone,
                          "Unfortunately, digesting any of it is fatal.");
                    end_engulf(mdef);
                    done(DIED, msgcat("unwisely tried to eat ", mdef->data->mname));
                    return 0;   /* lifesaved */
                }

                if (Slow_digestion) {
                    dam = 0;
                    break;
                }

                /* Use up amulet of life saving */
                if (((otmp = mlifesaver(mdef))) != NULL)
                    m_useup(mdef, otmp);

                newuhs(FALSE);
                xkilled(mdef, 2);
                if (!DEADMONSTER(mdef)) {    /* monster lifesaved */
                    pline(msgc_substitute,
                          "You hurriedly regurgitate the sizzling in your %s.",
                          body_part(STOMACH));
                } else {
                    tmp = 1 + (mdef->data->cwt >> 8);
                    if (corpse_chance(mdef, &youmonst, TRUE) &&
                        !(mvitals[monsndx(mdef->data)].mvflags & G_NOCORPSE)) {
                        /* nutrition only if there can be a corpse */
                        u.uhunger += (mdef->data->cnutrit + 1) / 2;
                    } else
                        tmp = 0;
                    const char *msgbuf =
                        msgprintf("You totally digest %s.", mon_nam(mdef));
                    if (tmp != 0) {
                        pline(msgc_statusbad,
                              "You pause to digest %s.", mon_nam(mdef));
                        if (Slow_digestion)
                            tmp *= 2;
                        helpless(tmp, hr_busy, "digesting something", msgbuf);
                    } else
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "%s", msgbuf);
                    if (mdef->data == &mons[PM_GREEN_SLIME]) {
                        pline(msgc_fatal, "%s isn't sitting well with you.",
                              The(mdef->data->mname));
                        if (!Unchanging && !unsolid(youmonst.data) &&
                            level->locations[u.ux][u.uy].typ != LAVAPOOL) {
                            set_property(&youmonst, SLIMED, 5, TRUE);
                            set_delayed_killer(TURNED_SLIME,
                                               killer_msg(TURNED_SLIME, "digested green slime"));
                        }
                    } else
                        exercise(A_CON, TRUE);
                }
                end_engulf(mdef);
                return 2;
            case AD_PHYS:
                if (youmonst.data == &mons[PM_FOG_CLOUD]) {
                    if (unbreathing(mdef) && !flaming(mdef->data)) {
                        dam = 0;
                        pline(combat_msgc(&youmonst, mdef, cr_immune),
                              "%s doesn't seem harmed by your moisture.",
                              Monnam(mdef));
                    } else {
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "%s is laden with your moisture.", Monnam(mdef));
                    }
                } else
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "%s is pummeled with your debris!", Monnam(mdef));
                break;
            case AD_ACID:
                if (!cancelled(&youmonst)) {
                    if (resists_acid(mdef)) {
                        pline(combat_msgc(&youmonst, mdef, cr_immune),
                              "Your goo covers %s, but doesn't harm %s.",
                              mon_nam(mdef), mhe(mdef));
                        dam = 0;
                    } else
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "%s is covered with your goo!",
                              Monnam(mdef));
                } else
                    dam = 0;
                break;
            case AD_BLND:
                if (can_blnd(&youmonst, mdef, mattk->aatyp, NULL)) {
                    if (!blind(mdef))
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "%s can't see in there!", Monnam(mdef));
                    inc_timeout(mdef, BLINDED, dam, TRUE);
                }
                dam = 0;
                break;
            case AD_ELEC:
                if (!cancelled(&youmonst) && rn2(2)) {
                    if (resists_elec(mdef)) {
                        pline(combat_msgc(&youmonst, mdef, cr_immune),
                              "Your crackle of electricity fails to hurt %s.",
                              mon_nam(mdef));
                        dam = 0;
                    } else
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "The air around %s crackles with electricity.",
                              mon_nam(mdef));
                    golemeffects(mdef, (int)mattk->adtyp, dam);
                } else
                    dam = 0;
                break;
            case AD_COLD:
                if (!cancelled(&youmonst) && rn2(2)) {
                    if (resists_cold(mdef)) {
                        pline(combat_msgc(&youmonst, mdef, cr_immune),
                              "%s seems mildly chilly.", Monnam(mdef));
                        dam = 0;
                    } else
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "%s is freezing to death!", Monnam(mdef));
                    golemeffects(mdef, (int)mattk->adtyp, dam);
                } else
                    dam = 0;
                break;
            case AD_FIRE:
                if (!cancelled(&youmonst) && rn2(2)) {
                    if (resists_fire(mdef)) {
                        pline(combat_msgc(&youmonst, mdef, cr_immune),
                              "%s seems mildly hot.", Monnam(mdef));
                        dam = 0;
                    } else
                        pline(combat_msgc(&youmonst, mdef, cr_hit),
                              "%s is burning to a crisp!", Monnam(mdef));
                    golemeffects(mdef, (int)mattk->adtyp, dam);
                    burn_away_slime(mdef);
                } else
                    dam = 0;
                break;
            }
            end_engulf(mdef);
            if ((mdef->mhp -= dam) <= 0) {
                killed(mdef);
                if (DEADMONSTER(mdef))     /* not lifesaved */
                    return 2;
            }
            pline(msgc_yafm, "You %s %s!", is_animal(youmonst.data) ?
                  "regurgitate" : "expel", mon_nam(mdef));
            if (Slow_digestion || is_animal(youmonst.data)) {
                pline_implied(msgc_yafm, "Obviously, you didn't like %s taste.",
                              s_suffix(mon_nam(mdef)));
            }
        } else {
            pline(msgc_fatal, "You bite into %s.", mon_nam(mdef));
            instapetrify(killer_msg(STONING,
                msgprintf("swallowing %s whole", an(mdef->data->mname))));
        }
    }
    return 0;
}

void
missum(struct monst *mdef, const struct attack *mattk)
{
    if (could_seduce(&youmonst, mdef, mattk))
        pline(combat_msgc(&youmonst, mdef, cr_miss),
              "You pretend to be friendly to %s.", mon_nam(mdef));
    else if (canspotmon(mdef) && flags.verbose)
        pline(combat_msgc(&youmonst, mdef, cr_miss),
              "You miss %s.", mon_nam(mdef));
    else
        pline(combat_msgc(&youmonst, mdef, cr_miss), "You miss it.");
    if (!mdef->msleeping && mdef->mcanmove)
        wakeup(mdef, FALSE);
}

/* attack monster as a monster. */
static boolean
hmonas(struct monst *mon, int tmp, schar dx, schar dy)
{
    const struct attack *mattk;
    struct attack alt_attk;
    int i, sum[NATTK], hittmp = 0;
    int nsum = 0;
    int dhit = 0;
    int dieroll;

    for (i = 0; i < NATTK; i++) {

        sum[i] = 0;
        mattk = getmattk(youmonst.data, i, sum, &alt_attk);
        switch (mattk->aatyp) {
        case AT_WEAP:
        use_weapon:
            /* Certain monsters don't use weapons when encountered as enemies,
               but players who polymorph into them have hands or claws and thus
               should be able to use weapons.  This shouldn't prohibit the use
               of most special abilities, either. */
            /* Potential problem: if the monster gets multiple weapon attacks,
               we currently allow the player to get each of these as a weapon
               attack.  Is this really desirable? */
            if (uwep && (obj_properties(uwep) & opm_nasty)) {
                pline(combat_msgc(NULL, &youmonst, cr_hit),
                      "The nasty weapon hurts you!");
                learn_oprop(uwep, opm_nasty);
                losehp(rnd(6), killer_msg(DIED, "a nasty weapon"));
            }

            hittmp = hitval(uwep, mon);
            hittmp += weapon_hit_bonus(uwep);
            tmp += hittmp;
            dhit = (tmp > (dieroll = rnd(20)) || Engulfed);
            /* KMH -- Don't accumulate to-hit bonuses */
            if (uwep)
                tmp -= hittmp;
            /* Enemy dead, before any special abilities used */
            if (!known_hitum(mon, &dhit, mattk, dx, dy, dieroll)) {
                sum[i] = 2;
                break;
            } else
                sum[i] = dhit;
            /* might be a worm that gets cut in half */
            if (m_at(level, u.ux + dx, u.uy + dy) != mon)
                return (boolean) (nsum != 0);
            /* Do not print "You hit" message, since known_hitum already did
               it. */
            if (dhit && mattk->adtyp != AD_PHYS)
                sum[i] = damageum(mon, mattk);
            break;
        case AT_CLAW:
            if (i == 0 && uwep && !cantwield(youmonst.data))
                goto use_weapon;
            /* succubi/incubi are humanoid, but their _second_ attack is
               AT_CLAW, not their first... */
            if (i == 1 && uwep &&
                (u.umonnum == PM_SUCCUBUS || u.umonnum == PM_INCUBUS))
                goto use_weapon;
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            if (i == 0 && uwep && (youmonst.data->mlet == S_LICH))
                goto use_weapon;
            if ((dhit = (tmp > rnd(20) || Engulfed))) {
                int compat;

                if (!Engulfed &&
                    (compat = could_seduce(&youmonst, mon, mattk))) {
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You %s %s %s.",
                          !blind(mon) && haseyes(mon->data) ?
                          "smile at" : "talk to", mon_nam(mon),
                          compat == 2 ? "engagingly" : "seductively");
                    /* doesn't anger it; no wakeup() */
                    sum[i] = damageum(mon, mattk);
                    break;
                }
                wakeup(mon, FALSE);
                /* maybe this check should be in damageum()? */
                if (mon->data == &mons[PM_SHADE] &&
                    !(mattk->aatyp == AT_KICK && uarmf && uarmf->blessed)) {
                    pline(combat_msgc(&youmonst, mon, cr_immune),
                          "Your attack passes harmlessly through %s.",
                          mon_nam(mon));
                    break;
                }
                if (mattk->aatyp == AT_KICK)
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You kick %s.", mon_nam(mon));
                else if (mattk->aatyp == AT_BITE)
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You bite %s.", mon_nam(mon));
                else if (mattk->aatyp == AT_STNG)
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You sting %s.", mon_nam(mon));
                else if (mattk->aatyp == AT_BUTT)
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You butt %s.", mon_nam(mon));
                else if (mattk->aatyp == AT_TUCH)
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You touch %s.", mon_nam(mon));
                else if (mattk->aatyp == AT_TENT)
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "Your tentacles suck %s.", mon_nam(mon));
                else
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You hit %s.", mon_nam(mon));
                sum[i] = damageum(mon, mattk);
            } else
                missum(mon, mattk);
            break;

        case AT_HUGS:
            /* automatic if prev two attacks succeed, or if already grabbed in
               a previous attack */
            dhit = 1;
            wakeup(mon, FALSE);
            if (mon->data == &mons[PM_SHADE])
                pline(combat_msgc(&youmonst, mon, cr_immune),
                      "Your hug passes harmlessly through %s.", mon_nam(mon));
            else if (!sticks(mon->data) && !Engulfed) {
                if (mon == u.ustuck) {
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "%s is being %s.", Monnam(mon),
                          u.umonnum == PM_ROPE_GOLEM ? "choked" : "crushed");
                    sum[i] = damageum(mon, mattk);
                } else if (i >= 2 && sum[i - 1] && sum[i - 2]) {
                    pline(combat_msgc(&youmonst, mon, cr_hit),
                          "You grab %s!", mon_nam(mon));
                    u.ustuck = mon;
                    sum[i] = damageum(mon, mattk);
                }
            }
            break;

        case AT_EXPL:  /* automatic hit if next to */
            dhit = -1;
            wakeup(mon, FALSE);
            sum[i] = explum(mon, mattk);
            break;

        case AT_ENGL:
            if ((dhit = (tmp > rnd(20 + i)))) {
                wakeup(mon, FALSE);
                if (mon->data == &mons[PM_SHADE])
                    pline(combat_msgc(&youmonst, mon, cr_immune),
                          "Your attempt to surround %s is harmless.",
                          mon_nam(mon));
                else {
                    sum[i] = gulpum(mon, mattk);
                    if (sum[i] == 2 &&
                        (mon->data->mlet == S_ZOMBIE ||
                         mon->data->mlet == S_MUMMY) && rn2(5) &&
                        !resists_sick(&youmonst)) {
                        pline(msgc_consequence,
                              "You feel %ssick.", (sick(&youmonst)) ? "very " : "");
                        mdamageu(mon, rnd(8));
                    }
                }
            } else
                missum(mon, mattk);
            break;

        case AT_NONE:
        case AT_BOOM:
            continue;
            /* Not break--avoid passive attacks from enemy */

        case AT_BREA:
        case AT_SPIT:
        case AT_GAZE:  /* all done using #monster command */
            dhit = 0;
            break;

        default:       /* Strange... */
            impossible("strange attack of yours (%d)", mattk->aatyp);
        }
        if (dhit == -1) {
            u.mh = -1;  /* dead in the current form */
            rehumanize(EXPLODED, "used a suicidal attack");
        }
        if (sum[i] == 2)
            return (boolean) passive(mon, 1, 0, mattk->aatyp);
        /* defender dead */
        else {
            passive(mon, sum[i], 1, mattk->aatyp);
            nsum |= sum[i];
        }
        if (!Upolyd)
            break;      /* No extra attacks if no longer a monster */
        if (u_helpless(hm_all))
            break;      /* If paralyzed while attacking, i.e. floating eye */
    }
    return (boolean) (nsum != 0);
}


/* Special (passive) attacks on you by monsters done here.         */
int
passive(struct monst *mon, boolean mhit, int malive, uchar aatyp)
{
    const struct permonst *ptr = mon->data;
    int i, tmp;

    for (i = 0;; i++) {
        if (i >= NATTK)
            return malive | mhit;       /* no passive attacks */
        if (ptr->mattk[i].aatyp == AT_NONE)
            break;      /* try this one */
    }
    /* Note: tmp not always used */
    if (ptr->mattk[i].damn)
        tmp = dice((int)ptr->mattk[i].damn, (int)ptr->mattk[i].damd);
    else if (ptr->mattk[i].damd)
        tmp = dice((int)mon->m_lev + 1, (int)ptr->mattk[i].damd);
    else
        tmp = 0;

    /* These affect you even if they just died */

    switch (ptr->mattk[i].adtyp) {

    case AD_ACID:
        if (mhit && rn2(2)) {
            enum msg_channel msgc =
                combat_msgc(mon, &youmonst,
                            Acid_resistance ? cr_miss : cr_hit);
            if (Blind || !flags.verbose)
                pline(msgc, "You are splashed!");
            else
                pline(msgc, "You are splashed by %s acid!",
                      s_suffix(mon_nam(mon)));

            if (!Acid_resistance)
                mdamageu(mon, tmp);
            if (!rn2(30))
                hurtarmor(&youmonst, ERODE_CORRODE);
        }
        if (mhit) {
            if (aatyp == AT_KICK) {
                if (uarmf && !rn2(6))
                    erode_obj(uarmf, xname(uarmf), ERODE_CORRODE, TRUE, TRUE);
            } else if (aatyp == AT_WEAP || aatyp == AT_CLAW || aatyp == AT_TUCH)
                passive_obj(mon, NULL, &(ptr->mattk[i]));
        }
        exercise(A_STR, FALSE);
        break;
    case AD_STON:
        if (mhit) {     /* successful attack */
            long protector = attk_protection((int)aatyp);

            if (protector == 0L ||      /* no protection */
                (protector == W_MASK(os_armg) && !uarmg && !uwep) ||
                (protector == W_MASK(os_armf) && !uarmf) ||
                (protector == W_MASK(os_armh) && !uarmh) ||
                (protector == (W_MASK(os_armc) | W_MASK(os_armg)) &&
                 (!uarmc || !uarmg))) {
                const char *killer = "attacking %s directly";
                if (protector == W_MASK(os_armg))
                    killer = "punching %s barehanded";
                else if (protector == W_MASK(os_armf))
                    killer = "kicking %s barefoot";
                else if (protector == W_MASK(os_armh))
                    killer = "headbutting %s with no helmet";
                else if (protector == (W_MASK(os_armc) | W_MASK(os_armg)))
                    killer = uarmc ? "hugging %s without gloves" :
                        "hugging %s without a cloak";
                killer = msgprintf(killer, k_monnam(mon));
                instapetrify(killer_msg(STONING, killer));
                return 2;
            }
        }
        break;
    case AD_RUST:
        if (mhit && !cancelled(mon)) {
            if (aatyp == AT_KICK) {
                if (uarmf)
                    erode_obj(uarmf, xname(uarmf), ERODE_RUST, TRUE, TRUE);
            } else if (aatyp == AT_WEAP || aatyp == AT_CLAW || aatyp == AT_TUCH)
                passive_obj(mon, NULL, &(ptr->mattk[i]));
        }
        break;
    case AD_CORR:
        if (mhit && !cancelled(mon)) {
            if (aatyp == AT_KICK) {
                if (uarmf)
                    erode_obj(uarmf, xname(uarmf), ERODE_CORRODE, TRUE, TRUE);
            } else if (aatyp == AT_WEAP || aatyp == AT_CLAW || aatyp == AT_TUCH)
                passive_obj(mon, NULL, &(ptr->mattk[i]));
        }
        break;
    case AD_MAGM:
        /* wrath of gods for attacking Oracle */
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline(msgc_playerimmune,
                  "A hail of magic missiles narrowly misses you!");
        } else {
            pline(combat_msgc(mon, &youmonst, cr_hit),
                  "You are hit by magic missiles appearing from thin air!");
            mdamageu(mon, tmp);
        }
        break;
    case AD_ENCH:      /* KMH -- remove enchantment (disenchanter) */
        if (mhit) {
            struct obj *obj = NULL;

            if (aatyp == AT_KICK) {
                obj = uarmf;
                if (!obj)
                    break;
            } else if (aatyp == AT_BITE || aatyp == AT_BUTT ||
                       (aatyp >= AT_STNG && aatyp < AT_WEAP)) {
                break;  /* no object involved */
            }
            passive_obj(mon, obj, &(ptr->mattk[i]));
        }
        break;
    default:
        break;
    }

    /* These only affect you if they still live */

    if (malive && !cancelled(mon) && rn2(3)) {

        switch (ptr->mattk[i].adtyp) {
        case AD_SLOW:
            if (blind(mon)) {
                pline(msgc_badidea, "%snot defend %sself.", M_verbs(mon, "can"),
                      mhim(mon));
                if (!rn2(500))
                    change_luck(-1);
                break;
            }
            if (cancelled(mon) ||
                !(msensem(&youmonst, mon) & MSENSE_VISION) ||
                !(msensem(mon, &youmonst) & MSENSE_VISION))
                break;
            if (slow(&youmonst))
                pline(msgc_statusbad,
                      "You feel as if you will be slow for longer.");
            else
                pline(combat_msgc(mon, &youmonst, cr_hit),
                      "You slow down under %s gaze!", s_suffix(mon_nam(mon)));
            inc_timeout(&youmonst, SLOW, tmp, TRUE);
            break;
        case AD_PLYS:
            if (free_action(&youmonst)) {
                pline(combat_msgc(mon, &youmonst, cr_immune),
                      "You momentarily stiffen.");
                break;
            }

            pline(msgc_statusbad, "You are frozen by %s!", mon_nam(mon));
            helpless(tmp, hr_paralyzed, "frozen by attacking a monster",
                     NULL);
            exercise(A_DEX, FALSE);
            break;
        case AD_COLD:  /* brown mold or blue jelly */
            if (monnear(mon, u.ux, u.uy)) {
                if (Cold_resistance) {
                    shieldeff(u.ux, u.uy);
                    pline(combat_msgc(mon, &youmonst, cr_miss),
                          "You feel a mild chill.");
                    ugolemeffects(AD_COLD, tmp);
                    break;
                }
                pline(combat_msgc(mon, &youmonst, cr_hit),
                      "You are suddenly very cold!");
                mdamageu(mon, tmp);
                /* monster gets stronger with your heat! */
                mon->mhp += tmp / 2;
                if (mon->mhpmax < mon->mhp)
                    mon->mhpmax = mon->mhp;
                /* at a certain point, the monster will reproduce! */
                if (mon->mhpmax > ((int)(mon->m_lev + 1) * 8))
                    split_mon(mon, &youmonst);
            }
            break;
        case AD_STUN:  /* specifically yellow mold */
            if (!Stunned)
                inc_timeout(&youmonst, STUNNED, tmp, FALSE);
            break;
        case AD_FIRE:
            if (monnear(mon, u.ux, u.uy)) {
                if (resists_fire(&youmonst)) {
                    shieldeff(u.ux, u.uy);
                    pline(combat_msgc(mon, &youmonst, cr_miss),
                          "You feel mildly warm.");
                    ugolemeffects(AD_FIRE, tmp);
                    break;
                }
                pline(combat_msgc(mon, &youmonst, cr_hit),
                      "You are suddenly very hot!");
                mdamageu(mon, tmp);
            }
            break;
        case AD_ELEC:
            if (resists_elec(&youmonst)) {
                shieldeff(u.ux, u.uy);
                pline(combat_msgc(mon, &youmonst, cr_miss),
                      "You feel a mild tingle.");
                ugolemeffects(AD_ELEC, tmp);
                break;
            }
            pline(combat_msgc(mon, &youmonst, cr_hit),
                  "You are jolted with electricity!");
            mdamageu(mon, tmp);
            break;
        default:
            break;
        }
    }
    return malive | mhit;
}

/*
 * Special (passive) attacks on an attacking object by monsters done here.
 * Assumes the attack was successful.
 */
void
passive_obj(struct monst *mon, /* could be youmonst */
            struct obj *obj, /* null means pick uwep, uswapwep or uarmg */
            const struct attack *mattk)
{       /* null means we find one internally */
    const struct permonst *ptr = mon->data;
    int i;

    /* if caller hasn't specified an object, use uwep, uswapwep or uarmg */
    if (!obj) {
        obj = (u.twoweap && uswapwep && !rn2(2)) ? uswapwep : uwep;
        if (!obj && mattk->adtyp == AD_ENCH)
            obj = uarmg;        /* no weapon? then must be gloves */
        if (!obj)
            return;     /* no object to affect */
    }

    /* if caller hasn't specified an attack, find one */
    if (!mattk) {
        for (i = 0;; i++) {
            if (i >= NATTK)
                return; /* no passive attacks */
            if (ptr->mattk[i].aatyp == AT_NONE)
                break;  /* try this one */
        }
        mattk = &(ptr->mattk[i]);
    }

    switch (mattk->adtyp) {

    case AD_ACID:
        if (!rn2(6)) {
            erode_obj(obj, NULL, ERODE_CORRODE, TRUE, TRUE);
        }
        break;
    case AD_RUST:
        if (!cancelled(mon)) {
            erode_obj(obj, NULL, ERODE_RUST, TRUE, TRUE);
        }
        break;
    case AD_CORR:
        if (!cancelled(mon)) {
            erode_obj(obj, NULL, ERODE_CORRODE, TRUE, TRUE);
        }
        break;
    case AD_ENCH:
        if (!cancelled(mon)) {
            if (drain_item(obj) && carried(obj) &&
                (obj->known || obj->oclass == ARMOR_CLASS)) {
                pline(msgc_itemloss, "Your %s less effective.",
                      aobjnam(obj, "seem"));
            }
            break;
        }
    default:
        break;
    }

    if (carried(obj))
        update_inventory();
}

/* Note: caller must ascertain mtmp is mimicking... */
void
stumble_onto_mimic(struct monst *mtmp, schar dx, schar dy)
{
    const char *fmt = "Wait!  That's %s!", *generic = "a monster", *what = 0;

    if (!u.ustuck && !mtmp->mflee && dmgtype(mtmp->data, AD_STCK))
        u.ustuck = mtmp;

    if (Blind) {
        if (!Blind_telepat)
            what = generic;     /* with default fmt */
        else if (mtmp->m_ap_type == M_AP_MONSTER)
            what = a_monnam(mtmp);      /* differs from what was sensed */
    } else {
        if (level->locations[u.ux + dx][u.uy + dy].mem_bg == S_hcdoor ||
            level->locations[u.ux + dx][u.uy + dy].mem_bg == S_vcdoor)
            fmt = "The door actually was %s!";
        else if (level->locations[u.ux + dx][u.uy + dy].mem_obj - 1 ==
                 GOLD_PIECE)
            fmt = "That gold was %s!";

        /* cloned Wiz starts out mimicking some other monster and might make
           himself invisible before being revealed */
        if (invisible(mtmp) && !see_invisible(&youmonst))
            what = generic;
        else
            what = a_monnam(mtmp);
    }
    if (what)
        pline(msgc_youdiscover, fmt, what);

    wakeup(mtmp, TRUE);       /* clears mimicking */
    update_objpile(level, u.ux + dx, u.uy + dy);
}

static void
nohandglow(struct monst *mon)
{
    const char *hands = makeplural(body_part(HAND));

    if (!u.umconf || confused(mon))
        return;
    if (u.umconf == 1) {
        if (Blind)
            pline(msgc_statusend, "Your %s stop tingling.", hands);
        else
            pline(msgc_statusend, "Your %s stop glowing %s.", hands,
                  hcolor("red"));
    } else {
        if (Blind)
            pline(msgc_statusend, "The tingling in your %s lessens.", hands);
        else
            pline(msgc_statusend, "Your %s no longer glow so brightly %s.",
                  hands, hcolor("red"));
    }
    u.umconf--;
}

/*uhitm.c*/
