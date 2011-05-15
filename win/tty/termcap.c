/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "nethack.h"
#include "color.h"

#if !defined(NO_TERMS)

#include "wintty.h"
#include "tcap.h"

#define SIZE(x) (int)(sizeof(x) / sizeof(x[0]))
#define Tgetstr(key) (tgetstr(key,&tbufptr))

static char * s_atr2str(int);
static char * e_atr2str(int);

void cmov(int, int);
void nocmov(int, int);
#  if !defined(UNIX)
static void analyze_seq(char *, int *, int *);
#  endif
static void init_hilite(void);
static void kill_hilite(void);

	/* (see tcap.h) -- nh_CM, nh_ND, nh_CD, nh_HI,nh_HE, nh_US,nh_UE,
				ul_hack */
struct tc_lcl_data tc_lcl_data = { 0, 0, 0, 0,0, 0,0, FALSE };

static char *HO, *CL, *CE, *UP, *XD, *BC, *SO, *SE, *TI, *TE;
static char *VS, *VE;
static char *ME;
static char *MR;
static char *MD;
static int SG;
static char PC = '\0';
static char tbuf[512];

char *hilites[CLR_MAX]; /* terminal escapes for the various colors */

static char *KS = NULL, *KE = NULL;	/* keypad sequences */
static char nullstr[] = "";

#if !defined(NO_TERMS)
extern boolean HE_resets_AS;
#endif


void tty_startup(int *wid, int *hgt)
{
	int i;
	const char *term;
	char *tptr;
	char *tbufptr, *pc;

	term = getenv("TERM");

	if (!term)
#ifndef ANSI_DEFAULT
		error("Can't get TERM.");
#else
	{
		HO = "\033[H";
/*		nh_CD = "\033[J"; */
		CE = "\033[K";		/* the ANSI termcap */
		nh_CM = "\033[%i%d;%dH";
		UP = "\033[A";
		nh_ND = "\033[C";
		XD = "\033[B";
		BC = "\033[D";
		nh_HI = SO = "\033[1m";
		nh_US = "\033[4m";
		MR = "\033[7m";
		TI = nh_HE = ME = SE = nh_UE = "\033[0m";
		/* strictly, SE should be 2, and nh_UE should be 24,
		   but we can't trust all ANSI emulators to be
		   that complete.  -3. */
		AS = "\016";
		AE = "\017";
		TE = VS = VE = nullstr;
		for (i = 0; i < CLR_MAX / 2; i++)
		    if (i != CLR_BLACK) {
			hilites[i|BRIGHT] = malloc(sizeof("\033[1;3%dm"));
			sprintf(hilites[i|BRIGHT], "\033[1;3%dm", i);
			if (i != CLR_GRAY)
			    {
				hilites[i] = malloc(sizeof("\033[0;3%dm"));
				sprintf(hilites[i], "\033[0;3%dm", i);
			    }
		    }
		*wid = CO;
		*hgt = LI;
		CL = "\033[2J";		/* last thing set */
		return;
	}
#endif /* ANSI_DEFAULT */

	tptr = (char *) malloc(1024);

	tbufptr = tbuf;
	if(tgetent(tptr, term) < 1) {
		char buf[BUFSZ];
		strncpy(buf, term,
				(BUFSZ - 1) - (sizeof("Unknown terminal type: .  ")));
		buf[BUFSZ-1] = '\0';
		error("Unknown terminal type: %s.", term);
	}
	if ((pc = Tgetstr("pc")) != 0)
		PC = *pc;

	if(!(BC = Tgetstr("le")))	/* both termcap and terminfo use le */
	    error("Terminal must backspace.");
	    if(!(BC = Tgetstr("bc"))) {	/* termcap also uses bc/bs */
		if(!tgetflag("bs"))
			error("Terminal must backspace.");
		BC = tbufptr;
		tbufptr += 2;
		*BC = '\b';
	    }

	HO = Tgetstr("ho");
	/*
	 * LI and CO are set in ioctl.c via a TIOCGWINSZ if available.  If
	 * the kernel has values for either we should use them rather than
	 * the values from TERMCAP ...
	 */
	if (!CO) CO = tgetnum("co");
	if (!LI) LI = tgetnum("li");
	nh_ND = Tgetstr("nd");
	if(tgetflag("os"))
		error("NetHack can't have OS.");
	if(tgetflag("ul"))
		ul_hack = TRUE;
	CE = Tgetstr("ce");
	UP = Tgetstr("up");
	/* It seems that xd is no longer supported, and we should use
	   a linefeed instead; unfortunately this requires resetting
	   CRMOD, and many output routines will have to be modified
	   slightly. Let's leave that till the next release. */
	XD = Tgetstr("xd");
/* not:		XD = Tgetstr("do"); */
	if(!(nh_CM = Tgetstr("cm"))) {
	    if(!UP && !HO)
		error("NetHack needs CM or UP or HO.");
	    tty_raw_print("Playing NetHack on terminals without CM is suspect.");
	    tty_wait_synch();
	}
	SO = Tgetstr("so");
	SE = Tgetstr("se");
	nh_US = Tgetstr("us");
	nh_UE = Tgetstr("ue");
	SG = tgetnum("sg");	/* -1: not fnd; else # of spaces left by so */
	if(!SO || !SE || (SG > 0)) SO = SE = nh_US = nh_UE = nullstr;
	TI = Tgetstr("ti");
	TE = Tgetstr("te");
	VS = VE = nullstr;
	VS = Tgetstr("eA");	/* enable graphics */
	KS = Tgetstr("ks");	/* keypad start (special mode) */
	KE = Tgetstr("ke");	/* keypad end (ordinary mode [ie, digits]) */
	MR = Tgetstr("mr");	/* reverse */
	ME = Tgetstr("me");	/* turn off all attributes */
	if (!ME || (SE == nullstr)) ME = SE;	/* default to SE value */

	/* Get rid of padding numbers for nh_HI and nh_HE.  Hope they
	 * aren't really needed!!!  nh_HI and nh_HE are outputted to the
	 * pager as a string - so how can you send it NULs???
	 *  -jsb
	 */
	nh_HI = malloc((unsigned)(strlen(SO)+1));
	nh_HE = malloc((unsigned)(strlen(ME)+1));
	i = 0;
	while (isdigit(SO[i])) i++;
	strcpy(nh_HI, &SO[i]);
	i = 0;
	while (isdigit(ME[i])) i++;
	strcpy(nh_HE, &ME[i]);
	AS = Tgetstr("as");
	AE = Tgetstr("ae");
	nh_CD = Tgetstr("cd");
	MD = Tgetstr("md");
	
	init_hilite();
	
	*wid = CO;
	*hgt = LI;
	if (!(CL = Tgetstr("cl")))	/* last thing set */
		error("NetHack needs CL.");
	if ((int)(tbufptr - tbuf) > (int)(sizeof tbuf))
		error("TERMCAP entry too big...\n");
	free(tptr);
}

/* note: at present, this routine is not part of the formal window interface */
/* deallocate resources prior to final termination */
void tty_shutdown(void)
{
	kill_hilite();
	/* we don't attempt to clean up individual termcap variables [yet?] */
	return;
}

void tty_number_pad(int state)
{
	switch (state) {
	    case -1:	/* activate keypad mode (escape sequences) */
		    if (KS && *KS) xputs(KS);
		    break;
	    case  1:	/* activate numeric mode for keypad (digits) */
		    if (KE && *KE) xputs(KE);
		    break;
	    case  0:	/* don't need to do anything--leave terminal as-is */
	    default:
		    break;
	}
}

static void tty_decgraphics_termcap_fixup(void);

/*
   We call this routine whenever DECgraphics mode is enabled, even if it
   has been previously set, in case the user manages to reset the fonts.
   The actual termcap fixup only needs to be done once, but we can't
   call xputs() from the option setting or graphics assigning routines,
   so this is a convenient hook.
 */
static void tty_decgraphics_termcap_fixup(void)
{
	static char ctrlN[]   = "\016";
	static char ctrlO[]   = "\017";
	static char appMode[] = "\033=";
	static char numMode[] = "\033>";

	/* these values are missing from some termcaps */
	if (!AS) AS = ctrlN;	/* ^N (shift-out [graphics font]) */
	if (!AE) AE = ctrlO;	/* ^O (shift-in  [regular font])  */
	if (!KS) KS = appMode;	/* ESC= (application keypad mode) */
	if (!KE) KE = numMode;	/* ESC> (numeric keypad mode)	  */
	/*
	 * Select the line-drawing character set as the alternate font.
	 * Do not select NA ASCII as the primary font since people may
	 * reasonably be using the UK character set.
	 */
	if (iflags2.DECgraphics) xputs("\033)0");

#if !defined(NO_TERMS)
	/* some termcaps suffer from the bizarre notion that resetting
	   video attributes should also reset the chosen character set */
    {
	const char *nh_he = nh_HE, *ae = AE;
	int he_limit, ae_length;

	if (isdigit(*ae)) {	/* skip over delay prefix, if any */
	    do ++ae; while (isdigit(*ae));
	    if (*ae == '.') { ++ae; if (isdigit(*ae)) ++ae; }
	    if (*ae == '*') ++ae;
	}
	/* can't use nethack's case-insensitive strstri() here, and some old
	   systems don't have strstr(), so use brute force substring search */
	ae_length = strlen(ae), he_limit = strlen(nh_he);
	while (he_limit >= ae_length) {
	    if (strncmp(nh_he, ae, ae_length) == 0) {
		HE_resets_AS = TRUE;
		break;
	    }
	    ++nh_he, --he_limit;
	}
    }
#endif
}

void tty_start_screen(void)
{
	xputs(TI);
	xputs(VS);
	if (iflags2.DECgraphics) tty_decgraphics_termcap_fixup();
	/* set up callback in case option is not set yet but toggled later */
	decgraphics_mode_callback = tty_decgraphics_termcap_fixup;
	if (iflags2.num_pad) tty_number_pad(1);	/* make keypad send digits */
}

void tty_end_screen(void)
{
	clear_screen();
	xputs(VE);
	xputs(TE);
}

/* Cursor movements */


void nocmov(int x, int y)
{
	if ((int) ttyDisplay->cury > y) {
		if(UP) {
			while ((int) ttyDisplay->cury > y) {	/* Go up. */
				xputs(UP);
				ttyDisplay->cury--;
			}
		} else if(nh_CM) {
			cmov(x, y);
		} else if(HO) {
			home();
			move_cursor(BASE_WINDOW, x+1, y);
		} /* else impossible("..."); */
	} else if ((int) ttyDisplay->cury < y) {
		if(XD) {
			while((int) ttyDisplay->cury < y) {
				xputs(XD);
				ttyDisplay->cury++;
			}
		} else if(nh_CM) {
			cmov(x, y);
		} else {
			while((int) ttyDisplay->cury < y) {
				xputc('\n');
				ttyDisplay->curx = 0;
				ttyDisplay->cury++;
			}
		}
	}
	if ((int) ttyDisplay->curx < x) {		/* Go to the right. */
		if(!nh_ND) cmov(x, y); else	/* bah */
			/* should instead print what is there already */
		while ((int) ttyDisplay->curx < x) {
			xputs(nh_ND);
			ttyDisplay->curx++;
		}
	} else if ((int) ttyDisplay->curx > x) {
		while ((int) ttyDisplay->curx > x) {	/* Go to the left. */
			xputs(BC);
			ttyDisplay->curx--;
		}
	}
}

void cmov(int x, int y)
{
	xputs(tgoto(nh_CM, x, y));
	ttyDisplay->cury = y;
	ttyDisplay->curx = x;
}

void xputc(char c)
{
	putchar(c);
}

void xputs(const char *s)
{
	tputs(s, 1, (int (*)())xputc);
}

void cl_end(void)
{
	if(CE)
		xputs(CE);
	else {	/* no-CE fix - free after Harold Rynes */
		/* this looks terrible, especially on a slow terminal
		   but is better than nothing */
		int cx = ttyDisplay->curx+1;

		while(cx < CO) {
			xputc(' ');
			cx++;
		}
		move_cursor(BASE_WINDOW, (int)ttyDisplay->curx+1,
						(int)ttyDisplay->cury);
	}
}


void clear_screen(void)
{
	/* note: if CL is null, then termcap initialization failed,
		so don't attempt screen-oriented I/O during final cleanup.
	 */
	if (CL) {
		xputs(CL);
		home();
	}
}


void home(void)
{
	if(HO)
		xputs(HO);
	else if(nh_CM)
		xputs(tgoto(nh_CM, 0, 0));
	else
		move_cursor(BASE_WINDOW, 1, 0);	/* using UP ... */
	ttyDisplay->curx = ttyDisplay->cury = 0;
}

void standoutbeg(void)
{
	if(SO) xputs(SO);
}

void standoutend(void)
{
	if(SE) xputs(SE);
}

void backsp(void)
{
	xputs(BC);
}

void tty_nhbell(void)
{
}


void graph_on(void)
{
	if (AS) xputs(AS);
}

void graph_off(void)
{
	if (AE) xputs(AE);
}


static const short tmspc10[] = {		/* from termcap */
	0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10, 5
};

/* delay 50 ms */
void tty_delay_output(void)
{
	fflush(stdout);
	msleep(50);		/* sleep for 50 milliseconds */
	return;
}


void cl_eos(void)
{			/* must only be called with curx = 1 */

	if(nh_CD)
		xputs(nh_CD);
	else {
		int cy = ttyDisplay->cury+1;
		while(cy <= LI-2) {
			cl_end();
			xputc('\n');
			cy++;
		}
		cl_end();
		move_cursor(BASE_WINDOW, (int)ttyDisplay->curx+1,
						(int)ttyDisplay->cury);
	}
}

# if defined(UNIX)
/*
 * Sets up color highlighting, using terminfo(4) escape sequences.
 *
 * Having never seen a terminfo system without curses, we assume this
 * inclusion is safe.  On systems with color terminfo, it should define
 * the 8 COLOR_FOOs, and avoid us having to guess whether this particular
 * terminfo uses BGR or RGB for its indexes.
 *
 * If we don't get the definitions, then guess.  Original color terminfos
 * used BGR for the original Sf (setf, Standard foreground) codes, but
 * there was a near-total lack of user documentation, so some subsequent
 * terminfos, such as early Linux ncurses and SCO UNIX, used RGB.  Possibly
 * as a result of the confusion, AF (setaf, ANSI Foreground) codes were
 * introduced, but this caused yet more confusion.  Later Linux ncurses
 * have BGR Sf, RGB AF, and RGB COLOR_FOO, which appears to be the SVR4
 * standard.  We could switch the colors around when using Sf with ncurses,
 * which would help things on later ncurses and hurt things on early ncurses.
 * We'll try just preferring AF and hoping it always agrees with COLOR_FOO,
 * and falling back to Sf if AF isn't defined.
 *
 * In any case, treat black specially so we don't try to display black
 * characters on the assumed black background.
 */

	/* `curses' is aptly named; various versions don't like these
	    macros used elsewhere within nethack; fortunately they're
	    not needed beyond this point, so we don't need to worry
	    about reconstructing them after the header file inclusion. */
#undef delay_output
#undef TRUE
#undef FALSE
#define m_move curses_m_move	/* Some curses.h decl m_move(), not used here */

#include <curses.h>

#  ifdef COLOR_BLACK	/* trust include file */
#undef COLOR_BLACK
#  else
#define COLOR_BLUE    1
#define COLOR_GREEN   2
#define COLOR_CYAN    3
#define COLOR_RED     4
#define COLOR_MAGENTA 5
#define COLOR_YELLOW  6
#define COLOR_WHITE   7
#  endif
#define COLOR_BLACK COLOR_BLUE

const int ti_map[8] = {
	COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
	COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE };

static void init_hilite(void)
{
	int c;
	char *setf, *scratch;

	for (c = 0; c < SIZE(hilites); c++)
		hilites[c] = nh_HI;
	hilites[CLR_GRAY] = hilites[NO_COLOR] = NULL;

	if (tgetnum("Co") < 8
	    || ((setf = tgetstr("AF", NULL)) == NULL
		 && (setf = tgetstr("Sf", NULL)) == NULL))
		return;

	for (c = 0; c < CLR_MAX / 2; c++) {
	    scratch = tparm(setf, ti_map[c]);
	    if (c != CLR_GRAY) {
		hilites[c] = malloc(strlen(scratch) + 1);
		strcpy(hilites[c], scratch);
	    }
	    if (c != CLR_BLACK) {
		hilites[c|BRIGHT] = malloc(strlen(scratch)+strlen(MD)+1);
		strcpy(hilites[c|BRIGHT], MD);
		strcat(hilites[c|BRIGHT], scratch);
	    }

	}
}

# else /* UNIX */

/* find the foreground and background colors set by nh_HI or nh_HE */
static void analyze_seq(char *str, int *fg, int *bg)
{
	int c, code;
	int len;

	*fg = *bg = NO_COLOR;

	c = (str[0] == '\233') ? 1 : 2;	 /* index of char beyond esc prefix */
	len = strlen(str) - 1;		 /* length excluding attrib suffix */
	if ((c != 1 && (str[0] != '\033' || str[1] != '[')) ||
	    (len - c) < 1 || str[len] != 'm')
		return;

	while (c < len) {
	    if ((code = atoi(&str[c])) == 0) { /* reset */
		/* this also catches errors */
		*fg = *bg = NO_COLOR;
	    } else if (code == 1) { /* bold */
		*fg |= BRIGHT;
	    } else if (code == 7 || code == 27) { /* reverse */
		code = *fg & ~BRIGHT;
		*fg = *bg | (*fg & BRIGHT);
		*bg = code;
	    } else if (code >= 30 && code <= 37) { /* hi_foreground RGB */
		*fg = code - 30;
	    } else if (code >= 40 && code <= 47) { /* hi_background RGB */
		*bg = code - 40;
	    }
	    while (digit(str[++c]));
	    c++;
	}
}

/*
 * Sets up highlighting sequences, using ANSI escape sequences (highlight code
 * found in print.c).  The nh_HI and nh_HE sequences (usually from SO) are
 * scanned to find foreground and background colors.
 */

static void init_hilite(void)
{
	int c;
	int backg, foreg, hi_backg, hi_foreg;

	for (c = 0; c < SIZE(hilites); c++)
	    hilites[c] = nh_HI;
	hilites[CLR_GRAY] = hilites[NO_COLOR] = NULL;

	analyze_seq(nh_HI, &hi_foreg, &hi_backg);
	analyze_seq(nh_HE, &foreg, &backg);

	for (c = 0; c < SIZE(hilites); c++)
	    /* avoid invisibility */
	    if ((backg & ~BRIGHT) != c) {
		if (c == foreg)
		    hilites[c] = NULL;
		else if (c != hi_foreg || backg != hi_backg) {
		    hilites[c] = malloc(sizeof("\033[%d;3%d;4%dm"));
		    sprintf(hilites[c], "\033[%d", !!(c & BRIGHT));
		    if ((c | BRIGHT) != (foreg | BRIGHT))
			sprintf(eos(hilites[c]), ";3%d", c & ~BRIGHT);
		    if (backg != CLR_BLACK)
			sprintf(eos(hilites[c]), ";4%d", backg & ~BRIGHT);
		    strcat(hilites[c], "m");
		}
	    }
}
# endif /* UNIX */

static void kill_hilite(void)
{
	int c;

	for (c = 0; c < CLR_MAX / 2; c++) {
	    if (hilites[c|BRIGHT] == hilites[c])  hilites[c|BRIGHT] = 0;
	    if (hilites[c] && (hilites[c] != nh_HI))
		free(hilites[c]),  hilites[c] = 0;
	    if (hilites[c|BRIGHT] && (hilites[c|BRIGHT] != nh_HI))
		free(hilites[c|BRIGHT]),  hilites[c|BRIGHT] = 0;
	}
	return;
}


static char nulstr[] = "";

static char *s_atr2str(int n)
{
    switch (n) {
	    case ATR_ULINE:
		    if(nh_US) return nh_US;
	    case ATR_BOLD:
	    case ATR_BLINK:
		    if (MD)
			return MD;
		    return nh_HI;
	    case ATR_INVERSE:
		    return MR;
    }
    return nulstr;
}

static char *e_atr2str(int n)
{
    switch (n) {
	    case ATR_ULINE:
		    if(nh_UE) return nh_UE;
	    case ATR_BOLD:
	    case ATR_BLINK:
		    return nh_HE;
	    case ATR_INVERSE:
		    return ME;
    }
    return nulstr;
}


void term_start_attr(int attr)
{
	if (attr) {
		xputs(s_atr2str(attr));
	}
}


void term_end_attr(int attr)
{
	if(attr) {
		xputs(e_atr2str(attr));
	}
}


void term_start_raw_bold(void)
{
	xputs(nh_HI);
}


void term_end_raw_bold(void)
{
	xputs(nh_HE);
}


void term_end_color(void)
{
	xputs(nh_HE);
}


void term_start_color(int color)
{
	xputs(hilites[color]);
}


int has_color(int color)
{
	return hilites[color] != NULL;
}

#endif /* NO_TERMS */

/*termcap.c*/
