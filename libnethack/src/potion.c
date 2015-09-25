/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-09-25 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "edog.h"
#include "epri.h"
#include "emin.h"

boolean notonhead = FALSE;

static int nothing, unkn;
static const char beverages_and_fountains[] =
    { ALLOW_NONE, NONE_ON_COMMA, POTION_CLASS, 0 };
static const char beverages[] = { POTION_CLASS, 0 };

static long itimeout(long);
static long itimeout_incr(long, int);
static void ghost_from_bottle(void);
static short mixtype(struct obj *, struct obj *);

/* force `val' to be within valid range for intrinsic timeout value */
static long
itimeout(long val)
{
    if (val >= TIMEOUT)
        val = TIMEOUT;
    else if (val < 1)
        val = 0;

    return val;
}

/* increment `old' by `incr' and force result to be valid intrinsic timeout */
static long
itimeout_incr(long old, int incr)
{
    return itimeout((old & TIMEOUT) + (long)incr);
}

/* set the timeout field of intrinsic `which' */
void
set_itimeout(unsigned int *which, long val)
{
    *which &= ~TIMEOUT;
    *which |= itimeout(val);
}

/* increment the timeout field of intrinsic `which' */
void
incr_itimeout(unsigned int *which, long incr)
{
    set_itimeout(which, itimeout_incr(*which, incr));
}


void
make_confused(long xtime, boolean talk)
{
    long old = HConfusion;

    if (!xtime && old) {
        if (talk)
            pline("You feel less %s now.",
                  Hallucination ? "trippy" : "confused");
    }

    set_itimeout(&HConfusion, xtime);
}


void
make_stunned(long xtime, boolean talk)
{
    long old = HStun;

    if (!xtime && old) {
        if (talk)
            pline("You feel %s now.",
                  Hallucination ? "less wobbly" : "a bit steadier");
    }
    if (xtime && !old) {
        if (talk) {
            if (u.usteed)
                pline("You wobble in the saddle.");
            else
                pline("You %s...", stagger(youmonst.data, "stagger"));
        }
    }

    set_itimeout(&HStun, xtime);
}


void
make_sick(long xtime, const char *cause, boolean talk, int type)
{
    long old = Sick;

    if (xtime > 0L) {
        if (Sick_resistance)
            return;
        if (!old) {
            /* newly sick */
            pline("You feel deathly sick.");
        } else {
            /* already sick */
            if (talk)
                pline("You feel %s worse.",
                      xtime <= Sick / 2L ? "much" : "even");
        }
        set_itimeout(&Sick, xtime);
        u.usick_type |= type;
    } else if (old && (type & u.usick_type)) {
        /* was sick, now not */
        u.usick_type &= ~type;
        if (u.usick_type) {     /* only partly cured */
            if (talk)
                pline("You feel somewhat better.");
            set_itimeout(&Sick, Sick * 2);      /* approximation */
        } else {
            if (talk)
                pline("What a relief!");
            Sick = 0L;  /* set_itimeout(&Sick, 0L) */
        }
    }

    if (Sick) {
        exercise(A_CON, FALSE);
        if (cause) {
            set_delayed_killer(POISONING, killer_msg(POISONING, cause));
        } else
            set_delayed_killer(POISONING, NULL);
    } else
        set_delayed_killer(POISONING, NULL);
}


void
make_vomiting(long xtime, boolean talk)
{
    long old = Vomiting;

    if (!xtime && old)
        if (talk)
            pline("You feel much less nauseated now.");

    set_itimeout(&Vomiting, xtime);
}

static const char vismsg[] =
    "Your vision seems to %s for a moment but is %s now.";
static const char eyemsg[] = "Your %s momentarily %s.";

void
eyepline(const char *verb_one_eye, const char *verb_two_eyes)
{
    if (eyecount(youmonst.data) == 1)
        pline(eyemsg, body_part(EYE), verb_one_eye);
    else
        pline(eyemsg, makeplural(body_part(EYE)), verb_two_eyes);
}

void
make_blinded(long xtime, boolean talk)
{
    long old = Blinded;
    boolean u_could_see, can_see_now;

    /* we need to probe ahead in case the Eyes of the Overworld are or will be
       overriding blindness */
    u_could_see = !Blind;
    Blinded = xtime ? 1L : 0L;
    can_see_now = !Blind;
    Blinded = old;      /* restore */

    if (u_helpless(hm_unconscious))
        talk = FALSE;

    if (can_see_now && !u_could_see) {  /* regaining sight */
        if (talk) {
            if (Hallucination)
                pline("Far out!  A light show!");
            else
                pline("You can see again.");
        }
    } else if (old && !xtime) {
        /* clearing temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling(NULL, NULL);
            } else if (Blindfolded) {
                eyepline("itches", "itch");
            } else {    /* Eyes of the Overworld */
                pline(vismsg, "brighten", Hallucination ? "sadder" : "normal");
            }
        }
    }

    if (u_could_see && !can_see_now) {  /* losing sight */
        if (talk) {
            if (Hallucination)
                pline("Oh, bummer!  Everything is dark!  Help!");
            else
                pline("A cloud of darkness falls upon you.");
        }
        /* Before the hero goes blind, set the ball&chain variables. */
        if (Punished)
            set_bc(0);
    } else if (!old && xtime) {
        /* setting temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling(NULL, NULL);
            } else if (Blindfolded) {
                eyepline("twitches", "twitch");
            } else {    /* Eyes of the Overworld */
                pline(vismsg, "dim", Hallucination ? "happier" : "normal");
            }
        }
    }

    set_itimeout(&Blinded, xtime);

    if (u_could_see ^ can_see_now) {    /* one or the other but not both */
        turnstate.vision_full_recalc = TRUE; /* blindness just got toggled */
        if (Blind_telepat || Infravision)
            see_monsters(FALSE);
    }
}


boolean
make_hallucinated(long xtime,   /* nonzero if this is an attempt to turn on
                                   hallucination */
                  boolean talk)
{       /* nonzero if resistance status should change by mask */
    long old = HHallucination;
    boolean changed = 0;
    const char *message, *verb;

    message =
        (!xtime) ? "Everything %s SO boring now." :
        "Oh wow!  Everything %s so cosmic!";
    verb = (!Blind) ? "looks" : "feels";

    if (!Halluc_resistance && (! !HHallucination != ! !xtime))
        changed = TRUE;
    set_itimeout(&HHallucination, xtime);

    /* clearing temporary hallucination without toggling vision */
    if (!changed && !HHallucination && old && talk) {
        if (!haseyes(youmonst.data)) {
            strange_feeling(NULL, NULL);
        } else if (Blind) {
            eyepline("itches", "itch");
        } else {    /* Grayswandir */
            pline(vismsg, "flatten", "normal");
        }
    }

    if (changed && !program_state.suppress_screen_updates) {
        if (Engulfed) {
            swallowed(0);       /* redraw swallow display */
        } else {
            /* The see_* routines should be called *before* the pline. */
            see_monsters(TRUE);
            see_objects(TRUE);
            see_traps(TRUE);
        }

        /* for perm_inv and anything similar (eg. Qt windowport's equipped
           items display) */
        update_inventory();

        if (talk)
            pline(message, verb);
    }
    return changed;
}


static void
ghost_from_bottle(void)
{
    struct monst *mtmp =
        makemon(&mons[PM_GHOST], level, u.ux, u.uy, NO_MM_FLAGS);

    if (!mtmp) {
        pline("This bottle turns out to be empty.");
        return;
    }
    if (Blind) {
        pline("As you open the bottle, something emerges.");
        return;
    }
    if (Hallucination) {
        int idx = rndmonidx();

        pline("As you open the bottle, %s emerges!", monnam_is_pname(idx)
              ? monnam_for_index(idx)
              : (idx < SPECIAL_PM && (mons[idx].geno & G_UNIQ))
              ? the(monnam_for_index(idx))
              : an(monnam_for_index(idx)));
    } else {
        pline("As you open the bottle, an enormous ghost emerges!");
    }
    if (flags.verbose)
        pline("You are frightened to death, and unable to move.");

    helpless(3, hr_afraid, "being frightened to death",
             "You regain your composure.");
}


/* "Quaffing is like drinking, except you spill more."  -- Terry Pratchett
 */
int
dodrink(const struct nh_cmd_arg *arg)
{
    struct obj *potion;
    const char *potion_descr;
    void (*terrain) (void) = 0;

    if (Strangled) {
        pline("If you can't breathe air, how can you drink liquid?");
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
            pline("Do you know what lives in this water?!");
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
        if (!strcmp(potion_descr, "milky") && flags.ghost_count < MAXMONNO &&
            !rn2(POTION_OCCUPANT_CHANCE(flags.ghost_count))) {
            ghost_from_bottle();
            useup(potion);
            return 1;
        } else if (!strcmp(potion_descr, "smoky") &&
                   flags.djinni_count < MAXMONNO &&
                   !rn2_on_rng(POTION_OCCUPANT_CHANCE(flags.djinni_count),
                               rng_smoky_potion)) {
            djinni_from_bottle(potion);
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
    nothing = unkn = 0;
    if ((retval = peffects(&youmonst, otmp)) >= 0)
        return retval;

    if (nothing) {
        unkn++;
        pline("You have a %s feeling for a moment, then it passes.",
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
peffects(struct monst *mon, struct obj *otmp)
{
    int i, ii, lim;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    int dmg = 0;
    int heal = 0;
    int healmax = 0;
    /* For nutrition */
    struct edog *edog;
    edog = (!you && mon->mtame && !mon->isminion) ? EDOG(mon) : 0;
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

    if (you)
        alignment = u.ualign.type;
    else if (mon->ispriest || (mon->isminion && roamer_type(mon->data)))
        alignment = CONST_EPRI(mon)->shralign;
    else if (mon->isminion)
        alignment = EMIN(mon)->min_align;
    else {
        alignment = mon->data->maligntyp;
        alignment =
            (alignment > 0) ? A_LAWFUL :
            (alignment == -128) ? A_NONE :
            (alignment < 0) ? A_CHAOTIC : A_NEUTRAL;
    }

    switch (otmp->otyp) {
    case POT_RESTORE_ABILITY:
    case SPE_RESTORE_ABILITY:
        if (otmp->cursed) {
            if (you)
                pline("Ulch!  This makes you feel mediocre!");
            else if (vis)
                pline("%s looks strangely mediocre.", Monnam(mon));
            break;
        } else {
            if (!you) {
                /* Raise max HP depending on how far from current level's HP cap the monster is
                   If the potion is uncursed, only do a single raise.
                   Equavilent to what "drain strength" does against monsters (but in reverse).
                */
                if (!vis)
                    unkn++;
                else
                    pline("%s looks %s!", Monnam(mon), otmp->blessed ? "great" : "better");
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
            pline("Wow!  This makes you feel %s!",
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
                pline("Oh wow!  Great stuff!");
            else if (vis)
                pline("%s looks strangely crazed!", Monnam(mon));
        } else {
            unkn++;
            nothing++;
        }
        set_property(mon, prop, rn1(200, 600 - 300 * bcsign(otmp)), TRUE);
        break;
    case POT_WATER:
        if (!otmp->blessed && !otmp->cursed) {
            if (you)
                pline("This tastes like water.");
            else
                unkn++;
            if (you) {
                u.uhunger += rnd(10);
                newuhs(FALSE);
            } else if (edog)
                dog_eat(mon, otmp, mon->mx, mon->my, FALSE);
            break;
        }
        if (!you && !vis)
            unkn++;
        if (is_undead(mon->data) || is_demon(mon->data) ||
            alignment == A_CHAOTIC) {
            if (otmp->blessed) {
                if (you) {
                    pline("This burns like acid!");
                    exercise(A_CON, FALSE);
                } else if (vis)
                    pline("%s shrieks in pain!", Monnam(mon));
                if ((you && u.ulycn >= LOW_PM) || is_were(mon->data)) {
                    if (you || vis)
                        pline("%s affinity to %s disappears!", Mons,
                              makeplural(you ? mons[u.ulycn].mname : mon_nam(mon)));
                    mon_unwere(mon, TRUE, FALSE);
                }
                if (you)
                    losehp(dice(2, 6), killer_msg(DIED, "a potion of holy water"));
                else {
                    mon->mhp -= dice(2, 6);
                    if (mon->mhp < 1)
                        mondied(mon);
                }
            } else if (otmp->cursed) {
                if (you || vis)
                    pline("%s %s quite proud of %sself.", Mon, looks,
                          you ? "your" : mhim(mon));
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
                    pline("%s %s full of awe.", Mon, looks);
                set_property(mon, SICK, -2, FALSE);
                if (you) {
                    exercise(A_WIS, TRUE);
                    exercise(A_CON, TRUE);
                }
                if (is_were(mon->data))
                    mon_unwere(mon, TRUE, TRUE);
            } else {
                if (alignment == A_LAWFUL) {
                    if (you)
                        pline("This burns like acid!");
                    else if (vis)
                        pline("%s shrieks in pain!", Monnam(mon));
                    if (you)
                        losehp(dice(2, 6),
                               killer_msg(DIED, "a potion of unholy water"));
                    else {
                        mon->mhp -= dice(2, 6);
                        if (mon->mhp < 1)
                            mondied(mon);
                    }
                } else if (you)
                    pline("You feel full of dread.");
                else if (vis)
                    pline("%s looks dreaded.", Monnam(mon));
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
            unkn++;
        if (you)
            pline("Ooph!  This tastes like %s%s!",
                  otmp->odiluted ? "watered down " : "",
                  Hallucination ? "dandelion wine" : "liquid fire");
        else if (vis)
            pline("%s looks less coordinated.", Monnam(mon));
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
                pline("%s pass%s out!", Mon,
                      you ? "" : "es");
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
            unkn++;
            if (you) {
                pline("You have an uneasy feeling.");
                exercise(A_WIS, FALSE);
            } else
                pline("%s looks uneasy.", Monnam(mon));
        } else {
            if (you && otmp->blessed) {
                adjattrib(A_INT, 1, FALSE);
                adjattrib(A_WIS, 1, FALSE);
            }
            pline("%s %s self-knowledgeable...", Mon, looks);
            /* no real effect on monsters at the moment */
            if (you) {
                win_pause_output(P_MESSAGE);
                enlightenment(0);
                pline("The feeling subsides.");
                exercise(A_WIS, TRUE);
            }
        }
        break;
    case SPE_INVISIBILITY:
        /* spell cannot penetrate mummy wrapping */
        if (binvisible(mon)) {
            if (you || vis)
                pline("%s %s rather itchy under %s %s.", Mon, looks, his,
                      xname(which_armor(mon, os_armc)));
            break;
        }
        /* FALLTHRU */
    case POT_INVISIBILITY:
        if (blind(&youmonst) || (!you && !vis))
            unkn++;
        if (invisible(mon) || (blind(&youmonst) && you))
            nothing++;
        set_property(mon, INVIS, otmp->blessed ? 0 : rn1(15, 31), FALSE);
        newsym(m_mx(mon), m_my(mon));     /* update position */
        if (otmp->cursed) {
            if (you) {
                pline("For some reason, you feel your presence is known.");
                aggravate();
            } else
                you_aggravate(mon);
        }
        break;
    case POT_SEE_INVISIBLE:
        /* tastes like fruit juice in Rogue */
    case POT_FRUIT_JUICE:
        {
            unkn++;
            if (otmp->cursed && you)
                pline("Yecch!  This tastes %s.",
                      Hallucination ? "overripe" : "rotten");
            else if (you)
                pline(Hallucination ?
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
                unkn--;
            break;
        }
    case POT_PARALYSIS:
        if (free_action(mon)) {
            if (you || vis)
                pline("%s stiffen%s momentarily.", Mon, you ? "" : "s");
            break;
        }
        if (you || vis) {
            if (levitates(mon) || Is_airlevel(m_mz(mon)) || Is_waterlevel(m_mz(mon)))
                pline("%s %s motionlessly suspended.", Mon, you ? "are" : "is");
            else if (you && u.usteed)
                pline("You are frozen in place!");
            else
                pline("%s %s are frozen to the %s!", Mons,
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
                pline("%s yawn%s", Mon, you ? "" : "s");
            break;
        }
        if (you || vis)
            pline("%s suddenly %s asleep!", Mon, you ? "" : "s");
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
                nothing++;
                unkn++;
            } else if (!you)
                unkn++;
            /* after a while, repeated uses become less effective */
            if (property_timeout(mon, DETECT_MONSTERS) >= 300)
                i = 1;
            else
                i = rn1(40, 21);
            set_property(mon, DETECT_MONSTERS, i, FALSE);
            break;
        }
        /* TODO: uncursed (or unskilled/basic spell) for monsters */
        if (!you) {
            unkn++;
            if (vis)
                pline("%s is granted an insight!", Mon);
        } else if (you && monster_detect(otmp, 0))
            return 1;   /* nothing detected */
        if (you)
            exercise(A_WIS, TRUE);
        break;
    case POT_OBJECT_DETECTION:
    case SPE_DETECT_TREASURE:
        if (!you) {
            unkn++;
            if (vis)
                pline("%s is granted an insight!", Mon);
        } else if (object_detect(otmp, 0))
            return 1;   /* nothing detected */
        if (you)
            exercise(A_WIS, TRUE);
        break;
    case POT_SICKNESS:
        if (you)
            pline("Yecch!  This stuff tastes like poison.");
        else if (vis)
            pline("%s looks %ssick.", Mon, otmp->blessed ? "slightly" : "");
        if (otmp->blessed) {
            if (you)
                pline("(But in fact it was mildly stale %s.)", fruitname(TRUE));
            if (!resists_poison(mon))
                /* NB: blessed otmp->fromsink is not possible */
                losehp(1, killer_msg(DIED, "a mildly contaminated potion"));
        } else {
            if (you) {
                if (resists_poison(mon))
                    pline("(But in fact it was biologically contaminated %s.)",
                          fruitname(TRUE));
                if (Role_if(PM_HEALER))
                    pline("Fortunately, you have been immunized.");
            }
            if ((you && !Role_if(PM_HEALER)) ||
                monsndx(mon->data) != PM_HEALER) {
                int typ = rn2(A_MAX);
                if (!fixed_abilities(mon)) {
                    if (you) {
                        poisontell(typ);
                        adjattrib(typ, resists_poison(mon) ? -1 : -rn1(4, 3), TRUE);
                    } else {
                        int dmg = resists_poison(mon) ? -1 : -rn1(4, 3);
                        dmg = dice(dmg, 8);
                        mon->mhpmax -= dmg;
                        mon->mhp -= dmg;
                        if (mon->mhpmax < 1)
                            mon->mhpmax = 1; /* avoid strange issues if mon gets lifesaved */
                        if (mon->mhp < 1) {
                            mondied(mon);
                            break;
                        }
                    }
                }
                if (resists_poison(mon)) {
                    if (you && otmp->fromsink)
                        losehp(rnd(10) + 5 * ! !(otmp->cursed),
                               killer_msg(DIED, "contaminated tap water"));
                    else if (you)
                        losehp(rnd(10) + 5 * ! !(otmp->cursed),
                               killer_msg(DIED, "contaminated potion"));
                    else {
                        mon->mhp -= rnd(10) + 5;
                        if (mon->mhp < 1)
                            mondied(mon);
                    }
                }
                if (you)
                    exercise(A_CON, FALSE);
            }
        }
        if (hallucinating(mon)) {
            if (you || vis)
                pline("%s %s shocked back to %s senses!", Mon,
                      you ? "are" : "is", his);
            set_property(mon, HALLUC, -2, TRUE);
        }
        break;
    case POT_CONFUSION:
        if (confused(mon))
            nothing++;
        set_property(mon, CONFUSION, rn1(7, 16 - 8 * bcsign(otmp)), FALSE);
        break;
    case POT_GAIN_ABILITY:
        if (otmp->cursed) {
            if (you)
                pline("Ulch!  That potion tasted foul!");
            unkn++;
            break;
        } else if (fixed_abilities(mon)) {
            nothing++;
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
        /* FALLTHROUGH for monsters -- make gain level/ability equavilent for noncursed*/
    case POT_GAIN_LEVEL:
        if (otmp->cursed) {
            unkn++;
            /* they went up a level */
            if ((ledger_no(m_mz(mon)) == 1 && mon_has_amulet(mon)) ||
                Can_rise_up(m_mx(mon), m_my(mon), m_mz(mon))) {
                const char *riseup = "%s rise%s up, through the %s!";

                if (ledger_no(m_mz(mon)) == 1) {
                    pline(riseup, Mon, you ? "" : "s",
                          ceiling(m_mx(mon), m_my(mon)));
                    if (you)
                        goto_level(&earth_level, FALSE, FALSE, FALSE);
                    else { /* ouch... */
                        migrate_to_level(mon, ledger_no(&earth_level), MIGR_NEAR_PLAYER, NULL);
                        if (vis) {
                            pline("Congratulations, %s!", mortal_or_creature(mon->data, TRUE));
                            pline("But now thou must face the final Test...");
                        }
                        pline("%s managed to enter the Planes with the Amulet...",
                              Monnam(mon));
                        pline("You feel a sense of despair as you realize that all is lost.");
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
                        if (you)
                            pline("It tasted bad.");
                        break;
                    } else
                        pline(riseup, ceiling(u.ux, u.uy));
                    if (you)
                        goto_level(&newlevel, FALSE, FALSE, FALSE);
                    else
                        migrate_to_level(mon, ledger_no(&newlevel), MIGR_RANDOM, NULL);
                }
            } else if (you)
                pline("You have an uneasy feeling.");
            else if (vis)
                pline("%s looks uneasy.", Monnam(mon));
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
                unkn++;
            else {
                if (otmp->otyp == POT_GAIN_LEVEL)
                    pline("%s seems more experienced.", Mon);
                else
                    pline("%s abilities looks improved.", Mons);
            }
            if (!grow_up(mon, NULL))
                return unkn;
        }
        break;
    case POT_SPEED:
        if (you && Wounded_legs && !otmp->cursed && !u.usteed
            /* heal_legs() would heal steeds legs */ ) {
            heal_legs(Wounded_leg_side);
            unkn++;
            break;
        }       /* and fall through */
    case SPE_HASTE_SELF:
        if (you)
            exercise(A_DEX, TRUE);
        set_property(mon, FAST, rn1(10, 100 + 60 * bcsign(otmp)), FALSE);
        break;
    case POT_BLINDNESS:
        if (blind(mon))
            nothing++;
        set_property(mon, BLINDED, rn1(200, 250 - 125 * bcsign(otmp)), FALSE);
        break;
    case POT_FULL_HEALING:
        heal = 400;
        healmax = otmp->blessed ? 8 : 4;
        if (you || vis)
            pline("%s %s fully healed", Mon, looks);
        /* Increase level if you lost some/many */
        if (you && otmp->blessed && u.ulevel < u.ulevelmax)
            pluslvl(FALSE);
        /* fallthrough */
    case POT_EXTRA_HEALING:
        if (!heal) {
            heal = dice(6 + 2 * bcsign(otmp), 8);
            healmax = otmp->blessed ? 5 : 2;
            if (you || vis)
                pline("%s %s much better", Mon, looks);
        }
        set_property(mon, HALLUC, -2, FALSE);
        if (you)
            exercise(A_STR, TRUE);
    case POT_HEALING:
        if (!heal) {
            heal = dice(6 + 2 * bcsign(otmp), 4);
            healmax = 1;
            if (you || vis)
                pline("%s %s better.", Mon, looks);
        }
        /* cure blindness for EH/FH or noncursed H */
        if (healmax > 1 || !otmp->cursed)
            set_property(mon, BLINDED, -2, FALSE);
        /* cure sickness for noncursed EX/FH or blessed H */
        if (otmp->blessed || (healmax > 1 && !otmp->cursed))
            set_property(mon, SICK, -2, FALSE);
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
            nothing++;
        if (otmp->cursed) /* convert controlled->uncontrolled levi */
            /* TRUE to avoid float_down() */
            set_property(mon, LEVITATION, -1, TRUE);
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
                    pline("%s hit%s %s %s on the %s.", Mon, you ? "" : "s", his,
                          mbodypart(mon, HEAD), ceiling(m_mx(mon), m_my(mon)));
                dmg = which_armor(mon, os_armh) ? 1 : rnd(10);
                if (you)
                    losehp(dmg, killer_msg(DIED, "colliding with the ceiling"));
                else {
                    mon->mhp -= dmg;
                    if (mon->mhp < 1)
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
                pline("%s %s lackluster.", Mon, looks);
        } else {
            if (you || vis)
                pline("Magical energies course through %s body.",
                      you ? "your" : s_suffix(mon_nam(mon)));
            set_property(mon, CANCELLED, -2, FALSE);
        }
        int num;
        num = rnd(5) + 5 * otmp->blessed + 1;
        if (you) {
            u.uenmax += (otmp->cursed) ? -num : num;
            u.uen += (otmp->cursed) ? -num : num;
            if (u.uenmax <= 0)
                u.uenmax = 0;
            if (u.uen <= 0)
                u.uen = 0;
            exercise(A_WIS, otmp->cursed ? FALSE : TRUE);
        }
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
                        pline("Ahh, a refreshing drink.");
                    else if (vis)
                        pline("%s looks refreshed.", Monnam(mon));
                    good_for_you = TRUE;
                } else {
                    pline("%s burn%s %s %s.", Mon, you ? "" : "s", his,
                          mbodypart(mon, FACE));
                    dmg = dice(resists_fire(mon) ? 1 : 3, 4);
                    if (you)
                        losehp(dmg, killer_msg(DIED, "a burning potion of oil"));
                    else {
                        mon->mhp -= dmg;
                        if (mon->mhp < 1)
                            mondied(mon);
                    }
                }
            } else if (you) {
                if (otmp->cursed)
                    pline("This tastes like castor oil.");
                else
                    pline("That was smooth!");
                exercise(A_WIS, good_for_you);
            } else
                unkn++;
        }
        break;
    case POT_ACID:
        if (resists_acid(mon)) {
            if (you)
                /* Not necessarily a creature who _likes_ acid */
                pline("This tastes %s.", Hallucination ? "tangy" : "sour");
        } else {
            if (you)
                pline("This burns%s!",
                      otmp->blessed ? " a little" : otmp->
                      cursed ? " a lot" : " like acid");
            else if (vis)
                pline("%s shrieks in pain!", Monnam(mon));
            dmg = dice(otmp->cursed ? 2 : 1, otmp->blessed ? 4 : 8);
            if (you) {
                losehp(dmg, killer_msg(DIED, "drinking acid"));
                exercise(A_CON, FALSE);
            } else {
                mon->mhp -= dmg;
                if (mon->mhp < 1)
                    mondied(mon);
            }
        }
        if (you && Stoned) /* FIXME: move monster unstone here as well */
            fix_petrification();
        unkn++; /* holy/unholy water can burn like acid too */
        break;
    case POT_POLYMORPH:
        if (you || vis) {
            if (you)
                pline("You feel a little %s.", Hallucination ? "normal" : "strange");
            else if (vis && !unchanging(mon))
                pline("%s suddenly mutates!", Mon);
            else if (vis)
                pline("%s looks a little %s.", Mon,
                      hallucinating(&youmonst) ? "normal" : "strange");
        }
        if (!unchanging(mon)) {
            if (you)
                polyself(FALSE);
            else
                newcham(mon, NULL, FALSE, FALSE);
        }
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
        make_blinded(0L, TRUE);
    if (curesick)
        make_sick(0L, NULL, TRUE, SICK_ALL);
    return;
}

void
strange_feeling(struct obj *obj, const char *txt)
{
    if (flags.beginner || !txt)
        pline("You have a %s feeling for a moment, then it passes.",
              Hallucination ? "normal" : "strange");
    else
        pline("%s", txt);

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


void
potionhit(struct monst *mon, struct obj *obj, boolean your_fault)
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
        pline("The %s crashes on your %s and breaks into shards.", botlnam,
              body_part(HEAD));
        losehp(rnd(2), killer_msg(DIED, "a thrown potion"));
    } else {
        distance = distu(mon->mx, mon->my);
        if (!cansee(mon->mx, mon->my))
            pline("Crash!");
        else {
            const char *mnam = mon_nam(mon);
            const char *buf;

            if (has_head(mon->data))
                buf = msgprintf("%s %s", s_suffix(mnam),
                                (notonhead ? "body" : "head"));
            else
                buf = mnam;

            pline("The %s crashes on %s and breaks into shards.", botlnam, buf);
        }
        if (rn2(5) && mon->mhp > 1)
            mon->mhp--;
    }

    /* oil doesn't instantly evaporate */
    if (obj->otyp != POT_OIL && cansee(mon->mx, mon->my))
        pline("%s.", Tobjnam(obj, "evaporate"));

    if (isyou) {
        switch (obj->otyp) {
        case POT_OIL:
            if (lamplit)
                splatter_burning_oil(u.ux, u.uy);
            break;
        case POT_POLYMORPH:
            pline("You feel a little %s.",
                  Hallucination ? "normal" : "strange");
            if (!Unchanging && !Antimagic)
                polyself(FALSE);
            break;
        case POT_ACID:
            if (!Acid_resistance) {
                pline("This burns%s!",
                      obj->blessed ? " a little" : obj->cursed ? " a lot" : "");
                losehp(dice(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8),
                       killer_msg(DIED, "being doused in acid"));
            }
            break;
        }
    } else {
        boolean angermon = TRUE;

        if (!your_fault)
            angermon = FALSE;
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
                    pline("%s looks sound and hale again.", Monnam(mon));
            }
            break;
        case POT_SICKNESS:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_healing;
            if (dmgtype(mon->data, AD_DISE) || dmgtype(mon->data, AD_PEST) ||
                /* won't happen, see prior goto */
                resists_poison(mon)) {
                if (canseemon(mon))
                    pline("%s looks unharmed.", Monnam(mon));
                break;
            }
        do_illness:
            if ((mon->mhpmax > 3) && !resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mhpmax /= 2;
            if ((mon->mhp > 2) && !resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mhp /= 2;
            if (mon->mhp > mon->mhpmax)
                mon->mhp = mon->mhpmax;
            if (canseemon(mon))
                pline("%s looks rather ill.", Monnam(mon));
            break;
        case POT_CONFUSION:
        case POT_BOOZE:
            if (!resist(mon, POTION_CLASS, 0, NOTELL))
                set_property(mon, CONFUSION, dice(3, 8), FALSE);
            break;
        case POT_INVISIBILITY:
            angermon = FALSE;
            set_property(mon, INVIS, rn1(15, 31), FALSE);
            break;
        case POT_SLEEPING:
            /* wakeup() doesn't rouse victims of temporary sleep */
            if (sleep_monst(mon, rnd(12), POTION_CLASS)) {
                pline("%s falls asleep.", Monnam(mon));
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
                  !resist(mon, POTION_CLASS, 0, NOTELL);

                if (btmp)
                    set_property(mon, BLINDED, btmp, FALSE);
            }
            break;
        case POT_WATER:
            if (is_undead(mon->data) || is_demon(mon->data) ||
                is_were(mon->data)) {
                if (obj->blessed) {
                    pline("%s %s in pain!", Monnam(mon),
                          is_silent(mon->data) ? "writhes" : "shrieks");
                    if (!is_silent(mon->data))
                        aggravate();
                    mon->mhp -= dice(2, 6);
                    /* should only be by you */
                    if (mon->mhp < 1)
                        killed(mon);
                    else if (is_were(mon->data) && !is_human(mon->data))
                        new_were(mon);  /* revert to human */
                } else if (obj->cursed) {
                    angermon = FALSE;
                    if (canseemon(mon))
                        pline("%s looks healthier.", Monnam(mon));
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
                    pline("%s rusts.", Monnam(mon));
                mon->mhp -= dice(1, 6);
                /* should only be by you */
                if (mon->mhp < 1)
                    killed(mon);
            }
            break;
        case POT_OIL:
            if (lamplit)
                splatter_burning_oil(mon->mx, mon->my);
            break;
        case POT_ACID:
            if (!resists_acid(mon) && !resist(mon, POTION_CLASS, 0, NOTELL)) {
                pline("%s %s in pain!", Monnam(mon),
                      is_silent(mon->data) ? "writhes" : "shrieks");
                if (!is_silent(mon->data))
                    aggravate();
                mon->mhp -= dice(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
                if (mon->mhp < 1) {
                    if (your_fault)
                        killed(mon);
                    else
                        monkilled(mon, "", AD_ACID);
                }
            }
            break;
        case POT_POLYMORPH:
            bhitm(&youmonst, mon, obj);
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
                pline("Ulch!  That potion smells terrible!");
            else if (haseyes(youmonst.data)) {
                int numeyes = eyecount(youmonst.data);

                pline("Your %s sting%s!",
                      (numeyes ==
                       1) ? body_part(EYE) : makeplural(body_part(EYE)),
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
            pline("You have a momentary vision.");
            kn++;
        }
        break;
    case POT_CONFUSION:
    case POT_BOOZE:
        set_property(mon, CONFUSION, rnd(5), FALSE);
        break;
    case POT_INVISIBILITY:
        if (!blind(&youmonst) && !invisible(mon) && (you || vis)) {
            kn++;
            if (you)
                pline("For an instant you %s!",
                      See_invisible ? "could see right through yourself" :
                      "couldn't see yourself");
            else
                pline("%s %s for a moment!", Monnam(mon),
                      see_invisible(&youmonst) ? "turns transparent" : "disappears");
        }
        break;
    case POT_PARALYSIS:
        if (you || vis) {
            kn++;
            if (!free_action(mon)) {
                pline("Something seems to be holding %s.",
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
                pline("%s stiffen%s momentarily.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s");
        }
        break;
    case POT_SLEEPING:
        if (you || vis) {
            kn++;
            if (!free_action(mon) && !resists_sleep(mon)) {
                if (you) {
                    pline("You feel rather tired.");
                    helpless(5, hr_asleep, "sleeping off potion vapours", NULL);
                    exercise(A_DEX, FALSE);
                } else {
                    pline("%s falls asleep.", Monnam(mon));
                    mon->mfrozen += 5;
                    if (mon->mfrozen)
                        mon->mcanmove = 0;
                }
            } else
                pline("%s yawn%s.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s");
        }
        break;
    case POT_SPEED:
        if (you && !fast(mon)) {
            pline("Your knees seem more flexible now.");
            kn++;
        }
        set_property(mon, FAST, 5, TRUE);
        if (you)
            exercise(A_DEX, TRUE);
        break;
    case POT_BLINDNESS:
        if (you && !blind(mon)) {
            kn++;
            pline("It suddenly gets dark.");
        }
        set_property(mon, BLINDED, rnd(5), FALSE);
        if (you && !blind(mon))
            pline("Your vision quickly clears.");
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


static short
mixtype(struct obj *o1, struct obj *o2)
/* returns the potion type when o1 is dipped in o2 */
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

    if (!(arg->argtype & CMD_ARG_OBJ)) {
        here = level->locations[u.ux][u.uy].typ;
        /* Is there a fountain to dip into here? */
        if (IS_FOUNTAIN(here)) {
            qbuf = msgprintf("Dip %s into the fountain?",
                             safe_qbuf("", sizeof ("Dip  into the fountain?"),
                                       the(xname(obj)),
                                       the(simple_typename(obj->otyp)),
                                       "this item"));
            if (yn(qbuf) == 'y') {
                dipfountain(obj);
                return 1;
            }
        } else if (is_pool(level, u.ux, u.uy)) {
            tmp = waterbody_name(u.ux, u.uy);
            qbuf = msgprintf("Dip %s into the %s?",
                             safe_qbuf("",
                                       sizeof ("Dip  into the pool of water?"),
                                       the(xname(obj)),
                                       the(simple_typename(obj->otyp)),
                                       "this item"), tmp);
            if (yn(qbuf) == 'y') {
                if (Levitation) {
                    floating_above(tmp);
                } else if (u.usteed && !swims(u.usteed) &&
                           P_SKILL(P_RIDING) < P_BASIC) {
                    rider_cant_reach(); /* not skilled enough to reach */
                } else {
                    water_damage(obj, NULL, TRUE);
                    if (obj->otyp == POT_ACID)
                        useup(obj);
                }
                return 1;
            }
        }
    }
    qbuf = msgprintf("dip %s into",
                     safe_qbuf("", sizeof ("dip  into"), the(xname(obj)),
                               the(simple_typename(obj->otyp)), "this item"));
    potion = getargobj(arg, beverages, qbuf);
    if (!potion)
        return 0;

    if (potion == obj && potion->quan == 1L) {
        pline("That is a potion bottle, not a Klein bottle!");
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
                    pline("%s %s %s.", Your_buf, aobjnam(obj, "softly glow"),
                          hcolor("amber"));
                uncurse(obj);
                obj->bknown = 1;
                usedup = TRUE;
            } else if (!obj->blessed) {
                if (useeit) {
                    tmp = hcolor("light blue");
                    pline("%s %s with a%s %s aura.", Your_buf,
                          aobjnam(obj, "softly glow"),
                          strchr(vowels, *tmp) ? "n" : "", tmp);
                }
                bless(obj);
                obj->bknown = 1;
                usedup = TRUE;
            }
        } else if (potion->cursed) {
            if (obj->blessed) {
                if (useeit)
                    pline("%s %s %s.", Your_buf, aobjnam(obj, "glow"),
                          hcolor((const char *)"brown"));
                unbless(obj);
                obj->bknown = 1;
                usedup = TRUE;
            } else if (!obj->cursed) {
                if (useeit) {
                    tmp = hcolor("black");
                    pline("%s %s with a%s %s aura.", Your_buf,
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
            pline("Nothing happens.");
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
                pline("The object you dipped changed slightly.");
                makeknown(POT_POLYMORPH);
                useup(potion);
                return 1;
            }
        }
        potion->in_use = FALSE; /* didn't go poof */
        return 1;
    } else if (obj->oclass == POTION_CLASS && obj->otyp != potion->otyp) {
        /* Mixing potions is dangerous... */
        pline("The potions mix...");
        /* KMH, balance patch -- acid is particularly unstable */
        /* AIS: We use a custom RNG for this; many players alchemize only once
           or twice per game with an enormous stack of potions, and whether they
           survive or are consumed has balance implications */
        if (obj->cursed || obj->otyp == POT_ACID ||
            !rn2_on_rng(10, rng_alchemic_blast)) {
            pline("BOOM!  They explode!");
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
                    pline("The mixture glows brightly and evaporates.");
                useup(obj);
                useup(potion);
                return 1;
            }
        }

        obj->odiluted = (obj->otyp != POT_WATER);

        if (obj->otyp == POT_WATER && !Hallucination) {
            pline("The mixture bubbles%s.", Blind ? "" : ", then clears");
        } else if (!Blind) {
            pline("The mixture looks %s.",
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
                pline("Where did %s go?", the(xname(obj)));
            else
                pline("You notice a little haziness around %s.",
                      the(xname(obj)));
        }
        makeknown(POT_INVISIBILITY);
        useup(potion);
        return 1;
    } else if (potion->otyp == POT_SEE_INVISIBLE && obj->oinvis) {
        obj->oinvis = FALSE;
        if (!Blind) {
            if (!See_invisible)
                pline("So that's where %s went!", the(xname(obj)));
            else
                pline("The haziness around %s disappears.", the(xname(obj)));
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
            pline("%s forms a coating on %s.", buf, the(xname(obj)));
            obj->opoisoned = TRUE;
            makeknown(POT_SICKNESS);
            useup(potion);
            return 1;
        } else if (obj->opoisoned &&
                   (potion->otyp == POT_HEALING ||
                    potion->otyp == POT_EXTRA_HEALING ||
                    potion->otyp == POT_FULL_HEALING)) {
            pline("A coating wears off %s.", the(xname(obj)));
            obj->opoisoned = 0;
            /* trigger the "recently broken" prompt because there are
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
                pline("%s %s to burn for a moment.", Yname2(obj),
                      otense(obj, "seem"));
            } else {
                if ((omat == PLASTIC || omat == PAPER) && !obj->oartifact)
                    obj->oeroded = MAX_ERODE;
                pline("The burning oil %s %s.",
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
                        verbalize("You burnt it, you bought it!");
                        bill_dummy_object(obj);
                    }
                    obj->oeroded++;
                }
            }
        } else if (potion->cursed) {
            pline("The potion spills and covers your %s with oil.",
                  makeplural(body_part(FINGER)));
            incr_itimeout(&Glib, dice(2, 10));
        } else if (obj->oclass != WEAPON_CLASS && !is_weptool(obj)) {
            /* the following cases apply only to weapons */
            goto more_dips;
            /* Oil removes rust and corrosion, but doesn't unburn. Arrows, etc
               are classed as metallic due to arrowhead material, but dipping
               in oil shouldn't repair them. */
        } else if ((!is_rustprone(obj) && !is_corrodeable(obj)) || is_ammo(obj)
                   || (!obj->oeroded && !obj->oeroded2)) {
            /* uses up potion, doesn't set obj->greased */
            pline("%s %s with an oily sheen.", Yname2(obj),
                  otense(obj, "gleam"));
        } else {
            pline("%s %s less %s.", Yname2(obj), otense(obj, "are"),
                  (obj->oeroded &&
                   obj->oeroded2) ? "corroded and rusty" : obj->
                  oeroded ? "rusty" : "corroded");
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
            pline("%s %s full.", Yname2(obj), otense(obj, "are"));
            potion->in_use = FALSE;     /* didn't go poof */
        } else {
            pline("You fill %s with oil.", yname(obj));
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
            pline("You use it, you pay for it.");
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
            pline("The %spotion%s %s.", oldbuf,
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

    pline("Interesting...");
    return 1;
}


void
djinni_from_bottle(struct obj *obj)
{
    struct monst *mtmp;

    if (!(mtmp = makemon(&mons[PM_DJINNI], level, u.ux, u.uy, NO_MM_FLAGS))) {
        pline("It turns out to be empty.");
        return;
    }

    if (!Blind) {
        pline("In a cloud of smoke, %s emerges!", a_monnam(mtmp));
        pline("%s speaks.", Monnam(mtmp));
    } else {
        pline("You smell acrid fumes.");
        pline("Something speaks.");
    }

    int dieroll;
    /* The wish has the largest balance implications; use wish_available to
       balance it against other wish sources with the same probability; also
       find out what the dieroll was so that the non-wish results will be
       consistent if the only 80%/20%/5% wish sources so far were djinn */
    if (wish_available(obj->blessed ? 80 : obj->cursed ? 5 : 20, &dieroll)) {
        msethostility(mtmp, FALSE, TRUE); /* show as peaceful while wishing */
        verbalize("I am in your debt.  I will grant one wish!");
        makewish();
        mongone(mtmp);
        return;
    }

    /* otherwise, typically an equal chance of each other result, although
       for a cursed potion, we have a hostile djinni with an 80% chance */
    if (obj->cursed && dieroll >= 20)
        dieroll = 0;

    switch (dieroll % 4) {
    case 0:
        verbalize("You disturbed me, fool!");
        msethostility(mtmp, TRUE, TRUE);
        break;
    case 1:
        verbalize("Thank you for freeing me!");
        tamedog(mtmp, NULL);
        break;
    case 2:
        verbalize("You freed me!");
        msethostility(mtmp, FALSE, TRUE);
        break;
    case 3:
        verbalize("It is about time!");
        pline("%s vanishes.", Monnam(mtmp));
        mongone(mtmp);
        break;
    }
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
            pline("You multiply%s!", reason);
        }
    } else {
        mtmp2 = clone_mon(mon, 0, 0);
        if (mtmp2) {
            mtmp2->mhpmax = mon->mhpmax / 2;
            mon->mhpmax -= mtmp2->mhpmax;
            if (canspotmon(mon))
                pline("%s multiplies%s!", Monnam(mon), reason);
        }
    }
    return mtmp2;
}

/*potion.c*/
