/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include "trietable.h"
#include <stdint.h>

static void restore_autopickup_rules(struct memfile *mf,
                                     struct nh_autopickup_rules *r);
static void restore_utracked(struct memfile *mf, struct you *you);
static void find_lev_obj(struct level *lev);
static void restlevchn(struct memfile *mf);
static void restdamage(struct memfile *mf, struct level *lev, boolean ghostly);
static void restobjchn(struct memfile *mf, struct level *lev,
                       boolean ghostly, boolean frozen, struct obj **chain,
                       struct trietable **table);
static struct monst *restmonchn(struct memfile *mf, struct level *lev,
                                boolean ghostly);
static struct fruit *loadfruitchn(struct memfile *mf);
static void freefruitchn(struct fruit *);
static void ghostfruit(struct obj *);
static void restgamestate(struct memfile *mf);
static void reset_oattached_mids(boolean ghostly, struct level *lev);

/*
 * Save a mapping of IDs from ghost levels to the current level.  This
 * map is used by the timer routines when restoring ghost levels.
 */
#define N_PER_BUCKET 64
struct bucket {
    struct bucket *next;
    struct {
        unsigned gid;   /* ghost ID */
        unsigned nid;   /* new ID */
    } map[N_PER_BUCKET];
};

static void clear_id_mapping(void);
static void add_id_mapping(unsigned, unsigned);

static int n_ids_mapped = 0;
static struct bucket *id_map = 0;


#include "quest.h"

static struct fruit *oldfruit;

#define Is_IceBox(o) ((o)->otyp == ICE_BOX ? TRUE : FALSE)

/* Recalculate lev->objects[x][y], since this info was not saved. */
static void
find_lev_obj(struct level *lev)
{
    struct obj *fobjtmp = NULL;
    struct obj *otmp;
    int x, y;

    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            lev->objects[x][y] = NULL;

    /*
     * Reverse the entire lev->objlist chain, which is necessary so that we can
     * place the objects in the proper order.  Make all obj in chain
     * OBJ_FREE so place_object will work correctly.
     *
     * TODO: This is currently a quadratic algorithm due to the extract_nobj
     * on the floating objects list in place_object.
     */
    while ((otmp = lev->objlist) != 0) {
        lev->objlist = otmp->nobj;
        /* We use nexthere for the temporary lev->objlist chain, because nobj
           is required to keep track of floating objects. */
        otmp->nexthere = fobjtmp;
        otmp->where = OBJ_FREE;
        otmp->nobj = turnstate.floating_objects;
        turnstate.floating_objects = otmp;
        fobjtmp = otmp;
    }
    /* lev->objlist should now be empty */

    /* Set lev->objects (as well as reversing the chain back again) */
    while ((otmp = fobjtmp) != 0) {
        fobjtmp = otmp->nexthere;
        place_object(otmp, lev, otmp->ox, otmp->oy);
    }
}

/* Things that were marked "in_use" when the game was saved (ex. via the
   infamous "HUP" cheat) get used up here.

   TODO: If this isn't a no-op, it's going to cause a desync and an unloadable
   save file. */
void
inven_inuse(boolean quietly)
{
    struct obj *otmp, *otmp2;

    for (otmp = invent; otmp; otmp = otmp2) {
        otmp2 = otmp->nobj;
        if (otmp->in_use) {
            if (!quietly)
                pline(msgc_noidea, "Finishing off %s...", xname(otmp));
            useup(otmp);
        }
    }
}

static void
restlevchn(struct memfile *mf)
{
    int cnt;
    s_level *tmplev, *x;

    gamestate.sp_levchn = NULL;
    cnt = mread32(mf);
    for (; cnt > 0; cnt--) {
        tmplev = malloc(sizeof (s_level));
        tmplev->flags = restore_d_flags(mf);
        restore_dlevel(mf, &tmplev->dlevel);
        mread(mf, tmplev->proto, sizeof (tmplev->proto));
        tmplev->boneid = mread8(mf);
        tmplev->rndlevs = mread8(mf);

        if (!gamestate.sp_levchn)
            gamestate.sp_levchn = tmplev;
        else {
            for (x = gamestate.sp_levchn; x->next; x = x->next)
                ;
            x->next = tmplev;
        }
        tmplev->next = NULL;
    }
}


static void
restdamage(struct memfile *mf, struct level *lev, boolean ghostly)
{
    int counter;
    struct damage *tmp_dam;
    struct damage **pnext;

    counter = mread32(mf);
    if (!counter)
        return;

    tmp_dam = malloc(sizeof (struct damage));
    pnext = &(lev->damagelist);

    while (--counter >= 0) {
        char damaged_shops[5], *shp = NULL;

        tmp_dam->when = mread32(mf);
        tmp_dam->cost = mread32(mf);
        tmp_dam->place.x = mread8(mf);
        tmp_dam->place.y = mread8(mf);
        tmp_dam->typ = mread8(mf);

        if (ghostly)
            tmp_dam->when += (moves - lev->lastmoves);
        strcpy(damaged_shops,
               in_rooms(lev, tmp_dam->place.x, tmp_dam->place.y, SHOPBASE));

/* TODO: Maybe we should consider doing this on level change. It definitely
   mustn't happen on save game restore, though, otherwise saving and restoring a
   game allows you to repair damage while the shopkeeper is asleep or
   paralyzed, something that wouldn't happen without this code running. */
#if 0
        for (shp = damaged_shops; *shp; shp++) {
            struct monst *shkp = shop_keeper(lev, *shp);

            if (shkp && inhishop(shkp) &&
                repair_damage(lev, shkp, tmp_dam, TRUE))
                break;
        }
#endif

        if (!shp || !*shp) {
            tmp_dam->next = 0;
            *pnext = tmp_dam;
            pnext = &(tmp_dam->next);
            tmp_dam = malloc(sizeof (*tmp_dam));
        }
    }
    free(tmp_dam);
}


static void
restobjchn(struct memfile *mf, struct level *lev, boolean ghostly,
           boolean frozen, struct obj **chainloc, struct trietable **table)
{
    struct obj *otmp;
    unsigned int count;

    mfmagic_check(mf, OBJCHAIN_MAGIC);
    count = mread32(mf);

    *chainloc = NULL;

    while (count--) {
        otmp = restore_obj(mf);

        /* Move the object from the floating objects chain onto the
           *chainloc chain. */
        extract_nobj(otmp, &turnstate.floating_objects, NULL, 0);
        *chainloc = otmp;
        chainloc = &(otmp->nobj);
        otmp->nobj = NULL;

        otmp->olev = lev;

        if (ghostly) {
            unsigned nid = next_ident();

            add_id_mapping(otmp->o_id, nid);
            otmp->o_id = nid;
        }
        if (ghostly && otmp->otyp == SLIME_MOLD)
            ghostfruit(otmp);
        /* Ghost levels get object age shifted from old player's clock to new
           player's clock.  Assumption: new player arrived immediately after old
           player died. */
        if (ghostly && !frozen && !age_is_relative(otmp))
            otmp->age = moves - lev->lastmoves + otmp->age;

        /* we might need to produce an index, for speed in relinking IDs */
        if (table)
            trietable_add(table, otmp->o_id, otmp);

        /* get contents of a container or statue */
        if (Has_contents(otmp)) {
            struct obj *otmp3;

            restobjchn(mf, lev, ghostly, Is_IceBox(otmp), &otmp->cobj, table);
            /* restore container back pointers */
            for (otmp3 = otmp->cobj; otmp3; otmp3 = otmp3->nobj)
                otmp3->ocontainer = otmp;
        }
        if (otmp->bypass)
            otmp->bypass = 0;
    }
}


static struct monst *
restmonchn(struct memfile *mf, struct level *lev, boolean ghostly)
{
    struct monst *mtmp, *mtmp2 = NULL;
    struct monst *first = NULL;
    struct obj *obj;
    unsigned int count, mndx;

    /* get the original base address */
    mfmagic_check(mf, MONCHAIN_MAGIC);
    count = mread32(mf);

    while (count--) {
        mtmp = restore_mon(mf, lev);
        if (!first)
            first = mtmp;
        else
            mtmp2->nmon = mtmp;

        if (ghostly) {
            unsigned nid = next_ident();

            add_id_mapping(mtmp->m_id, nid);
            mtmp->m_id = nid;

            mndx = monsndx(mtmp->data);
            if (propagate(mndx, TRUE, ghostly) == 0) {
                /* cookie to trigger purge in getbones() */
                mtmp->mhpmax = DEFUNCT_MONSTER;
            }
        }

        if (mtmp->minvent) {
            restobjchn(mf, lev, ghostly, FALSE, &(mtmp->minvent), NULL);
            /* restore monster back pointer */
            for (obj = mtmp->minvent; obj; obj = obj->nobj)
                obj->ocarry = mtmp;
        }

        if (mtmp->mw) {
            for (obj = mtmp->minvent; obj; obj = obj->nobj)
                if (obj->owornmask & W_MASK(os_wep))
                    break;
            if (obj)
                mtmp->mw = obj;
            else {
                MON_NOWEP(mtmp);
                impossible("bad monster weapon restore");
            }
        }

        if (ghostly) {
            /* these cases are from makemon.c */
            mtmp->mpeaceful = peace_minded(mtmp->data);
            if (is_unicorn(mtmp->data) &&
                sgn(u.ualign.type) == sgn(mtmp->data->maligntyp))
                mtmp->mpeaceful = TRUE;
            else if (mtmp->data->mlet == S_ORC && Race_if(PM_ELF))
                mtmp->mpeaceful = FALSE;
            set_malign(mtmp);
        }

        if (mtmp->isshk)
            restshk(mtmp, ghostly);
        if (mtmp->ispriest)
            restpriest(mtmp, ghostly);

        mtmp2 = mtmp;
    }
    if (first && mtmp2->nmon) {
        impossible("Restmonchn: error reading monchn.");
        mtmp2->nmon = 0;
    }

    return first;
}


static struct fruit *
loadfruitchn(struct memfile *mf)
{
    struct fruit *flist = NULL, *fnext, *fcurr, *fprev;
    unsigned int count;

    mfmagic_check(mf, FRUITCHAIN_MAGIC);
    count = mread32(mf);

    if (!count)
        return flist;

    while (count--) {
        fnext = newfruit();
        mread(mf, fnext->fname, sizeof (fnext->fname));
        fnext->fid = mread32(mf);
        fnext->nextf = flist;
        flist = fnext;
    }

    /* fruit list loaded above is reversed, so put it back in the right order */
    fcurr = flist;
    fprev = NULL;
    while (fcurr) {
        fnext = fcurr->nextf;
        fcurr->nextf = fprev;
        fprev = fcurr;
        fcurr = fnext;
    }
    flist = fprev;

    return flist;
}


static void
freefruitchn(struct fruit *flist)
{
    struct fruit *fnext;

    while (flist) {
        fnext = flist->nextf;
        dealloc_fruit(flist);
        flist = fnext;
    }
}


static void
ghostfruit(struct obj *otmp)
{
    struct fruit *oldf;

    for (oldf = oldfruit; oldf; oldf = oldf->nextf)
        if (oldf->fid == otmp->spe)
            break;

    if (!oldf)
        impossible("no old fruit?");
    else
        otmp->spe = fruitadd(oldf->fname);
}


static void
restore_mvitals(struct memfile *mf)
{
    int i;

    for (i = 0; i < NUMMONS; i++) {
        mvitals[i].born = mread8(mf);
        mvitals[i].died = mread8(mf);
        mvitals[i].mvflags = mread8(mf);
    }
}


static void
restore_quest_status(struct memfile *mf, struct q_score *q)
{
    unsigned int qflags;

    qflags = mread32(mf);
    q->first_start = (qflags >> 31) & 1;
    q->met_leader = (qflags >> 30) & 1;
    q->not_ready = (qflags >> 27) & 7;
    q->pissed_off = (qflags >> 26) & 1;
    q->got_quest = (qflags >> 25) & 1;
    q->first_locate = (qflags >> 24) & 1;
    q->met_intermed = (qflags >> 23) & 1;
    q->got_final = (qflags >> 22) & 1;
    q->made_goal = (qflags >> 19) & 7;
    q->met_nemesis = (qflags >> 18) & 1;
    q->killed_nemesis = (qflags >> 17) & 1;
    q->in_battle = (qflags >> 16) & 1;
    q->cheater = (qflags >> 15) & 1;
    q->touched_artifact = (qflags >> 14) & 1;
    q->offered_artifact = (qflags >> 13) & 1;
    q->got_thanks = (qflags >> 12) & 1;
    q->leader_is_dead = (qflags >> 11) & 1;

    q->leader_m_id = mread32(mf);
}


static void
restore_spellbook(struct memfile *mf)
{
    int i;

    for (i = 0; i < MAXSPELL + 1; i++) {
        spl_book[i].sp_know = save_decode_32(mread32(mf), -moves, -moves);
        spl_book[i].sp_id = mread16(mf);
        spl_book[i].sp_lev = mread8(mf);
    }
}


static void
restgamestate(struct memfile *mf)
{
    struct monst *mtmp;
    struct level *lev;

    mfmagic_check(mf, STATE_MAGIC);

    lev = levels[ledger_no(&u.uz)];

    /* this stuff comes after potential aborted restore attempts */
    restore_timers(mf, lev, RANGE_GLOBAL, FALSE, 0L);
    restore_light_sources(mf, lev);
    restobjchn(mf, lev, FALSE, FALSE, &invent, NULL);
    migrating_mons = restmonchn(mf, NULL, FALSE);
    restore_mvitals(mf);

    restore_spellbook(mf);
    restore_artifacts(mf);
    restore_oracles(mf);

    mread(mf, gamestate.fruits.curname, sizeof gamestate.fruits.curname);
    gamestate.fruits.current = mread32(mf);
    /* clean up fruit(s) made by initoptions() */
    freefruitchn(gamestate.fruits.chain);
    /* set it to NULL before loadfruitchn, otherwise loading a faulty fruit
       chain will crash in terminate -> freedynamicdata -> freefruitchn */
    gamestate.fruits.chain = NULL;
    gamestate.fruits.chain = loadfruitchn(mf);

    restnames(mf);
    restore_waterlevel(mf, lev);

    mread(mf, flags.rngstate, sizeof flags.rngstate);

    restore_track(mf);
    restore_rndmonst_state(mf);
    restore_history(mf);

    /* must come after all mons & objs are restored */
    relink_timers(FALSE, lev, NULL);
    relink_light_sources(FALSE, lev);

    if (u.ustuck) {
        for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
            if (mtmp->m_id == (intptr_t) u.ustuck)
                break;
        if (!mtmp)
            panic("Cannot find the monster ustuck.");
        u.ustuck = mtmp;
    }
    if (u.usteed) {
        for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon)
            if (mtmp->m_id == (intptr_t) u.usteed)
                break;
        if (!mtmp)
            panic("Cannot find the monster usteed.");
        u.usteed = mtmp;
        remove_monster(lev, mtmp->mx, mtmp->my);
    }
}

void
restore_you(struct memfile *mf, struct you *y)
{
    int i;
    unsigned int yflags, eflags;

    memset(y, 0, sizeof (struct you));
    u.ubirthday = mread64(mf);

    yflags = mread32(mf);
    eflags = mread32(mf);

    y->uswallow = (yflags >> 31) & 1;
    y->uinwater = (yflags >> 30) & 1;
    y->uundetected = (yflags >> 29) & 1;
    y->mfemale = (yflags >> 28) & 1;
    y->uinvulnerable = (yflags >> 27) & 1;
    y->uburied = (yflags >> 26) & 1;
    y->uedibility = (yflags >> 25) & 1;
    y->uwelcomed = (yflags >> 24) & 1;
    y->usick_type = (yflags >> 22) & 3;
    y->ufemale = (yflags >> 21) & 1;

    y->uevent.minor_oracle = (eflags >> 31) & 1;
    y->uevent.major_oracle = (eflags >> 30) & 1;
    y->uevent.qcalled = (eflags >> 29) & 1;
    y->uevent.qexpelled = (eflags >> 28) & 1;
    y->uevent.qcompleted = (eflags >> 27) & 1;
    y->uevent.uheard_tune = (eflags >> 25) & 3;
    y->uevent.uopened_dbridge = (eflags >> 24) & 1;
    y->uevent.invoked = (eflags >> 23) & 1;
    y->uevent.gehennom_entered = (eflags >> 22) & 1;
    y->uevent.uhand_of_elbereth = (eflags >> 20) & 3;
    y->uevent.udemigod = (eflags >> 19) & 1;
    y->uevent.ascended = (eflags >> 18) & 1;

    y->uhp = mread32(mf);
    y->uhpmax = mread32(mf);
    y->uen = mread32(mf);
    y->uenmax = mread32(mf);
    y->ulevel = mread32(mf);
    y->umoney0 = mread32(mf);
    y->uexp = mread32(mf);
    y->urexp = mread32(mf);
    y->ulevelmax = mread32(mf);
    y->umonster = mread32(mf);
    y->umonnum = mread32(mf);
    y->mh = mread32(mf);
    y->mhmax = mread32(mf);
    y->mtimedone = mread32(mf);
    y->ulycn = mread32(mf);
    y->utrap = save_decode_32(mread32(mf), -moves, -moves);
    y->utraptype = mread32(mf);
    y->uhunger = save_decode_32(mread32(mf), -moves, -moves);
    y->uhs = mread32(mf);
    y->oldcap = mread32(mf);
    y->umconf = save_decode_32(mread32(mf), -moves, -moves);
    y->nv_range = mread32(mf);
    y->bglyph = mread32(mf);
    y->cglyph = mread32(mf);
    y->bc_order = mread32(mf);
    y->bc_felt = mread32(mf);
    y->ucreamed = mread32(mf);
    y->uswldtim = mread32(mf);
    y->udg_cnt = mread32(mf);
    y->next_attr_check = mread32(mf);
    y->ualign.record = mread32(mf);
    y->ugangr = mread32(mf);
    y->ugifts = mread32(mf);
    y->ublessed = mread32(mf);
    y->ublesscnt = save_decode_32(mread32(mf), -moves, -moves);
    y->ucleansed = mread32(mf);
    y->uinvault = mread32(mf);
    y->ugallop = mread32(mf);
    y->urideturns = mread32(mf);
    y->umortality = mread32(mf);
    y->ugrave_arise = mread32(mf);
    y->weapon_slots = mread32(mf);
    y->skills_advanced = mread32(mf);
    y->initrole = mread32(mf);
    y->initrace = mread32(mf);
    y->initgend = mread32(mf);
    y->initalign = mread32(mf);
    y->upantheon = mread32(mf);

    /* at this point, ustuck and usteed are mon ids rather than pointers */
    y->ustuck = (void *)(intptr_t) mread32(mf);
    y->usteed = (void *)(intptr_t) mread32(mf);

    y->ux = mread8(mf);
    y->uy = mread8(mf);
    y->tx = mread8(mf);
    y->ty = mread8(mf);
    y->ux0 = mread8(mf);
    y->uy0 = mread8(mf);
    y->uz.dnum = mread8(mf);
    y->uz.dlevel = mread8(mf);
    /* Padding to replace utolev/utotype, which was removed. */
    /* SAVEBREAK (4.3-beta1 -> 4.3-beta2): delete the next few lines. */
    y->save_compat_bytes[0] = mread8(mf);
    y->save_compat_bytes[1] = mread8(mf);
    y->save_compat_bytes[2] = mread8(mf);
    y->umoved = mread8(mf);
    y->ualign.type = mread8(mf);
    y->ualignbase[0] = mread8(mf);
    y->ualignbase[1] = mread8(mf);
    y->uluck = mread8(mf);
    y->moreluck = mread8(mf);
    y->uhitinc = mread8(mf);
    y->udaminc = mread8(mf);
    y->uac = mread8(mf);
    y->uspellprot = mread8(mf);
    y->usptime = save_decode_8(mread8(mf), -moves, -moves);
    y->uspmtime = mread8(mf);
    y->twoweap = mread8(mf);
    y->bashmsg = mread8(mf);
    y->moveamt = mread8(mf);

    /* Ignore the padding added in save.c */
    for (i = 0; i < 511; i++)
        (void) mread8(mf);

    mread(mf, y->ever_extrinsic, sizeof (y->ever_extrinsic));
    mread(mf, y->ever_intrinsic, sizeof (y->ever_intrinsic));
    mread(mf, y->ever_temporary, sizeof (y->ever_temporary));
    mread(mf, y->uwhybusy, sizeof (y->uwhybusy));
    mread(mf, y->urooms, sizeof (y->urooms));
    mread(mf, y->urooms0, sizeof (y->urooms0));
    mread(mf, y->uentered, sizeof (y->uentered));
    mread(mf, y->ushops, sizeof (y->ushops));
    mread(mf, y->ushops0, sizeof (y->ushops0));
    mread(mf, y->ushops_entered, sizeof (y->ushops_entered));
    mread(mf, y->ushops_left, sizeof (y->ushops_left));
    mread(mf, y->macurr.a, sizeof (y->macurr.a));
    mread(mf, y->mamax.a, sizeof (y->mamax.a));
    mread(mf, y->acurr.a, sizeof (y->acurr.a));
    mread(mf, y->aexe.a, sizeof (y->aexe.a));
    mread(mf, y->abon.a, sizeof (y->abon.a));
    mread(mf, y->amax.a, sizeof (y->amax.a));
    mread(mf, y->atemp.a, sizeof (y->atemp.a));
    mread(mf, y->atime.a, sizeof (y->atime.a));
    mread(mf, y->skill_record, sizeof (y->skill_record));
    mread(mf, y->uplname, sizeof (y->uplname));

    for (i = 0; i < num_conducts; i++) {
        y->uconduct[i] = mread32(mf);
        y->uconduct_time[i] = mread32(mf);
    }
    for (i = 0; i <= LAST_PROP; i++) {
        y->uintrinsic[i] = mread32(mf);
    }
    for (i = 0; i < P_NUM_SKILLS; i++) {
        y->weapon_skills[i].skill = mread8(mf);
        y->weapon_skills[i].max_skill = mread8(mf);
        y->weapon_skills[i].advance = mread16(mf);
    }

    restore_quest_status(mf, &y->quest_status);

    int len = mread32(mf);
    if (len > 0) {
        char *buf = malloc(len + 1);
        mread(mf, buf, len);
        buf[len] = '\0';
        y->delayed_killers.stoning = buf;
    } else {
        y->delayed_killers.stoning = NULL;
    }

    len = mread32(mf);
    if (len > 0) {
        char *buf = malloc(len + 1);
        mread(mf, buf, len);
        buf[len] = '\0';
        y->delayed_killers.sliming = buf;
    } else {
        y->delayed_killers.sliming = NULL;
    }

    len = mread32(mf);
    if (len > 0) {
        char *buf = malloc(len + 1);
        mread(mf, buf, len);
        buf[len] = '\0';
        y->delayed_killers.illness = buf;
    } else {
        y->delayed_killers.illness = NULL;
    }

    len = mread32(mf);
    if (len > 0) {
        char *buf = malloc(len + 1);
        mread(mf, buf, len);
        buf[len] = '\0';
        y->delayed_killers.genocide = buf;
    } else {
        y->delayed_killers.genocide = NULL;
    }

    y->lastinvnr = mread32(mf);
}

static void
restore_utracked(struct memfile *mf, struct you *y)
{
    int i;
    for (i = 0; i <= tos_last_slot; i++) {
        int oid;
        y->utracked[i] = NULL;
        oid = mread32(mf);
        if (oid == -1)
            y->utracked[i] = &zeroobj;
        else if (oid)
            y->utracked[i] = find_oid(oid);
        y->uoccupation_progress[i] = mread32(mf);
    }
    for (i = 0; i <= tl_last_slot; i++) {
        y->utracked_location[i].x = mread8(mf);
        y->utracked_location[i].y = mread8(mf);
    }
}


static void
restore_autopickup_rules(struct memfile *mf, struct nh_autopickup_rules *ar)
{
    int len = mread32(mf), i;
    ar->num_rules = len;
    ar->rules = malloc(len * sizeof(struct nh_autopickup_rule));
    for (i = 0; i < len; ++i) {
        mread(mf, ar->rules[i].pattern, sizeof (ar->rules[i].pattern));
        ar->rules[i].oclass = mread16(mf);
        ar->rules[i].buc = mread8(mf);
        ar->rules[i].action = mread8(mf);
    }
}


/* WARNING: This function is responsible for determining what save encoding
   is used in a file. Thus, it cannot use any save encoding functions itself. */
void
restore_flags(struct memfile *mf, struct flag *f)
{
    struct nh_autopickup_rules *ar = f->ap_rules;
    int lsbl;
    int i;

    if (ar)
        free(ar->rules);
    memset(f, 0, sizeof (struct flag));

    f->turntime = mread64(mf);

    f->djinni_count = mread32(mf);
    f->ghost_count = mread32(mf);
    f->timezone = mread32(mf);
    f->polyinit_mnum = mread32(mf);
    f->ident = mread32(mf);
    f->last_cmd = mread32(mf);
    f->moonphase = mread32(mf);
    f->no_of_wizards = mread32(mf);
    f->pickup_burden = mread32(mf);
    f->recently_broken_otyp = mread32(mf);

    f->autodig = mread8(mf);
    f->autodigdown = mread8(mf);
    f->autoquiver = mread8(mf);
    f->beginner = mread8(mf);
    f->bones_enabled = mread8(mf);
    f->cblock = mread8(mf);
    f->corridorbranch = mread8(mf);
    f->debug = mread8(mf);
    f->desync = mread8(mf);
    f->explore = mread8(mf);
    f->elbereth_enabled = mread8(mf);
    f->end_disclose = mread8(mf);
    f->friday13 = mread8(mf);
    f->incomplete = mread8(mf);
    f->interaction_mode = mread8(mf);
    f->interrupted = mread8(mf);
    f->legacy = mread8(mf);
    f->made_amulet = mread8(mf);
    f->menu_style = mread8(mf);
    f->mon_generation = mread8(mf);
    f->mon_moving = mread8(mf);
    f->mon_polycontrol = mread8(mf);
    f->occupation = mread8(mf);
    f->permablind = mread8(mf);
    f->permahallu = mread8(mf);
    f->pickup = mread8(mf);
    f->pickup_thrown = mread8(mf);
    f->prayconfirm = mread8(mf);
    f->pushweapon = mread8(mf);
    f->rogue_enabled = mread8(mf);
    f->seduce_enabled = mread8(mf);
    f->showrace = mread8(mf);
    f->show_uncursed = mread8(mf);
    f->sortpack = mread8(mf);
    f->sparkle = mread8(mf);
    f->tombstone = mread8(mf);
    f->travel_interrupt = mread8(mf);
    f->verbose = mread8(mf);

    f->last_arg.argtype = mread32(mf);
    f->last_arg.dir = mread32(mf);
    f->last_arg.pos.x = mread16(mf);
    f->last_arg.pos.y = mread16(mf);
    f->last_arg.invlet = mread8(mf);
    f->last_arg.spelllet = mread8(mf);
    f->last_arg.limit = mread32(mf);

    f->actions = mread8(mf);
    f->save_encoding = mread8(mf);
    f->hide_implied = mread8(mf);

    /* Ignore the padding added in save.c */
    for (i = 0; i < 109; i++)
        (void) mread8(mf);

    mread(mf, f->setseed, sizeof (f->setseed));
    mread(mf, f->inv_order, sizeof (f->inv_order));

    lsbl = mread32(mf);
    if (f->last_str_buf)
        free(f->last_str_buf);
    f->last_str_buf = malloc(lsbl + 1);
    mread(mf, f->last_str_buf, lsbl);
    f->last_str_buf[lsbl] = '\0';
    f->last_arg.str = f->last_str_buf;

    if (!ar)
        ar = malloc(sizeof(struct nh_autopickup_rules));
    restore_autopickup_rules(mf, ar);
    f->ap_rules = ar;

    restore_coords(mf, &f->travelcc, 1);
}


int
dorecover(struct memfile *mf)
{
    int count;
    xchar ltmp;
    struct obj *otmp;
    struct monst *mtmp;

    int temp_pos;       /* in case we're both reading and writing the file */

    temp_pos = mf->pos;
    mf->pos = 0;

    if (!uptodate(mf, NULL)) {
        mf->pos = temp_pos;
        return 0;
    }

    program_state.restoring_binary_save = TRUE;

    moves = mread32(mf);

    level = NULL;       /* level restore must not use this pointer */

    restore_flags(mf, &flags);
    flags.bypasses = 0; /* never use a saved value of bypasses */

    restore_you(mf, &u);
    role_init();       /* Reset the initial role, race, gender, and alignment */
    pantheon_init(FALSE);

    mtmp = restore_mon(mf, NULL);
    youmonst = *mtmp;
    dealloc_monst(mtmp);
    set_uasmon();       /* fix up youmonst.data */

    /* restore dungeon */
    restore_dungeon(mf);
    restlevchn(mf);

    /* restore levels */
    count = mread32(mf);
    for (; count; count--) {
        ltmp = mread8(mf);
        getlev(mf, ltmp, FALSE);
    }

    restgamestate(mf);

    /* utracked is read last, because it might need to find items on any level
       or in the player inventory */
    restore_utracked(mf, &u);

    /* all data has been read, prepare for player */
    level = levels[ledger_no(&u.uz)];

    max_rank_sz();      /* to recompute mrank_sz (botl.c) */
    /* take care of iron ball & chain */
    for (otmp = level->objlist; otmp; otmp = otmp->nobj)
        if (otmp->owornmask)
            setworn(otmp, otmp->owornmask);
    /*
     * in_use processing must be after:
     * + The inventory has been read so that freeinv() works.
     * + The current level has been restored so billing information is
     *   available. */
    inven_inuse(FALSE);

    load_qtlist();      /* re-load the quest text info */

    program_state.restoring_binary_save = FALSE;

    flags.desync = FALSE;   /* desync the save if this was set to TRUE */

    /* Note: dorecover() no longer calls run_timers() or doredraw(), because
       those can have side effects that are saved in the save file (e.g. running
       the vision code), which would cause loading and immediately saving to
       change the save file, something that the save code detects as a
       desync. Therefore, this is now the caller's job. */

    /* Success! */
    mf->pos = temp_pos;
    return 1;
}

void
trickery(const char *reason)
{
    pline(msgc_saveload, "Strange, this map is not as I remember it.");
    pline(msgc_saveload, "Somebody is trying some trickery here...");
    pline(msgc_saveload, "This game is void.");
    pline(msgc_saveload, "%s", reason);
    done(TRICKED, NULL);
}


static void
restore_location(struct memfile *mf, struct rm *loc)
{
    unsigned int lflags1;
    unsigned short lflags2;
    int l = 0, t = 0;

    lflags1 = mread32(mf);
    loc->typ = mread8(mf);
    loc->seenv = mread8(mf);
    lflags2 = mread16(mf);
    loc->mem_bg = (lflags1 >> 26) & 63;
    loc->mem_trap = (lflags1 >> 21) & 31;
    loc->mem_obj = (lflags1 >> 11) & 1023;
    loc->mem_obj_mn = (lflags1 >> 2) & 511;
    loc->mem_invis = (lflags1 >> 1) & 1;
    loc->mem_stepped = (lflags1 >> 0) & 1;
    loc->flags = (lflags2 >> 11) & 31;
    loc->horizontal = (lflags2 >> 10) & 1;
    loc->lit = (lflags2 >> 9) & 1;
    loc->waslit = (lflags2 >> 8) & 1;
    loc->roomno = (lflags2 >> 2) & 63;
    loc->edge = (lflags2 >> 1) & 1;

    /* unpack mem_door_l, mem_door_t from mem_bg */
    switch (loc->mem_bg) {
    case S_vodoor_memlt:
        loc->mem_bg = S_vodoor;
        l = t = 1;
        break;
    case S_vodoor_meml:
        loc->mem_bg = S_vodoor;
        l = 1;
        break;
    case S_vodoor_memt:
        loc->mem_bg = S_vodoor;
        t = 1;
        break;
    case S_hodoor_memlt:
        loc->mem_bg = S_hodoor;
        l = t = 1;
        break;
    case S_hodoor_meml:
        loc->mem_bg = S_hodoor;
        l = 1;
        break;
    case S_hodoor_memt:
        loc->mem_bg = S_hodoor;
        t = 1;
        break;
    case S_vcdoor_memlt:
        loc->mem_bg = S_vcdoor;
        l = t = 1;
        break;
    case S_vcdoor_meml:
        loc->mem_bg = S_vcdoor;
        l = 1;
        break;
    case S_vcdoor_memt:
        loc->mem_bg = S_vcdoor;
        t = 1;
        break;
    case S_hcdoor_memlt:
        loc->mem_bg = S_hcdoor;
        l = t = 1;
        break;
    case S_hcdoor_meml:
        loc->mem_bg = S_hcdoor;
        l = 1;
        break;
    case S_hcdoor_memt:
        loc->mem_bg = S_hcdoor;
        t = 1;
        break;
    }
    loc->mem_door_l = l;
    loc->mem_door_t = t;
}


static struct trap *
restore_traps(struct memfile *mf)
{
    struct trap *trap, *first = NULL, *prev = NULL;
    unsigned int count, tflags;

    mfmagic_check(mf, TRAPCHAIN_MAGIC);
    count = mread32(mf);

    while (count--) {
        trap = newtrap();

        trap->tx = mread8(mf);
        trap->ty = mread8(mf);
        trap->dst.dnum = mread8(mf);
        trap->dst.dlevel = mread8(mf);
        trap->launch.x = mread8(mf);
        trap->launch.y = mread8(mf);

        tflags = mread16(mf);
        trap->ttyp = (tflags >> 11) & 31;
        trap->tseen = (tflags >> 10) & 1;
        trap->once = (tflags >> 9) & 1;
        trap->madeby_u = (tflags >> 8) & 1;

        trap->vl.v_launch_otyp = mread16(mf);

        trap->ntrap = NULL;
        if (!first)
            first = trap;
        else
            prev->ntrap = trap;
        prev = trap;
    }

    return first;
}


static void
restore_stairway(struct memfile *mf, stairway *s)
{
    /* SAVEBREAK (4.3-beta1 -> 4.3-beta2)

       If s.sx and s.sy are COLNO and ROWNO respectively, save as 0, 0. */
    s->sx = mread8(mf);
    s->sy = mread8(mf);
    if (!s->sx && !s->sy)
        s->sx = COLNO, s->sy = ROWNO;
    restore_dlevel(mf, &s->tolev);
    s->up = mread8(mf);
}


static void
restore_dest_area(struct memfile *mf, dest_area *a)
{
    a->lx = mread8(mf);
    a->ly = mread8(mf);
    a->hx = mread8(mf);
    a->hy = mread8(mf);
    a->nlx = mread8(mf);
    a->nly = mread8(mf);
    a->nhx = mread8(mf);
    a->nhy = mread8(mf);

    /* SAVEBREAK (4.3-beta1 -> 4.3-beta2)

       A level region spanning the entire level is saved with eight zeroes
       in -beta1. It should be using COLNO/ROWNO sentinels. */
    if (!a->lx && !a->ly && !a->hx && !a->hy) {
        a->lx = COLNO;
        a->ly = ROWNO;
        a->hx = COLNO;
        a->hy = ROWNO;
    }
    if (!a->nlx && !a->nly && !a->nhx && !a->nhy) {
        a->nlx = COLNO;
        a->nly = ROWNO;
        a->nhx = COLNO;
        a->nhy = ROWNO;
    }
}


void
restore_coords(struct memfile *mf, coord *c, int n)
{
    coord *p;
    for (p = c; p < c + n; ++p) {
        p->x = mread8(mf);
        p->y = mread8(mf);
    }
}


struct level *
getlev(struct memfile *mf, xchar levnum, boolean ghostly)
{
    struct monst *mtmp;
    branch *br;
    int x, y;
    unsigned int lflags;
    struct level *lev;
    struct trietable *table = NULL;

    if (ghostly)
        clear_id_mapping();

    /* Load the old fruit info.  We have to do it first, so the information is
       available when restoring the objects. */
    if (ghostly)
        oldfruit = loadfruitchn(mf);

    /* for bones files, there is fruit chain data before the level data */
    mfmagic_check(mf, LEVEL_MAGIC);

    if (levels[levnum])
        panic("Unsupported: trying to restore level %d which already exists.\n",
              levnum);
    lev = levels[levnum] = alloc_level(NULL);

    if (ghostly) {
        /* A bones level may move from its original location. We need to make
         * sure it's set to the correct new location. */
        lev->z.dnum = ledger_to_dnum(levnum);
        if (mread8(mf) != lev->z.dnum)
            panic("Restoring level into wrong dungeon!");
        lev->z.dlevel = ledger_to_dlev(levnum);
        (void) mread8(mf); /* ignore the dlevel in the bones file */

        if (!on_level(&u.uz, &lev->z))
            panic("Restoring the wrong level.");
    } else {
        /* SAVEBREAK: Eliminate this branch, which is only necessary to maintain
         * compatibility with bones files loaded incorrectly and avoid desyncing
         * them. */
        lev->z.dnum = mread8(mf);
        lev->z.dlevel = mread8(mf);
    }

    /* can't do this until we know lev->z */
    init_rect(rng_for_level(&lev->z));

    mread(mf, lev->levname, sizeof (lev->levname));
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            restore_location(mf, &lev->locations[x][y]);

    lev->lastmoves = mread32(mf);
    restore_stairway(mf, &lev->upstair);
    restore_stairway(mf, &lev->dnstair);
    restore_stairway(mf, &lev->upladder);
    restore_stairway(mf, &lev->dnladder);
    restore_stairway(mf, &lev->sstairs);
    restore_dest_area(mf, &lev->updest);
    restore_dest_area(mf, &lev->dndest);

    lflags = mread32(mf);
    lev->flags.noteleport = (lflags >> 22) & 1;
    lev->flags.hardfloor = (lflags >> 21) & 1;
    lev->flags.nommap = (lflags >> 20) & 1;
    lev->flags.hero_memory = (lflags >> 19) & 1;
    lev->flags.shortsighted = (lflags >> 18) & 1;
    lev->flags.graveyard = (lflags >> 17) & 1;
    lev->flags.is_maze_lev = (lflags >> 16) & 1;
    lev->flags.is_cavernous_lev = (lflags >> 15) & 1;
    lev->flags.arboreal = (lflags >> 14) & 1;
    lev->flags.forgotten = (lflags >> 13) & 1;

    restore_coords(mf, lev->doors, DOORMAX);
    rest_rooms(mf, lev);        /* No joke :-) */
    if (lev->nroom)
        lev->doorindex =
            lev->rooms[lev->nroom - 1].fdoor +
            lev->rooms[lev->nroom - 1].doorct;
    else
        lev->doorindex = 0;

    restore_timers(mf, lev, RANGE_LEVEL, ghostly, moves - lev->lastmoves);
    restore_light_sources(mf, lev);
    lev->monlist = restmonchn(mf, lev, ghostly);

    if (ghostly) {
        struct monst *mtmp2;

        for (mtmp = lev->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon;
            /* reset peaceful/malign relative to new character */
            if (!mtmp->isshk)
                /* shopkeepers will reset based on name */
                mtmp->mpeaceful = peace_minded(mtmp->data);
            set_malign(mtmp);

            /* update shape-changers in case protection against them is
               different now than when the level was saved */
            restore_cham(mtmp);
        }
    }

    rest_worm(mf, lev); /* restore worm information */
    lev->lev_traps = restore_traps(mf);
    restobjchn(mf, lev, ghostly, FALSE, &lev->objlist, &table);
    find_lev_obj(lev);
    /* restobjchn()'s `frozen' argument probably ought to be a callback routine
       so that we can check for objects being buried under ice */
    restobjchn(mf, lev, ghostly, FALSE, &lev->buriedobjlist, &table);
    restobjchn(mf, lev, ghostly, FALSE, &lev->billobjs, &table);
    rest_engravings(mf, lev);

    /* reset level->monsters for new level */
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            lev->monsters[x][y] = NULL;
    for (mtmp = lev->monlist; mtmp; mtmp = mtmp->nmon) {
        if (mtmp->isshk)
            set_residency(mtmp, FALSE);

        /* mtmp->mx == COLNO is a sentinel value. place_monster rejects it to
         * avoid illegal positions popping up accidentally. We don't perform
         * other checks because we would like any other value to cause an
         * impossible() call.
         */
        if (mtmp->mx != COLNO)
            place_monster(mtmp, mtmp->mx, mtmp->my);
        if (mtmp->wormno)
            place_wsegs(mtmp);
    }
    restdamage(mf, lev, ghostly);

    rest_regions(mf, lev, ghostly);
    if (ghostly) {
        /* assert(lev->z == u.uz); */

        /* Now get rid of all the temp fruits... */
        freefruitchn(oldfruit), oldfruit = 0;

        /* TODO: Is this dead code? I /hope/ it's dead code.

           The purpose seems to ensure that below-Medusa filler levels in bones
           have a downstairs, no matter what. I can't think of a situation where
           such a level wouldn't have a downstairs. */
        if (levnum > ledger_no(&medusa_level) &&
            levnum < ledger_no(&stronghold_level) &&
            !isok(lev->dnstair.sx, lev->dnstair.sy)) {
            coord cc;

            mazexy(lev, &cc);
            lev->dnstair.sx = cc.x;
            lev->dnstair.sy = cc.y;
            lev->locations[cc.x][cc.y].typ = STAIRS;
        }

        br = Is_branchlev(&lev->z);
        if (br && lev->z.dlevel == 1) {
            d_level ltmp;

            if (on_level(&lev->z, &br->end1))
                assign_level(&ltmp, &br->end2);
            else
                assign_level(&ltmp, &br->end1);

            switch (br->type) {
            case BR_STAIR:
            case BR_NO_END1:
            case BR_NO_END2:   /* OK to assign to sstairs if it's not used */
                assign_level(&lev->sstairs.tolev, &ltmp);
                break;
            case BR_PORTAL:    /* max of 1 portal per level */
                {
                    struct trap *ttmp;

                    for (ttmp = lev->lev_traps; ttmp; ttmp = ttmp->ntrap)
                        if (ttmp->ttyp == MAGIC_PORTAL)
                            break;
                    if (!ttmp)
                        panic("getlev: need portal but none found");
                    assign_level(&ttmp->dst, &ltmp);
                }
                break;
            }
        } else if (!br) {
            /* Remove any dangling portals. */
            struct trap *ttmp;

            for (ttmp = lev->lev_traps; ttmp; ttmp = ttmp->ntrap)
                if (ttmp->ttyp == MAGIC_PORTAL) {
                    deltrap(lev, ttmp);
                    break;      /* max of 1 portal/level */
                }
        }
    }

    /* must come after all mons & objs are restored */
    relink_timers(ghostly, lev, &table);
    relink_light_sources(ghostly, lev);
    reset_oattached_mids(ghostly, lev);

    if (ghostly)
        clear_id_mapping();

    trietable_empty(&table);

    return lev;
}


/* Clear all structures for object and monster ID mapping. */
static void
clear_id_mapping(void)
{
    struct bucket *curr;

    while ((curr = id_map) != 0) {
        id_map = curr->next;
        free(curr);
    }
    n_ids_mapped = 0;
}

/* Add a mapping to the ID map. */
static void
add_id_mapping(unsigned gid, unsigned nid)
{
    int idx;

    idx = n_ids_mapped % N_PER_BUCKET;
    /* idx is zero on first time through, as well as when a new bucket is */
    /* needed */
    if (idx == 0) {
        struct bucket *gnu = malloc(sizeof (struct bucket));

        gnu->next = id_map;
        id_map = gnu;
    }

    id_map->map[idx].gid = gid;
    id_map->map[idx].nid = nid;
    n_ids_mapped++;
}

/*
 * Global routine to look up a mapping.  If found, return TRUE and fill
 * in the new ID value.  Otherwise, return false and return -1 in the new
 * ID.
 */
boolean
lookup_id_mapping(unsigned gid, unsigned *nidp)
{
    int i;
    struct bucket *curr;

    if (n_ids_mapped)
        for (curr = id_map; curr; curr = curr->next) {
            /* first bucket might not be totally full */
            if (curr == id_map) {
                i = n_ids_mapped % N_PER_BUCKET;
                if (i == 0)
                    i = N_PER_BUCKET;
            } else
                i = N_PER_BUCKET;

            while (--i >= 0)
                if (gid == curr->map[i].gid) {
                    *nidp = curr->map[i].nid;
                    return TRUE;
                }
        }

    return FALSE;
}

static void
reset_oattached_mids(boolean ghostly, struct level *lev)
{
    struct obj *otmp;
    unsigned oldid, nid;

    for (otmp = lev->objlist; otmp; otmp = otmp->nobj) {
        if (ghostly && otmp->oattached == OATTACHED_MONST && otmp->oxlth) {
            struct monst *mtmp = (struct monst *)otmp->oextra;

            mtmp->m_id = 0;
            mtmp->mpeaceful = mtmp->mtame = 0;  /* pet's owner died! */
        }
        if (ghostly && otmp->oattached == OATTACHED_M_ID) {
            memcpy(&oldid, (void *)otmp->oextra, sizeof (oldid));
            if (lookup_id_mapping(oldid, &nid))
                memcpy(otmp->oextra, (void *)&nid, sizeof (nid));
            else
                otmp->oattached = OATTACHED_NOTHING;
        }
    }
}


/*restore.c*/
