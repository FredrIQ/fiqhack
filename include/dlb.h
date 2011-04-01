/*	SCCS Id: @(#)dlb.h	3.4	1997/07/29	*/
/* Copyright (c) Kenneth Lorber, Bethesda, Maryland, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef DLB_H
#define DLB_H
/* definitions for data library */

#ifdef DLB

/* directory structure in memory */
typedef struct dlb_directory {
    char *fname;	/* file name as seen from calling code */
    long foffset;	/* offset in lib file to start of this file */
    long fsize;		/* file size */
    char handling;	/* how to handle the file (compression, etc) */
} libdir;

/* information about each open library */
typedef struct dlb_library {
    FILE *fdata;	/* opened data file */
    long fmark;		/* current file mark */
    libdir *dir;	/* directory of library file */
    char *sspace;	/* pointer to string space */
    long nentries;	/* # of files in directory */
    long rev;		/* dlb file revision */
    long strsize;	/* dlb file string size */
} library;

/* library definitions */
# ifndef DLBFILE
#  define DLBFILE	"nhdat"			/* name of library */
# endif
# ifndef FILENAME_CMP
#  define FILENAME_CMP	strcmp			/* case sensitive */
# endif



typedef struct dlb_handle {
    FILE *fp;		/* pointer to an external file, use if non-null */
    library *lib;	/* pointer to library structure */
    long start;		/* offset of start of file */
    long size;		/* size of file */
    long mark;		/* current file marker */
} dlb;

#if defined(ULTRIX_PROTO) && !defined(__STDC__)
 /* buggy old Ultrix compiler wants this for the (*dlb_fread_proc)
    and (*dlb_fgets_proc) prototypes in struct dlb_procs (dlb.c);
    we'll use it in all the declarations for consistency */
#define DLB_P struct dlb_handle *
#else
#define DLB_P dlb *
#endif

boolean NDECL(dlb_init);
void NDECL(dlb_cleanup);

dlb *FDECL(dlb_fopen, (const char *,const char *));
int FDECL(dlb_fclose, (DLB_P));
int FDECL(dlb_fread, (char *,int,int,DLB_P));
int FDECL(dlb_fseek, (DLB_P,long,int));
char *FDECL(dlb_fgets, (char *,int,DLB_P));
int FDECL(dlb_fgetc, (DLB_P));
long FDECL(dlb_ftell, (DLB_P));


#else /* DLB */

# define dlb FILE

# define dlb_init()
# define dlb_cleanup()

# define dlb_fopen	fopen
# define dlb_fclose	fclose
# define dlb_fread	fread
# define dlb_fseek	fseek
# define dlb_fgets	fgets
# define dlb_fgetc	fgetc
# define dlb_ftell	ftell

#endif /* DLB */


/* various other I/O stuff we don't want to replicate everywhere */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

#define RDTMODE "r"
#if defined(WIN32) && defined(DLB)
#define WRTMODE "w+b"
#else
#define WRTMODE "w+"
#endif
#if defined(MICRO) || defined(THINK_C) || defined(__MWERKS__) || defined(WIN32)
# define RDBMODE "rb"
# define WRBMODE "w+b"
#else
# define RDBMODE "r"
# define WRBMODE "w+"
#endif

#endif	/* DLB_H */
