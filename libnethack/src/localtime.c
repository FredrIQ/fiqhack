/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-11-14 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) Robert Patrick Rankin, 1991                      */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include <time.h>
#ifdef UNIX
# include <sys/time.h>
#endif

/*
 * Local time routines. These were previously in hacklib.c, but need to adjust
 * for timezones and for the coarser time sampling regime, and thus need more
 * knowledge of NetHack than hacklib.c has.
 *
 * There are two main timebases used:
 * - The actual UTC clock (utc_time()) is used outside the game, for:
 *   - Dates in log and xlog files
 *   - Dumplog filenames
 *   - Panic log
 *
 * - The time of the player's last input to the game, modified by the
 *   timezone birth option, is used inside the game, for:
 *   - Phase of the moon (luck bonus, werefoo, etc.)
 *   - Night and midnight
 *   - Tombstone
 *   - Friday 13th determination (also other special holidays, should we
 *     choose to implement those in the future)
 *
 * Both these timebases are represented as an nh_microseconds value (a long
 * long) internally, to avoid the Y2038K problem; these count in microseconds.
 *
 * The former timebase is updated constantly by the operating system; the latter
 * is only sampled at inputs, via calls to log_timeline, and follows all the
 * conventions used for user input (i.e. record/replay, not affected by zero
 * time commands, and so on). It's stored in memory and in the save file via
 * flags.turntime, with the timezone offset already applied.
 */

/* Outside-the-game time. */
microseconds
utc_time(void)
{
#ifdef UNIX
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((microseconds)tv.tv_sec) * 1000000LL +
        (microseconds)tv.tv_usec;
#else
    return (microseconds)time(NULL) * 1000000LL;
#endif
}

/* Inside-the-game time should be read from flags.turntime, almost always.
   However, something has to look at the clock to set flags.turntime, so
   here we are. This should only be called from log_time_line, or functions
   that are similarly aware of save file discipline; otherwise this will
   cause save desyncs. */
microseconds
time_for_time_line(void)
{
    return utc_time() + ((microseconds)flags.timezone * 1000000LL);
}

/* Produces a broken-down time structure from turntime. */
static struct tm *
getlt(void)
{
    time_t t = (time_t)(flags.turntime / 1000000LL);
    /* "gmtime" is the time breakdown function that does no timezone conversions
       itself; the output is in the same timezone as the input. This applies
       whether they're both UTC, or whether as (in this case) they're both
       local. */
    return gmtime(&t);
}

int
getyear(void)
{
    return 1900 + getlt()->tm_year;
}

long
yyyymmdd(microseconds date)
{
    long datenum;
    struct tm *lt;
    time_t t = (time_t)(date/1000000LL);

    if (date == 0)
        panic("Attempted to log in-game time as out-of-game time");
    else
        lt = gmtime(&t);

    /* just in case somebody's gmtime supplies (year % 100) rather than the
       expected (year - 1900) */
    if (lt->tm_year < 70)
        datenum = (long)lt->tm_year + 2000L;
    else
        datenum = (long)lt->tm_year + 1900L;
    /* yyyy --> yyyymm */
    datenum = datenum * 100L + (long)(lt->tm_mon + 1);
    /* yyyymm --> yyyymmdd */
    datenum = datenum * 100L + (long)lt->tm_mday;
    return datenum;
}

/*
 * moon period = 29.53058 days ~= 30, year = 365.2422 days
 * days moon phase advances on first day of year compared to preceding year
 *      = 365.2422 - 12*29.53058 ~= 11
 * years in Metonic cycle (time until same phases fall on the same days of
 *      the month) = 18.6 ~= 19
 * moon phase on first day of year (epact) ~= (11*(year%19) + 29) % 30
 *      (29 as initial condition)
 * current phase in days = first day phase + days elapsed in year
 * 6 moons ~= 177 days
 * 177 ~= 8 reported phases * 22
 * + 11/22 for rounding
 */
int
phase_of_the_moon(void)
{       /* 0-7, with 0: new, 4: full */
    struct tm *lt = getlt();
    int epact, diy, goldn;

    diy = lt->tm_yday;
    goldn = (lt->tm_year % 19) + 1;
    epact = (11 * goldn + 18) % 30;
    if ((epact == 25 && goldn > 11) || epact == 24)
        epact++;

    return (((((diy + epact) * 6) + 11) % 177) / 22) & 7;
}

boolean
friday_13th(void)
{
    struct tm *lt = getlt();

    return (boolean) (lt->tm_wday == 5 /* friday */  && lt->tm_mday == 13);
}

boolean
night(void)
{
    int hour = getlt()->tm_hour;

    return hour < 6 || hour > 21;
}

boolean
midnight(void)
{
    return getlt()->tm_hour == 0;
}

