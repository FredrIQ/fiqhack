/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "date.h"
/*
 * All the references to the contents of patchlevel.h have been moved
 * into makedefs....
 */
#include "patchlevel.h"

/* #define BETA_INFO "" *//* "[ beta n]" */

static const char *getversionstring(void);


/* fill buffer with short version (so caller can avoid including date.h) */
const char *
version_string(void)
{
    return VERSION_STRING;
}

/* fill and return the given buffer with the long nethack version string */
const char *
getversionstring(void)
{
    const char *rv = VERSION_ID;
#if defined(BETA) && defined(BETA_INFO)
    rv = VERSION_ID " " BETA_INFO;
#endif
    return rv;
}

int
doversion(const struct nh_cmd_arg *arg)
{
    (void) arg;

    pline(msgc_info, "%s", getversionstring());
    return 0;
}


boolean
check_version(struct version_info * version_data, const char *filename,
              boolean complain)
{
    if (
#ifdef VERSION_COMPATIBILITY
           version_data->incarnation < VERSION_COMPATIBILITY ||
           version_data->incarnation > VERSION_NUMBER
#else
           version_data->incarnation != VERSION_NUMBER
#endif
        ) {
        if (complain)
            pline(msgc_saveload, "Version mismatch for file \"%s\".", filename);
        return FALSE;
    } else if (version_data->feature_set != VERSION_FEATURES ||
               version_data->entity_count != VERSION_SANITY1) {
        if (complain)
            pline(msgc_saveload,
                  "Configuration incompatibility for file \"%s\".", filename);
        return FALSE;
    }
    return TRUE;
}

/* this used to be based on file date and somewhat OS-dependent,
   but now examines the initial part of the file's contents */
boolean
uptodate(struct memfile * mf, const char *name)
{
    struct version_info vers_info;
    boolean verbose = name ? TRUE : FALSE;

    vers_info.incarnation = mread32(mf);
    vers_info.feature_set = mread32(mf);
    vers_info.entity_count = mread32(mf);
    if (!check_version(&vers_info, name, verbose))
        return FALSE;

    return TRUE;
}

void
store_version(struct memfile *mf)
{
    mtag(mf, 0, MTAG_VERSION);
    mwrite32(mf, VERSION_NUMBER);
    mwrite32(mf, VERSION_FEATURES);
    mwrite32(mf, VERSION_SANITY1);
    return;
}


/*version.c*/

