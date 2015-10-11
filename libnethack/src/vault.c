/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-10-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "vault.h"

static struct monst *findgd(void);

static boolean clear_fcorr(struct monst *, boolean);
static void restfakecorr(struct monst *);
static boolean in_fcorridor(struct monst *, int, int);
static void move_gold(struct obj *, int);
static void wallify_vault(struct monst *);
static boolean find_guard_dest(struct monst *, xchar *, xchar *);

static boolean
clear_fcorr(struct monst *grd, boolean forceshow)
{
    int fcx, fcy, fcbeg, oldtyp;
    struct monst *mtmp;
    boolean showmsg = FALSE;

    if (!on_level(&(EGD(grd)->gdlevel), &u.uz))
        return TRUE;

    while ((fcbeg = EGD(grd)->fcbeg) < EGD(grd)->fcend) {
        fcx = EGD(grd)->fakecorr[fcbeg].fx;
        fcy = EGD(grd)->fakecorr[fcbeg].fy;
        if ((DEADMONSTER(grd) || !in_fcorridor(grd, u.ux, u.uy)) &&
            EGD(grd)->gddone)
            forceshow = TRUE;
        if ((u.ux == fcx && u.uy == fcy && !DEADMONSTER(grd))
            || (!forceshow && couldsee(fcx, fcy)))
            return FALSE;

        if ((Punished && !carried(uball)
             && uball->ox == fcx && uball->oy == fcy)) {
            unplacebc();
            placebc();
        }

        if ((mtmp = m_at(level, fcx, fcy)) != 0) {
            if (mtmp->isgd)
                return FALSE;
            else if (!in_fcorridor(grd, u.ux, u.uy)) {
                if (mtmp->mtame)
                    yelp(mtmp);
                rloc(mtmp, FALSE);
            }
        }
        oldtyp = level->locations[fcx][fcy].typ;
        level->locations[fcx][fcy].typ = EGD(grd)->fakecorr[fcbeg].ftyp;
        if (!ACCESSIBLE(level->locations[fcx][fcy].typ) && ACCESSIBLE(oldtyp)) {
            struct trap *t = t_at(level, fcx, fcy);

            if (couldsee(fcx, fcy))
                showmsg = TRUE;
            if (t)
                deltrap(level, t);
            level->locations[fcx][fcy].lit = FALSE;
            block_point(fcx, fcy);
        }
        map_location(fcx, fcy, 1, FALSE);      /* bypass vision */
        EGD(grd)->fcbeg++;
    }
    if (DEADMONSTER(grd)) {
        if (showmsg) {
            if (!Blind)
                pline("The corridor disappears.");
            else
                pline("You feel claustrophobic.");
        }
        if (IS_ROCK(level->locations[u.ux][u.uy].typ))
            pline("You are encased in rock.");
    }
    return TRUE;
}

static void
restfakecorr(struct monst *grd)
{
    /* it seems you left the corridor - let the guard disappear */
    if (clear_fcorr(grd, FALSE))
        mongone(grd);
}

/* called in mon.c */
boolean
grddead(struct monst *grd)
{
    boolean dispose = clear_fcorr(grd, TRUE);

    if (!dispose) {
        /* see comment by newpos in gd_move() */
        remove_monster(level, grd->mx, grd->my);
        newsym(grd->mx, grd->my);
        grd->mx = COLNO;
        grd->my = ROWNO;
        EGD(grd)->ogx = grd->mx;
        EGD(grd)->ogy = grd->my;
        dispose = clear_fcorr(grd, TRUE);
    }
    return dispose;
}

static boolean
in_fcorridor(struct monst *grd, int x, int y)
{
    int fci;

    for (fci = EGD(grd)->fcbeg; fci < EGD(grd)->fcend; fci++)
        if (x == EGD(grd)->fakecorr[fci].fx && y == EGD(grd)->fakecorr[fci].fy)
            return TRUE;
    return FALSE;
}

static struct monst *
findgd(void)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (mtmp->isgd && !DEADMONSTER(mtmp) &&
            on_level(&(EGD(mtmp)->gdlevel), &u.uz))
            return mtmp;
    return NULL;
}


char
vault_occupied(char *array)
{
    char *ptr;

    for (ptr = array; *ptr; ptr++)
        if (level->rooms[*ptr - ROOMOFFSET].rtype == VAULT)
            return *ptr;
    return '\0';
}

static boolean
find_guard_dest(struct monst *guard, xchar * rx, xchar * ry)
{
    int x, y, dd, lx = 0, ly = 0;

    for (dd = 2; (dd < ROWNO || dd < COLNO); dd++) {
        for (y = u.uy - dd; y <= u.uy + dd; ly = y, y++) {
            if (y < 0 || y > ROWNO - 1)
                continue;
            for (x = u.ux - dd; x <= u.ux + dd; lx = x, x++) {
                if (y != u.uy - dd && y != u.uy + dd && x != u.ux - dd)
                    x = u.ux + dd;
                if (x < 0 || x > COLNO - 1)
                    continue;
                if (guard &&
                    ((x == guard->mx && y == guard->my) ||
                     (guard->isgd && in_fcorridor(guard, x, y))))
                    continue;
                if (level->locations[x][y].typ == CORR) {
                    if (x < u.ux)
                        lx = x + 1;
                    else if (x > u.ux)
                        lx = x - 1;
                    else
                        lx = x;
                    if (y < u.uy)
                        ly = y + 1;
                    else if (y > u.uy)
                        ly = y - 1;
                    else
                        ly = y;
                    if (level->locations[lx][ly].typ != STONE &&
                        level->locations[lx][ly].typ != CORR)
                        goto incr_radius;
                    *rx = x;
                    *ry = y;
                    return TRUE;
                }
            }
        }
    incr_radius:;
    }
    impossible("Not a single corridor on this level??");
    tele();
    return FALSE;
}

void
invault(void)
{
    struct monst *guard;
    int trycount, vaultroom = (int)vault_occupied(u.urooms);
    boolean messages = TRUE;

    if (!vaultroom) {
        u.uinvault = 0;
        return;
    }

    vaultroom -= ROOMOFFSET;

    guard = findgd();
    if (++u.uinvault % 30 == 0 && !guard) {  /* if time ok and no guard now. */
        int x, y, gx, gy;
        xchar rx, ry;
        long umoney;
        const char *buf;

        /* first find the goal for the guard */
        if (!find_guard_dest(NULL, &rx, &ry))
            return;
        gx = rx;
        gy = ry;

        /* next find a good place for a door in the wall */
        x = u.ux;
        y = u.uy;
        if (level->locations[x][y].typ != ROOM) {       /* player dug a door
                                                           and is in it */
            if (level->locations[x + 1][y].typ == ROOM)
                x = x + 1;
            else if (level->locations[x][y + 1].typ == ROOM)
                y = y + 1;
            else if (level->locations[x - 1][y].typ == ROOM)
                x = x - 1;
            else if (level->locations[x][y - 1].typ == ROOM)
                y = y - 1;
            else if (level->locations[x + 1][y + 1].typ == ROOM) {
                x = x + 1;
                y = y + 1;
            } else if (level->locations[x - 1][y - 1].typ == ROOM) {
                x = x - 1;
                y = y - 1;
            } else if (level->locations[x + 1][y - 1].typ == ROOM) {
                x = x + 1;
                y = y - 1;
            } else if (level->locations[x - 1][y + 1].typ == ROOM) {
                x = x - 1;
                y = y + 1;
            }
        }
        while (level->locations[x][y].typ == ROOM) {
            int dx, dy;

            dx = (gx > x) ? 1 : (gx < x) ? -1 : 0;
            dy = (gy > y) ? 1 : (gy < y) ? -1 : 0;
            if (abs(gx - x) >= abs(gy - y))
                x += dx;
            else
                y += dy;
        }
        if (x == u.ux && y == u.uy) {
            if (level->locations[x + 1][y].typ == HWALL ||
                level->locations[x + 1][y].typ == DOOR)
                x = x + 1;
            else if (level->locations[x - 1][y].typ == HWALL ||
                     level->locations[x - 1][y].typ == DOOR)
                x = x - 1;
            else if (level->locations[x][y + 1].typ == VWALL ||
                     level->locations[x][y + 1].typ == DOOR)
                y = y + 1;
            else if (level->locations[x][y - 1].typ == VWALL ||
                     level->locations[x][y - 1].typ == DOOR)
                y = y - 1;
            else
                return;
        }

        /* make something interesting happen */
        if (!(guard = makemon(&mons[PM_GUARD], level, x, y, NO_MM_FLAGS)))
            return;
        guard->isgd = 1;
        msethostility(guard, FALSE, TRUE);
        EGD(guard)->gddone = 0;
        EGD(guard)->ogx = x;
        EGD(guard)->ogy = y;
        assign_level(&(EGD(guard)->gdlevel), &u.uz);
        EGD(guard)->vroom = vaultroom;
        EGD(guard)->warncnt = 0;

        /* We used to reset fainted status here, but that doesn't really make
           sense; instead, that's treated like normal helplessness */
        if (canspotmon(guard))
            pline("Suddenly one of the Vault's guards enters!");
        else if (canhear())
            You_hear("someone else enter the Vault.");
        else
            messages = FALSE;

        newsym(guard->mx, guard->my);
        if (youmonst.m_ap_type == M_AP_OBJECT || u.uundetected || u.uburied) {
            if (youmonst.m_ap_type == M_AP_OBJECT &&
                youmonst.mappearance != GOLD_PIECE)
                verbalize("Hey! Who left that %s in here?",
                          mimic_obj_name(&youmonst));
            /* You're mimicking some object or you're hidden. */
            if (messages)
                pline("Puzzled, %s turns around and leaves.", mhe(guard));
            mongone(guard);
            return;
        }
        if (Engulfed) {
            if ((!u.ustuck->minvis || perceives(guard->data)))
                verbalize("How did that %s get in here?", m_monnam(u.ustuck));
            if (messages)
                pline("Puzzled, %s turns around and leaves.", mhe(guard));
            mongone(guard);
            return;
        }
        if (Strangled || is_silent(youmonst.data) || u_helpless(hm_all)) {
            /* [we ought to record whether this this message has already been
               given in order to vary it upon repeat visits, but discarding the 
               monster and its egd data renders that hard] */
            verbalize("I'll be back when you're ready to speak to me!");
            mongone(guard);
            return;
        }

        action_interrupted();

        trycount = 5;
        do {
            buf = getlin("\"Hello stranger, who are you?\" -", FALSE);
            buf = msgmungspaces(buf);
        } while (!letter(buf[0]) && --trycount > 0);

        if (u.ualign.type == A_LAWFUL &&
            /* ignore trailing text, in case player includes character's rank */
            strncmpi(buf, u.uplname, (int)strlen(u.uplname)) != 0) {
            adjalign(-1);       /* Liar! */
        }

        if (!strcmpi(buf, "Croesus") || !strcmpi(buf, "Kroisos") ||
            !strcmpi(buf, "Creosote")) {
            if (!mvitals[PM_CROESUS].died) {
                verbalize("Oh, yes, of course.  Sorry to have disturbed you.");
                mongone(guard);
            } else {
                setmangry(guard);
                verbalize("Back from the dead, are you?  I'll remedy that!");
                /* don't want guard to waste next turn wielding a weapon */
                if (!MON_WEP(guard)) {
                    guard->weapon_check = NEED_HTH_WEAPON;
                    mon_wield_item(guard);
                }
            }
            return;
        }
        verbalize("I don't know you.");
        umoney = money_cnt(invent);
        if (!umoney && !hidden_gold())
            verbalize("Please follow me.");
        else {
            if (!umoney)
                verbalize("You have hidden money.");
            verbalize("Most likely all your money was stolen from this vault.");
            verbalize("Please drop that money and follow me.");
        }

        EGD(guard)->gdx = gx;
        EGD(guard)->gdy = gy;
        EGD(guard)->fcbeg = 0;
        EGD(guard)->fakecorr[0].fx = x;
        EGD(guard)->fakecorr[0].fy = y;
        if (IS_WALL(level->locations[x][y].typ))
            EGD(guard)->fakecorr[0].ftyp = level->locations[x][y].typ;
        else {  /* the initial guard location is a dug door */
            int vlt = EGD(guard)->vroom;
            xchar lowx = level->rooms[vlt].lx, hix = level->rooms[vlt].hx;
            xchar lowy = level->rooms[vlt].ly, hiy = level->rooms[vlt].hy;

            if (x == lowx - 1 && y == lowy - 1)
                EGD(guard)->fakecorr[0].ftyp = TLCORNER;
            else if (x == hix + 1 && y == lowy - 1)
                EGD(guard)->fakecorr[0].ftyp = TRCORNER;
            else if (x == lowx - 1 && y == hiy + 1)
                EGD(guard)->fakecorr[0].ftyp = BLCORNER;
            else if (x == hix + 1 && y == hiy + 1)
                EGD(guard)->fakecorr[0].ftyp = BRCORNER;
            else if (y == lowy - 1 || y == hiy + 1)
                EGD(guard)->fakecorr[0].ftyp = HWALL;
            else if (x == lowx - 1 || x == hix + 1)
                EGD(guard)->fakecorr[0].ftyp = VWALL;
        }
        level->locations[x][y].typ = DOOR;
        level->locations[x][y].doormask = D_NODOOR;
        unblock_point(x, y);    /* doesn't block light */
        EGD(guard)->fcend = 1;
        EGD(guard)->warncnt = 1;
    }
}


static void
move_gold(struct obj *gold, int vroom)
{
    xchar nx, ny;

    remove_object(gold);
    newsym(gold->ox, gold->oy);
    nx = level->rooms[vroom].lx + rn2(2);
    ny = level->rooms[vroom].ly + rn2(2);
    place_object(gold, level, nx, ny);
    stackobj(gold);
    newsym(nx, ny);
}

static void
wallify_vault(struct monst *grd)
{
    int x, y, typ;
    int vlt = EGD(grd)->vroom;
    char tmp_viz;
    xchar lox = level->rooms[vlt].lx - 1, hix = level->rooms[vlt].hx + 1, loy =
        level->rooms[vlt].ly - 1, hiy = level->rooms[vlt].hy + 1;
    struct monst *mon;
    struct obj *gold;
    struct trap *trap;
    boolean fixed = FALSE;
    boolean movedgold = FALSE;

    for (x = lox; x <= hix; x++)
        for (y = loy; y <= hiy; y++) {
            /* if not on the room boundary, skip ahead */
            if (x != lox && x != hix && y != loy && y != hiy)
                continue;

            if (!IS_WALL(level->locations[x][y].typ) &&
                !in_fcorridor(grd, x, y)) {
                if ((mon = m_at(level, x, y)) != 0 && mon != grd) {
                    if (mon->mtame)
                        yelp(mon);
                    rloc(mon, FALSE);
                }
                if ((gold = gold_at(level, x, y)) != 0) {
                    move_gold(gold, EGD(grd)->vroom);
                    movedgold = TRUE;
                }
                if ((trap = t_at(level, x, y)) != 0)
                    deltrap(level, trap);
                if (x == lox)
                    typ = (y == loy) ? TLCORNER : (y == hiy) ? BLCORNER : VWALL;
                else if (x == hix)
                    typ = (y == loy) ? TRCORNER : (y == hiy) ? BRCORNER : VWALL;
                else    /* not left or right side, must be top or bottom */
                    typ = HWALL;
                level->locations[x][y].typ = typ;
                level->locations[x][y].doormask = 0;
                /* 
                 * hack: player knows walls are restored because of the
                 * message, below, so show this on the screen.
                 */
                tmp_viz = viz_array[y][x];
                viz_array[y][x] = IN_SIGHT | COULD_SEE;
                newsym(x, y);
                viz_array[y][x] = tmp_viz;
                block_point(x, y);
                fixed = TRUE;
            }
        }

    if (movedgold || fixed) {
        if (mon_visible(grd))
            pline("%s whispers an incantation.", Monnam(grd));
        else
            You_hear("a %s chant.", in_fcorridor(grd, grd->mx, grd->my)
                     ? "nearby" : "distant");
        if (movedgold)
            pline("A mysterious force moves the gold into the vault.");
        if (fixed)
            pline("The damaged vault's walls are magically restored!");
    }
}

/*
 * return  1: guard moved,  0: guard didn't,  -1: let m_move do it,  -2: died
 */
int
gd_move(struct monst *grd)
{
    int x, y, nx, ny, m, n;
    int dx, dy, gx = 0, gy = 0, fci;
    uchar typ;
    struct fakecorridor *fcp;
    struct egd *egrd = EGD(grd);
    struct rm *crm;
    boolean goldincorridor = FALSE, u_in_vault =
        vault_occupied(u.urooms) ? TRUE : FALSE, grd_in_vault =
        *in_rooms(level, grd->mx, grd->my, VAULT) ? TRUE : FALSE;
    boolean disappear_msg_seen = FALSE, semi_dead = DEADMONSTER(grd);
    long umoney = money_cnt(invent);
    boolean u_carry_gold = ((umoney + hidden_gold()) > 0L);
    boolean see_guard;

    if (!on_level(&(egrd->gdlevel), &u.uz))
        return -1;
    nx = ny = m = n = 0;
    if (!u_in_vault && !grd_in_vault)
        wallify_vault(grd);
    if (!grd->mpeaceful) {
        if (semi_dead) {
            egrd->gddone = 1;
            goto newpos;
        }
        if (!u_in_vault &&
            (grd_in_vault ||
             (in_fcorridor(grd, grd->mx, grd->my) &&
              !in_fcorridor(grd, u.ux, u.uy)))) {
            rloc(grd, FALSE);
            wallify_vault(grd);
            clear_fcorr(grd, TRUE);
            goto letknow;
        }
        if (!in_fcorridor(grd, grd->mx, grd->my))
            clear_fcorr(grd, TRUE);
        return -1;
    }
    if (abs(egrd->ogx - grd->mx) > 1 || abs(egrd->ogy - grd->my) > 1)
        return -1;      /* teleported guard - treat as monster */
    if (egrd->fcend == 1) {
        if (u_in_vault && (u_carry_gold || um_dist(grd->mx, grd->my, 1))) {
            if (egrd->warncnt == 3)
                verbalize("I repeat, %sfollow me!",
                          u_carry_gold ? (!umoney ?
                                          "drop that hidden money and " :
                                          "drop that money and ") : "");
            if (egrd->warncnt == 7) {
                m = grd->mx;
                n = grd->my;
                verbalize("You've been warned, knave!");
                mnexto(grd);
                level->locations[m][n].typ = egrd->fakecorr[0].ftyp;
                newsym(m, n);
                msethostility(grd, TRUE, FALSE);
                return -1;
            }
            /* not fair to get mad when (s)he's fainted or paralyzed */
            if (!u_helpless(hm_all))
                egrd->warncnt++;
            return 0;
        }

        if (!u_in_vault) {
            if (u_carry_gold) { /* player teleported */
                m = grd->mx;
                n = grd->my;
                rloc(grd, FALSE);
                level->locations[m][n].typ = egrd->fakecorr[0].ftyp;
                newsym(m, n);
                msethostility(grd, TRUE, FALSE);
            letknow:
                if (!cansee(grd->mx, grd->my) || !mon_visible(grd))
                    You_hear("the shrill sound of a guard's whistle.");
                else
                    pline(um_dist(grd->mx, grd->my, 2) ?
                          "You see an angry guard approaching." :
                          "You are confronted by an angry guard.");
                return -1;
            } else {
                verbalize("Well, begone.");
                wallify_vault(grd);
                egrd->gddone = 1;
                goto cleanup;
            }
        }
    }

    if (egrd->fcend > 1) {
        if (egrd->fcend > 2 && in_fcorridor(grd, grd->mx, grd->my) &&
            !egrd->gddone && !in_fcorridor(grd, u.ux, u.uy) &&
            level->locations[egrd->fakecorr[0].fx][egrd->fakecorr[0].fy].typ ==
            egrd->fakecorr[0].ftyp) {
            if (canseemon(grd)) {
                pline("%s, confused, disappears.", Monnam(grd));
                disappear_msg_seen = TRUE;
            }
            goto cleanup;
        }
        if (u_carry_gold && (in_fcorridor(grd, u.ux, u.uy) ||
                             /* cover a 'blind' spot */
                             (egrd->fcend > 1 && u_in_vault))) {
            if (!grd->mx) {
                restfakecorr(grd);
                return -2;
            }
            if (egrd->warncnt < 6) {
                egrd->warncnt = 6;
                verbalize("Drop all your gold, scoundrel!");
                return 0;
            } else {
                verbalize("So be it, rogue!");
                msethostility(grd, TRUE, FALSE);
                return -1;
            }
        }
    }
    for (fci = egrd->fcbeg; fci < egrd->fcend; fci++)
        if (gold_at(level, egrd->fakecorr[fci].fx, egrd->fakecorr[fci].fy)) {
            m = egrd->fakecorr[fci].fx;
            n = egrd->fakecorr[fci].fy;
            goldincorridor = TRUE;
        }
    if (goldincorridor && !egrd->gddone) {
        boolean yours = FALSE;

        x = grd->mx;
        y = grd->my;
        if (m == u.ux && n == u.uy) {
            struct obj *gold = gold_at(level, m, n);

            yours = TRUE;
            /* Grab the gold from between the hero's feet.  */
            obj_extract_self(gold);
            add_to_minv(grd, gold);
            newsym(m, n);
        } else if (m == x && n == y) {
            mpickgold(grd);     /* does a newsym */
        } else {
            /* just for insurance... */
            if (MON_AT(level, m, n) && m != grd->mx && n != grd->my) {
                verbalize("Out of my way, scum!");
                rloc(m_at(level, m, n), FALSE);
            }
            remove_monster(level, grd->mx, grd->my);
            newsym(grd->mx, grd->my);
            place_monster(grd, m, n);
            mpickgold(grd);     /* does a newsym */
        }
        if (cansee(m, n)) {
            if (yours) {
                pline("%s%s picks up the gold.", Monnam(grd),
                      grd->mpeaceful ? " calms down and" : "");
            } else {
                pline("%s picks up some gold.", Monnam(grd));
            }
        }
        if (x != grd->mx || y != grd->my) {
            remove_monster(level, grd->mx, grd->my);
            newsym(grd->mx, grd->my);
            place_monster(grd, x, y);
            newsym(x, y);
        }
        if (!grd->mpeaceful)
            return -1;
        else {
            egrd->warncnt = 5;
            return 0;
        }
    }
    if (um_dist(grd->mx, grd->my, 1) || egrd->gddone) {
        if (!egrd->gddone && !rn2(10))
            verbalize("Move along!");
        restfakecorr(grd);
        return 0;       /* didn't move */
    }
    x = grd->mx;
    y = grd->my;

    if (u_in_vault)
        goto nextpos;

    /* look around (hor & vert only) for accessible places */
    for (nx = x - 1; nx <= x + 1; nx++)
        for (ny = y - 1; ny <= y + 1; ny++) {
            if ((nx == x || ny == y) && (nx != x || ny != y) && isok(nx, ny)) {

                typ = (crm = &level->locations[nx][ny])->typ;
                if (!IS_STWALL(typ) && !IS_POOL(typ)) {

                    if (in_fcorridor(grd, nx, ny))
                        goto nextnxy;

                    if (*in_rooms(level, nx, ny, VAULT))
                        continue;

                    /* seems we found a good place to leave him alone */
                    egrd->gddone = 1;
                    if (ACCESSIBLE(typ))
                        goto newpos;
                    crm->typ = (typ == SCORR) ? CORR : DOOR;
                    if (crm->typ == DOOR)
                        crm->doormask = D_NODOOR;
                    goto proceed;
                }
            }
        nextnxy:;
        }
nextpos:
    nx = x;
    ny = y;
    gx = egrd->gdx;
    gy = egrd->gdy;
    dx = (gx > x) ? 1 : (gx < x) ? -1 : 0;
    dy = (gy > y) ? 1 : (gy < y) ? -1 : 0;
    if (abs(gx - x) >= abs(gy - y))
        nx += dx;
    else
        ny += dy;

    while ((typ = (crm = &level->locations[nx][ny])->typ) != 0) {
        /* in view of the above we must have IS_WALL(typ) or typ == POOL */
        /* must be a wall here */
        if (isok(nx + nx - x, ny + ny - y) && !IS_POOL(typ) &&
            IS_ROOM(level->locations[nx + nx - x][ny + ny - y].typ)) {
            crm->typ = DOOR;
            crm->doormask = D_NODOOR;
            goto proceed;
        }
        if (dy && nx != x) {
            nx = x;
            ny = y + dy;
            continue;
        }
        if (dx && ny != y) {
            ny = y;
            nx = x + dx;
            dy = 0;
            continue;
        }
        /* I don't like this, but ... */
        if (IS_ROOM(typ)) {
            crm->typ = DOOR;
            crm->doormask = D_NODOOR;
            goto proceed;
        }
        break;
    }
    crm->typ = CORR;
proceed:
    unblock_point(nx, ny);      /* doesn't block light */
    if (cansee(nx, ny))
        newsym(nx, ny);

    if ((nx != gx || ny != gy) || (grd->mx != gx || grd->my != gy)) {
        fcp = &(egrd->fakecorr[egrd->fcend]);
        if (egrd->fcend++ == FCSIZ)
            panic("fakecorr overflow");
        fcp->fx = nx;
        fcp->fy = ny;
        fcp->ftyp = typ;
    } else if (!egrd->gddone) {
        /* We're stuck, so try to find a new destination. */
        if (!find_guard_dest(grd, &egrd->gdx, &egrd->gdy) ||
            (egrd->gdx == gx && egrd->gdy == gy)) {
            pline("%s, confused, disappears.", Monnam(grd));
            disappear_msg_seen = TRUE;
            goto cleanup;
        } else
            goto nextpos;
    }
newpos:
    if (egrd->gddone) {
        /* The following is a kludge.  We need to keep the guard around in order
           to be able to make the fake corridor disappear as the player moves
           out of it, but we also need the guard out of the way.  We send the
           guard to never-never land.  We set ogx ogy to mx my in order to avoid
           a check at the top of this function.  At the end of the process, the
           guard is killed in restfakecorr().  */
    cleanup:
        x = grd->mx;
        y = grd->my;

        see_guard = canspotmon(grd);
        wallify_vault(grd);
        remove_monster(level, grd->mx, grd->my);
        newsym(grd->mx, grd->my);
        grd->mx = COLNO;
        grd->my = ROWNO;
        egrd->ogx = grd->mx;
        egrd->ogy = grd->my;
        restfakecorr(grd);
        if (!semi_dead && (in_fcorridor(grd, u.ux, u.uy) || cansee(x, y))) {
            if (!disappear_msg_seen && see_guard)
                pline("Suddenly, %s disappears.", noit_mon_nam(grd));
            return 1;
        }
        return -2;
    }
    egrd->ogx = grd->mx;        /* update old positions */
    egrd->ogy = grd->my;
    remove_monster(level, grd->mx, grd->my);
    place_monster(grd, nx, ny);
    newsym(grd->mx, grd->my);
    restfakecorr(grd);
    return 1;
}

/* Routine when dying or quitting with a vault guard around */
void
paygd(void)
{
    struct monst *grd = findgd();
    long umoney = money_cnt(invent);
    struct obj *coins, *nextcoins;
    int gx, gy;

    if (!umoney || !grd)
        return;

    if (u.uinvault) {
        pline("Your %ld %s goes into the Magic Memory Vault.", umoney,
              currency(umoney));
        gx = u.ux;
        gy = u.uy;
    } else {
        if (grd->mpeaceful) {   /* guard has no "right" to your gold */
            mongone(grd);
            return;
        }
        mnexto(grd);
        pline("%s remits your gold to the vault.", Monnam(grd));
        gx = level->rooms[EGD(grd)->vroom].lx + rn2(2);
        gy = level->rooms[EGD(grd)->vroom].ly + rn2(2);
        make_grave(level, gx, gy,
                   msgprintf(
                       "To Croesus: here's the gold recovered from %s the %s.",
                       u.uplname, mons[u.umonster].mname));
    }
    
    for (coins = invent; coins; coins = nextcoins) {
        nextcoins = coins->nobj;
        if (objects[coins->otyp].oc_class == COIN_CLASS) {
            unwield_silently(coins);
            freeinv(coins);
            place_object(coins, level, gx, gy);
            stackobj(coins);
        }
    }
    mongone(grd);
}

long
hidden_gold(void)
{
    long value = 0L;
    struct obj *obj;

    for (obj = invent; obj; obj = obj->nobj)
        if (Has_contents(obj))
            value += contained_gold(obj);
    /* unknown gold stuck inside statues may cause some consternation... */

    return value;
}

/* prevent "You hear footsteps.." when inappropriate */
boolean
gd_sound(void)
{
    struct monst *grd = findgd();

    if (vault_occupied(u.urooms))
        return FALSE;
    else
        return (boolean) (grd == NULL);
}

/*vault.c*/

