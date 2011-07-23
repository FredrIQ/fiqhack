/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

int (*afternmv)(void);
int (*occupation)(void);

/* from xxxmain.c */
int hackpid;		/* current process id */

int bases[MAXOCLASSES];

int multi;
int nroom;
int nsubroom;
int occtime;

int x_maze_max, y_maze_max;	/* initialized in main, used in mkmaze.c */
int otg_temp;			/* used by object_to_glyph() [otg] */

/*
 *	The following structure will be initialized at startup time with
 *	the level numbers of some "important" things in the game.
 */
struct dgn_topology dungeon_topology;

#include "quest.h"
struct q_score	quest_status;

int smeq[MAXNROFROOMS+1];
int doorindex;

char *saved_cmd = NULL;
int killer_format;
const char *killer;
const char *delayed_killer;
#ifdef GOLDOBJ
long done_money;
#endif
char killer_buf[BUFSZ];
const char *nomovemsg;
const char nul[40];			/* contains zeros */
char plname[PL_NSIZ];			/* player name */
char pl_character[PL_CSIZ];
char pl_race;

char pl_fruit[PL_FSIZ];
int current_fruit;
struct fruit *ffruit = NULL;

char tune[6];

const char *occtxt;
const char quitchars[] = " \r\n\033";
const char vowels[] = "aeiouAEIOU";
const char ynchars[] = "yn";
const char ynqchars[] = "ynq";
const char ynaqchars[] = "ynaq";
const char ynNaqchars[] = "yn#aq";
long yn_number;

const char disclosure_options[] = "iavgc";

#if defined(WIN32)
char hackdir[PATHLEN];		/* where rumors, help, record are */
#endif


struct linfo level_info[MAXLINFO];

struct sinfo program_state;

const schar xdir[11] = { -1,-1, 0, 1, 1, 1, 0,-1, 0, 0, 0 };
const schar ydir[11] = {  0,-1,-1,-1, 0, 1, 1, 1, 0, 0, 0 };
const schar zdir[11] = {  0, 0, 0, 0, 0, 0, 0, 0, 1,-1, 0 };

schar tbx, tby;	/* mthrowu: target */

/* for xname handling of multiple shot missile volleys:
   number of shots, index of current one, validity check, shoot vs throw */
struct multishot m_shot = { 0, 0, STRANGE_OBJECT, FALSE };

struct dig_info digging;

dungeon dungeons[MAXDUNGEON];	/* ini'ed by init_dungeon() */
s_level *sp_levchn;
stairway upstair = { 0, 0 }, dnstair = { 0, 0 };
stairway upladder = { 0, 0 }, dnladder = { 0, 0 };
stairway sstairs = { 0, 0 };
dest_area updest = { 0, 0, 0, 0, 0, 0, 0, 0 };
dest_area dndest = { 0, 0, 0, 0, 0, 0, 0, 0 };
coord inv_pos = { 0, 0 };

boolean in_mklev;
boolean stoned;	/* done to monsters hit by 'c' */
boolean unweapon;
boolean mrg_to_wielded;
			 /* weapon picked is merged with wielded one */
struct obj *current_wand;	/* wand currently zapped/applied */

boolean in_steed_dismounting = FALSE;

coord bhitpos;
coord doors[DOORMAX];

struct mkroom rooms[(MAXNROFROOMS+1)*2];
struct mkroom* subrooms = &rooms[MAXNROFROOMS+1];
struct mkroom *upstairs_room, *dnstairs_room, *sstairs_room;

dlevel_t level;		/* level map */
struct trap *ftrap;
struct monst youmonst;
struct flag flags;
struct instance_flags iflags;
struct instance_flags2 iflags2;
struct you u;

struct obj *invent,
	*uwep, *uarm,
	*uswapwep,
	*uquiver, /* quiver */
	*uarmu, /* under-wear, so to speak */
	*uskin, /* dragon armor, if a dragon */
	*uarmc, *uarmh,
	*uarms, *uarmg,
	*uarmf, *uamul,
	*uright,
	*uleft,
	*ublindf,
	*uchain,
	*uball;

const int shield_static[SHIELD_COUNT] = {
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,	/* 7 per row */
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,
    E_ss1, E_ss2, E_ss3, E_ss2, E_ss1, E_ss2, E_ss4,
};

struct spell spl_book[MAXSPELL + 1];

long moves = 1L, monstermoves = 1L;
	 /* These diverge when player is Fast */
long wailmsg;

/* objects that are moving to another dungeon level */
struct obj *migrating_objs;
/* objects not yet paid for */
struct obj *billobjs;

/* used to zero all elements of a struct obj */
struct obj zeroobj;

/* originally from dog.c */
char dogname[PL_PSIZ];
char catname[PL_PSIZ];
char horsename[PL_PSIZ];
char preferred_pet;	/* '\0', 'c', 'd', 'n' (none) */
/* monsters that went down/up together with @ */
struct monst *mydogs = NULL;
/* monsters that are moving to another dungeon level */
struct monst *migrating_mons = NULL;

struct mvitals mvitals[NUMMONS];

struct c_color_names c_color_names = {
	"black", "amber", "golden",
	"light blue", "red", "green",
	"silver", "blue", "purple",
	"white"
};

const char *c_obj_colors[] = {
	"black",		/* CLR_BLACK */
	"red",			/* CLR_RED */
	"green",		/* CLR_GREEN */
	"brown",		/* CLR_BROWN */
	"blue",			/* CLR_BLUE */
	"magenta",		/* CLR_MAGENTA */
	"cyan",			/* CLR_CYAN */
	"gray",			/* CLR_GRAY */
	"transparent",		/* no_color */
	"orange",		/* CLR_ORANGE */
	"bright green",		/* CLR_BRIGHT_GREEN */
	"yellow",		/* CLR_YELLOW */
	"bright blue",		/* CLR_BRIGHT_BLUE */
	"bright magenta",	/* CLR_BRIGHT_MAGENTA */
	"bright cyan",		/* CLR_BRIGHT_CYAN */
	"white",		/* CLR_WHITE */
};

const struct c_common_strings c_common_strings = {
	"Nothing happens.",		"That's enough tries!",
	"That is a silly thing to %s.",	"shudder for a moment.",
	"something", "Something", "You can move again.", "Never mind.",
	"vision quickly clears.", {"the", "your"}
};

/* NOTE: the order of these words exactly corresponds to the
   order of oc_material values #define'd in objclass.h. */
const char *materialnm[] = {
	"mysterious", "liquid", "wax", "organic", "flesh",
	"paper", "cloth", "leather", "wooden", "bone", "dragonhide",
	"iron", "metal", "copper", "silver", "gold", "platinum", "mithril",
	"plastic", "glass", "gemstone", "stone"
};

/* Vision */
boolean vision_full_recalc;
char **viz_array;/* used in cansee() and couldsee() macros */

char *fqn_prefix[PREFIX_COUNT] = { NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL };

const char *fqn_prefix_names[PREFIX_COUNT] = { "hackdir", "leveldir", "savedir",
					"bonesdir", "datadir", "scoredir",
					"lockdir", "troubledir" };

boolean botl;	/* partially redo status line */
boolean botlx;	/* print an entirely new bottom line */


/*decl.c*/
