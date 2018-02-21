/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-02-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "hungerstatus.h"
#include "zap.h"

static int use_camera(struct obj *, const struct nh_cmd_arg *);
static int use_towel(struct obj *);
static boolean its_dead(int, int, int *);
static int use_stethoscope(struct obj *, const struct nh_cmd_arg *);
static int use_whistle(struct obj *);
static int use_magic_whistle(struct obj *);
static int use_leash(struct obj *, const struct nh_cmd_arg *);
static int use_mirror(struct obj *, const struct nh_cmd_arg *);
static void use_bell(struct obj **);
static int use_candelabrum(struct obj *);
static int use_candle(struct obj **);
static int use_lamp(struct obj *);
static int light_cocktail(struct obj *);
static int use_tinning_kit(struct obj *);
static int use_figurine(struct obj **obj, const struct nh_cmd_arg *);
static int use_grease(struct obj *);
static int use_trap(struct obj *, const struct nh_cmd_arg *);
static int use_stone(struct obj *);
static int set_trap(void);      /* occupation callback */
static int use_whip(struct obj *, const struct nh_cmd_arg *);
static boolean find_polearm_target(struct obj *, int, int, coord *);
static boolean update_polearm_target(struct obj *, int, int, coord *);
static boolean find_polearm_target_recurse(struct obj *, int, int, coord *,
                                           boolean, boolean);
static int use_cream_pie(struct obj **);
static int use_grapple(struct obj *, const struct nh_cmd_arg *);
static boolean figurine_location_checks(struct obj *, coord *, boolean);
static boolean uhave_graystone(void);

static const char no_elbow_room[] =
    "You don't have enough elbow-room to maneuver.";

static int
use_camera(struct obj *obj, const struct nh_cmd_arg *arg)
{
    schar dx, dy, dz;

    if (Underwater) {
        pline(msgc_cancelled,
              "Using your camera underwater would void the warranty.");
        return 0;
    }
    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    if (obj->spe <= 0) {
        pline(msgc_failcurse, "The camera is out of film.");
        obj->known = 1;
        return 1;
    }
    consume_obj_charge(obj, TRUE);

    if (obj->cursed && !rn2(2))
        bhitm(&youmonst, &youmonst, obj, 10);
    else if (Engulfed)
        pline(msgc_yafm,
              "You take a picture of %s %s.", s_suffix(mon_nam(u.ustuck)),
              mbodypart(u.ustuck, STOMACH));
    else if (dz)
        pline(msgc_yafm, "You take a picture of the %s.",
              (dz > 0) ? surface(u.ux, u.uy) : ceiling(u.ux, u.uy));
    else
        weffects(&youmonst, obj, dx, dy, dz);
    return 1;
}

static int
use_towel(struct obj *obj)
{
    if (!freehand()) {
        pline(msgc_cancelled, "You have no free %s!", body_part(HAND));
        return 0;
    } else if (obj->owornmask & W_WORN) {
        pline(msgc_cancelled, "You cannot use it while you're wearing it!");
        return 0;
    } else if (obj->cursed) {
        long old;

        switch (rn2(3)) {
        case 2:
            old = slippery_fingers(&youmonst);
            inc_timeout(&youmonst, GLIB, rn1(10, 3), TRUE);
            pline(msgc_statusbad, "Your %s %s!", makeplural(body_part(HAND)),
                  (old ? "are filthier than ever" : "get slimy"));
            return 1;
        case 1:
            if (!ublindf) {
                old = blind(&youmonst);
                pline(msgc_statusbad, "Yecch! Your %s %s gunk on it!",
                      body_part(FACE), (u.ucreamed ? "has more" : "now has"));
                u.ucreamed += rn1(10, 3);
                if (blind(&youmonst) && !old)
                    pline(msgc_statusbad, "You can't see any more.");
            } else {
                const char *what =
                    (ublindf->otyp == LENSES) ? "lenses" : "blindfold";
                if (ublindf->cursed) {
                    pline(msgc_substitute, "You push your %s %s.", what,
                          rn2(2) ? "cock-eyed" : "crooked");
                } else {
                    struct obj *saved_ublindf = ublindf;

                    pline(msgc_substitute, "You push your %s off.", what);
                    setequip(os_tool, NULL, em_silent);
                    dropx(saved_ublindf);
                }
            }
            return 1;
        case 0:
            break;
        }
    }

    if (slippery_fingers(&youmonst)) {
        set_property(&youmonst, GLIB, -2, TRUE);
        pline(msgc_statusheal, "You wipe off your %s.",
              makeplural(body_part(HAND)));
        return 1;
    } else if (u.ucreamed) {
        boolean wasblind = blind(&youmonst);
        u.ucreamed = 0;
        if (wasblind && !blind(&youmonst)) {
            pline(msgc_statusheal, "You've got the glop off.");
            pline(msgc_statusheal, "You can see again.");
        } else
            pline(msgc_statusheal, "Your %s feels clean now.", body_part(FACE));
        return 1;
    }

    pline(msgc_cancelled, "Your %s and %s are already clean.", body_part(FACE),
          makeplural(body_part(HAND)));

    return 0;
}

/* maybe give a stethoscope message based on floor objects */
static boolean
its_dead(int rx, int ry, int *resp)
{
    struct obj *otmp;
    struct trap *ttmp;

    if (!can_reach_floor())
        return FALSE;

    /* additional stethoscope messages from jyoung@apanix.apana.org.au */
    if (Hallucination && sobj_at(CORPSE, level, rx, ry)) {
        /* (a corpse doesn't retain the monster's sex, so we're forced to use
           generic pronoun here) */
        You_hear(msgc_yafm, "a voice say, \"It's dead, Jim.\"");
        *resp = 1;
        return TRUE;
    } else if (Role_if(PM_HEALER) &&
               ((otmp = sobj_at(CORPSE, level, rx, ry)) != 0 ||
                (otmp = sobj_at(STATUE, level, rx, ry)) != 0)) {
        /* possibly should check uppermost {corpse,statue} in the pile if both
           types are present, but it's not worth the effort */
        if (vobj_at(rx, ry)->otyp == STATUE)
            otmp = vobj_at(rx, ry);
        if (otmp->otyp == CORPSE) {
            pline(msgc_yafm, "You determine that %s unfortunate being is dead.",
                  (rx == u.ux && ry == u.uy) ? "this" : "that");
        } else {
            ttmp = t_at(level, rx, ry);
            pline(msgc_yafm, "%s appears to be in %s health for a statue.",
                  The(opm_name(otmp)),
                  (ttmp && ttmp->ttyp == STATUE_TRAP) ?
                  "extraordinary" : "excellent");
        }
        return TRUE;
    }
    return FALSE;
}

static const char hollow_str[] = "a hollow sound.  This must be a secret %s!";

/* Strictly speaking it makes no sense for usage of a stethoscope to not take
   any time; however, unless it did, the stethoscope would be almost useless.
   As a compromise, one use per action per stethoscope is free, another uses up
   the turn; this makes curse status have a tangible effect.

   The last stethoscope use turn is stored in obj->lastused; the last
   stethoscope use flags.actions is stored in obj->spe. */
static int
use_stethoscope(struct obj *obj, const struct nh_cmd_arg *arg)
{
    struct monst *mtmp;
    struct rm *loc;
    int rx, ry, res;
    schar dx, dy, dz;
    boolean interference = (Engulfed && is_whirly(u.ustuck->data) &&
                            !rn2(Role_if(PM_HEALER) ? 10 : 3));

    if (nohands(youmonst.data)) {      /* also check for no ears and/or deaf? */
        pline(msgc_cancelled, "You have no hands!"); /* not `body_part(HAND)' */
        return 0;
    } else if (!freehand()) {
        pline(msgc_cancelled, "You have no free %s.", body_part(HAND));
        return 0;
    }
    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    res = (moves == obj->lastused) && (flags.actions == obj->spe);
    if (obj->blessed)
        res = 0;

    obj->lastused = moves;
    obj->spe = flags.actions;

    if (u.usteed && dz > 0) {
        if (interference) {
            pline(msgc_substitute, "%s interferes.", Monnam(u.ustuck));
            mstatusline(u.ustuck);
        } else
            mstatusline(u.usteed);
        return res;
    } else if (Engulfed && (dx || dy || dz)) {
        mstatusline(u.ustuck);
        return res;
    } else if (Engulfed && interference) {
        pline(msgc_substitute, "%s interferes.", Monnam(u.ustuck));
        mstatusline(u.ustuck);
        return res;
    } else if (dz) {
        if (Underwater)
            You_hear(msgc_yafm, "faint splashing.");
        else if (dz < 0 || !can_reach_floor())
            pline(msgc_cancelled1, "You can't reach the %s.",
                  (dz > 0) ? surface(u.ux, u.uy) : ceiling(u.ux, u.uy));
        else if (its_dead(u.ux, u.uy, &res))
            ;  /* message already given */
        else if (Is_stronghold(&u.uz))
            You_hear(msgc_actionok, "the crackling of hellfire.");
        else
            pline(msgc_yafm, "The %s seems healthy enough.",
                  surface(u.ux, u.uy));
        return res;
    } else if (obj->cursed && !rn2(2)) {
        You_hear(obj->bknown ? msgc_failrandom : msgc_failcurse,
                 "your heart beat.");
        obj->bknown = TRUE;
        return res;
    }
    if (Stunned || (Confusion && !rn2(5)))
        confdir(&dx, &dy);
    if (!dx && !dy) {
        mstatusline(&youmonst);
        return res;
    }

    rx = u.ux + dx;
    ry = u.uy + dy;
    if (!isok(rx, ry)) {
        You_hear(msgc_yafm, "a faint typing noise.");
        return 0;
    }

    reveal_monster_at(rx, ry, FALSE);

    if ((mtmp = m_at(level, rx, ry)) != 0) {
        mstatusline(mtmp);
        reveal_monster_at(rx, ry, FALSE);
        return res;
    }

    loc = &level->locations[rx][ry];
    switch (loc->typ) {
    case SDOOR:
        You_hear(msgc_youdiscover, hollow_str, "door");
        cvt_sdoor_to_door(loc, &u.uz);  /* ->typ = DOOR */
        if (Blind)
            feel_location(rx, ry);
        else
            newsym(rx, ry);
        return res;
    case SCORR:
        You_hear(msgc_youdiscover, hollow_str, "passage");
        loc->typ = CORR;
        unblock_point(rx, ry);
        if (Blind)
            feel_location(rx, ry);
        else
            newsym(rx, ry);
        return res;
    }

    if (!its_dead(rx, ry, &res))
        pline(msgc_notarget, "You hear nothing special.");  /* not You_hear() */
    return res;
}

static const char whistle_str[] = "You produce a %s whistling sound.";

static int
use_whistle(struct obj *obj)
{
    if (Upolyd && !can_blow_instrument(youmonst.data)) {
        pline(msgc_cancelled, "You are incapable of blowing the whistle!");
        return 0;
    }
    pline(msgc_actionok, whistle_str, obj->cursed ? "shrill" : "high");
    tell_discovery(obj);
    wake_nearby(TRUE);

    struct monst *mtmp;
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp) && mtmp->mtame && !isminion(mtmp))
            mx_edog(mtmp)->whistletime = moves;

    return 1;
}

static int
use_magic_whistle(struct obj *obj)
{
    struct monst *mtmp, *nextmon;

    if (!can_blow_instrument(youmonst.data)) {
        pline(msgc_cancelled, "You are incapable of blowing the whistle!");
        return 0;
    }

    if (obj->cursed && !rn2(2)) {
        pline(msgc_substitute, "You produce a high-pitched humming noise.");
        wake_nearby(FALSE);
    } else if (!obj->blessed && level->flags.noteleport)
        pline(msgc_substitute, "A mysterious force interferes with the magic!");
    else {
        pline(msgc_actionok, whistle_str, Hallucination ? "normal" : "strange");
        for (mtmp = level->monlist; mtmp; mtmp = nextmon) {
            nextmon = mtmp->nmon;       /* trap might kill mon */
            if (DEADMONSTER(mtmp))
                continue;
            if (mtmp->mtame) {
                if (mtmp->mtrapped) {
                    /* no longer in previous trap (affects mintrap) */
                    mtmp->mtrapped = 0;
                    fill_pit(level, mtmp->mx, mtmp->my);
                }
                mnexto(mtmp);
                if (mintrap(mtmp) == 2)
                    change_luck(-1);
            }
        }
    }
    tell_discovery(obj);
    return 1;
}

boolean
um_dist(xchar x, xchar y, xchar n)
{
    return (boolean) (abs(u.ux - x) > n || abs(u.uy - y) > n);
}

int
number_leashed(void)
{
    int i = 0;
    struct obj *obj;

    for (obj = youmonst.minvent; obj; obj = obj->nobj)
        if (obj->otyp == LEASH && obj->leashmon != 0)
            i++;
    return i;
}

void
o_unleash(struct obj *otmp)
{       /* otmp is about to be destroyed or stolen */
    struct monst *mtmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (mtmp->m_id == (unsigned)otmp->leashmon)
            mtmp->mleashed = 0;
    otmp->leashmon = 0;
}

void
m_unleash(struct monst *mtmp, boolean feedback)
{       /* mtmp is about to die, or become untame */
    struct obj *otmp;

    if (feedback) {
        /* Note: use petfatal here because callers rely on this function to
           inform the player of trouble when feedback is set */
        if (canseemon(mtmp))
            pline(msgc_petfatal, "%s pulls free of %s leash!", Monnam(mtmp),
                  mhis(mtmp));
        else
            pline(msgc_petfatal, "Your leash falls slack.");
    }
    for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == LEASH && otmp->leashmon == (int)mtmp->m_id)
            otmp->leashmon = 0;
    mtmp->mleashed = 0;
}

void
unleash_all(void)
{       /* player is about to die (for bones) */
    struct obj *otmp;
    struct monst *mtmp;

    for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == LEASH)
            otmp->leashmon = 0;
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        mtmp->mleashed = 0;
}

#define MAXLEASHED      2

/* ARGSUSED */
static int
use_leash(struct obj *obj, const struct nh_cmd_arg *arg)
{
    coord cc;
    struct monst *mtmp;
    int spotmon;
    schar dx, dy, dz;

    if (!obj->leashmon && number_leashed() >= MAXLEASHED) {
        pline(msgc_cancelled, "You cannot leash any more pets.");
        return 0;
    }

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    cc.x = u.ux + dx;
    cc.y = u.uy + dy;
    if (!isok(cc.x, cc.y))
        return 0;

    if (!dx && !dy) {
        if (u.usteed && dz > 0) {
            mtmp = u.usteed;
            spotmon = 1;
            goto got_target;
        }
        pline(msgc_cancelled, "Leash yourself?  Very funny...");
        return 0;
    }

    if (knownwormtail(cc.x, cc.y)) {
        pline(msgc_cancelled, "You can't leash a monster by its tail!");
        return 0;
    }

    if (!((mtmp = m_at(level, cc.x, cc.y))) ||
        mtmp->mx != cc.x || mtmp->my != cc.y) {
        pline(msgc_mispaste, "There is no creature there.");
        return 0;
    }

    spotmon = canspotmon(mtmp);
got_target:

    if (!mtmp->mtame) {
        if (!spotmon)
            pline(msgc_mispaste, "There is no creature there.");
        else
            pline(msgc_cancelled, "%s %s leashed!", Monnam(mtmp),
                  (!obj->leashmon) ? "cannot be" : "is not");
        return 0;
    }
    if (!obj->leashmon) {
        if (mtmp->mleashed) {
            pline(msgc_cancelled, "This %s is already leashed.",
                  spotmon ? l_monnam(mtmp) : "monster");
            return 0;
        }
        pline(msgc_actionok, "You slip the leash around %s%s.",
              spotmon ? "your " : "", l_monnam(mtmp));
        mtmp->mleashed = 1;
        obj->leashmon = (int)mtmp->m_id;
        mtmp->msleeping = 0;
        return 1;
    }
    if (obj->leashmon != (int)mtmp->m_id) {
        pline(msgc_cancelled, "This leash is not attached to that creature.");
        return 0;
    } else {
        if (obj->cursed) {
            pline(obj->bknown ? msgc_cancelled1 : msgc_failcurse,
                  "The leash would not come off!");
            obj->bknown = TRUE;
            return 1;
        }
        mtmp->mleashed = 0;
        obj->leashmon = 0;
        pline(msgc_actionok, "You remove the leash from %s%s.",
              spotmon ? "your " : "", l_monnam(mtmp));
    }
    return 1;
}

struct obj *
get_mleash(struct monst *mtmp)
{       /* assuming mtmp->mleashed has been checked */
    struct obj *otmp;

    otmp = youmonst.minvent;
    while (otmp) {
        if (otmp->otyp == LEASH && otmp->leashmon == (int)mtmp->m_id)
            return otmp;
        otmp = otmp->nobj;
    }
    return NULL;
}


boolean
next_to_u(void)
{
    struct monst *mtmp;
    struct obj *otmp;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (mtmp->mleashed) {
            if (distu(mtmp->mx, mtmp->my) > 2)
                mnexto(mtmp);
            if (distu(mtmp->mx, mtmp->my) > 2) {
                for (otmp = youmonst.minvent; otmp;
                     otmp = otmp->nobj)
                    if (otmp->otyp == LEASH &&
                        otmp->leashmon == (int)mtmp->m_id) {
                        if (otmp->cursed)
                            return FALSE;
                        pline(msgc_petfatal, "You feel %s leash go slack.",
                              (number_leashed() > 1) ? "a" : "the");
                        mtmp->mleashed = 0;
                        otmp->leashmon = 0;
                    }
            }
        }
    }
    /* no pack mules for the Amulet */
    if (u.usteed && mon_has_amulet(u.usteed))
        return FALSE;
    return TRUE;
}


void
check_leash(xchar x, xchar y)
{
    struct obj *otmp;
    struct monst *mtmp;

    for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj) {
        if (otmp->otyp != LEASH || otmp->leashmon == 0)
            continue;
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
            if (DEADMONSTER(mtmp))
                continue;
            if ((int)mtmp->m_id == otmp->leashmon)
                break;
        }
        if (!mtmp) {
            impossible("leash in use isn't attached to anything?");
            otmp->leashmon = 0;
            continue;
        }
        if (dist2(u.ux, u.uy, mtmp->mx, mtmp->my) >
            dist2(x, y, mtmp->mx, mtmp->my)) {
            if (!um_dist(mtmp->mx, mtmp->my, 3)) {
                ;       /* still close enough */
            } else if (otmp->cursed && !breathless(mtmp->data)) {
                if (um_dist(mtmp->mx, mtmp->my, 5) ||
                    (mtmp->mhp -= rnd(2)) <= 0) {
                    int save_pacifism = u.uconduct[conduct_killer];
                    int turn = u.uconduct_time[conduct_killer];

                    pline(msgc_petfatal, "Your leash chokes %s to death!",
                          mon_nam(mtmp));
                    /* hero might not have intended to kill pet, but that's the 
                       result of his actions; gain experience, lose pacifism,
                       take alignment and luck hit, make corpse less likely to
                       remain tame after revival */
                    xkilled(mtmp, 0);   /* no "you kill it" message */
                    /* life-saving doesn't ordinarily reset this */
                    if (!DEADMONSTER(mtmp)) {
                        u.uconduct[conduct_killer] = save_pacifism;
                        u.uconduct_time[conduct_killer] = turn;
                    }
                } else {
                    pline(msgc_petfatal, "%s chokes on the leash!",
                          Monnam(mtmp));
                    /* tameness eventually drops to 1 here (never 0) */
                    if (mtmp->mtame && rn2(mtmp->mtame))
                        mtmp->mtame--;
                }
            } else {
                if (um_dist(mtmp->mx, mtmp->my, 5)) {
                    pline(msgc_petfatal, "%s leash snaps loose!",
                          s_suffix(Monnam(mtmp)));
                    m_unleash(mtmp, FALSE);
                } else {
                    pline(msgc_petwarning, "You pull on the leash.");
                    if (mtmp->data->msound != MS_SILENT)
                        switch (rn2(3)) {
                        case 0:
                            growl(mtmp);
                            break;
                        case 1:
                            yelp(mtmp);
                            break;
                        default:
                            whimper(mtmp);
                            break;
                        }
                }
            }
        }
    }
}

static const char look_str[] = "You look %s.";

static int
use_mirror(struct obj *obj, const struct nh_cmd_arg *arg)
{
    struct monst *mon[COLNO];
    boolean vis;
    schar dx, dy, dz;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    if (obj->cursed && !rn2(2)) {
        if (!Blind) {
            pline(obj->bknown ? msgc_failrandom : msgc_failcurse,
                  "The %s fogs up and doesn't reflect!",
                  simple_typename(obj->otyp));
            obj->bknown = TRUE;
        }
        return 1;
    }
    if (!dx && !dy && !dz) {
        if (!Blind && !Invisible) {
            if (u.umonnum == PM_FLOATING_EYE) {
                if (!Free_action) {
                    if (Hallucination)
                        pline(msgc_statusbad, "Yow!  The %s stares back!",
                              simple_typename(obj->otyp));
                    else
                        pline(msgc_statusbad,
                              "Yikes!  You've frozen yourself!");
                    helpless(rnd((MAXULEV + 6) - u.ulevel), hr_paralyzed,
                             "gazing into a mirror", NULL);
                } else
                    pline(msgc_yafm,
                          "You stiffen momentarily under your gaze.");
            } else if (youmonst.data->mlet == S_VAMPIRE)
                pline(msgc_yafm, "You don't have a reflection.");
            else if (u.umonnum == PM_UMBER_HULK) {
                pline(msgc_statusbad, "Huh?  That doesn't look like you!");
                inc_timeout(&youmonst, CONFUSION, dice(3, 4), TRUE);
            } else if (Hallucination)
                pline(msgc_yafm, look_str, hcolor(NULL));
            else if (sick(&youmonst))
                pline(msgc_yafm, look_str, "peaked");
            else if (u.uhs >= WEAK)
                pline(msgc_yafm, look_str, "undernourished");
            else
                pline(msgc_yafm, "You look as %s as ever.", beautiful());
        } else {
            /* TODO: see invisible? or does that not work in mirrors? */
            pline(Blind ? msgc_cancelled1 : msgc_yafm,
                  "You can't see your %s %s.", beautiful(), body_part(FACE));
        }
        return 1;
    }
    if (Engulfed) {
        if (!Blind)
            pline(msgc_yafm, "You reflect %s %s.", s_suffix(mon_nam(u.ustuck)),
                  mbodypart(u.ustuck, STOMACH));
        return 1;
    }
    if (Underwater) {
        pline(msgc_yafm, Hallucination ?
              "You give the fish a chance to fix their makeup." :
              "You reflect the murky water.");
        return 1;
    }
    if (dz) {
        if (!Blind)
            pline(msgc_yafm, "You reflect the %s.",
                  (dz > 0) ? surface(u.ux, u.uy) : ceiling(u.ux, u.uy));
        return 1;
    }

    schar sx = u.ux;
    schar sy = u.uy;
    int range = COLNO;
    struct monst *hitmon, *hitby;
    boolean self = FALSE;
    int obstr[COLNO]; /* how obstructed is the view */
    int obstructing = 0;
    int amount = 0;
    /* Add potentially affected monsters to mon[] */
    while (range--) {
        sx += dx;
        sy += dy;
        if (!isok(sx, sy) ||
            !ZAP_POS(level->locations[sx][sy].typ) ||
            closed_door(level, sx, sy))
            break;
        if (!(hitmon = m_at(level, sx, sy)))
            continue;
        mon[amount] = hitmon;
        /* Invisible monsters doesn't obstruct anything */
        if (invisible(hitmon))
            continue;

        obstructing += rnd(20);
        obstr[amount] = obstructing;
        amount++;
        if (obstructing > 25)
            break; /* at this point, the view is too obstructed */
    }

    /* Iterate through affected monsters */
    int i, j, randobstr;
    for (i = 0; i < amount; i++) {
        hitmon = mon[i];
        vis = canseemon(hitmon);

        /* can this monster be affected in first place? */
        if (hitmon->msleeping) {
            if (vis)
                pline(msgc_combatimmune, "%s is too tired to look at your %s.",
                      Monnam(hitmon), simple_typename(obj->otyp));
            continue;
        }
        if (blind(hitmon) || !haseyes(hitmon->data)) {
            if (vis)
                pline(msgc_combatimmune, "%s can't see anything.",
                      Monnam(hitmon));
            continue;
        }
        randobstr = rnd(30); /* limit until view is too obstructed */
        /* Do a secondary iteration, in case the monster is affected by something
           behind or in front */
        for (j = 0; j < amount; j++) {
            self = FALSE;
            hitby = mon[j];
            if (DEADMONSTER(hitby))
                continue; /* can happen if some monsters died in the iteration */
            if (hitmon == hitby)
                self = TRUE;
            if (hitby->data->mlet == S_VAMPIRE || noncorporeal(hitby->data)) {
                if (vis && self)
                    pline(msgc_combatimmune, "%s doesn't have a reflection.",
                          Monnam(hitmon));
                continue;
            }
            if (randobstr < (self && !j ? 0 : obstr[j - 1]) +
                (self && !i ? 0 : obstr[i - 1])) {
                if (vis)
                    pline(msgc_combatimmune, "%s doesn't notice any "
                          "reflections with the obstructed view.",
                          Monnam(hitmon));
                break; /* obstruction prevents monsters seeing */
            }
            if (!rn2(self ? 2 : 4)) {
                if (self && vis)
                    pline(combat_msgc(hitby, hitmon, cr_immune),
                          "%s doesn't pay attention to %s reflection.",
                          Monnam(hitmon), mhis(hitby));
                continue; /* not paying attention */
            }
            if (invisible(hitby) && !see_invisible(hitmon)) {
                if (self && vis)
                    pline(combat_msgc(hitby, hitmon, cr_immune),
                          "%s doesn't notice its' reflection.",
                          Monnam(hitmon));
                continue; /* can't see this monster */
            }
            if (!cancelled(hitby) && attacktype(hitby->data, AT_GAZE)) {
                /* gaze attacks */
                if (reflecting(hitmon)) {
                    const char *reflbuf =
                        "%s gaze is reflected further by %%s %%s!";
                    reflbuf = msgcat(reflbuf, s_suffix(Monnam(hitby)));
                    if (vis)
                        mon_reflects(hitmon, hitby, FALSE, reflbuf,
                                     self ? mhis(hitmon) :
                                     s_suffix(mon_nam(hitmon)));
                    break;
                }
                /* TODO: don't special case monsters */
                if (hitby->data == &mons[PM_MEDUSA]) {
                    if (resists_ston(hitmon) && !self) {
                        if (vis)
                            pline(combat_msgc(hitby, hitmon, cr_immune),
                                  "%s isn't petrified.",
                                  Monnam(hitmon));
                    } else if (!resists_ston(hitmon))
                        minstapetrify(&youmonst, hitmon);
                    else { /* Medusa */
                        pline(msgc_kill, "%s turned to stone!",
                              M_verbs(hitmon, "are"));
                        monstone(hitmon);
                    }
                    /* not hitby -- that would credit hitby with the kill */
                    break;
                }
                if (hitby->data == &mons[PM_FLOATING_EYE]) {
                    int tmp = dice((int)hitmon->m_lev, (int)hitmon->data->mattk[0].damd);

                    if (!rn2(4))
                        tmp = 120;
                    if (vis)
                        pline(msgc_combatgood, "%s is frozen by %s reflection.",
                              Monnam(hitmon), self ? "its" : mon_nam(hitby));
                    else
                        You_hear(msgc_combatgood, "something stop moving.");
                    hitmon->mcanmove = 0;
                    if ((int)hitmon->mfrozen + tmp > 127)
                        hitmon->mfrozen = 127;
                    else
                        hitmon->mfrozen += tmp;
                    break;
                }
                if (hitby->data == &mons[PM_UMBER_HULK]) {
                    set_property(hitmon, CONFUSION, dice(3, 8), FALSE);
                    break;
                }
            }
            if (!self)
                continue; /* the rest of the things only affect self-reflects */
            if (!cancelled(hitmon) &&
                ((hitmon->data->mlet == S_NYMPH ||
                  hitmon->data == &mons[PM_INCUBUS]) &&
                 hitmon->female)) {
                if (vis) {
                    pline(msgc_actionok, "%s admires herself in your %s.",
                          Monnam(hitmon), simple_typename(obj->otyp));
                    pline_implied(msgc_substitute, "She takes it!");
                } else
                    pline(msgc_substitute, "It steals your %s!",
                          simple_typename(obj->otyp));
                setnotworn(obj);        /* in case mirror was wielded */
                freeinv(obj);
                mpickobj(hitmon, obj, NULL);
                if (!tele_restrict(hitmon))
                    rloc(hitmon, TRUE);
                return 1; /* someone snatched the mirror, so no more reflections */
            }
            if (!is_unicorn(hitmon->data) && !humanoid(hitmon->data) &&
                (!invisible(hitmon) || see_invisible(hitmon))) {
                if (vis)
                    pline(msgc_combatgood, "%s is frightened by its reflection.",
                          Monnam(hitmon));
                monflee(hitmon, dice(2, 4), FALSE, FALSE);
                break;
            } else if (vis) {
                pline(msgc_failrandom, "%s ignores %s reflection.",
                      Monnam(hitmon), mhis(hitmon));
                break;
            }
        }
    }
    return 1;
}

static void
use_bell(struct obj **optr)
{
    struct obj *obj = *optr;
    struct monst *mtmp;
    boolean wakem = FALSE, learno = FALSE;
    boolean ordinary = obj->otyp != BELL_OF_OPENING || !obj->spe;
    boolean invoking = obj->otyp == BELL_OF_OPENING &&
        invocation_pos(&u.uz, u.ux, u.uy) && !On_stairs(u.ux, u.uy);

    pline(msgc_occstart, "You ring %s.", the(xname(obj)));

    if (Underwater || (Engulfed && ordinary)) {
        pline(msgc_cancelled1, "But the sound is muffled.");

    } else if (invoking && ordinary) {
        /* needs to be recharged... */
        pline(obj->known ? msgc_cancelled1 : msgc_failcurse,
              "But it makes no sound.");
        learno = TRUE;  /* help player figure out why */

    } else if (ordinary) {
        if (obj->cursed && !rn2(4) &&
            /* note: once any of them are gone, we stop all of them */
            !(mvitals[PM_WOOD_NYMPH].mvflags & G_GONE) &&
            !(mvitals[PM_WATER_NYMPH].mvflags & G_GONE) &&
            !(mvitals[PM_MOUNTAIN_NYMPH].mvflags & G_GONE) &&
            ((mtmp = makemon(mkclass(&u.uz, S_NYMPH, 0, rng_main), level,
                             u.ux, u.uy, NO_MINVENT)))) {
            pline(msgc_substitute, "You summon %s!", a_monnam(mtmp));
            if (!obj_resists(obj, 93, 100)) {
                pline(msgc_substitute, "%s shattered!", Tobjnam(obj, "have"));
                useup(obj);
                *optr = 0;
            } else
                switch (rn2(3)) {
                default:
                    break;
                case 1:
                    set_property(mtmp, FAST, 0, TRUE);
                    break;
                case 2:
                    pline(msgc_statusbad,
                          "You freeze for a moment in surprise.");
                    helpless(rnd(2), hr_afraid, "surprised by a nymph", NULL);
                    break;
                }
        }
        wakem = TRUE;

    } else {
        /* charged Bell of Opening */
        consume_obj_charge(obj, TRUE);

        if (Engulfed) {
            if (!obj->cursed)
                openit();
            else {
                pline(obj->bknown ? msgc_cancelled1 : msgc_failcurse,
                      "Nothing happens.");
                obj->bknown = TRUE;
            }

        } else if (obj->cursed) {
            coord mm;

            mm.x = u.ux;
            mm.y = u.uy;
            mkundead(level, &mm, FALSE, NO_MINVENT);
            wakem = TRUE;

        } else if (invoking) {
            pline(msgc_actionok, "%s an unsettling shrill sound...",
                  Tobjnam(obj, "issue"));
            obj->age = moves;
            learno = TRUE;
            wakem = TRUE;

        } else if (obj->blessed) {
            int res = 0;

            if (uchain) {
                unpunish();
                res = 1;
            }
            res += openit();
            switch (res) {
            case 0: /* no targets */
                pline(msgc_notarget, "Nothing happens.");
                break;
            case 1: /* one target */
                pline(msgc_actionok, "Something opens...");
                learno = TRUE;
                break;
            default:
                pline(msgc_actionok, "Things open around you...");
                learno = TRUE;
                break;
            }

        } else {        /* uncursed */
            if (findit(BOLT_LIM) != 0)
                learno = TRUE;
            else
                pline(msgc_notarget, "Nothing happens."); /* no targets */
        }

    }   /* charged BofO */

    if (learno) {
        tell_discovery(obj);
        obj->known = 1;
    }
    if (wakem)
        wake_nearby(TRUE);
}

static int
use_candelabrum(struct obj *obj)
{
    const char *s = (obj->spe != 1) ? "candles" : "candle";

    if (Underwater) {
        pline(msgc_cancelled, "You cannot make fire under water.");
        return 0;
    }
    if (obj->lamplit) {
        pline(msgc_actionok, "You snuff the %s.", s);
        end_burn(obj, TRUE);
        return 1;
    }
    if (obj->spe <= 0) {
        pline(msgc_cancelled1, "This %s has no %s.", xname(obj), s);
        return 1;
    }
    if (Engulfed || obj->cursed) {
        enum msg_channel msgc = Engulfed ? msgc_cancelled1 :
            obj->bknown ? msgc_cancelled1 : msgc_failcurse;
        if (Blind)
            pline(msgc, "The %s %s seem to have lit.", s,
                  obj->spe > 1L ? "don't" : "doesn't");
        else
            pline(msgc, "The %s %s for a moment, then %s.", s,
                  vtense(s, "flicker"), vtense(s, "die"));
        obj->bknown = TRUE;
        return 1;
    }
    if (obj->spe < 7) {
        pline(msgc_hint, "There %s only %d %s in %s.", vtense(s, "are"),
              obj->spe, s, the(xname(obj)));
        if (!Blind)
            pline_implied(msgc_substitute, "%s lit.  %s dimly.",
                          obj->spe == 1 ? "It is" : "They are",
                          Tobjnam(obj, "shine"));
    } else {
        pline(msgc_actionok, "%s's %s burn%s", The(xname(obj)), s,
              (Blind ? "." : " brightly!"));
    }
    if (!Blind)
        obj->bknown = TRUE;

    if (!invocation_pos(&u.uz, u.ux, u.uy) || On_stairs(u.ux, u.uy)) {
        pline_implied(msgc_hint, "The %s %s being rapidly consumed!", s,
                      vtense(s, "are"));
        obj->age = (obj->age + 1) / 2;
    } else {
        if (obj->spe == 7) {
            if (Blind)
                pline_implied(msgc_hint, "%s a strange warmth!",
                              Tobjnam(obj, "radiate"));
            else
                pline_implied(msgc_hint, "%s with a strange light!",
                              Tobjnam(obj, "glow"));
        }
        obj->known = 1;
    }
    begin_burn(obj, FALSE);
    return 1;
}

static int
use_candle(struct obj **optr)
{
    struct obj *obj = *optr;
    struct obj *otmp;
    const char *s;
    const char *qbuf;

    if (Engulfed) {
        pline(msgc_cancelled, no_elbow_room);
        return 0;
    }

    otmp = carrying(CANDELABRUM_OF_INVOCATION);
    if (!otmp || otmp->spe == 7) {
        if (Underwater) {
            pline(msgc_cancelled, "Sorry, fire and water don't mix.");
            return 0;
        }
        return use_lamp(obj);
    }

    qbuf = msgprintf("Attach %s", the(xname(obj)));
    qbuf = msgprintf("%s to %s?", qbuf,
            safe_qbuf(qbuf, sizeof (" to ?"), the(xname(otmp)),
                      the(simple_typename(otmp->otyp)), "it"));

    if (yn(qbuf) == 'n') {
        if (Underwater) {
            pline(msgc_cancelled, "Sorry, fire and water don't mix.");
            return 0;
        }
        if (!obj->lamplit)
            pline(msgc_occstart, "You try to light %s...", the(xname(obj)));
        return use_lamp(obj);
    } else {
        if ((long)otmp->spe + obj->quan > 7L)
            obj = splitobj(obj, 7L - (long)otmp->spe);
        else
            *optr = 0;
        s = (obj->quan != 1) ? "candles" : "candle";
        pline(msgc_actionok, "You attach %d%s %s to %s.", (int)obj->quan,
              !otmp->spe ? "" : " more", s, the(xname(otmp)));
        if (!otmp->spe || otmp->age > obj->age)
            otmp->age = obj->age;
        otmp->spe += (int)obj->quan;
        if (otmp->lamplit && !obj->lamplit)
            pline_implied(msgc_consequence, "The new %s magically %s!", s,
                          vtense(s, "ignite"));
        else if (!otmp->lamplit && obj->lamplit)
            pline_implied(msgc_consequence, "%s out.",
                          (obj->quan > 1L) ? "They go" : "It goes");
        if (obj->unpaid)
            verbalize(msgc_unpaid, "You %s %s, you bought %s!",
                      otmp->lamplit ? "burn" : "use",
                      (obj->quan > 1L) ? "them" : "it",
                      (obj->quan > 1L) ? "them" : "it");
        if (obj->quan < 7L && otmp->spe == 7)
            pline_implied(msgc_hint, "%s now has seven%s candles attached.",
                          The(xname(otmp)), otmp->lamplit ? " lit" : "");
        /* candelabrum's light range might increase */
        if (otmp->lamplit)
            obj_merge_light_sources(otmp, otmp);
        /* candles are no longer a separate light source */
        if (obj->lamplit)
            end_burn(obj, TRUE);
        /* candles are now gone */
        useupall(obj);
        otmp->owt = weight(otmp);
        /* no encumber_msg: weight on player didn't change */
    }
    return 1;
}

boolean
snuff_candle(struct obj *otmp)
{       /* call in drop, throw, and put in box, etc. */
    if (!otmp) {
        impossible("snuffing null object");
        return FALSE;
    }
    boolean candle = Is_candle(otmp);

    if ((candle || otmp->otyp == CANDELABRUM_OF_INVOCATION) && otmp->lamplit) {
        xchar x, y;
        boolean many = candle ? otmp->quan > 1L : otmp->spe > 1;

        get_obj_location(otmp, &x, &y, 0);
        if (otmp->where == OBJ_MINVENT ? cansee(x, y) : !Blind) {
            pline(msgc_consequence, "%s %scandle%s flame%s extinguished.",
                  Shk_Your(otmp), (candle ? "" : "candelabrum's "),
                  (many ? "s'" : "'s"), (many ? "s are" : " is"));
        }
        end_burn(otmp, TRUE);
        return TRUE;
    }
    return FALSE;
}

/* called when lit lamp is hit by water or put into a container or
   you've been swallowed by a monster; obj might be in transit while
   being thrown or dropped so don't assume that its location is valid */
boolean
snuff_lit(struct obj * obj)
{
    if (!obj) {
        impossible("snuffing null object");
        return FALSE;
    }

    xchar x, y;

    if (obj->lamplit) {
        if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
            obj->otyp == BRASS_LANTERN || obj->otyp == POT_OIL) {
            get_obj_location(obj, &x, &y, 0);
            if (obj->where == OBJ_MINVENT ? cansee(x, y) : !Blind)
                pline(msgc_consequence, "%s %s out!", Yname2(obj),
                      otense(obj, "go"));
            end_burn(obj, TRUE);
            return TRUE;
        }
        if (snuff_candle(obj))
            return TRUE;
    }
    return FALSE;
}

/* Called when potentially lightable object is affected by fire_damage().
   Return TRUE if object was lit and FALSE otherwise --ALI */
boolean
catch_lit(struct obj * obj)
{
    xchar x, y;

    if (!obj->lamplit && (obj->otyp == MAGIC_LAMP || ignitable(obj))) {
        if ((obj->otyp == MAGIC_LAMP || obj->otyp == CANDELABRUM_OF_INVOCATION)
            && obj->spe == 0)
            return FALSE;
        else if (obj->otyp != MAGIC_LAMP && obj->age == 0)
            return FALSE;
        if (!get_obj_location(obj, &x, &y, 0))
            return FALSE;
        if (obj->otyp == CANDELABRUM_OF_INVOCATION && obj->cursed)
            return FALSE;
        if ((obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
             obj->otyp == BRASS_LANTERN) && obj->cursed && !rn2(2))
            return FALSE;
        if (obj->where == OBJ_MINVENT ? cansee(x, y) : !Blind)
            pline(msgc_consequence, "%s %s light!", Yname2(obj),
                  otense(obj, "catch"));
        if (obj->otyp == POT_OIL)
            tell_discovery(obj);
        if (obj->unpaid && costly_spot(u.ux, u.uy) &&
            obj->where == OBJ_INVENT) {
            /* if it catches while you have it, then it's your tough luck */
            check_unpaid(obj);
            verbalize(msgc_unpaid,
                      "That's in addition to the cost of %s %s, of course.",
                      Yname2(obj), obj->quan == 1 ? "itself" : "themselves");
            bill_dummy_object(obj);
        }
        begin_burn(obj, FALSE);
        return TRUE;
    }
    return FALSE;
}

static int
use_lamp(struct obj *obj)
{
    if (Underwater) {
        pline(msgc_cancelled, "This is not a diving lamp.");
        return 0;
    }
    if (obj->lamplit) {
        if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
            obj->otyp == BRASS_LANTERN)
            pline(msgc_actionok, "%s lamp is now off.", Shk_Your(obj));
        else
            pline(msgc_actionok, "You snuff out %s.", yname(obj));
        end_burn(obj, TRUE);
        /* For monsters using light sources, the player is going to want to
           turn them off, and that can cause a whole load of items stacking up
           if we don't stack them here. */
        if (obj->where == OBJ_INVENT) {
            struct obj *otmp;

            /* This code is based on the code from doorganise. */
            extract_nobj(obj, &youmonst.minvent,
                         &turnstate.floating_objects, OBJ_FREE);
            for (otmp = youmonst.minvent; otmp;) {
                if (merged(&otmp, &obj)) {
                    obj = otmp;
                    otmp = otmp->nobj;
                    extract_nobj(obj, &youmonst.minvent,
                                 &turnstate.floating_objects, OBJ_FREE);
                } else
                    otmp = otmp->nobj;
            }
            extract_nobj(obj, &turnstate.floating_objects,
                         &youmonst.minvent, OBJ_INVENT);
            reorder_invent();
            update_inventory();
        }
        return 1;
    }
    /* magic lamps with an spe == 0 (wished for) cannot be lit */
    if ((!Is_candle(obj) && obj->age == 0)
        || (obj->otyp == MAGIC_LAMP && obj->spe == 0)) {
        if (obj->otyp == BRASS_LANTERN)
            pline(obj->known ? msgc_cancelled1 : msgc_failcurse,
                  "Your lamp has run out of power.");
        else
            pline(obj->known ? msgc_cancelled1 : msgc_failcurse,
                  "This %s has no oil.", xname(obj));
        obj->known = TRUE;
        return 1;
    }
    if (obj->cursed && !rn2(2)) {
        if (Blind)
            pline(obj->bknown ? msgc_failrandom : msgc_failcurse,
                  "%s %s seem to have lit.", Yname2(obj),
                  obj->quan > 1L ? "don't" : "doesn't");
        else {
            pline(obj->bknown ? msgc_failrandom : msgc_failcurse,
                  "%s for a moment, then %s.",
                  Tobjnam(obj, "flicker"), otense(obj, "die"));
            obj->bknown = TRUE;
        }
    } else {
        if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
            obj->otyp == BRASS_LANTERN) {
            check_unpaid(obj);
            pline(msgc_actionok, "%s lamp is now on.", Shk_Your(obj));
        } else {        /* candle(s) */
            pline(msgc_actionok, "%s flame%s %s%s", s_suffix(Yname2(obj)),
                  plur(obj->quan), otense(obj, "burn"),
                  Blind ? "." : " brightly!");
            if (obj->unpaid && costly_spot(u.ux, u.uy) &&
                obj->age == 20L * (long)objects[obj->otyp].oc_cost) {
                const char *ithem = obj->quan > 1L ? "them" : "it";

                verbalize(msgc_unpaid, "You burn %s, you bought %s!",
                          ithem, ithem);
                bill_dummy_object(obj);
            }
        }

        if (!Blind)
            obj->bknown = TRUE;

        begin_burn(obj, FALSE);
    }
    return 1;
}

static int
light_cocktail(struct obj *obj)
{       /* obj is a potion of oil */
    if (Engulfed) {
        pline(msgc_cancelled, no_elbow_room);
        return 0;
    }

    if (obj->lamplit) {
        pline(msgc_actionok, "You snuff the lit potion.");
        end_burn(obj, TRUE);
        /* Free and add to re-merge the potion. This will average the age of the
           potions.  Not exactly the best solution, but it's easy. */
        if (obj->owornmask == 0) {
            freeinv(obj);
            addinv(obj);
        }
        return 1;
    } else if (Underwater) {
        pline(msgc_cancelled, "There is not enough oxygen to sustain a fire.");
        return 0;
    }

    pline(msgc_actionok, "You light %s potion.%s", shk_your(obj),
          Blind ? "" : "  It gives off a dim light.");
    if (!Blind && obj->blessed)
        obj->bknown = TRUE;

    if (obj->unpaid && costly_spot(u.ux, u.uy)) {
        /* Normally, we shouldn't both partially and fully charge for an item,
           but (Yendorian Fuel) Taxes are inevitable... */
        check_unpaid(obj);
        verbalize(msgc_unpaid,
                  "That's in addition to the cost of the potion, of course.");
        bill_dummy_object(obj);
    }
    tell_discovery(obj);

    if (obj->quan > 1L) {
        obj = splitobj(obj, 1L);
        begin_burn(obj, FALSE); /* burn before free to get position */
        obj_extract_self(obj);  /* free from inv */

        /* shouldn't merge */
        hold_another_object(obj, "You drop %s!", doname(obj), NULL);
    } else
        begin_burn(obj, FALSE);

    return 1;
}

static const char cuddly[] = { TOOL_CLASS, GEM_CLASS, 0 };

int
dorub(const struct nh_cmd_arg *arg)
{
    struct obj *obj;

    obj = getargobj(arg, cuddly, "rub");
    if (!obj)
        return 0;

    if (obj->oclass == GEM_CLASS) {
        if (is_graystone(obj)) {
            return use_stone(obj);
        } else {
            pline(msgc_mispaste, "Sorry, I don't know how to use that.");
            return 0;
        }
    }

    /* TODO: Why are these the only three items that need wielding to rub */
    if (obj->otyp == MAGIC_LAMP || obj->otyp == OIL_LAMP ||
        obj->otyp == BRASS_LANTERN) {
        int wtstatus = wield_tool(
            obj, obj->otyp == BRASS_LANTERN ?
            "preparing to rub your lantern" : "preparing to rub your lamp",
            occ_prepare, FALSE);
        if (wtstatus & 2)
            return 1;
        if (!(wtstatus & 1))
            return 0;
    }

    if (obj->otyp == MAGIC_LAMP) {
        if (uwep->spe > 0 && !rn2(3)) {
            check_unpaid_usage(uwep, TRUE);     /* unusual item use */
            makeknown(MAGIC_LAMP);
            uwep->otyp = OIL_LAMP;
            uwep->spe = 0;      /* for safety */
            uwep->age = rn1(500, 1000);
            if (uwep->lamplit)
                begin_burn(uwep, TRUE);
            djinni_from_bottle(&youmonst, uwep);
            update_inventory();
        } else if (rn2(2) && !Blind) {
            pline(msgc_failrandom, "You see a puff of smoke.");
            tell_discovery(obj);
        } else
            pline(msgc_failrandom, "Nothing happens.");
    } else if (obj->otyp == BRASS_LANTERN) {
        /* message from Adventure */
        pline(msgc_yafm,
              "Rubbing the electric lamp is not particularly rewarding.");
        pline_implied(msgc_yafm, "Anyway, nothing exciting happens.");
    } else /* TODO: check if known non-magical and change message channel */
        pline(msgc_failrandom, "Nothing happens.");
    return 1;
}

int
dojump(const struct nh_cmd_arg *arg)
{
    struct musable m = arg_to_musable(arg);

    /* Physical jump */
    return jump(&m, FALSE);
}

/* Returns 0 if we can't jump, 1 if we can, 2 if our attempt ends the turn
   without actually performing a jump. If checking is TRUE, we're only checking
   if we can jump in first place. */
int
validate_jump(const struct musable *m, coord *cc, boolean magic,
              boolean checking)
{
    struct monst *mon = m->mon;
    boolean you = (mon == &youmonst);
    /* Used to point at the right monster for wounded legs */
    struct monst *maybe_steed = you && u.usteed ? u.usteed : mon;
    boolean show = !checking && (you || canseemon(mon));
    int trinsic = jumps(mon);
    int ox = m_mx(mon);
    int oy = m_my(mon);

    if (!magic && (nolimbs(mon->data) || slithy(mon->data))) {
        bpline(msgc_cancelled, show, "You can't jump; you have no legs!");
        return 0;
    } else if (!magic && !trinsic) {
        bpline(msgc_cancelled, show, "You can't jump very far.");
        return 0;
    } else if (you && Engulfed) {
        bpline(msgc_cancelled, show, "You've got to be kidding!!");
        return 0;
    } else if (m_underwater(mon)) {
        bpline(msgc_cancelled, show, "This calls for swimming, not jumping!");
        return 0;
    } else if (mon == u.ustuck || (you && u.ustuck)) {
        if (u.ustuck->mtame && !Conflict && !confused(u.ustuck)) {
            if (!checking) {
                bpline(msgc_actionok, show, "%s free from %s.",
                       M_verbs(mon, "pull"), !you ? "you" : mon_nam(u.ustuck));
                u.ustuck = 0;
                return 2;
            }
        } else {
            /* TODO: this spoils something (no conflict/pet confusion) under
               some circumstances, which shouldn't be the case for
               msgc_cancelled */
            bpline(msgc_cancelled, show, "You cannot escape from %s!",
                   mon_nam(u.ustuck));
            return 0;
        }
    } else if (levitates(mon) || Is_airlevel(m_mz(mon)) ||
               Is_waterlevel(m_mz(mon))) {
        bpline(msgc_cancelled, show, "You don't have enough traction to jump.");
        return 0;
    } else if (!magic && you && near_capacity() > UNENCUMBERED) {
        bpline(msgc_cancelled, show, "You are carrying too much to jump!");
        return 0;
    } else if (!magic && you && (u.uhunger <= 100 || ACURR(A_STR) < 6)) {
        bpline(msgc_cancelled, show, "You lack the strength to jump!");
        return 0;
    } else if (leg_hurt(maybe_steed)) {
        const char *bp = body_part(LEG);

        if (leg_hurtl(maybe_steed) && leg_hurtr(maybe_steed))
            bp = makeplural(bp);
        if (u.usteed)
            bpline(msgc_cancelled, show, "%s is in no shape for jumping.",
                   Monnam(u.usteed));
        else
            bpline(msgc_cancelled, show,
                   "Your %s%s %s in no shape for jumping.",
                   (!leg_hurtr(maybe_steed)) ? "left " :
                   (!leg_hurtl(maybe_steed)) ? "right " : "",
                   bp, (leg_hurtl(maybe_steed) &&
                        leg_hurtr(maybe_steed)) ? "are" : "is");
        return 0;
    } else if (you && u.usteed && u.utrap) {
        bpline(msgc_cancelled, show, "%s is stuck in a trap.",
               Monnam(u.usteed));
        return 0;
    } else if (you && u.usteed && !u.usteed->mcanmove) {
        bpline(msgc_cancelled, show,
               "%s won't move sideways, much less upwards.",
               Monnam(u.usteed));
        return 0;
    }

    if (!checking) {
        if (you)
            bpline(msgc_uiprompt, show, "Where do you want to jump?");
        cc->x = ox;
        cc->y = oy;
        if (mgetargpos(m, cc, FALSE,
                       "the desired position") == NHCR_CLIENT_CANCEL)
            return 0;       /* user pressed ESC */
    }

    if (!isok(cc->x, cc->y)) {
        bpline(msgc_cancelled, show, "You cannot jump there!");
        return 0;
    } else if (!magic && !(trinsic & EXTRINSIC) &&
               dist2(ox, oy, cc->x, cc->y) != 5) {
        bpline(dist2(ox, oy, cc->x, cc->y) > 20 ? msgc_mispaste :
               msgc_cancelled, show, "Illegal move!");
        return 0;
    }

    /* Figure out allowed distance */
    int skill = MP_SKILL(mon, spell_skilltype(SPE_JUMPING));
    if (skill < P_UNSKILLED)
        skill = P_UNSKILLED;

    int dist = 9;
    if (magic)
        dist = 6 + skill * 3;

    if (dist2(ox, oy, cc->x, cc->y) > dist) {
        bpline(dist2(ox, oy, cc->x, cc->y) > 20 ? msgc_mispaste :
               msgc_cancelled, show, "Too far!");
        return 0;
    } else if (!m_cansee(mon, cc->x, cc->y)) {
        bpline(msgc_cancelled, show, "You cannot see where to land!");
        return 0;
    }

    if (you ? u.utrap : mon->mtrapped) {
        long side = rn2(3) ? LEFT_SIDE : RIGHT_SIDE;
        unsigned traptype;
        struct trap *t = t_at(m_dlevel(mon), ox, oy);
        traptype = (t->ttyp == PIT || t->ttyp == SPIKED_PIT ? TT_PIT :
                    t->ttyp == BEAR_TRAP ? TT_BEARTRAP :
                    t->ttyp == WEB ? TT_WEB : 0);
        if (you)
            traptype = u.utraptype;

        switch (traptype) {
        case TT_BEARTRAP:
            bpline(you ? msgc_badidea : msgc_monneutral, show,
                   "%s %sself free of the bear trap!  Ouch!",
                   M_verbs(mon, "rip"), mhim(mon));

            if (checking)
                return 1;

            if (maybe_steed == &youmonst)
                losehp(rnd(10),
                       killer_msg(DIED, "jumping out of a bear trap"));
            else {
                maybe_steed->mhp -= rnd(10);
                if (maybe_steed->mhp <= 0) {
                    if (you)
                        killed(maybe_steed);
                    else
                        mondied(maybe_steed);
                    return 2;
                }
            }

            set_wounded_legs(u.usteed ? u.usteed : &youmonst,
                             side, rn1(1000, 500));
            break;
        case TT_PIT:
            bpline(msgc_actionok, show,  "You leap from the pit!");
            break;
        case TT_WEB:
            bpline(msgc_actionok, show,
                   "%s the web apart as %s pull %sself free!",
                   M_verbs(mon, "tear"), mhe(mon), mhim(mon));
            if (!checking)
                deltrap(level, t_at(level, u.ux, u.uy));
            break;
        case TT_LAVA:
            bpline(msgc_actionok, show, "%s %sself above the lava!",
                   M_verbs(mon, "pull"), mhim(mon));
            if (!checking) {
                if (you)
                    u.utrap = 0;
                else
                    mon->mtrapped = 0;
            }
            return 2;
        case TT_INFLOOR:
            bpline(you ? msgc_badidea : msgc_monneutral, show,
                   "%s %s %s, but %s still stuck in the floor.",
                   M_verbs(mon, "strain"), mhis(mon),
                   makeplural(mbodypart(mon, LEG)),
                   m_verbs(mon, "are"));
            set_wounded_legs(mon, LEFT_SIDE, rn1(10, 11));
            set_wounded_legs(mon, RIGHT_SIDE, rn1(10, 11));
            return 2;
        }
    }

    return 1;
}

void
jump_to_coords(struct monst *mon, coord *cc)
{
    coord uc;
    int range, temp;

    /*
     * Check the path from uc to cc, calling hurtle_step at each
     * location.  The final position actually reached will be
     * in cc.
     */
    uc.x = m_mx(mon);
    uc.y = m_my(mon);
    /* calculate max(abs(dx), abs(dy)) as the range */
    range = cc->x - uc.x;
    if (range < 0)
        range = -range;
    temp = cc->y - uc.y;
    if (temp < 0)
        temp = -temp;
    if (range < temp)
        range = temp;
    if (mon == &youmonst) {
        walk_path(&uc, cc, hurtle_step, &range);
        teleds(cc->x, cc->y, TRUE);
        helpless(1, hr_moving, "jumping around", NULL);
        morehungry(rnd(25));
    } else {
        walk_path(&uc, cc, mhurtle_step, mon);
        mon->mfrozen = 1;
        mon->mcanmove = 0;
        mintrap(mon);
    }
}

int
jump(const struct musable *m, boolean magic)
{
    coord cc;

    /* Get the coordinates.  This might involve aborting. */
    int res = validate_jump(m, &cc, magic, FALSE);
    if (res != 1)
        return !!res;

    /* Now do the actual jumping. */
    jump_to_coords(m->mon, &cc);
    return 1;
}

boolean
tinnable(const struct obj * corpse)
{
    if (!(corpse->otyp == CORPSE)) /* Originally the caller was expected */
        return 0;                  /* to check this, but that caused bugs. */
    if (corpse->oeaten)
        return 0;
    if (!mons[corpse->corpsenm].cnutrit)
        return 0;
    return 1;
}

static int
use_tinning_kit(struct obj *obj)
{
    struct obj *corpse, *can;

    /* This takes only 1 move.  If this is to be changed to take many moves,
       we've got to deal with decaying corpses... */
    if (obj->spe <= 0) {
        pline(obj->known ? msgc_cancelled1 : msgc_failcurse,
              "You seem to be out of tins.");
        obj->known = TRUE;
        return 1;
    }

    /* We can't reuse an argument here, because the argument is the tinning
       kit, not the item being tinned. We signal this to floorfood using a NULL
       argument pointer. */
    if (!(corpse = floorfood("tin", NULL)))
        return 0;
    if (corpse->oeaten) {
        pline(msgc_cancelled,
              "You cannot tin something which is partly eaten.");
        return 0;
    }
    if (touch_petrifies(&mons[corpse->corpsenm])
        && !resists_ston(&youmonst) && !uarmg) {
        if (poly_when_stoned(youmonst.data))
            pline(msgc_consequence, "You tin %s without wearing gloves.",
                  an(opm_name(corpse)));
        else
            pline(msgc_badidea,
                  "Tinning %s without wearing gloves is a fatal mistake...",
                  an(opm_name(corpse)));

        instapetrify(killer_msg(STONING,
                                msgprintf("trying to tin %s without gloves",
                                          an(opm_name(corpse)))));
    }
    if (is_rider(&mons[corpse->corpsenm])) {
        revive_corpse(corpse);
        verbalize(msgc_badidea,
                  "Yes...  But War does not preserve its enemies...");
        return 1;
    }
    if (mons[corpse->corpsenm].cnutrit == 0) {
        pline(msgc_cancelled, "That's too insubstantial to tin.");
        return 0;
    }
    consume_obj_charge(obj, TRUE);

    if ((can = mksobj(level, TIN, FALSE, FALSE, rng_main)) != 0) {
        static const char you_buy_it[] = "You tin it, you bought it!";

        can->corpsenm = corpse->corpsenm;
        can->cursed = obj->cursed;
        can->blessed = obj->blessed;
        can->owt = weight(can);
        can->known = 1;
        can->spe = corpse->spe;
        can->spe |= OPM_HOMEMADE; /* Mark tinned tins. No spinach allowed... */
        if (carried(corpse)) {
            if (corpse->unpaid)
                verbalize(msgc_unpaid, you_buy_it);
            useup(corpse);
        } else {
            if (costly_spot(corpse->ox, corpse->oy) && !corpse->no_charge)
                verbalize(msgc_unpaid, you_buy_it);
            useupf(corpse, 1L);
        }
        hold_another_object(can, "You make, but cannot pick up, %s.",
                            doname(can), NULL);
    } else
        impossible("Tinning failed.");
    return 1;
}

/* Using an unicorn horn. A cursed one selects a random trouble out of
   Conf/Ill/Blind/Stun/Hallu to give for 1-100 turns. A blessed one will fix
   all these troubles, but has a 10% of unblessing. An uncursed one will select
   one at random to cure if the player has it. */
void
use_unicorn_horn(struct monst *mon, struct obj *obj)
{
    int trouble[] = {SICK, BLINDED, HALLUC, CONFUSION, STUNNED};
    int resist[] = {SICK_RES, 0, HALLUC_RES, 0, STUN_RES};
    int turns = rnd(100);
    boolean cursed = (obj && obj->cursed);
    boolean blessed = (obj && obj->blessed);
    int cure = rn2_on_rng(5, cursed ? rng_cursed_unihorn : rng_main);
    boolean res = FALSE;
    boolean any_trouble = FALSE;
    if (vomiting(mon) || zombifying(mon))
        any_trouble = TRUE;

    /* players often apply known-cursed unihorns out of habit because they
       haven't looked in inventory recently; ensure we have a
       msgc_substitute message so that the player can see they used a cursed
       object */
    if (cursed) {
        if (mon == &youmonst) {
            pline_once(msgc_substitute,
                       "Something is wrong with your unicorn horn.");
            obj->bknown = TRUE;
        } else
            obj->mbknown = TRUE;
    }

    int i;
    for (i = 0; i < SIZE(trouble); i++) {
        if (cursed && has_property(mon, resist[i]))
            continue;

        if (has_property(mon, trouble[i]))
            any_trouble = TRUE;

        if (!blessed && i != cure)
            continue;

        if (!i) {
            if (cursed) {
                make_sick(mon, (unsigned long)rn1(ACURR(A_CON), 20),
                          killer_xname(obj), TRUE, SICK_NONVOMITABLE);
                res = TRUE;
            } else {
                res |= set_property(mon, SICK, -2, FALSE);
                res |= set_property(mon, VOMITING, -2, FALSE);
                res |= set_property(mon, ZOMBIE, -2, FALSE);
            }
        } else {
            if (cursed)
                res |= inc_timeout(mon, trouble[i], turns, FALSE);
            else
                res |= set_property(mon, trouble[i], -2, FALSE);
        }
    }

    if (!res && mon == &youmonst) {
        if (any_trouble || cursed)
            pline(msgc_failrandom, "Nothing seems to happen.");
        else
            pline(msgc_actionok, "Nothing happens.");
    }

    if (blessed && !rn2_on_rng(10, mon == &youmonst ? rng_unbless_unihorn :
                               rng_main)) {
        if (!blind(mon)) {
            if (mon == &youmonst) {
                pline(msgc_itemloss, "%s %s %s.", Shk_Your(obj),
                      aobjnam(obj, "glow"), hcolor("brown"));
                obj->bknown = TRUE;
            } else
                obj->mbknown = TRUE;
        }
        unbless(obj);
    }
}

/*
 * Timer callback routine: turn figurine into monster
 */
void
fig_transform(void *arg, long timeout)
{
    struct obj *figurine = (struct obj *)arg;
    struct monst *mtmp;
    coord cc;
    boolean cansee_spot, silent, okay_spot;
    boolean redraw = FALSE;
    const char *monnambuf, *carriedby;

    if (!figurine)
        return;

    if (timeout < 0)
        silent = TRUE;
    else
        silent = ((unsigned long)timeout != moves);    /* happened while away */

    okay_spot = get_obj_location(figurine, &cc.x, &cc.y, 0);
    if (figurine->where == OBJ_INVENT || figurine->where == OBJ_MINVENT)
        okay_spot = enexto(&cc, level, cc.x, cc.y, &mons[figurine->corpsenm]);
    if (!okay_spot || !figurine_location_checks(figurine, &cc, TRUE)) {
        /* reset the timer to try again later */
        start_timer(figurine->olev, (long)rnd(5000), TIMER_OBJECT,
                    FIG_TRANSFORM, figurine);
        return;
    }

    cansee_spot = cansee(cc.x, cc.y);
    mtmp = make_familiar(&youmonst, figurine, cc.x, cc.y, TRUE);
    if (mtmp) {
        if (!(mtmp->data->mflags2 & (M2_MALE | M2_FEMALE)))
            mtmp->female = !!(figurine->spe & OPM_FEMALE);

        monnambuf = msgprintf("%s", an(m_monnam(mtmp)));
        switch (figurine->where) {
        case OBJ_INVENT:
            if (Blind)
                pline(msgc_youdiscover, "You feel something %s from your pack!",
                      locomotion(mtmp->data, "drop"));
            else
                pline(msgc_youdiscover, "You see %s %s out of your pack!",
                      monnambuf, locomotion(mtmp->data, "drop"));
            break;

        case OBJ_FLOOR:
            if (cansee_spot && !silent) {
                pline(msgc_youdiscover,
                      "You suddenly see a figurine transform into %s!",
                      monnambuf);
                redraw = TRUE;  /* update figurine's map location */
            }
            break;

        case OBJ_MINVENT:
            if (cansee_spot && !silent) {
                struct monst *mon;

                mon = figurine->ocarry;
                /* Figurine carring monster might be invisible; or due to
                   polymorph, the monster might not have a backpack, which means
                   that the message needs serious rephrasing */
                if (notake(mon->data) && canseemon(mon))
                    pline(msgc_youdiscover, "You see %s appear near %s!",
                          monnambuf, a_monnam(mon));
                else {
                    if (canseemon(mon))
                        carriedby = msgprintf("%s pack",
                                              s_suffix(a_monnam(mon)));
                    else if (is_pool(level, mon->mx, mon->my))
                        carriedby = "empty water";
                    else
                        carriedby = "thin air";
                    pline(msgc_youdiscover, "You see %s %s out of %s!",
                          monnambuf, locomotion(mtmp->data, "drop"), carriedby);
                }
            }
            break;

        default:
            impossible("figurine came to life where? (%d)",
                       (int)figurine->where);
            break;
        }
    }
    /* free figurine now */
    obj_extract_self(figurine);
    obfree(figurine, NULL);
    if (redraw)
        newsym(cc.x, cc.y);
}


static boolean
figurine_location_checks(struct obj *obj, coord * cc, boolean quietly)
{
    xchar x, y;

    if (carried(obj) && Engulfed) {
        if (!quietly)
            pline(msgc_cancelled, "You don't have enough room in here.");
        return FALSE;
    }
    if (!cc) {
        impossible("figurine not on coordinate?");
        return FALSE;
    }
    x = cc->x;
    y = cc->y;
    if (!isok(x, y)) {
        if (!quietly)
            pline(msgc_mispaste, "You cannot put the figurine there.");
        return FALSE;
    }
    if (IS_ROCK(level->locations[x][y].typ) &&
        !(pm_phasing(&mons[obj->corpsenm]) && may_passwall(level, x, y))) {
        if (!quietly)
            pline(msgc_mispaste, "You cannot place a figurine in %s!",
                  IS_TREE(level->
                          locations[x][y].typ) ? "a tree" : "solid rock");
        return FALSE;
    }
    if (sobj_at(BOULDER, level, x, y) && !pm_phasing(&mons[obj->corpsenm])
        && !throws_rocks(&mons[obj->corpsenm])) {
        if (!quietly)
            pline(msgc_cancelled,
                  "You cannot fit the figurine on the boulder.");
        return FALSE;
    }
    return TRUE;
}


static int
use_figurine(struct obj **objp, const struct nh_cmd_arg *arg)
{
    xchar x, y;
    coord cc;
    schar dx, dy, dz;
    struct obj *obj = *objp;

    if (Engulfed) {
        /* can't activate a figurine while swallowed */
        if (!figurine_location_checks(obj, NULL, FALSE))
            return 0;
    }
    if (!getargdir(arg, NULL, &dx, &dy, &dz)) {
        action_completed();
        return 0;
    }

    x = u.ux + dx;
    y = u.uy + dy;
    cc.x = x;
    cc.y = y;

    /* Passing FALSE arg here will result in messages displayed */
    if (!figurine_location_checks(obj, &cc, FALSE))
        return 0;
    pline(msgc_actionok, "You %s and it transforms.",
          (dx || dy) ? "set the figurine beside you" :
          (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
           is_pool(level, cc.x, cc.y)) ?
          "release the figurine" :
          (dz < 0 ? "toss the figurine into the air" :
           "set the figurine on the ground"));
    make_familiar(&youmonst, obj, cc.x, cc.y, FALSE);
    stop_timer(obj->olev, FIG_TRANSFORM, obj);
    useup(obj);
    *objp = NULL;
    return 1;
}

static const char lubricables[] = { ALL_CLASSES, ALLOW_NONE, 0 };

static const char need_to_remove_outer_armor[] =
    "You need to remove your %s to grease your %s.";

static int
use_grease(struct obj *obj)
{
    struct obj *otmp;
    int res = 0;

    if (slippery_fingers(&youmonst)) {
        pline(msgc_cancelled1, "%s from your %s.", Tobjnam(obj, "slip"),
              makeplural(body_part(FINGER)));
        unwield_silently(obj);
        dropx(obj);
        return 1;
    }

    if (obj->spe > 0) {
        if ((obj->cursed || Fumbling) && !rn2(2)) {
            consume_obj_charge(obj, TRUE);

            pline(msgc_substitute, "%s from your %s.", Tobjnam(obj, "slip"),
                  makeplural(body_part(FINGER)));
            unwield_silently(obj);
            dropx(obj);
            return 1;
        }
        otmp = getobj(lubricables, "grease", FALSE);
        if (!otmp)
            return 0;
        if ((otmp->owornmask & W_MASK(os_arm)) && uarmc) {
            pline(msgc_cancelled, need_to_remove_outer_armor,
                  xname(uarmc), xname(otmp));
            return 0;
        }
        if ((otmp->owornmask & W_MASK(os_armu)) && (uarmc || uarm)) {
            const char *buf;
            buf = uarmc ? xname(uarmc) : "";
            if (uarmc && uarm)
                buf = msgcat(buf, " and ");
            buf = msgcat(buf, uarm ? xname(uarm) : "");
            pline(msgc_cancelled, need_to_remove_outer_armor,
                  buf, xname(otmp));
            return 0;
        }
        consume_obj_charge(obj, TRUE);
        res = 1;

        if (otmp != &zeroobj) {
            pline(msgc_actionok, "You cover %s with a thick layer of grease.",
                  yname(otmp));
            otmp->greased = 1;
            if (obj->cursed && !nohands(youmonst.data)) {
                inc_timeout(&youmonst, GLIB, rnd(15), TRUE);
                pline(msgc_substitute,
                      "Some of the grease gets all over your %s.",
                      makeplural(body_part(HAND)));
            }
        } else {
            inc_timeout(&youmonst, GLIB, rnd(15), TRUE);
            pline(msgc_actionok, "You coat your %s with grease.",
                  makeplural(body_part(FINGER)));
        }
    } else {
        if (obj->known)
            pline(msgc_cancelled, "%s empty.", Tobjnam(obj, "are"));
        else /* TODO: Shouldn't this cost time? msgc_cancelled1 in reverse... */
            pline(msgc_cancelled, "%s to be empty.", Tobjnam(obj, "seem"));
    }
    update_inventory();
    return res;
}

/* touchstones - by Ken Arnold */
static int
use_stone(struct obj *tstone)
{
    struct obj *obj;
    boolean do_scratch;
    const char *streak_color, *choices;
    const char *stonebuf;
    static const char scritch[] = "\"scritch, scritch\"";
    static const char allowall[3] = { ALL_CLASSES, 0 };
    static const char justgems[3] = { ALL_CLASSES, GEM_CLASS, 0 };

    /* in case it was acquired while blinded */
    if (!Blind)
        tstone->dknown = 1;
    /* when the touchstone is fully known, don't bother listing extra junk as
       likely candidates for rubbing */
    choices = (tstone->otyp == TOUCHSTONE && tstone->dknown &&
               objects[TOUCHSTONE].oc_name_known) ? justgems : allowall;
    stonebuf = msgprintf("rub on the stone%s", plur(tstone->quan));
    if ((obj = getobj(choices, stonebuf, FALSE)) == 0)
        return 0;

    if (obj == tstone && obj->quan == 1) {
        pline(msgc_mispaste, "You can't rub %s on itself.", the(xname(obj)));
        return 0;
    }

    if (tstone->otyp == TOUCHSTONE && tstone->cursed && obj->oclass == GEM_CLASS
        && !is_graystone(obj) && !obj_resists(obj, 80, 100)) {
        if (Blind)
            pline(msgc_substitute, "You feel something shatter.");
        else if (Hallucination)
            pline(msgc_substitute, "Oh, wow, look at the pretty shards.");
        else
            pline(msgc_substitute, "A sharp crack shatters %s%s.",
                  (obj->quan > 1) ? "one of " : "", the(xname(obj)));
        useup(obj);
        return 1;
    }

    if (Blind) {
        pline(msgc_yafm, scritch);
        return 1;
    } else if (Hallucination) {
        pline(msgc_yafm, "Oh wow, man: Fractals!");
        return 1;
    }

    do_scratch = FALSE;
    streak_color = 0;
    boolean id_touchstone = FALSE;

    switch (obj->oclass) {
    case GEM_CLASS:    /* these have class-specific handling below */
    case RING_CLASS:
        if (tstone->otyp != TOUCHSTONE) {
            do_scratch = TRUE;
        } else if (obj->oclass == GEM_CLASS &&
                   (tstone->blessed ||
                    (!tstone->cursed &&
                     (Role_if(PM_ARCHEOLOGIST) || Race_if(PM_GNOME))))) {
            makeknown(obj->otyp);
            prinv(NULL, obj, 0L);
            tell_discovery(tstone);
            return 1;
        } else {
            /* either a ring or the touchstone was not effective */
            if (objects[obj->otyp].oc_material == GLASS) {
                do_scratch = TRUE;
                if (obj->oclass == GEM_CLASS)
                    id_touchstone = TRUE;
                break;
            }
        }
        streak_color = c_obj_colors[objects[obj->otyp].oc_color];
        break;  /* gem or ring */

    default:
        switch (objects[obj->otyp].oc_material) {
        case CLOTH:
            pline(msgc_yafm, "%s a little more polished now.",
                  Tobjnam(tstone, "look"));
            return 1;
        case LIQUID:
            if (!obj->known)    /* note: not "whetstone" */
                pline(msgc_yafm, "You must think this is a wetstone, do you?");
            else
                pline(msgc_yafm, "%s a little wetter now.",
                      Tobjnam(tstone, "are"));
            return 1;
        case WAX:
            streak_color = "waxy";
            break;      /* okay even if not touchstone */
        case WOOD:
            streak_color = "wooden";
            break;      /* okay even if not touchstone */
        case GOLD:
            do_scratch = TRUE;  /* scratching and streaks */
            streak_color = "golden";
            break;
        case SILVER:
            do_scratch = TRUE;  /* scratching and streaks */
            streak_color = "silvery";
            break;
        default:
            /* Objects passing the is_flimsy() test will not scratch a stone.
               They will leave streaks on non-touchstones and touchstones
               alike. */
            if (is_flimsy(obj))
                streak_color = c_obj_colors[objects[obj->otyp].oc_color];
            else
                do_scratch = (tstone->otyp != TOUCHSTONE);
            break;
        }
        break;  /* default oclass */
    }

    stonebuf = msgprintf("stone%s", plur(tstone->quan));
    if (do_scratch)
        pline(msgc_actionok, "You make %s%sscratch marks on the %s.",
              streak_color ? streak_color : (const char *)"",
              streak_color ? " " : "", stonebuf);
    else if (streak_color)
        pline(msgc_actionok, "You see %s streaks on the %s.",
              streak_color, stonebuf);
    else
        pline(msgc_actionok, scritch);
    if (id_touchstone)
        tell_discovery(tstone);
    return 1;
}

/* Place a landmine/bear trap.  Helge Hafting */
static int
use_trap(struct obj *otmp, const struct nh_cmd_arg *arg)
{
    int ttyp, tmp;
    const char *what = NULL;
    boolean mispasting = FALSE;
    const char *occutext = "setting the trap";

    if (nohands(youmonst.data))
        what = "without hands";
    else if (Stunned)
        what = "while stunned";
    else if (Engulfed)
        what = is_animal(u.ustuck->data) ? "while swallowed" : "while engulfed";
    else if (Underwater)
        what = "underwater";
    else if (is_pool(level, u.ux, u.uy))
        what = "in water", mispasting = 1;
    else if (levitates(&youmonst))
        what = "while levitating";
    else if (is_lava(level, u.ux, u.uy))
        what = "in lava", mispasting = 1;
    else if (On_stairs(u.ux, u.uy))
        what = (u.ux == level->dnladder.sx ||
                u.ux == level->upladder.sx) ? "on the ladder" : "on the stairs";
    else if (IS_FURNITURE(level->locations[u.ux][u.uy].typ) ||
             IS_ROCK(level->locations[u.ux][u.uy].typ) ||
             closed_door(level, u.ux, u.uy) || t_at(level, u.ux, u.uy))
        what = "here", mispasting = 1;
    if (what) {
        pline(mispasting ? msgc_mispaste : msgc_cancelled,
              "You can't set a trap %s!", what);
        u.utracked[tos_trap] = 0;
        return 0;
    }
    ttyp = (otmp->otyp == LAND_MINE) ? LANDMINE : BEAR_TRAP;
    makeknown(otmp->otyp);
    if (otmp == u.utracked[tos_trap] &&
        u.ux == u.utracked_location[tl_trap].x &&
        u.uy == u.utracked_location[tl_trap].y) {
        if (turnstate.continue_message)
            pline(msgc_occstart, "You resume setting %s %s.", shk_your(otmp),
                  trapexplain[what_trap(ttyp, u.ux, u.uy, rn2) - 1]);
        one_occupation_turn(set_trap, occutext, occ_trap);
        return 1;
    }
    u.utracked[tos_trap] = otmp;
    u.utracked_location[tl_trap].x = u.ux;
    u.utracked_location[tl_trap].y = u.uy;
    tmp = ACURR(A_DEX);
    u.uoccupation_progress[tos_trap] =
        (tmp > 17) ? 2 : (tmp > 12) ? 3 : (tmp > 7) ? 4 : 5;
    if (Blind)
        u.uoccupation_progress[tos_trap] *= 2;
    tmp = ACURR(A_STR);
    if (ttyp == BEAR_TRAP && tmp < 18)
        u.uoccupation_progress[tos_trap] += (tmp > 12) ? 1 : (tmp > 7) ? 2 : 4;
    /* [fumbling and/or confusion and/or cursed object check(s) should be
       incorporated here instead of in set_trap] */
    if (u.usteed && P_SKILL(P_RIDING) < P_BASIC) {
        boolean chance;
        const char *buf;

        if (Fumbling || otmp->cursed)
            chance = (rnl(10) > 3);
        else
            chance = (rnl(10) > 5);
        pline(msgc_uiprompt, "You aren't very skilled at reaching from %s.",
              mon_nam(u.usteed));
        buf = msgprintf("Continue your attempt to set %s?",
                        the(trapexplain[what_trap(ttyp, -1, -1, rn2) - 1]));
        if (yn(buf) == 'y') {
            if (chance) {
                switch (ttyp) {
                case LANDMINE:
                    /* TODO: Perhaps we should have an explosion here?
                       3.4.3 did, but the code is really ugly. */
                case BEAR_TRAP: /* drop it without arming it */
                    u.utracked[tos_trap] = 0;
                    pline(msgc_badidea, "You drop %s!",
                          the(trapexplain[what_trap(ttyp, -1, -1, rn2) - 1]));
                    unwield_silently(otmp);
                    dropx(otmp);
                    return 1;
                }
            }
        } else {
            u.utracked[tos_trap] = 0;
            return 0;
        }
    }

    pline(msgc_occstart, "You begin setting %s %s.", shk_your(otmp),
          trapexplain[what_trap(ttyp, -1, -1, rn2) - 1]);
    one_occupation_turn(set_trap, occutext, occ_trap);
    return 1;
}


static int
set_trap(void)
{
    struct obj *otmp = u.utracked[tos_trap];
    struct trap *ttmp;
    int ttyp;

    if (!otmp || !carried(otmp) ||
        u.ux != u.utracked_location[tl_trap].x ||
        u.uy != u.utracked_location[tl_trap].y) {
        u.utracked[tos_trap] = 0;
        return 0;
    }

    if (--u.uoccupation_progress[tos_trap] > 0)
        return 1;       /* still busy */

    ttyp = (otmp->otyp == LAND_MINE) ? LANDMINE : BEAR_TRAP;
    ttmp = maketrap(level, u.ux, u.uy, ttyp, rng_main);
    if (ttmp) {
        ttmp->tseen = 1;
        ttmp->madeby_u = 1;
        newsym(u.ux, u.uy);     /* if our hero happens to be invisible */
        if (*in_rooms(level, u.ux, u.uy, SHOPBASE)) {
            add_damage(u.ux, u.uy, 0L); /* schedule removal */
        }
        pline(msgc_actionok, "You finish arming %s.",
              the(trapexplain[what_trap(ttyp, u.ux, u.uy, rn2) - 1]));
        if ((otmp->cursed || Fumbling) && rnl(10) > 5)
            dotrap(ttmp, 0);
    } else {
        /* this shouldn't happen */
        impossible("Trap setting attempt failed?");
    }
    useup(otmp);
    u.utracked[tos_trap] = 0;
    return 0;
}


static int
use_whip(struct obj *obj, const struct nh_cmd_arg *arg)
{
    struct monst *mtmp;
    struct obj *otmp;
    int rx, ry, proficient;
    const char *msg_slipsfree = "The bullwhip slips free.";
    const char *msg_snap = "Snap!";
    schar dx, dy, dz;
    const char *buf;

    int wtstatus = wield_tool(obj, "preparing to lash your whip", occ_prepare, FALSE);
    if (wtstatus & 2)
        return 1;
    if (!(wtstatus & 1))
        return 0;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    if (Stunned || (Confusion && !rn2(5)))
        confdir(&dx, &dy);
    rx = u.ux + dx;
    ry = u.uy + dy;
    mtmp = (isok(rx, ry)) ? m_at(level, rx, ry) : NULL;

    /* fake some proficiency checks */
    proficient = 0;
    if (Role_if(PM_ARCHEOLOGIST))
        ++proficient;
    if (ACURR(A_DEX) < 6)
        proficient--;
    else if (ACURR(A_DEX) >= 14)
        proficient += (ACURR(A_DEX) - 14);
    if (Fumbling)
        --proficient;
    if (proficient > 3)
        proficient = 3;
    if (proficient < 0)
        proficient = 0;

    /* moving the bullwhip through a square reveals the presence or absence of a
       monster there; also alerts the monster that you know if it's there */
    if (!Engulfed)
        reveal_monster_at(rx, ry, 1);

    if (Engulfed) {
        enum attack_check_status attack_status =
            attack(u.ustuck, dx, dy, FALSE);
        return attack_status != ac_cancel;
    } else if (Underwater) {
        pline(msgc_cancelled1,
              "There is too much resistance to flick your bullwhip.");

    } else if (dz < 0) {
        pline(msgc_yafm, "You flick a bug off of the %s.", ceiling(u.ux, u.uy));

    } else if ((!dx && !dy) || (dz > 0)) {
        int dam;

        /* Sometimes you hit your steed by mistake */
        if (u.usteed && !rn2(proficient + 2)) {
            pline(msgc_substitute, "You whip %s!", mon_nam(u.usteed));
            kick_steed();
            return 1;
        }

        if (levitates(&youmonst) || u.usteed) {
            /* Have a shot at snaring something on the floor */
            otmp = level->objects[u.ux][u.uy];
            if (otmp && otmp->otyp == CORPSE && otmp->corpsenm == PM_HORSE) {
                pline(msgc_yafm, "Why beat a dead horse?");
                return 1;
            }
            if (otmp && proficient) {
                pline(msgc_occstart,
                      "You wrap your bullwhip around %s on the %s.",
                      an(singular(otmp, xname)), surface(u.ux, u.uy));
                if (rnl(6) || pickup_object(otmp, 1L, TRUE) < 1)
                    pline(msgc_failrandom, "%s", msg_slipsfree);
                return 1;
            }
        }
        dam = rnd(2) + dbon() + obj->spe;
        if (dam <= 0)
            dam = 1;
        pline(msgc_badidea, "You hit your %s with your bullwhip.",
              body_part(FOOT));
        buf = msgprintf("killed %sself with %s bullwhip", uhim(), uhis());
        losehp(dam, buf);
        return 1;

    } else if ((fumbling(&youmonst) || slippery_fingers(&youmonst)) &&
               !obj->cursed && !rn2(5)) {
        pline(msgc_substitute, "The bullwhip slips out of your %s.",
              body_part(HAND));
        unwield_silently(obj);
        dropx(obj);

    } else if (u.utrap && u.utraptype == TT_PIT) {
        /*
         *     Assumptions:
         *
         *      if you're in a pit
         *              - you are attempting to get out of the pit
         *              - or, if you are applying it towards a small
         *                monster then it is assumed that you are
         *                trying to hit it.
         *      else if the monster is wielding a weapon
         *              - you are attempting to disarm a monster
         *      else
         *              - you are attempting to hit the monster
         *
         *      if you're confused (and thus off the mark)
         *              - you only end up hitting.
         *
         */
        const char *wrapped_what = NULL;

        if (!isok(rx, ry)) {
            pline(msgc_yafm, "%s",
                  Is_airlevel(&u.uz) ? "You snap your whip through thin air." :
                  msg_snap);
            return 1;
        }

        if (mtmp) {
            if (bigmonst(mtmp->data)) {
                wrapped_what = mon_nam(mtmp);
            } else if (proficient) {
                enum attack_check_status attack_status =
                    attack(mtmp, dx, dy, FALSE);
                if (attack_status == ac_continue)
                    pline(msgc_failrandom, "%s", msg_snap);
                else
                    return attack_status != ac_cancel;
            }
        }
        if (!wrapped_what) {
            if (IS_FURNITURE(level->locations[rx][ry].typ))
                wrapped_what = "something";
            else if (sobj_at(BOULDER, level, rx, ry))
                wrapped_what = "a boulder";
        }
        if (wrapped_what) {
            coord cc;

            cc.x = rx;
            cc.y = ry;
            pline(msgc_occstart, "You wrap your bullwhip around %s.",
                  wrapped_what);
            if (proficient && rn2(proficient + 2)) {
                if (!mtmp || enexto(&cc, level, rx, ry, youmonst.data)) {
                    pline(msgc_actionok, "You yank yourself out of the pit!");
                    teleds(cc.x, cc.y, TRUE);
                    u.utrap = 0;
                    turnstate.vision_full_recalc = TRUE;
                }
            } else {
                pline(msgc_failrandom, "%s", msg_slipsfree);
            }
            if (mtmp)
                wakeup(mtmp, FALSE);
        } else
            pline(msgc_notarget, "%s", msg_snap);

    } else if (mtmp) {
        otmp = MON_WEP(mtmp);   /* can be null */
        if (otmp) {
            const char *onambuf = cxname(otmp);
            const char *mon_hand;
            boolean gotit = proficient && (!Fumbling || !rn2(10));

            if (gotit) {
                mon_hand = mbodypart(mtmp, HAND);
                if (bimanual(otmp))
                    mon_hand = makeplural(mon_hand);
            } else
                mon_hand = 0;   /* lint suppression */

            pline(msgc_occstart, "You wrap your bullwhip around %s %s.",
                  s_suffix(mon_nam(mtmp)), onambuf);
            if (gotit && otmp->cursed) {
                /* assume the player isn't tracking which monsters are wielding
                   cursed objects for message channel purposes */
                pline(msgc_failcurse, "%s welded to %s %s%c",
                      (otmp->quan == 1L) ? "It is" : "They are", mhis(mtmp),
                      mon_hand, !otmp->bknown ? '!' : '.');
                otmp->bknown = 1;
                gotit = FALSE;  /* can't pull it free */
            }
            if (gotit) {
                obj_extract_self(otmp);
                possibly_unwield(mtmp, FALSE);
                setmnotwielded(mtmp, otmp);

                switch (rn2(proficient + 1)) {
                case 2:
                    /* to floor near you */
                    pline(msgc_actionok, "You yank %s %s to the %s!",
                          s_suffix(mon_nam(mtmp)), onambuf,
                          surface(u.ux, u.uy));
                    place_object(otmp, level, u.ux, u.uy);
                    stackobj(otmp);
                    break;
                case 3:
                    /* right into your inventory */
                    pline(msgc_actionok, "You snatch %s %s!",
                          s_suffix(mon_nam(mtmp)), onambuf);
                    if (otmp->otyp == CORPSE && !uarmg &&
                        touched_monster(otmp->corpsenm)) {
                        /* Note that this isn't msgc_badidea: you didn't know
                           that a trice corpse is what you would grab. It's
                           msgc_substitute, "action malfunctions due to spoiler
                           information". */
                        pline(msgc_substitute,
                              "Snatching %s corpse is a fatal mistake.",
                              an(opm_name(otmp)));
                        instapetrify(killer_msg(STONING,
                            msgprintf("snatching %s corpse",
                                      an(opm_name(otmp)))));
                    }
                    hold_another_object(otmp, "You drop %s!", doname(otmp),
                                        NULL);
                    break;
                default:
                    /* to floor beneath mon */
                    pline(msgc_actionok, "You yank %s from %s %s!",
                          the(onambuf), s_suffix(mon_nam(mtmp)), mon_hand);
                    obj_no_longer_held(otmp);
                    place_object(otmp, level, mtmp->mx, mtmp->my);
                    stackobj(otmp);
                    break;
                }
            } else {
                pline(msgc_failrandom, "%s", msg_slipsfree);
            }
            wakeup(mtmp, TRUE);
        } else {
            if (mtmp->m_ap_type && !Protection_from_shape_changers &&
                !sensemon(mtmp))
                stumble_onto_mimic(mtmp, dx, dy);
            else
                pline(proficient ? msgc_occstart : msgc_failrandom,
                      "You flick your bullwhip towards %s.", mon_nam(mtmp));
            if (proficient) {
                enum attack_check_status attack_status =
                    attack(mtmp, dx, dy, FALSE);
                if (attack_status == ac_continue)
                    pline(msgc_notarget, "%s", msg_snap);
                else
                    return attack_status != ac_cancel;
            }
        }

    } else if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)) {
        /* it must be air -- water checked above */
        pline(msgc_notarget, "You snap your whip through thin air.");

    } else {
        pline(msgc_notarget, "%s", msg_snap);

    }
    return 1;
}

/* Choose an appropriate starting position for prompting */
static boolean
find_polearm_target(struct obj *obj, int minr, int maxr, coord *cc)
{
    if (!find_polearm_target_recurse(obj, minr, maxr, cc, TRUE, FALSE))
        return find_polearm_target_recurse(obj, minr, maxr, cc, FALSE, FALSE);
    return TRUE;
}

static boolean
update_polearm_target(struct obj *obj, int minr, int maxr, coord *cc)
{
    return find_polearm_target_recurse(obj, minr, maxr, cc, FALSE, TRUE);
}

static boolean
find_polearm_target_recurse(struct obj *obj, int minr, int maxr, coord *cc,
                            boolean use_poledir, boolean update_poledir)
{
    struct monst *mon;

    /* Fallback */
    if (!update_poledir) {
        cc->x = u.ux;
        cc->y = u.uy;
    }

    int x, y;
    int i = 0;
    int poledir = 0;
    if (obj && use_poledir)
        poledir = obj->poledir;

    for (x = (u.ux - 2); x <= (u.ux + 2); x++) {
        for (y = (u.uy - 2); y <= (u.uy + 2); y++) {
            i++;
            if (update_poledir) {
                if (x == cc->x && y == cc->y) {
                    obj->poledir = i;
                    return TRUE;
                }
                continue;
            }

            if (poledir && i != poledir)
                continue;

            if (!isok(x, y) || distu(x, y) < minr || distu(x, y) > maxr ||
                !couldsee(x, y))
                continue;

            mon = vismon_at(level, x, y);
            if (mon && !mon->mpeaceful) {
                cc->x = x;
                cc->y = y;
                return TRUE;
            }
        }
    }

    return FALSE;
}

static const char
     not_enough_room[] =
    "There's not enough room here to use that.", where_to_hit[] =
    "Where do you want to hit?", cant_see_spot[] =
    "You won't hit anything if you can't see that spot.", cant_reach[] =
    "You can't reach that spot from here.";

/* Distance attacks by pole-weapons */
int
use_pole(struct obj *obj, const struct nh_cmd_arg *arg)
{
    int wtstatus, typ, max_range = 4, min_range = 4;
    coord cc;
    struct monst *mtmp;
    boolean autofiring = last_command_was("fire");

    /* Are you allowed to use the pole? */
    if (Engulfed) {
        pline(msgc_cancelled, not_enough_room);
        return 0;
    }

    /* Calculate range */
    typ = weapon_type(obj);
    if (typ == P_NONE || P_SKILL(typ) <= P_BASIC)
        max_range = 4;
    else if (P_SKILL(typ) == P_SKILLED)
        max_range = 5;
    else
        max_range = 8;

    /* See if we have an appropriate target to autofire */
    if (autofiring && !find_polearm_target(NULL, min_range, max_range, &cc)) {
        pline(msgc_cancelled,
              "There doesn't seem to be an appropriate target to thrust.");
        return 0;
    }

    wtstatus = wield_tool(obj, "preparing to swing your polearm", occ_prepare,
                          autofiring);

    if (wtstatus & 2)
        return 1;
    if (!(wtstatus & 1))
        return 0;

    if (!autofiring) {
        /* Prompt for a location */
        pline(msgc_uiprompt, where_to_hit);
        find_polearm_target(obj, min_range, max_range, &cc);
        if (getargpos(arg, &cc, FALSE, "the spot to hit") == NHCR_CLIENT_CANCEL)
            return 0;     /* user pressed ESC */
    }

    if (distu(cc.x, cc.y) > max_range) {
        pline(distu(cc.x, cc.y) > 20 ? msgc_mispaste : msgc_cancelled,
              "Too far!");
        return 0;
    } else if (distu(cc.x, cc.y) < min_range) {
        pline(msgc_cancelled, "Too close!");
        return 0;
    } else if (!cansee(cc.x, cc.y) &&
               (mtmp = vismon_at(level, cc.x, cc.y)) == NULL) {
        pline(msgc_cancelled, cant_see_spot);
        return 0;
    } else if (!couldsee(cc.x, cc.y)) { /* Eyes of the Overworld */
        pline(msgc_cancelled, cant_reach);
        return 0;
    }

    /* Attack the monster there */
    if ((mtmp = m_at(level, cc.x, cc.y)) != NULL) {
        update_polearm_target(obj, min_range, max_range, &cc);
        int oldhp = mtmp->mhp;

        if (resolve_uim(flags.interaction_mode, TRUE, cc.x, cc.y) == uia_halt)
            return 0;

        bhitpos = cc;
        check_caitiff(mtmp);
        thitmonst(mtmp, uwep, NULL);
        /* check the monster's HP because thitmonst() doesn't return an
           indication of whether it hit.  Not perfect (what if it's a
           non-silver weapon on a shade?) */
        if (mtmp->mhp < oldhp)
            break_conduct(conduct_weaphit);
    } else
        /* Now you know that nothing is there... */
        pline(msgc_notarget, "Nothing happens.");
    return 1;
}


static int
use_cream_pie(struct obj **objp)
{
    boolean wasblind = !!blind(&youmonst);
    boolean wascreamed = u.ucreamed;
    boolean several = FALSE;
    struct obj *obj = *objp;

    if (obj->quan > 1L) {
        several = TRUE;
        obj = splitobj(obj, 1L);
    }
    if (Hallucination)
        pline(msgc_actionok, "You give yourself a facial.");
    else
        pline(msgc_actionok, "You immerse your %s in %s%s.", body_part(FACE),
              several ? "one of " : "",
              several ? makeplural(the(xname(obj))) : the(xname(obj)));
    if (can_blnd(NULL, &youmonst, AT_WEAP, obj)) {
        u.ucreamed += rnd(25);
        if (wasblind == !!blind(&youmonst)) /* was blind, or didn't become */
            pline(msgc_statusbad, "There's %ssticky goop all over your %s.",
                  wascreamed ? "more " : "", body_part(FACE));
        else
            pline(msgc_statusbad,
                  "You can't see through all the sticky goop on your %s.",
                  body_part(FACE));
        pline(msgc_controlhelp, "Use the command #wipe to clean your %s.",
              body_part(FACE));
    }
    if (obj->unpaid) {
        verbalize(msgc_unpaid, "You used it, you bought it!");
        bill_dummy_object(obj);
    }
    obj_extract_self(obj);
    delobj(obj);
    *objp = NULL;
    return 1;
}


static int
use_grapple(struct obj *obj, const struct nh_cmd_arg *arg)
{
    int wtstatus, typ, max_range = 4, tohit;
    coord cc;
    struct monst *mtmp;
    struct obj *otmp;

    /* Are you allowed to use the hook? */
    if (Engulfed) {
        pline(msgc_cancelled, not_enough_room);
        return 0;
    }

    wtstatus = wield_tool(obj, "preparing to grapple", occ_prepare, FALSE);

    if (wtstatus & 2)
        return 1;
    if (!(wtstatus & 1))
        return 0;

    /* Prompt for a location */
    pline(msgc_uiprompt, where_to_hit);
    cc.x = u.ux;
    cc.y = u.uy;
    if (getargpos(arg, &cc, TRUE, "the spot to hit") == NHCR_CLIENT_CANCEL)
        return 0;     /* user pressed ESC */

    /* Calculate range */
    typ = uwep_skill_type();
    if (typ == P_NONE || P_SKILL(typ) <= P_BASIC)
        max_range = 4;
    else if (P_SKILL(typ) == P_SKILLED)
        max_range = 5;
    else
        max_range = 8;
    if (distu(cc.x, cc.y) > max_range) {
        pline(distu(cc.x, cc.y) > 20 ? msgc_mispaste : msgc_cancelled,
              "Too far!");
        return 0;
    } else if (!cansee(cc.x, cc.y)) {
        pline(msgc_cancelled, cant_see_spot);
        return 0;
    } else if (!couldsee(cc.x, cc.y)) { /* Eyes of the Overworld */
        pline(msgc_cancelled, cant_reach);
        return 0;
    }

    /* What do you want to hit? */
    tohit = rn2(5);
    if (typ != P_NONE && P_SKILL(typ) >= P_SKILLED) {
        struct nh_menuitem items[3];
        const int *selected;

        set_menuitem(&items[0], 1, MI_NORMAL, "", 0, FALSE);
        snprintf(items[0].caption, SIZE(items[0].caption), "an object on the %s", surface(cc.x, cc.y));

        set_menuitem(&items[1], 2, MI_NORMAL, "a monster", 0, FALSE);

        set_menuitem(&items[2], 3, MI_NORMAL, "", 0, FALSE);
        snprintf(items[2].caption, SIZE(items[2].caption), "the %s", surface(cc.x, cc.y));

        if (display_menu
            (&(struct nh_menulist){.items = items, .icount = 3},
             "Aim for what?", PICK_ONE, PLHINT_ANYWHERE, &selected) &&
            rn2(P_SKILL(typ) > P_SKILLED ? 20 : 2))
            tohit = selected[0];
    }

    /* What did you hit? */
    switch (tohit) {
    case 0:    /* Trap */
        /* FIXME -- untrap needs to deal with non-adjacent traps */
        break;
    case 1:    /* Object */
        if ((otmp = level->objects[cc.x][cc.y]) != 0) {
            /* We use msgc_occstart before pickup_object, because attempting to
               pick up an object can fail and pickup_object always prints a
               message. */
            pline(msgc_occstart, "You snag an object from the %s!",
                  surface(cc.x, cc.y));
            pickup_object(otmp, 1L, FALSE);
            /* If pickup fails, leave it alone */
            newsym(cc.x, cc.y);
            return 1;
        }
        break;
    case 2:    /* Monster */
        if ((mtmp = m_at(level, cc.x, cc.y)) == NULL)
            break;
        if (verysmall(mtmp->data) && !rn2(4) &&
            enexto(&cc, level, u.ux, u.uy, NULL)) {
            pline(msgc_actionok, "You pull in %s!", mon_nam(mtmp));
            mtmp->mundetected = 0;
            rloc_to(mtmp, cc.x, cc.y);
            return 1;
        } else if ((!bigmonst(mtmp->data) && !strongmonst(mtmp->data)) ||
                   rn2(4)) {
            thitmonst(mtmp, uwep, NULL);
            return 1;
        }
        /* TODO: This fallthrough looks very suspicious, even though there's a
           comment saying it's intentional. (Wouldn't be the first time
           something like that had happened...) */
        /* FALL THROUGH */
    case 3:    /* Surface */
        if (IS_AIR(level->locations[cc.x][cc.y].typ) ||
            is_pool(level, cc.x, cc.y))
            pline(msgc_notarget, "The hook slices through the %s.",
                  surface(cc.x, cc.y)); /* verifies pool there when blind */
        else {
            pline(msgc_actionok, "You are yanked toward the %s!",
                  surface(cc.x, cc.y));
            hurtle(sgn(cc.x - u.ux), sgn(cc.y - u.uy), 1, FALSE);
            spoteffects(TRUE);
        }
        return 1;
    default:   /* Yourself (oops!) */
        if (P_SKILL(typ) <= P_BASIC) {
            pline(msgc_substitute, "You hook yourself!");
            losehp(rn1(10, 10), killer_msg(DIED, "a grappling hook"));
            return 1;
        }
        break;
    }
    pline(msgc_failrandom, "Nothing happens.");
    return 1;
}


#define BY_OBJECT       (NULL)

/* return 1 if the wand is broken, hence some time elapsed */
int
do_break_wand(struct obj *obj, boolean intentional)
{
    static const char nothing_else_happens[] = "But nothing else happens...";
    int i, x, y;
    int dmg;
    int shop_damage = 0; /* 1=digging, 2=from bhit_at */
    int expltype = EXPL_MAGICAL;
    const char *confirm, *the_wand;

    if (intentional) {
        the_wand = yname(obj);
        confirm = msgprintf("Are you really sure you want to break %s?",
                             safe_qbuf("", sizeof
                                       "Are you really sure you want to break ?",
                                       the_wand, ysimple_name(obj),
                                       "the wand"));
        if (yn(confirm) == 'n')
            return 0;
        if (nohands(youmonst.data)) {
            pline(msgc_cancelled, "You can't break %s without hands!", the_wand);
            return 0;
        } else if (ACURR(A_STR) < 10 || obj->oartifact) {
            pline(msgc_cancelled,
                  "You don't have the strength to break %s!", the_wand);
            return 0;
        }
        pline(msgc_occstart, "Raising %s high above your %s, you break it in two!",
              the_wand, body_part(HEAD));
    }

    /* [ALI] Do this first so that wand is removed from bill. Otherwise, the
       freeinv() below also hides it from setpaid() which causes problems. */
    if (obj->unpaid) {
        check_unpaid(obj);      /* Extra charge for use */
        bill_dummy_object(obj);
    }

    turnstate.tracked[ttos_wand] = obj; /* destroy_item might reset this */
    setnotworn(obj);    /* so it can be freed (it's hidden from destroy) */
    freeinv(obj);       /* hide it from destroy_item */

    if (obj->spe <= 0) {
        pline(msgc_failcurse, nothing_else_happens);
        goto discard_broken_wand;
    }
    obj->ox = u.ux;
    obj->oy = u.uy;
    dmg = obj->spe * 4;

    switch (obj->otyp) {
    case WAN_WISHING:
    case WAN_NOTHING:
    case WAN_LOCKING:
    case WAN_PROBING:
    case WAN_ENLIGHTENMENT:
    case WAN_OPENING:
    case WAN_SECRET_DOOR_DETECTION:
        /* We could potentially check if the wand is IDed and switch to
           msgc_badidea in that case, but it probably isn't worth it */
        pline(msgc_failcurse, nothing_else_happens);
        goto discard_broken_wand;
    case WAN_DEATH:
    case WAN_LIGHTNING:
        dmg *= 4;
        goto wanexpl;
    case WAN_FIRE:
        expltype = EXPL_FIERY;
    case WAN_COLD:
        if (expltype == EXPL_MAGICAL)
            expltype = EXPL_FROSTY;
        dmg *= 2;
    case WAN_MAGIC_MISSILE:
    wanexpl:
        explode(u.ux, u.uy, (obj->otyp - WAN_MAGIC_MISSILE), dmg, WAND_CLASS,
                expltype, NULL, 0);
        tell_discovery(obj);   /* explode described the effect */
        goto discard_broken_wand;
    case WAN_STRIKING:
        /* we want this before the explosion instead of at the very end */
        pline_implied(msgc_badidea,
                      "A wall of force smashes down around you!");
        dmg = dice(1 + obj->spe, 6);    /* normally 2d12 */
        break;
    default:
        break;
    }

    /* magical explosion and its visual effect occur before specific effects */
    explode(obj->ox, obj->oy, 0, rnd(dmg), WAND_CLASS, EXPL_MAGICAL, NULL, 0);

    /* this makes it hit us last, so that we can see the action first */
    int bhit = BHIT_NONE;
    for (i = 0; i <= 8; i++) {
        bhitpos.x = x = obj->ox + xdir[i];
        bhitpos.y = y = obj->oy + ydir[i];
        if (!isok(x, y))
            continue;

        if (obj->otyp == WAN_DIGGING) {
            if (dig_check(BY_OBJECT, msgc_mute, x, y)) {
                if (IS_WALL(level->locations[x][y].typ) ||
                    IS_DOOR(level->locations[x][y].typ)) {
                    /* normally, pits and holes don't anger guards, but they do 
                       if it's a wall or door that's being dug */
                    watch_warn(NULL, x, y, TRUE);
                    if (*in_rooms(level, x, y, SHOPBASE))
                        shop_damage = 1;
                }
                digactualhole(x, y, BY_OBJECT,
                              (rn2(obj->spe) < 3 ||
                               !can_dig_down(level)) ? PIT : HOLE);
            }
            continue;
        } else if (obj->otyp == WAN_CREATE_MONSTER) {
            /* u.ux,u.uy creates it near you--x,y might create it in rock */
            makemon(NULL, level, u.ux, u.uy, MM_CREATEMONSTER | MM_CMONSTER_U);
            continue;
        } else if ((bhit = bhit_at(&youmonst, obj, x, y, 10))) {
            if (bhit & BHIT_SHOPDAM)
                shop_damage = 2;
            bot(); /* blindness */
        }
    }

    /* Note: if player fell thru, this call is a no-op. Damage is handled in
       digactualhole in that case */
    if (shop_damage)
        pay_for_damage(shop_damage == 1 ? "dig into" : "destroy", FALSE);

    if (obj->otyp == WAN_LIGHT)
        litroom(&youmonst, TRUE, obj, TRUE);     /* only needs to be done once */

discard_broken_wand:
    obj = turnstate.tracked[ttos_wand]; /* [see dozap() and destroy_mitem()] */
    turnstate.tracked[ttos_wand] = 0;
    if (obj)
        delobj(obj);
    action_completed();
    return 1;
}

static boolean
uhave_graystone(void)
{
    struct obj *otmp;

    for (otmp = youmonst.minvent; otmp; otmp = otmp->nobj)
        if (is_graystone(otmp))
            return TRUE;
    return FALSE;
}

int
doapply(const struct nh_cmd_arg *arg)
{
    static const char tools[] = {
        ALLOW_COUNT, ALL_CLASSES, ALLOW_NONE, NONE_ON_COMMA, SPLIT_LETTER,
        TOOL_CLASS, WEAPON_CLASS, 0
    };

    static const char tools_too[] = {
        ALLOW_COUNT, ALL_CLASSES, ALLOW_NONE, NONE_ON_COMMA, SPLIT_LETTER,
        TOOL_CLASS, POTION_CLASS, WEAPON_CLASS, WAND_CLASS, GEM_CLASS, 0
    };

    int res = 1;
    const char *class_list;
    struct obj *obj;

    if (check_capacity(NULL))
        return 0;

    if (carrying(POT_OIL) || uhave_graystone())
        class_list = tools_too;
    else
        class_list = tools;
    if (carrying(CREAM_PIE) || carrying(EUCALYPTUS_LEAF))
        class_list = msgkitten(class_list, FOOD_CLASS);

    obj = getargobj(arg, class_list, "use or apply");
    if (!obj)
        return 0;

    if (obj == &zeroobj) {
        /* "a," for doing looting */
        return doloot(arg);
    }

    if (obj->oartifact && !touch_artifact(obj, &youmonst))
        return 1;       /* evading your grasp costs a turn; just be grateful
                           that you don't drop it as well */

    if (obj->oclass == WAND_CLASS) {
        pline(msgc_controlhelp,
              "To break wands, use the 'invoke' command (typically on 'V').");
        return 0;
    }

    switch (obj->otyp) {
    case BLINDFOLD:
    case LENSES:
        res = equip_in_slot(obj, os_tool, FALSE);
        break;
    case CREAM_PIE:
        res = use_cream_pie(&obj);
        break;
    case BULLWHIP:
        res = use_whip(obj, arg);
        break;
    case GRAPPLING_HOOK:
        res = use_grapple(obj, arg);
        break;
    case LARGE_BOX:
    case CHEST:
    case ICE_BOX:
    case SACK:
    case BAG_OF_HOLDING:
    case OILSKIN_SACK:
        res = use_container(obj, 1, FALSE, FALSE);
        if (res < 0)
            res = 0;
        break;
    case BAG_OF_TRICKS:
        bagotricks(obj);
        break;
    case CAN_OF_GREASE:
        res = use_grease(obj);
        break;
    case LOCK_PICK:
    case CREDIT_CARD:
    case SKELETON_KEY:
        res = pick_lock(obj, arg, NULL);
        break;
    case TINNING_KIT:
        res = use_tinning_kit(obj);
        break;
    case LEASH:
        res = use_leash(obj, arg);
        break;
    case SADDLE:
        res = use_saddle(obj, arg);
        break;
    case MAGIC_WHISTLE:
        res = use_magic_whistle(obj);
        break;
    case TIN_WHISTLE:
        res = use_whistle(obj);
        break;
    case EUCALYPTUS_LEAF:
        /* MRKR: Every Australian knows that a gum leaf makes an excellent
           whistle, especially if your pet is a tame kangaroo named Skippy. */
        if (obj->blessed) {
            use_magic_whistle(obj);
            /* sometimes the blessing will be worn off */
            if (!rn2_on_rng(49, rng_eucalyptus)) {
                if (obj->quan > 1)
                    obj = splitobj(obj, 1);
                if (!Blind) {
                    pline(msgc_itemloss, "%s %s %s.", Shk_Your(obj),
                          aobjnam(obj, "glow"), hcolor("brown"));
                    obj->bknown = 1;
                }
                unbless(obj);
                obj_extract_self(obj);
                hold_another_object(obj, "You drop %s!", doname(obj), NULL);
            }
        } else {
            use_whistle(obj);
        }
        break;
    case STETHOSCOPE:
        res = use_stethoscope(obj, arg);
        break;
    case MIRROR:
        res = use_mirror(obj, arg);
        break;
    case BELL:
    case BELL_OF_OPENING:
        use_bell(&obj);
        break;
    case CANDELABRUM_OF_INVOCATION:
        res = use_candelabrum(obj);
        break;
    case WAX_CANDLE:
    case TALLOW_CANDLE:
        res = use_candle(&obj);
        break;
    case OIL_LAMP:
    case MAGIC_LAMP:
    case BRASS_LANTERN:
        res = use_lamp(obj);
        break;
    case POT_OIL:
        res = light_cocktail(obj);
        break;
    case EXPENSIVE_CAMERA:
        res = use_camera(obj, arg);
        break;
    case TOWEL:
        res = use_towel(obj);
        break;
    case CRYSTAL_BALL:
        use_crystal_ball(obj);
        break;
    case MAGIC_MARKER:
        res = dowrite(obj, arg);
        break;
    case TIN_OPENER:
        if (!carrying(TIN)) {
            pline(msgc_mispaste, "You have no tin to open.");
            goto xit;
        }
        pline(msgc_hint, "You cannot open a tin without eating or "
              "discarding its contents.");
        pline(msgc_controlhelp, "In order to eat, use the 'e' command.");
        if (obj != uwep)
            pline(msgc_hint, "Opening the tin will be much easier if you "
                  "wield the tin opener.");
        goto xit;

    case FIGURINE:
        res = use_figurine(&obj, arg);
        break;
    case UNICORN_HORN:
        use_unicorn_horn(&youmonst, obj);
        break;
    case WOODEN_FLUTE:
    case MAGIC_FLUTE:
    case TOOLED_HORN:
    case FROST_HORN:
    case FIRE_HORN:
    case WOODEN_HARP:
    case MAGIC_HARP:
    case BUGLE:
    case LEATHER_DRUM:
    case DRUM_OF_EARTHQUAKE:
        res = do_play_instrument(obj, arg);
        break;
    case HORN_OF_PLENTY:       /* not a musical instrument */
        if (obj->spe > 0) {
            struct obj *otmp;
            const char *what;

            consume_obj_charge(obj, TRUE);
            if (!rn2_on_rng(13, rng_horn_of_plenty)) {
                otmp = mkobj(level, POTION_CLASS, FALSE, rng_horn_of_plenty);
                if (objects[otmp->otyp].oc_magic)
                    do {
                        otmp->otyp = rnd_class(
                            POT_BOOZE, POT_WATER, rng_horn_of_plenty);
                    } while (otmp->otyp == POT_SICKNESS);
                what = "A potion";
            } else {
                otmp = mkobj(level, FOOD_CLASS, FALSE, rng_horn_of_plenty);
                if (otmp->otyp == FOOD_RATION &&
                    !rn2_on_rng(7, rng_horn_of_plenty))
                    otmp->otyp = LUMP_OF_ROYAL_JELLY;
                what = "Some food";
            }
            pline(msgc_actionok, "%s spills out.", what);
            otmp->blessed = obj->blessed;
            otmp->cursed = obj->cursed;
            otmp->owt = weight(otmp);
            hold_another_object(
                otmp, Engulfed ? "Oops!  %s out of your reach!"
                : (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
                   level->locations[u.ux][u.uy].typ < IRONBARS
                   || level->locations[u.ux][u.uy].typ >=
                   ICE) ? "Oops!  %s away from you!" :
                "Oops!  %s to the floor!",
                The(aobjnam(otmp, "slip")), NULL);
        } else {
            pline(obj->known ? msgc_cancelled1 : msgc_failcurse,
                  "You feel an absence of magical power.");
            obj->known = TRUE;
        }

        /* other horns make frightful sounds if discharged, so we know it's a
           horn of plenty no matter what. */
        tell_discovery(obj);
        break;
    case LAND_MINE:
    case BEARTRAP:
        res = use_trap(obj, arg);
        break;
    case FLINT:
    case LUCKSTONE:
    case LOADSTONE:
    case TOUCHSTONE:
        res = use_stone(obj);
        break;
    default:
        /* Pole-weapons can strike at a distance */
        if (is_pole(obj)) {
            res = use_pole(obj, arg);
            break;
        } else if (is_pick(obj) || is_axe(obj)) {
            res = use_pick_axe(obj, arg);
            break;
        }
        pline(msgc_mispaste, "Sorry, I don't know how to use that.");
    xit:
        action_completed();
        return 0;
    }
    if (res && obj && obj->oartifact)
        arti_speak(obj);
    return res;
}

/* Keep track of unfixable troubles for purposes of messages saying you feel
 * great.
 */
int
unfixable_trouble_count(boolean is_horn)
{
    int unfixable_trbl = 0;

    if (petrifying(&youmonst))
        unfixable_trbl++;
    if (strangled(&youmonst))
        unfixable_trbl++;
    if (leg_hurt(&youmonst))
        unfixable_trbl++;
    if (sliming(&youmonst))
        unfixable_trbl++;
    /* lycanthropy is not desirable, but it doesn't actually make you feel
       bad */

    /* Intrinsics doesn't make you feel bad
       (stun in bat form, halluc as light, etc) */
    if (!is_horn) {
        if (confused(&youmonst) & ~INTRINSIC)
            unfixable_trbl++;
        if (sick(&youmonst) & ~INTRINSIC)
            unfixable_trbl++;
        if (hallucinating(&youmonst) & ~INTRINSIC)
            unfixable_trbl++;
        if (vomiting(&youmonst) & ~INTRINSIC)
            unfixable_trbl++;
        if (stunned(&youmonst) & ~INTRINSIC)
            unfixable_trbl++;
    }
    return unfixable_trbl;
}

/*apply.c*/
