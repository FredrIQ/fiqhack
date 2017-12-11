/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#define Your_Own_Role(mndx) ((mndx) == urole.num)
#define Your_Own_Race(mndx) ((mndx) == urace.num)

static const char readable[] = { ALL_CLASSES, 0 };
static const char all_count[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

static void wand_explode(struct monst *, struct obj *);
static void do_class_genocide(struct monst *);
static int mon_choose_reverse_genocide(struct monst *);
static int mon_choose_genocide(struct monst *, boolean, int);
static int maybe_target_class(boolean, int);
static void stripspe(struct monst *, struct obj *);
static void p_glow1(enum msg_channel, const struct monst *,
                    struct obj *);
static void p_glow2(enum msg_channel, const struct monst *,
                    struct obj *, const char *);
static void randomize(int *, int);
static void forget_single_object(int);
static void forget(int);
static void maybe_tame(struct monst *, struct monst *, struct obj *);
static void do_earth(struct level *, int, int, int, struct monst *);
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
                  is_silent(youmonst.data) || strangled(&youmonst) ? "cogitate" :
                  "pronounce");
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
    if (!seffects(&youmonst, scroll, &known)) {
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
stripspe(struct monst *mon, struct obj *obj)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    /* BUG: not really right if obj used to be cancelled */
    if ((obj->blessed || obj->spe <= 0) && (you || vis))
        pline(you ? msgc_noconsequence : msgc_monneutral,
              "Nothing happens.");
    else {
        obj->spe = 0;
        if (obj->otyp == OIL_LAMP || obj->otyp == BRASS_LANTERN)
            obj->age = 0;
        if (you || vis)
            pline(you ? msgc_itemloss : msgc_monneutral,
                  "%s %s %s briefly.",
                  you ? "Your" : s_suffix(Monnam(mon)),
                  xname(obj), otense(obj, "vibrate"));
    }
}

static void
p_glow1(enum msg_channel msgc, const struct monst *mon,
        struct obj *otmp)
{
    boolean you = (mon == &youmonst);
    pline(you ? msgc : msgc_monneutral, "%s %s %s briefly.",
          you ? "Your" : s_suffix(Monnam(mon)),
          you ? xname(otmp) : distant_name(otmp, doname),
          otense(otmp, Blind ? "vibrate" : "glow"));
}

static void
p_glow2(enum msg_channel msgc, const struct monst *mon,
        struct obj *otmp, const char *color)
{
    boolean you = (mon == &youmonst);
    pline(you ? msgc : msgc_monneutral,
          "%s %s %s%s%s for a moment.",
          you ? "Your" : s_suffix(Monnam(mon)),
          you ? xname(otmp) : distant_name(otmp, doname),
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
       list, to avoid telling them apart from oil lamps with a ?oC. */
    if (obj->oclass == TOOL_CLASS)
        return (boolean) (objects[obj->otyp].oc_charged ||
                          obj->otyp == BRASS_LANTERN || obj->otyp == OIL_LAMP ||
                          obj->otyp == MAGIC_LAMP);

    return FALSE;       /* why are weapons/armor considered charged anyway? */
}

/* Monster choosing what to recharge. This is done by applying a score to
   each item and then pick the one with the highest */
struct obj *
mon_choose_recharge(struct monst *mon, int bcsign)
{
    struct obj *obj;
    struct obj *obj_best = NULL;
    int score = -1;
    int score_best = -1;
    int otyp;
    /* count object amount */
    for (obj = mon->minvent; obj; obj = obj->nobj) {
        score = -1;
        otyp = obj->otyp;
        /* check for a valid object -- we can't use
           is_chargeable because it deals with hero IDing */
        if (obj->oclass == WAND_CLASS ||
            (obj->oclass == RING_CLASS &&
             objects[otyp].oc_charged) ||
            (obj->oclass == TOOL_CLASS &&
             objects[otyp].oc_charged &&
             !is_weptool(obj)) ||
            otyp == OIL_LAMP ||
            otyp == BRASS_LANTERN)
            score++;
        else
            continue;
        /* sanity checks for guranteed wand/ring explodes */
        if ((obj->oclass == WAND_CLASS &&
             ((otyp == WAN_WISHING &&
               (obj->recharged || obj->spe >= 3)) ||
              obj->recharged == 7)) ||
            (obj->oclass == RING_CLASS &&
             obj->spe > 6)) {
            score = -1;
            continue;
        }
        /* FIXME: a ring is currently unusable for monsters, priority it last */
        if (obj->oclass != RING_CLASS) {
            if (obj->oclass == WAND_CLASS) {
                score = 60;
                /* prefer unrecharged wands */
                score -= (10 * obj->recharged);
                switch (otyp) {
                case WAN_WISHING:
                    /* a wand of wishing should preferably be blessed charged
                       when 0 charges left... */
                    if (bcsign != 1 || obj->spe > 0)
                        score = 3 - obj->spe; /* only selected if nothing else to charge */
                    else
                        score = 500; /* always charge this first */
                    break;
                case WAN_DEATH: /* offensive items below */
                    score += 3;
                case WAN_CREATE_MONSTER:
                    score += 3;
                case WAN_SLEEP:
                    score += 3;
                case WAN_SLOW_MONSTER:
                    score += 3;
                case WAN_LIGHTNING:
                    score += 3;
                case WAN_FIRE:
                case WAN_COLD:
                    score += 3;
                case WAN_MAGIC_MISSILE:
                    score += 3;
                case WAN_STRIKING:
                    score += 3;
                case WAN_UNDEAD_TURNING:
                    score += 3;
                    score += 10; /* +10 for all offensive items */
                case WAN_DIGGING: /* escape items below */
                    score += 3;
                case WAN_TELEPORTATION:
                    score += 3;
                    score += 5; /* +5 for all escape items */
                case WAN_SPEED_MONSTER: /* misc items below */
                    score += 3;
                case WAN_MAKE_INVISIBLE:
                    score += 3;
                case WAN_POLYMORPH:
                    score += 3;
                    score += 10; /* +10 for all usable wands */
                default: /* other wands are unusable */
                    /* penalty for wands with charges left */
                    if (obj->spe >= 0)
                        score /= (obj->spe + 1);
                    /* major penalty for overcharged wands */
                    if (obj->spe >= 6) {
                        score -= 20;
                        if (score < 5)
                            score = 5;
                    }
                }
            } else { /* tools */
                score = 20;
                switch (otyp) {
                case MAGIC_MARKER:
                    /* a magic marker can't be recharged if already done once */
                    if (obj->recharged) {
                        score = -1;
                        break;
                    }
                    if (obj->spe >= 16) { /* charging now is unnecessary */
                        score = 50 - obj->spe;
                        if (score < 4)
                            score = 4;
                        break;
                    }
                    score += 5;
                case FIRE_HORN: /* offensive items */
                case FROST_HORN:
                    score += 3;
                    score += 10; /* +10 for offensive items */
                default: /* everything else is unusable for now */
                    break;
                }
            }
            /* avoid cursed charging unless obj is cancelled */
            if (bcsign == -1 && obj->spe >= 0)
                score = -1;
        } else {
            /* rings unusable for now, but try not to blow things up */
            score = 6 - obj->spe;
            /* cursed ring charging is always harmful */
            if (bcsign == -1)
                score = -1;
        }

        if (score >= score_best) {
            obj_best = obj;
            score_best = score;
        }
    }
    if (!obj_best || score_best <= -1) /* nothing to charge */
        return NULL;
    return obj_best;
}
/*
 * recharge an object; curse_bless is -1 if the recharging implement
 * was cursed, +1 if blessed, 0 otherwise.
 */
void
recharge(struct monst *mon, struct obj *obj, int curse_bless)
{
    int n;
    boolean is_cursed, is_blessed;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    const char *your;
    int timer_offset = 0; /* for lit objects */
    your = (you ? "Your" : s_suffix(Monnam(mon)));

    is_cursed = curse_bless < 0;
    is_blessed = curse_bless > 0;

    /* Scrolls of charging now ID charge count, as well as doing the charging,
       unless cursed. */
    if (!is_cursed) {
        if (you || vis)
            obj->known = 1;
        if (!you)
            obj->mknown = 1;
    }

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
            wand_explode(mon, obj);
            return;
        }
        /* didn't explode, so increment the recharge count */
        obj->recharged = (unsigned)(n + 1);

        /* now handle the actual recharging */
        if (is_cursed) {
            stripspe(mon, obj);
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
                wand_explode(mon, obj);
                return;
            }
            if (you || vis) {
                if (obj->spe >= lim)
                    p_glow2(msgc_itemrepair, mon, obj, "blue");
                else
                    p_glow1(msgc_itemrepair, mon, obj);
            }
        }

    } else if (obj->oclass == RING_CLASS && objects[obj->otyp].oc_charged) {
        /* charging does not affect ring's curse/bless status */
        int s = is_blessed ? rnd(3) : is_cursed ? -rnd(2) : 1;
        boolean is_on = FALSE;
        if (you)
            is_on = (obj == uleft || obj == uright);
        else
            is_on = !!obj->owornmask;

        /* destruction depends on current state, not adjustment */
        if (obj->spe > rn2(7) || obj->spe <= -5) {
            if (you || vis)
                pline(you ? msgc_itemloss : msgc_monneutral,
                      "%s %s %s momentarily, then %s!", your, xname(obj),
                      otense(obj, "pulsate"), otense(obj, "explode"));
            if (is_on) {
                if (you)
                    setunequip(obj);
                else {
                    mon->misc_worn_check &= ~obj->owornmask;
                    obj->owornmask = 0;
                }
            }
            s = rnd(3 * abs(obj->spe)); /* amount of damage */
            if (you) {
                useup(obj);
                losehp(s, killer_msg(DIED, "an exploding ring"));
            } else {
                m_useup(mon, obj);
                mon->mhp -= s;
                if (mon->mhp <= 0)
                    mondied(mon);
            }
        } else {
            enum objslot slot = obj == uleft ? os_ringl : os_ringr;

            pline(!you ? msgc_monneutral : s > 0 ? msgc_itemrepair : msgc_itemloss,
                  "%s %s spins %sclockwise for a moment.", your, xname(obj),
                  s < 0 ? "counter" : "");
            /* cause attributes and/or properties to be updated */
            if (is_on && you)
                setunequip(obj);
            obj->spe += s;      /* update the ring while it's off */
            if (is_on && you)
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
                stripspe(mon, obj);
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
                stripspe(mon, obj);
            else if (rechrg && obj->otyp == MAGIC_MARKER) {
                /* previously recharged */
                obj->recharged = 1;     /* override increment done above */
                if (obj->spe < 3 && (you || vis))
                    pline(you ? msgc_failcurse : msgc_monneutral,
                          "%s marker seems permanently dried out.", your);
                else if (you || vis)
                    pline(you ? msgc_failcurse : msgc_monneutral,
                          "Nothing happens.");
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
                if (you || vis)
                    p_glow2(msgc_itemrepair, mon, obj, "blue");
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
                if (you || vis)
                    p_glow2(msgc_itemrepair, mon, obj, "white");
            }
            break;
        case OIL_LAMP:
        case BRASS_LANTERN:
            if (obj->lamplit) {
                timer_offset = report_timer(obj->olev, BURN_OBJECT,
                                            (const void *)obj);
                timer_offset -= moves;
            }

            if (is_cursed) {
                stripspe(mon, obj);
                if (obj->lamplit) {
                    if (!blind(&youmonst) && (you || vis))
                        pline(you ? msgc_consequence : msgc_monneutral,
                              "%s out!", Tobjnam(obj, "go"));
                    end_burn(obj, TRUE);
                }
            } else if (is_blessed) {
                obj->spe = 1;
                obj->age = 1500 - timer_offset;
                if (you || vis)
                    p_glow2(msgc_itemrepair, mon, obj, "blue");
            } else {
                obj->spe = 1;
                obj->age += 750;
                if ((obj->age + timer_offset) > 1500)
                    obj->age = 1500 - timer_offset;
                if (you || vis)
                    p_glow1(msgc_itemrepair, mon, obj);
            }
            break;
        case CRYSTAL_BALL:
            if (is_cursed)
                stripspe(mon, obj);
            else if (is_blessed) {
                obj->spe = 6;
                if (you || vis)
                    p_glow2(msgc_itemrepair, mon, obj, "blue");
            } else {
                if (obj->spe < 5) {
                    obj->spe++;
                    if (you || vis)
                        p_glow1(msgc_itemrepair, mon, obj);
                } else if (you || vis)
                    pline(you ? msgc_failcurse : msgc_monneutral,
                          "Nothing happens.");
            }
            break;
        case HORN_OF_PLENTY:
        case BAG_OF_TRICKS:
        case CAN_OF_GREASE:
            if (is_cursed)
                stripspe(mon, obj);
            else if (is_blessed) {
                if (obj->spe <= 10)
                    obj->spe += rn1(10, 6);
                else
                    obj->spe += rn1(5, 6);
                if (obj->spe > 50)
                    obj->spe = 50;
                if (you || vis)
                    p_glow2(msgc_itemrepair, mon, obj, "blue");
            } else {
                obj->spe += rnd(5);
                if (obj->spe > 50)
                    obj->spe = 50;
                if (you || vis)
                    p_glow1(msgc_itemrepair, mon, obj);
            }
            break;
        case MAGIC_FLUTE:
        case MAGIC_HARP:
        case FROST_HORN:
        case FIRE_HORN:
        case DRUM_OF_EARTHQUAKE:
            if (is_cursed) {
                stripspe(mon, obj);
            } else if (is_blessed) {
                obj->spe += dice(2, 4);
                if (obj->spe > 20)
                    obj->spe = 20;
                if (you || vis)
                    p_glow2(msgc_itemrepair, mon, obj, "blue");
            } else {
                obj->spe += rnd(4);
                if (obj->spe > 20)
                    obj->spe = 20;
                if (you || vis)
                    p_glow1(msgc_itemrepair, mon, obj);
            }
            break;
        default:
            goto not_chargable;
        }       /* switch */
        
    } else {
    not_chargable:
        if (you)
            pline(msgc_badidea, "You have a feeling of loss.");
        else
            impossible("monster picked nothing or bad item to charge?");
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
maybe_tame(struct monst *mon, struct monst *mtmp, struct obj *sobj)
{
    boolean you = (mon == &youmonst);
    /* cursed objects has no effect for peacefuls */
    if (sobj->cursed && !you && !mon->mtame && mon->mpeaceful)
        return;

    /* no effect on self */
    if (mon == mtmp)
        return;

    /* if resisted, do nothing */
    if (!mx_eshk(mtmp) && resist(mon, mtmp, sobj->oclass, NOTELL, bcsign(sobj)))
        return;

    /* -1: hostile, 0: peaceful, 1: tame */
    int tame_result = -1;
    if (mon->mpeaceful && !you && !mon->mtame)
        tame_result = 0;
    else if (sobj->cursed ?
             (!you && !mon->mtame) :
             (you || mon->mtame))
        tame_result = 1;

    if (mtmp == &youmonst) {
        /* taming on player increases or decreases the alignment record */
        adjalign(tame_result * 5);
        if (tame_result != 0)
            pline(tame_result > 0 ? msgc_intrgain : msgc_intrloss,
                  "You feel %s %s.",
                  tame_result > 0 ? "appropriately" : "insufficiently",
                  align_str(u.ualign.type));
        return;
    }

    tame_result++;
    switch (tame_result) {
    case 0:
        if (!mtmp->mpeaceful && !mtmp->mtame)
            break;

        if (!you) /* setmangry screws with your alignment */
            msethostility(mtmp, TRUE, FALSE);
        else
            setmangry(mtmp);
        break;
    case 1:
        if (mtmp->mpeaceful && !mtmp->mtame)
            break;

        if (!mx_eshk(mtmp)) /* peaceful taming keeps hostile shk */
            msethostility(mtmp, FALSE, FALSE);
        break;
    case 2:
        if (mtmp->mtame)
            break;

        if (mx_eshk(mtmp))
            make_happy_shk(mtmp, FALSE);
        else
            tamedog(mtmp, NULL);
    }
}

static void
do_earth(struct level *lev, int x, int y, int confused,
         struct monst *magr)
{
    struct obj *obj = mksobj(lev, confused ? ROCK : BOULDER,
                             FALSE, FALSE, rng_main);

    if (!obj)
        return; /* Shouldn't happen */
    obj->quan = confused ? rn1(5, 2) : 1;
    obj->owt = weight(obj);

    struct monst *mdef = um_at(lev, x, y);

    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef || canseemon(magr) ||
                   canseemon(mdef));

    boolean steed = (udef && u.usteed);
    int dmg = 0;
    int sdmg = 0; /* for the steed */

    struct obj *armh;
    if (mdef && !amorphous(mdef->data) &&
        !phasing(mdef) &&
        !noncorporeal(mdef->data) &&
        !unsolid(mdef->data)) {
        armh = which_armor(mdef, os_armh);
        dmg = dmgval(obj, mdef) * obj->quan;

        if (!udef && !canseemon(mdef))
            map_invisible(mdef->mx, mdef->my);
        if (!armh || !is_metallic(armh)) {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s hit by %s!", M_verbs(mdef, "are"),
                      doname(obj));
        } else {
            if (vis)
                pline(combat_msgc(magr, mdef, cr_resist),
                      "%s bounces off %s hard %s.",
                      An(xname(obj)), udef ? "your" :
                      s_suffix(mon_nam(mdef)),
                      helmet_name(armh));
            else
                You_hear(msgc_levelsound,
                         "a clanging sound.");
            if (dmg > 2)
                dmg = 2;
        }

        if (udef) {
            /* Check steed */
            if (steed) {
                armh = which_armor(u.usteed, os_armh);
                sdmg = dmgval(obj, u.usteed) * obj->quan;

                if (!armh || !is_metallic(armh))
                    pline(combat_msgc(magr, u.usteed, cr_hit),
                          "%s hit by %s!",
                          M_verbs(u.usteed, "are"), doname(obj));
                else {
                    pline(combat_msgc(magr, u.usteed, cr_resist),
                          "%s bounces off %s hard %s.",
                          An(xname(obj)),
                          s_suffix(mon_nam(u.usteed)),
                          helmet_name(armh));
                    if (sdmg > 2)
                        sdmg = 2;
                }
            }
        }
    }

    /* Must come before the hit to avoid bones oddities */
    if (!flooreffects(obj, x, y, "fall")) {
        place_object(obj, lev, x, y);
        stackobj(obj);
        newsym(x, y);   /* map the rock */
    }

    /* The monster/steed might have died already from flooreffects,
       so we need to check for it. */
    if (sdmg && u.usteed && !DEADMONSTER(u.usteed)) {
        u.usteed->mhp -= sdmg;
        if (u.usteed->mhp <= 0)
            monkilled(magr, u.usteed, "", AD_PHYS);
    }

    if (udef) {
        if (uagr)
            losehp(dmg, killer_msg(DIED, "a scroll of earth"));
        else {
            const char *kbuf;
            kbuf = msgprintf("%s scroll of earth",
                             s_suffix(k_monnam(magr)));
            losehp(dmg, killer_msg(DIED, kbuf));
        }
    } else if (mdef && !DEADMONSTER(mdef)) {
        mdef->mhp -= dmg;
        if (mdef->mhp <= 0)
            monkilled(magr, mdef, "", AD_PHYS);
    }
}

/* Returns 1 if object was used up already, otherwise 0 */
int
seffects(struct monst *mon, struct obj *sobj, boolean *known)
{
    int cval;
    boolean confused = !!confused(mon);
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    boolean spell = (sobj->oclass == SPBOOK_CLASS);
    /* Monster knew BUC */
    boolean mbknown = (spell || sobj->mbknown);
    const char *your = you ? "Your" : s_suffix(Monnam(mon));
    struct obj *otmp;
    struct obj *obj;
    struct obj *twep = (m_mwep(mon));
    enum rng rng = rng_main; /* In case RNG will switch, do so for you only */

    if (you && objects[sobj->otyp].oc_magic)
        exercise(A_WIS, TRUE);  /* just for trying */
    switch (sobj->otyp) {
    case SCR_ENCHANT_ARMOR:
        {
            schar s;
            boolean special_armor;
            boolean same_color;

            otmp = some_armor(mon);
            if (!otmp) {
                if (you) {
                    strange_feeling(sobj,
                                    !Blind ? "Your skin glows then fades." :
                                    "Your skin feels warm for a moment.");
                    exercise(A_CON, !sobj->cursed);
                    exercise(A_STR, !sobj->cursed);
                } else if (vis)
                    pline(msgc_monneutral,
                          "%s skin glows then fades.",
                          s_suffix(Monnam(mon)));
                return 1;
            }
            if (confused) {
                otmp->oerodeproof = !(sobj->cursed);
                if (Blind) {
                    otmp->rknown = FALSE;
                    if (you)
                        pline(msgc_nospoil, "Your %s %s warm for a moment.",
                              xname(otmp), otense(otmp, "feel"));
                } else {
                    if (you || vis) {
                        otmp->rknown = TRUE;
                        pline(!you ? msgc_monneutral :
                              sobj->cursed ? msgc_itemloss : msgc_itemrepair,
                              "%s %s %s covered by a %s %s %s!", your,
                              xname(otmp),
                              otense(otmp, "are"),
                              sobj->cursed ? "mottled" : "shimmering",
                              hcolor(sobj->cursed ? "black" : "golden"),
                              sobj->cursed ? "glow" : (is_shield(otmp) ? "layer" :
                                                       "shield"));
                    }
                }
                if (otmp->oerodeproof && (otmp->oeroded || otmp->oeroded2)) {
                    otmp->oeroded = otmp->oeroded2 = 0;
                    if (you || vis)
                        pline(you ? msgc_itemrepair : msgc_monneutral,
                              "%s %s %s as good as new!", your, xname(otmp),
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
                if (you || vis)
                    pline(you ? msgc_itemloss : msgc_monneutral,
                          "%s %s violently %s%s%s for a while, then %s.", your,
                          xname(otmp), otense(otmp, Blind ? "vibrate" : "glow"),
                          (!Blind && !same_color) ? " " : "",
                          (Blind || same_color) ? "" :
                          hcolor(sobj->cursed ? "black" : "silver"),
                          otense(otmp, "evaporate"));
                if (you) {
                    setunequip(otmp);
                    useup(otmp);
                } else {
                    mon->misc_worn_check &= ~(otmp->owornmask);
                    m_useup(mon, otmp);
                }
                break;
            }

            /* OTOH, a custom RNG for /this/ is useful. The main balance
               implication here is whether armour goes to 5 or stops at 4. As
               such, if we're enchanting from 2 or 3 precisely, and the
               resulting spe is 4 or 5 precisely, we re-randomize which.
               Exception: an uncursed enchant from 3 always goes to 4 (we never
               bump it up to 5). */
            if (you)
                rng = rng_armor_ench_4_5;
            s = sobj->cursed ? -1 :
                otmp->spe >= 9 ? (rn2(otmp->spe) == 0) :
                sobj->blessed ? rnd(3 - otmp->spe / 3) : 1;
            if ((otmp->spe == 2 || otmp->spe == 3) && sobj->blessed &&
                (otmp->spe + s == 4 || otmp->spe + s == 5))
                s = 4 + rn2_on_rng(2, rng) - otmp->spe;
            if (you && s < 0 && otmp->unpaid)
                costly_damage_obj(otmp);
            if (s >= 0 && otmp->otyp >= GRAY_DRAGON_SCALES &&
                otmp->otyp <= YELLOW_DRAGON_SCALES) {
                /* dragon scales get turned into dragon scale mail */
                if (you || vis)
                    pline(you ? msgc_itemrepair : msgc_monneutral,
                          "%s %s merges and hardens!", your, xname(otmp));
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
            pline(!you ? msgc_monneutral :
                  (otmp->known && s == 0) ? msgc_failrandom :
                  (!otmp->known && Hallucination) ? msgc_nospoil :
                  sobj->cursed ? msgc_itemloss : msgc_itemrepair,
                  "%s %s %s%s%s%s for a %s.", your, xname(otmp),
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
                *known = otmp->known;
            }

            if ((otmp->spe > (special_armor ? 5 : 3)) &&
                (special_armor || !rn2(7)))
                if (you)
                    pline_implied(otmp->known ? msgc_hint : msgc_itemrepair,
                                  "Your %s suddenly %s %s.",
                                  xname(otmp), otense(otmp, "vibrate"),
                                  Blind ? "again" : "unexpectedly");
            if (you && otmp->unpaid && s > 0)
                adjust_bill_val(otmp);
            break;
        }
    case SCR_DESTROY_ARMOR:
        {
            otmp = some_armor(mon);
            if (confused) {
                if (!otmp) {
                    if (you) {
                        strange_feeling(sobj, "Your bones itch.");
                        exercise(A_STR, FALSE);
                        exercise(A_CON, FALSE);
                    } else if (vis)
                        pline(msgc_monneutral, "%s looks itchy.",
                              Monnam(mon));
                    return 1;
                }
                otmp->oerodeproof = sobj->cursed;
                if (you || vis)
                    p_glow2(!sobj->bknown ? msgc_nospoil :
                            otmp->oerodeproof ? msgc_itemrepair :
                            msgc_itemloss, mon, otmp, "purple");
                break;
            }
            if (!sobj->cursed || !otmp || !otmp->cursed) {
                if (!destroy_arm(mon, otmp)) {
                    if (you) {
                        strange_feeling(sobj, "Your skin itches.");
                        exercise(A_STR, FALSE);
                        exercise(A_CON, FALSE);
                    } else if (vis)
                        pline(msgc_monneutral, "%s looks itchy.",
                              Monnam(mon));
                    return 1;
                } else if (you || vis)
                    *known = TRUE;
            } else {    /* armor and scroll both cursed */
                if (you || vis)
                    pline(you ? msgc_itemloss : msgc_monneutral,
                          "%s %s %s.", your, xname(otmp), otense(otmp, "vibrate"));
                if (otmp->spe >= -6) {
                    otmp->spe--;
                    if (you && (otmp->otyp == HELM_OF_BRILLIANCE ||
                                otmp->otyp == GAUNTLETS_OF_DEXTERITY))
                        makeknown(otmp->otyp);
                }
                if (!resists_stun(mon))
                    inc_timeout(mon, STUNNED, rn1(10, 10), FALSE);
            }
        }
        break;
    case SCR_CONFUSE_MONSTER:
    case SPE_CONFUSE_MONSTER:
        if (sobj->cursed)
            inc_timeout(mon, CONFUSION, rnd(100), FALSE);
        else if (confused) {
            if (!sobj->blessed) {
                if (you || vis)
                    pline(you ? msgc_statusbad : msgc_monneutral,
                          "%s %s begin to %s%s.", your,
                          makeplural(mbodypart(mon, HAND)),
                          Blind ? "tingle" : "glow ",
                          Blind ? "" : hcolor("purple"));
                inc_timeout(mon, CONFUSION, rnd(100), TRUE);
            } else {
                if (you || vis)
                    pline(you ? msgc_statusheal : msgc_monneutral,
                          "A %s%s surrounds %s %s.", Blind ? "" : hcolor("red"),
                          Blind ? "faint buzz" : " glow",
                          you ? "your" : s_suffix(mon_nam(mon)),
                          mbodypart(mon, HEAD));
                set_property(mon, CONFUSION, -2, TRUE);
            }
        } else {
            if (you) {
                if (!sobj->blessed) {
                    pline(msgc_statusgood,
                          "Your %s%s %s%s.", makeplural(body_part(HAND)),
                          Blind ? "" : " begin to glow",
                          Blind ? "tingle" : hcolor("red"),
                          u.umconf ? " even more" : "");
                    u.umconf++;
                } else {
                    if (Blind)
                        pline(msgc_statusgood, "Your %s tingle %s sharply.",
                              makeplural(body_part(HAND)),
                              u.umconf ? "even more" : "very");
                    else
                        pline(msgc_statusgood,
                              "Your %s glow a%s brilliant %s.",
                              makeplural(body_part(HAND)),
                              u.umconf ? "n even more" : "", hcolor("red"));
                    /* after a while, repeated uses become less effective */
                    if (u.umconf >= 40)
                        u.umconf++;
                    else
                        u.umconf += rn1(8, 2);
                }
            } else {
                impossible("Monster read/cast confuse monster, which is not implemented?");
            }
        }
        break;
    case SCR_SCARE_MONSTER:
    case SPE_CAUSE_FEAR:
        if (you) {
            int ct = 0;
            struct monst *mtmp;
            
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                if (cansee(mtmp->mx, mtmp->my)) {
                    if (confused || sobj->cursed) {
                        mtmp->mflee = mtmp->mfrozen = mtmp->msleeping = 0;
                        mtmp->mcanmove = 1;
                    } else if (!resist(mon, mtmp, sobj->oclass, NOTELL, bcsign(sobj)))
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
                         (confused ||
                          sobj->cursed) ? "sad wailing" : "maniacal laughter");
        } else
            impossible("Monster using scare monster/cause fear?");
        break;
    case SCR_BLANK_PAPER:
        if (you) {
            if (Blind)
                pline(msgc_yafm, "You don't remember there being any "
                      "magic words on this scroll.");
            else
                pline(msgc_yafm, "This scroll seems to be blank.");
        } else if (vis)
            pline(msgc_yafm,
                  "%s looks rather disappointed by the lack of any effects.",
                  Monnam(mon));
        if (you || vis)
            *known = TRUE;
        break;
    case SCR_REMOVE_CURSE:
    case SPE_REMOVE_CURSE:
        if (confused) {
            if (Hallucination) {
                if (you)
                    pline(msgc_itemloss,
                          "You feel the power of the Force against you!");
                else if (vis)
                    pline(msgc_monneutral,
                          "You feel the Force conspiring against %s!",
                          mon_nam(mon));
                else
                    pline(msgc_levelsound,
                          "You sense a disturbance in the Force...");
            } else if (you || vis)
                pline(you ? msgc_itemloss : msgc_monneutral,
                      "You feel as if %s need%s some help.",
                      you ? "you" : mon_nam(mon), you ? "" : "s");
        } else if (you && Hallucination)
            pline(msgc_itemrepair,
                  "You feel in touch with the Universal Oneness.");
        else if (you || vis)
            pline(you ? msgc_itemrepair : msgc_monneutral,
                  "You feel like someone is helping %s.",
                  you ? "you" : mon_nam(mon));
        
        if (sobj->cursed && (you || vis))
            pline(you ? msgc_failcurse : msgc_monneutral,
                  "The scroll disintegrates.");
        else {
            /* Monsters has no concept of a quiver -- try to emulate one here. This is done as follows:
               - Monster is wielding a sling: uncurse 1st found stackable gems
               - Otherwise: uncurse 1st found stackable weapon that isn't already wielded */
            struct obj *mquiver = NULL;
            for (obj = mon->minvent; obj; obj = obj->nobj) {
                long wornmask;
                
                /* gold isn't subject to cursing and blessing */
                if (obj->oclass == COIN_CLASS)
                    continue;
                
                wornmask = obj->owornmask & W_MASKABLE;
                if (wornmask && !sobj->blessed) {
                    /* handle a couple of special cases; we don't allow
                       auxiliary weapon slots to be used to artificially
                       increase number of worn items */
                    if (you && obj == uswapwep) {
                        if (!u.twoweap)
                            wornmask = 0L;
                    } else if (you && obj == uquiver) {
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
                    } else if (!you) {
                        if ((!mon->mw || mon->mw != obj) &&
                            ((obj->oclass == WEAPON_CLASS &&
                              objects[obj->otyp].oc_merge) ||
                             (obj->oclass == GEM_CLASS)) &&
                            (!mquiver || mquiver->oclass != GEM_CLASS)) {
                            if (obj->oclass == GEM_CLASS && mon->mw &&
                                mon->mw->otyp == SLING)
                                mquiver = obj;
                            else if (obj->oclass == WEAPON_CLASS &&
                                     !mquiver && objects[obj->otyp].oc_merge)
                                mquiver = obj;
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
            /* blessed scrolls already uncursed the item */
            if (mquiver && !sobj->blessed) {
                if (confused)
                    blessorcurse(mquiver, 2, rng_main);
                else
                    uncurse(mquiver);
            }
        }
        if (you && Punished && !confused)
            unpunish();
        if (you)
            update_inventory();
        if (you || vis)
            *known = TRUE;
        break;
    case SCR_CREATE_MONSTER:
    case SPE_CREATE_MONSTER:
        if (create_critters
            (1 + ((confused || sobj->cursed) ? 12 : 0) +
             ((sobj->blessed ||
               rn2(73)) ? 0 : rnd(4)), confused ? &mons[PM_ACID_BLOB] : NULL,
             m_mx(mon), m_my(mon)))
            *known = TRUE;
        /* no need to flush monsters; we ask for identification only if the
           monsters are not visible */
        break;
    case SCR_ENCHANT_WEAPON:
        if (twep && (twep->oclass == WEAPON_CLASS || is_weptool(twep))
            && confused) {
            /* oclass check added 10/25/86 GAN */
            twep->oerodeproof = !(sobj->cursed);
            if (you && Blind) {
                twep->rknown = FALSE;
                pline(msgc_nospoil, "Your weapon feels warm for a moment.");
            } else if (you || vis) {
                twep->rknown = TRUE;
                pline(!you ? msgc_monneutral :
                      sobj->cursed ? msgc_itemloss : msgc_itemrepair,
                      "%s %s covered by a %s %s %s!", your, aobjnam(uwep, "are"),
                      sobj->cursed ? "mottled" : "shimmering",
                      hcolor(sobj->cursed ? "purple" : "golden"),
                      sobj->cursed ? "glow" : "shield");
            } else
                twep->rknown = FALSE; /* equavilent to the you && Blind case */
            if (twep->oerodeproof && (twep->oeroded || twep->oeroded2)) {
                twep->oeroded = uwep->oeroded2 = 0;
                if (you || vis)
                    pline_implied(you ? msgc_itemrepair : msgc_monneutral,
                                  "%s %s as good as new!", your,
                                  aobjnam(twep, Blind ? "feel" : "look"));
            }
        } else {
            /* don't bother with a custom RNG here, 6/7 on weapons is much less
               balance-affecting than 4/5 on armour */
            return !chwepon(mon, sobj,
                            sobj->cursed ? -1 : !twep ? 1 :
                            twep->spe >= 9 ? (rn2(twep->spe) == 0) :
                            sobj->blessed ? rnd(3 - twep->spe / 3) : 1);
        }
        break;
    case SCR_TAMING:
    case SPE_CHARM_MONSTER:
        if (you && Engulfed) {
            maybe_tame(mon, u.ustuck, sobj);
        } else {
            int i, j, bd = confused ? 5 : 1;
            struct monst *mtmp;
            
            for (i = -bd; i <= bd; i++) {
                for (j = -bd; j <= bd; j++) {
                    if (!isok(m_mx(mon) + i, m_my(mon) + j))
                        continue;
                    if (!you && (m_mx(mon) + i) == u.ux && (m_my(mon) + j) == u.uy)
                        maybe_tame(mon, &youmonst, sobj);
                    else if ((mtmp = m_at(level, m_mx(mon) + i, m_my(mon) + j)) != 0)
                        maybe_tame(mon, mtmp, sobj);
                }
            }
        }
        break;
    case SCR_GENOCIDE:
        if (you || vis) {
            pline(you ? msgc_intrgain : msgc_monneutral,
                  "%s found a scroll of genocide!",
                  M_verbs(mon, "have"));
            *known = TRUE;
        }
        if (sobj->blessed)
            do_class_genocide(mon);
        else
            do_genocide(mon, (!sobj->cursed) | (2 * confused),
                        (sobj->cursed && sobj->mbknown));
        break;
    case SCR_LIGHT:
        if (!Blind)
            *known = TRUE;
        litroom(mon, !confused && !sobj->cursed, sobj, TRUE);
        break;
    case SCR_TELEPORTATION:
        if (confused || sobj->cursed) {
            mon_level_tele(mon);
            *known = TRUE;
        } else {
            /* In case we land on the same position, don't reveal the scroll's ID */
            int sx = m_mx(mon);
            int sy = m_my(mon);
            if (!mon_tele(mon, !!teleport_control(mon) ||
                          sobj->blessed) || /* "A mysterious force ..." */
                sx != m_mx(mon) || sy != m_my(mon))
                *known = TRUE;
        }
        if (sobj->blessed)
            *known = TRUE; /* since it's controlled */
        break;
    case SCR_GOLD_DETECTION:
        if (confused || sobj->cursed)
            return trap_detect(mon, sobj);
        else
            return gold_detect(mon, sobj, known);
    case SCR_FOOD_DETECTION:
        if (!you) {
            impossible("monster casting detect food?");
            break;
        }
        return food_detect(sobj, known);
    case SCR_IDENTIFY:
    case SPE_IDENTIFY:
        cval = rn2_on_rng(5, you ? rng_id_count : rng_main);
        if (you && Luck > 0 && cval == 1)
            cval++;

        int idpower = P_UNSKILLED;
        if (confused) {
            if (you || vis)
                pline(msgc_substitute,
                      "%s %sself.", M_verbs(mon, "identify"),
                      mhim(mon));
            if (you)
                enlightenment(FALSE);
        }
        else {
            /* TODO: give msgc_itemloss if you lack identifiable objects */
            if (you) {
                if (sobj->oclass == SCROLL_CLASS)
                    pline_implied(!cval ? msgc_youdiscover : msgc_uiprompt,
                                  "This is an identify scroll.");
            } else if (vis)
                pline(msgc_monneutral, "%s is granted an insight!", Monnam(mon));

            if (sobj->otyp == SPE_IDENTIFY)
                idpower = (you ? P_SKILL(P_DIVINATION_SPELL) :
                           mprof(mon, MP_SDIVN));
            else {
                if (sobj->blessed)
                    idpower = P_EXPERT;
                else if (!sobj->cursed)
                    idpower = P_BASIC;
            }
        }

        if (sobj->otyp == SCR_IDENTIFY) {
            if ((you || vis) && !objects[sobj->otyp].oc_name_known)
                more_experienced(0, 10);
            m_useup(mon, sobj);
            if (you || vis)
                makeknown(SCR_IDENTIFY);
        }

        if (mon->minvent && !confused) {
            identify_pack(mon, cval, idpower);
        }
        return 1;

    case SCR_CHARGING:
    case SPE_CHARGING:
        if (confused) {
            if (you || vis)
                pline(you ? msgc_statusheal : msgc_monneutral,
                      "%s %s charged up!", Monnam(mon), mfeel(mon));
            if (mon->pw < mon->pwmax)
                mon->pw = mon->pwmax;
            else
                mon->pw = (mon->pwmax += dice(5, 4));

            /* cure cancellation too */
            set_property(mon, CANCELLED, -2, TRUE);
            break;
        }
        if (you && !spell) {
            pline(msgc_uiprompt, "This is a charging scroll.");
            makeknown(SCR_CHARGING);
        }

        cval = sobj->cursed ? -1 : sobj->blessed ? 1 : 0;
        if (!objects[sobj->otyp].oc_name_known)
            more_experienced(0, 10);
        if (you)
            useup(sobj);
        else
            m_useup(mon, sobj);
        if (you) {
            otmp = getobj(all_count, "charge", FALSE);
            if (!otmp)
                return 1;
        } else {
            /* Monster might change his mind on target based on BUC */
            otmp = mon_choose_recharge(mon, mbknown ? cval : 0);
            if (!otmp) /* monster aborted */
                return 1;
        }
        recharge(mon, otmp, cval);
        return 1;

    case SCR_MAGIC_MAPPING:
        if (level->flags.nommap) {
            if (you)
                pline(msgc_statusbad, "Your mind is filled with crazy lines!");
            if (you && Hallucination)
                pline_implied(msgc_statusbad, "Wow!  Modern art.");
            else if (you || vis)
                /* can't use pline_implied in case it is a monster reading */
                pline(you ? msgc_statusbad : msgc_monneutral,
                      "%s %s spins in bewilderment.", your, mbodypart(mon, HEAD));
            inc_timeout(mon, CONFUSION, rnd(30), TRUE);
            break;
        }
        if (!you) {
            /* no real effect for now, monsters has no concept of a known map */
            if (vis)
                pline(msgc_monneutral, "%s is granted an insight!", Monnam(mon));
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
            if (you || vis)
                pline(you ? msgc_statusbad : msgc_monneutral,
                      "%s %s spins as something blocks the spell!",
                      your, mbodypart(mon, HEAD));
            set_property(mon, CONFUSION, rnd(30), TRUE);
            break;
        }
        if (!you) {
            if (vis)
                pline(msgc_monneutral, "%s is granted an insight!", Monnam(mon));
            break;
        }
        pline(msgc_youdiscover, "A map coalesces in your mind!");
        cval = (sobj->cursed && !confused);
        if (cval)
            set_property(&youmonst, CONFUSION, 1, TRUE);
        do_mapping();
        if (cval) {
            set_property(&youmonst, CONFUSION, -2, TRUE);
            pline(msgc_failcurse,
                  "Unfortunately, you can't grasp the details.");
        }
        break;
    case SCR_AMNESIA:
        if (!you) {
            /* forget items */
            for (otmp = mon->minvent; otmp; otmp = otmp->nobj) {
                otmp->mknown = 0;
                otmp->mbknown = 0;
            }

            /* reset strategy */
            mon->mstrategy = 0;

            /* reset spells */
            if (sobj->cursed) {
                mon->mspells = 0;
            }

            if (vis)
                pline(msgc_monneutral, "%s looks forgetful.", Monnam(mon));
            break;
        }
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
        if (you || vis)
            if (!objects[sobj->otyp].oc_name_known)
                more_experienced(0, 10);
        if (you)
            useup(sobj);
        else
            m_useup(mon, sobj);
        if (you || vis)
            makeknown(SCR_FIRE);
        if (confused) {
            if (!you && vis)
                pline(msgc_monneutral, "Oh, look, what a pretty fire!");
            if (immune_to_fire(mon))
                shieldeff(m_mx(mon), m_my(mon));
            if (you) {
                if (!immune_to_fire(mon))
                    pline(msgc_substitute,
                          "The scroll catches fire and you burn your %s%s.",
                          makeplural(body_part(HAND)),
                          resists_fire(&youmonst) ? " a bit" : "");
                else if (!blind(&youmonst))
                    pline(msgc_playerimmune,
                          "Oh, look, what a pretty fire in your %s.",
                          makeplural(body_part(HAND)));
                else
                    pline(msgc_playerimmune,
                          "You feel a pleasant warmth in your %s.",
                          makeplural(body_part(HAND)));
            }
            if (!immune_to_fire(mon)) {
                int dmg = 1;
                if (!resists_fire(mon))
                    dmg++;

                if (you)
                    losehp(dmg, killer_msg(DIED, "a scroll of fire"));
                else {
                    mon->mhp -= dmg;
                    if (mon->mhp <= 0)
                        mondied(mon);
                }
            }
            return 1;
        }
        if (you && Underwater)
            pline(msgc_actionok, "The water around you vaporizes violently!");
        else if (you || vis)
            pline(msgc_actionok, "The scroll erupts in a tower of flame!");
        explode(m_mx(mon), m_my(mon), 11, (2 * (rn1(3, 3) + 2 * cval) + 1) / 3,
                SCROLL_CLASS, EXPL_FIERY, NULL, 0);
        return 1;
    case SCR_EARTH:
        /* TODO: handle steeds */
        if (!Is_rogue_level(m_mz(mon)) &&
            (!In_endgame(m_mz(mon)) || Is_earthlevel(m_mz(mon)))) {
            int x, y;

            /* Identify the scroll */
            if (you || vis || cansee(mon->mx, mon->my)) {
                pline(you ? msgc_actionok : msgc_monneutral,
                      "The %s rumbles %s %s!", ceiling(m_mx(mon), m_my(mon)),
                      sobj->blessed ? "around" : "above",
                      you ? "you" : (vis ? mon_nam(mon) : "something unseen!"));
                if (you || vis)
                    *known = TRUE;
                else if (invisible(mon))
                    map_invisible(mon->mx, mon->my);
            }
            if (you && In_sokoban(&u.uz))
                change_luck(-1);        /* Sokoban guilt */

            boolean hityou = FALSE;
            /* Loop through the surrounding squares */
            for (x = m_mx(mon) - 1; x <= m_mx(mon) + 1; x++) {
                for (y = m_my(mon) - 1; y <= m_my(mon) + 1; y++) {
                    /* Is this a suitable spot? */
                    if (!isok(x, y) || closed_door(level, x, y) ||
                        IS_ROCK(level->locations[x][y].typ) ||
                        IS_AIR(level->locations[x][y].typ))
                        continue;

                    /* Is this a valid spot to attack? */
                    if (x == m_mx(mon) && y == m_my(mon)) {
                        if (sobj->blessed)
                            continue;
                    } else {
                        if (sobj->cursed)
                            continue;
                    }

                    /* If the spot is the player, postpone it for later to avoid
                       bones oddities */
                    if (x == u.ux && y == u.uy) {
                        hityou = TRUE;
                        continue;
                    }

                    /* Make the object(s) */
                    do_earth(m_dlevel(mon), x, y, confused, mon);
                }
            }

            if (hityou)
                do_earth(m_dlevel(mon), u.ux, u.uy, confused, mon);
        }
        break;
    case SCR_PUNISHMENT:
        *known = TRUE;
        if (confused || sobj->blessed) {
            pline(msgc_failcurse, "%s guilty.", M_verbs(mon, "feel"));
            break;
        } else if (!you) {
            /* Yeah, this is a bit unfair, but I don't feel like implementing ball&chains for monsters.
               Alternative suggestions welcome. Divine punishment perhaps? */
            if (vis)
                pline(msgc_monneutral, "%s looks very guilty!", Monnam(mon));
            break;
        }
        punish(sobj);
        break;
    case SCR_STINKING_CLOUD:
        if (you || vis) {
            pline(you ? msgc_uiprompt : msgc_monneutral,
                  "%s found a scroll of stinking cloud!", M_verbs(mon, "have"));
            *known = TRUE;
        }
        coord cc;

        cc.x = m_mx(mon);
        cc.y = m_my(mon);

        if (you) {
            pline(msgc_uiprompt, "Where do you want to center the cloud?");
            if (getpos(&cc, TRUE, "the desired position", FALSE) ==
                NHCR_CLIENT_CANCEL) {
                pline(msgc_cancelled, "Never mind.");
                return 0;
            }
        } else {
            struct musable m;
            init_musable(mon, &m);
            if (!mon_choose_spectarget(&m, sobj, &cc))
                return 0;
        }

        boolean valid_target = (dist2(cc.x, cc.y, m_mx(mon), m_my(mon)) <= 32 &&
                                m_cansee(mon, cc.x, cc.y));
        if (!cansee(cc.x, cc.y) || !valid_target)
            pline(msgc_yafm, "You smell rotten eggs.");
        if (valid_target)
            create_gas_cloud(level, cc.x, cc.y, 3 + bcsign(sobj),
                             8 + 4 * bcsign(sobj));
        break;
    default:
        impossible("What weird effect is this? (%u)", sobj->otyp);
    }
    return 0;
}

static void
wand_explode(struct monst *mon, struct obj *obj)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    obj->in_use = TRUE; /* in case losehp() is fatal */
    if (you || vis)
        pline(you ? msgc_itemloss : msgc_monneutral,
              "%s %s vibrates violently, and explodes!",
              you ? "Your" : s_suffix(Monnam(mon)), xname(obj));
    if (you)
        losehp(rnd(2 * ((Upolyd ? u.mhmax : u.uhpmax) + 1) / 3),
               killer_msg(DIED, "an exploding wand"));
    else {
        m_useup(mon, obj);
        mon->mhp -= rnd(2 * (mon->mhpmax + 1) / 3);
        if (mon->mhp <= 0)
            mondied(mon);
    }
    if (you) {
        useup(obj);
        exercise(A_STR, FALSE);
    }
}

/* Low-level lit-field update routine. */
static void
set_lit(int x, int y, void *val)
{
    struct monst *mtmp;
    struct obj *otmp;
    if (val)
        level->locations[x][y].lit = 1;
    else {
        level->locations[x][y].lit = 0;

        /* kill light sources on the ground */
        for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
            snuff_lit(otmp);

        /* kill light sources in inventories of monsters */
        mtmp = m_at(level, x, y);
        if (!mtmp && x == u.ux && y == u.uy)
            mtmp = &youmonst;
        if (mtmp)
            for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
                snuff_lit(otmp);
    }
}

void
litroom(struct monst *mon, boolean on, struct obj *obj, boolean tell)
{
    char is_lit;        /* value is irrelevant; we use its address as a `not
                           null' flag for set_lit() */
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    boolean do_ball = FALSE; /* ball&chain updating */
    int wandlevel = 0;
    if (obj && obj->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mon, obj);
    int lightradius = 5;
    if (obj && obj->oclass == SCROLL_CLASS && obj->blessed)
        lightradius = 9;
    else if (wandlevel)
        lightradius = (wandlevel == P_UNSKILLED ? 3  :
                       wandlevel == P_BASIC     ? 5  :
                       wandlevel == P_SKILLED   ? 9  :
                       wandlevel == P_EXPERT    ? 15 :
                       wandlevel == P_MASTER    ? -1 :
                       1);

    /* first produce the text (provided you're not blind) */
    if (!on) {
        struct obj *otmp;

        if (you && Engulfed) {
            /* Since engulfing will prevent set_lit(), douse lamps/etc here as well */
            for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj)
                snuff_lit(otmp);
            if (tell)
                pline(msgc_yafm, "It seems even darker in here than before.");
            return;
        }

        if (!blind(&youmonst) && (you || vis)) {
            struct obj *wep = m_mwep(mon);
            if (wep && artifact_light(wep) && wep->lamplit && tell)
                pline(msgc_substitute,
                      "Suddenly, the only light left comes from %s!",
                      the(xname(wep)));
            else if (tell)
                pline(msgc_failcurse,
                      "%s %s surrounded by darkness!", you ? "You" : Monnam(mon),
                      you ? "are" : "is");
        }
    } else {
        if (you && Engulfed) {
            if (!blind(&youmonst) && tell) {
                if (is_animal(u.ustuck->data))
                    pline(msgc_yafm, "%s %s is lit.", s_suffix(Monnam(u.ustuck)),
                          mbodypart(u.ustuck, STOMACH));
                else if (is_whirly(u.ustuck->data))
                    pline(msgc_yafm, "%s shines briefly.", Monnam(u.ustuck));
                else
                    pline(msgc_yafm, "%s glistens.", Monnam(u.ustuck));
            }
            return;
        }

        if (((!blind(&youmonst) && you) || vis) && tell)
            pline(you ? msgc_actionok : msgc_monneutral,
                  "A lit field surrounds %s!",
                  you ? "you" : mon_nam(mon));
    }

    /* No-op in water - can only see the adjacent squares and that's it! */
    if ((you && Underwater) || Is_waterlevel(&u.uz))
        return;

    /* If we are darkening the room and the hero is punished but not
       blind, then we have to pick up and replace the ball and chain so
       that we don't remember them if they are out of sight. */
    if (Punished && !on && !blind(&youmonst) && (you || mon->dlevel == level)) {
        if (lightradius == -1 ||
            dist2(m_mx(mon), m_my(mon), uball->ox, uball->oy) <= lightradius ||
            dist2(m_mx(mon), m_my(mon), uchain->ox, uchain->oy) <= lightradius) {
            do_ball = TRUE; /* for later */
            move_bc(1, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);
        }
    }

    if (Is_rogue_level(m_mz(mon))) {
        /* Can't use do_clear_area because MAX_RADIUS is too small */
        /* rogue lighting must light the entire room */
        int rnum = level->locations[m_mx(mon)][m_my(mon)].roomno - ROOMOFFSET;
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
    } else {
        if (lightradius == -1) { /* entire floor */
            int x, y;

            for (x = 0; x < COLNO; x++)
                for (y = 0; y < ROWNO; y++)
                    set_lit(x, y, (on ? &is_lit : NULL));
        } else
            do_clear_area(m_mx(mon), m_my(mon), lightradius, set_lit,
                          (on ? &is_lit : NULL));
    }

    /* If we are not blind, then force a redraw on all positions in sight.
       The vision recalculation will correctly update all previously seen
       positions *and* correctly set the waslit bit [could be messed up
       from above]. Do this even if hero wasn't the user of the spell
       unconditionally, it doesn't hurt if redundant, and checking if a
       square in LOS would be affected would result in needlessy
       complicated code. */
    if (!blind(&youmonst)) {
        vision_recalc(2);

        /* replace ball&chain */
        if (do_ball)
            move_bc(0, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);
    }

    turnstate.vision_full_recalc = TRUE; /* delayed vision recalculation */
}


static void
do_class_genocide(struct monst *mon)
{
    int i, j, immunecnt, gonecnt, goodcnt, class, feel_dead = 0;
    const char *buf;
    const char mimic_buf[] = {def_monsyms[S_MIMIC], '\0'};
    boolean gameover = FALSE;   /* true iff killed self */
    if (mon != &youmonst && mon->mtame && canseemon(mon)) {
        pline(msgc_intrgain, "%s looks at you curiously.", Monnam(mon));
        mon = &youmonst; /* redirect genocide selection to player */
    }
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);

    for (j = 0;; j++) {
        if (j >= 5) {
            if (you)
                pline(msgc_cancelled, "That's enough tries!");
            else if (vis)
                pline(msgc_monneutral, "%s failed to genocide anything...",
                      Monnam(mon));
            return;
        }

        if (you) {
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
        } else {
            /* in case the below decides to read buf for whatever reason... */
            buf = "f"; /* unlikely choice of geno target, so if something goes wrong, it
                          would be noticed rather quickly */
            class = mon_choose_genocide(mon, TRUE, j);
            if (!class) { /* decided not to genocide anything */
                if (vis) {
                    if (!j) /* didn't even try */
                        pline(msgc_monneutral,
                              "%s decided not to genocide anything.", Monnam(mon));
                    else
                        pline(msgc_monneutral,
                              "%s gave up on trying to genocide something.", Monnam(mon));
                }
                return;
            }
        }

        /* class will never be 0 for monsters performing genocide */
        immunecnt = gonecnt = goodcnt = 0;
        for (i = LOW_PM; i < NUMMONS; i++) {
            if (you && class == 0 && strstri(monexplain[(int)mons[i].mlet], buf) != 0)
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
        /* TODO[?]: If user's input doesn't match any class description,
           check individual species names. */
        if (!goodcnt && ((you && class != mons[urole.num].mlet &&
                          class != mons[urace.num].mlet) ||
                         (!you && class != mon->data->mlet))) {
            if (gonecnt) {
                if (you)
                    pline(msgc_cancelled,
                          "All such monsters are already nonexistent.");
                else
                    impossible("monster tried to genocide genocided species?");
            } else if (immunecnt || (you && buf[0] == DEF_INVISIBLE && buf[1] == '\0')) {
                if (you)
                    pline(msgc_cancelled,
                          "You aren't permitted to genocide such monsters.");
                else if (vis)
                    pline(msgc_monneutral,
                          "%s tried to genocide something immune to genocide.", Monnam(mon));
            } else if (wizard && you && buf[0] == '*') {
                pline(msgc_debug,
                      "Blessed genocide of '*' is deprecated. Use #levelcide "
                      "for the same result.\n");
                do_level_genocide();
                return;
            } else if (you)
                pline(msgc_cancelled, "That symbol does not represent any monster.");
            else
                impossible("monster tried to genocide invalid class?");
            continue;
        }

        for (i = LOW_PM; i < NUMMONS; i++) {
            if (mons[i].mlet == class) {
                const char *nam = makeplural(mons[i].mname);

                /* Although "genus" is Latin for race, the hero benefits from
                   both race and role; thus genocide affects either. */
                if ((you && (Your_Own_Role(i) || Your_Own_Race(i))) ||
                    (!you && monsndx(mon->data) == i) ||
                    ((mons[i].geno & G_GENO)
                     && !(mvitals[i].mvflags & G_GENOD))) {
                    /* This check must be first since player monsters might
                       have G_GENOD or !G_GENO. */
                    mvitals[i].mvflags |= (G_GENOD | G_NOCORPSE);
                    kill_genocided_monsters();
                    update_inventory(); /* eggs & tins */
                    if (you)
                        break_conduct(conduct_genocide);
                    pline(you ? msgc_intrgain : msgc_intrloss,
                          "Wiped out all %s.", nam);
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
                    /* Self-genocide if it matches either your race or role. */
                    if (i == urole.num || i == urace.num) {
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
                              "%s %s permitted to genocide %s%s.",
                              you ? "You" : Monnam(mon),
                              you ? "aren't" : "isn't",
                              (uniq && !named) ? "the " : "",
                              (uniq || named) ? mons[i].mname : nam);
                    }
                }
            }
        }
        if (gameover || u.uhp == -1) {
            const char *kbuf;
            if (you)
                kbuf = "a blessed scroll of genocide";
            else
                kbuf = msgprintf("a blessed genocide by %s", k_monnam(mon));
            (gameover ? done : set_delayed_killer)(
                GENOCIDED, killer_msg(GENOCIDED, kbuf));
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
/* known_cursed is if a monster is fully aware that it is doing a revgeno */
void
do_genocide(struct monst *mon, int how, boolean known_cursed)
{
    const char *buf;
    int i, killplayer = 0;
    int mndx;
    const struct permonst *ptr;
    const char *which;
    if (mon->mtame && canseemon(mon)) {
        pline(msgc_intrgain, "%s looks at you curiously.", Monnam(mon));
        mon = &youmonst;
    }
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    struct monst *mtmp;
    int mtmp_vis = 0;

    if (how & PLAYER) {
        if (you)
            mndx = u.umonster;      /* non-polymorphed mon num */
        else
            mndx = monsndx(mon->data);
        ptr = &mons[mndx];
        buf = msg_from_string(ptr->mname);
        if (you || mon->data == youmonst.data || Your_Own_Role(monsndx(mon->data)) ||
            Your_Own_Race(monsndx(mon->data)))
            killplayer++;
    } else {
        for (i = 0;; i++) {
            if (i >= 5) {
                if (you)
                    pline(msgc_cancelled, "That's enough tries!");
                else if (vis)
                    pline(msgc_monneutral,
                          "%s failed to genocide anything...", Monnam(mon));
                if (!(how & REALLY)) {
                    if (!you && vis)
                        pline(msgc_substitute, "But wait...");
                    ptr = rndmonst(&u.uz, rng_main);
                    if (!ptr)
                        return; /* no message, like normal case */
                    mndx = monsndx(ptr);
                    break;      /* remaining checks don't apply */
                } else
                    return;
            }
            if (you) {
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
            } else {
                buf = "kitten"; /* fallback if something goes wrong */
                if (known_cursed)
                    mndx = mon_choose_reverse_genocide(mon);
                else
                    mndx = mon_choose_genocide(mon, FALSE, i);
                if (!mndx) {
                    if (vis) {
                        if (!i)
                            pline(msgc_monneutral,
                                  "%s decided not to genocide anything.", Monnam(mon));
                        else
                            pline(msgc_monneutral,
                                  "%s gave up on trying to genocide something.", Monnam(mon));
                    }
                    if (!(how & REALLY)) {
                        ptr = rndmonst(&u.uz, rng_main);
                        if (!ptr)
                            return; /* no message, like normal case */
                        if (vis)
                            pline(msgc_substitute, "But wait...");
                        mndx = monsndx(ptr);
                        break;      /* remaining checks don't apply */
                    } else
                        return;
                }
            }
            if (mndx == NON_PM || (mvitals[mndx].mvflags & G_GENOD)) {
                if (you)
                    pline(msgc_cancelled,
                          "Such creatures %s exist in this world.",
                          (mndx == NON_PM) ? "do not" : "no longer");
                else
                    impossible("monster selected an invalid/genocided target?");
                continue;
            }
            ptr = &mons[mndx];

            if (!(ptr->geno & G_GENO) &&
                !((you && (Your_Own_Role(mndx) || Your_Own_Race(mndx))) ||
                  ptr == mon->data)) {
                if (canhear()) {
                    /* fixme: unconditional "caverns" will be silly in some
                       circumstances */

                    pline(msgc_npcvoice,
                          "A thunderous voice booms through the caverns:");
                    verbalize(you ? msgc_hint : msgc_monneutral,
                              "No, %s!  That will not be done.",
                              you ? mortal_or_creature(youmonst.data, TRUE) : "creature");
                }
                continue;
            }

            /* Although "genus" is Latin for race, the hero benefits from both
               race and role; thus genocide affects either. */
            if (Your_Own_Role(mndx) || Your_Own_Race(mndx) || ptr == youmonst.data) {
                killplayer++;
                break;
            }

            if (you && is_human(ptr))
                adjalign(-sgn(u.ualign.type));
            if (you && is_demon(ptr))
                adjalign(sgn(u.ualign.type));
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

        /* Do this before killing the player, just in case */
        kill_genocided_monsters();

        if (killplayer) {
            const char *killer;
            if (!you)
                killer = killer_msg(GENOCIDED,
                                    "a monster's scroll of genocide");
            else if (how & PLAYER)
                killer = killer_msg(GENOCIDED, "genocidal confusion");
            else if (how & ONTHRONE)
                /* player selected while on a throne */
                killer = killer_msg(GENOCIDED, "an imperious order");
            else if (you)   /* selected player deliberately, not confused */
                killer = killer_msg(GENOCIDED, "a scroll of genocide");

            /* Polymorphed characters will die as soon as they're rehumanized.
               KMH -- Unchanging prevents rehumanization */
            if (Upolyd && ptr != youmonst.data) {
                pline(msgc_fatal, "You feel dead inside.");
                set_delayed_killer(GENOCIDED, killer);
            } else
                done(GENOCIDED, killer);
        }

        /* While endgame messages track whether you genocided
           by means other than looking at u.uconduct, call
           break_conduct anyway to correctly note the first turn
           in which it happened. */
        if (you)
            break_conduct(conduct_genocide);
        update_inventory();     /* in case identified eggs were affected */
    } else {
        int cnt = 0;

        if (!(mons[mndx].geno & G_UNIQ) &&
            !(mvitals[mndx].mvflags & (G_GENOD | G_EXTINCT)))
            for (i = rn1(3, 4); i > 0; i--) {
                if (!(mtmp = makemon(ptr, level, m_mx(mon), m_my(mon),
                                     NO_MINVENT | MM_ADJACENTOK)))
                    break;      /* couldn't make one */
                if (canseemon(mtmp))
                    mtmp_vis++;
                ++cnt;
                if (mvitals[mndx].mvflags & G_EXTINCT)
                    break;      /* just made last one */
            }
        if (cnt && mtmp_vis)
            pline(msgc_substitute,
                  "Sent in %s %s.", mtmp_vis == 1 ? "a" : "some",
                  mtmp_vis == 1 ? buf : makeplural(buf));
        else if (cnt && (you || vis))
            pline(you ? msgc_substitute : msgc_monneutral,
                  "Nothing appears to happen.");
        else if (you || vis)
            pline(you ? msgc_noconsequence : msgc_monneutral,
                  "Nothing happens.");
    }
}

/* Returns a mndx for the monster to REVERSE genocide (cursed scroll) */
static int
mon_choose_reverse_genocide(struct monst *mon)
{
    /* if the monster is hostile, revgeno nasty things */
    if (!mon->mpeaceful) {
        int i;
        int nasty;
        for (i = 0; i < 10; i++) { /* try to pick a suitable nasty 10 times */
            nasty = pick_nasty();
            if (mvitals[nasty].mvflags & G_GENOD) /* genocided, try again */
                continue;
            /* we found a suitable nasty, return it */
            return nasty;
        }
        /* found no suitable nasty, return self */
        return monsndx(mon->data);
    }

    /* peacefuls/tame should not do this */
    impossible("peaceful monster doing a deliberate reverse-genocide?");
    return monsndx(mon->data); /* just return self */
}

/* Returns a class on class genocide, a mndx otherwise */
static int
mon_choose_genocide(struct monst *mon, boolean class, int cur_try)
{
    int try = 0;
    int mndx[5] = {0}; /* zerofill in case we return early */
    if (mon == &youmonst) {
        impossible("mon_choose_genocide called with &youmonst");
        return 0;
    }
    /* Monsters has no idea what they can and can't genocide.
       However, after trying and failing to genocide a target, it will
       try the next monster in line (the purpose of 'cur_try').
       Monsters will decide on genocide target in this order:
       - Grudges (except for purple worms -> shriekers)
       - Monsters hostile to it in view
       - Monsters hostile they can sense
       Monsters will skip already genocided targets.
       Note: a monster will never be able to genocide your role, because
       for consistency with you vs other player roles, that is invalid.
       (In the same manner, a monster can always genocide itself despite
       restrictions, but that will never happen unless confused) */
    int i;
    for (i = LOW_PM; i < NUMMONS; i++) {
        if (i == monsndx(mon->data))
            continue;
        if (class && mons[i].mlet == mon->data->mlet)
            continue;
        if (grudge(mon->data, &mons[i]) && !(mvitals[i].mvflags & G_GENOD) &&
            (mon->data != &mons[PM_PURPLE_WORM] || i != PM_SHRIEKER))
            mndx[try++] = i;
        if (try > 4)
            return maybe_target_class(class, mndx[cur_try]);
    }

    /* Peacefuls monsters never have anything hostile around */
    if (mon->mpeaceful && !mon->mtame)
        return maybe_target_class(class, mndx[cur_try]);

    /* hostile monsters it can see */
    struct monst *mtmp;
    for (mtmp = monlist(mon->dlevel); mtmp; mtmp = monnext(mtmp)) {
        /* do not genocide own kind */
        if (monsndx(mon->data) == monsndx(mtmp->data))
            continue;
        if (class && mon->data->mlet == mtmp->data->mlet)
            continue;
        if (mm_aggression(mon, mtmp) && (msensem(mon, mtmp) & MSENSE_ANYVISION))
            mndx[try++] = monsndx(mtmp->data);
        if (try > 4)
            return maybe_target_class(class, mndx[cur_try]);
    }

    /* hostile monsters it can sense */
    for (mtmp = monlist(mon->dlevel); mtmp; mtmp = monnext(mtmp)) {
        if (monsndx(mon->data) == monsndx(mtmp->data))
            continue;
        if (class && mon->data->mlet == mtmp->data->mlet)
            continue;
        /* not sensed only by warning, because that doesn't tell the mlet */
        if (mm_aggression(mon, mtmp) && (msensem(mon, mtmp) &
                                         (~MSENSE_ANYVISION & ~MSENSE_WARNING)))
            mndx[try++] = monsndx(mtmp->data);
        if (try > 4)
            return maybe_target_class(class, mndx[cur_try]);
    }

    return maybe_target_class(class, mndx[cur_try]);
}

/* Helper function for converting PM_* into class for blessed genocides */
static int
maybe_target_class(boolean class, int mndx)
{
    if (class)
        return mons[mndx].mlet;
    return mndx;
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
        || *mtype == PM_ALIGNED_PRIEST || *mtype == PM_HIGH_PRIEST) {
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
        which = urole.num;  /* an arbitrary index into mons[] */
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

        int l;
        for (;;) {
            l = 0;
            /* allow the initial disposition to be specified */
            if (!strncmpi(bufp, "tame ", l = 5))
                maketame = TRUE;
            else if (!strncmpi(bufp, "peaceful ", l = 9))
                makepeaceful = TRUE;
            else if (!strncmpi(bufp, "hostile ", l = 8))
                makehostile = TRUE;
            else if (!strncmpi(bufp, "cancelled ", l = 10) ||
                     !strncmpi(bufp, "canceled ", l = 9))
                cancelled = TRUE;
            else if (!strncmpi(bufp, "fast ", l = 5))
                fast = TRUE;
            else if (!strncmpi(bufp, "slow ", l = 5))
                slow = TRUE;
            else if (!strncmpi(bufp, "revived ", l = 8))
                revived = TRUE;
            else if (!strncmpi(bufp, "fleeing ", l = 8))
                fleeing = TRUE;
            else if (!strncmpi(bufp, "blind ", l = 6) ||
                     !strncmpi(bufp, "blinded ", l = 8))
                blind = TRUE;
            else if (!strncmpi(bufp, "paralyzed ", l = 10))
                paralyzed = TRUE;
            else if (!strncmpi(bufp, "sleeping ", l = 9))
                sleeping = TRUE;
            else if (!strncmpi(bufp, "stunned ", l = 8))
                stunned = TRUE;
            else if (!strncmpi(bufp, "confused ", l = 9))
                confused = TRUE;
            else if (!strncmpi(bufp, "mavenge ", l = 8))
                mavenge = TRUE;
            else
                break;
            bufp += l;
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
                    set_property(mtmp, CANCELLED, 0, FALSE);
                if (fast)
                    set_property(mtmp, FAST, 0, FALSE);
                if (slow)
                    set_property(mtmp, SLOW, 65535, FALSE);
                if (revived)
                    mtmp->mrevived = 1;
                if (fleeing)
                    mtmp->mflee = 1;
                if (blind)
                    set_property(mtmp, BLINDED, 100, FALSE);
                if (paralyzed)
                    mtmp->mcanmove = 0;
                if (sleeping)
                    mtmp->msleeping = 1;
                if (stunned)
                    set_property(mtmp, STUNNED, 100, FALSE);
                if (confused)
                    set_property(mtmp, CONFUSION, 100, FALSE);
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
