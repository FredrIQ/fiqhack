/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-19 */
/* Copyright (C) 1990 by Ken Arromdee                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Note: Arrays are column first, while the screen is row first */
static const int explosion[3][3] = {
    {E_explode1, E_explode4, E_explode7},
    {E_explode2, E_explode5, E_explode8},
    {E_explode3, E_explode6, E_explode9}
};

/* "Chained" explosions, used by skilled wand users
   FIXME: make skilled casting of fireball/cone of cold use this */
void
chain_explode(int x, int y, int type,
              int dam, char olet, int expltype,
              const char *descr, int raylevel, int amount)
{
    int ex = x;
    int ey = y;
    while (amount--) {
        ex = x + rnd(3) - 2;
        ey = y + rnd(3) - 2;
        if (!isok(ex, ey) || IS_STWALL(level->locations[ex][ey].typ)) {
            ex = x;
            ey = y;
        }
        explode(ex, ey, type, dam, olet, expltype, descr, raylevel);
    }
}

/* Note: I had to choose one of three possible kinds of "type" when writing
   this function: a wand type (like in zap.c), an adtyp, or an object type.
   Wand types get complex because they must be converted to adtyps for
   determining such things as fire resistance.  Adtyps get complex in that
   they don't supply enough information--was it a player or a monster that
   did it, and with a wand, spell, or breath weapon?  Object types share both
   these disadvantages....

   The descr argument should be used to describe the explosion. It should be
   a string suitable for use with an().
   raylevel is used for explosions caused by skilled wand usage (0=no wand) */
void
explode(int x, int y, int type, /* the same as in zap.c */
        int dam, char olet, int expltype, const char *descr, int raylevel)
{
    int i, j, k, damu = dam;
    boolean visible, any_shield, resist_death;
    resist_death = FALSE;
    int uhurt = 0;      /* 0=unhurt, 1=items damaged, 2=you and items damaged */
    const char *str;
    const char *dispbuf = "";   /* lint suppression; I think the code's OK */
    const char *killer;
    boolean expl_needs_the = TRUE;
    int idamres, idamnonres;
    struct monst *mtmp;
    uchar adtyp;
    int explmask[3][3];

    int whattype = abs(type) % 10;

    /* 0=normal explosion, 1=do shieldeff, 2=do nothing */
    boolean shopdamage = FALSE;

    if (olet == MON_EXPLODE) {
        str = descr;
        adtyp = AD_PHYS;
        if (Hallucination) {
            int name = rndmonidx();

            dispbuf = msgcat(s_suffix(monnam_for_index(name)), " explosion");
            expl_needs_the = !monnam_is_pname(name);
        } else {
            dispbuf = str;
        }
    } else {
        adtyp = whattype + 1;
        boolean done = FALSE, hallu = Hallucination;

        if (hallu) {
            do {
                whattype = rn2(8);
            } while (whattype == 3);
        }
    tryagain:
        switch (whattype) {
        case 0:
            str = "magical blast";
            break;
        case 1:
            str =
                olet == BURNING_OIL ? "burning oil" : olet ==
                SCROLL_CLASS ? "tower of flame" : "fireball";
            break;
        case 2:
            str = "ball of cold";
            break;
        case 3:
            str = "sleeping gas";
            break;
        case 4:
            str = (olet == WAND_CLASS) ? "death field" : "disintegration field";
            break;
        case 5:
            str = "ball of lightning";
            break;
        case 6:
            str = "poison gas cloud";
            break;
        case 7:
            str = "splash of acid";
            break;
        default:
            impossible("explosion base type %d?", type);
            return;
        }
        if (!done) {
            dispbuf = str;
            done = TRUE;
            if (hallu) {
                whattype = adtyp - 1;
                goto tryagain;
            }
        }
    }

    any_shield = visible = FALSE;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            if (!isok(i + x - 1, j + y - 1)) {
                explmask[i][j] = 2;
                continue;
            } else
                explmask[i][j] = 0;

            if (i + x - 1 == u.ux && j + y - 1 == u.uy) {
                switch (adtyp) {
                case AD_PHYS:
                    explmask[i][j] = 0;
                    break;
                case AD_MAGM:
                    explmask[i][j] = !!(raylevel < P_EXPERT &&
                                        resists_magm(&youmonst));
                    break;
                case AD_FIRE:
                    explmask[i][j] = !!immune_to_fire(&youmonst);
                    break;
                case AD_COLD:
                    explmask[i][j] = !!immune_to_cold(&youmonst);
                    break;
                case AD_SLEE:
                    explmask[i][j] = !!immune_to_sleep(&youmonst);
                    break;
                case AD_DISN:
                    if (raylevel == P_UNSKILLED && Drain_resistance)
                        resist_death = TRUE;
                    /* why MR doesn't resist general deathfields is beyond me, but... */
                    if (resists_death(&youmonst))
                        resist_death = TRUE;
                    if (raylevel && Antimagic)
                        resist_death = TRUE;
                    if (raylevel >= P_EXPERT && !Drain_resistance)
                        resist_death = FALSE;
                    explmask[i][j] =
                        (olet == WAND_CLASS) ? !!resist_death :
                        !!Disint_resistance;
                    break;
                case AD_ELEC:
                    explmask[i][j] = !!immune_to_elec(&youmonst);
                    break;
                case AD_DRST:
                    explmask[i][j] = !!immune_to_poison(&youmonst);
                    break;
                case AD_ACID:
                    explmask[i][j] = !!immune_to_acid(&youmonst);
                    break;
                default:
                    impossible("explosion type %d?", adtyp);
                    break;
                }
            }
            /* can be both you and mtmp if you're swallowed */
            mtmp = m_at(level, i + x - 1, j + y - 1);
            if (!mtmp && i + x - 1 == u.ux && j + y - 1 == u.uy)
                mtmp = u.usteed;
            if (mtmp) {
                if (DEADMONSTER(mtmp))
                    explmask[i][j] = 2;
                else
                    switch (adtyp) {
                    case AD_PHYS:
                        break;
                    case AD_MAGM:
                        explmask[i][j] |= (raylevel < P_EXPERT &&
                                           resists_magm(mtmp));
                        break;
                    case AD_FIRE:
                        explmask[i][j] |= !!immune_to_fire(mtmp);
                        break;
                    case AD_COLD:
                        explmask[i][j] |= !!immune_to_cold(mtmp);
                        break;
                    case AD_SLEE:
                        explmask[i][j] |= !!immune_to_sleep(mtmp);
                    case AD_DISN:
                        if (raylevel == P_UNSKILLED && resists_drli(mtmp))
                        resist_death = TRUE;
                        if (resists_death(mtmp))
                            resist_death = TRUE;
                        if (raylevel && resists_magm(mtmp))
                            resist_death = TRUE;
                        if (raylevel >= P_EXPERT && !resists_drli(mtmp))
                            resist_death = FALSE;
                        explmask[i][j] |=
                            !!((olet == WAND_CLASS) ? resist_death :
                               resists_disint(mtmp));
                        break;
                    case AD_ELEC:
                        explmask[i][j] |= !!immune_to_elec(mtmp);
                        break;
                    case AD_DRST:
                        explmask[i][j] |= !!immune_to_poison(mtmp);
                        break;
                    case AD_ACID:
                        explmask[i][j] |= !!immune_to_acid(mtmp);
                        break;
                    default:
                        impossible("explosion type %d?", adtyp);
                        break;
                    }
            }
            reveal_monster_at(i + x - 1, j + y - 1, TRUE);

            if (cansee(i + x - 1, j + y - 1))
                visible = TRUE;
            if (explmask[i][j] == 1)
                any_shield = TRUE;
        }

    if (visible) {
        struct tmp_sym *tsym = tmpsym_init(DISP_BEAM, 0);

        /* Start the explosion */
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++) {
                if (explmask[i][j] == 2)
                    continue;
                tmpsym_change(tsym, dbuf_explosion(expltype, explosion[i][j]));
                tmpsym_at(tsym, i + x - 1, j + y - 1);
            }
        flush_screen(); /* will flush screen and output */

        if (any_shield && flags.sparkle) {      /* simulate shield effect */
            for (k = 0; k < flags.sparkle; k++) {
                for (i = 0; i < 3; i++)
                    for (j = 0; j < 3; j++) {
                        if (explmask[i][j] == 1)
                            /* 
                             * Bypass tmpsym_at() and send the shield glyphs
                             * directly to the buffered screen.  tmpsym_at()
                             * will clean up the location for us later.
                             */
                            dbuf_set_effect(i + x - 1, j + y - 1,
                                            dbuf_effect(E_MISC,
                                                        shield_static[k]));
                    }
                flush_screen(); /* will flush screen and output */
                win_delay_output();
            }

            /* Cover last shield glyph with blast symbol. */
            for (i = 0; i < 3; i++)
                for (j = 0; j < 3; j++) {
                    if (explmask[i][j] == 1)
                        dbuf_set_effect(i + x - 1, j + y - 1,
                                        dbuf_explosion(expltype,
                                                       explosion[i][j]));
                }

        } else {        /* delay a little bit. */
            win_delay_output();
            win_delay_output();
        }

        tmpsym_end(tsym);       /* clear the explosion */
    } else {
        if (olet == MON_EXPLODE) {
            str = "explosion";
        }
        You_hear(msgc_levelsound, "a blast.");
    }

    if (dam)
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++) {
                if (explmask[i][j] == 2)
                    continue;
                if (i + x - 1 == u.ux && j + y - 1 == u.uy)
                    uhurt = (explmask[i][j] == 1) ? 1 : 2;
                idamres = idamnonres = 0;
                if (type >= 0)
                    zap_over_floor((xchar) (i + x - 1), (xchar) (j + y - 1),
                                   type, &shopdamage);

                mtmp = m_at(level, i + x - 1, j + y - 1);
                if (!mtmp && i + x - 1 == u.ux && j + y - 1 == u.uy)
                    mtmp = u.usteed;
                if (!mtmp)
                    continue;
                /* TODO: We want to calculate message channels using
                   combat_msgc(), but that requires knowing the attacker as well
                   as the defender. We don't, so for now, we're just using
                   guesses. */
                if (Engulfed && mtmp == u.ustuck) {
                    if (is_animal(u.ustuck->data))
                        pline(msgc_combatgood, "%s gets %s!", Monnam(u.ustuck),
                              (adtyp == AD_FIRE) ? "heartburn" :
                              (adtyp == AD_COLD) ? "chilly" :
                              (adtyp == AD_DISN) ? ((olet == WAND_CLASS) ?
                                                    "irradiated by pure energy"
                                                    : "perforated") :
                              (adtyp == AD_ELEC) ? "shocked" :
                              (adtyp == AD_DRST) ? "poisoned" :
                              (adtyp == AD_ACID) ? "an upset stomach" :
                              "fried");
                    else
                        pline(msgc_combatgood, "%s gets slightly %s!",
                              Monnam(u.ustuck),
                              (adtyp == AD_FIRE) ? "toasted" :
                              (adtyp == AD_COLD) ? "chilly" :
                              (adtyp == AD_DISN) ? ((olet == WAND_CLASS) ?
                                                    "overwhelmed by pure energy"
                                                    : "perforated") :
                              (adtyp == AD_ELEC) ? "shocked" :
                              (adtyp == AD_DRST) ? "intoxicated" :
                              (adtyp == AD_ACID) ? "burned" : "fried");
                } else if (cansee(i + x - 1, j + y - 1)) {
                    if (mtmp->m_ap_type)
                        seemimic(mtmp);
                    /* TODO: figure out the source and use combat_msgc() */
                    pline(mtmp->mtame ? msgc_petwarning :
                          mtmp->mpeaceful ? msgc_moncombatbad :
                          msgc_monneutral, "%s is caught in %s%s!",
                          Monnam(mtmp),
                          expl_needs_the ? "the " : "", dispbuf);
                }

                if (adtyp == AD_FIRE)
                    burnarmor(mtmp);
                idamnonres += destroy_mitem(mtmp, ALL_CLASSES, (int)adtyp, NULL);
                if (adtyp == AD_FIRE)
                    burn_away_slime(mtmp);

                if (explmask[i][j] == 1) {
                    golemeffects(mtmp, (int)adtyp, dam + idamres);
                    mtmp->mhp -= idamnonres;
                } else {
                    int mdam = dam;

                    if (((adtyp != AD_DISN || !raylevel) &&
                         resist(NULL, mtmp, olet, FALSE, 0)) ||
                        (adtyp == AD_PHYS && half_phys_dam(mtmp)) ||
                        (adtyp == AD_MAGM && resists_magm(mtmp)) ||
                        (adtyp == AD_FIRE && resists_fire(mtmp)) ||
                        (adtyp == AD_COLD && resists_cold(mtmp)) ||
                        (adtyp == AD_SLEE && resists_sleep(mtmp)) ||
                        (adtyp == AD_ELEC && resists_elec(mtmp)) ||
                        (adtyp == AD_DRST && resists_poison(mtmp)) ||
                        (adtyp == AD_ACID && resists_acid(mtmp))) {
                        /* TODO: again, message channel is a guess */
                        if (cansee(i + x - 1, j + y - 1))
                            pline(msgc_monneutral,
                                  "%s resists %s%s!", Monnam(mtmp),
                                  expl_needs_the ? "the " : "", dispbuf);
                        if (adtyp != AD_SLEE) /* reduced in sleep_monst */
                            mdam = dam / 2;
                    }
                    if (resists_cold(mtmp) && adtyp == AD_FIRE)
                        mdam *= 2;
                    else if (resists_fire(mtmp) && adtyp == AD_COLD)
                        mdam *= 2;
                    if (adtyp == AD_SLEE && raylevel) {
                        sleep_monst(NULL, mtmp, mdam, WAND_CLASS);
                        mdam = 0;
                    }
                    if (adtyp == AD_DISN && raylevel) {
                        if (resists_death(mtmp) || resists_magm(mtmp) ||
                            raylevel == P_UNSKILLED)
                            mlosexp(NULL, mtmp, "", FALSE);
                        else
                            mdam = mtmp->mhp; /* instadeath */
                    }
                    mtmp->mhp -= mdam;
                    mtmp->mhp -= (idamres + idamnonres);
                }
                if (mtmp->mhp <= 0) {
                    /* KMH -- Don't blame the player for pets killing gas
                       spores */
                    monkilled(find_mid(level, flags.mon_moving, FM_EVERYWHERE),
                              mtmp, "", (int)adtyp);
                } else if (!flags.mon_moving)
                    setmangry(mtmp);
            }

    /* Do your injury last */
    if (uhurt) {
        if (flags.verbose)
            pline_implied(olet == MON_EXPLODE ?
                          msgc_nonmonbad : msgc_nonmonbad,
                          "You are caught in %s%s!", expl_needs_the ? "the " : "",
                  dispbuf);
        /* do property damage first, in case we end up leaving bones */
        if (adtyp == AD_FIRE)
            burn_away_slime(&youmonst);
        if (u.uinvulnerable) {
            damu = 0;
            pline(msgc_playerimmune, "You are unharmed!");
        } else if ((adtyp == AD_PHYS && half_phys_dam(&youmonst)) ||
                   (adtyp == AD_MAGM && resists_magm(&youmonst)) ||
                   (adtyp == AD_FIRE && resists_fire(&youmonst)) ||
                   (adtyp == AD_COLD && resists_cold(&youmonst)) ||
                   (adtyp == AD_SLEE && resists_sleep(&youmonst)) ||
                   (adtyp == AD_ELEC && resists_elec(&youmonst)) ||
                   (adtyp == AD_DRST && resists_poison(&youmonst)) ||
                   (adtyp == AD_ACID && resists_acid(&youmonst))) {
            pline(combat_msgc(NULL, &youmonst, cr_resist),
                  "You resist %s%s", expl_needs_the ? "the " : "", dispbuf);
            if (adtyp != AD_SLEE)
                damu = (damu + 1) / 2;
        }
        if (damu && raylevel && uhurt == 2) {
            if (adtyp == AD_SLEE) {
                sleep_monst(NULL, &youmonst, damu, WAND_CLASS);
                damu = 0;
            }
            if (adtyp == AD_DISN) {
                if (resists_death(&youmonst) || Antimagic ||
                    raylevel == P_UNSKILLED) {
                    losexp("drained by a death field",FALSE);
                    damu = 0;
                } else {
                    done(DIED, "killed by a death field");
                    damu = 0; /* lifesaved */
                }
            }
        }
        if (adtyp == AD_FIRE)
            burnarmor(&youmonst);
        int item_dmg = destroy_mitem(&youmonst, ALL_CLASSES, (int)adtyp, &killer);
        damu += item_dmg;

        ugolemeffects((int)adtyp, damu);
        if (uhurt == 2) {
            if (Upolyd)
                u.mh -= damu;
            else
                u.uhp -= damu;
        }

        if (u.uhp <= 0 || (Upolyd && u.mh <= 0)) {
            int death = adtyp == AD_FIRE ? BURNING : DIED;

            struct monst *offender = find_mid(level, flags.mon_moving,
                                              FM_EVERYWHERE);

            /* Maybe the item destruction killed */
            if (item_dmg && item_dmg >= rnd(damu + item_dmg))
                ; /* killer is set above */
            if (olet == MON_EXPLODE) {
                killer = killer_msg(death, an(str));
            } else if (type >= 0 && olet != SCROLL_CLASS) {
                /* check whether or not we were the source of the explosion */
                if (offender == &youmonst)
                    killer = msgprintf("caught %sself in %s own %s", uhim(),
                                       uhis(), str);
                else if (offender)
                    killer = msgprintf("killed by %s %s",
                                       s_suffix(k_monnam(offender)), str);
                else
                    killer = msgprintf("killed by a %s", str);
            } else if (!strcmp(str, "burning oil")) {
                /* This manual check hack really sucks */
                killer = killer_msg(death, str);
            } else if (offender && offender != &youmonst) {
                killer = killer_msg(death,
                                    msgprintf("%s %s",
                                              s_suffix(k_monnam(offender)),
                                              str));
            } else {
                killer = killer_msg(death, an(str));
            }
            /* Known BUG: BURNING suppresses corpse in bones data, but done 
               does not handle killer reason correctly */
            if (Upolyd) {
                rehumanize(death, killer);
            } else {
                done(death, killer);
            }
        }
        exercise(A_STR, FALSE);
    }

    if (shopdamage) {
        pay_for_damage(adtyp == AD_FIRE ? "burn away" : adtyp ==
                       AD_COLD ? "shatter" : adtyp ==
                       AD_DISN ? "disintegrate" : "destroy", FALSE);
    }

    /* explosions are noisy */
    i = dam * dam;
    if (i < 50)
        i = 50; /* in case random damage is very small */
    wake_nearto(x, y, i);
}


struct scatter_chain {
    struct scatter_chain *next; /* pointer to next scatter item */
    struct obj *obj;    /* pointer to the object */
    xchar ox;   /* location of */
    xchar oy;   /* item */
    schar dx;   /* direction of */
    schar dy;   /* travel */
    int range;  /* range of object */
    boolean stopped;    /* flag for in-motion/stopped */
};

/*
 * scflags:
 *      VIS_EFFECTS     Add visual effects to display
 *      MAY_HITMON      Objects may hit monsters
 *      MAY_HITYOU      Objects may hit hero
 *      MAY_HIT         Objects may hit you or monsters
 *      MAY_DESTROY     Objects may be destroyed at random
 *      MAY_FRACTURE    Stone objects can be fractured (statues, boulders)
 */

/* returns number of scattered objects */
long
scatter(int sx, int sy, /* location of objects to scatter */
        int blastforce, /* force behind the scattering */
        unsigned int scflags, struct obj *obj)
{       /* only scatter this obj */
    struct obj *otmp;
    struct level *lev = obj ? obj->olev : level;
    int tmp;
    int farthest = 0;
    uchar typ;
    long qtmp;
    boolean used_up;
    boolean individual_object = obj ? TRUE : FALSE;
    struct monst *mtmp;
    struct scatter_chain *stmp, *stmp2 = 0;
    struct scatter_chain *schain = NULL;
    long total = 0L;
    boolean visible = (lev == level && cansee(sx, sy));

    while ((otmp = individual_object ? obj : lev->objects[sx][sy]) != 0) {
        if (otmp->quan > 1L) {
            qtmp = otmp->quan - 1;
            if (qtmp > LARGEST_INT)
                qtmp = LARGEST_INT;
            qtmp = (long)rnd((int)qtmp);
            otmp = splitobj(otmp, qtmp);
        } else {
            obj = NULL; /* all used */
        }
        obj_extract_self(otmp);
        used_up = FALSE;

        /* 9 in 10 chance of fracturing boulders or statues */
        if ((scflags & MAY_FRACTURE)
            && ((otmp->otyp == BOULDER) || (otmp->otyp == STATUE))
            && rn2(10)) {
            if (otmp->otyp == BOULDER) {
                if (visible)
                    pline(msgc_consequence, "%s apart.",
                          Tobjnam(otmp, "break"));
                fracture_rock(otmp);
                place_object(otmp, lev, sx, sy);
                if ((otmp = sobj_at(BOULDER, lev, sx, sy)) != 0) {
                    /* another boulder here, restack it to the top */
                    obj_extract_self(otmp);
                    place_object(otmp, lev, sx, sy);
                }
            } else {
                struct trap *trap;

                if ((trap = t_at(lev, sx, sy)) && trap->ttyp == STATUE_TRAP)
                    deltrap(lev, trap);
                if (visible)
                    pline(msgc_consequence, "%s.", Tobjnam(otmp, "crumble"));
                break_statue(otmp);
                place_object(otmp, lev, sx, sy); /* put fragments on floor */
            }
            used_up = TRUE;

            /* 1 in 10 chance of destruction of obj; glass, egg destruction */
        } else if ((scflags & MAY_DESTROY) &&
                   (!rn2(10) || (objects[otmp->otyp].oc_material == GLASS ||
                                 otmp->otyp == EGG))) {
            if (breaks(otmp, (xchar) sx, (xchar) sy))
                used_up = TRUE;
        }

        if (!used_up) {
            stmp = malloc(sizeof (struct scatter_chain));
            stmp->next = NULL;
            stmp->obj = otmp;
            stmp->ox = sx;
            stmp->oy = sy;
            tmp = rn2(8);       /* get the direction */
            stmp->dx = xdir[tmp];
            stmp->dy = ydir[tmp];
            tmp = blastforce - (otmp->owt / 40);
            if (tmp < 1)
                tmp = 1;
            stmp->range = rnd(tmp);     /* anywhere up to that determ. by wt */
            if (farthest < stmp->range)
                farthest = stmp->range;
            stmp->stopped = FALSE;
            if (!schain)
                schain = stmp;
            else
                stmp2->next = stmp;
            stmp2 = stmp;
        }
    }

    while (farthest-- > 0) {
        for (stmp = schain; stmp; stmp = stmp->next) {
            if ((stmp->range-- > 0) && (!stmp->stopped)) {
                bhitpos.x = stmp->ox + stmp->dx;
                bhitpos.y = stmp->oy + stmp->dy;
                typ = lev->locations[bhitpos.x][bhitpos.y].typ;
                if (!isok(bhitpos.x, bhitpos.y)) {
                    bhitpos.x -= stmp->dx;
                    bhitpos.y -= stmp->dy;
                    stmp->stopped = TRUE;
                } else if (!ZAP_POS(typ) ||
                           closed_door(lev, bhitpos.x, bhitpos.y)) {
                    bhitpos.x -= stmp->dx;
                    bhitpos.y -= stmp->dy;
                    stmp->stopped = TRUE;
                } else if ((mtmp = m_at(lev, bhitpos.x, bhitpos.y)) != 0) {
                    if (scflags & MAY_HITMON) {
                        stmp->range--;
                        if (ohitmon(mtmp, stmp->obj, NULL, NULL,
                                    1, FALSE)) {
                            stmp->obj = NULL;
                            stmp->stopped = TRUE;
                        }
                    }
                } else if (bhitpos.x == u.ux && bhitpos.y == u.uy) {
                    if (scflags & MAY_HITYOU) {
                        int hitvalu, hitu;

                        action_interrupted();

                        hitvalu = 8 + stmp->obj->spe;
                        if (bigmonst(youmonst.data))
                            hitvalu++;
                        hitu =
                            thitu(hitvalu, dmgval(stmp->obj, &youmonst),
                                  stmp->obj, NULL);
                        if (hitu)
                            stmp->range -= 3;
                    }
                } else {
                    if (scflags & VIS_EFFECTS) {
                        /* tmpsym_at(bhitpos.x, bhitpos.y); */
                        /* delay_output(); */
                    }
                }
                stmp->ox = bhitpos.x;
                stmp->oy = bhitpos.y;
            }
        }
    }
    for (stmp = schain; stmp; stmp = stmp2) {
        int x, y;

        stmp2 = stmp->next;
        x = stmp->ox;
        y = stmp->oy;
        if (stmp->obj) {
            if (x != sx || y != sy)
                total += stmp->obj->quan;
            place_object(stmp->obj, lev, x, y);
            stackobj(stmp->obj);
        }
        free(stmp);
        if (lev == level)
            newsym(x, y);
    }

    return total;
}


/*
 * Splatter burning oil from x,y to the surrounding area.
 *
 * This routine should really take a how and direction parameters.
 * The how is how it was caused, e.g. kicked verses thrown.  The
 * direction is which way to spread the flaming oil.  Different
 * "how"s would give different dispersal patterns.  For example,
 * kicking a burning flask will splatter differently from a thrown
 * flask hitting the ground.
 *
 * For now, just perform a "regular" explosion.
 */
void
splatter_burning_oil(int x, int y)
{
/* ZT_SPELL(ZT_FIRE) = ZT_SPELL(AD_FIRE-1) = 10+(2-1) = 11 */
#define ZT_SPELL_O_FIRE 11      /* value kludge, see zap.c */
    explode(x, y, ZT_SPELL_O_FIRE, dice(4, 4), BURNING_OIL, EXPL_FIERY, NULL, 0);
}

/*explode.c*/

