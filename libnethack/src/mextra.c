/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-17 */
/* Copyright (c) Fredrik Ljungdahl, 2015. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Handling of extended monster memory.
   mx_free(): frees up mx, mx_possiblyfree() frees up empty mextras, mx_new() creates a new one.
   For the mextra structs themselves, mx_STRUCT() returns the struct or NULL if no mextra or no mextra->STRUCT,
   mx_STRUCT_new() creates a new one, mx_STRUCT_free frees it (and potentially frees up mextra).
   To simplify the code, the error handling is performed inside the functions, no need to do it on callers.
   However, they only handle the structs -- e.g. they don't reset tameness on mx_edog_free for example.
   Names are special -- just call christen_monst(mon, name) (name==NULL removes name) and it will handle
   things. Should you wish to add a new mextra struct, add it in /include/mextra.h (and to struct mextra),
   add a GEN_MEXTRA() line with your new struct, and add your struct to save/restore_mextra, mx_free and
   mx_possiblyfree. WARNING: ensure that you get the mextra save/restore order right, otherwise it will
   appear to work with only a single mextra, but fail with stacked ones... */

#define GEN_MEXTRA(nmx)                                                 \
    extern struct nmx *                                                 \
    mx_##nmx(const struct monst *mon) {                                 \
        return (mon->mextra ? mon->mextra->nmx : NULL);                 \
    }                                                                   \
    extern void                                                         \
    mx_##nmx##_new(struct monst *mon) {                                 \
        if (mon->mextra && mon->mextra->nmx)                            \
            return;                                                     \
        mx_new(mon);                                                    \
        mon->mextra->nmx = malloc(sizeof (struct nmx));                 \
        memset(mon->mextra->nmx, 0,                                     \
               sizeof (struct nmx));                                    \
    }                                                                   \
    extern void                                                         \
    mx_##nmx##_free(struct monst *mon) {                                \
        if (!mon->mextra || !mon->mextra->nmx)                          \
            return;                                                     \
        free(mon->mextra->nmx);                                         \
        mon->mextra->nmx = NULL;                                        \
        mx_possiblyfree(mon);                                           \
    }

GEN_MEXTRA(edog)
GEN_MEXTRA(epri)
GEN_MEXTRA(eshk)
GEN_MEXTRA(egd)
#undef GEN_MEXTRA

void
mx_new(struct monst *mon)
{
    if (mon->mextra)
        return;

    mon->mextra = malloc(sizeof (struct mextra));
    memset(mon->mextra, 0, sizeof (struct mextra));
}
void
mx_free(struct monst *mon)
{
    if (!mon->mextra)
        return;

    christen_monst(mon, NULL);
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
    if (!mx || mx->edog || mx->epri || mx->eshk || mx->egd ||
        mx->name)
        return;

    free(mon->mextra);
    mon->mextra = NULL;
}

char *
mx_name(const struct monst *mon) {
    return (mon->mextra ? mon->mextra->name : NULL);
}

void
christen_monst(struct monst *mon, const char *name)
{
    /* free old name if it exists */
    if (mx_name(mon)) {
        free(mon->mextra->name);
        mon->mextra->name = NULL;
    }

    /* if we aren't creating a new name, bail out */
    if (!name || !*name) {
        mx_possiblyfree(mon);
        return;
    }

    /* create new mextra if needed */
    mx_new(mon);

    int lth;
    char buf[PL_PSIZ] = {0};

    /* dogname & catname are PL_PSIZ arrays; object names have same limit */
    lth = (strlen(name) + 1);
    strncpy(buf, name, PL_PSIZ - 1);
    if (lth > PL_PSIZ)
        lth = PL_PSIZ;

    mon->mextra->name = malloc((unsigned) lth);
    strncpy(mon->mextra->name, buf, lth);
}

/* not static -- restore_mon's legacy code needs it (SAVEBREAK: make static) */
void
restore_shkbill(struct memfile *mf, struct bill_x *b)
{
    b->bo_id = mread32(mf);
    b->price = mread32(mf);
    b->bquan = mread32(mf);
    b->useup = mread8(mf);
}

/* not static (see above) */
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
    int mxcontent = mread8(mf);

    if (mxcontent)
        mx_new(mon);
    else
        panic("restore_mextra: nothing to restore?");

    struct mextra *mx = mon->mextra;

    if (mxcontent & MX_NAME) {
        int namelth = mread8(mf);
        char namebuf[namelth];

        if (namelth) {
            mread(mf, namebuf, namelth);
            christen_monst(mon, namebuf);
        }
    }
    if (mxcontent & MX_EDOG) {
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
    if (mxcontent & MX_EPRI) {
        mx_epri_new(mon);
        mx->epri->shroom = mread8(mf);
        mx->epri->shrpos.x = mread8(mf);
        mx->epri->shrpos.y = mread8(mf);
        mx->epri->shrlevel.dnum = mread8(mf);
        mx->epri->shrlevel.dlevel = mread8(mf);
    }
    if (mxcontent & MX_ESHK) {
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
    if (mxcontent & MX_EGD) {
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

    /* 3 free bits until savebreak */
    int mxcontent;
    mxcontent =
        (!!mx->name << 4) |
        (!!mx->egd << 3) | (!!mx->eshk << 2) |
        (!!mx->epri << 1) | (!!mx->edog << 0);

    if (!mxcontent) {
        panic("save_mextra: mextra struct is empty?");
        return;
    }

    mtag(mf, mon->m_id, MTAG_MX);
    mwrite8(mf, mxcontent);

    if (mxcontent & MX_NAME) {
        mtag(mf, mon->m_id, MTAG_MXNAME);
        int namelth = strlen(mx->name) + 1;
        if (namelth > PL_PSIZ) {
            panic("save_mextra: monster name too long");
            return;
        }

        mwrite8(mf, namelth);
        mwrite(mf, mx->name, namelth);
    }

    if (mxcontent & MX_EDOG) {
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
    if (mxcontent & MX_EPRI) {
        mtag(mf, mon->m_id, MTAG_MXEPRI);
        mwrite8(mf, mx->epri->shroom);
        mwrite8(mf, mx->epri->shrpos.x);
        mwrite8(mf, mx->epri->shrpos.y);
        mwrite8(mf, mx->epri->shrlevel.dnum);
        mwrite8(mf, mx->epri->shrlevel.dlevel);
    }
    if (mxcontent & MX_ESHK) {
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
    if (mxcontent & MX_EGD) {
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

/*mextra.c*/
