/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Contains code for picking objects up, and container use.
 */

#include "hack.h"

static void check_here(boolean);
static boolean n_or_more(const struct obj *);
static boolean all_but_uchain(const struct obj *);
static int autopick(struct obj *, int, struct object_pick **);
static int count_categories(struct obj *, int);
static long carry_count(struct obj *, struct obj *, long, boolean, int *,
                        int *);
static int lift_object(struct obj *, struct obj *, long *, boolean);
static int mbag_explodes(struct obj *, int);
static int in_container(struct obj *);
static int out_container(struct obj *);
static long mbag_item_gone(int, struct obj *);
static int menu_loot(int, struct obj *, boolean);
static int in_or_out_menu(const char *, struct obj *, boolean, boolean);
static int container_at(int, int, boolean);
static boolean able_to_loot(int, int);
static boolean mon_beside(int, int);

/* define for query_objlist() and autopickup() */
#define FOLLOW(curr, flags) \
    (((flags) & BY_NEXTHERE) ? (curr)->nexthere : (curr)->nobj)

/*
 *  How much the weight of the given container will change when the given
 *  object is removed from it.  This calculation must match the one used
 *  by weight() in mkobj.c.
 */
#define DELTA_CWT(cont,obj)             \
    ((cont)->cursed ? (obj)->owt * 2 :  \
                      ((cont)->blessed ? ((obj)->owt + 3) / 4 : \
                                         ((obj)->owt + 1) / 2))
#define GOLD_WT(n)         (((n) + 50L) / 100L)
/* if you can figure this out, give yourself a hearty pat on the back... */
#define GOLD_CAPACITY(w,n) (((w) * -100L) - ((n) + 50L) - 1L)

static const char moderateloadmsg[] = "You have a little trouble lifting";
static const char nearloadmsg[] = "You have much trouble lifting";
static const char overloadmsg[] = "You have extreme difficulty lifting";

/* look at the objects at our location, unless there are too many of them */
static void
check_here(boolean picked_some)
{
    struct obj *obj;
    int ct = 0;
    boolean autoexploring = flags.occupation == occ_autoexplore;

    /* count the objects here */
    for (obj = level->objects[u.ux][u.uy]; obj; obj = obj->nexthere) {
        if (obj != uchain)
            ct++;
    }

    /* If there are objects here, take a look unless autoexploring a previously
       explored space. */
    if (ct && !(autoexploring && level->locations[u.ux][u.uy].mem_stepped)) {
        if (flags.occupation == occ_move || travelling())
            action_completed();
        flush_screen();
        look_here(ct, picked_some, FALSE, Blind);
    } else {
        read_engr_at(u.ux, u.uy);
    }
}

/* Value set by query_objlist() for n_or_more(). */
static long val_for_n_or_more;

/* query_objlist callback: return TRUE if obj's count is >= reference value */
static boolean
n_or_more(const struct obj *obj)
{
    if (obj == uchain)
        return FALSE;
    return obj->quan >= val_for_n_or_more;
}

/* List of valid menu classes for query_objlist() and allow_category callback */
static char valid_menu_classes[MAXOCLASSES + 2];

void
add_valid_menu_class(int c)
{
    static int vmc_count = 0;

    if (c == 0) /* reset */
        vmc_count = 0;
    else
        valid_menu_classes[vmc_count++] = (char)c;
    valid_menu_classes[vmc_count] = '\0';
}

/* query_objlist callback: return TRUE if not uchain */
static boolean
all_but_uchain(const struct obj *obj)
{
    return obj != uchain;
}

/* query_objlist callback: return TRUE */
 /*ARGSUSED*/ boolean allow_all(const struct obj * obj)
{
    return TRUE;
}

boolean
allow_category(const struct obj * obj)
{
    boolean priest = Role_if(PM_PRIEST);

    if (((strchr(valid_menu_classes, 'u') != NULL) && obj->unpaid) ||
        (strchr(valid_menu_classes, obj->oclass) != NULL))
        return TRUE;
    else if (((strchr(valid_menu_classes, 'U') != NULL) &&
              (obj->oclass != COIN_CLASS && (priest || obj->bknown) &&
               !obj->blessed && !obj->cursed)))
        return TRUE;
    else if (((strchr(valid_menu_classes, 'B') != NULL) &&
              (obj->oclass != COIN_CLASS && (priest || obj->bknown) &&
               obj->blessed)))
        return TRUE;
    else if (((strchr(valid_menu_classes, 'C') != NULL) &&
              (obj->oclass != COIN_CLASS && (priest || obj->bknown) &&
               obj->cursed)))
        return TRUE;
    else if (((strchr(valid_menu_classes, 'X') != NULL) &&
              (obj->oclass != COIN_CLASS && !(priest || obj->bknown))))
        return TRUE;
    else if (((strchr(valid_menu_classes, 'I') != NULL) &&
              not_fully_identified_core(obj, TRUE)))
        return TRUE;
    else
        return FALSE;
}


/* query_objlist callback: return TRUE if valid class and worn */
boolean
is_worn_by_type(const struct obj * otmp)
{
    return (otmp->owornmask & W_EQUIP) &&
        (strchr(valid_menu_classes, otmp->oclass) != NULL);
}

/*
 * Have the hero pick up things from the ground or a monster's inventory if
 * swallowed. Autopickup might be vetoed, based on the interaction mode.
 *
 * Arg what:
 *      >0  autopickup
 *      =0  interactive
 *      <0  pickup count of something
 *
 * Returns 1 if tried to pick something up, whether
 * or not it succeeded.
 */
int
pickup(int what, enum u_interaction_mode uim)
{
    int i, n, res, count, n_tried = 0, n_picked = 0;
    struct object_pick *pick_list = NULL;
    boolean autopickup = what > 0;
    struct obj *objchain;
    int traverse_how;

    if (what < 0)       /* pick N of something */
        count = -what;
    else        /* pick anything */
        count = 0;

    if (!Engulfed) {
        struct trap *ttmp = t_at(level, u.ux, u.uy);

        /* no auto-pick if no-pick move, nothing there, or in a pool */
        if (autopickup &&
            (uim == uim_nointeraction || !OBJ_AT(u.ux, u.uy) ||
             (is_pool(level, u.ux, u.uy) && !Underwater) ||
             is_lava(level, u.ux, u.uy))) {
            read_engr_at(u.ux, u.uy);
            return 0;
        }

        /* no pickup if levitating & not on air or water level */
        if (!can_reach_floor()) {
            read_engr_at(u.ux, u.uy);
            return 0;
        }

        if (ttmp && ttmp->tseen) {
            /* Allow pickup from holes and trap doors that you escaped from
               because that stuff is teetering on the edge just like you, but
               not pits, because there is an elevation discrepancy with stuff
               in pits. */
            if ((ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT) &&
                (!u.utrap || (u.utrap && u.utraptype != TT_PIT)) &&
                !Passes_walls) {
                read_engr_at(u.ux, u.uy);
                return 0;
            }
        }

        /* Don't autopick up while teleporting while helpless, or if the player
           explicitly turned autopickup off. */
        if (u_helpless(hm_all) || (autopickup && !flags.pickup)) {
            check_here(FALSE);
            return 0;
        }

        if (notake(youmonst.data)) {
            if (!autopickup)
                pline(msgc_cancelled,
                      "You are physically incapable of picking anything up.");
            else
                check_here(FALSE);
            return 0;
        }

        /* If there's anything here, stop running and travel, but not
           autoexplore unless it picks something up, which is handled later. */
        if (OBJ_AT(u.ux, u.uy) && (flags.occupation == occ_move ||
                                   flags.occupation == occ_travel))
            action_completed();
    }

    add_valid_menu_class(0);    /* reset */
    if (!Engulfed) {
        objchain = level->objects[u.ux][u.uy];
        traverse_how = BY_NEXTHERE;
    } else {
        objchain = u.ustuck->minvent;
        traverse_how = 0;       /* nobj */
    }
    /* 
     * Start the actual pickup process.  This is split into two main
     * sections, the newer menu and the older "traditional" methods.
     * Automatic pickup has been split into its own menu-style routine
     * to make things less confusing.
     */
    if (autopickup) {
        n = autopick(objchain, traverse_how, &pick_list);
        goto menu_pickup;
    }

    if (count) {        /* looking for N of something */
        const char *qbuf;

        qbuf = msgprintf("Pick %d of what?", count);
        val_for_n_or_more = count;      /* set up callback selector */
        n = query_objlist(qbuf, objchain,
                          traverse_how | AUTOSELECT_SINGLE | INVORDER_SORT,
                          &pick_list, PICK_ONE, n_or_more);
        /* correct counts, if any given */
        for (i = 0; i < n; i++)
            pick_list[i].count = count;
    } else {
        n = query_objlist("Pick up what?", objchain,
                          traverse_how | AUTOSELECT_SINGLE | INVORDER_SORT |
                          FEEL_COCKATRICE, &pick_list, PICK_ANY,
                          all_but_uchain);
    }
menu_pickup:
    n_tried = n;
    for (n_picked = i = 0; i < n; i++) {
        res = pickup_object(pick_list[i].obj, pick_list[i].count, FALSE);
        if (res < 0)
            break;      /* can't continue */
        n_picked += res;
    }
    if (pick_list)
        free(pick_list);

    if (!Engulfed) {
        if (!OBJ_AT(u.ux, u.uy))
            u.uundetected = 0;

        /* position may need updating (invisible hero) */
        if (n_picked)
            newsym(u.ux, u.uy);

        /* see whether there's anything else here, after auto-pickup is done */
        if (autopickup)
            check_here(n_picked > 0);
    }

    /* Stop autoexplore if this pile hasn't been explored or auto-pickup (tried 
       to) pick up anything. */
    if (flags.occupation == occ_autoexplore &&
        (!level->locations[u.ux][u.uy].mem_stepped ||
         (autopickup && n_tried > 0)))
        action_completed();

    return n_tried > 0;
}


static boolean
autopickup_match(struct obj *obj)
{
    int i;
    struct nh_autopickup_rule *r;
    const char *objdesc;
    enum nh_bucstatus objbuc;

    if (!flags.ap_rules)
        return FALSE;

    objdesc = makesingular(doname_price(obj));
    if (obj->bknown) {
        if (obj->blessed)
            objbuc = B_BLESSED;
        else if (obj->cursed)
            objbuc = B_CURSED;
        else
            objbuc = B_UNCURSED;
    } else
        objbuc = B_UNKNOWN;

    /* Test the autopickup rules in order. If any of the rules matches this
       object, return the result. */
    r = &flags.ap_rules->rules[0];
    for (i = 0; i < flags.ap_rules->num_rules; i++, r++) {
        if ((!strlen(r->pattern) || pmatch(r->pattern, objdesc)) &&
            (r->oclass == OCLASS_ANY ||
             r->oclass == def_oc_syms[(int)obj->oclass]) &&
            (r->buc == B_DONT_CARE || r->buc == objbuc))
            return r->action == AP_GRAB;
    }
    return FALSE;
}


/* Pick from the given list using autopickup rules. Return the number of items
   picked (not counts). Create an array that returns pointers and counts of the
   items to be picked up. If the number of items picked is zero, the pickup list
   is left alone. The caller of this function must free the pickup list. */
static int
autopick(struct obj *olist,     /* the object list */
         int follow,    /* how to follow the object list */
         struct object_pick **pick_list)
{       /* list of objects and counts to pick up */
    struct object_pick *pi;     /* pick item */
    struct obj *curr;
    int n;

    /* first count the number of eligible items */
    for (n = 0, curr = olist; curr; curr = FOLLOW(curr, follow)) {
        examine_object(curr);
        if (curr->was_dropped)
            continue;
        if ((flags.pickup_thrown && curr->was_thrown) ||
            autopickup_match(curr))
            n++;
    }

    if (n) {
        *pick_list = pi = malloc(sizeof (struct object_pick) * n);
        for (n = 0, curr = olist; curr; curr = FOLLOW(curr, follow)) {
            if (curr->was_dropped)
                continue;
            if ((flags.pickup_thrown && curr->was_thrown) ||
                autopickup_match(curr)) {
                pi[n].obj = curr;
                pi[n].count = curr->quan;
                n++;
            }
        }
    }
    return n;
}


void
add_objitem(struct nh_objlist *objlist,
            enum nh_menuitem_role role, int id, const char *caption,
            struct obj *obj, boolean use_invlet)
{
    struct nh_objitem *it;
    struct objclass *ocl;

    /* objlist->items && !objlist->size means don't try to manage the memory
       ourselves, let the caller do it */
    if (objlist->icount >= objlist->size &&
        (!objlist->items || objlist->size)) {
        if (objlist->size < 2)
            objlist->size = 2;
        objlist->size *= 2;
        objlist->items =
            realloc(objlist->items, objlist->size * sizeof (struct nh_objitem));
        if (!objlist->items)
            panic("Out of memory");
    }

    it = objlist->items + objlist->icount;
    memset(it, 0, sizeof (struct nh_objitem));

    objlist->icount++;

    it->id = id;
    it->weight = -1;
    it->role = role;
    strcpy(it->caption, caption);

    if (role == MI_NORMAL && obj) {
        ocl = &objects[obj->otyp];

        it->count = obj->quan;
        it->accel = use_invlet ? obj->invlet : 0;
        it->group_accel = def_oc_syms[(int)obj->oclass];
        it->otype = obfuscate_object(obj->otyp + 1);
        it->oclass = obj->oclass;
        it->worn = (obj->owornmask & ~(u.twoweap ? 0 : W_MASK(os_swapwep))) ||
            obj->lamplit;

        /* don't unconditionally reveal weight, otherwise lodestones on the
           floor could be identified by their weight in the pickup dialog */
        if (obj->where == OBJ_INVENT || ocl->oc_name_known || obj->invlet ||
            (obj->where == OBJ_CONTAINED &&
             obj->ocontainer->where == OBJ_INVENT))
            it->weight = obj->owt;

        if (!obj->bknown)
            it->buc = B_UNKNOWN;
        else if (obj->blessed)
            it->buc = B_BLESSED;
        else if (obj->cursed)
            it->buc = B_CURSED;
        else
            it->buc = B_UNCURSED;
    } else {
        it->accel = 0;
        it->group_accel = obj ? def_oc_syms[(int)obj->oclass] : 0;
        it->otype = it->oclass = -1;
    }

}


/* inv_order-only object comparison function for qsort */
static int
invo_obj_compare(const void *o1, const void *o2)
{
    struct obj *obj1 = *(struct obj *const *)o1;
    struct obj *obj2 = *(struct obj *const *)o2;

    /* compare positions in inv_order */
    char *pos1 = strchr(flags.inv_order, obj1->oclass);
    char *pos2 = strchr(flags.inv_order, obj2->oclass);

    if (pos1 != pos2)
        return pos1 - pos2;

    return 0;
}

/* object comparison function for qsort */
int
obj_compare(const void *o1, const void *o2)
{
    int cmp, val1, val2;
    struct obj *obj1 = *(struct obj *const *)o1;
    struct obj *obj2 = *(struct obj *const *)o2;

    /* compare positions in inv_order */
    int inv_order_cmp = invo_obj_compare(o1, o2);

    if (inv_order_cmp)
        return inv_order_cmp;

    /* compare names */
    cmp = strcmp(cxname2(obj1), cxname2(obj2));
    if (cmp)
        return cmp;

    /* Sort by enchantment. Map unknown to -1000, which is comfortably below
       the range of ->spe. */
    val1 = obj1->known ? obj1->spe : -1000;
    val2 = obj2->known ? obj2->spe : -1000;
    if (val1 != val2)
        return val2 - val1;     /* Because bigger is better. */

    /* BUC state -> int (sort order: blessed, uncursed, cursed, unknown)
       blessed = 3, uncursed = 2, cursed = 1, unknown = 0 */
    val1 = obj1->bknown ? (obj1->blessed + !obj1->cursed + 1) : 0;
    val2 = obj2->bknown ? (obj2->blessed + !obj2->cursed + 1) : 0;
    if (val1 != val2)
        return val2 - val1;

    /* Sort by erodeproofing. Map known-invulnerable to 1, and both
       known-vulnerable and unknown-vulnerability to 0. */
    val1 = obj1->rknown && obj1->oerodeproof;
    val2 = obj2->rknown && obj2->oerodeproof;
    if (val1 != val2)
        return val2 - val1;     /* Because bigger is better. */

    /* Sort by erosion. The effective amount is what matters. */
    val1 = greatest_erosion(obj1);
    val2 = greatest_erosion(obj2);
    if (val1 != val2)
        return val1 - val2;     /* Because bigger is WORSE. */

    if (obj1->greased != obj2->greased)
        return obj2->greased - obj1->greased;

    return 0;
}

/*
 * Put up a menu using the given object list.  Only those objects on the
 * list that meet the approval of the allow function are displayed.  Return
 * a count of the number of items selected, as well as an allocated array of
 * menu_items, containing pointers to the objects selected and counts.  The
 * returned counts are guaranteed to be in bounds and non-zero.
 *
 * Query flags:
 *      BY_NEXTHERE       - Follow object list via nexthere instead of nobj.
 *      AUTOSELECT_SINGLE - Don't ask if only 1 object qualifies - just
 *                          use it.
 *      USE_INVLET        - Use object's invlet.
 *      INVORDER_SORT     - Use hero's pack order.
 *      SIGNAL_NOMENU     - Return -1 rather than 0 if nothing passes "allow".
 *	SIGNAL_ESCAPE	  - Return -2 rather than 0 if menu is escaped.
 */
int
query_objlist(const char *qstr, /* query string */
              struct obj *olist,        /* the list to pick from */
              int qflags,       /* options to control the query */
              struct object_pick **pick_list,  /* return list of items picked */
              int how,  /* type of query */
              boolean(*allow) (const struct obj *))
{       /* allow function */
    int n = 0, i, prev_oclass, nr_objects;
    struct obj *curr, *last;
    struct nh_objlist objmenu;

    *pick_list = NULL;
    if (!olist)
        return 0;

    /* count the number of items allowed */
    for (n = 0, last = 0, curr = olist; curr; curr = FOLLOW(curr, qflags))
        if ((*allow) (curr)) {
            last = curr;
            n++;
        }

    if (n == 0) /* nothing to pick here */
        return (qflags & SIGNAL_NOMENU) ? -1 : 0;

    if (n == 1 && (qflags & AUTOSELECT_SINGLE)) {
        *pick_list = malloc(sizeof (struct object_pick));
        (*pick_list)->obj = last;
        (*pick_list)->count = last->quan;
        return 1;
    }

    /* add all allowed objects to the list */
    nr_objects = 0;

    struct obj *allowed[n];

    for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
        if ((qflags & FEEL_COCKATRICE) && curr->otyp == CORPSE &&
            will_feel_cockatrice(curr, FALSE)) {
            look_here(0, FALSE, TRUE, Blind);
            return 0;
        }
        if ((!(qflags & INVORDER_SORT) || strchr(flags.inv_order, curr->oclass))
            && (*allow) (curr))
            allowed[nr_objects++] = curr;
    }

    /* sort the list in place according to (1) inv_order and... */
    if (qflags & INVORDER_SORT) {
        if (qflags & USE_INVLET)        /* ... (2) only inv_order. */
            qsort(allowed, nr_objects, sizeof (struct obj *), invo_obj_compare);
        else    /* ... (2) object name. */
            qsort(allowed, nr_objects, sizeof (struct obj *), obj_compare);
    }

    init_objmenulist(&objmenu);
    prev_oclass = -1;

    for (i = 0; i < nr_objects; i++) {
        curr = allowed[i];
        /* if sorting, print type name */
        if ((qflags & INVORDER_SORT) && prev_oclass != curr->oclass)
            add_objitem(&objmenu, MI_HEADING, 0,
                        let_to_name(curr->oclass, FALSE), curr, FALSE);
        /* add the object to the list */
        examine_object(curr);
        add_objitem(&objmenu, MI_NORMAL, i + 1,
                    doname(curr), curr, (qflags & USE_INVLET) != 0);
        prev_oclass = curr->oclass;
    }

    const struct nh_objresult *selection = NULL;

    if (objmenu.icount > 0)
        n = display_objects(&objmenu, qstr, how, PLHINT_INVENTORY,
                            &selection);
    else
        dealloc_objmenulist(&objmenu);

    if (n > 0) {
        *pick_list = malloc(n * sizeof (struct object_pick));

        for (i = 0; i < n; i++) {
            curr = allowed[selection[i].id - 1];
            (*pick_list)[i].obj = curr;
            if (selection[i].count == -1 || selection[i].count > curr->quan)
                (*pick_list)[i].count = curr->quan;
            else
                (*pick_list)[i].count = selection[i].count;
        }
    } else if (n < 0) {
        /* callers don't expect -1 by this point */
        n = (qflags & SIGNAL_ESCAPE) ? -2 : 0;
    }

    return n;
}

/*
 * allow menu-based category (class) selection (for Drop,take off etc.)
 */
int
query_category(const char *qstr,        /* query string */
               struct obj *olist,       /* the list to pick from */
               int qflags,              /* behaviour modification flags */
               const int **pick_list,   /* return list of items picked */
               int how)
{       /* type of query */
    int n;
    struct obj *curr;
    char *pack;
    boolean collected_type_name;
    char invlet;
    int ccount;
    boolean do_unpaid = FALSE;
    boolean do_blessed = FALSE, do_cursed = FALSE, do_uncursed =
        FALSE, do_buc_unknown = FALSE, do_unidentified = FALSE;
    int num_buc_types = 0;
    struct nh_menulist menu;

    if (!olist)
        return 0;
    if ((qflags & UNPAID_TYPES) && count_unpaid(olist))
        do_unpaid = TRUE;
    if ((qflags & BUC_BLESSED) && count_buc(olist, BUC_BLESSED)) {
        do_blessed = TRUE;
        num_buc_types++;
    }
    if ((qflags & BUC_CURSED) && count_buc(olist, BUC_CURSED)) {
        do_cursed = TRUE;
        num_buc_types++;
    }
    if ((qflags & BUC_UNCURSED) && count_buc(olist, BUC_UNCURSED)) {
        do_uncursed = TRUE;
        num_buc_types++;
    }
    if ((qflags & BUC_UNKNOWN) && count_buc(olist, BUC_UNKNOWN)) {
        do_buc_unknown = TRUE;
        num_buc_types++;
    }
    if ((qflags & UNIDENTIFIED) && count_buc(olist, UNIDENTIFIED))
        do_unidentified = TRUE;

    ccount = count_categories(olist, qflags);
    /* no point in actually showing a menu for a single category */
    if (ccount == 1 && !do_unpaid && num_buc_types <= 1 &&
        !(qflags & BILLED_TYPES)) {
        for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
            if ((qflags & WORN_TYPES) && !(curr->owornmask & W_EQUIP))
                continue;
            break;
        }
        if (curr) {
            int *pl = xmalloc(&turnstate.message_chain, sizeof (int));
            pl[0] = curr->oclass;
            *pick_list = pl;
            return 1;
        }
        return 0;
    }

    init_menulist(&menu);

    pack = flags.inv_order;
    invlet = 'a';
    if ((qflags & ALL_TYPES) && (ccount > 1)) {
        add_menuitem(&menu, ALL_TYPES_SELECTED,
                     (qflags & WORN_TYPES) ? "All worn types" : "All types",
                     invlet, FALSE);
        invlet = 'b';
    }

    do {
        collected_type_name = FALSE;
        for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
            if (curr->oclass == *pack) {
                if ((qflags & WORN_TYPES) && !(curr->owornmask & W_EQUIP))
                    continue;
                if (!collected_type_name) {
                    add_menuitem(&menu, curr->oclass, let_to_name(*pack, FALSE),
                                 invlet++, FALSE);
                    menu.items[menu.icount - 1].group_accel =
                        def_oc_syms[(int)objects[curr->otyp].oc_class];
                    collected_type_name = TRUE;
                }
            }
        }
        pack++;
        if (invlet >= 'u') {
            dealloc_menulist(&menu);
            impossible("query_category: too many categories");
            return 0;
        }
    } while (*pack);
    /* unpaid items if there are any */
    if (do_unpaid)
        add_menuitem(&menu, 'u', "Unpaid items", 'u', FALSE);

    /* billed items: checked by caller, so always include if BILLED_TYPES */
    if (qflags & BILLED_TYPES)
        add_menuitem(&menu, 'x', "Unpaid items already used up", 'x', FALSE);

    if (qflags & CHOOSE_ALL)
        add_menuitem(&menu, 'A',
                     (qflags & WORN_TYPES) ? "Auto-select every item being worn"
                     : "Auto-select every item", 'A', FALSE);

    if (do_unidentified)
        add_menuitem(&menu, 'I', "Unidentified Items", 'I', FALSE);

    /* items with b/u/c/unknown if there are any */
    if (do_blessed)
        add_menuitem(&menu, 'B', "Items known to be Blessed", 'B', FALSE);

    if (do_cursed)
        add_menuitem(&menu, 'C', "Items known to be Cursed", 'C', FALSE);

    if (do_uncursed)
        add_menuitem(&menu, 'U', "Items known to be Uncursed", 'U', FALSE);

    if (do_buc_unknown)
        add_menuitem(&menu, 'X', "Items of unknown B/C/U status", 'X', FALSE);

    n = display_menu(&menu, qstr, how, PLHINT_INVENTORY,
                     pick_list);

    if (n < 0)
        n = 0;  /* callers don't expect -1 */

    return n;
}


static int
count_categories(struct obj *olist, int qflags)
{
    char *pack;
    boolean counted_category;
    int ccount = 0;
    struct obj *curr;

    pack = flags.inv_order;
    do {
        counted_category = FALSE;
        for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
            if (curr->oclass == *pack) {
                if ((qflags & WORN_TYPES) && !(curr->owornmask & W_EQUIP))
                    continue;
                if (!counted_category) {
                    ccount++;
                    counted_category = TRUE;
                }
            }
        }
        pack++;
    } while (*pack);
    return ccount;
}

/* could we carry `obj'? if not, could we carry some of it/them? */
static long
carry_count(struct obj *obj,    /* object to pick up */
            struct obj *container,      /* bag it's coming out of */
            long count, boolean telekinesis, int *wt_before, int *wt_after)
{
    boolean adjust_wt = container &&
        carried(container), is_gold = obj->oclass == COIN_CLASS;
    int wt, iw, ow, oow;
    long qq, savequan;
    long umoney = money_cnt(invent);
    unsigned saveowt;
    const char *verb, *prefx1, *prefx2, *suffx, *obj_nambuf, *where;

    savequan = obj->quan;
    saveowt = obj->owt;

    iw = max_capacity();

    if (count != savequan) {
        obj->quan = count;
        obj->owt = (unsigned)weight(obj);
    }
    wt = iw + (int)obj->owt;
    if (adjust_wt)
        wt -= (container->otyp == BAG_OF_HOLDING) ?
            (int)DELTA_CWT(container, obj) : (int)obj->owt;
    /* This will go with silver+copper & new gold weight */
    if (is_gold)        /* merged gold might affect cumulative weight */
        wt -= (GOLD_WT(umoney) + GOLD_WT(count) - GOLD_WT(umoney + count));
    if (count != savequan) {
        obj->quan = savequan;
        obj->owt = saveowt;
    }
    *wt_before = iw;
    *wt_after = wt;

    if (wt < 0)
        return count;

    /* see how many we can lift */
    if (is_gold) {
        iw -= (int)GOLD_WT(umoney);
        if (!adjust_wt) {
            qq = GOLD_CAPACITY((long)iw, umoney);
        } else {
            oow = 0;
            qq = 50L - (umoney % 100L) - 1L;
            if (qq < 0L)
                qq += 100L;
            for (; qq <= count; qq += 100L) {
                obj->quan = qq;
                obj->owt = (unsigned)GOLD_WT(qq);
                ow = (int)GOLD_WT(umoney + qq);
                ow -= (container->otyp == BAG_OF_HOLDING) ?
                    (int)DELTA_CWT(container, obj) : (int)obj->owt;
                if (iw + ow >= 0)
                    break;
                oow = ow;
            }
            iw -= oow;
            qq -= 100L;
        }
        if (qq < 0L)
            qq = 0L;
        else if (qq > count)
            qq = count;
        wt = iw + (int)GOLD_WT(umoney + qq);
    } else if (count > 1 || count < obj->quan) {
        /* 
         * Ugh. Calc num to lift by changing the quan of of the
         * object and calling weight.
         *
         * This works for containers only because containers
         * don't merge.         -dean
         */
        for (qq = 1L; qq <= count; qq++) {
            obj->quan = qq;
            obj->owt = (unsigned)(ow = weight(obj));
            if (adjust_wt)
                ow -= (container->otyp == BAG_OF_HOLDING) ?
                    (int)DELTA_CWT(container, obj) : (int)obj->owt;
            if (iw + ow >= 0)
                break;
            wt = iw + ow;
        }
        --qq;
    } else {
        /* there's only one, and we can't lift it */
        qq = 0L;
    }
    obj->quan = savequan;
    obj->owt = saveowt;

    if (qq < count) {
        /* some message will be given */
        obj_nambuf = doname(obj);
        if (container) {
            where = msgprintf("in %s", the(xname(container)));
            verb = "carry";
        } else {
            where = "lying here";
            verb = telekinesis ? "acquire" : "lift";
        }
    } else {
        /* lint supppression */
        obj_nambuf = where = verb = "";
    }
    /* we can carry qq of them */
    if (qq > 0) {
        if (qq < count)
            pline(msgc_substitute, "You can only %s %s of the %s %s.", verb,
                  (qq == 1L) ? "one" : "some", obj_nambuf, where);
        *wt_after = wt;
        return qq;
    }

    if (!container)
        where = "here";  /* slightly shorter form */
    if (invent || umoney) {
        prefx1 = "you cannot ";
        prefx2 = "";
        suffx = " any more";
    } else {
        prefx1 = (obj->quan == 1L) ? "it " : "even one ";
        prefx2 = "is too heavy for you to ";
        suffx = "";
    }
    /* tries and fails, thus msgc_yafm, not msgc_cancelled or msgc_cancelled1 */
    pline(msgc_yafm, "There %s %s %s, but %s%s%s%s.", otense(obj, "are"),
          obj_nambuf, where, prefx1, prefx2, verb, suffx);

    /* *wt_after = iw; */
    return 0L;
}

/* determine whether character is able and player is willing to carry `obj' */
static int
lift_object(struct obj *obj, struct obj *container, long *cnt_p,
            boolean telekinesis)
{
    int result, old_wt, new_wt, prev_encumbr, next_encumbr;

    if (obj->otyp == BOULDER && In_sokoban(&u.uz)) {
        pline(msgc_yafm, "You cannot get your %s around this %s.",
              body_part(HAND), xname(obj));
        return -1;
    }
    if (obj->otyp == LOADSTONE)
        return 1;       /* lift regardless of current situation */

    *cnt_p = carry_count(obj, container, *cnt_p, telekinesis, &old_wt, &new_wt);
    if (obj->otyp == BOULDER && throws_rocks(youmonst.data))
        *cnt_p = 1;

    if (*cnt_p < 1L) {
        result = -1;    /* nothing lifted */
    } else if (!can_hold(obj)) {
        pline(msgc_yafm, "Your knapsack cannot accommodate any more items.");
        result = -1;    /* nothing lifted */
    } else {
        result = 1;
        prev_encumbr = near_capacity();
        if (prev_encumbr < flags.pickup_burden)
            prev_encumbr = flags.pickup_burden;
        next_encumbr = calc_capacity(new_wt - old_wt);
        if (next_encumbr > prev_encumbr) {
            if (telekinesis) {
                result = 0;     /* don't lift */
            } else {
                const char *qbuf;
                long savequan = obj->quan;

                obj->quan = *cnt_p;
                qbuf = ((next_encumbr > HVY_ENCUMBER) ? overloadmsg :
                        (next_encumbr > MOD_ENCUMBER) ? nearloadmsg :
                        moderateloadmsg);
                qbuf = msgprintf("%s %s. Continue?", qbuf,
                                 safe_qbuf(qbuf, sizeof (" . Continue?"),
                                           doname(obj),
                                           an(simple_typename(obj->otyp)),
                                           "something"));
                obj->quan = savequan;
                switch (ynq(qbuf)) {
                case 'q':
                    result = -1;
                    break;
                case 'n':
                    result = 0;
                    break;
                default:
                    break;      /* 'y' => result == 1 */
                }
            }
        }
    }

    if (obj->otyp == SCR_SCARE_MONSTER && result <= 0 && !container)
        obj->spe = 0;
    return result;
}

/* Shortens a prompt if it would be very long. This function was previously used
   to avoid buffer overflows; nowadays, its use is to keep the interface neater,
   because the buffers are no longer of fixed size. Thus, the limit is now
   COLNO-10 (a string that fits comfortably onscreen) rather than a fixed
   128. */
const char *
safe_qbuf(const char *qbuf, unsigned padlength, const char *planA,
          const char *planB, const char *last_resort)
{
    /* convert size_t (or int for ancient systems) to ordinary unsigned */
    unsigned len_qbuf = (unsigned)strlen(qbuf), len_planA =
        (unsigned)strlen(planA), len_planB =
        (unsigned)strlen(planB), len_lastR = (unsigned)strlen(last_resort);
    unsigned textleft = COLNO - 10 - (len_qbuf + padlength);

    if (len_lastR >= textleft) {
        impossible("safe_qbuf: last_resort too large at %u characters.",
                   len_lastR);
        return "";
    }
    return (len_planA < textleft) ? planA : (len_planB <
                                             textleft) ? planB : last_resort;
}

/* Pick up <count> of obj from the ground and add it to the hero's inventory.
   Returns -1 if caller should break out of its loop, 0 if nothing picked up, 1
   otherwise. */
int
pickup_object(struct obj *obj, long count, boolean telekinesis)
{       /* not picking it up directly by hand */
    int res, nearload;

    if (obj->quan < count) {
        impossible("pickup_object: count %ld > quan %ld?",
                   (long)count, (long)obj->quan);
        return 0;
    }

    /* In case of auto-pickup, where we haven't had a chance to look at it yet; 
       affects docall(SCR_SCARE_MONSTER). */
    if (!Blind)
#ifdef INVISIBLE_OBJECTS
        if (!obj->oinvis || See_invisible)
#endif
            obj->dknown = 1;

    if (obj == uchain) {        /* do not pick up attached chain */
        return 0;
    } else if (obj->oartifact && !touch_artifact(obj, &youmonst)) {
        return 0;
    } else if (obj->otyp == CORPSE) {
        if (feel_cockatrice(obj, TRUE, telekinesis ? "pulling in" :
                            "picking up"))
            return -1;

        if (is_rider(&mons[obj->corpsenm])) {
            pline(msgc_substitute, "At your %s, the corpse suddenly moves...",
                  telekinesis ? "attempted acquisition" : "touch");
            revive_corpse(obj);
            exercise(A_WIS, FALSE);
            return -1;
        }
    } else if (obj->otyp == SCR_SCARE_MONSTER) {
        if (obj->blessed)
            obj->blessed = 0;
        else if (!obj->spe && !obj->cursed)
            obj->spe = 1;
        else {
            pline(msgc_substitute,
                  "The scroll%s %s to dust as you %s %s up.", plur(obj->quan),
                  otense(obj, "turn"), telekinesis ? "raise" : "pick",
                  (obj->quan == 1L) ? "it" : "them");
            if (!(objects[SCR_SCARE_MONSTER].oc_name_known) &&
                !(objects[SCR_SCARE_MONSTER].oc_uname))
                docall(obj);
            useupf(obj, obj->quan);
            return 1;   /* tried to pick something up and failed, but don't
                           want to terminate pickup loop yet */
        }
    }

    if ((res = lift_object(obj, NULL, &count, telekinesis)) <= 0)
        return res;

    if (obj->quan != count && obj->otyp != LOADSTONE)
        obj = splitobj(obj, count);

    obj = pick_obj(obj);

    nearload = near_capacity();
    prinv(nearload == SLT_ENCUMBER ? moderateloadmsg : NULL, obj, count);
    return 1;
}

/*
 * Do the actual work of picking otmp from the floor or monster's interior
 * and putting it in the hero's inventory.  Take care of billing.  Return a
 * pointer to the object where otmp ends up.  This may be different
 * from otmp because of merging.
 */
struct obj *
pick_obj(struct obj *otmp)
{
    obj_extract_self(otmp);
    if (!Engulfed && otmp != uball && costly_spot(otmp->ox, otmp->oy)) {
        char saveushops[5], fakeshop[2];

        /* addtobill cares about your location rather than the object's;
           usually they'll be the same, but not when using telekinesis (if ever 
           implemented) or a grappling hook */
        strcpy(saveushops, u.ushops);
        fakeshop[0] = *in_rooms(level, otmp->ox, otmp->oy, SHOPBASE);
        fakeshop[1] = '\0';
        strcpy(u.ushops, fakeshop);
        /* sets obj->unpaid if necessary */
        addtobill(otmp, TRUE, FALSE, FALSE);
        strcpy(u.ushops, saveushops);
        /* if you're outside the shop, make shk notice */
        if (!strchr(u.ushops, *fakeshop))
            remote_burglary(otmp->ox, otmp->oy);
    }
    if (otmp->no_charge)        /* only applies to objects outside invent */
        otmp->no_charge = 0;
    if (otmp->olev == level)
        newsym(otmp->ox, otmp->oy);
    return addinv(otmp);        /* might merge it with other objects */
}

/*
 * Reset state for encumber_msg() so it doesn't bleed between games.
 */
void
reset_encumber_msg(void)
{
    u.oldcap = UNENCUMBERED;
}

/*
 * prints a message if encumbrance changed since the last check and
 * returns the new encumbrance value (from near_capacity()).
 */
int
encumber_msg(void)
{
    int newcap = near_capacity();

    if (u.oldcap < newcap) {
        switch (newcap) {
        case 1:
            pline(msgc_statusbad,
                  "Your movements are slowed slightly because of your load.");
            break;
        case 2:
            pline(msgc_statusbad,
                  "You rebalance your load.  Movement is difficult.");
            break;
        case 3:
            pline(msgc_statusbad,
                  "You %s under your heavy load.  Movement is very hard.",
                  stagger(youmonst.data, "stagger"));
            break;
        default:
            pline(msgc_fatal, "You %s move a handspan with this load!",
                  newcap == 4 ? "can barely" : "can't even");
            break;
        }
    } else if (u.oldcap > newcap) {
        switch (newcap) {
        case 0:
            pline(msgc_statusheal, "Your movements are now unencumbered.");
            break;
        case 1:
            pline(msgc_statusheal,
                  "Your movements are only slowed slightly by your load.");
            break;
        case 2:
            pline(msgc_statusheal,
                  "You rebalance your load.  Movement is still difficult.");
            break;
        case 3:
            pline(msgc_statusheal,
                  "You %s under your load.  Movement is still very hard.",
                  stagger(youmonst.data, "stagger"));
            break;
        }
    }

    u.oldcap = newcap;
    return newcap;
}

/* Is there a container at x,y. Optional: return count of containers at x,y */
static int
container_at(int x, int y, boolean countem)
{
    struct obj *cobj, *nobj;
    int container_count = 0;

    for (cobj = level->objects[x][y]; cobj; cobj = nobj) {
        nobj = cobj->nexthere;
        if (Is_container(cobj)) {
            container_count++;
            if (!countem)
                break;
        }
    }
    return container_count;
}

static boolean
able_to_loot(int x, int y)
{
    if (!can_reach_floor()) {
        if (u.usteed && P_SKILL(P_RIDING) < P_BASIC)
            rider_cant_reach(); /* not skilled enough to reach */
        else
            pline(msgc_cancelled, "You cannot reach the %s.", surface(x, y));
        return FALSE;
    } else if (is_pool(level, x, y) || is_lava(level, x, y)) {
        /* at present, can't loot in water even when Underwater */
        pline(msgc_cancelled, "You cannot loot things that are deep in the %s.",
              is_lava(level, x, y) ? "lava" : "water");
        return FALSE;
    } else if (nolimbs(youmonst.data)) {
        pline(msgc_cancelled, "Without limbs, you cannot loot anything.");
        return FALSE;
    } else if (!freehand()) {
        pline(msgc_cancelled, "Without a free %s, you cannot loot anything.",
              body_part(HAND));
        return FALSE;
    }
    return TRUE;
}

static boolean
mon_beside(int x, int y)
{
    int i, j, nx, ny;

    for (i = -1; i <= 1; i++)
        for (j = -1; j <= 1; j++) {
            nx = x + i;
            ny = y + j;
            if (isok(nx, ny) && MON_AT(level, nx, ny))
                return TRUE;
        }
    return FALSE;
}

static boolean
Is_container_func(const struct obj *otmp)
{
    return Is_container(otmp);
}

/* loot a container on the floor or loot saddle from mon. */
int
doloot(const struct nh_cmd_arg *arg)
{
    struct obj *cobj;
    int c = -1;
    int timepassed = 0;
    coord cc;
    boolean underfoot = TRUE;
    const char *dont_find_anything = "don't find anything";
    struct monst *mtmp;
    int prev_inquiry = 0;
    boolean prev_loot = FALSE;
    int container_count = 0;

    /* We don't allow directions, for now. TODO: In general, this function
       needs an entirely different UI anyway (e.g. menu-driven). */
    (void) arg;

    if (check_capacity(NULL)) {
        /* "Can't do that while carrying so much stuff." */
        return 0;
    }
    if (nohands(youmonst.data)) {
        pline(msgc_cancelled, "You have no hands!"); /* not `body_part(HAND)' */
        return 0;
    }
    cc.x = u.ux;
    cc.y = u.uy;

lootcont:

    if ((container_count = container_at(cc.x, cc.y, TRUE))) {
        struct object_pick *lootlist;
        int i, n;

        if (!able_to_loot(cc.x, cc.y))
            return 0;

        n = query_objlist("Loot which containers?", level->objects[cc.x][cc.y],
                          BY_NEXTHERE | SIGNAL_ESCAPE | AUTOSELECT_SINGLE,
                          &lootlist, PICK_ANY, Is_container_func);

        if (n < 0) {
            return 0;
        } else if (n == 0) {
            c = 'n';
        } else if (n > 0) {
            c = 'y';
            for (i = 0; i < n; i++) {
                cobj = lootlist[i].obj;
                if (cobj->olocked) {
                    pline(msgc_failcurse, "Hmmm, it seems to be locked.");
                    continue;
                }
                if (cobj->otyp == BAG_OF_TRICKS) {
                    int tmp;

                    pline_implied(msgc_occstart,
                                  "You carefully open the bag...");
                    pline(msgc_substitute,
                          "It develops a huge set of teeth and bites you!");
                    tmp = rnd(10);
                    if (Half_physical_damage)
                        tmp = (tmp + 1) / 2;
                    losehp(tmp, killer_msg(DIED, "a carnivorous bag"));
                    makeknown(BAG_OF_TRICKS);
                    free(lootlist);
                    return 1;
                }

                pline(msgc_occstart, "You carefully open %s...",
                      the(xname(cobj)));
                timepassed |= use_container(cobj, 0);
                if (u_helpless(hm_all)) {        /* e.g. a chest trap */
                    free(lootlist);
                    return 1;
                }
            }
            free(lootlist);
        }
    } else if (Confusion) {
        struct obj *goldob;

        /* Find a money object to mess with */
        for (goldob = invent; goldob; goldob = goldob->nobj) {
            if (goldob->oclass == COIN_CLASS)
                break;
        }
        if (goldob) {
            long contribution = rnd((int)min(LARGEST_INT, goldob->quan));

            if (contribution < goldob->quan)
                goldob = splitobj(goldob, contribution);
            unwield_silently(goldob);
            freeinv(goldob);

            if (IS_THRONE(level->locations[u.ux][u.uy].typ)) {
                struct obj *coffers = NULL;
                int pass;

                /* find the original coffers chest, or any chest */
                for (pass = 2; pass > -1; pass -= 2)
                    for (coffers = level->objlist; coffers;
                         coffers = coffers->nobj)
                        if (coffers->otyp == CHEST && coffers->spe == pass)
                            goto gotit; /* two level break */
            gotit:
                if (coffers) {
                    verbalize
                        (msgc_npcvoice,
                         "Thank you for your contribution to reduce the debt.");
                    add_to_container(coffers, goldob);
                    coffers->owt = weight(coffers);
                } else {
                    struct monst *mon = makemon(courtmon(&u.uz, rng_main),
                                                level, u.ux, u.uy, NO_MM_FLAGS);

                    if (mon) {
                        add_to_minv(mon, goldob);
                        pline(msgc_levelwarning,
                              "The exchequer accepts your contribution.");
                    } else {
                        dropy(goldob);
                    }
                }
            } else {
                dropy(goldob);
                pline(msgc_substitute, "Ok, now there is loot here.");
            }
        }
    } else if (IS_GRAVE(level->locations[cc.x][cc.y].typ)) {
        pline(msgc_hint,
              "You need to dig up the grave to effectively loot it...");
    }
    /* 
     * 3.3.1 introduced directional looting for some things.
     */
    if (c != 'y' && mon_beside(u.ux, u.uy)) {
        schar dz;

        if (!get_adjacent_loc
            ("Loot in what direction?", "Invalid loot location", u.ux, u.uy,
             &cc, &dz))
            return 0;
        if (cc.x == u.ux && cc.y == u.uy) {
            underfoot = TRUE;
            if (container_at(cc.x, cc.y, FALSE))
                goto lootcont;
        } else
            underfoot = FALSE;
        if (dz < 0) {
            pline(msgc_cancelled1, "You %s to loot on the %s.",
                  dont_find_anything, ceiling(cc.x, cc.y));
            timepassed = 1;
            return timepassed;
        }
        mtmp = m_at(level, cc.x, cc.y);
        if (mtmp)
            timepassed = loot_mon(mtmp, &prev_inquiry, &prev_loot);

        /* Preserve pre-3.3.1 behaviour for containers. Adjust this if-block to 
           allow container looting from one square away to change that in the
           future. */
        if (!underfoot) {
            if (container_at(cc.x, cc.y, FALSE)) {
                if (mtmp) {
                    pline(msgc_cancelled,
                          "You can't loot anything %sthere with %s in the way.",
                          prev_inquiry ? "else " : "", mon_nam(mtmp));
                    return timepassed;
                } else {
                    pline(msgc_cancelled,
                          "You have to be at a container to loot it.");
                }
            } else {
                pline(msgc_cancelled, "You %s %sthere to loot.",
                      dont_find_anything,
                      (prev_inquiry || prev_loot) ? "else " : "");
                return timepassed;
            }
        }
    } else if (c != 'y' && c != 'n') {
        pline(msgc_cancelled, "You %s %s to loot.", dont_find_anything,
              underfoot ? "here" : "there");
    }
    return timepassed;
}

/* loot_mon() returns amount of time passed. */
int
loot_mon(struct monst *mtmp, int *passed_info, boolean * prev_loot)
{
    int c = -1;
    int timepassed = 0;
    struct obj *otmp;
    const char *qbuf;

    /* 3.3.1 introduced the ability to remove saddle from a steed */
    /* *passed_info is set to TRUE if a loot query was given.  */
    /* *prev_loot is set to TRUE if something was actually acquired in here. */
    if (mtmp && mtmp != u.usteed && (otmp = which_armor(mtmp, os_saddle))) {
        long unwornmask;

        if (passed_info)
            *passed_info = 1;
        qbuf = msgprintf("Do you want to remove the saddle from %s?",
                         x_monnam(mtmp, ARTICLE_THE, NULL,
                                  SUPPRESS_SADDLE, FALSE));
        if ((c = yn_function(qbuf, ynqchars, 'n')) == 'y') {
            if (nolimbs(youmonst.data)) {
                pline(msgc_cancelled, "You can't do that without limbs.");
                /* not body_part(HAND); we're talking about an appendage that
                   we /don't/ have */
                return 0;
            }
            if (otmp->cursed) {
                pline(otmp->bknown ? msgc_cancelled1 : msgc_failcurse,
                      "You can't. The saddle seems to be stuck to %s.",
                      x_monnam(mtmp, ARTICLE_THE, NULL, SUPPRESS_SADDLE,
                               FALSE));
                otmp->bknown = TRUE;
                /* the attempt costs you time */
                return 1;
            }
            obj_extract_self(otmp);
            if ((unwornmask = otmp->owornmask) != 0L) {
                mtmp->misc_worn_check &= ~unwornmask;
                otmp->owornmask = 0L;
                update_mon_intrinsics(mtmp, otmp, FALSE, FALSE);
            }
            hold_another_object(otmp, "You drop %s!", doname(otmp), NULL);
            timepassed = rnd(3);
            if (prev_loot)
                *prev_loot = TRUE;
        } else if (c == 'q') {
            return 0;
        }
    }
    /* 3.4.0 introduced the ability to pick things up from within swallower's
       stomach */
    if (Engulfed) {
        int count = passed_info ? *passed_info : 0;

        /* override user interaction mode, the user explicitly asked us to
           pick items up */
        timepassed = pickup(count, uim_standard);
    }
    return timepassed;
}

/* Decide whether an object being placed into a magic bag will cause it to
   explode.  If the object is a bag itself, check recursively. 

   Return value: 0 = no explosion, 1 = explosion, 2 = disallow one item */
static int
mbag_explodes(struct obj *obj, int depthin)
{
    /* these won't cause an explosion when they're empty */
    if ((obj->otyp == WAN_CANCELLATION || obj->otyp == BAG_OF_TRICKS) &&
        obj->spe <= 0)
        return 0;
    else if (obj->otyp == WAN_CANCELLATION || obj->otyp == BAG_OF_TRICKS) {
        if (obj->where == OBJ_CONTAINED)
            pline(msgc_substitute,
                  "As you nest the bags, you see a crazily bright glow.");
        else
            pline(msgc_substitute,
                  "As you put %s inside, it glows crazily bright for a while.",
                  doname(obj));
        pline(msgc_itemloss,
              "Then you feel a strange absence of magical power...");
        obj->spe = 0;   /* charge a lot for the free identify, as this would
                           have destroyed the item and the bag in vanila */
        obj->known = TRUE;      /* mark the absence of charges, so players know
                                   what happened */
        /* TODO: alert monsters that can see within LOE? */
        makeknown(obj->otyp);
        return 0;
    } else if (Is_mbag(obj) && depthin < 2) {
        /* There's a legitimate use to try nesting BoHes, but we don't want it
           to happen by mistake. You always get an explosion unless there are at
           least two layers of sacks in between, and so "100% chance of things
           going wrong" is a good heuristic to see if it's intentional. */
        pline(msgc_yafm,
              "You feel too much resistance trying to nest the bags.");
        pline_implied(
            msgc_hint,
            "Perhaps you need more padding? But that could be risky...");
        return 2;
    }

    /* odds: 1/1, 2/2, 3/4, 4/8, 5/16, 6/32, 7/64, 8/128, 9/128, 10/128,...

       TODO: Might potentially use a custom RNG here, but it's too rare an
       action to be worth it. */
    if ((Is_mbag(obj) || obj->otyp == WAN_CANCELLATION) &&
        (rn2(1 << (depthin > 7 ? 7 : depthin)) <= depthin))
        return 1;
    else if (Has_contents(obj)) {
        struct obj *otmp;

        for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
            int i = mbag_explodes(otmp, depthin + 1);

            if (i)
                return i;
        }
    }
    return 0;
}

/* A variable set in use_container(), to be used by the callback routines   */
/* in_container(), and out_container() from askchain() and use_container(). */
static struct obj *current_container;

#define Icebox (current_container->otyp == ICE_BOX)

/* Returns: -1 to stop, 1 item was inserted, 0 item was not inserted. */
static int
in_container(struct obj *obj)
{
    boolean floor_container = !carried(current_container);
    boolean was_unpaid = FALSE;
    int mbag_explodes_reason = 0;

    if (obj == uball || obj == uchain) {
        pline(msgc_yafm, "You must be kidding.");
        return 0;
    } else if (obj == current_container && objects[obj->otyp].oc_name_known &&
               obj->otyp == BAG_OF_HOLDING) {
        pline(msgc_yafm,
              "Creating an artificial black hole could ruin your whole day!");
        return 0;
    } else if (obj == current_container) {
        pline(msgc_yafm, "That would be an interesting topological exercise.");
        return 0;
    }
    /* must check for obj == current_container before doing this */
    if (Is_mbag(current_container))
        mbag_explodes_reason = mbag_explodes(obj, 0);
    if (mbag_explodes_reason == 2)
        return 0;

    if (obj->owornmask & W_WORN) {
        pline_once(msgc_yafm, "You cannot %s something you are wearing.",
                   Icebox ? "refrigerate" : "stash");
        return 0;
    } else if ((obj->otyp == LOADSTONE) && obj->cursed) {
        obj->bknown = 1;
        pline(msgc_failcurse, "The stone%s won't leave your person.",
              plur(obj->quan));
        return 0;
    } else if (obj->otyp == AMULET_OF_YENDOR ||
               obj->otyp == CANDELABRUM_OF_INVOCATION ||
               obj->otyp == BELL_OF_OPENING ||
               obj->otyp == SPE_BOOK_OF_THE_DEAD) {
        /* Prohibit Amulets in containers; if you allow it, monsters can't
           steal them.  It also becomes a pain to check to see if someone has
           the Amulet.  Ditto for the Candelabrum, the Bell and the Book. */
        pline(msgc_failcurse, "%s cannot be confined in such trappings.",
              The(xname(obj)));
        return 0;
    } else if (obj->otyp == LEASH && obj->leashmon != 0) {
        pline(msgc_failcurse, "%s attached to your pet.", Tobjnam(obj, "are"));
        return 0;
    } else if (obj == uwep) {
        if (welded(obj)) {
            weldmsg(msgc_yafm, obj);
            return 0;
        }
        setuwep(NULL);
        if (uwep)
            return 0;   /* unwielded, died, rewielded */
    } else if (obj == uswapwep) {
        setuswapwep(NULL);
        if (uswapwep)
            return 0;   /* unwielded, died, rewielded */
    } else if (obj == uquiver) {
        setuqwep(NULL);
        if (uquiver)
            return 0;   /* unwielded, died, rewielded */
    }

    if (obj->otyp == CORPSE) {
        if (feel_cockatrice(obj, TRUE, "stashing away"))
            return -1;
    }

    /* boxes, boulders, and big statues can't fit into any container */
    if (obj->otyp == ICE_BOX || Is_box(obj) || obj->otyp == BOULDER ||
        (obj->otyp == STATUE && bigmonst(&mons[obj->corpsenm]))) {
        pline(msgc_failcurse, "You cannot fit %s into %s.",
              the(xname(obj)),
              the(xname(current_container)));
        return 0;
    }

    freeinv(obj);

    if (obj_is_burning(obj))    /* this used to be part of freeinv() */
        snuff_lit(obj);

    if (floor_container && costly_spot(u.ux, u.uy)) {
        if (current_container->no_charge && !obj->unpaid) {
            /* don't sell when putting the item into your own container */
            obj->no_charge = 1;
        } else if (obj->oclass != COIN_CLASS) {
            /* sellobj() will take an unpaid item off the shop bill note: coins 
               are handled later */
            was_unpaid = obj->unpaid ? TRUE : FALSE;
            sellobj_state(SELL_DELIBERATE);
            sellobj(obj, u.ux, u.uy);
            sellobj_state(SELL_NORMAL);
        }
    }
    if (Icebox && !age_is_relative(obj)) {
        obj->age = moves - obj->age;    /* actual age */
        /* stop any corpse timeouts when frozen */
        if (obj->otyp == CORPSE && obj->timed) {
            long rot_alarm = stop_timer(obj->olev, ROT_CORPSE, obj);

            stop_timer(obj->olev, REVIVE_MON, obj);
            /* mark a non-reviving corpse as such */
            if (rot_alarm)
                obj->norevive = 1;
        }
    } else if (mbag_explodes_reason == 1) {
        /* explicitly mention what item is triggering the explosion */
        pline(msgc_substitute,
              "As you put %s inside, you are blasted by a magical explosion!",
              doname(obj));
        /* did not actually insert obj yet */
        if (was_unpaid)
            addtobill(obj, FALSE, FALSE, TRUE);
        obfree(obj, NULL);
        delete_contents(current_container);
        if (!floor_container)
            useup(current_container);
        else if (obj_here(current_container, u.ux, u.uy))
            useupf(current_container, obj->quan);
        else
            panic("in_container:  bag not found.");

        losehp(dice(6, 6), killer_msg(DIED, "a magical explosion"));
        current_container = NULL;       /* baggone = TRUE; */

        action_interrupted();
    }

    if (current_container) {
        pline(msgc_actionboring, "You put %s into %s.", doname(obj),
              the(xname(current_container)));

        /* gold in container always needs to be added to credit */
        if (floor_container && obj->oclass == COIN_CLASS)
            sellobj(obj, current_container->ox, current_container->oy);
        add_to_container(current_container, obj);
        current_container->owt = weight(current_container);
    }
    /* gold needs this, and freeinv() many lines above may cause the
       encumbrance to disappear from the status, so just always update status
       immediately. */
    bot();

    return current_container ? 1 : -1;
}


/* Returns: -1 to stop, 1 item was removed, 0 item was not removed. */
static int
out_container(struct obj *obj)
{
    struct obj *otmp;
    boolean is_gold = (obj->oclass == COIN_CLASS);
    int res, loadlev;
    long count;

    if (!current_container) {
        impossible("<out> no current_container?");
        return -1;
    } else if (is_gold) {
        obj->owt = weight(obj);
    }

    if (obj->oartifact && !touch_artifact(obj, &youmonst))
        return 0;

    if (obj->otyp == CORPSE) {
        if (feel_cockatrice(obj, TRUE, "retrieving"))
            return -1;
    }

    count = obj->quan;
    if ((res = lift_object(obj, current_container, &count, FALSE)) <= 0)
        return res;

    if (obj->quan != count && obj->otyp != LOADSTONE)
        obj = splitobj(obj, count);

    /* Remove the object from the list. */
    obj_extract_self(obj);
    current_container->owt = weight(current_container);

    if (Icebox && !age_is_relative(obj)) {
        obj->age = moves - obj->age;    /* actual age */
        if (obj->otyp == CORPSE)
            start_corpse_timeout(obj);
    }
    /* simulated point of time */

    if (!obj->unpaid && !carried(current_container) &&
        costly_spot(current_container->ox, current_container->oy)) {
        obj->ox = current_container->ox;
        obj->oy = current_container->oy;
        addtobill(obj, FALSE, FALSE, FALSE);
    }
    if (is_pick(obj) && !obj->unpaid && *u.ushops &&
        shop_keeper(level, *u.ushops))
        verbalize(msgc_npcvoice,
                  "You sneaky cad! Get out of here with that pick!");

    otmp = addinv(obj);
    loadlev = near_capacity();
    prinv(loadlev
          ? (loadlev <
             MOD_ENCUMBER ? "You have a little trouble removing" :
             "You have much trouble removing") : NULL, otmp, count);

    if (is_gold)
        bot();  /* update character's gold piece count immediately */

    return 1;
}

/* an object inside a cursed bag of holding is being destroyed */
static long
mbag_item_gone(int held, struct obj *item)
{
    struct monst *shkp;
    long loss = 0L;

    if (item->dknown)
        pline(msgc_itemloss, "%s %s vanished!",
              Doname2(item), otense(item, "have"));
    else
        pline(msgc_itemloss, "You %s %s disappear!", Blind ? "notice" : "see",
              doname(item));

    if (*u.ushops && (shkp = shop_keeper(level, *u.ushops)) != 0) {
        if (held ? (boolean) item->unpaid : costly_spot(u.ux, u.uy))
            loss =
                stolen_value(item, u.ux, u.uy, (boolean) shkp->mpeaceful, TRUE);
    }
    obfree(item, NULL);
    return loss;
}

void
observe_quantum_cat(struct obj *box)
{
    static const char sc[] = "Schroedinger's Cat";
    struct obj *deadcat;
    struct monst *livecat;
    xchar ox, oy;

    box->spe = 0;       /* box->owt will be updated below */
    if (get_obj_location(box, &ox, &oy, 0))
        box->ox = ox, box->oy = oy;     /* in case it's being carried */

    /* This isn't really right, since any form of observation (telepathic or
       monster/object/food detection) ought to force the determination of alive 
       vs dead state; but basing it just on opening the box is much simpler to
       cope with.

       This /must/ be on rng_main: the whole point is that the fate of the cat
       isn't determined until you open the box. (To really do things correctly,
       we should do an entropy collection on rng_main just beforehand, but
       that'd require logging in the save, which would be a bunch of extra
       complexity.) */
    livecat = rn2(2) ?
        makemon(&mons[PM_HOUSECAT], level, box->ox, box->oy, NO_MINVENT) : 0;
    if (livecat) {
        msethostility(livecat, FALSE, TRUE);
        if (!canspotmon(livecat))
            pline(msgc_levelsound, "You think something brushed your %s.",
                  body_part(FOOT));
        else
            pline(msgc_monneutral, "%s inside the box is still alive!",
                  Monnam(livecat));
        christen_monst(livecat, sc);
    } else {
        deadcat =
            mk_named_object(CORPSE, &mons[PM_HOUSECAT], box->ox, box->oy, sc);
        if (deadcat) {
            obj_extract_self(deadcat);
            add_to_container(box, deadcat);
        }
        if (Hallucination) {
            int idx = rndmonidx();
            boolean pname = monnam_is_pname(idx);

            pline(msgc_monneutral, "%s%s inside the box%s is dead!",
                  pname ? monnam_for_index(idx)
                  : The(monnam_for_index(idx)), pname ? "," : "",
                  pname ? "," : "");
        } else {
            pline(msgc_monneutral, "The housecat inside the box is dead!");
        }

    }
    box->owt = weight(box);
    return;
}

#undef Icebox

int
use_container(struct obj *obj, int held)
{
    struct obj *curr, *otmp;
    boolean quantum_cat = FALSE, loot_out = FALSE, loot_in = FALSE;
    const char *qbuf, *emptymsg;
    long loss = 0L;
    int cnt = 0, used = 0, menu_on_request;

    emptymsg = "";
    if (nohands(youmonst.data)) {
        pline(msgc_cancelled, "You have no hands!"); /* not `body_part(HAND)' */
        return 0;
    } else if (!freehand()) {
        pline(msgc_cancelled, "You have no free %s.", body_part(HAND));
        return 0;
    }
    if (obj->olocked) {
        pline(msgc_cancelled, "%s to be locked.", Tobjnam(obj, "seem"));
        if (held)
            pline_implied(msgc_hint, "You must put it down to unlock.");
        return 0;
    } else if (obj->otrapped) {
        if (held)
            pline(msgc_occstart, "You open %s...", the(xname(obj)));
        chest_trap(obj, HAND, FALSE);
        /* even if the trap fails, you've used up this turn */
        action_interrupted();
        return 1;
    }
    current_container = obj;    /* for use by in/out_container */

    if (obj->spe == 1) {
        observe_quantum_cat(obj);
        used = 1;
        quantum_cat = TRUE;     /* for adjusting "it's empty" message */
    }
    /* Count the number of contained objects. Sometimes toss objects if a cursed
       magic bag. Don't use a custom RNG: we can't know how many items were
       placed into the bag (and multiple games can't get the same bones file, so
       there's no point in synchronizing that either). */
    for (curr = obj->cobj; curr; curr = otmp) {
        otmp = curr->nobj;
        if (Is_mbag(obj) && obj->cursed && !rn2(13)) {
            obj_extract_self(curr);
            loss += mbag_item_gone(held, curr);
            used = 1;
        } else {
            cnt++;
        }
    }

    if (loss)   /* magic bag lost some shop goods */
        pline(msgc_unpaid, "You owe %ld %s for lost merchandise.",
              loss, currency(loss));
    obj->owt = weight(obj);     /* in case any items were lost */

    if (!cnt)
        emptymsg = msgprintf("%s is %sempty.", Yname2(obj),
                             quantum_cat ? "now " : "");

    if (cnt || flags.menu_style == MENU_FULL) {
        qbuf = "Do you want to take something out of ";
        qbuf = msgprintf("%s%s?", qbuf,
                         safe_qbuf(qbuf, 1, yname(obj),
                                   ysimple_name(obj), "it"));

        if (flags.menu_style == MENU_FULL) {
            int t;
            char menuprompt[BUFSZ];
            boolean outokay = (cnt != 0);
            boolean inokay = (invent != 0);

            if (!outokay && !inokay) {
                pline(msgc_info, "%s", emptymsg);
                pline(msgc_info, "You don't have anything to put in.");
                return used;
            }
            menuprompt[0] = '\0';
            if (!cnt)
                snprintf(menuprompt, SIZE(menuprompt), "%s ", emptymsg);
            strcat(menuprompt, "Do what?");
            t = in_or_out_menu(menuprompt, current_container, outokay, inokay);
            if (t <= 0)
                return 0;
            loot_out = (t & 0x01) != 0;
            loot_in = (t & 0x02) != 0;
        } else {        /* MENU_PARTIAL */
            loot_out = (yn_function(qbuf, ynqchars, 'n') == 'y');
        }

        if (loot_out) {
            add_valid_menu_class(0);    /* reset */
            used |= menu_loot(0, current_container, FALSE) > 0;
        }

    } else {
        pline(msgc_info, "%s", emptymsg);  /* <whatever> is empty. */
    }

    if (!invent) {
        /* nothing to put in, but some feedback is necessary */
        pline(msgc_info, "You don't have anything to put in.");
        return used;
    }
    if (flags.menu_style != MENU_FULL) {
        qbuf = "Do you wish to put something in?";

        switch (yn_function(qbuf, ynqchars, 'n')) {
        case 'y':
            loot_in = TRUE;
            break;
        case 'n':
            break;
        case 'm':
            add_valid_menu_class(0);    /* reset */
            menu_on_request = -2;       /* triggers ALL_CLASSES */
            used |= menu_loot(menu_on_request, current_container, TRUE) > 0;
            break;
        case 'q':
        default:
            return used;
        }
    }
    /* 
     * Gone: being nice about only selecting food if we know we are
     * putting things in an ice chest.
     */
    if (loot_in) {
        add_valid_menu_class(0);        /* reset */
        used |= menu_loot(0, current_container, TRUE) > 0;
    }

    return used;
}

/* Loot a container (take things out, put things in), using a menu. */
static int
menu_loot(int retry, struct obj *container, boolean put_in)
{
    int n, i, n_looted = 0;
    boolean all_categories = TRUE, loot_everything = FALSE;
    const char *buf;
    const char *takeout = "Take out", *putin = "Put in";
    struct obj *otmp, *otmp2;
    const int *pick_list;
    struct object_pick *obj_pick_list;
    int mflags, res;
    long count;

    if (retry) {
        all_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
        all_categories = FALSE;
        buf = msgprintf("%s what type of objects?", put_in ? putin : takeout);
        mflags =
            put_in ? ALL_TYPES | BUC_ALLBKNOWN | BUC_UNKNOWN | UNIDENTIFIED :
            ALL_TYPES | CHOOSE_ALL | BUC_ALLBKNOWN | BUC_UNKNOWN | UNIDENTIFIED;
        n = query_category(buf, put_in ? invent : container->cobj, mflags,
                           &pick_list, PICK_ANY);
        if (!n)
            return 0;
        for (i = 0; i < n; i++) {
            if (pick_list[i] == 'A')
                loot_everything = TRUE;
            else if (pick_list[i] == ALL_TYPES_SELECTED)
                all_categories = TRUE;
            else
                add_valid_menu_class(pick_list[i]);
        }
    }

    if (loot_everything) {
        for (otmp = container->cobj; otmp; otmp = otmp2) {
            otmp2 = otmp->nobj;
            res = out_container(otmp);
            if (res < 0)
                break;
        }
    } else {
        mflags = INVORDER_SORT;
        if (put_in)
            mflags |= USE_INVLET;
        buf = msgprintf("%s what?", put_in ? putin : takeout);
        n = query_objlist(buf, put_in ? invent : container->cobj, mflags,
                          &obj_pick_list, PICK_ANY,
                          all_categories ? allow_all : allow_category);
        if (n) {
            n_looted = n;
            for (i = 0; i < n; i++) {
                otmp = obj_pick_list[i].obj;
                count = obj_pick_list[i].count;
                if (count > 0 && count < otmp->quan) {
                    otmp = splitobj(otmp, count);
                    /* special split case also handled by askchain() */
                }
                res = put_in ? in_container(otmp) : out_container(otmp);
                if (res < 0) {
                    if (otmp != obj_pick_list[i].obj) {
                        /* split occurred, merge again */
                        merged(&obj_pick_list[i].obj, &otmp);
                    }
                    break;
                }
            }
            free(obj_pick_list);
        }
    }
    return n_looted;
}

static int
in_or_out_menu(const char *prompt, struct obj *obj, boolean outokay,
               boolean inokay)
{
    struct nh_menuitem items[3];
    const int *selection;
    const char *buf;
    int n, nr = 0;

    if (outokay) {
        buf = msgprintf("Take something out of %s", the(xname(obj)));
        set_menuitem(&items[nr++], 1, MI_NORMAL, buf, 'o', FALSE);
    }

    if (inokay) {
        buf = msgprintf("Put something into %s", the(xname(obj)));
        set_menuitem(&items[nr++], 2, MI_NORMAL, buf, 'i', FALSE);
    }

    if (outokay && inokay)
        set_menuitem(&items[nr++], 3, MI_NORMAL, "Both of the above", 'b',
                     FALSE);

    n = display_menu(&(struct nh_menulist){.items = items, .icount = nr},
                     prompt, PICK_ONE, PLHINT_CONTAINER, &selection);
    if (n > 0)
        n = selection[0];

    return n;
}

/*pickup.c*/

