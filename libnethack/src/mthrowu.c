/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-02-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"    /* ALLOW_M */

#define URETREATING(x,y) (distmin(youmonst.mx,youmonst.my,x,y) > distmin(u.ux0,u.uy0,x,y))

#define POLE_LIM 5      /* How far monsters can use pole-weapons */

/* Hero is hit by something other than a monster.

   TODO: track whether a monster is responsible */
int
thitu(int tlev, int dam, struct obj *obj, const char *name)
{       /* if null, then format `obj' */
    const char *onm, *killer;
    boolean is_acid;

    /* TODO: credit the monster that fired the object with the kill */
    if (!name) {
        if (!obj)
            panic("thitu: name & obj both null?");
        name = doname(obj);
        killer = killer_msg_obj(DIED, obj);
    } else {
        killer = killer_msg(DIED, an(name));
    }
    onm = (obj && obj_is_pname(obj)) ? the(name) :
      (obj && obj->quan > 1L) ? name : an(name);
    is_acid = (obj && obj->otyp == ACID_VENOM);

    if (find_mac(&youmonst) + tlev <= rnd(20)) {
        if (Blind || !flags.verbose)
            pline(msgc_nonmongood, "It misses.");
        else
            pline(msgc_nonmongood, "You are almost hit by %s.", onm);
        return 0;
    } else {
        if (Blind || !flags.verbose)
            pline(msgc_nonmonbad, "You are hit!");
        else
            pline(msgc_nonmonbad, "You are hit by %s%s", onm, exclam(dam));

        if (obj && objects[obj->otyp].oc_material == SILVER &&
            hates_silver(youmonst.data)) {
            dam += rnd(20);
            pline(msgc_statusbad, "The silver sears your flesh!");
            exercise(A_CON, FALSE);
        }
        if (is_acid && Acid_resistance)
            pline(msgc_playerimmune, "It doesn't seem to hurt you.");
        else {
            if (is_acid)
                pline(msgc_statusbad, "It burns!");
            if (Half_physical_damage)
                dam = (dam + 1) / 2;
            losehp(dam, killer);
            exercise(A_STR, FALSE);
        }
        return 1;
    }
}

boolean
linedup(xchar ax, xchar ay, xchar bx, xchar by)
{
    int dx = ax - bx;
    int dy = ay - by;

    /* sometimes displacement makes a monster think that you're at its own
       location; prevent it from throwing and zapping in that case */
    if (!dx && !dy)
        return FALSE;

    if ((!dx || !dy || abs(dx) == abs(dy))  /* straight line or diagonal */
        && distmin(dx, dy, 0, 0) <= BOLT_LIM) {
        if (clear_path(ax, ay, bx, by, viz_array))
            return TRUE;
    }
    return FALSE;
}

/*mthrowu.c*/
