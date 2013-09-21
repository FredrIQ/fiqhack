/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* $Id: mapmerge.c,v 1.1 2002/09/01 21:58:19 j_ali Exp $ */
/* Copyright (c) Slash'EM Development Team 2002 */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <stdio.h>

#define BUFSZ 256

int
main(int argc, char **argv)
{
    FILE *fpi, *fpo;
    int j, tile_number = 0;
    char buf[BUFSZ], buf2[BUFSZ];

    if (argc < 2) {
        (void)fprintf(stderr, "usage: mapmerge outfile [infile] ...\n");
        exit(EXIT_FAILURE);
    }
    fpo = fopen(argv[1], "w");
    if (!fpo) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }
    for (j = 2; j < argc; j++) {
        fpi = fopen(argv[j], "r");
        if (!fpi) {
            perror(argv[j]);
            fclose(fpo);
            remove(argv[1]);
            exit(EXIT_FAILURE);
        }
        while (fgets(buf, sizeof (buf), fpi)) {
            if (sscanf(buf, "tile %*d %[^\n]", buf2) == 1)
                fprintf(fpo, "tile %d %s\n", tile_number++, buf2);
            else
                fputs(buf, fpo);
        }
        fclose(fpi);
    }
    fclose(fpo);
    exit(EXIT_SUCCESS);
}
