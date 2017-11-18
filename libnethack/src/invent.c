/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#define NOINVSYM        '#'
#define CONTAINED_SYM   '>'     /* designator for inside a container */

static boolean mergable(struct obj *, struct obj *);
static void invdisp_nothing(const char *, const char *);
static boolean worn_wield_only(const struct obj *);
static boolean only_here(const struct obj *);
static const char *compactify(const char *);
static char display_pickinv(const char *, boolean, long *);
static boolean this_type_only(const struct obj *);
static void dounpaid(void);
static struct obj *find_unpaid(struct obj *, struct obj **);
static void menu_identify(int);
static boolean tool_in_use(struct obj *);
static char obj_to_let(struct obj *);
static int identify(struct obj *);
static const char *dfeature_at(int, int);

static void addinv_stats(struct obj *);
static void freeinv_stats(struct obj *);


/* Keep this in order; larger value are more impossible to use. */
enum obj_use_status {
    OBJECT_USABLE,             /* using this object is reasonable */
    NONSENSIBLE_USE,           /* using this object is not reasonable */
    CURRENTLY_NOT_USABLE,      /* using this object is impossible for now */
    IMPOSSIBLE_USE,            /* using this object is impossible forever */
};


void
assigninvlet(struct obj *otmp)
{
    boolean inuse[52];
    /*
     * Preferentially assign letters as follows:
     * Food should not be assigned to e, y, Y, n, or N.
     * (Eating from ground used to be a y/n prompt.)
     * Potions should not be assigned to q, y, Y, n, or N.
     * (Quaffing from fountains used to be a y/n prompt.)
     * Scrolls and spellbooks should not be assigned to r.
     * Wands should not be assigned to z.
     * Armor, gems, rocks, and balls/chains should try to be assigned to one of
     * these letters.
     * (Players only infrequently care about their inventory letters.)
     * Update danger_letters if the list of letters we care about changes.
     */
    boolean deprecated[52];
    boolean should_displace = FALSE;
    const char *danger_letters = "enqryzNY";
    int i, to_use;
    struct obj *obj;

    /* There is only one of these in inventory... */
    if (otmp->oclass == COIN_CLASS) {
        otmp->invlet = GOLD_SYM;
        return;
    }

    for (i = 0; i < 52; i++) {
        inuse[i] = FALSE;
        deprecated[i] = FALSE;
    }

    if (otmp->oclass == FOOD_CLASS) {
        deprecated['e' - 'a'] = TRUE;
        deprecated['n' - 'a'] = TRUE;
        deprecated['y' - 'a'] = TRUE;
        deprecated['N' - 'A' + 26] = TRUE;
        deprecated['Y' - 'A' + 26] = TRUE;
    }
    if (otmp->oclass == POTION_CLASS) {
        deprecated['q' - 'a'] = TRUE;
        deprecated['n' - 'a'] = TRUE;
        deprecated['y' - 'a'] = TRUE;
        deprecated['N' - 'A' + 26] = TRUE;
        deprecated['Y' - 'A' + 26] = TRUE;
    }
    if (otmp->oclass == SCROLL_CLASS || otmp->oclass == SPBOOK_CLASS)
        deprecated['r' - 'a'] = TRUE;
    if (otmp->oclass == WAND_CLASS)
        deprecated['z' - 'a'] = TRUE;
    if (otmp->oclass == ARMOR_CLASS || otmp->oclass == GEM_CLASS ||
        otmp->oclass == ROCK_CLASS || otmp->oclass == BALL_CLASS ||
        otmp->oclass == CHAIN_CLASS)
        should_displace = TRUE;

    for (obj = invent; obj; obj = obj->nobj)
        if (obj != otmp) {
            i = obj->invlet;
            if ('a' <= i && i <= 'z')
                inuse[i - 'a'] = TRUE;
            else if ('A' <= i && i <= 'Z')
                inuse[i - 'A' + 26] = TRUE;
            if (i == otmp->invlet)
                otmp->invlet = 0;
        }
    /* First, if the item's been in inventory before, assign its old letter
       if possible. */
    if (((i = otmp->invlet)) &&
        (('a' <= i && i <= 'z') || ('A' <= i && i <= 'Z')))
        return;
    /* If the item should take up the dangerous letters, see if one of them
       is free.
       Don't change u.lastinvnr in this case; that'd cause unnecessary trampling
       of other invlets. */
    if (should_displace) {
        const char *cur;
        for (cur = danger_letters; *cur; cur++) {
            if (('a' <= *cur && *cur <= 'z' && !inuse[*cur - 'a']) ||
                ('A' <= *cur && *cur <= 'Z' && !inuse[*cur - 'A' + 26])) {
                otmp->invlet = *cur;
                return;
            }
        }
    }

    /* A bug in 4.3-beta1 means that u.lastinvnr can end up in the stratosphere
       sometimes. That bug is now fixed, but we nonetheless need to be able to
       load old saves correctly, so set it to the value it should have had. */
    if (u.lastinvnr >= 52)
        u.lastinvnr = 0;

    for (to_use = i = u.lastinvnr + 1; i != u.lastinvnr; i++) {
        if (i >= 52) {
            i = -1;
            continue;
        }
        if (!inuse[i]) {
            to_use = i;
            /* If this letter is deprecated, keep looking. */
            if (!deprecated[i])
                break;
        }
    }

    /* The above loop misses one case */
    if (inuse[to_use] && !inuse[u.lastinvnr])
        to_use = u.lastinvnr;

    if (to_use >= 52)
        to_use = 0;

    otmp->invlet =
        (inuse[to_use] ? NOINVSYM : (to_use < 26) ? ('a' + to_use)
                                                  : ('A' + to_use - 26));
    u.lastinvnr = to_use;
}


/* in ASCII, toggling a bit puts lowercase in front of uppercase */
static_assert(('a' ^ 040) == 'A', "ASCII case handling is required");
#define inv_rank(o) ((o)->invlet ^ 040)

/* sort the inventory; used by addinv() and doorganize() */
extern void
reorder_invent(void)
{
    struct obj *otmp, *prev, *next;
    boolean need_more_sorting;

    do {
        /*
         * We expect at most one item to be out of order, so this
         * isn't nearly as inefficient as it may first appear.
         */
        need_more_sorting = FALSE;
        for (otmp = invent, prev = 0; otmp;) {
            next = otmp->nobj;
            if (next && inv_rank(next) < inv_rank(otmp)) {
                need_more_sorting = TRUE;
                if (prev)
                    prev->nobj = next;
                else
                    invent = next;
                otmp->nobj = next->nobj;
                next->nobj = otmp;
                prev = next;
            } else {
                prev = otmp;
                otmp = next;
            }
        }
    } while (need_more_sorting);
}

#undef inv_rank

/* scan a list of objects to see whether another object will merge with
   one of them; used in pickup.c when all 52 inventory slots are in use,
   to figure out whether another object could still be picked up */
struct obj *
merge_choice(struct obj *objlist, struct obj *obj)
{
    struct monst *shkp;
    int save_nocharge;

    if (obj->otyp == SCR_SCARE_MONSTER) /* punt on these */
        return NULL;
    /* if this is an item on the shop floor, the attributes it will have when
       carried are different from what they are now; prevent that from
       eliciting an incorrect result from mergable() */
    save_nocharge = obj->no_charge;
    if (objlist == invent && obj->where == OBJ_FLOOR &&
        (shkp =
         shop_keeper(level, inside_shop(obj->olev, obj->ox, obj->oy))) != 0) {
        if (obj->no_charge)
            obj->no_charge = 0;
        /* A billable object won't have its `unpaid' bit set, so would
           erroneously seem to be a candidate to merge with a similar ordinary
           object.  That's no good, because once it's really picked up, it
           won't merge after all.  It might merge with another unpaid object,
           but we can't check that here (depends too much upon shk's bill) and
           if it doesn't merge it would end up in the '#' overflow inventory
           slot, so reject it now. */
        else if (inhishop(shkp))
            return NULL;
    }
    while (objlist) {
        if (mergable(objlist, obj))
            break;
        objlist = objlist->nobj;
    }
    obj->no_charge = save_nocharge;
    return objlist;
}

/* merge obj with otmp and delete obj if types agree */
int
merged(struct obj **potmp, struct obj **pobj)
{
    struct obj *otmp = *potmp, *obj = *pobj;

    if (mergable(otmp, obj)) {
        /* Approximate age: we do it this way because if we were to do it
           "accurately" (merge only when ages are identical) we'd wind up never
           merging any corpses. otmp->age = otmp->age*(1-proportion) +
           obj->age*proportion; Don't do the age manipulation if lit.  We would
           need to stop the burn on both items, then merge the age, then
           restart the burn. */
        if (!obj->lamplit)
            otmp->age = ((otmp->age * otmp->quan) + (obj->age * obj->quan))
                / (otmp->quan + obj->quan);

        if (obj->bknown)
            otmp->bknown = 1;

        otmp->quan += obj->quan;
        if (otmp->oclass == COIN_CLASS)
            otmp->owt = weight(otmp);
        else
            otmp->owt += obj->owt;
        if (!otmp->onamelth && obj->onamelth)
            otmp = *potmp = oname(otmp, ONAME(obj));
        obj_extract_self(obj);

        /* really should merge the timeouts */
        if (obj->lamplit)
            obj_merge_light_sources(obj, otmp);
        if (obj->timed)
            obj_stop_timers(obj);       /* follows lights */

        /* fixup for `#adjust' merging wielded darts, daggers, &c */
        if (obj->owornmask && carried(otmp)) {
            long wmask = otmp->owornmask | obj->owornmask;

            /* Both the items might be worn in competing slots; merger
               preference (regardless of which is which): primary weapon +
               alternate weapon -> primary weapon; primary weapon + quiver ->
               primary weapon; alternate weapon + quiver -> alternate weapon.
               (Prior to 3.3.0, it was not possible for the two stacks to be
               worn in different slots and `obj' didn't need to be unworn when
               merging.) */
            if (wmask & W_MASK(os_wep))
                wmask = W_MASK(os_wep);
            else if (wmask & W_MASK(os_swapwep))
                wmask = W_MASK(os_swapwep);
            else if (wmask & W_MASK(os_quiver))
                wmask = W_MASK(os_quiver);
            else {
                impossible("merging strangely worn items (%lx)", wmask);
                wmask = otmp->owornmask;
            }
            if ((otmp->owornmask & ~wmask) != 0L)
                setnotworn(otmp);
            setworn(otmp, wmask);
            setnotworn(obj);
        }

        obfree(obj, otmp);      /* free(obj), bill->otmp */
        return 1;
    }
    return 0;
}

/*
  Adjust hero intrinsics as if this object was being added to the hero's
  inventory.  Called _before_ the object has been added to the hero's inventory.
  (TODO: The previous sentence appears to be incorrect. Are there any bugs
  caused by this?)

  This is called when adding objects to the hero's inventory normally (via
  addinv) or when an object in the hero's inventory has been polymorphed
  in-place.

  It may be valid to merge this code with with addinv_core2().
*/
static void
addinv_stats(struct obj *obj)
{
    if (obj->otyp == AMULET_OF_YENDOR) {
        historic_event(!obj->known, "gained the Amulet of Yendor!");
    } else if (obj->oartifact) {
        if (is_quest_artifact(obj)) {
            artitouch();
        }
    }

    if (confers_luck(obj)) {
        /* new luckstone must be in inventory by this point for correct
           calculation */
        set_moreluck();
    }
}

/* Add obj to the hero's inventory.  Make sure the object is "free".
   Adjust hero attributes as necessary. */
struct obj *
addinv(struct obj *obj)
{
    struct obj *otmp;
    boolean obj_was_thrown;

    if (obj->where != OBJ_FREE)
        panic("addinv: obj not free");

    obj_was_thrown = obj->was_thrown;

    obj->no_charge = 0; /* not meaningful for invent */
    obj->was_thrown = 0;
    obj->was_dropped = 0;

    examine_object(obj);

    /* merge if possible; find end of chain in the process */
    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (merged(&otmp, &obj)) {
            obj = otmp;
            goto added;
        }

    /* didn't merge, so insert into chain */
    assigninvlet(obj);
    extract_nobj(obj, &turnstate.floating_objects, &invent, OBJ_INVENT);
    reorder_invent();

    /* fill empty quiver if obj was thrown */
    if (flags.pickup_thrown && !uquiver && obj_was_thrown &&
        (throwing_weapon(obj) || is_ammo(obj)))
        setuqwep(obj);

added:
    addinv_stats(obj);
    carry_obj_effects(obj);     /* carrying affects the obj */
    update_inventory();
    return obj;
}

/*
 * Some objects are affected by being carried.
 * Make those adjustments here. Called _after_ the object
 * has been added to the hero's or monster's inventory,
 * and after hero's intrinsics have been updated.
 */
void
carry_obj_effects(struct obj *obj)
{
    /* Cursed figurines can spontaneously transform when carried. */
    if (obj->otyp == FIGURINE) {
        if (obj->cursed && obj->corpsenm != NON_PM &&
            !dead_species(obj->corpsenm, TRUE)) {
            attach_fig_transform_timeout(obj);
        }
    }
}


boolean
can_hold(struct obj *obj)
{
    if (obj->oclass == COIN_CLASS)
        return TRUE;
    if (merge_choice(invent, obj))
        return TRUE;
    if (inv_cnt(TRUE) < 52)
        return TRUE;
    return FALSE;
}

/*
 * Add an item to the inventory unless we're fumbling or it refuses to be
 * held (via touch_artifact), and give a message.
 * If there aren't any free inventory slots, we'll drop it instead.
 * If both success and failure messages are NULL, then we're just doing the
 * fumbling/slot-limit checking for a silent grab.  In any case,
 * touch_artifact will print its own messages if they are warranted.
 */
struct obj *
hold_another_object(struct obj *obj, const char *drop_fmt, const char *drop_arg,
                    const char *hold_msg)
{
    if (!Blind)
        obj->dknown = 1;        /* maximize mergability */
    if (obj->oartifact) {
        /* place_object may change these */
        boolean crysknife = (obj->otyp == CRYSKNIFE);
        int oerode = obj->oerodeproof;
        boolean wasUpolyd = Upolyd;

        /* in case touching this object turns out to be fatal */
        place_object(obj, level, u.ux, u.uy);

        if (!touch_artifact(obj, &youmonst)) {
            obj_extract_self(obj);      /* remove it from the floor */
            dropy(obj); /* now put it back again :-) */
            return obj;
        } else if (wasUpolyd && !Upolyd) {
            /* loose your grip if you revert your form */
            if (drop_fmt)
                pline(msgc_substitute, drop_fmt, drop_arg);
            obj_extract_self(obj);
            dropy(obj);
            return obj;
        }
        obj_extract_self(obj);
        if (crysknife) {
            obj->otyp = CRYSKNIFE;
            obj->oerodeproof = oerode;
        }
    }
    if (Fumbling) {
        if (drop_fmt)
            pline(msgc_substitute, drop_fmt, drop_arg);
        dropy(obj);
    } else {
        long oquan = obj->quan;
        int prev_encumbr = near_capacity();     /* before addinv() */

        /* encumbrance only matters if it would now become worse than max(
           current_value, stressed ) */
        if (prev_encumbr < MOD_ENCUMBER)
            prev_encumbr = MOD_ENCUMBER;

        obj = addinv(obj);
        if (obj->invlet == NOINVSYM || ((obj->otyp != LOADSTONE || !obj->cursed)
                                        && near_capacity() > prev_encumbr)) {
            if (drop_fmt)
                pline(msgc_substitute, drop_fmt, drop_arg);
            /* undo any merge which took place */
            if (obj->quan > oquan)
                obj = splitobj(obj, oquan);
            dropx(obj);
        } else {
            if (flags.autoquiver && !uquiver && !obj->owornmask &&
                (is_missile(obj) || ammo_and_launcher(obj, uwep) ||
                 ammo_and_launcher(obj, uswapwep)))
                setuqwep(obj);
            if (hold_msg || drop_fmt)
                prinv(hold_msg, obj, oquan);
        }
    }
    return obj;
}

/* useup() all of an item regardless of its quantity */
void
useupall(struct obj *obj)
{
    setnotworn(obj);
    freeinv(obj);
    obfree(obj, NULL);  /* deletes contents also */
}

void
useup(struct obj *obj)
{
    /* Note: This works correctly for containers because they */
    /* (containers) don't merge.  */
    if (obj->quan > 1L) {
        obj->in_use = FALSE;    /* no longer in use */
        obj->quan--;
        obj->owt = weight(obj);
        update_inventory();
    } else {
        useupall(obj);
    }
}

/* use one charge from an item and possibly incur shop debt for it */
void
consume_obj_charge(struct obj *obj, boolean maybe_unpaid)
/* maybe_unpaid: false if caller handles shop billing */
{
    if (maybe_unpaid)
        check_unpaid(obj);
    obj->spe -= 1;
    if (obj->known)
        update_inventory();
}


/*
Adjust hero's attributes as if this object was being removed from the
hero's inventory.  This should only be called from freeinv() and
where we are polymorphing an object already in the hero's inventory.

Should think of a better name...
*/
static void
freeinv_stats(struct obj *obj)
{
    if (obj->otyp == AMULET_OF_YENDOR) {
        /* Minor information leak about the Amulet of Yendor (vs fakes). You
           don't get any more info than you do by turning on show_uncursed
           though. */
        historic_event(!obj->known, "lost the Amulet of Yendor.");
    } else if (obj->oartifact) {
        uninvoke_artifact(obj);
    }

    if (obj->otyp == LOADSTONE) {
        curse(obj);
    } else if (confers_luck(obj)) {
        set_moreluck();
    } else if (obj->otyp == FIGURINE && obj->timed) {
        stop_timer(obj->olev, FIG_TRANSFORM, obj);
    }
}

/* remove an object from the hero's inventory */
void
freeinv(struct obj *obj)
{
    if (obj == uwep) {
        impossible("dropping item before unwielding it");
        uwepgone();
    }
    if (obj == uswapwep) {
        impossible("dropping item before unreadying it");
        uwepgone();
    }
    extract_nobj(obj, &invent, &turnstate.floating_objects, OBJ_FREE);
    freeinv_stats(obj);
    update_inventory();
}

/* swap one object for another in the inventory
 *
 * it is assumed that replace_obj() has been used already
 */
void
swapinv(struct obj *oldobj, struct obj *newobj)
{
    freeinv_stats(oldobj);
    addinv_stats(newobj);
}

void
delallobj(int x, int y)
{
    struct obj *otmp, *otmp2;

    for (otmp = level->objects[x][y]; otmp; otmp = otmp2) {
        if (otmp == uball)
            unpunish();
        /* after unpunish(), or might get deallocated chain */
        otmp2 = otmp->nexthere;
        if (otmp == uchain)
            continue;
        delobj(otmp);
    }
}


/* destroy object in obj->olev->objlist chain (if unpaid, it remains on the
   bill) */
void
delobj(struct obj *obj)
{
    boolean update_map;
    struct monst *mtmp;

    if (obj->otyp == AMULET_OF_YENDOR || obj->otyp == CANDELABRUM_OF_INVOCATION
        || obj->otyp == BELL_OF_OPENING || obj->otyp == SPE_BOOK_OF_THE_DEAD) {
        /* player might be doing something stupid, but we can't guarantee that.
           assume special artifacts are indestructible via drawbridges, and
           exploding chests, and golem creation, and ... */
        return;
    }
    update_map = (obj->where == OBJ_FLOOR);
    obj_extract_self(obj);
    if (!OBJ_AT_LEV(obj->olev, obj->ox, obj->oy) &&
        (mtmp = m_at(obj->olev, obj->ox, obj->oy)) && mtmp->mundetected &&
        hides_under(mtmp->data)) {
        mtmp->mundetected = 0;
    }
    if (update_map && obj->olev == level)
        newsym(obj->ox, obj->oy);
    obfree(obj, NULL);  /* frees contents also */
}


struct obj *
sobj_at(int otyp, struct level *lev, int x, int y)
{
    struct obj *otmp;

    for (otmp = lev->objects[x][y]; otmp; otmp = otmp->nexthere)
        if (otmp->otyp == otyp)
            return otmp;
    return NULL;
}


struct obj *
carrying(int type)
{
    struct obj *otmp;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == type)
            return otmp;
    return NULL;
}

struct obj *
carrying_questart(void)
{
    struct obj *otmp;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (otmp->oartifact && is_quest_artifact(otmp))
            return otmp;
    return NULL;
}

/* Used by functions that need to track time-consuming actions by the player on
   an object, to see if the object is still around and in reach. Returns TRUE
   for a non-NULL object in inventory or on the ground on the player's square.
   Returns FALSE for a NULL argument or for zeroobj. */
boolean
obj_with_u(struct obj *obj)
{
    if (!obj || obj == &zeroobj)
        return FALSE;
    if (obj->where == OBJ_INVENT)
        return TRUE;
    if (obj->where != OBJ_FLOOR)
        return FALSE;
    return obj_here(obj, u.ux, u.uy);
}

const char *
currency(long amount)
{
    if (amount == 1L)
        return "zorkmid";
    else
        return "zorkmids";
}

boolean
have_lizard(void)
{
    struct obj *otmp;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (otmp->otyp == CORPSE && otmp->corpsenm == PM_LIZARD)
            return TRUE;
    return FALSE;
}

struct obj *
o_on(unsigned int id, struct obj *objchn)
{
    struct obj *temp;

    while (objchn) {
        if (objchn->o_id == id)
            return objchn;
        if (Has_contents(objchn) && (temp = o_on(id, objchn->cobj)))
            return temp;
        objchn = objchn->nobj;
    }
    return NULL;
}

boolean
obj_here(struct obj *obj, int x, int y)
{
    struct obj *otmp;

    for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
        if (obj == otmp)
            return TRUE;
    return FALSE;
}


struct obj *
gold_at(struct level *lev, int x, int y)
{
    struct obj *obj = lev->objects[x][y];

    while (obj) {
        if (obj->oclass == COIN_CLASS)
            return obj;
        obj = obj->nexthere;
    }
    return NULL;
}


/* compact a string of inventory letters by dashing runs of letters; assumes at
   least three letters are given and input is in sorted order */
static const char *
compactify(const char *buf_orig)
{
    int i1 = 0, i2 = 0;
    char ilet, ilet1, ilet2;
    char buf[strlen(buf_orig)+1];
    strcpy(buf, buf_orig);

    if (buf[0] + 1 == buf[1])
        buf[1] = '-';

    ilet2 = buf[0];
    ilet1 = buf[1];
    ++i2;
    ++i1;
    ilet = buf[i1];
    while (ilet) {
        if (ilet == ilet1 + 1) {
            if (ilet1 == ilet2 + 1)
                buf[i2 - 1] = ilet1 = '-';
            else if (ilet2 == '-') {
                buf[i2 - 1] = ++ilet1;
                buf[i2] = buf[++i1];
                ilet = buf[i1];
                continue;
            }
        }
        ilet2 = ilet1;
        ilet1 = ilet;
        buf[++i2] = buf[++i1];
        ilet = buf[i1];
    }

    return msg_from_string(buf);
}

/* ugly checks that were pulled out of getobj. The intent is to determine
   whether object otmp should be displayed for the query descibed by word. This
   function assumes that otmp->oclass is known to be allowed.

   TODO: Eventually we want to remove oclass-based validation altogether and
   just have this function do everything itself. */
static enum obj_use_status
object_selection_checks(struct obj *otmp, const char *word)
{
    long dummy;
    int otyp = otmp->otyp;

    /* cblock controls whether we allow equip commands that require removing
       other items temporarily. */
    boolean cblock = flags.cblock;

    /* Check to see if equipping/unequipping is known to be unreasonable, either
       because the object is inappropriate or the slot is blocked by a cursed
       item (cblock == TRUE) or any item (cblock == FALSE).

       There are three sets of checks. We return IMPOSSIBLE_USE if the object is
       one that inherently can't be worn (this never happens for wield checks);
       CURRENTLY_NOT_USABLE if the request is impossible right now;
       NONSENSIBLE_USE if the request is possible right now but not sensible
       (e.g. wielding food rations).

       ALREADY_IN_USE no longer exists; we don't want to ruin muscle memory via
       printing "You don't have anything else to wear" under any circumstances,
       and we allow the caller to print a more sensible message as to why
       something can't be equipped. We consider already worn items to be
       non-sensible choices instead, and currently wielded items to be just fine
       as choices. In the case where a player tries to wield an object over
       itself, we get equip_heartbeat to print the message. */
    if ((!strcmp(word, "wear") || !strcmp(word, "take off")) &&
        otmp->oclass != ARMOR_CLASS && otmp->oclass != AMULET_CLASS &&
        otmp->oclass != RING_CLASS && otmp->otyp != MEAT_RING &&
        otmp->otyp != BLINDFOLD && otmp->otyp != LENSES && otmp->otyp != TOWEL)
        return IMPOSSIBLE_USE;
    if (((!strcmp(word, "take off") || !strcmp(word, "unequip")) &&
         !canunwearobj(otmp, FALSE, FALSE, cblock)) ||
        (!strcmp(word, "wear") &&
         !canwearobj(otmp, &dummy, FALSE, FALSE, cblock)) ||
        (!strcmp(word, "wield") && !canwieldobj(otmp, FALSE, FALSE, cblock)) ||
        (!strcmp(word, "ready") && !canreadyobj(otmp, FALSE, FALSE, cblock)) ||
        (!strcmp(word, "quiver") && !canreadyobj(otmp, FALSE, FALSE, cblock)))
        return CURRENTLY_NOT_USABLE;
    if ((!strcmp(word, "wield") &&
         canwieldobj(otmp, FALSE, FALSE, cblock) != 2) ||
        (!strcmp(word, "ready") &&
         !(canreadyobj(otmp, FALSE, FALSE, cblock) & 4)) ||
        (!strcmp(word, "quiver") &&
         !(canreadyobj(otmp, FALSE, FALSE, cblock) & 2)) ||
        (!strcmp(word, "wear") && (otmp->owornmask & W_WORN)))
        return NONSENSIBLE_USE;

    /* Other verbs: impossible uses (i.e. these actions cannot meaningfully
       even be attempted). */
    if ((!strcmp(word, "eat") && !is_edible(otmp, FALSE)) ||
        (!strcmp(word, "rub") && /* TODO: Downgrade to nonsensible */
         ((otmp->oclass == TOOL_CLASS && otyp != OIL_LAMP &&
           otyp != MAGIC_LAMP && otyp != BRASS_LANTERN) ||
          (otmp->oclass == GEM_CLASS && !is_graystone(otmp)))) ||
        (!strcmp(word, "sacrifice") && !can_sacrifice(otmp)) ||
        (!strcmp(word, "tin") && (otyp != CORPSE || !tinnable(otmp))) ||
        ((!strcmp(word, "use or apply") || !strcmp(word, "untrap with")) &&
         /* Picks, axes, pole-weapons, bullwhips */
         ((otmp->oclass == WEAPON_CLASS && !is_pick(otmp) &&
           !is_axe(otmp) && !is_pole(otmp) && otyp != BULLWHIP) ||
          (otmp->oclass == FOOD_CLASS && otyp != CREAM_PIE &&
           otyp != EUCALYPTUS_LEAF) ||
          (otmp->oclass == GEM_CLASS && !is_graystone(otmp)))) ||
        (!strcmp(word, "read") &&
         (otmp->oclass != SCROLL_CLASS && otmp->oclass != SPBOOK_CLASS &&
          otyp != FORTUNE_COOKIE && otyp != T_SHIRT)) ||
        (!strcmp(word, "untrap with") &&
         (otmp->oclass == TOOL_CLASS && otyp != CAN_OF_GREASE)) ||
        (!strcmp(word, "charge") &&
         (otmp->oclass != TOOL_CLASS && otmp->oclass != WAND_CLASS &&
          otmp->oclass != RING_CLASS)))
        return IMPOSSIBLE_USE;

    /* Other verbs: currently impossible uses. */
    if ((!strcmp(word, "eat") && !is_edible(otmp, TRUE)))
        return CURRENTLY_NOT_USABLE;

    /* Other verbs: non-recommended uses. */
    if ((!strcmp(word, "write with") &&
         (otmp->oclass == TOOL_CLASS && otyp != MAGIC_MARKER &&
          otyp != TOWEL)) ||
        (!strcmp(word, "read") &&
         (otmp->oclass != SCROLL_CLASS && otmp->oclass != SPBOOK_CLASS)) ||
        (!strncmp(word, "rub on the stone", 16) &&
         otmp->oclass == GEM_CLASS && /* using known touchstone */
         otmp->dknown && objects[otyp].oc_name_known) ||
        (!strncmp(word, "invoke", 6) &&
         (!otmp->oartifact && !objects[otyp].oc_unique &&
          (otyp != FAKE_AMULET_OF_YENDOR || otmp->known) &&
          otmp->oclass != WAND_CLASS &&   /* V for breaking wands */
          ((otmp->oclass == TOOL_CLASS && /* V for rubbing */
            otyp != OIL_LAMP && otyp != MAGIC_LAMP && otyp != BRASS_LANTERN) ||
           (otmp->oclass == GEM_CLASS && !is_graystone(otmp)) ||
           (otmp->oclass != TOOL_CLASS && otmp->oclass != GEM_CLASS)) &&
          otyp != CRYSTAL_BALL &&         /* V for applying */
          /* note: presenting the possibility of invoking non-artifact
             mirrors and/or lamps is a simply a cruel deception... */
          otyp != MIRROR && otyp != MAGIC_LAMP &&
          (otyp != OIL_LAMP || /* don't list known oil lamp */
           (otmp->dknown && objects[OIL_LAMP].oc_name_known)))) ||
        (!strcmp(word, "throw") &&
         !(ammo_and_launcher(otmp, uwep) || is_missile(otmp)) &&
         !(throws_rocks(youmonst.data) && otyp == BOULDER)) ||
        (!strcmp(word, "charge") && !is_chargeable(otmp)))
        return NONSENSIBLE_USE;

    return OBJECT_USABLE;
}


/*
 * Requests an object from the player's inventory.
 *
 * getobj returns:
 *      struct obj *xxx:  object to do something with.
 *      NULL              error return: no object.
 *      &zeroobj          explicitly no object (as in w-).
 * TODO: test if gold can be used in unusual ways (eaten etc.)
 *
 * The user will be given a list of the "OBJECT_USABLE" uses by default;
 * pressing ? also shows this list. Pressing * shows that list, and also all
 * "NONSENSIBLE_USE" objects. With allowall, all objects are treated as at
 * least "NONSENSIBLE_USE". (TODO: Remove allowall altogether.)
 */
struct obj *
getobj(const char *let, const char *word, boolean isarg)
{
    struct obj *otmp;
    char ilet;
    const char *qbuf;
    xchar allowcnt = 0; /* 0, 1 or 2 */
    boolean split_letter = FALSE;
    boolean allowall = FALSE;
    boolean allownone = FALSE;
    char nonechar = '-';
    int cnt;
    boolean prezero = FALSE;

    if (isarg && !program_state.in_zero_time_command)
        flags.last_arg.argtype &= ~CMD_ARG_OBJ;

    for (; *let > MAXOCLASSES; ++let) {
        if (*let == ALLOW_COUNT)
            allowcnt = 1;
        else if (*let == ALL_CLASSES)
            allowall = TRUE;
        else if (*let == ALLOW_NONE)
            allownone = TRUE;
        else if (*let == NONE_ON_COMMA)
            nonechar = ',';
        else if (*let == SPLIT_LETTER)
            split_letter = TRUE;
        else
            panic("invalid class parameter to getobj(): %d", (int)*let);
    }

    /* Count the number of items that may end up in "buf". */
    int bufmaxlen = 0;
    for (otmp = invent; otmp; otmp = otmp->nobj)
        bufmaxlen++;
    bufmaxlen += 9; /* ", " ITEMS " or ?*\0" */

    /* Calculate two buffers:
       - "buf" lists OBJECT_USABLE objects only
       - "altbuf" also lists NONSENSIBLE_USE objects
    */
    char buf[bufmaxlen];
    char *bp = buf;
    char altbuf[bufmaxlen];
    char *ap = altbuf;

    int buflen = 0; /* number of item in buf, not counting nonechar */

    if (allownone) {
        *bp++ = *ap++ = nonechar;
        *bp++ = *ap++ = ' ';
    }

    /* This code is massively refactored from the 3.4.3 version because the
       old version was such a pain to follow (e.g. "bp" and "ap" used entirely
       different storage). */
    for (otmp = invent; otmp; otmp = otmp->nobj) {
        if (!*let || strchr(let, otmp->oclass)) {
            switch (object_selection_checks(otmp, word)) {
            case CURRENTLY_NOT_USABLE:
            case IMPOSSIBLE_USE:
                break;

            case NONSENSIBLE_USE:
                *ap++ = otmp->invlet;
                break;

            case OBJECT_USABLE:
                *ap++ = *bp++ = otmp->invlet;
                buflen++;
                break;
            }
        } else if (allowall) {
            switch (object_selection_checks(otmp, word)) {
            case CURRENTLY_NOT_USABLE:
            case IMPOSSIBLE_USE:
                break;

            case OBJECT_USABLE:
            case NONSENSIBLE_USE:
                *ap++ = otmp->invlet;
                break;
            }
        }
    }

    *ap = *bp = '\0';
    if (!buflen && bp > buf && bp[-1] == ' ')
        *--bp = 0;

    const char *buf_compact = buflen > 5 ? compactify(buf) : buf;

    /* TODO: Get rid of this! */
    if (!*altbuf && !allowall) {
        pline(msgc_cancelled, "You don't have anything to %s.", word);
        return NULL;
    }

    for (;;) {
        cnt = 0;
        if (allowcnt == 2)
            allowcnt = 1;       /* abort previous count */
        if (!buf[0]) {
            qbuf = msgprintf("What do you want to %s? [*]", word);
        } else {
            qbuf = msgprintf("What do you want to %s? [%s or ?*]",
                             word, buf_compact);
        }
        ilet = query_key(qbuf, nonechar == ',' ? NQKF_INVENTORY_OR_FLOOR :
                         allownone ? NQKF_INVENTORY_ITEM_NULLABLE :
                         NQKF_INVENTORY_ITEM, allowcnt ? &cnt : NULL);
        if (allowcnt == 1 && cnt != -1) {
            allowcnt = 2;       /* signal presence of cnt */
            if (cnt == 0)
                prezero = TRUE; /* cnt was explicitly set to 0 */
        }
        if (cnt == -1)
            cnt = 0;

        if (strchr(quitchars, ilet)) {
            pline(msgc_cancelled, "Never mind.");
            return NULL;
        }
        if (ilet == nonechar) {
            if (allownone && isarg && !program_state.in_zero_time_command) {
                flags.last_arg.argtype |= CMD_ARG_OBJ;
                flags.last_arg.invlet = nonechar;
            }
            return allownone ? &zeroobj : NULL;
        }
        if (ilet == def_oc_syms[COIN_CLASS]) {
            if (cnt == 0 && prezero)
                return NULL;
            /* Historic note: early Nethack had a bug which was first reported
               for Larn, where trying to drop 2^32-n gold pieces was allowed,
               and did interesting things to your money supply.  The LRS is the
               tax bureau from Larn. */
            if (cnt < 0) {
                pline(msgc_cancelled, "The LRS would be very interested to "
                      "know you have that much.");
                return NULL;
            }
        }
        if (ilet == '?' || ilet == '*') {
            char *allowed_choices = (ilet == '?') ? buf : altbuf;
            long ctmp = 0;

            ilet =
                display_pickinv(allowed_choices, TRUE, allowcnt ? &ctmp : NULL);
            if (!ilet)
                continue;
            if (allowcnt && ctmp >= 0) {
                cnt = ctmp;
                if (!cnt)
                    prezero = TRUE;
                allowcnt = 2;
            }
            if (ilet == '\033') {
                pline(msgc_cancelled, "Never mind.");
                return NULL;
            }
            /* they typed a letter (not a space) at the prompt */
        }
        if (allowcnt == 2 && !strcmp(word, "throw")) {
            /* permit counts for throwing gold, but don't accept counts for
               other things since the throw code will split off a single item
               anyway */
            if (ilet != def_oc_syms[COIN_CLASS])
                allowcnt = 1;
            if (cnt == 0 && prezero)
                return NULL;
            /* TODO: This is simply factually incorrect. Daggerstorm, anyone? */
            if (cnt > 1) {
                pline(msgc_hint, "You can only throw one item at a time.");
                continue;
            }
        }

        for (otmp = invent; otmp; otmp = otmp->nobj)
            if (otmp->invlet == ilet)
                break;
        if (!otmp) {
            pline(msgc_mispaste, "You don't have that object.");
            continue;
        } else if (cnt < 0 || otmp->quan < cnt) {
            pline(msgc_cancelled,
                  "You don't have that many!  You have only %ld.",
                  (long)otmp->quan);
            continue;
        }
        break;
    }
    if (!allowall && let && !strchr(let, otmp->oclass)) {
        silly_thing(word, otmp);
        return NULL;
    }
    if (allowcnt == 2) {        /* cnt given */
        if (cnt == 0)
            return NULL;
        if (cnt != otmp->quan) {
            if (split_letter) {
                if (otmp->otyp == LOADSTONE && otmp->cursed) {
                    pline(msgc_cancelled, "You can't seem to get them apart.");
                    return NULL;
                }
                else if (inv_cnt(TRUE) >= 52) {
                    pline(msgc_cancelled,
                          "You don't have room to handle those separately.");
                    return NULL;
                }

                otmp = splitobj(otmp, cnt);
                assigninvlet(otmp);
                reorder_invent();
                update_inventory();
            } else {
                /* don't split a stack of cursed loadstones */
                if (otmp->otyp == LOADSTONE && otmp->cursed)
                    /* kludge for canletgo()'s can't-drop-this message */
                    otmp->corpsenm = (int)cnt;
                else
                    otmp = splitobj(otmp, cnt);
            }
        }
    }

    if (isarg && !program_state.in_zero_time_command) {
        flags.last_arg.argtype |= CMD_ARG_OBJ;
        flags.last_arg.invlet = otmp->invlet;
    }

    return otmp;
}


boolean
validate_object(struct obj * obj, const char *lets, const char *word)
{
    boolean allowall = !lets || !!strchr(lets, ALL_CLASSES);

    if (!allowall && lets && !strchr(lets, obj->oclass)) {
        silly_thing(word, obj);
        return FALSE;
    }

    switch (object_selection_checks(obj, word)) {
    default:
    case CURRENTLY_NOT_USABLE:
        if (allowall)
            return TRUE;
        /*FALLTHRU*/
    case IMPOSSIBLE_USE:
        silly_thing(word, obj);
        return FALSE;

    case OBJECT_USABLE:
    case NONSENSIBLE_USE:
        return TRUE;
    }
}


void
silly_thing(const char *word, struct obj *otmp)
{
    pline(msgc_mispaste, "That is a silly thing to %s.", word);
}


boolean
wearing_armor(void)
{
    return ((boolean) ((uarm && !uskin()) ||
                       uarmc || uarmf || uarmg || uarmh || uarms || uarmu));
}

boolean
is_worn(const struct obj * otmp)
{
    return ((boolean) (!!(otmp->owornmask & (W_EQUIP | W_MASK(os_saddle)))));
}


/*
 * Object identification routines:
 */

/* make an object actually be identified; no display updating */
void
fully_identify_obj(struct obj *otmp)
{
    makeknown(otmp->otyp);
    if (otmp->oartifact)
        discover_artifact((xchar) otmp->oartifact);
    otmp->known = otmp->dknown = otmp->bknown = otmp->rknown = 1;
    if (otmp->otyp == EGG && otmp->corpsenm != NON_PM)
        learn_egg_type(otmp->corpsenm);
}

/* ggetobj callback routine; identify an object and give immediate feedback */
int
identify(struct obj *otmp)
{
    fully_identify_obj(otmp);
    prinv(NULL, otmp, 0L);
    return 1;
}

/* menu of unidentified objects; select and identify up to id_limit of them */
static void
menu_identify(int id_limit)
{
    struct object_pick *pick_list;
    int n, i, first = 1;
    const char *buf;

    /* assumptions: id_limit > 0 and at least one unID'd item is present */

    while (id_limit) {
        buf = msgprintf("What would you like to identify %s?",
                        first ? "first" : "next");
        n = query_objlist(buf, invent,
                          SIGNAL_NOMENU | USE_INVLET | INVORDER_SORT,
                          &pick_list, PICK_ANY, not_fully_identified);

        if (n > 0) {
            if (n > id_limit)
                n = id_limit;
            for (i = 0; i < n; i++, id_limit--)
                identify(pick_list[i].obj);
            free(pick_list);
        } else {
            if (n < 0)
                pline(msgc_info, "That was all.");
            id_limit = 0;       /* Stop now */
        }
        first = 0;
    }
}

/* dialog with user to identify a given number of items; 0 means all */
void
identify_pack(int id_limit)
{
    struct obj *obj, *the_obj;
    int n, unid_cnt;

    unid_cnt = 0;
    the_obj = 0;        /* if unid_cnt ends up 1, this will be it */
    for (obj = invent; obj; obj = obj->nobj)
        if (not_fully_identified(obj))
            ++unid_cnt, the_obj = obj;

    if (!unid_cnt) {
        pline(msgc_info,
              "You have already identified all of your possessions.");
    } else if (!id_limit) {
        /* identify everything */
        if (unid_cnt == 1) {
            identify(the_obj);
        } else {

            /* TODO: use fully_identify_obj and cornline/menu/whatever here */
            for (obj = invent; obj; obj = obj->nobj)
                if (not_fully_identified(obj))
                    identify(obj);

        }
    } else {
        /* identify up to `id_limit' items */
        n = 0;
        if (n == 0 || n < -1)
            menu_identify(id_limit);
    }
    update_inventory();
}


/* should of course only be called for things in invent */
static char
obj_to_let(struct obj *obj)
{
    return obj->invlet;
}

/*
 * Print the indicated quantity of the given object.  If quan == 0L then use
 * the current quantity.
 */
void
prinv(const char *prefix, struct obj *obj, long quan)
{
    if (!prefix)
        prefix = "";
    pline(msgc_info, "%s%s%s", prefix, *prefix ? " " : "",
          xprname(obj, NULL, obj_to_let(obj), TRUE, 0L, quan));
}



const char *
xprname(struct obj *obj, const char *txt, /* text to print instead of obj */
        char let, /* inventory letter */
        boolean dot,      /* append period; (dot && cost => Iu) */
        long cost,        /* cost (for inventory of unpaid or expended
                             items) */
        long quan) {      /* if non-0, print this quantity, not obj->quan */
    const char *li;
    boolean use_invlet = let != CONTAINED_SYM;
    long savequan = 0;

    if (quan && obj) {
        savequan = obj->quan;
        obj->quan = quan;
    }

    /*
     * If let is:
     *  *  Then obj == null and we are printing a total amount.
     *  >  Then the object is contained and doesn't have an inventory letter.
     */
    if (cost != 0 || let == '*') {
        /* if dot is true, we're doing Iu, otherwise Ix */
        li = msgprintf("%c - %-45s %6ld %s",
                       (dot && use_invlet ? obj->invlet : let),
                       (txt ? txt : doname(obj)), cost, currency(cost));
    } else {
        /* ordinary inventory display or pickup message */
        li = msgprintf("%c - %s%s", (use_invlet ? obj->invlet : let),
                       (txt ? txt : doname(obj)), (dot ? "." : ""));
    }
    if (savequan)
        obj->quan = savequan;

    return li;
}



/* the 'i' command */
int
ddoinv(const struct nh_cmd_arg *arg)
{
    (void) arg;
    if (!invent)
        pline(msgc_info, "You are not carrying anything.");
    else
        display_inventory(NULL, FALSE);
    return 0;
}

/*
 * find_unpaid()
 *
 * Scan the given list of objects.  If last_found is NULL, return the first
 * unpaid object found.  If last_found is not NULL, then skip over unpaid
 * objects until last_found is reached, then set last_found to NULL so the
 * next unpaid object is returned.  This routine recursively follows
 * containers.
 */
static struct obj *
find_unpaid(struct obj *list, struct obj **last_found)
{
    struct obj *obj;

    while (list) {
        if (list->unpaid) {
            if (*last_found) {
                /* still looking for previous unpaid object */
                if (list == *last_found)
                    *last_found = NULL;
            } else
                return *last_found = list;
        }
        if (Has_contents(list)) {
            if ((obj = find_unpaid(list->cobj, last_found)) != 0)
                return obj;
        }
        list = list->nobj;
    }
    return NULL;
}


static void
make_invlist(struct nh_objlist *objlist, const char *lets)
{
    struct obj *otmp;
    char ilet;
    int classcount;
    const char *invlet = flags.inv_order;

nextclass:
    classcount = 0;
    for (otmp = invent; otmp; otmp = otmp->nobj) {
        ilet = otmp->invlet;
        if (!lets || !*lets || strchr(lets, ilet)) {
            if (!flags.sortpack || otmp->oclass == *invlet) {
                if (flags.sortpack && !classcount) {
                    add_objitem(objlist, MI_HEADING, 0,
                                let_to_name(*invlet, FALSE), otmp, FALSE);
                    classcount++;
                }
                examine_object(otmp);
                add_objitem(objlist, MI_NORMAL, ilet,
                            doname(otmp), otmp, TRUE);
            }
        }
    }
    if (flags.sortpack) {
        if (*++invlet)
            goto nextclass;
    }
}


/*
 * Internal function used by display_inventory and getobj that can display
 * inventory and return a count as well as a letter. If out_cnt is not null,
 * any count returned from the menu selection is placed here.
 */
static char
display_pickinv(const char *lets, boolean want_reply, long *out_cnt)
{
    struct obj *otmp;
    char ret;
    int n = 0;
    struct nh_objlist objlist;

    /*
       Exit early if no inventory -- but keep going if we are doing a permanent
       inventory update.  We need to keep going so the permanent inventory
       window updates itself to remove the last item(s) dropped.  One down
       side: the addition of the exception for permanent inventory window
       updates _can_ pop the window up when it's not displayed -- even if it's
       empty -- because we don't know at this level if its up or not.  This may
       not be an issue if empty checks are done before hand and the call to
       here is short circuited away. */
    if (!invent && !(!lets && !want_reply)) {
        pline(msgc_info, "Not carrying anything.");
        return 0;
    }

    if (lets && strlen(lets) == 1) {
        /* when only one item of interest, use pline instead of menus */
        ret = '\0';
        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (otmp->invlet == lets[0]) {
                pline(msgc_info, "%s",
                      xprname(otmp, NULL, lets[0], TRUE, 0L, 0L));
                if (out_cnt)
                    *out_cnt = -1L;     /* select all */
                break;
            }
        }
        return ret;
    }

    init_objmenulist(&objlist);
    make_invlist(&objlist, lets);

    const struct nh_objresult *selected = NULL;

    if (objlist.icount)
        n = display_objects(&objlist, want_reply ? NULL : "Inventory:",
                            want_reply ? PICK_ONE : PICK_NONE, PLHINT_INVENTORY,
                            &selected);
    else
        dealloc_objmenulist(&objlist);

    if (n > 0) {
        ret = (char)selected[0].id;
        if (out_cnt)
            *out_cnt = selected[0].count;
    } else
        ret = !n ? '\0' : '\033';       /* cancelled */

    return ret;
}

/*
 * If lets == NULL or "", list all objects in the inventory.  Otherwise,
 * list all objects with object classes that match the order in lets.
 *
 * Returns the letter identifier of a selected item, or 0 if nothing
 * was selected.
 */
char
display_inventory(const char *lets, boolean want_reply)
{
    return display_pickinv(lets, want_reply, NULL);
}


void
update_inventory(void)
{
    struct nh_objlist objlist;

    if (!windowprocs.win_list_items || program_state.suppress_screen_updates)
        return;

    init_objmenulist(&objlist);
    make_invlist(&objlist, NULL);
    win_list_items(&objlist, TRUE);
}


/*
 * Returns the number of unpaid items within the given list.  This includes
 * contained objects.
 */
int
count_unpaid(struct obj *list)
{
    int count = 0;

    while (list) {
        if (list->unpaid)
            count++;
        if (Has_contents(list))
            count += count_unpaid(list->cobj);
        list = list->nobj;
    }
    return count;
}

/*
 * Returns the number of items with b/u/c/unknown within the given list.
 * This does NOT include contained objects.
 */
int
count_buc(struct obj *list, int type)
{
    int count = 0;

    while (list) {
        if (Role_if(PM_PRIEST))
            list->bknown = TRUE;
        switch (type) {
        case BUC_BLESSED:
            if (list->oclass != COIN_CLASS && list->bknown && list->blessed)
                count++;
            break;
        case BUC_CURSED:
            if (list->oclass != COIN_CLASS && list->bknown && list->cursed)
                count++;
            break;
        case BUC_UNCURSED:
            if (list->oclass != COIN_CLASS && list->bknown && !list->blessed &&
                !list->cursed)
                count++;
            break;
        case BUC_UNKNOWN:
            if (list->oclass != COIN_CLASS && !list->bknown)
                count++;
            break;
        case UNIDENTIFIED:
            if (not_fully_identified_core(list, TRUE))
                count++;
            break;
        default:
            impossible("need count of curse status %d?", type);
            return 0;
        }
        list = list->nobj;
    }
    return count;
}

static void
dounpaid(void)
{
    struct obj *otmp, *marker;
    char ilet;
    char *invlet = flags.inv_order;
    int classcount, count, num_so_far;
    int save_unpaid = 0;        /* lint init */
    long cost, totcost;
    struct nh_menulist menu;

    count = count_unpaid(invent);

    if (count == 1) {
        marker = NULL;
        otmp = find_unpaid(invent, &marker);

        /* see if the unpaid item is in the top level inventory */
        for (marker = invent; marker; marker = marker->nobj)
            if (marker == otmp)
                break;

        pline(msgc_info, "%s",
              xprname(otmp, distant_name(otmp, doname),
                      marker ? otmp->invlet : CONTAINED_SYM, TRUE, 0, 0L));
        return;
    }

    totcost = 0;
    num_so_far = 0;     /* count of # printed so far */

    init_menulist(&menu);

    do {
        classcount = 0;
        for (otmp = invent; otmp; otmp = otmp->nobj) {
            ilet = otmp->invlet;
            if (otmp->unpaid) {
                if (!flags.sortpack || otmp->oclass == *invlet) {
                    if (flags.sortpack && !classcount) {
                        add_menutext(&menu, let_to_name(*invlet, TRUE));
                        classcount++;
                    }

                    totcost += cost = unpaid_cost(otmp);
                    /* suppress "(unpaid)" suffix */
                    save_unpaid = otmp->unpaid;
                    otmp->unpaid = 0;
                    add_menutext(&menu,
                                 xprname(otmp, distant_name(otmp, doname), ilet,
                                         TRUE, cost, 0L));
                    otmp->unpaid = save_unpaid;
                    num_so_far++;
                }
            }
        }
    } while (flags.sortpack && (*++invlet));

    if (count > num_so_far) {
        /* something unpaid is contained */
        if (flags.sortpack)
            add_menutext(&menu, let_to_name(CONTAINED_SYM, TRUE));

        /*
         * Search through the container objects in the inventory for
         * unpaid items.  The top level inventory items have already
         * been listed.
         */
        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (Has_contents(otmp)) {
                marker = NULL;  /* haven't found any */
                while (find_unpaid(otmp->cobj, &marker)) {
                    totcost += cost = unpaid_cost(marker);
                    save_unpaid = marker->unpaid;
                    marker->unpaid = 0; /* suppress "(unpaid)" suffix */
                    add_menutext(&menu,
                                 xprname(marker, distant_name(marker, doname),
                                         CONTAINED_SYM, TRUE, cost, 0L));
                    marker->unpaid = save_unpaid;
                }
            }
        }
    }

    add_menutext(&menu, "");
    add_menutext(&menu, xprname(NULL, "Total:", '*', FALSE, totcost, 0L));

    display_menu(&menu, NULL, PICK_NONE, PLHINT_INVENTORY, NULL);
}


/* query objlist callback: return TRUE if obj type matches "this_type" */
static int this_type;

static boolean
this_type_only(const struct obj *obj)
{
    return obj->oclass == this_type;
}

/* the 'I' command */
int
dotypeinv(const struct nh_cmd_arg *arg)
{
    char c = '\0';
    int n, i = 0;
    int unpaid_count;
    boolean billx = *u.ushops && doinvbill(0);
    const int *pick_list;
    struct object_pick *dummy;
    const char *prompt = "What type of object do you want an inventory of?";

    (void) arg;

    if (!invent && !billx) {
        pline(msgc_info, "You aren't carrying anything.");
        return 0;
    }
    unpaid_count = count_unpaid(invent);

    i = UNPAID_TYPES;
    if (billx)
        i |= BILLED_TYPES;
    n = query_category(prompt, invent, i, &pick_list, PICK_ONE);
    if (!n)
        return 0;
    this_type = c = pick_list[0];

    if (c == 'x') {
        if (billx)
            doinvbill(1);
        else
            pline(msgc_cancelled, "No used-up objects on your shopping bill.");
        return 0;
    }
    if (c == 'u') {
        if (unpaid_count)
            dounpaid();
        else
            pline(msgc_cancelled, "You are not carrying any unpaid objects.");
        return 0;
    }

    if (query_objlist
        (NULL, invent, USE_INVLET | INVORDER_SORT, &dummy, PICK_NONE,
         this_type_only) > 0)
        free(dummy);
    return 0;
}

/* return a string describing the dungeon feature at <x,y> if there
   is one worth mentioning at that location; otherwise null */
static const char *
dfeature_at(int x, int y)
{
    struct rm *loc = &level->locations[x][y];
    int ltyp = loc->typ, cmap = -1;
    const char *dfeature = NULL;

    if (IS_DOOR(ltyp)) {
        switch (loc->doormask) {
        case D_NODOOR:
            cmap = S_ndoor;
            break;      /* "doorway" */
        case D_ISOPEN:
            cmap = S_vodoor;
            break;      /* "open door" */
        case D_BROKEN:
            dfeature = "broken door";
            break;
        default:
            cmap = S_vcdoor;
            break;      /* "closed door" */
        }
        /* override door description for open drawbridge */
        if (is_drawbridge_wall(x, y) >= 0)
            dfeature = "open drawbridge portcullis", cmap = -1;
    } else if (IS_FOUNTAIN(ltyp))
        cmap = S_fountain;      /* "fountain" */
    else if (IS_THRONE(ltyp))
        cmap = S_throne;        /* "opulent throne" */
    else if (is_lava(level, x, y))
        cmap = S_lava;  /* "molten lava" */
    else if (is_ice(level, x, y))
        cmap = S_ice;   /* "ice" */
    else if (is_pool(level, x, y))
        dfeature = "pool of water";
    else if (IS_SINK(ltyp))
        cmap = S_sink;  /* "sink" */
    else if (IS_ALTAR(ltyp))
        dfeature = msgprintf("altar to %s (%s)", a_gname(),
                             align_str(Amask2align(loc->altarmask & AM_MASK)));
    else if (x == level->sstairs.sx && y == level->sstairs.sy &&
               level->sstairs.up)
        cmap = S_upsstair;      /* "long ladder up" */
    else if (x == level->sstairs.sx && y == level->sstairs.sy)
        cmap = S_dnsstair;      /* "long ladder down" */
    else if (x == level->upstair.sx && y == level->upstair.sy)
        cmap = S_upstair;       /* "staircase up" */
    else if (x == level->dnstair.sx && y == level->dnstair.sy)
        cmap = S_dnstair;       /* "staircase down" */
    else if (x == level->upladder.sx && y == level->upladder.sy)
        cmap = S_upladder;      /* "ladder up" */
    else if (x == level->dnladder.sx && y == level->dnladder.sy)
        cmap = S_dnladder;      /* "ladder down" */
    else if (ltyp == DRAWBRIDGE_DOWN)
        cmap = S_vodbridge;     /* "lowered drawbridge" */
    else if (ltyp == DBWALL)
        cmap = S_vcdbridge;     /* "raised drawbridge" */
    else if (IS_GRAVE(ltyp))
        cmap = S_grave; /* "grave" */
    else if (ltyp == TREE)
        cmap = S_tree;  /* "tree" */
    else if (ltyp == IRONBARS)
        dfeature = "set of iron bars";

    if (cmap >= 0)
        dfeature = defexplain[cmap];
    return dfeature;
}


/* A companion to look_here, this function will fully describe the location for
   win_list_items, including traps and features. It does not call pline. This
   function will only list up to 4 items if Blind, unless the caller sets
   all_objects to TRUE. In that case the caller should also perform cockatrice
   checks. */
void
update_location(boolean all_objects)
{
    boolean minv = FALSE;
    struct obj *otmp = level->objects[u.ux][u.uy];
    struct trap *trap;
    const char *dfeature = NULL;
    int ocount;
    struct nh_objlist objlist;

    if (Blind && !can_reach_floor()) {
        win_list_items(NULL, FALSE);
        return;
    }

    init_objmenulist(&objlist);

    if (Engulfed && u.ustuck) {
        otmp = u.ustuck->minvent;
        minv = TRUE;
    } else {
        if ((trap = t_at(level, u.ux, u.uy)) && trap->tseen) {
            const char *buf;
            buf = msgprintf("There is %s here.",
                            an(trapexplain[trap->ttyp - 1]));
            add_objitem(&objlist, MI_TEXT, 0, buf, NULL, FALSE);
        }

        dfeature = dfeature_at(u.ux, u.uy);
        if (dfeature && !strcmp(dfeature, "pool of water") && Underwater)
            dfeature = NULL;
        if (dfeature) {
            const char *buf;
            buf = msgprintf("There is %s here.", an(dfeature));
            add_objitem(&objlist, MI_TEXT, 0, buf, NULL, FALSE);
        }
    }

    /* match look_here: unless Underwater (checked above), water and lava
       don't appear to contain items */
    ocount = 0;
    if (!dfeature || (strcmp(dfeature, "pool of water") != 0 &&
                      strcmp(dfeature, "molten lava") != 0))
        for (; otmp; otmp = minv ? otmp->nobj : otmp->nexthere) {
            examine_object(otmp);
            if (dfeature && !ocount)
                add_objitem(&objlist, MI_TEXT, 0, "", NULL, FALSE);
            if (!Blind || all_objects || ocount < 5)
                add_objitem(&objlist, MI_NORMAL, 0,
                            doname_price(otmp), otmp, FALSE);
            ocount++;
        }

    if (Blind && !all_objects && ocount >= 5) {
        const char *buf;
        buf = msgprintf("There are %s other objects here.",
                        (ocount <= 10) ? "several" : "many");
        add_objitem(&objlist, MI_TEXT, 0, buf, NULL, FALSE);
    }

    win_list_items(&objlist, FALSE);
}


/* look at what is here; if there are many objects (5 or more),
   don't show them unless obj_cnt is 0 */
int
look_here(int obj_cnt,  /* obj_cnt > 0 implies that autopickup is in progess */
          boolean picked_some, boolean show_weight, boolean feeling)
{
    struct obj *otmp;
    struct trap *trap;
    const char *verb = feeling ? "feel" : "see";
    const char *dfeature = NULL;
    boolean felt_cockatrice = FALSE;
    struct nh_objlist objlist;
    const char *fbuf = NULL;
    const char *title =
        feeling ? "Things that you feel here:" : "Things that are here:";

    if (Blind && !feeling) {
        pline(msgc_controlhelp, "You can't see!  "
              "(You can feel around with 'grope', typically on ^G.)");
        return 0;
    }

    /* Show the "things that are here" window if it was explicitly requested
       (obj_cnt == 0). Otherwise, print a summary. */
    boolean looked_explicitly = !obj_cnt;

    update_location(looked_explicitly);

    if (Engulfed && u.ustuck) {
        struct monst *mtmp = u.ustuck;

        /* If you ever need to implement code for autopickup inside a monster
           rather than this impossible(), make sure you don't pop up a message
           box, like the current code does. */
        if (!looked_explicitly)
            impossible("Autopickup inside a monster?");

        fbuf = msgprintf("Contents of %s %s", s_suffix(mon_nam(mtmp)),
                         mbodypart(mtmp, STOMACH));
        /* Skip "Contents of " by using fbuf index 12 */
        pline(msgc_occstart, "You %s to %s what is lying in %s.",
              feeling ? "try" : "look around", verb, fbuf + 12);
        otmp = mtmp->minvent;
        if (otmp) {
            for (; otmp; otmp = otmp->nobj) {
                /* If swallower is an animal, it should have become stone
                   but... */
                if (otmp->otyp == CORPSE)
                    feel_cockatrice(otmp, feeling,
                                    msgcat_many("searching a monster's ",
                                                mbodypart(mtmp, STOMACH),
                                                " for", NULL));
            }
            if (feeling)
                fbuf = "You feel";
            display_minventory(mtmp, MINV_ALL, msgcat(fbuf, ":"));
        } else {
            pline(msgc_info, "You %s no objects here.", verb);
        }
        return !!feeling;
    }
    if (looked_explicitly && (trap = t_at(level, u.ux, u.uy)) && trap->tseen)
        pline(msgc_info, "There is %s here.", an(trapexplain[trap->ttyp - 1]));

    otmp = level->objects[u.ux][u.uy];
    dfeature = dfeature_at(u.ux, u.uy);
    if (dfeature && !strcmp(dfeature, "pool of water") && Underwater)
        dfeature = NULL;

    if (feeling) {
        boolean drift = Is_airlevel(&u.uz) || Is_waterlevel(&u.uz);

        if (dfeature && !strncmp(dfeature, "altar ", 6)) {
            /* don't say "altar" twice, dfeature has more info */
            pline(msgc_occstart, "You try to feel what is here.");
        } else {
            pline(msgc_occstart, "You try to feel what is %s%s.",
                  drift ? "floating here" : "lying here on the ",
                  drift ? "" : surface(u.ux, u.uy));
        }
        if (dfeature && !drift && !strcmp(dfeature, surface(u.ux, u.uy)))
            dfeature = NULL;    /* ice already identifed */
        if (!can_reach_floor()) {
            /* Don't assume a msg_occstart message was printed (it's a
               reasonable category to turn off); probably "floor" is nicer than
               surface() here */
            pline(msgc_cancelled, "You can't reach the floor!");
            return 0;
        }
    }

    if (dfeature)
        fbuf = msgprintf("There is %s here.", an(dfeature));

    if (!otmp || is_lava(level, u.ux, u.uy) ||
        (is_pool(level, u.ux, u.uy) && !Underwater)) {
        if (dfeature)
            pline(msgc_info, "%s", fbuf);
        read_engr_at(u.ux, u.uy);
        if (looked_explicitly && (feeling || !dfeature))
            pline(msgc_info, "You %s no objects here.", verb);
        return !!feeling;
    }
    /* we know there is something here */

    if (otmp->nexthere && !looked_explicitly) {
        /* multiple objects here, and this is an autopickup command */
        if (dfeature)
            pline(msgc_info, "%s", fbuf);
        read_engr_at(u.ux, u.uy);
        pline(msgc_info, "There are %s%s objects here.",
              (obj_cnt <= 4) ? "a few" :
              (obj_cnt <= 10) ? "several" : "many", picked_some ? " more" : "");
    } else if (!otmp->nexthere) {
        /* only one object */
        if (dfeature)
            pline(msgc_info, "%s", fbuf);
        read_engr_at(u.ux, u.uy);
#ifdef INVISIBLE_OBJECTS
        if (otmp->oinvis && !See_invisible)
            verb = "feel";
#endif
        /* Don't show weight if the player shouldn't know what the weight is. */
        if (show_weight && (objects[otmp->otyp].oc_name_known || otmp->invlet))
            pline(msgc_info, "You %s here %s {%d}.", verb, doname_price(otmp),
                  otmp->owt);
        else
            pline(msgc_info, "You %s here %s.", verb, doname_price(otmp));
        /* This is the same death message as beow, contrary to the normal rules
           for death messages, because petrifying yourself on a cockatrice works
           the same way whether there's one or many items on the square. */
        if (otmp->otyp == CORPSE)
            feel_cockatrice(otmp, feeling, "feeling around for");
    } else {

        init_objmenulist(&objlist);

        if (dfeature) {
            add_objitem(&objlist, MI_TEXT, 0, fbuf, NULL, FALSE);
            add_objitem(&objlist, MI_TEXT, 0, "", NULL, FALSE);
        }

        for (; otmp; otmp = otmp->nexthere) {
            if (otmp->otyp == CORPSE && will_feel_cockatrice(otmp, feeling)) {
                const char *buf;

                felt_cockatrice = TRUE;
                buf = msgcat(doname_price(otmp), "...");
                add_objitem(&objlist, MI_NORMAL, 0, buf, otmp, FALSE);
                break;
            }
            add_objitem(&objlist, MI_NORMAL, 0, doname(otmp), otmp, FALSE);
        }

        display_objects(&objlist, title, PICK_NONE, PLHINT_CONTAINER, NULL);

        if (felt_cockatrice)
            feel_cockatrice(otmp, feeling, "feeling around for");
        read_engr_at(u.ux, u.uy);
    }
    return ! !feeling;
}

/* Explicitly look at what is here, including all objects.  This is called by a
   CMD_NOTIME command, so it should never make any changes to the gamestate. */
int
dolook(const struct nh_cmd_arg *arg)
{
    (void) arg;
    if (look_here(0, FALSE, TRUE, FALSE))
        impossible("dolook wanted to take time");
    return 0;
}

/* Feel around for what's at your feet, as with : when blind formerly. */
int
dofeel(const struct nh_cmd_arg *arg)
{
    (void) arg;
    return look_here(0, FALSE, TRUE, TRUE);
}

boolean
will_feel_cockatrice(struct obj * otmp, boolean force_touch)
{
    if ((Blind || force_touch) && !uarmg && !Stone_resistance &&
        (otmp->otyp == CORPSE && touch_petrifies(&mons[otmp->corpsenm])))
        return TRUE;
    return FALSE;
}

/* Returns TRUE if the action in question was aborted via the stoning
   process. */
boolean
feel_cockatrice(struct obj *otmp, boolean force_touch, const char *verbing)
{
    boolean rv = FALSE;
    if (will_feel_cockatrice(otmp, force_touch)) {
        if (poly_when_stoned(youmonst.data))
            pline(msgc_statusgood,
                  "You touched the %s corpse with your bare %s.",
                  mons[otmp->corpsenm].mname, makeplural(body_part(HAND)));
        else {
            pline(msgc_fatal_predone,
                  "Touching the %s corpse is a fatal mistake...",
                  mons[otmp->corpsenm].mname);
            rv = TRUE;
        }
        instapetrify(killer_msg(STONING,
                                msgcat_many(verbing, " ",
                                            killer_xname(otmp), NULL)));
    }
    return rv;
}

void
stackobj(struct obj *obj)
{
    struct obj *otmp;

    for (otmp = obj->olev->objects[obj->ox][obj->oy]; otmp;
         otmp = otmp->nexthere)
        if (otmp != obj && merged(&obj, &otmp))
            break;
    return;
}

/* returns TRUE if obj & otmp can be merged */
static boolean
mergable(struct obj *otmp, struct obj *obj)
{
    if (obj->otyp != otmp->otyp)
        return FALSE;

    /* coins of the same kind will always merge, even in containers */
    if (obj->oclass == COIN_CLASS)
        return TRUE;

    if (obj->unpaid != otmp->unpaid || obj->spe != otmp->spe ||
        obj->dknown != otmp->dknown || obj->cursed != otmp->cursed ||
        obj->blessed != otmp->blessed || obj->no_charge != otmp->no_charge ||
        obj->obroken != otmp->obroken || obj->otrapped != otmp->otrapped ||
        obj->lamplit != otmp->lamplit ||
#ifdef INVISIBLE_OBJECTS
        obj->oinvis != otmp->oinvis ||
#endif
        obj->greased != otmp->greased || obj->oeroded != otmp->oeroded ||
        obj->oeroded2 != otmp->oeroded2 || obj->bypass != otmp->bypass)
        return FALSE;

    if ((obj->oclass == WEAPON_CLASS || obj->oclass == ARMOR_CLASS) &&
        (obj->oerodeproof != otmp->oerodeproof || obj->rknown != otmp->rknown))
        return FALSE;

    if (obj->oclass == FOOD_CLASS &&
        (obj->oeaten != otmp->oeaten || obj->orotten != otmp->orotten))
        return FALSE;

    if (obj->otyp == CORPSE || obj->otyp == EGG || obj->otyp == TIN) {
        if (obj->corpsenm != otmp->corpsenm)
            return FALSE;
    }

    /* hatching eggs don't merge; ditto for revivable corpses */
    if ((obj->otyp == EGG && (obj->timed || otmp->timed)) ||
        (obj->otyp == CORPSE && otmp->corpsenm >= LOW_PM &&
         is_reviver(&mons[otmp->corpsenm])))
        return FALSE;

    /* allow candle merging only if their ages are close */
    /* see begin_burn() for a reference for the magic "25" */
    if (Is_candle(obj) && obj->age / 25 != otmp->age / 25)
        return FALSE;

    /* burning potions of oil never merge */
    if (obj->otyp == POT_OIL && obj->lamplit)
        return FALSE;

    /* don't merge surcharged item with base-cost item */
    if (obj->unpaid && !same_price(obj, otmp))
        return FALSE;

    /* if they have names, make sure they're the same */
    if ((obj->onamelth != otmp->onamelth) ||
        (obj->onamelth && otmp->onamelth &&
         strncmp(ONAME(obj), ONAME(otmp), (int)obj->onamelth)))
        return FALSE;

    /* for the moment, any additional information is incompatible */
    if (obj->oxlth || otmp->oxlth)
        return FALSE;

    if (obj->oartifact != otmp->oartifact)
        return FALSE;

    if (obj->known == otmp->known || !objects[otmp->otyp].oc_uses_known) {
        return (boolean) (objects[obj->otyp].oc_merge);
    } else
        return FALSE;
}

int
doprgold(const struct nh_cmd_arg *arg)
{
    (void) arg;

    /* the messages used to refer to "carrying gold", but that didn't take
       containers into account */
    long umoney = money_cnt(invent);

    if (!umoney)
        pline(msgc_info, "Your wallet is empty.");
    else
        pline(msgc_info, "Your wallet contains %ld %s.",
              umoney, currency(umoney));

    shopper_financial_report();
    return 0;
}


int
doprwep(const struct nh_cmd_arg *arg)
{
    (void) arg;

    if (!uwep) {
        pline(msgc_info, "You are empty %s.", body_part(HANDED));
    } else {
        prinv(NULL, uwep, 0L);
        if (u.twoweap)
            prinv(NULL, uswapwep, 0L);
    }
    return 0;
}

int
doprarm(const struct nh_cmd_arg *arg)
{
    (void) arg;
    if (!wearing_armor())
        pline(msgc_info, "You are not wearing any armor.");
    else {
        char lets[8];
        int ct = 0;

        if (uarmu)
            lets[ct++] = obj_to_let(uarmu);
        if (uarm)
            lets[ct++] = obj_to_let(uarm);
        if (uarmc)
            lets[ct++] = obj_to_let(uarmc);
        if (uarmh)
            lets[ct++] = obj_to_let(uarmh);
        if (uarms)
            lets[ct++] = obj_to_let(uarms);
        if (uarmg)
            lets[ct++] = obj_to_let(uarmg);
        if (uarmf)
            lets[ct++] = obj_to_let(uarmf);
        lets[ct] = 0;
        display_inventory(lets, FALSE);
    }
    return 0;
}

int
doprring(const struct nh_cmd_arg *arg)
{
    (void) arg;
    if (!uleft && !uright)
        pline(msgc_info, "You are not wearing any rings.");
    else {
        char lets[3];
        int ct = 0;

        if (uleft)
            lets[ct++] = obj_to_let(uleft);
        if (uright)
            lets[ct++] = obj_to_let(uright);
        lets[ct] = 0;
        display_inventory(lets, FALSE);
    }
    return 0;
}

int
dopramulet(const struct nh_cmd_arg *arg)
{
    (void) arg;
    if (!uamul)
        pline(msgc_info, "You are not wearing an amulet.");
    else
        prinv(NULL, uamul, 0L);
    return 0;
}

static boolean
tool_in_use(struct obj *obj)
{
    if ((obj->owornmask & (W_MASK(os_tool) | W_MASK(os_saddle))) != 0L)
        return TRUE;
    if (obj->oclass != TOOL_CLASS)
        return FALSE;
    return (boolean) (obj == uwep || obj->lamplit ||
                      (obj->otyp == LEASH && obj->leashmon));
}

int
doprtool(const struct nh_cmd_arg *arg)
{
    struct obj *otmp;
    int ct = 0;
    char lets[52 + 1];

    (void) arg;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (tool_in_use(otmp))
            lets[ct++] = obj_to_let(otmp);
    lets[ct] = '\0';
    if (!ct)
        pline(msgc_info, "You are not using any tools.");
    else
        display_inventory(lets, FALSE);
    return 0;
}

/* '*' command; combines the ')' + '[' + '=' + '"' + '(' commands;
   show inventory of all currently wielded, worn, or used objects */
int
doprinuse(const struct nh_cmd_arg *arg)
{
    struct obj *otmp;
    int ct = 0;
    char lets[52 + 1];

    (void) arg;

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (is_worn(otmp) || tool_in_use(otmp))
            lets[ct++] = obj_to_let(otmp);
    lets[ct] = '\0';
    if (!ct)
        pline(msgc_info, "You are not wearing or wielding anything.");
    else
        display_inventory(lets, FALSE);
    return 0;
}

/*
 * uses up an object that's on the floor, charging for it as necessary
 */
void
useupf(struct obj *obj, long numused)
{
    struct obj *otmp;
    boolean at_u = (obj->ox == u.ux && obj->oy == u.uy);

    /* burn_floor_paper() keeps an object pointer that it tries to useupf()
       multiple times, so obj must survive if plural */
    if (obj->quan > numused)
        otmp = splitobj(obj, numused);
    else
        otmp = obj;
    if (costly_spot(otmp->ox, otmp->oy)) {
        if (strchr(u.urooms, *in_rooms(level, otmp->ox, otmp->oy, 0)))
            addtobill(otmp, FALSE, FALSE, FALSE);
        else
            stolen_value(otmp, otmp->ox, otmp->oy, FALSE, FALSE);
    }
    delobj(otmp);
    if (at_u && u.uundetected && hides_under(youmonst.data)) {
        u.uundetected = OBJ_AT(u.ux, u.uy);
        newsym(u.ux, u.uy);
    }
}


/*
 * Conversion from a class to a string for printing.
 * This must match the object class order.
 */
static const char *const names[] = { 0,
    "Illegal objects", "Weapons", "Armor", "Rings", "Amulets",
    "Tools", "Comestibles", "Potions", "Scrolls", "Spellbooks",
    "Wands", "Coins", "Gems", "Boulders/Statues", "Iron balls",
    "Chains", "Venoms"
};

static const char oth_symbols[] = {
    CONTAINED_SYM,
    '\0'
};

static const char *const oth_names[] = {
    "Bagged/Boxed items"
};

const char *
let_to_name(char let, boolean unpaid)
{
    const char *class_name;
    const char *pos;
    int oclass = (let >= 1 && let < MAXOCLASSES) ? let : 0;

    if (oclass)
        class_name = names[oclass];
    else if ((pos = strchr(oth_symbols, let)) != 0)
        class_name = oth_names[pos - oth_symbols];
    else
        class_name = names[0];

    return unpaid ? msgcat("Unpaid ", class_name) : class_name;
}


/* all but coins */
static const char organizable[] = {
    ALLOW_COUNT, SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS,
    AMULET_CLASS, GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS,
    WEAPON_CLASS, ROCK_CLASS, CHAIN_CLASS, BALL_CLASS, FOOD_CLASS, 0
};

int
doorganize(const struct nh_cmd_arg *arg)
{       /* inventory organizer by Del Lamb */
    struct obj *obj, *otmp;
    int ix, cur;
    char let;
    char alphabet[52 + 1], buf[52 + 1];
    const char *qbuf, *cbuf;
    const char *adj_type;

    /* get a pointer to the object the user wants to organize; this can split
       the stack if the user specifies a count */
    if (!(obj = getargobj(arg, organizable, "adjust")))
        return 0;

    /* initialize the list with all upper and lower case letters */
    for (let = 'a', ix = 0; let <= 'z';)
        alphabet[ix++] = let++;
    for (let = 'A', ix = 26; let <= 'Z';)
        alphabet[ix++] = let++;
    alphabet[52] = 0;

    /* blank out all the letters currently in use in the inventory */
    /* except those that will be merged with the selected object */
    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (otmp != obj && !mergable(otmp, obj)) {
            if (otmp->invlet == '$')
                continue;       /* can't adjust to or from $ (gold) */
            else if (otmp->invlet <= 'Z')
                alphabet[(otmp->invlet) - 'A' + 26] = ' ';
            else
                alphabet[(otmp->invlet) - 'a'] = ' ';
        }

    /* compact the list by removing all the blanks */
    for (ix = cur = 0; ix <= 52; ix++)
        if (alphabet[ix] != ' ')
            buf[cur++] = alphabet[ix];

    /* and by dashing runs of letters */
    if (cur > 5)
        cbuf = compactify(buf);
    else
        cbuf = buf;

    /* get new letter to use as inventory letter */
    for (;;) {
        qbuf = msgprintf("Adjust letter to what [%s]?", cbuf);
        let = query_key(qbuf, NQKF_LETTER_REASSIGNMENT, NULL);
        if (strchr(quitchars, let)) {
            pline(msgc_cancelled, "Never mind.");
            goto cleansplit;
        }
        if (let == '@' || !letter(let))
            pline(msgc_uiprompt, "Select an inventory slot letter.");
        else
            break;
    }

    /* change the inventory and print the resulting item */
    boolean prtmp = FALSE;

    if (let == obj->invlet) {
        otmp = obj;
    } else {
        for (otmp = invent; otmp && (otmp == obj || otmp->invlet != let);
             otmp = otmp->nobj) {}
    }

    if (!otmp)
        adj_type = "Moving:";
    else if (otmp == obj) {
        adj_type = "Merging:";
        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (obj != otmp && mergable(otmp, obj)) {
                extract_nobj(obj, &invent, &turnstate.floating_objects,
                             OBJ_FREE);
                merged(&otmp, &obj);
                obj = otmp;
            }
        }
    } else if (mergable(otmp, obj)) {
        adj_type = "Merging:";
        extract_nobj(obj, &invent, &turnstate.floating_objects, OBJ_FREE);
        merged(&otmp, &obj);
        obj = otmp;
    } else {
        struct obj *otmp2;

        for (otmp2 = invent;
             otmp2 && (otmp2 == obj || otmp2->invlet != obj->invlet);
             otmp2 = otmp2->nobj) {}

        if (otmp2) {
            char oldlet = obj->invlet;

            adj_type = "Displacing:";

            assigninvlet(obj);

            if (obj->invlet == NOINVSYM) {
                pline(msgc_cancelled, "There's nowhere to put that.");
                obj->invlet = oldlet;
                goto cleansplit;
            }
        } else
            adj_type = "Swapping:";
        otmp->invlet = obj->invlet;
        prtmp = TRUE;
    }

    /* inline addinv (assuming !merged) */
    /* don't use freeinv/addinv to avoid double-touching artifacts, dousing
       lamps, losing luck, cursing loadstone, etc. */
    extract_nobj(obj, &invent, &turnstate.floating_objects, OBJ_FREE);
    obj->invlet = let;
    extract_nobj(obj, &turnstate.floating_objects, &invent, OBJ_INVENT);
    reorder_invent();

    prinv(adj_type, obj, 0L);
    if (prtmp)
        prinv(NULL, otmp, 0L);
    update_inventory();
    return 0;

cleansplit:
    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (otmp != obj && otmp->invlet == obj->invlet)
            merged(&otmp, &obj);

    return 0;
}


/* common to display_minventory and display_cinventory */
static void
invdisp_nothing(const char *hdr, const char *txt)
{
    struct nh_menuitem items[3];

    set_menuitem(&items[0], 0, MI_HEADING, hdr, 0, FALSE);
    set_menuitem(&items[1], 0, MI_NORMAL, "", 0, FALSE);
    set_menuitem(&items[2], 0, MI_NORMAL, txt, 0, FALSE);

    display_menu(&(struct nh_menulist){.items = items, .icount = 3},
                 NULL, PICK_NONE, PLHINT_INVENTORY, NULL);
}

/* query_objlist callback: return things that could possibly be worn/wielded */
static boolean
worn_wield_only(const struct obj *obj)
{
    return (obj->oclass == WEAPON_CLASS || obj->oclass == ARMOR_CLASS ||
            obj->oclass == AMULET_CLASS || obj->oclass == RING_CLASS ||
            obj->oclass == TOOL_CLASS);
}

/*
 * Display a monster's inventory.
 * Returns a pointer to the object from the monster's inventory selected
 * or NULL if nothing was selected.
 *
 * By default, only worn and wielded items are displayed.  The caller
 * can pick one.  Modifier flags are:
 *
 *      MINV_NOLET      - nothing selectable
 *      MINV_ALL        - display all inventory
 */
struct obj *
display_minventory(struct monst *mon, int dflags, const char *title)
{
    struct obj *ret;
    const char *qbuf;
    int n;
    struct object_pick *selected = NULL;
    int do_all = (dflags & MINV_ALL) != 0;

    qbuf = msgprintf("%s %s:", s_suffix(noit_Monnam(mon)),
                     do_all ? "possessions" : "armament");

    if (do_all ? (mon->minvent != 0)
        : (mon->misc_worn_check || MON_WEP(mon))) {
        /* Fool the 'weapon in hand' routine into displaying 'weapon in claw',
           etc. properly. */
        youmonst.data = mon->data;

        n = query_objlist(title ? title : qbuf, mon->minvent, INVORDER_SORT,
                          &selected,
                          (dflags & MINV_NOLET) ? PICK_NONE : PICK_ONE,
                          do_all ? allow_all : worn_wield_only);

        set_uasmon();
    } else {
        invdisp_nothing(title ? title : qbuf, "(none)");
        n = 0;
    }

    if (n > 0) {
        ret = selected[0].obj;
        free(selected);
    } else
        ret = NULL;
    return ret;
}

/*
 * Display the contents of a container in inventory style.
 * Currently, this is only used for statues, via wand of probing.
 */
struct obj *
display_cinventory(struct obj *obj)
{
    struct obj *ret = NULL;
    const char *qbuf = msgprintf("Contents of %s:", doname(obj));
    int n;
    struct object_pick *selected = 0;

    if (obj->cobj) {
        n = query_objlist(qbuf, obj->cobj, INVORDER_SORT, &selected,
                          PICK_NONE, allow_all);
    } else {
        invdisp_nothing(qbuf, "(empty)");
        n = 0;
    }

    if (n > 0) {
        ret = selected[0].obj;
        free(selected);
    }

    return ret;
}

/* query objlist callback: return TRUE if obj is at given location */
static coord only;

static boolean
only_here(const struct obj *obj)
{
    return obj->ox == only.x && obj->oy == only.y;
}

/*
 * Display a list of buried items in inventory style.  Return a non-zero
 * value if there were items at that spot.
 *
 * Currently, this is only used with a wand of probing zapped downwards.
 */
int
display_binventory(int x, int y, boolean as_if_seen)
{
    struct obj *obj;
    struct object_pick *selected = NULL;
    int n;

    /* count # of objects here */
    for (n = 0, obj = level->buriedobjlist; obj; obj = obj->nobj)
        if (obj->ox == x && obj->oy == y) {
            if (as_if_seen)
                obj->dknown = 1;
            n++;
        }

    if (n) {
        only.x = x;
        only.y = y;
        if (query_objlist
            ("Things that are buried here:", level->buriedobjlist,
             INVORDER_SORT, &selected, PICK_NONE, only_here) > 0)
            free(selected);
        only.x = only.y = 0;
    }
    return n;
}

/*invent.c*/

