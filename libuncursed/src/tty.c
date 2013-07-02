/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack general public license
 *  - the GNU General Public license v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This gives a terminal backend for the uncursed rendering library, designed to
   work well on virtual terminals, but sacrificing support for physical
   terminals (at least, those that require padding). The minimum feature set
   required by the terminal is 8 colors + bold for another 8, basic cursor
   movement commands, and understanding either code page 437 or Unicode
   (preferably both). This file is tested against the following terminal
   emulators, and intended to work correctly on all of them:

   Terminals built into operating systems:
   Linux console (fbcon)
   DOSBOx emulation of the DOS console
   Windows console (Vista and higher)

   Terminal emulation software:
   xterm
   gnome-terminal
   konsole
   PuTTY
   urxvt

   Terminal multiplexers:
   screen
   tmux

   Terminal recording renderers:
   termplay
   jettyplay
   (Note that some ttyrec players, such as ttyplay, use the terminal for
   rendering rather than doing their own rendering; those aren't included on
   this list. ipbt is not included because it does not support character sets
   beyond ASCII; it also has a major issue where it understands the codes for
   colors above 8, but is subsequently unable to render them.)

   Probably the most commonly used terminal emulator not on this list is
   original rxvt, which has very poor support for all sorts of features we would
   want to use (although the main reason it isn't supported is that it relies on
   the font used for encoding support rather than trying to do any encoding
   handling itself, meaning that text that's meant to be CP437 tends to get
   printed as Latin-1 in practice). It's entirely possible that you could manage
   to configure rxvt to work with uncursed, but save yourself some trouble and
   use urxvt instead :)

   This file is written to be platform-agnostic, but contains platform-specific
   code on occasion (e.g. signals, delays). Where there's a choice of platform-
   specific function, the most portable is used (e.g. select() rather than
   usleep() for delays, because it's in older versions of POSIX).
*/

/* UNIX-specific headers */
#define _POSIX_SOURCE 1
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

/* Linux, and maybe other UNIX, -specific headers */
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "uncursed_hooks.h"
#include "uncursed.h"

/* Note: ifile only uses platform-specific read functions like read();
   ofile only uses stdio for output (e.g. fputs). Try not to muddle these! */
#define ofile stdout
#define ifile stdin

#define CSI "\x1b["

static int is_inited = 0;

/* Portable helper functions */
/* Returns the first codepoint in the given UTF-8 string, or -1 if it's
   incomplete, or -2 if it's invalid. */
static long parse_utf8(char *s) {
    /* This signedness trick is normally illegal, but fortunately, it's legal
       for chars in particular */
    unsigned char *t = (unsigned char *)s;
    if (*t < 0x80) return *s;
    if (*t < 0xc0) return -2;
    int charcount; 
    wchar_t r;
    if      (*t < 0xe0) {charcount = 2; r = *t & 0x1f;}
    else if (*t < 0xf0) {charcount = 3; r = *t & 0x0f;}
    else if (*t < 0xf8) {charcount = 4; r = *t & 0x07;}
    else return -2;
    while (--charcount) {
        t++;
        if (!*t) return -1; /* incomplete */
        if (*t < 0x80 || *t >= 0xc0) return -2;
        r *= 0x40;
        r += *t & 0x3f;
    }
    if (r >= 0x110000) return -2;
    return r;
}

/* Linux-specific functions */
static void measure_terminal_size(int *h, int *w) {
    /* We need to determine the size of the terminal, which is nontrivial to do
       in a vaguely portable manner. On Linux, and maybe other UNIXes, we can do
       it like this: */
    struct winsize ws;
    ioctl(fileno(ofile), TIOCGWINSZ, &ws);
    *h = ws.ws_row;
    *w = ws.ws_col;
}

/* UNIX-specific functions */
static int selfpipe[2] = {-1, -1};
static struct termios ti, ti_orig, to, to_orig;
static int sighup_mode = 0; /* return KEY_HANGUP as every input if set */

static int raw_isig = 0;
static void platform_specific_rawsignals(int raw) {
    if (raw) ti.c_lflag &= ~ISIG;
    else ti.c_lflag |= ISIG;
    tcsetattr(fileno(ifile), TCSANOW, &ti);
    raw_isig = raw;
}

/* TODO: make selfpipe non-blocking for writes, so that being spammed signals
   doesn't lock up the program; this is particularly important with SIGPIPE */
static void handle_sigwinch(int unused) {
    (void) unused;
    write(selfpipe[1], "r", 1);
}
static void handle_sighup(int unused) {
    (void) unused;
    write(selfpipe[1], "h", 1);
}
static void handle_sigtstp(int unused) {
    (void) unused;
    write(selfpipe[1], "s", 1);
}
static void handle_sigcont(int unused) {
    (void) unused;
    write(selfpipe[1], "c", 1);
}

static void platform_specific_init(void) {
    /* Set up the self-pipe. (Do this first, so we can exit early if it
       fails. */
    if (selfpipe[0] == -1) {
        if (pipe(selfpipe) != 0) {
            perror("Could not initialise uncursed tty library");
            exit(1);
        }
    }

    /* Set up the terminal control flags. */
    tcgetattr(fileno(ifile), &ti_orig);
    tcgetattr(fileno(ofile), &to_orig);
    /* Terminal control flags to turn off for the input file:
       ISTRIP    bit 8 stripping (incompatible with Unicode)
       INPCK     parity checking (ditto)
       PARENB    more parity checking (ditto)
       IGNCR     ignore CR (means there's a key we can't read)
       INLCR     translate NL to CR (ditto)
       ICRNL     translate CR to NL (ditto)
       IXON      insert XON/XOFF into the stream (confuses the client)
       IXOFF     respect XON/XOFF (causes the process to lock up on ^S)
       ICANON    read a line at a time (means we can't read keys)
       ECHO      echo typed characters (confuses our view of the screen)
       We also set ISIG based on raw_isig, and VMIN to 1, VTIME to 0,
       which gives select() the correct semantics; and we turn on CS8,
       to be able to read Unicode (although on Unicode systems it seems
       to be impossible to turn it off anyway).
    */
    /* Terminal control flags to turn off for the output file:
       OPOST     general output postprocessing (portability nightmare)
       OCRNL     map CR to NL (we need to stay on the same line with CR)
       ONLRET    delete CR (we want to be able to use CR)
       OFILL     delay using padding (only works on physical terminals)
       We modify the flags for to only after modifying the flags for ti,
       so that one set of changes doesn't overwrite the other.
    */
    tcgetattr(fileno(ifile), &ti);
    ti.c_iflag &= ~(ISTRIP|INPCK|IGNCR|INLCR|ICRNL|IXON|IXOFF);
    ti.c_cflag &= ~PARENB;
    ti.c_cflag |= CS8;
    ti.c_lflag &= ~(ISIG|ICANON|ECHO);
    ti.c_cc[VMIN] = 1; ti.c_cc[VTIME] = 0;
    if (raw_isig) ti.c_lflag |= ISIG;
    tcsetattr(fileno(ifile), TCSANOW, &ti);
    tcgetattr(fileno(ofile), &to);
    ti.c_oflag &= ~(OPOST|OCRNL|ONLRET|OFILL);
    tcsetattr(fileno(ofile), TCSADRAIN, &to);

    /* Set up signal handlers. (Luckily, sigaction is just as portable as
       tcsetattr, so we don't have to deal with the inconsistency of signal.) */
    struct sigaction sa;
    sa.sa_flags = 0; /* SA_RESTART would be nice, but too recent */
    sigemptyset(&(sa.sa_mask));
    sa.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &sa, 0);
    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    sigaction(SIGPIPE, &sa, 0);
    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, 0);
    sa.sa_handler = handle_sigcont;
    sigaction(SIGCONT, &sa, 0);
}

static void platform_specific_exit(void) {
    /* Put the terminal back to the way the user had it. */
    tcsetattr(fileno(ifile), TCSANOW, &ti_orig);
    tcsetattr(fileno(ofile), TCSADRAIN, &to_orig);
}

static char *KEYSTRING_HANGUP, *KEYSTRING_RESIZE, *KEYSTRING_INVALID;
static int inited_when_stopped = 0;

static char* platform_specific_getkeystring(int timeout_ms) {
    /* We want to wait for a character from the terminal (i.e. a keypress or
       mouse click), or for a signal (we handle SIGWINCH, SIGTSTP/SIGCONT, and
       SIGHUP/SIGTERM/SIGPIPE; SIGINT and friends are left unhandled because
       that's what rawsignals is for, SIGTTOU and friends are left unhandled
       because the default behaviour is what we want anyway, and SIGSEGV and
       friends can't meaningfully be handled). Note that we read bytes here;
       Unicode conversion is done later.

       There are two difficulties: reading the multiple input sources at once,
       and stopping at the right moment. We wait timeout_ms for the first byte
       of the key (which could be 0, or could be negative = infinite); then we
       wait 200ms for the subsequent characters /if/ we have reason to believe
       that there might be subsequent characters. (Exception: ESC ESC is parsed
       as ESC, so that the user can send ESC quickly.) */
    struct timeval t;
    t.tv_sec = timeout_ms / 1000;
    t.tv_usec = (timeout_ms % 1000) * 1000;
    static char keystring[80];
    char *r = keystring;
    while(1) {
        if (sighup_mode) return KEYSTRING_HANGUP;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fileno(ifile), &readfds);
        int max = fileno(ifile);
        if (r == keystring) {
            FD_SET(selfpipe[0], &readfds);
            if (max < selfpipe[0]) max = selfpipe[0];
        } else {
            t.tv_sec = 0;
            t.tv_usec = 200000;
        }
        errno = 0;
        int s = select(max+1, &readfds, 0, 0,
                       (timeout_ms >= 0 || r != keystring) ? &t : 0);
        if (!s) break; /* timeout */
        if (s < 0 && errno == EINTR) continue;
        if (s < 0) sighup_mode = 1;
        /* If this is not the first character, we don't read from the
           signal pipe anyway, so no need to worry about handling a
           partial character read on a signal. If it is the first
           character, signals take precedence, and we just leave any
           character in the input queue. */
        if (FD_ISSET(selfpipe[0], &readfds)) {
            char signalcode[1];
            read(selfpipe[0], signalcode, 1);
            switch (*signalcode) {
            case 'r': /* SIGWINCH */
                return KEYSTRING_RESIZE;
            case 'h': /* SIGHUP, SIGTERM, SIGPIPE */
                sighup_mode = 1;
                return KEYSTRING_HANGUP;
            case 's': /* SIGTSTP */
                if (is_inited) {
                    uncursed_hook_exit();
                    inited_when_stopped = 1;
                }
                raise(SIGSTOP);
                break;
            case 'c': /* SIGCONT */
                if (is_inited) {
                    /* We were stopped unexpectedly; completely reset the
                       terminal */
                    uncursed_hook_exit();
                    inited_when_stopped = 1;
                }
                if (inited_when_stopped) {
                    int h, w;
                    uncursed_hook_init(&h, &w);
                    inited_when_stopped = 0;
                    uncursed_hook_fullredraw();
                }
                break;
            }
        } else {
            /* The user pressed a key. We need to return immediately if:
               - It wasn't ESC, and was the last character of a UTF-8
                 encoding (including ASCII), or
               - It's the second character, the first was ESC, and it
                 isn't [ or O;
               - It's the third or subsequent character, the first was
                 ESC, and it isn't [, ;, or a digit;
               - We're running out of room in the buffer (malicious
                 input). */
            if (read(fileno(ifile), r++, 1) == 0) {
                /* EOF on the input is unrecoverable, so the best we can do is
                   to treat it as a hangup. */
                sighup_mode = 1;
                return KEYSTRING_HANGUP;
            }
            if (r - keystring > 75) {
                /* Someone's trying to overflow the buffer... */
                return KEYSTRING_INVALID;
            }
            if (*keystring == 27) { /* Escape */
                if (r - keystring == 2 && r[-1] != '[' && r[-1] != 'O') break;
                if (r - keystring > 2 && r[-1] != '[' && r[-1] != ';' &&
                    (r[-1] < '0' || r[-1] > '9')) break;
            } else { /* not Escape */
                *r = 0;
                long c = parse_utf8(keystring);
                if (c >= 0) break;
                if (c == -2) return KEYSTRING_INVALID; /* invalid input */
            }
        }
    }
    if (r == keystring) return 0;
    *r = '\0';
    return keystring;
}

static void platform_specific_delay(int ms) {
    struct timeval t;
    t.tv_sec = ms / 1000;
    t.tv_usec = (ms % 1000) * 1000;
    select(0, 0, 0, 0, &t);
}

/* (mostly) portable functions */
static int terminal_contents_unknown = 1;
static int last_color = -1;
static int supports_utf8 = 0;
static int last_y = -1, last_x = -1;

static int last_h = -1, last_w = -1;
static void record_and_measure_terminal_size(int *h, int *w) {
    measure_terminal_size(h, w);
    if (*h != last_h || *w != last_w) {
        /* Send a terminal resize code, for watchers/recorders. */
        fprintf(ofile, CSI "8;%d;%dt", *h, *w);
    }
    last_h = *h;
    last_w = *w;
}

/* (y, x) are the coordinates of a character that can safely be clobbered,
   or (-1, -1) if it doesn't matter if we print garbage; the cursor is left
   in an unknown location */
static void set_charset(int y, int x) {
    if (x > -1) fprintf(ofile, CSI "%d;%dH", y+1, x+1);
    fputs("\x0f", ofile); /* select character set G0 */
    if (x > -1) fprintf(ofile, CSI "%d;%dH", y+1, x+1);
    if (supports_utf8) {
        fputs("\x1b(B", ofile); /* select default character set for G0 */
        if (x > -1) fprintf(ofile, CSI "%d;%dH", y+1, x+1);
        fputs("\x1b%G", ofile); /* set character set as UTF-8 */
    } else {
        fputs("\x1b%@", ofile); /* disable Unicode, set default character set */
        if (x > -1) fprintf(ofile, CSI "%d;%dH", y+1, x+1);
        fputs("\x1b(U", ofile); /* select null mapping, = cp437 on a PC */
    }
    last_y = last_x = -1;
}

void uncursed_hook_beep(void) {
    fputs("\x07", ofile);
    fflush(ofile);
}

void uncursed_hook_setcursorsize(int size) {
    /* TODO: Shield fom DOS */
    if (size == 0) fputs(CSI "?25l", ofile);
    else fputs(CSI "?25h", ofile);
    /* Don't change the cursor size; we'd have no way to undo the change, and
       it leads to garbage on many terminals. */
    fflush(ofile);
}
void uncursed_hook_positioncursor(int y, int x) {
    fprintf(ofile, CSI "%d;%dH", y+1, x+1);
    fflush(ofile);
    last_y = y; last_x = x;
}

void uncursed_hook_init(int *h, int *w) {
    platform_specific_init();
    /* Save the character sets. */
    fputs("\x1b""7", ofile);
    /* Switch to the alternate screen, if possible, so that we don't overwrite
       the user's scrollback. */
    fputs(CSI "?1049h", ofile);
    /* Hide any scrollbar, because it's useless. */
    fputs(CSI "?30l", ofile);
    /* Work out the terminal size; also set it to itself (so that people
       watching, and recordings, have the terminal size known). */
    record_and_measure_terminal_size(h, w);
    /* Take control over the numeric keypad, if possible. Even when it isn't
       (e.g. xterm), this lets us distinguish Enter from Return. */
    fputs("\x1b=", ofile);
    /* TODO: turn on mouse tracking? */
    /* Work out the Unicodiness of the terminal. We do this by sending 4 bytes
       = 2 Unicode codepoints, and seeing how far the cursor moves.*/
    fputs("\r\n\xc2\xa0\xc2\xa0\x1b[6n\nConfiguring terminal, please wait.\n",
          ofile);
    /* Very primitive terminals might not send any reply. So we wait 2 seconds
       for a reply before moving on. The reply is written as a PF3 with
       modifiers; the row is eaten, and the column is returned in the modifiers
       (column 3 = unicode = KEY_PF3 | (KEY_SHIFT*2), column 5 = not unicode =
       KEY_PF3 | (KEY_SHIFT*4)). VT100 is weird! */
    supports_utf8 = 0;
    wint_t kp = 0;
    while (kp != (KEY_PF3 | (KEY_SHIFT*2)) &&
           kp != (KEY_PF3 | (KEY_SHIFT*4)) && kp != KEY_SILENCE) {
        kp = uncursed_hook_getkeyorcodepoint(2000);
        if (kp < 0x110000) kp = 0;
        else kp -= KEY_BIAS;
    }
    if (kp == (KEY_PF3 | (KEY_SHIFT * 2))) supports_utf8 = 1;

    /* Tell the terminal what Unicodiness we detected from it. There are two
       reasons to do this: a) if we're somehow wrong, this might make our
       output work anyway; b) other people watching/replaying will have the
       same Unicode settings, if the terminal supports them. */
    set_charset(-1, -1);

    /* Note: we intentionally don't support DECgraphics, because there's no
       way to tell whether it's working or not and it has a rather limited
       character set. However, the portable way to do it is to switch to G1
       for DECgraphics, defining both G0 and G1 as DECgraphics; then switch
       back to G0 for normal text, defining just G0 as normal text. */

    /* Clear the terminal. */
    fputs(CSI "2J", ofile);
    terminal_contents_unknown = 1;
    last_color = -1;
    last_y = last_x = -1;
    is_inited = 1;
}

void uncursed_hook_exit(void) {
    /* It'd be great to reset the terminal right now, but that wipes out
       scrollback. We can try to undo as many of our settings as we can, but
       that's not very many, due to problems with the protocol. */
    fputs("\x1b>", ofile); /* turn off keypad special-casing */
    fputs(CSI "?25h", ofile); /* turn the cursor back on */
    fputs(CSI "2J", ofile); /* and we can at least wipe the screen */
    /* Switch back to the main screen. */
    fputs(CSI "?1049l", ofile);
    /* Restore G0/G1. */
    fputs("\x1b""8", ofile);
    platform_specific_exit();
    is_inited = 0;
}

void uncursed_hook_delay(int ms) {
    last_color = -1; /* send a new SGR on the next redraw */
    platform_specific_delay(ms);
}
void uncursed_hook_rawsignals(int raw) { platform_specific_rawsignals(raw); }
wint_t uncursed_hook_getkeyorcodepoint(int timeout_ms) {
    last_color = -1; /* send a new SGR on the next redraw */
    char *ks = platform_specific_getkeystring(timeout_ms);
    if (ks == KEYSTRING_HANGUP) return KEY_HANGUP + KEY_BIAS;
    if (ks == KEYSTRING_RESIZE) return KEY_RESIZE + KEY_BIAS;
    if (ks == KEYSTRING_INVALID) return KEY_INVALID + KEY_BIAS;
    if (!ks) return KEY_SILENCE + KEY_BIAS;
    if (!*ks) return KEY_SILENCE + KEY_BIAS; /* should never happen */
    if (*ks != '\x1b') {
        if (*ks == 127) return KEY_BACKSPACE + KEY_BIAS; /* a special case */
        /* Interpret the key as UTF-8, if we can. Otherwise guess Latin-1;
           nobody uses CP437 for /input/. */
        long c = parse_utf8(ks);
        if (c == -2 && ks[1] == 0) return *ks;
        if (c < 0) return KEY_INVALID + KEY_BIAS;
        return c;
    } else if (ks[1] == 0) {
        /* ESC, by itself. */
        return KEY_ESCAPE + KEY_BIAS;
    } else if ((ks[1] != '[' && ks[1] != 'O') || !ks[2]) {
        /* An Alt-modified key. The curses API doesn't understand alt plus
           arbitrary unicode, so for now we just send the key without alt
           if it's outside the ASCII range. */
        if (!ks[2] && ks[1] < 127) return (KEY_ALT | ks[1]) + KEY_BIAS;
        long c = parse_utf8(ks+1);
        if (c < 0) return KEY_INVALID + KEY_BIAS;
        return c;
    } else {
        /* The string consists of ESC, [ or O, then 0 to 2 semicolon-separated
           decimal numbers, then one character that's neither a digit nor a
           semicolon. getkeystring doesn't guarantee it's not malformed, so
           care is needed. */
        int args[2] = {0, 0};
        int argp = 0;
        char *r = ks+2;
        /* Linux console function keys use a different rule, */
        if (*r == '[' && r[1] && !r[2]) {
            return KEY_BIAS + (KEY_NONDEC | r[1]);
        }
        while (*r && ((*r >= '0' && *r <= '9') || *r == ';')) {
            if (*r == ';') argp++;
            else if (argp < 2) {args[argp] *= 10; args[argp] += *r - '0';}
            r++;
        }
        if (args[0] > 256 || args[1] > 8) return KEY_INVALID + KEY_BIAS;
        if (args[1] == 0) args[1] = 1;
        /* A special case where ESC O and ESC [ are different */
        if (*r == 'P' && ks[1] == '[') return KEY_BREAK + KEY_BIAS;
        if (*r == '~') {
            /* A function key. */
            return KEY_BIAS + (KEY_FUNCTION | args[0] | KEY_SHIFT * (args[1]-1));
        } else {
            /* A keypad key. */
            return KEY_BIAS + (KEY_KEYPAD | *r | KEY_SHIFT * (args[1]-1));
        }
    }
    /* Unreachable, but gcc seems not to realise that */
    return KEY_BIAS + KEY_INVALID;
}

void uncursed_hook_update(int y, int x) {
    /* If we need to do a full redraw, do so. */
    if (terminal_contents_unknown) {
        terminal_contents_unknown = 0;
        last_color = -1;
        int j, i;
        for (j = 0; j < last_h; j++)
            for (i = 0; i < last_w; i++)
                uncursed_hook_update(j, i);
        return;
    }
    if (last_color == -1) {
        /* In addition to resending color, resend the other font information
           too. */
        set_charset(y, x);
    }
    if (!uncursed_rhook_needsupdate(y, x)) return;
    if (last_y != y || last_x == -1) {
        fprintf(ofile, CSI "%d;%dH", y+1, x+1);
    } else if (last_x > x) {
        fprintf(ofile, CSI "%dD", last_x - x);
    } else if (last_x < x) {
        fprintf(ofile, CSI "%dC", x - last_x);
    }
    int color = uncursed_rhook_color_at(y, x);
    if (color != last_color) {
        last_color = color;
        /* The general idea here is to specify bold for bright foreground, but
           blink for bright background only on terminals without 256-color
           support (via exploiting the "5" in the code for setting 256-color
           background). We set the colors using the 8-color code first, then
           the 16-color code, to get support for 16 colors without losing
           support for 8 colors. */
        fputs(CSI "0;", ofile);
        if ((color & 31) == 16) /* default fg */
            fputs("39;", ofile);
        else if ((color & 31) >= 8) /* bright fg */
            fprintf(ofile, "1;%d;%d;", (color & 31) + 22, (color & 31) + 82);
        else /* dark fg */
            fprintf(ofile, "%d;", (color & 31) + 30);
        color >>= 5;
        if (color & 32) fputs("4;", ofile);
        color &= 31;
        if (color == 16) /* default bg */
            fputs("49m", ofile);
        else if (color >= 8) /* bright bg */
            fprintf(ofile, "48;5;5;%d;%dm", color + 32, color + 92);
        else
            fprintf(ofile, "%dm", color + 40);
    }
    if (supports_utf8) 
        fputs(uncursed_rhook_utf8_at(y, x), ofile);
    else
        fputc(uncursed_rhook_cp437_at(y, x), ofile);
    uncursed_rhook_updated(y, x);
    last_x = x+1;
    if (last_x > COLS) last_x = -1;
    last_y = y;
}

void uncursed_hook_fullredraw(void) {
    terminal_contents_unknown = 1;
    fputs(CSI "2J", ofile);
    uncursed_hook_update(0,0);
}

void uncursed_hook_flush(void) {
    fflush(stdout);
}
