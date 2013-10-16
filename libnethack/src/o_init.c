/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-10-16 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"        /* save & restore info */

static void setgemprobs(const d_level * dlev);
static void shuffle(int, int, boolean);
static void shuffle_all(void);
static boolean interesting_to_discover(int);


static void
setgemprobs(const d_level * dlev)
{
    int j, first, lev;

    if (dlev)
        lev = (ledger_no(dlev) > maxledgerno())
            ? maxledgerno() : ledger_no(dlev);
    else
        lev = 0;
    first = bases[GEM_CLASS];

    for (j = 0; j < 9 - lev / 3; j++)
        objects[first + j].oc_prob = 0;
    first += j;
    if (first > LAST_GEM || objects[first].oc_class != GEM_CLASS ||
        OBJ_NAME(objects[first]) == NULL) {
        raw_printf("Not enough gems? - first=%d j=%d LAST_GEM=%d\n", first, j,
                   LAST_GEM);
    }
    for (j = first; j <= LAST_GEM; j++)
        objects[j].oc_prob = (171 + j - first) / (LAST_GEM + 1 - first);
}

/* shuffle descriptions on objects o_low to o_high */
static void
shuffle(int o_low, int o_high, boolean domaterial)
{
    int i, j, num_to_shuffle;
    short sw;
    int color;

    for (num_to_shuffle = 0, j = o_low; j <= o_high; j++)
        if (!objects[j].oc_name_known)
            num_to_shuffle++;
    if (num_to_shuffle < 2)
        return;

    for (j = o_low; j <= o_high; j++) {
        if (objects[j].oc_name_known)
            continue;
        do
            i = j + rn2(o_high - j + 1);
        while (objects[i].oc_name_known);
        sw = objects[j].oc_descr_idx;
        objects[j].oc_descr_idx = objects[i].oc_descr_idx;
        objects[i].oc_descr_idx = sw;
        sw = objects[j].oc_tough;
        objects[j].oc_tough = objects[i].oc_tough;
        objects[i].oc_tough = sw;
        color = objects[j].oc_color;
        objects[j].oc_color = objects[i].oc_color;
        objects[i].oc_color = color;

        /* shuffle material */
        if (domaterial) {
            sw = objects[j].oc_material;
            objects[j].oc_material = objects[i].oc_material;
            objects[i].oc_material = sw;
        }
    }
}


#define COPY_OBJ_DESCR(o_dst,o_src) \
            o_dst.oc_descr_idx = o_src.oc_descr_idx,\
            o_dst.oc_color = o_src.oc_color

void
init_objects(void)
{
    int i, first, last, sum;
    char oclass;

    /* initialize object descriptions */
    for (i = 0; i < NUM_OBJECTS; i++)
        objects[i].oc_name_idx = objects[i].oc_descr_idx = i;
    /* init base; if probs given check that they add up to 1000, otherwise
       compute probs */
    first = 0;
    while (first < NUM_OBJECTS) {
        oclass = objects[first].oc_class;
        last = first + 1;
        while (last < NUM_OBJECTS && objects[last].oc_class == oclass)
            last++;

        if (oclass == GEM_CLASS) {
            setgemprobs(NULL);

            if (rn2(2)) {       /* change turquoise from green to blue? */
                COPY_OBJ_DESCR(objects[TURQUOISE], objects[SAPPHIRE]);
            }
            if (rn2(2)) {       /* change aquamarine from green to blue? */
                COPY_OBJ_DESCR(objects[AQUAMARINE], objects[SAPPHIRE]);
            }
            switch (rn2(4)) {   /* change fluorite from violet? */
            case 0:
                break;
            case 1:    /* blue */
                COPY_OBJ_DESCR(objects[FLUORITE], objects[SAPPHIRE]);
                break;
            case 2:    /* white */
                COPY_OBJ_DESCR(objects[FLUORITE], objects[DIAMOND]);
                break;
            case 3:    /* green */
                COPY_OBJ_DESCR(objects[FLUORITE], objects[EMERALD]);
                break;
            }
        }
    check:
        sum = 0;
        for (i = first; i < last; i++)
            sum += objects[i].oc_prob;
        if (sum == 0) {
            for (i = first; i < last; i++)
                objects[i].oc_prob = (1000 + i - first) / (last - first);
            goto check;
        }
        if (sum != 1000)
            raw_printf("init-prob error for class %d (%d%%)\n", oclass, sum);

        first = last;
    }
    /* shuffle descriptions */
    shuffle_all();
}

static void
shuffle_all(void)
{
    int first, last, oclass;

    for (oclass = 1; oclass < MAXOCLASSES; oclass++) {
        first = bases[oclass];
        last = first + 1;
        while (last < NUM_OBJECTS && objects[last].oc_class == oclass)
            last++;

        if (OBJ_DESCR(objects[first]) != NULL && oclass != TOOL_CLASS &&
            oclass != WEAPON_CLASS && oclass != ARMOR_CLASS &&
            oclass != GEM_CLASS) {
            int j = last - 1;

            if (oclass == POTION_CLASS)
                j -= 1; /* only water has a fixed description */
            else if (oclass == AMULET_CLASS || oclass == SCROLL_CLASS ||
                     oclass == SPBOOK_CLASS) {
                while (!objects[j].oc_magic || objects[j].oc_unique)
                    j--;
            }

            /* non-magical amulets, scrolls, and spellbooks (ex. imitation
               Amulets, blank, scrolls of mail) and one-of-a-kind magical
               artifacts at the end of their class in objects[] have fixed
               descriptions. */
            shuffle(first, j, TRUE);
        }
    }

    /* shuffle the helmets */
    shuffle(HELMET, HELM_OF_TELEPATHY, FALSE);

    /* shuffle the gloves */
    shuffle(LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY, FALSE);

    /* shuffle the cloaks */
    shuffle(CLOAK_OF_PROTECTION, CLOAK_OF_DISPLACEMENT, FALSE);

    /* shuffle the boots [if they change, update find_skates() below] */
    shuffle(SPEED_BOOTS, LEVITATION_BOOTS, FALSE);
}

/* find the object index for snow boots; used [once] by slippery ice code */
int
find_skates(void)
{
    int i;
    const char *s;

    for (i = SPEED_BOOTS; i <= LEVITATION_BOOTS; i++)
        if ((s = OBJ_DESCR(objects[i])) != 0 && !strcmp(s, "snow boots"))
            return i;

    impossible("snow boots not found?");
    return -1;  /* not 0, or caller would try again each move */
}

void
oinit(const struct level *lev)
{       /* level dependent initialization */
    setgemprobs(&lev->z);
}


static void
saveobjclass(struct memfile *mf, struct objclass *ocl)
{
    int namelen = 0;
    unsigned int oflags;

    /* no mtag useful; object classes are always saved in the same order and
       there are always the same number of them */
    oflags =
        (ocl->oc_name_known << 31) | (ocl->oc_merge << 30) |
        (ocl->oc_uses_known << 29) | (ocl->oc_pre_discovered << 28) |
        (ocl->oc_magic << 27) | (ocl->oc_charged << 26) |
        (ocl->oc_unique << 25) | (ocl->oc_nowish << 24) |
        (ocl->oc_big << 23) | (ocl->oc_tough << 22) |
        (ocl->oc_dir << 20) | (ocl->oc_material << 15) |
        (ocl->oc_disclose_id << 14);
    mwrite32(mf, oflags);
    mwrite16(mf, ocl->oc_name_idx);
    mwrite16(mf, ocl->oc_descr_idx);
    mwrite16(mf, ocl->oc_weight);
    mwrite16(mf, ocl->oc_prob);
    mwrite16(mf, ocl->oc_cost);
    mwrite16(mf, ocl->oc_nutrition);

    mwrite8(mf, ocl->oc_subtyp);
    mwrite8(mf, ocl->oc_oprop);
    mwrite8(mf, ocl->oc_class);
    mwrite8(mf, ocl->oc_delay);
    mwrite8(mf, ocl->oc_color);
    mwrite8(mf, ocl->oc_wsdam);
    mwrite8(mf, ocl->oc_wldam);
    mwrite8(mf, ocl->oc_oc1);
    mwrite8(mf, ocl->oc_oc2);

    /* as long as we use only one version of Hack we need not save oc_name and
       oc_descr, but we must save oc_uname for all objects */
    namelen = ocl->oc_uname ? strlen(ocl->oc_uname) + 1 : 0;
    mwrite32(mf, namelen);
    if (namelen)
        mwrite(mf, ocl->oc_uname, namelen);
}


void
savenames(struct memfile *mf)
{
    int i;

    mtag(mf, 0, MTAG_OCLASSES);
    mfmagic_set(mf, OCLASSES_MAGIC);

    for (i = 0; i < NUM_OBJECTS; i++)
        mwrite32(mf, disco[i]);

    for (i = 0; i < NUM_OBJECTS; i++)
        saveobjclass(mf, &objects[i]);
}


void
freenames(void)
{
    int i;

    for (i = 0; i < NUM_OBJECTS; i++)
        if (objects[i].oc_uname) {
            free(objects[i].oc_uname);
            objects[i].oc_uname = NULL;
        }
}


static void
restobjclass(struct memfile *mf, struct objclass *ocl)
{
    int namelen;
    unsigned int oflags;

    oflags = mread32(mf);
    ocl->oc_name_known = (oflags >> 31) & 1;
    ocl->oc_merge = (oflags >> 30) & 1;
    ocl->oc_uses_known = (oflags >> 29) & 1;
    ocl->oc_pre_discovered = (oflags >> 28) & 1;
    ocl->oc_magic = (oflags >> 27) & 1;
    ocl->oc_charged = (oflags >> 26) & 1;
    ocl->oc_unique = (oflags >> 25) & 1;
    ocl->oc_nowish = (oflags >> 24) & 1;
    ocl->oc_big = (oflags >> 23) & 1;
    ocl->oc_tough = (oflags >> 22) & 1;
    ocl->oc_dir = (oflags >> 20) & 3;
    ocl->oc_material = (oflags >> 15) & 31;
    ocl->oc_disclose_id = (oflags >> 14) & 1;

    ocl->oc_name_idx = mread16(mf);
    ocl->oc_descr_idx = mread16(mf);
    ocl->oc_weight = mread16(mf);
    ocl->oc_prob = mread16(mf);
    ocl->oc_cost = mread16(mf);
    ocl->oc_nutrition = mread16(mf);

    ocl->oc_subtyp = mread8(mf);
    ocl->oc_oprop = mread8(mf);
    ocl->oc_class = mread8(mf);
    ocl->oc_delay = mread8(mf);
    ocl->oc_color = mread8(mf);
    ocl->oc_wsdam = mread8(mf);
    ocl->oc_wldam = mread8(mf);
    ocl->oc_oc1 = mread8(mf);
    ocl->oc_oc2 = mread8(mf);

    ocl->oc_uname = NULL;
    namelen = mread32(mf);
    if (namelen) {
        ocl->oc_uname = malloc(namelen);
        mread(mf, ocl->oc_uname, namelen);
    }

}


void
restnames(struct memfile *mf)
{
    int i;

    mfmagic_check(mf, OCLASSES_MAGIC);

    for (i = 0; i < NUM_OBJECTS; i++)
        disco[i] = mread32(mf);

    for (i = 0; i < NUM_OBJECTS; i++)
        restobjclass(mf, &objects[i]);
}


void
discover_object(int oindx, boolean mark_as_known, boolean credit_hero,
                boolean disclose_only)
{
    if (!objects[oindx].oc_name_known) {
        int dindx, acls = objects[oindx].oc_class;

        /* Loop thru disco[] 'til we find the target (which may have been
           uname'd) or the next open slot; one or the other will be found
           before we reach the next class... */
        for (dindx = bases[acls]; disco[dindx] != 0; dindx++)
            if (disco[dindx] == oindx)
                break;
        disco[dindx] = oindx;

        if (mark_as_known) {
            objects[oindx].oc_name_known = 1;
            if (disclose_only)
                objects[oindx].oc_disclose_id = 1;
            if (credit_hero)
                exercise(A_WIS, TRUE);
        }
        if (moves > 1L)
            update_inventory();
    }
}

/* if a class name has been cleared, we may need to purge it from disco[] */
void
undiscover_object(int oindx)
{
    if (!objects[oindx].oc_name_known) {
        int dindx, acls = objects[oindx].oc_class;
        boolean found = FALSE;

        /* find the object; shift those behind it forward one slot */
        for (dindx = bases[acls];
             dindx < NUM_OBJECTS && disco[dindx] != 0 &&
             objects[dindx].oc_class == acls; dindx++)
            if (found)
                disco[dindx - 1] = disco[dindx];
            else if (disco[dindx] == oindx)
                found = TRUE;

        /* clear last slot */
        if (found)
            disco[dindx - 1] = 0;
        else
            impossible("named object not in disco");
        update_inventory();
    }
}

static boolean
interesting_to_discover(int i)
{
    /* Pre-discovered objects are now printed with a '*' */
    return ((boolean)
            (objects[i].oc_uname != NULL ||
             (objects[i].oc_name_known && OBJ_DESCR(objects[i]) != NULL)));
}

/* items that should stand out once they're known */
static const short uniq_objs[] = {
    AMULET_OF_YENDOR,
    SPE_BOOK_OF_THE_DEAD,
    CANDELABRUM_OF_INVOCATION,
    BELL_OF_OPENING,
};

int
dodiscovered(void)
{
    int i, dis;
    int ct = 0;
    char *s, oclass, prev_class, classes[MAXOCLASSES];
    struct menulist menu;
    char buf[BUFSZ];

    init_menulist(&menu);
    add_menutext(&menu, "Discoveries");
    add_menutext(&menu, "");

    /* gather "unique objects" into a pseudo-class; note that they'll also be
       displayed individually within their regular class */
    for (i = dis = 0; i < SIZE(uniq_objs); i++)
        if (objects[uniq_objs[i]].oc_name_known) {
            if (!dis++)
                add_menuheading(&menu, "Unique Items");
            sprintf(buf, "  %s", OBJ_NAME(objects[uniq_objs[i]]));
            add_menutext(&menu, buf);
            ++ct;
        }
    /* display any known artifacts as another pseudo-class */
    ct += disp_artifact_discoveries(&menu);

    /* several classes are omitted from packorder; one is of interest here */
    strcpy(classes, flags.inv_order);
    if (!strchr(classes, VENOM_CLASS)) {
        s = eos(classes);
        *s++ = VENOM_CLASS;
        *s = '\0';
    }

    for (s = classes; *s; s++) {
        oclass = *s;
        prev_class = oclass + 1;        /* forced different from oclass */
        for (i = bases[(int)oclass];
             i < NUM_OBJECTS && objects[i].oc_class == oclass; i++) {
            if ((dis = disco[i]) && interesting_to_discover(dis)) {
                ct++;
                if (oclass != prev_class) {
                    add_menuheading(&menu, let_to_name(oclass, FALSE));
                    prev_class = oclass;
                }
                sprintf(buf, "%s %s",
                        (objects[dis].oc_pre_discovered ? "*" : " "),
                        obj_typename(dis));
                add_menutext(&menu, buf);
            }
        }
    }
    if (ct == 0) {
        pline("You haven't discovered anything yet...");
    } else
        display_menu(menu.items, menu.icount, NULL, PICK_NONE, PLHINT_ANYWHERE,
                     NULL);
    free(menu.items);

    return 0;
}

/* Returns the number of objects discovered but not prediscovered. The
   objects must be formally IDed; merely naming doesn't count. Returns
   actual and maximum possible values. */
void
count_discovered_objects(int *curp, int *maxp)
{
    int i;

    *maxp = 0;
    *curp = 0;
    for (i = 0; i < NUM_OBJECTS; i++) {
        if (objects[i].oc_pre_discovered)
            continue;
        if (OBJ_DESCR(objects[i]) == (char *)0)
            continue;
        (*maxp)++;
        if (!objects[i].oc_name_known)
            continue;
        if (objects[i].oc_disclose_id)
            continue;   /* identified in DYWYPI */
        (*curp)++;
    }
}

/*o_init.c*/
