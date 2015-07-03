/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-10-17 */
/* Copyright (c) 2014 by Sean Hunt */
/* NetHack may be freely redistributed. See license for details. */

#ifndef LEV_COMPILER_H
#define LEV_COMPILER_H

#include "config.h"
#include "hack.h"
#include "verinfo.h"
#include "sp_lev.h"
#ifdef STRICT_REF_DEF
# include "tcap.h"
#endif

/* lev_comp.l */
void init_yyin(FILE *);
void init_yyout(FILE *);
int yylex(void);

/* lev_comp.y */
int yyparse(void);

/* lev_main.c */
void yyerror(const char *);
void yywarning(const char *);
int yywrap(void);
int get_floor_type(char);
int get_room_type(char *);
int get_trap_type(char *);
int get_monster_id(char *,char);
int get_object_id(char *,char);
boolean check_monster_char(char);
boolean check_object_char(char);
char what_map_char(char);
void scan_map(char *);
void wallify_map(void);
boolean check_subrooms(void);
void check_coord(int,int,const char *);
void store_part(void);
void store_room(void);
boolean write_level_file(char *,splev *,specialmaze *);
void free_rooms(splev *);

/* lev_comp.y */
extern char tmpmessage[];
extern char tmphallumsg[];
extern altar *tmpaltar[];
extern lad *tmplad[];
extern stair *tmpstair[];
extern digpos *tmpdig[];
extern digpos *tmppass[];
extern char *tmpmap[];
extern region *tmpreg[];
extern lev_region *tmplreg[];
extern door *tmpdoor[];
extern room_door *tmprdoor[];
extern trap *tmptrap[];
extern monster *tmpmonst[];
extern object *tmpobj[];
extern drawbridge *tmpdb[];
extern walk *tmpwalk[];
extern gold *tmpgold[];
extern fountain *tmpfountain[];
extern sink *tmpsink[];
extern pool *tmppool[];
extern engraving *tmpengraving[];
extern mazepart *tmppart[];
extern room *tmproom[];

extern int n_olist, n_mlist, n_plist;

extern unsigned int nlreg, nreg, ndoor, ntrap, nmons, nobj;
extern unsigned int ndb, nwalk, npart, ndig, npass, nlad, nstair;
extern unsigned int naltar, ncorridor, nrooms, ngold, nengraving;
extern unsigned int nfountain, npool, nsink;

extern unsigned int max_x_map, max_y_map;

extern int line_number, colon_line_number;

/* lev_main.c */
extern const char *fname;
extern int fatal_error;
extern int want_warnings;

#endif /* LEV_COMPILER_H */
