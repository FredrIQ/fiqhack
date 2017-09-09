/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-02-19 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int poly_zapped;

static void costly_cancel(struct obj *);
static void polyuse(struct obj *, int, int);
static void create_polymon(struct obj *, int);
static boolean zap_updown(struct monst *, struct obj *, schar);
static void zap_hit_mon(struct monst *, struct monst *,
                        int, int, int, boolean);
static void revive_egg(struct obj *);
static boolean zap_steed(struct obj *);
static void cancel_item(struct obj *);
static void destroy_item(int, int);
static boolean obj_shudders(struct obj *);
static void do_osshock(struct obj *);
static void bhit(struct monst *, int, int, int, struct obj *);
static int zap_hit_check(int, int);
static int spell_hit_bonus(int);

#define ZT_MAGIC_MISSILE        (AD_MAGM-1)
#define ZT_FIRE                 (AD_FIRE-1)
#define ZT_COLD                 (AD_COLD-1)
#define ZT_SLEEP                (AD_SLEE-1)
#define ZT_DEATH                (AD_DISN-1)     /* or disintegration */
#define ZT_LIGHTNING            (AD_ELEC-1)
#define ZT_POISON_GAS           (AD_DRST-1)
#define ZT_ACID                 (AD_ACID-1)
#define ZT_STUN                 (AD_STUN-1)
/* 9 is currently unassigned */

#define ZT_WAND(x)              (x)
#define ZT_SPELL(x)             (10+(x))
#define ZT_BREATH(x)            (20+(x))

#define is_hero_spell(type)     ((type) >= 10 && (type) < 20)

const char *const flash_types[] = {   /* also used in buzzmu(mcastu.c) */
    "magic missile",    /* Wands must be 0-9 */
    "bolt of fire",
    "bolt of cold",
    "sleep ray",
    "death ray",
    "bolt of lightning",
    "",
    "",
    "",
    "",

    "magic missile",    /* Spell equivalents must be 10-19 */
    "fireball",
    "cone of cold",
    "sleep ray",
    "finger of death",
    "bolt of lightning",        /* There is no spell, used for retribution */
    "noxious energy",
    "acid stream",
    "stun ray",
    "",

    "blast of missiles",        /* Dragon breath equivalents 20-29 */
    "blast of fire",
    "blast of frost",
    "blast of sleep gas",
    "blast of disintegration",
    "blast of lightning",
    "poison cloud",
    "blast of acid",
    "blast of disorientation",
    ""
};

/* Routines for IMMEDIATE wands and spells. */
/* bhitm: monster mdef was hit by the effect of wand or spell otmp
   by magr. range is used for wands of light and expensive cameras to
   determine how strong blindness. */
int
bhitm(struct monst *magr, struct monst *mdef, struct obj *otmp, int range)
{
    boolean wake = TRUE;        /* Most 'zaps' should wake monster */
    boolean reveal_invis = FALSE;
    boolean known = FALSE;
    boolean dbldam = Role_if(PM_KNIGHT) && Uhave_questart && magr == &youmonst;
    boolean useen = (magr == &youmonst || canseemon(magr));
    boolean tseen = (mdef == &youmonst || canseemon(mdef));
    boolean yours = (magr == &youmonst);
    boolean hityou = (mdef == &youmonst);
    boolean selfzap = (magr == mdef);
    int dmg, otyp = otmp->otyp;
    const char *zap_type_text = "force bolt";
    struct obj *obj;
    int wandlevel = 0;
    if (otmp->oclass == WAND_CLASS) {
        wandlevel = getwandlevel(magr, otmp);
        zap_type_text = "wand";
    }
    boolean hostile;
    boolean tame;
    if (magr->mtame) {
        hostile = !mdef->mpeaceful;
        tame = mdef->mtame;
    } else if (magr->mpeaceful) {
        hostile = FALSE;
        tame = mdef->mpeaceful;
    } else {
        hostile = mdef->mtame;
        tame = !mdef->mpeaceful;
    }
    boolean mlevel = mdef->m_lev;
    boolean disguised_mimic = (mdef->data->mlet == S_MIMIC &&
                               mdef->m_ap_type != M_AP_NOTHING);

    if (Engulfed && mdef == u.ustuck)
        reveal_invis = FALSE;

    if (hityou && u.uinvulnerable) {
        pline(combat_msgc(magr, mdef, cr_miss), "You're unaffected.");
        return 0;
    }
    if (hityou)
        action_interrupted();

    switch (otyp) {
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        reveal_invis = TRUE;
        if (resists_magm(mdef)) {
            shieldeff(mdef->mx, mdef->my);
            if (hityou || tseen)
                pline(msgc_yafm, "Boing!");
            break;      /* skip makeknown */
        } else if (selfzap || (yours && Engulfed) ||
                   wandlevel || rnd(20) < 10 + find_mac(mdef)) {
            dmg = dice(2, 12);
            if (wandlevel == P_MASTER)
                dmg = dice(8, 12);
            else if (wandlevel)
                dmg = dice(wandlevel, 12);
            if (dbldam)
                dmg *= 2;
            if (otyp == SPE_FORCE_BOLT && yours)
                dmg += spell_damage_bonus();
            if (useen || tseen) {
                if (!selfzap)
                    hit(zap_type_text, mdef, exclam(dmg), magr);
                else
                    pline(msgc_yafm, "%s %sself!", M_verbs(mdef, "bash"),
                          hityou ? "your" : mhim(mdef));
            }
            if (hityou) {
                const char *kbuf;
                kbuf = msgprintf("%s%s %s", yours ? mhis(magr) : s_suffix(k_monnam(magr)),
                                yours ? " own" : "", zap_type_text);
                losehp(dmg, killer_msg(DIED, kbuf));
            } else {
                if (resist(magr, mdef, otmp->oclass, TELL, bcsign(otmp)))
                    dmg = dmg / 2 + 1;
                mdef->mhp -= dmg;
                if (mdef->mhp <= 0)
                    monkilled(magr, mdef, "", AD_RBRE);
            }
        } else
            miss(zap_type_text, mdef, magr);
        known = TRUE;
        break;
    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
        if (wandlevel)
            inc_timeout(mdef, SLOW, rn1((4 * wandlevel), (6 * wandlevel)), FALSE);
        else
            inc_timeout(mdef, SLOW, dice(3, 8), FALSE);
        if (wandlevel == P_MASTER)
            set_property(mdef, FAST, -2, TRUE);
        else if (wandlevel >= P_SKILLED)
            set_property(mdef, FAST, -1, TRUE);
        if (!hityou)
            m_dowear(mdef, FALSE);      /* might want speed boots */
        if (Engulfed && (mdef == u.ustuck) && is_whirly(mdef->data)) {
            if (yours)
                pline(msgc_statusheal, "You disrupt %s!", mon_nam(mdef));
            else
                pline(msgc_statusheal, "%s is disrupted!", Monnam(mdef));
            pline_implied(msgc_statusheal, "A huge hole opens up...");
            expels(mdef, mdef->data, TRUE);
            known = TRUE;
        }
        break;
    case WAN_SPEED_MONSTER:
        dmg = dice(2, 20);
        if (wandlevel >= P_BASIC)
            dmg += 20;
        if (wandlevel >= P_EXPERT)
            dmg += 30;
        if (wandlevel == P_MASTER) {
            dmg += 50;
            dmg += dice(3, 20);
        }
        known = inc_timeout(mdef, FAST, dmg, FALSE);
        if (wandlevel >= P_SKILLED) {
            set_property(mdef, FAST, 0, FALSE);
            exercise(A_DEX, TRUE);
        }
        break;
    case WAN_UNDEAD_TURNING:
    case SPE_TURN_UNDEAD:
        if (tseen)
            known = TRUE;

        wake = FALSE;
        if (wandlevel >= P_BASIC && unturn_dead(mdef))
            wake = TRUE;
        if (is_undead(mdef->data)) {
            reveal_invis = TRUE;
            wake = TRUE;
            known = TRUE;
            flags.bypasses = TRUE;      /* for make_corpse() */
            if (hityou) {
                if (wandlevel < P_SKILLED) {
                    pline(combat_msgc(magr, mdef, cr_hit),
                          Stunned ? "You struggle to keep your balance." :
                          "You reel...");
                    dmg = dice(ACURR(A_DEX) < 12 ? 6 : 4, 4);
                    if (Half_spell_damage)
                        dmg = (dmg + 1) / 2;
                    inc_timeout(&youmonst, STUNNED, dmg, TRUE);
                } else {
                    if (Upolyd)
                        rehumanize(DIED, "a turn undead effect");
                    else
                        done(DIED, "a turn undead effect");
                }
            } else if ((wandlevel < P_SKILLED &&
                        !resist(magr, mdef, otmp->oclass, NOTELL, bcsign(otmp))) ||
                       (wandlevel >= P_SKILLED &&
                        resist(magr, mdef, otmp->oclass, NOTELL, bcsign(otmp)))) {
                if (tseen) {
                    if (stunned(mdef))
                        pline(combat_msgc(magr, mdef, cr_hit),
                              "%s struggles to keep %s balance.", Monnam(mdef),
                              mhis(mdef));
                    else
                        pline(combat_msgc(magr, mdef, cr_hit),
                              "%s reels...", Monnam(mdef));
                }
                dmg = dice(6, 4);
                if (half_spell_dam(mdef))
                        dmg = (dmg + 1) / 2;
                inc_timeout(mdef, STUNNED, dmg, TRUE);
                monflee(mdef, 0, FALSE, TRUE);
            } else if (wandlevel >= P_SKILLED)
                monkilled(magr, mdef, "", AD_RBRE);
        } else if (tseen)
            pline(msgc_yafm, "%s in dread.", M_verbs(mdef, "shudder"));

        break;
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case POT_POLYMORPH:
        if (unchanging(mdef) || (resists_magm(mdef) && !selfzap)) {
            /* magic resistance protects from polymorph traps, so make it guard
               against involuntary polymorph attacks too... */
            shieldeff(m_mx(mdef), m_my(mdef));
            if (tseen)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s %s momentarily different.",
                      hityou ? "You" : Monnam(mdef),
                      hityou ? "feel" : "looks");
            known = TRUE;
        } else if (hityou) {
            polyself(FALSE); /* FIXME: make skilled users able to affect the outcome */
            known = TRUE;
        } else if (!resist(magr, mdef, otmp->oclass, NOTELL, bcsign(otmp))) {
            /* natural shapechangers aren't affected by system shock (unless
               protection from shapechangers is interfering with their
               metabolism...) */
            if (mdef->cham == CHAM_ORDINARY && !rn2(25) &&
                (!tame || wandlevel < P_EXPERT)) {
                if (canseemon(mdef)) {
                    pline(combat_msgc(magr, mdef, cr_kill),
                          "%s shudders!", Monnam(mdef));
                }
                /* dropped inventory shouldn't be hit by this zap */
                for (obj = mdef->minvent; obj; obj = obj->nobj)
                    bypass_obj(obj);
                /* flags.bypasses = TRUE; ## for make_corpse() */
                /* no corpse after system shock */
                monkilled(magr, mdef, "", -AD_RBRE);
                break;
            }
            int tries = 1;
            boolean polymorphed = FALSE;
            if (wandlevel >= P_SKILLED)
                tries++;
            if (wandlevel == P_MASTER)
                tries++;
            while (tries-- > 0) {
                if (!newcham(mdef, NULL, (otyp != POT_POLYMORPH || selfzap), FALSE))
                    tries = 0;
                else {
                    polymorphed = TRUE;
                    if ((hostile && mlevel < mdef->m_lev) || (tame && mlevel > mdef->m_lev))
                        tries = 0;
                }
            }
            if (!Hallucination && polymorphed)
                known = TRUE;
        }
        break;
    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
        cancel_monst(mdef, otmp, magr, TRUE, FALSE);
        break;
    case WAN_TELEPORTATION:
    case SPE_TELEPORT_AWAY:
        known = TRUE;
        if ((wandlevel < P_EXPERT || selfzap) && tele_restrict(mdef))
            break; /* noteleport */
        if (level->flags.noteleport) {
            /* master proficiency can bypass noteleport */
            if (mdef == &youmonst)
                safe_teleds(FALSE);
            else
                rloc(mdef, TRUE);
            break;
        }
        reveal_invis = !mon_tele(mdef, (level->flags.noteleport ?
                                        FALSE : !!teleport_control(mdef)));
        break;
    case WAN_MAKE_INVISIBLE:
        if (wandlevel >= P_SKILLED && invisible(mdef))
            known = set_property(mdef, INVIS, -2, FALSE);
        else if (wandlevel == P_UNSKILLED)
            known = inc_timeout(mdef, INVIS, rnd(50), FALSE);
        else
            known = set_property(mdef, INVIS, 0, FALSE);
        break;
    case WAN_NOTHING:
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        wake = FALSE;
        break;
    case WAN_PROBING:
        wake = FALSE;
        reveal_invis = TRUE;
        probe_monster(mdef);
        if (wandlevel >= P_SKILLED)
            enlighten_mon(mdef, 0);
        known = TRUE;
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        wake = FALSE;   /* don't want immediate counterattack */
        if (Engulfed && mdef == u.ustuck) {
            if (is_animal(mdef->data)) {
                if (Blind)
                    pline(msgc_statusheal, "You feel a sudden rush of air!");
                else
                    pline(msgc_statusheal, "%s opens its mouth!", Monnam(mdef));
            }
            expels(mdef, mdef->data, TRUE);
            known = TRUE;
        } else if ((obj = which_armor(mdef, os_saddle))) {
            mdef->misc_worn_check &= ~obj->owornmask;
            obj->owornmask = 0L;
            if (mdef == u.usteed)
                dismount_steed(DISMOUNT_FELL);
            obj_extract_self(obj);
            place_object(obj, level, mdef->mx, mdef->my);
            /* call stackobj() if we ever drop anything that can merge */
            newsym(mdef->mx, mdef->my);
            known = TRUE;
        } else if (hityou && Punished) {
            pline(msgc_yafm, "Your chain quivers for a moment.");
            known = TRUE;
        }
        break;
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
        reveal_invis = TRUE;
        dmg = dice(6, otyp == SPE_EXTRA_HEALING ? 8 : 4);
        if (mdef->data != &mons[PM_PESTILENCE]) {
            wake = FALSE;       /* wakeup() makes the target angry */
            if (mdef == &youmonst)
                healup(dmg, 0, FALSE, FALSE);
            else {
                mdef->mhp += dmg;
                if (mdef->mhp > mdef->mhpmax)
                    mdef->mhp = mdef->mhpmax;
            }
            if (!selfzap) /* only cure blindness if zapped by someone else */
                set_property(mdef, BLINDED, -2, FALSE);
            if (mdef != &youmonst && canseemonoritem(mdef) &&
                disguised_mimic) {
                if (mdef->m_ap_type == M_AP_OBJECT &&
                    mdef->mappearance == STRANGE_OBJECT) {
                    /* it can do better now */
                    set_mimic_sym(mdef, level, rng_main);
                    newsym(mdef->mx, mdef->my);
                } else
                    mimic_hit_msg(mdef, otyp);
            } else
                pline(msgc_actionok, "%s %s%s better.",
                      mdef == &youmonst ? "You" : Monnam(mdef),
                      mdef == &youmonst ? "feel" : "looks",
                      otyp == SPE_EXTRA_HEALING ? " much" : "");
            if (yours && (mdef->mtame || mdef->mpeaceful)) {
                adjalign(Role_if(PM_HEALER) ? 1 : sgn(u.ualign.type));
            }
        } else {        /* Pestilence */
            /* Pestilence will always resist; damage is half of 3d{4,8} */
            if (resist(magr, mdef, otmp->oclass, TELL, bcsign(otmp)))
                dmg = dmg / 2 + 1;
            mdef->mhp -= dmg;
            if (mdef->mhp <= 0)
                monkilled(magr, mdef, "", AD_RBRE);
        }
        break;
    case WAN_LIGHT:    /* (broken wand) */
    case EXPENSIVE_CAMERA:
        if (mdef->data->mlet == S_LIGHT) {
            if (mdef == &youmonst)
                u.mh = u.mhmax;
            else
                mdef->mhp = mdef->mhpmax;
            if (tseen)
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s %s strengthened by the flash!",
                      hityou ? "You" : Monnam(mdef),
                      hityou ? "feel" : "looks");
            break;
        }

        if (mdef->data == &mons[PM_GREMLIN]) {
            /* Rule #1: Keep them out of the light. */
            int hp = m_mhp(mdef);
            if (hityou && Upolyd)
                hp = u.mh;

            dmg = otmp->otyp == WAN_LIGHT ?
                dice(1 + otmp->spe, 4) : rnd(min(m_mhp(mdef), 2 * range));
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s in %s!", M_verbs(mdef, dmg > hp / 2 ? "wail" :
                                      "cry"),
                  dmg > hp / 2 ? "agony" : "pain");
            if (hityou)
                losehp(dmg, killer_msg(DIED, "exposure to strong brightness"));
            else {
                mdef->mhp -= dmg;
                if (mdef->mhp <= 0)
                    monkilled(magr, mdef, NULL, AD_BLND);
            }
        }

        if (resists_blnd(mdef))
            break;

        dmg = rnd(10) * rnd(range);
        if (tseen)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "%s blinded by the flash!", M_verbs(mdef, "are"));

        if (dmg > 20 && !mx_eshk(mdef) && rn2(4)) {
            if (rn2(4))
                monflee(mdef, rnd(100), FALSE, TRUE);
            else
                monflee(mdef, 0, FALSE, TRUE);
        }
        set_property(mdef, BLINDED, dmg, FALSE);
        break;
    case WAN_SLEEP:    /* (broken wand) */
        /* [wakeup() doesn't rouse victims of temporary sleep, so it's okay to
           leave `wake' set to TRUE here] */
        reveal_invis = TRUE;
        if (sleep_monst(magr, mdef, dice(1 + otmp->spe, 12), WAND_CLASS) && !hityou)
            slept_monst(mdef);
        known = TRUE;
        break;
    case SPE_STONE_TO_FLESH:
        wake = FALSE;
        if (monsndx(mdef->data) == PM_STONE_GOLEM) {
            wake = TRUE;
            if (hityou)
                polymon(PM_FLESH_GOLEM, TRUE);
            else {
                const char *name = Monnam(mdef);
                if (newcham(mdef, &mons[PM_FLESH_GOLEM], FALSE, FALSE)) {
                    if (canseemon(mdef))
                        pline(msgc_actionok, "%s turns to flesh!", name);
                } else {
                    if (canseemon(mdef))
                        pline(msgc_substitute,
                              "%s looks rather fleshy for a moment.", name);
                }
            }
        }

        set_property(mdef, STONED, -2, FALSE); /* saved! */
        /* but at a cost.. */
        if (selfzap) {
            struct obj *otemp;
            struct obj *onext;
            boolean didmerge;
            for (otemp = m_minvent(mdef); otemp; otemp = onext) {
                onext = otemp->nobj;
                bhito(otemp, otmp);
            }
            /* It is possible that we can now merge some inventory.
               Do a higly paranoid merge.  Restart from the beginning
               until no merges. */
            do {
                didmerge = FALSE;
                for (otemp = invent; !didmerge && otemp; otemp = otemp->nobj)
                    for (onext = otemp->nobj; onext; onext = onext->nobj)
                        if (merged(&otemp, &onext)) {
                            didmerge = TRUE;
                            break;
                        }
            } while (didmerge);
        }
        break;
    case SPE_DRAIN_LIFE:
        dmg = rnd(8);
        if (dbldam)
            dmg *= 2;
        if (otyp == SPE_DRAIN_LIFE)
            dmg += spell_damage_bonus();
        if (half_spell_dam(mdef))
            dmg = dmg / 2 + 1;
        if (resists_drli(mdef)) {
            shieldeff(m_mx(mdef), m_my(mdef));
            if (hityou || canseemon(mdef))
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%sn't drained.", M_verbs(mdef, "are"));
        } else if (hityou)
            losexp(msgcat_many("drained by %s spell", s_suffix(k_monnam(magr)), NULL),
                   FALSE);
        else if (!resist(magr, mdef, otmp->oclass, TELL, bcsign(otmp))) {
            mdef->mhp -= dmg;
            mdef->mhpmax -= dmg;
            if (mdef->mhp <= 0 || mdef->mhpmax <= 0 || mdef->m_lev < 1)
                monkilled(magr, mdef, "", AD_DRLI);
            else {
                mdef->m_lev--;
                if (canseemon(mdef))
                    pline(combat_msgc(&youmonst, mdef, cr_hit),
                          "%s suddenly seems weaker!", Monnam(mdef));
            }
        }
        break;
    default:
        impossible("What an interesting effect (%d)", otyp);
        break;
    }
    if (wake) {
        if (!DEADMONSTER(mdef)) {
            if (yours) {
                wakeup(mdef, FALSE);
                m_respond(mdef);
                if (mx_eshk(mdef) && !*u.ushops)
                    hot_pursuit(mdef);
            } else
                mdef->msleeping = 0;
        } else if (mdef->m_ap_type)
            seemimic(mdef);     /* might unblock if mimicing a boulder/door */
    }
    /* note: bhitpos won't be set if swallowed, but that's okay since
       reveal_invis will be false.  We can't use mdef->mx, my since it might be
       an invisible worm hit on the tail. */
    if (reveal_invis) {
        if (!DEADMONSTER(mdef) && cansee(bhitpos.x, bhitpos.y) &&
            !canspotmon(mdef))
            map_invisible(bhitpos.x, bhitpos.y);
    }
    if (known && useen && tseen && (otmp->oclass == WAND_CLASS || otmp->oclass == POTION_CLASS))
        makeknown(otyp);
    return 0;
}


void
probe_monster(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    struct obj *otmp;

    mstatusline(mon);
    if (!you && notonhead)
        return; /* don't show minvent for long worm tail */

    if (m_minvent(mon)) {
        for (otmp = m_minvent(mon); otmp; otmp = otmp->nobj)
            otmp->dknown = 1;   /* treat as "seen" */
        if (you)
            display_inventory(NULL, FALSE);
        else
            display_minventory(mon, MINV_ALL, NULL);
    } else {
        pline(msgc_info, "%s not carrying anything.",
              M_verbs(mon, "are"));
    }
}


/* Return the object's physical location.  This only makes sense for objects
   that are currently on the level (i.e. migrating objects are nowhere).  By
   default, only things that can be seen (in hero's inventory, monster's
   inventory, or on the ground) are reported.  By adding BURIED_TOO and/or
   CONTAINED_TOO flags, you can also get the location of buried and contained
   objects.  Note that if an object is carried by a monster, its reported
   position may change from turn to turn.  This function returns FALSE if the
   position is not available or subject to the constraints above. */
boolean
get_obj_location(const struct obj *obj, xchar * xp, xchar * yp, int locflags)
{
    switch (obj->where) {
    case OBJ_INVENT:
        *xp = u.ux;
        *yp = u.uy;
        return TRUE;
    case OBJ_FLOOR:
        *xp = obj->ox;
        *yp = obj->oy;
        return TRUE;
    case OBJ_MINVENT:
        if (isok(obj->ocarry->mx, obj->ocarry->my)) {
            *xp = obj->ocarry->mx;
            *yp = obj->ocarry->my;
            return TRUE;
        }
        break;  /* !isok implies a migrating monster */
    case OBJ_BURIED:
        if (locflags & BURIED_TOO) {
            *xp = obj->ox;
            *yp = obj->oy;
            return TRUE;
        }
        break;
    case OBJ_CONTAINED:
        if (locflags & CONTAINED_TOO)
            return get_obj_location(obj->ocontainer, xp, yp, locflags);
        break;
    }
    *xp = *yp = 0;
    return FALSE;
}

/* locflags non-zero means get location even if monster is buried */
boolean
get_mon_location(struct monst * mon, xchar * xp, xchar * yp, int locflags)
{
    if (mon == &youmonst) {
        *xp = u.ux;
        *yp = u.uy;
        return TRUE;
    } else if (mon->mx != COLNO && (!mon->mburied || locflags)) {
        *xp = mon->mx;
        *yp = mon->my;
        return TRUE;
    } else {    /* migrating or buried */
        *xp = COLNO;
        *yp = ROWNO;
        return FALSE;
    }
}

static struct level *
object_dlevel(struct obj *obj)
{
    switch (obj->where) {
    case OBJ_FLOOR:
    case OBJ_BURIED:
        return obj->olev;
    case OBJ_CONTAINED:
        return object_dlevel(obj->ocontainer);
    case OBJ_INVENT:
        return level;
    case OBJ_MINVENT:
        return obj->ocarry->dlevel;
    case OBJ_ONBILL:
        panic("Object on bill in object_dlevel");
    case OBJ_MIGRATING:
        panic("Migrating object in object_dlevel");
    case OBJ_FREE:
    default:
        panic("Object is nowhere in object_dlevel");
    }
    /* This should not occur, but silence warnings */
    return 0;
}

/* used by revive() and animate_statue() */
struct monst *
montraits(struct obj *obj, coord * cc)
{
    struct monst *mtmp = NULL;
    struct monst *mtmp2 = NULL;

    if (!ox_monst(obj))
        panic("montraits() called without a monster to restore");
    mtmp2 = get_mtraits(obj, TRUE);

    if (mtmp2) {
        /* save_mtraits() validated mtmp2->mnum */
        mtmp2->data = &mons[mtmp2->orig_mnum];
        if (mtmp2->mhpmax <= 0)
            mtmp2->mhpmax = 1; /* Allow level drained monsters to revive */

        mtmp = makemon(mtmp2->data, level, cc->x, cc->y,
                       NO_MINVENT | MM_NOWAIT | MM_NOCOUNTBIRTH);
        if (!mtmp)
            return mtmp;

        /* heal the monster */
        if (mtmp->mhpmax > mtmp2->mhpmax && is_rider(mtmp2->data))
            mtmp2->mhpmax = mtmp->mhpmax;
        mtmp2->mhp = mtmp2->mhpmax;
        /* Get these ones from mtmp */
        mtmp2->minvent = mtmp->minvent; /* redundant */
        /* monster ID is available if the monster died in the current game, but
           should be zero if the corpse was in a bones level (we cleared it
           when loading bones) */
        if (!mtmp2->m_id)
            mtmp2->m_id = mtmp->m_id;
        mtmp2->dlevel = level;
        mtmp2->mx = mtmp->mx;
        mtmp2->my = mtmp->my;
        mtmp2->dx = mtmp->dx;
        mtmp2->dy = mtmp->dy;
        mtmp2->mux = COLNO;
        mtmp2->muy = ROWNO;
        mtmp2->mw = mtmp->mw;
        mtmp2->wormno = mtmp->wormno;
        mtmp2->misc_worn_check = mtmp->misc_worn_check;
        mtmp2->weapon_check = mtmp->weapon_check;
        mtmp2->mtrapseen = mtmp->mtrapseen;
        mtmp2->mflee = mtmp->mflee;
        mtmp2->mburied = mtmp->mburied;
        mtmp2->mundetected = mtmp->mundetected;
        mtmp2->mfleetim = mtmp->mfleetim;
        mtmp2->mlstmv = mtmp->mlstmv;
        mtmp2->m_ap_type = mtmp->m_ap_type;
        /* set these ones explicitly */
        mtmp2->mavenge = 0;
        mtmp2->meating = 0;
        mtmp2->mleashed = 0;
        mtmp2->mtrapped = 0;
        mtmp2->msleeping = 0;
        mtmp2->mfrozen = 0;
        mtmp2->mcanmove = 1;
        mtmp2->ustoned = 0;
        mtmp2->uslimed = 0;
        mtmp2->usicked = 0;
        /* the corpse may have been moved, set the monster's location from the
           corpse's location */
        mtmp2->dlevel = object_dlevel(obj);
        replmon(mtmp, mtmp2);
    }
    return mtmp2;
}

/*
 * get_container_location() returns the following information
 * about the outermost container:
 * loc argument gets set to:
 *   OBJ_INVENT   if in hero's inventory; return 0.
 *   OBJ_FLOOR    if on the floor; return 0.
 *   OBJ_BURIED   if buried; return 0.
 *   OBJ_MINVENT  if in monster's inventory; return monster.
 * container_nesting is updated with the nesting depth of the containers
 * if applicable.
 */
struct monst *
get_container_location(struct obj *obj, int *loc, int *container_nesting)
{
    if (!obj || !loc)
        return 0;

    if (container_nesting)
        *container_nesting = 0;
    while (obj && obj->where == OBJ_CONTAINED) {
        if (container_nesting)
            *container_nesting += 1;
        obj = obj->ocontainer;
    }
    if (obj) {
        *loc = obj->where;      /* outermost container's location */
        if (obj->where == OBJ_MINVENT)
            return obj->ocarry;
    }
    return NULL;
}

/*
 * Attempt to revive the given corpse, return the revived monster if
 * successful.  Note: this does NOT use up the corpse if it fails.
 */
struct monst *
revive(struct obj *obj)
{
    struct monst *mtmp = NULL;
    struct obj *container = NULL;
    int container_nesting = 0;
    schar savetame = 0;
    boolean recorporealization = FALSE;
    boolean in_container = FALSE;

    if (obj->otyp == CORPSE) {
        int montype = obj->corpsenm;
        xchar x, y;

        if (obj->where == OBJ_CONTAINED) {
            /* deal with corpses in [possibly nested] containers */
            struct monst *carrier;
            int holder = 0;

            container = obj->ocontainer;
            carrier =
                get_container_location(container, &holder, &container_nesting);
            switch (holder) {
            case OBJ_MINVENT:
                x = carrier->mx;
                y = carrier->my;
                in_container = TRUE;
                break;
            case OBJ_INVENT:
                x = u.ux;
                y = u.uy;
                in_container = TRUE;
                break;
            case OBJ_FLOOR:
                if (!get_obj_location(obj, &x, &y, CONTAINED_TOO))
                    return NULL;
                in_container = TRUE;
                break;
            default:
                return NULL;
            }
        } else {
            /* only for invent, minvent, or floor */
            if (!get_obj_location(obj, &x, &y, 0))
                return NULL;
        }
        if (in_container) {
            /* Rules for revival from containers: - the container cannot be
               locked - the container cannot be heavily nested (>2 is
               arbitrary) - the container cannot be a statue or bag of holding
               (except in very rare cases for the latter) */
            if (!x || !y || container->olocked || container_nesting > 2 ||
                container->otyp == STATUE || (container->otyp == BAG_OF_HOLDING
                                              && rn2(40)))
                return NULL;
        }

        if (MON_AT(level, x, y)) {
            coord new_xy;

            if (enexto(&new_xy, level, x, y, &mons[montype]))
                x = new_xy.x, y = new_xy.y;
        }

        if (cant_create(&montype, TRUE)) {
            /* make a zombie or worm instead */
            mtmp = makemon(&mons[montype], level, x, y, NO_MINVENT | MM_NOWAIT);
            if (mtmp) {
                mtmp->mhp = mtmp->mhpmax = 100;
                set_property(mtmp, FAST, 0, TRUE);
            }
        } else {
            if (ox_monst(obj)) {
                coord xy;

                xy.x = x;
                xy.y = y;
                mtmp = montraits(obj, &xy);
                if (mtmp && mtmp->mtame && !isminion(mtmp))
                    wary_dog(mtmp, TRUE);
            } else
                mtmp = makemon(&mons[montype], level, x, y,
                               NO_MINVENT | MM_NOWAIT | MM_NOCOUNTBIRTH);
            if (mtmp) {
                if (obj->m_id) {
                    struct monst *ghost;

                    ghost = find_mid(level, obj->m_id, FM_FMON);
                    if (ghost && ghost->data == &mons[PM_GHOST]) {
                        int x2, y2;

                        x2 = ghost->mx;
                        y2 = ghost->my;
                        if (ghost->mtame)
                            savetame = ghost->mtame;
                        if (canseemon(ghost))
                            pline(msgc_consequence,
                                  "%s is suddenly drawn into its former body!",
                                  Monnam(ghost));
                        mondead(ghost);
                        recorporealization = TRUE;
                        newsym(x2, y2);
                    }
                }
                /* Monster retains its name */
                christen_monst(mtmp, ox_name(obj));
                /* flag the quest leader as alive. */
                if (mtmp->data->msound == MS_LEADER ||
                    mtmp->m_id == u.quest_status.leader_m_id) {
                    u.quest_status.leader_m_id = mtmp->m_id;
                    u.quest_status.leader_is_dead = FALSE;
                }
            }
        }
        if (mtmp) {
            if (obj->oeaten)
                mtmp->mhp = eaten_stat(mtmp->mhp, obj);
            /* track that this monster was revived at least once */
            mtmp->mrevived = 1;

            if (recorporealization) {
                /* If mtmp is revivification of former tame ghost */
                if (savetame) {
                    struct monst *mtmp2 = tamedog(mtmp, NULL);

                    if (mtmp2) {
                        mtmp2->mtame = savetame;
                        mtmp = mtmp2;
                    }
                }
                /* was ghost, now alive, it's all very confusing */
                inc_timeout(mtmp, CONFUSION, dice(3, 8), FALSE);
            }

            switch (obj->where) {
            case OBJ_INVENT:
                useup(obj);
                break;
            case OBJ_FLOOR:
                /* in case MON_AT+enexto for invisible mon */
                x = obj->ox, y = obj->oy;
                /* not useupf(), which charges */
                if (obj->quan > 1L)
                    obj = splitobj(obj, 1L);
                delobj(obj);
                newsym(x, y);
                break;
            case OBJ_MINVENT:
                m_useup(obj->ocarry, obj);
                break;
            case OBJ_CONTAINED:
                obj_extract_self(obj);
                obfree(obj, NULL);
                break;
            default:
                panic("revive");
            }
        }
    }
    return mtmp;
}

static void
revive_egg(struct obj *obj)
{
    /* Generic eggs with corpsenm set to NON_PM will never hatch. */
    if (obj->otyp == EGG &&
        obj->corpsenm != NON_PM &&
        !dead_species(obj->corpsenm, TRUE))
        attach_egg_hatch_timeout(obj);
}

/* try to revive all corpses and eggs carried by `mon' */
int
unturn_dead(struct monst *mon)
{
    struct obj *otmp, *otmp2;
    struct monst *mtmp2;
    boolean youseeit;
    int once = 0, res = 0;
    const char *owner, *corpse = NULL;

    youseeit = (mon == &youmonst) ? TRUE : canseemon(mon);
    otmp2 = (mon == &youmonst) ? invent : mon->minvent;

    while ((otmp = otmp2) != 0) {
        otmp2 = otmp->nobj;
        if (otmp->otyp == EGG)
            revive_egg(otmp);
        if (otmp->otyp != CORPSE)
            continue;
        /* save the name; the object is liable to go away */
        if (youseeit)
            corpse = corpse_xname(otmp, TRUE);

        /* for a merged group, only one is revived; should this be fixed? */
        if ((mtmp2 = revive(otmp)) != 0) {
            ++res;
            if (youseeit) {
                if (!once++) {
                    owner = (mon == &youmonst) ? "Your" : s_suffix(Monnam(mon));
                    pline(msgc_consequence,
                          "%s %s suddenly comes alive!", owner, corpse);
                }
            } else if (canseemon(mtmp2))
                pline(msgc_levelwarning, "%s suddenly appears!",
                      Amonnam(mtmp2));
        }
    }
    return res;
}


static const char charged_objs[] = { WAND_CLASS, WEAPON_CLASS, ARMOR_CLASS, 0 };

static void
costly_cancel(struct obj *obj)
{
    char objroom;
    struct monst *shkp = NULL;

    if (obj->no_charge)
        return;

    switch (obj->where) {
    case OBJ_INVENT:
        if (obj->unpaid) {
            shkp = shop_keeper(level, *u.ushops);
            if (!shkp)
                return;
            pline_once(msgc_unpaid,
                       "You cancel an unpaid object, you pay for it!");
            bill_dummy_object(obj);
        }
        break;
    case OBJ_FLOOR:
        objroom = *in_rooms(level, obj->ox, obj->oy, SHOPBASE);
        shkp = shop_keeper(level, objroom);
        if (!costly_spot(obj->ox, obj->oy))
            return;
        /* "if costly_spot(u.ux, u.uy)" is correct. It checks whether shk can
           force the player to pay for the item by blocking the door. */
        if (costly_spot(u.ux, u.uy) && objroom == *u.ushops) {
            pline_once(msgc_unpaid, "You cancel it, you pay for it!");
            bill_dummy_object(obj);
        } else
            stolen_value(obj, obj->ox, obj->oy, FALSE, FALSE);
        break;
    }
}

/* cancel obj, possibly carried by you or a monster */
void
cancel_item(struct obj *obj)
{
    boolean holy = (obj->otyp == POT_WATER && (obj->blessed || obj->cursed));

    if (objects[obj->otyp].oc_magic ||
        (obj->spe &&
         (obj->oclass == ARMOR_CLASS || obj->oclass == WEAPON_CLASS ||
          is_weptool(obj)))
        || obj->otyp == POT_ACID || obj->otyp == POT_SICKNESS) {
        if (obj->spe != ((obj->oclass == WAND_CLASS) ? -1 : 0) &&
            obj->otyp != WAN_CANCELLATION &&
            /* can't cancel cancellation */
            obj->otyp != MAGIC_LAMP && obj->otyp != CANDELABRUM_OF_INVOCATION) {
            costly_cancel(obj);
            obj->spe = (obj->oclass == WAND_CLASS) ? -1 : 0;
        }
        switch (obj->oclass) {
        case SCROLL_CLASS:
            costly_cancel(obj);
            obj->otyp = SCR_BLANK_PAPER;
            obj->spe = 0;
            break;
        case SPBOOK_CLASS:
            if (obj->otyp != SPE_CANCELLATION &&
                obj->otyp != SPE_BOOK_OF_THE_DEAD) {
                costly_cancel(obj);
                obj->otyp = SPE_BLANK_PAPER;
            }
            break;
        case POTION_CLASS:
            costly_cancel(obj);
            if (obj->otyp == POT_SICKNESS || obj->otyp == POT_SEE_INVISIBLE) {
                /* sickness is "biologically contaminated" fruit juice; cancel
                   it and it just becomes fruit juice... whereas see invisible
                   tastes like "enchanted" fruit juice, it similarly cancels. */
                obj->otyp = POT_FRUIT_JUICE;
            } else {
                obj->otyp = POT_WATER;
                obj->odiluted = 0;      /* same as any other water */
            }
            break;
        }
    }
    if (holy)
        costly_cancel(obj);
    unbless(obj);
    uncurse(obj);
#ifdef INVISIBLE_OBJECTS
    if (obj->oinvis)
        obj->oinvis = 0;
#endif
    return;
}

/* Remove a positive enchantment or charge from obj,
 * possibly carried by you or a monster
 */
boolean
drain_item(struct obj * obj)
{
    /* Is this a charged/enchanted object? */
    if (!obj ||
        (!objects[obj->otyp].oc_charged && obj->oclass != WEAPON_CLASS &&
         obj->oclass != ARMOR_CLASS && !is_weptool(obj)) || obj->spe <= 0)
        return FALSE;
    if (obj->oartifact && defends(AD_DRLI, obj))
        return FALSE;
    if (obj_resists(obj, 10, 90))
        return FALSE;

    /* Charge for the cost of the object */
    costly_cancel(obj); /* The term "cancel" is okay for now */

    /* Drain the object and any implied effects */
    obj->spe--;

    if (carried(obj))
        update_inventory();
    return TRUE;
}

/* ochance, achance:  percent chance for ordinary objects, artifacts */
boolean
obj_resists(struct obj * obj, int ochance, int achance)
{       /* percent chance for ordinary objects, artifacts */
    if (obj->otyp == AMULET_OF_YENDOR || obj->otyp == SPE_BOOK_OF_THE_DEAD ||
        obj->otyp == CANDELABRUM_OF_INVOCATION || obj->otyp == BELL_OF_OPENING
        || (obj->otyp == CORPSE && is_rider(&mons[obj->corpsenm]))) {
        return TRUE;
    } else {
        int chance = rn2(100);

        return (boolean) (chance < (obj->oartifact ? achance : ochance));
    }
}

boolean
obj_shudders(struct obj * obj)
{
    int zap_odds;

    if (obj->oclass == WAND_CLASS)
        zap_odds = 3;   /* half-life = 2 zaps */
    else if (obj->cursed)
        zap_odds = 3;   /* half-life = 2 zaps */
    else if (obj->blessed)
        zap_odds = 12;  /* half-life = 8 zaps */
    else
        zap_odds = 8;   /* half-life = 6 zaps */

    /* adjust for "large" quantities of identical things */
    if (obj->quan > 4L)
        zap_odds /= 2;

    return (boolean) (!rn2(zap_odds));
}


/* Use up at least minwt number of things made of material mat.
 * There's also a chance that other stuff will be used up.  Finally,
 * there's a random factor here to keep from always using the stuff
 * at the top of the pile.
 */
static void
polyuse(struct obj *objhdr, int mat, int minwt)
{
    struct obj *otmp, *otmp2;

    for (otmp = objhdr; minwt > 0 && otmp; otmp = otmp2) {
        otmp2 = otmp->nexthere;
        if (otmp == uball || otmp == uchain)
            continue;
        if (obj_resists(otmp, 0, 0))
            continue;   /* preserve unique objects */

        if (((int)objects[otmp->otyp].oc_material == mat) ==
            (rn2(minwt + 1) != 0)) {
            /* appropriately add damage to bill */
            if (costly_spot(otmp->ox, otmp->oy)) {
                if (*u.ushops)
                    addtobill(otmp, FALSE, FALSE, FALSE);
                else
                    stolen_value(otmp, otmp->ox, otmp->oy, FALSE, FALSE);
            }
            if (otmp->quan < LARGEST_INT)
                minwt -= (int)otmp->quan;
            else
                minwt = 0;
            delobj(otmp);
        }
    }
}

/*
 * Polymorph some of the stuff in this pile into a monster, preferably
 * a golem of the kind okind.
 */
static void
create_polymon(struct obj *obj, int okind)
{
    const struct permonst *mdat = NULL;
    struct monst *mtmp;
    const char *material;
    int pm_index;

    /* no golems if you zap only one object -- not enough stuff */
    if (!obj || (!obj->nexthere && obj->quan == 1L))
        return;

    /* some of these choices are arbitrary */
    switch (okind) {
    case IRON:
    case METAL:
    case MITHRIL:
        pm_index = PM_IRON_GOLEM;
        material = "metal ";
        break;
    case COPPER:
    case SILVER:
    case PLATINUM:
    case GEMSTONE:
    case MINERAL:
        pm_index = rn2(2) ? PM_STONE_GOLEM : PM_CLAY_GOLEM;
        material = "lithic ";
        break;
    case 0:
    case FLESH:
        /* there is no flesh type, but all food is type 0, so we use it */
        pm_index = PM_FLESH_GOLEM;
        material = "organic ";
        break;
    case WOOD:
        pm_index = PM_WOOD_GOLEM;
        material = "wood ";
        break;
    case LEATHER:
        pm_index = PM_LEATHER_GOLEM;
        material = "leather ";
        break;
    case CLOTH:
        pm_index = PM_ROPE_GOLEM;
        material = "cloth ";
        break;
    case BONE:
        pm_index = PM_SKELETON; /* nearest thing to "bone golem" */
        material = "bony ";
        break;
    case GOLD:
        pm_index = PM_GOLD_GOLEM;
        material = "gold ";
        break;
    case GLASS:
        pm_index = PM_GLASS_GOLEM;
        material = "glassy ";
        break;
    case PAPER:
        pm_index = PM_PAPER_GOLEM;
        material = "paper ";
        break;
    default:
        /* if all else fails... */
        pm_index = PM_STRAW_GOLEM;
        material = "";
        break;
    }

    if (!(mvitals[pm_index].mvflags & G_GENOD))
        mdat = &mons[pm_index];

    mtmp = makemon(mdat, level, obj->ox, obj->oy, NO_MM_FLAGS);
    polyuse(obj, okind, (int)mons[pm_index].cwt);

    if (mtmp && cansee(mtmp->mx, mtmp->my)) {
        pline(msgc_substitute,
              "Some %sobjects meld, and %s arises from the pile!",
              material, a_monnam(mtmp));
    }
}

/* Assumes obj is on the floor. */
void
do_osshock(struct obj *obj)
{
    long i;

    pline(msgc_itemloss, "You feel shuddering vibrations.");

    if (poly_zapped < 0) {
        /* some may metamorphosize */
        for (i = obj->quan; i; i--)
            if (!rn2(Luck + 45)) {
                poly_zapped = objects[obj->otyp].oc_material;
                break;
            }
    }

    /* if quan > 1 then some will survive intact */
    long broken_quan = 1;
    if (obj->quan > 1L) {
        if (obj->quan > LARGEST_INT)
            broken_quan = rnd(30000);
        else
            broken_quan = rnd((int)obj->quan - 1);
    }

    useupf(obj, broken_quan);
}

/* Is the object immune to polymorphing?

   Currently true for items of polymorph and amulets of unchanging. */
boolean
poly_proof(struct obj *obj)
{
    if (obj == uskin() || obj == uball)
        return TRUE;

    switch (obj->otyp) {
    case POT_POLYMORPH:
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case RIN_POLYMORPH:
    case RIN_POLYMORPH_CONTROL:
    case AMULET_OF_UNCHANGING:
        return TRUE;
    default:
        return FALSE;
    }
}

/*
 * Polymorph the object to the given object ID.  If the ID is STRANGE_OBJECT
 * then pick random object from the source's class (this is the standard
 * "polymorph" case).  If ID is set to a specific object, inhibit fusing
 * n objects into 1.  This could have been added as a flag, but currently
 * it is tied to not being the standard polymorph case. The new polymorphed
 * object replaces obj in its link chains.  Return value is a pointer to
 * the new object.
 *
 * A worn object remains worn if possible, with any attendant effects. This may
 * cause the object to be destroyed (e.g. an amulet of change), in which case
 * the return value will be 0.
 *
 * This should be safe to call for an object anywhere.
 */
struct obj *
poly_obj(struct obj *obj, int id)
{
    struct obj *otmp;
    xchar ox, oy;
    boolean can_merge = (id == STRANGE_OBJECT);
    int obj_location = obj->where;
    enum rng rng = (flags.mon_moving ? rng_main : rng_poly_obj);

    if (obj->owornmask & W_MASK(os_saddle) ||
        obj == uball || obj == uchain) {
        impossible("Polymorphing a chain, ball, or saddle!");
        return obj;
    }

    if (obj->otyp == BOULDER && In_sokoban(&u.uz))
        change_luck(-1);        /* Sokoban guilt */
    if (id == STRANGE_OBJECT) { /* preserve symbol */
        int try_limit = 3;

        /* Try up to 3 times to make the magic-or-not status of the new item be
           the same as it was for the old one. */
        otmp = NULL;
        do {
            if (otmp)
                delobj(otmp);
            /* Ideally, we'd use a different RNG for each object class, but
               for now, we just use one common RNG. */
            otmp = mkobj(level, obj->oclass, FALSE, rng);
        } while (--try_limit > 0 &&
                 objects[obj->otyp].oc_magic != objects[otmp->otyp].oc_magic);
    } else {
        /* literally replace obj with this new thing; don't use rng_poly_obj
           because we already know what we're replacing with */
        rng = rng_main;
        otmp = mksobj(level, id, FALSE, FALSE, rng_main);
        /* Actually more things use corpsenm but they polymorph differently */
#define USES_CORPSENM(typ) ((typ)==CORPSE || (typ)==STATUE || (typ)==FIGURINE)
        if (USES_CORPSENM(obj->otyp) && USES_CORPSENM(id))
            otmp->corpsenm = obj->corpsenm;
#undef USES_CORPSENM
    }

    /* preserve quantity */
    otmp->quan = obj->quan;
    /* preserve the shopkeepers (lack of) interest */
    otmp->no_charge = obj->no_charge;
    /* preserve inventory letter if in inventory */
    if (obj_location == OBJ_INVENT)
        otmp->invlet = obj->invlet;

    /* avoid abusing eggs laid by you */
    if (obj->otyp == EGG && obj->spe) {
        int mnum, tryct = 100;

        /* first, turn into a generic egg */
        if (otmp->otyp == EGG)
            kill_egg(otmp);
        else {
            otmp->otyp = EGG;
            otmp->owt = weight(otmp);
        }
        otmp->corpsenm = NON_PM;
        otmp->spe = 0;

        /* now change it into something laid by the hero */
        while (tryct--) {
            mnum = can_be_hatched(rn2(NUMMONS));
            if (mnum != NON_PM && !dead_species(mnum, TRUE)) {
                otmp->spe = 1;  /* layed by hero */
                otmp->corpsenm = mnum;
                attach_egg_hatch_timeout(otmp);
                break;
            }
        }
    }

    /* keep special fields (including charges on wands) */
    if (strchr(charged_objs, otmp->oclass))
        otmp->spe = obj->spe;
    otmp->recharged = obj->recharged;

    otmp->cursed = obj->cursed;
    otmp->blessed = obj->blessed;
    otmp->oeroded = obj->oeroded;
    otmp->oeroded2 = obj->oeroded2;
    if (!is_flammable(otmp) && !is_rustprone(otmp))
        otmp->oeroded = 0;
    if (!is_corrodeable(otmp) && !is_rottable(otmp))
        otmp->oeroded2 = 0;
    if (is_damageable(otmp))
        otmp->oerodeproof = obj->oerodeproof;

    /* Keep chest/box traps and poisoned ammo if we may */
    if (obj->otrapped && Is_box(otmp))
        otmp->otrapped = TRUE;

    if (obj->opoisoned && is_poisonable(otmp))
        otmp->opoisoned = TRUE;

    if (id == STRANGE_OBJECT && obj->otyp == CORPSE) {
        /* turn crocodile corpses into shoes */
        if (obj->corpsenm == PM_CROCODILE) {
            otmp->otyp = LOW_BOOTS;
            otmp->oclass = ARMOR_CLASS;
            otmp->spe = 0;
            otmp->oeroded = 0;
            otmp->oerodeproof = TRUE;
            otmp->quan = 1L;
            otmp->cursed = FALSE;
        }
    }

    /* no box contents --KAA */
    if (Has_contents(otmp))
        delete_contents(otmp);

    /* 'n' merged objects may be fused into 1 object */
    if (otmp->quan > 1L &&
        (!objects[otmp->otyp].oc_merge ||
         (can_merge && otmp->quan > (long)rn2(1000))))
        otmp->quan = 1L;

    switch (otmp->oclass) {
    case AMULET_CLASS:
        while (otmp->otyp == AMULET_OF_UNCHANGING)
            otmp->otyp = rnd_class(AMULET_OF_ESP, AMULET_OF_MAGICAL_BREATHING,
                                   rng);
        break;

    case TOOL_CLASS:
        if (otmp->otyp == MAGIC_LAMP) {
            otmp->otyp = OIL_LAMP;
            otmp->age = 1500L;  /* "best" oil lamp possible */
        } else if (otmp->otyp == MAGIC_MARKER) {
            otmp->recharged = 1;        /* degraded quality */
        }
        /* don't care about the recharge count of other tools */
        break;

    case WAND_CLASS:
        while (otmp->otyp == WAN_WISHING || otmp->otyp == WAN_POLYMORPH)
            otmp->otyp = rnd_class(WAN_LIGHT, WAN_LIGHTNING, rng);
        /* altering the object tends to degrade its quality (analogous to
           spellbook `read count' handling) */
        if ((int)otmp->recharged < rn2(7))      /* recharge_limit */
            otmp->recharged++;
        break;

    case POTION_CLASS:
        while (otmp->otyp == POT_POLYMORPH)
            otmp->otyp = rnd_class(POT_GAIN_ABILITY, POT_WATER, rng);
        break;

    case SPBOOK_CLASS:
        while (otmp->otyp == SPE_POLYMORPH)
            otmp->otyp = rnd_class(SPE_DIG, SPE_BLANK_PAPER, rng);
        /* reduce spellbook abuse */
        otmp->spestudied = obj->spestudied + 1;
        break;

    case RING_CLASS:
        while (otmp->otyp == RIN_POLYMORPH ||
               otmp->otyp == RIN_POLYMORPH_CONTROL)
            otmp->otyp = rnd_class(RIN_ADORNMENT,
                                   RIN_PROTECTION_FROM_SHAPE_CHANGERS,
                                   rng);
        break;
    case GEM_CLASS:
        if (otmp->quan > (long)rnd(4) &&
            objects[obj->otyp].oc_material == MINERAL &&
            objects[otmp->otyp].oc_material != MINERAL) {
            otmp->otyp = ROCK;  /* transmutation backfired */
            otmp->quan /= 2L;   /* some material has been lost */
        }
        break;
    }

    /* update the weight */
    otmp->owt = weight(otmp);

    if (obj_location == OBJ_FLOOR && obj->otyp == BOULDER &&
        otmp->otyp != BOULDER)
        unblock_point(obj->ox, obj->oy);

    /* ** we are now done adjusting the object ** */


    /* swap otmp for obj */
    long old_mask = obj->owornmask;
    if (obj_location == OBJ_INVENT)
        remove_worn_item(obj, TRUE);
    replace_object(obj, otmp);
    if (obj_location == OBJ_INVENT) {
        /* We may need to do extra adjustments for the hero if we're messing
           with the hero's inventory.  The following calls are equivalent to
           calling freeinv on obj and addinv on otmp, while doing an in-place
           swap of the actual objects. */
        long new_mask;
        /* boolean save_twoweap = u.twoweap; */
        swapinv(obj, otmp);

        /* This code counts the number of bits set in the mask.  We want to be
           sure that only one bit is set, because otherwise we're very
           confused.  v&(v-1) is nonzero whenever multiple bits are set.

           TODO: The way canwearobj currently works, this code will cause
           polymorphed items to fall off if they're worn beneath cursed
           items. This is probably not what we want. We do at least set cblock
           to TRUE, so that they won't fall off beneath uncursed items. */
        long v = old_mask & W_EQUIP;
        if (v & (v-1)) {
            impossible("More than one worn mask set in poly_obj?!?");
        } else if (canwearobj(otmp, &new_mask, FALSE, TRUE, TRUE)) {
            /* canwearobj only checks for wearable armor */
            const int body_slots =
                (W_MASK(os_arm) | W_MASK(os_armc) | W_MASK(os_armu));
            if (old_mask == new_mask ||
                (old_mask & body_slots && new_mask & body_slots) ||
                (old_mask & W_RING && new_mask & W_RING)) {
                if (new_mask == W_RING) new_mask = old_mask;
                if (setequip(objslot_from_mask(new_mask), otmp, em_silent)) {
                    /* otmp was destroyed */
                    if (obj->unpaid)
                        costly_damage_obj(obj);
                    delobj(obj);
                    return NULL;
                }
            }
        } else if (old_mask & W_MASK(os_wep)) {
            if (!ready_weapon(otmp))
                impossible("Failed to ready polymorphed weapon?");
            /* FIXME: two-weaponing */
        } else if (old_mask & W_MASK(os_swapwep)) {
            setuswapwep(otmp);
            /* FIXME: two-weaponing */
        }
    } else if (old_mask) {
        impossible("owornmask not in inventory in poly_obj?");
        otmp->owornmask = old_mask;
    }

    if ((!carried(otmp) || obj->unpaid) &&
        get_obj_location(otmp, &ox, &oy, BURIED_TOO | CONTAINED_TOO) &&
        costly_spot(ox, oy)) {
        struct monst *shkp =
            shop_keeper(level, *in_rooms(level, ox, oy, SHOPBASE));

        if ((!obj->no_charge ||
             (Has_contents(obj) &&
              (contained_cost(obj, shkp, 0L, FALSE, FALSE) != 0L)))
            && inhishop(shkp)) {
            if (shkp->mpeaceful) {
                if (*u.ushops &&
                    *in_rooms(level, u.ux, u.uy, 0) ==
                    *in_rooms(level, shkp->mx, shkp->my, 0) &&
                    !costly_spot(u.ux, u.uy))
                    make_angry_shk(shkp, ox, oy);
                else {
                    pline(msgc_npcanger, "%s gets angry!", Monnam(shkp));
                    hot_pursuit(shkp);
                }
            } else
                pline_once(msgc_npcvoice, "%s is furious!", Monnam(shkp));
        }
    }
    delobj(obj);
    return otmp;
}


static int
hito_stone_to_flesh(struct obj *obj)
{
    int res = 1;
    boolean smell = FALSE;
    xchar refresh_x = obj->ox;
    xchar refresh_y = obj->oy;
    xchar oox = -1, ooy = -1;

    if (objects[obj->otyp].oc_material != MINERAL &&
        objects[obj->otyp].oc_material != GEMSTONE)
        return 0;

    /* add more if stone objects are added.. */
    switch (objects[obj->otyp].oc_class) {
    case ROCK_CLASS:   /* boulders and statues */
        if (obj->otyp == BOULDER) {
            poly_obj(obj, HUGE_CHUNK_OF_MEAT);
            smell = TRUE;
            break;
        } else if (obj->otyp == STATUE) {

            get_obj_location(obj, &oox, &ooy, 0);
            refresh_x = oox;
            refresh_y = ooy;
            if (vegetarian(&mons[obj->corpsenm])) {
                /* Don't animate monsters that aren't flesh */
                poly_obj(obj, MEATBALL);
                smell = TRUE;
                break;
            }
            if (!animate_statue(obj, oox, ooy, ANIMATE_SPELL, NULL))
                goto makecorpse;
        } else {        /* new rock class object... */
            /* impossible? */
            res = 0;
        }
        break;
    case TOOL_CLASS:   /* figurine */
        {
            struct monst *mon;

            if (obj->otyp != FIGURINE) {
                res = 0;
                break;
            }
            if (vegetarian(&mons[obj->corpsenm])) {
                /* Don't animate monsters that aren't flesh */
                poly_obj(obj, MEATBALL);
                smell = TRUE;
                break;
            }
            get_obj_location(obj, &oox, &ooy, 0);
            refresh_x = oox;
            refresh_y = ooy;
            mon = makemon(&mons[obj->corpsenm], level, oox, ooy, NO_MM_FLAGS);
            if (mon) {
                delobj(obj);
                if (cansee(mon->mx, mon->my))
                    pline(msgc_consequence, "The figurine animates!");
                break;
            }
            goto makecorpse;
        }
        /* maybe add weird things to become? */
    case RING_CLASS:   /* some of the rings are stone */
        poly_obj(obj, MEAT_RING);
        smell = TRUE;
        break;
    case WAND_CLASS:   /* marble wand */
        poly_obj(obj, MEAT_STICK);
        smell = TRUE;
        break;
    case GEM_CLASS:    /* rocks & gems */
        poly_obj(obj, MEATBALL);
        smell = TRUE;
        break;
    case WEAPON_CLASS: /* crysknife */
        /* fall through */
    default:
        res = 0;
        break;
    }

end:
    if (smell) {
        if (herbivorous(youmonst.data) &&
            (!carnivorous(youmonst.data) || Role_if(PM_MONK) ||
             !u.uconduct[conduct_vegetarian]))
            pline_once(msgc_levelsound, "You smell the odor of meat.");
        else
            pline_once(msgc_levelsound, "You smell a delicious smell.");
    }

    newsym(refresh_x, refresh_y);
    return res;

makecorpse:;
    struct obj *item;

    if (mons[obj->corpsenm].geno & (G_NOCORPSE | G_UNIQ)) {
        res = 0;
        goto end;
    }
    /* Unlikely to get here since genociding
       monsters also sets the G_NOCORPSE flag.
       Drop the contents, poly_obj loses them. */
    while ((item = obj->cobj) != 0) {
        obj_extract_self(item);
        place_object(item, level, oox, ooy);
    }
    poly_obj(obj, CORPSE);
    goto end;
}

/*
 * Object obj was hit by the effect of the wand/spell otmp.  Return
 * non-zero if the wand/spell had any effect.
 */
int
bhito(struct obj *obj, struct obj *otmp)
{
    int res = 1;        /* affected object by default */

    if (obj->bypass) {
        /* The bypass bit is currently only used as follows: POLYMORPH - When a
           monster being polymorphed drops something from its inventory as a
           result of the change.  If the items fall to the floor, they are not
           subject to direct subsequent polymorphing themselves on that same
           zap. This makes it consistent with items that remain in the
           monster's inventory. They are not polymorphed either. UNDEAD_TURNING
           - When an undead creature gets killed via undead turning, prevent
           its corpse from being immediately revived by the same effect. The
           bypass bit on all objects is reset each turn, whenever
           flags.bypasses is set. We check the obj->bypass bit above AND
           flags.bypasses as a safeguard against any stray occurrence left in
           an obj struct someplace, although that should never happen. */
        if (flags.bypasses)
            return 0;
        else
            obj->bypass = 0;
    }

    /*
     * Some parts of this function expect the object to be on the floor
     * obj->{ox,oy} to be valid.  The exception to this (so far) is
     * for the STONE_TO_FLESH spell.
     */
    if (!(obj->where == OBJ_FLOOR || otmp->otyp == SPE_STONE_TO_FLESH))
        impossible("bhito: obj is not floor or Stone To Flesh spell");

    if (obj == uball) {
        res = 0;
    } else if (obj == uchain) {
        if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK) {
            unpunish();
            makeknown(otmp->otyp);
        } else
            res = 0;
    } else
        switch (otmp->otyp) {
        case WAN_POLYMORPH:
        case SPE_POLYMORPH: {
            int ox, oy;
            if (poly_proof(obj) || obj_resists(obj, 5, 95)) {
                res = 0;
                break;
            }
            /* With the new wand balance, monster wand accidents might
               cause object polymorph. Only break conduct if the hero
               did it. This can -- and will -- cause abuse potential,
               but it is better than breaking conducts players have no
               control over */
            if (!flags.mon_moving)
                break_conduct(conduct_polypile);
            /* any saved lock context will be dangerously obsolete */
            if (Is_box(obj))
                boxlock(obj, otmp);

            if (obj_shudders(obj)) {
                if (cansee(obj->ox, obj->oy))
                    makeknown(otmp->otyp);
                do_osshock(obj);
                break;
            }

            ox = obj->ox, oy = obj->oy;
            obj = poly_obj(obj, STRANGE_OBJECT);
            /* poly_obj doesn't block vision, do that ourselves now. */
            if (obj && obj->where == OBJ_FLOOR && obj->otyp == BOULDER)
                block_point(obj->ox, obj->oy);
            if (obj && (obj->ox != ox || obj->oy != oy))
                impossible("Polymorphed object moved?!?");
            newsym(ox, oy);
            break;
        }
        case WAN_PROBING:
            res = !obj->dknown;
            /* target object has now been "seen (up close)" */
            obj->dknown = 1;
            if (Is_container(obj) || obj->otyp == STATUE) {
                boolean quantum_cat = FALSE;

                if ((obj->spe == 1) && (obj->otyp != STATUE)) {
                    observe_quantum_cat(obj);
                    quantum_cat = TRUE;
                }
                if (!obj->cobj)
                    pline(msgc_info, "%s %sempty.", Tobjnam(obj, "are"),
                          quantum_cat ? "now " : "");
                else {
                    struct obj *o;

                    /* view contents (not recursively) */
                    for (o = obj->cobj; o; o = o->nobj)
                        o->dknown = 1;  /* "seen", even if blind */
                    display_cinventory(obj);
                }
                res = 1;
            }
            if (res)
                makeknown(WAN_PROBING);
            break;
        case WAN_STRIKING:
        case SPE_FORCE_BOLT:
            if (obj->otyp == BOULDER)
                fracture_rock(obj);
            else if (obj->otyp == STATUE)
                break_statue(obj);
            else {
                if (!flags.mon_moving)
                    hero_breaks(obj, obj->ox, obj->oy, FALSE);
                else
                    breaks(obj, obj->ox, obj->oy);
                res = 0;
            }
            /* BUG[?]: shouldn't this depend upon you seeing it happen? */
            makeknown(otmp->otyp);
            break;
        case WAN_CANCELLATION:
        case SPE_CANCELLATION:
            cancel_item(obj);
            newsym(obj->ox, obj->oy);   /* might change color */
            break;
        case SPE_DRAIN_LIFE:
            drain_item(obj);
            break;
        case WAN_TELEPORTATION:
        case SPE_TELEPORT_AWAY:
            rloco(obj);
            break;
        case WAN_MAKE_INVISIBLE:
#ifdef INVISIBLE_OBJECTS
            obj->oinvis = TRUE;
            newsym(obj->ox, obj->oy);   /* make object disappear */
#endif
            break;
        case WAN_UNDEAD_TURNING:
        case SPE_TURN_UNDEAD:
            if (obj->otyp == EGG)
                revive_egg(obj);
            else
                res = ! !revive(obj);
            break;
        case WAN_OPENING:
        case SPE_KNOCK:
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
            if (Is_box(obj))
                res = boxlock(obj, otmp);
            else
                res = 0;
            if (res /* && otmp->oclass == WAND_CLASS */ )
                makeknown(otmp->otyp);
            break;
        case WAN_SLOW_MONSTER: /* no effect on objects */
        case SPE_SLOW_MONSTER:
        case WAN_SPEED_MONSTER:
        case WAN_NOTHING:
        case SPE_HEALING:
        case SPE_EXTRA_HEALING:
        case EXPENSIVE_CAMERA:
            res = 0;
            break;
        case SPE_STONE_TO_FLESH:
            res = hito_stone_to_flesh(obj);
            break;
        default:
            impossible("What an interesting effect (%d)", otmp->otyp);
            break;
        }
    return res;
}

/* returns nonzero if something was hit */
int
bhitpile(struct obj *obj, int (*fhito) (struct obj *, struct obj *), int tx,
         int ty)
{
    int hitanything = 0;
    struct obj *otmp, *next_obj;

    if (obj->otyp == SPE_FORCE_BOLT || obj->otyp == WAN_STRIKING) {
        struct trap *t = t_at(level, tx, ty);

        /* We can't settle for the default calling sequence of bhito(otmp) ->
           break_statue(otmp) -> activate_statue_trap(ox,oy) because that last
           call might end up operating on our `next_obj' (below), rather than
           on the current object, if it happens to encounter a statue which
           mustn't become animated. */
        if (t && t->ttyp == STATUE_TRAP && activate_statue_trap(t, tx, ty, TRUE)
            && obj->otyp == WAN_STRIKING)
            makeknown(obj->otyp);
    }

    poly_zapped = -1;
    for (otmp = level->objects[tx][ty]; otmp; otmp = next_obj) {
        /* Fix for polymorph bug, Tim Wright */
        next_obj = otmp->nexthere;
        hitanything += (*fhito) (otmp, obj);
    }
    if (poly_zapped >= 0)
        create_polymon(level->objects[tx][ty], poly_zapped);

    return hitanything;
}


/*
 * wrestable - returns 1 if a wand can only be zapped
 *             by wresting it.
 * added by bcd@pvv.org 16/9/08
 */
int
wrestable(struct obj *wand)
{
    return (wand->spe == 0);
}

/*
 * zappable - returns 1 if zap is available, 0 otherwise.
 *            it removes a charge from the wand if zappable.
 * added by GAN 11/03/86
 */
int
zappable(struct monst *mon, struct obj *wand)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    if (wand->spe < 0 || (wand->spe == 0 && rn2(121))) {
        if (you || vis) {
            pline(you ? msgc_failrandom : msgc_monneutral,
                  "You feel an absence of magical power.");
            wand->known = 1; /* we know the :0 */
        }
        if (!you)
            wand->mknown = 1; /* monster learns charge count */
        return 0;
    }

    if (wand->spe == 0)
        if (you || vis)
            pline(msgc_actionok,
                  "%s wrest%s one last charge from the worn-out wand.",
                  you ? "You" : Monnam(mon), you ? "" : "s");
    wand->spe--;
    return 1;
}

/*
 * zapnodir - zaps a NODIR wand/spell.
 * added by GAN 11/03/86
 */
void
zapnodir(struct monst *mon, struct obj *obj)
{
    boolean known = FALSE;
    int wandlevel = 0;
    int howmany;
    int detectradius;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    if (obj && obj->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mon, obj);

    switch (obj->otyp) {
    case WAN_LIGHT:
    case SPE_LIGHT:
        litroom(mon, TRUE, obj, TRUE);
        if (!blind(&youmonst) && (you || vis))
            known = TRUE;
        break;
    case WAN_SECRET_DOOR_DETECTION:
    case SPE_DETECT_UNSEEN:
        if (you) {
            detectradius = (wandlevel == P_UNSKILLED ? 1  :
                            wandlevel == P_BASIC     ? 5  :
                            wandlevel == P_SKILLED   ? 9  :
                            wandlevel == P_EXPERT    ? 15 :
                            wandlevel == P_MASTER    ? -1 :
                            BOLT_LIM);
            if (!findit(detectradius))
                return;
            if (!blind(&youmonst))
                known = TRUE;
        }
        break;
    case WAN_CREATE_MONSTER:
        howmany = rn2(wandlevel == P_UNSKILLED ? 1  :
                      wandlevel == P_BASIC     ? 2  :
                      wandlevel == P_SKILLED   ? 4  :
                      wandlevel == P_EXPERT    ? 8  :
                      wandlevel == P_MASTER    ? 16 :
                      1);
        howmany++;

        known = create_critters(howmany, NULL, m_mx(mon), m_my(mon));
        break;
    case WAN_WISHING:
        if (you) {
            known = TRUE;
            if (Luck + rn2(5) < 0) {
                pline(msgc_itemloss,
                      "Unfortunately, nothing happens.");
                break;
            }
            makewish();
        } else {
            if (vis)
                pline(msgc_monneutral, "%s may wish for an object.",
                      Monnam(mon));
            mon_makewish(mon);
        }
        break;
    case WAN_ENLIGHTENMENT:
        if (you) {
            known = TRUE;
            pline(msgc_info, "You feel self-knowledgeable...");
            win_pause_output(P_MESSAGE);
            enlightenment(FALSE);
            pline_implied(msgc_info, "The feeling subsides.");
            exercise(A_WIS, TRUE);
        } else if (vis) {
            known = TRUE;
            pline(msgc_info, "%s looks self-knowledgeable...",
                  Monnam(mon));
            /* TODO: id unknown magical objects */
        }
        break;
    }
    if (known && !objects[obj->otyp].oc_name_known) {
        makeknown(obj->otyp);
        more_experienced(0, 10);
    }
}


void
backfire(struct obj *otmp)
{
    otmp->in_use = TRUE;        /* in case losehp() is fatal */
    if (otmp->oartifact) {
        /*
         * Artifacts aren't destroyed by a backfire, but the
         * explosion is more violent.
         */
        pline(msgc_substitute,
              "%s suddenly produces a violent outburst of energy!",
              The(xname(otmp)));
        losehp(dice(otmp->spe + 4, 8), killer_msg(DIED, "an outbursting wand"));
    } else {
        pline(msgc_substitute, "%s suddenly explodes!", The(xname(otmp)));
        do_break_wand(otmp, FALSE);
    }
}

static const char zap_syms[] = { WAND_CLASS, 0 };

int
dozap(const struct nh_cmd_arg *arg)
{
    schar dx = 0, dy = 0, dz = 0;
    struct obj *obj;
    int wandlevel;

    if (check_capacity(NULL))
        return 0;

    obj = getargobj(arg, zap_syms, "zap");
    if (!obj)
        return 0;

    wandlevel = getwandlevel(&youmonst, obj);
    check_unpaid(obj);

    if (obj->oartifact && !touch_artifact(obj, &youmonst))
        return 1;

    /* zappable addition done by GAN 11/03/86 */
    if (!zappable(&youmonst, obj)) {       /* zappable prints the message itself */
    } else if (!wandlevel) {
        backfire(obj);  /* the wand blows up in your face! */
        exercise(A_STR, FALSE);
        return 1;
    } else if (!(objects[obj->otyp].oc_dir == NODIR) &&
               !getargdir(arg, NULL, &dx, &dy, &dz)) {
        if (!Blind)
            pline(msgc_itemloss, "%s glows and fades.", The(xname(obj)));
        /* make him pay for knowing !NODIR */
    } else {

        /* Are we having fun yet? weffects -> buzz(obj->otyp) -> zhitm (temple
           priest) -> attack -> hitum -> known_hitum -> ghod_hitsu ->
           buzz(AD_ELEC) -> destroy_item(WAND_CLASS) -> useup -> obfree ->
           dealloc_obj -> free(obj) */
        turnstate.tracked[ttos_wand] = obj;
        weffects(&youmonst, obj, dx, dy, dz);
        obj = turnstate.tracked[ttos_wand];
        turnstate.tracked[ttos_wand] = 0;
    }
    if (obj && obj->spe < 0) {
        pline(msgc_itemloss, "%s to dust.", Tobjnam(obj, "turn"));
        useup(obj);
    }
    update_inventory(); /* maybe used a charge */
    return 1;
}

/* you've zapped a wand downwards while riding
 * Return TRUE if the steed was hit by the wand.
 * Return FALSE if the steed was not hit by the wand.
 */
/* obj: wand or spell */
static boolean
zap_steed(struct obj *obj)
{
    int steedhit = FALSE;
    int ox = u.ux;
    int oy = u.uy;

    switch (obj->otyp) {

        /*
         * Wands that are allowed to hit the steed
         * Carefully test the results of any that are
         * moved here from the bottom section.
         */
    case WAN_PROBING:
        probe_monster(u.usteed);
        makeknown(WAN_PROBING);
        steedhit = TRUE;
        break;
    case WAN_TELEPORTATION:
    case SPE_TELEPORT_AWAY:
        /* you go together */
        tele();
        if (Teleport_control || (ox != u.ux && oy != u.uy))
            makeknown(obj->otyp);
        steedhit = TRUE;
        break;

        /* Default processing via bhitm() for these */
    case SPE_CURE_SICKNESS:
    case WAN_MAKE_INVISIBLE:
    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
    case WAN_SPEED_MONSTER:
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
    case SPE_DRAIN_LIFE:
    case WAN_OPENING:
    case SPE_KNOCK:
        bhitm(&youmonst, u.usteed, obj, 7);
        steedhit = TRUE;
        break;

    default:
        steedhit = FALSE;
        break;
    }
    return steedhit;
}

/* Cancel a monster (possibly the hero). Inventory is cancelled only if the
   monster is zapping itself directly, since otherwise the effect is too strong.
   Currently non-hero monsters do not zap themselves with cancellation.
   magr is the responsible monster. For now, NULL is valid, for if the source of
   the cancellation is unknown. (In the future, it may not be.) */
boolean
cancel_monst(struct monst *mdef, struct obj *obj, struct monst *magr,
             boolean allow_cancel_kill, boolean self_cancel)
{
    boolean youdefend = (mdef == &youmonst);
    static const char writing_vanishes[] =
        "Some writing vanishes from %s head!";
    static const char your[] = "your";  /* should be extern */

    /* Nonplayers can use resist() in place of MR. */
    boolean has_mr = FALSE;
    if (!youdefend) {
        if (magr != mdef && resist(magr, mdef, obj->oclass, NOTELL, bcsign(obj)))
            has_mr = TRUE;
    }
    if (resists_magm(mdef))
        has_mr = TRUE;

    int wandlevel = 0;
    if (obj->oclass == WAND_CLASS)
        wandlevel = getwandlevel(magr, obj);

    /* Cancel is averted with MR (or for non-players, resist() as well) 100% of the time
       below expert wands (and for the spell), 90% of the time for expert wands,
       50% at the time for master wands */
    if (has_mr && magr != mdef &&
        (!wandlevel || wandlevel < P_EXPERT ||
         (wandlevel == P_EXPERT ? rn2(10) :
          wandlevel == P_MASTER ? rn2(2) : 0)))
        return FALSE;

    if (self_cancel) {  /* 1st cancel inventory */
        struct obj *otmp;

        for (otmp = (youdefend ? invent : mdef->minvent); otmp;
             otmp = otmp->nobj)
            cancel_item(otmp);
    }

    set_property(mdef, CANCELLED, 0, FALSE);
    if (is_were(mdef->data) && mdef->data->mlet != S_HUMAN) {
        if (youdefend)
            you_unwere(FALSE);
        else
            were_change(mdef);
    }

    if (mdef->data == &mons[PM_CLAY_GOLEM]) {
        /* canseemon even if &youmonst -- if you can't see yourself, you can't
           see writing vanishing */
        if (canseemon(mdef))
            pline(combat_msgc(magr, mdef, allow_cancel_kill ? cr_kill : cr_hit),
                  writing_vanishes, youdefend ? your : s_suffix(mon_nam(mdef)));
        if (allow_cancel_kill) {
            if (youdefend)
                rehumanize(DIED, "loss of magical runes");
            else
                monkilled(magr, mdef, "", AD_RBRE);
        }
    }
    return TRUE;
}

/* you or a monster zapped an immediate type wand up or down */
/* obj: wand or spell */
static boolean
zap_updown(struct monst *mon, struct obj *obj, schar dz)
{
    boolean striking = FALSE, disclose = FALSE;
    int x, y, xx, yy, ptmp;
    struct obj *otmp;
    struct engr *e;
    struct trap *ttmp;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);

    /* some wands have special effects other than normal bhitpile */
    /* drawbridge might change <u.ux,u.uy> */
    x = xx = m_mx(mon);      /* <x,y> is zap location */
    y = yy = m_my(mon);      /* <xx,yy> is drawbridge (portcullis) position */
    ttmp = t_at(level, x, y);   /* trap if there is one */

    switch (obj->otyp) {
    case WAN_PROBING:
        ptmp = 0;
        if (dz < 0) {
            if (you || vis)
                pline(you ? msgc_yafm : msgc_monneutral,
                      "%s towards the %s.",
                      M_verbs(mon, "probe"),
                      ceiling(m_mx(mon), m_my(mon)));
        } else {
            if (you)
                ptmp += bhitpile(obj, bhito, x, y);
            if (you || vis)
                pline(you ? msgc_info : msgc_monneutral,
                      "%s beneath the %s.",
                      M_verbs(mon, "probe"),
                      surface(m_mx(mon), m_my(mon)));
            if (you)
                ptmp += display_binventory(x, y, TRUE);
        }
        if (!ptmp && you)
            pline(msgc_info, "Your probe reveals nothing.");
        return TRUE;    /* we've done our own bhitpile */
    case WAN_OPENING:
    case SPE_KNOCK:
        /* up or down, but at closed portcullis only */
        if (is_db_wall(x, y) && find_drawbridge(&xx, &yy)) {
            open_drawbridge(xx, yy);
            disclose = TRUE;
        } else if (dz > 0 &&
                   (x == level->dnstair.sx && y == level->dnstair.sy) &&
                   /* can't use the stairs down to quest level 2 until leader
                      "unlocks" them; give feedback if you try */
                   on_level(&u.uz, &qstart_level) && !ok_to_quest(FALSE)) {
            if (you || vis)
                pline(you ? msgc_hint : msgc_monneutral,
                      "The stairs seem to ripple momentarily.");
            disclose = TRUE;
        }
        break;
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        striking = TRUE;
        /*FALLTHRU*/
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        /* down at open bridge or up or down at open portcullis */
        if ((level->locations[x][y].typ == DRAWBRIDGE_DOWN) ? (dz > 0)
            : (is_drawbridge_wall(x, y) && !is_db_wall(x, y)) &&
            find_drawbridge(&xx, &yy)) {
            if (!striking)
                close_drawbridge(xx, yy);
            else
                destroy_drawbridge(xx, yy);
            disclose = TRUE;
        } else if (striking && dz < 0 && rn2(3) && !Is_airlevel(&u.uz) &&
                   !Is_waterlevel(&u.uz) && !Underwater && !Is_qstart(&u.uz)) {
            /* similar to zap_dig() */
            pline(you ? msgc_badidea : msgc_monneutral,
                  "A rock is dislodged from the %s and falls on %s %s.",
                  ceiling(x, y), you ? "your" : s_suffix(mon_nam(mon)),
                  mbodypart(mon, HEAD));
            struct obj *armh = which_armor(mon, os_armh);
            if (you)
                losehp(rnd((armh && is_metallic(armh)) ? 2 : 6),
                       killer_msg(DIED, "smashing up the ceiling"));
            else {
                mon->mhp -= rnd((armh && is_metallic(armh)) ? 2 : 6);
                if (mon->mhp <= 0)
                    mondied(mon);
            }
            if ((otmp = mksobj_at(ROCK, level, x, y, FALSE, FALSE, rng_main))) {
                examine_object(otmp);    /* set dknown, maybe bknown */
                stackobj(otmp);
            }
            newsym(x, y);
        } else if (!striking && ttmp && ttmp->ttyp == TRAPDOOR && dz > 0) {
            if (!blind(&youmonst) || (!you && !vis)) {
                if (ttmp->tseen) {
                    pline(msgc_actionok,
                          "A trap door beneath %s closes up then vanishes.",
                          you ? "you" : mon_nam(mon));
                    disclose = TRUE;
                } else {
                    pline(msgc_actionok, "You see a swirl of %s beneath %s.",
                          is_ice(level, x, y) ? "frost" : "dust",
                          you ? "you" : mon_nam(mon));
                }
            } else
                You_hear(msgc_levelsound, "a twang followed by a thud.");
            deltrap(level, ttmp);
            ttmp = NULL;
            newsym(x, y);
        }
        break;
    case SPE_STONE_TO_FLESH:
        if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) || Underwater ||
            (Is_qstart(&u.uz) && dz < 0)) {
            if (you || vis)
                pline(msgc_yafm, "Nothing happens.");
        } else if (dz < 0) {    /* we should do more... */
            if (you || vis)
                pline(msgc_yafm, "Blood drips on %s %s.",
                      you ? "your" : s_suffix(mon_nam(mon)),
                      body_part(FACE));
        } else if (dz > 0 && !OBJ_AT(u.ux, u.uy)) {
            /* Print this message only if there wasn't an engraving affected
               here.  If water or ice, act like waterlevel case. */
            e = engr_at(level, u.ux, u.uy);
            if (!(e && e->engr_type == ENGRAVE)) {
                if (is_pool(level, u.ux, u.uy) || is_ice(level, u.ux, u.uy)) {
                    if (you || vis)
                        pline(msgc_yafm, "Nothing happens.");
                } else if (you || vis)
                    pline(msgc_yafm, "Blood %ss %s your %s.",
                          is_lava(level, u.ux, u.uy) ? "boil" : "pool",
                          Levitation ? "beneath" : "at",
                          makeplural(body_part(FOOT)));
            }
        }
        break;
    default:
        break;
    }

    if (dz > 0) {
        /* zapping downward */
        bhitpile(obj, bhito, x, y);

        /* subset of engraving effects; none sets `disclose' */
        if ((e = engr_at(level, x, y)) != 0 && e->engr_type != HEADSTONE) {
            switch (obj->otyp) {
            case WAN_POLYMORPH:
            case SPE_POLYMORPH:
                del_engr(e, level);
                make_engr_at(level, x, y, random_engraving(rng_main), moves,
                             (xchar) 0);
                break;
            case WAN_CANCELLATION:
            case SPE_CANCELLATION:
            case WAN_MAKE_INVISIBLE:
                del_engr(e, level);
                break;
            case WAN_TELEPORTATION:
            case SPE_TELEPORT_AWAY:
                rloc_engr(e);
                break;
            case SPE_STONE_TO_FLESH:
                if (e->engr_type == ENGRAVE) {
                    /* only affects things in stone */
                    pline(msgc_actionok,
                          Hallucination ? "The floor runs like butter!" :
                          "The edges on the floor get smoother.");
                    wipe_engr_at(level, x, y, dice(2, 4));
                }
                break;
            case WAN_STRIKING:
            case SPE_FORCE_BOLT:
                wipe_engr_at(level, x, y, dice(2, 4));
                break;
            default:
                break;
            }
        }
    }

    return disclose;
}


/* called for various wand and spell effects - M. Stephenson */
void
weffects(struct monst *mon, struct obj *obj, schar dx, schar dy, schar dz)
{
    int otyp = obj->otyp;
    int wandlevel = 0;
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    if (obj->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mon, obj);
    if (you && wandlevel)
        use_skill(P_WANDS, wandlevel); /* successful wand use exercises */
    boolean disclose = FALSE, was_unkn = !objects[otyp].oc_name_known;

    if (!dx && !dy && !dz && objects[otyp].oc_dir == IMMEDIATE) { /* zapped self */
        bhitm(mon, mon, obj, 7);
        return;
    }

    if (you)
        exercise(A_WIS, TRUE);
    if (you && u.usteed && (objects[otyp].oc_dir != NODIR) &&
        !dx && !dy && (dz > 0) &&
        zap_steed(obj)) {
        disclose = TRUE;
    } else if (objects[otyp].oc_dir == IMMEDIATE || otyp == EXPENSIVE_CAMERA) {
        if (Engulfed) {
            bhitm(&youmonst, u.ustuck, obj, 7);
            /* [how about `bhitpile(u.ustuck->minvent)' effect?] */
        } else if (dz) {
            disclose = zap_updown(mon, obj, dz);
        } else if (dx || dy) {
            bhit(mon, dx, dy, rn1(8, 6), obj);
        }
    } else if (objects[otyp].oc_dir == NODIR) {
        zapnodir(mon, obj);

    } else {
        /* neither immediate nor directionless */

        if (otyp == WAN_DIGGING || otyp == SPE_DIG)
            zap_dig(mon, obj, dx, dy, dz);
        else if (otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_FINGER_OF_DEATH) {
            int divisor = 2;
            int wandlvl = 0;
            if (otyp == SPE_MAGIC_MISSILE) {
                int skill;
                if (you)
                    skill = P_SKILL(P_ATTACK_SPELL);
                else
                    skill = mprof(mon, MP_SATTK);
                divisor = (skill == P_UNSKILLED ? 5 :
                           skill == P_BASIC ? 4 :
                           skill == P_SKILLED ? 3 :
                           2);
                if (skill >= P_SKILLED)
                    wandlvl = skill;
            }
            if (you)
                buzz(otyp - SPE_MAGIC_MISSILE + 10, u.ulevel / divisor + 1, u.ux, u.uy,
                     dx, dy, wandlvl);
            else
                buzz(-10 - (otyp - SPE_MAGIC_MISSILE), mon->m_lev / divisor + 1,
                     m_mx(mon), m_my(mon), dx, dy, wandlvl);
        } else if (otyp >= WAN_MAGIC_MISSILE && otyp <= WAN_LIGHTNING) {
            if (you)
                buzz(otyp - WAN_MAGIC_MISSILE, (wandlevel == P_UNSKILLED) ? 3 : 6,
                     u.ux, u.uy, dx, dy, wandlevel);
            else
                buzz((int)(-30 - (otyp - WAN_MAGIC_MISSILE)),
                     wandlevel == P_UNSKILLED ? 3 : 6, m_mx(mon), m_my(mon),
                     dx, dy, wandlevel);
        } else
            impossible("weffects: unexpected spell or wand x:%d",
                       objects[otyp].oc_dir);
        disclose = TRUE;
    }
    if (disclose && (you || vis) && was_unkn) {
        makeknown(otyp);
        more_experienced(0, 10);
    }
    return;
}


/*
 * Generate the to damage bonus for a spell. Based on the hero's intelligence
 */
int
spell_damage_bonus(void)
{
    int tmp, intell = ACURR(A_INT);

    /* Punish low intellegence before low level else low intellegence gets
       punished only when high level */
    if (intell < 10)
        tmp = -3;
    else if (u.ulevel < 5)
        tmp = 0;
    else if (intell < 14)
        tmp = 0;
    else if (intell <= 18)
        tmp = 1;
    else        /* helm of brilliance */
        tmp = 2;

    return tmp;
}

/*
 * Generate the to hit bonus for a spell.  Based on the hero's skill in
 * spell class and dexterity.
 */
static int
spell_hit_bonus(int skill)
{
    int hit_bon = 0;
    int dex = ACURR(A_DEX);

    switch (P_SKILL(spell_skilltype(skill))) {
    case P_ISRESTRICTED:
    case P_UNSKILLED:
        hit_bon = -4;
        break;
    case P_BASIC:
        hit_bon = 0;
        break;
    case P_SKILLED:
        hit_bon = 2;
        break;
    case P_EXPERT:
        hit_bon = 3;
        break;
    }

    if (dex < 4)
        hit_bon -= 3;
    else if (dex < 6)
        hit_bon -= 2;
    else if (dex < 8)
        hit_bon -= 1;
    else if (dex < 14)
        hit_bon -= 0;   /* Will change when print stuff below removed */
    else
        hit_bon += dex - 14;    /* Even increment for dextrous heroes (see
                                   weapon.c abon) */

    return hit_bon;
}

const char *
exclam(int force)
{
    /* force == 0 occurs e.g. with sleep ray */
    /* note that large force is usual with wands so that !! would require
       information about hand/weapon/wand */
    return (const char *)((force < 0) ? "?" : (force <= 4) ? "." : "!");
}

void
hit(const char *str, struct monst *mdef, const char *force,
    struct monst *magr)
{       /* usually either "." or "!" */
    if (((!cansee(bhitpos.x, bhitpos.y) && !canspotmon(mdef) &&
          !(Engulfed && mdef == u.ustuck)) ||
         !flags.verbose) && mdef != &youmonst)
        pline(combat_msgc(magr, mdef, cr_hit),
              "%s %s it.", The(str), vtense(str, "hit"));
    else
        pline(combat_msgc(magr, mdef, cr_hit),
              "%s %s %s%s", The(str), vtense(str, "hit"),
              mdef == &youmonst ? "you" : mon_nam(mdef), force);
}

void
miss(const char *str, struct monst *mdef, struct monst *magr)
{
    pline(combat_msgc(magr, mdef, cr_miss), "%s %s %s.",
          The(str), vtense(str, "miss"),
          ((cansee(bhitpos.x, bhitpos.y) || canspotmon(mdef) ||
            mdef == &youmonst) && flags.verbose) ?
          (mdef == &youmonst ? "you" : mon_nam(mdef)) : "it");
}

/* bhit() -- zap immediate wand in any direction. */
static void
bhit(struct monst *mon, int dx, int dy, int range, struct obj *obj) {
    struct monst *mdef;
    struct obj *otmp;
    struct tmp_sym *tsym = NULL;
    uchar typ;
    boolean shopdoor = FALSE; /* for determining if you should pay for ruining a door */
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);

    bhitpos.x = m_mx(mon);
    bhitpos.y = m_my(mon);
    if (obj->otyp == EXPENSIVE_CAMERA)
        tsym = tmpsym_init(DISP_BEAM, dbuf_effect(E_MISC, E_flashbeam));

    while (range-- > 0) {
        int x, y;

        bhitpos.x += dx;
        bhitpos.y += dy;
        x = bhitpos.x;
        y = bhitpos.y;

        if (!isok(x, y)) {
            bhitpos.x -= dx;
            bhitpos.y -= dy;
            break;
        }
        
        typ = level->locations[bhitpos.x][bhitpos.y].typ;
        if (tsym) {
            tmpsym_at(tsym, bhitpos.x, bhitpos.y);
            win_delay_output();
        }
        if (find_drawbridge(&x, &y))
            switch (obj->otyp) {
            case WAN_OPENING:
            case SPE_KNOCK:
                if (is_db_wall(bhitpos.x, bhitpos.y)) {
                    if (cansee(x, y) || cansee(bhitpos.x, bhitpos.y))
                        if (you || vis)
                            makeknown(obj->otyp);
                    open_drawbridge(x, y);
                }
                break;
            case WAN_LOCKING:
            case SPE_WIZARD_LOCK:
                if ((cansee(x, y) || cansee(bhitpos.x, bhitpos.y))
                    && level->locations[x][y].typ == DRAWBRIDGE_DOWN)
                    if (you || vis)
                        makeknown(obj->otyp);
                close_drawbridge(x, y);
                break;
            case WAN_STRIKING:
            case SPE_FORCE_BOLT:
                if (typ != DRAWBRIDGE_UP)
                    destroy_drawbridge(x, y);
                if (you || vis)
                    makeknown(obj->otyp);
                break;
            }
        mdef = m_at(level, bhitpos.x, bhitpos.y);
        if (!mdef && bhitpos.x == u.ux && bhitpos.y == u.uy)
            mdef = &youmonst;
        if (mdef) {
            if (cansee(bhitpos.x, bhitpos.y) && !canspotmon(mdef))
                map_invisible(bhitpos.x, bhitpos.y);
            if (obj->otyp != EXPENSIVE_CAMERA || !invisible(mdef))
                range -= 3;
            bhitm(mon, mdef, obj, min(range, 1));
        } else if ((mdef = vismon_at(level, bhitpos.x, bhitpos.y))) {
            pline(combat_msgc(mon, mdef, cr_miss),
                  "The %s passes through %s displaced image!",
                  obj->otyp == EXPENSIVE_CAMERA ? "flash" : "beam",
                  mdef == &youmonst ? "your" : s_suffix(mon_nam(mdef)));
        }
        /* boulders block vision, so make them block camera flashes too... */
        if (sobj_at(BOULDER, level, bhitpos.x, bhitpos.y) &&
            obj->otyp == EXPENSIVE_CAMERA)
            range = 0;

        if (bhitpile(obj, bhito, bhitpos.x, bhitpos.y))
            range--;
        if (you && obj->otyp == WAN_PROBING &&
            level->locations[bhitpos.x][bhitpos.y].mem_invis) {
            level->locations[bhitpos.x][bhitpos.y].mem_invis = 0;
            newsym(x, y);
        }
        if (IS_DOOR(typ) || typ == SDOOR) {
            switch (obj->otyp) {
            case WAN_OPENING:
            case WAN_LOCKING:
            case WAN_STRIKING:
            case SPE_KNOCK:
            case SPE_WIZARD_LOCK:
            case SPE_FORCE_BOLT:
                if (doorlock(obj, bhitpos.x, bhitpos.y)) {
                    if (you || vis)
                        makeknown(obj->otyp);
                    if (level->locations[bhitpos.x][bhitpos.y].doormask ==
                        D_BROKEN &&
                        *in_rooms(level, bhitpos.x, bhitpos.y, SHOPBASE)) {
                        shopdoor = TRUE;
                        add_damage(bhitpos.x, bhitpos.y, you ? 400L : 0L);
                    }
                }
                break;
            }
        }
        if (!ZAP_POS(typ) ||
            (IS_DOOR(typ) &&
             (level->locations[bhitpos.x][bhitpos.y].
              doormask & (D_LOCKED | D_CLOSED)))
            ) {
            bhitpos.x -= dx;
            bhitpos.y -= dy;
            break;
        }
    }

    if (tsym)
        tmpsym_end(tsym);

    if (you && shopdoor)
        pay_for_damage("destroy", FALSE);
}

/* will zap/spell/breath attack score a hit against armor class `ac'? */
static int
zap_hit_check(int ac, int type)
{       /* either hero cast spell type or 0 */
    int chance = rn2(20);
    int spell_bonus = type ? spell_hit_bonus(type) : 0;

    /* small chance for naked target to avoid being hit */
    if (!chance)
        return rnd(10) < ac + spell_bonus;

    /* very high armor protection does not achieve invulnerability */
    ac = AC_VALUE(ac);

    return (3 - chance) < ac + spell_bonus;
}

/* type ==   0 to   9 : you shooting a wand */
/* type ==  10 to  19 : you casting a spell */
/* type ==  20 to  29 : you breathing as a monster */
/* type == -10 to -19 : monster casting spell */
/* type == -20 to -29 : monster breathing at you */
/* type == -30 to -39 : monster shooting a wand */
/* called with dx = dy = 0 with vertical bolts */
/* buzztyp gives a consistent number for kind of ray, magr can be used combined
   with this to determine source+kind without unneccessary arithmetic */
void
buzz(int type, int nd, xchar sx, xchar sy, int dx, int dy, int raylevel)
{
    int range;
    int expltype;
    struct rm *loc;
    xchar lsx, lsy;
    struct monst *mon;
    coord save_bhitpos;
    boolean shopdamage = FALSE;
    const char *fltxt;
    struct tmp_sym *tsym;
    int spell_type;
    int buzztyp = abs(type) % 30; /* % 30 to catch -30-39 */
    boolean selfreflect = FALSE;
    boolean selfzap = FALSE;
    boolean spell = (buzztyp >= 10 && buzztyp <= 19);
    /* has nothing to do with absolute value. TODO: give this var a better name */
    int abstype = buzztyp % 10;
    /* you = buzz hits you, vis = you can see mon, yours = you created the buzz */
    boolean you = FALSE;
    boolean vis = FALSE;
    boolean yours = (type >= 0);
    /* TODO: source of buzz in a better way */
    struct monst *magr = NULL;
    if (buzztyp != ZT_SPELL(ZT_LIGHTNING)) {
        magr = m_at(level, sx, sy);
        if (yours)
            magr = &youmonst;
    }

    /* if its a Hero Spell then get its SPE_TYPE */
    spell_type = is_hero_spell(type) ? SPE_MAGIC_MISSILE + abstype : 0;

    fltxt = flash_types[buzztyp];
    if (Engulfed && yours) {
        zap_hit_mon(magr, u.ustuck, type, nd, raylevel, FALSE);
        return;
    }
    newsym(u.ux, u.uy);
    range = rn1(7, 7);
    if (dx == 0 && dy == 0) {
        range = 1;
        selfzap = TRUE;
    }
    save_bhitpos = bhitpos;

    tsym = tmpsym_init(DISP_BEAM, zapdir_to_effect(dx, dy, abstype));
    while (range-- > 0) {
        you = FALSE;
        vis = FALSE;
        lsx = sx;
        sx += dx;
        lsy = sy;
        sy += dy;
        if (isok(sx, sy) && (loc = &level->locations[sx][sy])->typ) {
            mon = m_at(level, sx, sy);
            if (!mon && sx == u.ux && sy == u.uy) {
                mon = &youmonst;
                if (u.usteed && !rn2(3) && !reflecting(u.usteed))
                    mon = u.usteed;
                you = (mon == &youmonst);
            }
            if (cansee(sx, sy)) {
                /* reveal/unreveal invisible monsters before tmpsym_at() */
                if (!you && mon && !canspotmon(mon))
                    map_invisible(sx, sy);
                else if ((!mon || you) && level->locations[sx][sy].mem_invis) {
                    level->locations[sx][sy].mem_invis = FALSE;
                    newsym(sx, sy);
                }
                if (ZAP_POS(loc->typ) || cansee(lsx, lsy))
                    tmpsym_at(tsym, sx, sy);
                win_delay_output();     /* wait a little */
            }
        } else
            goto make_bounce;

        /* hit() and miss() need bhitpos to match the target */
        bhitpos.x = sx, bhitpos.y = sy;
        /* Fireballs only damage when they explode */
        if (buzztyp != ZT_SPELL(ZT_FIRE))
            range += zap_over_floor(sx, sy, type, &shopdamage);

        if (mon) {
            selfreflect = FALSE;
            if (mon == magr)
                selfreflect = TRUE;
            vis = canseemon(mon);
            if (buzztyp == ZT_SPELL(ZT_FIRE))
                break;
            if (yours && !you && idle(mon))
                mon->mstrategy = st_none;
            if (selfzap ||
                zap_hit_check(find_mac(mon), spell_type)) {
                range -= 2;
                if (!selfzap && /* the fact that it hits is implied */
                    (yours || you || vis))
                    hit(fltxt, mon, "!", magr);
                if (reflecting(mon) && !selfzap) {
                    if (you || vis) {
                        shieldeff(sx, sy);
                        if (raylevel >= P_SKILLED) {
                            if (blind(&youmonst))
                                pline(combat_msgc(magr, mon, cr_resist),
                                      "%s is disrupted by something!", The(fltxt));
                            else
                                mon_reflects(mon, magr, selfreflect,
                                             "%s is disrupted by %s %s!",
                                             The(fltxt));
                        } else {
                            if (blind(&youmonst))
                                pline(combat_msgc(magr, mon, cr_immune),
                                      "For some reason, %s %s not affected.",
                                      you ? "you" : mon_nam(mon),
                                      you ? "are" : "is");
                            else
                                mon_reflects(mon, magr, selfreflect,
                                             "But %s reflects from %s %s!", the(fltxt));
                        }
                    }
                    if (raylevel >= P_SKILLED) {
                        range = 0;
                        if (spell)
                            zap_hit_mon(magr, mon, type, nd, raylevel, selfzap);
                        continue;
                    }
                    dx = -dx;
                    dy = -dy;
                } else if (raylevel == P_MASTER) {
                    pline(combat_msgc(magr, mon, cr_hit),
                          "The powerful %s explodes!", fltxt);
                    range = 0;
                    continue;
                } else
                    zap_hit_mon(magr, mon, type, nd, raylevel, selfzap);
            } else if (you || yours || vis)
                pline(combat_msgc(magr, mon, cr_miss),
                      "%s whizzes by %s!", The(fltxt),
                      you ? "you" : mon_nam(mon));
            if (you)
                action_interrupted();
        } else if ((mon = vismon_at(level, sx, sy)) &&
                   (yours || cansee(sx, sy)))
            pline(combat_msgc(magr, mon, cr_miss),
                  "%s whizzes by %s displaced image!", The(fltxt),
                  you ? "your" : s_suffix(mon_nam(mon)));

        if (!ZAP_POS(loc->typ) ||
            (closed_door(level, sx, sy) && (range >= 0))) {
            int bounce;
            uchar rmn;

        make_bounce:
            if (buzztyp == ZT_SPELL(ZT_FIRE)) {
                sx = lsx;
                sy = lsy;
                break;  /* fireballs explode before the wall */
            }
            bounce = 0;
            range--;
            if (range && isok(lsx, lsy) && cansee(lsx, lsy))
                pline(msgc_consequence, "%s bounces!", The(fltxt));
            if (!dx || !dy || !rn2(20)) {
                dx = -dx;
                dy = -dy;
            } else {
                if (isok(sx, lsy) &&
                    ZAP_POS(rmn = level->locations[sx][lsy].typ) &&
                    !closed_door(level, sx, lsy) &&
                    (IS_ROOM(rmn) ||
                     (isok(sx + dx, lsy) &&
                      ZAP_POS(level->locations[sx + dx][lsy].typ))))
                    bounce = 1;
                if (isok(lsx, sy) &&
                    ZAP_POS(rmn = level->locations[lsx][sy].typ) &&
                    !closed_door(level, lsx, sy) &&
                    (IS_ROOM(rmn) ||
                     (isok(lsx, sy + dy) &&
                      ZAP_POS(level->locations[lsx][sy + dy].typ))))
                    if (!bounce || rn2(2))
                        bounce = 2;

                switch (bounce) {
                case 0:
                    dx = -dx;   /* fall into... */
                case 1:
                    dy = -dy;
                    break;
                case 2:
                    dx = -dx;
                    break;
                }
                tmpsym_change(tsym, zapdir_to_effect(dx, dy, abstype));
            }
        }
    }
    tmpsym_end(tsym);
    if (buzztyp == ZT_SPELL(ZT_FIRE))
        explode(sx, sy, type, dice(12, 6), 0, EXPL_FIERY, NULL, 0);
    if (raylevel >= P_SKILLED && !spell) {
        if (abstype == ZT_FIRE)
            expltype = EXPL_FIERY;
        else if (abstype == ZT_COLD)
            expltype = EXPL_FROSTY;
        else
            expltype = EXPL_MAGICAL;
        explode(sx, sy, type, dice(nd, 6), WAND_CLASS, expltype, NULL, raylevel);
        if (raylevel == P_MASTER)
            chain_explode(sx, sy, type, dice(nd, 6),
                          WAND_CLASS, expltype, NULL, raylevel, rnd(5));
    }
    if (shopdamage)
        pay_for_damage(abstype == ZT_FIRE ? "burn away" : abstype ==
                       ZT_COLD ? "shatter" : abstype ==
                       ZT_DEATH ? "disintegrate" : "destroy", FALSE);
    bhitpos = save_bhitpos;
}

/* returns damage to mon */
/* selfzap is special, it implies magr==mdef, but the reverse isn't true.
   Basically, it is used when the monster zapped the wand at itself,
   not when it happens to catch itself in a rebound. */
static void
zap_hit_mon(struct monst *magr, struct monst *mdef, int type,
            int nd, int raylevel, boolean selfzap)
{
    int tmp = dice(nd, 6);
    int abstype = abs(type) % 10;
    int resisted = 0; /* 1=show msg, 2=don't */
    const char *ztyp;
    const char *fltxt;
    boolean oseen = FALSE;
    oseen = mon_visible(mdef);
    boolean you = (mdef == &youmonst);
    if (you)
        oseen = TRUE;
    boolean spellcaster = is_hero_spell(type);  /* maybe get a bonus! */
    int buzztyp = (type <= -30 ? abstype : abs(type));
    boolean yours = (type >= 0);
    boolean disintegrated = (buzztyp == ZT_BREATH(ZT_DEATH));
    boolean wand = (buzztyp < 10);
    int bcsign = 0;
    struct obj *otmp;

    /* we need to figure out BUC for potential wand for resist() */
    int wandlevel = P_SKILL(P_WANDS);
    if (magr && magr != &youmonst)
        wandlevel = mprof(magr, MP_WANDS);
    if (raylevel)
        bcsign = raylevel - wandlevel;

    fltxt = flash_types[buzztyp];
    switch (abstype) {
    case ZT_MAGIC_MISSILE:
        if (resists_magm(mdef)) {
            if (raylevel >= P_EXPERT &&
                (magr != mdef || wand)) {
                tmp = (tmp / 2) + 1;
                if (oseen)
                    pline(combat_msgc(magr, mdef, cr_resist),
                          "Some of the missiles bounce off!");
            } else {
                resisted = 2;
                if (oseen)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "The missiles bounce off!");
            }
            break;
        }
        if (you)
            exercise(A_STR, FALSE);
        if (!resisted && selfzap && oseen)
            pline(you ? msgc_badidea : msgc_yafm,
                  "%s%s shot %sself!", you ? "Idiot!  " : "", M_verbs(mdef, "have"),
                  you ? "your" : mhim(mdef));
        break;
    case ZT_FIRE:
        ztyp = "hot";
        if (resists_fire(mdef)) {
            resisted = 1;
            if (likes_fire(mdef->data)) {
                /* innately hot creatures probably look hot... */
                resisted = 2;
                if (oseen)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s unharmed!", M_verbs(mdef, "are"));
            }
        }
        if (resists_cold(mdef))
            tmp += dice(nd, 3);
        if (!resisted && selfzap && oseen)
            pline(you ? msgc_badidea : msgc_yafm,
                  "%s set %sself on fire!", M_verbs(mdef, "have"),
                  you ? "your" : mhim(mdef));
        burn_away_slime(mdef);
        golemeffects(mdef, AD_FIRE, tmp);
        if (raylevel != P_UNSKILLED && burnarmor(mdef)) {
            if (!rn2(3))
                destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
            if (!rn2(3))
                destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
            if (!rn2(5))
                destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
        }
        break;
    case ZT_COLD:
        ztyp = "cold";
        if (resists_cold(mdef))
            resisted = 1;
        if (resists_fire(mdef))
            tmp += dice(nd, 3);
        if (!resisted && selfzap && oseen)
            pline(you ? msgc_badidea : msgc_yafm,
                  "%s a popsicle!", M_verbs(mdef, "imitate"));
        golemeffects(mdef, AD_COLD, tmp);
        if (raylevel != P_UNSKILLED && !rn2(3))
            destroy_mitem(mdef, POTION_CLASS, AD_COLD);
        break;
    case ZT_SLEEP:
        ztyp = "sleepy";
        tmp = 0;
        if (resists_sleep(mdef))
            resisted = 1;
        else {
            sleep_monst(magr, mdef, dice(nd, 6),
                        buzztyp == ZT_WAND(ZT_SLEEP) ? WAND_CLASS : '\0');
            if (!you)
                slept_monst(mdef);
        }
        break;
    case ZT_DEATH:     /* death/disintegration */
        ztyp = "dead";
        if (!disintegrated) { /* death */
            if (mdef->data == &mons[PM_DEATH]) {
                if (canseemon(mdef)) {
                    pline_implied(combat_msgc(magr, mdef, cr_immune),
                                  "%s absorbs the deadly %s!", Monnam(mdef),
                                  buzztyp < ZT_SPELL(0) ? "ray" : "spell");
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "It seems even stronger than before.");
                }
                mdef->mhpmax += mdef->mhpmax / 2;
                if (mdef->mhpmax > 999) /* arbitrary -- cap HP at 999 */
                    mdef->mhpmax = 999;
                mdef->mhp = mdef->mhpmax;
                return;
            }
            if (nonliving(mdef->data) || is_demon(mdef->data) ||
                (!selfzap && resists_magm(mdef)) ||
                raylevel == P_UNSKILLED) {
                if (resists_drli(mdef) || raylevel <= P_SKILLED) {
                    resisted = 2;
                    if (oseen) {
                        if (selfzap && wand)
                            pline(msgc_yafm, "The wand shoots an apparently "
                                  "harmless beam at %s.", you ? "you" : mon_nam(mdef));
                        else
                            pline(combat_msgc(magr, mdef, cr_immune),
                                  "%s unaffected.", M_verbs(mdef, "are"));
                    }
                    break;
                } else {
                    if (you) {
                        losexp("drained by a wand of death", FALSE);
                        return;
                    } else {
                        tmp = dice(2, 6);
                        if (oseen)
                            pline(combat_msgc(magr, mdef, cr_hit),
                                  "%s suddenly seems weaker!", Monnam(mdef));
                        mdef->mhpmax -= tmp;
                        mdef->mhp -= tmp;
                        if (mdef->m_lev > 0) {
                            mdef->m_lev--;
                            return;
                        }
                        /* level 0 monsters are killed below */
                    }
                }
            } else if (selfzap && (you || oseen))
                pline(combat_msgc(magr, mdef, cr_kill),
                      "%s %sself with pure energy!", M_verbs(mdef, "irradiate"),
                      you ? "your" : mhim(mdef));
        } else { /* disintegration */
            if (is_rider(mdef->data)) {
                if (canseemon(mdef)) {
                    pline_implied(combat_msgc(magr, mdef, cr_immune),
                                  "%s disintegrates.", Monnam(mdef));
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s body reintegrates before your %s!",
                          s_suffix(Monnam(mdef)),
                          (eyecount(youmonst.data) ==
                           1) ? body_part(EYE) :
                          makeplural(body_part(EYE)));
                    pline_implied(combat_msgc(magr, mdef, cr_immune),
                                  "%s resurrects!", Monnam(mdef));
                }
                mdef->mhp = mdef->mhpmax;
                return;
            }
            int dummy;
            /* Destroy shield first, then suit, then monster.
               If suit or monster is to be disintegrated,
               also destroy cloak. If monster is to be
               disintegrated, also destroy shirt. */
            if ((otmp = which_armor(mdef, os_arms)) ||
                ((otmp = which_armor(mdef, os_arm)) &&
                 otmp != uskin())) {
                struct obj *otmp2;
                if (is_suit(otmp) &&
                    (otmp2 = which_armor(mdef, os_armc))) {
                    if (!item_provides_extrinsic(otmp2, DISINT_RES, &dummy))
                        destroy_arm(mdef, otmp2);
                }

                if (!item_provides_extrinsic(otmp, DISINT_RES, &dummy))
                    destroy_arm(mdef, otmp);
                return;
            }

            if ((otmp = which_armor(mdef, os_armc)) &&
                !item_provides_extrinsic(otmp, DISINT_RES, &dummy))
                destroy_arm(mdef, otmp);
            if ((otmp = which_armor(mdef, os_armu)) &&
                !item_provides_extrinsic(otmp, DISINT_RES, &dummy))
                destroy_arm(mdef, otmp);

            if (resists_disint(mdef)) {
                shieldeff(m_mx(mdef), m_my(mdef));
                if (oseen)
                    pline(combat_msgc(magr, mdef, cr_immune),
                          "%s not disintegrated.", M_verbs(mdef, "are"));
                return;
            }
            pline(combat_msgc(magr, mdef, cr_kill),
                  "%s disintegrated!", M_verbs(mdef, "are"));
        }
        if (you) {
            /* when killed by disintegration breath, don't leave corpse */
            u.ugrave_arise = disintegrated ? -3 : NON_PM;
            done(DIED, killer_msg(DIED, an(fltxt)));
        } else {
            mdef->mhp = -1;
            monkilled(magr, mdef, "", disintegrated ? -AD_RBRE : AD_RBRE);
        }
        return; /* lifesaved */
    case ZT_LIGHTNING:
        ztyp = "shocked";
        if (resists_elec(mdef))
            resisted = 1;
        else if (you)
            exercise(A_CON, FALSE);
        if (!resisted && selfzap && oseen)
            pline(you ? msgc_badidea : msgc_yafm,
                  "%s %sself", M_verbs(mdef, "shock"), you ? "your" : mhim(mdef));
        if (!resists_blnd(mdef) && !(yours && Engulfed && mdef == u.ustuck)) {
            if (oseen)
                pline(combat_msgc(magr, mdef, cr_hit),
                      "%s blinded by the flash!", M_verbs(mdef, "are"));
            inc_timeout(mdef, BLINDED, rnd(50), TRUE);
            if (!blind(mdef))
                pline(combat_msgc(magr, mdef, cr_immune),
                      "%s vision quickly clears.",
                      you ? "Your" : s_suffix(Monnam(mdef)));
        }
        golemeffects(mdef, AD_ELEC, tmp);
        if (raylevel != P_UNSKILLED && !rn2(3))
            destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
        break;
    case ZT_POISON_GAS:
        ztyp = "affected";
        if (resists_poison(mdef)) {
            resisted = 1;
            break;
        }

        tmp = 0;
        poisoned(mdef, "blast", A_DEX, killer_msg(DIED, "a poisoned blast"), 15);
        break;
    case ZT_ACID:
        ztyp = "burned";
        if (resists_acid(mdef))
            resisted = 1;
        else if (you) {
            pline_implied(combat_msgc(magr, mdef, cr_hit),
                          "The acid burns!");
            exercise(A_STR, FALSE);
        }
        /* using two weapons at once makes both of them more vulnerable */
        if (!rn2((you && u.twoweap) ? 3 : 6))
            acid_damage(MON_WEP(mdef));
        if (you && u.twoweap && !rn2(3))
            acid_damage(uswapwep);
        if (!rn2(6))
            hurtarmor(mdef, ERODE_CORRODE);
        break;
    case ZT_STUN:
        ztyp = "stunned";
        if (you)
            exercise(A_DEX, FALSE);
        inc_timeout(mdef, STUNNED, tmp, FALSE);
        tmp = 0;
        break;
    }
    if (resisted) {
        shieldeff(m_mx(mdef), m_my(mdef));
        if (ztyp && resisted == 1)
            pline(combat_msgc(magr, mdef, cr_immune),
                  "%s %s %s.",
                  you ? "You" : Monnam(mdef),
                  you ? "don't feel" : "doesn't seem",
                  ztyp);
        return;
    }
    if (tmp && spellcaster)
        tmp += spell_damage_bonus();
    if (is_hero_spell(type) && Role_if(PM_KNIGHT) &&
        Uhave_questart && mdef->data != &mons[PM_KNIGHT])
        tmp *= 2;
    if (tmp > 0 && resist(magr, mdef, buzztyp < ZT_SPELL(0) ? WAND_CLASS : '\0', NOTELL,
                          bcsign))
        tmp /= 2;
    if (half_spell_dam(mdef) && tmp && buzztyp < ZT_BREATH(0))
        tmp = (tmp + 1) / 2;
    if (tmp < 0)
        tmp = 0;        /* don't allow negative damage */
    if (raylevel == P_UNSKILLED) {
        /* unskilled rays has a 40% maxHP cap on damage */
        int hpmax = m_mhpmax(mdef);
        if (you && Upolyd)
            hpmax = u.mhmax;
        tmp = min((40 * hpmax + 1) / 100, tmp);
    }
    if (you)
        losehp(tmp, killer_msg(DIED, an(fltxt)));
    else {
        mdef->mhp -= tmp;
        if (mdef->mhp <= 0)
            monkilled(magr, mdef, fltxt, AD_RBRE);
    }
    return;
}

/*
 * burn scrolls and spellbooks on floor at position x,y
 * return the number of scrolls and spellbooks burned
 */
int
burn_floor_paper(struct level *lev, int x, int y,
                 boolean give_feedback, /* caller needs to decide
                                           about visibility checks */
                 boolean u_caused)
{
    struct obj *obj, *obj2;
    const char *buf1 = "", *buf2 = "";  /* lint suppression */
    long i, scrquan, delquan;
    int cnt = 0;

    for (obj = lev->objects[x][y]; obj; obj = obj2) {
        obj2 = obj->nexthere;
        if (obj->oclass == SCROLL_CLASS || obj->oclass == SPBOOK_CLASS) {
            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL ||
                obj_resists(obj, 2, 100))
                continue;
            scrquan = obj->quan;        /* number present */
            delquan = 0;        /* number to destroy */
            for (i = scrquan; i > 0; i--)
                if (!rn2(3))
                    delquan++;
            if (delquan) {
                /* save name before potential delobj() */
                if (give_feedback) {
                    obj->quan = 1;

                    buf1 = (x == u.ux && y == u.uy) ?
                        xname(obj) : distant_name(obj, xname);
                    obj->quan = 2;
                    buf2 = (x == u.ux && y == u.uy) ?
                        xname(obj) : distant_name(obj, xname);
                    obj->quan = scrquan;
                }
                /* useupf(), which charges, only if hero caused damage */
                if (u_caused)
                    useupf(obj, delquan);
                else if (delquan < scrquan)
                    obj->quan -= delquan;
                else
                    delobj(obj);
                cnt += delquan;
                if (give_feedback) {
                    if (delquan > 1)
                        pline(msgc_itemloss, "%ld %s burn.", delquan, buf2);
                    else
                        pline(msgc_itemloss, "%s burns.", An(buf1));
                }
            }
        }
    }
    return cnt;
}

void
melt_ice(struct level *lev, xchar x, xchar y)
{
    struct rm *loc = &lev->locations[x][y];
    struct obj *otmp;
    boolean visible = (lev == level && cansee(x, y));

    if (loc->typ == DRAWBRIDGE_UP)
        loc->drawbridgemask &= ~DB_ICE; /* revert to DB_MOAT */
    else {      /* loc->typ == ICE */
        loc->typ = (loc->icedpool == ICED_POOL ? POOL : MOAT);
        loc->icedpool = 0;
    }
    obj_ice_effects(lev, x, y, FALSE);
    unearth_objs(lev, x, y);

    if (Underwater)
        vision_recalc(1);
    if (lev == level)
        newsym(x, y);
    if (visible)
        pline_once(msgc_levelsound, "The ice crackles and melts.");
    if ((otmp = sobj_at(BOULDER, lev, x, y)) != 0) {
        if (visible)
            pline(msgc_consequence, "%s settles...", An(xname(otmp)));
        do {
            obj_extract_self(otmp);     /* boulder isn't being pushed */
            if (!boulder_hits_pool(otmp, x, y, FALSE))
                impossible("melt_ice: no pool?");
            /* try again if there's another boulder and pool didn't fill */
        } while (is_pool(lev, x, y) &&
                 (otmp = sobj_at(BOULDER, lev, x, y)) != 0);
        if (lev == level)
            newsym(x, y);
    }
    if (lev == level && x == u.ux && y == u.uy)
        spoteffects(TRUE);      /* possibly drown, notice objects */
}

/* Burn floor scrolls, evaporate pools, etc. in a single square. Used both for
   normal bolts of fire, cold, etc. and for fireballs. Sets shopdamage to TRUE
   if a shop door is destroyed, and returns the amount by which range is reduced
   (the latter is just ignored by fireballs) */
int
zap_over_floor(xchar x, xchar y, int type, boolean * shopdamage)
{
    struct monst *mon;
    int abstype = abs(type) % 10;
    struct rm *loc = &level->locations[x][y];
    int rangemod = 0;

    if (abstype == ZT_FIRE) {
        struct trap *t = t_at(level, x, y);

        if (t && t->ttyp == WEB) {
            /* a burning web is too flimsy to notice if you can't see it */
            if (cansee(x, y))
                pline_once(msgc_consequence, "A web bursts into flames!");
            delfloortrap(level, t);
            if (cansee(x, y))
                newsym(x, y);
        }
        if (is_ice(level, x, y)) {
            melt_ice(level, x, y);
        } else if (is_pool(level, x, y)) {
            const char *msgtxt = "You hear hissing gas.";

            if (loc->typ != POOL) {     /* MOAT or DRAWBRIDGE_UP */
                if (cansee(x, y))
                    msgtxt = "Some water evaporates.";
            } else {
                struct trap *ttmp;

                rangemod -= 3;
                loc->typ = ROOM;
                ttmp = maketrap(level, x, y, PIT, rng_main);
                if (ttmp)
                    ttmp->tseen = 1;
                if (cansee(x, y))
                    msgtxt = "The water evaporates.";
            }
            pline_once(msgc_consequence, "%s", msgtxt);
            if (loc->typ == ROOM)
                newsym(x, y);
        } else if (IS_FOUNTAIN(loc->typ)) {
            if (cansee(x, y))
                pline(msgc_consequence, "Steam billows from the fountain.");
            rangemod -= 1;
            dryup(x, y, type > 0);
        }
    } else if (abstype == ZT_COLD &&
               (is_pool(level, x, y) || is_lava(level, x, y))) {
        boolean lava = is_lava(level, x, y);
        boolean moat = is_moat(level, x, y);

        if (loc->typ == WATER) {
            /* For now, don't let WATER freeze. */
            if (cansee(x, y))
                pline(msgc_consequence, "The water freezes for a moment.");
            else
                You_hear(msgc_levelsound, "a soft crackling.");
            rangemod -= 1000;   /* stop */
        } else {
            rangemod -= 3;
            if (loc->typ == DRAWBRIDGE_UP) {
                loc->drawbridgemask &= ~DB_UNDER;       /* clear lava */
                loc->drawbridgemask |= (lava ? DB_FLOOR : DB_ICE);
            } else {
                if (!lava)
                    loc->icedpool = (loc->typ == POOL ? ICED_POOL : ICED_MOAT);
                loc->typ = (lava ? ROOM : ICE);
            }
            bury_objs(level, x, y);
            if (cansee(x, y)) {
                if (moat)
                    pline_once(msgc_consequence,
                               "The moat is bridged with ice!");
                else if (lava)
                    pline_once(msgc_consequence,
                               "The lava cools and solidifies.");
                else
                    pline_once(msgc_consequence, "The water freezes.");
                newsym(x, y);
            } else if (!lava)
                You_hear(msgc_levelsound, "a crackling sound.");

            if (x == u.ux && y == u.uy) {
                if (u.uinwater) {       /* not just `if (Underwater)' */
                    /* leave the no longer existent water */
                    u.uinwater = 0;
                    u.uundetected = 0;
                    doredraw();
                    turnstate.vision_full_recalc = TRUE;
                } else if (u.utrap && u.utraptype == TT_LAVA) {
                    if (Passes_walls) {
                        pline(msgc_yafm,
                              "You pass through the now-solid rock.");
                    } else {
                        u.utrap = rn1(50, 20);
                        u.utraptype = TT_INFLOOR;
                        pline(msgc_statusbad,
                              "You are firmly stuck in the cooling rock.");
                    }
                }
            } else if ((mon = m_at(level, x, y)) != 0) {
                /* probably ought to do some hefty damage to any non-ice
                   creature caught in freezing water; at a minimum, eels are
                   forced out of hiding */
                if (swims(mon) && mon->mundetected) {
                    mon->mundetected = 0;
                    newsym(x, y);
                }
            }
        }
        obj_ice_effects(level, x, y, TRUE);
    }
    if (closed_door(level, x, y)) {
        int new_doormask = -1;
        const char *see_txt = NULL, *sense_txt = NULL, *hear_txt = NULL;

        rangemod = -1000;
        switch (abstype) {
        case ZT_FIRE:
            new_doormask = D_NODOOR;
            see_txt = "The door is consumed in flames!";
            sense_txt = "You smell smoke.";
            break;
        case ZT_COLD:
            new_doormask = D_NODOOR;
            see_txt = "The door freezes and shatters!";
            sense_txt = "You feel cold.";
            break;
        case ZT_DEATH:
            /* death spells/wands don't disintegrate */
            if (abs(type) != ZT_BREATH(ZT_DEATH))
                goto def_case;
            new_doormask = D_NODOOR;
            see_txt = "The door disintegrates!";
            hear_txt = "crashing wood.";
            break;
        case ZT_LIGHTNING:
            new_doormask = D_BROKEN;
            see_txt = "The door splinters!";
            hear_txt = "crackling.";
            break;
        default:
        def_case:
            if (cansee(x, y)) {
                pline(msgc_consequence, "The door absorbs %s %s!",
                      (type < 0) ? "the" : "your",
                      abs(type) < ZT_SPELL(0) ? "bolt" : abs(type) <
                      ZT_BREATH(0) ? "spell" : "blast");
            } else
                pline(msgc_levelsound, "You feel vibrations.");
            break;
        }
        if (new_doormask >= 0) {        /* door gets broken */
            if (*in_rooms(level, x, y, SHOPBASE)) {
                if (type >= 0) {
                    add_damage(x, y, 400L);
                    *shopdamage = TRUE;
                } else  /* caused by monster */
                    add_damage(x, y, 0L);
            }
            loc->doormask = new_doormask;
            unblock_point(x, y);        /* vision */
            if (cansee(x, y)) {
                pline(msgc_consequence, "%s", see_txt);
                newsym(x, y);
            } else if (sense_txt) {
                pline(msgc_levelsound, "%s", sense_txt);
            } else if (hear_txt) {
                You_hear(msgc_levelsound, "%s", hear_txt);
            }
        }
    }

    if (OBJ_AT(x, y) && abstype == ZT_FIRE)
        if (burn_floor_paper(level, x, y, FALSE, type > 0) && couldsee(x, y)) {
            newsym(x, y);
            pline(msgc_itemloss, "You %s of smoke.",
                  !Blind ? "see a puff" : "smell a whiff");
        }
    if ((mon = m_at(level, x, y)) != 0) {
        /* Cannot use wakeup() which also angers the monster */
        mon->msleeping = 0;
        if (mon->m_ap_type)
            seemimic(mon);
        if (type >= 0) {
            setmangry(mon);
            if (ispriest(mon) && *in_rooms(level, mon->mx, mon->my, TEMPLE))
                ghod_hitsu(mon);
            if (mx_eshk(mon) && !*u.ushops)
                hot_pursuit(mon);
        }
    }
    return rangemod;
}

/* fractured by pick-axe or wand of striking */
void
fracture_rock(struct obj *obj)
{
    /* A little Sokoban guilt... */
    if (obj->otyp == BOULDER && In_sokoban(&u.uz) && !flags.mon_moving)
        change_luck(-1);

    obj->otyp = ROCK;
    obj->quan = (long)rn1(60, 7);
    obj->owt = weight(obj);
    obj->oclass = GEM_CLASS;
    obj->known = FALSE;
    ox_free(obj); /* kill potential extra object data */
    if (obj->where == OBJ_FLOOR) {
        struct level *olev = obj->olev;
        obj_extract_self(obj);  /* move rocks back on top */
        place_object(obj, olev, obj->ox, obj->oy);
        if (olev == level) {
            if (!does_block(olev, obj->ox, obj->oy))
                unblock_point(obj->ox, obj->oy);
            if (cansee(obj->ox, obj->oy))
                newsym(obj->ox, obj->oy);
        }
    }
}

/* handle statue hit by striking/force bolt/pick-axe */
boolean
break_statue(struct obj *obj)
{
    /* [obj is assumed to be on floor, so no get_obj_location() needed] */
    struct trap *trap = t_at(level, obj->ox, obj->oy);
    struct obj *item;

    if (trap && trap->ttyp == STATUE_TRAP &&
        activate_statue_trap(trap, obj->ox, obj->oy, TRUE))
        return FALSE;
    /* drop any objects contained inside the statue */
    while ((item = obj->cobj) != 0) {
        obj_extract_self(item);
        place_object(item, level, obj->ox, obj->oy);
    }
    if (Role_if(PM_ARCHEOLOGIST) && !flags.mon_moving &&
        (obj->spe & STATUE_HISTORIC)) {
        if (cansee(obj->ox, obj->oy))
            pline(msgc_alignbad,
                  "You feel guilty about damaging such a historic statue.");
        adjalign(-1);
    }
    obj->spe = 0;
    fracture_rock(obj);
    return TRUE;
}

struct destroy_message destroy_messages[num_destroy_msgs] = {
    {"freezes and shatters", "freeze and shatter", "shattered potion"},
    {"boils and explodes", "boil and explode", "boiling potion"},
    {"catches fire and burns", "catch fire and burn", "burning scroll"},
    {"catches fire and burns", "catch fire and burn", "burning book"},
    {"breaks apart and explodes", "break apart and explode", "exploding wand"},
};

static void
destroy_item(int osym, int dmgtyp)
{
    struct obj *obj, *obj2;
    int dmg, xresist, skip;
    long i, cnt, quan;
    enum destroy_msg_type dindx;
    const char *mult;

    for (obj = invent; obj; obj = obj2) {
        obj2 = obj->nobj;
        if (obj->oclass != osym)
            continue;   /* test only objs of type osym */
        if (obj->oartifact)
            continue;   /* don't destroy artifacts */
        if (obj->in_use && obj->quan == 1)
            continue;   /* not available */
        xresist = skip = 0;
        dmg = dindx = 0;
        quan = 0L;

        switch (dmgtyp) {
        case AD_COLD:
            if (osym == POTION_CLASS && obj->otyp != POT_OIL) {
                quan = obj->quan;
                dindx = destroy_msg_potion_cold;
                dmg = rnd(4);
            } else
                skip++;
            break;
        case AD_FIRE:
            xresist = (Fire_resistance && obj->oclass != POTION_CLASS);

            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                skip++;
            if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                skip++;
                if (!Blind)
                    pline(msgc_noconsequence,
                          "%s glows a strange %s, but remains intact.",
                          The(xname(obj)), hcolor("dark red"));
            }
            quan = obj->quan;
            switch (osym) {
            case POTION_CLASS:
                dindx = destroy_msg_potion_fire;
                dmg = rnd(6);
                break;
            case SCROLL_CLASS:
                dindx = destroy_msg_scroll_fire;
                dmg = 1;
                break;
            case SPBOOK_CLASS:
                dindx = destroy_msg_spellbook_fire;
                dmg = 1;
                break;
            default:
                skip++;
                break;
            }
            break;
        case AD_ELEC:
            xresist = (Shock_resistance && obj->oclass != RING_CLASS);
            quan = obj->quan;
            switch (osym) {
            case WAND_CLASS:
                if (obj->otyp == WAN_LIGHTNING) {
                    skip++;
                    break;
                }
                dindx = destroy_msg_wand_elec;
                dmg = rnd(10);
                break;
            default:
                skip++;
                break;
            }
            break;
        default:
            skip++;
            break;
        }
        if (!skip) {
            if (obj->in_use)
                --quan; /* one will be used up elsewhere */
            for (i = cnt = 0L; i < quan; i++)
                if (!rn2(3))
                    cnt++;

            if (!cnt)
                continue;
            if (cnt == quan)
                mult = "Your";
            else
                mult = (cnt == 1L) ? "One of your" : "Some of your";
            pline(msgc_itemloss, "%s %s %s!", mult, xname(obj),
                  (cnt > 1L) ? destroy_messages[dindx].singular
                  : destroy_messages[dindx].plural);
            if (osym == POTION_CLASS && dmgtyp != AD_COLD) {
                if (!breathless(youmonst.data) || haseyes(youmonst.data))
                    potionbreathe(&youmonst, obj);
            }
            setunequip(obj);

            /* If destroying the item that (perhaps indirectly) caused the
               destruction, we need to notify the caller, but obfree() does that
               (via modifying turnstate), and obfree() is a better place (it
               handles more possible reasons that the item might be
               destroyed). */
            for (i = 0; i < cnt; i++)
                useup(obj);
            if (dmg) {
                if (xresist)
                    pline(msgc_noconsequence, "You aren't hurt!");
                else {
                    const char *how = destroy_messages[dindx].killer;
                    boolean one = (cnt == 1L);

                    losehp(dmg, killer_msg(DIED, one ? an(how) :
                                           makeplural(how)));
                    exercise(A_STR, FALSE);
                }
            }
        }
    }
    return;
}

int
destroy_mitem(struct monst *mtmp, int osym, int dmgtyp)
{
    /* Extrinsic properties protect against item destruction */
    if ((dmgtyp == AD_FIRE && ehas_property(mtmp, FIRE_RES)) ||
        (dmgtyp == AD_COLD && ehas_property(mtmp, COLD_RES)) ||
        (dmgtyp == AD_ELEC && ehas_property(mtmp, SHOCK_RES)))
        return 0;

    struct obj *obj, *obj2;
    int skip, tmp = 0;
    long i, cnt, quan;
    enum destroy_msg_type dindx;
    boolean vis;
    const char *mult;

    if (mtmp == &youmonst) {    /* this simplifies artifact_hit() */
        destroy_item(osym, dmgtyp);
        return 0;       /* arbitrary; value doesn't matter to artifact_hit() */
    }

    vis = canseemon(mtmp);
    for (obj = mtmp->minvent; obj; obj = obj2) {
        obj2 = obj->nobj;
        if (obj->oclass != osym)
            continue;   /* test only objs of type osym */
        skip = 0;
        quan = 0L;
        dindx = 0;

        switch (dmgtyp) {
        case AD_COLD:
            if (osym == POTION_CLASS && obj->otyp != POT_OIL) {
                quan = obj->quan;
                dindx = destroy_msg_potion_cold;
                tmp++;
            } else
                skip++;
            break;
        case AD_FIRE:
            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                skip++;
            if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                skip++;
                if (vis)
                    pline(msgc_noconsequence,
                          "%s glows a strange %s, but remains intact.",
                          The(distant_name(obj, xname)), hcolor("dark red"));
            }
            quan = obj->quan;
            switch (osym) {
            case POTION_CLASS:
                dindx = destroy_msg_potion_fire;
                tmp++;
                break;
            case SCROLL_CLASS:
                dindx = destroy_msg_scroll_fire;
                tmp++;
                break;
            case SPBOOK_CLASS:
                dindx = destroy_msg_spellbook_fire;
                tmp++;
                break;
            default:
                skip++;
                break;
            }
            break;
        case AD_ELEC:
            quan = obj->quan;
            switch (osym) {
            case WAND_CLASS:
                if (obj->otyp == WAN_LIGHTNING) {
                    skip++;
                    break;
                }
                dindx = destroy_msg_wand_elec;
                tmp++;
                break;
            default:
                skip++;
                break;
            }
            break;
        default:
            skip++;
            break;
        }
        if (!skip) {
            for (i = cnt = 0L; i < quan; i++)
                if (!rn2(3))
                    cnt++;

            if (!cnt)
                continue;
            if (cnt == quan)
                mult = "";
            else
                mult = (cnt == 1L) ? "One of " : "Some of ";
            if (vis)
                pline(msgc_itemloss, "%s%s %s %s!", mult,
                      s_suffix(cnt == quan ? Monnam(mtmp) : mon_nam(mtmp)),
                      xname(obj),
                      (cnt > 1L) ? destroy_messages[dindx].singular
                      : destroy_messages[dindx].plural);
            for (i = 0; i < cnt; i++)
                m_useup(mtmp, obj);
        }
    }
    return tmp;
}


int
resist(const struct monst *magr, const struct monst *mdef,
       char oclass, int domsg, int buc)
{
    int resisted;
    int alev, dlev;

    /* attack level */
    switch (oclass) {
    case WAND_CLASS:
        alev = 12;
        if (magr) {
            int wandlevel = P_SKILL(P_WANDS);
            if (magr != &youmonst)
                wandlevel = mprof(magr, MP_WANDS);
            wandlevel += buc;
            alev = (wandlevel == P_UNSKILLED ? 5 :
                    wandlevel == P_BASIC ? 10 :
                    wandlevel == P_SKILLED ? 20 :
                    wandlevel == P_EXPERT ? 30 :
                    wandlevel == P_MASTER ? 50 :
                    1);
        }
        break;
    case TOOL_CLASS:
        alev = 10;
        break;  /* instrument */
    case WEAPON_CLASS:
        alev = 10;
        break;  /* artifact */
    case SCROLL_CLASS:
        alev = 9;
        break;
    case POTION_CLASS:
        alev = 6;
        break;
    case RING_CLASS:
        alev = 5;
        break;
    default:
        alev = magr ? m_mlev(magr) : 10;
        break;  /* spell */
    }
    /* defense level */
    dlev = (int)mdef->m_lev;
    if (dlev > 50)
        dlev = 50;
    else if (dlev < 1)
        dlev = is_mplayer(mdef->data) ? u.ulevel : 1;

    resisted = rn2(100 + alev - dlev) < mdef->data->mr;
    if (resisted) {
        if (domsg) {
            shieldeff(mdef->mx, mdef->my);
            pline(msgc_noconsequence, "%s resists!", Monnam(mdef));
        }
    }

    return resisted;
}

/* Player wishes. Monster wishing is in muse.c */
void
makewish(void)
{
    const char *origbuf;
    struct obj *otmp, nothing;
    int tries = 0;

    nothing = zeroobj;  /* lint suppression; only its address matters */
    pline(msgc_uiprompt, "You may wish for an object.");
retry:
    /* never repeat the part where you specify what to wish for */
    origbuf = getlin("For what do you wish?", FALSE);

    if (origbuf[0] == '\033')
        origbuf = "";

    /* readobjnam isn't variable-length-string aware yet; avoid an overflow by
       giving it BUFSZ chars plus enough to hold the original wish */
    char buf[strlen(origbuf) + BUFSZ];
    strcpy(buf, origbuf);

    /*
     *  Note: if they wished for and got a non-object successfully,
     *  otmp == &zeroobj.  That includes gold, or an artifact that
     *  has been denied.  Wishing for "nothing" requires a separate
     *  value to remain distinct.
     */
    otmp = readobjnam(buf, &nothing, TRUE);
    if (!otmp) {
        pline(msgc_cancelled,
              "Nothing fitting that description exists in the game.");
        if (++tries < 5)
            goto retry;
        pline(msgc_itemloss, "That's enough tries!");
        otmp = readobjnam(NULL, NULL, TRUE);
        livelog_flubbed_wish(origbuf, otmp);
        if (!otmp)
            return;     /* for safety; should never happen */
    } else if (otmp == &nothing) {
        historic_event(FALSE, TRUE, "refused a wish");
        /* explicitly wished for "nothing", presumeably attempting to retain
           wishless conduct */
        return;
    } else {
        /* Don't have historic_event livelog the wishes for us, because we want
           to special-case them to use different fields. */
        historic_event(FALSE, FALSE, "wished for \"%s\".", origbuf);
        livelog_wish(origbuf);
    }

    /* KMH, conduct */
    break_conduct(conduct_wish);

    if (otmp != &zeroobj) {
        examine_object(otmp);
        /* The(aobjnam()) is safe since otmp is unidentified -dlc */
        hold_another_object(otmp,
                            Engulfed ? "Oops!  %s out of your reach!"
                            : (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
                               level->locations[u.ux][u.uy].typ < IRONBARS ||
                               level->locations[u.ux][u.uy].typ >=
                               ICE) ? "Oops!  %s away from you!" :
                            "Oops!  %s to the floor!",
                            The(aobjnam
                                (otmp, Is_airlevel(&u.uz) ||
                                 u.uinwater ? "slip" : "drop")), NULL);
        u.ublesscnt += rn1(100, 50);    /* the gods take notice */
    }
}


int
getwandlevel(const struct monst *user, struct obj *obj) {
    int wandlevel;
    if (user == &youmonst) {
        wandlevel = P_SKILL(P_WANDS);
        if (!wandlevel) /* restricted users would return a 0 */
            wandlevel++;
    } else
        wandlevel = mprof(user, MP_WANDS);

    /* wandlevel is 0-5 depending on skill + BUC combination */
    if (!obj || obj->oclass != WAND_CLASS) {
        impossible("getwandlevel() with no wand obj?");
        return 0;
    }
    if (obj->cursed)
        wandlevel--;
    else if (obj->blessed)
        wandlevel++;
    return wandlevel;
}

/*zap.c*/
