/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-03 */
/*   Copyright (c) NetHack Development Team 1995                    */
/*   NetHack may be freely redistributed.  See license for details. */

#error !AIMAKE_FAIL_SILENTLY!
This is currently unused, and needs updating to the new tile system.


/* Routines for magnifying tiles by a factor of 2, based on the Scale2X
   algorithm from AdvanceMAME <http://advancemame.sourceforge.net/scale2x.html>.

                                  B
   Pixel E from a source image  D E F  is magnified to a set of 4 destination
                                  H
           E0 E1
   pixels  E2 E3  by the following rules:

   E0 = D == B && B != F && D != H ? D : E;
   E1 = B == F && B != D && F != H ? F : E;
   E2 = D == H && D != B && H != F ? D : E;
   E3 = H == F && D != H && B != F ? F : E; */

#include "tile.h"

void
magnify_tile_in_place(pixel (*pixels)[MAX_TILE_X], const char *name,
                      int *max_x, int *max_y)
{
    int i, j;
    pixel bigpixels[MAX_TILE_Y][MAX_TILE_X];
    pixel pixB, pixD, pixE, pixF, pixH;

    /* All tiles magnify the same way, regardless of their names. */
    (void) name;

    if (*max_x * 2 > MAX_TILE_X || *max_y * 2 > MAX_TILE_Y) {
        fprintf(stderr, "error: magnifying tile makes it too large\n");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < *max_y; j++)
        for (i = 0; i < *max_x; i++) {
            pixE = pixels[j][i];
            pixB = ((j == 0) ? pixE : pixels[j - 1][i]);
            pixD = ((i == 0) ? pixE : pixels[j][i - 1]);
            pixF = ((i == (*max_x - 1)) ? pixE : pixels[j][i + 1]);
            pixH = ((j == (*max_y - 1)) ? pixE : pixels[j + 1][i]);
            bigpixels[2 * j][2 * i] =
                ((pixel_equal(pixD, pixB) && !pixel_equal(pixB, pixF) &&
                  !pixel_equal(pixD, pixH)) ? pixD : pixE);
            bigpixels[2 * j][2 * i + 1] =
                ((pixel_equal(pixB, pixF) && !pixel_equal(pixB, pixD) &&
                  !pixel_equal(pixF, pixH)) ? pixF : pixE);
            bigpixels[2 * j + 1][2 * i] =
                ((pixel_equal(pixD, pixH) && !pixel_equal(pixD, pixB) &&
                  !pixel_equal(pixH, pixF)) ? pixD : pixE);
            bigpixels[2 * j + 1][2 * i + 1] =
                ((pixel_equal(pixH, pixF) && !pixel_equal(pixD, pixH) &&
                  !pixel_equal(pixB, pixF)) ? pixF : pixE);
        }
    *max_x *= 2;
    *max_y *= 2;
    memcpy(pixels, bigpixels, sizeof bigpixels);
}

/*magtile.c*/
