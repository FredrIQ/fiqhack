/* Copyright (c) Daniel Thaler, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>

/* malloc wrapper functions for "external" memory allocations
 * 
 * The idea is to avoid a transfer of responsibility for freeing memory to to
 * callers of libnethack api functions. This would require clearly stating which
 * pointers refer to heap-allocated memory (requiring free) and which are
 * statically allocated. This seems undesirable.
 * 
 * Instead a lifetime rule is introduced: returned memory is only valid until
 * the next move. After that memory is automatically freed.
 */

struct xmalloc_block {
    void *mem;
    struct xmalloc_block *next;
};

struct xmalloc_block *xm_blocklist = NULL;

void *xmalloc(int size)
{
    void *mem;
    struct xmalloc_block *b;
    
    mem = malloc(size);
    if (!mem)
	return NULL;
    
    b = malloc(sizeof(struct xmalloc_block));
    if (!b) {
	free(mem);
	return NULL;
    }
    
    b->mem = mem;
    b->next = xm_blocklist;
    xm_blocklist = b;
    
    return mem;
}


void xmalloc_cleanup(void)
{
    struct xmalloc_block *b;
    while (xm_blocklist) {
	b = xm_blocklist;
	xm_blocklist = xm_blocklist->next;
	
	free(b->mem);
	free(b);
    }
}