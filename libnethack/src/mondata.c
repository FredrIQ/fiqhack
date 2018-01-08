/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-01-08 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static int pm_gender(int);
static const struct attack *dmgtype_fromattack(const struct permonst *, int,
                                               int);

/* These routines provide basic data for any type of monster. */

/* Is the permonst newer than the save revision? */
boolean
is_new_pm(int pm)
{
    return FALSE; /* No new monsters so far */
}

boolean
is_removed_pm(int pm)
{
    if (flags.save_revision < 11) {
        if (pm == 290 /* old PM_INCUBUS */)
            return TRUE;
        if (pm == 331 /* old PM_CAVEWOMAN */)
            return TRUE;
        if (pm == 336 /* old PM_PRIESTESS */)
            return TRUE;
    }
    return FALSE;
}

int
pm_offset(int pm)
{
    int offset = 0;
    if (flags.save_revision < 11) {
        if (pm >= 290)
            offset--;
        if (pm >= 331)
            offset--;
        if (pm >= 336)
            offset--;
    }
    return offset;
}

/* Set up a new mvitals, for new monsters. */
void
new_mvitals(int pm)
{
    mvitals[pm].born = 0;
    mvitals[pm].died = 0;
    mvitals[pm].mvflags = mons[pm].geno & G_NOCORPSE;
}

/* Returns male name of monster */
const char *
pm_male(int pm)
{
    if (pm < LOW_PM || pm >= NUMMONS) {
        impossible("pm_male: pm out of range (%d)", pm);
        return "?m";
    }

    return mons[pm].mname;
}

/* Returns female name of monster */
const char *
pm_female(int pm)
{
    if (pm < LOW_PM || pm >= NUMMONS) {
        impossible("pm_male: pm out of range (%d)", pm);
        return "?f";
    }

    return mons[pm].fname;
}

/* Returns male or female name for mon appropriately */
const char *
pm_name(const struct monst *mon)
{
    boolean female = mon->female;
    if (mon == &youmonst)
        female = u.ufemale;
    if (is_neuter(mon->data))
        female = FALSE;

    int pm = NON_PM;
    if (mon != &youmonst)
        pm = monsndx(mon->data);
    else
        pm = u.umonnum;

    if (female)
        return pm_female(pm);
    return pm_male(pm);
}

/* Returns male or female name for object mon appropriately. */
const char *
opm_name(const struct obj *obj)
{
    int gender = obj_gender(obj);
    if (!strcmp(pm_male(obj->corpsenm), pm_female(obj->corpsenm)) &&
        gender != 2) {
        if (gender == 1)
            return msgprintf("female %s", pm_female(obj->corpsenm));
        return msgprintf("male %s", pm_male(obj->corpsenm));
    }

    if (gender == 1)
        return pm_female(obj->corpsenm);
    return pm_male(obj->corpsenm); /* or neuter */
}

void
set_mon_data(struct monst *mon, const struct permonst *ptr)
{
    mon->data = ptr;
}


const struct attack *
attacktype_fordmg(const struct permonst *ptr, int atyp, int dtyp)
{
    const struct attack *a;

    for (a = &ptr->mattk[0]; a < &ptr->mattk[NATTK]; a++)
        if (a->aatyp == atyp && (dtyp == AD_ANY || a->adtyp == dtyp))
            return a;

    return NULL;
}

boolean
attacktype(const struct permonst * ptr, int atyp)
{
    return attacktype_fordmg(ptr, atyp, AD_ANY) ? TRUE : FALSE;
}


boolean
poly_when_stoned(const struct permonst * ptr)
{
    return ((boolean)
            (is_golem(ptr) && ptr != &mons[PM_STONE_GOLEM] &&
             !(mvitals[PM_STONE_GOLEM].mvflags & G_GENOD)));
    /* allow G_EXTINCT */
}

/* Returns a bonus counting equipment enchantment and a potential extra.
   Used for rings of increase damage/accuracy/protection/searching. */
int
mon_bon(struct monst *mon, int otyp, int extra)
{
    int ret = 0;
    struct obj *obj;
    for (obj = mon->minvent; obj; obj = obj->nobj)
        if (obj->otyp == otyp &&
            ((obj->oclass == RING_CLASS &&
              (obj->owornmask & W_RING)) ||
             (obj->owornmask & W_MASK(which_slot(obj)))))
            ret += obj->spe;

    ret += extra;
    return ret;
}

/* Returns true if the monster is far away from hero */
boolean
distant(const struct monst *mon)
{
    return (distu(mon->mx, mon->my) <= (BOLT_LIM * BOLT_LIM));
}

int
searchbon(struct monst *mon)
{
    int ret = mon_bon(mon, RIN_SEARCHING, (mon)->msearchinc);
    struct obj *arm = which_armor(mon, os_arm);
    if (arm && (arm->otyp == WHITE_DRAGON_SCALES || arm->otyp == WHITE_DRAGON_SCALE_MAIL))
        ret += arm->spe;
    return ret;
}

/* TRUE iff monster is resistant to light-induced blindness */
boolean
resists_blnd(const struct monst * mon)
{
    const struct permonst *ptr = mon->data;
    boolean is_you = (mon == &youmonst);
    struct obj *o;

    if (blind(mon) || mon->msleeping)
        return TRUE;
    /* yellow light, Archon; !dust vortex, !cobra, !raven */
    if (dmgtype_fromattack(ptr, AD_BLND, AT_EXPL) ||
        dmgtype_fromattack(ptr, AD_BLND, AT_GAZE))
        return TRUE;
    o = is_you ? uwep : MON_WEP(mon);
    if (o && o->oartifact && defends(AD_BLND, o))
        return TRUE;
    o = mon->minvent;
    for (; o; o = o->nobj)
        if (o->oartifact && protects(AD_BLND, o))
            return TRUE;
    return FALSE;
}

/* TRUE if monster can be slowed. */
boolean
resists_slow(const struct monst *mon)
{
    if (free_action(mon) && rn2(3))
        return TRUE;

    struct obj *arm = which_armor(mon, os_arm);
    if (arm &&
        (arm->otyp == BLUE_DRAGON_SCALES ||
         arm->otyp == BLUE_DRAGON_SCALE_MAIL))
        return TRUE;

    return FALSE;
}

/* TRUE iff monster can be blinded by the given attack */
/* Note: may return TRUE when mdef is blind (e.g. new cream-pie attack) */
boolean
can_blnd(struct monst * magr,   /* NULL == no specific aggressor */
         struct monst * mdef, uchar aatyp, struct obj * obj)
{       /* aatyp == AT_WEAP, AT_SPIT */
    boolean is_you = (mdef == &youmonst);
    boolean check_visor = FALSE;
    struct obj *o;
    const char *s;

    /* no eyes protect against all attacks for now */
    if (!haseyes(mdef->data))
        return FALSE;

    switch (aatyp) {
    case AT_EXPL:
    case AT_BOOM:
    case AT_GAZE:
    case AT_BREA:      /* assumed to be lightning */
        /* light-based attacks may be cancelled or resisted */
        if (magr && cancelled(magr))
            return FALSE;
        return !resists_blnd(mdef);

    case AT_WEAP:
    case AT_SPIT:
    case AT_NONE:
        /* an object is used (thrown/spit/other) */
        if (obj && (obj->otyp == CREAM_PIE)) {
            if (blind(mdef) & W_MASK(os_tool))
                return FALSE;
        } else if (obj && (obj->otyp == BLINDING_VENOM)) {
            /* all ublindf, including LENSES, protect, cream-pies too */
            if (is_you && (ublindf || u.ucreamed))
                return FALSE;
            check_visor = TRUE;
        } else if (obj && (obj->otyp == POT_BLINDNESS)) {
            return TRUE;        /* no defense */
        } else
            return FALSE;       /* other objects cannot cause blindness yet */
        if ((magr == &youmonst) && Engulfed)
            return FALSE;       /* can't affect eyes while inside monster */
        break;

    case AT_ENGL:
        if (is_you && ((blind(&youmonst) & W_MASK(os_tool)) ||
                       u_helpless(hm_asleep) || u.ucreamed))
            return FALSE;
        if (!is_you && mdef->msleeping)
            return FALSE;
        break;

    case AT_CLAW:
        /* e.g. raven: all ublindf, including LENSES, protect */
        if (is_you && ublindf)
            return FALSE;
        if ((magr == &youmonst) && Engulfed)
            return FALSE;       /* can't affect eyes while inside monster */
        check_visor = TRUE;
        break;

    case AT_TUCH:
    case AT_STNG:
        /* some physical, blind-inducing attacks can be cancelled */
        if (magr && cancelled(magr))
            return FALSE;
        break;

    default:
        break;
    }

    /* check if wearing a visor (only checked if visor might help) */
    if (check_visor) {
        o = mdef->minvent;
        for (; o; o = o->nobj)
            if ((o->owornmask & W_MASK(os_armh)) &&
                (s = OBJ_DESCR(objects[o->otyp])) != NULL &&
                !strcmp(s, "visored helmet"))
                return FALSE;
    }

    return TRUE;
}


/* returns TRUE if monster can attack at range */
boolean
ranged_attk(const struct permonst * ptr)
{
    int i, atyp;
    long atk_mask = (1L << AT_BREA) | (1L << AT_SPIT) | (1L << AT_GAZE);

    /* was: (attacktype(ptr, AT_BREA) || attacktype(ptr, AT_WEAP) ||
       attacktype(ptr, AT_SPIT) || attacktype(ptr, AT_GAZE));
       but that's too slow -dlc */
    for (i = 0; i < NATTK; i++) {
        atyp = ptr->mattk[i].aatyp;
        if (atyp >= AT_WEAP)
            return TRUE;
        /* assert(atyp < 32); */
        if ((atk_mask & (1L << atyp)) != 0L)
            return TRUE;
    }

    return FALSE;
}


/* returns TRUE if monster is especially affected by silver weapons */
boolean
hates_silver(const struct permonst * ptr)
{
    return ((boolean)
            (is_were(ptr) || ptr->mlet == S_VAMPIRE || is_demon(ptr) ||
             ptr == &mons[PM_SHADE] || (ptr->mlet == S_IMP &&
                                        ptr != &mons[PM_TENGU])));
}

/* true iff the type of monster pass through iron bars */
boolean
passes_bars(const struct monst * mon)
{
    const struct permonst *mptr = mon->data;
    return (boolean) (phasing(mon) || amorphous(mptr) || is_whirly(mptr)
                      || verysmall(mptr) || (slithy(mptr) && !bigmonst(mptr)));
}


/* returns TRUE if monster can track well */
boolean
can_track(const struct permonst * ptr)
{
    if (uwep && uwep->oartifact == ART_EXCALIBUR)
        return TRUE;
    else
        return (boolean)(haseyes(ptr) || has_scent(ptr));
}

/* creature will slide out of armor */
boolean
sliparm(const struct permonst * ptr)
{
    return ((boolean)
            (is_whirly(ptr) || ptr->msize <= MZ_SMALL || noncorporeal(ptr)));
}

/* creature will break out of armor */
boolean
breakarm(const struct permonst * ptr)
{
    return ((bigmonst(ptr) || (ptr->msize > MZ_SMALL && !humanoid(ptr)) ||
             /* special cases of humanoids that cannot wear body armor */
             ptr == &mons[PM_MARILITH] || ptr == &mons[PM_WINGED_GARGOYLE])
            && !sliparm(ptr));
}

/* creature sticks other creatures it hits */
boolean
sticks(const struct permonst * ptr)
{
    return ((boolean)
            (dmgtype(ptr, AD_STCK) || dmgtype(ptr, AD_WRAP) ||
             attacktype(ptr, AT_HUGS)));
}

/* number of horns this type of monster has on its head */
int
num_horns(const struct permonst *ptr)
{
    switch (monsndx(ptr)) {
    case PM_HORNED_DEVIL:      /* ? "more than one" */
    case PM_MINOTAUR:
    case PM_ASMODEUS:
    case PM_BALROG:
        return 2;
    case PM_WHITE_UNICORN:
    case PM_GRAY_UNICORN:
    case PM_BLACK_UNICORN:
    case PM_KI_RIN:
        return 1;
    default:
        break;
    }
    return 0;
}

const struct attack *
dmgtype_fromattack(const struct permonst *ptr, int dtyp, int atyp)
{
    const struct attack *a;

    for (a = &ptr->mattk[0]; a < &ptr->mattk[NATTK]; a++)
        if (a->adtyp == dtyp && (atyp == AT_ANY || a->aatyp == atyp))
            return a;

    return NULL;
}

boolean
dmgtype(const struct permonst * ptr, int dtyp)
{
    return dmgtype_fromattack(ptr, dtyp, AT_ANY) ? TRUE : FALSE;
}

/* returns the maximum damage a defender can do to the attacker via
   a passive defense */
int
max_passive_dmg(const struct monst *mdef, const struct monst *magr)
{
    int i, n = 0, dmg = 0;
    uchar adtyp;

    for (i = 0; i < NATTK; i++) {
        if ((magr->data->mattk[i].aatyp == AT_NONE) ||
            (magr->data->mattk[i].aatyp == AT_BOOM))
            break;

        n++;
    }

    for (i = 0; i < NATTK; i++)
        if (mdef->data->mattk[i].aatyp == AT_NONE ||
            mdef->data->mattk[i].aatyp == AT_BOOM) {
            adtyp = mdef->data->mattk[i].adtyp;
            if ((adtyp == AD_ACID && !immune_to_acid(magr)) ||
                (adtyp == AD_COLD && !immune_to_cold(magr)) ||
                (adtyp == AD_FIRE && !immune_to_fire(magr)) ||
                (adtyp == AD_ELEC && !immune_to_elec(magr)) ||
                adtyp == AD_PHYS) {
                dmg = mdef->data->mattk[i].damn;
                if (!dmg)
                    dmg = mdef->data->mlevel + 1;
                dmg *= mdef->data->mattk[i].damd;
            } else
                dmg = 0;

            return n * dmg;
        }
    return 0;
}


/* return an index into the mons array
 * note: monsndx(mtmp->data) != mtmp->mnum for shape changers
 */
int
monsndx(const struct permonst *ptr)
{
    int i;

    i = (int)(ptr - &mons[0]);
    if (i < LOW_PM || i >= NUMMONS) {
        if (ptr == &pm_leader)
            return urole.ldrnum;
        else if (ptr == &pm_guardian)
            return urole.guardnum;
        else if (ptr == &pm_nemesis)
            return urole.neminum;
        else if (ptr == &pm_you_male || ptr == &pm_you_female)
            return u.umonnum;

        panic("monsndx - could not index monster (%p)", ptr);
    }

    return i;
}

int
name_to_mon(const char *in_str)
{
    int gend;
    return gname_to_mon(in_str, &gend);
}

/* gend is used to set the gender that the name specified */
int
gname_to_mon(const char *in_str, int *gend)
{
    /* Be careful.  We must check the entire string in case it was something
       such as "ettin zombie corpse".  The calling routine doesn't know about
       the "corpse" until the monster name has already been taken off the
       front, so we have to be able to read the name with extraneous stuff such
       as "corpse" stuck on the end. This causes a problem for names which
       prefix other names such as "ettin" on "ettin zombie".  In this case we
       want the _longest_ name which exists. This also permits plurals created
       by adding suffixes such as 's' or 'es'.  Other plurals must still be
       handled explicitly. */
    int i;
    int mntmp = NON_PM;
    char *s, *str, *term;
    /* Note: these bounds assume that we never lengthen str. */
    char mutable_in_str[strlen(in_str) + 1];
    int len, slen;

    /* special case: debug-mode players can create monsters by number; this is
       intended for programmatic monster creation */
    if (wizard && sscanf(in_str, "monsndx #%d", &mntmp) == 1 &&
        mntmp >= LOW_PM && mntmp < SPECIAL_PM)
        return mntmp;

    str = strcpy(mutable_in_str, in_str);

    if (!strncmp(str, "a ", 2))
        str += 2;
    else if (!strncmp(str, "an ", 3))
        str += 3;

    slen = strlen(str);
    term = str + slen;

    if ((s = strstri_mutable(str, "vortices")) != 0)
        strcpy(s + 4, "ex");
    /* be careful with "ies"; "priest", "zombies" */
    else if (slen > 3 && !strcmpi(term - 3, "ies") &&
             (slen < 7 || strcmpi(term - 7, "zombies")))
        strcpy(term - 3, "y");
    /* luckily no monster names end in fe or ve with ves plurals */
    else if (slen > 3 && !strcmpi(term - 3, "ves"))
        strcpy(term - 3, "f");

    slen = strlen(str); /* length possibly needs recomputing */

    {
        static const struct alt_spl {
            const char *name;
            short pm_val;
            int gender; /* 0: male, 1: female, 2: none/unspecified */
        } names[] = {
            /* Alternate spellings */
            { "grey dragon", PM_GRAY_DRAGON, 2 },
            { "baby grey dragon", PM_BABY_GRAY_DRAGON, 2 },
            { "grey unicorn", PM_GRAY_UNICORN, 2 },
            { "grey ooze", PM_GRAY_OOZE, 2 },
            { "gray-elf", PM_GREY_ELF, 2 },
            { "mindflayer", PM_MIND_FLAYER, 2 },
            { "master mindflayer", PM_MASTER_MIND_FLAYER, 2 },
            /* Hyphenated names */
            { "ki rin", PM_KI_RIN, 2 },
            { "uruk hai", PM_URUK_HAI, 2 },
            { "orc captain", PM_ORC_CAPTAIN, 2 },
            { "woodland elf", PM_WOODLAND_ELF, 2 },
            { "green elf", PM_GREEN_ELF, 2 },
            { "grey elf", PM_GREY_ELF, 2 },
            { "gray elf", PM_GREY_ELF, 2 },
            { "elf lord", PM_ELF_LORD, 0 },
            { "elf lady", PM_ELF_LORD, 1 },
            { "olog hai", PM_OLOG_HAI, 2 },
            { "arch lich", PM_ARCH_LICH, 2 },
            /* Some irregular plurals */
            { "incubi", PM_INCUBUS, 0 },
            { "succubi", PM_INCUBUS, 1 },
            { "violet fungi", PM_VIOLET_FUNGUS, 2 },
            { "homunculi", PM_HOMUNCULUS, 2 },
            { "baluchitheria", PM_BALUCHITHERIUM, 2 },
            { "lurkers above", PM_LURKER_ABOVE, 2 },
            { "cavemen", PM_CAVEMAN, 0 },
            { "cavewomen", PM_CAVEMAN, 1 },
            { "djinn", PM_DJINNI, 2 },
            { "mumakil", PM_MUMAK, 2 },
            { "erinyes", PM_ERINYS, 2 },
            /* falsely caught by -ves check above */
            { "master of thief", PM_MASTER_OF_THIEVES, 2 },
            /* end of list */
            {0, 0}
        };
        const struct alt_spl *namep;

        for (namep = names; namep->name; namep++)
            if (!strncmpi(str, namep->name, (int)strlen(namep->name))) {
                *gend = namep->gender;
                return namep->pm_val;
            }
    }

    for (len = 0, i = LOW_PM; i < NUMMONS; i++) {
        boolean match = FALSE;
        int m_i_len = strlen(mons[i].mname);
        int f_i_len = strlen(mons[i].fname);
        if (m_i_len > len && !strncmpi(mons[i].mname, str, m_i_len)) {
            match = TRUE;
            *gend = 2;
            if (strcmpi(mons[i].mname, mons[i].fname))
                *gend = 0;
        } else if (f_i_len > len && !strncmpi(mons[i].fname, str, f_i_len)) {
            match = TRUE;
            m_i_len = f_i_len;
            *gend = 1;
        }

        if (match) {
            if (m_i_len == slen)
                return i;       /* exact match */
            else if (slen > m_i_len &&
                     (str[m_i_len] == ' ' || !strcmpi(&str[m_i_len], "s") ||
                      !strncmpi(&str[m_i_len], "s ", 2) ||
                      !strcmpi(&str[m_i_len], "'") ||
                      !strncmpi(&str[m_i_len], "' ", 2) ||
                      !strcmpi(&str[m_i_len], "'s") ||
                      !strncmpi(&str[m_i_len], "'s ", 3) ||
                      !strcmpi(&str[m_i_len], "es") ||
                      !strncmpi(&str[m_i_len], "es ", 3))) {
                mntmp = i;
                len = m_i_len;
            }
        }
    }
    if (mntmp == NON_PM)
        mntmp = title_to_mon(str, NULL, NULL);
    return mntmp;
}


/* returns 3 values (0=male, 1=female, 2=none) */
int
gender(struct monst *mtmp)
{
    if (is_neuter(mtmp->data))
        return 2;
    return mtmp->female;
}

/* returns 3 values (0=male, 1=female, 2=neuter, 3=varies) */
static int
pm_gender(int pm)
{
    if (pm < LOW_PM || pm > NUMMONS)
        panic("pm_gender: permonst out of range (%d)", pm);

    const struct permonst *mdat = &mons[pm];
    if (is_neuter(mdat))
        return 2;
    if (is_male(mdat))
        return 0;
    if (is_female(mdat))
        return 1;
    return 3;
}

/* returns 3 values (0=male, 1=female, 2=none) */
int
obj_gender(const struct obj *obj)
{
    if (!corpsenm_is_relevant(obj->otyp))
        return 2;
    if (obj->otyp == TIN && (obj->spe & OPM_SPINACH))
        return 2;
    if (obj->otyp == EGG && obj->corpsenm == NON_PM)
        return 2;

    int fixedgend = pm_gender(obj->corpsenm);
    if (fixedgend == 3)
        return ((obj->spe & OPM_MALE) ? 0 : 1);
    return fixedgend;
}


/* Like gender(), but lower animals and such are still "it". */
/* This is the one we want to use when printing messages. */
int
pronoun_gender(const struct monst *mtmp)
{
    if (mtmp == &youmonst)
        return (u.ufemale ? 1 : 0);

    if (is_neuter(mtmp->data) || !canclassifymon(mtmp))
        return 2;
    return (humanoid(mtmp->data) || (mtmp->data->geno & G_UNIQ) ||
            type_is_pname(mtmp->data)) ? (int)mtmp->female : 2;
}


/* used for nearby monsters when you go to another level */
boolean
levl_follower(struct monst *mtmp)
{
    /* monsters with the Amulet--even pets--won't follow across levels */
    if (mon_has_amulet(mtmp))
        return FALSE;

    /* some monsters will follow even while intending to flee from you */
    if (mtmp->mtame || mtmp->iswiz || is_fshk(mtmp))
        return TRUE;

    /* stalking types follow, but won't when fleeing unless you hold the Amulet 
     */
    return (boolean) ((mtmp->data->mflags2 & M2_STALK) &&
                      (!mtmp->mflee || Uhave_amulet));
}

static const short grownups[][2] = {
    {PM_CHICKATRICE, PM_COCKATRICE},
    {PM_LITTLE_DOG, PM_DOG}, {PM_DOG, PM_LARGE_DOG},
    {PM_HELL_HOUND_PUP, PM_HELL_HOUND},
    {PM_WINTER_WOLF_CUB, PM_WINTER_WOLF},
    {PM_KITTEN, PM_HOUSECAT}, {PM_HOUSECAT, PM_LARGE_CAT},
    {PM_PONY, PM_HORSE}, {PM_HORSE, PM_WARHORSE},
    {PM_KOBOLD, PM_LARGE_KOBOLD}, {PM_LARGE_KOBOLD, PM_KOBOLD_LORD},
    {PM_GNOME, PM_GNOME_LORD}, {PM_GNOME_LORD, PM_GNOME_KING},
    {PM_DWARF, PM_DWARF_LORD}, {PM_DWARF_LORD, PM_DWARF_KING},
    {PM_MIND_FLAYER, PM_MASTER_MIND_FLAYER},
    {PM_ORC, PM_ORC_CAPTAIN}, {PM_HILL_ORC, PM_ORC_CAPTAIN},
    {PM_MORDOR_ORC, PM_ORC_CAPTAIN}, {PM_URUK_HAI, PM_ORC_CAPTAIN},
    {PM_SEWER_RAT, PM_GIANT_RAT},
    {PM_CAVE_SPIDER, PM_GIANT_SPIDER},
    {PM_OGRE, PM_OGRE_LORD}, {PM_OGRE_LORD, PM_OGRE_KING},
    {PM_ELF, PM_ELF_LORD}, {PM_WOODLAND_ELF, PM_ELF_LORD},
    {PM_GREEN_ELF, PM_ELF_LORD}, {PM_GREY_ELF, PM_ELF_LORD},
    {PM_ELF_LORD, PM_ELVENKING},
    {PM_LICH, PM_DEMILICH}, {PM_DEMILICH, PM_MASTER_LICH},
    {PM_MASTER_LICH, PM_ARCH_LICH},
    {PM_VAMPIRE, PM_VAMPIRE_LORD}, {PM_BAT, PM_GIANT_BAT},
    {PM_BABY_GRAY_DRAGON, PM_GRAY_DRAGON},
    {PM_BABY_SILVER_DRAGON, PM_SILVER_DRAGON},
    {PM_BABY_SHIMMERING_DRAGON, PM_SHIMMERING_DRAGON},
    {PM_BABY_RED_DRAGON, PM_RED_DRAGON},
    {PM_BABY_WHITE_DRAGON, PM_WHITE_DRAGON},
    {PM_BABY_ORANGE_DRAGON, PM_ORANGE_DRAGON},
    {PM_BABY_BLACK_DRAGON, PM_BLACK_DRAGON},
    {PM_BABY_BLUE_DRAGON, PM_BLUE_DRAGON},
    {PM_BABY_GREEN_DRAGON, PM_GREEN_DRAGON},
    {PM_BABY_YELLOW_DRAGON, PM_YELLOW_DRAGON},
    {PM_RED_NAGA_HATCHLING, PM_RED_NAGA},
    {PM_BLACK_NAGA_HATCHLING, PM_BLACK_NAGA},
    {PM_GOLDEN_NAGA_HATCHLING, PM_GOLDEN_NAGA},
    {PM_GUARDIAN_NAGA_HATCHLING, PM_GUARDIAN_NAGA},
    {PM_SMALL_MIMIC, PM_LARGE_MIMIC}, {PM_LARGE_MIMIC, PM_GIANT_MIMIC},
    {PM_BABY_LONG_WORM, PM_LONG_WORM},
    {PM_BABY_PURPLE_WORM, PM_PURPLE_WORM},
    {PM_BABY_CROCODILE, PM_CROCODILE},
    {PM_SOLDIER, PM_SERGEANT},
    {PM_SERGEANT, PM_LIEUTENANT},
    {PM_LIEUTENANT, PM_CAPTAIN},
    {PM_WATCHMAN, PM_WATCH_CAPTAIN},
    {PM_ALIGNED_PRIEST, PM_HIGH_PRIEST},
    {PM_STUDENT, PM_ARCHEOLOGIST},
    {PM_ATTENDANT, PM_HEALER},
    {PM_PAGE, PM_KNIGHT},
    {PM_ACOLYTE, PM_PRIEST},
    {PM_APPRENTICE, PM_WIZARD},
    {PM_MANES, PM_LEMURE},
    {PM_KEYSTONE_KOP, PM_KOP_SERGEANT},
    {PM_KOP_SERGEANT, PM_KOP_LIEUTENANT},
    {PM_KOP_LIEUTENANT, PM_KOP_KAPTAIN},
    {NON_PM, NON_PM}
};

int
little_to_big(int montype)
{
    int i;

    for (i = 0; grownups[i][0] >= LOW_PM; i++)
        if (montype == grownups[i][0])
            return grownups[i][1];
    return montype;
}

int
big_to_little(int montype)
{
    int i;

    for (i = 0; grownups[i][0] >= LOW_PM; i++)
        if (montype == grownups[i][1])
            return grownups[i][0];
    return montype;
}

/*
 * Return the permonst ptr for the race of the monster.
 * Returns correct pointer for non-polymorphed and polymorphed
 * player.  It does not return a pointer to player role character.
 */
const struct permonst *
raceptr(struct monst *mtmp)
{
    if (mtmp == &youmonst && !Upolyd)
        return &mons[urace.num];
    else
        return mtmp->data;
}

static const char *const levitate[4] = { "float", "Float", "wobble", "Wobble" };
static const char *const flys[4] = { "fly", "Fly", "flutter", "Flutter" };
static const char *const flyl[4] = { "fly", "Fly", "stagger", "Stagger" };
static const char *const slither[4] =
    { "slither", "Slither", "falter", "Falter" };
static const char *const ooze[4] = { "ooze", "Ooze", "tremble", "Tremble" };
static const char *const immobile[4] =
    { "wiggle", "Wiggle", "pulsate", "Pulsate" };
static const char *const crawl[4] = { "crawl", "Crawl", "falter", "Falter" };

/* BUG: this doesn't check current monster properties (ex: ring of levitation) */
const char *
locomotion(const struct permonst *ptr, const char *def)
{
    int capitalize = (*def == highc(*def));

    return (is_floater(ptr) ? levitate[capitalize]
            : (is_flyer(ptr) &&
               ptr->msize <= MZ_SMALL) ? flys[capitalize] :
            (is_flyer(ptr) && ptr->msize > MZ_SMALL) ?
            flyl[capitalize] : slithy(ptr) ? slither[capitalize] :
            amorphous(ptr) ? ooze[capitalize] : !ptr->mmove ?
            immobile[capitalize] : nolimbs(ptr) ? crawl[capitalize] : def);

}

const char *
stagger(const struct permonst *ptr, const char *def)
{
    int capitalize = 2 + (*def == highc(*def));

    return (is_floater(ptr) ? levitate[capitalize]
            : (is_flyer(ptr) &&
               ptr->msize <= MZ_SMALL) ? flys[capitalize] :
            (is_flyer(ptr) && ptr->msize > MZ_SMALL) ?
            flyl[capitalize] : slithy(ptr) ? slither[capitalize] :
            amorphous(ptr) ? ooze[capitalize] : !ptr->mmove ?
            immobile[capitalize] : nolimbs(ptr) ? crawl[capitalize] : def);

}

/* return a phrase describing the effect of fire attack on a type of monster */
const char *
on_fire(const struct permonst *mptr, const struct attack *mattk)
{
    const char *what;

    switch (monsndx(mptr)) {
    case PM_FLAMING_SPHERE:
    case PM_FIRE_VORTEX:
    case PM_FIRE_ELEMENTAL:
    case PM_SALAMANDER:
        what = "already on fire";
        break;
    case PM_WATER_ELEMENTAL:
    case PM_FOG_CLOUD:
    case PM_STEAM_VORTEX:
        what = "boiling";
        break;
    case PM_ICE_VORTEX:
    case PM_GLASS_GOLEM:
        what = "melting";
        break;
    case PM_STONE_GOLEM:
    case PM_CLAY_GOLEM:
    case PM_GOLD_GOLEM:
    case PM_AIR_ELEMENTAL:
    case PM_EARTH_ELEMENTAL:
    case PM_DUST_VORTEX:
    case PM_ENERGY_VORTEX:
        what = "heating up";
        break;
    default:
        what = (mattk->aatyp == AT_HUGS) ? "being roasted" : "on fire";
        break;
    }
    return what;
}

static int skill_to_mskill[P_NUM_SKILLS] = {
    [P_ATTACK_SPELL] = MP_SATTK,
    [P_HEALING_SPELL] = MP_SHEAL,
    [P_DIVINATION_SPELL] = MP_SDIVN,
    [P_ENCHANTMENT_SPELL] = MP_SENCH,
    [P_CLERIC_SPELL] = MP_SCLRC,
    [P_ESCAPE_SPELL] = MP_SESCA,
    [P_MATTER_SPELL] = MP_SMATR,
    [P_WANDS] = MP_WANDS,
};

/* check monster proficiency */
short
mprof(const struct permonst *mdat, int proficiency)
{
    int mskill = skill_to_mskill[proficiency];
    if (!mskill)
        return P_ISRESTRICTED;

    /* return the relevant bits. */
    return (short) ((((mdat)->mskill / mskill) % 4) + 1);
}

/*mondata.c*/
