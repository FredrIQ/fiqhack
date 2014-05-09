/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-09 */
/* Copyright (c) Kenneth Lorber, Bethesda, Maryland, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef IOMODES_H
# define IOMODES_H

#include <stdio.h>

/* various I/O stuff we don't want to replicate everywhere */

# ifndef SEEK_SET
#  define SEEK_SET 0
# endif
# ifndef SEEK_CUR
#  define SEEK_CUR 1
# endif
# ifndef SEEK_END
#  define SEEK_END 2
# endif

# define RDTMODE "r"

# if defined(WIN32)
#  define WRTMODE "w+b"
#  define RDBMODE "rb"
#  define WRBMODE "w+b"
# else
#  define WRTMODE "w+"
#  define RDBMODE "r"
#  define WRBMODE "w+"
# endif

enum locktype
{
    LT_NONE,    /* do not lock at all */
    LT_MONITOR, /* for use only on program_state.logfile; do a server cancel
                   whenever the file is write-locked by another process */
    LT_READ,    /* do not allow other processes to write to this file */
    LT_WRITE,   /* do not allow other processes to read or write to this file */
};

#endif /* IOMODES_H */

