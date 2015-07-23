/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-21 */
/* Copyright (c) Dean Luick, 1994                                       */
/* NetHack may be freely redistributed.  See license for details.       */

#include "hack.h"
#include "lev.h"
#include <stdint.h>

/*
 * Mobile light sources.
 *
 * This implementation minimizes memory at the expense of extra recalculations.
 *
 * Light sources are "things" that have a physical position and range.  They
 * have a type, which gives us information about them.  Currently they are only
 * attached to objects and monsters.  Note well: the polymorphed-player handling
 * assumes that youmonst.m_id will always remain 0.
 *
 * Light sources, like timers, either follow game play (RANGE_GLOBAL) or stay on
 * a level (RANGE_LEVEL).  Light sources are unique by their (type, id) pair.
 * For light sources attached to objects, this id is a pointer to the object.
 *
 * The major working function is do_light_sources(). It is called when the
 * vision system is recreating its "could see" array.  Here we add a flag
 * (TEMP_LIT) to the array for all locations that are lit via a light source.
 * The bad part of this is that we have to re-calculate the LOS of each light
 * source every time the vision system runs.  Even if the light sources and any
 * topology (vision blocking positions) have not changed.  The good part is that
 * no extra memory is used, plus we don't have to figure out how far the sources
 * have moved, or if the topology has changed.
 *
 * The structure of the save/restore mechanism is amazingly similar to the timer
 * save/restore.  This is because they both have the same principals of having
 * pointers into objects that must be recalculated across saves and restores.
 */

/* flags */
#define LSF_SHOW        0x1     /* display the light source */
#define LSF_NEEDS_FIXUP 0x2     /* need oid fixup */

static void write_ls(struct memfile *mf, light_source *);
static int maybe_write_ls(struct memfile *mf, struct level *lev, int range,
                          boolean write_it);

/* Create a new light source.  */
void
new_light_source(struct level *lev, xchar x, xchar y, int range, int type,
                 void *id)
{
    light_source *ls;

    if (range > MAX_RADIUS || range < 1) {
        impossible("new_light_source:  illegal range %d", range);
        return;
    }

    ls = malloc(sizeof (light_source));

    ls->next = lev->lev_lights;
    ls->x = x;
    ls->y = y;
    ls->range = range;
    ls->type = type;
    ls->id = id;
    ls->flags = 0;
    lev->lev_lights = ls;

    turnstate.vision_full_recalc = TRUE;     /* make the source show up */
}

/*
 * Delete a light source. This assumes only one light source is attached
 * to an object at a time.
 */
void
del_light_source(struct level *lev, int type, void *id)
{
    light_source *curr, *prev;
    intptr_t tmp_id;

    /* need to be prepared for dealing a with light source which has only been
       partially restored during a level change (in particular: chameleon vs
       prot. from shape changers) */
    switch (type) {
    case LS_OBJECT:
        tmp_id = (((struct obj *)id)->o_id);
        break;
    case LS_MONSTER:
        tmp_id = (((struct monst *)id)->m_id);
        break;
    default:
        tmp_id = 0;
        break;
    }

    for (prev = 0, curr = lev->lev_lights; curr;
         prev = curr, curr = curr->next) {
        if (curr->type != type)
            continue;
        if (curr->id ==
               ((curr->flags & LSF_NEEDS_FIXUP) ? (void *)tmp_id : id)) {
            if (prev)
                prev->next = curr->next;
            else
                lev->lev_lights = curr->next;

            free(curr);
            turnstate.vision_full_recalc = TRUE;
            return;
        }
    }
    impossible("del_light_source: not found type=%d, id=%p", type, id);
}

/* Mark locations that are temporarily lit via mobile light sources. */
void
do_light_sources(char **cs_rows)
{
    int x, y, min_x, max_x, max_y, offset;
    const char *limits;
    short at_hero_range = 0;
    light_source *ls;
    char *row;

    for (ls = level->lev_lights; ls; ls = ls->next) {
        ls->flags &= ~LSF_SHOW;

        /* 
         * Check for moved light sources.  It may be possible to
         * save some effort if an object has not moved, but not in
         * the current setup -- we need to recalculate for every
         * vision recalc.
         */
        if (ls->type == LS_OBJECT) {
            if (get_obj_location((struct obj *)ls->id, &ls->x, &ls->y, 0))
                ls->flags |= LSF_SHOW;
        } else if (ls->type == LS_MONSTER) {
            if (get_mon_location((struct monst *)ls->id, &ls->x, &ls->y, 0))
                ls->flags |= LSF_SHOW;
        }

        /* minor optimization: don't bother with duplicate light sources at
           hero */
        if (ls->x == u.ux && ls->y == u.uy) {
            if (at_hero_range >= ls->range)
                ls->flags &= ~LSF_SHOW;
            else
                at_hero_range = ls->range;
        }

        if (ls->flags & LSF_SHOW) {
            /* 
             * Walk the points in the circle and see if they are
             * visible from the center.  If so, mark'em.
             *
             * Kevin's tests indicated that doing this brute-force
             * method is faster for radius <= 3 (or so).
             */
            limits = circle_ptr(ls->range);
            if ((max_y = (ls->y + ls->range)) >= ROWNO)
                max_y = ROWNO - 1;
            if ((y = (ls->y - ls->range)) < 0)
                y = 0;
            for (; y <= max_y; y++) {
                row = cs_rows[y];
                offset = limits[abs(y - ls->y)];
                if ((min_x = (ls->x - offset)) < 0)
                    min_x = 0;
                if ((max_x = (ls->x + offset)) >= COLNO)
                    max_x = COLNO - 1;

                for (x = min_x; x <= max_x; x++)
                    if (clear_path((int)ls->x, (int)ls->y, x, y, cs_rows))
                        row[x] |= TEMP_LIT;
            }
        }
    }
}

/* (mon->mx == COLNO) implies migrating */
#define mon_is_local(mon) ((mon) != &youmonst && (mon)->mx != COLNO)

struct monst *
find_mid(struct level *lev, unsigned nid, unsigned fmflags)
{
    struct monst *mtmp;

    if (!nid)
        return &youmonst;
    if (fmflags & FM_FMON)
        for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
            if (!DEADMONSTER(mtmp) && mtmp->m_id == nid)
                return mtmp;
    if (fmflags & FM_MIGRATE)
        for (mtmp = migrating_mons; mtmp; mtmp = mtmp->nmon)
            if (mtmp->m_id == nid)
                return mtmp;
    if (fmflags & FM_MYDOGS)
        for (mtmp = turnstate.migrating_pets; mtmp; mtmp = mtmp->nmon)
            if (mtmp->m_id == nid)
                return mtmp;
    return NULL;
}


void
transfer_lights(struct level *oldlev, struct level *newlev, unsigned int obj_id)
{
    light_source **prev, *curr;
    boolean transfer;

    if (newlev == oldlev)
        return;

    for (prev = &oldlev->lev_lights; (curr = *prev) != 0;) {
        switch (curr->type) {
        case LS_OBJECT:
            if (obj_id)
                transfer = (((struct obj *)curr->id)->o_id == obj_id);
            else
                transfer = !obj_is_local((struct obj *)curr->id);
            break;
        case LS_MONSTER:
            if (obj_id)
                transfer = FALSE;
            else
                transfer = !mon_is_local((struct monst *)curr->id);
            break;
        default:
            transfer = FALSE;
            break;
        }
        /* associate light sources with the new level */
        if (transfer) {
            *prev = curr->next;
            curr->next = newlev->lev_lights;
            newlev->lev_lights = curr;
        } else {
            prev = &(*prev)->next;
        }
    }
}


/* Save all light sources of the given range. */
void
save_light_sources(struct memfile *mf, struct level *lev, int range)
{
    int count, actual;

    mtag(mf, 2 * (int)ledger_no(&lev->z) + range, MTAG_LIGHTS);
    count = maybe_write_ls(mf, lev, range, FALSE);
    mwrite32(mf, count);

    actual = maybe_write_ls(mf, lev, range, TRUE);
    if (actual != count)
        panic("counted %d light sources, wrote %d! [range=%d]", count, actual,
              range);
}


void
free_light_sources(struct level *lev)
{
    light_source *next, *curr;

    for (curr = lev->lev_lights; curr; curr = next) {
        next = curr->next;
        free(curr);
    }
    lev->lev_lights = NULL;
}


/*
 * Pull in the structures from disk, but don't recalculate the object
 * pointers.
 */
void
restore_light_sources(struct memfile *mf, struct level *lev)
{
    int count;
    intptr_t id;
    /* we need to restore and preserve order *but* we don't need
     * to preserve the absolute order, only the relative orders of all
     * global lights and all local lights---this is because global
     * lights are for stored on the level and moved around as you do.
     * As a result, they are saved in different places, and consequently
     * it will *not* cause a desync as long as relative order is preserved.
     * So it doesn't actually matter that we retain relative order to other
     * light sources on the level, only that we restore the light sources here
     * in reverse order. Inserting immediately after the first light leads to
     * the simplest code.
     */
    light_source *ls, *prev = lev->lev_lights, *rest = prev ? prev->next : NULL;

    /* restore elements */
    count = mread32(mf);

    while (count-- > 0) {
        ls = malloc(sizeof (light_source));
        ls->type = mread32(mf);
        ls->range = mread16(mf);
        ls->flags = mread16(mf);
        id = mread32(mf);
        ls->id = (void *)id;
        ls->x = mread8(mf);
        ls->y = mread8(mf);

        ls->next = rest;
        if (prev)
            prev->next = ls;
        else
            lev->lev_lights = ls;
        prev = ls;
    }
}

/* Relink all lights that are so marked. */
void
relink_light_sources(boolean ghostly, struct level *lev)
{
    char which;
    unsigned nid;
    light_source *ls;

    for (ls = lev->lev_lights; ls; ls = ls->next) {
        if (ls->flags & LSF_NEEDS_FIXUP) {
            if (ls->type == LS_OBJECT || ls->type == LS_MONSTER) {
                if (ghostly) {
                    if (!lookup_id_mapping((intptr_t) ls->id, &nid))
                        impossible("relink_light_sources: no id mapping");
                } else
                    nid = (intptr_t) ls->id;
                if (ls->type == LS_OBJECT) {
                    which = 'o';
                    ls->id = find_oid(nid);
                } else {
                    which = 'm';
                    ls->id = find_mid(lev, nid, FM_EVERYWHERE);
                }
                if (!ls->id)
                    impossible("relink_light_sources: cant find %c_id %d",
                               which, nid);
            } else
                impossible("relink_light_sources: bad type (%d)", ls->type);

            ls->flags &= ~LSF_NEEDS_FIXUP;
        }
    }
}

/*
 * Part of the light source save routine.  Count up the number of light
 * sources that would be written.  If write_it is true, actually write
 * the light source out.
 */
static int
maybe_write_ls(struct memfile *mf, struct level *lev, int range,
               boolean write_it)
{
    int count = 0, is_global;
    light_source *ls;

    for (ls = lev->lev_lights; ls; ls = ls->next) {
        if (!ls->id) {
            impossible("maybe_write_ls: no id! [range=%d]", range);
            continue;
        }
        switch (ls->type) {
        case LS_OBJECT:
            is_global = !obj_is_local((struct obj *)ls->id);
            break;
        case LS_MONSTER:
            is_global = !mon_is_local((struct monst *)ls->id);
            break;
        default:
            is_global = 0;
            impossible("maybe_write_ls: bad type (%d) [range=%d]", ls->type,
                       range);
            break;
        }
        /* if global and not doing local, or vice versa, count it */
        if (is_global ^ (range == RANGE_LEVEL)) {
            count++;
            if (write_it)
                write_ls(mf, ls);
        }
    }

    return count;
}

/* Write a light source structure to disk. */
static void
write_ls(struct memfile *mf, light_source * ls)
{
    struct obj *otmp;
    struct monst *mtmp;
    long id;

    if ((ls->flags & LSF_NEEDS_FIXUP)) {
        id = *(int32_t *) ls->id;
    } else if (ls->type == LS_OBJECT) {
        otmp = (struct obj *)ls->id;
        id = otmp->o_id;
    } else if (ls->type == LS_MONSTER) {
        mtmp = (struct monst *)ls->id;
        id = mtmp->m_id;
    } else {
        impossible("write_ls: bad type (%d)", ls->type);
        id = 0;
    }


    mtag(mf, id * 2 + ls->type, MTAG_LIGHT);
    mwrite32(mf, ls->type);
    mwrite16(mf, ls->range);

    mwrite16(mf, ls->flags | LSF_NEEDS_FIXUP);
    mwrite32(mf, id);
    mwrite8(mf, ls->x);
    mwrite8(mf, ls->y);
}

/* Change light source's ID from src to dest. */
void
obj_move_light_source(struct obj *src, struct obj *dest)
{
    light_source *ls;

    for (ls = src->olev->lev_lights; ls; ls = ls->next)
        if (ls->type == LS_OBJECT && ls->id == src)
            ls->id = dest;
    src->lamplit = 0;
    dest->lamplit = 1;
}

/* return true if there exist any light sources */
boolean
any_light_source(void)
{
    return level->lev_lights != NULL;
}

/*
 * Snuff an object light source if at (x,y).  This currently works
 * only for burning light sources.
 */
void
snuff_light_source(int x, int y)
{
    light_source *ls;
    struct obj *obj;

    for (ls = level->lev_lights; ls; ls = ls->next)
        /* 
           Is this position check valid??? Can I assume that the positions will 
           always be correct because the objects would have been updated with
           the last vision update? [Is that recent enough???] */
        if (ls->type == LS_OBJECT && ls->x == x && ls->y == y) {
            obj = (struct obj *)ls->id;
            if (obj_is_burning(obj)) {
                /* The only way to snuff Sunsword is to unwield it.  Darkness
                   scrolls won't affect it.  (If we got here because it was
                   dropped or thrown inside a monster, this won't matter anyway
                   because it will go out when dropped.) */
                if (artifact_light(obj))
                    continue;
                end_burn(obj, obj->otyp != MAGIC_LAMP);
                /* 
                 * The current ls element has just been removed (and
                 * ls->next is now invalid).  Return assuming that there
                 * is only one light source attached to each object.
                 */
                return;
            }
        }
}

/* Return TRUE if object sheds any light at all. */
boolean
obj_sheds_light(struct obj * obj)
{
    /* so far, only burning objects shed light */
    return obj_is_burning(obj);
}

/* Return TRUE if sheds light AND will be snuffed by end_burn(). */
boolean
obj_is_burning(struct obj * obj)
{
    return (obj->lamplit &&
            (obj->otyp == MAGIC_LAMP || ignitable(obj) || artifact_light(obj)));
}

/* copy the light source(s) attachted to src, and attach it/them to dest */
void
obj_split_light_source(struct obj *src, struct obj *dest)
{
    light_source *ls, *new_ls;

    for (ls = level->lev_lights; ls; ls = ls->next)
        if (ls->type == LS_OBJECT && ls->id == src) {
            /* 
             * Insert the new source at beginning of list.  This will
             * never interfere us walking down the list - we are already
             * past the insertion point.
             */
            new_ls = malloc(sizeof (light_source));
            *new_ls = *ls;
            if (Is_candle(src)) {
                /* split candles may emit less light than original group */
                ls->range = candle_light_range(src);
                new_ls->range = candle_light_range(dest);
                turnstate.vision_full_recalc = TRUE; /* in case range changed */
            }
            new_ls->id = dest;
            new_ls->next = level->lev_lights;
            level->lev_lights = new_ls;
            dest->lamplit = 1;  /* now an active light source */
        }
}

/* light source `src' has been folded into light source `dest';
   used for merging lit candles and adding candle(s) to lit candelabrum */
void
obj_merge_light_sources(struct obj *src, struct obj *dest)
{
    light_source *ls;

    /* src == dest implies adding to candelabrum */
    if (src != dest)
        end_burn(src, TRUE);    /* extinguish candles */

    for (ls = level->lev_lights; ls; ls = ls->next)
        if (ls->type == LS_OBJECT && ls->id == dest) {
            ls->range = candle_light_range(dest);
            turnstate.vision_full_recalc = TRUE;     /* in case range changed */
            break;
        }
}

/* Candlelight is proportional to the number of candles;
   minimum range is 2 rather than 1 for playability. */
int
candle_light_range(struct obj *obj)
{
    int radius;

    if (obj->otyp == CANDELABRUM_OF_INVOCATION) {
        /* 
         *      The special candelabrum emits more light than the
         *      corresponding number of candles would.
         *       1..3 candles, range 2 (minimum range);
         *       4..6 candles, range 3 (normal lamp range);
         *          7 candles, range 4 (bright).
         */
        radius = (obj->spe < 4) ? 2 : (obj->spe < 7) ? 3 : 4;
    } else if (Is_candle(obj)) {
        /* 
         *      Range is incremented by powers of 7 so that it will take
         *      wizard mode quantities of candles to get more light than
         *      from a lamp, without imposing an arbitrary limit.
         *       1..6   candles, range 2;
         *       7..48  candles, range 3;
         *      49..342 candles, range 4; &c.
         */
        long n = obj->quan;

        radius = 1;     /* always incremented at least once */
        do {
            radius++;
            n /= 7L;
        } while (n > 0L);
    } else {
        /* we're only called for lit candelabrum or candles */
        /* impossible("candlelight for %d?", obj->otyp); */
        radius = 3;     /* lamp's value */
    }
    return radius;
}


int
wiz_light_sources(const struct nh_cmd_arg *arg)
{
    struct nh_menulist menu;
    const char *buf;
    light_source *ls;

    (void) arg;

    init_menulist(&menu);

    buf = msgprintf("Mobile light sources: hero @ (%2d,%2d)", u.ux, u.uy);
    add_menutext(&menu, buf);
    add_menutext(&menu, "");

    if (level->lev_lights) {
        add_menutext(&menu, "location range flags  type    id");
        add_menutext(&menu, "-------- ----- ------ ----  -------");
        for (ls = level->lev_lights; ls; ls = ls->next) {
            buf = msgprintf("  %2d,%2d   %2d   0x%04x  %s  %p",
                    ls->x, ls->y, ls->range, ls->flags,
                    (ls->type == LS_OBJECT ? "obj" :
                     ls->type == LS_MONSTER ? (
                       mon_is_local((struct monst *)ls->id) ? "mon" :
                       ((struct monst *)ls->id == &youmonst) ? "you" :
                       "<m>") : /* migrating monster */ "???"),
                    ls->id);
            add_menutext(&menu, buf);
        }
    } else
        add_menutext(&menu, "<none>");


    display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);

    return 0;
}

/*light.c*/

