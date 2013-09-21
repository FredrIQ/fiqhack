/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) Robert Patrick Rankin, 1991                      */
/* NetHack may be freely redistributed.  See license for details. */

#include "config.h"
#include "hacklib.h"
#include <limits.h>
#ifndef AIMAKE_BUILDOS_MSWin32
# include <sys/time.h>
#endif

/*=
    Assorted 'small' utility routines.  They're entirely independent of NetHack.

      return type     routine name    argument type(s)
        boolean         digit           (char)
        boolean         letter          (char)
        char            highc           (char)
        char            lowc            (char)
        char *          lcase           (char *)
        char *          upstart         (char *)
        char *          mungspaces      (char *)
        char *          eos             (char *)
        char *          strkitten       (char *,char)
        char *          s_suffix        (const char *)
        char *          xcrypt          (const char *, char *)
        boolean         onlyspace       (const char *)
        char *          tabexpand       (char *)
        char *          visctrl         (char)
        const char *    ordin           (int)
        char *          sitoa           (int)
        int             sgn             (int)
        int             rounddiv        (long, int)
        int             distmin         (int, int, int, int)
        int             dist2           (int, int, int, int)
        boolean         online2         (int, int)
        boolean         pmatch          (const char *, const char *)
        int             strncmpi        (const char *, const char *, int)
        char *          strstri         (const char *, const char *)
        boolean         fuzzymatch      (const char *,const char *,const char *,boolean)
        void            setrandom       (void)
        unsigned int    get_seedval     (void)
=*/

boolean
digit(char c)
{       /* is 'c' a digit? */
    return (boolean) ('0' <= c && c <= '9');
}

boolean
letter(char c)
{       /* is 'c' a letter? note: '@' classed as letter */
    return (boolean) (('@' <= c && c <= 'Z') || ('a' <= c && c <= 'z'));
}


char
highc(char c)
{       /* force 'c' into uppercase */
    return (char)(('a' <= c && c <= 'z') ? (c & ~040) : c);
}

char
lowc(char c)
{       /* force 'c' into lowercase */
    return (char)(('A' <= c && c <= 'Z') ? (c | 040) : c);
}


char *
lcase(char *s)
{       /* convert a string into all lowercase */
    char *p;

    for (p = s; *p; p++)
        if ('A' <= *p && *p <= 'Z')
            *p |= 040;
    return s;
}

char *
upstart(char *s)
{       /* convert first character of a string to uppercase */
    if (s)
        *s = highc(*s);
    return s;
}

/* remove excess whitespace from a string buffer (in place) */
char *
mungspaces(char *bp)
{
    char c, *p, *p2;
    boolean was_space = TRUE;

    for (p = p2 = bp; (c = *p) != '\0'; p++) {
        if (c == '\t')
            c = ' ';
        if (c != ' ' || !was_space)
            *p2++ = c;
        was_space = (c == ' ');
    }
    if (was_space && p2 > bp)
        p2--;
    *p2 = '\0';
    return bp;
}


char *
eos(char *s)
{       /* return the end of a string (pointing at '\0') */
    while (*s)
        s++;    /* s += strlen(s); */
    return s;
}

char *
s_suffix(const char *s)
{       /* return a name converted to possessive */
    static char buf[BUFSZ];

    strcpy(buf, s);
    if (!strcmpi(buf, "it"))
        strcat(buf, "s");
    else if (*(eos(buf) - 1) == 's')
        strcat(buf, "'");
    else
        strcat(buf, "'s");
    return buf;
}

char *
xcrypt(const char *str, char *buf)
{       /* trivial text encryption routine (see makedefs) */
    const char *p;
    char *q;
    int bitmask;

    for (bitmask = 1, p = str, q = buf; *p; q++) {
        *q = *p++;
        if (*q & (32 | 64))
            *q ^= bitmask;
        if ((bitmask <<= 1) >= 32)
            bitmask = 1;
    }
    *q = '\0';
    return buf;
}


boolean
onlyspace(const char *s)
{       /* is a string entirely whitespace? */
    for (; *s; s++)
        if (*s != ' ' && *s != '\t')
            return FALSE;
    return TRUE;
}


char *
tabexpand(char *sbuf)
{       /* expand tabs into proper number of spaces */
    char buf[BUFSZ];
    char *bp, *s = sbuf;
    int idx;

    if (!*s)
        return sbuf;

    /* warning: no bounds checking performed */
    for (bp = buf, idx = 0; *s; s++)
        if (*s == '\t') {
            do
                *bp++ = ' ';
            while (++idx % 8);
        } else {
            *bp++ = *s;
            idx++;
        }
    *bp = 0;
    return strcpy(sbuf, buf);
}


const char *
ordin(int n)
{       /* return the ordinal suffix of a number */
    /* note: n should be non-negative */
    int dd = n % 10;

    return (dd == 0 || dd > 3 || (n % 100) / 10 == 1) ? "th" :
        (dd == 1) ? "st" : (dd == 2) ? "nd" : "rd";
}


char *
sitoa(int n)
{       /* make a signed digit string from a number */
    static char buf[13];

    sprintf(buf, (n < 0) ? "%d" : "+%d", n);
    return buf;
}

int
sgn(int n)
{       /* return the sign of a number: -1, 0, or 1 */
    return (n < 0) ? -1 : (n != 0);
}


int
rounddiv(long x, int y)
{       /* calculate x/y, rounding as appropriate; division by 0
           returns INT_MAX/INT_MIN */
    int r, m;
    int divsgn = 1;

    if (y == 0) {
        if (x < 0) return INT_MIN;
        return INT_MAX;
    } else if (y < 0) {
        divsgn = -divsgn;
        y = -y;
    }
    if (x < 0) {
        divsgn = -divsgn;
        x = -x;
    }
    r = x / y;
    m = x % y;
    if (2 * m >= y)
        r++;

    return divsgn * r;
}


/* distance between two points, in moves */
int
distmin(int x0, int y0, int x1, int y1)
{
    int dx = x0 - x1, dy = y0 - y1;

    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    /* The minimum number of moves to get from (x0,y0) to (x1,y1) is the :
       larger of the [absolute value of the] two deltas. */
    return (dx < dy) ? dy : dx;
}

/* square of euclidean distance between pair of pts */
int
dist2(int x0, int y0, int x1, int y1)
{
    int dx = x0 - x1, dy = y0 - y1;

    return dx * dx + dy * dy;
}

/* are two points lined up (on a straight line)? */
boolean
online2(int x0, int y0, int x1, int y1)
{
    int dx = x0 - x1, dy = y0 - y1;

    /* If either delta is zero then they're on an orthogonal line, else if the
       deltas are equal (signs ignored) they're on a diagonal. */
    return (boolean)(!dy || !dx || (dy == dx) ||
                     (dy + dx == 0)); /* (dy == -dx) */
}


/* match a string against a pattern */
boolean
pmatch(const char *patrn, const char *strng)
{
    char s, p;

    /* 
       : Simple pattern matcher: '*' matches 0 or more characters, '?' matches
       : any single character.  Returns TRUE if 'strng' matches 'patrn'. */
pmatch_top:
    s = *strng++;
    p = *patrn++;       /* get next chars and pre-advance */
    if (!p)     /* end of pattern */
        return (boolean) (s == '\0');   /* matches iff end of string too */
    else if (p == '*')  /* wildcard reached */
        return ((boolean)
                ((!*patrn || pmatch(patrn, strng - 1)) ? TRUE :
                 s ? pmatch(patrn - 1, strng) : FALSE));
    else if (p != s && (p != '?' || !s))        /* check single character */
        return FALSE;   /* doesn't match */
    else        /* return pmatch(patrn, strng); */
        goto pmatch_top;        /* optimize tail recursion */
}


#ifndef STRSTRI
/* case insensitive substring search */
char *
strstri(const char *str, const char *sub)
{
    const char *s1, *s2;
    int i, k;

# define TABSIZ 0x20    /* 0x40 would be case-sensitive */
    char tstr[TABSIZ], tsub[TABSIZ];    /* nibble count tables */

    /* special case: empty substring */
    if (!*sub)
        return (char *)str;

    /* do some useful work while determining relative lengths */
    for (i = 0; i < TABSIZ; i++)
        tstr[i] = tsub[i] = 0;  /* init */
    for (k = 0, s1 = str; *s1; k++)
        tstr[*s1++ & (TABSIZ - 1)]++;
    for (s2 = sub; *s2; --k)
        tsub[*s2++ & (TABSIZ - 1)]++;

    /* evaluate the info we've collected */
    if (k < 0)
        return NULL;    /* sub longer than str, so can't match */
    for (i = 0; i < TABSIZ; i++)        /* does sub have more 'x's than str? */
        if (tsub[i] > tstr[i])
            return NULL;        /* match not possible */

    /* now actually compare the substring repeatedly to parts of the string */
    for (i = 0; i <= k; i++) {
        s1 = &str[i];
        s2 = sub;
        while (lowc(*s1++) == lowc(*s2++))
            if (!*s2)
                return (char *)&str[i]; /* full match */
    }
    return NULL;        /* not found */
}
#endif /* STRSTRI */

/* compare two strings for equality, ignoring the presence of specified
   characters (typically whitespace) and possibly ignoring case */
boolean
fuzzymatch(const char *s1, const char *s2, const char *ignore_chars,
           boolean caseblind)
{
    char c1, c2;

    do {
        while ((c1 = *s1++) != '\0' && strchr(ignore_chars, c1) != 0)
            continue;
        while ((c2 = *s2++) != '\0' && strchr(ignore_chars, c2) != 0)
            continue;
        if (!c1 || !c2)
            break;      /* stop when end of either string is reached */

        if (caseblind) {
            c1 = lowc(c1);
            c2 = lowc(c2);
        }
    } while (c1 == c2);

    /* match occurs only when the end of both strings has been reached */
    return (boolean) (!c1 && !c2);
}

/* used to make the rng seed unguessable; this is only useful for server games
 * as otherwise you can simply read the seed from the logfile */
unsigned int
get_seedval(void)
{
#if defined(UNIX)
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_usec;
#else
    return 0;
#endif
}

/*hacklib.c*/
