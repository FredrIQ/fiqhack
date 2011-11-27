/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include <ctype.h>

#define WINTYPELEN 16

static int change_inv_order(char *op);

/* -------------------------------------------------------------------------- */

#define listlen(list) (sizeof(list)/sizeof(struct nh_listitem))

static const struct nh_listitem disclose_list[] = {
	{DISCLOSE_NO_WITHOUT_PROMPT, "no"},
	{DISCLOSE_PROMPT_DEFAULT_NO, "ask, default no"},
	{DISCLOSE_PROMPT_DEFAULT_YES, "ask, default yes"},
	{DISCLOSE_YES_WITHOUT_PROMPT, "yes"}
};
static const struct nh_enum_option disclose_spec = {disclose_list, listlen(disclose_list)};

static const struct nh_listitem menustyle_list[] = {
	{MENU_PARTIAL, "partial"},
	{MENU_FULL, "full"}
};
static const struct nh_enum_option menustyle_spec = {menustyle_list, listlen(menustyle_list)};

static const struct nh_listitem pickup_burden_list[] = {
	{UNENCUMBERED, "unencumbered"},
	{SLT_ENCUMBER, "burdened"},
	{MOD_ENCUMBER, "stressed"},
	{HVY_ENCUMBER, "strained"},
	{EXT_ENCUMBER, "overtaxed"},
	{OVERLOADED, "overloaded"}
};
static const struct nh_enum_option pickup_burden_spec =
			{pickup_burden_list, listlen(pickup_burden_list)};

static const struct nh_listitem runmode_list[] = {
	{RUN_CRAWL, "crawl"},
	{RUN_STEP, "step"},
	{RUN_LEAP, "leap"},
	{RUN_TPORT, "teleport"}
};
static const struct nh_enum_option runmode_spec = {runmode_list, listlen(runmode_list)};

static const struct nh_listitem align_list[] = {
	{0, "lawful"},
	{1, "neutral"},
	{2, "chaotic"},
	{ROLE_NONE, "ask"},
	{ROLE_RANDOM, "random"}
};
static const struct nh_enum_option align_spec = {align_list, listlen(align_list)};

static const struct nh_listitem gender_list[] = {
	{0, "male"},
	{1, "female"},
	{ROLE_NONE, "ask"},
	{ROLE_RANDOM, "random"}
};
static const struct nh_enum_option gender_spec = {gender_list, listlen(gender_list)};

static struct nh_enum_option race_spec = {NULL, 0};
static struct nh_enum_option role_spec = {NULL, 0};

static const struct nh_listitem pettype_list[] = {
	{'c', "cat"},
	{'d', "dog"},
	{'h', "horse"},
	{'n', "no pet"},
	{0, "random"}
};
static const struct nh_enum_option pettype_spec = {pettype_list, listlen(pettype_list)};

#define VTRUE (void*)TRUE
#define VFALSE (void*)FALSE

static const struct nh_option_desc const_options[] = {
    /* boolean options */
    {"autodig",		"dig if moving and wielding digging tool",	OPTTYPE_BOOL, { VFALSE }},
    {"autopickup",	"automatically pick up objects you move over",	OPTTYPE_BOOL, { VTRUE }},
    {"autoquiver",	"when firing with an empty quiver, select something suitable",	OPTTYPE_BOOL, { VFALSE }},
    {"confirm",		"ask before hitting tame or peaceful monsters",	OPTTYPE_BOOL, { VTRUE }},
    {"fixinv",		"try to retain the same letter for the same object",	OPTTYPE_BOOL, { VTRUE }},
    {"help",		"print all available info when using the / command",	OPTTYPE_BOOL, { VTRUE }},
    {"lit_corridor",	"show a dark corridor as lit if in sight",	OPTTYPE_BOOL, { VFALSE }},
    {"lootabc",		"use a/b/c rather than o/i/b when looting",	OPTTYPE_BOOL, { VFALSE }},
    {"pickup_thrown",	"autopickup items you threw or fired",	OPTTYPE_BOOL, { VTRUE }},
    {"prayconfirm",	"use confirmation prompt when #pray command issued",	OPTTYPE_BOOL, { VTRUE }},
    {"pushweapon",	"when wielding a new weapon, put your previous weapon into the secondary weapon slot",	OPTTYPE_BOOL, { VFALSE }},
    {"safe_pet",	"prevent you from (knowingly) attacking your pet(s)",	OPTTYPE_BOOL, { VTRUE }},
    {"showrace",	"show yourself by your race rather than by role",	OPTTYPE_BOOL, { VFALSE }},
    {"sortpack",	"group similar kinds of objects in inventory",	OPTTYPE_BOOL, { VTRUE }},
    {"sparkle",		"display sparkly effect for resisted magical attacks",	OPTTYPE_BOOL, { VTRUE }},
    {"tombstone",	"print tombstone when you die",	OPTTYPE_BOOL, { VTRUE }},
    {"toptenwin",	"print topten in a window rather than stdout",	OPTTYPE_BOOL, { VFALSE }},
    {"verbose",		"print more commentary during the game",	OPTTYPE_BOOL, { VTRUE }},
    
    /* complicated options */
    {"disclose_inventory", "the kinds of information to disclose at end of game", OPTTYPE_ENUM, {(void*)DISCLOSE_PROMPT_DEFAULT_YES}},
    {"disclose_attribs", "disclose your attributes at end of the game", OPTTYPE_ENUM, {(void*)DISCLOSE_PROMPT_DEFAULT_YES}},
    {"disclose_vanquished", "disclose the list of vanquished enemies at end of the game", OPTTYPE_ENUM, {(void*)DISCLOSE_PROMPT_DEFAULT_YES}},
    {"disclose_genocided", "disclose which monsters were genocided at end of the game", OPTTYPE_ENUM, {(void*)DISCLOSE_PROMPT_DEFAULT_YES}},
    {"disclose_conduct", "disclose your conduct at end of the game", OPTTYPE_ENUM, {(void*)DISCLOSE_PROMPT_DEFAULT_YES}},
    {"fruit", "the name of a fruit you enjoy eating", OPTTYPE_STRING, {"slime mold"}},
    {"menustyle", "user interface for object selection", OPTTYPE_ENUM, {(void*)MENU_FULL}},
    {"packorder", "the inventory order of the items in your pack", OPTTYPE_STRING, {"\")[%?+!=/(*`0_"}},
    {"pickup_burden",  "maximum burden picked up before prompt", OPTTYPE_ENUM, {(void*)MOD_ENCUMBER}},
    {"pickup_types", "types of objects to pick up automatically", OPTTYPE_STRING, {NULL}},
    {"runmode", "display frequency when `running' or `travelling'", OPTTYPE_ENUM, {(void*)RUN_LEAP}},
    
    {NULL, NULL, OPTTYPE_BOOL, { NULL }}
};


static const struct nh_option_desc const_birth_options[] = {
    { "elbereth", "difficulty: the E-word repels monsters", OPTTYPE_BOOL, { VTRUE }},
    { "reincarnation", "Special Rogue-like levels", OPTTYPE_BOOL, { VTRUE }},
    { "seduction", "certain monsters may seduce you", OPTTYPE_BOOL, { VTRUE }},
    { "bones", "allow bones levels", OPTTYPE_BOOL, { VTRUE }},
    { "legacy",   "print introductory message", OPTTYPE_BOOL, { VTRUE }},
    { "align",    "your starting alignment", OPTTYPE_ENUM, {(void*)ROLE_NONE}},
    { "gender",   "your starting gender", OPTTYPE_ENUM, {(void*)ROLE_NONE}},
    { "race",     "your starting race", OPTTYPE_ENUM, {(void*)ROLE_NONE}},
    { "role",     "your starting role", OPTTYPE_ENUM, {(void*)ROLE_NONE}},
    { "catname",  "the name of your (first) cat", OPTTYPE_STRING, {NULL}},
    { "dogname",  "the name of your (first) dog", OPTTYPE_STRING, {NULL}},
    { "horsename", "the name of your (first) horse", OPTTYPE_STRING, {NULL}},
    { "pettype",  "your preferred initial pet type", OPTTYPE_ENUM, {0}},
    
    {NULL, NULL, OPTTYPE_BOOL, { NULL }}
};


/* associate boolean options with variables directly */
static const struct nh_boolopt_map boolopt_map[] = {
	{"autodig", &flags.autodig},
	{"autopickup", &flags.pickup},
	{"autoquiver", &flags.autoquiver},
	{"confirm",&flags.confirm},
	{"female", &flags.female},
	{"fixinv", &flags.invlet_constant},
	{"help", &flags.help},
	{"legacy", &flags.legacy},
	{"lit_corridor", &flags.lit_corridor},
	{"lootabc", &iflags.lootabc},
	/* for menu debugging only*/
	{"pickup_thrown", &iflags.pickup_thrown},
	{"prayconfirm", &flags.prayconfirm},
	{"pushweapon", &flags.pushweapon},
	{"safe_pet", &flags.safe_dog},
	{"showrace", &iflags.showrace},
	{"sortpack", &flags.sortpack},
	{"sparkle", &flags.sparkle},
	{"tombstone",&flags.tombstone},
	{"toptenwin",&flags.toptenwin},
	{"verbose", &flags.verbose},

	/* birth options */
	{"elbereth", &flags.elbereth_enabled},
	{"reincarnation", &flags.rogue_enabled},
	{"seduction", &flags.seduce_enabled},
	{"bones", &flags.bones_enabled},
	{NULL, NULL}
};


static const char def_inv_order[MAXOCLASSES] = {
	COIN_CLASS, AMULET_CLASS, WEAPON_CLASS, ARMOR_CLASS, FOOD_CLASS,
	SCROLL_CLASS, SPBOOK_CLASS, POTION_CLASS, RING_CLASS, WAND_CLASS,
	TOOL_CLASS, GEM_CLASS, ROCK_CLASS, BALL_CLASS, CHAIN_CLASS, 0,
};


struct nh_option_desc *ui_options;
struct nh_boolopt_map *ui_boolopt_map;
boolean(*ui_option_callback)(struct nh_option_desc *);

/* most environment variables will eventually be printed in an error
 * message if they don't work, and most error message paths go through
 * BUFSZ buffers, which could be overflowed by a maliciously long
 * environment variable.  if a variable can legitimately be long, or
 * if it's put in a smaller buffer, the responsible code will have to
 * bounds-check itself.
 */
char *nh_getenv(const char *ev)
{
	char *getev = getenv(ev);

	if (getev && strlen(getev) <= (BUFSZ / 2))
		return getev;
	else
		return NULL;
}


static struct nh_option_desc *find_option(struct nh_option_desc *optlist, const char *name)
{
	int i;
	for (i = 0; optlist[i].name; i++)
	    if (!strcmp(name, optlist[i].name))
		return &optlist[i];
	
	return NULL;
}


static void build_role_spec(void)
{
	int i;
	struct nh_listitem *choices;
	
	/* build list of roles */
	for (i = 0; roles[i].name.m || roles[i].name.f; i++)
	    ; /* just count em */
	role_spec.numchoices = i + 2;
	choices = malloc((i+2) * sizeof(struct nh_listitem));
	for (i = 0; roles[i].name.m || roles[i].name.f; i++) {
	    choices[i].id = i;
	    if (roles[i].name.m)
		choices[i].caption = (char*)roles[i].name.m;
	    else
		choices[i].caption = (char*)roles[i].name.f;
	}
	choices[i].id = ROLE_NONE;
	choices[i].caption = "ask";
	choices[i+1].id = ROLE_RANDOM;
	choices[i+1].caption = "random";
	
	role_spec.choices = choices;
}

static void build_race_spec(void)
{
	int i;
	struct nh_listitem *choices;
	
	/* build list of races */
	for (i = 0; races[i].noun; i++)
	    ; /* just count em */
	race_spec.numchoices = i+2;
	choices = malloc((i+2) * sizeof(struct nh_listitem));
	for (i = 0; races[i].noun; i++) {
	    choices[i].id = i;
	    choices[i].caption = (char*)races[i].noun;
	}
	choices[i].id = ROLE_NONE;
	choices[i].caption = "ask";
	choices[i+1].id = ROLE_RANDOM;
	choices[i+1].caption = "random";
	
	race_spec.choices = choices;
}

void init_opt_struct(void)
{
	options = clone_optlist(const_options);
	birth_options = clone_optlist(const_birth_options);
	
	build_role_spec();
	build_race_spec();
	
	/* initialize option definitions */
	find_option(options, "disclose_inventory")->e = disclose_spec;
	find_option(options, "disclose_attribs")->e = disclose_spec;
	find_option(options, "disclose_vanquished")->e = disclose_spec;
	find_option(options, "disclose_genocided")->e = disclose_spec;
	find_option(options, "disclose_conduct")->e = disclose_spec;
	find_option(options, "fruit")->s.maxlen = PL_FSIZ;
	find_option(options, "menustyle")->e = menustyle_spec;
	find_option(options, "pickup_burden")->e = pickup_burden_spec;
	find_option(options, "pickup_types")->s.maxlen = MAXOCLASSES;
	find_option(options, "packorder")->s.maxlen = MAXOCLASSES;
	find_option(options, "runmode")->e = runmode_spec;
	
	find_option(birth_options, "align")->e = align_spec;
	find_option(birth_options, "gender")->e = gender_spec;
	find_option(birth_options, "role")->e = role_spec;
	find_option(birth_options, "race")->e = race_spec;
	find_option(birth_options, "pettype")->e = pettype_spec;
	find_option(birth_options, "catname")->s.maxlen = PL_PSIZ;
	find_option(birth_options, "dogname")->s.maxlen = PL_PSIZ;
	find_option(birth_options, "horsename")->s.maxlen = PL_PSIZ;
}


void cleanup_opt_struct(void)
{
	free((void*)role_spec.choices);
	role_spec.choices = NULL;
	free((void*)race_spec.choices);
	race_spec.choices = NULL;
	free_optlist(options);
	options = NULL;
	free_optlist(birth_options);
	birth_options = NULL;
}


void initoptions(void)
{
	int i;
	
	iflags.travelcc.x = iflags.travelcc.y = -1;
	flags.warnlevel = 1;
	flags.warntype = 0L;
	flags.pickup_types[0] = '\0';
	
	/* init flags.inv_order this way, as setting it via the option
	 * requires a preexisting order */
	memcpy(flags.inv_order, def_inv_order, sizeof flags.inv_order);

	/* since this is done before init_objects(), do partial init here */
	objects[SLIME_MOLD].oc_name_idx = SLIME_MOLD;
	strncpy(pl_fruit, OBJ_NAME(objects[SLIME_MOLD]), PL_FSIZ);
	fruitadd(pl_fruit);
	
	/* init from option definitions */
	for (i = 0; birth_options[i].name; i++)
		nh_set_option(birth_options[i].name, birth_options[i].value, FALSE);
	
	for (i = 0; options[i].name; i++)
		nh_set_option(options[i].name, options[i].value, FALSE);
	
	active_birth_options = clone_optlist(birth_options);
}


static boolean option_value_ok(struct nh_option_desc *option,
			       union nh_optvalue value)
{
	int i;
	
	switch (option->type) {
	    case OPTTYPE_BOOL:
		if (value.b == !!value.b)
		    return TRUE;
		break;
		
	    case OPTTYPE_INT:
		if (value.i >= option->i.min && value.i <= option->i.max)
		    return TRUE;
		break;
		
	    case OPTTYPE_ENUM:
		for (i = 0; i < option->e.numchoices; i++)
		    if (value.e == option->e.choices[i].id)
			return TRUE;
		break;
		
	    case OPTTYPE_STRING:
		if (!value.s)
		    break;
		
		if (strlen(value.s) > option->s.maxlen)
		    break;
		
		if (strlen(value.s) == 0)
		    value.s = NULL;
		
		return TRUE;
	}
	
	return FALSE;
}


static union nh_optvalue string_to_optvalue(struct nh_option_desc *option, char *str)
{
	union nh_optvalue value;
	int i;
    
	switch (option->type) {
	    case OPTTYPE_BOOL:
		if (!strcmp(str, "TRUE") || !strcmp(str, "true") || !strcmp(str, "1"))
		    value.b = TRUE;
		else if  (!strcmp(str, "FALSE") || !strcmp(str, "false") || !strcmp(str, "0"))
		    value.b = FALSE;
		else
		    value.i = 2; /* intentionally invalid */
		
		break;
		
	    case OPTTYPE_INT:
		sscanf(str, "%d", &value.i);
		break;
		
	    case OPTTYPE_ENUM:
		for (i = 0; i < option->e.numchoices; i++)
		    if (!strcmp(str, option->e.choices[i].caption))
			value.e = option->e.choices[i].id;
		break;
		
	    case OPTTYPE_STRING:
		if (strlen(str) > 0)
		    value.s = str;
		else
		    value.s = NULL;
		break;
	}
	
	return value;
}


/* copy values carefully: copying pointers to strings on the stack is not good */
static boolean copy_option_value(struct nh_option_desc *option, union nh_optvalue value)
{
	if (option->type == OPTTYPE_STRING) {
	    if (option->value.s == value.s ||
		(option->value.s && value.s && !strcmp(option->value.s, value.s)))
		return FALSE; /* setting the option to it's current value; nothing to copy */
	    
	    if (option->value.s)
		free(option->value.s);
	    option->value.s = NULL;
	    if (value.s) {
		option->value.s = malloc(strlen(value.s) + 1);
		strcpy(option->value.s, value.s);
	    }
	} else if ((option->type == OPTTYPE_ENUM && option->value.e == value.e) ||
	    (option->type == OPTTYPE_INT && option->value.i == value.i) ||
	    (option->type == OPTTYPE_BOOL && option->value.b == value.b))
	    return FALSE;
	else
	    option->value = value;
	
	return TRUE;
	
}


static boolean set_option(const char *name, union nh_optvalue value, boolean isstring)
{
	boolean is_ui = FALSE;
	struct nh_option_desc *option = NULL;
	
	if (options)
	    option = find_option(options, name);
	
	if (!option && !program_state.game_running && birth_options)
	    option = find_option(birth_options, name);
	
	if (!option && ui_options) {
	    option = find_option(ui_options, name);
	    is_ui = TRUE;
	}
	    
	if (!option)
	    return FALSE;
	
	if (isstring)
	    value = string_to_optvalue(option, value.s);
	
	if (!option_value_ok(option, value))
		return FALSE;
	
	if (copy_option_value(option, value) && !is_ui)
	    log_option(option); /* prev value != new value */
	
	if (option->type == OPTTYPE_BOOL) {
		int i;
		boolean *bvar = NULL;
		const struct nh_boolopt_map *boolmap = boolopt_map;
		if (is_ui)
		    boolmap = ui_boolopt_map;
		
		for (i = 0; boolmap[i].optname && !bvar; i++)
			if (!strcmp(option->name, boolmap[i].optname))
				bvar = boolmap[i].addr;
				
		if (!bvar)
			/* shouldn't happen */
			return FALSE;
		
		*bvar = option->value.b;
		/* allow the ui to "see" changes to booleans, but the return
		 * value doesn't mattter as the option was set here. */
		ui_option_callback(option);
		return TRUE;
	}
	else if (is_ui)
	    return ui_option_callback(option);
	/* regular non-boolean options */
	else if (!strcmp("disclose_inventory", option->name)) {
		flags.end_disclose[0] = option->value.e;
	}
	else if (!strcmp("disclose_attribs", option->name)) {
		flags.end_disclose[1] = option->value.e;
	}
	else if (!strcmp("disclose_genocided", option->name)) {
		flags.end_disclose[2] = option->value.e;
	}
	else if (!strcmp("disclose_vanquished", option->name)) {
		flags.end_disclose[3] = option->value.e;
	}
	else if (!strcmp("disclose_conduct", option->name)) {
		flags.end_disclose[4] = option->value.e;
	}
	else if (!strcmp("fruit", option->name)) {
		strncpy(pl_fruit, option->value.s, PL_FSIZ);
		if (objects) /* don't do fruitadd before the game is running */
		    fruitadd(pl_fruit);
	}
	else if (!strcmp("menustyle", option->name)) {
		flags.menu_style = option->value.e;
	}
	else if (!strcmp("packorder", option->name)) {
		if (!change_inv_order(option->value.s))
			return FALSE;
	}
	else if (!strcmp("pickup_burden", option->name)) {
		flags.pickup_burden = option->value.e;
	}
	else if (!strcmp("pickup_types", option->name)) {
		int num = 0;
		const char *op = option->value.s;
		while (*op) {
		    int oc_sym = def_char_to_objclass(*op);
		    /* make sure all are valid obj symbols occuring once */
		    if (oc_sym != MAXOCLASSES &&
			!strchr(flags.pickup_types, oc_sym)) {
			flags.pickup_types[num] = (char)oc_sym;
			flags.pickup_types[++num] = '\0';
		    } else
			return FALSE;
		    op++;
		}
	}
	else if (!strcmp("runmode", option->name)) {
		iflags.runmode = option->value.e;
	}
	
	/* birth options */
	else if (!strcmp("align", option->name)) {
		flags.initalign = option->value.e;
	}
	else if (!strcmp("gender", option->name)) {
		flags.initgend = option->value.e;
	}
	else if (!strcmp("race", option->name)) {
		flags.initrace = option->value.e;
	}
	else if (!strcmp("role", option->name)) {
		flags.initrole = option->value.e;
	}
	
	else if (!strcmp("catname", option->name)) {
		strncpy(catname, option->value.s, PL_PSIZ);
	}
	else if (!strcmp("dogname", option->name)) {
		strncpy(dogname, option->value.s, PL_PSIZ);
	}
	else if (!strcmp("horsename", option->name)) {
		strncpy(horsename, option->value.s, PL_PSIZ);
	}
	else if (!strcmp("pettype", option->name)) {
		preferred_pet = (char)option->value.e;
	}
	
	else
	    /* unrecognized option */
	    return FALSE;
	
	/* assume that any recognized option has been handled. */
	return TRUE;
}


boolean nh_set_option(const char *name, union nh_optvalue value, boolean isstring)
{
	boolean rv;
	
	if (!api_entry_checkpoint())
	    return FALSE;
	
	rv = set_option(name, value, isstring);
	
	api_exit();
	return rv;
}


struct nh_option_desc *nh_get_options(enum nh_option_list list)
{
	switch (list) {
	    case CURRENT_BIRTH_OPTIONS:
		return birth_options;
	    case ACTIVE_BIRTH_OPTIONS:
		return active_birth_options;
	    case GAME_OPTIONS:
		return options;
	}
	return NULL;
}


struct nh_option_desc *clone_optlist(const struct nh_option_desc *in)
{
	int i;
	struct nh_option_desc *out;
	
	for (i = 0; in[i].name; i++)
	    ;
	i++;
	out = malloc(sizeof(struct nh_option_desc) * i);
	memcpy(out, in, sizeof(struct nh_option_desc) * i);
	
	for (i = 0; in[i].name; i++)
	    if (in[i].type == OPTTYPE_STRING && in[i].value.s)
		out[i].value.s = strdup(in[i].value.s);
	
	return out;
}


void free_optlist(struct nh_option_desc *opt)
{
	int i;
	
	if (!opt)
	    return;
	
	for (i = 0; opt[i].name; i++)
	    if (opt[i].type == OPTTYPE_STRING && opt[i].value.s)
		free(opt[i].value.s);
	
	free(opt);
}


/* Returns the fid of the fruit type; if that type already exists, it
 * returns the fid of that one; if it does not exist, it adds a new fruit
 * type to the chain and returns the new one.
 */
int fruitadd(char *str)
{
	int i;
	struct fruit *f;
	struct fruit *lastf = 0;
	int highest_fruit_id = 0;
	char buf[PL_FSIZ];
	boolean user_specified = (str == pl_fruit);
	/* if not user-specified, then it's a fruit name for a fruit on
	 * a bones level...
	 */

	/* Note: every fruit has an id (spe for fruit objects) of at least
	 * 1; 0 is an error.
	 */
	if (user_specified) {
		/* disallow naming after other foods (since it'd be impossible
		 * to tell the difference)
		 */

		boolean found = FALSE, numeric = FALSE;

		for (i = bases[FOOD_CLASS]; objects[i].oc_class == FOOD_CLASS;
						i++) {
			if (!strcmp(OBJ_NAME(objects[i]), pl_fruit)) {
				found = TRUE;
				break;
			}
		}
		{
		    char *c;

		    for (c = pl_fruit; *c >= '0' && *c <= '9'; c++)
			;
		    if (isspace(*c) || *c == 0) numeric = TRUE;
		}
		if (found || numeric ||
		    !strncmp(str, "cursed ", 7) ||
		    !strncmp(str, "uncursed ", 9) ||
		    !strncmp(str, "blessed ", 8) ||
		    !strncmp(str, "partly eaten ", 13) ||
		    (!strncmp(str, "tin of ", 7) &&
		        (!strcmp(str+7, "spinach") ||
			 name_to_mon(str+7) >= LOW_PM)) ||
		    !strcmp(str, "empty tin") ||
		    ((!strncmp(eos(str)-7," corpse",7) ||
			    !strncmp(eos(str)-4, " egg",4)) &&
			name_to_mon(str) >= LOW_PM))
			{
			    strcpy(buf, pl_fruit);
			    strcpy(pl_fruit, "candied ");
			    strncat(pl_fruit+8, buf, PL_FSIZ-8);
			}
	}
	for (f=ffruit; f; f = f->nextf) {
		lastf = f;
		if (f->fid > highest_fruit_id) highest_fruit_id = f->fid;
		if (!strncmp(str, f->fname, PL_FSIZ))
			goto nonew;
	}
	/* if adding another fruit would overflow spe, use a random
	   fruit instead... we've got a lot to choose from. */
	if (highest_fruit_id >= 127) return rnd(127);
	highest_fruit_id++;
	f = newfruit();
	memset(f, 0, sizeof(struct fruit));
	if (ffruit) lastf->nextf = f;
	else ffruit = f;
	strcpy(f->fname, str);
	f->fid = highest_fruit_id;
	f->nextf = 0;
nonew:
	if (user_specified) current_fruit = highest_fruit_id;
	return f->fid;
}


/*
 * Convert the given string of object classes to a string of default object
 * symbols.
 */
static void oc_to_str(char *src, char *dest)
{
    int i;

    while ((i = (int) *src++) != 0) {
	if (i < 0 || i >= MAXOCLASSES)
	    impossible("oc_to_str:  illegal object class %d", i);
	else
	    *dest++ = def_oc_syms[i];
    }
    *dest = '\0';
}


int dotogglepickup(void)
{
	char buf[BUFSZ], ocl[MAXOCLASSES+1];

	flags.pickup = !flags.pickup;
	if (flags.pickup) {
	    oc_to_str(flags.pickup_types, ocl);
	    sprintf(buf, "ON, for %s objects%s", ocl[0] ? ocl : "all",
#ifdef AUTOPICKUP_EXCEPTIONS
			(iflags.autopickup_exceptions[AP_LEAVE] ||
			 iflags.autopickup_exceptions[AP_GRAB]) ?
			 ((count_ape_maps(NULL, NULL) == 1) ?
			    ", with one exception" : ", with some exceptions") :
#endif
			"");
	} else {
	    strcpy(buf, "OFF");
	}
	pline("Autopickup: %s.", buf);
	return 0;
}


static int change_inv_order(char *op)
{
    int oc_sym, num;
    char *sp, buf[BUFSZ];

    num = 0;
#ifndef GOLDOBJ
    if (!strchr(op, GOLD_SYM))
	buf[num++] = COIN_CLASS;
#else
    /*  !!!! probably unnecessary with gold as normal inventory */
#endif

    for (sp = op; *sp; sp++) {
	oc_sym = def_char_to_objclass(*sp);
	/* reject bad or duplicate entries */
	if (oc_sym == MAXOCLASSES ||
		oc_sym == RANDOM_CLASS || oc_sym == ILLOBJ_CLASS ||
		!strchr(flags.inv_order, oc_sym) || strchr(sp+1, *sp))
	    return 0;
	/* retain good ones */
	buf[num++] = (char) oc_sym;
    }
    buf[num] = '\0';

    /* fill in any omitted classes, using previous ordering */
    for (sp = flags.inv_order; *sp; sp++)
	if (!strchr(buf, *sp)) {
	    buf[num++] = *sp;
	    buf[num] = '\0';	/* explicitly terminate for next strchr() */
	}

    strcpy(flags.inv_order, buf);
    return 1;
}


/* convenience function: allows the ui to share option handling code */
void nh_setup_ui_options(struct nh_option_desc *uioptions,
			 struct nh_boolopt_map *boolmap,
			 boolean(*callback)(struct nh_option_desc *))
{
    ui_options = uioptions;
    ui_boolopt_map = boolmap;
    ui_option_callback = callback;
}

/*options.c*/
