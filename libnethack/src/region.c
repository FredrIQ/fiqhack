/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) 1996 by Jean-Christophe Collet  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"

/*
 * Regions are currently used for only one purpose: stinking gas clouds
 * This is reflected in the display code: visible regions are displayed in the
 * effect layer
 */

#define NO_CALLBACK (-1)

static boolean inside_gas_cloud(void *, void *);
static boolean expire_gas_cloud(void *, void *);
static boolean inside_rect(struct nhrect *, int, int);
static boolean inside_region(struct region *, int, int);
static struct region *create_region(struct nhrect *, int);
static void add_rect_to_reg(struct region *, struct nhrect *);
static void add_mon_to_reg(struct region *, struct monst *);
static void remove_mon_from_reg(struct region *, struct monst *);
static boolean mon_in_region(struct region *, struct monst *);

void free_region(struct region *);
void add_region(struct level *lev, struct region *);
void remove_region(struct region *);


static void reset_region_mids(struct region *);

static const callback_proc callbacks[] = {
#define INSIDE_GAS_CLOUD 0
    inside_gas_cloud,
#define EXPIRE_GAS_CLOUD 1
    expire_gas_cloud
};

/* Should be inlined. */
static boolean
inside_rect(struct nhrect *r, int x, int y)
{
    return x >= r->lx && x <= r->hx && y >= r->ly && y <= r->hy;
}

/*
 * Check if a point is inside a region.
 */
static boolean
inside_region(struct region *reg, int x, int y)
{
    int i;

    if (reg == NULL || !inside_rect(&(reg->bounding_box), x, y))
        return FALSE;
    for (i = 0; i < reg->nrects; i++)
        if (inside_rect(&(reg->rects[i]), x, y))
            return TRUE;
    return FALSE;
}

/*
 * Create a region. It does not activate it.
 */
static struct region *
create_region(struct nhrect *rects, int nrect)
{
    int i;
    struct region *reg;

    reg = malloc(sizeof (struct region));
    memset(reg, 0, sizeof (struct region));

    /* Determines bounding box */
    if (nrect > 0) {
        reg->bounding_box = rects[0];
    } else {
        reg->bounding_box.lx = 99;
        reg->bounding_box.ly = 99;
        reg->bounding_box.hx = 0;
        reg->bounding_box.hy = 0;
    }
    reg->nrects = nrect;
    reg->rects = nrect > 0 ? malloc((sizeof (struct nhrect)) * nrect) : NULL;
    for (i = 0; i < nrect; i++) {
        if (rects[i].lx < reg->bounding_box.lx)
            reg->bounding_box.lx = rects[i].lx;
        if (rects[i].ly < reg->bounding_box.ly)
            reg->bounding_box.ly = rects[i].ly;
        if (rects[i].hx > reg->bounding_box.hx)
            reg->bounding_box.hx = rects[i].hx;
        if (rects[i].hy > reg->bounding_box.hy)
            reg->bounding_box.hy = rects[i].hy;
        reg->rects[i] = rects[i];
    }
    reg->ttl = -1;      /* Defaults */
    reg->expire_f = NO_CALLBACK;
    reg->enter_f = NO_CALLBACK;
    reg->can_enter_f = NO_CALLBACK;
    reg->leave_f = NO_CALLBACK;
    reg->can_leave_f = NO_CALLBACK;
    reg->inside_f = NO_CALLBACK;
    clear_hero_inside(reg);
    clear_heros_fault(reg);

    return reg;
}

/*
 * Add rectangle to region.
 */
static void
add_rect_to_reg(struct region *reg, struct nhrect *rect)
{
    struct nhrect *tmp_rect;

    tmp_rect = malloc(sizeof (struct nhrect) * (reg->nrects + 1));
    if (reg->nrects > 0) {
        memcpy(tmp_rect, reg->rects, (sizeof (struct nhrect) * reg->nrects));
        free(reg->rects);
    }
    tmp_rect[reg->nrects] = *rect;
    reg->nrects++;
    reg->rects = tmp_rect;
    /* Update bounding box if needed */
    if (reg->bounding_box.lx > rect->lx)
        reg->bounding_box.lx = rect->lx;
    if (reg->bounding_box.ly > rect->ly)
        reg->bounding_box.ly = rect->ly;
    if (reg->bounding_box.hx < rect->hx)
        reg->bounding_box.hx = rect->hx;
    if (reg->bounding_box.hy < rect->hy)
        reg->bounding_box.hy = rect->hy;
}

/*
 * Add a monster to the region
 */
static void
add_mon_to_reg(struct region *reg, struct monst *mon)
{
    int i;
    unsigned *tmp_m;

    if (reg->max_monst <= reg->n_monst) {
        tmp_m = malloc(sizeof (unsigned) * (reg->max_monst + MONST_INC));
        if (reg->max_monst > 0) {
            for (i = 0; i < reg->max_monst; i++)
                tmp_m[i] = reg->monsters[i];
            free(reg->monsters);
        }
        reg->monsters = tmp_m;
        reg->max_monst += MONST_INC;
    }
    reg->monsters[reg->n_monst++] = mon->m_id;
}

/*
 * Remove a monster from the region list (it left or died...)
 */
static void
remove_mon_from_reg(struct region *reg, struct monst *mon)
{
    int i;

    for (i = 0; i < reg->n_monst; i++)
        if (reg->monsters[i] == mon->m_id) {
            reg->n_monst--;
            reg->monsters[i] = reg->monsters[reg->n_monst];
            return;
        }
}

/*
 * Check if a monster is inside the region.
 * It's probably quicker to check with the region internal list
 * than to check for coordinates.
 */
static boolean
mon_in_region(struct region *reg, struct monst *mon)
{
    int i;

    for (i = 0; i < reg->n_monst; i++)
        if (reg->monsters[i] == mon->m_id)
            return TRUE;
    return FALSE;
}


/*
 * Free mem from region.
 */
void
free_region(struct region *reg)
{
    if (reg) {
        if (reg->rects)
            free(reg->rects);
        if (reg->monsters)
            free(reg->monsters);
        free(reg);
    }
}

/*
 * Add a region to the list.
 * This actually activates the region.
 */
void
add_region(struct level *lev, struct region *reg)
{
    struct region **tmp_reg;
    int i, j;

    if (lev->max_regions <= lev->n_regions) {
        tmp_reg = lev->regions;
        lev->regions =
            malloc(sizeof (struct region *) * (lev->max_regions + 10));
        if (lev->max_regions > 0) {
            memcpy(lev->regions, tmp_reg,
                   lev->max_regions * sizeof (struct region *));
            free(tmp_reg);
        }
        lev->max_regions += 10;
    }
    reg->lev = lev;
    lev->regions[lev->n_regions] = reg;
    lev->n_regions++;
    /* Check for monsters inside the region */
    for (i = reg->bounding_box.lx; i <= reg->bounding_box.hx; i++)
        for (j = reg->bounding_box.ly; j <= reg->bounding_box.hy; j++) {
            /* Some lev->regions can cross the level boundaries */
            if (!isok(i, j))
                continue;
            if (MON_AT(level, i, j) && inside_region(reg, i, j))
                add_mon_to_reg(reg, level->monsters[i][j]);
            if (reg->visible && cansee(i, j))
                newsym(i, j);
        }
    /* Check for player now... */
    if (inside_region(reg, u.ux, u.uy))
        set_hero_inside(reg);
    else
        clear_hero_inside(reg);
}

/*
 * Remove a region from the list & free it.
 */
void
remove_region(struct region *reg)
{
    int i, x, y;
    struct level *lev = reg->lev;

    for (i = 0; i < reg->lev->n_regions; i++)
        if (reg->lev->regions[i] == reg)
            break;
    if (i == reg->lev->n_regions)
        return;

    /* Update screen if necessary */
    if (reg->visible && level == lev)
        for (x = reg->bounding_box.lx; x <= reg->bounding_box.hx; x++)
            for (y = reg->bounding_box.ly; y <= reg->bounding_box.hy; y++)
                if (isok(x, y) && inside_region(reg, x, y) && cansee(x, y))
                    newsym(x, y);

    free_region(reg);
    lev->regions[i] = lev->regions[lev->n_regions - 1];
    lev->regions[lev->n_regions - 1] = NULL;
    lev->n_regions--;
}

/* Remove all regions and clear all related data. */
void
free_regions(struct level *lev)
{
    int i;

    for (i = 0; i < lev->n_regions; i++)
        free_region(lev->regions[i]);
    lev->n_regions = 0;
    if (lev->max_regions > 0)
        free(lev->regions);
    lev->max_regions = 0;
    lev->regions = NULL;
}

/*
 * This function is called every turn.
 * It makes the regions age, if necessary and calls the appropriate
 * callbacks when needed.
 */
void
run_regions(struct level *lev)
{
    int i, j, k;
    int f_indx;

    /* End of life ? */
    /* Do it backward because the array will be modified */
    for (i = lev->n_regions - 1; i >= 0; i--) {
        if (lev->regions[i]->ttl == 0) {
            if ((f_indx = lev->regions[i]->expire_f) == NO_CALLBACK ||
                (*callbacks[f_indx]) (lev->regions[i], 0))
                remove_region(lev->regions[i]);
        }
    }

    /* Process remaining lev->regions */
    for (i = 0; i < lev->n_regions; i++) {
        /* Make the region age */
        if (lev->regions[i]->ttl > 0)
            lev->regions[i]->ttl--;
        /* Check if player is inside region */
        f_indx = lev->regions[i]->inside_f;
        if (f_indx != NO_CALLBACK && hero_inside(lev->regions[i]))
            (void)(*callbacks[f_indx]) (lev->regions[i], 0);
        /* Check if any monster is inside region */
        if (f_indx != NO_CALLBACK) {
            for (j = 0; j < lev->regions[i]->n_monst; j++) {
                struct monst *mtmp =
                    find_mid(lev, lev->regions[i]->monsters[j], FM_FMON);

                if (!mtmp || DEADMONSTER(mtmp) ||
                    (*callbacks[f_indx]) (lev->regions[i], mtmp)) {
                    /* The monster died, remove it from list */
                    k = (lev->regions[i]->n_monst -= 1);
                    lev->regions[i]->monsters[j] = lev->regions[i]->monsters[k];
                    lev->regions[i]->monsters[k] = 0;
                    --j;        /* current slot has been reused; recheck it
                                   next */
                }
            }
        }
    }
}

/*
 * check whether player enters/leaves one or more regions.
 */
boolean
in_out_region(struct level *lev, xchar x, xchar y)
{
    int i, f_indx;

    /* First check if we can do the move */
    for (i = 0; i < lev->n_regions; i++) {
        if (inside_region(lev->regions[i], x, y)
            && !hero_inside(lev->regions[i]) && !lev->regions[i]->attach_2_u) {
            if ((f_indx = lev->regions[i]->can_enter_f) != NO_CALLBACK)
                if (!(*callbacks[f_indx]) (lev->regions[i], 0))
                    return FALSE;
        } else if (hero_inside(lev->regions[i])
                   && !inside_region(lev->regions[i], x, y)
                   && !lev->regions[i]->attach_2_u) {
            if ((f_indx = lev->regions[i]->can_leave_f) != NO_CALLBACK)
                if (!(*callbacks[f_indx]) (lev->regions[i], 0))
                    return FALSE;
        }
    }

    /* Callbacks for the regions we do leave */
    for (i = 0; i < lev->n_regions; i++)
        if (hero_inside(lev->regions[i]) && !lev->regions[i]->attach_2_u &&
            !inside_region(lev->regions[i], x, y)) {
            clear_hero_inside(lev->regions[i]);
            if (lev->regions[i]->leave_msg != NULL)
                pline("%s", lev->regions[i]->leave_msg);
            if ((f_indx = lev->regions[i]->leave_f) != NO_CALLBACK)
                (void)(*callbacks[f_indx]) (lev->regions[i], 0);
        }

    /* Callbacks for the regions we do enter */
    for (i = 0; i < lev->n_regions; i++)
        if (!hero_inside(lev->regions[i]) && !lev->regions[i]->attach_2_u &&
            inside_region(lev->regions[i], x, y)) {
            set_hero_inside(lev->regions[i]);
            if (lev->regions[i]->enter_msg != NULL)
                pline("%s", lev->regions[i]->enter_msg);
            if ((f_indx = lev->regions[i]->enter_f) != NO_CALLBACK)
                (void)(*callbacks[f_indx]) (lev->regions[i], 0);
        }
    return TRUE;
}

/*
 * check whether a monster enters/leaves one or more region.
*/
boolean
m_in_out_region(struct monst * mon, xchar x, xchar y)
{
    int i, f_indx;

    /* First check if we can do the move */
    for (i = 0; i < mon->dlevel->n_regions; i++) {
        if (inside_region(mon->dlevel->regions[i], x, y) &&
            !mon_in_region(mon->dlevel->regions[i], mon) &&
            mon->dlevel->regions[i]->attach_2_m != mon->m_id) {
            if ((f_indx = mon->dlevel->regions[i]->can_enter_f) != NO_CALLBACK)
                if (!(*callbacks[f_indx]) (mon->dlevel->regions[i], mon))
                    return FALSE;
        } else if (mon_in_region(mon->dlevel->regions[i], mon) &&
                   !inside_region(mon->dlevel->regions[i], x, y) &&
                   mon->dlevel->regions[i]->attach_2_m != mon->m_id) {
            if ((f_indx = mon->dlevel->regions[i]->can_leave_f) != NO_CALLBACK)
                if (!(*callbacks[f_indx]) (mon->dlevel->regions[i], mon))
                    return FALSE;
        }
    }

    /* Callbacks for the regions we do leave */
    for (i = 0; i < mon->dlevel->n_regions; i++)
        if (mon_in_region(mon->dlevel->regions[i], mon) &&
            mon->dlevel->regions[i]->attach_2_m != mon->m_id &&
            !inside_region(mon->dlevel->regions[i], x, y)) {
            remove_mon_from_reg(mon->dlevel->regions[i], mon);
            if ((f_indx = mon->dlevel->regions[i]->leave_f) != NO_CALLBACK)
                (void)(*callbacks[f_indx]) (mon->dlevel->regions[i], mon);
        }

    /* Callbacks for the regions we do enter */
    for (i = 0; i < mon->dlevel->n_regions; i++)
        if (!hero_inside(mon->dlevel->regions[i]) &&
            !mon->dlevel->regions[i]->attach_2_u &&
            inside_region(mon->dlevel->regions[i], x, y)) {
            add_mon_to_reg(mon->dlevel->regions[i], mon);
            if ((f_indx = mon->dlevel->regions[i]->enter_f) != NO_CALLBACK)
                (void)(*callbacks[f_indx]) (mon->dlevel->regions[i], mon);
        }
    return TRUE;
}

/*
 * Checks player's regions after a teleport for instance.
 */
void
update_player_regions(struct level *lev)
{
    int i;

    for (i = 0; i < lev->n_regions; i++)
        if (!lev->regions[i]->attach_2_u &&
            inside_region(lev->regions[i], u.ux, u.uy))
            set_hero_inside(lev->regions[i]);
        else
            clear_hero_inside(lev->regions[i]);
}

/*
 * Ditto for a specified monster.
 */
void
update_monster_region(struct monst *mon)
{
    int i;

    for (i = 0; i < mon->dlevel->n_regions; i++) {
        if (inside_region(mon->dlevel->regions[i], mon->mx, mon->my)) {
            if (!mon_in_region(mon->dlevel->regions[i], mon))
                add_mon_to_reg(mon->dlevel->regions[i], mon);
        } else {
            if (mon_in_region(mon->dlevel->regions[i], mon))
                remove_mon_from_reg(mon->dlevel->regions[i], mon);
        }
    }
}


/*
 * Check if a spot is under a visible region (eg: gas cloud).
 * Returns NULL if not, otherwise returns region.
 */
struct region *
visible_region_at(struct level *lev, xchar x, xchar y)
{
    int i;

    for (i = 0; i < lev->n_regions; i++)
        if (inside_region(lev->regions[i], x, y) && lev->regions[i]->visible &&
            lev->regions[i]->ttl != 0)
            return lev->regions[i];
    return NULL;
}


static void
save_rect(struct memfile *mf, struct nhrect r)
{
    mwrite8(mf, r.lx);
    mwrite8(mf, r.ly);
    mwrite8(mf, r.hx);
    mwrite8(mf, r.hy);
}


/**
 * save_regions :
 */
void
save_regions(struct memfile *mf, struct level *lev)
{
    int i, j;
    unsigned len1, len2;
    struct region *r;

    mfmagic_set(mf, REGION_MAGIC);
    mtag(mf, ledger_no(&lev->z), MTAG_REGION);
    mwrite32(mf, save_encode_32(moves, moves, moves));    /* timestamp */
    mwrite32(mf, lev->n_regions);

    /* Note: level regions don't have ID numbers, so we can't tag individual
       regions; instead, diff efficiency depends on the fact that regions tend
       to stay in the order and are typically inserted or deleted near the end
       of the list */
    for (i = 0; i < lev->n_regions; i++) {
        r = lev->regions[i];

        save_rect(mf, r->bounding_box);
        mwrite32(mf, r->attach_2_m);
        mwrite32(mf, r->effect_id);
        mwrite32(mf, r->arg);
        mwrite16(mf, r->nrects);
        for (j = 0; j < r->nrects; j++)
            save_rect(mf, r->rects[j]);
        mwrite16(mf, r->ttl);
        mwrite16(mf, r->expire_f);
        mwrite16(mf, r->can_enter_f);
        mwrite16(mf, r->enter_f);
        mwrite16(mf, r->can_leave_f);
        mwrite16(mf, r->leave_f);
        mwrite16(mf, r->inside_f);
        mwrite16(mf, r->n_monst);
        mwrite8(mf, r->player_flags);
        mwrite8(mf, r->attach_2_u);
        mwrite8(mf, r->visible);

        for (j = 0; j < r->n_monst; j++)
            mwrite32(mf, r->monsters[j]);

        len1 = r->enter_msg ? strlen(r->enter_msg) + 1 : 0;
        mwrite16(mf, len1);
        len2 = r->leave_msg ? strlen(r->leave_msg) + 1 : 0;
        mwrite16(mf, len2);

        if (len1 > 0)
            mwrite(mf, r->enter_msg, len1);

        if (len2 > 0)
            mwrite(mf, r->leave_msg, len2);
    }
}


static void
restore_rect(struct memfile *mf, struct nhrect *r)
{
    r->lx = mread8(mf);
    r->ly = mread8(mf);
    r->hx = mread8(mf);
    r->hy = mread8(mf);
}


void
rest_regions(struct memfile *mf, struct level *lev, boolean ghostly)
{       /* If a bones file restore */
    int i, j;
    unsigned len1, len2;
    long tmstamp;
    char *msg_buf;
    struct region *r;

    free_regions(lev);  /* Just for security */
    mfmagic_check(mf, REGION_MAGIC);
    tmstamp = save_decode_32(mread32(mf), moves, moves);
    if (ghostly)
        tmstamp = 0;
    else
        tmstamp = (moves - tmstamp);
    lev->n_regions = mread32(mf);
    lev->max_regions = lev->n_regions;
    if (lev->n_regions > 0)
        lev->regions = malloc(sizeof (struct region *) * lev->n_regions);

    for (i = 0; i < lev->n_regions; i++) {
        lev->regions[i] = malloc(sizeof (struct region));

        r = lev->regions[i];
        memset(r, 0, sizeof (struct region));

        r->lev = lev;

        restore_rect(mf, &r->bounding_box);
        r->attach_2_m = mread32(mf);
        r->effect_id = mread32(mf);
        r->arg = mread32(mf);
        r->nrects = mread16(mf);
        if (r->nrects > 0)
            r->rects = malloc(sizeof (struct nhrect) * r->nrects);
        for (j = 0; j < r->nrects; j++)
            restore_rect(mf, &r->rects[j]);
        r->ttl = mread16(mf);
        r->expire_f = mread16(mf);
        r->can_enter_f = mread16(mf);
        r->enter_f = mread16(mf);
        r->can_leave_f = mread16(mf);
        r->leave_f = mread16(mf);
        r->inside_f = mread16(mf);
        r->n_monst = mread16(mf);
        r->max_monst = r->n_monst;
        r->player_flags = mread8(mf);
        r->attach_2_u = mread8(mf);
        r->visible = mread8(mf);

        if (r->n_monst > 0)
            r->monsters = malloc(sizeof (unsigned) * r->n_monst);
        for (j = 0; j < r->n_monst; j++)
            r->monsters[j] = mread32(mf);

        len1 = mread16(mf);
        len2 = mread16(mf);

        if (len1 > 0) {
            msg_buf = malloc(len1);
            mread(mf, msg_buf, len1);
            msg_buf[len1 - 1] = '\0';
            r->enter_msg = msg_buf;
        }

        if (len2 > 0) {
            msg_buf = malloc(len2);
            mread(mf, msg_buf, len2);
            msg_buf[len2 - 1] = '\0';
            r->leave_msg = msg_buf;
        }

        /* check for expired region */
        if (r->ttl >= 0)
            r->ttl = (r->ttl > tmstamp) ? r->ttl - tmstamp : 0;

        if (ghostly) {  /* settings pertained to old player */
            clear_hero_inside(r);
            clear_heros_fault(r);
        }
    }
    for (i = lev->n_regions - 1; i >= 0; i--)
        if (ghostly && lev->regions[i]->n_monst > 0)
            reset_region_mids(lev->regions[i]);
}

/* update monster IDs for region being loaded from bones; `ghostly' implied */
static void
reset_region_mids(struct region *reg)
{
    int i = 0, n = reg->n_monst;
    unsigned *mid_list = reg->monsters;

    while (i < n)
        if (!lookup_id_mapping(mid_list[i], &mid_list[i])) {
            /* shrink list to remove missing monster; order doesn't matter */
            mid_list[i] = mid_list[--n];
        } else {
            /* move on to next monster */
            ++i;
        }
    reg->n_monst = n;
    return;
}


/*--------------------------------------------------------------*
 *                                                              *
 *                      Gas cloud related code                  *
 *                                                              *
 *--------------------------------------------------------------*/

/*
 * Here is an example of an expire function that may prolong
 * region life after some mods...
 */
boolean
expire_gas_cloud(void *p1, void *p2)
{
    struct region *reg;
    int damage;

    reg = (struct region *)p1;
    damage = reg->arg;

    /* If it was a thick cloud, it dissipates a little first */
    if (damage >= 5) {
        damage /= 2;    /* It dissipates, let's do less damage */
        reg->arg = damage;
        reg->ttl = 2;   /* Here's the trick : reset ttl */
        return FALSE;   /* THEN return FALSE, means "still there" */
    }
    return TRUE;        /* OK, it's gone, you can free it! */
}

boolean
inside_gas_cloud(void *p1, void *p2)
{
    struct region *reg;
    struct monst *mtmp;
    long dam;

    reg = (struct region *)p1;
    dam = (long)reg->arg;
    if (p2 == NULL) {   /* This means *YOU* Bozo! */
        if (nonliving(youmonst.data) || u.uinvulnerable)
            return FALSE;
        /* If you will unblind next turn, extend the blindness so that you do
         * not get a "You can see again!" message immediately before being
         * blinded again. */
        if (!Blind || Blinded == 1)
            make_blinded(2L, FALSE);
        if (Breathless)
            return FALSE;
        if (!Poison_resistance) {
            pline("Something is burning your %s!", makeplural(body_part(LUNG)));
            pline("You cough and spit blood!");
            losehp(rnd(dam) + 5, killer_msg(DIED, "a gas cloud"));
            return FALSE;
        } else {
            pline("You cough!");
            return FALSE;
        }
    } else {    /* A monster is inside the cloud */
        mtmp = (struct monst *)p2;

        /* Non living and non breathing monsters are not concerned */
        if (!nonliving(mtmp->data) && !breathless(mtmp->data)) {
            if (cansee(mtmp->mx, mtmp->my))
                pline("%s coughs!", Monnam(mtmp));
            setmangry(mtmp);
            if (haseyes(mtmp->data) && mtmp->mcansee) {
                mtmp->mblinded = 1;
                mtmp->mcansee = 0;
            }
            if (resists_poison(mtmp))
                return FALSE;
            mtmp->mhp -= rnd(dam) + 5;
            if (mtmp->mhp <= 0) {
                if (heros_fault(reg))
                    killed(mtmp);
                else
                    monkilled(mtmp, "gas cloud", AD_DRST);
                if (DEADMONSTER(mtmp)) {   /* not lifesaved */
                    return TRUE;
                }
            }
        }
    }
    return FALSE;       /* Monster is still alive */
}

struct region *
create_gas_cloud(struct level *lev, xchar x, xchar y, int radius, int damage)
{
    struct region *cloud;
    int i, nrect;
    struct nhrect tmprect;

    cloud = create_region(NULL, 0);
    nrect = radius;
    tmprect.lx = x;
    tmprect.hx = x;
    tmprect.ly = y - (radius - 1);
    tmprect.hy = y + (radius - 1);
    for (i = 0; i < nrect; i++) {
        add_rect_to_reg(cloud, &tmprect);
        tmprect.lx--;
        tmprect.hx++;
        tmprect.ly++;
        tmprect.hy--;
    }
    cloud->ttl = rn1(3, 4);
    if (!in_mklev && !flags.mon_moving)
        set_heros_fault(cloud); /* assume player has created it */
    cloud->inside_f = INSIDE_GAS_CLOUD;
    cloud->expire_f = EXPIRE_GAS_CLOUD;
    cloud->arg = damage;
    cloud->visible = TRUE;
    cloud->effect_id = dbuf_effect(E_MISC, E_gascloud);
    add_region(lev, cloud);
    return cloud;
}

/*region.c*/

