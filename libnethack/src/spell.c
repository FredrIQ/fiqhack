/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-09-21 */
/* Copyright (c) M. Stephenson 1988                               */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* spellmenu arguments; 0 thru n-1 used as spl_book[] index when swapping */
#define SPELLMENU_CAST (-2)
#define SPELLMENU_VIEW (-1)

#define KEEN 20000
#define MAX_SPELL_STUDY 3
#define incrnknow(spell)        spl_book[spell].sp_know = KEEN

#define spellev(spell)          spl_book[spell].sp_lev

static const char *
spellname(int spell)
{
    int sid = spellid(spell);
    if (SPELL_IS_FROM_SPELLBOOK(spell))
        return OBJ_NAME(objects[sid]);
    else if (sid == SPID_PRAY)
        return "pray for help";
    else if (sid == SPID_TURN)
        return "turn undead";
    else if (sid == SPID_RLOC)
        return "teleportitis";
    else if (sid == SPID_JUMP)
        return "supernatural jump"; /* distinguish from the spell */
    else if (sid == SPID_MONS) {
        struct polyform_ability pa;
        if (has_polyform_ability(youmonst.data, &pa))
            return pa.description;
        panic("SPID_MONS is in spellbook, but #monster is invalid");
    }
    panic("Unknown spell number %d", sid);
}

#define spelllet_from_no(spell)                                 \
    ((char)((spell < 26) ? ('a' + spell) : ('A' + spell - 26)))
#define spellno_from_let(slet)                                  \
    (((slet) >= 'a' && (slet) <= 'z') ? slet - 'a' :            \
     ((slet) >= 'A' && (slet) <= 'Z') ? slet - 'A' + 26 : -1)

static boolean cursed_book(struct monst *, struct obj *bp);
static boolean confused_book(struct monst *, struct obj *);
static int learn(void);
static boolean getspell(int *);
static boolean dospellmenu(const char *, int, int *);
static int percent_success(const struct monst *, int);
static int throwspell(boolean, schar *dx, schar *dy, const struct nh_cmd_arg *arg);
static void cast_protection(struct monst *);
static void spell_backfire(int);

static int mspell_skilltype(int);
static const char *spelltypemnemonic(int);

/* The roles[] table lists the role-specific values for tuning
 * percent_success().
 *
 * Reasoning:
 *   spelbase, spelheal:
 *      Arc are aware of magic through historical research
 *      Bar abhor magic (Conan finds it "interferes with his animal instincts")
 *      Cav are ignorant to magic
 *      Hea are very aware of healing magic through medical research
 *      Kni are moderately aware of healing from Paladin training
 *      Mon use magic to attack and defend in lieu of weapons and armor
 *      Pri are very aware of healing magic through theological research
 *      Ran avoid magic, preferring to fight unseen and unheard
 *      Rog are moderately aware of magic through trickery
 *      Sam have limited magical awareness, prefering meditation to conjuring
 *      Tou are aware of magic from all the great films they have seen
 *      Val have limited magical awareness, prefering fighting
 *      Wiz are trained mages
 *
 *      The arms penalty is lessened for trained fighters Bar, Kni, Ran,
 *      Sam, Val -
 *      the penalty is its metal interference, not encumbrance.
 *      The `spelspec' is a single spell which is fundamentally easier
 *       for that role to cast.
 *
 *  spelspec, spelsbon:
 *      Arc map masters (SPE_MAGIC_MAPPING)
 *      Bar fugue/berserker (SPE_HASTE_SELF)
 *      Cav born to dig (SPE_DIG)
 *      Hea to heal (SPE_CURE_SICKNESS)
 *      Kni to turn back evil (SPE_TURN_UNDEAD)
 *      Mon to preserve their abilities (SPE_RESTORE_ABILITY)
 *      Pri to bless (SPE_REMOVE_CURSE)
 *      Ran to hide (SPE_INVISIBILITY)
 *      Rog to find loot (SPE_DETECT_TREASURE)
 *      Sam to be At One (SPE_CLAIRVOYANCE)
 *      Tou to smile (SPE_CHARM_MONSTER)
 *      Val control the cold (SPE_CONE_OF_COLD)
 *      Wiz all really, but SPE_MAGIC_MISSILE is their party trick
 *
 *      See percent_success() below for more comments.
 *
 *  uarmbon, uarmsbon, uarmhbon, uarmgbon, uarmfbon:
 *      Fighters find body armor & shield a little less limiting.
 *      Headgear, Gauntlets and Footwear are not role-specific (but
 *      still have an effect, except helm of brilliance, which is designed
 *      to permit magic-use).
 */

#define uarmhbon 4      /* Metal helmets interfere with the mind */
#define uarmgbon 6      /* Casting channels through the hands */
#define uarmfbon 2      /* All metal interferes to some degree */

/* since the spellbook itself doesn't blow up, don't say just "explodes" */
static const char explodes[] = "radiates explosive energy";

/* TRUE: book should be destroyed by caller */
static boolean
cursed_book(struct monst *mon, struct obj *bp)
{
    int lev = objects[bp->otyp].oc_level;
    boolean was_inuse;
    boolean you = (mon == &youmonst);

    switch (rn2(lev)) {
    case 0:
        if (you) {
            pline("You feel a wrenching sensation.");
            tele(); /* teleport him */
        } else
            mon_tele(mon, !!teleport_control(mon));
        break;
    case 1:
        if (you) {
            pline("You feel threatened.");
            aggravate();
        } else
            you_aggravate(mon);
        break;
    case 2:
        set_property(mon, BLINDED, rn1(100, 250), FALSE);
        break;
    case 3:
        /* TODO: make a take_gold() for monsters */
        if (you)
            take_gold();
        break;
    case 4:
        if (you)
            pline("These runes were just too much to comprehend.");
        set_property(mon, CONFUSION, rn1(7, 16), you); /* show msg if !you */
        break;
    case 5:
        if (you)
            pline("The book was coated with contact poison!");
        if (which_armor(mon, os_armg)) {
            erode_obj(which_armor(mon, os_armg), "gloves",
                      ERODE_CORRODE, TRUE, TRUE);
            break;
        }
        /* Temporarily disable in_use; death should not destroy the book.

           Paranoia: ensure that we don't turn /on/ in_use, that causes a
           desync. (That used to be able to happen via an old codepath, but that
           codepath has since been removed. I don't think there are others, but
           someone might unintentionally add one.) */
        was_inuse = bp->in_use;
        bp->in_use = FALSE;
        if (you) {
            losestr(Poison_resistance ? rn1(2, 1) : rn1(4, 3), DIED,
                    killer_msg(DIED, "a contact-poisoned spellbook"), NULL);
            losehp(rnd(Poison_resistance ? 6 : 10),
                   killer_msg(DIED, "a contact-poisoned spellbook"));
        } else {
            /* do a bit more damage since monsters can't lose str */
            mon->mhp -= resists_poison(mon) ? 10 : 15;
            if (mon->mhp < 1)
                mondied(mon);
        }
        bp->in_use = was_inuse;
        break;
    case 6:
        if (resists_magm(mon)) {
            shieldeff(u.ux, u.uy);
            if (you)
                pline("The book %s, but you are unharmed!", explodes);
            else if (canseemon(mon))
                pline("The book %s, but %s is unharmed!", explodes, mon_nam(mon));
        } else {
            if (you)
                pline("As you read the book, it %s in your %s!", explodes,
                      body_part(FACE));
            else if (canseemon(mon))
                pline("As %s reads the book, it %s in %s %s!", mon_nam(mon),
                      explodes, mhis(mon), mbodypart(mon, FACE));
            if (you)
                losehp(2 * rnd(10) + 5, killer_msg(DIED, "an exploding rune"));
            else {
                mon->mhp -= 2 * rnd(10) + 5;
                if (mon->mhp < 1)
                    mondied(mon);
            }
        }
        return TRUE;
    default:
        if (you)
            rndcurse();
        else
            mrndcurse(mon);
        break;
    }
    return FALSE;
}

/* study while confused: returns TRUE if the book is destroyed */
static boolean
confused_book(struct monst *mon, struct obj *spellbook)
{
    boolean gone = FALSE;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);

    if (!rn2(3) && spellbook->otyp != SPE_BOOK_OF_THE_DEAD) {
        spellbook->in_use = TRUE;       /* in case called from learn */
        if (you || vis)
            pline("Being confused, %s %s difficulties in controlling %s "
                  "actions.", you ? "you" : mon_nam(mon),
                  you ? "have" : "has",
                  you ? "your" : mhis(mon));
        win_pause_output(P_MESSAGE);
        if (you || vis)
            pline("%s accidentally tear%s the spellbook to pieces!",
                  you ? "You" : Monnam(mon), you ? "" : "s");
        if ((you || vis) &&
            !objects[spellbook->otyp].oc_name_known &&
            !objects[spellbook->otyp].oc_uname)
            docall(spellbook);
        if (you)
            useup(spellbook);
        else
            m_useup(mon, spellbook);
        gone = TRUE;
    } else if (you) {
        pline("You find yourself reading the %s line over and over again.",
              spellbook == u.utracked[tos_book] ? "next" : "first");
    } else if (vis)
        pline("%s seems lost in %s reading.", Monnam(mon), mhis(mon));
    return gone;
}

/* special effects for The Book of the Dead */
void
deadbook(struct obj *book2, boolean invoked)
{
    struct monst *mtmp, *mtmp2;
    coord mm;

    if (!invoked)
        pline("You turn the pages of the Book of the Dead...");
    makeknown(SPE_BOOK_OF_THE_DEAD);
    /* KMH -- Need ->known to avoid "_a_ Book of the Dead" */
    book2->known = 1;
    if (invocation_pos(&u.uz, u.ux, u.uy) && !On_stairs(u.ux, u.uy)) {
        struct obj *otmp;
        boolean arti1_primed = FALSE, arti2_primed = FALSE, arti_cursed = FALSE;

        if (invoked) {
            if (Blind)
                You_hear("a crisp flicker...");
            else
                pline("The Book of the Dead opens of its own accord...");
        }

        if (book2->cursed) {
            if (invoked) {
                if (Hallucination)
                    You_hear("gratuitous bleeping.");
                else
                    You_hear("a mumbled curse.");
            } else
                pline("The runes appear scrambled.  You can't read them!");
            return;
        }

        if (!Uhave_bell || !Uhave_menorah) {
            pline("A chill runs down your %s.", body_part(SPINE));
            if (!Uhave_bell) {
                if (Hallucination)
                    pline("You feel like a tuning fork!");
                else
                    You_hear("a faint chime...");
            }
            if (!Uhave_menorah) {
                if (Hallucination) {
                    pline("Nosferatu giggles.");
                } else if (mvitals[PM_DOPPELGANGER].mvflags & G_GENOD) {
                    /* suggestion by b_jonas: can't talk about doppelgangers
                       if they don't exist */
                    if (Uhave_bell)
                        pline("Nothing seems to happen.");
                    /* otherwise no message, we already printed one. */
                } else {
                    pline("Vlad's doppelganger is amused.");
                }
            }
            return;
        }

        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (otmp->otyp == CANDELABRUM_OF_INVOCATION && otmp->spe == 7 &&
                otmp->lamplit) {
                if (!otmp->cursed)
                    arti1_primed = TRUE;
                else
                    arti_cursed = TRUE;
            }
            if (otmp->otyp == BELL_OF_OPENING && (moves - otmp->age) < 5L) {
                /* you rang it recently */
                if (!otmp->cursed)
                    arti2_primed = TRUE;
                else
                    arti_cursed = TRUE;
            }
        }

        if (arti_cursed) {
            pline("The invocation fails!");
            if (Hallucination)
                pline("At least one of your heirlooms is in a tizzy!");
            else
                pline("At least one of your artifacts is cursed...");
        } else if (arti1_primed && arti2_primed) {
            unsigned soon = (unsigned)dice(2, 6); /* time til next intervene */

            /* successful invocation */
            mkinvokearea();
            u.uevent.invoked = 1;
            historic_event(FALSE, "performed the invocation.");
            /* in case you haven't killed the Wizard yet, behave as if you just
               did */
            u.uevent.udemigod = 1;      /* wizdead() */
            if (!u.udg_cnt || u.udg_cnt > soon)
                u.udg_cnt = soon;
        } else {        /* at least one artifact not prepared properly */
            pline("You have a feeling that something is amiss...");
            goto raise_dead;
        }
        return;
    }

    /* when not an invocation situation */
    if (invoked) {
        pline("Nothing happens.");
        return;
    }

    if (book2->cursed) {
    raise_dead:

        if (Hallucination)
            You_hear("Michael Jackson dancing!");
        else
            pline("You raised the dead!");
        /* first maybe place a dangerous adversary; don't bother with
           MM_CREATEMONSTER, that's mostly used to ensure that consistent
           species of monsters generate */
        if (!rn2(3) &&
            ((mtmp = makemon(&mons[PM_MASTER_LICH], level, u.ux, u.uy,
                             NO_MINVENT)) != 0 ||
             (mtmp = makemon(&mons[PM_NALFESHNEE], level, u.ux, u.uy,
                             NO_MINVENT)) != 0)) {
            msethostility(mtmp, TRUE, TRUE);
        }
        /* next handle the effect on things you're carrying */
        unturn_dead(&youmonst);
        /* last place some monsters around you */
        mm.x = u.ux;
        mm.y = u.uy;
        mkundead(level, &mm, TRUE, NO_MINVENT);
    } else if (book2->blessed) {
        for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon; /* tamedog() changes chain */
            if (DEADMONSTER(mtmp))
                continue;

            if (is_undead(mtmp->data) && cansee(mtmp->mx, mtmp->my)) {
                msethostility(mtmp, FALSE, FALSE); /* TODO: reset alignment? */
                if (sgn(mtmp->data->maligntyp) == sgn(u.ualign.type)
                    && distu(mtmp->mx, mtmp->my) < 4)
                    if (mtmp->mtame) {
                        if (mtmp->mtame < 20)
                            mtmp->mtame++;
                    } else
                        tamedog(mtmp, NULL);
                else
                    monflee(mtmp, 0, FALSE, TRUE);
            }
        }
    } else {
        switch (rn2(3)) {
        case 0:
            pline("Your ancestors are annoyed with you!");
            break;
        case 1:
            pline("The headstones in the cemetery begin to move!");
            break;
        default:
            pline("Oh my!  Your name appears in the book!");
        }
    }
    return;
}

/* Handles one turn of book reading. Returns 1 if unfinished, 0 if finshed. */
static int
learn(void)
{
    int i;
    short booktype;
    boolean costly = TRUE;
    boolean already_known = FALSE;
    int first_unknown = MAXSPELL;
    int known_spells = 0;
    const char *splname;

    /* JDS: lenses give 50% faster reading; 33% smaller read time */
    if (u.uoccupation_progress[tos_book] &&
        ublindf && ublindf->otyp == LENSES && rn2(2))
        u.uoccupation_progress[tos_book]++;
    if (Confusion) {    /* became confused while learning */
        confused_book(&youmonst, u.utracked[tos_book]);
        u.utracked[tos_book] = 0;       /* no longer studying */
        helpless(-u.uoccupation_progress[tos_book], hr_busy,
                 "absorbed in a spellbook",
                 "You're finally able to put the book down.");
        u.uoccupation_progress[tos_book] = 0;
        return 0;
    }

    booktype = u.utracked[tos_book]->otyp;
    if (booktype == SPE_BOOK_OF_THE_DEAD) {
        deadbook(u.utracked[tos_book], FALSE);
        u.utracked[tos_book] = 0;
        u.uoccupation_progress[tos_book] = 0;
        return 0;
    }

    /* The book might get cursed while we're reading it. In this case,
       immediately stop reading it, cancel progress, and apply a few turns of
       helplessness. (3.4.3 applies negative spellbook effects but lets you
       memorize the spell anyway; this makes no sense. It destroys the spellbook
       on the "contact poison" result, which makes even less sense.) */
    if (u.utracked[tos_book]->cursed) {
        pline("This book isn't making sense any more.");
        helpless(rn1(5,5), hr_busy,
                 "making sense of a spellbook",
                 "You give up trying to make sense of the spellbook.");
        u.uoccupation_progress[tos_book] = 0;
        u.utracked[tos_book] = 0;
        return 0;
    }

    if (++u.uoccupation_progress[tos_book] < 0)
        return 1;       /* still busy */

    exercise(A_WIS, TRUE);      /* you're studying. */

    splname = msgprintf(objects[booktype].oc_name_known ?
                        "\"%s\"" : "the \"%s\" spell",
                        OBJ_NAME(objects[booktype]));
    for (i = 0; i < MAXSPELL; i++) {
        if (spellid(i) == booktype) {
            already_known = TRUE;
            if (u.utracked[tos_book]->spestudied > MAX_SPELL_STUDY) {
                pline("This spellbook is too faint to be read any more.");
                u.utracked[tos_book]->otyp = booktype = SPE_BLANK_PAPER;
            } else if (spellknow(i) <= 1000) {
                pline("Your knowledge of %s is keener.", splname);
                incrnknow(i);
                u.utracked[tos_book]->spestudied++;
                exercise(A_WIS, TRUE);  /* extra study */
            } else {    /* 1000 < spellknow(i) <= MAX_SPELL_STUDY */
                pline("You know %s quite well already.", splname);
                if (yn("Do you want to read the book anyway?") == 'y') {
                    pline("You refresh your knowledge of %s.", splname);
                    incrnknow(i);
                    u.utracked[tos_book]->spestudied++;
                } else
                    costly = FALSE;
            }
            /* make book become known even when spell is already known, in case
               amnesia made you forget the book */
            makeknown((int)booktype);
            break;
        } else if (spellid(i) == NO_SPELL &&
                   (i < first_unknown ||
                    i == spellno_from_let(objects[booktype].oc_defletter)))
            first_unknown = i;
        else
            known_spells++;
    }

    if (first_unknown == MAXSPELL && !already_known)
        panic("Too many spells memorized!");

    if (!already_known) {
        spl_book[first_unknown].sp_id = booktype;
        spl_book[first_unknown].sp_lev = objects[booktype].oc_level;
        incrnknow(first_unknown);
        u.utracked[tos_book]->spestudied++;
        pline(known_spells > 0 ? "You add %s to your repertoire." :
              "You learn %s.", splname);
        makeknown((int)booktype);
    }

    if (costly)
        check_unpaid(u.utracked[tos_book]);
    u.utracked[tos_book] = 0;
    return 0;
}

/* Monster book studying works a bit differently.
   The spellbook delay is currently ignored and replaced with
   a delay of 5 if the reading succeeds. This is due to current
   lack of functionality for "safe" multi-turn actions for monsters.
   A monster's spell memory is binary -- either they know the spell,
   or they don't. If they learn something, it will stay forever, with the
   theoretical exception of amnesia -- something monsters don't read at present.
   Cursed books still retain their original delay and behaviour, and confused
   books retain their behaviour. Returns 0 if nothing happened, -1 if turn
   was wasted with no spell learned, spell id otherwise. */
int
mon_study_book(struct monst *mon, struct obj *spellbook)
{
    int booktype = spellbook->otyp;
    boolean confused = confused(mon);
    boolean too_hard = FALSE;
    boolean vis = canseemon(mon);
    int delay = 0;
    int level = 0;
    boolean spellcaster = FALSE;
    boolean intel = 11;
    struct obj *eyewear = which_armor(mon, os_tool);

    if (attacktype(mon->data, AT_MAGC))
        spellcaster = TRUE;
    if (spellcaster)
        intel = 18;
    if (mon->iswiz)
        intel = 20;

    if (booktype == SPE_BLANK_PAPER) {
        if (vis) {
            pline("%s attempts to read a blank spellbook.", Monnam(mon));
            makeknown(SPE_BLANK_PAPER);
        }
        return 0;
    }
    /* Spellbook delay multiplier. 1 for level 1-2, 2-3 for level 3-4, 5-6 for level 5-6, 8 for level 7 */
    level = objects[booktype].oc_level;
    switch (level) {
    case 1:
    case 2:
        level = 1;
        break;
    case 3:
    case 4:
        level -= 1;
        break;
    case 7:
        level = 8;
        break;
    default:
        break;
    }
    delay = level * objects[booktype].oc_delay;
    if (booktype == SPE_BOOK_OF_THE_DEAD) {
        impossible("Monster reading Book of the Dead?");
        return 0;
    }
    if (spellbook->cursed)
        too_hard = TRUE;
    else if (!spellbook->blessed) {
        /* uncursed - chance to fail */
        int read_ability =
            intel + 4 + mon->m_lev / 2 -
            2 * objects[booktype].oc_level +
            ((eyewear && eyewear->otyp == LENSES) ? 2 : 0);
        /* only wizards know if a spell is too difficult */
        if (wizard) {
            if (read_ability < 12) {
                if (vis)
                    pline("%s realizes the difficulty of the book and puts it down.", Monnam(mon));
                return 0;
            }
        }
        /* it's up to random luck now */
        if (rnd(20) > read_ability) {
            too_hard = TRUE;
        }
    }

    if (too_hard) {
        if (vis)
            pline("%s tries to read a spellbook, but fails!", Monnam(mon));

        boolean gone = cursed_book(mon, spellbook);
        /* update vis in case mon teleported */
        boolean vis_old = vis;
        vis = canseemon(mon);

        mon->mcanmove = 0;
        mon->mfrozen = delay;
        if (gone || !rn2(3)) {
            if (!gone && vis) {
                if (vis_old)
                    pline("The spellbook crumbles to dust!");
                else /* a monster appeared out of nowhere */
                    pline("A spellbook that %s is holding crumbles to dust!", mon_nam(mon));
            }
            if (!objects[spellbook->otyp].oc_name_known &&
                !objects[spellbook->otyp].oc_uname && vis)
                docall(spellbook);
            m_useup(mon, spellbook);
        } 
        return -1;
    } else if (confused) {
        if (!confused_book(mon, spellbook)) {
            spellbook->in_use = FALSE;
        }
        mon->mcanmove = 0;
        mon->mfrozen = delay;
        return -1;
    }

    if (vis)
        pline("%s begins to memorize some spellbook runes.", Monnam(mon));
    mon->mcanmove = 0;
    mon->mfrozen = 5; /* see comment above function. TODO: allow safe helplessness for monsters */

    /* FIXME: don't rely on a certain spell being first */
    int mspellid = (booktype - SPE_DIG);
    mon->mspells |= (uint64_t)(1 << mspellid);
    return mspellid;
}

int
study_book(struct obj *spellbook, const struct nh_cmd_arg *arg)
{
    int booktype = spellbook->otyp;
    boolean confused = (Confusion != 0);
    boolean too_hard = FALSE;

    if (u.uoccupation_progress[tos_book] && !confused &&
        spellbook == u.utracked[tos_book] &&
        /* handle the sequence: start reading, get interrupted, have book
           become erased somehow, resume reading it */
        booktype != SPE_BLANK_PAPER) {
        if (turnstate.continue_message)
            pline("You continue your efforts to memorize the spell.");
    } else {
        /* Restarting reading the book */

        /* KMH -- Simplified this code */
        if (booktype == SPE_BLANK_PAPER) {
            pline("This spellbook is all blank.");
            makeknown(booktype);
            return 1;
        }
        switch (objects[booktype].oc_level) {
        case 1:
        case 2:
            u.uoccupation_progress[tos_book] = -objects[booktype].oc_delay;
            break;
        case 3:
        case 4:
            u.uoccupation_progress[tos_book] =
                -(objects[booktype].oc_level - 1) * objects[booktype].oc_delay;
            break;
        case 5:
        case 6:
            u.uoccupation_progress[tos_book] =
                -objects[booktype].oc_level * objects[booktype].oc_delay;
            break;
        case 7:
            u.uoccupation_progress[tos_book] = -8 * objects[booktype].oc_delay;
            break;
        default:
            impossible("Unknown spellbook level %d, book %d;",
                       objects[booktype].oc_level, booktype);
            return 0;
        }

        /* Books are often wiser than their readers (Rus.) */
        spellbook->in_use = TRUE;
        if (!spellbook->blessed && spellbook->otyp != SPE_BOOK_OF_THE_DEAD) {
            if (spellbook->cursed) {
                too_hard = TRUE;
            } else {
                /* uncursed - chance to fail */
                int read_ability =
                    ACURR(A_INT) + 4 + u.ulevel / 2 -
                    2 * objects[booktype].oc_level +
                    ((ublindf && ublindf->otyp == LENSES) ? 2 : 0);
                /* only wizards know if a spell is too difficult */
                if (Role_if(PM_WIZARD) && read_ability < 20 && !confused) {
                    const char *qbuf;

                    qbuf = msgprintf("This spellbook is %sdifficult to "
                                     "comprehend. Continue?",
                                     (read_ability < 12 ? "very " : ""));
                    if (yn(qbuf) != 'y') {
                        spellbook->in_use = FALSE;
                        return 1;
                    }
                }
                /* it's up to random luck now */
                if (rnd(20) > read_ability) {
                    too_hard = TRUE;
                }
            }
        }

        if (too_hard) {
            boolean gone = cursed_book(&youmonst, spellbook);

            helpless(-u.uoccupation_progress[tos_book], hr_paralyzed,
                     "frozen by a spellbook", NULL);
            u.uoccupation_progress[tos_book] = 0;
            if (gone || !rn2(3)) {
                if (!gone)
                    pline("The spellbook crumbles to dust!");
                if (!objects[spellbook->otyp].oc_name_known &&
                    !objects[spellbook->otyp].oc_uname)
                    docall(spellbook);
                useup(spellbook);
            } else
                spellbook->in_use = FALSE;
            return 1;
        } else if (confused) {
            if (!confused_book(&youmonst, spellbook)) {
                spellbook->in_use = FALSE;
            }
            helpless(-u.uoccupation_progress[tos_book], hr_busy,
                     "absorbed in a spellbook",
                     "You're finally able to put the book down.");
            u.uoccupation_progress[tos_book] = 0;
            u.utracked[tos_book] = 0;
            return 1;
        }
        spellbook->in_use = FALSE;

        pline("You begin to %s the runes.",
              spellbook->otyp == SPE_BOOK_OF_THE_DEAD ? "recite" : "memorize");
    }

    u.utracked[tos_book] = spellbook;

    one_occupation_turn(learn, "studying", occ_book);

    return 1;
}

/* called from moveloop() */
void
age_spells(void)
{
    int i;

    /* The time relative to the hero (a pass through move loop) causes all spell
       knowledge to be decremented.  The hero's speed, rest status, conscious
       status etc. does not alter the loss of memory. */
    for (i = 0; i < MAXSPELL; i++) {
        if (!SPELL_IS_FROM_SPELLBOOK(i))
            continue;
        if (spellknow(i))
            decrnknow(i);
    }
    return;
}

/*
 * Return TRUE if a spell was picked, with the spell index in the return
 * parameter.  Otherwise return FALSE.
 */
static boolean
getspell(int *spell_no)
{
    return dospellmenu("Choose which spell to cast", SPELLMENU_CAST, spell_no);
}

static boolean
getargspell(const struct nh_cmd_arg *arg, int *spell_no)
{
    if (arg->argtype & CMD_ARG_SPELL) {
        char slet = arg->spelllet;
        int sno = spellno_from_let(slet);
        if (sno >= 0 && spellid(sno) != NO_SPELL) {
            *spell_no = sno;
            return TRUE;
        }
    }

    return getspell(spell_no);
}

/* the 'Z' command -- cast a spell */
int
docast(const struct nh_cmd_arg *arg)
{
    int spell_no;

    if (getargspell(arg, &spell_no))
        return spelleffects(spell_no, FALSE, arg);
    return 0;
}

/*
 * Add SPID_* values to the player's spellbook, and remove those which are no
 * longer necessary.
 *
 * SPID_PRAY always exists;
 * SPID_TURN exists according to the player's role.
 *
 * The other three change dynamically:
 *
 * SPID_RLOC: on intrinsic/extrinsic change
 *            (requires Teleportation + xlvl 12, or 8 as a wizard; and/or
 *             Teleportation from polyform specifically)
 * SPID_JUMP: on intrinsic/extrinsic change (requires Jumping)
 * SPID_MONS: on polyform change
 *
 * Thus, we call this function:
 * - At character creation
 * - When levelling up
 * - When polymorphing
 * - When changing equipment
 * - When gaining/losing intrinsic teleportitis
 * This assumes that there's no source of Jumping or Teleportation on a
 * timeout; at present there isn't.
 */
void
update_supernatural_abilities(void)
{
    int i, j;

    /* Look through our spellbook for abilities we no longer have, and delete
       them. */
    for (i = 0; i < MAXSPELL; i++) {
        if (!SPELL_IS_FROM_SPELLBOOK(i) && spellid(i) &&
            !supernatural_ability_available(spellid(i))) {
            spl_book[i].sp_id = 0;
            spl_book[i].sp_know = 0;
            spl_book[i].sp_lev = 0;
        }
    }

    /* For each ability we have, look for it in our spellbook. If we can't find
       it there, assign a letter to it. We default to letters near the end of
       the alphabet, specific to each spell. We might have to use a different
       letter, though, if the one we want is already taken. */
    for (j = -1; j >= -SPID_COUNT; j--) {
        if (!supernatural_ability_available(j))
            continue;
        int best = -1;
        for (i = 0; i < MAXSPELL; i++) {
            /* Later letters are better than earlier ones, apart from the
               last SPID_COUNT which are worth avoiding if possible. Two
               exceptions trump everything: the letter where the spell already
               is; and the preferred letter. */
            if (spellid(i) == j)
                goto next_spid; /* multi-level continue */
            if (spellid(i) == NO_SPELL &&
                (i < MAXSPELL - SPID_COUNT || best == -1 ||
                 spelllet_from_no(i) == SPID_PREFERRED_LETTER[-1-j]))
                best = i;
        }
        if (best == -1)
            panic("too many spells + supernatural abilities");
        spl_book[best].sp_id = j;
        spl_book[best].sp_lev = 0;
        spl_book[best].sp_know = 0;
    next_spid:
        ;
    }
}

boolean
supernatural_ability_available(int spid)
{
    switch (spid) {
    case SPID_PRAY:
        return TRUE;
    case SPID_TURN: /* 3.4.3 doturn: "Knights & Priest(esse)s only please" */
        return Role_if(PM_PRIEST) || Role_if(PM_KNIGHT);
    case SPID_RLOC:
        return teleportitis(&youmonst) &&
            (u.ulevel >= (Role_if(PM_WIZARD) ? 8 : 12));
    case SPID_JUMP:
        return Jumping;
    case SPID_MONS:
        return has_polyform_ability(youmonst.data, NULL);
    default:
        impossible("Unknown supernatural ability %d", spid);
        return FALSE;
    }
}

static const char *
spelltypemnemonic(int skill)
{
    switch (skill) {
    case P_ATTACK_SPELL:
        return "attack";
    case P_HEALING_SPELL:
        return "healing";
    case P_DIVINATION_SPELL:
        return "divination";
    case P_ENCHANTMENT_SPELL:
        return "enchantment";
    case P_CLERIC_SPELL:
        return "clerical";
    case P_ESCAPE_SPELL:
        return "escape";
    case P_MATTER_SPELL:
        return "matter";
    default:
        impossible("Unknown spell skill, %d;", skill);
        return "";
    }
}

int
spell_skilltype(int booktype)
{
    return objects[booktype].oc_skill;
}

/* P_* -> MP_* conversion */
static int
mspell_skilltype(int booktype)
{
    int pskill = spell_skilltype(booktype);
    switch (pskill) {
    case P_ATTACK_SPELL:
        return MP_SATTK;
    case P_ESCAPE_SPELL:
        return MP_SESCA;
    case P_CLERIC_SPELL:
        return MP_SCLRC;
    case P_DIVINATION_SPELL:
        return MP_SDIVN;
    case P_MATTER_SPELL:
        return MP_SMATR;
    case P_ENCHANTMENT_SPELL:
        return MP_SENCH;
    case P_HEALING_SPELL:
        return MP_SHEAL;
    }
    impossible("invalid monster proficiency: %d", pskill);
    return 0;
}

static void
cast_protection(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    int loglev = 0;
    int l = you ? u.ulevel : mon->m_lev;
    /* Monsters can be level 0, ensure that no oddities occur if that is the case. */
    if (l == 0)
        l = 1;
    int natac = find_mac(mon) + m_mspellprot(mon);
    int gain;

    /* loglev=log2(u.ulevel)+1 (1..5) */
    while (l) {
        loglev++;
        l /= 2;
    }

    /*
     * The more u.uspellprot you already have, the less you get,
     * and the better your natural ac, the less you get.
     * 0, 31-63 is only reachable by monsters
     *
     *	LEVEL AC    SPELLPROT from sucessive SPE_PROTECTION casts
     *      0-1   10    0,  1,  2,  3,  4
     *      0-1    0    0,  1,  2,  3
     *      0-1  -10    0,  1,  2
     *      2-3   10    0,  2,  4,  5,  6,  7,  8
     *      2-3    0    0,  2,  4,  5,  6
     *      2-3  -10    0,  2,  3,  4
     *      4-7   10    0,  3,  6,  8,  9, 10, 11, 12
     *      4-7    0    0,  3,  5,  7,  8,  9
     *      4-7  -10    0,  3,  5,  6
     *      7-15 -10    0,  3,  5,  6
     *      8-15  10    0,  4,  7, 10, 12, 13, 14, 15, 16
     *      8-15   0    0,  4,  7,  9, 10, 11, 12
     *      8-15 -10    0,  4,  6,  7,  8
     *     16-30  10    0,  5,  9, 12, 14, 16, 17, 18, 19, 20
     *     16-30   0    0,  5,  9, 11, 13, 14, 15
     *     16-31 -10    0,  5,  8,  9, 10
     *     32-63  10    0,  6, 11, 15, 18, 20, 21, 22, 23, 24
     *     32-63   0    0,  6, 10, 13, 15, 16, 17, 18
     *     32-63 -10    0,  6,  9, 11, 12
     */
    gain = loglev - (int)m_mspellprot(mon) / (4 - min(3, (10 - natac) / 10));

    if (gain > 0) {
        if (!blind(&youmonst) && (you || vis)) {
            const char *hgolden = hcolor("golden");

            if (m_mspellprot(mon))
                pline("The %s haze around %s becomes more dense.", hgolden,
                      you ? "you" : mon_nam(mon));
            else
                pline("The %s around %s begins to shimmer with %s haze.",
                      (Underwater || Is_waterlevel(&u.uz)) ? "water" :
                      Engulfed ? mbodypart(u.ustuck, STOMACH) :
                      IS_STWALL(level->locations[u.ux][u.uy].typ) ? "stone" :
                      "air", you ? "you" : mon_nam(mon),
                      an(hgolden));
        } else if (you) {
            if (m_mspellprot(mon))
                pline("Your skin begins feeling warmer.");
            else
                pline("Your skin feels even hotter.");
        }
        if (you) {
            u.uspellprot += gain;
            u.uspmtime =
                P_SKILL(spell_skilltype(SPE_PROTECTION)) == P_EXPERT ? 20 : 10;
            if (!u.usptime)
                u.usptime = u.uspmtime;
        } else {
            /* Monster's "golden haze" instead works by increasing a mt_prop for
               the PROTECTION property. monspellprot() then converts this into
               an AC bonus. */
            int cur_prot = m_mspellprot(mon);
            cur_prot += gain;
            cur_prot *= 10;
            mon->mt_prop[mt_protection] = cur_prot;
            /* mt_protection is special cased to only decrease 50% of the time
               on Expert. */
        }
    } else if (you) {
        pline("Your skin feels warm for a moment.");
    }
}

/* monster golden haze level */
int
monspellprot(struct monst *mon)
{
    return ((mon->mt_prop[mt_protection] + 9) / 10);
}

/* attempting to cast a forgotten spell will cause disorientation */
static void
spell_backfire(int spell)
{
    long duration = (long)((spellev(spell) + 1) * 3);   /* 6..24 */

    /* prior to 3.4.1, the only effect was confusion; it still predominates */
    switch (rn2(10)) {
    case 0:
    case 1:
    case 2:
    case 3:
        make_confused(duration, FALSE); /* 40% */
        break;
    case 4:
    case 5:
    case 6:
        make_confused(2L * duration / 3L, FALSE);       /* 30% */
        make_stunned(duration / 3L, FALSE);
        break;
    case 7:
    case 8:
        make_stunned(2L * duration / 3L, FALSE);        /* 20% */
        make_confused(duration / 3L, FALSE);
        break;
    case 9:
        make_stunned(duration, FALSE);  /* 10% */
        break;
    }
    return;
}

/* Can a monster cast a specific spell? If the monster doesn't even know the spell
   in first place, this will always return FALSE. Otherwise, it will return TRUE
   based on the percentage success of the spell. The side effect is that monsters
   with a 80% failure rate on a spell will only return TRUE 1/5 of the time, meaning
   that monsters will generally (try to) cast those spells much more rarely. This is
   by design. */
boolean
mon_castable(struct monst *mon, int spell)
{
    /* FIXME: don't rely on spell order */
    int mspellid = spell - SPE_DIG;

    /* is the spell part of the monster's spell list */
    pline("check castable %ld", mon->mspells);
    if (!(mon->mspells & (uint64_t)(1 << mspellid)))
        return FALSE;

    pline("is castable!");
    /* calculate fail rate */
    /* Confusion also makes spells fail 100% of the time,
       but don't make monsters savvy about that for now. */
    int chance = percent_success(mon, spell);
    pline("fail rate: %d", 100 - chance);
    if (rnd(100) > chance)
        return FALSE;
    return TRUE;
}

/* Monster spellcasting. Currently it is not possible to reuse spelleffects()
   because a lot of things it does relies on your command argument(s).
   TODO: reuse spelleffects() */
int
m_spelleffects(struct monst *mon, int spell, schar dx, schar dy, schar dz)
{
    int energy, chance, n;
    int skill, role_skill;
    boolean confused = !!confused(mon);
    boolean vis = canseemon(mon);
    boolean dummy;
    coord cc;
    boolean ray = FALSE;
    struct obj *pseudo;
    skill = mspell_skilltype(spell);
    role_skill = mprof(mon, skill);
    int count = 0;

    if (vis)
        pline("%s casts a spell!", Monnam(mon));

    /* highlevel casters can cast more than lowlevel ones */
    int cooldown = 7 - (mon->m_lev / 5);
    if (cooldown < 2)
        cooldown = 2;
    energy = (objects[spell].oc_level * cooldown);
    if (mon_has_amulet(mon)) {
        /* since monsters either have the mspec to cast or not,
           make the amulet occasionally make the spell fail. */
        if (rn2(5)) {
                if (vis)
                    pline("The amulet drains %s energy away...",
                          s_suffix(mon_nam(mon)));
                energy += 2 * rnd(energy);
        } else {
            if (vis)
                pline("But the amulet drained %s spell...",
                      s_suffix(mon_nam(mon)));
            return 0;
        }
    }

    /* can't cast -- energy is used up! */
    if (mon->mspec_used) {
        if (vis)
            pline("But %s lacks the energy to cast the spell (en: %d).",
                  mon_nam(mon), mon->mspec_used);
        return 0;
    }

    chance = percent_success(mon, spell);
    if (confused || (rnd(100) > chance)) {
        pline("But %s fails to cast the spell correctly.",
              mon_nam(mon));
        mon->mspec_used += energy / 2;
        return 1;
    }

    mon->mspec_used += energy;
    /* pseudo is a temporary "false" object containing the spell stats */
    pseudo = mktemp_sobj(level, spell);
    pseudo->blessed = pseudo->cursed = 0;
    pseudo->quan = 20L; /* do not let useup get it */

    switch (pseudo->otyp) {
    case SPE_CONE_OF_COLD:
    case SPE_FIREBALL:
        if (role_skill >= P_SKILLED) {
            cc.x = dx;
            cc.y = dy;
            n = rnd(8) + 1;
            while (n--) {
                explode(dx, dy, pseudo->otyp - SPE_MAGIC_MISSILE + 10,
                        mon->m_lev / 2 + 1 + spell_damage_bonus(), 0,
                        (pseudo->otyp ==
                         SPE_CONE_OF_COLD) ? EXPL_FROSTY : EXPL_FIERY,
                        NULL, 0);
                dx = cc.x + rnd(3) - 2;
                dy = cc.y + rnd(3) - 2;
                if (!isok(dx, dy) || !cansee(dx, dy) ||
                    IS_STWALL(level->locations[dx][dy].typ)) {
                    /* Spell is reflected back to center */
                    dx = cc.x;
                    dy = cc.y;
                }
            }
            break;
        }

        /* else fall through... */
        /* these spells are all duplicates of wand effects */
    case SPE_MAGIC_MISSILE:
    case SPE_SLEEP:
    case SPE_FINGER_OF_DEATH:
        ray = TRUE;
    case SPE_FORCE_BOLT:
    case SPE_KNOCK:
    case SPE_SLOW_MONSTER:
    case SPE_WIZARD_LOCK:
    case SPE_DIG:
    case SPE_TURN_UNDEAD:
    case SPE_POLYMORPH:
    case SPE_TELEPORT_AWAY:
    case SPE_CANCELLATION:
    case SPE_LIGHT:
    case SPE_DETECT_UNSEEN:
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
    case SPE_DRAIN_LIFE:
    case SPE_STONE_TO_FLESH:
        if (objects[pseudo->otyp].oc_dir != NODIR) {
            if (!dx && !dy && !dz)
                bhitm(mon, mon, pseudo);
            else if (ray)
                buzz(-10 - (pseudo->otyp - SPE_MAGIC_MISSILE),
                     mon->m_lev / 2 + 1, mon->mx, mon->my,
                     dx, dy, 0);
            else if (pseudo->otyp == SPE_DIG)
                zap_dig(mon, pseudo, dx, dy, dz);
            else
                mbhit(mon, rn1(8, 6), pseudo);
        } else {
            impossible("Monster tried to use non-directional wand-based spell?");
            break;
        }
        break;

        /* these are all duplicates of scroll effects */
    case SPE_REMOVE_CURSE:
    case SPE_CONFUSE_MONSTER:
    case SPE_CAUSE_FEAR:
        /* high skill yields effect equivalent to blessed scroll */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
        /* fall through */
    case SPE_CHARM_MONSTER:
    case SPE_MAGIC_MAPPING:
    case SPE_CREATE_MONSTER:
    case SPE_IDENTIFY:
    case SPE_CHARGING:
        seffects(mon, pseudo, &dummy);
        break;

        /* these are all duplicates of potion effects */
    case SPE_HASTE_SELF:
    case SPE_DETECT_TREASURE:
    case SPE_DETECT_MONSTERS:
    case SPE_LEVITATION:
    case SPE_RESTORE_ABILITY:
        /* high skill yields effect equivalent to blessed potion */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
        /* fall through */
    case SPE_INVISIBILITY:
        peffects(mon, pseudo);
        break;

    case SPE_CURE_BLINDNESS:
        set_property(mon, BLINDED, -2, FALSE);
        break;
    case SPE_CURE_SICKNESS:
        set_property(mon, SICK, -2, FALSE);
        set_property(mon, SLIMED, -2, FALSE);
        break;
    case SPE_CREATE_FAMILIAR:
        make_familiar(mon, NULL, m_mx(mon), m_my(mon), FALSE);
        break;
    case SPE_SUMMON_NASTY:
        cc.x = dx;
        cc.y = dy;
        count = nasty(mon, cc);
        if (count) {
            pline("%s from nowhere!", count > 1 ? "Monsters appear" :
                  "A monster appears");
        }
        break;
    case SPE_CLAIRVOYANCE:
        if (!bclairvoyant(mon))
            /*do_vicinity_map(); wont work on monsters */
        break;
    case SPE_PROTECTION:
        cast_protection(mon);
        break;
    case SPE_JUMPING:
        jump_to_coords(&cc);
        break;
    case SPE_ASTRAL_EYESIGHT:
        set_property(mon, XRAY_VISION, rn1(20, 11), FALSE);
        break;
    case SPE_PHASE:
        set_property(mon, PASSES_WALLS, rn1(40, 21), FALSE);
        break;
    default:
        impossible("Unknown or nonimplemented monster spell %d attempted.", spell);
        obfree(pseudo, NULL);
        return 0;
    }

    obfree(pseudo, NULL);       /* now, get rid of it */
    return 1;
}

int
spelleffects(int spell, boolean atme, const struct nh_cmd_arg *arg)
{
    int energy, damage, chance, n, intell;
    int skill, role_skill;
    boolean confused = (Confusion != 0);
    struct obj *pseudo;
    boolean dummy;
    coord cc;
    schar dx = 0, dy = 0, dz = 0;
    boolean thrownasty = FALSE;
    int count = 0; /* for nasty */

    if (!SPELL_IS_FROM_SPELLBOOK(spell)) {
        /* At the moment, we implement this via calling the code for the
           shortcut command. Eventually, it would make sense to invert this
           (and make the shortcut commands wrappers for spelleffects). */
        switch (spellid(spell)) {
        case SPID_PRAY:
            return dopray(arg);
        case SPID_TURN:
            return doturn(arg);
        case SPID_RLOC:
            return dotele(arg);
        case SPID_JUMP:
            return dojump(arg);
        case SPID_MONS:
            return domonability(arg);
        default:
            impossible("Unknown spell number %d?", spellid(spell));
            return 0;
        }
    }

    /*
     * Find the skill the hero has in a spell type category.
     * See spell_skilltype for categories.
     */
    skill = spell_skilltype(spellid(spell));
    role_skill = P_SKILL(skill);

    /* Get the direction or target, if applicable.

       We want to do this *before* determining spell success, both for interface
       consistency and to cut down on needless mksobj calls. */
    switch (spellid(spell)) {

    /* These spells ask the user to target a specific space. */
    case SPE_SUMMON_NASTY:
        thrownasty = TRUE;
    case SPE_CONE_OF_COLD:
    case SPE_FIREBALL:
        /* If Skilled or better, get a specific space. */
        if (role_skill >= P_SKILLED) {
            if (throwspell(thrownasty, &dx, &dy, arg)) {
                dz = 0;
                break;
            }
            else {
                /* Decided not to target anything.  Abort the spell. */
                pline("Spell canceled.");
                return 0;
            }
        }
        /* If not Skilled, fall through. */

    /* These spells ask the user to target a direction. */
    case SPE_FORCE_BOLT:
    case SPE_SLEEP:
    case SPE_MAGIC_MISSILE:
    case SPE_KNOCK:
    case SPE_SLOW_MONSTER:
    case SPE_WIZARD_LOCK:
    case SPE_DIG:
    case SPE_TURN_UNDEAD:
    case SPE_POLYMORPH:
    case SPE_TELEPORT_AWAY:
    case SPE_CANCELLATION:
    case SPE_FINGER_OF_DEATH:
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
    case SPE_DRAIN_LIFE:
    case SPE_STONE_TO_FLESH:
        if (atme)
            dx = dy = dz = 0;
        else if (!getargdir(arg, NULL, &dx, &dy, &dz)) {
            /* getdir cancelled, abort */
            pline("Spell canceled.");
            return 0;
        }
        break;
    case SPE_JUMPING:
        if(!get_jump_coords(arg, &cc, max(role_skill, 1))) {
            /* No jumping after all, I guess. */
            pline("Spell canceled.");
            return 0;
        }
        break;

    /* The rest of the spells don't have targeting. */
    default:
        break;
    }


    /* Spell casting no longer affects knowledge of the spell. A decrement of
       spell knowledge is done every turn. */
    if (spellknow(spell) <= 0) {
        pline("Your knowledge of this spell is twisted.");
        pline("It invokes nightmarish images in your mind...");
        spell_backfire(spell);
        return 0;
    } else if (spellknow(spell) <= 200) {       /* 1% */
        pline("You strain to recall the spell.");
    } else if (spellknow(spell) <= 1000) {      /* 5% */
        pline("Your knowledge of this spell is growing faint.");
    }
    energy = (spellev(spell) * 5);      /* 5 <= energy <= 35 */

    if (u.uhunger <= 10) {
        pline("You are too hungry to cast that spell.");
        return 0;
    } else if (ACURR(A_STR) < 4) {
        pline("You lack the strength to cast spells.");
        return 0;
    } else
        if (check_capacity
            ("Your concentration falters while carrying so much stuff.")) {
        return 1;
    } else if (!freehand()) {
        pline("Your arms are not free to cast!");
        return 0;
    }

    if (Uhave_amulet) {
        pline("You feel the amulet draining your energy away.");
        energy += rnd(2 * energy);
    }
    if (energy > u.uen) {
        pline("You don't have enough energy to cast that spell.");
        return 0;
    } else {
        int hungr = energy * 2;

        /*
         * If hero is a wizard, their current intelligence
         * (bonuses + temporary + current)
         * affects hunger reduction in casting a spell.
         * 1. int = 17-18 no reduction
         * 2. int = 16    1/4 hungr
         * 3. int = 15    1/2 hungr
         * 4. int = 1-14  normal reduction
         * The reason for this is:
         * a) Intelligence affects the amount of exertion
         * in thinking.
         * b) Wizards have spent their life at magic and
         * understand quite well how to cast spells.
         */
        intell = acurr(A_INT);
        if (!Role_if(PM_WIZARD))
            intell = 10;
        if (intell >= 17)
            hungr = 0;
        else if (intell == 16)
            hungr /= 4;
        else if (intell == 15)
            hungr /= 2;

        /* don't put player (quite) into fainting from casting a spell,
           particularly since they might not even be hungry at the
           beginning; however, this is low enough that they must eat before
           casting anything else except detect food */
        if (hungr > u.uhunger - 3)
            hungr = u.uhunger - 3;
        morehungry(hungr);
    }

    chance = percent_success(&youmonst, spellid(spell));
    if (confused || (rnd(100) > chance)) {
        pline("You fail to cast the spell correctly.");
        u.uen -= energy / 2;
        return 1;
    }

    u.uen -= energy;
    exercise(A_WIS, TRUE);
    /* pseudo is a temporary "false" object containing the spell stats */
    pseudo = mktemp_sobj(level, spellid(spell));
    pseudo->blessed = pseudo->cursed = 0;
    pseudo->quan = 20L; /* do not let useup get it */

    switch (pseudo->otyp) {
        /*
         * At first spells act as expected.  As the hero increases in skill
         * with the appropriate spell type, some spells increase in their
         * effects, e.g. more damage, further distance, and so on, without
         * additional cost to the spellcaster.
         */
    case SPE_CONE_OF_COLD:
    case SPE_FIREBALL:
        if (role_skill >= P_SKILLED) {
            cc.x = dx;
            cc.y = dy;
            n = rnd(8) + 1;
            while (n--) {
                if (!dx && !dy && !dz) {
                    if ((damage = zapyourself(pseudo, TRUE)) != 0)
                        losehp(damage, msgprintf(
                                   "zapped %sself with an exploding spell",
                                   uhim()));
                } else {
                    explode(dx, dy, pseudo->otyp - SPE_MAGIC_MISSILE + 10,
                            u.ulevel / 2 + 1 + spell_damage_bonus(), 0,
                            (pseudo->otyp ==
                             SPE_CONE_OF_COLD) ? EXPL_FROSTY : EXPL_FIERY,
                            NULL, 0);
                }
                dx = cc.x + rnd(3) - 2;
                dy = cc.y + rnd(3) - 2;
                if (!isok(dx, dy) || !cansee(dx, dy) ||
                    IS_STWALL(level->locations[dx][dy].typ) || Engulfed) {
                    /* Spell is reflected back to center */
                    dx = cc.x;
                    dy = cc.y;
                }
            }
            break;
        }

        /* else fall through... */
        /* these spells are all duplicates of wand effects */
    case SPE_FORCE_BOLT:
    case SPE_SLEEP:
    case SPE_MAGIC_MISSILE:
    case SPE_KNOCK:
    case SPE_SLOW_MONSTER:
    case SPE_WIZARD_LOCK:
    case SPE_DIG:
    case SPE_TURN_UNDEAD:
    case SPE_POLYMORPH:
    case SPE_TELEPORT_AWAY:
    case SPE_CANCELLATION:
    case SPE_FINGER_OF_DEATH:
    case SPE_LIGHT:
    case SPE_DETECT_UNSEEN:
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
    case SPE_DRAIN_LIFE:
    case SPE_STONE_TO_FLESH:
        if (objects[pseudo->otyp].oc_dir != NODIR) {
            if (!dx && !dy && !dz) {
                if ((damage = zapyourself(pseudo, TRUE)) != 0) {
                    losehp(damage, msgprintf("zapped %sself with a spell",
                                             uhim()));
                }
            } else
                weffects(pseudo, dx, dy, dz);
        } else
            weffects(pseudo, 0, 0, 0);
        update_inventory();     /* spell may modify inventory */
        break;

        /* these are all duplicates of scroll effects */
    case SPE_REMOVE_CURSE:
    case SPE_CONFUSE_MONSTER:
    case SPE_CAUSE_FEAR:
        /* high skill yields effect equivalent to blessed scroll */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
        /* fall through */
    case SPE_CHARM_MONSTER:
    case SPE_MAGIC_MAPPING:
    case SPE_CREATE_MONSTER:
    case SPE_IDENTIFY:
    case SPE_CHARGING:
        seffects(&youmonst, pseudo, &dummy);
        break;

        /* these are all duplicates of potion effects */
    case SPE_HASTE_SELF:
    case SPE_DETECT_TREASURE:
    case SPE_DETECT_MONSTERS:
    case SPE_LEVITATION:
    case SPE_RESTORE_ABILITY:
        /* high skill yields effect equivalent to blessed potion */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
        /* fall through */
    case SPE_INVISIBILITY:
        peffects(&youmonst, pseudo);
        break;

    case SPE_CURE_BLINDNESS:
        healup(0, 0, FALSE, TRUE);
        break;
    case SPE_CURE_SICKNESS:
        if (Sick)
            pline("You are no longer ill.");
        if (Slimed) {
            pline("The slime disappears!");
            Slimed = 0;
        }
        healup(0, 0, TRUE, FALSE);
        break;
    case SPE_CREATE_FAMILIAR:
        make_familiar(&youmonst, NULL, u.ux, u.uy, FALSE);
        break;
    case SPE_SUMMON_NASTY:
        cc.x = dx;
        cc.y = dy;
        count = nasty(&youmonst, cc);
        if (count) {
            pline("%s from nowhere!", count > 1 ? "Monsters appear" :
                  "A monster appears");
        } else
            pline("You feel lonely.");
        /* summoning aggravates the target */
        struct monst *mtmp = m_at(level, cc.x, cc.y);
        if (mtmp) {
            if (mtmp->mtame)
                abuse_dog(mtmp);
            else if (mtmp->mpeaceful)
                setmangry(mtmp);
        }
        break;
    case SPE_CLAIRVOYANCE:
        if (!BClairvoyant)
            do_vicinity_map();
        /* at present, only one thing blocks clairvoyance */
        else if (uarmh && uarmh->otyp == CORNUTHAUM)
            pline("You sense a pointy hat on top of your %s.", body_part(HEAD));
        break;
    case SPE_PROTECTION:
        cast_protection(&youmonst);
        break;
    case SPE_JUMPING:
        jump_to_coords(&cc);
        break;
    case SPE_ASTRAL_EYESIGHT:
        set_property(&youmonst, XRAY_VISION, rn1(20, 11), FALSE);
        break;
    case SPE_PHASE:
        set_property(&youmonst, PASSES_WALLS, rn1(40, 21), FALSE);
        break;
    default:
        impossible("Unknown spell %d attempted.", spell);
        obfree(pseudo, NULL);
        return 0;
    }

    /* gain skill for successful cast */
    use_skill(skill, spellev(spell));

    obfree(pseudo, NULL);       /* now, get rid of it */
    return 1;
}

/* Choose location where spell takes effect.
   Fireball/CoC and summon nasty have different conditions. */
static int
throwspell(boolean nasty, schar *dx, schar *dy, const struct nh_cmd_arg *arg)
{
    coord cc;

    if (!nasty && u.uinwater) {
        pline("You're joking! In this weather?");
        return 0;
    } else if (!nasty && Is_waterlevel(&u.uz)) {
        pline("You had better wait for the sun to come out.");
        return 0;
    }

    if (nasty)
        pline("What monster do you want to target?");
    else
        pline("Where do you want to cast the spell?");
    cc.x = u.ux;
    cc.y = u.uy;
    if (getargpos(arg, &cc, FALSE, "the desired position") == NHCR_CLIENT_CANCEL)
        return 0;       /* user pressed ESC */
    /* The number of moves from hero to where the spell drops. */
    if (distmin(u.ux, u.uy, cc.x, cc.y) > 10) {
        pline("The spell dissipates over the distance!");
        return 0;
    } else if (Engulfed) {
        pline("The spell is cut short!");
        exercise(A_WIS, FALSE); /* What were you THINKING! */
        *dx = 0;
        *dy = 0;
        return 1;
    } else if (nasty && (cc.x != u.ux || cc.y != u.uy) &&
               (!MON_AT(level, cc.x, cc.y) ||
                !canspotmon(m_at(level, cc.x, cc.y)))) {
        pline("You fail to sense a monster there!");
        return 0;
    } else
        if ((!cansee(cc.x, cc.y) &&
             (!MON_AT(level, cc.x, cc.y) ||
              !canspotmon(m_at(level, cc.x, cc.y)))) ||
            IS_STWALL(level->locations[cc.x][cc.y].typ)) {
        pline("Your mind fails to lock onto that location!");
        return 0;
    } else {
        *dx = cc.x;
        *dy = cc.y;
        return 1;
    }
}

void
losespells(void)
{
    int n;
    for (n = 0; n < MAXSPELL; n++) {
        if(!SPELL_IS_FROM_SPELLBOOK(n))
            continue;
        spellknow(n) = rn2(spellknow(n) + 1);
    }
}

/* the '+' command -- view known spells */
int
dovspell(const struct nh_cmd_arg *arg)
{
    const char *qbuf;
    int splnum, othnum;
    struct spell spl_tmp;

    (void) arg;

    while (dospellmenu("Your magical abilities", SPELLMENU_VIEW, &splnum)) {
        qbuf = msgprintf("Reordering magic: adjust '%c' to",
                         spelllet_from_no(splnum));
        if (!dospellmenu(qbuf, splnum, &othnum))
            break;

        spl_tmp = spl_book[splnum];
        spl_book[splnum] = spl_book[othnum];
        spl_book[othnum] = spl_tmp;
    }
    return 0;
}

static boolean
dospellmenu(const char *prompt,
            int splaction,  /* SPELLMENU_CAST, SPELLMENU_VIEW, or
                               spl_book[] index */
            int *spell_no)
{
    int i, n, how, count = 0;
    struct nh_menuitem items[MAXSPELL + 1];
    const int *selected;

    set_menuitem(&items[count++], 0, MI_HEADING,
                 "Name\tLevel\tCategory\tFail\tMemory", 0, FALSE);
    for (i = 0; i < MAXSPELL; i++) {
        if (spellid(i) == NO_SPELL)
            continue;
        const char *buf = SPELL_IS_FROM_SPELLBOOK(i) ?
            msgprintf("%s\t%-d%s\t%s\t%-d%%\t%-d%%", spellname(i), spellev(i),
                      spellknow(i) ? " " : "*",
                      spelltypemnemonic(spell_skilltype(spellid(i))),
                      100 - percent_success(&youmonst, spellid(i)),
                      (spellknow(i) * 100 + (KEEN - 1)) / KEEN) :
            msgprintf("%s\t--\t%s\t?\t--", spellname(i),
                      (spellid(i) == SPID_PRAY || spellid(i) == SPID_TURN) ?
                      "divine" : "ability");
        set_menuitem(&items[count++], i + 1, MI_NORMAL, buf,
                     spelllet_from_no(i), FALSE);
    }

    how = PICK_ONE;
    if (splaction >= 0)
        how = PICK_LETTER;      /* We're swapping spells. */

    n = display_menu(&(struct nh_menulist){.items = items, .icount = count},
                     prompt, how, PLHINT_ANYWHERE, &selected);

    if (n > 0) {
        *spell_no = selected[0] - 1;
        /* menu selection for `PICK_ONE' does not de-select any preselected
           entry */
        if (n > 1 && *spell_no == splaction)
            *spell_no = selected[1] - 1;
        /* default selection of preselected spell means that user chose not to
           swap it with anything */
        if (*spell_no == splaction)
            return FALSE;
        return TRUE;
    } else if (splaction >= 0) {
        /* explicit de-selection of preselected spell means that user is still
           swapping but not for the current spell */
        *spell_no = splaction;
        return TRUE;
    }
    return FALSE;
}


void
dump_spells(void)
{
    /* note: the actual dumping is done in dump_display_menu(), we just need to
       get the data there. */
    dospellmenu("Spells and supernatural/magical abilities:",
                SPELLMENU_VIEW, NULL);
}

/* Note: only gives sensible results if SPELL_IS_FROM_SPELLBOOK(spell)
   Potentially we could expand this to prayer and the like, but that'd need a
   rewrite of the prayer success code (and mathematical analysis of rnz)
   TODO: this formula is stupid, look into a potential rework of it
   TODO: this completely break As spawning with shields of reflection, making
   them unable to cast sanely due to an unwieldy shield, figure out a fix... */
static int
percent_success(const struct monst *mon, int spell)
{
    /* Intrinsic and learned ability are combined to calculate the probability
       of player's success at cast a given spell. */
    int chance, splcaster, special, statused;
    int difficulty;
    int skill;
    boolean you = (mon == &youmonst);

    /* Calculate intrinsic ability (splcaster) */

    if (you) {
        splcaster = urole.spelbase;
        special = urole.spelheal;
        statused = ACURR(urole.spelstat);
    } else if (!mon->iswiz) {
        splcaster = attacktype(mon->data, AT_MAGC) ? 3 : 8;
        special = mon->data == &mons[PM_NURSE] ? -3 : -1;
        statused = attacktype(mon->data, AT_MAGC) ? 18 : 11;
    } else {
        /* Wizard of Yendor has superior spellcasting skills */
        splcaster = 1;
        special = -3;
        statused = 20;
    }
    struct obj *arm = which_armor(mon, os_arm);
    struct obj *armc = which_armor(mon, os_armc);
    struct obj *arms = which_armor(mon, os_arms);
    struct obj *armh = which_armor(mon, os_armh);
    struct obj *armg = which_armor(mon, os_armg);
    struct obj *armf = which_armor(mon, os_armf);
    int spelarmr = you ? urole.spelarmr : 10;
    int spelshld = you ? urole.spelshld : 1;
    int xl = you ? u.ulevel : mon->m_lev;

    if (arm && is_metallic(arm))
        splcaster += (armc &&
                      armc->otyp ==
                      ROBE) ? spelarmr / 2 : spelarmr;
    else if (armc && armc->otyp == ROBE)
        splcaster -= spelarmr;
    if (arms)
        splcaster += spelshld;

    if (armh && is_metallic(armh)) {
        if (armh->otyp != HELM_OF_BRILLIANCE)
            splcaster += uarmhbon;
        else if (!you) { /* HoB boost was not factored in above if !you */
            statused += armh->spe;
            if (statused > 25)
                statused = 25;
            else if (statused < 3)
                statused = 3;
        }
    }
    if (armg && is_metallic(armg))
        splcaster += uarmgbon;
    if (armf && is_metallic(armf))
        splcaster += uarmfbon;

    if (you && spell == urole.spelspec)
        splcaster += urole.spelsbon;


    /* `healing spell' bonus */
    if (spell == SPE_HEALING ||
        spell == SPE_EXTRA_HEALING ||
        spell == SPE_CURE_BLINDNESS ||
        spell == SPE_CURE_SICKNESS ||
        spell == SPE_RESTORE_ABILITY ||
        spell == SPE_REMOVE_CURSE)
        splcaster += special;

    if (splcaster > 20)
        splcaster = 20;

    /* Calculate learned ability */

    /* Players basic likelihood of being able to cast any spell is based of
       their `magic' statistic. (Int or Wis) */
    chance = 11 * statused / 2;

    /*
     * High level spells are harder.  Easier for higher level casters.
     * The difficulty is based on the hero's level and their skill level
     * in that spell type.
     */
    if (you)
        skill = P_SKILL(spell_skilltype(spell));
    else
        skill = mprof(mon, mspell_skilltype(spell));
    skill = max(skill, P_UNSKILLED) - 1;        /* unskilled => 0 */
    difficulty = (objects[spell].oc_level - 1) * 4 - ((skill * 6) + (xl / 3) + 1);

    if (difficulty > 0) {
        /* Player is too low level or unskilled. */
        chance -= isqrt(900 * difficulty + 2000);
    } else {
        /* Player is above level.  Learning continues, but the law of
           diminishing returns sets in quickly for low-level spells.  That is,
           a player quickly gains no advantage for raising level. */
        int learning = 15 * -difficulty / objects[spell].oc_level;

        chance += learning > 20 ? 20 : learning;
    }

    /* Clamp the chance: >18 stat and advanced learning only help to a limit,
       while chances below "hopeless" only raise the specter of overflowing
       16-bit ints (and permit wearing a shield to raise the chances :-). */
    if (chance < 0)
        chance = 0;
    if (chance > 120)
        chance = 120;

    /* Wearing anything but a light shield makes it very awkward to cast a
       spell.  The penalty is not quite so bad for the player's role-specific
       spell. */
    if (arms && weight(arms) > (int)objects[SMALL_SHIELD].oc_weight) {
        if (you && spell == urole.spelspec) {
            chance /= 2;
        } else {
            chance /= 4;
        }
    }

    /* Finally, chance (based on player intell/wisdom and level) is combined
       with ability (based on player intrinsics and encumbrances).  No matter
       how intelligent/wise and advanced a player is, intrinsics and
       encumbrance can prevent casting; and no matter how able, learning is
       always required. */
    chance = chance * (20 - splcaster) / 15 - splcaster;

    /* Clamp to percentile */
    if (chance > 100)
        chance = 100;
    if (chance < 0)
        chance = 0;

    return chance;
}


/* Learn a spell during creation of the initial inventory */
void
initialspell(struct obj *obj)
{
    int i;

    for (i = -1; i < MAXSPELL; i++) {
        int j = i;
        if (i == -1)
            j = spellno_from_let(objects[obj->otyp].oc_defletter);
        if (spellid(j) == obj->otyp) {
            pline("Error: Spell %s already known.",
                  OBJ_NAME(objects[obj->otyp]));
            return;
        }
        if (spellid(j) == NO_SPELL) {
            spl_book[j].sp_id = obj->otyp;
            spl_book[j].sp_lev = objects[obj->otyp].oc_level;
            incrnknow(j);
            return;
        }
    }
    impossible("Too many spells memorized!");
    return;
}

/*spell.c*/
