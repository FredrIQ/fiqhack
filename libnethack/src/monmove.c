/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"
#include "artifact.h"
#include "epri.h"


static int disturb(struct monst *);
static void distfleeck(struct monst *, int *, int *, int *);
static int m_arrival(struct monst *);
static void watch_on_duty(struct monst *);


/* TRUE : mtmp died */
boolean
mb_trapped(struct monst *mtmp)
{
    if (flags.verbose) {
        if (cansee(mtmp->mx, mtmp->my))
            pline("KABOOM!!  You see a door explode.");
        else
            You_hear("a distant explosion.");
    }
    wake_nearto(mtmp->mx, mtmp->my, 7 * 7);
    mtmp->mstun = 1;
    mtmp->mhp -= rnd(15);
    if (mtmp->mhp <= 0) {
        mondied(mtmp);
        if (mtmp->mhp > 0)      /* lifesaved */
            return FALSE;
        else
            return TRUE;
    }
    return FALSE;
}


/* Called every turn by the Watch to see if they notice any misbehaviour.
   Currently this only handles occupations. (There are other behaviours
   forbidden by the Watch via other codepaths.) */
static void
watch_on_duty(struct monst *mtmp)
{
    int x, y;

    if (mtmp->mpeaceful && in_town(u.ux, u.uy) && mtmp->mcansee &&
        m_canseeu(mtmp) && !rn2(3)) {

        /* If you're digging, or if you're picking a lock (chests are OK), the
           Watch may potentially be annoyed. We calculate the location via
           working out which location command repeat would affect. (These cases
           are merged because they both affect map locations on an ongoing
           basis, so the code for both is much the same.) */
        if ((flags.occupation == occ_dig ||
             (flags.occupation == occ_lock &&
              u.utracked[tos_lock] == &zeroobj)) &&
            (flags.last_arg.argtype & CMD_ARG_DIR)) {
            schar dx, dy, dz;

            dir_to_delta(flags.last_arg.dir, &dx, &dy, &dz);
            x = u.ux + dx;
            y = u.uy + dy;

            if (isok(x, y))
                watch_warn(mtmp, x, y, FALSE);
        }
    }
}


int
dochugw(struct monst *mtmp)
{
    int x = mtmp->mx, y = mtmp->my;
    boolean already_saw_mon = canspotmon(mtmp);
    int rd = dochug(mtmp);

    /* a similar check is in monster_nearby() in hack.c */
    /* check whether hero notices monster and stops current activity */
    if (!rd && !Confusion && (!mtmp->mpeaceful || Hallucination) &&
        /* it's close enough to be a threat */
        distu(mtmp->mx, mtmp->my) <= (BOLT_LIM + 1) * (BOLT_LIM + 1) &&
        /* and either couldn't see it before, or it was too far away */
        (!already_saw_mon || !couldsee(x, y) ||
         distu(x, y) > (BOLT_LIM + 1) * (BOLT_LIM + 1)) &&
        /* can see it now, or sense it and would normally see it

           TODO: This can spoil the existence of walls in dark areas. */
        (canseemon(mtmp) || (sensemon(mtmp) && couldsee(mtmp->mx, mtmp->my))) &&
        mtmp->mcanmove && !noattacks(mtmp->data) && !onscary(u.ux, u.uy, mtmp))
        action_interrupted();

    return rd;
}


boolean
onscary(int x, int y, struct monst * mtmp)
{
    if (mtmp->isshk || mtmp->isgd || mtmp->iswiz || !mtmp->mcansee ||
        mtmp->mpeaceful || mtmp->data->mlet == S_HUMAN || is_lminion(mtmp) ||
        mtmp->data == &mons[PM_ANGEL] || is_rider(mtmp->data) ||
        mtmp->data == &mons[PM_MINOTAUR])
        return FALSE;

    return (boolean) (sobj_at(SCR_SCARE_MONSTER, level, x, y)
                      || (sengr_at("Elbereth", x, y) && flags.elbereth_enabled)
                      || (mtmp->data->mlet == S_VAMPIRE &&
                          IS_ALTAR(level->locations[x][y].typ)));
}


/* regenerate lost hit points */
void
mon_regen(struct monst *mon, boolean digest_meal)
{
    if (mon->mhp < mon->mhpmax && (moves % 20 == 0 || regenerates(mon->data)))
        mon->mhp++;
    if (mon->mspec_used)
        mon->mspec_used--;
    if (digest_meal) {
        if (mon->meating)
            mon->meating--;
    }
}

/*
 * Possibly awaken the given monster.  Return a 1 if the monster has been
 * jolted awake.
 */
static int
disturb(struct monst *mtmp)
{
    /*
     * + Ettins are hard to surprise.
     * + Nymphs, jabberwocks, and leprechauns do not easily wake up.
     *
     * Wake up if:
     *      in line of effect                                       AND
     *      within 10 squares                                       AND
     *      not stealthy or (mon is an ettin and 9/10)              AND
     *      (mon is not a nymph, jabberwock, or leprechaun) or 1/50 AND
     *      Aggravate or mon is (dog or human) or
     *          (1/7 and mon is not mimicing furniture or object)
     */
    if (couldsee(mtmp->mx, mtmp->my) && distu(mtmp->mx, mtmp->my) <= 100 &&
        (!Stealth || (mtmp->data == &mons[PM_ETTIN] && rn2(10))) &&
        (!(mtmp->data->mlet == S_NYMPH || mtmp->data == &mons[PM_JABBERWOCK]
           || mtmp->data->mlet == S_LEPRECHAUN) || !rn2(50)) &&
        (Aggravate_monster ||
         (mtmp->data->mlet == S_DOG || mtmp->data->mlet == S_HUMAN)
         || (!rn2(7) && mtmp->m_ap_type != M_AP_FURNITURE &&
             mtmp->m_ap_type != M_AP_OBJECT))) {
        mtmp->msleeping = 0;
        return 1;
    }
    return 0;
}

/*
 * monster begins fleeing for the specified time, 0 means untimed flee
 * if first, only adds fleetime if monster isn't already fleeing
 * if fleemsg, prints a message about new flight, otherwise, caller should
 */
void
monflee(struct monst *mtmp, int fleetime, boolean first, boolean fleemsg)
{
    if (DEADMONSTER(mtmp))
        return;

    if (u.ustuck == mtmp) {
        if (Engulfed)
            expels(mtmp, mtmp->data, TRUE);
        else if (!sticks(youmonst.data)) {
            unstuck(mtmp);      /* monster lets go when fleeing */
            pline("You get released!");
        }
    }

    if (!first || !mtmp->mflee) {
        /* don't lose untimed scare */
        if (!fleetime)
            mtmp->mfleetim = 0;
        else if (!mtmp->mflee || mtmp->mfleetim) {
            fleetime += mtmp->mfleetim;
            /* ensure monster flees long enough to visibly stop fighting */
            if (fleetime == 1)
                fleetime++;
            mtmp->mfleetim = min(fleetime, 127);
        }
        if (!mtmp->mflee && fleemsg && canseemon(mtmp) && !mtmp->mfrozen) {
            if (mtmp->data->mlet != S_MIMIC)
                pline("%s turns to flee!", (Monnam(mtmp)));
            else
                pline("%s mimics a chicken for a moment!", (Monnam(mtmp)));
        }
        mtmp->mflee = 1;
    }
}

static void
distfleeck(struct monst *mtmp, int *inrange, int *nearby, int *scared)
{
    int seescaryx, seescaryy;

    *inrange = aware_of_u(mtmp) &&
        ((Engulfed && mtmp == u.ustuck) ||
         (dist2(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy)
          <= BOLT_LIM * BOLT_LIM));

    *nearby = *inrange && ((Engulfed && mtmp == u.ustuck) ||
                           monnear(mtmp, mtmp->mux, mtmp->muy));

    /* Note: if your image is displaced, the monster sees the Elbereth at your
       displaced position, thus never attacking your displaced position, but
       possibly attacking you by accident.  If you are invisible, it sees the
       Elbereth at your real position, thus never running into you by accident
       but possibly attacking the spot where it guesses you are. */
    if (awareness_reason(mtmp) == mar_guessing_displaced) {
        seescaryx = mtmp->mux;
        seescaryy = mtmp->muy;
    } else {
        seescaryx = u.ux;
        seescaryy = u.uy;
    }
    *scared = (*nearby &&
               (onscary(seescaryx, seescaryy, mtmp) ||
                (!mtmp->mpeaceful && in_your_sanctuary(mtmp, 0, 0))));

    if (*scared) {
        if (rn2(7))
            monflee(mtmp, rnd(10), TRUE, TRUE);
        else
            monflee(mtmp, rnd(100), TRUE, TRUE);
    }

}

/* perform a special one-time action for a monster; returns -1 if nothing
   special happened, 0 if monster uses up its turn, 1 if monster is killed */
static int
m_arrival(struct monst *mon)
{
    mon->mstrategy &= ~STRAT_ARRIVE;    /* always reset */

    return -1;
}

/* returns 1 if monster died moving, 0 otherwise */
/* The whole dochugw/m_move/distfleeck/mfndpos section is serious spaghetti
 * code. --KAA
 */
int
dochug(struct monst *mtmp)
{
    const struct permonst *mdat;
    int tmp = 0;
    int inrange, nearby, scared; /* note: all these depend on aware_of_u */
    struct obj *ygold = 0, *lepgold = 0;
    struct musable musable;

    /* Pre-movement adjustments */

    mdat = mtmp->data;

    if (mtmp->mstrategy & STRAT_ARRIVE) {
        int res = m_arrival(mtmp);

        if (res >= 0)
            return res;
    }

    /* check for waitmask status change */
    if ((mtmp->mstrategy & STRAT_WAITFORU) &&
        (m_canseeu(mtmp) || mtmp->mhp < mtmp->mhpmax))
        mtmp->mstrategy &= ~STRAT_WAITFORU;

    /* update quest status flags */
    quest_stat_check(mtmp);

    /* TODO: Quest leaders should really be affected by invisibility and
       displacement, but that's not only more of a balance change than I'm
       comfortable with, it also seems likely to introduce weird bugs. So this
       uses monnear and your real location. */
    if (!mtmp->mcanmove || (mtmp->mstrategy & STRAT_WAITMASK)) {
        if (Hallucination)
            newsym(mtmp->mx, mtmp->my);
        if (mtmp->mcanmove && (mtmp->mstrategy & STRAT_CLOSE) &&
            !mtmp->msleeping && monnear(mtmp, u.ux, u.uy))
            quest_talk(mtmp);   /* give the leaders a chance to speak */
        return 0;       /* other frozen monsters can't do anything */
    }

    /* there is a chance we will wake it */
    if (mtmp->msleeping && !disturb(mtmp)) {
        if (Hallucination)
            newsym(mtmp->mx, mtmp->my);
        return 0;
    }

    /* not frozen or sleeping: wipe out texts written in the dust */
    wipe_engr_at(mtmp->dlevel, mtmp->mx, mtmp->my, 1);

    /* confused monsters get unconfused with small probability */
    if (mtmp->mconf && !rn2(50))
        mtmp->mconf = 0;

    /* stunned monsters get un-stunned with larger probability */
    if (mtmp->mstun && !rn2(10))
        mtmp->mstun = 0;

    /* some monsters teleport */
    if (mtmp->mflee && !rn2(40) && can_teleport(mdat) && !mtmp->iswiz &&
        !level->flags.noteleport) {
        rloc(mtmp, TRUE);
        return 0;
    }
    if (mdat->msound == MS_SHRIEK && !um_dist(mtmp->mx, mtmp->my, 1))
        m_respond(mtmp);
    if (mdat == &mons[PM_MEDUSA] && couldsee(mtmp->mx, mtmp->my))
        m_respond(mtmp);
    if (mtmp->mhp <= 0)
        return 1;       /* m_respond gaze can kill medusa */

    /* fleeing monsters might regain courage */
    if (mtmp->mflee && !mtmp->mfleetim && mtmp->mhp == mtmp->mhpmax && !rn2(25))
        mtmp->mflee = 0;

    strategy(mtmp, FALSE); /* calls set_apparxy */
    /* Must be done after you move and before the monster does.  The
       set_apparxy() call in m_move() doesn't suffice since the variables
       inrange, etc. all depend on stuff set by set_apparxy(). */

    /* Monsters that want to acquire things */
    /* may teleport, so do it before inrange is set */
    if (is_covetous(mdat)) {
        tmp = tactics(mtmp);
        if (tmp != 0)
            return tmp == 2;
    }

    /* check distance and scariness of attacks */
    distfleeck(mtmp, &inrange, &nearby, &scared);

    if (find_defensive(mtmp, &musable)) {
        if (use_defensive(mtmp, &musable) != 0)
            return 1;
    } else if (find_misc(mtmp, &musable)) {
        if (use_misc(mtmp, &musable) != 0)
            return 1;
    }

    /* Demonic Blackmail! */
    if (nearby && mdat->msound == MS_BRIBE && mtmp->mpeaceful && !mtmp->mtame &&
        !Engulfed) {
        if (u_helpless(hm_all))
            return 0; /* wait for you to be able to respond */
        if (!knows_ux_uy(mtmp)) {
            if (aware_of_u(mtmp)) {
                pline("%s whispers at thin air.",
                      cansee(mtmp->mux, mtmp->muy) ? Monnam(mtmp) : "It");

                if (is_demon(youmonst.data)) {
                    /* "Good hunting, brother" */
                    if (!tele_restrict(mtmp))
                        rloc(mtmp, TRUE);
                } else {
                    mtmp->minvis = mtmp->perminvis = 0;
                    /* Why? For the same reason in real demon talk */
                    pline("%s gets angry!", Amonnam(mtmp));
                    msethostility(mtmp, TRUE, FALSE);
                    /* TODO: reset alignment? */
                    /* since no way is an image going to pay it off */
                }
            }
        } else if (demon_talk(mtmp))
            return 1;   /* you paid it off */
    }

    /* the watch will look around and see if you are up to no good :-) */
    if (mdat == &mons[PM_WATCHMAN] || mdat == &mons[PM_WATCH_CAPTAIN])
        watch_on_duty(mtmp);

    else if (is_mind_flayer(mdat) && !rn2(20)) {
        struct monst *m2, *nmon = NULL;

        if (canseemon(mtmp))
            pline("%s concentrates.", Monnam(mtmp));
        if (distu(mtmp->mx, mtmp->my) > BOLT_LIM * BOLT_LIM) {
            pline("You sense a faint wave of psychic energy.");
            goto toofar;
        }
        pline("A wave of psychic energy pours over you!");
        if (mtmp->mpeaceful && (!Conflict || resist(mtmp, RING_CLASS, 0, 0)))
            pline("It feels quite soothing.");
        else {
            boolean m_sen = sensemon(mtmp);

            if (m_sen || (Blind_telepat && rn2(2)) || !rn2(10)) {
                int dmg;

                pline("It locks on to your %s!",
                      m_sen ? "telepathy" : Blind_telepat ? "latent telepathy" :
                      "mind");
                dmg = rnd(15);
                if (Half_spell_damage)
                    dmg = (dmg + 1) / 2;
                losehp(dmg, killer_msg(DIED, "a psychic blast"));
            }
        }
        for (m2 = level->monlist; m2; m2 = nmon) {
            nmon = m2->nmon;
            if (DEADMONSTER(m2))
                continue;
            if (m2->mpeaceful == mtmp->mpeaceful)
                continue;
            if (mindless(m2->data))
                continue;
            if (m2 == mtmp)
                continue;
            if ((telepathic(m2->data) && (rn2(2) || m2->mblinded)) ||
                !rn2(10)) {
                if (cansee(m2->mx, m2->my))
                    pline("It locks on to %s.", mon_nam(m2));
                m2->mhp -= rnd(15);
                if (m2->mhp <= 0)
                    monkilled(m2, "", AD_DRIN);
                else
                    m2->msleeping = 0;
            }
        }
    }
toofar:

    /* If monster is nearby you, and has to wield a weapon, do so.  This costs
       the monster a move, of course. */
    if ((!mtmp->mpeaceful || Conflict) && inrange &&
        (engulfing_u(mtmp) ||
         dist2(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy) <= 8) &&
        attacktype(mdat, AT_WEAP)) {
        struct obj *mw_tmp;

        /* The scared check is necessary.  Otherwise a monster that is one
           square near the player but fleeing into a wall would keep switching
           between pick-axe and weapon.  If monster is stuck in a trap, prefer
           ranged weapon (wielding is done in thrwmu). This may cost the
           monster an attack, but keeps the monster from switching back and
           forth if carrying both. */
        mw_tmp = MON_WEP(mtmp);
        if (!(scared && mw_tmp && is_pick(mw_tmp)) &&
            mtmp->weapon_check == NEED_WEAPON &&
            !(mtmp->mtrapped && !nearby && select_rwep(mtmp))) {
            mtmp->weapon_check = NEED_HTH_WEAPON;
            if (mon_wield_item(mtmp) != 0)
                return 0;
        }
    }

    /* Look for other monsters to fight (at a distance) */
    if ((attacktype(mtmp->data, AT_BREA) || attacktype(mtmp->data, AT_GAZE) ||
         attacktype(mtmp->data, AT_SPIT) ||
         (attacktype(mtmp->data, AT_WEAP) && select_rwep(mtmp) != 0)) &&
        mtmp->mlstmv != moves) {
        struct monst *mtmp2 = mfind_target(mtmp);

        if (mtmp2 && mtmp2 != &youmonst) {
            if (mattackm(mtmp, mtmp2) & MM_AGR_DIED)
                return 1;       /* Oops. */

            return 0;   /* that was our move for the round */
        }
    }

    /* Now the actual movement phase */

    if (mdat->mlet == S_LEPRECHAUN) {
        ygold = findgold(invent);
        lepgold = findgold(mtmp->minvent);
    }

    /* We have two AI branches: "immediately attack the player's apparent
       location", and "don't immediately attack the player's apparent location"
       (in which case attacking the player's apparent location is still an
       option, but it'll only be taken if the player's in the monster's way).
       For the fallthroughs to work correctly, the "don't attack" branch comes
       first, and we decide to use it via this rather large if statement. */

    if (!nearby || mtmp->mflee || scared || mtmp->mconf || mtmp->mstun ||
        (mtmp->minvis && !rn2(3)) ||
        (mdat->mlet == S_LEPRECHAUN && !ygold &&
         (lepgold || rn2(2))) || (is_wanderer(mdat) && !rn2(4)) ||
        (Conflict && !mtmp->iswiz) || (!mtmp->mcansee && !rn2(4)) ||
        mtmp->mpeaceful) {
        /* Possibly cast an undirected spell if not attacking you */
        /* note that most of the time castmu() will pick a directed spell and
           do nothing, so the monster moves normally */
        /* arbitrary distance restriction to keep monster far away from you
           from having cast dozens of sticks-to-snakes or similar spells by the
           time you reach it */
        if (dist2(mtmp->mx, mtmp->my, u.ux, u.uy) <= 49 && !mtmp->mspec_used) {
            const struct attack *a;

            for (a = &mdat->mattk[0]; a < &mdat->mattk[NATTK]; a++) {
                if (a->aatyp == AT_MAGC &&
                    (a->adtyp == AD_SPEL || a->adtyp == AD_CLRC)) {
                    if (castmu(mtmp, a, 0)) {
                        break;
                    }
                }
            }
        }

        tmp = m_move(mtmp, 0);
        distfleeck(mtmp, &inrange, &nearby, &scared);   /* recalc */

        switch (tmp) {
        case 0:        /* no movement, but it can still attack you */
        case 3:        /* absolutely no movement */
            /* for pets, case 0 and 3 are equivalent */
            /* vault guard might have vanished */
            if (mtmp->isgd &&
                (mtmp->mhp < 1 || (mtmp->mx == COLNO && mtmp->my == ROWNO)))
                return 1;       /* behave as if it died */
            /* During hallucination, monster appearance should still change -
               even if it doesn't move. */
            if (Hallucination)
                newsym(mtmp->mx, mtmp->my);
            break;
        case 1:        /* monster moved */
            /* Maybe it stepped on a trap and fell asleep... */
            if (mtmp->msleeping || !mtmp->mcanmove)
                return 0;
            if (!nearby &&
                (ranged_attk(mdat) || find_offensive(mtmp, &musable)))
                break;
            else if (Engulfed && mtmp == u.ustuck) {
                /* a monster that's digesting you can move at the same time
                   -dlc */
                return mattacku(mtmp);
            } else
                return 0;
             /*NOTREACHED*/ break;
        case 2:        /* monster died */
            return 1;
        }
    }

    /* The other branch: attacking the player's apparent location. We jump to
       this immediately if no condition for not attacking (peaceful, outside
       melee range, etc.) is met. We also can end up here as a fallthrough,
       e.g. if a fleeing monster is stuck in a dead end, or a confused hostile
       monster stumbles into the player.

       At this point, we have established that the monster wants to either move
       to or attack the player's apparent location. We don't know which, and we
       don't know what's there. Stun and confusion are checked by m_move, which
       won't fall through here unless the player's apparent square happened to
       be selected by the movement randomizer. Thus, we do a hostile/conflict
       check in order to ensure that the monster is willing to attack, then tell
       it to attack the square it believes the player to be on. We also check to
       make sure that the monster's physically capable of attacking the square,
       and that the monster hasn't used its turn already (tmp == 3). */

    if (!mtmp->mpeaceful || (Conflict && !resist(mtmp, RING_CLASS, 0, 0))) {
        if (inrange && !noattacks(mdat) && u.uhp > 0 && !scared && tmp != 3 &&
            aware_of_u(mtmp))
            if (engulfing_u(mtmp) ? mattackq(mtmp, u.ux, u.uy) :
                mattackq(mtmp, mtmp->mux, mtmp->muy))
                return 1;       /* monster died (e.g. exploded) */

        if (mtmp->wormno)
            wormhitu(mtmp);
    }
    /* special speeches for quest monsters */
    if (!mtmp->msleeping && mtmp->mcanmove && nearby)
        quest_talk(mtmp);
    /* extra emotional attack for vile monsters */
    if (inrange && mtmp->data->msound == MS_CUSS && !mtmp->mpeaceful &&
        couldsee(mtmp->mx, mtmp->my) && !mtmp->minvis && !rn2(5))
        cuss(mtmp);

    return tmp == 2;
}

static const char practical[] =
    { WEAPON_CLASS, ARMOR_CLASS, GEM_CLASS, FOOD_CLASS, 0 };
static const char magical[] = {
    AMULET_CLASS, POTION_CLASS, SCROLL_CLASS, WAND_CLASS, RING_CLASS,
    SPBOOK_CLASS, 0
};

boolean
monster_would_take_item(struct monst *mtmp, struct obj *otmp)
{
    int pctload = (curr_mon_load(mtmp) * 100) / max_mon_load(mtmp);

    if (is_unicorn(mtmp->data) && objects[otmp->otyp].oc_material != GEMSTONE)
        return FALSE;

    if (!mindless(mtmp->data) && !is_animal(mtmp->data) && pctload < 75 &&
        searches_for_item(mtmp, otmp))
        return TRUE;
    if (likes_gold(mtmp->data) && otmp->otyp == GOLD_PIECE && pctload < 95)
        return TRUE;
    if (likes_gems(mtmp->data) && otmp->oclass == GEM_CLASS &&
        otmp->otyp != ROCK && pctload < 85)
        return TRUE;
    if (likes_objs(mtmp->data) && strchr(practical, otmp->oclass) &&
        pctload < 75)
        return TRUE;
    if (likes_magic(mtmp->data) && strchr(magical, otmp->oclass) &&
        pctload < 85)
        return TRUE;
    if (throws_rocks(mtmp->data) && otmp->otyp == BOULDER &&
        pctload < 50 && !In_sokoban(&(mtmp->dlevel->z)))
        return TRUE;
    /* note: used to check for artifacts, but this had side effects, also I'm
       not sure if gelatinous cubes understand the concept of artifacts
       anyway */
    if (mtmp->data == &mons[PM_GELATINOUS_CUBE] &&
        otmp->oclass != ROCK_CLASS && otmp->oclass != BALL_CLASS &&
        (otmp->otyp != CORPSE || !touch_petrifies(&mons[otmp->corpsenm])))
        return TRUE;

    return FALSE;
}

boolean
itsstuck(struct monst *mtmp)
{
    if (sticks(youmonst.data) && mtmp == u.ustuck && !Engulfed) {
        pline("%s cannot escape from you!", Monnam(mtmp));
        return TRUE;
    }
    return FALSE;
}

/*
 * Return values:
 * 0: Did not move, but can still attack and do other stuff.
 *    Returning this value will (in the current codebase) cause the monster to
 *    immediately attempt a melee or ranged attack on the player, if it's in a
 *    state (hostile/conflicted) in which it doesn't mind doing that, and it's
 *    on a map square from which it's physically capable of doing that.
 * 1: Moved, possibly can attack.
 *    This will only attempt an attack if a ranged attack is a possibility.
 * 2: Monster died.
 * 3: Did not move, and can't do anything else either.
 *
 * This function is only called in situations where the monster's first
 * preference is not a melee attack on the player.  Thus, a return value of 0
 * can be used to signify a melee attack on the player as a lesser preference,
 * e.g. when fleeing but stuck in a dead end.
 */
int
m_move(struct monst *mtmp, int after)
{
    int appr;
    xchar gx, gy, nix, niy;
    int chi;    /* could be schar except for stupid Sun-2 compiler */
    boolean can_tunnel = 0, can_open = 0, can_unlock = 0, doorbuster = 0;
    boolean setlikes = 0;
    boolean avoid = FALSE;
    const struct permonst *ptr;
    schar mmoved = 0;   /* not strictly nec.: chi >= 0 will do */
    long info[9];
    long flag;
    int omx = mtmp->mx, omy = mtmp->my;
    struct obj *mw_tmp;

    if (mtmp->mtrapped) {
        int i = mintrap(mtmp);

        if (i >= 2) {
            newsym(mtmp->mx, mtmp->my);
            return 2;
        }       /* it died */
        if (i == 1)
            return 0;   /* still in trap, so didn't move */
    }
    ptr = mtmp->data;   /* mintrap() can change mtmp->data -dlc */

    if (mtmp->meating) {
        mtmp->meating--;
        return 3;       /* still eating */
    }
    if (hides_under(ptr) && OBJ_AT(mtmp->mx, mtmp->my) && rn2(10))
        return 0;       /* do not leave hiding place */

    /* Note: we don't call set_apparxy() from here any more. When being called
       in the usual way, it was doubling the chances of the monster tracking the
       player. This means it might not be called if a leprechaun dodges on its
       first turn out, but we now ensure that muxy always has a sensible value,
       so nothing breaks. */

    if (!Is_rogue_level(&u.uz))
        can_tunnel = tunnels(ptr);
    can_open = !(nohands(ptr) || verysmall(ptr));
    can_unlock = ((can_open && m_carrying(mtmp, SKELETON_KEY)) || mtmp->iswiz ||
                  is_rider(ptr));
    doorbuster = is_giant(ptr);
    if (mtmp->wormno)
        goto not_special;
    /* my dog gets special treatment */
    if (mtmp->mtame) {
        mmoved = dog_move(mtmp, after);
        goto postmov;
    }

    /* likewise for shopkeeper */
    if (mtmp->isshk) {
        mmoved = shk_move(mtmp);
        if (mmoved == -2)
            return 2;
        if (mmoved >= 0)
            goto postmov;
        mmoved = 0;     /* follow player outside shop */
    }

    /* and for the guard */
    if (mtmp->isgd) {
        mmoved = gd_move(mtmp);
        if (mmoved == -2)
            return 2;
        if (mmoved >= 0)
            goto postmov;
        mmoved = 0;
    }

    /* and the acquisitive monsters get special treatment

       TODO: This plus tactics() are split between files in one of the more
       bizarre possible ways. We should consolidate them together and make sure
       that all the code is in m_move or dochug, not both. (I prefer m_move.)
       The current situation allows (in fact, forces) the Wizard of Yendor to
       teleport before moving. */
    if (is_covetous(ptr) && mtmp->mstrategy & STRAT_TARGMASK &&
        mtmp->mstrategy & STRAT_GOAL) {
        xchar tx = STRAT_GOALX(mtmp->mstrategy), ty =
            STRAT_GOALY(mtmp->mstrategy);
        struct monst *intruder = m_at(level, tx, ty);

        /*
         * if there's a monster on the object or in possession of it,
         * attack it.
         */
        if ((dist2(mtmp->mx, mtmp->my, tx, ty) < 2) && intruder &&
            (intruder != mtmp)) {

            notonhead = (intruder->mx != tx || intruder->my != ty);
            if (mattackm(mtmp, intruder) == 2)
                return 2;
            mmoved = 1;
        } else
            mmoved = 0;
        goto postmov;
    }

    /* and for the priest */
    if (mtmp->ispriest) {
        mmoved = pri_move(mtmp);
        if (mmoved == -2)
            return 2;
        if (mmoved >= 0)
            goto postmov;
        mmoved = 0;
    }

    /* teleport if that lies in our nature */
    if (ptr == &mons[PM_TENGU] && !rn2(5) && !mtmp->mcan &&
        !tele_restrict(mtmp)) {
        if (mtmp->mhp < 7 || mtmp->mpeaceful || rn2(2))
            rloc(mtmp, TRUE);
        else
            mnexto(mtmp);
        mmoved = 1;
        goto postmov;
    }
not_special:

    if (Engulfed && !mtmp->mflee && u.ustuck != mtmp)
        return 1;

    /* Work out where the monster is aiming, from strategy(). */
    omx = mtmp->mx;
    omy = mtmp->my;

    if (!isok(omx, omy))
        panic("monster AI run with an off-level monster: %s (%d, %d)",
              k_monnam(mtmp), omx, omy);

    if (mtmp->mstrategy & (STRAT_TARGMASK | STRAT_ESCAPE)) {
        gx = STRAT_GOALX(mtmp->mstrategy);
        gy = STRAT_GOALY(mtmp->mstrategy);
    } else {
        gx = mtmp->mx;
        gy = mtmp->my;
    }

    /* Calculate whether the monster wants to move towards or away from the goal
       (or neither). */
    appr = (mtmp->mflee || mtmp->mstrategy & STRAT_ESCAPE) ? -1 : 1;
    if (mtmp->mconf || (Engulfed && mtmp == u.ustuck) ||
        mtmp->mstrategy & STRAT_NONE)
        appr = 0;

    /* monsters with limited control of their actions */
    if (((monsndx(ptr) == PM_STALKER || ptr->mlet == S_BAT ||
          ptr->mlet == S_LIGHT) && !rn2(3)))
        appr = 0;

    if ((!mtmp->mpeaceful || !rn2(10)) && (!Is_rogue_level(&u.uz))) {
        boolean in_line = lined_up(mtmp) &&
            (distmin(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy) <=
             (throws_rocks(youmonst.data) ? 20 : ACURRSTR / 2 + 1));

        if (appr != 1 || !in_line)
            setlikes = TRUE;
    }

    /* don't tunnel if hostile and close enough to prefer a weapon */
    if (can_tunnel && needspick(ptr) &&
        ((!mtmp->mpeaceful || Conflict) &&
         dist2(mtmp->mx, mtmp->my, gx, gy) <= 8))
        can_tunnel = FALSE;

    nix = omx;
    niy = omy;
    flag = 0L;
    if (mtmp->mpeaceful && (!Conflict || resist(mtmp, RING_CLASS, 0, 0)))
        flag |= (ALLOW_SANCT | ALLOW_SSM);
    else
        flag |= ALLOW_MUXY;
    if (is_minion(ptr) || is_rider(ptr))
        flag |= ALLOW_SANCT;
    /* unicorn may not be able to avoid hero on a noteleport level */
    if (is_unicorn(ptr) && !level->flags.noteleport)
        flag |= NOTONL;
    if (passes_walls(ptr))
        flag |= (ALLOW_WALL | ALLOW_ROCK);
    if (passes_bars(ptr))
        flag |= ALLOW_BARS;
    if (can_tunnel)
        flag |= ALLOW_DIG;
    if (is_human(ptr) || ptr == &mons[PM_MINOTAUR])
        flag |= ALLOW_SSM;
    if (is_undead(ptr) && !noncorporeal(ptr))
        flag |= NOGARLIC;
    if (throws_rocks(ptr))
        flag |= ALLOW_ROCK;
    if (can_open)
        flag |= OPENDOOR;
    if (can_unlock)
        flag |= UNLOCKDOOR;
    if (doorbuster)
        flag |= BUSTDOOR;
    {
        int i, nx, ny, nearer, distance_tie;
        int cnt, chcnt;
        int ndist, nidist;
        coord poss[9];

        struct distmap_state ds;

        distmap_init(&ds, gx, gy, mtmp);

        cnt = mfndpos(mtmp, poss, info, flag);
        chcnt = 0;
        chi = -1;
        nidist = distmap(&ds, omx, omy);

        if (is_unicorn(ptr) && level->flags.noteleport) {
            /* on noteleport levels, perhaps we cannot avoid hero */
            for (i = 0; i < cnt; i++)
                if (!(info[i] & NOTONL))
                    avoid = TRUE;
        }

        for (i = 0; i < cnt; i++) {
            if (avoid && (info[i] & NOTONL))
                continue;
            nx = poss[i].x;
            ny = poss[i].y;

            nearer = ((ndist = distmap(&ds, nx, ny)) < nidist);
            distance_tie = (ndist == nidist);

            if ((appr == 1 && nearer) ||
                (appr == -1 && !nearer && !distance_tie) ||
                (appr && distance_tie && !rn2(++chcnt)) ||
                (!appr && !rn2(++chcnt)) || !mmoved) {
                nix = nx;
                niy = ny;
                nidist = ndist;
                chi = i;
                mmoved = 1;

                if (appr && !distance_tie)
                    chcnt = 1;
            }
        }
    }

    /* If the monster didn't get any nearer to where it was aiming then when it
       started, clear its strategy. Exception: if it /couldn't/ move, then no
       strategy is any better than any other. */
    int actual_appr = dist2(omx, omy, gx, gy) - dist2(nix, niy, gx, gy);
    if (((actual_appr >= 0 && appr < 0) || (actual_appr <= 0 && appr > 0)) &&
        mmoved && rn2(2)) {
        mtmp->mstrategy = STRAT_NONE;
        strategy(mtmp, FALSE);
    }

    if (mmoved) {
        if (mmoved == 1 && (u.ux != nix || u.uy != niy) && itsstuck(mtmp))
            return 3;

        if (((IS_ROCK(level->locations[nix][niy].typ) &&
              may_dig(level, nix, niy)) || closed_door(level, nix, niy)) &&
            mmoved == 1 && can_tunnel && needspick(ptr)) {
            if (closed_door(level, nix, niy)) {
                if (!(mw_tmp = MON_WEP(mtmp)) || !is_pick(mw_tmp) ||
                    !is_axe(mw_tmp))
                    mtmp->weapon_check = NEED_PICK_OR_AXE;
            } else if (IS_TREE(level->locations[nix][niy].typ)) {
                if (!(mw_tmp = MON_WEP(mtmp)) || !is_axe(mw_tmp))
                    mtmp->weapon_check = NEED_AXE;
            } else if (!(mw_tmp = MON_WEP(mtmp)) || !is_pick(mw_tmp)) {
                mtmp->weapon_check = NEED_PICK_AXE;
            }
            if (mtmp->weapon_check >= NEED_PICK_AXE && mon_wield_item(mtmp))
                return 3;
        }
        /* If ALLOW_MUXY is set, the monster thinks it's trying to attack you.

           In most cases, this codepath won't happen. There are two main AI
           codepaths in the game: "immediately commit to a melee attack", and
           "don't immediately commit to a melee attack". This function
           implements the latter codepath; however, in most cases where a
           monster would actually /want/ to attack the player, the former
           codepath would be used.

           So it's best to think of ALLOW_MUXY as meaning "the monster doesn't
           /mind/ attacking the player, and wants to go to (or accidentally went
           to due to confusion) the player's square".  We obviously can't move
           the monster in this case; if it doesn't mind attacking the player,
           and the player is in the way, it will attack.  A good example of this
           situation is a fleeing monster stuck in a dead end; the "fleeing"
           status causes it to not immediately commit to a melee attack, but the
           only direction to flee in is back past the player.

           Thus, the solution is simply to leave everything as is and return 0.
           This causes the other codepath - the one in which the monster attacks
           the player's apparent square - to run.

           In 3.4.3, this code was rather more complex; a bug in dochug meant
           that it wouldn't handle this case correctly if the player was
           displaced, and so the code for handling that case was placed in
           m_move instead.  The buggy codepath was still accessible via the
           immediately-commit AI, though.  4.3 uses the other alternative; we
           fall out to dochug, and used the correct code for monster/muxy combat
           from this function to replace the incorrect code there (although it's
           been factored out into a separate function, mattackq, in mhitq.c).
        */
        if (info[chi] & ALLOW_MUXY)
            return 0;

        /* We also have to take into account another situation: the situation
           where the monster doesn't believe it's attacking you, but selects
           your square to move onto by mistake. The solution to this in 3.4.3 is
           to alert the monster to your location (which makes sense), and to
           return 0 (which seems a little unfair, as it will then be able to
           immediately attack you, and unless peaceful, probably will).

           Because 4.3 aims to make player hidden-ness factor properly into the
           monster AI, the monster should at least lose its turn. We also add a
           message, partly for the changed mechanic, partly so players
           understand where the monster turn went. */
        if (nix == u.ux && niy == u.uy) {

            /* Exception: the monster has you engulfed. */
            if (engulfing_u(mtmp))
                return 0;

            mtmp->mux = u.ux;
            mtmp->muy = u.uy;

            /* Exception: if you've laid a trap for the monster. */
            if (u.uundetected)
                return 0;

            pline("%s bumps right into you!", Monnam(mtmp));
            return 3;
        }

        /* The monster may attack another based on 1 of 2 conditions:

           1 - It may be confused.

           2 - It may mistake the monster for your (displaced) image.

           Pets get taken care of above and shouldn't reach this code. Conflict
           does not turn on the mm_aggression flag, so isn't taken care of here
           either (although it affects this code by turning off the sanity
           checks on it, meanining that, say, a monster can kill itself via
           passive damage). */
        if (info[chi] & ALLOW_M)
            return mattackq(mtmp, nix, niy);

        /* The monster's moving to an empty space. */

        if (!m_in_out_region(mtmp, nix, niy))
            return 3;
        remove_monster(level, omx, omy);
        place_monster(mtmp, nix, niy);

        /* Place a segment at the old position. */
        if (mtmp->wormno)
            worm_move(mtmp);

    } else {
        if (is_unicorn(ptr) && rn2(2) && !tele_restrict(mtmp)) {
            rloc(mtmp, TRUE);
            return 1;
        }
        if (mtmp->wormno)
            worm_nomove(mtmp);
    }
postmov:
    if (mmoved == 1 || mmoved == 3) {
        boolean canseeit = isok(mtmp->mx, mtmp->my) &&
            cansee(mtmp->mx, mtmp->my);

        if (mmoved == 1) {
            newsym(omx, omy);   /* update the old position */
            if (mintrap(mtmp) >= 2) {
                if (mtmp->mx != COLNO)
                    newsym(mtmp->mx, mtmp->my);
                return 2;       /* it died */
            }
            ptr = mtmp->data;

            /* open a door, or crash through it, if you can */
            if (isok(mtmp->mx, mtmp->my)
                && IS_DOOR(level->locations[mtmp->mx][mtmp->my].typ)
                && !passes_walls(ptr)   /* doesn't need to open doors */
                && !can_tunnel   /* taken care of below */
                ) {
                struct rm *here = &level->locations[mtmp->mx][mtmp->my];
                boolean btrapped = (here->doormask & D_TRAPPED);

                if (here->doormask & (D_LOCKED | D_CLOSED) && amorphous(ptr)) {
                    if (flags.verbose && canseemon(mtmp))
                        pline("%s %s under the door.", Monnam(mtmp),
                              (ptr == &mons[PM_FOG_CLOUD] ||
                               ptr == &mons[PM_YELLOW_LIGHT])
                              ? "flows" : "oozes");
                } else if (here->doormask & D_LOCKED && can_unlock) {
                    if (btrapped) {
                        here->doormask = D_NODOOR;
                        newsym(mtmp->mx, mtmp->my);
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                        if (mb_trapped(mtmp))
                            return 2;
                    } else {
                        if (flags.verbose) {
                            if (canseeit)
                                pline("You see a door unlock and open.");
                            else
                                You_hear("a door unlock and open.");
                        }
                        here->doormask = D_ISOPEN;
                        /* newsym(mtmp->mx, mtmp->my); */
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                    }
                } else if (here->doormask == D_CLOSED && can_open) {
                    if (btrapped) {
                        here->doormask = D_NODOOR;
                        newsym(mtmp->mx, mtmp->my);
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                        if (mb_trapped(mtmp))
                            return 2;
                    } else {
                        if (flags.verbose) {
                            if (canseeit)
                                pline("You see a door open.");
                            else
                                You_hear("a door open.");
                        }
                        here->doormask = D_ISOPEN;
                        /* newsym(mtmp->mx, mtmp->my); *//* done below */
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                    }
                } else if (here->doormask & (D_LOCKED | D_CLOSED)) {
                    /* mfndpos guarantees this must be a doorbuster */
                    if (btrapped) {
                        here->doormask = D_NODOOR;
                        newsym(mtmp->mx, mtmp->my);
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                        if (mb_trapped(mtmp))
                            return 2;
                    } else {
                        if (flags.verbose) {
                            if (canseeit)
                                pline("You see a door crash open.");
                            else
                                You_hear("a door crash open.");
                        }
                        if (here->doormask & D_LOCKED && !rn2(2))
                            here->doormask = D_NODOOR;
                        else
                            here->doormask = D_BROKEN;
                        /* newsym(mtmp->mx, mtmp->my); *//* done below */
                        unblock_point(mtmp->mx, mtmp->my);  /* vision */
                    }
                    /* if it's a shop door, schedule repair */
                    if (*in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE))
                        add_damage(mtmp->mx, mtmp->my, 0L);
                }
            } else if (isok(mtmp->mx, mtmp->my) &&
                       level->locations[mtmp->mx][mtmp->my].typ == IRONBARS) {
                if (flags.verbose && canseemon(mtmp))
                    pline_once("%s %s %s the iron bars.", Monnam(mtmp),
                               /* pluralization fakes verb conjugation */
                               makeplural(locomotion(ptr, "pass")),
                               passes_walls(ptr) ? "through" : "between");
            }

            /* possibly dig */
            if (can_tunnel && mdig_tunnel(mtmp))
                return 2;       /* mon died (position already updated) */

            /* set also in domove(), hack.c */
            if (Engulfed && mtmp == u.ustuck &&
                (mtmp->mx != omx || mtmp->my != omy)) {
                /* If the monster moved, then update */
                u.ux0 = u.ux;
                u.uy0 = u.uy;
                u.ux = mtmp->mx;
                u.uy = mtmp->my;
                if (Punished) {
                    unplacebc();
                    placebc();
                }
                swallowed(0);
            } else if (isok(mtmp->mx, mtmp->my))
                newsym(mtmp->mx, mtmp->my);
        }
        if (isok(mtmp->mx, mtmp->my) && setlikes &&
            OBJ_AT(mtmp->mx, mtmp->my) && mtmp->mcanmove) {

            /* Maybe a rock mole just ate some metal object */
            if (metallivorous(ptr)) {
                if (meatmetal(mtmp) == 2)
                    return 2;   /* it died */
            }

            /* Maybe a cube ate just about anything */
            if (ptr == &mons[PM_GELATINOUS_CUBE]) {
                if (meatobj(mtmp) == 2)
                    return 2;   /* it died */
            }

            if (!*in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE) || !rn2(25)) {
                if (mpickstuff(mtmp))
                    mmoved = 3;
            }

            /* We can't condition this on being invisible any more; maybe a
               monster just picked up gold or an invocation item */
            newsym(mtmp->mx, mtmp->my);
            if (mtmp->wormno)
                see_wsegs(mtmp);
        }

        if (isok(mtmp->mx, mtmp->my) &&
            (hides_under(ptr) || ptr->mlet == S_EEL)) {
            /* Always set--or reset--mundetected if it's already hidden (just
               in case the object it was hiding under went away); usually set
               mundetected unless monster can't move.  */
            if (mtmp->mundetected ||
                (mtmp->mcanmove && !mtmp->msleeping && rn2(5)))
                mtmp->mundetected = (ptr->mlet != S_EEL) ?
                    OBJ_AT(mtmp->mx, mtmp->my) :
                    (is_pool(level, mtmp->mx, mtmp->my) &&
                     !Is_waterlevel(&u.uz));
            newsym(mtmp->mx, mtmp->my);
        }
        if (mtmp->isshk) {
            after_shk_move(mtmp);
        }
    }
    return mmoved;
}


boolean
closed_door(struct level * lev, int x, int y)
{
    return (boolean) (IS_DOOR(lev->locations[x][y].typ) &&
                      (lev->locations[x][y].doormask & (D_LOCKED | D_CLOSED)));
}

boolean
accessible(int x, int y)
{
    return (boolean) (ACCESSIBLE(level->locations[x][y].typ) &&
                      !closed_door(level, x, y));
}


/* decide where the monster thinks you are standing */
void
set_apparxy(struct monst *mtmp)
{
    int disp;
    boolean actually_adjacent = distmin(mtmp->mx, mtmp->my, u.ux, u.uy) <= 1;
    boolean loe = couldsee(mtmp->mx, mtmp->my);
    unsigned msense_status;

    /* do cheapest and/or most likely tests first */

    /* pet knows your smell; grabber still has hold of you */
    if (mtmp->mtame || mtmp == u.ustuck) {
        if (engulfing_u(mtmp)) {
            /* we don't use mux/muy for engulfers because having them set to
               a monster's own square causes chaos in several ways */
            mtmp->mux = COLNO;
            mtmp->muy = ROWNO;
        } else {
            mtmp->mux = u.ux;
            mtmp->muy = u.uy;
        }
        return;
    }

    /* monsters which are adjacent to you and actually know your location will
       continue to know it */
    if (knows_ux_uy(mtmp) && actually_adjacent) {
        mtmp->mux = u.ux;
        mtmp->muy = u.uy;
        return;
    }

    /* monsters won't track you beyond a certain range on some levels (mostly
       endgame levels), for balance */
    if (!mtmp->mpeaceful && mtmp->dlevel->flags.shortsighted &&
        dist2(mtmp->mx, mtmp->my, u.ux, u.uy) > (loe ? 144 : 36)) {
        mtmp->mux = COLNO;
        mtmp->muy = ROWNO;
        return;
    }

    /* expensive, but the result is used in a ton of checks and very likely */
    msense_status = msensem(mtmp, &youmonst);

    /* if the monster can't sense you via any normal means and doesn't have LOE,
       we don't give the monster random balance spoilers; otherwise, monsters
       outside LOE will seek out the player, which is bizarre */
    if (!loe && !(msense_status & ~MSENSE_ITEMMIMIC)) {
        mtmp->mux = COLNO;
        mtmp->muy = ROWNO;
        return;
    }

    /* Note: for balance reasons, monsters sometimes detect you even if
       invisible or displaced.

       In 3.4.3, there was a 1/3 chance of being detected if invisible and 1/4
       if displaced; also, you'd be ignored 10/11 of the time for monster
       movement purposes if invisible (meaning monsters would random-walk even
       though they knew where you were, which is a bit weird).

       Additionally, 3.4.3 had a codepath in an entirely unrelated part of the
       code (inside mfndpos) which caused your actual location to be detected
       when actually adjacent. However, the actual change that implemented this
       basically just turned off displacement; invisibility was implemented as
       always knowing where you were, and sometimes just ignoring it.

       In 4.3, we approximate the same rules but in a less inconsistent way:
       monsters can sense invisible or displaced players 1/3 or 1/1 of the time
       respectively if adjacent, and 1/11 or 1/4 of the time respectively if not
       adjacent.

       Additionally, displacement only works on vision-based methods of sensing
       the player. Monsters that are telepathic, warned, etc. won't be fooled by
       it. */

    disp = 0;
    if (!msense_status) {
        if (!rn2(actually_adjacent ? 3 : 11))
            disp = 0;
        else {
            /* monster has no idea where you are */
            mtmp->mux = COLNO;
            mtmp->muy = ROWNO;
            return;
        }
    } else if (Displaced && !(msense_status & MSENSE_ANYDETECT)) {
        /* TODO: As described above, this actually_adjacent check was moved from
           mfndpos. It's worth considering modifying it. The comment in its
           previous location in mfndpos should also be changed in that case. */
        disp = actually_adjacent ? 0 : !rn2(4) ? 0 : loe ? 2 : 1;
    } else
        disp = 0;

    if (!disp) {
        mtmp->mux = u.ux;
        mtmp->muy = u.uy;
        return;
    }

    int try_cnt = 0;

    /* Look for an appropriate place for the displaced image to appear. */

    int mx, my;
    do {
        if (++try_cnt > 200) {
            mx = u.ux;
            my = u.uy;
            break;
        }

        mx = u.ux - disp + rn2(2 * disp + 1);
        my = u.uy - disp + rn2(2 * disp + 1);
    } while (!isok(mx, my)
             || (mx == mtmp->mx && my == mtmp->my)
             || ((mx != u.ux || my != u.uy) && !passes_walls(mtmp->data) &&
                 (!ACCESSIBLE(level->locations[mx][my].typ) ||
                  (closed_door(level, mx, my) && !can_ooze(mtmp))))
             || !couldsee(mx, my));

    mtmp->mux = mx;
    mtmp->muy = my;
}


boolean
can_ooze(struct monst *mtmp)
{
    struct obj *chain, *obj;

    if (!amorphous(mtmp->data))
        return FALSE;

    chain = m_minvent(mtmp);

    for (obj = chain; obj; obj = obj->nobj) {
        int typ = obj->otyp;

        switch (obj->oclass) {
        case CHAIN_CLASS:
        case VENOM_CLASS:
            impossible("illegal object in monster's inventory");
            break;

        case WEAPON_CLASS:
            if (typ >= ARROW && typ <= BOOMERANG)
                break;

            if (typ >= DAGGER && typ <= CRYSKNIFE)
                break;

            if (typ == SLING)
                break;

            return FALSE;

        case ARMOR_CLASS:
            if (is_cloak(obj) || is_gloves(obj) || is_shirt(obj) ||
                typ == LEATHER_JACKET)
                break;

            return FALSE;

        case RING_CLASS:
        case AMULET_CLASS:
        case SCROLL_CLASS:
        case WAND_CLASS:
        case COIN_CLASS:
        case GEM_CLASS:
            break;

        case TOOL_CLASS:
            if (typ == SACK || typ == BAG_OF_HOLDING || typ == BAG_OF_TRICKS ||
                typ == OILSKIN_SACK) {
                /* stuff in bag: we'll assume the result is too thick, except for a
                * bag of holding which ignores its contents. */
                if (obj->cobj && typ != BAG_OF_HOLDING)
                    return FALSE;
                break;
            }

            if (typ == LEASH || typ == TOWEL || typ == BLINDFOLD)
                break;

            if (typ == STETHOSCOPE || typ == TIN_WHISTLE || typ == MAGIC_WHISTLE ||
                typ == MAGIC_MARKER || typ == TIN_OPENER || typ == SKELETON_KEY ||
                typ == LOCK_PICK || typ == CREDIT_CARD)
                break;

            return FALSE;

        case FOOD_CLASS:
            if (typ == CORPSE && verysmall(&mons[obj->corpsenm]))
                break;

            if (typ == FORTUNE_COOKIE || typ == CANDY_BAR || typ == PANCAKE ||
                typ == LEMBAS_WAFER || typ == LUMP_OF_ROYAL_JELLY)
                break;

            return FALSE;


        case POTION_CLASS:
        case SPBOOK_CLASS:
        case ROCK_CLASS:
        case BALL_CLASS:
            return FALSE;

        case RANDOM_CLASS:
        case ILLOBJ_CLASS:
        default:
            panic("illegal object class in can_ooze");
        }
    }
    return TRUE;
}

/*monmove.c*/
