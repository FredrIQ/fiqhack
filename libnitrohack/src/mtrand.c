/* This is the ``Mersenne Twister'' random number generator MT19937, which
   generates pseudorandom integers uniformly distributed in 0..(2^32 - 1)
   starting from any odd seed in 0..(2^32 - 1).  This version is adapted from
   a recode by Shawn Cokus (Cokus@math.washington.edu).

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published by
   the Free Software Foundation (either version 2 of the License or, at your
   option, any later version).  This library is distributed in the hope that
   it will be useful, but WITHOUT ANY WARRANTY, without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. */

#include "hack.h"

#define N              624
#define M              397
#define K              (0x9908B0DFU)
#define hiBit(u)       ((u) & 0x80000000U)
#define loBit(u)       ((u) & 0x00000001U)
#define loBits(u)      ((u) & 0x7FFFFFFFU)
#define mixBits(u, v)  (hiBit(u) | loBits(v))

static unsigned int state[N+1];     /* state vector + 1 extra to not violate ANSI C */
static unsigned int *next;          /* next random value is computed from here */
static int          left = -1;      /* can *next++ this many times before reloading */
static int dpos;


void mt_srand(unsigned int seed)
{
    /* We initialize state[0..(N-1)] via the generator
     *
     *   x_new = (69069 * x_old) mod 2^32
     *
     * from Line 15 of Table 1, p. 106, Sec. 3.3.4 of Knuth's
     * _The Art of Computer Programming_, Volume 2, 3rd ed. */
    int j;
    unsigned int x = (seed | 1) & 0xFFFFFFFF;
    unsigned int *s = state;

    left = 0;
    *s++ = x;
    for (j = N; j > 1; --j)
	 *s++ = (x*=69069U) & 0xFFFFFFFF;
}


static unsigned int mt_reload(void)
{
    unsigned int *p0=state, *p2=state+2, *pM=state+M, s0, s1;
    int j;

    left = N-1;
    next = state+1;
    dpos = 0;
    
    s0 = state[0];
    s1 = state[1];
    for (j = N-M+1; j > 1; --j) {
	*p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);
	s0 = s1;
	s1 = *p2++;
    }

    pM = state;
    for (j=M; j > 1; --j) {
	*p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);
	s0 = s1;
	s1 = *p2++;
    }

    s1 = state[0];
    *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 <<  7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return s1 ^ (s1 >> 18);
}


unsigned int mt_random(void)
{
    unsigned int y;

    if (--left < 0)
        return mt_reload();

    y  = *next++;
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    return y ^ (y >> 18);
}


unsigned int mt_nextstate(void)
{
    if (!left)
	return 0;
    return *next;
}


void save_mt_state(struct memfile *mf)
{
    unsigned int i, pos = next - state;
    
    for (i = 0; i < N+1; i++)
	mwrite32(mf, state[i]);
    mwrite32(mf, pos);
    mwrite32(mf, left);
}


void restore_mt_state(struct memfile *mf)
{
    unsigned int i, pos;
    for (i = 0; i < N+1; i++)
	state[i] = mread32(mf);
    pos = mread32(mf);
    left = mread32(mf);
    next = &state[pos];
}


/* A special-purpose rng for random_monster(), random_object(), random_trap()
 * and coyotename()
 * These functions are used for displaying hallucinated things.
 * Re-using "used up" random values is not a problem for that and is preferable
 * to messing with the system rng, while running a second mt with it's own state
 * seems like overkill. */
int display_rng(int x)
{
    static unsigned lcg_val = 1073741827u;
    unsigned int num = state[dpos];
    dpos = (dpos + 1) % N;
    /* a simple lcg generator which modifies the outpur from the mersenn sequence
     * this is pure paranoia, induced by ais523, in order to avoid leaking info
     * about the main rng sequence during hallucination
     * 4294967291 is the largest prime under 2^32; 65537 and 7 are also prime.
     * This gives the lcg a full period according to Hull & Dobell.
     */
    lcg_val = ((lcg_val * 7 + 65537) % 4294967291u) ^ num;
    
    return lcg_val % x;
}

