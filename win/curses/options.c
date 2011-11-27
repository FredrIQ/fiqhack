/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "nhcurses.h"

enum option_lists {
    NO_LIST,
    ACT_BIRTH_OPTS,
    CUR_BIRTH_OPTS,
    GAME_OPTS,
    UI_OPTS
};

enum optstyles {
    OPTSTYLE_DESC,
    OPTSTYLE_NAME,
    OPTSTYLE_FULL
};

enum extra_opttypes {
    OPTTYPE_KEYMAP = 1000
};

static void read_ui_config(void);

/*----------------------------------------------------------------------------*/

#define listlen(list) (sizeof(list)/sizeof(struct nh_listitem))

static struct nh_listitem menu_headings_list[] = {
    {A_NORMAL, "none"},
    {A_REVERSE, "inverse"},
    {A_BOLD, "bold"},
    {A_UNDERLINE, "underline"}
};
static struct nh_enum_option menu_headings_spec =
{menu_headings_list, listlen(menu_headings_list)};

static struct nh_listitem graphics_list[] = {
    {ASCII_GRAPHICS, "plain"},
    {DEC_GRAPHICS, "DEC graphics"},
    {IBM_GRAPHICS, "IBM graphics"},
    {UNICODE_GRAPHICS, "Unicode graphics"}
};
static struct nh_enum_option graphics_spec = {graphics_list, listlen(graphics_list)};

static struct nh_listitem optstyle_list[] = {
    {OPTSTYLE_DESC, "description only"},
    {OPTSTYLE_NAME, "name only"},
    {OPTSTYLE_FULL, "name + description"}
};
static struct nh_enum_option optstyle_spec = {optstyle_list, listlen(optstyle_list)};



#define VTRUE (void*)TRUE

struct nh_option_desc tty_options[] = {
    {"name", "name for new characters (blank = ask)", OPTTYPE_STRING, {NULL}},
    {"blink", "show multiple symbols for each location by switching between them", OPTTYPE_BOOL, { VTRUE }},
    {"extmenu", "use a menu for selecting extended commands (#)", OPTTYPE_BOOL, {FALSE}},
    {"frame", "draw a frame around the window sections", OPTTYPE_BOOL, { VTRUE }},    
    {"status3", "3 line status display", OPTTYPE_BOOL, { VTRUE }},
    {"hilite_pet", "highlight your pet", OPTTYPE_BOOL, { FALSE }},
    {"invweight", "show item weights in the inventory", OPTTYPE_BOOL, { VTRUE }},
    {"ignintr", "ignore interrupt signal, including breaks", OPTTYPE_BOOL, { FALSE }},
    {"showexp", "show experience points", OPTTYPE_BOOL, {VTRUE}},
    {"showscore", "show your score in the status line", OPTTYPE_BOOL, {FALSE}},
    {"standout", "use standout for --More--", OPTTYPE_BOOL, {FALSE}},
    {"time", "display elapsed game time, in moves", OPTTYPE_BOOL, {VTRUE}},
    {"use_inverse", "use inverse video for some things", OPTTYPE_BOOL, { VTRUE }},
    {"unicode", "try to use unicode for drawing", OPTTYPE_BOOL, { VTRUE }},
    {"menu_headings", "display style for menu headings", OPTTYPE_ENUM, {(void*)A_REVERSE}},
    {"msgheight", "message window height", OPTTYPE_INT, {(void*)4}},
    {"msghistory", "number of messages saved for prevmsg", OPTTYPE_INT, {(void*)256}},
    {"optstyle", "option menu display style", OPTTYPE_ENUM, {(void*)OPTSTYLE_FULL}},

    {"graphics", "enhanced line drawing style", OPTTYPE_ENUM, {(void*)ASCII_GRAPHICS}},
    {"scores_own", "show all your own scores in the list", OPTTYPE_BOOL, { FALSE }},
    {"scores_top", "how many top scores to show", OPTTYPE_INT, {(void*)3}},
    {"scores_around", "the number of scores shown around your score", OPTTYPE_INT, {(void*)2}},
    {"sidebar", "draw the inventory sidebar", OPTTYPE_BOOL, { VTRUE }},    
    {"keymap", "alter the key to command mapping", OPTTYPE_KEYMAP, {}},
    {NULL, NULL, OPTTYPE_BOOL, { NULL }}
};

struct nh_boolopt_map boolopt_map[] = {
    {"blink", &settings.blink},
    {"extmenu", &settings.extmenu},
    {"frame", &settings.frame},
    {"hilite_pet", &settings.hilite_pet},
    {"invweight", &settings.invweight},
    {"status3", &settings.status3},
    {"scores_own", &settings.end_own},
    {"showexp", &settings.showexp},
    {"showscore", &settings.showscore},
    {"sidebar", &settings.sidebar},
    {"standout", &settings.standout},
    {"time", &settings.time},
    {"use_inverse", &settings.use_inverse},
    {"ignintr", &settings.ignintr},
    {"unicode", &settings.unicode},
    {NULL, NULL}
};


boolean option_change_callback(struct nh_option_desc *option)
{
    if (!strcmp(option->name, "frame") ||
	!strcmp(option->name, "status3") ||
	!strcmp(option->name, "sidebar")) {
	rebuild_ui();
	return TRUE;
    }
    
    if (!strcmp(option->name, "showexp") ||
	!strcmp(option->name, "showscore") ||
	!strcmp(option->name, "time"))
	curses_update_status(NULL);
    
    if (!strcmp(option->name, "menu_headings")) {
	settings.menu_headings = option->value.e;
    }
    else if (!strcmp(option->name, "graphics")) {
	settings.graphics = option->value.e;
	switch_graphics(option->value.e);
	if (ui_flags.ingame) {
	    draw_map(0);
	    redraw_game_windows();
	}
    }
    else if (!strcmp(option->name, "scores_top")) {
	settings.end_top = option->value.i;
    }
    else if (!strcmp(option->name, "scores_around")) {
	settings.end_around = option->value.i;
    }
    else if (!strcmp(option->name, "optstyle")) {
	settings.optstyle = option->value.e;
    }
    else if (!strcmp(option->name, "msgheight")) {
	settings.msgheight = option->value.i;
	rebuild_ui();
    }
    else if (!strcmp(option->name, "msghistory")) {
	settings.msghistory = option->value.i;
	alloc_hist_array();
    }
    else if (!strcmp(option->name, "name")) {
	if (option->value.s)
	    strcpy(settings.plname, option->value.s);
	else
	    settings.plname[0] = '\0';
    }
    else
	return FALSE;
    
    return TRUE;
}


static struct nh_option_desc *find_option(const char *name)
{
    int i;
    for (i = 0; tty_options[i].name; i++)
	if (!strcmp(name, tty_options[i].name))
	    return &tty_options[i];
    
    return NULL;
}


void init_options(void)
{
    int i;

    find_option("name")->s.maxlen = PL_NSIZ;
    find_option("menu_headings")->e = menu_headings_spec;
    find_option("msgheight")->i.min = 1;
    find_option("msgheight")->i.max = 10;
    find_option("msghistory")->i.min = 20;   /* arbitrary min/max values */
    find_option("msghistory")->i.max = 20000;
    find_option("graphics")->e = graphics_spec;
    find_option("optstyle")->e = optstyle_spec;
    find_option("scores_top")->i.max = 10000;
    find_option("scores_around")->i.max = 100;
    
    nh_setup_ui_options(tty_options, boolopt_map, option_change_callback);
    
    /* set up option defaults; this is necessary for options that are not
     * specified in the config file */
    for (i = 0; tty_options[i].name; i++)
	nh_set_option(tty_options[i].name, tty_options[i].value, FALSE);
    
    read_ui_config();
}


static const char* get_option_string(struct nh_option_desc *option, char *valbuf)
{
    const char *valstr;
    int i;
    
    switch ((int)option->type) {
	case OPTTYPE_BOOL:
	    valstr = option->value.b ? "true" : "false";
	    break;
	    
	case OPTTYPE_ENUM:
	    valstr = "(invalid)";
	    for (i = 0; i < option->e.numchoices; i++)
		if (option->value.e == option->e.choices[i].id)
		    valstr = option->e.choices[i].caption;
	    break;
	    
	case OPTTYPE_INT:
	    sprintf(valbuf, "%d", option->value.i);
	    valstr = valbuf;
	    break;
	    
	case OPTTYPE_STRING:
	    if (!option->value.s)
		valstr = "";
	    else
		valstr = option->value.s;
	    break;
	    
	case OPTTYPE_KEYMAP:
	    valstr = "submenu";
    }
    return valstr;
}


static void print_option_string(struct nh_option_desc *option, char *buf)
{
    char valbuf[8], fmt[16];
    const char *opttxt;
    const char *valstr = get_option_string(option, valbuf);
    
    switch (settings.optstyle) {
	case OPTSTYLE_DESC:
	    opttxt = option->helptxt;
	    if (!opttxt || strlen(opttxt) < 2)
		opttxt = option->name;
	    
	    sprintf(fmt, "%%.%ds\t[%%s]", COLS - 21);
	    snprintf(buf, BUFSZ, fmt, opttxt, valstr);
	    break;
	    
	case OPTSTYLE_NAME:
	    sprintf(fmt, "%%.%ds\t[%%s]", COLS - 21);
	    snprintf(buf, BUFSZ, fmt, option->name, valstr);
	    break;
	    
	default:
	case OPTSTYLE_FULL:
	    sprintf(fmt, "%%s\t%%.%ds\t[%%s]", COLS - 42);
	    snprintf(buf, BUFSZ, fmt, option->name, option->helptxt, valstr);
	    break;
    }
}


/* add a list of options to the given selection menu */
static int menu_add_options(struct nh_menuitem **items, int *size, int *icount,
	    int listid, struct nh_option_desc *options, boolean read_only)
{
    int i, id;
    char optbuf[256];
    
    for (i = 0; options[i].name; i++) {
	id = (listid << 10) | i;
	print_option_string(&options[i], optbuf);
	if (read_only)
	    add_menu_txt(*items, *size, *icount, optbuf, MI_TEXT);
	else
	    add_menu_item(*items, *size, *icount, id, optbuf, 0, 0);
    }
    
    /* add an empty line */
    add_menu_txt(*items, *size, *icount, "", MI_TEXT);
    
    return i;
}


/* display a selection menu for enum options */
static void select_enum_value(union nh_optvalue *value, struct nh_option_desc *option)
{
    struct nh_menuitem *items;
    int icount, size;
    int i, n, selectidx, *pick_list;
    
    icount = 0; size = 10;
    items = malloc(sizeof(struct nh_menuitem) * size);
    
    for (i = 0; i < option->e.numchoices; i++) {
	/* don't use the choice ids directly, 0 is a valid value for those */
	add_menu_item(items, size, icount, i+1, option->e.choices[i].caption, 0, 0);
    }
    pick_list = malloc(sizeof(int) * icount);
    n = curses_display_menu(items, icount, option->name, PICK_ONE, pick_list);
    free(items);
    
    value->e = option->value.e; /* in case of ESC */
    if (n == 1) {
	selectidx = pick_list[0] - 1;
	value->e = option->e.choices[selectidx].id;
    }
    free(pick_list);
}


/* get a new value of the appropriate type for the given option */
static boolean get_option_value(struct win_menu *mdat, int idx)
{
    char buf[BUFSZ], query[BUFSZ];
    union nh_optvalue value;
    struct nh_option_desc *option, *optlist;
    int listid = mdat->items[idx].id >> 10;
    int id = mdat->items[idx].id & 0x1ff;
    char strbuf[BUFSZ];
    int prev_optstyle = settings.optstyle;
    
    switch (listid) {
	case ACT_BIRTH_OPTS:
	    optlist = nh_get_options(ACTIVE_BIRTH_OPTIONS); break;
	case CUR_BIRTH_OPTS:
	    optlist = nh_get_options(CURRENT_BIRTH_OPTIONS); break;
	case GAME_OPTS:
	    optlist = nh_get_options(GAME_OPTIONS); break;
	case UI_OPTS:
	    optlist = tty_options; break;
	    
	default:
	    return FALSE;
    }
    
    option = &optlist[id];
    value.s = strbuf;
    
    switch ((int)option->type) {
	case OPTTYPE_BOOL:
	    value.b = !option->value.b;
	    break;
	    
	case OPTTYPE_INT:
	    sprintf(query, "New value for %s (number from %d to %d)",
		    option->name, option->i.min, option->i.max);
	    sprintf(buf, "%d", value.i);
	    curses_getline(query, buf);
	    if (buf[0] == '\033')
		return FALSE;
	    sscanf(buf, "%d", &value.i);
	    break;
	    
	case OPTTYPE_ENUM:
	    select_enum_value(&value, option);
	    break;
	    
	case OPTTYPE_STRING:
	    sprintf(query, "New value for %s (text)", option->name);
	    curses_getline(query, value.s);
	    if (value.s[0] == '\033')
		return FALSE;
	    break;
	    
	case OPTTYPE_KEYMAP:
	    show_keymap_menu(FALSE);
	    return FALSE;
	    
	default:
	    return FALSE;
    }
    
    if (!nh_set_option(option->name, value, FALSE)) {
	sprintf(strbuf, "new value for %s rejected", option->name);
	curses_msgwin(strbuf);
    } else
	print_option_string(option, mdat->items[idx].caption);
    
    /* special case: directly redo option menu appearance */
    if (settings.optstyle != prev_optstyle)
	return TRUE;
    
    return FALSE;
}


/* display the option dialog */
void display_options(boolean change_birth_opt)
{
    struct nh_menuitem *items;
    int icount, size;
    struct nh_option_desc *nhoptions = nh_get_options(GAME_OPTIONS);
    struct nh_option_desc *birthoptions = NULL;
    int n;
    
    size = 10;
    items = malloc(sizeof(struct nh_menuitem) * size);
    
    do {
	icount = 0;
	if (!change_birth_opt) {
	    birthoptions = nh_get_options(ACTIVE_BIRTH_OPTIONS);
	    /* add general game options */
	    add_menu_txt(items, size, icount, "Game options:", MI_HEADING);
	    menu_add_options(&items, &size, &icount, GAME_OPTS, nhoptions,
				FALSE);
	    
	    /* add or display birth options */
	    add_menu_txt(items, size, icount, "Birth options for this game:",
			    MI_HEADING);
	    menu_add_options(&items, &size, &icount, ACT_BIRTH_OPTS,
				birthoptions, TRUE);
	} else {
	    birthoptions = nh_get_options(CURRENT_BIRTH_OPTIONS);
	    /* add or display birth options */
	    add_menu_txt(items, size, icount, "Birth options:", MI_HEADING);
	    menu_add_options(&items, &size, &icount, CUR_BIRTH_OPTS,
				birthoptions, FALSE);
	    
	    add_menu_txt(items, size, icount, "Game options:", MI_HEADING);
	    menu_add_options(&items, &size, &icount, GAME_OPTS, nhoptions, FALSE);
	}
	
	/* add tty-specific options */
	add_menu_txt(items, size, icount, "TTY interface options:", MI_HEADING);
	menu_add_options(&items, &size, &icount, UI_OPTS, tty_options, FALSE);
	
	n = curses_display_menu_core(items, icount, "Set what options?", PICK_ONE,
				NULL, 0, 0, COLS, LINES, get_option_value);
    } while (n > 0);
    free(items);
    
    write_config();
}


/* parse a single line from the config file and set the option */
static void read_config_line(char* line)
{
    char *comment, *delim, *name, *value;
    union nh_optvalue optval;

    comment = strchr(line, '#');
    if (comment)
	comment = '\0';
    delim = strchr(line, '=');
    if (!delim)
	return; /* could whine about junk chars in the config, but why bother */
    
    name = line;
    value = delim + 1;
    *delim-- = '\0';
    
    /* remove space around name */
    while (isspace(*name))
	name++;
    while (isspace(*delim))
	*delim-- = '\0';
    
    /* remove spaces around value */
    delim = value;
    while (*delim)
	delim++;
    delim--;
    
    while (isspace(*value))
	value++;
    while (isspace(*delim))
	*delim-- = '\0';
    
    /* value may be enclosed with double quotes (") */
    if (*value == '"' && *delim == '"') {
	value++;
	*delim ='\0';
    }
    
    optval.s = value;
    nh_set_option(name, optval, TRUE);
}


/* open a config file and separate it into lines for read_config_line() */
static void read_config_file(const char *filename)
{
    FILE *fp;
    int fsize;
    char *buf, *line;
    
    fp = fopen(filename, "rb");
    if (!fp)
	    return;

    /* obtain file size. */
    fseek(fp , 0 , SEEK_END);
    fsize = ftell(fp);
    rewind(fp);

    buf = malloc(fsize+1);
    if (!buf)
	    return;

    fread(buf, fsize, 1, fp);
    fclose(fp);
    
    buf[fsize] = '\0';
    
    /* each option is expected to have the following format:
	* name=value\n
	*/
    line = strtok(buf, "\n");
    do {
	read_config_line(line);
	
	line = strtok(NULL, "\n");
    } while (line);
    
    free(buf);
}


/* determine the correct filename for the config file */
static void get_config_name(char *buf, boolean ui)
{
    char *envval;
    buf[0] = '\0';
    
    if (!ui) {
	/* check for env override first */
	envval = getenv("NETHACKOPTIONS");
	if (envval) {
	    strncpy(buf, envval, BUFSZ);
	    return;
	}
    }
    
    /* look in regular location */
    if (!get_gamedir(CONFIG_DIR, buf))
	return;
    strncat(buf, ui ? "curses.conf" : "NetHack.conf", BUFSZ);
    
#if defined(WIN32)
    TCHAR szPath[MAX_PATH];
    /* get the application data directory: 
	* C:\Users\somename\AppData\roaming\ on Vista and 7 */
    if (!SUCCEEDED(SHGetFolderPath( NULL, CSIDL_APPDATA, NULL, 0, szPath )))
	return;
    PathAppend( szPath, _T("\\NetHack\\NetHack.conf") );
#endif
}


void read_nh_config(void)
{
    char filename[BUFSZ];
    get_config_name(filename, FALSE);
    read_config_file(filename);
}

void read_ui_config(void)
{
    char uiconfname[BUFSZ];
    get_config_name(uiconfname, TRUE);
    read_config_file(uiconfname);    
}


static void make_config_dir(char *filename)
{
    char filename_copy[BUFSZ];
    char *dir;
    mode_t mask;
    
    /* dirname may modify its argument */
    strncpy(filename_copy, filename, BUFSZ);
    dir = dirname(filename_copy);
    
    mask = umask(0);
    if (mkdir(dir, 0755) == -1) {
	/* couldn't create last level dir; try creating 2 levels */
	dir = dirname(dir);
	mkdir(dir, 0755);
	
	strncpy(filename_copy, filename, BUFSZ);
	dir = dirname(filename_copy);
	mkdir(dir, 0755);
    }
    umask(mask);
}


static FILE *open_config_file(char *filename)
{
    FILE *fp;
    
    fp = fopen(filename, "w");
    if (!fp && (errno == ENOTDIR || errno == ENOENT)) {
	make_config_dir(filename);
	fp = fopen(filename, "w");
    }
    
    if (!fp) {
	fprintf(stderr, "could not open %s: %s", filename, strerror(errno));
	return NULL;
    }
    
    fprintf(fp, "# note: this file is rewritten whenever options are changed ingame\n");
    
    return fp;
}


static void write_config_options(FILE *fp, struct nh_option_desc *options)
{
    int i;
    char workbuf[8];
    const char *optval;
    
    for (i = 0; options[i].name; i++) {
	optval = get_option_string(&options[i], workbuf);
	if (options[i].type == OPTTYPE_STRING ||
	    options[i].type == OPTTYPE_ENUM)
	    fprintf(fp, "%s=\"%s\"\n", options[i].name, optval);
	else
	    fprintf(fp, "%s=%s\n", options[i].name, optval);
    }
}


void write_config(void)
{
    FILE *fp;
    char filename[BUFSZ];
    char uiconfname[BUFSZ];
    
    get_config_name(filename, FALSE);
    get_config_name(uiconfname, TRUE);
    
    fp = open_config_file(filename);
    if (fp) {
	write_config_options(fp, nh_get_options(GAME_OPTIONS));
	write_config_options(fp, nh_get_options(CURRENT_BIRTH_OPTIONS));
	fclose(fp);
    }
    
    fp = open_config_file(uiconfname);
    if (fp) {
	write_config_options(fp, tty_options);
	fclose(fp);
    }
}

