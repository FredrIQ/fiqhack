/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) 1989 by Jean-Christophe Collet                   */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the drawbridge manipulation (create, open, close,
 * destroy).
 *
 * Added comprehensive monster-handling, and the "entity" structure to
 * deal with players as well. - 11/89
 */

#include "hack.h"

static void get_wall_for_db(int *, int *);
static struct entity *e_at(int, int);
static void m_to_e(struct monst *, int, int, struct entity *);
static void u_to_e(struct entity *);
static void set_entity(int, int, struct entity *);
static const char *e_nam(struct entity *);
static const char *E_phrase(struct entity *, const char *);
static boolean e_survives_at(struct entity *, int, int);
static void e_died(struct entity *, int, int, const char *);
static boolean automiss(struct entity *);
static boolean e_missed(struct entity *, boolean);
static boolean e_jumps(struct entity *);
static void do_entity(struct entity *);

boolean
is_pool(struct level *lev, int x, int y)
{
    schar ltyp;

    if (!isok(x, y))
        return FALSE;
    ltyp = lev->locations[x][y].typ;
    /* The ltyp == MOAT is not redundant to is_moat due to drawbridges and the
       Juiblex level. There is probably a better way to express this. */
    if (ltyp == POOL || ltyp == WATER || ltyp == MOAT || is_moat(lev, x, y))
        return TRUE;
    return FALSE;
}

boolean
is_lava(struct level * lev, int x, int y)
{
    schar ltyp;

    if (!isok(x, y))
        return FALSE;
    ltyp = lev->locations[x][y].typ;
    if (ltyp == LAVAPOOL ||
        (ltyp == DRAWBRIDGE_UP &&
         (lev->locations[x][y].drawbridgemask & DB_UNDER) == DB_LAVA))
        return TRUE;
    return FALSE;
}

boolean
is_ice(struct level * lev, int x, int y)
{
    schar ltyp;

    if (!isok(x, y))
        return FALSE;
    ltyp = lev->locations[x][y].typ;
    if (ltyp == ICE ||
        (ltyp == DRAWBRIDGE_UP &&
         (lev->locations[x][y].drawbridgemask & DB_UNDER) == DB_ICE))
        return TRUE;
    return FALSE;
}

boolean
is_moat(struct level * lev, int x, int y)
{
    schar ltyp;

    if (!isok(x, y))
        return FALSE;
    ltyp = lev->locations[x][y].typ;
    if (!Is_juiblex_level(&lev->z) &&
        (ltyp == MOAT ||
         (ltyp == DRAWBRIDGE_UP &&
          (lev->locations[x][y].drawbridgemask & DB_UNDER) == DB_MOAT)))
        return TRUE;
    return FALSE;
}

/*
 * We want to know whether a wall (or a door) is the portcullis (passageway)
 * of an eventual drawbridge.
 *
 * Return value:  the direction of the drawbridge.
 */

int
is_drawbridge_wall(int x, int y)
{
    struct rm *loc;

    loc = &level->locations[x][y];
    if (loc->typ != DOOR && loc->typ != DBWALL)
        return -1;

    if (IS_DRAWBRIDGE(level->locations[x + 1][y].typ) &&
        (level->locations[x + 1][y].drawbridgemask & DB_DIR) == DB_WEST)
        return DB_WEST;
    if (IS_DRAWBRIDGE(level->locations[x - 1][y].typ) &&
        (level->locations[x - 1][y].drawbridgemask & DB_DIR) == DB_EAST)
        return DB_EAST;
    if (IS_DRAWBRIDGE(level->locations[x][y - 1].typ) &&
        (level->locations[x][y - 1].drawbridgemask & DB_DIR) == DB_SOUTH)
        return DB_SOUTH;
    if (IS_DRAWBRIDGE(level->locations[x][y + 1].typ) &&
        (level->locations[x][y + 1].drawbridgemask & DB_DIR) == DB_NORTH)
        return DB_NORTH;

    return -1;
}

/*
 * Use is_db_wall where you want to verify that a
 * drawbridge "wall" is UP in the location x, y
 * (instead of UP or DOWN, as with is_drawbridge_wall).
 */
boolean
is_db_wall(int x, int y)
{
    return (boolean) (level->locations[x][y].typ == DBWALL);
}


/*
 * Return true with x,y pointing to the drawbridge if x,y initially indicate
 * a drawbridge or drawbridge wall.
 */
boolean
find_drawbridge(int *x, int *y)
{
    int dir;

    if (IS_DRAWBRIDGE(level->locations[*x][*y].typ))
        return TRUE;
    dir = is_drawbridge_wall(*x, *y);
    if (dir >= 0) {
        switch (dir) {
        case DB_NORTH:
            (*y)++;
            break;
        case DB_SOUTH:
            (*y)--;
            break;
        case DB_EAST:
            (*x)--;
            break;
        case DB_WEST:
            (*x)++;
            break;
        }
        return TRUE;
    }
    return FALSE;
}


/*
 * Find the drawbridge wall associated with a drawbridge.
 */
static void
get_wall_for_db(int *x, int *y)
{
    switch (level->locations[*x][*y].drawbridgemask & DB_DIR) {
    case DB_NORTH:
        (*y)--;
        break;
    case DB_SOUTH:
        (*y)++;
        break;
    case DB_EAST:
        (*x)++;
        break;
    case DB_WEST:
        (*x)--;
        break;
    }
}

/*
 * Creation of a drawbridge at pos x,y.
 *     dir is the direction.
 *     flag must be put to TRUE if we want the drawbridge to be opened.
 */

boolean
create_drawbridge(struct level *lev, int x, int y, int dir, boolean flag)
{
    int x2, y2;
    boolean horiz;
    boolean lava = lev->locations[x][y].typ == LAVAPOOL; /* assume initialized
                                                            map */

    x2 = x;
    y2 = y;
    switch (dir) {
    case DB_NORTH:
        horiz = TRUE;
        y2--;
        break;
    case DB_SOUTH:
        horiz = TRUE;
        y2++;
        break;
    case DB_EAST:
        horiz = FALSE;
        x2++;
        break;
    default:
        impossible("bad direction in create_drawbridge");
        /* fall through */
    case DB_WEST:
        horiz = FALSE;
        x2--;
        break;
    }
    if (!IS_WALL(lev->locations[x2][y2].typ))
        return FALSE;
    if (flag) { /* We want the bridge open */
        lev->locations[x][y].typ = DRAWBRIDGE_DOWN;
        lev->locations[x2][y2].typ = DOOR;
        lev->locations[x2][y2].doormask = D_NODOOR;
    } else {
        lev->locations[x][y].typ = DRAWBRIDGE_UP;
        lev->locations[x2][y2].typ = DBWALL;
        /* Drawbridges are non-diggable. */
        lev->locations[x2][y2].wall_info = W_NONDIGGABLE;
    }
    lev->locations[x][y].horizontal = !horiz;
    lev->locations[x2][y2].horizontal = horiz;
    lev->locations[x][y].drawbridgemask = dir;
    if (lava)
        lev->locations[x][y].drawbridgemask |= DB_LAVA;
    return TRUE;
}

struct entity {
    struct monst *emon; /* youmonst for the player */
    const struct permonst *edata;       /* must be non-zero for record to be
                                           valid */
    int ex, ey;
};

#define ENTITIES 2

static struct entity occupants[ENTITIES];

static struct entity *
e_at(int x, int y)
{
    int entitycnt;

    for (entitycnt = 0; entitycnt < ENTITIES; entitycnt++)
        if ((occupants[entitycnt].edata) && (occupants[entitycnt].ex == x) &&
            (occupants[entitycnt].ey == y))
            break;
    return ((entitycnt == ENTITIES) ? NULL : &(occupants[entitycnt]));
}

static void
m_to_e(struct monst *mtmp, int x, int y, struct entity *etmp)
{
    etmp->emon = mtmp;
    if (mtmp) {
        etmp->ex = x;
        etmp->ey = y;
        if (mtmp->wormno && (x != mtmp->mx || y != mtmp->my))
            etmp->edata = &mons[PM_LONG_WORM_TAIL];
        else
            etmp->edata = mtmp->data;
    } else
        etmp->edata = NULL;
}

static void
u_to_e(struct entity *etmp)
{
    etmp->emon = &youmonst;
    etmp->ex = u.ux;
    etmp->ey = u.uy;
    etmp->edata = youmonst.data;
}

static void
set_entity(int x, int y, struct entity *etmp)
{
    if ((x == u.ux) && (y == u.uy))
        u_to_e(etmp);
    else if (MON_AT(level, x, y))
        m_to_e(m_at(level, x, y), x, y, etmp);
    else
        etmp->edata = NULL;
}

#define is_u(etmp) (etmp->emon == &youmonst)
#define e_canseemon(etmp) (is_u(etmp) ? (boolean)TRUE : canseemon(etmp->emon))

/*
 * e_strg is a utility routine which is not actually in use anywhere, since
 * the specialized routines below suffice for all current purposes.
 */

/* #define e_strg(etmp, func) (is_u(etmp)? NULL : func(etmp->emon)) */

static const char *
e_nam(struct entity *etmp)
{
    return is_u(etmp) ? "you" : mon_nam(etmp->emon);
}

/*
 * Generates capitalized entity name, makes 2nd -> 3rd person conversion on
 * verb, where necessary.
 */
static const char *
E_phrase(struct entity *etmp, const char *verb)
{
    static char wholebuf[80];

    strcpy(wholebuf, is_u(etmp) ? "You" : Monnam(etmp->emon));
    if (!*verb)
        return wholebuf;
    strcat(wholebuf, " ");
    if (is_u(etmp))
        strcat(wholebuf, verb);
    else
        strcat(wholebuf, vtense(NULL, verb));
    return wholebuf;
}

/*
 * Simple-minded "can it be here?" routine
 */
static boolean
e_survives_at(struct entity *etmp, int x, int y)
{
    if (noncorporeal(etmp->edata))
        return TRUE;
    if (is_pool(level, x, y))
        return (boolean) ((is_u(etmp) &&
                           (Wwalking || Amphibious || Swimming || Flying ||
                            Levitation)) || is_swimmer(etmp->edata) ||
                          is_flyer(etmp->edata) || is_floater(etmp->edata));
    /* must force call to lava_effects in e_died if is_u */
    if (is_lava(level, x, y))
        return (boolean) ((is_u(etmp) && (Levitation || Flying)) ||
                          likes_lava(etmp->edata) || is_flyer(etmp->edata));
    if (is_db_wall(x, y))
        return ((boolean)
                (is_u(etmp) ? Passes_walls : passes_walls(etmp->edata)));
    return TRUE;
}

static void
e_died(struct entity *etmp, int dest, int how, const char *killer)
{
    if (is_u(etmp)) {
        if (how == DROWNING) {
            drown();
        } else if (how == BURNING) {
            lava_effects();
        } else {
            coord xy;

            done(how, killer);
            /* So, you didn't die */
            if (!e_survives_at(etmp, etmp->ex, etmp->ey)) {
                if (enexto(&xy, level, etmp->ex, etmp->ey, etmp->edata)) {
                    pline(msgc_statusheal, "A %s force teleports you away...",
                          Hallucination ? "normal" : "strange");
                    teleds(xy.x, xy.y, FALSE);
                }
                /* otherwise on top of the drawbridge is the only viable spot
                   in the dungeon, so stay there */
            }
        }
        /* we might have crawled out of the moat to survive */
        etmp->ex = u.ux, etmp->ey = u.uy;
    } else {
        int entitycnt;

        /* fake "digested to death" damage-type suppresses corpse */
#define mk_message(dest) ((dest & 1) ? "" : NULL)
#define mk_corpse(dest)  ((dest & 2) ? AD_DGST : AD_PHYS)
        /* if monsters are moving, one of them caused the destruction */
        if (flags.mon_moving)
            monkilled(NULL, etmp->emon, mk_message(dest), mk_corpse(dest));
        else    /* you caused it */
            xkilled(etmp->emon, dest);
        etmp->edata = NULL;

        /* dead long worm handling */
        for (entitycnt = 0; entitycnt < ENTITIES; entitycnt++) {
            if (etmp != &(occupants[entitycnt]) &&
                etmp->emon == occupants[entitycnt].emon)
                occupants[entitycnt].edata = NULL;
        }
#undef mk_message
#undef mk_corpse
    }
}


/*
 * These are never directly affected by a bridge or portcullis.
 */
static boolean
automiss(struct entity *etmp)
{
    return (boolean) ((is_u(etmp) ? Passes_walls : passes_walls(etmp->edata)) ||
                      noncorporeal(etmp->edata));
}

/*
 * Does falling drawbridge or portcullis miss etmp?
 */
static boolean
e_missed(struct entity *etmp, boolean chunks)
{
    int misses;

    if (automiss(etmp))
        return TRUE;

    if (is_flyer(etmp->edata) &&
        (is_u(etmp) ? !Sleeping
         : (etmp->emon->mcanmove && !etmp->emon->msleeping)))
        /* flying requires mobility */
        misses = 5;     /* out of 8 */
    else if (is_floater(etmp->edata) || (is_u(etmp) && Levitation))
        /* doesn't require mobility */
        misses = 3;
    else if (chunks && is_pool(level, etmp->ex, etmp->ey))
        misses = 2;     /* sitting ducks */
    else
        misses = 0;

    if (is_db_wall(etmp->ex, etmp->ey))
        misses -= 3;    /* less airspace */

    enum rng rng = is_u(etmp) ? rng_survive_dbridge : rng_main;

    return (boolean) ((misses > rn2_on_rng(8, rng)) ? TRUE : FALSE);
}

/*
 * Can etmp jump from death?
 */
static boolean
e_jumps(struct entity *etmp)
{
    int tmp = 4;        /* out of 10 */

    if (is_u(etmp) ? (Sleeping || Fumbling)
        : (!etmp->emon->mcanmove || etmp->emon->msleeping || !etmp->edata->mmove
           || etmp->emon->wormno))
        return FALSE;

    if (is_u(etmp) ? Confusion : etmp->emon->mconf)
        tmp -= 2;

    if (is_u(etmp) ? Stunned : etmp->emon->mstun)
        tmp -= 3;

    if (is_db_wall(etmp->ex, etmp->ey))
        tmp -= 2;       /* less room to maneuver */

    enum rng rng = is_u(etmp) ? rng_survive_dbridge : rng_main;

    return (boolean) ((tmp > rn2_on_rng(10, rng)) ? TRUE : FALSE);
}

static void
do_entity(struct entity *etmp)
{
    int newx, newy, at_portcullis, oldx, oldy;
    boolean must_jump = FALSE, relocates = FALSE, e_inview;
    struct rm *crm;

    if (!etmp->edata)
        return;

    /* TODO: Long worms? */
    e_inview = e_canseemon(etmp);
    oldx = etmp->ex;
    oldy = etmp->ey;
    at_portcullis = is_db_wall(oldx, oldy);
    crm = &level->locations[oldx][oldy];

    enum msg_channel miss_msgc, hit_msgc;
    if (is_u(etmp)) {
        miss_msgc = msgc_fatalavoid;
        hit_msgc = msgc_fatal_predone;
    } else if (etmp->emon->mtame && canspotmon(etmp->emon))
        miss_msgc = hit_msgc = msgc_petfatal;
    else
        miss_msgc = hit_msgc = msgc_monneutral;

    if (automiss(etmp) && e_survives_at(etmp, oldx, oldy)) {
        if (e_inview && (at_portcullis || IS_DRAWBRIDGE(crm->typ)))
            pline(miss_msgc, "The %s passes through %s!",
                  at_portcullis ? "portcullis" : "drawbridge", e_nam(etmp));
        if (is_u(etmp))
            spoteffects(FALSE);
        return;
    }
    if (e_missed(etmp, FALSE)) {
        if (at_portcullis)
            pline(miss_msgc, "The portcullis misses %s!", e_nam(etmp));
        if (e_survives_at(etmp, oldx, oldy))
            return;
        else {
            if (at_portcullis)
                must_jump = TRUE;
            else
                relocates = TRUE;       /* just ride drawbridge in */
        }
    } else {
        if (crm->typ == DRAWBRIDGE_DOWN) {
            pline(hit_msgc, "%s crushed underneath the drawbridge.",
                  E_phrase(etmp, "are")); /* no jump */
            e_died(etmp, e_inview ? 3 : 2, CRUSHING,
                   killer_msg(CRUSHING, "a falling drawbridge"));  /* no corpse */
            return;     /* Note: Beyond this point, we know we're */
        }       /* not at an opened drawbridge, since all */
        must_jump = TRUE;       /* *missable* creatures survive on the */
    }   /* square, and all the unmissed ones die.  */
    if (must_jump) {
        if (at_portcullis) {
            if (e_jumps(etmp)) {
                relocates = TRUE;
            } else {
                if (e_inview)
                    pline(hit_msgc, "%s crushed by the falling portcullis!",
                          E_phrase(etmp, "are"));
                else
                    You_hear(msgc_levelsound, "a crushing sound.");
                e_died(etmp, e_inview ? 3 : 2, CRUSHING,
                       killer_msg(CRUSHING, "a falling portcullis"));
                /* no corpse */
                return;
            }
        } else {        /* tries to jump off bridge to original square */
            relocates = !e_jumps(etmp);
        }
    }

    /* Here's where we try to do relocation. Assumes that etmp is not arriving
       at the portcullis square while the drawbridge is falling, since this
       square would be inaccessible (i.e. etmp started on drawbridge square) or
       unnecessary (i.e. etmp started here) in such a situation. */
    newx = oldx;
    newy = oldy;
    find_drawbridge(&newx, &newy);
    if ((newx == oldx) && (newy == oldy))
        get_wall_for_db(&newx, &newy);
    if (relocates && (e_at(newx, newy))) {

        /* Standoff problem: one or both entities must die, and/or both switch
           places. Avoid infinite recursion by checking first whether the other
           entity is staying put. Clean up if we happen to move/die in
           recursion. */
        struct entity *other;

        other = e_at(newx, newy);
        if (e_survives_at(other, newx, newy) && automiss(other)) {
            relocates = FALSE;  /* "other" won't budge */
        } else {

            while ((e_at(newx, newy) != 0) && (e_at(newx, newy) != etmp))
                do_entity(other);
            if (e_at(oldx, oldy) != etmp) {
                return;
            }
        }
    }
    if (relocates && !e_at(newx, newy)) {       /* if e_at() entity = worm tail 
                                                 */
        if (!is_u(etmp)) {
            remove_monster(level, etmp->ex, etmp->ey);
            place_monster(etmp->emon, newx, newy);
            update_monster_region(etmp->emon);
        } else {
            u.ux = newx;
            u.uy = newy;
        }
        etmp->ex = newx;
        etmp->ey = newy;
        e_inview = e_canseemon(etmp);
    }
    if (is_db_wall(etmp->ex, etmp->ey)) {
        if (e_inview) {
            if (is_u(etmp)) {
                pline(msgc_nonmonbad,
                      "You tumble towards the closed portcullis!");
                if (automiss(etmp))
                    pline(miss_msgc, "You pass through it!");
                else
                    pline(e_survives_at(etmp, etmp->ex, etmp->ey) ?
                          miss_msgc : hit_msgc, "The drawbridge closes in...");
            } else
                pline(hit_msgc, "%s behind the drawbridge.",
                      E_phrase(etmp, "disappear"));
        }
        if (!e_survives_at(etmp, etmp->ex, etmp->ey)) {
            e_died(etmp, 0, CRUSHING,
                   killer_msg(CRUSHING, "a closing drawbridge"));
            return;
        }
    } else {
        if (is_pool(level, etmp->ex, etmp->ey) && !e_inview)
            You_hear(msgc_levelsound, "a splash.");
        if (e_survives_at(etmp, etmp->ex, etmp->ey)) {
            if (e_inview && !is_flyer(etmp->edata) && !is_floater(etmp->edata))
                pline(miss_msgc, "%s from the bridge.", E_phrase(etmp, "fall"));
            return;
        }
        if (is_pool(level, etmp->ex, etmp->ey) ||
            is_lava(level, etmp->ex, etmp->ey))
            if (e_inview && !is_u(etmp)) {
                /* drown() will supply msgs if nec. */
                boolean lava = is_lava(level, etmp->ex, etmp->ey);

                if (Hallucination)
                    pline(hit_msgc, "%s the %s and disappears.",
                          E_phrase(etmp, "drink"),
                          lava ? "lava" : waterbody_name(etmp->ex, etmp->ey));
                else
                    pline(hit_msgc, "%s into the %s.", E_phrase(etmp, "fall"),
                          lava ? "lava" : waterbody_name(etmp->ex, etmp->ey));
            }
        e_died(etmp, e_inview ? 3 : 2,  /* CRUSHING is arbitrary */
               (is_pool(level, etmp->ex, etmp->ey)) ? DROWNING :
               (is_lava(level, etmp->ex, etmp->ey)) ? BURNING :
               CRUSHING, "fell from a drawbridge");    /* no corpse */
        return;
    }
}

/*
 * Close the drawbridge located at x,y
 */
void
close_drawbridge(int x, int y)
{
    struct rm *loc1, *loc2;
    struct trap *t;
    int x2, y2;

    loc1 = &level->locations[x][y];
    if (loc1->typ != DRAWBRIDGE_DOWN)
        return;
    x2 = x;
    y2 = y;
    get_wall_for_db(&x2, &y2);
    if (cansee(x, y) || cansee(x2, y2))
        pline(msgc_actionok, "You see a drawbridge %s up!",
              (((u.ux == x || u.uy == y) && !Underwater) ||
               distu(x2, y2) < distu(x, y)) ? "coming" : "going");
    loc1->typ = DRAWBRIDGE_UP;
    loc2 = &level->locations[x2][y2];
    loc2->typ = DBWALL;
    switch (loc1->drawbridgemask & DB_DIR) {
    case DB_NORTH:
    case DB_SOUTH:
        loc2->horizontal = TRUE;
        break;
    case DB_WEST:
    case DB_EAST:
        loc2->horizontal = FALSE;
        break;
    }
    loc2->wall_info = W_NONDIGGABLE;
    set_entity(x, y, &(occupants[0]));
    set_entity(x2, y2, &(occupants[1]));
    do_entity(&(occupants[0])); /* Do set_entity after first */
    set_entity(x2, y2, &(occupants[1]));        /* do_entity for worm tail */
    do_entity(&(occupants[1]));
    if (OBJ_AT(x, y))
        You_hear(msgc_levelsound, "smashing and crushing.");
    revive_nasty(x, y, NULL);
    revive_nasty(x2, y2, NULL);
    delallobj(x, y);
    delallobj(x2, y2);
    if ((t = t_at(level, x, y)) != 0)
        deltrap(level, t);
    if ((t = t_at(level, x2, y2)) != 0)
        deltrap(level, t);
    newsym(x, y);
    newsym(x2, y2);
    block_point(x2, y2);        /* vision */
}

/*
 * Open the drawbridge located at x,y
 */
void
open_drawbridge(int x, int y)
{
    struct rm *loc1, *loc2;
    struct trap *t;
    int x2, y2;

    loc1 = &level->locations[x][y];
    if (loc1->typ != DRAWBRIDGE_UP)
        return;
    x2 = x;
    y2 = y;
    get_wall_for_db(&x2, &y2);
    if (cansee(x, y) || cansee(x2, y2))
        pline(msgc_actionok, "You see a drawbridge %s down!",
              (distu(x2, y2) < distu(x, y)) ? "going" : "coming");
    loc1->typ = DRAWBRIDGE_DOWN;
    loc2 = &level->locations[x2][y2];
    loc2->typ = DOOR;
    loc2->doormask = D_NODOOR;
    set_entity(x, y, &(occupants[0]));
    set_entity(x2, y2, &(occupants[1]));
    do_entity(&(occupants[0])); /* do set_entity after first */
    set_entity(x2, y2, &(occupants[1]));        /* do_entity for worm tails */
    do_entity(&(occupants[1]));
    revive_nasty(x, y, NULL);
    delallobj(x, y);
    if ((t = t_at(level, x, y)) != 0)
        deltrap(level, t);
    if ((t = t_at(level, x2, y2)) != 0)
        deltrap(level, t);
    newsym(x, y);
    newsym(x2, y2);
    unblock_point(x2, y2);      /* vision */
    if (Is_stronghold(&u.uz))
        u.uevent.uopened_dbridge = TRUE;
}

/*
 * Let's destroy the drawbridge located at x,y
 */
void
destroy_drawbridge(int x, int y)
{
    struct rm *loc1, *loc2;
    struct trap *t;
    int x2, y2;
    boolean e_inview;
    struct entity *etmp1 = &(occupants[0]), *etmp2 = &(occupants[1]);

    loc1 = &level->locations[x][y];
    if (!IS_DRAWBRIDGE(loc1->typ))
        return;
    x2 = x;
    y2 = y;
    get_wall_for_db(&x2, &y2);
    loc2 = &level->locations[x2][y2];
    if ((loc1->drawbridgemask & DB_UNDER) == DB_MOAT ||
        (loc1->drawbridgemask & DB_UNDER) == DB_LAVA) {
        struct obj *otmp;
        boolean lava = (loc1->drawbridgemask & DB_UNDER) == DB_LAVA;

        if (loc1->typ == DRAWBRIDGE_UP) {
            if (cansee(x2, y2))
                pline(msgc_consequence,
                      "The portcullis of the drawbridge falls into the %s!",
                      lava ? "lava" : waterbody_name(x2, y2));
            else
                You_hear(msgc_levelsound, "a loud *SPLASH*!");
        } else {
            if (cansee(x, y))
                pline(msgc_consequence,
                      "The drawbridge collapses into the %s!",
                      lava ? "lava" : waterbody_name(x, y));
            else
                You_hear(msgc_levelsound, "a loud *SPLASH*!");
        }
        loc1->typ = lava ? LAVAPOOL : MOAT;
        loc1->drawbridgemask = 0;
        if ((otmp = sobj_at(BOULDER, level, x, y)) != 0) {
            obj_extract_self(otmp);
            flooreffects(otmp, x, y, "fall");
        }
    } else {
        if (cansee(x, y))
            pline(msgc_consequence, "The drawbridge disintegrates!");
        else
            You_hear(msgc_levelsound, "a loud *CRASH*!");
        loc1->typ = ((loc1->drawbridgemask & DB_ICE) ? ICE : ROOM);
        loc1->icedpool = ((loc1->drawbridgemask & DB_ICE) ? ICED_MOAT : 0);
    }
    wake_nearto(x, y, 500);
    loc2->typ = DOOR;
    loc2->doormask = D_NODOOR;
    if ((t = t_at(level, x, y)) != 0)
        deltrap(level, t);
    if ((t = t_at(level, x2, y2)) != 0)
        deltrap(level, t);
    newsym(x, y);
    newsym(x2, y2);
    if (!does_block(level, x2, y2))
        unblock_point(x2, y2);  /* vision */
    if (Is_stronghold(&u.uz))
        u.uevent.uopened_dbridge = TRUE;

    set_entity(x2, y2, etmp2);  /* currently only automissers can be here */
    if (etmp2->edata) {
        enum msg_channel hit_msgc;
        if (is_u(etmp2))
            hit_msgc = msgc_fatal_predone;
        else if (etmp2->emon->mtame && canspotmon(etmp2->emon))
            hit_msgc = msgc_petfatal;
        else
            hit_msgc = msgc_monneutral;
        
        e_inview = e_canseemon(etmp2);
        if (!automiss(etmp2)) {
            if (e_inview)
                pline(hit_msgc, "%s blown apart by flying debris.",
                      E_phrase(etmp2, "are"));
            e_died(etmp2, e_inview ? 3 : 2, CRUSHING,
                   killer_msg(CRUSHING, "an exploding drawbridge"));
        }       /* nothing which is vulnerable can survive this */
    }
    set_entity(x, y, etmp1);
    if (etmp1->edata) {
        enum msg_channel hit_msgc;
        if (is_u(etmp1))
            hit_msgc = msgc_fatal_predone;
        else if (etmp1->emon->mtame && canspotmon(etmp1->emon))
            hit_msgc = msgc_petfatal;
        else
            hit_msgc = msgc_monneutral;
        
        e_inview = e_canseemon(etmp1);
        if (!e_missed(etmp1, TRUE)) {
            if (e_inview) {
                if (!is_u(etmp1) && Hallucination)
                    pline(hit_msgc, "%s into some heavy metal!",
                          E_phrase(etmp1, "get"));
                else
                    pline(hit_msgc, "%s hit by a huge chunk of metal!",
                          E_phrase(etmp1, "are"));
            } else {
                if (!is_u(etmp1) && !is_pool(level, x, y))
                    You_hear(msgc_levelsound, "a crushing sound.");
            }
            e_died(etmp1, e_inview ? 3 : 2, CRUSHING,
                   killer_msg(CRUSHING, "a collapsing drawbridge"));
            /* if (loc1->typ == MOAT) do_entity(etmp1); */
        }
        if (is_u(etmp1))
            spoteffects(FALSE);
        else if (!DEADMONSTER(etmp1->emon))
            minliquid(etmp1->emon);
    }
}

/*dbridge.c*/

