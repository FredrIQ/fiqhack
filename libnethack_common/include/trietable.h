/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-22 */
/* Copyright (c) Alex Smith, 2015. */
/* NetHack may be freely redistributed.  See license for details. */

#include "compilers.h"

#ifndef TRIETABLE_H
# define TRIETABLE_H

/*
 * A trietable associates positive numeric keys with (arbitrary) values. The
 * values are here expressed via a void * pointer; the trietable does /not/ own
 * the values pointed to via these pointers.
 *
 * The trietable itself is a mutable struct trietable * (i.e. you have to pass
 * around a pointer to it to the trietable functions). An empty trietable is
 * a NULL pointer. Trietables have internal state that's allocated via malloc;
 * however, after emptying one, it goes back to being a NULL pointer with no
 * allocated internal state.
 *
 * The actual implementation is recursive:
 *
 * - An empty trietable is a NULL pointer;
 * - A trietable containing one element has NULL ptr0 and ptr1, and stores that
 *   element in key and value;
 * - A trietable containing more than one element contains:
 *   - the element with key 0 (if any) in key and value;
 *   - a trietable of all the elements with even keys in ptr0 (with those keys
 *     halved);
 *   - a trietable of all the elements with odd keys in ptr1 (with those keys
 *     rounded down and halved).
 *
 * Note that this wold technically work even without the second case here,
 * which would mean we wouldn't need a key field. However, it would make tables
 * that were empty (or nearly so) a lot larger. (If key is not 0, that implies
 * that the table contains only one element. If key is 0, the table might still
 * only contain one element, in which case that element's key is 0.)
 */
struct trietable {
    struct trietable *ptr0;      /* pointer for a 0 bit */
    struct trietable *ptr1;      /* pointer for a 1 bit */
    void *value;                 /* value at this location */
    unsigned key;                /* key at this location */
};

/* The operations NetHack 4 actually uses: add, find, empty. Delete can be
   implemented in this data structure, but there hasn't been a need for it
   yet. Adding a key that already exists to a trietable will overwrite whatever
   is already there. */
extern void trietable_add(struct trietable **table, unsigned key, void *value);
extern void *trietable_find(struct trietable **table, unsigned key);
extern void trietable_empty(struct trietable **table);

#endif
