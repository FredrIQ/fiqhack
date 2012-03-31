/* NitroHack may be freely redistributed.  See license for details. */

#ifndef RND_H
#define RND_H

#define RND(x)	(int)(mt_random() % (long)(x))

/* 0 <= rn2(x) < x */
static inline int rn2(int x)
{
	return RND(x);
}

/* 0 <= rnl(x) < x; sometimes subtracting Luck */
/* good luck approaches 0, bad luck approaches (x-1) */
static inline int rnl(int x)
{
	int i;

	i = RND(x);

	if (Luck && rn2(50 - Luck)) {
	    i -= (x <= 15 && Luck >= -5 ? Luck/3 : Luck);
	    if (i < 0) i = 0;
	    else if (i >= x) i = x-1;
	}

	return i;
}


/* 1 <= rnd(x) <= x */
static inline int rnd(int x)
{
	return RND(x)+1;
}


/* n <= d(n,x) <= (n*x) */
static inline int dice(int n, int x)
{
	int tmp = n;

	while (n--)
	    tmp += RND(x);
	return tmp; /* Alea iacta est. -- J.C. */
}


static inline int rne(int x)
{
	int tmp;

	tmp = 1;
	while (tmp < 10 && !rn2(x))
		tmp++;
	return tmp;
}

static inline int rnz(int i)
{
	long x = i;
	long tmp = 1000;

	tmp += rn2(1000);
	tmp *= rne(4);
	if (rn2(2)) { x *= tmp; x /= 1000; }
	else { x *= 1000; x /= tmp; }
	return (int)x;
}

#undef RND

#endif

/*rnd.h*/
