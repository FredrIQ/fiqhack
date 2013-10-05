/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "sp_lev.h"
#include "lev.h"        /* save & restore info */

/* from sp_lev.c, for fixup_special() */
extern char *lev_message;
extern lev_region *lregions;
extern int num_lregions;

static boolean iswall(struct level *lev, int x, int y);
static boolean iswall_or_stone(struct level *lev, int x, int y);
static boolean is_solid(struct level *lev, int x, int y);
static int extend_spine(int[3][3], int, int, int);
static boolean okay(struct level *lev, int x, int y, int dir);
static void maze0xy(coord *);
static boolean put_lregion_here(struct level *lev, xchar, xchar, xchar, xchar,
                                xchar, xchar, xchar, boolean, d_level *);
static void fixup_special(struct level *lev);
static void move(int *, int *, int);
static void setup_waterlevel(struct level *lev);
static boolean bad_location(struct level *lev, xchar x, xchar y, xchar lx,
                            xchar ly, xchar hx, xchar hy);


static boolean bubble_up;


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
                locale[nx][0] && locale[nx][2]) {       /* diag are wall/stone */
                spine = 0;
            } else {
                spine = 1;
            }
        } else {        /* dy */
            if (locale[0][1] && locale[2][1] && /* NS are wall/stone */
                locale[0][ny] && locale[2][ny]) {       /* diag are wall/stone */
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
wallification(struct level *lev, int x1, int y1, int x2, int y2)
{
    uchar type;
    int x, y;
    struct rm *loc;
    int bits;
    int locale[3][3];   /* rock or wall status surrounding positions */

    /* 
     * Value 0 represents a free-standing wall.  It could be anything,
     * so even though this table says VWALL, we actually leave whatever
     * typ was there alone.
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
        }
}

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

/* find random starting point for maze generation */
static void
maze0xy(coord * cc)
{
    cc->x = 3 + 2 * rn2((x_maze_max >> 1) - 1);
    cc->y = 3 + 2 * rn2((y_maze_max >> 1) - 1);
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

/* pick a location in area (lx, ly, hx, hy) but not in (nlx, nly, nhx, nhy) */
/* and place something (based on rtype) in that region */
void
place_lregion(struct level *lev, xchar lx, xchar ly, xchar hx, xchar hy,
              xchar nlx, xchar nly, xchar nhx, xchar nhy, xchar rtype,
              d_level * dest_lvl)
{
    int trycnt;
    boolean oneshot;
    xchar x, y;

    if (!lx) {  /* default to whole level */
        /* 
         * if there are rooms and this a branch, let place_branch choose
         * the branch location (to avoid putting branches in corridors).
         */
        if (rtype == LR_BRANCH && lev->nroom) {
            place_branch(lev, Is_branchlev(&lev->z), 0, 0);
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
        x = rn1((hx - lx) + 1, lx);
        y = rn1((hy - ly) + 1, ly);
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

static boolean was_waterlevel;  /* ugh... this shouldn't be needed */

/* this is special stuff that the level compiler cannot (yet) handle */
static void
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
        place_lregion(lev, 0, 0, 0, 0, 0, 0, 0, 0, LR_BRANCH, NULL);
    }

    /* KMH -- Sokoban levels */
    if (In_sokoban(&lev->z))
        sokoban_detect(lev);

    /* Still need to add some stuff to level file */
    if (Is_medusa_level(&lev->z)) {
        struct obj *otmp;
        int tryct;

        croom = &lev->rooms[0]; /* only one room on the medusa level */
        for (tryct = rnd(4); tryct; tryct--) {
            x = somex(croom);
            y = somey(croom);
            if (goodpos(lev, x, y, NULL, 0)) {
                otmp = mk_tt_object(lev, STATUE, x, y);
                while (otmp &&
                       (poly_when_stoned(&mons[otmp->corpsenm]) ||
                        pm_resistance(&mons[otmp->corpsenm], MR_STONE))) {
                    otmp->corpsenm = rndmonnum(&lev->z);
                    otmp->owt = weight(otmp);
                }
            }
        }

        if (rn2(2)) {
            y = somey(croom);
            x = somex(croom);
            otmp = mk_tt_object(lev, STATUE, x, y);
        } else {        /* Medusa statues don't contain books */
            y = somey(croom);
            x = somex(croom);
            otmp = mkcorpstat(STATUE, NULL, NULL, lev, x, y, FALSE);
        }
        if (otmp) {
            while (pm_resistance(&mons[otmp->corpsenm], MR_STONE)
                   || poly_when_stoned(&mons[otmp->corpsenm])) {
                otmp->corpsenm = rndmonnum(&lev->z);
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
        lev->flags.graveyard = lev->flags.has_morgue = 0;
        croom->rtype = OROOM;   /* perhaps it should be set to VAULT? */
        /* stock the main vault */
        for (x = croom->lx; x <= croom->hx; x++)
            for (y = croom->ly; y <= croom->hy; y++) {
                mkgold((long)rn1(300, 600), lev, x, y);
                if (!rn2(3) && !is_pool(lev, x, y))
                    maketrap(lev, x, y, rn2(3) ? LANDMINE : SPIKED_PIT);
            }
    } else if (Role_if(PM_PRIEST) && In_quest(&lev->z)) {
        /* less chance for undead corpses (lured from lower morgues) */
        lev->flags.graveyard = 1;
    } else if (Is_stronghold(&lev->z)) {
        lev->flags.graveyard = 1;
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

void
makemaz(struct level *lev, const char *s)
{
    int x, y;
    char protofile[20];
    s_level *sp = Is_special(&lev->z);
    coord mm;

    if (*s) {
        if (sp && sp->rndlevs)
            sprintf(protofile, "%s-%d", s, rnd((int)sp->rndlevs));
        else
            strcpy(protofile, s);
    } else if (*(dungeons[lev->z.dnum].proto)) {
        if (dunlevs_in_dungeon(&lev->z) > 1) {
            if (sp && sp->rndlevs)
                sprintf(protofile, "%s%d-%d", dungeons[lev->z.dnum].proto,
                        dunlev(&lev->z), rnd((int)sp->rndlevs));
            else
                sprintf(protofile, "%s%d", dungeons[lev->z.dnum].proto,
                        dunlev(&lev->z));
        } else if (sp && sp->rndlevs) {
            sprintf(protofile, "%s-%d", dungeons[lev->z.dnum].proto,
                    rnd((int)sp->rndlevs));
        } else
            strcpy(protofile, dungeons[lev->z.dnum].proto);

    } else
        strcpy(protofile, "");

    /* SPLEVTYPE format is "level-choice,level-choice"... */
    if (wizard && *protofile && sp && sp->rndlevs) {
        char *ep = getenv("SPLEVTYPE"); /* not nh_getenv */

        if (ep) {
            /* rindex always succeeds due to code in prior block */
            int len = (strrchr(protofile, '-') - protofile) + 1;

            while (ep && *ep) {
                if (!strncmp(ep, protofile, len)) {
                    int pick = atoi(ep + len);

                    /* use choice only if valid */
                    if (pick > 0 && pick <= (int)sp->rndlevs)
                        sprintf(protofile + len, "%d", pick);
                    break;
                } else {
                    ep = strchr(ep, ',');
                    if (ep)
                        ++ep;
                }
            }
        }
    }

    if (*protofile) {
        strcat(protofile, LEV_EXT);
        if (load_special(lev, protofile)) {
            fixup_special(lev);
            /* some levels can end up with monsters on dead mon list, including 
               light source monsters */
            dmonsfree(lev);
            return;     /* no mazification right now */
        }
        impossible("Couldn't load \"%s\" - making a maze.", protofile);
    }

    lev->flags.is_maze_lev = TRUE;

    for (x = 2; x <= x_maze_max; x++)
        for (y = 2; y <= y_maze_max; y++)
            lev->locations[x][y].typ = ((x % 2) && (y % 2)) ? STONE : HWALL;

    maze0xy(&mm);
    walkfrom(lev, mm.x, mm.y);
    /* put a boulder at the maze center */
    mksobj_at(BOULDER, lev, (int)mm.x, (int)mm.y, TRUE, FALSE);

    wallification(lev, 2, 2, x_maze_max, y_maze_max);
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

        inv_pos.x = inv_pos.y = 0;      /* {occupied() => invocation_pos()} */
        do {
            x = rn1(x_range, x_maze_min + INVPOS_X_MARGIN + 1);
            y = rn1(y_range, y_maze_min + INVPOS_Y_MARGIN + 1);
            /* we don't want it to be too near the stairs, nor to be on a spot
               that's already in use (wall|trap) */
        } while (x == lev->upstair.sx || y == lev->upstair.sy ||
                 /* (direct line) */
                 abs(x - lev->upstair.sx) == abs(y - lev->upstair.sy) ||
                 distmin(x, y, lev->upstair.sx,
                         lev->upstair.sy) <= INVPOS_DISTANCE ||
                 !SPACE_POS(lev->locations[x][y].typ) || occupied(lev, x, y));
        inv_pos.x = x;
        inv_pos.y = y;
        maketrap(lev, inv_pos.x, inv_pos.y, VIBRATING_SQUARE);
#undef INVPOS_X_MARGIN
#undef INVPOS_Y_MARGIN
#undef INVPOS_DISTANCE
#undef x_maze_min
#undef y_maze_min
    }

    /* place branch stair or portal */
    place_branch(lev, Is_branchlev(&lev->z), 0, 0);

    for (x = rn1(8, 11); x; x--) {
        mazexy(lev, &mm);
        mkobj_at(rn2(2) ? GEM_CLASS : 0, lev, mm.x, mm.y, TRUE);
    }
    for (x = rn1(10, 2); x; x--) {
        mazexy(lev, &mm);
        mksobj_at(BOULDER, lev, mm.x, mm.y, TRUE, FALSE);
    }
    for (x = rn2(3); x; x--) {
        mazexy(lev, &mm);
        makemon(&mons[PM_MINOTAUR], lev, mm.x, mm.y, NO_MM_FLAGS);
    }
    for (x = rn1(5, 7); x; x--) {
        mazexy(lev, &mm);
        makemon(NULL, lev, mm.x, mm.y, NO_MM_FLAGS);
    }
    for (x = rn1(6, 7); x; x--) {
        mazexy(lev, &mm);
        mkgold(0L, lev, mm.x, mm.y);
    }
    for (x = rn1(6, 7); x; x--)
        mktrap(lev, 0, 1, NULL, NULL);
}


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
        dir = dirs[rn2(q)];
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


/* find random point in generated corridors,
 * so we don't create items in moats, bunkers, or walls */
void
mazexy(struct level *lev, coord * cc)
{
    int cpt = 0;

    do {
        cc->x = 3 + 2 * rn2((x_maze_max >> 1) - 1);
        cc->y = 3 + 2 * rn2((y_maze_max >> 1) - 1);
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
                lev->locations[x][y].wall_info |= W_NONDIGGABLE;
            }
}

void
mkportal(struct level *lev, xchar x, xchar y, xchar todnum, xchar todlevel)
{
    /* a portal "trap" must be matched by a */
    /* portal in the destination dungeon/dlevel */
    struct trap *ttmp = maketrap(lev, x, y, MAGIC_PORTAL);

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
static int xmin, ymin, xmax, ymax;      /* level boundaries */

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

    vision_recalc(2);
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
                            remove_worm(mon);
                        else
                            remove_monster(level, x, y);

                        newsym(x, y);   /* clean up old position */
                        mon->mx = mon->my = 0;
                    }
                    if (!u.uswallow && x == u.ux && y == u.uy) {
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
                    block_point(x, y);
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
    vision_full_recalc = 1;
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
        pline("Water turbulence affects your movements.");
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
    mwrite32(mf, xmin);
    mwrite32(mf, ymin);
    mwrite32(mf, xmax);
    mwrite32(mf, ymax);
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
    xmin = mread32(mf);
    ymin = mread32(mf);
    xmax = mread32(mf);
    ymax = mread32(mf);
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

int
waterbody_prefix(xchar x, xchar y)
{
    return (Is_waterlevel(&u.uz) || is_lava(level, x, y) ||
            !strcmp(waterbody_name(x, y), "water"))
        ? KILLED_BY : KILLED_BY_AN;
}

const char *
waterbody_name(xchar x, xchar y)
{
    struct rm *loc;
    schar ltyp;

    if (!isok(x, y))
        return "drink"; /* should never happen */
    loc = &level->locations[x][y];
    ltyp = loc->typ;

    if (is_lava(level, x, y))
        return "lava";
    else if (is_ice(level, x, y))
        return "ice";
    else if (is_moat(level, x, y))
        return "moat";
    else if ((ltyp != POOL) && (ltyp != WATER) && Is_juiblex_level(&u.uz))
        return "swamp";
    else if (ltyp == POOL)
        return "pool of water";
    else
        return "water";
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

static void
setup_waterlevel(struct level *lev)
{
    int x, y;
    int xskip, yskip;

    /* ouch, hardcoded... */

    xmin = 3;
    ymin = 1;
    xmax = 78;
    ymax = 20;

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
    b->dx = 1 - rn2(3);
    b->dy = 1 - rn2(3);
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
        /* pline("mv_bubble: dx = %d, dy = %d", dx, dy); */
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
        pline("bubble xmin: x = %d, xmin = %d", b->x, bxmin);
        b->x = bxmin;
    }
    if (b->y < bymin) {
        pline("bubble ymin: y = %d, ymin = %d", b->y, bymin);
        b->y = bymin;
    }
    if ((int)(b->x + b->bm[0] - 1) > bxmax) {
        pline("bubble xmax: x = %d, xmax = %d", b->x + b->bm[0] - 1, bxmax);
        b->x = bxmax - b->bm[0] + 1;
    }
    if ((int)(b->y + b->bm[1] - 1) > bymax) {
        pline("bubble ymax: y = %d, ymax = %d", b->y + b->bm[1] - 1, bymax);
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
                unblock_point(x, y);
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
