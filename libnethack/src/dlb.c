/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-10-17 */
/* Copyright (c) Kenneth Lorber, Bethesda, Maryland, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "nethack_types.h"
#include "config.h"
#include "dlb.h"

/* without extern.h via hack.h, these haven't been declared for us */
extern FILE *fopen_datafile(const char *, const char *, int);

/*
 * Data librarian.  Present a STDIO-like interface to NetHack while
 * multiplexing on one or more "data libraries".  If a file is not found
 * in a given library, look for it outside the libraries.
 */

typedef struct dlb_procs {
    boolean(*dlb_init_proc) (void);
    void (*dlb_cleanup_proc) (void);
         boolean(*dlb_fopen_proc) (DLB_P, const char *, const char *);
    int (*dlb_fclose_proc) (DLB_P);
    int (*dlb_fread_proc) (char *, int, int, DLB_P);
    int (*dlb_fseek_proc) (DLB_P, long, int);
    char *(*dlb_fgets_proc) (char *, int, DLB_P);
    int (*dlb_fgetc_proc) (DLB_P);
    long (*dlb_ftell_proc) (DLB_P);
} dlb_procs_t;


/*
 * Library Implementation:
 *
 * When initialized, we open all library files and read in their tables
 * of contents.  The library files stay open all the time.  When
 * a open is requested, the libraries' directories are searched.  If
 * successful, we return a descriptor that contains the library, file
 * size, and current file mark.  This descriptor is used for all
 * successive calls.
 *
 * The ability to open more than one library is supported but used
 * only in the Amiga port (the second library holds the sound files).
 * For Unix, the idea would be to split the NetHack library
 * into text and binary parts, where the text version could be shared.
 */

#define MAX_LIBS 4
static library dlb_libs[MAX_LIBS];

static boolean readlibdir(library * lp);
static boolean find_file(const char *name, library ** lib, long *startp,
                         long *sizep);
static boolean lib_dlb_init(void);
static void lib_dlb_cleanup(void);
static boolean lib_dlb_fopen(dlb *, const char *, const char *);
static int lib_dlb_fclose(dlb *);
static int lib_dlb_fread(char *, int, int, dlb *);
static int lib_dlb_fseek(dlb *, long, int);
static char *lib_dlb_fgets(char *, int, dlb *);
static int lib_dlb_fgetc(dlb *);
static long lib_dlb_ftell(dlb *);

/* not static because shared with dlb_main.c */
boolean open_library(const char *lib_name, library * lp);
void close_library(library * lp);


/*
 * Read the directory out of the library.  Return 1 if successful,
 * 0 if it failed.
 *
 * NOTE: An improvement of the file structure should be the file
 * size as part of the directory entry or perhaps in place of the
 * offset -- the offset can be calculated by a running tally of
 * the sizes.
 *
 * Library file structure:
 *
 * HEADER:
 * %3ld library FORMAT revision (currently rev 1)
 * %1c  space
 * %8ld # of files in archive (includes 1 for directory)
 * %1c  space
 * %8ld size of allocation for string space for directory names
 * %1c  space
 * %8ld library offset - sanity check - lseek target for start of first file
 * %1c  space
 * %8ld size - sanity check - byte size of complete archive file
 *
 * followed by one DIRECTORY entry for each file in the archive, including
 *  the directory itself:
 * %1c  handling information (compression, etc.)  Always ' ' in rev 1.
 * %s   file name
 * %1c  space
 * %8ld offset in archive file of start of this file
 * %c   newline
 *
 * followed by the contents of the files
 */
#define DLB_MIN_VERS  1 /* min library version readable by this code */
#define DLB_MAX_VERS  1 /* max library version readable by this code */

/*
 * Read the directory from the library file.   This will allocate and
 * fill in our globals.  The file pointer is reset back to position
 * zero.  If any part fails, leave nothing that needs to be deallocated.
 *
 * Return TRUE on success, FALSE on failure.
 */
static boolean
readlibdir(library * lp)
/* lp: library pointer to fill in */
{
    int i;
    char *sp;
    long liboffset, totalsize;

    if (fscanf
        (lp->fdata, "%ld %ld %ld %ld %ld\n", &lp->rev, &lp->nentries,
         &lp->strsize, &liboffset, &totalsize) != 5)
        return FALSE;
    if (lp->rev > DLB_MAX_VERS || lp->rev < DLB_MIN_VERS)
        return FALSE;

    lp->dir = malloc(lp->nentries * sizeof (libdir));
    lp->sspace = malloc(lp->strsize);

    /* read in each directory entry */
    for (i = 0, sp = lp->sspace; i < lp->nentries; i++) {
        lp->dir[i].fname = sp;
        if (fscanf
            (lp->fdata, "%c%s %ld\n", &lp->dir[i].handling, sp,
             &lp->dir[i].foffset) != 3) {
            free(lp->dir);
            free(lp->sspace);
            lp->dir = NULL;
            lp->sspace = NULL;
            return FALSE;
        }
        sp = sp + strlen(sp) + 1;
    }

    /* calculate file sizes using offset information */
    for (i = 0; i < lp->nentries; i++) {
        if (i == lp->nentries - 1)
            lp->dir[i].fsize = totalsize - lp->dir[i].foffset;
        else
            lp->dir[i].fsize = lp->dir[i + 1].foffset - lp->dir[i].foffset;
    }

    fseek(lp->fdata, 0L, SEEK_SET);     /* reset back to zero */
    lp->fmark = 0;

    return TRUE;
}

/*
 * Look for the file in our directory structure.  Return 1 if successful,
 * 0 if not found.  Fill in the size and starting position.
 */
static boolean
find_file(const char *name, library ** lib, long *startp, long *sizep)
{
    int i, j;
    library *lp;

    for (i = 0; i < MAX_LIBS && dlb_libs[i].fdata; i++) {
        lp = &dlb_libs[i];
        for (j = 0; j < lp->nentries; j++) {
            if (FILENAME_CMP(name, lp->dir[j].fname) == 0) {
                *lib = lp;
                *startp = lp->dir[j].foffset;
                *sizep = lp->dir[j].fsize;
                return TRUE;
            }
        }
    }
    *lib = NULL;
    *startp = *sizep = 0;
    return FALSE;
}

/*
 * Open the library of the given name and fill in the given library
 * structure.  Return TRUE if successful, FALSE otherwise.
 */
boolean
open_library(const char *lib_name, library * lp)
{
    boolean status = FALSE;

    lp->fdata = fopen_datafile(lib_name, RDBMODE, DATAPREFIX);
    if (lp->fdata) {
        if (readlibdir(lp)) {
            status = TRUE;
        } else {
            fclose(lp->fdata);
            lp->fdata = NULL;
        }
    }
    return status;
}

void
close_library(library * lp)
{
    fclose(lp->fdata);
    free(lp->dir);
    free(lp->sspace);

    memset((char *)lp, 0, sizeof (library));
}

/*
 * Open the library file once using stdio.  Keep it open, but
 * keep track of the file position.
 */
static boolean
lib_dlb_init(void)
{
    /* zero out array */
    memset((char *)&dlb_libs[0], 0, sizeof (dlb_libs));

    /* To open more than one library, add open library calls here. */
    if (!open_library(DLBFILE, &dlb_libs[0]))
        return FALSE;
#ifdef DLBFILE2
    if (!open_library(DLBFILE2, &dlb_libs[1])) {
        close_library(&dlb_libs[0]);
        return FALSE;
    }
#endif
    return TRUE;
}

static void
lib_dlb_cleanup(void)
{
    int i;

    /* close the data file(s) */
    for (i = 0; i < MAX_LIBS && dlb_libs[i].fdata; i++)
        close_library(&dlb_libs[i]);
}

static boolean
lib_dlb_fopen(dlb * dp, const char *name, const char *mode)
{
    long start, size;
    library *lp;

    (void)mode;
    /* look up file in directory */
    if (find_file(name, &lp, &start, &size)) {
        dp->lib = lp;
        dp->start = start;
        dp->size = size;
        dp->mark = 0;
        return TRUE;
    }

    return FALSE;       /* failed */
}

static int
lib_dlb_fclose(dlb * dp)
{
    /* nothing needs to be done */
    (void)dp;
    return 0;
}

static int
lib_dlb_fread(char *buf, int size, int quan, dlb * dp)
{
    long pos, nread, nbytes;

    /* make sure we don't read into the next file */
    if ((dp->size - dp->mark) < (size * quan))
        quan = (dp->size - dp->mark) / size;
    if (quan == 0)
        return 0;

    pos = dp->start + dp->mark;
    if (dp->lib->fmark != pos) {
        fseek(dp->lib->fdata, pos, SEEK_SET);   /* check for error??? */
        dp->lib->fmark = pos;
    }

    nread = fread(buf, size, quan, dp->lib->fdata);
    nbytes = nread * size;
    dp->mark += nbytes;
    dp->lib->fmark += nbytes;

    return nread;
}

static int
lib_dlb_fseek(dlb * dp, long pos, int whence)
{
    long curpos;

    switch (whence) {
    case SEEK_CUR:
        curpos = dp->mark + pos;
        break;
    case SEEK_END:
        curpos = dp->size - pos;
        break;
    default:   /* set */
        curpos = pos;
        break;
    }
    if (curpos < 0)
        curpos = 0;
    if (curpos > dp->size)
        curpos = dp->size;

    dp->mark = curpos;
    return 0;
}

static char *
lib_dlb_fgets(char *buf, int len, dlb * dp)
{
    int i;
    char *bp, c = 0;

    if (len <= 0)
        return buf;     /* sanity check */

    /* return NULL on EOF */
    if (dp->mark >= dp->size)
        return NULL;

    len--;      /* save room for null */
    for (i = 0, bp = buf; i < len && dp->mark < dp->size && c != '\n';
         i++, bp++) {
        if (dlb_fread(bp, 1, 1, dp) <= 0)
            break;      /* EOF or error */
        c = *bp;
    }
    *bp = '\0';

#if defined(WIN32)
    if ((bp = strchr(buf, '\r')) != 0) {
        *bp++ = '\n';
        *bp = '\0';
    }
#endif

    return buf;
}

static int
lib_dlb_fgetc(dlb * dp)
{
    char c;

    if (lib_dlb_fread(&c, 1, 1, dp) != 1)
        return EOF;
    return (int)c;
}


static long
lib_dlb_ftell(dlb * dp)
{
    return dp->mark;
}

static const dlb_procs_t lib_dlb_procs = {
    lib_dlb_init,
    lib_dlb_cleanup,
    lib_dlb_fopen,
    lib_dlb_fclose,
    lib_dlb_fread,
    lib_dlb_fseek,
    lib_dlb_fgets,
    lib_dlb_fgetc,
    lib_dlb_ftell
};

/* Global wrapper functions ------------------------------------------------ */

#define do_dlb_init (*dlb_procs->dlb_init_proc)
#define do_dlb_cleanup (*dlb_procs->dlb_cleanup_proc)
#define do_dlb_fopen (*dlb_procs->dlb_fopen_proc)
#define do_dlb_fclose (*dlb_procs->dlb_fclose_proc)
#define do_dlb_fread (*dlb_procs->dlb_fread_proc)
#define do_dlb_fseek (*dlb_procs->dlb_fseek_proc)
#define do_dlb_fgets (*dlb_procs->dlb_fgets_proc)
#define do_dlb_fgetc (*dlb_procs->dlb_fgetc_proc)
#define do_dlb_ftell (*dlb_procs->dlb_ftell_proc)

static const dlb_procs_t *dlb_procs;
static boolean dlb_initialized = FALSE;

boolean
dlb_init(void)
{
    if (!dlb_initialized) {
        dlb_procs = &lib_dlb_procs;
        if (dlb_procs)
            dlb_initialized = do_dlb_init();
    }

    return dlb_initialized;
}

void
dlb_cleanup(void)
{
    if (dlb_initialized) {
        do_dlb_cleanup();
        dlb_initialized = FALSE;
    }
}

dlb *
dlb_fopen(const char *name, const char *mode)
{
    FILE *fp;
    dlb *dp;

    if (!dlb_initialized)
        return NULL;

    dp = malloc(sizeof (dlb));
    if (do_dlb_fopen(dp, name, mode))
        dp->fp = NULL;
    else if ((fp = fopen_datafile(name, mode, DATAPREFIX)) != 0)
        dp->fp = fp;
    else {
        /* can't find anything */
        free(dp);
        dp = NULL;
    }

    return dp;
}

int
dlb_fclose(dlb * dp)
{
    int ret = 0;

    if (dlb_initialized) {
        if (dp->fp)
            ret = fclose(dp->fp);
        else
            ret = do_dlb_fclose(dp);

        free(dp);
    }
    return ret;
}

int
dlb_fread(void *buf, int size, int quan, dlb * dp)
{
    if (!dlb_initialized || size <= 0 || quan <= 0)
        return 0;
    if (dp->fp)
        return fread(buf, size, quan, dp->fp);
    return do_dlb_fread(buf, size, quan, dp);
}

int
dlb_fseek(dlb * dp, long pos, int whence)
{
    if (!dlb_initialized)
        return EOF;
    if (dp->fp)
        return fseek(dp->fp, pos, whence);
    return do_dlb_fseek(dp, pos, whence);
}

char *
dlb_fgets(void *buf, int len, dlb * dp)
{
    if (!dlb_initialized)
        return NULL;
    if (dp->fp)
        return fgets(buf, len, dp->fp);
    return do_dlb_fgets(buf, len, dp);
}

int
dlb_fgetc(dlb * dp)
{
    if (!dlb_initialized)
        return EOF;
    if (dp->fp)
        return fgetc(dp->fp);
    return do_dlb_fgetc(dp);
}

long
dlb_ftell(dlb * dp)
{
    if (!dlb_initialized)
        return 0;
    if (dp->fp)
        return ftell(dp->fp);
    return do_dlb_ftell(dp);
}

/*dlb.c*/

