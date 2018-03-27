/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-03-27 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#define ARTIFACT_C
#include "artilist.h"
#include "lev.h"

/*
 * Note:  both artilist[] and artigen[] have a dummy element #0,
 *        so loops over them should normally start at #1.  The primary
 *        exception is the save & restore code, which doesn't care about
 *        the contents, just the total size.
 */

#define get_artifact(o) \
                (((o)&&(o)->oartifact) ? &artilist[(int) (o)->oartifact] : 0)

static int spec_applies(const struct artifact *, const struct monst *);
static int arti_invoke(struct obj *);
static boolean magicbane_hit(struct monst *magr, struct monst *mdef,
                             struct obj *, int *, int, boolean,
                             const char *, int);
static boolean spfx_provides_extrinsic(const struct obj *,
                                       unsigned long, int, int *);
static long spec_m2(const struct obj *);

/* The amount added to the victim's total hit points to insure that the
   victim will be killed even after damage bonus/penalty adjustments.
   Most such penalties are small, and 200 is plenty; the exception is
   half physical damage.  3.3.1 and previous versions tried to use a very
   large number to account for this case; now, we just compute the fatal
   damage by adding it to 2 times the total hit points instead of 1 time.
   Note: this will still break if they have more than about half the number
   of hit points that will fit in a 15 bit integer. */
#define FATAL_DAMAGE_MODIFIER 200

/* flags including which artifacts have already been created */
static boolean artigen[1 + NROFARTIFACTS + 1];

/* and a discovery list for them (no dummy first entry here) */
static xchar artidisco[NROFARTIFACTS];

static void hack_artifacts(void);
static boolean attacks(int, struct obj *);


void
init_artilist(void)
{
    artilist = malloc(sizeof (const_artilist));
    memcpy(artilist, const_artilist, sizeof (const_artilist));
}

/* handle some special cases; must be called after u_init() */
static void
hack_artifacts(void)
{
    struct artifact *art;
    int alignmnt = aligns[u.initalign].value;

    /* Fix up the alignments of "gift" artifacts */
    for (art = artilist + 1; art->otyp; art++)
        if (art->role == Role_switch && art->alignment != A_NONE)
            art->alignment = alignmnt;

    /* Excalibur can be used by any lawful character, not just knights */
    if (!Role_if(PM_KNIGHT))
        artilist[ART_EXCALIBUR].role = NON_PM;

    /* Fix up the quest artifact */
    if (urole.questarti) {
        artilist[urole.questarti].alignment = alignmnt;
        artilist[urole.questarti].role = Role_switch;
    }
    return;
}

/* zero out the artifact existence list */
void
init_artifacts(void)
{
    memset(artigen, 0, sizeof artigen);
    memset(artidisco, 0, sizeof artidisco);
    hack_artifacts();
}

void
save_artifacts(struct memfile *mf)
{
    /* artigen and artidisco are arrays of bytes, so writing them in one go
       is safe and portable */
    mtag(mf, 0, MTAG_ARTIFACT);
    mwrite(mf, artigen, sizeof artigen);
    mwrite(mf, artidisco, sizeof artidisco);
}

void
restore_artifacts(struct memfile *mf)
{
    mread(mf, artigen, sizeof artigen);
    mread(mf, artidisco, sizeof artidisco);
    hack_artifacts();   /* redo non-saved special cases */
}

const char *
artiname(int artinum)
{
    if (artinum <= 0 || artinum > NROFARTIFACTS)
        return "";
    return artilist[artinum].name;
}

int
artityp(int artinum)
{
    if (artinum <= 0 || artinum > NROFARTIFACTS)
        return 0;
    return artilist[artinum].otyp;
}

/*
   Make an artifact.  If a specific alignment is specified, then an object of
   the appropriate alignment is created from scratch, or 0 is returned if
   none is available.  (If at least one aligned artifact has already been
   given, then unaligned ones also become eligible for this.)
   If no alignment is given, then 'otmp' is converted
   into an artifact of matching type, or returned as-is if that's not possible.
   For the 2nd case, caller should use ``obj = mk_artifact(lev, obj, A_NONE);''
   for the 1st, ``obj = mk_artifact(lev, NULL, some_alignment);''.
 */
struct obj *
mk_artifact(
    struct level *lev,	/* level to create artifact on if otmp not given */
    struct obj *otmp,	/* existing object; ignored if alignment specified */
    aligntyp alignment, /* target alignment, or A_NONE */
    enum rng rng        /* which random number generator to use */
    ) {
    const struct artifact *a;
    int n, m;
    boolean by_align = (alignment != A_NONE);
    short o_typ = (by_align || !otmp) ? 0 : otmp->otyp;
    boolean unique = !by_align && otmp && objects[o_typ].oc_unique;
    short eligible[NROFARTIFACTS];

    static const int round_number = 27720; /* divisible by all numbers 1..12 */
    int artif_number = rn2_on_rng(27720, rng);

    /* gather eligible artifacts */
    for (n = 0, a = artilist + 1, m = 1; a->otyp; a++, m++)
        if ((!by_align ? a->otyp ==
             o_typ : (a->alignment == alignment ||
                      (a->alignment == A_NONE && u.ugifts > 0))) &&
            (!(a->spfx & SPFX_NOGEN) || unique) && artigen[m] == ag_none) {
            if (by_align && a->race != NON_PM && race_hostile(&mons[a->race]))
                continue;       /* skip enemies' equipment */
            else if (by_align && Role_if(a->role))
                goto make_artif;        /* 'a' points to the desired one */
            else
                eligible[n++] = m;
        }

    /* If we didn't get any eligible artifacts, it's possible that the only
       valid first sacrifice gift was already generated.  Let's try again
       with looser restrictions. */
    if (n == 0)
        for (n = 0, a = artilist + 1, m = 1; a->otyp; a++, m++)
            if ((!by_align ? a->otyp ==
                 o_typ : (a->alignment == alignment || a->alignment == A_NONE)) &&
                (!(a->spfx & SPFX_NOGEN) || unique) && artigen[m] == ag_none) {
                if (by_align && a->race != NON_PM && race_hostile(&mons[a->race]))
                    continue;       /* skip enemies' equipment */
                else if (by_align && Role_if(a->role))
                    goto make_artif;        /* 'a' points to the desired one */
                else
                    eligible[n++] = m;
            }

    if (n) {    /* found at least one candidate */
        m = eligible[artif_number / (round_number / n)];   /* [0..n-1] */
        a = &artilist[m];

        /* make an appropriate object if necessary, then christen it */
    make_artif:
        /* note: not rng, that'd desync during level creation

           i.e. the /identity/ of the artifact is on the given RNG, but the
           /stats/ of the artifact (+1, etc.) are on rng_main */
        if (by_align)
            otmp = mksobj(lev, (int)a->otyp, TRUE, FALSE, rng_main);
        otmp = oname(otmp, a->name);
    } else {
        /* nothing appropriate could be found; return the original object */
        if (by_align)
            otmp = 0;   /* (there was no original object) */
    }
    return otmp;
}

/*
 * Returns the full name (with articles and correct capitalization) of an
 * artifact named "name" if one exists, or NULL, it not.
 * The given name must be rather close to the real name for it to match.
 * The object type of the artifact is returned in otyp if the return value
 * is non-NULL.
 */
const char *
artifact_name(const char *name, short *otyp)
{
    const struct artifact *a;
    const char *aname;

    if (!strncmpi(name, "the ", 4))
        name += 4;

    for (a = artilist + 1; a->otyp; a++) {
        aname = a->name;
        if (!strncmpi(aname, "the ", 4))
            aname += 4;
        if (!strcmpi(name, aname)) {
            *otyp = a->otyp;
            return a->name;
        }
    }

    return NULL;
}

boolean
exist_artifact(int otyp, const char *name)
{
    const struct artifact *a;
    boolean *arex;

    if (otyp && *name)
        for (a = artilist + 1, arex = artigen + 1; a->otyp; a++, arex++)
            if ((int)a->otyp == otyp && !strcmp(a->name, name))
                return *arex;
    return FALSE;
}

void
artifact_exists(struct obj *otmp, const char *name, int value)
{
    const struct artifact *a;

    if (otmp && *name)
        for (a = artilist + 1; a->otyp; a++)
            if (a->otyp == otmp->otyp && !strcmp(a->name, name)) {
                int m = a - artilist;

                otmp->oartifact = (char)(value ? m : 0);
                otmp->age = 0;
                artigen[m] = value;
                break;
            }
    return;
}

int
nartifact_exist(void)
{
    int a = 0;
    int n = SIZE(artigen);

    while (n > 1)
        if (artigen[--n] != ag_none)
            a++;

    return a;
}

int
nartifact_value(int value)
{
    int a = 0;
    int n = SIZE(artigen);

    while (n > 1)
        if (artigen[--n] == value)
            a++;

    return a;
}

boolean
spec_ability(struct obj * otmp, unsigned long abil)
{
    const struct artifact *arti = get_artifact(otmp);

    return (boolean) (arti && (arti->spfx & abil));
}

/* used so that callers don't need to known about SPFX_ codes */
boolean
confers_luck(struct obj * obj)
{
    /* might as well check for this too */
    if (obj->otyp == LUCKSTONE)
        return TRUE;

    return obj->oartifact && spec_ability(obj, SPFX_LUCK);
}

/* used to check whether a monster is getting reflection from an artifact */
boolean
arti_reflects(struct obj * obj)
{
    const struct artifact *arti = get_artifact(obj);

    if (arti) {
        /* while being worn */
        if ((obj->owornmask & W_MASKABLE) &&
            (arti->spfx & SPFX_REFLECT))
            return TRUE;
        /* just being carried */
        if (arti->cspfx & SPFX_REFLECT)
            return TRUE;
    }
    return FALSE;
}


boolean
restrict_name(struct obj * otmp, const char *name)
{       /* returns 1 if name is restricted for otmp->otyp */
    const struct artifact *a;
    const char *aname;

    if (!*name)
        return FALSE;
    if (!strncmpi(name, "the ", 4))
        name += 4;

    /* Since almost every artifact is SPFX_RESTR, it doesn't cost us much to do 
       the string comparison before the spfx check. Bug fix: don't name
       multiple elven daggers "Sting". */
    for (a = artilist + 1; a->otyp; a++) {
        if (a->otyp != otmp->otyp)
            continue;
        aname = a->name;
        if (!strncmpi(aname, "the ", 4))
            aname += 4;
        if (!strcmp(aname, name))
            return ((boolean)
                    ((a->spfx & (SPFX_NOGEN | SPFX_RESTR)) != 0 ||
                     otmp->quan > 1L));
    }

    return FALSE;
}

static boolean
attacks(int adtyp, struct obj *otmp)
{
    if (!otmp)
        return FALSE;

    if (!otmp->oartifact) {
        /* don't let armor of fire res do fire damage/similar */
        if (otmp->oclass != WEAPON_CLASS && !is_weptool(otmp))
            return FALSE;

        uint64_t props = obj_properties(otmp);
        if ((adtyp == AD_FIRE && (props & opm_fire)) ||
            (adtyp == AD_COLD && (props & opm_frost)) ||
            (adtyp == AD_ELEC && (props & opm_shock)))
            return TRUE;
        return FALSE;
    }

    const struct artifact *weap;
    if ((weap = get_artifact(otmp)) != 0)
        return (boolean) (weap->attk.adtyp == adtyp);
    return FALSE;
}

boolean
defends(int adtyp, struct obj * otmp)
{
    const struct artifact *weap;

    if ((weap = get_artifact(otmp)) != 0)
        return (boolean) (weap->defn.adtyp == adtyp);
    return FALSE;
}

/* used for monsters */
boolean
protects(int adtyp, struct obj * otmp)
{
    const struct artifact *weap;

    if ((weap = get_artifact(otmp)) != 0)
        return (boolean) (weap->cary.adtyp == adtyp);
    return FALSE;
}

/* Returns extrinsics the item provides and in which slot.
   For warntypes, returns the result in the provided *warntype
   pointer. */
long
item_provides_extrinsic(const struct obj *otmp, int extrinsic,
                        int *warntype)
{
    long res;
    res = item_provides_extrinsic_before_oprop(otmp, extrinsic,
                                               warntype);

    if (res == W_WORN || res == W_EQUIP)
        res &= otmp->owornmask;
    if (res && res != W_MASK(os_quiver) &&
        (res != W_MASK(os_swapwep) ||
         (otmp->where == OBJ_INVENT && u.twoweap)))
        return res;

    enum objslot slot = which_slot(otmp);
    if (slot == os_invalid) {
        if (otmp->oclass == WEAPON_CLASS || is_weptool(otmp)) {
            res = W_EQUIP;

            /* these are only offensive on weapons */
            if (extrinsic == FIRE_RES ||
                extrinsic == COLD_RES ||
                extrinsic == SHOCK_RES ||
                extrinsic == DRAIN_RES)
                return 0L;
        }
        else
            res = W_MASK(os_carried);
    } else
        res = W_WORN;

    if (obj_oprops_provides_property(otmp, extrinsic)) {
        if (res == W_WORN || res == W_EQUIP)
            res &= otmp->owornmask;
        res &= ~W_MASK(os_quiver);
        if (res == W_MASK(os_swapwep) &&
            (otmp->where != OBJ_INVENT || !u.twoweap))
            res = 0L;
        return res;
    }

    return 0L;
}

/* Checks everything except for object properties. Called
   directly by obj_properties() to figure out which ones are
   potentially redundant. This returns if the item provides
   an extrinsic for a slot *in general*, not if it does it
   *right now*. */
long
item_provides_extrinsic_before_oprop(const struct obj *otmp,
                                     int extrinsic,
                                     int *warntype)
{
    if (!otmp) {
        panic("Trying to check non-existing item?");
        return 0L;
    }
    const struct artifact *oart = get_artifact(otmp);
    uchar dtyp;
    long equipmask;

    /* Don't allow non-weapon equippables to provide an extrinsic
       if wielded */
    enum objslot slot = which_slot(otmp);
    if (slot == os_invalid) {
        if (otmp->oclass == WEAPON_CLASS || is_weptool(otmp))
            equipmask = W_EQUIP;
        else
            equipmask = W_MASK(os_carried);
    } else
        equipmask = W_WORN;

    /* Does the base item (artifact or not) provide the property in question?
       Skip WARN_OF_MON for paranoia reasons; it wouldn't work if it were
       used, because there'd be no way to communicate which monster is being
       warned against. Potions don't grant their innate properties. */
    if (objects[otmp->otyp].oc_oprop == extrinsic && extrinsic != WARN_OF_MON && otmp->oclass != POTION_CLASS)
        return equipmask;

    /* Non-artifact item properties go here. */
    if (otmp->otyp == AMULET_OF_RESTFUL_SLEEP && extrinsic == SLEEP_RES)
        return equipmask; /* gives a sleep resistance, but not a sleep immunity. */
    if (otmp->otyp == ALCHEMY_SMOCK && extrinsic == ACID_RES)
        return equipmask;
    if ((otmp->otyp == SHIMMERING_DRAGON_SCALE_MAIL ||
         otmp->otyp == SHIMMERING_DRAGON_SCALES) && extrinsic == STUN_RES)
        return equipmask;
    if ((otmp->otyp == RED_DRAGON_SCALE_MAIL ||
         otmp->otyp == RED_DRAGON_SCALES) &&
        (extrinsic == INFRAVISION || extrinsic == WARNING ||
         extrinsic == SEE_INVIS))
        return equipmask;
    if ((otmp->otyp == WHITE_DRAGON_SCALE_MAIL ||
         otmp->otyp == WHITE_DRAGON_SCALES) &&
        (extrinsic == WATERPROOF || extrinsic == SEARCHING))
        return equipmask;
    if ((otmp->otyp == ORANGE_DRAGON_SCALE_MAIL ||
         otmp->otyp == ORANGE_DRAGON_SCALES) && extrinsic == FREE_ACTION)
        return equipmask;
    if ((otmp->otyp == BLACK_DRAGON_SCALE_MAIL ||
         otmp->otyp == BLACK_DRAGON_SCALES) && extrinsic == DRAIN_RES)
        return equipmask;
    if ((otmp->otyp == BLUE_DRAGON_SCALE_MAIL ||
         otmp->otyp == BLUE_DRAGON_SCALES) && extrinsic == FAST)
        return equipmask;
    if ((otmp->otyp == GREEN_DRAGON_SCALE_MAIL ||
         otmp->otyp == GREEN_DRAGON_SCALES) && extrinsic == SICK_RES)
        return equipmask;
    if ((otmp->otyp == YELLOW_DRAGON_SCALE_MAIL ||
         otmp->otyp == YELLOW_DRAGON_SCALES) && extrinsic == STONE_RES)
        return equipmask;
    if (otmp->otyp == AMULET_OF_YENDOR &&
        (extrinsic == CLAIRVOYANT ||
         extrinsic == AGGRAVATE_MONSTER))
        return W_MASK(os_carried);

    if (!oart)
        return 0L;

    if (oart->inv_prop == extrinsic &&
        (otmp->owornmask & W_MASK(os_invoked)))
        return W_MASK(os_invoked);

    dtyp = oart->cary.adtyp;
    if ((dtyp == AD_FIRE && extrinsic == FIRE_RES) ||
        (dtyp == AD_COLD && extrinsic == COLD_RES) ||
        (dtyp == AD_ELEC && extrinsic == SHOCK_RES) ||
        (dtyp == AD_MAGM && extrinsic == ANTIMAGIC) ||
        (dtyp == AD_DISN && extrinsic == DISINT_RES) ||
        (dtyp == AD_DRST && extrinsic == POISON_RES) ||
        (dtyp == AD_DISE && extrinsic == SICK_RES) ||
        (dtyp == AD_DRLI && extrinsic == DRAIN_RES))
        return W_MASK(os_carried);

    dtyp = oart->defn.adtyp;
    if ((dtyp == AD_FIRE && extrinsic == FIRE_RES) ||
        (dtyp == AD_COLD && extrinsic == COLD_RES) ||
        (dtyp == AD_ELEC && extrinsic == SHOCK_RES) ||
        (dtyp == AD_MAGM && extrinsic == ANTIMAGIC) ||
        (dtyp == AD_DISN && extrinsic == DISINT_RES) ||
        (dtyp == AD_DRST && extrinsic == POISON_RES) ||
        (dtyp == AD_DISE && extrinsic == SICK_RES) ||
        (dtyp == AD_DRLI && extrinsic == DRAIN_RES))
        return equipmask;

    if (spfx_provides_extrinsic(otmp, oart->cspfx, extrinsic,
                                warntype))
        return W_MASK(os_carried);
    else if (spfx_provides_extrinsic(otmp, oart->spfx, extrinsic,
                                     warntype))
        return equipmask;

    return 0L;
}

/* Returns true if the given artifact spfx field provides
   extrinsic. Also messes with warntype if it's WARN_OF_MON.
   obj is needed to properly grab the warn of mon info. */
static boolean
spfx_provides_extrinsic(const struct obj *obj, unsigned long spfx,
                        int extrinsic, int *warntype)
{
    if ((spfx & SPFX_SEARCH  && extrinsic == SEARCHING) ||
        (spfx & SPFX_ESP     && extrinsic == TELEPAT) ||
        (spfx & SPFX_STLTH   && extrinsic == STEALTH) ||
        (spfx & SPFX_REGEN   && extrinsic == REGENERATION) ||
        (spfx & SPFX_TCTRL   && extrinsic == TELEPORT_CONTROL) ||
        (spfx & SPFX_EREGEN  && extrinsic == ENERGY_REGENERATION) ||
        (spfx & SPFX_HSPDAM  && extrinsic == HALF_SPDAM) ||
        (spfx & SPFX_HPHDAM  && extrinsic == HALF_PHDAM) ||
        (spfx & SPFX_HALRES  && extrinsic == HALLUC_RES) ||
        (spfx & SPFX_REFLECT && extrinsic == REFLECTING) ||
        (spfx & SPFX_XRAY    && extrinsic == XRAY_VISION) ||
        (spfx & SPFX_FREEACT && extrinsic == FREE_ACTION))
        return TRUE;

    if (spfx & SPFX_WARN) {
        if (spec_m2(obj)) {
            if (extrinsic == WARN_OF_MON) {
                *warntype = spec_m2(obj);
                return TRUE;
            }
        } else {
            if (extrinsic == WARNING)
                return TRUE;
        }
    }

    return FALSE;
}

/*
 * creature (usually player) tries to touch (pick up or wield) an artifact obj.
 * Returns 0 if the object refuses to be touched.
 * This routine does not change any object chains.
 * Ignores such things as gauntlets, assuming the artifact is not
 * fooled by such trappings.
 */
int
touch_artifact(struct obj *obj, const struct monst *mon)
{
    const struct artifact *oart = get_artifact(obj);
    boolean badclass, badalign, self_willed, yours;

    if (!oart)
        return 1;

    yours = (mon == &youmonst);
    /* all quest artifacts are self-willed; it this ever changes, `badclass'
       will have to be extended to explicitly include quest artifacts */
    self_willed = ((oart->spfx & SPFX_INTEL) != 0);
    if (yours) {
        badclass = self_willed &&
            ((oart->role != NON_PM && !Role_if(oart->role)) ||
             (oart->race != NON_PM && !Race_if(oart->race)));
        badalign = (oart->spfx & SPFX_RESTR) && oart->alignment != A_NONE &&
            (oart->alignment != u.ualign.type || u.ualign.record < 0);
    } else if (!is_covetous(mon->data) && !is_mplayer(mon->data)) {
        badclass = self_willed && oart->role != NON_PM &&
            oart != &artilist[ART_EXCALIBUR];
        badalign = (oart->spfx & SPFX_RESTR) && oart->alignment != A_NONE &&
            (oart->alignment != sgn(mon->data->maligntyp));
    } else {    /* an M3_WANTSxxx monster or a fake player */
        /* special monsters trying to take the Amulet, invocation tools or
           quest item can touch anything except for `spec_applies' artifacts */
        badclass = badalign = FALSE;
    }
    /* weapons which attack specific categories of monsters are bad for them
       even if their alignments happen to match */
    if (!badalign && (oart->spfx & SPFX_DBONUS) != 0) {
        struct artifact tmp;

        tmp = *oart;
        tmp.spfx &= SPFX_DBONUS;
        badalign = spec_applies(&tmp, mon);
    }

    if (((badclass || badalign) && self_willed) ||
        (badalign && (!yours || !rn2(4)))) {
        int dmg;
        const char *buf;

        if (!yours)
            return 0;
        pline(msgc_substitute, "You are blasted by %s power!",
              s_suffix(the(xname(obj))));
        dmg = dice((Antimagic ? 2 : 4), (self_willed ? 10 : 4));
        buf = msgprintf("touching %s", oart->name);
        losehp(dmg, killer_msg(DIED, buf));
        exercise(A_WIS, FALSE);
    }

    /* can pick it up unless you're totally non-synch'd with the artifact */
    if (badclass && badalign && self_willed) {
        /* not msgc_cancelled1, because you might not know your own numerical
           alignment record */
        if (yours)
            pline(msgc_failcurse, "%s your grasp!", Tobjnam(obj, "evade"));
        return 0;
    }

    return 1;
}


/* Decide whether an artifact's special attacks apply against mtmp.
   Returns 0 (doesn't apply), 1 (applies partially), 2 (applies completely) */
static int
spec_applies(const struct artifact *weap, const struct monst *mtmp)
{
    const struct permonst *ptr;
    boolean yours;

    if (!(weap->spfx & (SPFX_DBONUS | SPFX_ATTK)))
        return (weap->attk.adtyp == AD_PHYS) ? 2 : 0;

    yours = (mtmp == &youmonst);
    ptr = mtmp->data;

    if (weap->spfx & SPFX_DMONS) {
        return (ptr == &mons[(int)weap->mtype]) ? 2 : 0;
    } else if (weap->spfx & SPFX_DCLAS) {
        return (weap->mtype == (unsigned long)ptr->mlet) ? 2 : 0;
    } else if (weap->spfx & SPFX_DFLAG1) {
        return ((ptr->mflags1 & weap->mtype) != 0L) ? 2 : 0;
    } else if (weap->spfx & SPFX_DFLAG2) {
        return (((ptr->mflags2 & weap->mtype) ||
                 (yours &&
                  ((!Upolyd && (urace.selfmask & weap->mtype)) ||
                   ((weap->mtype & M2_WERE) && u.ulycn >= LOW_PM))))) ? 2 : 0;
    } else if (weap->spfx & SPFX_DALIGN) {
        return (yours ? (u.ualign.type != weap->alignment) :
                (ptr->maligntyp == A_NONE ||
                 sgn(ptr->maligntyp) != weap->alignment)) ? 2 : 0;
    } else if (weap->spfx & SPFX_ATTK) {
        struct obj *defending_weapon = (yours ? uwep : MON_WEP(mtmp));

        if (defending_weapon && defending_weapon->oartifact &&
            defends((int)weap->attk.adtyp, defending_weapon))
            return 0;
        switch (weap->attk.adtyp) {
        case AD_FIRE:
            return (!resists_fire(mtmp) ? 2 :
                    !immune_to_fire(mtmp) ? 1 : 0);
        case AD_COLD:
            return (!resists_cold(mtmp) ? 2 :
                    !immune_to_cold(mtmp) ? 1 : 0);
        case AD_ELEC:
            return (!resists_elec(mtmp) ? 2 :
                    !immune_to_elec(mtmp) ? 1 : 0);
        case AD_MAGM:
            return (!resists_magm(mtmp) ? 2 : 0);
        case AD_STUN:
            return (!resists_stun(mtmp) ? 2 : 0);
        case AD_DRST:
            return (!resists_poison(mtmp) ? 2 :
                    !immune_to_poison(mtmp) ? 1 : 0);
        case AD_DRLI:
            return (!resists_drli(mtmp) ? 2 : 0);
        case AD_STON:
            return (!resists_ston(mtmp) ? 2 : 0);
        default:
            impossible("Weird weapon special attack.");
        }
    }
    return 0;
}

/* return the M2 flags of monster that an artifact's special attacks apply
 * against */
static long
spec_m2(const struct obj *otmp)
{
    const struct artifact *artifact = get_artifact(otmp);

    if (artifact)
        return artifact->mtype;
    return 0L;
}

/* special attack bonus */
int
spec_abon(struct obj *otmp, struct monst *mon)
{
    const struct artifact *weap = get_artifact(otmp);

    /* no need for an extra check for `NO_ATTK' because this will always return 
       0 for any artifact which has that attribute */

    if (weap && weap->attk.damn && spec_applies(weap, mon))
        return rnd((int)weap->attk.damn);
    return 0;
}

/* special damage bonus */
int
spec_dbon(struct obj *otmp, struct monst *mon, int tmp,
          int *spec_dbon_applies)
{
    if (!otmp->oartifact) {
        uint64_t props = obj_properties(otmp);
        int bonus = 0;
        *spec_dbon_applies = 0;
        if ((props & opm_drain) && !resists_drli(mon)) {
            *spec_dbon_applies = 2;
            if (resists_drli(mon))
                *spec_dbon_applies = 1;
        }
        if (props & opm_vorpal)
            *spec_dbon_applies = 2;
        if ((props & opm_fire) && !immune_to_fire(mon)) {
            *spec_dbon_applies = 2;
            if (resists_fire(mon)) {
                *spec_dbon_applies = 1;
                bonus += rnd(3);
            } else
                bonus += rnd(6);
        }
        if ((props & opm_frost) && !immune_to_cold(mon)) {
            *spec_dbon_applies = 2;
            if (resists_cold(mon)) {
                *spec_dbon_applies = 1;
                bonus += rnd(3);
            } else
                bonus += rnd(6);
        }
        if ((props & opm_shock) && !immune_to_elec(mon)) {
            *spec_dbon_applies = 2;
            if (resists_elec(mon)) {
                *spec_dbon_applies = 1;
                bonus += rnd(3);
            } else
                bonus += rnd(6);
        }

        return bonus;
    }

    const struct artifact *weap = get_artifact(otmp);

    if (!weap || (weap->attk.adtyp == AD_PHYS &&       /* check for `NO_ATTK' */
                  weap->attk.damn == 0 && weap->attk.damd == 0))
        *spec_dbon_applies = 0;
    else
        *spec_dbon_applies = spec_applies(weap, mon);

    if (*spec_dbon_applies) {
        int res = weap->attk.damd ? rnd((int)weap->attk.damd) : max(tmp, 1);
        if (*spec_dbon_applies == 1)
            res = (res + 1) / 2;
        return res;
    }
    return 0;
}

/* add identified artifact to discoveries list */
void
discover_artifact(xchar m)
{
    int i;

    /* look for this artifact in the discoveries list; if we hit an empty slot
       then it's not present, so add it */
    for (i = 0; i < NROFARTIFACTS; i++)
        if (artidisco[i] == 0 || artidisco[i] == m) {
            artidisco[i] = m;
            return;
        }
    /* there is one slot per artifact, so we should never reach the end without 
       either finding the artifact or an empty slot... */
    impossible("couldn't discover artifact (%d)", (int)m);
}


/* used to decide whether an artifact has been fully identified */
boolean
undiscovered_artifact(xchar m)
{
    int i;

    /* look for this artifact in the discoveries list; if we hit an empty slot
       then it's undiscovered */
    for (i = 0; i < NROFARTIFACTS; i++)
        if (artidisco[i] == m)
            return FALSE;
        else if (artidisco[i] == 0)
            break;
    return TRUE;
}


/* display a list of discovered artifacts; return their count */
int
disp_artifact_discoveries(struct nh_menulist *menu)
{
    int i, m, otyp;
    const char *buf;

    for (i = 0; i < NROFARTIFACTS; i++) {
        if (artidisco[i] == 0)
            break;      /* empty slot implies end of list */
        if (i == 0)
            add_menuheading(menu, "Artifacts");
        m = artidisco[i];
        otyp = artilist[m].otyp;
        buf = msgprintf("  %s [%s %s]", artiname(m),
                        align_str(artilist[m].alignment),
                        simple_typename(otyp));
        add_menutext(menu, buf);
    }
    return i;
}


        /* 
         * Magicbane's intrinsic magic is incompatible with normal
         * enchantment magic.  Thus, its effects have a negative
         * dependence on spe.  Against low mr victims, it typically
         * does "double athame" damage, 2d4.  Occasionally, it will
         * cast unbalancing magic which effectively averages out to
         * 4d4 damage (3d4 against high mr victims), for spe = 0.
         *
         * Prior to 3.4.1, the cancel (aka purge) effect always
         * included the scare effect too; now it's one or the other.
         * Likewise, the stun effect won't be combined with either
         * of those two; it will be chosen separately or possibly
         * used as a fallback when scare or cancel fails.
         *
         * [Historical note: a change to artifact_hit() for 3.4.0
         * unintentionally made all of Magicbane's special effects
         * be blocked if the defender successfully saved against a
         * stun attack.  As of 3.4.1, those effects can occur but
         * will be slightly less likely than they were in 3.3.x.]
         */
#define MB_MAX_DIEROLL          8       /* rolls above this aren't magical */
static const char *const mb_verb[2][4] = {
    {"probe", "stun", "scare", "cancel"},
    {"prod", "amaze", "tickle", "purge"},
};

#define MB_INDEX_PROBE          0
#define MB_INDEX_STUN           1
#define MB_INDEX_SCARE          2
#define MB_INDEX_CANCEL         3

/* called when someone is being hit by Magicbane */
static boolean
magicbane_hit(struct monst *magr,   /* attacker */
              struct monst *mdef,   /* defender */
              struct obj *mb,       /* Magicbane */
              int *dmgptr,          /* extra damage target will suffer */
              int dieroll,          /* d20 that has already scored a hit */
              boolean vis,          /* whether the action can be seen */
              const char *hittee,   /* target's name: "you" or mon_nam(mdef) */
              int spec_dbon_applies
    )
{
    const struct permonst *old_uasmon;
    const char *verb;
    boolean youattack = (magr == &youmonst), youdefend =
        (mdef == &youmonst), resisted = FALSE, do_stun, do_confuse, result;
    int attack_indx, scare_dieroll = MB_MAX_DIEROLL / 2;

    result = FALSE;     /* no message given yet */
    /* the most severe effects are less likely at higher enchantment */
    if (mb->spe >= 3)
        scare_dieroll /= (1 << (mb->spe / 3));
    /* if target successfully resisted the artifact damage bonus, reduce
       overall likelihood of the assorted special effects */
    if (!spec_dbon_applies)
        dieroll += 1;

    /* might stun even when attempting a more severe effect, but in that case
       it will only happen if the other effect fails; extra damage will apply
       regardless; 3.4.1: sometimes might just probe even when it hasn't been
       enchanted */
    do_stun = (max(mb->spe, 0) < rn2(spec_dbon_applies ? 11 : 7));

    /* the special effects also boost physical damage; increments are generally 
       cumulative, but since the stun effect is based on a different criterium
       its damage might not be included; the base damage is either 1d4 (athame) 
       or 2d4 (athame+spec_dbon) depending on target's resistance check against 
       AD_STUN (handled by caller) [note that a successful save against AD_STUN 
       doesn't actually prevent the target from ending up stunned] */
    attack_indx = MB_INDEX_PROBE;
    *dmgptr += rnd(4);  /* (2..3)d4 */
    if (do_stun) {
        attack_indx = MB_INDEX_STUN;
        *dmgptr += rnd(4);      /* (3..4)d4 */
    }
    if (dieroll <= scare_dieroll) {
        attack_indx = MB_INDEX_SCARE;
        *dmgptr += rnd(4);      /* (3..5)d4 */
    }
    if (dieroll <= (scare_dieroll / 2)) {
        attack_indx = MB_INDEX_CANCEL;
        *dmgptr += rnd(4);      /* (4..6)d4 */
    }

    /* give the hit message prior to inflicting the effects */
    verb = mb_verb[! !Hallucination][attack_indx];
    if (youattack || youdefend || vis) {
        result = TRUE;
        pline(combat_msgc(magr, mdef, cr_hit),
              "The magic-absorbing %s %s %s!",
              mb->otyp == QUARTERSTAFF ? "staff" : "blade", /* old saves */
              vtense(NULL, verb), hittee);
        /* assume probing has some sort of noticeable feedback even if it is
           being done by one monster to another */
        if (attack_indx == MB_INDEX_PROBE)
            reveal_monster_at(mdef->mx, mdef->my, FALSE);
    }

    /* now perform special effects */
    switch (attack_indx) {
    case MB_INDEX_CANCEL:
        old_uasmon = youmonst.data;
        /* No mdef->mcan check: even a cancelled monster can be polymorphed
           into a golem, and the "cancel" effect acts as if some magical energy 
           remains in spellcasting defenders to be absorbed later. */
        if (!cancel_monst(mdef, mb, magr, FALSE, FALSE)) {
            resisted = TRUE;
        } else {
            do_stun = FALSE;
            if (youdefend) {
                if (youmonst.data != old_uasmon)
                    *dmgptr = 0;        /* rehumanized, so no more damage */
                if (youmonst.pwmax > 0) {
                    pline(msgc_intrloss, "You lose magical energy!");
                    youmonst.pwmax--;
                    if (youmonst.pw > 0)
                        youmonst.pw--;
                }
            } else {
                if (mdef->data == &mons[PM_CLAY_GOLEM])
                    mdef->mhp = 1;      /* cancelled clay golems will die */
                if (youattack && spellcaster(mdef->data)) {
                    pline(msgc_intrgain, "You absorb magical energy!");
                    youmonst.pwmax++;
                    youmonst.pw++;
                }
            }
        }
        break;

    case MB_INDEX_SCARE:
        if (youdefend) {
            if (Antimagic) {
                resisted = TRUE;
            } else {
                helpless(3, hr_afraid, "being scared stiff",
                         "You regain your composure.");
                if (magr && magr == u.ustuck && sticks(youmonst.data)) {
                    u.ustuck = NULL;
                    pline(msgc_statusend, "You release %s!", mon_nam(magr));
                }
            }
        } else {
            if (rn2(2) && resist(magr, mdef, WEAPON_CLASS, NOTELL, 0))
                resisted = TRUE;
            else
                monflee(mdef, 3, FALSE, (mdef->mhp > *dmgptr));
        }
        if (!resisted)
            do_stun = FALSE;
        break;

    case MB_INDEX_STUN:
        do_stun = TRUE; /* (this is redundant...) */
        break;

    case MB_INDEX_PROBE:
        if (youattack && (mb->spe == 0 || !rn2(3 * abs(mb->spe)))) {
            pline_implied(msgc_youdiscover, "The %s is insightful.", verb);
            /* pre-damage status */
            probe_monster(mdef);
        }
        break;
    }
    if (resists_stun(mdef))
        do_stun = FALSE;

    /* stun if that was selected and a worse effect didn't occur */
    if (do_stun) {
        inc_timeout(mdef, STUNNED, 3, TRUE);
        /* avoid extra stun message below if we used mb_verb["stun"] above */
        if (attack_indx == MB_INDEX_STUN)
            do_stun = FALSE;
    }
    /* lastly, all this magic can be confusing... */
    do_confuse = !rn2(12);
    if (do_confuse)
        inc_timeout(mdef, CONFUSION, 4, TRUE);

    if (youattack || youdefend || vis) {
        hittee = msgupcasefirst(hittee);
        if (resisted) {
            pline(combat_msgc(magr, mdef, cr_resist), "%s %s!", hittee,
                  vtense(hittee, "resist"));
            shieldeff(m_mx(mdef), m_my(mdef));
        }

        const char *buf = NULL;

        if (do_stun && do_confuse)
            buf = "stunned and confused";
        else if (do_stun)
            buf = "stunned";
        else if (do_confuse)
            buf = "confused";

        if (buf)
            pline(youdefend ? msgc_statusbad :
                  mdef->mtame ? msgc_petwarning : msgc_combatgood,
                  "%s %s %s%c", hittee, vtense(hittee, "are"), buf,
                  (do_stun && do_confuse) ? '!' : '.');
    }

    return result;
}


/* the tsurugi of muramasa or vorpal blade hit someone */
static boolean
artifact_hit_behead(struct monst *magr, struct monst *mdef, struct obj *otmp,
                    int *dmgptr, int dieroll)
{
    boolean youattack = (magr == &youmonst);
    boolean youdefend = (mdef == &youmonst);
    boolean vis = (youattack || youdefend ||
                   (magr && canseemon(magr)) ||
                   (mdef && canseemon(mdef)));

    const char *wepdesc;
    const char *hittee = youdefend ? "you" : mon_nam(mdef);

    uint64_t props = 0;
    if (!otmp->oartifact) {
        props = obj_properties(otmp);
        if (!(props & opm_vorpal)) {
            impossible("Running vorpal damage check on a non-vorpal weapon?");
            return FALSE;
        }
    }

    /* We really want "on a natural 20" but Nethack does it in reverse from
       AD&D. */
    if (otmp->oartifact == ART_TSURUGI_OF_MURAMASA && dieroll == 1) {
        wepdesc = "The razor-sharp blade";
        /* not really beheading, but so close, why add another SPFX */
        if (youattack && Engulfed && mdef == u.ustuck) {
            pline(msgc_combatalert, "You slice %s wide open!", mon_nam(mdef));
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
            return TRUE;
        }
        if (!youdefend) {
            /* allow normal cutworm() call to add extra damage */
            if (notonhead)
                return FALSE;

            if (bigmonst(mdef->data)) {
                if (youattack)
                    pline(msgc_combatalert, "You slice deeply into %s!",
                          mon_nam(mdef));
                else if (vis)
                    pline(msgc_combatalert, "%s cuts deeply into %s!",
                          Monnam(magr), hittee);
                *dmgptr *= 2;
                return TRUE;
            }
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
            pline(msgc_combatalert, "%s cuts %s in half!", wepdesc,
                  mon_nam(mdef));
            otmp->dknown = TRUE;
            return TRUE;
        } else {
            if (bigmonst(youmonst.data)) {
                pline(msgc_fatalavoid, "%s cuts deeply into you!",
                      magr ? Monnam(magr) : wepdesc);
                *dmgptr *= 2;
                return TRUE;
            }

            /* Players with negative ACs take less damage instead of just not
               getting hit.  We must add a large enough value to the damage so
               that this reduction in damage does not prevent death. */
            *dmgptr = 2 * (Upolyd ? u.mh : u.uhp) + FATAL_DAMAGE_MODIFIER;
            pline(msgc_fatal_predone, "%s cuts you in half!", wepdesc);
            otmp->dknown = TRUE;
            return TRUE;
        }
    } else if (((props & opm_vorpal) ||
                otmp->oartifact == ART_VORPAL_BLADE) &&
               (dieroll == 1 || mdef->data == &mons[PM_JABBERWOCK])) {
        if (vis && !otmp->oartifact)
            learn_oprop(otmp, opm_vorpal);

        static const char *const behead_msg[2] = {
            "%s beheads %s!",
            "%s decapitates %s!"
        };

        if (youattack && Engulfed && mdef == u.ustuck)
            return FALSE;
        if (!otmp->oartifact)
            wepdesc = "The weapon";
        else
            wepdesc = artilist[ART_VORPAL_BLADE].name;

        if (!youdefend) {
            if (!has_head(mdef->data) || notonhead || Engulfed) {
                if (youattack)
                    pline(msgc_combatalert, "Somehow, you miss %s wildly.",
                          mon_nam(mdef));
                else if (vis)
                    pline(msgc_fatalavoid, "Somehow, %s misses wildly.",
                          mon_nam(magr));
                *dmgptr = 0;
                return (boolean) (youattack || vis);
            }
            if (noncorporeal(mdef->data) || amorphous(mdef->data)) {
                pline(youattack ? msgc_combatalert : msgc_fatalavoid,
                      "%s slices through %s %s.", wepdesc,
                      s_suffix(mon_nam(mdef)), mbodypart(mdef, NECK));
                return TRUE;
            }
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
            pline(youattack ? msgc_combatalert : msgc_fatalavoid,
                  behead_msg[rn2(SIZE(behead_msg))], wepdesc, mon_nam(mdef));
            otmp->dknown = TRUE;
            return TRUE;
        } else {
            if (!has_head(youmonst.data)) {
                pline(msgc_fatalavoid, "Somehow, %s misses you wildly.",
                      magr ? mon_nam(magr) : wepdesc);
                *dmgptr = 0;
                return TRUE;
            }
            if (noncorporeal(youmonst.data) || amorphous(youmonst.data)) {
                pline(msgc_fatalavoid, "%s slices through your %s.", wepdesc,
                      body_part(NECK));
                return TRUE;
            }
            *dmgptr = 2 * (Upolyd ? u.mh : u.uhp)
                + FATAL_DAMAGE_MODIFIER;
            pline(msgc_fatal_predone, behead_msg[rn2(SIZE(behead_msg))],
                  wepdesc, "you");
            otmp->dknown = TRUE;
            /* Should amulets fall off? */
            return TRUE;
        }
    }
    return FALSE;
}


static boolean
artifact_hit_drainlife(struct monst *magr, struct monst *mdef, struct obj *otmp,
                       int *dmgptr)
{
    boolean youattack = (magr == &youmonst);
    boolean youdefend = (mdef == &youmonst);
    boolean vis = (youattack || youdefend ||
                   (magr && canseemon(magr)) ||
                   (mdef && canseemon(mdef)));

    /* drain life artifacts can't end up here, but weapon object
       properties stacking can result in this running when the
       defender resists drain life */
    if (resists_drli(mdef))
        return FALSE;

    if (vis && !otmp->oartifact)
        learn_oprop(otmp, opm_drain);

    enum msg_channel msgc = youattack ? msgc_combatalert :
        youdefend ? msgc_intrloss :
        (mdef->mtame && canspotmon(mdef)) ? msgc_petfatal : msgc_monneutral;

    if (!youdefend) {
        if (vis) {
            if (otmp->oartifact == ART_STORMBRINGER)
                pline(msgc, "The %s blade draws the life from %s!",
                      hcolor("black"), mon_nam(mdef));
            else
                pline(msgc, "%s draws the life from %s!",
                      The(distant_name(otmp, xname)), mon_nam(mdef));
        }
        mlosexp(magr, mdef, NULL, FALSE);
        return vis;
    } else {    /* youdefend */
        int oldhpmax = u.uhpmax;

        if (Blind)
            pline(msgc, "You feel an %s drain your life!",
                  otmp->oartifact ==
                  ART_STORMBRINGER ? "unholy blade" : "object");
        else if (otmp->oartifact == ART_STORMBRINGER)
            pline(msgc, "The %s blade drains your life!", hcolor("black"));
        else
            pline(msgc, "%s drains your life!", The(distant_name(otmp, xname)));
        losexp(msgcat("drained of life by ", otmp->oartifact ?
                      artiname(otmp->oartifact) : "a thirsty weapon"), FALSE);
        if (magr && magr->mhp < magr->mhpmax) {
            magr->mhp += (oldhpmax - u.uhpmax) / 2;
            if (magr->mhp > magr->mhpmax)
                magr->mhp = magr->mhpmax;
        }
        return TRUE;
    }
    return FALSE;
}


/* Function used when someone attacks someone else with an artifact weapon.
   Only adds the special (artifact) damage, and returns a 1 if it did something
   special (in which case the caller won't print the normal hit message). This
   should be called once upon every artifact attack; dmgval() no longer takes
   artifact bonuses into account. Possible extension: change the killer so that
   when an orc kills you with Stormbringer it's "killed by Stormbringer" instead
   of "killed by an orc". */
boolean
artifact_hit(struct monst *magr, struct monst *mdef, struct obj *otmp,
             int *dmgptr, int dieroll)  /* needed for Magicbane and
                                           vorpal blades */
{
    if (!otmp)
        return FALSE;
    uint64_t props = 0;
    if (!otmp->oartifact) {
        /* don't let armor of fire res do fire damage/similar */
        if (otmp->oclass != WEAPON_CLASS && !is_weptool(otmp))
            return FALSE;

        props = obj_properties(otmp);
        if (!(props & (opm_fire | opm_frost | opm_shock |
                       opm_drain | opm_vorpal)))
            return FALSE;
    }

    boolean uagr = (magr == &youmonst);
    boolean udef = (mdef == &youmonst);
    boolean vis = (uagr || udef ||
                   (magr && canseemon(magr)) ||
                   (mdef && canseemon(mdef)));
    boolean res = FALSE;
    const char *hittee = mon_nam(mdef);
    int spec_dbon_applies = 0;

    /* The following takes care of most of the damage, but not all-- the
       exception being for level draining, which is specially handled. Messages 
       are done in this function, however. */
    *dmgptr += spec_dbon(otmp, mdef, *dmgptr, &spec_dbon_applies);

    if (uagr && udef) {
        impossible("attacking yourself with weapon?");
        return FALSE;
    }

    /* the four basic attacks: fire, cold, shock and missiles */
    if (attacks(AD_FIRE, otmp)) {
        if (vis)
            pline(combat_msgc(magr, mdef, cr_hit), "The %s %s %s%c",
                  otmp->oartifact ? "fiery blade" : "weapon",
                  !spec_dbon_applies ? "hits" : (mdef->data ==
                                                &mons[PM_WATER_ELEMENTAL]) ?
                  "vaporizes part of" : "burns", mon_nam(mdef),
                  !spec_dbon_applies ? '.' : '!');

        if (!rn2(4))
            *dmgptr += destroy_mitem(mdef, POTION_CLASS, AD_FIRE, NULL);
        if (!rn2(4))
            *dmgptr += destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE, NULL);
        if (!rn2(7))
            *dmgptr += destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE, NULL);
        if (sliming(mdef))
            burn_away_slime(mdef);
        if (otmp->oartifact)
            return vis;
        if (vis) {
            learn_oprop(otmp, opm_fire);
            res = TRUE;
        }
    }
    if (attacks(AD_COLD, otmp)) {
        if (vis)
            pline(combat_msgc(magr, mdef, cr_hit), "The %s %s %s%c",
                  otmp->oartifact ? "ice-cold blade" : "weapon",
                  !spec_dbon_applies ? "hits" : "freezes",
                  mon_nam(mdef), !spec_dbon_applies ? '.' : '!');
        if (!rn2(4))
            *dmgptr += destroy_mitem(mdef, POTION_CLASS, AD_COLD, NULL);
        if (otmp->oartifact)
            return vis;
        if (vis) {
            learn_oprop(otmp, opm_frost);
            res = TRUE;
        }
    }
    if (attacks(AD_ELEC, otmp)) {
        if (vis) {
            if (immune_to_elec(mdef))
                pline(combat_msgc(magr, mdef, cr_immune),
                      "Lightning strikes %s, but %s%s unfazed.",
                      mon_nam(mdef), udef ? "you" : mhe(mdef),
                      udef ? "'re" : " seems");
            else {
                if (otmp->oartifact)
                    pline_implied(combat_msgc(magr, mdef, cr_hit),
                                  "The massive hammer hits!");
                pline(combat_msgc(magr, mdef, cr_hit),
                      "Lightning strikes %s!", mon_nam(mdef));
            }
        }

        if (!rn2(5))
            *dmgptr += destroy_mitem(mdef, WAND_CLASS, AD_ELEC, NULL);
        if (otmp->oartifact)
            return vis;
        if (vis) {
            learn_oprop(otmp, opm_shock);
            res = TRUE;
        }
    }
    if (attacks(AD_MAGM, otmp)) {
        impossible("artifact (or object property) with magic damage?");
        if (vis)
            pline(combat_msgc(magr, mdef, cr_hit),
                  "The imaginary widget hits%s %s%c",
                  !spec_dbon_applies ? "" :
                  "!  A hail of magic missiles strikes",
                  mon_nam(mdef), !spec_dbon_applies ? '.' : '!');
        return vis;
    }

    if (attacks(AD_STUN, otmp) && dieroll <= MB_MAX_DIEROLL) {
        /* Magicbane's special attacks (possibly modifies hittee[]) */
        return magicbane_hit(magr, mdef, otmp, dmgptr, dieroll, vis,
                             hittee, spec_dbon_applies);
    }

    if (!spec_dbon_applies) {
        /* since damage bonus didn't apply, nothing more to do; no further
           attacks have side-effects on inventory */
        return res;
    }

    /* Tsurugi of Muramasa, Vorpal Blade */
    if ((props & opm_vorpal) ||
        (otmp->oartifact && spec_ability(otmp, SPFX_BEHEAD))) {
        res = artifact_hit_behead(magr, mdef, otmp, dmgptr,
                                  dieroll);
        if (otmp->oartifact)
            return res;
    }

    /* Stormbringer */
    if ((props & opm_drain) ||
        (otmp->oartifact && spec_ability(otmp, SPFX_DRLI))) {
        res = (artifact_hit_drainlife(magr, mdef, otmp,
                                      dmgptr) || res);
        if (otmp->oartifact)
            return res;
    }

    return res;
}

static const char recharge_type[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

/* #invoke: an "ugly check" filters out most objects */
static const char invoke_types[] = { ALL_CLASSES, 0 };

int
doinvoke(const struct nh_cmd_arg *arg)
{
    struct obj *obj;

    obj = getargobj(arg, invoke_types, "invoke, break, or rub");
    if (!obj)
        return 0;

    if (obj->oartifact && !touch_artifact(obj, &youmonst))
        return 1;

    const struct artifact *oart = get_artifact(obj);

    if (!oart || !oart->inv_prop) {
        if (obj->oclass == WAND_CLASS)
            return do_break_wand(obj, TRUE);
        else if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
                 obj->otyp == BRASS_LANTERN)
            return dorub(&(struct nh_cmd_arg){.argtype = CMD_ARG_OBJ,
                                              .invlet = obj->invlet});
        else if (obj->otyp == CRYSTAL_BALL)
            use_crystal_ball(obj);
        else if (obj->otyp == SPE_BOOK_OF_THE_DEAD)
            deadbook(obj, TRUE); /* deadbook() handles nothing happening */
        else /* both msgc_cancelled1 and msgc_mispaste; send msgc_mispaste to
                the client as that's what it cares about, this comment mentions
                msgc_cancelled1 so that it'll be found in a grep */
            pline(msgc_mispaste, "Nothing happens.");
        return 1;
    }

    return arti_invoke(obj);
}

/* Turns off any toggled-by-invoking artifact powers that this artifact might
   have. The caller should verify that the item in question is actually an
   artifact, but does not need to verify that it's been invoked. */
void
uninvoke_artifact(struct obj *obj)
{
    if (!(obj->owornmask & W_MASK(os_invoked)))
        return;
    arti_invoke(obj);
}

static int
arti_invoke(struct obj *obj)
{
    if (!obj) {
        impossible("invoking nothing?");
        return 0;
    }

    const struct artifact *oart = get_artifact(obj);

    if (oart->inv_prop > LAST_PROP) {
        /* It's a special power, not "just" a property */
        if (obj->age > moves) {
            /* the artifact is tired :-) */
            pline(msgc_failrandom, "You feel that %s %s ignoring you.",
                  the(xname(obj)), otense(obj, "are"));
            /* and just got more so; patience is essential... */
            obj->age += (long)dice(3, 10);
            return 1;
        }
        obj->age = moves + rnz_on_rng(100, rng_artifact_invoke);

        switch (oart->inv_prop) {
        case SELF_UNCURSE:
            if (obj->cursed) {
                uncurse(obj);
                pline(msgc_itemrepair, "%s %s %s",
                       The(xname(obj)), aobjnam(obj, "softly glow"),
                       hcolor("amber"));
            }
            break;
        case TAMING:{
                struct obj *pseudo = mktemp_sobj(NULL, SCR_TAMING);
                boolean unused_known;

                seffects(&youmonst, pseudo, &unused_known);
                obfree(pseudo, NULL);
                break;
            }
        case HEALING:{
                int healamt = (u.uhpmax + 1 - u.uhp) / 2;

                if (Upolyd)
                    healamt = (u.mhmax + 1 - u.mh) / 2;
                if (healamt || sick(&youmonst) || sliming(&youmonst) ||
                    property_timeout(&youmonst, BLINDED))
                    pline(msgc_statusheal, "You feel better.");
                else
                    goto nothing_special;
                if (healamt > 0) {
                    if (Upolyd)
                        u.mh += healamt;
                    else
                        u.uhp += healamt;
                }
                if (sick(&youmonst))
                    set_property(&youmonst, SICK, -2, FALSE);
                if (zombifying(&youmonst))
                    set_property(&youmonst, ZOMBIE, -2, FALSE);
                if (sliming(&youmonst))
                    set_property(&youmonst, SLIMED, -2, FALSE);
                if (blind(&youmonst))
                    set_property(&youmonst, BLINDED, -2, FALSE);
                break;
            }
        case ENERGY_BOOST:{
                int epboost = (youmonst.pwmax + 1 - youmonst.pw) / 2;

                if (epboost > 120)
                    epboost = 120;      /* arbitrary */
                else if (epboost < 12)
                    epboost = youmonst.pwmax - youmonst.pw;
                if (epboost) {
                    pline(msgc_statusgood, "You feel re-energized.");
                    youmonst.pw += epboost;
                } else
                    goto nothing_special;
                break;
            }
        case UNTRAP:{
                if (!untrap(&(struct nh_cmd_arg){.argtype = 0}, TRUE)) {
                    obj->age = 0;       /* don't charge for changing their mind 
                                         */
                    return 0;
                }
                break;
            }
        case CHARGE_OBJ:{
                struct obj *otmp = getobj(recharge_type, "charge", FALSE);
                boolean b_effect;

                if (!otmp) {
                    obj->age = 0;
                    return 0;
                }
                b_effect = obj->blessed && (Role_switch == oart->role ||
                                            !oart->role);
                recharge(&youmonst, otmp, b_effect ? 1 : obj->cursed ? -1 : 0);
                update_inventory();
                break;
            }
        case LEV_TELE:
            level_tele();
            break;
        case CREATE_PORTAL:{
                int i, num_ok_dungeons, last_ok_dungeon = 0;
                d_level newlev;
                struct nh_menulist menu;

                init_menulist(&menu);

                num_ok_dungeons = 0;
                for (i = 0; i < n_dgns; i++) {
                    if (!gamestate.dungeons[i].dunlev_ureached)
                        continue;

                    add_menu_item(&menu, i + 1, gamestate.dungeons[i].dname,
                                  0, FALSE);
                    num_ok_dungeons++;
                    last_ok_dungeon = i;
                }

                if (num_ok_dungeons > 1) {
                    /* more than one entry; display menu for choices */
                    int n;
                    const int *selected;

                    n = display_menu(&menu,
                                     "Open a portal to which dungeon?",
                                     PICK_ONE, PLHINT_ANYWHERE, &selected);
                    if (n <= 0)
                        goto nothing_special;

                    i = selected[0] - 1;
                } else {
                    dealloc_menulist(&menu);
                    i = last_ok_dungeon;     /* also first & only OK dungeon */
                }

                /* 
                 * i is now index into dungeon structure for the new dungeon.
                 * Find the closest level in the given dungeon, open
                 * a use-once portal to that dungeon and go there.
                 * The closest level is either the entry or dunlev_ureached.
                 */
                newlev.dnum = i;
                if (gamestate.dungeons[i].depth_start >= depth(&u.uz))
                    newlev.dlevel = gamestate.dungeons[i].entry_lev;
                else
                    newlev.dlevel = gamestate.dungeons[i].dunlev_ureached;
                if (Uhave_amulet || In_endgame(&u.uz) || In_endgame(&newlev)
                    || newlev.dnum == u.uz.dnum) {
                    pline(msgc_cancelled1,
                          "You feel very disoriented for a moment.");
                } else {
                    if (!Blind)
                        pline_implied(
                            msgc_branchchange,
                            "You are surrounded by a shimmering sphere!");
                    else
                        pline_implied(msgc_branchchange,
                                      "You feel weightless for a moment.");
                    goto_level(&newlev, FALSE, FALSE, FALSE);
                }
                break;
            }
        case ENLIGHTENING:
            enlightenment(0);
            break;
        case CREATE_AMMO:{
                struct obj *otmp =
                    mksobj(level, ARROW, TRUE, FALSE, rng_main);

                if (!otmp)
                    goto nothing_special;
                otmp->blessed = obj->blessed;
                otmp->cursed = obj->cursed;
                otmp->bknown = obj->bknown;
                if (obj->blessed) {
                    if (otmp->spe < 0)
                        otmp->spe = 0;
                    otmp->quan += rnd(10);
                } else if (obj->cursed) {
                    if (otmp->spe > 0)
                        otmp->spe = 0;
                } else
                    otmp->quan += rnd(5);
                otmp->owt = weight(otmp);
                hold_another_object(otmp, "Suddenly %s out.",
                                    aobjnam(otmp, "fall"), NULL);
                break;
            }
        }
    } else {
        /* on is true if invoked prop is about to be set */
        boolean on = !(obj->owornmask & W_MASK(os_invoked));
        boolean redundant = m_has_property(&youmonst, oart->inv_prop,
                                           ANY_PROPERTY, TRUE);

        if (on && obj->age > moves) {
            /* the artifact is tired :-) */
            pline(msgc_failrandom, "You feel that %s %s ignoring you.",
                  the(xname(obj)), otense(obj, "are"));
            /* can't just keep repeatedly trying */
            obj->age += (long)dice(3, 10);
            return 1;
        } else if (!on) {
            /* when turning off property, determine downtime */
            /* arbitrary for now until we can tune this -dlc */
            obj->age = moves + rnz_on_rng(100, rng_artifact_invoke);
        }
        obj->owornmask ^= W_MASK(os_invoked);

        if (redundant) {
        nothing_special:
            /* you had the property from some other source too */
            if (carried(obj))
                pline(msgc_statusextend, "You feel a surge of power, "
                      "but nothing seems to happen.");
            return 1;
        }
        switch (oart->inv_prop) {
        case CONFLICT:
            if (on)
                pline(msgc_statusgood, "You feel like a rabble-rouser.");
            else
                pline(msgc_statusend,
                      "You feel the tension decrease around you.");
            break;
        case LEVITATION:
            if (on) {
                /* first is to override the timer since it kills any
                   intrinsic levi */
                set_property(&youmonst, LEVITATION, -2, TRUE);
                set_property(&youmonst, LEVITATION, 0, FALSE);
                spoteffects(FALSE);
            } else
                set_property(&youmonst, LEVITATION, -2, FALSE);
            break;
        case INVIS:
            if (BInvis || Blind)
                goto nothing_special;
            newsym(u.ux, u.uy);
            if (on)
                pline(msgc_statusgood,
                      "Your body takes on a %s transparency...",
                      Hallucination ? "normal" : "strange");
            else
                pline(msgc_statusend, "Your body seems to unfade...");
            break;
        }
    }

    return 1;
}


/* WAC return TRUE if artifact is always lit */
boolean
artifact_light(const struct obj * obj)
{
    return get_artifact(obj) && obj->oartifact == ART_SUNSWORD;
}

/* KMH -- Talking artifacts are finally implemented */
void
arti_speak(struct obj *obj)
{
    const struct artifact *oart = get_artifact(obj);
    const char *line;
    int truth;

    /* Is this a speaking artifact? */
    if (!oart || !(oart->spfx & SPFX_SPEAK))
        return;

    line = getrumor(bcsign(obj), TRUE, &truth, rng_main);
    if (truth)
        exercise(A_WIS, truth == 1);
    if (!*line)
        line = "NetHack rumors file closed for renovation.";
    pline_implied(msgc_npcvoice, "%s:", Tobjnam(obj, "whisper"));
    verbalize(msgc_rumor, "%s", line);
    return;
}

boolean
artifact_has_invprop(struct obj * otmp, uchar inv_prop)
{
    const struct artifact *arti = get_artifact(otmp);

    return (boolean) (arti && (arti->inv_prop == inv_prop));
}

/* Return the price sold to the hero of a given artifact or unique item */
long
arti_cost(const struct obj *otmp)
{
    if (!otmp->oartifact)
        return (long)objects[otmp->otyp].oc_cost;
    else if (artilist[(int)otmp->oartifact].cost)
        return artilist[(int)otmp->oartifact].cost;
    else
        return 100L * (long)objects[otmp->otyp].oc_cost;
}

/*artifact.c*/

