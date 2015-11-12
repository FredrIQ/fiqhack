/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

extern const int monstr[];

/* monster mage spells */
#define MGC_PSI_BOLT    0
#define MGC_CURE_SELF   1
#define MGC_HASTE_SELF  2
#define MGC_STUN_YOU    3
#define MGC_DISAPPEAR   4
#define MGC_WEAKEN_YOU  5
#define MGC_DESTRY_ARMR 6
#define MGC_CURSE_ITEMS 7
#define MGC_AGGRAVATION 8
#define MGC_SUMMON_MONS 9
#define MGC_CLONE_WIZ   10
#define MGC_DEATH_TOUCH 11

/* monster cleric spells */
#define CLC_OPEN_WOUNDS 0
#define CLC_CURE_SELF   1
#define CLC_CONFUSE_YOU 2
#define CLC_PARALYZE    3
#define CLC_BLIND_YOU   4
#define CLC_INSECTS     5
#define CLC_CURSE_ITEMS 6
#define CLC_LIGHTNING   7
#define CLC_FIRE_PILLAR 8
#define CLC_GEYSER      9

static void cursetxt(struct monst *, boolean);
static int choose_magic_spell(int);
static int choose_clerical_spell(int);
static void cast_wizard_spell(struct monst *, int, int);
static void cast_cleric_spell(struct monst *, int, int);
static boolean is_undirected_spell(unsigned int, int);
static boolean mmspell_would_be_useless(struct monst *, struct monst *,
                                        unsigned int, int);
static void ucast_wizard_spell(struct monst *, struct monst *, int, int);
static void ucast_cleric_spell(struct monst *, struct monst *, int, int);

/* feedback when frustrated monster couldn't cast a spell */
static void
cursetxt(struct monst *mtmp, boolean undirected)
{
    if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my)) {
        const char *point_msg;  /* spellcasting monsters are impolite */

        if (undirected)
            point_msg = "all around, then curses";
        else
            point_msg = ((const char *[]){
                    [mar_unaware] = NULL,
                    [mar_guessing_invis] =
                        "and curses in your general direction",
                    [mar_guessing_displaced] =
                        "and curses at your displaced image",
                    [mar_guessing_other] = /* e.g. mimic polyform*/
                        "and curses in your general direction",
                    [mar_aware] =
                        "at you, then curses"
                })[awareness_reason(mtmp)];

        if (!point_msg)
            impossible("monster directed-spellcasting at player "
                       "despite !aware_of_u?");
        pline("%s points %s.", Monnam(mtmp), point_msg);

    } else if ((!(moves % 4) || !rn2(4)) && canhear()) {
            pline_once("You hear a mumbled curse.");
    }
}


/* convert a level based random selection into a specific mage spell;
   inappropriate choices will be screened out by mmspell_would_be_useless() */
static int
choose_magic_spell(int spellval)
{
    switch (spellval) {
    case 22:
    case 21:
    case 20:
        return MGC_DEATH_TOUCH;
    case 19:
    case 18:
        return MGC_CLONE_WIZ;
    case 17:
    case 16:
    case 15:
        return MGC_SUMMON_MONS;
    case 14:
    case 13:
        return MGC_AGGRAVATION;
    case 12:
    case 11:
    case 10:
        return MGC_CURSE_ITEMS;
    case 9:
    case 8:
        return MGC_DESTRY_ARMR;
    case 7:
    case 6:
        return MGC_WEAKEN_YOU;
    case 5:
    case 4:
        return MGC_DISAPPEAR;
    case 3:
        return MGC_STUN_YOU;
    case 2:
        return MGC_HASTE_SELF;
    case 1:
        return MGC_CURE_SELF;
    case 0:
    default:
        return MGC_PSI_BOLT;
    }
}

/* convert a level based random selection into a specific cleric spell */
static int
choose_clerical_spell(int spellnum)
{
    switch (spellnum) {
    case 13:
        return CLC_GEYSER;
    case 12:
        return CLC_FIRE_PILLAR;
    case 11:
        return CLC_LIGHTNING;
    case 10:
    case 9:
        return CLC_CURSE_ITEMS;
    case 8:
        return CLC_INSECTS;
    case 7:
    case 6:
        return CLC_BLIND_YOU;
    case 5:
    case 4:
        return CLC_PARALYZE;
    case 3:
    case 2:
        return CLC_CONFUSE_YOU;
    case 1:
        return CLC_CURE_SELF;
    case 0:
    default:
        return CLC_OPEN_WOUNDS;
    }
}

/* return values:
 * 1: successful spell
 * 0: unsuccessful spell
 */
int
castmu(struct monst *mtmp, const struct attack *mattk, int allow_directed)
{
    int dmg, ml = mtmp->m_lev;
    int ret;
    int spellnum = 0;
    boolean casting_directed = FALSE;
    boolean casting_elemental = (mattk->adtyp != AD_SPEL &&
                                 mattk->adtyp != AD_CLRC);

    if (!casting_elemental && ml) {
        /*
         * We have four cases here, up from three in 3.4.3.
         *
         * - The monster is in combat (allow_directed is 1), and knows your
         *   location. It searches for and uses a useful spell.
         *
         * - The monster is in combat, and doesn't know your location but thinks
         *   it does. It searches for a useful spell, attempts to use it, and
         *   succeeds if and only if it's undirected (otherwise it curses).
         *
         *   Setting allow_directed to -1 will force this specific failure case.
         *   Eventually, that will be the only way for this failure mode to
         *   happen (and castmu will only be called if known_ux_uy).
         *
         * - The monster is in combat, but doesn't know your location. It casts
         *   an undirected spell, if it can. (This case is new in 4.3.)
         *
         * - The monster is not in combat. It selects a random spell. If it's
         *   directed or useless, this function returns without doing anything,
         *   and the monster does something else instead; this serves as a
         *   method to reduce the frequency of spellcasts outside
         *   combat. Otherwise, it casts it as in the previous case.
         *
         */

        int cnt = 40;

        do {
            spellnum = rn2(ml);
            if (mattk->adtyp == AD_SPEL)
                spellnum = choose_magic_spell(spellnum);
            else
                spellnum = choose_clerical_spell(spellnum);

            casting_directed = !is_undirected_spell(mattk->adtyp, spellnum);

            /* not trying to attack? don't allow directed spells */
            if (!allow_directed) {
                if (casting_directed ||
                    mmspell_would_be_useless(mtmp, &youmonst,
                                             mattk->adtyp, spellnum)) {
                    return 0;
                }
                break;
            }
        } while (--cnt > 0 &&
                 ((casting_directed && !aware_of_u(mtmp)) ||
                  mmspell_would_be_useless(mtmp, &youmonst,
                                           mattk->adtyp, spellnum)));
        if (cnt == 0)
            return 0;
    }

    /* note: at this point, casting_directed implies aware_of_u (although not
       necessarily knows_ux_uy, a stronger condition) */

    /* monster unable to cast spells? */
    if (mtmp->mcan || (mtmp->mspec_used && !casting_elemental) || !ml) {
        cursetxt(mtmp, !casting_directed);
        return 0;
    }

    if (!casting_elemental) {
        mtmp->mspec_used = 10 - mtmp->m_lev;
        if (mtmp->mspec_used < 2)
            mtmp->mspec_used = 2;
    }

    /* monster can cast spells, but is casting a directed spell at the wrong
       place? If so, give a message, and return.  Do this *after* penalizing
       mspec_used. TODO: Merge all this up into a castmq(), so we can get the
       location right. */
    if ((!knows_ux_uy(mtmp) || allow_directed == -1) && casting_directed) {
        pline("%s casts a spell at %s!",
              canclassifymon(mtmp) ? Monnam(mtmp) : "Something",
              level->locations[mtmp->mux][mtmp->muy].typ ==
              WATER ? "empty water" : "thin air");
        return 0;
    }

    action_interrupted();

    if (rn2(ml * 10) < (mtmp->mconf ? 100 : 20)) {      /* fumbled attack */
        if (canseemon(mtmp) && canhear())
            pline("The air crackles around %s.", mon_nam(mtmp));
        return 0;
    }
    if (canspotmon(mtmp) || casting_directed) {
        pline("%s casts a spell%s!",
              canspotmon(mtmp) ? Monnam(mtmp) : "Something",
              (!casting_directed && !casting_elemental) ? "" :
              awareness_reason(mtmp) == mar_guessing_displaced ?
              " at your displaced image" :
              awareness_reason(mtmp) == mar_aware ? " at you" :
              " at a spot near you");
    }

    /*
     * As these are spells, the damage is related to the level
     * of the monster casting the spell.
     */
    if (!knows_ux_uy(mtmp)) {
        dmg = 0;
        if (casting_elemental) {
            /* The monster's casting a damage spell (as opposed to monster
               spell) at the wrong location. We already printed a message if the
               player is aware of the monster casting it. If the monster missed
               the player and the player can't see the monster, we don't
               currently produce a message. TODO: consider changing this. */
            return 0;
        }
    } else if (mattk->damd)
        dmg = dice((int)((ml / 2) + mattk->damn), (int)mattk->damd);
    else
        dmg = dice((int)((ml / 2) + 1), 6);
    if (Half_spell_damage)
        dmg = (dmg + 1) / 2;

    ret = 1;

    switch (mattk->adtyp) {

    case AD_FIRE:
        pline("You're enveloped in flames.");
        if (Fire_resistance) {
            shieldeff(u.ux, u.uy);
            pline("But you resist the effects.");
            dmg = 0;
        }
        burn_away_slime();
        break;
    case AD_COLD:
        pline("You're covered in frost.");
        if (Cold_resistance) {
            shieldeff(u.ux, u.uy);
            pline("But you resist the effects.");
            dmg = 0;
        }
        break;
    case AD_MAGM:
        pline("You are hit by a shower of missiles!");
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("The missiles bounce off!");
            dmg = 0;
        } else
            dmg = dice((int)mtmp->m_lev / 2 + 1, 6);
        break;
    case AD_SPEL:      /* wizard spell */
    case AD_CLRC:      /* clerical spell */
        {
            if (mattk->adtyp == AD_SPEL)
                cast_wizard_spell(mtmp, dmg, spellnum);
            else
                cast_cleric_spell(mtmp, dmg, spellnum);
            dmg = 0;    /* done by the spell casting functions */
            break;
        }
    }
    if (dmg)
        mdamageu(mtmp, dmg);
    return ret;
}

/* monster wizard and cleric spellcasting functions */
/*
   If dmg is zero, then the monster is not casting at you.
   If the monster is intentionally not casting at you, we have previously
   called mmspell_would_be_useless() and spellnum should always be a valid
   undirected spell.
   If you modify either of these, be sure to change is_undirected_spell()
   and mmspell_would_be_useless().
 */
static void
cast_wizard_spell(struct monst *mtmp, int dmg, int spellnum)
{
    if (dmg == 0 && !is_undirected_spell(AD_SPEL, spellnum)) {
        impossible("cast directed wizard spell (%d) with dmg=0?", spellnum);
        return;
    }

    switch (spellnum) {
    case MGC_DEATH_TOUCH:
        pline("Oh no, %s's using the touch of death!", mhe(mtmp));
        if (nonliving(youmonst.data) || is_demon(youmonst.data)) {
            pline("You seem no deader than before.");
        } else if (!Antimagic && rn2(mtmp->m_lev) > 12) {
            if (Hallucination) {
                pline("You have an out of body experience.");
            } else {
                done(DIED, killer_msg(DIED, "the touch of death"));
            }
        } else {
            if (Antimagic)
                shieldeff(u.ux, u.uy);
            pline("Lucky for you, it didn't work!");
        }
        dmg = 0;
        break;
    case MGC_CLONE_WIZ:
        if (mtmp->iswiz && flags.no_of_wizards == 1) {
            pline("Double Trouble...");
            clonewiz();
            dmg = 0;
        } else
            impossible("bad wizard cloning?");
        break;
    case MGC_SUMMON_MONS:
        {
            int count;

            count = nasty(mtmp);        /* summon something nasty */
            if (mtmp->iswiz)
                verbalize("Destroy the thief, my pet%s!", plur(count));
            else {
                const char *mappear =
                    (count == 1) ? "A monster appears" : "Monsters appear";

                /* messages not quite right if plural monsters created but only
                   a single monster is seen */
                switch (awareness_reason(mtmp)) {
                case mar_aware:
                    pline("%s from nowhere!", mappear);
                    break;
                case mar_guessing_displaced:
                    pline("%s around your displaced image!", mappear);
                    break;
                default:
                    pline("%s around a spot near you!", mappear);
                }
            }
            dmg = 0;
            break;
        }
    case MGC_AGGRAVATION:
        pline("You feel that monsters are aware of your presence.");
        aggravate();
        dmg = 0;
        break;
    case MGC_CURSE_ITEMS:
        pline("You feel as if you need some help.");
        rndcurse();
        dmg = 0;
        break;
    case MGC_DESTRY_ARMR:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("A field of force surrounds you!");
        } else if (!destroy_arm(some_armor(&youmonst))) {
            pline("Your skin itches.");
        }
        dmg = 0;
        break;
    case MGC_WEAKEN_YOU:       /* drain strength */
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("You feel momentarily weakened.");
        } else {
            pline("You suddenly feel weaker!");
            dmg = mtmp->m_lev - 6;
            if (Half_spell_damage)
                dmg = (dmg + 1) / 2;
            losestr(rnd(dmg), DIED, msgcat("was drained of all strength by ",
                                           k_monnam(mtmp)), mtmp);
        }
        dmg = 0;
        break;
    case MGC_DISAPPEAR:        /* makes self invisible */
        if (!mtmp->minvis && !mtmp->invis_blkd) {
            if (canseemon(mtmp))
                pline("%s suddenly %s!", Monnam(mtmp),
                      !See_invisible ? "disappears" : "becomes transparent");
            mon_set_minvis(mtmp);
            dmg = 0;
        } else
            impossible("no reason for monster to cast disappear spell?");
        break;
    case MGC_STUN_YOU:
        if (Antimagic || Free_action) {
            shieldeff(u.ux, u.uy);
            if (!Stunned)
                pline("You feel momentarily disoriented.");
            make_stunned(1L, FALSE);
        } else {
            pline(Stunned ? "You struggle to keep your balance." :
                  "You reel...");
            dmg = dice(ACURR(A_DEX) < 12 ? 6 : 4, 4);
            if (Half_spell_damage)
                dmg = (dmg + 1) / 2;
            make_stunned(HStun + dmg, FALSE);
        }
        dmg = 0;
        break;
    case MGC_HASTE_SELF:
        mon_adjust_speed(mtmp, 1, NULL);
        dmg = 0;
        break;
    case MGC_CURE_SELF:
        if (mtmp->mhp < mtmp->mhpmax) {
            if (canseemon(mtmp))
                pline("%s looks better.", Monnam(mtmp));
            /* note: player healing does 6d4; this used to do 1d8 */
            if ((mtmp->mhp += dice(3, 6)) > mtmp->mhpmax)
                mtmp->mhp = mtmp->mhpmax;
        }
        dmg = 0;
        break;
    case MGC_PSI_BOLT:
        /* prior to 3.4.0 Antimagic was setting the damage to 1--this made the
           spell virtually harmless to players with magic res. */
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            dmg = (dmg + 1) / 2;
        }
        if (dmg <= 5)
            pline("You get a slight %sache.", body_part(HEAD));
        else if (dmg <= 10)
            pline("Your brain is on fire!");
        else if (dmg <= 20)
            pline("Your %s suddenly aches painfully!", body_part(HEAD));
        else
            pline("Your %s suddenly aches very painfully!", body_part(HEAD));
        break;
    default:
        impossible("mcastu: invalid magic spell (%d)", spellnum);
        dmg = 0;
        break;
    }

    if (dmg)
        mdamageu(mtmp, dmg);
}

static void
cast_cleric_spell(struct monst *mtmp, int dmg, int spellnum)
{
    if (dmg == 0 && !is_undirected_spell(AD_CLRC, spellnum)) {
        impossible("cast directed cleric spell (%d) with dmg=0?", spellnum);
        return;
    }

    switch (spellnum) {
    case CLC_GEYSER:
        /* this is physical damage, not magical damage */
        pline("A sudden geyser slams into you from nowhere!");
        dmg = dice(8, 6);
        if (Half_physical_damage)
            dmg = (dmg + 1) / 2;
        break;
    case CLC_FIRE_PILLAR:
        pline("A pillar of fire strikes all around you!");
        if (Fire_resistance) {
            shieldeff(u.ux, u.uy);
            dmg = 0;
        } else
            dmg = dice(8, 6);
        if (Half_spell_damage)
            dmg = (dmg + 1) / 2;
        burn_away_slime();
        burnarmor(&youmonst);
        destroy_item(SCROLL_CLASS, AD_FIRE);
        destroy_item(POTION_CLASS, AD_FIRE);
        destroy_item(SPBOOK_CLASS, AD_FIRE);
        burn_floor_paper(level, u.ux, u.uy, TRUE, FALSE);
        break;
    case CLC_LIGHTNING:
        {
            boolean reflects;

            pline("A bolt of lightning strikes down at you from above!");
            reflects = ureflects("It bounces off your %s%s.", "");
            if (reflects || Shock_resistance) {
                shieldeff(u.ux, u.uy);
                dmg = 0;
            } else
                dmg = dice(8, 6);
            if (!reflects) {
                if (Half_spell_damage) dmg = (dmg + 1) / 2;
                destroy_item(WAND_CLASS, AD_ELEC);
                destroy_item(RING_CLASS, AD_ELEC);
            }
            if (!resists_blnd(&youmonst)) {
                pline("You are blinded by the flash!");
                make_blinded((long)rnd(100),FALSE);
                if (!Blind)
                    pline("Your vision quickly clears.");
            }
            break;
        }
    case CLC_CURSE_ITEMS:
        pline("You feel as if you need some help.");
        rndcurse();
        dmg = 0;
        break;
    case CLC_INSECTS:
        {
            /* Try for insects, and if there are none left, go for (sticks to)
               snakes.  -3. */
            const struct permonst *pm = mkclass(&u.uz, S_ANT, 0, rng_main);
            struct monst *mtmp2 = NULL;
            char let = (pm ? S_ANT : S_SNAKE);
            boolean success;
            int i;
            coord bypos;
            int quan;

            /* If engulfed, the monster can be aware of you without the muxy
               being set correctly */
            int centered_on_muxy = aware_of_u(mtmp) && !engulfing_u(mtmp);

            quan = (mtmp->m_lev < 2) ? 1 : rnd((int)mtmp->m_lev / 2);
            if (quan < 3)
                quan = 3;
            success = pm ? TRUE : FALSE;
            for (i = 0; i <= quan; i++) {
                int spelltarget_x = centered_on_muxy ? mtmp->mux : mtmp->mx;
                int spelltarget_y = centered_on_muxy ? mtmp->muy : mtmp->my;

                if (!enexto(&bypos, level,
                            spelltarget_x, spelltarget_y, mtmp->data))
                    break;
                if ((pm = mkclass(&u.uz, let, 0, rng_main)) != 0 &&
                    (mtmp2 = makemon(pm, level, bypos.x, bypos.y,
                                     MM_CREATEMONSTER | MM_CMONSTER_M)) != 0) {
                    success = TRUE;
                    mtmp2->msleeping = 0;
                    msethostility(mtmp, TRUE, TRUE);
                }
            }
            /*
             * Not quite right:
             * -- message doesn't always make sense for unseen caster
             *    (particularly the first message)
             * -- message assumes plural monsters summoned
             *    (non-plural should be very rare, unlike in nasty())
             * -- message assumes plural monsters seen
             */
            if (!success)
                pline("%s casts at a clump of sticks, but nothing happens.",
                      Monnam(mtmp));
            else if (let == S_SNAKE)
                pline("%s transforms a clump of sticks into snakes!",
                      Monnam(mtmp));
            else switch (awareness_reason(mtmp)) {
                case mar_guessing_displaced:
                    pline("%s summons insects around your displaced image!",
                          Monnam(mtmp));
                    break;
                case mar_aware:
                    pline("%s summons insects!", Monnam(mtmp));
                    break;
                default:
                    pline("%s summons insects around a spot near you!",
                          Monnam(mtmp));
                }
            break;
        }
    case CLC_BLIND_YOU:
        /* note: resists_blnd() doesn't apply here */
        if (!Blinded) {
            int num_eyes = eyecount(youmonst.data);

            pline("Scales cover your %s!",
                  (num_eyes ==
                   1) ? body_part(EYE) : makeplural(body_part(EYE)));
            make_blinded(Half_spell_damage ? 100L : 200L, FALSE);
            if (!Blind)
                pline("Your vision quickly clears.");
            dmg = 0;
        } else
            impossible("no reason for monster to cast blindness spell?");
        break;
    case CLC_PARALYZE:
        if (Antimagic || Free_action) {
            shieldeff(u.ux, u.uy);
            if (!u_helpless(hm_all))
                pline("You stiffen briefly.");
            helpless(1, hr_paralyzed, "paralyzed by a monster", NULL);
        } else {
            if (!u_helpless(hm_all))
                pline("You are frozen in place!");
            dmg = 4 + (int)mtmp->m_lev;
            if (Half_spell_damage)
                dmg = (dmg + 1) / 2;
            helpless(dmg, hr_paralyzed, "paralyzed by a monster", NULL);
        }
        dmg = 0;
        break;
    case CLC_CONFUSE_YOU:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("You feel momentarily dizzy.");
        } else {
            boolean oldprop = ! !Confusion;

            dmg = (int)mtmp->m_lev;
            if (Half_spell_damage)
                dmg = (dmg + 1) / 2;
            make_confused(HConfusion + dmg, TRUE);
            if (Hallucination)
                pline("You feel %s!", oldprop ? "trippier" : "trippy");
            else
                pline("You feel %sconfused!", oldprop ? "more " : "");
        }
        dmg = 0;
        break;
    case CLC_CURE_SELF:
        if (mtmp->mhp < mtmp->mhpmax) {
            if (canseemon(mtmp))
                pline("%s looks better.", Monnam(mtmp));
            /* note: player healing does 6d4; this used to do 1d8 */
            if ((mtmp->mhp += dice(3, 6)) > mtmp->mhpmax)
                mtmp->mhp = mtmp->mhpmax;
            dmg = 0;
        }
        break;
    case CLC_OPEN_WOUNDS:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            dmg = (dmg + 1) / 2;
        }
        if (dmg <= 5)
            pline("Your skin itches badly for a moment.");
        else if (dmg <= 10)
            pline("Wounds appear on your body!");
        else if (dmg <= 20)
            pline("Severe wounds appear on your body!");
        else
            pline("Your body is covered with painful wounds!");
        break;
    default:
        impossible("mcastu: invalid clerical spell (%d)", spellnum);
        dmg = 0;
        break;
    }

    if (dmg)
        mdamageu(mtmp, dmg);
}

static boolean
is_undirected_spell(unsigned int adtyp, int spellnum)
{
    if (adtyp == AD_SPEL) {
        switch (spellnum) {
        case MGC_CLONE_WIZ:
        case MGC_SUMMON_MONS:
        case MGC_AGGRAVATION:
        case MGC_DISAPPEAR:
        case MGC_HASTE_SELF:
        case MGC_CURE_SELF:
            return TRUE;
        default:
            break;
        }
    } else if (adtyp == AD_CLRC) {
        switch (spellnum) {
        case CLC_INSECTS:
        case CLC_CURE_SELF:
            return TRUE;
        default:
            break;
        }
    }
    return FALSE;
}

/* Some spells are useless under some circumstances. */
static boolean
mmspell_would_be_useless(struct monst *magr, struct monst *mdef,
                         unsigned int adtyp, int spellnum)
{
    /* Can the aggressor see the square it thinks the defender is on? */
    int believed_mdef_mx = mdef ? m_mx(mdef) : -1;
    int believed_mdef_my = mdef ? m_my(mdef) : -1;
    if (mdef == &youmonst && !engulfing_u(mdef)) {
        believed_mdef_mx = magr->mux;
        believed_mdef_my = magr->muy;
    }
    char **appropriate_vizarray = viz_array;
    if (Engulfed && (magr == u.ustuck || mdef == u.ustuck))
        appropriate_vizarray = NULL;

    boolean believed_loe = mdef ? clear_path(magr->mx, magr->my,
                                             believed_mdef_mx, believed_mdef_my,
                                             appropriate_vizarray) : FALSE;
    boolean magr_peaceful = magr == &youmonst || magr->mpeaceful;
    boolean magr_tame = magr == &youmonst || magr->mtame;

    if (adtyp == AD_SPEL) {
        /* monster creation and aggravation either aren't properly symmetrical,
           or would cause problems if they were */
        if (magr_peaceful &&
            (spellnum == MGC_AGGRAVATION || spellnum == MGC_SUMMON_MONS ||
             spellnum == MGC_CLONE_WIZ))
            return TRUE;
        /* haste self when already fast */
        if (m_has_property(magr, FAST, ANY_PROPERTY, TRUE) &&
            spellnum == MGC_HASTE_SELF)
            return TRUE;
        /* invisibility when already invisible */
        if (m_has_property(magr, INVIS, ANY_PROPERTY, TRUE) &&
            spellnum == MGC_DISAPPEAR)
            return TRUE;
        /* invisibility when wearing a mummy wrapping */
        if (spellnum == MGC_DISAPPEAR &&
            ((magr == &youmonst && BInvis) ||
             (magr != &youmonst && magr->invis_blkd)))
            return TRUE;
        /* peaceful monster won't cast invisibility if you can't see invisible,
           same as when monsters drink potions of invisibility.  This doesn't
           really make a lot of sense, but lets the player avoid hitting
           peaceful monsters by mistake */
        if (magr != &youmonst && magr_peaceful && !See_invisible &&
            spellnum == MGC_DISAPPEAR)
            return TRUE;
        /* healing when already healed */
        if (m_mhp(magr) == m_mhpmax(magr) && spellnum == MGC_CURE_SELF)
            return TRUE;
        /* don't summon monsters if it doesn't think you're around */
        if (!believed_loe &&
            (spellnum == MGC_SUMMON_MONS ||
             (!magr->iswiz && spellnum == MGC_CLONE_WIZ)))
            return TRUE;
        if ((!magr->iswiz || flags.no_of_wizards > 1)
            && spellnum == MGC_CLONE_WIZ)
            return TRUE;
        /* spells that harm master while tame and not conflicted */
        if (magr_tame && !Conflict &&
            (spellnum == MGC_CURSE_ITEMS || spellnum == MGC_DISAPPEAR ||
             spellnum == MGC_DESTRY_ARMR))
            return TRUE;

    } else if (adtyp == AD_CLRC) {
        /* as with the summoning spells above */
        if (magr_peaceful && spellnum == CLC_INSECTS)
            return TRUE;
        /* healing when already healed */
        if (m_mhp(magr) == m_mhpmax(magr) && spellnum == CLC_CURE_SELF)
            return TRUE;
        /* don't summon insects if it doesn't think you're around */
        if (!believed_loe && spellnum == CLC_INSECTS)
            return TRUE;
        /* blindness spell on blinded target */
        if ((m_has_property(mdef, BLINDED, ANY_PROPERTY, TRUE) ||
             !haseyes(mdef->data)) &&
            spellnum == CLC_BLIND_YOU)
            return TRUE;
        /* spells that harm master while tame and not conflicted */
        if (magr_tame && !Conflict && spellnum == CLC_CURSE_ITEMS)
            return TRUE;
    }
    return FALSE;
}

/* convert 1..10 to 0..9; add 10 for second group (spell casting) */
#define ad_to_typ(k) (10 + (int)k - 1)

/* monster uses spell (ranged) */
int
buzzmu(struct monst *mtmp, const struct attack *mattk)
{
    /* don't print constant stream of curse messages for 'normal' spellcasting
       monsters at range */
    if (mattk->adtyp > AD_SPC2)
        return 0;

    if (mtmp->mcan) {
        cursetxt(mtmp, FALSE);
        return 0;
    }
    if (lined_up(mtmp) && rn2(3)) {
        action_interrupted();
        if (mattk->adtyp && (mattk->adtyp < 11)) {      /* no cf unsigned >0 */
            if (canseemon(mtmp))
                pline("%s zaps you with a %s!", Monnam(mtmp),
                      flash_types[ad_to_typ(mattk->adtyp)]);
            buzz(-ad_to_typ(mattk->adtyp), (int)mattk->damn, mtmp->mx, mtmp->my,
                 sgn(tbx), sgn(tby));
        }
    }
    return 1;
}

/* return values:
 * 2: target died
 * 1: successful spell
 * 0: unsuccessful spell
 */
int
castmm(struct monst *mtmp, struct monst *mdef, const struct attack *mattk)
{
    int dmg, ml = mtmp->m_lev;
    int ret;
    int spellnum = 0;

    if ((mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) && ml) {
        int cnt = 40;

        do {
            spellnum = rn2(ml);
            if (mattk->adtyp == AD_SPEL)
                spellnum = choose_magic_spell(spellnum);
            else
                spellnum = choose_clerical_spell(spellnum);
            /* not trying to attack? don't allow directed spells */
        } while (--cnt > 0 &&
                 mmspell_would_be_useless(mtmp, mdef, mattk->adtyp, spellnum));
        if (cnt == 0)
            return 0;
    }

    /* monster unable to cast spells? */
    if (mtmp->mcan || mtmp->mspec_used || !ml) {
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my)) {
            if (is_undirected_spell(mattk->adtyp, spellnum))
                pline("%s points all around, then curses.", Monnam(mtmp));
            else
                pline("%s points at %s, then curses.",
                      Monnam(mtmp), mon_nam(mdef));

        } else if ((!(moves % 4) || !rn2(4))) {
            if (canhear())
                pline_once("You hear a mumbled curse.");
        }
        return (0);
    }

    if (mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) {
        mtmp->mspec_used = 10 - mtmp->m_lev;
        if (mtmp->mspec_used < 2)
            mtmp->mspec_used = 2;
    }

    if (rn2(ml * 10) < (mtmp->mconf ? 100 : 20)) {      /* fumbled attack */
        if (canseemon(mtmp) && canhear())
            pline("The air crackles around %s.", mon_nam(mtmp));
        return (0);
    }
    if (canspotmon(mtmp) || canspotmon(mdef)) {
        pline("%s casts a spell%s!",
              /* not canclassifymon; the monster's tail isn't casting */
              canspotmon(mtmp) ? Monnam(mtmp) : "Something",
              is_undirected_spell(mattk->adtyp, spellnum) ? "" :
              msgcat(" at ", mon_nam(mdef)));
    }

    if (mattk->damd)
        dmg = dice((int)((ml / 2) + mattk->damn), (int)mattk->damd);
    else
        dmg = dice((int)((ml / 2) + 1), 6);

    ret = 1;

    switch (mattk->adtyp) {

    case AD_FIRE:
        if (canspotmon(mdef))
            pline("%s is enveloped in flames.", Monnam(mdef));
        if (resists_fire(mdef)) {
            shieldeff(mdef->mx, mdef->my);
            if (canspotmon(mdef))
                pline("But %s resists the effects.", mhe(mdef));
            dmg = 0;
        }
        break;
    case AD_COLD:
        if (canspotmon(mdef))
            pline("%s is covered in frost.", Monnam(mdef));
        if (resists_fire(mdef)) {
            shieldeff(mdef->mx, mdef->my);
            if (canspotmon(mdef))
                pline("But %s resists the effects.", mhe(mdef));
            dmg = 0;
        }
        break;
    case AD_MAGM:
        if (canspotmon(mdef))
            pline("%s is hit by a shower of missiles!", Monnam(mdef));
        if (resists_magm(mdef)) {
            shieldeff(mdef->mx, mdef->my);
            if (canspotmon(mdef))
                pline("The missiles bounce off!");
            dmg = 0;
        } else
            dmg = dice((int)mtmp->m_lev / 2 + 1, 6);
        break;
    case AD_SPEL:      /* wizard spell */
    case AD_CLRC:      /* clerical spell */
        {
            /* aggravation is a special case; it's undirected but should still
               target the victim so as to aggravate you */
            if (is_undirected_spell(mattk->adtyp, spellnum) &&
                (mattk->adtyp != AD_SPEL ||
                 (spellnum != MGC_AGGRAVATION &&
                  spellnum != MGC_SUMMON_MONS))) {
                if (mattk->adtyp == AD_SPEL)
                    cast_wizard_spell(mtmp, dmg, spellnum);
                else
                    cast_cleric_spell(mtmp, dmg, spellnum);
            } else if (mattk->adtyp == AD_SPEL)
                ucast_wizard_spell(mtmp, mdef, dmg, spellnum);
            else
                ucast_cleric_spell(mtmp, mdef, dmg, spellnum);
            dmg = 0;    /* done by the spell casting functions */
            break;
        }
    }
    if (dmg > 0 && !DEADMONSTER(mdef)) {
        mdef->mhp -= dmg;
        if (mdef->mhp < 1)
            monkilled(mdef, "", mattk->adtyp);
    }
    if (DEADMONSTER(mdef))
        return 2;
    return ret;
}

/* return values:
 * 2: target died
 * 1: successful spell
 * 0: unsuccessful spell
 */
int
castum(struct monst *mtmp, const struct attack *mattk)
{
    int dmg, ml = mons[u.umonnum].mlevel;
    int ret;
    int spellnum = 0;
    boolean directed = FALSE;

    /* Three cases: -- monster is attacking you.  Search for a useful spell. --
       monster thinks it's attacking you.  Search for a useful spell, without
       checking for undirected.  If the spell found is directed, it fails with
       cursetxt() and loss of mspec_used. -- monster isn't trying to attack.
       Select a spell once.  Don't keep searching; if that spell is not useful
       (or if it's directed), return and do something else. Since most spells
       are directed, this means that a monster that isn't attacking casts
       spells only a small portion of the time that an attacking monster does.
       */
    if ((mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) && ml) {
        int cnt = 40;

        do {
            spellnum = rn2(ml);
            if (mattk->adtyp == AD_SPEL)
                spellnum = choose_magic_spell(spellnum);
            else
                spellnum = choose_clerical_spell(spellnum);
            /* not trying to attack? don't allow directed spells */
            if (!mtmp || DEADMONSTER(mtmp)) {
                if (is_undirected_spell(mattk->adtyp, spellnum) &&
                    !mmspell_would_be_useless(&youmonst, mtmp,
                                              mattk->adtyp, spellnum)) {
                    break;
                }
            }
        } while (--cnt > 0 &&
                 ((!mtmp && !is_undirected_spell(mattk->adtyp, spellnum))
                  || mmspell_would_be_useless(&youmonst, mtmp,
                                              mattk->adtyp, spellnum)));
        if (cnt == 0) {
            pline("You have no spells to cast right now!");
            return 0;
        }
    }

    if (spellnum == MGC_AGGRAVATION && !mtmp) {
        /* choose a random monster on the level */
        int j = 0, k = 0;

        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
            if (!mtmp->mtame && !mtmp->mpeaceful)
                j++;
        if (j > 0) {
            k = rn2(j);
            for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
                if (!mtmp->mtame && !mtmp->mpeaceful)
                    if (--k < 0)
                        break;
        }
    }

    directed = mtmp && !is_undirected_spell(mattk->adtyp, spellnum);

    /* unable to cast spells? */
    if (u.uen < ml) {
        if (directed)
            pline("You point at %s, then curse.", mon_nam(mtmp));
        else
            pline("You point all around, then curse.");
        return (0);
    }

    if (mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) {
        u.uen -= ml;
    }

    if (rn2(ml * 10) < (Confusion ? 100 : 20)) {        /* fumbled attack */
        pline("The air crackles around you.");
        return (0);
    }

    pline("You cast a spell%s%s!", directed ? " at " : "",
          directed ? mon_nam(mtmp) : "");

/*
 * As these are spells, the damage is related to the level
 * of the monster casting the spell.
 */
    if (mattk->damd)
        dmg = dice((int)((ml / 2) + mattk->damn), (int)mattk->damd);
    else
        dmg = dice((int)((ml / 2) + 1), 6);

    ret = 1;

    switch (mattk->adtyp) {

    case AD_FIRE:
        pline("%s is enveloped in flames.", Monnam(mtmp));
        if (resists_fire(mtmp)) {
            shieldeff(mtmp->mx, mtmp->my);
            pline("But %s resists the effects.", mhe(mtmp));
            dmg = 0;
        }
        break;
    case AD_COLD:
        pline("%s is covered in frost.", Monnam(mtmp));
        if (resists_fire(mtmp)) {
            shieldeff(mtmp->mx, mtmp->my);
            pline("But %s resists the effects.", mhe(mtmp));
            dmg = 0;
        }
        break;
    case AD_MAGM:
        pline("%s is hit by a shower of missiles!", Monnam(mtmp));
        if (resists_magm(mtmp)) {
            shieldeff(mtmp->mx, mtmp->my);
            pline("The missiles bounce off!");
            dmg = 0;
        } else
            dmg = dice((int)ml / 2 + 1, 6);
        break;
    case AD_SPEL:      /* wizard spell */
    case AD_CLRC:      /* clerical spell */
        {
            if (mattk->adtyp == AD_SPEL)
                ucast_wizard_spell(&youmonst, mtmp, dmg, spellnum);
            else
                ucast_cleric_spell(&youmonst, mtmp, dmg, spellnum);
            dmg = 0;    /* done by the spell casting functions */
            break;
        }
    }

    if (dmg > 0 && !DEADMONSTER(mtmp)) {
        mtmp->mhp -= dmg;
        if (mtmp->mhp < 1)
            killed(mtmp);
    }
    if (mtmp && DEADMONSTER(mtmp))
        return 2;

    return ret;
}

extern const int nasties[];

/* monster wizard and cleric spellcasting functions */
/*
   If dmg is zero, then the monster is not casting at you.
   If the monster is intentionally not casting at you, we have previously
   called mmspell_would_be_useless() and spellnum should always be a valid
   undirected spell.
   If you modify either of these, be sure to change is_undirected_spell()
   and mmspell_would_be_useless().
 */
static void
ucast_wizard_spell(struct monst *mattk, struct monst *mtmp, int dmg,
                   int spellnum)
{
    boolean resisted = FALSE;
    boolean yours = (mattk == &youmonst);

    if (dmg == 0 && !is_undirected_spell(AD_SPEL, spellnum)) {
        impossible("cast directed wizard spell (%d) with dmg=0?", spellnum);
        return;
    }

    if (mtmp && DEADMONSTER(mtmp)) {
        impossible("monster already dead?");
        return;
    }

    switch (spellnum) {
    case MGC_DEATH_TOUCH:
        if (!mtmp) {
            impossible("touch of death with no mtmp");
            return;
        }
        if (yours)
            pline("You're using the touch of death!");
        else if (canseemon(mattk)) {
            const char *buf;

            buf = msgprintf("%s%s", mtmp->mtame ? "Oh no, " : "", mhe(mattk));
            if (!mtmp->mtame)
                buf = msgupcasefirst(buf);

            pline("%s's using the touch of death!", buf);
        }

        if (nonliving(mtmp->data) || is_demon(mtmp->data)) {
            if (yours || canseemon(mtmp))
                pline("%s seems no deader than before.", Monnam(mtmp));
        } else if (!(resisted = resist(mtmp, 0, 0, FALSE)) ||
                   rn2(mons[u.umonnum].mlevel) > 12) {
            mtmp->mhp = -1;
            if (yours)
                killed(mtmp);
            else
                monkilled(mtmp, "", AD_SPEL);
            return;
        } else {
            if (resisted)
                shieldeff(mtmp->mx, mtmp->my);
            if (yours || canseemon(mtmp)) {
                if (mtmp->mtame)
                    pline("Lucky for %s, it didn't work!", mon_nam(mtmp));
                else
                    pline("That didn't work...");
            }
        }
        dmg = 0;
        break;
    case MGC_SUMMON_MONS:
        {
            int count = 0;
            struct monst *mpet;

            if (!rn2(10) && Inhell) {
                if (yours)
                    demonpet();
                else
                    msummon(mattk, &mattk->dlevel->z);
            } else {
                int i, j;
                int makeindex, tmp = (u.ulevel > 3) ? u.ulevel / 3 : 1;
                coord bypos;

                if (mtmp)
                    bypos.x = mtmp->mx, bypos.y = mtmp->my;
                else if (yours)
                    bypos.x = u.ux, bypos.y = u.uy;
                else
                    bypos.x = mattk->mx, bypos.y = mattk->my;

                for (i = rnd(tmp); i > 0; --i)
                    for (j = 0; j < 20; j++) {

                        do {
                            makeindex = pick_nasty();
                        } while (attacktype(&mons[makeindex], AT_MAGC) &&
                                 monstr[makeindex] >= monstr[u.umonnum]);
                        if (yours &&
                            !enexto(&bypos, level, u.ux, u.uy,
                                    &mons[makeindex]))
                            continue;
                        if (!yours &&
                            !enexto(&bypos, level, mattk->mx, mattk->my,
                                    &mons[makeindex]))
                            continue;
                        if ((mpet = makemon(&mons[makeindex], level,
                                            bypos.x, bypos.y,
                                            (yours || mattk->mtame) ?
                                            MM_EDOG : NO_MM_FLAGS))) {
                            mpet->msleeping = 0;
                            if (yours || mattk->mtame)
                                /* TODO: We might want to consider taming the
                                   monster, but that has both balance issues,
                                   and (if it's covetous) technical issues. */
                                msethostility(mpet, FALSE, TRUE);
                            else if (mattk->mpeaceful)
                                msethostility(mpet, FALSE, TRUE);
                            else
                                msethostility(mpet, TRUE, TRUE);
                        } else  /* GENOD? */
                            mpet = makemon((struct permonst *)0, level,
                                           bypos.x, bypos.y, NO_MM_FLAGS);
                        if (mpet &&
                            (u.ualign.type == 0 || mpet->data->maligntyp == 0 ||
                             sgn(mpet->data->maligntyp) ==
                             sgn(u.ualign.type))) {
                            count++;
                            break;
                        }
                    }

                const char *mappear =
                    (count == 1) ? "A monster appears" : "Monsters appear";

                if (yours || canseemon(mtmp))
                    pline("%s from nowhere!", mappear);
            }

            dmg = 0;
            break;
        }
    case MGC_AGGRAVATION:
        if (!mtmp) {
            pline("You feel lonely.");
            return;
        }
        you_aggravate(mtmp);
        dmg = 0;
        break;
    case MGC_CURSE_ITEMS:
        if (!mtmp) {
            impossible("curse spell with no mtmp");
            return;
        }
        if (yours || canseemon(mtmp))
            pline("You feel as if %s needs some help.", mon_nam(mtmp));
        mrndcurse(mtmp);
        dmg = 0;
        break;
    case MGC_DESTRY_ARMR:
        if (!mtmp) {
            impossible("destroy spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            if (yours || canseemon(mtmp))
                pline("A field of force surrounds %s!", mon_nam(mtmp));
        } else {
            struct obj *otmp = some_armor(mtmp);

#define oresist_disintegration(obj) \
            (objects[obj->otyp].oc_oprop == DISINT_RES || \
             obj_resists(obj, 0, 90) || is_quest_artifact(obj))

            if (otmp && !oresist_disintegration(otmp)) {
                pline("%s %s %s!", s_suffix(Monnam(mtmp)), xname(otmp),
                      is_cloak(otmp) ? "crumbles and turns to dust" :
                      is_shirt(otmp) ? "crumbles into tiny threads" :
                      is_helmet(otmp) ? "turns to dust and is blown away" :
                      is_gloves(otmp) ? "vanish" : is_boots(otmp) ?
                      "disintegrate" : is_shield(otmp) ? "crumbles away" :
                      "turns to dust");
                obj_extract_self(otmp);
                obfree(otmp, (struct obj *)0);
            } else if (yours || canseemon(mtmp))
                pline("%s looks itchy.", Monnam(mtmp));
        }
        dmg = 0;
        break;
    case MGC_WEAKEN_YOU:       /* drain strength */
        if (!mtmp) {
            impossible("weaken spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            pline("%s looks momentarily weakened.", Monnam(mtmp));
        } else {
            if (yours || canseemon(mtmp))
                pline("%s suddenly seems weaker!", Monnam(mtmp));
            /* monsters don't have strength, so drain max hp instead */
            mtmp->mhpmax -= dmg;
            if (((mtmp->mhp -= dmg)) <= 0) {
                if (yours)
                    killed(mtmp);
                else
                    monkilled(mtmp, "", AD_SPEL);
            }
        }
        dmg = 0;
        break;
    case MGC_DISAPPEAR:        /* makes self invisible */
        if (!yours) {
            impossible("ucast disappear but not yours?");
            return;
        }
        if (!(HInvis & INTRINSIC)) {
            HInvis |= FROMOUTSIDE;
            if (!Blind && !BInvis)
                self_invis_message();
            dmg = 0;
        } else
            impossible("no reason for player to cast disappear spell?");
        break;
    case MGC_STUN_YOU:
        if (!mtmp) {
            impossible("stun spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            if (yours || canseemon(mtmp))
                pline("%s seems momentarily disoriented.", Monnam(mtmp));
        } else {

            if (yours || canseemon(mtmp)) {
                if (mtmp->mstun)
                    pline("%s struggles to keep %s balance.", Monnam(mtmp),
                          mhis(mtmp));
                else
                    pline("%s reels...", Monnam(mtmp));
            }
            mtmp->mstun = 1;
        }
        dmg = 0;
        break;
    case MGC_HASTE_SELF:
        if (!yours) {
            impossible("ucast haste but not yours?");
            return;
        }
        if (!(HFast & INTRINSIC))
            pline("You are suddenly moving faster.");
        HFast |= INTRINSIC;
        dmg = 0;
        break;
    case MGC_CURE_SELF:
        if (!yours)
            impossible("ucast healing but not yours?");
        else if (u.mh < u.mhmax) {
            pline("You feel better.");
            if ((u.mh += dice(3, 6)) > u.mhmax)
                u.mh = u.mhmax;
            dmg = 0;
        }
        break;
    case MGC_PSI_BOLT:
        if (!mtmp) {
            impossible("psibolt spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            dmg = (dmg + 1) / 2;
        }
        if (canseemon(mtmp))
            pline("%s winces%s", Monnam(mtmp), (dmg <= 5) ? "." : "!");
        break;
    default:
        impossible("mcastu: invalid magic spell (%d)", spellnum);
        dmg = 0;
        break;
    }

    if (dmg > 0 && mtmp->mhp > 0) {
        mtmp->mhp -= dmg;
        if (mtmp->mhp < 1) {
            if (yours)
                killed(mtmp);
            else
                monkilled(mtmp, "", AD_SPEL);
        }
    }
}

static void
ucast_cleric_spell(struct monst *mattk, struct monst *mtmp, int dmg,
                   int spellnum)
{
    boolean yours = (mattk == &youmonst);

    if (dmg == 0 && !is_undirected_spell(AD_CLRC, spellnum)) {
        impossible("cast directed cleric spell (%d) with dmg=0?", spellnum);
        return;
    }

    if (mtmp && DEADMONSTER(mtmp)) {
        impossible("monster already dead?");
        return;
    }

    switch (spellnum) {
    case CLC_GEYSER:
        /* this is physical damage, not magical damage */
        if (!mtmp) {
            impossible("geyser spell with no mtmp");
            return;
        }
        if (yours || canseemon(mtmp))
            pline("A sudden geyser slams into %s from nowhere!", mon_nam(mtmp));
        dmg = dice(8, 6);
        break;
    case CLC_FIRE_PILLAR:
        if (!mtmp) {
            impossible("firepillar spell with no mtmp");
            return;
        }
        if (yours || canseemon(mtmp))
            pline("A pillar of fire strikes all around %s!", mon_nam(mtmp));
        if (resists_fire(mtmp)) {
            shieldeff(mtmp->mx, mtmp->my);
            dmg = 0;
        } else
            dmg = dice(8, 6);
        (void)burnarmor(mtmp);
        destroy_mitem(mtmp, SCROLL_CLASS, AD_FIRE);
        destroy_mitem(mtmp, POTION_CLASS, AD_FIRE);
        destroy_mitem(mtmp, SPBOOK_CLASS, AD_FIRE);
        (void)burn_floor_paper(mtmp->dlevel, mtmp->mx, mtmp->my, TRUE, FALSE);
        break;
    case CLC_LIGHTNING:
        {
            boolean reflects;

            if (!mtmp) {
                impossible("lightning spell with no mtmp");
                return;
            }

            if (yours || canseemon(mtmp))
                pline("A bolt of lightning strikes down at %s from above!",
                      mon_nam(mtmp));
            reflects = mon_reflects(mtmp, "It bounces off %s %s.");
            if (reflects || resists_elec(mtmp)) {
                shieldeff(u.ux, u.uy);
                dmg = 0;
            } else
                dmg = dice(8, 6);
            if (!reflects) {
                destroy_mitem(mtmp, WAND_CLASS, AD_ELEC);
                destroy_mitem(mtmp, RING_CLASS, AD_ELEC);
            }
            if (!resists_blnd(mtmp)) {
                unsigned rnd_tmp = rnd(50);
                mtmp->mcansee = 0;
                if((mtmp->mblinded + rnd_tmp) > 127)
                    mtmp->mblinded = 127;
                else
                    mtmp->mblinded += rnd_tmp;
            }
            break;
        }
    case CLC_CURSE_ITEMS:
        if (!mtmp) {
            impossible("curse spell with no mtmp");
            return;
        }
        if (yours || canseemon(mtmp))
            pline("You feel as if %s needs some help.", mon_nam(mtmp));
        mrndcurse(mtmp);
        dmg = 0;
        break;
    case CLC_INSECTS:
        {
            /* Try for insects, and if there are none left, go for (sticks to)
               snakes.  -3. */
            const struct permonst *pm = mkclass(&u.uz, S_ANT, 0, rng_main);
            struct monst *mtmp2 = (struct monst *)0;
            char let = (pm ? S_ANT : S_SNAKE);
            boolean success;
            int i;
            coord bypos;
            int quan;

            if (!mtmp) {
                impossible("insect spell with no mtmp");
                return;
            }

            quan = (mons[u.umonnum].mlevel < 2) ? 1 :
                rnd(mons[u.umonnum].mlevel / 2);
            if (quan < 3)
                quan = 3;
            success = pm ? TRUE : FALSE;
            for (i = 0; i <= quan; i++) {
                if (!enexto(&bypos, level, mtmp->mx, mtmp->my, mtmp->data))
                    break;
                if ((pm = mkclass(&u.uz, let, 0, rng_main)) != 0 &&
                    ((mtmp2 = makemon(pm, level, bypos.x, bypos.y,
                                      MM_CREATEMONSTER | MM_CMONSTER_M)))) {
                    success = TRUE;
                    mtmp2->msleeping = 0;
                    if (yours || mattk->mtame)
                        (void)tamedog(mtmp2, (struct obj *)0);
                    else
                        msethostility(mtmp2, !mattk->mpeaceful, TRUE);
                }
            }

            if (yours) {
                if (!success)
                    pline
                        ("You cast at a clump of sticks, but nothing happens.");
                else if (let == S_SNAKE)
                    pline("You transform a clump of sticks into snakes!");
                else
                    pline("You summon insects!");
            } else if (canseemon(mtmp)) {
                if (!success)
                    pline("%s casts at a clump of sticks, but nothing happens.",
                          Monnam(mattk));
                else if (let == S_SNAKE)
                    pline("%s transforms a clump of sticks into snakes!",
                          Monnam(mattk));
                else
                    pline("%s summons insects!", Monnam(mattk));
            }
            dmg = 0;
            break;
        }
    case CLC_BLIND_YOU:
        if (!mtmp) {
            impossible("blindness spell with no mtmp");
            return;
        }
        /* note: resists_blnd() doesn't apply here */
        if (!mtmp->mblinded && haseyes(mtmp->data)) {
            if (!resists_blnd(mtmp)) {
                int num_eyes = eyecount(mtmp->data);

                pline("Scales cover %s %s!", s_suffix(mon_nam(mtmp)),
                      (num_eyes == 1) ? "eye" : "eyes");

                mtmp->mblinded = 127;
            }
            dmg = 0;

        } else
            impossible("no reason for monster to cast blindness spell?");
        break;
    case CLC_PARALYZE:
        if (!mtmp) {
            impossible("paralysis spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            if (yours || canseemon(mtmp))
                pline("%s stiffens briefly.", Monnam(mtmp));
        } else {
            if (yours || canseemon(mtmp))
                pline("%s is frozen in place!", Monnam(mtmp));
            dmg = 4 + mons[u.umonnum].mlevel;
            mtmp->mcanmove = 0;
            mtmp->mfrozen = dmg;
        }
        dmg = 0;
        break;
    case CLC_CONFUSE_YOU:
        if (!mtmp) {
            impossible("confusion spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            if (yours || canseemon(mtmp))
                pline("%s seems momentarily dizzy.", Monnam(mtmp));
        } else {
            if (yours || canseemon(mtmp))
                pline("%s seems %sconfused!", Monnam(mtmp),
                      mtmp->mconf ? "more " : "");
            mtmp->mconf = 1;
        }
        dmg = 0;
        break;
    case CLC_CURE_SELF:
        if (u.mh < u.mhmax) {
            pline("You feel better.");
            /* note: player healing does 6d4; this used to do 1d8 */
            if ((u.mh += dice(3, 6)) > u.mhmax)
                u.mh = u.mhmax;
        }
        dmg = 0;
        break;
    case CLC_OPEN_WOUNDS:
        if (!mtmp) {
            impossible("wound spell with no mtmp");
            return;
        }
        if (resist(mtmp, 0, 0, FALSE)) {
            shieldeff(mtmp->mx, mtmp->my);
            dmg = (dmg + 1) / 2;
        }
        /* not canseemon; if you can't see it you don't know it was wounded
           TODO: This seems suspicious despite the comment, perhaps it should
           check canseemon but not yours? */
        if (yours) {
            if (dmg <= 5)
                pline("%s looks itchy!", Monnam(mtmp));
            else if (dmg <= 10)
                pline("Wounds appear on %s!", mon_nam(mtmp));
            else if (dmg <= 20)
                pline("Severe wounds appear on %s!", mon_nam(mtmp));
            else
                pline("%s is covered in wounds!", Monnam(mtmp));
        }
        break;
    default:
        impossible("mcastu: invalid clerical spell (%d)", spellnum);
        dmg = 0;
        break;
    }

    if (dmg > 0 && mtmp->mhp > 0) {
        mtmp->mhp -= dmg;
        if (mtmp->mhp < 1) {
            if (yours)
                killed(mtmp);
            else
                monkilled(mtmp, "", AD_CLRC);
        }
    }
}


/*mcastu.c*/
