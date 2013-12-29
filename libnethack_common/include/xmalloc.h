/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-29 */
/* Copyright (c) Daniel Thaler, 2011. */
/* Copyright (c) Alex Smith, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef XMALLOC_H
# define XMALLOC_H

struct xmalloc_block {
    void *mem;
    struct xmalloc_block *next;
};

extern void *xmalloc(struct xmalloc_block **blocklist, int size);
extern void xmalloc_cleanup(struct xmalloc_block **blocklist);

#endif
