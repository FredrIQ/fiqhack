/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2018-12-14 */
/* Copyright (c) Fredrik Ljungdahl, 2017. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Handles map memory of objects and traps to allow players to search for
   objects they remember seeing. */

static struct entity find_objects(struct level *, struct obj *, int *,
                                  boolean *, const char *,
                                  struct nh_menulist *, int, int);
static struct entity findobj_prompt(int, const char *);

/* A generic entity struct for merging objects, traps and dungeon features
   into a single data type for handling the various possible kinds of
   things you can find. */
enum ent_typ {
    ENT_NONE,
    ENT_OBJ,
    ENT_TRAP,
};

struct entity {
    enum ent_typ typ;
    union {
        struct obj *obj;
        struct trap *trap;
    };

    struct level *lev; /* NULL means part of your inventory */
    int x;
    int y;
    int where;
};

/* Returns an object or trap in an entity form. */
struct entity
get_entity(struct level *lev, struct obj *obj, struct trap *trap)
{
    struct entity ent;
    ent.lev = lev;

    if (!obj) {
        ent.typ = ENT_TRAP;
        ent.trap = trap;
        ent.x = trap->tx;
        ent.y = trap->ty;
    } else {
        ent.typ = ENT_OBJ;
        ent.obj = obj;
        ent.x = obj->ox;
        ent.y = obj->oy;
        ent.where = obj->where;
    }

    return ent;
}

/* Returns the name of an entity */
static const char *
entity_name(struct entity ent)
{
    struct level *lev = ent.lev;
    struct obj *obj;
    struct obj *upper;
    const char *dname;

    if (ent.typ == ENT_TRAP)
        return trapexplain[ent.trap->ttyp - 1];

    obj = ent.obj;
    dname = distant_name(obj, doname);

    if (obj->where == OBJ_CONTAINED) {
        upper = obj;
        while (upper && upper->memory != OM_MEMORY_LOST &&
               upper->where == OBJ_CONTAINED)
            upper = upper->ocontainer;
        if (upper && upper->memory == OM_MEMORY_LOST)
            return NULL;

        if (upper)
            dname = msgprintf("%s (inside %s)", dname,
                              doname(upper));
    }

    return dname;
}

/* Adds an entity to the search result menu */
void
add_entry(struct level *lev, int *found, boolean *did_header,
          struct nh_menulist *menu, int depth, int getobj,
          const char *str, struct entity ent)
{
    /* If we didn't find anything beforehand, inititalize
       the window now. */
    if (getobj) {
        (*found)++;
        return;
    }

    if (!*found) {
        init_menulist(menu);
        const char *buf = msgprintf("Searching for \"%s\".",
                                    str);
        add_menutext(menu, buf);
    }
    (*found)++;

    /* If this is our first object/trap in level or inventory,
       give a header. */
    if (!*did_header) {
        *did_header = TRUE;
        add_menutext(menu, "");
        const char *header;
        if (ent.typ == ENT_OBJ && ent.where == OBJ_INVENT)
            header = "Inventory";
        else
            header = "Magic chest";
        if (lev)
            header = describe_dungeon_level(lev);

        add_menuheading(menu, header);
    }

    const char *depthstr = "";
    while (depth--)
        depthstr = msgcat(depthstr, "  ");

    add_menuitem(menu, *found, msgcat(depthstr, entity_name(ent)), 0, FALSE);
}

static struct entity
find_objects(struct level *lev, struct obj *chain, int *found,
             boolean *did_header, const char *str,
             struct nh_menulist *menu, int depth, int getobj)
{
    struct obj *obj, *upper;
    struct entity ent = {0};
    const char *dname;
    int oclass = ALL_CLASSES;

    if (strlen(str) == 1)
        oclass = def_char_to_objclass(*str);
    if (oclass == MAXOCLASSES)
        oclass = ALL_CLASSES;

    for (obj = chain; obj; obj = obj->nobj) {
        ent = get_entity(lev, obj, NULL);
        dname = entity_name(ent);
        if (!dname)
            continue; /* inside a lost container */

        if (obj->memory == OM_MEMORY_LOST ||
            ((oclass == ALL_CLASSES && !strstri(dname, str)) ||
             (oclass != ALL_CLASSES && obj->oclass != oclass))) {
            if (Has_contents(obj) && obj->otyp != MAGIC_CHEST) {
                ent = find_objects(lev, obj->cobj, found, did_header,
                                   str, menu, depth + 1, getobj);
                if (ent.typ != ENT_NONE)
                    return ent;
            }
            continue;
        }
        /* We have a match. */

        add_entry(lev, found, did_header, menu, depth, getobj, str, ent);

        if (getobj && *found == getobj)
            return ent;

        if (Has_contents(obj) && obj->otyp != MAGIC_CHEST) {
            ent = find_objects(lev, obj->cobj, found, did_header,
                               str, menu, depth + 1, getobj);
            if (ent.typ != ENT_NONE)
                return ent;
        }
    }

    if (lev) {
        struct trap *trap;
        for (trap = lev->lev_traps; trap; trap = trap->ntrap) {
            ent = get_entity(lev, NULL, trap);
            if (!trap->tseen)
                continue;

            dname = entity_name(ent);

            if (!strstri(dname, str))
                continue;

            add_entry(lev, found, did_header, menu, depth, getobj, str, ent);

            if (getobj && *found == getobj)
                return ent;
        }
    }

    ent.typ = ENT_NONE;
    return ent;
}

static struct entity
findobj_prompt(int getobj, const char *str)
{
    /* Find objects in levels */
    const char *buf;
    struct nh_menulist menu;
    struct entity ent = {0};
    int found = 0;
    boolean did_header;
    int i;
    for (i = 0; i <= maxledgerno(); i++) {
        if (levels[i]) {
            did_header = FALSE;
            ent = find_objects(levels[i], levels[i]->memobjlist,
                               &found, &did_header, str, &menu,
                               0, getobj);
            if (ent.typ != ENT_NONE)
                return ent;
        }
    }

    /* Find objects in player inventory. Useful in replaymode
       (containers) */
    did_header = FALSE;
    if (ent.typ == ENT_NONE)
        ent = find_objects(NULL, youmonst.meminvent, &found,
                           &did_header, str, &menu, 0, getobj);

    /* Find objects in magic chests */
    did_header = FALSE;
    if (ent.typ == ENT_NONE)
        ent = find_objects(NULL, gamestate.chest, &found,
                           &did_header, str, &menu, 0, getobj);

    if (getobj)
        return ent;

    if (!found) {
        pline(msgc_actionok, "Failed to find any objects.");
        ent.typ = ENT_NONE;
        return ent;
    }

    buf = msgprintf("Found %d object%s", found,
                    found == 1 ? "" : "s");

    const int *selected;
    int n;
    n = display_menu(&menu, buf, PICK_ONE, PLHINT_ANYWHERE, &selected);
    if (n <= 0) {
        ent.typ = ENT_NONE;
        return ent;
    }
    return findobj_prompt(selected[0], str);
}

int
dofindobj(const struct nh_cmd_arg *arg)
{
    const char *buf;
    buf = getlin("Search for what objects, traps or glyph?", FALSE);
    if (!*buf || *buf == '\033')
        return 0;

    struct entity ent = findobj_prompt(0, buf);
    if (ent.typ != ENT_NONE) {
        struct obj *obj = NULL;
        if (ent.typ == ENT_OBJ)
            obj = ent.obj;
        struct obj *upper = obj; /* Points to uppermost container, or obj */
        while (upper && upper->where == OBJ_CONTAINED)
            upper = upper->ocontainer;
        if (upper && upper->where == OBJ_INVENT) {
            if (obj == upper)
                pline(msgc_cancelled, "Look in your inventory!");
            else
                pline(msgc_actionok, "It's in %s %s.",
                      shk_your(upper), cxname(upper));
            return 0;
        } else if (upper && upper->where != OBJ_FLOOR) {
            impossible("Where did %s go? %d",
                       killer_xname(upper), upper->where);
            return 0;
        } else if (obj->where == OBJ_CONTAINED) {
            pline(msgc_cancelled, "It's inside any magic chest.");
            return 0;
        }

        if (upper) {
            ent.lev = upper->olev;
            ent.x = upper->ox;
            ent.y = upper->oy;
        }

        struct level *lev = ent.lev;
        if (!lev) {
            impossible("dofindobj: target lev is NULL");
            return 0;
        }

        /* Display target level */
        struct level *old_level = level;
        if (lev != level) {
            level = lev;
            int x, y;
            for (x = 0; x < COLNO; x++)
                for (y = 0; y < ROWNO; y++)
                    dbuf_set_memory(lev, x, y);

            pline(msgc_actionok, "Viewing %s.",
                  describe_dungeon_level(lev));
            notify_levelchange(&lev->z);
            flush_screen_nopos();
            look_at_map(ent.x, ent.y);
            level = old_level;
        }

        /* Display object position */
        cls();
        level = lev;
        dbuf_set_memory(lev, ent.x, ent.y);
        const char *dname = The(entity_name(ent));
        const char *cdname = "";
        if (upper)
            cdname = distant_name(upper, doname);
        pline(msgc_actionok, "%s %s located here%s.", dname,
              vtense(dname, "are"), upper == obj ? "" :
              msgcat_many(", inside ", cdname, NULL));
        flush_screen_nopos();
        look_at_map(ent.x, ent.y);
        level = old_level;
        notify_levelchange(NULL);
        doredraw();
    }
    return 0;
}

void
show_obj_memories_at(struct level *lev, int x, int y)
{
    struct obj *memobj;
    struct nh_objlist objlist;
    const char *dfeature = NULL;
    const char *fbuf;

    if (!lev->memobjects[x][y])
        return;

    if (lev == level)
        dfeature = dfeature_at(x, y);

    if (dfeature && !strcmp(dfeature, "pool of water") && Underwater)
        dfeature = NULL;

    init_objmenulist(&objlist);

    if (dfeature) {
        fbuf = msgprintf("There is %s here.", an(dfeature));
        add_objitem(&objlist, MI_TEXT, 0, fbuf, NULL, FALSE);
        add_objitem(&objlist, MI_TEXT, 0, "", NULL, FALSE);
    }

    for (memobj = lev->memobjects[x][y]; memobj; memobj = memobj->nexthere)
        add_objitem(&objlist, MI_NORMAL, 0, distant_name(memobj, doname), memobj, FALSE);

    display_objects(&objlist, "Remembered objects", PICK_NONE, PLHINT_CONTAINER, NULL);
}

void
update_obj_memories(struct level *lev)
{
    /* Update level */
    int x, y;
    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++)
            if (lev == level && cansee(x, y))
                update_obj_memories_at(lev, x, y);

    /* First, mark memories as lost, or free them. */
    struct obj *obj, *memobj, *next;
    for (memobj = youmonst.meminvent; memobj; memobj = next) {
        next = memobj->nobj;
        obj = memobj->mem_obj;
        if (!obj) {
            /* Apparently the object disappeared, deallocate it. */
            free_obj_memory(memobj);
            continue;
        }

        memobj->memory = OM_MEMORY_LOST;
    }

    /* Now set up up to date memories of objects inside. */
    for (obj = youmonst.minvent; obj; obj = obj->nobj)
        update_obj_memory(obj, NULL);
}

/* Refreshes object memories at location. Assumes the player can know
   whatever is here. */
void
update_obj_memories_at(struct level *lev, int x, int y)
{
    /* Screen redraws end up calling this. We don't want to
       change the gamestate in this case, so check for it. */
    if (program_state.in_zero_time_command)
        return;

    /* Maybe we can't see underwater/lava... In that case, leave memories
       untouched (don't lose memories either) so that the hero can still
       remember them if object detection or similar was used. */
    if (is_lava(lev, x, y) ||
        (is_pool(lev, x, y) && !Underwater))
        return;

    struct obj *obj, *memobj, *next;

    for (memobj = lev->memobjects[x][y]; memobj; memobj = next) {
        next = memobj->nexthere;
        obj = memobj->mem_obj;
        if (!obj) {
            free_obj_memory(memobj);
            continue;
        }

        memobj->memory = OM_MEMORY_LOST;
    }

    for (obj = lev->objects[x][y]; obj; obj = obj->nexthere)
        update_obj_memory(obj, NULL);

    /* If there is a mimic on the tile, create a fake object memory. */
    struct monst *mon = m_at(lev, x, y);
    if (!mon || mon->m_ap_type != M_AP_OBJECT)
        return;
    if (m_helpless(mon, 1 << hr_mimicking) &&
        (lev != level || !Protection_from_shape_changers) &&
        (msensem(&youmonst, mon) & MSENSE_ITEMMIMIC))
        update_obj_memory(NULL, mon);
}

/* Updates container memory */
void
update_container_memory(struct obj *obj)
{
    if (!obj) {
        impossible("update_container_memory: container is NULL?");
        return;
    }

    /* Mark container as investigated */
    obj->cknown = TRUE;

    /* Update container itself first. */
    update_obj_memory(obj, NULL);

    struct obj *memobj = obj->mem_obj;
    struct obj *next;

    for (memobj = memobj->cobj; memobj; memobj = next) {
        next = memobj->nobj;
        if (!memobj->mem_obj) {
            free_obj_memory(memobj);
            continue;
        }

        memobj->memory = OM_MEMORY_LOST;
    }

    for (obj = obj->cobj; obj; obj = obj->nobj)
        update_obj_memory(obj, NULL);
}

/* Returns amount of remembered objects or -1 if not remembered/not a container */
int
remembered_contained(const struct obj *obj)
{
    if (!obj)
        panic("remembered_contained: obj is NULL");

    boolean magic_chest = FALSE;
    if (magic_chest(obj))
        magic_chest = TRUE;

    if (!obj->cknown && !magic_chest)
        return -1;

    /* If we're creating bones, memories has already
       been freed before cknown has. */
    if (program_state.gameover)
        return -1;

    const struct obj *memobj = obj;
    if (memobj->memory == OM_NO_MEMORY && !magic_chest)
        memobj = memobj->mem_obj;

    if (!memobj)
        panic("remembered_contained: obj->mem_obj is NULL");

    if (!Is_container(memobj) ||
        (memobj->otyp == BAG_OF_TRICKS && memobj->dknown &&
         objects[BAG_OF_TRICKS].oc_name_known))
        return -1;

    int ret = 0;
    struct obj *chain = memobj->cobj;
    if (magic_chest)
        chain = gamestate.chest;

    for (obj = chain; obj; obj = obj->nobj)
        ret++;
    return ret;
}

/* Creates or updates an object memory for given object, or for a mimic. */
void
update_obj_memory(struct obj *obj, struct monst *mon)
{
    if (!obj) {
        if (!mon) {
            impossible("update_obj_memory: both obj and mon is NULL?");
            return;
        }

        /* Create a pseudo-object based on a mimic's appearance. */
        obj = mktemp_sobj(m_dlevel(mon), mon->mappearance);
        obj->ox = m_mx(mon);
        obj->oy = m_my(mon);
        if (corpsenm_is_relevant(obj->otyp))
            obj->corpsenm = PM_TENGU; /* consistent with pager.c */
    } else if (obj && mon) {
        /* which one is it, object or monster? */
        impossible("update_obj_memory: both obj and mon is non-NULL?");
        return;
    }

    struct obj *memobj = obj->mem_obj;
    if (!memobj) {
        /* Create a new memory */
        memobj = newobj(obj);
        obj->mem_obj = memobj;
        memobj->mem_obj = obj;

        /* Assign a new ID */
        memobj->o_id = next_ident();

        /* Kill container information */
        memobj->cobj = NULL;
        memobj->memory = OM_MEMORY_OK;
    }
    struct level *lev = obj->olev;

    /* We are about to overwrite the content of the memory object with obj.
       Object ID and container content needs to be kept, though. */
    int o_id = memobj->o_id;
    struct obj *cobj = memobj->cobj;

    extract_obj_memory(memobj);
    extract_nobj(memobj, &turnstate.floating_objects, NULL, OBJ_FREE);
    ox_free(memobj);

    *memobj = *obj;
    memobj->mem_obj = obj;

    /* give error conditions a chance at freeing the memory properly */
    memobj->where = OBJ_FREE;
    memobj->nobj = turnstate.floating_objects;
    turnstate.floating_objects = memobj;

    /* Set up fields that are different */
    ox_copy(memobj, obj);
    memobj->o_id = o_id;
    memobj->cobj = cobj;
    memobj->memory = OM_MEMORY_OK;
    memobj->timed = 0;
    memobj->lamplit = 0;
    memobj->owornmask = 0;
    memobj->nexthere = NULL;

    /* just in case... */
    memobj->no_charge = 1;
    memobj->unpaid = 0;
    memobj->in_use = FALSE;

    /* If the object can't possibly contain anything, kill memory of its
       content. This can happen on polymorph. */
    if (!Is_container(memobj) ||
        (memobj->otyp == BAG_OF_TRICKS && memobj->dknown &&
         objects[BAG_OF_TRICKS].oc_name_known)) {
        free_memobj_chain(memobj->cobj);
        memobj->cobj = NULL;
    }

    if (obj->where == OBJ_FLOOR || mon) {
        extract_nobj(memobj, &turnstate.floating_objects,
                     &memobj->olev->memobjlist, OBJ_FLOOR);
        memobj->nexthere = lev->memobjects[memobj->ox][memobj->oy];
        lev->memobjects[memobj->ox][memobj->oy] = memobj;

        if (mon)
            obfree(obj, NULL); /* get rid of the pseudo-object */
        return;
    }

    switch (obj->where) {
    case OBJ_FREE:
        panic("update_obj_memory: updating memory for a floating object?");
        /* NOTREACHED */ break;
    case OBJ_CONTAINED:
        obj = obj->ocontainer->mem_obj;
        /* We must have a memory for the container, or something is wrong */
        if (!obj)
            panic("update_obj_memory: container has no memory?");

        memobj->ocontainer = obj;
        extract_nobj(memobj, &turnstate.floating_objects, &(obj->cobj),
                     OBJ_CONTAINED);
        break;
    case OBJ_INVENT:
        extract_nobj(memobj, &turnstate.floating_objects, &(youmonst.meminvent),
                     OBJ_INVENT);
        break;
    default:
        panic("update_obj_memory: unknown location %d", obj->where);
    }
}

/* Frees an object memory */
void
free_obj_memory(struct obj *memobj)
{
    if (memobj->mem_obj)
        memobj->mem_obj->mem_obj = NULL;

    if (Has_contents(memobj))
        free_memobj_chain(memobj->cobj);

    extract_obj_memory(memobj);
    dealloc_obj(memobj);
}

/* Frees an object memory chain */
void
free_memobj_chain(struct obj *chain)
{
    struct obj *memobj;
    struct obj *next;
    for (memobj = chain; memobj; memobj = next) {
        next = memobj->nobj;
        free_obj_memory(memobj);
    }
}

/* Frees object memories completely. */
void
free_memobj(void)
{
    /* Clear levels */
    int i;
    for (i = 0; i <= maxledgerno(); i++)
        if (levels[i])
            free_memobj_chain(levels[i]->memobjlist);

    /* Clear player inventory */
    free_memobj_chain(youmonst.meminvent);
}

/* Moves an object memory to the floating object chain.
   Equavilent to obj_extract_self for real objects. */
void
extract_obj_memory(struct obj *memobj)
{
    if (memobj->memory == OM_NO_MEMORY)
        panic("extract_obj_memory: memobj is not an object memorg.");

    switch (memobj->where) {
    case OBJ_FREE:
        break;
    case OBJ_FLOOR:
        extract_nexthere(memobj,
                         &memobj->olev->memobjects[memobj->ox][memobj->oy]);
        extract_nobj(memobj, &memobj->olev->memobjlist,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_CONTAINED:
        extract_nobj(memobj, &memobj->ocontainer->cobj,
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    case OBJ_INVENT:
        extract_nobj(memobj, &(youmonst.meminvent),
                     &turnstate.floating_objects, OBJ_FREE);
        break;
    default:
        panic("extract_obj_memory: unknown location %d", memobj->where);
    }
}

/*memobj.c*/
