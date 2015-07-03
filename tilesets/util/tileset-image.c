/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-10 */
/*
 * Copyright (C) Andrew Apted <ajapted@users.sourceforge.net> 2002
 *		 Slash'EM Development Team 2003
 * Copyright (C) 2014 Alex Smith.
 *
 * NetHack may be freely redistributed.  See license for details.
 */

#define PNG_SETJMP_SUPPORTED
#include <png.h> /* must be included first, for complicated reasons to do with
                    <setjmp.h> */
#include <zlib.h>

#include "tilecompile.h"
#include "hacklib.h"

/* Loads a PNG file into images_seen as a new reference image.  The given
   FILE* pointer must be open in binary mode, and have had PNG_HEADER_SIZE
   bytes already read from it.

   This is based on code I (Alex Smith) wrote, which is in turn based on a
   public domain example that ships with libpng.

   If the PNG file has a nhTb or nhTs chunk, it'll be passed to
   load_binary_tileset or load_text_tileset respectively.  (If it has both, only
   the nhTs chunk will be read.)  This will be before reading in the images; the
   tileset loaders know that they may have to read not-yet-allocated images, but
   that's OK.

   Returns 1 on success, 0 on error. */
bool
load_png_file(FILE *in)
{
    volatile png_structp png_ptr;
    png_structp png_ptr_nv;
    volatile png_infop info_ptr;
    png_infop info_ptr_nv;
    png_unknown_chunkp unknown_ptr;

    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, has_alpha, i, j, x, y, t;

    int tilecount;
    volatile int rv = 0;
    
    /* The pointers are volatile. What they point to isn't. */
    unsigned char *volatile pixeldata = NULL;
    unsigned char **volatile rowpointers = NULL;

    /* Reset for a new image file. */
    start_of_reference_image = seen_image_count;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Error allocating png_ptr for image file\n");
        goto cleanup_fopen;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error allocating info_ptr for image file\n");
        goto cleanup_png_ptr;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error reading image file: bad PNG format\n");
        goto cleanup_info_and_rowpointers;
    }

    png_init_io(png_ptr, in);
    png_set_sig_bytes(png_ptr, PNG_HEADER_SIZE);

    /* assumes ASCII */
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);

    /* Change the data to a standard format (32bpp RGBA). */

    /* Force pixels into individual bits */
    if (bit_depth < 8)
        png_set_packing(png_ptr);

    /* Force to 8bpp */
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    /* Convert color key to alpha */
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
        has_alpha = 1;
    } else
        has_alpha = (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
                     color_type == PNG_COLOR_TYPE_RGB_ALPHA);

    /* Force to RGB */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
        color_type == PNG_COLOR_TYPE_GRAY)
        png_set_gray_to_rgb(png_ptr);

    /* Add an alpha channel, if the image still hasn't got one */
    if (!has_alpha)
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    /* Allocate memory for the PNG image */
    png_read_update_info(png_ptr, info_ptr);
    if (png_get_rowbytes(png_ptr, info_ptr) != width * 4) {
        fprintf(stderr,
                "Error in PNG memory allocation: expected %ld bytes, "
                "found %ld bytes.\n", (long)width * 4,
                (long)png_get_rowbytes(png_ptr, info_ptr));
    }

    pixeldata = malloc(height * width * 4);
    rowpointers = malloc(sizeof *rowpointers * height);
    if (!pixeldata || !rowpointers) {
        fprintf(stderr, "Error allocating image memory for PNG file\n");
        goto cleanup_info_and_rowpointers;
    }

    for (i = 0; i < height; i++)
        rowpointers[i] = pixeldata + (i * width * 4);
    png_read_image(png_ptr, rowpointers);

    png_read_end(png_ptr, NULL);

    /* Look for nhTb and/or nhTs chunks. Check for nhTs first. */
    t = png_get_unknown_chunks(png_ptr, info_ptr, &unknown_ptr);
    for (j = 0; j <= 1; j++) {
        for (i = 0; i < t; i++) {
            if (unknown_ptr[i].name[0] == 'n' &&
                unknown_ptr[i].name[1] == 'h' &&
                unknown_ptr[i].name[2] == 'T' &&
                unknown_ptr[i].name[3] == (j ? 'b' : 's')) {
                
                x = (j ? load_binary_tileset : load_text_tileset)
                    (unknown_ptr[i].data, unknown_ptr[i].size);
                t = 0; /* don't load any more nhTb/nhTs chunks */
                if (!x)
                    goto cleanup_info_and_rowpointers;
                break;
            }
        }
    }

    /* Reset the start of the reference image, in case there were text images in
       an nhTs chunk. (Not that that makes any sense, but it's better to handle
       that case gracefully than get confused about which image is which.) */
    start_of_reference_image = seen_image_count;
    
    /* By this point, we need to have seen the tileset dimensions. */
    if (tileset_width < 0 || tileset_height < 0) {
        fprintf(stderr, "Error: The dimensions of the tileset are unknown\n");
        goto cleanup_info_and_rowpointers;
    }
    if (tileset_width == 0 || tileset_height == 0) {
        fprintf(stderr,
                "Error: Cannot use an image with a text-based tileset\n");
        goto cleanup_info_and_rowpointers;
    }

    if ((width % tileset_width) != 0) {
        fprintf(stderr, "Error: The width of one tile does not divide into "
                "the width of the tile image\n");
        goto cleanup_info_and_rowpointers;
    }
    if ((height % tileset_height) != 0) {
        fprintf(stderr, "Error: The height of one tile does not divide into "
                "the height of the tile image\n");
        goto cleanup_info_and_rowpointers;
    }
    tilecount = width / tileset_width * height / tileset_height;

    /* Allocate the pixel array we return into. */
    pixel ***images = &images_seen;
    int first = start_of_reference_image;
    int *count = &seen_image_count;

    if (image_locking) {
        image_locking = 0;
        image_locked = 1;
        images = &locked_images_seen;
        first = 0;
        count = &locked_image_count;
    }

    *images = realloc(*images, (tilecount + first) * sizeof **images);
    if (!*images) {
        fprintf(stderr, "Error allocating memory for tile images\n");
        goto cleanup_info_and_rowpointers;
    }

    /* Copy the PNG data into the tiles. */
    for (t = 0; t < tilecount; t++) {

        pixel **p = *images + t + first;
        *p = malloc(tileset_width * tileset_height * sizeof (pixel));
        if (!*p) {
            fprintf(stderr, "Error allocating memory for tile image\n");
            goto cleanup_info_and_rowpointers;
        }

        (*count)++;

        i = (t % (width / tileset_width)) * tileset_width;
        j = (t / (width / tileset_width)) * tileset_height;

        for (y = 0; y < tileset_height; y++)
            for (x = 0; x < tileset_width; x++) {

                (*p)[y * tileset_width + x] = (pixel){
                    .r = rowpointers[j + y][(i + x) * 4 + 0],
                    .g = rowpointers[j + y][(i + x) * 4 + 1],
                    .b = rowpointers[j + y][(i + x) * 4 + 2],
                    .a = rowpointers[j + y][(i + x) * 4 + 3]};

                /* Special case: if alpha is zero, override the color content to
                   (71, 108, 108).  This has two purposes; one is for backwards
                   compatibility with Slash'EM; and the other is to avoid
                   needing more than one transparent entry in the palette.  (Two
                   transparent colors are indistinguishable from each other
                   anyway, so with alpha at 0, r/g/b don't render.) */
                if ((*p)[y * tileset_width + x].a == 0) {
                    (*p)[y * tileset_width + x] = (pixel){
                        .r = TRANSPARENT_R, .g = TRANSPARENT_G,
                        .b = TRANSPARENT_B, .a = 0
                    };
                }
            }
    }

    rv = 1; /* success */

    /* Cleanup label chain; this C idiom is one of the few legitimate uses of
       goto, because it doesn't have an appropriate control flow structure */
cleanup_info_and_rowpointers:
    if (pixeldata)
        free(pixeldata);
    if (rowpointers)
        free(rowpointers);
    png_ptr_nv = png_ptr;
    info_ptr_nv = info_ptr;
    png_destroy_read_struct(&png_ptr_nv, &info_ptr_nv, NULL);
    goto cleanup_fopen;
cleanup_png_ptr:
    png_ptr_nv = png_ptr;
    png_destroy_read_struct(&png_ptr_nv, NULL, NULL);
cleanup_fopen:
    fclose(in);
    return rv;
}

static png_byte *allocated_chunk;
static png_size_t allocated_chunk_len;
static bool
allocation_callback(png_byte *chunk, png_size_t len)
{
    allocated_chunk = malloc(len);
    if (!allocated_chunk) {
        fprintf(stderr, "Error: Could not allocate memory\n");
        return 0;
    }
    memcpy(allocated_chunk, chunk, len);
    allocated_chunk_len = len;
    return 1;
}

/* Writes a PNG file suitable as a reference image. All images from images_seen
   will be incorporated into one image (except deleted ones, which will not be
   included in the count). If palettechannels == 4, the global palette will be
   used. Otherwise, the image is in RGBA format.

   If add_nhTb_nhTs is set, then additional chunks will be added to embed
   text (II_HEX) and binary format tilesets.

   This code is loosely based on the Slash'EM tile utilities.

   Returns 1 on success, 0 on error. */
bool
write_png_file(const char *filename, bool add_nhTb_nhTs)
{
    volatile png_structp png_ptr = NULL;
    png_structp png_ptr_nv;
    volatile png_infop info_ptr = NULL;
    png_infop info_ptr_nv;
    png_bytep *volatile row_pointers = NULL;
    png_byte *volatile image_contents = NULL;
    volatile png_unknown_chunk uchunk[2];
    png_unknown_chunk uchunknv[2];

    FILE *volatile fp;

    volatile bool rv = 0;

    int tilecount, tilesacross, tilesdown;
    int i, j, x, y, ii, p; /* yes, we're iterating over six things at once */

    uchunk[0].data = NULL;
    uchunk[1].data = NULL;

    fp = fopen(filename, "wb");
    if (!fp) {
        perror(filename);
        goto cleanup_nothing;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Error allocating png_ptr for output file\n");
        goto cleanup_file;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error allocating info_ptr for output file\n");
        goto cleanup_memory;
    }

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        fprintf(stderr, "Error: Unknown problem writing PNG.\n");
        rv = 0;
        goto cleanup_memory;
    }

    /* Work out the number of tiles we need. */
    tilecount = 0;
    for (ii = 0; ii < seen_image_count; ii++)
        if (images_seen[ii])
            tilecount++;

    tilesacross = isqrt(tilecount);
    tilesdown = (tilecount - 1) / tilesacross + 1;

    /* We need one byte per palette entry; there are 4 channels, so we need (4 /
       palettechannels) palette entries per pixel. We use calloc so that we get
       a ready-zeroed (most likely transparent or black) background where there
       aren't tiles. */
    image_contents = calloc(tilesacross * tilesdown *
                            tileset_width * tileset_height *
                            4 / palettechannels, sizeof (png_byte));
    row_pointers = malloc(tilesdown * tileset_height * sizeof (png_bytep));

     if (!image_contents || !row_pointers) {
        fprintf(stderr, "Error allocating memor for output image\n");
        goto cleanup_memory;
    }

    for (y = 0; y < tilesdown * tileset_height; y++)
        row_pointers[y] = image_contents +
            y * tilesacross * tileset_width * 4 / palettechannels;

    /* Basic info about the image. */
    png_init_io(png_ptr, fp);
    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
    png_set_IHDR(png_ptr, info_ptr, tilesacross * tileset_width,
                 tilesdown * tileset_height, 8,
                 palettechannels == 4 ? PNG_COLOR_TYPE_PALETTE :
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    /* If we're using a palette, we need to tell libpng what it is. It uses a
       somewhat different format to us (separate RGB from alpha). */
    if (palettechannels == 4) {
        png_color png_palette[palettesize];
        png_byte alpha_palette[palettesize];
        int nonopaque_count = 0;

        assert(palettesize <= MAX_PNG_PALETTE_SIZE);

        for (p = 0; p < palettesize; p++) {
            png_palette[p].red = palette[p].r;
            png_palette[p].green = palette[p].g;
            png_palette[p].blue = palette[p].b;
            alpha_palette[p] = palette[p].a;
            if (palette[p].a != 255)
                nonopaque_count = p+1;
        }
        png_set_PLTE(png_ptr, info_ptr, png_palette, palettesize);
        png_set_tRNS(png_ptr, info_ptr, alpha_palette, nonopaque_count, NULL);
    }

    if (add_nhTb_nhTs) {
        if (!callback_with_binary_tileset(allocation_callback))
            goto cleanup_memory;
        uchunk[0].name[0] = 'n';
        uchunk[0].name[1] = 'h';
        uchunk[0].name[2] = 'T';
        uchunk[0].name[3] = 'b';
        uchunk[0].name[4] = 0;
        uchunk[0].data = allocated_chunk;
        uchunk[0].size = allocated_chunk_len;
        uchunk[0].location = PNG_HAVE_IHDR;

        if (!callback_with_text_tileset(II_FILEPOS, allocation_callback))
            goto cleanup_memory;
        uchunk[1].name[0] = 'n';
        uchunk[1].name[1] = 'h';
        uchunk[1].name[2] = 'T';
        uchunk[1].name[3] = 's';
        uchunk[1].name[4] = 0;
        uchunk[1].data = allocated_chunk;
        uchunk[1].size = allocated_chunk_len;
        uchunk[1].location = PNG_HAVE_IHDR;

        uchunknv[0] = uchunk[0];
        uchunknv[1] = uchunk[1];

        png_set_unknown_chunks(png_ptr, info_ptr, uchunknv, 2);
        png_set_unknown_chunk_location(png_ptr, info_ptr, 0, PNG_HAVE_IHDR);
        png_set_unknown_chunk_location(png_ptr, info_ptr, 1, PNG_HAVE_IHDR);
    }

    png_write_info(png_ptr, info_ptr);

    /* Copy the images into image_contents. */
    i = 0;
    j = 0;
    for (ii = 0; ii < seen_image_count; ii++)
        if (images_seen[ii]) {
            for (y = 0; y < tileset_height; y++)
                for (x = 0; x < tileset_width; x++) {
                    png_byte *icstart = image_contents +
                        ((j * tileset_height + y) *
                         (tileset_width * tilesacross) +
                         (i * tileset_width + x)) * 4 / palettechannels;
                    pixel *pix = images_seen[ii] + y * tileset_width + x;
                    if (palettechannels == 1) {
                        icstart[0] = pix->r;
                        icstart[1] = pix->g;
                        icstart[2] = pix->b;
                        icstart[3] = pix->a;
                    } else {
                        for (p = 0; p < palettesize; p++) {
                            if (palette[p].r == pix->r &&
                                palette[p].g == pix->g &&
                                palette[p].b == pix->b &&
                                palette[p].a == pix->a) {
                                *icstart = p;
                                break;
                            }
                        }
                    }
                }

            i++;
            if (i == tilesacross) {
                i = 0;
                j++;
            }
        }

    /* Write the file. */
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);

    rv = 1;

    printf("Info: wrote '%s', %d tiles (%d x %d)\n",
           filename, tilecount, tilesacross, tilesdown);

cleanup_memory:
    free(row_pointers);
    free(image_contents);
    free(uchunk[0].data);
    free(uchunk[1].data);
    uchunk[0].data = NULL;
    uchunk[1].data = NULL;
    png_ptr_nv = png_ptr;
    info_ptr_nv = info_ptr;
    if (png_ptr_nv) {
        if (info_ptr_nv)
            png_destroy_write_struct(&png_ptr_nv, &info_ptr_nv);
        else
            png_destroy_write_struct(&png_ptr_nv, NULL);
    }
cleanup_file:
    if (fclose(fp)) {
        perror("Error writing output file");
        rv = 0;
    }
cleanup_nothing:
    return rv;
}
