/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"

static void toss_wsegs(struct level *lev, struct wseg *curr,
                       boolean display_update);
static void shrink_worm(int);
static void random_dir(xchar, xchar, xchar *, xchar *, enum rng);
static struct wseg *create_worm_tail(int);

/*  Description of long worm implementation.
 *
 *  Each monst struct of the head of a tailed worm has a wormno set to
 *      1 <= wormno < MAX_NUM_WORMS
 *  If wormno == 0 this does not mean that the monster is not a worm,
 *  it just means that the monster does not have a long worm tail.
 *
 *  The actual segments of a worm are not full blown monst structs.
 *  They are small wseg structs, and their position in the levels.monsters[][]
 *  array is held by the monst struct of the head of the worm.  This makes
 *  things like probing and hit point bookkeeping much easier.
 *
 *  The segments of the long worms on a level are kept as an array of
 *  singly threaded linked lists.  The wormno variable is used as an index
 *  for these segment arrays.
 *
 *  wtails:     The first (starting struct) of a linked list.  This points
 *              to the tail (last) segment of the worm.
 *
 *  wheads:     The last (end) of a linked list of segments.  This points to
 *              the segment that is at the same position as the real monster
 *              (the head).  Note that the segment that wheads[wormno] points
 *              to, is not displayed.  It is simply there to keep track of
 *              where the head came from, so that worm movement and display are
 *              simplified later.
 *              Keeping the head segment of the worm at the end of the list
 *              of tail segments is an endless source of confusion, but it is
 *              necessary.
 *              From now on, we will use "start" and "end" to refer to the
 *              linked list and "head" and "tail" to refer to the worm.
 *
 *  One final worm array is:
 *
 *  wgrowtime:  This tells us when to add another segment to the worm.
 *
 *  When a worm is moved, we add a new segment at the head, and delete the
 *  segment at the tail (unless we want it to grow).  This new head segment is
 *  located in the same square as the actual head of the worm.  If we want
 *  to grow the worm, we don't delete the tail segment, and we give the worm
 *  extra hit points, which possibly go into its maximum.
 *
 *  Non-moving worms (worm_nomove) are assumed to be surrounded by their own
 *  tail, and, thus, shrink instead of grow (as their tails keep going while
 *  their heads are stopped short).  In this case, we delete the last tail
 *  segment, and remove hit points from the worm.
 * 
 *  (note: wheads, wtails and wgrowtime have been moved into struct level)
 */

/*
 *  get_wormno()
 *
 *  Find an unused worm tail slot and return the index.  A zero means that
 *  there are no slots available.  This means that the worm head can exist,
 *  it just cannot ever grow a tail.
 *
 *  It, also, means that there is an optimisation to made.  The [0] positions
 *  of the arrays are never used.  Meaning, we really *could* have one more
 *  tailed worm on the level, or use a smaller array (using wormno - 1).
 *
 *  Implementation is left to the interested hacker.
 */
int
get_wormno(struct level *lev)
{
    int new_wormno = 1;

    while (new_wormno < MAX_NUM_WORMS) {
        if (!lev->wheads[new_wormno])
            return new_wormno;  /* found an empty wtails[] slot at new_wormno */
        new_wormno++;
    }

    return 0;   /* level infested with worms */
}

/*
 *  initworm()
 *
 *  Use if (mon->wormno = get_wormno()) before calling this function!
 *
 *  Initialize the worm entry.  This will set up the worm grow time, and
 *  create and initialize the dummy segment for wheads[] and wtails[].
 *
 *  If the worm has no tail (ie get_wormno() fails) then this function need
 *  not be called.
 */
void
initworm(struct monst *worm, int wseg_count)
{
    struct wseg *seg, *new_tail = create_worm_tail(wseg_count);
    int wnum = worm->wormno;

/*  if (!wnum) return;  bullet proofing */

    if (new_tail) {
        worm->dlevel->wtails[wnum] = new_tail;
        for (seg = new_tail; seg->nseg; seg = seg->nseg)
            ;
        worm->dlevel->wheads[wnum] = seg;
    } else {
        worm->dlevel->wtails[wnum] = worm->dlevel->wheads[wnum] = seg =
            malloc(sizeof (struct wseg));
        seg->nseg = NULL;
        seg->wx = worm->mx;
        seg->wy = worm->my;
    }
    worm->dlevel->wgrowtime[wnum] = 0L;
}


/*
 *  toss_wsegs()
 *
 *  Get rid of all worm segments on and following the given pointer curr.
 *  The display may or may not need to be updated as we free the segments.
 */
static void
toss_wsegs(struct level *lev, struct wseg *curr, boolean display_update)
{
    struct wseg *seg;

    while (curr) {
        seg = curr->nseg;

        /* remove from level->monsters[][] */

        /* need to check curr->wx for genocided while migrating_mon */
        if (curr->wx) {
            lev->monsters[curr->wx][curr->wy] = NULL;

            /* update screen before deallocation */
            if (display_update && lev == level)
                newsym(curr->wx, curr->wy);
        }

        /* free memory used by the segment */
        free(curr);
        curr = seg;
    }
}


/*
 *  shrink_worm()
 *
 *  Remove the tail segment of the worm (the starting segment of the list).
 */
/* wnum: worm number */
static void
shrink_worm(int wnum)
{
    struct wseg *seg;

    if (level->wtails[wnum] == level->wheads[wnum])
        return; /* no tail */

    seg = level->wtails[wnum];
    level->wtails[wnum] = seg->nseg;
    seg->nseg = NULL;
    toss_wsegs(level, seg, TRUE);
}

/*
 *  worm_move()
 *
 *  Check for mon->wormno before calling this function!
 *
 *  Move the worm.  Maybe grow.
 */
void
worm_move(struct monst *worm)
{
    struct wseg *seg, *new_seg; /* new segment */
    int wnum = worm->wormno;    /* worm number */


/*  if (!wnum) return;  bullet proofing */

    /* 
     *  Place a segment at the old worm head.  The head has already moved.
     */
    seg = level->wheads[wnum];
    place_worm_seg(worm, seg->wx, seg->wy);
    newsym(seg->wx, seg->wy);   /* display the new segment */

    /* 
     *  Create a new dummy segment head and place it at the end of the list.
     */
    new_seg = malloc(sizeof (struct wseg));
    new_seg->wx = worm->mx;
    new_seg->wy = worm->my;
    new_seg->nseg = NULL;
    seg->nseg = new_seg;        /* attach it to the end of the list */
    level->wheads[wnum] = new_seg;      /* move the end pointer */


    if (level->wgrowtime[wnum] <= moves) {
        if (!level->wgrowtime[wnum])
            level->wgrowtime[wnum] = moves + rnd(5);
        else
            level->wgrowtime[wnum] += rn1(15, 3);
        worm->mhp += 3;
        if (worm->mhp > MHPMAX)
            worm->mhp = MHPMAX;
        if (worm->mhp > worm->mhpmax)
            worm->mhpmax = worm->mhp;
    } else
        /* The worm doesn't grow, so the last segment goes away. */
        shrink_worm(wnum);
}

/*
 *  worm_nomove()
 *
 *  Check for mon->wormno before calling this function!
 *
 *  The worm don't move so it should shrink.
 */
void
worm_nomove(struct monst *worm)
{
    shrink_worm((int)worm->wormno);     /* shrink */

    if (worm->mhp > 3)
        worm->mhp -= 3; /* mhpmax not changed ! */
    else
        worm->mhp = 1;
}

/*
 *  wormgone()
 *
 *  Check for mon->wormno before calling this function!
 *
 *  Kill a worm tail.
 */
void
wormgone(struct monst *worm)
{
    struct level *lev = worm->dlevel;
    int wnum = worm->wormno;

/*  if (!wnum) return;  bullet proofing */

    worm->wormno = 0;

    /* This will also remove the real monster (ie 'w') from the its position in 
       level->monsters[][]. */
    toss_wsegs(lev, lev->wtails[wnum], TRUE);

    lev->wheads[wnum] = lev->wtails[wnum] = NULL;
}

/*
 *  wormhitu()
 *
 *  Check for mon->wormno before calling this function!
 *
 *  If the hero is near any part of the worm, the worm will try to attack.
 */
void
wormhitu(struct monst *worm)
{
    int wnum = worm->wormno;
    struct wseg *seg;

/*  if (!wnum) return;  bullet proofing */

/*  This does not work right now because mattacku() thinks that the head is
 *  out of range of the player.  We might try to kludge, and bring the head
 *  within range for a tiny moment, but this needs a bit more looking at
 *  before we decide to do this.
 */
    for (seg = level->wtails[wnum]; seg; seg = seg->nseg)
        if (distu(seg->wx, seg->wy) < 3 && aware_of_u(worm) &&
            !engulfing_u(worm))
            mattackq(worm, worm->mux, worm->muy);
}

/*  cutoff()
*
*  Remove the tail of a worm and adjust the hp of the worm.
*/
static void
cutoff(struct monst *worm, struct wseg *tail)
{
    if (flags.mon_moving)
        pline("Part of the tail of %s is cut off.", mon_nam(worm));
    else
        pline("You cut part of the tail off of %s.", mon_nam(worm));
    toss_wsegs(level, tail, TRUE);
    if (worm->mhp >= 2)
        worm->mhp /= 2;
}

/*  cutworm()
 *
 *  Check for mon->wormno before calling this function!
 *
 *  When hitting a worm (worm) at position x, y, with a weapon (weap),
 *  there is a chance that the worm will be cut in half, and a chance
 *  that both halves will survive.
 */
void
cutworm(struct monst *worm, xchar x, xchar y, struct obj *weap)
{
    struct wseg *curr, *new_tail;
    struct monst *new_worm;
    int wnum = worm->wormno;
    int cut_chance, new_wnum;

    if (!wnum)
        return; /* bullet proofing */

    if (x == worm->mx && y == worm->my)
        return; /* hit on head */

    /* cutting goes best with a bladed weapon */
    cut_chance = rnd(20);       /* Normally 1-16 does not cut */
    /* Normally 17-20 does */

    if (weap && objects[weap->otyp].oc_dir & SLASH)
        cut_chance += 10;       /* With a slashing weapon 1- 6 does not cut */
    /* 7-20 does */

    if (cut_chance < 17)
        return; /* not good enough */

    /* Find the segment that was attacked. */
    curr = level->wtails[wnum];

    while ((curr->wx != x) || (curr->wy != y)) {
        curr = curr->nseg;
        if (!curr) {
            impossible("cutworm: no segment at (%d,%d)", (int)x, (int)y);
            return;
        }
    }

    /* If this is the tail segment, then the worm just loses it. */
    if (curr == level->wtails[wnum]) {
        shrink_worm(wnum);
        return;
    }

    /* 
     *  Split the worm.  The tail for the new worm is the old worm's tail.
     *  The tail for the old worm is the segment that follows "curr",
     *  and "curr" becomes the dummy segment under the new head.
     */
    new_tail = level->wtails[wnum];
    level->wtails[wnum] = curr->nseg;
    curr->nseg = NULL;  /* split the worm */

    /* 
     *  At this point, the old worm is correct.  Any new worm will have
     *  it's head at "curr" and its tail at "new_tail".
     */

    /* Sometimes the tail end dies. */
    if (rn2(3) || !(new_wnum = get_wormno(level)) || !worm->m_lev) {
        cutoff(worm, new_tail);
        return;
    }

    remove_monster(level, x, y);        /* clone_mon puts new head here */
    if (!(new_worm = clone_mon(worm, x, y))) {
        cutoff(worm, new_tail);
        return;
    }
    new_worm->wormno = new_wnum;        /* affix new worm number */

    /* Devalue the monster level of both halves of the worm. */
    worm->m_lev =
        ((unsigned)worm->m_lev <=
         3) ? (unsigned)worm->m_lev : max((unsigned)worm->m_lev - 2, 3);
    new_worm->m_lev = worm->m_lev;

    /* Calculate the mhp on the new_worm for the (lower) monster level. */
    new_worm->mhpmax = new_worm->mhp = dice((int)new_worm->m_lev, 8);

    /* Calculate the mhp on the old worm for the (lower) monster level. */
    if (worm->m_lev > 3) {
        worm->mhpmax = dice((int)worm->m_lev, 8);
        if (worm->mhpmax < worm->mhp)
            worm->mhp = worm->mhpmax;
    }

    level->wtails[new_wnum] = new_tail; /* We've got all the info right now */
    level->wheads[new_wnum] = curr;     /* so we can do this faster than */
    level->wgrowtime[new_wnum] = 0L;    /* trying to call initworm().  */

    /* Place the new monster at all the segment locations. */
    place_wsegs(new_worm);

    if (flags.mon_moving)
        pline("%s is cut in half.", Monnam(worm));
    else
        pline("You cut %s in half.", mon_nam(worm));
}


/*
 *  see_wsegs()
 *
 *  Refresh all of the segments of the given worm.  This is only called
 *  from see_monster() in display.c or when a monster goes minvis.  It
 *  is located here for modularity.
 */
void
see_wsegs(struct monst *worm)
{
    struct wseg *curr = level->wtails[worm->wormno];

/*  if (!mtmp->wormno) return;  bullet proofing */

    while (curr != level->wheads[worm->wormno]) {
        newsym(curr->wx, curr->wy);
        curr = curr->nseg;
    }
}

/*
 *  detect_wsegs()
 *
 *  Display all of the segments of the given worm for detection.
 */
void
detect_wsegs(struct monst *worm, boolean use_detection_glyph)
{
    int dflag;
    struct wseg *curr = level->wtails[worm->wormno];

/*  if (!mtmp->wormno) return;  bullet proofing */

    while (curr != level->wheads[worm->wormno]) {
        dflag = use_detection_glyph ? MON_DETECTED : 0;
        dbuf_set(curr->wx, curr->wy, S_unexplored, 0, 0, 0, 0,
                 PM_LONG_WORM_TAIL + 1, dflag, 0, 0);
        curr = curr->nseg;
    }
}


/*
 *  save_worm()
 *
 *  Save the worm information for later use.  The count is the number
 *  of segments, including the dummy.  Called from save.c.
 */
void
save_worm(struct memfile *mf, struct level *lev)
{
    int i, count;
    struct wseg *curr;

    for (i = 1; i < MAX_NUM_WORMS; i++) {
        for (count = 0, curr = lev->wtails[i]; curr; curr = curr->nseg)
            count++;
        mtag(mf, (int)ledger_no(&lev->z) * MAX_NUM_WORMS + i, MTAG_WORMS);
        /* Save number of segments */
        mwrite32(mf, count);
        /* Save segment locations of the monster. */
        if (count) {
            for (curr = lev->wtails[i]; curr; curr = curr->nseg) {
                mwrite8(mf, curr->wx);
                mwrite8(mf, curr->wy);
            }
            mwrite32(mf, lev->wgrowtime[i]);
        }
    }
}


void
free_worm(struct level *lev)
{
    int i;
    struct wseg *curr, *temp;

    /* Free the segments only.  free_monchn() will take care of the monsters. */
    for (i = 1; i < MAX_NUM_WORMS; i++) {
        if (!(curr = lev->wtails[i]))
            continue;

        while (curr) {
            temp = curr->nseg;
            free(curr); /* free the segment */
            curr = temp;
        }
        lev->wheads[i] = lev->wtails[i] = NULL;
    }
}

/*
 *  rest_worm()
 *
 *  Restore the worm information from the save file.  Called from restore.c
 */
void
rest_worm(struct memfile *mf, struct level *lev)
{
    int i, j, count;
    struct wseg *curr, *temp;

    for (i = 1; i < MAX_NUM_WORMS; i++) {
        count = mread32(mf);
        if (!count)
            continue;   /* none */

        /* Get the segments. */
        for (curr = NULL, j = 0; j < count; j++) {
            temp = malloc(sizeof (struct wseg));
            temp->nseg = NULL;
            temp->wx = mread8(mf);
            temp->wy = mread8(mf);
            if (curr)
                curr->nseg = temp;
            else
                lev->wtails[i] = temp;
            curr = temp;
        }
        lev->wgrowtime[i] = mread32(mf);
        lev->wheads[i] = curr;
    }
}

/*
 *  place_wsegs()
 *
 *  Place the segments of the given worm.  Called from restore.c
 */
void
place_wsegs(struct monst *worm)
{
    struct wseg *curr = worm->dlevel->wtails[worm->wormno];

/*  if (!mtmp->wormno) return;  bullet proofing */

    while (curr != worm->dlevel->wheads[worm->wormno]) {
        place_worm_seg(worm, curr->wx, curr->wy);
        curr = curr->nseg;
    }
}

/*
 *  remove_worm()
 *
 *  This function is equivalent to the remove_monster #define in
 *  rm.h, only it will take the worm *and* tail out of the levels array.
 *  It does not get rid of (dealloc) the worm tail structures, and it does
 *  not remove the mon from the level->monlist chain.
 */
void
remove_worm(struct monst *worm)
{
    struct wseg *curr = level->wtails[worm->wormno];

/*  if (!mtmp->wormno) return;  bullet proofing */

    while (curr) {
        remove_monster(level, curr->wx, curr->wy);
        newsym(curr->wx, curr->wy);
        curr = curr->nseg;
    }
}

/*
 *  place_worm_tail_randomly()
 *
 *  Place a worm tail somewhere on a level behind the head.
 *  This routine essentially reverses the order of the wsegs from head
 *  to tail while placing them.
 *  x, and y are most likely the worm->mx, and worm->my, but don't *need* to
 *  be, if somehow the head is disjoint from the tail.
 */
void
place_worm_tail_randomly(struct monst *worm, xchar x, xchar y, enum rng rng)
{
    int wnum = worm->wormno;
    struct level *lev = worm->dlevel;
    struct wseg *curr = lev->wtails[wnum];
    struct wseg *new_tail;
    xchar ox = x, oy = y;

/*  if (!wnum) return;  bullet proofing */

    if (wnum && (!lev->wtails[wnum] || !lev->wheads[wnum])) {
        impossible("place_worm_tail_randomly: wormno is set without a tail!");
        return;
    }

    lev->wheads[wnum] = new_tail = curr;
    curr = curr->nseg;
    new_tail->nseg = NULL;
    new_tail->wx = x;
    new_tail->wy = y;

    while (curr) {
        xchar nx, ny;
        char tryct = 0;

        /* pick a random direction from x, y and search for goodpos() */

        do {
            random_dir(ox, oy, &nx, &ny, rng);
        } while (!goodpos(lev, nx, ny, worm, 0) && (tryct++ < 50));

        if (tryct < 50) {
            place_worm_seg(worm, nx, ny);
            curr->wx = ox = nx;
            curr->wy = oy = ny;
            lev->wtails[wnum] = curr;
            curr = curr->nseg;
            lev->wtails[wnum]->nseg = new_tail;
            new_tail = lev->wtails[wnum];
            if (lev == level)
                newsym(nx, ny);
        } else {        /* Oops.  Truncate because there was */
            toss_wsegs(lev, curr, FALSE);      /* no place for the rest of it */
            curr = NULL;
        }
    }
}

/*
 * Given a coordinate x, y.
 * return in *nx, *ny, the coordinates of one of the <= 8 squares adjoining.
 *
 * This function, and the loop it serves, could be eliminated by coding
 * enexto() with a search radius.
 */
static void
random_dir(xchar x, xchar y, xchar * nx, xchar * ny, enum rng rng)
{
    *nx = x;
    *ny = y;

    *nx += (x > 0 ?                     /* extreme left ? */
            (x < COLNO - 1 ?            /* extreme right ? */
             (rn2_on_rng(3, rng) - 1)   /* neither so +1, 0, or -1 */
             : -rn2_on_rng(2, rng))     /* 0, or -1 */
            : rn2_on_rng(2, rng));      /* 0, or 1 */

    *ny += (*nx == x ?  /* same kind of thing with y */
            (y > 0 ? (y < ROWNO - 1 ? (rn2_on_rng(2, rng) ? 1 : -1)
                      : -1)
             : 1)
            : (y > 0 ? (y < ROWNO - 1 ? (rn2_on_rng(3, rng) - 1)
                        : -rn2_on_rng(2, rng))
               : rn2_on_rng(2, rng)));
}

/*  count_wsegs()
 *
 *  returns
 *  the number of visible segments that a worm has.
 */

int
count_wsegs(struct monst *mtmp)
{
    int i = 0;
    struct wseg *curr = (mtmp->dlevel->wtails[mtmp->wormno])->nseg;

/*  if (!mtmp->wormno) return 0;  bullet proofing */

    while (curr) {
        i++;
        curr = curr->nseg;
    }

    return i;
}

/*  create_worm_tail()
 *
 *  will create a worm tail chain of (num_segs + 1) and return a pointer to it.
 */
static struct wseg *
create_worm_tail(int num_segs)
{
    int i = 0;
    struct wseg *new_tail, *curr;

    if (!num_segs)
        return NULL;

    new_tail = curr = malloc(sizeof (struct wseg));
    curr->nseg = NULL;
    curr->wx = 0;
    curr->wy = 0;

    while (i < num_segs) {
        curr->nseg = malloc(sizeof (struct wseg));
        curr = curr->nseg;
        curr->nseg = NULL;
        curr->wx = 0;
        curr->wy = 0;
        i++;
    }

    return new_tail;
}

/*worm.c*/

