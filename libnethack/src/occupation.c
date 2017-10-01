/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-02 */
/* Copyright (c) Fredrik Ljungdahl, 2017. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Occupation code, handling multi-turn voluntary actions. */

char *
whybusy(const struct monst *mon)
{
    if (!mx_eocc(mon)) {
        impossible("whybusy: no occupation?");
        return NULL;
    }

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
    set_whybusy(&youmonst, reason);
    flags.incomplete = TRUE;
    flags.occupation = ocode;
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
    if (flags.incomplete && !flags.interrupted)
        pline(msgc_interrupted, "You stop %s.", whybusy(&youmonst));
    mx_eocc_free(&youmonst);
    flags.interrupted = TRUE;
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
        panic("maction_complete: no eocc?");

    mx_eocc_free(mon);
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
