/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int cost(struct obj *);

/* returns base cost of a scroll or a spellbook */
static int
cost(struct obj *otmp)
{

    if (otmp->oclass == SPBOOK_CLASS)
        return 10 * objects[otmp->otyp].oc_level;

    switch (otmp->otyp) {
    case SCR_LIGHT:
    case SCR_GOLD_DETECTION:
    case SCR_FOOD_DETECTION:
    case SCR_MAGIC_MAPPING:
    case SCR_AMNESIA:
    case SCR_FIRE:
    case SCR_EARTH:
        return 8;
/*       break; */
    case SCR_DESTROY_ARMOR:
    case SCR_CREATE_MONSTER:
    case SCR_PUNISHMENT:
        return 10;
/*      break; */
    case SCR_CONFUSE_MONSTER:
        return 12;
/*      break; */
    case SCR_IDENTIFY:
        return 14;
/*      break; */
    case SCR_ENCHANT_ARMOR:
    case SCR_REMOVE_CURSE:
    case SCR_ENCHANT_WEAPON:
    case SCR_CHARGING:
        return 16;
/*      break; */
    case SCR_SCARE_MONSTER:
    case SCR_STINKING_CLOUD:
    case SCR_TAMING:
    case SCR_TELEPORTATION:
        return 20;
/*      break; */
    case SCR_GENOCIDE:
        return 30;
/*      break; */
    case SCR_BLANK_PAPER:
    default:
        impossible("You can't write such a weird scroll!");
    }
    return 1000;
}

static const char write_on[] = { SCROLL_CLASS, SPBOOK_CLASS, 0 };

int
dowrite(struct obj *pen, const struct nh_cmd_arg *arg)
{
    struct obj *paper;
    const char *namebuf, *nm, *bp;
    struct obj *new_obj;
    int basecost, actualcost;
    int curseval;
    const char *qbuf;
    int first, last, i;
    boolean by_descr = FALSE, by_name = FALSE;
    const char *typeword;

    if (nohands(youmonst.data)) {
        pline(msgc_cancelled, "You need hands to be able to write!");
        return 0;
    } else if (Glib) {
        pline(msgc_badidea, "%s from your %s.", Tobjnam(pen, "slip"),
              makeplural(body_part(FINGER)));
        unwield_silently(pen);
        dropx(pen);
        return 1;
    }

    /* get paper to write on */
    paper = getargobj(arg, write_on, "write on");
    if (!paper)
        return 0;

    typeword = (paper->oclass == SPBOOK_CLASS) ? "spellbook" : "scroll";
    if (Blind && !paper->dknown) {
        pline(msgc_cancelled1,
              "You don't know if that %s is blank or not!", typeword);
        return 1;
    }
    paper->dknown = 1;
    if (paper->otyp != SCR_BLANK_PAPER && paper->otyp != SPE_BLANK_PAPER) {
        pline(msgc_cancelled1, "That %s is not blank!", typeword);
        exercise(A_WIS, FALSE);
        return 1;
    }

    /* what to write */
    qbuf = msgprintf("What type of %s do you want to write?", typeword);
    namebuf = getarglin(arg, qbuf);
    namebuf = msgmungspaces(namebuf);   /* remove any excess whitespace */
    if (namebuf[0] == '\033' || !namebuf[0])
        return 1;
    nm = namebuf;
    if (!strncmpi(nm, "scroll ", 7))
        nm += 7;
    else if (!strncmpi(nm, "spellbook ", 10))
        nm += 10;
    if (!strncmpi(nm, "of ", 3))
        nm += 3;

    if ((bp = strstri(nm, " armour")) != 0)
        nm = msgcat_many(msgchop(nm, bp-nm), " armor", bp+7, NULL);

    first = bases[(int)paper->oclass];
    last = bases[(int)paper->oclass + 1] - 1;
    for (i = first; i <= last; i++) {
        /* extra shufflable descr not representing a real object */
        if (!OBJ_NAME(objects[i]))
            continue;

        if (!strcmpi(OBJ_NAME(objects[i]), nm))
            goto found;
        if (!strcmpi(OBJ_DESCR(objects[i]), nm)) {
            by_descr = TRUE;
            goto found;
        }
        if (objects[i].oc_uname &&
            !strcmpi(objects[i].oc_uname, nm)) {
            by_name = TRUE;
            goto found;
        }
    }

    pline(msgc_cancelled1, "There is no such %s!", typeword);
    return 1;
found:

    if (i == SCR_BLANK_PAPER || i == SPE_BLANK_PAPER) {
        pline(msgc_cancelled1, "You can't write that!");
        pline(msgc_cancelled1, "It's obscene!");
        return 1;
    } else if (i == SPE_BOOK_OF_THE_DEAD) {
        pline(msgc_cancelled1, "No mere dungeon adventurer could write that.");
        return 1;
    } else if ((by_descr || by_name) && paper->oclass == SPBOOK_CLASS &&
               !objects[i].oc_name_known) {
        /* can't write unknown spellbooks by description */
        pline(msgc_cancelled1,
              "Unfortunately you don't have enough information to go on.");
        return 1;
    }

    /* KMH, conduct */
    break_conduct(conduct_illiterate);

    new_obj = mksobj(level, i, FALSE, FALSE, rng_main);
    new_obj->bknown = (paper->bknown && pen->bknown);

    /* shk imposes a flat rate per use, not based on actual charges used */
    check_unpaid(pen);

    /* see if there's enough ink */
    basecost = cost(new_obj);
    if (pen->spe < basecost / 2) {
        pline(msgc_failcurse, "Your marker is too dry to write that!");
        obfree(new_obj, NULL);
        return 1;
    }

    /* we're really going to write now, so calculate cost

       no custom RNG used: too much influence from player actions */
    actualcost = rn1(basecost / 2, basecost / 2);
    curseval = bcsign(pen) + bcsign(paper);
    exercise(A_WIS, TRUE);
    /* dry out marker */
    if (pen->spe < actualcost) {
        pen->spe = 0;
        pline(msgc_itemloss, "Your marker dries out!");
        /* scrolls disappear, spellbooks don't */
        if (paper->oclass == SPBOOK_CLASS) {
            pline(msgc_failcurse,
                  "The spellbook is left unfinished and your writing fades.");
            update_inventory(); /* pen charges */
        } else {
            pline(msgc_failcurse, "The scroll is now useless and disappears!");
            useup(paper);
        }
        obfree(new_obj, NULL);
        return 1;
    }
    pen->spe -= actualcost;

    /* can't write if we don't know it - unless we're lucky */
    if (!(objects[new_obj->otyp].oc_name_known) &&
        (rnl(Role_if(PM_WIZARD) ? 3 : 15))) {
        pline(msgc_failrandom, "You %s to write that!",
              by_descr ? "fail" : "don't know how");
        /* scrolls disappear, spellbooks don't */
        if (paper->oclass == SPBOOK_CLASS) {
            pline_implied(msgc_failrandom,
                          "You write in your best handwriting:  "
                          "\"My Diary\", but it quickly fades.");
            update_inventory(); /* pen charges */
        } else {
            const char *written;
            if (by_descr) {
                written = OBJ_DESCR(objects[new_obj->otyp]);
                written = eroded_text(written,
                                      (6 + MAXULEV - u.ulevel) / 6, 0);
            } else
                written = msgprintf("%s was here!", u.uplname);
            pline_implied(msgc_failrandom,
                          "You write \"%s\" and the scroll disappears.",
                          written);
            useup(paper);
        }
        obfree(new_obj, NULL);
        return 1;
    }

    /* useup old scroll / spellbook */
    useup(paper);

    /* success */
    if (new_obj->oclass == SPBOOK_CLASS) {
        /* acknowledge the change in the object's description... */
        pline(msgc_actionok, "The spellbook warps strangely, then turns %s.",
              OBJ_DESCR(objects[new_obj->otyp]));
    }
    new_obj->blessed = (curseval > 0);
    new_obj->cursed = (curseval < 0);
    hold_another_object(new_obj, "Oops!  %s out of your grasp!",
                        The(aobjnam(new_obj, "slip")), NULL);
    return 1;
}

/*write.c*/

