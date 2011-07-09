/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 *	Contains code for picking objects up, and container use.
 */

#include "hack.h"

static void check_here(boolean);
static boolean n_or_more(struct obj *);
static boolean all_but_uchain(struct obj *);
static int autopick(struct obj*, int, struct object_pick **);
static int count_categories(struct obj *,int);
static long carry_count (struct obj *,struct obj *,long,boolean,int *,int *);
static int lift_object(struct obj *,struct obj *,long *,boolean);
static boolean mbag_explodes(struct obj *,int);
static int in_container(struct obj *);
static int out_container(struct obj *);
static long mbag_item_gone(int,struct obj *);
static void observe_quantum_cat(struct obj *);
static int menu_loot(int, struct obj *, boolean);
static int in_or_out_menu(const char *,struct obj *, boolean, boolean);
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
#define DELTA_CWT(cont,obj)		\
    ((cont)->cursed ? (obj)->owt * 2 :	\
		      1 + ((obj)->owt / ((cont)->blessed ? 4 : 2)))
#define GOLD_WT(n)		(((n) + 50L) / 100L)
/* if you can figure this out, give yourself a hearty pat on the back... */
#define GOLD_CAPACITY(w,n)	(((w) * -100L) - ((n) + 50L) - 1L)

static const char moderateloadmsg[] = "You have a little trouble lifting";
static const char nearloadmsg[] = "You have much trouble lifting";
static const char overloadmsg[] = "You have extreme difficulty lifting";

#ifndef GOLDOBJ
int collect_obj_classes(char ilets[], struct obj *otmp, boolean here,
			boolean incl_gold, boolean (*filter)(struct obj*),
			int *itemcount)
#else
int collect_obj_classes(char ilets[], struct obj *otmp, boolean here,
			boolean (*filter)(struct obj*), int *itemcount)
#endif
{
	int iletct = 0;
	char c;

	*itemcount = 0;
#ifndef GOLDOBJ
	if (incl_gold)
	    ilets[iletct++] = def_oc_syms[COIN_CLASS];
#endif
	ilets[iletct] = '\0'; /* terminate ilets so that index() will work */
	while (otmp) {
	    c = def_oc_syms[(int)otmp->oclass];
	    if (!index(ilets, c) && (!filter || (*filter)(otmp)))
		ilets[iletct++] = c,  ilets[iletct] = '\0';
	    *itemcount += 1;
	    otmp = here ? otmp->nexthere : otmp->nobj;
	}

	return iletct;
}


/* look at the objects at our location, unless there are too many of them */
static void check_here(boolean picked_some)
{
	struct obj *obj;
	int ct = 0;

	/* count the objects here */
	for (obj = level.objects[u.ux][u.uy]; obj; obj = obj->nexthere) {
	    if (obj != uchain)
		ct++;
	}

	/* If there are objects here, take a look. */
	if (ct) {
	    if (flags.run) nomul(0);
	    flush_screen(1);
	    look_here(ct, picked_some);
	} else {
	    read_engr_at(u.ux,u.uy);
	}
}

/* Value set by query_objlist() for n_or_more(). */
static long val_for_n_or_more;

/* query_objlist callback: return TRUE if obj's count is >= reference value */
static boolean n_or_more(struct obj *obj)
{
    if (obj == uchain) return FALSE;
    return obj->quan >= val_for_n_or_more;
}

/* List of valid menu classes for query_objlist() and allow_category callback */
static char valid_menu_classes[MAXOCLASSES + 2];

void add_valid_menu_class(int c)
{
	static int vmc_count = 0;

	if (c == 0)  /* reset */
	  vmc_count = 0;
	else
	  valid_menu_classes[vmc_count++] = (char)c;
	valid_menu_classes[vmc_count] = '\0';
}

/* query_objlist callback: return TRUE if not uchain */
static boolean all_but_uchain(struct obj *obj)
{
    return obj != uchain;
}

/* query_objlist callback: return TRUE */
/*ARGSUSED*/
boolean allow_all(struct obj *obj)
{
    return TRUE;
}

boolean allow_category(struct obj *obj)
{
    if (Role_if (PM_PRIEST)) obj->bknown = TRUE;
    if (((index(valid_menu_classes,'u') != NULL) && obj->unpaid) ||
	(index(valid_menu_classes, obj->oclass) != NULL))
	return TRUE;
    else if (((index(valid_menu_classes,'U') != NULL) &&
	(obj->oclass != COIN_CLASS && obj->bknown && !obj->blessed && !obj->cursed)))
	return TRUE;
    else if (((index(valid_menu_classes,'B') != NULL) &&
	(obj->oclass != COIN_CLASS && obj->bknown && obj->blessed)))
	return TRUE;
    else if (((index(valid_menu_classes,'C') != NULL) &&
	(obj->oclass != COIN_CLASS && obj->bknown && obj->cursed)))
	return TRUE;
    else if (((index(valid_menu_classes,'X') != NULL) &&
	(obj->oclass != COIN_CLASS && !obj->bknown)))
	return TRUE;
    else
	return FALSE;
}


/* query_objlist callback: return TRUE if valid class and worn */
boolean is_worn_by_type(struct obj *otmp)
{
	return((boolean)(!!(otmp->owornmask &
			(W_ARMOR | W_RING | W_AMUL | W_TOOL | W_WEP | W_SWAPWEP | W_QUIVER)))
	        && (index(valid_menu_classes, otmp->oclass) != NULL));
}

/*
 * Have the hero pick things from the ground
 * or a monster's inventory if swallowed.
 *
 * Arg what:
 *	>0  autopickup
 *	=0  interactive
 *	<0  pickup count of something
 *
 * Returns 1 if tried to pick something up, whether
 * or not it succeeded.
 */
int pickup(int what)		/* should be a long */
{
	int i, n, res, count, n_tried = 0, n_picked = 0;
	struct object_pick *pick_list = NULL;
	boolean autopickup = what > 0;
	struct obj *objchain;
	int traverse_how;

	if (what < 0)		/* pick N of something */
	    count = -what;
	else			/* pick anything */
	    count = 0;

	if (!u.uswallow) {
		struct trap *ttmp = t_at(u.ux, u.uy);
		/* no auto-pick if no-pick move, nothing there, or in a pool */
		if (autopickup && (flags.nopick || !OBJ_AT(u.ux, u.uy) ||
			(is_pool(u.ux, u.uy) && !Underwater) || is_lava(u.ux, u.uy))) {
			read_engr_at(u.ux, u.uy);
			return 0;
		}

		/* no pickup if levitating & not on air or water level */
		if (!can_reach_floor()) {
		    if ((multi && !flags.run) || (autopickup && !flags.pickup))
			read_engr_at(u.ux, u.uy);
		    return 0;
		}
		if (ttmp && ttmp->tseen) {
		    /* Allow pickup from holes and trap doors that you escaped
		     * from because that stuff is teetering on the edge just
		     * like you, but not pits, because there is an elevation
		     * discrepancy with stuff in pits.
		     */
		    if ((ttmp->ttyp == PIT || ttmp->ttyp == SPIKED_PIT) &&
			(!u.utrap || (u.utrap && u.utraptype != TT_PIT))) {
			read_engr_at(u.ux, u.uy);
			return 0;
		    }
		}
		/* multi && !flags.run means they are in the middle of some other
		 * action, or possibly paralyzed, sleeping, etc.... and they just
		 * teleported onto the object.  They shouldn't pick it up.
		 */
		if ((multi && !flags.run) || (autopickup && !flags.pickup)) {
		    check_here(FALSE);
		    return 0;
		}
		if (notake(youmonst.data)) {
		    if (!autopickup)
			You("are physically incapable of picking anything up.");
		    else
			check_here(FALSE);
		    return 0;
		}

		/* if there's anything here, stop running */
		if (OBJ_AT(u.ux,u.uy) && flags.run && flags.run != 8 && !flags.nopick) nomul(0);
	}

	add_valid_menu_class(0);	/* reset */
	if (!u.uswallow) {
		objchain = level.objects[u.ux][u.uy];
		traverse_how = BY_NEXTHERE;
	} else {
		objchain = u.ustuck->minvent;
		traverse_how = 0;	/* nobj */
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


	if (count) {	/* looking for N of something */
	    char buf[QBUFSZ];
	    sprintf(buf, "Pick %d of what?", count);
	    val_for_n_or_more = count;	/* set up callback selector */
	    n = query_objlist(buf, objchain,
			traverse_how|AUTOSELECT_SINGLE|INVORDER_SORT,
			&pick_list, PICK_ONE, n_or_more);
	    /* correct counts, if any given */
	    for (i = 0; i < n; i++)
		pick_list[i].count = count;
	} else {
	    n = query_objlist("Pick up what?", objchain,
		    traverse_how|AUTOSELECT_SINGLE|INVORDER_SORT|FEEL_COCKATRICE,
		    &pick_list, PICK_ANY, all_but_uchain);
	}
menu_pickup:
	n_tried = n;
	for (n_picked = i = 0 ; i < n; i++) {
	    res = pickup_object(pick_list[i].obj, pick_list[i].count,
				    FALSE);
	    if (res < 0) break;	/* can't continue */
	    n_picked += res;
	}
	if (pick_list)
	    free(pick_list);


	if (!u.uswallow) {
		if (!OBJ_AT(u.ux,u.uy)) u.uundetected = 0;

		/* position may need updating (invisible hero) */
		if (n_picked) newsym(u.ux,u.uy);

		/* see whether there's anything else here, after auto-pickup is done */
		if (autopickup) check_here(n_picked > 0);
	}
	return n_tried > 0;
}

#ifdef AUTOPICKUP_EXCEPTIONS
boolean is_autopickup_exception(struct obj *obj,
				boolean grab)	 /* forced pickup, rather than forced leave behind? */
{
	/*
	 *  Does the text description of this match an exception?
	 */
	char *objdesc = makesingular(doname(obj));
	struct autopickup_exception *ape = (grab) ?
					iflags.autopickup_exceptions[AP_GRAB] :
					iflags.autopickup_exceptions[AP_LEAVE];
	while (ape) {
		if (pmatch(ape->pattern, objdesc)) return TRUE;
		ape = ape->next;
	}
	return FALSE;
}
#endif /* AUTOPICKUP_EXCEPTIONS */

/*
 * Pick from the given list using flags.pickup_types.  Return the number
 * of items picked (not counts).  Create an array that returns pointers
 * and counts of the items to be picked up.  If the number of items
 * picked is zero, the pickup list is left alone.  The caller of this
 * function must free the pickup list.
 */
static int autopick(struct obj *olist,	/* the object list */
		    int follow,		/* how to follow the object list */
		    struct object_pick **pick_list) /* list of objects and counts to pick up */
{
	struct object_pick *pi;	/* pick item */
	struct obj *curr;
	int n;
	const char *otypes = flags.pickup_types;

	/* first count the number of eligible items */
	for (n = 0, curr = olist; curr; curr = FOLLOW(curr, follow))


#ifndef AUTOPICKUP_EXCEPTIONS
	    if (!*otypes || index(otypes, curr->oclass))
#else
	    if ((!*otypes || index(otypes, curr->oclass) ||
		 is_autopickup_exception(curr, TRUE)) &&
	    	 !is_autopickup_exception(curr, FALSE))
#endif
		n++;

	if (n) {
	    *pick_list = pi = malloc(sizeof(struct object_pick) * n);
	    for (n = 0, curr = olist; curr; curr = FOLLOW(curr, follow))
#ifndef AUTOPICKUP_EXCEPTIONS
		if (!*otypes || index(otypes, curr->oclass)) {
#else
	    if ((!*otypes || index(otypes, curr->oclass) ||
		 is_autopickup_exception(curr, TRUE)) &&
	    	 !is_autopickup_exception(curr, FALSE)) {
#endif
		    pi[n].obj = curr;
		    pi[n].count = curr->quan;
		    n++;
		}
	}
	return n;
}


void add_objitem(struct nh_objitem **items, int *nr_items, int idx, int id,
		 char *caption, struct obj *obj, boolean use_invlet)
{
	if (idx >= *nr_items) {
	    *nr_items = *nr_items * 2;
	    *items = realloc(*items, *nr_items * sizeof(struct nh_objitem));
	}
	
	struct nh_objitem *it = &((*items)[idx]);
	it->id = id;
	strcpy(it->caption, caption);
	
	if (obj) {
	    it->count = obj->quan;
	    it->accel = use_invlet ? obj->invlet : 0;
	    it->group_accel = def_oc_syms[(int)objects[obj->otyp].oc_class];
	    it->otype = obj->otyp;
	    it->oclass = obj->oclass;
	
	    if (!obj->bknown)
		it->buc = BUC_UNKNOWN;
	    else if (obj->blessed)
		it->buc = BUC_BLESSED;
	    else if (obj->cursed)
		it->buc = BUC_CURSED;
	    else
		it->buc = BUC_UNCURSED;
	} else {
	    it->accel = it->group_accel = 0;
	    it->otype = it->oclass = -1;
	}
}

/*
 * Put up a menu using the given object list.  Only those objects on the
 * list that meet the approval of the allow function are displayed.  Return
 * a count of the number of items selected, as well as an allocated array of
 * menu_items, containing pointers to the objects selected and counts.  The
 * returned counts are guaranteed to be in bounds and non-zero.
 *
 * Query flags:
 *	BY_NEXTHERE	  - Follow object list via nexthere instead of nobj.
 *	AUTOSELECT_SINGLE - Don't ask if only 1 object qualifies - just
 *			    use it.
 *	USE_INVLET	  - Use object's invlet.
 *	INVORDER_SORT	  - Use hero's pack order.
 *	SIGNAL_NOMENU	  - Return -1 rather than 0 if nothing passes "allow".
 */
int query_objlist(const char *qstr,	/* query string */
		  struct obj *olist,	/* the list to pick from */
		  int qflags,		/* options to control the query */
		  struct object_pick **pick_list,/* return list of items picked */
		  int how,		/* type of query */
		  boolean (*allow)(struct obj*)) /* allow function */
{
	int n;
	struct obj *curr, *last;
	char *pack;
	boolean printed_type_name;
	int nr_items = 0, cur_entry = 0;
	struct nh_objitem *items = NULL;
	struct obj **id_to_obj = NULL;
	struct nh_objresult *selection = NULL;

	*pick_list = NULL;
	if (!olist)
	    return 0;

	/* count the number of items allowed */
	for (n = 0, last = 0, curr = olist; curr; curr = FOLLOW(curr, qflags))
	    if ((*allow)(curr)) {
		last = curr;
		n++;
	    }

	if (n == 0)	/* nothing to pick here */
	    return (qflags & SIGNAL_NOMENU) ? -1 : 0;

	if (n == 1 && (qflags & AUTOSELECT_SINGLE)) {
	    *pick_list = malloc(sizeof(struct object_pick));
	    (*pick_list)->obj = last;
	    (*pick_list)->count = last->quan;
	    return 1;
	}

	nr_items = 10;
	items = malloc(nr_items * sizeof(struct nh_objitem));
	id_to_obj = malloc(nr_items * sizeof(struct obj*));
	/*
	 * Run through the list and add the objects to the menu.  If
	 * INVORDER_SORT is set, we'll run through the list once for
	 * each type so we can group them.  The allow function will only
	 * be called once per object in the list.
	 */
	pack = flags.inv_order;
	do {
	    printed_type_name = FALSE;
	    for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
		if ((qflags & FEEL_COCKATRICE) && curr->otyp == CORPSE &&
		     will_feel_cockatrice(curr, FALSE)) {
			free(items);
			free(id_to_obj);
			look_here(0, FALSE);
			return 0;
		}
		if ((!(qflags & INVORDER_SORT) || curr->oclass == *pack)
							&& (*allow)(curr)) {

		    /* if sorting, print type name (once only) */
		    if (qflags & INVORDER_SORT && !printed_type_name) {
			add_objitem(&items, &nr_items, cur_entry, 0,
				    let_to_name(*pack, FALSE), NULL, FALSE);
			id_to_obj = realloc(id_to_obj, 
					    nr_items * sizeof(struct obj*));
			id_to_obj[cur_entry] = NULL;
			cur_entry++;
			printed_type_name = TRUE;
		    }
		    
		    add_objitem(&items, &nr_items, cur_entry, cur_entry+1,
				doname(curr), curr, (qflags & USE_INVLET) != 0);
		    id_to_obj = realloc(id_to_obj, nr_items * sizeof(struct obj*));
		    id_to_obj[cur_entry] = curr;
		    cur_entry++;
		}
	    }
	    pack++;
	} while (qflags & INVORDER_SORT && *pack);

	selection = malloc(cur_entry * sizeof(struct nh_objresult));
	n = display_objects(items, cur_entry, qstr, how, selection);
	
	if (n > 0) {
	    int i;
	    *pick_list = malloc(n * sizeof(struct object_pick));
	    
	    for (i = 0; i < n; i++) {
		curr = id_to_obj[selection[i].id - 1];
		(*pick_list)[i].obj = curr;
		if (selection[i].count == -1 || selection[i].count > curr->quan)
		    (*pick_list)[i].count = curr->quan;
		else
		    (*pick_list)[i].count = selection[i].count;
	    }
	} else if (n < 0) {
	    n = 0;	/* caller's don't expect -1 */
	}
	free(selection);
	free(id_to_obj);
	free(items);
	
	return n;
}

/*
 * allow menu-based category (class) selection (for Drop,take off etc.)
 *
 */
int query_category(const char *qstr,	/* query string */
		   struct obj *olist,	/* the list to pick from */
		   int qflags,		/* behaviour modification flags */
		   int *pick_list,	/* return list of items picked */
		   int how)		/* type of query */
{
	int n;
	struct obj *curr;
	char *pack;
	boolean collected_type_name;
	char invlet;
	int ccount;
	boolean do_unpaid = FALSE;
	boolean do_blessed = FALSE, do_cursed = FALSE, do_uncursed = FALSE,
	    do_buc_unknown = FALSE;
	int num_buc_types = 0;
	struct menulist menu;

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

	ccount = count_categories(olist, qflags);
	/* no point in actually showing a menu for a single category */
	if (ccount == 1 && !do_unpaid && num_buc_types <= 1 &&
	    !(qflags & BILLED_TYPES)) {
	    for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
		if ((qflags & WORN_TYPES) &&
		    !(curr->owornmask & (W_ARMOR|W_RING|W_AMUL|W_TOOL|W_WEP|W_SWAPWEP|W_QUIVER)))
		    continue;
		break;
	    }
	    if (curr) {
		pick_list[0] = curr->oclass;
		return 1;
	    } else {
#ifdef DEBUG
		impossible("query_category: no single object match");
#endif
	    }
	    return 0;
	}
	
	init_menulist(&menu);

	pack = flags.inv_order;
	invlet = 'a';
	if ((qflags & ALL_TYPES) && (ccount > 1)) {
		add_menuitem(&menu, ALL_TYPES_SELECTED, (qflags & WORN_TYPES) ?
		             "All worn types" : "All types", invlet, FALSE);
		invlet = 'b';
	}
	
	do {
	    collected_type_name = FALSE;
	    for (curr = olist; curr; curr = FOLLOW(curr, qflags)) {
		if (curr->oclass == *pack) {
		   if ((qflags & WORN_TYPES) &&
		   		!(curr->owornmask & (W_ARMOR | W_RING | W_AMUL | W_TOOL |
		    	W_WEP | W_SWAPWEP | W_QUIVER)))
			 continue;
		   if (!collected_type_name) {
			add_menuitem(&menu, curr->oclass,
				     let_to_name(*pack, FALSE), invlet++, FALSE);
			menu.items[menu.icount - 1].group_accel =
			    def_oc_syms[(int)objects[curr->otyp].oc_class];
			collected_type_name = TRUE;
		   }
		}
	    }
	    pack++;
	    if (invlet >= 'u') {
		free(menu.items);
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
	    add_menuitem(&menu, 'A', (qflags & WORN_TYPES) ?
		    "Auto-select every item being worn" :
		    "Auto-select every item", 'A', FALSE);
	
	/* items with b/u/c/unknown if there are any */
	if (do_blessed)
	    add_menuitem(&menu, 'B', "Items known to be Blessed", 'B', FALSE);
	
	if (do_cursed)
	    add_menuitem(&menu, 'C', "Items known to be Cursed", 'C', FALSE);
	
	if (do_uncursed)
	    add_menuitem(&menu, 'U', "Items known to be Uncursed", 'U', FALSE);
	
	if (do_buc_unknown)
	    add_menuitem(&menu, 'X', "Items of unknown B/C/U status", 'X', FALSE);
	
	n = display_menu(menu.items, menu.icount, qstr, how, pick_list);
	free(menu.items);
	if (n < 0)
	    n = 0;	/* callers don't expect -1 */
	
	return n;
}


static int count_categories(struct obj *olist, int qflags)
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
		   if ((qflags & WORN_TYPES) &&
		    	!(curr->owornmask & (W_ARMOR | W_RING | W_AMUL | W_TOOL |
		    	W_WEP | W_SWAPWEP | W_QUIVER)))
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
static long carry_count(struct obj *obj,	/* object to pick up */
			struct obj *container,	/* bag it's coming out of */
			long count,
			boolean telekinesis,
			int *wt_before, int *wt_after)
{
    boolean adjust_wt = container && carried(container),
	    is_gold = obj->oclass == COIN_CLASS;
    int wt, iw, ow, oow;
    long qq, savequan;
#ifdef GOLDOBJ
    long umoney = money_cnt(invent);
#endif
    unsigned saveowt;
    const char *verb, *prefx1, *prefx2, *suffx;
    char obj_nambuf[BUFSZ], where[BUFSZ];

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
#ifndef GOLDOBJ
    if (is_gold)	/* merged gold might affect cumulative weight */
	wt -= (GOLD_WT(u.ugold) + GOLD_WT(count) - GOLD_WT(u.ugold + count));
#else
    /* This will go with silver+copper & new gold weight */
    if (is_gold)	/* merged gold might affect cumulative weight */
	wt -= (GOLD_WT(umoney) + GOLD_WT(count) - GOLD_WT(umoney + count));
#endif
    if (count != savequan) {
	obj->quan = savequan;
	obj->owt = saveowt;
    }
    *wt_before = iw;
    *wt_after  = wt;

    if (wt < 0)
	return count;

    /* see how many we can lift */
    if (is_gold) {
#ifndef GOLDOBJ
	iw -= (int)GOLD_WT(u.ugold);
	if (!adjust_wt) {
	    qq = GOLD_CAPACITY((long)iw, u.ugold);
	} else {
	    oow = 0;
	    qq = 50L - (u.ugold % 100L) - 1L;
#else
	iw -= (int)GOLD_WT(umoney);
	if (!adjust_wt) {
	    qq = GOLD_CAPACITY((long)iw, umoney);
	} else {
	    oow = 0;
	    qq = 50L - (umoney % 100L) - 1L;
#endif
	    if (qq < 0L) qq += 100L;
	    for ( ; qq <= count; qq += 100L) {
		obj->quan = qq;
		obj->owt = (unsigned)GOLD_WT(qq);
#ifndef GOLDOBJ
		ow = (int)GOLD_WT(u.ugold + qq);
#else
		ow = (int)GOLD_WT(umoney + qq);
#endif
		ow -= (container->otyp == BAG_OF_HOLDING) ?
			(int)DELTA_CWT(container, obj) : (int)obj->owt;
		if (iw + ow >= 0) break;
		oow = ow;
	    }
	    iw -= oow;
	    qq -= 100L;
	}
	if (qq < 0L) qq = 0L;
	else if (qq > count) qq = count;
#ifndef GOLDOBJ
	wt = iw + (int)GOLD_WT(u.ugold + qq);
#else
	wt = iw + (int)GOLD_WT(umoney + qq);
#endif
    } else if (count > 1 || count < obj->quan) {
	/*
	 * Ugh. Calc num to lift by changing the quan of of the
	 * object and calling weight.
	 *
	 * This works for containers only because containers
	 * don't merge.		-dean
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
	strcpy(obj_nambuf, doname(obj));
	if (container) {
	    sprintf(where, "in %s", the(xname(container)));
	    verb = "carry";
	} else {
	    strcpy(where, "lying here");
	    verb = telekinesis ? "acquire" : "lift";
	}
    } else {
	/* lint supppression */
	*obj_nambuf = *where = '\0';
	verb = "";
    }
    /* we can carry qq of them */
    if (qq > 0) {
	if (qq < count)
	    You("can only %s %s of the %s %s.",
		verb, (qq == 1L) ? "one" : "some", obj_nambuf, where);
	*wt_after = wt;
	return qq;
    }

    if (!container) strcpy(where, "here");  /* slightly shorter form */
#ifndef GOLDOBJ
    if (invent || u.ugold) {
#else
    if (invent || umoney) {
#endif
	prefx1 = "you cannot ";
	prefx2 = "";
	suffx  = " any more";
    } else {
	prefx1 = (obj->quan == 1L) ? "it " : "even one ";
	prefx2 = "is too heavy for you to ";
	suffx  = "";
    }
    There("%s %s %s, but %s%s%s%s.",
	  otense(obj, "are"), obj_nambuf, where,
	  prefx1, prefx2, verb, suffx);

 /* *wt_after = iw; */
    return 0L;
}

/* determine whether character is able and player is willing to carry `obj' */
static int lift_object(struct obj *obj, struct obj *container,
		       long *cnt_p, boolean telekinesis)
{
    int result, old_wt, new_wt, prev_encumbr, next_encumbr;

    if (obj->otyp == BOULDER && In_sokoban(&u.uz)) {
	You("cannot get your %s around this %s.",
			body_part(HAND), xname(obj));
	return -1;
    }
    if (obj->otyp == LOADSTONE ||
	    (obj->otyp == BOULDER && throws_rocks(youmonst.data)))
	return 1;		/* lift regardless of current situation */

    *cnt_p = carry_count(obj, container, *cnt_p, telekinesis, &old_wt, &new_wt);
    if (*cnt_p < 1L) {
	result = -1;	/* nothing lifted */
#ifndef GOLDOBJ
    } else if (obj->oclass != COIN_CLASS && inv_cnt() >= 52 &&
		!merge_choice(invent, obj)) {
#else
    } else if (inv_cnt() >= 52 && !merge_choice(invent, obj)) {
#endif
	Your("knapsack cannot accommodate any more items.");
	result = -1;	/* nothing lifted */
    } else {
	result = 1;
	prev_encumbr = near_capacity();
	if (prev_encumbr < flags.pickup_burden)
		prev_encumbr = flags.pickup_burden;
	next_encumbr = calc_capacity(new_wt - old_wt);
	if (next_encumbr > prev_encumbr) {
	    if (telekinesis) {
		result = 0;	/* don't lift */
	    } else {
		char qbuf[BUFSZ];
		long savequan = obj->quan;

		obj->quan = *cnt_p;
		strcpy(qbuf,
			(next_encumbr > HVY_ENCUMBER) ? overloadmsg :
			(next_encumbr > MOD_ENCUMBER) ? nearloadmsg :
			moderateloadmsg);
		sprintf(eos(qbuf), " %s. Continue?",
			safe_qbuf(qbuf, sizeof(" . Continue?"),
				doname(obj), an(simple_typename(obj->otyp)), "something"));
		obj->quan = savequan;
		switch (ynq(qbuf)) {
		case 'q':  result = -1; break;
		case 'n':  result =  0; break;
		default:   break;	/* 'y' => result == 1 */
		}
		clear_nhwindow(NHW_MESSAGE);
	    }
	}
    }

    if (obj->otyp == SCR_SCARE_MONSTER && result <= 0 && !container)
	obj->spe = 0;
    return result;
}

/* To prevent qbuf overflow in prompts use planA only
 * if it fits, or planB if PlanA doesn't fit,
 * finally using the fallback as a last resort.
 * last_restort is expected to be very short.
 */
const char *safe_qbuf(const char *qbuf, unsigned padlength, const char *planA,
		      const char *planB, const char *last_resort)
{
	/* convert size_t (or int for ancient systems) to ordinary unsigned */
	unsigned len_qbuf = (unsigned)strlen(qbuf),
	         len_planA = (unsigned)strlen(planA),
	         len_planB = (unsigned)strlen(planB),
	         len_lastR = (unsigned)strlen(last_resort);
	unsigned textleft = QBUFSZ - (len_qbuf + padlength);

	if (len_lastR >= textleft) {
	    impossible("safe_qbuf: last_resort too large at %u characters.",
		       len_lastR);
	    return "";
	}
	return (len_planA < textleft) ? planA :
		    (len_planB < textleft) ? planB : last_resort;
}

/*
 * Pick up <count> of obj from the ground and add it to the hero's inventory.
 * Returns -1 if caller should break out of its loop, 0 if nothing picked
 * up, 1 if otherwise.
 */
int pickup_object(struct obj *obj, long count,
		  boolean telekinesis) /* not picking it up directly by hand */
{
	int res, nearload;
#ifndef GOLDOBJ
	const char *where = (obj->ox == u.ux && obj->oy == u.uy) ?
			    "here" : "there";
#endif

	if (obj->quan < count) {
	    impossible("pickup_object: count %ld > quan %ld?",
		count, obj->quan);
	    return 0;
	}

	/* In case of auto-pickup, where we haven't had a chance
	   to look at it yet; affects docall(SCR_SCARE_MONSTER). */
	if (!Blind)
#ifdef INVISIBLE_OBJECTS
		if (!obj->oinvis || See_invisible)
#endif
		obj->dknown = 1;

	if (obj == uchain) {    /* do not pick up attached chain */
	    return 0;
	} else if (obj->oartifact && !touch_artifact(obj,&youmonst)) {
	    return 0;
#ifndef GOLDOBJ
	} else if (obj->oclass == COIN_CLASS) {
	    /* Special consideration for gold pieces... */
	    long iw = (long)max_capacity() - GOLD_WT(u.ugold);
	    long gold_capacity = GOLD_CAPACITY(iw, u.ugold);

	    if (gold_capacity <= 0L) {
		pline(
	       "There %s %ld gold piece%s %s, but you cannot carry any more.",
		      otense(obj, "are"),
		      obj->quan, plur(obj->quan), where);
		return 0;
	    } else if (gold_capacity < count) {
		You("can only %s %s of the %ld gold pieces lying %s.",
		    telekinesis ? "acquire" : "carry",
		    gold_capacity == 1L ? "one" : "some", obj->quan, where);
		pline("%s %ld gold piece%s.",
		    nearloadmsg, gold_capacity, plur(gold_capacity));
		u.ugold += gold_capacity;
		obj->quan -= gold_capacity;
		costly_gold(obj->ox, obj->oy, gold_capacity);
	    } else {
		u.ugold += count;
		if ((nearload = near_capacity()) != 0)
		    pline("%s %ld gold piece%s.",
			  nearload < MOD_ENCUMBER ?
			  moderateloadmsg : nearloadmsg,
			  count, plur(count));
		else
		    prinv(NULL, obj, count);
		costly_gold(obj->ox, obj->oy, count);
		if (count == obj->quan)
		    delobj(obj);
		else
		    obj->quan -= count;
	    }
	    botl = 1;
	    if (flags.run) nomul(0);
	    return 1;
#endif
	} else if (obj->otyp == CORPSE) {
	    if ( (touch_petrifies(&mons[obj->corpsenm])) && !uarmg
				&& !Stone_resistance && !telekinesis) {
		if (poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))
		    display_nhwindow(NHW_MESSAGE, FALSE);
		else {
			char kbuf[BUFSZ];

			strcpy(kbuf, an(corpse_xname(obj, TRUE)));
			pline("Touching %s is a fatal mistake.", kbuf);
			instapetrify(kbuf);
		    return -1;
		}
	    } else if (is_rider(&mons[obj->corpsenm])) {
		pline("At your %s, the corpse suddenly moves...",
			telekinesis ? "attempted acquisition" : "touch");
		revive_corpse(obj);
		exercise(A_WIS, FALSE);
		return -1;
	    }
	} else  if (obj->otyp == SCR_SCARE_MONSTER) {
	    if (obj->blessed) obj->blessed = 0;
	    else if (!obj->spe && !obj->cursed) obj->spe = 1;
	    else {
		pline_The("scroll%s %s to dust as you %s %s up.",
			plur(obj->quan), otense(obj, "turn"),
			telekinesis ? "raise" : "pick",
			(obj->quan == 1L) ? "it" : "them");
		if (!(objects[SCR_SCARE_MONSTER].oc_name_known) &&
				    !(objects[SCR_SCARE_MONSTER].oc_uname))
		    docall(obj);
		useupf(obj, obj->quan);
		return 1;	/* tried to pick something up and failed, but
				   don't want to terminate pickup loop yet   */
	    }
	}

	if ((res = lift_object(obj, NULL, &count, telekinesis)) <= 0)
	    return res;

#ifdef GOLDOBJ
        /* Whats left of the special case for gold :-) */
	if (obj->oclass == COIN_CLASS) botl = 1;
#endif
	if (obj->quan != count && obj->otyp != LOADSTONE)
	    obj = splitobj(obj, count);

	obj = pick_obj(obj);

	if (uwep && uwep == obj) mrg_to_wielded = TRUE;
	nearload = near_capacity();
	prinv(nearload == SLT_ENCUMBER ? moderateloadmsg : NULL,
	      obj, count);
	mrg_to_wielded = FALSE;
	return 1;
}

/*
 * Do the actual work of picking otmp from the floor or monster's interior
 * and putting it in the hero's inventory.  Take care of billing.  Return a
 * pointer to the object where otmp ends up.  This may be different
 * from otmp because of merging.
 *
 * Gold never reaches this routine unless GOLDOBJ is defined.
 */
struct obj *pick_obj(struct obj *otmp)
{
	obj_extract_self(otmp);
	if (!u.uswallow && otmp != uball && costly_spot(otmp->ox, otmp->oy)) {
	    char saveushops[5], fakeshop[2];

	    /* addtobill cares about your location rather than the object's;
	       usually they'll be the same, but not when using telekinesis
	       (if ever implemented) or a grappling hook */
	    strcpy(saveushops, u.ushops);
	    fakeshop[0] = *in_rooms(otmp->ox, otmp->oy, SHOPBASE);
	    fakeshop[1] = '\0';
	    strcpy(u.ushops, fakeshop);
	    /* sets obj->unpaid if necessary */
	    addtobill(otmp, TRUE, FALSE, FALSE);
	    strcpy(u.ushops, saveushops);
	    /* if you're outside the shop, make shk notice */
	    if (!index(u.ushops, *fakeshop))
		remote_burglary(otmp->ox, otmp->oy);
	}
	if (otmp->no_charge)	/* only applies to objects outside invent */
	    otmp->no_charge = 0;
	newsym(otmp->ox, otmp->oy);
	return addinv(otmp);	/* might merge it with other objects */
}

/*
 * prints a message if encumbrance changed since the last check and
 * returns the new encumbrance value (from near_capacity()).
 */
int encumber_msg(void)
{
    static int oldcap = UNENCUMBERED;
    int newcap = near_capacity();

    if (oldcap < newcap) {
	switch(newcap) {
	case 1: Your("movements are slowed slightly because of your load.");
		break;
	case 2: You("rebalance your load.  Movement is difficult.");
		break;
	case 3: You("%s under your heavy load.  Movement is very hard.",
		    stagger(youmonst.data, "stagger"));
		break;
	default: You("%s move a handspan with this load!",
		     newcap == 4 ? "can barely" : "can't even");
		break;
	}
	botl = 1;
    } else if (oldcap > newcap) {
	switch(newcap) {
	case 0: Your("movements are now unencumbered.");
		break;
	case 1: Your("movements are only slowed slightly by your load.");
		break;
	case 2: You("rebalance your load.  Movement is still difficult.");
		break;
	case 3: You("%s under your load.  Movement is still very hard.",
		    stagger(youmonst.data, "stagger"));
		break;
	}
	botl = 1;
    }

    oldcap = newcap;
    return newcap;
}

/* Is there a container at x,y. Optional: return count of containers at x,y */
static int container_at(int x, int y, boolean countem)
{
	struct obj *cobj, *nobj;
	int container_count = 0;
	
	for (cobj = level.objects[x][y]; cobj; cobj = nobj) {
		nobj = cobj->nexthere;
		if (Is_container(cobj)) {
			container_count++;
			if (!countem) break;
		}
	}
	return container_count;
}

static boolean able_to_loot(int x, int y)
{
	if (!can_reach_floor()) {
		if (u.usteed && P_SKILL(P_RIDING) < P_BASIC)
			rider_cant_reach(); /* not skilled enough to reach */
		else
			You("cannot reach the %s.", surface(x, y));
		return FALSE;
	} else if (is_pool(x, y) || is_lava(x, y)) {
		/* at present, can't loot in water even when Underwater */
		You("cannot loot things that are deep in the %s.",
		    is_lava(x, y) ? "lava" : "water");
		return FALSE;
	} else if (nolimbs(youmonst.data)) {
		pline("Without limbs, you cannot loot anything.");
		return FALSE;
	} else if (!freehand()) {
		pline("Without a free %s, you cannot loot anything.",
			body_part(HAND));
		return FALSE;
	}
	return TRUE;
}

static boolean mon_beside(int x, int y)
{
	int i,j,nx,ny;
	for (i = -1; i <= 1; i++)
	    for (j = -1; j <= 1; j++) {
	    	nx = x + i;
	    	ny = y + j;
		if (isok(nx, ny) && MON_AT(nx, ny))
			return TRUE;
	    }
	return FALSE;
}

/* loot a container on the floor or loot saddle from mon. */
int doloot(void)
{
    struct obj *cobj, *nobj;
    int c = -1;
    int timepassed = 0;
    coord cc;
    boolean underfoot = TRUE;
    const char *dont_find_anything = "don't find anything";
    struct monst *mtmp;
    char qbuf[BUFSZ];
    int prev_inquiry = 0;
    boolean prev_loot = FALSE;

    if (check_capacity(NULL)) {
	/* "Can't do that while carrying so much stuff." */
	return 0;
    }
    if (nohands(youmonst.data)) {
	You("have no hands!");	/* not `body_part(HAND)' */
	return 0;
    }
    cc.x = u.ux; cc.y = u.uy;

lootcont:

    if (container_at(cc.x, cc.y, FALSE)) {
	boolean any = FALSE;

	if (!able_to_loot(cc.x, cc.y)) return 0;
	for (cobj = level.objects[cc.x][cc.y]; cobj; cobj = nobj) {
	    nobj = cobj->nexthere;

	    if (Is_container(cobj)) {
		sprintf(qbuf, "There is %s here, loot it?",
			safe_qbuf("", sizeof("There is  here, loot it?"),
			     doname(cobj), an(simple_typename(cobj->otyp)),
			     "a container"));
		c = ynq(qbuf);
		if (c == 'q') return timepassed;
		if (c == 'n') continue;
		any = TRUE;

		if (cobj->olocked) {
		    pline("Hmmm, it seems to be locked.");
		    continue;
		}
		if (cobj->otyp == BAG_OF_TRICKS) {
		    int tmp;
		    You("carefully open the bag...");
		    pline("It develops a huge set of teeth and bites you!");
		    tmp = rnd(10);
		    if (Half_physical_damage) tmp = (tmp+1) / 2;
		    losehp(tmp, "carnivorous bag", KILLED_BY_AN);
		    makeknown(BAG_OF_TRICKS);
		    timepassed = 1;
		    continue;
		}

		You("carefully open %s...", the(xname(cobj)));
		timepassed |= use_container(cobj, 0);
		if (multi < 0) return 1;		/* chest trap */
	    }
	}
	if (any) c = 'y';
    } else if (Confusion) {
#ifndef GOLDOBJ
	if (u.ugold){
	    long contribution = rnd((int)min(LARGEST_INT,u.ugold));
	    struct obj *goldob = mkgoldobj(contribution);
#else
	struct obj *goldob;
	/* Find a money object to mess with */
	for (goldob = invent; goldob; goldob = goldob->nobj) {
	    if (goldob->oclass == COIN_CLASS) break;
	}
	if (goldob){
	    long contribution = rnd((int)min(LARGEST_INT, goldob->quan));
	    if (contribution < goldob->quan)
		goldob = splitobj(goldob, contribution);
	    freeinv(goldob);
#endif
	    if (IS_THRONE(level.locations[u.ux][u.uy].typ)){
		struct obj *coffers;
		int pass;
		/* find the original coffers chest, or any chest */
		for (pass = 2; pass > -1; pass -= 2)
		    for (coffers = level.objlist; coffers; coffers = coffers->nobj)
			if (coffers->otyp == CHEST && coffers->spe == pass)
			    goto gotit;	/* two level break */
gotit:
		if (coffers) {
	    verbalize("Thank you for your contribution to reduce the debt.");
		    add_to_container(coffers, goldob);
		    coffers->owt = weight(coffers);
		} else {
		    struct monst *mon = makemon(courtmon(),
					    u.ux, u.uy, NO_MM_FLAGS);
		    if (mon) {
#ifndef GOLDOBJ
			mon->mgold += goldob->quan;
			delobj(goldob);
			pline("The exchequer accepts your contribution.");
		    } else {
			dropx(goldob);
		    }
		}
	    } else {
		dropx(goldob);
#else
			add_to_minv(mon, goldob);
			pline("The exchequer accepts your contribution.");
		    } else {
			dropy(goldob);
		    }
		}
	    } else {
		dropy(goldob);
#endif
		pline("Ok, now there is loot here.");
	    }
	}
    } else if (IS_GRAVE(level.locations[cc.x][cc.y].typ)) {
	You("need to dig up the grave to effectively loot it...");
    }
    /*
     * 3.3.1 introduced directional looting for some things.
     */
    if (c != 'y' && mon_beside(u.ux, u.uy)) {
	schar dz;
	if (!get_adjacent_loc("Loot in what direction?", "Invalid loot location",
			u.ux, u.uy, &cc, &dz))
	    return 0;
	if (cc.x == u.ux && cc.y == u.uy) {
	    underfoot = TRUE;
	    if (container_at(cc.x, cc.y, FALSE))
		goto lootcont;
	} else
	    underfoot = FALSE;
	if (dz < 0) {
	    You("%s to loot on the %s.", dont_find_anything,
		ceiling(cc.x, cc.y));
	    timepassed = 1;
	    return timepassed;
	}
	mtmp = m_at(cc.x, cc.y);
	if (mtmp) timepassed = loot_mon(mtmp, &prev_inquiry, &prev_loot);

	/* Preserve pre-3.3.1 behaviour for containers.
	 * Adjust this if-block to allow container looting
	 * from one square away to change that in the future.
	 */
	if (!underfoot) {
	    if (container_at(cc.x, cc.y, FALSE)) {
		if (mtmp) {
		    You_cant("loot anything %sthere with %s in the way.",
			    prev_inquiry ? "else " : "", mon_nam(mtmp));
		    return timepassed;
		} else {
		    You("have to be at a container to loot it.");
		}
	    } else {
		You("%s %sthere to loot.", dont_find_anything,
			(prev_inquiry || prev_loot) ? "else " : "");
		return timepassed;
	    }
	}
    } else if (c != 'y' && c != 'n') {
	You("%s %s to loot.", dont_find_anything,
		    underfoot ? "here" : "there");
    }
    return timepassed;
}

/* loot_mon() returns amount of time passed.
 */
int loot_mon(struct monst *mtmp, int *passed_info, boolean *prev_loot)
{
    int c = -1;
    int timepassed = 0;
    struct obj *otmp;
    char qbuf[QBUFSZ];

    /* 3.3.1 introduced the ability to remove saddle from a steed             */
    /* 	*passed_info is set to TRUE if a loot query was given.               */
    /*	*prev_loot is set to TRUE if something was actually acquired in here. */
    if (mtmp && mtmp != u.usteed && (otmp = which_armor(mtmp, W_SADDLE))) {
	long unwornmask;
	if (passed_info) *passed_info = 1;
	sprintf(qbuf, "Do you want to remove the saddle from %s?",
		x_monnam(mtmp, ARTICLE_THE, NULL, SUPPRESS_SADDLE, FALSE));
	if ((c = yn_function(qbuf, ynqchars, 'n')) == 'y') {
		if (nolimbs(youmonst.data)) {
		    You_cant("do that without limbs."); /* not body_part(HAND) */
		    return 0;
		}
		if (otmp->cursed) {
		    You("can't. The saddle seems to be stuck to %s.",
			x_monnam(mtmp, ARTICLE_THE, NULL,
				SUPPRESS_SADDLE, FALSE));
			    
		    /* the attempt costs you time */
			return 1;
		}
		obj_extract_self(otmp);
		if ((unwornmask = otmp->owornmask) != 0L) {
		    mtmp->misc_worn_check &= ~unwornmask;
		    otmp->owornmask = 0L;
		    update_mon_intrinsics(mtmp, otmp, FALSE, FALSE);
		}
		otmp = hold_another_object(otmp, "You drop %s!", doname(otmp),
					NULL);
		timepassed = rnd(3);
		if (prev_loot) *prev_loot = TRUE;
	} else if (c == 'q') {
		return 0;
	}
    }
    /* 3.4.0 introduced the ability to pick things up from within swallower's stomach */
    if (u.uswallow) {
	int count = passed_info ? *passed_info : 0;
	timepassed = pickup(count);
    }
    return timepassed;
}

/*
 * Decide whether an object being placed into a magic bag will cause
 * it to explode.  If the object is a bag itself, check recursively.
 */
static boolean mbag_explodes(struct obj *obj, int depthin)
{
    /* these won't cause an explosion when they're empty */
    if ((obj->otyp == WAN_CANCELLATION || obj->otyp == BAG_OF_TRICKS) &&
	    obj->spe <= 0)
	return FALSE;

    /* odds: 1/1, 2/2, 3/4, 4/8, 5/16, 6/32, 7/64, 8/128, 9/128, 10/128,... */
    if ((Is_mbag(obj) || obj->otyp == WAN_CANCELLATION) &&
	(rn2(1 << (depthin > 7 ? 7 : depthin)) <= depthin))
	return TRUE;
    else if (Has_contents(obj)) {
	struct obj *otmp;

	for (otmp = obj->cobj; otmp; otmp = otmp->nobj)
	    if (mbag_explodes(otmp, depthin+1)) return TRUE;
    }
    return FALSE;
}

/* A variable set in use_container(), to be used by the callback routines   */
/* in_container(), and out_container() from askchain() and use_container(). */
static struct obj *current_container;
#define Icebox (current_container->otyp == ICE_BOX)

/* Returns: -1 to stop, 1 item was inserted, 0 item was not inserted. */
static int in_container(struct obj *obj)
{
	boolean floor_container = !carried(current_container);
	boolean was_unpaid = FALSE;
	char buf[BUFSZ];

	if (!current_container) {
		impossible("<in> no current_container?");
		return 0;
	} else if (obj == uball || obj == uchain) {
		You("must be kidding.");
		return 0;
	} else if (obj == current_container) {
		pline("That would be an interesting topological exercise.");
		return 0;
	} else if (obj->owornmask & (W_ARMOR | W_RING | W_AMUL | W_TOOL)) {
		Norep("You cannot %s %s you are wearing.",
			Icebox ? "refrigerate" : "stash", something);
		return 0;
	} else if ((obj->otyp == LOADSTONE) && obj->cursed) {
		obj->bknown = 1;
	      pline_The("stone%s won't leave your person.", plur(obj->quan));
		return 0;
	} else if (obj->otyp == AMULET_OF_YENDOR ||
		   obj->otyp == CANDELABRUM_OF_INVOCATION ||
		   obj->otyp == BELL_OF_OPENING ||
		   obj->otyp == SPE_BOOK_OF_THE_DEAD) {
	/* Prohibit Amulets in containers; if you allow it, monsters can't
	 * steal them.  It also becomes a pain to check to see if someone
	 * has the Amulet.  Ditto for the Candelabrum, the Bell and the Book.
	 */
	    pline("%s cannot be confined in such trappings.", The(xname(obj)));
	    return 0;
	} else if (obj->otyp == LEASH && obj->leashmon != 0) {
		pline("%s attached to your pet.", Tobjnam(obj, "are"));
		return 0;
	} else if (obj == uwep) {
		if (welded(obj)) {
			weldmsg(obj);
			return 0;
		}
		setuwep(NULL);
		if (uwep) return 0;	/* unwielded, died, rewielded */
	} else if (obj == uswapwep) {
		setuswapwep(NULL);
		if (uswapwep) return 0;     /* unwielded, died, rewielded */
	} else if (obj == uquiver) {
		setuqwep(NULL);
		if (uquiver) return 0;     /* unwielded, died, rewielded */
	}

	if (obj->otyp == CORPSE) {
	    if ( (touch_petrifies(&mons[obj->corpsenm])) && !uarmg
		 && !Stone_resistance) {
		if (poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))
		    display_nhwindow(NHW_MESSAGE, FALSE);
		else {
		    char kbuf[BUFSZ];

		    strcpy(kbuf, an(corpse_xname(obj, TRUE)));
		    pline("Touching %s is a fatal mistake.", kbuf);
		    instapetrify(kbuf);
		    return -1;
		}
	    }
	}

	/* boxes, boulders, and big statues can't fit into any container */
	if (obj->otyp == ICE_BOX || Is_box(obj) || obj->otyp == BOULDER ||
		(obj->otyp == STATUE && bigmonst(&mons[obj->corpsenm]))) {
		/*
		 *  xname() uses a static result array.  Save obj's name
		 *  before current_container's name is computed.  Don't
		 *  use the result of strcpy() within You() --- the order
		 *  of evaluation of the parameters is undefined.
		 */
		strcpy(buf, the(xname(obj)));
		You("cannot fit %s into %s.", buf,
		    the(xname(current_container)));
		return 0;
	}

	freeinv(obj);

	if (obj_is_burning(obj))	/* this used to be part of freeinv() */
		snuff_lit(obj);

	if (floor_container && costly_spot(u.ux, u.uy)) {
	    if (current_container->no_charge && !obj->unpaid) {
		/* don't sell when putting the item into your own container */
		obj->no_charge = 1;
	    } else if (obj->oclass != COIN_CLASS) {
		/* sellobj() will take an unpaid item off the shop bill
		 * note: coins are handled later */
		was_unpaid = obj->unpaid ? TRUE : FALSE;
		sellobj_state(SELL_DELIBERATE);
		sellobj(obj, u.ux, u.uy);
		sellobj_state(SELL_NORMAL);
	    }
	}
	if (Icebox && !age_is_relative(obj)) {
		obj->age = monstermoves - obj->age; /* actual age */
		/* stop any corpse timeouts when frozen */
		if (obj->otyp == CORPSE && obj->timed) {
			long rot_alarm = stop_timer(ROT_CORPSE, obj);
			stop_timer(REVIVE_MON, obj);
			/* mark a non-reviving corpse as such */
			if (rot_alarm) obj->norevive = 1;
		}
	} else if (Is_mbag(current_container) && mbag_explodes(obj, 0)) {
		/* explicitly mention what item is triggering the explosion */
		pline(
	      "As you put %s inside, you are blasted by a magical explosion!",
		      doname(obj));
		/* did not actually insert obj yet */
		if (was_unpaid) addtobill(obj, FALSE, FALSE, TRUE);
		obfree(obj, NULL);
		delete_contents(current_container);
		if (!floor_container)
			useup(current_container);
		else if (obj_here(current_container, u.ux, u.uy))
			useupf(current_container, obj->quan);
		else
			panic("in_container:  bag not found.");

		losehp(d(6,6),"magical explosion", KILLED_BY_AN);
		current_container = 0;	/* baggone = TRUE; */
	}

	if (current_container) {
	    strcpy(buf, the(xname(current_container)));
	    You("put %s into %s.", doname(obj), buf);

	    /* gold in container always needs to be added to credit */
	    if (floor_container && obj->oclass == COIN_CLASS)
		sellobj(obj, current_container->ox, current_container->oy);
	    add_to_container(current_container, obj);
	    current_container->owt = weight(current_container);
	}
	/* gold needs this, and freeinv() many lines above may cause
	 * the encumbrance to disappear from the status, so just always
	 * update status immediately.
	 */
	bot();

	return current_container ? 1 : -1;
}


/* Returns: -1 to stop, 1 item was removed, 0 item was not removed. */
static int out_container(struct obj *obj)
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

	if (obj->oartifact && !touch_artifact(obj,&youmonst)) return 0;

	if (obj->otyp == CORPSE) {
	    if ( (touch_petrifies(&mons[obj->corpsenm])) && !uarmg
		 && !Stone_resistance) {
		if (poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))
		    display_nhwindow(NHW_MESSAGE, FALSE);
		else {
		    char kbuf[BUFSZ];

		    strcpy(kbuf, an(corpse_xname(obj, TRUE)));
		    pline("Touching %s is a fatal mistake.", kbuf);
		    instapetrify(kbuf);
		    return -1;
		}
	    }
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
		obj->age = monstermoves - obj->age; /* actual age */
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
	if (is_pick(obj) && !obj->unpaid && *u.ushops && shop_keeper(*u.ushops))
		verbalize("You sneaky cad! Get out of here with that pick!");

	otmp = addinv(obj);
	loadlev = near_capacity();
	prinv(loadlev ?
	      (loadlev < MOD_ENCUMBER ?
	       "You have a little trouble removing" :
	       "You have much trouble removing") : NULL,
	      otmp, count);

	if (is_gold) {
#ifndef GOLDOBJ
		dealloc_obj(obj);
#endif
		bot();	/* update character's gold piece count immediately */
	}
	return 1;
}

/* an object inside a cursed bag of holding is being destroyed */
static long mbag_item_gone(int held, struct obj *item)
{
    struct monst *shkp;
    long loss = 0L;

    if (item->dknown)
	pline("%s %s vanished!", Doname2(item), otense(item, "have"));
    else
	You("%s %s disappear!", Blind ? "notice" : "see", doname(item));

    if (*u.ushops && (shkp = shop_keeper(*u.ushops)) != 0) {
	if (held ? (boolean) item->unpaid : costly_spot(u.ux, u.uy))
	    loss = stolen_value(item, u.ux, u.uy,
				(boolean)shkp->mpeaceful, TRUE);
    }
    obfree(item, NULL);
    return loss;
}

static void observe_quantum_cat(struct obj *box)
{
    static const char sc[] = "Schroedinger's Cat";
    struct obj *deadcat;
    struct monst *livecat;
    xchar ox, oy;

    box->spe = 0;		/* box->owt will be updated below */
    if (get_obj_location(box, &ox, &oy, 0))
	box->ox = ox, box->oy = oy;	/* in case it's being carried */

    /* this isn't really right, since any form of observation
       (telepathic or monster/object/food detection) ought to
       force the determination of alive vs dead state; but basing
       it just on opening the box is much simpler to cope with */
    livecat = rn2(2) ? makemon(&mons[PM_HOUSECAT],
			       box->ox, box->oy, NO_MINVENT) : 0;
    if (livecat) {
	livecat->mpeaceful = 1;
	set_malign(livecat);
	if (!canspotmon(livecat))
	    You("think %s brushed your %s.", something, body_part(FOOT));
	else
	    pline("%s inside the box is still alive!", Monnam(livecat));
	christen_monst(livecat, sc);
    } else {
	deadcat = mk_named_object(CORPSE, &mons[PM_HOUSECAT],
				  box->ox, box->oy, sc);
	if (deadcat) {
	    obj_extract_self(deadcat);
	    add_to_container(box, deadcat);
	}
	pline_The("%s inside the box is dead!",
	    Hallucination ? rndmonnam() : "housecat");
    }
    box->owt = weight(box);
    return;
}

#undef Icebox

int use_container(struct obj *obj, int held)
{
	struct obj *curr, *otmp;
#ifndef GOLDOBJ
	struct obj *u_gold = NULL;
#endif
	boolean quantum_cat = FALSE,
		loot_out = FALSE, loot_in = FALSE;
	char qbuf[BUFSZ], emptymsg[BUFSZ], pbuf[QBUFSZ];
	long loss = 0L;
	int cnt = 0, used = 0,
	    menu_on_request;

	emptymsg[0] = '\0';
	if (nohands(youmonst.data)) {
		You("have no hands!");	/* not `body_part(HAND)' */
		return 0;
	} else if (!freehand()) {
		You("have no free %s.", body_part(HAND));
		return 0;
	}
	if (obj->olocked) {
	    pline("%s to be locked.", Tobjnam(obj, "seem"));
	    if (held) You("must put it down to unlock.");
	    return 0;
	} else if (obj->otrapped) {
	    if (held) You("open %s...", the(xname(obj)));
	    chest_trap(obj, HAND, FALSE);
	    /* even if the trap fails, you've used up this turn */
	    if (multi >= 0) {	/* in case we didn't become paralyzed */
		nomul(-1);
		nomovemsg = "";
	    }
	    return 1;
	}
	current_container = obj;	/* for use by in/out_container */

	if (obj->spe == 1) {
	    observe_quantum_cat(obj);
	    used = 1;
	    quantum_cat = TRUE;	/* for adjusting "it's empty" message */
	}
	/* Count the number of contained objects. Sometimes toss objects if */
	/* a cursed magic bag.						    */
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

	if (loss)	/* magic bag lost some shop goods */
	    You("owe %ld %s for lost merchandise.", loss, currency(loss));
	obj->owt = weight(obj);	/* in case any items were lost */

	if (!cnt)
	    sprintf(emptymsg, "%s is %sempty.", Yname2(obj),
		    quantum_cat ? "now " : "");

	if (cnt || flags.menu_style == MENU_FULL) {
	    strcpy(qbuf, "Do you want to take something out of ");
	    sprintf(eos(qbuf), "%s?",
		    safe_qbuf(qbuf, 1, yname(obj), ysimple_name(obj), "it"));

	    if (flags.menu_style == MENU_FULL) {
		int t;
		char menuprompt[BUFSZ];
		boolean outokay = (cnt != 0);
#ifndef GOLDOBJ
		boolean inokay = (invent != 0) || (u.ugold != 0);
#else
		boolean inokay = (invent != 0);
#endif
		if (!outokay && !inokay) {
		    pline("%s", emptymsg);
		    You("don't have anything to put in.");
		    return used;
		}
		menuprompt[0] = '\0';
		if (!cnt) sprintf(menuprompt, "%s ", emptymsg);
		strcat(menuprompt, "Do what?");
		t = in_or_out_menu(menuprompt, current_container, outokay, inokay);
		if (t <= 0) return 0;
		loot_out = (t & 0x01) != 0;
		loot_in  = (t & 0x02) != 0;
	    } else {	/* MENU_COMBINATION or MENU_PARTIAL */
		loot_out = (yn_function(qbuf, "ynq", 'n') == 'y');
	    }
	    
	    if (loot_out) {
		add_valid_menu_class(0);	/* reset */
		used |= menu_loot(0, current_container, FALSE) > 0;
	    }
	    
	} else {
	    pline("%s", emptymsg);		/* <whatever> is empty. */
	}

#ifndef GOLDOBJ
	if (!invent && u.ugold == 0) {
#else
	if (!invent) {
#endif
	    /* nothing to put in, but some feedback is necessary */
	    You("don't have anything to put in.");
	    return used;
	}
	if (flags.menu_style != MENU_FULL) {
	    sprintf(qbuf, "Do you wish to put %s in?", something);
	    strcpy(pbuf, ynqchars);

	    switch (yn_function(qbuf, pbuf, 'n')) {
		case 'y':
		    loot_in = TRUE;
		    break;
		case 'n':
		    break;
		case 'm':
		    add_valid_menu_class(0);	  /* reset */
		    menu_on_request = -2; /* triggers ALL_CLASSES */
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
#ifndef GOLDOBJ
	    if (u.ugold) {
		/*
		 * Hack: gold is not in the inventory, so make a gold object
		 * and put it at the head of the inventory list.
		 */
		u_gold = mkgoldobj(u.ugold);	/* removes from u.ugold */
		u_gold->in_use = TRUE;
		u.ugold = u_gold->quan;		/* put the gold back */
		assigninvlet(u_gold);		/* might end up as NOINVSYM */
		u_gold->nobj = invent;
		invent = u_gold;
	    }
#endif
	    add_valid_menu_class(0);	  /* reset */
	    used |= menu_loot(0, current_container, TRUE) > 0;
	}

#ifndef GOLDOBJ
	if (u_gold && invent && invent->oclass == COIN_CLASS) {
	    /* didn't stash [all of] it */
	    u_gold = invent;
	    invent = u_gold->nobj;
	    u_gold->in_use = FALSE;
	    dealloc_obj(u_gold);
	}
#endif
	return used;
}

/* Loot a container (take things out, put things in), using a menu. */
static int menu_loot(int retry, struct obj *container, boolean put_in)
{
    int n, i, n_looted = 0;
    boolean all_categories = TRUE, loot_everything = FALSE;
    char buf[BUFSZ];
    const char *takeout = "Take out", *putin = "Put in";
    struct obj *otmp, *otmp2;
    int pick_list[30];
    struct object_pick *obj_pick_list;
    int mflags, res;
    long count;

    if (retry) {
	all_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
	all_categories = FALSE;
	sprintf(buf,"%s what type of objects?", put_in ? putin : takeout);
	mflags = put_in ? ALL_TYPES | BUC_ALLBKNOWN | BUC_UNKNOWN :
		          ALL_TYPES | CHOOSE_ALL | BUC_ALLBKNOWN | BUC_UNKNOWN;
	n = query_category(buf, put_in ? invent : container->cobj,
			   mflags, pick_list, PICK_ANY);
	if (!n) return 0;
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
	    if (res < 0) break;
	}
    } else {
	mflags = INVORDER_SORT;
	if (put_in && flags.invlet_constant) mflags |= USE_INVLET;
	sprintf(buf,"%s what?", put_in ? putin : takeout);
	n = query_objlist(buf, put_in ? invent : container->cobj,
			  mflags, &obj_pick_list, PICK_ANY,
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

static int in_or_out_menu(const char *prompt, struct obj *obj,
			  boolean outokay, boolean inokay)
{
    struct nh_menuitem items[3];
    int selection[1];
    char buf[BUFSZ];
    int n, nr = 0;
    const char *menuselector = iflags.lootabc ? "abc" : "oib";

    if (outokay) {
	sprintf(buf,"Take %s out of %s", something, the(xname(obj)));
	set_menuitem(&items[nr], 1, MI_NORMAL, buf, menuselector[nr], FALSE);
	nr++;
    }
    
    if (inokay) {
	sprintf(buf,"Put %s into %s", something, the(xname(obj)));
	set_menuitem(&items[nr], 2, MI_NORMAL, buf, menuselector[nr], FALSE);
	nr++;
    }
    
    if (outokay && inokay) {
	set_menuitem(&items[nr], 3, MI_NORMAL, "Both of the above",
		     menuselector[nr], FALSE);
	nr++;
    }
    
    n = display_menu(items, nr, prompt, PICK_ONE, selection);
    if (n > 0)
	n = selection[0];
    
    return n;
}

/*pickup.c*/
