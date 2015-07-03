/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-29 */
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

    (Some routines have been deleted from this file due to not being entirely
    independent of NetHack; some were unused, others trivial and virtually
    unused, and two (s_suffix, upstart) used in so many places that it made
    sense to hook it up to libnethack's memory management systems and thus it
    was no longer appropriate for this file.)

      return type     routine name    argument type(s)
        boolean         digit           (char)
        boolean         letter          (char)
        char            highc           (char)
        char            lowc            (char)
        char *          mungspaces      (char *)
        char *          xcrypt          (const char *, char *)
        int             base85enclen    (int)
        int             base85declen    (int)
        int             base85enc       (const unsigned char *, int, char *)
        int             base85dec       (const char *, unsigned char *)
        boolean         onlyspace       (const char *)
        char *          tabexpand       (char *)
        char *          visctrl         (char)
        const char *    ordin           (int)
        int             sgn             (int)
        int             rounddiv        (long, int)
        int             isqrt           (int)
        int             ilog2           (long long)
        int             popcount        (unsigned long long)
        int             nextprime       (int)
        int             distmin         (int, int, int, int)
        int             dist2           (int, int, int, int)
        boolean         online2         (int, int)
        boolean         pmatch          (const char *, const char *)
        int             strncmpi        (const char *, const char *, int)
        char *          strstri         (const char *, const char *)
        boolean         fuzzymatch      (const char *,const char *,const char *,
                                         boolean)
        void            setrandom       (void)
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

/* Trivial text encryption routine (see makedefs). This does not obey the normal
   memory management behaviour, because it's needed outside libnethack; instead,
   it encrypts str into buf, and returns buf. buf must be at least large
   enough to hold str plus a NUL. */
char *
xcrypt(const char *str, char *buf)
{
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

/* Base 85 encoding and decoding.

   We encode every 4 octets of the input into 5 octets of output. If we have
   fewer than 4 octets on input, we encode them into that many plus one octets
   of output.

   The encoding itself treats 4 octets of base 256, or 5 octets of base 85, as
   a little-endian number, and translates it into the other number system. */
int
base85enclen(int declen)
{
    int dwords = declen / 4;
    int remainder = declen % 4;
    return dwords * 5 + (remainder == 0 ? 0 : remainder + 1);
}

int
base85declen(int enclen)
{
    int dwords = enclen / 5;
    int remainder = enclen % 5;

    if (remainder == 1)
        return -1; /* shouldn't happen */

    return dwords * 4 + (remainder == 0 ? 0 : remainder - 1);
}

static const uint64_t powers_of_85[] =
{ 1, 85, 85*85, 85L*85L*85L, 85L*85L*85L*85L, 85LL*85LL*85LL*85LL*85LL };

static_assert(' ' == 32, "Charset must be ASCII");
static_assert('~' == 126, "Charset must be ASCII");

/* Requires: allocated length of encode_into >= base85enclen(declen) + 1 (the +1
   is for the trailing NUL). Returns strlen(encode_into), to save the caller
   having to recalculate it. (This function has no error conditions.) */
int
base85enc(const unsigned char *decoded, int declen, char *encode_into)
{
    uint32_t n = 0;
    int bytes = 0;
    int outbytes = 0;

    while (declen) {
        n |= ((uint32_t)(*decoded) << (bytes * 8));
        decoded++;
        bytes++;
        declen--;

        if (!declen || bytes == 4) {
            if (bytes)
                bytes++;

            int bytes_upwards = 0;
            while (bytes) {
                bytes--;
                *encode_into = '%' + ((n / powers_of_85[bytes_upwards]) % 85);
                bytes_upwards++;
                encode_into++;
                outbytes++;
            }

            n = 0;
        }
    }

    *encode_into = '\0';
    return outbytes;
}

/* Requires: allocated length of decode_into >= base85declen(strlen(encoded)).
   Returns the number of bytes in the original decoded input (and writes that
   input into decode_into. Returns -1 on error. */
int
base85dec(const char *encoded, unsigned char *decode_into)
{
    uint64_t n = 0;
    int bytes = 0;
    int outbytes = 0;

    while (*encoded) {
        if (*encoded < '%' || *encoded >= '%' + 85)
            return -1;

        n += powers_of_85[bytes] * ((uint64_t)(*encoded - '%'));
        encoded++;
        bytes++;

        if (!*encoded || bytes == 5) {
            if (bytes == 1)
                return -1;
            if (n > 0xffffffffLLU)
                return -1;
            if (bytes)
                bytes--;

            int bytes_upwards = 0;
            while (bytes) {
                bytes--;
                *decode_into = (n >> (bytes_upwards * 8)) & 0xff;
                bytes_upwards++;
                decode_into++;
                outbytes++;
            }

            n = 0;
        }
    }

    return outbytes;
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

/* Integer square root function without using floating point. */
long long
isqrt(long long val)
{
    long long rv = 1;
    long long converge_distance = LLONG_MAX;

    if (val == 0)
        return 0;
    if (val < 0)
        return INT_MIN; /* this will do for a sentinel, the caller may not want
                           a long long */

    while (1) {
        long long rv2 = val / rv;
        long long d = rv - rv2;
        rv = (rv + rv2) / 2;
        if (d < 0) d = -d;
        if (d >= converge_distance)
            return rv;
        converge_distance = d;
    }
}

/* Integer base-2 logarithm without using floating point. The output is scaled
   by 1024 and rounded down. Returns -1024 for ilog2(0) (-1024 is actually the
   logarithm of 0.5). */
long long
ilog2(long long val)
{
    if (val <= 0)
        return -1024;

    /* invariant: ilog2(val@entry) == rv + ilog2(val) * rvscale / 1024 */

    long long rv = 0;
    long long rvscale = 1024;
    while (val != 1) {
        /* The number given here is sqrt(2**63) rounded down. */
        if (val >= 3037000499LL || rvscale == 1 || !(val%2)) {
            /* Doing this doesn't lose precision if val is even or rvscale is 1,
               and is needed to avoid integer overflow if val > sqrt(2**63). */
            val /= 2;       /* invariant -= 1024 * rvscale / 1024 */
            rv += rvscale;  /* invariant += rvscale */
        } else {
            val *= val;     /* doubles ilog2(val) */
            rvscale /= 2;   /* restores the invariant */
        }
    }

    /* now val is 1, so ilog2(val) is 0 */
    return rv;
}

/* Number of bits set in a given value.

   There's almost certainly a more efficient algorithm than this, but this one
   will do for now. */
int
popcount(unsigned long long u)
{
    int p = 0;
    int i;
    for (i = 0; i < CHAR_BIT * sizeof (unsigned long long); i++)
        if (u & (1ULL << i))
            p++;
    return p;
}

/* The next prime after the given integer. */
int
nextprime(int n)
{
    while (++n) {
        int factor;
        int sqrtn = isqrt(n);
        for (factor = 2; factor <= sqrtn; factor++)
            if (n % factor == 0)
                break;
        if (factor > sqrtn)
            break;
    }
    return n;
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
const char *
strstri(const char *str, const char *sub)
{
    const char *s1, *s2;
    int i, k;

# define TABSIZ 0x20    /* 0x40 would be case-sensitive */
    int tstr[TABSIZ], tsub[TABSIZ];    /* nibble count tables */

    /* special case: empty substring */
    if (!*sub)
        return str;

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
                return &str[i]; /* full match */
    }
    return NULL;        /* not found */
}
#endif /* STRSTRI */

/* ditto, on mutable strings*/
char *
strstri_mutable(char *str, const char *sub)
{
    const char *s1, *s2;
    int i, k;

# define TABSIZ 0x20    /* 0x40 would be case-sensitive */
    int tstr[TABSIZ], tsub[TABSIZ];    /* nibble count tables */

    /* special case: empty substring */
    if (!*sub)
        return str;

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
                return &str[i]; /* full match */
    }
    return NULL;        /* not found */
}


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

/*hacklib.c*/

