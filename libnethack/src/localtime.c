/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) Robert Patrick Rankin, 1991                      */
/* NetHack may be freely redistributed.  See license for details. */

#include <hack.h>

/* Local time routines. These were previously in hacklib.c, but need to adjust
 * for restoring a save, and thus need more knowledge of NetHack than hacklib.c
 * has.
 *
 * The time is used for:
 *      - seed for rand()
 *      - year on tombstone and yyyymmdd in record file
 *      - phase of the moon (various monsters react to NEW_MOON or FULL_MOON)
 *      - night and midnight (the undead are dangerous at midnight)
 *      - determination of what files are "very old"
 */

static struct tm *getlt(void);

long
get_tz_offset(void)
{
#if !defined(__FreeBSD__)
    tzset();    /* sets the extern "timezone" which has the offset from UTC in
                   seconds */
    return timezone;
#else
    time_t t = time(NULL);

    return -localtime(&t)->tm_gmtoff;
#endif
}

static struct tm *
getlt(void)
{
    return gmtime((time_t *) & turntime);
}

int
getyear(void)
{
    return 1900 + getlt()->tm_year;
}

long
yyyymmdd(time_t date)
{
    long datenum;
    struct tm *lt;

    if (date == 0)
        lt = getlt();
    else
        lt = gmtime(&date);

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

int
night(void)
{
    int hour = getlt()->tm_hour;

    return hour < 6 || hour > 21;
}

int
midnight(void)
{
    return getlt()->tm_hour == 0;
}

