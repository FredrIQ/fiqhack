/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "eshk.h"

/*#define DEBUG*/

#define PAY_SOME    2
#define PAY_BUY     1
#define PAY_CANT    0   /* too poor */
#define PAY_SKIP  (-1)
#define PAY_BROKE (-2)

static void makekops(coord *);
static void call_kops(struct monst *, boolean);
static void kops_gone(boolean);

#define IS_SHOP(lev, x) ((lev)->rooms[x].rtype >= SHOPBASE)

static long int followmsg;      /* last time of follow message */

static void setpaid(struct monst *);
static long addupbill(struct monst *);
static void pacify_shk(struct monst *);
static struct bill_x *onbill(const struct obj *, struct monst *, boolean);
static struct monst *next_shkp(struct monst *, boolean);
static long shop_debt(struct eshk *);
static const char *shk_owns(const struct obj *);
static const char *mon_owns(const struct obj *);
static void clear_unpaid(struct monst *, struct obj *);
static long check_credit(long, struct monst *);
static void pay(long, struct monst *);
static long get_cost(const struct obj *, struct monst *);
static long set_cost(struct obj *, struct monst *);
static const char *shk_embellish(struct obj *, long);
static long cost_per_charge(struct monst *, struct obj *, boolean);
static long cheapest_item(struct monst *);
static int dopayobj(struct monst *, struct bill_x *, struct obj **, int,
                    boolean);
static long stolen_container(struct obj *, struct monst *, long, boolean);
static long getprice(const struct obj *, boolean);
static void shk_names_obj(struct monst *, struct obj *, const char *, long,
                          const char *);
static struct obj *bp_to_obj(struct bill_x *);
static boolean inherits(struct monst *, int, int);
static void set_repo_loc(struct eshk *);
static boolean angry_shk_exists(void);
static void rile_shk(struct monst *);
static void rouse_shk(struct monst *, boolean);
static void remove_damage(struct monst *, boolean);
static void sub_one_frombill(struct obj *, struct monst *);
static void add_one_tobill(struct obj *, boolean);
static void dropped_container(struct obj *, struct monst *, boolean);
static void add_to_billobjs(struct obj *);
static void bill_box_content(struct obj *, boolean, boolean, struct monst *);
static boolean rob_shop(struct monst *);
static struct obj *find_oid_lev(struct level *lev, unsigned id);

/*
    invariants: obj->unpaid iff onbill(obj) [unless bp->useup]
        obj->quan <= bp->bquan
 */


/* Transfer money from inventory to monster when paying shopkeepers, priests,
   oracle, succubus, & other demons. Simple with only gold coins. This routine
   will handle money changing when multiple coin types is implemented, only
   appropriate monsters will pay change. (Peaceful shopkeepers, priests & the
   oracle try to maintain goodwill while selling their wares or services. Angry
   monsters and all demons will keep anything they get their hands on. Returns
   the amount actually paid, so we can know if the monster kept the change. */
long
money2mon(struct monst *mon, long amount)
{
    struct obj *ygold = findgold(invent);

    if (amount <= 0) {
        impossible("%s payment in money2mon!", amount ? "negative" : "zero");
        return 0L;
    }
    if (!ygold || ygold->quan < amount) {
        impossible("Paying without %s money?", ygold ? "enough" : "");
        return 0L;
    }

    if (ygold->quan > amount)
        ygold = splitobj(ygold, amount);
    else if (ygold->owornmask)
        remove_worn_item(ygold, FALSE); /* quiver */
    freeinv(ygold);
    add_to_minv(mon, ygold);
    return amount;
}


/*
    Transfer money from monster to inventory.
    Used when the shopkeeper pay for items, and when
    the priest gives you money for an ale.
 */
void
money2u(struct monst *mon, long amount)
{
    struct obj *mongold = findgold(mon->minvent);

    if (amount <= 0) {
        impossible("%s payment in money2u!", amount ? "negative" : "zero");
        return;
    }
    if (!mongold || mongold->quan < amount) {
        impossible("%s paying without %s money?", a_monnam(mon),
                   mongold ? "enough" : "");
        return;
    }

    if (mongold->quan > amount)
        mongold = splitobj(mongold, amount);
    obj_extract_self(mongold);

    if (!can_hold(mongold)) {
        pline(msgc_substitute, "You have no room for the money!");
        dropy(mongold);
    } else
        addinv(mongold);
}


static struct monst *
next_shkp(struct monst *shkp, boolean withbill)
{
    for (; shkp; shkp = shkp->nmon) {
        if (DEADMONSTER(shkp))
            continue;
        if (shkp->isshk && (ESHK(shkp)->billct || !withbill))
            break;
    }

    if (shkp) {
        if (NOTANGRY(shkp)) {
            if (ESHK(shkp)->surcharge)
                pacify_shk(shkp);
        } else {
            if (!ESHK(shkp)->surcharge)
                rile_shk(shkp);
        }
    }
    return shkp;
}

/* called in do_name.c */
const char *
shkname(const struct monst *mtmp)
{
    return msg_from_string(CONST_ESHK(mtmp)->shknam);
}

/* called in mon.c */
void
shkgone(struct monst *mtmp)
{
    struct eshk *eshk = ESHK(mtmp);
    struct level *shoplev = levels[ledger_no(&eshk->shoplevel)];
    struct mkroom *sroom = &shoplev->rooms[eshk->shoproom - ROOMOFFSET];
    struct obj *otmp;
    char *p;
    int sx, sy;

    remove_damage(mtmp, TRUE);
    sroom->resident = NULL;

    /* items on shop floor revert to ordinary objects */
    for (sx = sroom->lx; sx <= sroom->hx; sx++)
        for (sy = sroom->ly; sy <= sroom->hy; sy++)
            for (otmp = shoplev->objects[sx][sy]; otmp; otmp = otmp->nexthere)
                otmp->no_charge = 0;

    if (on_level(&eshk->shoplevel, &mtmp->dlevel->z)) {
        /* Make sure bill is set only when the dead shk is the resident shk. */
        if ((p = strchr(u.ushops, eshk->shoproom)) != 0) {
            setpaid(mtmp);
            eshk->bill_inactive = TRUE;
            /* remove eshk->shoproom from u.ushops */
            do {
                *p = *(p + 1);
            } while (*++p);
        }
    }
}

void
set_residency(struct monst *shkp, boolean zero_out)
{
    if (on_level(&(ESHK(shkp)->shoplevel), &shkp->dlevel->z))
        shkp->dlevel->rooms[ESHK(shkp)->shoproom - ROOMOFFSET].resident =
            (zero_out) ? NULL : shkp;
}

void
replshk(struct monst *mtmp, struct monst *mtmp2)
{
    level->rooms[ESHK(mtmp2)->shoproom - ROOMOFFSET].resident = mtmp2;
    if (inhishop(mtmp) && *u.ushops == ESHK(mtmp)->shoproom) {
        ESHK(mtmp2)->bill_inactive = FALSE;
    }
}

/* do shopkeeper specific structure munging -dlc */
void
restshk(struct monst *shkp, boolean ghostly)
{
    struct eshk *eshkp = ESHK(shkp);

    /* shoplevel can change as dungeons move around */
    /* paybill() guarantees that non-homed shk's will be gone */
    if (ghostly) {
        assign_level(&eshkp->shoplevel, &u.uz);
        if (ANGRY(shkp) && strncmpi(eshkp->customer, u.uplname, PL_NSIZ))
            pacify_shk(shkp);
    }
}


/* Clear the unpaid bit on all of the objects in the list. */
static void
clear_unpaid(struct monst *shkp, struct obj *list)
{
    while (list) {
        if (Has_contents(list))
            clear_unpaid(shkp, list->cobj);
        if (onbill(list, shkp, TRUE))
            list->unpaid = 0;
        list = list->nobj;
    }
}


/* either you paid or left the shop or the shopkeeper died */
static void
setpaid(struct monst *shkp)
{
    struct obj *obj;
    struct monst *mtmp;

    clear_unpaid(shkp, invent);
    clear_unpaid(shkp, level->objlist);
    clear_unpaid(shkp, level->buriedobjlist);
    if (thrownobj && onbill(thrownobj, shkp, TRUE))
        thrownobj->unpaid = 0;
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        clear_unpaid(shkp, mtmp->minvent);
    for (mtmp = migrating_mons; mtmp; mtmp = mtmp->nmon)
        clear_unpaid(shkp, mtmp->minvent);

    while ((obj = level->billobjs) != 0) {
        obj_extract_self(obj);
        dealloc_obj(obj);
    }
    ESHK(shkp)->billct = 0;
    ESHK(shkp)->credit = 0L;
    ESHK(shkp)->debit = 0L;
    ESHK(shkp)->loan = 0L;
}

static long
addupbill(struct monst *shkp)
{
    if (ESHK(shkp)->bill_inactive) {
        impossible("adding up inactive bill");
        return 0;
    }

    int ct = ESHK(shkp)->billct;
    struct bill_x *bp = ESHK(shkp)->bill;
    long total = 0L;

    while (ct--) {
        total += bp->price * bp->bquan;
        bp++;
    }
    return total;
}


static void
call_kops(struct monst *shkp, boolean nearshop)
{
    /* Keystone Kops srt@ucla */
    boolean nokops;

    if (!shkp)
        return;

    if (canhear())
        pline(msgc_levelwarning, "An alarm sounds!");

    nokops = ((mvitals[PM_KEYSTONE_KOP].mvflags & G_GONE) &&
              (mvitals[PM_KOP_SERGEANT].mvflags & G_GONE) &&
              (mvitals[PM_KOP_LIEUTENANT].mvflags & G_GONE) &&
              (mvitals[PM_KOP_KAPTAIN].mvflags & G_GONE));

    if (!angry_guards(!canhear()) && nokops) {
        if (canhear())
            pline(msgc_noconsequence, "But no one seems to respond to it.");
        return;
    }

    if (nokops)
        return;

    {
        coord mm;

        if (nearshop) {
            /* Create swarm around you, if you merely "stepped out" */
            if (flags.verbose)
                pline_implied(msgc_levelwarning, "The Keystone Kops appear!");
            mm.x = u.ux;
            mm.y = u.uy;
            makekops(&mm);
            return;
        }

        pline_implied(msgc_levelwarning, "The Keystone Kops are after you!");

        /* Create swarm near down staircase (hinders return to level) */
        if (isok(level->dnstair.sx, level->dnstair.sy)) {
            mm.x = level->dnstair.sx;
            mm.y = level->dnstair.sy;
            makekops(&mm);
        }

        /* Create swarm near shopkeeper (hinders return to shop) */
        mm.x = shkp->mx;
        mm.y = shkp->my;
        makekops(&mm);
    }
}


/* x,y is strictly inside shop */
char
inside_shop(struct level *lev, xchar x, xchar y)
{
    char rno;

    rno = lev->locations[x][y].roomno;
    if ((rno < ROOMOFFSET) || lev->locations[x][y].edge ||
        !IS_SHOP(lev, rno - ROOMOFFSET))
        return NO_ROOM;
    else
        return rno;
}

void
u_left_shop(char *leavestring, boolean newlev)
{
    struct monst *shkp;
    struct eshk *eshkp;

    /* 
     * IF player
     * ((didn't leave outright) AND
     *  ((he is now strictly-inside the shop) OR
     *   (he wasn't strictly-inside last turn anyway)))
     * THEN (there's nothing to do, so just return)
     */
    if (!*leavestring &&
        (!level->locations[u.ux][u.uy].edge ||
         level->locations[u.ux0][u.uy0].edge))
        return;

    shkp = shop_keeper(level, *u.ushops0);
    if (!shkp || !inhishop(shkp))
        return; /* shk died, teleported, changed levels... */

    eshkp = ESHK(shkp);
    if (!eshkp->billct && !eshkp->debit)        /* bill is settled */
        return;

    if (!*leavestring && shkp->mcanmove && !shkp->msleeping) {
        /* 
         * Player just stepped onto shop-boundary (known from above logic).
         * Try to intimidate him into paying his bill
         */
        verbalize(msgc_npcvoice,
                  NOTANGRY(shkp) ? "%s!  Please pay before leaving." :
                  "%s!  Don't you leave without paying!", u.uplname);
        return;
    }

    if (rob_shop(shkp)) {
        call_kops(shkp, (!newlev && level->locations[u.ux0][u.uy0].edge));
    }
}

/* robbery from outside the shop via telekinesis or grappling hook */
void
remote_burglary(xchar x, xchar y)
{
    struct monst *shkp;
    struct eshk *eshkp;

    shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE));
    if (!shkp || !inhishop(shkp))
        return; /* shk died, teleported, changed levels... */

    eshkp = ESHK(shkp);
    if (!eshkp->billct && !eshkp->debit)        /* bill is settled */
        return;

    if (rob_shop(shkp)) {
        /* [might want to set 2nd arg based on distance from shop doorway] */
        call_kops(shkp, FALSE);
    }
}

/* shop merchandise has been taken; pay for it with any credit available;  
   return false if the debt is fully covered by credit, true otherwise */
static boolean
rob_shop(struct monst *shkp)
{
    struct eshk *eshkp;
    long total;

    eshkp = ESHK(shkp);
    rouse_shk(shkp, TRUE);
    total = (addupbill(shkp) + eshkp->debit);
    if (eshkp->credit >= total) {
        pline(msgc_consequence,
              "Your credit of %ld %s is used to cover your shopping bill.",
              (long)eshkp->credit, currency(eshkp->credit));
        total = 0L;     /* credit gets cleared by setpaid() */
    } else {
        pline(msgc_npcanger, "You escaped the shop without paying!");
        total -= eshkp->credit;
    }
    setpaid(shkp);
    if (!total)
        return FALSE;

    /* by this point, we know an actual robbery has taken place */
    eshkp->robbed += total;
    pline(Role_if(PM_ROGUE) ? msgc_info : msgc_alignchaos,
          "You stole %ld %s worth of merchandise.", total, currency(total));
    if (!Role_if(PM_ROGUE))     /* stealing is unlawful */
        adjalign(-sgn(u.ualign.type));

    hot_pursuit(shkp);
    return TRUE;
}

void
u_entered_shop(char *enterstring)
{

    int rt;
    struct monst *shkp;
    struct eshk *eshkp;
    static const char no_shk[] = "This shop appears to be deserted.";
    static char empty_shops[5];

    if (!*enterstring)
        return;

    if (!(shkp = shop_keeper(level, *enterstring))) {
        if (!strchr(empty_shops, *enterstring) &&
            in_rooms(level, u.ux, u.uy, SHOPBASE) !=
            in_rooms(level, u.ux0, u.uy0, SHOPBASE))
            pline(msgc_info, no_shk);
        strcpy(empty_shops, u.ushops);
        u.ushops[0] = '\0';
        return;
    }

    eshkp = ESHK(shkp);

    if (!inhishop(shkp)) {
        /* dump core when referenced */
        eshkp->bill_inactive = TRUE;
        if (!strchr(empty_shops, *enterstring))
            pline(msgc_info, no_shk);
        strcpy(empty_shops, u.ushops);
        u.ushops[0] = '\0';
        return;
    }

    eshkp->bill_inactive = FALSE;

    if ((!eshkp->visitct || *eshkp->customer) &&
        strncmpi(eshkp->customer, u.uplname, PL_NSIZ)) {
        /* You seem to be new here */
        eshkp->visitct = 0;
        eshkp->following = 0;
        strncpy(eshkp->customer, u.uplname, PL_NSIZ);
        pacify_shk(shkp);
    }

    if (shkp->msleeping || !shkp->mcanmove || eshkp->following)
        return; /* no dialog */

    if (Invis) {
        pline_implied(msgc_notresisted, "%s senses your presence.",
                      shkname(shkp));
        verbalize(msgc_npcvoice, "Invisible customers are not welcome!");
        return;
    }

    rt = level->rooms[*enterstring - ROOMOFFSET].rtype;

    if (ANGRY(shkp)) {
        verbalize(msgc_npcvoice,
                  "So, %s, you dare return to %s %s?!", u.uplname,
                  s_suffix(shkname(shkp)), shtypes[rt - SHOPBASE].name);
    } else if (eshkp->robbed) {
        pline(msgc_npcvoice, "%s mutters imprecations against shoplifters.",
              shkname(shkp));
    } else {
        verbalize(msgc_npcvoice, "%s, %s!  Welcome%s to %s %s!", Hello(shkp),
                  u.uplname, eshkp->visitct++ ? " again" : "",
                  s_suffix(shkname(shkp)), shtypes[rt - SHOPBASE].name);
    }
    /* can't do anything about blocking if teleported in */
    if (!inside_shop(level, u.ux, u.uy)) {
        boolean should_block;
        int cnt;
        const char *tool;
        struct obj *pick = carrying(PICK_AXE), *mattock =
            carrying(DWARVISH_MATTOCK);

        if (pick || mattock) {
            cnt = 1;    /* so far */
            if (pick && mattock) {      /* carrying both types */
                tool = "digging tool";
                cnt = 2;        /* `more than 1' is all that matters */
            } else if (pick) {
                tool = "pick-axe";
                /* hack: `pick' already points somewhere into inventory */
                while ((pick = pick->nobj) != 0)
                    if (pick->otyp == PICK_AXE)
                        ++cnt;
            } else {    /* assert(mattock != 0) */
                tool = "mattock";
                while ((mattock = mattock->nobj) != 0)
                    if (mattock->otyp == DWARVISH_MATTOCK)
                        ++cnt;
                /* [ALI] Shopkeeper identifies mattock(s) */
                if (!Blind)
                    makeknown(DWARVISH_MATTOCK);
            }
            verbalize(msgc_hint, NOTANGRY(shkp) ?
                      "Will you please leave your %s%s outside?" :
                      "Leave the %s%s outside.", tool, plur(cnt));
            should_block = TRUE;
        } else if (u.usteed) {
            verbalize(msgc_hint, NOTANGRY(shkp) ?
                      "Will you please leave %s outside?" :
                      "Leave %s outside.", y_monnam(u.usteed));
            should_block = TRUE;
        } else {
            should_block = (Fast &&
                            (sobj_at(PICK_AXE, level, u.ux, u.uy) ||
                             sobj_at(DWARVISH_MATTOCK, level, u.ux, u.uy)));
        }
        if (should_block)
            dochug(shkp);       /* shk gets extra move */
    }
    return;
}

/*
   Decide whether two unpaid items are mergable; caller is responsible for
   making sure they're unpaid and the same type of object; we check the price
   quoted by the shopkeeper and also that they both belong to the same shk.
 */
boolean
same_price(struct obj * obj1, struct obj * obj2)
{
    struct monst *shkp1, *shkp2;
    struct bill_x *bp1 = 0, *bp2 = 0;
    boolean are_mergable = FALSE;

    /* look up the first object by finding shk whose bill it's on */
    for (shkp1 = next_shkp(level->monlist, TRUE); shkp1;
         shkp1 = next_shkp(shkp1->nmon, TRUE))
        if ((bp1 = onbill(obj1, shkp1, TRUE)) != 0)
            break;
    /* second object is probably owned by same shk; if not, look harder */
    if (shkp1 && (bp2 = onbill(obj2, shkp1, TRUE)) != 0) {
        shkp2 = shkp1;
    } else {
        for (shkp2 = next_shkp(level->monlist, TRUE); shkp2;
             shkp2 = next_shkp(shkp2->nmon, TRUE))
            if ((bp2 = onbill(obj2, shkp2, TRUE)) != 0)
                break;
    }

    if (!bp1 || !bp2)
        impossible("same_price: object wasn't on any bill!");
    else
        are_mergable = (shkp1 == shkp2 && bp1->price == bp2->price);
    return are_mergable;
}

/*
 * Figure out how much is owed to a given shopkeeper.
 * At present, we ignore any amount robbed from the shop, to avoid
 * turning the `$' command into a way to discover that the current
 * level is bones data which has a shk on the warpath.
 */
static long
shop_debt(struct eshk *eshkp)
{
    struct bill_x *bp;
    int ct;
    long debt = eshkp->debit;

    if (eshkp->bill_inactive) {
        impossible("finding debt on inactive bill");
        return 0;
    }

    for (bp = eshkp->bill, ct = eshkp->billct; ct > 0; bp++, ct--)
        debt += bp->price * bp->bquan;
    return debt;
}

/* called in response to the `$' command */
void
shopper_financial_report(void)
{
    struct monst *shkp, *this_shkp =
        shop_keeper(level, inside_shop(level, u.ux, u.uy));
    struct eshk *eshkp;
    long amt;
    int pass;

    if (this_shkp && !(ESHK(this_shkp)->credit || shop_debt(ESHK(this_shkp)))) {
        pline(msgc_info, "You have no credit or debt in here.");
        this_shkp = 0;  /* skip first pass */
    }

    /* pass 0: report for the shop we're currently in, if any; pass 1: report
       for all other shops on this level. */
    for (pass = this_shkp ? 0 : 1; pass <= 1; pass++)
        for (shkp = next_shkp(level->monlist, FALSE); shkp;
             shkp = next_shkp(shkp->nmon, FALSE)) {
            if ((shkp != this_shkp) ^ pass)
                continue;
            eshkp = ESHK(shkp);
            if ((amt = eshkp->credit) != 0)
                pline(msgc_info, "You have %ld %s credit at %s %s.",
                      amt, currency(amt), s_suffix(shkname(shkp)),
                      shtypes[eshkp->shoptype - SHOPBASE].name);
            else if (shkp == this_shkp)
                pline(msgc_info, "You have no credit in here.");
            if ((amt = shop_debt(eshkp)) != 0)
                pline(msgc_info, "You owe %s %ld %s.", shkname(shkp),
                      amt, currency(amt));
            else if (shkp == this_shkp)
                pline(msgc_info, "You don't owe any money here.");
        }
}


int
inhishop(struct monst *mtmp)
{
    return (strchr
            (in_rooms(mtmp->dlevel, mtmp->mx, mtmp->my, SHOPBASE),
             ESHK(mtmp)->shoproom) &&
            on_level(&(ESHK(mtmp)->shoplevel), &mtmp->dlevel->z));
}

struct monst *
shop_keeper(struct level *lev, char rmno)
{
    struct monst *shkp =
        rmno >= ROOMOFFSET ? lev->rooms[rmno - ROOMOFFSET].resident : 0;

    if (shkp) {
        if (NOTANGRY(shkp)) {
            if (ESHK(shkp)->surcharge)
                pacify_shk(shkp);
        } else {
            if (!ESHK(shkp)->surcharge)
                rile_shk(shkp);
        }
    }
    return shkp;
}

boolean
tended_shop(struct mkroom * sroom)
{
    struct monst *mtmp = sroom->resident;

    if (!mtmp)
        return FALSE;
    else
        return (boolean) (inhishop(mtmp));
}

static struct bill_x *
onbill(const struct obj *obj, struct monst *shkp, boolean silent)
{
    if (shkp) {
        if (ESHK(shkp)->bill_inactive) {
            impossible("onbill: adding to inactive bill");
            return NULL;
        }

        struct bill_x *bp = ESHK(shkp)->bill;
        int ct = ESHK(shkp)->billct;

        while (--ct >= 0)
            if (bp->bo_id == obj->o_id) {
                if (!obj->unpaid)
                    impossible("onbill: paid obj on bill?");
                return bp;
            } else
                bp++;
    }
    if (obj->unpaid && !silent)
        impossible("onbill: unpaid obj not on bill?");
    return NULL;
}

/* Delete the contents of the given object. */
void
delete_contents(struct obj *obj)
{
    struct obj *curr;

    while ((curr = obj->cobj) != 0) {
        obj_extract_self(curr);
        obfree(curr, NULL);
    }
}

/* called with two args on merge */
void
obfree(struct obj *obj, struct obj *merge)
{
    struct bill_x *bp;
    struct bill_x *bpm;
    struct monst *shkp;

    int i;

    if (obj->otyp == LEASH && obj->leashmon)
        o_unleash(obj);

    /* The FOOD_CLASS check here is very very suspicious, but has been left in
       from 3.4.3 so as not to unexpectedly break things. It's unclear whether
       it was a bizarre optimization ("this will only do anything for food, so
       check that first"), actually required to not break things, or simply a
       no-op all along. */
    if (obj->oclass == FOOD_CLASS && obj->timed)
        obj_stop_timers(obj);

    /* If tracking the object, report the fact that it's been freed */
    for (i = 0; i <= tos_last_slot; i++) {
        if (obj == u.utracked[i])
            u.utracked[i] = NULL;
    }
    for (i = 0; i <= ttos_last_slot; i++) {
        if (obj == turnstate.tracked[i])
            turnstate.tracked[i] = NULL;
    }

    if (Has_contents(obj))
        delete_contents(obj);

    shkp = 0;
    if (obj->unpaid) {
        /* look for a shopkeeper who owns this object */
        for (shkp = next_shkp(level->monlist, TRUE); shkp;
             shkp = next_shkp(shkp->nmon, TRUE))
            if (onbill(obj, shkp, TRUE))
                break;
    }
    /* sanity check, more or less */
    if (!shkp)
        shkp = shop_keeper(level, *u.ushops);
    /* 
     * Note:  `shkp = shop_keeper(level, *u.ushops)' used to be
     *        unconditional.  But obfree() is used all over
     *        the place, so making its behavior be dependent
     *        upon player location doesn't make much sense.
     */

    if ((bp = onbill(obj, shkp, FALSE)) != 0) {
        if (!merge) {
            bp->useup = 1;
            obj->unpaid = 0;    /* only for doinvbill */
            add_to_billobjs(obj);
            return;
        }
        bpm = onbill(merge, shkp, FALSE);
        if (!bpm) {
            /* this used to be a rename */
            impossible("obfree: not on bill??");
            return;
        } else {
            /* this was a merger */
            bpm->bquan += bp->bquan;
            ESHK(shkp)->billct--;
            *bp = ESHK(shkp)->bill[ESHK(shkp)->billct];

        }
    }

    unwield_silently(obj);

    dealloc_obj(obj);
}


static long
check_credit(long tmp, struct monst *shkp)
{
    long credit = ESHK(shkp)->credit;

    if (credit == 0L)
        return tmp;
    if (credit >= tmp) {
        pline(msgc_actionok, "The price is deducted from your credit.");
        ESHK(shkp)->credit -= tmp;
        tmp = 0L;
    } else {
        pline(msgc_actionok, "The price is partially covered by your credit.");
        ESHK(shkp)->credit = 0L;
        tmp -= credit;
    }
    return tmp;
}

static void
pay(long tmp, struct monst *shkp)
{
    long robbed = ESHK(shkp)->robbed;
    long balance = ((tmp <= 0L) ? tmp : check_credit(tmp, shkp));

    if (balance > 0)
        money2mon(shkp, balance);
    else if (balance < 0)
        money2u(shkp, -balance);

    if (robbed) {
        robbed -= tmp;
        if (robbed < 0)
            robbed = 0L;
        ESHK(shkp)->robbed = robbed;
    }
}


/* return shkp to home position */
void
home_shk(struct monst *shkp, boolean killkops)
{
    xchar x = ESHK(shkp)->shk.x, y = ESHK(shkp)->shk.y;

    mnearto(shkp, x, y, TRUE);
    if (killkops) {
        kops_gone(TRUE);
        pacify_guards();
    }
    after_shk_move(shkp);
}

static boolean
angry_shk_exists(void)
{
    struct monst *shkp;

    for (shkp = next_shkp(level->monlist, FALSE); shkp;
         shkp = next_shkp(shkp->nmon, FALSE))
        if (ANGRY(shkp))
            return TRUE;
    return FALSE;
}

/* remove previously applied surcharge from all billed items */
static void
pacify_shk(struct monst *shkp)
{
    if (ESHK(shkp)->bill_inactive) {
        impossible("pacifying shkp with inactive bill");
        return;
    }

    NOTANGRY(shkp) = TRUE;      /* make peaceful */
    if (ESHK(shkp)->surcharge) {
        struct bill_x *bp = ESHK(shkp)->bill;
        int ct = ESHK(shkp)->billct;

        ESHK(shkp)->surcharge = FALSE;
        while (ct-- > 0) {
            long reduction = (bp->price + 3L) / 4L;

            bp->price -= reduction;     /* undo 33% increase */
            bp++;
        }
    }
}

/* add aggravation surcharge to all billed items */
static void
rile_shk(struct monst *shkp)
{
    if (ESHK(shkp)->bill_inactive) {
        impossible("riling shkp with inactive bill");
        return;
    }

    NOTANGRY(shkp) = FALSE;     /* make angry */
    if (!ESHK(shkp)->surcharge) {
        struct bill_x *bp = ESHK(shkp)->bill;
        int ct = ESHK(shkp)->billct;

        ESHK(shkp)->surcharge = TRUE;
        while (ct-- > 0) {
            long surcharge = (bp->price + 2L) / 3L;

            bp->price += surcharge;
            bp++;
        }
    }
}

/* wakeup and/or unparalyze shopkeeper */
static void
rouse_shk(struct monst *shkp, boolean verbosely)
{
    if (!shkp->mcanmove || shkp->msleeping) {
        /* greed induced recovery... */
        if (verbosely && canspotmon(shkp))
            pline(msgc_moncombatbad, "%s %s.", Monnam(shkp),
                  shkp->msleeping ? "wakes up" : "can move again");
        shkp->msleeping = 0;
        shkp->mfrozen = 0;
        shkp->mcanmove = 1;
    }
}

void
make_happy_shk(struct monst *shkp, boolean silentkops)
{
    boolean wasmad = ANGRY(shkp);
    struct eshk *eshkp = ESHK(shkp);

    pacify_shk(shkp);
    eshkp->following = 0;
    eshkp->robbed = 0L;
    if (!Role_if(PM_ROGUE))
        adjalign(sgn(u.ualign.type));
    if (!inhishop(shkp)) {
        boolean vanished = canseemon(shkp);

        const char *shk_nam = mon_nam(shkp);
        if (on_level(&eshkp->shoplevel, &shkp->dlevel->z)) {
            home_shk(shkp, FALSE);
            /* didn't disappear if shk can still be seen */
            if (canseemon(shkp))
                vanished = FALSE;
        } else {
            /* if sensed, does disappear regardless whether seen */
            if (sensemon(shkp))
                vanished = TRUE;
            /* can't act as porter for the Amulet, even if shk happens to be
               going farther down rather than up */
            mdrop_special_objs(shkp);
            /* arrive near shop's door */
            migrate_to_level(shkp, ledger_no(&eshkp->shoplevel), MIGR_APPROX_XY,
                             &eshkp->shd);
        }
        if (vanished)
            pline(msgc_actionok, "Satisfied, %s suddenly disappears!",
                  shk_nam);
    } else if (wasmad)
        pline(msgc_actionok, "%s calms down.", Monnam(shkp));

    if (!angry_shk_exists()) {
        kops_gone(silentkops);
        pacify_guards();
    }
}

void
hot_pursuit(struct monst *shkp)
{
    if (!shkp->isshk)
        return;

    rile_shk(shkp);
    strncpy(ESHK(shkp)->customer, u.uplname, PL_NSIZ);
    ESHK(shkp)->following = 1;
}

/* used when the shkp is teleported or falls (ox == 0) out of his shop,
 * or when the player is not on a costly_spot and he
 * damages something inside the shop.  these conditions
 * must be checked by the calling function.
 */
void
make_angry_shk(struct monst *shkp, xchar ox, xchar oy)
{
    xchar sx, sy;
    struct eshk *eshkp = ESHK(shkp);

    /* all pending shop transactions are now "past due" */
    if (eshkp->billct || eshkp->debit || eshkp->loan || eshkp->credit) {
        eshkp->robbed += (addupbill(shkp) + eshkp->debit + eshkp->loan);
        eshkp->robbed -= eshkp->credit;
        if (eshkp->robbed < 0L)
            eshkp->robbed = 0L;
        /* billct, debit, loan, and credit will be cleared by setpaid */
        setpaid(shkp);
    }

    /* If you just used a wand of teleportation to send the shk away, you might 
       not be able to see her any more.  Monnam would yield "it", which makes
       this message look pretty silly, so temporarily restore her original
       location during the call to Monnam. */
    sx = shkp->mx, sy = shkp->my;
    if (isok(ox, oy) && cansee(ox, oy) && !cansee(sx, sy))
        shkp->mx = ox, shkp->my = oy;
    pline(msgc_npcanger, "%s %s!", Monnam(shkp),
          !ANGRY(shkp) ? "gets angry" : "is furious");
    shkp->mx = sx, shkp->my = sy;
    hot_pursuit(shkp);
}

static const char no_money[] = "Moreover, you%s have no money.";
static const char not_enough_money[] =
    "Besides, you don't have enough to interest %s.";


/* delivers the cheapest item on the list */
static long
cheapest_item(struct monst *shkp)
{
    if (ESHK(shkp)->bill_inactive) {
        impossible("finding cheapest item on inactive bill");
        return 0;
    }

    int ct = ESHK(shkp)->billct;
    struct bill_x *bp = ESHK(shkp)->bill;
    long gmin = (bp->price * bp->bquan);

    while (ct--) {
        if (bp->price * bp->bquan < gmin)
            gmin = bp->price * bp->bquan;
        bp++;
    }
    return gmin;
}


int
dopay(const struct nh_cmd_arg *arg)
{
    struct eshk *eshkp;
    struct monst *shkp;
    struct monst *nxtm, *resident;
    long ltmp, umoney;
    int pass, tmp, sk = 0, seensk = 0;
    char iprompt;
    boolean paid = FALSE, stashed_gold = (hidden_gold() > 0L);
    struct obj *oneitem = NULL;

    char allowall[] = {ALL_CLASSES, 0};
    if (arg->argtype & CMD_ARG_OBJ)
        oneitem = getargobj(arg, allowall, "pay for");

    /* TODO: This seems to be there only because the command is not reasonable
       to repeat. I'm not sure if overriding the user's intentions is
       worthwhile, though; if someone types n2p, they probably mean it. */
    action_completed();

    /* find how many shk's there are, how many are in */
    /* sight, and are you in a shop room with one.  */
    nxtm = resident = 0;
    for (shkp = next_shkp(level->monlist, FALSE); shkp;
         shkp = next_shkp(shkp->nmon, FALSE)) {
        sk++;
        if (ANGRY(shkp) && distu(shkp->mx, shkp->my) <= 2)
            nxtm = shkp;
        if (canspotmon(shkp))
            seensk++;
        if (inhishop(shkp) && (*u.ushops == ESHK(shkp)->shoproom))
            resident = shkp;
    }

    if (nxtm) {         /* Player should always appease an */
        shkp = nxtm;    /* irate shk standing next to them. */
        goto proceed;
    }

    if ((!sk && (!Blind || Blind_telepat)) || (!Blind && !seensk)) {
        pline(msgc_cancelled, "There appears to be no shopkeeper here "
              "to receive your payment.");
        return 0;
    }

    if (!seensk) {
        pline(msgc_cancelled, "You can't see...");
        return 0;
    }

    /* the usual case.  allow paying at a distance when */
    /* inside a tended shop.  should we change that? */
    if (sk == 1 && resident) {
        shkp = resident;
        goto proceed;
    }

    if (seensk == 1) {
        for (shkp = next_shkp(level->monlist, FALSE); shkp;
             shkp = next_shkp(shkp->nmon, FALSE))
            if (canspotmon(shkp))
                break;
        if (shkp != resident && distu(shkp->mx, shkp->my) > 2) {
            pline(msgc_cancelled,
                  "%s is not near enough to receive your payment.",
                  Monnam(shkp));
            return 0;
        }
    } else {
        struct monst *mtmp;
        coord cc;
        int cx, cy;

        pline(msgc_uiprompt, "Pay whom?");
        cc.x = u.ux;
        cc.y = u.uy;
        if (getpos(&cc, TRUE, "the creature you want to pay", FALSE) ==
            NHCR_CLIENT_CANCEL)
            return 0;   /* player pressed ESC */
        cx = cc.x;
        cy = cc.y;
        if (cx < 0) {
            pline(msgc_cancelled, "Try again...");
            return 0;
        }
        if (u.ux == cx && u.uy == cy) {
            pline(msgc_cancelled, "You are generous to yourself.");
            return 0;
        }
        mtmp = m_at(level, cx, cy);
        if (!mtmp) {
            pline(msgc_cancelled,
                  "There is no one there to receive your payment.");
            return 0;
        }
        if (!mtmp->isshk) {
            pline(msgc_cancelled, "%s is not interested in your payment.",
                  Monnam(mtmp));
            return 0;
        }
        if (mtmp != resident && distu(mtmp->mx, mtmp->my) > 2) {
            pline(msgc_cancelled,
                  "%s is too far to receive your payment.", Monnam(mtmp));
            return 0;
        }
        shkp = mtmp;
    }

    if (!shkp)
        return 0;

proceed:
    eshkp = ESHK(shkp);
    ltmp = eshkp->robbed;

    if (eshkp->bill_inactive) {
        impossible("paying shopkeeper with inactive bill");
        return 0;
    }

    /* wake sleeping shk when someone who owes money offers payment */
    if (ltmp || eshkp->billct || eshkp->debit)
        rouse_shk(shkp, TRUE);

    if (!shkp->mcanmove || shkp->msleeping) {   /* still asleep/paralyzed */
        pline(msgc_cancelled, "%s %s.", Monnam(shkp),
              rn2(2) ? "seems to be napping" : "doesn't respond");
        return 0;
    }

    if (shkp != resident && NOTANGRY(shkp)) {
        umoney = money_cnt(invent);

        if (!ltmp)
            pline(msgc_cancelled1, "You do not owe %s anything.",
                  mon_nam(shkp));
        else if (!umoney) {
            pline(msgc_cancelled1, "You %shave no money.",
                  stashed_gold ? "seem to " : "");
            if (stashed_gold)
                pline(msgc_cancelled1, "But you have some gold stashed away.");
        } else {
            if (umoney > ltmp) {
                pline(msgc_actionok,
                      "You give %s the %ld gold piece%s %s asked for.",
                      mon_nam(shkp), ltmp, plur(ltmp), mhe(shkp));
                pay(ltmp, shkp);
            } else {
                pline(msgc_actionok, "You give %s all your%s gold.",
                      mon_nam(shkp), stashed_gold ? " openly kept" : "");
                pay(umoney, shkp);
                if (stashed_gold)
                    pline(msgc_hint, "But you have hidden gold!");
            }
            if ((umoney < ltmp / 2L) || (umoney < ltmp && stashed_gold))
                pline(msgc_badidea,
                      "Unfortunately, %s doesn't look satisfied.", mhe(shkp));
            else
                make_happy_shk(shkp, FALSE);
        }
        return 1;
    }

    /* ltmp is still eshkp->robbed here */
    if (!eshkp->billct && !eshkp->debit) {
        umoney = money_cnt(invent);

        if (!ltmp && NOTANGRY(shkp)) {
            pline(msgc_cancelled1, "You do not owe %s anything.",
                  mon_nam(shkp));
            if (!umoney)
                pline(msgc_cancelled1, no_money,
                      stashed_gold ? " seem to" : "");
        } else if (ltmp) {
            /* This message is remarkably hard to channelize. npcvoice seems to
               fit best. */
            pline(msgc_npcvoice, "%s is after blood, not money!",
                  Monnam(shkp));
            if (umoney < ltmp / 2L || (umoney < ltmp && stashed_gold)) {
                if (!umoney)
                    pline(msgc_cancelled1, no_money,
                          stashed_gold ? " seem to" : "");
                else
                    pline(msgc_cancelled1, not_enough_money, mhim(shkp));
                return 1;
            }
            pline(msgc_actionok, "But since %s shop has been robbed recently,",
                  mhis(shkp));
            pline(msgc_actionok, "you %scompensate %s for %s losses.",
                  (umoney < ltmp) ? "partially " : "", mon_nam(shkp),
                  mhis(shkp));
            pay(umoney < ltmp ? umoney : ltmp, shkp);
            make_happy_shk(shkp, FALSE);
        } else {
            /* shopkeeper is angry, but has not been robbed -- door broken,
               attacked, etc. */
            if (umoney < 1000L) {
                pline(msgc_npcvoice, "%s is after your %s, not your money!",
                      Monnam(shkp), mbodypart(&youmonst, HIDE));
                if (!umoney)
                    pline(msgc_cancelled1, no_money,
                          stashed_gold ? " seem to" : "");
                else
                    pline(msgc_cancelled1, not_enough_money, mhim(shkp));
                return 1;
            }
            pline(msgc_occstart,
                  "You try to appease %s by giving %s 1000 gold pieces.",
                  x_monnam(shkp, ARTICLE_THE, "angry", 0, FALSE), mhim(shkp));
            pay(1000L, shkp);
            if (strncmp(eshkp->customer, u.uplname, PL_NSIZ) || rn2(3))
                make_happy_shk(shkp, FALSE);
            else
                pline(msgc_failrandom,
                      "But despite being paid, %s is as angry as ever.",
                      mon_nam(shkp));
        }
        return 1;
    }
    if (shkp != resident) {
        impossible("dopay: not to shopkeeper?");
        if (resident)
            setpaid(resident);
        return 0;
    }
    /* pay debt, if any, first */
    if (eshkp->debit) {
        long dtmp = eshkp->debit;
        long loan = eshkp->loan;
        const char *sbuf;

        umoney = money_cnt(invent);
        sbuf = msgprintf("You owe %s %ld %s ", shkname(shkp), dtmp,
                         currency(dtmp));
        if (loan) {
            if (loan == dtmp)
                sbuf = msgcat(sbuf, "you picked up in the store.");
            else
                sbuf = msgcat(sbuf,
                              "for gold picked up and the use of merchandise.");
        } else
            sbuf = msgcat(sbuf, "for the use of merchandise.");
        pline(msgc_unpaid, "%s", sbuf);
        if (umoney + eshkp->credit < dtmp) {
            pline(msgc_cancelled1, "But you don't%s have enough gold%s.",
                  stashed_gold ? " seem to" : "",
                  eshkp->credit ? " or credit" : "");
            return 1;
        } else {
            if (eshkp->credit >= dtmp) {
                eshkp->credit -= dtmp;
                eshkp->debit = 0L;
                eshkp->loan = 0L;
                pline(msgc_actionok, "Your debt is covered by your credit.");
            } else if (!eshkp->credit) {
                money2mon(shkp, dtmp);
                eshkp->debit = 0L;
                eshkp->loan = 0L;
                pline(msgc_actionok, "You pay that debt.");
            } else {
                dtmp -= eshkp->credit;
                eshkp->credit = 0L;
                money2mon(shkp, dtmp);
                eshkp->debit = 0L;
                eshkp->loan = 0L;
                pline(msgc_actionok,
                      "That debt is partially offset by your credit.");
                pline_implied(msgc_actionok, "You pay the remainder.");
            }
            paid = TRUE;
        }
    }
    /* now check items on bill */
    if (eshkp->billct) {
        boolean itemize;
        boolean oneitem_found = FALSE;

        umoney = money_cnt(invent);
        if (!umoney && !eshkp->credit) {
            /* TODO: this can return 0 despite paying off a debt earlier (the
               reverse situation to msgc_cancelled1); we should check if we did
               something earlier, and if we did, use msgc_yafm and return 1 */
            pline(msgc_cancelled, "You %shave no money or credit%s.",
                  stashed_gold ? "seem to " : "", paid ? " left" : "");
            return 0;
        }
        if ((umoney + eshkp->credit) < cheapest_item(shkp)) {
            pline(msgc_cancelled,
                  "You don't have enough money to buy%s the item%s you picked.",
                  eshkp->billct > 1 ? " any of" : "", plur(eshkp->billct));
            if (stashed_gold)
                pline(msgc_hint, "Maybe you have some gold stashed away?");
            return 0;
        }

        /* this isn't quite right; it itemizes without asking if the single
           item on the bill is partly used up and partly unpaid */
        if (!oneitem) {
            iprompt = (eshkp->billct > 1 ? ynq("Itemized billing?") : 'y');
            itemize = iprompt == 'y';
            if (iprompt == 'q')
                goto thanks;
        } else {
            /* Paying for an item can't be undone, so put up a prompt. */
            itemize = TRUE;
        }

        for (pass = 0; pass <= 1; pass++) {
            tmp = 0;
            while (tmp < eshkp->billct) {
                struct obj *otmp;
                struct bill_x *bp = eshkp->bill + tmp;

                /* find the object on one of the lists */
                if ((otmp = bp_to_obj(bp)) != 0) {
                    /* if completely used up, object quantity is stale;
                       restoring it to its original value here avoids making
                       the partly-used-up code more complicated */
                    if (bp->useup)
                        otmp->quan = bp->bquan;
                } else {
                    impossible("Shopkeeper administration out of order.");
                    setpaid(shkp);      /* be nice to the player */
                    return 1;
                }
                if (pass == bp->useup && otmp->quan == bp->bquan) {
                    /* pay for used-up items on first pass and others on
                       second, so player will be stuck in the store less often; 
                       things which are partly used up are processed on both
                       passes */
                    tmp++;
                } else if (oneitem == NULL || otmp == oneitem) {
                    if (oneitem && !oneitem_found)
                        oneitem_found = TRUE;
                    switch (dopayobj(shkp, bp, &otmp, pass, itemize)) {
                    case PAY_CANT:
                        return 1;       /* break */
                    case PAY_BROKE:
                        paid = TRUE;
                        goto thanks;    /* break */
                    case PAY_SKIP:
                        tmp++;
                        continue;       /* break */
                    case PAY_SOME:
                        paid = TRUE;
                        if (itemize)
                            bot();
                        continue;       /* break */
                    case PAY_BUY:
                        paid = TRUE;
                        break;
                    }
                    if (itemize)
                        bot();
                    *bp = eshkp->bill[--eshkp->billct];
                } else {
                    /* Paying for one item, but this otmp isn't it. */
                    tmp++;
                }
            }
        }
        if (oneitem && !oneitem_found)
            pline(msgc_yafm, "%s does not own %s.", shkname(shkp),
                  oneitem->quan == 1L ? "that" : "those");
    thanks:
        if (!itemize)
            update_inventory(); /* Done in dopayobj() if itemize. */
    }
    if (!ANGRY(shkp) && paid)
        verbalize(msgc_npcvoice, "Thank you for shopping in %s %s!",
                  s_suffix(shkname(shkp)),
                  shtypes[eshkp->shoptype - SHOPBASE].name);
    return 1;
}


/* return 2 if used-up portion paid */
/*        1 if paid successfully    */
/*        0 if not enough money     */
/*       -1 if skip this object     */
/*       -2 if no money/credit left */
static int
dopayobj(struct monst *shkp, struct bill_x *bp, struct obj **obj_p,
         int which,  /* 0 => used-up item, 1 => other (unpaid or lost) */
         boolean itemize)
{
    struct obj *obj = *obj_p;
    long ltmp, quan, save_quan;
    long umoney = money_cnt(invent);
    int buy;
    boolean stashed_gold = (hidden_gold() > 0L), consumed = (which == 0);

    if (!obj->unpaid && !bp->useup) {
        impossible("Paid object on bill??");
        return PAY_BUY;
    }
    if (itemize && umoney + ESHK(shkp)->credit == 0L) {
        pline(msgc_yafm, "You %shave no money or credit left.",
              stashed_gold ? "seem to " : "");
        return PAY_BROKE;
    }
    /* we may need to temporarily adjust the object, if part of the original
       quantity has been used up but part remains unpaid */
    save_quan = obj->quan;
    if (consumed) {
        /* either completely used up (simple), or split needed */
        quan = bp->bquan;
        if (quan > obj->quan)   /* difference is amount used up */
            quan -= obj->quan;
    } else {
        /* dealing with ordinary unpaid item */
        quan = obj->quan;
    }
    obj->quan = quan;   /* to be used by doname() */
    obj->unpaid = 0;    /* ditto */
    ltmp = bp->price * quan;
    buy = PAY_BUY;      /* flag; if changed then return early */

    if (itemize) {
        const char *qbuf;

        qbuf = msgprintf("%s for %ld %s.  Pay?",
                         quan == 1L ? Doname2(obj) : doname(obj),
                         ltmp, currency(ltmp));
        if (yn(qbuf) == 'n') {
            buy = PAY_SKIP;     /* don't want to buy */
        } else if (quan < bp->bquan && !consumed) {     /* partly used goods */
            obj->quan = bp->bquan - save_quan;  /* used up amount */
            verbalize(msgc_hint, "%s for the other %s before buying %s.",
                      ANGRY(shkp) ? "Pay" : "Please pay", xname(obj),
                      save_quan > 1L ? "these" : "this one");
            buy = PAY_SKIP;     /* shk won't sell */
        }
    }
    if (buy == PAY_BUY && umoney + ESHK(shkp)->credit < ltmp) {
        pline(msgc_hint, "You don't%s have gold%s enough to pay for %s.",
              stashed_gold ? " seem to" : "",
              (ESHK(shkp)->credit > 0L) ? " or credit" : "", doname(obj));
        buy = itemize ? PAY_SKIP : PAY_CANT;
    }

    if (buy != PAY_BUY) {
        /* restore unpaid object to original state */
        obj->quan = save_quan;
        obj->unpaid = 1;
        return buy;
    }

    pay(ltmp, shkp);
    shk_names_obj(shkp, obj,
                  consumed ? "You paid for %s at a cost of %ld gold piece%s.%s"
                  : "You bought %s for %ld gold piece%s.%s", ltmp, "");
    obj->quan = save_quan;      /* restore original count */
    /* quan => amount just bought, save_quan => remaining unpaid count */
    if (consumed) {
        if (quan != bp->bquan) {
            /* eliminate used-up portion; remainder is still unpaid */
            bp->bquan = obj->quan;
            obj->unpaid = 1;
            bp->useup = 0;
            buy = PAY_SOME;
        } else {        /* completely used-up, so get rid of it */
            obj_extract_self(obj);
            /* assert( obj == *obj_p ); */
            dealloc_obj(obj);
            *obj_p = 0; /* destroy pointer to freed object */
        }
    } else if (itemize)
        update_inventory();     /* Done just once in dopay() if !itemize. */
    return buy;
}


static coord repo_location;     /* repossession context */

/* routine called after dying (or quitting) */
/* returns TRUE if the shopkeeper takes the hero's inventory */
boolean
paybill(int croaked)
{
    struct monst *mtmp, *mtmp2, *resident = NULL;
    boolean taken = FALSE;
    int numsk = 0;

    /* if we escaped from the dungeon, shopkeepers can't reach us; shops don't
       occur on level 1, but this could happen if hero level teleports out of
       the dungeon and manages not to die */
    if (croaked < 0)
        return FALSE;

    /* this is where inventory will end up if any shk takes it */
    repo_location.x = repo_location.y = 0;

    /* give shopkeeper first crack */
    if ((mtmp = shop_keeper(level, *u.ushops)) && inhishop(mtmp)) {
        numsk++;
        resident = mtmp;
        taken = inherits(resident, numsk, croaked);
    }
    for (mtmp = next_shkp(level->monlist, FALSE); mtmp;
         mtmp = next_shkp(mtmp2, FALSE)) {
        mtmp2 = mtmp->nmon;
        if (mtmp != resident) {
            /* for bones: we don't want a shopless shk around */
            if (!on_level(&(ESHK(mtmp)->shoplevel), &mtmp->dlevel->z))
                mongone(mtmp);
            else {
                numsk++;
                taken |= inherits(mtmp, numsk, croaked);
            }
        }
    }
    if (numsk == 0)
        return FALSE;
    return taken;
}

static boolean
inherits(struct monst *shkp, int numsk, int croaked)
{
    long loss = 0L;
    long umoney;
    struct eshk *eshkp = ESHK(shkp);
    boolean take = FALSE, taken = FALSE;
    int roomno = *u.ushops;

    /* the simplifying principle is that first-come */
    /* already took everything you had.  */
    if (numsk > 1) {
        if (cansee(shkp->mx, shkp->my) && croaked)
            pline(msgc_npcvoice, "%s %slooks at your corpse%s and %s.",
                  Monnam(shkp),
                  (!shkp->mcanmove || shkp->msleeping) ? "wakes up, " : "",
                  !rn2(2) ? (shkp->female ? ", shakes her head," :
                             ", shakes his head,") : "",
                  !inhishop(shkp) ? "disappears" : "sighs");
        rouse_shk(shkp, FALSE); /* wake shk for bones */
        taken = (roomno == eshkp->shoproom);
        goto skip;
    }

    /* get one case out of the way: you die in the shop, the */
    /* shopkeeper is peaceful, nothing stolen, nothing owed. */
    if (roomno == eshkp->shoproom && inhishop(shkp) && !eshkp->billct &&
        !eshkp->robbed && !eshkp->debit && NOTANGRY(shkp) &&
        !eshkp->following) {
        if (invent)
            pline(msgc_outrobad,
                  "%s gratefully inherits all your possessions.",
                  shkname(shkp));
        set_repo_loc(eshkp);
        goto clear;
    }

    if (eshkp->billct || eshkp->debit || eshkp->robbed) {
        if (roomno == eshkp->shoproom && inhishop(shkp))
            loss = addupbill(shkp) + eshkp->debit;
        if (loss < eshkp->robbed)
            loss = eshkp->robbed;
        take = TRUE;
    }

    if (eshkp->following || ANGRY(shkp) || take) {
        if (!invent)
            goto skip;
        umoney = money_cnt(invent);
        const char *takes = "takes";
        if (distu(shkp->mx, shkp->my) > 2)
            takes = msgcat("comes and ", takes);
        if (!shkp->mcanmove || shkp->msleeping)
            takes = msgcat("wakes up and ", takes);

        if (loss > umoney || !loss || roomno == eshkp->shoproom) {
            eshkp->robbed -= umoney;
            if (eshkp->robbed < 0L)
                eshkp->robbed = 0L;
            if (umoney > 0)
                money2mon(shkp, umoney);
            pline(msgc_outrobad, "%s %s all your possessions.",
                  shkname(shkp), takes);
            taken = TRUE;
            /* where to put player's invent (after disclosure) */
            set_repo_loc(eshkp);
        } else {
            money2mon(shkp, loss);
            pline(msgc_outrobad, "%s %s the %ld %s %sowed %s.", Monnam(shkp),
                  takes, loss, currency(loss),
                  strncmp(eshkp->customer, u.uplname, PL_NSIZ) ? "" : "you ",
                  shkp->female ? "her" : "him");
            /* shopkeeper has now been paid in full */
            pacify_shk(shkp);
            eshkp->following = 0;
            eshkp->robbed = 0L;
        }
    skip:
        /* in case we create bones */
        rouse_shk(shkp, FALSE); /* wake up */
        if (!inhishop(shkp))
            home_shk(shkp, FALSE);
    }
clear:
    setpaid(shkp);
    return taken;
}

static void
set_repo_loc(struct eshk *eshkp)
{
    xchar ox, oy;

    /* if you're not in this shk's shop room, or if you're in its doorway or
       entry spot, then your gear gets dumped all the way inside */
    if (*u.ushops != eshkp->shoproom ||
        IS_DOOR(level->locations[u.ux][u.uy].typ) ||
        (u.ux == eshkp->shk.x && u.uy == eshkp->shk.y)) {
        /* shk.x,shk.y is the position immediately in front of the door -- move 
           in one more space */
        ox = eshkp->shk.x;
        oy = eshkp->shk.y;
        ox += sgn(ox - eshkp->shd.x);
        oy += sgn(oy - eshkp->shd.y);
    } else {    /* already inside this shk's shop */
        ox = u.ux;
        oy = u.uy;
    }
    /* finish_paybill will deposit invent here */
    repo_location.x = ox;
    repo_location.y = oy;
}

/* Dump inventory to the floor, called at game exit, after inventory disclosure
   but before making bones */
void
finish_paybill(void)
{
    struct obj *otmp;
    int ox = repo_location.x, oy = repo_location.y;

    /* If twoweaponing and the swap weapon gets cursed, it slips to the ground
       and we get a warning message. */
    unwield_weapons_silently();

    /* normally done by savebones(), but that's too late in this case */
    unleash_all();
    /* transfer all of the character's inventory to the shop floor */
    while ((otmp = invent) != 0) {
        otmp->owornmask = 0L;   /* perhaps we should call setnotworn? */
        otmp->lamplit = 0;      /* avoid "goes out" msg from freeinv */
        if (rn2(5))
            curse(otmp);        /* normal bones treatment for invent */
        obj_extract_self(otmp);
        place_object(otmp, level, ox, oy);
    }
}

/* find obj on one of the lists */
static struct obj *
bp_to_obj(struct bill_x *bp)
{
    struct obj *obj;
    unsigned int id = bp->bo_id;

    if (bp->useup)
        obj = o_on(id, level->billobjs);
    else
        obj = find_oid(id);
    return obj;
}


static struct obj *
find_oid_lev(struct level *lev, unsigned id)
{
    struct obj *obj;
    struct monst *mon;

    if ((obj = o_on(id, lev->objlist)) != 0)
        return obj;
    if ((obj = o_on(id, lev->buriedobjlist)) != 0)
        return obj;

    for (mon = lev->monlist; mon; mon = mon->nmon)
        if ((obj = o_on(id, mon->minvent)) != 0)
            return obj;

    return NULL;
}

/*
 * Look for o_id on all lists but billobj.  Return obj or NULL if not found.
 * It's OK for restore_timers() to call this function, there should not
 * be any timeouts on the billobjs chain.
 */
struct obj *
find_oid(unsigned id)
{
    struct obj *obj;
    struct monst *mon;
    int i;

    /* try searching the current level, if any */
    if (level && (obj = find_oid_lev(level, id)))
        return obj;

    /* first check various obj lists directly */
    if ((obj = o_on(id, invent)))
        return obj;

    /* not found yet; check inventory for members of various monst lists */
    for (mon = migrating_mons; mon; mon = mon->nmon)
        if ((obj = o_on(id, mon->minvent)))
            return obj;
    for (mon = turnstate.migrating_pets; mon; mon = mon->nmon)
        if ((obj = o_on(id, mon->minvent)))
            return obj;

    /* search all levels */
    for (i = 0; i <= maxledgerno(); i++)
        if (levels[i] && (obj = find_oid_lev(levels[i], id)))
            return obj;

    /* not found at all */
    return NULL;
}


int
shop_item_cost(const struct obj *obj)
{
    struct monst *shkp;
    xchar x, y;
    int cost = 0;

    if (get_obj_location(obj, &x, &y, 0) &&
        (obj->unpaid ||
         (obj->where == OBJ_FLOOR && !obj->no_charge && costly_spot(x, y)))) {
        if (!
            (shkp =
             shop_keeper(obj->olev, *in_rooms(obj->olev, x, y, SHOPBASE))))
            return 0;
        if (!inhishop(shkp))
            return 0;
        if (!costly_spot(x, y))
            return 0;
        if (!*u.ushops)
            return 0;

        if (obj->oclass != COIN_CLASS) {
            cost = (obj == uball ||
                    obj == uchain) ? 0L : obj->quan * get_cost(obj, shkp);
            if (Has_contents(obj))
                cost += contained_cost(obj, shkp, 0L, FALSE, FALSE);
        }
    }
    return cost;
}


/* calculate the value that the shk will charge for [one of] an object */
static long
get_cost(const struct obj *obj, struct monst *shkp)
{       /* if angry, impose a surcharge */
    long tmp = getprice(obj, FALSE);

    if (!tmp)
        tmp = 5L;
    /* shopkeeper may notice if the player isn't very knowledgeable -
       especially when gem prices are concerned */
    if (!obj->dknown || !objects[obj->otyp].oc_name_known) {
        if (obj->oclass == GEM_CLASS &&
            objects[obj->otyp].oc_material == GLASS) {
            int i;

            /* get a value that's 'random' from game to game, but the same
               within the same game */
            boolean pseudorand =
                (((unsigned)u.ubirthday % obj->otyp) >= obj->otyp / 2);

            /* all gems are priced high - real or not */
            switch (obj->otyp - LAST_GEM) {
            case 1:    /* white */
                i = pseudorand ? DIAMOND : OPAL;
                break;
            case 2:    /* blue */
                i = pseudorand ? SAPPHIRE : AQUAMARINE;
                break;
            case 3:    /* red */
                i = pseudorand ? RUBY : JASPER;
                break;
            case 4:    /* yellowish brown */
                i = pseudorand ? AMBER : TOPAZ;
                break;
            case 5:    /* orange */
                i = pseudorand ? JACINTH : AGATE;
                break;
            case 6:    /* yellow */
                i = pseudorand ? CITRINE : CHRYSOBERYL;
                break;
            case 7:    /* black */
                i = pseudorand ? BLACK_OPAL : JET;
                break;
            case 8:    /* green */
                i = pseudorand ? EMERALD : JADE;
                break;
            case 9:    /* violet */
                i = pseudorand ? AMETHYST : FLUORITE;
                break;
            default:
                impossible("bad glass gem %d?", obj->otyp);
                i = STRANGE_OBJECT;
                break;
            }
            tmp = (long)objects[i].oc_cost;
        } else if (!(obj->o_id % 4))    /* arbitrarily impose surcharge */
            tmp += tmp / 3L;
    }
    if ((Role_if(PM_TOURIST) && u.ulevel < (MAXULEV / 2))
        || (uarmu && !uarm && !uarmc))  /* touristy shirt visible */
        tmp += tmp / 3L;
    else if (uarmh && uarmh->otyp == DUNCE_CAP)
        tmp += tmp / 3L;

    if (ACURR(A_CHA) > 18)
        tmp /= 2L;
    else if (ACURR(A_CHA) > 17)
        tmp -= tmp / 3L;
    else if (ACURR(A_CHA) > 15)
        tmp -= tmp / 4L;
    else if (ACURR(A_CHA) < 6)
        tmp *= 2L;
    else if (ACURR(A_CHA) < 8)
        tmp += tmp / 2L;
    else if (ACURR(A_CHA) < 11)
        tmp += tmp / 3L;
    if (tmp <= 0L)
        tmp = 1L;
    else if (obj->oartifact)
        tmp *= 4L;
    /* anger surcharge should match rile_shk's */
    if (shkp && ESHK(shkp)->surcharge)
        tmp += (tmp + 2L) / 3L;
    return tmp;
}


/* returns the price of a container's content.  the price
 * of the "top" container is added in the calling functions.
 * a different price quoted for selling as vs. buying.
 */
long
contained_cost(const struct obj *obj, struct monst *shkp, long price,
               boolean usell, boolean unpaid_only)
{
    struct obj *otmp;

    /* the price of contained objects */
    for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
        if (otmp->oclass == COIN_CLASS)
            continue;
        /* the "top" container is evaluated by caller */
        if (usell) {
            if (saleable(shkp, otmp) && !otmp->unpaid &&
                otmp->oclass != BALL_CLASS && !(otmp->oclass == FOOD_CLASS &&
                                                otmp->oeaten) &&
                !(Is_candle(otmp) &&
                  otmp->age < 20L * (long)objects[otmp->otyp].oc_cost))
                price += set_cost(otmp, shkp);
        } else if (!otmp->no_charge &&
                   (!unpaid_only || (unpaid_only && otmp->unpaid))) {
            price += get_cost(otmp, shkp) * otmp->quan;
        }

        if (Has_contents(otmp))
            price += contained_cost(otmp, shkp, price, usell, unpaid_only);
    }

    return price;
}

long
contained_gold(struct obj *obj)
{
    struct obj *otmp;
    long value = 0L;

    /* accumulate contained gold */
    for (otmp = obj->cobj; otmp; otmp = otmp->nobj)
        if (otmp->oclass == COIN_CLASS)
            value += otmp->quan;
        else if (Has_contents(otmp))
            value += contained_gold(otmp);

    return value;
}

static void
dropped_container(struct obj *obj, struct monst *shkp, boolean sale)
{
    struct obj *otmp;

    /* the "top" container is treated in the calling fn */
    for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
        if (otmp->oclass == COIN_CLASS)
            continue;

        if (!otmp->unpaid && !(sale && saleable(shkp, otmp)))
            otmp->no_charge = 1;

        if (Has_contents(otmp))
            dropped_container(otmp, shkp, sale);
    }
}

void
picked_container(struct obj *obj)
{
    struct obj *otmp;

    /* the "top" container is treated in the calling fn */
    for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
        if (otmp->oclass == COIN_CLASS)
            continue;

        if (otmp->no_charge)
            otmp->no_charge = 0;

        if (Has_contents(otmp))
            picked_container(otmp);
    }
}


/* calculate how much the shk will pay when buying [all of] an object */
static long
set_cost(struct obj *obj, struct monst *shkp)
{
    long tmp = getprice(obj, TRUE) * obj->quan;

    if ((Role_if(PM_TOURIST) && u.ulevel < (MAXULEV / 2))
        || (uarmu && !uarm && !uarmc))  /* touristy shirt visible */
        tmp /= 3L;
    else if (uarmh && uarmh->otyp == DUNCE_CAP)
        tmp /= 3L;
    else
        tmp /= 2L;

    /* shopkeeper may notice if the player isn't very knowledgeable -
       especially when gem prices are concerned */
    if (!obj->dknown || !objects[obj->otyp].oc_name_known) {
        if (obj->oclass == GEM_CLASS) {
            /* different shop keepers give different prices */
            if (objects[obj->otyp].oc_material == GEMSTONE ||
                objects[obj->otyp].oc_material == GLASS) {
                tmp = (obj->otyp % (6 - shkp->m_id % 3));
                tmp = (tmp + 3) * obj->quan;
            }
        } else if (tmp > 1L && !rn2(4))
            tmp -= tmp / 4L;
    }
    return tmp;
}


/* called from doinv(invent.c) for inventory of unpaid objects */
/* unp_obj: known to be unpaid */
long
unpaid_cost(const struct obj *unp_obj)
{
    struct bill_x *bp = NULL;
    struct monst *shkp;

    for (shkp = next_shkp(level->monlist, TRUE); shkp;
         shkp = next_shkp(shkp->nmon, TRUE))
        if ((bp = onbill(unp_obj, shkp, TRUE)) != 0)
            break;

    /* onbill() gave no message if unexpected problem occurred */
    if (!bp)
        impossible("unpaid_cost: object wasn't on any bill!");

    return bp ? unp_obj->quan * bp->price : 0L;
}

static void
add_one_tobill(struct obj *obj, boolean dummy)
{
    struct monst *shkp;
    struct bill_x *bp;
    int bct;
    char roomno = *u.ushops;

    if (!roomno)
        return;
    if (!(shkp = shop_keeper(level, roomno)))
        return;
    if (!inhishop(shkp))
        return;

    if (onbill(obj, shkp, FALSE) ||     /* perhaps thrown away earlier */
        (obj->oclass == FOOD_CLASS && obj->oeaten))
        return;

    /* TODO: Exploitable, in an amusing way. */
    if (ESHK(shkp)->billct == BILLSZ) {
        pline(msgc_consequence, "You got that for free!");
        return;
    }

    /* To recognize objects the shopkeeper is not interested in. -dgk */
    if (obj->no_charge) {
        obj->no_charge = 0;
        return;
    }

    bct = ESHK(shkp)->billct;
    bp = ESHK(shkp)->bill + bct;
    bp->bo_id = obj->o_id;
    bp->bquan = obj->quan;
    if (dummy) {        /* a dummy object must be inserted into */
        bp->useup = 1;  /* the billobjs chain here.  crucial for */
        add_to_billobjs(obj);   /* eating floorfood in shop.  see eat.c */
    } else
        bp->useup = 0;
    bp->price = get_cost(obj, shkp);
    ESHK(shkp)->billct++;
    obj->unpaid = 1;
}

static void
add_to_billobjs(struct obj *obj)
{
    if (obj->where != OBJ_FREE)
        panic("add_to_billobjs: obj not free");
    if (obj->timed)
        obj_stop_timers(obj);

    extract_nobj(obj, &turnstate.floating_objects,
                 &obj->olev->billobjs, OBJ_ONBILL);
}

/* recursive billing of objects within containers. */
static void
bill_box_content(struct obj *obj, boolean ininv, boolean dummy,
                 struct monst *shkp)
{
    struct obj *otmp;

    for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
        if (otmp->oclass == COIN_CLASS)
            continue;

        /* the "top" box is added in addtobill() */
        if (!otmp->no_charge)
            add_one_tobill(otmp, dummy);
        if (Has_contents(otmp))
            bill_box_content(otmp, ininv, dummy, shkp);
    }

}

/* shopkeeper tells you what you bought or sold, sometimes partly IDing it */
static void
shk_names_obj(struct monst *shkp, struct obj *obj,
              const char *fmt,     /* "%s %ld %s %s", doname(obj),
                                      amt, plur(amt), arg */
              long amt, const char *arg)
{
    const char *obj_name;
    boolean was_unknown = !obj->dknown;

    obj->dknown = TRUE;
    /* Use real name for ordinary weapons/armor, and spell-less scrolls/books
       (that is, blank and mail), but only if the object is within the shk's
       area of interest/expertise. */
    if (!objects[obj->otyp].oc_magic && saleable(shkp, obj) &&
        (obj->oclass == WEAPON_CLASS || obj->oclass == ARMOR_CLASS ||
         obj->oclass == SCROLL_CLASS || obj->oclass == SPBOOK_CLASS ||
         obj->otyp == MIRROR)) {
        was_unknown |= !objects[obj->otyp].oc_name_known;
        makeknown(obj->otyp);
    }
    obj_name = doname(obj);
    /* Use an alternate message when extra information is being provided */
    if (was_unknown) {
        const char *fmtbuf = msgprintf("%%s; %c%s", lowc(fmt[0]), fmt + 1);
        obj_name = msgupcasefirst(obj_name);
        pline(msgc_info, fmtbuf, obj_name, (obj->quan > 1) ? "them" : "it",
              amt, plur(amt), arg);
    } else {
        pline(msgc_info, fmt, obj_name, amt, plur(amt), arg);
    }
}

void
addtobill(struct obj *obj, boolean ininv, boolean dummy, boolean silent)
{
    struct monst *shkp;
    char roomno = *u.ushops;
    long ltmp = 0L, cltmp = 0L, gltmp = 0L;
    boolean container = Has_contents(obj);

    if (!*u.ushops)
        return;

    if (!(shkp = shop_keeper(level, roomno)))
        return;

    if (!inhishop(shkp))
        return;

    /* perhaps we threw it away earlier */
    if (onbill(obj, shkp, FALSE) || (obj->oclass == FOOD_CLASS && obj->oeaten))
        return;

    if (ESHK(shkp)->billct == BILLSZ) {
        pline(msgc_consequence, "You got that for free!");
        return;
    }

    if (obj->oclass == COIN_CLASS) {
        costly_gold(obj->ox, obj->oy, obj->quan);
        return;
    }

    if (!obj->no_charge)
        ltmp = get_cost(obj, shkp);

    if (obj->no_charge && !container) {
        obj->no_charge = 0;
        return;
    }

    if (container) {
        if (obj->cobj == NULL) {
            if (obj->no_charge) {
                obj->no_charge = 0;
                return;
            } else {
                add_one_tobill(obj, dummy);
                goto speak;
            }
        } else {
            cltmp += contained_cost(obj, shkp, cltmp, FALSE, FALSE);
            gltmp += contained_gold(obj);
        }

        if (ltmp)
            add_one_tobill(obj, dummy);
        if (cltmp)
            bill_box_content(obj, ininv, dummy, shkp);
        picked_container(obj);  /* reset contained obj->no_charge */

        ltmp += cltmp;

        if (gltmp) {
            costly_gold(obj->ox, obj->oy, gltmp);
            if (!ltmp)
                return;
        }

        if (obj->no_charge)
            obj->no_charge = 0;

    } else      /* i.e., !container */
        add_one_tobill(obj, dummy);
speak:
    if (shkp->mcanmove && !shkp->msleeping && !silent) {
        const char *buf;

        if (!ltmp) {
            pline(msgc_noconsequence, "%s has no interest in %s.",
                  Monnam(shkp), the(xname(obj)));
            return;
        }
        buf = "\"For you, ";
        if (ANGRY(shkp))
            buf = msgcat(buf, "scum");
        else {
            static const char *const honored[5] = {
                "good", "honored", "most gracious", "esteemed",
                "most renowned and sacred"
            };
            buf = msgcat(buf, honored[rn2(4) + u.uevent.udemigod]);
            if (!is_human(youmonst.data))
                buf = msgcat(buf, " creature");
            else
                buf = msgcat(buf, (u.ufemale) ? " lady" : " sir");
        }
        if (ininv) {
            long quan = obj->quan;

            obj->quan = 1L;     /* fool xname() into giving singular */
            pline(msgc_unpaid, "%s; only %ld %s %s.\"", buf, ltmp,
                  (quan > 1L) ? "per" : "for this", xname(obj));
            obj->quan = quan;
        } else
            pline(msgc_unpaid, "%s will cost you %ld %s%s.",
                  The(xname(obj)), ltmp, currency(ltmp),
                  (obj->quan > 1L) ? " each" : "");
    } else if (!silent) {
        if (ltmp)
            pline(msgc_unpaid, "The list price of %s is %ld %s%s.",
                  the(xname(obj)), ltmp, currency(ltmp),
                  (obj->quan > 1L) ? " each" : "");
        else
            pline(msgc_monneutral, "%s does not notice.", Monnam(shkp));
    }
}

void
splitbill(struct obj *obj, struct obj *otmp)
{
    /* otmp has been split off from obj */
    struct bill_x *bp;
    long tmp;
    struct monst *shkp = shop_keeper(level, *u.ushops);

    if (!shkp || !inhishop(shkp)) {
        impossible("splitbill: no resident shopkeeper??");
        return;
    }
    bp = onbill(obj, shkp, FALSE);
    if (!bp) {
        impossible("splitbill: not on bill?");
        return;
    }
    if (bp->bquan < otmp->quan) {
        impossible("Negative quantity on bill??");
    }
    if (bp->bquan == otmp->quan) {
        impossible("Zero quantity on bill??");
    }
    bp->bquan -= otmp->quan;

    if (ESHK(shkp)->billct == BILLSZ)
        otmp->unpaid = 0;
    else {
        tmp = bp->price;
        bp = ESHK(shkp)->bill + ESHK(shkp)->billct;
        bp->bo_id = otmp->o_id;
        bp->bquan = otmp->quan;
        bp->useup = 0;
        bp->price = tmp;
        ESHK(shkp)->billct++;
    }
}

static void
sub_one_frombill(struct obj *obj, struct monst *shkp)
{
    struct bill_x *bp;

    if ((bp = onbill(obj, shkp, FALSE)) != 0) {
        struct obj *otmp;

        obj->unpaid = 0;
        if (bp->bquan > obj->quan) {
            otmp = newobj(0, obj);
            bp->bo_id = otmp->o_id = next_ident();
            otmp->quan = (bp->bquan -= obj->quan);
            otmp->owt = 0;      /* superfluous */
            otmp->onamelth = 0;
            otmp->oxlth = 0;
            otmp->oattached = OATTACHED_NOTHING;
            bp->useup = 1;
            add_to_billobjs(otmp);
            return;
        }
        ESHK(shkp)->billct--;
        *bp = ESHK(shkp)->bill[ESHK(shkp)->billct];

        return;
    } else if (obj->unpaid) {
        impossible("sub_one_frombill: unpaid object not on bill");
        obj->unpaid = 0;
    }
}

/* recursive check of unpaid objects within nested containers. */
void
subfrombill(struct obj *obj, struct monst *shkp)
{
    struct obj *otmp;

    sub_one_frombill(obj, shkp);

    if (Has_contents(obj))
        for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
            if (otmp->oclass == COIN_CLASS)
                continue;

            if (Has_contents(otmp))
                subfrombill(otmp, shkp);
            else
                sub_one_frombill(otmp, shkp);
        }
}


static long
stolen_container(struct obj *obj, struct monst *shkp, long price, boolean ininv)
{
    struct obj *otmp;

    if (ininv && obj->unpaid)
        price += get_cost(obj, shkp);
    else {
        if (!obj->no_charge)
            price += get_cost(obj, shkp);
        obj->no_charge = 0;
    }

    /* the price of contained objects, if any */
    for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {

        if (otmp->oclass == COIN_CLASS)
            continue;

        if (!Has_contents(otmp)) {
            if (ininv) {
                if (otmp->unpaid)
                    price += otmp->quan * get_cost(otmp, shkp);
            } else {
                if (!otmp->no_charge) {
                    if (otmp->oclass != FOOD_CLASS || !otmp->oeaten)
                        price += otmp->quan * get_cost(otmp, shkp);
                }
                otmp->no_charge = 0;
            }
        } else
            price += stolen_container(otmp, shkp, price, ininv);
    }

    return price;
}


long
stolen_value(struct obj *obj, xchar x, xchar y, boolean peaceful,
             boolean silent)
{
    long value = 0L, gvalue = 0L;
    struct monst *shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE));

    if (!shkp || !inhishop(shkp))
        return 0L;

    if (obj->oclass == COIN_CLASS) {
        gvalue += obj->quan;
    } else if (Has_contents(obj)) {
        boolean ininv = ! !count_unpaid(obj->cobj);

        value += stolen_container(obj, shkp, value, ininv);
        if (!ininv)
            gvalue += contained_gold(obj);
    } else if (!obj->no_charge && saleable(shkp, obj)) {
        value += get_cost(obj, shkp);
    }

    if (gvalue + value == 0L)
        return 0L;

    value += gvalue;

    if (peaceful) {
        boolean credit_use = ! !ESHK(shkp)->credit;

        value = check_credit(value, shkp);
        /* 'peaceful' affects general treatment, but doesn't affect the fact
           that other code expects that all charges after the shopkeeper is
           angry are included in robbed, not debit */
        if (ANGRY(shkp))
            ESHK(shkp)->robbed += value;
        else
            ESHK(shkp)->debit += value;

        if (!silent) {
            const char *still = "";

            if (credit_use) {
                if (ESHK(shkp)->credit) {
                    pline(msgc_info, "You have %ld %s credit remaining.",
                          (long)ESHK(shkp)->credit,
                          currency(ESHK(shkp)->credit));
                    return value;
                } else if (!value) {
                    pline(msgc_info, "You have no credit remaining.");
                    return 0;
                }
                still = "still ";
            }
            if (obj->oclass == COIN_CLASS)
                pline(msgc_info, "You %sowe %s %ld %s!", still,
                      mon_nam(shkp), value, currency(value));
            else
                pline(msgc_info, "You %sowe %s %ld %s for %s!", still,
                      mon_nam(shkp), value, currency(value),
                      obj->quan > 1L ? "them" : "it");
        }
    } else {
        ESHK(shkp)->robbed += value;

        if (!silent) {
            if (cansee(shkp->mx, shkp->my)) {
                pline_once(msgc_npcanger,
                           "%s booms: \"%s, you are a thief!\"",
                           Monnam(shkp), u.uplname);
            } else
                pline_once(msgc_npcanger, "You hear a scream, \"Thief!\"");
        }
        hot_pursuit(shkp);
        angry_guards(FALSE);
    }
    return value;
}

/* auto-response flag for/from "sell foo?" 'a' => 'y', 'q' => 'n' */
static char sell_response = 'a';
static int sell_how = SELL_NORMAL;

/* can't just use sell_response='y' for auto_credit because the 'a' response
   shouldn't carry over from ordinary selling to credit selling */
static boolean auto_credit = FALSE;

void
sellobj_state(int deliberate)
{
    /* If we're deliberately dropping something, there's no automatic response
       to the shopkeeper's "want to sell" query; however, if we accidentally
       drop anything, the shk will buy it/them without asking. This retains the 
       old pre-query risk that slippery fingers while in shops entailed: you
       drop it, you've lost it. */
    sell_response = (deliberate != SELL_NORMAL) ? '\0' : 'a';
    sell_how = deliberate;
    auto_credit = FALSE;
}

void
sellobj(struct obj *obj, xchar x, xchar y)
{
    struct monst *shkp;
    struct eshk *eshkp;
    long ltmp = 0L, cltmp = 0L, gltmp = 0L, offer;
    boolean saleitem, cgold = FALSE, container = Has_contents(obj);
    boolean isgold = (obj->oclass == COIN_CLASS);
    boolean only_partially_your_contents = FALSE;

    if (!(shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE))) ||
        !inhishop(shkp))
        return;
    if (!costly_spot(x, y))
        return;
    if (!*u.ushops)
        return;

    if (obj->unpaid && !container && !isgold) {
        sub_one_frombill(obj, shkp);
        return;
    }
    if (container) {
        /* find the price of content before subfrombill */
        cltmp += contained_cost(obj, shkp, cltmp, TRUE, FALSE);
        /* find the value of contained gold */
        gltmp += contained_gold(obj);
        cgold = (gltmp > 0L);
    }

    saleitem = saleable(shkp, obj);
    if (!isgold && !obj->unpaid && saleitem)
        ltmp = set_cost(obj, shkp);

    offer = ltmp + cltmp;

    /* get one case out of the way: nothing to sell, and no gold */
    if (!isgold && ((offer + gltmp) == 0L || sell_how == SELL_DONTSELL)) {
        boolean unpaid = (obj->unpaid ||
                          (container && count_unpaid(obj->cobj)));

        if (container) {
            dropped_container(obj, shkp, FALSE);
            if (!obj->unpaid && !saleitem)
                obj->no_charge = 1;
            if (obj->unpaid || count_unpaid(obj->cobj))
                subfrombill(obj, shkp);
        } else
            obj->no_charge = 1;

        if (!unpaid && (sell_how != SELL_DONTSELL))
            pline(msgc_noconsequence, "%s seems uninterested.", Monnam(shkp));
        return;
    }

    /* you dropped something of your own - probably want to sell it */
    rouse_shk(shkp, TRUE);      /* wake up sleeping or paralyzed shk */
    eshkp = ESHK(shkp);

    if (ANGRY(shkp)) {  /* they become shop-objects, no pay */
        pline(msgc_substitute, "Thank you, scum!");
        subfrombill(obj, shkp);
        return;
    }

    if (eshkp->robbed) {        /* shkp is not angry? */
        if (isgold)
            offer = obj->quan;
        else if (cgold)
            offer += cgold;
        if ((eshkp->robbed -= offer < 0L))
            eshkp->robbed = 0L;
        if (offer)
            verbalize(msgc_actionok, "Thank you for your contribution "
                      "to restock this recently plundered shop.");
        subfrombill(obj, shkp);
        return;
    }

    if (isgold || cgold) {
        if (!cgold)
            gltmp = obj->quan;

        if (eshkp->debit >= gltmp) {
            if (eshkp->loan) {  /* you carry shop's gold */
                if (eshkp->loan >= gltmp)
                    eshkp->loan -= gltmp;
                else
                    eshkp->loan = 0L;
            }
            eshkp->debit -= gltmp;
            pline(msgc_actionok, "Your debt is %spaid off.",
                  eshkp->debit ? "partially " : "");
        } else {
            long delta = gltmp - eshkp->debit;

            eshkp->credit += delta;
            if (eshkp->debit) {
                eshkp->debit = 0L;
                eshkp->loan = 0L;
                pline(msgc_actionok, "Your debt is paid off.");
            }
            pline(msgc_actionok, "%ld %s %s added to your credit.", delta,
                  currency(delta), delta > 1L ? "are" : "is");
        }
        if (offer)
            goto move_on;
        else {
            if (!isgold) {
                if (container)
                    dropped_container(obj, shkp, FALSE);
                if (!obj->unpaid && !saleitem)
                    obj->no_charge = 1;
                subfrombill(obj, shkp);
            }
            return;
        }
    }
move_on:
    if ((!saleitem && !(container && cltmp > 0L))
        || eshkp->billct == BILLSZ || obj->oclass == BALL_CLASS ||
        obj->oclass == CHAIN_CLASS || offer == 0L ||
        (obj->oclass == FOOD_CLASS && obj->oeaten) ||
        (Is_candle(obj) && obj->age < 20L * (long)objects[obj->otyp].oc_cost)) {
        pline(msgc_noconsequence, "%s seems uninterested%s.", Monnam(shkp),
              cgold ? " in the rest" : "");
        if (container)
            dropped_container(obj, shkp, FALSE);
        obj->no_charge = 1;
        return;
    }

    if (!money_cnt(shkp->minvent)) {
        char c;
        const char *qbuf;
        long tmpcr = ((offer * 9L) / 10L) + (offer <= 1L);

        if (sell_how == SELL_NORMAL || auto_credit) {
            c = sell_response = 'y';
        } else if (sell_response != 'n') {
            pline(msgc_substitute, "%s cannot pay you at present.",
                  Monnam(shkp));
            qbuf = msgprintf("Will you accept %ld %s in credit for %s?",
                             tmpcr, currency(tmpcr), doname(obj));
            /* won't accept 'a' response here */
            /* KLY - 3/2000 yes, we will, it's a damn nuisance to have to
               constantly hit 'y' to sell for credit */
            c = ynaq(qbuf);
            if (c == 'a') {
                c = 'y';
                auto_credit = TRUE;
            }
        } else  /* previously specified "quit" */
            c = 'n';

        if (c == 'y') {
            shk_names_obj(shkp, obj,
                          (sell_how !=
                           SELL_NORMAL) ?
                          "You traded %s for %ld zorkmid%s in %scredit." :
                          "You relinquish %s and acquire %ld zorkmid%s in "
                          "%scredit.", tmpcr,
                          (eshkp->credit > 0L) ? "additional " : "");
            eshkp->credit += tmpcr;
            subfrombill(obj, shkp);
        } else {
            if (c == 'q')
                sell_response = 'n';
            if (container)
                dropped_container(obj, shkp, FALSE);
            if (!obj->unpaid)
                obj->no_charge = 1;
            subfrombill(obj, shkp);
        }
    } else {
        const char *qbuf;
        long shkmoney = money_cnt(shkp->minvent);
        boolean short_funds = (offer > shkmoney);

        if (short_funds)
            offer = shkmoney;
        if (!sell_response) {
            only_partially_your_contents =
                (contained_cost(obj, shkp, 0L, FALSE, FALSE) !=
                 contained_cost(obj, shkp, 0L, FALSE, TRUE));
            qbuf = msgprintf("%s offers%s %ld gold piece%s for%s %s %s.  "
                             "Sell %s?", Monnam(shkp), short_funds ?
                             " only" : "", offer, plur(offer),
                             (!ltmp && cltmp && only_partially_your_contents) ?
                             " your items in" : (!ltmp && cltmp) ?
                             " the contents of" : "",
                             obj->unpaid ? "the" : "your", cxname(obj),
                             (obj->quan == 1L &&
                              !(!ltmp && cltmp && only_partially_your_contents))
                             ? "it" : "them");
        } else
            qbuf = "";     /* just to pacify lint */

        switch (sell_response ? sell_response : ynaq(qbuf)) {
        case 'q':
            sell_response = 'n';
        case 'n':
            if (container)
                dropped_container(obj, shkp, FALSE);
            if (!obj->unpaid)
                obj->no_charge = 1;
            subfrombill(obj, shkp);
            break;
        case 'a':
            sell_response = 'y';
        case 'y':
            if (container)
                dropped_container(obj, shkp, TRUE);
            if (!obj->unpaid && !saleitem)
                obj->no_charge = 1;
            subfrombill(obj, shkp);
            pay(-offer, shkp);
            shk_names_obj(
                shkp, obj,
                (sell_how != SELL_NORMAL) ?
                (!ltmp && cltmp && only_partially_your_contents) ?
                "You sold some items inside %s for %ld gold piece%s.%s" :
                "You sold %s for %ld gold piece%s.%s" :
                "You relinquish %s and receive %ld gold piece%s "
                "in compensation.%s", offer, "");
            break;
        default:
            impossible("invalid sell response");
        }
    }
}

/* mode:  0: deliver count 1: paged */
int
doinvbill(int mode)
{
    struct monst *shkp;
    struct eshk *eshkp;
    struct bill_x *bp, *end_bp;
    struct obj *obj;
    long totused;
    const char *buf_p;
    struct nh_menulist menu;

    shkp = shop_keeper(level, *u.ushops);
    if (!shkp || !inhishop(shkp)) {
        if (mode != 0)
            impossible("doinvbill: no shopkeeper?");
        return 0;
    }
    eshkp = ESHK(shkp);

    if (eshkp->bill_inactive) {
        impossible("doinvbill: inactive bill");
        return 0;
    }

    if (mode == 0) {
        /* count expended items, so that the `I' command can decide whether to
           include 'x' in its prompt string */
        int cnt = !eshkp->debit ? 0 : 1;

        for (bp = eshkp->bill, end_bp = eshkp->bill + eshkp->billct;
             bp < end_bp; bp++)
            if (bp->useup ||
                ((obj = bp_to_obj(bp)) != 0 && obj->quan < bp->bquan))
                cnt++;
        return cnt;
    }

    init_menulist(&menu);
    add_menutext(&menu, "Unpaid articles already used up:");
    add_menutext(&menu, "");

    totused = 0L;
    for (bp = eshkp->bill, end_bp = eshkp->bill + eshkp->billct;
         bp < end_bp; bp++) {
        obj = bp_to_obj(bp);
        if (!obj) {
            impossible("Bad shopkeeper administration.");
            dealloc_menulist(&menu);
            return 0;
        }
        if (bp->useup || bp->bquan > obj->quan) {
            long oquan, uquan, thisused;
            unsigned save_unpaid;

            save_unpaid = obj->unpaid;
            oquan = obj->quan;
            uquan = (bp->useup ? bp->bquan : bp->bquan - oquan);
            thisused = bp->price * uquan;
            totused += thisused;
            obj->unpaid = 0;    /* ditto */
            /* Why 'x'? To match `I x', more or less. */
            buf_p = xprname(obj, NULL, 'x', FALSE, thisused, uquan);
            obj->unpaid = save_unpaid;
            add_menutext(&menu, buf_p);
        }
    }
    if (eshkp->debit) {
        /* additional shop debt which has no itemization available */
        if (totused)
            add_menutext(&menu, "");
        totused += eshkp->debit;
        buf_p =
            xprname(NULL, "usage charges and/or other fees", GOLD_SYM, FALSE,
                    eshkp->debit, 0L);
        add_menutext(&menu, buf_p);
    }
    buf_p = xprname(NULL, "Total:", '*', FALSE, totused, 0L);
    add_menutext(&menu, "");
    add_menutext(&menu, buf_p);

    display_menu(&menu, NULL, PICK_NONE, PLHINT_CONTAINER,
                 NULL);
    return 0;
}

#define HUNGRY  2

static long
getprice(const struct obj *obj, boolean shk_buying)
{
    long tmp = (long)objects[obj->otyp].oc_cost;

    if (obj->oartifact) {
        tmp = arti_cost(obj);
        if (shk_buying)
            tmp /= 4;
    }
    switch (obj->oclass) {
    case FOOD_CLASS:
        /* simpler hunger check, (2-4)*cost */
        if (u.uhs >= HUNGRY && !shk_buying)
            tmp *= (long)u.uhs;
        if (obj->oeaten)
            tmp = 0L;
        break;
    case WAND_CLASS:
        if (obj->spe == -1)
            tmp = 0L;
        break;
    case POTION_CLASS:
        if (obj->otyp == POT_WATER && !obj->blessed && !obj->cursed)
            tmp = 0L;
        break;
    case ARMOR_CLASS:
    case WEAPON_CLASS:
        if (obj->spe > 0)
            tmp += 10L * (long)obj->spe;
        break;
    case TOOL_CLASS:
        if (Is_candle(obj) && obj->age < 20L * (long)objects[obj->otyp].oc_cost)
            tmp /= 2L;
        break;
    }
    return tmp;
}

/* shk catches thrown pick-axe */
struct monst *
shkcatch(struct obj *obj, xchar x, xchar y)
{
    struct monst *shkp;

    if (!(shkp = shop_keeper(level, inside_shop(level, x, y))) ||
        !inhishop(shkp))
        return 0;

    if (shkp->mcanmove && !shkp->msleeping &&
        (*u.ushops != ESHK(shkp)->shoproom || !inside_shop(level, u.ux, u.uy))
        && dist2(shkp->mx, shkp->my, x, y) < 3 &&
        /* if it is the shk's pos, you hit and anger him */
        (shkp->mx != x || shkp->my != y)) {
        if (mnearto(shkp, x, y, TRUE))
            verbalize(msgc_npcvoice, "Out of my way, scum!");
        if (cansee(x, y)) {
            pline(msgc_itemloss, "%s nimbly%s catches %s.", Monnam(shkp),
                  (x == shkp->mx && y == shkp->my) ? "" : " reaches over and",
                  the(xname(obj)));
            if (!canspotmon(shkp))
                map_invisible(x, y);
            win_delay_output();
        }
        subfrombill(obj, shkp);
        mpickobj(shkp, obj);
        return shkp;
    }
    return NULL;
}

void
add_damage(xchar x, xchar y, long cost)
{
    struct damage *tmp_dam;
    char *shops;

    if (IS_DOOR(level->locations[x][y].typ)) {
        struct monst *mtmp;

        /* Don't schedule for repair unless it's a real shop entrance */
        for (shops = in_rooms(level, x, y, SHOPBASE); *shops; shops++)
            if ((mtmp = shop_keeper(level, *shops)) != 0 &&
                x == ESHK(mtmp)->shd.x && y == ESHK(mtmp)->shd.y)
                break;
        if (!*shops)
            return;
    }
    for (tmp_dam = level->damagelist; tmp_dam; tmp_dam = tmp_dam->next)
        if (tmp_dam->place.x == x && tmp_dam->place.y == y) {
            tmp_dam->cost += cost;
            return;
        }
    tmp_dam = malloc((unsigned)sizeof (struct damage));
    tmp_dam->when = moves;
    tmp_dam->place.x = x;
    tmp_dam->place.y = y;
    tmp_dam->cost = cost;
    tmp_dam->typ = level->locations[x][y].typ;
    tmp_dam->next = level->damagelist;
    level->damagelist = tmp_dam;
    /* If player saw damage, display as a wall forever */
    if (cansee(x, y))
        level->locations[x][y].seenv = SVALL;
}


/*
 * Do something about damage. Either (!croaked) try to repair it, or
 * (croaked) just discard damage structs for non-shared locations, since
 * they'll never get repaired. Assume that shared locations will get
 * repaired eventually by the other shopkeeper(s). This might be an erroneous
 * assumption (they might all be dead too), but we have no reasonable way of
 * telling that.
 */
static void
remove_damage(struct monst *shkp, boolean croaked)
{
    struct damage *tmp_dam, *tmp2_dam;
    boolean did_repair = FALSE, saw_door = FALSE;
    boolean saw_floor = FALSE;
    boolean saw_untrap = FALSE;
    uchar saw_walls = 0;
    struct level *lev = levels[ledger_no(&ESHK(shkp)->shoplevel)];

    tmp_dam = lev->damagelist;
    tmp2_dam = 0;
    while (tmp_dam) {
        xchar x = tmp_dam->place.x, y = tmp_dam->place.y;
        char shops[5];
        int disposition;

        disposition = 0;
        strcpy(shops, in_rooms(lev, x, y, SHOPBASE));
        if (strchr(shops, ESHK(shkp)->shoproom)) {
            if (croaked)
                disposition = (shops[1]) ? 0 : 1;
            else
                disposition = repair_damage(lev, shkp, tmp_dam, FALSE);
        }

        if (!disposition) {
            tmp2_dam = tmp_dam;
            tmp_dam = tmp_dam->next;
            continue;
        }

        if (disposition > 1) {
            did_repair = TRUE;
            if (cansee(x, y)) {
                if (IS_WALL(lev->locations[x][y].typ))
                    saw_walls++;
                else if (IS_DOOR(lev->locations[x][y].typ))
                    saw_door = TRUE;
                else if (disposition == 3)      /* untrapped */
                    saw_untrap = TRUE;
                else
                    saw_floor = TRUE;
            }
        }

        tmp_dam = tmp_dam->next;
        if (!tmp2_dam) {
            free(lev->damagelist);
            lev->damagelist = tmp_dam;
        } else {
            free(tmp2_dam->next);
            tmp2_dam->next = tmp_dam;
        }
    }
    if (!did_repair)
        return;
    if (saw_walls) {
        pline(msgc_monneutral, "Suddenly, %s section%s of wall close%s up!",
              (saw_walls == 1) ? "a" : (saw_walls <= 3) ? "some" : "several",
              (saw_walls == 1) ? "" : "s", (saw_walls == 1) ? "s" : "");
        if (saw_door)
            pline(msgc_monneutral, "The shop door reappears!");
        if (saw_floor)
            pline(msgc_monneutral, "The floor is repaired!");
    } else {
        if (saw_door)
            pline(msgc_monneutral, "Suddenly, the shop door reappears!");
        else if (saw_floor)
            pline(msgc_monneutral, "Suddenly, the floor damage is gone!");
        else if (saw_untrap)
            pline(msgc_monneutral,
                  "Suddenly, the trap is removed from the floor!");
        else if (inside_shop(level, u.ux, u.uy) == ESHK(shkp)->shoproom)
            pline(msgc_levelsound, "You feel more claustrophobic than before.");
        else if (canhear() && !rn2(10))
            pline_once(msgc_levelsound,
                       "The dungeon acoustics noticeably change.");
    }
}

/*
 * 0: repair postponed, 1: silent repair (no messages), 2: normal repair
 * 3: untrap
 */
int
repair_damage(struct level *lev, struct monst *shkp, struct damage *tmp_dam,
              boolean catchup)
{       /* restoring a level */
    xchar x, y, i;
    xchar litter[9];
    struct monst *mtmp;
    struct obj *otmp;
    struct trap *ttmp;

    if ((moves - tmp_dam->when) < REPAIR_DELAY)
        return 0;
    if (shkp->msleeping || !shkp->mcanmove || ESHK(shkp)->following)
        return 0;
    x = tmp_dam->place.x;
    y = tmp_dam->place.y;
    if (!IS_ROOM(tmp_dam->typ)) {
        if (x == u.ux && y == u.uy)
            if (!Passes_walls)
                return 0;
        if (x == shkp->mx && y == shkp->my)
            return 0;
        if ((mtmp = m_at(lev, x, y)) && (!passes_walls(mtmp->data)))
            return 0;
    }
    if ((ttmp = t_at(lev, x, y)) != 0) {
        boolean floordamage = FALSE;

        if (x == u.ux && y == u.uy)
            if (!Passes_walls)
                return 0;
        if (ttmp->ttyp == LANDMINE || ttmp->ttyp == BEAR_TRAP) {
            /* convert to an object */
            otmp =
                mksobj(lev, (ttmp->ttyp == LANDMINE) ? LAND_MINE : BEARTRAP,
                       TRUE, FALSE, rng_main);
            otmp->quan = 1;
            otmp->owt = weight(otmp);
            mpickobj(shkp, otmp);
        } else if (ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT ||
                   ttmp->ttyp == HOLE)
            floordamage = TRUE;
        deltrap(lev, ttmp);
        if (IS_DOOR(tmp_dam->typ)) {
            lev->locations[x][y].doormask = D_CLOSED;   /* arbitrary */
            block_point(x, y);
        } else if (IS_WALL(tmp_dam->typ)) {
            lev->locations[x][y].typ = tmp_dam->typ;
            block_point(x, y);
        }
        if (lev == level)
            newsym(x, y);
        return floordamage ? 2 : 3;
    }
    if (IS_ROOM(tmp_dam->typ)) {
        /* No messages, because player already filled trap door */
        return 1;
    }
    if ((tmp_dam->typ == lev->locations[x][y].typ) &&
        (!IS_DOOR(tmp_dam->typ) || (lev->locations[x][y].doormask > D_BROKEN)))
        /* No messages if player already replaced shop door */
        return 1;
    lev->locations[x][y].typ = tmp_dam->typ;
    memset(litter, 0, sizeof (litter));
    if ((otmp = lev->objects[x][y]) != 0) {
        /* Scatter objects haphazardly into the shop */
#define NEED_UPDATE 1
#define OPEN        2
#define INSHOP      4
#define horiz(i)    ((i%3)-1)
#define vert(i)     ((i/3)-1)
        for (i = 0; i < 9; i++) {
            if ((i == 4) ||
                (!ZAP_POS(lev->locations[x + horiz(i)][y + vert(i)].typ)))
                continue;
            litter[i] = OPEN;
            if (inside_shop(lev, x + horiz(i), y + vert(i)) ==
                ESHK(shkp)->shoproom)
                litter[i] |= INSHOP;
        }
        if (Punished && !Engulfed &&
            ((uchain->ox == x && uchain->oy == y) ||
             (uball->ox == x && uball->oy == y))) {
            /* 
             * Either the ball or chain is in the repair location.
             *
             * Take the easy way out and put ball&chain under hero.
             */
            verbalize(msgc_npcvoice, "Get your junk out of my wall!");
            unplacebc();        /* pick 'em up */
            placebc();  /* put 'em down */
        }
        while ((otmp = lev->objects[x][y]) != 0)
            /* Don't mess w/ boulders -- just merge into wall */
            if ((otmp->otyp == BOULDER) || (otmp->otyp == ROCK)) {
                obj_extract_self(otmp);
                obfree(otmp, NULL);
            } else {
                while (!(litter[i = rn2(9)] & INSHOP))
                    ;
                remove_object(otmp);
                place_object(otmp, lev, x + horiz(i), y + vert(i));
                litter[i] |= NEED_UPDATE;
            }
    }
    if (catchup)
        return 1;       /* repair occurred while off level */

    block_point(x, y);
    if (IS_DOOR(tmp_dam->typ)) {
        lev->locations[x][y].doormask = D_CLOSED;       /* arbitrary */
        if (lev == level)
            newsym(x, y);
    } else {
        /* don't set doormask - it is (hopefully) the same as it was */
        /* if not, perhaps save it with the damage array...  */

        if (IS_WALL(tmp_dam->typ) && cansee(x, y)) {
            /* Player sees actual repair process, so they KNOW it's a wall */
            lev->locations[x][y].seenv = SVALL;
            if (lev == level)
                newsym(x, y);
        }
        /* Mark this wall as "repaired".  There currently is no code */
        /* to do anything about repaired walls, so don't do it.  */
    }
    for (i = 0; i < 9; i++)
        if (litter[i] & NEED_UPDATE && lev == level)
            newsym(x + horiz(i), y + vert(i));
    return 2;
#undef NEED_UPDATE
#undef OPEN
#undef INSHOP
#undef vert
#undef horiz
}

/*
 * shk_move: return 1: moved  0: didn't  -1: let m_move do it  -2: died
 */
int
shk_move(struct monst *shkp)
{
    xchar gx, gy, omx, omy;
    int udist;
    schar appr;
    struct eshk *eshkp = ESHK(shkp);
    int z;
    boolean uondoor = FALSE, satdoor, avoid = FALSE, badinv;

    omx = shkp->mx;
    omy = shkp->my;

    if (inhishop(shkp))
        remove_damage(shkp, FALSE);

    if ((udist = distu(omx, omy)) < 3 &&
        (shkp->data != &mons[PM_GRID_BUG] || (omx == u.ux || omy == u.uy))) {
        if (ANGRY(shkp) || (Conflict && !resist(shkp, RING_CLASS, 0, 0))) {
            if (Displaced)
                pline(msgc_notresisted,
                      "Your displaced image doesn't fool %s!", mon_nam(shkp));
            mattacku(shkp);
            return 0;
        }
        if (eshkp->following) {
            if (strncmp(eshkp->customer, u.uplname, PL_NSIZ)) {
                verbalize(msgc_npcvoice, "%s, %s!  I was looking for %s.",
                          Hello(shkp), u.uplname, eshkp->customer);
                eshkp->following = 0;
                return 0;
            }
            if (moves > followmsg + 4) {
                verbalize(msgc_npcvoice, "%s, %s!  Didn't you forget to pay?",
                          Hello(shkp), u.uplname);
                followmsg = moves;
                if (!rn2(9)) {
                    pline(msgc_npcanger,
                          "%s doesn't like customers who don't pay.",
                          Monnam(shkp));
                    rile_shk(shkp);
                }
            }
            if (udist < 2)
                return 0;
        }
    }

    appr = 1;
    gx = eshkp->shk.x;
    gy = eshkp->shk.y;
    satdoor = (gx == omx && gy == omy);
    if (eshkp->following || ((z = holetime()) >= 0 && z * z <= udist)) {
        /* [This distance check used to apply regardless of whether the shk was 
           following, but that resulted in m_move() sometimes taking the shk
           out of the shop if the player had fenced him in with boulders or
           traps. Such voluntary abandonment left unpaid objects in invent,
           triggering billing impossibilities on the next level once the
           character fell through the hole.] */
        if (udist > 4 && eshkp->following)
            return -1;  /* leave it to m_move */
        gx = u.ux;
        gy = u.uy;
    } else if (ANGRY(shkp)) {
        /* Move towards the hero if the shopkeeper can sense him. */
        if (shkp->mcansee && m_cansenseu(shkp)) {
            gx = u.ux;
            gy = u.uy;
        }
        avoid = FALSE;
    } else {
#define GDIST(x,y)  (dist2(x,y,gx,gy))
        if (Invis || u.usteed) {
            avoid = FALSE;
        } else {
            uondoor = (u.ux == eshkp->shd.x && u.uy == eshkp->shd.y);
            if (uondoor) {
                badinv = (carrying(PICK_AXE) || carrying(DWARVISH_MATTOCK) ||
                          (Fast &&
                           (sobj_at(PICK_AXE, level, u.ux, u.uy) ||
                            sobj_at(DWARVISH_MATTOCK, level, u.ux, u.uy))));
                if (satdoor && badinv)
                    return 0;
                avoid = !badinv;
            } else {
                avoid = (*u.ushops && distu(gx, gy) > 8);
                badinv = FALSE;
            }

            if (((!eshkp->robbed && !eshkp->billct && !eshkp->debit)
                 || avoid) && GDIST(omx, omy) < 3) {
                if (!badinv && !onlineu(omx, omy))
                    return 0;
                if (satdoor)
                    appr = gx = gy = 0;
            }
        }
    }

    z = move_special(shkp, inhishop(shkp), appr, uondoor, avoid, omx, omy, gx,
                     gy);
    if (z > 0)
        after_shk_move(shkp);

    return z;
}

/* called after shopkeeper moves, in case the move causes re-entry into shop */
void
after_shk_move(struct monst *shkp)
{
    struct eshk *eshkp = ESHK(shkp);

    if (eshkp->bill_inactive && inhishop(shkp)) {
        /* reactivate bill, need to re-calc player's occupancy too */
        eshkp->bill_inactive = FALSE;
        check_special_room(FALSE);
    }
}


/* for use in levl_follower (mondata.c) */
boolean
is_fshk(struct monst *mtmp)
{
    return (boolean) (mtmp->isshk && ESHK(mtmp)->following);
}

/* You are digging in the shop. */
void
shopdig(int fall)
{
    struct monst *shkp = shop_keeper(level, *u.ushops);
    int lang;
    const char *grabs = "grabs";

    if (!shkp)
        return;

    /* 0 == can't speak, 1 == makes animal noises, 2 == speaks */
    lang = 0;
    if (shkp->msleeping || !shkp->mcanmove || is_silent(shkp->data))
        ;  /* lang stays 0 */
    else if (shkp->data->msound <= MS_ANIMAL)
        lang = 1;
    else if (shkp->data->msound >= MS_HUMANOID)
        lang = 2;

    if (!inhishop(shkp)) {
        if (Role_if(PM_KNIGHT)) {
            pline(msgc_alignchaos, "You feel like a common thief.");
            adjalign(-sgn(u.ualign.type));
        }
        return;
    }

    if (!fall) {
        if (lang == 2) {
            if (u.utraptype == TT_PIT)
                verbalize(msgc_npcvoice, "Be careful, %s, or you might fall "
                          "through the floor.", u.ufemale ? "madam" : "sir");
            else
                verbalize(msgc_npcvoice, "%s, do not damage the floor here!",
                          u.ufemale ? "Madam" : "Sir");
        }
        if (Role_if(PM_KNIGHT)) {
            pline(msgc_alignchaos, "You feel like a common thief.");
            adjalign(-sgn(u.ualign.type));
        }
    } else if (!um_dist(shkp->mx, shkp->my, 5) && !shkp->msleeping &&
               shkp->mcanmove && (ESHK(shkp)->billct || ESHK(shkp)->debit)) {
        struct obj *obj, *obj2;

        if (nolimbs(shkp->data)) {
            grabs = "knocks off";
        }
        if (distu(shkp->mx, shkp->my) > 2) {
            mnexto(shkp);
            /* for some reason the shopkeeper can't come next to you */
            if (distu(shkp->mx, shkp->my) > 2) {
                if (lang == 2)
                    pline(msgc_noconsequence,
                          "%s curses you in anger and frustration!",
                          shkname(shkp));
                rile_shk(shkp);
                return;
            } else
                pline(msgc_itemloss, "%s %s, and %s your backpack!",
                      shkname(shkp), makeplural(locomotion(shkp->data, "leap")),
                      grabs);
        } else
            pline(msgc_itemloss, "%s %s your backpack!", shkname(shkp), grabs);

        for (obj = invent; obj; obj = obj2) {
            obj2 = obj->nobj;
            if ((obj->owornmask & ~(W_MASK(os_swapwep) |
                                    W_MASK(os_quiver))) != 0 ||
                (obj == uswapwep && u.twoweap) ||
                (obj->otyp == LEASH && obj->leashmon))
                continue;
            if (obj == turnstate.tracked[ttos_wand])
                continue;
            setnotworn(obj);
            freeinv(obj);
            subfrombill(obj, shkp);
            add_to_minv(shkp, obj);     /* may free obj */
        }
    }
}


static void
makekops(coord * mm)
{
    static const short k_mndx[4] = {
        PM_KEYSTONE_KOP, PM_KOP_SERGEANT, PM_KOP_LIEUTENANT, PM_KOP_KAPTAIN
    };
    int k_cnt[4], cnt, mndx, k;

    k_cnt[0] = cnt = abs(depth(&u.uz)) + rnd(5);
    k_cnt[1] = (cnt / 3) + 1;   /* at least one sarge */
    k_cnt[2] = (cnt / 6);       /* maybe a lieutenant */
    k_cnt[3] = (cnt / 9);       /* and maybe a kaptain */

    for (k = 0; k < 4; k++) {
        if ((cnt = k_cnt[k]) == 0)
            break;
        mndx = k_mndx[k];
        if (mvitals[mndx].mvflags & G_GONE)
            continue;

        while (cnt--)
            if (enexto(mm, level, mm->x, mm->y, &mons[mndx]))
                makemon(&mons[mndx], level, mm->x, mm->y, NO_MM_FLAGS);
    }
}


void
pay_for_damage(const char *dmgstr, boolean cant_mollify)
{
    struct monst *shkp = NULL;
    char shops_affected[5];
    boolean uinshp = (*u.ushops != '\0');
    char qbuf[80];
    xchar x, y;
    boolean dugwall = !strcmp(dmgstr, "dig into") ||    /* wand */
        !strcmp(dmgstr, "damage");      /* pick-axe */
    struct damage *tmp_dam, *appear_here = 0;

    /* any number >= (80*80)+(24*24) would do, actually */
    long cost_of_damage = 0L;
    unsigned int nearest_shk = 7000, nearest_damage = 7000;
    int picks = 0;

    for (tmp_dam = level->damagelist; (tmp_dam && (tmp_dam->when == moves));
         tmp_dam = tmp_dam->next) {
        char *shp;

        if (!tmp_dam->cost)
            continue;
        cost_of_damage += tmp_dam->cost;
        strcpy(shops_affected,
               in_rooms(level, tmp_dam->place.x, tmp_dam->place.y, SHOPBASE));
        for (shp = shops_affected; *shp; shp++) {
            struct monst *tmp_shk;
            unsigned int shk_distance;

            if (!(tmp_shk = shop_keeper(level, *shp)))
                continue;
            if (tmp_shk == shkp) {
                unsigned int damage_distance =
                    distu(tmp_dam->place.x, tmp_dam->place.y);

                if (damage_distance < nearest_damage) {
                    nearest_damage = damage_distance;
                    appear_here = tmp_dam;
                }
                continue;
            }
            if (!inhishop(tmp_shk))
                continue;
            shk_distance = distu(tmp_shk->mx, tmp_shk->my);
            if (shk_distance > nearest_shk)
                continue;
            if ((shk_distance == nearest_shk) && picks) {
                if (rn2(++picks))
                    continue;
            } else
                picks = 1;
            shkp = tmp_shk;
            nearest_shk = shk_distance;
            appear_here = tmp_dam;
            nearest_damage = distu(tmp_dam->place.x, tmp_dam->place.y);
        }
    }

    if (!cost_of_damage || !shkp)
        return;

    x = appear_here->place.x;
    y = appear_here->place.y;

    /* not the best introduction to the shk... */
    strncpy(ESHK(shkp)->customer, u.uplname, PL_NSIZ);

    /* if the shk is already on the war path, be sure it's all out */
    if (ANGRY(shkp) || ESHK(shkp)->following) {
        hot_pursuit(shkp);
        return;
    }

    /* if the shk is not in their shop.. */
    if (!*in_rooms(level, shkp->mx, shkp->my, SHOPBASE)) {
        if (!cansee(shkp->mx, shkp->my))
            return;
        goto getcad;
    }

    if (uinshp) {
        if (um_dist(shkp->mx, shkp->my, 1) && !um_dist(shkp->mx, shkp->my, 3)) {
            pline(msgc_moncombatbad, "%s leaps towards you!", shkname(shkp));
            mnexto(shkp);
        }
        if (um_dist(shkp->mx, shkp->my, 1))
            goto getcad;
    } else {
        /* Make shkp show up at the door. Effect: If there is a monster in the
           doorway, have the hero hear the shopkeeper yell a bit, pause, then
           have the shopkeeper appear at the door, having yanked the hapless
           critter out of the way. */
        if (MON_AT(level, x, y)) {
            You_hear(msgc_monneutral, "an angry voice:");
            verbalize(msgc_monneutral, "Out of my way, scum!");
        }
        mnearto(shkp, x, y, TRUE);
    }

    if ((um_dist(x, y, 1) && !uinshp) || cant_mollify ||
        (money_cnt(invent) + ESHK(shkp)->credit) < cost_of_damage || !rn2(50)) {
        if (um_dist(x, y, 1) && !uinshp) {
            pline(msgc_npcvoice, "%s shouts:", shkname(shkp));
            verbalize(msgc_npcanger, "Who dared %s my %s?",
                      dmgstr, dugwall ? "shop" : "door");
        } else {
        getcad:
            verbalize(msgc_npcanger, "How dare you %s my %s?", dmgstr,
                      dugwall ? "shop" : "door");
        }
        hot_pursuit(shkp);
        return;
    }

    if (Invis)
        pline(msgc_notresisted, "Your invisibility does not fool %s!",
              shkname(shkp));
    snprintf(qbuf, SIZE(qbuf), "\"Cad!  You did %ld %s worth of damage!\"  Pay? ",
            cost_of_damage, currency(cost_of_damage));
    if (yn(qbuf) != 'n') {
        cost_of_damage = check_credit(cost_of_damage, shkp);
        money2mon(shkp, cost_of_damage);
        pline(msgc_actionok, "Mollified, %s accepts your restitution.",
              shkname(shkp));
        /* move shk back to his home loc */
        home_shk(shkp, FALSE);
        pacify_shk(shkp);
    } else {
        verbalize(msgc_npcanger, "Oh, yes!  You'll pay!");
        hot_pursuit(shkp);
        adjalign(-sgn(u.ualign.type));
    }
}

/* called in dokick.c when we kick an object that might be in a store */
boolean
costly_spot(xchar x, xchar y)
{
    struct monst *shkp;

    if (!search_special(level, ANY_SHOP))
        return FALSE;
    shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE));
    if (!shkp || !inhishop(shkp))
        return FALSE;

    return ((boolean)
            (inside_shop(level, x, y) &&
             !(x == ESHK(shkp)->shk.x && y == ESHK(shkp)->shk.y)));
}


/* called by dotalk(sounds.c) when #chatting; returns obj if location
   contains shop goods and shopkeeper is willing & able to speak */
struct obj *
shop_object(xchar x, xchar y)
{
    struct obj *otmp;
    struct monst *shkp;

    if (!(shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE))) ||
        !inhishop(shkp))
        return NULL;

    for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
        if (otmp->oclass != COIN_CLASS)
            break;
    /* note: otmp might have ->no_charge set, but that's ok */
    return (otmp && costly_spot(x, y) && NOTANGRY(shkp)
            && shkp->mcanmove && !shkp->msleeping)
        ? otmp : NULL;
}


/* give price quotes for all objects linked to this one (ie, on this spot) */
void
price_quote(struct obj *first_obj)
{
    struct obj *otmp;
    char price[40];
    long cost;
    int cnt = 0;
    struct nh_menulist menu;
    struct monst *shkp = shop_keeper(level, inside_shop(level, u.ux, u.uy));
    const char *buf = ""; /* lint suppression */

    init_menulist(&menu);

    add_menutext(&menu, "Fine goods for sale:");
    add_menutext(&menu, "");
    for (otmp = first_obj; otmp; otmp = otmp->nexthere) {
        if (otmp->oclass == COIN_CLASS)
            continue;
        cost = (otmp->no_charge || otmp == uball ||
                otmp == uchain) ? 0L : get_cost(otmp, NULL);
        if (Has_contents(otmp))
            cost += contained_cost(otmp, shkp, 0L, FALSE, FALSE);
        if (!cost) {
            strcpy(price, "no charge");
        } else {
            snprintf(price, SIZE(price), "%ld %s%s", cost, currency(cost),
                    otmp->quan > 1L ? " each" : "");
        }
        buf = msgcat_many(doname(otmp), ", ", price, NULL);
        add_menutext(&menu, buf);
        cnt++;
    }

    if (cnt > 1) {
        display_menu(&menu, NULL, PICK_NONE, PLHINT_CONTAINER, NULL);
    } else if (cnt == 1) {
        dealloc_menulist(&menu);
        if (first_obj->no_charge || first_obj == uball || first_obj == uchain) {
            pline(msgc_info, "%s!", buf);  /* buf still contains the string */
        } else {
            /* print cost in slightly different format, so can't reuse buf */
            cost = get_cost(first_obj, NULL);
            if (Has_contents(first_obj))
                cost += contained_cost(first_obj, shkp, 0L, FALSE, FALSE);
            pline(msgc_info, "%s, price %ld %s%s%s",
                  msgupcasefirst(doname(first_obj)), cost,
                  currency(cost), first_obj->quan > 1L ? " each" : "",
                  shk_embellish(first_obj, cost));
        }
    } else
        dealloc_menulist(&menu);
}


static const char *
shk_embellish(struct obj *itm, long cost)
{
    if (!rn2(3)) {
        int o, choice = rn2(5);

        if (choice == 0)
            choice = (cost < 100L ? 1 : cost < 500L ? 2 : 3);
        switch (choice) {
        case 4:
            if (cost < 10L)
                break;
            else
                o = itm->oclass;
            if (o == FOOD_CLASS)
                return ", gourmets' delight!";
            if (objects[itm->otyp].oc_name_known ? objects[itm->otyp].
                oc_magic : (o == AMULET_CLASS || o == RING_CLASS ||
                            o == WAND_CLASS || o == POTION_CLASS ||
                            o == SCROLL_CLASS || o == SPBOOK_CLASS))
                return ", painstakingly developed!";
            return ", superb craftsmanship!";
        case 3:
            return ", finest quality.";
        case 2:
            return ", an excellent choice.";
        case 1:
            return ", a real bargain.";
        default:
            break;
        }
    } else if (itm->oartifact) {
        return ", one of a kind!";
    }
    return ".";
}

/* First 4 supplied by Ronen and Tamar, remainder by development team */
static const char *const Izchak_speaks[] = {
    "%s says: 'These shopping malls give me a headache.'",
    "%s says: 'Slow down.  Think clearly.'",
    "%s says: 'You need to take things one at a time.'",
    "%s says: 'I don't like poofy coffee... give me Columbian Supremo.'",
    "%s says that getting the devteam's agreement on anything is difficult.",
    "%s says that he has noticed those who serve their deity will prosper.",
    "%s says: 'Don't try to steal from me - I have friends in high places!'",
    "%s says: 'You may well need something from this shop in the future.'",
    "%s comments about the Valley of the Dead as being a gateway."
};

void
shk_chat(struct monst *shkp)
{
    struct eshk *eshk;
    long shkmoney;

    if (!shkp->isshk) {
        /* The monster type is shopkeeper, but this monster is not actually a
           shk, which could happen if someone wishes for a shopkeeper statue
           and then animates it. (Note: shkname() would be "" in a case like
           this.) */
        pline(msgc_npcvoice,
              "%s asks whether you've seen any untended shops recently.",
              Monnam(shkp));
        /* [Perhaps we ought to check whether this conversation is taking place 
           inside an untended shop, but a shopless shk can probably be expected 
           to be rather disoriented.] */
        return;
    }

    eshk = ESHK(shkp);
    if (ANGRY(shkp))
        pline(msgc_npcvoice, "%s mentions how much %s dislikes %s customers.",
              shkname(shkp), mhe(shkp), eshk->robbed ? "non-paying" : "rude");
    else if (eshk->following) {
        if (strncmp(eshk->customer, u.uplname, PL_NSIZ)) {
            verbalize(msgc_npcvoice, "%s %s!  I was looking for %s.",
                      Hello(shkp), u.uplname, eshk->customer);
            eshk->following = 0;
        } else {
            verbalize(msgc_npcvoice, "%s %s!  Didn't you forget to pay?",
                      Hello(shkp), u.uplname);
        }
    } else if (eshk->billct) {
        long total = addupbill(shkp) + eshk->debit;

        pline(msgc_info, "%s says that your bill comes to %ld %s.",
              shkname(shkp), total, currency(total));
    } else if (eshk->debit)
        pline(msgc_info, "%s reminds you that you owe %s %ld %s.",
              shkname(shkp), mhim(shkp), (long)eshk->debit,
              currency(eshk->debit));
    else if (eshk->credit)
        pline(msgc_info, "%s encourages you to use your %ld %s of credit.",
              shkname(shkp), (long)eshk->credit, currency(eshk->credit));
    else if (eshk->robbed)
        pline(msgc_npcvoice, "%s complains about a recent robbery.",
              shkname(shkp));
    else if ((shkmoney = money_cnt(shkp->minvent)) < 50)
        pline(msgc_npcvoice, "%s complains that business is bad.",
              shkname(shkp));
    else if (shkmoney > 4000)
        pline(msgc_npcvoice, "%s says that business is good.", shkname(shkp));
    else if (strcmp(shkname(shkp), "Izchak") == 0)
        pline(msgc_rumor, Izchak_speaks[rn2(SIZE(Izchak_speaks))],
              shkname(shkp));
    else
        pline(msgc_npcvoice, "%s talks about the problem of shoplifters.",
              shkname(shkp));
}


static void
kops_gone(boolean silent)
{
    int cnt = 0;
    struct monst *mtmp, *mtmp2;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
        mtmp2 = mtmp->nmon;
        if (mtmp->data->mlet == S_KOP) {
            if (canspotmon(mtmp))
                cnt++;
            mongone(mtmp);
        }
    }
    if (cnt && !silent)
        pline(msgc_monneutral,
              "The Kop%s (disappointed) vanish%s into thin air.",
              plur(cnt), cnt == 1 ? "es" : "");
}


static long
cost_per_charge(struct monst *shkp, struct obj *otmp, boolean altusage)
{       /* some items have an "alternate" use with different cost */
    long tmp = 0L;

    if (!shkp || !inhishop(shkp))
        return 0L;      /* insurance */
    tmp = get_cost(otmp, shkp);

    /* The idea is to make the exhaustive use of */
    /* an unpaid item more expensive than buying */
    /* it outright.  */
    if (otmp->otyp == MAGIC_LAMP) {     /* 1 */
        /* normal use (ie, as light source) of a magic lamp never degrades its
           value, but not charging anything would make identifcation too easy;
           charge an amount comparable to what is charged for an ordinary lamp
           (don't bother with angry shk surchage) */
        if (!altusage)
            tmp = (long)objects[OIL_LAMP].oc_cost;
        else
            tmp += tmp / 3L;    /* djinni is being released */
    } else if (otmp->otyp == MAGIC_MARKER) {    /* 70 - 100 */
        /* no way to determine in advance */
        /* how many charges will be wasted. */
        /* so, arbitrarily, one half of the */
        /* price per use.  */
        tmp /= 2L;
    } else if (otmp->otyp == BAG_OF_TRICKS ||         /* 1 - 20 */
               otmp->otyp == HORN_OF_PLENTY) {
        tmp /= 5L;
    } else if (otmp->otyp == CRYSTAL_BALL ||          /* 1 - 5 */
               otmp->otyp == OIL_LAMP ||              /* 1 - 10 */
               otmp->otyp == BRASS_LANTERN ||
               (otmp->otyp >= MAGIC_FLUTE &&
                otmp->otyp <= DRUM_OF_EARTHQUAKE) ||  /* 5 - 9 */
               otmp->oclass == WAND_CLASS) {          /* 3 - 11 */
        if (otmp->spe > 1)
            tmp /= 4L;
    } else if (otmp->oclass == SPBOOK_CLASS) {
        tmp -= tmp / 5L;
    } else if (otmp->otyp == CAN_OF_GREASE || otmp->otyp == TINNING_KIT ||
               otmp->otyp == EXPENSIVE_CAMERA) {
        tmp /= 10L;
    } else if (otmp->otyp == POT_OIL) {
        tmp /= 5L;
    }
    return tmp;
}


/* Charge the player for partial use of an unpaid object.
 *
 * Note that bill_dummy_object() should be used instead
 * when an object is completely used.
 */
void
check_unpaid_usage(struct obj *otmp, boolean altusage)
{
    struct monst *shkp;
    const char *fmt, *arg1, *arg2;
    long tmp;

    if (!otmp->unpaid || !*u.ushops ||
        (otmp->spe <= 0 && objects[otmp->otyp].oc_charged))
        return;
    if (!(shkp = shop_keeper(level, *u.ushops)) || !inhishop(shkp))
        return;
    if ((tmp = cost_per_charge(shkp, otmp, altusage)) == 0L)
        return;

    arg1 = arg2 = "";
    if (otmp->oclass == SPBOOK_CLASS) {
        fmt = "%sYou owe%s %ld %s.";
        arg1 = rn2(2) ? "This is no free library, cad!  " : "";
        arg2 = ESHK(shkp)->debit > 0L ? " an additional" : "";
    } else if (otmp->otyp == POT_OIL) {
        fmt = "%s%sThat will cost you %ld %s (Yendorian Fuel Tax).";
    } else {
        fmt = "%s%sUsage fee, %ld %s.";
        if (!rn2(3))
            arg1 = "Hey!  ";
        if (!rn2(3))
            arg2 = "Ahem.  ";
    }

    if (shkp->mcanmove || !shkp->msleeping)
        verbalize(msgc_unpaid, fmt, arg1, arg2, tmp, currency(tmp));
    ESHK(shkp)->debit += tmp;
    exercise(A_WIS, TRUE);      /* you just got info */
}

/* for using charges of unpaid objects "used in the normal manner" */
void
check_unpaid(struct obj *otmp)
{
    check_unpaid_usage(otmp, FALSE);    /* normal item use */
}

void
costly_gold(xchar x, xchar y, long amount)
{
    long delta;
    struct monst *shkp;
    struct eshk *eshkp;

    if (!costly_spot(x, y))
        return;
    /* shkp now guaranteed to exist by costly_spot() */
    shkp = shop_keeper(level, *in_rooms(level, x, y, SHOPBASE));

    eshkp = ESHK(shkp);
    if (eshkp->credit >= amount) {
        if (eshkp->credit > amount)
            pline(msgc_unpaid, "Your credit is reduced by %ld %s.",
                  amount, currency(amount));
        else
            pline(msgc_unpaid, "Your credit is erased.");
        eshkp->credit -= amount;
    } else {
        delta = amount - eshkp->credit;
        if (eshkp->credit)
            pline(msgc_unpaid, "Your credit is erased.");
        if (eshkp->debit)
            pline(msgc_unpaid, "Your debt increases by %ld %s.", delta,
                  currency(delta));
        else
            pline(msgc_unpaid, "You owe %s %ld %s.", shkname(shkp), delta,
                  currency(delta));
        eshkp->debit += delta;
        eshkp->loan += delta;
        eshkp->credit = 0L;
    }
}

/* used in domove to block diagonal shop-exit */
/* x,y should always be a door */
boolean
block_door(xchar x, xchar y)
{
    int roomno = *in_rooms(level, x, y, SHOPBASE);
    struct monst *shkp;

    if (roomno < 0 || !IS_SHOP(level, roomno))
        return FALSE;
    if (!IS_DOOR(level->locations[x][y].typ))
        return FALSE;
    if (roomno != *u.ushops)
        return FALSE;

    if (!(shkp = shop_keeper(level, (char)roomno)) || !inhishop(shkp))
        return FALSE;

    if (shkp->mx == ESHK(shkp)->shk.x && shkp->my == ESHK(shkp)->shk.y
        /* Actually, the shk should be made to block _any_ door, including a
           door the player digs, if the shk is within a 'jumping' distance. */
        && ESHK(shkp)->shd.x == x && ESHK(shkp)->shd.y == y && shkp->mcanmove &&
        !shkp->msleeping && (ESHK(shkp)->debit || ESHK(shkp)->billct ||
                             ESHK(shkp)->robbed)) {
        pline(msgc_moncombatbad, "%s%s blocks your way!", shkname(shkp),
              Invis ? " senses your motion and" : "");
        return TRUE;
    }
    return FALSE;
}

/* used in domove to block diagonal shop-entry */
/* u.ux, u.uy should always be a door */
boolean
block_entry(xchar x, xchar y)
{
    xchar sx, sy;
    int roomno;
    struct monst *shkp;

    if (!
        (IS_DOOR(level->locations[u.ux][u.uy].typ) &&
         level->locations[u.ux][u.uy].doormask == D_BROKEN))
        return FALSE;

    roomno = *in_rooms(level, x, y, SHOPBASE);
    if (roomno < 0 || !IS_SHOP(level, roomno))
        return FALSE;
    if (!(shkp = shop_keeper(level, (char)roomno)) || !inhishop(shkp))
        return FALSE;

    if (ESHK(shkp)->shd.x != u.ux || ESHK(shkp)->shd.y != u.uy)
        return FALSE;

    sx = ESHK(shkp)->shk.x;
    sy = ESHK(shkp)->shk.y;

    if (shkp->mx == sx && shkp->my == sy && shkp->mcanmove && !shkp->msleeping
        && (x == sx - 1 || x == sx + 1 || y == sy - 1 || y == sy + 1)
        && (Invis || carrying(PICK_AXE) || carrying(DWARVISH_MATTOCK)
            || u.usteed)) {
        pline(msgc_moncombatbad, "%s%s blocks your way!", shkname(shkp),
              Invis ? " senses your motion and" : "");
        return TRUE;
    }
    return FALSE;
}

const char *
shk_your(const struct obj *obj)
{
    const char *rv;
    rv = shk_owns(obj);
    if (!rv)
        rv = mon_owns(obj);
    if (!rv)
        rv = carried(obj) ? "your" : "the";
    return rv;
}

const char *
Shk_Your(const struct obj *obj)
{
    return msgupcasefirst(shk_your(obj));
}

static const char *
shk_owns(const struct obj *obj)
{
    struct monst *shkp;
    xchar x, y;

    if (get_obj_location(obj, &x, &y, 0) &&
        (obj->unpaid ||
         (obj->where == OBJ_FLOOR && !obj->no_charge && costly_spot(x, y)))) {
        shkp = shop_keeper(level, inside_shop(level, x, y));
        return shkp ? s_suffix(shkname(shkp)) : "the";
    }
    return NULL;
}

static const char *
mon_owns(const struct obj *obj)
{
    if (obj->where == OBJ_MINVENT)
        return s_suffix(mon_nam(obj->ocarry));
    return NULL;
}

void
adjust_bill_val(struct obj *obj)
{
    struct bill_x *bp = NULL;
    struct monst *shkp;

    if (!obj->unpaid) {
        impossible("adjust_bill_val: object wasn't unpaid!");
        return;
    }

    for (shkp = next_shkp(level->monlist, TRUE); shkp;
         shkp = next_shkp(shkp->nmon, TRUE))
        if ((bp = onbill(obj, shkp, TRUE)) != 0)
            break;

    /* onbill() gave no message if unexpected problem occurred */
    if (!bp) {
        impossible("adjust_bill_val: object wasn't on any bill!");
        return;
    }

    bp->price = get_cost(obj, shkp);
}

void
costly_damage_obj(struct obj *obj)
{
    if (!obj->unpaid) {
        impossible("costly_damage_obj: object wasn't unpaid!");
        return;
    }
    if (flags.mon_moving) {
        adjust_bill_val(obj);
        return;
    }
    if (obj->unpaid) {
        struct monst *shkp = shop_keeper(level, *u.ushops);

        if (shkp) {
            pline(msgc_unpaid, "You damage it, you pay for it!");
            bill_dummy_object(obj);
        }
    }
}

/*shk.c*/

