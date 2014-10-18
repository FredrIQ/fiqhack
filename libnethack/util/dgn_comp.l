%{
/* Last modified by Sean Hunt, 2014-10-17 */
/*	Copyright (c) 1989 by Jean-Christophe Collet */
/*	Copyright (c) 1990 by M. Stephenson	     */
/* NetHack may be freely redistributed.  See license for details. */

#define DGN_COMP

#include "config.h"
#include "dgn_comp.h"
#include "dgn_file.h"
#include "dgn_compiler.h"

#ifdef FLEX_SCANNER
#define YY_MALLOC_DECL \
	       void * malloc(size_t); \
	       void * realloc(void *,size_t);
#define YY_NO_INPUT
#endif

/* this doesn't always get put in dgn_comp.h
 * (esp. when using older versions of bison)
 */

extern YYSTYPE yylval;

int line_number = 1;

#ifdef __clang__
/* We can't control the generated code, so we have to disable the warning
 * instead. Otherwise, it will complain about yy_fatal_error and various
 * shadowed variables. */
# pragma clang diagnostic ignored "-Wmissing-noreturn"
# pragma clang diagnostic ignored "-Wshadow"
# pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#endif

%}
%option nounput
%%
DUNGEON		return A_DUNGEON;
up		{ yylval.i=1; return UP_OR_DOWN; }
down		{ yylval.i=0; return UP_OR_DOWN; }
ENTRY		return ENTRY;
stair		return STAIR;
no_up		return NO_UP;
no_down		return NO_DOWN;
portal		return PORTAL;
PROTOFILE	return PROTOFILE;
DESCRIPTION	return DESCRIPTION;
LEVELDESC	return LEVELDESC;
ALIGNMENT       return ALIGNMENT;
LEVALIGN        return LEVALIGN;
town		{ yylval.i=TOWN ; return DESCRIPTOR; }
hellish		{ yylval.i=HELLISH ; return DESCRIPTOR; }
mazelike	{ yylval.i=MAZELIKE ; return DESCRIPTOR; }
roguelike	{ yylval.i=ROGUELIKE ; return DESCRIPTOR; }
unaligned       { yylval.i=D_ALIGN_NONE ; return DESCRIPTOR; }
noalign         { yylval.i=D_ALIGN_NONE ; return DESCRIPTOR; }
lawful          { yylval.i=D_ALIGN_LAWFUL ; return DESCRIPTOR; }
neutral         { yylval.i=D_ALIGN_NEUTRAL ; return DESCRIPTOR; }
chaotic         { yylval.i=D_ALIGN_CHAOTIC ; return DESCRIPTOR; }
BRANCH		return BRANCH;
CHAINBRANCH	return CHBRANCH;
LEVEL		return LEVEL;
RNDLEVEL	return RNDLEVEL;
CHAINLEVEL	return CHLEVEL;
RNDCHLEVEL	return RNDCHLEVEL;
[-0-9]+		{ yylval.i=atoi(yytext); return INTEGER; }
\"[^"]*\"	{ yytext[yyleng-1] = 0; /* Discard the trailing \" */
		  yylval.str = malloc(strlen(yytext+1)+1);
		  strcpy(yylval.str, yytext+1); /* Discard the first \" */
		  return STRING; }
^#.*\n		{ line_number++; }
\r?\n		{ line_number++; }
[ \t]+		;	/* skip trailing tabs & spaces */
.		{ return yytext[0]; }
%%

/* routine to switch to another input file; needed for flex */
void init_yyin( FILE *input_f )
{
#if defined(FLEX_SCANNER) || defined(FLEXHACK_SCANNER)
	if (yyin)
	    yyrestart(input_f);
	else
#endif
	    yyin = input_f;
}
/* analogous routine (for completeness) */
void init_yyout( FILE *output_f )
{
	yyout = output_f;
}

/*dgn_comp.l*/

