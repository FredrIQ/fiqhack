/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Benson I. Margulies, Mike Stephenson, Steve Linhart, 1989. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "epri.h"
#include "hungerstatus.h"

static struct obj *worst_cursed_item(void);
static enum pray_trouble in_trouble(void);
static void fix_worst_trouble(int);
static void angrygods(aligntyp);
static void at_your_feet(const char *);
static void gcrownu(void);
static void pleased(aligntyp);
static void godvoice(enum msg_channel, aligntyp, const char *);
static void god_zaps_you(aligntyp);
static void fry_by_god(aligntyp);
static void gods_angry(aligntyp);
static void gods_upset(aligntyp);
static void consume_offering(struct obj *);
static boolean water_prayer(boolean);
static boolean blocked_boulder(int, int);

/* simplify a few tests */
#define Cursed_obj(obj,typ) ((obj) && (obj)->otyp == (typ) && (obj)->cursed)

/*
 * Logic behind deities and altars and such:
 * + prayers are made to your god if not on an altar, and to the altar's god
 *   if you are on an altar
 * + If possible, your god answers all prayers, which is why bad things happen
 *   if you try to pray on another god's altar
 * + sacrifices work basically the same way, but the other god may decide to
 *   accept your allegiance, after which they are your god.  If rejected,
 *   your god takes over with your punishment.
 * + if you're in Gehennom, all messages come from Moloch
 */

/*
 * Moloch, who dwells in Gehennom, is the "renegade" cruel god
 * responsible for the theft of the Amulet from Marduk, the Creator.
 * Moloch is unaligned.
 */
static const char *const Moloch = "Moloch";

static const char *const godvoices[] = {
    "booms out",
    "thunders",
    "rings out",
    "booms",
};

#define PIOUS 20
#define DEVOUT 14
#define FERVENT 9
#define STRIDENT 4

/* We could force rehumanize of polyselfed people, but we can't tell
   unintentional shape changes from the other kind. Oh well.
   3.4.2: make an exception if polymorphed into a form which lacks
   hands; that's a case where the ramifications override this doubt.
 */

/* Return 0 if nothing particular seems wrong, positive numbers for
   serious trouble, and negative numbers for comparative annoyances. This
   returns the worst problem. There may be others, and the gods may fix
   more than one.

This could get as bizarre as noting surrounding opponents, (or hostile dogs),
but that's really hard.
 */

#define ugod_is_angry() (u.ualign.record < 0)
#define on_altar()      (IS_ALTAR(level->locations[u.ux][u.uy].typ) && \
                         !Engulfed)
#define on_shrine()     ((level->locations[u.ux][u.uy].altarmask & AM_SHRINE) \
                            != 0)
#define a_align(x,y)    ((aligntyp)Amask2align( \
                            level->locations[x][y].altarmask & AM_MASK))

static enum pray_trouble
in_trouble(void)
{
    struct obj *otmp;
    int i, j, count = 0;

    /*
     * major troubles
     */
    if (Stoned)
        return ptr_stoned;
    if (Slimed)
        return ptr_slimed;
    if (Strangled)
        return ptr_strangled;
    if (u.utrap && u.utraptype == TT_LAVA)
        return ptr_lava;
    if (Sick)
        return ptr_sick;
    if (u.uhs >= WEAK)
        return ptr_starving;
    if (Upolyd ? (u.mh <= 5 || u.mh * 7 <= u.mhmax)
        : (u.uhp <= 5 || u.uhp * 7 <= u.uhpmax))
        return ptr_hit;
    if (u.ulycn >= LOW_PM)
        return ptr_lycanthrope;
    if (near_capacity() >= EXT_ENCUMBER && AMAX(A_STR) - ABASE(A_STR) > 3)
        return ptr_collapsing;

    for (i = -1; i <= 1; i++)
        for (j = -1; j <= 1; j++) {
            if (!i && !j)
                continue;
            if (!isok(u.ux + i, u.uy + j) ||
                IS_ROCK(level->locations[u.ux + i][u.uy + j].typ)
                || (blocked_boulder(i, j) && !throws_rocks(youmonst.data)))
                count++;
        }
    if (count == 8 && !Passes_walls)
        return ptr_stuck;

    if (Cursed_obj(uarmf, LEVITATION_BOOTS) || stuck_ring(uleft, RIN_LEVITATION)
        || stuck_ring(uright, RIN_LEVITATION))
        return ptr_levitation;
    if (nohands(youmonst.data) || !freehand()) {
        /* for bag/box access [cf use_container()]... make sure it's a case
           that we know how to handle; otherwise "fix all troubles" would get
           stuck in a loop */
        if (welded(uwep))
            return ptr_hands;
        if (Upolyd && nohands(youmonst.data) &&
            (!Unchanging || ((otmp = unchanger()) != 0 && otmp->cursed)))
            return ptr_hands;
    }
    if (Blindfolded && ublindf->cursed)
        return ptr_blindfold;

    /*
     * minor troubles
     */
    if (Punished)
        return ptr_punished;
    if (Cursed_obj(uarmg, GAUNTLETS_OF_FUMBLING) ||
        Cursed_obj(uarmf, FUMBLE_BOOTS))
        return ptr_fumbling;
    if (worst_cursed_item())
        return ptr_cursed;
    if (u.usteed) {     /* can't voluntarily dismount from a cursed saddle */
        otmp = which_armor(u.usteed, os_saddle);
        if (Cursed_obj(otmp, SADDLE))
            return ptr_saddle;
    }

    if (Blinded > 1 && haseyes(youmonst.data))
        return ptr_blind;
    for (i = 0; i < A_MAX; i++)
        if (ABASE(i) < AMAX(i))
            return ptr_poisoned;
    if (Wounded_legs && !u.usteed)
        return ptr_wounded_legs;
    if (u.uhs >= HUNGRY)
        return ptr_hungry;
    if (HStun)
        return ptr_stunned;
    if (HConfusion)
        return ptr_confused;
    if (Hallucination)
        return ptr_hallucinating;
    return ptr_invalid;
}

/* select an item for ptr_cursed */
static struct obj *
worst_cursed_item(void)
{
    struct obj *otmp;

    /* if strained or worse, check for loadstone first */
    if (near_capacity() >= HVY_ENCUMBER) {
        for (otmp = invent; otmp; otmp = otmp->nobj)
            if (Cursed_obj(otmp, LOADSTONE))
                return otmp;
    }
    /* weapon takes precedence if it is interfering with taking off a ring or
       putting on a shield */
    if (welded(uwep) && (uright || bimanual(uwep))) {   /* weapon */
        otmp = uwep;
        /* gloves come next, due to rings */
    } else if (uarmg && uarmg->cursed) {        /* gloves */
        otmp = uarmg;
        /* then shield due to two handed weapons and spells */
    } else if (uarms && uarms->cursed) {        /* shield */
        otmp = uarms;
        /* then cloak due to body armor */
    } else if (uarmc && uarmc->cursed) {        /* cloak */
        otmp = uarmc;
    } else if (uarm && uarm->cursed) {  /* suit */
        otmp = uarm;
    } else if (uarmh && uarmh->cursed) {        /* helmet */
        otmp = uarmh;
    } else if (uarmf && uarmf->cursed) {        /* boots */
        otmp = uarmf;
    } else if (uarmu && uarmu->cursed) {        /* shirt */
        otmp = uarmu;
    } else if (uamul && uamul->cursed) {        /* amulet */
        otmp = uamul;
    } else if (uleft && uleft->cursed) {        /* left ring */
        otmp = uleft;
    } else if (uright && uright->cursed) {      /* right ring */
        otmp = uright;
    } else if (ublindf && ublindf->cursed) {    /* eyewear */
        otmp = ublindf; /* must be non-blinding lenses */
        /* if weapon wasn't handled above, do it now */
    } else if (welded(uwep)) {  /* weapon */
        otmp = uwep;
        /* active secondary weapon even though it isn't welded */
    } else if (uswapwep && uswapwep->cursed && u.twoweap) {
        otmp = uswapwep;
        /* all worn items ought to be handled by now */
    } else {
        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (!otmp->cursed)
                continue;
            if (otmp->otyp == LOADSTONE || confers_luck(otmp))
                break;
        }
    }
    return otmp;
}

static void
fix_worst_trouble(int trouble)
{
    int i;
    struct obj *otmp = 0;
    const char *what = NULL;
    static const char leftglow[] = "left ring softly glows", rightglow[] =
        "right ring softly glows";

    switch (trouble) {
    case ptr_stoned:
        pline(msgc_statusheal, "You feel more limber.");
        Stoned = 0;
        set_delayed_killer(STONING, NULL);
        break;
    case ptr_slimed:
        pline(msgc_statusheal, "The slime disappears.");
        Slimed = 0;
        set_delayed_killer(TURNED_SLIME, NULL);
        break;
    case ptr_strangled:
        if (uamul && uamul->otyp == AMULET_OF_STRANGULATION) {
            pline(msgc_itemloss, "Your amulet vanishes!");
            useup(uamul);
        }
        pline(msgc_statusheal, "You can breathe again.");
        Strangled = 0;
        break;
    case ptr_lava:
        pline(msgc_statusheal, "You are back on solid ground.");
        /* teleport should always succeed, but if not, just untrap them. */
        if (!safe_teleds(FALSE))
            u.utrap = 0;
        break;
    case ptr_starving:
        /* TODO: Verify this bugfix (was previously STARVED, which is part of
           the wrong "enum"). */
        losestr(-1, STARVING, "<should be unused>", NULL);
        /* fall into... */
    case ptr_hungry:
        pline(msgc_statusheal, "Your %s feels content.", body_part(STOMACH));
        init_uhunger();
        break;
    case ptr_sick:
        pline(msgc_statusheal, "You feel better.");
        make_sick(0L, NULL, FALSE, SICK_ALL);
        break;
    case ptr_hit:
        /* "fix all troubles" will keep trying if hero has 5 or less hit
           points, so make sure they're always boosted to be more than that */
        pline(msgc_statusheal, "You feel much better.");
        if (Upolyd) {
            u.mhmax += rnd(5);
            if (u.mhmax <= 5)
                u.mhmax = 5 + 1;
            u.mh = u.mhmax;
        }
        if (u.uhpmax < u.ulevel * 5 + 11)
            u.uhpmax += rnd(5);
        if (u.uhpmax <= 5)
            u.uhpmax = 5 + 1;
        u.uhp = u.uhpmax;
        break;
    case ptr_collapsing:
        ABASE(A_STR) = AMAX(A_STR);
        break;
    case ptr_stuck:
        pline(msgc_statusheal, "Your surroundings change.");
        /* no control, but works on no-teleport levels */
        safe_teleds(FALSE);
        break;
    case ptr_levitation:
        if (Cursed_obj(uarmf, LEVITATION_BOOTS)) {
            otmp = uarmf;
        } else if ((otmp = stuck_ring(uleft, RIN_LEVITATION)) != 0) {
            if (otmp == uleft)
                what = leftglow;
        } else if ((otmp = stuck_ring(uright, RIN_LEVITATION)) != 0) {
            if (otmp == uright)
                what = rightglow;
        }
        goto decurse;
    case ptr_hands:
        if (welded(uwep)) {
            otmp = uwep;
            goto decurse;
        }
        if (Upolyd && nohands(youmonst.data)) {
            if (!Unchanging) {
                pline(msgc_statusheal, "Your shape becomes uncertain.");
                rehumanize(DIED, NULL);   /* "You return to {normal} form." */
            } else if ((otmp = unchanger()) != 0 && otmp->cursed) {
                /* otmp is an amulet of unchanging */
                goto decurse;
            }
        }
        if (nohands(youmonst.data) || !freehand())
            impossible("fix_worst_trouble: couldn't cure hands.");
        break;
    case ptr_blindfold:
        otmp = ublindf;
        goto decurse;
    case ptr_lycanthrope:
        you_unwere(TRUE);
        break;
    case ptr_punished:
        pline(msgc_statusheal, "Your chain disappears.");
        unpunish();
        break;
    case ptr_fumbling:
        if (Cursed_obj(uarmg, GAUNTLETS_OF_FUMBLING))
            otmp = uarmg;
        else if (Cursed_obj(uarmf, FUMBLE_BOOTS))
            otmp = uarmf;
        goto decurse;
    case ptr_cursed:
        otmp = worst_cursed_item();
        if (otmp == uright)
            what = rightglow;
        else if (otmp == uleft)
            what = leftglow;
    decurse:
        if (!otmp) {
            impossible("fix_worst_trouble: nothing to uncurse.");
            return;
        }
        uncurse(otmp);
        if (!Blind) {
            pline(msgc_itemrepair, "Your %s %s.",
                  what ? what : (const char *)aobjnam(otmp, "softly glow"),
                  hcolor("amber"));
            otmp->bknown = TRUE;
        }
        update_inventory();
        break;
    case ptr_poisoned:
        if (Hallucination)
            pline(msgc_intrgain, "There's a tiger in your tank.");
        else
            pline(msgc_intrgain, "You feel in good health again.");
        for (i = 0; i < A_MAX; i++)
            if (ABASE(i) < AMAX(i))
                ABASE(i) = AMAX(i);
        encumber_msg();
        break;
    case ptr_blind:
        {
            int num_eyes = eyecount(youmonst.data);
            const char *eye = body_part(EYE);

            pline(msgc_statusheal, "Your %s feel%s better.",
                  (num_eyes == 1) ? eye : makeplural(eye),
                  (num_eyes == 1) ? "s" : "");
            u.ucreamed = 0;
            make_blinded(0L, FALSE);
            break;
        }
    case ptr_wounded_legs:
        heal_legs(Wounded_leg_side);
        break;
    case ptr_stunned:
        make_stunned(0L, TRUE);
        break;
    case ptr_confused:
        make_confused(0L, TRUE);
        break;
    case ptr_hallucinating:
        pline(msgc_statusheal, "Looks like you are back in Kansas.");
        make_hallucinated(0L, FALSE);
        break;
    case ptr_saddle:
        otmp = which_armor(u.usteed, os_saddle);
        uncurse(otmp);
        if (!Blind) {
            pline(msgc_itemrepair, "%s %s %s.", s_suffix(
                      msgupcasefirst(y_monnam(u.usteed))),
                  aobjnam(otmp, "softly glow"), hcolor("amber"));
            otmp->bknown = TRUE;
        }
        break;
    default:
        impossible("Invalid trouble in fix_worst_trouble");
        break;
    }
}

/* "I am sometimes shocked by...  the nuns who never take a bath without wearing
   a bathrobe all the time.  When asked why, since no man can see them, they
   reply 'Oh, but you forget the good God'.  Apparently they conceive of the
   Deity as a Peeping Tom, whose omnipotence enables Him to see through bathroom
   walls, but who is foiled by bathrobes." --Bertrand Russell, 1943

   Divine wrath, dungeon walls, and armor follow the same principle. */
static void
god_zaps_you(aligntyp resp_god)
{
    /* We can save the more urgent message channels for later; the player is
       either going to survive the entire sequence, in which case alignbad is
       appropriate, or die, in which case there's no point in warning them
       after it's too already late to do anything. */
    if (Engulfed) {
        pline(msgc_alignbad,
              "Suddenly a bolt of lightning comes down at you from the "
              "heavens!");
        if (!resists_elec(u.ustuck)) {
            pline(combat_msgc(NULL, u.ustuck, cr_hit),
                  "%s is hit by it, and fries to a crisp!", Monnam(u.ustuck));
            /* Yup, you get experience.  It takes guts to successfully pull off
               this trick on your god, anyway. */
            xkilled(u.ustuck, 0);
        } else
            pline(combat_msgc(NULL, u.ustuck, cr_immune),
                  "%s is hit by it, but seems unaffected.", Monnam(u.ustuck));
    } else {
        pline(msgc_alignbad, "Suddenly, a bolt of lightning strikes you!");
        if (Reflecting) {
            shieldeff(u.ux, u.uy);
            if (Blind)
                pline(msgc_playerimmune, "For some reason you're unaffected.");
            else
                ureflects("%s reflects from your %s.", "It");
        } else if (Shock_resistance) {
            shieldeff(u.ux, u.uy);
            pline(msgc_playerimmune, "It seems not to affect you.");
        } else
            fry_by_god(resp_god);
    }

    pline(msgc_alignbad, "%s is not deterred...", align_gname(resp_god));
    if (Engulfed) {
        if (!resists_disint(u.ustuck)) {
            pline(combat_msgc(NULL, u.ustuck, cr_hit),
                  "A wide-angle disintegration beam fries %s to a crisp!",
                  mon_nam(u.ustuck));
            xkilled(u.ustuck, 2);       /* no corpse */
        } else
            pline(combat_msgc(NULL, u.ustuck, cr_immune),
                  "%s blocks a wide-angle disintegration beam!",
                  Monnam(u.ustuck));
    } else {
        pline(msgc_itemloss, "A wide-angle disintegration beam hits you!");

        /* disintegrate shield and body armor before disintegrating the
           impudent mortal, like black dragon breath -3. */
#define slot_affected(slot) EQUIP(slot) &&                      \
            !(worn_extrinsic(REFLECTING) & W_MASK(slot))        \
            && !(worn_extrinsic(DISINT_RES) & W_MASK(slot))
        if (slot_affected(os_arms))
            destroy_arm(uarms);
        if (slot_affected(os_armc))
            destroy_arm(uarmc);
        if (slot_affected(os_arm) && !uarmc && !uskin())
            destroy_arm(uarm);
#undef slot_affected
        if (uarmu && !uarm && !uarmc)
            destroy_arm(uarmu);
        if (!Disint_resistance)
            fry_by_god(resp_god);
        else {
            pline(msgc_playerimmune,
                  "You bask in its %s glow for a minute...", hcolor("black"));
            godvoice(msgc_noconsequence, resp_god, "I believe it not!");
        }
        if (has_sanctum(level, Align2amask(resp_god))) {
            /* one more try for high altars */
            verbalize(msgc_alignbad, "Thou cannot escape my wrath, %s!",
                      mortal_or_creature(youmonst.data, TRUE));
            summon_minion(resp_god, FALSE);
            summon_minion(resp_god, FALSE);
            summon_minion(resp_god, FALSE);
            verbalize(msgc_levelwarning, "Destroy %s, my servants!", uhim());
        }
    }
}

static void
fry_by_god(aligntyp resp_god)
{
    pline(msgc_fatal_predone, "You fry to a crisp.");
    done(DIED, killer_msg(DIED,
                          msgcat("the wrath of ", align_gname(resp_god))));
}

static void
angrygods(aligntyp resp_god)
{
    int maxanger;

    if (Inhell)
        resp_god = A_NONE;
    u.ublessed = 0;

    /* changed from tmp = u.ugangr + abs (u.uluck) -- rph */
    /* added test for alignment diff -dlc */
    if (resp_god != u.ualign.type)
        maxanger = u.ualign.record / 2 + (Luck > 0 ? -Luck / 3 : -Luck);
    else
        maxanger =
            3 * u.ugangr +
            ((Luck > 0 || u.ualign.record >= STRIDENT) ? -Luck / 3 : -Luck);
    if (maxanger < 1)
        maxanger = 1;   /* possible if bad align & good luck */
    else if (maxanger > 15)
        maxanger = 15;  /* be reasonable */

    switch (rn2_on_rng(maxanger, rng_god_anger)) {
    case 0:
    case 1:
        pline(msgc_alignbad, "You feel that %s is %s.", align_gname(resp_god),
              Hallucination ? "bummed" : "displeased");
        break;
    case 2:
    case 3:
        godvoice(msgc_npcvoice, resp_god, NULL);
        pline(msgc_alignbad,
              "\"Thou %s, %s.\"", (ugod_is_angry() && resp_god == u.ualign.type)
              ? "hast strayed from the path" : "art arrogant",
              mortal_or_creature(youmonst.data, TRUE));
        verbalize(msgc_statusbad, "Thou must relearn thy lessons!");
        /* and we're about to get intrloss messages here */
        adjattrib(A_WIS, -1, FALSE);
        losexp(NULL, FALSE);
        break;
    case 6:
        if (!Punished) {
            gods_angry(resp_god);
            punish(NULL);
            break;
        }       /* else fall thru */
    case 4:
    case 5:
        gods_angry(resp_god);
        if (!Blind && !Antimagic)
            pline(msgc_itemloss, "%s glow surrounds you.", An(hcolor("black")));
        rndcurse();
        break;
    case 7:
    case 8:
        godvoice(msgc_npcvoice, resp_god, NULL);
        verbalize(msgc_alignbad, "Thou durst %s me?",
                  (on_altar() &&
                   (a_align(u.ux, u.uy) != resp_god)) ? "scorn" : "call upon");
        pline(msgc_levelwarning,
              "\"Then die, %s!\"", mortal_or_creature(youmonst.data, TRUE));
        summon_minion(resp_god, FALSE);
        break;

    default:
        gods_angry(resp_god);
        god_zaps_you(resp_god);
        break;
    }
    u.ublesscnt = rnz(300);
    return;
}

/* helper to print "str appears at your feet", or appropriate */
static void
at_your_feet(const char *str)
{
    if (Blind)
        str = "Something";
    if (Engulfed) {
        /* barrier between you and the floor */
        pline(msgc_youdiscover,
              "%s %s into %s %s.", str, vtense(str, "drop"),
              s_suffix(mon_nam(u.ustuck)), mbodypart(u.ustuck, STOMACH));
    } else {
        pline(msgc_youdiscover,
              "%s %s %s your %s!", str, Blind ? "lands" : vtense(str, "appear"),
              Levitation ? "beneath" : "at", makeplural(body_part(FOOT)));
    }
}

#define ok_wep(o) ((o) && ((o)->oclass == WEAPON_CLASS || is_weptool(o)))
static void
gcrownu(void)
{
    struct obj *obj;
    boolean already_exists, in_hand;
    short class_gift;
    int sp_no;

    HSee_invisible |= FROMOUTSIDE;
    HFire_resistance |= FROMOUTSIDE;
    HCold_resistance |= FROMOUTSIDE;
    HShock_resistance |= FROMOUTSIDE;
    HSleep_resistance |= FROMOUTSIDE;
    HPoison_resistance |= FROMOUTSIDE;
    godvoice(msgc_npcvoice, u.ualign.type, NULL);

    obj = ok_wep(uwep) ? uwep : 0;
    already_exists = in_hand = FALSE;   /* lint suppression */
    switch (u.ualign.type) {
    case A_LAWFUL:
        u.uevent.uhand_of_elbereth = 1;
        verbalize(msgc_intrgain, "I crown thee...  The Hand of Elbereth!");
        historic_event(FALSE, "were crowned as the Hand of Elbereth!");
        break;
    case A_NEUTRAL:
        u.uevent.uhand_of_elbereth = 2;
        in_hand = (uwep && uwep->oartifact == ART_VORPAL_BLADE);
        already_exists = exist_artifact(LONG_SWORD, artiname(ART_VORPAL_BLADE));
        verbalize(msgc_intrgain, "Thou shalt be my Envoy of Balance!");
        historic_event(FALSE, "were named as the Envoy of Balance!");
        break;
    case A_CHAOTIC:
        u.uevent.uhand_of_elbereth = 3;
        in_hand = (uwep && uwep->oartifact == ART_STORMBRINGER);
        already_exists = exist_artifact(RUNESWORD, artiname(ART_STORMBRINGER));
        verbalize(msgc_intrgain, "Thou art chosen to %s for My Glory!",
                  already_exists && !in_hand ? "take lives" : "steal souls");
        historic_event(FALSE, "were chosen to %s for your god's glory!",
                       already_exists &&
                       !in_hand ? "take lives" : "steal souls");
        break;
    }

    class_gift = STRANGE_OBJECT;
    /* 3.3.[01] had this in the A_NEUTRAL case below, preventing chaotic
       wizards from receiving a spellbook */
    if (Role_if(PM_WIZARD) &&
        (!uwep ||
         (uwep->oartifact != ART_VORPAL_BLADE &&
          uwep->oartifact != ART_STORMBRINGER)) &&
        !carrying(SPE_FINGER_OF_DEATH)) {
        class_gift = SPE_FINGER_OF_DEATH;
    make_splbk:
        obj = mksobj(level, class_gift, TRUE, FALSE, rng_main);
        bless(obj);
        obj->bknown = TRUE;
        at_your_feet("A spellbook");
        dropy(obj);
        u.ugifts++;
        /* when getting a new book for known spell, enhance currently wielded
           weapon rather than the book */
        for (sp_no = 0; sp_no < MAXSPELL; sp_no++)
            if (spl_book[sp_no].sp_id == class_gift) {
                if (ok_wep(uwep))
                    obj = uwep; /* to be blessed,&c */
                break;
            }
    } else if (Role_if(PM_MONK) && (!uwep || !uwep->oartifact) &&
               !carrying(SPE_RESTORE_ABILITY)) {
        /* monks rarely wield a weapon */
        class_gift = SPE_RESTORE_ABILITY;
        goto make_splbk;
    }

    switch (u.ualign.type) {
    case A_LAWFUL:
        if (class_gift != STRANGE_OBJECT) {
            ;   /* already got bonus above */
        } else if (obj && obj->otyp == LONG_SWORD && !obj->oartifact) {
            if (!Blind)
                /* worthy of intrgain, but we just printed one of those */
                pline(msgc_itemrepair,
                      "Your sword shines brightly for a moment.");
            obj = oname(obj, artiname(ART_EXCALIBUR));
            if (obj && obj->oartifact == ART_EXCALIBUR)
                u.ugifts++;
        }
        /* acquire Excalibur's skill regardless of weapon or gift */
        unrestrict_weapon_skill(P_LONG_SWORD);
        if (obj && obj->oartifact == ART_EXCALIBUR)
            discover_artifact(ART_EXCALIBUR);
        break;
    case A_NEUTRAL:
        if (class_gift != STRANGE_OBJECT) {
            ;   /* already got bonus above */
        } else if (in_hand) {
            pline(msgc_levelsound, "Your %s goes snicker-snack!", xname(obj));
            obj->dknown = TRUE;
        } else if (!already_exists) {
            obj = mksobj(level, LONG_SWORD, FALSE, FALSE, rng_main);
            obj = oname(obj, artiname(ART_VORPAL_BLADE));
            obj->spe = 1;
            at_your_feet("A sword");
            dropy(obj);
            u.ugifts++;
        }
        /* acquire Vorpal Blade's skill regardless of weapon or gift */
        unrestrict_weapon_skill(P_LONG_SWORD);
        if (obj && obj->oartifact == ART_VORPAL_BLADE)
            discover_artifact(ART_VORPAL_BLADE);
        break;
    case A_CHAOTIC:
        {
            const char *swordbuf = msgcat(hcolor("black"), " sword");

            if (class_gift != STRANGE_OBJECT) {
                ;       /* already got bonus above */
            } else if (in_hand) {
                pline(msgc_levelsound, "Your %s hums ominously!", swordbuf);
                obj->dknown = TRUE;
            } else if (!already_exists) {
                obj = mksobj(level, RUNESWORD, FALSE, FALSE, rng_main);
                obj = oname(obj, artiname(ART_STORMBRINGER));
                at_your_feet(An(swordbuf));
                obj->spe = 1;
                dropy(obj);
                u.ugifts++;
            }
            /* acquire Stormbringer's skill regardless of weapon or gift */
            unrestrict_weapon_skill(P_BROAD_SWORD);
            if (obj && obj->oartifact == ART_STORMBRINGER)
                discover_artifact(ART_STORMBRINGER);
            break;
        }
    default:
        obj = 0;        /* lint */
        break;
    }

    /* enhance weapon regardless of alignment or artifact status */
    if (ok_wep(obj)) {
        bless(obj);
        obj->oeroded = obj->oeroded2 = 0;
        obj->oerodeproof = TRUE;
        obj->bknown = obj->rknown = TRUE;
        if (obj->spe < 1)
            obj->spe = 1;
        /* acquire skill in this weapon */
        unrestrict_weapon_skill(weapon_type(obj));
    } else if (class_gift == STRANGE_OBJECT) {
        /* opportunity knocked, but there was nobody home... */
        pline(msgc_noconsequence, "You feel unworthy.");
    }
    update_inventory();
    return;
}


static void
pleased(aligntyp g_align)
{
    /* don't use saved trouble, worst trouble may get fixed while praying */
    int trouble = in_trouble(); /* what's your worst difficulty? */
    int pat_on_head = 0, kick_on_butt;

    pline(msgc_aligngood, "You feel that %s is %s.", align_gname(g_align),
          u.ualign.record >= DEVOUT ?
          (Hallucination ? "pleased as punch" : "well-pleased") :
          u.ualign.record >= STRIDENT ?
          (Hallucination ? "ticklish" : "pleased") :
          (Hallucination ? "full" : "satisfied"));

    /* not your deity */
    if (on_altar() && turnstate.pray.align != u.ualign.type) {
        adjalign(-1);
        return;
    } else if (u.ualign.record < 2 && trouble <= 0)
        adjalign(1);

    /* depending on your luck & align level, the god you prayed to will: - fix
       your worst problem if it's major. - fix all your major problems. - fix
       your worst problem if it's minor. - fix all of your problems. - do you a
       gratuitous favor.

       if you make it to the the last category, you roll randomly again to see
       what they do for you.

       If your luck is at least 0, then you are guaranteed rescued from your
       worst major problem. */

    if (trouble == ptr_invalid && u.ualign.record >= DEVOUT) {
        /* if hero was in trouble, but got better, no special favor */
        if (turnstate.pray.trouble == ptr_invalid)
            pat_on_head = 1;
    } else {
        int action = rn1(Luck + (on_altar()? 3 + on_shrine() : 2), 1);

        if (!on_altar())
            action = min(action, 3);
        if (u.ualign.record < STRIDENT)
            action = (u.ualign.record > 0 || !rnl(2)) ? 1 : 0;

        switch (min(action, 5)) {
        case 5:
            pat_on_head = 1;
        case 4:
            do
                fix_worst_trouble(trouble);
            while ((trouble = in_trouble()) != 0);
            break;

        case 3:
            fix_worst_trouble(trouble);
        case 2:
            while ((trouble = in_trouble()) > 0)
                fix_worst_trouble(trouble);
            break;

        case 1:
            if (trouble > 0)
                fix_worst_trouble(trouble);
        case 0:
            break;      /* your god blows you off, too bad */
        }
    }

    /* note: can't get pat_on_head unless all troubles have just been fixed or
       there were no troubles to begin with; hallucination won't be in effect
       so special handling for it is superfluous

       Luck input means we can't balance this between games. */
    if (pat_on_head)
        switch (rn2((Luck + 6) >> 1)) {
        case 0:
            break;
        case 1:
            if (uwep &&
                (welded(uwep) || uwep->oclass == WEAPON_CLASS ||
                 is_weptool(uwep))) {
                const char *repair_buf = "";

                if (uwep->oeroded || uwep->oeroded2)
                    repair_buf = msgprintf(" and %s now as good as new",
                                           otense(uwep, "are"));

                if (uwep->cursed) {
                    uncurse(uwep);
                    uwep->bknown = TRUE;
                    if (!Blind)
                        pline(msgc_itemrepair, "Your %s %s%s.",
                              aobjnam(uwep, "softly glow"),
                              hcolor("amber"), repair_buf);
                    else
                        pline(msgc_itemrepair,
                              "You feel the power of %s over your %s.",
                              u_gname(), xname(uwep));
                    repair_buf = "";
                } else if (!uwep->blessed) {
                    bless(uwep);
                    uwep->bknown = TRUE;
                    if (!Blind)
                        pline(msgc_itemrepair, "Your %s with %s aura%s.",
                              aobjnam(uwep, "softly glow"),
                              an(hcolor("light blue")), repair_buf);
                    else
                        pline(msgc_itemrepair,
                              "You feel the blessing of %s over your %s.",
                              u_gname(), xname(uwep));
                    repair_buf = "";
                }

                /* fix any rust/burn/rot damage, but don't protect against
                   future damage */
                if (uwep->oeroded || uwep->oeroded2) {
                    uwep->oeroded = uwep->oeroded2 = 0;
                    /* only give this message if we didn't just bless or
                       uncurse (which has already given a message) */
                    if (*repair_buf)
                        pline(msgc_itemrepair, "Your %s as good as new!",
                              aobjnam(uwep, Blind ? "feel" : "look"));
                }
                update_inventory();
            }
            break;
        case 3:
            /* takes 2 hints to get the music to enter the stronghold */
            if (!u.uevent.uopened_dbridge) {
                if (u.uevent.uheard_tune < 1) {
                    godvoice(msgc_npcvoice, g_align, NULL);
                    verbalize(msgc_npcvoice, "Hark, %s!",
                              mortal_or_creature(youmonst.data, TRUE));
                    verbalize
                        (msgc_hint,
                         "To enter the castle, thou must play the right tune!");
                    u.uevent.uheard_tune++;
                    break;
                } else if (u.uevent.uheard_tune < 2) {
                    You_hear(msgc_npcvoice, "a divine music...");
                    pline(msgc_hint, "It sounds like:  \"%s\".",
                          gamestate.castle_tune);
                    u.uevent.uheard_tune++;
                    break;
                }
            }
            /* Otherwise, falls into next case */
        case 2:
            if (!Blind)
                pline(msgc_statusheal, "You are surrounded by %s glow.",
                      an(hcolor("golden")));
            /* if any levels have been lost (and not yet regained), treat this
               effect like blessed full healing */
            if (u.ulevel < u.ulevelmax) {
                u.ulevelmax -= 1;       /* see potion.c */
                pluslvl(FALSE);
            } else {
                u.uhpmax += 5;
                if (Upolyd)
                    u.mhmax += 5;
            }
            u.uhp = u.uhpmax;
            if (Upolyd)
                u.mh = u.mhmax;
            ABASE(A_STR) = AMAX(A_STR);
            if (u.uhunger < 900)
                init_uhunger();
            if (u.uluck < 0)
                u.uluck = 0;
            make_blinded(0L, TRUE);
            break;
        case 4:{
                struct obj *otmp;
                int any = 0;

                if (Blind)
                    pline(msgc_aligngood, "You feel the power of %s.",
                          u_gname());
                else
                    pline(msgc_aligngood, "You are surrounded by %s aura.",
                          an(hcolor("light blue")));
                for (otmp = invent; otmp; otmp = otmp->nobj) {
                    if (otmp->cursed) {
                        uncurse(otmp);
                        if (!Blind) {
                            pline(msgc_itemrepair, "Your %s %s.",
                                  aobjnam(otmp, "softly glow"),
                                  hcolor("amber"));
                            otmp->bknown = TRUE;
                            ++any;
                        }
                    }
                }
                if (any)
                    update_inventory();
                break;
            }
        case 5:{
                const char *msg = "\"and thus I grant thee the gift of %s!\"";

                godvoice(msgc_aligngood, u.ualign.type,
                         "Thou hast pleased me with thy progress,");
                if (!(HTelepat & INTRINSIC)) {
                    HTelepat |= FROMOUTSIDE;
                    pline(msgc_intrgain, msg, "Telepathy");
                    if (Blind)
                        see_monsters(FALSE);
                } else if (!(HFast & INTRINSIC)) {
                    HFast |= FROMOUTSIDE;
                    pline(msgc_intrgain, msg, "Speed");
                } else if (!(HStealth & INTRINSIC)) {
                    HStealth |= FROMOUTSIDE;
                    pline(msgc_intrgain, msg, "Stealth");
                } else {
                    if (!(HProtection & INTRINSIC)) {
                        HProtection |= FROMOUTSIDE;
                        if (!u.ublessed)
                            u.ublessed = 2 +
                                rn2_on_rng(3, rng_first_protection);
                    } else
                        u.ublessed++;
                    pline(msgc_intrgain, msg, "my protection");
                }
                verbalize(msgc_aligngood, "Use it wisely in my name!");
                break;
            }
        case 7:
        case 8:
        case 9:        /* KMH -- can occur during full moons */
            if (flags.elbereth_enabled)
                if (u.ualign.record >= PIOUS && !u.uevent.uhand_of_elbereth) {
                    gcrownu();
                    break;
                }       /* else FALLTHRU */
        case 6:{
                struct obj *otmp;
                int sp_no, trycnt = u.ulevel + 1;

                at_your_feet("An object");
                /* not yet known spells given preference over already known
                   ones */
                /* Also, try to grant a spell for which there is a skill slot */
                otmp = mkobj(level, SPBOOK_CLASS, TRUE, rng_main);
                while (--trycnt > 0) {
                    if (otmp->otyp != SPE_BLANK_PAPER) {
                        for (sp_no = 0; sp_no < MAXSPELL; sp_no++)
                            if (spl_book[sp_no].sp_id == otmp->otyp)
                                break;
                        if (sp_no == MAXSPELL &&
                            !P_RESTRICTED(spell_skilltype(otmp->otyp)))
                            break;      /* usable, but not yet known */
                    } else {
                        if (!objects[SPE_BLANK_PAPER].oc_name_known ||
                            carrying(MAGIC_MARKER))
                            break;
                    }
                    otmp->otyp =
                        rnd_class(bases[SPBOOK_CLASS], SPE_BLANK_PAPER,
                                  rng_spellbook_gift);
                }
                bless(otmp);
                place_object(otmp, level, u.ux, u.uy);
                break;
            }
        default:
            impossible("Confused deity!");
            break;
        }

    u.ublesscnt = rnz_on_rng(350, rng_prayer_timeout);
    kick_on_butt = u.uevent.udemigod ? 1 : 0;

    if (flags.elbereth_enabled && u.uevent.uhand_of_elbereth)
        kick_on_butt++;

    if (kick_on_butt)
        u.ublesscnt += kick_on_butt * rnz(1000);

    return;
}

/* Either blesses or curses water on the altar. Returns true if it found any
   water here. */
static boolean
water_prayer(boolean bless_water)
{
    struct obj *otmp;
    long changed = 0;
    boolean other = FALSE, bc_known = !(Blind || Hallucination);

    for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere) {
        /* turn water into (un)holy water */
        if (otmp->otyp == POT_WATER &&
            (bless_water ? !otmp->blessed : !otmp->cursed)) {
            otmp->blessed = bless_water;
            otmp->cursed = !bless_water;
            otmp->bknown = bc_known;
            changed += otmp->quan;
        } else if (otmp->oclass == POTION_CLASS)
            other = TRUE;
    }
    if (!Blind && changed) {
        pline(bless_water ? msgc_itemrepair : msgc_itemloss,
              "%s potion%s on the altar glow%s %s for a moment.",
              ((other && changed > 1L) ? "Some of the" :
               (other ? "One of the" : "The")),
              ((other || changed > 1L) ? "s" : ""), (changed > 1L ? "" : "s"),
              (bless_water ? hcolor("light blue") : hcolor("black")));
    }
    return (boolean) (changed > 0L);
}

static void
godvoice(enum msg_channel msgc, aligntyp g_align, const char *words)
{
    const char *quot = "";

    if (words)
        quot = "\"";
    else
        words = "";

    pline(msgc, "The voice of %s %s: %s%s%s", align_gname(g_align),
          godvoices[rn2(SIZE(godvoices))], quot, words, quot);
}

static void
gods_angry(aligntyp g_align)
{
    godvoice(msgc_alignbad, g_align, "Thou hast angered me.");
}

/* The g_align god is upset with you. */
static void
gods_upset(aligntyp g_align)
{
    if (g_align == u.ualign.type)
        u.ugangr++;
    else if (u.ugangr)
        u.ugangr--;
    angrygods(g_align);
}

static const char sacrifice_types[] = { FOOD_CLASS, AMULET_CLASS, 0 };

static void
consume_offering(struct obj *otmp)
{
    if (Hallucination)
        switch (rn2(3)) {
        case 0:
            pline(msgc_actionok, "Your sacrifice sprouts wings and a "
                  "propeller and roars away!");
            break;
        case 1:
            pline(msgc_actionok, "Your sacrifice puffs up, swelling bigger and "
                  "bigger, and pops!");
            break;
        case 2:
            pline(msgc_actionok, "Your sacrifice collapses into a cloud of "
                  "dancing particles and fades away!");
            break;
    } else if (Blind && u.ualign.type == A_LAWFUL)
        pline(msgc_actionok, "Your sacrifice disappears!");
    else
        pline(msgc_actionok, "Your sacrifice is consumed in a %s!",
              u.ualign.type == A_LAWFUL ? "flash of light" : "burst of flame");
    if (carried(otmp))
        useup(otmp);
    else
        useupf(otmp, 1L);
    exercise(A_WIS, TRUE);
}


int
dosacrifice(const struct nh_cmd_arg *arg)
{
    int value = 0;
    int pm;
    aligntyp altaralign = a_align(u.ux, u.uy);
    boolean sanctum = level->locations[u.ux][u.uy].altarmask & AM_SANCTUM;
    int cnt; /* for loop initial declarations are only allowed in C99 mode */
    struct obj *otmp;

    if (!on_altar() || Engulfed) {
        pline(msgc_cancelled, "You are not standing on an altar.");
        return 0;
    }

    /* TODO: Proper handling for amulet sacrifices outside the Sanctum, and food
       sacrifices inside. (Note: Rider corpses are meant to be non-sacrificable,
       but making the interface refuse to accept the keystrokes is probably the
       wrong implementation.) */
    if (sanctum && altaralign != A_NONE) {
        otmp = getargobj(arg, sacrifice_types, "sacrifice");
        if (!otmp)
            return 0;
    } else {
        /* This implementation will, if the user command-repeats a sacrifice
           from the ground, ask for another item from the ground. This is
           reasonable behaviour. */
        otmp = floorfood("sacrifice", arg);
        if (!otmp)
            return 0;
    }
    /*
       Was based on nutritional value and aging behavior (< 50 moves).
       Sacrificing a food ration got you max luck instantly, making the gods as
       easy to please as an angry dog!

       Now only accepts corpses, based on the game's evaluation of their
       toughness.  Human and pet sacrifice, as well as sacrificing unicorns of
       your alignment, is strongly discouraged. */

#define MAXVALUE 24     /* Highest corpse value (besides Wiz) */

    if (otmp->otyp == CORPSE) {
        const struct permonst *ptr = &mons[otmp->corpsenm];
        struct monst *mtmp;
        extern const int monstr[];

        /* KMH, conduct */
        break_conduct(conduct_gnostic);

        /* you're handling this corpse, even if it was killed upon the altar */
        feel_cockatrice(otmp, TRUE, "sacrificing");

        if (otmp->corpsenm == PM_ACID_BLOB ||
            (moves <= peek_at_iced_corpse_age(otmp) + 50)) {
            value = monstr[otmp->corpsenm] + 1;
            if (otmp->oeaten)
                value = eaten_stat(value, otmp);
        }

        if (your_race(ptr)) {
            if (is_demon(youmonst.data)) {
                pline(msgc_aligngood, "You find the idea very satisfying.");
                exercise(A_WIS, TRUE);
            } else if (u.ualign.type != A_CHAOTIC) {
                pline(msgc_alignbad, "You'll regret this infamous offense!");
                exercise(A_WIS, FALSE);
            }

            if (altaralign != A_CHAOTIC && altaralign != A_NONE) {
                /* curse the lawful/neutral altar */
                pline(msgc_substitute, "The altar is stained with %s blood.",
                      urace.adj);
                if (!sanctum) {
                    /* This is supposed to be &= */
                    level->locations[u.ux][u.uy].altarmask &=
                        AM_SHRINE & AM_SANCTUM;
                    level->locations[u.ux][u.uy].altarmask |= AM_CHAOTIC;
                }
                angry_priest();
            } else {
                struct monst *dmon;
                const char *demonless_msg;

                /* Human sacrifice on a chaotic or unaligned altar */
                /* is equivalent to demon summoning */
                if (altaralign == A_CHAOTIC && u.ualign.type != A_CHAOTIC &&
                    !sanctum) {
                    pline(msgc_badidea, "The blood floods the altar, which "
                          "vanishes in %s cloud!", an(hcolor("black")));
                    level->locations[u.ux][u.uy].typ = ROOM;
                    level->locations[u.ux][u.uy].altarmask = 0;
                    newsym(u.ux, u.uy);
                    angry_priest();
                    demonless_msg = "cloud dissipates";
                } else {
                    /* either you're chaotic or altar is Moloch's or both */
                    pline(msgc_actionok, "The blood covers the altar!");
                    change_luck(altaralign == u.ualign.type ? 2 : -2);
                    demonless_msg = "blood coagulates";
                }
                if ((pm = dlord(altaralign)) != NON_PM &&
                    ((dmon =
                      makemon(&mons[pm], level, u.ux, u.uy, NO_MM_FLAGS)))) {
                    pline(msgc_levelwarning, "You have summoned %s!",
                          a_monnam(dmon));
                    if (sgn(u.ualign.type) == sgn(dmon->data->maligntyp))
                        msethostility(dmon, FALSE, FALSE); /* TODO: reset? */
                    pline(msgc_statusbad,
                          "You are terrified, and unable to move.");
                    helpless(3, hr_afraid, "being terrified of a demon",
                             NULL);
                } else
                    pline(msgc_noconsequence, "The %s.", demonless_msg);
            }

            if (u.ualign.type != A_CHAOTIC) {
                adjalign(-5);
                u.ugangr += 3;
                adjattrib(A_WIS, -1, TRUE);
                if (!Inhell)
                    angrygods(u.ualign.type);
                change_luck(-5);
            } else
                adjalign(5);
            if (carried(otmp))
                useup(otmp);
            else
                useupf(otmp, 1L);
            return 1;
        } else if (otmp->oxlth && otmp->oattached == OATTACHED_MONST &&
                   ((mtmp = get_mtraits(otmp, FALSE)) != NULL)
                   && mtmp->mtame) {
            /* mtmp is a temporary pointer to a tame monster's attributes, not
               a real monster */
            pline(msgc_alignbad, "So this is how you repay loyalty?");
            adjalign(-3);
            value = -1;
            HAggravate_monster |= FROMOUTSIDE;
        } else if (is_undead(ptr)) {    /* Not demons--no demon corpses */
            if (u.ualign.type != A_CHAOTIC)
                value += 1;
        } else if (is_unicorn(ptr)) {
            int unicalign = sgn(ptr->maligntyp);

            /* If same as altar, always a very bad action. */
            if (unicalign == altaralign) {
                pline(msgc_alignbad, "Such an action is an insult to %s!",
                      (unicalign == A_CHAOTIC)
                      ? "chaos" : unicalign ? "law" : "balance");
                adjattrib(A_WIS, -1, TRUE);
                value = -5;
            } else if (u.ualign.type == altaralign) {
                /* If different from altar, and altar is same as yours, */
                /* it's a very good action */
                if (u.ualign.record < ALIGNLIM)
                    pline(msgc_aligngood, "You feel appropriately %s.",
                          align_str(u.ualign.type));
                else
                    pline(msgc_alignbad,
                          "You feel you are thoroughly on the right path.");
                adjalign(5);
                value += 3;
            } else
                /* If sacrificing unicorn of your alignment to altar not of */
                /* your alignment, your god gets angry and it's a conversion */
            if (unicalign == u.ualign.type) {
                for (cnt = 0; cnt <= u.ualign.record; cnt++) {
                    break_conduct(conduct_lostalign);
                }
                u.ualign.record = -1;
                value = 1;
            } else
                value += 3;
        }
    }
    /* corpse */
    if (otmp->otyp == AMULET_OF_YENDOR) {
        if (!sanctum || altaralign == A_NONE) {
            if (Hallucination)
                pline(msgc_hint, "You feel homesick.");
            else
                pline(msgc_hint, "You feel an urge to return to the surface.");
            return 1;
        } else {
            /* The final Test. Did you win? */
            if (uamul == otmp)
                setequip(os_amul, NULL, em_voluntary);
            u.uevent.ascended = 1;
            if (carried(otmp))
                useup(otmp);    /* well, it's gone now */
            else
                useupf(otmp, 1L);
            pline(msgc_occstart, "You offer the Amulet of Yendor to %s...",
                  a_gname());
            if (u.ualign.type != altaralign) {
                /* And the opposing team picks you up and carries you off on
                   their shoulders */
                adjalign(-99);
                pline(msgc_outrobad,
                      "%s accepts your gift, and gains dominion over %s...",
                      a_gname(), u_gname());
                pline(msgc_outrobad, "%s is enraged...", u_gname());
                pline(msgc_outrobad, "Fortunately, %s permits you to live...",
                      a_gname());
                pline(msgc_outrobad, "A cloud of %s smoke surrounds you...",
                      hcolor("orange"));
                done(ESCAPED, NULL); /* "in celestial disgrace" added later */
            } else {    /* super big win */
                adjalign(10);
                pline(msgc_outrogood,
                      "An invisible choir sings, and you are bathed in "
                      "radiance...");
                godvoice(msgc_outrogood, altaralign,
                         msgcat_many("Congratulations, ",
                                     mortal_or_creature(youmonst.data, TRUE),
                                     "!", NULL));
                win_pause_output(P_MESSAGE);
                verbalize(
                    msgc_outrogood,
                    "In return for thy service, I grant thee the gift of %s",
                    is_undead(youmonst.data) || nonliving(youmonst.data) ?
                    "Eternal Power!" : "Immortality!");
                pline(msgc_outrogood,
                      "You ascend to the status of Demigod%s...",
                      u.ufemale ? "dess" : "");
                historic_event(FALSE,
                               "offered the Amulet of Yendor to %s and ascended"
                               " to the status of Demigod%s!", u_gname(),
                               u.ufemale ? "dess" : "");
                done(ASCENDED, NULL);
            }
        }
    } /* real Amulet */

    if (otmp->otyp == FAKE_AMULET_OF_YENDOR) {
        if (canhear())
            You_hear(msgc_alignbad, "a nearby thunderclap.");
        if (!otmp->known) {
            pline_implied(msgc_hint, "You realize you have made a %s.",
                          Hallucination ? "boo-boo" : "mistake");
            otmp->known = TRUE;
            change_luck(-1);
            return 1;
        } else {
            /* don't you dare try to fool the gods */
            change_luck(-3);
            adjalign(-1);
            u.ugangr += 3;
            value = -3;
        }
    }

    /* fake Amulet */
    if (value == 0) {
        pline(msgc_noconsequence, "Nothing happens.");
        return 1;
    }

    if (altaralign != u.ualign.type && sanctum) {
        /*
         * REAL BAD NEWS!!! High altars cannot be converted.  Even an attempt
         * gets the god who owns it truly pissed off.
         */
        pline_implied(msgc_levelwarning,
                      "You feel the air around you grow charged...");
        pline_implied(msgc_alignbad,
                      "Suddenly, you realize that %s has noticed you...", a_gname());
        godvoice(msgc_alignbad, altaralign, msgcat_many(
                     "So, ", mortal_or_creature(youmonst.data, TRUE),
                     "!  You dare desecrate my High Temple!", NULL));
        /* Throw everything we have at the player */
        god_zaps_you(altaralign);
    } else if (value < 0) {     /* I don't think the gods are gonna like
                                   this... */
        gods_upset(altaralign);
    } else {
        int saved_anger = u.ugangr;
        int saved_cnt = u.ublesscnt;
        int saved_luck = u.uluck;

        /* Sacrificing at an altar of a different alignment */
        if (u.ualign.type != altaralign) {
            /* Is this a conversion ? */
            /* An unaligned altar in Gehennom will always elicit rejection. */
            if (ugod_is_angry() || (altaralign == A_NONE && Inhell)) {
                if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL] &&
                    altaralign != A_NONE) {
                    pline_implied(
                        msgc_alignbad,
                        "You have a strong feeling that %s is angry...",
                        u_gname());
                    consume_offering(otmp);
                    pline_implied(msgc_intrloss, "%s accepts your allegiance.",
                                  a_gname());

                    /* The player wears a helm of opposite alignment? */
                    if (uarmh && uarmh->otyp == HELM_OF_OPPOSITE_ALIGNMENT)
                        u.ualignbase[A_CURRENT] = altaralign;
                    else
                        u.ualign.type = u.ualignbase[A_CURRENT] = altaralign;
                    u.ublessed = 0;

                    pline(msgc_intrgain,
                          "You have a sudden sense of a new direction.");
                    /* Beware, Conversion is costly */
                    change_luck(-3);
                    u.ublesscnt += 300;
                    adjalign((int)(u.ualignbase[A_ORIGINAL] * (ALIGNLIM / 2)));
                } else {
                    u.ugangr += 3;
                    adjalign(-5);
                    pline_implied(msgc_alignbad, "%s rejects your sacrifice!",
                                  a_gname());
                    godvoice(msgc_alignbad, altaralign, "Suffer, infidel!");
                    change_luck(-5);
                    adjattrib(A_WIS, -2, TRUE);
                    if (!Inhell)
                        angrygods(u.ualign.type);
                }
                return 1;
            } else {
                consume_offering(otmp);
                pline_implied(
                    msgc_occstart, "You sense a conflict between %s and %s.",
                    u_gname(), a_gname());
                if (rn2_on_rng(8 + u.ulevel, rng_altar_convert) > 5) {
                    struct monst *pri;

                    pline(msgc_intrgain, "You feel the power of %s increase.",
                          u_gname());
                    exercise(A_WIS, TRUE);
                    change_luck(1);
                    /* Yes, this is supposed to be &=, not |= */
                    level->locations[u.ux][u.uy].altarmask &=
                        AM_SHRINE & AM_SANCTUM;
                    level->locations[u.ux][u.uy].altarmask |=
                        Align2amask(u.ualign.type);
                    if (!Blind)
                        pline_implied(
                            msgc_info, "The altar glows %s.",
                            hcolor(u.ualign.type == A_LAWFUL ? "white" :
                                   u.ualign.type ? "black" : "gray"));

                    if (rnl(u.ulevel) > 6 && u.ualign.record > 0 &&
                        rnd(u.ualign.record) > (3 * ALIGNLIM) / 4)
                        summon_minion(altaralign, TRUE);
                    /* anger priest; test handles bones files */
                    if ((pri = findpriest(temple_occupied(u.urooms))) &&
                        !p_coaligned(pri))
                        angry_priest();
                } else {
                    pline(msgc_failrandom,
                          "Unluckily, you feel the power of %s decrease.",
                          u_gname());
                    change_luck(-1);
                    exercise(A_WIS, FALSE);
                    if (rnl(u.ulevel) > 6 && u.ualign.record > 0 &&
                        rnd(u.ualign.record) > (7 * ALIGNLIM) / 8)
                        summon_minion(altaralign, TRUE);
                }
                return 1;
            }
        }

        consume_offering(otmp);
        /* OK, you get brownie points. */
        if (u.ugangr) {
            u.ugangr -=
                ((value * (u.ualign.type == A_CHAOTIC ? 2 : 3)) / MAXVALUE);
            if (u.ugangr < 0)
                u.ugangr = 0;
            if (u.ugangr != saved_anger) {
                if (u.ugangr) {
                    pline(msgc_aligngood, "%s seems %s.", u_gname(),
                          Hallucination ? "groovy" : "slightly mollified");

                    if ((int)u.uluck < 0)
                        change_luck(1);
                } else {
                    pline(msgc_statusheal, "%s seems %s.", u_gname(),
                          Hallucination ? "cosmic (not a new fact)" :
                          "mollified");

                    if ((int)u.uluck < 0)
                        u.uluck = 0;
                }
            } else {    /* not satisfied yet */
                if (Hallucination)
                    pline(msgc_yafm, "The gods seem tall.");
                else
                    pline(msgc_yafm, "You have a feeling of inadequacy.");
            }
        } else if (ugod_is_angry()) {
            if (value > MAXVALUE)
                value = MAXVALUE;
            if (value > -u.ualign.record)
                value = -u.ualign.record;
            adjalign(value);
            pline(msgc_aligngood, "You feel partially absolved.");
        } else if (u.ublesscnt > 0) {
            u.ublesscnt -=
                ((value * (u.ualign.type == A_CHAOTIC ? 500 : 300)) / MAXVALUE);
            if (u.ublesscnt < 0)
                u.ublesscnt = 0;
            if (u.ublesscnt != saved_cnt) {
                if (u.ublesscnt) {
                    if (Hallucination)
                        pline(msgc_aligngood, "You realize that the gods are "
                              "not like you and I.");
                    else
                        pline(msgc_aligngood, "You have a hopeful feeling.");
                    if ((int)u.uluck < 0)
                        change_luck(1);
                } else {
                    if (Hallucination)
                        pline(msgc_statusheal,
                              "Overall, there is a smell of fried onions.");
                    else
                        pline(msgc_statusheal,
                              "You have a feeling of reconciliation.");
                    if ((int)u.uluck < 0)
                        u.uluck = 0;
                }
            }
        } else {
            int nartifacts = nartifact_exist();

            /* you were already in pretty good standing */
            /* The player can gain an artifact */
            /* The chance goes down as the number of artifacts goes up */
            if (u.ulevel > 2 && u.uluck >= 0) {
                if (!rn2_on_rng(10 + (2 * u.ugifts * nartifacts),
                                rng_altar_gift)) {
                    otmp = mk_artifact(level, NULL, a_align(u.ux, u.uy),
                                       rng_altar_gift);
                    if (otmp) {
                        if (otmp->spe < 0)
                            otmp->spe = 0;
                        if (otmp->cursed)
                            uncurse(otmp);
                        otmp->oerodeproof = TRUE;
                        dropy(otmp);
                        at_your_feet("An object");
                        godvoice(msgc_aligngood, u.ualign.type,
                                 "Use my gift wisely!");
                        historic_event(FALSE, "received %s from %s.",
                                       artiname(otmp->oartifact), u_gname());
                        u.ugifts++;
                        u.ublesscnt = rnz(300 + (50 * nartifacts));
                        exercise(A_WIS, TRUE);
                        /* make sure we can use this weapon */
                        unrestrict_weapon_skill(weapon_type(otmp));
                        discover_artifact(otmp->oartifact);
                        return 1;
                    }
                } else {
                    /* we used a seed from rng_altar_gift but didn't get an
                       artifact; burn a seed to match the one that would be
                       consumed in mk_artifact (relevant if nartifacts is
                       different between games) */
                    rn2_on_rng(27720, rng_altar_gift);
                }
            }
            change_luck((value * LUCKMAX) / (MAXVALUE * 2));
            if ((int)u.uluck < 0)
                u.uluck = 0;
            if (u.uluck != saved_luck) {
                if (Blind)
                    pline(msgc_aligngood,
                          "You think something brushed your %s.",
                          body_part(FOOT));
                else
                    pline(msgc_aligngood, Hallucination ? "You see crabgrass "
                          "at your %s.  A funny thing in a dungeon." :
                          "You glimpse a four-leaf clover at your %s.",
                          makeplural(body_part(FOOT)));
            }
        }
    }
    return 1;
}


/* determine prayer results in advance; also used for enlightenment */
/* praying: false means no messages should be given */
boolean
can_pray(boolean praying)
{
    int alignment;

    aligntyp align = on_altar()? a_align(u.ux, u.uy) : u.ualign.type;

    if (is_demon(youmonst.data) && (align != A_CHAOTIC)) {
        if (praying)
            pline(msgc_cancelled,
                  "The very idea of praying to a %s god is repugnant to you.",
                  align ? "lawful" : "neutral");
        return FALSE;
    }

    turnstate.pray.align = align;
    enum pray_trouble trouble = in_trouble();
    turnstate.pray.trouble = trouble;

    if (praying)
        pline(msgc_occstart, "You begin praying to %s.", align_gname(align));

    if (u.ualign.type && u.ualign.type == -align)
        alignment = -u.ualign.record;   /* Opposite alignment altar */
    else if (u.ualign.type != align)
        alignment = u.ualign.record / 2;        /* Different alignment altar */
    else
        alignment = u.ualign.record;

    if ((trouble >= ptr_first_major && trouble <= ptr_last_major)
          ? (u.ublesscnt > 200) : /* big trouble */
        (trouble >= ptr_first_minor && trouble <= ptr_last_minor)
          ? (u.ublesscnt > 100) : /* minor difficulties */
        (u.ublesscnt > 0)) {     /* not in trouble */
        turnstate.pray.type = pty_too_soon;     /* too soon... */
    } else if ((int)Luck < 0 || u.ugangr || alignment < 0)
        turnstate.pray.type = pty_anger;     /* too naughty... */
    else {      /* alignment >= 0 */
        if (on_altar() && u.ualign.type != align)
            turnstate.pray.type = pty_conversion;
        else
            turnstate.pray.type = pty_favour;
    }

    if (is_undead(youmonst.data) && !Inhell &&
        (align == A_LAWFUL || (align == A_NEUTRAL && !rn2(10))))
        turnstate.pray.type = pty_smite_undead;
    /* Note: when !praying, the random factor for neutrals makes the return
       value a non-deterministic approximation for enlightenment. This case
       should be uncommon enough to live with... */

    return !praying ? turnstate.pray.type == pty_favour && !Inhell : TRUE;
}

int
dopray(const struct nh_cmd_arg *arg)
{
    (void) arg;

    /* Confirm accidental slips of Alt-P */
    if (flags.prayconfirm)
        if (yn("Are you sure you want to pray?") == 'n')
            return 0;

    break_conduct(conduct_gnostic);

    /* set up turnstate alignment and trouble */
    if (!can_pray(TRUE))
        return 0;

    if (wizard && turnstate.pray.type != pty_smite_undead) {
        if (yn("Force the gods to be pleased?") == 'y') {
            u.ublesscnt = 0;
            if (u.uluck < 0)
                u.uluck = 0;
            if (u.ualign.record <= 0)
                u.ualign.record = 1;
            u.ugangr = 0;
            turnstate.pray.type = pty_favour;
        }
    }

    if (turnstate.pray.type == pty_favour && !Inhell) {
        /* if you've been true to your god you can't die while you pray */
        if (!Blind)
            pline(msgc_statusgood, "You are surrounded by a shimmering light.");
        u.uinvulnerable = TRUE;
    }

    helpless(3, hr_praying, "praying", NULL);

    return 1;
}

void
prayer_done(void)
{
    aligntyp alignment = turnstate.pray.align;

    u.uinvulnerable = FALSE;

    if (turnstate.pray.type == pty_smite_undead) {
        godvoice(msgc_alignbad, alignment,
                 alignment ==
                 A_LAWFUL ? "Vile creature, thou durst call upon me?" :
                 "Walk no more, perversion of nature!");
        pline(msgc_statusend, "You feel like you are falling apart.");
        /* KMH -- Gods have mastery over unchanging */
        rehumanize(DIED, NULL);
        losehp(rnd(20), killer_msg(DIED, "an residual undead turning effect"));
        exercise(A_CON, FALSE);
    } else if (Inhell) {
        pline(msgc_substitute,
              "Since you are in Gehennom, %s won't help you.",
              align_gname(alignment));
        /* haltingly aligned is least likely to anger */
        if (u.ualign.record <= 0 || rnl(u.ualign.record))
            angrygods(u.ualign.type);
    } else if (turnstate.pray.type == pty_too_soon) {
        if (on_altar() && u.ualign.type != alignment)
            water_prayer(FALSE);
        u.ublesscnt += rnz(250);
        change_luck(-3);
        gods_upset(u.ualign.type);
    } else if (turnstate.pray.type == pty_anger) {
        if (on_altar() && u.ualign.type != alignment)
            water_prayer(FALSE);
        angrygods(u.ualign.type);       /* naughty */
    } else if (turnstate.pray.type == pty_conversion) {
        if (water_prayer(FALSE)) {
            /* attempted water prayer on a non-coaligned altar */
            u.ublesscnt += rnz(250);
            change_luck(-3);
            gods_upset(u.ualign.type);
        } else
            pleased(alignment);
    } else if (turnstate.pray.type == pty_favour) {
        /* coaligned */
        if (on_altar())
            water_prayer(TRUE);
        pleased(alignment);     /* nice */
    } else {
        impossible("Invalid prayer type in prayer_done");
    }

    turnstate.pray.align = A_NONE;
    turnstate.pray.type = pty_invalid;
    turnstate.pray.trouble = ptr_invalid;
}

int
doturn(const struct nh_cmd_arg *arg)
{
    struct monst *mtmp, *mtmp2;
    int once, range, xlev;

    (void) arg;

    if (!supernatural_ability_available(SPID_TURN)) {
        /* Try to use turn undead spell. */
        if (objects[SPE_TURN_UNDEAD].oc_name_known) {
            int sp_no;

            for (sp_no = 0;
                 sp_no < MAXSPELL && spl_book[sp_no].sp_id != SPE_TURN_UNDEAD;
                 sp_no++)
                ;

            if (sp_no < MAXSPELL && spl_book[sp_no].sp_id == SPE_TURN_UNDEAD &&
                spellknow(sp_no) > 0)
                return spelleffects(sp_no, TRUE, arg);
        }

        pline(msgc_cancelled, "You don't know how to turn undead!");
        return 0;
    }
    break_conduct(conduct_gnostic);

    if ((u.ualign.type != A_CHAOTIC &&
         (is_demon(youmonst.data) || is_undead(youmonst.data))) ||
        u.ugangr > 6   /* "Die, mortal!" */ ) {

        pline(msgc_failcurse, "For some reason, %s seems to ignore you.",
              u_gname());
        aggravate();
        exercise(A_WIS, FALSE);
        return 0;
    }

    if (Inhell) {
        pline(msgc_badidea, "Since you are in Gehennom, %s won't help you.",
              u_gname());
        aggravate();
        return 0;
    }
    pline(msgc_occstart, "Calling upon %s, you chant an arcane formula.",
          u_gname());
    exercise(A_WIS, TRUE);

    /* note: does not perform unturn_dead() on victims' inventories */
    range = BOLT_LIM + (u.ulevel / 5);  /* 5 to 11 */
    range *= range;
    once = 0;
    for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;

        if (DEADMONSTER(mtmp))
            continue;
        if (!cansee(mtmp->mx, mtmp->my) || distu(mtmp->mx, mtmp->my) > range)
            continue;

        if (!mtmp->mpeaceful &&
            (is_undead(mtmp->data) ||
             (is_demon(mtmp->data) && (u.ulevel > (MAXULEV / 2))))) {

            mtmp->msleeping = 0;
            if (Confusion) {
                if (!once++)
                    pline(msgc_substitute,
                          "Unfortunately, your voice falters.");
                mtmp->mflee = 0;
                mtmp->mfrozen = 0;
                mtmp->mcanmove = 1;
            } else if (!resist(mtmp, '\0', 0, TELL)) {
                xlev = 6;
                switch (mtmp->data->mlet) {
                    /* this is intentional, lichs are tougher than zombies. */
                case S_LICH:
                    xlev += 4; /*FALLTHRU*/
                case S_VAMPIRE:
                    xlev += 2; /*FALLTHRU*/
                case S_WRAITH:
                    if (noncorporeal(mtmp->data)) /* actually a ghost */
                        xlev += 4;
                    xlev += 2; /*FALLTHRU*/
                case S_MUMMY:
                    xlev += 2; /*FALLTHRU*/
                case S_ZOMBIE:
                    if (u.ulevel >= xlev && !resist(mtmp, '\0', 0, NOTELL)) {
                        if (u.ualign.type == A_CHAOTIC)
                            msethostility(mtmp, FALSE, TRUE);
                        else
                            killed(mtmp);
                        break;
                    }   /* else flee */
                 /*FALLTHRU*/ default:
                    monflee(mtmp, 0, FALSE, TRUE);
                    break;
                }
            }
        }
    }
    helpless(5, hr_busy, "trying to turn the monsters", NULL);
    return 1;
}

const char *
a_gname(void)
{
    return a_gname_at(u.ux, u.uy);
}

/* returns the name of an altar's deity */
const char *
a_gname_at(xchar x, xchar y)
{
    if (!IS_ALTAR(level->locations[x][y].typ))
        return NULL;

    return align_gname(a_align(x, y));
}

/* returns the name of the player's deity */
const char *
u_gname(void)
{
    return align_gname(u.ualign.type);
}

const char *
align_gname(aligntyp alignment)
{
    const char *gnam;

    switch (alignment) {
    case A_NONE:
        gnam = Moloch;
        break;
    case A_LAWFUL:
        gnam = urole.lgod;
        break;
    case A_NEUTRAL:
        gnam = urole.ngod;
        break;
    case A_CHAOTIC:
        gnam = urole.cgod;
        break;
    default:
        impossible("unknown alignment.");
        gnam = "someone";
        break;
    }
    if (*gnam == '_')
        ++gnam;
    return gnam;
}

static const char *hallu_gods[] = {
    "the Flying Spaghetti Monster",     /* Church of the FSM */
    "Eris",     /* Discordianism */
    "the Martians",     /* every science fiction ever */
    "Xom",      /* Crawl */
    "AnDoR dRaKoN",     /* ADOM */
    "the Central Bank of Yendor",       /* economics */
    "Tooth Fairy",      /* real world(?) */
    "Om",       /* Discworld */
    "Yawgmoth", /* Magic: the Gathering */
    "Morgoth", /* LoTR */
    "Cthulhu", /* Lovecraft */
    "the Ori", /* Stargate */
    "destiny", /* why not? */
    "your Friend the Computer", /* Paranoia */
};

/* hallucination handling for priest/minion names: select a random god
   iff character is hallucinating */
const char *
halu_gname(aligntyp alignment)
{
    const char *gnam = NULL;
    int which;

    if (!Hallucination)
        return align_gname(alignment);

    do
        which = randrole(rng_display);
    while (!roles[which].lgod);

    switch (rn2_on_display_rng(9)) {
    case 0:
    case 1:
        gnam = roles[which].lgod;
        break;
    case 2:
    case 3:
        gnam = roles[which].ngod;
        break;
    case 4:
    case 5:
        gnam = roles[which].cgod;
        break;
    case 6:
    case 7:
        gnam = hallu_gods[rn2_on_display_rng(
                sizeof hallu_gods / sizeof *hallu_gods)];
        break;
    case 8:
        gnam = Moloch;
        break;
    default:
        impossible("rn2_on_display_rng broken in halu_gname?!?");
    }
    if (!gnam) {
        impossible("No random god name?");
        gnam = "your Friend the Computer"; /* Paranoia */
    }
    if (*gnam == '_')
        ++gnam;
    return gnam;
}

/* deity's title */
const char *
align_gtitle(aligntyp alignment)
{
    const char *gnam, *result = "god";

    switch (alignment) {
    case A_LAWFUL:
        gnam = urole.lgod;
        break;
    case A_NEUTRAL:
        gnam = urole.ngod;
        break;
    case A_CHAOTIC:
        gnam = urole.cgod;
        break;
    default:
        gnam = 0;
        break;
    }
    if (gnam && *gnam == '_')
        result = "goddess";
    return result;
}

void
altar_wrath(int x, int y)
{
    aligntyp altaralign = a_align(x, y);

    if (!strcmp(align_gname(altaralign), u_gname())) {
        godvoice(msgc_alignbad, altaralign,
                 "How darest thou desecrate my altar!");
        adjattrib(A_WIS, -1, FALSE);
    } else {
        pline(msgc_npcvoice, "A voice (could it be %s?) whispers:",
              align_gname(altaralign));
        verbalize(msgc_alignbad, "Thou shalt pay, infidel!");
        change_luck(-1);
    }
}

/* assumes isok() at one space away, but not necessarily at two */
static boolean
blocked_boulder(int dx, int dy)
{
    struct obj *otmp;
    long count = 0L;

    for (otmp = level->objects[u.ux + dx][u.uy + dy]; otmp;
         otmp = otmp->nexthere) {
        if (otmp->otyp == BOULDER)
            count += otmp->quan;
    }

    switch (count) {
    case 0:
        return FALSE;   /* no boulders--not blocked */
    case 1:
        break;  /* possibly blocked depending on if it's pushable */
    default:
        return TRUE;    /* >1 boulder--blocked after they push the top one;
                           don't force them to push it first to find out */
    }

    if (!isok(u.ux + 2 * dx, u.uy + 2 * dy))
        return TRUE;
    if (IS_ROCK(level->locations[u.ux + 2 * dx][u.uy + 2 * dy].typ))
        return TRUE;
    if (sobj_at(BOULDER, level, u.ux + 2 * dx, u.uy + 2 * dy))
        return TRUE;

    return FALSE;
}

/*pray.c*/
