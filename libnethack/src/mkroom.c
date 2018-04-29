/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-04-29 */
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
static struct mkroom *mktemple(struct level *lev);
static void mkseminary(struct level *lev);
static void mksubmerged(struct level *lev);
static void mkstatuary(struct level *lev);
static coord *shrine_pos(struct level *lev, int roomno);
static const struct permonst *morguemon(const d_level *dlev, enum rng rng);
static const struct permonst *squadmon(const d_level *dlev);
static const struct permonst *zoomon(const d_level *dlev, enum rng rng);
static const struct permonst *demondenmon(const d_level *dlev, enum rng rng);
static const struct permonst *abbatoirmon(const d_level *dlev, enum rng rng);
static void save_room(struct memfile *mf, struct mkroom *);
static void rest_room(struct memfile *mf, struct level *lev, struct mkroom *r);
static boolean has_dnstairs(struct level *lev, struct mkroom *);
static boolean has_upstairs(struct level *lev, struct mkroom *);

#define sq(x) ((x)*(x))

/* Return TRUE if a room's rectangular floor area is larger than 20 */
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
    if (roomtype == OROOM)
        return; /* not actually trying to make a special room */

    if (roomtype >= SHOPBASE)
        mkshop(lev); /* someday, we should be able to specify shop type */
    else
        switch (roomtype) {
        case COURT:
        case ZOO:
        case MORGUE:
            /* uses level rng */
            mkzoo(lev, roomtype, rng_for_level(&lev->z));
            break;
        case BEEHIVE:
        case BARRACKS:
        case LEPREHALL:
        case COCKNEST:
        case ANTHOLE:
        case DEMONDEN:
        case LAVAROOM:
        case ABBATOIR:
            /* uses standard rng */
            mkzoo(lev, roomtype, rng_main);
            break;
        case SWAMP:
            mkswamp(lev);
            break;
        case TEMPLE:
            mktemple(lev);
            break;
        case SEMINARY:
            mkseminary(lev);
            break;
        case SUBMERGED:
            mksubmerged(lev);
            break;
        case STATUARY:
            mkstatuary(lev);
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

/* Select a room on the level that is suitable to place a special room in.
   The room must be of ordinary type, and cannot contain the upstairs.
   It cannot contain the downstairs either, unless strict is FALSE, in which
   case it may allow it with 1/3 chance.
   Rooms with exactly one door are heavily preferred; there is only a 1/5
   chance of selecting a room with more doors than that. */
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

/* Try to find a suitable room for a zoo of the given type and, if one can be
   found, set its room type and call fill_zoo to stock it. */
static void
mkzoo(struct level *lev, int type, enum rng rng)
{
    struct mkroom *sroom;

    if ((sroom = pick_room(lev, (type == LAVAROOM), rng)) != 0) {
        sroom->rtype = type;
        fill_zoo(lev, sroom, rng);
    }
}

/* Populate one of the zoo-type rooms with monsters, objects, and possibly
   dungeon features. Also set any appropriate level flags for level sound
   purposes.
   Currently, all of these involve placing a monster on every square of the
   room, whereas objects may or may not be. */
void
fill_zoo(struct level *lev, struct mkroom *sroom, enum rng rng)
{
    struct monst *mon = NULL;
    int sx, sy, i;
    int sh, tx, ty, goldlim, type = sroom->rtype;
    int rmno = (sroom - lev->rooms) + ROOMOFFSET;
    coord mm;

    tx = ty = goldlim = 0;

    sh = sroom->fdoor;
    switch (type) {
    case COURT:
        /* Did a special level hardcode the throne in a given spot? */
        if (lev->flags.is_maze_lev) {
            for (tx = sroom->lx; tx <= sroom->hx; tx++)
                for (ty = sroom->ly; ty <= sroom->hy; ty++)
                    if (IS_THRONE(lev->locations[tx][ty].typ))
                        goto throne_placed;
        }
        /* If not, pick a random spot. */
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
    /* fill room with monsters */
    for (sx = sroom->lx; sx <= sroom->hx; sx++)
        for (sy = sroom->ly; sy <= sroom->hy; sy++) {
            /* Don't fill the square right next to the door, or any of the ones
               along the same wall as the door if the room is rectangular. */
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

            /* create the appropriate room filler monster */
            mon = NULL;
            const struct permonst *pm =
                ((type == ZOO) ? zoomon(&lev->z, rng) :
                 (type == COURT) ? courtmon(&lev->z, rng) :
                 (type == BARRACKS) ? squadmon(&lev->z) :
                 (type == MORGUE) ? morguemon(&lev->z, rng) :
                 (type == BEEHIVE) ? (sx == tx && sy == ty ?
                                      &mons[PM_QUEEN_BEE] :
                                      &mons[PM_KILLER_BEE]) :
                 (type == LEPREHALL) ? &mons[PM_LEPRECHAUN] :
                 (type == COCKNEST) ? &mons[PM_COCKATRICE] :
                 (type == ANTHOLE) ? antholemon(&lev->z) :
                 (type == DEMONDEN) ? demondenmon(&lev->z, rng) :
                 (type == ABBATOIR) ? abbatoirmon(&lev->z, rng) :
                 (type == LAVAROOM &&
                  !rn2_on_rng(5, rng)) ? &mons[PM_SALAMANDER] : NULL);
            if (pm)
                mon = makemon(pm, lev, sx, sy,
                              rng == rng_main ? NO_MM_FLAGS : MM_ALLLEVRNG);

            /* All special rooms currently generate all their monsters
               asleep. */
            if (mon) {
                mon->msleeping = 1;
                if (type == COURT && mon->mpeaceful) {
                    /* Courts are also always hostile. */
                    sethostility(mon, TRUE, TRUE);
                }
            }
            switch (type) {
            case ZOO:
            case LEPREHALL:
                /* place floor gold */
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
                /* corpses and chests and headstones */
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
            case DEMONDEN:
                if (mon) {
                    if (!rn2_on_rng(3, rng)) {
                        /* undo sleep */
                        mon->msleeping = FALSE;
                    }
                    /* treasure */
                    mkgold(rn2_on_rng(200, rng) + 10, lev, sx, sy, rng);
                    for (i = rn2_on_rng(3, rng) + 1; i; i--)
                        mkobj_at(rn2_on_rng(2, rng) ? RANDOM_CLASS : GEM_CLASS,
                                 lev, sx, sy, TRUE, rng);
                }
                break;
            case LAVAROOM:
                if (rn2_on_rng(2, rng))
                    lev->locations[sx][sy].typ = LAVAPOOL;
                break;
            case ABBATOIR:
                /* scatter some corpses, leashes, knives, blood */
                if (mon)
                    mon->msleeping = 0;

                if (!rn2_on_rng(7, rng)) {
                    struct obj* sobj = mksobj_at(CORPSE, lev, sx, sy,
                                                 TRUE, FALSE, rng);
                    sobj->corpsenm = monsndx(zoomon(&lev->z, rng));
                }
                if (!rn2_on_rng(10, rng))
                    mksobj_at(LEASH, lev, sx, sy, FALSE, FALSE, rng);

                if (!rn2_on_rng(6, rng))
                    mksobj_at(KNIFE, lev, sx, sy, TRUE, FALSE, rng);

                if (!rn2(2)) {
                    const char* bloodstains[] =
                         { "/", "-", "\\", ".", "," ":" };
                    make_engr_at(lev, sx, sy,
                                 bloodstains[rn2(SIZE(bloodstains))],
                                 0, ENGR_BLOOD);
                }
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

/* Make a swarm of undead around mm.
   Why is this in mkroom.c? It's only used when using cursed invocation
   items. */
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

/* Return an appropriate undead monster type for generating in graveyards or
   when raising the dead. */
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

/* Return an appropriate ant monster type for an anthole.
   This is deterministic, so that all ants on the same level (practically
   speaking, in a single anthole) are the same type of ant. */
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

/* Pick random zoo-like monsters. */
static const struct permonst *
zoomon(const d_level * dlev, enum rng rng)
{
    const struct permonst *pm;
    int i;
    for (i = 0; i < 100; i++) {
        /* try to re-roll until finding an animal */
        pm = rndmonst(dlev, rng);
        if (is_animal(pm))
            break;
    }
    return pm;
}

/* Pick random zoo-like monsters. */
static const struct permonst *
demondenmon(const d_level * dlev, enum rng rng)
{
    if (rn2_on_rng(4, rng))
        return NULL;

    if (rn2_on_rng(8, rng))
        return mkclass(dlev, S_DEMON, 0, rng);
    return mkclass(dlev, S_IMP, 0, rng);
}

static const struct permonst *
abbatoirmon(const d_level * dlev, enum rng rng)
{
    if (rn2_on_rng(10, rng))
        return NULL;

    if (rn2_on_rng(6, rng))
        return &mons[PM_MARILITH]; /* butcher */
    return zoomon(dlev, rng); /* mon for slaughter */
}

/* Turn up to 5 ordinary rooms into swamp rooms.
   Swamps contain a checkerboard pattern of pools (except next to doors),
   F-class monsters, and possibly one sea monster, apiece. */
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

/* Return the position within a room at which its altar should be placed, if it
   is to be a temple. It will be the exact center of the room, unless the center
   isn't actually a square, in which case it'll be offset one space to the
   side. */
static coord *
shrine_pos(struct level *lev, int roomno)
{
    static coord buf;
    struct mkroom *troom = &lev->rooms[roomno - ROOMOFFSET];

    buf.x = troom->lx + ((troom->hx - troom->lx) / 2);
    buf.y = troom->ly + ((troom->hy - troom->ly) / 2);
    return &buf;
}

/* Try and find a suitable room for a temple and if successful, create the
   temple with its altar and attendant priest. */
static struct mkroom *
mktemple(struct level *lev)
{
    struct mkroom *sroom;
    coord *shrine_spot;
    struct rm *loc;

    if (!(sroom = pick_room(lev, TRUE, rng_for_level(&lev->z))))
        return NULL;

    /* set up Priest and shrine */
    sroom->rtype = TEMPLE;
    /* 
     * In temples, shrines are blessed altars
     * located in the center of the room
     */
    shrine_spot = shrine_pos(lev, (sroom - lev->rooms) + ROOMOFFSET);
    loc = &lev->locations[shrine_spot->x][shrine_spot->y];
    loc->typ = ALTAR;
    loc->flags = (In_hell(&lev->z) ? AM_NONE :
                  induced_align(&lev->z, 80, rng_for_level(&lev->z)));
    priestini(lev, sroom, shrine_spot->x, shrine_spot->y, FALSE);
    loc->flags |= AM_SHRINE;
    return sroom;
}

/* Create a seminary - a temple containing the usual peaceful priest and some
   roaming priests of the same god. */
static void
mkseminary(struct level *lev)
{
    enum rng rng = rng_for_level(&lev->z);
    struct mkroom *sroom = mktemple(lev);
    int i;
    xchar x, y;
    coord *ss; /* shrine spot */

    if (!sroom) /* temple creation failed */
        return;

    /* get altar alignment, roaming priests should have the same */
    ss = shrine_pos(lev, ((sroom - lev->rooms) + ROOMOFFSET));
    if (lev->locations[ss->x][ss->y].typ != ALTAR) {
        impossible("mkseminary: altar not present?");
        return;
    }
    sroom->rtype = SEMINARY;

    aligntyp altaralign =
        Amask2align(lev->locations[ss->x][ss->y].flags & AM_MASK);

    for (i = rn2_on_rng(4, rng) + 1; i; --i) {
        x = somex(sroom, rng);
        y = somey(sroom, rng);
        if (MON_AT(lev, x, y)) {
            i++;
            continue;
        }
        /* peaceful if they're of your alignment */
        mk_roamer(&mons[PM_ALIGNED_PRIEST], altaralign, lev, x, y,
                  (u.ualign.type == altaralign), MM_ALLLEVRNG);
    }
}

/* Create a submerged room - filled entirely with water, populated with sea
   monsters and kelp and hidden treasure. */
static void
mksubmerged(struct level *lev)
{
    enum rng rng = rng_for_level(&lev->z);
    struct mkroom *sroom;
    struct obj *chest, *obj;
    xchar x, y;

    if (!(sroom = pick_room(lev, TRUE, rng)))
        return;

    sroom->rtype = SUBMERGED;

    for (x = sroom->lx; x <= sroom->hx; x++) {
        for (y = sroom->ly; y <= sroom->hy; y++) {
            lev->locations[x][y].typ = MOAT;
            if (!rn2(4)) {
                makemon(!rn2_on_rng(8, rng) ? &mons[PM_KRAKEN] :
                        !rn2_on_rng(4, rng) ? &mons[PM_PIRANHA] :
                        !rn2_on_rng(3, rng) ? &mons[PM_SHARK] :
                        !rn2_on_rng(2, rng) ? &mons[PM_ELECTRIC_EEL] :
                        &mons[PM_GIANT_EEL], lev, x, y, MM_ALLLEVRNG);
            }
            if (!rn2(20)) {
                mksobj_at(KELP_FROND, lev, x, y, TRUE, FALSE, rng);
            }
        }
    }
    x = somex(sroom, rng);
    y = somey(sroom, rng);
    chest = mksobj_at(CHEST, lev, x, y, TRUE, FALSE, rng);
    obj = mksobj(lev, GOLD_PIECE, TRUE, FALSE, rng);
    obj->quan = rn2_on_rng(1000, rng) + 1000;
    add_to_container(chest, obj);
    for (x = rn2_on_rng(10, rng); x > 0; --x) {
        add_to_container(chest, mkobj(lev, GEM_CLASS, FALSE, rng));
    }
    if (!rn2_on_rng(10, rng)) {
        add_to_container(chest, mksobj(lev, MAGIC_LAMP, TRUE, FALSE, rng));
    }
}

/* Create a statuary room - eerily lined with empty statues of the player */
static void
mkstatuary(struct level *lev)
{
    enum rng rng = rng_for_level(&lev->z);
    struct mkroom *sroom;
    struct obj *statue;
    xchar x, y, width, height;

    if (!(sroom = pick_room(lev, FALSE, rng)))
        return;

    sroom->rtype = STATUARY;

#define MKSTATUE(x, y)                                                  \
    if (!nexttodoor(lev, x, y)) {                                       \
        statue = mkcorpstat(STATUE, NULL, &mons[u.umonster], lev,       \
                            x, y, 0, rng);                              \
        oname(statue, u.uplname);                                       \
    }

    /* pick the longer dimension to place statues */
    width = sroom->hx - sroom->lx;
    height = sroom->hy - sroom->ly;
    if (width > height || (width == height && rn2(2))) {
        for (x = sroom->lx; x <= sroom->hx; x++) {
            MKSTATUE(x, sroom->ly);
            MKSTATUE(x, sroom->hy);
        }
    }
    else {
        for (y = sroom->ly; y <= sroom->hy; y++) {
            MKSTATUE(sroom->lx, y);
            MKSTATUE(sroom->hx, y);
        }
    }

#undef MKSTATUE
}

/* Return TRUE if the given location is next to a door or a secret door in any
   direction. */
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

/* Return TRUE if the given room contains downstairs (regular or branch). */
boolean
has_dnstairs(struct level *lev, struct mkroom *sroom)
{
    if (sroom == lev->dnstairs_room)
        return TRUE;
    if (isok(lev->sstairs.sx, lev->sstairs.sy) && !lev->sstairs.up)
        return (boolean) (sroom == lev->sstairs_room);
    return FALSE;
}

/* Return TRUE if the given room contains upstairs (regular or branch). */
boolean
has_upstairs(struct level *lev, struct mkroom *sroom)
{
    if (sroom == lev->upstairs_room)
        return TRUE;
    if (isok(lev->sstairs.sx, lev->sstairs.sy) && lev->sstairs.up)
        return (boolean) (sroom == lev->sstairs_room);
    return FALSE;
}

/* Return a random x coordinate within the x limits of a room. */
int
somex(struct mkroom *croom, enum rng rng)
{
    return rn2_on_rng(croom->hx - croom->lx + 1, rng) + croom->lx;
}

/* Return a random y coordinate within the y limits of a room. */
int
somey(struct mkroom *croom, enum rng rng)
{
    return rn2_on_rng(croom->hy - croom->ly + 1, rng) + croom->ly;
}

/* Return TRUE if the given position falls within both the x and y limits
   of a room.
   Assumes that the room is rectangular; this probably won't work on irregular
   rooms. Also doesn't check roomno. */
boolean
inside_room(struct mkroom *croom, xchar x, xchar y)
{
    return ((boolean)
            (x >= croom->lx - 1 && x <= croom->hx + 1 && y >= croom->ly - 1 &&
             y <= croom->hy + 1));
}

/* Populate c.x and c.y with some random coordinate inside the given room.
   Return TRUE if it was able to do this successfully, and FALSE if it failed
   for some reason. */
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

/* Return an appropriate monster type for generating in throne rooms. */
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

/* Return an appropriate Yendorian Army monster type for generating in
   barracks. They will generate with the percentage odds given above. */
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
    if (flags.save_revision < 19 && r->rtype >= 14)
        r->rtype += 20;

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
