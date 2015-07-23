/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-22 */
/* Copyright (c) Alex Smith, 2015. */
/* NetHack may be freely redistributed.  See license for details. */

#include "trietable.h"
#include <stdlib.h>

/* See trietable.h for information on what's going on here. */

void
trietable_add(struct trietable **table, unsigned key, void *value)
{
    if (*table == NULL) {
        *table = malloc(sizeof **table);
        (*table)->ptr0 = NULL;
        (*table)->ptr1 = NULL;
        (*table)->value = value;
        (*table)->key = key;
    } else if (key == 0) {
        unsigned oldkey = (*table)->key;
        void *oldvalue = (*table)->value;
        (*table)->key = 0;
        (*table)->value = value;
        if (oldkey != 0)
            trietable_add(table, oldkey, oldvalue);
    } else if (key & 1)
        trietable_add(&(*table)->ptr1, key >> 1, value);
    else
        trietable_add(&(*table)->ptr0, key >> 1, value);
}

/* Returns NULL on failure to find the key. */
void *
trietable_find(struct trietable **table, unsigned key)
{
    if (*table == NULL)
        return NULL;
    else if ((*table)->key == key)
        return (*table)->value;
    else if (key & 1)
        return trietable_find(&(*table)->ptr1, key >> 1);
    else
        return trietable_find(&(*table)->ptr0, key >> 1);
}

void
trietable_empty(struct trietable **table)
{
    if (*table == NULL)
        return;
    trietable_empty(&(*table)->ptr0);
    trietable_empty(&(*table)->ptr1);
    free(*table);
    *table = NULL;
}
