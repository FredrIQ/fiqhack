/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-30 */
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This is a graphical backend for the uncursed rendering library, that uses SDL
   to do its rendering. */

#define PNG_SETJMP_SUPPORTED
#include <png.h>

#define SDL_MAIN_HANDLED /* don't use SDL's main, use the calling process's */
#include <SDL2/SDL.h>

#include <stdint.h>
#include <stdio.h>

#include "uncursed.h"
#include "uncursed_hooks.h"
#include "uncursed_sdl.h"

static int fontwidth = 8;
static int fontheight = 14;
static int winwidth = 132; /* width of the window, in units of fontwidth */
static int winheight = 36; /* height of the window, in units of fontheight */
static int resize_queued = 0;
static int suppress_resize = 0;
static int ignore_resize_count = 0;

static int hangup_mode = 0;

/* force the minimum size as 80x24; many programs don't function properly with
   less than that */
#define MINCHARWIDTH 80
#define MINCHARHEIGHT 24

static SDL_Window *win = NULL;
static SDL_Renderer *render = NULL;
static SDL_Texture *font = NULL;

static SDL_Texture* load_png_file_to_texture(char *filename, int *w, int *h) {
    /* Based on a public domain example that ships with libpng */
    volatile png_structp png_ptr;
    png_structp png_ptr_nv;
    volatile png_infop info_ptr;
    png_infop info_ptr_nv;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, has_alpha, i;
    FILE *in = fopen(filename, "rb");
    SDL_Surface *surface = NULL;
    SDL_Texture *rv = NULL;
    /* The pointers are volatile. What they point to isn't. */
    unsigned char * volatile pixeldata = NULL;
    unsigned char ** volatile rowpointers = NULL;
    if (!in) {
        perror(filename);
        goto cleanup_nothing;
    }
    
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Error allocating png_ptr for font file\n");
        goto cleanup_fopen;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error allocating info_ptr for font file\n");
        goto cleanup_png_ptr;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error reading font file: bad PNG format\n");
        goto cleanup_info_and_rowpointers;
    }

    png_init_io(png_ptr, in);

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, int_p_NULL, int_p_NULL);

    /* Change the data to a standard format (32bpp RGBA). */

    /* Force pixels into individual bits */
    if (bit_depth < 8) png_set_packing(png_ptr);

    /* Force to 8bpp */
    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    /* Convert color key to alpha */
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
        has_alpha = 1;
    } else has_alpha = (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
                        color_type == PNG_COLOR_TYPE_RGB_ALPHA);

    /* Force to RGB */
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
        color_type == PNG_COLOR_TYPE_GRAY) png_set_gray_to_rgb(png_ptr);

    /* TODO: Do we want gamma processing? */

    /* Add an alpha channel, if the image still hasn't got one */
    if (!has_alpha)
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    /* Allocate memory. */
    png_read_update_info(png_ptr, info_ptr);
    if (png_get_rowbytes(png_ptr, info_ptr) != width * 4) {
        fprintf(stderr, "Error in PNG memory allocation: expected %ld bytes, "
                "found %ld bytes.\n", (long)width * 4, 
                (long)png_get_rowbytes(png_ptr, info_ptr));
    }

    pixeldata = malloc(height * width * 4);
    rowpointers = malloc(sizeof *rowpointers * height);
    if (!pixeldata || !rowpointers) {
        fprintf(stderr, "Error allocating image memory for font file\n");
        goto cleanup_info_and_rowpointers;
    }

    for (i = 0; i < height; i++)
        rowpointers[i] = pixeldata + (i * width * 4);
    png_read_image(png_ptr, rowpointers);

    /* Create an SDL surface from the pixel data */
    surface = SDL_CreateRGBSurfaceFrom(
        pixeldata, width, height, 32 /*bpp*/, width * 4 /*rowbytes*/,
        0xFF000000U, 0x00FF0000U, 0x0000FF00U, 0x000000FFU /*masks*/);
    if (!surface) {
        fprintf(stderr, "Error creating SDL font surface: %s\n",
                SDL_GetError());
        goto cleanup_info_and_rowpointers;
    }

    rv = SDL_CreateTextureFromSurface(render, surface);
    if (!rv) {
        fprintf(stderr, "Error creating SDL font texture: %s\n",
                SDL_GetError());
    } else {
        *w = width;
        *h = height;
    }

    /* Cleanup label chain; this C idiom is one of the few legitimate uses of
       goto, because it doesn't have an appropriate control flow structure */
    SDL_FreeSurface(surface);
cleanup_info_and_rowpointers:
    if (pixeldata) free(pixeldata);
    if (rowpointers) free(rowpointers);
    png_ptr_nv = png_ptr;
    info_ptr_nv = info_ptr;
    png_destroy_read_struct(&png_ptr_nv, &info_ptr_nv, png_infopp_NULL);
    goto cleanup_fopen;
cleanup_png_ptr:
    png_ptr_nv = png_ptr;
    png_destroy_read_struct(&png_ptr_nv, png_infopp_NULL, png_infopp_NULL);
cleanup_fopen:
    fclose(in);
cleanup_nothing:
    return rv;
}


void sdl_hook_beep(void) {
    /* TODO */
}

void sdl_hook_setcursorsize(int size) {
    /* TODO */
}
void sdl_hook_positioncursor(int y, int x) {
    /* TODO */
}

/* Called whenever the window or font size changes. */
static void update_window_sizes(void) {
    /* We set the window's minimum size to 80x24 times the font size; increase
       the window to the minimum size if necessary; and decrease the window to
       an integer multiple of the font size if possible. */
    int w, h, worig, horig;
    SDL_GetWindowSize(win, &w, &h);
    worig = w; horig = h;
    w /= fontwidth; h /= fontheight;
    if (w < MINCHARWIDTH) w = MINCHARWIDTH;
    if (h < MINCHARHEIGHT) h = MINCHARHEIGHT;
    if (w != worig || h != horig) {
        SDL_SetWindowSize(win, w * fontwidth, h * fontheight);
        SDL_RenderSetLogicalSize(render, w * fontwidth, h * fontheight);
    }
    SDL_SetWindowMinimumSize(
        win, MINCHARWIDTH * fontwidth, MINCHARHEIGHT * fontheight);
    if (w != winwidth || h != winheight) resize_queued = 1;
    winwidth = w; winheight = h;
    ignore_resize_count++;
}

static void exit_handler(void) {
    if (win) {
        SDL_DestroyWindow(win);
        SDL_Quit();
    }
    win = NULL;
}
void sdl_hook_init(int *h, int *w) {
    if (!win) {
        if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }
        win = SDL_CreateWindow(
            "Uncursed",   /* title */
            SDL_WINDOWPOS_UNDEFINED, /* initial x */
            SDL_WINDOWPOS_UNDEFINED, /* initial y */
            fontwidth * winwidth, fontheight * winheight, /* size */
            SDL_WINDOW_RESIZABLE);
        if (!win) {
            fprintf(stderr, "Error creating an SDL window: %s\n",
                    SDL_GetError());
            exit(EXIT_FAILURE);
        }
        atexit(exit_handler);
        update_window_sizes();
        resize_queued = 0;
        *w = winwidth; *h = winheight;
        render = SDL_CreateRenderer(
            win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!render) {
            fprintf(stderr, "Error creating an SDL renderer: %s\n",
                    SDL_GetError());
            exit(EXIT_FAILURE);
        }
        SDL_StartTextInput();
    }
}
void sdl_hook_exit(void) {
    /* Actually tearing down the window (or worse, quitting SDL) would be
       overkill, given that this is used to allow raw writing to the console,
       and it's possible to do that behind the SDL window anyway. So do nothing;
       the atexit wil shut it down at actual system exit if the hook is called
       because the program is exiting, and otherwise we're going to have init
       called in the near future. (Perhaps we should hide the window, in case
       the code takes console input while the window's hidden, but even that
       would look weird.) */
}

void sdl_hook_set_faketerm_font_file(char *filename) {
    int w, h;
    SDL_Texture *t = load_png_file_to_texture(filename, &w, &h);
    if (t) {
        if (font) SDL_DestroyTexture(font);
        font = t;
        /* Fonts are 16x16 grids. */
        fontwidth = w / 16;
        fontheight = h / 16;
        update_window_sizes();
        sdl_hook_fullredraw(); /* draw with the new font */
    }
}

void sdl_hook_rawsignals(int raw) {
    /* meaningless to this plugin */
}
    
void sdl_hook_delay(int ms) {
    /* We want to discard keys for the given length of time. (If the window is
       resized during the delay, we keep quiet about the resize until the next
       key request, because otherwise the application wouldn't learn about it
       and might try to draw out of bounds. On a hangup, we end the delay
       early.)

       TODO: handle SDL_GetTicks overflow */
    long tick_target = SDL_GetTicks() + ms;
    suppress_resize = 1;
    while (SDL_GetTicks() < tick_target) {
        int k = sdl_hook_getkeyorcodepoint(tick_target - SDL_GetTicks());
        if (k == KEY_HANGUP) break;
    }
    suppress_resize = 0;
}

#define TEXTEDITING_FILTER_TIME 2 /* milliseconds */
int sdl_hook_getkeyorcodepoint(int timeout_ms) {
    long tick_target = SDL_GetTicks() + timeout_ms;
    long key_tick_target = -1;
    long last_textediting_tick = -1 - TEXTEDITING_FILTER_TIME;
    int kc = KEY_INVALID + KEY_BIAS, i, j, k;

    if (hangup_mode) return KEY_HANGUP + KEY_BIAS;

    do {
        SDL_Event e;
        if (!suppress_resize && resize_queued) {
            update_window_sizes();
            resize_queued = 0;
            uncursed_rhook_setsize(winheight, winwidth);
            return KEY_RESIZE + KEY_BIAS;            
        }
        if ((key_tick_target != -1 ?
             SDL_WaitEventTimeout(&e, key_tick_target - SDL_GetTicks()) :
             timeout_ms == -1 ? SDL_WaitEvent(&e) :
             SDL_WaitEventTimeout(&e, tick_target - SDL_GetTicks())) == 0) {
            /* WaitEventTimeout returns 0 on timeout expiry; both functions
               return 0 on error. */

            if (key_tick_target != -1) return kc;
            return KEY_SILENCE + KEY_BIAS;
        }
        switch (e.type) {
        case SDL_WINDOWEVENT:
            /* The events we're interested in here are closing the window,
               and resizing the window. We also need to redraw, if the
               window manager requests that. */
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
                ignore_resize_count) {
                /* We don't want to trigger a resize in response to our
                   own resize requests. */
                ignore_resize_count--;
                break;
            }
            if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                update_window_sizes();
            if (e.window.event == SDL_WINDOWEVENT_EXPOSED)
                sdl_hook_fullredraw();
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                hangup_mode = 1;
                return KEY_HANGUP + KEY_BIAS;
            }
            break;

        case SDL_TEXTEDITING:
            key_tick_target = -1;
            last_textediting_tick = SDL_GetTicks();
            break;

        case SDL_TEXTINPUT:
            /* The user pressed a key that's interpreted as a printable.
               Convert it from UTF-8 to UTF-32. */
            k = 0;
#define ett ((unsigned char)*e.text.text)
            if      (ett < 0x80) {k = ett & 0x7F; i = 0;}
            else if (ett < 0xD0) {k = ett & 0x1F; i = 1;}
            else if (ett < 0xF0) {k = ett & 0x0F; i = 2;}
            else if (ett < 0xF8) {k = ett & 0x07; i = 3;}
#undef ett
            else return KEY_INVALID + KEY_BIAS;
            k <<= i * 6;
            for (j = 1; i > 0; i--) {
                k += (((unsigned char)e.text.text[i]) & 0x3F) * j;
                j <<= 6;
            }
            if (k > 0x10ffff) return KEY_INVALID + KEY_BIAS;

            /* Hack for X11 (i.e. most practical uses of this on Linux as of
               2013): If the user presses Alt + an ASCII printable, prefer to
               send the key combination (Alt+letter), rather than the Unicode
               key it produces (just the letter by itself). */
            if (k >= ' ' && k <= '~' && kc == (k | KEY_ALT) + KEY_BIAS)
                return kc;

            return k;

        case SDL_KEYDOWN:
            /* We care about this if it's a function key (i.e. not corresponding
               to text), but not if it's used as part of an input method. The
               heuristic used is to try to handle the key itself if it didn't
               cause an SDL_TEXTEDITING or SDL_TEXTINPUT within 2 ms.

               First, though, in case that event doesn't happen, we try to
               calculate a code for the key itself, so that we can return it
               2 ms later, except in the easy case where there was a text
               editing/input event within the /previous/ 2ms. */
            if ((long)SDL_GetTicks() <
                last_textediting_tick + TEXTEDITING_FILTER_TIME) {
                break;
            }

            kc = 0;
#define K(x,y) case SDLK_##x: kc = KEY_BIAS + KEY_##y; break
#define KK(x) K(x,x)
            switch (e.key.keysym.sym) {

                /* nonprintables in SDL that correspond to control codes */

            case SDLK_RETURN: kc = '\x0d'; break;
            case SDLK_TAB: kc = '\x09'; break;

                /* nonprintables that exist in both SDL and uncursed */

                KK(F1); KK(F2); KK(F3); KK(F4); KK(F5); KK(F6); KK(F7);
                KK(F8); KK(F9); KK(F10); KK(F11); KK(F12); KK(F13); KK(F14);
                KK(F15); KK(F16); KK(F17); KK(F18); KK(F19); KK(F20);
                    
                KK(ESCAPE); KK(BACKSPACE);
                K(PRINTSCREEN,PRINT); K(PAUSE,BREAK);
                KK(HOME); KK(END); K(INSERT,IC); K(DELETE,DC); 
                K(PAGEUP,PPAGE); K(PAGEDOWN,NPAGE);
                KK(RIGHT); KK(LEFT); KK(UP); KK(DOWN);

                K(KP_DIVIDE,NUMDIVIDE); K(KP_MULTIPLY,NUMTIMES);
                K(KP_MINUS,NUMMINUS); K(KP_PLUS,NUMPLUS); K(KP_ENTER,ENTER);
                K(KP_1,C1); K(KP_2,C2); K(KP_3,C3); K(KP_4,B1); K(KP_5,B2);
                K(KP_6,B3); K(KP_7,A1); K(KP_8,A2); K(KP_9,A3); K(KP_0,D1);
                K(KP_PERIOD,D3);

                /* we intentionally ignore modifier keys */
            case SDLK_CAPSLOCK: case SDLK_SCROLLLOCK:
            case SDLK_NUMLOCKCLEAR:
            case SDLK_LCTRL: case SDLK_LSHIFT: case SDLK_LALT:
            case SDLK_RCTRL: case SDLK_RSHIFT: case SDLK_RALT:
            case SDLK_MODE: case SDLK_LGUI: case SDLK_RGUI:
                break;
                    
            default:
                /* Other keys are either printables, or else keys that
                   uncursed doesn't know about. If they're printables, we
                   just store them as is in kc. Otherwise, we synthesize a
                   number for them via masking off SLK_SCANCODE_MASK and
                   adding KEY_LAST_FUNCTION. If that goes to 512 or higher,
                   we give up. */
                if (e.key.keysym.sym >= ' ' && e.key.keysym.sym <= '~')
                    kc = e.key.keysym.sym;
                else if ((e.key.keysym.sym & ~SDLK_SCANCODE_MASK) +
                         KEY_LAST_FUNCTION < 512)
                    kc = (e.key.keysym.sym & ~SDLK_SCANCODE_MASK) +
                        KEY_LAST_FUNCTION + KEY_BIAS;
                break;
            }
#undef KK
#undef K
            if (kc) {
                if (kc >= KEY_BIAS) {
                    if (e.key.keysym.mod & KMOD_ALT) kc |= KEY_ALT;
                    if (e.key.keysym.mod & KMOD_CTRL) kc |= KEY_CTRL;
                    if (e.key.keysym.mod & KMOD_SHIFT) kc |= KEY_SHIFT;
                } else {
                    if (e.key.keysym.mod & KMOD_CTRL) kc &= ~96;
                    else if (e.key.keysym.mod & KMOD_SHIFT) {
                        if (kc >= 'a' && kc <= 'z')
                            kc -= 'a' - 'A';
                    }
                }
                if (kc <= 127 && e.key.keysym.mod & KMOD_ALT) {
                    kc |= KEY_ALT;
                    kc += KEY_BIAS;
                }
                key_tick_target = SDL_GetTicks() + TEXTEDITING_FILTER_TIME;
            }
            break;

        case SDL_QUIT:
            hangup_mode = 1;
            return KEY_HANGUP + KEY_BIAS;
        }
    } while (timeout_ms == -1 || SDL_GetTicks() < tick_target);
    return KEY_SILENCE + KEY_BIAS;
}

/* copied from tty.c; perhaps this should go in a header somewhere */
static Uint8 palette[][3] = {
    {0x00,0x00,0x00}, {0xaf,0x00,0x00}, {0x00,0x87,0x00}, {0xaf,0x5f,0x00},
    {0x00,0x00,0xaf}, {0x87,0x00,0x87}, {0x00,0xaf,0x87}, {0xaf,0xaf,0xaf},
    {0x5f,0x5f,0x5f}, {0xff,0x5f,0x00}, {0x00,0xff,0x00}, {0xff,0xff,0x00},
    {0x87,0x5f,0xff}, {0xff,0x5f,0xaf}, {0x00,0xd7,0xff}, {0xff,0xff,0xff}
};

void sdl_hook_update(int y, int x) {
    unsigned char ch = uncursed_rhook_cp437_at(y, x);
    int a = uncursed_rhook_color_at(y, x);

    Uint8 *fgcolor = palette[a & 15];
    Uint8 *bgcolor = palette[(a >> 5) & 15];
    if (a & 16)  fgcolor = palette[7];
    if (a & 512) bgcolor = palette[0];
    /* TODO: Underlining (1024s bit) */

    /* Draw the background. */
    SDL_SetRenderDrawColor(render, bgcolor[0], bgcolor[1], bgcolor[2],
                           SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(render, &(SDL_Rect){
        .x = x * fontwidth, .y = y * fontheight,
        .w = fontwidth, .h = fontheight});
    /* Draw the foreground. */
    if (!font) {
        /* Just draw blocks of color. */
        SDL_SetRenderDrawColor(render, fgcolor[0], fgcolor[1], fgcolor[2],
                               SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(render, &(SDL_Rect) {
                .x = x * fontwidth + 2, .y = y * fontheight + 2,
                .w = fontwidth - 4, .h = fontheight - 4
            });
    } else {
        /* Blit from the font onto the screen. */
        SDL_SetTextureColorMod(font, fgcolor[0], fgcolor[1], fgcolor[2]);
        SDL_RenderCopy(render, font, &(SDL_Rect) { /* source */
                .x = (ch % 16) * fontwidth, .y = (ch / 16) * fontheight,
                .w = fontwidth, .h = fontheight
            }, &(SDL_Rect) { /* destination */
                .x = x * fontwidth, .y = y * fontheight,
                .w = fontwidth, .h = fontheight
            });
    }

    uncursed_rhook_updated(y, x);
}

void sdl_hook_fullredraw(void) {
    int i, j;

    for (j = 0; j < winheight; j++)
        for (i = 0; i < winwidth; i++)
            sdl_hook_update(j, i);
}

void sdl_hook_flush(void) {
    SDL_RenderPresent(render);
}
