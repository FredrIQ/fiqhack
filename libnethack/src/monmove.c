/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"
#include "artifact.h"

static int disturb(struct monst *);
static void distfleeck(struct monst *, int *, int *, int *);
static void watch_on_duty(struct monst *);


/* TRUE : mtmp died */
boolean
mb_trapped(struct monst *mtmp)
{
    if (cansee(mtmp->mx, mtmp->my))
        pline(msgc_levelsound, "KABOOM!!  You see a door explode.");
    else
        You_hear(msgc_levelsound, "a distant explosion.");
    wake_nearto(mtmp->mx, mtmp->my, 7 * 7);
    int dmg = rnd(15);
    if (!resists_stun(mtmp))
        set_property(mtmp, STUNNED, dmg, TRUE);
    mtmp->mhp -= dmg;
    if (mtmp->mhp <= 0) {
        mondied(mtmp);
        if (!DEADMONSTER(mtmp))      /* i.e. it lifesaved */
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

    if (mtmp->mpeaceful && in_town(u.ux, u.uy) && !blind(mtmp) &&
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
    /* BUG[?]: cansee() will not work on tiles seen by xray, thus check for
       previous seen by xray seperately. */
    boolean already_saw_xray = !!((sensemon(mtmp)) & MSENSE_XRAY);
    int x = mtmp->mx, y = mtmp->my;
    boolean already_saw_mon = canspotmon(mtmp);
    int rd = dochug(mtmp);

    /* a similar check is in monster_nearby() in hack.c */
    /* check whether hero notices monster and stops current activity */
    if (!rd && !Confusion && (!mtmp->mpeaceful || Hallucination) &&
        /* it's close enough to be a threat */
        distu(mtmp->mx, mtmp->my) <= (BOLT_LIM + 1) * (BOLT_LIM + 1) &&
        /* and either couldn't see it before, or it was too far away */
        (!already_saw_mon || (!couldsee(x, y) && !already_saw_xray) ||
         distu(x, y) > (BOLT_LIM + 1) * (BOLT_LIM + 1)) &&
        /* can see it now, or sense it and would normally see it

           TODO: This can spoil the existence of walls in dark areas. */
        (canseemon(mtmp) || (sensemon(mtmp) && couldsee(mtmp->mx, mtmp->my))) &&
        mtmp->mcanmove && !noattacks(mtmp->data) && !onscary(u.ux, u.uy, mtmp))
        action_interrupted();

    return rd;
}


boolean
onscary(int x, int y, const struct monst *mtmp)
{
    if (mx_eshk(mtmp) || mx_egd(mtmp) || mtmp->iswiz || blind(mtmp) ||
        mtmp->mpeaceful || mtmp->data->mlet == S_HUMAN || is_lminion(mtmp) ||
        mtmp->data == &mons[PM_ANGEL] || is_rider(mtmp->data) ||
        (mtmp->data->geno & G_UNIQ))
        return FALSE;

    return (boolean) (sobj_at(SCR_SCARE_MONSTER, level, x, y)
                      || (sengr_at("Elbereth", x, y) && flags.elbereth_enabled)
                      || (mtmp->data->mlet == S_VAMPIRE &&
                          IS_ALTAR(level->locations[x][y].typ)));
}

/* Returns a number to generate this turn based on regeneration rate.
   Base number of regeneration rate is 100 which means 1(HP|Pw)/turn.
   200 means twice that, 50 means regenerate 1 every other turn, etc. */
int
regeneration_by_rate(int regen_rate)
{
    int ret = regen_rate / 100;
    regen_rate %= 100;
    int movecount = moves % 100;
    int rate_counter = 0;
    int i;
    for (i = 0; i <= movecount; i++) {
        if (rate_counter >= 100)
            rate_counter -= 100;
        rate_counter += regen_rate;
    }
    if (rate_counter >= 100)
        ret++;
    return ret;
}

/* Returns the regeneration rate for the monster.
   Works on players. */
int
regen_rate(const struct monst *mon, boolean energy)
{
    int regen = 0;
    int role = monsndx(mon->data);
    if (mon == &youmonst)
        role = Role_switch;

    if (energy ? pw_regenerates(mon) : regenerates(mon))
        regen += 100;

    if (role == (energy ? PM_WIZARD : PM_HEALER))
        regen += 33;

    regen += 3 * m_mlev(mon);

    int attrib = acurr(mon, energy ? A_WIS : A_CON);

    if (!energy)
        attrib -= 5;

    if (!energy || attrib > 3)
        regen += 3 * attrib;
    if (regen < 1)
        regen = 1;

    return regen;
}

/* regenerate lost hit points */
void
mon_regen(struct monst *mon, boolean digest_meal)
{
    mon->mhp += regeneration_by_rate(regen_rate(mon, FALSE));
    if (mon->mhp > mon->mhpmax)
        mon->mhp = mon->mhpmax;

    mon->pw += regeneration_by_rate(regen_rate(mon, TRUE));
    if (mon->pw > mon->pwmax)
        mon->pw = mon->pwmax;

    if (mon->mspec_used)
        mon->mspec_used--;

    /* Why is this here? */
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
            pline(msgc_statusheal, "You get released!");
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
        enum msg_channel msgc = mtmp->mtame ? msgc_petneutral : msgc_monneutral;
        if (!mtmp->mflee && fleemsg && canseemon(mtmp) && !mtmp->mfrozen) {
            if (mtmp->data->mlet != S_MIMIC)
                pline(msgc, "%s turns to flee!", Monnam(mtmp));
            else
                pline(msgc, "%s mimics a chicken for a moment!", Monnam(mtmp));
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

    /* check for waitmask status change */
    if (mtmp->mstrategy == st_waiting &&
        (m_canseeu(mtmp) || mtmp->mhp < mtmp->mhpmax))
        mtmp->mstrategy = st_none;

    /* update quest status flags */
    quest_stat_check(mtmp);

    /* TODO: Quest leaders should really be affected by invisibility and
       displacement, but that's not only more of a balance change than I'm
       comfortable with, it also seems likely to introduce weird bugs. So this
       uses monnear and your real location. */
    if (!mtmp->mcanmove || idle(mtmp)) {
        if (Hallucination)
            newsym(mtmp->mx, mtmp->my);
        if (mtmp->mcanmove && mtmp->mstrategy == st_close &&
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

    if (mdat->msound == MS_SHRIEK && !um_dist(mtmp->mx, mtmp->my, 1))
        m_respond(mtmp);
    if (mdat == &mons[PM_MEDUSA] && couldsee(mtmp->mx, mtmp->my))
        m_respond(mtmp);
    if (DEADMONSTER(mtmp))
        return 1;       /* m_respond gaze can kill medusa */

    /* fleeing monsters might regain courage */
    if (mtmp->mflee && !mtmp->mfleetim && mtmp->mhp == mtmp->mhpmax && !rn2(25))
        mtmp->mflee = 0;

    /* if a player monster gets the amulet, it wants to ascend */
    /* TODO: make this work outside Astral (in which the monster heads there) */
    if (is_mplayer(mtmp->data) && mon_has_amulet(mtmp) &&
        Is_astralevel(m_mz(mtmp)) && mtmp->mstrategy != st_ascend) {
        mtmp->mstrategy = st_ascend;
        mtmp->sx = COLNO;
        mtmp->sy = ROWNO;
        int x, y;
        /* find coaligned altar */
        for (x = 0; x < COLNO; x++) {
            for (y = 0; y < ROWNO; y++) {
                if (mtmp->dlevel->locations[x][y].typ == ALTAR) {
                    int mask = mtmp->dlevel->locations[x][y].flags;
                    int alignment = mtmp->data->maligntyp;
                    if ((mask & AM_LAWFUL && alignment > 0) ||
                        (mask & AM_NEUTRAL && alignment == 0) ||
                        (mask & AM_CHAOTIC && alignment < 0)) {
                        mtmp->sx = x;
                        mtmp->sy = y;
                        break;
                    }
                }
            }
        }
        if (mtmp->sx == COLNO) { /* no altar...? */
            impossible("No coaligned altar on astral for player monster?");
            mtmp->mstrategy = st_none;
        }
    }
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

    /* item usage logic */
    if (find_item(mtmp, &musable)) {
        if (use_item(&musable) != 0)
            return 1;
    }

    /* Demonic Blackmail! */
    if (nearby && mdat->msound == MS_BRIBE && mtmp->mpeaceful && !mtmp->mtame &&
        !Engulfed) {
        if (u_helpless(hm_all))
            return 0; /* wait for you to be able to respond */
        if (!knows_ux_uy(mtmp)) {
            if (aware_of_u(mtmp)) {
                pline(msgc_npcvoice, "%s whispers at thin air.",
                      cansee(mtmp->mux, mtmp->muy) ? Monnam(mtmp) : "It");

                if (is_demon(youmonst.data)) {
                    /* "Good hunting, brother" */
                    if (!tele_restrict(mtmp))
                        rloc(mtmp, TRUE);
                } else {
                    set_property(mtmp, INVIS, -2, FALSE);
                    /* Why? For the same reason in real demon talk */
                    pline(msgc_npcanger, "%s gets angry!", Amonnam(mtmp));
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
        int dmg;
        struct monst *m2, *nmon = NULL;

        if (canseemon(mtmp))
            pline_implied(combat_msgc(mtmp, NULL, cr_hit),
                          "%s concentrates.", Monnam(mtmp));
        if (distu(mtmp->mx, mtmp->my) > BOLT_LIM * BOLT_LIM)
            pline(msgc_levelwarning,
                  "You sense a faint wave of psychic energy.");
        else {
            if (!mtmp->mpeaceful || (Conflict &&
                                     !resist(&youmonst, mtmp, RING_CLASS, 0, 0))) {
                boolean m_sen = sensemon(mtmp);

                if (!u.uinvulnerable &&
                    (m_sen || (Blind_telepat && rn2(2)) || !rn2(10))) {
                    pline(combat_msgc(mtmp, &youmonst, cr_hit),
                          "A wave of psychic energy locks on to your %s!", m_sen ?
                          "telepathy" : Blind_telepat ? "latent telepathy" :
                          "mind");
                    dmg = rnd(15);
                    if (Half_spell_damage)
                        dmg = (dmg + 1) / 2;
                    losehp(dmg, killer_msg(DIED, "a psychic blast"));
                }
                pline(combat_msgc(mtmp, &youmonst, cr_miss),
                      "A wave of psychic energy washes around you.");
            }
            else
                pline(msgc_petneutral, "Soothing psychic energy surrounds you.");
        }
        for (m2 = level->monlist; m2; m2 = nmon) {
            nmon = m2->nmon;
            if (DEADMONSTER(m2) ||
                m2->mpeaceful == mtmp->mpeaceful ||
                mindless(m2->data) ||
                m2 == mtmp ||
                dist2(mtmp->mx, mtmp->my, m2->mx, m2->my) > BOLT_LIM * BOLT_LIM)
                continue;
            if (msensem(m2, mtmp) || (telepathic(m2) && (rn2(2) || blind(m2))) ||
                !rn2(10)) {
                if (cansee(m2->mx, m2->my))
                    pline(combat_msgc(mtmp, m2, cr_hit),
                          "It locks on to %s.", mon_nam(m2));
                dmg = rnd(15);
                if (half_spell_dam(m2))
                    dmg = (dmg + 1) / 2;
                m2->mhp -= dmg;
                if (m2->mhp <= 0)
                    monkilled(mtmp, m2, "", AD_DRIN);
                else
                    m2->msleeping = 0;
            } else
                pline(combat_msgc(mtmp, m2, cr_miss),
                      "It fails to lock onto %s.", mon_nam(m2));
        }
    }

    /* If monster is nearby you, and has to wield a weapon, do so.  This costs
       the monster a move unless the monster already wields a weapon (similar
       to players being able to switch weapon for free with x) */
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
    if ((((attacktype(mtmp->data, AT_BREA) || attacktype(mtmp->data, AT_GAZE) ||
           attacktype(mtmp->data, AT_SPIT)) && !mtmp->mspec_used) ||
         (attacktype(mtmp->data, AT_WEAP) && select_rwep(mtmp) != 0)) &&
        mtmp->mlstmv != moves && mtmp->mstrategy != st_ascend) {
        struct monst *mtmp2 = mfind_target(mtmp, FALSE);

        if (mtmp2) {
            int tx = mtmp2->mx, ty = mtmp2->my;
            if (mtmp2 == &youmonst) {
                /* use muxy */
                tx = mtmp->mux;
                ty = mtmp->muy;
            }

            if (!monnear(mtmp, tx, ty)) {
                if (mattackq(mtmp, tx, ty) & MM_AGR_DIED)
                    return 1;       /* Oops. */

                return 0; /* that was our move for the round */
            }
        }
    }

    /* Now the actual movement phase */

    if (mdat->mlet == S_LEPRECHAUN) {
        ygold = findgold(youmonst.minvent);
        lepgold = findgold(mtmp->minvent);
    }

    boolean dragon = FALSE;
    if (mdat->mlet == S_DRAGON && extra_nasty(mdat))
        dragon = TRUE;

    /* We have two AI branches: "immediately attack the player's apparent
       location", and "don't immediately attack the player's apparent location"
       (in which case attacking the player's apparent location is still an
       option, but it'll only be taken if the player's in the monster's way).
       For the fallthroughs to work correctly, the "don't attack" branch comes
       first, and we decide to use it via this rather large if statement. */

    if (!nearby || mtmp->mflee || scared || confused(mtmp) || stunned(mtmp) ||
        (dragon && !mtmp->mspec_used &&
         mtmp->mhp <= (mtmp->mhpmax / 3)) || /* dragons at low HP step back to breathe */
        (invisible(mtmp) && !rn2(3)) ||
        (mdat->mlet == S_LEPRECHAUN && !ygold &&
         (lepgold || rn2(2))) || (is_wanderer(mdat) && !rn2(4)) ||
        (Conflict && !mtmp->iswiz) || (blind(mtmp) && !rn2(4)) ||
        mtmp->mpeaceful || mtmp->mstrategy == st_ascend) {
        tmp = m_move(mtmp, 0);
        distfleeck(mtmp, &inrange, &nearby, &scared);   /* recalc */

        switch (tmp) {
        case 0:        /* no movement, but it can still attack you */
        case 3:        /* absolutely no movement */
            /* for pets, case 0 and 3 are equivalent */
            /* vault guard might have vanished */
            if (mx_egd(mtmp) &&
                (DEADMONSTER(mtmp) || (mtmp->mx == COLNO && mtmp->my == ROWNO)))
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

    if (!mtmp->mpeaceful || (Conflict && !resist(&youmonst, mtmp, RING_CLASS, 0, 0))) {
        if (nearby && !noattacks(mdat) && u.uhp > 0 && !scared && tmp != 3 &&
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
        couldsee(mtmp->mx, mtmp->my) && !invisible(mtmp) && !rn2(5))
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
    /* monsters don't pickup corpses apart from the ones that searches_for_item
       lets through */
    if (otmp->otyp == CORPSE)
        return FALSE;
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
        pline(combat_msgc(&youmonst, mtmp, cr_hit),
              "%s cannot escape from you!", Monnam(mtmp));
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
 * 1: Moved
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
    long info[ROWNO * COLNO];
    long flag;
    int omx = mtmp->mx, omy = mtmp->my;
    struct obj *mw_tmp;
    struct musable unlocker;

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

    /* Dragon pathfinding is a bit special */
    boolean dragon = FALSE;

    /* extra_nasty skips baby dragons */
    if (ptr->mlet == S_DRAGON && extra_nasty(ptr))
        dragon = TRUE;

    if (mtmp->meating) {
        mtmp->meating--;
        return 3;       /* still eating */
    }
    if (hides_under(ptr) && OBJ_AT(mtmp->mx, mtmp->my) && rn2(10) && !mtmp->mpeaceful)
        return 0;       /* do not leave hiding place */

    /* Note: we don't call set_apparxy() from here any more. When being called
       in the usual way, it was doubling the chances of the monster tracking the
       player. This means it might not be called if a leprechaun dodges on its
       first turn out, but we now ensure that muxy always has a sensible value,
       so nothing breaks. */

    /* If the monster is a steed and no longer tame, force a dismount */
    if (mtmp == u.usteed && !mtmp->mtame) {
        dismount_steed(DISMOUNT_THROWN);
        return 3;
    }

    if (!Is_rogue_level(&u.uz))
        can_tunnel = tunnels(ptr);
    can_open = !(nohands(ptr) || verysmall(ptr));
    can_unlock = (mtmp->iswiz || is_rider(ptr));
    if (!can_unlock) /* Use muse logic to find knock, opening or keys */
        can_unlock = (can_open && find_unlocker(mtmp, &unlocker));
    doorbuster = is_giant(ptr);
    if (mtmp->wormno)
        goto not_special;

    /* my dog gets special treatment */
    if (mtmp->mtame) {
        mmoved = dog_move(mtmp, after);
        goto postmov;
    }

    /* likewise for shopkeeper */
    if (mx_eshk(mtmp)) {
        mmoved = shk_move(mtmp);
        if (mmoved == -2)
            return 2;
        if (mmoved >= 0)
            goto postmov;
        mmoved = 0;     /* follow player outside shop */
    }

    /* and for the guard */
    if (mx_egd(mtmp)) {
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
    if (is_covetous(ptr) && st_target(mtmp)) {
        struct monst *intruder = m_at(level, mtmp->sx, mtmp->sy);

        /*
         * if there's a monster on the object or in possession of it,
         * attack it.
         */
        if ((dist2(mtmp->mx, mtmp->my, mtmp->sx, mtmp->sy) < 2) &&
            intruder && (intruder != mtmp)) {

            notonhead = (intruder->mx != mtmp->sx ||
                         intruder->my != mtmp->sy);
            if (mattackm(mtmp, intruder) == 2)
                return 2;
            mmoved = 1;
        } else
            mmoved = 0;
        goto postmov;
    }

    /* and for the priest */
    if (ispriest(mtmp)) {
        mmoved = pri_move(mtmp);
        if (mmoved == -2)
            return 2;
        if (mmoved >= 0)
            goto postmov;
        mmoved = 0;
    }

    /* check if there is a good reason to teleport at will, or occasioally
       do it anyway */
    if (teleport_at_will(mtmp) && mtmp->pw >= 15 && !cancelled(mtmp) &&
        !tele_wary(mtmp) && mtmp->mstrategy != st_ascend) {
        if (mtmp->mstrategy == st_escape || st_target(mtmp) ||
            !rn2(25)) {
            mtmp->pw -= 15;
            mmoved = 1;
            mon_tele(mtmp, !!teleport_control(mtmp));
        }
        if (mmoved)
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

    if (mtmp->mstrategy == st_escape || st_target(mtmp)) {
        gx = mtmp->sx;
        gy = mtmp->sy;
    } else {
        gx = mtmp->mx;
        gy = mtmp->my;
    }

    /* Calculate whether the monster wants to move towards or away from the goal
       (or neither). */
    appr = (mtmp->mflee || mtmp->mstrategy == st_escape) ? -1 : 1;
    if (confused(mtmp) || (Engulfed && mtmp == u.ustuck) ||
        mtmp->mstrategy == st_none)
        appr = 0;

    /* monsters with limited control of their actions */
    if (((monsndx(ptr) == PM_STALKER || ptr->mlet == S_BAT ||
          ptr->mlet == S_LIGHT) && !rn2(3)))
        appr = 0;

    if ((!mtmp->mpeaceful || !rn2(10)) && (!Is_rogue_level(&u.uz))) {
        boolean in_line = lined_up(mtmp) &&
            (distmin(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy) <=
             (throws_rocks(youmonst.data) ? 20 : ACURR(A_STR) / 2 + 1));

        if (appr != 1 || !in_line)
            setlikes = TRUE;
        /* Chest traps can kill */
        if (setlikes && !mtmp->mtame && mpickstuff(mtmp, FALSE))
            return DEADMONSTER(mtmp) ? 2 : 3;
    }

    /* 20% of the monster's actions, check for better gear */
    if (!rn2(5)) {
        m_dowear(mtmp, FALSE);
        if (!mtmp->mcanmove) /* putting on armor */
            return 3;
    }

    /* don't tunnel if hostile and close enough to prefer a weapon */
    if (can_tunnel && needspick(ptr) &&
        ((!mtmp->mpeaceful || Conflict) &&
         dist2(mtmp->mx, mtmp->my, gx, gy) <= 8))
        can_tunnel = FALSE;

    int jump = jump_ok(mtmp);

    nix = omx;
    niy = omy;
    flag = 0L;
    if (mtmp->mpeaceful && (!Conflict || resist(&youmonst, mtmp, RING_CLASS, 0, 0)))
        flag |= (ALLOW_SANCT | ALLOW_SSM);
    else
        flag |= ALLOW_MUXY;
    if (!mtmp->mpeaceful || mtmp->mtame)
        flag |= ALLOW_PEACEFUL;
    if (pm_isminion(ptr) || is_rider(ptr) || is_mplayer(ptr))
        flag |= ALLOW_SANCT;
    /* unicorn may not be able to avoid hero on a noteleport level */
    if (is_unicorn(ptr) && !level->flags.noteleport)
        flag |= NOTONL;
    if (phasing(mtmp))
        flag |= (ALLOW_WALL | ALLOW_ROCK);
    if (passes_bars(mtmp))
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
    if (jump)
        flag |= ALLOW_JUMP;
    {
        int i, nx, ny, nearer, distance_tie;
        int cnt, chcnt;
        int ndist, nidist;
        coord poss[ROWNO * COLNO];
        int ogx = gx;
        int ogy = gy;
        boolean forceline = FALSE;

        struct distmap_state ds;

        /* Dragons try to avoid melee initiative, but there's no reason to
           avoid it if their target is helpless */
        boolean thelpless = FALSE;
        if (dragon && mtmp->mstrategy == st_mon) {
            if (gx == mtmp->mux && gy == mtmp->muy) {
                /* player is target */
                if (u_helpless(hm_all))
                    thelpless = TRUE;
            } else {
                struct monst *targetmon = m_at(mtmp->dlevel, gx, gy);
                if (targetmon && m_helpless(targetmon, hm_all))
                    thelpless = TRUE;
            }
        }

        /* Some monsters try to lineup but as far away as possible */
        if (((ptr->mflags3 & M3_LINEUP) ||
             (dragon && !mtmp->mspec_used)) &&
            mtmp->mstrategy == st_mon) {
            find_best_lineup(mtmp, &gx, &gy);
            if (gx != ogx || gy != ogy)
                forceline = TRUE;
        }

        distmap_init(&ds, gx, gy, mtmp);

        cnt = mfndpos(mtmp, poss, info, flag, jump ? jump : 1);
        chcnt = 0;
        chi = -1;
        if (flag & OPENDOOR)
            ds.mmflags |= MM_IGNOREDOORS;
        if (flag & ALLOW_PEACEFUL)
            ds.mmflags |= MM_IGNOREPEACE;
        nidist = distmap(&ds, omx, omy);

        /* Check if we can actually force lineup */
        if (forceline) {
            forceline = FALSE;
            for (i = 0; i < cnt; i++) {
                if (info[i] & NOTONL) {
                    forceline = TRUE;
                    break;
                }
            }
        } else if ((dragon && mtmp->mspec_used && !thelpless) ||
                   (is_unicorn(ptr) && level->flags.noteleport)) {
            /* On noteleport, perhaps we can't avoid lineup */
            for (i = 0; i < cnt; i++) {
                if (!(info[i] & NOTONL)) {
                    avoid = TRUE;
                    break;
                }
            }

            /* Dragons not already in line will always avoid being
               in line */
            if (dragon && mtmp->mspec_used &&
                !linedup(mtmp->mx, mtmp->my, gx, gy))
                avoid = TRUE;
        }

        /* Do 2 passes. Don't check for jumping on the 2nd pass, in case
           our jumping attempt failed (spellcast failure) */
        int pass = 2;
        while (pass--) {
            for (i = 0; i < cnt; i++) {
                if (avoid && (info[i] & NOTONL))
                    continue;
                else if (forceline && !(info[i] & NOTONL))
                    continue;
                else if (!pass && (info[i] & ALLOW_JUMP))
                    continue;

                nx = poss[i].x;
                ny = poss[i].y;

                ndist = distmap(&ds, nx, ny);

                nearer = (ndist < nidist);
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

            if (mmoved && info[chi] & ALLOW_JUMP) {
                int jumpres = mon_jump(mtmp, nix, niy);
                if (jumpres)
                    return jumpres;
                mmoved = 0;
            } else
                break;
        }
    }

    /* If the monster didn't get any nearer to where it was aiming then when it
       started, clear its strategy. Exception: if it /couldn't/ move, then no
       strategy is any better than any other. */
    int actual_appr = dist2(omx, omy, gx, gy) - dist2(nix, niy, gx, gy);
    if (((actual_appr >= 0 && appr < 0) || (actual_appr <= 0 && appr > 0)) &&
        mmoved && rn2(2)) {
        mtmp->mstrategy = st_none;
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

            pline(combat_msgc(mtmp, &youmonst, cr_miss),
                  "%s bumps right into you!", Monnam(mtmp));
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

        /* Check for door at target location */
        /* TODO: simplify */

        if ((can_unlock || can_open) &&
            IS_DOOR(level->locations[nix][niy].typ) &&
            !mtmp->iswiz && !is_rider(mtmp->data) && /* cheats */
            !phasing(mtmp) && !amorphous(mtmp->data)) { /* bypass doors */
            struct rm *door = &level->locations[nix][niy];
            boolean btrapped = (door->flags & D_TRAPPED);
            if ((door->flags & D_LOCKED) && can_unlock) {
                /* doorbusters are taken care of in postmov */
                unlocker.x = nix - mtmp->mx;
                unlocker.y = niy - mtmp->my;
                unlocker.z = 0;
                if (use_item(&unlocker) == 1) /* died */
                    return 2;
                return 3;
            }
            if (door->flags == D_CLOSED && can_open) {
                if (btrapped) {
                    door->flags = D_NODOOR;
                    newsym(nix, niy);
                    unblock_point(nix, niy);      /* vision */
                    if (mb_trapped(mtmp))
                        return 2;
                } else {
                    if (flags.verbose) {
                        if (cansee(nix, niy))
                            pline(msgc_levelwarning,
                                  "You see a door open.");
                        else
                            You_hear(msgc_levelwarning,
                                     "a door open.");
                    }
                    door->flags = D_ISOPEN;
                    newsym(nix, niy);
                    unblock_point(nix, niy);
                }
                return 3;
            }
            /* doorbusters are taken care of in postmov */
        }

        struct monst *dmon = m_at(level, nix, niy);
        if ((info[chi] & ALLOW_PEACEFUL) && dmon) {
            if (cansee(mtmp->mx, mtmp->my) ||
                cansee(dmon->mx, dmon->my))
                pline_once(msgc_monneutral, "%s %s.",
                           M_verbs(mtmp, "displace"),
                           mon_nam(dmon));
            remove_monster(level, omx, omy);
            remove_monster(level, nix, niy);
            place_monster(mtmp, nix, niy, TRUE);
            place_monster(dmon, omx, omy, TRUE);
        } else {
            remove_monster(level, omx, omy);
            place_monster(mtmp, nix, niy, TRUE);
        }

        /* Place a segment at the old position. */
        if (mtmp->wormno)
            worm_move(mtmp);

    } else {
        if (is_unicorn(ptr) && rn2(2) && !tele_restrict(mtmp)) {
            rloc(mtmp, TRUE);
            return 1;
        }

        /* Update displaced image anyway */
        if (displacement(mtmp))
            update_displacement(mtmp);
        set_displacement(mtmp);

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
                && !phasing(mtmp)   /* doesn't need to open doors */
                && !can_tunnel   /* taken care of below */
                ) {
                struct rm *here = &level->locations[mtmp->mx][mtmp->my];
                boolean btrapped = (here->flags & D_TRAPPED);

                if (here->flags & (D_LOCKED | D_CLOSED) && amorphous(ptr)) {
                    /* monneutral even for pets; basically nothing is happening
                       here */
                    if (canseemon(mtmp))
                        pline(msgc_monneutral, "%s %s under the door.",
                              Monnam(mtmp),
                              (ptr == &mons[PM_FOG_CLOUD] ||
                               ptr == &mons[PM_YELLOW_LIGHT])
                              ? "flows" : "oozes");
                } else if (here->flags & D_LOCKED && can_unlock) {
                    if (btrapped) {
                        here->flags = D_NODOOR;
                        newsym(mtmp->mx, mtmp->my);
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                        if (mb_trapped(mtmp))
                            return 2;
                    } else {
                        if (canseeit)
                            pline(msgc_monneutral,
                                  "You see a door unlock and open.");
                        else
                            You_hear(msgc_levelsound,
                                     "a door unlock and open.");
                        here->flags = D_ISOPEN;
                        /* newsym(mtmp->mx, mtmp->my); */
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                    }
                } else if (here->flags == D_CLOSED && can_open) {
                    if (btrapped) {
                        here->flags = D_NODOOR;
                        newsym(mtmp->mx, mtmp->my);
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                        if (mb_trapped(mtmp))
                            return 2;
                    } else {
                        if (canseeit)
                            pline(msgc_monneutral, "You see a door open.");
                        else
                            You_hear(msgc_levelsound, "a door open.");
                        here->flags = D_ISOPEN;
                        /* newsym(mtmp->mx, mtmp->my); *//* done below */
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                    }
                } else if (here->flags & (D_LOCKED | D_CLOSED)) {
                    /* mfndpos guarantees this must be a doorbuster */
                    if (btrapped) {
                        here->flags = D_NODOOR;
                        newsym(mtmp->mx, mtmp->my);
                        unblock_point(mtmp->mx, mtmp->my);      /* vision */
                        if (mb_trapped(mtmp))
                            return 2;
                    } else {
                        if (canseeit)
                            pline(msgc_monneutral,
                                  "You see a door crash open.");
                        else
                            You_hear(msgc_levelsound, "a door crash open.");
                        if (here->flags & D_LOCKED && !rn2(2))
                            here->flags = D_NODOOR;
                        else
                            here->flags = D_BROKEN;
                        /* newsym(mtmp->mx, mtmp->my); *//* done below */
                        unblock_point(mtmp->mx, mtmp->my);  /* vision */
                    }
                    /* if it's a shop door, schedule repair */
                    if (*in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE))
                        add_damage(mtmp->mx, mtmp->my, 0L);
                }
            } else if (isok(mtmp->mx, mtmp->my) &&
                       level->locations[mtmp->mx][mtmp->my].typ == IRONBARS) {
                if (canseemon(mtmp))
                    pline_once(msgc_monneutral,
                               "%s %s %s the iron bars.", Monnam(mtmp),
                               /* pluralization fakes verb conjugation */
                               makeplural(locomotion(ptr, "pass")),
                               phasing(mtmp) ? "through" : "between");
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
        if (isok(mtmp->mx, mtmp->my) &&
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

            if (!*in_rooms(level, mtmp->mx, mtmp->my, SHOPBASE) &&
                !mtmp->mtame && mpickstuff(mtmp, TRUE))
                mmoved = 3;

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
        if (mx_eshk(mtmp)) {
            after_shk_move(mtmp);
        }
    }
    return mmoved;
}


boolean
closed_door(struct level * lev, int x, int y)
{
    return (boolean) (IS_DOOR(lev->locations[x][y].typ) &&
                      (lev->locations[x][y].flags & (D_LOCKED | D_CLOSED)));
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

    /* if you aren't on the level, then the monster can't sense you */
    if (mtmp->dlevel != level) {
        mtmp->mux = COLNO;
        mtmp->muy = ROWNO;
        return;
    }

    boolean actually_adjacent = distmin(mtmp->mx, mtmp->my, u.ux, u.uy) <= 1;
    boolean loe = couldsee(mtmp->mx, mtmp->my);
    unsigned msense_status;

    /* do cheapest and/or most likely tests first */

    /* pet knows your smell; grabber still has hold of you */
    if (mtmp->mtame || mtmp == u.ustuck) {
        if (engulfing_u(mtmp) || mtmp == u.usteed) {
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
             || ((mx != u.ux || my != u.uy) && !phasing(mtmp) &&
                 (!ACCESSIBLE(level->locations[mx][my].typ) ||
                  (closed_door(level, mx, my) && !can_ooze(mtmp))))
             || !couldsee(mx, my));

    mtmp->mux = mx;
    mtmp->muy = my;
}


boolean
can_ooze(const struct monst *mtmp)
{
    struct obj *chain, *obj;

    if (!amorphous(mtmp->data))
        return FALSE;

    chain = mtmp->minvent;

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
