/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-02-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"    /* ALLOW_M */

#define URETREATING(x,y) (distmin(youmonst.mx,youmonst.my,x,y) > distmin(u.ux0,u.uy0,x,y))

#define POLE_LIM 5      /* How far monsters can use pole-weapons */

/*
 * Keep consistent with breath weapons in zap.c, and AD_* in monattk.h.
 */
static const char *const breathwep[] = {
    "fragments",
    "fire",
    "frost",
    "sleep gas",
    "a disintegration blast",
    "lightning",
    "a poisonous cloud",
    "acid",
    "a disorienting blast",
    "strange breath #9"
};

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

/* Remove an item from the monster's inventory and destroy it. */
void
m_useup(struct monst *mon, struct obj *obj)
{
    if (obj->quan > 1L) {
        obj->in_use = FALSE; /* no longer used */
        obj->quan--;
        obj->owt = weight(obj);
    } else {
        obj_extract_self(obj);
        possibly_unwield(mon, FALSE);
        if (obj->owornmask) {
            mon->misc_worn_check &= ~obj->owornmask;
            if (obj->otyp == SADDLE && mon == u.usteed)
                dismount_steed(DISMOUNT_FELL);
            update_property(mon, objects[obj->otyp].oc_oprop, which_slot(obj));
        }
        obfree(obj, NULL);
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
        && distmin(dx, dy, 0, 0) < BOLT_LIM) {
        if (clear_path(ax, ay, bx, by, viz_array))
            return TRUE;
    }
    return FALSE;
}

/* Check if a monster is carrying a particular item. */
struct obj *
m_carrying(const struct monst *mon, int type)
{
    return m_carrying_recursive(mon, m_minvent(mon),
                                type, FALSE);
}

/* Check if a monster is carrying a particular item, recursively. */
struct obj *
m_carrying_recursive(const struct monst *mon, struct obj *chain,
                     int type, boolean recursive)
{
    struct obj *otmp;
    struct obj *cotmp = NULL;

    for (otmp = chain; otmp; otmp = otmp->nobj) {
        if (Has_contents(otmp) && recursive)
            cotmp = m_carrying_recursive(mon, otmp->cobj, type, recursive);
        if (otmp->otyp == type)
            return otmp;
    }
    return cotmp ? cotmp : NULL;
}

/* TRUE iff thrown/kicked/rolled object doesn't pass through iron bars */
boolean
hits_bars(struct obj ** obj_p, /* *obj_p will be set to NULL if object breaks */
          int x, int y, int always_hit, /* caller can force a hit for items
                                           which would fit through */
          int whodidit)
{       /* 1==hero, 0=other, -1==just check whether it'll pass thru */
    struct obj *otmp = *obj_p;
    int obj_type = otmp->otyp;
    boolean hits = always_hit;

    if (!hits)
        switch (otmp->oclass) {
        case WEAPON_CLASS:
            {
                int oskill = objects[obj_type].oc_skill;

                hits = (oskill != -P_BOW && oskill != -P_CROSSBOW &&
                        oskill != -P_DART && oskill != -P_SHURIKEN &&
                        oskill != P_SPEAR && oskill != P_JAVELIN &&
                        oskill != P_KNIFE); /* but not dagger */
                break;
            }
        case ARMOR_CLASS:
            hits = (objects[obj_type].oc_armcat != ARM_GLOVES);
            break;
        case TOOL_CLASS:
            hits = (obj_type != SKELETON_KEY && obj_type != LOCK_PICK &&
                    obj_type != CREDIT_CARD && obj_type != TALLOW_CANDLE &&
                    obj_type != WAX_CANDLE && obj_type != LENSES &&
                    obj_type != TIN_WHISTLE && obj_type != MAGIC_WHISTLE);
            break;
        case ROCK_CLASS:       /* includes boulder */
            if (obj_type != STATUE || mons[otmp->corpsenm].msize > MZ_TINY)
                hits = TRUE;
            break;
        case FOOD_CLASS:
            if (obj_type == CORPSE && mons[otmp->corpsenm].msize > MZ_TINY)
                hits = TRUE;
            else
                hits = (obj_type == MEAT_STICK ||
                        obj_type == HUGE_CHUNK_OF_MEAT);
            break;
        case SPBOOK_CLASS:
        case WAND_CLASS:
        case BALL_CLASS:
        case CHAIN_CLASS:
            hits = TRUE;
            break;
        default:
            break;
        }

    if (hits && whodidit != -1)
        hits_bars_hit(obj_p, x, y, whodidit);

    return hits;
}

/* Split from hits_bars to allow additional bookkeeping before destroying the obj */
void
hits_bars_hit(struct obj **obj_p, int x, int y, int whodidit)
{
    struct obj *obj = *obj_p;
    if (whodidit ? hero_breaks(obj, x, y, FALSE) : breaks(obj, x, y))
        *obj_p = obj = NULL; /* object is now gone */
    else if (obj->otyp == BOULDER || obj->otyp == HEAVY_IRON_BALL)
        pline(msgc_levelsound, "Whang!");
    else if (obj->oclass == COIN_CLASS ||
             objects[obj->otyp].oc_material == GOLD ||
             objects[obj->otyp].oc_material == SILVER)
        pline(msgc_levelsound, "Clink!");
    else
        pline(msgc_levelsound, "Clonk!");
}

/*mthrowu.c*/
