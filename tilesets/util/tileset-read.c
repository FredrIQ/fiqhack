/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-03 */
/* Copyright (C) 2014 Alex Smith. */
/* NetHack may be freely redistributed. See license for details. */

#include "tilecompile.h"
#include "tilesequence.h"

#define EPRINT(s, ...)                                       \
    do {                                                     \
        fprintf(stderr, "line %d: " s, lineno, __VA_ARGS__); \
        return 0;                                            \
    } while (0)

#define EPRINTN(s)                                           \
    do {                                                     \
        fprintf(stderr, "line %d: " s, lineno);              \
        return 0;                                            \
    } while (0)

/* Utility functions */

static int
palette_key_to_int(png_byte key)
{
    if (key == '_')
        return 0;
    else if (key == '$')
        return 63;
    else if (key >= 'A' && key <= 'Z')
        return key - 'A' + 1;
    else if (key >= 'a' && key <= 'z')
        return key - 'a' + 27;
    else if (key >= '0' && key <= '9')
        return key - '0' + 53;
    else
        return -1;
}

/* Binary loading. This /might/ come from a PNG file, so we use png_byte.
   Returns 1 on success, 0 on error. */
bool
load_binary_tileset(png_byte *data, png_size_t size)
{
    png_bytep dp;
    long tw, th;

    if (size < TILESET_NAME_SIZE + 2 + 2 + sizeof BINARY_TILESET_HEADER) {
        fprintf(stderr, "Binary tileset too short");
        return 0;
    }
    if (memcmp(data, BINARY_TILESET_HEADER,
               sizeof BINARY_TILESET_HEADER) != 0) {
        fprintf(stderr, "Bad binary tileset header");
        return 0;
    }

    /* Load the fields from the header. */
    memcpy(tileset_name, data + sizeof BINARY_TILESET_HEADER,
           TILESET_NAME_SIZE);
    tileset_name[TILESET_NAME_SIZE] = '\0';

    dp = data + (sizeof BINARY_TILESET_HEADER) + TILESET_NAME_SIZE;
    tw = dp[0] + (dp[1] << 8);
    th = dp[2] + (dp[3] << 8);

    if (tileset_width != -1 && tileset_width != tw) {
        fprintf(stderr, "Error: inconsistent width (%ld, %ld)\n",
                tileset_width, tw);
        return 0;
    }
    if (tileset_height != -1 && tileset_height != th) {
        fprintf(stderr, "Error: inconsistent width (%ld, %ld)\n",
                tileset_height, th);
        return 0;
    }

    tileset_width = tw;
    tileset_height = th;

    for (dp += 4; dp - data < size; dp += 16) {
        if (seen_tile_count >= allocated_tile_count) {
            allocated_tile_count += 8;
            allocated_tile_count *= 2;
            tiles_seen = realloc(tiles_seen, allocated_tile_count *
                                 sizeof *tiles_seen);
            if (!tiles_seen) {
                fprintf(stderr, "Error: not enough memory for tiles\n");
                return 0;
            }
        }

        uint32_t tileno =
            (((uint32_t)dp[0]) << 0) +
            (((uint32_t)dp[1]) << 8) +
            (((uint32_t)dp[2]) << 16) +
            (((uint32_t)dp[3]) << 24);
        uint64_t substitution =
            (((uint64_t)dp[4]) << 0) +
            (((uint64_t)dp[5]) << 8) +
            (((uint64_t)dp[6]) << 16) +
            (((uint64_t)dp[7]) << 24) +
            (((uint64_t)dp[8]) << 32) +
            (((uint64_t)dp[9]) << 40) +
            (((uint64_t)dp[10]) << 48) +
            (((uint64_t)dp[11]) << 56);
        uint32_t image_index =
            (((uint32_t)dp[12]) << 0) +
            (((uint32_t)dp[13]) << 8) +
            (((uint32_t)dp[14]) << 16) +
            (((uint32_t)dp[15]) << 24);

        tiles_seen[seen_tile_count++] =
            (tile){.tilenumber = tileno,
                   .substitution = substitution,
                   .image_index = image_index + start_of_reference_image};
    }

    return 1;
}

/* Text loading. This has the same API as binary loading. */
bool
load_text_tileset(png_byte *data, png_size_t size)
{
    png_size_t i;

    /* We also need to store the palette for the current file, so that we can
       interpret what it means. */
    int filepalettechannels = 0, filekeywidth = 0;
    pixel filepalette[MAX_PALETTE_SIZE];
    bool filepalettevalid[MAX_PALETTE_SIZE] = {0};

    /* Sometimes, tiles are given without the s-number suffix. For such cases,
       we need to record how many times we've seen the unsuffixed tile within
       this file. */
    int unsuffixed_seen_count[TILESEQ_COUNT] = {0};

    /* Some tilesets specify image references via file position. */
    int filepos = 0;
    bool using_filepos = 0, avoiding_filepos = 0;

    int lineno = 1;

    int pk1, pk2, pk, pw;

    /* Run through the entirety of data. We convert newline to NUL to make
       parsing easier, and also check for illegal characters (anything outside
       the ASCII range; no tile keys contain Unicode). */
    for (i = 0; i < size; i++) {
        if (data[i] == '\n') {
            data[i] = 0;
            lineno++;
        } else if (data[i] == '\t')
            data[i] = ' ';
        else if (data[i] < ' ' || data[i] > '~')
            EPRINT("Error: Invalid byte '\\x%02X' in text file\n",
                   (int)data[i]);
    }

    /* Requiring text files to end with newlines, like they're supposed to,
       makes things much simpler. */
    if (data[size-1])
        EPRINTN("Error: text file does not end with a newline\n");

    /* Run through the file again, parsing each line.

       (Using strlen rather than memchr would be simpler, but we can't guarantee
       that the pointer has the correct signedness. rawmemchr would also be
       simpler, but it's a GNU extension.) */
    for (i = 0, lineno = 1; i < size;
         i = (png_byte *)memchr(data + i, 0, size - i) - data + 1, lineno++) {

        int len = (png_byte *)memchr(data + i, 0, size - i) - data;

        /* Is it a comment line (leading !)? */
        if (data[i] == '!')
            continue;

        /* Or a blank line? */
        if (!data[i])
            continue;

        /* Is it a palette line? These are one or two characters followed by
           " = (". */
        pw = 0;
        pk1 = palette_key_to_int(data[i + 0]);
        pk2 = palette_key_to_int(data[i + 1]);

        if (pk1 >= 0 && memcmp(data + i + 1, " = (", 4) == 0) {
            pw = 1;
            pk = pk1;
        } else if (pk1 >= 0 && pk2 >= 0 &&
                   memcmp(data + i + 2, " = (", 4) == 0) {
            pw = 2;
            pk = pk1 * 64 + pk2;
        }

        if (pw) {
            if (filekeywidth && filekeywidth != pw)
                EPRINTN("Error: text file has inconsistent palette keys\n");

            filekeywidth = pw;

            /* Now parse the numbers given. */
            uint8_t *keylocs[] =
                { &(filepalette[pk].r), &(filepalette[pk].g),
                  &(filepalette[pk].b), &(filepalette[pk].a) };
            int curchannel = 0;
            unsigned long curchannelval = 0;
            png_bytep dp = data + i + pw + 4;
            for (;;) {
                if (*dp >= '0' && *dp <= '9') {
                    curchannelval *= 10; curchannelval += *dp - '0';
                } else if (*dp == ',' || *dp == ')') {
                    if (curchannel >= 4)
                        EPRINTN("Error: images are limited to 4 channels\n");
                    if (curchannelval > 255)
                        EPRINTN("Error: channel value above 255\n");
                    *(keylocs[curchannel]) = (uint8_t) curchannelval;
                    curchannelval = 0;
                    curchannel++;
                    if (*dp == ')')
                        break;
                } else if (*dp == ' ') {
                    /* do nothing */
                } else if (!*dp) {
                    EPRINTN("Error: missing ')' in palette\n");
                } else {
                    EPRINT("Error: unexpected '%c' in palette\n", (int)*dp);
                }
                dp++;
            }
            if (curchannel == 3) {
                /* Add an alpha channel based on Slash'EM's transparency
                   rules. */
                curchannel = 4;
                *(keylocs[3]) = (*keylocs[0] == TRANSPARENT_R &&
                                 *keylocs[1] == TRANSPARENT_G &&
                                 *keylocs[2] == TRANSPARENT_B) ? 0 : 255;
            }
            if (curchannel != 1 && curchannel != 4)
                EPRINTN("Error: palette entries must be "
                        "1, 3, or 4 channels wide\n");
            if (filepalettechannels && filepalettechannels != curchannel)
                EPRINTN("Error: image is both paletted and RGBA\n");
            if (filepalettevalid[pk])
                EPRINT("Error: duplicate palette key '%.*s'\n", pw, data + i);
            filepalettevalid[pk] = true;
            filepalettechannels = curchannel;
            continue;
        }

        /* It's not a palette. Is it a name/width/height directive? */

        if (len > 7 && memcmp(data + i, "# name ", 7) == 0) {
            if (len > TILESET_NAME_SIZE)
                EPRINTN("Error: tileset specifies overlong name\n");
            memcpy(tileset_name, data + i + 7, len + 1);
            continue;
        }

        /* We combine the code for width and height directives, as it's
           almost the same. */
        {
            png_bytep dp = 0;
            long *td = 0;
            if (len > 8 && memcmp(data + i, "# width ", 8) == 0) {
                td = &tileset_width;
                dp = data + i + 8;
            } else if (len > 9 && memcmp(data + i, "# height ", 9) == 0) {
                td = &tileset_height;
                dp = data + i + 9;
            }
            if (td) {
                long v = 0;
                while (*dp) {
                    if (*dp >= '0' && *dp <= '9') {
                        v *= 10;
                        v += *dp - '0';
                        if (v > INT_MAX)
                            EPRINTN("Error: width/height too large\n");
                    } else if (*dp != ' ') {
                        EPRINT("Error: unexpected '%c' in width/height\n",
                               (int)*dp);
                    }
                    dp++;
                }
                if (*td != -1 && *td != v)
                    EPRINT("Error: inconsistent width/height "
                           "(%ld, %ld)\n", *td, v);
                *td = v;
                continue;
            }
        }

        /* At this point, it must be a tile definition (or a syntax error).

           First, find the tile name. It's in one of these formats:

           # tile 0 (tile name)\0
           # tile 0 (tile name):
           # tile name\0
           # tile name: */
        {
            /* len + 1, plus "s 4294967295"; we cannot use a VLA here, it
               causes a stack leak in gcc (!) */
            char *tilename = malloc(len + 12);
            png_bytep dp = data + i;
            char *tp = tilename;
            if (len > 6 && memcmp(data + i, "# tile ", 6) == 0) {
                while (*dp && *dp != '(')
                    dp++;
                if (*dp == '(')
                    dp++;
            }
            while (*dp && *dp != ')' && *dp != ':') {
                *(tp++) = *(dp++);
            }
            *tp = '\0';

            /* Remove any comment from the tilename. */
            if (strncmp(tilename, "cmap / ", 7) == 0)
                memmove(tilename , tilename + 7, strlen(tilename) - 6);
            else if (strstr(tilename, " / "))
                *(strstr(tilename, " / ")) = '\0';

            /* Do a sanity check on the tile name we found, in case it's
               someone trying to use syntax from elsewhere in the file and
               getting it wrong. */
            if (strspn(tilename, "-, 0123456789"
                       "abcdefghijklmnopqrstuvwxyz"
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != strlen(tilename))
                EPRINT("Error: Syntax error or bad tile name '%s'\n",
                        tilename);

            const char *tp2 = tilename;
            tp = tilename + strlen(tilename);
            unsigned long long substitution = substitution_from_name(&tp2);
            int tileno = tileno_from_name(tp2, TILESEQ_INVALID_OFF);

            /* If it isn't a valid name, it may be that we have to
               disambiguate. Try with an "s 0" suffix. If that works, change
               the 0 to the actual unsuffixed seen count and try again. */
            if (tileno == TILESEQ_INVALID_OFF) {
                strcat(tilename, "s 0");
                tileno = tileno_from_name(tp2, TILESEQ_INVALID_OFF);
                if (tileno != TILESEQ_INVALID_OFF) {
                    sprintf(tp, "s %d", unsuffixed_seen_count[tileno]++);
                    tileno = tileno_from_name(tp2, TILESEQ_INVALID_OFF);
                }
                *tp = '\0';
            }

            /* If it still isn't a valid name, then warn and ignore the
               reference, unless we've specifically been told to store the
               reference anyway, in which case invent a tile number for it. */
            if (tileno == TILESEQ_INVALID_OFF) {
                if (copy_unknown_tile_names) {
                    if (unknown_name_count == allocated_name_count) {
                        allocated_name_count += 8;
                        allocated_name_count *= 2;
                        unknown_tile_names =
                            realloc(unknown_tile_names,
                                    allocated_name_count *
                                    sizeof *unknown_tile_names);
                        if (!unknown_tile_names)
                            EPRINTN("Error: Could not allocate memory\n");
                    }
                    unknown_tile_names[unknown_name_count] =
                        malloc(strlen(tilename) + 1);
                    if (!unknown_tile_names[unknown_name_count])
                        EPRINTN("Error: Could not allocate memory\n");
                    strcpy(unknown_tile_names[unknown_name_count], tilename);
                    tileno = TILESEQ_COUNT + 1 + unknown_name_count;
                    unknown_name_count++;
                } else {
                    fprintf(stderr, "Warning: unknown tile '%s'\n", tilename);
                    filepos++;

                    /* Check for a tile image. If we find one, we must discard
                       it. */
                    if (*dp == ')')
                        dp++;

                    if (!*dp && dp - data < size - 1 && dp[1] == '{') {
                        while (*dp != '}') {
                            if (!*dp)
                                lineno++;
                            dp++;
                            if (dp >= data + size)
                                EPRINTN("Error: unmatched '{'\n");
                        }
                        i = dp - data;
                    }

                    free(tilename);
                    continue;
                }
            }

            free(tilename);

            /* Now we have to parse the tile data. dp is looking either at
               the data, or a closing paren just before it. */
            if (*dp == ')')
                dp++;

            /* Possible formats for the data:
               "\0{" followed by a literal image
               ": " followed by a hexadecimal number
               ": " followed by a cchar string
               absent, in which case we use file position

               No valid cchar string starts "0x", so we can just look at
               the next few characters to disambiguate. */
            uint32_t ii;
            if (*dp == ':' && dp[1] == ' ') {
                if (using_filepos)
                    EPRINTN("Error: cannot mix map and tileset syntax\n");
                avoiding_filepos = 1;

                if (dp[2] == '0' && dp[3] == 'x') {
                    dp += 4;
                    ii = 0;
                    while (*dp) {
                        ii *= 16;
                        if (*dp >= '0' && *dp <= '9')
                            ii += *dp - '0';
                        else if (*dp >= 'a' && *dp <= 'f')
                            ii += *dp - 'a' + 10;
                        else if (*dp >= 'A' && *dp <= 'F')
                            ii += *dp - 'A' + 10;
                        else
                            EPRINTN("Error: bad hex number\n");
                        dp++;
                    }
                    /* Note: this assumes that start_of_reference_image is
                       always 0 for cchar-based tilesets. We verify this
                       fact later, when /writing/ the tileset. For now, it's
                       harmless, and we may not have enough information to
                       check (e.g. the first file is a list of name:ii pairs,
                       which is ambiguous). */
                    ii += start_of_reference_image;
                } else {
                    assert(!"TODO: cchar string");
                }
            } else if (!*dp) {
                if (dp - data < size - 1 && dp[1] == '{') {
                    if (using_filepos)
                        EPRINTN("Error: cannot mix map and tileset syntax\n");
                    avoiding_filepos = 1;

                    /* Allocate some memory to store the new image in. If this
                       is the very first image, we might not know how large it
                       is, but it can't possibly have any more pixels than size,
                       and overallocating isn't an issue if we only do it once.
                       If it isn't, we know how much memory to allocate. */
                    int pixelcount;
                    if (tileset_width == -1)
                        pixelcount = size;
                    else if (tileset_width == 0 || tileset_height == 0)
                        EPRINTN("Error: Text-based tilesets cannot have image "
                                "data \n");
                    else
                        pixelcount = tileset_width * tileset_height;

                    if (!filekeywidth)
                        EPRINTN("Error: Textual images need a palette\n");

                    pixel *image = malloc(pixelcount * sizeof (pixel));
                    int x = 0, y = -1, w = tileset_width, c = 0;
                    lineno++;
                    for (dp += 2; *dp != '}'; dp++) {
                            if (dp >= data + size)
                                EPRINTN("Error: unmatched '{'\n");
                        if (*dp == ' ')
                            continue;
                        if (*dp == 0) {
                            y++;
                            lineno++;
                            if (w == -1 && y)
                                w = x;
                            else if (w != x && y)
                                EPRINTN("Error: Image width is not consistent\n");
                            if (!w)
                                EPRINTN("Error: Image has zero width\n");
                            x = 0;
                            continue;
                        }
                        if (y < 0)
                            EPRINTN("Error: Junk after opening brace\n");
                        if (tileset_height != -1 && y >= tileset_height)
                            EPRINTN("Error: Image height is not consistent\n");
                        if (x > w && w != -1)
                            EPRINTN("Error: Image width is not consistent\n");

                        int pk = palette_key_to_int(*dp);
                        if (pk < 0)
                            EPRINT("Error: Invalid '%c' in image\n", *dp);
                        if (filekeywidth == 2) {
                            pk *= 64;
                            int pk2 = palette_key_to_int(*++dp);
                            if (pk2 < 0)
                                EPRINT("Error: Invalid '%c' in image\n", *dp);
                            pk += pk2;
                        }

                        if (!filepalettevalid[pk])
                            EPRINTN("Error: Undefined palette key\n");

                        if (filepalettechannels == 4)
                            image[y * w + x] = filepalette[pk];
                        else if (c == 0)
                            image[y * w + x].r = filepalette[pk].r;
                        else if (c == 1)
                            image[y * w + x].g = filepalette[pk].r;
                        else if (c == 2)
                            image[y * w + x].b = filepalette[pk].r;
                        else if (c == 3)
                            image[y * w + x].a = filepalette[pk].r;

                        c += filepalettechannels;
                        if (c == 4) {
                            c = 0;
                            x++;
                        }
                    }

                    if (y == 0)
                        EPRINTN("Error: Image has zero width\n");

                    if (tileset_height == -1)
                        tileset_height = y;
                    else if (y != tileset_height)
                        EPRINTN("Error: Image height is not consistent\n");

                    if (tileset_width == -1)
                        tileset_width = w;
                    else if (w != tileset_width)
                        EPRINTN("Error: Image width is not consistent\n");

                    images_seen = realloc(images_seen, (seen_image_count + 1)
                                          * sizeof *images_seen);
                    if (!images_seen)
                        EPRINTN("Error allocating memory for tile images\n");

                    ii = seen_image_count;
                    images_seen[seen_image_count++] = image;

                    i = dp - data;

                } else {
                    if (avoiding_filepos)
                        EPRINTN("Error: cannot mix map and tileset syntax\n");
                    using_filepos = 1;
                    ii = filepos++ + start_of_reference_image;
                }
            } else {
                EPRINTN("Error: junk after tile name\n");
            }

            /* We now have our tile. */
            if (seen_tile_count >= allocated_tile_count) {
                allocated_tile_count += 8;
                allocated_tile_count *= 2;
                tiles_seen = realloc(tiles_seen, allocated_tile_count *
                                     sizeof *tiles_seen);
                if (!tiles_seen)
                    EPRINTN("Error: not enough memory for tiles\n");
            }
            tiles_seen[seen_tile_count++] =
                (tile){.tilenumber = tileno,
                       .substitution = substitution,
                       .image_index = ii};
        }
    }

    if (palette_locking)
    {
        if (palette_locked) {
            /* not EPRINT; this has nothing to do with the current line */
            fprintf(stderr, "Error: attempt to lock the palette twice\n");
            return 0;
        }

        /* Copy the palette we've been working from to the global palette. */
        palettesize = 0;
        palettechannels = filepalettechannels;

        int i;
        for (i = 0; i < MAX_PALETTE_SIZE; i++) {
            if (filepalettevalid[i])
                palette[palettesize++] = filepalette[i];
        }

        palette_locking = 0;
        palette_locked = 1;
    }

    return 1;
}
