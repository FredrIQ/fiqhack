/*	SCCS Id: @(#)unixtty.c	3.4	1990/22/02 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* tty.c - (Unix) version */

/* With thanks to the people who sent code for SYSV - hpscdi!jon,
 * arnold@ucsf-cgl, wcs@bo95b, cbcephus!pds and others.
 */

#include "hack.h"

# include <termios.h>
# include <unistd.h>
# define termstruct	termios
# ifdef LINUX
#  include <sys/ioctl.h>
#  undef delay_output	/* curses redefines this */
#  include <curses.h>
# endif
# define kill_sym	c_cc[VKILL]
# define erase_sym	c_cc[VERASE]
# define intr_sym	c_cc[VINTR]
# ifdef TAB3	/* not a POSIX flag, but some have it anyway */
#  define EXTABS	TAB3
# else
#  define EXTABS	0
# endif
# define tabflgs	c_oflag
# define echoflgs	c_lflag
# define cbrkflgs	c_lflag
# define CBRKMASK	ICANON
# define CBRKON		! /* reverse condition */
# define OSPEED(x)	(speednum(cfgetospeed(&x)))
# define IS_7BIT(x)	((x).c_cflag & CS7)
# define inputflags	c_iflag
# define STRIPHI	ISTRIP

# define GTTY(x)	(tcgetattr(0, x))
# define STTY(x)	(tcsetattr(0, TCSADRAIN, x))
# define GTTY2(x)	1
# define STTY2(x)	1

#  if defined(BSD)
#   define nonesuch	_POSIX_VDISABLE
#  else
#   define nonesuch	(fpathconf(0, _PC_VDISABLE))
#  endif
# define inittyb2	inittyb
# define curttyb2	curttyb


#if defined(TTY_GRAPHICS) && (!defined(SYSV) || defined(SVR4))
extern short ospeed;	/* terminal baudrate; set by gettty */
#else
short ospeed = 0;	/* gets around "not defined" error message */
#endif

#if defined(BSD)
unsigned
#endif
	char erase_char, intr_char, kill_char;
static boolean settty_needed = FALSE;
struct termstruct inittyb, curttyb;

static int
speednum(speed)
speed_t speed;
{
	switch (speed) {
		case B0:	return 0;
		case B50:	return 1;
		case B75:	return 2;
		case B110:	return 3;
		case B134:	return 4;
		case B150:	return 5;
		case B200:	return 6;
		case B300:	return 7;
		case B600:	return 8;
		case B1200:	return 9;
		case B1800:	return 10;
		case B2400:	return 11;
		case B4800:	return 12;
		case B9600:	return 13;
		case B19200:	return 14;
		case B38400:	return 15;
	}

	return 0;
}

static void
setctty()
{
	if(STTY(&curttyb) < 0 || STTY2(&curttyb2) < 0)
		perror("NetHack (setctty)");
}

/*
 * Get initial state of terminal, set ospeed (for termcap routines)
 * and switch off tab expansion if necessary.
 * Called by startup() in termcap.c and after returning from ! or ^Z
 */
void
gettty()
{
	if(GTTY(&inittyb) < 0 || GTTY2(&inittyb2) < 0)
		perror("NetHack (gettty)");
	curttyb = inittyb;
	curttyb2 = inittyb2;
	ospeed = OSPEED(inittyb);
	erase_char = inittyb.erase_sym;
	kill_char = inittyb.kill_sym;
	intr_char = inittyb2.intr_sym;
	getioctls();

	/* do not expand tabs - they might be needed inside a cm sequence */
	if(curttyb.tabflgs & EXTABS) {
		curttyb.tabflgs &= ~EXTABS;
		setctty();
	}
	settty_needed = TRUE;
}

/* reset terminal to original state */
void
settty(s)
const char *s;
{
	end_screen();
	if(s) raw_print(s);
	if(STTY(&inittyb) < 0 || STTY2(&inittyb2) < 0)
		perror("NetHack (settty)");
	iflags.echo = (inittyb.echoflgs & ECHO) ? ON : OFF;
	iflags.cbreak = (CBRKON(inittyb.cbrkflgs & CBRKMASK)) ? ON : OFF;
	curttyb.inputflags |= STRIPHI;
	setioctls();
}

void
setftty()
{
	int ef = 0;			/* desired value of flags & ECHO */
	int cf = CBRKON(CBRKMASK);	/* desired value of flags & CBREAK */
	int change = 0;
	iflags.cbreak = ON;
	iflags.echo = OFF;
	/* Should use (ECHO|CRMOD) here instead of ECHO */
	if((curttyb.echoflgs & ECHO) != ef){
		curttyb.echoflgs &= ~ECHO;
/*		curttyb.echoflgs |= ef;					*/
		change++;
	}
	if((curttyb.cbrkflgs & CBRKMASK) != cf){
		curttyb.cbrkflgs &= ~CBRKMASK;
		curttyb.cbrkflgs |= cf;
		/* be satisfied with one character; no timeout */
		curttyb.c_cc[VMIN] = 1;		/* was VEOF */
		curttyb.c_cc[VTIME] = 0;	/* was VEOL */
#ifdef VDSUSP /* SunOS Posix extensions */
		curttyb.c_cc[VDSUSP] = nonesuch;
#endif
#ifdef VREPRINT
		curttyb.c_cc[VREPRINT] = nonesuch;
#endif
#ifdef VDISCARD
		curttyb.c_cc[VDISCARD] = nonesuch;
#endif
#ifdef VWERASE
		curttyb.c_cc[VWERASE] = nonesuch;
#endif
#ifdef VLNEXT
		curttyb.c_cc[VLNEXT] = nonesuch;
#endif
		change++;
	}
	if(!IS_7BIT(inittyb)) curttyb.inputflags &=~ STRIPHI;
	/* If an interrupt character is used, it will be overriden and
	 * set to ^C.
	 */
	if(intr_char != nonesuch && curttyb2.intr_sym != '\003') {
	    curttyb2.intr_sym = '\003';
	    change++;
	}

	if(change) setctty();
	start_screen();
}

void
intron()		/* enable kbd interupts if enabled when game started */
{
#ifdef TTY_GRAPHICS
	/* Ugly hack to keep from changing tty modes for non-tty games -dlc */
	if (!strcmp(windowprocs.name, "tty") &&
	    intr_char != nonesuch && curttyb2.intr_sym != '\003') {
	    curttyb2.intr_sym = '\003';
	    setctty();
	}
#endif
}

void
introff()		/* disable kbd interrupts if required*/
{
#ifdef TTY_GRAPHICS
	/* Ugly hack to keep from changing tty modes for non-tty games -dlc */
	if (!strcmp(windowprocs.name, "tty") &&
	   curttyb2.intr_sym != nonesuch) {
	    curttyb2.intr_sym = nonesuch;
	    setctty();
	}
#endif
}

#ifdef __linux__		/* via Jesse Thilo and Ben Gertzfield */
# include <sys/vt.h>

int linux_flag_console = 0;

void linux_mapon(void);
void linux_mapoff(void);
void check_linux_console(void);
void init_linux_cons(void);

void
linux_mapon()
{
# ifdef TTY_GRAPHICS
	int count = 0;
	if (!strcmp(windowprocs.name, "tty") && linux_flag_console) {
		count = write(1, "\033(B", 3);
	}
# endif
}

void
linux_mapoff()
{
# ifdef TTY_GRAPHICS
	int count = 0;
	if (!strcmp(windowprocs.name, "tty") && linux_flag_console) {
		count = write(1, "\033(U", 3);
	}
# endif
}

void
check_linux_console()
{
	struct vt_mode vtm;

	if (isatty(0) && ioctl(0,VT_GETMODE,&vtm) >= 0) {
		linux_flag_console = 1;
	}
}

void
init_linux_cons()
{
# ifdef TTY_GRAPHICS
	if (!strcmp(windowprocs.name, "tty") && linux_flag_console) {
		atexit(linux_mapon);
		linux_mapoff();
#  ifdef TEXTCOLOR
		if (has_colors())
			iflags.use_color = TRUE;
#  endif
	}
# endif
}
#endif	/* __linux__ */


#ifndef __begui__	/* the Be GUI will define its own error proc */
/* fatal error */
/*VARARGS1*/
void
error (const char *s, ...)
{
	va_list the_args;
	va_start(the_args, s);
	if(settty_needed)
		settty((char *)0);
	vprintf(s, the_args);
	(void) putchar('\n');
	va_end(the_args);
	exit(EXIT_FAILURE);
}
#endif /* !__begui__ */
