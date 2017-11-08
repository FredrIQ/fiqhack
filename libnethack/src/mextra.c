/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-08 */
/* Copyright (c) Fredrik Ljungdahl, 2015. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/*
 * Extra monst/obj structs, seperated to preserve space for cases where
 * the additional space would be significant to be overkill to have on
 * every monst/obj struct.
 *
 * Common functions:
 * ex_new(ent)                Creates a new extra struct
 * ex_copy(to, from)          Copy ent->extra "to" based on "from"
 * ex_free(ent)               Frees up the extra struct for the entity
 * ex_possiblyfree(ent)       Frees up the extra struct if it's empty
 * christen_entity(ent, name) Gives the entity the given name
 *
 * Functions for each struct within entity->extra:
 * ex_estruct(ent)            Returns ent->extra->estruct or NULL, safely
 * ex_estruct_new(ent)        Creates ent->extra->estruct
 * ex_estruct_free(ent)       Frees ent->extra->struct, runs possiblyfree
 *
 * Making a new extra->estruct:
 * - Insert a GEN_EXTYP() as applicable
 * - Tweak _free, _possiblyfree, _copy as applicable
 * - Tweak save/restore_extra()
 *
 * TODO: Figure out a way to minimize workload for adding a new estruct
 */


/* Base functions -- sacrifices readability slightly to avoid duplication
   TODO: can we make x_possiblyfree/x_free common sanely without making
   the result unreadable? Also, this is probably makedefs.c material */
#define GEN_EXBASE(entity, emo)                                         \
    void                                                                \
    emo##x_new(struct entity *ent) {                                    \
        if (ent->emo##extra)                                            \
            return;                                                     \
        ent->emo##extra = malloc(sizeof (struct emo##extra));           \
        memset(ent->emo##extra, 0, sizeof (struct emo##extra));         \
    }                                                                   \
                                                                        \
    char *                                                              \
    emo##x_name(const struct entity *ent) {                             \
        return (ent->emo##extra ? ent->emo##extra->name : NULL);        \
    }                                                                   \
                                                                        \
    void                                                                \
    christen_##entity(struct entity *ent, const char *name) {           \
        if (emo##x_name(ent)) {                                         \
            free(ent->emo##extra->name);                                \
            ent->emo##extra->name = NULL;                               \
        }                                                               \
        if (!name || !*name) {                                          \
            emo##x_possiblyfree(ent);                                   \
            return;                                                     \
        }                                                               \
        emo##x_new(ent);                                                \
        int lth;                                                        \
        char buf[PL_PSIZ] = {0};                                        \
        lth = (strlen(name) + 1);                                       \
        strncpy(buf, name, PL_PSIZ - 1);                                \
        if (lth > PL_PSIZ)                                              \
            lth = PL_PSIZ;                                              \
        ent->emo##extra->name = malloc((unsigned) lth);                 \
        strncpy(ent->emo##extra->name, buf, lth);                       \
    }

#define GEN_EXTYP(extyp, entity, emo, dealloc_fn)                       \
    struct extyp *                                                      \
    emo##x_##extyp(const struct entity *ent) {                          \
        return (ent->emo##extra ? ent->emo##extra->extyp : NULL);       \
    }                                                                   \
                                                                        \
    void                                                                \
    emo##x_##extyp##_new(struct entity *ent) {                          \
        if (ent->emo##extra && ent->emo##extra->extyp)                  \
            return;                                                     \
        emo##x_new(ent);                                                \
        ent->emo##extra->extyp = malloc(sizeof (struct extyp));         \
        memset(ent->emo##extra->extyp, 0, sizeof (struct extyp));       \
    }                                                                   \
                                                                        \
    void                                                                \
    emo##x_##extyp##_free(struct entity *ent) {                         \
        if (!ent->emo##extra || !ent->emo##extra->extyp)                \
            return;                                                     \
        dealloc_fn(ent->emo##extra->extyp);                             \
        ent->emo##extra->extyp = NULL;                                  \
        emo##x_possiblyfree(ent);                                       \
    }

GEN_EXBASE(monst, m)
GEN_EXBASE(obj, o)

GEN_EXTYP(eyou, monst, m, free)
GEN_EXTYP(edog, monst, m, free)
GEN_EXTYP(epri, monst, m, free)
GEN_EXTYP(eshk, monst, m, free)
GEN_EXTYP(egd, monst, m, free)
GEN_EXTYP(monst, obj, o, dealloc_monst) /* dealloc_monst frees mextra */

#undef GEN_EXBASE
#undef GEN_EXTYP

void
mx_copy(struct monst *mon, const struct monst *mtmp)
{
    /* Free old mextra if it exists, unless it's the same pointer
       as the copy target (to prevent dangling pointers).
       If it's identical, just set the mon->mextra pointer to NULL */
    if (mon->mextra != mtmp->mextra)
        mx_free(mon);
    else
        mon->mextra = NULL;

    christen_monst(mon, mx_name(mtmp));
    if (mx_eyou(mtmp)) {
        mx_eyou_new(mon);
        memcpy(mx_eyou(mon), mx_eyou(mtmp), sizeof (struct eyou));
    }
    if (mx_edog(mtmp)) {
        mx_edog_new(mon);
        memcpy(mx_edog(mon), mx_edog(mtmp), sizeof (struct edog));
    }
    if (mx_epri(mtmp)) {
        mx_epri_new(mon);
        memcpy(mx_epri(mon), mx_epri(mtmp), sizeof (struct epri));
    }
    if (mx_eshk(mtmp)) {
        mx_eshk_new(mon);
        memcpy(mx_eshk(mon), mx_eshk(mtmp), sizeof (struct eshk));
    }
    if (mx_egd(mtmp)) {
        mx_egd_new(mon);
        memcpy(mx_egd(mon), mx_egd(mtmp), sizeof (struct egd));
    }
}

void
mx_free(struct monst *mon)
{
    if (!mon->mextra)
        return;

    christen_monst(mon, NULL);
    mx_eyou_free(mon);
    mx_edog_free(mon);
    mx_epri_free(mon);
    mx_eshk_free(mon);
    mx_egd_free(mon);
    if (mon->mextra) /* the above frees run possiblyfree */
        panic("trying to free mextra failed?");
}

void
mx_possiblyfree(struct monst *mon)
{
    struct mextra *mx = mon->mextra;
    if (!mx || mx->eyou || mx->edog || mx->epri || mx->eshk || mx->egd ||
        mx->name)
        return;

    free(mon->mextra);
    mon->mextra = NULL;
}

void
ox_copy(struct obj *obj, const struct obj *otmp)
{
    if (obj->oextra != otmp->oextra)
        ox_free(obj);
    else
        obj->oextra = NULL;

    christen_obj(obj, ox_name(otmp));
    if (ox_monst(otmp)) {
        ox_monst_new(obj);
        memcpy(ox_monst(obj), ox_monst(otmp), sizeof (struct monst));
        mx_copy(ox_monst(obj), ox_monst(otmp));
    }
}

void
ox_free(struct obj *obj)
{
    if (!obj->oextra)
        return;

    christen_obj(obj, NULL);
    ox_monst_free(obj);
    if (obj->oextra) /* the above frees run possiblyfree */
        panic("trying to free oextra failed?");
}

void
ox_possiblyfree(struct obj *obj)
{
    struct oextra *ox = obj->oextra;
    if (!ox || ox->monst || ox->name)
        return;

    free(obj->oextra);
    obj->oextra = NULL;
}

int
mxcontent(const struct monst *mon)
{
    struct mextra *mx = mon->mextra;
    if (!mx)
        return 0;
    return
        (mx->name ? MX_NAME : 0) |
        (mx->edog ? MX_EDOG : 0) |
        (mx->eyou ? MX_EYOU : 0) |
        (mx->epri ? MX_EPRI : 0) |
        (mx->eshk ? MX_ESHK : 0) |
        (mx->egd ? MX_EGD : 0);
}

int
oxcontent(const struct obj *obj)
{
    struct oextra *ox = obj->oextra;
    if (!ox)
        return 0;
    return
        (ox->name ? OX_NAME : 0) |
        (ox->monst ? OX_MONST : 0);
}

void
restore_shkbill(struct memfile *mf, struct bill_x *b)
{
    b->bo_id = mread32(mf);
    b->price = mread32(mf);
    b->bquan = mread32(mf);
    b->useup = mread8(mf);
}


void
restore_fcorr(struct memfile *mf, struct fakecorridor *f)
{
    f->fx = mread8(mf);
    f->fy = mread8(mf);
    f->ftyp = mread8(mf);
}


void
restore_mextra(struct memfile *mf, struct monst *mon)
{
    int i;
    int extyp = mread8(mf);

    if (extyp)
        mx_new(mon);
    else
        panic("restore_mextra: nothing to restore for %s?",
              k_monnam(mon));

    struct mextra *mx = mon->mextra;

    if (extyp & MX_NAME) {
        int namelth = mread8(mf);
        char namebuf[namelth];

        if (namelth) {
            mread(mf, namebuf, namelth);
            christen_monst(mon, namebuf);
        }
    }
    if (extyp & MX_EYOU) {
        mx_eyou_new(mon);
        mx->eyou->last_pray_action = mread32(mf);
        mx->eyou->prayed_result = mread8(mf);

        for (i = 0; i < 1000; i++)
            mread8(mf);
    }
    if (extyp & MX_EDOG) {
        mx_edog_new(mon);
        mx->edog->droptime = mread32(mf);
        mx->edog->dropdist = mread32(mf);
        mx->edog->apport = mread32(mf);
        mx->edog->whistletime = mread32(mf);
        mx->edog->hungrytime = mread32(mf);
        mx->edog->abuse = mread32(mf);
        mx->edog->revivals = mread32(mf);
        mx->edog->mhpmax_penalty = mread32(mf);
        mx->edog->killed_by_u = mread8(mf);
    }
    if (extyp & MX_EPRI) {
        mx_epri_new(mon);
        mx->epri->shroom = mread8(mf);
        mx->epri->shrpos.x = mread8(mf);
        mx->epri->shrpos.y = mread8(mf);
        mx->epri->shrlevel.dnum = mread8(mf);
        mx->epri->shrlevel.dlevel = mread8(mf);
    }
    if (extyp & MX_ESHK) {
        mx_eshk_new(mon);
        struct eshk *shk = mx_eshk(mon);
        shk->bill_inactive = mread32(mf);
        shk->shk.x = mread8(mf);
        shk->shk.y = mread8(mf);
        shk->shd.x = mread8(mf);
        shk->shd.y = mread8(mf);
        shk->robbed = mread32(mf);
        shk->credit = mread32(mf);
        shk->debit = mread32(mf);
        shk->loan = mread32(mf);
        shk->shoptype = mread16(mf);
        shk->billct = mread16(mf);
        shk->visitct = mread16(mf);
        shk->shoplevel.dnum = mread8(mf);
        shk->shoplevel.dlevel = mread8(mf);
        shk->shoproom = mread8(mf);
        shk->following = mread8(mf);
        shk->surcharge = mread8(mf);
        mread(mf, shk->customer, sizeof (shk->customer));
        for (i = 0; i < BILLSZ; i++)
            restore_shkbill(mf, &shk->bill[i]);
    }
    if (extyp & MX_EGD) {
        mx_egd_new(mon);
        mx->egd->fcbeg = mread32(mf);
        mx->egd->fcend = mread32(mf);
        mx->egd->vroom = mread32(mf);
        mx->egd->gdx = mread8(mf);
        mx->egd->gdy = mread8(mf);
        mx->egd->ogx = mread8(mf);
        mx->egd->ogy = mread8(mf);
        mx->egd->gdlevel.dnum = mread8(mf);
        mx->egd->gdlevel.dlevel = mread8(mf);
        mx->egd->warncnt = mread8(mf);
        mx->egd->gddone = mread8(mf);
        for (i = 0; i < FCSIZ; i++)
            restore_fcorr(mf, &(mx->egd)->fakecorr[i]);
    }
}

static void
save_shkbill(struct memfile *mf, const struct bill_x *b)
{
    /* no mtag needed; saved as part of a particular shk's data */
    mwrite32(mf, b->bo_id);
    mwrite32(mf, b->price);
    mwrite32(mf, b->bquan);
    mwrite8(mf, b->useup);
}


static void
save_fcorr(struct memfile *mf, const struct fakecorridor *f)
{
    /* no mtag needed; saved as part of a particular guard's data */
    mwrite8(mf, f->fx);
    mwrite8(mf, f->fy);
    mwrite8(mf, f->ftyp);
}


void
save_mextra(struct memfile *mf, const struct monst *mon)
{
    int i;
    struct mextra *mx = mon->mextra;
    if (!mx) {
        panic("save_mextra: no mon->mextra?");
        return;
    }

    int extyp = mxcontent(mon);
    if (!extyp) {
        panic("save_mextra: mextra struct is empty?");
        return;
    }

    mtag(mf, mon->m_id, MTAG_MX);
    mwrite8(mf, extyp);

    if (extyp & MX_NAME) {
        mtag(mf, mon->m_id, MTAG_MXNAME);
        int namelth = strlen(mx->name) + 1;
        if (namelth > PL_PSIZ) {
            panic("save_mextra: monster name too long");
            return;
        }

        mwrite8(mf, namelth);
        mwrite(mf, mx->name, namelth);
    }

    if (extyp & MX_EYOU) {
        mtag(mf, mon->m_id, MTAG_MXEYOU);
        mwrite32(mf, mx->eyou->last_pray_action);
        mwrite8(mf, mx->eyou->prayed_result);

        for (i = 0; i < 1000; i++)
            mwrite8(mf, 0);
    }
    if (extyp & MX_EDOG) {
        mtag(mf, mon->m_id, MTAG_MXEDOG);
        mwrite32(mf, mx->edog->droptime);
        mwrite32(mf, mx->edog->dropdist);
        mwrite32(mf, mx->edog->apport);
        mwrite32(mf, mx->edog->whistletime);
        mwrite32(mf, mx->edog->hungrytime);
        mwrite32(mf, mx->edog->abuse);
        mwrite32(mf, mx->edog->revivals);
        mwrite32(mf, mx->edog->mhpmax_penalty);
        mwrite8(mf, mx->edog->killed_by_u);
    }
    if (extyp & MX_EPRI) {
        mtag(mf, mon->m_id, MTAG_MXEPRI);
        mwrite8(mf, mx->epri->shroom);
        mwrite8(mf, mx->epri->shrpos.x);
        mwrite8(mf, mx->epri->shrpos.y);
        mwrite8(mf, mx->epri->shrlevel.dnum);
        mwrite8(mf, mx->epri->shrlevel.dlevel);
    }
    if (extyp & MX_ESHK) {
        mtag(mf, mon->m_id, MTAG_MXESHK);
        mwrite32(mf, mx->eshk->bill_inactive);
        mwrite8(mf, mx->eshk->shk.x);
        mwrite8(mf, mx->eshk->shk.y);
        mwrite8(mf, mx->eshk->shd.x);
        mwrite8(mf, mx->eshk->shd.y);
        mwrite32(mf, mx->eshk->robbed);
        mwrite32(mf, mx->eshk->credit);
        mwrite32(mf, mx->eshk->debit);
        mwrite32(mf, mx->eshk->loan);
        mwrite16(mf, mx->eshk->shoptype);
        mwrite16(mf, mx->eshk->billct);
        mwrite16(mf, mx->eshk->visitct);
        mwrite8(mf, mx->eshk->shoplevel.dnum);
        mwrite8(mf, mx->eshk->shoplevel.dlevel);
        mwrite8(mf, mx->eshk->shoproom);
        mwrite8(mf, mx->eshk->following);
        mwrite8(mf, mx->eshk->surcharge);
        mwrite(mf, mx->eshk->customer, sizeof (mx->eshk->customer));
        for (i = 0; i < BILLSZ; i++)
            save_shkbill(mf, &(mx->eshk)->bill[i]);
    }
    if (extyp & MX_EGD) {
        mwrite32(mf, mx->egd->fcbeg);
        mwrite32(mf, mx->egd->fcend);
        mwrite32(mf, mx->egd->vroom);
        mwrite8(mf, mx->egd->gdx);
        mwrite8(mf, mx->egd->gdy);
        mwrite8(mf, mx->egd->ogx);
        mwrite8(mf, mx->egd->ogy);
        mwrite8(mf, mx->egd->gdlevel.dnum);
        mwrite8(mf, mx->egd->gdlevel.dlevel);
        mwrite8(mf, mx->egd->warncnt);
        mwrite8(mf, mx->egd->gddone);
        for (i = 0; i < FCSIZ; i++)
            save_fcorr(mf, &(mx->egd)->fakecorr[i]);
    }
}


void
restore_oextra(struct memfile *mf, struct obj *obj)
{
    int extyp = mread8(mf);

    if (extyp)
        ox_new(obj);
    else
        panic("restore_oextra: nothing to restore?");

    struct oextra *ox = obj->oextra;

    if (extyp & OX_NAME) {
        int namelth = mread8(mf);
        char namebuf[namelth];

        if (namelth) {
            mread(mf, namebuf, namelth);
            christen_obj(obj, namebuf);
        }
    }
    if (extyp & OX_MONST) {
        ox_monst_new(obj);
        restore_mon(mf, ox->monst, NULL);
    }
}


void
save_oextra(struct memfile *mf, const struct obj *obj)
{
    struct oextra *ox = obj->oextra;
    if (!ox) {
        panic("save_oextra: no obj->oextra?");
        return;
    }

    int extyp = oxcontent(obj);
    if (!extyp) {
        panic("save_oextra: oextra struct is empty?");
        return;
    }

    mtag(mf, obj->o_id, MTAG_OX);
    mwrite8(mf, extyp);

    if (extyp & OX_NAME) {
        mtag(mf, obj->o_id, MTAG_OXNAME);
        int namelth = strlen(ox->name) + 1;
        if (namelth > PL_PSIZ) {
            panic("save_mextra: monster name too long");
            return;
        }

        mwrite8(mf, namelth);
        mwrite(mf, ox->name, namelth);
    }

    if (extyp & OX_MONST) {
        mtag(mf, obj->o_id, MTAG_OXMONST);
        save_mon(mf, obj->oextra->monst, NULL);
    }
}
/*mextra.c*/
