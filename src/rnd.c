/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* "Rand()"s definition is determined by [OS]conf.h */
#if defined(UNIX) || defined(RANDOM)
#define RND(x)	(int)(Rand() % (long)(x))
#else
/* Good luck: the bottom order bits are cyclic. */
#define RND(x)	(int)((Rand()>>3) % (x))
#endif


/* 0 <= rn2(x) < x */
int rn2(int x)
{
#ifdef DEBUG
	if (x <= 0) {
		impossible("rn2(%d) attempted", x);
		return 0;
	}
	x = RND(x);
	return x;
#else
	return RND(x);
#endif
}

/* 0 <= rnl(x) < x; sometimes subtracting Luck */
/* good luck approaches 0, bad luck approaches (x-1) */
int rnl(int x)
{
	int i;

#ifdef DEBUG
	if (x <= 0) {
		impossible("rnl(%d) attempted", x);
		return 0;
	}
#endif
	i = RND(x);

	if (Luck && rn2(50 - Luck)) {
	    i -= (x <= 15 && Luck >= -5 ? Luck/3 : Luck);
	    if (i < 0) i = 0;
	    else if (i >= x) i = x-1;
	}

	return i;
}


/* 1 <= rnd(x) <= x */
int rnd(int x)
{
#ifdef DEBUG
	if (x <= 0) {
		impossible("rnd(%d) attempted", x);
		return 1;
	}
	x = RND(x)+1;
	return x;
#else
	return RND(x)+1;
#endif
}


/* n <= d(n,x) <= (n*x) */
int d(int n, int x)
{
	int tmp = n;

#ifdef DEBUG
	if (x < 0 || n < 0 || (x == 0 && n != 0)) {
		impossible("d(%d,%d) attempted", n, x);
		return 1;
	}
#endif
	while(n--) tmp += RND(x);
	return tmp; /* Alea iacta est. -- J.C. */
}


int rne(int x)
{
	int tmp, utmp;

	utmp = (u.ulevel < 15) ? 5 : u.ulevel/3;
	tmp = 1;
	while (tmp < utmp && !rn2(x))
		tmp++;
	return tmp;
}

int rnz(int i)
{
	long x = i;
	long tmp = 1000;

	tmp += rn2(1000);
	tmp *= rne(4);
	if (rn2(2)) { x *= tmp; x /= 1000; }
	else { x *= 1000; x /= tmp; }
	return (int)x;
}

/*rnd.c*/
