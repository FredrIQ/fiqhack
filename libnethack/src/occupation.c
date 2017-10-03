/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-03 */
/* Copyright (c) Fredrik Ljungdahl, 2017. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Occupation code, handling multi-turn voluntary actions. */

char *
whybusy(const struct monst *mon)
{
    if (!mx_eocc(mon))
        panic("whybusy: no occupation?");

    return mon->mextra->eocc->whybusy;
}

void
set_whybusy(struct monst *mon, const char *str)
{
    if (!mx_eocc(mon))
        panic("set_whybusy: no occupation going on?");

    struct eocc *occ = mx_eocc(mon);
    strncpy(occ->whybusy, str, BUFSZ - 1);
}

int
mon_occupied(const struct monst *mon)
{
    struct eocc *occ = mx_eocc(mon);
    if (!occ)
        return 0;

    return occ->current;
}

/* Called from command implementations to indicate that they're multi-turn
   commands; all but the last turn should call this */
void
action_incomplete(const char *reason, enum occupation ocode)
{
    maction_incomplete(&youmonst, reason, ocode);
}

void
maction_incomplete(struct monst *mon, const char *reason, enum occupation ocode)
{
    struct eocc *occ = mx_eocc_new(mon);
    set_whybusy(mon, reason);
    occ->incomplete = TRUE;
    occ->current = ocode;
}

/*
 * Called from just about anywhere to abort an interruptible multi-turn
 * command. This happens even if the server doesn't consider a multi-turn
 * command to be in progress; the current API allows the client to substitute
 * its own definition if it wants to.
 *
 * The general rule about which action_ function to use is:
 * - action_interrupted: if something surprises the character that is unrelated
 *   to the command they input
 * - action_completed: if the interruption is due to the action that the
 *   character was attempting, either because of invalid input, or something
 *   that interrupts that command in particular (such as walking onto an item
 *   while exploring).
 *
 * The user interface is aware of the difference between these functions.
 * However, no present interface treats them differently (apart from the message
 * that action_interrupted prints).
 */
void
action_interrupted(void)
{
    maction_interrupted(&youmonst);
}

void
maction_interrupted(struct monst *mon)
{
    interrupt_occupation(mon, ocm_all);
}

void
interrupt_occupation(struct monst *mon, enum occupation_mask ocm)
{
    boolean you = (mon == &youmonst);
    boolean vis = (you || canseemon(mon));

    if (!(ocm & (1 << busy(mon))))
        return;

    mon->interrupted = TRUE;
    if (occ_incomplete(mon) && !mon->interrupted && vis)
        pline(you ? msgc_interrupted : msgc_monneutral,
              "%s %s.", M_verbs(mon, "stop"), whybusy(mon));
}

/* Called when an action is logically complete: it completes it and interrupts
   it with no message. For single-action commands, this effectively means "I
   know the action only takes one turn, but even if you gave a repeat count it
   shouldn't be repeated." For multi-action commands, this means "I thought this
   action might take more than one turn, but it didn't." */
void
action_completed(void)
{
    maction_completed(&youmonst);
}

void
maction_completed(struct monst *mon)
{
    mon->interrupted = TRUE;
    struct eocc *occ = mx_eocc(mon);
    if (!occ)
        return;

    occ->incomplete = FALSE;
    occ->current = occ_none;
}

boolean
occ_incomplete(struct monst *mon)
{
    struct eocc *occ = mx_eocc(mon);
    if (!occ)
        return FALSE;
    return occ->incomplete;
}

enum occupation
busy(const struct monst *mon)
{
    const struct eocc *occ = mx_eocc(mon);
    if (!occ)
        return occ_none;
    return occ->current;
}

/* Helper function for occupations. */
void
one_occupation_turn(int (*callback)(void), const char *gerund,
                    enum occupation ocode)
{
    action_incomplete(gerund, ocode);
    if (!callback())
        action_completed();
}


/*mextra.c*/
