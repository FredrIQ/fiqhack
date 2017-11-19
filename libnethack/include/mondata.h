/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-19 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MONDATA_H
# define MONDATA_H

# define verysmall(ptr)         ((ptr)->msize < MZ_SMALL)
# define bigmonst(ptr)          ((ptr)->msize >= MZ_LARGE)

# define pm_resistance(ptr,typ) (((ptr)->mresists & (typ)) != 0)

# define has_property(mon,prop) (m_has_property(mon, prop, ANY_PROPERTY, FALSE))
# define bhas_property(mon,prop) (m_has_property(mon, prop, ANY_PROPERTY, TRUE) & \
                                  W_MASK(os_blocked))
# define ihas_property(mon,prop) (has_property(mon, prop) & INTRINSIC)
# define ehas_property(mon,prop) (has_property(mon, prop) & EXTRINSIC)

/* Any properties */
# define resists_fire(mon)      (has_property(mon, FIRE_RES))
# define resists_cold(mon)      (has_property(mon, COLD_RES))
# define resists_sleep(mon)     (has_property(mon, SLEEP_RES))
# define resists_disint(mon)    (has_property(mon, DISINT_RES))
# define resists_elec(mon)      (has_property(mon, SHOCK_RES))
# define resists_poison(mon)    (has_property(mon, POISON_RES))
# define resists_acid(mon)      (has_property(mon, ACID_RES))
# define resists_ston(mon)      (has_property(mon, STONE_RES))
# define resists_drli(mon)      (has_property(mon, DRAIN_RES))
# define resists_magm(mon)      (has_property(mon, ANTIMAGIC))
# define resists_sick(mon)      (has_property(mon, SICK_RES))
# define resists_hallu(mon)     (has_property(mon, HALLUC_RES))
# define resists_stun(mon)      (has_property(mon, STUN_RES))
# define resists_death(mon)     (has_property(mon, DEATH_RES))
# define immune_to_fire(mon)    (has_immunity(mon, FIRE_RES))
# define immune_to_cold(mon)    (has_immunity(mon, COLD_RES))
# define immune_to_sleep(mon)   (has_immunity(mon, SLEEP_RES))
# define immune_to_elec(mon)    (has_immunity(mon, SHOCK_RES))
# define immune_to_poison(mon)  (has_immunity(mon, POISON_RES))
# define immune_to_acid(mon)    (has_immunity(mon, ACID_RES))
# define reflecting(mon)        (has_property(mon, REFLECTING))
# define half_spell_dam(mon)    (has_property(mon, HALF_SPDAM))
# define half_phys_dam(mon)     (has_property(mon, HALF_PHDAM))
# define displacement(mon)      (has_property(mon, DISPLACED))
/* displaced: displacement is active right now */
# define displaced(mon)         (mon->dx != COLNO)
# define adorned(mon)           (has_property(mon, ADORNED))
# define regenerates(mon)       (has_property(mon, REGENERATION))
# define pw_regenerates(mon)    (has_property(mon, ENERGY_REGENERATION))
# define searching(mon)         (has_property(mon, SEARCHING))
# define see_invisible(mon)     (has_property(mon, SEE_INVIS))
# define invisible(mon)         (has_property(mon, INVIS))
/* binvisible: invisibility blocked */
# define binvisible(mon)        (bhas_property(mon, INVIS))
# define teleportitis(mon)      (has_property(mon, TELEPORT))
# define teleport_control(mon)  (has_property(mon, TELEPORT_CONTROL))
# define polymorphitis(mon)     (has_property(mon, POLYMORPH))
# define polymorph_control(mon) (has_property(mon, POLYMORPH_CONTROL))
# define levitates(mon)         (has_property(mon, LEVITATION))
# define stealthy(mon)          (has_property(mon, STEALTH))
# define aggravating(mon)       (has_property(mon, AGGRAVATE_MONSTER))
# define conflicting(mon)       (has_property(mon, CONFLICT))
# define protected(mon)         (has_property(mon, PROTECTION))
# define shapeshift_prot(mon)   (has_property(mon, PROT_FROM_SHAPE_CHANGERS))
# define warned(mon)            (has_property(mon, WARNING))
# define telepathic(mon)        (has_property(mon, TELEPAT))
# define fast(mon)              (has_property(mon, FAST))
/* ifast: whether or not you have intrinsic fast in particular */
# define ifast(mon)             (ihas_property(mon, FAST))
# define very_fast(mon)         (ehas_property(mon, FAST))
# define stunned(mon)           (has_property(mon, STUNNED))
# define confused(mon)          (has_property(mon, CONFUSION))
# define sick(mon)              (has_property(mon, SICK))
# define blind(mon)             (has_property(mon, BLINDED))
# define restful_sleep(mon)     (has_property(mon, SLEEPING))
# define leg_hurtl(mon)         (has_property(mon, LWOUNDED_LEGS))
# define leg_hurtr(mon)         (has_property(mon, RWOUNDED_LEGS))
# define leg_hurt(mon)          (leg_hurtl(mon) | leg_hurtr(mon))
# define leg_hurtsides(mon)     ((leg_hurtl(mon) ? LEFT_SIDE : 0) | \
                                 (leg_hurtr(mon) ? RIGHT_SIDE : 0))
# define petrifying(mon)        (has_property(mon, STONED))
# define strangled(mon)         (has_property(mon, STRANGLED))
# define hallucinating(mon)     (has_property(mon, HALLUC))
# define fumbling(mon)          (has_property(mon, FUMBLING))
# define jumps(mon)             (has_property(mon, JUMPING))
# define waterwalks(mon)        (has_property(mon, WWALKING))
# define hunger(mon)            (has_property(mon, HUNGER))
# define slippery_fingers(mon)  (has_property(mon, GLIB))
# define will_be_lifesaved(mon) (has_property(mon, LIFESAVED))
# define clairvoyant(mon)       (has_property(mon, CLAIRVOYANT))
/* bclairvoyant -- clairvoance blocked */
# define bclairvoyant(mon)      (bhas_property(mon, CLAIRVOYANT))
# define vomiting(mon)          (has_property(mon, VOMITING))
# define unbreathing(mon)       (has_property(mon, MAGICAL_BREATHING))
# define warned_of_undead(mon)  (has_property(mon, WARN_UNDEAD))
# define cancelled(mon)         (has_property(mon, CANCELLED))
# define free_action(mon)       (has_property(mon, FREE_ACTION))
# define swims(mon)             (has_property(mon, SWIMMING))
# define sliming(mon)           (has_property(mon, SLIMED))
# define fixed_abilities(mon)   (has_property(mon, FIXED_ABIL))
# define flying(mon)            (has_property(mon, FLYING))
# define unchanging(mon)        (has_property(mon, UNCHANGING))
# define phasing(mon)           (has_property(mon, PASSES_WALLS))
# define slow_digestion(mon)    (has_property(mon, SLOW_DIGESTION))
# define infravision(mon)       (has_property(mon, INFRAVISION))
# define warned_of_mon(mon)     (has_property(mon, WARN_OF_MON))
# define astral_vision(mon)     (has_property(mon, XRAY_VISION))
# define detects_monsters(mon)  (has_property(mon, DETECT_MONSTERS))
# define slow(mon)              (has_property(mon, SLOW))
# define zombifying(mon)        (property_timeout(mon, ZOMBIE))
/* living but under the power of undead (and immune to zombification) */
# define izombie(mon)           (ihas_property(mon, ZOMBIE))
# define waterproof(mon)        (has_property(mon, WATERPROOF))

# define hitbon(mon)            (mon_bon(mon, RIN_INCREASE_ACCURACY, (mon)->mhitinc))
# define dambon(mon)            (mon_bon(mon, RIN_INCREASE_DAMAGE, (mon)->mdaminc))
# define protbon(mon)           (mon_bon(mon, RIN_PROTECTION, (mon)->mac))

# define is_lminion(mon)        (pm_isminion((mon)->data) &&    \
                                 malign(mon) == A_LAWFUL)

# define is_flyer(ptr)          (((ptr)->mflags1 & M1_FLY) != 0L)
# define is_floater(ptr)        ((ptr)->mlet == S_EYE)
# define is_clinger(ptr)        (((ptr)->mflags1 & M1_CLING) != 0L)
# define pm_swims(ptr)        (((ptr)->mflags1 & M1_SWIM) != 0L)
# define breathless(ptr)        (((ptr)->mflags1 & M1_BREATHLESS) != 0L)
# define can_blow_instrument(ptr) \
                                (!(breathless(ptr) && \
                                   (ptr->msound == MS_SILENT || \
                                    ptr->msound == MS_BONES)))
# define amphibious(ptr) \
    (((ptr)->mflags1 & (M1_AMPHIBIOUS | M1_BREATHLESS)) != 0L)
# define pm_phasing(ptr)        (((ptr)->mflags1 & M1_WALLWALK) != 0L)
# define amorphous(ptr)         (((ptr)->mflags1 & M1_AMORPHOUS) != 0L)
# define noncorporeal(ptr)      ((ptr) == &mons[PM_GHOST] || \
                                 (ptr) == &mons[PM_SHADE])
# define tunnels(ptr)           (((ptr)->mflags1 & M1_TUNNEL) != 0L)
# define needspick(ptr)         (((ptr)->mflags1 & M1_NEEDPICK) != 0L)
# define hides_under(ptr)       (((ptr)->mflags1 & M1_CONCEAL) != 0L)
# define is_hider(ptr)          (((ptr)->mflags1 & M1_HIDE) != 0L)
# define haseyes(ptr)           (((ptr)->mflags1 & M1_NOEYES) == 0L)
# define eyecount(ptr)          (!haseyes(ptr) ? 0 : \
                                 ((ptr) == &mons[PM_CYCLOPS] || \
                                  (ptr) == &mons[PM_FLOATING_EYE]) ? 1 : 2)
# define nohands(ptr)           (((ptr)->mflags1 & M1_NOHANDS) != 0L)
# define nolimbs(ptr)           (((ptr)->mflags1 & M1_NOLIMBS) == M1_NOLIMBS)
# define notake(ptr)            (((ptr)->mflags1 & M1_NOTAKE) != 0L)
# define has_head(ptr)          (((ptr)->mflags1 & M1_NOHEAD) == 0L)
# define has_horns(ptr)         (num_horns(ptr) > 0)
# define is_whirly(ptr)         ((ptr)->mlet == S_VORTEX || \
                                 (ptr) == &mons[PM_AIR_ELEMENTAL])
# define flaming(ptr)           ((ptr) == &mons[PM_FIRE_VORTEX] || \
                                 (ptr) == &mons[PM_FLAMING_SPHERE] || \
                                 (ptr) == &mons[PM_FIRE_ELEMENTAL] || \
                                 (ptr) == &mons[PM_SALAMANDER])
# define is_silent(ptr)         ((ptr)->msound == MS_SILENT)
# define unsolid(ptr)           (((ptr)->mflags1 & M1_UNSOLID) != 0L)
# define mindless(ptr)          (((ptr)->mflags1 & M1_MINDLESS) != 0L)
# define humanoid(ptr)          (((ptr)->mflags1 & M1_HUMANOID) != 0L)
# define is_animal(ptr)         (((ptr)->mflags1 & M1_ANIMAL) != 0L)
# define slithy(ptr)            (((ptr)->mflags1 & M1_SLITHY) != 0L)
# define is_wooden(ptr)         ((ptr) == &mons[PM_WOOD_GOLEM])
# define thick_skinned(ptr)     (((ptr)->mflags1 & M1_THICK_HIDE) != 0L)
# define lays_eggs(ptr)         (((ptr)->mflags1 & M1_OVIPAROUS) != 0L)
# define is_armed(ptr)          attacktype(ptr, AT_WEAP)
# define acidic(ptr)            (((ptr)->mflags1 & M1_ACID) != 0L)
# define poisonous(ptr)         (((ptr)->mflags1 & M1_POIS) != 0L)
# define carnivorous(ptr)       (((ptr)->mflags1 & M1_CARNIVORE) != 0L)
# define herbivorous(ptr)       (((ptr)->mflags1 & M1_HERBIVORE) != 0L)
# define metallivorous(ptr)     (((ptr)->mflags1 & M1_METALLIVORE) != 0L)
# define polyok(ptr)            (((ptr)->mflags2 & M2_NOPOLY) == 0L)
# define is_undead(ptr)         (((ptr)->mflags2 & M2_UNDEAD) != 0L)
# define is_were(ptr)           (((ptr)->mflags2 & M2_WERE) != 0L)
# define is_elf(ptr)            (((ptr)->mflags2 & M2_ELF) != 0L)
# define is_dwarf(ptr)          (((ptr)->mflags2 & M2_DWARF) != 0L)
# define is_gnome(ptr)          (((ptr)->mflags2 & M2_GNOME) != 0L)
# define is_orc(ptr)            (((ptr)->mflags2 & M2_ORC) != 0L)
# define is_human(ptr)          (((ptr)->mflags2 & M2_HUMAN) != 0L)
# define your_race(ptr)         (((ptr)->mflags2 & urace.selfmask) != 0L)
# define is_bat(ptr)            ((ptr) == &mons[PM_BAT] || \
                                 (ptr) == &mons[PM_GIANT_BAT] || \
                                 (ptr) == &mons[PM_VAMPIRE_BAT])
# define is_bird(ptr)           ((ptr)->mlet == S_BAT && !is_bat(ptr))
# define is_giant(ptr)          (((ptr)->mflags2 & M2_GIANT) != 0L)
# define is_golem(ptr)          ((ptr)->mlet == S_GOLEM)
# define is_domestic(ptr)       (((ptr)->mflags2 & M2_DOMESTIC) != 0L)
# define is_demon(ptr)          (((ptr)->mflags2 & M2_DEMON) != 0L)
# define is_mercenary(ptr)      (((ptr)->mflags2 & M2_MERC) != 0L)
# define is_male(ptr)           (((ptr)->mflags2 & M2_MALE) != 0L)
# define is_female(ptr)         (((ptr)->mflags2 & M2_FEMALE) != 0L)
# define is_neuter(ptr)         (((ptr)->mflags2 & M2_NEUTER) != 0L)
# define is_wanderer(ptr)       (((ptr)->mflags2 & M2_WANDER) != 0L)
# define always_hostile(ptr)    (((ptr)->mflags2 & M2_HOSTILE) != 0L)
# define always_peaceful(ptr)   (((ptr)->mflags2 & M2_PEACEFUL) != 0L)
# define race_hostile(ptr)      (((ptr)->mflags2 & urace.hatemask) != 0L)
# define race_peaceful(ptr)     (((ptr)->mflags2 & urace.lovemask) != 0L)
# define extra_nasty(ptr)       (((ptr)->mflags2 & M2_NASTY) != 0L)
# define strongmonst(ptr)       (((ptr)->mflags2 & M2_STRONG) != 0L)
# define can_breathe(ptr)       attacktype(ptr, AT_BREA)
# define cantwield(ptr)         (nohands(ptr) || verysmall(ptr))
# define could_twoweap(ptr)     ((ptr)->mattk[1].aatyp == AT_WEAP)
# define cantweararm(ptr)       (breakarm(ptr) || sliparm(ptr))
# define throws_rocks(ptr)      (((ptr)->mflags2 & M2_ROCKTHROW) != 0L)
# define type_is_pname(ptr)     (((ptr)->mflags2 & M2_PNAME) != 0L)
# define is_lord(ptr)           (((ptr)->mflags2 & M2_LORD) != 0L)
# define is_prince(ptr)         (((ptr)->mflags2 & M2_PRINCE) != 0L)
# define is_ndemon(ptr)         (is_demon(ptr) && \
                                 (((ptr)->mflags2 & (M2_LORD|M2_PRINCE)) == 0L))
# define is_dlord(ptr)          (is_demon(ptr) && is_lord(ptr))
# define is_dprince(ptr)        (is_demon(ptr) && is_prince(ptr))
# define pm_isminion(ptr)       (((ptr)->mflags2 & M2_MINION) != 0L)
# define likes_gold(ptr)        (((ptr)->mflags2 & M2_GREEDY) != 0L)
# define likes_gems(ptr)        (((ptr)->mflags2 & M2_JEWELS) != 0L)
# define likes_objs(ptr)        (((ptr)->mflags2 & M2_COLLECT) != 0L || \
                                 is_armed(ptr))
# define likes_magic(ptr)       (((ptr)->mflags2 & M2_MAGIC) != 0L)
# define webmaker(ptr)          ((ptr) == &mons[PM_CAVE_SPIDER] || \
                                 (ptr) == &mons[PM_GIANT_SPIDER])
# define is_unicorn(ptr)        ((ptr)->mlet == S_UNICORN && likes_gems(ptr))
# define is_longworm(ptr)       (((ptr) == &mons[PM_BABY_LONG_WORM]) || \
                                 ((ptr) == &mons[PM_LONG_WORM]) || \
                                 ((ptr) == &mons[PM_LONG_WORM_TAIL]))
# define is_covetous(ptr)       ((ptr->mflags3 & M3_COVETOUS) != 0L)
# define pm_infravision(ptr)    ((ptr->mflags3 & M3_INFRAVISION) != 0L)
# define pm_infravisible(ptr)   ((ptr->mflags3 & M3_INFRAVISIBLE) != 0L)
# define has_scent(ptr)         ((ptr->mflags3 & M3_SCENT) != 0L)
# define spellcaster(ptr)       (((ptr)->mflags3 & M3_SPELLCASTER) != 0L)
# define is_mplayer(ptr)        (((ptr) >= &mons[PM_ARCHEOLOGIST]) && \
                                 ((ptr) <= &mons[PM_WIZARD]))
# define is_rider(ptr)          ((ptr) == &mons[PM_DEATH] || \
                                 (ptr) == &mons[PM_FAMINE] || \
                                 (ptr) == &mons[PM_PESTILENCE])
# define is_placeholder(ptr)    ((ptr) == &mons[PM_ORC] || \
                                 (ptr) == &mons[PM_GIANT] || \
                                 (ptr) == &mons[PM_ELF] || \
                                 (ptr) == &mons[PM_HUMAN])
/* return TRUE if the monster tends to revive */
# define is_reviver(ptr)        (is_rider(ptr) || (ptr)->mlet == S_TROLL)

/* this returns the light's range, or 0 if none; if we add more light emitting
   monsters, we'll likely have to add a new light range field to mons[] */
# define emits_light(ptr)       (((ptr)->mlet == S_LIGHT || \
                                  (ptr) == &mons[PM_FLAMING_SPHERE] || \
                                  (ptr) == &mons[PM_SHOCKING_SPHERE] || \
                                  (ptr) == &mons[PM_FIRE_VORTEX]) ? 1 : \
                                 ((ptr) == &mons[PM_FIRE_ELEMENTAL]) ? 1 : 0)
/*      [note: the light ranges above were reduced to 1 for performance...] */
# define likes_lava(ptr)        (ptr == &mons[PM_FIRE_ELEMENTAL] || \
                                 ptr == &mons[PM_SALAMANDER])
# define pm_invisible(ptr)      ((ptr) == &mons[PM_STALKER] || \
                                 (ptr) == &mons[PM_BLACK_LIGHT] || \
                                 (ptr) == &mons[PM_QUASIT])
# define pm_zombie(ptr)         dmgtype(ptr, AD_ZOMB)

/* could probably add more */
# define likes_fire(ptr)        ((ptr) == &mons[PM_FIRE_VORTEX] || \
                                  (ptr) == &mons[PM_FLAMING_SPHERE] || \
                                 likes_lava(ptr))

# define touch_petrifies(ptr)   ((ptr) == &mons[PM_COCKATRICE] || \
                                 (ptr) == &mons[PM_CHICKATRICE])

# define is_mind_flayer(ptr)    ((ptr) == &mons[PM_MIND_FLAYER] || \
                                 (ptr) == &mons[PM_MASTER_MIND_FLAYER])

# define is_bee(ptr)            ((ptr) == &mons[PM_KILLER_BEE] || \
                                 (ptr) == &mons[PM_QUEEN_BEE])

# define nonliving(ptr)         (is_golem(ptr) || is_undead(ptr) || \
                                 (ptr)->mlet == S_VORTEX || \
                                 (ptr) == &mons[PM_MANES])

/* What types of monsters can be Astral Plane minions? */
# define roamer_type(ptr)       ((ptr) == &mons[PM_ALIGNED_PRIEST] || \
                                 (ptr) == &mons[PM_ANGEL])

/* Used for conduct with corpses, tins, and digestion attacks */
/* G_NOCORPSE monsters might still be swallowed as a purple worm */
/* Maybe someday this could be in mflags... */
# define vegan(ptr)             ((ptr)->mlet == S_BLOB || \
                                 (ptr)->mlet == S_JELLY ||            \
                                 (ptr)->mlet == S_FUNGUS ||           \
                                 (ptr)->mlet == S_VORTEX ||           \
                                 (ptr)->mlet == S_LIGHT ||            \
                                ((ptr)->mlet == S_ELEMENTAL &&        \
                                 (ptr) != &mons[PM_STALKER]) ||       \
                                ((ptr)->mlet == S_GOLEM &&            \
                                 (ptr) != &mons[PM_FLESH_GOLEM] &&    \
                                 (ptr) != &mons[PM_LEATHER_GOLEM]) || \
                                 noncorporeal(ptr))
# define vegetarian(ptr)        (vegan(ptr) || \
                                ((ptr)->mlet == S_PUDDING &&         \
                                 (ptr) != &mons[PM_BLACK_PUDDING]))

# define befriend_with_obj(ptr, obj) ((obj)->oclass == FOOD_CLASS && \
                                      is_domestic(ptr))

/* Generated in readonly.c */
extern const int monstr[];
/* Dummy address */
extern struct monst zeromonst;

#endif /* MONDATA_H */

