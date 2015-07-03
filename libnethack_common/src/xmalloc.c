/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-04-13 */
/* Copyright (c) Daniel Thaler, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "xmalloc.h"

/* Malloc wrappers for creating memory chains.
 
   This is basically a vastly simplified version of garbage collection; we
   record the pointers we allocate on chains, and after a specific point in
   time, we know that all pointers on the chain should have died (e.g. messages
   by the end of the turn, API returns by the next API call). Thus, at that
   point, we can just clean up all the pointers at once. */

void *
xmalloc(struct xmalloc_block **blocklist, size_t size)
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

/* Resizes a pointer that's on an xmalloc chain.

   This is intended for use with pointers that have only just been allocated,
   although it will work for any pointer on the chain.

   It can also be used to free a pointer "early", by setting size to 0. */
void *
xrealloc(struct xmalloc_block **blocklist, void *ptr, size_t size)
{
    if (!ptr) /* same special case as realloc */
        return xmalloc(blocklist, size);

    /* This loop effectively treats a blocklist as being defined recursively: a
       blocklist contains a memory location and another (possibly NULL)
       blocklist. This leads to the following neat method of looping over a
       singly linked list in a way that allows deletion of elements. */
    while (*blocklist) {
        if ((*blocklist)->mem == ptr) {

            if (size > 0) {

                void *newptr = realloc(ptr, size);
                if (!newptr)
                    return NULL;

                (*blocklist)->mem = newptr;
                return newptr;
                
            } else {

                /* My reading of C11 implies that it's possible for realloc(ptr,
                   0) to deallocate ptr, then allocate and return a valid block
                   of memory (that cannot be dereferenced, due to being
                   effectively zero bytes long). Obviously, this is stupid, but
                   we may as well use free() to dodge the whole issue... */

                struct xmalloc_block *oldblock = *blocklist;
                (*blocklist) = (*blocklist)->next;
                
                free(ptr);
                free(oldblock);
                return NULL;
            }

        }
        blocklist = &((*blocklist)->next);
    }

    /* We didn't find it. The correct reaction to memory corruption like this is
       a segfault, the same way as a NULL dereference or the like.

       C11 actually officially defines segfaults as something that exist,
       although it doesn't require an implementation to produce them in any
       situation other than a function explicitly saying "this situation is a
       segfault"; that is, however, the situation we have here. Some older
       non-UNIX compilers may not be aware of segfaults, though, so we
       substitute an abort() in that situation. */

#ifdef SIGSEGV
    raise(SIGSEGV);
#endif
    /* We alo substitute an abort if a SIGSEGV handler returned. (That shouldn't
       happen either.) */
    abort();
}

/* vasprintf, allocating on an xmalloc chain. */
char *
xmvasprintf(struct xmalloc_block **blocklist, const char *fmt, va_list args)
{
    /* According to the C standards, vsnprintf returns the amount of size
       necessary for a buffer if you don't give it enough space. Windows' libc,
       however, blatantly breaks this rule, returning an error return instead.

       Thus, we allocate a small buffer, and expand it until the string fits.
       If the implementation tells us how large the buffer shoudld be, great;
       otherwise, we just work it out by experimentation.

       This code is a (clearer and also more efficient) version of the
       allocation loop from vw_printw from the (NGPL) libuncursed, which I
       originally wrote. */

    va_list args2;
    char *buf = xmalloc(blocklist, 8);

    int buffer_size = 8;
    int buffer_size_guess;

    for(;;) {
        va_copy(args2, args);
        *buf = 1; /* to distinguish a success and an error return of 0 chars */
        buffer_size_guess = vsnprintf(buf, buffer_size, fmt, args2);
        va_end(args2);

        if (buffer_size_guess < 0 || (buffer_size_guess == 0 && *buf))
            /* Error return. Produce a longer string. */
            buffer_size = buffer_size * 2;
        else if (buffer_size_guess < buffer_size)
            /* Success return: the string in question fits the buffer.

               Note: not <=, because vnsprintf does not count the '\0'. */
            return buf;
        else
            /* The return means "you need a buffer this large". */
            buffer_size = buffer_size_guess + 1;

        buf = xrealloc(blocklist, buf, buffer_size);
    }
}

/* astrftime, operating on an xmalloc chain.

   WARNING: This may only be used with formats that produce at least one byte of
   output, due to a bug in the protocol of strftime. If the format would produce
   empty output, this function may well go into an infinite loop or allocate all
   the system's memory. */
char *
xmastrftime(struct xmalloc_block **blocklist,
            const char *fmt, const struct tm *tm)
{
    /* Unlike snprintf, strftime's error behaviour in both theory practice is
       "return 0 or the length of the buffer", meaning we need to try larger
       buffers until one works. Additionally, success returning an empty string
       is indistinguishable from failure; not only does failure return 0, it
       also clobbers the buffer.

       So this is about the best we can do. We could consider placing an
       arbitrary cap on the output length to detect zero-byte output, but it's
       unclear if that's any better. */

    char *buf = xmalloc(blocklist, 8);
    int buffer_size = 8;
    int strftime_return;

    for(;;) {
        strftime_return = strftime(buf, buffer_size, fmt, tm);
        if (strftime_return > 0 && strftime_return < buffer_size)
            return buf;

        buffer_size = buffer_size * 2;
        buf = xrealloc(blocklist, buf, buffer_size);
    }
}
