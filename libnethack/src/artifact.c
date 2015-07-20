/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#define ARTIFACT_C
#include "artilist.h"
#include "lev.h"

/*
 * Note:  both artilist[] and artiexist[] have a dummy element #0,
 *        so loops over them should normally start at #1.  The primary
 *        exception is the save & restore code, which doesn't care about
 *        the contents, just the total size.
 */

#define get_artifact(o) \
                (((o)&&(o)->oartifact) ? &artilist[(int) (o)->oartifact] : 0)

static int spec_applies(const struct artifact *, const struct monst *);
static int arti_invoke(struct obj *);
static boolean magicbane_hit(struct monst *magr, struct monst *mdef,
                             struct obj *, int *, int, boolean, const char *);
static long spec_m2(struct obj *);

/* The amount added to the victim's total hit points to insure that the
   victim will be killed even after damage bonus/penalty adjustments.
   Most such penalties are small, and 200 is plenty; the exception is
   half physical damage.  3.3.1 and previous versions tried to use a very
   large number to account for this case; now, we just compute the fatal
   damage by adding it to 2 times the total hit points instead of 1 time.
   Note: this will still break if they have more than about half the number
   of hit points that will fit in a 15 bit integer. */
#define FATAL_DAMAGE_MODIFIER 200

/* coordinate effects from spec_dbon() with messages in artifact_hit() */
static int spec_dbon_applies = 0;

/* flags including which artifacts have already been created */
static boolean artiexist[1 + NROFARTIFACTS + 1];

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
    memset(artiexist, 0, sizeof artiexist);
    memset(artidisco, 0, sizeof artidisco);
    hack_artifacts();
}

void
save_artifacts(struct memfile *mf)
{
    /* artiexist and artidisco are arrays of bytes, so writing them in one go
       is safe and portable */
    mtag(mf, 0, MTAG_ARTIFACT);
    mwrite(mf, artiexist, sizeof artiexist);
    mwrite(mf, artidisco, sizeof artidisco);
}

void
restore_artifacts(struct memfile *mf)
{
    mread(mf, artiexist, sizeof artiexist);
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
            (!(a->spfx & SPFX_NOGEN) || unique) && !artiexist[m]) {
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
                (!(a->spfx & SPFX_NOGEN) || unique) && !artiexist[m]) {
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
        otmp->oartifact = m;
        artiexist[m] = TRUE;
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
        for (a = artilist + 1, arex = artiexist + 1; a->otyp; a++, arex++)
            if ((int)a->otyp == otyp && !strcmp(a->name, name))
                return *arex;
    return FALSE;
}

void
artifact_exists(struct obj *otmp, const char *name, boolean mod)
{
    const struct artifact *a;

    if (otmp && *name)
        for (a = artilist + 1; a->otyp; a++)
            if (a->otyp == otmp->otyp && !strcmp(a->name, name)) {
                int m = a - artilist;

                otmp->oartifact = (char)(mod ? m : 0);
                otmp->age = 0;
                if (otmp->otyp == RIN_INCREASE_DAMAGE)
                    otmp->spe = 0;
                artiexist[m] = mod;
                break;
            }
    return;
}

int
nartifact_exist(void)
{
    int a = 0;
    int n = SIZE(artiexist);

    while (n > 1)
        if (artiexist[--n])
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


/* Given an item and an extrinsic, returns whether that item provides that
   extrinsic, either through artifact properties or through a "normal" item
   property. In the special case of warning about a particular type of monster,
   returns that in *warntype.  Returns the artifact's worn mask (if it's
   relevant), bitwise-or W_MASK(os_carried), bitwise-or W_MASK(os_invoked) if
   relevant. */
long
item_provides_extrinsic(struct obj *otmp, int extrinsic, int *warntype)
{
    const struct artifact *oart = get_artifact(otmp);
    uchar dtyp;
    long spfx;
    boolean equipped;
    long mask;    

    /* Artifacts can provide elemental defense, either when equipped, or when
       carried. Non-artifact items (and artifact properties that depend on their
       base item) only work when equipped. Our definition of "equipped" depends
       on the base type; wearing is always enough, wielding is only enough for
       weapons and weapon-tools.

       TODO: Wielding as the secondary weapon while twoweaponing. This currently
       isn't allowed for artifacts, and never comes up for non-artifacts (no
       non-artifact weapons currently have properties), but it's unclear whether
       that's for technical or balance reasons. We should at least fix the
       technical issues here, even though balance considerations imply that it
       probably shouldn't be allowed anyway. */
    mask = W_WORN | W_MASK(os_invoked);
    if (otmp->oclass == WEAPON_CLASS || is_weptool(otmp))
        mask |= W_MASK(os_wep);
    mask &= otmp->owornmask;
    equipped = !!(mask & W_EQUIP);

    /* Does the base item (artifact or not) provide the property in question?
       Skip WARN_OF_MON for paranoia reasons; it wouldn't work if it were
       used, because there'd be no way to communicate which monster is being
       warned against. */
    if (objects[otmp->otyp].oc_oprop == extrinsic && extrinsic != WARN_OF_MON)
        return mask & W_EQUIP;

    /* Non-artifact item properties go here. At the present:

       - alchemy smocks give two extrinsics, and so only one can be placed in
         its item property field, with the other one being special-cased;

       - the Amulet of Yendor is not an artifact but grants clairvoyance when
         carried */
    if (otmp->otyp == ALCHEMY_SMOCK && extrinsic == ACID_RES)
        return mask & W_EQUIP;
    if (otmp->otyp == AMULET_OF_YENDOR && extrinsic == CLAIRVOYANT)
        return mask | W_MASK(os_carried);

    if (!oart)
        return 0L;

    if (oart->inv_prop == extrinsic && mask & W_MASK(os_invoked))
        return mask;

    /* Anything from here on is based on artifact properties */
    mask |= W_MASK(os_carried);

    dtyp = oart->cary.adtyp;
 
    if ((dtyp == AD_FIRE && extrinsic == FIRE_RES) ||
        (dtyp == AD_COLD && extrinsic == COLD_RES) ||
        (dtyp == AD_ELEC && extrinsic == SHOCK_RES) ||
        (dtyp == AD_MAGM && extrinsic == ANTIMAGIC) ||
        (dtyp == AD_DISN && extrinsic == DISINT_RES) ||
        (dtyp == AD_DRST && extrinsic == POISON_RES) ||
        (dtyp == AD_DISE && extrinsic == SICK_RES))
        return mask;

    if (equipped) {
        dtyp = oart->defn.adtyp;

        if ((dtyp == AD_FIRE && extrinsic == FIRE_RES) ||
            (dtyp == AD_COLD && extrinsic == COLD_RES) ||
            (dtyp == AD_ELEC && extrinsic == SHOCK_RES) ||
            (dtyp == AD_MAGM && extrinsic == ANTIMAGIC) ||
            (dtyp == AD_DISN && extrinsic == DISINT_RES) ||
            (dtyp == AD_DRST && extrinsic == POISON_RES) ||
            (dtyp == AD_DISE && extrinsic == SICK_RES))
            return mask;
    }

    /* extrinsics from the spfx field; there could be more than one */
    spfx = oart->cspfx;
 
    if (equipped)
        spfx |= oart->spfx;

    /* TODO: this function used to print a message for hallucination
       resistance, but it's definitely the wrong place. Discover where the
       correct place is. */
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
        (spfx & SPFX_XRAY    && extrinsic == XRAY_VISION))
        return mask;

    if (spfx & SPFX_WARN) {
        if (spec_m2(otmp)) {
            if (extrinsic == WARN_OF_MON) {
                *warntype = spec_m2(otmp);
                return mask;
            }
        } else {
            if (extrinsic == WARNING)
                return mask;
        }
    }

    return 0L;
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
        badalign = ! !spec_applies(&tmp, mon);
    }

    if (((badclass || badalign) && self_willed) ||
        (badalign && (!yours || !rn2(4)))) {
        int dmg;
        const char *buf;

        if (!yours)
            return 0;
        pline("You are blasted by %s power!", s_suffix(the(xname(obj))));
        dmg = dice((Antimagic ? 2 : 4), (self_willed ? 10 : 4));
        buf = msgprintf("touching %s", oart->name);
        losehp(dmg, killer_msg(DIED, buf));
        exercise(A_WIS, FALSE);
    }

    /* can pick it up unless you're totally non-synch'd with the artifact */
    if (badclass && badalign && self_willed) {
        if (yours)
            pline("%s your grasp!", Tobjnam(obj, "evade"));
        return 0;
    }

    return 1;
}


/* decide whether an artifact's special attacks apply against mtmp */
static int
spec_applies(const struct artifact *weap, const struct monst *mtmp)
{
    const struct permonst *ptr;
    boolean yours;

    if (!(weap->spfx & (SPFX_DBONUS | SPFX_ATTK)))
        return weap->attk.adtyp == AD_PHYS;

    yours = (mtmp == &youmonst);
    ptr = mtmp->data;

    if (weap->spfx & SPFX_DMONS) {
        return ptr == &mons[(int)weap->mtype];
    } else if (weap->spfx & SPFX_DCLAS) {
        return weap->mtype == (unsigned long)ptr->mlet;
    } else if (weap->spfx & SPFX_DFLAG1) {
        return (ptr->mflags1 & weap->mtype) != 0L;
    } else if (weap->spfx & SPFX_DFLAG2) {
        return ((ptr->mflags2 & weap->mtype) ||
                (yours &&
                 ((!Upolyd && (urace.selfmask & weap->mtype)) ||
                  ((weap->mtype & M2_WERE) && u.ulycn >= LOW_PM))));
    } else if (weap->spfx & SPFX_DALIGN) {
        return yours ? (u.ualign.type != weap->alignment) :
          (ptr->maligntyp == A_NONE || sgn(ptr->maligntyp) != weap->alignment);
    } else if (weap->spfx & SPFX_ATTK) {
        struct obj *defending_weapon = (yours ? uwep : MON_WEP(mtmp));

        if (defending_weapon && defending_weapon->oartifact &&
            defends((int)weap->attk.adtyp, defending_weapon))
            return FALSE;
        switch (weap->attk.adtyp) {
        case AD_FIRE:
            return !(yours ? Fire_resistance : resists_fire(mtmp));
        case AD_COLD:
            return !(yours ? Cold_resistance : resists_cold(mtmp));
        case AD_ELEC:
            return !(yours ? Shock_resistance : resists_elec(mtmp));
        case AD_MAGM:
        case AD_STUN:
            return !(yours ? Antimagic : (rn2(100) < ptr->mr));
        case AD_DRST:
            return !(yours ? Poison_resistance : resists_poison(mtmp));
        case AD_DRLI:
            return !(yours ? Drain_resistance : resists_drli(mtmp));
        case AD_STON:
            return !(yours ? Stone_resistance : resists_ston(mtmp));
        default:
            impossible("Weird weapon special attack.");
        }
    }
    return 0;
}

/* return the M2 flags of monster that an artifact's special attacks apply
 * against */
static long
spec_m2(struct obj *otmp)
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
spec_dbon(struct obj *otmp, struct monst *mon, int tmp)
{
    const struct artifact *weap = get_artifact(otmp);

    if (!weap || (weap->attk.adtyp == AD_PHYS &&       /* check for `NO_ATTK' */
                  weap->attk.damn == 0 && weap->attk.damd == 0))
        spec_dbon_applies = FALSE;
    else
        spec_dbon_applies = spec_applies(weap, mon);

    if (spec_dbon_applies)
        return weap->attk.damd ? rnd((int)weap->attk.damd) : max(tmp, 1);
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
              const char *hittee    /* target's name: "you" or mon_nam(mdef) */
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
        pline("The magic-absorbing blade %s %s!", vtense(NULL, verb), hittee);
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
        if (!cancel_monst(mdef, mb, youattack, FALSE, FALSE)) {
            resisted = TRUE;
        } else {
            do_stun = FALSE;
            if (youdefend) {
                if (youmonst.data != old_uasmon)
                    *dmgptr = 0;        /* rehumanized, so no more damage */
                if (u.uenmax > 0) {
                    pline("You lose magical energy!");
                    u.uenmax--;
                    if (u.uen > 0)
                        u.uen--;
                }
            } else {
                if (mdef->data == &mons[PM_CLAY_GOLEM])
                    mdef->mhp = 1;      /* cancelled clay golems will die */
                if (youattack && attacktype(mdef->data, AT_MAGC)) {
                    pline("You absorb magical energy!");
                    u.uenmax++;
                    u.uen++;
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
                    pline("You release %s!", mon_nam(magr));
                }
            }
        } else {
            if (rn2(2) && resist(mdef, WEAPON_CLASS, 0, NOTELL))
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
            pline("The %s is insightful.", verb);
            /* pre-damage status */
            probe_monster(mdef);
        }
        break;
    }
    /* stun if that was selected and a worse effect didn't occur */
    if (do_stun) {
        if (youdefend)
            make_stunned((HStun + 3), FALSE);
        else
            mdef->mstun = 1;
        /* avoid extra stun message below if we used mb_verb["stun"] above */
        if (attack_indx == MB_INDEX_STUN)
            do_stun = FALSE;
    }
    /* lastly, all this magic can be confusing... */
    do_confuse = !rn2(12);
    if (do_confuse) {
        if (youdefend)
            make_confused(HConfusion + 4, FALSE);
        else
            mdef->mconf = 1;
    }

    if (youattack || youdefend || vis) {
        hittee = msgupcasefirst(hittee);
        if (resisted) {
            pline("%s %s!", hittee, vtense(hittee, "resist"));
            shieldeff(m_mx(mdef), m_my(mdef));
        }
        if (flags.verbose) {
            const char *buf = NULL;

            if (do_stun && do_confuse)
                buf = "stunned and confused";
            else if (do_stun)
                buf = "stunned";
            else if (do_confuse)
                buf = "confused";

            if (buf)
                pline("%s %s %s%c", hittee, vtense(hittee, "are"), buf,
                      (do_stun && do_confuse) ? '!' : '.');
        }
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
    boolean vis = (!youattack && magr && cansee(magr->mx, magr->my))
        || (!youdefend && cansee(mdef->mx, mdef->my))
        || (youattack && Engulfed && mdef == u.ustuck && !Blind);
    const char *wepdesc;
    const char *hittee = youdefend ? "you" : mon_nam(mdef);

    /* We really want "on a natural 20" but Nethack does it in reverse from
       AD&D. */
    if (otmp->oartifact == ART_TSURUGI_OF_MURAMASA && dieroll == 1) {
        wepdesc = "The razor-sharp blade";
        /* not really beheading, but so close, why add another SPFX */
        if (youattack && Engulfed && mdef == u.ustuck) {
            pline("You slice %s wide open!", mon_nam(mdef));
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
            return TRUE;
        }
        if (!youdefend) {
            /* allow normal cutworm() call to add extra damage */
            if (notonhead)
                return FALSE;

            if (bigmonst(mdef->data)) {
                if (youattack)
                    pline("You slice deeply into %s!", mon_nam(mdef));
                else if (vis)
                    pline("%s cuts deeply into %s!", Monnam(magr), hittee);
                *dmgptr *= 2;
                return TRUE;
            }
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
            pline("%s cuts %s in half!", wepdesc, mon_nam(mdef));
            otmp->dknown = TRUE;
            return TRUE;
        } else {
            if (bigmonst(youmonst.data)) {
                pline("%s cuts deeply into you!",
                      magr ? Monnam(magr) : wepdesc);
                *dmgptr *= 2;
                return TRUE;
            }

            /* Players with negative ACs take less damage instead of just not
               getting hit.  We must add a large enough value to the damage so
               that this reduction in damage does not prevent death. */
            *dmgptr = 2 * (Upolyd ? u.mh : u.uhp) + FATAL_DAMAGE_MODIFIER;
            pline("%s cuts you in half!", wepdesc);
            otmp->dknown = TRUE;
            return TRUE;
        }
    } else if (otmp->oartifact == ART_VORPAL_BLADE &&
               (dieroll == 1 || mdef->data == &mons[PM_JABBERWOCK])) {
        static const char *const behead_msg[2] = {
            "%s beheads %s!",
            "%s decapitates %s!"
        };

        if (youattack && Engulfed && mdef == u.ustuck)
            return FALSE;
        wepdesc = artilist[ART_VORPAL_BLADE].name;
        if (!youdefend) {
            if (!has_head(mdef->data) || notonhead || Engulfed) {
                if (youattack)
                    pline("Somehow, you miss %s wildly.", mon_nam(mdef));
                else if (vis)
                    pline("Somehow, %s misses wildly.", mon_nam(magr));
                *dmgptr = 0;
                return (boolean) (youattack || vis);
            }
            if (noncorporeal(mdef->data) || amorphous(mdef->data)) {
                pline("%s slices through %s %s.", wepdesc,
                      s_suffix(mon_nam(mdef)), mbodypart(mdef, NECK));
                return TRUE;
            }
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
            pline(behead_msg[rn2(SIZE(behead_msg))], wepdesc, mon_nam(mdef));
            otmp->dknown = TRUE;
            return TRUE;
        } else {
            if (!has_head(youmonst.data)) {
                pline("Somehow, %s misses you wildly.",
                      magr ? mon_nam(magr) : wepdesc);
                *dmgptr = 0;
                return TRUE;
            }
            if (noncorporeal(youmonst.data) || amorphous(youmonst.data)) {
                pline("%s slices through your %s.", wepdesc, body_part(NECK));
                return TRUE;
            }
            *dmgptr = 2 * (Upolyd ? u.mh : u.uhp)
                + FATAL_DAMAGE_MODIFIER;
            pline(behead_msg[rn2(SIZE(behead_msg))], wepdesc, "you");
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
    boolean vis = (!youattack && magr && cansee(magr->mx, magr->my))
        || (!youdefend && cansee(mdef->mx, mdef->my))
        || (youattack && Engulfed && mdef == u.ustuck && !Blind);

    if (!youdefend) {
        if (vis) {
            if (otmp->oartifact == ART_STORMBRINGER)
                pline("The %s blade draws the life from %s!", hcolor("black"),
                      mon_nam(mdef));
            else
                pline("%s draws the life from %s!",
                      The(distant_name(otmp, xname)), mon_nam(mdef));
        }
        if (mdef->m_lev == 0) {
            *dmgptr = 2 * mdef->mhp + FATAL_DAMAGE_MODIFIER;
        } else {
            int drain = rnd(8);

            *dmgptr += drain;
            mdef->mhpmax -= drain;
            mdef->m_lev--;
            drain /= 2;
            if (drain)
                healup(drain, 0, FALSE, FALSE);
        }
        return vis;
    } else {    /* youdefend */
        int oldhpmax = u.uhpmax;

        if (Blind)
            pline("You feel an %s drain your life!",
                  otmp->oartifact ==
                  ART_STORMBRINGER ? "unholy blade" : "object");
        else if (otmp->oartifact == ART_STORMBRINGER)
            pline("The %s blade drains your life!", hcolor("black"));
        else
            pline("%s drains your life!", The(distant_name(otmp, xname)));
        losexp(msgcat("drained of life by ", artiname(otmp->oartifact)), FALSE);
        if (magr && magr->mhp < magr->mhpmax) {
            magr->mhp += (oldhpmax - u.uhpmax) / 2;
            if (magr->mhp > magr->mhpmax)
                magr->mhp = magr->mhpmax;
        }
        return TRUE;
    }
    return FALSE;
}


/* Function used when someone attacks someone else with an artifact
 * weapon.  Only adds the special (artifact) damage, and returns a 1 if it
 * did something special (in which case the caller won't print the normal
 * hit message).  This should be called once upon every artifact attack;
 * dmgval() no longer takes artifact bonuses into account.  Possible
 * extension: change the killer so that when an orc kills you with
 * Stormbringer it's "killed by Stormbringer" instead of "killed by an orc".
 */
boolean
artifact_hit(struct monst * magr, struct monst * mdef, struct obj * otmp,
             int *dmgptr, int dieroll)  /* needed for Magicbane and
                                           vorpal blades */
{
    boolean youattack = (magr == &youmonst);
    boolean youdefend = (mdef == &youmonst);
    boolean vis = (!youattack && magr && cansee(magr->mx, magr->my))
        || (!youdefend && cansee(mdef->mx, mdef->my))
        || (youattack && Engulfed && mdef == u.ustuck && !Blind);
    boolean realizes_damage;
    const char *hittee = youdefend ? "you" : mon_nam(mdef);

    /* The following takes care of most of the damage, but not all-- the
       exception being for level draining, which is specially handled. Messages 
       are done in this function, however. */
    *dmgptr += spec_dbon(otmp, mdef, *dmgptr);

    if (youattack && youdefend) {
        impossible("attacking yourself with weapon?");
        return FALSE;
    }

    realizes_damage = (youdefend || vis ||
                       /* feel the effect even if not seen */
                       (youattack && mdef == u.ustuck));

    /* the four basic attacks: fire, cold, shock and missiles */
    if (attacks(AD_FIRE, otmp)) {
        if (realizes_damage)
            pline("The fiery blade %s %s%c",
                  !spec_dbon_applies ? "hits" : (mdef->data ==
                                                 &mons[PM_WATER_ELEMENTAL]) ?
                  "vaporizes part of" : "burns", hittee,
                  !spec_dbon_applies ? '.' : '!');
        if (!rn2(4))
            destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
        if (!rn2(4))
            destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
        if (!rn2(7))
            destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
        if (youdefend && Slimed)
            burn_away_slime();
        return realizes_damage;
    }
    if (attacks(AD_COLD, otmp)) {
        if (realizes_damage)
            pline("The ice-cold blade %s %s%c",
                  !spec_dbon_applies ? "hits" : "freezes", hittee,
                  !spec_dbon_applies ? '.' : '!');
        if (!rn2(4))
            destroy_mitem(mdef, POTION_CLASS, AD_COLD);
        return realizes_damage;
    }
    if (attacks(AD_ELEC, otmp)) {
        if (realizes_damage)
            pline("The massive hammer hits%s %s%c",
                  !spec_dbon_applies ? "" : "!  Lightning strikes", hittee,
                  !spec_dbon_applies ? '.' : '!');
        if (!rn2(5))
            destroy_mitem(mdef, RING_CLASS, AD_ELEC);
        if (!rn2(5))
            destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
        return realizes_damage;
    }
    if (attacks(AD_MAGM, otmp)) {
        if (realizes_damage)
            pline("The imaginary widget hits%s %s%c",
                  !spec_dbon_applies ? "" :
                  "!  A hail of magic missiles strikes", hittee,
                  !spec_dbon_applies ? '.' : '!');
        return realizes_damage;
    }

    if (attacks(AD_STUN, otmp) && dieroll <= MB_MAX_DIEROLL) {
        /* Magicbane's special attacks (possibly modifies hittee[]) */
        return magicbane_hit(magr, mdef, otmp, dmgptr, dieroll, vis, hittee);
    }

    if (!spec_dbon_applies) {
        /* since damage bonus didn't apply, nothing more to do; no further
           attacks have side-effects on inventory */
        return FALSE;
    }

    /* Tsurugi of Muramasa, Vorpal Blade */
    if (spec_ability(otmp, SPFX_BEHEAD))
        return artifact_hit_behead(magr, mdef, otmp, dmgptr, dieroll);

    /* Stormbringer */
    if (spec_ability(otmp, SPFX_DRLI))
        return artifact_hit_drainlife(magr, mdef, otmp, dmgptr);

    return FALSE;
}

static const char recharge_type[] = { ALLOW_COUNT, ALL_CLASSES, 0 };
static const char invoke_types[] = { ALL_CLASSES, 0 };

                /* #invoke: an "ugly check" filters out most objects */

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
            return do_break_wand(obj);
        else if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
                 obj->otyp == BRASS_LANTERN)
            return dorub(&(struct nh_cmd_arg){.argtype = CMD_ARG_OBJ,
                                              .invlet = obj->invlet});
        else if (obj->otyp == CRYSTAL_BALL)
            use_crystal_ball(obj);
        else if (obj->otyp == SPE_BOOK_OF_THE_DEAD)
            deadbook(obj, TRUE); /* deadbook() handles nothing happening */
        else
            pline("Nothing happens.");
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
            pline("You feel that %s %s ignoring you.", the(xname(obj)),
                  otense(obj, "are"));
            /* and just got more so; patience is essential... */
            obj->age += (long)dice(3, 10);
            return 1;
        }
        obj->age = moves + rnz_on_rng(100, rng_artifact_invoke);

        switch (oart->inv_prop) {
        case TAMING:{
                struct obj *pseudo = mktemp_sobj(NULL, SCR_TAMING);
                boolean unused_known;

                seffects(pseudo, &unused_known);
                obfree(pseudo, NULL);
                break;
            }
        case HEALING:{
                int healamt = (u.uhpmax + 1 - u.uhp) / 2;
                long creamed = (long)u.ucreamed;

                if (Upolyd)
                    healamt = (u.mhmax + 1 - u.mh) / 2;
                if (healamt || Sick || Slimed || Blinded > creamed)
                    pline("You feel better.");
                else
                    goto nothing_special;
                if (healamt > 0) {
                    if (Upolyd)
                        u.mh += healamt;
                    else
                        u.uhp += healamt;
                }
                if (Sick)
                    make_sick(0L, NULL, FALSE, SICK_ALL);
                if (Slimed)
                    Slimed = 0L;
                if (Blinded > creamed)
                    make_blinded(creamed, FALSE);
                break;
            }
        case ENERGY_BOOST:{
                int epboost = (u.uenmax + 1 - u.uen) / 2;

                if (epboost > 120)
                    epboost = 120;      /* arbitrary */
                else if (epboost < 12)
                    epboost = u.uenmax - u.uen;
                if (epboost) {
                    pline("You feel re-energized.");
                    u.uen += epboost;
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
                recharge(otmp, b_effect ? 1 : obj->cursed ? -1 : 0);
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
                    pline("You feel very disoriented for a moment.");
                } else {
                    if (!Blind)
                        pline("You are surrounded by a shimmering sphere!");
                    else
                        pline("You feel weightless for a moment.");
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
        boolean redundant = u.uintrinsic[oart->inv_prop] ||
            worn_extrinsic(oart->inv_prop) & ~W_MASK(os_invoked);

        if (on && obj->age > moves) {
            /* the artifact is tired :-) */
            pline("You feel that %s %s ignoring you.", the(xname(obj)),
                  otense(obj, "are"));
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
                pline
                    ("You feel a surge of power, but nothing seems to happen.");
            return 1;
        }
        switch (oart->inv_prop) {
        case CONFLICT:
            if (on)
                pline("You feel like a rabble-rouser.");
            else
                pline("You feel the tension decrease around you.");
            break;
        case LEVITATION:
            if (on) {
                float_up();
                spoteffects(FALSE);
            } else
                float_down(I_SPECIAL | TIMEOUT);
            break;
        case INVIS:
            if (BInvis || Blind)
                goto nothing_special;
            newsym(u.ux, u.uy);
            if (on)
                pline("Your body takes on a %s transparency...",
                      Hallucination ? "normal" : "strange");
            else
                pline("Your body seems to unfade...");
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
    pline("%s:", Tobjnam(obj, "whisper"));
    verbalize("%s", line);
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

