/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef UNIX
#ifndef UNIXCONF_H
#define UNIXCONF_H

/*
 * The remainder of the file should not need to be changed.
 */
#include <time.h>

#include "system.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#ifndef index	/* some systems seem to do this for you */
#define index	strchr
#endif

#ifndef rindex
#define rindex	strrchr
#endif

#endif /* UNIXCONF_H */
#endif /* UNIX */
