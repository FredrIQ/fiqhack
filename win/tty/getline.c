/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nethack.h"
#include "wintty.h"

struct extcmd_hook_args {
    const char **namelist;
    const char **desclist;
    int listlen;
};

char morc = 0;	/* tell the outside world what char you chose */
static boolean ext_cmd_getlin_hook(char *, void *);

typedef boolean (*getlin_hook_proc)(char *, void *);

static void hooked_tty_getlin(const char*, char*, getlin_hook_proc, void *);

extern char erase_char, kill_char;	/* from appropriate tty.c file */

/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
void tty_getlin(const char *query, char *bufp)
{
    hooked_tty_getlin(query, bufp, NULL, NULL);
}

static void hooked_tty_getlin(const char *query, char *bufp,
			      getlin_hook_proc hook, void *hook_proc_arg)
{
	char *obufp = bufp;
	char qbuf[BUFSZ];
	int c;
	struct WinDesc *cw = wins[WIN_MESSAGE];
	boolean doprev = 0;

	if(ttyDisplay->toplin == 1 && !(cw->flags & WIN_STOP)) more();
	cw->flags &= ~WIN_STOP;
	ttyDisplay->toplin = 3; /* special prompt state */
	ttyDisplay->inread++;
	sprintf(qbuf, "%s ", query);
	tty_putstr(WIN_MESSAGE, 0, qbuf);
	*obufp = 0;
	for(;;) {
		fflush(stdout);
		sprintf(toplines, "%s ", query);
		strcat(toplines, obufp);
		if((c = base_nhgetch()) == EOF) {
			break;
		}
		if(c == '\033') {
			*obufp = c;
			obufp[1] = 0;
			break;
		}
		if (ttyDisplay->intr) {
		    ttyDisplay->intr--;
		    *bufp = 0;
		}
		if(c == '\020') { /* ctrl-P */
		    if (ui_flags.prevmsg_window != 's') {
			int sav = ttyDisplay->inread;
			ttyDisplay->inread = 0;
			tty_doprev_message();
			ttyDisplay->inread = sav;
			clear_nhwindow(WIN_MESSAGE);
			cw->maxcol = cw->maxrow;
			addtopl(query);
			addtopl(" ");
			*bufp = 0;
			addtopl(obufp);
		    } else {
			if (!doprev)
			    tty_doprev_message();/* need two initially */
			tty_doprev_message();
			doprev = 1;
			continue;
		    }
		} else if (doprev && ui_flags.prevmsg_window == 's') {
		    clear_nhwindow(WIN_MESSAGE);
		    cw->maxcol = cw->maxrow;
		    doprev = 0;
		    addtopl(query);
		    addtopl(" ");
		    *bufp = 0;
		    addtopl(obufp);
		}
		if(c == erase_char || c == '\b') {
			if(bufp != obufp) {
				char *i;

				bufp--;
				putsyms("\b");
				for (i = bufp; *i; ++i) putsyms(" ");
				for (; i > bufp; --i) putsyms("\b");
				*bufp = 0;
			} else	tty_nhbell();
		} else if(c == '\n') {
			break;
		} else if(' ' <= (unsigned char) c && c != '\177' &&
			    (bufp-obufp < BUFSZ-1 && bufp-obufp < COLNO)) {
				/* avoid isprint() - some people don't have it
				   ' ' is not always a printing char */
			char *i = bufp + strlen(bufp);

			*bufp = c;
			bufp[1] = 0;
			putsyms(bufp);
			bufp++;
			if (hook && (*hook)(obufp, hook_proc_arg)) {
			    putsyms(bufp);
			    /* pointer and cursor left where they were */
			    for (i = bufp; *i; ++i) putsyms("\b");
			} else if (i > bufp) {
			    char *s = i;

			    /* erase rest of prior guess */
			    for (; i > bufp; --i) putsyms(" ");
			    for (; s > bufp; --s) putsyms("\b");
			}
		} else if(c == kill_char || c == '\177') { /* Robert Viduya */
				/* this test last - @ might be the kill_char */
			for (; *bufp; ++bufp) putsyms(" ");
			for (; bufp != obufp; --bufp) putsyms("\b \b");
			*bufp = 0;
		} else
			tty_nhbell();
	}
	ttyDisplay->toplin = 2;		/* nonempty, no --More-- required */
	ttyDisplay->inread--;
	clear_nhwindow(WIN_MESSAGE);	/* clean up after ourselves */
}


/* s: chars allowed besides return */
void xwaitforspace(const char *s)
{
    int c, x = ttyDisplay ? (int) ttyDisplay->dismiss_more : '\n';

    morc = 0;

    while((c = base_nhgetch()) != '\n') {
	if(ui_flags.cbreak) {
	    if ((s && index(s,c)) || c == x) {
		morc = (char) c;
		break;
	    }
	    tty_nhbell();
	}
    }

}


/*
 * This is controlled via runtime option 'extmenu'
 * we get here after # - now show pick-list of possible commands */
static int extcmd_via_menu(const char **namelist, const char **desclist, int listlen)	
{
    menu_item *pick_list = NULL;
    winid win;
    anything any;
    int choices[listlen+1];
    char buf[BUFSZ];
    char cbuf[QBUFSZ], prompt[QBUFSZ], fmtstr[20];
    int i, n, nchoices, acount;
    int ret,  biggest;
    int accelerator, prevaccelerator;
    int  matchlevel = 0;

    ret = 0;
    cbuf[0] = '\0';
    biggest = 0;
    while (!ret) {
	    nchoices = n = 0;
	    accelerator = 0;
	    any.a_void = 0;
	    /* populate choices */
	    for (i = 0; i < listlen; i++) {
// 	    for(efp = extcmdlist; efp->ef_txt; efp++) {
		if (!matchlevel || !strncmp(namelist[i], cbuf, matchlevel)) {
			choices[nchoices++] = i;
			if (strlen(desclist[i]) > biggest) {
				biggest = strlen(desclist[i]);
				sprintf(fmtstr,"%%-%ds", biggest + 15);
			}
		}
	    }
	    choices[nchoices] = -1;
	    
	    /* if we're down to one, we have our selection so get out of here */
	    if (nchoices == 1) {
		for (i = 0; i < listlen; i++)
			if (!strncmp(namelist[i], cbuf, matchlevel)) {
				ret = i;
				break;
			}
		break;
	    }

	    /* otherwise... */
	    win = tty_create_nhwindow(NHW_MENU);
	    tty_start_menu(win);
	    prevaccelerator = 0;
	    acount = 0;
	    for(i = 0; i < listlen && choices[i] >= 0; ++i) {
		accelerator = namelist[choices[i]][matchlevel];
		if (accelerator != prevaccelerator || nchoices < (ROWNO - 3)) {
		    if (acount) {
 			/* flush the extended commands for that letter already in buf */
			sprintf(buf, fmtstr, prompt);
			any.a_char = prevaccelerator;
			tty_add_menu(win, NO_GLYPH, &any, any.a_char, 0,
					ATR_NONE, buf, FALSE);
			acount = 0;
		    }
		}
		prevaccelerator = accelerator;
		if (!acount || nchoices < (ROWNO - 3)) {
		    sprintf(prompt, "%s [%s]", namelist[choices[i]],
				desclist[choices[i]]);
		} else if (acount == 1) {
		    sprintf(prompt, "%s or %s", namelist[choices[i-1]],
				namelist[choices[i]]);
		} else {
		    strcat(prompt," or ");
		    strcat(prompt, namelist[choices[i]]);
		}
		++acount;
	    }
	    if (acount) {
		/* flush buf */
		sprintf(buf, fmtstr, prompt);
		any.a_char = prevaccelerator;
		tty_add_menu(win, NO_GLYPH, &any, any.a_char, 0, ATR_NONE, buf, FALSE);
	    }
	    sprintf(prompt, "Extended Command: %s", cbuf);
	    tty_end_menu(win, prompt);
	    n = tty_select_menu(win, PICK_ONE, &pick_list);
	    tty_destroy_nhwindow(win);
	    if (n==1) {
		if (matchlevel > (QBUFSZ - 2)) {
			free(pick_list);
#ifdef DEBUG
			impossible("Too many characters (%d) entered in extcmd_via_menu()",
				matchlevel);
#endif
			ret = -1;
		} else {
			cbuf[matchlevel++] = pick_list[0].item.a_char;
			cbuf[matchlevel] = '\0';
			free(pick_list);
		}
	    } else {
		if (matchlevel) {
			ret = 0;
			matchlevel = 0;
		} else
			ret = -1;
	    }
    }
    return ret;
}


/*
 * Implement extended command completion by using this hook into
 * tty_getlin.  Check the characters already typed, if they uniquely
 * identify an extended command, expand the string to the whole
 * command.
 *
 * Return TRUE if we've extended the string at base.  Otherwise return FALSE.
 * Assumptions:
 *
 *	+ we don't change the characters that are already in base
 *	+ base has enough room to hold our string
 */
static boolean ext_cmd_getlin_hook(char *base, void *hook_arg)
{
	int oindex, com_index;
	struct extcmd_hook_args *hpa = hook_arg;

	com_index = -1;
	for (oindex = 0; oindex < hpa->listlen; oindex++) {
		if (!strncmp(base, hpa->namelist[oindex], strlen(base))) {
			if (com_index == -1)	/* no matches yet */
			    com_index = oindex;
			else			/* more than 1 match */
			    return FALSE;
		}
	}
	if (com_index >= 0) {
		strcpy(base, hpa->namelist[com_index]);
		return TRUE;
	}

	return FALSE;	/* didn't match anything */
}

/* remove excess whitespace from a string buffer (in place) */
char *mungspaces(char *bp)
{
    char c, *p, *p2;
    boolean was_space = TRUE;

    for (p = p2 = bp; (c = *p) != '\0'; p++) {
	if (c == '\t') c = ' ';
	if (c != ' ' || !was_space) *p2++ = c;
	was_space = (c == ' ');
    }
    if (was_space && p2 > bp) p2--;
    *p2 = '\0';
    return bp;
}

/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
int tty_get_ext_cmd(const char **namelist, const char **desclist, int listlen)
{
	int i;
	char buf[BUFSZ];
	struct extcmd_hook_args hpa = {namelist, desclist, listlen};

	if (ui_flags.extmenu)
	    return extcmd_via_menu(namelist, desclist, listlen);
	/* maybe a runtime option? */
	hooked_tty_getlin("#", buf, ext_cmd_getlin_hook, &hpa);
	mungspaces(buf);
	if (buf[0] == 0 || buf[0] == '\033')
	    return -1;

	for (i = 0; i < listlen; i++)
	    if (!strcmp(buf, namelist[i])) break;


	if (namelist[i] == NULL) {
	    char msg[BUFSZ];
	    sprintf(msg, "%s: unknown extended command.", buf);
	    tty_putstr(WIN_MESSAGE, 0, msg);
	    i = -1;
	}

	return i;
}

/*getline.c*/
