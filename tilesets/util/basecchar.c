/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-10 */
/* Copyright (c) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

/* Generates a default ASCII tileset using information from the game core. This
   is missing a few tiles (invis, genbranding, monbranding) that the game core
   doesn't know about or doens't have data on; those will need to be specified
   in a separate data file.

   Almost all the hard work is done by tilesequence.c; this is just a
   wrapper. */

#include "tilesequence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Usage: basecchar -o outputfile.txt\n");
        return EXIT_FAILURE;
    }

    FILE *out = fopen(argv[2], "w");
    if (!out) {
        perror(argv[2]);
        return EXIT_FAILURE;
    }

    int i;
    for (i = 0; i < TILESEQ_COUNT; i++) {
        unsigned long cchar = cchar_from_tileno(i);
        if (cchar == ULONG_MAX)
            continue;

        /* Translate black to dark gray. */
        if (!(cchar & (0x1fUL << 21)))
            cchar |= 8UL << 21;

        fprintf(out, "%s: 0x%08lX\n", name_from_tileno(i), cchar);
    }

    if (fclose(out)) {
        perror("Error: Closing output file");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
