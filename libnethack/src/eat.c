/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int eatmdone(void);
static int eatfood(void);
static void costly_tin(const char *);
static int opentin(void);
static int unfaint(void);

static const char *food_xname(struct obj *, boolean);
static void choke(struct obj *);
static void recalc_wt(void);
static struct obj *touchfood(struct obj *);
static void do_reset_eat(void);
static void done_eating(boolean);
static void cprefx(int);
static int intrinsic_possible(int, const struct permonst *);
static void givit(int, const struct permonst *ptr);
static void cpostfx(int);
static void start_tin(struct obj *);
static int eatcorpse(struct obj *);
static void start_eating(struct obj *);
static void fprefx(struct obj *);
static void accessory_has_effect(struct obj *);
static void fpostfx(struct obj *);
static int bite(void);
static int edibility_prompts(struct obj *);
static int rottenfood(struct obj *);
static void eatspecial(void);
static void eataccessory(struct obj *);
static const char *foodword(struct obj *);
static boolean maybe_cannibal(int, boolean);
static void lesshungry(int);

char msgbuf[BUFSZ];


/* hunger texts used on bottom line (each 8 chars long) */
#define SATIATED        0
#define NOT_HUNGRY      1
#define HUNGRY          2
#define WEAK            3
#define FAINTING        4
#define FAINTED         5
#define STARVED         6

/* also used to see if you're allowed to eat cats and dogs */
#define CANNIBAL_ALLOWED() (Role_if (PM_CAVEMAN) || Race_if(PM_ORC))

static const char comestibles[] = { ALLOW_NONE, NONE_ON_COMMA, FOOD_CLASS, 0 };

static const char allobj[] = {
    ALLOW_NONE, NONE_ON_COMMA,
    COIN_CLASS, WEAPON_CLASS, ARMOR_CLASS, POTION_CLASS, SCROLL_CLASS,
    WAND_CLASS, RING_CLASS, AMULET_CLASS, FOOD_CLASS, TOOL_CLASS,
    GEM_CLASS, ROCK_CLASS, BALL_CLASS, CHAIN_CLASS, SPBOOK_CLASS, 0
};

static boolean force_save_hs = FALSE;
static unsigned newuhs_save_hs;
static boolean newuhs_saved_hs = FALSE;


/*
 * Decide whether a particular object can be eaten by the possibly
 * polymorphed character.  Not used for monster checks.
 */
boolean
is_edible(struct obj *obj)
{
    /* protect invocation tools but not Rider corpses (handled elsewhere) */
    /* if (obj->oclass != FOOD_CLASS && obj_resists(obj, 0, 0)) */
    if (objects[obj->otyp].oc_unique)
        return FALSE;
    /* above also prevents the Amulet from being eaten, so we must never allow
       fake amulets to be eaten either [which is already the case] */

    if (metallivorous(youmonst.data) && is_metallic(obj) &&
        (youmonst.data != &mons[PM_RUST_MONSTER] || is_rustprone(obj)))
        return TRUE;
    if (u.umonnum == PM_GELATINOUS_CUBE && is_organic(obj) &&
        /* [g.cubes can eat containers and retain all contents as engulfed
           items, but poly'd player can't do that] */
        !Has_contents(obj))
        return TRUE;

    /* return (boolean)(!!strchr(comestibles, obj->oclass)); */
    return (boolean) (obj->oclass == FOOD_CLASS);
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
    {
    "deep fried", 60}, {
    "pickled", 40}, {
    "soup made from", 20}, {
    "pureed", 500},
#define ROTTEN_TIN 4
    {
    "rotten", -50},
#define HOMEMADE_TIN 5
    {
    "homemade", 50}, {
    "stir fried", 80}, {
    "candied", 100}, {
    "boiled", 50}, {
    "dried", 55}, {
    "szechuan", 70},
#define FRENCH_FRIED_TIN 11
    {
    "french fried", 40}, {
    "sauteed", 95}, {
    "broiled", 80}, {
    "smoked", 50}, {
    "", 0}
};

#define TTSZ    SIZE(tintxts)

static struct tin {
    struct obj *tin;
    int usedtime, reqtime;
} tin;

static struct victual {
    struct obj *piece;  /* the thing being eaten, or last thing that was
                           partially eaten, unless that thing was a tin, which
                           uses the tin structure above, in which case this
                           should be 0 */
    /* doeat() initializes these when piece is valid */
    int usedtime,       /* turns spent eating */
        reqtime;        /* turns required to eat */
    int nmod;   /* coded nutrition per turn */
    unsigned canchoke:1;        /* was satiated at beginning */

    /* start_eating() initializes these */
    unsigned fullwarn:1;        /* have warned about being full */
    unsigned eating:1;  /* victual currently being eaten */
    unsigned doreset:1; /* stop eating at end of turn */
} victual;

static char *eatmbuf = 0;       /* set by cpostfx() */

static int
eatmdone(void)
{       /* called after mimicing is over */
    /* release `eatmbuf' */
    if (eatmbuf) {
        if (nomovemsg == eatmbuf)
            nomovemsg = 0;
        free(eatmbuf), eatmbuf = 0;
    }
    /* update display */
    if (youmonst.m_ap_type) {
        youmonst.m_ap_type = M_AP_NOTHING;
        newsym(u.ux, u.uy);
    }
    return 0;
}

/* ``[the(] singular(food, xname) [)]'' with awareness of unique monsters */
static const char *
food_xname(struct obj *food, boolean the_pfx)
{
    const char *result;
    int mnum = food->corpsenm;

    if (food->otyp == CORPSE && (mons[mnum].geno & G_UNIQ)) {
        /* grab xname()'s modifiable return buffer for our own use */
        char *bufp = xname(food);

        sprintf(bufp, "%s%s corpse",
                (the_pfx &&
                 !type_is_pname(&mons[mnum])) ? "the " : "",
                s_suffix(mons[mnum].mname));
        result = bufp;
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
        pline("You feel like a glutton!");
    }

    exercise(A_CON, FALSE);

    if (Breathless || (!Strangled && !rn2(20))) {
        /* choking by eating AoS doesn't involve stuffing yourself */
        if (food && food->otyp == AMULET_OF_STRANGULATION) {
            pline("You choke, but recover your composure.");
            return;
        }
        pline("You stuff yourself and then vomit voluminously.");
        morehungry(1000);       /* you just got *very* sick! */
        nomovemsg = 0;
        vomit();
    } else {
        killer_format = KILLED_BY_AN;
        /* 
         * Note all "killer"s below read "Choked on a/an %s" on the
         * high score list & tombstone.  So plan accordingly.
         */
        if (food) {
            pline("You choke over your %s.", foodword(food));
            if (food->oclass == COIN_CLASS) {
                killer = "very rich meal";
            } else {
                killer = food_xname(food, FALSE);
                if (food->otyp == CORPSE &&
                    (mons[food->corpsenm].geno & G_UNIQ)) {
                    if (!type_is_pname(&mons[food->corpsenm]))
                        killer = the(killer);
                    killer_format = KILLED_BY;
                }
            }
        } else {
            pline("You choke over it.");
            killer = "quick snack";
        }
        pline("You die...");
        done(CHOKING);
    }
}

/* modify object wt. depending on time spent consuming it */
static void
recalc_wt(void)
{
    struct obj *piece = victual.piece;

    piece->owt = weight(piece);
}

void
reset_eat(void)
{       /* called when eating interrupted by an event */
    /* we only set a flag here - the actual reset process is done after the
       round is spent eating. */
    if (victual.eating && !victual.doreset)
        victual.doreset = TRUE;

    return;
}

static struct obj *
touchfood(struct obj *otmp)
{
    if (otmp->quan > 1L) {
        if (!carried(otmp))
            splitobj(otmp, otmp->quan - 1L);
        else
            otmp = splitobj(otmp, 1L);
    }

    if (!otmp->oeaten) {
        if (((!carried(otmp) && costly_spot(otmp->ox, otmp->oy) &&
              !otmp->no_charge)
             || otmp->unpaid)) {
            /* create a dummy duplicate to put on bill */
            verbalize("You bit it, you bought it!");
            bill_dummy_object(otmp);
        }
        otmp->oeaten =
            (otmp->otyp ==
             CORPSE ? mons[otmp->corpsenm].cnutrit : objects[otmp->otyp].
             oc_nutrition);
    }

    if (carried(otmp)) {
        freeinv(otmp);
        otmp->oxlth++;      /* hack to prevent merge */
        if (!can_hold(otmp)) {
            sellobj_state(SELL_DONTSELL);
            dropy(otmp);
            sellobj_state(SELL_NORMAL);
        } else {
            otmp = addinv(otmp);
        }
        otmp->oxlth--;
    }
    return otmp;
}

/* When food decays, in the middle of your meal, we don't want to dereference
 * any dangling pointers, so set it to null (which should still trigger
 * do_reset_eat() at the beginning of eatfood()) and check for null pointers
 * in do_reset_eat().
 */
void
food_disappears(struct obj *obj)
{
    if (obj == victual.piece)
        victual.piece = NULL;
    if (obj->timed)
        obj_stop_timers(obj);
}

/* renaming an object usually results in it having a different address;
   so the sequence start eating/opening, get interrupted, name the food,
   resume eating/opening would restart from scratch */
void
food_substitution(struct obj *old_obj, struct obj *new_obj)
{
    if (old_obj == victual.piece)
        victual.piece = new_obj;
    if (old_obj == tin.tin)
        tin.tin = new_obj;
}

static void
do_reset_eat(void)
{
    if (victual.piece) {
        victual.piece = touchfood(victual.piece);
        recalc_wt();
    }
    victual.fullwarn = victual.eating = victual.doreset = FALSE;
    /* Do not set canchoke to FALSE; if we continue eating the same object we
       need to know if canchoke was set when they started eating it the
       previous time.  And if we don't continue eating the same object canchoke 
       always gets recalculated anyway. */
    stop_occupation();
    newuhs(FALSE);
}

static int
eatfood(void)
{       /* called each move during eating process */
    if (!victual.piece ||
        (!carried(victual.piece) && !obj_here(victual.piece, u.ux, u.uy))) {
        /* maybe it was stolen? */
        do_reset_eat();
        return 0;
    }
    if (!victual.eating)
        return 0;

    if (++victual.usedtime <= victual.reqtime) {
        if (bite())
            return 0;
        return 1;       /* still busy */
    } else {    /* done */
        done_eating(TRUE);
        return 0;
    }
}

static void
done_eating(boolean message)
{
    victual.piece->in_use = TRUE;
    occupation = 0;     /* do this early, so newuhs() knows we're done */
    newuhs(FALSE);
    if (nomovemsg) {
        if (message)
            pline(nomovemsg);
        nomovemsg = 0;
    } else if (message)
        pline("You finish eating %s.", food_xname(victual.piece, TRUE));

    if (victual.piece->otyp == CORPSE)
        cpostfx(victual.piece->corpsenm);
    else
        fpostfx(victual.piece);

    if (carried(victual.piece))
        useup(victual.piece);
    else
        useupf(victual.piece, 1L);
    victual.piece = NULL;
    victual.fullwarn = victual.eating = victual.doreset = FALSE;
}

static boolean
maybe_cannibal(int pm, boolean allowmsg)
{
    if (!CANNIBAL_ALLOWED() && your_race(&mons[pm])) {
        if (allowmsg) {
            if (Upolyd)
                pline("You have a bad feeling deep inside.");
            pline("You cannibal!  You will regret this!");
        }
        HAggravate_monster |= FROMOUTSIDE;
        change_luck(-rn1(4, 2));        /* -5..-2 */
        return TRUE;
    }
    return FALSE;
}

static void
cprefx(int pm)
{
    maybe_cannibal(pm, TRUE);
    if (touch_petrifies(&mons[pm]) || pm == PM_MEDUSA) {
        if (!Stone_resistance &&
            !(poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))) {
            sprintf(killer_buf, "tasting %s meat", mons[pm].mname);
            killer_format = KILLED_BY;
            killer = killer_buf;
            pline("You turn to stone.");
            done(STONING);
            if (victual.piece)
                victual.eating = FALSE;
            return;     /* lifesaved */
        }
    }

    switch (pm) {
    case PM_LITTLE_DOG:
    case PM_DOG:
    case PM_LARGE_DOG:
    case PM_KITTEN:
    case PM_HOUSECAT:
    case PM_LARGE_CAT:
        if (!CANNIBAL_ALLOWED()) {
            pline("You feel that eating the %s was a bad idea.",
                  mons[pm].mname);
            HAggravate_monster |= FROMOUTSIDE;
        }
        break;
    case PM_LIZARD:
        if (Stoned)
            fix_petrification();
        break;
    case PM_DEATH:
    case PM_PESTILENCE:
    case PM_FAMINE:
        {
            char buf[BUFSZ];

            pline("Eating that is instantly fatal.");
            sprintf(buf, "unwisely ate the body of %s", mons[pm].mname);
            killer = buf;
            killer_format = NO_KILLER_PREFIX;
            done(DIED);
            /* It so happens that since we know these monsters */
            /* cannot appear in tins, victual.piece will always */
            /* be what we want, which is not generally true. */
            if (revive_corpse(victual.piece))
                victual.piece = NULL;
            return;
        }
    case PM_GREEN_SLIME:
        if (!Slimed && !Unchanging && !flaming(youmonst.data) &&
            !unsolid(youmonst.data) &&
            level->locations[u.ux][u.uy].typ != LAVAPOOL &&
            youmonst.data != &mons[PM_GREEN_SLIME]) {
            pline("You don't feel very well.");
            Slimed = 10L;
            iflags.botl = 1;
        }
        /* Fall through */
    default:
        if (acidic(&mons[pm]) && Stoned)
            fix_petrification();
        break;
    }
}

void
fix_petrification(void)
{
    Stoned = 0;
    delayed_killer = 0;
    if (Hallucination)
        pline("What a pity - you just ruined a future piece of %sart!",
              ACURR(A_CHA) > 15 ? "fine " : "");
    else
        pline("You feel limber!");
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
        return can_teleport(ptr);

    case TELEPORT_CONTROL:
        return control_teleport(ptr);

    case TELEPAT:
        return telepathic(ptr);

    default:
        return FALSE;
    }
 /*NOTREACHED*/}

/* givit() tries to give you an intrinsic based on the monster's level
 * and what type of intrinsic it is trying to give you.
 */
static void
givit(int type, const struct permonst *ptr)
{
    int chance;

    /* some intrinsics are easier to get than others */
    switch (type) {
    case POISON_RES:
        if ((ptr == &mons[PM_KILLER_BEE] || ptr == &mons[PM_SCORPION]) &&
            !rn2(4))
            chance = 1;
        else
            chance = 15;
        break;
    case TELEPORT:
        chance = 10;
        break;
    case TELEPORT_CONTROL:
        chance = 12;
        break;
    case TELEPAT:
        chance = 1;
        break;
    default:
        chance = 15;
        break;
    }

    if (ptr->mlevel <= rn2(chance))
        return; /* failed die roll */

    switch (type) {
    case FIRE_RES:
        if (!(HFire_resistance & FROMOUTSIDE)) {
            pline(Hallucination ? "You be chillin'." :
                  "You feel a momentary chill.");
            HFire_resistance |= FROMOUTSIDE;
        }
        break;

    case SLEEP_RES:
        if (!(HSleep_resistance & FROMOUTSIDE)) {
            pline("You feel wide awake.");
            HSleep_resistance |= FROMOUTSIDE;
        }
        break;

    case COLD_RES:
        if (!(HCold_resistance & FROMOUTSIDE)) {
            pline("You feel full of hot air.");
            HCold_resistance |= FROMOUTSIDE;
        }
        break;

    case DISINT_RES:
        if (!(HDisint_resistance & FROMOUTSIDE)) {
            pline(Hallucination ? "You feel totally together, man." :
                  "You feel very firm.");
            HDisint_resistance |= FROMOUTSIDE;
        }
        break;

    case SHOCK_RES:    /* shock (electricity) resistance */
        if (!(HShock_resistance & FROMOUTSIDE)) {
            if (Hallucination)
                pline("You feel grounded in reality.");
            else
                pline("Your health currently feels amplified!");
            HShock_resistance |= FROMOUTSIDE;
        }
        break;

    case POISON_RES:
        if (!(HPoison_resistance & FROMOUTSIDE)) {
            pline(Poison_resistance ? "You feel especially healthy." :
                  "You feel healthy.");
            HPoison_resistance |= FROMOUTSIDE;
        }
        break;

    case TELEPORT:
        if (!(HTeleportation & FROMOUTSIDE)) {
            pline(Hallucination ? "You feel diffuse." : "You feel very jumpy.");
            HTeleportation |= FROMOUTSIDE;
        }
        break;

    case TELEPORT_CONTROL:
        if (!(HTeleport_control & FROMOUTSIDE)) {
            pline(Hallucination ? "You feel centered in your personal space." :
                  "You feel in control of yourself.");
            HTeleport_control |= FROMOUTSIDE;
        }
        break;

    case TELEPAT:
        if (!(HTelepat & FROMOUTSIDE)) {
            pline(Hallucination ? "You feel in touch with the cosmos." :
                  "You feel a strange mental acuity.");
            HTelepat |= FROMOUTSIDE;
            /* If blind, make sure monsters show up. */
            if (Blind)
                see_monsters();
        }
        break;

    default:
        break;
    }
}

static void
cpostfx(int pm)
{       /* called after completely consuming a corpse */
    int tmp = 0;
    boolean catch_lycanthropy = FALSE;

    /* in case `afternmv' didn't get called for previously mimicking gold,
       clean up now to avoid `eatmbuf' memory leak */
    if (eatmbuf)
        eatmdone();

    switch (pm) {
    case PM_NEWT:
        /* MRKR: "eye of newt" may give small magical energy boost */
        if (rn2(3) || 3 * u.uen <= 2 * u.uenmax) {
            int old_uen = u.uen;

            u.uen += rnd(3);
            if (u.uen > u.uenmax) {
                if (!rn2(3))
                    u.uenmax++;
                u.uen = u.uenmax;
            }
            if (old_uen != u.uen) {
                pline("You feel a mild buzz.");
                iflags.botl = 1;
            }
        }
        break;
    case PM_WRAITH:
        pluslvl(FALSE);
        break;
    case PM_HUMAN_WERERAT:
        catch_lycanthropy = TRUE;
        u.ulycn = PM_WERERAT;
        break;
    case PM_HUMAN_WEREJACKAL:
        catch_lycanthropy = TRUE;
        u.ulycn = PM_WEREJACKAL;
        break;
    case PM_HUMAN_WEREWOLF:
        catch_lycanthropy = TRUE;
        u.ulycn = PM_WEREWOLF;
        break;
    case PM_NURSE:
        if (Upolyd)
            u.mh = u.mhmax;
        else
            u.uhp = u.uhpmax;
        iflags.botl = 1;
        break;
    case PM_STALKER:
        if (!Invis) {
            set_itimeout(&HInvis, (long)rn1(100, 50));
            if (!Blind && !BInvis)
                self_invis_message();
        } else {
            if (!(HInvis & INTRINSIC))
                pline("You feel hidden!");
            HInvis |= FROMOUTSIDE;
            HSee_invisible |= FROMOUTSIDE;
        }
        newsym(u.ux, u.uy);
        /* fall into next case */
    case PM_YELLOW_LIGHT:
        /* fall into next case */
    case PM_GIANT_BAT:
        make_stunned(HStun + 30, FALSE);
        /* fall into next case */
    case PM_BAT:
        make_stunned(HStun + 30, FALSE);
        break;
    case PM_GIANT_MIMIC:
        tmp += 10;
        /* fall into next case */
    case PM_LARGE_MIMIC:
        tmp += 20;
        /* fall into next case */
    case PM_SMALL_MIMIC:
        tmp += 20;
        if (youmonst.data->mlet != S_MIMIC && !Unchanging) {
            char buf[BUFSZ];

            pline("You can't resist the temptation to mimic %s.",
                  Hallucination ? "an orange" : "a pile of gold");
            /* A pile of gold can't ride. */
            if (u.usteed)
                dismount_steed(DISMOUNT_FELL);
            nomul(-tmp, "pretending to be a pile of gold");
            sprintf(buf,
                    Hallucination ?
                    "You suddenly dread being peeled and mimic %s again!" :
                    "You now prefer mimicking %s again.",
                    an(Upolyd ? youmonst.data->mname : urace.noun));
            eatmbuf = strcpy(malloc(strlen(buf) + 1), buf);
            nomovemsg = eatmbuf;
            afternmv = eatmdone;
            /* ??? what if this was set before? */
            youmonst.m_ap_type = M_AP_OBJECT;
            youmonst.mappearance = Hallucination ? ORANGE : GOLD_PIECE;
            newsym(u.ux, u.uy);
            flush_screen();
            /* make gold symbol show up now */
            win_pause_output(P_MAP);
        }
        break;
    case PM_QUANTUM_MECHANIC:
        pline("Your velocity suddenly seems very uncertain!");
        if (HFast & INTRINSIC) {
            HFast &= ~INTRINSIC;
            pline("You seem slower.");
        } else {
            HFast |= FROMOUTSIDE;
            pline("You seem faster.");
        }
        break;
    case PM_LIZARD:
        if (HStun > 2)
            make_stunned(2L, FALSE);
        if (HConfusion > 2)
            make_confused(2L, FALSE);
        break;
    case PM_CHAMELEON:
    case PM_DOPPELGANGER:
        /* case PM_SANDESTIN: */
        if (!Unchanging) {
            pline("You feel a change coming over you.");
            polyself(FALSE);
        }
        break;
    case PM_MIND_FLAYER:
    case PM_MASTER_MIND_FLAYER:
        if (ABASE(A_INT) < ATTRMAX(A_INT)) {
            if (!rn2(2)) {
                pline("Yum! That was real brain food!");
                adjattrib(A_INT, 1, FALSE);
                break;  /* don't give them telepathy, too */
            }
        } else {
            pline("For some reason, that tasted bland.");
        }
        /* fall through to default case */
    default:{
            const struct permonst *ptr = &mons[pm];
            int i, count;

            if (dmgtype(ptr, AD_STUN) || dmgtype(ptr, AD_HALU) ||
                pm == PM_VIOLET_FUNGUS) {
                pline("Oh wow!  Great stuff!");
                make_hallucinated(HHallucination + 200, FALSE, 0L);
            }
            if (is_giant(ptr))
                gainstr(NULL, 0);

            /* Check the monster for all of the intrinsics.  If this monster
               can give more than one, pick one to try to give from among all
               it can give. If a monster can give 4 intrinsics then you have a
               1/1 * 1/2 * 2/3 * 3/4 = 1/4 chance of getting the first, a 1/2 *
               2/3 * 3/4 = 1/4 chance of getting the second, a 1/3 * 3/4 = 1/4
               chance of getting the third, and a 1/4 chance of getting the
               fourth. And now a proof by induction: it works for 1 intrinsic (1
               in 1 of getting it) for 2 you have a 1 in 2 chance of getting the
               second, otherwise you keep the first for 3 you have a 1 in 3 chance 
               of getting the third, otherwise you keep the first or the second
               for n+1 you have a 1 in n+1 chance of getting the (n+1)st,
               otherwise you keep the previous one. Elliott Kleinrock, October 5,
               1990 */

            count = 0;  /* number of possible intrinsics */
            tmp = 0;    /* which one we will try to give */
            for (i = 1; i <= LAST_PROP; i++) {
                if (intrinsic_possible(i, ptr)) {
                    count++;
                    /* a 1 in count chance of replacing the old one with this
                       one, and a count-1 in count chance of keeping the old
                       one.  (note that 1 in 1 and 0 in 1 are what we want for
                       the first one */
                    if (!rn2(count))
                        tmp = i;
                }
            }

            /* if any found try to give them one */
            if (count)
                givit(tmp, ptr);
        }
        break;
    }

    if (catch_lycanthropy && defends(AD_WERE, uwep)) {
        if (!touch_artifact(uwep, &youmonst)) {
            dropx(uwep);
            uwepgone();
        }
    }

    return;
}

void
violated_vegetarian(void)
{
    u.uconduct.unvegetarian++;
    if (Role_if(PM_MONK)) {
        pline("You feel guilty.");
        adjalign(-1);
    }
    return;
}

/* common code to check and possibly charge for 1 context.tin.tin,
 * will split() context.tin.tin if necessary */
static void
costly_tin(const char *verb /* if 0, the verb is "open" */ )
{
    if (((!carried(tin.tin) && costly_spot(tin.tin->ox, tin.tin->oy) &&
          !tin.tin->no_charge)
         || tin.tin->unpaid)) {
        verbalize("You %s it, you bought it!", verb ? verb : "open");
        if (tin.tin->quan > 1L)
            tin.tin = splitobj(tin.tin, 1L);
        bill_dummy_object(tin.tin);
    }
}

static int
opentin(void)
{       /* called during each move whilst opening a tin */
    int r;
    const char *what;
    int which;

    if (!carried(tin.tin) && !obj_here(tin.tin, u.ux, u.uy))
        /* perhaps it was stolen? */
        return 0;       /* %% probably we should use tinoid */
    if (tin.usedtime++ >= 50) {
        pline("You give up your attempt to open the tin.");
        return 0;
    }
    if (tin.usedtime < tin.reqtime)
        return 1;       /* still busy */
    if (tin.tin->otrapped || (tin.tin->cursed && tin.tin->spe != -1 && !rn2(8))) {
        b_trapped("tin", 0);
        costly_tin("destroyed");
        goto use_me;
    }
    pline("You succeed in opening the tin.");
    if (tin.tin->spe != 1) {
        if (tin.tin->corpsenm == NON_PM) {
            pline("It turns out to be empty.");
            tin.tin->dknown = tin.tin->known = TRUE;
            costly_tin(NULL);
            goto use_me;
        }
        r = tin.tin->cursed ? ROTTEN_TIN :      /* always rotten if cursed */
            (tin.tin->spe == -1) ? HOMEMADE_TIN :       /* player made it */
            rn2(TTSZ - 1);      /* else take your pick */
        if (r == ROTTEN_TIN &&
            (tin.tin->corpsenm == PM_LIZARD || tin.tin->corpsenm == PM_LICHEN))
            r = HOMEMADE_TIN;   /* lizards don't rot */
        else if (tin.tin->spe == -1 && !tin.tin->blessed && !rn2(7))
            r = ROTTEN_TIN;     /* some homemade tins go bad */
        which = 0;      /* 0=>plural, 1=>as-is, 2=>"the" prefix */
        if (Hallucination) {
            int idx = rndmonidx();

            what = monnam_for_index(idx);
            if ((idx < SPECIAL_PM) &&
                ! !(mons[tin.tin->corpsenm].geno & G_UNIQ))
                which = type_is_pname(&mons[idx]) ? 1 : 2;
            else
                which = monnam_is_pname(idx) ? 1 : 0;
        } else {
            what = mons[tin.tin->corpsenm].mname;
            if (mons[tin.tin->corpsenm].geno & G_UNIQ)
                which = type_is_pname(&mons[tin.tin->corpsenm]) ? 1 : 2;
        }
        if (which == 0)
            what = makeplural(what);
        pline("It smells like %s%s.", (which == 2) ? "the " : "", what);
        if (yn("Eat it?") == 'n') {
            if (!Hallucination)
                tin.tin->dknown = tin.tin->known = TRUE;
            if (flags.verbose)
                pline("You discard the open tin.");
            costly_tin(NULL);
            goto use_me;
        }
        /* in case stop_occupation() was called on previous meal */
        victual.piece = NULL;
        victual.fullwarn = victual.eating = victual.doreset = FALSE;

        pline("You consume %s %s.", tintxts[r].txt,
              mons[tin.tin->corpsenm].mname);

        /* KMH, conduct */
        u.uconduct.food++;
        if (!vegan(&mons[tin.tin->corpsenm]))
            u.uconduct.unvegan++;
        if (!vegetarian(&mons[tin.tin->corpsenm]))
            violated_vegetarian();

        tin.tin->dknown = tin.tin->known = TRUE;
        cprefx(tin.tin->corpsenm);
        cpostfx(tin.tin->corpsenm);

        /* charge for one at pre-eating cost */
        costly_tin(NULL);

        /* check for vomiting added by GAN 01/16/87 */
        if (tintxts[r].nut < 0)
            make_vomiting((long)rn1(15, 10), FALSE);
        else
            lesshungry(tintxts[r].nut);

        if (r == 0 || r == FRENCH_FRIED_TIN) {
            /* Assume !Glib, because you can't open tins when Glib. */
            incr_itimeout(&Glib, rnd(15));
            pline("Eating deep fried food made your %s very slippery.",
                  makeplural(body_part(FINGER)));
        }
    } else {
        if (tin.tin->cursed)
            pline("It contains some decaying%s%s substance.", Blind ? "" : " ",
                  Blind ? "" : hcolor("green"));
        else
            pline("It contains spinach.");

        if (yn("Eat it?") == 'n') {
            if (!Hallucination && !tin.tin->cursed)
                tin.tin->dknown = tin.tin->known = TRUE;
            if (flags.verbose)
                pline("You discard the open tin.");
            costly_tin(NULL);
            goto use_me;
        }

        tin.tin->dknown = tin.tin->known = TRUE;
        costly_tin(NULL);

        if (!tin.tin->cursed)
            pline("This makes you feel like %s!",
                  Hallucination ? "Swee'pea" : "Popeye");
        lesshungry(600);
        gainstr(tin.tin, 0);
        u.uconduct.food++;
    }
use_me:
    if (carried(tin.tin))
        useup(tin.tin);
    else
        useupf(tin.tin, 1L);
    tin.tin = NULL;
    return 0;
}

static void
start_tin(struct obj *otmp)
{       /* called when starting to open a tin */
    int tmp;

    if (metallivorous(youmonst.data)) {
        pline("You bite right into the metal tin...");
        tmp = 1;
    } else if (nolimbs(youmonst.data)) {
        pline("You cannot handle the tin properly to open it.");
        return;
    } else if (otmp->blessed) {
        pline("The tin opens like magic!");
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
        pline("Using your %s you try to open the tin.", aobjnam(uwep, NULL));
    } else {
    no_opener:
        pline("It is not so easy to open this tin.");
        if (Glib) {
            pline("The tin slips from your %s.", makeplural(body_part(FINGER)));
            if (otmp->quan > 1L) {
                otmp = splitobj(otmp, 1L);
            }
            if (carried(otmp))
                dropx(otmp);
            else
                stackobj(otmp);
            return;
        }
        tmp = rn1(1 + 500 / ((int)(ACURR(A_DEX) + ACURRSTR)), 10);
    }
    tin.reqtime = tmp;
    tin.usedtime = 0;
    tin.tin = otmp;
    set_occupation(opentin, "opening the tin", 0);
    return;
}

int
Hear_again(void)
{       /* called when waking up after fainting */
    flags.soundok = 1;
    return 0;
}

/* called on the "first bite" of rotten food */
static int
rottenfood(struct obj *obj)
{
    pline("Blecch!  Rotten %s!", foodword(obj));
    if (!rn2(4)) {
        if (Hallucination)
            pline("You feel rather trippy.");
        else
            pline("You feel rather %s.", body_part(LIGHT_HEADED));
        make_confused(HConfusion + dice(2, 4), FALSE);
    } else if (!rn2(4) && !Blind) {
        pline("Everything suddenly goes dark.");
        make_blinded((long)dice(2, 10), FALSE);
        if (!Blind)
            pline("Your vision quickly clears.");
    } else if (!rn2(3)) {
        const char *what, *where;

        if (!Blind)
            what = "goes", where = "dark";
        else if (Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))
            what = "you lose control of", where = "yourself";
        else
            what = "you slap against the", where =
                (u.usteed) ? "saddle" : surface(u.ux, u.uy);
        pline("The world spins and %s %s.", what, where);
        flags.soundok = 0;
        nomul(-rnd(10), "unconscious from rotten food");
        nomovemsg = "You are conscious again.";
        afternmv = Hear_again;
        see_monsters();
        see_objects();
        vision_full_recalc = 1;
        return 1;
    }
    return 0;
}

/* called when a corpse is selected as food */
static int
eatcorpse(struct obj *otmp)
{
    int tp = 0, mnum = otmp->corpsenm;
    long rotted = 0L;
    boolean uniq = ! !(mons[mnum].geno & G_UNIQ);
    int retcode = 0;
    boolean stoneable = (touch_petrifies(&mons[mnum]) && !Stone_resistance &&
                         !poly_when_stoned(youmonst.data));

    /* KMH, conduct */
    if (!vegan(&mons[mnum]))
        u.uconduct.unvegan++;
    if (!vegetarian(&mons[mnum]))
        violated_vegetarian();

    if (mnum != PM_LIZARD && mnum != PM_LICHEN) {
        long age = peek_at_iced_corpse_age(otmp);

        rotted = (moves - age) / (10L + rn2(20));
        if (otmp->cursed)
            rotted += 2L;
        else if (otmp->blessed)
            rotted -= 2L;
    }

    if (mnum != PM_ACID_BLOB && !stoneable && rotted > 5L) {
        boolean cannibal = maybe_cannibal(mnum, FALSE);

        pline("Ulch!  That %s was tainted%s!",
              mons[mnum].mlet ==
              S_FUNGUS ? "fungoid vegetation" : !vegetarian(&mons[mnum]) ?
              "meat" : "protoplasm", cannibal ? "; you cannibal" : "");
        if (Sick_resistance) {
            pline("It doesn't seem at all sickening, though...");
        } else {
            char buf[BUFSZ];
            long sick_time;

            sick_time = (long)rn1(10, 10);
            /* make sure new ill doesn't result in improvement */
            if (Sick && (sick_time > Sick))
                sick_time = (Sick > 1L) ? Sick - 1L : 1L;
            if (!uniq)
                sprintf(buf, "rotted %s", corpse_xname(otmp, TRUE));
            else
                sprintf(buf, "%s%s rotted corpse",
                        !type_is_pname(&mons[mnum]) ? "the " : "",
                        s_suffix(mons[mnum].mname));
            make_sick(sick_time, buf, TRUE, SICK_VOMITABLE);
        }
        if (carried(otmp))
            useup(otmp);
        else
            useupf(otmp, 1L);
        return 2;
    } else if (acidic(&mons[mnum]) && !Acid_resistance) {
        tp++;
        pline("You have a very bad case of stomach acid.");
        /* not body_part() */
        losehp(rnd(15), "acidic corpse", KILLED_BY_AN);
    } else if (poisonous(&mons[mnum]) && rn2(5)) {
        tp++;
        pline("Ecch - that must have been poisonous!");
        if (!Poison_resistance) {
            losestr(rnd(4));
            losehp(rnd(15), "poisonous corpse", KILLED_BY_AN);
        } else
            pline("You seem unaffected by the poison.");
        /* now any corpse left too long will make you mildly ill */
    } else if ((rotted > 5L || (rotted > 3L && rn2(5)))
               && !Sick_resistance) {
        tp++;
        pline("You feel %ssick.", (Sick) ? "very " : "");
        losehp(rnd(8), "cadaver", KILLED_BY_AN);
    }

    /* delay is weight dependent */
    victual.reqtime = 3 + (mons[mnum].cwt >> 6);

    if (!tp && mnum != PM_LIZARD && mnum != PM_LICHEN &&
        (otmp->orotten || !rn2(7))) {
        if (rottenfood(otmp)) {
            otmp->orotten = TRUE;
            touchfood(otmp);
            retcode = 1;
        }

        if (!mons[otmp->corpsenm].cnutrit) {
            /* no nutrution: rots away, no message if you passed out */
            if (!retcode)
                pline("The corpse rots away completely.");
            if (carried(otmp))
                useup(otmp);
            else
                useupf(otmp, 1L);
            retcode = 2;
        }

        if (!retcode)
            consume_oeaten(otmp, 2);    /* oeaten >>= 2 */
    } else {
        pline("%s%s %s!",
              !uniq ? "This " : !type_is_pname(&mons[mnum]) ? "The " : "",
              food_xname(otmp, FALSE), (vegan(&mons[mnum])
                                        ? (!carnivorous(youmonst.data) &&
                                           herbivorous(youmonst.data))
                                        : (carnivorous(youmonst.data) &&
                                           !herbivorous(youmonst.data)))
              ? "is delicious" : "tastes terrible");
    }

    return retcode;
}

static void
start_eating(struct obj *otmp)
{       /* called as you start to eat */
    victual.fullwarn = victual.doreset = FALSE;
    victual.eating = TRUE;

    if (otmp->otyp == CORPSE) {
        cprefx(victual.piece->corpsenm);
        if (!victual.piece || !victual.eating) {
            /* rider revived, or died and lifesaved */
            return;
        }
    }

    if (bite())
        return;

    if (++victual.usedtime >= victual.reqtime) {
        /* print "finish eating" message if they just resumed -dlc */
        done_eating(victual.reqtime > 1 ? TRUE : FALSE);
        return;
    }

    sprintf(msgbuf, "eating %s", food_xname(otmp, TRUE));
    set_occupation(eatfood, msgbuf, 0);
}


/*
 * called on "first bite" of (non-corpse) food.
 * used for non-rotten non-tin non-corpse food
 */
static void
fprefx(struct obj *otmp)
{
    switch (otmp->otyp) {
    case FOOD_RATION:
        if (u.uhunger <= 200)
            pline(Hallucination ? "Oh wow, like, superior, man!" :
                  "That food really hit the spot!");
        else if (u.uhunger <= 700)
            pline("That satiated your %s!", body_part(STOMACH));
        break;
    case TRIPE_RATION:
        if (carnivorous(youmonst.data) && !humanoid(youmonst.data))
            pline("That tripe ration was surprisingly good!");
        else if (maybe_polyd(is_orc(youmonst.data), Race_if(PM_ORC)))
            pline(Hallucination ? "Tastes great! Less filling!" :
                  "Mmm, tripe... not bad!");
        else {
            pline("Yak - dog food!");
            more_experienced(1, 0);
            newexplevel();
            /* not cannibalism, but we use similar criteria for deciding
               whether to be sickened by this meal */
            if (rn2(2) && !CANNIBAL_ALLOWED())
                make_vomiting((long)rn1(victual.reqtime, 14), FALSE);
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
            make_vomiting((long)rn1(victual.reqtime, 5), FALSE);
            break;
        }
        /* Fall through otherwise */
    default:
        if (otmp->otyp == SLIME_MOLD && !otmp->cursed &&
            otmp->spe == current_fruit)
            pline("My, that was a %s %s!", Hallucination ? "primo" : "yummy",
                  singular(otmp, xname));
        else
#ifdef UNIX
        if (otmp->otyp == APPLE || otmp->otyp == PEAR) {
            if (!Hallucination)
                pline("Core dumped.");
            else {
/* This is based on an old Usenet joke, a fake a.out manual page */
                int x = rnd(100);

                if (x <= 75)
                    pline("Segmentation fault -- core dumped.");
                else if (x <= 99)
                    pline("Bus error -- core dumped.");
                else
                    pline("Yo' mama -- core dumped.");
            }
        } else
#endif
        if (otmp->otyp == EGG && stale_egg(otmp)) {
            pline("Ugh.  Rotten egg."); /* perhaps others like it */
            make_vomiting(Vomiting + dice(10, 4), TRUE);
        } else
        give_feedback:
            pline("This %s is %s", singular(otmp, xname),
                  otmp->cursed ? (Hallucination ? "grody!" : "terrible!")
                  : (otmp->otyp == CRAM_RATION || otmp->otyp == K_RATION ||
                     otmp->otyp == C_RATION)
                  ? "bland." : Hallucination ? "gnarly!" : "delicious!");
        break;
    }
}

static void
accessory_has_effect(struct obj *otmp)
{
    pline("Magic spreads through your body as you digest the %s.",
          otmp->oclass == RING_CLASS ? "ring" : "amulet");
}

static void
eataccessory(struct obj *otmp)
{
    int typ = otmp->otyp;
    long oldprop;

    /* Note: rings are not so common that this is unbalancing. */
    /* (How often do you even _find_ 3 rings of polymorph in a game?) */
    oldprop = u.uprops[objects[typ].oc_oprop].intrinsic;
    if (otmp == uleft || otmp == uright) {
        Ring_gone(otmp);
        if (u.uhp <= 0)
            return;     /* died from sink fall */
    }
    otmp->known = otmp->dknown = 1;     /* by taste */
    if (!rn2(otmp->oclass == RING_CLASS ? 3 : 5)) {
        switch (otmp->otyp) {
        default:
            if (!objects[typ].oc_oprop)
                break;  /* should never happen */

            if (!(u.uprops[objects[typ].oc_oprop].intrinsic & FROMOUTSIDE))
                accessory_has_effect(otmp);

            u.uprops[objects[typ].oc_oprop].intrinsic |= FROMOUTSIDE;

            switch (typ) {
            case RIN_SEE_INVISIBLE:
                set_mimic_blocking();
                see_monsters();
                if (Invis && !oldprop && !ESee_invisible &&
                    !perceives(youmonst.data) && !Blind) {
                    newsym(u.ux, u.uy);
                    pline("Suddenly you can see yourself.");
                    makeknown(typ);
                }
                break;
            case RIN_INVISIBILITY:
                if (!oldprop && !EInvis && !BInvis && !See_invisible && !Blind) {
                    newsym(u.ux, u.uy);
                    pline("Your body takes on a %s transparency...",
                          Hallucination ? "normal" : "strange");
                    makeknown(typ);
                }
                break;
            case RIN_PROTECTION_FROM_SHAPE_CHANGERS:
                resistcham();
                break;
            case RIN_LEVITATION:
                /* undo the `.intrinsic |= FROMOUTSIDE' done above */
                u.uprops[LEVITATION].intrinsic = oldprop;
                if (!Levitation) {
                    float_up();
                    incr_itimeout(&HLevitation, dice(10, 20));
                    makeknown(typ);
                }
                break;
            }
            break;
        case RIN_ADORNMENT:
            accessory_has_effect(otmp);
            if (adjattrib(A_CHA, otmp->spe, -1))
                makeknown(typ);
            break;
        case RIN_GAIN_STRENGTH:
            accessory_has_effect(otmp);
            if (adjattrib(A_STR, otmp->spe, -1))
                makeknown(typ);
            break;
        case RIN_GAIN_CONSTITUTION:
            accessory_has_effect(otmp);
            if (adjattrib(A_CON, otmp->spe, -1))
                makeknown(typ);
            break;
        case RIN_INCREASE_ACCURACY:
            accessory_has_effect(otmp);
            u.uhitinc += otmp->spe;
            break;
        case RIN_INCREASE_DAMAGE:
            accessory_has_effect(otmp);
            u.udaminc += otmp->spe;
            break;
        case RIN_PROTECTION:
            accessory_has_effect(otmp);
            HProtection |= FROMOUTSIDE;
            u.ublessed += otmp->spe;
            iflags.botl = 1;
            break;
        case RIN_FREE_ACTION:
            /* Give sleep resistance instead */
            if (!(HSleep_resistance & FROMOUTSIDE))
                accessory_has_effect(otmp);
            if (!Sleep_resistance)
                pline("You feel wide awake.");
            HSleep_resistance |= FROMOUTSIDE;
            break;
        case AMULET_OF_CHANGE:
            accessory_has_effect(otmp);
            makeknown(typ);
            change_sex();
            pline("You are suddenly very %s!",
                  flags.female ? "feminine" : "masculine");
            iflags.botl = 1;
            break;
        case AMULET_OF_UNCHANGING:
            /* un-change: it's a pun */
            if (!Unchanging && Upolyd) {
                accessory_has_effect(otmp);
                makeknown(typ);
                rehumanize();
            }
            break;
        case AMULET_OF_STRANGULATION:  /* bad idea! */
            /* no message--this gives no permanent effect */
            choke(otmp);
            break;
        case AMULET_OF_RESTFUL_SLEEP:  /* another bad idea! */
            if (!(HSleeping & FROMOUTSIDE))
                accessory_has_effect(otmp);
            HSleeping = FROMOUTSIDE | rnd(100);
            break;
        case RIN_SUSTAIN_ABILITY:
        case AMULET_OF_LIFE_SAVING:
        case AMULET_OF_REFLECTION:     /* nice try */
            /* can't eat Amulet of Yendor or fakes, and no oc_prop even if you
               could -3. */
            break;
        }
    }
}

static void
eatspecial(void)
{       /* called after eating non-food */
    struct obj *otmp = victual.piece;

    /* lesshungry wants an occupation to handle choke messages correctly */
    set_occupation(eatfood, "eating non-food", 0);
    lesshungry(victual.nmod);
    occupation = 0;
    victual.piece = NULL;
    victual.eating = 0;
    if (otmp->oclass == COIN_CLASS) {
        if (otmp->where == OBJ_FREE)
            dealloc_obj(otmp);
        else
            useupf(otmp, otmp->quan);
        return;
    }
    if (otmp->oclass == POTION_CLASS) {
        otmp->quan++;   /* dopotion() does a useup() */
        dopotion(otmp);
    }
    if (otmp->oclass == RING_CLASS || otmp->oclass == AMULET_CLASS)
        eataccessory(otmp);
    else if (otmp->otyp == LEASH && otmp->leashmon)
        o_unleash(otmp);

    /* KMH -- idea by "Tommy the Terrorist" */
    if ((otmp->otyp == TRIDENT) && !otmp->cursed) {
        pline(Hallucination ? "Four out of five dentists agree." :
              "That was pure chewing satisfaction!");
        exercise(A_WIS, TRUE);
    }
    if ((otmp->otyp == FLINT) && !otmp->cursed) {
        pline("Yabba-dabba delicious!");
        exercise(A_CON, TRUE);
    }

    if (otmp == uwep && otmp->quan == 1L)
        uwepgone();
    if (otmp == uquiver && otmp->quan == 1L)
        uqwepgone();
    if (otmp == uswapwep && otmp->quan == 1L)
        uswapwepgone();

    if (otmp == uball)
        unpunish();
    if (otmp == uchain)
        unpunish();     /* but no useup() */
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
static void
fpostfx(struct obj *otmp)
{
    switch (otmp->otyp) {
    case SPRIG_OF_WOLFSBANE:
        if (u.ulycn >= LOW_PM || is_were(youmonst.data))
            you_unwere(TRUE);
        break;
    case CARROT:
        make_blinded((long)u.ucreamed, TRUE);
        break;
    case FORTUNE_COOKIE:
        outrumor(bcsign(otmp), BY_COOKIE);
        if (!Blind)
            u.uconduct.literate++;
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
                rehumanize();
            }
        } else {
            u.uhp += otmp->cursed ? -rnd(20) : rnd(20);
            if (u.uhp > u.uhpmax) {
                if (!rn2(17))
                    u.uhpmax++;
                u.uhp = u.uhpmax;
            } else if (u.uhp <= 0) {
                killer_format = KILLED_BY_AN;
                killer = "rotten lump of royal jelly";
                done(POISONING);
            }
        }
        if (!otmp->cursed)
            heal_legs();
        break;
    case EGG:
        if (touch_petrifies(&mons[otmp->corpsenm])) {
            if (!Stone_resistance &&
                !(poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))) {
                if (!Stoned)
                    Stoned = 5;
                killer_format = KILLED_BY_AN;
                sprintf(killer_buf, "%s egg", mons[otmp->corpsenm].mname);
                delayed_killer = killer_buf;
            }
        }
        break;
    case EUCALYPTUS_LEAF:
        if (Sick && !otmp->cursed)
            make_sick(0L, NULL, TRUE, SICK_ALL);
        if (Vomiting && !otmp->cursed)
            make_vomiting(0L, TRUE);
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

    char buf[BUFSZ], foodsmell[BUFSZ], it_or_they[QBUFSZ],
        eat_it_anyway[QBUFSZ];
    boolean cadaver = (otmp->otyp == CORPSE), stoneorslime = FALSE;
    int material = objects[otmp->otyp].oc_material, mnum = otmp->corpsenm;
    long rotted = 0L;

    strcpy(foodsmell, Tobjnam(otmp, "smell"));
    strcpy(it_or_they, (otmp->quan == 1L) ? "it" : "they");
    sprintf(eat_it_anyway, "Eat %s anyway?",
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
        sprintf(buf, "%s like %s could be tainted! %s", foodsmell, it_or_they,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (stoneorslime) {
        sprintf(buf, "%s like %s could be something very dangerous! %s",
                foodsmell, it_or_they, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (u.uedibility && (otmp->orotten || (cadaver && rotted > 3L))) {
        /* Rotten */
        sprintf(buf, "%s like %s could be rotten! %s", foodsmell, it_or_they,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (cadaver && poisonous(&mons[mnum]) && !Poison_resistance) {
        /* poisonous */
        sprintf(buf, "%s like %s might be poisonous! %s", foodsmell, it_or_they,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (cadaver && !vegetarian(&mons[mnum]) && !u.uconduct.unvegetarian &&
        Role_if(PM_MONK)) {
        sprintf(buf, "%s unsuitable for a vegetarian monk. %s", foodsmell,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    /* HP check is needed to stop this being annoying */
    if (cadaver && acidic(&mons[mnum]) && !Acid_resistance && u.uhp < -20) {
        sprintf(buf, "%s rather acidic, and you're low on health. %s",
                foodsmell, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    if (Upolyd && u.umonnum == PM_RUST_MONSTER && is_metallic(otmp) &&
        otmp->oerodeproof && u.uedibility) {
        sprintf(buf, "%s disgusting to you right now. %s", foodsmell,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }

    /* 
     * Potentially fatal for other reasons.
     */
    if (u.uhs == SATIATED) {
        sprintf(buf, "You are not really in the mood to eat. %s",
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }


    /* 
     * Breaks conduct, but otherwise safe.
     */

    if (!u.uconduct.unvegetarian && moves > 1800 &&
        ((material == LEATHER || material == BONE || material == DRAGON_HIDE) ||
         (cadaver && !vegetarian(&mons[mnum])))) {
        sprintf(buf, "%s of meat. %s", foodsmell, eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }

    if (!u.uconduct.unvegan && moves > 1800 &&
        ((material == LEATHER || material == BONE || material == DRAGON_HIDE ||
          material == WAX) || (cadaver && !vegan(&mons[mnum])))) {
        sprintf(buf, "%s like an animal byproduct. %s", foodsmell,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }

    if (cadaver && mnum != PM_ACID_BLOB && rotted > 5L && Sick_resistance &&
        u.uedibility) {
        /* Tainted meat with Sick_resistance */
        sprintf(buf, "%s like %s could be tainted! %s", foodsmell, it_or_they,
                eat_it_anyway);
        if (yn_function(buf, ynchars, 'n') == 'n')
            return 1;
        else
            return 2;
    }
    return 0;
}

int
doeat(struct obj *otmp)
{       /* generic "eat" command funtion (see cmd.c) */
    int basenutrit;     /* nutrition of full item */
    boolean dont_start = FALSE;

    if (Strangled) {
        pline("If you can't breathe air, how can you consume solids?");
        return 0;
    }
    if (otmp && !validate_object(otmp, allobj + 2, "eat"))
        return 0;
    else if (!otmp)
        otmp = floorfood("eat", 0);
    if (!otmp)
        return 0;
    if (check_capacity(NULL))
        return 0;

    {
        int res = edibility_prompts(otmp);

        if (res && u.uedibility) {
            pline
                ("Your %s stops tingling and your sense of smell returns to normal.",
                 body_part(NOSE));
            u.uedibility = 0;
        }
        if (res == 1)
            return 0;
    }

    /* We have to make non-foods take 1 move to eat, unless we want to do
       ridiculous amounts of coding to deal with partly eaten plate mails,
       players who polymorph back to human in the middle of their metallic
       meal, etc.... */
    if (!is_edible(otmp)) {
        pline("You cannot eat that!");
        return 0;
    } else if ((otmp->owornmask & (W_ARMOR | W_TOOL | W_AMUL | W_SADDLE)) != 0) {
        /* let them eat rings */
        pline("You can't eat something you're wearing.");
        return 0;
    }
    if (is_metallic(otmp) && u.umonnum == PM_RUST_MONSTER && otmp->oerodeproof) {
        otmp->rknown = TRUE;
        if (otmp->quan > 1L) {
            if (!carried(otmp))
                splitobj(otmp, otmp->quan - 1L);
            else
                otmp = splitobj(otmp, 1L);
        }
        pline("Ulch!  That %s was rustproofed!", xname(otmp));
        /* The regurgitated object's rustproofing is gone now */
        otmp->oerodeproof = 0;
        make_stunned(HStun + rn2(10), TRUE);
        pline("You spit %s out onto the %s.", the(xname(otmp)),
              surface(u.ux, u.uy));
        if (carried(otmp)) {
            freeinv(otmp);
            dropy(otmp);
        }
        stackobj(otmp);
        return 1;
    }
    /* KMH -- Slow digestion is... indigestible */
    if (otmp->otyp == RIN_SLOW_DIGESTION) {
        pline("This ring is indigestible!");
        rottenfood(otmp);
        if (otmp->dknown && !objects[otmp->otyp].oc_name_known &&
            !objects[otmp->otyp].oc_uname)
            docall(otmp);
        return 1;
    }
    if (otmp->oclass != FOOD_CLASS) {
        int material;

        if (!touch_artifact(otmp, &youmonst))
            return 1;
        if (!is_edible(otmp)) {
            pline("You can no longer eat %s.", doname(otmp));
            return 1;
        }

        victual.reqtime = 1;
        victual.piece = otmp;
        /* Don't split it, we don't need to if it's 1 move */
        victual.usedtime = 0;
        victual.canchoke = (u.uhs == SATIATED);
        /* Note: gold weighs 1 pt. for each 1000 pieces (see */
        /* pickup.c) so gold and non-gold is consistent. */
        if (otmp->oclass == COIN_CLASS)
            basenutrit =
                ((otmp->quan > 200000L) ? 2000 : (int)(otmp->quan / 100L));
        else if (otmp->oclass == BALL_CLASS || otmp->oclass == CHAIN_CLASS)
            basenutrit = weight(otmp);
        /* oc_nutrition is usually weight anyway */
        else
            basenutrit = objects[otmp->otyp].oc_nutrition;
        victual.nmod = basenutrit;
        victual.eating = TRUE;  /* needed for lesshungry() */

        material = objects[otmp->otyp].oc_material;
        if (material == LEATHER || material == BONE || material == DRAGON_HIDE) {
            u.uconduct.unvegan++;
            violated_vegetarian();
        } else if (material == WAX)
            u.uconduct.unvegan++;
        u.uconduct.food++;

        if (otmp->cursed)
            rottenfood(otmp);

        if (otmp->oclass == WEAPON_CLASS && otmp->opoisoned) {
            pline("Ecch - that must have been poisonous!");
            if (!Poison_resistance) {
                losestr(rnd(4));
                losehp(rnd(15), xname(otmp), KILLED_BY_AN);
            } else
                pline("You seem unaffected by the poison.");
        } else if (!otmp->cursed)
            pline("This %s is delicious!",
                  otmp->oclass == COIN_CLASS ? foodword(otmp) :
                  singular(otmp, xname));

        eatspecial();
        return 1;
    }

    if (otmp == victual.piece) {
        /* If they weren't able to choke, they don't suddenly become able to
           choke just because they were interrupted.  On the other hand, if
           they were able to choke before, if they lost food it's possible they 
           shouldn't be able to choke now. */
        if (u.uhs != SATIATED)
            victual.canchoke = FALSE;
        victual.piece = touchfood(otmp);
        pline("You resume your meal.");
        start_eating(victual.piece);
        return 1;
    }

    /* nothing in progress - so try to find something. */
    /* tins are a special case */
    /* tins must also check conduct separately in case they're discarded */
    if (otmp->otyp == TIN) {
        start_tin(otmp);
        return 1;
    }

    /* KMH, conduct */
    u.uconduct.food++;

    victual.piece = otmp = touchfood(otmp);
    victual.usedtime = 0;

    /* Now we need to calculate delay and nutritional info. The base nutrition
       calculated here and in eatcorpse() accounts for normal vs. rotten food.
       The reqtime and nutrit values are then adjusted in accordance with the
       amount of food left. */
    if (otmp->otyp == CORPSE) {
        int tmp = eatcorpse(otmp);

        if (tmp == 2) {
            /* used up */
            victual.piece = NULL;
            return 1;
        } else if (tmp)
            dont_start = TRUE;
        /* if not used up, eatcorpse sets up reqtime and may modify oeaten */
    } else {
        /* No checks for WAX, LEATHER, BONE, DRAGON_HIDE.  These are all
           handled in the != FOOD_CLASS case, above */
        switch (objects[otmp->otyp].oc_material) {
        case FLESH:
            u.uconduct.unvegan++;
            if (otmp->otyp != EGG) {
                violated_vegetarian();
            }
            break;

        default:
            if (otmp->otyp == PANCAKE ||
                otmp->otyp == FORTUNE_COOKIE || /* eggs */
                otmp->otyp == CREAM_PIE ||
                otmp->otyp == CANDY_BAR ||   /* milk */
                otmp->otyp == LUMP_OF_ROYAL_JELLY)
                u.uconduct.unvegan++;
            break;
        }

        victual.reqtime = objects[otmp->otyp].oc_delay;
        if (otmp->otyp != FORTUNE_COOKIE &&
            (otmp->cursed ||
             (((moves - otmp->age) > (int)otmp->blessed ? 50 : 30) &&
              (otmp->orotten || !rn2(7))))) {

            if (rottenfood(otmp)) {
                otmp->orotten = TRUE;
                dont_start = TRUE;
            }
            consume_oeaten(otmp, 1);    /* oeaten >>= 1 */
        } else
            fprefx(otmp);
    }

    /* re-calc the nutrition */
    if (otmp->otyp == CORPSE)
        basenutrit = mons[otmp->corpsenm].cnutrit;
    else
        basenutrit = objects[otmp->otyp].oc_nutrition;

    victual.reqtime =
        (basenutrit ==
         0 ? 0 : rounddiv(victual.reqtime * (long)otmp->oeaten, basenutrit));

    /* calculate the modulo value (nutrit. units per round eating) note: this
       isn't exact - you actually lose a little nutrition due to this method.
       TODO: add in a "remainder" value to be given at the end of the meal. */
    if (victual.reqtime == 0 || otmp->oeaten == 0)
        /* possible if most has been eaten before */
        victual.nmod = 0;
    else if ((int)otmp->oeaten >= victual.reqtime)
        victual.nmod = -((int)otmp->oeaten / victual.reqtime);
    else
        victual.nmod = victual.reqtime % otmp->oeaten;
    victual.canchoke = (u.uhs == SATIATED);

    if (!dont_start)
        start_eating(otmp);
    return 1;
}

/* Take a single bite from a piece of food, checking for choking and
 * modifying usedtime.  Returns 1 if they choked and survived, 0 otherwise.
 */
static int
bite(void)
{
    if (victual.canchoke && u.uhunger >= 2000) {
        choke(victual.piece);
        return 1;
    }
    if (victual.doreset) {
        do_reset_eat();
        return 0;
    }
    force_save_hs = TRUE;
    if (victual.nmod < 0) {
        lesshungry(-victual.nmod);
        consume_oeaten(victual.piece, victual.nmod);    /* -= -nmod */
    } else if (victual.nmod > 0 && (victual.usedtime % victual.nmod)) {
        lesshungry(1);
        consume_oeaten(victual.piece, -1);      /* -= 1 */
    }
    force_save_hs = FALSE;
    recalc_wt();
    return 0;
}


void
gethungry(void)
{       /* as time goes by - called by moveloop() and domove() */
    if (u.uinvulnerable)
        return; /* you don't feel hungrier */

    if ((!u.usleep || !rn2(10)) /* slow metabolic rate while asleep */
        &&(carnivorous(youmonst.data) || herbivorous(youmonst.data))
        && !Slow_digestion)
        u.uhunger--;    /* ordinary food consumption */

    if (moves % 2) {    /* odd turns */
        /* Regeneration uses up food, unless due to an artifact */
        if (HRegeneration ||
            ((ERegeneration & (~W_ART)) &&
             (ERegeneration != W_WEP || !uwep->oartifact)))
            u.uhunger--;
        if (near_capacity() > SLT_ENCUMBER)
            u.uhunger--;
    } else {    /* even turns */
        if (Hunger)
            u.uhunger--;
        /* Conflict uses up food too */
        if (HConflict || (EConflict & (~W_ARTI)))
            u.uhunger--;
        /* +0 charged rings don't do anything, so don't affect hunger */
        /* Slow digestion still uses ring hunger */
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
            if (u.uhave.amulet)
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
lesshungry(int num)
{
    /* See comments in newuhs() for discussion on force_save_hs */
    boolean iseating = (occupation == eatfood) || force_save_hs;

    u.uhunger += num;
    if (u.uhunger >= 2000) {
        if (!iseating || victual.canchoke) {
            if (iseating) {
                choke(victual.piece);
                reset_eat();
            } else
                choke(occupation == opentin ? tin.tin : NULL);
            /* no reset_eat() */
        }
    } else {
        /* Have lesshungry() report when you're nearly full so all eating warns 
           when you're about to choke. */
        if (u.uhunger >= 1500) {
            if (!victual.eating || (victual.eating && !victual.fullwarn)) {
                pline("You're having a hard time getting all of it down.");
                nomovemsg = "You're finally finished.";
                if (!victual.eating)
                    multi = -2;
                else {
                    victual.fullwarn = TRUE;
                    if (victual.canchoke && victual.reqtime > 1) {
                        /* a one-gulp food will not survive a stop */
                        if (yn_function("Stop eating?", ynchars, 'y') == 'y') {
                            reset_eat();
                            nomovemsg = NULL;
                        }
                    }
                }
            }
        }
    }
    newuhs(FALSE);
}

static int
unfaint(void)
{
    Hear_again();
    if (u.uhs > FAINTING)
        u.uhs = FAINTING;
    stop_occupation();
    iflags.botl = 1;
    return 0;
}


boolean
is_fainted(void)
{
    return (boolean) (u.uhs == FAINTED);
}

/* call when a faint must be prematurely terminated */
void
reset_faint(void)
{
    if (is_fainted())
        nomul(0, NULL);
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

    /* While you're eating, you may pass from WEAK to HUNGRY to NOT_HUNGRY.
       This should not produce the message "you only feel hungry now"; that
       message should only appear if HUNGRY is an endpoint.  Therefore we check 
       to see if we're in the middle of eating.  If so, we save the first
       hunger status, and at the end of eating we decide what message to print
       based on the _entire_ meal, not on each little bit. */
    /* It is normally possible to check if you are in the middle of a meal by
       checking occupation == eatfood, but there is one special case:
       start_eating() can call bite() for your first bite before it sets the
       occupation. Anyone who wants to get that case to work _without_ an ugly
       static force_save_hs variable, feel free. */
    /* Note: If you become a certain hunger status in the middle of the meal,
       and still have that same status at the end of the meal, this will
       incorrectly print the associated message at the end of the meal instead
       of the middle.  Such a case is currently impossible, but could become
       possible if a message for SATIATED were added or if HUNGRY and WEAK were 
       separated by a big enough gap to fit two bites. */
    if (occupation == eatfood || force_save_hs) {
        if (!newuhs_saved_hs) {
            newuhs_save_hs = u.uhs;
            newuhs_saved_hs = TRUE;
        }
        u.uhs = newhs;
        return;
    } else {
        if (newuhs_saved_hs) {
            u.uhs = newuhs_save_hs;
            newuhs_saved_hs = FALSE;
        }
    }

    if (newhs == FAINTING) {
        if (is_fainted())
            newhs = FAINTED;
        if (u.uhs <= WEAK || rn2(20 - u.uhunger / 10) >= 19) {
            if (!is_fainted() && multi >= 0 /* %% */ ) {
                /* stop what you're doing, then faint */
                stop_occupation();
                pline("You faint from lack of food.");
                flags.soundok = 0;
                nomul(-10 + (u.uhunger / 10), "fainted from lack of food");
                nomovemsg = "You regain consciousness.";
                afternmv = unfaint;
                newhs = FAINTED;
            }
        } else if (u.uhunger < -(int)(200 + 20 * ACURR(A_CON))) {
            u.uhs = STARVED;
            iflags.botl = 1;
            bot();
            pline("You die from starvation.");
            killer_format = KILLED_BY;
            killer = "starvation";
            done(STARVING);
            /* if we return, we lifesaved, and that calls newuhs */
            return;
        }
    }

    if (newhs != u.uhs) {
        if (newhs >= WEAK && u.uhs < WEAK)
            losestr(1); /* this may kill you -- see below */
        else if (newhs < WEAK && u.uhs >= WEAK)
            losestr(-1);
        switch (newhs) {
        case HUNGRY:
            if (Hallucination) {
                pline((!incr) ? "You now have a lesser case of the munchies." :
                      "You are getting the munchies.");
            } else
                pline((!incr) ? "You only feel hungry now." : (u.uhunger < 145)
                      ? "You feel hungry." :
                      "You are beginning to feel hungry.");
            if (incr && occupation &&
                (occupation != eatfood && occupation != opentin))
                stop_occupation();
            break;
        case WEAK:
            if (Hallucination)
                pline((!incr) ? "You still have the munchies." :
                      "The munchies are interfering with your motor capabilities.");
            else if (incr &&
                     (Role_if(PM_WIZARD) || Race_if(PM_ELF) ||
                      Role_if(PM_VALKYRIE)))
                pline("%s needs food, badly!",
                      (Role_if(PM_WIZARD) ||
                       Role_if(PM_VALKYRIE)) ? urole.name.m : "Elf");
            else
                pline((!incr) ? "You feel weak now." : (u.uhunger < 45) ?
                      "You feel weak." : "You are beginning to feel weak.");
            if (incr && occupation &&
                (occupation != eatfood && occupation != opentin))
                stop_occupation();
            break;
        }
        u.uhs = newhs;
        iflags.botl = 1;
        bot();
        if ((Upolyd ? u.mh : u.uhp) < 1) {
            pline("You die from hunger and exhaustion.");
            killer_format = KILLED_BY;
            killer = "exhaustion";
            done(STARVING);
            return;
        }
    }
}


/* Returns an object representing food.  Object may be either on floor or
 * in inventory.
 */
struct obj *
floorfood(      /* get food from floor or pack */
             const char *verb, int corpsecheck)
{       /* 0, no check, 1, corpses, 2, tinnable corpses */
    struct obj *otmp;
    char qbuf[QBUFSZ];
    char c;
    struct trap *ttmp = t_at(level, u.ux, u.uy);
    boolean feeding = (!strcmp(verb, "eat"));
    int can_floorfood = 0;
    boolean checking_can_floorfood = TRUE;

    /* if we can't touch floor objects then use invent food only */
    if (!can_reach_floor() ||
        (feeding && u.usteed) ||  /* can't eat off floor while riding */
        ((is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy)) &&
         (Wwalking || is_clinger(youmonst.data) || (Flying && !Breathless))) ||
        (ttmp && ttmp->tseen && (ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT)
         && (!u.utrap || (u.utrap && u.utraptype != TT_PIT)) && !Passes_walls))
        goto skipfloor;

eat_floorfood:
    if (feeding && metallivorous(youmonst.data)) {
        struct obj *gold;

        if (ttmp && ttmp->tseen && ttmp->ttyp == BEAR_TRAP) {
            if (!checking_can_floorfood) {
                /* If not already stuck in the trap, perhaps there should be a
                   chance to becoming trapped? Probably not, because then the
                   trap would just get eaten on the _next_ turn... */
                sprintf(qbuf, "There is a bear trap here (%s); eat it?",
                        (u.utrap &&
                         u.utraptype == TT_BEARTRAP) ? "holding you" : "armed");
                if ((c = yn_function(qbuf, ynqchars, 'n')) == 'y') {
                    u.utrap = u.utraptype = 0;
                    deltrap(level, ttmp);
                    return mksobj(level, BEARTRAP, TRUE, FALSE);
                } else if (c == 'q') {
                    return NULL;
                }
            } else
                can_floorfood++;
        }

        if (youmonst.data != &mons[PM_RUST_MONSTER] &&
            (gold = gold_at(level, u.ux, u.uy)) != 0) {
            if (!checking_can_floorfood) {
                if (gold->quan == 1L)
                    sprintf(qbuf, "There is 1 gold piece here; eat it?");
                else
                    sprintf(qbuf, "There are %d gold pieces here; eat them?",
                            gold->quan);
                if ((c = yn_function(qbuf, ynqchars, 'n')) == 'y') {
                    return gold;
                } else if (c == 'q') {
                    return NULL;
                }
            } else
                can_floorfood++;
        }
    }

    /* Is there some food (probably a heavy corpse) here on the ground? */
    for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere) {
        if (corpsecheck
            ? (otmp->otyp == CORPSE &&
               (corpsecheck == 1 ||
                tinnable(otmp))) : feeding ? (otmp->oclass != COIN_CLASS &&
                                              is_edible(otmp)) : otmp->oclass ==
            FOOD_CLASS) {
            if (!checking_can_floorfood) {
                sprintf(qbuf, "There %s %s here; %s %s?", otense(otmp, "are"),
                        doname(otmp), verb, (otmp->quan == 1L) ? "it" : "one");
                if (can_floorfood == 1 ||
                    (c = yn_function(qbuf, ynqchars, 'n')) == 'y')
                    return otmp;
                else if (c == 'q')
                    return NULL;
            } else
                can_floorfood++;
        }
    }

skipfloor:
    /* We cannot use ALL_CLASSES since that causes getobj() to skip its "ugly
       checks" and we need to check for inedible items. */
    otmp = getobj(feeding ? (const char *)(allobj + (can_floorfood ? 0 : 2))
                  : (const char *)(comestibles + (can_floorfood ? 0 : 2)),
                  verb);
    if (otmp == &zeroobj) {
        checking_can_floorfood = FALSE;
        goto eat_floorfood;
    }
    if (corpsecheck && otmp)
        if (otmp->otyp != CORPSE || (corpsecheck == 2 && !tinnable(otmp))) {
            pline("You can't %s that!", verb);
            return NULL;
        }
    return otmp;
}

/* Side effects of vomiting */
/* added nomul (MRS) - it makes sense, you're too busy being sick! */
void
vomit(void)
{       /* A good idea from David Neves */
    make_sick(0L, NULL, TRUE, SICK_VOMITABLE);
    nomul(-2, "vomiting");
}

int
eaten_stat(int base, struct obj *obj)
{
    long uneaten_amt, full_amount;

    uneaten_amt = (long)obj->oeaten;
    full_amount =
        (obj->otyp ==
         CORPSE) ? (long)mons[obj->corpsenm].cnutrit : (long)objects[obj->otyp].
        oc_nutrition;
    if (uneaten_amt > full_amount) {
        impossible
            ("partly eaten food (%ld) more nutritious than untouched food (%ld)",
             uneaten_amt, full_amount);
        uneaten_amt = full_amount;
    }

    base = (int)(full_amount ? (long)base * uneaten_amt / full_amount : 0L);
    return (base < 1) ? 1 : base;
}

/* reduce obj's oeaten field, making sure it never hits or passes 0 */
void
consume_oeaten(struct obj *obj, int amt)
{
    /* 
     * This is a hack to try to squelch several long standing mystery
     * food bugs.  A better solution would be to rewrite the entire
     * victual handling mechanism from scratch using a less complex
     * model.  Alternatively, this routine could call done_eating()
     * or food_disappears() but its callers would need revisions to
     * cope with victual.piece unexpectedly going away.
     *
     * Multi-turn eating operates by setting the food's oeaten field
     * to its full nutritional value and then running a counter which
     * independently keeps track of whether there is any food left.
     * The oeaten field can reach exactly zero on the last turn, and
     * the object isn't removed from inventory until the next turn
     * when the "you finish eating" message gets delivered, so the
     * food would be restored to the status of untouched during that
     * interval.  This resulted in unexpected encumbrance messages
     * at the end of a meal (if near enough to a threshold) and would
     * yield full food if there was an interruption on the critical
     * turn.  Also, there have been reports over the years of food
     * becoming massively heavy or producing unlimited satiation;
     * this would occur if reducing oeaten via subtraction attempted
     * to drop it below 0 since its unsigned type would produce a
     * huge positive value instead.  So far, no one has figured out
     * _why_ that inappropriate subtraction might sometimes happen.
     */

    if (amt > 0) {
        /* bit shift to divide the remaining amount of food */
        obj->oeaten >>= amt;
    } else {
        /* simple decrement; value is negative so we actually add it */
        if ((int)obj->oeaten > -amt)
            obj->oeaten += amt;
        else
            obj->oeaten = 0;
    }

    if (obj->oeaten == 0) {
        if (obj == victual.piece)       /* always true unless wishing... */
            victual.reqtime = victual.usedtime; /* no bites left */
        obj->oeaten = 1;        /* smallest possible positive value */
    }
}


/* called when eatfood occupation has been interrupted,
   or in the case of theft, is about to be interrupted */
boolean
maybe_finished_meal(boolean stopping)
{
    /* in case consume_oeaten() has decided that the food is all gone */
    if (occupation == eatfood && victual.usedtime >= victual.reqtime) {
        if (stopping)
            occupation = 0;     /* for do_reset_eat */
        eatfood();      /* calls done_eating() to use up victual.piece */
        return TRUE;
    }
    return FALSE;
}


void
save_food(struct memfile *mf)
{
    unsigned int oid, vflags;

    /* no mtag useful; fixed distance after track */

    oid = victual.piece ? victual.piece->o_id : 0;
    mwrite32(mf, oid);
    mwrite32(mf, victual.usedtime);
    mwrite32(mf, victual.reqtime);
    mwrite32(mf, victual.nmod);

    vflags =
        (victual.canchoke << 31) | (victual.fullwarn << 30) |
        (victual.eating << 29) | (victual.doreset << 28);
    mwrite32(mf, vflags);

    oid = tin.tin ? tin.tin->o_id : 0;
    mwrite32(mf, oid);
    mwrite32(mf, tin.usedtime);
    mwrite32(mf, tin.reqtime);
}


void
restore_food(struct memfile *mf)
{
    unsigned int oid, vflags;

    oid = mread32(mf);
    victual.piece = oid ? find_oid(oid) : NULL;
    victual.usedtime = mread32(mf);
    victual.reqtime = mread32(mf);
    victual.nmod = mread32(mf);

    vflags = mread32(mf);
    victual.canchoke = (vflags >> 31) & 1;
    victual.fullwarn = (vflags >> 30) & 1;
    victual.eating = (vflags >> 29) & 1;
    victual.doreset = (vflags >> 28) & 1;

    oid = mread32(mf);
    tin.tin = oid ? find_oid(oid) : NULL;
    tin.usedtime = mread32(mf);
    tin.reqtime = mread32(mf);
}


void
reset_food(void)
{
    memset(&victual, 0, sizeof (victual));
    memset(&tin, 0, sizeof (tin));
    newuhs_save_hs = 0;
    newuhs_saved_hs = 0;
}

/*eat.c*/
