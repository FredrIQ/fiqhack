/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-04-02 */
/* Copyright (c) Izchak Miller, Steve Linhart, 1989.              */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"
#include "eshk.h"
#include "epri.h"
#include "emin.h"

/* this matches the categorizations shown by enlightenment */
#define ALGN_SINNED     (-4)    /* worse than strayed */

static boolean histemple_at(struct monst *, xchar, xchar);
static xchar has_shrine(const struct monst *);

/*
 * Move for priests and shopkeepers.  Called from shk_move() and pri_move().
 * Valid returns are  1: moved  0: didn't  -1: let m_move do it  -2: died.
 */
int
move_special(struct monst *mtmp, boolean in_his_shop, schar appr,
             boolean uondoor, boolean avoid, xchar omx, xchar omy, xchar gx,
             xchar gy)
{
    xchar nx, ny, nix, niy;
    schar i;
    schar chcnt, cnt;
    coord poss[9];
    long info[9];
    long allowflags;
    struct obj *ib = NULL;

    if (omx == gx && omy == gy)
        return 0;
    if (mtmp->mconf) {
        avoid = FALSE;
        appr = 0;
    }

    nix = omx;
    niy = omy;
    if (mtmp->isshk)
        allowflags = ALLOW_SSM;
    else
        allowflags = ALLOW_SSM | ALLOW_SANCT;
    if (passes_walls(mtmp->data))
        allowflags |= (ALLOW_ROCK | ALLOW_WALL);
    if (throws_rocks(mtmp->data))
        allowflags |= ALLOW_ROCK;
    if (tunnels(mtmp->data))
        allowflags |= ALLOW_DIG;
    if (!nohands(mtmp->data) && !verysmall(mtmp->data)) {
        allowflags |= OPENDOOR;
        if (m_carrying(mtmp, SKELETON_KEY))
            allowflags |= BUSTDOOR;
    }
    if (is_giant(mtmp->data))
        allowflags |= BUSTDOOR;
    cnt = mfndpos(mtmp, poss, info, allowflags);

    if (mtmp->isshk && avoid && uondoor) {    /* perhaps we cannot avoid him */
        for (i = 0; i < cnt; i++)
            if (!(info[i] & NOTONL))
                goto pick_move;
        avoid = FALSE;
    }
#define GDIST(x,y)      (dist2(x,y,gx,gy))
pick_move:
    chcnt = 0;
    for (i = 0; i < cnt; i++) {
        nx = poss[i].x;
        ny = poss[i].y;
        if (level->locations[nx][ny].typ == ROOM ||
            (mtmp->ispriest && level->locations[nx][ny].typ == ALTAR) ||
            (mtmp->isshk && (!in_his_shop || ESHK(mtmp)->following))) {
            if (avoid && (info[i] & NOTONL))
                continue;
            if ((!appr && !rn2(++chcnt)) ||
                (appr && GDIST(nx, ny) < GDIST(nix, niy))) {
                nix = nx;
                niy = ny;
            }
        }
    }
    if (mtmp->ispriest && avoid && nix == omx && niy == omy &&
        onlineu(omx, omy)) {
        /* might as well move closer as long it's going to stay lined up */
        avoid = FALSE;
        goto pick_move;
    }

    if (nix != omx || niy != omy) {
        remove_monster(level, omx, omy);
        place_monster(mtmp, nix, niy);
        newsym(nix, niy);
        if (mtmp->isshk && !in_his_shop && inhishop(mtmp))
            check_special_room(FALSE);
        if (ib) {
            if (cansee(mtmp->mx, mtmp->my))
                pline("%s picks up %s.", Monnam(mtmp),
                      distant_name(ib, doname));
            obj_extract_self(ib);
            mpickobj(mtmp, ib);
        }
        return 1;
    }
    return 0;
}


char
temple_occupied(char *array)
{
    char *ptr;

    for (ptr = array; *ptr; ptr++)
        if (level->rooms[*ptr - ROOMOFFSET].rtype == TEMPLE)
            return *ptr;
    return '\0';
}


static boolean
histemple_at(struct monst *priest, xchar x, xchar y)
{
    return ((boolean)
            ((CONST_EPRI(priest)->shroom == *in_rooms(level, x, y, TEMPLE)) &&
             on_level(&(CONST_EPRI(priest)->shrlevel), &u.uz)));
}

/*
 * pri_move: return 1: moved  0: didn't  -1: let m_move do it  -2: died
 */
int
pri_move(struct monst *priest)
{
    xchar gx, gy, omx, omy;
    schar temple;
    boolean avoid = TRUE;

    omx = priest->mx;
    omy = priest->my;

    if (!histemple_at(priest, omx, omy))
        return -1;

    temple = CONST_EPRI(priest)->shroom;

    gx = CONST_EPRI(priest)->shrpos.x;
    gy = CONST_EPRI(priest)->shrpos.y;

    gx += rn1(3, -1);   /* mill around the altar */
    gy += rn1(3, -1);

    if (!priest->mpeaceful || (Conflict && !resist(priest, RING_CLASS, 0, 0))) {
        if (monnear(priest, u.ux, u.uy)) {
            if (Displaced)
                pline("Your displaced image doesn't fool %s!", mon_nam(priest));
            mattacku(priest);
            return 0;
        } else if (strchr(u.urooms, temple)) {
            /* chase player if inside temple & can sense him */
            if (m_cansenseu(priest)) {
                gx = u.ux;
                gy = u.uy;
            }
            avoid = FALSE;
        }
    } else if (Invis)
        avoid = FALSE;

    return move_special(priest, FALSE, TRUE, FALSE, avoid, omx, omy, gx, gy);
}

/* exclusively for mktemple(); uses level creation RNG */
void
priestini(struct level *lev, struct mkroom *sroom, int sx, int sy,
          boolean sanctum)
{       /* is it the seat of the high priest? */
    struct monst *priest = NULL;
    struct obj *otmp;
    int cnt;

    coord *priest_pos, pos_array[] = {
        { sx + 1, sy },
        { sx - 1, sy },
        { sx, sy + 1 },
        { sx, sy - 1 },
        { sx, sy },
        { COLNO, ROWNO },
    };

    /* Search for a good position for the priest. The -1 in the array bound is
     * to ensure that we stop on the { COLNO, ROWNO } entry which is not ok. Do
     * not pass a monster to goodpos(), because we will move any monster later.
     */
    for (priest_pos = pos_array;
         !goodpos(lev, priest_pos->x, priest_pos->y, NULL, 0) &&
         (priest_pos < pos_array + ARRAY_SIZE(pos_array) - 1);
         ++priest_pos)
         {}
    
    if (!isok(priest_pos->x, priest_pos->y)) {
        impossible("Unable to find location for priest in shrine");
    } else {
        if (MON_AT(lev, priest_pos->x, priest_pos->y))
            rloc(m_at(lev, priest_pos->x, priest_pos->y), FALSE);

        priest = makemon(&mons[sanctum ? PM_HIGH_PRIEST : PM_ALIGNED_PRIEST],
                         lev, priest_pos->x, priest_pos->y, MM_ALLLEVRNG);
    }

    if (priest) {
        EPRI(priest)->shroom = (sroom - lev->rooms) + ROOMOFFSET;
        EPRI(priest)->shralign = Amask2align(lev->locations[sx][sy].altarmask);
        EPRI(priest)->shrpos.x = sx;
        EPRI(priest)->shrpos.y = sy;
        assign_level(&(EPRI(priest)->shrlevel), &lev->z);
        priest->mtrapseen = ~0; /* traps are known */
        priest->mpeaceful = 1;
        priest->ispriest = 1;
        priest->msleeping = 0;
        set_malign(priest);     /* mpeaceful may have changed */

        /* now his/her goodies... */
        if (sanctum && CONST_EPRI(priest)->shralign == A_NONE &&
            on_level(&sanctum_level, &lev->z)) {
            mongets(priest, AMULET_OF_YENDOR, rng_for_level(&lev->z));
        }
        /* 2 to 4 spellbooks */
        for (cnt = rn1(3, 2); cnt > 0; --cnt) {
            mpickobj(priest, mkobj(level, SPBOOK_CLASS, FALSE,
                                   rng_for_level(&lev->z)));
        }
        /* robe [via makemon()] */
        if (mklev_rn2(2, lev) && (otmp = which_armor(priest, os_armc)) != 0) {
            if (p_coaligned(priest))
                uncurse(otmp);
            else
                curse(otmp);
        }
    }
}

static const char *hallu_priest_types[] = {
    "priestess",
    "poohbah",  /* The Mikado */
    "priest",
    "prior"     /* Stargate */
};

/*
 * Specially aligned monsters are named specially.
 *      - aligned priests with ispriest and high priests have shrines
 *          they retain ispriest and epri when polymorphed
 *      - aligned priests without ispriest and Angels are roamers
 *          they retain isminion and access epri as emin when polymorphed
 *          (coaligned Angels are also created as minions, but they
 *          use the same naming convention)
 *      - minions do not have ispriest but have isminion and emin
 *      - caller needs to inhibit Hallucination if it wants to force
 *          the true name even when under that influence
 */
const char *
priestname(const struct monst *mon, boolean override_hallu)
{
    const char *what;
    boolean do_the = TRUE;
    const char *(*gname_function)(aligntyp) = align_gname;
    const char *pname = "";

    if (Hallucination && !override_hallu) {
        int idx = rndmonidx();

        what = monnam_for_index(idx);
        do_the = !monnam_is_pname(idx);
        gname_function = halu_gname;
    } else {
        what = mon->data->mname;
    }

    if (do_the)
        pname = "the ";
    if (mon->minvis)
        pname = msgcat(pname, "invisible ");
    if (mon->ispriest || mon->data == &mons[PM_ALIGNED_PRIEST] ||
        mon->data == &mons[PM_ANGEL]) {
        /* use epri */
        if (mon->mtame && mon->data == &mons[PM_ANGEL])
            pname = msgcat(pname, "guardian ");
        if (mon->data != &mons[PM_ALIGNED_PRIEST] &&
            mon->data != &mons[PM_HIGH_PRIEST]) {
            pname = msgcat_many(pname, what, " ", NULL);
        }
        if (mon->data != &mons[PM_ANGEL]) {
            if (!mon->ispriest && CONST_EPRI(mon)->renegade)
                pname = msgcat(pname, "renegade ");
            if (mon->data == &mons[PM_HIGH_PRIEST])
                pname = msgcat(pname, "high ");
            if (Hallucination && !override_hallu) {
                pname = msgcat(
                    pname,
                    hallu_priest_types[rn2_on_display_rng(
                            sizeof hallu_priest_types /
                            sizeof *hallu_priest_types)]);
                pname = msgcat(pname, " ");
            } else if (mon->female)
                pname = msgcat(pname, "priestess ");
            else
                pname = msgcat(pname, "priest ");
        }
        pname = msgcat(pname, "of ");
        pname = msgcat(pname, gname_function((int)CONST_EPRI(mon)->shralign));
        return pname;
    }
    /* use emin instead of epri */
    pname = msgcat(pname, what);
    pname = msgcat(pname, " of ");
    pname = msgcat(pname, gname_function((int)CONST_EMIN(mon)->min_align));
    return pname;
}

boolean
p_coaligned(const struct monst * priest)
{
    return (boolean) (u.ualign.type == ((int)CONST_EPRI(priest)->shralign));
}

static xchar
has_shrine(const struct monst *pri)
{
    struct rm *loc;

    if (!pri)
        return FALSE;
    loc = &level->locations
        [CONST_EPRI(pri)->shrpos.x][CONST_EPRI(pri)->shrpos.y];
    if (!IS_ALTAR(loc->typ))
        return FALSE;
    return (CONST_EPRI(pri)->shralign == Amask2align(loc->altarmask & AM_MASK))
        ? loc->altarmask & (AM_SHRINE | AM_SANCTUM) : 0;
}

struct monst *
findpriest(char roomno)
{
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (mtmp->ispriest && (CONST_EPRI(mtmp)->shroom == roomno) &&
            histemple_at(mtmp, mtmp->mx, mtmp->my))
            return mtmp;
    }
    return NULL;
}

/* called from check_special_room() when the player enters the temple room */
void
intemple(int roomno)
{
    struct monst *priest = findpriest((char)roomno);
    boolean tended = (priest != NULL);
    boolean sanctum, can_speak;
    xchar shrined;
    const char *msg1, *msg2;

    if (!temple_occupied(u.urooms0)) {
        if (tended) {
            shrined = has_shrine(priest);
            sanctum = (priest->data == &mons[PM_HIGH_PRIEST] &&
                       (shrined & AM_SANCTUM));
            can_speak = (priest->mcanmove && !priest->msleeping &&
                         canhear());
            if (can_speak) {
                unsigned save_priest = priest->ispriest;

                /* don't reveal the altar's owner upon temple entry in the
                   endgame; for the Sanctum, the next message names Moloch so
                   suppress the "of Moloch" for him here too */
                if (sanctum && !Hallucination)
                    priest->ispriest = 0;
                pline("%s intones:",
                      canseemon(priest) ? Monnam(priest) : "A nearby voice");
                priest->ispriest = save_priest;
            }
            msg2 = 0;
            if (sanctum && CONST_EPRI(priest)->shralign == A_NONE) {
                if (priest->mpeaceful) {
                    msg1 = "Infidel, you have entered Moloch's Sanctum!";
                    msg2 = "Be gone!";
                    msethostility(priest, TRUE, TRUE);
                } else
                    msg1 = "You desecrate this place by your presence!";
            } else {
                msg1 = msgprintf("Pilgrim, you enter a %s place!",
                                 !shrined ? "desecrated" : "sacred");
            }
            if (can_speak) {
                verbalize("%s", msg1);
                if (msg2)
                    verbalize("%s", msg2);
            }
            if (!sanctum) {
                /* !tended -> !shrined */
                if (!shrined || !p_coaligned(priest) ||
                    u.ualign.record <= ALGN_SINNED)
                    pline("You have a%s forbidding feeling...",
                          (!shrined) ? "" : " strange");
                else
                    pline("You experience a strange sense of peace.");
            }
        } else {
            switch (rn2(3)) {
            case 0:
                pline("You have an eerie feeling...");
                break;
            case 1:
                pline("You feel like you are being watched.");
                break;
            default:
                pline("A shiver runs down your %s.", body_part(SPINE));
                break;
            }
            if (!rn2(5)) {
                struct monst *mtmp;

                if (!((mtmp = makemon(&mons[PM_GHOST], level, 
                                      u.ux, u.uy, NO_MM_FLAGS))))
                    return;
                if (!Blind || sensemon(mtmp))
                    pline("An enormous ghost appears next to you!");
                else
                    pline("You sense a presence close by!");
                msethostility(mtmp, TRUE, TRUE);
                if (flags.verbose)
                    pline("You are frightened to death, and unable to move.");
                helpless(3, hr_afraid, "frightened to death",
                         "You regain your composure.");
            }
        }
    }
}

void
priest_talk(struct monst *priest)
{
    boolean coaligned = p_coaligned(priest);
    boolean strayed = (u.ualign.record < 0);

    /* KMH, conduct */
    break_conduct(conduct_gnostic);

    if (priest->mflee || (!priest->ispriest && coaligned && strayed)) {
        pline("%s doesn't want anything to do with you!", Monnam(priest));
        msethostility(priest, TRUE, FALSE);
        return;
    }

    /* priests don't chat unless peaceful and in their own temple */
    if (!histemple_at(priest, priest->mx, priest->my) || !priest->mpeaceful ||
        !priest->mcanmove || priest->msleeping) {
        static const char *const cranky_msg[3] = {
            "Thou wouldst have words, eh?  I'll give thee a word or two!",
            "Talk?  Here is what I have to say!",
            "Pilgrim, I would speak no longer with thee."
        };

        if (!priest->mcanmove || priest->msleeping) {
            pline("%s breaks out of %s reverie!", Monnam(priest), mhis(priest));
            priest->mfrozen = priest->msleeping = 0;
            priest->mcanmove = 1;
        }
        msethostility(priest, TRUE, FALSE);
        verbalize("%s", cranky_msg[rn2(3)]);
        return;
    }

    /* you desecrated the temple and now you want to chat? */
    if (priest->mpeaceful && *in_rooms(level, priest->mx, priest->my, TEMPLE) &&
        !has_shrine(priest)) {
        verbalize
            ("Begone!  Thou desecratest this holy place with thy presence.");
        msethostility(priest, TRUE, FALSE);
        return;
    }
    if (!money_cnt(invent)) {
        if (coaligned && !strayed) {
            long pmoney = money_cnt(priest->minvent);

            if (pmoney > 0L) {
                /* Note: two bits is actually 25 cents.  Hmm. */
                pline("%s gives you %s for an ale.", Monnam(priest),
                      (pmoney == 1L) ? "one bit" : "two bits");
                money2u(priest, pmoney > 1L ? 2 : 1);
            } else
                pline("%s preaches the virtues of poverty.", Monnam(priest));
            exercise(A_WIS, TRUE);
        } else
            pline("%s is not interested.", Monnam(priest));
        return;
    } else {
        long offer;

        pline("%s asks you for a contribution for the temple.", Monnam(priest));
        if ((offer = bribe(priest)) == 0) {
            verbalize("Thou shalt regret thine action!");
            if (coaligned)
                adjalign(-1);
        } else if (offer < (u.ulevel * 200)) {
            if (money_cnt(invent) > (offer * 2L))
                verbalize("Cheapskate.");
            else {
                verbalize("I thank thee for thy contribution.");
                /* give player some token */
                exercise(A_WIS, TRUE);
            }
        } else if (offer < (u.ulevel * 400)) {
            verbalize("Thou art indeed a pious individual.");
            if (money_cnt(invent) < (offer * 2L)) {
                if (coaligned && u.ualign.record <= ALGN_SINNED)
                    adjalign(1);
                verbalize("I bestow upon thee a blessing.");
                incr_itimeout(&HClairvoyant, rn1(500, 500));
            }
        } else if (offer < (u.ulevel * 600) && u.ublessed < 20 &&
                   (u.ublessed < 9 || !rn2(u.ublessed))) {
            verbalize("Thy devotion has been rewarded.");
            if (!(HProtection & INTRINSIC)) {
                HProtection |= FROMOUTSIDE;
                if (!u.ublessed)
                    u.ublessed = rn1(3, 2);
            } else
                u.ublessed++;
        } else {
            verbalize("Thy selfless generosity is deeply appreciated.");
            if (money_cnt(invent) < (offer * 2L) && coaligned) {
                if (strayed && (moves - u.ucleansed) > 5000L) {
                    u.ualign.record = 0;        /* cleanse thee */
                    u.ucleansed = moves;
                } else {
                    adjalign(2);
                }
            }
        }
    }
}

struct monst *
mk_roamer(const struct permonst *ptr, aligntyp alignment, struct level *lev,
          xchar x, xchar y, boolean peaceful, int mm_flags)
{
    struct monst *roamer;
    boolean coaligned = (u.ualign.type == alignment);

    if (ptr != &mons[PM_ALIGNED_PRIEST] && ptr != &mons[PM_ANGEL])
        return NULL;

    if (MON_AT(lev, x, y))
        rloc(m_at(lev, x, y), FALSE);   /* insurance */

    if (!(roamer = makemon(ptr, lev, x, y, mm_flags)))
        return NULL;

    EPRI(roamer)->shralign = alignment;
    if (coaligned && !peaceful)
        EPRI(roamer)->renegade = TRUE;
    /* roamer->ispriest == FALSE naturally */
    roamer->isminion = TRUE;    /* borrowing this bit */
    roamer->mtrapseen = ~0;     /* traps are known */
    msethostility(roamer, !peaceful, TRUE); /* TODO: handle in_mklev */
    roamer->msleeping = 0;

    /* MORE TO COME */
    return roamer;
}

void
reset_hostility(struct monst *roamer)
{
    if (! (roamer->isminion &&
           (roamer->data == &mons[PM_ALIGNED_PRIEST] ||
            roamer->data == &mons[PM_ANGEL])))
        return;

    if (EPRI(roamer)->shralign != u.ualign.type)
        msethostility(roamer, TRUE, TRUE);
}

boolean
in_your_sanctuary(struct monst *mon,    /* if non-null, <mx,my> overrides <x,y> 
                                         */
                  xchar x, xchar y)
{
    char roomno;
    struct monst *priest;

    if (mon) {
        if (is_minion(mon->data) || is_rider(mon->data))
            return FALSE;
        x = mon->mx, y = mon->my;
    }
    if (u.ualign.record <= ALGN_SINNED) /* sinned or worse */
        return FALSE;
    if ((roomno = temple_occupied(u.urooms)) == 0 ||
        roomno != *in_rooms(level, x, y, TEMPLE))
        return FALSE;
    if ((priest = findpriest(roomno)) == 0)
        return FALSE;
    return (boolean) (has_shrine(priest) && p_coaligned(priest) &&
                      priest->mpeaceful);
}

/* when attacking "priest" in his temple */
void
ghod_hitsu(struct monst *priest)
{
    int x, y, ax, ay, roomno = (int)temple_occupied(u.urooms);
    struct mkroom *troom;

    if (!roomno || !has_shrine(priest))
        return;

    ax = x = CONST_EPRI(priest)->shrpos.x;
    ay = y = CONST_EPRI(priest)->shrpos.y;
    troom = &level->rooms[roomno - ROOMOFFSET];

    if ((u.ux == x && u.uy == y) || !linedup(u.ux, u.uy, x, y)) {
        if (IS_DOOR(level->locations[u.ux][u.uy].typ)) {

            if (u.ux == troom->lx - 1) {
                x = troom->hx;
                y = u.uy;
            } else if (u.ux == troom->hx + 1) {
                x = troom->lx;
                y = u.uy;
            } else if (u.uy == troom->ly - 1) {
                x = u.ux;
                y = troom->hy;
            } else if (u.uy == troom->hy + 1) {
                x = u.ux;
                y = troom->ly;
            }
        } else {
            switch (rn2(4)) {
            case 0:
                x = u.ux;
                y = troom->ly;
                break;
            case 1:
                x = u.ux;
                y = troom->hy;
                break;
            case 2:
                x = troom->lx;
                y = u.uy;
                break;
            default:
                x = troom->hx;
                y = u.uy;
                break;
            }
        }
        if (!linedup(u.ux, u.uy, x, y))
            return;
    }

    switch (rn2(3)) {
    case 0:
        pline("%s roars in anger:  \"Thou shalt suffer!\"", a_gname_at(ax, ay));
        break;
    case 1:
        pline("%s voice booms:  \"How darest thou harm my servant!\"",
              s_suffix(a_gname_at(ax, ay)));
        break;
    default:
        pline("%s roars:  \"Thou dost profane my shrine!\"",
              a_gname_at(ax, ay));
        break;
    }

    /* bolt of lightning */
    buzz(-10 - (AD_ELEC - 1), 6, x, y, sgn(tbx), sgn(tby));
    exercise(A_WIS, FALSE);
}

void
angry_priest(void)
{
    struct monst *priest;
    struct rm *loc;

    if ((priest = findpriest(temple_occupied(u.urooms))) != 0) {
        wakeup(priest, FALSE);
        /* 
         * If the altar has been destroyed or converted, let the
         * priest run loose.
         * (When it's just a conversion and there happens to be
         *  a fresh corpse nearby, the priest ought to have an
         *  opportunity to try converting it back; maybe someday...)
         */
        loc = &level->locations
            [CONST_EPRI(priest)->shrpos.x][CONST_EPRI(priest)->shrpos.y];
        if (!IS_ALTAR(loc->typ) ||
            ((aligntyp) Amask2align(loc->altarmask & AM_MASK) !=
             CONST_EPRI(priest)->shralign)) {
            priest->ispriest = 0;       /* now a roamer */
            priest->isminion = 1;       /* but still aligned */
            /* this overloads the `shroom' field, which is now clobbered */
            EPRI(priest)->renegade = 0;
        }
    }
}

/*
 * When saving bones, find priests that aren't on their shrine level,
 * and remove them.   This avoids big problems when restoring bones.
 */
void
clearpriests(void)
{
    struct monst *mtmp, *mtmp2;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;
        if (!DEADMONSTER(mtmp) && mtmp->ispriest &&
            !on_level(&(CONST_EPRI(mtmp)->shrlevel), &u.uz))
            mongone(mtmp);
    }
}

/* munge priest-specific structure when restoring -dlc */
void
restpriest(struct monst *mtmp, boolean ghostly)
{
    if (u.uz.dlevel) {
        if (ghostly)
            assign_level(&(EPRI(mtmp)->shrlevel), &u.uz);
    }
}

/*priest.c*/

