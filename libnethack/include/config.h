/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef CONFIG_H /* make sure the compiler does not see the typedefs twice */
#define CONFIG_H

/* swap byte order while reading and writing saves and bones files. This should
 * enable portability between architectures.
 * If you do not intend to import or export saves or bones files, this option
 * doesn't matter.
 * Endianness can be autodetected on some systems. */

/* #define IS_BIG_ENDIAN */

#ifndef WIZARD		/* allow for compile-time or Makefile changes */
# define WIZARD  "daniel" /* the person allowed to use the -D option */
#endif

#define PANICLOG "paniclog"	/* log of panic and impossible events */

#include "global.h"	/* Define everything else according to choices above */

#endif /* CONFIG_H */
