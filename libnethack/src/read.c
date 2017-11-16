/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* KMH -- Copied from pray.c; this really belongs in a header file */
#define DEVOUT 14
#define STRIDENT 4

#define Your_Own_Role(mndx) \
        ((mndx) == urole.malenum || \
         (urole.femalenum != NON_PM && (mndx) == urole.femalenum))
#define Your_Own_Race(mndx) \
        ((mndx) == urace.malenum || \
         (urace.femalenum != NON_PM && (mndx) == urace.femalenum))

static const char readable[] = { ALL_CLASSES, 0 };
static const char all_count[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

static void wand_explode(struct obj *);
static void do_class_genocide(void);
static void stripspe(struct obj *);
static void p_glow1(enum msg_channel, struct obj *);
static void p_glow2(enum msg_channel, struct obj *, const char *);
static void randomize(int *, int);
static void forget_single_object(int);
static void forget(int);
static void maybe_tame(struct monst *, struct obj *);

static void set_lit(int, int, void *);

int
doread(const struct nh_cmd_arg *arg)
{
    boolean confused, known;
    struct obj *scroll;

    known = FALSE;
    if (check_capacity(NULL))
        return 0;

    scroll = getargobj(arg, readable, "read");
    if (!scroll)
        return 0;

    /* outrumor has its own blindness check */
    if (scroll->otyp == FORTUNE_COOKIE) {
        pline(msgc_occstart,
              "You break up the cookie and throw away the pieces.");
        outrumor(bcsign(scroll), BY_COOKIE);
        if (!Blind)
            break_conduct(conduct_illiterate);
        useup(scroll);
        return 1;
    } else if (scroll->otyp == T_SHIRT) {
        static const char *const shirt_msgs[] = {       /* Scott Bigham */
            "I explored the Dungeons of Doom and all I got was this lousy "
                "T-shirt!",
            "Is that Mjollnir in your pocket or are you just happy to see me?",
            "It's not the size of your sword, it's how #enhance'd you are "
                "with it.",
            "Madame Elvira's House O' Succubi Lifetime Customer",
            "Madame Elvira's House O' Succubi Employee of the Month",
            "Ludios Vault Guards Do It In Small, Dark Rooms",
            "Yendor Military Soldiers Do It In Large Groups",
            "I survived Yendor Military Boot Camp",
            "Ludios Accounting School Intra-Mural Lacrosse Team",
            "Oracle(TM) Fountains 10th Annual Wet T-Shirt Contest",
            "Hey, black dragon!  Disintegrate THIS!",
            "I'm With Stupid -->",
            "Don't blame me, I voted for Izchak!",
            "Don't Panic",      /* HHGTTG */
            "Furinkan High School Athletic Dept.",      /* Ranma 1/2 */
            "Hel-LOOO, Nurse!", /* Animaniacs */
        };
        const char *buf;
        int erosion;

        if (Blind) {
            pline(msgc_cancelled, "You can't feel any Braille writing.");
            return 0;
        }
        break_conduct(conduct_illiterate);
        pline_implied(msgc_info, "It reads:");
        buf = shirt_msgs[scroll->o_id % SIZE(shirt_msgs)];
        erosion = greatest_erosion(scroll);
        if (erosion)
            buf = eroded_text(buf,
                              (int)(strlen(buf) * erosion / (2 * MAX_ERODE)),
                              scroll->o_id ^ (unsigned)u.ubirthday);
        pline(msgc_info, "\"%s\"", buf);
        return 1;
    } else if (scroll->oclass != SCROLL_CLASS &&
               scroll->oclass != SPBOOK_CLASS) {
        pline(msgc_cancelled, "That is a silly thing to read.");
        return 0;
    } else if (Blind) {
        const char *what = 0;

        if (scroll->oclass == SPBOOK_CLASS)
            what = "mystic runes";
        else if (!scroll->dknown)
            what = "formula on the scroll";
        if (what) {
            pline(msgc_cancelled, "Being blind, you cannot read the %s.", what);
            return 0;
        }
    }

    /* TODO: When we add a conduct assistance option, add a condition here.  Or
       better yet, check for all reading of things. */
    if (scroll->otyp == SPE_BOOK_OF_THE_DEAD &&
        !u.uconduct[conduct_illiterate] &&
        yn("You are currently illiterate and could invoke the Book "
           "instead. Read it nonetheless?") != 'y')
        return 0;

    if (scroll->otyp != SPE_BLANK_PAPER && scroll->otyp != SCR_BLANK_PAPER)
        break_conduct(conduct_illiterate);

    confused = (Confusion != 0);
    if (scroll->oclass == SPBOOK_CLASS)
        return study_book(scroll, arg);

    scroll->in_use = TRUE;      /* scroll, not spellbook, now being read */
    if (scroll->otyp != SCR_BLANK_PAPER) {
        if (Blind)
            pline(msgc_occstart,
                  "As you %s the formula on it, the scroll disappears.",
                  is_silent(youmonst.data) ? "cogitate" : "pronounce");
        else
            pline(msgc_occstart, "As you read the scroll, it disappears.");
        if (confused) {
            if (Hallucination)
                pline(msgc_substitute, "Being so trippy, you screw up...");
            else
                pline(msgc_substitute,
                      "Being confused, you mis%s the magic words...",
                      is_silent(youmonst.data) ? "understand" : "pronounce");
        }
    }
    if (!seffects(scroll, &known)) {
        if (!objects[scroll->otyp].oc_name_known) {
            if (known) {
                makeknown(scroll->otyp);
                more_experienced(0, 10);
            } else if (!objects[scroll->otyp].oc_uname)
                docall(scroll);
        }
        if (scroll->otyp != SCR_BLANK_PAPER)
            useup(scroll);
        else
            scroll->in_use = FALSE;
    }
    return 1;
}

static void
stripspe(struct obj *obj)
{
    if (obj->blessed)
        pline(msgc_noconsequence, "Nothing happens.");
    else {
        if (obj->spe > 0) {
            obj->spe = 0;
            if (obj->otyp == OIL_LAMP || obj->otyp == BRASS_LANTERN)
                obj->age = 0;
            pline(msgc_itemloss, "Your %s %s briefly.", xname(obj),
                  otense(obj, "vibrate"));
        } else
            pline(msgc_noconsequence, "Nothing happens.");
    }
}

static void
p_glow1(enum msg_channel msgc, struct obj *otmp)
{
    pline(msgc, "Your %s %s briefly.", xname(otmp),
          otense(otmp, Blind ? "vibrate" : "glow"));
}

static void
p_glow2(enum msg_channel msgc, struct obj *otmp, const char *color)
{
    pline(msgc, "Your %s %s%s%s for a moment.", xname(otmp),
          otense(otmp, Blind ? "vibrate" : "glow"), Blind ? "" : " ",
          Blind ? "" : hcolor(color));
}

/* Is the object chargeable?  For purposes of inventory display; it is
   possible to be able to charge things for which this returns FALSE. */
boolean
is_chargeable(struct obj *obj)
{
    if (obj->oclass == WAND_CLASS)
        return TRUE;
    /* known && !uname is possible after amnesia/mind flayer */
    if (obj->oclass == RING_CLASS)
        return (boolean) ((objects[obj->otyp].oc_charged &&
                           ((objects[obj->otyp].oc_name_known &&
                             obj->dknown) || obj->known)) ||
                          (obj->dknown &&
                           objects[obj->otyp].oc_uname &&
                           !objects[obj->otyp].oc_name_known));
    if (is_weptool(obj))        /* specific check before general tools */
        return FALSE;

    /* Magic lamps can't reasonably be recharged. But they must be shown in the
       list, to avoid telling them apart from brass lamps with a ?oC. */
    if (obj->oclass == TOOL_CLASS)
        return (boolean) (objects[obj->otyp].oc_charged ||
                          obj->otyp == BRASS_LANTERN || obj->otyp == OIL_LAMP ||
                          obj->otyp == MAGIC_LAMP);

    return FALSE;       /* why are weapons/armor considered charged anyway? */
}

/*
 * recharge an object; curse_bless is -1 if the recharging implement
 * was cursed, +1 if blessed, 0 otherwise.
 */
void
recharge(struct obj *obj, int curse_bless)
{
    int n;
    boolean is_cursed, is_blessed;

    is_cursed = curse_bless < 0;
    is_blessed = curse_bless > 0;

    /* Scrolls of charging now ID charge count, as well as doing the charging,
       unless cursed. */
    if (!is_cursed)
        obj->known = 1;

    if (obj->oclass == WAND_CLASS) {
        /* undo any prior cancellation, even when is_cursed */
        if (obj->spe == -1)
            obj->spe = 0;

        /*
         * Recharging might cause wands to explode.
         *
         *  v = number of previous recharges
         *        v = percentage chance to explode on this attempt
         *                v = cumulative odds for exploding
         *  0 :   0       0
         *  1 :   0.29    0.29
         *  2 :   2.33    2.62
         *  3 :   7.87   10.28
         *  4 :  18.66   27.02
         *  5 :  36.44   53.62
         *  6 :  62.97   82.83
         *  7 : 100     100
         *
         * No custom RNG, because we can't know whether the user will recharge
         * the same wand repeatedly, or recharge a number of different wands
         * separately, or a mixture, and this woud throw off the RNG.
         */
        n = (int)obj->recharged;
        if (n > 0 && (obj->otyp == WAN_WISHING ||
                      (n * n * n > rn2(7 * 7 * 7)))) {      /* recharge_limit */
            wand_explode(obj);
            return;
        }
        /* didn't explode, so increment the recharge count */
        obj->recharged = (unsigned)(n + 1);

        /* now handle the actual recharging */
        if (is_cursed) {
            stripspe(obj);
        } else {
            int lim = (obj->otyp == WAN_WISHING) ? 3 :
                (objects[obj->otyp].oc_dir != NODIR) ? 8 : 15;

            n = (lim == 3) ? 3 : rn1(5, lim + 1 - 5);
            if (!is_blessed)
                n = rnd(n);

            if (obj->spe < n)
                obj->spe = n;
            else
                obj->spe++;
            if (obj->otyp == WAN_WISHING && obj->spe > 3) {
                wand_explode(obj);
                return;
            }
            if (obj->spe >= lim)
                p_glow2(msgc_itemrepair, obj, "blue");
            else
                p_glow1(msgc_itemrepair, obj);
        }

    } else if (obj->oclass == RING_CLASS && objects[obj->otyp].oc_charged) {
        /* charging does not affect ring's curse/bless status */
        int s = is_blessed ? rnd(3) : is_cursed ? -rnd(2) : 1;
        boolean is_on = (obj == uleft || obj == uright);

        /* destruction depends on current state, not adjustment */
        if (obj->spe > rn2(7) || obj->spe <= -5) {
            pline(msgc_itemloss, "Your %s %s momentarily, then %s!",
                  xname(obj), otense(obj, "pulsate"), otense(obj, "explode"));
            if (is_on)
                setunequip(obj);
            s = rnd(3 * abs(obj->spe)); /* amount of damage */
            useup(obj);
            losehp(s, killer_msg(DIED, "an exploding ring"));
        } else {
            enum objslot slot = obj == uleft ? os_ringl : os_ringr;

            pline(s > 0 ? msgc_itemrepair : msgc_itemloss,
                  "Your %s spins %sclockwise for a moment.", xname(obj),
                  s < 0 ? "counter" : "");
            /* cause attributes and/or properties to be updated */
            if (is_on)
                setunequip(obj);
            obj->spe += s;      /* update the ring while it's off */
            if (is_on)
                setequip(slot, obj, em_silent);
            /* oartifact: if a touch-sensitive artifact ring is ever created
               the above will need to be revised */
        }

    } else if (obj->oclass == TOOL_CLASS) {
        int rechrg = (int)obj->recharged;

        /* tools don't have a limit, but the counter used does */
        if (rechrg < 7) /* recharge_limit */
            obj->recharged++;

        switch (obj->otyp) {
        case BELL_OF_OPENING:
            if (is_cursed)
                stripspe(obj);
            else if (is_blessed)
                obj->spe += rnd(3);
            else
                obj->spe += 1;
            if (obj->spe > 5)
                obj->spe = 5;
            break;
        case MAGIC_MARKER:
        case TINNING_KIT:
        case EXPENSIVE_CAMERA:
            if (is_cursed)
                stripspe(obj);
            else if (rechrg && obj->otyp == MAGIC_MARKER) {
                /* previously recharged */
                obj->recharged = 1;     /* override increment done above */
                if (obj->spe < 3)
                    pline(msgc_failcurse,
                          "Your marker seems permanently dried out.");
                else
                    pline(msgc_failcurse, "Nothing happens.");
            } else if (is_blessed) {
                n = rn1(16, 15);        /* 15..30 */
                if (obj->spe + n <= 50)
                    obj->spe = 50;
                else if (obj->spe + n <= 75)
                    obj->spe = 75;
                else {
                    int chrg = (int)obj->spe;

                    if ((chrg + n) > 127)
                        obj->spe = 127;
                    else
                        obj->spe += n;
                }
                p_glow2(msgc_itemrepair, obj, "blue");
            } else {
                n = rn1(11, 10);        /* 10..20 */
                if (obj->spe + n <= 50)
                    obj->spe = 50;
                else {
                    int chrg = (int)obj->spe;

                    if ((chrg + n) > 127)
                        obj->spe = 127;
                    else
                        obj->spe += n;
                }
                p_glow2(msgc_itemrepair, obj, "white");
            }
            break;
        case OIL_LAMP:
        case BRASS_LANTERN:
            if (is_cursed) {
                stripspe(obj);
                if (obj->lamplit) {
                    if (!Blind)
                        pline(msgc_consequence, "%s out!", Tobjnam(obj, "go"));
                    end_burn(obj, TRUE);
                }
            } else if (is_blessed) {
                obj->spe = 1;
                obj->age = 1500;
                p_glow2(msgc_itemrepair, obj, "blue");
            } else {
                obj->spe = 1;
                obj->age += 750;
                if (obj->age > 1500)
                    obj->age = 1500;
                p_glow1(msgc_itemrepair, obj);
            }
            break;
        case CRYSTAL_BALL:
            if (is_cursed)
                stripspe(obj);
            else if (is_blessed) {
                obj->spe = 6;
                p_glow2(msgc_itemrepair, obj, "blue");
            } else {
                if (obj->spe < 5) {
                    obj->spe++;
                    p_glow1(msgc_itemrepair, obj);
                } else
                    pline(msgc_failcurse, "Nothing happens.");
            }
            break;
        case HORN_OF_PLENTY:
        case BAG_OF_TRICKS:
        case CAN_OF_GREASE:
            if (is_cursed)
                stripspe(obj);
            else if (is_blessed) {
                if (obj->spe <= 10)
                    obj->spe += rn1(10, 6);
                else
                    obj->spe += rn1(5, 6);
                if (obj->spe > 50)
                    obj->spe = 50;
                p_glow2(msgc_itemrepair, obj, "blue");
            } else {
                obj->spe += rnd(5);
                if (obj->spe > 50)
                    obj->spe = 50;
                p_glow1(msgc_itemrepair, obj);
            }
            break;
        case MAGIC_FLUTE:
        case MAGIC_HARP:
        case FROST_HORN:
        case FIRE_HORN:
        case DRUM_OF_EARTHQUAKE:
            if (is_cursed) {
                stripspe(obj);
            } else if (is_blessed) {
                obj->spe += dice(2, 4);
                if (obj->spe > 20)
                    obj->spe = 20;
                p_glow2(msgc_itemrepair, obj, "blue");
            } else {
                obj->spe += rnd(4);
                if (obj->spe > 20)
                    obj->spe = 20;
                p_glow1(msgc_itemrepair, obj);
            }
            break;
        default:
            goto not_chargable;
        }       /* switch */

    } else {
    not_chargable:
        pline(msgc_badidea, "You have a feeling of loss.");
    }
}


/* Forget known information about this object class. */
static void
forget_single_object(int obj_id)
{
    const char *knownname;
    char *new_uname;

    if (!objects[obj_id].oc_name_known)
        return; /* nothing to do */
    knownname = simple_typename(obj_id);
    objects[obj_id].oc_name_known = 0;
    if (objects[obj_id].oc_uname) {
        free(objects[obj_id].oc_uname);
        objects[obj_id].oc_uname = 0;
    }
    undiscover_object(obj_id);  /* after clearing oc_name_known */

    /* Record what the object was before we forgot it. Doing anything else is
       an interface screw. (In other words, we're formally-unIDing objects, but
       not forcing the player to write down what they were. */
    new_uname = strcpy(malloc((unsigned)strlen(knownname) + 1), knownname);
    objects[obj_id].oc_uname = new_uname;
    discover_object(obj_id, FALSE, TRUE, FALSE);
}


/* randomize the given list of numbers  0 <= i < count */
static void
randomize(int *indices, int count)
{
    int i, iswap, temp;

    for (i = count - 1; i > 0; i--) {
        if ((iswap = rn2(i + 1)) == i)
            continue;
        temp = indices[i];
        indices[i] = indices[iswap];
        indices[iswap] = temp;
    }
}


/* Forget % of known objects. */
void
forget_objects(int percent)
{
    int i, count;
    int indices[NUM_OBJECTS];

    if (percent == 0)
        return;
    if (percent <= 0 || percent > 100) {
        impossible("forget_objects: bad percent %d", percent);
        return;
    }

    for (count = 0, i = 1; i < NUM_OBJECTS; i++)
        if (OBJ_DESCR(objects[i]) && (objects[i].oc_name_known))
            indices[count++] = i;

    randomize(indices, count);

    /* forget first % of randomized indices */
    count = ((count * percent) + 50) / 100;
    for (i = 0; i < count; i++)
        forget_single_object(indices[i]);
}

/*
 * Forget some things (e.g. after reading a scroll of amnesia).  When called,
 * the following are always forgotten:
 *
 *      - felt ball & chain
 *      - skill training
 *
 * Other things are subject to flags:
 *
 *      howmuch & ALL_SPELLS    = forget all spells
 *      howmuch & ALL_MAP       = forget all objects (no more map amnesia)
 */
static void
forget(int howmuch)
{

    if (Punished)
        u.bc_felt = 0;  /* forget felt ball&chain */

    /* People complain that this is silly and it doesn't work anyway, so it's
       disabled for now */
    /* forget_objects(howmuch & ALL_MAP ? 100 : rn2(25)+25); */

    if (howmuch & ALL_SPELLS)
        losespells();

    /* Forget some skills. */
    drain_weapon_skill(rnd(howmuch ? 5 : 3));

    /*
     * Make sure that what was seen is restored correctly.  To do this,
     * we need to go blind for an instant --- turn off the display,
     * then restart it.  All this work is needed to correctly handle
     * walls which are stone on one side and wall on the other.  Turning
     * off the seen bits above will make the wall revert to stone,  but
     * there are cases where we don't want this to happen.  The easiest
     * thing to do is to run it through the vision system again, which
     * is always correct.
     */
    doredraw(); /* this correctly will reset vision */
}

/* monster is hit by scroll of taming's effect */
static void
maybe_tame(struct monst *mtmp, struct obj *sobj)
{
    if (sobj->cursed) {
        setmangry(mtmp);
    } else {
        if (mtmp->isshk)
            make_happy_shk(mtmp, FALSE);
        else if (!resist(mtmp, sobj->oclass, 0, NOTELL))
            tamedog(mtmp, NULL);
    }
}

int
seffects(struct obj *sobj, boolean *known)
{
    int cval;
    boolean confused = (Confusion != 0);
    struct obj *otmp;

    if (objects[sobj->otyp].oc_magic)
        exercise(A_WIS, TRUE);  /* just for trying */
    switch (sobj->otyp) {
    case SCR_ENCHANT_ARMOR:
        {
            schar s;
            boolean special_armor;
            boolean same_color;

            otmp = some_armor(&youmonst);
            if (!otmp) {
                strange_feeling(sobj,
                                !Blind ? "Your skin glows then fades." :
                                "Your skin feels warm for a moment.");
                exercise(A_CON, !sobj->cursed);
                exercise(A_STR, !sobj->cursed);
                return 1;
            }
            if (confused) {
                otmp->oerodeproof = !(sobj->cursed);
                if (Blind) {
                    otmp->rknown = FALSE;
                    pline(msgc_nospoil, "Your %s %s warm for a moment.",
                          xname(otmp), otense(otmp, "feel"));
                } else {
                    otmp->rknown = TRUE;
                    pline(sobj->cursed ? msgc_itemloss : msgc_itemrepair,
                          "Your %s %s covered by a %s %s %s!", xname(otmp),
                          otense(otmp, "are"),
                          sobj->cursed ? "mottled" : "shimmering",
                          hcolor(sobj->cursed ? "black" : "golden"),
                          sobj->cursed ? "glow" :
                          (is_shield(otmp) ? "layer" : "shield"));
                }
                if (otmp->oerodeproof && (otmp->oeroded || otmp->oeroded2)) {
                    otmp->oeroded = otmp->oeroded2 = 0;
                    pline_implied(msgc_itemrepair,
                                  "Your %s %s as good as new!", xname(otmp),
                                  otense(otmp, Blind ? "feel" : "look"));
                }
                break;
            }
            /* elven armor vibrates warningly when enchanted beyond a limit */
            special_armor = is_elven_armor(otmp) || (Role_if(PM_WIZARD) &&
                                                     otmp->otyp == CORNUTHAUM);
            if (sobj->cursed)
                same_color = (otmp->otyp == BLACK_DRAGON_SCALE_MAIL ||
                              otmp->otyp == BLACK_DRAGON_SCALES);
            else
                same_color = (otmp->otyp == SILVER_DRAGON_SCALE_MAIL ||
                              otmp->otyp == SILVER_DRAGON_SCALES ||
                              otmp->otyp == SHIELD_OF_REFLECTION);
            if (Blind)
                same_color = FALSE;

            /* a custom RNG for this would only give marginal benefits:
               intentional overenchantment attemps are rare */

            /* KMH -- catch underflow */
            s = sobj->cursed ? -otmp->spe : otmp->spe;
            if (s > (special_armor ? 5 : 3) && rn2(s)) {
                pline(msgc_itemloss,
                      "Your %s violently %s%s%s for a while, then %s.",
                      xname(otmp), otense(otmp, Blind ? "vibrate" : "glow"),
                      (!Blind && !same_color) ? " " : "",
                      (Blind || same_color) ? "" :
                      hcolor(sobj->cursed ? "black" : "silver"),
                      otense(otmp, "evaporate"));
                setunequip(otmp);
                useup(otmp);
                break;
            }

            /* OTOH, a custom RNG for /this/ is useful. The main balance
               implication here is whether armour goes to 5 or stops at 4. As
               such, if we're enchanting from 2 or 3 precisely, and the
               resulting spe is 4 or 5 precisely, we re-randomize which.
               Exception: an uncursed enchant from 3 always goes to 4 (we never
               bump it up to 5). */
            s = sobj->cursed ? -1 :
                otmp->spe >= 9 ? (rn2(otmp->spe) == 0) :
                sobj->blessed ? rnd(3 - otmp->spe / 3) : 1;
            if ((otmp->spe == 2 || otmp->spe == 3) && sobj->blessed &&
                (otmp->spe + s == 4 || otmp->spe + s == 5))
                s = 4 + rn2_on_rng(2, rng_armor_ench_4_5) - otmp->spe;
            if (s < 0 && otmp->unpaid)
                costly_damage_obj(otmp);
            if (s >= 0 && otmp->otyp >= GRAY_DRAGON_SCALES &&
                otmp->otyp <= YELLOW_DRAGON_SCALES) {
                /* dragon scales get turned into dragon scale mail */
                pline(msgc_itemrepair, "Your %s merges and hardens!",
                      xname(otmp));
                setworn(NULL, W_MASK(os_arm));
                /* assumes same order */
                otmp->otyp =
                    GRAY_DRAGON_SCALE_MAIL + otmp->otyp - GRAY_DRAGON_SCALES;
                otmp->cursed = 0;
                if (sobj->blessed) {
                    otmp->spe++;
                    otmp->blessed = 1;
                }
                otmp->known = 1;
                setworn(otmp, W_MASK(os_arm));
                *known = TRUE;
                if (otmp->unpaid && s > 0)
                    adjust_bill_val(otmp);
                break;
            }
            pline((otmp->known && s == 0) ? msgc_failrandom :
                  (!otmp->known && Hallucination) ? msgc_nospoil :
                  sobj->cursed ? msgc_itemloss : msgc_itemrepair,
                  "Your %s %s%s%s%s for a %s.", xname(otmp),
                  s == 0 ? "violently " : "",
                  otense(otmp, Blind ? "vibrate" : "glow"),
                  (!Blind && !same_color) ? " " : "",
                  (Blind || same_color) ? "" :
                  hcolor(sobj->cursed ? "black" : "silver"),
                  (s * s > 1) ? "while" : "moment");
            otmp->cursed = sobj->cursed;
            if (!otmp->blessed || sobj->cursed)
                otmp->blessed = sobj->blessed;
            if (s) {
                otmp->spe += s;
                adj_abon(otmp, s);
                *known = otmp->known;
            }

            if ((otmp->spe > (special_armor ? 5 : 3)) &&
                (special_armor || !rn2(7)))
                pline_implied(msgc_hint, "Your %s suddenly %s %s.", xname(otmp),
                              otense(otmp, "vibrate"),
                              Blind ? "again" : "unexpectedly");
            if (otmp->unpaid && s > 0)
                adjust_bill_val(otmp);
            break;
        }
    case SCR_DESTROY_ARMOR:
        {
            otmp = some_armor(&youmonst);
            if (confused) {
                if (!otmp) {
                    strange_feeling(sobj, "Your bones itch.");
                    exercise(A_STR, FALSE);
                    exercise(A_CON, FALSE);
                    return 1;
                }
                otmp->oerodeproof = sobj->cursed;
                p_glow2(!sobj->bknown ? msgc_nospoil :
                        otmp->oerodeproof ? msgc_itemrepair : msgc_itemloss,
                        otmp, "purple");
                break;
            }
            if (!sobj->cursed || !otmp || !otmp->cursed) {
                if (!destroy_arm(otmp)) {
                    strange_feeling(sobj, "Your skin itches.");
                    exercise(A_STR, FALSE);
                    exercise(A_CON, FALSE);
                    return 1;
                } else
                    *known = TRUE;
            } else {    /* armor and scroll both cursed */
                pline(msgc_itemloss, "Your %s %s.", xname(otmp),
                      otense(otmp, "vibrate"));
                if (otmp->spe >= -6) {
                    otmp->spe--;
                    if (otmp->otyp == HELM_OF_BRILLIANCE) {
                        ABON(A_INT)--;
                        ABON(A_WIS)--;
                        makeknown(otmp->otyp);
                    }
                    if (otmp->otyp == GAUNTLETS_OF_DEXTERITY) {
                        ABON(A_DEX)--;
                        makeknown(otmp->otyp);
                    }
                }
                make_stunned(HStun + rn1(10, 10), TRUE);
            }
        }
        break;
    case SCR_CONFUSE_MONSTER:
    case SPE_CONFUSE_MONSTER:
        if (youmonst.data->mlet != S_HUMAN || sobj->cursed) {
            if (!HConfusion)
                pline(msgc_statusbad, "You feel confused.");
            make_confused(HConfusion + rnd(100), FALSE);
        } else if (confused) {
            if (!sobj->blessed) {
                pline(msgc_statusbad, "Your %s begin to %s%s.",
                      makeplural(body_part(HAND)), Blind ? "tingle" : "glow ",
                      Blind ? "" : hcolor("purple"));
                make_confused(HConfusion + rnd(100), FALSE);
            } else {
                pline(msgc_statusheal, "A %s%s surrounds your %s.",
                      Blind ? "" : hcolor("red"),
                      Blind ? "faint buzz" : " glow", body_part(HEAD));
                make_confused(0L, TRUE);
            }
        } else {
            if (!sobj->blessed) {
                pline(msgc_statusgood, "Your %s%s %s%s.",
                      makeplural(body_part(HAND)),
                      Blind ? "" : " begin to glow",
                      Blind ? (const char *)"tingle" : hcolor("red"),
                      u.umconf ? " even more" : "");
                u.umconf++;
            } else {
                if (Blind)
                    pline(msgc_statusgood, "Your %s tingle %s sharply.",
                          makeplural(body_part(HAND)),
                          u.umconf ? "even more" : "very");
                else
                    pline(msgc_statusgood, "Your %s glow a%s brilliant %s.",
                          makeplural(body_part(HAND)),
                          u.umconf ? "n even more" : "", hcolor("red"));
                /* after a while, repeated uses become less effective */
                if (u.umconf >= 40)
                    u.umconf++;
                else
                    u.umconf += rn1(8, 2);
            }
        }
        break;
    case SCR_SCARE_MONSTER:
    case SPE_CAUSE_FEAR:
        {
            int ct = 0;
            struct monst *mtmp;

            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                if (cansee(mtmp->mx, mtmp->my)) {
                    if (confused || sobj->cursed) {
                        mtmp->mflee = mtmp->mfrozen = mtmp->msleeping = 0;
                        mtmp->mcanmove = 1;
                    } else if (!resist(mtmp, sobj->oclass, 0, NOTELL))
                        monflee(mtmp, 0, FALSE, FALSE);
                    if (!mtmp->mtame)
                        ct++;   /* pets don't laugh at you */
                }
            }
            if (!ct)
                You_hear(msgc_levelsound, "%s in the distance.",
                         (confused || sobj->cursed) ?
                         "sad wailing" : "maniacal laughter");
            else if (sobj->otyp == SCR_SCARE_MONSTER)
                You_hear(msgc_levelsound, "%s close by.",
                         (confused || sobj->cursed) ?
                         "sad wailing" : "maniacal laughter");
            break;
        }
    case SCR_BLANK_PAPER:
        if (Blind)
            pline(msgc_yafm, "You don't remember there being any "
                  "magic words on this scroll.");
        else
            pline(msgc_yafm, "This scroll seems to be blank.");
        *known = TRUE;
        break;
    case SCR_REMOVE_CURSE:
    case SPE_REMOVE_CURSE:
        {
            struct obj *obj;

            if (confused)
                if (Hallucination)
                    pline(msgc_itemloss,
                          "You feel the power of the Force against you!");
                else
                    pline(msgc_itemloss, "You feel like you need some help.");
            else if (Hallucination)
                pline(msgc_itemrepair,
                      "You feel in touch with the Universal Oneness.");
            else
                pline(msgc_itemrepair, "You feel like someone is helping you.");

            if (sobj->cursed) {
                pline(msgc_failcurse, "The scroll disintegrates.");
            } else {
                for (obj = invent; obj; obj = obj->nobj) {
                    long wornmask;

                    /* gold isn't subject to cursing and blessing */
                    if (obj->oclass == COIN_CLASS)
                        continue;

                    wornmask = obj->owornmask & W_MASKABLE;
                    if (wornmask && !sobj->blessed) {
                        /* handle a couple of special cases; we don't allow
                           auxiliary weapon slots to be used to artificially
                           increase number of worn items */
                        if (obj == uswapwep) {
                            if (!u.twoweap)
                                wornmask = 0L;
                        } else if (obj == uquiver) {
                            if (obj->oclass == WEAPON_CLASS) {
                                /* mergeable weapon test covers ammo, missiles,
                                   spears, daggers & knives */
                                if (!objects[obj->otyp].oc_merge)
                                    wornmask = 0L;
                            } else if (obj->oclass == GEM_CLASS) {
                                /* possibly ought to check whether alternate
                                   weapon is a sling... */
                                if (!uslinging())
                                    wornmask = 0L;
                            } else {
                                /* weptools don't merge and aren't reasonable
                                   quivered weapons */
                                wornmask = 0L;
                            }
                        }
                    }
                    if (sobj->blessed || wornmask || obj->otyp == LOADSTONE ||
                        (obj->otyp == LEASH && obj->leashmon)) {
                        if (confused)
                            blessorcurse(obj, 2, rng_main);
                        else
                            uncurse(obj);
                    }
                }
            }
            if (Punished && !confused)
                unpunish();
            update_inventory();
            break;
        }
    case SCR_CREATE_MONSTER:
    case SPE_CREATE_MONSTER:
        if (create_critters
            (1 + ((confused || sobj->cursed) ? 12 : 0) +
             ((sobj->blessed ||
               rn2(73)) ? 0 : rnd(4)), confused ? &mons[PM_ACID_BLOB] : NULL))
            *known = TRUE;
        /* no need to flush monsters; we ask for identification only if the
           monsters are not visible */
        break;
    case SCR_ENCHANT_WEAPON:
        if (uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep))
            && confused) {
            /* oclass check added 10/25/86 GAN */
            uwep->oerodeproof = !(sobj->cursed);
            if (Blind) {
                uwep->rknown = FALSE;
                pline(msgc_nospoil, "Your weapon feels warm for a moment.");
            } else {
                uwep->rknown = TRUE;
                pline(sobj->cursed ? msgc_itemloss : msgc_itemrepair,
                      "Your %s covered by a %s %s %s!", aobjnam(uwep, "are"),
                      sobj->cursed ? "mottled" : "shimmering",
                      hcolor(sobj->cursed ? "purple" : "golden"),
                      sobj->cursed ? "glow" : "shield");
            }
            if (uwep->oerodeproof && (uwep->oeroded || uwep->oeroded2)) {
                uwep->oeroded = uwep->oeroded2 = 0;
                pline_implied(msgc_itemrepair, "Your %s as good as new!",
                              aobjnam(uwep, Blind ? "feel" : "look"));
            }
        } else {
            /* don't bother with a custom RNG here, 6/7 on weapons is much less
               balance-affecting than 4/5 on armour */
            return !chwepon(sobj,
                            sobj->cursed ? -1 : !uwep ? 1 :
                            uwep->spe >= 9 ? (rn2(uwep->spe) == 0) :
                            sobj->blessed ? rnd(3 - uwep->spe / 3) : 1);
        }
        break;
    case SCR_TAMING:
    case SPE_CHARM_MONSTER:
        if (Engulfed) {
            maybe_tame(u.ustuck, sobj);
        } else {
            int i, j, bd = confused ? 5 : 1;
            struct monst *mtmp;

            for (i = -bd; i <= bd; i++)
                for (j = -bd; j <= bd; j++) {
                    if (!isok(u.ux + i, u.uy + j))
                        continue;
                    if ((mtmp = m_at(level, u.ux + i, u.uy + j)) != 0)
                        maybe_tame(mtmp, sobj);
                }
        }
        break;
    case SCR_GENOCIDE:
        pline(msgc_intrgain, "You have found a scroll of genocide!");
        *known = TRUE;
        if (sobj->blessed)
            do_class_genocide();
        else
            do_genocide((!sobj->cursed) | (2 * ! !Confusion));
        break;
    case SCR_LIGHT:
        if (!Blind)
            *known = TRUE;
        litroom(!confused && !sobj->cursed, sobj);
        break;
    case SCR_TELEPORTATION:
        if (confused || sobj->cursed)
            level_tele();
        else {
            if (sobj->blessed && !Teleport_control) {
                *known = TRUE;
                if (yn("Do you wish to teleport?") == 'n')
                    break;
            }
            tele();
            if (Teleport_control || !couldsee(u.ux0, u.uy0) ||
                (distu(u.ux0, u.uy0) >= 16))
                *known = TRUE;
        }
        break;
    case SCR_GOLD_DETECTION:
        if (confused || sobj->cursed)
            return trap_detect(sobj);
        else
            return gold_detect(sobj, known);
    case SCR_FOOD_DETECTION:
    case SPE_DETECT_FOOD:
        if (food_detect(sobj, known))
            return 1;   /* nothing detected */
        break;
    case SPE_IDENTIFY:
        cval = rn2_on_rng(25, rng_id_count) / 5;
        goto id;
    case SCR_IDENTIFY:
        /* known = TRUE; */

        cval = rn2_on_rng(25, rng_id_count);
        if (sobj->cursed || (!sobj->blessed && cval % 5))
            cval = 1;  /* cursed 100%, uncursed 80% chance of 1 */
        else if (sobj->blessed && cval / 5 == 1 && Luck > 0)
            cval = 2;  /* with positive luck, interpret 1 as 2 when blessed */
        else
            cval /= 5; /* otherwise, randomize all/1/2/3/4 items IDed */

        if (confused)
            pline(msgc_yafm, "You identify this as an identify scroll.");
        else
            pline_implied((cval == 5 && invent) ?
                          msgc_youdiscover : msgc_uiprompt,
                          "This is an identify scroll.");

        if (!objects[sobj->otyp].oc_name_known)
            more_experienced(0, 10);
        useup(sobj);
        makeknown(SCR_IDENTIFY);
    id:
        if (invent && !confused) {
            identify_pack(cval);
        }
        return 1;

    case SCR_CHARGING:
        if (confused) {
            pline(msgc_statusheal, "You feel charged up!");
            if (u.uen < u.uenmax)
                u.uen = u.uenmax;
            else
                u.uen = (u.uenmax += dice(5, 4));
            break;
        }
        pline(msgc_uiprompt, "This is a charging scroll.");

        cval = sobj->cursed ? -1 : (sobj->blessed ? 1 : 0);
        if (!objects[sobj->otyp].oc_name_known)
            more_experienced(0, 10);
        useup(sobj);
        makeknown(SCR_CHARGING);

        otmp = getobj(all_count, "charge", FALSE);
        if (!otmp)
            return 1;
        recharge(otmp, cval);
        return 1;

    case SCR_MAGIC_MAPPING:
        if (level->flags.nommap) {
            pline(msgc_statusbad, "Your mind is filled with crazy lines!");
            if (Hallucination)
                pline_implied(msgc_statusbad, "Wow!  Modern art.");
            else
                pline_implied(msgc_statusbad, "Your %s spins in bewilderment.",
                              body_part(HEAD));
            make_confused(HConfusion + rnd(30), FALSE);
            break;
        }
        if (sobj->blessed) {
            int x, y;

            for (x = 0; x < COLNO; x++)
                for (y = 0; y < ROWNO; y++)
                    if (level->locations[x][y].typ == SDOOR)
                        cvt_sdoor_to_door(&level->locations[x][y], &u.uz);
            /* do_mapping() already reveals secret passages */
        }
        *known = TRUE;
    case SPE_MAGIC_MAPPING:
        if (level->flags.nommap) {
            pline(msgc_statusbad,
                  "Your %s spins as something blocks the spell!",
                  body_part(HEAD));
            make_confused(HConfusion + rnd(30), FALSE);
            break;
        }
        pline(msgc_youdiscover, "A map coalesces in your mind!");
        cval = (sobj->cursed && !confused);
        if (cval)
            HConfusion = 1;     /* to screw up map */
        do_mapping();
        if (cval) {
            HConfusion = 0;     /* restore */
            pline(msgc_substitute,
                  "Unfortunately, you can't grasp the details.");
        }
        break;
    case SCR_AMNESIA:
        *known = TRUE;
        forget((!sobj->blessed ? ALL_SPELLS : 0) |
               (!confused || sobj->cursed ? ALL_MAP : 0));
        if (Hallucination)      /* Ommmmmm! */
            pline(msgc_intrloss,
                  "Your mind releases itself from mundane concerns.");
        else if (!strncmpi(u.uplname, "Maud", 4))
            pline(msgc_intrloss, "As your mind turns inward on itself, you "
                  "forget everything else.");
        else if (rn2(2))
            pline(msgc_intrloss, "Who was that Maud person anyway?");
        else
            pline(msgc_intrloss,
                  "Thinking of Maud you forget everything else.");
        exercise(A_WIS, FALSE);
        break;
    case SCR_FIRE:
        /* Note: Modifications have been made as of 3.0 to allow for
           some damage under all potential cases. */
        cval = bcsign(sobj);
        if (!objects[sobj->otyp].oc_name_known)
            more_experienced(0, 10);
        useup(sobj);
        makeknown(SCR_FIRE);
        if (confused) {
            if (Fire_resistance) {
                shieldeff(u.ux, u.uy);
                if (!Blind)
                    pline(msgc_playerimmune,
                          "Oh, look, what a pretty fire in your %s.",
                          makeplural(body_part(HAND)));
                else
                    pline(msgc_playerimmune,
                          "You feel a pleasant warmth in your %s.",
                          makeplural(body_part(HAND)));
            } else {
                pline(msgc_substitute,
                      "The scroll catches fire and you burn your %s.",
                      makeplural(body_part(HAND)));
                losehp(1, killer_msg(DIED, "a scroll of fire"));
            }
            return 1;
        }
        if (Underwater)
            pline(msgc_actionok, "The water around you vaporizes violently!");
        else {
            pline(msgc_actionok, "The scroll erupts in a tower of flame!");
            burn_away_slime();
        }
        explode(u.ux, u.uy, 11, (2 * (rn1(3, 3) + 2 * cval) + 1) / 3,
                SCROLL_CLASS, EXPL_FIERY, NULL);
        return 1;
    case SCR_EARTH:
        /* TODO: handle steeds */
        if (!Is_rogue_level(&u.uz) &&
            (!In_endgame(&u.uz) || Is_earthlevel(&u.uz))) {
            int x, y;

            /* Identify the scroll */
            pline(msgc_actionok, "The %s rumbles %s you!",
                  ceiling(u.ux, u.uy), sobj->blessed ? "around" : "above");
            *known = TRUE;
            if (In_sokoban(&u.uz))
                change_luck(-1);        /* Sokoban guilt */

            /* Loop through the surrounding squares */
            if (!sobj->cursed)
                for (x = u.ux - 1; x <= u.ux + 1; x++) {
                    for (y = u.uy - 1; y <= u.uy + 1; y++) {
                        /* Is this a suitable spot? */
                        if (isok(x, y) && !closed_door(level, x, y) &&
                            !IS_ROCK(level->locations[x][y].typ) &&
                            !IS_AIR(level->locations[x][y].typ) &&
                            (x != u.ux || y != u.uy)) {
                            struct obj *otmp2;
                            struct monst *mtmp;

                            /* Make the object(s) */
                            otmp2 = mksobj(level, confused ? ROCK : BOULDER,
                                           FALSE, FALSE, rng_main);
                            if (!otmp2)
                                continue;       /* Shouldn't happen */
                            otmp2->quan = confused ? rn1(5, 2) : 1;
                            otmp2->owt = weight(otmp2);

                            /* Find the monster here (won't be player) */
                            mtmp = m_at(level, x, y);
                            if (mtmp && !amorphous(mtmp->data) &&
                                !passes_walls(mtmp->data) &&
                                !noncorporeal(mtmp->data) &&
                                !unsolid(mtmp->data)) {
                                struct obj *helmet = which_armor(mtmp, os_armh);
                                int mdmg;

                                if (cansee(mtmp->mx, mtmp->my)) {
                                    if (!helmet || !is_metallic(helmet))
                                        pline(combat_msgc(&youmonst, mtmp,
                                                          cr_hit),
                                              "%s is hit by %s!",
                                              Monnam(mtmp), doname(otmp2));
                                    if (!canspotmon(mtmp))
                                        map_invisible(mtmp->mx, mtmp->my);
                                }
                                mdmg = dmgval(otmp2, mtmp) * otmp2->quan;
                                if (helmet) {
                                    if (is_metallic(helmet)) {
                                        if (canseemon(mtmp))
                                            pline(combat_msgc(&youmonst, mtmp,
                                                              cr_resist),
                                                  "A %s bounces off "
                                                  "%s hard %s.",
                                                  doname(otmp2),
                                                  s_suffix(mon_nam(mtmp)),
                                                  helmet_name(helmet));
                                        else
                                            You_hear(msgc_levelsound,
                                                     "a clanging sound.");
                                        if (mdmg > 2)
                                            mdmg = 2;
                                    } else {
                                        if (canseemon(mtmp))
                                            pline_implied(
                                                combat_msgc(&youmonst,
                                                            mtmp, cr_hit),
                                                "%s's %s does not protect %s.",
                                                Monnam(mtmp), xname(helmet),
                                                mhim(mtmp));
                                    }
                                }
                                mtmp->mhp -= mdmg;
                                if (mtmp->mhp <= 0)
                                    xkilled(mtmp, 1);
                            }
                            /* Drop the rock/boulder to the floor */
                            if (!flooreffects(otmp2, x, y, "fall")) {
                                place_object(otmp2, level, x, y);
                                stackobj(otmp2);
                                newsym(x, y);   /* map the rock */
                            }
                        }
                    }
                }
            /* Attack the player */
            if (!sobj->blessed) {
                int dmg;
                struct obj *otmp2;

                /* Okay, _you_ write this without repeating the code */
                otmp2 = mksobj(level, confused ? ROCK : BOULDER,
                               FALSE, FALSE, rng_main);
                if (!otmp2)
                    break;
                otmp2->quan = confused ? rn1(5, 2) : 1;
                otmp2->owt = weight(otmp2);
                if (!amorphous(youmonst.data) && !Passes_walls &&
                    !noncorporeal(youmonst.data) && !unsolid(youmonst.data)) {
                    pline(msgc_badidea, "You are hit by %s!", doname(otmp2));
                    dmg = dmgval(otmp2, &youmonst) * otmp2->quan;
                    if (uarmh && !sobj->cursed) {
                        if (is_metallic(uarmh)) {
                            pline(msgc_playerimmune,
                                  "Fortunately, you are wearing a hard %s.",
                                  helmet_name(uarmh));
                            if (dmg > 2)
                                dmg = 2;
                        } else if (flags.verbose) {
                            pline(msgc_notresisted,
                                  "Your %s does not protect you.",
                                  xname(uarmh));
                        }
                    }
                } else
                    dmg = 0;
                /* Must be before the losehp(), for bones files */
                if (!flooreffects(otmp2, u.ux, u.uy, "fall")) {
                    place_object(otmp2, level, u.ux, u.uy);
                    stackobj(otmp2);
                    newsym(u.ux, u.uy);
                }
                if (dmg)
                    losehp(dmg, killer_msg(DIED, "a scroll of earth"));
            }
        }
        break;
    case SCR_PUNISHMENT:
        *known = TRUE;
        if (confused || sobj->blessed) {
            pline(msgc_failcurse, "You feel guilty.");
            break;
        }
        punish(sobj);
        break;
    case SCR_STINKING_CLOUD:{
            coord cc;

            pline(msgc_hint, "You have found a scroll of stinking cloud!");
            *known = TRUE;
            pline(msgc_uiprompt, "Where do you want to center the cloud?");
            cc.x = u.ux;
            cc.y = u.uy;
            if (getpos(&cc, TRUE, "the desired position", FALSE) ==
                NHCR_CLIENT_CANCEL) {
                pline(msgc_cancelled, "Never mind.");
                return 0;
            }
            if (!cansee(cc.x, cc.y) || distu(cc.x, cc.y) >= 32) {
                pline(msgc_yafm, "You smell rotten eggs.");
                return 0;
            }
            create_gas_cloud(level, cc.x, cc.y, 3 + bcsign(sobj),
                             8 + 4 * bcsign(sobj));
            break;
        }
    default:
        impossible("What weird effect is this? (%u)", sobj->otyp);
    }
    return 0;
}

static void
wand_explode(struct obj *obj)
{
    obj->in_use = TRUE; /* in case losehp() is fatal */
    pline(msgc_consequence, "Your %s vibrates violently, and explodes!",
          xname(obj));
    losehp(rnd(2 * (u.uhpmax + 1) / 3), killer_msg(DIED, "an exploding wand"));
    useup(obj);
    exercise(A_STR, FALSE);
}

/*
 * Low-level lit-field update routine.
 */
static void
set_lit(int x, int y, void *val)
{
    if (val)
        level->locations[x][y].lit = 1;
    else {
        level->locations[x][y].lit = 0;
        snuff_light_source(x, y);
    }
}

void
litroom(boolean on, struct obj *obj)
{
    char is_lit;        /* value is irrelevant; we use its address as a `not
                           null' flag for set_lit() */

    /* first produce the text (provided you're not blind) */
    if (!on) {
        struct obj *otmp;

        if (!Blind) {
            if (Engulfed) {
                pline(msgc_yafm, "It seems even darker in here than before.");
                return;
            }
            if (uwep && artifact_light(uwep) && uwep->lamplit)
                pline(msgc_substitute,
                      "Suddenly, the only light left comes from %s!",
                      the(xname(uwep)));
            else
                pline(msgc_substitute, "You are surrounded by darkness!");
        }

        /* the magic douses lamps, et al, too */
        for (otmp = invent; otmp; otmp = otmp->nobj)
            if (otmp->lamplit)
                snuff_lit(otmp);
        if (Blind)
            goto do_it;
    } else {
        if (Blind)
            goto do_it;
        if (Engulfed) {
            if (is_animal(u.ustuck->data))
                pline(msgc_yafm, "%s %s is lit.", s_suffix(Monnam(u.ustuck)),
                      mbodypart(u.ustuck, STOMACH));
            else if (is_whirly(u.ustuck->data))
                pline(msgc_actionok, "%s shines briefly.", Monnam(u.ustuck));
            else
                pline(msgc_actionok, "%s glistens.", Monnam(u.ustuck));
            return;
        }
        pline(msgc_actionok, "A lit field surrounds you!");
    }

do_it:
    /* No-op in water - can only see the adjacent squares and that's it! */
    if (Underwater || Is_waterlevel(&u.uz))
        return;
    /*
     *  If we are darkening the room and the hero is punished but not
     *  blind, then we have to pick up and replace the ball and chain so
     *  that we don't remember them if they are out of sight.
     */
    if (Punished && !on && !Blind)
        move_bc(1, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);

    if (Is_rogue_level(&u.uz)) {
        /* Can't use do_clear_area because MAX_RADIUS is too small */
        /* rogue lighting must light the entire room */
        int rnum = level->locations[u.ux][u.uy].roomno - ROOMOFFSET;
        int rx, ry;

        if (rnum >= 0) {
            for (rx = level->rooms[rnum].lx - 1;
                 rx <= level->rooms[rnum].hx + 1; rx++)
                for (ry = level->rooms[rnum].ly - 1;
                     ry <= level->rooms[rnum].hy + 1; ry++)
                    set_lit(rx, ry, (on ? &is_lit : NULL));
            level->rooms[rnum].rlit = on;
        }
        /* hallways remain dark on the rogue level */
    } else
        do_clear_area(u.ux, u.uy,
                      (obj && obj->oclass == SCROLL_CLASS &&
                       obj->blessed) ? 9 : 5, set_lit, (on ? &is_lit : NULL));

    /*
     *  If we are not blind, then force a redraw on all positions in sight
     *  by temporarily blinding the hero.  The vision recalculation will
     *  correctly update all previously seen positions *and* correctly
     *  set the waslit bit [could be messed up from above].
     */
    if (!Blind) {
        vision_recalc(2);

        /* replace ball&chain */
        if (Punished && !on)
            move_bc(0, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);
    }

    turnstate.vision_full_recalc = TRUE; /* delayed vision recalculation */
}


static void
do_class_genocide(void)
{
    int i, j, immunecnt, gonecnt, goodcnt, class, feel_dead = 0;
    const char *buf;
    const char mimic_buf[] = {def_monsyms[S_MIMIC], '\0'};
    boolean gameover = FALSE;   /* true iff killed self */

    for (j = 0;; j++) {
        if (j >= 5) {
            pline(msgc_cancelled, "That's enough tries!");
            return;
        }

        buf = getlin("What class of monsters do you wish to genocide?",
                     FALSE); /* not meaningfully repeatable... */
        buf = msgmungspaces(buf);

        /* choosing "none" preserves genocideless conduct */
        if (!strcmpi(buf, "none") || !strcmpi(buf, "nothing"))
            return;

        if (strlen(buf) == 1) {
            if (buf[0] == ILLOBJ_SYM)
                buf = mimic_buf;
            class = def_char_to_monclass(buf[0]);
        } else {
            class = 0;
            buf = makesingular(buf);
        }

        immunecnt = gonecnt = goodcnt = 0;
        for (i = LOW_PM; i < NUMMONS; i++) {
            if (class == 0 && strstri(monexplain[(int)mons[i].mlet], buf) != 0)
                class = mons[i].mlet;
            if (mons[i].mlet == class) {
                if (!(mons[i].geno & G_GENO))
                    immunecnt++;
                else if (mvitals[i].mvflags & G_GENOD)
                    gonecnt++;
                else
                    goodcnt++;
            }
        }
        /*
         * TODO[?]: If user's input doesn't match any class
         *          description, check individual species names.
         */
        if (!goodcnt && class != mons[urole.malenum].mlet &&
            class != mons[urace.malenum].mlet) {
            if (gonecnt)
                pline(msgc_cancelled,
                      "All such monsters are already nonexistent.");
            else if (immunecnt || (buf[0] == DEF_INVISIBLE && buf[1] == '\0'))
                pline(msgc_cancelled,
                      "You aren't permitted to genocide such monsters.");
            else if (wizard && buf[0] == '*') {
                pline(msgc_debug,
                      "Blessed genocide of '*' is deprecated. Use #levelcide "
                      "for the same result.\n");
                do_level_genocide();
                return;
            } else
                pline(msgc_cancelled,
                      "That symbol does not represent any monster.");
            continue;
        }

        for (i = LOW_PM; i < NUMMONS; i++) {
            if (mons[i].mlet == class) {
                const char *nam = makeplural(mons[i].mname);

                /* Although "genus" is Latin for race, the hero benefits from
                   both race and role; thus genocide affects either. */
                if (Your_Own_Role(i) || Your_Own_Race(i) ||
                    ((mons[i].geno & G_GENO)
                     && !(mvitals[i].mvflags & G_GENOD))) {
                    /* This check must be first since player monsters might
                       have G_GENOD or !G_GENO. */
                    mvitals[i].mvflags |= (G_GENOD | G_NOCORPSE);
                    reset_rndmonst(i);
                    kill_genocided_monsters();
                    update_inventory(); /* eggs & tins */
                    /* While endgame messages track whether you genocided
                     * by means other than looking at u.uconduct, call
                     * break_conduct anyway to correctly note the first turn
                     * in which it happened. */
                    break_conduct(conduct_genocide);
                    pline(msgc_intrgain, "Wiped out all %s.", nam);
                    if (Upolyd && i == u.umonnum) {
                        u.mh = -1;
                        if (Unchanging) {
                            if (!feel_dead++)
                                pline(msgc_fatal_predone, "You die.");
                            /* finish genociding this class of monsters before
                               ultimately dying */
                            gameover = TRUE;
                        } else
                            /* Cannot cause a "stuck in monster form" death
                               (!Unchanging); but cannot have a null reason
                               (u.mh just got damaged). Thus we use a
                               searchable string for the death reason that
                               looks out of place, because it should never
                               show up. */
                            rehumanize(GENOCIDED, "arbitrary death reason");
                    }
                    /* Self-genocide if it matches either your race or role.
                       Assumption: male and female forms share same monster
                       class. */
                    if (i == urole.malenum || i == urace.malenum) {
                        u.uhp = -1;
                        if (Upolyd) {
                            if (!feel_dead++)
                                pline(msgc_fatal, "You feel dead inside.");
                        } else {
                            if (!feel_dead++)
                                pline(msgc_fatal_predone, "You die.");
                            gameover = TRUE;
                        }
                    }
                } else if (mvitals[i].mvflags & G_GENOD) {
                    if (!gameover)
                        pline(msgc_yafm, "All %s are already nonexistent.",
                              nam);
                } else if (!gameover) {
                    /* suppress feedback about quest beings except for those
                       applicable to our own role */
                    if ((mons[i].msound != MS_LEADER ||
                         quest_info(MS_LEADER) == i)
                        && (mons[i].msound != MS_NEMESIS ||
                            quest_info(MS_NEMESIS) == i)
                        && (mons[i].msound != MS_GUARDIAN ||
                            quest_info(MS_GUARDIAN) == i)
                        /* non-leader/nemesis/guardian role-specific monster */
                        && (i != PM_NINJA ||    /* nuisance */
                            Role_if(PM_SAMURAI))) {
                        boolean named, uniq;

                        named = type_is_pname(&mons[i]) ? TRUE : FALSE;
                        uniq = (mons[i].geno & G_UNIQ) ? TRUE : FALSE;
                        /* one special case */
                        if (i == PM_HIGH_PRIEST)
                            uniq = FALSE;

                        pline(msgc_yafm,
                              "You aren't permitted to genocide %s%s.",
                              (uniq && !named) ? "the " : "",
                              (uniq || named) ? mons[i].mname : nam);
                    }
                }
            }
        }
        if (gameover || u.uhp == -1) {
            (gameover ? done : set_delayed_killer)(
                GENOCIDED, killer_msg(GENOCIDED,
                                      "a blessed scroll of genocide"));
        }
        return;
    }
}

void
do_level_genocide(void)
{
    /* to aid in topology testing; remove pesky monsters */
    struct monst *mtmp, *mtmp2;

    int gonecnt = 0;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;
        if (DEADMONSTER(mtmp))
            continue;
        mongone(mtmp);
        gonecnt++;
    }
    pline(msgc_debug, "Eliminated %d monster%s.", gonecnt, plur(gonecnt));
}

#define REALLY 1
#define PLAYER 2
#define ONTHRONE 4
/* how: 0 = no genocide; create monsters (cursed scroll) */
/*      1 = normal genocide */
/*      3 = forced genocide of player */
/*      5 (4 | 1) = normal genocide from throne */
void
do_genocide(int how)
{
    const char *buf;
    int i, killplayer = 0;
    int mndx;
    const struct permonst *ptr;
    const char *which;

    if (how & PLAYER) {
        mndx = u.umonster;      /* non-polymorphed mon num */
        ptr = &mons[mndx];
        buf = msg_from_string(ptr->mname);
        killplayer++;
    } else {
        for (i = 0;; i++) {
            if (i >= 5) {
                pline(msgc_cancelled, "That's enough tries!");
                return;
            }
            buf = getlin("What monster do you want to genocide? [type the name]",
                         FALSE); /* not meaningfully repeatable... */
            buf = msgmungspaces(buf);
            /* choosing "none" preserves genocideless conduct */
            if (!strcmpi(buf, "none") || !strcmpi(buf, "nothing")) {
                /* ... but no free pass if cursed */
                if (!(how & REALLY)) {
                    ptr = rndmonst(&u.uz, rng_main);
                    if (!ptr)
                        return; /* no message, like normal case */
                    mndx = monsndx(ptr);
                    break;      /* remaining checks don't apply */
                } else
                    return;
            }

            mndx = name_to_mon(buf);
            if (mndx == NON_PM || (mvitals[mndx].mvflags & G_GENOD)) {
                pline(msgc_cancelled, "Such creatures %s exist in this world.",
                      (mndx == NON_PM) ? "do not" : "no longer");
                continue;
            }
            ptr = &mons[mndx];
            /* Although "genus" is Latin for race, the hero benefits from both
               race and role; thus genocide affects either. */
            if (Your_Own_Role(mndx) || Your_Own_Race(mndx)) {
                killplayer++;
                break;
            }
            if (is_human(ptr))
                adjalign(-sgn(u.ualign.type));
            if (is_demon(ptr))
                adjalign(sgn(u.ualign.type));

            if (!(ptr->geno & G_GENO)) {
                if (canhear()) {
                    /* fixme: unconditional "caverns" will be silly in some
                       circumstances */
                    pline(msgc_npcvoice,
                          "A thunderous voice booms through the caverns:");
                    verbalize(msgc_hint, "No, %s!  That will not be done.",
                              mortal_or_creature(youmonst.data, TRUE));
                }
                continue;
            }
            /* KMH -- Unchanging prevents rehumanization */
            if (Unchanging && ptr == youmonst.data)
                killplayer++;
            break;
        }
    }

    which = "all ";
    if (Hallucination) {
        if (Upolyd)
            buf = youmonst.data->mname;
        else {
            buf = (u.ufemale && urole.name.f) ? urole.name.f : urole.name.m;
            buf = msglowercase(msg_from_string(buf));
        }
    } else {
        buf = ptr->mname;       /* make sure we have standard singular */
        if ((ptr->geno & G_UNIQ) && ptr != &mons[PM_HIGH_PRIEST])
            which = !type_is_pname(ptr) ? "the " : "";
    }
    if (how & REALLY) {
        /* setting no-corpse affects wishing and random tin generation */
        mvitals[mndx].mvflags |= (G_GENOD | G_NOCORPSE);
        pline(msgc_intrgain, "Wiped out %s%s.", which,
              (*which != 'a') ? buf : makeplural(buf));

        if (killplayer) {
            /* might need to wipe out dual role */
            if (urole.femalenum != NON_PM && mndx == urole.malenum)
                mvitals[urole.femalenum].mvflags |= (G_GENOD | G_NOCORPSE);
            if (urole.femalenum != NON_PM && mndx == urole.femalenum)
                mvitals[urole.malenum].mvflags |= (G_GENOD | G_NOCORPSE);
            if (urace.femalenum != NON_PM && mndx == urace.malenum)
                mvitals[urace.femalenum].mvflags |= (G_GENOD | G_NOCORPSE);
            if (urace.femalenum != NON_PM && mndx == urace.femalenum)
                mvitals[urace.malenum].mvflags |= (G_GENOD | G_NOCORPSE);

            u.uhp = -1;

            const char *killer;
            if (how & PLAYER) {
                killer = killer_msg(GENOCIDED, "genocidal confusion");
            } else if (how & ONTHRONE) {
                /* player selected while on a throne */
                killer = killer_msg(GENOCIDED, "an imperious order");
            } else {    /* selected player deliberately, not confused */
                killer = killer_msg(GENOCIDED, "a scroll of genocide");
            }

            /* Polymorphed characters will die as soon as they're rehumanized. */
            /* KMH -- Unchanging prevents rehumanization */
            if (Upolyd && ptr != youmonst.data) {
                pline(msgc_fatal, "You feel dead inside.");
                set_delayed_killer(GENOCIDED, killer);
            } else
                done(GENOCIDED, killer);
        } else if (ptr == youmonst.data) {
            /* As above: the death reason should never be relevant. */
            rehumanize(GENOCIDED, "arbitrary death reason");
        }
        reset_rndmonst(mndx);
        /* While endgame messages track whether you genocided
         * by means other than looking at u.uconduct, call
         * break_conduct anyway to correctly note the first turn
         * in which it happened. */
        break_conduct(conduct_genocide);
        kill_genocided_monsters();
        update_inventory();     /* in case identified eggs were affected */
    } else {
        int cnt = 0;

        if (!(mons[mndx].geno & G_UNIQ) &&
            !(mvitals[mndx].mvflags & (G_GENOD | G_EXTINCT)))
            for (i = rn1(3, 4); i > 0; i--) {
                if (!makemon(ptr, level, u.ux, u.uy, NO_MINVENT))
                    break;      /* couldn't make one */
                ++cnt;
                if (mvitals[mndx].mvflags & G_EXTINCT)
                    break;      /* just made last one */
            }
        if (cnt)
            pline(msgc_substitute, "Sent in some %s.", makeplural(buf));
        else
            pline(msgc_noconsequence, "Nothing happens.");
    }
}

void
punish(struct obj *sobj)
{
    /* KMH -- Punishment is still okay when you are riding */
    pline(msgc_statusbad, "You are being punished for your misbehavior!");
    if (Punished) {
        pline_implied(msgc_statusbad, "Your iron ball gets heavier.");
        uball->owt += 160 * (1 + sobj->cursed);
        return;
    }
    if (amorphous(youmonst.data) || is_whirly(youmonst.data) ||
        unsolid(youmonst.data)) {
        pline_implied(msgc_playerimmune,
                      "A ball and chain appears, then falls away.");
        dropy(mkobj(level, BALL_CLASS, TRUE, rng_main));
        return;
    }
    uchain = mkobj(level, CHAIN_CLASS, TRUE, rng_main);
    uball = mkobj(level, BALL_CLASS, TRUE, rng_main);
    uball->spe = 1;     /* special ball (see save) */

    /* Place ball & chain. We can do this while swallowed now, too (and must do,
       because otherwise they'll be floating objects). */
    placebc();
    if (Blind)
        set_bc(1);  /* set up ball and chain variables */
    newsym(u.ux, u.uy);     /* see ball&chain if can't see self */
}

void
unpunish(void)
{       /* remove the ball and chain */
    struct obj *savechain = uchain;

    obj_extract_self(uchain);
    newsym(uchain->ox, uchain->oy);
    dealloc_obj(savechain);
    uball->spe = 0;
    uball = uchain = NULL;
}

/* some creatures have special data structures that only make sense in their
 * normal locations -- if the player tries to create one elsewhere, or to revive
 * one, the disoriented creature becomes a zombie
 */
boolean
cant_create(int *mtype, boolean revival)
{

    /* SHOPKEEPERS can be revived now */
    if (*mtype == PM_GUARD || (*mtype == PM_SHOPKEEPER && !revival)
        || *mtype == PM_ALIGNED_PRIEST /* || *mtype==PM_ANGEL */ ) {
        *mtype = PM_HUMAN_ZOMBIE;
        return TRUE;
    } else if (*mtype == PM_LONG_WORM_TAIL) {   /* for create_particular() */
        *mtype = PM_LONG_WORM;
        return TRUE;
    }
    return FALSE;
}


/*
 * Make a new monster with the type controlled by the user.
 *
 * Note:  when creating a monster by class letter, specifying the
 * "strange object" (']') symbol produces a random monster rather
 * than a mimic; this behavior quirk is useful so don't "fix" it...
 */
boolean
create_particular(const struct nh_cmd_arg *arg)
{
    char monclass = MAXMCLASSES;
    const char *buf, *bufp;
    int which, tries, i;
    const struct permonst *whichpm;
    struct monst *mtmp;
    boolean madeany = FALSE;
    boolean parseadjective = TRUE;
    boolean maketame, makepeaceful, makehostile;
    boolean cancelled, fast, slow, revived;
    boolean fleeing, blind, paralyzed, sleeping;
    boolean stunned, confused, suspicious, mavenge;
    int quan = 1;

    /* This is explicitly a debug mode operation only. (In addition to the
       rather unflavoured messages, it prints some additional debug info that
       would leak info in a non-debug-mode game.) */
    if (!wizard)
        impossible("calling create_particular outside debug mode");

    if ((arg->argtype & CMD_ARG_LIMIT) && arg->limit > 1)
        quan = arg->limit;

    tries = 0;
    do {
        which = urole.malenum;  /* an arbitrary index into mons[] */
        maketame = makepeaceful = makehostile = FALSE;
        cancelled = fast = slow = revived = fleeing = blind = mavenge = FALSE;
        paralyzed = sleeping = stunned = confused = suspicious = FALSE;
        buf = getarglin(
            arg, "Create what kind of monster? [type the name or symbol]");
        bufp = msgmungspaces(buf);
        if (*bufp == '\033')
            return FALSE;

        /* get quantity */
        if (digit(*bufp) && strcmp(bufp, "0")) {
            quan = atoi(bufp);
            while (digit(*bufp))
                bufp++;
            while (*bufp == ' ')
                bufp++;
        }

        while (parseadjective) {
            parseadjective = FALSE;
            /* allow the initial disposition to be specified */
            if (!strncmpi(bufp, "tame ", 5)) {
                bufp += 5;
                parseadjective = maketame = TRUE;
            } else if (!strncmpi(bufp, "peaceful ", 9)) {
                bufp += 9;
                parseadjective = makepeaceful = TRUE;
            } else if (!strncmpi(bufp, "hostile ", 8)) {
                bufp += 8;
                parseadjective = makehostile = TRUE;
            }
            /* allow status effects to be specified */
            if (!strncmpi(bufp, "cancelled ", 10)) {
                bufp += 10;
                parseadjective = cancelled = TRUE;
            } else if (!strncmpi(bufp, "canceled ", 9)) {
                bufp += 9;
                parseadjective = cancelled = TRUE;
            }
            if (!strncmpi(bufp, "fast ", 5)) {
                bufp += 5;
                parseadjective = fast = TRUE;
            } else if (!strncmpi(bufp, "slow ", 5)) {
                bufp += 5;
                parseadjective = slow = TRUE;
            }
            if (!strncmpi(bufp, "revived ", 8)) {
                bufp += 9;
                parseadjective = revived = TRUE;
            }
            if (!strncmpi(bufp, "fleeing ", 8)) {
                bufp += 8;
                parseadjective = fleeing = TRUE;
            }
            if (!strncmpi(bufp, "blind ", 6)) {
                bufp += 6;
                parseadjective = blind = TRUE;
            }
            if (!strncmpi(bufp, "paralyzed ", 10)) {
                bufp += 10;
                parseadjective = paralyzed = TRUE;
            }
            if (!strncmpi(bufp, "sleeping ", 9)) {
                bufp += 9;
                parseadjective = sleeping = TRUE;
            }
            if (!strncmpi(bufp, "stunned ", 8)) {
                bufp += 8;
                parseadjective = stunned = TRUE;
            }
            if (!strncmpi(bufp, "confused ", 9)) {
                bufp += 9;
                parseadjective = confused = TRUE;
            }
            if (!strncmpi(bufp, "suspicious ", 11)) {
                bufp += 11;
                parseadjective = suspicious = TRUE;
            }
            if (!strncmpi(bufp, "mavenge ", 8)) {
                bufp += 8;
                parseadjective = mavenge = TRUE;
            }
        }
            
        /* decide whether a valid monster was chosen */
        if (strlen(bufp) == 1) {
            monclass = def_char_to_monclass(*bufp);
            if (monclass != MAXMCLASSES)
                break;  /* got one */
        } else {
            which = name_to_mon(bufp);
            if (which >= LOW_PM)
                break;  /* got one */
        }
        /* no good; try again... */
        pline(msgc_cancelled, "I've never heard of such monsters.");
    } while (++tries < 5);

    mtmp = NULL;

    if (tries == 5) {
        pline(msgc_cancelled, "That's enough tries!");
    } else {
        cant_create(&which, FALSE);
        whichpm = &mons[which];
        for (i = 0; i < quan; i++) {
            /* no reason to use a custom RNG for wizmode commands */
            if (monclass != MAXMCLASSES)
                whichpm = mkclass(&u.uz, monclass, 0, rng_main);
            if (maketame) {
                mtmp = makemon(whichpm, level, u.ux, u.uy, MM_EDOG);
                if (mtmp)
                    initedog(mtmp);
            } else {
                mtmp = makemon(whichpm, level, u.ux, u.uy, NO_MM_FLAGS);
                if ((makepeaceful || makehostile) && mtmp)
                    msethostility(mtmp, !makepeaceful, TRUE);
            }
            if (mtmp) {
                madeany = TRUE;
                if (cancelled)
                    mtmp->mcan = 1;
                if (fast)
                    mtmp->mspeed = MFAST;
                else if (slow)
                    mtmp->mspeed = MSLOW;
                if (revived)
                    mtmp->mrevived = 1;
                if (fleeing)
                    mtmp->mflee = 1;
                if (blind)
                    mtmp->mcansee = 0;
                if (paralyzed)
                    mtmp->mcanmove = 0;
                if (sleeping)
                    mtmp->msleeping = 1;
                if (stunned)
                    mtmp->mstun = 1;
                if (confused)
                    mtmp->mconf = 1;
                if (suspicious)
                    mtmp->msuspicious = 1;
                if (mavenge)
                    mtmp->mavenge = 1;
            }
        }
    }

    /* Output the coordinates of one created monster. This is used by the
       testbench, so that it can #genesis up a monster and then start aiming at
       it. (Note that #genesis is zero-time, meaning that the monster will have
       no chance to move out of position until after the player's next turn.
       Arguably, it would ease debugging further to give the monster summoning
       sickness too, but we don't do that yet, because other sorts of debugging
       might want the monster to be as "normal" as possible.) */
    if (mtmp) {
        int dx = mtmp->mx - u.ux;
        int dy = mtmp->my - u.uy;

        enum nh_direction dir = DIR_NONE;

        if (dx == 0)
            dir = dy < 0 ? DIR_N : DIR_S;
        else if (dy == 0)
            dir = dx < 0 ? DIR_W : DIR_E;
        else if (dx == dy)
            dir = dx < 0 ? DIR_NW : DIR_SE;
        else if (dx == -dy)
            dir = dx < 0 ? DIR_SW : DIR_NE;

        pline(msgc_debug, "Created a monster at (%d, %d), direction %d",
              mtmp->mx, mtmp->my, dir);
    }

    return madeany;
}

/*read.c*/
