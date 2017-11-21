/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-11-18 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Detection routines, including crystal ball, magic mapping, and search
 * command.
 */

#include "hack.h"
#include "artifact.h"

#define OBJDET_MON       (1 << 0)
#define OBJDET_FLOOR     (1 << 1)
#define OBJDET_CONTAINED (1 << 2)
#define OBJDET_BURIED    (1 << 3)
#define OBJDET_UNMAPPED  (1 << 4)
#define OBJDET_SELF      (1 << 5)

static int find_obj_map(boolean, char, unsigned);
static int find_obj_tile(int, int, boolean, char, unsigned);
static void find_obj(struct obj *, int, int, int, int *,
                     boolean, char, unsigned);
static boolean find_obj_content(struct obj *, struct obj **,
                                boolean, char, unsigned);
static boolean find_obj_match(struct obj *, boolean, char, unsigned);
static void sense_trap(struct trap *, xchar, xchar, int);
static void show_map_spot(int, int);
static void findone(int, int, void *);
static void openone(int, int, void *);
static void search_tile(int, int, struct monst *, int);
static const char *level_distance(d_level *);

/*
 * Returns nonzero if we found anything. Extra flags:
 * OBJDET_UNMAPPED   Returned if we didn't find anything but unmapped
 *                   previous knowledge
 *
 * OBJDET_SELF       We only found stuff in our inventory or below us
 */
static int
find_obj_map(boolean set_dknown, char oclass, unsigned material)
{
    int res = 0;
    int ires = 0;

    int x, y;
    for (x = 0; x < COLNO; x++) {
        for (y = 0; y < ROWNO; y++) {
            ires = find_obj_tile(x, y, set_dknown, oclass, material);

            /* Don't mark UNMAPPED in res unless it's empty or only contains
               player inventory */
            if (ires & OBJDET_UNMAPPED) {
                if (!res || !(res & ~(OBJDET_MON | OBJDET_SELF)))
                    res = ires;
                ires = 0;
            }

            if (ires) {
                /* Only mark OBJDET_SELF if res is empty */
                if ((ires & OBJDET_SELF) && res)
                    ires &= ~OBJDET_SELF;

                res |= ires;
            }
        }
    }

    return res;
}

/* Returns nonzero if we found anything on the square.
   OBJDET_SELF is set if the square is the one the hero is standing on. */
static int
find_obj_tile(int x, int y,
              boolean set_dknown, char oclass, unsigned material)
{
    int memobj;
    struct monst *mon;
    int res = 0;

    memobj = level->locations[x][y].mem_obj;

    /* Show objects in given priority: floor > player/monster > buried */
    find_obj(level->buriedobjlist, x, y, OBJDET_BURIED, &res,
             set_dknown, oclass, material);

    for (mon = level->monlist; mon; mon = mon->nmon) {
        if (mon->mx == x && mon->my == y) {
            find_obj(mon->minvent, x, y, OBJDET_MON, &res,
                     set_dknown, oclass, material);
            break;
        }
    }

    /* Regard the player as just another monster */
    if (x == u.ux && y == u.uy)
        find_obj(youmonst.minvent, x, y, OBJDET_MON, &res,
                 set_dknown, oclass, material);

    find_obj(level->objects[x][y], x, y,  OBJDET_FLOOR, &res,
             set_dknown, oclass, material);

    if (!res && memobj) {
        /* If the remembered object is of a type we would have detected, unmap it */
        memobj--; /* mem_obj is 1-indexed so that 0 means "nothing here" */

        if ((material && objects[memobj].oc_material == material) ||
            (oclass && objects[memobj].oc_class == oclass) ||
            (!oclass && !material)) {
            unmap_object(x, y);
            unset_objpile(level, x, y);
            res |= OBJDET_UNMAPPED;
        }
    }

    if ((res & ~OBJDET_UNMAPPED) && x == u.ux && y == u.uy)
        res |= OBJDET_SELF;

    if (res)
        newsym(x, y);

    return res;
}

/* Updates res depending on if, and what, we found. */
static void
find_obj(struct obj *chain, int x, int y, int how, int *res,
         boolean set_dknown, char oclass, unsigned material)
{
    struct obj *obj, *contained;;
    for (obj = chain; obj; obj = ((how == OBJDET_FLOOR) ? obj->nexthere : obj->nobj)) {
        /* If carried by a monster, update ox/oy so we can work on it properly */
        if (how == OBJDET_MON) {
            obj->ox = x;
            obj->oy = y;
        }

        if (obj->ox != x || obj->oy != y)
            continue;

        if (find_obj_match(obj, set_dknown, oclass, material)) {
            if (*res != how) {
                *res = how;
                map_object(obj, 1, TRUE);
                unset_objpile(level, obj->ox, obj->oy);
            } else
                set_objpile(level, obj->ox, obj->oy);
        }

        /* Check if there's any object of the material/class contained recursively */
        contained = NULL;

        if (find_obj_content(obj, &contained, set_dknown, oclass, material)) {
            contained->ox = obj->ox;
            contained->oy = obj->oy;
            if (!(*res & how)) {
                map_object(contained, 1, TRUE);
                unset_objpile(level, contained->ox, contained->oy);
                *res = (how | OBJDET_CONTAINED);
            }
        }
    }
}

/* Returns TRUE if we found a matching object contained inside */
static boolean
find_obj_content(struct obj *container, struct obj **contained,
                 boolean set_dknown, char oclass, unsigned material)
{
    struct obj *obj;
    if (!Has_contents(container))
        return FALSE;

    for (obj = container->cobj; obj; obj = obj->nobj) {
        if (find_obj_match(obj, set_dknown, oclass, material)) {
            if (!(*contained)) {
                *contained = obj;
            }
        }

        find_obj_content(obj, contained, set_dknown, oclass, material);
    }

    if (*contained)
        return TRUE;

    return FALSE;
}

/* Returns TRUE if the object matches our filters in oclass/material and
   sets dknown if applicable. Also updates object memory. */
static boolean
find_obj_match(struct obj *obj,
               boolean set_dknown, char oclass, unsigned material)
{
    boolean ret = FALSE;
    if (!obj)
        return ret;

    ret = ((material && objects[obj->otyp].oc_material == material) ||
           (oclass && obj->oclass == oclass) ||
           (!oclass && !material));
    if (ret) {
        if (set_dknown)
            obj->dknown = TRUE;
        if (obj->where == OBJ_FLOOR)
            update_obj_memory(obj, NULL);
    }

    return ret;
}

/* look for gold, on the floor or in monsters' possession */
int
gold_detect(struct monst *mon, struct obj *sobj, boolean *scr_known)
{
    int uw = u.uinwater;
    boolean you = (mon == &youmonst);

    if (!you) {
        impossible("monster using gold detection?");
        return 0;
    }

    cls();

    int obj_res = find_obj_map(sobj->blessed, COIN_CLASS, sobj->blessed ? GOLD : 0);
    struct obj gold; /* Fake object to make map_object happy */
    gold.otyp = GOLD_PIECE;
    gold.quan = 2L;

    /* Find gold golems */
    struct monst *gmon;
    for (gmon = level->monlist; gmon; gmon = gmon->nmon) {
        if (DEADMONSTER(gmon))
            continue;

        if (monsndx(gmon->data) == PM_GOLD_GOLEM) {
            obj_res = OBJDET_MON;
            gold.ox = gmon->mx;
            gold.oy = gmon->my;
            map_object(&gold, 1, TRUE);
            set_objpile(level, gmon->mx, gmon->my);
        }
    }

    /* Find the player as a gold golem */
    if (youmonst.data == &mons[PM_GOLD_GOLEM]) {
        if (!obj_res)
            obj_res = (OBJDET_MON | OBJDET_SELF);

        gold.ox = u.ux;
        gold.oy = u.uy;
        map_object(&gold, 1, TRUE);
        set_objpile(level, u.ux, u.uy);
    }

    *scr_known = !!obj_res;
    obj_res &= ~OBJDET_UNMAPPED;

    if (!obj_res || !(obj_res & ~(OBJDET_MON | OBJDET_SELF))) {
        /* No gold found, or gold only found in inventory, or
           user is a gold golem */
        const char *buf;

        doredraw();
        if (youmonst.data == &mons[PM_GOLD_GOLEM]) {
            buf = msgprintf("You feel like a million %s!", currency(2L));
        } else if (hidden_gold() || money_cnt(youmonst.minvent))
            buf = "You feel worried about your future financial situation.";
        else
            buf = "You feel materially poor.";
        if (!(*scr_known))
            strange_feeling(sobj, buf);
        else {
            pline(msgc_failcurse, "%s", buf);
            if (sobj)
                useup(sobj);
        }
        return 1;
    }

    /* only under user - no separate display required */
    if (obj_res & OBJDET_SELF) {
        doredraw();
        pline(msgc_youdiscover, "You notice some gold %s%s.",
              (obj_res & OBJDET_BURIED) ? "below you" : "between your ",
              (obj_res & OBJDET_BURIED) ? "" :
              makeplural(body_part(FOOT)));
        return 0;
    }

    u.uinwater = 0;

    pline(msgc_youdiscover, "You feel very greedy, and sense gold!");
    exercise(A_WIS, TRUE);
    look_at_map(u.ux, u.uy);

    doredraw();
    u.uinwater = uw;
    if (Underwater)
        under_water(2);
    if (u.uburied)
        under_ground(2);
    return 0;
}

/* returns 1 if nothing was detected            */
/* returns 0 if something was detected          */
int
food_detect(struct obj *sobj, boolean *scr_known)
{
    boolean confused = (Confusion || (sobj && sobj->cursed));
    char oclass = confused ? POTION_CLASS : FOOD_CLASS;
    const char *what = confused ? "something" : "food";
    int uw = u.uinwater;

    cls();

    int obj_res = find_obj_map(sobj->blessed, oclass, 0);

    *scr_known = !!obj_res;
    if (sobj && sobj->blessed && !u.uedibility)
        *scr_known = TRUE;

    if (obj_res == OBJDET_UNMAPPED) {
        /* Food was seen but is now gone */

        doredraw();
        pline(msgc_failcurse, "You sense a lack of %s nearby.", what);

        if (sobj && sobj->blessed) {
            if (!u.uedibility)
                pline(msgc_statusgood, "Your %s starts to tingle.",
                      body_part(NOSE));
            u.uedibility = 1;
        }
        return 1;
    }

    obj_res &= ~OBJDET_UNMAPPED;

    if (!obj_res || !(obj_res & ~(OBJDET_MON | OBJDET_SELF))) {
        /* nothing found, or only in user inventory */
        doredraw();
        const char *buf;
        buf = msgprintf("Your %s twitches%s.",
                        body_part(NOSE),
                        !u.uedibility && sobj && sobj->blessed ?
                        " then starts to tingle" : "");

        if (sobj && sobj->blessed)
            u.uedibility = 1;

        if (*scr_known) {
            pline(msgc_failcurse, "%s", buf);
            if (sobj)
                useup(sobj);
        } else
            strange_feeling(sobj, buf);

        return 1;
    }

    if (obj_res & OBJDET_SELF) {
        /* only under user */
        doredraw();
        pline(msgc_youdiscover, "You smell %s nearby.", what);
        if (sobj && sobj->blessed) {
            if (!u.uedibility)
                pline(msgc_statusgood, "Your %s starts to tingle.",
                      body_part(NOSE));
            u.uedibility = 1;
        }
        return 0;
    }

    if (sobj && sobj->blessed) {
        pline(u.uedibility ? msgc_youdiscover : msgc_intrgain,
              "Your %s %s to tingle and you smell %s.",
              body_part(NOSE), u.uedibility ? "continues" : "starts",
              what);
        u.uedibility = 1;
    } else
        pline(msgc_youdiscover, "Your %s tingles and you smell %s.",
              body_part(NOSE), what);
    exercise(A_WIS, TRUE);
    look_at_map(u.ux, u.uy);
    doredraw();
    u.uinwater = uw;
    if (Underwater)
        under_water(2);
    if (u.uburied)
        under_ground(2);
    return 0;
}

/*
 * Used for scrolls, potions, spells, and crystal balls.  Returns:
 *
 *      1 - nothing was detected
 *      0 - something was detected
 */
int
object_detect(struct obj *detector,     /* object doing the detecting */
              int class /* an object class, 0 for all */ )
{
    int uw = u.uinwater;
    const char *stuff;
    const char *buf;
    int do_dknown = (detector &&
                     (detector->oclass == POTION_CLASS ||
                      detector->oclass == SPBOOK_CLASS) && detector->blessed);

    if (class < 0 || class >= MAXOCLASSES) {
        impossible("object_detect:  illegal class %d", class);
        class = 0;
    }

    cls();

    int obj_res = find_obj_map(do_dknown, class, 0);

    if (Hallucination || (Confusion && class == SCROLL_CLASS))
        stuff = "something";
    else
        stuff = class ? oclass_names[class] : "objects";

    if (obj_res == OBJDET_UNMAPPED) {
        /* Remembered objects are now gone */

        doredraw();
        pline(msgc_failcurse, "You feel a lack of %s.", stuff);
        if (detector)
            useup(detector);

        return 1;
    }

    obj_res &= ~OBJDET_UNMAPPED;

    buf = msgprintf("You sense the %s of %s.",
                    (!obj_res || !(obj_res & ~(OBJDET_MON | OBJDET_SELF))) ?
                    "absence" : "presence", stuff);

    if (!obj_res || !(obj_res & ~(OBJDET_MON | OBJDET_SELF))) {
        /* nothing found, or only in user inventory */
        doredraw();
        strange_feeling(detector, buf);

        return 1;
    }

    if (obj_res & OBJDET_SELF) {
        /* only under user */
        pline(msgc_youdiscover, "You sense %s nearby.", stuff);
        return 0;
    }

    pline(msgc_youdiscover, "%s", buf);
    exercise(A_WIS, TRUE);
    look_at_map(u.ux, u.uy);
    doredraw();
    u.uinwater = uw;
    if (Underwater)
        under_water(2);
    if (u.uburied)
        under_ground(2);
    return 0;
}

/*
 * Used by: crystal balls, potions, fountains
 *
 * Returns 1 if nothing was detected.
 * Returns 0 if something was detected.
 */
int
monster_detect(struct obj *otmp,        /* detecting object (if any) */
               int mclass /* monster class, 0 for all */ )
{
    struct monst *mtmp;
    int mcnt = 0;

    /* Note: This used to just check level->monlist for a non-zero value but in
       versions since 3.3.0 level->monlist can test TRUE due to the presence of
       dmons, so we have to find at least one that's still alive to know for
       sure. */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp)) {
            mcnt++;
            break;
        }

    if (!mcnt) {
        if (otmp)
            strange_feeling(otmp,
                            Hallucination ? "You get the heebie jeebies." :
                            "You feel threatened.");
        return 1;
    } else {
        boolean woken = FALSE;

        cls();
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
            if (DEADMONSTER(mtmp))
                continue;
            if (!mclass || mtmp->data->mlet == mclass ||
                (mtmp->data == &mons[PM_LONG_WORM] && mclass == S_WORM_TAIL))
                if (mtmp->mx < COLNO) {
                    dbuf_set(mtmp->mx, mtmp->my, S_unexplored, 0, 0, 0, 0,
                             dbuf_monid(mtmp, mtmp->mx,
                                        mtmp->my, rn2), 0, 0, 0);
                    /* don't be stingy - display entire worm */
                    if (mtmp->data == &mons[PM_LONG_WORM])
                        detect_wsegs(mtmp, 0);
                }
            if (otmp && otmp->cursed && (mtmp->msleeping || !mtmp->mcanmove)) {
                mtmp->msleeping = 0;
                /* Don't unfreeze a petrifying monster */
                if (!(property_timeout(mtmp, STONED) <= 3)) {
                    mtmp->mfrozen = 0;
                    mtmp->mcanmove = 1;
                }
                woken = TRUE;
            }
        }

        /* Temporarily set intrinsic monster detection to give a sane value for
           farlooking. */
        boolean had_previously = !!(detects_monsters(&youmonst) & W_MASK(os_outside));
        set_property(&youmonst, DETECT_MONSTERS, 0, TRUE);
        display_self();
        pline(msgc_youdiscover, "You sense the presence of monsters.");
        if (woken)
            pline(msgc_statusbad, "Monsters sense the presence of you.");
        look_at_map(u.ux, u.uy);
        if (!had_previously)
            set_property(&youmonst, DETECT_MONSTERS, -1, TRUE);
        doredraw();
        if (Underwater)
            under_water(2);
        if (u.uburied)
            under_ground(2);
    }
    return 0;
}

static void
sense_trap(struct trap *trap, xchar x, xchar y, int src_cursed)
{
    if (Hallucination || src_cursed) {
        struct obj obj; /* fake object */

        if (trap) {
            obj.ox = trap->tx;
            obj.oy = trap->ty;
        } else {
            obj.ox = x;
            obj.oy = y;
        }
        obj.otyp = src_cursed ? GOLD_PIECE : random_object(rn2);
        obj.corpsenm = random_monster(rn2);        /* if otyp == CORPSE */
        map_object(&obj, 1, TRUE);
    } else if (trap) {
        map_trap(trap, 1, TRUE);
        trap->tseen = 1;
    } else {
        struct trap temp_trap;  /* fake trap */

        temp_trap.tx = x;
        temp_trap.ty = y;
        temp_trap.ttyp = BEAR_TRAP;     /* some kind of trap */
        map_trap(&temp_trap, 1, TRUE);
    }

}

/* the detections are pulled out so they can    */
/* also be used in the crystal ball routine     */
/* returns 1 if nothing was detected            */
/* returns 0 if something was detected          */
int
trap_detect(struct monst *mon, struct obj *sobj)
/* sobj is null if crystal ball, *scroll if gold detection scroll */
{
    struct trap *ttmp;
    struct obj *obj;
    int door;
    int uw = u.uinwater;
    boolean found = FALSE;
    coord cc;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    /* Whether or not to reveal traps (or fake gold) -- happens for you, or
       if a monster you can see is tame. The reason tame monsters share the
       information (unlike hostiles to each other) is because tame monsters'
       trap memory is in fact equavilent to yours. */
    boolean reveal = (you || (vis && mon->mtame && (!sobj || !sobj->cursed)));
    boolean mreveal = (!you && (!sobj || !sobj->cursed));

    for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap) {
        if (ttmp->tx != m_mx(mon) || ttmp->ty != m_my(mon))
            goto outtrapmap;
        else
            found = TRUE;
    }
    for (obj = level->objlist; obj; obj = obj->nobj) {
        if ((obj->otyp == LARGE_BOX || obj->otyp == CHEST) && obj->otrapped) {
            if (obj->ox != m_mx(mon) || obj->oy != m_my(mon))
                goto outtrapmap;
            else
                found = TRUE;
        }
    }
    for (door = 0; door < level->doorindex; door++) {
        cc = level->doors[door];
        if (level->locations[cc.x][cc.y].flags & D_TRAPPED) {
            if (cc.x != m_mx(mon) || cc.y != m_my(mon))
                goto outtrapmap;
            else
                found = TRUE;
        }
    }
    if (!found) {
        char buf[42]; /* TODO: convert to msgprintf */

        snprintf(buf, SIZE(buf), "Your %s stop itching.", makeplural(body_part(TOE)));
        if (you)
            strange_feeling(sobj, buf);
        else {
            if (vis)
                pline(msgc_failcurse,
                      "%s was unable to gain any insights.", Monnam(mon));
            if (sobj)
                m_useup(mon, sobj);
        }

        return 1;
    }
    /* traps exist, but only under me - no separate display required */
    if (you)
        pline(msgc_youdiscover, "Your %s itch.", makeplural(body_part(TOE)));
    else if (vis)
        pline(msgc_monneutral, "%s looks momentarily itchy for some reason.", Monnam(mon));
    return 0;
outtrapmap:
    if (reveal)
        cls();

    /* FIXME: why the water reset? */
    if (reveal)
        u.uinwater = 0;
    for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap) {
        if (reveal)
            sense_trap(ttmp, 0, 0, sobj && sobj->cursed);
        if (mreveal)
            mon->mtrapseen |= (1 << (ttmp->ttyp - 1));
    }

    for (obj = level->objlist; obj; obj = obj->nobj)
        if ((obj->otyp == LARGE_BOX || obj->otyp == CHEST) && obj->otrapped)
            if (reveal)
                sense_trap(NULL, obj->ox, obj->oy, sobj && sobj->cursed);

    for (door = 0; door < level->doorindex; door++) {
        cc = level->doors[door];
        /* make the door, and its trapped status, show up on the player's
           memory */
        if (!sobj || !sobj->cursed) {
            if (level->locations[cc.x][cc.y].flags & D_TRAPPED) {
                if (reveal) {
                    level->locations[cc.x][cc.y].mem_door_t = 1;
                    map_background(cc.x, cc.y, TRUE);
                }
            }
        }
    }

    if (reveal) {
        newsym(u.ux, u.uy);
        if (you)
            pline(msgc_youdiscover, "You feel %s.",
                  sobj && sobj->cursed ? "very greedy" : "entrapped");
        else
            pline(msgc_youdiscover, "%s is granted an insight, and shares it with you!",
                  Monnam(mon));
        look_at_map(u.ux, u.uy);
        doredraw();
        u.uinwater = uw;
        if (Underwater)
            under_water(2);
        if (u.uburied)
            under_ground(2);
    } else if (mreveal)
        pline(msgc_monneutral, "%s is granted an insight!", Monnam(mon));
    return 0;
}


static const char *
level_distance(d_level * where)
{
    schar ll = depth(&u.uz) - depth(where);
    boolean indun = (u.uz.dnum == where->dnum);

    if (ll < 0) {
        if (ll < (-8 - rn2(3)))
            if (!indun)
                return "far away";
            else
                return "far below";
        else if (ll < -1)
            if (!indun)
                return "away below you";
            else
                return "below you";
        else if (!indun)
            return "in the distance";
        else
            return "just below";
    } else if (ll > 0) {
        if (ll > (8 + rn2(3)))
            if (!indun)
                return "far away";
            else
                return "far above";
        else if (ll > 1)
            if (!indun)
                return "away above you";
            else
                return "above you";
        else if (!indun)
            return "in the distance";
        else
            return "just above";
    } else if (!indun)
        return "in the distance";
    else
        return "near you";
}

static const struct {
    const char *what;
    d_level *where;
} level_detects[] = {
    {
    "Delphi", &oracle_level}, {
    "Medusa's lair", &medusa_level}, {
    "a castle", &stronghold_level}, {
"the Wizard of Yendor's tower", &wiz1_level},};

void
use_crystal_ball(struct obj *obj)
{
    char ch;
    int oops;

    if (Blind) {
        pline(msgc_cancelled1, "Too bad you can't see %s.", the(xname(obj)));
        return;
    }

    /* We use a custom RNG for whether looking into the crystal ball succeeds or
       not, because the number of uses you get out of one crystal ball can be
       strategically important. */
    int oops_result = rn2_on_rng(20, rng_crystal_ball);
    oops = (rn2_on_rng(20, rng_crystal_ball) >= ACURR(A_INT) || obj->cursed);
    if (oops && (obj->spe > 0)) {
        /* Guarantee lined-up results between artifact and non-artifact unless a
           non-artifact would explode; substitute a random result for artifacts
           in that case. (It's OK to use rnd() for the actual timeout, because
           that typically isn't very strategically important.) */
        if (obj->oartifact && oops_result % 5 == 4)
            oops_result /= 5;
        else
            oops_result %= 5;

        switch (oops_result) {
        case 0:
            pline(msgc_failrandom, "%s too much to comprehend!",
                  Tobjnam(obj, "are"));
            break;
        case 1:
            pline(msgc_statusbad, "%s you!", Tobjnam(obj, "confuse"));
            inc_timeout(&youmonst, CONFUSION, rnd(100), TRUE);
            break;
        case 2:
            if (!resists_blnd(&youmonst)) {
                pline(msgc_statusbad, "%s your vision!",
                      Tobjnam(obj, "damage"));
                inc_timeout(&youmonst, BLINDED, rnd(100), TRUE);
                if (!blind(&youmonst))
                    pline(msgc_statusheal, "Your vision quickly clears.");
            } else {
                pline(msgc_playerimmune, "%s your vision.  You are unaffected!",
                      Tobjnam(obj, "assault"));
            }
            break;
        case 3:
            pline(msgc_statusbad, "%s your mind!", Tobjnam(obj, "zap"));
            inc_timeout(&youmonst, HALLUC, rnd(100), TRUE);
            break;
        case 4:
            pline(msgc_substitute, "%s!", Tobjnam(obj, "explode"));
            useup(obj);
            obj = 0;    /* it's gone */
            losehp(rnd(30), killer_msg(DIED, "an exploding crystal ball"));
            break;
        }
        if (obj)
            consume_obj_charge(obj, TRUE);
        return;
    }

    if (Hallucination) {
        if (!obj->spe) {
            pline(obj->known ? msgc_cancelled1 : msgc_failcurse,
                  "All you see is funky %s haze.", hcolor(NULL));
            obj->known = TRUE;
        } else {
            switch (rnd(6)) {
            case 1:
                pline(msgc_yafm,
                      "You grok some groovy globs of incandescent lava.");
                break;
            case 2:
                pline(msgc_yafm, "Whoa!  Psychedelic colors, %s!",
                      poly_gender() == 1 ? "babe" : "dude");
                break;
            case 3:
                pline(msgc_yafm, "The crystal pulses with sinister %s light!",
                      hcolor(NULL));
                break;
            case 4:
                pline(msgc_yafm,
                      "You see goldfish swimming above fluorescent rocks.");
                break;
            case 5:
                pline(msgc_yafm, "You see tiny snowflakes spinning around "
                      "a miniature farmhouse.");
                break;
            default:
                pline(msgc_yafm, "Oh wow... like a kaleidoscope!");
                break;
            }
            consume_obj_charge(obj, TRUE);
        }
        return;
    }

    /* This was treated like msgc_controlhelp in 3.4.3, but uiprompt seems to
       fit better. Not sure yet, it depends on whether uiprompts generally turn
       out to be obvious enough that we can turn them off with !verbose. */
    pline(msgc_uiprompt, "You may look for an object or monster symbol.");
    /* read a single character */
    ch = query_key("What do you look for?", NQKF_SYMBOL, NULL);
    if (strchr(quitchars, ch)) {
        pline(msgc_cancelled, "Never mind.");
        return;
    }
    pline(msgc_occstart, "You peer into %s...", the(xname(obj)));
    helpless(rnd(10), hr_busy, "gazing into a crystal ball",
             "You finish your crystal-gazing.");
    if (obj->spe <= 0) {
        /* actually msgc_cancelled ## rnd(10), but... */
        pline(obj->known ? msgc_cancelled1 : msgc_nospoil,
              "The vision is unclear.");
    } else {
        int class;
        int ret = 0;

        makeknown(CRYSTAL_BALL);
        consume_obj_charge(obj, TRUE);

        /* special case: accept ']' as synonym for mimic we have to do this
           before the def_char_to_objclass check */
        if (ch == DEF_MIMIC_DEF)
            ch = DEF_MIMIC;

        if ((class = def_char_to_objclass(ch)) != MAXOCLASSES)
            ret = object_detect(NULL, class);
        else if ((class = def_char_to_monclass(ch)) != MAXMCLASSES)
            ret = monster_detect(NULL, class);
        else
            switch (ch) {
            case '^':
                ret = trap_detect(&youmonst, NULL);
                break;
            default:
                {
                    int i = rn2(SIZE(level_detects));

                    pline(msgc_youdiscover, "You see %s, %s.",
                          level_detects[i].what,
                          level_distance(level_detects[i].where));
                }
                ret = 0;
                break;
            }

        if (ret) {
            if (!rn2(100))      /* make them nervous */
                pline(msgc_failrandom,
                      "You see the Wizard of Yendor gazing out at you.");
            else
                pline(msgc_nospoil, "The vision is unclear.");
        }
    }
    return;
}

static void
show_map_spot(int x, int y)
{
    struct rm *loc;

    if (Confusion && rn2(7))
        return;
    loc = &level->locations[x][y];

    loc->seenv = SVALL;

    /* Secret corridors are found, but not secret doors. */
    if (loc->typ == SCORR) {
        loc->typ = CORR;
        unblock_point(x, y);
    }

    /* Now we're using the Slash'EM display engine, we can map unconditionally
       (with the 3.4.3 display engine, it's necessary to not overwrite
       remembered objects or traps) */
    if (level->flags.hero_memory) {
        magic_map_background(x, y, 0);
        newsym(x, y);       /* show it, if not blocked */
    } else {
        magic_map_background(x, y, 1);      /* display it */
    }
}


void
do_mapping(void)
{
    int zx, zy;
    int uw = u.uinwater;

    u.uinwater = 0;
    for (zx = 0; zx < COLNO; zx++)
        for (zy = 0; zy < ROWNO; zy++)
            show_map_spot(zx, zy);
    exercise(A_WIS, TRUE);
    u.uinwater = uw;
    if (!level->flags.hero_memory || Underwater) {
        flush_screen(); /* flush temp screen */
        look_at_map(u.ux, u.uy); /* wait */
        doredraw();
    }
}


void
do_vicinity_map(void)
{
    int zx, zy;
    int lo_y = (u.uy - 5 < 0 ? 0 : u.uy - 5),
        hi_y = (u.uy + 6 > ROWNO ? ROWNO : u.uy + 6),
        lo_x = (u.ux - 9 < 0 ? 0 : u.ux - 9),
        hi_x = (u.ux + 10 > COLNO ? COLNO : u.ux + 10);

    for (zx = lo_x; zx < hi_x; zx++)
        for (zy = lo_y; zy < hi_y; zy++)
            show_map_spot(zx, zy);

    if (!level->flags.hero_memory || Underwater) {
        flush_screen(); /* flush temp screen */
        look_at_map(u.ux, u.uy); /* wait */
        doredraw();
    }
}

/* convert a secret door into a normal door */
void
cvt_sdoor_to_door(struct rm *loc, const d_level * dlev)
{
    int newmask = loc->flags & ~WM_MASK;

    if (Is_rogue_level(dlev))
        /* rogue didn't have doors, only doorways */
        newmask = D_NODOOR;
    else
        /* newly exposed door is closed */
    if (!(newmask & D_LOCKED))
        newmask |= D_CLOSED;

    loc->typ = DOOR;
    loc->flags = newmask;
}


static void
findone(int zx, int zy, void *num)
{
    struct trap *ttmp;
    struct monst *mtmp;

    if (level->locations[zx][zy].typ == SDOOR) {
        cvt_sdoor_to_door(&level->locations[zx][zy], &u.uz);   /* .typ = DOOR */
        magic_map_background(zx, zy, 0);
        newsym(zx, zy);
        (*(int *)num)++;
    } else if (level->locations[zx][zy].typ == SCORR) {
        level->locations[zx][zy].typ = CORR;
        unblock_point(zx, zy);
        magic_map_background(zx, zy, 0);
        newsym(zx, zy);
        (*(int *)num)++;
    } else if ((ttmp = t_at(level, zx, zy)) != 0)
        sense_trap(ttmp, 0, 0, 0);
    else if (cansee(zx, zy) && (mtmp = m_at(level, zx, zy)) != 0) {
        if (mtmp->m_ap_type && !level->locations[zx][zy].mem_invis) {
            map_invisible(zx, zy);
            (*(int *)num)++;
        }
        if (mtmp->mundetected &&
            (is_hider(mtmp->data) || mtmp->data->mlet == S_EEL)) {
            mtmp->mundetected = 0;
            newsym(zx, zy);
            (*(int *)num)++;
        }
        if (!canspotmon(mtmp) && !level->locations[zx][zy].mem_invis)
            map_invisible(zx, zy);
    } else if (level->locations[zx][zy].mem_invis) {
        unmap_object(zx, zy);
        newsym(zx, zy);
        (*(int *)num)++;
    }
}


static void
openone(int zx, int zy, void *num)
{
    struct trap *ttmp;
    struct obj *otmp;

    if (OBJ_AT(zx, zy)) {
        for (otmp = level->objects[zx][zy]; otmp; otmp = otmp->nexthere) {
            if (Is_box(otmp) && otmp->olocked) {
                otmp->olocked = 0;
                (*(int *)num)++;
            }
        }
        /* let it fall to the next cases. could be on trap. */
    }
    if (level->locations[zx][zy].typ == SDOOR ||
        (level->locations[zx][zy].typ == DOOR &&
         (level->locations[zx][zy].flags & (D_CLOSED | D_LOCKED)))) {
        if (level->locations[zx][zy].typ == SDOOR)
            cvt_sdoor_to_door(&level->locations[zx][zy], &u.uz); /* typ=DOOR */
        if (level->locations[zx][zy].flags & D_TRAPPED) {
            if (distu(zx, zy) < 3)
                b_trapped("door", 0);
            else
                pline_once(msgc_levelsound, "You %s an explosion!",
                           cansee(zx, zy) ? "see" :
                           (canhear() ? "hear" : "feel the shock of"));
            wake_nearto(zx, zy, 11 * 11);
            level->locations[zx][zy].flags = D_NODOOR;
        } else
            level->locations[zx][zy].flags = D_ISOPEN;
        unblock_point(zx, zy);
        newsym(zx, zy);
        (*(int *)num)++;
    } else if (level->locations[zx][zy].typ == SCORR) {
        level->locations[zx][zy].typ = CORR;
        unblock_point(zx, zy);
        newsym(zx, zy);
        (*(int *)num)++;
    } else if ((ttmp = t_at(level, zx, zy)) != 0) {
        if (!ttmp->tseen && ttmp->ttyp != STATUE_TRAP) {
            ttmp->tseen = 1;
            newsym(zx, zy);
            (*(int *)num)++;
        }
    } else if (find_drawbridge(&zx, &zy)) {
        /* make sure it isn't an open drawbridge */
        open_drawbridge(zx, zy);
        (*(int *)num)++;
    }
}

int
findit(int radius)
{       /* returns number of things found */
    int num = 0;

    if (Engulfed)
        return 0;
    if (radius == -1) {
        int x, y;

        for (x = 0; x < COLNO; x++)
            for (y = 0; y < ROWNO; y++)
                findone(x, y, &num);
    } else
        do_clear_area(u.ux, u.uy, radius, findone, &num);
    return num;
}

int
openit(void)
{       /* returns number of things found and opened */
    int num = 0;

    if (Engulfed) {
        if (is_animal(u.ustuck->data)) {
            if (Blind)
                pline(msgc_actionok, "Its mouth opens!");
            else
                pline(msgc_actionok, "%s opens its mouth!", Monnam(u.ustuck));
        }
        expels(u.ustuck, u.ustuck->data, TRUE);
        return -1;
    }

    do_clear_area(u.ux, u.uy, BOLT_LIM, openone, &num);
    return num;
}


void
find_trap(struct trap *trap)
{
    int tt = what_trap(trap->ttyp, trap->tx, trap->ty, rn2);
    boolean cleared = FALSE;

    trap->tseen = 1;
    exercise(A_WIS, TRUE);
    if (Blind)
        feel_location(trap->tx, trap->ty);
    else
        newsym(trap->tx, trap->ty);

    if (level->locations[trap->tx][trap->ty].mem_obj ||
        level->locations[trap->tx][trap->ty].mem_invis) {
        /* There's too much clutter to see your find otherwise */
        cls();
        map_trap(trap, 1, TRUE);
        display_self();
        cleared = TRUE;
    }

    pline(msgc_youdiscover, "You find %s.", an(trapexplain[tt - 1]));

    if (cleared) {
        look_at_map(u.ux, u.uy); /* wait */
        doredraw();
    }
}

/* Gives spoiler information about the presence or absence of a monster on a
   given square; called when the player does something that would determine the
   presence of a monster on a given square. With true "unhide", unhides mimics
   and hidden monsters, otherwise uses an invisible-I mark. For monsters that
   aren't hiding but can't be seen, just uses an invisible-I mark directly.
   "unhide" should be TRUE if there's no reason to believe that the player could
   do the action cautiously; searching shouldn't disturb mimics.

   Exception to all this: if the monster is visible and not hiding, do
   nothing. Returns TRUE if the monster wasn't known to be there before the
   call, and it is known that there's a monster there now. */
boolean
reveal_monster_at(int x, int y, boolean unhide)
{
    if (!isok(x, y))
        return FALSE;

    boolean unknown = !level->locations[x][y].mem_invis;
    struct monst *mon = m_at(level, x, y);

    if (mon && DEADMONSTER(mon))
        mon = NULL;

    if (mon && cansuspectmon(mon) && !(msensem(&youmonst, mon) & MSENSE_DISPLACED))
        unknown = FALSE;

    /* Clear any invisible-monster I on this square.

       Note: We can't use the normal unmap_object because there might be a
       remembered item on the square (e.g. via object detection when blind). */
    level->locations[x][y].mem_invis = 0;

    if (!mon) {
        newsym(x, y);    /* required after unmap_object */
        return FALSE;
    }

    if (unhide)
        mon->msleeping = 0; /* wakeup() angers mon */

    if ((msensem(&youmonst, mon) & MSENSE_DISPLACED) ||
        (!canspotmon(mon) && !knownwormtail(x, y)))
        map_invisible(x, y);

    newsym(x, y);
    return unknown;
}

/* Search function wrapper */
int
dosearch0(int aflag)
{
    if (Engulfed) {
        /* Note: unlike most cases where we take time performing an action known
           to be impossible, this one is at least justifiable: many players use
           's' in order to wait a turn, even when engulfed, and in fact it's
           reasonable to not keybind 'wait' at all. This could therefore be
           considered msgc_yafm instead, if/when we get rid of all the
           msgc_cancelled1 cases.*/
        if (!aflag)
            pline(msgc_cancelled1, "What are you looking for?  The exit?");
        return 1;
    }

    /* We can't use do_clear_area because it iterates a ball radius, and it will be
       blocked by secret doors/corridors... Instead, iterate a 11x11 square centered on
       the player. The iteration is done in an outwards "spiral" to avoid oddities where
       a tile further away fails to be found because a tile closer was blocked even if
       it was unblocked as a result of the searching. */

    /* search center tile */
    search_tile(u.ux, u.uy, &youmonst, aflag);
    int i, x, y, iter_typ;
    for (i = 1; i <= 5; i++) {
        x = u.ux - i;
        y = u.uy - i;
        iter_typ = 0;
        while (iter_typ < 4) {
            search_tile(x, y, &youmonst, aflag);
            switch (iter_typ) {
            case 0:
                x++;
                break;
            case 1:
                y++;
                break;
            case 2:
                x--;
                break;
            case 3:
                y--;
                break;
            }
            if (x == u.ux + ((iter_typ == 0 || iter_typ == 1) ? i : -i) &&
                y == u.uy + ((iter_typ == 1 || iter_typ == 2) ? i : -i))
                iter_typ++;
        }
    }
    return 1;
}

/* Main search logic */
static void
search_tile(int x, int y, struct monst *mon, int autosearch)
{
    if (!isok(x, y))
        return;

    struct level *lev = m_dlevel(mon);
    struct trap *trap;
    boolean you = (mon == &youmonst);
    int dist = distmin(x, y, m_mx(mon), m_my(mon));

    /* Proc autosearch only 1/3 of the time (or 3+enchantment/6 if extrinsic) */
    if (autosearch) {
        int autorate = 2;
        if (ehas_property(mon, SEARCHING)) {
            autorate++;
            autorate += searchbon(mon);
        }
        if (autorate < rnd(6))
            return;
    }

    /* If you are blind, searching only functions if done deliberately, and only then for
       tiles next to you. */
    if (blind(mon)) {
        if (autosearch || dist > 1)
            return;

        if (you)
            feel_location(x, y);
    }

    /* If the tile isn't next to us, don't find anything there if the tile is dark. */
    if (dist > 1 && !lev->locations[x][y].lit && !templit(x, y))
        return;

    /*
     * Search rate:
     * 20
     * + Luck
     * + 5*searchbonus
     * + 5 with lenses
     * + 10 for archeologists
     * This is divided by 20 * 2 ^ (Manhattan distance - 1) to form a search rate
     * This means that with no luck or other bonuses, you find things next to you
     * 20/20 of the time (100%), 2 tiles away 20/40 (1/2), 3 tiles away 20/80 (1/4), etc
     */
    int baserate = 20;
    if (you)
        baserate += Luck;
    baserate += searchbon(mon) * 5;
    struct obj *tool = which_armor(mon, os_tool);
    if (tool && tool->otyp == LENSES)
        baserate += 5;
    if ((you && Role_if(PM_ARCHEOLOGIST)) ||
        (!you && mon->data == &mons[PM_ARCHEOLOGIST]))
        baserate += 10;
    if (baserate < 1)
        baserate = 1; /* Make it not impossible to find doors/etc */

    int basediv = 20;
    int i;
    for (i = 1; i < dist; i++)
        basediv *= 2;

    if (baserate < rnd(basediv))
        return;

    /* Don't allow searching through walls/similar */
    if (!clear_path(x, y, m_mx(mon), m_my(mon), viz_array))
        return;

    if (lev->locations[x][y].typ == SDOOR) {
        cvt_sdoor_to_door(&lev->locations[x][y], m_mz(mon));
        if (you) {
            exercise(A_WIS, TRUE);
            action_completed();
        }
        if (you && blind(mon))
            feel_location(x, y); /* make sure it shows up */
        else
            newsym(x, y);
    } else if (lev->locations[x][y].typ == SCORR) {
        lev->locations[x][y].typ = CORR;
        unblock_point(x, y); /* vision */
        if (you) {
            exercise(A_WIS, TRUE);
            action_completed();
        }
        newsym(x, y);
    } else {
        /* Be careful not to find anything in an SCORR or
           SDOOR. */

        boolean already = 0; /* finding a monster already found? */
        if (dist == 1 && you) {
            if (reveal_monster_at(x, y, FALSE)) {
                exercise(A_WIS, TRUE);
                /* changed mechanic = changed message; also
                   don't imply we're touching a trice */
                if (m_at(level, x, y)->m_ap_type)
                    pline(msgc_youdiscover,
                          "You think there's a mimic there.");
                else
                    pline(msgc_youdiscover,
                          "You sense a monster nearby!");
                return;
            }
            /* If there was an I there, and there still is,
               let the user know; this is needed to prevent
               autoexplore repeatedly searching the same
               square. The case where it's new is handled
               by reveal_monster_at, so we only need to
               handle the case where it isn't new.
               Check this last in case there's something more
               urgent to report. */
            if (level->locations[x][y].mem_invis && !autosearch)
                already = 1;
        }

        if ((trap = t_at(level, x, y)) && !trap->tseen) {
            int traprate = 1;
            if (ehas_property(mon, SEARCHING)) {
                traprate++;
                traprate += searchbon(mon);
            }
            if (traprate >= rnd(6)) {
                action_interrupted();
                find_trap(trap);
            }
        }

        if (already)
            pline(msgc_yafm, "There's still a monster there.");
    }
}

int
dosearch(const struct nh_cmd_arg *arg)
{
    limited_turns(arg, occ_search);
    return dosearch0(0);
}

/* Pre-map the sokoban levels (called during level creation)
 * Don't use map_<foo>: Those functions are meant to be used when the player
 * is on the level, which is not the case here. */
void
sokoban_detect(struct level *lev)
{
    int x, y;
    struct trap *ttmp;
    struct obj *obj;

    /* Map the background and boulders */
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++) {
            int cmap;

            lev->locations[x][y].seenv = SVALL;
            lev->locations[x][y].waslit = TRUE;
            /* seenv, waslit must be set before back_to_cmap is called, or
               it'll give us a view as seen from the wrong side (e.g. wall
               looks like rock if seen from an adjoining corridor) */
            cmap = back_to_cmap(lev, x, y);
            lev->locations[x][y].mem_bg = cmap;
            if (cmap == S_vodoor || cmap == S_hodoor || cmap == S_vcdoor ||
                cmap == S_hcdoor) {
                lev->locations[x][y].mem_door_l = 1;
                lev->locations[x][y].mem_door_t = 1;
            } else {
                /* mem_door_l, mem_door_t must be 0 for non-doors */
                lev->locations[x][y].mem_door_l = 0;
                lev->locations[x][y].mem_door_t = 0;
            }

            for (obj = lev->objects[x][y]; obj; obj = obj->nexthere)
                if (obj->otyp == BOULDER)
                    lev->locations[x][y].mem_obj =
                        what_obj(BOULDER, -1, -1, rn2) + 1;
        }

    /* Map the traps */
    for (ttmp = lev->lev_traps; ttmp; ttmp = ttmp->ntrap) {
        ttmp->tseen = 1;
        lev->locations[ttmp->tx][ttmp->ty].mem_trap =
            what_trap(ttmp->ttyp, -1, -1, rn2);
    }
}

/*detect.c*/
