/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-02-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

boolean notonhead = FALSE;

static const char beverages_and_fountains[] =
    { ALLOW_NONE, NONE_ON_COMMA, POTION_CLASS, 0 };
static const char beverages[] = { POTION_CLASS, 0 };

static int allowed_wonder[] = {
    FIRE_RES,
    COLD_RES,
    SLEEP_RES,
    DISINT_RES,
    SHOCK_RES,
    POISON_RES,
    ACID_RES,
    STONE_RES,
    REGENERATION,
    SEARCHING,
    SEE_INVIS,
    INVIS,
    TELEPORT,
    TELEPORT_CONTROL,
    POLYMORPH,
    POLYMORPH_CONTROL,
    LEVITATION,
    STEALTH,
    AGGRAVATE_MONSTER,
    CONFLICT,
    PROT_FROM_SHAPE_CHANGERS,
    WARNING,
    TELEPAT,
    FAST,
    SLEEPING,
    WWALKING,
    HUNGER,
    REFLECTING,
    ANTIMAGIC,
    DISPLACED,
    CLAIRVOYANT,
    ENERGY_REGENERATION,
    MAGICAL_BREATHING,
    SICK_RES,
    DRAIN_RES,
    CANCELLED,
    FREE_ACTION,
    SWIMMING,
    FIXED_ABIL,
    FLYING,
    UNCHANGING,
    PASSES_WALLS,
    INFRAVISION,
    SLOW,
};

static int allowed_wonder_size = sizeof(allowed_wonder) / sizeof(allowed_wonder[0]);

static short mixtype(struct obj *, struct obj *);


void
make_sick(struct monst *mon, long xtime, const char *cause,
          boolean talk, int type)
{
    int old = property_timeout(mon, SICK);
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    enum msg_channel msgc = (you ? msgc_fatal :
                             mon->mtame ? msgc_petfatal :
                             msgc_monneutral);
    if (xtime > 0L) {
        if (resists_sick(mon))
            return;
        if (talk && (you || vis))
            pline(msgc, !old ? "%s %s deathly sick." :
                  xtime <= old / 2L ? "%s %s much worse" :
                  "%s %s even worse", you ? "You" : Monnam(mon),
                  you ? "feel" : "looks");
        if (old) {
            xtime = old;
            if (type == SICK_VOMITABLE)
                xtime--;
            else
                xtime /= 3;
            if (xtime < 1)
                xtime = 1;
        }

        set_property(mon, SICK, xtime, TRUE);
        if (you) {
            exercise(A_CON, FALSE);
            u.usick_type |= type;
            set_delayed_killer(POISONING,
                               cause ? killer_msg(POISONING, cause) : NULL);
        } else if (!flags.mon_moving) /* you made the monster sick */
            mon->usicked = 1;
    } else if ((old || zombifying(mon)) &&
               (!you || (type & u.usick_type) ||
                zombifying(mon))) {
        /* TODO: usick_type equavilent for monsters */
        if (zombifying(mon)) {
            set_property(mon, ZOMBIE, -2, TRUE);
            if (you || vis)
                pline(you ? msgc_statusheal : msgc_monneutral,
                      "%s zombifying disease is cured.",
                      s_suffix(Monnam(mon)));
            if (!sick(mon))
                return;
        }
        /* was sick, now not */
        if (you)
            u.usick_type &= ~type;
        if (you && u.usick_type) { /* only partly cured */
            if (talk && (you || vis))
                pline(you ? msgc_statusheal : msgc_monneutral,
                      "%s %s somewhat better.", you ? "You" : Monnam(mon),
                      you ? "feel" : "looks");
            inc_timeout(mon, SICK, old, TRUE);
        } else
            set_property(mon, SICK, -2, FALSE);
    }
}


static const char eyemsg[] = "Your %s momentarily %s.";

void
eyepline(const char *verb_one_eye, const char *verb_two_eyes)
{
    if (eyecount(youmonst.data) == 1)
        pline(msgc_playerimmune, eyemsg, body_part(EYE), verb_one_eye);
    else
        pline(msgc_playerimmune, eyemsg, makeplural(body_part(EYE)),
              verb_two_eyes);
}

void
ghost_from_bottle(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    struct monst *mtmp;
    enum msg_channel msgc = msgc_monneutral;

    if (!(mtmp = makemon(&mons[PM_GHOST], level, m_mx(mon), m_my(mon),
                         MM_ADJACENTOK))) {
        if (you)
            msgc = msgc_noconsequence;
        if (you || vis)
            pline(msgc, "It turns out to be empty.");
        return;
    }

    const char *ghost;
    ghost = a_monnam(mtmp);
    if (you && Blind)
        ghost = "something";
    else if (!Hallucination)
        ghost = "an enormous ghost";

    if (you || vis)
        pline(msgc_substitute, "As %s the bottle, %s emerges%s",
              m_verbs(mon, "open"), ghost, blind(mon) ? "!" : ".");

    /* only scare the monster if it can actually see the ghost */
    if (blind(mon))
        return;

    if (you)
        msgc = msgc_statusbad;

    if (you || vis)
        pline_implied(msgc, "%s frightened to death, and unable to move.",
                      m_verbs(mon, "are"));

    if (you)
        helpless(3, hr_afraid, "being frightened to death",
                 "You regain your composure.");
    else {
        mon->mcanmove = 0;
        mon->mfrozen += 3;
    }
}


void
djinni_from_bottle(struct monst *mon, struct obj *obj)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    struct monst *mtmp;
    enum msg_channel msgc = msgc_monneutral;

    if (!(mtmp = makemon(&mons[PM_DJINNI], level, m_mx(mon), m_my(mon),
                         MM_ADJACENTOK))) {
        if (you)
            msgc = msgc_noconsequence;
        if (you || vis)
            pline(msgc, "It turns out to be empty.");
        return;
    }

    /* only hear the djinni if there is clear LOE to the monster... */
    boolean speak = FALSE;
    if (you || couldsee(mon->mx, mon->my)) {
        if (you)
            msgc = msgc_occstart;
        if (!blind(&youmonst))
            pline_implied(msgc, "In a cloud of smoke, %s emerges!",
                          a_monnam(mtmp));
        else
            pline_implied(msgc, "You smell acrid fumes.");
        if (canhear()) {
            speak = TRUE;
            pline_implied(msgc_npcvoice, "%s speaks.",
                          blind(&youmonst) ? "Something" : Monnam(mtmp));
        }
    }

    int dieroll;
    /* The wish has the largest balance implications; use wish_available to
       balance it against other wish sources with the same probability; also
       find out what the dieroll was so that the non-wish results will be
       consistent if the only 80%/20%/5% wish sources so far were djinn.
       If you wasn't the one drinking the potion, perform rng checks on
       rng_main instead */
    if ((you && wish_available(obj->blessed ? 80 : obj->cursed ? 5 : 20, &dieroll)) ||
        (!you &&
         ((dieroll = rn2(100)) < (obj->blessed ? 80 : obj->cursed ? 5 : 20)))) {
        if (you)
            msgc = msgc_intrgain;
        msethostility(mtmp, FALSE, TRUE); /* show as peaceful while wishing */
        /* TODO: consider something different than monneutral for monster wishing */
        if (speak)
            verbalize(msgc, "I am in your debt.  I will grant one wish!");
        if (you)
            makewish();
        else
            mon_makewish(mon);
        mongone(mtmp);
        return;
    }

    /* otherwise, typically an equal chance of each other result, although
       for a cursed potion, we have a hostile djinni with an 80% chance */
    if (obj->cursed && dieroll >= 20)
        dieroll = 0;

    /* avoid failrandom here; that may have been spammed beforehand */
    if (you) {
        msgc = msgc_actionok;
        if (!(dieroll % 4))
            msgc = msgc_substitute;
    }

    switch (dieroll % 4) {
    case 0:
        if (speak)
            verbalize(msgc, "You disturbed me, fool!");
        mtmp->mpeaceful = (you ? 0 : !mon->mpeaceful);
        /* TODO: allow monster specific grudges */
        if (!you && !mon->mpeaceful)
            tamedog(mtmp, NULL);
        break;
    case 1:
        if (speak)
            verbalize(msgc, "Thank you for freeing me!");
        mtmp->mpeaceful = (you || mon->mpeaceful);
        if (you || mon->mtame)
            tamedog(mtmp, NULL);
        break;
    case 2:
        if (speak)
            verbalize(msgc, "You freed me!");
        msethostility(mtmp, FALSE, TRUE);
        break;
    case 3:
        if (speak)
            verbalize(msgc, "It is about time!");
        if (you || canseemon(mtmp))
            pline(msgc_actionok, "%s vanishes.", Monnam(mtmp));
        mongone(mtmp);
        break;
    }
}


/* "Quaffing is like drinking, except you spill more." -- Terry Pratchett */
int
dodrink(const struct nh_cmd_arg *arg)
{
    struct obj *potion;
    const char *potion_descr;
    void (*terrain) (void) = 0;

    if (strangled(&youmonst)) {
        pline(msgc_cancelled,
              "If you can't breathe air, how can you drink liquid?");
        return 0;
    }
    /* Is there a fountain to drink from here? */
    if (IS_FOUNTAIN(level->locations[u.ux][u.uy].typ) && !Engulfed &&
        !Levitation) {
        terrain = drinkfountain;
    }
    /* Or a kitchen sink? */
    if (IS_SINK(level->locations[u.ux][u.uy].typ) && !Engulfed) {
        terrain = drinksink;
    }

    /* Or are you surrounded by water? */
    if (Underwater && !Engulfed) {
        if (yn("Drink the water around you?") == 'y') {
            pline(msgc_cancelled1, "Do you know what lives in this water?!");
            return 1;
        }
    }

    potion = getargobj(arg, terrain ? beverages_and_fountains : beverages,
                       "drink");
    if (!potion)
        return 0;

    if (potion == &zeroobj) {
        if (!terrain)
            return 0;   /* sanity; should be impossible */
        terrain();
        return 1;
    }

    potion->in_use = TRUE;      /* you've opened the stopper */

#define POTION_OCCUPANT_CHANCE(n) (13 + 2*(n))  /* also in muse.c */

    potion_descr = OBJ_DESCR(objects[potion->otyp]);
    if (potion_descr) {
        if (!strcmp(potion_descr, "milky") &&
            !(mvitals[PM_GHOST].mvflags & G_GONE) &&
            !rn2(POTION_OCCUPANT_CHANCE(mvitals[PM_GHOST].born))) {
            ghost_from_bottle(&youmonst);
            useup(potion);
            return 1;
        } else if (!strcmp(potion_descr, "smoky") &&
                   !(mvitals[PM_DJINNI].mvflags & G_GONE) &&
                   !rn2_on_rng(POTION_OCCUPANT_CHANCE(mvitals[PM_DJINNI].born),
                               rng_smoky_potion)) {
            djinni_from_bottle(&youmonst, potion);
            useup(potion);
            return 1;
        }
    }
    return dopotion(potion);
}


int
dopotion(struct obj *otmp)
{
    int retval;

    if (otmp == uwep) {
        /* Unwield the potion to avoid a crash if its effect causes the player
           to drop it. We don't print a message here; setuwep doesn't either. */
        setuwep(0);
    }

    otmp->in_use = TRUE;
    int nothing = 0;
    int unkn = 0;
    if ((retval = peffects(&youmonst, otmp, &nothing, &unkn)) >= 0)
        return retval;

    if (nothing) {
        unkn++;
        pline(msgc_nospoil,
              "You have a %s feeling for a moment, then it passes.",
              Hallucination ? "normal" : "peculiar");
    }
    if (otmp->dknown && !objects[otmp->otyp].oc_name_known) {
        if (!unkn) {
            makeknown(otmp->otyp);
            more_experienced(0, 10);
        } else if (!objects[otmp->otyp].oc_uname)
            docall(otmp);
    }
    useup(otmp);
    return 1;
}


int
peffects(struct monst *mon, struct obj *otmp, int *nothing, int *unkn)
{
    int i, ii, lim;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    int dmg = 0;
    int heal = 0;
    int healmax = 0;
    /* For nutrition */
    struct edog *edog;
    edog = (!you && mon->mtame && !isminion(mon)) ?
        mx_edog(mon) : NULL;
    enum youprop prop;
    aligntyp alignment;
    /* Dynamic strings based on whether it was you or the monster */
    const char *Mon;
    Mon = you ? "You" : Monnam(mon);
    const char *Mons;
    Mons = you ? "Your" : s_suffix(Monnam(mon));
    const char *looks;
    looks = you ? "feel" : "looks";
    const char *his;
    his = you ? "your" : mhis(mon);
    /* and for msgchannels */
    enum msg_channel statusheal = you ? msgc_statusheal : msgc_monneutral;
    enum msg_channel statusbad = you ? msgc_statusbad : msgc_monneutral;
    enum msg_channel badidea = you ? msgc_badidea : msgc_monneutral;

    alignment = malign(mon);

    switch (otmp->otyp) {
    case POT_RESTORE_ABILITY:
    case SPE_RESTORE_ABILITY:
        if (otmp->cursed) {
            if (you)
                pline(statusbad,
                      "Ulch!  This makes you feel mediocre!");
            else if (vis)
                pline(msgc_monneutral,
                      "%s looks strangely mediocre.", Monnam(mon));
            break;
        } else {
            if (!you) {
                /* Raise max HP depending on how far from current level's HP cap the monster is
                   If the potion is uncursed, only do a single raise.
                   Equavilent to what "drain strength" does against monsters (but in reverse).
                */
                if (!vis)
                    *unkn = 1;
                else
                    pline(msgc_monneutral,
                          "%s looks %s!", Monnam(mon), otmp->blessed ? "great" : "better");
                if (mon->mhpmax > (mon->m_lev * 8 - 8))
                    break;
                int raised_maxhp = 0;
                for (i = 0; i < (mon->m_lev - (mon->mhpmax / 8 - 1)); i++) {
                    raised_maxhp += rnd(8);
                    if (!otmp->blessed)
                        break;
                }
                if (raised_maxhp) {
                    mon->mhpmax += raised_maxhp;
                    mon->mhp += raised_maxhp;
                }
                break;
            }
            pline(statusheal, "Wow!  This makes you feel %s!",
                  (otmp->blessed) ? (unfixable_trouble_count(FALSE) ? "better" :
                                     "great")
                  : "good");
            i = rn2(A_MAX);     /* start at a random point */
            for (ii = 0; ii < A_MAX; ii++) {
                lim = AMAX(i);
                if (i == A_STR && u.uhs >= 3)
                    --lim;      /* WEAK */
                if (ABASE(i) < lim) {
                    ABASE(i) = lim;
                    /* only first found if not blessed */
                    if (!otmp->blessed)
                        break;
                }
                if (++i >= A_MAX)
                    i = 0;
            }
        }
        break;
    case POT_HALLUCINATION:
        /* Hallu doesn't exist for monsters, but black lights cause monster confusion so...
           Also, should these things really last for 600-1100 (!) turns? */
        prop = you ? HALLUC : CONFUSION;
        /* we can't simply use set_property's return var, because monster hallu -> confusion, thus
           we need to see if the monster had the property before by hand, and only then, give a message */
        if (!has_property(mon, prop)) {
            if (you && !resists_hallu(mon))
                pline(msgc_statusbad, "Oh wow!  Great stuff!");
            else if (vis)
                pline(msgc_monneutral, "%s looks cross-eyed!", Monnam(mon));
        } else {
            *unkn = 1;
            *nothing = 1;
        }
        set_property(mon, prop, rn1(200, 600 - 300 * bcsign(otmp)), TRUE);
        break;
    case POT_WATER:
        if (!otmp->blessed && !otmp->cursed) {
            if (you)
                pline(msgc_actionok, "This tastes like water.");
            else
                *unkn = 1;
            if (you) {
                u.uhunger += rnd(10);
                newuhs(FALSE);
            } else if (edog)
                dog_eat(mon, otmp, mon->mx, mon->my, FALSE);
            break;
        }
        if (!you && !vis)
            *unkn = 1;
        if (is_undead(mon->data) || is_demon(mon->data) ||
            alignment == A_CHAOTIC) {
            if (otmp->blessed) {
                if (you) {
                    pline(badidea, "This burns like acid!");
                    exercise(A_CON, FALSE);
                } else if (vis) {
                    pline(badidea, "%s %s in pain!", Monnam(mon),
                          is_silent(mon->data) ? "writhes" : "shrieks");
                    if (!is_silent(mon->data))
                        mwake_nearby(mon, FALSE);
                }
                if ((you && u.ulycn >= LOW_PM) || is_were(mon->data)) {
                    if (you || vis)
                        pline(statusheal, "%s affinity to %s disappears!", Mons,
                              makeplural(you ? mons[u.ulycn].mname : mon_nam(mon)));
                    mon_unwere(mon, TRUE, FALSE);
                }
                if (you)
                    losehp(dice(2, 6), killer_msg(DIED, "a potion of holy water"));
                else {
                    mon->mhp -= dice(2, 6);
                    if (mon->mhp <= 0)
                        mondied(mon);
                }
            } else if (otmp->cursed) {
                if (you || vis)
                    pline(statusheal, "%s %s quite proud of %sself.",
                          Mon, looks, you ? "your" : mhim(mon));
                if (you) {
                    healup(dice(2, 6), 0, 0, 0);
                    exercise(A_CON, TRUE);
                } else
                    mon->mhp += min(mon->mhp + dice(2, 6), mon->mhpmax);
                if ((you && u.ulycn >= LOW_PM && !Upolyd) ||
                    (!you && is_human(mon->data) && is_were(mon->data))) {
                    if (you)
                        you_were();
                    else
                        new_were(mon);
                }
            }
        } else {
            if (otmp->blessed) {
                if (you || vis)
                    pline(statusheal, "%s %s full of awe.", Mon, looks);
                set_property(mon, SICK, -2, FALSE);
                set_property(mon, ZOMBIE, -2, FALSE);
                if (you) {
                    exercise(A_WIS, TRUE);
                    exercise(A_CON, TRUE);
                }
                if (is_were(mon->data))
                    mon_unwere(mon, TRUE, TRUE);
            } else {
                if (alignment == A_LAWFUL) {
                    if (you)
                        pline(badidea, "This burns like acid!");
                    else if (vis)
                        pline(badidea, "%s %s in pain!", Monnam(mon),
                              is_silent(mon->data) ? "writhes" : "shrieks");
                    if (!you && !is_silent(mon->data))
                        mwake_nearby(mon, FALSE);
                    if (you)
                        losehp(dice(2, 6),
                               killer_msg(DIED, "a potion of unholy water"));
                    else {
                        mon->mhp -= dice(2, 6);
                        if (mon->mhp <= 0)
                            mondied(mon);
                    }
                } else if (you)
                    pline(msgc_actionok, "You feel full of dread.");
                else if (vis)
                    pline(msgc_monneutral, "%s looks dreaded.", Monnam(mon));
                if ((you && u.ulycn >= LOW_PM && !Upolyd) ||
                    (!you && is_human(mon->data) && is_were(mon->data))) {
                    if (you)
                        you_were();
                    else
                        new_were(mon);
                }
                if (you)
                    exercise(A_CON, FALSE);
            }
        }
        break;
    case POT_BOOZE:
        if (!you && !vis)
            *unkn = 1;
        if (you)
            pline(statusbad, "Ooph!  This tastes like %s%s!",
                  otmp->odiluted ? "watered down " : "",
                  Hallucination ? "dandelion wine" : "liquid fire");
        else if (vis)
            pline(statusbad, "%s looks less coordinated.", Monnam(mon));
        if (!otmp->blessed)
            set_property(mon, CONFUSION, dice(3, 8), TRUE);
        /* the whiskey makes us feel better */
        if (!otmp->odiluted) {
            if (you)
                healup(1, 0, FALSE, FALSE);
            else
                mon->mhp = min(mon->mhp + 1, mon->mhpmax);
        }
        if (you) {
            u.uhunger += 10 * (2 + bcsign(otmp));
            newuhs(FALSE);
            exercise(A_WIS, FALSE);
        } else if (edog)
            dog_eat(mon, otmp, mon->mx, mon->my, FALSE);
        if (otmp->cursed) {
            if (you || vis)
                pline(statusbad, "%s out!", M_verbs(mon, "pass"));
            if (you) {
                helpless(rnd(15), hr_fainted, "drunk",
                         "You awake with a headache.");
                see_monsters(FALSE);
                see_objects(FALSE);
                turnstate.vision_full_recalc = TRUE;
            } else {
                mon->mcanmove = 0;
                mon->mfrozen += rnd(15);
            }
        }
        break;
    case POT_ENLIGHTENMENT:
        if (otmp->cursed) {
            *unkn = 1;
            if (you) {
                pline(msgc_failcurse, "You have an uneasy feeling.");
                exercise(A_WIS, FALSE);
            } else
                pline(msgc_failcurse, "%s looks uneasy.", Monnam(mon));
        } else {
            if (you && otmp->blessed) {
                adjattrib(A_INT, 1, FALSE);
                adjattrib(A_WIS, 1, FALSE);
            }
            pline(you ? msgc_info : msgc_monneutral,
                  "%s %s self-knowledgeable...", Mon, looks);
            /* no real effect on monsters at the moment */
            if (you) {
                win_pause_output(P_MESSAGE);
                enlightenment(0);
                pline_implied(msgc_info, "The feeling subsides.");
                exercise(A_WIS, TRUE);
            }
        }
        break;
    case SPE_INVISIBILITY:
        /* spell cannot penetrate mummy wrapping */
        if (binvisible(mon)) {
            if (you || vis)
                pline(you ? msgc_failcurse : msgc_monneutral,
                      "%s %s rather itchy under %s %s.", Mon, looks, his,
                      xname(which_armor(mon, os_armc)));
            break;
        }
        /* FALLTHRU */
    case POT_INVISIBILITY:
        if (blind(&youmonst) || (!you && !vis))
            *unkn = 1;
        if (invisible(mon) || (blind(&youmonst) && you))
            *nothing = 1;
        set_property(mon, INVIS, otmp->blessed ? 0 : rn1(15, 31), FALSE);
        newsym(m_mx(mon), m_my(mon));     /* update position */
        if (otmp->cursed) {
            if (you) {
                pline(msgc_levelwarning,
                      "For some reason, you feel your presence is known.");
                aggravate();
            } else
                you_aggravate(mon);
        }
        break;
    case POT_SEE_INVISIBLE:
        /* tastes like fruit juice in Rogue */
    case POT_FRUIT_JUICE:
        {
            *unkn = 1;
            if (otmp->cursed && you)
                pline(msgc_failcurse, "Yecch!  This tastes %s.",
                      Hallucination ? "overripe" : "rotten");
            else if (you)
                pline(msgc_actionok, Hallucination ?
                      "This tastes like 10%% real %s%s all-natural beverage." :
                      "This tastes like %s%s.",
                      otmp->odiluted ? "reconstituted " : "", fruitname(TRUE));
            if (otmp->otyp == POT_FRUIT_JUICE) {
                if (you) {
                    u.uhunger += (otmp->odiluted ? 5 : 10) * (2 + bcsign(otmp));
                    newuhs(FALSE);
                } else if (edog)
                    dog_eat(mon, otmp, mon->mx, mon->my, FALSE);
                break;
            }
            if (!otmp->cursed)
                set_property(mon, BLINDED, -2, FALSE);
            if (set_property(mon, SEE_INVIS, otmp->blessed ? 0 : rn1(100, 750), FALSE) &&
                you)
                *unkn = 0;
            break;
        }
    case POT_PARALYSIS:
        if (free_action(mon)) {
            if (you || vis)
                pline(combat_msgc(NULL, mon, cr_immune),
                      "%s momentarily.", M_verbs(mon, "stiffen"));
            break;
        }
        if (you || vis) {
            if (levitates(mon) || Is_airlevel(m_mz(mon)) || Is_waterlevel(m_mz(mon)))
                pline(statusbad, "%s motionlessly suspended.",
                      M_verbs(mon, "are"));
            else if (you && u.usteed)
                pline(statusbad, "You are frozen in place!");
            else
                pline(statusbad, "%s %s are frozen to the %s!", Mons,
                      makeplural(mbodypart(mon, FOOT)), surface(m_mx(mon), m_my(mon)));
        }
        if (you) {
            helpless(rn1(10, 25 - 12 * bcsign(otmp)), hr_paralyzed,
                     "frozen by a potion", NULL);
            exercise(A_DEX, FALSE);
        } else {
            mon->mcanmove = 0;
            mon->mfrozen += rn1(10, 25 - 12 * bcsign(otmp));
        }
        break;
    case POT_SLEEPING:
        if (resists_sleep(mon) || free_action(mon)) {
            if (you || vis)
                pline(combat_msgc(NULL, mon, cr_immune), "%s", M_verbs(mon, "yawn"));
            break;
        }
        if (you || vis)
            pline(statusbad, "%s suddenly %s asleep!", Mon, you ? "" : "s");
        if (you)
            helpless(rn1(10, 25 - 12 * bcsign(otmp)), hr_asleep, "sleeping",
                     NULL);
        else {
            mon->mcanmove = 0;
            mon->mfrozen += rn1(10, 25 - 12 * bcsign(otmp));
        }
        break;
    case POT_MONSTER_DETECTION:
    case SPE_DETECT_MONSTERS:
        if (otmp->blessed) {
            if (detects_monsters(mon)) {
                *nothing = 1;
                *unkn = 1;
            } else if (!you)
                *unkn = 1;
            /* after a while, repeated uses become less effective */
            if (property_timeout(mon, DETECT_MONSTERS) >= 300)
                i = 1;
            else
                i = rn1(40, 21);
            inc_timeout(mon, DETECT_MONSTERS, i, FALSE);
            break;
        }
        /* TODO: uncursed (or unskilled/basic spell) for monsters */
        if (!you) {
            *unkn = 1;
            if (vis)
                pline(msgc_monneutral, "%s is granted an insight!", Mon);
        } else if (you && monster_detect(otmp, 0))
            return 1;   /* nothing detected */
        if (you)
            exercise(A_WIS, TRUE);
        break;
    case POT_OBJECT_DETECTION:
    case SPE_DETECT_TREASURE:
        if (!you) {
            *unkn = 1;
            if (vis)
                pline(msgc_monneutral, "%s is granted an insight!", Mon);
        }
        object_detect(otmp, 0);
        if (you)
            exercise(A_WIS, TRUE);
        break;
    case POT_SICKNESS:
        if (mon->data == &mons[PM_PESTILENCE])
            goto pestilence_heal;
        if (you)
            pline(badidea, "Yecch!  This stuff tastes like poison.");
        else if (vis)
            pline(badidea, "%s looks %ssick.", Mon, otmp->blessed ? "slightly" : "");
        if (otmp->blessed) {
            if (you)
                pline(msgc_failcurse, "(But in fact it was mildly stale %s.)",
                      fruitname(TRUE));
            if (!resists_poison(mon)) {
                /* NB: blessed otmp->fromsink is not possible */
                if (you)
                    losehp(1, killer_msg(DIED, "a mildly contaminated potion"));
                else {
                    mon->mhp -= 1;
                    if (mon->mhp <= 0)
                        mondied(mon);
                }
            }
        } else {
            if (you) {
                if (resists_poison(mon))
                    pline(msgc_notresisted,
                          "(But in fact it was biologically contaminated %s.)",
                          fruitname(TRUE));
                if (Role_if(PM_HEALER))
                    pline(msgc_playerimmune,
                          "Fortunately, you have been immunized.");
            }
            if ((you && !Role_if(PM_HEALER)) ||
                monsndx(mon->data) != PM_HEALER) {
                int typ = rn2(A_MAX);
                if (!fixed_abilities(mon)) {
                    if (you) {
                        poisontell(typ);
                        adjattrib(typ, resists_poison(mon) ? -1 : -rn1(4, 3), TRUE);
                    } else {
                        int dmg = resists_poison(mon) ? 1 : rn1(4, 3);
                        dmg = dice(dmg, 8);
                        mon->mhpmax -= dmg;
                        mon->mhp -= dmg;
                        if (mon->mhpmax < 1)
                            mon->mhpmax = 1; /* avoid strange issues if mon gets lifesaved */
                        if (mon->mhp <= 0) {
                            mondied(mon);
                            break;
                        }
                    }
                }
                if (!resists_poison(mon)) {
                    if (you && otmp->fromsink)
                        losehp(rnd(10) + 5 * ! !(otmp->cursed),
                               killer_msg(DIED, "contaminated tap water"));
                    else if (you)
                        losehp(rnd(10) + 5 * ! !(otmp->cursed),
                               killer_msg(DIED, "contaminated potion"));
                    else {
                        mon->mhp -= rnd(10) + 5;
                        if (mon->mhp <= 0)
                            mondied(mon);
                    }
                }
                if (you)
                    exercise(A_CON, FALSE);
            }
        }
        if (hallucinating(mon)) {
            if (you || vis)
                pline(statusheal, "%s shocked back to %s senses!",
                      M_verbs(mon, "are"), you ? "your" : mhis(mon));
            set_property(mon, HALLUC, -2, TRUE);
        }
        break;
    case POT_CONFUSION:
        if (confused(mon))
            *nothing = 1;
        inc_timeout(mon, CONFUSION, rn1(7, 16 - 8 * bcsign(otmp)), FALSE);
        break;
    case POT_GAIN_ABILITY:
        if (otmp->cursed) {
            if (you)
                pline(msgc_failcurse, "Ulch!  That potion tasted foul!");
            *unkn = 1;
            break;
        } else if (fixed_abilities(mon)) {
            *nothing = 1;
            break;
        } else if (you) { /* If blessed, increase all; if not, try up to */
            int itmp;     /* 6 times to find one which can be increased. */

            i = -1;     /* increment to 0 */
            for (ii = A_MAX; ii > 0; ii--) {
                i = (otmp->blessed ? i + 1 : rn2(A_MAX));
                /* only give "your X is already as high as it can get" message
                   on last attempt (except blessed potions) */
                itmp = (otmp->blessed || ii == 1) ? 0 : -1;
                if (adjattrib(i, 1, itmp) && !otmp->blessed)
                    break;
            }
            break;
        }
        /* FALLTHROUGH for monsters -- make gain level/ability equavilent for
           noncursed */
    case POT_GAIN_LEVEL:
        if (otmp->cursed) {
            *unkn = 1;
            /* they went up a level */
            if ((ledger_no(m_mz(mon)) == 1 && mon_has_amulet(mon)) ||
                Can_rise_up(m_mx(mon), m_my(mon), m_mz(mon))) {
                if (you || vis)
                    pline(msgc_substitute, "%s rise%s up, through the %s!",
                          Mon, you ? "" : "s",
                          ceiling(m_mx(mon), m_my(mon)));
                if (ledger_no(m_mz(mon)) == 1) {
                    if (you)
                        goto_level(&earth_level, FALSE, FALSE, FALSE);
                    else { /* ouch... */
                        migrate_to_level(mon, ledger_no(&earth_level), MIGR_NEAR_PLAYER, NULL);
                        if (vis) {
                            pline(msgc_outrobad, "Congratulations, %s!",
                                  mortal_or_creature(mon->data, TRUE));
                            pline(msgc_outrobad, "But now thou must face the final Test...");
                        }
                        pline(msgc_outrobad, "%s managed to enter the Planes with the Amulet...",
                              Monnam(mon));
                        pline(msgc_outrobad,
                              "You feel a sense of despair as you realize that all is lost.");
                        const char *ebuf;
                        ebuf = msgprintf("lost the Amulet as %s entered the Planes with it",
                                         k_monnam(mon));
                        done(ESCAPED, ebuf);
                    }
                } else {
                    int newlev = depth(m_mz(mon)) - 1;
                    d_level newlevel;

                    get_level(&newlevel, newlev);
                    if (on_level(&newlevel, m_mz(mon))) {
                        impossible("can rise up, but invalid newlevel?");
                        break;
                    }
                    if (you)
                        goto_level(&newlevel, FALSE, FALSE, FALSE);
                    else
                        migrate_to_level(mon, ledger_no(&newlevel), MIGR_RANDOM, NULL);
                }
            } else if (you)
                pline(msgc_yafm, "You have an uneasy feeling.");
            else if (vis)
                pline(msgc_monneutral, "%s looks uneasy.", Monnam(mon));
            break;
        }
        if (you) {
            pluslvl(FALSE);
            if (otmp->blessed)
                /* blessed potions place you at a random spot in the middle of the
                   new level instead of the low point */
                u.uexp = rndexp(TRUE);
        } else {
            if (!vis)
                *unkn = 1;
            else {
                if (otmp->otyp == POT_GAIN_LEVEL)
                    pline(msgc_monneutral, "%s seems more experienced.", Mon);
                else
                    pline(msgc_monneutral, "%s abilities looks improved.", Mons);
            }
            if (!grow_up(mon, NULL))
                return *unkn;
        }
        break;
    case POT_SPEED:
        if (leg_hurt(mon) && !otmp->cursed) {
            heal_legs(mon, leg_hurtsides(mon));
            break;
        }       /* and fall through */
    case SPE_HASTE_SELF:
        if (you)
            exercise(A_DEX, TRUE);
        set_property(mon, FAST, rn1(10, 100 + 60 * bcsign(otmp)), FALSE);
        if (otmp->blessed)
            set_property(mon, FAST, 0, FALSE);
        break;
    case POT_BLINDNESS:
        if (blind(mon))
            *nothing = 1;
        set_property(mon, BLINDED, rn1(200, 250 - 125 * bcsign(otmp)), FALSE);
        break;
    case POT_FULL_HEALING:
    pestilence_heal:
        heal = 400;
        healmax = otmp->blessed ? 8 : 4;
        if (you || vis)
            pline(statusheal, "%s %s fully healed", Mon, looks);
        /* Increase level if you lost some/many */
        if (you && otmp->blessed && u.ulevel < u.ulevelmax)
            pluslvl(FALSE);
        /* fallthrough */
    case POT_EXTRA_HEALING:
        if (!heal) {
            heal = dice(6 + 2 * bcsign(otmp), 8);
            healmax = otmp->blessed ? 5 : 2;
            if (you || vis)
                pline(statusheal, "%s %s much better", Mon, looks);
        }
        set_property(mon, HALLUC, -2, FALSE);
        if (you)
            exercise(A_STR, TRUE);
    case POT_HEALING:
        if (!heal) {
            heal = dice(6 + 2 * bcsign(otmp), 4);
            healmax = 1;
            if (you || vis)
                pline(statusheal, "%s %s better.", Mon, looks);
        }
        /* cure blindness for EH/FH or noncursed H */
        if (healmax > 1 || !otmp->cursed)
            set_property(mon, BLINDED, -2, FALSE);
        /* cure sickness for noncursed EX/FH or blessed H */
        if (otmp->blessed || (healmax > 1 && !otmp->cursed)) {
            set_property(mon, SICK, -2, FALSE);
            set_property(mon, ZOMBIE, -2, FALSE);
        }
        /* cursed potions give no max HP gains */
        if (otmp->cursed)
            healmax = 0;
        if (you) {
            healup(heal, healmax, FALSE, FALSE);
            exercise(A_CON, TRUE);
        } else {
            mon->mhp += heal;
            if (mon->mhp >= mon->mhpmax) {
                if (otmp->blessed)
                    mon->mhpmax += healmax;
                mon->mhp = mon->mhpmax;
            }
        }
        break;
    case POT_LEVITATION:
    case SPE_LEVITATION:
        if (levitates(mon) && !otmp->cursed)
            *nothing = 1;
        if (otmp->cursed) /* convert controlled->uncontrolled levi */
            /* TRUE to avoid float_down() */
            set_property(mon, LEVITATION, -1,
                         levitates(mon) & ~FROMOUTSIDE ? TRUE : FALSE);
        if (otmp->blessed) {
            set_property(mon, LEVITATION, 0, FALSE);
            set_property(mon, LEVITATION, rn1(50, 250), FALSE);
        } else
            set_property(mon, LEVITATION, rn1(140, 10), FALSE);
        if (otmp->cursed && !Is_waterlevel(m_mz(mon))) {
            if ((m_mx(mon) != level->upstair.sx || m_my(mon) != level->upstair.sy) &&
                (m_mx(mon) != level->sstairs.sx ||
                 m_my(mon) != level->sstairs.sy || !level->sstairs.up) &&
                (m_mx(mon) != level->upladder.sx ||
                 m_my(mon) != level->upladder.sy)) {
                if (you || vis)
                    pline(you ? msgc_nonmonbad : msgc_monneutral,
                          "%s %s %s on the %s.", M_verbs(mon, "hit"), his,
                          mbodypart(mon, HEAD), ceiling(m_mx(mon), m_my(mon)));
                dmg = which_armor(mon, os_armh) ? 1 : rnd(10);
                if (you)
                    losehp(dmg, killer_msg(DIED, "colliding with the ceiling"));
                else {
                    mon->mhp -= dmg;
                    if (mon->mhp <= 0)
                        mondied(mon);
                }
            } else if (you)
                doup();
        }
        if (you)
            spoteffects(FALSE); /* for sinks */
        break;
    case POT_GAIN_ENERGY:      /* M. Stephenson */
        if (otmp->cursed) {
            if (you || vis)
                pline(msgc_failcurse, "%s %s lackluster.", Mon, looks);
        } else {
            if (you || vis)
                pline(statusheal, "Magical energies course through %s body.",
                      you ? "your" : s_suffix(mon_nam(mon)));
            set_property(mon, CANCELLED, -2, FALSE);
        }
        int num;
        num = rnd(5) + 5 * otmp->blessed + 1;
        mon->pwmax += (otmp->cursed) ? -num : num;
        mon->pw += (otmp->cursed) ? -num : num;
        if (mon->pwmax <= 0)
            mon->pwmax = 0;
        if (mon->pw <= 0)
            mon->pw = 0;
        if (you)
            exercise(A_WIS, otmp->cursed ? FALSE : TRUE);
        if (otmp->cursed)
            mon->mspec_used += num * 5;
        else
            mon->mspec_used = 0;
        break;
    case POT_OIL:      /* P. Winner */
        {
            boolean good_for_you = FALSE;

            if (otmp->lamplit) {
                if (likes_fire(mon->data)) {
                    if (you)
                        pline(msgc_yafm, "Ahh, a refreshing drink.");
                    else if (vis)
                        pline(msgc_yafm, "%s looks refreshed.", Monnam(mon));
                    good_for_you = TRUE;
                } else {
                    if (you || vis)
                        pline(badidea, "%s %s %s.", M_verbs(mon, "burn"), his,
                              mbodypart(mon, FACE));
                    dmg = dice(resists_fire(mon) ? 1 : 3, 4);
                    if (you)
                        losehp(dmg, killer_msg(DIED, "a burning potion of oil"));
                    else {
                        mon->mhp -= dmg;
                        if (mon->mhp <= 0)
                            mondied(mon);
                    }
                }
            } else if (you) {
                if (otmp->cursed)
                    pline(badidea, "This tastes like castor oil.");
                else
                    pline(badidea, "That was smooth!");
            } else
                *unkn = 1;
            if (you)
                exercise(A_WIS, good_for_you);
        }
        break;
    case POT_ACID:
        if (resists_acid(mon)) {
            if (you)
                /* Not necessarily a creature who _likes_ acid */
                pline(msgc_playerimmune,
                      "This tastes %s.", Hallucination ? "tangy" : "sour");
        } else {
            if (you)
                pline(badidea, "This burns%s!",
                      otmp->blessed ? " a little" : otmp->
                      cursed ? " a lot" : " like acid");
            else if (vis)
                pline(badidea, "%s %s in pain!", Monnam(mon),
                      is_silent(mon->data) ? "writhes" : "shrieks");
            if (!you && !is_silent(mon->data))
                mwake_nearby(mon, FALSE);
            dmg = dice(otmp->cursed ? 2 : 1, otmp->blessed ? 4 : 8);
            if (you) {
                losehp(dmg, killer_msg(DIED, "drinking acid"));
                exercise(A_CON, FALSE);
            } else {
                mon->mhp -= dmg;
                if (mon->mhp <= 0)
                    mondied(mon);
            }
        }
        if (!set_property(mon, STONED, -2, FALSE))
            *unkn = 1; /* holy/unholy water can burn like acid too */
        break;
    case POT_POLYMORPH:
        if (you || vis) {
            if (you)
                pline(unchanging(mon) ? msgc_failcurse : msgc_actionok,
                      "You feel a little %s.", Hallucination ? "normal" : "strange");
            else if (vis && !unchanging(mon))
                pline(msgc_monneutral, "%s suddenly mutates!", Mon);
            else if (vis)
                pline(msgc_monneutral, "%s looks a little %s.", Mon,
                      hallucinating(&youmonst) ? "normal" : "strange");
        }
        if (!unchanging(mon)) {
            if (you)
                polyself(FALSE);
            else
                newcham(mon, NULL, FALSE, FALSE);
        }
        break;
    case POT_WONDER:
        if (you)
            pline(msgc_actionok, "You feel a little %s...",
                  Hallucination ? "normal" : "strange");
        else if (vis)
            pline(msgc_monneutral, "%s a little %s...",
                  M_verbs(mon, "look"),
                  Hallucination ? "normal" : "strange");

        int intrinsic = allowed_wonder[rn2(allowed_wonder_size)];
        if (otmp->cursed)
            set_property(mon, intrinsic, -1, FALSE);
        else if (otmp->blessed)
            set_property(mon, intrinsic, 0, FALSE);
        else
            inc_timeout(mon, intrinsic, 2000, FALSE);
        break;
    default:
        impossible("What a funny potion! (%u)", otmp->otyp);
        return 0;
    }
    return -1;
}

void
healup(int nhp, int nxtra, boolean curesick, boolean cureblind)
{
    if (nhp) {
        if (Upolyd) {
            u.mh += nhp;
            if (u.mh > u.mhmax)
                u.mh = (u.mhmax += nxtra);
        } else {
            u.uhp += nhp;
            if (u.uhp > u.uhpmax)
                u.uhp = (u.uhpmax += nxtra);
        }
    }
    if (cureblind)
        set_property(&youmonst, BLINDED, -2, FALSE);
    if (curesick)
        make_sick(&youmonst, 0L, NULL, TRUE, SICK_ALL);
    return;
}

void
strange_feeling(struct obj *obj, const char *txt)
{
    if (flags.beginner || !txt)
        pline(msgc_nospoil,
              "You have a %s feeling for a moment, then it passes.",
              Hallucination ? "normal" : "strange");
    else
        pline(msgc_hint, "%s", txt);

    if (!obj)   /* e.g., crystal ball finds no traps */
        return;

    if (obj->dknown && !objects[obj->otyp].oc_name_known &&
        !objects[obj->otyp].oc_uname)
        docall(obj);
    useup(obj);
}

static const char *const bottlenames[] = {
    "bottle", "phial", "flagon", "carafe", "flask", "jar", "vial"
};


const char *
bottlename(void)
{
    return bottlenames[rn2(SIZE(bottlenames))];
}

/* magr = potion thrower; &youmonst for you; NULL for unknown/not a monster */
void
potionhit(struct monst *mon, struct obj *obj, struct monst *magr)
{
    const char *botlnam = bottlename();
    boolean isyou = (mon == &youmonst);
    int distance;
    boolean lamplit;

    /* Remove any timers attached to the object (lit potion of oil). */
    lamplit = (obj->otyp == POT_OIL && obj->lamplit);
    if (obj->timed)
        obj_stop_timers(obj);

    if (isyou) {
        distance = 0;
        pline(combat_msgc(magr, mon, cr_hit),
              "The %s crashes on your %s and breaks into shards.", botlnam,
              body_part(HEAD));
        losehp(rnd(2), killer_msg(DIED, "a thrown potion"));
    } else {
        distance = distu(mon->mx, mon->my);
        if (!cansee(mon->mx, mon->my))
            pline(msgc_levelsound, "Crash!");
        else {
            const char *mnam = mon_nam(mon);
            const char *buf;

            if (has_head(mon->data))
                buf = msgprintf("%s %s", s_suffix(mnam),
                                (notonhead ? "body" : "head"));
            else
                buf = mnam;

            pline(combat_msgc(magr, mon, cr_hit),
                  "The %s crashes on %s and breaks into shards.", botlnam, buf);
        }
        if (rn2(5) && mon->mhp >= 2)
            mon->mhp--;
    }

    /* oil doesn't instantly evaporate */
    if (obj->otyp != POT_OIL && cansee(mon->mx, mon->my))
        pline_implied(msgc_consequence, "%s.", Tobjnam(obj, "evaporate"));

    if (isyou) {
        switch (obj->otyp) {
        case POT_OIL:
            if (lamplit)
                splatter_burning_oil(u.ux, u.uy);
            break;
        case POT_POLYMORPH:
            pline(combat_msgc(magr, mon, cr_hit), "You feel a little %s.",
                  Hallucination ? "normal" : "strange");
            if (!Unchanging && !Antimagic)
                polyself(FALSE);
            break;
        case POT_ACID:
            if (!Acid_resistance) {
                pline(combat_msgc(magr, mon, cr_hit), "This burns%s!",
                      obj->blessed ? " a little" : obj->cursed ? " a lot" : "");
                losehp(dice(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8),
                       killer_msg(DIED, "being doused in acid"));
            }
            break;
        }
    } else {
        boolean angermon = magr == &youmonst;

        switch (obj->otyp) {
        case POT_HEALING:
        case POT_EXTRA_HEALING:
        case POT_FULL_HEALING:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_illness;
         /*FALLTHRU*/ case POT_RESTORE_ABILITY:
        case POT_GAIN_ABILITY:
        do_healing:
            angermon = FALSE;
            if (mon->mhp < mon->mhpmax) {
                mon->mhp = mon->mhpmax;
                if (canseemon(mon))
                    pline(magr == &youmonst ? msgc_actionok : msgc_monneutral,
                          "%s looks sound and hale again.", Monnam(mon));
            }
            break;
        case POT_SICKNESS:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_healing;
            if (dmgtype(mon->data, AD_DISE) ||
                dmgtype(mon->data, AD_PEST) || /* currently impossible */
                resists_poison(mon)) {
                if (canseemon(mon))
                    pline(combat_msgc(magr, mon, cr_immune),
                          "%s looks unharmed.", Monnam(mon));
                break;
            }
        do_illness:
            if ((mon->mhpmax > 3) && !resist(magr, mon, POTION_CLASS, NOTELL, 0))
                mon->mhpmax /= 2;
            if ((mon->mhp > 2) && !resist(magr, mon, POTION_CLASS, NOTELL, 0))
                mon->mhp /= 2;
            if (mon->mhp > mon->mhpmax)
                mon->mhp = mon->mhpmax;
            if (canseemon(mon))
                pline(combat_msgc(magr, mon, cr_hit),
                      "%s looks rather ill.", Monnam(mon));
            break;
        case POT_CONFUSION:
        case POT_BOOZE:
            if (!resist(magr, mon, POTION_CLASS, NOTELL, 0))
                set_property(mon, CONFUSION, dice(3, 8), FALSE);
            break;
        case POT_INVISIBILITY:
            angermon = FALSE;
            set_property(mon, INVIS, rn1(15, 31), FALSE);
            break;
        case POT_SLEEPING:
            /* wakeup() doesn't rouse victims of temporary sleep */
            if (sleep_monst(magr, mon, rnd(12), POTION_CLASS)) {
                pline(combat_msgc(magr, mon, cr_hit),
                      "%s falls asleep.", Monnam(mon));
                slept_monst(mon);
            }
            break;
        case POT_PARALYSIS:
            if (mon->mcanmove) {
                mon->mcanmove = 0;
                /* really should be rnd(5) for consistency with players
                   breathing potions, but... */
                mon->mfrozen = rnd(25);
            }
            break;
        case POT_SPEED:
            angermon = FALSE;
            set_property(mon, FAST, rn1(10, 100 + 60 * bcsign(obj)), FALSE);
            break;
        case POT_BLINDNESS:
            if (haseyes(mon->data)) {
                int btmp = 64 + rn2(32) + rn2(32) *
                    !resist(magr, mon, POTION_CLASS, NOTELL, 0);

                if (btmp)
                    set_property(mon, BLINDED, btmp, FALSE);
            }
            break;
        case POT_WATER:
            if (is_undead(mon->data) || is_demon(mon->data) ||
                is_were(mon->data)) {
                if (obj->blessed) {
                    if (canseemon(magr))
                        pline(combat_msgc(magr, mon, cr_hit),
                              "%s %s in pain!", Monnam(mon),
                              is_silent(mon->data) ? "writhes" : "shrieks");
                    if (!is_silent(mon->data))
                        aggravate();
                    mon->mhp -= dice(2, 6);
                    /* should only be by you */
                    if (mon->mhp <= 0)
                        killed(mon);
                    else if (is_were(mon->data) && !is_human(mon->data))
                        new_were(mon);  /* revert to human */
                } else if (obj->cursed) {
                    angermon = FALSE;
                    if (canseemon(mon))
                        pline(magr == &youmonst ?
                              msgc_actionok : msgc_monneutral,
                              "%s looks healthier.", Monnam(mon));
                    mon->mhp += dice(2, 6);
                    if (mon->mhp > mon->mhpmax)
                        mon->mhp = mon->mhpmax;
                    if (is_were(mon->data) && is_human(mon->data) &&
                        !Protection_from_shape_changers)
                        new_were(mon);  /* transform into beast */
                }
            } else if (mon->data == &mons[PM_GREMLIN]) {
                angermon = FALSE;
                split_mon(mon, NULL);
            } else if (mon->data == &mons[PM_IRON_GOLEM]) {
                if (canseemon(mon))
                    pline(combat_msgc(magr, mon, cr_hit), "%s rusts.",
                          Monnam(mon));
                mon->mhp -= dice(1, 6);
                /* should only be by you */
                if (mon->mhp <= 0)
                    killed(mon);
            }
            break;
        case POT_OIL:
            if (lamplit)
                splatter_burning_oil(mon->mx, mon->my);
            break;
        case POT_ACID:
            if (!resists_acid(mon) && !resist(magr, mon, POTION_CLASS, NOTELL, 0)) {
                pline(combat_msgc(magr, mon, cr_hit),
                      "%s %s in pain!", Monnam(mon),
                      is_silent(mon->data) ? "writhes" : "shrieks");
                if (!is_silent(mon->data))
                    mwake_nearby(mon, FALSE);
                mon->mhp -= dice(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
                if (mon->mhp <= 0) {
                    if (magr == &youmonst)
                        killed(mon);
                    else
                        monkilled(magr, mon, "", AD_ACID);
                }
            }
            break;
        case POT_POLYMORPH:
            bhitm(&youmonst, mon, obj, 7);
            break;
/*
        case POT_GAIN_LEVEL:
        case POT_LEVITATION:
        case POT_FRUIT_JUICE:
        case POT_MONSTER_DETECTION:
        case POT_OBJECT_DETECTION:
            break;
*/
        }
        if (angermon)
            wakeup(mon, FALSE);
        else
            mon->msleeping = 0;
    }

    /* Note: potionbreathe() does its own docall() */
    if ((distance == 0 || ((distance < 3) && rn2(5))) &&
        (!breathless(youmonst.data) || haseyes(youmonst.data)))
        potionbreathe(&youmonst, obj);
    else if (obj->dknown && !objects[obj->otyp].oc_name_known &&
             !objects[obj->otyp].oc_uname && cansee(mon->mx, mon->my))
        docall(obj);
    if (*u.ushops && obj->unpaid) {
        struct monst *shkp =
            shop_keeper(level, *in_rooms(level, u.ux, u.uy, SHOPBASE));

        if (!shkp)
            obj->unpaid = 0;
        else {
            stolen_value(obj, u.ux, u.uy, (boolean) shkp->mpeaceful, FALSE);
            subfrombill(obj, shkp);
        }
    }
    obfree(obj, NULL);
}


/* vapors are inhaled or get in your eyes */
void
potionbreathe(struct monst *mon, struct obj *obj)
{
    boolean vis = canseemon(mon);
    boolean you = (mon == &youmonst);
    boolean pestilence = (!you && mon->data == &mons[PM_PESTILENCE]);
    int i, ii, isdone, kn = 0;

    switch (obj->otyp) {
    case POT_RESTORE_ABILITY:
    case POT_GAIN_ABILITY:
        if (you && obj->cursed) {
            if (!breathless(youmonst.data))
                pline(msgc_noconsequence,
                      "Ulch!  That potion smells terrible!");
            else if (haseyes(youmonst.data)) {
                int numeyes = eyecount(youmonst.data);

                pline(msgc_noconsequence, "Your %s sting%s!",
                      (numeyes == 1) ? body_part(EYE) :
                      makeplural(body_part(EYE)),
                      (numeyes == 1) ? "s" : "");
            }
            break;
        } else if (!obj->cursed) {
            if (you) {
                i = rn2(A_MAX);     /* start at a random point */
                for (isdone = ii = 0; !isdone && ii < A_MAX; ii++) {
                    if (ABASE(i) < AMAX(i)) {
                        ABASE(i)++;
                        /* only first found if not blessed */
                        isdone = !(obj->blessed);
                    }
                }
                if (++i >= A_MAX)
                    i = 0;
            } else {
                if (mon->mhp < mon->mhpmax)
                    mon->mhp++;
            }
        }
        break;
    case POT_FULL_HEALING:
        if (you) {
            if (Upolyd && u.mh < u.mhmax)
                u.mh++;
            if (u.uhp < u.uhpmax)
                u.uhp++;
        } else if (!pestilence) {
            if (mon->mhp < mon->mhpmax)
                mon->mhp++;
        } else {
            mon->mhp--;
            if (!mon->mhp)
                mon->mhp = 1;
        }
        /* FALL THROUGH */
    case POT_EXTRA_HEALING:
        if (you) {
            if (Upolyd && u.mh < u.mhmax)
                u.mh++;
            if (u.uhp < u.uhpmax)
                u.uhp++;
        } else if (!pestilence) {
            if (mon->mhp < mon->mhpmax)
                mon->mhp++;
        } else {
            mon->mhp--;
            if (!mon->mhp)
                mon->mhp = 1;
        }
        /* FALL THROUGH */
    case POT_HEALING:
        if (you) {
            if (Upolyd && u.mh < u.mhmax)
                u.mh++;
            if (u.uhp < u.uhpmax)
                u.uhp++;
        } else if (!pestilence) {
            if (mon->mhp < mon->mhpmax)
                mon->mhp++;
        } else {
            mon->mhp--;
            if (!mon->mhp)
                mon->mhp = 1;
        }
        if (you)
            exercise(A_CON, TRUE);
        break;
    case POT_SICKNESS:
        if (!pestilence && mon->data != &mons[PM_HEALER]) {
            if (you) {
                if (Upolyd) {
                    if (u.mh <= 5)
                        u.mh = 1;
                    else
                        u.mh -= 5;
                } else {
                    if (u.uhp <= 5)
                        u.uhp = 1;
                    else
                        u.uhp -= 5;
                }
                exercise(A_CON, FALSE);
            } else {
                if (mon->mhp <= 5)
                    mon->mhp = 1;
                else
                    mon->mhp -= 5;
            }
        } else if (pestilence) {
            /* equavilent to full healing */
            mon->mhp += 3;
            if (mon->mhp > mon->mhpmax)
                mon->mhp = mon->mhpmax;
        }
        break;
    case POT_HALLUCINATION:
        if (you) {
            pline(msgc_noconsequence, "You have a momentary vision.");
            kn++;
        }
        break;
    case POT_CONFUSION:
    case POT_BOOZE:
        inc_timeout(mon, CONFUSION, rnd(5), FALSE);
        break;
    case POT_INVISIBILITY:
        if (!blind(&youmonst) && !invisible(mon) && (you || vis)) {
            kn++;
            if (you)
                pline(msgc_noconsequence, "For an instant you %s!",
                      See_invisible ? "could see right through yourself" :
                      "couldn't see yourself");
            else
                pline(msgc_noconsequence, "%s %s for a moment!", Monnam(mon),
                      see_invisible(&youmonst) ? "turns transparent" : "disappears");
        }
        break;
    case POT_PARALYSIS:
        if (you || vis) {
            kn++;
            if (!free_action(mon)) {
                pline(you ? msgc_statusbad : msgc_monneutral,
                      "Something seems to be holding %s.",
                      you ? "you" : mon_nam(mon));
                if (you) {
                    helpless(5, hr_paralyzed, "frozen by potion vapours", NULL);
                    exercise(A_DEX, FALSE);
                } else {
                    mon->mfrozen += 5;
                    if (mon->mfrozen)
                        mon->mcanmove = 0;
                }
            } else
                pline(combat_msgc(NULL, mon, cr_immune),
                      "%s stiffen%s momentarily.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s");
        }
        break;
    case POT_SLEEPING:
        if (you || vis) {
            kn++;
            if (!free_action(mon) && !resists_sleep(mon)) {
                if (you) {
                    pline(msgc_statusbad, "You feel rather tired.");
                    helpless(5, hr_asleep, "sleeping off potion vapours", NULL);
                    exercise(A_DEX, FALSE);
                } else {
                    pline(msgc_monneutral, "%s falls asleep.", Monnam(mon));
                    mon->mfrozen += 5;
                    if (mon->mfrozen)
                        mon->mcanmove = 0;
                }
            } else
                pline(combat_msgc(NULL, mon, cr_immune), "%s", M_verbs(mon, "yawn"));
        }
        break;
    case POT_SPEED:
        if (you && !fast(mon)) {
            pline(msgc_statusgood, "Your knees seem more flexible now.");
            kn++;
        }
        set_property(mon, FAST, 5, TRUE);
        if (you)
            exercise(A_DEX, TRUE);
        break;
    case POT_BLINDNESS:
        if (you && !blind(mon)) {
            kn++;
            pline(msgc_statusbad, "It suddenly gets dark.");
        }
        set_property(mon, BLINDED, rnd(5), FALSE);
        if (you && !blind(mon))
            pline(msgc_statusheal, "Your vision quickly clears.");
        break;
    case POT_WATER:
        if (mon->data == &mons[PM_GREMLIN]) {
            split_mon(mon, NULL);
        } else if (you && u.ulycn >= LOW_PM) {
            /* vapor from [un]holy water will trigger transformation but won't
               cure lycanthropy */
            if (obj->blessed && youmonst.data == &mons[u.ulycn])
                you_unwere(FALSE);
            else if (obj->cursed && !Upolyd)
                you_were();
        }
        break;
    case POT_ACID:
        /* Acid damage handled elsewhere. */
        if ((you || vis) && !resists_acid(mon))
            kn++;
        if (you)
            exercise(A_CON, FALSE);
        break;
    case POT_POLYMORPH:
        /* Potion of polymorph always gives a unique message, even if
           unchanging. */
        if (you || vis)
            kn++;
        if (you)
            exercise(A_CON, FALSE);
        break;
/*  why are these commented out? -FIQ
    case POT_GAIN_LEVEL:
    case POT_LEVITATION:
    case POT_FRUIT_JUICE:
    case POT_MONSTER_DETECTION:
    case POT_OBJECT_DETECTION:
    case POT_OIL:
        break;
*/
    }
    /* note: no obfree() */
    if (obj->dknown) {
        if (kn)
            makeknown(obj->otyp);
        else if (!objects[obj->otyp].oc_name_known &&
                 !objects[obj->otyp].oc_uname)
            docall(obj);
    }
}


/* returns the potion type when o1 is dipped in o2 */
static short
mixtype(struct obj *o1, struct obj *o2)
{
    /* cut down on the number of cases below */
    if (o1->oclass == POTION_CLASS &&
        (o2->otyp == POT_GAIN_LEVEL || o2->otyp == POT_GAIN_ENERGY ||
         o2->otyp == POT_HEALING || o2->otyp == POT_EXTRA_HEALING ||
         o2->otyp == POT_FULL_HEALING || o2->otyp == POT_ENLIGHTENMENT ||
         o2->otyp == POT_FRUIT_JUICE)) {
        struct obj *swp;

        swp = o1;
        o1 = o2;
        o2 = swp;
    }

    switch (o1->otyp) {
    case POT_HEALING:
        switch (o2->otyp) {
        case POT_SPEED:
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_EXTRA_HEALING;
        }
    case POT_EXTRA_HEALING:
        switch (o2->otyp) {
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_FULL_HEALING;
        }
    case POT_FULL_HEALING:
        switch (o2->otyp) {
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_GAIN_ABILITY;
        }
    case UNICORN_HORN:
        switch (o2->otyp) {
        case POT_SICKNESS:
            return POT_FRUIT_JUICE;
        case POT_HALLUCINATION:
        case POT_BLINDNESS:
        case POT_CONFUSION:
            return POT_WATER;
        }
        break;
    case AMETHYST:     /* "a-methyst" == "not intoxicated" */
        if (o2->otyp == POT_BOOZE)
            return POT_FRUIT_JUICE;
        break;
    case POT_GAIN_LEVEL:
    case POT_GAIN_ENERGY:
        switch (o2->otyp) {
        case POT_CONFUSION:
            return rn2(3) ? POT_BOOZE : POT_ENLIGHTENMENT;
        case POT_HEALING:
            return POT_EXTRA_HEALING;
        case POT_EXTRA_HEALING:
            return POT_FULL_HEALING;
        case POT_FULL_HEALING:
            return POT_GAIN_ABILITY;
        case POT_FRUIT_JUICE:
            return POT_SEE_INVISIBLE;
        case POT_BOOZE:
            return POT_HALLUCINATION;
        }
        break;
    case POT_FRUIT_JUICE:
        switch (o2->otyp) {
        case POT_SICKNESS:
            return POT_SICKNESS;
        case POT_SPEED:
            return POT_BOOZE;
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_SEE_INVISIBLE;
        }
        break;
    case POT_ENLIGHTENMENT:
        switch (o2->otyp) {
        case POT_LEVITATION:
            if (rn2(3))
                return POT_GAIN_LEVEL;
            break;
        case POT_FRUIT_JUICE:
            return POT_BOOZE;
        case POT_BOOZE:
            return POT_CONFUSION;
        }
        break;
    }

    return 0;
}


int
dodip(const struct nh_cmd_arg *arg)
{
    struct obj *obj, *potion;
    struct obj *singlepotion;
    boolean allowfloor = FALSE;
    const char *tmp;
    uchar here;
    char allowall[2] = { ALL_CLASSES, 0 };
    short mixture;
    const char *qbuf, *Your_buf = NULL;

    /* The dip /into/ is taken from arg; the object to dip must therefore
       be propmted for each time.

       TODO: "What do you want to dip into your potions of sickness?" */
    obj = getobj(allowall, "dip", FALSE);
    if (!obj)
        return 0;

    here = level->locations[u.ux][u.uy].typ;
    /* Is there a fountain or pool to dip into here? */
    if ((IS_FOUNTAIN(here) || is_pool(level, u.ux, u.uy)) &&
        !Levitation &&
        !(u.usteed && !swims(u.usteed) &&
          P_SKILL(P_RIDING) < P_BASIC &&
          is_pool(level, u.ux, u.uy)))
        allowfloor = TRUE;

    qbuf = msgprintf("dip %s into",
                     safe_qbuf("", sizeof ("dip  into"), the(xname(obj)),
                               the(simple_typename(obj->otyp)), "this item"));
    potion = getargobj(arg, allowfloor ? beverages_and_fountains :
                       beverages, qbuf);
    if (!potion)
        return 0;

    if (potion == &zeroobj) {
        if (IS_FOUNTAIN(here))
            dipfountain(obj);
        else {
            water_damage(obj, NULL, TRUE);
            if (obj->otyp == POT_ACID)
                useup(obj);
        }
        return 1;
    }

    if (potion == obj && potion->quan == 1L) {
        pline(msgc_cancelled, "That is a potion bottle, not a Klein bottle!");
        return 0;
    }
    potion->in_use = TRUE;      /* assume it will be used up */
    if (potion->otyp == POT_WATER) {
        boolean useeit = !Blind;
        boolean usedup = FALSE;

        if (useeit)
            Your_buf = Shk_Your(obj);
        if (potion->blessed) {
            if (obj->cursed) {
                if (useeit)
                    pline(msgc_itemrepair, "%s %s %s.", Your_buf,
                          aobjnam(obj, "softly glow"), hcolor("amber"));
                uncurse(obj);
                obj->bknown = 1;
                usedup = TRUE;
            } else if (!obj->blessed) {
                if (useeit) {
                    tmp = hcolor("light blue");
                    pline(msgc_itemrepair, "%s %s with a%s %s aura.",
                          Your_buf, aobjnam(obj, "softly glow"),
                          strchr(vowels, *tmp) ? "n" : "", tmp);
                }
                bless(obj);
                obj->bknown = 1;
                usedup = TRUE;
            }
        } else if (potion->cursed) {
            if (obj->blessed) {
                if (useeit)
                    pline(msgc_itemloss, "%s %s %s.", Your_buf,
                          aobjnam(obj, "glow"), hcolor((const char *)"brown"));
                unbless(obj);
                obj->bknown = 1;
                usedup = TRUE;
            } else if (!obj->cursed) {
                if (useeit) {
                    tmp = hcolor("black");
                    pline(msgc_itemloss, "%s %s with a%s %s aura.", Your_buf,
                          aobjnam(obj, "glow"), strchr(vowels, *tmp) ? "n" : "",
                          tmp);
                }
                curse(obj);
                obj->bknown = 1;
                usedup = TRUE;
            }
        } else if (water_damage(obj, NULL, TRUE) >= 2)
            usedup = TRUE;

        if (usedup) {
            makeknown(POT_WATER);
            useup(potion);
            return 1;
        }

    } else if (potion->otyp == POT_POLYMORPH) {
        /* some objects can't be polymorphed */
        if (poly_proof(obj) || obj_resists(obj, 5, 95)) {
            pline(msgc_failrandom, "Nothing happens.");
        } else {
            int save_otyp = obj->otyp;

            /* KMH, conduct */
            break_conduct(conduct_polypile);

            obj = poly_obj(obj, STRANGE_OBJECT);

            if (!obj || obj->otyp != save_otyp) {
                makeknown(POT_POLYMORPH);
                useup(potion);
                if (obj && !(obj->owornmask & W_EQUIP))
                    /* If equipped, this will already have been done. */
                    prinv(NULL, obj, 0L);
                return 1;
            } else {
                /* different message from 3.4.3; having this almost identical
                   to the regular message is silly, given that the message is
                   unique and the fact that the potion is used up is a
                   giveaway that something is up */
                pline(msgc_failrandom,
                      "The object you dipped changed slightly.");
                makeknown(POT_POLYMORPH);
                useup(potion);
                return 1;
            }
        }
        potion->in_use = FALSE; /* didn't go poof */
        return 1;
    } else if (potion->otyp == POT_WONDER) {
        potion->in_use = TRUE;
        boolean vis = !Blind;
        boolean did_anything = FALSE;
        int current_props = obj->oprops;
        int prop = 0;

        /*
         * Cursed: Remove a property.
         * Uncursed: Try to add a random (valid) property once, potentially redundant.
         * Blessed: If we can, always add a new property.
         * For noncursed potions, there is a 1-1/<amount of properties + 1> chance of
         * removing all the properties instead, and a new property is only attempted
         * 1/<amount of properties + 1> of the time.
         */
        if (potion->cursed) {
            if (obj->oprops) {
                do {
                    prop = rn2(32);
                    prop = 1 << prop;
                    obj->oprops &= ~prop;
                } while (obj->oprops == current_props);

                if (vis)
                    pline(msgc_itemloss, "%s %s in a %s light.", Shk_Your(obj),
                          aobjnam(obj, "glow"), hcolor("purple"));
                did_anything = TRUE;
            }
        } else {
            /* Only try to add a property 1/<existing properties + 1> of the time. */
            int prop_amount = 0;
            prop = 1;
            while (prop <= opm_all) {
                if (obj->oprops & prop)
                    prop_amount++;
                prop <<= 1;
            }

            /* Check if we can add anything by comparing properties valid after opm_all
               filtering with current properties. */
            obj->oprops = opm_all;
            obj->oprops = obj_properties(obj);

            int valid_props = obj->oprops;
            obj->oprops = current_props;

            if (rn2(prop_amount + 1)) {
                /* Uh-oh... */
                obj->oprops = 0;
                pline(msgc_itemloss, "%s %s in a %s light, and then you feel a loss of "
                      "power!", Shk_Your(obj), aobjnam(obj, "violently glow"),
                      hcolor("golden"));
                did_anything = TRUE;
            } else if (!rn2(prop_amount + 1) && current_props != valid_props) {
                /* Add a property! */
                do {
                    prop = 0;
                    while (!(prop & valid_props)) {
                        prop = rn2(32);
                        prop = 1 << prop;
                    }

                    obj->oprops |= prop;
                    obj->oprops = obj_properties(obj);
                } while (potion->blessed && obj->oprops == current_props);

                boolean newprop = (obj->oprops != current_props);
                if (vis)
                    pline(newprop ? msgc_failrandom : msgc_itemrepair,
                          "%s %s in a %s light%s.", Shk_Your(obj),
                          aobjnam(obj, "glow"), hcolor("golden"),
                          newprop ? "" : " for a moment");
                did_anything = TRUE;
            } else if (current_props != valid_props) {
                /* Nothing happens */
                obj->oprops = current_props;
                pline(msgc_failrandom, "Nothing seems to happen.");
                did_anything = TRUE;
            }

            if (did_anything) {
                makeknown(POT_WONDER);
                useup(potion);
                return 1;
            } else
                potion->in_use = FALSE;
        }
    } else if (obj->oclass == POTION_CLASS && obj->otyp != potion->otyp) {
        /* Mixing potions is dangerous... */
        pline(msgc_occstart, "The potions mix...");
        /* KMH, balance patch -- acid is particularly unstable */
        /* AIS: We use a custom RNG for this; many players alchemize only once
           or twice per game with an enormous stack of potions, and whether they
           survive or are consumed has balance implications */
        if (obj->cursed || obj->otyp == POT_ACID ||
            !rn2_on_rng(10, rng_alchemic_blast)) {
            pline(msgc_failrandom, "BOOM!  They explode!");
            wake_nearby(FALSE);
            exercise(A_STR, FALSE);
            if (!breathless(youmonst.data) || haseyes(youmonst.data))
                potionbreathe(&youmonst, obj);
            useup(obj);
            useup(potion);
            losehp(rnd(10), killer_msg(DIED, "an alchemic blast"));
            return 1;
        }

        obj->blessed = obj->cursed = obj->bknown = 0;
        if (Blind || Hallucination)
            obj->dknown = 0;

        if ((mixture = mixtype(obj, potion)) != 0) {
            obj->otyp = mixture;
        } else {
            switch (obj->odiluted ? 1 : rnd(8)) {
            case 1:
                obj->otyp = POT_WATER;
                break;
            case 2:
            case 3:
                obj->otyp = POT_SICKNESS;
                break;
            case 4:
                {
                    struct obj *otmp;

                    otmp = mkobj(level, POTION_CLASS, FALSE, rng_main);
                    obj->otyp = otmp->otyp;
                    obfree(otmp, NULL);
                }
                break;
            default:
                if (!Blind)
                    pline(msgc_failrandom,
                          "The mixture glows brightly and evaporates.");
                useup(obj);
                useup(potion);
                return 1;
            }
        }

        obj->odiluted = (obj->otyp != POT_WATER);

        if (obj->otyp == POT_WATER && !Hallucination) {
            pline(msgc_failrandom, "The mixture bubbles%s.",
                  Blind ? "" : ", then clears");
        } else if (!Blind) {
            pline(msgc_actionok, "The mixture looks %s.",
                  hcolor(OBJ_DESCR(objects[obj->otyp])));
        }

        useup(potion);
        return 1;
    }
#ifdef INVISIBLE_OBJECTS
    if (potion->otyp == POT_INVISIBILITY && !obj->oinvis) {
        obj->oinvis = TRUE;
        if (!Blind) {
            if (!See_invisible)
                pline(msgc_actionok, "Where did %s go?", the(xname(obj)));
            else
                pline(msgc_actionok, "You notice a little haziness around %s.",
                      the(xname(obj)));
        }
        makeknown(POT_INVISIBILITY);
        useup(potion);
        return 1;
    } else if (potion->otyp == POT_SEE_INVISIBLE && obj->oinvis) {
        obj->oinvis = FALSE;
        if (!Blind) {
            if (!See_invisible)
                pline(msgc_actionok, "So that's where %s went!",
                      the(xname(obj)));
            else
                pline(msgc_actionok, "The haziness around %s disappears.",
                      the(xname(obj)));
        }
        makeknown(POT_SEE_INVISIBLE);
        useup(potion);
        return 1;
    }
#endif

    if (is_poisonable(obj)) {
        if (potion->otyp == POT_SICKNESS && !obj->opoisoned) {
            const char *buf;

            if (potion->quan > 1L)
                buf = msgcat("One of ", the(xname(potion)));
            else
                buf = The(xname(potion));
            pline(msgc_itemrepair, "%s forms a coating on %s.", buf,
                  the(xname(obj)));
            obj->opoisoned = TRUE;
            makeknown(POT_SICKNESS);
            useup(potion);
            return 1;
        } else if (obj->opoisoned &&
                   (potion->otyp == POT_HEALING ||
                    potion->otyp == POT_EXTRA_HEALING ||
                    potion->otyp == POT_FULL_HEALING)) {
            pline(msgc_itemrepair, "A coating wears off %s.", the(xname(obj)));
            obj->opoisoned = 0;
            /* trigger the "recently used" prompt because there are
               multiple possibilities */
            if (!(objects[potion->otyp].oc_name_known) &&
                !(objects[potion->otyp].oc_uname))
                docall(potion);
            useup(potion);
            return 1;
        }
    }

    if (potion->otyp == POT_OIL) {
        boolean wisx = FALSE;

        if (potion->lamplit) {  /* burning */
            int omat = objects[obj->otyp].oc_material;

            /* the code here should be merged with fire_damage */
            if (catch_lit(obj)) {
                /* catch_lit does all the work if true */
            } else if (obj->oerodeproof || obj_resists(obj, 5, 95) ||
                       !is_flammable(obj) || obj->oclass == FOOD_CLASS) {
                pline(msgc_failrandom, "%s %s to burn for a moment.",
                      Yname2(obj), otense(obj, "seem"));
            } else {
                if ((omat == PLASTIC || omat == PAPER) && !obj->oartifact)
                    obj->oeroded = MAX_ERODE;
                pline(msgc_itemloss, "The burning oil %s %s.",
                      obj->oeroded == MAX_ERODE ? "destroys" : "damages",
                      yname(obj));
                if (obj->oeroded == MAX_ERODE) {
                    obj_extract_self(obj);
                    obfree(obj, NULL);
                    obj = NULL;
                } else {
                    /* we know it's carried */
                    if (obj->unpaid) {
                        /* create a dummy duplicate to put on bill */
                        verbalize(msgc_unpaid, "You burnt it, you bought it!");
                        bill_dummy_object(obj);
                    }
                    obj->oeroded++;
                }
            }
        } else if (potion->cursed) {
            pline(msgc_failcurse,
                  "The potion spills and covers your %s with oil.",
                  makeplural(body_part(FINGER)));
            inc_timeout(&youmonst, GLIB, dice(2, 10), TRUE);
        } else if (obj->oclass != WEAPON_CLASS && !is_weptool(obj)) {
            /* the following cases apply only to weapons */
            goto more_dips;
            /* Oil removes rust and corrosion, but doesn't unburn. Arrows, etc
               are classed as metallic due to arrowhead material, but dipping
               in oil shouldn't repair them. */
        } else if ((!is_rustprone(obj) && !is_corrodeable(obj)) || is_ammo(obj)
                   || (!obj->oeroded && !obj->oeroded2)) {
            /* uses up potion, doesn't set obj->greased */
            pline(msgc_yafm, "%s %s with an oily sheen.", Yname2(obj),
                  otense(obj, "gleam"));
        } else {
            pline(msgc_itemrepair, "%s %s less %s.", Yname2(obj),
                  otense(obj, "are"), (obj->oeroded && obj->oeroded2) ?
                  "corroded and rusty" : obj->oeroded ? "rusty" : "corroded");
            if (obj->oeroded > 0)
                obj->oeroded--;
            if (obj->oeroded2 > 0)
                obj->oeroded2--;
            wisx = TRUE;
        }
        exercise(A_WIS, wisx);
        makeknown(potion->otyp);
        useup(potion);
        return 1;
    }
more_dips:

    /* Allow filling of MAGIC_LAMPs to prevent identification by player */
    if ((obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP) &&
        (potion->otyp == POT_OIL)) {
        /* Turn off engine before fueling, turn off fuel too :-) */
        if (obj->lamplit || potion->lamplit) {
            useup(potion);
            explode(u.ux, u.uy, 11, dice(6, 6), 0, EXPL_FIERY, NULL, 0);
            exercise(A_WIS, FALSE);
            return 1;
        }
        /* Adding oil to an empty magic lamp renders it into an oil lamp */
        if ((obj->otyp == MAGIC_LAMP) && obj->spe == 0) {
            obj->otyp = OIL_LAMP;
            obj->age = 0;
        }
        if (obj->age > 1000L) {
            pline(msgc_yafm, "%s %s full.", Yname2(obj), otense(obj, "are"));
            potion->in_use = FALSE;     /* didn't go poof */
        } else {
            pline(msgc_actionok, "You fill %s with oil.", yname(obj));
            check_unpaid(potion);       /* Yendorian Fuel Tax */
            obj->age += 2 * potion->age;        /* burns more efficiently */
            if (obj->age > 1500L)
                obj->age = 1500L;
            useup(potion);
            exercise(A_WIS, TRUE);
        }
        makeknown(POT_OIL);
        obj->spe = 1;
        update_inventory();
        return 1;
    }

    potion->in_use = FALSE;     /* didn't go poof */
    if ((obj->otyp == UNICORN_HORN || obj->otyp == AMETHYST) &&
        (mixture = mixtype(obj, potion)) != 0) {
        short old_otyp = potion->otyp;
        boolean old_dknown = FALSE;
        boolean more_than_one = potion->quan > 1;
        const char *oldbuf, *newbuf;

        oldbuf = "";
        if (potion->dknown) {
            old_dknown = TRUE;
            oldbuf = msgcat(hcolor(OBJ_DESCR(objects[potion->otyp])), " ");
        }
        /* with multiple merged potions, split off one and just clear it */
        if (potion->quan > 1L) {
            singlepotion = splitobj(potion, 1L);
        } else
            singlepotion = potion;

        if (singlepotion->unpaid && costly_spot(u.ux, u.uy)) {
            pline(msgc_unpaid, "You use it, you pay for it.");
            bill_dummy_object(singlepotion);
        }
        singlepotion->otyp = mixture;
        singlepotion->blessed = 0;
        if (mixture == POT_WATER)
            singlepotion->cursed = singlepotion->odiluted = 0;
        else
            singlepotion->cursed = obj->cursed; /* odiluted left as-is */
        singlepotion->bknown = FALSE;
        if (Blind) {
            singlepotion->dknown = FALSE;
        } else {
            singlepotion->dknown = !Hallucination;
            if (mixture == POT_WATER && singlepotion->dknown)
                newbuf = "clears";
            else
                newbuf = msgcat("turns ", hcolor(OBJ_DESCR(objects[mixture])));
            pline(msgc_actionok, "The %spotion%s %s.", oldbuf,
                  more_than_one ? " that you dipped into" : "", newbuf);
            if (!objects[old_otyp].oc_uname && !objects[old_otyp].oc_name_known
                && old_dknown) {
                struct obj *fakeobj = mktemp_sobj(NULL, old_otyp);

                fakeobj->dknown = 1;
                docall(fakeobj);
                obfree(fakeobj, NULL);
            }
        }
        obj_extract_self(singlepotion);
        hold_another_object(singlepotion, "You juggle and drop %s!",
                            doname(singlepotion), NULL);
        update_inventory();
        return 1;
    }

    pline(msgc_cancelled1, "Interesting...");
    return 1;
}


/* clone a gremlin or mold (2nd arg non-null implies heat as the trigger);
   hit points are cut in half (odd HP stays with original) */
struct monst *
split_mon(struct monst *mon,    /* monster being split */
          struct monst *mtmp)
{       /* optional attacker whose heat triggered it */
    struct monst *mtmp2;
    const char *reason;

    reason = "";
    if (mtmp)
        reason = msgprintf(" from %s heat", (mtmp == &youmonst) ?
                           (const char *)"your" :
                           (const char *)s_suffix(mon_nam(mtmp)));

    if (mon == &youmonst) {
        mtmp2 = cloneu();
        if (mtmp2) {
            mtmp2->mhpmax = u.mhmax / 2;
            u.mhmax -= mtmp2->mhpmax;
            pline(msgc_statusgood, "You multiply%s!", reason);
        }
    } else {
        mtmp2 = clone_mon(mon, 0, 0);
        if (mtmp2) {
            mtmp2->mhpmax = mon->mhpmax / 2;
            mon->mhpmax -= mtmp2->mhpmax;
            if (canspotmon(mon))
                pline(combat_msgc(mon, NULL, cr_hit),
                      "%s multiplies%s!", Monnam(mon), reason);
        }
    }
    return mtmp2;
}

/*potion.c*/
