/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-16 */
/* Copyright (c) J. C. Collet, M. Stephenson and D. Cohrs, 1992   */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "sp_lev.h"

#define HEIGHT  (ROWNO - 1)
#define WIDTH   (COLNO - 1)

static void init_map(struct level *lev, schar bg_typ);
static void init_fill(struct level *lev, schar bg_typ, schar fg_typ);
static schar get_map(struct level *lev, int col, int row, schar bg_typ);
static void pass_one(struct level *lev, schar, schar);
static void pass_two(struct level *lev, schar, schar);
static void pass_three(struct level *lev, schar, schar);
static void wallify_map(struct level *lev);
static void join_map(struct level *lev, schar, schar);
static void finish_map(struct level *lev, schar, schar, xchar, xchar);
static void remove_room(struct level *lev, unsigned roomno);

static char *new_locations;
int min_rx, max_rx, min_ry, max_ry;     /* rectangle bounds for regions */
static int n_loc_filled;

static void
init_map(struct level *lev, schar bg_typ)
{
    int i, j;

    for (i = 0; i < COLNO; i++)
        for (j = 0; j < ROWNO; j++)
            lev->locations[i][j].typ = bg_typ;
}

static void
init_fill(struct level *lev, schar bg_typ, schar fg_typ)
{
    int i, j;
    long limit, count;

    limit = (WIDTH * HEIGHT * 2) / 5;
    count = 0;
    while (count < limit) {
        i = 1 + mklev_rn2(WIDTH - 1, lev);
        j = 1 + mklev_rn2(HEIGHT - 1, lev);
        if (lev->locations[i][j].typ == bg_typ) {
            lev->locations[i][j].typ = fg_typ;
            count++;
        }
    }
}

static schar
get_map(struct level *lev, int col, int row, schar bg_typ)
{
    if (col <= 0 || row < 0 || col > WIDTH || row >= HEIGHT)
        return bg_typ;
    return lev->locations[col][row].typ;
}

static const int dirs[16] = {
    -1, -1 /**/, -1, 0 /**/, -1, 1 /**/,
    0, -1 /**/, 0, 1 /**/,
    1, -1 /**/, 1, 0 /**/, 1, 1
};

static void
pass_one(struct level *lev, schar bg_typ, schar fg_typ)
{
    int i, j;
    short count, dr;

    for (i = 1; i <= WIDTH; i++)
        for (j = 1; j < HEIGHT; j++) {
            for (count = 0, dr = 0; dr < 8; dr++)
                if (get_map
                    (lev, i + dirs[dr * 2], j + dirs[(dr * 2) + 1], bg_typ)
                    == fg_typ)
                    count++;

            switch (count) {
            case 0:    /* death */
            case 1:
            case 2:
                lev->locations[i][j].typ = bg_typ;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
                lev->locations[i][j].typ = fg_typ;
                break;
            default:
                break;
            }
        }
}

#define new_loc(i,j) *(new_locations+ ((j)*(WIDTH+1)) + (i))

static void
pass_two(struct level *lev, schar bg_typ, schar fg_typ)
{
    int i, j;
    short count, dr;

    for (i = 1; i <= WIDTH; i++)
        for (j = 1; j < HEIGHT; j++) {
            for (count = 0, dr = 0; dr < 8; dr++)
                if (get_map
                    (lev, i + dirs[dr * 2], j + dirs[(dr * 2) + 1], bg_typ)
                    == fg_typ)
                    count++;
            if (count == 5)
                new_loc(i, j) = bg_typ;
            else
                new_loc(i, j) = get_map(lev, i, j, bg_typ);
        }

    for (i = 1; i <= WIDTH; i++)
        for (j = 1; j < HEIGHT; j++)
            lev->locations[i][j].typ = new_loc(i, j);
}

static void
pass_three(struct level *lev, schar bg_typ, schar fg_typ)
{
    int i, j;
    short count, dr;

    for (i = 1; i <= WIDTH; i++)
        for (j = 1; j < HEIGHT; j++) {
            for (count = 0, dr = 0; dr < 8; dr++)
                if (get_map
                    (lev, i + dirs[dr * 2], j + dirs[(dr * 2) + 1], bg_typ)
                    == fg_typ)
                    count++;
            if (count < 3)
                new_loc(i, j) = bg_typ;
            else
                new_loc(i, j) = get_map(lev, i, j, bg_typ);
        }

    for (i = 1; i <= WIDTH; i++)
        for (j = 1; j < HEIGHT; j++)
            lev->locations[i][j].typ = new_loc(i, j);
}

/*
 * use a flooding algorithm to find all locations that should
 * have the same rm number as the current location.
 * if anyroom is TRUE, use IS_ROOM to check room membership instead of
 * exactly matching level->locations[sx][sy].typ and walls are included as well.
 */
void
flood_fill_rm(struct level *lev, int sx, int sy, int rmno, boolean lit,
              boolean anyroom)
{
    int i;
    int nx;
    schar fg_typ = lev->locations[sx][sy].typ;

    /* back up to find leftmost uninitialized location */
    while (sx >= 0 &&
           (anyroom ? IS_ROOM(lev->locations[sx][sy].typ) :
            lev->locations[sx][sy].typ == fg_typ) &&
           (int)lev->locations[sx][sy].roomno != rmno)
        sx--;
    sx++;       /* compensate for extra decrement */

    /* assume sx,sy is valid */
    if (sx < min_rx)
        min_rx = sx;
    if (sy < min_ry)
        min_ry = sy;

    for (i = sx; i <= WIDTH && lev->locations[i][sy].typ == fg_typ; i++) {
        lev->locations[i][sy].roomno = rmno;
        lev->locations[i][sy].lit = lit;
        if (anyroom) {
            /* add walls to room as well */
            int ii, jj;

            for (ii = (i == sx ? i - 1 : i); ii <= i + 1; ii++)
                for (jj = sy - 1; jj <= sy + 1; jj++)
                    if (isok(ii, jj) &&
                        (IS_WALL(lev->locations[ii][jj].typ) ||
                         IS_DOOR(lev->locations[ii][jj].typ))) {
                        lev->locations[ii][jj].edge = 1;
                        if (lit)
                            lev->locations[ii][jj].lit = lit;
                        if ((int)lev->locations[ii][jj].roomno != rmno)
                            lev->locations[ii][jj].roomno = SHARED;
                    }
        }
        n_loc_filled++;
    }
    nx = i;

    if (isok(sx, sy - 1)) {
        for (i = sx; i < nx; i++)
            if (lev->locations[i][sy - 1].typ == fg_typ) {
                if ((int)lev->locations[i][sy - 1].roomno != rmno)
                    flood_fill_rm(lev, i, sy - 1, rmno, lit, anyroom);
            } else {
                if ((i > sx || isok(i - 1, sy - 1)) &&
                    lev->locations[i - 1][sy - 1].typ == fg_typ) {
                    if ((int)lev->locations[i - 1][sy - 1].roomno != rmno)
                        flood_fill_rm(lev, i - 1, sy - 1, rmno, lit, anyroom);
                }
                if ((i < nx - 1 || isok(i + 1, sy - 1)) &&
                    lev->locations[i + 1][sy - 1].typ == fg_typ) {
                    if ((int)lev->locations[i + 1][sy - 1].roomno != rmno)
                        flood_fill_rm(lev, i + 1, sy - 1, rmno, lit, anyroom);
                }
            }
    }
    if (isok(sx, sy + 1)) {
        for (i = sx; i < nx; i++)
            if (lev->locations[i][sy + 1].typ == fg_typ) {
                if ((int)lev->locations[i][sy + 1].roomno != rmno)
                    flood_fill_rm(lev, i, sy + 1, rmno, lit, anyroom);
            } else {
                if ((i > sx || isok(i - 1, sy + 1)) &&
                    lev->locations[i - 1][sy + 1].typ == fg_typ) {
                    if ((int)lev->locations[i - 1][sy + 1].roomno != rmno)
                        flood_fill_rm(lev, i - 1, sy + 1, rmno, lit, anyroom);
                }
                if ((i < nx - 1 || isok(i + 1, sy + 1)) &&
                    lev->locations[i + 1][sy + 1].typ == fg_typ) {
                    if ((int)lev->locations[i + 1][sy + 1].roomno != rmno)
                        flood_fill_rm(lev, i + 1, sy + 1, rmno, lit, anyroom);
                }
            }
    }

    if (nx > max_rx)
        max_rx = nx - 1;        /* nx is just past valid region */
    if (sy > max_ry)
        max_ry = sy;
}

/*
 * If we have drawn a map without walls, this allows us to
 * auto-magically wallify it.  Taken from lev_main.c.
 */
static void
wallify_map(struct level *lev)
{

    int x, y, xx, yy;

    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            if (lev->locations[x][y].typ == STONE) {
                for (yy = y - 1; yy <= y + 1; yy++)
                    for (xx = x - 1; xx <= x + 1; xx++)
                        if (isok(xx, yy) &&
                            lev->locations[xx][yy].typ == ROOM) {
                            if (yy != y)
                                lev->locations[x][y].typ = HWALL;
                            else
                                lev->locations[x][y].typ = VWALL;
                        }
            }
}

static void
join_map(struct level *lev, schar bg_typ, schar fg_typ)
{
    struct mkroom *croom, *croom2;

    int i, j;
    int sx, sy;
    coord sm, em;

    enum rng rng = rng_for_level(&lev->z);

    /* first, use flood filling to find all of the regions that need joining */
    for (i = 1; i <= WIDTH; i++)
        for (j = 1; j < HEIGHT; j++) {
            if (lev->locations[i][j].typ == fg_typ &&
                lev->locations[i][j].roomno == NO_ROOM) {
                min_rx = max_rx = i;
                min_ry = max_ry = j;
                n_loc_filled = 0;
                flood_fill_rm(lev, i, j, lev->nroom + ROOMOFFSET, FALSE, FALSE);
                if (n_loc_filled > 3) {
                    add_room(lev, min_rx, min_ry, max_rx, max_ry, FALSE, OROOM,
                             TRUE);
                    lev->rooms[lev->nroom - 1].irregular = TRUE;
                    if (lev->nroom >= (MAXNROFROOMS * 2))
                        goto joinm;
                } else {
                    /* 
                     * it's a tiny hole; erase it from the map to avoid
                     * having the player end up here with no way out.
                     */
                    for (sx = min_rx; sx <= max_rx; sx++)
                        for (sy = min_ry; sy <= max_ry; sy++)
                            if ((int)lev->locations[sx][sy].roomno ==
                                lev->nroom + ROOMOFFSET) {
                                lev->locations[sx][sy].typ = bg_typ;
                                lev->locations[sx][sy].roomno = NO_ROOM;
                            }
                }
            }
        }

joinm:
    /* 
     * Ok, now we can actually join the regions with fg_typ's.
     * The rooms are already sorted due to the previous loop,
     * so don't call sort_rooms(), which can screw up the roomno's
     * validity in the level->locations structure.
     */
    for (croom = &lev->rooms[0], croom2 = croom + 1;
         croom2 < &lev->rooms[lev->nroom];) {
        /* pick random starting and end locations for "corridor" */
        if (!somexy(lev, croom, &sm, rng) || !somexy(lev, croom2, &em, rng)) {
            /* ack! -- the level is going to be busted */
            /* arbitrarily pick centers of both rooms and hope for the best */
            impossible("No start/end room loc in join_map.");
            sm.x = croom->lx + ((croom->hx - croom->lx) / 2);
            sm.y = croom->ly + ((croom->hy - croom->ly) / 2);
            em.x = croom2->lx + ((croom2->hx - croom2->lx) / 2);
            em.y = croom2->ly + ((croom2->hy - croom2->ly) / 2);
        }

        dig_corridor(lev, &sm, &em, FALSE, fg_typ, bg_typ);

        /* choose next region to join */
        /* only increment croom if croom and croom2 are non-overlapping */
        if (croom2->lx > croom->hx ||
            ((croom2->ly > croom->hy || croom2->hy < croom->ly) &&
             mklev_rn2(3, lev))) {
            croom = croom2;
        }
        croom2++;       /* always increment the next room */
    }
}

static void
finish_map(struct level *lev, schar fg_typ, schar bg_typ, boolean lit,
           boolean walled)
{
    int i, j;

    if (walled)
        wallify_map(lev);

    if (lit) {
        for (i = 0; i < COLNO; i++)
            for (j = 0; j < ROWNO; j++)
                if ((!IS_ROCK(fg_typ) && lev->locations[i][j].typ == fg_typ) ||
                    (!IS_ROCK(bg_typ) && lev->locations[i][j].typ == bg_typ) ||
                    (bg_typ == TREE && lev->locations[i][j].typ == bg_typ) ||
                    (walled && IS_WALL(lev->locations[i][j].typ)))
                    lev->locations[i][j].lit = TRUE;
        for (i = 0; i < lev->nroom; i++)
            lev->rooms[i].rlit = 1;
    }
    /* light lava even if everything's otherwise unlit */
    for (i = 0; i < COLNO; i++)
        for (j = 0; j < ROWNO; j++)
            if (lev->locations[i][j].typ == LAVAPOOL)
                lev->locations[i][j].lit = TRUE;
}

/*
 * When level processed by join_map is overlaid by a MAP, some rooms may no
 * longer be valid.  All rooms in the region lx <= x < hx, ly <= y < hy are
 * removed.  Rooms partially in the region are truncated.  This function
 * must be called before the REGIONs or ROOMs of the map are processed, or
 * those rooms will be removed as well.  Assumes roomno fields in the
 * region are already cleared, and roomno and irregular fields outside the
 * region are all set.
 */
void
remove_rooms(struct level *lev, int lx, int ly, int hx, int hy)
{
    int i;
    struct mkroom *croom;

    for (i = lev->nroom - 1; i >= 0; --i) {
        croom = &lev->rooms[i];
        if (croom->hx < lx || croom->lx >= hx || croom->hy < ly ||
            croom->ly >= hy)
            continue;   /* no overlap */

        if (croom->lx < lx || croom->hx >= hx ||
            croom->ly < ly || croom->hy >= hy) { /* partial overlap */
            /* TODO: ensure remaining parts of room are still joined */

            if (!croom->irregular)
                impossible("regular room in joined map");
        } else {
            /* total overlap, remove the room */
            remove_room(lev, (unsigned)i);
        }
    }
}

/*
 * Remove roomno from the level->rooms array, decrementing level->nroom.  Also
 * updates all level roomno values of affected higher numbered rooms.  Assumes
 * level structure contents corresponding to roomno have already been reset.
 * Currently handles only the removal of rooms that have no subrooms.
 */
static void
remove_room(struct level *lev, unsigned roomno)
{
    struct mkroom *croom = &lev->rooms[roomno];
    struct mkroom *maxroom = &lev->rooms[--lev->nroom];
    int i, j;
    unsigned oroomno;

    if (croom != maxroom) {
        /* since the order in the array only matters for making corridors, copy 
           the last room over the one being removed on the assumption that
           corridors have already been dug. */
        memcpy(croom, (void *)maxroom, sizeof (struct mkroom));

        /* since maxroom moved, update affected level roomno values */
        oroomno = lev->nroom + ROOMOFFSET;
        roomno += ROOMOFFSET;
        for (i = croom->lx; i <= croom->hx; ++i)
            for (j = croom->ly; j <= croom->hy; ++j) {
                if (lev->locations[i][j].roomno == oroomno)
                    lev->locations[i][j].roomno = roomno;
            }
    }

    maxroom->hx = -1;   /* just like add_room */
}

#define N_P1_ITER 1     /* tune map generation via this value */
#define N_P2_ITER 1     /* tune map generation via this value */
#define N_P3_ITER 2     /* tune map smoothing via this value */

void
mkmap(struct level *lev, lev_init *init_lev)
{
    schar bg_typ = init_lev->bg, fg_typ = init_lev->fg;
    boolean smooth = init_lev->smoothed, join = init_lev->joined;
    xchar lit = init_lev->lit, walled = init_lev->walled;
    int i;

    if (lit < 0)
        lit = (mklev_rn2(1 + abs(depth(&u.uz)), lev) < 10 &&
               mklev_rn2(77, lev)) ? 1 : 0;
    else if (lit == 2) {
        /* Always bright above Minetown. Always dark below
           TODO: Figure out a better way to specify dungon level */
        s_level *minetownslev = find_level("minetn");
        if (!minetownslev)
            panic("Failed to find minetown.");

        lit = !!(depth(&lev->z) < depth(&minetownslev->dlevel));
    }

    new_locations = malloc((WIDTH + 1) * HEIGHT);

    init_map(lev, bg_typ);
    init_fill(lev, bg_typ, fg_typ);

    for (i = 0; i < N_P1_ITER; i++)
        pass_one(lev, bg_typ, fg_typ);

    for (i = 0; i < N_P2_ITER; i++)
        pass_two(lev, bg_typ, fg_typ);

    if (smooth)
        for (i = 0; i < N_P3_ITER; i++)
            pass_three(lev, bg_typ, fg_typ);

    if (join)
        join_map(lev, bg_typ, fg_typ);

    finish_map(lev, fg_typ, bg_typ, (boolean) lit, (boolean) walled);
    /* a walled, joined level is cavernous, not mazelike -dlc */
    if (walled && join) {
        lev->flags.is_maze_lev = FALSE;
        lev->flags.is_cavernous_lev = TRUE;
    }
    free(new_locations);
}

/*mkmap.c*/

