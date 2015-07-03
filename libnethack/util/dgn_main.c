/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-10-17 */
/* Copyright (c) 1989 by Jean-Christophe Collet */
/* Copyright (c) 1990 by M. Stephenson          */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the main function for the parser
 * and some useful functions needed by yacc
 */

#include "config.h"
#include "dlb.h"
#include "dgn_compiler.h"

#define MAX_ERRORS 25

const char *fname = "(stdin)";
int fatal_error = 0;

static const char *outprefix = "";

int
main(int argc, char **argv)
{
    char infile[1024], outfile[1024], basename[1024];
    FILE *fin, *fout;
    int i, len;
    boolean errors_encountered = FALSE;

    strcpy(infile, "(stdin)");
    fin = stdin;
    strcpy(outfile, "(stdout)");
    fout = stdout;

    if (argc == 1) {    /* Read standard input */
        init_yyin(fin);
        init_yyout(fout);
        yyparse();
        if (fatal_error > 0)
            errors_encountered = TRUE;
    } else {
        /* first two args may be "-o outprefix" */
        i = 1;
        if (!strcmp(argv[1], "-o") && argc > 3) {
            outprefix = argv[2];
            i = 3;
        }
        /* Otherwise every argument is a filename */
        for (; i < argc; i++) {
            fname = strncpy(infile, argv[i], sizeof (infile));
            /* the input file had better be a .pdf file */
            len = strlen(fname) - 4;    /* length excluding suffix */
            if (len < 0 || strncmp(".pdf", fname + len, 4)) {
                fprintf(stderr, "Error - file name \"%s\" in wrong format.\n",
                        fname);
                errors_encountered = TRUE;
                continue;
            }

            /* build output file name */
            /* Use the whole name - strip off the last 3 or 4 chars. */
            strncpy(basename, infile, len);
            basename[len] = '\0';

            outfile[0] = '\0';
            strcat(outfile, outprefix);
            strcat(outfile, basename);

            fin = freopen(infile, "r", stdin);
            if (!fin) {
                fprintf(stderr, "Can't open %s for input.\n", infile);
                perror(infile);
                errors_encountered = TRUE;
                continue;
            }
            fout = freopen(outfile, WRBMODE, stdout);
            if (!fout) {
                fprintf(stderr, "Can't open %s for output.\n", outfile);
                perror(outfile);
                errors_encountered = TRUE;
                continue;
            }
            init_yyin(fin);
            init_yyout(fout);
            yyparse();
            line_number = 1;
            if (fatal_error > 0) {
                errors_encountered = TRUE;
                fatal_error = 0;
            }
        }
    }
    if (fout && fclose(fout) < 0) {
        fprintf(stderr, "Can't finish output file.");
        perror(outfile);
        errors_encountered = TRUE;
    }
    exit(errors_encountered ? EXIT_FAILURE : EXIT_SUCCESS);
     /*NOTREACHED*/ return 0;
}

/*
 * Each time the parser detects an error, it uses this function.
 * Here we take count of the errors. To continue farther than
 * MAX_ERRORS wouldn't be reasonable.
 */

void
yyerror(const char *s)
{
    fprintf(stderr, "%s : line %d : %s\n", fname, line_number, s);
    if (++fatal_error > MAX_ERRORS) {
        fprintf(stderr, "Too many errors, good bye!\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Just display a warning (that is : a non fatal error)
 */
void
yywarning(const char *s)
{
    fprintf(stderr, "%s : line %d : WARNING : %s\n", fname, line_number, s);
}

int
yywrap(void)
{
    return 1;
}

/*dgn_main.c*/

