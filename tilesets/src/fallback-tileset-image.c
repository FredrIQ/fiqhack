/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-10 */
/* Copyright (C) 2015 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

/* Erroring-out implementations of load_png_file, write_png_file which are used
   when we don't compile with GUI support. The first line is indented
   differently from usual, so that a grep will find the implementations in
   tileset-image rather than these ones. */

#include "tilecompile.h"

bool load_png_file(FILE *in) 
{
    fclose(in);
    fprintf(stderr, "Cannot open PNG image; PNG support "
            "was not compiled into this tilecompile\n");
    return 0;
}

bool write_png_file(const char *filename, bool add_nhTb_nhTs)
{
    fprintf(stderr, "Cannot write PNG image; PNG support "
            "was not compiled into this tilecompile\n");
    return 0;
}

