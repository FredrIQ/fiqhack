#include "config.h"
#include "nethack.h"
#include "wintty.h"

struct nh_option_desc tty_options[] = {
    {NULL, NULL, OPTTYPE_BOOL, { NULL }}
};

/* add a list of options to the given selection menu */
static int menu_add_options(winid window, int firstid,
			    struct nh_option_desc *options, boolean read_only)
{
	int i, j;
	char *valstr = NULL, valbuf[8], optbuf[128];
	const char *opttxt;
	anything any;
	
	any.a_void = NULL;
	for (i = 0; options[i].name; i++) {
	    any.a_int = (read_only) ? 0 : firstid + i;
	    
	    switch (options[i].type) {
		case OPTTYPE_BOOL:
		    valstr = options[i].value.b ? "true" : "false";
		    break;
		    
		case OPTTYPE_ENUM:
		    valstr = "(invalid)";
		    for (j = 0; j < options[i].e.numchoices; j++)
			if (options[i].value.e == options[i].e.choices[j].id)
			    valstr =  options[i].e.choices[j].caption;
		    break;
		    
		case OPTTYPE_INT:
		    sprintf(valbuf, "%d", options[i].value.i);
		    valstr = valbuf;
		    break;
		    
		case OPTTYPE_STRING:
		    if (!options[i].value.s)
			valstr = "(not set)";
		    else
			valstr = options[i].value.s;
		    break;
	    }
	    
	    opttxt = options[i].helptxt;
	    if (!opttxt || strlen(opttxt) < 2)
		opttxt = options[i].name;
	    
	    sprintf(optbuf, "%-60.60s [%s]", opttxt, valstr);
	    tty_add_menu(window, NO_GLYPH, &any, 0, 0, 0,
		 optbuf, MENU_UNSELECTED);
	}
	
	/* add an empty line */
	any.a_void = NULL;
	tty_add_menu(window, NO_GLYPH, &any, 0, 0, 0, "", MENU_UNSELECTED);
	
	return i;
}

/* display a selection menu for enum options */
static void select_enum_value(union nh_optvalue *value, struct nh_option_desc *option)
{
	winid window;
	anything any;
	menu_item *selected;
	int i, n, selectidx;
	
	any.a_void = NULL;
	window = tty_create_nhwindow(NHW_MENU);
	tty_start_menu(window);
	
	for (i = 0; i < option->e.numchoices; i++) {
	    /* don't use the choice ids directly, 0 is a valid value for those */
	    any.a_int = i+1;
	    tty_add_menu(window, NO_GLYPH, &any, 0, 0, 0,
		 option->e.choices[i].caption, MENU_UNSELECTED);
	}
	tty_end_menu(window, option->name);
	n = tty_select_menu(window, PICK_ONE, &selected);
	
	value->e = option->value.e; /* in case of ESC */
	if (n == 1) {
	    selectidx = selected[0].item.a_int - 1;
	    value->e = option->e.choices[selectidx].id;
	}
	tty_destroy_nhwindow(window);
}

/* get a new value of the appropriate type for the given option */
static void get_option_value(union nh_optvalue *value, struct nh_option_desc *option)
{
	char buf[BUFSZ], query[BUFSZ];
	
	switch (option->type) {
	    case OPTTYPE_BOOL:
		value->b = !option->value.b;
		break;
		
	    case OPTTYPE_INT:
		sprintf(query, "New value for %s (number)", option->name);
		sprintf(buf, "%d", value->i);
		tty_getlin(query, buf);
		sscanf(buf, "%d", &value->i);
		break;
		
	    case OPTTYPE_ENUM:
		select_enum_value(value, option);
		break;
		
	    case OPTTYPE_STRING:
		sprintf(query, "New value for %s (text)", option->name);
		tty_getlin(query, value->s);
		break;
	}
}

/* display the option dialog */
void display_options(boolean change_birth_opt)
{
	winid tmpwin;
	anything any;
	int gameoptidx, birthoptidx, ttyoptidx, pick_cnt, i;
	menu_item *pick_list;
	struct nh_option_desc *nhoptions = nh_get_options(FALSE);
	struct nh_option_desc *birthoptions = nh_get_options(TRUE);
	struct nh_option_desc *option = NULL;
	union nh_optvalue value;
	char strbuf[BUFSZ];
	
	tmpwin = tty_create_nhwindow(NHW_MENU);
	tty_start_menu(tmpwin);
	
	/* add general game options */
	gameoptidx = 1;
	any.a_void = NULL;
	tty_add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
		 "Game options:", MENU_UNSELECTED);
	birthoptidx = menu_add_options(tmpwin, gameoptidx, nhoptions, FALSE);
	
	/* add or display birth options */
	any.a_void = 0;
	tty_add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
		 "Birth options:", MENU_UNSELECTED);
	ttyoptidx = menu_add_options(tmpwin, gameoptidx, birthoptions, !change_birth_opt);
	
	/* add tty-specific options */
	any.a_void = 0;
	tty_add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
		 "TTY interface options:", MENU_UNSELECTED);
	menu_add_options(tmpwin, gameoptidx, tty_options, FALSE);
	
	tty_end_menu(tmpwin, "Set what options?");
	
	pick_cnt = tty_select_menu(tmpwin, PICK_ANY, &pick_list);
	if (pick_cnt <= 0)
	    return;
	
	value.s = strbuf;
	for (i = 0; i < pick_cnt; ++i) {
	    int idx = pick_list[i].item.a_int;
	    
	    if (idx < birthoptidx) {
		option = &nhoptions[idx - gameoptidx];
		get_option_value(&value, option);
		if (!nh_set_option(option->name, value))
		    pline("new value for %s rejected", option->name);
		
	    } else if (idx < ttyoptidx) {
		option = &birthoptions[idx - birthoptidx];
		get_option_value(&value, option);
		if (!nh_set_option(option->name, value))
		    pline("new value for %s rejected", option->name);
		
	    } else {
		option = &tty_options[idx - ttyoptidx];
		get_option_value(&value, option);
		/* tty_set_option() */
	    }
	}
	tty_destroy_nhwindow(tmpwin);
}

