/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright 1988, 1989 by Ken Arromdee                           */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Support code for "rogue"-style level.
 */

#include "hack.h"

struct rogueroom {
    xchar rlx, rly;
    xchar dx, dy;
    boolean real;
    uchar doortable;
    int nroom;  /* Only meaningful for "real" rooms */
};

#define UP 1
#define DOWN 2
#define LEFT 4
#define RIGHT 8

static struct rogueroom r[3][3];
static void roguejoin(struct level *lev, int, int, int, int, int);
static void roguecorr(struct level *lev, int, int, int);
static void miniwalk(struct level *lev, int, int);
static void corr(struct level *lev, int, int);


static void
roguejoin(struct level *lev, int x1, int y1, int x2, int y2, int horiz)
{
    int x, y, middle;

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
    if (horiz) {
        middle = x1 + mklev_rn2(x2 - x1 + 1, lev);
        for (x = MIN(x1, middle); x <= MAX(x1, middle); x++)
            corr(lev, x, y1);
        for (y = MIN(y1, y2); y <= MAX(y1, y2); y++)
            corr(lev, middle, y);
        for (x = MIN(middle, x2); x <= MAX(middle, x2); x++)
            corr(lev, x, y2);
    } else {
        middle = y1 + mklev_rn2(y2 - y1 + 1, lev);
        for (y = MIN(y1, middle); y <= MAX(y1, middle); y++)
            corr(lev, x1, y);
        for (x = MIN(x1, x2); x <= MAX(x1, x2); x++)
            corr(lev, x, middle);
        for (y = MIN(middle, y2); y <= MAX(middle, y2); y++)
            corr(lev, x2, y);
    }
}


static void
roguecorr(struct level *lev, int x, int y, int dir)
{
    int fromx, fromy, tox, toy;

    if (dir == DOWN) {
        r[x][y].doortable &= ~DOWN;
        if (!r[x][y].real) {
            fromx = r[x][y].rlx;
            fromy = r[x][y].rly;
            fromx += 1 + 26 * x;
            fromy += 7 * y;
        } else {
            fromx = r[x][y].rlx + mklev_rn2(r[x][y].dx, lev);
            fromy = r[x][y].rly + r[x][y].dy;
            fromx += 1 + 26 * x;
            fromy += 7 * y;
            if (!IS_WALL(lev->locations[fromx][fromy].typ))
                impossible("down: no wall at %d,%d?", fromx, fromy);
            dodoor(lev, fromx, fromy, &lev->rooms[r[x][y].nroom]);
            lev->locations[fromx][fromy].doormask = D_NODOOR;
            fromy++;
        }
        if (y >= 2) {
            impossible("down door from %d,%d going nowhere?", x, y);
            return;
        }
        y++;
        r[x][y].doortable &= ~UP;
        if (!r[x][y].real) {
            tox = r[x][y].rlx;
            toy = r[x][y].rly;
            tox += 1 + 26 * x;
            toy += 7 * y;
        } else {
            tox = r[x][y].rlx + mklev_rn2(r[x][y].dx, lev);
            toy = r[x][y].rly - 1;
            tox += 1 + 26 * x;
            toy += 7 * y;
            if (!IS_WALL(lev->locations[tox][toy].typ))
                impossible("up: no wall at %d,%d?", tox, toy);
            dodoor(lev, tox, toy, &lev->rooms[r[x][y].nroom]);
            lev->locations[tox][toy].doormask = D_NODOOR;
            toy--;
        }
        roguejoin(lev, fromx, fromy, tox, toy, FALSE);
        return;
    } else if (dir == RIGHT) {
        r[x][y].doortable &= ~RIGHT;
        if (!r[x][y].real) {
            fromx = r[x][y].rlx;
            fromy = r[x][y].rly;
            fromx += 1 + 26 * x;
            fromy += 7 * y;
        } else {
            fromx = r[x][y].rlx + r[x][y].dx;
            fromy = r[x][y].rly + mklev_rn2(r[x][y].dy, lev);
            fromx += 1 + 26 * x;
            fromy += 7 * y;
            if (!IS_WALL(lev->locations[fromx][fromy].typ))
                impossible("down: no wall at %d,%d?", fromx, fromy);
            dodoor(lev, fromx, fromy, &lev->rooms[r[x][y].nroom]);
            lev->locations[fromx][fromy].doormask = D_NODOOR;
            fromx++;
        }
        if (x >= 2) {
            impossible("right door from %d,%d going nowhere?", x, y);
            return;
        }
        x++;
        r[x][y].doortable &= ~LEFT;
        if (!r[x][y].real) {
            tox = r[x][y].rlx;
            toy = r[x][y].rly;
            tox += 1 + 26 * x;
            toy += 7 * y;
        } else {
            tox = r[x][y].rlx - 1;
            toy = r[x][y].rly + mklev_rn2(r[x][y].dy, lev);
            tox += 1 + 26 * x;
            toy += 7 * y;
            if (!IS_WALL(lev->locations[tox][toy].typ))
                impossible("left: no wall at %d,%d?", tox, toy);
            dodoor(lev, tox, toy, &lev->rooms[r[x][y].nroom]);
            lev->locations[tox][toy].doormask = D_NODOOR;
            tox--;
        }
        roguejoin(lev, fromx, fromy, tox, toy, TRUE);
        return;
    } else
        impossible("corridor in direction %d?", dir);
}


/* Modified walkfrom() from mkmaze.c */
static void
miniwalk(struct level *lev, int x, int y)
{
    int q, dir;
    int dirs[4];

    while (1) {
        q = 0;
#define doorhere (r[x][y].doortable)
        if (x > 0 && (!(doorhere & LEFT)) &&
            (!r[x - 1][y].doortable || !mklev_rn2(10, lev)))
            dirs[q++] = 0;
        if (x < 2 && (!(doorhere & RIGHT)) &&
            (!r[x + 1][y].doortable || !mklev_rn2(10, lev)))
            dirs[q++] = 1;
        if (y > 0 && (!(doorhere & UP)) && (!r[x][y - 1].doortable ||
                                            !mklev_rn2(10, lev)))
            dirs[q++] = 2;
        if (y < 2 && (!(doorhere & DOWN)) &&
            (!r[x][y + 1].doortable || !mklev_rn2(10, lev)))
            dirs[q++] = 3;
        /* Rogue levels aren't just 3 by 3 mazes; they have some extra
           connections, thus that 1/10 chance */
        if (!q)
            return;
        dir = dirs[mklev_rn2(q, lev)];
        switch (dir) {  /* Move in direction */
        case 0:
            doorhere |= LEFT;
            x--;
            doorhere |= RIGHT;
            break;
        case 1:
            doorhere |= RIGHT;
            x++;
            doorhere |= LEFT;
            break;
        case 2:
            doorhere |= UP;
            y--;
            doorhere |= DOWN;
            break;
        case 3:
            doorhere |= DOWN;
            y++;
            doorhere |= UP;
            break;
        }
        miniwalk(lev, x, y);
    }
}


void
makeroguerooms(struct level *lev, int *smeq)
{
    int x, y;

    /* Rogue levels are structured 3 by 3, with each section containing a room
       or an intersection.  The minimum width is 2 each way. One difference
       between these and "real" Rogue levels: real Rogue uses 24 rows and
       NetHack only 23.  So we cheat a bit by making the second row of rooms
       not as deep. Each normal space has 6/7 rows and 25 columns in which a
       room may actually be placed.  Walls go from rows 0-5/6 and columns 0-24.
       Not counting walls, the room may go in rows 1-5 and columns 1-23
       (numbering starting at 0).  A room coordinate of this type may be
       converted to a level coordinate by adding 1+28*x to the column, and 7*y
       to the row.  (The 1 is because column 0 isn't used [we only use 1-78]).
       Room height may be 2-4 (2-5 on last row), length 2-23 (not counting
       walls) */
#define here r[x][y]

    lev->nroom = 0;
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++) {
            /* Note: we want to insure at least 1 room.  So, if the first 8 are 
               all dummies, force the last to be a room. */
            if (!mklev_rn2(5, lev) && (lev->nroom || (x < 2 && y < 2))) {
                /* Arbitrary: dummy rooms may only go where real ones do. */
                here.real = FALSE;
                here.rlx = 2 + mklev_rn2(22, lev);
                here.rly = 2 + mklev_rn2((y == 2) ? 4 : 3, lev);
            } else {
                here.real = TRUE;
                here.dx = 2 + mklev_rn2(22, lev);      /* 2-23 long, plus walls */
                here.dy = 2 + mklev_rn2((y == 2) ? 4 : 3, lev);
                    /* 2-5 high, plus walls */

                /* boundaries of room floor */
                here.rlx = 1 + mklev_rn2(23 - here.dx + 1, lev);
                here.rly = 1 + mklev_rn2(((y == 2) ? 5 : 4) - here.dy + 1, lev);
                lev->nroom++;
            }
            here.doortable = 0;
        }
    x = mklev_rn2(3, lev);
    y = mklev_rn2(3, lev);
    miniwalk(lev, x, y);
    lev->nroom = 0;
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++) {
            if (here.real) {    /* Make a room */
                int lowx, lowy, hix, hiy;

                r[x][y].nroom = lev->nroom;
                smeq[lev->nroom] = lev->nroom;

                lowx = 1 + 26 * x + here.rlx;
                lowy = 7 * y + here.rly;
                hix = 1 + 26 * x + here.rlx + here.dx - 1;
                hiy = 7 * y + here.rly + here.dy - 1;
                /* Strictly speaking, it should be lit only if above level 10,
                   but since Rogue rooms are only encountered below level 10,
                   use !rn2(7). */
                add_room(lev, lowx, lowy, hix, hiy,
                         (boolean)!mklev_rn2(7, lev), OROOM, FALSE);
            }
        }

    /* Now, add connecting corridors. */
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++) {
            if (here.doortable & DOWN)
                roguecorr(lev, x, y, DOWN);
            if (here.doortable & RIGHT)
                roguecorr(lev, x, y, RIGHT);
            if (here.doortable & LEFT)
                impossible("left end of %d, %d never connected?", x, y);
            if (here.doortable & UP)
                impossible("up end of %d, %d never connected?", x, y);
        }
}


static void
corr(struct level *lev, int x, int y)
{
    if (mklev_rn2(50, lev)) {
        lev->locations[x][y].typ = CORR;
    } else {
        lev->locations[x][y].typ = SCORR;
    }
}


void
makerogueghost(struct level *lev)
{
    struct monst *ghost;
    struct obj *ghostobj;
    struct mkroom *croom;
    int x, y;
    enum rng rng = rng_for_level(&lev->z);

    if (!lev->nroom)
        return; /* Should never happen */
    croom = &lev->rooms[mklev_rn2(lev->nroom, lev)];
    x = somex(croom, rng);
    y = somey(croom, rng);

    if (!(ghost = makemon(&mons[PM_GHOST], lev, x, y, MM_ALLLEVRNG)))
        return;
    ghost->msleeping = 1;
    christen_monst(ghost, roguename());

    if (mklev_rn2(4, lev)) {
        ghostobj = mksobj_at(FOOD_RATION, lev, x, y, FALSE, FALSE, rng);
        ghostobj->quan = 1 + mklev_rn2(7, lev);
        ghostobj->owt = weight(ghostobj);
    }
    if (mklev_rn2(2, lev)) {
        ghostobj = mksobj_at(MACE, lev, x, y, FALSE, FALSE, rng);
        ghostobj->spe = 1 + mklev_rn2(3, lev);
        if (mklev_rn2(4, lev))
            curse(ghostobj);
    } else {
        ghostobj = mksobj_at(TWO_HANDED_SWORD, lev, x, y, FALSE, FALSE, rng);
        ghostobj->spe = mklev_rn2(5, lev) - 1;
        if (mklev_rn2(4, lev))
            curse(ghostobj);
    }
    ghostobj = mksobj_at(BOW, lev, x, y, FALSE, FALSE, rng);
    ghostobj->spe = 1;
    if (mklev_rn2(4, lev))
        curse(ghostobj);

    ghostobj = mksobj_at(ARROW, lev, x, y, FALSE, FALSE, rng);
    ghostobj->spe = 0;
    ghostobj->quan = (long)rn1(10, 25);
    ghostobj->owt = weight(ghostobj);
    if (mklev_rn2(4, lev))
        curse(ghostobj);

    if (mklev_rn2(2, lev)) {
        ghostobj = mksobj_at(RING_MAIL, lev, x, y, FALSE, FALSE, rng);
        ghostobj->spe = mklev_rn2(3, lev);
        if (!mklev_rn2(3, lev))
            ghostobj->oerodeproof = TRUE;
        if (mklev_rn2(4, lev))
            curse(ghostobj);
    } else {
        ghostobj = mksobj_at(PLATE_MAIL, lev, x, y, FALSE, FALSE, rng);
        ghostobj->spe = mklev_rn2(5, lev) - 1;
        if (!mklev_rn2(3, lev))
            ghostobj->oerodeproof = TRUE;
        if (mklev_rn2(4, lev))
            curse(ghostobj);
    }
    if (mklev_rn2(2, lev)) {
        ghostobj = mksobj_at(FAKE_AMULET_OF_YENDOR, lev, x, y, TRUE, FALSE,
                             rng);
        ghostobj->known = TRUE;
    }
}

/*extralev.c*/

