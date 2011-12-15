/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>
#include <time.h>

#define LISTSZ 32
nh_bool player_selection(int *out_role, int *out_race, int *out_gend,
			 int *out_align, int randomall)
{
    struct nh_menuitem *items;
    int icount, size;
    int i, k, n, listlen, id;
    char pick4u = 'n', thisch, lastch = 0;
    char pbuf[QBUFSZ], plbuf[QBUFSZ];
    struct nh_listitem list[LISTSZ]; /* need enough space for lists of roles or races */
    char listbuffers[LISTSZ][256];
    int pick_list[2];
    int initrole, initrace, initgend, initalign;
    
    initrole = *out_role; initrace = *out_race;
    initalign = *out_align; initgend = *out_gend;
    nh_get_role_defaults(&initrole, &initrace, &initgend, &initalign);
    
    for (i = 0; i < LISTSZ; i++) {
	listbuffers[i][0] = '\0';
	list[i].caption = listbuffers[i];
    }
    
    srand(time(NULL));
    
    /* Should we randomly pick for the player? */
    if (!randomall &&
	(initrole == ROLE_NONE || initrace == ROLE_NONE ||
	    initgend == ROLE_NONE || initalign == ROLE_NONE)) {
	char *prompt = nh_build_plselection_prompt(pbuf, QBUFSZ, initrole,
			    initrace, initgend, initalign);

	pick4u = curses_yn_function(prompt, "ynq", 'y');	
	if (pick4u != 'y' && pick4u != 'n')
	    return FALSE;
    }

    nh_root_plselection_prompt(plbuf, QBUFSZ - 1,
		    initrole, initrace, initgend, initalign);
    icount = 0; size = 10;
    items = malloc(sizeof(struct nh_menuitem) * size);

    /* Select a role, if necessary */
    /* we'll try to be compatible with pre-selected race/gender/alignment,
	* but may not succeed */
    if (initrole < 0) {
	listlen = nh_get_valid_roles(initrace, initgend, initalign, list, LISTSZ);
	if (listlen == 0) {
	    curses_msgwin("Incompatible role!");
	    listlen = nh_get_valid_roles(ROLE_NONE, ROLE_NONE, ROLE_NONE, list, LISTSZ);
	}
	
	/* Process the choice */
	if (pick4u == 'y' || initrole == ROLE_RANDOM || randomall) {
	    /* Pick a random role */
	    initrole = list[random() % listlen].id;
	} else {
	    /* Prompt for a role */
	    for (i = 0; i < listlen; i++) {
		id = list[i].id + 1; /* list[i].id starts at 0 */
		thisch = tolower(*list[i].caption);
		if (thisch == lastch)
		    thisch = toupper(thisch);
		add_menu_item(items, size, icount, id, list[i].caption, thisch, 0);
		lastch = thisch;
	    }
	    id = list[random() % listlen].id+1;
	    add_menu_item(items, size, icount, id, "Random", '*', 0);
	    add_menu_item(items, size, icount, -1, "Quit", '*', 0);
	    
	    sprintf(pbuf, "Pick a role for your %s", plbuf);
	    n = curses_display_menu(items, icount, pbuf, PICK_ONE, pick_list);

	    /* Process the choice */
	    if (n != 1 || pick_list[0] == -1)
		goto give_up;		/* Selected quit */

	    initrole = pick_list[0] - 1;
	    icount = 0;
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
	    curses_msgwin("Incompatible race!");
	    listlen = nh_get_valid_races(initrole, ROLE_NONE, ROLE_NONE, list, LISTSZ);
	}
	
	if (pick4u == 'y' || initrace == ROLE_RANDOM || randomall) {
	    initrace = list[random() % listlen].id;
	} else {	/* pick4u == 'n' */
	    /* Count the number of valid races */
	    k = list[0].id;	/* valid race */

	    /* Permit the user to pick, if there is more than one */
	    if (listlen > 1) {
		for (i = 0; i < listlen; i++) {
		    id = list[i].id + 1;
		    add_menu_item(items, size, icount, id, list[i].caption,
				  list[i].caption[0], 0);
		}
		id = list[random() % listlen].id+1;
		add_menu_item(items, size, icount, id, "Random", '*', 0);
		add_menu_item(items, size, icount, -1, "Quit", '*', 0);

		sprintf(pbuf, "Pick the race of your %s", plbuf);
		n = curses_display_menu(items, icount, pbuf, PICK_ONE, pick_list);
		
		if (n != 1 || pick_list[0] == -1)
		    goto give_up;		/* Selected quit */

		k = pick_list[0] - 1;
		icount = 0;
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
	    curses_msgwin("Incompatible gender!");
	    listlen = nh_get_valid_genders(initrole, initrace, ROLE_NONE, list, LISTSZ);
	}
	if (pick4u == 'y' || initgend == ROLE_RANDOM || randomall) {
	    initgend = list[random() % listlen].id;
	} else {	/* pick4u == 'n' */
	    /* Count the number of valid genders */
	    k = list[0].id;	/* valid gender */

	    /* Permit the user to pick, if there is more than one */
	    if (listlen > 1) {
		for (i = 0; i < listlen; i++) {
		    id = list[i].id + 1;
		    add_menu_item(items, size, icount, id, list[i].caption,
				  list[i].caption[0], 0);
		}
		id = list[random() % listlen].id+1;
		add_menu_item(items, size, icount, id, "Random", '*', 0);
		add_menu_item(items, size, icount, -1, "Quit", '*', 0);

		sprintf(pbuf, "Pick the gender of your %s", plbuf);
		n = curses_display_menu(items, icount, pbuf, PICK_ONE, pick_list);
		
		if (n != 1 || pick_list[0] == -1)
		    goto give_up;		/* Selected quit */

		k = pick_list[0] - 1;
		icount = 0;
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
		for (i = 0; i < listlen; i++) {
		    id = list[i].id + 1;
		    add_menu_item(items, size, icount, id, list[i].caption,
				  list[i].caption[0], 0);
		}
		id = list[random() % listlen].id+1;
		add_menu_item(items, size, icount, id, "Random", '*', 0);
		add_menu_item(items, size, icount, -1, "Quit", '*', 0);
		
		sprintf(pbuf, "Pick the alignment of your %s", plbuf);
		n = curses_display_menu(items, icount, pbuf, PICK_ONE, pick_list);
		
		if (n != 1 || pick_list[0] == -1)
		    goto give_up;		/* Selected quit */

		k = pick_list[0] - 1;
	    }
	    initalign = k;
	}
    }
    
    *out_role = initrole;
    *out_race = initrace;
    *out_gend = initgend;
    *out_align = initalign;
    
    free(items);
    return TRUE;
    
give_up:
    free(items);
    return FALSE;
}
