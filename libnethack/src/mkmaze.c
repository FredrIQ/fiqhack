/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-05-01 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "sp_lev.h"
#include "lev.h"        /* save & restore info */

static boolean iswall(struct level *lev, int x, int y);
static boolean iswall_or_stone(struct level *lev, int x, int y);
static boolean is_solid(struct level *lev, int x, int y);
static int extend_spine(int[3][3], int, int, int);
static boolean okay(struct level *lev, int x, int y, int dir);
static void maze0xy(coord *, enum rng);
static boolean put_lregion_here(struct level *lev, xchar, xchar, xchar, xchar,
                                xchar, xchar, xchar, boolean, d_level *);
static void move(int *, int *, int);
static boolean bad_location(struct level *lev, xchar x, xchar y, xchar lx,
                            xchar ly, xchar hx, xchar hy);
static boolean maze_inbounds(xchar, xchar);
static boolean maze_edge(xchar, xchar);
static void maze_remove_deadends(struct level *);
static boolean maze_room_ok(struct level *);
static void maze_add_rooms(struct level *, int, int *);
static void maze_touchup_rooms(struct level *, int);
static const char * waterbody_impl(xchar x, xchar y, boolean article);

/* adjust a coordinate one step in the specified direction */
#define mz_move(X, Y, dir) \
    do {                                                         \
        switch (dir) {                                           \
        case 0:  --(Y);  break;                                  \
        case 1:  (X)++;  break;                                  \
        case 2:  (Y)++;  break;                                  \
        case 3:  --(X);  break;                                  \
        default: panic("mz_move: bad direction %d", dir);        \
        }                                                        \
    } while (0)

static boolean bubble_up;

/* Returns TRUE if the terrain at the given location is in-bounds and is a
   wall, door, secret door, or iron bars.
   Despite the name, any door will return TRUE here, even if it's open, broken,
   or missing. */
static boolean
iswall(struct level *lev, int x, int y)
{
    int type;

    if (!isok(x, y))
        return FALSE;
    type = lev->locations[x][y].typ;
    return (IS_WALL(type) || IS_DOOR(type) || type == SDOOR ||
            type == IRONBARS);
}

/* Same as iswall(), except also returns TRUE if the terrain at the given
   location is stone. */
static boolean
iswall_or_stone(struct level *lev, int x, int y)
{
    int type;

    /* out of bounds = stone */
    if (!isok(x, y))
        return TRUE;

    type = lev->locations[x][y].typ;
    return (type == STONE || IS_WALL(type) || IS_DOOR(type) || type == SDOOR ||
            type == IRONBARS);
}

/* return TRUE if out of bounds, wall or rock */
static boolean
is_solid(struct level *lev, int x, int y)
{
    return !isok(x, y) || IS_STWALL(lev->locations[x][y].typ);
}


/*
 * Return 1 (not TRUE - we're doing bit vectors here) if we want to extend
 * a wall spine in the (dx,dy) direction.  Return 0 otherwise.
 *
 * To extend a wall spine in that direction, first there must be a wall there.
 * Then, extend a spine unless the current position is surrounded by walls
 * in the direction given by (dx,dy).  E.g. if 'x' is our location, 'W'
 * a wall, '.' a room, 'a' anything (we don't care), and our direction is
 * (0,1) - South or down - then:
 *
 *      a a a
 *      W x W           This would not extend a spine from x down
 *      W W W           (a corridor of walls is formed).
 *
 *      a a a
 *      W x W           This would extend a spine from x down.
 *      . W W
 */
static int
extend_spine(int locale[3][3], int wall_there, int dx, int dy)
{
    int spine, nx, ny;

    nx = 1 + dx;
    ny = 1 + dy;

    if (wall_there) {   /* wall in that direction */
        if (dx) {
            if (locale[1][0] && locale[1][2] && /* EW are wall/stone */
                locale[nx][0] && locale[nx][2]) {      /* diag are wall/stone */
                spine = 0;
            } else {
                spine = 1;
            }
        } else {        /* dy */
            if (locale[0][1] && locale[2][1] && /* NS are wall/stone */
                locale[0][ny] && locale[2][ny]) {      /* diag are wall/stone */
                spine = 0;
            } else {
                spine = 1;
            }
        }
    } else {
        spine = 0;
    }

    return spine;
}


/*
 * Wall cleanup.  This function has two purposes: (1) remove walls that
 * are totally surrounded by stone - they are redundant.  (2) correct
 * the types so that they extend and connect to each other.
 */
void
wallification(struct level *lev, int x1, int y1, int x2, int y2,
              boolean destroy_freestanding)
{
    uchar type;
    int x, y;
    struct rm *loc;
    int bits;
    int locale[3][3];   /* rock or wall status surrounding positions */

    /* 
     * Value 0 represents a free-standing wall.  It could be anything,
     * so even though this table says VWALL, we actually leave whatever
     * typ was there alone, unless destroy_freestanding is set.
     */
    static const xchar spine_array[16] = {
        VWALL, HWALL, HWALL, HWALL,
        VWALL, TRCORNER, TLCORNER, TDWALL,
        VWALL, BRCORNER, BLCORNER, TUWALL,
        VWALL, TLWALL, TRWALL, CROSSWALL
    };

    /* sanity check on incoming variables */
    if (x1 < 0 || x2 >= COLNO || x1 > x2 || y1 < 0 || y2 >= ROWNO || y1 > y2)
        panic("wallification: bad bounds (%d,%d) to (%d,%d)", x1, y1, x2, y2);

    /* Step 1: change walls surrounded by rock to rock. */
    for (x = x1; x <= x2; x++)
        for (y = y1; y <= y2; y++) {
            loc = &lev->locations[x][y];
            type = loc->typ;
            if (IS_WALL(type) && type != DBWALL) {
                if (is_solid(lev, x - 1, y - 1) && is_solid(lev, x - 1, y) &&
                    is_solid(lev, x - 1, y + 1) && is_solid(lev, x, y - 1) &&
                    is_solid(lev, x, y + 1) && is_solid(lev, x + 1, y - 1) &&
                    is_solid(lev, x + 1, y) && is_solid(lev, x + 1, y + 1))
                    loc->typ = STONE;
            }
        }

    /* 
     * Step 2: set the correct wall type.  We can't combine steps
     * 1 and 2 into a single sweep because we depend on knowing if
     * the surrounding positions are stone.
     */
    for (x = x1; x <= x2; x++)
        for (y = y1; y <= y2; y++) {
            loc = &lev->locations[x][y];
            type = loc->typ;
            if (!(IS_WALL(type) && type != DBWALL))
                continue;

            /* set the locations TRUE if rock or wall or out of bounds */
            locale[0][0] = iswall_or_stone(lev, x - 1, y - 1);
            locale[1][0] = iswall_or_stone(lev, x, y - 1);
            locale[2][0] = iswall_or_stone(lev, x + 1, y - 1);

            locale[0][1] = iswall_or_stone(lev, x - 1, y);
            locale[2][1] = iswall_or_stone(lev, x + 1, y);

            locale[0][2] = iswall_or_stone(lev, x - 1, y + 1);
            locale[1][2] = iswall_or_stone(lev, x, y + 1);
            locale[2][2] = iswall_or_stone(lev, x + 1, y + 1);

            /* determine if wall should extend to each direction NSEW */
            bits = (extend_spine(locale, iswall(lev, x, y - 1), 0, -1) << 3)
                | (extend_spine(locale, iswall(lev, x, y + 1), 0, 1) << 2)
                | (extend_spine(locale, iswall(lev, x + 1, y), 1, 0) << 1)
                | extend_spine(locale, iswall(lev, x - 1, y), -1, 0);

            /* don't change typ if wall is free-standing */
            if (bits)
                loc->typ = spine_array[bits];
            else if (destroy_freestanding)
                loc->typ = ROOM;
        }
}

/* Returns TRUE if the spot two steps in dir direction from the given location
   is:
   1: Stone terrain.
   2: More than 3 steps away from the left and top maze edges. (x=0 doesn't
      exist, so it makes sense that the maze wall starts at x=3, but I'm not
      sure why the upper wall can't start at y=0 and the maze at y=1).
   3: Otherwise within the maze.
   Used exclusively in walkfrom() to determine whether it should carve a path
   to a new space. */
static boolean
okay(struct level *lev, int x, int y, int dir)
{
    move(&x, &y, dir);
    move(&x, &y, dir);
    if (x < 3 || y < 3 || x > x_maze_max || y > y_maze_max ||
        lev->locations[x][y].typ != 0)
        return FALSE;
    return TRUE;
}

/* Choose a random starting point for maze generation.
   The point is guaranteed to be on the maze grid: that is, it must have odd x
   and y coordinates and have 3 <= x < x_maze_max and 3 <= y < y_maze_max */
static void
maze0xy(coord *cc, enum rng rng)
{
    cc->x = 3 + 2 * rn2_on_rng((x_maze_max >> 1) - 1, rng);
    cc->y = 3 + 2 * rn2_on_rng((y_maze_max >> 1) - 1, rng);
    return;
}

/*
 * Bad if:
 *      pos is occupied OR
 *      pos is inside restricted region (lx,ly,hx,hy) OR
 *      NOT (pos is corridor and a maze level OR pos is a room OR pos is air)
 */
boolean
bad_location(struct level * lev, xchar x, xchar y, xchar lx, xchar ly, xchar hx,
             xchar hy)
{
    return ((boolean)
            (occupied(lev, x, y) || within_bounded_area(x, y, lx, ly, hx, hy) ||
             !((lev->locations[x][y].typ == CORR && lev->flags.is_maze_lev) ||
               lev->locations[x][y].typ == ROOM ||
               lev->locations[x][y].typ == AIR)));
}

/* pick a location in area (lx, ly, hx, hy) but not in (nlx, nly, nhx, nhy)
   and place something (based on rtype) in that region.
   If lx = 0, it will try the whole level rather than a subregion. */
void
place_lregion(struct level *lev, xchar lx, xchar ly, xchar hx, xchar hy,
              xchar nlx, xchar nly, xchar nhx, xchar nhy, xchar rtype,
              d_level * dest_lvl)
{
    int trycnt;
    boolean oneshot;
    xchar x, y;

    if (lx == COLNO) {  /* default to whole level */
        /* 
         * if there are rooms and this a branch, let place_branch choose
         * the branch location (to avoid putting branches in corridors).
         */
        if (rtype == LR_BRANCH && lev->nroom) {
            place_branch(lev, Is_branchlev(&lev->z), COLNO, ROWNO);
            return;
        }

        lx = 1;
        hx = COLNO - 1;
        ly = 1;
        hy = ROWNO - 1;
    }

    /* first a probabilistic approach */
    oneshot = (lx == hx && ly == hy);
    for (trycnt = 0; trycnt < 200; trycnt++) {
        x = lx + mklev_rn2((hx - lx) + 1, lev);
        y = ly + mklev_rn2((hy - ly) + 1, lev);
        if (put_lregion_here
            (lev, x, y, nlx, nly, nhx, nhy, rtype, oneshot, dest_lvl))
            return;
    }

    /* then a deterministic one */
    oneshot = TRUE;
    for (x = lx; x <= hx; x++)
        for (y = ly; y <= hy; y++)
            if (put_lregion_here
                (lev, x, y, nlx, nly, nhx, nhy, rtype, oneshot, dest_lvl))
                return;

    impossible("Couldn't place lregion type %d!", rtype);
}

/* Try to place something based on rtype specifically at (x,y).
   If the location is bad (occupied, has a trap, or is within the rectangle
   nlx, nly, nhx, nhy), it'll fail, UNLESS oneshot is specified, in which case
   it'll try to make it non-bad by removing a trap.
   oneshot basically means we're only trying this space, so don't tell the
   caller to try somewhere else. */
static boolean
put_lregion_here(struct level *lev, xchar x, xchar y, xchar nlx, xchar nly,
                 xchar nhx, xchar nhy, xchar rtype, boolean oneshot,
                 d_level * dest_lvl)
{
    if (bad_location(lev, x, y, nlx, nly, nhx, nhy)) {
        if (!oneshot) {
            return FALSE;       /* caller should try again */
        } else {
            /* Must make do with the only location possible; avoid failure due
               to a misplaced trap. It might still fail if there's a dungeon
               feature here. */
            struct trap *t = t_at(lev, x, y);

            if (t && t->ttyp != MAGIC_PORTAL && t->ttyp != VIBRATING_SQUARE)
                deltrap(lev, t);
            if (bad_location(lev, x, y, nlx, nly, nhx, nhy))
                return FALSE;
        }
    }
    switch (rtype) {
    case LR_TELE:
    case LR_UPTELE:
    case LR_DOWNTELE:
        /* "something" means the player in this case */
        if (MON_AT(lev, x, y)) {
            /* move the monster if no choice, or just try again */
            if (oneshot)
                rloc(m_at(lev, x, y), FALSE);
            else
                return FALSE;
        }
        u_on_newpos(x, y);
        break;
    case LR_PORTAL:
        mkportal(lev, x, y, dest_lvl->dnum, dest_lvl->dlevel);
        break;
    case LR_DOWNSTAIR:
    case LR_UPSTAIR:
        mkstairs(lev, x, y, (char)rtype, NULL);
        break;
    case LR_BRANCH:
        place_branch(lev, Is_branchlev(&lev->z), x, y);
        break;
    }
    return TRUE;
}

static boolean
maze_inbounds(xchar x, xchar y)
{
    return (isok(x, y) && x >= 2 && y >= 2 &&
            x < x_maze_max && y < y_maze_max);
}

/* Return TRUE iff the given location is on the edge of the valid maze area;
   that is, if you could step one square in any direction and then go out of
   bounds. */
static boolean
maze_edge(xchar x, xchar y)
{
    return (!maze_inbounds(x - 1, y) || !maze_inbounds(x + 1, y) ||
            !maze_inbounds(x, y - 1) || !maze_inbounds(x, y + 1));
}

/* Remove all dead ends from the maze.
   Does this by looking for spots that have 3 or more directions in which they
   are separated from another open space 2 squares away by some inaccessible
   terrain. Then it picks a random one of these directions and replaces the
   wall between them with whatever terrain typ is.
   If it decides to remove the dead end in a direction that goes into a room,
   create a door there instead. */
void
maze_remove_deadends(struct level *lev)
{
    char dirok[4];
    int x, y, dir, idx, idx2, dx, dy, dx2, dy2;

    dirok[0] = 0; /* lint suppression */
    for (x = 2; x < x_maze_max; x++)
        for (y = 2; y < y_maze_max; y++)
            if (ACCESSIBLE(lev->locations[x][y].typ) &&
                (x % 2) && (y % 2)) {
                idx = idx2 = 0;
                for (dir = 0; dir < 4; dir++) {
                    /* note: mz_move() is a macro which modifies
                       one of its first two parameters */
                    dx = dx2 = x;
                    dy = dy2 = y;
                    mz_move(dx, dy, dir);
                    if (!maze_inbounds(dx, dy)) {
                        idx2++;
                        continue;
                    }
                    mz_move(dx2, dy2, dir);
                    mz_move(dx2, dy2, dir);
                    if (!maze_inbounds(dx2, dy2)) {
                        idx2++;
                        continue;
                    }
                    if (!ACCESSIBLE(lev->locations[dx][dy].typ)
                        && ACCESSIBLE(lev->locations[dx2][dy2].typ)) {
                        dirok[idx++] = dir;
                        idx2++;
                    }
                }
                if (idx2 >= 3 && idx > 0) {
                    dx = x;
                    dy = y;
                    dir = dirok[rn2(idx)];
                    mz_move(dx, dy, dir);
                    if (lev->locations[dx][dy].roomno != NO_ROOM) {
                        dodoor(lev, dx, dy,
                               &lev->rooms[lev->locations[dx][dy].roomno]);
                    }
                    else {
                        lev->locations[dx][dy].typ = ROOM;
                    }
                }
            }
}

static boolean
maze_room_ok(struct level *lev)
{
    if (Is_special(&lev->z) || Invocation_lev(&lev->z))
        return FALSE;
    return TRUE;
}

static void
maze_add_rooms(struct level *lev, int attempts, int *smeq)
{
    if (!maze_room_ok(lev))
        return;

    xchar x, y;

    /* Pre-room setup: set the whole level to STONE so that rooms don't object
       to being placed on it. */
    for (x = 2; x <= x_maze_max; x++)
        for (y = 2; y <= y_maze_max; y++)
            lev->locations[x][y].typ = STONE;

    /* TODO:
       Break this room-creating logic into its own function.
       Add regularly generated doors to rooms.
       Allow rooms to
       YANI: rooms can generate without doors - make them into treasure vaults,
       make all but one wall (in a door position) undiggable, and stock with
       treasure.
       Gehennom special room types: water room with sea monsters,
       slaughterhouse, demon den, regular rooms but with sinks or fountains,
       more graveyards. */

    /* Add some rooms to the maze!
       Rooms go first, before the mazewalk is executed. */

    for (; attempts; attempts--) {
        int roomidx;
        coord roompos;
        xchar width, height;
        boolean intersect = FALSE;
        int door_attempts = 5;
        int no_doors_made = 0;

        /* room must fit nicely in the maze; that is, its corner spaces should
           all be valid maze0xy() locations. Thus, it should have odd
           dimensions. */
        width = 2 * mklev_rn2(4, lev) + 3;
        height = 2 * mklev_rn2(4, lev) + 3;

        /* Pick a corner location. Which directions the room points out from
           this corner are randomized so that we don't have a bias towards any
           edge of the map. */
        maze0xy(&roompos, rng_for_level(&lev->z));
        if (mklev_rn2(2, lev)) {
            /* original roompos is a bottom corner */
            roompos.y -= (height-1);
        }
        if (mklev_rn2(2, lev)) {
            /* original roompos is a right corner */
            roompos.x -= (width - 1);
        }
        /* these variables are the boundaries including walls */
        xchar lx = roompos.x - 1;
        xchar ly = roompos.y - 1;
        xchar hx = roompos.x + width;
        xchar hy = roompos.y + height;

        if (!maze_inbounds(lx, ly) || !maze_inbounds(hx, hy)) {
            /* can't place, out of bounds */
            continue;
        }

        /* Does it intersect with an existing room?
           Because of the constraints on room coordinates and the behavior of
           inside_room, this should ensure that no two rooms ever touch.
           Also make sure no two room walls ever touch; this should guarantee
           that the mazewalk can reach everywhere. */
        for (roomidx = 0; roomidx < lev->nroom; roomidx++) {
            struct mkroom* croom = &lev->rooms[roomidx];
            /* coordinates of the bounding walls, not the actual room */
            if (inside_room(croom, lx, ly)
                || inside_room(croom, hx, ly)
                || inside_room(croom, lx, hy)
                || inside_room(croom, hx, hy)) {
                intersect = TRUE;
                break;
            }
        }
        if (intersect) {
            /* can't place this one */
            continue;
        }
        /* pick a suitable place in the maze to add a room */
        if (!create_room(lev, roompos.x, roompos.y, width, height,
                         RM_LEFT, RM_TOP, OROOM, 1, smeq))
            continue;

        /* put in some doors */
        for (; door_attempts > 0; door_attempts--) {
            /* don't use finddpos - it'll bias doors towards the top left of
               the room */
            xchar doorx, doory;
            boolean horiz = mklev_rn2(2, lev) ? TRUE : FALSE;
            if (horiz) {
                doorx = mklev_rn2(hx - lx + 1, lev) + lx;
                doory = mklev_rn2(2, lev) ? ly : hy;
            }
            else {
                doorx = mklev_rn2(2, lev) ? hx : lx;
                doory = mklev_rn2(hy - ly + 1, lev) + ly;
            }
            /* Don't generate doors on the maze edges */
            if (maze_edge(doorx, doory))
                continue;
            /* Make sure doors don't generate at possible wall-intersection
               points (inverse of valid maze0xy() locations).
               Because we're on a wall, one of doorx and doory will be even --
               but the other should not be.
               Shouldn't need okdoor() - with this rule, doors can't be next to
               each other and they should always be on walls */
            if ((doorx % 2 == 0) && (doory % 2 == 0)) {
                if (door_attempts == 1) {
                    door_attempts++; /* try again */
                    no_doors_made++;
                    if (no_doors_made == 100)
                        break;
                }
                continue;
            }
            dodoor(lev, doorx, doory, &lev->rooms[lev->nroom-1]);
        }
        if (no_doors_made >= 100) {
            impossible("maze_add_rooms: couldn't place a door?");
        }

        /* set roomno so mazewalk and other maze generation ignore it */
        topologize(lev, &lev->rooms[lev->nroom-1]);
    }
}

/* Utility function to destroy a wall at the given spot. */
static void
destroy_wall(struct level *lev, xchar x, xchar y)
{
    /* don't destroy walls of the maze border */
    if (maze_edge(x, y))
        return;
    if (IS_WALL(lev->locations[x][y].typ) ||
        IS_DOOR(lev->locations[x][y].typ)) {
        lev->locations[x][y].typ = ROOM;
        lev->locations[x][y].flags = 0; /* clear door mask */
    }
}

/* Postprocessing after most of the rest of the level has been created
   (including stairs), and some rooms exist.
   Select attempts rooms to be converted into special rooms, then possibly
   place some other furniture inside the room. For non-special rooms, also
   knock down some walls to make the maze more accessible. */
static void
maze_touchup_rooms(struct level *lev, int attempts)
{
    if (!maze_room_ok(lev))
        return;

    enum rng rng = rng_for_level(&lev->z);
    int i;

    fix_stair_rooms(lev, TRUE);

    for (; attempts; attempts--)
        mkroom(lev, rand_roomtype(lev));
    for (i = 0; i < lev->nroom; ++i) {
        if (lev->rooms[i].rtype == OROOM) {
            /* probabilities here are deflated from makelevel() */
            if (!rn2_on_rng(20, rng))
                mkfount(lev, FALSE, &lev->rooms[i]);
            if (!rn2_on_rng(80, rng))
                mksink(lev, &lev->rooms[i]);
            if (!rn2_on_rng(100, rng))
                mkgrave(lev, &lev->rooms[i]);
            if (!rn2_on_rng(100, rng))
                mkaltar(lev, &lev->rooms[i]);
        }

        if (lev->rooms[i].rtype == OROOM ||
            lev->rooms[i].rtype == FAKEWIZ) {
            /* Remove the walls for this room. */
            xchar x, y;
            for (x = lev->rooms[i].lx; x <= lev->rooms[i].hx; x++) {
                destroy_wall(lev, x, lev->rooms[i].ly - 1);
                destroy_wall(lev, x, lev->rooms[i].hy + 1);
            }
            for (y = lev->rooms[i].ly; y <= lev->rooms[i].hy; y++) {
                destroy_wall(lev, lev->rooms[i].lx - 1, y);
                destroy_wall(lev, lev->rooms[i].hx + 1, y);
            }
        }
    }

    wallification(lev, 2, 2, x_maze_max, y_maze_max, TRUE);
}

void
makemaz(struct level *lev, const char *s, int *smeq)
{
    int x, y;
    char protofile[20];
    s_level *sp = Is_special(&lev->z);
    coord mm;

    if (*s) {
        if (sp && sp->rndlevs)
            snprintf(protofile, SIZE(protofile), "%s-%d", s,
                     1 + mklev_rn2((int)sp->rndlevs, lev));
        else
            strcpy(protofile, s);
    } else if (*(find_dungeon(&lev->z).proto)) {
        if (dunlevs_in_dungeon(&lev->z) > 1) {
            if (sp && sp->rndlevs)
                snprintf(protofile, SIZE(protofile), "%s%d-%d",
                         find_dungeon(&lev->z).proto,
                         dunlev(&lev->z), 1 + mklev_rn2((int)sp->rndlevs, lev));
            else
                snprintf(protofile, SIZE(protofile), "%s%d",
                         find_dungeon(&lev->z).proto, dunlev(&lev->z));
        } else if (sp && sp->rndlevs) {
            snprintf(protofile, SIZE(protofile), "%s-%d",
                     find_dungeon(&lev->z).proto,
                     1 + mklev_rn2((int)sp->rndlevs, lev));
        } else
            strcpy(protofile, find_dungeon(&lev->z).proto);

    } else
        strcpy(protofile, "");

    if (*protofile) {
        strcat(protofile, LEV_EXT);
        if (load_special(lev, protofile, smeq)) {
            fixup_special(lev);
            /* some levels can end up with monsters on dead mon list, including
               light source monsters */
            dmonsfree(lev);
            return;     /* no mazification right now */
        }
        impossible("Couldn't load \"%s\" - making a maze.", protofile);
    }

    lev->flags.is_maze_lev = TRUE;

    maze_add_rooms(lev, 20, smeq);

    for (x = 2; x <= x_maze_max; x++)
        for (y = 2; y <= y_maze_max; y++)
            if (lev->locations[x][y].roomno == NO_ROOM)
                lev->locations[x][y].typ = ((x % 2) && (y % 2)) ? STONE : HWALL;

    do {
        maze0xy(&mm, rng_for_level(&lev->z));
        /* continue searching for spots until we find one that's not in a room;
           the room generation code should never generate rooms such that the
           mazewalk has no available locations or cannot spread over the rest
           of the unoccupied space */
    } while (lev->locations[mm.x][mm.y].roomno != NO_ROOM);

    walkfrom(lev, mm.x, mm.y);

    if (!mklev_rn2(5, lev))
        maze_remove_deadends(lev);

    /* put a boulder at the maze center */
    mksobj_at(BOULDER, lev, (int)mm.x, (int)mm.y, TRUE, FALSE,
              rng_for_level(&lev->z));

    wallification(lev, 2, 2, x_maze_max, y_maze_max, FALSE);
    mazexy(lev, &mm);
    mkstairs(lev, mm.x, mm.y, 1, NULL); /* up */
    if (!Invocation_lev(&lev->z)) {
        mazexy(lev, &mm);
        mkstairs(lev, mm.x, mm.y, 0, NULL);     /* down */
    } else {    /* choose "vibrating square" location */
#define x_maze_min 2
#define y_maze_min 2
        /* 
         * Pick a position where the stairs down to Moloch's Sanctum
         * level will ultimately be created.  At that time, an area
         * will be altered:  walls removed, moat and traps generated,
         * boulders destroyed.  The position picked here must ensure
         * that that invocation area won't extend off the map.
         *
         * We actually allow up to 2 squares around the usual edge of
         * the area to get truncated; see mkinvokearea(mklev.c).
         */
#define INVPOS_X_MARGIN (6 - 2)
#define INVPOS_Y_MARGIN (5 - 2)
#define INVPOS_DISTANCE 11
        int x_range =
            x_maze_max - x_maze_min - 2 * INVPOS_X_MARGIN - 1, y_range =
            y_maze_max - y_maze_min - 2 * INVPOS_Y_MARGIN - 1;

        /* {occupied() => invocation_pos()} */
        gamestate.inv_pos.x = gamestate.inv_pos.y = 0;
        do {
            x = mklev_rn2(x_range, lev) + x_maze_min + INVPOS_X_MARGIN + 1;
            y = mklev_rn2(y_range, lev) + y_maze_min + INVPOS_Y_MARGIN + 1;
            /* we don't want it to be too near the stairs, nor to be on a spot
               that's already in use (wall|trap) */
        } while (x == lev->upstair.sx || y == lev->upstair.sy ||
                 /* (direct line) */
                 abs(x - lev->upstair.sx) == abs(y - lev->upstair.sy) ||
                 distmin(x, y, lev->upstair.sx,
                         lev->upstair.sy) <= INVPOS_DISTANCE ||
                 !SPACE_POS(lev->locations[x][y].typ) || occupied(lev, x, y));
        gamestate.inv_pos.x = x;
        gamestate.inv_pos.y = y;
        maketrap(lev, gamestate.inv_pos.x, gamestate.inv_pos.y,
                 VIBRATING_SQUARE, rng_for_level(&lev->z));
#undef INVPOS_X_MARGIN
#undef INVPOS_Y_MARGIN
#undef INVPOS_DISTANCE
#undef x_maze_min
#undef y_maze_min
    }

    /* place branch stair or portal */
    place_branch(lev, Is_branchlev(&lev->z), COLNO, ROWNO);

    maze_touchup_rooms(lev, mklev_rn2(3, lev));

    for (x = 11 + mklev_rn2(8, lev); x; x--) {
        mazexy(lev, &mm);
        mkobj_at(mklev_rn2(2, lev) ? GEM_CLASS : 0, lev, mm.x, mm.y,
                 TRUE, rng_for_level(&lev->z));
    }
    for (x = 2 + mklev_rn2(10, lev); x; x--) {
        mazexy(lev, &mm);
        mksobj_at(BOULDER, lev, mm.x, mm.y,
                  TRUE, FALSE, rng_for_level(&lev->z));
    }
    for (x = mklev_rn2(3, lev); x; x--) {
        mazexy(lev, &mm);
        makemon(&mons[PM_MINOTAUR], lev, mm.x, mm.y, MM_ALLLEVRNG);
    }
    for (x = 7 + mklev_rn2(5, lev); x; x--) {
        mazexy(lev, &mm);
        makemon(NULL, lev, mm.x, mm.y, MM_ALLLEVRNG);
    }
    for (x = 7 + mklev_rn2(6, lev); x; x--) {
        mazexy(lev, &mm);
        mkgold(0L, lev, mm.x, mm.y, rng_for_level(&lev->z));
    }
    for (x = 7 + mklev_rn2(6, lev); x; x--)
        mktrap(lev, 0, 1, NULL, NULL);
}

/* Main guts of the maze generator.
   First converts this space to typ, unless it's a door (presumably for special
   levels).
   From the given location, search all four directions for a valid place to
   carve a path (it must be STONE to be valid, see okay()).
   If more than one direction can have a path carved, pick one randomly, carve
   the path there, then recurse on it.
   This algorithm is a depth-first search and is generally known as "Recursive
   Backtracker". It tends to generate long twisty passages with some long and
   some short twisty dead-end side branches. */
void
walkfrom(struct level *lev, int x, int y)
{
    int q, a, dir;
    int dirs[4];

    if (!IS_DOOR(lev->locations[x][y].typ)) {
        /* might still be on edge of MAP, so don't overwrite */
        lev->locations[x][y].typ = ROOM;
        lev->locations[x][y].flags = 0;
    }

    while (1) {
        q = 0;
        for (a = 0; a < 4; a++)
            if (okay(lev, x, y, a))
                dirs[q++] = a;
        if (!q)
            return;
        dir = dirs[mklev_rn2(q, lev)];
        move(&x, &y, dir);
        lev->locations[x][y].typ = ROOM;
        move(&x, &y, dir);
        walkfrom(lev, x, y);
    }
}


static void
move(int *x, int *y, int dir)
{
    switch (dir) {
    case 0:
        --(*y);
        break;
    case 1:
        (*x)++;
        break;
    case 2:
        (*y)++;
        break;
    case 3:
        --(*x);
        break;
    default:
        panic("move: bad direction");
    }
}

/* Finds a random point in the maze area which has the designated floor type of
   the maze (CORR if the corrmaze flag is set, ROOM otherwise).
   This is to avoid creating items, monsters, and features in illegal terrain
   like moats, bunkers, or walls.
   Argument is a pointer to coord that will be set by this function.
   Panics if it cannot find any valid points. */
void
mazexy(struct level *lev, coord * cc)
{
    int cpt = 0;

    do {
        cc->x = 3 + 2 * mklev_rn2((x_maze_max >> 1) - 1, lev);
        cc->y = 3 + 2 * mklev_rn2((y_maze_max >> 1) - 1, lev);
        cpt++;
    } while (cpt < 100 && lev->locations[cc->x][cc->y].typ != ROOM);
    if (cpt >= 100) {
        int x, y;

        /* last try */
        for (x = 0; x < (x_maze_max >> 1) - 1; x++)
            for (y = 0; y < (y_maze_max >> 1) - 1; y++) {
                cc->x = 3 + 2 * x;
                cc->y = 3 + 2 * y;
                if (lev->locations[cc->x][cc->y].typ == ROOM)
                    return;
            }
        panic("mazexy: can't find a place!");
    }
    return;
}

/* put a non-diggable boundary around the initial portion of a level map.
 * assumes that no level will initially put things beyond the isok() range.
 *
 * we can't bound unconditionally on the last line with something in it,
 * because that something might be a niche which was already reachable,
 * so the boundary would be breached
 *
 * we can't bound unconditionally on one beyond the last line, because
 * that provides a window of abuse for WALLIFIED_MAZE special levels
 */
void
bound_digging(struct level *lev)
{
    int x, y;
    unsigned typ;
    struct rm *loc;
    boolean found, nonwall;
    int xmin, xmax, ymin, ymax;

    if (Is_earthlevel(&lev->z))
        return; /* everything diggable here */

    found = nonwall = FALSE;
    for (xmin = 0; !found; xmin++) {
        loc = &lev->locations[xmin][0];
        for (y = 0; y <= ROWNO - 1; y++, loc++) {
            typ = loc->typ;
            if (typ != STONE) {
                found = TRUE;
                if (!IS_WALL(typ))
                    nonwall = TRUE;
            }
        }
    }
    xmin -= (nonwall || !lev->flags.is_maze_lev) ? 2 : 1;
    if (xmin < 0)
        xmin = 0;

    found = nonwall = FALSE;
    for (xmax = COLNO - 1; !found; xmax--) {
        loc = &lev->locations[xmax][0];
        for (y = 0; y <= ROWNO - 1; y++, loc++) {
            typ = loc->typ;
            if (typ != STONE) {
                found = TRUE;
                if (!IS_WALL(typ))
                    nonwall = TRUE;
            }
        }
    }
    xmax += (nonwall || !lev->flags.is_maze_lev) ? 2 : 1;
    if (xmax >= COLNO)
        xmax = COLNO - 1;

    found = nonwall = FALSE;
    for (ymin = 0; !found; ymin++) {
        loc = &lev->locations[xmin][ymin];
        for (x = xmin; x <= xmax; x++, loc += ROWNO) {
            typ = loc->typ;
            if (typ != STONE) {
                found = TRUE;
                if (!IS_WALL(typ))
                    nonwall = TRUE;
            }
        }
    }
    ymin -= (nonwall || !lev->flags.is_maze_lev) ? 2 : 1;

    found = nonwall = FALSE;
    for (ymax = ROWNO - 1; !found; ymax--) {
        loc = &lev->locations[xmin][ymax];
        for (x = xmin; x <= xmax; x++, loc += ROWNO) {
            typ = loc->typ;
            if (typ != STONE) {
                found = TRUE;
                if (!IS_WALL(typ))
                    nonwall = TRUE;
            }
        }
    }
    ymax += (nonwall || !lev->flags.is_maze_lev) ? 2 : 1;

    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            if (y <= ymin || y >= ymax || x <= xmin || x >= xmax) {
                lev->locations[x][y].flags |= W_NONDIGGABLE;
            }
}

/* Creates a magic portal at the given location, pointing to the given dungeon
   number and level. */
void
mkportal(struct level *lev, xchar x, xchar y, xchar todnum, xchar todlevel)
{
    /* a portal "trap" must be matched by a */
    /* portal in the destination dungeon/dlevel */
    struct trap *ttmp = maketrap(lev, x, y, MAGIC_PORTAL,
                                 rng_for_level(&lev->z));

    if (!ttmp) {
        impossible("portal on top of portal??");
        return;
    }
    ttmp->dst.dnum = todnum;
    ttmp->dst.dlevel = todlevel;
    return;
}

/*
 * Special waterlevel stuff in endgame (TH).
 *
 * Some of these functions would probably logically belong to some
 * other source files, but they are all so nicely encapsulated here.
 */

#define CONS_OBJ   0
#define CONS_MON   1
#define CONS_HERO  2
#define CONS_TRAP  3

static struct bubble *bbubbles, *ebubbles;

static struct trap *wportal;

static const int xmin = 3;
static const int ymin = 1;
static const int xmax = 78;
static const int ymax = 20;

/* bubble movement boundaries */
#define bxmin (xmin + 1)
#define bymin (ymin + 1)
#define bxmax (xmax - 1)
#define bymax (ymax - 1)

static void set_wportal(struct level *lev);
static void mk_bubble(struct level *lev, int x, int y, int n);
static void mv_bubble(struct level *lev, struct bubble *b, int dx, int dy,
                      boolean ini);

/*
 * These bit masks make visually pleasing bubbles on a normal aspect
 * 25x80 terminal, which naturally results in them being mathematically
 * anything but symmetric.  For this reason they cannot be computed
 * in situ, either.  The first two elements tell the dimensions of
 * the bubble's bounding box.
 */
static const uchar bm2[] = { 2, 1, 0x3 }, bm3[] = {
3, 2, 0x7, 0x7}, bm4[] = {
4, 3, 0x6, 0xf, 0x6}, bm5[] = {
5, 3, 0xe, 0x1f, 0xe}, bm6[] = {
6, 4, 0x1e, 0x3f, 0x3f, 0x1e}, bm7[] = {
7, 4, 0x3e, 0x7f, 0x7f, 0x3e}, bm8[] = {
8, 4, 0x7e, 0xff, 0xff, 0x7e}, *const bmask[] =
    { bm2, bm3, bm4, bm5, bm6, bm7, bm8 };

void
movebubbles(void)
{
    struct bubble *b;
    int x, y, i, j;
    struct trap *btrap;

    static const struct rm water_pos = {
        S_water, 0, 0, 0, 0, 0, WATER /* typ */ ,
        0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    /* set up the portal the first time bubbles are moved */
    if (!wportal)
        set_wportal(level);

    /* keep attached ball&chain separate from bubble objects */
    if (Punished)
        unplacebc();

    /* 
     * Pick up everything inside of a bubble then fill all bubble
     * locations.
     */

    for (b = bubble_up ? bbubbles : ebubbles; b;
         b = bubble_up ? b->next : b->prev) {
        if (b->cons)
            panic("movebubbles: cons != null");
        for (i = 0, x = b->x; i < (int)b->bm[0]; i++, x++)
            for (j = 0, y = b->y; j < (int)b->bm[1]; j++, y++)
                if (b->bm[j + 2] & (1 << i)) {
                    if (!isok(x, y)) {
                        impossible("movebubbles: bad pos (%d,%d)", x, y);
                        continue;
                    }

                    /* pick up objects, monsters, hero, and traps */
                    if (OBJ_AT(x, y)) {
                        struct obj *olist = NULL, *otmp;
                        struct container *cons =
                            malloc(sizeof (struct container));

                        while ((otmp = level->objects[x][y]) != 0) {
                            remove_object(otmp);
                            otmp->ox = otmp->oy = 0;
                            otmp->nexthere = olist;
                            olist = otmp;
                        }

                        cons->x = x;
                        cons->y = y;
                        cons->what = CONS_OBJ;
                        cons->list = olist;
                        cons->next = b->cons;
                        b->cons = cons;
                    }
                    if (MON_AT(level, x, y)) {
                        struct monst *mon = m_at(level, x, y);
                        struct container *cons =
                            malloc(sizeof (struct container));

                        cons->x = x;
                        cons->y = y;
                        cons->what = CONS_MON;
                        cons->list = mon;

                        cons->next = b->cons;
                        b->cons = cons;

                        if (mon->wormno)
                            remove_worm(mon, level);
                        else
                            remove_monster(level, x, y);

                        newsym(x, y);   /* clean up old position */
                        mon->mx = COLNO;
                        mon->my = ROWNO;
                    }
                    if (!Engulfed && x == u.ux && y == u.uy) {
                        struct container *cons =
                            malloc(sizeof (struct container));

                        cons->x = x;
                        cons->y = y;
                        cons->what = CONS_HERO;
                        cons->list = NULL;

                        cons->next = b->cons;
                        b->cons = cons;
                    }
                    if ((btrap = t_at(level, x, y)) != 0) {
                        struct container *cons =
                            malloc(sizeof (struct container));

                        cons->x = x;
                        cons->y = y;
                        cons->what = CONS_TRAP;
                        cons->list = btrap;

                        cons->next = b->cons;
                        b->cons = cons;
                    }

                    level->locations[x][y] = water_pos;
                }
    }

    /* 
     * Every second time traverse down.  This is because otherwise
     * all the junk that changes owners when bubbles overlap
     * would eventually end up in the last bubble in the chain.
     */

    bubble_up = !bubble_up;
    for (b = bubble_up ? bbubbles : ebubbles; b;
         b = bubble_up ? b->next : b->prev) {
        int rx = rn2(3), ry = rn2(3);

        mv_bubble(level, b, b->dx + 1 - (!b->dx ? rx : (rx ? 1 : 0)),
                  b->dy + 1 - (!b->dy ? ry : (ry ? 1 : 0)), FALSE);
    }

    /* put attached ball&chain back */
    if (Punished)
        placebc();
    turnstate.vision_full_recalc = TRUE;
}

/* when moving in water, possibly (1 in 3) alter the intended destination */
void
water_friction(schar * udx, schar * udy)
{
    int x, y, dx, dy;
    boolean eff = FALSE;

    if (Swimming && rn2(4))
        return; /* natural swimmers have advantage */

    if (*udx && !rn2(!*udy ? 3 : 6)) {  /* 1/3 chance or half that */
        /* cancel delta x and choose an arbitrary delta y value */
        x = u.ux;
        do {
            dy = rn2(3) - 1;    /* -1, 0, 1 */
            y = u.uy + dy;
        } while (dy && (!isok(x, y) || !is_pool(level, x, y)));
        *udx = 0;
        *udy = dy;
        eff = TRUE;
    } else if (*udy && !rn2(!*udx ? 3 : 5)) {   /* 1/3 or 1/5*(5/6) */
        /* cancel delta y and choose an arbitrary delta x value */
        y = u.uy;
        do {
            dx = rn2(3) - 1;    /* -1 .. 1 */
            x = u.ux + dx;
        } while (dx && (!isok(x, y) || !is_pool(level, x, y)));
        *udy = 0;
        *udx = dx;
        eff = TRUE;
    }
    if (eff)
        pline(msgc_substitute, "Water turbulence affects your movements.");
}


void
save_waterlevel(struct memfile *mf)
{
    struct bubble *b;
    int i, n;

    if (!Is_waterlevel(&u.uz))
        return;

    n = 0;
    for (b = bbubbles; b; b = b->next)
        ++n;
    /* We're assuming that the number of bubbles stays constant, as we can't
       reasonably tag it anyway. If they don't, we just end up with a diff
       that's longer than it needs to be. */
    mtag(mf, 0, MTAG_WATERLEVEL);
    mwrite32(mf, n);
    mwrite8(mf, bubble_up);
    for (b = bbubbles; b; b = b->next) {
        mwrite8(mf, b->x);
        mwrite8(mf, b->y);
        mwrite8(mf, b->dx);
        mwrite8(mf, b->dy);
        for (i = 0; i < SIZE(bmask); i++)
            if (b->bm == bmask[i])
                mwrite8(mf, i);
    }
}

void
restore_waterlevel(struct memfile *mf, struct level *lev)
{
    struct bubble *b = NULL, *btmp;
    int i, idx;
    int n;

    if (!Is_waterlevel(&lev->z))
        return;

    bbubbles = NULL;

    set_wportal(lev);
    n = mread32(mf);
    bubble_up = mread8(mf);

    for (i = 0; i < n; i++) {
        btmp = b;
        b = malloc(sizeof (struct bubble));
        b->x = mread8(mf);
        b->y = mread8(mf);
        b->dx = mread8(mf);
        b->dy = mread8(mf);
        idx = mread8(mf);
        b->bm = bmask[idx];
        b->next = NULL;
        b->cons = NULL;

        if (bbubbles) {
            btmp->next = b;
            b->prev = btmp;
        } else {
            bbubbles = b;
            b->prev = NULL;
        }
    }
    ebubbles = b;
    was_waterlevel = TRUE;
}


static const char *
waterbody_impl(xchar x, xchar y, boolean article)
{
    struct rm *loc;
    schar ltyp;

    if (!isok(x, y)) /* should never happen */
        return msgcat(article ? "a " : "", "drink");
    loc = &level->locations[x][y];
    ltyp = loc->typ;

    if (is_lava(level, x, y))
        return "lava";
    else if (is_ice(level, x, y))
        return "ice";
    else if (is_moat(level, x, y))
        return msgcat(article ? "a " : "", "moat");
    else if ((ltyp != POOL) && (ltyp != WATER) && Is_juiblex_level(&u.uz))
        return msgcat(article ? "a " : "", "swamp");
    else if (ltyp == POOL)
        return msgcat(article ? "a " : "", "pool of water");
    else
        return "water";
}


const char *
a_waterbody(xchar x, xchar y)
{
    return waterbody_impl(x, y, TRUE);
}


const char *
waterbody_name(xchar x, xchar y)
{
    return waterbody_impl(x, y, FALSE);
}

static void
set_wportal(struct level *lev)
{
    /* there better be only one magic portal on water level... */
    for (wportal = lev->lev_traps; wportal; wportal = wportal->ntrap)
        if (wportal->ttyp == MAGIC_PORTAL)
            return;
    impossible("set_wportal(): no portal!");
}

void
setup_waterlevel(struct level *lev)
{
    int x, y;
    int xskip, yskip;

    /* set hero's memory to water */

    for (x = xmin; x <= xmax; x++)
        for (y = ymin; y <= ymax; y++)
            lev->locations[x][y].mem_bg = S_water;

    /* make bubbles */

    xskip = 10 + rn2(10);
    yskip = 4 + rn2(4);
    for (x = bxmin; x <= bxmax; x += xskip)
        for (y = bymin; y <= bymax; y += yskip)
            mk_bubble(lev, x, y, rn2(7));
}

void
free_waterlevel(void)
{
    struct bubble *b, *bb;

    /* free bubbles */

    for (b = bbubbles; b; b = bb) {
        bb = b->next;
        free(b);
    }
    bbubbles = ebubbles = NULL;
}


static void
mk_bubble(struct level *lev, int x, int y, int n)
{
    struct bubble *b;

    if (x >= bxmax || y >= bymax)
        return;
    if (n >= SIZE(bmask)) {
        impossible("n too large (mk_bubble)");
        n = SIZE(bmask) - 1;
    }
    b = malloc(sizeof (struct bubble));
    if ((x + (int)bmask[n][0] - 1) > bxmax)
        x = bxmax - bmask[n][0] + 1;
    if ((y + (int)bmask[n][1] - 1) > bymax)
        y = bymax - bmask[n][1] + 1;
    b->x = x;
    b->y = y;
    b->dx = 1 - mklev_rn2(3, lev);
    b->dy = 1 - mklev_rn2(3, lev);
    b->bm = bmask[n];
    b->cons = 0;
    if (!bbubbles)
        bbubbles = b;
    if (ebubbles) {
        ebubbles->next = b;
        b->prev = ebubbles;
    } else
        b->prev = NULL;
    b->next = NULL;
    ebubbles = b;
    mv_bubble(lev, b, 0, 0, TRUE);
}

/*
 * The player, the portal and all other objects and monsters
 * float along with their associated bubbles.  Bubbles may overlap
 * freely, and the contents may get associated with other bubbles in
 * the process.  Bubbles are "sticky", meaning that if the player is
 * in the immediate neighborhood of one, he/she may get sucked inside.
 * This property also makes leaving a bubble slightly difficult.
 */
static void
mv_bubble(struct level *lev, struct bubble *b, int dx, int dy, boolean ini)
{
    int x, y, i, j, colli = 0;
    struct container *cons, *ctemp;

    /* move bubble */
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1) {
        /* pline(msgc_debug, "mv_bubble: dx = %d, dy = %d", dx, dy); */
        dx = sgn(dx);
        dy = sgn(dy);
    }

    /* 
     * collision with level borders?
     *      1 = horizontal border, 2 = vertical, 3 = corner
     */
    if (b->x <= bxmin)
        colli |= 2;
    if (b->y <= bymin)
        colli |= 1;
    if ((int)(b->x + b->bm[0] - 1) >= bxmax)
        colli |= 2;
    if ((int)(b->y + b->bm[1] - 1) >= bymax)
        colli |= 1;

    if (b->x < bxmin) {
        pline(msgc_debug, "bubble xmin: x = %d, xmin = %d", b->x, bxmin);
        b->x = bxmin;
    }
    if (b->y < bymin) {
        pline(msgc_debug, "bubble ymin: y = %d, ymin = %d", b->y, bymin);
        b->y = bymin;
    }
    if ((int)(b->x + b->bm[0] - 1) > bxmax) {
        pline(msgc_debug, "bubble xmax: x = %d, xmax = %d",
              b->x + b->bm[0] - 1, bxmax);
        b->x = bxmax - b->bm[0] + 1;
    }
    if ((int)(b->y + b->bm[1] - 1) > bymax) {
        pline(msgc_debug, "bubble ymax: y = %d, ymax = %d",
              b->y + b->bm[1] - 1, bymax);
        b->y = bymax - b->bm[1] + 1;
    }

    /* bounce if we're trying to move off the border */
    if (b->x == bxmin && dx < 0)
        dx = -dx;
    if (b->x + b->bm[0] - 1 == bxmax && dx > 0)
        dx = -dx;
    if (b->y == bymin && dy < 0)
        dy = -dy;
    if (b->y + b->bm[1] - 1 == bymax && dy > 0)
        dy = -dy;

    b->x += dx;
    b->y += dy;

    /* void positions inside bubble */

    for (i = 0, x = b->x; i < (int)b->bm[0]; i++, x++)
        for (j = 0, y = b->y; j < (int)b->bm[1]; j++, y++)
            if (b->bm[j + 2] & (1 << i)) {
                lev->locations[x][y].typ = AIR;
                lev->locations[x][y].lit = 1;
            }

    /* replace contents of bubble */
    for (cons = b->cons; cons; cons = ctemp) {
        ctemp = cons->next;
        cons->x += dx;
        cons->y += dy;

        switch (cons->what) {
        case CONS_OBJ:{
                struct obj *olist, *otmp;

                for (olist = (struct obj *)cons->list; olist; olist = otmp) {
                    otmp = olist->nexthere;
                    place_object(olist, lev, cons->x, cons->y);
                }
                break;
            }

        case CONS_MON:{
                struct monst *mon = (struct monst *)cons->list;

                mnearto(mon, cons->x, cons->y, TRUE);
                break;
            }

        case CONS_HERO:{
                int ux0 = u.ux, uy0 = u.uy;

                /* change u.ux0 and u.uy0? */
                u.ux = cons->x;
                u.uy = cons->y;
                newsym(ux0, uy0);       /* clean up old position */

                if (MON_AT(lev, cons->x, cons->y)) {
                    mnexto(m_at(lev, cons->x, cons->y));
                }
                break;
            }

        case CONS_TRAP:{
                struct trap *btrap = (struct trap *)cons->list;

                btrap->tx = cons->x;
                btrap->ty = cons->y;
                break;
            }

        default:
            impossible("mv_bubble: unknown bubble contents");
            break;
        }
        free(cons);
    }
    b->cons = 0;

    /* boing? */

    switch (colli) {
    case 1:
        b->dy = -b->dy;
        break;
    case 3:
        b->dy = -b->dy; /* fall through */
    case 2:
        b->dx = -b->dx;
        break;
    default:
        /* sometimes alter direction for fun anyway (higher probability for
           stationary bubbles) */
        if (!ini && ((b->dx || b->dy) ? !rn2(20) : !rn2(5))) {
            b->dx = 1 - rn2(3);
            b->dy = 1 - rn2(3);
        }
    }
}

/*mkmaze.c*/
