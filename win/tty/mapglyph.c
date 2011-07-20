/* NetHack may be freely redistributed.  See license for details. */

#include "nethack.h"
#include "wintty.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define array_size(x) (sizeof(x)/sizeof(x[0]))

void (*decgraphics_mode_callback)(void);
static int corpse_id;
struct nh_drawing_info *default_drawing;
static struct nh_drawing_info *d, *ibm_drawing,
                              *dec_drawing, *rogue_drawing;


static struct nh_symdef ibm_graphics_ovr[] = {
    /* bg */
    {0xb3,	"vwall",	-1}, /* meta-3, vertical rule */
    {0xc4,	"hwall",	-1}, /* meta-D, horizontal rule */
    {0xda,	"tlcorn",	-1}, /* meta-Z, top left corner */
    {0xbf,	"trcorn",	-1}, /* meta-?, top right corner */
    {0xc0,	"blcorn",	-1}, /* meta-@, bottom left */
    {0xd9,	"brcorn",	-1}, /* meta-Y, bottom right */
    {0xc5,	"crwall",	-1}, /* meta-E, cross */
    {0xc1,	"tuwall",	-1}, /* meta-A, T up */
    {0xc2,	"tdwall",	-1}, /* meta-B, T down */
    {0xb4,	"tlwall",	-1}, /* meta-4, T left */
    {0xc3,	"trwall",	-1}, /* meta-C, T right */
    {0xfa,	"ndoor",	-1}, /* meta-z, centered dot */
    {0xfe,	"vodoor",	-1}, /* meta-~, small centered square */
    {0xfe,	"hodoor",	-1}, /* meta-~, small centered square */
    {240,	"bars",		-1}, /* equivalence symbol */
    {241,	"tree",		-1}, /* plus or minus symbol */
    {0xfa,	"room",		-1}, /* meta-z, centered dot */
    {0xb0,	"corr",		-1}, /* meta-0, light shading */
    {0xb1,	"litcorr",	-1}, /* meta-1, medium shading */
    {0xf4,	"fountain",	-1}, /* meta-t, integral top half */
    {0xf7,	"pool",		-1}, /* meta-w, approx. equals */
    {0xfa,	"ice",		-1}, /* meta-z, centered dot */
    {0xf7,	"lava",		-1}, /* meta-w, approx. equals */
    {0xfa,	"vodbridge",	-1}, /* meta-z, centered dot */
    {0xfa,	"hodbridge",	-1}, /* meta-z, centered dot */
    {0xf7,	"water",	-1}, /* meta-w, approx. equals */

    /* zap */
    {0xb3,	"zap_v",	-1}, /* meta-3, vertical rule */
    {0xc4,	"zap_h",	-1}, /* meta-D, horizontal rule */

    /* swallow */
    {0xb3,	"swallow_mid_l",-1}, /* meta-3, vertical rule */
    {0xb3,	"swallow_mid_r",-1}, /* meta-3, vertical rule */
    
    /* explosion */
    {0xb3,	"exp_mid_l",	-1}, /* meta-3, vertical rule */
    {0xb3,	"exp_mid_r",	-1}, /* meta-3, vertical rule */
};


static struct nh_symdef dec_graphics_ovr[] = {
    /* bg */
    {0xf8,	"vwall",	-1}, /* meta-x, vertical rule */
    {0xf1,	"hwall",	-1}, /* meta-q, horizontal rule */
    {0xec,	"tlcorn",	-1}, /* meta-l, top left corner */
    {0xeb,	"trcorn",	-1}, /* meta-k, top right corner */
    {0xed,	"blcorn",	-1}, /* meta-m, bottom left */
    {0xea,	"brcorn",	-1}, /* meta-j, bottom right */
    {0xee,	"crwall",	-1}, /* meta-n, cross */
    {0xf6,	"tuwall",	-1}, /* meta-v, T up */
    {0xf7,	"tdwall",	-1}, /* meta-w, T down */
    {0xf5,	"tlwall",	-1}, /* meta-u, T left */
    {0xf4,	"trwall",	-1}, /* meta-t, T right */
    {0xfe,	"ndoor",	-1}, /* meta-~, centered dot */
    {0xe1,	"vodoor",	-1}, /* meta-a, solid block */
    {0xe1,	"hodoor",	-1}, /* meta-a, solid block */
    {0xfb,	"bars",		-1}, /* meta-{, small pi */
    {0xe7,	"tree",		-1}, /* meta-g, plus-or-minus */
    {0xfe,	"room",		-1}, /* meta-~, centered dot */
    {0xf9,	"upladder",	-1}, /* meta-y, greater-than-or-equals */
    {0xfa,	"dnladder",	-1}, /* meta-z, less-than-or-equals */
    {0xe0,	"pool",		-1}, /* meta-\, diamond */
    {0xfe,	"ice",		-1}, /* meta-~, centered dot */
    {0xe0,	"lava",		-1}, /* meta-\, diamond */
    {0xfe,	"vodbridge",	-1}, /* meta-~, centered dot */
    {0xfe,	"hodbridge",	-1}, /* meta-~, centered dot */
    {0xe0,	"water",	-1}, /* meta-\, diamond */
    
    /* zap */
    {0xf8,	"zap_v",	-1}, /* meta-x, vertical rule */
    {0xf1,	"zap_h",	-1}, /* meta-q, horizontal rule */
    
    /* swallow */
    {0xef,	"swallow_top_c",-1}, /* meta-o, high horizontal line */
    {0xf8,	"swallow_mid_l",-1}, /* meta-x, vertical rule */
    {0xf8,	"swallow_mid_r",-1}, /* meta-x, vertical rule */
    {0xf3,	"swallow_bot_c",-1}, /* meta-s, low horizontal line */
    
    /* explosion */
    {0xef,	"exp_top_c",	-1}, /* meta-o, high horizontal line */
    {0xf8,	"exp_mid_l",	-1}, /* meta-x, vertical rule */
    {0xf8,	"exp_mid_r",	-1}, /* meta-x, vertical rule */
    {0xf3,	"exp_bot_c",	-1}, /* meta-s, low horizontal line */
};


static struct nh_symdef rogue_graphics_ovr[] = {
    {'+',	"vodoor",	-1},
    {'+',	"hodoor",	-1},
    {'+',	"ndoor",	-1},
    {'%',	"upstair",	-1},
    {'%',	"dnstair",	-1},
    
    {'*',	"gold piece",	-1},
    
    {':',	"corpse",	-1}, /* the 2 most common food items... */
    {':',	"food ration",	-1}
};


static boolean apply_override_list(struct nh_symdef *list, int len,
				   const struct nh_symdef *ovr)
{
    int i;
    for (i = 0; i < len; i++)
	if (!strcmp(list[i].symname, ovr->symname)) {
	    list[i].ch = ovr->ch;
	    if (ovr->color != -1)
		list[i].color = ovr->color;
	    return TRUE;
	}
    return FALSE;
}


static void apply_override(struct nh_drawing_info *di,
			   const struct nh_symdef *ovr, int olen)
{
    int i;
    boolean ok;
    
    for (i = 0; i < olen; i++) {
	ok = FALSE;
	ok |= apply_override_list(di->bgelements, di->num_bgelements, &ovr[i]);
	ok |= apply_override_list(di->traps, di->num_traps, &ovr[i]);
	ok |= apply_override_list(di->objects, di->num_objects, &ovr[i]);
	ok |= apply_override_list(di->effects, di->num_effects, &ovr[i]);
	ok |= apply_override_list(di->explsyms, NUMEXPCHARS, &ovr[i]);
	ok |= apply_override_list(di->swallowsyms, NUMSWALLOWCHARS, &ovr[i]);
	ok |= apply_override_list(di->zapsyms, NUMZAPCHARS, &ovr[i]);
	
	if (!ok)
	    fprintf(stdout, "sym override %s could not be applied\n", ovr[i].symname);
    }
}


static struct nh_symdef *copy_symarray(const struct nh_symdef *src, int len)
{
    int i;
    struct nh_symdef *copy = malloc(len * sizeof(struct nh_symdef));
    memcpy(copy, src, len * sizeof(struct nh_symdef));
    
    for (i = 0; i < len; i++)
	copy[i].symname = strdup(src[i].symname);
    
    return copy;
}


static struct nh_drawing_info *copy_drawing_info(const struct nh_drawing_info *orig)
{
    struct nh_drawing_info *copy = malloc(sizeof(struct nh_drawing_info));
    memcpy(copy, orig, sizeof(struct nh_drawing_info));
    
    copy->bgelements = copy_symarray(orig->bgelements, copy->num_bgelements);
    copy->traps = copy_symarray(orig->traps, copy->num_traps);
    copy->objects = copy_symarray(orig->objects, copy->num_objects);
    copy->monsters = copy_symarray(orig->monsters, copy->num_monsters);
    copy->warnings = copy_symarray(orig->warnings, copy->num_warnings);
    copy->invis = copy_symarray(orig->invis, 1);
    copy->effects = copy_symarray(orig->effects, copy->num_effects);
    copy->expltypes = copy_symarray(orig->expltypes, copy->num_expltypes);
    copy->explsyms = copy_symarray(orig->explsyms, NUMEXPCHARS);
    copy->zaptypes = copy_symarray(orig->zaptypes, copy->num_zaptypes);
    copy->zapsyms = copy_symarray(orig->zapsyms, NUMZAPCHARS);
    copy->swallowsyms = copy_symarray(orig->swallowsyms, NUMSWALLOWCHARS);
    
    return copy;
}


void init_displaychars(void)
{
    int i;
    struct nh_drawing_info *dinfo = nh_get_drawing_info();
    
    default_drawing = copy_drawing_info(dinfo);
    ibm_drawing = copy_drawing_info(dinfo);
    dec_drawing = copy_drawing_info(dinfo);
    rogue_drawing = copy_drawing_info(dinfo);
    
    apply_override(ibm_drawing, ibm_graphics_ovr, array_size(ibm_graphics_ovr));
    apply_override(dec_drawing, dec_graphics_ovr, array_size(dec_graphics_ovr));
    apply_override(rogue_drawing, rogue_graphics_ovr, array_size(rogue_graphics_ovr));
    
    d = default_drawing;
    
    /* find objects that need special treatment */
    for (i = 0; i < d->num_objects; i++) {
	if (!strcmp("corpse", d->objects[i].symname))
	    corpse_id = i;
    }
    
    /* options are parsed before display is initialized, so redo switch */
    switch_graphics(ui_flags.graphics);
}


void mapglyph(struct nh_dbuf_entry *dbe, int *ochar, int *ocolor,
	      int x, int y)
{
    int id;
    int color = NO_COLOR;
    uchar ch;

    if (dbe->effect) {
	id = NH_EFFECT_ID(dbe->effect);
	
	switch (NH_EFFECT_TYPE(dbe->effect)) {
	    case E_EXPLOSION:
		ch = d->explsyms[id % NUMEXPCHARS].ch;
		color = d->expltypes[id / NUMEXPCHARS].color;
		break;
		
	    case E_SWALLOW:
		ch = d->swallowsyms[id & 0x7].ch;
		color = d->monsters[id >> 3].color;
		break;
		
	    case E_ZAP:
		ch = d->zapsyms[id & 0x3].ch;
		color = d->zaptypes[id >> 2].color;
		break;
		
	    case E_MISC:
		ch = d->effects[id].ch;
		color = d->effects[id].color;
		break;
	}
	
    } else if (dbe->invis) {
	ch = d->invis->ch;
	color = d->invis->color;
	
    } else if (dbe->mon) {
	if (dbe->mon > d->num_monsters && (dbe->monflags & MON_WARNING)) {
	    id = dbe->mon - 1 - d->num_monsters;
	    ch = d->warnings[id].ch;
	    color = d->warnings[id].color;
	} else {
	    id = dbe->mon - 1;
	    ch = d->monsters[id].ch;
	    color = d->monsters[id].color;
	}
	
    } else if (dbe->obj) {
	id = dbe->obj - 1;
	if (id == corpse_id) {
	    ch = d->objects[id].ch;
	    color = d->monsters[dbe->obj_mn - 1].color;
	} else {
	    ch = d->objects[id].ch;
	    color = d->objects[id].color;
	}
	
    } else if (dbe->trap) {
	id = dbe->trap - 1;
	ch = d->traps[id].ch;
	color = d->traps[id].color;
	
    } else if (dbe->bg) {
	id = dbe->bg;
	ch = d->bgelements[id].ch;
	color = d->bgelements[id].color;
    } else
	ch = 0;

    /* Turn off color if rogue level w/o PC graphics. */
    if (levelmode == LDM_ROGUE && ui_flags.graphics != IBM_GRAPHICS)
	color = NO_COLOR;

    *ochar = (int)ch;
    *ocolor = color;
    return;
}


void set_rogue_level(boolean enable)
{
    if (enable)
	d = rogue_drawing;
    else
	switch_graphics(ui_flags.graphics);
}


void switch_graphics(enum nh_text_mode mode)
{
    switch (mode) {
	default:
	case ASCII_GRAPHICS:
	    d = default_drawing;
	    break;
	    
	case IBM_GRAPHICS:
/*
 * Use the nice IBM Extended ASCII line-drawing characters (codepage 437).
 * This fails on UTF-8 terminals and on terminals that use other codepages.
 */
	    d = ibm_drawing;
	    break;
	    
	case DEC_GRAPHICS:
/*
 * Use the VT100 line drawing character set.
 * VT100 emulation is very common on Unix, so this should generally work.
 */
	    d = dec_drawing;
	    if (decgraphics_mode_callback)
		(*decgraphics_mode_callback)();
	    
	    break;
    }
}

/*mapglyph.c*/
