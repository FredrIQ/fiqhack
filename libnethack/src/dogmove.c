/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-09 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#include "mfndpos.h"

#define DOG_SATIATED 3000

static boolean dog_hunger(struct monst *, struct edog *);
static int dog_invent(struct monst *, struct edog *, int);
static int dog_goal(struct monst *, struct edog *, int, int, int);

static struct obj *DROPPABLES(struct monst *);
static boolean can_reach_location(struct monst *, xchar, xchar, xchar, xchar);
static boolean could_reach_item(struct monst *, xchar, xchar);
static boolean is_better_armor(const struct monst *mtmp, struct obj *otmp);
static boolean could_use_item(struct monst *mtmp, struct obj *otmp);

/*
 * See if this armor is better than what we're wearing.
 */
static boolean
is_better_armor(const struct monst *mtmp, struct obj *otmp)
{
    struct obj *obj;
    struct obj *best = (struct obj *)0;

    if (otmp->oclass != ARMOR_CLASS)
        return FALSE;

    if (mtmp->data == &mons[PM_KI_RIN] || mtmp->data == &mons[PM_COUATL])
        return FALSE;

    if (cantweararm(mtmp->data) &&
        !(is_cloak(otmp) && mtmp->data->msize == MZ_SMALL))
        return FALSE;

    if (is_shirt(otmp) && (mtmp->misc_worn_check & W_MASK(os_arm)))
        return FALSE;

    if (is_shield(otmp) && (mtmp == &youmonst) ? (uwep && bimanual(uwep))
        : (MON_WEP(mtmp) && bimanual(MON_WEP(mtmp))))
        return FALSE;

    if (is_gloves(otmp) && nohands(mtmp->data))
        return FALSE;

    if (is_boots(otmp) && (slithy(mtmp->data) || mtmp->data->mlet == S_CENTAUR))
        return FALSE;

    if (is_helmet(otmp) && !is_flimsy(otmp) && num_horns(mtmp->data) > 0)
        return FALSE;

    obj = mtmp->minvent;

    for (; obj; obj = obj->nobj) {
        if (is_cloak(otmp) && !is_cloak(obj))
            continue;
        if (is_suit(otmp) && !is_suit(obj))
            continue;
        if (is_shirt(otmp) && !is_shirt(obj))
            continue;
        if (is_boots(otmp) && !is_boots(obj))
            continue;
        if (is_shield(otmp) && !is_shield(obj))
            continue;
        if (is_helmet(otmp) && !is_helmet(obj))
            continue;
        if (is_gloves(otmp) && !is_gloves(obj))
            continue;

        if (!obj->owornmask)
            continue;

        if (best &&
            (ARM_BONUS(obj) + extra_pref(mtmp, obj) >=
             ARM_BONUS(best) + extra_pref(mtmp, best)))
            best = obj;
    }

    return ((best == (struct obj *)0) ||
            (ARM_BONUS(otmp) + extra_pref(mtmp, otmp) >
             ARM_BONUS(best) + extra_pref(mtmp, best)));
}

/*
 * See if a monst could use this item in an offensive or defensive capacity.
 */
static boolean
could_use_item(struct monst *mtmp, struct obj *otmp)
{
    /* make sure this is an intelligent monster */
    if (!mtmp || is_animal(mtmp->data) || mindless(mtmp->data) ||
        nohands(mtmp->data))
        return FALSE;

    /* Musables */
    boolean can_use = searches_for_item(mtmp, otmp);

    /* food, weapons, armor */
    if (dogfood(mtmp, otmp) > df_apport ||
        (attacktype(mtmp->data, AT_WEAP) &&
         (otmp->oclass == WEAPON_CLASS || is_weptool(otmp)) &&
         (would_prefer_hwep(mtmp, otmp) || would_prefer_rwep(mtmp, otmp))) ||
        (otmp->oclass == ARMOR_CLASS && is_better_armor(mtmp, otmp)))
        can_use = TRUE;

    if (can_use) {
        /* arbitrary - greedy monsters keep any item you can use */
        if (likes_gold(mtmp->data))
            return TRUE;

        if (otmp->oclass == ARMOR_CLASS)
            return !is_better_armor(&youmonst, otmp);
        else {
            /* For bags, any container the player has is OK. Otherwise,
               only hoard items if the player has any of the same type.
               Never hoard wish items. */
            if (otmp->otyp == WAN_WISHING || otmp->otyp == MAGIC_LAMP)
                return FALSE;

            if (otmp->otyp == SACK || otmp->otyp == BAG_OF_HOLDING ||
                otmp->otyp == OILSKIN_SACK)
                return (carrying(SACK) || carrying(OILSKIN_SACK) ||
                        carrying(BAG_OF_HOLDING));

            if (m_carrying_recursive(&youmonst, youmonst.minvent,
                                     otmp->otyp, TRUE))
                return TRUE;
        }
    }

    return FALSE;
}

void
initialize_pet_weapons(struct monst *mtmp, struct pet_weapons *p)
{
    struct obj *obj;

    p->mon = mtmp;
    p->uses_weapons = !!attacktype(mtmp->data, AT_WEAP);
    p->wep = MON_WEP(mtmp);
    p->hwep = p->uses_weapons ? select_hwep(mtmp) : NULL;
    p->proj = p->uses_weapons ? select_rwep(mtmp) : NULL;
    p->rwep = p->uses_weapons ? propellor : &zeroobj;

    p->pick = NULL;
    p->unihorn = NULL;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (is_pick(obj) ||
            (obj->otyp == DWARVISH_MATTOCK && !which_armor(mtmp, os_arms)))
            p->pick = obj;
        if (obj->otyp == UNICORN_HORN && !obj->cursed)
            p->unihorn = obj;
    }

    if (is_animal(mtmp->data) || mindless(mtmp->data))
        p->unihorn = NULL;
    if (!tunnels(mtmp->data) || !needspick(mtmp->data))
        p->pick = NULL;
}

/* Determines whether a pet wants an item that's currently in its inventory.
   (If not, the pet will want to drop it.) */
boolean
pet_wants_object(const struct pet_weapons *p, struct obj *obj)
{
    if (obj->owornmask || could_use_item(p->mon, obj))
        return TRUE;

    if (obj == p->pick || obj == p->unihorn)
        return TRUE;

    if (p->uses_weapons)
        if (obj == p->wep || obj == p->proj ||
            obj == p->hwep || obj == p->rwep ||
            would_prefer_hwep(p->mon, obj) || would_prefer_rwep(p->mon, obj) ||
            (p->rwep == &zeroobj && (is_ammo(obj) || is_launcher(obj))) ||
            (p->rwep != &zeroobj && ammo_and_launcher(obj, p->rwep)))
            return TRUE;

    return FALSE;
}

static struct obj *
DROPPABLES(struct monst *mon)
{
    struct obj *obj;
    struct pet_weapons p;
    initialize_pet_weapons(mon, &p);

    for (obj = mon->minvent; obj; obj = obj->nobj)
        if (!pet_wants_object(&p, obj))
            return obj;
    return NULL;
}

static const char nofetch[] = { BALL_CLASS, CHAIN_CLASS, ROCK_CLASS, 0 };

static xchar gx, gy;      /* type and position of dog's current goal */

static boolean cursed_object_at(int, int);

static boolean
cursed_object_at(int x, int y)
{
    struct obj *otmp;

    for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
        if (otmp->cursed)
            return TRUE;
    return FALSE;
}

static int
dog_nutrition_value(struct monst *mtmp, struct obj *obj, boolean set_meating)
{
    int nutrit;

    /*
     * It is arbitrary that the pet takes the same length of time to eat
     * as a human, but gets more nutritional value.
     */
    if (obj->oclass == FOOD_CLASS) {
        if (obj->otyp == CORPSE) {
            if (set_meating)
                mtmp->meating = 3 + (mons[obj->corpsenm].cwt >> 6);
            nutrit = mons[obj->corpsenm].cnutrit;
        } else {
            if (set_meating)
                mtmp->meating = objects[obj->otyp].oc_delay;
            nutrit = objects[obj->otyp].oc_nutrition;
        }
        switch (mtmp->data->msize) {
        case MZ_TINY:
            nutrit *= 8;
            break;
        case MZ_SMALL:
            nutrit *= 6;
            break;
        default:
        case MZ_MEDIUM:
            nutrit *= 5;
            break;
        case MZ_LARGE:
            nutrit *= 4;
            break;
        case MZ_HUGE:
            nutrit *= 3;
            break;
        case MZ_GIGANTIC:
            nutrit *= 2;
            break;
        }
        if (obj->oeaten) {
            mtmp->meating = eaten_stat(mtmp->meating, obj);
            nutrit = eaten_stat(nutrit, obj);
        }
    } else if (obj->oclass == COIN_CLASS) {
        if (set_meating)
            mtmp->meating = (int)(obj->quan / 2000) + 1;
        if (set_meating && mtmp->meating < 0)
            mtmp->meating = 1;
        nutrit = (int)(obj->quan / 20);
        if (nutrit < 0)
            nutrit = 0;
    } else {
        /* Unusual pet such as gelatinous cube eating odd stuff. meating made
           consistent with wild monsters in mon.c. nutrit made consistent with
           polymorphed player nutrit in eat.c.  (This also applies to pets
           eating gold.) */
        if (set_meating)
            mtmp->meating = obj->owt / 20 + 1;
        nutrit = 5 * objects[obj->otyp].oc_nutrition;
    }
    return nutrit;
}

int
dog_nutrition(struct monst *mtmp, struct obj *obj)
{
    return dog_nutrition_value(mtmp, obj, TRUE);
}

/* returns 2 if pet dies, otherwise 1 */
int
dog_eat(struct monst *mtmp, struct obj *obj, int x, int y, boolean devour)
{
    struct edog *edog = mx_edog(mtmp);
    boolean was_starving = FALSE;
    int nutrit;

    if (edog && edog->hungrytime < moves)
        edog->hungrytime = moves;
    if (obj->oclass != POTION_CLASS)
        nutrit = dog_nutrition(mtmp, obj);
    else {
        switch (obj->otyp) {
        case POT_WATER:
            nutrit = rnd(10);
            break;
        case POT_BOOZE:
            nutrit = 10 * (2 + bcsign(obj));
            break;
        case POT_FRUIT_JUICE:
            nutrit = (obj->odiluted ? 5 : 10) * (2 + bcsign(obj));
            break;
        default:
            nutrit = 0;
            break;
        }
    }
    if (devour) {
        if (mtmp->meating > 1)
            mtmp->meating /= 2;
        if (nutrit > 1)
            nutrit = (nutrit * 3) / 4;
    }
    if (edog) {
        edog->hungrytime += nutrit;
        set_property(mtmp, CONFUSION, -2, FALSE);
        if (edog->mhpmax_penalty) {
            /* no longer starving */
            mtmp->mhpmax += edog->mhpmax_penalty;
            edog->mhpmax_penalty = 0;
            was_starving = TRUE;
        }
        if (mtmp->mflee && mtmp->mfleetim > 1)
            mtmp->mfleetim /= 2;
        if (mtmp->mtame < 20)
            mtmp->mtame++;
        if (x != mtmp->mx || y != mtmp->my) {       /* moved & ate on same turn */
            newsym(x, y);
            newsym(mtmp->mx, mtmp->my);
        }
    }

    if (obj->oclass == POTION_CLASS)
        return 1;
    if (is_pool(level, x, y) && !Underwater) {
        /* Don't print obj */
        /* TODO: Reveal presence of sea monster (especially sharks) */
    } else if (cansee(x, y) || cansee(mtmp->mx, mtmp->my)) {
        /* hack: observe the action if either new or old location is in view */
        /* However, invisible monsters should still be "it" even though out of
           sight locations should not. */
        if (mon_visible(mtmp) && tunnels(mtmp->data) && was_starving)
            pline(msgc_petneutral, "%s digs in!", Monnam(mtmp));
        else
            pline(msgc_petneutral, "%s %s %s.",
                  mon_visible(mtmp) ? noit_Monnam(mtmp) : "It",
                  devour ? "devours" : "eats",
                  (obj->oclass == FOOD_CLASS) ?
                  singular(obj, doname) : doname(obj));
    }

    /* It's a reward if it's DOGFOOD and the player dropped/threw it. */
    /* We know the player had it if invlet is set -dlc */
    if (edog) {
        if (dogfood(mtmp, obj) == df_treat && obj->invlet)
            edog->apport +=
                (int)(200L / ((long)edog->dropdist + moves - edog->droptime));
    }

    if (mtmp->data == &mons[PM_RUST_MONSTER] && obj->oerodeproof &&
        is_metallic(obj)) {
        /* The object's rustproofing is gone now */
        obj->oerodeproof = 0;
        set_property(mtmp, STUNNED, dice(4, 4), FALSE);
        if (canseemon(mtmp) && flags.verbose) {
            pline(msgc_petwarning, "%s spits %s out in disgust!", Monnam(mtmp),
                  distant_name(obj, doname));
        }
    }
    if (obj->oclass != FOOD_CLASS) {
        eatspecial(mtmp, nutrit, obj);
        if (DEADMONSTER(mtmp))
            return 2;
        return 1;
    } else if (obj->otyp == CORPSE)
        eatcorpse(mtmp, obj); /* performs useup/etc */
    else if (obj->quan > 1L) {
        obj->quan--;
        obj->owt = weight(obj);
    } else
        delobj(obj);

    return DEADMONSTER(mtmp) ? 2 : 1;
}


/* hunger effects -- returns TRUE on starvation */
static boolean
dog_hunger(struct monst *mtmp, struct edog *edog)
{
    /* Pets who don't eat don't go through this logic at all */
    if (!carnivorous(mtmp->data) && !herbivorous(mtmp->data)) {
        edog->hungrytime = moves + 500;
        return FALSE;
    }

    if (moves > edog->hungrytime) {
        /* We're hungry; check if we're carrying anything we can eat
           Intelligent pets should be able to carry such food */
        struct obj *otmp, *obest = NULL;
        int cur_nutrit = -1, best_nutrit = -1;
        enum dogfood cur_food = df_apport, best_food = df_apport;

        for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
            cur_nutrit = dog_nutrition_value(mtmp, otmp, FALSE);
            cur_food = dogfood(mtmp, otmp);
            if (cur_food > best_food && cur_nutrit > best_nutrit) {
                best_nutrit = cur_nutrit;
                best_food = cur_food;
                obest = otmp;
            }
        }
        if (obest) {
            obj_extract_self(obest);
            place_object(obest, level, mtmp->mx, mtmp->my);
            if (dog_eat(mtmp, obest, mtmp->mx, mtmp->my, FALSE) == 2)
                return TRUE;
            return FALSE;
        }
    }
    if (moves > edog->hungrytime + 500) {
        if (!edog->mhpmax_penalty) {
            /* starving pets are limited in healing */
            int newmhpmax = mtmp->mhpmax / 3;

            set_property(mtmp, CONFUSION, 255, TRUE);
            edog->mhpmax_penalty = mtmp->mhpmax - newmhpmax;
            mtmp->mhpmax = newmhpmax;
            if (mtmp->mhp > mtmp->mhpmax)
                mtmp->mhp = mtmp->mhpmax;
            if (mtmp->mhp <= 0)
                goto dog_died;
            if (cansee(mtmp->mx, mtmp->my))
                pline(msgc_petfatal, "%s is confused from hunger.",
                      Monnam(mtmp));
            else if (couldsee(mtmp->mx, mtmp->my))
                beg(mtmp);
            else
                pline(msgc_petfatal, "You feel worried about %s.",
                      y_monnam(mtmp));
            action_interrupted();
        } else if (moves > edog->hungrytime + 750 || mtmp->mhp <= 0) {
        dog_died:
            if (mtmp->mleashed && mtmp != u.usteed)
                pline(msgc_petfatal, "Your leash goes slack.");
            else if (cansee(mtmp->mx, mtmp->my))
                pline(msgc_petfatal, "%s starves.", Monnam(mtmp));
            else
                pline(msgc_petfatal, "You feel %s for a moment.",
                      Hallucination ? "bummed" : "sad");
            mondied(mtmp);
            return TRUE;
        }
    }
    return FALSE;
}

/* do something with object (drop, pick up, eat) at current position
 * returns 1 if object eaten (since that counts as dog's move), 2 if died
 */
static int
dog_invent(struct monst *mtmp, struct edog *edog, int udist)
{
    int omx, omy;
    struct obj *obj;

    boolean droppables = FALSE;

    if (mtmp->msleeping || !mtmp->mcanmove)
        return 0;

    omx = mtmp->mx;
    omy = mtmp->my;

    /* if we are carrying sth then we drop it (perhaps near @) */
    /* Note: if apport == 1 then our behaviour is independent of udist */
    /* Use udist+1 so steed won't cause divide by zero */
    if (DROPPABLES(mtmp)) {
        if (!rn2(udist + 1) || !rn2(edog->apport))
            if (rn2(10) < edog->apport) {
                relobj(mtmp, (int)invisible(mtmp), TRUE);
                if (edog->apport > 1)
                    edog->apport--;
                edog->dropdist = udist; /* hpscdi!jon */
                edog->droptime = moves;
            }
        droppables = TRUE;
    }
    if ((obj = level->objects[omx][omy]) && !strchr(nofetch, obj->oclass)) {
        int edible = dogfood(mtmp, obj);

        if (!droppables && (edible >= df_good ||
                            /* starving pet is more aggressive about eating */
                            (edog->mhpmax_penalty && edible == df_acceptable)) &&
            could_reach_item(mtmp, obj->ox, obj->oy)) {
            if (edog->hungrytime < moves + DOG_SATIATED) {
                if (levitates(mtmp) && levitates_at_will(mtmp, TRUE, FALSE))
                    return mon_remove_levitation(mtmp, FALSE);
                else if (!levitates(mtmp))
                    return dog_eat(mtmp, obj, omx, omy, FALSE);
            }
        }

        if (can_carry(mtmp, obj) && !obj->cursed &&
            could_reach_item(mtmp, obj->ox, obj->oy)) {
            boolean can_use = could_use_item(mtmp, obj);

            if (can_use || (!droppables && rn2(20) < edog->apport + 3)) {
                if (can_use || rn2(udist) || !rn2(edog->apport)) {
                    if (cansee(omx, omy) && flags.verbose)
                        pline(msgc_petneutral, "%s picks up %s.",
                              Monnam(mtmp), distant_name(obj, doname));
                    obj_extract_self(obj);
                    newsym(omx, omy);
                    mpickobj(mtmp, obj, NULL);
                    if (attacktype(mtmp->data, AT_WEAP) &&
                        mtmp->weapon_check == NEED_WEAPON) {
                        mtmp->weapon_check = NEED_HTH_WEAPON;
                        mon_wield_item(mtmp);
                    }
                    m_dowear(mtmp, FALSE);
                }
            }
        }
    }
    return 0;
}

/* set dog's goal -- gtyp, gx, gy
 * returns -1/0/1 (dog's desire to approach player) or -2 (abort move)
 */
static int
dog_goal(struct monst *mtmp, struct edog *edog, int after, int udist,
         int whappr)
{
    int omx, omy;
    boolean in_masters_sight, dog_has_minvent;
    struct obj *obj;
    xchar otyp;
    int appr;
    enum dogfood gtyp;

    /* Steeds don't move on their own will */
    if (mtmp == u.usteed)
        return -2;

    omx = mtmp->mx;
    omy = mtmp->my;

    in_masters_sight = couldsee(omx, omy);
    dog_has_minvent = (DROPPABLES(mtmp) != 0);

    if (!edog || mtmp->mleashed) {      /* he's not going anywhere... */
        gtyp = df_apport;
        gx = u.ux;
        gy = u.uy;
    } else {
#define DDIST(x,y) (dist2(x,y,omx,omy))
#define SQSRCHRADIUS 5
        int min_x, max_x, min_y, max_y;
        int nx, ny;
        boolean can_use = FALSE;

        gtyp = df_nofood;   /* no goal as yet */
        gx = gy = 0;        /* suppress 'used before set' message */

        if ((min_x = omx - SQSRCHRADIUS) < 0)
            min_x = 0;
        if ((max_x = omx + SQSRCHRADIUS) >= COLNO)
            max_x = COLNO - 1;
        if ((min_y = omy - SQSRCHRADIUS) < 0)
            min_y = 0;
        if ((max_y = omy + SQSRCHRADIUS) >= ROWNO)
            max_y = ROWNO - 1;

        /* nearby food is the first choice, then other objects */
        for (obj = level->objlist; obj; obj = obj->nobj) {
            nx = obj->ox;
            ny = obj->oy;
            if (nx >= min_x && nx <= max_x && ny >= min_y && ny <= max_y) {
                otyp = dogfood(mtmp, obj);
                /* skip inferior goals */
                if (otyp < gtyp || otyp == df_nofood)
                    continue;
                /* avoid cursed items unless starving */
                if (cursed_object_at(nx, ny) &&
                    !(edog->mhpmax_penalty && otyp > df_manfood))
                    continue;
                /* skip completely unreacheable goals */
                if (!could_reach_item(mtmp, nx, ny) ||
                    !can_reach_location(mtmp, mtmp->mx, mtmp->my, nx, ny))
                    continue;
                if (otyp > df_manfood) {
                    if (otyp > gtyp || DDIST(nx, ny) < DDIST(gx, gy)) {
                        gx = nx;
                        gy = ny;
                        gtyp = otyp;
                    }
                } else if (gtyp == df_nofood && in_masters_sight &&
                           ((can_use = could_use_item(mtmp, obj)) &&
                            !dog_has_minvent) &&
                           (!level->locations[omx][omy].lit ||
                            level->locations[u.ux][u.uy].lit) &&
                           (otyp == df_manfood || m_cansee(mtmp, nx, ny)) &&
                           (can_use || edog->apport > rn2(8)) &&
                           can_carry(mtmp, obj)) {
                    gx = nx;
                    gy = ny;
                    gtyp = df_apport;
                }
            }
        }
    }

    /* follow player if appropriate */
    if (gtyp == df_nofood ||
        (gtyp != df_treat && gtyp != df_apport && moves < edog->hungrytime)) {
        gx = u.ux;
        gy = u.uy;
        if (after && udist <= 4 && gx == u.ux && gy == u.uy)
            return -2;
        appr = (udist >= 9) ? 1 : (mtmp->mflee) ? -1 : 0;
        if (udist > 1) {
            if (!IS_ROOM(level->locations[u.ux][u.uy].typ) || !rn2(4) || whappr
                || (dog_has_minvent && rn2(edog->apport)))
                appr = 1;
        }
        /* if you have dog food it'll follow you more closely */
        if (appr == 0) {
            obj = youmonst.minvent;
            while (obj) {
                if (dogfood(mtmp, obj) == df_treat) {
                    appr = 1;
                    break;
                }
                obj = obj->nobj;
            }
        }
    } else
        appr = 1;       /* gtyp != UNDEF */
    if (confused(mtmp))
        appr = 0;

    /* If aiming for the master, locate them using strategy if possible.
       Otherwise, the dog is lost, and will random-walk. (This shouldn't
       happen because set_apparxy gives pets perfect knowledge of where
       their master is.) */
    if (gx == u.ux && gy == u.uy && !in_masters_sight) {
        if (st_target(mtmp)) {
            gx = mtmp->sx;
            gy = mtmp->sy;
        } else {
            gx = mtmp->mx;
            gy = mtmp->my;
        }
    }
    return appr;
}

/* return 0 (no move), 1 (move) or 2 (dead) */
int
dog_move(struct monst *mtmp, int after)
{       /* this is extra fast monster movement */
    int omx, omy;       /* original mtmp position */
    int appr, whappr, udist;
    int i, j;
    struct edog *edog = mx_edog(mtmp);
    struct obj *obj = NULL;
    xchar otyp;
    boolean has_edog, cursemsg[9], do_eat = FALSE;
    xchar nix, niy;     /* position mtmp is (considering) moving to */
    int nx, ny; /* temporary coordinates */
    xchar cnt, uncursedcnt, chcnt;
    int chi = -1, nidist, ndist;
    coord poss[9];
    long info[9], allowflags;
    struct musable m;
    struct distmap_state ds;
    boolean mercy = FALSE;
    if (mtmp->mw && (obj_properties(mtmp->mw) & opm_mercy) &&
        mtmp->mw->mknown)
        mercy = TRUE;

    /*
     * Tame Angels have isminion set and an ispriest structure instead of
     * an edog structure.  Fortunately, guardian Angels need not worry
     * about mundane things like eating and fetching objects, and can
     * spend all their energy defending the player.  (They are the only
     * monsters with other structures that can be tame.)
     */
    has_edog = !isminion(mtmp);

    omx = mtmp->mx;
    omy = mtmp->my;
    if (has_edog && dog_hunger(mtmp, edog))
        return 2;       /* starved */

    udist = distu(omx, omy);
    /* Let steeds eat and maybe throw rider during Conflict */
    if (mtmp == u.usteed) {
        if (Conflict && !resist(&youmonst, mtmp, RING_CLASS, 0, 0)) {
            dismount_steed(DISMOUNT_THROWN);
            return 1;
        }
        udist = 1;
    } else if (!udist)
        /* maybe we tamed him while being swallowed --jgm */
        return 0;

    nix = omx;  /* set before newdogpos */
    niy = omy;
    cursemsg[0] = FALSE;        /* lint suppression */
    info[0] = 0;        /* ditto */

    if (has_edog) {
        j = dog_invent(mtmp, edog, udist);
        if (j == 2)
            return 2;   /* died */
        else if (j == 1)
            goto newdogpos;     /* eating something */

        whappr = (moves - edog->whistletime < 5);
    } else
        whappr = 0;

    appr = dog_goal(mtmp, has_edog ? edog : NULL, after, udist, whappr);
    if (appr == -2)
        return 0;

    allowflags = (ALLOW_TRAPS | ALLOW_SSM | ALLOW_SANCT |
                  ALLOW_PEACEFUL);
    if (phasing(mtmp))
        allowflags |= (ALLOW_ROCK | ALLOW_WALL);
    if (passes_bars(mtmp))
        allowflags |= ALLOW_BARS;
    if (throws_rocks(mtmp->data))
        allowflags |= ALLOW_ROCK;
    if (Conflict && !resist(&youmonst, mtmp, RING_CLASS, 0, 0)) {
        allowflags |= ALLOW_MUXY | ALLOW_M;
        if (!has_edog) {
            coord mm;

            /* Guardian angel refuses to be conflicted; rather, it disappears,
               angrily, and sends in some nasties */
            if (canspotmon(mtmp)) {
                pline(msgc_npcvoice, "%s rebukes you, saying:", Monnam(mtmp));
                verbalize(msgc_petfatal,
                          "Since you desire conflict, have some more!");
            }
            mongone(mtmp);
            i = rnd(4);
            while (i--) {
                mm.x = u.ux;
                mm.y = u.uy;
                if (enexto(&mm, level, mm.x, mm.y, &mons[PM_ANGEL]))
                    mk_roamer(&mons[PM_ANGEL], u.ualign.type, level, mm.x, mm.y,
                              FALSE, NO_MM_FLAGS);
            }
            return 2;

        }
    }
    if (!Conflict && !confused(mtmp) && mtmp == u.ustuck &&
        !sticks(youmonst.data)) {
        unstuck(mtmp);  /* swallowed case handled above */
        pline(msgc_petneutral, "You get released!");
    }

/*
 * We haven't moved yet, so search for monsters to attack from a
 * distance and attack them if it's plausible.
 */
    if (find_item(mtmp, &m)) {
        int ret = use_item(&m);

        if (ret == 1)
            return 2;   /* died */
        if (ret == 2)
            return 1;   /* did something */
    } else if ((attacktype(mtmp->data, AT_BREA) ||
                attacktype(mtmp->data, AT_GAZE) ||
                attacktype(mtmp->data, AT_SPIT) ||
                (attacktype(mtmp->data, AT_WEAP) && select_rwep(mtmp))) &&
               mtmp->mlstmv != moves) {
        struct monst *mon = mfind_target(mtmp, FALSE);

        if (mon) {
            if (mattackm(mtmp, mon) & MM_AGR_DIED)
                return 2;       /* died */

            return 1;   /* attacked */
        }
    }

    if (!nohands(mtmp->data) && !verysmall(mtmp->data)) {
        allowflags |= OPENDOOR;
        if (m_carrying(mtmp, SKELETON_KEY))
            allowflags |= BUSTDOOR;
    }
    if (is_giant(mtmp->data))
        allowflags |= BUSTDOOR;
    if (!Is_rogue_level(&u.uz) && tunnels(mtmp->data))
        allowflags |= ALLOW_DIG;
    cnt = mfndpos(mtmp, poss, info, allowflags, 1);

    /* Normally dogs don't step on cursed items, but if they have no other
       choice they will.  This requires checking ahead of time to see how many
       uncursed item squares are around. */
    uncursedcnt = 0;
    for (i = 0; i < cnt; i++) {
        nx = poss[i].x;
        ny = poss[i].y;
        if (MON_AT(level, nx, ny) && !(info[i] & ALLOW_M))
            continue;
        if (cursed_object_at(nx, ny))
            continue;
        uncursedcnt++;
    }

    distmap_init(&ds, gx, gy, mtmp);

#define GDIST(x,y) (distmap(&ds,(x),(y)))

    chcnt = 0;
    chi = -1;
    nidist = GDIST(nix, niy);

    struct monst *mtmp2 = NULL;
    for (i = 0; i < cnt; i++) {
        nx = poss[i].x;
        ny = poss[i].y;
        cursemsg[i] = FALSE;

        /* if leashed, we drag him along. */
        if (mtmp->mleashed && distu(nx, ny) > 4)
            continue;

        /* if a guardian, try to stay close by choice */
        if (!has_edog && (j = distu(nx, ny)) > 16 && j >= udist)
            continue;

        mtmp2 = m_at(level, nx, ny);
        if (mtmp2 &&
            ((mtmp2->mtame && mercy) || (info[i] & ALLOW_M))) {
            int mstatus;

            /* anti-stupidity checks moved to mm_aggression in mon.c */

            if (after)
                return 0;       /* hit only once each move */

            notonhead = 0;
            mstatus = mattackm(mtmp, mtmp2);

            /* aggressor (pet) died */
            if (mstatus & MM_AGR_DIED)
                return 2;

            if ((mstatus & MM_HIT) && !(mstatus & MM_DEF_DIED) && rn2(4) &&
                mtmp2->mlstmv != moves && !onscary(mtmp->mx, mtmp->my, mtmp2) &&
                /* monnear check needed: long worms hit on tail */
                monnear(mtmp2, mtmp->mx, mtmp->my)) {
                mstatus = mattackm(mtmp2, mtmp);        /* return attack */
                if (mstatus & MM_DEF_DIED)
                    return 2;
            }

            return 0;
        }

        {
            /* Dog avoids harmful traps, but perhaps it has to pass one in order
               to follow player.  (Non-harmful traps do not have ALLOW_TRAPS in
               info[].) The dog only avoids the trap if you've seen it, unlike
               enemies who avoid traps if they've seen some trap of that type
               sometime in the past.  (Neither behavior is really realistic.) */
            struct trap *trap;

            if ((info[i] & ALLOW_TRAPS) && (trap = t_at(level, nx, ny))) {
                if (mtmp->mleashed) {
                    whimper(mtmp);
                } else
                    /* 1/40 chance of stepping on it anyway, in case it has to
                       pass one to follow the player... */
                if (trap->tseen && rn2(40))
                    continue;
            }
        }

        /* dog eschews cursed objects, but likes dog food */
        /* (minion isn't interested; `cursemsg' stays FALSE) */
        if (has_edog)
            for (obj = level->objects[nx][ny]; obj; obj = obj->nexthere) {
                /* levitating monsters will not touch the cursed object */
                if (obj->cursed && !levitates(mtmp))
                    cursemsg[i] = TRUE;
                else if ((otyp = dogfood(mtmp, obj)) > df_manfood &&
                         (otyp > df_acceptable || edog->hungrytime <= moves) &&
                         edog->hungrytime < moves + DOG_SATIATED &&
                         (!levitates(mtmp) ||
                         levitates_at_will(mtmp, TRUE, FALSE))) {
                    /* Note: our dog likes the food so much that he might eat
                       it even when it conceals a cursed object */
                    nix = nx;
                    niy = ny;
                    chi = i;
                    do_eat = TRUE;
                    cursemsg[i] = FALSE;        /* not reluctant */
                    goto newdogpos;
                }
            }
        /* didn't find something to eat; if we saw a cursed item and aren't
           being forced to walk on it, usually keep looking */
        if (cursemsg[i] && !mtmp->mleashed && uncursedcnt > 0 &&
            rn2(13 * uncursedcnt))
            continue;

        j = ((ndist = GDIST(nx, ny)) - nidist) * appr;
        if ((j == 0 && !rn2(++chcnt)) || j < 0 ||
            (j > 0 && !whappr && ((omx == nix && omy == niy && !rn2(3))
                                  || !rn2(12))
            )) {
            nix = nx;
            niy = ny;
            nidist = ndist;
            if (j < 0)
                chcnt = 0;
            chi = i;
        }
    }
newdogpos:
    if (nix != omx || niy != omy) {
        struct obj *mw_tmp;

        if (info[chi] & ALLOW_MUXY) {
            if (mtmp->mleashed) {       /* play it safe */
                pline(msgc_petwarning, "%s breaks loose of %s leash!",
                      Monnam(mtmp), mhis(mtmp));
                m_unleash(mtmp, FALSE);
            }
            mattackq(mtmp, nix, niy);
            return 0;
        }
        if (!m_in_out_region(mtmp, nix, niy))
            return 1;
        if (((IS_ROCK(level->locations[nix][niy].typ) &&
              may_dig(level, nix, niy)) || closed_door(level, nix, niy)) &&
            mtmp->weapon_check != NO_WEAPON_WANTED && tunnels(mtmp->data) &&
            needspick(mtmp->data)) {
            if (closed_door(level, nix, niy)) {
                if (!(mw_tmp = MON_WEP(mtmp)) || !is_pick(mw_tmp) ||
                    !is_axe(mw_tmp))
                    mtmp->weapon_check = NEED_PICK_OR_AXE;
            } else if (IS_TREE(level->locations[nix][niy].typ)) {
                if (!(mw_tmp = MON_WEP(mtmp)) || !is_axe(mw_tmp))
                    mtmp->weapon_check = NEED_AXE;
            } else if (!(mw_tmp = MON_WEP(mtmp)) || !is_pick(mw_tmp)) {
                mtmp->weapon_check = NEED_PICK_AXE;
            }
            if (mtmp->weapon_check >= NEED_PICK_AXE && mon_wield_item(mtmp))
                return 0;
        }
        /* insert a worm_move() if worms ever begin to eat things */
        struct monst *dmon = m_at(level, nix, niy);
        if (info[chi] & ALLOW_PEACEFUL && dmon) {
            if (cansee(mtmp->mx, mtmp->my) ||
                cansee(dmon->mx, dmon->my))
                pline_once(msgc_monneutral, "%s %s.",
                           M_verbs(mtmp, "displace"),
                           mon_nam(dmon));
            remove_monster(level, omx, omy);
            remove_monster(level, nix, niy);
            place_monster(mtmp, nix, niy, TRUE);
            place_monster(dmon, omx, omy, TRUE);
        } else {
            remove_monster(level, omx, omy);
            place_monster(mtmp, nix, niy, TRUE);
        }
        if (cursemsg[chi] && (cansee(omx, omy) || cansee(nix, niy)))
            pline(msgc_petneutral, "%s moves only reluctantly.", Monnam(mtmp));
        /* We have to know if the pet's gonna do a combined eat and move before
           moving it, but it can't eat until after being moved.  Thus the
           do_eat flag. */
        if (do_eat) {
            if (levitates(mtmp)) {
                if (levitates_at_will(mtmp, TRUE, FALSE)) {
                    int cancel_levi = mon_remove_levitation(mtmp, FALSE);
                    if (cancel_levi)
                        return cancel_levi;
                    else
                        impossible("remove levitation performed no action?");
                }
            } else if (dog_eat(mtmp, obj, omx, omy, FALSE) == 2)
                return 2;
        }
    } else if (mtmp->mleashed && distu(omx, omy) > 4) {
        /* an incredible kludge, but the only way to keep pooch near after it
           spends time eating or in a trap, etc. */
        coord cc;

        nx = sgn(omx - u.ux);
        ny = sgn(omy - u.uy);
        cc.x = u.ux + nx;
        cc.y = u.uy + ny;
        if (goodpos(level, cc.x, cc.y, mtmp, 0))
            goto dognext;

        i = xytod(nx, ny);
        for (j = (i + 7) % 8; j < (i + 1) % 8; j++) {
            dtoxy(&cc, j);
            if (goodpos(level, cc.x, cc.y, mtmp, 0))
                goto dognext;
        }
        for (j = (i + 6) % 8; j < (i + 2) % 8; j++) {
            dtoxy(&cc, j);
            if (goodpos(level, cc.x, cc.y, mtmp, 0))
                goto dognext;
        }
        cc.x = mtmp->mx;
        cc.y = mtmp->my;
    dognext:
        if (!m_in_out_region(mtmp, nix, niy))
            return 1;
        remove_monster(level, mtmp->mx, mtmp->my);
        place_monster(mtmp, cc.x, cc.y, TRUE);
        newsym(cc.x, cc.y);
        set_apparxy(mtmp);
    }
    return 1;
}

/* check if a monster could pick up objects from a location */
static boolean
could_reach_item(struct monst *mon, xchar nx, xchar ny)
{
    if ((!levitates(mon) || levitates_at_will(mon, TRUE, FALSE)) &&
        (!is_pool(level, nx, ny) || swims(mon) || unbreathing(mon)) &&
        (!is_lava(level, nx, ny) || likes_lava(mon->data)) &&
        (!sobj_at(BOULDER, level, nx, ny) || throws_rocks(mon->data)))
        return TRUE;
    return FALSE;
}

/* Hack to prevent a dog from being endlessly stuck near an object that
 * it can't reach, such as caught in a teleport scroll niche.  It recursively
 * checks to see if the squares in between are good.  The checking could be a
 * little smarter; a full check would probably be useful in m_move() too.
 * Since the maximum food distance is 5, this should never be more than 5 calls
 * deep.
 */
static boolean
can_reach_location(struct monst *mon, xchar mx, xchar my, xchar fx, xchar fy)
{
    int i, j;
    int dist;

    if (mx == fx && my == fy)
        return TRUE;
    if (!isok(mx, my))
        return FALSE;   /* should not happen */

    dist = dist2(mx, my, fx, fy);
    for (i = mx - 1; i <= mx + 1; i++) {
        for (j = my - 1; j <= my + 1; j++) {
            if (!isok(i, j))
                continue;
            if (dist2(i, j, fx, fy) >= dist)
                continue;
            if (IS_ROCK(level->locations[i][j].typ) && !phasing(mon)
                && (!may_dig(level, i, j) || !tunnels(mon->data)))
                continue;
            if (IS_DOOR(level->locations[i][j].typ) &&
                (level->locations[i][j].doormask & (D_CLOSED | D_LOCKED)))
                continue;
            if (!could_reach_item(mon, i, j))
                continue;
            if (can_reach_location(mon, i, j, fx, fy))
                return TRUE;
        }
    }
    return FALSE;
}

/*dogmove.c*/
