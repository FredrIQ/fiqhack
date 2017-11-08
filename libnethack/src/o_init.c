/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-11-08 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"        /* save & restore info */

static void shuffle(int, int, boolean);
static void shuffle_all(void);
static boolean interesting_to_discover(int);

/* Returns TRUE if the object is new */
boolean
is_new_object(int otyp)
{
    if (flags.save_revision == SAVE_REVISION)
        return FALSE;

    if (otyp == 290 /* POT_WONDER */ && flags.save_revision < 4)
        return TRUE;
    return FALSE;
}

int
otyp_offset(int otyp)
{
    if (flags.save_revision == SAVE_REVISION)
        return 0;

    int ret = 0;
    if (otyp >= 290 /* POT_WONDER */ && flags.save_revision < 4)
        ret++;
    return ret;
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
        /* sanity check */
        if (!has_shuffled_appearance(j))
            impossible("shuffle() called on nonshuffleable otyp %d", j);

        if (objects[j].oc_name_known)
            continue;
        do
            i = j + rn2_on_rng(o_high - j + 1, rng_dungeon_gen);
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
    int i, first, last;
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
            if (rn2_on_rng(2, rng_dungeon_gen)) {
                /* change turquoise from green to blue? */
                COPY_OBJ_DESCR(objects[TURQUOISE], objects[SAPPHIRE]);
            }
            if (rn2_on_rng(2, rng_dungeon_gen)) {
                /* change aquamarine from green to blue? */
                COPY_OBJ_DESCR(objects[AQUAMARINE], objects[SAPPHIRE]);
            }
            switch (rn2_on_rng(4, rng_dungeon_gen)) {
                /* change fluorite from violet? */
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
            oclass != GEM_CLASS && oclass != VENOM_CLASS) {
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

/* Returns TRUE if the unidentified appearance of this object could differ from
   game to game. */
boolean
has_shuffled_appearance(int otyp)
{
    int oclass = objects[otyp].oc_class;

    /* If an object doesn't even have an unidentified appearance, it definitely
       doesn't have a shuffled appearance. */
    if (OBJ_DESCR(objects[otyp]) == NULL)
        return FALSE;

    /* Most object classes are almost entirely shuffled. */
    if (oclass == POTION_CLASS || oclass == WAND_CLASS ||
        oclass == AMULET_CLASS || oclass == GEM_CLASS ||
        oclass == SPBOOK_CLASS || oclass == RING_CLASS ||
        oclass == SCROLL_CLASS)
        return (oclass == WAND_CLASS || oclass == POTION_CLASS ||
                objects[otyp].oc_magic) &&
            !objects[otyp].oc_unique && otyp != POT_WATER && otyp != ROCK;

    /* Armour is partially shuffled. */
    if ((HELMET <= otyp && otyp <= HELM_OF_TELEPATHY) ||
        (LEATHER_GLOVES <= otyp && otyp <= GAUNTLETS_OF_DEXTERITY) ||
        (CLOAK_OF_PROTECTION <= otyp && otyp <= CLOAK_OF_DISPLACEMENT) ||
        (SPEED_BOOTS <= otyp && otyp <= LEVITATION_BOOTS))
        return TRUE;

    /* Other items are not shuffled. */
    return FALSE;
}

/* Returns TRUE if the given otyp has a "corpsenm" field that could potentially
   be unidentified. */
boolean
corpsenm_is_relevant(int otyp)
{
    return (otyp == CORPSE || otyp == STATUE || otyp == TIN ||
            otyp == FIGURINE || otyp == EGG);
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

/* Does a few dummy writes for things that doesn't change between games.
   The reason that data is written at all has to do with earlier versions where that
   data was written to the save, which causes issues when objects are modified. */
static void
saveobjclass(struct memfile *mf, struct objclass *ocl)
{
    int namelen = 0;
    unsigned int oflags;

    /* no mtag useful; object classes are always saved in the same order and
       there are always the same number of them */
    oflags =
        (ocl->oc_name_known << 31) | (ocl->oc_pre_discovered << 28) |
        (ocl->oc_tough << 22) | (ocl->oc_material << 15);
    mwrite32(mf, oflags);
    mwrite16(mf, ocl->oc_name_idx);
    mwrite16(mf, ocl->oc_descr_idx);
    mwrite64(mf, 0);
    mwrite32(mf, 0);

    mwrite8(mf, ocl->oc_color);

    mwrite32(mf, 0);

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

    mfmagic_set(mf, OCLASSES_MAGIC);
    mtag(mf, 0, MTAG_OCLASSES);

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
add_new_object(int oindx)
{
    /* object descriptions */
    objects[oindx].oc_name_idx = objects[oindx].oc_descr_idx = oindx;
    objects[oindx].oc_uname = NULL;
}

/* Does a few dummy reads for things that don't change between games (but can change
   between versions) */
static void
restobjclass(struct memfile *mf, struct objclass *ocl)
{
    int namelen;
    unsigned int oflags;

    oflags = mread32(mf);
    ocl->oc_name_known = (oflags >> 31) & 1;
    ocl->oc_pre_discovered = (oflags >> 28) & 1;
    ocl->oc_tough = (oflags >> 22) & 1;
    ocl->oc_material = (oflags >> 15) & 31;

    ocl->oc_name_idx = mread16(mf);
    ocl->oc_descr_idx = mread16(mf);

    ocl->oc_name_idx += otyp_offset(ocl->oc_name_idx);
    ocl->oc_descr_idx += otyp_offset(ocl->oc_descr_idx);

    mread64(mf);
    mread32(mf);
    
    ocl->oc_color = mread8(mf);

    mread32(mf);

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
    int i, j;

    mfmagic_check(mf, OCLASSES_MAGIC);

    /* The disco[] array is rather oddly constructed; it iterates sequentially
       independently of object type generally, but for whatever reason remains
       within the same object class space. So if we add new objects, we need to
       figure out where those objects were added, and if there was any newly
       added objects within given object class, insert zeroes at the end of said
       object class area. */
    int new_disco = 0;
    int oclass = 0;

    j = 0;
    for (i = 0; i < NUM_OBJECTS; i++) {
        if (oclass != objects[i].oc_class) {
            oclass = objects[i].oc_class;
            while (new_disco > 0) {
                new_disco--;
                disco[j++] = 0;
            }
        }

        if (is_new_object(i))
            new_disco++;
        else {
            disco[j] = mread32(mf);
            disco[j] += otyp_offset(disco[j]);
            j++;
        }
    }

    while (new_disco > 0) {
        new_disco--;
        disco[j++] = 0;
    }

    for (i = 0; i < NUM_OBJECTS; i++)
        if (!is_new_object(i))
            restobjclass(mf, &objects[i]);
        else
            add_new_object(i);
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
dodiscovered(const struct nh_cmd_arg *arg)
{
    (void) arg;

    int i, dis;
    int ct = 0;
    char *s, oclass, prev_class, classes[MAXOCLASSES];
    struct nh_menulist menu;

    init_menulist(&menu);
    add_menutext(&menu, "Discoveries");
    add_menutext(&menu, "");

    /* gather "unique objects" into a pseudo-class; note that they'll also be
       displayed individually within their regular class */
    for (i = dis = 0; i < SIZE(uniq_objs); i++)
        if (objects[uniq_objs[i]].oc_name_known) {
            if (!dis++)
                add_menuheading(&menu, "Unique Items");
            add_menutext(
                &menu, msgprintf("  %s", OBJ_NAME(objects[uniq_objs[i]])));
            ++ct;
        }
    /* display any known artifacts as another pseudo-class */
    ct += disp_artifact_discoveries(&menu);

    strcpy(classes, flags.inv_order);

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
                add_menutext(
                    &menu, msgprintf(
                        "%s %s",
                        (objects[dis].oc_pre_discovered ? "*" : " "),
                        obj_typename(dis)));
            }
        }
    }

    if (ct == 0) {
        pline(msgc_info, "You haven't discovered anything yet...");
        dealloc_menulist(&menu);
    } else
        display_menu(&menu, NULL, PICK_NONE, PLHINT_ANYWHERE, NULL);

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

