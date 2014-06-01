/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-05-30 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This gives a terminal backend for the uncursed rendering library, designed
   to work on the Windows console. */

/* Detect OS. */
#ifdef AIMAKE_BUILDOS_MSWin32
# undef WIN32
# define WIN32
#endif

#ifndef WIN32
# error !AIMAKE_FAIL_SILENTLY! wincon.c works on Windows systems only. \
    Use tty.c instead.
#endif

#include <windows.h>

/* wincon.c is always linked statically. */
#define UNCURSED_MAIN_PROGRAM

#include "uncursed.h"
#include "uncursed_hooks.h"
#include "uncursed_wincon.h"

static DWORD raw_mode = 0;
static HANDLE inhandle;
static HANDLE outhandle;

static DWORD orig_mode;
static CONSOLE_CURSOR_INFO orig_cursor;
static CONSOLE_SCREEN_BUFFER_INFO orig_csbi;

static int cur_h, set_h;
static int cur_w, set_w;
static int follow_window_size = 1;
static int save_cursor_x = 1, save_cursor_y = 1;
static unsigned char *drawn;

void
wincon_hook_beep(void)
{
    /* TODO */
}

void
wincon_hook_setcursorsize(int size)
{
    CONSOLE_CURSOR_INFO new_cursor;

    new_cursor.bVisible = size != 0;
    new_cursor.dwSize = (size == 2 ? 100 : 6);
    SetConsoleCursorInfo(outhandle, &new_cursor);
}

void
wincon_hook_positioncursor(int y, int x)
{
    save_cursor_x = x;
    save_cursor_y = y;
}

void
wincon_hook_init(int *h, int *w, const char *title)
{
    (void)title;
    COORD c;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    inhandle = GetStdHandle(STD_INPUT_HANDLE);
    outhandle = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(inhandle, &orig_mode);
    GetConsoleCursorInfo(outhandle, &orig_cursor);
    SetConsoleMode(inhandle,
                   (ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) |
                   (raw_mode ? 0 : ENABLE_PROCESSED_INPUT));
    GetConsoleScreenBufferInfo(outhandle, &orig_csbi);

    /* Determine the current size of the window. */
    *w = orig_csbi.srWindow.Right - orig_csbi.srWindow.Left + 1;
    *h = orig_csbi.srWindow.Bottom - orig_csbi.srWindow.Top + 1;
    cur_w = *w;
    cur_h = *h;

    if (drawn)
        free(drawn);
    drawn = calloc(cur_w, cur_h);

    /* It's the size of the screen buffer that determines how far the window
       can be resized. So we need to resize the screen buffer to the maximum
       possible window size. TODO: Using a secondary screen buffer would allow
       us to restore the prevous console contents afterwards. Not that anybody
       cares on Windows. */
    c = GetLargestConsoleWindowSize(outhandle);
    SetConsoleScreenBufferSize(outhandle, c);
    GetConsoleScreenBufferInfo(outhandle, &csbi);

    /* Record the buffer size we changed to, so that it doesn't look like an
       attempt by the user to set the size. */
    set_w = csbi.dwSize.X;
    set_h = csbi.dwSize.Y;
}

void
wincon_hook_exit(void)
{
    /* Restore all the settings we can. */
    SetConsoleMode(inhandle, orig_mode);
    SetConsoleCursorInfo(outhandle, &orig_cursor);
    SetConsoleScreenBufferSize(outhandle, orig_csbi.dwSize);
    SetConsoleCursorPosition(outhandle, orig_csbi.dwCursorPosition);
}

void
wincon_hook_rawsignals(int raw)
{
    SetConsoleMode(inhandle,
                   (ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) |
                   (raw_mode ? 0 : ENABLE_PROCESSED_INPUT));
}

void
wincon_hook_activatemouse(int active)
{
    /* We don't have mouse support (yet?) */
    (void) active;
}

void
wincon_hook_signal_getch(void)
{
    /* TODO: unimplemented */
}

void
wincon_hook_watch_fd(int fd, int watch)
{
    /* TODO: unimplemented */
    (void) fd;
    (void) watch;
}

static void
wincon_doredraw(void)
{
    COORD c;

    c.X = save_cursor_x;
    c.Y = save_cursor_y;
    SetConsoleCursorPosition(outhandle, c);

    int x, y;

    for (y = 0; y < cur_h; y++)
        for (x = 0; x < cur_w; x++) {

            if (drawn[x + y * cur_w])
                continue;
            drawn[x + y * cur_w] = 1;

            WCHAR ch = uncursed_rhook_ucs2_at(y, x);
            int a = uncursed_rhook_color_at(y, x);
            WORD attr = 0;
            COORD c;
            DWORD unused = 0;

            if (a & 1)
                attr |= FOREGROUND_RED;
            if (a & 2)
                attr |= FOREGROUND_GREEN;
            if (a & 4)
                attr |= FOREGROUND_BLUE;
            if (a & 8)
                attr |= FOREGROUND_INTENSITY;
            if (a & 16)
                attr |= FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN;
            if (a & 32)
                attr |= BACKGROUND_RED;
            if (a & 64)
                attr |= BACKGROUND_GREEN;
            if (a & 128)
                attr |= BACKGROUND_BLUE;
            if (a & 256)
                attr |= BACKGROUND_INTENSITY;

            /* Ignore 512s and 1024s bits; 512s bit = default background =
               black, 1024s bit = underline, which the console can't render */
            c.Y = y;
            c.X = x;

            FillConsoleOutputCharacterW(outhandle, ch, 1, c, &unused);
            FillConsoleOutputAttribute(outhandle, attr, 1, c, &unused);
        }
}

void
wincon_hook_delay(int ms)
{
    wincon_doredraw();
    /* TODO */
}

static int lastkeyorcodepoint = 0;
static int remainingrepeats = 0;
int
wincon_hook_getkeyorcodepoint(int timeout_ms)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    PKEY_EVENT_RECORD kp;
    INPUT_RECORD inrecords[1];
    DWORD count = 0;

    wincon_doredraw();

    /* Windows uses RLE encoding for its keypresses. Reverse that encoding
       here. */
    if (remainingrepeats) {
        remainingrepeats--;
        return lastkeyorcodepoint;
    }

    /* Has the /window/ been resized? If so, and the user didn't explicitly set
       the buffer size, adapt to the new window size. */
    if (follow_window_size == 1) {

        GetConsoleScreenBufferInfo(outhandle, &csbi);

        /* Determine the current size of the window. */
        int w, h;
        w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        if (w != cur_w || h != cur_h) {
            cur_w = w;
            cur_h = h;
            if (drawn)
                free(drawn);
            drawn = calloc(cur_w, cur_h);
            uncursed_rhook_setsize(cur_h, cur_w);
            return KEY_RESIZE + KEY_BIAS;
        }
    }

    /* TODO: Timeouts aren't implemented yet. This way of doing things at least
       has a chance of working. */
    if (timeout_ms < 500 && timeout_ms >= 0)
        return KEY_SILENCE + KEY_BIAS;

recheck:
    count = 0;
    while (!count) {
        ReadConsoleInputW(inhandle, inrecords, 1, &count);

        if (inrecords[0].EventType != WINDOW_BUFFER_SIZE_EVENT &&
            inrecords[0].EventType != KEY_EVENT)
            count = 0;
        if (inrecords[0].EventType == KEY_EVENT &&
            !inrecords[0].Event.KeyEvent.bKeyDown)
            count = 0;
    }
    if (inrecords[0].EventType == WINDOW_BUFFER_SIZE_EVENT) {
        /* Someone resized the buffer. Possibly. */
        /* If the user's explicitly resizing the buffer, ignore the window size
           from now on and just use the buffer they requested. */
        cur_w = inrecords[0].Event.WindowBufferSizeEvent.dwSize.X;
        cur_h = inrecords[0].Event.WindowBufferSizeEvent.dwSize.Y;

        if (drawn)
            free(drawn);
        drawn = calloc(cur_w, cur_h);

        /* Is this us setting it? Or someone else? */
        if (cur_w != set_w || cur_h != set_h) {
            follow_window_size = 0;
            uncursed_rhook_setsize(cur_h, cur_w);
            return KEY_RESIZE + KEY_BIAS;
        }

        goto recheck;
    }

    /* Someone pressed a key. */
    kp = &(inrecords[0].Event.KeyEvent);
    remainingrepeats = kp->wRepeatCount - 1;
    lastkeyorcodepoint = (int)kp->uChar.UnicodeChar;

    /* Most special keys give us a codepoint of 0. One special case: backspace
       gives us a codepoint of control-H. Also noteworthy is that Alt does not
       change the codepoint (Control and Shift do.) */
    switch (kp->wVirtualKeyCode) {
#define VKK(x,y) case VK_##x: lastkeyorcodepoint = KEY_BIAS + KEY_##y; break
        VKK(BACK, BACKSPACE); VKK(PAUSE, BREAK); VKK(PRIOR, PPAGE);
        VKK(NEXT, NPAGE); VKK(END, END); VKK(HOME, HOME);
        VKK(LEFT, LEFT); VKK(UP, UP); VKK(RIGHT, RIGHT); VKK(DOWN, DOWN);
        VKK(PRINT, PRINT); VKK(INSERT, IC); VKK(DELETE, DC);
        VKK(NUMPAD0, D1); VKK(DECIMAL, D3); VKK(CLEAR, B2);
        VKK(NUMPAD1, C1); VKK(NUMPAD2, C2); VKK(NUMPAD3, C3);
        VKK(NUMPAD4, B1); VKK(NUMPAD5, B2); VKK(NUMPAD6, B3);
        VKK(NUMPAD7, A1); VKK(NUMPAD8, A2); VKK(NUMPAD9, A3);
        VKK(ADD, NUMPLUS); VKK(SUBTRACT, NUMMINUS);
        VKK(NUMLOCK, PF1); VKK(MULTIPLY, NUMTIMES); VKK(DIVIDE, NUMDIVIDE);
        VKK(F1, F1); VKK(F2, F2); VKK(F3, F3); VKK(F4, F4); VKK(F5, F5);
        VKK(F6, F6); VKK(F7, F7); VKK(F8, F8); VKK(F9, F9); VKK(F10, F10);
        VKK(F11, F11); VKK(F12, F12); VKK(F13, F13); VKK(F14, F14);
        VKK(F15, F15); VKK(F16, F16); VKK(F17, F17); VKK(F18, F18);
        VKK(F19, F19); VKK(F20, F20);
#undef VKK

    case VK_SHIFT:
    case VK_CONTROL:
    case VK_RWIN:
    case VK_LWIN:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:      /* sent by Alt */
        goto recheck;

    default:
        if (lastkeyorcodepoint == 0) {
            if (kp->wVirtualKeyCode < 256)
                /* synthesize a key code for it */
                lastkeyorcodepoint =
                    KEY_BIAS + (KEY_FUNCTION | kp->wVirtualKeyCode);
            else
                lastkeyorcodepoint = KEY_BIAS + KEY_INVALID;
        }
        /* otherwise it's just an ordinary key */
    }

    if (lastkeyorcodepoint >= 32 && lastkeyorcodepoint <= 126) {
        if (kp->dwControlKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
            lastkeyorcodepoint = (KEY_ALT | lastkeyorcodepoint) + KEY_BIAS;
    } else if (lastkeyorcodepoint == 27) {
        lastkeyorcodepoint = KEY_ESCAPE + KEY_BIAS;
    } else if (lastkeyorcodepoint >= KEY_BIAS) {
        lastkeyorcodepoint -= KEY_BIAS;
        if (kp->dwControlKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
            lastkeyorcodepoint |= KEY_ALT;
        if (kp->dwControlKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
            lastkeyorcodepoint |= KEY_CTRL;
        if (kp->dwControlKeyState & SHIFT_PRESSED)
            lastkeyorcodepoint |= KEY_SHIFT;
        lastkeyorcodepoint += KEY_BIAS;
    }

    return lastkeyorcodepoint;
}

void
wincon_hook_update(int y, int x)
{
    /* We don't update immediately, but at the next attempt to delay or get a
       keypress; this makes the (very slow) redraws less ugly. */
    drawn[x + y * cur_w] = 0;
    uncursed_rhook_updated(y, x);
}

void
wincon_hook_fullredraw(void)
{
    int i, j;

    for (j = 0; j < cur_h; j++)
        for (i = 0; i < cur_w; i++)
            wincon_hook_update(j, i);
}

void
wincon_hook_flush(void)
{
    /* nothing to do */
}
