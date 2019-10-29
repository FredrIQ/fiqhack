/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2019-10-29 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"

/* croom->lx etc are schar (width <= int), so % arith ensures that */
/* conversion of result to int is reasonable */

static void makevtele(struct level *lev);
static void makelevel(struct level *lev);
static void mineralize(struct level *lev);
static boolean bydoor(struct level *lev, xchar, xchar);
static struct mkroom *find_branch_room(struct level *lev, coord *);
static struct mkroom *pos_to_room(struct level *lev, xchar x, xchar y);
static boolean place_niche(struct level *lev, struct mkroom *, int *, int *,
                           int *);
static void makeniche(struct level *lev, int);
static void make_niches(struct level *lev);

static int do_comp(const void *, const void *);

static void dosdoor(struct level *lev, xchar, xchar, struct mkroom *, int);
static void join(struct level *lev, int a, int b, boolean, int *);
static void do_room_or_subroom(struct level *lev, struct mkroom *, int, int,
                               int, int, boolean, schar, boolean, boolean);
static void makerooms(struct level *lev, int *smeq);
static void finddpos(struct level *lev, coord *, xchar, xchar, xchar, xchar);
static void mkinvpos(xchar, xchar, int);
static void mk_knox_portal(struct level *lev, xchar, xchar);

#define create_vault(lev) create_room(lev, -1, -1, 2, 2, -1, -1, \
                                      VAULT, TRUE, NULL)
#define do_vault()        (vault_x != -1)
static xchar vault_x, vault_y;
static boolean made_branch;     /* used only during level creation */

#define mrn2(x) mklev_rn2(x, lev)
#define mrng()  rng_for_level(&lev->z)

/* Compares two room pointers by their x-coordinate. Used as a callback to
   qsort. Args must be (const void *) so that qsort will always be happy. */
static int
do_comp(const void *vx, const void *vy)
{
    const struct mkroom *x, *y;

    x = (const struct mkroom *)vx;
    y = (const struct mkroom *)vy;

    /* sort by x coord first */
    if (x->lx != y->lx)
        return x->lx - y->lx;

    /* sort by ly if lx is equal The additional criterium is necessary to get
       consistent sorting across platforms with different qsort
       implementations. */
    return x->ly - y->ly;
}

/* Find a valid position to place a door within the rectangle bounded by
   (xl, yl, xh, yh), as defined by okdoor(). First, try to pick a single random
   spot, then iterate over the entire area.
   If it can't find any valid places it'll just default to an
   existing door. */
static void
finddpos(struct level *lev, coord * cc, xchar xl, xchar yl, xchar xh, xchar yh)
{
    xchar x, y;

    x = (xl == xh) ? xl : (xl + mrn2(xh - xl + 1));
    y = (yl == yh) ? yl : (yl + mrn2(yh - yl + 1));
    if (okdoor(lev, x, y))
        goto gotit;

    for (x = xl; x <= xh; x++)
        for (y = yl; y <= yh; y++)
            if (okdoor(lev, x, y))
                goto gotit;

    for (x = xl; x <= xh; x++)
        for (y = yl; y <= yh; y++)
            if (IS_DOOR(lev->locations[x][y].typ) ||
                lev->locations[x][y].typ == SDOOR)
                goto gotit;
    /* Cannot find something reasonable -- strange.
       Should this be an impossible()? */
    x = xl;
    y = yh;
gotit:
    cc->x = x;
    cc->y = y;
    return;
}

/* Sort the rooms array, using do_comp as the comparison function. */
void
sort_rooms(struct level *lev)
{
    qsort(lev->rooms, lev->nroom, sizeof (struct mkroom), do_comp);
}

/* Initialize the croom struct and the portion of the level it sits on. This
   must be a regular (rectangular) room.
   lowx, lowy, hix, hiy: the bounding box of the room floor, NOT including its
   walls.
   lit: Whether to light the whole room area.
   rtype: The room type. This directly sets croom->rtype without calling mkroom
   even for special rooms. All randomly generated rooms currently specify
   OROOM, but special levels may want to specify a rtype and leave the room
   unfilled (e.g. an abandoned temple).
   special: If FALSE, this function will initialize the room terrain to be a
   rectangle of floor surrounded by the appropriate walls. If TRUE, it will
   skip this step.
   is_room: Whether this room is a full room. FALSE if it's a subroom.
   Only relevant to wallification and if special = FALSE. */
static void
do_room_or_subroom(struct level *lev, struct mkroom *croom, int lowx, int lowy,
                   int hix, int hiy, boolean lit, schar rtype, boolean special,
                   boolean is_room)
{
    int x, y;
    struct rm *loc;

    /* locations might bump level edges in wall-less rooms */
    /* add/subtract 1 to allow for edge locations */
    if (!lowx)
        lowx++;
    if (!lowy)
        lowy++;
    if (hix >= COLNO - 1)
        hix = COLNO - 2;
    if (hiy >= ROWNO - 1)
        hiy = ROWNO - 2;

    if (lit) {
        for (x = lowx - 1; x <= hix + 1; x++) {
            loc = &lev->locations[x][max(lowy - 1, 0)];
            for (y = lowy - 1; y <= hiy + 1; y++)
                loc++->lit = 1;
        }
        croom->rlit = 1;
    } else
        croom->rlit = 0;

    croom->lx = lowx;
    croom->hx = hix;
    croom->ly = lowy;
    croom->hy = hiy;
    croom->rtype = rtype;
    croom->doorct = 0;
    /* if we're not making a vault, lev->doorindex will still be 0 if we are,
       we'll have problems adding niches to the previous room unless fdoor is
       at least doorindex */
    croom->fdoor = lev->doorindex;
    croom->irregular = FALSE;

    croom->nsubrooms = 0;
    croom->sbrooms[0] = NULL;
    if (!special) {
        for (x = lowx - 1; x <= hix + 1; x++)
            for (y = lowy - 1; y <= hiy + 1; y += (hiy - lowy + 2)) {
                lev->locations[x][y].typ = HWALL;
                lev->locations[x][y].horizontal = 1;
                /* For open/secret doors. */
            }
        for (x = lowx - 1; x <= hix + 1; x += (hix - lowx + 2))
            for (y = lowy; y <= hiy; y++) {
                lev->locations[x][y].typ = VWALL;
                lev->locations[x][y].horizontal = 0;
                /* For open/secret doors. */
            }
        for (x = lowx; x <= hix; x++) {
            loc = &lev->locations[x][lowy];
            for (y = lowy; y <= hiy; y++)
                loc++->typ = ROOM;
        }
        if (is_room) {
            lev->locations[lowx - 1][lowy - 1].typ = TLCORNER;
            lev->locations[hix + 1][lowy - 1].typ = TRCORNER;
            lev->locations[lowx - 1][hiy + 1].typ = BLCORNER;
            lev->locations[hix + 1][hiy + 1].typ = BRCORNER;
        } else {        /* a subroom */
            wallification(lev, lowx - 1, lowy - 1, hix + 1, hiy + 1, FALSE);
        }
    }
}

/* Adds a new room to the map.
   Arguments are the same as do_room_or_subroom(), except is_room is hardcoded
   to TRUE. */
void
add_room(struct level *lev, int lowx, int lowy, int hix, int hiy, boolean lit,
         schar rtype, boolean special)
{
    struct mkroom *croom;

    croom = &lev->rooms[lev->nroom];
    do_room_or_subroom(lev, croom, lowx, lowy, hix, hiy, lit, rtype, special,
                       (boolean) TRUE);
    croom++;
    croom->hx = -1;
    lev->nroom++;
}

/* Adds a new subroom to the map as part of the given room.
   Arguments are again the same as those passed to do_room_or_subroom() with
   is_room hardcoded to FALSE. */
void
add_subroom(struct level *lev, struct mkroom *proom, int lowx, int lowy,
            int hix, int hiy, boolean lit, schar rtype, boolean special)
{
    struct mkroom *croom;

    croom = &lev->subrooms[lev->nsubroom];
    do_room_or_subroom(lev, croom, lowx, lowy, hix, hiy, lit, rtype, special,
                       FALSE);
    proom->sbrooms[proom->nsubrooms++] = croom;
    croom++;
    croom->hx = -1;
    lev->nsubroom++;
}

/* Repeatedly create rooms and place them on the map until we can't create any
   more. */
static void
makerooms(struct level *lev, int *smeq)
{
    boolean tried_vault = FALSE;

    /* rnd_rect() will return 0 if no more rects are available... */
    while (lev->nroom < MAXNROFROOMS && rnd_rect()) {
        /* If a certain number of rooms have already been created, and we have
           not yet tried to make a vault, with 50% probability, try to create
           one. */
        if (lev->nroom >= (MAXNROFROOMS / 6) && mrn2(2) &&
            !tried_vault) {
            tried_vault = TRUE;
            if (create_vault(lev)) {
                /* This won't actually create the room and edit the terrain
                   with add_room. It'll just set the lx and ly of rooms[nroom]
                   to represent its location. */
                vault_x = lev->rooms[lev->nroom].lx;
                vault_y = lev->rooms[lev->nroom].ly;
                lev->rooms[lev->nroom].hx = -1;
            }
        } else {
            /* Try to create another random room. If it can't find anywhere for
               one to go, stop making rooms.
               Use the parameters for a totally random ordinary room. */
            if (!create_room(lev, -1, -1, -1, -1, -1, -1, OROOM, -1, smeq))
                return;
        }
    }
    return;
}

/* Join rooms a and b together by drawing a corridor and placing doors.
   If nxcor is TRUE, it will be pickier about whether to draw the corridor at
   all, and will not create doors in !okdoor() locations.
   The corridor will be made of CORR terrain unless this is an arboreal level
   in which case it will use ROOM.
   Afterwards, the smeq values of a and b will be set equal to each other.
   Should this return boolean (success or failure)? */
static void
join(struct level *lev, int a, int b, boolean nxcor, int *smeq)
{
    coord cc, tt, org, dest;
    xchar tx, ty, xx, yy;
    struct mkroom *croom, *troom;
    int dx, dy;

    croom = &lev->rooms[a];
    troom = &lev->rooms[b];

    /* find positions cc and tt for doors in croom and troom and direction for
       a corridor between them */

    /* if either room is not an actual room (hx = -1), or if too many doors
       exist already, abort */
    if (troom->hx < 0 || croom->hx < 0 || lev->doorindex >= DOORMAX)
        return;

    /* Determine how croom and troom are positioned relative to each other,
       then pick random positions on their walls that face each other where
       doors will be created.
       Note: This has a horizontal bias; if troom is, for instance, both to the
       right of and below croom, the ordering of the if clauses here will
       always place the doors on croom's right wall and troom's left wall.
       This may be intentional, since the playing field is much wider than it
       is tall. */
    if (troom->lx > croom->hx) {
        /* troom to the right of croom */
        dx = 1;
        dy = 0;
        xx = croom->hx + 1;
        tx = troom->lx - 1;
        finddpos(lev, &cc, xx, croom->ly, xx, croom->hy);
        finddpos(lev, &tt, tx, troom->ly, tx, troom->hy);
    } else if (troom->hy < croom->ly) {
        /* troom below croom */
        dy = -1;
        dx = 0;
        yy = croom->ly - 1;
        finddpos(lev, &cc, croom->lx, yy, croom->hx, yy);
        ty = troom->hy + 1;
        finddpos(lev, &tt, troom->lx, ty, troom->hx, ty);
    } else if (troom->hx < croom->lx) {
        /* troom to the left of croom */
        dx = -1;
        dy = 0;
        xx = croom->lx - 1;
        tx = troom->hx + 1;
        finddpos(lev, &cc, xx, croom->ly, xx, croom->hy);
        finddpos(lev, &tt, tx, troom->ly, tx, troom->hy);
    } else {
        /* otherwise troom must be above croom */
        dy = 1;
        dx = 0;
        yy = croom->hy + 1;
        ty = troom->ly - 1;
        finddpos(lev, &cc, croom->lx, yy, croom->hx, yy);
        finddpos(lev, &tt, troom->lx, ty, troom->hx, ty);
    }
    xx = cc.x;
    yy = cc.y;
    tx = tt.x - dx;
    ty = tt.y - dy;

    /* If nxcor is TRUE and the space outside croom's door isn't stone (maybe
       some previous corridor has already been drawn here?), abort.
       TODO: this check should also be converted to != STONE */
    if (nxcor && lev->locations[xx + dx][yy + dy].typ)
        return;

    /* If we can put a door in croom's wall or nxcor is FALSE, do so. */
    if (okdoor(lev, xx, yy) || !nxcor)
        dodoor(lev, xx, yy, croom);

    /* Attempt to dig the corridor. If it fails for some reason, abort. */
    org.x = xx + dx;
    org.y = yy + dy;
    dest.x = tx;
    dest.y = ty;
    if (!dig_corridor
        (lev, &org, &dest, nxcor, lev->flags.arboreal ? ROOM : CORR, STONE))
        return;

    /* We succeeded in digging the corridor.
       If we can put the door in troom's wall or nxcor is FALSE, do so. */
    if (okdoor(lev, tt.x, tt.y) || !nxcor)
        dodoor(lev, tt.x, tt.y, troom);

    /* Set the smeq values for these rooms to be equal to each other, denoting
       that these two rooms are now part of the same reachable section of the
       level.
       Importantly, this does NOT propagate the smeq value to any other rooms
       with the to-be-overwritten smeq value! */
    if (smeq[a] < smeq[b])
        smeq[b] = smeq[a];
    else
        smeq[a] = smeq[b];
}

/* Generate corridors connecting all the rooms on the level. */
void
makecorridors(struct level *lev, int *smeq)
{
    int a, b, i;
    boolean any = TRUE;

    /* Connect each room to the next room in rooms.

       Since during normal random level generation, rooms is sorted by order of
       x-coordinate prior to calling this function, this first step will,
       unless it hits the !rn2(50), connect each room to the next room to its
       right, which will set everyone's smeq value to the same number. This
       will deny the next two loops in this function from getting to connect
       anything. Occasionally a level will be created by this having a series
       of up-and-down switchbacks, and no other corridors.

       It's rather easy to see all the rooms joined in order from left to right
       across the level if you know what you're looking for. */
    for (a = 0; a < lev->nroom - 1; a++) {
        join(lev, a, a + 1, FALSE, smeq);
        if (!mrn2(50))
            break;      /* allow some randomness */
    }

    /* Connect each room to the room two rooms after it in rooms, if and only
       if they do not have the same smeq already. */
    for (a = 0; a < lev->nroom - 2; a++)
        if (smeq[a] != smeq[a + 2])
            join(lev, a, a + 2, FALSE, smeq);

    /* Connect any remaining rooms with different smeqs. The "any" variable is
       an optimization; if on a given loop no different smeqs were found from
       the current room, there's nothing more to be done. */
    for (a = 0; any && a < lev->nroom; a++) {
        any = FALSE;
        for (b = 0; b < lev->nroom; b++)
            if (smeq[a] != smeq[b]) {
                join(lev, a, b, FALSE, smeq);
                any = TRUE;
            }
    }
    /* By now, all rooms should be guaranteed to be connected. */

    /* Attempt to draw a few more corridors between rooms, but don't draw the
       corridor if it starts on an already carved out corridor space. Possibly
       also don't create the doors. */
    if (lev->nroom > 2)
        for (i = mrn2(lev->nroom) + 4; i; i--) {
            a = mrn2(lev->nroom);
            b = mrn2(lev->nroom - 2);
            if (b >= a)
                b += 2;
            join(lev, a, b, TRUE, smeq);
        }
}

/* Adds a door, not to the level itself, but to the doors array, and updates
   other mkroom structs as necessary.
   x and y are the coordinates of the door, and aroom is the room which is
   getting the door. */
void
add_door(struct level *lev, int x, int y, struct mkroom *aroom)
{
    struct mkroom *broom;
    int tmp;

    aroom->doorct++;
    broom = aroom + 1;
    if (broom->hx < 0)
        tmp = lev->doorindex;
    else
        for (tmp = lev->doorindex; tmp > broom->fdoor; tmp--)
            lev->doors[tmp] = lev->doors[tmp - 1];
    lev->doorindex++;
    lev->doors[tmp].x = x;
    lev->doors[tmp].y = y;
    for (; broom->hx >= 0; broom++)
        broom->fdoor++;
}

/* Create a door or a secret door (using type) in aroom at location (x,y).
   Sets the doormask randomly. Contains the guts of the random probabilities
   that determine what doorstate the door gets, and whether it becomes trapped.

   Doors are never generated broken. Shop doors tend to be generated open, and
   never generate trapped. (They can be locked, though, in which case the shop
   becomes closed for inventory.) Secret doors always generate closed or
   locked. */
static void
dosdoor(struct level *lev, xchar x, xchar y, struct mkroom *aroom, int type)
{
    boolean shdoor = ((*in_rooms(lev, x, y, SHOPBASE)) ? TRUE : FALSE);

    if (!IS_WALL(lev->locations[x][y].typ))     /* avoid SDOORs on already made
                                                   doors */
        type = DOOR;
    lev->locations[x][y].typ = type;
    if (type == DOOR) {
        if (!mrn2(3)) {  /* locked, closed, or doorway? */
            if (!mrn2(5))
                lev->locations[x][y].flags = D_ISOPEN;
            else if (!mrn2(6))
                lev->locations[x][y].flags = D_LOCKED;
            else
                lev->locations[x][y].flags = D_CLOSED;

            if (lev->locations[x][y].flags != D_ISOPEN && !shdoor &&
                level_difficulty(&lev->z) >= 5 && !mrn2(25))
                lev->locations[x][y].flags |= D_TRAPPED;
        } else
            lev->locations[x][y].flags = (shdoor ? D_ISOPEN : D_NODOOR);
        if (lev->locations[x][y].flags & D_TRAPPED) {
            struct monst *mtmp;

            if (level_difficulty(&lev->z) >= 9 && !mrn2(5)) {
                /* attempt to make a mimic instead; no longer conditional on
                   mimics not being genocided; makemon() is on the main RNG
                   because mkclass() won't necessarily always return the same
                   result (again, due to genocide) */
                lev->locations[x][y].flags = D_NODOOR;
                mtmp = makemon(mkclass(&lev->z, S_MIMIC, 0, mrng()),
                               lev, x, y, NO_MM_FLAGS);
                if (mtmp)
                    set_mimic_sym(mtmp, lev, mrng());
            }
        }
        if (level == lev)
            newsym(x, y);
    } else {    /* SDOOR */
        if (shdoor || !mrn2(5))
            lev->locations[x][y].flags = D_LOCKED;
        else
            lev->locations[x][y].flags = D_CLOSED;

        if (!shdoor && level_difficulty(&lev->z) >= 4 && !mrn2(20))
            lev->locations[x][y].flags |= D_TRAPPED;
    }

    add_door(lev, x, y, aroom);
}

/* Determine whether a niche (closet) can be placed on one edge of a room.
   Contrary to the name, this does not actually place a niche; perhaps it
   should be renamed to something more straightforward.
   If the niche can be placed, xx and yy will then contain the coordinate
   for the door, and dy will contain the direction it's supposed to go in (that
   is, the actual niche square is (xx, yy+dy)). */
static boolean
place_niche(struct level *lev, struct mkroom *aroom, int *dy, int *xx, int *yy)
{
    coord dd;

    /* Niches only ever generate on the top and bottom walls of rooms, for some
       reason. Probably because it looks better.
       Horizontal "niches" might still appear from time to time as a result of
       dig_corridor shenanigans, but they're failed corridors, not real niches.
       Look for a suitable spot on one of these walls to place a niche. */
    if (mrn2(2)) {
        *dy = 1;
        finddpos(lev, &dd, aroom->lx, aroom->hy + 1, aroom->hx, aroom->hy + 1);
    } else {
        *dy = -1;
        finddpos(lev, &dd, aroom->lx, aroom->ly - 1, aroom->hx, aroom->ly - 1);
    }
    *xx = dd.x;
    *yy = dd.y;
    /* Spot for the niche must be stone; other spot just inside the room must
       not be water or another dungeon feature.
       Note that there's no checking that the area surrounding the niche is
       also stone; niches can generate touching one or more corridor spaces. */
    return ((boolean)
            ((isok(*xx, *yy + *dy) &&
              lev->locations[*xx][*yy + *dy].typ == STONE)
             && (isok(*xx, *yy - *dy) &&
                 !IS_POOL(lev->locations[*xx][*yy - *dy].typ)
                 && !IS_FURNITURE(lev->locations[*xx][*yy - *dy].typ))));
}

/* there should be one of these per trap, in the same order as trap.h */
static const char *const trap_engravings[TRAPNUM] = {
    NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    /* 14..17: trap door, VS, teleport, level-teleport */
    "Vlad was here", NULL, "ad aerarium", "ad aerarium",
    NULL, NULL, NULL, NULL, NULL,
    NULL,
};

/* Actually create a niche/closet, on a random room. Place a trap on it if
   trap_type != NO_TRAP. */
static void
makeniche(struct level *lev, int trap_type)
{
    struct mkroom *aroom;
    struct rm *rm;
    int vct = 8; /* number of attempts */
    int dy, xx, yy;
    struct trap *ttmp;

    if (lev->doorindex < DOORMAX)
        while (vct--) {
            aroom = &lev->rooms[mrn2(lev->nroom)];
            if (aroom->rtype != OROOM)
                continue; /* don't place niches in special rooms */
            if (aroom->doorct == 1 && mrn2(5))
                continue; /* usually don't place in rooms with 1 door */
            if (!place_niche(lev, aroom, &dy, &xx, &yy))
                continue; /* didn't find a suitable spot */

            rm = &lev->locations[xx][yy + dy];
            if (trap_type || !mrn2(4)) {
                /* all closets with traps and 25% of other closets require some
                   searching */
                rm->typ = SCORR;
                if (trap_type) {
                    /* don't place fallthru traps on undiggable levels */
                    if ((trap_type == HOLE || trap_type == TRAPDOOR)
                        && !can_fall_thru(lev))
                        trap_type = ROCKTRAP;
                    ttmp = maketrap(lev, xx, yy + dy, trap_type, mrng());
                    if (ttmp) {
                        if (trap_type != ROCKTRAP)
                            ttmp->once = 1;
                        /* make the specified engraving in front of the door */
                        if (trap_engravings[trap_type]) {
                            make_engr_at(lev, xx, yy - dy,
                                         trap_engravings[trap_type], 0L, DUST);
                            wipe_engr_at(lev, xx, yy - dy, 5);  /* age it */
                        }
                    }
                }
                /* place the door */
                dosdoor(lev, xx, yy, aroom, SDOOR);
            } else {
                rm->typ = CORR;
                /* 1/7 of these niches are generated inaccessible - no actual
                   connection to their corresponding room */
                if (mrn2(7))
                    dosdoor(lev, xx, yy, aroom,
                            mrn2(5) ? SDOOR : DOOR);
                else {
                    /* Place a teleport scroll here so the player can escape.
                       If an inaccessible niche is generated on a no-tele
                       level, the player shouldn't be able to get into it
                       without some way of getting back out... */
                    if (!lev->flags.noteleport)
                        mksobj_at(SCR_TELEPORTATION, lev, xx, yy + dy, TRUE,
                                  FALSE, mrng());
                    if (!mrn2(3))
                        mkobj_at(0, lev, xx, yy + dy, TRUE, mrng());
                }
            }
            return;
        }
}

/* Try to create several random niches across an entire level.
   Does NOT include the niche for a vault teleporter, if one exists. */
static void
make_niches(struct level *lev)
{
    /* This should really be nroom / 2... */
    int ct = 1 + mrn2((lev->nroom >> 1) + 1), dep = depth(&lev->z);

    boolean ltptr = (!lev->flags.noteleport && dep > 15);
    boolean vamp = (dep > 5 && dep < 25);

    while (ct--) {
        if (ltptr && !mrn2(6)) {
            /* occasional fake vault teleporter */
            ltptr = FALSE;
            makeniche(lev, LEVEL_TELEP);
        } else if (vamp && !mrn2(6)) {
            /* "Vlad was here" trapdoor */
            vamp = FALSE;
            makeniche(lev, TRAPDOOR);
        } else
            makeniche(lev, NO_TRAP); /* regular untrapped niche */
    }
}

/* Create a vault teleporter niche.
   The code seems to assume that any teleport trap inside a niche should always
   go to a vault; this may become problematic if the player ever gains the
   ability to make teleport traps... */
static void
makevtele(struct level *lev)
{
    makeniche(lev, TELEP_TRAP);
}

/* Choose an appropriate special room type for the given level. */
int
rand_roomtype(struct level *lev)
{
    int u_depth = depth(&lev->z);
    /* minimum number of rooms needed to allow a random special room */
    int room_threshold = 1;
    int i;
    for (i = 0; i < lev->nroom; i++) {
        if (&lev->rooms[i] == lev->upstairs_room ||
            &lev->rooms[i] == lev->dnstairs_room ||
            &lev->rooms[i] == lev->sstairs_room)
            room_threshold++;
    }

    if (search_special(lev, VAULT))
        room_threshold++;

    if (!Inhell) {
        if (u_depth > 1 && u_depth < depth(&medusa_level) &&
            lev->nroom >= room_threshold && mrn2(u_depth) < 3)
            return SHOPBASE; /* random shop */
        else if (u_depth > 4 && !mrn2(6))
            return COURT;
        else if (u_depth > 5 && !mrn2(8) &&
                 !(mvitals[PM_LEPRECHAUN].mvflags & G_GONE))
            return LEPREHALL;
        else if (u_depth > 6 && !mrn2(7))
            return ZOO;
        else if (u_depth > 8 && !mrn2(5))
            return TEMPLE;
        else if (u_depth > 9 && !mrn2(5) &&
                 !(mvitals[PM_KILLER_BEE].mvflags & G_GONE))
            return BEEHIVE;
        else if (u_depth > 11 && !mrn2(6))
            return MORGUE;
        else if (u_depth > 12 && !mrn2(8) &&
                 antholemon(&lev->z))
            return ANTHOLE;
        else if (u_depth > 14 && !mrn2(4) &&
                 !(mvitals[PM_SOLDIER].mvflags & G_GONE))
            return BARRACKS;
        else if (u_depth > 15 && !mrn2(6))
            return SWAMP;
        else if (u_depth > 16 && !mrn2(8))
            return COCKNEST;
    } else {
        /* Gehennom random special rooms */
        /* provisionally: depth doesn't really matter too much since none of
           these rooms have a wildly higher difficulty. */
        int chance = mrn2(100);
        if (chance < 38)
            return FAKEWIZ;
        else if (chance < 48)
            return DEMONDEN;
        else if (chance < 63)
            return SUBMERGED;
        else if (chance < 78)
            return LAVAROOM;
        else if (chance < 83)
            return ABBATOIR;
        else if (chance < 93)
            return SEMINARY;
        else if (chance < 98)
            return TEMPLE; /* Moloch temple */
        else
            return STATUARY;
    }

    return OROOM;
}

/* Allocate a new level structure and make sure its fields contain sane
 * initial vales.
 * Some of this is only necessary for some types of levels (maze, normal,
 * special) but it's easier to put it all in one place than make sure
 * each type initializes what it needs to separately.
 */
struct level *
alloc_level(d_level *levnum)
{
    struct level *lev = malloc(sizeof (struct level));

    memset(lev, 0, sizeof (struct level));
    if (levnum)
        lev->z = *levnum;
    lev->subrooms = &lev->rooms[MAXNROFROOMS + 1];      /* compat */
    lev->rooms[0].hx = -1;
    lev->subrooms[0].hx = -1;
    lev->flags.hero_memory = 1;

    lev->updest.lx = lev->updest.hx = lev->updest.nlx = lev->updest.nhx =
        lev->dndest.lx = lev->dndest.hx = lev->dndest.nlx = lev->dndest.nhx =
        lev->upstair.sx = lev->dnstair.sx = lev->sstairs.sx =
        lev->upladder.sx = lev->dnladder.sx = COLNO;
    lev->updest.ly = lev->updest.hy = lev->updest.nly = lev->updest.nhy =
        lev->dndest.ly = lev->dndest.hy = lev->dndest.nly = lev->dndest.nhy =
        lev->upstair.sy = lev->dnstair.sy = lev->sstairs.sy =
        lev->upladder.sy = lev->dnladder.sy = ROWNO;

    /* these are not part of the level structure, but are only used while
       making new levels */
    vault_x = -1;
    made_branch = FALSE;

    return lev;
}

/* Full initialization of all level structures, map, objects, etc.
   Handles any level - special levels will load that special level, Gehennom
   will create mazes, and so on.
   Called only from mklev(). */
static void
makelevel(struct level *lev)
{
    struct mkroom *croom, *troom;
    int tryct;
    int x, y;
    struct monst *tmonst;       /* always put a web with a spider */
    branch *branchp;
    int room_threshold;

    int smeq[MAXNROFROOMS + 1];

    /* this is apparently used to denote that a lot of program state is
       uninitialized */
    if (wiz1_level.dlevel == 0)
        init_dungeons();

    /* FIXME: pointless braces? */
    {
        s_level *slevnum = Is_special(&lev->z);

        /* check for special levels */
        if (slevnum && !Is_rogue_level(&lev->z)) {
            /* special non-Rogue level */
            makemaz(lev, slevnum->proto, smeq);
            return;
        } else if (find_dungeon(&lev->z).proto[0]) {
            /* named prototype file */
            makemaz(lev, "", smeq);
            return;
        } else if (In_mines(&lev->z)) {
            /* mines filler */
            makemaz(lev, "minefill", smeq);
            return;
        } else if (In_quest(&lev->z)) {
            /* quest filler */
            char fillname[9];
            s_level *loc_levnum;

            snprintf(fillname, SIZE(fillname), "%s-loca", urole.filecode);
            loc_levnum = find_level(fillname);

            snprintf(fillname, SIZE(fillname), "%s-fil", urole.filecode);
            strcat(fillname,
                   (lev->z.dlevel < loc_levnum->dlevel.dlevel) ? "a" : "b");
            makemaz(lev, fillname, smeq);
            return;
        } else if (In_hell(&lev->z) ||
                   (mrn2(5) && lev->z.dnum == medusa_level.dnum &&
                    depth(&lev->z) > depth(&medusa_level))) {
            /* Gehennom, or 80% of levels below Medusa - maze filler */
            makemaz(lev, "", smeq);
            return;
        }
    }

    /* otherwise, fall through - it's a "regular" level. */
    if (Is_rogue_level(&lev->z)) {
        /* place rooms and fake bones pile */
        makeroguerooms(lev, smeq);
        makerogueghost(lev);
    } else
        makerooms(lev, smeq); /* regular dungeon fill level */

    /* order rooms[] by x-coordinate */
    sort_rooms(lev);

    /* construct stairs (up and down in different rooms if possible) */
    croom = &lev->rooms[mrn2(lev->nroom)];
    if (!Is_botlevel(&lev->z)) {
        y = somey(croom, mrng());
        x = somex(croom, mrng());
        mkstairs(lev, x, y, 0, croom);  /* down */
    }
    if (lev->nroom > 1) {
        troom = croom;
        croom = &lev->rooms[mrn2(lev->nroom - 1)];
        /* slight bias here for upstairs to be 1 room to the right of the
           downstairs room */
        /* TODO: This looks like the wrong test. */
        if (croom == troom)
            croom++;
    }

    /* now do the upstairs */
    if (lev->z.dlevel != 1) {
        xchar sx, sy;

        do {
            sx = somex(croom, mrng());
            sy = somey(croom, mrng());
        } while (occupied(lev, sx, sy));
        mkstairs(lev, sx, sy, 1, croom);        /* up */
    }

    branchp = Is_branchlev(&lev->z);    /* possible dungeon branch */
    room_threshold = branchp ? 4 : 3;   /* minimum number of rooms needed to
                                           allow a random special room */
    if (Is_rogue_level(&lev->z))
        goto skip0;
    makecorridors(lev, smeq);
    make_niches(lev);

    /* Did makerooms place a 2x2 unconnected room to be a vault? If so, fill
       it. Is there really a reason for do_vault() to be a macro? All it does
       is test whether vault_x is a real coordinate. It's only used here. */
    if (do_vault()) {
        xchar w, h;

        w = 1;
        h = 1;
        /* make sure vault can actually be placed */
        if (check_room(lev, &vault_x, &w, &vault_y, &h, TRUE)) {
        fill_vault:
            add_room(lev, vault_x, vault_y, vault_x + w, vault_y + h, TRUE,
                     VAULT, FALSE);
            ++room_threshold;
            fill_room(lev, &lev->rooms[lev->nroom - 1], FALSE);
            mk_knox_portal(lev, vault_x + w, vault_y + h);
            /* Only put a vault teleporter with 1/3 chance;
               a teleportation trap in a closet is a sure sign that a vault is
               on the level, but a vault is not a sure sign of a vault
               teleporter. */
            if (!lev->flags.noteleport && !mrn2(3))
                makevtele(lev);
        } else if (rnd_rect() && create_vault(lev)) {
            /* If we didn't create a vault already, try once more. */
            vault_x = lev->rooms[lev->nroom].lx;
            vault_y = lev->rooms[lev->nroom].ly;
            if (check_room(lev, &vault_x, &w, &vault_y, &h, TRUE))
                goto fill_vault;
            else
                lev->rooms[lev->nroom].hx = -1;
        }
    }

    /* Try to create one special room on the level.
       The available special rooms depend on how deep you are.
       If a special room is selected and fails to be created (e.g. it tried
       to make a shop and failed because no room had exactly 1 door), it
       won't try to create the other types of available special rooms. */
    mkroom(lev, rand_roomtype(lev));

skip0:
    /* Place multi-dungeon branch. */
    place_branch(lev, branchp, COLNO, ROWNO);

    /* for each room: put things inside */
    for (croom = lev->rooms; croom->hx > 0; croom++) {
        if (croom->rtype != OROOM)
            continue;

        /* put a sleeping monster inside */
        /* Note: monster may be on the stairs. This cannot be avoided: maybe
           the player fell through a trap door while a monster was on the
           stairs. Conclusion: we have to check for monsters on the stairs
           anyway. */

        if (!mrn2(3)) {
            x = somex(croom, mrng());
            y = somey(croom, mrng());
            tmonst = makemon(NULL, lev, x, y, MM_ALLLEVRNG);
            if (tmonst && tmonst->data == &mons[PM_GIANT_SPIDER] &&
                !occupied(lev, x, y))
                maketrap(lev, x, y, WEB, mrng());
        }
        /* put traps and mimics inside */
        x = 8 - (level_difficulty(&lev->z) / 6);
        if (x <= 1)
            x = 2; /* maxes out at level_difficulty() == 36 */
        while (!mrn2(x))
            mktrap(lev, 0, 0, croom, NULL);

        /* maybe put some gold inside */
        if (!mrn2(3)) {
            y = somey(croom, mrng());
            x = somex(croom, mrng());
            mkgold(0L, lev, x, y, mrng());
        }
        if (Is_rogue_level(&lev->z))
            goto skip_nonrogue;

        /* maybe place some dungeon features inside */
        if (!mrn2(10))
            mkfount(lev, 0, croom);
        if (!mrn2(60))
            mksink(lev, croom);
        if (!mrn2(60))
            mkaltar(lev, croom);
        x = 80 - (depth(&lev->z) * 2);
        if (x < 2)
            x = 2;
        if (!mrn2(x))
            mkgrave(lev, croom);

        /* put statues inside */
        if (!mrn2(20)) {
            y = somey(croom, mrng());
            x = somex(croom, mrng());
            mkcorpstat(STATUE, NULL, NULL, lev, x, y, TRUE, mrng());
        }
        /* put box/chest inside; 40% chance for at least 1 box, regardless of
           number of rooms; about 5 - 7.5% for 2 boxes, least likely when few
           rooms; chance for 3 or more is neglible. */
        if (!mrn2(lev->nroom * 5 / 2)) {
            /* Fix: somex and somey should not be called from the arg list for
               mksobj_at(). Arg evaluation order is not standardized and may
               differ between compilers and optimization levels, which breaks
               replays. */
            y = somey(croom, mrng());
            x = somex(croom, mrng());
            mksobj_at((mrn2(3)) ? LARGE_BOX : CHEST, lev, x, y,
                      TRUE, FALSE, mrng());
        }

        /* maybe make some graffiti
           chance decreases the lower you get in the dungeon */
        if (!mrn2(27 + 3 * abs(depth(&lev->z)))) {
            const char *mesg = random_engraving(mrng());

            if (mesg) {
                do {
                    x = somex(croom, mrng());
                    y = somey(croom, mrng());
                } while (lev->locations[x][y].typ != ROOM &&
                         !mrn2(40));
                if (!(IS_POOL(lev->locations[x][y].typ) ||
                      IS_FURNITURE(lev->locations[x][y].typ)))
                    make_engr_at(lev, x, y, mesg, 0L, MARK);
            }
        }

    skip_nonrogue:
        /* place a random object in the room, with a recursive 20% chance of
           placing another */
        if (!mrn2(3)) {
            y = somey(croom, mrng());
            x = somex(croom, mrng());
            mkobj_at(0, lev, x, y, TRUE, mrng());
            tryct = 0;
            while (!mrn2(5)) {
                if (++tryct > 100) {
                    impossible("tryct overflow4");
                    break;
                }
                y = somey(croom, mrng());
                x = somex(croom, mrng());
                mkobj_at(0, lev, x, y, TRUE, mrng());
            }
        }
    }
}

/* Place deposits of minerals (gold and misc gems) in the stone
   surrounding the rooms on the map.
   Also place kelp in water.
   mineralize(-1, -1, -1, -1, FALSE); => "default" behaviour
   The four probability arguments aren't percentages; assuming the spot to
   place the item is suitable, kelp will be placed with 1/prob chance;
   whereas gold and gems will be placed with prob/1000 chance.
   skip_lvl_checks will ignore any checks that items don't get mineralized in
   the wrong levels. This is currently only TRUE if a special level forces it
   to be. */
static void
mineralize(struct level *lev)
{
    s_level *sp;
    struct obj *otmp;
    int goldprob, gemprob, x, y, cnt;


    /* Place kelp, except on the plane of water */
    if (In_endgame(&lev->z))
        return;
    for (x = 1; x < (COLNO - 1); x++)
        for (y = 1; y < (ROWNO - 1); y++)
            if ((lev->locations[x][y].typ == POOL && !mrn2(10)) ||
                (lev->locations[x][y].typ == MOAT && !mrn2(30)))
                mksobj_at(KELP_FROND, lev, x, y, TRUE, FALSE, mrng());

    /* determine if it is even allowed; almost all special levels are excluded
       */
    if (In_hell(&lev->z) || In_V_tower(&lev->z) || Is_rogue_level(&lev->z) ||
        lev->flags.arboreal || ((sp = Is_special(&lev->z)) != 0 &&
                                !Is_oracle_level(&lev->z)
                                && (!In_mines(&lev->z) || sp->flags.town)
        ))
        return;

    /* basic level-related probabilities */
    goldprob = 20 + depth(&lev->z) / 3;
    gemprob = goldprob / 4;

    /* mines have ***MORE*** goodies - otherwise why mine? */
    if (In_mines(&lev->z)) {
        goldprob *= 2;
        gemprob *= 3;
    } else if (In_quest(&lev->z)) {
        goldprob /= 4;
        gemprob /= 6;
    }

    /*
     * Seed rock areas with gold and/or gems.
     * We use fairly low level object handling to avoid unnecessary
     * overhead from placing things in the floor chain prior to burial.
     */
    for (x = 1; x < (COLNO - 1); x++)
        for (y = 1; y < (ROWNO - 1); y++)
            if (lev->locations[x][y + 1].typ != STONE) {
                /* <x,y> spot not eligible */
                y += 2; /* next two spots aren't eligible either */
            } else if (lev->locations[x][y].typ != STONE) {
                /* this spot not eligible */
                y += 1; /* next spot isn't eligible either */
            } else if (!(lev->locations[x][y].flags & W_NONDIGGABLE) &&
                       lev->locations[x][y - 1].typ == STONE &&
                       lev->locations[x + 1][y - 1].typ == STONE &&
                       lev->locations[x - 1][y - 1].typ == STONE &&
                       lev->locations[x + 1][y].typ == STONE &&
                       lev->locations[x - 1][y].typ == STONE &&
                       lev->locations[x + 1][y + 1].typ == STONE &&
                       lev->locations[x - 1][y + 1].typ == STONE) {
                if (mrn2(1000) < goldprob) {
                    if ((otmp = mksobj(lev, GOLD_PIECE,
                                       FALSE, FALSE, mrng()))) {
                        otmp->ox = x, otmp->oy = y;
                        otmp->quan = 2L + mrn2(goldprob * 3);
                        otmp->owt = weight(otmp);
                        if (!mrn2(3))
                            add_to_buried(otmp);
                        else
                            place_object(otmp, lev, x, y);
                    }
                }
                if (mrn2(1000) < gemprob) {
                    for (cnt = 1 + mrn2(2 + dunlev(&lev->z) / 3);
                         cnt > 0; cnt--)
                        if ((otmp = mkobj(lev, GEM_CLASS, FALSE, mrng()))) {
                            if (otmp->otyp == ROCK) {
                                dealloc_obj(otmp);      /* discard it */
                            } else {
                                otmp->ox = x, otmp->oy = y;
                                if (!mrn2(3))
                                    add_to_buried(otmp);
                                else
                                    place_object(otmp, lev, x, y);
                            }
                        }
                }
            }
}

/* Topmost level creation routine.
   Mainly just wraps around makelevel(), but also handles loading bones files,
   mineralizing after the level is created, blocking digging, setting roomnos
   via topologize, and a couple other things.
   Called from a few places: newgame() (to generate level 1), goto_level (any
   other levels), and wiz_makemap (wizard mode regenerating the level). */
struct level *
mklev(d_level * levnum)
{
    struct mkroom *croom;
    int ln = ledger_no(levnum);
    struct level *lev;

    if (levels[ln])
        return levels[ln];

    if (getbones(levnum))
        return levels[ln];      /* initialized in getbones->getlev */

    lev = levels[ln] = alloc_level(levnum);
    init_rect(rng_for_level(levnum));

    in_mklev = TRUE;
    makelevel(lev);
    bound_digging(lev);
    mineralize(lev);
    in_mklev = FALSE;
    /* has_morgue gets cleared once morgue is entered; graveyard stays set
       (graveyard might already be set even when has_morgue is clear [see
       fixup_special()], so don't update it unconditionally) */
    if (search_special(lev, MORGUE))
        lev->flags.graveyard = 1;
    if (!lev->flags.is_maze_lev) {
        for (croom = &lev->rooms[0]; croom != &lev->rooms[lev->nroom]; croom++)
            topologize(lev, croom);
    }
    set_wall_state(lev);

    /* ensure that generated magic chests have nondiggable flag on them */
    struct obj *obj;
    for (obj = lev->objlist; obj; obj = obj->nobj)
        if (obj->otyp == MAGIC_CHEST)
            lev->locations[obj->ox][obj->oy].flags |= W_NONDIGGABLE;

    return lev;
}

/* Set the roomno correctly for all squares of the given room.
   Mostly this sets them to the roomno from croom, but if there are any walls
   that already have a roomno defined, it changes them to SHARED.
   Then it recurses on subrooms. */
void
topologize(struct level *lev, struct mkroom *croom)
{
    int x, y, roomno = (croom - lev->rooms) + ROOMOFFSET;
    int lowx = croom->lx, lowy = croom->ly;
    int hix = croom->hx, hiy = croom->hy;
    int subindex, nsubrooms = croom->nsubrooms;

    /* skip the room if already done; i.e. a shop handled out of order */
    /* also skip if this is non-rectangular (it _must_ be done already) */
    if ((int)lev->locations[lowx][lowy].roomno == roomno || croom->irregular)
        return;
    {
        /* do innards first */
        for (x = lowx; x <= hix; x++)
            for (y = lowy; y <= hiy; y++)
                lev->locations[x][y].roomno = roomno;
        /* top and bottom edges */
        for (x = lowx - 1; x <= hix + 1; x++)
            for (y = lowy - 1; y <= hiy + 1; y += (hiy - lowy + 2)) {
                lev->locations[x][y].edge = 1;
                if (lev->locations[x][y].roomno)
                    lev->locations[x][y].roomno = SHARED;
                else
                    lev->locations[x][y].roomno = roomno;
            }
        /* sides */
        for (x = lowx - 1; x <= hix + 1; x += (hix - lowx + 2))
            for (y = lowy; y <= hiy; y++) {
                lev->locations[x][y].edge = 1;
                if (lev->locations[x][y].roomno)
                    lev->locations[x][y].roomno = SHARED;
                else
                    lev->locations[x][y].roomno = roomno;
            }
    }
    /* subrooms */
    for (subindex = 0; subindex < nsubrooms; subindex++)
        topologize(lev, croom->sbrooms[subindex]);
}

/* Find an unused room for a branch location. */
static struct mkroom *
find_branch_room(struct level *lev, coord *mp)
{
    struct mkroom *croom = 0;

    if (lev->nroom == 0) {
        mazexy(lev, mp);        /* already verifies location */
    } else {
        /* not perfect - there may be only one stairway */
        if (lev->nroom > 2) {
            int tryct = 0;

            do
                croom = &lev->rooms[mrn2(lev->nroom)];
            while ((croom == lev->dnstairs_room || croom == lev->upstairs_room
                    || croom->rtype != OROOM) && (++tryct < 100));
        } else
            croom = &lev->rooms[mrn2(lev->nroom)];

        do {
            if (!somexy(lev, croom, mp, mrng()))
                impossible("Can't place branch!");
        } while (occupied(lev, mp->x, mp->y) ||
                 (lev->locations[mp->x][mp->y].typ != CORR &&
                  lev->locations[mp->x][mp->y].typ != ROOM));
    }
    return croom;
}

/* Find the room for (x,y).  Return null if not in a room. */
static struct mkroom *
pos_to_room(struct level *lev, xchar x, xchar y)
{
    int i;
    struct mkroom *curr;

    for (curr = lev->rooms, i = 0; i < lev->nroom; curr++, i++)
        if (inside_room(curr, x, y))
            return curr;
    return NULL;
}

/* Place a branch staircase or ladder for branch br at the coordinates (x,y).
   If x is zero, pick the branch room and coordinates within it randomly.
   If br is null, or the global made_branch is TRUE, do nothing. */
void
place_branch(struct level *lev, branch * br,    /* branch to place */
             xchar x, xchar y)
{       /* location */
    coord m;
    d_level *dest;
    boolean make_stairs;
    struct mkroom *br_room;

    /*
     * Return immediately if there is no branch to make or we have
     * already made one.  This routine can be called twice when
     * a special level is loaded that specifies an SSTAIR location
     * as a favored spot for a branch.
     *
     * As a special case, we also don't actually put anything into
     * the castle level.
     */
    if (!br || made_branch || Is_stronghold(&lev->z))
        return;

    if (x == COLNO) {   /* find random coordinates for branch */
        br_room = find_branch_room(lev, &m);
        x = m.x;
        y = m.y;
    } else {
        br_room = pos_to_room(lev, x, y);
    }

    if (on_level(&br->end1, &lev->z)) {
        /* we're on end1 */
        make_stairs = br->type != BR_NO_END1;
        dest = &br->end2;
    } else {
        /* we're on end2 */
        make_stairs = br->type != BR_NO_END2;
        dest = &br->end1;
    }

    if (!isok(x, y))
        panic("placing dungeon branch outside the map bounds");
    if (!x && !y)
        impossible("suspicious attempt to place dungeon branch at (0, 0)");

    if (br->type == BR_PORTAL) {
        mkportal(lev, x, y, dest->dnum, dest->dlevel);
    } else if (make_stairs) {
        lev->sstairs.sx = x;
        lev->sstairs.sy = y;
        lev->sstairs.up =
            (char)on_level(&br->end1, &lev->z) ? br->end1_up : !br->end1_up;
        assign_level(&lev->sstairs.tolev, dest);
        lev->sstairs_room = br_room;

        lev->locations[x][y].flags = lev->sstairs.up ? LA_UP : LA_DOWN;
        lev->locations[x][y].typ = STAIRS;
    }
    /*
     * Set made_branch to TRUE even if we didn't make a stairwell (i.e.
     * make_stairs is false) since there is currently only one branch
     * per level, if we failed once, we're going to fail again on the
     * next call.
     */
    made_branch = TRUE;
}

/* Return TRUE if the given location is directly adjacent to a door or secret
   door in any direction. */
static boolean
bydoor(struct level *lev, xchar x, xchar y)
{
    int typ;

    if (isok(x + 1, y)) {
        typ = lev->locations[x + 1][y].typ;
        if (IS_DOOR(typ) || typ == SDOOR)
            return TRUE;
    }
    if (isok(x - 1, y)) {
        typ = lev->locations[x - 1][y].typ;
        if (IS_DOOR(typ) || typ == SDOOR)
            return TRUE;
    }
    if (isok(x, y + 1)) {
        typ = lev->locations[x][y + 1].typ;
        if (IS_DOOR(typ) || typ == SDOOR)
            return TRUE;
    }
    if (isok(x, y - 1)) {
        typ = lev->locations[x][y - 1].typ;
        if (IS_DOOR(typ) || typ == SDOOR)
            return TRUE;
    }
    return FALSE;
}

/* Return TRUE if it is allowable to create a door at (x,y).
   The given coordinate must be a wall and not be adjacent to a door, and we
   can't be at the max number of doors.
   FIXME: This should return boolean. */
int
okdoor(struct level *lev, xchar x, xchar y)
{
    boolean near_door = bydoor(lev, x, y);

    return ((lev->locations[x][y].typ == HWALL ||
             lev->locations[x][y].typ == VWALL) && lev->doorindex < DOORMAX &&
            !near_door);
}

/* Wrapper for dosdoor. Create a door randomly at location (x,y) in aroom. For
   some reason, the logic of whether or not to make the door secret is here,
   while all the other logic of determining the door state is in dosdoor. */
void
dodoor(struct level *lev, int x, int y, struct mkroom *aroom)
{
    if (lev->doorindex >= DOORMAX) {
        impossible("DOORMAX exceeded?");
        return;
    }

    dosdoor(lev, x, y, aroom, mrn2(8) ? DOOR : SDOOR);
}

/* Return TRUE if the given location contains a trap, dungeon furniture, liquid
   terrain, or the vibrating square.
   Generally used for determining if a space is unsuitable for placing
   something. */
boolean
occupied(struct level * lev, xchar x, xchar y)
{
    return ((boolean) (t_at(lev, x, y)
                       || IS_FURNITURE(lev->locations[x][y].typ)
                       || is_lava(lev, x, y)
                       || is_pool(lev, x, y)
                       || invocation_pos(&lev->z, x, y)
            ));
}

/* Create a trap.
   If num is a valid trap index, create that specific trap.
   If tm is non-NULL, create the trap at tm's coordinates. Otherwise, if
   mazeflag is TRUE, choose a random maze position; if FALSE, assume that croom
   is non-NULL and pick a random location inside croom.

   If num is invalid as a trap index, it will create a random trap. In
   Gehennom, there is a 20% chance it will just pick fire trap. If various
   factors mean that the trap is unsuitable (usually because of difficulty), it
   will keep trying until it picks something valid.

   If a fallthru trap is created on a undiggable-floor level, it defaults to
   ROCKTRAP. If a WEB is created, a giant spider is created on top of it.
   Finally, if it is very early in the dungeon, and the trap is potentially

   lethal, create a minimal fake bones pile on the trap. */
void
mktrap(struct level *lev, int num, int mazeflag, struct mkroom *croom,
       coord * tm)
{
    int kind;
    coord m;

    /* no traps in pools */
    if (tm && is_pool(lev, tm->x, tm->y))
        return;

    if (num > 0 && num < TRAPNUM) {
        kind = num;
    } else if (Is_rogue_level(&lev->z)) {
        /* presumably Rogue-specific traps */
        switch (mrn2(7)) {
        default:
            kind = BEAR_TRAP;
            break;      /* 0 */
        case 1:
            kind = ARROW_TRAP;
            break;
        case 2:
            kind = DART_TRAP;
            break;
        case 3:
            kind = TRAPDOOR;
            break;
        case 4:
            kind = PIT;
            break;
        case 5:
            kind = SLP_GAS_TRAP;
            break;
        case 6:
            kind = RUST_TRAP;
            break;
        }
    } else if (In_hell(&lev->z) && !mrn2(5)) {
        /* bias the frequency of fire traps in Gehennom */
        kind = FIRE_TRAP;
    } else {
        unsigned lvl = level_difficulty(&lev->z);

        /* Try up to 200 times to find a random coordinate for the trap. */
        do {
            kind = 1 + mrn2(TRAPNUM - 1);
            /* reject "too hard" traps */
            switch (kind) {
            case MAGIC_PORTAL:
            case VIBRATING_SQUARE:
                kind = NO_TRAP;
                break;
            case ROLLING_BOULDER_TRAP:
            case SLP_GAS_TRAP:
                if (lvl < 2)
                    kind = NO_TRAP;
                break;
            case LEVEL_TELEP:
                if (lvl < 5 || lev->flags.noteleport)
                    kind = NO_TRAP;
                break;
            case SPIKED_PIT:
                if (lvl < 5)
                    kind = NO_TRAP;
                break;
            case LANDMINE:
                if (lvl < 6)
                    kind = NO_TRAP;
                break;
            case WEB:
                if (lvl < 7)
                    kind = NO_TRAP;
                break;
            case STATUE_TRAP:
            case POLY_TRAP:
                if (lvl < 8)
                    kind = NO_TRAP;
                break;
            case FIRE_TRAP:
                if (!In_hell(&lev->z))
                    kind = NO_TRAP;
                break;
            case TELEP_TRAP:
                if (lev->flags.noteleport)
                    kind = NO_TRAP;
                break;
            case HOLE:
                /* make these much less often than other traps */
                if (mrn2(7))
                    kind = NO_TRAP;
                break;
            }
        } while (kind == NO_TRAP);
    }

    if ((kind == TRAPDOOR || kind == HOLE) && !can_fall_thru(lev))
        kind = ROCKTRAP;

    if (tm)
        m = *tm;
    else {
        int tryct = 0;
        boolean avoid_boulder = (kind == PIT || kind == SPIKED_PIT ||
                                 kind == TRAPDOOR || kind == HOLE);

        do {
            if (++tryct > 200)
                return;
            if (mazeflag)
                mazexy(lev, &m);
            else if (!somexy(lev, croom, &m, mrng()))
                return;
        } while (occupied(lev, m.x, m.y) ||
                 (avoid_boulder && sobj_at(BOULDER, lev, m.x, m.y)));
    }

    maketrap(lev, m.x, m.y, kind, mrng());
    if (kind == WEB)
        makemon(&mons[PM_GIANT_SPIDER], lev, m.x, m.y, MM_ALLLEVRNG);
}

/* Create some (non-branch) stairs at (x,y) (absolute coords) inside croom.
   up is whether or not it's upstairs. */
void
mkstairs(struct level *lev, xchar x, xchar y, char up, struct mkroom *croom)
{
    if (!isok(x, y)) {
        impossible("mkstairs: bogus stair attempt at <%d,%d>", x, y);
        return;
    }
    if (!x && !y) {
        /* In 4.3-beta{1,2}, this doesn't save correctly, and there's no known
           way to get stairs here anyway... */
        impossible("mkstairs: suspicious stair attempt at <%d,%d>", x, y);
        return;
    }

    /*
     * We can't make a regular stair off an end of the dungeon.  This
     * attempt can happen when a special level is placed at an end and
     * has an up or down stair specified in its description file.
     */
    if ((dunlev(&lev->z) == 1 && up) ||
        (dunlev(&lev->z) == dunlevs_in_dungeon(&lev->z) && !up))
        return;

    if (up) {
        lev->upstair.sx = x;
        lev->upstair.sy = y;
        lev->upstairs_room = croom;
    } else {
        lev->dnstair.sx = x;
        lev->dnstair.sy = y;
        lev->dnstairs_room = croom;
    }

    lev->locations[x][y].typ = STAIRS;
    lev->locations[x][y].flags = up ? LA_UP : LA_DOWN;
}

/* Place a fountain.
   If mazeflag is TRUE, it will pick a random maze position; otherwise it will
   assume croom is non-null and will pick a random position inside it.
   May become a magic fountain with 1/7 chance. */
void
mkfount(struct level *lev, int mazeflag, struct mkroom *croom)
{
    coord m;
    int tryct = 0;

    /* This code is repeated across several functions and should probably be
       extracted into its own function... */
    do {
        if (++tryct > 200)
            return;
        if (mazeflag)
            mazexy(lev, &m);
        else if (!somexy(lev, croom, &m, mrng()))
            return;
    } while (occupied(lev, m.x, m.y) || bydoor(lev, m.x, m.y));

    lev->locations[m.x][m.y].typ = FOUNTAIN;
    /* Is it a "blessed" fountain? (affects drinking from fountain) */
    if (!mrn2(7))
        lev->locations[m.x][m.y].blessedftn = 1;
}

/* Place a sink somewhere in croom. */
void
mksink(struct level *lev, struct mkroom *croom)
{
    coord m;
    int tryct = 0;

    do {
        if (++tryct > 200)
            return;
        if (!somexy(lev, croom, &m, mrng()))
            return;
    } while (occupied(lev, m.x, m.y) || bydoor(lev, m.x, m.y));

    /* Put a sink at m.x, m.y */
    lev->locations[m.x][m.y].typ = SINK;
}

/* Place an altar somewhere in croom.
   Set its alignment randomly with uniform probability. */
void
mkaltar(struct level *lev, struct mkroom *croom)
{
    coord m;
    int tryct = 0;
    aligntyp al;

    if (croom->rtype != OROOM)
        return;

    do {
        if (++tryct > 200)
            return;
        if (!somexy(lev, croom, &m, mrng()))
            return;
    } while (occupied(lev, m.x, m.y) || bydoor(lev, m.x, m.y));

    /* Put an altar at m.x, m.y */
    lev->locations[m.x][m.y].typ = ALTAR;

    /* -1 - A_CHAOTIC, 0 - A_NEUTRAL, 1 - A_LAWFUL */
    if (In_hell(&lev->z))
        al = A_NONE;
    else
        al = mrn2((int)A_LAWFUL + 2) - 1;
    lev->locations[m.x][m.y].flags = Align2amask(al);
}

/* Create a grave (headstone) somewhere in croom. Special rules:
   1/10 of graves get a bell placed on them and a special inscription. The
        inscription is otherwise pulled from epitaph.
   1/3 of graves get gold placed on them.
   0-4 random cursed objects may be buried under the grave. */
void
mkgrave(struct level *lev, struct mkroom *croom)
{
    coord m;
    int tryct = 0;
    struct obj *otmp;
    boolean dobell = !mrn2(10);

    if (croom->rtype != OROOM)
        return;

    do {
        if (++tryct > 200)
            return;
        if (!somexy(lev, croom, &m, mrng()))
            return;
    } while (occupied(lev, m.x, m.y) || bydoor(lev, m.x, m.y));

    /* Put a grave at m.x, m.y */
    make_grave(lev, m.x, m.y, dobell ? "Saved by the bell!" : NULL);

    /* Possibly fill it with objects */
    if (!mrn2(3))
        mkgold(0L, lev, m.x, m.y, mrng());
    for (tryct = mrn2(5); tryct; tryct--) {
        otmp = mkobj(lev, RANDOM_CLASS, TRUE, mrng());
        if (!otmp)
            return;
        curse(otmp);
        otmp->ox = m.x;
        otmp->oy = m.y;
        add_to_buried(otmp);
    }

    /* Leave a bell, in case we accidentally buried someone alive */
    if (dobell)
        mksobj_at(BELL, lev, m.x, m.y, TRUE, FALSE, mrng());
    return;
}


/* maze levels have slightly different constraints from normal levels */
#define x_maze_min 2
#define y_maze_min 2
/* Major level transmutation: add a set of stairs (to the Sanctum) after
   an earthquake that leaves behind a a new topology, centered at inv_pos.
   Assumes there are no rooms within the invocation area and that inv_pos
   is not too close to the edge of the map.
   FIXME: Can't assume that the hero can see, they could be roleplaying
   blind. */
void
mkinvokearea(void)
{
    int dist;
    xchar xmin = gamestate.inv_pos.x, xmax = gamestate.inv_pos.x;
    xchar ymin = gamestate.inv_pos.y, ymax = gamestate.inv_pos.y;
    xchar i;

    pline(msgc_levelsound, "The floor shakes violently under you!");
    if (Blind)
        pline_implied(msgc_levelsound,
                      "The entire dungeon seems to be tearing apart!");
    else
        pline_implied(msgc_levelsound,
                      "The walls around you begin to bend and crumble!");
    win_pause_output(P_MESSAGE);

    mkinvpos(xmin, ymin, 0);    /* middle, before placing stairs */

    for (dist = 1; dist < 7; dist++) {
        xmin--;
        xmax++;

        /* top and bottom */
        if (dist != 3) {        /* the area is wider that it is high */
            ymin--;
            ymax++;
            for (i = xmin + 1; i < xmax; i++) {
                mkinvpos(i, ymin, dist);
                mkinvpos(i, ymax, dist);
            }
        }

        /* left and right */
        for (i = ymin; i <= ymax; i++) {
            mkinvpos(xmin, i, dist);
            mkinvpos(xmax, i, dist);
        }

        flush_screen(); /* make sure the new glyphs shows up */
        win_delay_output();
    }

    if (Blind)
        pline(msgc_levelsound,
              "You feel the stones reassemble below you!");
    else
        pline(msgc_levelsound,
              "You are standing at the top of a stairwell leading down!");
    mkstairs(level, u.ux, u.uy, 0, NULL);       /* down */
    newsym(u.ux, u.uy);
    turnstate.vision_full_recalc = TRUE;     /* everything changed */
}


/* Change level topology.  Boulders in the vicinity are eliminated.
 * Temporarily overrides vision in the name of a nice effect.
 */
static void
mkinvpos(xchar x, xchar y, int dist)
{
    struct trap *ttmp;
    struct obj *otmp;
    struct monst *mtmp;
    boolean make_rocks;
    struct rm *loc = &level->locations[x][y];

    /* clip at existing map borders if necessary */
    if (!within_bounded_area
        (x, y, x_maze_min + 1, y_maze_min + 1, x_maze_max - 1,
         y_maze_max - 1)) {
        /* only outermost 2 columns and/or rows may be truncated due to edge */
        if (dist < (7 - 2))
            panic("mkinvpos: <%d,%d> (%d) off map edge!", x, y, dist);
        return;
    }

    /* clear traps */
    if ((ttmp = t_at(level, x, y)) != 0)
        deltrap(level, ttmp);

    /* clear boulders; leave some rocks for non-{moat|trap} locations */
    make_rocks = (dist != 1 && dist != 4 && dist != 5) ? TRUE : FALSE;
    while ((otmp = sobj_at(BOULDER, level, x, y)) != 0) {
        if (make_rocks) {
            fracture_rock(otmp);
            make_rocks = FALSE; /* don't bother with more rocks */
        } else {
            obj_extract_self(otmp);
            obfree(otmp, NULL);
        }
    }

    if (!Blind) {
        /* make sure vision knows this location is open */
        unblock_point(x, y);

        /* fake out saved state */
        loc->seenv = 0;
        loc->flags = 0;
        if (dist < 6)
            loc->lit = TRUE;
        loc->waslit = TRUE;
        loc->horizontal = FALSE;
        clear_memory_glyph(x, y, S_unexplored);
        viz_array[y][x] = (dist < 6) ?
            (IN_SIGHT | COULD_SEE) :     /* short-circuit vision recalc */
            COULD_SEE;
    }

    switch (dist) {
    case 1:    /* fire traps */
        if (is_pool(level, x, y))
            break;
        loc->typ = ROOM;
        ttmp = maketrap(level, x, y, FIRE_TRAP, rng_main);
        if (ttmp)
            ttmp->tseen = TRUE;
        break;
    case 0:    /* lit room locations */
    case 2:
    case 3:
    case 6:    /* unlit room locations */
        loc->typ = ROOM;
        break;
    case 4:    /* pools (aka a wide moat) */
    case 5:
        loc->typ = MOAT;
        mtmp = m_at(level, x, y);
        if (mtmp)
            minliquid(mtmp);
        /* No kelp! */
        break;
    default:
        impossible("mkinvpos called with dist %d", dist);
        break;
    }

    /* display new value of position; could have a monster/object on it */
    newsym(x, y);
}

/*
 * The portal to Ludios is special.  The entrance can only occur within a
 * vault in the main dungeon at a depth greater than 10.  The Ludios branch
 * structure reflects this by having a bogus "source" dungeon:  the value
 * of n_dgns (thus, Is_branchlev() will never find it).
 *
 * Ludios will remain isolated until the branch is corrected by this function.
 */
static void
mk_knox_portal(struct level *lev, xchar x, xchar y)
{
    d_level *source;
    branch *br;
    schar u_depth;

    br = dungeon_branch("Fort Ludios");
    if (on_level(&knox_level, &br->end1)) {
        source = &br->end2;
    } else {
        /* disallow Knox branch on a level with one branch already */
        if (Is_branchlev(&lev->z))
            return;
        source = &br->end1;
    }

    /* Already set or 2/3 chance of deferring until a later level. */
    if (source->dnum < n_dgns)
        return;

    if (!(lev->z.dnum == oracle_level.dnum      /* in main dungeon */
          && !at_dgn_entrance(&lev->z, "The Quest")     /* but not Quest's
                                                           entry */
          &&(u_depth = depth(&lev->z)) > 10     /* beneath 10 */
          && u_depth < depth(&medusa_level)))   /* and above Medusa */
        return;

    /* Adjust source to be current level and re-insert branch. */
    *source = lev->z;
    insert_branch(br, TRUE);

    place_branch(lev, br, x, y);
}

/*mklev.c*/
