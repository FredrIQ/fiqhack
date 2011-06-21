/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef FUNC_TAB_H
#define FUNC_TAB_H


struct cmd_desc {
	const char *name;
	const char *desc;
	char defkey, altkey;
	boolean can_if_buried;
	const void *func;
	unsigned int flags;
	const char *text;
};

#endif /* FUNC_TAB_H */
