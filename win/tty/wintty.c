/* Copyright (c) David Cohrs, 1991				  */
/* NetHack may be freely redistributed.  See license for details. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "nethack.h"
#include "color.h"

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(a,b) ((a) > (b) ? (a) : (b))

#ifndef NO_TERMS
#include "tcap.h"
#endif

#include "wintty.h"

#define SIZE(x) (int)(sizeof(x) / sizeof(x[0]))

struct interface_flags ui_flags;

/* Interface definition, for windows.c */
struct window_procs tty_procs = {
    tty_player_selection,
    tty_get_nh_event,
    tty_exit_nhwindows,
    tty_suspend_nhwindows,
    tty_resume_nhwindows,
    tty_create_game_windows,
    tty_destroy_game_windows,
    tty_clear_nhwindow,
    tty_display_nhwindow,
    tty_curs,
    tty_display_buffer,
    tty_update_status,
    tty_print_message,
    tty_display_menu,
    tty_display_objects,
    tty_message_menu,
    tty_update_inventory,
    tty_mark_synch,
    tty_wait_synch,
    tty_print_glyph,
    tty_raw_print,
    tty_raw_print_bold,
    tty_nhgetch,
    tty_nh_poskey,
    tty_nhbell,
    tty_doprev_message,
    tty_yn_function,
    tty_getlin,
    tty_get_ext_cmd,
    tty_number_pad,
    tty_delay_output,

    /* other defs that really should go away (they're tty specific) */
    tty_start_screen,
    tty_end_screen,
    tty_outrip,
};

struct tc_gbl_data tc_gbl_data = { 0,0, 0,0 };	/* AS,AE, LI,CO */

winid WIN_MESSAGE = WIN_ERR;
winid WIN_STATUS = WIN_ERR;
winid WIN_MAP = WIN_ERR;

static int maxwin = 0;			/* number of windows in use */
winid BASE_WINDOW;
struct WinDesc *wins[MAXWIN];
struct DisplayDesc *ttyDisplay;	/* the tty display descriptor */

extern void cmov(int,int); /* from termcap.c */
extern void nocmov(int,int); /* from termcap.c */
#if defined(UNIX)
static char obuf[BUFSIZ];	/* BUFSIZ is defined in stdio.h */
#endif

char defmorestr[] = "--More--";
const char quitchars[] = " \r\n\033";
const char ynchars[] = "yn";
// char plname[PL_NSIZ];

struct nh_status_info cached_status;

#if !defined(NO_TERMS)
boolean GFlag = FALSE;
boolean HE_resets_AS;	/* see termcap.c */
#endif

#if defined(WIN32CON)
static const char to_continue[] = "to continue";
#define getret() getreturn(to_continue)
#else
static void getret(void);
#endif
static void erase_menu_or_text(winid, struct WinDesc *, boolean);
static void free_window_info(struct WinDesc *, boolean);
static void dmore(struct WinDesc *, const char *);
static void set_item_state(winid, int, tty_menu_item *);
static void set_all_on_page(winid,tty_menu_item *,tty_menu_item *);
static void unset_all_on_page(winid,tty_menu_item *,tty_menu_item *);
static void invert_all_on_page(winid,tty_menu_item *,tty_menu_item *, char);
static void invert_all(winid,tty_menu_item *,tty_menu_item *, char);
static void process_menu_window(winid,struct WinDesc *);
static void process_text_window(winid,struct WinDesc *);
static tty_menu_item *reverse(tty_menu_item *);
static const char * compress_str(const char *);
static void tty_putsym(winid, int, int, char);
static char *copy_of(const char *);
static void bail(const char *);	/* __attribute__((noreturn)) */

/*
 * A string containing all the default commands -- to add to a list
 * of acceptable inputs.
 */
static const char default_menu_cmds[] = {
	MENU_FIRST_PAGE,
	MENU_LAST_PAGE,
	MENU_NEXT_PAGE,
	MENU_PREVIOUS_PAGE,
	MENU_SELECT_ALL,
	MENU_UNSELECT_ALL,
	MENU_INVERT_ALL,
	MENU_SELECT_PAGE,
	MENU_UNSELECT_PAGE,
	MENU_INVERT_PAGE,
	0	/* null terminator */
};


static void winerror(int win)
{
    char buf[BUFSZ];
    sprintf(buf, "Bad window id %d", win);
    tty_raw_print(buf);
}


/* clean up and quit */
static void bail(const char *mesg)
{
    clearlocks();
    tty_exit_nhwindows(mesg);
    exit(EXIT_SUCCESS);
    /*NOTREACHED*/
}


void tty_init_nhwindows(void)
{
    int wid, hgt;
    char *opts;
    const char **banner;

    /*
     *  Remember tty modes, to be restored on exit.
     *
     *  gettty() must be called before tty_startup()
     *    due to ordering of LI/CO settings
     *  tty_startup() must be called before initoptions()
     *    due to ordering of graphics settings
     */
#if defined(UNIX)
    setbuf(stdout,obuf);
#endif
    gettty();

    /* to port dependant tty setup */
    tty_startup(&wid, &hgt);
    setftty();			/* calls start_screen */

    /* set up tty descriptor */
    ttyDisplay = malloc(sizeof(struct DisplayDesc));
    ttyDisplay->toplin = 0;
    ttyDisplay->rows = hgt;
    ttyDisplay->cols = wid;
    ttyDisplay->curx = ttyDisplay->cury = 0;
    ttyDisplay->inmore = ttyDisplay->inread = ttyDisplay->intr = 0;
    ttyDisplay->dismiss_more = 0;
    ttyDisplay->color = NO_COLOR;
    ttyDisplay->attrs = 0;

    /* set up the default windows */
    BASE_WINDOW = tty_create_nhwindow(NHW_BASE);
    wins[BASE_WINDOW]->active = 1;

    ttyDisplay->lastwin = WIN_ERR;

    /* add one a space forward menu command alias */
    add_menu_cmd_alias(' ', MENU_NEXT_PAGE);

    clear_nhwindow(BASE_WINDOW);

    banner = nh_get_copyright_banner();
    tty_putstr(BASE_WINDOW, 0, "");
    tty_putstr(BASE_WINDOW, 0, banner[0]);
    tty_putstr(BASE_WINDOW, 0, banner[1]);
    tty_putstr(BASE_WINDOW, 0, banner[2]);
    tty_putstr(BASE_WINDOW, 0, "");
    display_nhwindow(BASE_WINDOW, FALSE);
    
    
    /*
	* Set defaults for some options depending on what we can
	* detect about the environment's capabilities.
	* This has to be done after the global initialization above
	* and before reading user-specific initialization via
	* config file/environment variable below.
	*/
    /* this detects the IBM-compatible console on most 386 boxes */
    if ((opts = getenv("TERM")) && !strncmp(opts, "AT", 2)) {
	    switch_graphics(IBM_GRAPHICS);
    }
    /* detect whether a "vt" terminal can handle alternate charsets */
    if ((opts = getenv("TERM")) &&
	!strncmpi(opts, "vt", 2) && AS && AE &&
	index(AS, '\016') && index(AE, '\017')) {
	    switch_graphics(DEC_GRAPHICS);
    }
}


#define LISTSZ 32
void tty_player_selection(int initrole, int initrace, int initgend,
			  int initalign, int randomall)
{
	int i, k, n, listlen;
	char pick4u = 'n', thisch, lastch = 0;
	char pbuf[QBUFSZ], plbuf[QBUFSZ];
	winid win;
	anything any;
	menu_item *selected = 0;
	struct nh_listitem list[LISTSZ]; /* need enough space for lists of roles or races */
	char listbuffers[LISTSZ][256];
	
	for (i = 0; i < LISTSZ; i++) {
	    list[i].caption = listbuffers[i];
	    list[i].caption[0] = '\0';
	}

	/* Should we randomly pick for the player? */
	if (!randomall &&
	    (initrole == ROLE_NONE || initrace == ROLE_NONE ||
	     initgend == ROLE_NONE || initalign == ROLE_NONE)) {
	    int echoline;
	    char *prompt = nh_build_plselection_prompt(pbuf, QBUFSZ, initrole,
				initrace, initgend, initalign);

	    tty_putstr(BASE_WINDOW, 0, "");
	    echoline = wins[BASE_WINDOW]->cury;
	    tty_putstr(BASE_WINDOW, 0, prompt);
	    do {
		pick4u = tolower(tty_nhgetch());
		if (index(quitchars, pick4u)) pick4u = 'y';
	    } while(!index("ynq", pick4u));
	    if ((int)strlen(prompt) + 1 < CO) {
		/* Echo choice and move back down line */
		tty_putsym(BASE_WINDOW, (int)strlen(prompt)+1, echoline, pick4u);
		tty_putstr(BASE_WINDOW, 0, "");
	    } else
		/* Otherwise it's hard to tell where to echo, and things are
		 * wrapping a bit messily anyway, so (try to) make sure the next
		 * question shows up well and doesn't get wrapped at the
		 * bottom of the window.
		 */
		clear_nhwindow(BASE_WINDOW);
	    
	    if (pick4u != 'y' && pick4u != 'n') {
give_up:	/* Quit */
		if (selected) free(selected);
		bail(NULL);
		/*NOTREACHED*/
		return;
	    }
	}

	nh_root_plselection_prompt(plbuf, QBUFSZ - 1,
			initrole, initrace, initgend, initalign);

	/* Select a role, if necessary */
	/* we'll try to be compatible with pre-selected race/gender/alignment,
	 * but may not succeed */
	if (initrole < 0) {
	    listlen = nh_get_valid_roles(initrace, initgend, initalign, list, LISTSZ);
	    if (listlen == 0) {
		tty_putstr(BASE_WINDOW, 0, "Incompatible role!");
		listlen = nh_get_valid_roles(ROLE_NONE, ROLE_NONE, ROLE_NONE, list, LISTSZ);
	    }
	    
	    /* Process the choice */
	    if (pick4u == 'y' || initrole == ROLE_RANDOM || randomall) {
		/* Pick a random role */
		initrole = list[random() % listlen].id;
 	    } else {
	    	clear_nhwindow(BASE_WINDOW);
		tty_putstr(BASE_WINDOW, 0, "Choosing Character's Role");
		/* Prompt for a role */
		
		win = tty_create_nhwindow(NHW_MENU);
		tty_start_menu(win);
		any.a_void = 0;
		for (i = 0; i < listlen; i++) {
		    any.a_int = list[i].id + 1; /* list[i].id starts at 0 */
		    thisch = tolower(*list[i].caption);
		    if (thisch == lastch)
			thisch = toupper(thisch);
		    tty_add_menu(win, NO_GLYPH, &any, thisch,
			0, ATR_NONE, list[i].caption, MENU_UNSELECTED);
		    lastch = thisch;
		}
		any.a_int = list[random() % listlen].id+1;
		tty_add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
				"Random", MENU_UNSELECTED);
		any.a_int = -1;	/* must be non-zero */
		tty_add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
				"Quit", MENU_UNSELECTED);
		sprintf(pbuf, "Pick a role for your %s", plbuf);
		
		tty_end_menu(win, pbuf);
		n = tty_select_menu(win, PICK_ONE, &selected);
		tty_destroy_nhwindow(win);

		/* Process the choice */
		if (n != 1 || selected[0].item.a_int == any.a_int)
		    goto give_up;		/* Selected quit */

		initrole = selected[0].item.a_int - 1;
		free(selected);
		selected = NULL;
	    }
	    nh_root_plselection_prompt(plbuf, QBUFSZ - 1,
			initrole, initrace, initgend, initalign);
	}
	
	/* Select a race, if necessary */
	/* force compatibility with role, try for compatibility with
	 * pre-selected gender/alignment */
	if (initrace < 0 || !nh_validrace(initrole, initrace)) {
	    listlen = nh_get_valid_races(initrole, initgend, initalign, list, LISTSZ);
	    if (listlen == 0) {
		/* pre-selected race not valid */
		tty_putstr(BASE_WINDOW, 0, "Incompatible race!");
		listlen = nh_get_valid_races(initrole, ROLE_NONE, ROLE_NONE, list, LISTSZ);
	    }
	    
	    if (pick4u == 'y' || initrace == ROLE_RANDOM || randomall) {
		initrace = list[random() % listlen].id;
	    } else {	/* pick4u == 'n' */
		/* Count the number of valid races */
		k = list[0].id;	/* valid race */

		/* Permit the user to pick, if there is more than one */
		if (listlen > 1) {
		    clear_nhwindow(BASE_WINDOW);
		    tty_putstr(BASE_WINDOW, 0, "Choosing Race");
		    win = tty_create_nhwindow(NHW_MENU);
		    tty_start_menu(win);
		    any.a_void = 0;         /* zero out all bits */
		    for (i = 0; i < listlen; i++) {
			any.a_int = list[i].id + 1;
			tty_add_menu(win, NO_GLYPH, &any, list[i].caption[0],
			    0, ATR_NONE, list[i].caption, MENU_UNSELECTED);
		    }
		    any.a_int = list[random() % listlen].id+1;
		    tty_add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
				    "Random", MENU_UNSELECTED);
		    any.a_int = -1;	/* must be non-zero */
		    tty_add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
				    "Quit", MENU_UNSELECTED);
		    sprintf(pbuf, "Pick the race of your %s", plbuf);
		    
		    tty_end_menu(win, pbuf);
		    n = tty_select_menu(win, PICK_ONE, &selected);
		    tty_destroy_nhwindow(win);
		    if (n != 1 || selected[0].item.a_int == any.a_int)
			goto give_up;		/* Selected quit */

		    k = selected[0].item.a_int - 1;
		    free(selected);
		    selected = NULL;
		}
		initrace = k;
	    }
	    nh_root_plselection_prompt(plbuf, QBUFSZ - 1,
			initrole, initrace, initgend, initalign);
	}

	/* Select a gender, if necessary */
	/* force compatibility with role/race, try for compatibility with
	 * pre-selected alignment */
	if (initgend < 0 || !nh_validgend(initrole, initrace, initgend)) {
	    listlen = nh_get_valid_genders(initrole, initrace, initalign, list, LISTSZ);
	    if (listlen == 0) {
		/* pre-selected gender not valid */
		tty_putstr(BASE_WINDOW, 0, "Incompatible gender!");
		listlen = nh_get_valid_genders(initrole, initrace, ROLE_NONE, list, LISTSZ);
	    }
	    if (pick4u == 'y' || initgend == ROLE_RANDOM || randomall) {
		initgend = list[random() % listlen].id;
	    } else {	/* pick4u == 'n' */
		/* Count the number of valid genders */
		k = list[0].id;	/* valid gender */

		/* Permit the user to pick, if there is more than one */
		if (listlen > 1) {
		    clear_nhwindow(BASE_WINDOW);
		    tty_putstr(BASE_WINDOW, 0, "Choosing Gender");
		    win = tty_create_nhwindow(NHW_MENU);
		    tty_start_menu(win);
		    any.a_void = 0;         /* zero out all bits */
		    for (i = 0; i < listlen; i++) {
			any.a_int = list[i].id + 1;
			tty_add_menu(win, NO_GLYPH, &any, list[i].caption[0],
			    0, ATR_NONE, list[i].caption, MENU_UNSELECTED);
		    }
		    any.a_int = list[random() % listlen].id + 1;
		    tty_add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
				    "Random", MENU_UNSELECTED);
		    any.a_int = -1;	/* must be non-zero */
		    tty_add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
				    "Quit", MENU_UNSELECTED);
		    sprintf(pbuf, "Pick the gender of your %s", plbuf);
		    
		    tty_end_menu(win, pbuf);
		    n = tty_select_menu(win, PICK_ONE, &selected);
		    tty_destroy_nhwindow(win);
		    
		    if (n != 1 || selected[0].item.a_int == any.a_int)
			goto give_up;		/* Selected quit */

		    k = selected[0].item.a_int - 1;
		    free(selected);
		    selected = NULL;
		}
		initgend = k;
	    }
	    nh_root_plselection_prompt(plbuf, QBUFSZ - 1,
			initrole, initrace, initgend, initalign);
	}

	/* Select an alignment, if necessary */
	/* force compatibility with role/race/gender */
	if (initalign < 0 || !nh_validalign(initrole, initrace, initalign)) {
	    listlen = nh_get_valid_aligns(initrole, initrace, initgend, list, LISTSZ);
	    
	    if (pick4u == 'y' || initalign == ROLE_RANDOM || randomall) {
		initalign = list[random() % listlen].id;
	    } else {	/* pick4u == 'n' */
		/* Count the number of valid alignments */
		k = list[0].id;	/* valid alignment */

		/* Permit the user to pick, if there is more than one */
		if (listlen > 1) {
		    clear_nhwindow(BASE_WINDOW);
		    tty_putstr(BASE_WINDOW, 0, "Choosing Alignment");
		    win = tty_create_nhwindow(NHW_MENU);
		    tty_start_menu(win);
		    any.a_void = 0;         /* zero out all bits */
		    for (i = 0; i < listlen; i++) {
			any.a_int = list[i].id + 1;
			tty_add_menu(win, NO_GLYPH, &any, list[i].caption[0],
				0, ATR_NONE, list[i].caption, MENU_UNSELECTED);
		    }
		    any.a_int = list[random() % listlen].id + 1;
		    tty_add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
				    "Random", MENU_UNSELECTED);
		    any.a_int = -1;	/* must be non-zero */
		    tty_add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
				    "Quit", MENU_UNSELECTED);
		    sprintf(pbuf, "Pick the alignment of your %s", plbuf);
		    tty_end_menu(win, pbuf);
		    n = tty_select_menu(win, PICK_ONE, &selected);
		    tty_destroy_nhwindow(win);
		    if (n != 1 || selected[0].item.a_int == any.a_int)
			goto give_up;		/* Selected quit */

		    k = selected[0].item.a_int - 1;
		    free(selected);
		    selected = NULL;
		}
		initalign = k;
	    }
	}
	nh_set_role(initrole);
	nh_set_race(initrace);
	nh_set_gend(initgend);
	nh_set_align(initalign);
	
	/* Success! */
	display_nhwindow(BASE_WINDOW, FALSE);
}

/*
 * plname is filled either by an option (-u Player  or  -uPlayer) or
 * explicitly (by being the wizard) or by askname.
 * It may still contain a suffix denoting the role, etc.
 * Always called after init_nhwindows() and before display_gamewindows().
 */
void tty_askname(char *plname)
{
    static char who_are_you[] = "Who are you? ";
    int c, ct, tryct = 0;

    tty_putstr(BASE_WINDOW, 0, "");
    do {
	if (++tryct > 1) {
	    if (tryct > 10) bail("Giving up after 10 tries.\n");
	    move_cursor(BASE_WINDOW, 1, wins[BASE_WINDOW]->cury - 1);
	    tty_putstr(BASE_WINDOW, 0, "Enter a name for your character...");
	    /* erase previous prompt (in case of ESC after partial response) */
	    move_cursor(BASE_WINDOW, 1, wins[BASE_WINDOW]->cury),  cl_end();
	}
	tty_putstr(BASE_WINDOW, 0, who_are_you);
	move_cursor(BASE_WINDOW, (int)(sizeof who_are_you),
		 wins[BASE_WINDOW]->cury - 1);
	ct = 0;
	while((c = base_nhgetch()) != '\n') {
		if(c == EOF) error("End of input\n");
		if (c == '\033') { ct = 0; break; }  /* continue outer loop */
#if defined(WIN32CON)
		if (c == '\003') bail("^C abort.\n");
#endif
		/* some people get confused when their erase char is not ^H */
		if (c == '\b' || c == '\177') {
			if(ct) {
				ct--;
#ifdef WIN32CON
				ttyDisplay->curx--;
				backsp();       /* \b is visible on NT */
				putchar(' ');
				backsp();
#else
				putchar('\b');
				putchar(' ');
				putchar('\b');
#endif
			}
			continue;
		}
#if defined(UNIX)
		if(c != '-' && c != '@')
		if(c < 'A' || (c > 'Z' && c < 'a') || c > 'z') c = '_';
#endif
		if (ct < (int)(sizeof plname) - 1) {
			putchar(c);
			plname[ct++] = c;
#ifdef WIN32CON
			ttyDisplay->curx++;
#endif
		}
	}
	plname[ct] = 0;
    } while (ct == 0);

    /* move to next line to simulate echo of user's <return> */
    move_cursor(BASE_WINDOW, 1, wins[BASE_WINDOW]->cury + 1);
}

void tty_get_nh_event(void)
{
    return;
}

#if !defined(WIN32CON)
static void getret(void)
{
	xputs("\n");
	if(ui_flags.standout)
		standoutbeg();
	xputs("Hit ");
	xputs(ui_flags.cbreak ? "space" : "return");
	xputs(" to continue: ");
	if(ui_flags.standout)
		standoutend();
	xwaitforspace(" ");
}
#endif

void tty_suspend_nhwindows(const char *str)
{
    settty(str);		/* calls end_screen, perhaps raw_print */
    if (!str) tty_raw_print("");	/* calls fflush(stdout) */
}

void tty_resume_nhwindows(void)
{
    gettty();
    setftty();			/* calls start_screen */
    docrt();
}

void tty_exit_nhwindows(const char *str)
{
    winid i;

    tty_suspend_nhwindows(str);
    /* Just forget any windows existed, since we're about to exit anyway.
     * Disable windows to avoid calls to window routines.
     */
    for(i=0; i<MAXWIN; i++)
	if (wins[i] && (i != BASE_WINDOW)) {
#ifdef FREE_ALL_MEMORY
	    free_window_info(wins[i], TRUE);
	    free(wins[i]);
#endif
	    wins[i] = 0;
	}
#ifndef NO_TERMS		/*(until this gets added to the window interface)*/
    tty_shutdown();		/* cleanup termcap/terminfo/whatever */
#endif
    iflags2.window_inited = 0;
}


void tty_create_game_windows(void)
{
    WIN_MESSAGE = tty_create_nhwindow(NHW_MESSAGE);
    WIN_STATUS = tty_create_nhwindow(NHW_STATUS);
    WIN_MAP = tty_create_nhwindow(NHW_MAP);    
}


void tty_destroy_game_windows(void)
{
    tty_destroy_nhwindow(WIN_MAP);
    tty_destroy_nhwindow(WIN_STATUS);
    tty_destroy_nhwindow(WIN_MESSAGE);
    WIN_MESSAGE = WIN_STATUS = WIN_MAP = WIN_ERR;
}


winid tty_create_nhwindow(int type)
{
    struct WinDesc* newwin;
    int i;
    int newid;

    if(maxwin == MAXWIN)
	return WIN_ERR;

    newwin = malloc(sizeof(struct WinDesc));
    newwin->type = type;
    newwin->flags = 0;
    newwin->active = FALSE;
    newwin->curx = newwin->cury = 0;
    newwin->morestr = 0;
    newwin->mlist = NULL;
    newwin->plist = NULL;
    newwin->npages = newwin->plist_size = newwin->nitems = newwin->how = 0;
    switch(type) {
    case NHW_BASE:
	/* base window, used for absolute movement on the screen */
	newwin->offx = newwin->offy = 0;
	newwin->rows = ttyDisplay->rows;
	newwin->cols = ttyDisplay->cols;
	newwin->maxrow = newwin->maxcol = 0;
	break;
    case NHW_MESSAGE:
	/* message window, 1 line long, very wide, top of screen */
	newwin->offx = newwin->offy = 0;
	/* sanity check */
	if(ui_flags.msg_history < 20) ui_flags.msg_history = 20;
	else if(ui_flags.msg_history > 60) ui_flags.msg_history = 60;
	newwin->maxrow = newwin->rows = ui_flags.msg_history;
	newwin->maxcol = newwin->cols = 0;
	break;
    case NHW_STATUS:
	/* status window, 2 lines long, full width, bottom of screen */
	newwin->offx = 0;
	newwin->offy = min((int)ttyDisplay->rows-2, ROWNO+1);
	newwin->rows = newwin->maxrow = 2;
	newwin->cols = newwin->maxcol = min(ttyDisplay->cols, COLNO);
	break;
    case NHW_MAP:
	/* map window, ROWNO lines long, full width, below message window */
	newwin->offx = 0;
	newwin->offy = 1;
	newwin->rows = ROWNO;
	newwin->cols = COLNO;
	newwin->maxrow = 0;	/* no buffering done -- let gbuf do it */
	newwin->maxcol = 0;
	break;
    case NHW_MENU:
    case NHW_TEXT:
	/* inventory/menu window, variable length, full width, top of screen */
	/* help window, the same, different semantics for display, etc */
	newwin->offx = newwin->offy = 0;
	newwin->rows = 0;
	newwin->cols = ttyDisplay->cols;
	newwin->maxrow = newwin->maxcol = 0;
	break;
   default:
	return WIN_ERR;
    }

    for(newid = 0; newid<MAXWIN; newid++) {
	if(wins[newid] == 0) {
	    wins[newid] = newwin;
	    break;
	}
    }
    if(newid == MAXWIN) {
	return WIN_ERR;
    }

    if(newwin->maxrow) {
	newwin->data =
		malloc(sizeof(char *) * (unsigned)newwin->maxrow);
	newwin->datlen =
		malloc(sizeof(short) * (unsigned)newwin->maxrow);
	if(newwin->maxcol) {
	    for (i = 0; i < newwin->maxrow; i++) {
		newwin->data[i] = malloc((unsigned)newwin->maxcol);
		newwin->datlen[i] = newwin->maxcol;
	    }
	} else {
	    for (i = 0; i < newwin->maxrow; i++) {
		newwin->data[i] = NULL;
		newwin->datlen[i] = 0;
	    }
	}
	if(newwin->type == NHW_MESSAGE)
	    newwin->maxrow = 0;
    } else {
	newwin->data = NULL;
	newwin->datlen = NULL;
    }

    return newid;
}

static void erase_menu_or_text(winid window, struct WinDesc *cw, boolean clear)
{
    if(cw->offx == 0)
	if(cw->offy) {
	    move_cursor(window, 1, 0);
	    cl_eos();
	} else if (clear)
	    clear_screen();
	else
	    docrt();
    else
	docorner((int)cw->offx, cw->maxrow+1);
}

static void free_window_info(struct WinDesc *cw, boolean free_data)
{
    int i;

    if (cw->data) {
	if (cw == wins[WIN_MESSAGE] && cw->rows > cw->maxrow)
	    cw->maxrow = cw->rows;		/* topl data */
	for(i=0; i<cw->maxrow; i++)
	    if(cw->data[i]) {
		free(cw->data[i]);
		cw->data[i] = NULL;
		if (cw->datlen) cw->datlen[i] = 0;
	    }
	if (free_data) {
	    free(cw->data);
	    cw->data = NULL;
	    if (cw->datlen) free(cw->datlen);
	    cw->datlen = NULL;
	    cw->rows = 0;
	}
    }
    cw->maxrow = cw->maxcol = 0;
    if(cw->mlist) {
	tty_menu_item *temp;
	while ((temp = cw->mlist) != 0) {
	    cw->mlist = cw->mlist->next;
	    if (temp->str) free(temp->str);
	    free(temp);
	}
    }
    if (cw->plist) {
	free(cw->plist);
	cw->plist = 0;
    }
    cw->plist_size = cw->npages = cw->nitems = cw->how = 0;
    if(cw->morestr) {
	free(cw->morestr);
	cw->morestr = 0;
    }
}

void tty_clear_nhwindow(int type)
{
    if (type == NHW_MESSAGE)
	clear_nhwindow(WIN_MESSAGE);
    else if  (type == NHW_STATUS)
	clear_nhwindow(WIN_STATUS);
    else if  (type == NHW_MAP)
	clear_nhwindow(WIN_MAP);
}


void clear_nhwindow(winid window)
{
    struct WinDesc *cw = 0;

    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	winerror(window);
	return;
    }
    ttyDisplay->lastwin = window;

    switch(cw->type) {
    case NHW_MESSAGE:
	if(ttyDisplay->toplin) {
	    home();
	    cl_end();
	    if(cw->cury)
		docorner(1, cw->cury+1);
	    ttyDisplay->toplin = 0;
	}
	break;
    case NHW_STATUS:
	move_cursor(window, 1, 0);
	cl_end();
	move_cursor(window, 1, 1);
	cl_end();
	break;
    case NHW_MAP:
	/* cheap -- clear the whole thing and tell nethack to redraw botl */
	clear_screen();
	tty_update_status(&cached_status);
	break;
    case NHW_BASE:
	clear_screen();
	break;
    case NHW_MENU:
    case NHW_TEXT:
	if(cw->active)
	    erase_menu_or_text(window, cw, TRUE);
	free_window_info(cw, FALSE);
	break;
    }
    cw->curx = cw->cury = 0;
}

static void dmore(struct WinDesc *cw,
		  const char *s)	/* valid responses */
{
    const char *prompt = cw->morestr ? cw->morestr : defmorestr;
    int offset = (cw->type == NHW_TEXT) ? 1 : 2;

    move_cursor(BASE_WINDOW,
	     (int)ttyDisplay->curx + offset, (int)ttyDisplay->cury);
    if(ui_flags.standout)
	standoutbeg();
    xputs(prompt);
    ttyDisplay->curx += strlen(prompt);
    if(ui_flags.standout)
	standoutend();

    xwaitforspace(s);
}

static void set_item_state(winid window, int lineno, tty_menu_item *item)
{
    char ch = item->selected ? (item->count == -1L ? '+' : '#') : '-';
    move_cursor(window, 4, lineno);
    term_start_attr(item->attr);
    putchar(ch);
    ttyDisplay->curx++;
    term_end_attr(item->attr);
}

static void set_all_on_page(winid window, tty_menu_item *page_start,
			    tty_menu_item *page_end)
{
    tty_menu_item *curr;
    int n;

    for (n = 0, curr = page_start; curr != page_end; n++, curr = curr->next)
	if (curr->identifier.a_void && !curr->selected) {
	    curr->selected = TRUE;
	    set_item_state(window, n, curr);
	}
}

static void unset_all_on_page(winid window, tty_menu_item *page_start,
			      tty_menu_item *page_end)
{
    tty_menu_item *curr;
    int n;

    for (n = 0, curr = page_start; curr != page_end; n++, curr = curr->next)
	if (curr->identifier.a_void && curr->selected) {
	    curr->selected = FALSE;
	    curr->count = -1L;
	    set_item_state(window, n, curr);
	}
}

static void invert_all_on_page(winid window, 
			       tty_menu_item *page_start,
			       tty_menu_item *page_end,
			       char acc) /* group accelerator, 0 => all */
{
    tty_menu_item *curr;
    int n;

    for (n = 0, curr = page_start; curr != page_end; n++, curr = curr->next)
	if (curr->identifier.a_void && (acc == 0 || curr->gselector == acc)) {
	    if (curr->selected) {
		curr->selected = FALSE;
		curr->count = -1L;
	    } else
		curr->selected = TRUE;
	    set_item_state(window, n, curr);
	}
}

/*
 * Invert all entries that match the give group accelerator (or all if
 * zero).
 */
static void invert_all(winid window, 
		       tty_menu_item *page_start,
		       tty_menu_item *page_end,
		       char acc) /* group accelerator, 0 => all */
{
    tty_menu_item *curr;
    boolean on_curr_page;
    struct WinDesc *cw =  wins[window];

    invert_all_on_page(window, page_start, page_end, acc);

    /* invert the rest */
    for (on_curr_page = FALSE, curr = cw->mlist; curr; curr = curr->next) {
	if (curr == page_start)
	    on_curr_page = TRUE;
	else if (curr == page_end)
	    on_curr_page = FALSE;

	if (!on_curr_page && curr->identifier.a_void
				&& (acc == 0 || curr->gselector == acc)) {
	    if (curr->selected) {
		curr->selected = FALSE;
		curr->count = -1;
	    } else
		curr->selected = TRUE;
	}
    }
}

static void process_menu_window(winid window, struct WinDesc *cw)
{
    tty_menu_item *page_start, *page_end, *curr;
    long count;
    int n, curr_page, page_lines;
    boolean finished, counting, reset_count;
    char *cp, *rp, resp[QBUFSZ], gacc[QBUFSZ],
	 *msave, *morestr;

    curr_page = page_lines = 0;
    page_start = page_end = 0;
    msave = cw->morestr;	/* save the morestr */
    cw->morestr = morestr = malloc((unsigned) QBUFSZ);
    counting = FALSE;
    count = 0L;
    reset_count = TRUE;
    finished = FALSE;

    /* collect group accelerators; for PICK_NONE, they're ignored;
       for PICK_ONE, only those which match exactly one entry will be
       accepted; for PICK_ANY, those which match any entry are okay */
    gacc[0] = '\0';
    if (cw->how != PICK_NONE) {
	int i, gcnt[128];
#define GSELIDX(c) (c & 127)	/* guard against `signed char' */

	for (i = 0; i < SIZE(gcnt); i++) gcnt[i] = 0;
	for (n = 0, curr = cw->mlist; curr; curr = curr->next)
	    if (curr->gselector && curr->gselector != curr->selector) {
		++n;
		++gcnt[GSELIDX(curr->gselector)];
	    }

	if (n > 0)	/* at least one group accelerator found */
	    for (rp = gacc, curr = cw->mlist; curr; curr = curr->next)
		if (curr->gselector && !index(gacc, curr->gselector) &&
			(cw->how == PICK_ANY ||
			    gcnt[GSELIDX(curr->gselector)] == 1)) {
		    *rp++ = curr->gselector;
		    *rp = '\0';	/* re-terminate for index() */
		}
    }

    /* loop until finished */
    while (!finished) {
	if (reset_count) {
	    counting = FALSE;
	    count = 0;
	} else
	    reset_count = TRUE;

	if (!page_start) {
	    /* new page to be displayed */
	    if (curr_page < 0 || (cw->npages > 0 && curr_page >= cw->npages))
		return;

	    /* clear screen */
	    if (!cw->offx) {	/* if not corner, do clearscreen */
		if(cw->offy) {
		    move_cursor(window, 1, 0);
		    cl_eos();
		} else
		    clear_screen();
	    }

	    rp = resp;
	    if (cw->npages > 0) {
		/* collect accelerators */
		page_start = cw->plist[curr_page];
		page_end = cw->plist[curr_page + 1];
		for (page_lines = 0, curr = page_start;
			curr != page_end;
			page_lines++, curr = curr->next) {
		    if (curr->selector)
			*rp++ = curr->selector;

		    move_cursor(window, 1, page_lines);
		    if (cw->offx) cl_end();

		    putchar(' ');
		    ++ttyDisplay->curx;
		    /*
		     * Don't use xputs() because (1) under unix it calls
		     * tputstr() which will interpret a '*' as some kind
		     * of padding information and (2) it calls xputc to
		     * actually output the character.  We're faster doing
		     * this.
		     */
		    term_start_attr(curr->attr);
		    for (n = 0, cp = curr->str;
#ifndef WIN32CON
			  *cp && (int) ++ttyDisplay->curx < (int) ttyDisplay->cols;
			  cp++, n++)
#else
			  *cp && (int) ttyDisplay->curx < (int) ttyDisplay->cols;
			  cp++, n++, ttyDisplay->curx++)
#endif
			if (n == 2 && curr->identifier.a_void != 0 &&
							curr->selected) {
			    if (curr->count == -1L)
				putchar('+'); /* all selected */
			    else
				putchar('#'); /* count selected */
			} else
			    putchar(*cp);
		    term_end_attr(curr->attr);
		}
	    } else {
		page_start = 0;
		page_end = 0;
		page_lines = 0;
	    }
	    *rp = 0;

	    /* corner window - clear extra lines from last page */
	    if (cw->offx) {
		for (n = page_lines + 1; n < cw->maxrow; n++) {
		    move_cursor(window, 1, n);
		    cl_end();
		}
	    }

	    /* set extra chars.. */
	    strcat(resp, default_menu_cmds);
	    strcat(resp, "0123456789\033\n\r");	/* counts, quit */
	    strcat(resp, gacc);			/* group accelerators */
	    strcat(resp, mapped_menu_cmds);

	    if (cw->npages > 1)
		sprintf(cw->morestr, "(%d of %d)",
			curr_page + 1, (int) cw->npages);
	    else if (msave)
		strcpy(cw->morestr, msave);
	    else
		strcpy(cw->morestr, defmorestr);

	    move_cursor(window, 1, page_lines);
	    cl_end();
	    dmore(cw, resp);
	} else {
	    /* just put the cursor back... */
	    move_cursor(window, (int) strlen(cw->morestr) + 2, page_lines);
	    xwaitforspace(resp);
	}

	morc = map_menu_cmd(morc);
	switch (morc) {
	    case '0':
		/* special case: '0' is also the default ball class */
		if (!counting && index(gacc, morc)) goto group_accel;
		/* fall through to count the zero */
	    case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		count = (count * 10L) + (long) (morc - '0');
		/*
		 * It is debatable whether we should allow 0 to
		 * start a count.  There is no difference if the
		 * item is selected.  If not selected, then
		 * "0b" could mean:
		 *
		 *	count starting zero:	"zero b's"
		 *	ignore starting zero:	"select b"
		 *
		 * At present I don't know which is better.
		 */
		if (count != 0L) {	/* ignore leading zeros */
		    counting = TRUE;
		    reset_count = FALSE;
		}
		break;
	    case '\033':	/* cancel - from counting or loop */
		if (!counting) {
		    /* deselect everything */
		    for (curr = cw->mlist; curr; curr = curr->next) {
			curr->selected = FALSE;
			curr->count = -1L;
		    }
		    cw->flags |= WIN_CANCELLED;
		    finished = TRUE;
		}
		/* else only stop count */
		break;
	    case '\0':		/* finished (commit) */
	    case '\n':
	    case '\r':
		/* only finished if we are actually picking something */
		if (cw->how != PICK_NONE) {
		    finished = TRUE;
		    break;
		}
		/* else fall through */
	    case MENU_NEXT_PAGE:
		if (cw->npages > 0 && curr_page != cw->npages - 1) {
		    curr_page++;
		    page_start = 0;
		} else
		    finished = TRUE;	/* questionable behavior */
		break;
	    case MENU_PREVIOUS_PAGE:
		if (cw->npages > 0 && curr_page != 0) {
		    --curr_page;
		    page_start = 0;
		}
		break;
	    case MENU_FIRST_PAGE:
		if (cw->npages > 0 && curr_page != 0) {
		    page_start = 0;
		    curr_page = 0;
		}
		break;
	    case MENU_LAST_PAGE:
		if (cw->npages > 0 && curr_page != cw->npages - 1) {
		    page_start = 0;
		    curr_page = cw->npages - 1;
		}
		break;
	    case MENU_SELECT_PAGE:
		if (cw->how == PICK_ANY)
		    set_all_on_page(window, page_start, page_end);
		break;
	    case MENU_UNSELECT_PAGE:
		unset_all_on_page(window, page_start, page_end);
		break;
	    case MENU_INVERT_PAGE:
		if (cw->how == PICK_ANY)
		    invert_all_on_page(window, page_start, page_end, 0);
		break;
	    case MENU_SELECT_ALL:
		if (cw->how == PICK_ANY) {
		    set_all_on_page(window, page_start, page_end);
		    /* set the rest */
		    for (curr = cw->mlist; curr; curr = curr->next)
			if (curr->identifier.a_void && !curr->selected)
			    curr->selected = TRUE;
		}
		break;
	    case MENU_UNSELECT_ALL:
		unset_all_on_page(window, page_start, page_end);
		/* unset the rest */
		for (curr = cw->mlist; curr; curr = curr->next)
		    if (curr->identifier.a_void && curr->selected) {
			curr->selected = FALSE;
			curr->count = -1;
		    }
		break;
	    case MENU_INVERT_ALL:
		if (cw->how == PICK_ANY)
		    invert_all(window, page_start, page_end, 0);
		break;
	    default:
		if (cw->how == PICK_NONE || !index(resp, morc)) {
		    /* unacceptable input received */
		    tty_nhbell();
		    break;
		} else if (index(gacc, morc)) {
 group_accel:
		    /* group accelerator; for the PICK_ONE case, we know that
		       it matches exactly one item in order to be in gacc[] */
		    invert_all(window, page_start, page_end, morc);
		    if (cw->how == PICK_ONE) finished = TRUE;
		    break;
		}
		/* find, toggle, and possibly update */
		for (n = 0, curr = page_start;
			curr != page_end;
			n++, curr = curr->next)
		    if (morc == curr->selector) {
			if (curr->selected) {
			    if (counting && count > 0) {
				curr->count = count;
				set_item_state(window, n, curr);
			    } else { /* change state */
				curr->selected = FALSE;
				curr->count = -1L;
				set_item_state(window, n, curr);
			    }
			} else {	/* !selected */
			    if (counting && count > 0) {
				curr->count = count;
				curr->selected = TRUE;
				set_item_state(window, n, curr);
			    } else if (!counting) {
				curr->selected = TRUE;
				set_item_state(window, n, curr);
			    }
			    /* do nothing counting&&count==0 */
			}

			if (cw->how == PICK_ONE) finished = TRUE;
			break;	/* from `for' loop */
		    }
		break;
	}

    } /* while */
    cw->morestr = msave;
    free(morestr);
}


static void process_text_window(winid window, struct WinDesc *cw)
{
    int i, n, attr;
    char *cp;

    for (n = 0, i = 0; i < cw->maxrow; i++) {
	if (!cw->offx && (n + cw->offy == ttyDisplay->rows - 1)) {
	    move_cursor(window, 1, n);
	    cl_end();
	    dmore(cw, quitchars);
	    if (morc == '\033') {
		cw->flags |= WIN_CANCELLED;
		break;
	    }
	    if (cw->offy) {
		move_cursor(window, 1, 0);
		cl_eos();
	    } else
		clear_screen();
	    n = 0;
	}
	move_cursor(window, 1, n++);
	if (cw->offx) cl_end();
	if (cw->data[i]) {
	    attr = cw->data[i][0] - 1;
	    if (cw->offx) {
		putchar(' '); ++ttyDisplay->curx;
	    }
	    term_start_attr(attr);
	    for (cp = &cw->data[i][1];
#ifndef WIN32CON
		    *cp && (int) ++ttyDisplay->curx < (int) ttyDisplay->cols;
		    cp++)
#else
		    *cp && (int) ttyDisplay->curx < (int) ttyDisplay->cols;
		    cp++, ttyDisplay->curx++)
#endif
		putchar(*cp);
	    term_end_attr(attr);
	}
    }
    if (i == cw->maxrow) {
	move_cursor(BASE_WINDOW, (int)cw->offx + 1,
		 (cw->type == NHW_TEXT) ? (int) ttyDisplay->rows - 1 : n);
	cl_end();
	dmore(cw, quitchars);
	if (morc == '\033')
	    cw->flags |= WIN_CANCELLED;
    }
}


void tty_display_nhwindow(int type, boolean blocking)
{
    if (type == NHW_MESSAGE)
	display_nhwindow(WIN_MESSAGE, blocking);
    else if (type == NHW_STATUS)
	display_nhwindow(WIN_STATUS, blocking);
    else if (type == NHW_MAP)
	display_nhwindow(WIN_MAP, blocking);
}


void display_nhwindow(winid window,
			  boolean blocking) /* with ttys, all windows are blocking */
{
    struct WinDesc *cw = 0;

    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	winerror(window);
	return;
    }
    if(cw->flags & WIN_CANCELLED)
	return;
    ttyDisplay->lastwin = window;
    ttyDisplay->rawprint = 0;

    switch(cw->type) {
    case NHW_MESSAGE:
	if(ttyDisplay->toplin == 1) {
	    more();
	    ttyDisplay->toplin = 1; /* more resets this */
	    clear_nhwindow(window);
	} else
	    ttyDisplay->toplin = 0;
	cw->curx = cw->cury = 0;
	if(!cw->active)
	    iflags2.window_inited = TRUE;
	break;
    case NHW_MAP:
	end_glyphout();
	if(blocking) {
	    if(!ttyDisplay->toplin) ttyDisplay->toplin = 1;
	    display_nhwindow(WIN_MESSAGE, TRUE);
	    return;
	}
    case NHW_BASE:
	fflush(stdout);
	break;
    case NHW_TEXT:
	cw->maxcol = ttyDisplay->cols; /* force full-screen mode */
	/*FALLTHRU*/
    case NHW_MENU:
	cw->active = 1;
	/* avoid converting to uchar before calculations are finished */
	cw->offx = (uchar) (int)
	    max((int) 10, (int) (ttyDisplay->cols - cw->maxcol - 1));
	if(cw->type == NHW_MENU)
	    cw->offy = 0;
	if(ttyDisplay->toplin == 1)
	    display_nhwindow(WIN_MESSAGE, TRUE);
	if(cw->offx == 10 || cw->maxrow >= (int) ttyDisplay->rows) {
	    cw->offx = 0;
	    if(cw->offy) {
		move_cursor(window, 1, 0);
		cl_eos();
	    } else
		clear_screen();
	    ttyDisplay->toplin = 0;
	} else
	    clear_nhwindow(WIN_MESSAGE);

	if (cw->data || !cw->maxrow)
	    process_text_window(window, cw);
	else
	    process_menu_window(window, cw);
	break;
    }
    cw->active = 1;
}

void tty_dismiss_nhwindow(winid window)
{
    struct WinDesc *cw = 0;

    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	winerror(window);
	return;
    }

    switch(cw->type) {
    case NHW_MESSAGE:
	if (ttyDisplay->toplin)
	    display_nhwindow(WIN_MESSAGE, TRUE);
	/*FALLTHRU*/
    case NHW_STATUS:
    case NHW_BASE:
    case NHW_MAP:
	/*
	 * these should only get dismissed when the game is going away
	 * or suspending
	 */
	move_cursor(BASE_WINDOW, 1, (int)ttyDisplay->rows-1);
	cw->active = 0;
	break;
    case NHW_MENU:
    case NHW_TEXT:
	if(cw->active) {
	    if (iflags2.window_inited) {
		/* otherwise dismissing the text endwin after other windows
		 * are dismissed tries to redraw the map and panics.  since
		 * the whole reason for dismissing the other windows was to
		 * leave the ending window on the screen, we don't want to
		 * erase it anyway.
		 */
		erase_menu_or_text(window, cw, FALSE);
	    }
	    cw->active = 0;
	}
	break;
    }
    cw->flags = 0;
}

void tty_destroy_nhwindow(winid window)
{
    struct WinDesc *cw = 0;

    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	winerror(window);
	return;
    }

    if(cw->active)
	tty_dismiss_nhwindow(window);
    if(cw->type == NHW_MESSAGE)
	iflags2.window_inited = 0;
    if(cw->type == NHW_MAP)
	clear_screen();

    free_window_info(cw, TRUE);
    free(cw);
    wins[window] = 0;
}


void tty_curs(int x, int y)
{
    move_cursor(WIN_MAP, x, y);
}


void move_cursor(winid window,
	      int x, int y) /* not xchar: perhaps xchar is unsigned and
			       curx-x would be unsigned as well */
{
    struct WinDesc *cw = 0;
    int cx = ttyDisplay->curx;
    int cy = ttyDisplay->cury;

    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	winerror(window);
	return;
    }
    ttyDisplay->lastwin = window;

    cw->curx = --x;	/* column 0 is never used */
    cw->cury = y;
#ifdef DEBUG
    if(x<0 || y<0 || y >= cw->rows || x > cw->cols) {
	const char *s = "[unknown type]";
	switch(cw->type) {
	case NHW_MESSAGE: s = "[topl window]"; break;
	case NHW_STATUS: s = "[status window]"; break;
	case NHW_MAP: s = "[map window]"; break;
	case NHW_MENU: s = "[corner window]"; break;
	case NHW_TEXT: s = "[text window]"; break;
	case NHW_BASE: s = "[base window]"; break;
	}
	impossible("bad curs positioning win %d %s (%d,%d)", window, s, x, y);
	return;
    }
#endif
    x += cw->offx;
    y += cw->offy;

    if (y == cy && x == cx)
	return;

    if(cw->type == NHW_MAP)
	end_glyphout();

#ifndef NO_TERMS
    if(!nh_ND && (cx != x || x <= 3)) { /* Extremely primitive */
	cmov(x, y); /* bunker!wtm */
	return;
    }
#endif

    if((cy -= y) < 0) cy = -cy;
    if((cx -= x) < 0) cx = -cx;
    if(cy <= 3 && cx <= 3) {
	nocmov(x, y);
#ifndef NO_TERMS
    } else if ((x <= 3 && cy <= 3) || (!nh_CM && x < cx)) {
	putchar('\r');
	ttyDisplay->curx = 0;
	nocmov(x, y);
    } else if (!nh_CM) {
	nocmov(x, y);
#endif
    } else
	cmov(x, y);

    ttyDisplay->curx = x;
    ttyDisplay->cury = y;
}

static void tty_putsym(winid window, int x, int y, char ch)
{
    struct WinDesc *cw = 0;

    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	winerror(window);
	return;
    }

    switch(cw->type) {
    case NHW_STATUS:
    case NHW_MAP:
    case NHW_BASE:
	move_cursor(window, x, y);
	putchar(ch);
	ttyDisplay->curx++;
	cw->curx++;
	break;
    case NHW_MESSAGE:
    case NHW_MENU:
    case NHW_TEXT:
	break;
    }
}


static const char*compress_str(const char *str)
{
	static char cbuf[BUFSZ];
	/* compress in case line too long */
	if((int)strlen(str) >= CO) {
		const char *bp0 = str;
		char *bp1 = cbuf;

		do {
			if(*bp0 != ' ' || bp0[1] != ' ' || bp0[2] != ' ')
				*bp1++ = *bp0;
		} while(*bp0++);
	} else
	    return str;
	return cbuf;
}

void tty_putstr(winid window, int attr, const char *str)
{
    struct WinDesc *cw = 0;
    char *ob;
    const char *nb;
    int i, j, n0;

    /* Assume there's a real problem if the window is missing --
     * probably a panic message
     */
    if(window == WIN_ERR || (cw = wins[window]) == NULL) {
	tty_raw_print(str);
	return;
    }

    if(str == NULL ||
	((cw->flags & WIN_CANCELLED) && (cw->type != NHW_MESSAGE)))
	return;
    if(cw->type != NHW_MESSAGE)
	str = compress_str(str);

    ttyDisplay->lastwin = window;

    switch(cw->type) {
    case NHW_MESSAGE:
	/* really do this later */
	update_topl(str);
	break;

    case NHW_STATUS:
	ob = &cw->data[cw->cury][j = cw->curx];
	*ob = 0;
	if(!cw->cury && strlen(str) >= CO) {
	    /* the characters before "St:" are unnecessary */
	    nb = index(str, ':');
	    if(nb && nb > str+2)
		str = nb - 2;
	}
	nb = str;
	for(i = cw->curx+1, n0 = cw->cols; i < n0; i++, nb++) {
	    if(!*nb) {
		/* last char printed may be in middle of line */
		move_cursor(WIN_STATUS, i, cw->cury);
		cl_end();
		break;
	    }
	    if(*ob != *nb)
		tty_putsym(WIN_STATUS, i, cw->cury, *nb);
	    if(*ob) ob++;
	}

	strncpy(&cw->data[cw->cury][j], str, cw->cols - j - 1);
	cw->data[cw->cury][cw->cols-1] = '\0'; /* null terminate */
	cw->cury = (cw->cury+1) % 2;
	cw->curx = 0;
	break;
    case NHW_MAP:
	move_cursor(window, cw->curx+1, cw->cury);
	term_start_attr(attr);
	while(*str && (int) ttyDisplay->curx < (int) ttyDisplay->cols-1) {
	    putchar(*str);
	    str++;
	    ttyDisplay->curx++;
	}
	cw->curx = 0;
	cw->cury++;
	term_end_attr(attr);
	break;
    case NHW_BASE:
	move_cursor(window, cw->curx+1, cw->cury);
	term_start_attr(attr);
	while (*str) {
	    if ((int) ttyDisplay->curx >= (int) ttyDisplay->cols-1) {
		cw->curx = 0;
		cw->cury++;
		move_cursor(window, cw->curx+1, cw->cury);
	    }
	    putchar(*str);
	    str++;
	    ttyDisplay->curx++;
	}
	cw->curx = 0;
	cw->cury++;
	term_end_attr(attr);
	break;
    case NHW_MENU:
    case NHW_TEXT:
	if(cw->type == NHW_TEXT && cw->cury == ttyDisplay->rows-1) {
	    /* not a menu, so save memory and output 1 page at a time */
	    cw->maxcol = ttyDisplay->cols; /* force full-screen mode */
	    display_nhwindow(window, TRUE);
	    for(i=0; i<cw->maxrow; i++)
		if(cw->data[i]){
		    free(cw->data[i]);
		    cw->data[i] = 0;
		}
	    cw->maxrow = cw->cury = 0;
	}
	/* always grows one at a time, but alloc 12 at a time */
	if(cw->cury >= cw->rows) {
	    char **tmp;

	    cw->rows += 12;
	    tmp = malloc(sizeof(char *) * (unsigned)cw->rows);
	    for(i=0; i<cw->maxrow; i++)
		tmp[i] = cw->data[i];
	    if(cw->data)
		free(cw->data);
	    cw->data = tmp;

	    for(i=cw->maxrow; i<cw->rows; i++)
		cw->data[i] = 0;
	}
	if(cw->data[cw->cury])
	    free(cw->data[cw->cury]);
	n0 = strlen(str) + 1;
	ob = cw->data[cw->cury] = malloc((unsigned)n0 + 1);
	*ob++ = (char)(attr + 1);	/* avoid nuls, for convenience */
	strcpy(ob, str);

	if(n0 > cw->maxcol)
	    cw->maxcol = n0;
	if(++cw->cury > cw->maxrow)
	    cw->maxrow = cw->cury;
	if(n0 > CO) {
	    /* attempt to break the line */
	    for(i = CO-1; i && str[i] != ' ' && str[i] != '\n';)
		i--;
	    if(i) {
		cw->data[cw->cury-1][++i] = '\0';
		tty_putstr(window, attr, &str[i]);
	    }

	}
	break;
    }
}


void tty_display_buffer(char *buf, boolean trymove)
{
	char *line;
	char linebuf[BUFSZ];

	clear_nhwindow(WIN_MESSAGE);
	winid datawin = tty_create_nhwindow(NHW_TEXT);
	boolean empty = TRUE;

	if(trymove
#ifndef NO_TERMS
	    && nh_CD
#endif
	) {
	    /* attempt to scroll text below map window if there's room */
	    wins[datawin]->offy = wins[WIN_STATUS]->offy+3;
	    if((int) wins[datawin]->offy + 12 > (int) ttyDisplay->rows)
		wins[datawin]->offy = 0;
	}
	
	line = strtok(buf, "\n");
	do {
	    strncpy(linebuf, line, BUFSZ);
	    if (index(linebuf, '\t') != 0)
		tabexpand(linebuf);
	    empty = FALSE;
	    tty_putstr(datawin, 0, linebuf);
	    
	    if(wins[datawin]->flags & WIN_CANCELLED)
		break;
	    
	    line = strtok(NULL, "\n");
	} while (line);
	
	if (!empty) display_nhwindow(datawin, FALSE);
	tty_destroy_nhwindow(datawin);
}

void tty_start_menu(winid window)
{
    clear_nhwindow(window);
    return;
}

/*ARGSUSED*/
/*
 * Add a menu item to the beginning of the menu list.  This list is reversed
 * later.
 */
void tty_add_menu(
    winid window,	/* window to use, must be of type NHW_MENU */
    int glyph,		/* glyph to display with item (unused) */
    const anything *identifier,	/* what to return if selected */
    char ch,		/* keyboard accelerator (0 = pick our own) */
    char gch,		/* group accelerator (0 = no group) */
    int attr,		/* attribute for string (like tty_putstr()) */
    const char *str,	/* menu string */
    boolean preselected) /* item is marked as selected */
{
    struct WinDesc *cw = 0;
    tty_menu_item *item;
    const char *newstr;
    char buf[4+BUFSZ];

    if (str == NULL)
	return;

    if (window == WIN_ERR || (cw = wins[window]) == NULL
		|| cw->type != NHW_MENU) {
	winerror(window);
	return;
    }

    cw->nitems++;
    if (identifier->a_void) {
	int len = strlen(str);
	if (len >= BUFSZ) {
	    /* We *think* everything's coming in off at most BUFSZ bufs... */
	    len = BUFSZ - 1;
	}
	sprintf(buf, "%c - ", ch ? ch : '?');
	strncpy(buf+4, str, len);
	buf[4+len] = '\0';
	newstr = buf;
    } else
	newstr = str;

    item = malloc(sizeof(tty_menu_item));
    item->identifier = *identifier;
    item->count = -1L;
    item->selected = preselected;
    item->selector = ch;
    item->gselector = gch;
    item->attr = attr;
    item->str = copy_of(newstr);

    item->next = cw->mlist;
    cw->mlist = item;
}

/* Invert the given list, can handle NULL as an input. */
static tty_menu_item *reverse(tty_menu_item *curr)
{
    tty_menu_item *next, *head = 0;

    while (curr) {
	next = curr->next;
	curr->next = head;
	head = curr;
	curr = next;
    }
    return head;
}

/*
 * End a menu in this window, window must a type NHW_MENU.  This routine
 * processes the string list.  We calculate the # of pages, then assign
 * keyboard accelerators as needed.  Finally we decide on the width and
 * height of the window.
 */
void tty_end_menu(winid window,		/* menu to use */
		  const char *prompt)	/* prompt to for menu */
{
    struct WinDesc *cw = 0;
    tty_menu_item *curr;
    short len;
    int lmax, n;
    char menu_ch;

    if (window == WIN_ERR || (cw = wins[window]) == NULL ||
		cw->type != NHW_MENU) {
	winerror(window);
	return;
    }

    /* Reverse the list so that items are in correct order. */
    cw->mlist = reverse(cw->mlist);

    /* Put the promt at the beginning of the menu. */
    if (prompt) {
	anything any;

	any.a_void = 0;	/* not selectable */
	tty_add_menu(window, NO_GLYPH, &any, 0, 0, ATR_NONE, "", MENU_UNSELECTED);
	tty_add_menu(window, NO_GLYPH, &any, 0, 0, ATR_NONE, prompt, MENU_UNSELECTED);
    }

    lmax = min(52, (int)ttyDisplay->rows - 1);		/* # lines per page */
    cw->npages = (cw->nitems + (lmax - 1)) / lmax;	/* # of pages */

    /* make sure page list is large enough */
    if (cw->plist_size < cw->npages+1 /*need 1 slot beyond last*/) {
	if (cw->plist) free(cw->plist);
	cw->plist_size = cw->npages + 1;
	cw->plist = malloc(cw->plist_size * sizeof(tty_menu_item *));
    }

    cw->cols = 0; /* cols is set when the win is initialized... (why?) */
    menu_ch = '?';	/* lint suppression */
    for (n = 0, curr = cw->mlist; curr; n++, curr = curr->next) {
	/* set page boundaries and character accelerators */
	if ((n % lmax) == 0) {
	    menu_ch = 'a';
	    cw->plist[n/lmax] = curr;
	}
	if (curr->identifier.a_void && !curr->selector) {
	    curr->str[0] = curr->selector = menu_ch;
	    if (menu_ch++ == 'z') menu_ch = 'A';
	}

	/* cut off any lines that are too long */
	len = strlen(curr->str) + 2;	/* extra space at beg & end */
	if (len > (int)ttyDisplay->cols) {
	    curr->str[ttyDisplay->cols-2] = 0;
	    len = ttyDisplay->cols;
	}
	if (len > cw->cols) cw->cols = len;
    }
    cw->plist[cw->npages] = 0;	/* plist terminator */

    /*
     * If greater than 1 page, morestr is "(x of y) " otherwise, "(end) "
     */
    if (cw->npages > 1) {
	char buf[QBUFSZ];
	/* produce the largest demo string */
	sprintf(buf, "(%d of %d) ", cw->npages, cw->npages);
	len = strlen(buf);
	cw->morestr = copy_of("");
    } else {
	cw->morestr = copy_of("(end) ");
	len = strlen(cw->morestr);
    }

    if (len > (int)ttyDisplay->cols) {
	/* truncate the prompt if its too long for the screen */
	if (cw->npages <= 1)	/* only str in single page case */
	    cw->morestr[ttyDisplay->cols] = 0;
	len = ttyDisplay->cols;
    }
    if (len > cw->cols) cw->cols = len;

    cw->maxcol = cw->cols;

    /*
     * The number of lines in the first page plus the morestr will be the
     * maximum size of the window.
     */
    if (cw->npages > 1)
	cw->maxrow = cw->rows = lmax + 1;
    else
	cw->maxrow = cw->rows = cw->nitems + 1;
}

int tty_select_menu(winid window, int how, menu_item **menu_list)
{
    struct WinDesc *cw = 0;
    tty_menu_item *curr;
    menu_item *mi;
    int n, cancelled;

    if(window == WIN_ERR || (cw = wins[window]) == NULL
       || cw->type != NHW_MENU) {
	winerror(window);
	return -1;
    }

    *menu_list = NULL;
    cw->how = (short) how;
    morc = 0;
    display_nhwindow(window, TRUE);
    cancelled = !!(cw->flags & WIN_CANCELLED);
    tty_dismiss_nhwindow(window);	/* does not destroy window data */

    if (cancelled) {
	n = -1;
    } else {
	for (n = 0, curr = cw->mlist; curr; curr = curr->next)
	    if (curr->selected) n++;
    }

    if (n > 0) {
	*menu_list = malloc(n * sizeof(menu_item));
	for (mi = *menu_list, curr = cw->mlist; curr; curr = curr->next)
	    if (curr->selected) {
		mi->item = curr->identifier;
		mi->count = curr->count;
		mi++;
	    }
    }

    return n;
}


int tty_display_menu(struct nh_menuitem *items, int icount, const char *title,
		     int how, int *results)
{
    int i, n = 0;
    menu_item *selected = NULL;
    anything any;
    winid win;
    boolean is_text = TRUE;
    
    for (i = 0; i < icount; i++)
	is_text = is_text && (items[i].role != MI_NORMAL);
    
    any.a_void = NULL;
    win = tty_create_nhwindow(NHW_MENU);
    if (is_text) {
	for (i = 0; i < icount; i++) {
	    if (items[i].role == MI_HEADING)
		tty_putstr(win, ui_flags.menu_headings, items[i].caption);
	    else
		tty_putstr(win, 0, items[i].caption);
	}
	
	display_nhwindow(win, TRUE);
    } else {
	tty_start_menu(win);
	for (i = 0; i < icount; i++) {
	    any.a_int = items[i].id;
	    if (items[i].role == MI_HEADING)
		tty_add_menu(win, NO_GLYPH, &any, 0, 0, ui_flags.menu_headings,
				items[i].caption, MENU_UNSELECTED);
	    else
		tty_add_menu(win, NO_GLYPH, &any, items[i].accel,
			    items[i].group_accel, ATR_NONE, items[i].caption,
			    items[i].selected);
	}
	
	tty_end_menu(win, title);
	n = tty_select_menu(win, how, &selected);
	for (i = 0; i < n && results; i++)
	    results[i] = selected[i].item.a_int;
	
	free(selected);
    }
    
    tty_destroy_nhwindow(win);
    
    return n;
}


int tty_display_objects(struct nh_objitem *items, int icount, const char *title,
			int how, struct nh_objresult *pick_list)
{
    int i, n;
    menu_item *selected = NULL;
    anything any;
    winid win;
    
    any.a_void = NULL;
    win = tty_create_nhwindow(NHW_MENU);
    tty_start_menu(win);
    
    for (i = 0; i < icount; i++) {
	any.a_int = items[i].id;
	if (items[i].id == 0 && items[i].otype == -1)
	    tty_add_menu(win, NO_GLYPH, &any, 0, 0, ui_flags.menu_headings,
			    items[i].caption, MENU_UNSELECTED);
	else
	    tty_add_menu(win, items[i].glyph, &any, items[i].accel,
			items[i].group_accel, ATR_NONE, items[i].caption,
			MENU_UNSELECTED);
    }
    
    tty_end_menu(win, title);
    n = tty_select_menu(win, how, &selected);
    for (i = 0; i < n; i++) {
	pick_list[i].id = selected[i].item.a_int;
	pick_list[i].count = selected[i].count;
    }
    free(selected);
    
    tty_destroy_nhwindow(win);
    return n;
}


/* special hack for treating top line --More-- as a one item menu */
char tty_message_menu(char let, int how, const char *mesg)
{
    /* "menu" without selection; use ordinary pline, no more() */
    if (how == PICK_NONE) {
	tty_print_message(mesg);
	return 0;
    }

    ttyDisplay->dismiss_more = let;
    morc = 0;
    /* barebones pline(); since we're only supposed to be called after
       response to a prompt, we'll assume that the display is up to date */
    tty_putstr(WIN_MESSAGE, 0, mesg);
    /* if `mesg' didn't wrap (triggering --More--), force --More-- now */
    if (ttyDisplay->toplin == 1) {
	more();
	ttyDisplay->toplin = 1; /* more resets this */
	clear_nhwindow(WIN_MESSAGE);
    }
    /* normally <ESC> means skip further messages, but in this case
       it means cancel the current prompt; any other messages should
       continue to be output normally */
    wins[WIN_MESSAGE]->flags &= ~WIN_CANCELLED;
    ttyDisplay->dismiss_more = 0;

    return ((how == PICK_ONE && morc == let) || morc == '\033') ? morc : '\0';
}

void tty_update_inventory(void)
{
    return;
}

void tty_mark_synch(void)
{
    fflush(stdout);
}

void tty_wait_synch(void)
{
    /* we just need to make sure all windows are synch'd */
    if(!ttyDisplay || ttyDisplay->rawprint) {
	getret();
	if(ttyDisplay) ttyDisplay->rawprint = 0;
    } else {
	display_nhwindow(WIN_MAP, FALSE);
	if(ttyDisplay->inmore) {
	    addtopl("--More--");
	    fflush(stdout);
	} else if(ttyDisplay->inread > program_state.gameover) {
	    /* this can only happen if we were reading and got interrupted */
	    ttyDisplay->toplin = 3;
	    /* do this twice; 1st time gets the Quit? message again */
	    tty_doprev_message();
	    tty_doprev_message();
	    ttyDisplay->intr++;
	    fflush(stdout);
	}
    }
}

void docorner(int xmin, int ymax)
{
    int y;
    struct WinDesc *cw = wins[WIN_MAP];

    if (check_swallowed()) {	/* Can be done more efficiently */
	swallowed(1);
	return;
    }

    for (y = 0; y < ymax; y++) {
	move_cursor(BASE_WINDOW, xmin,y);	/* move cursor */
	cl_end();			/* clear to end of line */
	if (y<cw->offy || y > ROWNO) continue; /* only refresh board  */
	row_refresh(xmin-(int)cw->offx,COLNO-1,y-(int)cw->offy);
    }

    end_glyphout();
    if (ymax >= (int) wins[WIN_STATUS]->offy) {
					/* we have wrecked the bottom line */
	tty_update_status(&cached_status);
    }
}

void end_glyphout(void)
{
#if !defined(NO_TERMS)
    if (GFlag) {
	GFlag = FALSE;
	graph_off();
    }
#endif
    if(ttyDisplay->color != NO_COLOR) {
	term_end_color();
	ttyDisplay->color = NO_COLOR;
    }
}

#ifndef WIN32
void g_putch(int in_ch)
{
    char ch = (char)in_ch;

# if !defined(NO_TERMS)
    if (iflags2.IBMgraphics || ui_flags.eight_bit_input) {
	/* IBM-compatible displays don't need other stuff */
	putchar(ch);
    } else if (ch & 0x80) {
	if (!GFlag || HE_resets_AS) {
	    graph_on();
	    GFlag = TRUE;
	}
	putchar((ch ^ 0x80)); /* Strip 8th bit */
    } else {
	if (GFlag) {
	    graph_off();
	    GFlag = FALSE;
	}
	putchar(ch);
    }

#else
    putchar(ch);

#endif	/* !NO_TERMS */

    return;
}
#endif /* !WIN32 */


/*
 *  tty_print_glyph
 *
 *  Print the glyph to the output device.  Don't flush the output device.
 *
 *  Since this is only called from show_glyph(), it is assumed that the
 *  position and glyph are always correct (checked there)!
 */

void tty_print_glyph(xchar x, xchar y, int glyph)
{
    int ch;
    boolean reverse_on = FALSE;
    int	    color;
    unsigned special;
    winid window = WIN_MAP;
    
    /* map glyph to character and color */
    mapglyph(glyph, &ch, &color, &special, x, y);
    if (!has_color(color))
	color = NO_COLOR;

    /* Move the cursor. */
    move_cursor(window, x,y);

#ifndef NO_TERMS
    if (ul_hack && ch == '_') {		/* non-destructive underscore */
	putchar((char) ' ');
	backsp();
    }
#endif

    if (color != ttyDisplay->color) {
	if(ttyDisplay->color != NO_COLOR)
	    term_end_color();
	ttyDisplay->color = color;
	if(color != NO_COLOR)
	    term_start_color(color);
    }

    /* must be after color check; term_end_color may turn off inverse too */
    if (((special & MG_PET) && ui_flags.hilite_pet) ||
	((special & MG_DETECT) && ui_flags.use_inverse)) {
	term_start_attr(ATR_INVERSE);
	reverse_on = TRUE;
    }

	g_putch(ch);		/* print the character */

    if (reverse_on) {
    	term_end_attr(ATR_INVERSE);
	/* turn off color as well, ATR_INVERSE may have done this already */
	if(ttyDisplay->color != NO_COLOR) {
	    term_end_color();
	    ttyDisplay->color = NO_COLOR;
	}
    }

    wins[window]->curx++;	/* one character over */
    ttyDisplay->curx++;		/* the real cursor moved too */
}

void tty_raw_print(const char *str)
{
    if(ttyDisplay) ttyDisplay->rawprint++;
#if defined(WIN32CON)
    msmsg("%s\n", str);
#else
    puts(str);
    fflush(stdout);
#endif
}

void tty_raw_print_bold(const char *str)
{
    if(ttyDisplay)
	ttyDisplay->rawprint++;
    term_start_raw_bold();
#if defined(WIN32CON)
    msmsg("%s", str);
#else
    fputs(str, stdout);
#endif
    term_end_raw_bold();
#if defined(WIN32CON)
    msmsg("\n");
#else
    puts("");
    fflush(stdout);
#endif
}

int base_nhgetch(void)
{
    int i;
#ifdef UNIX
    /* kludge alert: Some Unix variants return funny values if getc()
     * is called, interrupted, and then called again.  There
     * is non-reentrant code in the internal _filbuf() routine, called by
     * getc().
     */
    static volatile int nesting = 0;
    char nestbuf;
#endif

    fflush(stdout);
    /* Note: if raw_print() and wait_synch() get called to report terminal
     * initialization problems, then wins[] and ttyDisplay might not be
     * available yet.  Such problems will probably be fatal before we get
     * here, but validate those pointers just in case...
     */
    if (WIN_MESSAGE != WIN_ERR && wins[WIN_MESSAGE])
	    wins[WIN_MESSAGE]->flags &= ~WIN_STOP;
    
#ifdef UNIX
    i = ((++nesting == 1) ? getchar() :
	(read(fileno(stdin), &nestbuf,1) == 1 ? (int)nestbuf :
								EOF));
    --nesting;
#else
    i = getchar();
#endif
    
    return i;
}


int tty_nhgetch(void)
{
    int i;

    i = base_nhgetch();

    if (i == 'O') {
	display_options(FALSE);
	i = '\033'; /* NetHack doesn't know 'O' */
    }
    
    if (!i) i = '\033'; /* map NUL to ESC since nethack doesn't expect NUL */
    if (ttyDisplay && ttyDisplay->toplin == 1)
	ttyDisplay->toplin = 2;
    return i;
}

/*
 * return a key, or 0, in which case a mouse button was pressed
 * mouse events should be returned as character postitions in the map window.
 * Since normal tty's don't have mice, just return a key.
 */
/*ARGSUSED*/
int tty_nh_poskey(int *x, int *y, int *mod)
{
# if defined(WIN32CON)
    int i;
    fflush(stdout);
    /* Note: if raw_print() and wait_synch() get called to report terminal
     * initialization problems, then wins[] and ttyDisplay might not be
     * available yet.  Such problems will probably be fatal before we get
     * here, but validate those pointers just in case...
     */
    if (WIN_MESSAGE != WIN_ERR && wins[WIN_MESSAGE])
	    wins[WIN_MESSAGE]->flags &= ~WIN_STOP;
    i = ntposkey(x, y, mod);
    if (!i && mod && *mod == 0)
    	i = '\033'; /* map NUL to ESC since nethack doesn't expect NUL */
    if (ttyDisplay && ttyDisplay->toplin == 1)
		ttyDisplay->toplin = 2;
    return i;
# else
    return base_nhgetch();
# endif
}

void win_tty_init(void)
{
# if defined(WIN32CON)
    nttty_open();
# endif
    return;
}

/*
 * Allocate a copy of the given string.  If null, return a string of
 * zero length.
 *
 * This is an exact duplicate of copy_of() in X11/winmenu.c.
 */
static char * copy_of(const char *s)
{
    if (!s) s = "";
    return strcpy(malloc((unsigned) (strlen(s) + 1)), s);
}

/*============================================================================*/

/* A normal tombstone for end of game display. */
static const char *rip_txt[] = {
"                       ----------",
"                      /          \\",
"                     /    REST    \\",
"                    /      IN      \\",
"                   /     PEACE      \\",
"                  /                  \\",
"                  |                  |", /* Name of player */
"                  |                  |", /* Amount of $ */
"                  |                  |", /* Type of death */
"                  |                  |", /* . */
"                  |                  |", /* . */
"                  |                  |", /* . */
"                  |       1001       |", /* Real year of death */
"                 *|     *  *  *      | *",
"        _________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______",
0
};
#define STONE_LINE_CENT 28	/* char[] element of center of stone face */
#define STONE_LINE_LEN 16	/* # chars that fit on one line
				 * (note 1 ' ' border)
				 */
#define NAME_LINE 6		/* *char[] line # for player name */
#define GOLD_LINE 7		/* *char[] line # for amount of gold */
#define DEATH_LINE 8		/* *char[] line # for death description */
#define YEAR_LINE 12		/* *char[] line # for year */

static void center(char *line, char *text)
{
	char *ip,*op;
	ip = text;
	op = &line[STONE_LINE_CENT - ((strlen(text)+1)>>1)];
	while(*ip) *op++ = *ip++;
}


void tty_outrip(struct nh_menuitem *items,int icount, int how, char *plname, long gold,
		char *killbuf, int year)
{
	char **dp, **rip;
	char *dpx;
	char buf[BUFSZ];
	int x, i;
	int line;
	winid tmpwin = tty_create_nhwindow(NHW_TEXT);
	

	rip = dp = malloc(sizeof(rip_txt));
	for (x = 0; rip_txt[x]; x++) {
		dp[x] = malloc((unsigned int)(strlen(rip_txt[x]) + 1));
		strcpy(dp[x], rip_txt[x]);
	}
	dp[x] = NULL;

	/* Put name on stone */
	sprintf(buf, "%s", plname);
	buf[STONE_LINE_LEN] = 0;
	center(rip[NAME_LINE], buf);

	/* Put $ on stone */
	sprintf(buf, "%ld Au", gold);
// #ifndef GOLDOBJ
// 	sprintf(buf, "%ld Au", u.ugold);
// #else
// 	sprintf(buf, "%ld Au", done_money);
// #endif
	buf[STONE_LINE_LEN] = 0; /* It could be a *lot* of gold :-) */
	center(rip[GOLD_LINE], buf);

	strcpy(buf, killbuf);
	/* Put death type on stone */
	for (line=DEATH_LINE, dpx = buf; line<YEAR_LINE; line++) {
		int i,i0;
		char tmpchar;

		if ( (i0=strlen(dpx)) > STONE_LINE_LEN) {
				for(i = STONE_LINE_LEN;
				    ((i0 > STONE_LINE_LEN) && i); i--)
					if(dpx[i] == ' ') i0 = i;
				if(!i) i0 = STONE_LINE_LEN;
		}
		tmpchar = dpx[i0];
		dpx[i0] = 0;
		center(rip[line], dpx);
		if (tmpchar != ' ') {
			dpx[i0] = tmpchar;
			dpx= &dpx[i0];
		} else  dpx= &dpx[i0+1];
	}

	/* Put year on stone */
	sprintf(buf, "%4d", year);
	center(rip[YEAR_LINE], buf);

	tty_putstr(tmpwin, 0, "");
	for(; *dp; dp++)
		tty_putstr(tmpwin, 0, *dp);

	tty_putstr(tmpwin, 0, "");
	tty_putstr(tmpwin, 0, "");

	for (x = 0; rip_txt[x]; x++) {
		free(rip[x]);
	}
	free(rip);
	rip = NULL;
	
	for (i = 0; i < icount; i++)
	    tty_putstr(tmpwin, 0, items[i].caption);
	
	display_nhwindow(tmpwin, TRUE);
	tty_destroy_nhwindow(tmpwin);
}


/*============================================================================*/

/* MAXCO must hold longest uncompressed status line, and must be larger
 * than COLNO
 *
 * longest practical second status line at the moment is
 *	Astral Plane $:12345 HP:700(700) Pw:111(111) AC:-127 Xp:30/123456789
 *	T:123456 Satiated Conf FoodPois Ill Blind Stun Hallu Overloaded
 * -- or somewhat over 130 characters
 */
#if COLNO <= 140
#define MAXCO 160
#else
#define MAXCO (COLNO+20)
#endif

static void bot1(struct nh_status_info *s)
{
	char newbot1[MAXCO];
	char *nb = newbot1;
	int i,j;

	strcpy(newbot1, s->plname);
	if('a' <= newbot1[0] && newbot1[0] <= 'z')
	    newbot1[0] += 'A'-'a';
	newbot1[10] = '\0';
	nb += strlen(newbot1);
	sprintf(nb, " the %s", s->rank);

	strncat(nb, "  ", MAXCO);
	i = s->mrank_sz + 15;
	j = strlen(newbot1);
	if((i - j) > 0)
		sprintf(nb += strlen(nb), "%*s", i-j, " ");	/* pad with spaces */
	
	if (s->st == 18) {
		if (s->st_extra < 100)
		    sprintf(nb += strlen(nb), "St:18/%02d ", s->st_extra);
		else
		    sprintf(nb += strlen(nb),"St:18/** ");
	} else
		sprintf(nb += strlen(nb), "St:%-1d ", s->st);
	
	sprintf(nb += strlen(nb), "Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",
		s->dx, s->co, s->in, s->wi, s->ch);
	sprintf(nb += strlen(nb), (s->align == A_CHAOTIC) ? "  Chaotic" :
			(s->align == A_NEUTRAL) ? "  Neutral" : "  Lawful");
	
	if (ui_flags.showscore)
	    sprintf(nb += strlen(nb), " S:%ld", s->score);
	
	move_cursor(WIN_STATUS, 1, 0);
	tty_putstr(WIN_STATUS, 0, newbot1);
}

static void bot2(struct nh_status_info *s)
{
	char newbot2[MAXCO];
	char *nb = newbot2;
	int i;

	strncpy(newbot2, s->level_desc, MAXCO);
	sprintf(nb += strlen(newbot2),
		"%c:%-2ld HP:%d(%d) Pw:%d(%d) AC:%-2d", s->coinsym,
		s->gold, s->hp, s->hpmax, s->en, s->enmax, s->ac);

	if (s->polyd)
		sprintf(nb += strlen(nb), " HD:%d", s->level);
	else if(ui_flags.showexp)
		sprintf(nb += strlen(nb), " Xp:%u/%-1ld", s->level, s->xp);
	else
		sprintf(nb += strlen(nb), " Exp:%u", s->level);

	if(ui_flags.time)
	    sprintf(nb += strlen(nb), " T:%ld", s->moves);
	
	for (i = 0; i < s->nr_items; i++)
	    sprintf(nb += strlen(nb), " %s", s->items[i]);
	
	move_cursor(WIN_STATUS, 1, 1);
	tty_putstr(WIN_STATUS, 0, newbot2);
}

void tty_update_status(struct nh_status_info *status)
{
    if (status != &cached_status)
	memcpy(&cached_status, status, sizeof(*status));
    
    bot1(status);
    bot2(status);
}


void tty_print_message(const char *msg)
{
    tty_putstr(WIN_MESSAGE, 0, msg);
}

/*wintty.c*/
