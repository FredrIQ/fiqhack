/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-04 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef ARTINAMES_H
/* in artinames.h, all we care about is the list of names */

# define A(nam,typ,s1,s2,mt,atk,dfn,cry,inv,al,cl,rac,cost) nam

static const char *artifact_names[] = {
#else
/* in artilist.h, set up the actual artifact list structure */

# include "artifact.h"
# include "align.h"
# include "permonst.h"
# include "monsym.h"
# include "onames.h"
# include "prop.h"
# include "pm.h"

# define A(nam,typ,s1,s2,mt,atk,dfn,cry,inv,al,cl,rac,cost) \
 { nam, cost, s1, s2, mt, atk, dfn, cry, al, inv, cl, rac, typ }

# define     NO_ATTK    {0,0,0,0}       /* no attack */
# define     NO_DFNS    {0,0,0,0}       /* no defense */
# define     NO_CARY    {0,0,0,0}       /* no carry effects */
# define     DFNS(c)    {0,c,0,0}
# define     CARY(c)    {0,c,0,0}
# define     PHYS(a,b)  {0,AD_PHYS,a,b} /* physical */
# define     DRLI(a,b)  {0,AD_DRLI,a,b} /* life drain */
# define     COLD(a,b)  {0,AD_COLD,a,b}
# define     FIRE(a,b)  {0,AD_FIRE,a,b}
# define     ELEC(a,b)  {0,AD_ELEC,a,b} /* electrical shock */
# define     STUN(a,b)  {0,AD_STUN,a,b} /* magical attack */

static const struct artifact const_artilist[] = {
#endif /* ARTINAMES_C */

/* Artifact cost rationale:
 * 1.  The more useful the artifact, the better its cost.
 * 2.  Quest artifacts are highly valued.
 * 3.  Chaotic artifacts are inflated due to scarcity (and balance).
 */

/* Note: If you ever add an artifact that can be equipped in an armor slot and
   also has an effect that toggles when invoking, you need to update setequip in
   do_wear.c to add behaviour for what happens when it is unequipped while
   invoked. (This combination should probably be avoided, because setequip does
   not currently have information about whether the artifact is about to be
   destroyed or not.) */

/*  dummy element #0, so that all interesting indices are non-zero */
    A("", STRANGE_OBJECT,
      0, 0, 0, NO_ATTK, NO_DFNS, NO_CARY, 0, A_NONE, NON_PM, NON_PM, 0L),

/*
  The bonuses of artifacts are defined partly by the list in this file. This is
  an array of struct artifact structures. This structure has a struct attack
  attk field whose members are used in a way completely unlike how struct attack
  is used for monster descriptions. In this struct attack, the adtyp field tells
  both what type of elemental damage the bonus damage deals, and how a monster
  can resist the bonus damage; the damn field gives the to-hit bonus, and the
  damd field gives the damage bonus.

  The to-hit bonus applies against any monsters, and is 1dX where X is the
  previously mentioned damn field.

  The damage bonus applies only to particular monsters, and either adds 1dY
  extra damage where Y is the damd field, or doubles the normal damage that
  would be dealt by the weapon (if the damd field is zero). This is handled in
  artifact.c:artifact_hit. There's special handling for artifacts that drain
  life or behead or bisect.

  Magicbane gets extra special bonuses in addition to the normal damage bonus
  code path. This handles both the effects and the extra damage for the scare,
  confuse, stun, cancel, probe bonuses.
 */

    A("Excalibur", LONG_SWORD,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_DEFN | SPFX_INTEL), 0, 0,
      PHYS(5, 10), DRLI(0, 0), NO_CARY, 0, A_LAWFUL, PM_KNIGHT, NON_PM, 4000L),
/*
 *      Stormbringer only has a 2 because it can drain a level,
 *      providing 8 more.
 */
    A("Stormbringer", RUNESWORD,
      (SPFX_RESTR | SPFX_ATTK | SPFX_DEFN | SPFX_INTEL | SPFX_DRLI), 0, 0,
      DRLI(5, 2), DRLI(0, 0), NO_CARY, 0, A_CHAOTIC, NON_PM, NON_PM, 8000L),
/*
 *      Mjollnir will return to the hand of the wielder when thrown
 *      if the wielder is a Valkyrie wearing Gauntlets of Power.
 */
    A("Mjollnir", WAR_HAMMER,   /* Mjo:llnir */
      (SPFX_RESTR | SPFX_ATTK), 0, 0,
      ELEC(5, 24), NO_DFNS, NO_CARY, 0, A_NEUTRAL, PM_VALKYRIE, NON_PM, 4000L),

    A("Cleaver", BATTLE_AXE,
      SPFX_RESTR, 0, 0,
      PHYS(3, 6), NO_DFNS, NO_CARY, 0, A_NEUTRAL, PM_BARBARIAN, NON_PM, 1500L),

    A("Grimtooth", ORCISH_DAGGER,
      SPFX_RESTR, 0, 0,
      PHYS(2, 6), NO_DFNS, NO_CARY, 0, A_CHAOTIC, NON_PM, PM_ORC, 300L),
/*
 *      Orcrist and Sting have same alignment as elves.
 */
    A("Orcrist", ELVEN_BROADSWORD,
      SPFX_DFLAG2, 0, M2_ORC,
      PHYS(5, 0), NO_DFNS, NO_CARY, 0, A_CHAOTIC, NON_PM, PM_ELF, 2000L),

/*
 *      The combination of SPFX_WARN and M2_something on an artifact
 *      will trigger EWarn_of_mon for all monsters that have the appropriate
 *      M2_something flags.  In Sting's case it will trigger EWarn_of_mon
 *      for M2_ORC monsters.
 */
    A("Sting", ELVEN_DAGGER,
      (SPFX_WARN | SPFX_DFLAG2), 0, M2_ORC,
      PHYS(5, 0), NO_DFNS, NO_CARY, 0, A_CHAOTIC, NON_PM, PM_ELF, 800L),
/*
 *      Magicbane is a bit different!  Its magic fanfare
 *      unbalances victims in addition to doing some damage.
 */
    A("Magicbane", QUARTERSTAFF,
      (SPFX_RESTR | SPFX_ATTK | SPFX_DEFN), 0, 0,
      STUN(3, 4), DFNS(AD_MAGM), NO_CARY, 0, A_NEUTRAL, PM_WIZARD, NON_PM,
      3500L),

    A("Frost Brand", LONG_SWORD,
      (SPFX_RESTR | SPFX_ATTK | SPFX_DEFN), 0, 0,
      COLD(5, 0), COLD(0, 0), NO_CARY, 0, A_NONE, NON_PM, NON_PM, 3000L),

    A("Fire Brand", LONG_SWORD,
      (SPFX_RESTR | SPFX_ATTK | SPFX_DEFN), 0, 0,
      FIRE(5, 0), FIRE(0, 0), NO_CARY, 0, A_NONE, NON_PM, NON_PM, 3000L),

    A("Dragonbane", LANCE,
      (SPFX_RESTR | SPFX_REFLECT | SPFX_DCLAS), 0, S_DRAGON,
      PHYS(5, 20), NO_DFNS, NO_CARY, 0, A_NONE, NON_PM, NON_PM, 500L),

    A("Demonbane", LONG_SWORD,
      (SPFX_RESTR | SPFX_DFLAG2), 0, M2_DEMON,
      PHYS(5, 20), NO_DFNS, NO_CARY, 0, A_LAWFUL, NON_PM, NON_PM, 2500L),

    A("Werebane", SILVER_SABER,
      (SPFX_RESTR | SPFX_DFLAG2), 0, M2_WERE,
      PHYS(5, 20), DFNS(AD_WERE), NO_CARY, 0, A_NONE, NON_PM, NON_PM, 1500L),

    A("Grayswandir", SILVER_SABER,
      (SPFX_RESTR | SPFX_HALRES), 0, 0,
      PHYS(5, 0), NO_DFNS, NO_CARY, 0, A_LAWFUL, NON_PM, NON_PM, 8000L),

    A("Giantslayer", LONG_SWORD,
      (SPFX_RESTR | SPFX_DFLAG2), 0, M2_GIANT,
      PHYS(5, 20), NO_DFNS, NO_CARY, 0, A_NEUTRAL, NON_PM, NON_PM, 200L),

    A("Ogresmasher", WAR_HAMMER,
      (SPFX_RESTR | SPFX_DCLAS), 0, S_OGRE,
      PHYS(5, 20), NO_DFNS, NO_CARY, 0, A_NONE, NON_PM, NON_PM, 200L),

    A("Trollsbane", MORNING_STAR,
      (SPFX_RESTR | SPFX_REGEN | SPFX_DCLAS), 0, S_TROLL,
      PHYS(5, 20), NO_DFNS, NO_CARY, 0, A_NONE, NON_PM, NON_PM, 200L),
/*
 *      Two problems:  1) doesn't let trolls regenerate heads,
 *      2) doesn't give unusual message for 2-headed monsters (but
 *      allowing those at all causes more problems than worth the effort).
 */
    A("Vorpal Blade", LONG_SWORD,
      (SPFX_RESTR | SPFX_BEHEAD), 0, 0,
      PHYS(5, 1), NO_DFNS, NO_CARY, 0, A_NEUTRAL, NON_PM, NON_PM, 4000L),
/*
 *      Ah, never shall I forget the cry,
 *              or the shriek that shrieked he,
 *      As I gnashed my teeth, and from my sheath
 *              I drew my Snickersnee!
 *                      --Koko, Lord high executioner of Titipu
 *                        (From Sir W.S. Gilbert's "The Mikado")
 */
    A("Snickersnee", KATANA,
      SPFX_RESTR, 0, 0,
      PHYS(0, 8), NO_DFNS, NO_CARY, 0, A_LAWFUL, PM_SAMURAI, NON_PM, 1200L),

    A("Sunsword", LONG_SWORD,
      (SPFX_RESTR | SPFX_DFLAG2), 0, M2_UNDEAD,
      PHYS(5, 20), DFNS(AD_BLND), NO_CARY, 0, A_LAWFUL, NON_PM, NON_PM, 1500L),

/*
 *      The artifacts for the quest dungeon, all self-willed.
 */

    A("The Orb of Detection", CRYSTAL_BALL,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL), (SPFX_ESP | SPFX_HSPDAM), 0,
      NO_ATTK, NO_DFNS, CARY(AD_MAGM),
      INVIS, A_LAWFUL, PM_ARCHEOLOGIST, NON_PM, 2500L),

    A("The Heart of Ahriman", LUCKSTONE,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL), SPFX_STLTH, 0,
      /* this stone does double damage if used as a projectile weapon */
      PHYS(5, 0), NO_DFNS, NO_CARY,
      LEVITATION, A_NEUTRAL, PM_BARBARIAN, NON_PM, 2500L),

    A("The Sceptre of Might", MACE,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_DALIGN), 0, 0,
      PHYS(5, 0), NO_DFNS, CARY(AD_MAGM),
      CONFLICT, A_LAWFUL, PM_CAVEMAN, NON_PM, 2500L),

    A("The Staff of Aesculapius", QUARTERSTAFF,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_ATTK | SPFX_INTEL | SPFX_DRLI |
       SPFX_REGEN), 0, 0,
      DRLI(0, 0), DRLI(0, 0), NO_CARY,
      HEALING, A_NEUTRAL, PM_HEALER, NON_PM, 5000L),

    A("The Magic Mirror of Merlin", MIRROR,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_SPEAK), SPFX_ESP, 0,
      NO_ATTK, NO_DFNS, CARY(AD_MAGM),
      0, A_LAWFUL, PM_KNIGHT, NON_PM, 1500L),

    A("The Eyes of the Overworld", LENSES,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_XRAY), 0, 0,
      NO_ATTK, NO_DFNS, CARY(AD_MAGM),
      ENLIGHTENING, A_NEUTRAL, PM_MONK, NON_PM, 2500L),

    A("The Mitre of Holiness", HELM_OF_BRILLIANCE,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_DFLAG2 | SPFX_INTEL), 0, M2_UNDEAD,
      NO_ATTK, NO_DFNS, CARY(AD_FIRE),
      ENERGY_BOOST, A_LAWFUL, PM_PRIEST, NON_PM, 2000L),

    A("The Longbow of Diana", BOW,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_REFLECT), SPFX_ESP, 0,
      PHYS(5, 0), NO_DFNS, NO_CARY,
      CREATE_AMMO, A_CHAOTIC, PM_RANGER, NON_PM, 4000L),

    A("The Master Key of Thievery", SKELETON_KEY,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_SPEAK),
      (SPFX_WARN | SPFX_TCTRL | SPFX_HPHDAM), 0,
      NO_ATTK, NO_DFNS, NO_CARY,
      UNTRAP, A_CHAOTIC, PM_ROGUE, NON_PM, 3500L),

    A("The Tsurugi of Muramasa", TSURUGI,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_BEHEAD | SPFX_LUCK), 0, 0,
      PHYS(0, 0), NO_DFNS, NO_CARY,
      SELF_UNCURSE, A_LAWFUL, PM_SAMURAI, NON_PM, 4500L),

    A("The Platinum Yendorian Express Card", CREDIT_CARD,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_DEFN),
      (SPFX_ESP | SPFX_HSPDAM), 0,
      NO_ATTK, NO_DFNS, CARY(AD_MAGM),
      CHARGE_OBJ, A_NEUTRAL, PM_TOURIST, NON_PM, 7000L),

    A("The Orb of Fate", CRYSTAL_BALL,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL | SPFX_LUCK),
      (SPFX_WARN | SPFX_HSPDAM | SPFX_HPHDAM), 0,
      NO_ATTK, NO_DFNS, NO_CARY,
      LEV_TELE, A_NEUTRAL, PM_VALKYRIE, NON_PM, 3500L),

    A("The Eye of the Aethiopica", AMULET_OF_ESP,
      (SPFX_NOGEN | SPFX_RESTR | SPFX_INTEL), (SPFX_EREGEN | SPFX_HSPDAM), 0,
      NO_ATTK, NO_DFNS, CARY(AD_MAGM),
      CREATE_PORTAL, A_NEUTRAL, PM_WIZARD, NON_PM, 4000L),

/*
 *  terminator; otyp must be zero
 */
    A(0, 0, 0, 0, 0, NO_ATTK, NO_DFNS, NO_CARY, 0, A_NONE, NON_PM, NON_PM, 0L)

};      /* artilist[] (or artifact_names[]) */

#undef  A

#ifndef ARTINAMES_H
# undef NO_ATTK
# undef NO_DFNS
# undef DFNS
# undef PHYS
# undef DRLI
# undef COLD
# undef FIRE
# undef ELEC
# undef STUN
#endif

/*artilist.h*/

