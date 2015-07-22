/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-22 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"        /* for checking save modes */
#include "trietable.h"
#include <stdint.h>

static void stoned_dialogue(void);
static void vomiting_dialogue(void);
static void choke_dialogue(void);
static void slime_dialogue(void);
static void slip_or_trip(void);
static void see_lamp_flicker(struct obj *, const char *);
static void lantern_message(struct obj *);
static void cleanup_burn(void *, long);
static void hatch_egg(void *, long);
static void burn_object(void *, long);


/* He is being petrified - dialogue by inmet!tower */
static const char *const stoned_texts[] = {
    "You are slowing down.",    /* 5 */
    "Your limbs are stiffening.",       /* 4 */
    "Your limbs have turned to stone.", /* 3 */
    "You have turned to stone.",        /* 2 */
    "You are a statue." /* 1 */
};

static void
stoned_dialogue(void)
{
    long i = (Stoned & TIMEOUT);

    if (i > 0L && i <= SIZE(stoned_texts)) {
        pline("%s", stoned_texts[SIZE(stoned_texts) - i]);
        /* ensure the player is able to act on this message */
        action_interrupted();
    }
    if (i == 5L)
        HFast = 0L;
    if (i == 3L)
        helpless(3, hr_paralyzed, "unable to move due to turning to stone",
                 NULL);
    exercise(A_DEX, FALSE);
}

/* He is getting sicker and sicker prior to vomiting */
static const char *const vomiting_texts[] = {
    "You are feeling mildly nauseated.",        /* 14 */
    "You feel slightly confused.",      /* 11 */
    "You can't seem to think straight.",        /* 8 */
    "You feel incredibly sick.",        /* 5 */
    "You suddenly vomit!"       /* 2 */
};

static void
vomiting_dialogue(void)
{
    long i = (Vomiting & TIMEOUT) / 3L;

    if ((((Vomiting & TIMEOUT) % 3L) == 2) && (i >= 0)
        && (i < SIZE(vomiting_texts)))
        pline("%s", vomiting_texts[SIZE(vomiting_texts) - i - 1]);

    switch ((int)i) {
    case 0:
        vomit();
        morehungry(20);
        break;
    case 2:
        make_stunned(HStun + dice(2, 4), FALSE);
        /* fall through */
    case 3:
        make_confused(HConfusion + dice(2, 4), FALSE);
        break;
    }
    exercise(A_CON, FALSE);
}

static const char *const choke_texts[] = {
    "You find it hard to breathe.",
    "You're gasping for air.",
    "You can no longer breathe.",
    "You're turning %s.",
    "You suffocate."
};

static const char *const choke_texts2[] = {
    "Your %s is becoming constricted.",
    "Your blood is having trouble reaching your brain.",
    "The pressure on your %s increases.",
    "Your consciousness is fading.",
    "You suffocate."
};

static void
choke_dialogue(void)
{
    long i = (Strangled & TIMEOUT);

    if (i > 0 && i <= SIZE(choke_texts)) {
        if (Breathless || !rn2(50))
            pline(choke_texts2[SIZE(choke_texts2) - i], body_part(NECK));
        else {
            const char *str = choke_texts[SIZE(choke_texts) - i];

            if (strchr(str, '%'))
                pline(str, hcolor("blue"));
            else
                pline("%s", str);
        }
    }
    exercise(A_STR, FALSE);
}

static const char *const slime_texts[] = {
    "You are turning a little %s.",     /* 5 */
    "Your limbs are getting oozy.",     /* 4 */
    "Your skin begins to peel away.",   /* 3 */
    "You are turning into %s.", /* 2 */
    "You have become %s."       /* 1 */
};

static void
slime_dialogue(void)
{
    long i = (Slimed & TIMEOUT) / 2L;

    if (((Slimed & TIMEOUT) % 2L) && i >= 0L && i < SIZE(slime_texts)) {
        const char *str = slime_texts[SIZE(slime_texts) - i - 1L];

        if (strchr(str, '%')) {
            if (i == 4L) {      /* "you are turning green" */
                if (!Blind)     /* [what if you're already green?] */
                    pline(str, hcolor("green"));
            } else {
                if (Hallucination) {
                    int idx = rndmonidx();

                    pline(str, monnam_is_pname(idx)
                          ? monnam_for_index(idx)
                          : (idx < SPECIAL_PM && (mons[idx].geno & G_UNIQ))
                          ? the(monnam_for_index(idx))
                          : an(monnam_for_index(idx)));
                } else {
                    pline(str, "a green slime");
                }
            }
        } else
            pline("%s", str);
    }
    if (i == 3L) {      /* limbs becoming oozy */
        HFast = 0L;     /* lose intrinsic speed */
        action_interrupted();
    }
    exercise(A_DEX, FALSE);
}

void
burn_away_slime(void)
{
    if (Slimed) {
        pline("The slime that covers you is burned away!");
        Slimed = 0L;
    }
    return;
}



void
nh_timeout(void)
{
    unsigned *upp;
    int sleeptime;
    int baseluck = (flags.moonphase == FULL_MOON) ? 1 : 0;

    if (flags.friday13)
        baseluck -= 1;

    if (u.uluck != baseluck &&
        moves % (Uhave_amulet || u.ugangr ? 300 : 600) == 0) {
        /* Cursed luckstones stop bad luck from timing out; blessed luckstones
           stop good luck from timing out; normal luckstones stop both; neither
           is stopped if you don't have a luckstone. Luck is based at 0
           usually, +1 if a full moon and -1 on Friday 13th */
        int time_luck = stone_luck(FALSE);
        boolean nostone = !carrying(LUCKSTONE) && !stone_luck(TRUE);

        if (u.uluck > baseluck && (nostone || time_luck < 0))
            u.uluck--;
        else if (u.uluck < baseluck && (nostone || time_luck > 0))
            u.uluck++;
    }
    if (u.uinvulnerable)
        return; /* things past this point could kill you */
    if (Stoned)
        stoned_dialogue();
    if (Slimed)
        slime_dialogue();
    if (Vomiting)
        vomiting_dialogue();
    if (Strangled)
        choke_dialogue();
    if (u.mtimedone && !--u.mtimedone) {
        if (Unchanging)
            u.mtimedone = rnd(100 * youmonst.data->mlevel + 1);
        else
            rehumanize(DIED, NULL);
    }
    if (u.ucreamed)
        u.ucreamed--;

    /* Dissipate spell-based protection. */
    if (u.usptime) {
        if (--u.usptime == 0 && u.uspellprot) {
            u.usptime = u.uspmtime;
            u.uspellprot--;
            if (!Blind)
                pline_once("The %s haze around you %s.", hcolor("golden"),
                           u.uspellprot ? "becomes less dense" : "disappears");
        }
    }

    if (u.ugallop) {
        if (--u.ugallop == 0L && u.usteed)
            pline("%s stops galloping.", Monnam(u.usteed));
    }

    for (upp = u.uintrinsic; upp < u.uintrinsic + SIZE(u.uintrinsic); upp++)
        if ((*upp & TIMEOUT) && !(--*upp & TIMEOUT)) {
            switch (upp - u.uintrinsic) {
            case STONED:
                done(STONING, delayed_killer(STONING));
                break;
            case SLIMED:
                done(TURNED_SLIME, delayed_killer(TURNED_SLIME));
                break;
            case VOMITING:
                make_vomiting(0L, TRUE);
                break;
            case SICK:
                pline("You die from your illness.");
                done(POISONING, delayed_killer(POISONING));
                break;
            case FAST:
                if (!Very_fast)
                    pline("You feel yourself slowing down%s.",
                          Fast ? " a bit" : "");
                break;
            case CONFUSION:
                HConfusion = 1; /* So make_confused works properly */
                make_confused(0L, TRUE);
                action_interrupted();
                break;
            case STUNNED:
                HStun = 1;
                make_stunned(0L, TRUE);
                action_interrupted();
                break;
            case BLINDED:
                Blinded = 1;
                make_blinded(0L, TRUE);
                action_interrupted();
                break;
            case INVIS:
                newsym(u.ux, u.uy);
                if (!Invis && !BInvis && !Blind) {
                    pline(!See_invisible ? "You are no longer invisible." :
                          "You can no longer see through yourself.");
                    action_interrupted();
                }
                break;
            case SEE_INVIS:
                set_mimic_blocking();   /* do special mimic handling */
                see_monsters(FALSE);    /* make invis mons appear */
                newsym(u.ux, u.uy);     /* make self appear */
                action_interrupted();
                break;
            case LWOUNDED_LEGS:
                heal_legs(LEFT_SIDE);
                action_interrupted();
                break;
            case RWOUNDED_LEGS:
                heal_legs(RIGHT_SIDE);
                action_interrupted();
                break;
            case HALLUC:
                HHallucination = 1;
                make_hallucinated(0L, TRUE);
                action_interrupted();
                break;
            case SLEEPING:
                if (u_helpless(hm_unconscious) || Sleep_resistance)
                    HSleeping += rnd(100);
                else if (Sleeping) {
                    pline("You fall asleep.");
                    sleeptime = rnd(20);
                    helpless(sleeptime, hr_asleep, "sleeping", NULL);
                    HSleeping += sleeptime + rnd(100);
                }
                break;
            case LEVITATION:
                float_down(I_SPECIAL | TIMEOUT);
                break;
            case STRANGLED:
                done(SUFFOCATION, killer_msg(
                         SUFFOCATION, u.uburied ?
                         "suffocation" : "strangulation"));
                break;
            case FUMBLING:
                /* call this only when a move took place.  */
                /* otherwise handle fumbling msgs locally. */
                if (u.umoved && !Levitation) {
                    slip_or_trip();
                    helpless(2, hr_moving, "fumbling", "");
                    /* The more you are carrying the more likely you are to
                       make noise when you fumble.  Adjustments to this number
                       must be thoroughly play tested. */
                    if ((inv_weight() > -500)) {
                        pline("You make a lot of noise!");
                        wake_nearby(FALSE);
                    }
                }
                /* from outside means slippery ice; don't reset counter if
                   that's the only fumble reason */
                HFumbling &= ~FROMOUTSIDE;
                if (Fumbling)
                    HFumbling += rnd(20);
                break;
            case DETECT_MONSTERS:
                see_monsters(FALSE);
                break;
            }
        }

    run_timers();
}


/* Attach an egg hatch timeout to the given egg. */
void
attach_egg_hatch_timeout(struct obj *egg)
{
    int i;

    /* stop previous timer, if any */
    stop_timer(egg->olev, HATCH_EGG, egg);

    /*
     * Decide if and when to hatch the egg.  The old hatch_it() code tried
     * once a turn from age 151 to 200 (inclusive), hatching if it rolled
     * a number x, 1<=x<=age, where x>150.  This yields a chance of
     * hatching > 99.9993%.  Mimic that here.
     */
    for (i = (MAX_EGG_HATCH_TIME - 50) + 1; i <= MAX_EGG_HATCH_TIME; i++)
        if (rnd(i) > 150) {
            /* egg will hatch */
            start_timer(egg->olev, (long)i, TIMER_OBJECT, HATCH_EGG, egg);
            break;
        }
}

/* prevent an egg from ever hatching */
void
kill_egg(struct obj *egg)
{
    /* stop previous timer, if any */
    stop_timer(egg->olev, HATCH_EGG, egg);
}

/* timer callback routine: hatch the given egg */
void
hatch_egg(void *arg, long timeout)
{
    struct obj *egg;
    struct monst *mon, *mon2;
    coord cc;
    xchar x, y;
    boolean yours, silent, knows_egg = FALSE;
    boolean cansee_hatchspot = FALSE;
    int i, mnum, hatchcount = 0;

    egg = (struct obj *)arg;
    /* sterilized while waiting */
    if (egg->corpsenm == NON_PM)
        return;

    mon = mon2 = NULL;
    mnum = big_to_little(egg->corpsenm);
    /* The identity of one's father is learned, not innate */
    yours = (egg->spe || (!u.ufemale && carried(egg) && !rn2(2)));
    silent = (timeout != moves);        /* hatched while away */

    /* only can hatch when in INVENT, FLOOR, MINVENT */
    if (get_obj_location(egg, &x, &y, 0)) {
        hatchcount = rnd((int)egg->quan);
        cansee_hatchspot = cansee(x, y) && !silent;
        if (!(mons[mnum].geno & G_UNIQ) &&
            !(mvitals[mnum].mvflags & (G_GENOD | G_EXTINCT))) {
            for (i = hatchcount; i > 0; i--) {
                if (!enexto(&cc, level, x, y, &mons[mnum]) ||
                    !((mon = makemon(&mons[mnum], level,
                                     cc.x, cc.y, NO_MINVENT))))
                    break;
                /* tame if your own egg hatches while you're on the same
                   dungeon level, or any dragon egg which hatches while it's in
                   your inventory */
                if ((yours && !silent) ||
                    (carried(egg) && mon->data->mlet == S_DRAGON)) {
                    if ((mon2 = tamedog(mon, NULL)) != 0) {
                        mon = mon2;
                        if (carried(egg) && mon->data->mlet != S_DRAGON)
                            mon->mtame = 20;
                    }
                }
                if (mvitals[mnum].mvflags & G_EXTINCT)
                    break;      /* just made last one */
                mon2 = mon;     /* in case makemon() fails on 2nd egg */
            }
            if (!mon)
                mon = mon2;
            hatchcount -= i;
            egg->quan -= (long)hatchcount;
        }
    }

    if (mon) {
        const char *monnambuf, *carriedby;
        boolean siblings = (hatchcount > 1), redraw = FALSE;


        monnambuf = msgcat(siblings ? "some " : "",
                           siblings ? makeplural(m_monnam(mon)) :
                           an(m_monnam(mon)));
        /* we don't learn the egg type here because learning an egg type
           requires either seeing the egg hatch or being familiar with the
           egg already, as well as being able to see the resulting monster,
           checked below */

        switch (egg->where) {
        case OBJ_INVENT:
            knows_egg = TRUE;   /* true even if you are blind */
            if (!cansee_hatchspot)
                pline("You feel something %s from your pack!",
                      locomotion(mon->data, "drop"));
            else
                pline("You see %s %s out of your pack!", monnambuf,
                      locomotion(mon->data, "drop"));
            if (yours) {
                pline("%s cries sound like \"%s%s\"",
                      siblings ? "Their" : "Its",
                      u.ufemale ? "mommy" : "daddy", egg->spe ? "." : "?");
            } else if (mon->data->mlet == S_DRAGON) {
                verbalize("Gleep!");    /* Mything eggs :-) */
            }
            break;

        case OBJ_FLOOR:
            if (cansee_hatchspot) {
                knows_egg = TRUE;
                pline("You see %s hatch.", monnambuf);
                redraw = TRUE;  /* update egg's map location */
            }
            break;

        case OBJ_MINVENT:
            if (cansee_hatchspot) {
                /* egg carring monster might be invisible; if the egg carrying
                   monster is sensed by telepathy or the like, you can't sense
                   its pack */
                if (canseemon(egg->ocarry)) {
                    carriedby = msgcat(s_suffix(a_monnam(egg->ocarry)),
                                       " pack");
                    knows_egg = TRUE;
                } else if (is_pool(level, mon->mx, mon->my))
                    carriedby = "empty water";
                else
                    carriedby = "thin air";
                pline("You see %s %s out of %s!", monnambuf,
                      locomotion(mon->data, "drop"), carriedby);
            }
            break;
        default:
            impossible("egg hatched where? (%d)", (int)egg->where);
            break;
        }

        if (cansee_hatchspot && knows_egg)
            learn_egg_type(mnum);

        /* Sanity check. */
        if (egg->olev != level)
            impossible("Egg hatched off-level?");

        if (egg->quan > 0) {
            /* still some eggs left */
            attach_egg_hatch_timeout(egg);
            if (egg->timed) {
                /* replace ordinary egg timeout with a short one */
                stop_timer(egg->olev, HATCH_EGG, egg);
                start_timer(egg->olev, (long)rnd(12), TIMER_OBJECT, HATCH_EGG,
                            egg);
            }
        } else if (carried(egg)) {
            useup(egg);
        } else {
            /* free egg here because we use it above */
            obj_extract_self(egg);
            obfree(egg, NULL);
        }
        if (redraw)
            newsym(x, y);
    }
}

/* Learn to recognize eggs of the given type. */
void
learn_egg_type(int mnum)
{
    /* baby monsters hatch from grown-up eggs */
    mnum = little_to_big(mnum);
    mvitals[mnum].mvflags |= MV_KNOWS_EGG;
    /* we might have just learned about other eggs being carried */
    update_inventory();
}

/* Attach a fig_transform timeout to the given figurine. */
void
attach_fig_transform_timeout(struct obj *figurine)
{
    int i;

    /* stop previous timer, if any */
    stop_timer(figurine->olev, FIG_TRANSFORM, figurine);

    /*
     * Decide when to transform the figurine.
     */
    i = rnd(9000) + 200;
    /* figurine will transform */
    start_timer(figurine->olev, (long)i, TIMER_OBJECT, FIG_TRANSFORM, figurine);
}

/* give a fumble message */
static void
slip_or_trip(void)
{
    struct obj *otmp = vobj_at(u.ux, u.uy);
    const char *what, *pronoun;
    boolean on_foot = TRUE;

    if (u.usteed)
        on_foot = FALSE;

    if (otmp && on_foot && !u.uinwater && is_pool(level, u.ux, u.uy))
        otmp = 0;

    if (otmp && on_foot) {      /* trip over something in particular */
        /*
           If there is only one item, it will have just been named during the
           move, so refer to by via pronoun; otherwise, if the top item has
           been or can be seen, refer to it by name; if not, look for rocks to
           trip over; trip over anonymous "something" if there aren't any
           rocks. */
        pronoun = otmp->quan == 1L ? "it" : Hallucination ? "they" : "them";
        what = !otmp->nexthere ? pronoun :
            (otmp->dknown || !Blind) ? doname(otmp) :
            ((otmp = sobj_at(ROCK, level, u.ux, u.uy)) == 0 ? "something" :
             (otmp-> quan == 1L ? "a rock" : "some rocks"));
        if (Hallucination) {
            pline("Egads!  %s bite%s your %s!", msgupcasefirst(what),
                  (!otmp || otmp->quan == 1L) ? "s" : "", body_part(FOOT));
        } else {
            pline("You trip over %s.", what);
        }
    } else if (rn2(3) && is_ice(level, u.ux, u.uy)) {
        pline("%s %s%s on the ice.",
              u.usteed ?
              msgupcasefirst(x_monnam(u.usteed,
                                      u.usteed->mnamelth ?
                                      ARTICLE_NONE : ARTICLE_THE, NULL,
                                      SUPPRESS_SADDLE, FALSE)) : "You",
              rn2(2) ? "slip" : "slide", on_foot ? "" : "s");
    } else {
        if (on_foot) {
            switch (rn2(4)) {
            case 1:
                pline("You trip over your own %s.",
                      Hallucination ? "elbow" : makeplural(body_part(FOOT)));
                break;
            case 2:
                pline("You slip %s.",
                      Hallucination ? "on a banana peel" : "and nearly fall");
                break;
            case 3:
                pline("You flounder.");
                break;
            default:
                pline("You stumble.");
                break;
            }
        } else {
            switch (rn2(4)) {
            case 1:
                pline("Your %s slip out of the stirrups.",
                      makeplural(body_part(FOOT)));
                break;
            case 2:
                pline("You let go of the reins.");
                break;
            case 3:
                pline("You bang into the saddle-horn.");
                break;
            default:
                pline("You slide to one side of the saddle.");
                break;
            }
            dismount_steed(DISMOUNT_FELL);
        }
    }
}

/* Print a lamp flicker message with tailer. */
static void
see_lamp_flicker(struct obj *obj, const char *tailer)
{
    switch (obj->where) {
    case OBJ_INVENT:
    case OBJ_MINVENT:
        pline("%s flickers%s.", Yname2(obj), tailer);
        break;
    case OBJ_FLOOR:
        pline("You see %s flicker%s.", an(xname(obj)), tailer);
        break;
    default:
        return;
    }
    obj->known = 1;     /* ID charge count */
}

/* Print a dimming message for brass lanterns. */
static void
lantern_message(struct obj *obj)
{
    /* from adventure */
    switch (obj->where) {
    case OBJ_INVENT:
        pline("Your lantern is getting dim.");
        if (Hallucination)
            pline("Batteries have not been invented yet.");
        break;
    case OBJ_FLOOR:
        pline("You see a lantern getting dim.");
        break;
    case OBJ_MINVENT:
        pline("%s lantern is getting dim.", s_suffix(Monnam(obj->ocarry)));
        break;
    default:
        return;
    }
    obj->known = 1;
}

/*
 * Timeout callback for for objects that are burning. E.g. lamps, candles.
 * See begin_burn() for meanings of obj->age and obj->spe.
 */
void
burn_object(void *arg, long timeout)
{
    struct obj *obj = (struct obj *)arg;
    boolean canseeit, many, menorah, need_newsym;
    xchar x, y;
    const char *whose;

    menorah = obj->otyp == CANDELABRUM_OF_INVOCATION;
    many = menorah ? obj->spe > 1 : obj->quan > 1L;

    /* timeout while away */
    if (timeout != moves) {
        long how_long = moves - timeout;

        if (how_long >= obj->age) {
            obj->age = 0;
            end_burn(obj, FALSE);

            if (menorah) {
                obj->spe = 0;   /* no more candles */
            } else if (Is_candle(obj) || obj->otyp == POT_OIL) {
                /* get rid of candles and burning oil potions */
                obj_extract_self(obj);
                obfree(obj, NULL);
                obj = NULL;
            }

        } else {
            obj->age -= how_long;
            begin_burn(obj, TRUE);
        }
        return;
    }

    /* set up `whose[]' to be "Your" or "Fred's" or "The goblin's" */
    whose = Shk_Your(obj);

    /* only interested in INVENT, FLOOR, and MINVENT */
    if (get_obj_location(obj, &x, &y, 0)) {
        canseeit = !Blind && cansee(x, y);
    } else {
        canseeit = FALSE;
    }
    need_newsym = FALSE;

    /* obj->age is the age remaining at this point.  */
    switch (obj->otyp) {
    case POT_OIL:
        /* this should only be called when we run out */
        if (canseeit) {
            switch (obj->where) {
            case OBJ_INVENT:
            case OBJ_MINVENT:
                pline("%s potion of oil has burnt away.", whose);
                break;
            case OBJ_FLOOR:
                pline("You see a burning potion of oil go out.");
                need_newsym = TRUE;
                break;
            }
        }
        end_burn(obj, FALSE);   /* turn off light source */
        obj_extract_self(obj);
        obfree(obj, NULL);
        obj = NULL;
        break;

    case BRASS_LANTERN:
    case OIL_LAMP:
        switch ((int)obj->age) {
        case 150:
        case 100:
        case 50:
            if (canseeit) {
                if (obj->otyp == BRASS_LANTERN)
                    lantern_message(obj);
                else
                    see_lamp_flicker(obj,
                                     obj->age == 50L ? " considerably" : "");
            }
            break;

        case 25:
            if (canseeit) {
                if (obj->otyp == BRASS_LANTERN)
                    lantern_message(obj);
                else {
                    switch (obj->where) {
                    case OBJ_INVENT:
                    case OBJ_MINVENT:
                        pline("%s %s seems about to go out.", whose,
                              xname(obj));
                        obj->known = 1;
                        break;
                    case OBJ_FLOOR:
                        pline("You see %s about to go out.", an(xname(obj)));
                        obj->known = 1;
                        break;
                    }
                }
            }
            break;

        case 0:
            /* even if blind you'll know if holding it */
            if (canseeit || obj->where == OBJ_INVENT) {
                switch (obj->where) {
                case OBJ_INVENT:
                case OBJ_MINVENT:
                    if (obj->otyp == BRASS_LANTERN)
                        pline("%s lantern has run out of power.", whose);
                    else
                        pline("%s %s has gone out.", whose, xname(obj));
                    obj->known = 1;
                    break;
                case OBJ_FLOOR:
                    if (obj->otyp == BRASS_LANTERN)
                        pline("You see a lantern run out of power.");
                    else
                        pline("You see %s go out.", an(xname(obj)));
                    obj->known = 1;
                    break;
                }
            }
            end_burn(obj, FALSE);
            break;

        default:
            /*
             * Someone added fuel to the lamp while it was
             * lit.  Just fall through and let begin burn
             * handle the new age.
             */
            break;
        }

        if (obj->age)
            begin_burn(obj, TRUE);

        break;

    case CANDELABRUM_OF_INVOCATION:
    case TALLOW_CANDLE:
    case WAX_CANDLE:
        switch (obj->age) {
        case 75:
            if (canseeit)
                switch (obj->where) {
                case OBJ_INVENT:
                case OBJ_MINVENT:
                    pline("%s %scandle%s getting short.", whose,
                          menorah ? "candelabrum's " : "",
                          many ? "s are" : " is");
                    obj->known = 1;
                    break;
                case OBJ_FLOOR:
                    pline("You see %scandle%s getting short.",
                          menorah ? "a candelabrum's " : many ? "some " : "a ",
                          many ? "s" : "");
                    obj->known = 1;
                    break;
                }
            break;

        case 15:
            if (canseeit)
                switch (obj->where) {
                case OBJ_INVENT:
                case OBJ_MINVENT:
                    pline("%s %scandle%s flame%s flicker%s low!", whose,
                          menorah ? "candelabrum's " : "", many ? "s'" : "'s",
                          many ? "s" : "", many ? "" : "s");
                    obj->known = 1;
                    break;
                case OBJ_FLOOR:
                    pline("You see %scandle%s flame%s flicker low!",
                          menorah ? "a candelabrum's " : many ? "some " : "a ",
                          many ? "s'" : "'s", many ? "s" : "");
                    obj->known = 1;
                    break;
                }
            break;

        case 0:
            /* we know even if blind and in our inventory */
            if (canseeit || obj->where == OBJ_INVENT) {
                if (menorah) {
                    switch (obj->where) {
                    case OBJ_INVENT:
                    case OBJ_MINVENT:
                        pline("%s candelabrum's flame%s.", whose,
                              many ? "s die" : " dies");
                        obj->known = 1;
                        break;
                    case OBJ_FLOOR:
                        pline("You see a candelabrum's flame%s die.",
                              many ? "s" : "");
                        obj->known = 1;
                        break;
                    }
                } else {
                    switch (obj->where) {
                    case OBJ_INVENT:
                    case OBJ_MINVENT:
                        pline("%s %s %s consumed!", whose, xname(obj),
                              many ? "are" : "is");
                        obj->known = 1;
                        break;
                    case OBJ_FLOOR:
                        /*
                           You see some wax candles consumed! You see a wax
                           candle consumed! */
                        pline("You see %s%s consumed!", many ? "some " : "",
                              many ? xname(obj) : an(xname(obj)));
                        need_newsym = TRUE;
                        obj->known = 1;
                        break;
                    }

                    /* post message */
                    pline(Hallucination
                          ? (many ? "They shriek!" : "It shrieks!") : Blind ? ""
                          : (many ? "Their flames die." : "Its flame dies."));
                }
            }
            end_burn(obj, FALSE);

            if (menorah) {
                obj->spe = 0;
                obj->owt = weight(obj); /* no more candles */
                (void)encumber_msg();
            } else {
                obj_extract_self(obj);
                obfree(obj, NULL);
                obj = NULL;
            }
            break;

        default:
            /*
             * Someone added fuel (candles) to the menorah while
             * it was lit.  Just fall through and let begin burn
             * handle the new age.
             */
            break;
        }

        if (obj && obj->age)
            begin_burn(obj, TRUE);

        break;

    default:
        impossible("burn_object: unexpeced obj %s", xname(obj));
        break;
    }
    if (need_newsym)
        newsym(x, y);
}

/*
 * Start a burn timeout on the given object. If not "already lit" then
 * create a light source for the vision system.  There had better not
 * be a burn already running on the object.
 *
 * Magic lamps stay lit as long as there's a genie inside, so don't start
 * a timer.
 *
 * Burn rules:
 *      potions of oil, lamps & candles:
 *         age = # of turns of fuel left
 *         spe = <unused>
 *
 *      magic lamps:
 *         age = <unused>
 *         spe = 0 not lightable, 1 lightable forever
 *
 *      candelabrum:
 *         age = # of turns of fuel left
 *         spe = # of candles
 *
 * Once the burn begins, the age will be set to the amount of fuel
 * remaining _once_the_burn_finishes_.  If the burn is terminated
 * early then fuel is added back.
 *
 * This use of age differs from the use of age for corpses and eggs.
 * For the latter items, age is when the object was created, so we
 * know when it becomes "bad".
 *
 * This is a "silent" routine - it should not print anything out.
 */
void
begin_burn(struct obj *obj, boolean already_lit)
{
    int radius = 3;
    long turns = 0;
    boolean do_timer = TRUE;

    if (obj->age == 0 && obj->otyp != MAGIC_LAMP && !artifact_light(obj))
        return;

    switch (obj->otyp) {
    case MAGIC_LAMP:
        obj->lamplit = 1;
        do_timer = FALSE;
        break;

    case POT_OIL:
        turns = obj->age;
        radius = 1;     /* very dim light */
        break;

    case BRASS_LANTERN:
    case OIL_LAMP:
        /* magic times are 150, 100, 50, 25, and 0 */
        if (obj->age > 150L)
            turns = obj->age - 150L;
        else if (obj->age > 100L)
            turns = obj->age - 100L;
        else if (obj->age > 50L)
            turns = obj->age - 50L;
        else if (obj->age > 25L)
            turns = obj->age - 25L;
        else
            turns = obj->age;
        break;

    case CANDELABRUM_OF_INVOCATION:
    case TALLOW_CANDLE:
    case WAX_CANDLE:
        /* magic times are 75, 15, and 0 */
        if (obj->age > 75L)
            turns = obj->age - 75L;
        else if (obj->age > 15L)
            turns = obj->age - 15L;
        else
            turns = obj->age;
        radius = candle_light_range(obj);
        break;

    default:
        /* [ALI] Support artifact light sources */
        if (artifact_light(obj)) {
            obj->lamplit = 1;
            do_timer = FALSE;
            radius = 2;
        } else {
            impossible("begin burn: unexpected %s", xname(obj));
            turns = obj->age;
        }
        break;
    }

    if (do_timer) {
        if (start_timer(obj->olev, turns, TIMER_OBJECT, BURN_OBJECT, obj)) {
            obj->lamplit = 1;
            obj->age -= turns;
            if (carried(obj) && !already_lit)
                update_inventory();
        } else {
            obj->lamplit = 0;
        }
    } else {
        if (carried(obj) && !already_lit)
            update_inventory();
    }

    if (obj->lamplit && !already_lit) {
        xchar x, y;

        if (get_obj_location(obj, &x, &y, CONTAINED_TOO | BURIED_TOO))
            new_light_source(level, x, y, radius, LS_OBJECT, obj);
        else
            impossible("begin_burn: can't get obj position");
    }
}

/*
 * Stop a burn timeout on the given object if timer attached.  Darken
 * light source.
 */
void
end_burn(struct obj *obj, boolean timer_attached)
{
    if (!obj->lamplit) {
        impossible("end_burn: obj %s not lit", xname(obj));
        return;
    }

    if (obj->otyp == MAGIC_LAMP || artifact_light(obj))
        timer_attached = FALSE;

    if (!timer_attached) {
        /* [DS] Cleanup explicitly, since timer cleanup won't happen */
        del_light_source(obj->olev, LS_OBJECT, obj);
        obj->lamplit = 0;
        if (obj->where == OBJ_INVENT)
            update_inventory();
    } else if (!stop_timer(obj->olev, BURN_OBJECT, obj))
        impossible("end_burn: obj %s not timed!", xname(obj));
}


/*
 * Cleanup a burning object if timer stopped.
 */
static void
cleanup_burn(void *arg, long expire_time)
{
    struct obj *obj = (struct obj *)arg;

    if (!obj->lamplit) {
        impossible("cleanup_burn: obj %s not lit", xname(obj));
        return;
    }

    del_light_source(obj->olev, LS_OBJECT, arg);

    /* restore unused time */
    obj->age += expire_time - moves;

    obj->lamplit = 0;

    if (obj->where == OBJ_INVENT)
        update_inventory();
}


void
do_storms(void)
{
    int nstrike;
    int x, y;
    int dirx, diry;
    int count;

    /* no lightning if not the air level or too often, even then */
    if (!Is_airlevel(&u.uz) || rn2(8))
        return;

    /* the number of strikes is 8-log2(nstrike) */
    for (nstrike = rnd(64); nstrike <= 64; nstrike *= 2) {
        count = 0;
        do {
            x = rn2(COLNO);
            y = rn2(ROWNO);
        } while (++count < 100 && level->locations[x][y].typ != CLOUD);

        if (count < 100) {
            dirx = rn2(3) - 1;
            diry = rn2(3) - 1;
            if (dirx != 0 || diry != 0)
                buzz(-15,       /* "monster" LIGHTNING spell */
                     8, x, y, dirx, diry);
        }
    }

    if (level->locations[u.ux][u.uy].typ == CLOUD) {
        /* inside a cloud during a thunder storm is deafening */
        pline("Kaboom!!!  Boom!!  Boom!!");
        if (!u.uinvulnerable)
            helpless(3, hr_afraid, "hiding from a thunderstorm", NULL);
    } else
        You_hear("a rumbling noise.");
}


/* ------------------------------------------------------------------------- */
/*
 * Generic Timeout Functions.
 *
 * Interface:
 *
 * General:
 *      boolean start_timer(struct level *lev, long timeout, short kind,
 *                          short func_index, void * arg)
 *         Start a timer of kind 'kind' that will expire at time
 *         moves+'timeout'.  Call the function at 'func_index'
 *         in the timeout table using argument 'arg'.  Return TRUE if
 *         a timer was started.  This places the timer on a list ordered
 *         "sooner" to "later".  If an object, increment the object's
 *         timer count.
 *
 *      long stop_timer(struct level *lev, short func_index, void * arg)
 *         Stop a timer specified by the (func_index, arg) pair.  This
 *         assumes that such a pair is unique.  Return the time the
 *         timer would have gone off.  If no timer is found, return 0.
 *         If an object, decrement the object's timer count.
 *
 *      long report_timer(struct level *lev, short func_index, void * arg)
 *         Look at a timer specified by the (func_index, arg) pair.  This
 *         assumes that such a pair is unique.  Return the time the
 *         timer is scheduled to go off.  If no timer is found, return 0.
 *
 *      void run_timers(void)
 *         Call timers that have timed out.
 *
 *
 * Save/Restore:
 *      void save_timers(struct memfile *mf, int mode, int range)
 *         Save all timers of range 'range'.  Range is either global
 *         or local.  Global timers follow game play, local timers
 *         are saved with a level.  Object and monster timers are
 *         saved using their respective id's instead of pointers.
 *
 *      void restore_timers(struct memfile *mf, int range, boolean ghostly,
 *                          long adjust)
 *         Restore timers of range 'range'.  If from a ghost pile,
 *         adjust the timeout by 'adjust'.  The object and monster
 *         ids are not restored until later.
 *
 *      void relink_timers(boolean ghostly)
 *         Relink all object and monster timers that had been saved
 *         using their object's or monster's id number.
 *
 * Object Specific:
 *      void obj_move_timers(struct obj *src, struct obj *dest)
 *         Reassign all timers from src to dest.
 *
 *      void obj_split_timers(struct obj *src, struct obj *dest)
 *         Duplicate all timers assigned to src and attach them to dest.
 *
 *      void obj_stop_timers(struct obj *obj)
 *         Stop all timers attached to obj.
 */

static const char *kind_name(short);
static void print_queue(struct nh_menulist *menu, timer_element *);
static void insert_timer(struct level *lev, timer_element * gnu);
static timer_element *remove_timer(timer_element **, short, void *);
static timer_element *peek_timer(timer_element **, short, const void *);
static void write_timer(struct memfile *mf, timer_element *);
static boolean mon_is_local(struct monst *);
static boolean timer_is_local(timer_element *);
static int maybe_write_timer(struct memfile *mf, struct level *lev, int range,
                             boolean write_it);

typedef struct {
    timeout_proc f, cleanup;
    const char *name;
} ttable;

/* table of timeout functions */
#define TTAB(a, b) {a, b, #a}
static const ttable timeout_funcs[NUM_TIME_FUNCS] = {
    TTAB(rot_organic,	NULL),
    TTAB(rot_corpse,	NULL),
    TTAB(revive_mon,	NULL),
    TTAB(burn_object,	cleanup_burn),
    TTAB(hatch_egg,	NULL),
    TTAB(fig_transform,	NULL)
};

#undef TTAB


static const char *
kind_name(short kind)
{
    switch (kind) {
    case TIMER_LEVEL:
        return "level";
    case TIMER_GLOBAL:
        return "global";
    case TIMER_OBJECT:
        return "object";
    }
    return "unknown";
}

static void
print_queue(struct nh_menulist *menu, timer_element * base)
{
    timer_element *curr;

    if (!base) {
        add_menutext(menu, "<empty>");
    } else {
        add_menutext(menu, "timeout\tid\tkind\tcall");
        for (curr = base; curr; curr = curr->next) {
            add_menutext(menu, msgprintf(
                             " %4u\t%4u\t%-6s #%d\t%s(%p)", curr->timeout,
                             curr->tid, kind_name(curr->kind), curr->func_index,
                             timeout_funcs[curr->func_index].name, curr->arg));
        }
    }
}

int
wiz_timeout_queue(const struct nh_cmd_arg *arg)
{
    struct nh_menulist menu;

    (void) arg;

    init_menulist(&menu);

    add_menutext(&menu, msgprintf("Current time = %u.", moves));
    add_menutext(&menu, "");
    add_menutext(&menu, "Active timeout queue:");
    add_menutext(&menu, "");
    print_queue(&menu, level->lev_timers);

    display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE, NULL);

    return 0;
}


/*
 * Pick off timeout elements from the global queue and call their functions.
 * Do this until their time is less than or equal to the move count.
 */
void
run_timers(void)
{
    timer_element *curr;

    /*
     * Always use the first element.  Elements may be added or deleted at
     * any time.  The list is ordered, we are done when the first element
     * is in the future.
     */
    while (level->lev_timers && level->lev_timers->timeout <= moves) {
        curr = level->lev_timers;
        level->lev_timers = curr->next;

        if (curr->kind == TIMER_OBJECT)
            ((struct obj *)(curr->arg))->timed--;
        (*timeout_funcs[curr->func_index].f) (curr->arg, curr->timeout);
        free(curr);
    }
}


/*
 * Start a timer.  Return TRUE if successful.
 */
boolean
start_timer(struct level *lev, long when, short kind, short func_index,
            void *arg)
{
    timer_element *gnu;

    if (func_index < 0 || func_index >= NUM_TIME_FUNCS)
        panic("start_timer");

    gnu = malloc(sizeof (timer_element));
    memset(gnu, 0, sizeof (timer_element));
    gnu->next = NULL;
    gnu->tid = timer_id++;
    gnu->timeout = moves + when;
    gnu->kind = kind;
    gnu->needs_fixup = FALSE;
    gnu->func_index = func_index;
    gnu->arg = arg;
    insert_timer(lev, gnu);

    if (kind == TIMER_OBJECT)   /* increment object's timed count */
        ((struct obj *)arg)->timed++;

    /* should check for duplicates and fail if any */
    return TRUE;
}


/*
 * Remove the timer from the current list and free it up.  Return the time
 * it would have gone off, 0 if not found.
 */
long
stop_timer(struct level *lev, short func_index, void *arg)
{
    timer_element *doomed;
    long timeout;

    doomed = remove_timer(&lev->lev_timers, func_index, arg);

    if (doomed) {
        timeout = doomed->timeout;
        if (doomed->kind == TIMER_OBJECT)
            ((struct obj *)arg)->timed--;
        if (timeout_funcs[doomed->func_index].cleanup)
            (*timeout_funcs[doomed->func_index].cleanup) (arg, timeout);
        free(doomed);
        return timeout;
    }
    return 0;
}

/*
 * Look at the timer list for a timer.  Return the time it was
 * scheduled to go off, 0 if not found.
 */
long
report_timer(struct level *lev, short func_index, const void *arg)
{
    timer_element *checking;

    checking = peek_timer(&lev->lev_timers, func_index, arg);

    if (checking) {
        return checking->timeout;
    }
    return 0;
}


/*
 * Move all object timers from src to dest, leaving src untimed.
 */
void
obj_move_timers(struct obj *src, struct obj *dest)
{
    int count;
    timer_element *curr;

    for (count = 0, curr = src->olev->lev_timers; curr; curr = curr->next)
        if (curr->kind == TIMER_OBJECT && curr->arg == src) {
            curr->arg = dest;
            dest->timed++;
            count++;
        }
    if (count != src->timed)
        panic("obj_move_timers");
    src->timed = 0;
}


/*
 * Find all object timers and duplicate them for the new object "dest".
 */
void
obj_split_timers(struct obj *src, struct obj *dest)
{
    timer_element *curr, *next_timer = 0;

    for (curr = src->olev->lev_timers; curr; curr = next_timer) {
        next_timer = curr->next;        /* things may be inserted */
        if (curr->kind == TIMER_OBJECT && curr->arg == src) {
            start_timer(dest->olev, curr->timeout - moves, TIMER_OBJECT,
                        curr->func_index, dest);
        }
    }
}


/*
 * Stop all timers attached to this object.  We can get away with this because
 * all object pointers are unique.
 */
void
obj_stop_timers(struct obj *obj)
{
    timer_element *curr, *prev, *next_timer = 0;

    for (prev = 0, curr = obj->olev->lev_timers; curr; curr = next_timer) {
        next_timer = curr->next;
        if (curr->kind == TIMER_OBJECT && curr->arg == obj) {
            if (prev)
                prev->next = curr->next;
            else
                obj->olev->lev_timers = curr->next;
            if (timeout_funcs[curr->func_index].cleanup)
                (*timeout_funcs[curr->func_index].cleanup)(
                    curr->arg, curr->timeout);
            free(curr);
        } else {
            prev = curr;
        }
    }
    obj->timed = 0;
}


/* Insert timer into the global queue */
static void
insert_timer(struct level *lev, timer_element * gnu)
{
    timer_element *curr, *prev;

    for (prev = 0, curr = lev->lev_timers; curr; prev = curr, curr = curr->next)
        /* For most purposes, > vs. >= has little effect. Using >=, however,
           ensures that we load timers in the same order as when they were saved
           to a file, which avoids desyncing the save. (We used to use > for
           the same reason, but timers are now loaded in reverse order in order
           to avoid a performance bottleneck.) */
        if (curr->timeout >= gnu->timeout)
            break;

    gnu->next = curr;
    if (prev)
        prev->next = gnu;
    else
        lev->lev_timers = gnu;
}


static timer_element *
remove_timer(timer_element ** base, short func_index, void *arg)
{
    timer_element *prev, *curr;

    for (prev = 0, curr = *base; curr; prev = curr, curr = curr->next)
        if (curr->func_index == func_index && curr->arg == arg)
            break;

    if (curr) {
        if (prev)
            prev->next = curr->next;
        else
            *base = curr->next;
    }

    return curr;
}

static timer_element *
peek_timer(timer_element ** base, short func_index, const void *arg)
{
    timer_element *curr;

    for (curr = *base; curr; curr = curr->next)
        if (curr->func_index == func_index && curr->arg == arg)
            break;

    return curr;
}

static void
write_timer(struct memfile *mf, timer_element * timer)
{
    intptr_t argval;
    boolean needs_fixup = FALSE;

    switch (timer->kind) {
    case TIMER_GLOBAL:
    case TIMER_LEVEL:
        /* assume no pointers in arg */
        argval = (intptr_t) timer->arg;
        break;

    case TIMER_OBJECT:
        if (timer->needs_fixup)
            argval = (intptr_t) timer->arg;
        else {
            /* replace object pointer with id */
            argval = ((struct obj *)timer->arg)->o_id;
            needs_fixup = TRUE;
        }
        break;

    default:
        panic("write_timer");
        break;
    }

    mtag(mf, timer->tid, MTAG_TIMER);
    mwrite32(mf, timer->tid);
    mwrite32(mf, timer->timeout);
    mwrite32(mf, argval);
    mwrite16(mf, timer->kind);
    mwrite8(mf, timer->func_index);
    mwrite8(mf, needs_fixup);
}


/*
 * Return TRUE if the object will stay on the level when the level is
 * saved.
 */
boolean
obj_is_local(struct obj *obj)
{
    switch (obj->where) {
    case OBJ_INVENT:
    case OBJ_MIGRATING:
        return FALSE;
    case OBJ_FLOOR:
    case OBJ_BURIED:
        return TRUE;
    case OBJ_CONTAINED:
        return obj_is_local(obj->ocontainer);
    case OBJ_MINVENT:
        return mon_is_local(obj->ocarry);
    }
    panic("obj_is_local");
    return FALSE;
}


/*
 * Return TRUE if the given monster will stay on the level when the
 * level is saved.
 */
static boolean
mon_is_local(struct monst *mon)
{
    struct monst *curr;

    for (curr = migrating_mons; curr; curr = curr->nmon)
        if (curr == mon)
            return FALSE;
    for (curr = turnstate.migrating_pets; curr; curr = curr->nmon)
        if (curr == mon)
            return FALSE;
    return TRUE;
}


/*
 * Return TRUE if the timer is attached to something that will stay on the
 * level when the level is saved.
 */
static boolean
timer_is_local(timer_element * timer)
{
    switch (timer->kind) {
    case TIMER_LEVEL:
        return TRUE;
    case TIMER_GLOBAL:
        return FALSE;
    case TIMER_OBJECT:
        return obj_is_local((struct obj *)timer->arg);
    }
    panic("timer_is_local");
    return FALSE;
}


/*
 * Part of the save routine.  Count up the number of timers that would
 * be written.  If write_it is true, actually write the timer.
 */
static int
maybe_write_timer(struct memfile *mf, struct level *lev, int range,
                  boolean write_it)
{
    int count = 0;
    timer_element *curr;

    for (curr = lev->lev_timers; curr; curr = curr->next) {
        if (range == RANGE_GLOBAL) {
            /* global timers */

            if (!timer_is_local(curr)) {
                count++;
                if (write_it)
                    write_timer(mf, curr);
            }

        } else {
            /* local timers */

            if (timer_is_local(curr)) {
                count++;
                if (write_it)
                    write_timer(mf, curr);
            }

        }
    }

    return count;
}


void
transfer_timers(struct level *oldlev, struct level *newlev,
                unsigned int obj_id)
{
    timer_element *curr, *prev = NULL, *next_timer = NULL;

    if (newlev == oldlev)
        return;

    for (curr = oldlev->lev_timers; curr; curr = next_timer) {
        next_timer = curr->next;        /* in case curr is removed */

	/* transfer global timers or timers of requested object */
	if ((!obj_id && !timer_is_local(curr)) ||
	    (obj_id && curr->kind == TIMER_OBJECT &&
	     ((struct obj *)curr->arg)->o_id == obj_id)) {
            if (prev)
                prev->next = curr->next;
            else
                oldlev->lev_timers = curr->next;

            insert_timer(newlev, curr);
            /* prev stays the same */
        } else {
            prev = curr;
        }
    }
}


/*
 * Save part of the timer list.  The parameter 'range' specifies either
 * global or level timers to save.  The timer ID is saved with the global
 * timers.
 *
 * Global range:
 *    + timeouts that follow the hero (global)
 *    + timeouts that follow obj & monst that are migrating
 *
 * Level range:
 *    + timeouts that are level specific (e.g. storms)
 *    + timeouts that stay with the level (obj & monst)
 */
void
save_timers(struct memfile *mf, struct level *lev, int range)
{
    int count;

    mtag(mf, 2 * (int)ledger_no(&lev->z) + range, MTAG_TIMERS);
    if (range == RANGE_GLOBAL)
        mwrite32(mf, timer_id);

    count = maybe_write_timer(mf, lev, range, FALSE);
    mwrite32(mf, count);
    maybe_write_timer(mf, lev, range, TRUE);
}


void
free_timers(struct level *lev)
{
    timer_element *curr, *next = NULL;

    for (curr = lev->lev_timers; curr; curr = next) {
        next = curr->next;
        free(curr);
    }
    lev->lev_timers = NULL;
}


/*
 * Pull in the structures from disk, but don't recalculate the object and
 * monster pointers.
 */
void
restore_timers(struct memfile *mf, struct level *lev, int range,
               boolean ghostly, /* restoring from a ghost level */
               long adjust)     /* how much to adjust timeout */
{
    int count;
    timer_element *curr;
    intptr_t argval;

    if (range == RANGE_GLOBAL)
        timer_id = mread32(mf);

    /* restore elements */
    count = mread32(mf);
    if (!count)
        return; /* don't generate a size-0 VLA */

    timer_element *temp_timers[count];
    int i = count;

    while (i-- > 0) {
        curr = malloc(sizeof (timer_element));

        curr->tid = mread32(mf);
        curr->timeout = mread32(mf);
        argval = mread32(mf);
        curr->arg = (void *)argval;
        curr->kind = mread16(mf);
        curr->func_index = mread8(mf);
        curr->needs_fixup = mread8(mf);

        if (ghostly)
            curr->timeout += adjust;

        /* The code here previously did `insert_timer(lev, curr);`. This is
           correct in terms of code effect. However, because the timer list
           is a linked list, and it's output front to back, and because the
           list of timers must be maintained in order, that had quadratic
           performance. Worse, during pudding farming, a /lot/ of timers are
           generated (one for each pudding corpse).

           Instead, we store the timers into an array temporarily, then add them
           in the opposite order, making the performance linear rather than
           quadratic. */
        temp_timers[i] = curr;
    }
    for (i = 0; i < count; i++)
        insert_timer(lev, temp_timers[i]);
}


/* reset all timers that are marked for resetting */
void
relink_timers(boolean ghostly, struct level *lev, struct trietable **table)
{
    timer_element *curr;
    unsigned nid;

    for (curr = lev->lev_timers; curr; curr = curr->next) {
        if (curr->needs_fixup) {
            if (curr->kind == TIMER_OBJECT) {
                if (ghostly) {
                    if (!lookup_id_mapping((intptr_t) curr->arg, &nid))
                        panic("relink_timers 1");
                } else
                    nid = (intptr_t) curr->arg;

                /* If necessary, we'll find the object in question using
                   find_oid. However, if we happened to cache its location in
                   a trietable, we can use that instead and save some time.

                   The semantics here are that the trietable is allowed to be
                   missing the object in question altogether; but if the
                   object's ID is a key in the table, the value will be the
                   object itself.

                   (This optimization is necessary; I've seen games with over
                   7000 timers, and without the optimization, they each had
                   to loop over all the objects on the level to find the one
                   they were applying to. That was quadratic performance, and
                   not irrelevantly so either.) */
                curr->arg = NULL;
                if (table)
                    curr->arg = trietable_find(table, nid);
                if (!curr->arg)
                    curr->arg = find_oid(nid);
                if (!curr->arg)
                    panic("cant find o_id %d", nid);
                curr->needs_fixup = 0;
            } else
                panic("relink_timers 2");
        }
    }
}

/*timeout.c*/
