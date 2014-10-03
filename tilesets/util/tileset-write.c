/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-03 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

#include "tilecompile.h"
#include "tilesequence.h"

/* Utility functions */

static void
write_palette_key(FILE *out, int key, int keywidth)
{
    if (keywidth == 1) {
        key++;
        key *= 64;
    }

    while (keywidth--) {
        int k = (key / 64) % 64;
        key *= 64;

        if (!k)
            putc('_', out);
        else if (k < 27)
            putc('A' + k - 1, out);
        else if (k < 53)
            putc('a' + k - 27, out);
        else if (k < 63)
            putc('0' + k - 53, out);
        else
            putc('$', out);
    }
}

static int
compare_tiles_for_image_index(const void *t1, const void *t2)
{
    const tile *t1c = t1, *t2c = t2;
    return (int)t1c->image_index - (int)t2c->image_index;
}

static int
compare_tiles_for_tile_number(const void *t1, const void *t2)
{
    const tile *t1c = t1, *t2c = t2;
    return (int)t1c->tilenumber - (int)t2c->tilenumber;
}

static const char *
tile_name_wrapper(int tileno)
{
    if (tileno < TILESEQ_COUNT)
        return name_from_tileno(tileno);
    else if (tileno > TILESEQ_COUNT &&
             tileno < TILESEQ_COUNT + 1 + unknown_name_count)
        return unknown_tile_names[tileno - TILESEQ_COUNT - 1];
    else
        return "invalid tile number";
}


/* Writes a text-format tileset from tiles_seen. Tile names will be encoded as
   text using tilesequence.c; images will be encoded using the given iiformat.

   Returns 1 on success, 0 on error. */
bool
write_text_tileset(const char *filename, enum iiformat iif)
{
    FILE *out = fopen(filename, "w");
    int keywidth = 0;
    bool embedding_images = iif == II_SPELTOUT && tileset_width != 0;

    if (!out) {
        perror(filename);
        return 0;
    }

    /* If we know the tileset name, output it. If we know the tileset
       dimensions, output those too, /except/ if we're spelling out images (and
       thus the dimensions are implicitly available from the images).  This
       makes it easier to generate tilesets Slash'EM will accept. */
    if (*tileset_name)
        fprintf(out, "# name %s\n", tileset_name);
    if (tileset_width != -1 && !embedding_images)
        fprintf(out, "# width %ld\n", tileset_width);
    if (tileset_height != -1 && !embedding_images)
        fprintf(out, "# height %ld\n", tileset_height);

    if (embedding_images) {
        /* We're going to be writing spelt-out images, so write the palette
           first. If there are more than 62 palette entries, use Slash'EM-style
           palette keys (keywidth = 2); otherwise, we can use NetHack 3 series
           palette keys (keywidth = 1) for even more portability. */
        keywidth = (palettesize > 62) ? 2 : 1;
        int i;
        for (i = 0; i < palettesize; i++)
        {
            write_palette_key(out, i, keywidth);
            if (palettechannels == 1)
                fprintf(out, " = (%d)\n", palette[i].r);
            else if (palette[i].a == 255 &&
                     (palette[i].r != TRANSPARENT_R ||
                      palette[i].g != TRANSPARENT_G ||
                      palette[i].b != TRANSPARENT_B))
                fprintf(out, " = (%d, %d, %d)\n",
                        palette[i].r, palette[i].g, palette[i].b);
            else if (palette[i].a == 0 &&
                     palette[i].r == TRANSPARENT_R &&
                     palette[i].g == TRANSPARENT_G &&
                     palette[i].b == TRANSPARENT_B)
                fprintf(out, " = (%d, %d, %d)\n",
                        TRANSPARENT_R, TRANSPARENT_G, TRANSPARENT_B);
            else
                fprintf(out, " = (%d, %d, %d, %d)\n", palette[i].r,
                        palette[i].g, palette[i].b, palette[i].a);
        }
    }

    /* Sort the tiles into image index order. We need to loop over image indexes
       rather than tiles for several reasons (generating map files, semicolon
       compression, storing otherwise unused images). This method lets us step
       through both arrays at the same time, because they're in the same
       order.

       Exception: for a cchar-based tileset, we sort by tile number instead,
       because cchar numbers are not a particularly meaningful sort order. */
    qsort(tiles_seen, seen_tile_count, sizeof (tile),
          tileset_width == 0 ? compare_tiles_for_tile_number :
          compare_tiles_for_image_index);

    int output_filepos = 0;
    int input_filepos = 0;
    int curtile = 0;
    int unused_count = 0;

    /* We could be producing one output tile per input filepos, or one per
       tile, depending on the format:

       cchar-based files are always one per tile (because there are 4 billion
       input fileposes); maps are always one per filepos (this is why you can't
       have a cchar-based map, incidentally); embedding images is one per
       filepos to avoid having to repeat images; and otherwise we use one per
       tile.

       Note: This code doesn't work if we're trying to produce a map for an
       image that isn't available. Currently, this is disallowed in main() by
       providing no set of command-line options that would do that.
    */
    if (embedding_images || iif == II_FILEPOS) {

        /* We're outputting one tile per output filepos. */
        while (input_filepos < seen_image_count)
        {
            if (!images_seen[input_filepos]) {
                /* We're deleting this image, thus causing input_filepos and
                   output_filepos to diverge. */
                input_filepos++;
                continue;
            }
            /* If embedding images, use the "# tile ()" nonsense to be compatible
               with Slash'EM. Otherwise, don't bother. */
            if (embedding_images)
                fprintf(out, "# tile %d (", output_filepos);
            /* Output all tiles with this filepos. curtile is set to the first
               tile whose filepos definitely hasn't been seen. */
            bool anyseen = 0;
            while (curtile < seen_tile_count &&
                   tiles_seen[curtile].image_index == input_filepos) {
                if (anyseen)
                    fprintf(out, "; ");
                fprintf(out, "%s%s", name_from_substitution(
                            tiles_seen[curtile].substitution),
                        tile_name_wrapper(tiles_seen[curtile].tilenumber));
                anyseen = 1;
                curtile++;
            }
            if (!anyseen) {
                /* Make sure the tile has at least one name. (Use an unknown
                   name so that the tile can get deleted again in subsequent
                   uses.) */
                fprintf(out, "unuseds %d", unused_count++);
            }
            if (embedding_images)
                fprintf(out, ")");
            fprintf(out, "\n");

            /* In the case of II_FILEPOS, we're done. In the case of
               II_SPELTOUT, we need to spell the image out. */
            if (embedding_images) {
                fprintf(out, "{\n");
                int x, y, c, p;
                for (y = 0; y < tileset_height; y++) {
                    fprintf(out, "  ");
                    for (x = 0; x < tileset_width; x++)
                        for (c = 0; c < 4 / palettechannels; c++)
                            for (p = 0; p < palettesize; p++) {
                                pixel ip = images_seen[input_filepos]
                                    [y * tileset_width + x];
                                pixel pp = palette[p];
                                if ((palettechannels == 4 &&
                                     ip.r == pp.r && ip.g == pp.g &&
                                     ip.b == pp.b && ip.a == pp.a) ||
                                    (palettechannels == 1 &&
                                     c == 0 && ip.r == pp.r) ||
                                    (c == 1 && ip.g == pp.r) ||
                                    (c == 2 && ip.b == pp.r) ||
                                    (c == 3 && ip.a == pp.r)) {
                                    write_palette_key(out, p, keywidth);
                                    break;
                                }
                            }
                    fprintf(out, "\n");
                }
                fprintf(out, "}\n");
            }
            input_filepos++;
            output_filepos++;
        }

    } else {

        /* We're outputting one tile per tile. */
        while (curtile < seen_tile_count) {

            /* Write the tile name. */
            fprintf(out, "%s%s: ", name_from_substitution(
                        tiles_seen[curtile].substitution),
                    tile_name_wrapper(tiles_seen[curtile].tilenumber));
            
            /* Determine the output filepos for this tile. If we had images,
               then we need to skip the deleted ones. Otherwise, it's the same
               as the input filepos; just copy image_index directly so that we
               don't need to iterate over the entire list of ints with a
               cchar-based tileset (not to mention, it isn't sorted in that
               case). */
            if (seen_image_count) {
                while (input_filepos < tiles_seen[curtile].image_index) {
                    if (images_seen[input_filepos])
                        output_filepos++;
                    input_filepos++;
                }
            } else {
                output_filepos = tiles_seen[curtile].image_index;
            }

            if (iif == II_HEX) {
                fprintf(out, "0x%08X\n", output_filepos);
            } else {
                assert(tileset_width == 0 && iif == II_SPELTOUT);
                assert(!"TODO: Text representation of cchars");
            }

            curtile++;
        }
    }

    if (fclose(out)) {
        perror("Error: Completing writing a text file");
        return 0; /* failure to close file */
    }

    if (embedding_images) {
        if (input_filepos != output_filepos)
            printf("Info: wrote '%s', %d images (%d omitted)\n",
                   filename, output_filepos, input_filepos - output_filepos);
        else if (unused_count)
            printf("Info: wrote '%s', %d images (%d unused)\n",
                   filename, output_filepos, unused_count);
        else
            printf("Info: wrote '%s', %d images\n", filename, output_filepos);
    } else if (iif == II_FILEPOS) {
        printf("Info: wrote '%s', %d tile names\n", filename, output_filepos);
    } else {
        printf("Info: wrote '%s', %d tile references\n", filename, curtile);
    }

    return 1;
}

bool
write_binary_tileset(const char *filename)
{
    assert(!"TODO: Writing binary tileset");
}
