/* Copyright (c) David Cohrs, 1991				  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "color.h"

int explcolors[] = {
	CLR_BLACK,	/* dark    */
	CLR_GREEN,	/* noxious */
	CLR_BROWN,	/* muddy   */
	CLR_BLUE,	/* wet     */
	CLR_MAGENTA,	/* magical */
	CLR_ORANGE,	/* fiery   */
	CLR_WHITE,	/* frosty  */
};

void mapglyph(struct nh_dbuf_entry *dbe, int *ochar, int *ocolor,
	      int x, int y)
{
    int id;
    int color = NO_COLOR;
    uchar ch;

    if (dbe->effect) {
	int etype = dbe->effect >> 16;
	id = (dbe->effect & 0xffff) - 1;
	
	switch (etype) {
	    case E_EXPLOSION:
		ch = showsyms[(id % MAXEXPCHARS) + S_explode1];
		color = explcolors[id / MAXEXPCHARS];
		break;
		
	    case E_SWALLOW:
		ch = showsyms[S_sw_tl + (id & 0x7)];
		color = mons[id >> 3].mcolor;
		break;
		
	    case E_ZAP:
		ch = showsyms[S_vbeam + (id & 0x3)];
		color = zapcolors[id >> 2];
		break;
		
	    case E_MISC:
		ch = showsyms[id];
		color = defsyms[id].color;
		break;
	}
	
    } else if (dbe->invis) {
	ch = DEF_INVISIBLE;
	color = NO_COLOR;
	
    } else if (dbe->mon) {
	if (dbe->mon > NUMMONS && (dbe->monflags & MON_WARNING)) {
	    id = dbe->mon - 1 - NUMMONS;
	    ch = warnsyms[id];
	    color = def_warnsyms[id].color;
	} else {
	    id = dbe->mon - 1;
	    ch = monsyms[(int)mons[id].mlet];
	    color = mons[id].mcolor;
	}
	
    } else if (dbe->obj) {
	id = dbe->obj - 1;
	if (id == CORPSE) {
	    ch = oc_syms[(int)objects[CORPSE].oc_class];
	    color = mons[dbe->obj_mn - 1].mcolor;
	} else if (id == BOULDER) {
	    ch = iflags.bouldersym;
	    color = objects[id].oc_color;
	} else {
	    ch = oc_syms[(int)objects[id].oc_class];
	    color = objects[id].oc_color;
	}
	
    } else if (dbe->trap) {
	id = dbe->trap - 1 + S_arrow_trap;
	ch = showsyms[id];
	color = defsyms[id].color;
	
    } else if (dbe->bg) {
	id = dbe->bg;
	ch = showsyms[id];
	/* provide a visible difference if normal and lit corridor
	 * use the same symbol */
	if (id == S_litcorr && ch == showsyms[S_corr])
	    color = CLR_WHITE;
	else
	    color = defsyms[id].color;
    }

    /* Turn off color if rogue level w/o PC graphics. */
    if (Is_rogue_level(&u.uz) && !iflags2.IBMgraphics)
	color = NO_COLOR;

    *ochar = (int)ch;
    *ocolor = color;
    return;
}

/*mapglyph.c*/
