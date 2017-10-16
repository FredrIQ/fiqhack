/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-16 */
/* Copyright (c) Fredrik Ljungdahl, 2017. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static struct obj *find_objects(struct level *, struct obj *, int *, boolean *,
                                const char *, struct nh_menulist *, int);
static struct obj *findobj_prompt(int, const char *);

/* Handles object memories to allow players to search for objects they remember
   seeing. */

static struct obj *
find_objects(struct level *lev, struct obj *chain, int *found,
             boolean *did_header, const char *str,
             struct nh_menulist *menu, int getobj)
{
    struct obj *obj, *objfound, *upper;
    const char *dname;
    for (obj = chain; obj; obj = obj->nobj) {
        dname = distant_name(obj, doname);
        if (obj->where == OBJ_CONTAINED) {
            /* See if any upper container was lost. */
            upper = obj;
            while (upper->memory != OM_MEMORY_LOST &&
                   upper->where == OBJ_CONTAINED)
                upper = upper->ocontainer;
            if (upper->memory == OM_MEMORY_LOST)
                continue;

            dname = msgprintf("%s (inside %s)", dname,
                              distant_name(upper, doname));
        }

        if (!strstri(dname, str) || obj->memory == OM_MEMORY_LOST) {
            if (Has_contents(obj)) {
                objfound = find_objects(lev, obj->cobj, found,
                                        did_header, str, menu,
                                        getobj);
                if (objfound)
                    return objfound;
            }
            continue;
        }
        /* We have a match. */

        /* If we didn't find anything beforehand, inititalize
           the window now. */
        if (!*found && !getobj) {
            init_menulist(menu);
            const char *buf = msgprintf("Searching for \"%s\".",
                                        str);
            add_menutext(menu, buf);
        }
        (*found)++;
        if (getobj) {
            if (*found == getobj)
                return obj;
        } else {
            /* If this is our first object in level or inventory,
               give a header. */
            if (!*did_header) {
                *did_header = TRUE;
                add_menutext(menu, "");
                const char *header = "Inventory";
                if (lev)
                    header = describe_dungeon_level(lev);

                add_menuheading(menu, header);
            }

            add_menuitem(menu, *found, dname, 0, FALSE);
        }

        if (Has_contents(obj)) {
            objfound = find_objects(lev, obj->cobj, found,
                                    did_header, str, menu, getobj);
            if (objfound)
                return objfound;
        }
    }

    return NULL;
}

static struct obj *
findobj_prompt(int getobj, const char *str)
{
    /* Find objects in levels */
    const char *buf;
    struct nh_menulist menu;
    struct obj *objfound = NULL;
    int found = 0;
    boolean did_header;
    int i;
    for (i = 0; i <= maxledgerno(); i++) {
        if (levels[i]) {
            did_header = FALSE;
            objfound = find_objects(levels[i], levels[i]->memobjlist,
                                    &found, &did_header, str, &menu,
                                    getobj);
            if (objfound)
                return objfound;
        }
    }

    /* Find objects in player inventory. Useful in replaymode
       (containers) */
    did_header = FALSE;
    objfound = find_objects(NULL, youmonst.meminvent, &found,
                            &did_header, str, &menu, getobj);
    if (objfound)
        return objfound;

    if (getobj)
        return NULL;

    if (!found) {
        pline(msgc_actionok, "Failed to find any objects.");
        return 0;
    }

    buf = msgprintf("Found %d object%s%s", found,
                    found == 1 ? "" : "s",
                    found >= 200 ? " (please be more specific)" :
                    "");

    const int *selected;
    int n;
    n = display_menu(&menu, buf, PICK_ONE, PLHINT_ANYWHERE, &selected);
    if (n <= 0)
        return NULL;
    return findobj_prompt(selected[0], str);
}

int
dofindobj(const struct nh_cmd_arg *arg)
{
    const char *buf;
    buf = getlin("Search for what objects?", FALSE);
    if (!*buf || *buf == '\033')
        return 0;

    struct obj *obj = findobj_prompt(0, buf);
    struct obj *upper = obj; /* Points to uppermost container, or obj */
    if (obj) {
        while (upper->where == OBJ_CONTAINED)
            upper = upper->ocontainer;
        if (upper->where == OBJ_INVENT) {
            if (obj == upper)
                pline(msgc_cancelled, "Look in your inventory!");
            else
                pline(msgc_actionok, "It's in %s %s.",
                      shk_your(upper), cxname(upper));
            return 0;
        } else if (upper->where != OBJ_FLOOR) {
            impossible("Where did %s go? %d",
                       killer_xname(upper), upper->where);
            return 0;
        }

        struct level *lev = upper->olev;
        if (!lev) {
            impossible("dofindobj: target obj olev is NULL "
                       "but on the floor?");
            return 0;
        }

        /* Display target level */
        if (lev != level) {
            int x, y;
            for (x = 0; x < COLNO; x++)
                for (y = 0; y < ROWNO; y++)
                    dbuf_set_memory(lev, x, y);

            pline(msgc_actionok, "Viewing %s.",
                  describe_dungeon_level(lev));
            notify_levelchange(&lev->z);
            flush_screen_nopos();
            look_at_map(upper->ox, upper->oy);
        }

        /* Display object position */
        cls();
        dbuf_set_memory(lev, upper->ox, upper->oy);
        const char *dname = The(distant_name(obj, cxname));
        const char *cdname = distant_name(upper, doname);
        pline(msgc_actionok, "%s %s located here%s.", dname,
              vtense(dname, "are"), upper == obj ? "" :
              msgcat_many(", inside ", cdname, NULL));
        flush_screen_nopos();
        look_at_map(upper->ox, upper->oy);
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
        next = memobj->nexthere;
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
        update_obj_memory(obj);
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
        update_obj_memory(obj);
}

/* Updates container memory */
void
update_container_memory(struct obj *obj)
{
    if (!obj) {
        impossible("update_container_memory: container is NULL?");
        return;
    }

    /* Update container itself first. */
    update_obj_memory(obj);

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
        update_obj_memory(obj);
}

/* Creates or updates an object memory for given object */
void
update_obj_memory(struct obj *obj)
{
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
    turnstate.floating_objects = memobj->nobj;
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
    memobj->nobj = NULL;
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

    /* Add to memory chain */
    switch (obj->where) {
    case OBJ_FREE:
        panic("update_obj_memory: updating memory for a floating object?");
        /* NOTREACHED */ break;
    case OBJ_FLOOR:
        extract_nobj(memobj, &turnstate.floating_objects,
                     &memobj->olev->memobjlist, OBJ_FLOOR);
        memobj->nexthere = lev->memobjects[memobj->ox][memobj->oy];
        lev->memobjects[memobj->ox][memobj->oy] = memobj;
        break;
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
