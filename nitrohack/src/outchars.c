/* Copyright (c) Daniel Thaler, 2011 */
/* NitroHack may be freely redistributed.  See license for details. */

/* NOTE: This file is utf-8 encoded; saving with a non utf-8 aware editor WILL
 * damage some symbols */

#include "nhcurses.h"
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>


#define array_size(x) (sizeof(x)/sizeof(x[0]))


static int corpse_id;
struct curses_drawing_info *default_drawing, *cur_drawing;
static struct curses_drawing_info *unicode_drawing, *rogue_drawing;


static struct curses_symdef rogue_graphics_ovr[] = {
    {"vodoor",	-1,	{0x002B, 0},	'+'},
    {"hodoor",	-1,	{0x002B, 0},	'+'},
    {"ndoor",	-1,	{0x002B, 0},	'+'},
    {"upstair",	-1,	{0x0025, 0},	'%'},
    {"dnstair",	-1,	{0x0025, 0},	'%'},
    {"upsstair",-1,	{0x0025, 0},	'%'},
    {"dnsstair",-1,	{0x0025, 0},	'%'},
    
    {"gold piece",-1,	{0x002A, 0},	'*'},
    
    {"corpse",	-1,	{0x003A, 0},	':'}, /* the 2 most common food items... */
    {"food ration",-1,	{0x003A, 0},	':'}
};


static struct curses_symdef unicode_graphics_ovr[] = {
    /* bg */
    {"vwall",	-1,	{0x2502, 0},	0},	/* │ vertical rule */
    {"hwall",	-1,	{0x2500, 0},	0},	/* ─ horizontal rule */
    {"tlcorn",	-1,	{0x250C, 0},	0},	/* ┌ top left corner */
    {"trcorn",	-1,	{0x2510, 0},	0},	/* ┐ top right corner */
    {"blcorn",	-1,	{0x2514, 0},	0},	/* └ bottom left */
    {"brcorn",	-1,	{0x2518, 0},	0},	/* ┘ bottom right */
    {"crwall",	-1,	{0x253C, 0},	0},	/* ┼ cross */
    {"tuwall",	-1,	{0x2534, 0},	0},	/* T up */
    {"tdwall",	-1,	{0x252C, 0},	0},	/* T down */
    {"tlwall",	-1,	{0x2524, 0},	0},	/* T left */
    {"trwall",	-1,	{0x251C, 0},	0},	/* T right */
    {"ndoor",	-1,	{0x00B7, 0},	0},	/* · centered dot */
    {"vodoor",	-1,	{0x2588, 0},	0},	/* █ solid block */
    {"hodoor",	-1,	{0x2588, 0},	0},	/* █ solid block */
    {"bars",	-1,	{0x2261, 0},	0},	/* ≡ equivalence symbol */
    {"fountain",-1,	{0x2320, 0},	0},	/* ⌠ top half of integral */
    {"room",	-1,	{0x00B7, 0},	0},	/* · centered dot */
    {"darkroom",-1,	{0x00B7, 0},	0},	/* · centered dot */
    {"corr",	-1,	{0x2591, 0},	0},	/* ░ light shading */
    {"litcorr",	-1,	{0x2592, 0},	0},	/* ▒ medium shading */
    {"upladder",-1,	{0x2264, 0},	0},	/* ≤ less-than-or-equals */
    {"dnladder",-1,	{0x2265, 0},	0},	/* ≥ greater-than-or-equals */
    {"altar",	-1,	{0x03A9, 0},	0},	/* Ω GREEK CAPITAL LETTER OMEGA */
    {"grave",	-1,	{0x2020, 0},	0},	/* † DAGGER */
    {"ice",	-1,	{0x00B7, 0},	0},	/* · centered dot */
    {"vodbridge",-1,	{0x00B7, 0},	0},	/* · centered dot */
    {"hodbridge",-1,	{0x00B7, 0},	0},	/* · centered dot */
    
    /* zap */
    {"zap_v",	-1,	{0x2502, 0},	0},	/* │ vertical rule */
    {"zap_h",	-1,	{0x2500, 0},	0},	/* ─ horizontal rule */
    
    /* swallow */
    {"swallow_top_c",-1,{0x2594, 0},	0},	/* ▔ UPPER ONE EIGHTH BLOCK */
    {"swallow_mid_l",-1,{0x258F, 0},	0},	/* ▏ LEFT ONE EIGHTH BLOCK */
    {"swallow_mid_r",-1,{0x2595, 0},	0},	/* ▕ RIGHT ONE EIGHTH BLOCK */
    {"swallow_bot_c",-1,{0x2581, 0},	0},	/* ▁ LOWER ONE EIGHTH BLOCK */
    
    /* explosion */
    {"exp_top_c", -1,	{0x2594, 0},	0},	/* ▔ UPPER ONE EIGHTH BLOCK */
    {"exp_mid_l", -1,	{0x258F, 0},	0},	/* ▏ LEFT ONE EIGHTH BLOCK */
    {"exp_mid_r", -1,	{0x2595, 0},	0},	/* ▕ RIGHT ONE EIGHTH BLOCK */
    {"exp_bot_c", -1,	{0x2581, 0},	0},	/* ▁ LOWER ONE EIGHTH BLOCK */
    
    /* traps */
    {"web",	-1,	{0x00A4, 0},	0},	/* ¤ currency symbol */

#if !defined(WIN32)
    {"pool",	-1,	{0x224B, 0},	0},	/* ≋ triple tilde */
    {"lava",	-1,	{0x224B, 0},	0},	/* ≋ triple tilde */
    {"water",	-1,	{0x224B, 0},	0},	/* ≋ triple tilde */
    {"tree",	-1,	{0x03a8, 0},	0},	/* Ψ GREEK CAPITAL LETTER PSI */
    {"upsstair",-1,     {0x227E, 0},    0},     /* ≾ precedes-or-equivalent-to */
    {"dnsstair",-1,     {0x227F, 0},    0},     /* ≿ succeeds-or-equivalent-to */
#else
    {"upsstair",-1,	{0x2264, 0},	0},	/* ≤ less-than-or-equals */
    {"dnsstair",-1,	{0x2265, 0},	0},	/* ≥ greater-than-or-equals */
#endif
    /* objects */
    {"boulder", -1,	{0x0030, 0},	0},	/* 0 zero */
};


static nh_bool apply_override_list(struct curses_symdef *list, int len,
				   const struct curses_symdef *ovr, nh_bool cust)
{
    int i;
    for (i = 0; i < len; i++)
	if (!strcmp(list[i].symname, ovr->symname)) {
	    if (ovr->unichar[0])
		memcpy(list[i].unichar, ovr->unichar, sizeof(wchar_t) * CCHARW_MAX);
	    if (ovr->ch)
		list[i].ch = ovr->ch;
	    if (ovr->color != -1)
		list[i].color = ovr->color;
	    list[i].custom = cust;
	    return TRUE;
	}
    return FALSE;
}


static void apply_override(struct curses_drawing_info *di,
			   const struct curses_symdef *ovr, int olen, nh_bool cust)
{
    int i;
    nh_bool ok;
    
    for (i = 0; i < olen; i++) {
	ok = FALSE;
	/* the override will effect exactly one of the symbol lists */
	ok |= apply_override_list(di->bgelements, di->num_bgelements, &ovr[i], cust);
	ok |= apply_override_list(di->traps, di->num_traps, &ovr[i], cust);
	ok |= apply_override_list(di->objects, di->num_objects, &ovr[i], cust);
	ok |= apply_override_list(di->monsters, di->num_monsters, &ovr[i], cust);
	ok |= apply_override_list(di->warnings, di->num_warnings, &ovr[i], cust);
	ok |= apply_override_list(di->invis, 1, &ovr[i], cust);
	ok |= apply_override_list(di->effects, di->num_effects, &ovr[i], cust);
	ok |= apply_override_list(di->expltypes, di->num_expltypes, &ovr[i], cust);
	ok |= apply_override_list(di->explsyms, NUMEXPCHARS, &ovr[i], cust);
	ok |= apply_override_list(di->zaptypes, di->num_zaptypes, &ovr[i], cust);
	ok |= apply_override_list(di->zapsyms, NUMZAPCHARS, &ovr[i], cust);
	ok |= apply_override_list(di->swallowsyms, NUMSWALLOWCHARS, &ovr[i], cust);
	
	if (!ok)
	    fprintf(stdout, "sym override %s could not be applied\n", ovr[i].symname);
    }
}


static struct curses_symdef *load_nh_symarray(const struct nh_symdef *src, int len)
{
    int i;
    struct curses_symdef *copy = malloc(len * sizeof(struct curses_symdef));
    memset(copy, 0, len * sizeof(struct curses_symdef));
    
    for (i = 0; i < len; i++) {
	copy[i].symname = strdup(src[i].symname);
	copy[i].ch = src[i].ch;
	copy[i].color = src[i].color;
	
	/* this works because ASCII 0x?? (for ?? < 128) == Unicode U+00?? */
	copy[i].unichar[0] = (wchar_t)src[i].ch; 
    }
    
    return copy;
}


static struct curses_drawing_info *load_nh_drawing_info(const struct nh_drawing_info *orig)
{
    struct curses_drawing_info *copy = malloc(sizeof(struct curses_drawing_info));
    
    copy->num_bgelements = orig->num_bgelements;
    copy->num_traps = orig->num_traps;
    copy->num_objects = orig->num_objects;
    copy->num_monsters = orig->num_monsters;
    copy->num_warnings = orig->num_warnings;
    copy->num_expltypes = orig->num_expltypes;
    copy->num_zaptypes = orig->num_zaptypes;
    copy->num_effects = orig->num_effects;
    copy->bg_feature_offset = orig->bg_feature_offset;
    
    copy->bgelements = load_nh_symarray(orig->bgelements, orig->num_bgelements);
    copy->traps = load_nh_symarray(orig->traps, orig->num_traps);
    copy->objects = load_nh_symarray(orig->objects, orig->num_objects);
    copy->monsters = load_nh_symarray(orig->monsters, orig->num_monsters);
    copy->warnings = load_nh_symarray(orig->warnings, orig->num_warnings);
    copy->invis = load_nh_symarray(orig->invis, 1);
    copy->effects = load_nh_symarray(orig->effects, orig->num_effects);
    copy->expltypes = load_nh_symarray(orig->expltypes, orig->num_expltypes);
    copy->explsyms = load_nh_symarray(orig->explsyms, NUMEXPCHARS);
    copy->zaptypes = load_nh_symarray(orig->zaptypes, orig->num_zaptypes);
    copy->zapsyms = load_nh_symarray(orig->zapsyms, NUMZAPCHARS);
    copy->swallowsyms = load_nh_symarray(orig->swallowsyms, NUMSWALLOWCHARS);
    
    return copy;
}


static void read_sym_line(char *line)
{
    struct curses_symdef ovr;
    char symname[64];
    char *bp;
    
    if (!strlen(line) || line[0] != '!' || line[1] != '"')
	return;
    
    line++; /* skip the ! */
    memset(&ovr, 0, sizeof(struct curses_symdef));
    
    /* line format: "symbol name" color unicode [combining marks] */
    bp = &line[1];
    while (*bp && *bp != '"') bp++; /* find the end of the symname */
    strncpy(symname, &line[1], bp - &line[1]);
    symname[bp - &line[1]] = '\0';
    ovr.symname = symname;
    bp++; /* go past the " at the end of the symname */
    
    while (*bp && isspace(*bp)) bp++; /* find the start of the next value */
    sscanf(bp, "%d", &ovr.color);
    
    while (*bp && !isspace(*bp)) bp++; /* go past the previous value */
    sscanf(bp, "%x", &ovr.unichar[0]);
    
    apply_override(unicode_drawing, &ovr, 1,  TRUE);
}


static void read_unisym_config(void)
{
    fnchar filename[BUFSZ];
    char *data, *line;
    int fd, size;
    
    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
	return;
    fnncat(filename, FN("unicode.conf"), BUFSZ);
    
    fd = sys_open(filename, O_RDONLY, 0);
    if (fd == -1)
	return;
    
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    data = malloc(size + 1);
    read(fd, data, size);
    data[size] = '\0';
    close(fd);
    
    line = strtok(data, "\r\n");
    while (line) {
	read_sym_line(line);
	
	line = strtok(NULL, "\r\n");
    }
    
    free(data);
}


static void write_symlist(int fd, const struct curses_symdef *list, int len)
{
    char buf[BUFSZ];
    int i;
    
    for (i = 0; i < len; i++) {
	sprintf(buf, "%c\"%s\"\t%d\t%04x\n", list[i].custom ? '!' : '#',
		list[i].symname, list[i].color, list[i].unichar[0]);
	write(fd, buf, strlen(buf));
    }
}

static const char uniconf_header[] =
"# Unicode symbol configuration for NitroHack\n"
"# Lines that begin with '#' are commented out.\n"
"# Change the '#' to an '!' to activate a line.\n";

static void write_unisym_config(void)
{
    fnchar filename[BUFSZ];
    int fd;
    
    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
	return;
    fnncat(filename, FN("unicode.conf"), BUFSZ);
    
    fd = sys_open(filename, O_TRUNC | O_CREAT | O_RDWR, 0660);
    if (fd == -1)
	return;
    
    write(fd, uniconf_header, strlen(uniconf_header));
    write_symlist(fd, unicode_drawing->bgelements, unicode_drawing->num_bgelements);
    write_symlist(fd, unicode_drawing->traps, unicode_drawing->num_traps);
    write_symlist(fd, unicode_drawing->objects, unicode_drawing->num_objects);
    write_symlist(fd, unicode_drawing->monsters, unicode_drawing->num_monsters);
    write_symlist(fd, unicode_drawing->warnings, unicode_drawing->num_warnings);
    write_symlist(fd, unicode_drawing->invis, 1);
    write_symlist(fd, unicode_drawing->effects, unicode_drawing->num_effects);
    write_symlist(fd, unicode_drawing->expltypes, unicode_drawing->num_expltypes);
    write_symlist(fd, unicode_drawing->explsyms, NUMEXPCHARS);
    write_symlist(fd, unicode_drawing->zaptypes, unicode_drawing->num_zaptypes);
    write_symlist(fd, unicode_drawing->zapsyms, NUMZAPCHARS);
    write_symlist(fd, unicode_drawing->swallowsyms, NUMSWALLOWCHARS);
    
    close(fd);
}


void init_displaychars(void)
{
    int i;
    struct nh_drawing_info *dinfo = nh_get_drawing_info();
    
    default_drawing = load_nh_drawing_info(dinfo);
    unicode_drawing = load_nh_drawing_info(dinfo);
    rogue_drawing = load_nh_drawing_info(dinfo);
    
    apply_override(unicode_drawing, unicode_graphics_ovr,
		   array_size(unicode_graphics_ovr), FALSE);
    apply_override(rogue_drawing, rogue_graphics_ovr, array_size(rogue_graphics_ovr), FALSE);
    
    read_unisym_config();
    
    cur_drawing = default_drawing;
    
    /* find objects that need special treatment */
    for (i = 0; i < cur_drawing->num_objects; i++) {
	if (!strcmp("corpse", cur_drawing->objects[i].symname))
	    corpse_id = i;
    }
    
    /* options are parsed before display is initialized, so redo switch */
    switch_graphics(settings.graphics);
}


static void free_symarray(struct curses_symdef *array, int len)
{
    int i;
    for (i = 0; i < len; i++)
	free((char*)array[i].symname);
    
    free(array);
}


static void free_drawing_info(struct curses_drawing_info *di)
{
    free_symarray(di->bgelements, di->num_bgelements);
    free_symarray(di->traps, di->num_traps);
    free_symarray(di->objects, di->num_objects);
    free_symarray(di->monsters, di->num_monsters);
    free_symarray(di->warnings, di->num_warnings);
    free_symarray(di->invis, 1);
    free_symarray(di->effects, di->num_effects);
    free_symarray(di->expltypes, di->num_expltypes);
    free_symarray(di->explsyms, NUMEXPCHARS);
    free_symarray(di->zaptypes, di->num_zaptypes);
    free_symarray(di->zapsyms, NUMZAPCHARS);
    free_symarray(di->swallowsyms, NUMSWALLOWCHARS);
    
    free(di);
}


void free_displaychars(void)
{
    write_unisym_config();
    
    free_drawing_info(default_drawing);
    free_drawing_info(unicode_drawing);
    free_drawing_info(rogue_drawing);
    
    default_drawing = rogue_drawing = NULL;
}


int mapglyph(struct nh_dbuf_entry *dbe, struct curses_symdef *syms)
{
    int id, count = 0;

    if (dbe->effect) {
	id = NH_EFFECT_ID(dbe->effect);
	
	switch (NH_EFFECT_TYPE(dbe->effect)) {
	    case E_EXPLOSION:
		syms[0] = cur_drawing->explsyms[id % NUMEXPCHARS];
		syms[0].color = cur_drawing->expltypes[id / NUMEXPCHARS].color;
		break;
		
	    case E_SWALLOW:
		syms[0] = cur_drawing->swallowsyms[id & 0x7];
		syms[0].color = cur_drawing->monsters[id >> 3].color;
		break;
		
	    case E_ZAP:
		syms[0] = cur_drawing->zapsyms[id & 0x3];
		syms[0].color = cur_drawing->zaptypes[id >> 2].color;
		break;
		
	    case E_MISC:
		syms[0] = cur_drawing->effects[id];
		syms[0].color = cur_drawing->effects[id].color;
		break;
	}
	
	return 1; /* we don't want to show other glyphs under effects */
    }
    
    if (dbe->invis)
	syms[count++] = cur_drawing->invis[0];
    
    else if (dbe->mon) {
	if (dbe->mon > cur_drawing->num_monsters && (dbe->monflags & MON_WARNING)) {
	    id = dbe->mon - 1 - cur_drawing->num_monsters;
	    syms[count++] = cur_drawing->warnings[id];
	} else {
	    id = dbe->mon - 1;
	    syms[count++] = cur_drawing->monsters[id];
	}
    }
    
    if (dbe->obj) {
	id = dbe->obj - 1;
	if (id == corpse_id) {
	    syms[count] = cur_drawing->objects[id];
	    syms[count].color = cur_drawing->monsters[dbe->obj_mn - 1].color;
	    count++;
	} else
	    syms[count++] = cur_drawing->objects[id];
    }
    
    if (dbe->trap) {
	id = dbe->trap - 1;
	syms[count++] = cur_drawing->traps[id];
    } 
    
    /* omit the background symbol from the list if it is boring */
    if (count == 0 ||
	dbe->bg >= cur_drawing->bg_feature_offset)
	syms[count++] = cur_drawing->bgelements[dbe->bg];

    return count; /* count <= 4 */
}


void set_rogue_level(nh_bool enable)
{
    if (enable)
	cur_drawing = rogue_drawing;
    else
	switch_graphics(settings.graphics);
}


void curses_notify_level_changed(int dmode)
{
    set_rogue_level(dmode == LDM_ROGUE);
}


void switch_graphics(enum nh_text_mode mode)
{
    switch (mode) {
	default:
	case ASCII_GRAPHICS:
	    cur_drawing = default_drawing;
	    break;
	    
/*
 * Drawing with the full unicode charset. Naturally this requires a unicode terminal.
 */
	case UNICODE_GRAPHICS:
	    if (ui_flags.unicode)
		cur_drawing = unicode_drawing;
	    break;
    }
}


void print_sym(WINDOW *win, struct curses_symdef *sym, int extra_attrs)
{
    int attr;
    cchar_t uni_out;
    
    /* nitrohack color index -> curses color */
    attr = A_NORMAL | extra_attrs;
    if (ui_flags.color)
	attr |= curses_color_attr(sym->color);
    
    /* print it; preferably as unicode */
    if (sym->unichar[0] && ui_flags.unicode) {
	int color = PAIR_NUMBER(attr);
	setcchar(&uni_out, sym->unichar, attr, color, NULL);
	wadd_wch(win, &uni_out);
    } else {
	wattron(win, attr);
	waddch(win, sym->ch);
	wattroff(win, attr);
    }
}

/* outchars.c */
