/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Kenneth Lorber, Bethesda, Maryland, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DLB_H
# define DLB_H

# include "iomodes.h"
# include "global.h"

/* directory structure in memory */
typedef struct dlb_directory {
    char *fname;        /* file name as seen from calling code */
    long foffset;       /* offset in lib file to start of this file */
    long fsize; /* file size */
    char handling;      /* how to handle the file (compression, etc) */
} libdir;

/* information about each open library */
typedef struct dlb_library {
    FILE *fdata;        /* opened data file */
    long fmark; /* current file mark */
    libdir *dir;        /* directory of library file */
    char *sspace;       /* pointer to string space */
    long nentries;      /* # of files in directory */
    long rev;   /* dlb file revision */
    long strsize;       /* dlb file string size */
} library;

/* library definitions */
# ifndef DLBFILE
#  define DLBFILE       "nhdat"
                        /* name of library */
# endif
# ifndef FILENAME_CMP
#  define FILENAME_CMP  strcmp  /* case sensitive */
# endif



typedef struct dlb_handle {
    FILE *fp;   /* pointer to an external file, use if non-null */
    library *lib;       /* pointer to library structure */
    long start; /* offset of start of file */
    long size;  /* size of file */
    long mark;  /* current file marker */
} dlb;

# define DLB_P dlb *

boolean dlb_init(void);
void dlb_cleanup(void);

dlb *dlb_fopen(const char *, const char *);
int dlb_fclose(DLB_P);
int dlb_fread(void *, int, int, DLB_P);
int dlb_fseek(DLB_P, long, int);
char *dlb_fgets(void *, int, DLB_P);
int dlb_fgetc(DLB_P);
long dlb_ftell(DLB_P);

#endif /* DLB_H */

