/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-06-09 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "epri.h"

struct monst *
christen_monst(struct monst *mtmp, const char *name)
{
    int lth;
    struct monst *mtmp2;
    char buf[PL_PSIZ];

    /* dogname & catname are PL_PSIZ arrays; object names have same limit */
    lth = *name ? (int)(strlen(name) + 1) : 0;
    if (lth > PL_PSIZ) {
        lth = PL_PSIZ;
        name = strncpy(buf, name, PL_PSIZ - 1);
        buf[PL_PSIZ - 1] = '\0';
    }
    if (lth == mtmp->mnamelth) {
        /* don't need to allocate a new monst struct */
        if (lth)
            strcpy(NAME_MUTABLE(mtmp), name);
        return mtmp;
    }
    mtmp2 = newmonst(mtmp->mxtyp, lth);
    *mtmp2 = *mtmp;
    memcpy(mtmp2->mextra, mtmp->mextra, mtmp->mxlth);
    mtmp2->mnamelth = lth;
    if (lth)
        strcpy(NAME_MUTABLE(mtmp2), name);
    replmon(mtmp, mtmp2);
    return mtmp2;
}


int
do_mname(const struct nh_cmd_arg *arg)
{
    coord cc;
    int cx, cy;
    struct monst *mtmp;
    const char *qbuf, *buf;

    if (Hallucination) {
        pline("You would never recognize it anyway.");
        return 0;
    }
    cc.x = u.ux;
    cc.y = u.uy;
    if (getargpos(arg, &cc, FALSE, "the monster you want to name") ==
        NHCR_CLIENT_CANCEL || (cx = cc.x) < 0)
        return 0;
    cy = cc.y;

    if (cx == u.ux && cy == u.uy) {
        if (u.usteed && canspotmon(u.usteed))
            mtmp = u.usteed;
        else {
            pline("This %s creature is called %s and cannot be renamed.",
                  beautiful(), u.uplname);
            return 0;
        }
    } else
        mtmp = m_at(level, cx, cy);

    unsigned msense_status = mtmp ? msensem(&youmonst, mtmp) : 0;

    if (!(msense_status & ~MSENSE_ITEMMIMIC)) {
        pline("I see no monster there.");
        return 0;
    } else if (!(msense_status & (MSENSE_ANYDETECT | MSENSE_ANYVISION))) {
        pline("You can't see it well enough to recognise it in the future.");
        return 0;
    }

    /* special case similar to the one in lookat() */
    qbuf = distant_monnam(mtmp, NULL, ARTICLE_THE);
    qbuf = msgprintf("What do you want to call %s?", qbuf);
    buf = getarglin(arg, qbuf);
    if (!*buf || *buf == '\033')
        return 0;
    /* strip leading and trailing spaces; unnames monster if all spaces */
    buf = msgmungspaces(buf);

    if (mtmp->data->geno & G_UNIQ) {
        qbuf = msgupcasefirst(distant_monnam(mtmp, NULL, ARTICLE_THE));
        pline("%s doesn't like being called names!", qbuf);
    } else
        christen_monst(mtmp, buf);
    return 0;
}

/* all but coins */
static const char nameable[] = {
    WEAPON_CLASS, ARMOR_CLASS, RING_CLASS, AMULET_CLASS, TOOL_CLASS,
    FOOD_CLASS, POTION_CLASS, SCROLL_CLASS, SPBOOK_CLASS, WAND_CLASS,
    GEM_CLASS, ROCK_CLASS, BALL_CLASS, CHAIN_CLASS, 0
};

/*
 * This routine changes the address of obj. Be careful not to call it
 * when there might be pointers around in unknown places. For now: only
 * when obj is in the inventory.
 */
int
do_oname(const struct nh_cmd_arg *arg)
{
    const char *qbuf, *buf;
    const char *aname;
    short objtyp;
    struct obj *obj;

    obj = getargobj(arg, nameable, "name");
    if (!obj)
        return 0;

    qbuf = msgprintf("What do you want to name %s %s?",
                     is_plural(obj) ? "these" : "this",
                     safe_qbuf("", sizeof("What do you want to name these ?"),
                               xname(obj), simple_typename(obj->otyp),
                               is_plural(obj) ? "things" : "thing"));
    buf = getarglin(arg, qbuf);
    if (!*buf || *buf == '\033')
        return 0;
    /* strip leading and trailing spaces; unnames item if all spaces */
    buf = msgmungspaces(buf);

    /* relax restrictions over proper capitalization for artifacts */
    if ((aname = artifact_name(buf, &objtyp)) != 0 && objtyp == obj->otyp)
        buf = aname;

    char slipbuf[strlen(buf) + 1];
    if (obj->oartifact) {
        pline("The artifact seems to resist the attempt.");
        return 0;
    } else if (restrict_name(obj, buf) || exist_artifact(obj->otyp, buf)) {
        int n = rn2((int)strlen(buf));
        char c1, c2;
        strcpy(slipbuf, buf);

        c1 = lowc(buf[n]);
        do
            c2 = 'a' + rn2('z' - 'a' + 1);
        while (c1 == c2);

        slipbuf[n] = (slipbuf[n] == c1) ? c2 : highc(c2);   /* keep same case */
        pline("While engraving your %s slips.", body_part(HAND));
        win_pause_output(P_MESSAGE);
        pline("You engrave: \"%s\".", slipbuf);
        buf = slipbuf;
    }
    oname(obj, buf);
    return 0;
}

/*
 * Allocate a new and possibly larger storage space for an obj.
 */
struct obj *
realloc_obj(struct obj *obj, int oextra_size, void *oextra_src, int oname_size,
            const char *name)
{
    struct obj *otmp;

    otmp = newobj(oextra_size + oname_size, obj);

    if (oextra_size) {
        if (oextra_src)
            memcpy(otmp->oextra, oextra_src, oextra_size);
    } else {
        otmp->oattached = OATTACHED_NOTHING;
    }
    otmp->oxlth = oextra_size;
    otmp->onamelth = oname_size;

    if (oname_size) {
        if (name)
            strcpy(ONAME_MUTABLE(otmp), name);
    }

    /* !obj->olev means the obj is currently being restored and no pointer from 
       or to it is valid. Re-equipping, timer linking, etc. will happen
       elsewhere in that case. */
    if (obj->olev) {
        int i;
        if (obj->owornmask) {
            boolean save_twoweap = u.twoweap;

            /* unwearing the old instance will clear dual-wield mode if this
               object is either of the two weapons */
            setworn(NULL, obj->owornmask);
            setworn(otmp, otmp->owornmask);
            u.twoweap = save_twoweap;
        }

        /* replace obj with otmp */
        replace_object(obj, otmp);

        /* fix ocontainer pointers */
        if (Has_contents(obj)) {
            struct obj *inside;

            for (inside = obj->cobj; inside; inside = inside->nobj)
                inside->ocontainer = otmp;
        }

        /* move timers and light sources from obj to otmp */
        otmp->timed = 0;        /* not timed, yet */
        if (obj->timed)
            obj_move_timers(obj, otmp);
        otmp->lamplit = 0;      /* ditto */
        if (obj->lamplit)
            obj_move_light_source(obj, otmp);

        /* objects possibly being manipulated by multi-turn occupations which
           have been interrupted but might be subsequently resumed */
        for (i = 0; i <= tos_last_slot; i++) {
            if (obj == u.utracked[i])
                u.utracked[i] = otmp;
        }
        /* This is probably paranoia; it would only come up if an item can
           end up being specific-named as a result of trying to use it. */
        for (i = 0; i <= ttos_last_slot; i++) {
            if (obj == turnstate.tracked[i])
                turnstate.tracked[i] = otmp;
        }
    } else {
        /* During restore, floating objects are on the floating objects
           chain, /but/ may not have OBJ_FREE set. */
        otmp->where = obj->where;
        obj->where = OBJ_FREE;
        obj->timed = FALSE;
        obj->lamplit = FALSE;
    }
    /* obfree(obj, otmp); now unnecessary: no pointers on bill */

    dealloc_obj(obj);   /* let us hope nobody else saved a pointer */
    return otmp;
}

struct obj *
oname(struct obj *obj, const char *name)
{
    int lth;
    char buf[PL_PSIZ];

    lth = *name ? (int)(strlen(name) + 1) : 0;
    if (lth > PL_PSIZ) {
        lth = PL_PSIZ;
        name = strncpy(buf, name, PL_PSIZ - 1);
        buf[PL_PSIZ - 1] = '\0';
    }
    /* If named artifact exists in the game, do not create another. Also trying 
       to create an artifact shouldn't de-artifact it (e.g. Excalibur from
       prayer). In this case the object will retain its current name. */
    if (obj->oartifact || (lth && exist_artifact(obj->otyp, name)))
        return obj;

    if (lth == obj->onamelth) {
        /* no need to replace entire object */
        if (lth)
            strcpy(ONAME_MUTABLE(obj), name);
    } else {
        obj = realloc_obj(obj, obj->oxlth, obj->oextra, lth, name);
    }
    if (lth)
        artifact_exists(obj, name, TRUE);
    if (obj->oartifact) {
        /* can't dual-wield with artifact as secondary weapon */
        if (obj == uswapwep)
            untwoweapon();
    }
    if (carried(obj))
        update_inventory();
    return obj;
}

static void
docall_inner(const struct nh_cmd_arg *arg, int otyp)
{
    const char *qbuf, *buf;
    char **str1;
    const char *ot = obj_typename(otyp);

    qbuf = "Call ";
    if (strstr(ot, " boots") || strstr(ot, " gloves"))
        qbuf = msgcat(qbuf, ot);
    else
        qbuf = msgcat(qbuf, an(ot));

    qbuf = msgcat(qbuf, ":");
    buf = getarglin(arg, qbuf);
    if (!*buf || *buf == '\033')
        return;

    /* clear old name */
    str1 = &(objects[otyp].oc_uname);
    if (*str1)
        free(*str1);

    /* strip leading and trailing spaces; uncalls item if all spaces */
    buf = msgmungspaces(buf);
    if (!*buf) {
        if (*str1) {    /* had name, so possibly remove from disco[] */
            /* strip name first, for the update_inventory() call from
               undiscover_object() */
            *str1 = (char *)0;
            undiscover_object(otyp);
        }
    } else {
        *str1 = strcpy((char *)malloc((unsigned)strlen(buf) + 1), buf);
        discover_object(otyp, FALSE, TRUE, TRUE); /* possibly add to disco[] */
    }
}

static const char callable[] = {
    SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS, AMULET_CLASS,
    GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS, 0
};

int
do_tname(const struct nh_cmd_arg *arg)
{
    struct obj *obj = getargobj(arg, callable, "call");
    if (obj) {
        /* behave as if examining it in inventory; this might set dknown if it 
           was picked up while blind and the hero can now see */
        examine_object(obj);

        if (!obj->dknown) {
            pline("You would never recognize another one.");
            return 0;
        }
        docall_inner(arg, obj->otyp);
    }
    return 0;
}

int
do_naming(const struct nh_cmd_arg *arg)
{
    int n;
    const int *selected;
    char classes[20], *s;
    struct nh_menulist menu;

    (void) arg;

    init_menulist(&menu);

    /* group_accel settings are for keystroke-compatibility with nethack.alt.org
       (which uses a/b/c/d); they shouldn't show in the interface because
       there's no good reason to use them other than muscle memory */
    add_menuitem(&menu, 1, "Name a monster", 'C', FALSE);
    menu.items[menu.icount-1].group_accel = 'a';
    add_menuitem(&menu, 2, "Name the current level", 'f', FALSE);
    add_menuitem(&menu, 3, "Name an individual item", 'y', FALSE);
    menu.items[menu.icount-1].group_accel = 'b';
    add_menuitem(&menu, 4, "Name all items of a certain type", 'n', FALSE);
    menu.items[menu.icount-1].group_accel = 'c';
    add_menuitem(&menu, 5, "Name an item type by appearance", 'A', FALSE);
    if (flags.recently_broken_otyp != STRANGE_OBJECT) {
        const char *buf;

        buf = msgprintf("Name %s (recently broken)",
                        an(obj_typename(flags.recently_broken_otyp)));
        add_menuitem(&menu, 6, buf, 'V', FALSE);
    }

    n = display_menu(&menu, "What do you wish to name?",
                     PICK_ONE, PLHINT_ANYWHERE, &selected);
    if (n > 0)
        n = selected[0] - 1;
    else
        return 0;

    switch (n) {
    default:
        break;
    case 0:
        do_mname(&(struct nh_cmd_arg){.argtype = 0});
        break;

    case 1:
        donamelevel(&(struct nh_cmd_arg){.argtype = 0});
        break;

    case 2:
        do_oname(&(struct nh_cmd_arg){.argtype = 0});
        break;

    case 3:
        do_tname(&(struct nh_cmd_arg){.argtype = 0});
        break;

    case 4:
        strcpy(classes, flags.inv_order);
        init_menulist(&menu);

        /* This leaks information unless we put things in a consistent order.
           It's probably simplest to alphabetise. */
        for (s = classes; *s; s++) {
            int alphaorder[NUM_OBJECTS];
            int aop = 0;
            int i;

            if (*s != RING_CLASS && *s != AMULET_CLASS && *s != SCROLL_CLASS &&
                *s != POTION_CLASS && *s != WAND_CLASS && *s != SPBOOK_CLASS &&
                *s != ARMOR_CLASS)
                continue;
            for (n = bases[(int)*s];
                 n < NUM_OBJECTS && objects[n].oc_class == *s; n++) {
                if (!objects[n].oc_name_known && !objects[n].oc_unique &&
                    n != FAKE_AMULET_OF_YENDOR) {
                    if (*s != ARMOR_CLASS ||
                        (n >= HELMET && n <= HELM_OF_TELEPATHY) ||
                        (n >= LEATHER_GLOVES && n <= GAUNTLETS_OF_DEXTERITY) ||
                        (n >= CLOAK_OF_PROTECTION && n <= CLOAK_OF_DISPLACEMENT)
                        || (n >= SPEED_BOOTS && n <= LEVITATION_BOOTS))
                        alphaorder[aop++] = n;
                }
            }
            for (n = 0; n < aop; n++) {
                for (i = n + 1; i < aop; i++) {
                    if (strcmp
                        (OBJ_DESCR(objects[alphaorder[i]]),
                         OBJ_DESCR(objects[alphaorder[n]])) < 0) {
                        int t = alphaorder[i];

                        alphaorder[i] = alphaorder[n];
                        alphaorder[n] = t;
                    }
                }
            }
            for (i = 0; i < aop; i++) {
                add_menuitem(&menu, alphaorder[i], obj_typename(alphaorder[i]),
                             0, FALSE);
            }
        }
        n = display_menu(&menu,
                         "Name items with which appearance?", PICK_ONE,
                         PLHINT_INVENTORY, &selected);
        if (n == 1)
            docall_inner(&(struct nh_cmd_arg){.argtype = 0},
                         selected[0]);
        break;

    case 5:
        docall_inner(&(struct nh_cmd_arg){.argtype = 0},
                     flags.recently_broken_otyp);
        break;
    }
    return 0;
}


void
docall(struct obj *obj)
{
    struct obj otemp;

    if (!obj->dknown)
        return; /* probably blind */
    otemp = *obj;
    otemp.quan = 1L;
    otemp.onamelth = 0;
    otemp.oxlth = 0;
    if (objects[otemp.otyp].oc_class == POTION_CLASS && otemp.fromsink)
        /* kludge, meaning it's sink water */
        pline("(You can name a stream of %s fluid from the item naming menu.)",
              OBJ_DESCR(objects[otemp.otyp]));
    else
        pline("(You can name %s from the item naming menu.)",
              an(xname(&otemp)));
    flags.recently_broken_otyp = otemp.otyp;
}


static const char *const ghostnames[] = {
    /* these names should have length < PL_NSIZ */
    /* Capitalize the names for aesthetics -dgk */
    "Adri", "Andries", "Andreas", "Bert", "David", "Dirk", "Emile",
    "Frans", "Fred", "Greg", "Hether", "Jay", "John", "Jon", "Karnov",
    "Kay", "Kenny", "Kevin", "Maud", "Michiel", "Mike", "Peter", "Robert",
    "Ron", "Tom", "Wilmar", "Nick Danger", "Phoenix", "Jiro", "Mizue",
    "Stephan", "Lance Braccus", "Shadowhawk"
};

/* ghost names formerly set by x_monnam(), now by makemon() instead */
const char *
rndghostname(void)
{
    return rn2(7) ? ghostnames[rn2(SIZE(ghostnames))] :
        (const char *)u.uplname;
}

/* Monster naming functions:
 * x_monnam is the generic monster-naming function.
 *                seen        unseen       detected               named
 * mon_nam:     the newt        it      the invisible orc       Fido
 * noit_mon_nam:the newt (as if detected) the invisible orc     Fido
 * l_monnam:    newt            it      invisible orc           dog called Fido
 * Monnam:      The newt        It      The invisible orc       Fido
 * noit_Monnam: The newt (as if detected) The invisible orc     Fido
 * Adjmonnam:   The poor newt   It      The poor invisible orc  The poor Fido
 * Amonnam:     A newt          It      An invisible orc        Fido
 * a_monnam:    a newt          it      an invisible orc        Fido
 * m_monnam:    newt            xan     orc                     Fido
 * y_monnam:    your newt     your xan  your invisible orc      Fido
 * k_monnam:    a newt        a xan     an invisible orc     a dog called "Fido"
 */

/* Bug: if the monster is a priest or shopkeeper, not every one of these
 * options works, since those are special cases.
 */
const char *
x_monnam(const struct monst *mtmp,
         int article, /* ARTICLE_NONE, ARTICLE_THE, ARTICLE_A: obvious
                         ARTICLE_YOUR: "your" on pets, "the" on
                         everything else
                         If the monster would be referred to as "it"
                         or if the monster has a name _and_ there is
                         no adjective, "invisible", "saddled", etc.,
                         override this and always use no article. */
         const char *adjective,
         int suppress,   /* SUPPRESS_IT, SUPPRESS_INVISIBLE,
                            SUPPRESS_HALLUCINATION,
                            SUPPRESS_SADDLE. EXACT_NAME: combination
                            of all the above */
         boolean called)
{
    const struct permonst *mdat = mtmp->data;
    boolean do_hallu, do_invis, do_it, do_saddle;
    boolean name_at_start, has_adjectives;
    const char *buf = "";
    const char *bp;

    if (program_state.gameover)
        suppress |= SUPPRESS_HALLUCINATION;
    if (article == ARTICLE_YOUR && !mtmp->mtame)
        article = ARTICLE_THE;

    do_hallu = Hallucination && !(suppress & SUPPRESS_HALLUCINATION);
    do_invis = mtmp->minvis && !(suppress & SUPPRESS_INVISIBLE);
    do_it = !canclassifymon(mtmp) && article != ARTICLE_YOUR &&
        !program_state.gameover && mtmp != u.usteed &&
        !(Engulfed && mtmp == u.ustuck) &&
        !(suppress & SUPPRESS_IT);
    do_saddle = !(suppress & SUPPRESS_SADDLE);

    /* unseen monsters, etc.  Use "it" */
    if (do_it) {
        return "it";
    }

    /* priests and minions: don't even use this function */
    if (mtmp->ispriest || mtmp->isminion) {
        struct monst *priestmon = newmonst(mtmp->mxtyp, mtmp->mnamelth);
        const char *name;

        memcpy(priestmon, mtmp,
               sizeof (struct monst) + mtmp->mxlth + mtmp->mnamelth);

        /* when true name is wanted, explicitly block Hallucination */
        if (!do_invis)
            priestmon->minvis = 0;
        name = priestname(priestmon, !do_hallu);

        if (article == ARTICLE_NONE && !strncmp(name, "the ", 4))
            name += 4;

        free(priestmon);
        return name;
    }

    /* Shopkeepers: use shopkeeper name.  For normal shopkeepers, just
       "Asidonhopo"; for unusual ones, "Asidonhopo the invisible shopkeeper" or 
       "Asidonhopo the blue dragon".  If hallucinating, none of this applies. */
    if (mtmp->isshk && !do_hallu) {
        if (adjective && article == ARTICLE_THE) {
            /* pathological case: "the angry Asidonhopo the blue dragon" sounds 
               silly */
            return msgcat_many("the ", adjective, " ", shkname(mtmp), NULL);
        }
        /* TODO: Shouldn't there be a case for "the angry Asidonhopo" here? */
        if (mdat == &mons[PM_SHOPKEEPER] && !do_invis)
            return shkname(mtmp);

        buf = msgcat(shkname(mtmp), " the ");
        if (do_invis)
            buf = msgcat(buf, "invisible ");
        buf = msgcat(buf, mdat->mname);
        return buf;
    }

    /* Put the adjectives in the buffer */
    if (adjective)
        buf = msgcat(adjective, " ");
    if (do_invis)
        buf = msgcat(buf, "invisible ");

    if (do_saddle && (mtmp->misc_worn_check & W_MASK(os_saddle)) && !Blind &&
        !Hallucination)
        buf = msgcat(buf, "saddled ");

    if (*buf)
        has_adjectives = TRUE;
    else
        has_adjectives = FALSE;

    /* Put the actual monster name or type into the buffer now */
    /* Be sure to remember whether the buffer starts with a name */
    if (do_hallu) {
        int idx = rndmonidx();

        buf = msgcat(buf, monnam_for_index(idx));
        name_at_start = monnam_is_pname(idx);
    } else if (mtmp->mnamelth) {
        const char *name = NAME(mtmp);

        if (mdat == &mons[PM_GHOST]) {
            buf = msgprintf("%s%s ghost", buf, s_suffix(name));
            name_at_start = TRUE;
        } else if (called) {
            buf = msgprintf("%s%s called %s", buf, mdat->mname, name);
            name_at_start = (boolean) type_is_pname(mdat);
        } else if (is_mplayer(mdat) && ((bp = strstri(name, " the "))) != 0) {
            /* <name> the <adjective> <invisible> <saddled> <rank> */
            buf = msgprintf("%.*s %s%s", (int) (bp - name + 5), name, buf,
                            bp + 5);
            article = ARTICLE_NONE;
            name_at_start = TRUE;
        } else {
            buf = msgcat(buf, name);
            name_at_start = TRUE;
        }
    } else if (is_mplayer(mdat) && !In_endgame(&u.uz)) {
        buf = msgcat(buf, msglowercase(rank_of((int)mtmp->m_lev, monsndx(mdat),
                                               (boolean) mtmp->female)));
        name_at_start = FALSE;
    } else {
        buf = msgcat(buf, mdat->mname);
        name_at_start = (boolean) type_is_pname(mdat);
    }

    if (name_at_start && (article == ARTICLE_YOUR || !has_adjectives)) {
        if (mdat == &mons[PM_WIZARD_OF_YENDOR])
            article = ARTICLE_THE;
        else
            article = ARTICLE_NONE;
    } else if ((mdat->geno & G_UNIQ) && article == ARTICLE_A) {
        article = ARTICLE_THE;
    }

    switch (article) {
    case ARTICLE_YOUR:
        return msgcat("your ", buf);
    case ARTICLE_THE:
        return msgcat("the ", buf);
    case ARTICLE_A:
        return an(buf);
    case ARTICLE_NONE:
    default:
        return buf;
    }
}


const char *
l_monnam(const struct monst *mtmp)
{
    return (x_monnam
            (mtmp, ARTICLE_NONE, NULL, mtmp->mnamelth ? SUPPRESS_SADDLE : 0,
             TRUE));
}


const char *
mon_nam(const struct monst *mtmp)
{
    return (x_monnam
            (mtmp, ARTICLE_THE, NULL, mtmp->mnamelth ? SUPPRESS_SADDLE : 0,
             FALSE));
}

/* print the name as if mon_nam() was called, but assume that the player can
   always see the monster--used for probing and for monsters aggravating the
   player with a cursed potion of invisibility */
const char *
noit_mon_nam(const struct monst *mtmp)
{
    return (x_monnam
            (mtmp, ARTICLE_THE, NULL,
             mtmp->mnamelth ? (SUPPRESS_SADDLE | SUPPRESS_IT) : SUPPRESS_IT,
             FALSE));
}

const char *
Monnam(const struct monst *mtmp)
{
    return msgupcasefirst(mon_nam(mtmp));
}

const char *
noit_Monnam(const struct monst *mtmp)
{
    return msgupcasefirst(noit_mon_nam(mtmp));
}

/* monster's own name */
const char *
m_monnam(const struct monst *mtmp)
{
    return x_monnam(mtmp, ARTICLE_NONE, NULL, EXACT_NAME, FALSE);
}

/* pet name: "your little dog" */
const char *
y_monnam(const struct monst *mtmp)
{
    int prefix, suppression_flag;

    prefix = mtmp->mtame ? ARTICLE_YOUR : ARTICLE_THE;
    /* "saddled" is redundant when mounted */
    suppression_flag = (mtmp->mnamelth ||
                        mtmp == u.usteed) ? SUPPRESS_SADDLE : 0;

    return x_monnam(mtmp, prefix, NULL, suppression_flag, FALSE);
}


const char *
Adjmonnam(const struct monst *mtmp, const char *adj)
{
    return msgupcasefirst(
        x_monnam(mtmp, ARTICLE_THE, adj,
                 mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE));
}

const char *
a_monnam(const struct monst *mtmp)
{
    return x_monnam(mtmp, ARTICLE_A, NULL, mtmp->mnamelth ? SUPPRESS_SADDLE : 0,
                    FALSE);
}

const char *
Amonnam(const struct monst *mtmp)
{
    return msgupcasefirst(a_monnam(mtmp));
}

/* used for monster ID by the '/', ';', and 'C' commands to block remote
   identification of the endgame altars via their attending priests */
/* article: only ARTICLE_NONE and ARTICLE_THE are handled here */
const char *
distant_monnam(const struct monst *mon, const char *adjective, int article)
{
    /* high priest(ess)'s identity is concealed on the Astral Plane, unless
       you're adjacent (overridden for hallucination which does its own
       obfuscation) */
    if (mon->data == &mons[PM_HIGH_PRIEST] && !Hallucination &&
        has_sanctum(level, Align2amask(CONST_EPRI(mon)->shralign)) &&
        CONST_EPRI(mon)->shralign != A_NONE && distu(mon->mx, mon->my) > 2) {
        return msgcat(article == ARTICLE_THE ? "the " : "",
                      mon->female ? "high priestess" : "high priest");
    } else {
        return x_monnam(mon, article, adjective, 0, TRUE);
    }
}

/* appropriate monster name for a death message */
const char *
k_monnam(const struct monst *mtmp) {
    const char *buf = "";
    boolean distorted = (boolean) (Hallucination && canclassifymon(mtmp));
    boolean article = FALSE;

    if ((mtmp->data->geno & G_UNIQ) != 0 &&
        !(mtmp->data == &mons[PM_HIGH_PRIEST] && !mtmp->ispriest)) {
        /* "killed by the high priest of Crom" is okay, "killed by the high
           priest" alone isn't */
        if (!type_is_pname(mtmp->data))
            buf = "the ";
    }
    else if (mtmp->data == &mons[PM_GHOST] && mtmp->mnamelth) {
        /* _the_ <invisible> <distorted> ghost of Dudley */
        buf = "the ";
    } else if (!(mtmp->data->geno & G_UNIQ)) {
        article = TRUE;
    }

    if (mtmp->minvis)
        buf = msgcat(buf, "invisible ");
    if (distorted)
        buf = msgcat(buf, "hallucinogen-distorted ");

    if (mtmp->data == &mons[PM_GHOST]) {
        buf = msgcat(buf, "ghost");
        if (mtmp->mnamelth)
            buf = msgprintf("%sof %s", buf, NAME(mtmp));
    } else if (mtmp->isshk) {
        buf = msgprintf("%s%s %s, the shopkeeper", buf,
                        (mtmp->female ? "Ms." : "Mr."), shkname(mtmp));
    } else if (mtmp->ispriest || mtmp->isminion) {
        /* m_monnam() suppresses "the" prefix plus "invisible", and it
           overrides the effect of Hallucination on priestname() */
        buf = msgcat(buf, m_monnam(mtmp));
    } else {
        buf = msgcat(buf, mtmp->data->mname);
        if (mtmp->mnamelth)
            buf = msgcat_many(buf, " called \"", NAME(mtmp), "\"", NULL);
    }

    return article ? an(buf) : buf;
}


/* Returns a name converted to possessive. Moved here from hacklib so that it's
   allowed to use the msg* functions; it's only called from libnethack
   anyway. */
const char *
s_suffix(const char *s)
{
    if (!*s)
        return "'s"; /* prevent underflow checking for a trailing 's' */
    if (!strcmpi(s, "it"))
        return "its";
    if (s[strlen(s)-1] == 's')
        return msgcat(s, "'");
    else
        return msgcat(s, "'s");
}


static struct {
    const char *name;
    const boolean pname;
} bogusmons[] = {
    /* misc. */
    {"jumbo shrimp", FALSE},
    {"giant pigmy", FALSE},
    {"gnu", FALSE},
    {"killer penguin", FALSE},
    {"giant cockroach", FALSE},
    {"giant slug", FALSE},
    {"maggot", FALSE},
    {"pterodactyl", FALSE},
    {"tyrannosaurus rex", FALSE},
    {"basilisk", FALSE},
    {"beholder", FALSE},
    {"nightmare", FALSE},
    {"efreeti", FALSE},
    {"marid", FALSE},
    {"rot grub", FALSE},
    {"bookworm", FALSE},
    {"master lichen", FALSE},
    {"shadow", FALSE},
    {"hologram", FALSE},
    {"jester", FALSE},
    {"attorney", FALSE},
    {"sleazoid", FALSE},
    {"killer tomato", FALSE},
    {"amazon", FALSE},
    {"robot", FALSE},
    {"battlemech", FALSE},
    {"rhinovirus", FALSE},
    {"harpy", FALSE},
    {"lion-dog", FALSE},
    {"rat-ant", FALSE},
    {"Y2K bug", FALSE},
        /* Quendor (Zork, &c.) */
    {"grue", FALSE},
    {"Christmas-tree monster", FALSE},
    {"luck sucker", FALSE},
    {"paskald", FALSE},
    {"brogmoid", FALSE},
    {"dornbeast", FALSE},
        /* Moria */
    {"Ancient Multi-Hued Dragon", FALSE},
    {"Evil Iggy", FALSE},
        /* Rogue */
    {"emu", FALSE},
    {"kestrel", FALSE},
    {"xeroc", FALSE},
    {"venus flytrap", FALSE},
        /* Wizardry */
    {"creeping coins", FALSE},
        /* Greek legend */
    {"hydra", FALSE},
    {"siren", FALSE},
        /* Monty Python */
    {"killer bunny", FALSE},
        /* The Princess Bride */
    {"rodent of unusual size", FALSE},
        /* "Only you can prevent forest fires!" */
    {"Smokey the bear", TRUE},
        /* Discworld */
    {"Luggage", FALSE},
        /* Lord of the Rings */
    {"Ent", FALSE},
        /* Xanth */
    {"tangle tree", FALSE},
    {"nickelpede", FALSE},
    {"wiggle", FALSE},
        /* Lewis Carroll */
    {"white rabbit", FALSE},
    {"snark", FALSE},
        /* Dr. Dolittle */
    {"pushmi-pullyu", FALSE},
        /* The Smurfs */
    {"smurf", FALSE},
        /* Star Trek */
    {"tribble", FALSE},
    {"Klingon", FALSE},
    {"Borg", FALSE},
        /* Star Wars */
    {"Ewok", FALSE},
        /* Tonari no Totoro */
    {"Totoro", FALSE},
        /* Nausicaa */
    {"ohmu", FALSE},
        /* Sailor Moon */
    {"youma", FALSE},
        /* Pokemon (Meowth) */
    {"nyaasu", FALSE},
        /* monster movies */
    {"Godzilla", TRUE},
    {"King Kong", TRUE},
        /* old L of SH */
    {"earthquake beast", FALSE},
        /* Robotech */
    {"Invid", FALSE},
        /* The Terminator */
    {"Terminator", FALSE},
        /* Bubblegum Crisis */
    {"boomer", FALSE},
        /* Dr. Who ("Exterminate!") */
    {"Dalek", FALSE},
        /* Hitchhiker's Guide to the Galaxy */
    {"microscopic space fleet", FALSE},
    {"Ravenous Bugblatter Beast of Traal", FALSE},
        /* TMNT */
    {"teenage mutant ninja turtle", FALSE},
        /* Usagi Yojimbo */
    {"samurai rabbit", FALSE},
        /* Cerebus */
    {"aardvark", FALSE},
        /* Little Shop of Horrors */
    {"Audrey II", TRUE},
        /* 50's rock 'n' roll */
    {"witch doctor", FALSE},
    {"one-eyed one-horned flying purple people eater", FALSE},
        /* saccharine kiddy TV */
    {"Barney the dinosaur", TRUE},
        /* Angband */
    {"Morgoth", TRUE},
        /* Babylon 5 */
    {"Vorlon", FALSE},
        /* King Arthur */
    {"questing beast", FALSE},
        /* Movie */
    {"Predator", FALSE},
        /* common pest */
    {"mother-in-law", FALSE},
        /* Battlestar Galactica */
    {"cylon", FALSE},
};


/* Return a random monster name, for hallucination.
 */
int
rndmonidx(void)
{
    int idx;

    do {
        idx = rn2_on_display_rng(SPECIAL_PM + SIZE(bogusmons) - LOW_PM)
            + LOW_PM;
    } while (idx < SPECIAL_PM &&
             (type_is_pname(&mons[idx]) || (mons[idx].geno & G_NOGEN)));

    return idx;
}

const char *
monnam_for_index(int idx)
{
    if (idx >= SPECIAL_PM)
        return (bogusmons[idx - SPECIAL_PM].name);
    return mons[idx].mname;
}

boolean
monnam_is_pname(int idx)
{
    if (idx >= SPECIAL_PM)
        return (bogusmons[idx - SPECIAL_PM].pname);
    return type_is_pname(&mons[idx]);
}

const char *
roguename(void)
{       /* Name of a Rogue player */
    return rn2(3) ? (rn2(2) ? "Michael Toy" : "Kenneth Arnold")
        : "Glenn Wichman";
}

static const char *const hcolors[] = {
    "ultraviolet", "infrared", "bluish-orange",
    "reddish-green", "dark white", "light black", "sky blue-pink",
    "salty", "sweet", "sour", "bitter",
    "striped", "spiral", "swirly", "plaid", "checkered", "argyle",
    "paisley", "blotchy", "guernsey-spotted", "polka-dotted",
    "square", "round", "triangular",
    "cabernet", "sangria", "fuchsia", "wisteria",
    "lemon-lime", "strawberry-banana", "peppermint",
    "romantic", "incandescent"
};

const char *
hcolor(const char *colorpref)
{
    return (Hallucination || !colorpref) ?
        hcolors[rn2_on_display_rng(SIZE(hcolors))] : colorpref;
}

/* return a random real color unless hallucinating */
const char *
rndcolor(void)
{
    int k = rn2(CLR_MAX);

    return Hallucination ? hcolor(NULL) : c_obj_colors[k];
}

/* Aliases for road-runner nemesis
 */
static const char *const coynames[] = {
    "Carnivorous Vulgaris", "Road-Runnerus Digestus",
    "Eatibus Anythingus", "Famishus-Famishus",
    "Eatibus Almost Anythingus", "Eatius Birdius",
    "Famishius Fantasticus", "Eternalii Famishiis",
    "Famishus Vulgarus", "Famishius Vulgaris Ingeniusi",
    "Eatius-Slobbius", "Hardheadipus Oedipus",
    "Carnivorous Slobbius", "Hard-Headipus Ravenus",
    "Evereadii Eatibus", "Apetitius Giganticus",
    "Hungrii Flea-Bagius", "Overconfidentii Vulgaris",
    "Caninus Nervous Rex", "Grotesques Appetitus",
    "Nemesis Riduclii", "Canis latrans"
};

const char *
coyotename(const struct monst *mtmp)
{
    return msgprintf("%s - %s",
                     x_monnam(mtmp, ARTICLE_NONE, NULL, 0, TRUE),
                     mtmp->mcan ? coynames[SIZE(coynames) - 1] :
                     coynames[rn2_on_display_rng(SIZE(coynames) - 1)]);
}

/*do_name.c*/

