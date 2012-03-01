/* Copyright (c) Daniel Thaler, 2011.                             */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"
#include "patchlevel.h"
#include <ctype.h>

extern int logfile;
extern unsigned int last_cmd_pos;


static void replay_pause(enum nh_pause_reason r) {}
static void replay_display_buffer(const char *buf, boolean trymove) {}
static void replay_update_status(struct nh_player_info *pi) {}
static void replay_print_message(int turn, const char *msg) {}
static void replay_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO], int ux, int uy) {}
static void replay_delay_output(void) {}
static void replay_level_changed(int displaymode) {}
static void replay_outrip(struct nh_menuitem *items,int icount, boolean tombstone,
	const char *name, int gold, const char *killbuf, int end_how, int year) {}
static int replay_display_menu(struct nh_menuitem *items, int icount,
				const char *title, int how, int *results);
static int replay_display_objects(struct nh_objitem *items, int icount, const char *title,
			int how, struct nh_objresult *pick_list);
static char replay_query_key(const char *query, int *count);
static int replay_getpos(int *x, int *y, boolean force, const char *goal);
static enum nh_direction replay_getdir(const char *query, boolean restricted);
static char replay_yn_function(const char *query, const char *rset, char defchoice);
static void replay_getlin(const char *query, char *buf);


static struct loginfo {
    char *mem;
    char **tokens;
    int tokencount;
    int next;
    int cmdcount; /* number of tokens that look like commands ">..." */
} loginfo;


struct replay_checkpoint {
    int actions, moves, nexttoken;
    struct nh_option_desc *opt; /* option state at the time of the checkpoint */
    struct memfile cpdata; /* binary save data */
};

static struct replay_checkpoint *checkpoints;
static char **commands;
static int cmdcount, cpcount;
static struct nh_option_desc *saved_options;
static struct nh_window_procs replay_windowprocs, orig_windowprocs;

/* base 64 decoding table */
static const char b64d[256] = {
    /* 32 control chars */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* ' ' - '/' */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 
    /* '0' - '9' */ 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    /* ':' - '@' */ 0, 0, 0, 0, 0, 0, 0,
    /* 'A' - 'Z' */ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                    17, 18, 19, 20, 21, 22, 23, 24, 25,
    /* '[' - '\''*/ 0, 0, 0, 0, 0, 0,
    /* 'a' - 'z' */ 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};


static const struct nh_window_procs def_replay_windowprocs = {
    replay_pause,
    replay_display_buffer,
    replay_update_status,
    replay_print_message,
    replay_display_menu,
    replay_display_objects,
    NULL, /* no function required for list_items */
    replay_update_screen,
    NULL, /* always use a given raw_print */
    replay_query_key,
    replay_getpos,
    replay_getdir,
    replay_yn_function,
    replay_getlin,
    replay_delay_output,
    replay_level_changed,
    replay_outrip,
};


static void base64_decode(const char* in, char *out)
{
    int i, len = strlen(in), pos = 0;
    
    for (i = 0; i < len; i += 4) {
	/* decode blocks; padding '=' are converted to 0 in the decoding table */
	out[pos  ] =   b64d[(int)in[i  ]] << 2          | b64d[(int)in[i+1]] >> 4;
	out[pos+1] =   b64d[(int)in[i+1]] << 4          | b64d[(int)in[i+2]] >> 2;
	out[pos+2] = ((b64d[(int)in[i+2]] << 6) & 0xc0) | b64d[(int)in[i+3]];
	pos += 3;
    }
    
    out[pos] = 0;
}


void replay_set_logfile(int logfd)
{
    if (!program_state.restoring && logfile)
	log_finish(LS_IN_PROGRESS);
    
    if (!lock_fd(logfd, 1)) {
	raw_printf("The game log is locked by another NitroHack process. Aborting.");
	terminate();
    }
    logfile = logfd;
}


void replay_begin(void)
{
    int filesize;
    int i, warned, nr_tokens;
    char *nexttoken;
    unsigned int endpos;
    boolean recovery = FALSE;
    
    if (loginfo.mem) {
	free(loginfo.mem);
	free(loginfo.tokens);
    }
    
    lseek(logfile, 0, SEEK_SET);
    loginfo.mem = loadfile(logfile, &filesize);
    if (filesize < 24 || !loginfo.mem ||
	!sscanf(loginfo.mem, "NHGAME %*s %x", &endpos) || endpos > filesize) {
	free(loginfo.mem);
	loginfo.mem = NULL;
	terminate();
    }
    
    if (!endpos) {
	endpos = filesize;
	recovery = TRUE;
    }
    loginfo.mem[endpos] = '\0';
    
    warned = 0;
    for (i = 0; i < endpos; i++)
	if (iscntrl(loginfo.mem[i]) && loginfo.mem[i] != '\n') {
	    if (recovery) {
		endpos = i;
		loginfo.mem[i] = '\0';
		break;
	    } else {
		loginfo.mem[i] = ' ';
		if (!warned++)
		    raw_print("Warning: found control characters in textual log section");
	    }
	}
    
    /* split the logfile into tokens */
    nr_tokens = 1024;
    loginfo.tokens = malloc(nr_tokens * sizeof(char*));
    loginfo.next = loginfo.cmdcount = 0;
    
    nexttoken = strtok(loginfo.mem, " \r\n");
    while (nexttoken) {
	loginfo.tokens[loginfo.next++] = nexttoken;
	if (nexttoken[0] == '>')
	    loginfo.cmdcount++;
	
	nexttoken = strtok(NULL, " \r\n");
	if (loginfo.next >= nr_tokens) {
	    nr_tokens *= 2;
	    loginfo.tokens = realloc(loginfo.tokens, nr_tokens * sizeof(char*));
	}
    }
    
    loginfo.tokencount = loginfo.next;
    loginfo.next = 0;
    
    if (recovery) {
	/* the last token should always be a command status, eg "<rngstate" */
	while (loginfo.tokencount > 0 &&
	    loginfo.tokens[loginfo.tokencount-1][0] != '<' &&
	    loginfo.tokens[loginfo.tokencount-1][0] != '!') {
	    endpos = (long)loginfo.tokens[loginfo.tokencount-1] - (long)loginfo.mem;
	    loginfo.tokencount--;
	}
    }
    
    last_cmd_pos = endpos;
    lseek(logfile, endpos, SEEK_SET);
    
    /* the log contains options that need to be set while the log is being replayed.
     * However, the current option state is the most recent state set by the user
     * and should be preserved */
    saved_options = clone_optlist(options);
}


void replay_end(void)
{
    int i;
    long tz_off;
    if (!loginfo.mem)
	return;
    
    free(loginfo.mem);
    free(loginfo.tokens);
    memset(&loginfo, 0, sizeof(loginfo));
    
    tz_off = get_tz_offset();
    if (tz_off != replay_timezone)
	log_timezone(tz_off);

    /* restore saved options */
    for (i = 0; saved_options[i].name; i++)
	nh_set_option(saved_options[i].name, saved_options[i].value, FALSE);
    free_optlist(saved_options);
    
    if (!commands)
	return;
    
    for (i = 1; i < cmdcount; i++)
	free(commands[i]);
    free(commands);
    commands = NULL;
}


static void NORETURN parse_error(const char *str)
{
    raw_printf("Error at token %d (\"%s\"): %s\n", loginfo.next,
	       loginfo.tokens[loginfo.next-1], str);
    terminate();
}


static char *next_log_token(void)
{
    if (!loginfo.tokens || loginfo.next == loginfo.tokencount)
	return NULL;
    
    return loginfo.tokens[loginfo.next++];
}


static int replay_display_menu(struct nh_menuitem *items, int icount,
				const char *title, int how, int *results)
{
    int i, j, val;
    char *token, *tab;
    char *resultbuf;
    boolean id_ok;
    
    if (how == PICK_NONE)
	return 0;
    
    token = next_log_token();
    if (!token || token[0] != 'm')
	parse_error("Bad menu data");
    resultbuf = token + 2;
    
    if (*resultbuf++ == 'x')
	return -1;
    
    i = 0;
    while (sscanf(resultbuf, "%x:", &val)) {
	/* make sure all ids are valid - program changes could have broken the save */
	id_ok  = FALSE;
	for (j = 0; j < icount && !id_ok; j++)
	    if (items[j].id == val)
		id_ok = TRUE;
	if (!id_ok)
	    parse_error("Invalid menu id in menu data");
	
	results[i++] = val;
	resultbuf = strchr(resultbuf, ':') + 1;
    }
    
    if (program_state.viewing && how != PICK_NONE) {
	char buf[BUFSZ] = "(none selected)";
	if (i == 1) {
	    for (j = 0; j < icount && items[j].id != results[0]; j++);
	    strcpy(buf, items[j].caption);
	    if ( (tab = strchr(buf, '\t')) )
		*tab = '\0';
	} else
	    sprintf(buf, "(%d selected)", i);
	pline("<%s: %s>", title ? title : "List of items", buf);
    }
    
    return i;
}


static int replay_display_objects(struct nh_objitem *items, int icount, const char *title,
			int how, struct nh_objresult *pick_list)
{
    int i, j, id, count;
    char *token;
    char *resultbuf;
    boolean id_ok;
    
    if (how == PICK_NONE)
	return 0;
    
    token = next_log_token();
    if (!token || token[0] != 'o')
	parse_error("Bad object menu data");
    resultbuf = token + 2;
    
    if (*resultbuf++ == 'x')
	return -1;
    
    i = 0;
    count = -1;
    while (sscanf(resultbuf, "%x,%x:", &id, &count)) {
	/* make sure all ids are valid - program changes could have broken the save */
	id_ok  = FALSE;
	for (j = 0; j < icount && !id_ok; j++)
	    if (items[j].id == id)
		id_ok = TRUE;
	if (!id_ok)
	    parse_error("Invalid menu id in object menu data");
	
	pick_list[i].id = id;
	pick_list[i].count = count;
	i++;
	resultbuf = strchr(resultbuf, ':') + 1;
	count = -1;
    }
    
    if (program_state.viewing && how != PICK_NONE) {
	char buf[BUFSZ] = "(none selected)";
	if (i == 1) {
	    for (j = 0; j < icount && items[j].id != pick_list[0].id; j++);
	    strcpy(buf, items[j].caption);
	} else
	    sprintf(buf, "(%d selected)", i);
	pline("<%s: %s>", title ? title : "List of items", buf);
    }
    
    return i;
}


static char replay_query_key(const char *query, int *count)
{
    int key;
    char *token = next_log_token();
    int cnt = -1, n;
    
    n = sscanf(token, "k:%x:%x", &key, &cnt);
    if (n < 1)
	parse_error("Bad query_key data");
    
    if (count)
	*count = cnt;
    
    if (program_state.viewing) {
	char buf[BUFSZ] = "";
	if (count && cnt != -1) sprintf(buf, "%d ", cnt);
	pline("<%s: %s%c>", query, buf, key);
    }
    return key;
}


static int replay_getpos(int *x, int *y, boolean force, const char *goal)
{
    int ret, n;
    char *token = next_log_token();
    
    n = sscanf(token, "p:%d:%x:%x", &ret, x, y);
    if (n != 3)
	parse_error("Bad getpos data");
    
    if (program_state.viewing)
	pline("<get pos: (%d, %d)>", *x, *y);
    return ret;
}


static enum nh_direction replay_getdir(const char *query, boolean restricted)
{
    const char *const dirnames[] = {"no direction", "west", "nortwest", "north", "northeast",
	"east", "southeast", "south", "southwest", "up", "down", "self"};
    enum nh_direction dir;
    char *token = next_log_token();
    
    int n = sscanf(token, "d:%d", &dir);
    if (n != 1)
	parse_error("Bad getdir data");
    
    if (program_state.viewing)
	pline("<%s: %s>", query ? query : "What direction?", dirnames[dir+1]);
    
    return dir;
}


static char replay_yn_function(const char *query, const char *rset, char defchoice)
{
    int key;
    char *token = next_log_token();
    
    int n = sscanf(token, "y:%x", &key);
    if (n != 1)
	parse_error("Bad yn_function data");

    if (program_state.viewing)
	pline("<%s [%s]: %c>", query, rset, key);
    return key;
}


static void replay_getlin(const char *query, char *buf)
{
    char *encdata, *token = next_log_token();
    
    encdata = strchr(token, ':');
    if (token[0] != 'l' || !encdata)
	parse_error("Bad getlin data");
    
    if (strlen(encdata) > EQBUFSZ)
	parse_error("Encoded getlin string is too long to decode into the target buffer.");
    
    base64_decode(encdata+1, buf);
    if (program_state.viewing)
	pline("<%s: %s>", query, buf[0] == '\033' ? "ESC" : buf);
}


char *replay_bones(int *buflen)
{
    char *b64data, *token = next_log_token();
    char *buf = NULL;
    
    if (!token) /* end of replay data reached */
	return NULL;
    
    if (strncmp(token, "b:", 2) != 0) {
	loginfo.next--; /* no bones to load */
	return NULL;
    }
    
    b64data = token+2;
    *buflen = strlen(b64data);
    buf = malloc(*buflen);
    memset(buf, 0, *buflen);
    
    base64_decode(b64data, buf);
    
    return buf;
}


void replay_setup_windowprocs(const struct nh_window_procs *procs)
{
    void (*win_raw_print)(const char *str);
    
    orig_windowprocs = windowprocs;
    
    if (!procs) {
	win_raw_print = windowprocs.win_raw_print;
	windowprocs = def_replay_windowprocs;
	windowprocs.win_raw_print = win_raw_print;
	return;
    }
    
    windowprocs = *procs;
    windowprocs.win_query_key = replay_query_key;
    windowprocs.win_yn_function = replay_yn_function;
    windowprocs.win_getlin = replay_getlin;
    windowprocs.win_getpos = replay_getpos;
    windowprocs.win_getdir = replay_getdir;
    windowprocs.win_display_menu = replay_display_menu;
    windowprocs.win_display_objects = replay_display_objects;
    windowprocs.win_pause = replay_pause;
    windowprocs.win_delay = replay_delay_output;
    
    replay_windowprocs = windowprocs;
}


void replay_restore_windowprocs(void)
{
    if (orig_windowprocs.win_raw_print) /* test if orig_windowprocs is inited */
	windowprocs = orig_windowprocs;
}


static void replay_read_commandlist(void)
{
    int i;
    char decbuf[BUFSZ], *token;
    
    cmdcount = strtol(next_log_token(), NULL, 16);
    if (!cmdcount)
	parse_error("expected number of commands");
    cmdcount++; /* the NULL command is not in the list */
    commands = malloc(cmdcount * sizeof(char*));
    commands[0] = NULL;
    
    for (i = 1; i < cmdcount; i++) {
	token = next_log_token();
	if (strlen(token) > ENCBUFSZ)
	    parse_error("Encoded command name is too long for the decode buffer");
	base64_decode(token, decbuf);
	commands[i] = strdup(decbuf);
    }
}


void replay_read_newgame(unsigned long long *init, int *playmode, char *namebuf,
			 int *initrole, int *initrace, int *initgend, int *initalign)
{
    char *header, *verstr;
    int ver1, ver2, ver3, n;
    unsigned int seed;
    
    header = next_log_token();
    if (!header || strcmp(header, "NHGAME"))
	parse_error("This file does not look like a NitroHack logfile.");
    
    next_log_token(); /* marker */
    next_log_token(); /* end pos, see replay_begin() */
    next_log_token(); /* game name */
    verstr = next_log_token();
    
    n = sscanf(verstr, "%d.%d.%d", &ver1, &ver2, &ver3);
    if (n != 3)
	parse_error("No version found where it was expected");
    
    if (ver1 != VERSION_MAJOR && ver2 != VERSION_MINOR)
	raw_printf("Warning: Version mismatch; expected %d.%d, got %d.%d\n",
		   VERSION_MAJOR, VERSION_MINOR, ver1, ver2);

    sscanf(next_log_token(), "%llx", init);
    sscanf(next_log_token(), "%x", &seed);
    *playmode = atoi(next_log_token());
    base64_decode(next_log_token(), namebuf);
    *initrole = str2role(next_log_token());
    *initrace = str2race(next_log_token());
    *initgend = str2gend(next_log_token());
    *initalign = str2align(next_log_token());
    
    if (*initrole == ROLE_NONE || *initrace == ROLE_NONE ||
	*initgend == ROLE_NONE || *initalign == ROLE_NONE)
	terminate();
    
    mt_srand(seed);
    
    replay_read_commandlist();
}


static void replay_read_timezone(char *token)
{
    int n;
    
    n = sscanf(token, "TZ%d", &replay_timezone);
    if (n != 1)
	parse_error("Bad timezone offset data.");
}


static void replay_read_option(char *token)
{
    char *name, *otype, *valstr, *arbuf, optname[BUFSZ], valbuf[BUFSZ];
    union nh_optvalue value;
    
    name = token + 1;
    otype = strchr(name, ':');
    if (!otype)
	terminate();
    *otype++ = '\0';
    valstr = strchr(otype, ':');
    if (!valstr)
	terminate();
    *valstr++ = '\0';
    
    base64_decode(name, optname);
    
    switch (otype[0]) {
	case 's':
	    base64_decode(valstr, valbuf);
	    value.s = (valbuf[0] != '\0') ? valbuf : NULL;
	    break;
	case 'e':
	    sscanf(valstr, "%x", &value.e);
	    break;
	case 'i':
	    sscanf(valstr, "%d", &value.i);
	    break;
	case 'b':
	    value.b = atoi(valstr);
	    break;
	case 'a':
	    arbuf = malloc(strlen(valstr) + 1);
	    base64_decode(valstr, arbuf);
	    value.ar = parse_autopickup_rules(arbuf);
	    free(arbuf);
	    break;
	    
	default:
	    parse_error("Unrecognized option type");
    }
    
    nh_set_option(optname, value, FALSE);
}


static boolean replay_parse_arg(char *argstr, struct nh_cmd_arg *arg)
{
    int n;
    
    if (!argstr || !arg)
	return FALSE;
    
    switch (argstr[0]) {
	case 'n':
	    arg->argtype = CMD_ARG_NONE;
	    return TRUE;
	    
	case 'd':
	    arg->argtype = CMD_ARG_DIR;
	    n = sscanf(argstr, "d:%d", &arg->d);
	    return n == 1;
	    
	case 'p':
	    arg->argtype = CMD_ARG_POS;
	    n = sscanf(argstr, "p:%hx:%hx", &arg->pos.x, &arg->pos.y);
	    return n == 2;

	case 'o':
	    arg->argtype = CMD_ARG_OBJ;
	    n = sscanf(argstr, "o:%c", &arg->invlet);
	    return n == 1;
	    
	default:
	    raw_printf("Error: unrecognized arg type %c\n", argstr[0]);
	    return FALSE;
    }
}


static void replay_read_command(char *cmdtok, char **cmd, int *count,
				    struct nh_cmd_arg *arg)
{
    int cmdidx, n;
    if (!cmdtok)
	return;
    
    n = sscanf(cmdtok, ">%llx:%x:%d", &turntime, &cmdidx, count);
    if (n != 3 || cmdidx > cmdcount)
	parse_error("Error: Incorrect command spec\n");
    
    *cmd = commands[cmdidx];
    
    if (!replay_parse_arg(next_log_token(), arg))
	parse_error("Bad command argument");
}


static void replay_check_cmdresult(char *token)
{
    int n;
    unsigned int rngstate;
    if (!token)
	return;
    
    n = sscanf(token, "<%x", &rngstate);
    if (n != 1)
	parse_error("Error: incorrect command result specification\n");
    
    if (rngstate != (mt_nextstate() & 0xffff))
	parse_error("Error: RNG state deviation detected.");
}


boolean replay_run_cmdloop(boolean optonly, boolean singlestep)
{
    char *cmd, *token;
    int count, cmdidx;
    struct nh_cmd_arg cmdarg;
    struct nh_option_desc *tmp;
    boolean did_action = FALSE;
    
    /* the log contains the birth options that are required for this game,
     * so nh_set_option calls during the replay must change active_birth_options */
    tmp = birth_options;
    birth_options = active_birth_options;
    active_birth_options = tmp;
    
    token = next_log_token();
    
    while (token) {
	switch (token[0]) {
	    case '!': /* Option */
		replay_read_option(token);
		break;
		
	    case 'T': /* timezone offset */
		replay_read_timezone(token);
		break;
		
	    case '>': /* command */
		if (!optonly) {
		    replay_read_command(token, &cmd, &count, &cmdarg);
		    cmdidx = get_command_idx(cmd);
		    command_input(cmdidx, count, &cmdarg);
		    did_action = TRUE;
		}
		
		if (singlestep) {
		    if (optonly)
			loginfo.next--; /* put the token back for later use */
		    goto out;
		}
		    
		break;
		
	    case '<': /* a command result */
		if (!optonly)
		    replay_check_cmdresult(token);
		break;
	}
	
	token = next_log_token();
    }

out:
    /* return the birth options to normal */
    tmp = birth_options;
    birth_options = active_birth_options;
    active_birth_options = tmp;
    
    return did_action;
}


static void make_checkpoint(int actions)
{
    /* only make a checkpoint if enough actions have happened since the last
     * one and creating a checkpoint is safe */
    if ((cpcount > 0 && (actions <= checkpoints[cpcount-1].actions + 1000 ||
	                   moves <= checkpoints[cpcount-1].moves)) ||
	multi || occupation) /* checkpointing while something is in progress doesn't work */
	return;
    
    cpcount++;
    checkpoints = realloc(checkpoints, sizeof(struct replay_checkpoint) * cpcount);
    checkpoints[cpcount-1].actions = actions;
    checkpoints[cpcount-1].moves = moves;
    checkpoints[cpcount-1].nexttoken = loginfo.next;
    /* the active option list must be saved: it is not part of the normal binary save */
    checkpoints[cpcount-1].opt = clone_optlist(options);
    memset(&checkpoints[cpcount-1].cpdata, 0, sizeof(struct memfile));
    savegame(&checkpoints[cpcount-1].cpdata);
    checkpoints[cpcount-1].cpdata.len = checkpoints[cpcount-1].cpdata.pos;
    checkpoints[cpcount-1].cpdata.pos = 0;
}


static int load_checkpoint(int idx)
{
    int playmode, i, irole, irace, igend, ialign;
    char namebuf[BUFSZ];
    
    if (idx < 0 || idx >= cpcount)
	return -1;
    
    replay_end();
    freedynamicdata();
    
    replay_begin();/* tokens get mangled during replay, so a new token list is needed */
    replay_read_newgame(&turntime, &playmode, namebuf, &irole, &irace, &igend, &ialign);
    loginfo.next = checkpoints[idx].nexttoken;
    
    program_state.restoring = TRUE;
    startup_common(namebuf, playmode);
    dorecover(&checkpoints[idx].cpdata);
    checkpoints[idx].cpdata.pos = 0;
    
    iflags.disable_log = TRUE;
    program_state.viewing = TRUE;
    program_state.game_running = TRUE;
    
    /* restore the full option state of the time of the checkpoint */
    for (i = 0; checkpoints[idx].opt[i].name; i++)
	nh_set_option(checkpoints[idx].opt[i].name, checkpoints[idx].opt[i].value, FALSE);
    
    return checkpoints[idx].actions;
}


static void free_checkpoints(void)
{
    int i;
    
    for (i = 0; i < cpcount; i++) {
	free_optlist(checkpoints[i].opt);
	free(checkpoints[i].cpdata.buf);
    }
    free(checkpoints);
    checkpoints = NULL;
    cpcount = 0;
}


static boolean find_next_command(char *buf, int buflen)
{
    int i, n, cmdidx, count;
    long dummy;
    const char *cmdname;
    
    buf[0] = '\0';
    for (i = loginfo.next; i < loginfo.tokencount; i++)
	if (*loginfo.tokens[i] == '>')
	    break;
	
    if (i == loginfo.tokencount)
	return FALSE;
    
    n = sscanf(loginfo.tokens[i], ">%lx:%x:%d", &dummy, &cmdidx, &count);
    if (n != 3)
	return FALSE;
    
    cmdname = commands[cmdidx] ? commands[cmdidx] : "<continue>";
    strncpy(buf, cmdname, buflen);
    return TRUE;
}


boolean nh_view_replay_start(int fd, struct nh_window_procs *rwinprocs,
			     struct nh_replay_info *info)
{
    int playmode;
    char namebuf[PL_NSIZ];
    struct nh_game_info gi;
        
    if (!api_entry_checkpoint())
	return FALSE;
    
    memset(info, 0, sizeof(struct nh_replay_info));
    if (logfile != -1 || nh_get_savegame_status(fd, &gi) == LS_INVALID) {
	api_exit();
	return FALSE;
    }
    
    program_state.restoring = TRUE;
    iflags.disable_log = TRUE;
    logfile = fd;
    replay_begin();
    replay_read_newgame(&turntime, &playmode, namebuf, &u.initrole, &u.initrace,
			&u.initgend, &u.initalign);
    replay_setup_windowprocs(rwinprocs);
    
    initoptions();
    replay_run_cmdloop(TRUE, TRUE); /* (re)set options */
    nh_start_game(fd, namebuf, u.initrole, u.initrace, u.initgend, u.initalign, playmode);
    program_state.restoring = FALSE;
    program_state.viewing = TRUE;
    replay_restore_windowprocs();
    
    /* the win_update_screen proc in the replay_windowprocs does nothing, so
     * flush (again) after switching back to regular window procs */
    flush_screen();
    
    info->max_moves = gi.moves;
    info->max_actions = loginfo.cmdcount;
    find_next_command(info->nextcmd, sizeof(info->nextcmd));
    update_inventory();
    make_checkpoint(0);
    
    api_exit();
    
    return TRUE;
}


boolean nh_view_replay_step(struct nh_replay_info *info,
					  enum replay_control action, int count)
{
    boolean did_action;
    int i, prev_actions, target;
    
    if (!program_state.viewing || !api_entry_checkpoint()) {
	info->actions++;
	did_action = TRUE;
	goto out2;
    }
    
    program_state.restoring = TRUE;
    replay_setup_windowprocs(&replay_windowprocs);
    info->moves = moves;
    switch (action) {
	case REPLAY_BACKWARD:
	    prev_actions = info->actions;
	    target = prev_actions - count;
	    for (i = 0; i < cpcount-1; i++)
		if (checkpoints[i+1].actions >= target)
		    break;
	
	    /* rewind the entire game state to the checkpoint */
	    info->actions = load_checkpoint(i);
	    count = target - info->actions;
	    if (count == 0) {
		did_action = TRUE;
		goto out;
	    }
	    /* else fall through */
	    
	case REPLAY_FORWARD:
	    did_action = info->actions < info->max_actions;
	    i = 0;
	    while (i < count && did_action) {
		i++;
		did_action = replay_run_cmdloop(FALSE, TRUE);
		if (did_action) {
		    info->actions++;
		    make_checkpoint(info->actions);
		}
	    }
	    break;
	    
	case REPLAY_GOTO:
	    target = count;
	    if (target < moves) {
		for (i = 0; i < cpcount-1; i++)
		    if (checkpoints[i+1].moves >= target)
			break;
		/* rewind the entire game state to the checkpoint */
		info->actions = load_checkpoint(i);
	    }
	    
	    did_action = info->actions < info->max_actions;
	    while (moves < count && did_action) {
		did_action = replay_run_cmdloop(FALSE, TRUE);
		if (did_action) {
		    info->actions++;
		    make_checkpoint(info->actions);
		}
	    }
	    did_action = moves == count;
	    break;
    }
    
out:
    api_exit();
out2:
    program_state.restoring = FALSE;
    info->moves = moves;
    find_next_command(info->nextcmd, sizeof(info->nextcmd));
    replay_restore_windowprocs();
    flush_screen(); /* must happen after replay_restore_windowprocs to ensure output */
    update_inventory();
    
    /* if we're going backwards, the timestamp on this message
     * will let the ui know it should erase messages in the future */
    print_message(moves, "");
    
    return did_action;
}


void nh_view_replay_finish(void)
{
    replay_restore_windowprocs();
    replay_end();
    program_state.viewing = FALSE;
    program_state.game_running = FALSE;
    freedynamicdata();
    free_checkpoints();
    logfile = -1;
    iflags.disable_log = FALSE;
}


enum nh_log_status nh_get_savegame_status(int fd, struct nh_game_info *gi)
{
    char header[128], status[8], encplname[PL_NSIZ * 2];
    char role[PLRBUFSZ], race[PLRBUFSZ], gend[PLRBUFSZ], algn[PLRBUFSZ];
    int n, v1, v2, v3;
    unsigned int savepos, endpos, seed, playmode;
    struct memfile mf;
    enum nh_log_status ret;
    boolean game_inited = (wiz1_level.dlevel != 0);
    struct you sg_you;
    struct flag sg_flags;
    long sg_moves;
    long long starttime;
    
    lseek(fd, 0, SEEK_SET);
    read(fd, header, 127);
    header[127] = '\0';
    n = sscanf(header, "NHGAME %4s %x NitroHack %d.%d.%d\n%llx %x %x %s %s %s %s %s",
	       status, &savepos, &v1, &v2, &v3, &starttime, &seed, &playmode, encplname,
	       role, race, gend, algn);
    if (n != 13) return LS_INVALID;
    
    endpos = lseek(fd, 0, SEEK_END);
    if (!strcmp(status, "done"))
	ret = LS_DONE;
    else if (!strcmp(status, "inpr"))
	ret = LS_CRASHED;
    else if (!strcmp(status, "save"))
	ret = LS_SAVED;
    else
	return LS_INVALID;
    
    if (ret == LS_SAVED && endpos == savepos)
	ret = LS_CRASHED;
    
    /* if we can't lock the file, it's in use */
    if (!lock_fd(fd, 0))
	ret = LS_IN_PROGRESS;
    unlock_fd(fd); /* don't need the lock, we're not going to write */

    if (!gi)
	return ret;
    
    memset(gi, 0, sizeof(struct nh_game_info));
    gi->playmode = playmode;
    base64_decode(encplname, gi->name);
    role[0] = lowc(role[0]);
    strcpy(gi->plrole, role);
    strcpy(gi->plrace, race);
    strcpy(gi->plgend, gend);
    strcpy(gi->plalign, algn);
    
    if (ret == LS_CRASHED || ret == LS_IN_PROGRESS)
	return ret;
    
    if (!api_entry_checkpoint())
	/* something went wrong, hopefully it isn't so bad that replay won't work */
	return LS_CRASHED;

    lseek(fd, savepos, SEEK_SET);
    if (ret == LS_SAVED) {
	mf.pos = 0;
	mf.buf = loadfile(fd, &mf.len);
	if (!mf.buf)
	    return 0;
	
	if (!uptodate(&mf, NULL)) {
	    free(mf.buf);
	    api_exit();
	    return LS_CRASHED; /* probably still a valid game */
	}
	
	restore_flags(&mf, &sg_flags);
	restore_you(&mf, &sg_you);
	sg_moves = mread32(&mf);
	free(mf.buf);
	
	/* make sure topten_level_name can work correctly */
	if (!game_inited) {
	    dlb_init();
	    init_dungeons();
	}
	
	gi->depth = depth(&sg_you.uz);
	gi->moves = sg_moves;
	gi->level_desc[0] = '\0';
	gi->has_amulet = sg_you.uhave.amulet;
	topten_level_name(sg_you.uz.dnum, depth(&sg_you.uz), gi->level_desc);
	
	if (!game_inited) {
	    free_dungeon();
	    dlb_cleanup();
	}
    } else if (ret == LS_DONE) {
	struct nh_topten_entry tt;
	
	read_log_toptenentry(fd, &tt);
	gi->moves = tt.moves;
	gi->depth = tt.maxlvl;
	strncpy(gi->death, tt.death, BUFSZ);
    }
    
    api_exit();
    
    return ret;
}
