/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-29 */
/* Copyright (c) Daniel Thaler, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include "xmalloc.h"

/* malloc wrapper functions for "external" memory allocations
 * 
 * The idea is to avoid a transfer of responsibility for freeing memory to to
 * callers of libnethack api functions. This would require clearly stating
 * which pointers refer to heap-allocated memory (requiring free) and which are
 * statically allocated. This seems undesirable.
 * 
 * Instead a lifetime rule is introduced: returned memory is only valid until
 * the next move. After that memory is automatically freed.
 */

void *
xmalloc(struct xmalloc_block **blocklist, int size)
{
    void *mem;
    struct xmalloc_block *b;

    mem = malloc(size);
    if (!mem)
        return NULL;

    b = malloc(sizeof (struct xmalloc_block));
    if (!b) {
        free(mem);
        return NULL;
    }

    b->mem = mem;
    b->next = *blocklist;
    *blocklist = b;

    return mem;
}


void
xmalloc_cleanup(struct xmalloc_block **blocklist)
{
    struct xmalloc_block *b;

    while (*blocklist) {
        b = *blocklist;
        *blocklist = b->next;

        free(b->mem);
        free(b);
    }
}
