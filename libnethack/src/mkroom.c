/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Entry points:
 *      mkroom() -- make and stock a room of a given type
 *      nexttodoor() -- return TRUE if adjacent to a door
 *      has_dnstairs() -- return TRUE if given room has a down staircase
 *      has_upstairs() -- return TRUE if given room has an up staircase
 *      courtmon() -- generate a court monster
 *      save_rooms() -- save level->rooms into file fd
 *      rest_rooms() -- restore level->rooms from file fd
 */

#include "hack.h"


static boolean isbig(struct mkroom *);
static struct mkroom *pick_room(struct level *lev, boolean strict, enum rng);
static void mkshop(struct level *lev);
static void mkzoo(struct level *lev, int type, enum rng rng);
static void mkswamp(struct level *lev);
static void mktemple(struct level *lev);
static coord *shrine_pos(struct level *lev, int roomno);
static const struct permonst *morguemon(const d_level *dlev, enum rng rng);
static const struct permonst *squadmon(const d_level *dlev);
static void save_room(struct memfile *mf, struct mkroom *);
static void rest_room(struct memfile *mf, struct level *lev, struct mkroom *r);
static boolean has_dnstairs(struct level *lev, struct mkroom *);
static boolean has_upstairs(struct level *lev, struct mkroom *);

#define sq(x) ((x)*(x))


static boolean
isbig(struct mkroom *sroom)
{
    int area = (sroom->hx - sroom->lx + 1)
        * (sroom->hy - sroom->ly + 1);

    return (boolean) (area > 20);
}

/* make and stock a room of a given type

   Note: must not use the level creation RNG if this is of a room type that's
   affected by genocide/extinction (LEPREHALL, BEEHIVE, BARRACKS, ANTHOLE,
   COCKNEST), or genocide/extinction would change the layout of the rest of the
   level. */
void
mkroom(struct level *lev, int roomtype)
{
    if (roomtype >= SHOPBASE)
        mkshop(lev);    /* someday, we should be able to specify shop type */
    else
        switch (roomtype) {
        case COURT:
            mkzoo(lev, COURT, rng_for_level(&lev->z));
            break;
        case ZOO:
            mkzoo(lev, ZOO, rng_for_level(&lev->z));
            break;
        case BEEHIVE:
            mkzoo(lev, BEEHIVE, rng_main);
            break;
        case MORGUE:
            mkzoo(lev, MORGUE, rng_for_level(&lev->z));
            break;
        case BARRACKS:
            mkzoo(lev, BARRACKS, rng_main);
            break;
        case SWAMP:
            mkswamp(lev);
            break;
        case TEMPLE:
            mktemple(lev);
            break;
        case LEPREHALL:
            mkzoo(lev, LEPREHALL, rng_main);
            break;
        case COCKNEST:
            mkzoo(lev, COCKNEST, rng_main);
            break;
        case ANTHOLE:
            mkzoo(lev, ANTHOLE, rng_main);
            break;
        default:
            impossible("Tried to make a room of type %d.", roomtype);
        }
}


static void
mkshop(struct level *lev)
{
    struct mkroom *sroom;
    int styp, j;
    char *ep = NULL;

    /* first determine shoptype */
    styp = -1;
    for (sroom = &lev->rooms[0];; sroom++) {
        if (sroom->hx < 0)
            return;
        if (sroom - lev->rooms >= lev->nroom) {
            impossible("lev->rooms not closed by -1?");
            return;
        }
        if (sroom->rtype != OROOM)
            continue;
        if (has_dnstairs(lev, sroom) || has_upstairs(lev, sroom))
            continue;
        if ((wizard && ep && sroom->doorct != 0) || sroom->doorct == 1)
            break;
    }
    if (!sroom->rlit) {
        int x, y;

        for (x = sroom->lx - 1; x <= sroom->hx + 1; x++)
            for (y = sroom->ly - 1; y <= sroom->hy + 1; y++)
                lev->locations[x][y].lit = 1;
        sroom->rlit = 1;
    }

    if (styp < 0) {
        /* pick a shop type at random */
        j = 1 + mklev_rn2(100, lev);
        for (styp = 0; (j -= shtypes[styp].prob) > 0; styp++)
            continue;

        /* big rooms cannot be wand or book shops, so make them general stores
           */
        if (isbig(sroom) &&
            (shtypes[styp].symb == WAND_CLASS ||
             shtypes[styp].symb == SPBOOK_CLASS))
            styp = 0;
    }

    sroom->rtype = SHOPBASE + styp;

    /* set room bits before stocking the shop */
    topologize(lev, sroom);

    /* stock the room with a shopkeeper and artifacts */
    stock_room(styp, lev, sroom);
}


/* pick an unused room, preferably with only one door */
static struct mkroom *
pick_room(struct level *lev, boolean strict, enum rng rng)
{
    struct mkroom *sroom;
    int i = lev->nroom;

    for (sroom = &lev->rooms[rn2_on_rng(lev->nroom, rng)]; i--; sroom++) {
        if (sroom == &lev->rooms[lev->nroom])
            sroom = &lev->rooms[0];
        if (sroom->hx < 0)
            return NULL;
        if (sroom->rtype != OROOM)
            continue;
        if (!strict) {
            if (has_upstairs(lev, sroom) ||
                (has_dnstairs(lev, sroom) && rn2_on_rng(3, rng)))
                continue;
        } else if (has_upstairs(lev, sroom) || has_dnstairs(lev, sroom))
            continue;
        if (sroom->doorct == 1 || !rn2_on_rng(5, rng) || wizard)
            return sroom;
    }
    return NULL;
}

static void
mkzoo(struct level *lev, int type, enum rng rng)
{
    struct mkroom *sroom;

    if ((sroom = pick_room(lev, FALSE, rng)) != 0) {
        sroom->rtype = type;
        fill_zoo(lev, sroom, rng);
    }
}

void
fill_zoo(struct level *lev, struct mkroom *sroom, enum rng rng)
{
    struct monst *mon;
    int sx, sy, i;
    int sh, tx, ty, goldlim, type = sroom->rtype;
    int rmno = (sroom - lev->rooms) + ROOMOFFSET;
    coord mm;

    tx = ty = goldlim = 0;

    sh = sroom->fdoor;
    switch (type) {
    case COURT:
        if (lev->flags.is_maze_lev) {
            for (tx = sroom->lx; tx <= sroom->hx; tx++)
                for (ty = sroom->ly; ty <= sroom->hy; ty++)
                    if (IS_THRONE(lev->locations[tx][ty].typ))
                        goto throne_placed;
        }
        i = 100;
        do {    /* don't place throne on top of stairs */
            somexy(lev, sroom, &mm, rng);
            tx = mm.x;
            ty = mm.y;
        } while (occupied(lev, tx, ty) && --i > 0);
    throne_placed:
        /* TODO: try to ensure the enthroned monster is an M2_PRINCE */
        break;
    case BEEHIVE:
        tx = sroom->lx + (sroom->hx - sroom->lx + 1) / 2;
        ty = sroom->ly + (sroom->hy - sroom->ly + 1) / 2;
        if (sroom->irregular) {
            /* center might not be valid, so put queen elsewhere */
            if ((int)lev->locations[tx][ty].roomno != rmno ||
                lev->locations[tx][ty].edge) {
                somexy(lev, sroom, &mm, rng);
                tx = mm.x;
                ty = mm.y;
            }
        }
        break;
    case ZOO:
    case LEPREHALL:
        goldlim = 500 * level_difficulty(&lev->z);
        break;
    }
    for (sx = sroom->lx; sx <= sroom->hx; sx++)
        for (sy = sroom->ly; sy <= sroom->hy; sy++) {
            if (sroom->irregular) {
                if ((int)lev->locations[sx][sy].roomno != rmno ||
                    lev->locations[sx][sy].edge ||
                    (sroom->doorct &&
                     distmin(sx, sy, lev->doors[sh].x, lev->doors[sh].y) <= 1))
                    continue;
            } else if (!SPACE_POS(lev->locations[sx][sy].typ) ||
                       (sroom->doorct &&
                        ((sx == sroom->lx && lev->doors[sh].x == sx - 1) ||
                         (sx == sroom->hx && lev->doors[sh].x == sx + 1) ||
                         (sy == sroom->ly && lev->doors[sh].y == sy - 1) ||
                         (sy == sroom->hy && lev->doors[sh].y == sy + 1))))
                continue;
            /* don't place monster on explicitly placed throne */
            if (type == COURT && IS_THRONE(lev->locations[sx][sy].typ))
                continue;
            mon = makemon((type == COURT) ? courtmon(&lev->z, rng) :
                          (type == BARRACKS) ? squadmon(&lev->z) :
                          (type == MORGUE) ? morguemon(&lev->z, rng) :
                          (type == BEEHIVE) ? (sx == tx && sy == ty ?
                                               &mons[PM_QUEEN_BEE] :
                                               &mons[PM_KILLER_BEE]) :
                          (type == LEPREHALL) ? &mons[PM_LEPRECHAUN] :
                          (type == COCKNEST) ? &mons[PM_COCKATRICE] :
                          (type == ANTHOLE) ? antholemon(&lev->z) :
                          NULL, lev, sx, sy,
                          rng == rng_main ? NO_MM_FLAGS : MM_ALLLEVRNG);
            if (mon) {
                mon->msleeping = 1;
                if (type == COURT && mon->mpeaceful)
                    msethostility(mon, TRUE, TRUE);
            }
            switch (type) {
            case ZOO:
            case LEPREHALL:
                if (sroom->doorct) {
                    int distval =
                        dist2(sx, sy, lev->doors[sh].x, lev->doors[sh].y);
                    i = sq(distval);
                } else
                    i = goldlim;
                if (i >= goldlim)
                    i = 5 * level_difficulty(&lev->z);
                goldlim -= i;
                mkgold(10 + rn2_on_rng(i, rng), lev, sx, sy, rng);
                break;
            case MORGUE:
                if (!rn2_on_rng(5, rng))
                    mk_tt_object(lev, CORPSE, sx, sy);
                if (!rn2_on_rng(10, rng))   /* lots of treasure */
                    mksobj_at(rn2_on_rng(3, rng) ? LARGE_BOX : CHEST,
                              lev, sx, sy, TRUE, FALSE, rng);
                if (!rn2_on_rng(5, rng))
                    make_grave(lev, sx, sy, NULL);
                break;
            case BEEHIVE:
                if (!rn2_on_rng(3, rng))
                    mksobj_at(LUMP_OF_ROYAL_JELLY, lev, sx, sy,
                              TRUE, FALSE, rng);
                break;
            case BARRACKS:
                if (!rn2_on_rng(20, rng))   /* the payroll and some loot */
                    mksobj_at((rn2(3)) ? LARGE_BOX : CHEST, lev, sx, sy,
                              TRUE, FALSE, rng);
                break;
            case COCKNEST:
                if (!rn2_on_rng(3, rng)) {
                    struct obj *sobj = mk_tt_object(lev, STATUE, sx, sy);

                    if (sobj) {
                        for (i = rn2_on_rng(5, rng); i; i--)
                            add_to_container(sobj, mkobj(lev, RANDOM_CLASS,
                                                         FALSE, rng));
                        sobj->owt = weight(sobj);
                    }
                }
                break;
            case ANTHOLE:
                if (!rn2_on_rng(3, rng))
                    mkobj_at(FOOD_CLASS, lev, sx, sy, FALSE, rng);
                break;
            }
        }

    if (type == COURT) {
        struct obj *chest;

        lev->locations[tx][ty].typ = THRONE;
        somexy(lev, sroom, &mm, rng);
        mkgold(10 + rn2_on_rng(50 * level_difficulty(&lev->z), rng),
               lev, mm.x, mm.y, rng);
        /* the royal coffers */
        chest = mksobj_at(CHEST, lev, mm.x, mm.y, TRUE, FALSE, rng);
        chest->spe = 2;     /* so it can be found later */
    }
}

/* make a swarm of undead around mm; uses the main RNG */
void
mkundead(struct level *lev, coord *mm, boolean revive_corpses, int mm_flags)
{
    int cnt = (level_difficulty(&lev->z) + 1) / 10 + rnd(5);
    const struct permonst *mdat;
    struct obj *otmp;
    coord cc;

    while (cnt--) {
        mdat = morguemon(&lev->z, rng_main);
        if (enexto(&cc, lev, mm->x, mm->y, mdat) &&
            (!revive_corpses || !(otmp = sobj_at(CORPSE, lev, cc.x, cc.y)) ||
             !revive(otmp)))
            makemon(mdat, lev, cc.x, cc.y, mm_flags);
    }
    lev->flags.graveyard = TRUE;        /* reduced chance for undead corpse */
}

static const struct permonst *
morguemon(const d_level *dlev, enum rng rng)
{
    int i = rn2_on_rng(100, rng);
    int hd = rn2_on_rng(level_difficulty(dlev), rng);

    if (hd > 10 && i < 10) {
        if (In_hell(dlev) || In_endgame(dlev))
            return mkclass(dlev, S_DEMON, 0, rng);
        else {
            int mnum = ndemon(dlev, A_NONE);
            if (mnum != NON_PM)
                return &mons[mnum];
            /* otherwise fall through */
        }
    } else if (hd > 8 && i > 85)
        return mkclass(dlev, S_VAMPIRE, 0, rng);

    return (i < 20) ? &mons[PM_GHOST] :
        (i < 40) ? &mons[PM_WRAITH] : mkclass(dlev, S_ZOMBIE, 0, rng);
}

const struct permonst *
antholemon(const d_level * dlev)
{
    int mtyp;

    /* Same monsters within a level, different ones between levels */
    switch (level_difficulty(dlev) % 3) {
    default:
        mtyp = PM_GIANT_ANT;
        break;
    case 0:
        mtyp = PM_SOLDIER_ANT;
        break;
    case 1:
        mtyp = PM_FIRE_ANT;
        break;
    }
    return (mvitals[mtyp].mvflags & G_GONE) ? NULL : &mons[mtyp];
}

static void
mkswamp(struct level *lev)
{
    struct mkroom *sroom;
    int sx, sy, i, eelct = 0;

    enum rng rng = rng_for_level(&lev->z);

    for (i = 0; i < 5; i++) {   /* turn up to 5 rooms swampy */
        sroom = &lev->rooms[rn2_on_rng(lev->nroom, rng)];
        if (sroom->hx < 0 || sroom->rtype != OROOM || has_upstairs(lev, sroom)
            || has_dnstairs(lev, sroom))
            continue;

        /* satisfied; make a swamp */
        sroom->rtype = SWAMP;
        for (sx = sroom->lx; sx <= sroom->hx; sx++)
            for (sy = sroom->ly; sy <= sroom->hy; sy++)
                if (!OBJ_AT_LEV(lev, sx, sy) && !MON_AT(lev, sx, sy) &&
                    !t_at(lev, sx, sy) && !nexttodoor(lev, sx, sy)) {
                    if ((sx + sy) % 2) {
                        lev->locations[sx][sy].typ = POOL;
                        if (!eelct || !rn2_on_rng(4, rng)) {
                            /* mkclass() won't do, as we might get kraken */
                            makemon(rn2_on_rng(5, rng) ? &mons[PM_GIANT_EEL]
                                    : rn2_on_rng(2, rng) ? &mons[PM_PIRANHA]
                                    : &mons[PM_ELECTRIC_EEL], lev, sx, sy,
                                    MM_ALLLEVRNG);
                            eelct++;
                        }
                    } else if (!rn2_on_rng(4, rng)) /* swamps tend to be moldy */
                        makemon(mkclass(&lev->z, S_FUNGUS, 0, rng),
                                lev, sx, sy, MM_ALLLEVRNG);
                }
    }
}

static coord *
shrine_pos(struct level *lev, int roomno)
{
    static coord buf;
    struct mkroom *troom = &lev->rooms[roomno - ROOMOFFSET];

    buf.x = troom->lx + ((troom->hx - troom->lx) / 2);
    buf.y = troom->ly + ((troom->hy - troom->ly) / 2);
    return &buf;
}

static void
mktemple(struct level *lev)
{
    struct mkroom *sroom;
    coord *shrine_spot;
    struct rm *loc;

    if (!(sroom = pick_room(lev, TRUE, rng_for_level(&lev->z))))
        return;

    /* set up Priest and shrine */
    sroom->rtype = TEMPLE;
    /* 
     * In temples, shrines are blessed altars
     * located in the center of the room
     */
    shrine_spot = shrine_pos(lev, (sroom - lev->rooms) + ROOMOFFSET);
    loc = &lev->locations[shrine_spot->x][shrine_spot->y];
    loc->typ = ALTAR;
    loc->altarmask = induced_align(&lev->z, 80, rng_for_level(&lev->z));
    priestini(lev, sroom, shrine_spot->x, shrine_spot->y, FALSE);
    loc->altarmask |= AM_SHRINE;
}

boolean
nexttodoor(struct level * lev, int sx, int sy)
{
    int dx, dy;
    struct rm *loc;

    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++) {
            if (!isok(sx + dx, sy + dy))
                continue;
            if (IS_DOOR((loc = &lev->locations[sx + dx][sy + dy])->typ) ||
                loc->typ == SDOOR)
                return TRUE;
        }
    return FALSE;
}

boolean
has_dnstairs(struct level *lev, struct mkroom *sroom)
{
    if (sroom == lev->dnstairs_room)
        return TRUE;
    if (isok(lev->sstairs.sx, lev->sstairs.sy) && !lev->sstairs.up)
        return (boolean) (sroom == lev->sstairs_room);
    return FALSE;
}

boolean
has_upstairs(struct level *lev, struct mkroom *sroom)
{
    if (sroom == lev->upstairs_room)
        return TRUE;
    if (isok(lev->sstairs.sx, lev->sstairs.sy) && lev->sstairs.up)
        return (boolean) (sroom == lev->sstairs_room);
    return FALSE;
}


int
somex(struct mkroom *croom, enum rng rng)
{
    return rn2_on_rng(croom->hx - croom->lx + 1, rng) + croom->lx;
}

int
somey(struct mkroom *croom, enum rng rng)
{
    return rn2_on_rng(croom->hy - croom->ly + 1, rng) + croom->ly;
}

boolean
inside_room(struct mkroom *croom, xchar x, xchar y)
{
    return ((boolean)
            (x >= croom->lx - 1 && x <= croom->hx + 1 && y >= croom->ly - 1 &&
             y <= croom->hy + 1));
}

boolean
somexy(struct level *lev, struct mkroom *croom, coord *c, enum rng rng)
{
    int try_cnt = 0;
    int i;

    if (croom->irregular) {
        i = (croom - lev->rooms) + ROOMOFFSET;

        while (try_cnt++ < 100) {
            c->x = somex(croom, rng);
            c->y = somey(croom, rng);
            if (!lev->locations[c->x][c->y].edge &&
                (int)lev->locations[c->x][c->y].roomno == i)
                return TRUE;
        }
        /* try harder; exhaustively search until one is found */
        for (c->x = croom->lx; c->x <= croom->hx; c->x++)
            for (c->y = croom->ly; c->y <= croom->hy; c->y++)
                if (!lev->locations[c->x][c->y].edge &&
                    (int)lev->locations[c->x][c->y].roomno == i)
                    return TRUE;
        return FALSE;
    }

    if (!croom->nsubrooms) {
        c->x = somex(croom, rng);
        c->y = somey(croom, rng);
        return TRUE;
    }

    /* Check that coords doesn't fall into a subroom or into a wall */

    while (try_cnt++ < 100) {
        c->x = somex(croom, rng);
        c->y = somey(croom, rng);
        if (IS_WALL(lev->locations[c->x][c->y].typ))
            continue;
        for (i = 0; i < croom->nsubrooms; i++)
            if (inside_room(croom->sbrooms[i], c->x, c->y))
                goto you_lose;
        break;
    you_lose:;
    }
    if (try_cnt >= 100)
        return FALSE;
    return TRUE;
}

/*
 * Search for a special room given its type (zoo, court, etc...)
 * Special values :
 *     - ANY_SHOP
 *     - ANY_TYPE
 */

struct mkroom *
search_special(struct level *lev, schar type)
{
    struct mkroom *croom;

    for (croom = &lev->rooms[0]; croom->hx >= 0; croom++)
        if ((type == ANY_TYPE && croom->rtype != OROOM) ||
            (type == ANY_SHOP && croom->rtype >= SHOPBASE) ||
            croom->rtype == type)
            return croom;
    for (croom = &lev->subrooms[0]; croom->hx >= 0; croom++)
        if ((type == ANY_TYPE && croom->rtype != OROOM) ||
            (type == ANY_SHOP && croom->rtype >= SHOPBASE) ||
            croom->rtype == type)
            return croom;
    return NULL;
}


const struct permonst *
courtmon(const d_level *dlev, enum rng rng)
{
    int i = rn2_on_rng(60, rng) + rn2_on_rng(3 * level_difficulty(dlev), rng);

    if (i > 100)
        return mkclass(dlev, S_DRAGON, 0, rng);
    else if (i > 95)
        return mkclass(dlev, S_GIANT, 0, rng);
    else if (i > 85)
        return mkclass(dlev, S_TROLL, 0, rng);
    else if (i > 75)
        return mkclass(dlev, S_CENTAUR, 0, rng);
    else if (i > 60)
        return mkclass(dlev, S_ORC, 0, rng);
    else if (i > 45)
        return &mons[PM_BUGBEAR];
    else if (i > 30)
        return &mons[PM_HOBGOBLIN];
    else if (i > 15)
        return mkclass(dlev, S_GNOME, 0, rng);
    else
        return mkclass(dlev, S_KOBOLD, 0, rng);
}

#define NSTYPES (PM_CAPTAIN - PM_SOLDIER + 1)

static const struct {
    unsigned pm;
    unsigned prob;
} squadprob[NSTYPES] = {
    {
    PM_SOLDIER, 80}, {
    PM_SERGEANT, 15}, {
    PM_LIEUTENANT, 4}, {
    PM_CAPTAIN, 1}
};

static const struct permonst *
squadmon(const d_level *dlev)
{       /* return soldier types. */
    int sel_prob, i, cpro, mndx;

    sel_prob = 1 + rn2_on_rng(80 + level_difficulty(dlev), rng_for_level(dlev));

    cpro = 0;
    for (i = 0; i < NSTYPES; i++) {
        cpro += squadprob[i].prob;
        if (cpro > sel_prob) {
            mndx = squadprob[i].pm;
            goto gotone;
        }
    }
    mndx = squadprob[rn2_on_rng(NSTYPES, rng_for_level(dlev))].pm;
gotone:
    if (!(mvitals[mndx].mvflags & G_GONE))
        return &mons[mndx];
    else
        return NULL;
}

/*
 * save_room : A recursive function that saves a room and its subrooms
 * (if any).
 */
static void
save_room(struct memfile *mf, struct mkroom *r)
{
    short i;

    /* no tag; we tag room saving once per level, because the rooms don't
       change in number once the level is created */
    mwrite8(mf, r->lx);
    mwrite8(mf, r->hx);
    mwrite8(mf, r->ly);
    mwrite8(mf, r->hy);
    mwrite8(mf, r->rtype);
    mwrite8(mf, r->rlit);
    mwrite8(mf, r->doorct);
    mwrite8(mf, r->fdoor);
    mwrite8(mf, r->nsubrooms);
    mwrite8(mf, r->irregular);

    for (i = 0; i < r->nsubrooms; i++)
        save_room(mf, r->sbrooms[i]);
}

/*
 * save_rooms : Save all the rooms on disk!
 */
void
save_rooms(struct memfile *mf, struct level *lev)
{
    short i;

    mfmagic_set(mf, ROOMS_MAGIC);       /* "RDAT" */
    mtag(mf, ledger_no(&lev->z), MTAG_ROOMS);
    /* First, write the number of rooms */
    mwrite32(mf, lev->nroom);
    for (i = 0; i < lev->nroom; i++)
        save_room(mf, &lev->rooms[i]);
}

static void
rest_room(struct memfile *mf, struct level *lev, struct mkroom *r)
{
    short i;

    r->lx = mread8(mf);
    r->hx = mread8(mf);
    r->ly = mread8(mf);
    r->hy = mread8(mf);
    r->rtype = mread8(mf);
    r->rlit = mread8(mf);
    r->doorct = mread8(mf);
    r->fdoor = mread8(mf);
    r->nsubrooms = mread8(mf);
    r->irregular = mread8(mf);

    for (i = 0; i < r->nsubrooms; i++) {
        r->sbrooms[i] = &lev->subrooms[lev->nsubroom];
        rest_room(mf, lev, &lev->subrooms[lev->nsubroom]);
        lev->subrooms[lev->nsubroom++].resident = NULL;
    }
}

/*
 * rest_rooms : That's for restoring rooms. Read the rooms structure from
 * the disk.
 */
void
rest_rooms(struct memfile *mf, struct level *lev)
{
    short i;

    mfmagic_check(mf, ROOMS_MAGIC);     /* "RDAT" */
    lev->nroom = mread32(mf);
    lev->nsubroom = 0;
    for (i = 0; i < lev->nroom; i++) {
        rest_room(mf, lev, &lev->rooms[i]);
        lev->rooms[i].resident = NULL;
    }
    lev->rooms[lev->nroom].hx = -1;     /* restore ending flags */
    lev->subrooms[lev->nsubroom].hx = -1;
}

/*mkroom.c*/

