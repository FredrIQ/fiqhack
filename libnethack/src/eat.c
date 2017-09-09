/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-02-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "hungerstatus.h"

static int eat_one_turn(void);
static void costly_tin(const char *);
static int eat_tin_one_turn(void);
static const char *food_xname(struct obj *, boolean);
static void choke(struct obj *);
static void nutrition_calculations(struct obj *, unsigned *,
                                   unsigned *, unsigned *);
static void touchfood(void);
static void done_eating(boolean);
static void cprefx(struct monst *, int);
static int intrinsic_possible(int, const struct permonst *);
static void givit(struct monst *, int, const struct permonst *ptr, int);
static void cpostfx(struct monst *, int);
static boolean start_tin(struct obj *);
static void fprefx(struct obj *);
static void accessory_has_effect(struct monst *, struct obj *);
static void fpostfx(struct obj *);
static int edibility_prompts(struct obj *);
static int rottenfood(struct obj *);
static void eataccessory(struct monst *, struct obj *);
static const char *foodword(struct obj *);
static boolean maybe_cannibal(int, boolean);
static void lesshungry(int, struct obj *);

/* also used to see if you're allowed to eat cats and dogs */
#define CANNIBAL_ALLOWED() (Role_if (PM_CAVEMAN) || Race_if(PM_ORC))

static const char comestibles[] = { ALLOW_NONE, NONE_ON_COMMA, FOOD_CLASS, 0 };

static const char allobj[] = {
    ALLOW_NONE, NONE_ON_COMMA,
    COIN_CLASS, WEAPON_CLASS, ARMOR_CLASS, POTION_CLASS, SCROLL_CLASS,
    WAND_CLASS, RING_CLASS, AMULET_CLASS, FOOD_CLASS, TOOL_CLASS,
    GEM_CLASS, ROCK_CLASS, BALL_CLASS, CHAIN_CLASS, SPBOOK_CLASS, 0
};

/* Decide whether a particular object can be eaten by the possibly polymorphed
   character; if now is set, return whether eating is possible now, otherwise
   return whether it will ever be possible. Not used for monster checks. */
boolean
is_edible(const struct obj *obj, boolean now)
{
    /* protect invocation tools but not Rider corpses (handled elsewhere) */
    /* if (obj->oclass != FOOD_CLASS && obj_resists(obj, 0, 0)) */
    if (objects[obj->otyp].oc_unique)
        return FALSE;
    /* above also prevents the Amulet from being eaten, so we must never allow
       fake amulets to be eaten either [which is already the case] */

    if (metallivorous(youmonst.data) &&
        (is_metallic(obj) || obj->oclass == COIN_CLASS) &&
        (youmonst.data != &mons[PM_RUST_MONSTER] || is_rustprone(obj)))
        return TRUE;
    if (u.umonnum == PM_GELATINOUS_CUBE && is_organic(obj) &&
        /* [g.cubes can eat containers and retain all contents as engulfed
           items, but poly'd player can't do that] */
        !Has_contents(obj))
        return TRUE;

    /* Items that could be eaten if polymorphed */
    if ((is_metallic(obj) || is_rustprone(obj) ||
         is_organic(obj) || obj->oclass == COIN_CLASS) && !now)
        return TRUE;

    return (boolean) (obj->oclass == FOOD_CLASS);
}
static boolean
is_edible_now(const struct obj *obj)
{
    return is_edible(obj, TRUE);
}

void
init_uhunger(void)
{
    u.uhunger = 900;
    u.uhs = NOT_HUNGRY;
}

static const struct {
    const char *txt;
    int nut;
} tintxts[] = {
  { "deep fried", 60},
  { "pickled", 40},
  { "soup made from", 20},
  { "pureed", 500},
#define ROTTEN_TIN 4
  { "rotten", -50},
#define HOMEMADE_TIN 5
  { "homemade", 50},
  { "stir fried", 80},
  { "candied", 100},
  { "boiled", 50},
  { "dried", 55},
  { "szechuan", 70},
#define FRENCH_FRIED_TIN 11
  { "french fried", 40},
  { "sauteed", 95},
  { "broiled", 80},
  { "smoked", 50},
  { "", 0}
};

#define TTSZ    SIZE(tintxts)

/* ``[the(] singular(food, xname) [)]'' with awareness of unique monsters */
static const char *
food_xname(struct obj *food, boolean the_pfx)
{
    const char *result;
    int mnum = food->corpsenm;

    if (food->otyp == CORPSE && (mons[mnum].geno & G_UNIQ)) {
        result = msgprintf("%s%s corpse",
                           (the_pfx && !type_is_pname(&mons[mnum])) ?
                           "the " : "",
                           s_suffix(mons[mnum].mname));
    } else {
        /* the ordinary case */
        result = singular(food, xname);
        if (the_pfx)
            result = the(result);
    }
    return result;
}


static void
choke(struct obj *food)
{       /* To a full belly all food is bad. (It.) */
    /* only happens if you were satiated */
    if (u.uhs != SATIATED) {
        if (!food || food->otyp != AMULET_OF_STRANGULATION)
            return;
    } else if (Role_if(PM_KNIGHT) && u.ualign.type == A_LAWFUL) {
        adjalign(-1);   /* gluttony is unchivalrous */
        pline(msgc_alignbad, "You feel like a glutton!");
    }

    exercise(A_CON, FALSE);

    /* Whatever you were doing, you're going to get rather distracted... */
    action_interrupted();

    if (Breathless || (!strangled(&youmonst) && !rn2(20))) {
        /* choking by eating AoS doesn't involve stuffing yourself */
        if (food && food->otyp == AMULET_OF_STRANGULATION) {
            pline(msgc_fatalavoid, "You choke, but recover your composure.");
            return;
        }
        pline(msgc_fatalavoid,
              "You stuff yourself and then vomit voluminously.");
        morehungry(1000);       /* you just got *very* sick! */
        vomit(&youmonst);
    } else {
        const char *killer;
        if (food) {
            pline(msgc_fatal_predone, "You choke over your %s.",
                  foodword(food));
            if (food->oclass == COIN_CLASS) {
                killer = killer_msg(CHOKING, "a very rich meal");
            } else {
                killer = killer_msg_obj(CHOKING, food);
            }
        } else {
            pline(msgc_fatal_predone, "You choke over it.");
            killer = killer_msg(CHOKING, "a quick snack");
        }
        pline(msgc_fatal_predone, "You die...");
        done(CHOKING, killer);
    }
}


/* Recalculate information about food. This will set the weight of the
   pointed-to object according to how much has been eaten, and optionally also
   return the total amount of nutrition in the food, the length of time it takes
   to eat the food, and/or the amount of nutrition that is eaten per turn. */
static void
nutrition_calculations(struct obj *obj, unsigned *total,
                       unsigned *timetaken, unsigned *perturn)
{
    unsigned temp_total, temp_timetaken;
    boolean corpse = obj->otyp == CORPSE;

    if (!total)
        total = &temp_total;
    if (!timetaken)
        timetaken = &temp_timetaken;

    if (corpse) {
        /* Work out the total amount of nutrition. */
        *total = mons[obj->corpsenm].cnutrit;

        /* The length of time it takes to eat an entire corpse, which
           depends on the weight of the monster. */
        *timetaken = 3 + (mons[obj->corpsenm].cwt >> 6);
    } else {
        /* The total nutrition and time are given by the object's
           statistics. */
        *total = objects[obj->otyp].oc_nutrition;
        *timetaken = objects[obj->otyp].oc_delay;
    }
    if (!*total)
        *total = 1;
    if (!*timetaken)
        *timetaken = 1;

    /* If we need per-turn statistics, just divide (with a minimum of 1
       nutrition/turn to ensure that we actually finish). */
    if (perturn) {
        *perturn = *total / *timetaken;
        if (*perturn == 0)
            *perturn = 1;
    }
}


/* Marks u.utracked[tos_food] as being partly eaten and forces it into a stack
   of 1. If the object is part of a stack, it will be split out from the stack.

   In extreme circumstances, merely marking food as partly eaten will destroy it
   (e.g. cockatrice corpse eaten from inventory, while stoning-resistant, while
   engulfed by a purple worm, with full inventory). The caller must take care to
   NULL-check u.utracked[tos_food] after calling this function, and take
   appropriate action. (Previously this function did the NULL check itself, but
   this confused static analysers.) */
static void
touchfood(void)
{
    /* abbreviate "u.utracked[tos_food]" to make the function easier to
       read */
    struct obj **const uttf = &(u.utracked[tos_food]);

    if ((*uttf)->quan > 1L) {
        if (!carried(*uttf))
            splitobj(*uttf, (*uttf)->quan - 1L);
        else
            *uttf = splitobj(*uttf, 1L);
    }

    if (!(*uttf)->oeaten) {
        if ((!carried(*uttf) && costly_spot((*uttf)->ox, (*uttf)->oy) &&
             !(*uttf)->no_charge) || (*uttf)->unpaid) {
            /* create a dummy duplicate to put on bill */
            verbalize(msgc_unpaid, "You bit it, you bought it!");
            bill_dummy_object(*uttf);
        }
        nutrition_calculations(*uttf, &((*uttf)->oeaten), NULL, NULL);
        (*uttf)->owt = weight(*uttf);
    }

    if (carried(*uttf)) {
        unwield_silently(*uttf);
        freeinv(*uttf);
        if (!can_hold(*uttf)) {
            sellobj_state(SELL_DONTSELL);
            /* TODO: dropy can destroy the object */
            dropy(*uttf);
            sellobj_state(SELL_NORMAL);
        } else {
            (*uttf)->nomerge = TRUE;
            *uttf = addinv(*uttf);
            (*uttf)->nomerge = FALSE;
        }
        if (!*uttf) /* explain what happened */
            pline(msgc_substitute,
                  "You must have fumbled and dropped your food.");
    }
}

/* Handles one action of eating food. The food must have been placed in
   u.utracked[tos_food] and marked as oeaten already by the caller.

   Returns 0 if finished, 1 if still eating. */
static int
eat_one_turn(void)
{
    unsigned nmod;

    if (!u.utracked[tos_food] ||
        (!carried(u.utracked[tos_food]) &&
         !obj_here(u.utracked[tos_food], u.ux, u.uy))) {
        /* this can happen if, for instance, the food was stolen, or the food
           was on the ground and the character teleported; reset_occupations
           does not check for this because walking away from a meal, back to it,
           and continuing to eat counts as continuing */
        pline(msgc_interrupted, "Huh? Where did my food go?");
        return 0;
    }

    if (!u.utracked[tos_food]->oeaten)
        impossible("Food is not partly eaten while eating it");

    nutrition_calculations(u.utracked[tos_food], NULL, NULL, &nmod);
    if (nmod > u.utracked[tos_food]->oeaten)
        nmod = u.utracked[tos_food]->oeaten;

    lesshungry(u.utracked[tos_food]->orotten ? nmod / 2 : nmod,
               u.utracked[tos_food]);
    u.utracked[tos_food]->oeaten -= nmod;

    if (!u.utracked[tos_food]->oeaten) {
        /* Call action_completed() directly to avoid the the action getting
         * interrupted if a corpse effect renders the player helpless. */
        action_completed();
        done_eating(TRUE);
        return 0;
    } else {
        /* Recalculate the object's weight. */
        u.utracked[tos_food]->owt = weight(u.utracked[tos_food]);
        return 1;
    }
}

static void
done_eating(boolean message)
{
    struct obj *otmp = u.utracked[tos_food];
    u.utracked[tos_food] = NULL;
    newuhs(FALSE);

    if (message)
        pline(msgc_actionok, "You finish eating %s.", food_xname(otmp, TRUE));

    if (otmp->otyp == CORPSE)
        cpostfx(&youmonst, otmp->corpsenm);
    else
        fpostfx(otmp);

    if (carried(otmp))
        useup(otmp);
    else
        useupf(otmp, 1L);
}

static boolean
maybe_cannibal(int pm, boolean allowmsg)
{
    if (!CANNIBAL_ALLOWED() && your_race(&mons[pm])) {
        if (allowmsg) {
            if (Upolyd)
                pline(msgc_intrloss, "You have a bad feeling deep inside.");
            pline(msgc_intrloss, "You cannibal!  You will regret this!");
        }
        set_property(&youmonst, AGGRAVATE_MONSTER, 0, FALSE);
        change_luck(-rn1(4, 2));        /* -5..-2 */
        return TRUE;
    }
    return FALSE;
}

static void
cprefx(struct monst *mon, int pm)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    if (you)
        maybe_cannibal(pm, TRUE);
    /* Note: can't use touched_monster here, Medusa acts differently on touching
       and eating */
    if ((touch_petrifies(&mons[pm]) || pm == PM_MEDUSA) &&
        ((you && !instapetrify(killer_msg(STONING,
                                          msgcat_many("tasting ", mons[pm].mname,
                                                      " meat", NULL)))) ||
         (!you && minstapetrify(NULL, mon)))) {
        return; /* abort if died or lifesaved */
    }

    switch (pm) {
    case PM_LITTLE_DOG:
    case PM_DOG:
    case PM_LARGE_DOG:
    case PM_KITTEN:
    case PM_HOUSECAT:
    case PM_LARGE_CAT:
        if (you) { /* only player can be cannibal for now */
            if (!CANNIBAL_ALLOWED()) {
                pline(msgc_intrloss, "You feel that eating the %s was a bad idea.",
                      mons[pm].mname);
                set_property(mon, AGGRAVATE_MONSTER, 0, FALSE);
            }
        }
        break;
    case PM_LIZARD:
        set_property(&youmonst, STONED, -2, FALSE);
        break;
    case PM_DEATH:
    case PM_PESTILENCE:
    case PM_FAMINE:
        {
            if (you || vis)
                pline(msgc_fatal_predone, "Eating that is instantly fatal.");
            if (you)
                done(DIED, msgcat("unwisely ate the body of ", mons[pm].mname));
            else
                mondied(mon);
            /* It so happens that since we know these monsters cannot appear in
               tins, u.utracked[tos_food] will always be what we want, which is
               not generally true. */
            if (you && revive_corpse(u.utracked[tos_food]))
                u.utracked[tos_food] = NULL;
            return;
        }
    case PM_GREEN_SLIME:
        if (!sliming(mon) && !unchanging(mon) && !flaming(mon->data) &&
            !unsolid(mon->data) && mon->data != &mons[PM_GREEN_SLIME] &&
            level->locations[m_mx(mon)][m_my(mon)].typ != LAVAPOOL)
            set_property(mon, SLIMED, 11, TRUE);
        /* Fall through */
    default:
        if (acidic(&mons[pm]))
            set_property(mon, STONED, -2, FALSE);
        break;
    }
}

/*
 * If you add an intrinsic that can be gotten by eating a monster, add it
 * to intrinsic_possible() and givit().  (It must already be in prop.h to
 * be an intrinsic property.)
 * It would be very easy to make the intrinsics not try to give you one
 * that you already had by checking to see if you have it in
 * intrinsic_possible() instead of givit().
 */

/* intrinsic_possible() returns TRUE iff a monster can give an intrinsic. */
static int
intrinsic_possible(int type, const struct permonst *ptr)
{
    switch (type) {
    case FIRE_RES:
        return ptr->mconveys & MR_FIRE;

    case SLEEP_RES:
        return ptr->mconveys & MR_SLEEP;

    case COLD_RES:
        return ptr->mconveys & MR_COLD;

    case DISINT_RES:
        return ptr->mconveys & MR_DISINT;

    case SHOCK_RES:    /* shock (electricity) resistance */
        return ptr->mconveys & MR_ELEC;

    case POISON_RES:
        return ptr->mconveys & MR_POISON;

    case TELEPORT:
        return ptr->mflags1 & M1_TPORT;

    case TELEPORT_CONTROL:
        return ptr->mflags1 & M1_TPORT_CNTRL;

    case TELEPAT:
        return ptr->mflags2 & M2_TELEPATHIC;

    default:
        return FALSE;
    }
}

/* givit() tries to give you an intrinsic based on the monster's level and what
   type of intrinsic it is trying to give you.

   Conferred intrinsics are clearly strategically important, and as such we want
   to line them up as much as possible between games. Therefore, seven of the
   intrinsics each have their own RNG. You get the intrinsic once the RNG rolls
   a 1/296 chance (not an arbitrary number, see below for the explanation); the
   more powerful the monster, the more rolls on the RNG you get. This way,
   there's no difference between strategies that use multiple weak monsters, and
   strategies that use a few strong monsters, you get the intrinsic at the same
   time either way.

   The other two intrinsics are telepathy and disintegration resistance. Both of
   these have a 100% chance of being conveyed. This is not affected by the scale
   factor (because there's no appropriate RNG). This doesn't matter very much,
   because telepathy-granting monsters tend to be either floating eyes or
   endgame quality, and monsters that grant disintegration resistance tend not
   to grant anything else. */
static void
givit(struct monst *mon, int type, const struct permonst *ptr, int scalefactor)
{
    boolean you = (mon == &youmonst);
    if (type != DISINT_RES && type != TELEPAT) {
        /* Calculate the probability of gaining the intrinsic, and the
           appropriate RNG. Some intrinsics are easier to get than others. */
        int l100; /* monster level * 6 required for a 100% chance */
        enum rng rng;
        switch (type) {
        case POISON_RES:       l100 = 90; rng = rng_intrinsic_poison; break;
        case TELEPORT:         l100 = 60; rng = rng_intrinsic_itis;   break;
        case TELEPORT_CONTROL: l100 = 72; rng = rng_intrinsic_tc;     break;
        case FIRE_RES:         l100 = 90; rng = rng_intrinsic_fire;   break;
        case SLEEP_RES:        l100 = 90; rng = rng_intrinsic_sleep;  break;
        case COLD_RES:         l100 = 90; rng = rng_intrinsic_cold;   break;
        case SHOCK_RES:        l100 = 90; rng = rng_intrinsic_shock;  break;
        default:
            impossible("Trying to confer unknown intrinsic %d", type);
            return;
        }

        /* Don't use custom RNG if it's for monster intrinsics */
        if (!you)
            rng = rng_main;

        int ml = ptr->mlevel * 6;

        if (ml > l100)
            ml = l100;

        ml /= scalefactor;

        if (ml < l100 && ml > 0) {
            /* The odds of /not/ getting the intrinsic on a direct roll are (1 -
               chance), naturally enough. The odds of not getting the intrinsic
               with n RNG rolls are (295/296) ** n. Rearranging, we get n =
               log(1 - chance) / log(295/296). Our existing integer logarithm
               function returns base-2 logarithms scaled by a factor of 1024;
               and 1024 * log(295/296) = -4.99939469..., which we approximate as
               -5.  Thus, the number of RNG rolls we get is ilog2(1 / (1 -
               chance)) / 5.

               We cannot use this formula as-is due to rounding errors: the
               chance is in the range 0..1, which does not work very well as an
               integer.  Thus, instead of calculating ilog2(1 / (1 - chance)),
               we instead calculate ilog2(1048576 / (1 - chance)) - 20480, which
               is mathematically identical.  In fact, we only subtract 20478,
               because C division truncates, and rounding will give us betrer
               results.

               In most cases, the chance is equal to mlevel / l100 (so the
               failchance is equal to (l100 - mlevel) / l100). Killer bees and
               scorpions are exceptions: they have a 25% chance of overriding
               the chance to 100% (i.e. have 75% of the fail chance). */
            long long failchance = (1048576LL * l100) / (l100 - ml);
            if ((ptr == &mons[PM_KILLER_BEE] || ptr == &mons[PM_SCORPION]) &&
                type == POISON_RES) {
                failchance = (failchance * 3) / 4;
            }

            int rngrolls = (ilog2(failchance) - 20478) / 5;
            while (rngrolls--)
                if (!rn2_on_rng(296, rng))
                    break;
            if (rngrolls == -1)
                return; /* haven't hit that 1/296 chance yet*/
        }

    }

    set_property(mon, type, 0, FALSE);
}

static void
cpostfx(struct monst *mon, int pm)
{       /* called after completely consuming a corpse */
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    int tmp = 0;
    boolean catch_lycanthropy = FALSE;
    enum msg_channel msgc = msgc_monneutral;

    switch (pm) {
    case PM_NEWT:
        /* MRKR: "eye of newt" may give small magical energy boost */
        if (you) {
            int old_uen = u.uen;
            msgc = msgc_statusheal;

            u.uen += 1 + rn2_on_rng(3, rng_newt_pw_boost);
            if (u.uen > u.uenmax) {
                u.uenmax++;
                u.uen = u.uenmax;
            } else
                u.uenmax++;

            if (you)
                msgc = msgc_intrgain;

            if (old_uen != u.uen)
                pline(msgc, "You feel a mild buzz.");
        } else if (mon->mspec_used > rn2(5)) {
            mon->mspec_used -= min(mon->mspec_used, rnd(3));
            if (vis)
                pline(msgc, "You sense a mild buzz coming from %s.",
                      mon_nam(mon));
        }
        break;
    case PM_WRAITH:
        if (you)
            pluslvl(FALSE);
        else
            grow_up(mon, NULL);
        break;
    case PM_HUMAN_WERERAT:
    case PM_HUMAN_WEREJACKAL:
    case PM_HUMAN_WEREWOLF:
        if (you) { /* TODO: improve monster lycantropy */
            catch_lycanthropy = TRUE;
            u.ulycn = counter_were(pm);
        }
        break;
    case PM_NURSE:
        if (you) {
            if (Upolyd)
                u.mh = u.mhmax;
            else
                u.uhp = u.uhpmax;
        } else
            mon->mhp = mon->mhpmax;
        break;
    case PM_STALKER:
        set_property(mon, INVIS, invisible(mon) ? 0 : rn1(100, 50), FALSE);
        /* fall into next case */
    case PM_GIANT_BAT:
        set_property(mon, STUNNED, 30, TRUE);
        /* fall into next case */
    case PM_BAT:
        set_property(mon, STUNNED, 30, TRUE);
        break;
    case PM_GIANT_MIMIC:
        tmp += 10;
        /* fall into next case */
    case PM_LARGE_MIMIC:
        tmp += 20;
        /* fall into next case */
    case PM_SMALL_MIMIC:
        tmp += 20;
        if (you && mon->data->mlet != S_MIMIC && !Unchanging) {
            /* TODO: make monsters do this too */
            const char *buf;

            pline(msgc_statusgood,
                  "You can't resist the temptation to mimic %s...",
                  Hallucination ? "an orange" : "a pile of gold");
            /* A pile of gold can't ride. */
            if (u.usteed)
                dismount_steed(DISMOUNT_FELL);
            buf = msgprintf(
                Hallucination ?
                "You suddenly dread being peeled and mimic %s again!" :
                "You now prefer mimicking %s again.",
                an(Upolyd ? youmonst.data->mname : urace.noun));
            helpless(tmp, hr_mimicking, "pretending to be a pile of gold", buf);
            youmonst.m_ap_type = M_AP_OBJECT;
            youmonst.mappearance = Hallucination ? ORANGE : GOLD_PIECE;
            newsym(u.ux, u.uy);
            flush_screen();
            /* make gold symbol show up now */
            win_pause_output(P_MAP);
        }
        break;
    case PM_QUANTUM_MECHANIC:
        if (you)
            msgc = (ifast(mon) ? msgc_intrloss : msgc_intrgain);
        set_property(mon, FAST, ifast(mon) ? -1 : 0, TRUE);
        if (you || vis) {
            pline_implied(msgc, "%s velocity suddenly seems very uncertain!",
                  s_suffix(you ? "You" : Monnam(mon)));
            pline(msgc, "%s seem%s %ser.",
                  you ? "You" : Monnam(mon),
                  you ? "" : "s",
                  ifast(mon) ? "fast" : "slow");
        }
        break;
    case PM_LIZARD:
        if (property_timeout(mon, STUNNED) > 2) {
            set_property(mon, STUNNED, -2, TRUE);
            set_property(mon, STUNNED, 2, FALSE);
        }
        if (property_timeout(mon, CONFUSION) > 2) {
            set_property(mon, CONFUSION, -2, TRUE);
            set_property(mon, CONFUSION, 2, FALSE);
        }
        break;
    case PM_CHAMELEON:
    case PM_DOPPELGANGER:
        /* case PM_SANDESTIN: */
        if (you)
            msgc = msgc_statusbad;
        if (!unchanging(mon)) {
            if (you) {
                pline(msgc, "You feel a change coming over you.");
                polyself(FALSE);
            } else {
                if (vis)
                    pline(msgc, "%s suddenly mutates!", Monnam(mon));
                newcham(mon, NULL, FALSE, vis);
            }
        }
        break;
    case PM_MIND_FLAYER:
    case PM_MASTER_MIND_FLAYER:
        if (ABASE(A_INT) < ATTRMAX(A_INT)) {
            if (you) {
                if (!rn2_on_rng(2, rng_50percent_a_int)) {
                    pline(msgc_intrgain,
                          "Yum! That was real brain food!");
                    adjattrib(A_INT, 1, FALSE);
                    break;  /* don't give them telepathy, too */
                }
            }
        } else if (you)
            pline(msgc_noconsequence, "For some reason, that tasted bland.");
        /* fall through to default case */
    default:{
            const struct permonst *ptr = &mons[pm];
            int i, count;

            if (dmgtype(ptr, AD_STUN) || dmgtype(ptr, AD_HALU) ||
                pm == PM_VIOLET_FUNGUS) {
                if (you)
                    pline(msgc_statusbad, "Oh wow!  Great stuff!");
                else if (vis)
                    pline(msgc_monneutral, "%s seems cross-eyed!",
                          Monnam(mon));
                set_property(mon, you ? HALLUC : CONFUSION, 200, TRUE);
            }
            if (is_giant(ptr) && you)
                gainstr(NULL, 0);

            /* Check the monster for all of the intrinsics. If this monster can
               give more than one, we reduce the chance of each by a factor of
               the number of intrinsics it can give. (This leads to a slight
               change from behaviour pre-4.3-beta2. Imagine a level-15 monster
               that conferred both fire and cold resistance. Under 4.3-beta1
               mechanics, it would have a 50% chance of conferring fire
               resistance, and a 50% chance of conferring cold resistance. Under
               4.3-beta2 mechanics, it has a 25% chance each of conferring one
               specific resistance, a 25% chance of conferring both, and a 25%
               chance of conferring neither. This is necessary to keep
               intrinsics lined up between multiple games on the same seed;
               otherwise, a player would be at a disadvantage if they happened
               to be "due" to get two intrinsics at the same moment.) */

            count = 0;  /* number of possible intrinsics */
            for (i = 1; i <= LAST_PROP; i++)
                if (intrinsic_possible(i, ptr))
                    count++;

            for (i = 1; i <= LAST_PROP; i++)
                if (intrinsic_possible(i, ptr))
                    givit(mon, i, ptr, count);
        }
        break;
    }

    if (you && catch_lycanthropy && defends(AD_WERE, uwep)) {
        if (!touch_artifact(uwep, &youmonst)) {
            struct obj *obj = uwep;
            unwield_silently(uwep);
            dropx(obj);
            uwepgone();
        }
    }

    return;
}

/* common code to check and possibly charge for 1 tin, will split() the tin if
   necessary */
static void
costly_tin(const char *verb /* if 0, the verb is "open" */ )
{
    if (((!carried(u.utracked[tos_tin]) &&
          costly_spot(u.utracked[tos_tin]->ox, u.utracked[tos_tin]->oy) &&
          !u.utracked[tos_tin]->no_charge)
         || u.utracked[tos_tin]->unpaid)) {
        verbalize(msgc_unpaid, "You %s it, you bought it!",
                  verb ? verb : "open");
        if (u.utracked[tos_tin]->quan > 1L)
            u.utracked[tos_tin] = splitobj(u.utracked[tos_tin], 1L);
        bill_dummy_object(u.utracked[tos_tin]);
    }
}


/* Handles one turn of opening a tin. Just like eat_one_turn(), the tin must
   have been placed in u.utracked[tos_tin] already, and
   u.uoccupation_progess[tos_tin] must be set to the number of turns it takes
   (-1 for infinity).

   Returns 0 if finished, 1 if still in progress. */
static int
eat_tin_one_turn(void)
{
    int r;
    const char *what;
    int which;

    /* The !u.utracked[tos_tin] case can't happen in the current codebase
       (there's no need for special handling to identify which object is being
       referred to when continuing the occupation, the code just uses inventory
       letter if there are no intervening commands and the user's going to
       explicitly specify the item otherwise), but is left in here for the
       future */
    if (!u.utracked[tos_tin] ||
        (!carried(u.utracked[tos_tin]) &&
         !obj_here(u.utracked[tos_tin], u.ux, u.uy))) {
        /* perhaps it was stolen? */
        pline(msgc_interrupted,
              "The tin you were opening seems to have gone missing.");
        return 0;
    }

    /* The player gives up trying to open a tin after 50 moves. To implement
       this, a tin that would take more than 50 moves is given a remaining moves
       count of -1 (i.e. infinity), and we complain when it reaches -51. */
    if (u.uoccupation_progress[tos_tin]-- < -50) {
        pline(msgc_interrupted,
              "You give up your attempt to open the tin.");
        u.utracked[tos_tin] = 0;
        u.uoccupation_progress[tos_tin] = 0;
        return 0;
    }

    if (u.uoccupation_progress[tos_tin] != 0)
        return 1;       /* still busy */

    /* It's open. */
    if (u.utracked[tos_tin]->otrapped ||
        (u.utracked[tos_tin]->cursed &&
         u.utracked[tos_tin]->spe != -1 && !rn2(8))) {
        b_trapped("tin", 0);
        costly_tin("destroyed");
        goto use_me;
    }
    pline(msgc_actionok, "You succeed in opening the tin.");
    if (u.utracked[tos_tin]->spe != 1) {
        if (u.utracked[tos_tin]->corpsenm == NON_PM) {
            pline(msgc_noconsequence, "It turns out to be empty.");
            u.utracked[tos_tin]->dknown = u.utracked[tos_tin]->known = TRUE;
            costly_tin(NULL);
            goto use_me;
        }
        r = u.utracked[tos_tin]->cursed ? ROTTEN_TIN : /* cursed => rotten */
            (u.utracked[tos_tin]->spe == -1) ? HOMEMADE_TIN :  /* player-made */
            rn2(TTSZ - 1);      /* else take your pick */
        if (r == ROTTEN_TIN &&
            (u.utracked[tos_tin]->corpsenm == PM_LIZARD ||
             u.utracked[tos_tin]->corpsenm == PM_LICHEN))
            r = HOMEMADE_TIN;   /* lizards don't rot */
        else if (u.utracked[tos_tin]->spe == -1 &&
                 !u.utracked[tos_tin]->blessed && !rn2(7))
            r = ROTTEN_TIN;     /* some homemade tins go bad */
        which = 0;      /* 0=>plural, 1=>as-is, 2=>"the" prefix */
        if (Hallucination) {
            int idx = rndmonidx();

            what = monnam_for_index(idx);
            if ((idx < SPECIAL_PM) &&
                !!(mons[u.utracked[tos_tin]->corpsenm].geno & G_UNIQ))
                which = type_is_pname(&mons[idx]) ? 1 : 2;
            else
                which = monnam_is_pname(idx) ? 1 : 0;
        } else {
            what = mons[u.utracked[tos_tin]->corpsenm].mname;
            if (mons[u.utracked[tos_tin]->corpsenm].geno & G_UNIQ)
                which =
                    type_is_pname(&mons[u.utracked[tos_tin]->corpsenm]) ? 1 : 2;
        }
        if (which == 0)
            what = makeplural(what);
        pline(msgc_info, "It smells like %s%s.",
              (which == 2) ? "the " : "", what);
        if (yn("Eat it?") == 'n') {
            if (!Hallucination)
                u.utracked[tos_tin]->dknown = u.utracked[tos_tin]->known = TRUE;
            pline(msgc_actionboring, "You discard the open tin.");
            costly_tin(NULL);
            goto use_me;
        }

        /* in case a previous meal was cancelled */
        u.utracked[tos_food] = NULL;

        pline(msgc_actionok, "You consume %s %s.", tintxts[r].txt,
              mons[u.utracked[tos_tin]->corpsenm].mname);

        /* KMH, conduct */
        break_conduct(conduct_food);
        if (!vegan(&mons[u.utracked[tos_tin]->corpsenm]))
            break_conduct(conduct_vegan);
        if (!vegetarian(&mons[u.utracked[tos_tin]->corpsenm]))
            break_conduct(conduct_vegetarian);

        u.utracked[tos_tin]->dknown = u.utracked[tos_tin]->known = TRUE;
        cprefx(&youmonst, u.utracked[tos_tin]->corpsenm);
        /* We call action_completed() here directly, so that the action is not
         * interruped if the player becomes helpless due to cpostfx. */
        action_completed();
        cpostfx(&youmonst, u.utracked[tos_tin]->corpsenm);

        /* charge for one at pre-eating cost */
        costly_tin(NULL);

        /* check for vomiting added by GAN 01/16/87 */
        if (tintxts[r].nut < 0 && !vomiting(&youmonst))
            set_property(&youmonst, VOMITING, rn1(15, 10), FALSE);
        else
            lesshungry(tintxts[r].nut, u.utracked[tos_tin]);

        if (r == 0 || r == FRENCH_FRIED_TIN) {
            /* Assume nonslippery fingers, because you can't open tins with
               slippery fingers. */
            inc_timeout(&youmonst, GLIB, rnd(15), TRUE);
            pline(msgc_statusbad,
                  "Eating deep fried food made your %s very slippery.",
                  makeplural(body_part(FINGER)));
        }
    } else {
        if (u.utracked[tos_tin]->cursed)
            pline(msgc_info,
                  "It contains some decaying%s%s substance.", Blind ? "" : " ",
                  Blind ? "" : hcolor("green"));
        else
            pline(msgc_info, "It contains spinach.");

        if (yn("Eat it?") == 'n') {
            if (!Hallucination && !u.utracked[tos_tin]->cursed)
                u.utracked[tos_tin]->dknown = u.utracked[tos_tin]->known = TRUE;
            pline(msgc_actionboring, "You discard the open tin.");
            costly_tin(NULL);
            goto use_me;
        }

        u.utracked[tos_tin]->dknown = u.utracked[tos_tin]->known = TRUE;
        costly_tin(NULL);

        if (!u.utracked[tos_tin]->cursed)
            pline_implied(msgc_intrgain, "This makes you feel like %s!",
                          Hallucination ? "Swee'pea" : "Popeye");
        lesshungry(600, u.utracked[tos_tin]);
        gainstr(u.utracked[tos_tin], 0);
        break_conduct(conduct_food);
    }
use_me:
    if (carried(u.utracked[tos_tin]))
        useup(u.utracked[tos_tin]);
    else
        useupf(u.utracked[tos_tin], 1L);
    u.utracked[tos_tin] = NULL;
    return 0;
}

/* Called when starting to open a tin.

   Returns TRUE if the caller should continue trying to open the tin,
   FALSE if something went wrong trying to start to open it. */
static boolean
start_tin(struct obj *otmp)
{
    int tmp;

    if (metallivorous(youmonst.data)) {
        pline(msgc_occstart, "You bite right into the metal tin...");
        tmp = 1;
    } else if (nolimbs(youmonst.data)) {
        pline(msgc_cancelled, "You cannot handle the tin properly to open it.");
        return FALSE;
    } else if (otmp->blessed) {
        pline(msgc_occstart, "The tin opens like magic!");
        tmp = 1;
    } else if (uwep) {
        switch (uwep->otyp) {
        case TIN_OPENER:
            tmp = 1;
            break;
        case DAGGER:
        case SILVER_DAGGER:
        case ELVEN_DAGGER:
        case ORCISH_DAGGER:
        case ATHAME:
        case CRYSKNIFE:
            tmp = 3;
            break;
        case PICK_AXE:
        case AXE:
            tmp = 6;
            break;
        default:
            goto no_opener;
        }
        pline(msgc_occstart, "Using your %s you try to open the tin.",
              aobjnam(uwep, NULL));
    } else {
    no_opener:
        pline(msgc_occstart, "It is not so easy to open this tin.");
        if (slippery_fingers(&youmonst)) {
            pline(msgc_cancelled1, "The tin slips from your %s.",
                  makeplural(body_part(FINGER)));
            if (otmp->quan > 1L) {
                otmp = splitobj(otmp, 1L);
            }
            if (carried(otmp)) {
                unwield_silently(otmp);
                dropx(otmp);
            } else
                stackobj(otmp);
            return FALSE;
        }
        tmp = rn1(1 + 500 / ((int)(ACURR(A_DEX) + ACURRSTR)), 10);
    }
    u.uoccupation_progress[tos_tin] = tmp;
    if (tmp > 50) /* attempts are abandoned after 50 moves */
        u.uoccupation_progress[tos_tin] = -1; /* abandon after 50 moves */
    u.utracked[tos_tin] = otmp;
    return TRUE;
}

/* Called on the "first bite" of rotten food. Returns TRUE to veto the action of
   eating the food. */
static int
rottenfood(struct obj *obj)
{
    pline(msgc_substitute, "Blecch!  Rotten %s!", foodword(obj));
    if (!rn2(4)) {
        if (Hallucination)
            pline(msgc_statusbad, "You feel rather trippy.");
        else
            pline(msgc_statusbad,
                  "You feel rather %s.", body_part(LIGHT_HEADED));
        inc_timeout(&youmonst, CONFUSION, dice(2, 4), TRUE);
    } else if (!rn2(4) && !Blind) {
        pline(msgc_statusbad, "Everything suddenly goes dark.");
        inc_timeout(&youmonst, BLINDED, dice(2, 10), TRUE);
        if (!blind(&youmonst))
            pline(msgc_statusheal, "Your vision quickly clears.");
    } else if (!rn2(3)) {
        const char *what, *where;

        if (!Blind)
            what = "goes", where = "dark";
        else if (Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))
            what = "you lose control of", where = "yourself";
        else
            what = "you slap against the", where =
                (u.usteed) ? "saddle" : surface(u.ux, u.uy);
        pline(msgc_statusbad, "The world spins and %s %s.", what, where);
        helpless(rnd(10), hr_fainted, "unconscious from rotten food", NULL);
        see_monsters(FALSE);
        see_objects(FALSE);
        turnstate.vision_full_recalc = TRUE;
        return 1;
    }
    return 0;
}

/* Returns rotness status. If test is true, return
   corpserot_tainted|sick|ok. Otherwise, return
   the worst possible case. This is for
   pet/interface purposes to try to stop you and
   pets from eating *potentially* tainted corpses. */
enum corpserot
corpse_rot_status(struct obj *obj, boolean test)
{
    int min = 19; /* best case */
    if (!test)
        min = rn2(20);
    boolean allow_sick = TRUE;
    if (!test && !rn2(5))
        allow_sick = FALSE;

    if (obj->corpsenm == PM_LICHEN ||
        obj->corpsenm == PM_LIZARD)
        return corpserot_ok; /* always safe */

    long rot = 0L;
    long maxrot = 0L;
    long age = peek_at_iced_corpse_age(obj);
    rot = (moves - age) / (10L + min);
    maxrot = (moves - age) / 10L;
    rot += bcsign(obj) * 2;
    maxrot += bcsign(obj) * 2;
    if (obj->corpsenm != PM_ACID_BLOB) {
        if (rot > 5L)
            return corpserot_tainted;
        else if (maxrot > 5L && test)
            return corpserot_maybe_tainted;
    }
    if (rot > 3L && allow_sick)
        return corpserot_sick;
    else if (maxrot > 3L && test && allow_sick)
        return corpserot_maybe_sick;
    return corpserot_ok;
}

/* called when a corpse is selected as food */
int
eatcorpse(struct monst *mon, struct obj *otmp)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    int tp = 0, mnum = otmp->corpsenm;
    enum corpserot rot = corpse_rot_status(otmp, FALSE);
    boolean uniq = ! !(mons[mnum].geno & G_UNIQ);
    int retcode = 0;
    boolean stoneable = (touch_petrifies(&mons[mnum]) &&
                         !resists_ston(mon) &&
                         !poly_when_stoned(mon->data));

    /* KMH, conduct */
    if (you) {
        if (!vegan(&mons[mnum]))
            break_conduct(conduct_vegan);
        if (!vegetarian(&mons[mnum]))
            break_conduct(conduct_vegetarian);
    }

    if (mnum != PM_ACID_BLOB && !stoneable &&
        rot == corpserot_tainted) {
        boolean cannibal = FALSE;
        if (you)
            cannibal = maybe_cannibal(mnum, FALSE);

        if (you) {
            pline(resists_sick(mon) ? msgc_playerimmune : msgc_fatal,
                  "Ulch!  That %s was tainted%s!",
                  mons[mnum].mlet ==
                  S_FUNGUS ? "fungoid vegetation" : !vegetarian(&mons[mnum]) ?
                  "meat" : "protoplasm", cannibal ? "; you cannibal" : "");
            if (resists_sick(mon))
                pline(msgc_playerimmune,
                      "It doesn't seem at all sickening, though...");
        }
        if (!resists_sick(mon)) {
            const char *buf;
            long sick_time;

            sick_time = rn2_on_rng(10, you ? rng_ddeath_d10p9 : rng_main) + 9;
            /* make sure new ill doesn't result in improvement */
            if (!uniq)
                buf = msgprintf("a rotted %s", corpse_xname(otmp, TRUE));
            else
                buf = msgprintf("the rotted corpse of %s%s",
                                !type_is_pname(&mons[mnum]) ? "the " : "",
                                mons[mnum].mname);
            make_sick(mon, sick_time, buf, TRUE, SICK_VOMITABLE);
        }
        if (mcarried(otmp))
            m_useup(mon, otmp);
        else if (carried(otmp))
            useup(otmp);
        else
            useupf(otmp, 1L);
        return 2;
    } else if (acidic(&mons[mnum]) && !Acid_resistance) {
        tp++;
        if (you || vis)
            pline(you ? msgc_badidea : msgc_monneutral,
                  "%s %s a very bad case of stomach acid.",
                  you ? "You" : Monnam(mon),
                  you ? "have" : "has");
        /* not body_part() */
        if (you)
            losehp(rnd(15), killer_msg(DIED, "an acidic corpse"));
        else {
            mon->mhp -= rnd(15);
            if (mon->mhp <= 0) {
                mondied(mon);
                retcode = 2;
            }
        }
    } else if (poisonous(&mons[mnum]) && rn2(5)) {
        tp++;
        if (you)
            pline(msgc_intrloss, "Ecch - that must have been poisonous!");
        else if (vis)
            pline(msgc_monneutral, "%s was poisoned by %s meal!",
                  Monnam(mon), mhis(mon));
        if (!resists_poison(mon)) {
            if (you) {
                losestr(rnd(4), DIED, killer_msg(DIED, "a poisonous corpse"), NULL);
                losehp(rnd(15), killer_msg(DIED, "a poisonous corpse"));
            } else {
                mon->mhp -= rnd(15);
                if (mon->mhp <= 0) {
                    mondied(mon);
                    retcode = 2;
                }
            }
        } else if (you || vis)
            pline(you ? msgc_playerimmune : msgc_monneutral,
                  "%s seem%s unaffected by the poison.",
                  you ? "You" : Monnam(mon),
                  you ? "" : "s");
        /* now any corpse left too long will make you mildly ill */
    } else if (rot == corpserot_sick && !resists_sick(mon)) {
        tp++;
        if (you || vis)
            pline(you ? msgc_statusbad : msgc_monneutral,
                  "%s %s %ssick.",
                  you ? "You" : Monnam(mon),
                  you ? "feel" : "looks",
                  (sick(mon)) ? "very " : "");
        if (you)
            losehp(rnd(8), killer_msg(DIED, "a cadaver"));
        else {
            mon->mhp -= rnd(8);
            if (mon->mhp <= 0) {
                mondied(mon);
                retcode = 2;
            }
        }
    }

    if (!tp && mnum != PM_LIZARD && mnum != PM_LICHEN &&
        (otmp->orotten || !rn2(7))) {
        if (you) { /* TODO: rotting for monsters */
            touchfood();
            if (rottenfood(otmp)) {
                if (!u.utracked[tos_food])
                    return 2;

                otmp = u.utracked[tos_food];
                retcode = 1;
            }
            if (u.utracked[tos_food])
                u.utracked[tos_food]->orotten = TRUE;

            if (!mons[otmp->corpsenm].cnutrit) {
                /* no nutrution: rots away, no message if you passed out */
                if (u.utracked[tos_food]) {
                    otmp = u.utracked[tos_food];
                    if (!retcode)
                        pline(msgc_itemloss,
                              "The corpse rots away completely.");
                    if (carried(otmp))
                        useup(otmp);
                    else
                        useupf(otmp, 1L);
                }
                retcode = 2;
            }
        }
    } else if (you) {
        pline(msgc_actionok, "%s%s %s!",
              !uniq ? "This " : !type_is_pname(&mons[mnum]) ? "The " : "",
              food_xname(otmp, FALSE), (vegan(&mons[mnum])
                                        ? (!carnivorous(youmonst.data) &&
                                           herbivorous(youmonst.data))
                                        : (carnivorous(youmonst.data) &&
                                           !herbivorous(youmonst.data)))
              ? "is delicious" : "tastes terrible");
    }

    if (!retcode) {
        cprefx(mon, mnum);
        /* If it was a monster's food, it will be done right away */
        if (!you) {
            cpostfx(mon, mnum);
            if (mcarried(otmp))
                m_useup(mon, otmp);
            else
                useupf(otmp, 1L);
        }
    }

    return retcode;
}

/* Called on "first bite" of (non-corpse) food. Used for non-rotten non-tin
   non-corpse food. */
static void
fprefx(struct obj *otmp)
{
    switch (otmp->otyp) {
    case FOOD_RATION:
        if (u.uhunger <= 200)
            pline(msgc_statusheal,
                  Hallucination ? "Oh wow, like, superior, man!" :
                  "That food really hit the spot!");
        else if (u.uhunger <= 700)
            pline(msgc_statusbad, "That satiated your %s!",
                  body_part(STOMACH));
        break;
    case TRIPE_RATION:
        if (carnivorous(youmonst.data) && !humanoid(youmonst.data))
            pline(msgc_playerimmune,
                  "That tripe ration was surprisingly good!");
        else if (maybe_polyd(is_orc(youmonst.data), Race_if(PM_ORC)))
            pline(msgc_playerimmune,
                  Hallucination ? "Tastes great! Less filling!" :
                  "Mmm, tripe... not bad!");
        else {
            pline(msgc_badidea, "Yak - dog food!");
            more_experienced(1, 0);
            newexplevel();
            /* not cannibalism, but we use similar criteria for deciding
               whether to be sickened by this meal */
            if (rn2(2) && !CANNIBAL_ALLOWED()) {
                unsigned reqtime;
                nutrition_calculations(otmp, NULL, &reqtime, NULL);
                if (!vomiting(&youmonst))
                    set_property(&youmonst, VOMITING, rn1(reqtime, 14), FALSE);
            }
        }
        break;
    case MEATBALL:
    case MEAT_STICK:
    case HUGE_CHUNK_OF_MEAT:
    case MEAT_RING:
        goto give_feedback;
        /* break; */
    case CLOVE_OF_GARLIC:
        if (is_undead(youmonst.data)) {
            unsigned reqtime;
            nutrition_calculations(otmp, NULL, &reqtime, NULL);
            if (!vomiting(&youmonst))
                set_property(&youmonst, VOMITING, rn1(reqtime, 5), FALSE);
            break;
        }
        /* Fall through otherwise */
    default:
        if (otmp->otyp == SLIME_MOLD && !otmp->cursed &&
            otmp->spe == gamestate.fruits.current)
            pline(msgc_actionok, "My, that was a %s %s!",
                  Hallucination ? "primo" : "yummy", singular(otmp, xname));
        else
#ifdef UNIX
        if (otmp->otyp == APPLE || otmp->otyp == PEAR) {
            if (!Hallucination)
                pline(msgc_actionok, "Core dumped.");
            else {
                /* This is based on an old Usenet joke, a fake a.out manual
                   page */
                int x = rnd(100);

                if (x <= 75)
                    pline(msgc_actionok, "Segmentation fault -- core dumped.");
                else if (x <= 99)
                    pline(msgc_actionok, "Bus error -- core dumped.");
                else
                    pline(msgc_actionok, "Yo' mama -- core dumped.");
            }
        } else
#endif
        if (otmp->otyp == EGG && stale_egg(otmp)) {
            /* perhaps others like it */
            pline(msgc_statusbad, "Ugh.  Rotten egg.");
            if (!vomiting(&youmonst))
                set_property(&youmonst, VOMITING, dice(10, 4), FALSE);
        } else
        give_feedback:
            pline(msgc_actionok, "This %s is %s", singular(otmp, xname),
                  otmp->cursed ? (Hallucination ? "grody!" : "terrible!")
                  : (otmp->otyp == CRAM_RATION || otmp->otyp == K_RATION ||
                     otmp->otyp == C_RATION)
                  ? "bland." : Hallucination ? "gnarly!" : "delicious!");
    }
}

static void
accessory_has_effect(struct monst *mon, struct obj *otmp)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);

    if (you)
        pline(msgc_intrgain,
              "Magic spreads through your body as you digest the %s.",
              otmp->oclass == RING_CLASS ? "ring" : "amulet");
    else if (vis)
        pline(msgc_monneutral,"%s seem to radiate magic for a while.",
              Monnam(mon));
}

static void
eataccessory(struct monst *mon, struct obj *otmp)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    int rng_ring = rng_intrinsic_ring;
    int rng_amulet = rng_intrinsic_amulet;
    if (!you) {
        rng_ring = rng_amulet = rng_main;
    }
    int typ = otmp->otyp;

    /* Note: rings are not so common that this is unbalancing. */
    /* (How often do you even _find_ 3 rings of polymorph in a game?) */
    if (otmp == uleft || otmp == uright) {
        setunequip(otmp);
        if (u.uhp <= 0)
            return;     /* died from sink fall */
    }
    if (you)
        otmp->known = otmp->dknown = 1; /* by taste */
    if (otmp->oclass == RING_CLASS ?
        !rn2_on_rng(3, rng_ring) :
        !rn2_on_rng(5, rng_amulet)) {
        switch (otmp->otyp) {
        default:
            if (!objects[typ].oc_oprop)
                break;  /* should never happen */

            /* disallow conflict for monsters */
            if (typ == RIN_CONFLICT && !you)
                break;

            if (set_property(mon, objects[typ].oc_oprop, 0, FALSE))
                accessory_has_effect(mon, otmp);

            break;
        case RIN_ADORNMENT:
            if (you) { /* monsters don't have attributes */
                accessory_has_effect(mon, otmp);
                if (adjattrib(A_CHA, otmp->spe, -1))
                    makeknown(typ);
            }
            break;
        case RIN_GAIN_STRENGTH:
            if (you) {
                accessory_has_effect(mon, otmp);
                if (adjattrib(A_STR, otmp->spe, -1))
                    makeknown(typ);
            }
            break;
        case RIN_GAIN_CONSTITUTION:
            if (you) {
                accessory_has_effect(mon, otmp);
                if (adjattrib(A_CON, otmp->spe, -1))
                    makeknown(typ);
            }
            break;
        case RIN_INCREASE_ACCURACY:
            accessory_has_effect(mon, otmp);
            mon->mhitinc += otmp->spe;
            break;
        case RIN_INCREASE_DAMAGE:
            accessory_has_effect(mon, otmp);
            mon->mdaminc += otmp->spe;
            break;
        case RIN_PROTECTION:
            accessory_has_effect(mon, otmp);
            set_property(mon, PROTECTION, 0, FALSE);
            mon->mac += otmp->spe;
            break;
        case AMULET_OF_CHANGE:
            accessory_has_effect(mon, otmp);
            if (you || vis)
                makeknown(typ);
            if (you)
                change_sex();
            else
                mon->female = mon->female ? 0 : 1;
            if (you)
                pline(msgc_intrloss, "You are suddenly very %s!",
                      u.ufemale ? "feminine" : "masculine");
            else if (vis)
                pline(msgc_monneutral, "%s suddenly looks very %s!",
                      Monnam(mon), mon->female ? "feminine" : "masculine");
            break;
        case AMULET_OF_UNCHANGING:
            /* un-change: it's a pun */
            if (you) {
                if (!Unchanging && Upolyd) {
                    accessory_has_effect(mon, otmp);
                    makeknown(typ);
                    rehumanize(DIED, NULL);
                }
            } else {
                /* TODO: monster unpolymorph */
            }
            break;
        case AMULET_OF_STRANGULATION:  /* bad idea! */
            /* no message--this gives no permanent effect */
            if (you)
                choke(otmp);
            /* TODO: monster choking */
            break;
        case AMULET_OF_LIFE_SAVING:
        case AMULET_OF_REFLECTION:     /* nice try */
            /* can't eat Amulet of Yendor or fakes, and no oc_oprop even if you
               could -3. */
            break;
        }
    }
}

/* Called after eating non-food. */
void
eatspecial(struct monst *mon, int nutrition, struct obj *otmp)
{
    boolean you = (mon == &youmonst);

    /* TODO: move monster nutrition logic to eat.c too */
    if (you)
        lesshungry(nutrition, otmp);

    if (otmp->oclass == COIN_CLASS) {
        if (otmp->where == OBJ_FREE)
            dealloc_obj(otmp);
        else
            useupf(otmp, otmp->quan);
        return;
    }

    if (otmp->oclass == POTION_CLASS) {
        if (!you) {
            impossible("unhandled monster potion eating?");
            return;
        }
        otmp->quan++;   /* dopotion() does a useup() */
        dopotion(otmp);
    }

    if (otmp->oclass == RING_CLASS || otmp->oclass == AMULET_CLASS)
        eataccessory(mon, otmp);
    else if (otmp->otyp == LEASH && otmp->leashmon)
        o_unleash(otmp);

    /* KMH -- idea by "Tommy the Terrorist" */
    if (you && (otmp->otyp == TRIDENT) && !otmp->cursed) {
        pline(msgc_yafm,
              Hallucination ? "Four out of five dentists agree." :
              "That was pure chewing satisfaction!");
        exercise(A_WIS, TRUE);
    }
    if (you && (otmp->otyp == FLINT) && !otmp->cursed) {
        pline(msgc_yafm, "Yabba-dabba delicious!");
        exercise(A_CON, TRUE);
    }

    if (otmp == m_mwep(mon) && otmp->quan == 1L) {
        if (!you) {
            setmnotwielded(mon, mon->mw);
            MON_NOWEP(mon);
        } else
            uwepgone();
    }

    /* only players have quivers/alternate weapons */
    if (you) {
        if (otmp == uquiver && otmp->quan == 1L)
            uqwepgone();
        if (otmp == uswapwep && otmp->quan == 1L)
            uswapwepgone();
    }

    if (otmp == uball)
        unpunish();
    if (otmp == uchain)
        unpunish();     /* but no useup() */
    else if (mcarried(otmp))
        m_useup(mon, otmp);
    else if (carried(otmp))
        useup(otmp);
    else
        useupf(otmp, 1L);
}

/* NOTE: the order of these words exactly corresponds to the
   order of oc_material values #define'd in objclass.h. */
static const char *const foodwords[] = {
    "meal", "liquid", "wax", "food", "meat",
    "paper", "cloth", "leather", "wood", "bone", "scale",
    "metal", "metal", "metal", "silver", "gold", "platinum", "mithril",
    "plastic", "glass", "rich food", "stone"
};

static const char *
foodword(struct obj *otmp)
{
    if (otmp->oclass == FOOD_CLASS)
        return "food";
    if (otmp->oclass == GEM_CLASS && objects[otmp->otyp].oc_material == GLASS &&
        otmp->dknown)
        makeknown(otmp->otyp);
    return foodwords[objects[otmp->otyp].oc_material];
}

/* called after consuming (non-corpse) food */
/* TODO: make this work on monsters */
static void
fpostfx(struct obj *otmp)
{
    switch (otmp->otyp) {
    case SPRIG_OF_WOLFSBANE:
        if (u.ulycn >= LOW_PM || is_were(youmonst.data))
            you_unwere(TRUE);
        break;
    case CARROT:
        set_property(&youmonst, BLINDED, -2, FALSE);
        break;
    case FORTUNE_COOKIE:
        outrumor(bcsign(otmp), BY_COOKIE);
        if (!Blind)
            break_conduct(conduct_illiterate);
        break;
    case LUMP_OF_ROYAL_JELLY:
        /* This stuff seems to be VERY healthy! */
        gainstr(otmp, 1);
        if (Upolyd) {
            u.mh += otmp->cursed ? -rnd(20) : rnd(20);
            if (u.mh > u.mhmax) {
                if (!rn2(17))
                    u.mhmax++;
                u.mh = u.mhmax;
            } else if (u.mh <= 0) {
                rehumanize(POISONING, "a rotten lump of royal jelly");
            }
        } else {
            u.uhp += otmp->cursed ? -rnd(20) : rnd(20);
            if (u.uhp > u.uhpmax) {
                if (!rn2(17))
                    u.uhpmax++;
                u.uhp = u.uhpmax;
            } else if (u.uhp <= 0) {
                done(POISONING,
                     killer_msg(POISONING, "a rotten lump of royal jelly"));
            }
        }
        if (!otmp->cursed && leg_hurt(&youmonst))
            heal_legs(&youmonst, leg_hurtsides(&youmonst));
        break;
    case EGG:
        if (!petrifying(&youmonst) && touched_monster(otmp->corpsenm)) {
            set_property(&youmonst, STONED, 5, TRUE);
            set_delayed_killer(STONING, killer_msg_obj(STONING, otmp));
        }
        break;
    case EUCALYPTUS_LEAF:
        if (!otmp->cursed) {
            set_property(&youmonst, SICK, -2, FALSE);
            set_property(&youmonst, ZOMBIE, -2, FALSE);
            set_property(&youmonst, VOMITING, -2, FALSE);
        }
        break;
    }
    return;
}

/*
 * return 0 if the food was not dangerous.
 * return 1 if the food was dangerous and you chose to stop.
 * return 2 if the food was dangerous and you chose to eat it anyway.
 *
 * Now triggers on all eating, but gives only vague information and is
 * overly cautious without edibility turned on. The general rule is that
 * edibility can magically check things that would require knowledge of
 * user-invisible information, without it it has to go on public info.
 * (An exception's made for age, where it's overly cautious rather than
 * leaving it out altogether to help avoid instadeath accidents.)
 */
static int
edibility_prompts(struct obj *otmp)
{
    /* blessed food detection granted you a one-use ability to detect food that
       is unfit for consumption or dangerous and avoid it. */

    const char *buf, *foodsmell, *it_or_they, *eat_it_anyway;
    boolean cadaver = (otmp->otyp == CORPSE), stoneorslime = FALSE;
    int material = objects[otmp->otyp].oc_material, mnum = otmp->corpsenm;
    long rotted = 0L;

    foodsmell = Tobjnam(otmp, "smell");
    it_or_they = (otmp->quan == 1L) ? "it" : "they";
    eat_it_anyway = msgprintf(
        "Eat %s anyway?",
        (otmp->quan == 1L || otmp->oclass == COIN_CLASS) ? "it" : "one");

    /* edibility's needed to ID the contents of eggs and tins */
    if (cadaver || (otmp->otyp == EGG && u.uedibility) ||
        (otmp->otyp == TIN && u.uedibility)) {
        /* These checks must match those in eatcorpse() */
        stoneorslime = (touch_petrifies(&mons[mnum]) && !Stone_resistance &&
                        !poly_when_stoned(youmonst.data));

        if (mnum == PM_GREEN_SLIME)
            stoneorslime = (!Unchanging && !flaming(youmonst.data) &&
                            youmonst.data != &mons[PM_GREEN_SLIME]);

        if (cadaver && mnum != PM_LIZARD && mnum != PM_LICHEN) {
            long age = peek_at_iced_corpse_age(otmp);

            /* worst case rather than random in this calculation to force
               prompt */
            rotted = (moves - age) / (10L + 0 /* was rn2(20) */ );
            if (otmp->cursed && u.uedibility)
                rotted += 2L;
            else if (otmp->blessed && u.uedibility)
                rotted -= 2L;
        }
    }

    /*
     * These problems with food should be checked in
     * order from most detrimental to least detrimental.
     */

    /* without edibility, you don't know BCU, so deduct 2 in case it's cursed */
    if (cadaver && mnum != PM_ACID_BLOB && rotted > (u.uedibility ? 5L : 3L) &&
        !Sick_resistance) {
        /* Tainted meat */
        if (u.uedibility)
            buf = msgprintf("%s like %s could be tainted! %s",
                            foodsmell, it_or_they, eat_it_anyway);
        else
            buf = msgprintf("%s too old for you to be certain %s %s "
                            "safe. %s", foodsmell, it_or_they,
                            (otmp->quan == 1L ? "is" : "are"), eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        /* otherwise fall through */
    }

    if (u.uhs == SATIATED) {
        buf = msgprintf("You are not really in the mood to eat. %s",
                        eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        /* otherwise fall through */
    }

    if (stoneorslime) {
        buf = msgprintf("%s like %s could be something very dangerous! %s",
                        foodsmell, it_or_they, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (u.uedibility && (otmp->orotten || (cadaver && rotted > 3L))) {
        /* Rotten */
        buf = msgprintf("%s like %s could be rotten! %s", foodsmell,
                        it_or_they, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (cadaver && poisonous(&mons[mnum]) && !Poison_resistance) {
        /* poisonous */
        buf = msgprintf("%s like %s might be poisonous! %s", foodsmell,
                        it_or_they, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (cadaver && !vegetarian(&mons[mnum]) &&
        !u.uconduct[conduct_vegetarian] && Role_if(PM_MONK)) {
        buf = msgprintf("%s unsuitable for a vegetarian monk. %s",
                        foodsmell, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    /* HP check is needed to stop this being annoying */
    if (cadaver && acidic(&mons[mnum]) && !Acid_resistance && u.uhp < 20) {
        buf = msgprintf("%s rather acidic, and you're low on health. %s",
                        foodsmell, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (Upolyd && u.umonnum == PM_RUST_MONSTER && is_metallic(otmp) &&
        otmp->oerodeproof && u.uedibility) {
        buf = msgprintf("%s disgusting to you right now. %s", foodsmell,
                        eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }


    /*
     * Breaks conduct, but otherwise safe.
     */
    if (!u.uconduct[conduct_vegetarian] && moves > 1800 &&
        ((material == LEATHER || material == BONE || material == DRAGON_HIDE) ||
         (cadaver && !vegetarian(&mons[mnum])))) {
        buf = msgprintf("%s of meat. %s", foodsmell, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }

    if (!u.uconduct[conduct_vegan] && moves > 1800 &&
        ((material == LEATHER || material == BONE || material == DRAGON_HIDE ||
          material == WAX) || (cadaver && !vegan(&mons[mnum])))) {
        buf = msgprintf("%s like an animal byproduct. %s", foodsmell,
                        eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }

    if (cadaver && mnum != PM_ACID_BLOB && rotted > 5L && Sick_resistance &&
        u.uedibility) {
        /* Tainted meat with Sick_resistance */
        buf = msgprintf("%s like %s could be tainted! %s", foodsmell,
                        it_or_they, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    return 0;
}

/* generic "eat" command funtion (see cmd.c) */
int
doeat(const struct nh_cmd_arg *arg)
{
    int basenutrit;     /* nutrition of full item */
    boolean dont_start = FALSE;
    struct obj *otmp;

    if (strangled(&youmonst)) {
        pline(msgc_cancelled,
              "If you can't breathe air, how can you consume solids?");
        return 0;
    }
    /* In the case of a continued action, continue eating the existing object if
       we can; it must either be in inventory, or on the floor at our
       location. Otherwise, fall through to floorfood(), which will read an
       object from the argument if specified, and otherwise prompt. (We need to
       special-case turnstate.continue_message because otherwise it would prompt
       every turn for floor items, and partially eat each item in a stack in
       inventory before finishing.) */
    if (!turnstate.continue_message && obj_with_u(u.utracked[tos_food]))
        otmp = u.utracked[tos_food];
    else
        otmp = floorfood("eat", arg);
    if (!otmp)
        return 0;
    if (check_capacity(NULL))
        return 0;

    /* We have to make non-foods take 1 move to eat, unless we want to do
       ridiculous amounts of coding to deal with partly eaten plate mails,
       players who polymorph back to human in the middle of their metallic
       meal, etc.... */
    if (!is_edible(otmp, TRUE)) {
        pline(msgc_mispaste, "You cannot eat that!");
        return 0;
    } else if ((otmp->owornmask & (W_ARMOR | W_MASK(os_tool) |
                                   W_MASK(os_amul) | W_MASK(os_saddle))) != 0) {
        /* let them eat rings */
        pline(msgc_cancelled, "You can't eat something you're wearing.");
        return 0;
    }
    if (is_metallic(otmp) && u.umonnum == PM_RUST_MONSTER &&
        otmp->oerodeproof) {
        otmp->rknown = TRUE;
        if (otmp->quan > 1L) {
            if (!carried(otmp))
                splitobj(otmp, otmp->quan - 1L);
            else
                otmp = splitobj(otmp, 1L);
        }
        pline(msgc_statusbad, "Ulch!  That %s was rustproofed!", xname(otmp));
        /* The regurgitated object's rustproofing is gone now */
        otmp->oerodeproof = 0;
        inc_timeout(&youmonst, STUNNED, rn2(10), FALSE);
        pline(msgc_consequence, "You spit %s out onto the %s.",
              the(xname(otmp)),
              surface(u.ux, u.uy));
        if (carried(otmp)) {
            unwield_silently(otmp);
            freeinv(otmp);
            dropy(otmp);
        }
        stackobj(otmp);
        return 1;
    }
    /* KMH -- Slow digestion is... indigestible */
    if (otmp->otyp == RIN_SLOW_DIGESTION) {
        pline(msgc_statusbad, "This ring is indigestible!");
        rottenfood(otmp);
        if (otmp->dknown && !objects[otmp->otyp].oc_name_known &&
            !objects[otmp->otyp].oc_uname)
            docall(otmp);
        return 1;
    }
    if (otmp->oclass != FOOD_CLASS) {
        int material;

        u.utracked[tos_food] = NULL;

        if (!touch_artifact(otmp, &youmonst))
            return 1;
        if (!is_edible(otmp, TRUE)) {
            pline(msgc_interrupted, "You can no longer eat %s.", doname(otmp));
            return 1;
        }

        /* Note: gold weighs 1 pt. for each 100 pieces (see pickup.c) so gold
        and non-gold is consistent. */
        if (otmp->oclass == COIN_CLASS)
            basenutrit =
                ((otmp->quan > 200000L) ? 2000 : (int)(otmp->quan / 100L));
        else if (otmp->oclass == BALL_CLASS || otmp->oclass == CHAIN_CLASS)
            basenutrit = weight(otmp);
        /* oc_nutrition is usually weight anyway */
        else
            basenutrit = objects[otmp->otyp].oc_nutrition;

        material = objects[otmp->otyp].oc_material;
        if (material == LEATHER || material == BONE ||
            material == DRAGON_HIDE) {
            break_conduct(conduct_vegan);
            break_conduct(conduct_vegetarian);
        } else if (material == WAX)
            break_conduct(conduct_vegan);
        break_conduct(conduct_food);

        if (otmp->cursed)
            rottenfood(otmp);

        if (otmp->oclass == WEAPON_CLASS && otmp->opoisoned) {
            if (!Poison_resistance) {
                pline(msgc_intrloss, "Ecch - that must have been poisonous!");
                losestr(rnd(4), DIED, killer_msg_obj(DIED, otmp), NULL);
                losehp(rnd(15), killer_msg_obj(DIED, otmp));
            } else /* now a single message, as with the poisonous() check for
                      corpses, but a different one for ID knowledge reasons */
                pline(msgc_playerimmune, "Was that thing poisoned?");
        } else if (!otmp->cursed)
            pline(msgc_actionok, "This %s is delicious!",
                  otmp->oclass == COIN_CLASS ? foodword(otmp) :
                  singular(otmp, xname));

        eatspecial(&youmonst, basenutrit, otmp);
        return 1;
    }

    /* We could either be continuing a previous meal, or starting anew. It
       counts as continuing if the item in question is still in
       u.utracked[tos_food]. We need to clear tos_food when changing meals so
       that if the player repeats eating a tin, they get another tin, rather
       than a partly eaten meal they were eating earlier.
     */
    if (otmp != u.utracked[tos_food])
        u.utracked[tos_food] = 0;

    if (otmp == u.utracked[tos_food]) {
        /* Continuing a meal. */
        if (turnstate.continue_message)
            pline(msgc_occstart, "You resume your meal.");

        /* 3.4.3 indirectly has a cprefx check here, but it doesn't make sense
           that the number of cannibalism penalties you get depends on how many
           times you get interrupted. TODO: We still want to check some things,
           like stoning. Probably cprefx needs an argument. */
    } else if (otmp->otyp == TIN) {
        /* special case */
        if (otmp != u.utracked[tos_tin] && !start_tin(otmp))
            return 1; /* something went wrong */

        one_occupation_turn(eat_tin_one_turn, "opening the tin", occ_tin);
        return 1;
    } else {
        /* Starting a new meal (including a partly eaten meal, if the eating
           state got reset in the meantime). */

        {
            int res = edibility_prompts(otmp);

            if (res && u.uedibility) {
                pline(msgc_intrloss,
                      "Your %s stops tingling and your "
                      "sense of smell returns to normal.", body_part(NOSE));
                u.uedibility = 0;
            }
            if (res == 1)
                return 0;
        }

        if (u.uhunger >= 2000) {
            choke(u.utracked[tos_food]);
            return 0;
        }

        u.utracked[tos_food] = otmp;

        if (otmp->otyp == CORPSE) {
            int tmp = eatcorpse(&youmonst, otmp);

            if (tmp == 2) {
                /* used up */
                u.utracked[tos_food] = NULL;
                return 1;
            } else if (tmp)
                dont_start = TRUE;
            /* if not used up, eatcorpse may modify oeaten */
        } else {
            /* No checks for WAX, LEATHER, BONE, DRAGON_HIDE. These are all
               handled in the != FOOD_CLASS case, above */
            switch (objects[otmp->otyp].oc_material) {
            case FLESH:
                break_conduct(conduct_vegan);
                if (otmp->otyp != EGG) {
                    break_conduct(conduct_vegetarian);
                }
                break;

            default:
                if (otmp->otyp == PANCAKE ||
                    otmp->otyp == FORTUNE_COOKIE || /* eggs */
                    otmp->otyp == CREAM_PIE ||
                    otmp->otyp == CANDY_BAR ||   /* milk */
                    otmp->otyp == LUMP_OF_ROYAL_JELLY)
                    break_conduct(conduct_vegan);
                break;
            }

            if (otmp->otyp != FORTUNE_COOKIE &&
                (otmp->cursed ||
                 (((moves - otmp->age) > (otmp->blessed ? 50 : 30)) &&
                  (otmp->orotten || !rn2(7))))) {
                if (rottenfood(otmp))
                    dont_start = TRUE;
                touchfood();
                if (u.utracked[tos_food])
                    u.utracked[tos_food]->orotten = TRUE;

                /* In 4.2, the nutrition is halved at this point, but that leads
                   to the nutrition being halved repeatedly if the player is
                   interrupted repeatedly. In 4.3, we instead halve the
                   nutrition gained from each bite individually via the orotten
                   flag. */
            } else
                fprefx(otmp);
        }

        /* Conduct check has been moved here for 3.4.3 so that you aren't
           charged for the conduct per-bite. */
        break_conduct(conduct_food);
    }

    /* Mark the food as partly eaten, and move it into a stack of its own, so
       that it can be gradually eaten over time. Technically speaking, we don't
       always need to do this, but it's idempotent so we may as well. */
    touchfood();
    if (!u.utracked[tos_food])
        dont_start = 1;

    if (dont_start)
        action_completed();
    else
        one_occupation_turn(eat_one_turn, "eating", occ_food);

    return 1;
}

/* as time goes by - called by moveloop() and domove() */
void
gethungry(void)
{
    if (u.uinvulnerable)
        return; /* you don't feel hungrier */

    if ((!u_helpless(hm_asleep) || !rn2(10)) /* slow metabolism while asleep */
        &&(carnivorous(youmonst.data) || herbivorous(youmonst.data))
        && !Slow_digestion)
        u.uhunger--;    /* ordinary food consumption */

    if (moves % 2) {    /* odd turns */
        /* Regeneration uses up food, unless due to an artifact.  Note: assumes
           that only artifacts can confer regneration via wield. */
        if (regenerates(&youmonst) & ~(W_ARTIFACT | W_MASK(os_wep)))
            u.uhunger--;
        if (near_capacity() > SLT_ENCUMBER)
            u.uhunger--;
    } else {    /* even turns */
        if (hunger(&youmonst))
            u.uhunger--;
        /* Conflict uses up food too */
        if (conflicting(&youmonst) & ~(W_ARTIFACT | W_MASK(os_wep)))
            u.uhunger--;
        /* +0 charged rings don't do anything, so don't affect hunger */
        /* Slow digestion still uses ring hunger (it suppresses normal hunger,
           leaving the character on ring hunger only) */
        switch ((int)(moves % 20)) {    /* note: use even cases only */
        case 4:
            if (uleft && (uleft->spe || !objects[uleft->otyp].oc_charged))
                u.uhunger--;
            break;
        case 8:
            if (uamul)
                u.uhunger--;
            break;
        case 12:
            if (uright && (uright->spe || !objects[uright->otyp].oc_charged))
                u.uhunger--;
            break;
        case 16:
            if (Uhave_amulet)
                u.uhunger--;
            break;
        default:
            break;
        }
    }
    newuhs(TRUE);
}

/* called after vomiting and after performing feats of magic */
void
morehungry(int num)
{
    u.uhunger -= num;
    newuhs(TRUE);
}

/* called after eating (and after drinking fruit juice) */
static void
lesshungry(int num, struct obj *otmp)
{
    u.uhunger += num;

    /* Have lesshungry() report when you're nearly full so all eating warns
       when you're about to choke. Exception: fruit juice. This now uses
       a new interface (stop automatically, continue with control-A). */
    if (u.uhunger >= 1500 && u.uhunger - num < 1500) {
        pline(msgc_fatal, "You're having a hard time getting all of it down.");
        /* Hack: the interruption can mean that you drop just back under
           1500 for next turn and get warned again. Using a range would mean
           that sometimes the warning doesn't appear, so instead we add on
           10 points of nutrition so that we don't slip back below 1500
           immediately. TODO: Some better way to do this. */
        u.uhunger += 10;

        action_completed();

        /* TODO: "You stop eating." message if the player hasn't finished
           eating already. */
    }

    newuhs(FALSE);
}


/* compute and comment on your (new?) hunger status */
void
newuhs(boolean incr)
{
    unsigned newhs;
    int h = u.uhunger;

    newhs = (h > 1000) ? SATIATED :
        (h > 150) ? NOT_HUNGRY :
        (h > 50) ? HUNGRY :
        (h > 0) ? WEAK : FAINTING;

    /* This code previously had a lot of complexity to omit the message for
       weak->hungry if you were going weak->default. Instead of dealing with the
       fragility of the code, the problem has been solved for 4.3 via rewording
       the messages so that they make sense in this case. */

    if (newhs == FAINTING) {
        if (u.uhs <= WEAK || rn2(20 - u.uhunger / 10) >= 19) {
            if (u.uhs != FAINTED && !u_helpless(hm_all)) {
                /* stop what you're doing, then faint */
                action_interrupted();
                pline(msgc_fatal, "You faint from lack of food.");
                newhs = u.uhs = FAINTED;
                helpless(10 - (u.uhunger / 10), hr_fainted,
                         "fainted from lack of food", NULL);
                /* cancel_helplessness puts it back to FAINTING if the character
                   is revived for any reason */
            }
        } else if (u.uhunger < -(int)(200 + 20 * ACURR(A_CON))) {
            u.uhs = STARVED;
            bot();
            pline(msgc_fatal_predone, "You die from starvation.");
            done(STARVING, killer_msg(STARVING, "starvation"));
            /* if we return, we lifesaved, and that calls newuhs */
            return;
        }
    }

    if (newhs != u.uhs) {
        enum msg_channel msgc = incr ? msgc_statusbad : msgc_statusheal;
        switch (newhs) {
        case HUNGRY:
            if (Hallucination) {
                pline(msgc, (!incr) ? "Your munchies are not as bad now." :
                      "You are getting the munchies.");
            } else
                pline(msgc, (!incr) ? "You don't feel so weak now." :
                      (u.uhunger < 145) ? "You feel hungry." :
                      "You are beginning to feel hungry.");
            if (incr && flags.occupation != occ_food)
                action_interrupted();
            break;
        case WEAK:
            if (Hallucination)
                pline(msgc, (!incr) ? "You still have the munchies." :
                      "The munchies are interfering with your motor "
                      "capabilities.");
            else if (incr &&
                     (Role_if(PM_WIZARD) || Race_if(PM_ELF) ||
                      Role_if(PM_VALKYRIE)))
                pline(msgc_fatal, "%s needs food, badly!",
                      (Role_if(PM_WIZARD) ||
                       Role_if(PM_VALKYRIE)) ? urole.name.m : "Elf");
            else
                pline(incr ? msgc_fatal : msgc_statusheal,
                      (!incr) ? "You feel less faint." : (u.uhunger < 45) ?
                      "You feel weak." : "You are beginning to feel weak.");
            if (incr && flags.occupation != occ_food)
                action_interrupted();
            break;
        }
        if (newhs >= WEAK && u.uhs < WEAK)
            losestr(1, STARVING, killer_msg(STARVING, "exhaustion"), NULL);
        else if (newhs < WEAK && u.uhs >= WEAK)
            losestr(-1, STARVING, killer_msg(STARVING, "exhaustion"), NULL);
        u.uhs = newhs;
        bot();
    }
}

boolean
can_sacrifice(const struct obj *otmp)
{
    return (otmp->otyp == CORPSE || otmp->otyp == AMULET_OF_YENDOR ||
            otmp->otyp == FAKE_AMULET_OF_YENDOR);
}

static boolean
other_floorfood(const struct obj *otmp)
{
    return otmp->oclass == FOOD_CLASS;
}

/* Returns an object on floor or in inventory. This is used for eating,
   sacrificing and tinning corpses, and has some special cases for each. */
struct obj *
floorfood(const char *verb, const struct nh_cmd_arg *arg)
{
    struct obj *otmp;
    const char *qbuf;
    char c;
    struct trap *ttmp = t_at(level, u.ux, u.uy);
    boolean feeding = (!strcmp(verb, "eat"));
    boolean sacrificing = (!strcmp(verb, "sacrifice"));
    boolean tinning = (!strcmp(verb, "tin"));
    boolean can_floorfood = FALSE;
    boolean checking_can_floorfood = TRUE;
    boolean (*floorfood_check)(const struct obj *);

    if (!verb || !*verb)
        impossible("floorfood: no verb given");

    floorfood_check = (sacrificing ? can_sacrifice :
                       tinning ? tinnable :
                       feeding ? is_edible_now : other_floorfood);

    /* if we can't touch floor objects then use invent food only */
    if (!can_reach_floor() || (feeding && u.usteed) ||  /* can't eat off floor
                                                           while riding */
        ((is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy)) &&
         (Wwalking || is_clinger(youmonst.data) || (Flying && !Breathless))) ||
        (ttmp && ttmp->tseen && (ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT)
         && (!u.utrap || (u.utrap && u.utraptype != TT_PIT)) && !Passes_walls))
        goto skipfloor;

eat_floorfood:
    if (feeding && metallivorous(youmonst.data)) {
	/* Two passes:
	 *
	 * 1) Check if anything on the floor can be chosen and make it available
	 *    from the object picking prompt.
	 * 2) If the floor was chosen (,) from that prompt, go through again,
	 *    this time asking for the specific floor option.
	 */

        if (ttmp && ttmp->tseen && ttmp->ttyp == BEAR_TRAP) {
            if (!checking_can_floorfood) {
                /* If not already stuck in the trap, perhaps there should be a
                   chance to becoming trapped? Probably not, because then the
                   trap would just get eaten on the _next_ turn... */
                qbuf = msgprintf("There is a bear trap here (%s); eat it?",
                                 (u.utrap && u.utraptype == TT_BEARTRAP) ?
                                 "holding you" : "armed");
                if ((c = yn_function(qbuf, ynqchars, 'n')) == 'y') {
                    u.utrap = u.utraptype = 0;
                    deltrap(level, ttmp);
                    return mksobj(level, BEARTRAP, TRUE, FALSE, rng_main);
                } else if (c == 'q') {
                    return NULL;
                }
            } else
                can_floorfood++;
        }
    }

    if (checking_can_floorfood) {
        for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere) {
            if ((*floorfood_check)(otmp)) {
                can_floorfood = TRUE;
                break;
            }
        }
    } else {
        struct object_pick *floorfood_list;
        int n;

        qbuf = msgprintf("%c%s what?", highc(*verb), verb + 1);
        n = query_objlist(qbuf, level->objects[u.ux][u.uy],
                          BY_NEXTHERE | INVORDER_SORT | AUTOSELECT_SINGLE,
                          &floorfood_list, PICK_ONE, floorfood_check);
        if (n) {
            otmp = floorfood_list[0].obj;
            free(floorfood_list);
        } else {
            otmp = NULL;
        }
        return otmp;
    }

skipfloor:
    /* We cannot use ALL_CLASSES since that causes getobj() to skip its "ugly
       checks" and we need to check for inedible items.

       An arg of NULL means that we should use a nonrepeatable prompt, rather
       than a command argument. */
    if (arg)
        otmp = getargobj(arg, feeding ?
                         (const char *)(allobj + (can_floorfood ? 0 : 2)) :
                         (const char *)(comestibles + (can_floorfood ? 0 : 2)),
                         verb);
    else
        otmp = getobj(feeding ?
                      (const char *)(allobj + (can_floorfood ? 0 : 2)) :
                      (const char *)(comestibles + (can_floorfood ? 0 : 2)),
                      verb, FALSE);
    if (otmp == &zeroobj) {
        checking_can_floorfood = FALSE;
        goto eat_floorfood;
    }

    if (otmp && !(*floorfood_check)(otmp)) {
        pline(msgc_mispaste, "You can't %s that!", verb);
        return NULL;
    }
    return otmp;
}

/* Side effects of vomiting */
/* added helplessness (MRS) - it makes sense, you're too busy being sick! */
void
vomit(struct monst *mon)
{       /* A good idea from David Neves */
    make_sick(mon, 0L, NULL, TRUE, SICK_VOMITABLE);
    if (mon == &youmonst)
        helpless(2, hr_busy, "vomiting", "You're done throwing up.");
    else {
        mon->mcanmove = 0;
        mon->mfrozen += 2;
    }
}

int
eaten_stat(int base, struct obj *obj)
{
    unsigned uneaten_amt, full_amount;

    uneaten_amt = (long)obj->oeaten;
    nutrition_calculations(obj, &full_amount, NULL, NULL);
    full_amount =
         (obj->otyp == CORPSE) ? (long)mons[obj->corpsenm].cnutrit :
         (long)objects[obj->otyp].oc_nutrition;

    /* can happen with a partly eaten wraith corpse; those shouldn't exist, but
       if one comes into being somehow (e.g. wizwish)... */
    if (uneaten_amt > full_amount)
        uneaten_amt = full_amount;

    base = (int)(full_amount ?
                 (long)base * (long)uneaten_amt / (long)full_amount :
                 0L);
    return (base < 1) ? 1 : base;
}

/*eat.c*/

