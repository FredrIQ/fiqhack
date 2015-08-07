/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/*      Copyright (c) 1989 by Jean-Christophe Collet */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the various functions that are related to the special
 * levels.
 * It contains also the special level loader.
 *
 */

#include "hack.h"
#include "dlb.h"

#include "sp_lev.h"
#include "rect.h"

static void get_room_loc(struct level *lev, schar *, schar *, struct mkroom *);
static void get_free_room_loc(struct level *lev, schar * x, schar * y,
                              struct mkroom *croom);
static void create_trap(struct level *lev, trap * t, struct mkroom *croom);
static int noncoalignment(aligntyp, struct level *);
static void create_monster(struct level *lev, monster *, struct mkroom *);
static void create_object(struct level *lev, object *, struct mkroom *);
static void create_engraving(struct level *lev, engraving *, struct mkroom *);
static void create_stairs(struct level *lev, stair *, struct mkroom *);
static void create_altar(struct level *lev, altar *, struct mkroom *);
static void create_gold(struct level *lev, gold *, struct mkroom *);
static void create_feature(struct level *lev, int, int, struct mkroom *, int);
static boolean search_door(struct mkroom *, xchar *, xchar *, xchar, int);
static void fix_stair_rooms(struct level *lev);
static void create_corridor(struct level *lev, corridor *, int *);

static boolean create_subroom(struct level *lev, struct mkroom *, xchar, xchar,
                              xchar, xchar, xchar, xchar);

#define mrn2(x) mklev_rn2(x, lev)
#define mrng()  rng_for_level(&lev->z)

#define LEFT    1
#define H_LEFT  2
#define CENTER  3
#define H_RIGHT 4
#define RIGHT   5

#define TOP     1
#define BOTTOM  5

#define sq(x)   ((x)*(x))

#define XLIM    4
#define YLIM    3

#define Fread(ptr, size, count, stream) \
    if (dlb_fread(ptr,size,count,stream) != count) goto err_out;
#define Fgetc                            (schar)dlb_fgetc
#define New(type)                        malloc(sizeof(type))
#define NewTab(type, size)               malloc(sizeof(type *) * (unsigned)size)
#define Free(ptr)                        if (ptr) free((ptr))

static walk walklist[50];

static char Map[COLNO][ROWNO];
static char robjects[10], rloc_x[10], rloc_y[10], rmonst[10];
static const aligntyp init_ralign[3] = { AM_CHAOTIC, AM_NEUTRAL, AM_LAWFUL };

static aligntyp ralign[3];
static xchar xstart, ystart;
static char xsize, ysize;

static void set_wall_property(struct level *lev, xchar, xchar, xchar, xchar,
                              int);
static int rnddoor(struct level *lev);
static int rndtrap(struct level *lev);
static void get_location(struct level *lev, schar * x, schar * y, int humidity);
static boolean is_ok_location(struct level *lev, schar, schar, int);
static void sp_lev_shuffle(char *, char *, int, struct level *lev);
static void light_region(struct level *lev, region * tmpregion);
static void load_common_data(struct level *lev, dlb *, int);
static void load_one_monster(dlb *, monster *);
static void load_one_object(dlb *, object *);
static void load_one_engraving(dlb *, engraving *);
static boolean load_rooms(struct level *lev, dlb *, int *);
static void maze1xy(struct level *lev, coord * m, int humidity);
static boolean load_maze(struct level *lev, dlb * fp);
static void create_door(struct level *lev, room_door *, struct mkroom *);
static void free_rooms(room **, int);
static void build_room(struct level *lev, room *, room *, int *);

static char *lev_message = 0;
static lev_region *lregions = 0;
static int num_lregions = 0;
static lev_init init_lev;

/*
 * Make walls of the area (x1, y1, x2, y2) non diggable/non passwall-able
 */

static void
set_wall_property(struct level *lev, xchar x1, xchar y1, xchar x2, xchar y2,
                  int prop)
{
    xchar x, y;

    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            if (IS_STWALL(lev->locations[x][y].typ))
                lev->locations[x][y].wall_info |= prop;
}

/*
 * Choose randomly the state (nodoor, open, closed or locked) for a door
 */
static int
rnddoor(struct level *lev)
{
    int i = 1 << mrn2(5);

    i >>= 1;
    return i;
}

/*
 * Select a random trap
 */
static int
rndtrap(struct level *lev)
{
    int rtrap;

    do {
        rtrap = 1 + mrn2(TRAPNUM - 1);
        switch (rtrap) {
        case HOLE:     /* no random holes on special levels */
        case VIBRATING_SQUARE:
        case MAGIC_PORTAL:
            rtrap = NO_TRAP;
            break;
        case TRAPDOOR:
            if (!can_dig_down(lev))
                rtrap = NO_TRAP;
            break;
        case LEVEL_TELEP:
        case TELEP_TRAP:
            if (lev->flags.noteleport)
                rtrap = NO_TRAP;
            break;
        case ROLLING_BOULDER_TRAP:
        case ROCKTRAP:
            if (In_endgame(&lev->z))
                rtrap = NO_TRAP;
            break;
        }
    } while (rtrap == NO_TRAP);
    return rtrap;
}

/*
 * Coordinates in special level files are handled specially:
 *
 *      if x or y is -11, we generate a random coordinate.
 *      if x or y is between -1 and -10, we read one from the corresponding
 *      register (x0, x1, ... x9).
 *      if x or y is nonnegative, we convert it from relative to the local map
 *      to global coordinates.
 *      The "humidity" flag is used to insure that engravings aren't
 *      created underwater, or eels on dry land.
 */
#define DRY     0x1
#define WET     0x2


static void
get_location(struct level *lev, schar * x, schar * y, int humidity)
{
    int cpt = 0;

    if (*x >= 0) {      /* normal locations */
        *x += xstart;
        *y += ystart;
    } else if (*x > -11) {      /* special locations */
        *y = ystart + rloc_y[-*y - 1];
        *x = xstart + rloc_x[-*x - 1];
    } else {    /* random location */
        do {
            *x = xstart + mrn2((int)xsize);
            *y = ystart + mrn2((int)ysize);
            if (is_ok_location(lev, *x, *y, humidity))
                break;
        } while (++cpt < 100);
        if (cpt >= 100) {
            int xx, yy;

            /* last try */
            for (xx = 0; xx < xsize; xx++)
                for (yy = 0; yy < ysize; yy++) {
                    *x = xstart + xx;
                    *y = ystart + yy;
                    if (is_ok_location(lev, *x, *y, humidity))
                        goto found_it;
                }
            panic("get_location:  can't find a place!");
        }
    }
found_it:

    if (!isok(*x, *y)) {
        impossible("get_location:  (%d,%d) out of bounds", *x, *y);
        *x = x_maze_max;
        *y = y_maze_max;
    }
}

static boolean
is_ok_location(struct level *lev, schar x, schar y, int humidity)
{
    int typ;

    if (Is_waterlevel(&lev->z))
        return TRUE;    /* accept any spot */

    if (t_at(lev, x, y))
        return FALSE;   /* don't spawn monsters on traps */

    if (humidity & DRY) {
        typ = lev->locations[x][y].typ;
        if (typ == ROOM || typ == AIR || typ == CLOUD || typ == ICE ||
            typ == CORR)
            return TRUE;
    }
    if (humidity & WET) {
        if (is_pool(lev, x, y) || is_lava(lev, x, y))
            return TRUE;
    }
    return FALSE;
}

/*
 * Shuffle the registers for locations, objects or monsters
 */
static void
sp_lev_shuffle(char list1[], char list2[], int n, struct level *lev)
{
    int i, j;
    char k;

    for (i = n - 1; i > 0; i--) {
        if ((j = mrn2(i + 1)) == i)
            continue;
        k = list1[j];
        list1[j] = list1[i];
        list1[i] = k;
        if (list2) {
            k = list2[j];
            list2[j] = list2[i];
            list2[i] = k;
        }
    }
}

/*
 * Get a relative position inside a room.
 * negative values for x or y means RANDOM!
 */

static void
get_room_loc(struct level *lev, schar * x, schar * y, struct mkroom *croom)
{
    coord c;

    if (*x < 0 && *y < 0) {
        if (somexy(lev, croom, &c, mrng())) {
            *x = c.x;
            *y = c.y;
        } else
            panic("get_room_loc : can't find a place!");
    } else {
        if (*x < 0)
            *x = mrn2(croom->hx - croom->lx + 1);
        if (*y < 0)
            *y = mrn2(croom->hy - croom->ly + 1);
        *x += croom->lx;
        *y += croom->ly;
    }
}

/*
 * Get a relative position inside a room.
 * negative values for x or y means RANDOM!
 */
static void
get_free_room_loc(struct level *lev, schar * x, schar * y, struct mkroom *croom)
{
    schar try_x, try_y;
    int trycnt = 0;

    do {
        try_x = *x, try_y = *y;
        get_room_loc(lev, &try_x, &try_y, croom);
    } while (lev->locations[try_x][try_y].typ != ROOM && ++trycnt <= 100);

    if (trycnt > 100)
        panic("get_free_room_loc:  can't find a place!");
    *x = try_x, *y = try_y;
}

boolean
check_room(struct level *lev, xchar * lowx, xchar * ddx, xchar * lowy,
           xchar * ddy, boolean vault)
{
    int x, y, hix = *lowx + *ddx, hiy = *lowy + *ddy;
    struct rm *loc;
    int xlim, ylim, ymax;

    xlim = XLIM + (vault ? 1 : 0);
    ylim = YLIM + (vault ? 1 : 0);

    if (*lowx < 2)
        *lowx = 2;
    if (*lowy < 2)
        *lowy = 2;
    if (hix > COLNO - 3)
        hix = COLNO - 3;
    if (hiy > ROWNO - 3)
        hiy = ROWNO - 3;
chk:
    if (hix <= *lowx || hiy <= *lowy)
        return FALSE;

    /* check area around room (and make room smaller if necessary) */
    for (x = *lowx - xlim; x <= hix + xlim; x++) {
        if (x <= 0 || x >= COLNO)
            continue;
        y = *lowy - ylim;
        ymax = hiy + ylim;
        if (y < 0)
            y = 0;
        if (ymax >= ROWNO)
            ymax = (ROWNO - 1);
        loc = &lev->locations[x][y];

        for (; y <= ymax; y++) {
            if (loc++->typ) {
                if (!mrn2(3))
                    return FALSE;

                if (x < *lowx)
                    *lowx = x + xlim + 1;
                else
                    hix = x - xlim - 1;
                if (y < *lowy)
                    *lowy = y + ylim + 1;
                else
                    hiy = y - ylim - 1;
                goto chk;
            }
        }
    }
    *ddx = hix - *lowx;
    *ddy = hiy - *lowy;
    return TRUE;
}

/*
 * Create a new room.
 */
boolean
create_room(struct level * lev, xchar x, xchar y, xchar w, xchar h, xchar xal,
            xchar yal, xchar rtype, xchar rlit, int *smeq)
{
    xchar xabs = 0, yabs = 0;
    int wtmp, htmp, xaltmp, yaltmp, xtmp, ytmp;
    struct nhrect *r1 = NULL, r2;
    int trycnt = 0;
    boolean vault = FALSE;
    int xlim = XLIM, ylim = YLIM;

    if (rtype == -1)    /* Is the type random ? */
        rtype = OROOM;

    if (rtype == VAULT) {
        vault = TRUE;
        xlim++;
        ylim++;
    }

    /* on low levels the room is lit (usually) */
    /* some other rooms may require lighting */

    /* is light state random ? */
    if (rlit == -1)
        rlit = (mrn2(1 + abs(depth(&lev->z))) <= 11 &&
                mrn2(77)) ? TRUE : FALSE;

    /* 
     * Here we will try to create a room. If some parameters are
     * random we are willing to make several try before we give
     * it up.
     */
    do {
        xchar xborder, yborder;

        wtmp = w;
        htmp = h;
        xtmp = x;
        ytmp = y;
        xaltmp = xal;
        yaltmp = yal;

        /* First case : a totaly random room */

        if ((xtmp < 0 && ytmp < 0 && wtmp < 0 && xaltmp < 0 && yaltmp < 0) ||
            vault) {
            xchar hx, hy, lx, ly, dx, dy;

            r1 = rnd_rect();    /* Get a random rectangle */

            if (!r1)    /* No more free rectangles ! */
                return FALSE;

            hx = r1->hx;
            hy = r1->hy;
            lx = r1->lx;
            ly = r1->ly;
            if (vault)
                dx = dy = 1;
            else {
                dx = 2 + mrn2((hx - lx > 28) ? 12 : 8);
                dy = 2 + mrn2(4);
                if (dx * dy > 50)
                    dy = 50 / dx;
            }
            xborder = (lx > 0 && hx < COLNO - 1) ? 2 * xlim : xlim + 1;
            yborder = (ly > 0 && hy < ROWNO - 1) ? 2 * ylim : ylim + 1;
            if (hx - lx < dx + 3 + xborder || hy - ly < dy + 3 + yborder) {
                r1 = 0;
                continue;
            }
            xabs = lx + (lx > 0 ? xlim : 3)
                + mrn2(hx - (lx > 0 ? lx : 3) - dx - xborder + 1);
            yabs = ly + (ly > 0 ? ylim : 2)
                + mrn2(hy - (ly > 0 ? ly : 2) - dy - yborder + 1);
            if (ly == 0 && hy >= (ROWNO - 1) &&
                (!lev->nroom || !mrn2(lev->nroom)) &&
                (yabs + dy > ROWNO / 2)) {
                yabs = 2 + mrn2(3);
                if (lev->nroom < 4 && dy > 1)
                    dy--;
            }
            if (!check_room(lev, &xabs, &dx, &yabs, &dy, vault)) {
                r1 = 0;
                continue;
            }
            wtmp = dx + 1;
            htmp = dy + 1;
            r2.lx = xabs - 1;
            r2.ly = yabs - 1;
            r2.hx = xabs + wtmp;
            r2.hy = yabs + htmp;
        } else {        /* Only some parameters are random */
            int rndpos = 0;

            if (xtmp < 0 && ytmp < 0) { /* Position is RANDOM */
                xtmp = 1 + mrn2(5);
                ytmp = 1 + mrn2(5);
                rndpos = 1;
            }
            if (wtmp < 0 || htmp < 0) { /* Size is RANDOM */
                wtmp = 3 + mrn2(15);
                htmp = 2 + mrn2(8);
            }
            if (xaltmp == -1)   /* Horizontal alignment is RANDOM */
                xaltmp = 1 + mrn2(3);
            if (yaltmp == -1)   /* Vertical alignment is RANDOM */
                yaltmp = 1 + mrn2(3);

            /* Try to generate real (absolute) coordinates here! */

            xabs = (((xtmp - 1) * COLNO) / 5) + 1;
            yabs = (((ytmp - 1) * ROWNO) / 5) + 1;
            switch (xaltmp) {
            case LEFT:
                break;
            case RIGHT:
                xabs += (COLNO / 5) - wtmp;
                break;
            case CENTER:
                xabs += ((COLNO / 5) - wtmp) / 2;
                break;
            }
            switch (yaltmp) {
            case TOP:
                break;
            case BOTTOM:
                yabs += (ROWNO / 5) - htmp;
                break;
            case CENTER:
                yabs += ((ROWNO / 5) - htmp) / 2;
                break;
            }

            if (xabs + wtmp - 1 > COLNO - 2)
                xabs = COLNO - wtmp - 3;
            if (xabs < 2)
                xabs = 2;
            if (yabs + htmp - 1 > ROWNO - 2)
                yabs = ROWNO - htmp - 3;
            if (yabs < 2)
                yabs = 2;

            /* Try to find a rectangle that fit our room ! */

            r2.lx = xabs - 1;
            r2.ly = yabs - 1;
            r2.hx = xabs + wtmp + rndpos;
            r2.hy = yabs + htmp + rndpos;
            r1 = get_rect(&r2);
        }
    } while (++trycnt <= 100 && !r1);
    if (!r1) {  /* creation of room failed ? */
        return FALSE;
    }
    split_rects(r1, &r2);

    if (!vault) {
        smeq[lev->nroom] = lev->nroom;
        add_room(lev, xabs, yabs, xabs + wtmp - 1, yabs + htmp - 1, rlit, rtype,
                 FALSE);
    } else {
        lev->rooms[lev->nroom].lx = xabs;
        lev->rooms[lev->nroom].ly = yabs;
    }
    return TRUE;
}

/*
 * Create a subroom in room proom at pos x,y with width w & height h.
 * x & y are relative to the parent room.
 */
static boolean
create_subroom(struct level *lev, struct mkroom *proom, xchar x, xchar y,
               xchar w, xchar h, xchar rtype, xchar rlit)
{
    xchar width, height;

    width = proom->hx - proom->lx + 1;
    height = proom->hy - proom->ly + 1;

    /* There is a minimum size for the parent room */
    if (width < 4 || height < 4)
        return FALSE;

    /* Check for random position, size, etc... */

    if (w == -1)
        w = 1 + mrn2(width - 3);
    if (h == -1)
        h = 1 + mrn2(height - 3);
    if (x == -1)
        x = mrn2(width - w - 1);
    if (y == -1)
        y = mrn2(height - h - 1);
    if (x == 1)
        x = 0;
    if (y == 1)
        y = 0;
    if ((x + w + 1) == width)
        x++;
    if ((y + h + 1) == height)
        y++;
    if (rtype == -1)
        rtype = OROOM;
    if (rlit == -1)
        rlit = (mrn2(1 + abs(depth(&lev->z))) <= 11 && mrn2(77)) ? TRUE : FALSE;
    add_subroom(lev, proom, proom->lx + x, proom->ly + y, proom->lx + x + w - 1,
                proom->ly + y + h - 1, rlit, rtype, FALSE);
    return TRUE;
}

/*
 * Create a new door in a room.
 * It's placed on a wall (north, south, east or west).
 */

static void
create_door(struct level *lev, room_door * dd, struct mkroom *broom)
{
    int x, y;
    int trycnt = 0;

    if (dd->secret == -1)
        dd->secret = mrn2(2);

    if (dd->mask == -1) {
        /* is it a locked door, closed, or a doorway? */
        if (!dd->secret) {
            if (!mrn2(3)) {
                if (!mrn2(5))
                    dd->mask = D_ISOPEN;
                else if (!mrn2(6))
                    dd->mask = D_LOCKED;
                else
                    dd->mask = D_CLOSED;
                if (dd->mask != D_ISOPEN && !mrn2(25))
                    dd->mask |= D_TRAPPED;
            } else
                dd->mask = D_NODOOR;
        } else {
            if (!mrn2(5))
                dd->mask = D_LOCKED;
            else
                dd->mask = D_CLOSED;

            if (!mrn2(20))
                dd->mask |= D_TRAPPED;
        }
    }

    do {
        int dwall, dpos;

        dwall = dd->wall;
        if (dwall == -1)        /* The wall is RANDOM */
            dwall = 1 << mrn2(4);

        dpos = dd->pos;
        if (dpos == -1) /* The position is RANDOM */
            dpos = mrn2((dwall == W_WEST ||
                         dwall == W_EAST) ? (broom->hy - broom->ly) :
                        (broom->hx - broom->lx));

        /* Convert wall and pos into an absolute coordinate! */

        switch (dwall) {
        case W_NORTH:
            y = broom->ly - 1;
            x = broom->lx + dpos;
            break;
        case W_SOUTH:
            y = broom->hy + 1;
            x = broom->lx + dpos;
            break;
        case W_WEST:
            x = broom->lx - 1;
            y = broom->ly + dpos;
            break;
        case W_EAST:
            x = broom->hx + 1;
            y = broom->ly + dpos;
            break;
        default:
            x = y = 0;
            panic("create_door: No wall for door!");
            break;
        }
        if (okdoor(lev, x, y))
            break;
    } while (++trycnt <= 100);
    if (trycnt > 100) {
        impossible("create_door: Can't find a proper place!");
        return;
    }
    add_door(lev, x, y, broom);
    lev->locations[x][y].typ = (dd->secret ? SDOOR : DOOR);
    lev->locations[x][y].doormask = dd->mask;
}

/*
 * Create a secret door in croom on any one of the specified walls.
 */
void
create_secret_door(struct level *lev, struct mkroom *croom, xchar walls)
{       /* any of W_NORTH | W_SOUTH | W_EAST | W_WEST (or W_ANY) */
    xchar sx, sy;       /* location of the secret door */
    int count;

    for (count = 0; count < 100; count++) {
        sx = croom->lx + mrn2(croom->hx - croom->lx + 1);
        sy = croom->ly + mrn2(croom->hy - croom->ly + 1);

        switch (mrn2(4)) {
        case 0:        /* top */
            if (!(walls & W_NORTH))
                continue;
            sy = croom->ly - 1;
            break;
        case 1:        /* bottom */
            if (!(walls & W_SOUTH))
                continue;
            sy = croom->hy + 1;
            break;
        case 2:        /* left */
            if (!(walls & W_EAST))
                continue;
            sx = croom->lx - 1;
            break;
        case 3:        /* right */
            if (!(walls & W_WEST))
                continue;
            sx = croom->hx + 1;
            break;
        }

        if (okdoor(lev, sx, sy)) {
            lev->locations[sx][sy].typ = SDOOR;
            lev->locations[sx][sy].doormask = D_CLOSED;
            add_door(lev, sx, sy, croom);
            return;
        }
    }

    impossible("couldn't create secret door on any walls 0x%x", walls);
}

/*
 * Create a trap in a room.
 */
static void
create_trap(struct level *lev, trap * t, struct mkroom *croom)
{
    schar x, y;
    coord tm;

    if (mrn2(100) < t->chance) {
        x = t->x;
        y = t->y;
        if (croom)
            get_free_room_loc(lev, &x, &y, croom);
        else
            get_location(lev, &x, &y, DRY);

        tm.x = x;
        tm.y = y;

        mktrap(lev, t->type, 1, NULL, &tm);
    }
}

/*
 * Create a monster in a room.
 */
static int
noncoalignment(aligntyp alignment, struct level *lev)
{
    int k;

    k = mrn2(2);
    if (!alignment)
        return k ? -1 : 1;
    return k ? -alignment : 0;
}

static void
create_monster(struct level *lev, monster * m, struct mkroom *croom)
{
    struct monst *mtmp;
    schar x, y;
    char class;
    aligntyp amask;
    coord cc;
    const struct permonst *pm;
    unsigned g_mvflags;
    boolean fallback_to_random = FALSE;

    if (mrn2(100) < m->chance) {

        if (m->class >= 0)
            class = (char)def_char_to_monclass((char)m->class);
        else if (m->class > -11)
            class = (char)def_char_to_monclass(rmonst[-m->class - 1]);
        else
            class = 0;

        if (class == MAXMCLASSES)
            panic("create_monster: unknown monster class '%c'", m->class);

        amask = (m->align == AM_SPLEV_CO) ?
            Align2amask(u.ualignbase[A_ORIGINAL]) :
            (m->align == AM_SPLEV_NONCO) ?
            Align2amask(noncoalignment(u.ualignbase[A_ORIGINAL], lev)) :
            (m->align <= -11) ? induced_align(&lev->z, 80, mrng()) :
            (m->align < 0 ? ralign[-m->align - 1] : m->align);

        if (!class)
            pm = NULL;
        else if (m->id != NON_PM) {
            pm = &mons[m->id];
            g_mvflags = (unsigned)mvitals[m->id].mvflags;
            if ((pm->geno & G_UNIQ) && (g_mvflags & G_EXTINCT))
                goto m_done;
            else if (g_mvflags & G_GONE) { /* genocided or extinct */
                fallback_to_random = TRUE;
                pm = NULL;      /* make random monster */
            }
        } else {
            pm = mkclass(&lev->z, class,
                         ((class == S_KOP) || (class == S_EEL)) ? G_NOGEN : 0,
                         mrng());
            /* if we can't get a specific monster type (pm == 0) then the class 
               has been genocided, so settle for a random monster */
            if (!pm)
                fallback_to_random = TRUE;
        }
        if (In_mines(&lev->z) && mrn2(3) && pm && your_race(pm) &&
            (Race_if(PM_DWARF) || Race_if(PM_GNOME))) {
            pm = NULL;
            fallback_to_random = TRUE;
        }

        x = m->x;
        y = m->y;
        if (croom)
            get_room_loc(lev, &x, &y, croom);
        else {
            if (!pm || !is_swimmer(pm))
                get_location(lev, &x, &y, DRY);
            else if (pm->mlet == S_EEL)
                get_location(lev, &x, &y, WET);
            else
                get_location(lev, &x, &y, DRY | WET);
        }
        /* try to find a close place if someone else is already there
           (on the main RNG) */
        if (MON_AT(lev, x, y) && enexto(&cc, lev, x, y, pm))
            x = cc.x, y = cc.y;

        if (m->align != -12)
            mtmp = mk_roamer(pm, Amask2align(amask), lev, x, y, m->peaceful,
                             fallback_to_random ? NO_MM_FLAGS : MM_ALLLEVRNG);
        else if (PM_ARCHEOLOGIST <= m->id && m->id <= PM_WIZARD)
            mtmp = mk_mplayer(pm, lev, x, y, FALSE, fallback_to_random ?
                              rng_main : mrng());
        else
            mtmp = makemon(pm, lev, x, y, fallback_to_random ?
                           NO_MM_FLAGS : MM_ALLLEVRNG);

        if (mtmp) {
            /* handle specific attributes for some special monsters */
            if (m->name.str)
                mtmp = christen_monst(mtmp, m->name.str);

            /* 
             * This is currently hardwired for mimics only.  It should
             * eventually be expanded.
             */
            if (m->appear_as.str && mtmp->data->mlet == S_MIMIC) {
                int i;

                switch (m->appear) {
                case M_AP_NOTHING:
                    impossible("create_monster: mon has an appearance, \"%s\", "
                               "but no type", m->appear_as.str);
                    break;

                case M_AP_FURNITURE:
                    for (i = 0; i < MAXPCHARS; i++)
                        if (!strcmp(defexplain[i], m->appear_as.str))
                            break;
                    if (i == MAXPCHARS) {
                        impossible("create_monster: can't find feature \"%s\"",
                                   m->appear_as.str);
                    } else {
                        mtmp->m_ap_type = M_AP_FURNITURE;
                        mtmp->mappearance = i;
                    }
                    break;

                case M_AP_OBJECT:
                    for (i = 0; i < NUM_OBJECTS; i++)
                        if (OBJ_NAME(objects[i]) &&
                            !strcmp(OBJ_NAME(objects[i]), m->appear_as.str))
                            break;
                    if (i == NUM_OBJECTS) {
                        impossible("create_monster: can't find object \"%s\"",
                                   m->appear_as.str);
                    } else {
                        mtmp->m_ap_type = M_AP_OBJECT;
                        mtmp->mappearance = i;
                    }
                    break;

                case M_AP_MONSTER:
                    /* note: mimics don't appear as monsters! */
                    /* (but chameleons can :-) */
                default:
                    impossible("create_monster: unimplemented mon appear type "
                               "[%d,\"%s\"]", m->appear, m->appear_as.str);
                    break;
                }
                if (does_block(lev, x, y))
                    block_point(x, y);
            }

            if (m->peaceful >= 0) {
                mtmp->mpeaceful = m->peaceful;
                /* changed mpeaceful again; have to reset malign */
                set_malign(mtmp);
            }
            if (m->asleep >= 0) {
                mtmp->msleeping = m->asleep;
            }
        }

    }
m_done:
    Free(m->name.str);
    Free(m->appear_as.str);
}

/*
 * Create an object in a room.
 */
static void
create_object(struct level *lev, object * o, struct mkroom *croom)
{
    struct obj *otmp;
    schar x, y;
    char c;
    boolean named;      /* has a name been supplied in level description? */

    if (mrn2(100) < o->chance) {
        named = o->name.str ? TRUE : FALSE;

        x = o->x;
        y = o->y;
        if (croom)
            get_room_loc(lev, &x, &y, croom);
        else
            get_location(lev, &x, &y, DRY);

        if (o->class >= 0)
            c = o->class;
        else if (o->class > -11)
            c = robjects[-(o->class + 1)];
        else
            c = 0;

        if (!c)
            otmp = mkobj_at(RANDOM_CLASS, lev, x, y, !named, mrng());
        else if (o->id != -1)
            otmp = mksobj_at(o->id, lev, x, y, TRUE, !named, mrng());
        else {
            /* 
             * The special levels are compiled with the default "text" object
             * class characters.  We must convert them to the internal format.
             */
            char oclass = (char)def_char_to_objclass(c);

            if (oclass == MAXOCLASSES)
                panic("create_object:  unexpected object class '%c'", c);

            /* KMH -- Create piles of gold properly */
            if (oclass == COIN_CLASS)
                otmp = mkgold(0L, lev, x, y, mrng());
            else
                otmp = mkobj_at(oclass, lev, x, y, !named, mrng());
        }

        if (o->spe != -127)     /* That means NOT RANDOM! */
            otmp->spe = (schar) o->spe;

        switch (o->curse_state) {
        case 1:
            bless(otmp);
            break;      /* BLESSED */
        case 2:
            unbless(otmp);
            uncurse(otmp);
            break;      /* uncursed */
        case 3:
            curse(otmp);
            break;      /* CURSED */
        default:
            break;      /* Otherwise it's random and we're happy with what
                           mkobj gave us! */
        }

        /* corpsenm is "empty" if -1, random if -2, otherwise specific */
        if (o->corpsenm == NON_PM - 1)
            otmp->corpsenm = rndmonnum(&lev->z, mrng());
        else if (o->corpsenm != NON_PM)
            otmp->corpsenm = o->corpsenm;

        /* assume we wouldn't be given an egg corpsenm unless it was hatchable
           */
        if (otmp->otyp == EGG && otmp->corpsenm != NON_PM) {
            if (dead_species(otmp->otyp, TRUE))
                kill_egg(otmp); /* make sure nothing hatches */
            else
                attach_egg_hatch_timeout(otmp); /* attach new hatch timeout */
        }

        if (named)
            otmp = oname(otmp, o->name.str);

        switch (o->containment) {
            static struct obj *container = 0;

            /* contents */
        case 1:
            if (!container) {
                impossible("create_object: no container");
                break;
            }
            remove_object(otmp);
            add_to_container(container, otmp);
            container->owt = weight(container);
            goto o_done;        /* don't stack, but do other cleanup */
            /* container */
        case 2:
            delete_contents(otmp);
            container = otmp;
            break;
            /* nothing */
        case 0:
            break;

        default:
            impossible("containment type %d?", (int)o->containment);
        }

        /* Medusa level special case: statues are petrified monsters, so they
           are not stone-resistant and have monster inventory.  They also lack
           other contents, but that can be specified as an empty container. */
        if (o->id == STATUE && Is_medusa_level(&lev->z) &&
            o->corpsenm == NON_PM) {
            struct monst *was = NULL;
            struct obj *obj;
            int wastyp;
            int i;

            /* Named random statues are of player types, and aren't stone-
               resistant (if they were, we'd have to reset the name as well as
               setting corpsenm). */
            wastyp = otmp->corpsenm;
            for (i = 0; i < 1000; i++) {
                /* makemon without rndmonst() might create a group */
                was = makemon(&mons[wastyp], lev, COLNO, ROWNO, MM_ALLLEVRNG);
                if (was) {
                    if (!resists_ston(was))
                        break;
                    mongone(was);
                }
                wastyp = rndmonnum(&lev->z, mrng());
            }
            if (was) {
                otmp->corpsenm = wastyp;
                while (was->minvent) {
                    obj = was->minvent;
                    obj->owornmask = 0;
                    obj_extract_self(obj);
                    add_to_container(otmp, obj);
                }
                otmp->owt = weight(otmp);
                mongone(was);
            }
        } else if (otmp->otyp == STATUE || otmp->otyp == CORPSE)
            otmp->owt = weight(otmp);

        stackobj(otmp);

    }   /* if (rn2(100) < o->chance) */
o_done:
    Free(o->name.str);
}

/*
 * Randomly place a specific engraving, then release its memory.
 */
static void
create_engraving(struct level *lev, engraving * e, struct mkroom *croom)
{
    xchar x, y;

    x = e->x, y = e->y;
    if (croom)
        get_room_loc(lev, &x, &y, croom);
    else
        get_location(lev, &x, &y, DRY);

    make_engr_at(lev, x, y, e->engr.str, 0L, e->etype);
    free(e->engr.str);
}

/*
 * Create stairs in a room.
 */
static void
create_stairs(struct level *lev, stair * s, struct mkroom *croom)
{
    schar x, y;

    x = s->x;
    y = s->y;
    get_free_room_loc(lev, &x, &y, croom);
    mkstairs(lev, x, y, (char)s->up, croom);
}

/*
 * Create an altar in a room.
 */
static void
create_altar(struct level *lev, altar * a, struct mkroom *croom)
{
    schar sproom, x, y;
    aligntyp amask;
    boolean croom_is_temple = TRUE;
    int oldtyp;

    x = a->x;
    y = a->y;

    if (croom) {
        get_free_room_loc(lev, &x, &y, croom);
        if (croom->rtype != TEMPLE)
            croom_is_temple = FALSE;
    } else {
        get_location(lev, &x, &y, DRY);
        if ((sproom = (schar) *in_rooms(lev, x, y, TEMPLE)))
            croom = &lev->rooms[sproom - ROOMOFFSET];
        else
            croom_is_temple = FALSE;
    }

    /* check for existing features */
    oldtyp = lev->locations[x][y].typ;
    if (oldtyp == STAIRS || oldtyp == LADDER)
        return;

    a->x = x;
    a->y = y;

    /* Is the alignment random ? If so, it's an 80% chance that the altar will
       be co-aligned. The alignment is encoded as amask values instead of
       alignment values to avoid conflicting with the rest of the encoding,
       shared by many other parts of the special level code. */

    amask = (a->align == AM_SPLEV_CO) ? Align2amask(u.ualignbase[A_ORIGINAL]) :
        (a->align == AM_SPLEV_NONCO) ?
        Align2amask(noncoalignment(u.ualignbase[A_ORIGINAL], lev)) :
        (a->align == -11) ? induced_align(&lev->z, 80, mrng()) :
        (a->align < 0 ? ralign[-a->align - 1] : a->align);

    lev->locations[x][y].typ = ALTAR;
    lev->locations[x][y].altarmask = amask;

    if (a->shrine < 0)
        a->shrine = mrn2(2);     /* handle random case */

    if (!croom_is_temple || !a->shrine)
        return;

    if (a->shrine) {    /* Is it a shrine or sanctum? */
        priestini(lev, croom, x, y, (a->shrine > 1));
        lev->locations[x][y].altarmask |= AM_SHRINE;
        if (a->shrine > 1) { /* It is a sanctum? */
            lev->locations[x][y].altarmask |= AM_SANCTUM;
        }
    }
}

/*
 * Create a gold pile in a room.
 */
static void
create_gold(struct level *lev, gold * g, struct mkroom *croom)
{
    schar x, y;

    x = g->x;
    y = g->y;
    if (croom)
        get_room_loc(lev, &x, &y, croom);
    else
        get_location(lev, &x, &y, DRY);

    if (g->amount == -1)
        g->amount = 1 + mrn2(200);
    mkgold((long)g->amount, lev, x, y, mrng());
}

/*
 * Create a feature (e.g a fountain) in a room.
 */
static void
create_feature(struct level *lev, int fx, int fy, struct mkroom *croom, int typ)
{
    schar x, y;
    int trycnt = 0;

    x = fx;
    y = fy;
    if (croom) {
        if (x < 0 && y < 0)
            do {
                x = -1;
                y = -1;
                get_room_loc(lev, &x, &y, croom);
            } while (++trycnt <= 200 && occupied(lev, x, y));
        else
            get_room_loc(lev, &x, &y, croom);
        if (trycnt > 200)
            return;
    } else {
        get_location(lev, &x, &y, DRY);
    }
    /* Don't cover up an existing feature (particularly randomly placed
       stairs).  However, if the _same_ feature is already here, it came from
       the map drawing and we still need to update the special counters. */
    if (IS_FURNITURE(lev->locations[x][y].typ) &&
        lev->locations[x][y].typ != typ)
        return;

    lev->locations[x][y].typ = typ;
}

/*
 * Search for a door in a room on a specified wall.
 */
static boolean
search_door(struct mkroom *croom, xchar * x, xchar * y, xchar wall, int cnt)
{
    int dx, dy;
    int xx, yy;

    switch (wall) {
    case W_NORTH:
        dy = 0;
        dx = 1;
        xx = croom->lx;
        yy = croom->hy + 1;
        break;
    case W_SOUTH:
        dy = 0;
        dx = 1;
        xx = croom->lx;
        yy = croom->ly - 1;
        break;
    case W_EAST:
        dy = 1;
        dx = 0;
        xx = croom->hx + 1;
        yy = croom->ly;
        break;
    case W_WEST:
        dy = 1;
        dx = 0;
        xx = croom->lx - 1;
        yy = croom->ly;
        break;
    default:
        dx = dy = xx = yy = 0;
        panic("search_door: Bad wall!");
        break;
    }
    while (xx <= croom->hx + 1 && yy <= croom->hy + 1) {
        if (IS_DOOR(level->locations[xx][yy].typ) ||
            level->locations[xx][yy].typ == SDOOR) {
            *x = xx;
            *y = yy;
            if (cnt-- <= 0)
                return TRUE;
        }
        xx += dx;
        yy += dy;
    }
    return FALSE;
}

/*
 * Dig a corridor between two points.
 */
boolean
dig_corridor(struct level * lev, coord * org, coord * dest, boolean nxcor,
             schar ftyp, schar btyp)
{
    int dx = 0, dy = 0, dix, diy, cct;
    struct rm *crm;
    int tx, ty, xx, yy;

    xx = org->x;
    yy = org->y;
    tx = dest->x;
    ty = dest->y;
    if (xx <= 0 || yy <= 0 || tx <= 0 || ty <= 0 || xx > COLNO - 1 ||
        tx > COLNO - 1 || yy > ROWNO - 1 || ty > ROWNO - 1)
        return FALSE;

    if (tx > xx)
        dx = 1;
    else if (ty > yy)
        dy = 1;
    else if (tx < xx)
        dx = -1;
    else
        dy = -1;

    xx -= dx;
    yy -= dy;
    cct = 0;
    while (xx != tx || yy != ty) {
        /* loop: dig corridor at [xx,yy] and find new [xx,yy] */
        if (cct++ > 500 || (nxcor && !mrn2(35)))
            return FALSE;

        xx += dx;
        yy += dy;

        if (xx >= COLNO - 1 || xx <= 0 || yy <= 0 || yy >= ROWNO - 1)
            return FALSE;       /* impossible */

        crm = &lev->locations[xx][yy];
        if (crm->typ == btyp) {
            if (ftyp != CORR || mrn2(100)) {
                crm->typ = ftyp;
                if (nxcor && !mrn2(50))
                    mksobj_at(BOULDER, lev, xx, yy, TRUE, FALSE, mrng());
            } else {
                crm->typ = SCORR;
            }
        } else if (crm->typ != ftyp && crm->typ != SCORR) {
            /* strange ... */
            return FALSE;
        }

        /* find next corridor position */
        dix = abs(xx - tx);
        diy = abs(yy - ty);

        /* do we have to change direction ? */
        if (dy && dix > diy) {
            int ddx = (xx > tx) ? -1 : 1;

            crm = &lev->locations[xx + ddx][yy];
            if (crm->typ == btyp || crm->typ == ftyp || crm->typ == SCORR) {
                dx = ddx;
                dy = 0;
                continue;
            }
        } else if (dx && diy > dix) {
            int ddy = (yy > ty) ? -1 : 1;

            crm = &lev->locations[xx][yy + ddy];
            if (crm->typ == btyp || crm->typ == ftyp || crm->typ == SCORR) {
                dy = ddy;
                dx = 0;
                continue;
            }
        }

        /* continue straight on? */
        crm = &lev->locations[xx + dx][yy + dy];
        if (crm->typ == btyp || crm->typ == ftyp || crm->typ == SCORR)
            continue;

        /* no, what must we do now?? */
        if (dx) {
            dx = 0;
            dy = (ty < yy) ? -1 : 1;
        } else {
            dy = 0;
            dx = (tx < xx) ? -1 : 1;
        }
        crm = &lev->locations[xx + dx][yy + dy];
        if (crm->typ == btyp || crm->typ == ftyp || crm->typ == SCORR)
            continue;
        dy = -dy;
        dx = -dx;
    }
    return TRUE;
}

/*
 * Disgusting hack: since special levels have their rooms filled before
 * sorting the rooms, we have to re-arrange the speed values upstairs_room
 * and dnstairs_room after the rooms have been sorted.  On normal levels,
 * stairs don't get created until _after_ sorting takes place.
 */
static void
fix_stair_rooms(struct level *lev)
{
    int i;
    struct mkroom *croom;

    if (isok(lev->dnstair.sx, lev->dnstair.sy) &&
        !((lev->dnstairs_room->lx <= lev->dnstair.sx &&
           lev->dnstair.sx <= lev->dnstairs_room->hx) &&
          (lev->dnstairs_room->ly <= lev->dnstair.sy &&
           lev->dnstair.sy <= lev->dnstairs_room->hy))) {
        for (i = 0; i < lev->nroom; i++) {
            croom = &lev->rooms[i];
            if ((croom->lx <= lev->dnstair.sx && lev->dnstair.sx <= croom->hx)
                && (croom->ly <= lev->dnstair.sy &&
                    lev->dnstair.sy <= croom->hy)) {
                lev->dnstairs_room = croom;
                break;
            }
        }
        if (i == lev->nroom)
            panic("Couldn't find dnstair room in fix_stair_rooms!");
    }
    if (isok(lev->upstair.sx, lev->upstair.sy) &&
        !((lev->upstairs_room->lx <= lev->upstair.sx &&
           lev->upstair.sx <= lev->upstairs_room->hx) &&
          (lev->upstairs_room->ly <= lev->upstair.sy &&
           lev->upstair.sy <= lev->upstairs_room->hy))) {
        for (i = 0; i < lev->nroom; i++) {
            croom = &lev->rooms[i];
            if ((croom->lx <= lev->upstair.sx && lev->upstair.sx <= croom->hx)
                && (croom->ly <= lev->upstair.sy &&
                    lev->upstair.sy <= croom->hy)) {
                lev->upstairs_room = croom;
                break;
            }
        }
        if (i == lev->nroom)
            panic("Couldn't find upstair room in fix_stair_rooms!");
    }
}

/*
 * Corridors always start from a door. But it can end anywhere...
 * Basically we search for door coordinates or for endpoints coordinates
 * (from a distance).
 */
static void
create_corridor(struct level *lev, corridor *c, int *smeq)
{
    coord org, dest;

    if (c->src.room == -1) {
        sort_rooms(lev);
        fix_stair_rooms(lev);
        makecorridors(lev, smeq);
        return;
    }

    if (!search_door
        (&lev->rooms[c->src.room], &org.x, &org.y, c->src.wall, c->src.door))
        return;

    if (c->dest.room != -1) {
        if (!search_door
            (&lev->rooms[c->dest.room], &dest.x, &dest.y, c->dest.wall,
             c->dest.door))
            return;
        switch (c->src.wall) {
        case W_NORTH:
            org.y--;
            break;
        case W_SOUTH:
            org.y++;
            break;
        case W_WEST:
            org.x--;
            break;
        case W_EAST:
            org.x++;
            break;
        }
        switch (c->dest.wall) {
        case W_NORTH:
            dest.y--;
            break;
        case W_SOUTH:
            dest.y++;
            break;
        case W_WEST:
            dest.x--;
            break;
        case W_EAST:
            dest.x++;
            break;
        }
        dig_corridor(lev, &org, &dest, FALSE, CORR, STONE);
    }
}


/*
 * Fill a room (shop, zoo, etc...) with appropriate stuff.
 */
void
fill_room(struct level *lev, struct mkroom *croom, boolean prefilled)
{
    if (!croom || croom->rtype == OROOM)
        return;

    if (!prefilled) {
        int x, y;

        /* Shop ? */
        if (croom->rtype >= SHOPBASE) {
            stock_room(croom->rtype - SHOPBASE, lev, croom);
            return;
        }

        switch (croom->rtype) {
        case VAULT:
            for (x = croom->lx; x <= croom->hx; x++)
                for (y = croom->ly; y <= croom->hy; y++)
                    mkgold(51 + mrn2(abs(depth(&lev->z)) * 100),
                           lev, x, y, mrng());
            break;
        case COURT:
        case ZOO:
        case BEEHIVE:
        case MORGUE:
        case BARRACKS:
            fill_zoo(lev, croom, mrng());
            break;
        }
    }
}

static void
free_rooms(room ** ro, int n)
{
    short j;
    room *r;

    while (n--) {
        r = ro[n];
        Free(r->name);
        Free(r->parent);
        if ((j = r->ndoor) != 0) {
            while (j--)
                Free(r->doors[j]);
            Free(r->doors);
        }
        if ((j = r->nstair) != 0) {
            while (j--)
                Free(r->stairs[j]);
            Free(r->stairs);
        }
        if ((j = r->naltar) != 0) {
            while (j--)
                Free(r->altars[j]);
            Free(r->altars);
        }
        if ((j = r->nfountain) != 0) {
            while (j--)
                Free(r->fountains[j]);
            Free(r->fountains);
        }
        if ((j = r->nsink) != 0) {
            while (j--)
                Free(r->sinks[j]);
            Free(r->sinks);
        }
        if ((j = r->npool) != 0) {
            while (j--)
                Free(r->pools[j]);
            Free(r->pools);
        }
        if ((j = r->ntrap) != 0) {
            while (j--)
                Free(r->traps[j]);
            Free(r->traps);
        }
        if ((j = r->nmonster) != 0) {
            while (j--)
                Free(r->monsters[j]);
            Free(r->monsters);
        }
        if ((j = r->nobject) != 0) {
            while (j--)
                Free(r->objects[j]);
            Free(r->objects);
        }
        if ((j = r->ngold) != 0) {
            while (j--)
                Free(r->golds[j]);
            Free(r->golds);
        }
        if ((j = r->nengraving) != 0) {
            while (j--)
                Free(r->engravings[j]);
            Free(r->engravings);
        }
        Free(r);
    }
    Free(ro);
}

static void
build_room(struct level *lev, room *r, room *pr, int *smeq)
{
    boolean okroom;
    struct mkroom *aroom;
    short i;
    xchar rtype = (!r->chance || mrn2(100) < r->chance) ? r->rtype : OROOM;

    if (pr) {
        aroom = &lev->subrooms[lev->nsubroom];
        okroom =
            create_subroom(lev, pr->mkr, r->x, r->y, r->w, r->h, rtype,
                           r->rlit);
    } else {
        aroom = &lev->rooms[lev->nroom];
        okroom =
            create_room(lev, r->x, r->y, r->w, r->h, r->xalign, r->yalign,
                        rtype, r->rlit, smeq);
        r->mkr = aroom;
    }

    if (okroom) {
        /* Create subrooms if necessary... */
        for (i = 0; i < r->nsubroom; i++)
            build_room(lev, r->subrooms[i], r, smeq);
        /* And now we can fill the room! */

        /* Priority to the stairs */

        for (i = 0; i < r->nstair; i++)
            create_stairs(lev, r->stairs[i], aroom);

        /* Then to the various elements (sinks, etc..) */
        for (i = 0; i < r->nsink; i++)
            create_feature(lev, r->sinks[i]->x, r->sinks[i]->y, aroom, SINK);
        for (i = 0; i < r->npool; i++)
            create_feature(lev, r->pools[i]->x, r->pools[i]->y, aroom, POOL);
        for (i = 0; i < r->nfountain; i++)
            create_feature(lev, r->fountains[i]->x, r->fountains[i]->y, aroom,
                           FOUNTAIN);
        for (i = 0; i < r->naltar; i++)
            create_altar(lev, r->altars[i], aroom);
        for (i = 0; i < r->ndoor; i++)
            create_door(lev, r->doors[i], aroom);

        /* The traps */
        for (i = 0; i < r->ntrap; i++)
            create_trap(lev, r->traps[i], aroom);

        /* The monsters */
        for (i = 0; i < r->nmonster; i++)
            create_monster(lev, r->monsters[i], aroom);

        /* The objects */
        for (i = 0; i < r->nobject; i++)
            create_object(lev, r->objects[i], aroom);

        /* The gold piles */
        for (i = 0; i < r->ngold; i++)
            create_gold(lev, r->golds[i], aroom);

        /* The engravings */
        for (i = 0; i < r->nengraving; i++)
            create_engraving(lev, r->engravings[i], aroom);

        topologize(lev, aroom); /* set roomno */
        /* MRS - 07/04/91 - This is temporary but should result in proper
           filling of shops, etc. DLC - this can fail if corridors are added to 
           this room at a later point.  Currently no good way to fix this. */
        if (aroom->rtype != OROOM && r->filled)
            fill_room(lev, aroom, FALSE);
    }
}

/*
 * set lighting in a region that will not become a room.
 */
static void
light_region(struct level *lev, region * tmpregion)
{
    boolean litstate = tmpregion->rlit ? 1 : 0;
    int hiy = tmpregion->y2;
    int x, y;
    struct rm *loc;
    int lowy = tmpregion->y1;
    int lowx = tmpregion->x1, hix = tmpregion->x2;

    if (litstate) {
        /* adjust region size for walls, but only if lighted */
        lowx = max(lowx - 1, 0);
        hix = min(hix + 1, COLNO - 1);
        lowy = max(lowy - 1, 0);
        hiy = min(hiy + 1, ROWNO - 1);
    }
    for (x = lowx; x <= hix; x++) {
        loc = &lev->locations[x][lowy];
        for (y = lowy; y <= hiy; y++) {
            if (loc->typ != LAVAPOOL)   /* this overrides normal lighting */
                loc->lit = litstate;
            loc++;
        }
    }
}

/* initialization common to all special levels */
static void
load_common_data(struct level *lev, dlb * fd, int typ)
{
    uchar n;
    long lev_flags;
    int i;

    {
        aligntyp atmp;

        /* shuffle 3 alignments; can't use sp_lev_shuffle() on aligntyp's */
        ralign[0] = init_ralign[0];
        ralign[1] = init_ralign[1];
        ralign[2] = init_ralign[2];
        i = mrn2(3);
        atmp = ralign[2];
        ralign[2] = ralign[i];
        ralign[i] = atmp;
        if (mrn2(2)) {
            atmp = ralign[1];
            ralign[1] = ralign[0];
            ralign[0] = atmp;
        }
    }

    lev->flags.is_maze_lev = typ == SP_LEV_MAZE;

    /* Read the level initialization data */
    Fread(&init_lev, 1, sizeof (lev_init), fd);
    if (init_lev.init_present) {
        if (init_lev.lit < 0)
            init_lev.lit = mrn2(2);
        mkmap(lev, &init_lev);
    }

    /* Read the per level flags */
    Fread(&lev_flags, 1, sizeof (lev_flags), fd);
    if (lev_flags & NOTELEPORT)
        lev->flags.noteleport = 1;
    if (lev_flags & HARDFLOOR)
        lev->flags.hardfloor = 1;
    if (lev_flags & NOMMAP)
        lev->flags.nommap = 1;
    if (lev_flags & SHORTSIGHTED)
        lev->flags.shortsighted = 1;
    if (lev_flags & ARBOREAL)
        lev->flags.arboreal = 1;

    /* Read message */
    Fread(&n, 1, sizeof (n), fd);
    if (n) {
        lev_message = malloc(n + 1);
        Fread(lev_message, 1, (int)n, fd);
        lev_message[n] = 0;
    }

    /* Read hallumsg */
    Fread(&n, 1, sizeof (n), fd);
    if (n) {
        if (Hallucination) {
            lev_message = realloc(lev_message, n + 1);
            Fread(lev_message, 1, (int)n, fd);
            lev_message[n] = 0;
        } else {
            dlb_fseek(fd, n, SEEK_CUR);
        }
    }

    return;
err_out:
    fprintf(stderr, "read error in load_common_data\n");
}

static void
load_one_monster(dlb * fd, monster * m)
{
    int size;

    Fread(m, 1, sizeof *m, fd);
    if ((size = m->name.len) != 0) {
        m->name.str = malloc((unsigned)size + 1);
        Fread(m->name.str, 1, size, fd);
        m->name.str[size] = '\0';
    } else
        m->name.str = NULL;
    if ((size = m->appear_as.len) != 0) {
        m->appear_as.str = malloc((unsigned)size + 1);
        Fread(m->appear_as.str, 1, size, fd);
        m->appear_as.str[size] = '\0';
    } else
        m->appear_as.str = NULL;

    return;
err_out:
    fprintf(stderr, "read error in load_one_monster\n");
}

static void
load_one_object(dlb * fd, object * o)
{
    int size;

    Fread(o, 1, sizeof *o, fd);
    if ((size = o->name.len) != 0) {
        o->name.str = malloc((unsigned)size + 1);
        Fread(o->name.str, 1, size, fd);
        o->name.str[size] = '\0';
    } else
        o->name.str = NULL;

    return;
err_out:
    fprintf(stderr, "read error in load_one_object\n");
}

static void
load_one_engraving(dlb * fd, engraving * e)
{
    int size;

    Fread(e, 1, sizeof *e, fd);
    size = e->engr.len;
    e->engr.str = malloc((unsigned)size + 1);
    Fread(e->engr.str, 1, size, fd);
    e->engr.str[size] = '\0';

    return;
err_out:
    fprintf(stderr, "read error in load_one_engraving\n");
}

static boolean
load_rooms(struct level *lev, dlb *fd, int *smeq)
{
    xchar nrooms, ncorr;
    char n;
    short size;
    corridor tmpcor;
    room **tmproom = NULL;
    int i, j;

    load_common_data(lev, fd, SP_LEV_ROOMS);

    Fread(&n, 1, sizeof (n), fd);       /* nrobjects */
    if (n) {
        Fread(robjects, sizeof (*robjects), n, fd);
        sp_lev_shuffle(robjects, NULL, (int)n, lev);
    }

    Fread(&n, 1, sizeof (n), fd);       /* nrmonst */
    if (n) {
        Fread(rmonst, sizeof (*rmonst), n, fd);
        sp_lev_shuffle(rmonst, NULL, (int)n, lev);
    }

    Fread(&nrooms, 1, sizeof (nrooms), fd);
    /* Number of rooms to read */
    tmproom = NewTab(room, nrooms);
    for (i = 0; i < nrooms; i++) {
        room *r;

        r = tmproom[i] = New(room);

        /* Let's see if this room has a name */
        Fread(&size, 1, sizeof (size), fd);
        if (size > 0) { /* Yup, it does! */
            r->name = malloc((unsigned)size + 1);
            Fread(r->name, 1, size, fd);
            r->name[size] = 0;
        } else
            r->name = NULL;

        /* Let's see if this room has a parent */
        Fread(&size, 1, sizeof (size), fd);
        if (size > 0) { /* Yup, it does! */
            r->parent = malloc((unsigned)size + 1);
            Fread(r->parent, 1, size, fd);
            r->parent[size] = 0;
        } else
            r->parent = NULL;

        Fread(&r->x, 1, sizeof (r->x), fd);
        /* x pos on the grid (1-5) */
        Fread(&r->y, 1, sizeof (r->y), fd);
        /* y pos on the grid (1-5) */
        Fread(&r->w, 1, sizeof (r->w), fd);
        /* width of the room */
        Fread(&r->h, 1, sizeof (r->h), fd);
        /* height of the room */
        Fread(&r->xalign, 1, sizeof (r->xalign), fd);
        /* horizontal alignment */
        Fread(&r->yalign, 1, sizeof (r->yalign), fd);
        /* vertical alignment */
        Fread(&r->rtype, 1, sizeof (r->rtype), fd);
        /* type of room (zoo, shop, etc.) */
        Fread(&r->chance, 1, sizeof (r->chance), fd);
        /* chance of room being special. */
        Fread(&r->rlit, 1, sizeof (r->rlit), fd);
        /* lit or not ? */
        Fread(&r->filled, 1, sizeof (r->filled), fd);
        /* to be filled? */
        r->nsubroom = 0;

        /* read the doors */
        Fread(&r->ndoor, 1, sizeof (r->ndoor), fd);
        if ((n = r->ndoor) != 0)
            r->doors = NewTab(room_door, n);
        while (n--) {
            r->doors[(int)n] = New(room_door);
            Fread(r->doors[(int)n], 1, sizeof (room_door), fd);
        }

        /* read the stairs */
        Fread(&r->nstair, 1, sizeof (r->nstair), fd);
        if ((n = r->nstair) != 0)
            r->stairs = NewTab(stair, n);
        while (n--) {
            r->stairs[(int)n] = New(stair);
            Fread(r->stairs[(int)n], 1, sizeof (stair), fd);
        }

        /* read the altars */
        Fread(&r->naltar, 1, sizeof (r->naltar), fd);
        if ((n = r->naltar) != 0)
            r->altars = NewTab(altar, n);
        while (n--) {
            r->altars[(int)n] = New(altar);
            Fread(r->altars[(int)n], 1, sizeof (altar), fd);
        }

        /* read the fountains */
        Fread(&r->nfountain, 1, sizeof (r->nfountain), fd);
        if ((n = r->nfountain) != 0)
            r->fountains = NewTab(fountain, n);
        while (n--) {
            r->fountains[(int)n] = New(fountain);
            Fread(r->fountains[(int)n], 1, sizeof (fountain), fd);
        }

        /* read the sinks */
        Fread(&r->nsink, 1, sizeof (r->nsink), fd);
        if ((n = r->nsink) != 0)
            r->sinks = NewTab(sink, n);
        while (n--) {
            r->sinks[(int)n] = New(sink);
            Fread(r->sinks[(int)n], 1, sizeof (sink), fd);
        }

        /* read the pools */
        Fread(&r->npool, 1, sizeof (r->npool), fd);
        if ((n = r->npool) != 0)
            r->pools = NewTab(pool, n);
        while (n--) {
            r->pools[(int)n] = New(pool);
            Fread(r->pools[(int)n], 1, sizeof (pool), fd);
        }

        /* read the traps */
        Fread(&r->ntrap, 1, sizeof (r->ntrap), fd);
        if ((n = r->ntrap) != 0)
            r->traps = NewTab(trap, n);
        while (n--) {
            r->traps[(int)n] = New(trap);
            Fread(r->traps[(int)n], 1, sizeof (trap), fd);
        }

        /* read the monsters */
        Fread(&r->nmonster, 1, sizeof (r->nmonster), fd);
        if ((n = r->nmonster) != 0) {
            r->monsters = NewTab(monster, n);
            while (n--) {
                r->monsters[(int)n] = New(monster);
                load_one_monster(fd, r->monsters[(int)n]);
            }
        } else
            r->monsters = 0;

        /* read the objects, in same order as mazes */
        Fread(&r->nobject, 1, sizeof (r->nobject), fd);
        if ((n = r->nobject) != 0) {
            r->objects = NewTab(object, n);
            for (j = 0; j < n; ++j) {
                r->objects[j] = New(object);
                load_one_object(fd, r->objects[j]);
            }
        } else
            r->objects = 0;

        /* read the gold piles */
        Fread(&r->ngold, 1, sizeof (r->ngold), fd);
        if ((n = r->ngold) != 0)
            r->golds = NewTab(gold, n);
        while (n--) {
            r->golds[(int)n] = New(gold);
            Fread(r->golds[(int)n], 1, sizeof (gold), fd);
        }

        /* read the engravings */
        Fread(&r->nengraving, 1, sizeof (r->nengraving), fd);
        if ((n = r->nengraving) != 0) {
            r->engravings = NewTab(engraving, n);
            while (n--) {
                r->engravings[(int)n] = New(engraving);
                load_one_engraving(fd, r->engravings[(int)n]);
            }
        } else
            r->engravings = 0;

    }

    /* Now that we have loaded all the rooms, search the subrooms and create
       the links. */

    for (i = 0; i < nrooms; i++)
        if (tmproom[i]->parent) {
            /* Search the parent room */
            for (j = 0; j < nrooms; j++)
                if (tmproom[j]->name &&
                    !strcmp(tmproom[j]->name, tmproom[i]->parent)) {
                    n = tmproom[j]->nsubroom++;
                    tmproom[j]->subrooms[(int)n] = tmproom[i];
                    break;
                }
        }

    /* 
     * Create the rooms now...
     */

    for (i = 0; i < nrooms; i++)
        if (!tmproom[i]->parent)
            build_room(lev, tmproom[i], NULL, smeq);

    free_rooms(tmproom, nrooms);

    /* read the corridors */

    Fread(&ncorr, sizeof (ncorr), 1, fd);
    for (i = 0; i < ncorr; i++) {
        Fread(&tmpcor, 1, sizeof (tmpcor), fd);
        create_corridor(lev, &tmpcor, smeq);
    }

    return TRUE;

err_out:
    /* TODO: Why is this using fprintf(stderr) rather than impossible()?
       I haven't changed the code because this is /so/ weird I assume that
       there's a good reason for it -- AIS */
    fprintf(stderr, "read error in load_rooms\n");
    if (tmproom)
        free(tmproom);
    return FALSE;
}

/*
 * Select a random coordinate in the maze.
 *
 * We want a place not 'touched' by the loader.  That is, a place in
 * the maze outside every part of the special level.
 */
static void
maze1xy(struct level *lev, coord * m, int humidity)
{
    int x, y, tryct = 2000;

    /* tryct: normally it won't take more than ten or so tries due to the
       circumstances under which we'll be called, but the `humidity' screening
       might drastically change the chances */

    do {
        x = 3 + mrn2(x_maze_max - 3);
        y = 3 + mrn2(y_maze_max - 3);
        if (--tryct < 0)
            break;      /* give up */
    } while (!(x % 2) || !(y % 2) || Map[x][y] ||
             !is_ok_location(lev, (schar) x, (schar) y, humidity));

    m->x = (xchar) x, m->y = (xchar) y;
}

/*
 * The Big Thing: special maze loader
 *
 * Could be cleaner, but it works.
 */
static boolean
load_maze(struct level *lev, dlb * fd)
{
    xchar x, y, typ;
    boolean prefilled, room_not_needed;

    char n, numpart = 0;
    xchar nwalk = 0, nwalk_sav;
    schar filling;
    char halign, valign;

    int xi, dir, size;
    coord mm;
    int mapcount, mapcountmax, mapfact;

    lev_region tmplregion;
    region tmpregion;
    door tmpdoor;
    trap tmptrap;
    monster tmpmons;
    object tmpobj;
    drawbridge tmpdb;
    walk tmpwalk;
    digpos tmpdig;
    lad tmplad;
    stair tmpstair, prevstair;
    altar tmpaltar;
    gold tmpgold;
    fountain tmpfountain;
    engraving tmpengraving;
    xchar mustfill[(MAXNROFROOMS + 1) * 2];
    struct trap *badtrap;
    boolean has_bounds;

    memset(&Map[0][0], 0, sizeof Map);
    load_common_data(lev, fd, SP_LEV_MAZE);

    /* Initialize map */
    Fread(&filling, 1, sizeof (filling), fd);
    if (!init_lev.init_present) {       /* don't init if mkmap() has been
                                           called */
        for (x = 2; x <= x_maze_max; x++)
            for (y = 0; y <= y_maze_max; y++)
                if (filling == -1) {
                    lev->locations[x][y].typ = (y < 2 ||
                                                ((x % 2) &&
                                                 (y % 2))) ? STONE : HWALL;
                } else {
                    lev->locations[x][y].typ = filling;
                }
    }

    /* Start reading the file */
    Fread(&numpart, 1, sizeof (numpart), fd);
    /* Number of parts */
    if (!numpart || numpart > 9)
        panic("load_maze error: numpart = %d", (int)numpart);

    while (numpart--) {
        Fread(&halign, 1, sizeof (halign), fd);
        /* Horizontal alignment */
        Fread(&valign, 1, sizeof (valign), fd);
        /* Vertical alignment */
        Fread(&xsize, 1, sizeof (xsize), fd);
        /* size in X */
        Fread(&ysize, 1, sizeof (ysize), fd);
        /* size in Y */
        switch ((int)halign) {
        case LEFT:
            xstart = 1;
            break;
        case H_LEFT:
            xstart = 2 + ((x_maze_max - 2 - xsize) / 4);
            break;
        case CENTER:
            xstart = 2 + ((x_maze_max - 2 - xsize) / 2);
            break;
        case H_RIGHT:
            xstart = 2 + ((x_maze_max - 2 - xsize) * 3 / 4);
            break;
        case RIGHT:
            xstart = x_maze_max - xsize - 1;
            break;
        }
        switch ((int)valign) {
        case TOP:
            ystart = 1;
            break;
        case CENTER:
            ystart = 2 + ((y_maze_max - 2 - ysize) / 2);
            break;
        case BOTTOM:
            ystart = y_maze_max - ysize - 1;
            break;
        }
        if (!(xstart % 2))
            xstart++;
        if (!(ystart % 2))
            ystart++;
        if ((ystart < 0) || (ystart + ysize > ROWNO)) {
            /* try to move the start a bit */
            ystart += (ystart > 0) ? -2 : 2;
            if (ysize == ROWNO)
                ystart = 0;
            if (ystart < 0 || ystart + ysize > ROWNO)
                panic("reading special level with ysize too large");
        }

        /* 
         * If any CROSSWALLs are found, must change to ROOM after REGION's
         * are laid out.  CROSSWALLS are used to specify "invisible"
         * boundaries where DOOR syms look bad or aren't desirable.
         */
        has_bounds = FALSE;

        if (init_lev.init_present && xsize <= 1 && ysize <= 1) {
            xstart = 0;
            ystart = 0;
            xsize = COLNO;
            ysize = ROWNO;
        } else {
            /* Load the map */
            for (y = ystart; y < ystart + ysize; y++)
                for (x = xstart; x < xstart + xsize; x++) {
                    lev->locations[x][y].typ = Fgetc(fd);
                    lev->locations[x][y].lit = FALSE;
                    /* clear out lev->locations: load_common_data may set them
                       */
                    lev->locations[x][y].flags = 0;
                    lev->locations[x][y].horizontal = 0;
                    lev->locations[x][y].roomno = 0;
                    lev->locations[x][y].edge = 0;
                    /* 
                     * Note: Even though lev->locations[x][y].typ is type schar,
                     *   lev_comp.y saves it as type char. Since schar != char
                     *   all the time we must make this exception or hack
                     *   through lev_comp.y to fix.
                     */

                    /* 
                     *  Set secret doors to closed (why not trapped too?).  Set
                     *  the horizontal bit.
                     */
                    if (lev->locations[x][y].typ == SDOOR ||
                        IS_DOOR(lev->locations[x][y].typ)) {
                        if (lev->locations[x][y].typ == SDOOR)
                            lev->locations[x][y].doormask = D_CLOSED;
                        /* 
                         *  If there is a wall to the left that connects to a
                         *  (secret) door, then it is horizontal.  This does
                         *  not allow (secret) doors to be corners of rooms.
                         */
                        if (x != xstart &&
                            (IS_WALL(lev->locations[x - 1][y].typ) ||
                             lev->locations[x - 1][y].horizontal))
                            lev->locations[x][y].horizontal = 1;
                    } else if (lev->locations[x][y].typ == HWALL ||
                               lev->locations[x][y].typ == IRONBARS)
                        lev->locations[x][y].horizontal = 1;
                    else if (lev->locations[x][y].typ == LAVAPOOL)
                        lev->locations[x][y].lit = 1;
                    else if (lev->locations[x][y].typ == CROSSWALL)
                        has_bounds = TRUE;
                    Map[x][y] = 1;
                }
            if (init_lev.init_present && init_lev.joined)
                remove_rooms(lev, xstart, ystart, xstart + xsize,
                             ystart + ysize);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of level regions */
        if (n) {
            if (num_lregions) {
                /* realloc the lregion space to add the new ones */
                /* don't really free it up until the whole level is done */
                lev_region *newl =
                    malloc(sizeof (lev_region) * (unsigned)(n + num_lregions));
                memcpy((newl + n), (void *)lregions,
                       sizeof (lev_region) * num_lregions);
                Free(lregions);
                num_lregions += n;
                lregions = newl;
            } else {
                num_lregions = n;
                lregions = malloc(sizeof (lev_region) * n);
            }
        }

        while (n--) {
            Fread(&tmplregion, sizeof (tmplregion), 1, fd);
            if ((size = tmplregion.rname.len) != 0) {
                tmplregion.rname.str = malloc((unsigned)size + 1);
                Fread(tmplregion.rname.str, size, 1, fd);
                tmplregion.rname.str[size] = '\0';
            } else
                tmplregion.rname.str = NULL;
            if (!tmplregion.in_islev) {
                get_location(lev, &tmplregion.inarea.x1, &tmplregion.inarea.y1,
                             DRY | WET);
                get_location(lev, &tmplregion.inarea.x2, &tmplregion.inarea.y2,
                             DRY | WET);
            }
            if (!tmplregion.del_islev) {
                get_location(lev, &tmplregion.delarea.x1,
                             &tmplregion.delarea.y1, DRY | WET);
                get_location(lev, &tmplregion.delarea.x2,
                             &tmplregion.delarea.y2, DRY | WET);
            }
            lregions[(int)n] = tmplregion;
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Random objects */
        if (n) {
            Fread(robjects, sizeof (*robjects), (int)n, fd);
            sp_lev_shuffle(robjects, NULL, (int)n, lev);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Random locations */
        if (n) {
            Fread(rloc_x, sizeof (*rloc_x), (int)n, fd);
            Fread(rloc_y, sizeof (*rloc_y), (int)n, fd);
            sp_lev_shuffle(rloc_x, rloc_y, (int)n, lev);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Random monsters */
        if (n) {
            Fread(rmonst, sizeof (*rmonst), (int)n, fd);
            sp_lev_shuffle(rmonst, NULL, (int)n, lev);
        }

        memset(mustfill, 0, sizeof (mustfill));
        Fread(&n, 1, sizeof (n), fd);
        /* Number of subrooms */
        while (n--) {
            struct mkroom *troom;

            Fread(&tmpregion, 1, sizeof (tmpregion), fd);

            if (tmpregion.rtype > MAXRTYPE) {
                tmpregion.rtype -= MAXRTYPE + 1;
                prefilled = TRUE;
            } else
                prefilled = FALSE;

            if (tmpregion.rlit < 0)
                tmpregion.rlit = !!(mrn2(1 + abs(depth(&lev->z))) <= 11 &&
                                    mrn2(77));

            get_location(lev, &tmpregion.x1, &tmpregion.y1, DRY | WET);
            get_location(lev, &tmpregion.x2, &tmpregion.y2, DRY | WET);

            /* for an ordinary room, `prefilled' is a flag to force an actual
               room to be created (such rooms are used to control placement of
               migrating monster arrivals) */
            room_not_needed = (tmpregion.rtype == OROOM && !tmpregion.rirreg &&
                               !prefilled);
            if (room_not_needed || lev->nroom >= MAXNROFROOMS) {
                if (!room_not_needed)
                    impossible("Too many rooms on new level!");
                light_region(lev, &tmpregion);
                continue;
            }

            troom = &lev->rooms[lev->nroom];

            /* mark rooms that must be filled, but do it later */
            if (tmpregion.rtype != OROOM)
                mustfill[lev->nroom] = (prefilled ? 2 : 1);

            if (tmpregion.rirreg) {
                min_rx = max_rx = tmpregion.x1;
                min_ry = max_ry = tmpregion.y1;
                flood_fill_rm(lev, tmpregion.x1, tmpregion.y1,
                              lev->nroom + ROOMOFFSET, tmpregion.rlit, TRUE);
                add_room(lev, min_rx, min_ry, max_rx, max_ry, FALSE,
                         tmpregion.rtype, TRUE);
                troom->rlit = tmpregion.rlit;
                troom->irregular = TRUE;
            } else {
                add_room(lev, tmpregion.x1, tmpregion.y1, tmpregion.x2,
                         tmpregion.y2, tmpregion.rlit, tmpregion.rtype, TRUE);
                topologize(lev, troom); /* set roomno */
            }
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of doors */
        while (n--) {
            struct mkroom *croom = &lev->rooms[0];

            Fread(&tmpdoor, 1, sizeof (tmpdoor), fd);

            x = tmpdoor.x;
            y = tmpdoor.y;
            typ = tmpdoor.mask == -1 ? rnddoor(lev) : tmpdoor.mask;

            get_location(lev, &x, &y, DRY);
            if (lev->locations[x][y].typ != SDOOR)
                lev->locations[x][y].typ = DOOR;
            else {
                if (typ < D_CLOSED)
                    typ = D_CLOSED;     /* force it to be closed */
            }
            lev->locations[x][y].doormask = typ;

            /* Now the complicated part, list it with each subroom */
            /* The dog move and mail daemon routines use this */
            while (croom->hx >= 0 && lev->doorindex < DOORMAX) {
                if (croom->hx >= x - 1 && croom->lx <= x + 1 &&
                    croom->hy >= y - 1 && croom->ly <= y + 1) {
                    /* Found it */
                    add_door(lev, x, y, croom);
                }
                croom++;
            }
        }

        /* now that we have rooms _and_ associated doors, fill the rooms */
        for (n = 0; n < SIZE(mustfill); n++)
            if (mustfill[(int)n])
                fill_room(lev, &lev->rooms[(int)n], (mustfill[(int)n] == 2));

        /* if special boundary syms (CROSSWALL) in map, remove them now */
        if (has_bounds) {
            for (x = xstart; x < xstart + xsize; x++)
                for (y = ystart; y < ystart + ysize; y++)
                    if (lev->locations[x][y].typ == CROSSWALL)
                        lev->locations[x][y].typ = ROOM;
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of drawbridges */
        while (n--) {
            Fread(&tmpdb, 1, sizeof (tmpdb), fd);

            x = tmpdb.x;
            y = tmpdb.y;
            get_location(lev, &x, &y, DRY | WET);

            if (!create_drawbridge(lev, x, y, tmpdb.dir, tmpdb.db_open))
                impossible("Cannot create drawbridge.");
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of mazewalks */
        while (n--) {
            Fread(&tmpwalk, 1, sizeof (tmpwalk), fd);

            get_location(lev, &tmpwalk.x, &tmpwalk.y, DRY | WET);

            walklist[nwalk++] = tmpwalk;
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of non_diggables */
        while (n--) {
            Fread(&tmpdig, 1, sizeof (tmpdig), fd);

            get_location(lev, &tmpdig.x1, &tmpdig.y1, DRY | WET);
            get_location(lev, &tmpdig.x2, &tmpdig.y2, DRY | WET);

            set_wall_property(lev, tmpdig.x1, tmpdig.y1, tmpdig.x2, tmpdig.y2,
                              W_NONDIGGABLE);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of non_passables */
        while (n--) {
            Fread(&tmpdig, 1, sizeof (tmpdig), fd);

            get_location(lev, &tmpdig.x1, &tmpdig.y1, DRY | WET);
            get_location(lev, &tmpdig.x2, &tmpdig.y2, DRY | WET);

            set_wall_property(lev, tmpdig.x1, tmpdig.y1, tmpdig.x2, tmpdig.y2,
                              W_NONPASSWALL);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of ladders */
        while (n--) {
            Fread(&tmplad, 1, sizeof (tmplad), fd);

            x = tmplad.x;
            y = tmplad.y;
            get_location(lev, &x, &y, DRY);

            lev->locations[x][y].typ = LADDER;
            if (tmplad.up == 1) {
                lev->upladder.sx = x;
                lev->upladder.sy = y;
                lev->locations[x][y].ladder = LA_UP;
            } else {
                lev->dnladder.sx = x;
                lev->dnladder.sy = y;
                lev->locations[x][y].ladder = LA_DOWN;
            }
        }

        prevstair.x = prevstair.y = 0;
        Fread(&n, 1, sizeof (n), fd);
        /* Number of stairs */
        while (n--) {
            Fread(&tmpstair, 1, sizeof (tmpstair), fd);

            xi = 0;
            do {
                x = tmpstair.x;
                y = tmpstair.y;
                get_location(lev, &x, &y, DRY);
            } while (prevstair.x && xi++ < 100 &&
                     distmin(x, y, prevstair.x, prevstair.y) <= 8);
            if ((badtrap = t_at(lev, x, y)) != 0)
                deltrap(lev, badtrap);
            mkstairs(lev, x, y, (char)tmpstair.up, NULL);
            prevstair.x = x;
            prevstair.y = y;
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of altars */
        while (n--) {
            Fread(&tmpaltar, 1, sizeof (tmpaltar), fd);

            create_altar(lev, &tmpaltar, NULL);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of fountains */
        while (n--) {
            Fread(&tmpfountain, 1, sizeof (tmpfountain), fd);

            create_feature(lev, tmpfountain.x, tmpfountain.y, NULL, FOUNTAIN);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of traps */
        while (n--) {
            Fread(&tmptrap, 1, sizeof (tmptrap), fd);

            create_trap(lev, &tmptrap, NULL);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of monsters */
        while (n--) {
            load_one_monster(fd, &tmpmons);

            create_monster(lev, &tmpmons, NULL);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of objects */
        while (n--) {
            load_one_object(fd, &tmpobj);

            create_object(lev, &tmpobj, NULL);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of gold piles */
        while (n--) {
            Fread(&tmpgold, 1, sizeof (tmpgold), fd);

            create_gold(lev, &tmpgold, NULL);
        }

        Fread(&n, 1, sizeof (n), fd);
        /* Number of engravings */
        while (n--) {
            load_one_engraving(fd, &tmpengraving);

            create_engraving(lev, &tmpengraving, NULL);
        }

    }   /* numpart loop */

    nwalk_sav = nwalk;
    while (nwalk--) {
        x = (xchar) walklist[nwalk].x;
        y = (xchar) walklist[nwalk].y;
        dir = walklist[nwalk].dir;

        /* don't use move() - it doesn't use W_NORTH, etc. */
        switch (dir) {
        case W_NORTH:
            --y;
            break;
        case W_SOUTH:
            y++;
            break;
        case W_EAST:
            x++;
            break;
        case W_WEST:
            --x;
            break;
        default:
            panic("load_maze: bad MAZEWALK direction");
        }

        if (!IS_DOOR(lev->locations[x][y].typ)) {
            lev->locations[x][y].typ = ROOM;
            lev->locations[x][y].flags = 0;
        }

        /* 
         * We must be sure that the parity of the coordinates for
         * walkfrom() is odd.  But we must also take into account
         * what direction was chosen.
         */
        if (!(x % 2)) {
            if (dir == W_EAST)
                x++;
            else
                x--;

            /* no need for IS_DOOR check; out of map bounds */
            lev->locations[x][y].typ = ROOM;
            lev->locations[x][y].flags = 0;
        }

        if (!(y % 2)) {
            if (dir == W_SOUTH)
                y++;
            else
                y--;
        }

        walkfrom(lev, x, y);
    }
    wallification(lev, 0, 0, COLNO - 1, ROWNO - 1);

    /* 
     * If there's a significant portion of maze unused by the special level,
     * we don't want it empty.
     *
     * Makes the number of traps, monsters, etc. proportional
     * to the size of the maze.
     */
    mapcountmax = mapcount = (x_maze_max - 2) * (y_maze_max - 2);

    for (x = 2; x < x_maze_max; x++)
        for (y = 0; y < y_maze_max; y++)
            if (Map[x][y])
                mapcount--;

    if (nwalk_sav && (mapcount > (int)(mapcountmax / 10))) {
        mapfact = (int)((mapcount * 100L) / mapcountmax);
        for (x = 1 + mrn2((int)(20 * mapfact) / 100); x; x--) {
            maze1xy(lev, &mm, DRY);
            mkobj_at(mrn2(2) ? GEM_CLASS : RANDOM_CLASS, lev, mm.x, mm.y,
                     TRUE, mrng());
        }
        for (x = 1 + mrn2((int)(12 * mapfact) / 100); x; x--) {
            maze1xy(lev, &mm, DRY);
            mksobj_at(BOULDER, lev, mm.x, mm.y, TRUE, FALSE, mrng());
        }
        for (x = 1 + mrn2(2); x; x--) {
            maze1xy(lev, &mm, DRY);
            makemon(&mons[PM_MINOTAUR], lev, mm.x, mm.y, MM_ALLLEVRNG);
        }
        for (x = 1 + mrn2((int)(12 * mapfact) / 100); x; x--) {
            maze1xy(lev, &mm, WET | DRY);
            makemon(NULL, lev, mm.x, mm.y, MM_ALLLEVRNG);
        }
        for (x = mrn2((int)(15 * mapfact) / 100); x; x--) {
            maze1xy(lev, &mm, DRY);
            mkgold(0L, lev, mm.x, mm.y, mrng());
        }
        for (x = mrn2((int)(15 * mapfact) / 100); x; x--) {
            int trytrap;

            maze1xy(lev, &mm, DRY);
            trytrap = rndtrap(lev);
            if (sobj_at(BOULDER, lev, mm.x, mm.y))
                while (trytrap == PIT || trytrap == SPIKED_PIT ||
                       trytrap == TRAPDOOR || trytrap == HOLE)
                    trytrap = rndtrap(lev);
            maketrap(lev, mm.x, mm.y, trytrap, mrng());
        }
    }
    return TRUE;

err_out:
    fprintf(stderr, "read error in load_maze\n");
    return FALSE;
}

/*
 * General loader
 */
boolean
load_special(struct level *lev, const char *name, int *smeq)
{
    dlb *fd;
    boolean result = FALSE;
    char c;
    struct version_info vers_info;

    fd = dlb_fopen(name, RDBMODE);
    if (!fd)
        return FALSE;

    Fread(&vers_info, sizeof vers_info, 1, fd);
    if (!check_version(&vers_info, name, TRUE))
        goto give_up;

    Fread(&c, sizeof c, 1, fd); /* c Header */

    switch (c) {
    case SP_LEV_ROOMS:
        result = load_rooms(lev, fd, smeq);
        break;
    case SP_LEV_MAZE:
        result = load_maze(lev, fd);
        break;
    default:   /* ??? */
        result = FALSE;
    }

give_up:
    dlb_fclose(fd);
    return result;

err_out:
    fprintf(stderr, "read error in load_special\n");
    return FALSE;
}

boolean was_waterlevel;  /* ugh... this shouldn't be needed */

/* this is special stuff that the level compiler cannot (yet) handle */
void
fixup_special(struct level *lev)
{
    lev_region *r = lregions;
    struct d_level lvl;
    int x, y;
    struct mkroom *croom;
    boolean added_branch = FALSE;

    if (was_waterlevel) {
        was_waterlevel = FALSE;
        u.uinwater = 0;
        free_waterlevel();
    } else if (Is_waterlevel(&lev->z)) {
        lev->flags.hero_memory = 0;
        was_waterlevel = TRUE;
        /* water level is an odd beast - it has to be set up before calling
           place_lregions etc. */
        setup_waterlevel(lev);
    }
    for (x = 0; x < num_lregions; x++, r++) {
        switch (r->rtype) {
        case LR_BRANCH:
            added_branch = TRUE;
            goto place_it;

        case LR_PORTAL:
            if (*r->rname.str >= '0' && *r->rname.str <= '9') {
                /* "chutes and ladders" */
                lvl = lev->z;
                lvl.dlevel = atoi(r->rname.str);
            } else {
                s_level *sp = find_level(r->rname.str);

                lvl = sp->dlevel;
            }
            /* fall into... */

        case LR_UPSTAIR:
        case LR_DOWNSTAIR:
        place_it:
            place_lregion(lev, r->inarea.x1, r->inarea.y1, r->inarea.x2,
                          r->inarea.y2, r->delarea.x1, r->delarea.y1,
                          r->delarea.x2, r->delarea.y2, r->rtype, &lvl);
            break;

        case LR_TELE:
        case LR_UPTELE:
        case LR_DOWNTELE:
            /* save the region outlines for goto_level() */
            if (r->rtype == LR_TELE || r->rtype == LR_UPTELE) {
                lev->updest.lx = r->inarea.x1;
                lev->updest.ly = r->inarea.y1;
                lev->updest.hx = r->inarea.x2;
                lev->updest.hy = r->inarea.y2;
                lev->updest.nlx = r->delarea.x1;
                lev->updest.nly = r->delarea.y1;
                lev->updest.nhx = r->delarea.x2;
                lev->updest.nhy = r->delarea.y2;
            }
            if (r->rtype == LR_TELE || r->rtype == LR_DOWNTELE) {
                lev->dndest.lx = r->inarea.x1;
                lev->dndest.ly = r->inarea.y1;
                lev->dndest.hx = r->inarea.x2;
                lev->dndest.hy = r->inarea.y2;
                lev->dndest.nlx = r->delarea.x1;
                lev->dndest.nly = r->delarea.y1;
                lev->dndest.nhx = r->delarea.x2;
                lev->dndest.nhy = r->delarea.y2;
            }
            /* place_lregion gets called from goto_level() */
            break;
        }

        if (r->rname.str)
            free(r->rname.str), r->rname.str = 0;
    }

    /* place dungeon branch if not placed above */
    if (!added_branch && Is_branchlev(&lev->z)) {
        place_lregion(lev, COLNO, ROWNO, COLNO, ROWNO, COLNO, ROWNO, COLNO, ROWNO, LR_BRANCH, NULL);
    }

    /* KMH -- Sokoban levels */
    if (In_sokoban(&lev->z))
        sokoban_detect(lev);

    /* Still need to add some stuff to level file */
    if (Is_medusa_level(&lev->z)) {
        struct obj *otmp;
        int tryct;

        croom = &lev->rooms[0]; /* only one room on the medusa level */
        for (tryct = 1 + mrn2(4); tryct; tryct--) {
            x = somex(croom, mrng());
            y = somey(croom, mrng());
            if (goodpos(lev, x, y, NULL, 0)) {
                otmp = mk_tt_object(lev, STATUE, x, y);
                while (otmp &&
                       (poly_when_stoned(&mons[otmp->corpsenm]) ||
                        pm_resistance(&mons[otmp->corpsenm], MR_STONE))) {
                    /* top ten table has an input into this, so... */
                    otmp->corpsenm = rndmonnum(&lev->z, rng_main);
                    otmp->owt = weight(otmp);
                }
            }
        }

        if (mrn2(2)) {
            y = somey(croom, mrng());
            x = somex(croom, mrng());
            otmp = mk_tt_object(lev, STATUE, x, y);
        } else {        /* Medusa statues don't contain books */
            y = somey(croom, mrng());
            x = somex(croom, mrng());
            otmp = mkcorpstat(STATUE, NULL, NULL, lev, x, y, FALSE, mrng());
        }
        if (otmp) {
            while (otmp->corpsenm < LOW_PM
                   || pm_resistance(&mons[otmp->corpsenm], MR_STONE)
                   || poly_when_stoned(&mons[otmp->corpsenm])) {
                otmp->corpsenm = rndmonnum(&lev->z, rng_main);
                otmp->owt = weight(otmp);
            }
        }
    } else if (Is_wiz1_level(&lev->z)) {
        croom = search_special(lev, MORGUE);

        create_secret_door(lev, croom, W_SOUTH | W_EAST | W_WEST);
    } else if (Is_knox(&lev->z)) {
        /* using an unfilled morgue for rm id */
        croom = search_special(lev, MORGUE);
        /* avoid inappropriate morgue-related messages */
        lev->flags.graveyard = 0;
        croom->rtype = OROOM;   /* perhaps it should be set to VAULT? */
        /* stock the main vault */
        for (x = croom->lx; x <= croom->hx; x++)
            for (y = croom->ly; y <= croom->hy; y++) {
                mkgold(600 + mrn2(300), lev, x, y, mrng());
                if (!mrn2(3) && !is_pool(lev, x, y))
                    maketrap(lev, x, y, mrn2(3) ? LANDMINE : SPIKED_PIT,
                             mrng());
            }
    } else if (Role_if(PM_PRIEST) && In_quest(&lev->z)) {
        /* less chance for undead corpses (lured from lower morgues) */
        lev->flags.graveyard = 1;
    } else if (Is_stronghold(&lev->z)) {
        lev->flags.graveyard = 1;

        /* ensure that the wand of wishing chest is not trapped */
        struct obj *cont;
        for (cont = lev->objlist; cont; cont = cont->nobj) {
            if (cont->otyp == CHEST) {
                struct obj *otmp;
                for (otmp = cont->cobj; otmp; otmp = otmp->nobj) {
                    if (otmp->otyp == WAN_WISHING)
                        cont->otrapped = 0;
                }
            }
        }
    } else if (Is_sanctum(&lev->z)) {
        croom = search_special(lev, TEMPLE);

        create_secret_door(lev, croom, W_ANY);
    } else if (on_level(&lev->z, &orcus_level)) {
        struct monst *mtmp, *mtmp2;

        /* it's a ghost town, get rid of shopkeepers */
        for (mtmp = lev->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon;
            if (mtmp->isshk)
                mongone(mtmp);
        }
    }

    if (lev_message) {
        char *str, *nl;

        for (str = lev_message; (nl = strchr(str, '\n')) != 0; str = nl + 1) {
            *nl = '\0';
            pline("%s", str);
        }
        if (*str)
            pline("%s", str);
        free(lev_message);
        lev_message = 0;
    }

    if (lregions)
        free(lregions), lregions = 0;
    num_lregions = 0;
}

/*sp_lev.c*/

