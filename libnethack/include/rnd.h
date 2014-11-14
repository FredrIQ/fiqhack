/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-11-14 */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef RND_H
# define RND_H

#define RNG_SEEDSPACE 2508
#define RNG_SEED_SIZE_BYTES 12

#define RNG_SEED_SIZE_BASE64 (RNG_SEED_SIZE_BYTES * 4 / 3)

# include "nethack_types.h"
# include "global.h"

enum rng {
    rng_display = -1,
    rng_initialseed = 0, /* never used, serves to remember the initial seed */
    rng_main = 1,
    number_of_rngs
};

static_assert(number_of_rngs * RNG_SEED_SIZE_BYTES <= RNG_SEEDSPACE,
              "too many RNGs defined");

/* Not in extern.h, because we can't guarantee that that's included before this
   header is. (And in general, moving things out of extern.h is a Good Idea
   anyway.) */
extern int rn2_on_rng(int, enum rng);
extern int rnl(int);
extern int rn2_on_display_rng(int);

extern void seed_rng_from_entropy(void);
extern boolean seed_rng_from_base64(const char [static RNG_SEED_SIZE_BASE64]);
extern void get_initial_rng_seed(char [static RNG_SEED_SIZE_BASE64]);

/* 0 <= rn2(x) < x */
static inline int
rn2(int x)
{
    return rn2_on_rng(x, rng_main);
}


/* 1 <= rnd(x) <= x */
static inline int
rnd(int x)
{
    return rn2(x) + 1;
}

/* n <= d(n,x) <= (n*x) */
static inline int
dice(int n, int x)
{
    int tmp = n;

    while (n--)
        tmp += rn2(x);
    return tmp; /* Alea iacta est. -- J.C. */
}

static inline int
rne(int x)
{
    int tmp;

    tmp = 1;
    while (tmp < 10 && !rn2(x))
        tmp++;
    return tmp;
}

static inline int
rnz(int i)
{
    long x = i;
    long tmp = 1000;

    tmp += rn2(1000);
    tmp *= rne(4);
    if (rn2(2)) {
        x *= tmp;
        x /= 1000;
    } else {
        x *= 1000;
        x /= tmp;
    }
    return (int)x;
}

#endif

/*rnd.h*/

