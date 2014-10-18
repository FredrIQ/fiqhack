/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-10-17 */
/* Copyright (c) 2014 by Sean Hunt */
/* NetHack may be freely redistributed. See license for details. */

#ifndef DGN_COMPILER_H
#define DGN_COMPILER_H

#include "config.h"

/* dgn_comp.l */
void init_yyin(FILE *);
void init_yyout(FILE *);
int yylex(void);

/* dgn_comp.y */
int yyparse(void);

/* dgn_main.c */
void yyerror(const char *);
void yywarning(const char *);
int yywrap(void);

/* dgn_comp.l */
extern int line_number;
extern FILE *yyout;

/* dgn_main.c */
extern const char *fname;
extern int fatal_error;

#endif /* DGN_COMPILER_H */
