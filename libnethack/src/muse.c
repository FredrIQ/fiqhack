/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-10 */
/* Copyright (C) 1990 by Ken Arromdee                              */
/* NetHack may be freely redistributed.  See license for details.  */

/*
 * Monster item usage routines.
 */

#include "hack.h"
#include "artifact.h"

extern const int monstr[];

/*
 * Monster item usage logic.  General guidelines:
 * - Monsters do not play the ID game with object types (it is unfeasible)
 * - When it comes to everything else regarding the ID game and object
 *   knowledge, player replication should be done whenever possible.
 * - The AI will not avoid reading scrolls while confused, or take it to
 *   its' adventage. This is by design. Maybe later some extra intelligent
 *   monsters can abuse this.
 * - Only intelligent monsters use items. Any non-mindless can spellcast.
 * - Monsters generally use items whenever they have a good reason to, and
 *   have no concept of resource management. For hostile monsters, this is
 *   OK but can be problematic with pets. Maybe for later.
 */

static void mconfdir(const struct monst *, schar *, schar *);
static int precheck(struct obj *obj, struct musable *m);
static void mzapmsg(struct monst *, struct obj *, boolean);
static void mreadmsg(struct monst *, struct obj *);
static void mquaffmsg(struct monst *, struct obj *);
static void mon_break_wand(struct monst *, struct obj *);
static int find_item_score(const struct monst *, struct obj *, coord *, struct musable *);
static int find_item_single(struct obj *, boolean, struct musable *, boolean, boolean);
static boolean mon_allowed(int);

static int trapx, trapy;

void
init_musable(struct monst *mon, struct musable *m)
{
    m->mon = mon;
    m->obj = NULL;
    m->tobj = NULL;
    m->spell = 0;
    m->use = MUSE_NONE;

    /* Set xyz to -1, which is never valid, as a sentinel for not having chosen any
       direction/position yet. */
    m->x = -1;
    m->y = -1;
    m->z = -1;
}

/* Converts a nh_cmd_arg to a musable, allowing one to use a single struct for both
   player and monster "commands". It is important to note that this performs no
   additional sanity checks, all it does is to convert arg to musable. So far, for
   the scope of this, limit and str (from arg) is unneeded, and pos/dir is
   mutually exclusive (they both use m.x/m.y for musable). Equavilents to functions that
   actually include sanity checks are also included below in the form of m*(), i.e.
   mgetargdir, mgetargpos, mgetargobj, mgetargspell. These will also (like the non-m
   equavilents) ask the player in case the data is missing or invalid. */
struct musable
arg_to_musable(const struct nh_cmd_arg *arg)
{
    struct musable m;
    init_musable(&youmonst, &m);

    /* Set xyz. Note that currently, muse uses x/y/z both for directions and absolute
       positions. So far, they have been mutually exclusive, but it is possible that
       this changes later. */
    if (arg->argtype & CMD_ARG_DIR)
        dir_to_delta(arg->dir, (schar *) &m.x, (schar *) &m.y, (schar *) &m.z);
    else if (arg->argtype & CMD_ARG_POS) {
        m.x = arg->pos.x;
        m.y = arg->pos.y;
        m.z = 0;
    }

    /* Set obj. &zeroobj is not particurly specific, but that's what getargobj() returns
       in the same context... */
    if (arg->argtype & CMD_ARG_OBJ) {
        if (arg->invlet == '-' || arg->invlet == ',')
            m.obj = &zeroobj;
        else
            for (m.obj = youmonst.minvent; m.obj; m.obj = m.obj->nobj)
                if (m.obj->invlet == arg->invlet)
                    break;
    }

/* from spell.c (TODO: these are pretty awkward in general, even in spell.c) */
#define spellid(spell)   spl_book[spell].sp_id
#define spellno_from_let(slet)                                  \
    (((slet) >= 'a' && (slet) <= 'z') ? slet - 'a' :            \
     ((slet) >= 'A' && (slet) <= 'Z') ? slet - 'A' + 26 : -1)
    if (arg->argtype & CMD_ARG_SPELL) {
        int sno = spellno_from_let(arg->spelllet);
        if (sno >= 0 && spellid(sno) != NO_SPELL)
            m.spell = spellid(sno);
    }
#undef spellid
#undef spellno_from_let

    return m;
}

/* A version of confdir for general purposes (needed due to grid bug checks) */
static void
mconfdir(const struct monst *mon, schar *dx, schar *dy)
{
    int x = (mon->data == &mons[PM_GRID_BUG]) ? 2 * rn2(4) : rn2(8);

    *dx = xdir[x];
    *dy = ydir[x];
    return;
}

/* Sets direction deltas in d*, possibly asking the user. */
int
mgetargdir(const struct musable *m, const char *query,
           schar *dx, schar *dy, schar *dz)
{
    struct monst *mon = m->mon;

    /* Is there a reasonable direction already? */
    if ((m->x != -1 || m->y != -1 || m->z != -1) &&
        (!m->x || !m->y || mon->data != &mons[PM_GRID_BUG])) {
        if (mon == &youmonst) {
            turnstate.intended_dx = m->x;
            turnstate.intended_dy = m->y;
        }

        /* Set musable params in d*. Technically redundant, but keeps consistency
           with getargdir and other mgetargs where they're actually needed */
        *dx = m->x;
        *dy = m->y;
        *dz = m->z;

        /* confusion/stun handling */
        if (!m->z && (stunned(mon) || (confused(mon) && !rn2(5))))
            mconfdir(mon, dx, dy);

        return 1;
    }

    if (m->mon != &youmonst) {
        impossible("mgetargdir: unclear direction for non-player?");
        return 0;
    }

    /* Otherwise, ask */
    return getdir(query, dx, dy, dz, TRUE);
}

/* Sets cc and performs sanity check on position */
int
mgetargpos(const struct musable *m, coord *cc, boolean force,
          const char *goal)
{
    /* Did the client specify an (in bounds) position? */
    if ((m->x != -1 || m->y != -1 || m->z != -1) && isok(cc->x, cc->y)) {
        cc->x = m->x;
        cc->y = m->y;
        return NHCR_ACCEPTED;
    }

    if (m->mon != &youmonst) {
        impossible("mgetargpos: unclear position for non-player?");
        return 0;
    }

    /* Otherwise, ask. */
    return getpos(cc, force, goal, TRUE);
}

/* Returns musable's object if valid or a queried-for object otherwise.
   The way parameters are set up doesn't really allow validation of monster
   choices, those are assumed to be valid (they should be anyway) */
struct obj *
mgetargobj(const struct musable *m, const char *let, const char *word)
{
    if (m->mon != &youmonst)
        return m->obj;

    /* Is there a valid object? (If not, ask for another.) */
    if ((m->obj && validate_object(m->obj, let, word)) ||
        (m->obj == &zeroobj && strchr(let, ALLOW_NONE)))
        return m->obj;

    /* Otherwise, prompt the user. */
    return getobj(let, word, TRUE);
}

boolean
mgetargspell(const struct musable *m, int *spell_no)
{
    if (m->spell) {
        *spell_no = m->spell;
        return TRUE;
    }

    if (m->mon != &youmonst) {
        impossible("mgetargspell: unclear spell for non-player?");
        return 0;
    }

    return getspell(spell_no);
}

/* Any preliminary checks which may result in the monster being unable to use
   the item.  Returns 0 if nothing happened, 2 if the monster can't do anything
   (i.e. it teleported) and 1 if it's dead. */
static int
precheck(struct obj *obj, struct musable *m)
{
    struct monst *mon = m->mon;
    boolean vis;
    int wandlevel;

    if (!obj)
        return 0;
    vis = cansee(mon->mx, mon->my);

#define POTION_OCCUPANT_CHANCE(n) (13 + 2*(n))  /* also in muse.c */

    if (obj->oclass == POTION_CLASS) {
        const char *potion_descr;
        potion_descr = OBJ_DESCR(objects[obj->otyp]);

        if (!strcmp(potion_descr, "milky") &&
            !(mvitals[PM_GHOST].mvflags & G_GONE) &&
            !rn2(POTION_OCCUPANT_CHANCE(mvitals[PM_GHOST].born))) {
            mquaffmsg(mon, obj);
            ghost_from_bottle(mon);
            m_useup(mon, obj);
            return 2;
        } else if (!strcmp(potion_descr, "smoky") &&
                   !(mvitals[PM_DJINNI].mvflags & G_GONE) &&
                   !rn2(POTION_OCCUPANT_CHANCE(mvitals[PM_DJINNI].born))) {
            /* not rng_smoky_potion; that's for wishes that players will get */
            mquaffmsg(mon, obj);
            djinni_from_bottle(mon, obj);
            m_useup(mon, obj);
            return 2;
        }
    }
    if (obj->oclass == WAND_CLASS) {
        wandlevel = getwandlevel(mon, obj);
        if (wandlevel == P_FAILURE) {
            /* critical failure */
            if (canhear()) {
                if (vis)
                    pline(msgc_itemloss,
                          "%s zaps %s, which suddenly explodes!", Monnam(mon),
                          an(xname(obj)));
                else
                    You_hear(msgc_itemloss,
                             "a zap and an explosion in the distance.");
            }
            mon_break_wand(mon, obj);
            m_useup(mon, obj);
            m->use = MUSE_NONE;
            return DEADMONSTER(mon) ? 1 : 2;
        }
    }
    return 0;
}

static void
mzapmsg(struct monst *mtmp, struct obj *otmp, boolean self)
{
    if (!mon_visible(mtmp))
        You_hear(msgc_levelsound, "a %s zap.",
                 distant(mtmp) ? "distant" : "nearby");
    else if (self)
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s zaps %sself with %s!",
              Monnam(mtmp), mhim(mtmp), doname(otmp));
    else {
        /* TODO: channelize based on results */
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s zaps %s!",
              Monnam(mtmp), an(xname(otmp)));
        action_interrupted();
    }
}

static void
mreadmsg(struct monst *mtmp, struct obj *otmp)
{
    boolean vismon = mon_visible(mtmp);
    short saverole;
    const char *onambuf;
    unsigned savebknown;

    if (!vismon && !canhear())
        return; /* no feedback */

    otmp->dknown = 1;   /* seeing or hearing it read reveals its label */
    /* shouldn't be able to hear curse/bless status of unseen scrolls; for
       priest characters, bknown will always be set during naming */
    savebknown = otmp->bknown;
    saverole = Role_switch;
    if (!vismon) {
        otmp->bknown = 0;
        if (Role_if(PM_PRIEST))
            Role_switch = 0;
    }
    onambuf = singular(otmp, doname);
    Role_switch = saverole;
    otmp->bknown = savebknown;

    if (vismon && !blind(mtmp) && haseyes(mtmp->data))
        pline(combat_msgc(mtmp, NULL, cr_hit),
              "%s reads %s!", Monnam(mtmp), onambuf);
    else
        You_hear(combat_msgc(mtmp, NULL, cr_hit),
                 "%s pronouncing the formula on %s!",
                 x_monnam(mtmp, ARTICLE_A, NULL,
                          (SUPPRESS_IT | SUPPRESS_INVISIBLE |
                           SUPPRESS_SADDLE | SUPPRESS_ENSLAVEMENT),
                          FALSE), onambuf);

    if (confused(mtmp))
        pline(msgc_substitute,
              "Being confused, %s mispronounces the magic words...",
              vismon ? mon_nam(mtmp) : mhe(mtmp));
}

static void
mquaffmsg(struct monst *mtmp, struct obj *otmp)
{
    if (mon_visible(mtmp)) {
        otmp->dknown = 1;
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s drinks %s!",
              Monnam(mtmp), singular(otmp, doname));
    } else
        You_hear(msgc_levelsound, "a chugging sound.");
}


/* Monster wishes.
   Balance considerations: Undead wishing can only be performed latergame
   since only V and L are undead *and* able to wish. Player monsters are
   either undead turned, reverse-genocided or on Astral. Mines inhabitants
   wishing for gems should be rarer than finding *actual* gems in there
   and can be a nice bonus and makes flavour sense as well. As arti-wishing
   makes flavour sense, and they are occasionally generated with said
   artifacts anyway. Rodney is Rodney, and guards knowing what they're doing
   makes sense. The general wishes are neither overpowered early or too harsh. */
#define FIRST_GEM    DILITHIUM_CRYSTAL
boolean
mon_makewish(struct monst *mon)
{
    if (canseemon(mon) && (msensem(mon, &youmonst) & MSENSE_ANYVISION) &&
        mon->mtame) {
        /* for tame monsters, redirect the wish if hero is in view */
        pline(msgc_intrgain, "%s looks at you curiously.", Monnam(mon));
        makewish();
        return TRUE;
    }
    const struct permonst *mdat = mon->data;
    int pm = monsndx(mdat);
    struct obj *wishobj;
    short wishtyp = 0;
    short wisharti = 0;

    /* Fleeing monsters wish for the best escape item there is */
    if (mon->mflee)
        wishtyp = POT_GAIN_LEVEL;
    else if ((is_gnome(mdat) || is_dwarf(mdat)) && !spellcaster(mdat))
        /* Gnomish wizards prefer spells (the spellcaster() check) */
        wishtyp = rn1((LAST_GEM - FIRST_GEM) + 1, FIRST_GEM);
    else if (is_undead(mdat))
        wishtyp = WAN_DEATH;
    else if (is_mplayer(mdat) && !Role_if(monsndx(mdat)))
        wisharti = role_quest_artifact(pm);
    else if (mdat->mlet == S_ANGEL && (!mon->mw || !mon->mw->oartifact))
        wisharti = rn2(2) ? ART_SUNSWORD : ART_DEMONBANE;
    else if (mon->iswiz) {
        /* magic resistance */
        if (!resists_magm(mon)) {
            if (!exist_artifact(AMULET_OF_ESP, artiname(ART_EYE_OF_THE_AETHIOPICA)))
                wisharti = ART_EYE_OF_THE_AETHIOPICA;
            else if (!exist_artifact(CREDIT_CARD, artiname(ART_YENDORIAN_EXPRESS_CARD)))
                wisharti = ART_YENDORIAN_EXPRESS_CARD;
            else if (!exist_artifact(LENSES, artiname(ART_EYES_OF_THE_OVERWORLD)))
                wisharti = ART_EYES_OF_THE_OVERWORLD;
            else if (!exist_artifact(MIRROR, artiname(ART_MAGIC_MIRROR_OF_MERLIN)))
                wisharti = ART_MAGIC_MIRROR_OF_MERLIN;
            else if (!exist_artifact(MACE, artiname(ART_SCEPTRE_OF_MIGHT)))
                wisharti = ART_SCEPTRE_OF_MIGHT;
            else if (!exist_artifact(CRYSTAL_BALL, artiname(ART_ORB_OF_DETECTION)))
                wisharti = ART_ORB_OF_DETECTION;
        }
        if (!wisharti || wisharti == role_quest_artifact(monsndx((&youmonst)->data))) {
            wisharti = 0;
            wishtyp = SCR_GENOCIDE; /* Destroy the thief, my pets! */
        }
    } else if (is_mercenary(mdat)) {
        /* mercenaries, being experienced warriors, knows the good stuff
           (Inspired from GruntHack's monster wishlist) */
        switch (rnd(7)) {
        case 1:
            /* don't replace DSM */
            if (!((reflecting(mon) | resists_magm(mon)) & W_MASK(os_arm))) {
                if (!reflecting(mon))
                    wishtyp = SILVER_DRAGON_SCALE_MAIL;
                else
                    wishtyp = GRAY_DRAGON_SCALE_MAIL;
                break;
            }
            /* fallthrough */
        case 2:
            if (!very_fast(mon)) {
                if (!(mon->misc_worn_check & W_MASK(os_armf)))
                    wishtyp = SPEED_BOOTS;
                else
                    wishtyp = WAN_SPEED_MONSTER;
                break;
            }
            /* fallthrough */
        case 3:
            wishtyp = WAN_DEATH;
            break; /* no fallthrough since 3 is unconditional */
        case 4:
            if (!(mons[PM_CHICKATRICE].geno & G_UNIQ) &&
                !(mvitals[PM_CHICKATRICE].mvflags & G_NOCORPSE) &&
                !(mons[PM_COCKATRICE].geno & G_UNIQ) &&
                !(mvitals[PM_COCKATRICE].mvflags & G_NOCORPSE) &&
                !(mon->mw && mon->mw->otyp == CORPSE) &&
                (mon->misc_worn_check & W_MASK(os_armg))) {
                wishtyp = CORPSE;
                break;
            }
            /* fallthrough */
        case 5:
            if (!resists_magm(mon))
                wishtyp = CLOAK_OF_MAGIC_RESISTANCE;
            else if (!((protected(mon) | resists_magm(mon)) & W_MASK(os_armc)))
                wishtyp = CLOAK_OF_PROTECTION;
            if (wishtyp)
                break;
            /* fallthrough */
        case 6:
            if (!(mon->misc_worn_check & W_MASK(os_amul))) {
                /* 50% of the time, give "oLS anyway */
                if (!reflecting(mon))
                    wishtyp = AMULET_OF_LIFE_SAVING;
                else
                    wishtyp = (rn2(2) ? AMULET_OF_REFLECTION :
                               AMULET_OF_LIFE_SAVING);
                break;
            }
            /* fallthrough */
        case 7:
            wishtyp = SCR_GENOCIDE;
        }
    } else if ((likes_gold(mdat) && !rn2(5)) || mx_eshk(mon))
        wishtyp = GOLD_PIECE;

    if (wisharti) {
        wishtyp = artityp(wisharti);
        /* 1/5 of the time, try anyway! */
        if (rn2(5) && exist_artifact(wishtyp, artiname(wisharti))) {
            wisharti = 0;
            wishtyp = 0;
        }
    }

    /* Below the wisharti check so that spellcasters that wished for artis
       gets a chance to fall back to spells */
    if (spellcaster(mdat) && !wisharti && !wishtyp) {
        /* first, wish for a helm of brilliance if we lack it */
        if (!has_horns(mdat) &&
            (!which_armor(mon, os_armh) ||
             which_armor(mon, os_armh)->otyp != HELM_OF_BRILLIANCE))
            wishtyp = HELM_OF_BRILLIANCE;
        else {
            int wish_spells[] = {
                SPE_SUMMON_NASTY, SPE_TURN_UNDEAD, SPE_EXTRA_HEALING,
                SPE_FINGER_OF_DEATH, SPE_MAGIC_MISSILE, SPE_SPEED_MONSTER,
                SPE_REMOVE_CURSE, SPE_CHARGING, SPE_IDENTIFY,
                SPE_PROTECTION
            };
            int i;
            /* pick a random spell out of the above */
            for (i = 0; i < 20; i++) {
                wishtyp = wish_spells[rn2(SIZE(wish_spells))];
                /* avoid spells in schools we lack or already know */
                if (mon_castable(mon, wishtyp, TRUE) ||
                    mprof(mon, mspell_skilltype(wishtyp)) == P_UNSKILLED)
                    wishtyp = 0;
                if (wishtyp) /* found a good spellbook */
                    break;
            }
        }
    }

    /* Generic wishing for everything else */
    if (!wishtyp) {
        if (!(mon->misc_worn_check & W_MASK(os_amul)))
            wishtyp = AMULET_OF_LIFE_SAVING;
        else /* Not wand of death, that might be too harsh */
            wishtyp = rn2(2) ? SCR_GENOCIDE : WAN_CREATE_MONSTER;
    }

    /* Wish decided -- perform the wish
       TODO: check luck */
    wishobj = mksobj(mon->dlevel, wishtyp, TRUE, FALSE, rng_main);

    /* Monsters and players share artiwish counter, monwish is for tracking who wished
       for it. */
    if (wisharti) {
        wishobj = oname(wishobj, artiname(wisharti));
        wishobj->quan = 1L;
        if (wishobj->oartifact)
            artifact_exists(wishobj, ox_name(wishobj), ag_monwish);
        if (is_quest_artifact(wishobj) ||
             (wishobj->oartifact &&
              rn2(nartifact_wished()))) {
            artifact_exists(wishobj, ox_name(wishobj), ag_none);
            obfree(wishobj, NULL);
            wishobj = &zeroobj;
            if (canseemon(mon))
                pline(msgc_itemloss, "For a moment, you see something in "
                      "%s %s, but it disappears!", s_suffix(mon_nam(mon)),
                      makeplural(mbodypart(mon, HAND)));
            return FALSE;
        }
    }

    if (wishtyp == CORPSE) {
        /* trice */
        wishobj->corpsenm = PM_CHICKATRICE;
        if ((mons[wishobj->corpsenm].geno & G_UNIQ) ||
            (mvitals[wishobj->corpsenm].mvflags & G_NOCORPSE))
            wishobj->corpsenm = PM_COCKATRICE;
        /* partly eaten */
        wishobj->oeaten = mons[wishobj->corpsenm].cnutrit;
    }
    if (wishtyp == GOLD_PIECE) {
        /* 1-5000 gold, shopkeepers always wish for 5000 */
        if (!mx_eshk(mon))
            wishobj->quan = rnd(5000);
        else
            wishobj->quan = 5000;
    } else {
        /* greased partly eaten very holy fireproof burnt +3 boots of speed of spinach */
        wishobj->blessed = 1;
        /* Undead/demons prefer nonblessed objects. However, wands get significantly
           more potent blessed */
        if (wishobj->oclass != WAND_CLASS && (is_demon(mdat) || is_undead(mdat)))
            wishobj->blessed = 0;
        wishobj->cursed = 0;
        if (wishtyp == POT_GAIN_LEVEL ||
            wishtyp == SCR_GENOCIDE) {
            /* monsters only wish for those cursed */
            wishobj->blessed = 0;
            wishobj->cursed = 1;
        }
        wishobj->greased = 1;
        wishobj->oerodeproof = 1;
        if (wishobj->oclass == ARMOR_CLASS ||
             wishobj->oclass == WEAPON_CLASS) {
            /* greedy monsters go for +5, others go for +2 or +3 */
            if (likes_gold(mdat))
                wishobj->spe = 5;
            else
                wishobj->spe = rn2(2) ? 2 : 3;
            /* convert to +0 when applicable */
            if (rnd(5) < wishobj->spe)
                wishobj->spe = 0;
        }
        if (!wishobj->oartifact &&
            (wishobj->oclass == POTION_CLASS ||
             wishobj->oclass == SCROLL_CLASS ||
             wishobj->oclass == GEM_CLASS)) {
            if (likes_gold(mdat))
                wishobj->quan = 5L;
            else
                wishobj->quan = rn2(2) ? 2L : 3L;
            if (rnd(5) < wishobj->quan)
                wishobj->quan = 1L;
        }
    }

    if (wishobj && canseemon(mon))
        pline(msgc_monneutral, "%s appears in %s %s!",
              distant_name(wishobj, Doname2),
              s_suffix(mon_nam(mon)),
              makeplural(mbodypart(mon, HAND)));
    if (mpickobj(mon, wishobj, &wishobj))
        wishobj = m_carrying(mon, wishtyp);
    if (!wishobj) {
        impossible("monster wished-for object disappeared?");
        return FALSE;
    }

    /* technically, player wishing only informally IDs the item
       (since you probably know what you wished for), but monsters
       have no concept of informal ID, so give it a formal one BUC-wise */
    wishobj->mbknown = 1;

    /* wear new equipment */
    if (wishobj->oclass == ARMOR_CLASS ||
        wishobj->oclass == AMULET_CLASS)
        m_dowear(mon, FALSE);

    return TRUE;
}

/* blacklist some items for now */
static boolean
mon_allowed(int otyp)
{
    /* Initial Release Test: free for all item using */
    return TRUE;

    /* The corresponding spells (when relevant) are OK */
    switch (otyp) {
    case WAN_WISHING: /* was controversial in GruntHack */
    case SCR_GENOCIDE: /* following items are rather valuable */
    case SCR_CHARGING:
    case SCR_IDENTIFY:
    case SCR_ENCHANT_ARMOR:
    case SCR_ENCHANT_WEAPON:
        return FALSE;
        break;
    default:
        return TRUE;
        break;
    }
    return FALSE;
}

/* Monster directional targeting. The monster will check through all
   directions, scoring based on what would be hit and their
   resistances/etc. The monster assumes perfect accuracy. Potions will
   only be scored based on first hit target (since the monster assumes
   perfect accuracy, a potion would simply be destroyed on the first
   target). */
int
mon_choose_dirtarget(const struct monst *mon, struct obj *obj, coord *cc)
{
    int oc_dir = objects[obj->otyp].oc_dir;
    int wandlevel = mprof(mon, MP_WANDS);
    int range;
    int score = 0;
    int score_best = 0;
    int tilescore = 0;
    int dx, dy; /* directions */
    int cx, cy; /* current directions (can change for bounces) */
    int sx, sy; /* specific coords */
    int lsx, lsy; /* last value of sx/sy (bounce logic needs it) */
    cc->x = 0;
    cc->y = 0;
    boolean self = FALSE;
    boolean wand = (obj->oclass == WAND_CLASS);
    boolean spell = (obj->oclass == SPBOOK_CLASS);
    boolean helpful = FALSE;

    /* if monsters know BUC, apply real wandlevel for wands */
    if (obj->mbknown && wand) {
        wandlevel = getwandlevel(mon, obj);
        if (!wandlevel) /* cursed wand is going to blow up */
            return 0;
    } else if (obj->otyp == SPE_MAGIC_MISSILE) {
        wandlevel = mprof(mon, MP_SATTK);
        if (wandlevel < P_SKILLED)
            wandlevel = 0;
    } else if (!wand)
        wandlevel = 0;
    struct monst *mtmp;
    struct rm *loc;
    for (dx = -1; dx <= 1; dx++) {
        for (dy = -1; dy <= 1; dy++) {
            self = FALSE;
            range = BOLT_LIM;
            score = 0;
            sx = mon->mx;
            sy = mon->my;
            cx = dx;
            cy = dy;
            /* x/y = 0 -> selfzap */
            if (!dx && !dy)
                self = TRUE;
            if (self && !wand && !spell)
                continue; /* only wands and spells can be selfzapped */
            /* TODO: implement monsters throwing stuff upwards
               (is this ever a good idea?) */
            if (self)
                range = 1;
            while (range-- > 0) {
                tilescore = 0;
                helpful = FALSE;
                self = FALSE;
                /* following objects are usually helpful */
                if (obj->otyp == SPE_HEALING ||
                    obj->otyp == SPE_EXTRA_HEALING ||
                    obj->otyp == SPE_STONE_TO_FLESH ||
                    obj->otyp == SPE_SPEED_MONSTER ||
                    obj->otyp == WAN_SPEED_MONSTER ||
                    obj->otyp == WAN_MAKE_INVISIBLE)
                    helpful = TRUE;
                /* ls* is for bounce purposes */
                lsx = sx;
                lsy = sy;
                sx += cx;
                sy += cy;
                if (!isok(sx, sy) || !(loc = &level->locations[sx][sy])->typ ||
                    !ZAP_POS(loc->typ) || closed_door(level, sx, sy)) {
                    range--;
                    if ((!wand && !spell) || oc_dir != RAY || obj->otyp == SPE_FIREBALL)
                        break;
                    if (range > 0) { /* bounce */
                        /* maybe only allow some monsters to visualize raybouncing?
                           TODO: I have no idea how this code actually works, but I
                           have a feeling that it could be simplified. As-is, it's
                           copied straight from buzz(), and I know that it works. */
                        int bounce = 0;
                        uchar rmn;
                        if (!cx || !cy || !rn2(20)) {
                            cx = -cx;
                            cy = -cy;
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
                                cx = -cx;   /* fall into... */
                            case 1:
                                cy = -cy;
                                break;
                            case 2:
                                cx = -cx;
                                break;
                            }
                        }
                    }
                    continue;
                }
                mtmp = m_at(mon->dlevel, sx, sy);
                if (!mtmp && sx == u.ux && sy == u.uy)
                    mtmp = &youmonst;
                if (sobj_at(BOULDER, level, sx, sy) &&
                    !throws_rocks(mon->data) &&
                    (obj->otyp == WAN_STRIKING ||
                     obj->otyp == SPE_FORCE_BOLT))
                    /* Boulders generally make the life harder for monsters, because they
                       have pretty much no control over them. Thus, give a scoring bonus
                       if they're nearby (but not for sokoban boulders!). */
                    tilescore += In_sokoban(m_mz(mon)) ? -20 : +20;
                if (sobj_at(CORPSE, level, sx, sy) &&
                    (obj->otyp == WAN_UNDEAD_TURNING ||
                     obj->otyp == SPE_TURN_UNDEAD))
                    /* hostiles revive corpses (it's very rarely a bad idea for hostile
                       monsters to do this, and even if it happens to be a pet, maybe the
                       hostile monster didn't realize?) */
                    tilescore += (mon->mpeaceful ? -20 : +30);
                if (mtmp) {
                    if (mon == mtmp)
                        self = TRUE;
                    /* ignore monsters that we can't sense */
                    if (!self && !msensem(mon, mtmp))
                        continue;
                    range -= 2; /* buzz */
                    if ((wand || spell) && oc_dir == IMMEDIATE)
                        range -= 1; /* -3 for beam wands */
                    if ((!wand && !spell) || obj->otyp == SPE_FIREBALL)
                        range = 0; /* fireballs hits 1st target only */
                    if (oc_dir == RAY && wandlevel < P_SKILLED &&
                        prop_wary(mon, mtmp, REFLECTING)) {
                        cx = -cx;
                        cy = -cy;
                        continue; /* reflecting a ray has no effect on monster */
                        /* TODO: lightning blindness? */
                    }
                    if (!obj_affects(mon, mtmp, obj))
                        continue; /* object has no effect */
                    /* curing slime is helpful */
                    if ((obj->otyp == WAN_FIRE ||
                         obj->otyp == SPE_FIREBALL) &&
                        prop_wary(mon, mtmp, SLIMED))
                        helpful = TRUE;
                    /* make invisible vs invisible things is harmful */
                    if (obj->otyp == WAN_MAKE_INVISIBLE &&
                        prop_wary(mon, mtmp, INVIS))
                        helpful = FALSE;
                    /* stone to flesh vs stone golems is harmful */
                    if (obj->otyp == SPE_STONE_TO_FLESH &&
                        mtmp->data == &mons[PM_STONE_GOLEM])
                        helpful = FALSE;
                    /* flesh to stone vs flesh golems is helpful */
                    if (obj->otyp == EGG && /* trice */
                        mtmp->data == &mons[PM_FLESH_GOLEM])
                        helpful = TRUE;
                    /* polymorph is special */
                    if (obj->otyp == WAN_POLYMORPH ||
                        obj->otyp == SPE_POLYMORPH) {
                        helpful = TRUE;
                        /* Polymorphing nasties is considered harmful */
                        if (extra_nasty(mtmp->data))
                            helpful = FALSE;
                        /* Otherwise, hit dice OR player level if not polymorphed decides */
                        if (mtmp == &youmonst && !Upolyd && u.ulevel >= 14)
                            helpful = FALSE;
                        if (mtmp == &youmonst && Upolyd && mtmp->data->mlevel >= 14)
                            helpful = FALSE;
                        if (mtmp != &youmonst && mtmp->data->mlevel >= 14)
                            helpful = FALSE;
                    }
                    /* Deathzapping Death will do no good. However, while a deathzap
                       against him would technically be helpful, declaring it as so would
                       encourage monsters to FoD/WoD him if hostile, which would be a
                       huge pain. Thus, he always get a scoring penalty */
                    if ((obj->otyp == WAN_DEATH ||
                         obj->otyp == SPE_FINGER_OF_DEATH) &&
                        mtmp->data == &mons[PM_DEATH]) {
                        tilescore -= 10;
                        continue;
                    }
                    /* Special case: make invisible and polymorph is always considered
                       harmful if zapped at player by tame or peaceful monster, unless */
                    if ((obj->otyp == WAN_MAKE_INVISIBLE ||
                         obj->otyp == WAN_POLYMORPH ||
                         obj->otyp == SPE_POLYMORPH) &&
                        mtmp == &youmonst && mon->mpeaceful)
                        helpful = FALSE;
                    if (self || (mon->mtame && mtmp == &youmonst))
                        /* -40 or +40 depending on helpfulness */
                        tilescore += (helpful ? 60 : -60);
                    /* target is hostile */
                    else if (mm_aggression(mon, mtmp))
                        tilescore += (helpful ? -10 : 20);
                    /* ally/peaceful -- we can't just perform "else" here, because pets
                       would heal hostiles that are too dangerous for it to target */
                    else if ((mtmp == &youmonst && mon->mpeaceful) ||
                             (mtmp != &youmonst &&
                              mon->mpeaceful == mtmp->mpeaceful)) {
                        tilescore += (helpful ? 20 : -10);
                        /* tame monsters like zapping friends and dislike collateral
                           damage */
                        if (mon->mtame) {
                            tilescore *= 2;
                            /* never hit allies with deathzaps */
                            if (obj->otyp == SPE_FINGER_OF_DEATH ||
                                obj->otyp == WAN_DEATH)
                                tilescore *= 10;
                        }
                    }
                    /* If monster is peaceful/tame and you don't see invisible, reveal
                       invisible things and don't make things invisible */
                    if (obj->otyp == WAN_MAKE_INVISIBLE && mon->mpeaceful &&
                        !see_invisible(&youmonst)) {
                        if (wandlevel >= P_SKILLED &&
                            m_has_property(mtmp, INVIS,
                                           (W_MASK(os_outside) | W_MASK(os_timeout)),
                                           TRUE))
                            tilescore = 30;
                        else
                            tilescore = 0;
                    }
                    /* cure stiffening ASAP */
                    if (obj->otyp == SPE_STONE_TO_FLESH)
                        tilescore *= 10;
                    /* adjust (extra) healing priority sometimes to vary between heal/extraheal */
                    if (obj->otyp == SPE_HEALING ||
                        obj->otyp == SPE_EXTRA_HEALING) {
                        if (rn2(2))
                            tilescore *= 2;
                        if (rn2(2))
                            tilescore /= 2;
                    }
                }
                score += tilescore;
            }
            /* kludge: for deathzaps, avoid zapping you if tame to avoid YAAD */
            /* no bounce handling here -- it simplifies the code, and bouncing here is an edge case */
            if (obj->otyp == SPE_FINGER_OF_DEATH &&
                obj->otyp == WAN_DEATH &&
                mon->mtame) {
                range = BOLT_LIM;
                while (--range) {
                    sx += cx;
                    sy += cy;
                    if (sx == u.ux && sy == u.uy)
                        score = 0;
                }
            }
            if (score > score_best) {
                cc->x = dx;
                cc->y = dy;
                score_best = score;
            }
        }
    }

    /* If the monster is stunned or 20% of the time if confused,
       scramble the direction */
    if (stunned(mon) || (confused(mon) && !rn2(5))) {
        int rand = rn2(9);
        cc->x = (rand / 3) - 1;
        cc->y = (rand % 3) - 1;
    }
    return score_best;
}

/* Monster specific position targeting. This works the following:
   The monster will check through all valid targets, assigning points based
   on what it will hit like this on each tile the spell/etc would hit:
   Enemy: +20
   Ally: -10
   Self if not resistant: -40
   The result is then divided by the range, and that makes up for that tile's
   score. The sum is the total score of all hit tiles.
   The winning score is the target. If there is no (positive) scoring targets,
   return 0, otherwise return best score (so it can be used to determine
   if it's worth using a scroll/spell/etc or not).
*/
int
mon_choose_spectarget(struct musable *m, struct obj *obj, coord *cc)
{
    struct monst *mon = m->mon;

    boolean stink = obj->otyp == SCR_STINKING_CLOUD ? TRUE : FALSE;
    int globrange = 10;
    if (stink)
        globrange = 5;
    int range = 2;
    if (stink)
        range = (obj->mbknown ? 2 + bcsign(obj) : 2);
    int tilescore = 0;
    int score = 0;
    int score_best = 0;
    int x, y, xx, yy;
    schar dx, dy;
    int x_best = 0;
    int y_best = 0;
    struct monst *mtmp;
    for (x = mon->mx - globrange; x <= mon->mx + globrange; x++) {
        for (y = mon->my - globrange; y <= mon->my + globrange; y++) {
            score = 0;

            /* Invalid targets */
            if (!isok(x, y))
                continue;
            if (IS_STWALL(level->locations[x][y].typ))
                continue;
            if (obj->otyp == SPE_FIREBALL ||
                obj->otyp == SPE_CONE_OF_COLD) {
                dx = x;
                dy = y;
                if (!throwspell(TRUE, FALSE, &dx, &dy, m))
                    continue;
            }

            mtmp = um_at(level, x, y);
            if (!m_cansee(mon, x, y) ||
                (!stink && (!mtmp || !mcanspotmon(mon, mtmp))))
                continue;
            if (dist2(mon->mx, mon->my, x, y) > (globrange * globrange))
                continue;

            /* Check what is hit here */
            for (xx = x - range; xx <= x + range; xx++) {
                for (yy = y - range; yy <= y + range; yy++) {
                    tilescore = 0;
                    /* invalid tile */
                    if (!isok(xx, yy))
                        continue;
                    /* out of stinking range */
                    if (distmin(x, y, xx, yy) > range && stink)
                        continue;

                    mtmp = um_at(mon->dlevel, xx, yy);
                    if (!mtmp)
                        continue;
                    if (!obj_affects(mon, mtmp, obj))
                        continue;
                    /* self harm */
                    if (mon == mtmp && (!sliming(mon) || obj->otyp != SPE_FIREBALL))
                        tilescore -= 40;
                    else if (mon == mtmp) /* cure slime */
                        tilescore += 40;
                    /* monster doesn't know of the target */
                    else if (!mcanspotmon(mon, mtmp))
                        continue;
                    /* target is hostile */
                    else if (mm_aggression(mon, mtmp))
                        tilescore += 20;
                    /* ally/peaceful */
                    else if ((mtmp == &youmonst && mon->mpeaceful) ||
                        (mtmp != &youmonst &&
                         mon->mpeaceful == mtmp->mpeaceful))
                        tilescore -= 10;

                    tilescore /= (distmin(x, y, xx, yy) + 1);
                    score += tilescore;
                }
            }

            if (score > score_best) {
                x_best = x;
                y_best = y;
                score_best = score;
            }
        }
    }
    if (score_best <= 0)
        return 0;
    cc->x = x_best;
    cc->y = y_best;
    return score_best;
}

/* Returns 20 if we have a valid target, otherwise -20 on ourself. */
static int
summon_nasty_score(struct musable *m, coord *tc)
{
    struct monst *mon = m->mon;
    struct monst *target = NULL;
    schar x, y;
    tc->x = mon->mx;
    tc->y = mon->my;
    for (target = monlist(mon->dlevel); target; target = monnext(target)) {
        if (DEADMONSTER(target))
            continue;

        x = m_mx(target);
        y = m_my(target);
        if (mm_aggression(mon, target) && msensem(mon, target) &&
            throwspell(TRUE, TRUE, &x, &y, m)) {
            tc->x = x;
            tc->y = y;
            return 20;
        }
    }

    return -20;
}

static int
find_item_score(const struct monst *mon, struct obj *obj, coord *tc,
                struct musable *m)
{
    int otyp = obj->otyp;
    int score = 0;
    struct monst *mtmp;
    tc->x = 0;
    tc->y = 0;
    if (otyp == SPE_CHARM_MONSTER ||
        otyp == SCR_TAMING ||
        otyp == BULLWHIP) {
        int x, y;
        for (x = mon->mx - 1; x <= mon->mx + 1; x++) {
            for (y = mon->my - 1; y <= mon->my + 1; y++) {
                if (!isok(x, y))
                    continue;
                mtmp = m_at(mon->dlevel, x, y);
                if (!mtmp) {
                    mtmp = &youmonst;
                    if (x != u.ux || y != u.uy)
                        continue;
                }
                if (!mm_aggression(mon, mtmp))
                    continue;
                if (otyp == BULLWHIP && m_mwep(mtmp)) {
                    tc->x = x;
                    tc->y = y;
                    score = 20;
                }
                if (otyp == SPE_CHARM_MONSTER ||
                    otyp == SCR_TAMING) {
                    if (mtmp == &youmonst)
                        continue;
                    if (mon->mpeaceful == mtmp->mpeaceful)
                        /* targeting monsters we grudge with charm spells will do no good... */
                        continue;
                    score += 20;
                }
            }
        }
    } else if (otyp == SCR_STINKING_CLOUD ||
               ((otyp == SPE_FIREBALL ||
                 otyp == SPE_CONE_OF_COLD) &&
                mprof(mon, MP_SATTK) >= P_SKILLED))
        score = mon_choose_spectarget(m, obj, tc);
    else if (otyp == SPE_SUMMON_NASTY)
        score = summon_nasty_score(m, tc);
    else
        score = mon_choose_dirtarget(mon, obj, tc);
    return score;
}

/* Find an unlocking tool */
boolean
find_unlocker(struct monst *mon, struct musable *m)
{
    /* Initialize musable */
    init_musable(mon, m);

    /* look for keys
       TODO: lockpicks should require an occupation timer */
    if (find_item_obj(mon->minvent, m, FALSE, SKELETON_KEY) ||
        find_item_obj(mon->minvent, m, FALSE, LOCK_PICK) ||
        find_item_obj(mon->minvent, m, FALSE, CREDIT_CARD))
        return TRUE;

    /* check if we can cast knock, only accepting
       80%+ success rate */
    if (mon_castable(mon, SPE_KNOCK, TRUE) >= 80 &&
        (mon->mstrategy != st_ascend ||
         mon_castable(mon, SPE_KNOCK, FALSE))) {
        m->spell = SPE_KNOCK;
        m->use = MUSE_SPE;
        return TRUE;
    }

    /* look for good wands of opening */
    if (find_item_obj(mon->minvent, m, FALSE, WAN_OPENING))
        return TRUE;
    /* if we are on the ascension run, accept dig/striking as well */
    if (mon->mstrategy == st_ascend) {
        if (find_item_obj(mon->minvent, m, FALSE, WAN_DIGGING) ||
            find_item_obj(mon->minvent, m, FALSE, WAN_STRIKING))
            return TRUE;
        if (mon_castable(mon, SPE_FORCE_BOLT, FALSE))
            m->spell = SPE_FORCE_BOLT;
        else if (mon_castable(mon, SPE_DIG, FALSE))
            m->spell = SPE_DIG;
        if (m->spell) {
            m->use = MUSE_SPE;
            return TRUE;
        }
    }
    return FALSE;
}

/* TODO: Move traps/stair use/etc to seperate logic (traps should be handled in pathfinding),
   it makes no sense to have it here */
boolean
find_item(struct monst *mon, struct musable *m)
{
    init_musable(mon, m);
    if (is_animal(mon->data) || mindless(mon->data))
        return FALSE;

    struct obj *obj = NULL;
    struct trap *t;
    int x = mon->mx, y = mon->my;
    struct level *lev = mon->dlevel;
    boolean stuck = (mon == u.ustuck && sticks(youmonst.data));
    boolean immobile = (mon->data->mmove == 0);
    coord tc;
    int fraction;
    m->x = 0;
    m->y = 0;
    m->z = 0;

    /* Find amount of hostiles seen/sensed and the closest one */
    int hostvis = 0;
    int hostsense = 0;
    int hostrange = 0;
    struct monst *mtmp;
    struct monst *mclose = NULL;
    if (mm_aggression(mon, &youmonst) && msensem(mon, &youmonst)) {
        hostsense++;
        if (m_cansee(mon, u.ux, u.uy) ||
            (msensem(mon, &youmonst) & MSENSE_ANYVISION)) {
            hostvis++;
            hostrange = dist2(mon->mx, mon->my, u.ux, u.uy);
            mclose = &youmonst;
        }
    }

    for (mtmp = mon->dlevel->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp) ||
            !mm_aggression(mon, mtmp) ||
            !msensem(mon, mtmp))
            continue;
        hostsense++;
        if (m_cansee(mon, mtmp->mx, mtmp->my) ||
            (msensem(mon, mtmp) & MSENSE_ANYVISION)) {
            hostvis++;
            if (!hostrange ||
                hostrange > dist2(mon->mx, mon->my, mtmp->mx, mtmp->my)) {
                hostrange = dist2(mon->mx, mon->my, mtmp->mx, mtmp->my);
                mclose = mtmp;
            }
        }
    }

    /* range of 100 is the cap on fireball, cone of cold and summon nasty */
    if (mclose && hostrange > 100)
        mclose = NULL; /* no close targets */

    m->obj = NULL;
    m->spell = 0;
    m->use = 0;

    /* Ascend */
    if (mon->sx == mon->mx && mon->sy == mon->my && mon->mstrategy == st_ascend) {
        m->use = MUSE_ASCEND;
        return TRUE;
    }

    /* Fix petrification */
    if (petrifying(mon)) {
        /* look for lizard */
        for (obj = mon->minvent; obj; obj = obj->nobj) {
            if (obj->otyp == CORPSE && obj->corpsenm == PM_LIZARD) {
                m->obj = obj;
                m->use = MUSE_EAT;
                return TRUE;
            }
        }

        /* cast s->f on self */
        if (mon_castable(mon, SPE_STONE_TO_FLESH, FALSE)) {
            m->spell = SPE_STONE_TO_FLESH;
            m->use = MUSE_SPE;
            return TRUE;
        }

        /* look for acidic corpses */
        for (obj = mon->minvent; obj; obj = obj->nobj) {
            if (obj->otyp == CORPSE && acidic(&mons[obj->corpsenm]) &&
                (obj->corpsenm != PM_GREEN_SLIME ||
                 likes_fire(mon->data) ||
                 mon->data == &mons[PM_GREEN_SLIME])) {
                m->obj = obj;
                m->use = MUSE_EAT;
                return TRUE;
            }
        }

        /* look for potions of acid */
        if (!nohands(mon->data)) {
            if (find_item_obj(mon->minvent, m, FALSE, POT_ACID))
                return TRUE;
        }

        /* polyself */
        if (mon_castable(mon, SPE_POLYMORPH, FALSE)) {
            m->spell = SPE_POLYMORPH;
            m->use = MUSE_SPE;
            return TRUE;
        }
        if (!nohands(mon->data)) {
            if (find_item_obj(mon->minvent, m, FALSE, POT_POLYMORPH))
                return TRUE;
            if (find_item_obj(mon->minvent, m, FALSE, WAN_POLYMORPH))
                return TRUE;
        }
    }

    /* Fix slime */
    if (sliming(mon)) {
        if (mon_castable(mon, SPE_CURE_SICKNESS, FALSE)) {
            m->spell = SPE_CURE_SICKNESS;
            m->use = MUSE_SPE;
            return TRUE;
        }
        if (!nohands(mon->data) &&
            (obj = m_carrying(mon, SCR_FIRE))) {
            m->obj = obj;
            m->use = MUSE_SCR;
            return TRUE;
        }
        if (!nohands(mon->data) &&
            (obj = m_carrying(mon, WAN_FIRE))) {
            m->obj = obj;
            m->use = MUSE_WAN;
            return TRUE;
        }
        if (mon_castable(mon, SPE_FIREBALL, FALSE)) {
            m->spell = SPE_FIREBALL;
            m->use = MUSE_SPE;
            if (mprof(mon, MP_SATTK) >= P_SKILLED) {
                /* target self -- for basic/unskilled and for wands, x/y is delta,
                   so there is no setting of it there, but it is needed here */
                m->x = mon->mx;
                m->y = mon->my;
                m->z = 0;
            }
            return TRUE;
        }
    }

    /* Fix other ailments */
    if (confused(mon) || stunned(mon) || blind(mon) || sick(mon) ||
        zombifying(mon)) {
        /* Sickness */
        if (sick(mon) || zombifying(mon)) {
            if (mon_castable(mon, SPE_CURE_SICKNESS, FALSE)) {
                m->spell = SPE_CURE_SICKNESS;
                m->use = MUSE_SPE;
                return TRUE;
            }
        }
        if (!is_unicorn(mon->data) && !nohands(mon->data)) {
            for (obj = mon->minvent; obj; obj = obj->nobj)
                if (obj->otyp == UNICORN_HORN && (!obj->cursed || !obj->mbknown))
                    break;
        }
        if (obj || is_unicorn(mon->data)) {
            m->obj = obj;
            m->use = MUSE_UNICORN_HORN;
            return TRUE;
        }
        /* Unihorn-less sickness: look for healing potions */
        if (sick(mon) || zombifying(mon)) {
            if (find_item_obj(mon->minvent, m, FALSE, POT_EXTRA_HEALING) ||
                find_item_obj(mon->minvent, m, FALSE, POT_FULL_HEALING))
                return TRUE;
        }
    }

    if (confused(mon)) {
        for (obj = mon->minvent; obj; obj = obj->nobj) {
            if (obj->otyp == CORPSE && obj->corpsenm == PM_LIZARD) {
                m->obj = obj;
                m->use = MUSE_EAT;
                return TRUE;
            }
        }
    }

    if (blind(mon) && find_item_obj(mon->minvent, m, FALSE, CARROT))
        return TRUE;
    /* It so happens there are two unrelated cases when we might want to check
       specifically for healing alone.  The first is when the monster is blind
       (healing cures blindness).  The second is when the monster is peaceful;
       then we don't want to flee the player, and by coincidence healing is all 
       there is that doesn't involve fleeing. These would be hard to combine
       because of the control flow. Pestilence won't use healing even when
       blind. */
    if (blind(mon) && !nohands(mon->data) &&
        mon->data != &mons[PM_PESTILENCE] && !telepathic(mon)) {
        if (mon_castable(mon, SPE_CURE_BLINDNESS, FALSE)) {
            m->spell = SPE_CURE_BLINDNESS;
            m->use = MUSE_SPE;
            return TRUE;
        }

        if (((obj = m_carrying(mon, POT_HEALING)) &&
             (!obj->cursed || !obj->mbknown)) ||
            (obj = m_carrying(mon, POT_EXTRA_HEALING)) ||
            (obj = m_carrying(mon, POT_FULL_HEALING))) {
            m->obj = obj;
            m->use = MUSE_POT;
        }
    }

    fraction = (100 * mon->mhp) / mon->mhpmax;

    /* !FH -> +EH -> +H -> !EH -> !H */
    if (mon->mpeaceful && fraction < 35) {
        if (!nohands(mon->data)) {
            if ((obj = m_carrying(mon, POT_FULL_HEALING))) {
                m->obj = obj;
                m->use = MUSE_POT;
                return TRUE;
            }

            if (mon_castable(mon, SPE_EXTRA_HEALING, FALSE)) {
                m->spell = SPE_EXTRA_HEALING;
                m->use = MUSE_SPE;
                return TRUE;
            }
            if (mon_castable(mon, SPE_HEALING, FALSE)) {
                m->spell = SPE_HEALING;
                m->use = MUSE_SPE;
                return TRUE;
            }
            if ((obj = m_carrying(mon, POT_EXTRA_HEALING)) ||
                (obj = m_carrying(mon, POT_HEALING))) {
                m->obj = obj;
                m->use = MUSE_POT;
                return TRUE;
            }
        }
    }

    if (fraction < 35 && mon != u.usteed && mon != u.ustuck) {
        if (lev->locations[x][y].typ == STAIRS && !stuck && !immobile) {
            if (x == lev->dnstair.sx && y == lev->dnstair.sy &&
                !levitates(mon))
                m->use = MUSE_DOWNSTAIRS;
            if (x == lev->upstair.sx && y == lev->upstair.sy &&
                (ledger_no(&u.uz) != 1 || !mon_has_special(mon)))
                /* Unfair to let the monsters leave the dungeon with the Amulet
                   (or go to the endlevel since you also need it, to get there)

                   unfair how? it's avoidable if you aren't silly -FIQ */
                m->use = MUSE_UPSTAIRS;
            if (lev->sstairs.sx == x && lev->sstairs.sy == y)
                m->use = MUSE_SSTAIRS;
        } else if (lev->locations[x][y].typ == LADDER && !stuck && !immobile) {
            if (x == lev->upladder.sx && y == lev->upladder.sy)
                m->use = MUSE_UP_LADDER;
            if (x == lev->dnladder.sx && y == lev->dnladder.sy &&
                !levitates(mon))
                m->use = MUSE_DN_LADDER;
        }
    }
    if (!stuck && !immobile && !m->use && mon != u.usteed) { /* FIXME: cleanup */
        /* Note: trap doors take precedence over teleport traps. */
        int xx, yy;

        for (xx = x - 1; xx <= x + 1; xx++)
            for (yy = y - 1; yy <= y + 1; yy++)
                if (isok(xx, yy))
                    if (xx != u.ux || yy != u.uy)
                        if (mon->data != &mons[PM_GRID_BUG] || xx == x ||
                            yy == y)
                            if ((xx == x && yy == y) || !lev->monsters[xx][yy])
                                if ((t = t_at(lev, xx, yy)) != 0)
                                    if ((verysmall(mon->data) ||
                                         throws_rocks(mon->data) ||
                                         phasing(mon)) ||
                                        !sobj_at(BOULDER, lev, xx, yy))
                                        if (!onscary(xx, yy, mon)) {
                                            if ((t->ttyp == TRAPDOOR ||
                                                 t->ttyp == HOLE)
                                                && !levitates(mon)
                                                && !mx_eshk(mon) && !mx_egd(mon)
                                                && !ispriest(mon) &&
                                                can_fall_thru(lev) &&
                                                fraction < 35) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->use = MUSE_TRAPDOOR;
                                            } else if (t->ttyp == TELEP_TRAP &&
                                                       m->use != MUSE_TRAPDOOR &&
                                                       fraction < 35) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->use = MUSE_TELEPORT_TRAP;
                                            } else if (t->ttyp == POLY_TRAP &&
                                                       !mon->cham &&
                                                       monstr[monsndx(mon->data)] < 6 &&
                                                       m->use != MUSE_TRAPDOOR &&
                                                       m->use != MUSE_TELEPORT_TRAP) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->use = MUSE_POLY_TRAP;
                                                return TRUE;
                                            }
                                        }
    }

    if (!nohands(mon->data) && is_mercenary(mon->data) && (obj = m_carrying(mon, BUGLE))) {
        int xx, yy;

        /* Distance is arbitrary.  What we really want to do is have the
           soldier play the bugle when it sees or remembers soldiers nearby...
        */
        for (xx = x - 3; xx <= x + 3; xx++)
            for (yy = y - 3; yy <= y + 3; yy++)
                if (isok(xx, yy))
                    if ((mtmp = m_at(lev, xx, yy)) && is_mercenary(mtmp->data) &&
                        !mx_egd(mtmp) && (mtmp->msleeping ||
                                          (!mtmp->mcanmove))) {
                        m->obj = obj;
                        m->use = MUSE_BUGLE;
                    }
    }

    /* use immediate physical escape prior to attempting magic */
    if (m->use) /* stairs, trap door or tele-trap, bugle alert */
        return TRUE;

    /* Clone ourselves */
    if (flags.no_of_wizards == 1 && !flags.double_troubled &&
        mon_castable(mon, SPE_BOOK_OF_THE_DEAD, FALSE)) {
        m->use = MUSE_SPE;
        m->spell = SPE_BOOK_OF_THE_DEAD; /* sentinel for double trouble */
        return TRUE;
    }

    if (!very_fast(mon) && !slow(mon) &&
        mon_castable(mon, SPE_SPEED_MONSTER, FALSE)) {
        m->use = MUSE_SPE;
        m->spell = SPE_SPEED_MONSTER;
        m->x = 0;
        m->y = 0;
        m->z = 0;
        return TRUE;
    }

    if (!m_mspellprot(mon) &&
        mon_castable(mon, SPE_PROTECTION, FALSE) &&
        !spell_maintained(mon, SPE_PROTECTION)) {
        m->use = MUSE_SPE;
        m->spell = SPE_PROTECTION;
        return TRUE;
    }

    int randcount = 1; /* for randomizing inventory usage */
    /* For figuring out the best use of target based stuff in particular */
    int spell_best = 0;
    int score = 0;
    int score_best = 0;
    int spell;
    coord tc_best;
    tc_best.x = 0;
    tc_best.y = 0;
    struct musable m2; /* for find_item_single */
    init_musable(m->mon, &m2);
    int usable = 0;

    /* Spell handling */
    for (spell = SPE_DIG; spell != SPE_BLANK_PAPER; spell++) {
        if (!mon_castable(mon, spell, FALSE))
            continue;

        obj = mktemp_sobj(level, spell);
        obj->blessed = obj->cursed = 0;
        obj->quan = 20L;
        /* when adding more spells where this matters, change this */
        if (((obj->otyp == SPE_DETECT_MONSTERS) &&
             mprof(mon, MP_SDIVN) >= P_SKILLED) ||
            ((obj->otyp == SPE_REMOVE_CURSE) &&
             mprof(mon, MP_SCLRC) >= P_SKILLED))
            obj->blessed = 1;

        usable = find_item_single(obj, TRUE, &m2, mclose ? TRUE : FALSE, FALSE);
        if (usable && mon_allowed(spell)) {
            if (usable != 1) {
                score = find_item_score(mon, obj, &tc, m);
                if (score > score_best) {
                    tc_best = tc;
                    spell_best = spell;
                    score_best = score;
                }
            } else if (!rn2(randcount)) {
                randcount++;
                m->spell = spell;
                m->x = m2.x;
                m->y = m2.y;
                m->z = m2.z;
                m->use = MUSE_SPE;
            }
        }
        obfree(obj, NULL);
    }

    /* if we can cast a spell, do so 3/4 of the time unless it's the only
       thing we can do (read: we lack hands) */
    if ((rn2(4) || nohands(mon->data)) && (m->use || spell_best)) {
        if (spell_best && (score_best > 20 ? rn2(3) : !rn2(3))) {
            m->x = tc_best.x;
            m->y = tc_best.y;
            m->z = 0;
            m->use = MUSE_SPE;
            m->spell = spell_best;
            return TRUE;
        }
        return TRUE;
    }

    /* at this point, if we lack hands, there's nothing we can do */
    if (nohands(mon->data))
        return FALSE;

    /* If the monster is heading for ascension, ignore nonspecific items
       2/3 of the time. This prevents monsters wasting valuable turns.
       Using spells is still OK since monsters only cast spells when they
       are able to anyway */
    if (mon->mstrategy == st_ascend && rn2(3))
        return FALSE;

    /* Object handling. find_item_obj is given an object chain,
       and if containers are found, those are checked recursively.
       Deeper objects are not considered before objects in the current
       chain. */
    init_musable(mon, m); /* reset musable */

    if (find_item_obj(mon->minvent, m, mclose ? TRUE : FALSE, 0))
        return TRUE; /* find_item_obj handles musable */

    /* If we are here, there was no relevant objects found.
       Stash fragile objects in our open inventory in a container.
       To avoid potential oscillation if an item is only borderline
       usable, ignore this action 19/20 of the time if hostiles are
       present */
    if (rn2(20) && mclose)
        return FALSE;

    struct obj *container = NULL;
    for (container = mon->minvent; container; container = container->nobj) {
        /* This check might seem backwards, but this way monsters can stash
           cancellation even if the first container happens to be a BoH */
        if (container->otyp == SACK ||
            container->otyp == OILSKIN_SACK ||
            (container->otyp == BAG_OF_HOLDING &&
             (!container->cursed || !container->mbknown))) {
            for (obj = mon->minvent; obj; obj = obj->nobj) {
                if ((obj->oclass == SPBOOK_CLASS ||
                     obj->oclass == SCROLL_CLASS ||
                     obj->oclass == POTION_CLASS ||
                     obj->oclass == WAND_CLASS) &&
                    ((obj->otyp != WAN_CANCELLATION &&
                      obj->otyp != BAG_OF_TRICKS && /* should never happen in current code */
                      obj->otyp != BAG_OF_HOLDING) ||
                     container->otyp != BAG_OF_HOLDING)) {
                    m->obj = container;
                    m->use = MUSE_CONTAINER;
                    return TRUE;
                }
            }
        }
    }

    /* found nothing to do */
    return FALSE;
}

boolean
find_item_obj(struct obj *chain, struct musable *m,
              boolean close, int specobj)
{
    /* If specobj is nonzero (an object type), only look for that object, performing
       only sanity checks (not discharged, can read if scroll, etc) */
    if (!chain)
        /* monster inventory is empty, or checked container is */
        return FALSE;

    int randcount = 1;
    int usable;
    int score = 0;
    int score_best = 0;
    coord tc;
    coord tc_best;
    struct monst *mon = m->mon;
    struct obj *obj;
    struct musable m2;
    init_musable(mon, &m2);
    struct obj *obj_best = NULL;

    for (obj = chain; obj; obj = obj->nobj) {
        /* Containers */
        if ((obj->otyp == SACK ||
             obj->otyp == OILSKIN_SACK ||
             obj->otyp == BAG_OF_HOLDING) &&
            !m->use) {
            /* check for cursed bag of holding */
            if (obj->otyp == BAG_OF_HOLDING && obj->cursed) {
                if (obj->mbknown)
                    continue; /* monster knows it's cursed, ignore it */
                /* only make stuff vanish if it isn't chained inside
                   another bag */
                if (chain == mon->minvent) {
                    int vanish = 0;
                    struct obj *otmp, *nextotmp;
                    for (otmp = obj->cobj; otmp; otmp = nextotmp) {
                        nextotmp = otmp->nobj;
                        if (rn2(13))
                            continue;
                        /* something vanished, monster knows BUC now... */
                        obj->mbknown = 1;
                        obj_extract_self(otmp);
                        obfree(otmp, NULL);
                        vanish++;
                    }
                    obj->owt = weight(obj); /* if anything disappeared */
                    if (vanish && canseemon(mon)) {
                        pline(msgc_itemloss,
                              "You barely notice %s item%s disappearing from %s bag!",
                              vanish > 5 ? "several" :
                              vanish > 1 ? "a few" :
                              "an", vanish != 1 ? "s" : "",
                              mon_nam(mon));
                        obj->bknown = 1;
                        makeknown(obj->otyp);
                    }
                }
            }

            /* This container is inside another, and the monster doesn't
               know what is present. inside. Take it out if we're safe. */
            if (!obj->mknown && chain != mon->minvent) {
                if (!close) {
                    m->use = MUSE_CONTAINER;
                    m->obj = obj;
                } else /* we aren't safe, ignore the chained bag for now */
                    continue;
            } else if ((obj->mknown = 1) &&
                       find_item_obj(obj->cobj, m, close, specobj)) {
                /* obj->mknown = 1 to mark the container as recognized -- monster knows what's inside */
                /* Check if there's something interesting in it */
                m->use = MUSE_CONTAINER; /* take the object out */

                /* if this bag is inside another, point to the bag itself */
                if (chain != mon->minvent)
                    m->obj = obj;
            }
            continue;
        }
        if (specobj && obj->otyp != specobj)
            continue;
        usable = find_item_single(obj, FALSE, &m2, close, !!specobj);
        if (usable && mon_allowed(obj->otyp)) {
            if (usable != 1) {
                score = find_item_score(mon, obj, &tc, m);
                if (score > score_best) {
                    tc_best = tc;
                    obj_best = obj;
                    score_best = score;
                }
            } else if (!rn2(randcount)) {
                randcount++;
                m->obj = obj;
                m->x = m2.x;
                m->y = m2.y;
                m->z = m2.z;
                m->use = (obj->oclass == WAND_CLASS   ? MUSE_WAN  :
                          obj->oclass == SCROLL_CLASS ? MUSE_SCR  :
                          obj->oclass == POTION_CLASS ? MUSE_POT  :
                          obj->oclass == SPBOOK_CLASS ? MUSE_BOOK :
                          obj->oclass == FOOD_CLASS   ? MUSE_EAT  :
                          obj->otyp == SKELETON_KEY   ? MUSE_KEY  :
                          obj->otyp == CREDIT_CARD    ? MUSE_KEY  :
                          obj->otyp == LOCK_PICK      ? MUSE_KEY  :
                          obj->otyp == BAG_OF_TRICKS  ? MUSE_BAG_OF_TRICKS :
                          0);
                if (m->use == 0)
                    impossible("AI error: Unhandled nondirectional musable for obj: %s",
                               killer_xname(obj));
            }
        }
    }

    if (obj_best || m->use) {
        if (!m->use || (obj_best && (score_best > 20 ? rn2(3) : !rn2(3)))) {
            m->x = tc_best.x;
            m->y = tc_best.y;
            m->z = 0;
            m->use = (obj_best->oclass == WAND_CLASS   ? MUSE_WAN :
                      obj_best->oclass == SCROLL_CLASS ? MUSE_SCR :
                      obj_best->oclass == POTION_CLASS ? MUSE_THROW :
                      obj_best->oclass == TOOL_CLASS   ? MUSE_DIRHORN :
                      obj_best->otyp == BULLWHIP       ? MUSE_BULLWHIP :
                      obj_best->otyp == EGG            ? MUSE_THROW :
                      0);
            if (m->use == 0)
                impossible("AI error: Unhandled directional musable for obj: %s",
                           killer_xname(obj_best));
            m->obj = obj_best;
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

/* Check a single item or spell if it's usable.
   Returns:
   2: perform a scoring for target logic
   1: usable
   0: non-usable
   TODO: maybe make this into a switch statement
   If specific is true, only perform sanity checks, return true after
   Monsters wont know to be careful with scrolls if confused, and
   (except for gain level to avoid serious issues) always assume uncursed
   if BUC is unknown. This is by design. Otherwise, they always try
   to use items semi-"intelligently" */
static int
find_item_single(struct obj *obj, boolean spell, struct musable *m, boolean close,
                 boolean specific)
{
    struct monst *mon = m->mon;
    boolean stuck = (mon == u.ustuck && sticks(youmonst.data));
    int x = mon->mx, y = mon->my;
    struct level *lev = mon->dlevel;
    m->x = 0;
    m->y = 0;
    m->z = 0;

    /* kludge to cut down on trap destruction (particularly portals) */
    struct trap *t = t_at(lev, x, y);
    if (t &&
        (t->ttyp == PIT || t->ttyp == SPIKED_PIT || t->ttyp == WEB ||
         t->ttyp == BEAR_TRAP))
        t = NULL;  /* ok for monster to dig here */
    int fraction = 100 * mon->mhp / mon->mhpmax;
    int otyp = obj->otyp;
    int oclass = obj->oclass;
    int spe = 1;
    int recharged = 0;
    boolean cursed = FALSE;
    boolean blessed = FALSE;
    if (obj->mknown) {
        spe = obj->spe;
        recharged = obj->recharged;
    }
    if (spell || obj->mbknown) {
        cursed = obj->cursed;
        blessed = obj->blessed;
    }
    struct obj *otmp;

    /* BEGIN SANITY CHECKS */

    if (spell && !mon_castable(mon, otyp, specific))
        return 0;

    if (!spell && oclass == SPBOOK_CLASS) {
        /* don't bother with those */
        if (otyp == SPE_BLANK_PAPER || otyp == SPE_BOOK_OF_THE_DEAD)
            return 0;

        /* spellcasters or monsters min. XL14 only */
        if (!spellcaster(mon->data) && mon->m_lev < 14)
            return 0;

        /* avoid cursed books */
        if (cursed)
            return 0;

        /* Aligned priests (and high ones) will not read nonblessed spellbooks
           at all to avoid ruining the temple if the book teleports them */
        if (ispriest(mon) && !blessed)
            return 0;

        /* monsters don't know how hard a book is unless mknown is set
           (from the identify spell or a spellcaster trying earlier).
           If they do, only read books with min. success rate 15 */
        if (study_rate(mon, obj) < 15 && obj->mknown && !blessed)
            return 0;

        /* we know the spell already */
        if (mon_castable(mon, otyp, TRUE))
            return 0;
    }

    /* this wand would explode on use */
    if (oclass == WAND_CLASS && mprof(mon, MP_WANDS) == P_UNSKILLED && cursed)
        return 0;

    /* discharged wand/tool */
    if ((oclass == WAND_CLASS ||
         (oclass == TOOL_CLASS &&
          !is_weptool(obj) && objects[otyp].oc_charged)) &&
        spe <= 0 && (obj->mknown || mprof(mon, P_WANDS) >= P_SKILLED) &&
        (otyp != WAN_WISHING || !recharged || close)) /* wrest wishing if safe */
        return 0;

    /* mknown -> monster knows appearance (only allows scrolls, not books,
       similar to players) */
    if (((oclass == SCROLL_CLASS && !obj->mknown) ||
         (!spell && oclass == SPBOOK_CLASS)) &&
        (blind(mon) || !haseyes(mon->data)))
        return 0;

    /* END SANITY CHECKS */
    if (specific)
        return 1;

    /* set mknown on scrolls if we aren't blind (to mark known appearance).
       TODO: figure out a better place to do it */
    if (oclass == SCROLL_CLASS && !blind(mon) && haseyes(mon->data))
        obj->mknown = 1;

    /* spellbooks for spells we haven't learned yet */
    if (!spell && oclass == SPBOOK_CLASS)
        return !close; /* only read books if there's nothing dangerous in sight */

    if (otyp == SPE_CHARGING ||
        (otyp == SCR_CHARGING && !cursed)) {
        for (otmp = mon->minvent; otmp; otmp = otmp->nobj) {
            if (otmp->mknown &&
                ((otmp->oclass == WAND_CLASS ||
                  (otmp->oclass == TOOL_CLASS &&
                   !is_weptool(otmp) && objects[otyp].oc_charged)) &&
                 spe <= 0)) {
                /* only blessed-charge /oW */
                if (otmp->otyp == WAN_WISHING && !blessed)
                    continue;

                /* don't charge these if we know it's 1:x */
                if ((otmp->otyp == WAN_WISHING || otmp->otyp == MAGIC_MARKER) &&
                    (otmp->recharged && otmp->mknown))
                    continue;
                return 1;
            }
        }
        return 0;
    }

    if (otyp == SPE_REMOVE_CURSE ||
        (otyp == SCR_REMOVE_CURSE && !cursed))
        for (otmp = mon->minvent; otmp; otmp = otmp->nobj)
            if (otmp->cursed && otmp->mbknown &&
                (otmp->owornmask || otmp->otyp == LOADSTONE || blessed))
                return 1;

    if (otyp == SPE_IDENTIFY ||
        otyp == SCR_IDENTIFY) {
        int lvl = mprof(mon, MP_SDIVN);
        if (otyp == SCR_IDENTIFY)
            lvl = blessed ? P_EXPERT : cursed ? P_UNSKILLED : P_BASIC;
        for (otmp = mon->minvent; otmp; otmp = otmp->nobj)
            if (otmp->otyp != GOLD_PIECE &&
                ((!otmp->mknown && lvl >= P_BASIC) ||
                 (!otmp->mbknown && lvl >= P_SKILLED)) &&
                otmp != obj)
                return 1;
    }

    /* Defensive only */
    if (fraction < 35) {
        if ((otyp == WAN_DIGGING || otyp == SPE_DIG) && !stuck && !t &&
            !mx_eshk(mon) && !mx_egd(mon) && !ispriest(mon) &&
            !levitates(mon)
            /* monsters digging in Sokoban can ruin things */
            && !In_sokoban(m_mz(mon))
            /* digging wouldn't be effective; assume they know that */
            && !(lev->locations[x][y].flags & W_NONDIGGABLE)
            && !(Is_botlevel(m_mz(mon)) || In_endgame(m_mz(mon)))
            && !(is_ice(lev, x, y) || is_pool(lev, x, y) || is_lava(lev, x, y))
            && (!(mon->data == &mons[PM_VLAD_THE_IMPALER]
                  || In_V_tower(m_mz(mon))))) {
            m->z = 1; /* digging down */
            return 1;
        }

        /* c!oGL is pretty much reverse digging */
        if (otyp == POT_GAIN_LEVEL && cursed &&
            !mx_eshk(mon) && !mx_egd(mon) && !ispriest(mon) &&
            !In_sokoban(m_mz(mon)) &&
            !In_endgame(m_mz(mon)))
            return 1;

        if (mon->data != &mons[PM_PESTILENCE]) {
            if (otyp == POT_FULL_HEALING ||
                otyp == POT_EXTRA_HEALING ||
                otyp == POT_HEALING ||
                otyp == SPE_EXTRA_HEALING ||
                otyp == SPE_HEALING)
                return 1;
        } else {        /* Pestilence */
            if (otyp == POT_SICKNESS)
                return 1;
        }

        /* Only when hostiles are near */
        if (close) {
            if ((otyp == WAN_TELEPORTATION ||
                 otyp == SPE_TELEPORT_AWAY ||
                 (otyp == SCR_TELEPORTATION &&
                  !(mx_eshk(mon) && inhishop(mon)))) &&
                !mx_egd(mon) && !ispriest(mon) && !tele_wary(mon))
                if (!mon_has_amulet(mon) || (otyp != SCR_TELEPORTATION && mfind_target(mon, FALSE)))
                    return mon_has_amulet(mon) ? 2 : 1;

            if (otyp == SCR_GENOCIDE && !cursed)
                return 1;
        }
    }

    /* Primarily offensive */
    if (fraction >= 35 || !rn2(4)) {
        /* Targeting needs extra checks */
        if ((otyp == WAN_FIRE ||
             otyp == SPE_FIREBALL ||
             otyp == FIRE_HORN ||
             otyp == WAN_COLD ||
             otyp == SPE_CONE_OF_COLD ||
             otyp == FROST_HORN ||
             otyp == WAN_LIGHTNING ||
             otyp == SPE_MAGIC_MISSILE ||
             otyp == WAN_MAGIC_MISSILE ||
             otyp == SPE_FORCE_BOLT ||
             otyp == WAN_STRIKING ||
             otyp == SPE_DRAIN_LIFE ||
             otyp == SCR_STINKING_CLOUD ||
             otyp == WAN_UNDEAD_TURNING ||
             otyp == SPE_TURN_UNDEAD ||
             otyp == WAN_SLOW_MONSTER ||
             otyp == SPE_SLOW_MONSTER ||
             otyp == WAN_CANCELLATION ||
             otyp == SPE_CANCELLATION ||
             otyp == SCR_TAMING ||
             otyp == SPE_CHARM_MONSTER ||
             otyp == SPE_SUMMON_NASTY ||
             otyp == POT_BLINDNESS ||
             otyp == POT_CONFUSION ||
             otyp == POT_ACID) &&
            close)
            return 2;
    }

    if ((otyp == WAN_MAKE_INVISIBLE ||
         otyp == WAN_POLYMORPH ||
         otyp == SPE_POLYMORPH ||
         otyp == WAN_SPEED_MONSTER ||
         otyp == SPE_SPEED_MONSTER ||
         otyp == SPE_HEALING ||
         otyp == SPE_EXTRA_HEALING ||
         otyp == SPE_STONE_TO_FLESH ||
         otyp == WAN_DEATH ||
         otyp == SPE_FINGER_OF_DEATH ||
         otyp == WAN_MAGIC_MISSILE ||
         otyp == SPE_MAGIC_MISSILE ||
         otyp == WAN_SLEEP ||
         otyp == SPE_SLEEP ||
         otyp == POT_PARALYSIS ||
         otyp == POT_SLEEPING ||
         otyp == EGG) && /* trice */
        close)
        return 2;

    /* tame monsters wont zap wishing */
    if (otyp == WAN_WISHING && !mon->mtame)
        return 1;

    /* only quaff unIDed !oGL if we can't ID it somehow (prevents shopkeepers/priests from quaffing c!oGL mostly) */
    if (otyp == POT_GAIN_LEVEL && !cursed &&
        (obj->mbknown ||
         (mon_castable(mon, SPE_IDENTIFY, TRUE) < 50 &&
          !m_carrying(mon, SCR_IDENTIFY))))
        return 1;

    if (otyp == POT_SEE_INVISIBLE && !see_invisible(mon) && !cursed)
        return 1;

    /* If peaceful, only selfinvis if hero can't see invisible.
       There is a similar check in choose_dirtarget. */
    if ((otyp == SPE_INVISIBILITY ||
         otyp == POT_INVISIBILITY) &&
        !invisible(mon) && !binvisible(mon) &&
        (!mon->mpeaceful || see_invisible(&youmonst)) &&
        (!attacktype(mon->data, AT_GAZE) || cancelled(mon)))
        return 1;

    if ((otyp == SPE_POLYMORPH ||
         otyp == POT_POLYMORPH) && !mon->cham &&
        monstr[monsndx(mon->data)] < 6)
        return 1;

    if (otyp == POT_SPEED && !very_fast(mon))
        return 1;

    if (otyp == BULLWHIP && !rn2(2) && close &&
        (!mon->mw || !(mon->mw)->cursed ||
         !(mon->mw)->mbknown || mon->mw == obj))
        return 2;

    if ((otyp == SPE_DETECT_MONSTERS ||
         otyp == POT_MONSTER_DETECTION) &&
        !detects_monsters(mon) &&
        blessed)
        return 1;

    if (otyp == SCR_ENCHANT_WEAPON && mon->mw && objects[mon->mw->otyp].oc_charged &&
        mon->mw->spe < 6 && !cursed)
        return 1;

    if ((otyp == SCR_ENCHANT_ARMOR || otyp == SCR_DESTROY_ARMOR) && !cursed) {
        /* find armor */
        int ret = 0;
        for (otmp = mon->minvent; otmp; otmp = otmp->nobj) {
            if (mon->mw && mon->mw == otmp)
                continue;
            /* Enchant/destroy armor only affects worn armor */
            if (!(otmp->owornmask & W_ARMOR))
                continue;
            /* only use destroy armor if all worn armor is cursed */
            if (otyp == SCR_DESTROY_ARMOR && (!otmp->cursed || otmp->spe > 0))
                return 0;
            /* avoid enchanting if there's something > +3 */
            if (otyp == SCR_ENCHANT_ARMOR && otmp->spe > 3)
                return 0;
            ret = 1;
        }
        return ret;
    }

    if ((((otyp == WAN_CREATE_MONSTER ||
           otyp == SPE_CREATE_MONSTER ||
           otyp == SCR_CREATE_MONSTER ||
           otyp == BAG_OF_TRICKS ||
           (otyp == SCR_GENOCIDE && cursed)) &&
          !mon->mpeaceful) || /* create monster makes no sense for peacefuls */
         otyp == SPE_CREATE_FAMILIAR) &&
        close)
        return 1;

    return 0;
}

/* Use item or dungeon feature (TODO: make it only items).
   Returns 0: can act again, 1: died, 2: can not act again */
int
use_item(struct musable *m)
{
    struct monst *mon = m->mon;
    struct obj *obj = m->obj;
    int i;
    if (obj &&
        m->use != MUSE_SPE &&       /* MUSE_SPE deals with m->spell, not m->obj */
        m->use != MUSE_THROW && /* thrown potions never release ghosts/djinni */
        m->use != MUSE_CONTAINER && /* BoH vanish logic is performed in find_item_obj */
        (i = precheck(obj, m)))
        return i;
    boolean vis = cansee(mon->mx, mon->my);
    boolean vismon = mon_visible(mon);
    boolean oseen = obj && vismon;
    boolean known = FALSE; /* for *effects */
    struct monst *mtmp = NULL;
    struct obj *otmp = NULL;
    struct obj *container = NULL; /* for contained objects */
    struct rm *door; /* for muse_key */
    boolean btrapped;
    int x, y;

    if (oseen)
        examine_object(obj);

    switch (m->use) {
    case MUSE_SPE:
        spelleffects(FALSE, m);
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_SCR:
        mreadmsg(mon, obj);
        obj->in_use = TRUE;
        if (!seffects(mon, obj, &known)) {
            if (!objects[obj->otyp].oc_name_known) {
                if (known) {
                    makeknown(obj->otyp);
                    more_experienced(0, 10);
                } else if (!objects[obj->otyp].oc_uname)
                    docall(obj);
            }
            if (obj->otyp != SCR_BLANK_PAPER)
                m_useup(mon, obj);
            else
                obj->in_use = FALSE;
        }
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_POT:
        mquaffmsg(mon, obj);
        int nothing = 0; /* nothing happened (pelicular feeling) */
        int unkn = 0; /* unknown for other reasons */
        peffects(mon, obj, &nothing, &unkn);
        if (!nothing && !unkn && oseen)
            makeknown(obj->otyp);

        m_useup(mon, obj);
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_WAN:
        if (objects[obj->otyp].oc_dir != NODIR &&
            !m->x && !m->y && !m->z)
            mzapmsg(mon, obj, TRUE);
        else
            mzapmsg(mon, obj, FALSE);
        if (zappable(mon, obj))
            weffects(mon, obj, m->x, m->y, m->z);
        if (obj && obj->spe < 0) {
            if (oseen)
                pline(msgc_itemloss,
                      "%s to dust.", Tobjnam(obj, "turn"));
            m_useup(mon, obj);
        }
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_THROW:
        if (cansee(mon->mx, mon->my)) {
            obj->dknown = 1;
            pline(msgc_monneutral,
                  "%s hurls %s!", Monnam(mon), singular(obj, doname));
        }
        m_throw(mon, mon->mx, mon->my, m->x, m->y,
                distmin(mon->mx, mon->my, m->x, m->y), obj,
                !distant(mon));
        return 2;
    case MUSE_EAT:
        dog_eat(mon, obj, mon->mx, mon->my, FALSE);
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_BOOK:
        /* reading a book, not casting a spell, that is MUSE_SPE */
        if (!mon_study_book(mon, obj))
            return 0;
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_CONTAINER:
        /* for picking stuff from containers */
        if (obj->where == OBJ_CONTAINED) {
            container = obj->ocontainer;
            obj_extract_self(obj);
            mpickobj(mon, obj, &obj);
            if (cansee(mon->mx, mon->my))
                pline(msgc_monneutral,
                      "%s removes %s from %s %s.", Monnam(mon),
                      an(xname(obj)), mhis(mon), xname(container));
            return 2;
        }
        /* for stashing stuff */
        for (container = mon->minvent; container; container = container->nobj) {
            if (container == obj) {
                struct obj *nobj;
                int contained = 0;
                for (otmp = mon->minvent; otmp; otmp = nobj) {
                    nobj = otmp->nobj;
                    if ((otmp->oclass == SPBOOK_CLASS ||
                         otmp->oclass == SCROLL_CLASS ||
                         otmp->oclass == POTION_CLASS ||
                         otmp->oclass == WAND_CLASS) &&
                        ((otmp->otyp != WAN_CANCELLATION &&
                          otmp->otyp != BAG_OF_TRICKS && /* should never happen in current code */
                          otmp->otyp != BAG_OF_HOLDING) ||
                         container->otyp != BAG_OF_HOLDING)) {
                        obj_extract_self(otmp);
                        add_to_container(container, otmp);
                        contained++;
                    }
                }
                if (contained) {
                    if (vismon)
                        pline(msgc_monneutral,
                              "%s stashes %s item%s in %s %s.", Monnam(mon),
                              contained > 5 ? "several" :
                              contained > 1 ? "a few" :
                              "an", contained != 1 ? "s" : "", mhis(mon), xname(obj));
                    return 2;
                }
                impossible("monster wanted to stash objects, but there was nothing to stash?");
                return 0;
            }
        }
        impossible("container for monster not found?");
        return 0;
    case MUSE_UNICORN_HORN:
        if (vismon) {
            if (obj)
                pline(msgc_monneutral,
                      "%s uses a unicorn horn!", Monnam(mon));
            else
                pline(msgc_monneutral,
                      "The tip of %s's horn glows!", mon_nam(mon));
        }
        if (obj && obj->cursed) {
            enum youprop prop = 0;
            switch (rn2(3)) {
            case 0:
                prop = BLINDED;
                break;
            case 1:
                prop = CONFUSION;
                break;
            case 2:
                prop = STUNNED;
            }
            if (!has_property(mon, prop) &&
                (prop != STUNNED || !resists_stun(mon))) {
                /* well, they know it's cursed now... */
                obj->mbknown = 1;
            }
            if (prop == STUNNED && !resists_stun(mon)) {
                if (vismon)
                    pline(msgc_monneutral,
                          "Nothing seems to happen.");
            } else
                set_property(mon, prop, rnd(100), FALSE);
        } else {
            set_property(mon, BLINDED, -2, FALSE);
            set_property(mon, CONFUSION, -2, FALSE);
            set_property(mon, STUNNED, -2, FALSE);
            set_property(mon, SICK, -2, FALSE);
            set_property(mon, ZOMBIE, -2, FALSE);
        }
        return 2;
    case MUSE_DIRHORN:
        if (oseen)
            pline(msgc_monneutral,
                  "%s plays a %s!", Monnam(mon), xname(obj));
        else
            You_hear(msgc_levelwarning, "a horn being played.");
        if (obj->spe < 1) {
            if (vismon)
                pline(msgc_substitute,
                      "%s produces a frightful, grave sound.", Monnam(mon));
            awaken_monsters(mon, mon->m_lev * 30);
            obj->mknown = 1;
            return 2;
        }
        obj->spe--;
        if (oseen)
            makeknown(obj->otyp);
        buzz(-30 - ((obj->otyp == FROST_HORN) ? AD_COLD - 1 : AD_FIRE - 1),
             m_mlev(mon), mon->mx, mon->my, m->x, m->y, 0);
        return DEADMONSTER(mon) ? 1 : 2;
    case MUSE_BUGLE:
        if (vismon)
            pline(msgc_levelwarning, "%s plays %s!", Monnam(mon), doname(obj));
        else
            You_hear(msgc_levelwarning, "a bugle playing reveille!");
        awaken_soldiers(mon);
        return 2;
    case MUSE_BAG_OF_TRICKS:
        if (vismon)
            pline(msgc_monneutral,
                  "%s reaches into %s!", Monnam(mon),
                  an(doname(obj)));
        if (obj->spe < 1) {
            if (vismon)
                pline(msgc_failcurse, "But nothing happens...");
            obj->mknown = 1; /* monster learns it's discharged */
            return 2;
        }
        obj->spe--;
        if (create_critters(!rn2(23) ? rn1(7, 2) : 1,
                            NULL, mon->mx, mon->my))
            makeknown(BAG_OF_TRICKS);
        return 2;
    case MUSE_KEY:
        x = mon->mx + m->x; /* revert from delta */
        y = mon->my + m->y;
        if (m->tobj) { /* monster unlocking a container */
            struct obj *otmp = m->tobj;
            /* sanity check */
            if (otmp->ox != x || otmp->oy != y) {
                impossible("monster unlocking box at bogus location?");
                return 0;
            }
            otmp->olocked = 0;
            if (vismon)
                pline(msgc_levelwarning, "%s %s %s.", Monnam(mon),
                      otmp->otyp == SKELETON_KEY ? "unlocks" :
                      "succeeds in picking the lock on",
                      an(xname(otmp)));
            if (otmp->otrapped)
                chest_trap(mon, otmp, FINGER, FALSE);
            return DEADMONSTER(mon) ? 1 : 2; /* in case chest trap killed */
        }
        door = &level->locations[x][y];
        btrapped = (door->flags & D_TRAPPED);
        if (vismon)
            pline(msgc_levelwarning, "%s %s a door.", Monnam(mon),
                  obj->otyp == SKELETON_KEY ? "unlocks" :
                  "succeeds in picking the lock on");
        else
            You_hear(msgc_levelwarning, "a door unlock.");
        door->flags = btrapped ? D_NODOOR : D_CLOSED;
        newsym(x, y);
        unblock_point(x, y); /* vision */
        if (btrapped && mb_trapped(mon))
            return 1;
        return 2;
    case MUSE_ASCEND:
        /* give full messaging unconditionally -- the player has lost at this point */
        if (!canseemon(mon))
            pline(msgc_outrobad, "You notice a ritual going on nearby...");
        pline(msgc_outrobad,
              "%s offers the Amulet of Yendor to %s...", Monnam(mon),
              a_gname_at(mon->mx, mon->my));
        pline(msgc_outrobad,
              "An invisible choir sings, and %s is bathed in radiance...",
              mon_nam(mon));
        pline(msgc_outrobad, "A voice booms out...");
        pline(msgc_outrobad,
              "Congratulations, %s!", mortal_or_creature(mon->data, TRUE));
        pline(msgc_outrobad,
              "In return for thy service, I grant thee the gift of %s!",
              nonliving(mon->data) ? "Eternal Power" : "Immortality");
        pline(msgc_outrobad, "%s ascends to the status of Demigod%s...",
              Monnam(mon), mon->female ? "dess" : "");
        const char *ebuf;
        ebuf = msgprintf("was beaten to the ascension by %s.",
                         k_monnam(mon));
        done(ESCAPED, ebuf);
        return 2;
    case MUSE_TRAPDOOR:
        /* trap doors on "bottom" levels of dungeons are rock-drop trap doors,
           not holes in the floor.  We check here for safety. */
        if (Is_botlevel(&u.uz))
            return 0;
        if (vis) {
            struct trap *t;

            t = t_at(level, trapx, trapy);
            pline(msgc_monneutral, "%s %s into a %s!", Monnam(mon),
                  makeplural(locomotion(mon->data, "jump")),
                  t->ttyp == TRAPDOOR ? "trap door" : "hole");
            if (level->locations[trapx][trapy].typ == SCORR) {
                level->locations[trapx][trapy].typ = CORR;
                unblock_point(trapx, trapy);
            }
            seetrap(t_at(level, trapx, trapy));
        }

        /* don't use rloc_to() because worm tails must "move" */
        remove_monster(level, mon->mx, mon->my);
        newsym(mon->mx, mon->my);     /* update old location */
        place_monster(mon, trapx, trapy, TRUE);
        if (mon->wormno)
            worm_move(mon);
        newsym(trapx, trapy);

        migrate_to_level(mon, ledger_no(&u.uz) + 1, MIGR_RANDOM, NULL);
        return 2;
    case MUSE_UPSTAIRS:
        /* Monsters without amulets escape the dungeon and are gone for good
           when they leave up the up stairs. Monsters with amulets would reach
           the endlevel, which we cannot allow since that would leave the
           player stranded. */
        if (ledger_no(&u.uz) == 1) {
            if (mon_has_special(mon))
                return 0;
            if (vismon)
                pline(msgc_monneutral, "%s escapes the dungeon!", Monnam(mon));
            mongone(mon);
            return 2;
        }

        if (vismon)
            pline(msgc_monneutral, "%s escapes upstairs!", Monnam(mon));
        migrate_to_level(mon, ledger_no(&u.uz) - 1, MIGR_STAIRS_DOWN,
                         NULL);
        return 2;
    case MUSE_DOWNSTAIRS:
        if (vismon)
            pline(msgc_monneutral, "%s escapes downstairs!", Monnam(mon));
        migrate_to_level(mon, ledger_no(&u.uz) + 1, MIGR_STAIRS_UP, NULL);
        return 2;
    case MUSE_UP_LADDER:
        if (vismon)
            pline(msgc_monneutral, "%s escapes up the ladder!", Monnam(mon));
        migrate_to_level(mon, ledger_no(&u.uz) - 1, MIGR_LADDER_DOWN, NULL);
        return 2;
    case MUSE_DN_LADDER:
        if (vismon)
            pline(msgc_monneutral, "%s escapes down the ladder!", Monnam(mon));
        migrate_to_level(mon, ledger_no(&u.uz) + 1, MIGR_LADDER_UP, NULL);
        return 2;
    case MUSE_SSTAIRS:
        /* the stairs leading up from the 1st level are */
        /* regular stairs, not sstairs.  */
        if (level->sstairs.up) {
            if (vismon)
                pline(msgc_monneutral, "%s escapes upstairs!", Monnam(mon));
            if (Inhell) {
                migrate_to_level(mon, ledger_no(&level->sstairs.tolev),
                                 MIGR_RANDOM, NULL);
                return 2;
            }
        } else if (vismon)
            pline(msgc_monneutral, "%s escapes downstairs!", Monnam(mon));
        migrate_to_level(mon, ledger_no(&level->sstairs.tolev), MIGR_SSTAIRS,
                         NULL);
        return 2;
    case MUSE_TELEPORT_TRAP:
        if (vis) {
            pline(msgc_monneutral, "%s %s onto a teleport trap!", Monnam(mon),
                  makeplural(locomotion(mon->data, "jump")));
            if (level->locations[trapx][trapy].typ == SCORR) {
                level->locations[trapx][trapy].typ = CORR;
                unblock_point(trapx, trapy);
            }
            seetrap(t_at(level, trapx, trapy));
        }
        /* don't use rloc_to() because worm tails must "move" */
        remove_monster(level, mon->mx, mon->my);
        newsym(mon->mx, mon->my);     /* update old location */
        place_monster(mon, trapx, trapy, TRUE);
        if (mon->wormno)
            worm_move(mon);
        newsym(trapx, trapy);
        mon_tele(mon, !!teleport_control(mon));
        return 2;
    case MUSE_POLY_TRAP:
        if (vismon) {
            /* If the player can see the monster jump onto a square and
               polymorph, they'll know there's a trap there even if they can't
               see the square the trap's on (e.g. infravisible monster). */
            pline(msgc_monneutral,
                  "%s deliberately %s onto a polymorph trap!", Monnam(mon),
                  makeplural(locomotion(mon->data, "jump")));
            seetrap(t_at(level, trapx, trapy));
        }

        /* don't use rloc() due to worms */
        remove_monster(level, mon->mx, mon->my);
        newsym(mon->mx, mon->my);
        place_monster(mon, trapx, trapy, TRUE);
        if (mon->wormno)
            worm_move(mon);
        newsym(trapx, trapy);

        newcham(mon, NULL, FALSE, FALSE);
        return 2;
    case MUSE_BULLWHIP:
        if (!mon->mw || mon->mw != obj) {
            if (mon->mw && (mon->mw)->cursed) {
                if (vis)
                    pline(msgc_monneutral,
                          "%s tries to wield a bullwhip, "
                          "but %s weapon is welded to %s hand!",
                          Monnam(mon), s_suffix(mon_nam(mon)),
                          mhis(mon));
                (mon->mw)->mbknown = 1;
                return 0;
            }
            /* FIXME: the below is duplicated from weapon.c, allow one to
               specify what weapon to make a monster wield */
            struct obj *mw_tmp = mon->mw;
            mon->mw = obj;  /* wield obj */
            if (mw_tmp)
                setmnotwielded(mon, mw_tmp);
            mon->weapon_check = NEED_WEAPON;
            obj->owornmask = W_MASK(os_wep);
            if (mon_visible(mon)) {
                pline(msgc_monneutral,
                      "%s wields %s%s", Monnam(mon), singular(obj, doname),
                      mon->mtame ? "." : "!");
                if (obj->cursed && obj->otyp != CORPSE) {
                    pline(msgc_monneutral,
                          "%s %s to %s %s!", Tobjnam(obj, "weld"),
                          is_plural(obj) ? "themselves" : "itself",
                          s_suffix(mon_nam(mon)), mbodypart(mon, HAND));
                    obj->bknown = 1;
                    obj->mbknown = 1;
                }
            }
        }

        mtmp = um_at(mon->dlevel, m->x, m->y);
        if (!mtmp) {
            /* can happen if a monster is confused or is trying to hit
               something invisible/displaced */
            mtmp = mvismon_at(mon, mon->dlevel, m->x, m->y);
            if (vis)
                pline(combat_msgc(mon, mtmp, cr_hit),
                      "%s flicks a whip at %s%s!", Monnam(mon),
                      !mtmp ? "thin air" : s_suffix(mon_nam(mtmp)),
                      mtmp ? " displaced image" : "");
            return 1;
        }

        const char *The_whip = vismon ? "The bullwhip" : "A whip";
        int where_to = rn2(4);
        otmp = m_mwep(mtmp);
        if (!otmp) {
            impossible("Monster targeting something without a weapon!");
            return 0;
        }
        const char *hand;
        const char *the_weapon = the(xname(otmp));

        hand = mbodypart(mtmp, HAND);
        if (bimanual(otmp))
            hand = makeplural(hand);

        if (vismon)
            pline(combat_msgc(mon, mtmp, cr_hit),
                  "%s flicks a bullwhip towards %s %s!", Monnam(mon),
                  s_suffix(mon_nam(mtmp)), hand);
        if (otmp->otyp == HEAVY_IRON_BALL) {
            pline(combat_msgc(mon, mtmp, cr_immune),
                  "%s fails to wrap around %s.", The_whip, the_weapon);
            return 1;
        }
        pline(combat_msgc(mon, mtmp, cr_hit),
              "%s wraps around %s %s wielding!", The_whip, the_weapon,
              m_verbs(mtmp, "are"));
        if ((mtmp == &youmonst && welded(otmp)) ||
            (otmp->cursed && otmp->otyp != CORPSE)) {
            pline(combat_msgc(mon, mtmp, cr_immune),
                  "%s welded to %s %s%c",
                  !is_plural(obj) ? "It is" : "They are", hand,
                  s_suffix(mon_nam(mtmp)), !obj->bknown ? '!' : '.');
            otmp->bknown = 1;
            otmp->mbknown = 1;
            where_to = 0;
        }
        if (!where_to) {
            pline(combat_msgc(mon, mtmp, cr_miss),
                  "The whip slips free.");  /* not `The_whip' */
            return 2;
        } else if (where_to == 3 && hates_silver(mon->data) &&
                   objects[otmp->otyp].oc_material == SILVER) {
            /* this monster won't want to catch a silver weapon; drop it at 
               hero's feet instead */
            where_to = 2;
        }
        if (mtmp == &youmonst) {
            uwepgone();
            freeinv(otmp);
        } else {
            MON_NOWEP(mtmp);
            otmp->owornmask = 0L;
            obj_extract_self(otmp);
        }
        switch (where_to) {
        case 1:    /* onto floor beneath mon */
            pline(mtmp == &youmonst ? msgc_itemloss :
                  combat_msgc(mon, mtmp, cr_hit),
                  "%s yanks %s from %s %s!", Monnam(mon), the_weapon,
                  s_suffix(mon_nam(mon)), hand);
            place_object(otmp, level, mon->mx, mon->my);
            break;
        case 2:    /* onto floor beneath you */
            pline(mtmp == &youmonst ? msgc_itemloss :
                  combat_msgc(mon, mtmp, cr_hit),
                  "%s yanks %s to the %s!", Monnam(mon), the_weapon,
                  surface(m_mx(mon), m_my(mon)));
            place_object(otmp, level, m_mx(mtmp), m_my(mtmp));
            break;
        case 3:    /* into mon's inventory */
            pline(mtmp == &youmonst ? msgc_itemloss :
                  combat_msgc(mon, mtmp, cr_hit),
                  "%s snatches %s!", Monnam(mon), the_weapon);
            mpickobj(mon, otmp, NULL);
            break;
        }
        return 1;
    case MUSE_NONE:
        return 0;
    default:
        impossible("%s wanted to perform action %d?", Monnam(mon),
                   m->use);
        break;
    }
    return 0;
}

int
rnd_defensive_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = monstr[monsndx(pm)];
    int trycnt = 0;

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == S_KOP)
        return 0;

    if (!rn2_on_rng(30, rng))
        return SCR_GENOCIDE;

try_again:
    switch (rn2_on_rng(8 + (difficulty > 3) + (difficulty > 6) +
                       (difficulty > 8), rng)) {
    case 6:
    case 9:
        if (mtmp->dlevel->flags.noteleport && ++trycnt < 2)
            goto try_again;
        if (!rn2_on_rng(3, rng))
            return WAN_TELEPORTATION;
        /* else FALLTHRU */
    case 0:
    case 1:
        return SCR_TELEPORTATION;
    case 8:
    case 10:
        if (!rn2_on_rng(9, rng))
            return WAN_CREATE_MONSTER;
        /* else FALLTHRU */
    case 2:
        return SCR_CREATE_MONSTER;
    case 3:
        return POT_HEALING;
    case 4:
        return POT_EXTRA_HEALING;
    case 5:
        return (mtmp->data !=
                &mons[PM_PESTILENCE]) ? POT_FULL_HEALING : POT_SICKNESS;
    case 7:
        if (levitates(mtmp) || mx_eshk(mtmp) ||
            mx_egd(mtmp) || ispriest(mtmp))
            return 0;
        else
            return WAN_DIGGING;
    }
     /*NOTREACHED*/ return 0;
}

/* Used for critical failures with wand use. Might also see
   use if monsters learn to break wands intelligently.
   FIXME: merge this with do_break_wand() */

/* what? (investigate do_break_wand for this...) */
#define BY_OBJECT       (NULL)
static void
mon_break_wand(struct monst *mtmp, struct obj *otmp)
{
    int i, x, y;
    int damage;
    int expltype;
    int otyp;
    boolean oseen = mon_visible(mtmp);

    otyp = otmp->otyp;
    otmp->ox = mtmp->mx;
    otmp->oy = mtmp->my;

    /* The following wands have no effect */
    if (otyp == WAN_WISHING ||
        otyp == WAN_NOTHING ||
        otyp == WAN_OPENING ||
        otyp == WAN_LOCKING ||
        otyp == WAN_PROBING ||
        otyp == WAN_ENLIGHTENMENT ||
        otyp == WAN_SECRET_DOOR_DETECTION ||
        otmp->spe <= 0) {
        if (oseen)
            pline(msgc_failcurse, "But nothing else happens...");
        return;
    }

    /* damage */
    damage = otmp->spe * 4;
    if (otyp != WAN_MAGIC_MISSILE)
        damage *= 2;
    if (otyp == WAN_DEATH || otyp == WAN_LIGHTNING)
        damage *= 2;

    /* explosion color */
    if (otyp == WAN_FIRE)
        expltype = EXPL_FIERY;
    else if (otyp == WAN_COLD)
        expltype = EXPL_FROSTY; 
    else
        expltype = EXPL_MAGICAL;

    /* (non-sleep) ray explosions */
    if (otyp == WAN_DEATH ||
        otyp == WAN_FIRE ||
        otyp == WAN_COLD ||
        otyp == WAN_LIGHTNING ||
        otyp == WAN_MAGIC_MISSILE) {
        explode(otmp->ox, otmp->oy, (otyp - WAN_MAGIC_MISSILE), damage, WAND_CLASS,
                expltype, NULL, 0);
        return;
    }

    if (otyp == WAN_STRIKING) {
        if (oseen)
            pline(msgc_monneutral,
                  "A wall of force smashes down around %s!",
                  mon_nam(mtmp));
        damage = dice(1 + otmp->spe, 6);
    }

    explode(otmp->ox, otmp->oy, 0, rnd(damage), WAND_CLASS, expltype, NULL, 0);

    /* affect all tiles around the monster */
    for (i = 0; i <= 8; i++) {
        bhitpos.x = x = otmp->ox + xdir[i];
        bhitpos.y = y = otmp->oy + ydir[i];
        if (!isok(x, y))
            continue;

        if (otyp == WAN_DIGGING && dig_check(BY_OBJECT, FALSE, x, y)) {
            if (IS_WALL(level->locations[x][y].typ) ||
                IS_DOOR(level->locations[x][y].typ)) {
                /* add potential shop damage for fixing */
                if (*in_rooms(level, x, y, SHOPBASE))
                    add_damage(bhitpos.x, bhitpos.y, 0L);
            }
            digactualhole(x, y, BY_OBJECT,
                          (rn2(otmp->spe) < 3 ||
                           !can_dig_down(level)) ? PIT : HOLE);
        } else if (otyp == WAN_CREATE_MONSTER)
            makemon(NULL, level, otmp->ox, otmp->oy, MM_CREATEMONSTER | MM_CMONSTER_U);
        else {
            bhit_at(mtmp, otmp, x, y, 10);
            bot(); /* blindness */
        }
    }

    if (otyp == WAN_LIGHT)
        litroom(mtmp, TRUE, otmp, TRUE);     /* only needs to be done once */
}

int
rnd_offensive_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = monstr[monsndx(pm)];

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == S_KOP)
        return 0;
    if (difficulty > 3 && !rn2_on_rng(15, rng))
        return WAN_SLOW_MONSTER;
    if (difficulty > 4 && !rn2_on_rng(20, rng))
        return SCR_STINKING_CLOUD;
    if (difficulty > 5 && !rn2_on_rng(25, rng))
        return WAN_CANCELLATION;
    if (difficulty > 6 && !rn2_on_rng(30, rng))
        return WAN_POLYMORPH;
    if (difficulty > 7 && !rn2_on_rng(35, rng))
        return WAN_DEATH;
    switch (rn2_on_rng(9 - (difficulty < 4) + 4 * (difficulty > 6), rng)) {
    case 0:
        return WAN_SLEEP;
    case 1:
        return WAN_STRIKING;
    case 2:
        return POT_ACID;
    case 3:
        return POT_CONFUSION;
    case 4:
        return POT_BLINDNESS;
    case 5:
        return POT_SLEEPING;
    case 6:
        return POT_PARALYSIS;
    case 7:
        return WAN_CREATE_MONSTER;
    case 8:
        return WAN_MAGIC_MISSILE;
    case 9:
        return WAN_SLEEP;
    case 10:
        return WAN_FIRE;
    case 11:
        return WAN_COLD;
    case 12:
        return WAN_LIGHTNING;
    }
     /*NOTREACHED*/ return 0;
}

void
you_aggravate(const struct monst *mtmp)
{
    pline(msgc_youdiscover, "For some reason, %s presence is known to you.",
          s_suffix(noit_mon_nam(mtmp)));
    cls();
    dbuf_set(mtmp->mx, mtmp->my, S_unexplored, 0, 0, 0, 0,
             dbuf_monid(mtmp, mtmp->mx, mtmp->my, rn2), 0, 0, 0);
    display_self();
    
    /* msgc_info is used for detect monster results, so makes sense here */
    pline_implied(msgc_info, "You feel aggravated at %s.", noit_mon_nam(mtmp));
    look_at_map(mtmp->mx, mtmp->my);
    doredraw();
    cancel_helplessness(hm_unconscious,
                        "Aggravated, you are jolted into full consciousness.");

    newsym(mtmp->mx, mtmp->my);
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);
}

int
rnd_misc_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == S_KOP)
        return 0;
    if (!rn2_on_rng(30, rng))
        return rn2_on_rng(3, rng) ? POT_POLYMORPH : WAN_POLYMORPH;

    if (!rn2_on_rng(40, rng) && !nonliving(pm))
        return AMULET_OF_LIFE_SAVING;

    switch (rn2_on_rng(3, rng)) {
    case 0:
        if (mx_egd(mtmp))
            return 0;
        return rn2_on_rng(6, rng) ? POT_SPEED : WAN_SPEED_MONSTER;
    case 1:
        return rn2_on_rng(6, rng) ? POT_INVISIBILITY : WAN_MAKE_INVISIBLE;
    case 2:
        return POT_GAIN_LEVEL;
    }
     /*NOTREACHED*/ return 0;
}

boolean
searches_for_item(struct monst *mon, struct obj *obj)
{
    int typ = obj->otyp;
    struct musable dummy; /* for find_item_obj */
    init_musable(mon, &dummy); /* sets musable->mon */

    /* don't loot bones piles */
    if (is_animal(mon->data) || mindless(mon->data) ||
        mon->data == &mons[PM_GHOST])
        return FALSE;

    /* don't pickup blacklisted items */
    if (!mon_allowed(typ))
        return FALSE;

    /* discharged wands/magical horns/bag of tricks */
    if ((obj->oclass == WAND_CLASS ||
         typ == FROST_HORN ||
         typ == FIRE_HORN ||
         typ == BAG_OF_TRICKS) &&
        obj->spe <= 0 &&
        obj->mknown &&
        typ != WAN_WISHING &&
        mon_castable(mon, SPE_CHARGING, TRUE) < 80 &&
        !find_item_obj(mon->minvent, &dummy, FALSE, SCR_CHARGING))
        return FALSE;

    /* discharged lamps */
    if ((typ == OIL_LAMP ||
         typ == BRASS_LANTERN) &&
        obj->age <= 0 &&
        obj->mknown)
        return FALSE;

    /* one can argue that this check is redundant since monsters can't pickup
       items while levitating in first place... but what if they could? */
    if (typ == WAN_DIGGING)
        return (boolean) (!levitates(mon) || levitates_at_will(mon, TRUE, FALSE));

    if (typ == POT_INVISIBILITY)
        return (boolean) (!m_has_property(mon, INVIS, W_MASK(os_outside), TRUE) &&
                          !attacktype(mon->data, AT_GAZE));
    if (typ == POT_SPEED)
        return (boolean) (!very_fast(mon));

    if (typ == POT_SEE_INVISIBLE)
        return !see_invisible(mon);

    if (typ == POT_BLINDNESS)
        return !attacktype(mon->data, AT_GAZE);

    switch (obj->oclass) {
    case WAND_CLASS:
        /* locking is TODO */
        if (typ == WAN_LIGHT ||
            typ == WAN_LOCKING ||
            typ == WAN_MAGIC_MISSILE ||
            typ == WAN_MAKE_INVISIBLE ||
            typ == WAN_OPENING ||
            typ == WAN_PROBING ||
            typ == WAN_SECRET_DOOR_DETECTION ||
            typ == WAN_SLOW_MONSTER ||
            typ == WAN_SPEED_MONSTER ||
            typ == WAN_STRIKING ||
            typ == WAN_UNDEAD_TURNING ||
            typ == WAN_COLD ||
            typ == WAN_FIRE ||
            typ == WAN_LIGHTNING ||
            typ == WAN_SLEEP ||
            typ == WAN_CANCELLATION ||
            typ == WAN_CREATE_MONSTER ||
            typ == WAN_POLYMORPH ||
            typ == WAN_TELEPORTATION ||
            typ == WAN_DEATH ||
            typ == WAN_WISHING)
            return TRUE;
        break;
    case POTION_CLASS:
        if (typ == POT_HEALING ||
            typ == POT_EXTRA_HEALING ||
            typ == POT_FULL_HEALING ||
            typ == POT_POLYMORPH ||
            typ == POT_GAIN_LEVEL ||
            typ == POT_MONSTER_DETECTION ||
            typ == POT_PARALYSIS ||
            typ == POT_SLEEPING ||
            typ == POT_ACID ||
            typ == POT_CONFUSION)
            return TRUE;
        break;
    case SCROLL_CLASS:
        if (typ == SCR_IDENTIFY ||
            typ == SCR_ENCHANT_WEAPON ||
            typ == SCR_ENCHANT_ARMOR ||
            typ == SCR_REMOVE_CURSE ||
            typ == SCR_DESTROY_ARMOR ||
            typ == SCR_FIRE || /* for curing sliming only */
            typ == SCR_TELEPORTATION ||
            typ == SCR_CREATE_MONSTER ||
            typ == SCR_TAMING ||
            typ == SCR_CHARGING ||
            typ == SCR_GENOCIDE ||
            typ == SCR_STINKING_CLOUD)
            return TRUE;
        break;
    case SPBOOK_CLASS:
        if (typ != SPE_BOOK_OF_THE_DEAD &&
            typ != SPE_BLANK_PAPER &&
            (study_rate(mon, obj) >= 15 || /* pickup unless we can't study it */
             !obj->mknown || (obj->mbknown && obj->blessed)) &&
            !mon_castable(mon, typ, TRUE))
            return TRUE;
        break;
    case TOOL_CLASS:
        if (typ == PICK_AXE)
            return (boolean) needspick(mon->data);
        if (typ == UNICORN_HORN)
            return (boolean) ((!obj->cursed || !obj->mbknown) && !is_unicorn(mon->data));
        if (typ == BRASS_LANTERN ||
            typ == OIL_LAMP ||
            typ == TALLOW_CANDLE ||
            typ == WAX_CANDLE)
            return (mon_castable(mon, SPE_JUMPING, TRUE) >= 80);
        if (typ == SACK ||
            typ == OILSKIN_SACK ||
            typ == BAG_OF_HOLDING ||
            typ == BAG_OF_TRICKS ||
            typ == MAGIC_LAMP ||
            typ == SKELETON_KEY ||
            typ == LOCK_PICK ||
            typ == CREDIT_CARD)
            return TRUE;
        if (typ == FROST_HORN ||
            typ == FIRE_HORN)
            return can_blow_instrument(mon->data);
        break;
    case FOOD_CLASS:
        if (typ == CORPSE)
            return (boolean) (((mon->misc_worn_check & W_MASK(os_armg)) &&
                               touch_petrifies(&mons[obj->corpsenm])) ||
                              (!resists_ston(mon) &&
                               (obj->corpsenm == PM_LIZARD ||
                                (acidic(&mons[obj->corpsenm]) &&
                                 obj->corpsenm != PM_GREEN_SLIME))));
        if (typ == EGG)
            return (boolean) (touch_petrifies(&mons[obj->corpsenm]));
        break;
    case AMULET_CLASS:
        if (typ == AMULET_OF_YENDOR) /* covetous is checked elsewhere */
            return is_mplayer(mon->data);
        if (typ == AMULET_OF_LIFE_SAVING) /* LS isn't desirable if nonliving */
            return (boolean) (!nonliving(mon->data));
        if (!objects[typ].oc_oprop)
            return FALSE; /* doesn't confer anything */
        return (!m_has_property(mon, objects[typ].oc_oprop, ANY_PROPERTY, TRUE));
    case RING_CLASS:
        /* Should match the list in m_dowear_type */
        if (typ == RIN_GAIN_CONSTITUTION ||
            typ == RIN_INCREASE_ACCURACY ||
            typ == RIN_INCREASE_DAMAGE ||
            typ == RIN_PROTECTION)
            return (obj->spe > 0 || !obj->mknown);
        return (!m_has_property(mon, objects[typ].oc_oprop, ANY_PROPERTY, TRUE));
    default:
        break;
    }

    return FALSE;
}

/* Helper for mon_reflects */
static boolean
reflect_slot(unsigned reason, enum objslot rslot, enum objslot *slot)
{
    if (!(reason & W_MASK(rslot)))
        return FALSE;
    *slot = rslot;
    return TRUE;
}

/* magr = monster whose attack or reflection is being reflected or NULL if
   the reflection wasn't caused by any monster (divine lightning zaps).

   recursive = TRUE if we're testing to see if a reflection is itself
   reflected. This is based on whether magr /intended/ to perform the attack,
   so using a mirror gives recursive = FALSE. */
boolean
mon_reflects(const struct monst *mon, const struct monst *magr,
             boolean recursive, const char *fmt, const char *str)
{
    /* Check from outermost to innermost objects */
    unsigned reflect_reason = has_property(mon, REFLECTING);
    const char *mon_s;
    mon_s = (mon == &youmonst) ? "your" : s_suffix(mon_nam(mon));
    enum objslot slot = os_invalid;
    if (reflect_reason) {
#define refl(rslot) (reflect_slot(reflect_reason, rslot, &slot))
        if (fmt && str)
            pline(combat_msgc(magr, mon, recursive ?
                              cr_miss : cr_immune),
                  fmt, str, mon_s,
                  refl(os_arms)     ? "shield"     :
                  refl(os_wep)      ? "weapon"     :
                  refl(os_swapwep)  ? "weapon"     :
                  refl(os_amul)     ? "amulet"     :
                  refl(os_role)     ? "scales"     :
                  refl(os_race)     ? "scales"     :
                  refl(os_polyform) ? "scales"     :
                  refl(os_arm)      ? "armor"      :
                  refl(os_armg)     ? "gloves"     :
                  refl(os_armf)     ? "boots"      :
                  refl(os_armh)     ? "helmet"     :
                  refl(os_armu)     ? "shirt"      :
                  refl(os_ringr)    ? "right ring" :
                  refl(os_ringl)    ? "left ring"  :
                  refl(os_outside)  ? "skin"       : /* potions of wonder */
                  "something weird"); /* os_arm after role/etc to suppress
                                         "armor" if uskin() */
        if (slot == os_wep) {
            struct obj *wep = m_mwep(mon);
            if (wep)
                learn_oprop(wep, opm_reflects);
        } else if (slot == os_swapwep) {
            if (mon != &youmonst)
                return TRUE; /* shouldn't happen */

            if (uswapwep)
                learn_oprop(uswapwep, opm_reflects);
        } else if (slot == os_arms) {
            struct obj *arms = which_armor(mon, os_arms);
            if (arms->otyp == SHIELD_OF_REFLECTION)
                makeknown(arms->otyp);
            learn_oprop(arms, opm_reflects);
        } else if (slot == os_amul) {
            struct obj *amul = which_armor(mon, os_amul);
            if (amul->otyp == AMULET_OF_REFLECTION)
                makeknown(amul->otyp);
            learn_oprop(amul, opm_reflects);
        } else if (slot != os_invalid && slot <= os_last_worn) {
            /* which_armor also work for rings */
            struct obj *arm = which_armor(mon, slot);
            if (!arm) {
                impossible("No armor for slot %d?", slot);
                return TRUE;
            }
            learn_oprop(arm, opm_reflects);
        }
#undef refl

        return TRUE;
    }
    return FALSE;
}

/*muse.c*/
