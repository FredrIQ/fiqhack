/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"    /* ALLOW_M */

static int drop_throw(struct obj *, boolean, int, int);
static boolean qlined_up(struct monst *mtmp, int ax, int ay, boolean breath);

#define URETREATING(x,y) (distmin(u.ux,u.uy,x,y) > distmin(u.ux0,u.uy0,x,y))

#define POLE_LIM 5      /* How far monsters can use pole-weapons */

/*
 * Keep consistent with breath weapons in zap.c, and AD_* in monattk.h.
 */
static const char *const breathwep[] = {
    "fragments",
    "fire",
    "frost",
    "sleep gas",
    "a disintegration blast",
    "lightning",
    "poison gas",
    "acid",
    "strange breath #8",
    "strange breath #9"
};

static int
ai_use_at_range(int n)
{
    if (n <= 0)
        return 1;
    return !rn2(n);
}

/* hero is hit by something other than a monster */
int
thitu(int tlev, int dam, struct obj *obj, const char *name)
{       /* if null, then format `obj' */
    const char *onm, *killer;
    boolean is_acid;

    /* TODO: credit the monster that fired the object with the kill */
    if (!name) {
        if (!obj)
            panic("thitu: name & obj both null?");
        name = (obj->quan > 1L) ? doname(obj) : mshot_xname(obj);
        killer = killer_msg_obj(DIED, obj);
    } else {
        killer = killer_msg(DIED, an(name));
    }
    onm = (obj && obj_is_pname(obj)) ? the(name) :
      (obj && obj->quan > 1L) ? name : an(name);
    is_acid = (obj && obj->otyp == ACID_VENOM);

    if (get_player_ac() + tlev <= rnd(20)) {
        if (Blind || !flags.verbose)
            pline("It misses.");
        else
            pline("You are almost hit by %s.", onm);
        return 0;
    } else {
        if (Blind || !flags.verbose)
            pline("You are hit!");
        else
            pline("You are hit by %s%s", onm, exclam(dam));

        if (obj && objects[obj->otyp].oc_material == SILVER &&
            hates_silver(youmonst.data)) {
            dam += rnd(20);
            pline("The silver sears your flesh!");
            exercise(A_CON, FALSE);
        }
        if (is_acid && Acid_resistance)
            pline("It doesn't seem to hurt you.");
        else {
            if (is_acid)
                pline("It burns!");
            if (Half_physical_damage)
                dam = (dam + 1) / 2;
            losehp(dam, killer);
            exercise(A_STR, FALSE);
        }
        return 1;
    }
}

/* Be sure this corresponds with what happens to player-thrown objects in
 * dothrow.c (for consistency). --KAA
 * Returns 0 if object still exists (not destroyed).
 */
static int
drop_throw(struct obj *obj, boolean ohit, int x, int y)
{
    int retvalu = 1;
    int create;
    struct monst *mtmp;
    struct trap *t;

    if (breaks(obj, x, y))
        return 1;

    if (ohit && (is_multigen(obj) || obj->otyp == ROCK))
        create = !rn2(3);
    else
        create = 1;

    if (create &&
        !((mtmp = m_at(level, x, y)) && (mtmp->mtrapped) &&
          (t = t_at(level, x, y)) && ((t->ttyp == PIT) ||
                                      (t->ttyp == SPIKED_PIT)))) {
        int objgone = 0;

        if (down_gate(x, y) != -1)
            objgone = ship_object(obj, x, y, FALSE);
        if (!objgone) {
            if (!flooreffects(obj, x, y, "fall")) {
                /* don't double-dip on damage */
                place_object(obj, level, x, y);
                if (!mtmp && x == u.ux && y == u.uy)
                    mtmp = &youmonst;
                if (mtmp && ohit)
                    passive_obj(mtmp, obj, NULL);
                stackobj(obj);
                retvalu = 0;
            }
        }
    } else
        obfree(obj, NULL);
    return retvalu;
}


/* an object launched by someone/thing other than player attacks a monster;
   return 1 if the object has stopped moving (hit or its range used up) */
int
ohitmon(struct monst *mtmp, /* accidental target */
        struct obj *otmp,   /* missile; might be destroyed by drop_throw */
        int range,  /* how much farther will object travel if it misses */
        /* Use -1 to signify to keep going even after hit, unless it's gone
           (used for rolling_boulder_traps) */
        boolean verbose) {  /* give message(s) even when you can't see what
                               happened */
    int damage, tmp;
    boolean vis, ismimic;
    int objgone = 1;

    ismimic = mtmp->m_ap_type && mtmp->m_ap_type != M_AP_MONSTER;
    vis = cansee(bhitpos.x, bhitpos.y);

    tmp = 5 + find_mac(mtmp) + omon_adj(mtmp, otmp, FALSE);
    if (tmp < rnd(20)) {
        if (!ismimic) {
            if (vis)
                miss(distant_name(otmp, mshot_xname), mtmp);
            else if (verbose)
                pline("It is missed.");
        }
        if (!range) {   /* Last position; object drops */
            if (is_pole(otmp))
                return 1;

            drop_throw(otmp, 0, mtmp->mx, mtmp->my);
            return 1;
        }
    } else if (otmp->oclass == POTION_CLASS) {
        if (ismimic)
            seemimic(mtmp);
        mtmp->msleeping = 0;
        if (vis)
            otmp->dknown = 1;
        potionhit(mtmp, otmp, FALSE);
        return 1;
    } else {
        damage = dmgval(otmp, mtmp);

        if (otmp->otyp == ACID_VENOM && resists_acid(mtmp))
            damage = 0;
        if (ismimic)
            seemimic(mtmp);
        mtmp->msleeping = 0;
        if (vis)
            hit(distant_name(otmp, mshot_xname), mtmp, exclam(damage));
        else if (verbose)
            pline("%s is hit%s", Monnam(mtmp), exclam(damage));

        if (otmp->opoisoned && is_poisonable(otmp)) {
            if (resists_poison(mtmp)) {
                if (vis)
                    pline("The poison doesn't seem to affect %s.",
                          mon_nam(mtmp));
            } else {
                if (rn2(30)) {
                    damage += rnd(6);
                } else {
                    if (vis)
                        pline("The poison was deadly...");
                    damage = mtmp->mhp;
                }
            }
        }
        if (objects[otmp->otyp].oc_material == SILVER &&
            hates_silver(mtmp->data)) {
            if (vis)
                pline("The silver sears %s flesh!", s_suffix(mon_nam(mtmp)));
            else if (verbose)
                pline("Its flesh is seared!");
        }
        if (otmp->otyp == ACID_VENOM && cansee(mtmp->mx, mtmp->my)) {
            if (resists_acid(mtmp)) {
                if (vis || verbose)
                    pline("%s is unaffected.", Monnam(mtmp));
                damage = 0;
            } else {
                if (vis)
                    pline("The acid burns %s!", mon_nam(mtmp));
                else if (verbose)
                    pline("It is burned!");
            }
        }
        mtmp->mhp -= damage;
        if (mtmp->mhp < 1) {
            if (vis || verbose)
                pline("%s is %s!", Monnam(mtmp),
                      (nonliving(mtmp->data) || !canclassifymon(mtmp))
                      ? "destroyed" : "killed");
            /* don't blame hero for unknown rolling boulder trap */
            if (!flags.mon_moving &&
                (otmp->otyp != BOULDER || range >= 0 || otmp->otrapped))
                xkilled(mtmp, 0);
            else
                mondied(mtmp);
        }

        if (can_blnd
            (NULL, mtmp,
             (uchar) (otmp->otyp == BLINDING_VENOM ? AT_SPIT : AT_WEAP),
             otmp)) {
            if (vis && mtmp->mcansee)
                pline("%s is blinded by %s.", Monnam(mtmp), the(xname(otmp)));
            mtmp->mcansee = 0;
            tmp = (int)mtmp->mblinded + rnd(25) + 20;
            if (tmp > 127)
                tmp = 127;
            mtmp->mblinded = tmp;
        }

        if (is_pole(otmp))
            return 1;

        objgone = drop_throw(otmp, 1, bhitpos.x, bhitpos.y);
        if (!objgone && range == -1) {  /* special case */
            obj_extract_self(otmp);     /* free it for motion again */
            return 0;
        }
        return 1;
    }
    return 0;
}


void
m_throw(struct monst *mon, int x, int y, int dx, int dy, int range,
        struct obj *obj, boolean verbose)
{
    struct monst *mtmp;
    struct obj *singleobj;
    struct tmp_sym *tsym = 0;
    int hitu, blindinc = 0;

    bhitpos.x = x;
    bhitpos.y = y;

    if (obj->quan == 1L) {
        /*
         * Remove object from minvent.  This cannot be done later on;
         * what if the player dies before then, leaving the monster
         * with 0 daggers?  (This caused the infamous 2^32-1 orcish
         * dagger bug).
         *
         * VENOM is not in minvent - it should already be OBJ_FREE.
         * The extract below does nothing.
         */

        /* not possibly_unwield, which checks the object's */
        /* location, not its existence */
        if (MON_WEP(mon) == obj) {
            setmnotwielded(mon, obj);
            MON_NOWEP(mon);
        }
        obj_extract_self(obj);
        singleobj = obj;
        obj = NULL;
    } else {
        singleobj = splitobj(obj, 1L);
        obj_extract_self(singleobj);
    }

    singleobj->owornmask = 0;   /* threw one of multiple weapons in hand? */
    singleobj->olev = level;    /* object is on the same level as monster */

    if ((singleobj->cursed || singleobj->greased) && (dx || dy) && !rn2(7)) {
        if (canseemon(mon) && flags.verbose) {
            if (is_ammo(singleobj))
                pline("%s misfires!", Monnam(mon));
            else
                pline("%s as %s throws it!", Tobjnam(singleobj, "slip"),
                      mon_nam(mon));
        }
        dx = rn2(3) - 1;
        dy = rn2(3) - 1;
        /* check validity of new direction */
        if (!dx && !dy) {
            drop_throw(singleobj, 0, bhitpos.x, bhitpos.y);
            return;
        }
    }

    /* pre-check for doors, walls and boundaries. Also need to pre-check for
       bars regardless of direction; the random chance for small objects
       hitting bars is skipped when reaching them at point blank range */
    if (!isok(bhitpos.x + dx, bhitpos.y + dy)
        || IS_ROCK(level->locations[bhitpos.x + dx][bhitpos.y + dy].typ)
        || closed_door(level, bhitpos.x + dx, bhitpos.y + dy)
        || (level->locations[bhitpos.x + dx][bhitpos.y + dy].typ == IRONBARS &&
            hits_bars(&singleobj, bhitpos.x, bhitpos.y, 0, 0))) {
        drop_throw(singleobj, 0, bhitpos.x, bhitpos.y);
        return;
    }

    /* Note: drop_throw may destroy singleobj.  Since obj must be destroyed
       early to avoid the dagger bug, anyone who modifies this code should be
       careful not to use either one after it's been freed. */
    tsym = tmpsym_initobj(singleobj);

    while (range-- > 0) {      /* Actually the loop is always exited by break */
        bhitpos.x += dx;
        bhitpos.y += dy;
        if ((mtmp = m_at(level, bhitpos.x, bhitpos.y)) != 0) {
            if (ohitmon(mtmp, singleobj, range, verbose))
                break;
        } else if (bhitpos.x == u.ux && bhitpos.y == u.uy) {
            action_interrupted();

            if (singleobj->oclass == GEM_CLASS &&
                singleobj->otyp <= LAST_GEM + 9
                /* 9 glass colors */
                && is_unicorn(youmonst.data) && !u_helpless(hm_all)) {
                if (singleobj->otyp > LAST_GEM) {
                    pline("You catch the %s.", xname(singleobj));
                    pline("You are not interested in %s junk.",
                          s_suffix(mon_nam(mon)));
                    makeknown(singleobj->otyp);
                    dropy(singleobj);
                } else {
                    pline("You accept %s gift in the spirit in which it was "
                          "intended.", s_suffix(mon_nam(mon)));
                    hold_another_object(singleobj, "You catch, but drop, %s.",
                                        xname(singleobj), "You catch:");
                }
                break;
            }
            if (singleobj->oclass == POTION_CLASS) {
                if (!Blind)
                    singleobj->dknown = 1;
                potionhit(&youmonst, singleobj, FALSE);
                break;
            }
            switch (singleobj->otyp) {
                int dam, hitv;

            case EGG:
                if (!touch_petrifies(&mons[singleobj->corpsenm])) {
                    impossible("monster throwing egg type %d",
                               singleobj->corpsenm);
                    hitu = 0;
                    break;
                }
                /* fall through */
            case CREAM_PIE:
            case BLINDING_VENOM:
                hitu = thitu(8, 0, singleobj, NULL);
                break;
            default:
                dam = dmgval(singleobj, &youmonst);
                hitv = 3 - distmin(u.ux, u.uy, mon->mx, mon->my);
                if (hitv < -4)
                    hitv = -4;
                if (is_elf(mon->data) &&
                    objects[singleobj->otyp].oc_skill == P_BOW) {
                    hitv++;
                    if (MON_WEP(mon) && MON_WEP(mon)->otyp == ELVEN_BOW)
                        hitv++;
                    if (singleobj->otyp == ELVEN_ARROW)
                        dam++;
                }
                if (bigmonst(youmonst.data))
                    hitv++;
                hitv += 8 + singleobj->spe;
                if (dam < 1)
                    dam = 1;
                hitu = thitu(hitv, dam, singleobj, NULL);
            }
            if (hitu && singleobj->opoisoned && is_poisonable(singleobj)) {
                poisoned(xname(singleobj), A_STR,
                         killer_msg_obj(POISONING, singleobj), -10);
            }
            if (hitu &&
                can_blnd(NULL, &youmonst,
                         (uchar) (singleobj->otyp ==
                                  BLINDING_VENOM ? AT_SPIT : AT_WEAP),
                         singleobj)) {
                blindinc = rnd(25);
                if (singleobj->otyp == CREAM_PIE) {
                    if (!Blind)
                        pline("Yecch!  You've been creamed.");
                    else
                        pline("There's something sticky all over your %s.",
                              body_part(FACE));
                } else if (singleobj->otyp == BLINDING_VENOM) {
                    int num_eyes = eyecount(youmonst.data);

                    /* venom in the eyes */
                    if (!Blind)
                        pline("The venom blinds you.");
                    else
                        pline("Your %s sting%s.",
                              (num_eyes ==
                               1) ? body_part(EYE) : makeplural(body_part(EYE)),
                              (num_eyes == 1) ? "s" : "");
                }
            }
            if (hitu && singleobj->otyp == EGG) {
                if (touched_monster(singleobj->corpsenm))
                    Stoned = 5;
            }
            action_interrupted();
            if (hitu || !range) {
                drop_throw(singleobj, hitu, u.ux, u.uy);
                break;
            }
        } else if (!range       /* reached end of path */
                   /* missile hits edge of screen */
                   || !isok(bhitpos.x + dx, bhitpos.y + dy)
                   /* missile hits the wall */
                   || IS_ROCK(level->
                              locations[bhitpos.x + dx][bhitpos.y + dy].typ)
                   /* missile hit closed door */
                   || closed_door(level, bhitpos.x + dx, bhitpos.y + dy)
                   /* missile might hit iron bars */
                   || (level->locations[bhitpos.x + dx][bhitpos.y + dy].typ ==
                       IRONBARS &&
                       hits_bars(&singleobj, bhitpos.x, bhitpos.y, !rn2(5), 0))
                   /* Thrown objects "sink" */
                   || IS_SINK(level->locations[bhitpos.x][bhitpos.y].typ)) {
            if (singleobj)      /* hits_bars might have destroyed it */
                drop_throw(singleobj, 0, bhitpos.x, bhitpos.y);
            break;
        }
        tmpsym_at(tsym, bhitpos.x, bhitpos.y);
        win_delay_output();
    }
    tmpsym_at(tsym, bhitpos.x, bhitpos.y);
    win_delay_output();
    tmpsym_end(tsym);

    if (blindinc) {
        u.ucreamed += blindinc;
        make_blinded(Blinded + (long)blindinc, FALSE);
        if (!Blind)
            pline("Your vision quickly clears.");
        else if (flags.verbose)
            pline("Use the command #wipe to clean your %s.", body_part(FACE));
    }
}


/* Remove an item from the monster's inventory and destroy it. */
void
m_useup(struct monst *mon, struct obj *obj)
{
    if (obj->quan > 1L) {
        obj->quan--;
        obj->owt = weight(obj);
    } else {
        obj_extract_self(obj);
        possibly_unwield(mon, FALSE);
        if (obj->owornmask) {
            mon->misc_worn_check &= ~obj->owornmask;
            update_mon_intrinsics(mon, obj, FALSE, FALSE);
        }
        obfree(obj, NULL);
    }
}

/* monster attempts ranged weapon attack against a square */
void
thrwmq(struct monst *mtmp, int xdef, int ydef)
{
    struct obj *otmp, *mwep;
    schar skill;
    int multishot;
    const char *onm;

    /* Rearranged beginning so monsters can use polearms not in a line */
    if (mtmp->weapon_check == NEED_WEAPON || !MON_WEP(mtmp)) {
        mtmp->weapon_check = NEED_RANGED_WEAPON;
        /* mon_wield_item resets weapon_check as appropriate */
        if (mon_wield_item(mtmp) != 0)
            return;
    }

    /* Pick a weapon */
    otmp = select_rwep(mtmp);
    if (!otmp)
        return;

    if (is_pole(otmp)) {
        int dam, hitv;

        /* TODO: LOE function between two arbitrary points. */
        if (dist2(mtmp->mx, mtmp->my, xdef, ydef) > POLE_LIM ||
            (xdef == u.ux && ydef == u.uy && !couldsee(mtmp->mx, mtmp->my)))
            return;     /* Out of range, or intervening wall */

        if (mon_visible(mtmp)) {
            onm = singular(otmp, xname);
            pline("%s thrusts %s.", Monnam(mtmp),
                  obj_is_pname(otmp) ? the(onm) : an(onm));
        }

        if (xdef == u.ux && ydef == u.uy) {

            dam = dmgval(otmp, &youmonst);
            hitv = 3 - distmin(u.ux, u.uy, mtmp->mx, mtmp->my);
            if (hitv < -4)
                hitv = -4;
            if (bigmonst(youmonst.data))
                hitv++;
            hitv += 8 + otmp->spe;
            if (dam < 1)
                dam = 1;

            thitu(hitv, dam, otmp, NULL);
            action_interrupted();

        } else if (MON_AT(level, xdef, ydef))
            (void)ohitmon(m_at(level, xdef, ydef), otmp, 0, FALSE);
        else if (mon_visible(mtmp))
            pline("But it misses wildly.");

        return;
    }

    if (!qlined_up(mtmp, xdef, ydef, FALSE) ||
        !ai_use_at_range(BOLT_LIM - distmin(mtmp->mx, mtmp->my, xdef, ydef)))
        return;

    skill = objects[otmp->otyp].oc_skill;
    mwep = MON_WEP(mtmp);       /* wielded weapon */

    /* Multishot calculations */
    multishot = 1;
    if ((ammo_and_launcher(otmp, mwep) || skill == P_DAGGER || skill == -P_DART
         || skill == -P_SHURIKEN) && !mtmp->mconf) {
        /* Assumes lords are skilled, princes are expert */
        if (is_prince(mtmp->data))
            multishot += 2;
        else if (is_lord(mtmp->data))
            multishot++;

        switch (monsndx(mtmp->data)) {
        case PM_RANGER:
            multishot++;
            break;
        case PM_ROGUE:
            if (skill == P_DAGGER)
                multishot++;
            break;
        case PM_NINJA:
        case PM_SAMURAI:
            if (otmp->otyp == YA && mwep && mwep->otyp == YUMI)
                multishot++;
            break;
        default:
            break;
        }
        /* racial bonus */
        if ((is_elf(mtmp->data) && otmp->otyp == ELVEN_ARROW && mwep &&
             mwep->otyp == ELVEN_BOW) || (is_orc(mtmp->data) &&
                                          otmp->otyp == ORCISH_ARROW && mwep &&
                                          mwep->otyp == ORCISH_BOW))
            multishot++;

        if ((long)multishot > otmp->quan)
            multishot = (int)otmp->quan;
        if (multishot < 1)
            multishot = 1;
        else
            multishot = rnd(multishot);
    }

    if (mon_visible(mtmp)) {
        if (multishot > 1) {
            /* "N arrows"; multishot > 1 implies otmp->quan > 1, so xname()'s
               result will already be pluralized */
            onm = msgprintf("%d %s", multishot, xname(otmp));
        } else {
            /* "an arrow" */
            onm = singular(otmp, xname);
            onm = obj_is_pname(otmp) ? the(onm) : an(onm);
        }
        m_shot.s = ammo_and_launcher(otmp, mwep) ? TRUE : FALSE;
        pline("%s %s %s!", Monnam(mtmp), m_shot.s ? "shoots" : "throws", onm);
        m_shot.o = otmp->otyp;
    } else {
        m_shot.o = STRANGE_OBJECT;      /* don't give multishot feedback */
    }

    m_shot.n = multishot;
    for (m_shot.i = 1; m_shot.i <= m_shot.n; m_shot.i++)
        m_throw(mtmp, mtmp->mx, mtmp->my, sgn(tbx), sgn(tby),
                distmin(mtmp->mx, mtmp->my, xdef, ydef), otmp, TRUE);
    m_shot.n = m_shot.i = 0;
    m_shot.o = STRANGE_OBJECT;
    m_shot.s = FALSE;

    action_interrupted();
}

/* Is a monster willing to attack with a compass beam in a given direction?

   Returns TRUE and sets *mdef to a monster if there's a monster it wants to
   attack (and no monster it doesn't want to attack). *mdef can be &youmonst.

   If mdef is non-NULL, also set the tbx/tby globals for backwards compatibility
   (clobbered on a *mdef == NULL return, appropriately otherwise). TODO: get rid
   of these, they're really spaghetti.

   Returns TRUE and sets *mdef to NULL if there's no reason it wants to attack
   in that direction, but no reason it dislikes attacking in that direction
   either.

   Returns FALSE if the monster refuses to attack in that direction. In this
   case, *mdef will be a monster that we don't want to attack (possibly
   &youmonst). */
boolean
m_beam_ok(struct monst *magr, int dx, int dy, struct monst **mdef)
{
    int x = magr->mx;
    int y = magr->my;
    int i;
    struct monst *mat;

    if (mdef)
        *mdef = NULL;

    for (i = 0; i < BOLT_LIM; i++) {
        x += dx;
        y += dy;

        /* Will the beam stop at this point? As usual, assume that monsters have
           perfect knowledge of the level layout. */
        if (!isok(x, y) || !ZAP_POS(level->locations[x][y].typ) ||
            closed_door(level, x, y))
            break;

        /* Will the monster think that the beam will hit the player? Does it
           care? Hostiles like to attack the player; peacefuls don't want to.
           Pets have perfect knowledge of their master's location even if they
           can't sense their master. Confused monsters can't aim a beam at the
           player (or to avoid the player); no monster can hit an engulfed
           player with a beam. */
        if ((x == magr->mux && y == magr->muy && msensem(magr, &youmonst)) ||
            (magr->mtame && x == u.ux && y == u.uy)) {
            if (!Engulfed && !magr->mconf) {
                if (mdef) {
                    *mdef = &youmonst;
                    tbx = x - magr->mx;
                    tby = y - magr->my;
                }

                if (magr->mpeaceful && !Conflict)
                    return FALSE;
            }
        }

        mat = m_at(level, x, y);

        /* special case: make sure we don't hit the quest leader with stray
           beams, as it can make the game unwinnable; do this regardless of LOS
           or hostility or Conflict or confusion or anything like that */
        if (mat && mat->data->msound == MS_LEADER) {
            if (mdef)
                *mdef = mat;
            return FALSE;
        }

        /* Confused monsters aren't trying to target anything in particular,
           because they don't have full control of their actions. Monsters won't
           intentionally aim at or to avoid a monster they can't see (apart from
           the above MS_LEADER case). */
        if (mat && (msensem(magr, mat) & ~MSENSE_ITEMMIMIC) && !magr->mconf) {
            /* Note: the couldsee() here is an LOE check and has nothing to
               do with vision; it determines conflict radius */
            if (Conflict && !resist(magr, RING_CLASS, 0, 0) &&
                couldsee(magr->mx, magr->my) &&
                distu(magr->mx, magr->my) <= BOLT_LIM * BOLT_LIM) {
                /* we're conflicted, anything is a valid target */
                if (mdef) {
                    *mdef = mat;
                    tbx = x - magr->mx;
                    tby = y - magr->my;
                }
            } else if (mm_aggression(magr, mat) & ALLOW_M) {
                /* we want to attack this monster */
                if (mdef) {
                    *mdef = mat;
                    tbx = x - magr->mx;
                    tby = y - magr->my;
                }
            } else if (magr->mpeaceful || mat->mpeaceful) {
                /* we don't want to attack this monster; peacefuls (including
                   pets) should avoid collateral damage; also handles the
                   pet_attacks_up_to_difficulty checks; symmetrised so that
                   hostiles won't attack pets who won't attack them */
                if (mdef)
                    *mdef = mat;
                return FALSE;
            }
        }
    }

    return TRUE;
}

/* Find a target for a ranged attack. */
struct monst *
mfind_target(struct monst *mtmp)
{
    int dirx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 },
        diry[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

    int dir, origdir = -1;

    struct monst *mret;

    if (!mtmp->mpeaceful && lined_up(mtmp))
        return &youmonst;     /* kludge - attack the player first if possible */

    for (dir = rn2(8); dir != origdir; dir = ((dir + 1) % 8)) {
        if (origdir < 0)
            origdir = dir;

        if (m_beam_ok(mtmp, dirx[dir], diry[dir], &mret) && mret) {
            /* also check for a bounce */
            if (m_beam_ok(mtmp, -dirx[dir], -diry[dir], NULL))
                return mret;
        }
    }

    /* Nothing lined up? */
    tbx = tby = 0;
    return NULL;
}


int
spitmq(struct monst *mtmp, int xdef, int ydef, const struct attack *mattk)
{
    struct obj *otmp;

    if (mtmp->mcan) {
        if (canhear())
            pline("A dry rattle comes from %s throat.",
                  s_suffix(mon_nam(mtmp)));
        return 0;
    }
    boolean linedup = qlined_up(mtmp, xdef, ydef, FALSE);
    if (linedup && ai_use_at_range(
            BOLT_LIM - distmin(mtmp->mx, mtmp->my, xdef, ydef))) {
        switch (mattk->adtyp) {
        case AD_BLND:
        case AD_DRST:
            otmp = mktemp_sobj(level, BLINDING_VENOM);
            break;
        default:
            impossible("bad attack type in spitm");
            /* fall through */
        case AD_ACID:
            otmp = mktemp_sobj(level, ACID_VENOM);
            break;
        }

        if (mon_visible(mtmp)) {
            pline("%s spits venom!", Monnam(mtmp));
            action_interrupted();
        }
        m_throw(mtmp, mtmp->mx, mtmp->my, sgn(tbx), sgn(tby),
                distmin(mtmp->mx, mtmp->my, xdef, ydef), otmp,
                FALSE);
        return 1;
    }
    return 0;
}


int
breamq(struct monst *mtmp, int xdef, int ydef, const struct attack *mattk)
{
    /* if new breath types are added, change AD_ACID to max type */
    int typ = (mattk->adtyp == AD_RBRE) ? rnd(AD_ACID) : mattk->adtyp;

    boolean youdef = u.ux == xdef && u.uy == ydef;

    if (!youdef && distmin(mtmp->mx, mtmp->my, xdef, ydef) < 3)
        return 0;

    boolean linedup = qlined_up(mtmp, xdef, ydef, TRUE);

    if (linedup) {
        if (mtmp->mcan) {
            if (canhear()) {
                if (mon_visible(mtmp))
                    pline("%s coughs.", Monnam(mtmp));
                else
                    You_hear("a cough.");
            }
            return 0;
        }
        if (!mtmp->mspec_used && rn2(3)) {
            if ((typ >= AD_MAGM) && (typ <= AD_ACID)) {
                if (mon_visible(mtmp)) {
                    pline("%s breathes %s!", Monnam(mtmp), breathwep[typ - 1]);
                    action_interrupted();
                }
                buzz((int)(-20 - (typ - 1)), (int)mattk->damn, mtmp->mx,
                     mtmp->my, sgn(tbx), sgn(tby));
                /* breath runs out sometimes. Also, give monster some cunning;
                   don't breath if the target fell asleep. */
                if (!rn2(3))
                    mtmp->mspec_used = 10 + rn2(20);
                boolean sleeping = youdef ? u_helpless(hm_asleep) :
                    MON_AT(level, xdef, ydef) ?
                    m_at(level, xdef, ydef)->msleeping : FALSE;
                if (typ == AD_SLEE && sleeping)
                    mtmp->mspec_used += rnd(20);
            } else
                impossible("Breath weapon %d used", typ - 1);
        }
    }
    return 1;
}

boolean
linedup(xchar ax, xchar ay, xchar bx, xchar by)
{
    tbx = ax - bx;      /* These two values are set for use */
    tby = ay - by;      /* after successful return.  */

    /* sometimes displacement makes a monster think that you're at its own
       location; prevent it from throwing and zapping in that case */
    if (!tbx && !tby)
        return FALSE;

    if ((!tbx || !tby || abs(tbx) == abs(tby))  /* straight line or diagonal */
        && distmin(tbx, tby, 0, 0) < BOLT_LIM) {
        if (clear_path(ax, ay, bx, by, viz_array))
            return TRUE;
    }
    return FALSE;
}

/* TODO: Merge code with mfind_target */
static boolean
qlined_up(struct monst *mtmp, int ax, int ay, boolean breath)
{
    boolean lined_up = linedup(ax, ay, mtmp->mx, mtmp->my);

    int dx = sgn(ax - mtmp->mx), dy = sgn(ay - mtmp->my);

    if (!lined_up)
        return FALSE;

    /* Ensure that this is a reasonable direction to attack in. We check in
       front of the monster, and also behind in case of bounces. */
    if (!m_beam_ok(mtmp, dx, dy, NULL) ||
        (breath && !m_beam_ok(mtmp, -dx, -dy, NULL)))
        return FALSE;
    /* We should really check for right-angle bounces too, but that's pretty
       difficult given the code. (Monsters never intentionally bounce attacks
       off walls, incidentally.) Let's just hope it doesn't happen; it isn't
       massively bad if it does, except for frustrating the player slightly. */

    return TRUE;
}

/* is mtmp in position to use ranged attack?

   Note: this checks aware_of_u, not msensem; a monster is happy to aim a
   ranged attack at the guessed location of a player. */
boolean
lined_up(struct monst *mtmp)
{
    if (engulfing_u(mtmp))
        return FALSE; /* can't ranged-attack someone inside you */

    if (!aware_of_u(mtmp))
        return FALSE; /* monster doesn't know where you are */

    return linedup(mtmp->mux, mtmp->muy, mtmp->mx, mtmp->my);
}

/* Check if a monster is carrying a particular item. */
struct obj *
m_carrying(const struct monst *mtmp, int type)
{
    struct obj *otmp;

    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == type)
            return otmp;
    return NULL;
}

/* TRUE iff thrown/kicked/rolled object doesn't pass through iron bars */
boolean
hits_bars(struct obj ** obj_p, /* *obj_p will be set to NULL if object breaks */
          int x, int y, int always_hit, /* caller can force a hit for items
                                           which would fit through */
          int whodidit)
{       /* 1==hero, 0=other, -1==just check whether it'll pass thru */
    struct obj *otmp = *obj_p;
    int obj_type = otmp->otyp;
    boolean hits = always_hit;

    if (!hits)
        switch (otmp->oclass) {
        case WEAPON_CLASS:
            {
                int oskill = objects[obj_type].oc_skill;

                hits = (oskill != -P_BOW && oskill != -P_CROSSBOW &&
                        oskill != -P_DART && oskill != -P_SHURIKEN &&
                        oskill != P_SPEAR && oskill != P_JAVELIN &&
                        oskill != P_KNIFE); /* but not dagger */
                break;
            }
        case ARMOR_CLASS:
            hits = (objects[obj_type].oc_armcat != ARM_GLOVES);
            break;
        case TOOL_CLASS:
            hits = (obj_type != SKELETON_KEY && obj_type != LOCK_PICK &&
                    obj_type != CREDIT_CARD && obj_type != TALLOW_CANDLE &&
                    obj_type != WAX_CANDLE && obj_type != LENSES &&
                    obj_type != TIN_WHISTLE && obj_type != MAGIC_WHISTLE);
            break;
        case ROCK_CLASS:       /* includes boulder */
            if (obj_type != STATUE || mons[otmp->corpsenm].msize > MZ_TINY)
                hits = TRUE;
            break;
        case FOOD_CLASS:
            if (obj_type == CORPSE && mons[otmp->corpsenm].msize > MZ_TINY)
                hits = TRUE;
            else
                hits = (obj_type == MEAT_STICK ||
                        obj_type == HUGE_CHUNK_OF_MEAT);
            break;
        case SPBOOK_CLASS:
        case WAND_CLASS:
        case BALL_CLASS:
        case CHAIN_CLASS:
            hits = TRUE;
            break;
        default:
            break;
        }

    if (hits && whodidit != -1) {
        if (whodidit ? hero_breaks(otmp, x, y, FALSE) : breaks(otmp, x, y))
            *obj_p = otmp = 0;  /* object is now gone */
        /* breakage makes its own noises */
        else if (obj_type == BOULDER || obj_type == HEAVY_IRON_BALL)
            pline("Whang!");
        else if (otmp->oclass == COIN_CLASS ||
                 objects[obj_type].oc_material == GOLD ||
                 objects[obj_type].oc_material == SILVER)
            pline("Clink!");
        else
            pline("Clonk!");
    }

    return hits;
}

/*mthrowu.c*/
