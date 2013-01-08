/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#define NOINVSYM        '#'
#define CONTAINED_SYM   '>'     /* designator for inside a container */

static boolean mergable(struct obj *, struct obj *);
static void invdisp_nothing(const char *, const char *);
static boolean worn_wield_only(const struct obj *);
static boolean only_here(const struct obj *);
static void compactify(char *);
static boolean taking_off(const char *);
static boolean putting_on(const char *);
static char display_pickinv(const char *, boolean, long *);
static boolean this_type_only(const struct obj *);
static void dounpaid(void);
static struct obj *find_unpaid(struct obj *, struct obj **);
static void menu_identify(int);
static boolean tool_in_use(struct obj *);
static char obj_to_let(struct obj *);
static int identify(struct obj *);
static const char *dfeature_at(int, int, char *);


enum obj_use_status {
    OBJECT_USABLE,
    ALREADY_IN_USE,
    UNSUITABLE_USE,
    CURRENTLY_NOT_USABLE
};


/* wizards can wish for venom, which will become an invisible inventory
 * item without this.  putting it in inv_order would mean venom would
 * suddenly become a choice for all the inventory-class commands, which
 * would probably cause mass confusion.  the test for inventory venom
 * is not wizard because the wizard can leave venom lying
 * around on a bones level for normal players to find.
 */
static const char venom_inv[] = { VENOM_CLASS, 0 };     /* (constant) */

void
assigninvlet(struct obj *otmp)
{
    boolean inuse[52];
    int i;
    struct obj *obj;

    /* There is only one of these in inventory... */
    if (otmp->oclass == COIN_CLASS) {
        otmp->invlet = GOLD_SYM;
        return;
    }

    for (i = 0; i < 52; i++)
        inuse[i] = FALSE;
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
    if ((i = otmp->invlet) &&
        (('a' <= i && i <= 'z') || ('A' <= i && i <= 'Z')))
        return;
    for (i = lastinvnr + 1; i != lastinvnr; i++) {
        if (i == 52) {
            i = -1;
            continue;
        }
        if (!inuse[i])
            break;
    }
    otmp->invlet =
        (inuse[i] ? NOINVSYM : (i < 26) ? ('a' + i) : ('A' + i - 26));
    lastinvnr = i;
}


/* note: assumes ASCII; toggling a bit puts lowercase in front of uppercase */
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
            if (wmask & W_WEP)
                wmask = W_WEP;
            else if (wmask & W_SWAPWEP)
                wmask = W_SWAPWEP;
            else if (wmask & W_QUIVER)
                wmask = W_QUIVER;
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
inventory.  Called _before_ the object has been added to the hero's
inventory.

This is called when adding objects to the hero's inventory normally (via
addinv) or when an object in the hero's inventory has been polymorphed
in-place.

It may be valid to merge this code with with addinv_core2().
*/
void
addinv_core1(struct obj *obj)
{
    if (obj->oclass == COIN_CLASS) {
        iflags.botl = 1;
    } else if (obj->otyp == AMULET_OF_YENDOR) {
        if (u.uhave.amulet)
            impossible("already have amulet?");
        u.uhave.amulet = 1;
        historic_event(!obj->known, "gained the Amulet of Yendor!");
    } else if (obj->otyp == CANDELABRUM_OF_INVOCATION) {
        if (u.uhave.menorah)
            impossible("already have candelabrum?");
        u.uhave.menorah = 1;
    } else if (obj->otyp == BELL_OF_OPENING) {
        if (u.uhave.bell)
            impossible("already have silver bell?");
        u.uhave.bell = 1;
    } else if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
        if (u.uhave.book)
            impossible("already have the book?");
        u.uhave.book = 1;
    } else if (obj->oartifact) {
        if (is_quest_artifact(obj)) {
            if (u.uhave.questart)
                impossible("already have quest artifact?");
            u.uhave.questart = 1;
            artitouch();
        }
        set_artifact_intrinsic(obj, 1, W_ART);
    }
}

/*
Adjust hero intrinsics as if this object was being added to the hero's
inventory.  Called _after_ the object has been added to the hero's
inventory.

This is called when adding objects to the hero's inventory normally (via
addinv) or when an object in the hero's inventory has been polymorphed
in-place.
*/
void
addinv_core2(struct obj *obj)
{
    if (confers_luck(obj)) {
        /* new luckstone must be in inventory by this point for correct
           calculation */
        set_moreluck();
    }
}

/*
Add obj to the hero's inventory.  Make sure the object is "free".
Adjust hero attributes as necessary.
*/
struct obj *
addinv(struct obj *obj)
{
    struct obj *otmp;

    if (obj->where != OBJ_FREE)
        panic("addinv: obj not free");
    obj->no_charge = 0; /* not meaningful for invent */
    obj->was_thrown = 0;

    examine_object(obj);
    addinv_core1(obj);

    /* merge if possible; find end of chain in the process */
    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (merged(&otmp, &obj)) {
            obj = otmp;
            goto added;
        }

    /* didn't merge, so insert into chain */
    assigninvlet(obj);
    obj->nobj = invent; /* insert at beginning */
    invent = obj;
    reorder_invent();
    obj->where = OBJ_INVENT;

added:
    addinv_core2(obj);
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

/* Add an item to the inventory unless we're fumbling or it refuses to be
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
    char buf[BUFSZ];

    if (!Blind)
        obj->dknown = 1;        /* maximize mergibility */
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
                pline(drop_fmt, drop_arg);
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
            pline(drop_fmt, drop_arg);
        dropy(obj);
    } else {
        long oquan = obj->quan;
        int prev_encumbr = near_capacity();     /* before addinv() */

        /* encumbrance only matters if it would now become worse than max(
           current_value, stressed ) */
        if (prev_encumbr < MOD_ENCUMBER)
            prev_encumbr = MOD_ENCUMBER;
        /* addinv() may redraw the entire inventory, overwriting drop_arg when
           it comes from something like doname() */
        if (drop_arg)
            drop_arg = strcpy(buf, drop_arg);

        obj = addinv(obj);
        if (obj->invlet == NOINVSYM || ((obj->otyp != LOADSTONE || !obj->cursed)
                               && near_capacity() > prev_encumbr)) {
            if (drop_fmt)
                pline(drop_fmt, drop_arg);
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
void
freeinv_core(struct obj *obj)
{
    if (obj->oclass == COIN_CLASS) {
        iflags.botl = 1;
        return;
    } else if (obj->otyp == AMULET_OF_YENDOR) {
        if (!u.uhave.amulet)
            impossible("don't have amulet?");
        u.uhave.amulet = 0;
        /* Minor information leak about the Amulet of Yendor (vs fakes). You
           don't get any more info than you do by turning on show_uncursed
           though. */
        historic_event(!obj->known, "lost the Amulet of Yendor.");
    } else if (obj->otyp == CANDELABRUM_OF_INVOCATION) {
        if (!u.uhave.menorah)
            impossible("don't have candelabrum?");
        u.uhave.menorah = 0;
    } else if (obj->otyp == BELL_OF_OPENING) {
        if (!u.uhave.bell)
            impossible("don't have silver bell?");
        u.uhave.bell = 0;
    } else if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
        if (!u.uhave.book)
            impossible("don't have the book?");
        u.uhave.book = 0;
    } else if (obj->oartifact) {
        if (is_quest_artifact(obj)) {
            if (!u.uhave.questart)
                impossible("don't have quest artifact?");
            u.uhave.questart = 0;
        }
        set_artifact_intrinsic(obj, 0, W_ART);
    }

    if (obj->otyp == LOADSTONE) {
        curse(obj);
    } else if (confers_luck(obj)) {
        set_moreluck();
        iflags.botl = 1;
    } else if (obj->otyp == FIGURINE && obj->timed) {
        stop_timer(obj->olev, FIG_TRANSFORM, obj);
    }
}

/* remove an object from the hero's inventory */
void
freeinv(struct obj *obj)
{
    extract_nobj(obj, &invent);
    if (uwep == obj)
        setuwep(NULL);
    if (uswapwep == obj)
        setuswapwep(NULL);
    freeinv_core(obj);
    update_inventory();
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


/* destroy object in level->objlist chain (if unpaid, it remains on the bill) */
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
    if (update_map)
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
obj_here(struct obj * obj, int x, int y)
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


/* compact a string of inventory letters by dashing runs of letters */
static void
compactify(char *buf)
{
    int i1 = 0, i2 = 0;
    char ilet, ilet1, ilet2;

    ilet2 = buf[0];
    ilet1 = buf[1];
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
}

/* match the prompt for either 'T' or 'R' command */
static boolean
taking_off(const char *action)
{
    return !strcmp(action, "take off") || !strcmp(action, "remove");
}

/* match the prompt for either 'W' or 'P' command */
static boolean
putting_on(const char *action)
{
    return !strcmp(action, "wear") || !strcmp(action, "put on");
}


/* ugly checks that were pulled out of getobj. The intent is to determine
 * whether object otmp should be displayed for the query descibed by word.
 * This function assumes that otmp->oclass is known to be allowed. */
static enum obj_use_status
object_selection_checks(struct obj *otmp, const char *word)
{
    long dummymask;
    int otyp = otmp->otyp;

    /* ugly check: remove inappropriate things */
    if ((taking_off(word) &&
         (!(otmp->owornmask & (W_ARMOR | W_RING | W_AMUL | W_TOOL))
          || (otmp == uarm && uarmc) || (otmp == uarmu && (uarm || uarmc))))
        || (putting_on(word) && (otmp->owornmask & /* already worn */
                                 (W_ARMOR | W_RING | W_AMUL | W_TOOL)))
        ||(!strcmp(word, "ready") &&
           (otmp == uwep || (otmp == uswapwep && u.twoweap))))
        return ALREADY_IN_USE;

    /* Second ugly check; unlike the first it won't trigger an "else" in "you
       don't have anything else to ___". */
    else if ((putting_on(word) &&
              ((otmp->oclass == FOOD_CLASS && otmp->otyp != MEAT_RING) ||
               (otmp->oclass == TOOL_CLASS && otyp != BLINDFOLD && otyp != TOWEL
                && otyp != LENSES)))
             || (!strcmp(word, "wield") &&
                 (otmp->oclass == TOOL_CLASS && !is_weptool(otmp)))
             || (!strcmp(word, "eat") && !is_edible(otmp))
             || (!strcmp(word, "sacrifice") &&
                 (otyp != CORPSE && otyp != AMULET_OF_YENDOR &&
                  otyp != FAKE_AMULET_OF_YENDOR))
             || (!strcmp(word, "write with") &&
                 (otmp->oclass == TOOL_CLASS && otyp != MAGIC_MARKER &&
                  otyp != TOWEL))
             || (!strcmp(word, "tin") && (otyp != CORPSE || !tinnable(otmp)))
             || (!strcmp(word, "rub") &&
                 ((otmp->oclass == TOOL_CLASS && otyp != OIL_LAMP &&
                   otyp != MAGIC_LAMP && otyp != BRASS_LANTERN) ||
                  (otmp->oclass == GEM_CLASS && !is_graystone(otmp))))
             || (!strncmp(word, "rub on the stone", 16) &&
                 otmp->oclass == GEM_CLASS && /* using known touchstone */
                 otmp->dknown && objects[otyp].oc_name_known)
             || ((!strcmp(word, "use or apply") || !strcmp(word, "untrap with"))
                 &&
                 /* Picks, axes, pole-weapons, bullwhips */
                 ((otmp->oclass == WEAPON_CLASS && !is_pick(otmp) &&
                   !is_axe(otmp) && !is_pole(otmp) && otyp != BULLWHIP) ||
                  (otmp->oclass == POTION_CLASS &&
                   /* only applicable potion is oil, and it will only be
                      offered as a choice when already discovered */
                   (otyp != POT_OIL || !otmp->dknown ||
                    !objects[POT_OIL].oc_name_known)) ||
                  (otmp->oclass == FOOD_CLASS && otyp != CREAM_PIE &&
                   otyp != EUCALYPTUS_LEAF) || (otmp->oclass == GEM_CLASS &&
                                                !is_graystone(otmp))))
             || (!strncmp(word, "invoke", 6) &&
                 (!otmp->oartifact && !objects[otyp].oc_unique &&
                  (otyp != FAKE_AMULET_OF_YENDOR || otmp->known) &&
                  otmp->oclass != WAND_CLASS &&   /* V for breaking wands */
                  ((otmp->oclass == TOOL_CLASS && /* V for rubbing */
                    otyp != OIL_LAMP && otyp != MAGIC_LAMP &&
                    otyp != BRASS_LANTERN) ||
                   (otmp->oclass == GEM_CLASS && !is_graystone(otmp)) ||
                   (otmp->oclass != TOOL_CLASS && otmp->oclass != GEM_CLASS)) &&
                  otyp != CRYSTAL_BALL &&         /* V for applying */
                  /* note: presenting the possibility of invoking non-artifact
                     mirrors and/or lamps is a simply a cruel deception... */
                  otyp != MIRROR && otyp != MAGIC_LAMP &&
                  (otyp != OIL_LAMP || /* don't list known oil lamp */
                   (otmp->dknown && objects[OIL_LAMP].oc_name_known))))
             || (!strcmp(word, "untrap with") &&
                 (otmp->oclass == TOOL_CLASS && otyp != CAN_OF_GREASE))
             || (!strcmp(word, "charge") && !is_chargeable(otmp))
             || (otmp->oclass == COIN_CLASS && !strcmp(word, "eat") &&
                 (!metallivorous(youmonst.data) ||
                  youmonst.data == &mons[PM_RUST_MONSTER])))
        return UNSUITABLE_USE;

    /* ugly check for unworn armor that can't be worn */
    else if (putting_on(word) && otmp->oclass == ARMOR_CLASS &&
             !canwearobj(otmp, &dummymask, FALSE))
        return CURRENTLY_NOT_USABLE;

    return OBJECT_USABLE;
}


/*
 * getobj returns:
 *      struct obj *xxx:  object to do something with.
 *      NULL              error return: no object.
 *      &zeroobj          explicitly no object (as in w-).
!!!! test if gold can be used in unusual ways (eaten etc.)
 */
struct obj *
getobj(const char *let, const char *word)
{
    struct obj *otmp;
    char ilet;
    char buf[BUFSZ], qbuf[QBUFSZ];
    char lets[BUFSZ], altlets[BUFSZ], *ap;
    int foo = 0;
    char *bp = buf;
    xchar allowcnt = 0; /* 0, 1 or 2 */
    boolean allowall = FALSE;
    boolean allownone = FALSE;
    char nonechar = '-';
    boolean useboulder = FALSE;
    xchar foox = 0;
    int cnt;
    boolean prezero = FALSE;

    if (*let == ALLOW_COUNT)
        let++, allowcnt = 1;
    if (*let == ALL_CLASSES)
        let++, allowall = TRUE;
    if (*let == ALLOW_NONE)
        let++, allownone = TRUE;
    if (*let == NONE_ON_COMMA)
        let++, nonechar = ',';
    /* "ugly check" for reading fortune cookies, part 1 */
    /* The normal 'ugly check' keeps the object on the inventory list. We don't 
       want to do that for shirts/cookies, so the check for them is handled a
       bit differently (and also requires that we set allowall in the caller) */
    if (allowall && !strcmp(word, "read"))
        allowall = FALSE;

    /* another ugly check: show boulders (not statues) */
    if (*let == WEAPON_CLASS && !strcmp(word, "throw") &&
        throws_rocks(youmonst.data))
        useboulder = TRUE;

    if (allownone)
        *bp++ = nonechar;
    if (bp > buf && bp[-1] == nonechar)
        *bp++ = ' ';
    ap = altlets;

    ilet = 'a';
    for (otmp = invent; otmp; otmp = otmp->nobj) {
        if (!*let || strchr(let, otmp->oclass)
            || (useboulder && otmp->otyp == BOULDER)) {
            bp[foo++] = otmp->invlet;

            switch (object_selection_checks(otmp, word)) {
            case ALREADY_IN_USE:       /* eg: wield the weapon in your hands */
                foo--;
                foox++;
                break;

            case UNSUITABLE_USE:       /* eg: putting on a tool */
                foo--;
                break;

            case CURRENTLY_NOT_USABLE:
                foo--;
                allowall = TRUE;
                *ap++ = otmp->invlet;
                break;

            default:
                break;
            }

        } else {

            /* "ugly check" for reading fortune cookies, part 2 */
            if ((!strcmp(word, "read") &&
                 (otmp->otyp == FORTUNE_COOKIE || otmp->otyp == T_SHIRT)))
                allowall = TRUE;
        }

        if (ilet == 'z')
            ilet = 'A';
        else
            ilet++;
    }
    bp[foo] = 0;
    if (foo == 0 && bp > buf && bp[-1] == ' ')
        *--bp = 0;
    strcpy(lets, bp);   /* necessary since we destroy buf */
    if (foo > 5)        /* compactify string */
        compactify(bp);
    *ap = '\0';

    /* if (!strncmp(word, "rub on the stone", 16)) { allowall = TRUE; usegold = 
       TRUE; } */

    if (!foo && !allowall && !allownone) {
        pline("You don't have anything %sto %s.", foox ? "else " : "", word);
        return NULL;
    }
    for (;;) {
        cnt = 0;
        if (allowcnt == 2)
            allowcnt = 1;       /* abort previous count */
        if (!buf[0]) {
            sprintf(qbuf, "What do you want to %s? [*]", word);
        } else {
            sprintf(qbuf, "What do you want to %s? [%s or ?*]", word, buf);
        }
        ilet = query_key(qbuf, allowcnt ? &cnt : NULL);
        if (allowcnt == 1 && cnt != -1) {
            allowcnt = 2;       /* signal presence of cnt */
            if (cnt == 0)
                prezero = TRUE; /* cnt was explicitly set to 0 */
        }
        if (cnt == -1)
            cnt = 0;

        if (strchr(quitchars, ilet)) {
            if (flags.verbose)
                pline("Never mind.");
            return NULL;
        }
        if (ilet == nonechar) {
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
                pline
                    ("The LRS would be very interested to know you have that much.");
                return NULL;
            }
        }
        if (ilet == '?' || ilet == '*') {
            char *allowed_choices = (ilet == '?') ? lets : NULL;
            long ctmp = 0;

            if (ilet == '?' && !*lets && *altlets)
                allowed_choices = altlets;
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
                if (flags.verbose)
                    pline("Never mind.");
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
            if (cnt > 1) {
                pline("You can only throw one item at a time.");
                continue;
            }
        }
        iflags.botl = 1;        /* May have changed the amount of money */

        for (otmp = invent; otmp; otmp = otmp->nobj)
            if (otmp->invlet == ilet)
                break;
        if (!otmp) {
            pline("You don't have that object.");
            continue;
        } else if (cnt < 0 || otmp->quan < cnt) {
            pline("You don't have that many!  You have only %ld.", otmp->quan);
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
            /* don't split a stack of cursed loadstones */
            if (otmp->otyp == LOADSTONE && otmp->cursed)
                /* kludge for canletgo()'s can't-drop-this message */
                otmp->corpsenm = (int)cnt;
            else
                otmp = splitobj(otmp, cnt);
        }
    }
    return otmp;
}


boolean
validate_object(struct obj * obj, const char *lets, const char *word)
{
    boolean allowall = !lets || ! !strchr(lets, ALL_CLASSES);

    if (!allowall && lets && !strchr(lets, obj->oclass)) {
        silly_thing(word, obj);
        return FALSE;
    }

    switch (object_selection_checks(obj, word)) {
    default:
    case ALREADY_IN_USE:
    case UNSUITABLE_USE:
        silly_thing(word, obj);
        return FALSE;

    case OBJECT_USABLE:
    case CURRENTLY_NOT_USABLE:
        return TRUE;
    }
}


void
silly_thing(const char *word, struct obj *otmp)
{
    const char *s1, *s2, *s3, *what;
    int ocls = otmp->oclass, otyp = otmp->otyp;

    s1 = s2 = s3 = 0;
    /* check for attempted use of accessory commands ('P','R') on armor and for 
       corresponding armor commands ('W','T') on accessories */
    if (ocls == ARMOR_CLASS) {
        if (!strcmp(word, "put on"))
            s1 = "W", s2 = "wear", s3 = "";
        else if (!strcmp(word, "remove"))
            s1 = "T", s2 = "take", s3 = " off";
    } else if ((ocls == RING_CLASS || otyp == MEAT_RING) || ocls == AMULET_CLASS
               || (otyp == BLINDFOLD || otyp == TOWEL || otyp == LENSES)) {
        if (!strcmp(word, "wear"))
            s1 = "P", s2 = "put", s3 = " on";
        else if (!strcmp(word, "take off"))
            s1 = "R", s2 = "remove", s3 = "";
    }
    if (s1) {
        what = "that";
        /* quantity for armor and accessory objects is always 1, but some
           things should be referred to as plural */
        if (otyp == LENSES || is_gloves(otmp) || is_boots(otmp))
            what = "those";
        pline("Use the '%s' command to %s %s%s.", s1, s2, what, s3);
    } else {
        pline("That is a silly thing to %s.", word);
    }
}


boolean
wearing_armor(void)
{
    return ((boolean)
            (uarm || uarmc || uarmf || uarmg || uarmh || uarms || uarmu));
}

boolean
is_worn(const struct obj * otmp)
{
    return ((boolean)
            (! !
             (otmp->
              owornmask & (W_ARMOR | W_RING | W_AMUL | W_TOOL | W_SADDLE | W_WEP
                           | W_SWAPWEP | W_QUIVER))));
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
    char buf[BUFSZ];

    /* assumptions: id_limit > 0 and at least one unID'd item is present */

    while (id_limit) {
        sprintf(buf, "What would you like to identify %s?",
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
                pline("That was all.");
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
        pline("You have already identified all of your possessions.");
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
    pline("%s%s%s", prefix, *prefix ? " " : "",
          xprname(obj, NULL, obj_to_let(obj), TRUE, 0L, quan));
}



char *xprname(struct obj *obj, const char *txt, /* text to print instead of obj 
                                                 */
              char let, /* inventory letter */
              boolean dot,      /* append period; (dot && cost => Iu) */
              long cost,        /* cost (for inventory of unpaid or expended
                                   items) */
              long quan) {      /* if non-0, print this quantity, not obj->quan 
                                 */
    static char li[BUFSZ];
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
        sprintf(li, "%c - %-45s %6ld %s",
                (dot &&
                 use_invlet ? obj->invlet : let), (txt ? txt : doname(obj)),
                cost, currency(cost));
    } else {
        /* ordinary inventory display or pickup message */
        sprintf(li, "%c - %s%s", (use_invlet ? obj->invlet : let),
                (txt ? txt : doname(obj)), (dot ? "." : ""));
    }
    if (savequan)
        obj->quan = savequan;

    return li;
}



/* the 'i' command */
int
ddoinv(void)
{
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


static struct nh_objitem *
make_invlist(const char *lets, int *icount)
{
    struct obj *otmp;
    char ilet;
    int nr_items = 10, cur_entry = 0, classcount;
    const char *invlet = flags.inv_order;
    struct nh_objitem *items = malloc(nr_items * sizeof (struct nh_objitem));

nextclass:
    classcount = 0;
    for (otmp = invent; otmp; otmp = otmp->nobj) {
        ilet = otmp->invlet;
        if (!lets || !*lets || strchr(lets, ilet)) {
            if (!flags.sortpack || otmp->oclass == *invlet) {
                if (flags.sortpack && !classcount) {
                    add_objitem(&items, &nr_items, MI_HEADING, cur_entry++, 0,
                                let_to_name(*invlet, FALSE), otmp, FALSE);
                    classcount++;
                }
                examine_object(otmp);
                add_objitem(&items, &nr_items, MI_NORMAL, cur_entry++, ilet,
                            doname(otmp), otmp, TRUE);
            }
        }
    }
    if (flags.sortpack) {
        if (*++invlet)
            goto nextclass;
        if (--invlet != venom_inv) {
            invlet = venom_inv;
            goto nextclass;
        }
    }

    *icount = cur_entry;
    return items;
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
    int icount = 0;
    struct nh_objitem *items;
    struct nh_objresult *selected = NULL;

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
        pline("Not carrying anything.");
        return 0;
    }

    if (lets && strlen(lets) == 1) {
        /* when only one item of interest, use pline instead of menus */
        ret = '\0';
        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (otmp->invlet == lets[0]) {
                pline("%s", xprname(otmp, NULL, lets[0], TRUE, 0L, 0L));
                if (out_cnt)
                    *out_cnt = -1L;     /* select all */
                break;
            }
        }
        return ret;
    }

    items = make_invlist(lets, &icount);

    if (icount) {
        selected = malloc(icount * sizeof (struct nh_objresult));
        n = display_objects(items, icount, want_reply ? NULL : "Inventory:",
                            want_reply ? PICK_ONE : PICK_NONE, PLHINT_INVENTORY,
                            selected);
    }
    if (n > 0) {
        ret = (char)selected[0].id;
        if (out_cnt)
            *out_cnt = selected[0].count;
    } else
        ret = !n ? '\0' : '\033';       /* cancelled */

    free(selected);
    free(items);
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
    int icount = 0;
    struct nh_objitem *items;

    if (!windowprocs.win_list_items || program_state.restoring)
        return;

    items = make_invlist(NULL, &icount);
    win_list_items(items, icount, TRUE);
    free(items);

    return;
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
    struct menulist menu;

    count = count_unpaid(invent);

    if (count == 1) {
        marker = NULL;
        otmp = find_unpaid(invent, &marker);

        /* see if the unpaid item is in the top level inventory */
        for (marker = invent; marker; marker = marker->nobj)
            if (marker == otmp)
                break;

        pline("%s",
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

    display_menu(menu.items, menu.icount, NULL, PICK_NONE, PLHINT_INVENTORY,
                 NULL);
    free(menu.items);
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
dotypeinv(void)
{
    char c = '\0';
    int n, i = 0;
    int unpaid_count;
    boolean billx = *u.ushops && doinvbill(0);
    int pick_list[30];
    struct object_pick *dummy;
    const char *prompt = "What type of object do you want an inventory of?";

    if (!invent && !billx) {
        pline("You aren't carrying anything.");
        return 0;
    }
    unpaid_count = count_unpaid(invent);

    i = UNPAID_TYPES;
    if (billx)
        i |= BILLED_TYPES;
    n = query_category(prompt, invent, i, pick_list, PICK_ONE);
    if (!n)
        return 0;
    this_type = c = pick_list[0];

    if (c == 'x') {
        if (billx)
            doinvbill(1);
        else
            pline("No used-up objects on your shopping bill.");
        return 0;
    }
    if (c == 'u') {
        if (unpaid_count)
            dounpaid();
        else
            pline("You are not carrying any unpaid objects.");
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
const char *
dfeature_at(int x, int y, char *buf)
{
    struct rm *loc = &level->locations[x][y];
    int ltyp = loc->typ, cmap = -1;
    const char *dfeature = 0;
    static char altbuf[BUFSZ];

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
    else if (IS_ALTAR(ltyp)) {
        sprintf(altbuf, "altar to %s (%s)", a_gname(),
                align_str(Amask2align(loc->altarmask & ~AM_SHRINE)));
        dfeature = altbuf;
    } else if (x == level->sstairs.sx && y == level->sstairs.sy &&
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
    if (dfeature)
        strcpy(buf, dfeature);
    return dfeature;
}


/* update_location()
 * companion to look_here, this function will fully describe the location
 * for win_list_items,including traps and features. It does not call pline.
 * This function will only list up to 4 items if Blind, unless the caller sets
 * all_objects to TRUE. In that case the caller should also perform cockatrice
 * checks. */
boolean
update_location(boolean all_objects)
{
    boolean ret, minv = FALSE;
    struct obj *otmp = level->objects[u.ux][u.uy];
    struct trap *trap;
    char buf[BUFSZ], fbuf[BUFSZ];
    const char *dfeature = NULL;
    int ocount, icount = 0, size = 10;
    struct nh_objitem *items;

    if (Blind && !can_reach_floor()) {
        win_list_items(NULL, 0, FALSE);
        return FALSE;
    }

    items = malloc(size * sizeof (struct nh_objitem));
    if (u.uswallow && u.ustuck) {
        otmp = u.ustuck->minvent;
        minv = TRUE;
    } else {
        if ((trap = t_at(level, u.ux, u.uy)) && trap->tseen) {
            sprintf(buf, "There is %s here.", an(trapexplain[trap->ttyp - 1]));
            add_objitem(&items, &size, MI_TEXT, icount++, 0, buf, NULL, FALSE);
        }

        dfeature = dfeature_at(u.ux, u.uy, fbuf);
        if (dfeature && !strcmp(dfeature, "pool of water") && Underwater)
            dfeature = NULL;
        if (dfeature) {
            sprintf(buf, "There is %s here.", an(dfeature));
            add_objitem(&items, &size, MI_TEXT, icount++, 0, buf, NULL, FALSE);
        }

        if (icount && otmp)
            add_objitem(&items, &size, MI_TEXT, icount++, 0, "", NULL, FALSE);
    }

    for (ocount = 0; otmp; otmp = minv ? otmp->nobj : otmp->nexthere) {
        examine_object(otmp);
        if (!Blind || all_objects || ocount < 5)
            add_objitem(&items, &size, MI_NORMAL, icount++, 0,
                        doname_price(otmp), otmp, FALSE);
        ocount++;
    }

    if (Blind && !all_objects && ocount >= 5) {
        sprintf(buf, "There are %s other objects here.",
                (ocount <= 10) ? "several" : "many");
        add_objitem(&items, &size, MI_TEXT, icount++, 0, buf, NULL, FALSE);
    }

    ret = win_list_items(items, icount, FALSE);
    free(items);
    return ret;
}


/* look at what is here; if there are many objects (5 or more),
   don't show them unless obj_cnt is 0 */
int
look_here(int obj_cnt,  /* obj_cnt > 0 implies that autopickup is in progess */
          boolean picked_some)
{
    struct obj *otmp;
    struct trap *trap;
    const char *verb = Blind ? "feel" : "see";
    const char *dfeature = NULL;
    char fbuf[BUFSZ], fbuf2[BUFSZ];
    boolean skip_objects = (obj_cnt >= 5), felt_cockatrice = FALSE;
    int icount = 0;
    int size = 10;
    struct nh_objitem *items = NULL;
    const char *title =
        Blind ? "Things that you feel here:" : "Things that are here:";

    /* show the "things that are here" window iff - the player didn't get the
       info via update_location -OR- - it was explicitly requested (obj_cnt ==
       0) */
    boolean skip_win = update_location(!skip_objects) && obj_cnt;

    if (u.uswallow && u.ustuck) {
        struct monst *mtmp = u.ustuck;

        sprintf(fbuf, "Contents of %s %s", s_suffix(mon_nam(mtmp)),
                mbodypart(mtmp, STOMACH));
        /* Skip "Contents of " by using fbuf index 12 */
        pline("You %s to %s what is lying in %s.",
              Blind ? "try" : "look around", verb, &fbuf[12]);
        otmp = mtmp->minvent;
        if (otmp) {
            for (; otmp; otmp = otmp->nobj) {
                /* If swallower is an animal, it should have become stone
                   but... */
                if (otmp->otyp == CORPSE)
                    feel_cockatrice(otmp, FALSE);
            }
            if (Blind)
                strcpy(fbuf, "You feel");
            strcat(fbuf, ":");
            display_minventory(mtmp, MINV_ALL, fbuf);
        } else {
            pline("You %s no objects here.", verb);
        }
        return ! !Blind;
    }
    if (!skip_objects && (trap = t_at(level, u.ux, u.uy)) && trap->tseen)
        pline("There is %s here.", an(trapexplain[trap->ttyp - 1]));

    otmp = level->objects[u.ux][u.uy];
    dfeature = dfeature_at(u.ux, u.uy, fbuf2);
    if (dfeature && !strcmp(dfeature, "pool of water") && Underwater)
        dfeature = NULL;

    if (Blind) {
        boolean drift = Is_airlevel(&u.uz) || Is_waterlevel(&u.uz);

        if (dfeature && !strncmp(dfeature, "altar ", 6)) {
            /* don't say "altar" twice, dfeature has more info */
            pline("You try to feel what is here.");
        } else {
            pline("You try to feel what is %s%s.",
                  drift ? "floating here" : "lying here on the ",
                  drift ? "" : surface(u.ux, u.uy));
        }
        if (dfeature && !drift && !strcmp(dfeature, surface(u.ux, u.uy)))
            dfeature = NULL;    /* ice already identifed */
        if (!can_reach_floor()) {
            pline("But you can't reach it!");
            return 0;
        }
    }

    if (dfeature)
        sprintf(fbuf, "There is %s here.", an(dfeature));

    if (!otmp || is_lava(level, u.ux, u.uy) ||
        (is_pool(level, u.ux, u.uy) && !Underwater)) {
        if (dfeature)
            pline(fbuf);
        read_engr_at(u.ux, u.uy);
        if (!skip_objects && (Blind || !dfeature))
            pline("You %s no objects here.", verb);
        return ! !Blind;
    }
    /* we know there is something here */

    if (skip_objects) {
        if (dfeature)
            pline(fbuf);
        read_engr_at(u.ux, u.uy);
        pline("There are %s%s objects here.",
              (obj_cnt <= 10) ? "several" : "many", picked_some ? " more" : "");
    } else if (!otmp->nexthere) {
        /* only one object */
        if (dfeature)
            pline(fbuf);
        read_engr_at(u.ux, u.uy);
#ifdef INVISIBLE_OBJECTS
        if (otmp->oinvis && !See_invisible)
            verb = "feel";
#endif
        pline("You %s here %s.", verb, doname_price(otmp));
        if (otmp->otyp == CORPSE)
            feel_cockatrice(otmp, FALSE);
    } else {
        items = malloc(size * sizeof (struct nh_objitem));
        if (dfeature) {
            add_objitem(&items, &size, MI_TEXT, icount++, 0, fbuf, NULL, FALSE);
            add_objitem(&items, &size, MI_TEXT, icount++, 0, "", NULL, FALSE);
        }

        for (; otmp; otmp = otmp->nexthere) {
            if (otmp->otyp == CORPSE && will_feel_cockatrice(otmp, FALSE)) {
                char buf[BUFSZ];

                felt_cockatrice = TRUE;
                strcpy(buf, doname_price(otmp));
                strcat(buf, "...");
                add_objitem(&items, &size, MI_NORMAL, icount++, 0, fbuf, otmp,
                            FALSE);
                break;
            }
            add_objitem(&items, &size, MI_NORMAL, icount++, 0, doname(otmp),
                        otmp, FALSE);
        }

        if (!skip_win || felt_cockatrice)
            display_objects(items, icount, title, PICK_NONE, PLHINT_CONTAINER,
                            NULL);
        free(items);

        if (felt_cockatrice)
            feel_cockatrice(otmp, FALSE);
        read_engr_at(u.ux, u.uy);
    }
    return ! !Blind;
}

/* explicilty look at what is here, including all objects */
int
dolook(void)
{
    return look_here(0, FALSE);
}

boolean
will_feel_cockatrice(struct obj * otmp, boolean force_touch)
{
    if ((Blind || force_touch) && !uarmg && !Stone_resistance &&
        (otmp->otyp == CORPSE && touch_petrifies(&mons[otmp->corpsenm])))
        return TRUE;
    return FALSE;
}

void
feel_cockatrice(struct obj *otmp, boolean force_touch)
{
    char kbuf[BUFSZ];

    if (will_feel_cockatrice(otmp, force_touch)) {
        if (poly_when_stoned(youmonst.data))
            pline("You touched the %s corpse with your bare %s.",
                  mons[otmp->corpsenm].mname, makeplural(body_part(HAND)));
        else
            pline("Touching the %s corpse is a fatal mistake...",
                  mons[otmp->corpsenm].mname);
        sprintf(kbuf, "%s corpse", an(mons[otmp->corpsenm].mname));
        instapetrify(kbuf);
    }
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
doprgold(void)
{
    /* the messages used to refer to "carrying gold", but that didn't take
       containers into account */
    long umoney = money_cnt(invent);

    if (!umoney)
        pline("Your wallet is empty.");
    else
        pline("Your wallet contains %ld %s.", umoney, currency(umoney));

    shopper_financial_report();
    return 0;
}


int
doprwep(void)
{
    if (!uwep) {
        pline("You are empty %s.", body_part(HANDED));
    } else {
        prinv(NULL, uwep, 0L);
        if (u.twoweap)
            prinv(NULL, uswapwep, 0L);
    }
    return 0;
}

int
doprarm(void)
{
    if (!wearing_armor())
        pline("You are not wearing any armor.");
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
doprring(void)
{
    if (!uleft && !uright)
        pline("You are not wearing any rings.");
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
dopramulet(void)
{
    if (!uamul)
        pline("You are not wearing an amulet.");
    else
        prinv(NULL, uamul, 0L);
    return 0;
}

static boolean
tool_in_use(struct obj *obj)
{
    if ((obj->owornmask & (W_TOOL | W_SADDLE)) != 0L)
        return TRUE;
    if (obj->oclass != TOOL_CLASS)
        return FALSE;
    return (boolean) (obj == uwep || obj->lamplit ||
                      (obj->otyp == LEASH && obj->leashmon));
}

int
doprtool(void)
{
    struct obj *otmp;
    int ct = 0;
    char lets[52 + 1];

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (tool_in_use(otmp))
            lets[ct++] = obj_to_let(otmp);
    lets[ct] = '\0';
    if (!ct)
        pline("You are not using any tools.");
    else
        display_inventory(lets, FALSE);
    return 0;
}

/* '*' command; combines the ')' + '[' + '=' + '"' + '(' commands;
   show inventory of all currently wielded, worn, or used objects */
int
doprinuse(void)
{
    struct obj *otmp;
    int ct = 0;
    char lets[52 + 1];

    for (otmp = invent; otmp; otmp = otmp->nobj)
        if (is_worn(otmp) || tool_in_use(otmp))
            lets[ct++] = obj_to_let(otmp);
    lets[ct] = '\0';
    if (!ct)
        pline("You are not wearing or wielding anything.");
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
    if (at_u && u.uundetected && hides_under(youmonst.data))
        u.uundetected = OBJ_AT(u.ux, u.uy);
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

static char *invbuf = NULL;
static unsigned invbufsiz = 0;

char *
let_to_name(char let, boolean unpaid)
{
    const char *class_name;
    const char *pos;
    int oclass = (let >= 1 && let < MAXOCLASSES) ? let : 0;
    unsigned len;

    if (oclass)
        class_name = names[oclass];
    else if ((pos = strchr(oth_symbols, let)) != 0)
        class_name = oth_names[pos - oth_symbols];
    else
        class_name = names[0];

    len = strlen(class_name) + (unpaid ? sizeof "unpaid_" : sizeof "");
    if (len > invbufsiz) {
        if (invbuf)
            free(invbuf);
        invbufsiz = len + 10;   /* add slop to reduce incremental realloc */
        invbuf = malloc(invbufsiz);
    }
    if (unpaid)
        strcat(strcpy(invbuf, "Unpaid "), class_name);
    else
        strcpy(invbuf, class_name);
    return invbuf;
}

void
free_invbuf(void)
{
    if (invbuf)
        free(invbuf), invbuf = NULL;
    invbufsiz = 0;
}


/* all but coins */
static const char organizable[] = {
    ALLOW_COUNT, SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS,
    AMULET_CLASS, GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS,
    WEAPON_CLASS, ROCK_CLASS, CHAIN_CLASS, BALL_CLASS, VENOM_CLASS,
    0
};

int
doorganize(void)
{       /* inventory organizer by Del Lamb */
    struct obj *obj, *otmp;
    int ix, cur;
    char let;
    char alphabet[52 + 1], buf[52 + 1];
    char qbuf[QBUFSZ];
    const char *adj_type;

    /* get a pointer to the object the user wants to organize */
    if (!(obj = getobj(organizable, "adjust")))
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
        compactify(buf);

    /* get new letter to use as inventory letter */
    for (;;) {
        sprintf(qbuf, "Adjust letter to what [%s]?", buf);
        let = query_key(qbuf, NULL);
        if (strchr(quitchars, let)) {
            pline("Never mind.");
            goto cleansplit;
        }
        if (let == '@' || !letter(let))
            pline("Select an inventory slot letter.");
        else
            break;
    }

    /* change the inventory and print the resulting item */

    /* 
     * don't use freeinv/addinv to avoid double-touching artifacts,
     * dousing lamps, losing luck, cursing loadstone, etc.
     */
    extract_nobj(obj, &invent);

    if (let == obj->invlet) {
        otmp = obj;
    } else {
        for (otmp = invent; otmp && otmp->invlet != let;)
            otmp = otmp->nobj;
    }

    if (!otmp)
        adj_type = "Moving:";
    else if (otmp == obj) {
        adj_type = "Merging:";
        for (otmp = invent; otmp;) {
            if (merged(&otmp, &obj)) {
                obj = otmp;
                otmp = otmp->nobj;
                extract_nobj(obj, &invent);
            } else {
                otmp = otmp->nobj;
            }
        }
    } else if (merged(&otmp, &obj)) {
        adj_type = "Merging:";
        obj = otmp;
        extract_nobj(obj, &invent);
    } else {
        struct obj *otmp2;

        for (otmp2 = invent; otmp2 && otmp2->invlet != obj->invlet;)
            otmp2 = otmp2->nobj;

        if (otmp2) {
            char oldlet = obj->invlet;

            adj_type = "Displacing:";

            /* Here be a nasty hack; solutions that don't * require duplication 
               of assigninvlet's code * here are welcome. */
            assigninvlet(obj);

            if (obj->invlet == NOINVSYM) {
                pline("There's nowhere to put that.");
                obj->invlet = oldlet;
                goto cleansplit;
            }
        } else
            adj_type = "Swapping:";
        otmp->invlet = obj->invlet;
    }

    /* inline addinv (assuming !merged) */
    obj->invlet = let;
    obj->nobj = invent; /* insert at beginning */
    obj->where = OBJ_INVENT;
    invent = obj;
    reorder_invent();

    prinv(adj_type, obj, 0L);
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

    display_menu(items, 3, NULL, PICK_NONE, PLHINT_INVENTORY, NULL);

    return;
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
display_minventory(struct monst *mon, int dflags, char *title)
{
    struct obj *ret;
    char tmp[QBUFSZ];
    int n;
    struct object_pick *selected = NULL;
    int do_all = (dflags & MINV_ALL) != 0;

    sprintf(tmp, "%s %s:", s_suffix(noit_Monnam(mon)),
            do_all ? "possessions" : "armament");

    if (do_all ? (mon->minvent != 0)
        : (mon->misc_worn_check || MON_WEP(mon))) {
        /* Fool the 'weapon in hand' routine into displaying 'weapon in claw',
           etc. properly. */
        youmonst.data = mon->data;

        n = query_objlist(title ? title : tmp, mon->minvent, INVORDER_SORT,
                          &selected,
                          (dflags & MINV_NOLET) ? PICK_NONE : PICK_ONE,
                          do_all ? allow_all : worn_wield_only);

        set_uasmon();
    } else {
        invdisp_nothing(title ? title : tmp, "(none)");
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
    char tmp[QBUFSZ];
    int n;
    struct object_pick *selected = 0;

    sprintf(tmp, "Contents of %s:", doname(obj));

    if (obj->cobj) {
        n = query_objlist(tmp, obj->cobj, INVORDER_SORT, &selected, PICK_NONE,
                          allow_all);
    } else {
        invdisp_nothing(tmp, "(empty)");
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
