/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"
#include "patchlevel.h"

extern int logfile;
extern unsigned int last_cmd_pos;


static void replay_clear_map(void) {}
static void replay_pause(enum nh_pause_reason r) {}
static void replay_display_buffer(char *buf, boolean trymove) {}
static void replay_update_status(struct nh_player_info *pi) {}
static void replay_print_message(int turn, const char *msg) {}
static void replay_update_screen(struct nh_dbuf_entry dbuf[ROWNO][COLNO]) {}
static void replay_delay_output(void) {}
static void replay_level_changed(int displaymode) {}
static void replay_outrip(struct nh_menuitem *items,int icount, boolean tombstone,
			   char *name, long gold, char *killbuf, int year) {}
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
} loginfo;

int first_cmd_token;

static char **commands;
static int cmdcount;
static struct nh_option_desc *saved_options;

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


static const struct nh_window_procs replay_windowprocs = {
    replay_clear_map,
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
	raw_printf("The game log is locked by another nethack process. Aborting.");
	terminate();
    }
    logfile = logfd;
}


void replay_begin(void)
{
    long filesize;
    int nr_tokens;
    char *nexttoken, header[24];
    unsigned int endpos;
    boolean truncated = FALSE;
    
    if (loginfo.mem) {
	free(loginfo.mem);
	free(loginfo.tokens);
    }
    
    filesize = lseek(logfile, 0, SEEK_END);
    if (filesize < 24)
	terminate(); /* too short to contain anything usable */
    
    lseek(logfile, 0, SEEK_SET);
    read(logfile, header, 23);
    header[23] = '\0';
    if (!sscanf(header, "NHGAME %*s %x", &endpos) || endpos > filesize)
	terminate();
    
    if (!endpos) {
	endpos = filesize;
	truncated = TRUE;
    }
    
    loginfo.mem = malloc(endpos+1);
    lseek(logfile, 0, SEEK_SET);
    read(logfile, loginfo.mem, endpos);
    loginfo.mem[endpos] = '\0';
    
    nr_tokens = 1024;
    loginfo.tokens = malloc(nr_tokens * sizeof(char*));
    
    nexttoken = strtok(loginfo.mem, " \r\n");
    while (nexttoken) {
	loginfo.tokens[loginfo.next++] = nexttoken;
	nexttoken = strtok(NULL, " \r\n");
	if (loginfo.next >= nr_tokens) {
	    nr_tokens *= 2;
	    loginfo.tokens = realloc(loginfo.tokens, nr_tokens * sizeof(char*));
	}
    }
    
    loginfo.tokencount = loginfo.next;
    loginfo.next = 0;
    
    if (truncated) {
	/* the last token should always be a command status, eg "<rng:u.ux:u.uy:u.uz" */
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
    if (!loginfo.mem)
	return;
    
    free(loginfo.mem);
    free(loginfo.tokens);
    memset(&loginfo, 0, sizeof(loginfo));

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


static void parse_error(const char *str)
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
    char *token;
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
    
    return i;
}


static char replay_query_key(const char *query, int *count)
{
    char key;
    char *token = next_log_token();
    int cnt = -1, n;
    
    n = sscanf(token, "k:%hhx:%x", &key, &cnt);
    if (n < 1)
	parse_error("Bad query_key data");
    
    if (count)
	*count = cnt;
    
    return key;
}


static int replay_getpos(int *x, int *y, boolean force, const char *goal)
{
    int ret, n;
    char *token = next_log_token();
    
    n = sscanf(token, "p:%d:%x:%x", &ret, x, y);
    if (n != 3)
	parse_error("Bad getpos data");
    
    return ret;
}


static enum nh_direction replay_getdir(const char *query, boolean restricted)
{
    enum nh_direction dir;
    char *token = next_log_token();
    
    int n = sscanf(token, "d:%d", &dir);
    if (n != 1)
	parse_error("Bad getdir data");
    
    return dir;
}


static char replay_yn_function(const char *query, const char *rset, char defchoice)
{
    char key;
    char *token = next_log_token();
    
    int n = sscanf(token, "y:%hhx", &key);
    if (n != 1)
	parse_error("Bad yn_function data");
    
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
}


char *replay_bones(int *buflen)
{
    char *b64data, *token = next_log_token();
    char *buf = NULL;
    
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
    if (!procs) {
	win_raw_print = windowprocs.win_raw_print;
	windowprocs = replay_windowprocs;
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
    
    first_cmd_token = loginfo.next;
}


void replay_read_newgame(time_t *seed, int *playmode, char *namebuf)
{
    char *header, *gamename, *verstr;
    int ver1, ver2, ver3, n;
    
    header = next_log_token();
    if (!header || strcmp(header, "NHGAME"))
	parse_error("This file does not look like a NetHack logfile.");
    
    next_log_token(); /* marker */
    next_log_token(); /* end pos, see replay_begin() */
    gamename = next_log_token();
    verstr = next_log_token();
    
    n = sscanf(verstr, "%d.%d.%d", &ver1, &ver2, &ver3);
    if (n != 3)
	parse_error("No version found where it was expected");
    
    if (ver1 != VERSION_MAJOR && ver2 != VERSION_MINOR)
	raw_printf("Warning: Version mismatch; expected %d.%d, got %d.%d\n",
		   VERSION_MAJOR, VERSION_MINOR, ver1, ver2);

    *seed = strtoul(next_log_token(), NULL, 16);
    *playmode = atoi(next_log_token());
    base64_decode(next_log_token(), namebuf);
    flags.initrole = nh_str2role(next_log_token());
    flags.initrace = nh_str2race(next_log_token());
    flags.initgend = nh_str2gend(next_log_token());
    flags.initalign = nh_str2align(next_log_token());
    
    if (flags.initrole == ROLE_NONE || flags.initrace == ROLE_NONE ||
	flags.initgend == ROLE_NONE || flags.initalign == ROLE_NONE)
	terminate();
    
    replay_read_commandlist();
}


static void replay_read_option(char *token)
{
    char *name, *otype, *valstr, optname[BUFSZ], valbuf[BUFSZ];
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
	    value.s = strlen(valbuf) > 0 ? valbuf : NULL;
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
    
    n = sscanf(cmdtok, ">%lx:%x:%d", &turntime, &cmdidx, count);
    if (n != 3 || cmdidx > cmdcount)
	parse_error("Error: Incorrect command spec\n");
    
    *cmd = commands[cmdidx];
    
    if (!replay_parse_arg(next_log_token(), arg))
	parse_error("Bad command argument");
}


static void replay_check_cmdresult(void)
{
    int n;
    unsigned int rngstate;
    char *token = next_log_token();
    if (!token)
	return;
    
    n = sscanf(token, "<%x", &rngstate);
    if (n != 1)
	parse_error("Error: incorrect command result specification\n");
    
    if (rngstate != (mt_nextstate() & 0xffff))
	parse_error("Error: RNG state deviation detected.");
}


void replay_run_cmdloop(boolean optonly)
{
    char *cmd;
    int count;
    struct nh_cmd_arg cmdarg;
    enum nh_input_status gamestate = READY_FOR_INPUT;
    struct nh_option_desc *tmp;
    
    loginfo.next = first_cmd_token;
    
    /* the log contains the birth options that are required for this game,
     * so nh_set_option calls during the replay must change active_birth_options */
    tmp = birth_options;
    birth_options = active_birth_options;
    active_birth_options = tmp;
    
    char *token = next_log_token();
    
    while (token && gamestate < GAME_OVER) {
	if (token[0] == '!') {
	    replay_read_option(token);
	    token = next_log_token();
	    continue;
	} else if (optonly)
	    break; /* first non-option token - initial option setup is done */
	
	replay_read_command(token, &cmd, &count, &cmdarg);
	gamestate = nh_do_move(cmd, count, &cmdarg);
	replay_check_cmdresult();
	
	token = next_log_token();
    }

    /* return the birth options to normal */
    tmp = birth_options;
    birth_options = active_birth_options;
    active_birth_options = tmp;
}


enum nh_log_status nh_get_savegame_status(int fd, struct nh_save_info *si)
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
    struct permonst sg_youmonst;
    long sg_moves;
    
    lseek(fd, 0, SEEK_SET);
    read(fd, header, 127);
    header[127] = '\0';
    n = sscanf(header, "NHGAME %4s %x NetHack %d.%d.%d\n%x %x %s %s %s %s %s",
	       status, &savepos, &v1, &v2, &v3, &seed, &playmode, encplname,
	       role, race, gend, algn);
    if (n != 12) return LS_INVALID;
    
    endpos = lseek(fd, 0, SEEK_END);
    if (!strcmp(status, "done"))
	return LS_DONE;
    else if (!strcmp(status, "inpr") || endpos == savepos)
	ret = LS_CRASHED;
    else if (!strcmp(status, "save"))
	ret = LS_SAVED;
    else
	return LS_INVALID;
    
    /* if we can't lock the file, it's in use */
    if (!lock_fd(fd, 0))
	ret = LS_IN_PROGRESS;
    unlock_fd(fd); /* don't need the lock, we're not going to write */

    if (!si)
	return ret;
    
    si->playmode = playmode;
    base64_decode(encplname, si->name);
    role[0] = lowc(role[0]);
    strcpy(si->plrole, role);
    strcpy(si->plrace, race);
    strcpy(si->plgend, gend);
    strcpy(si->plalign, algn);
    
    if (ret == LS_CRASHED || ret == LS_IN_PROGRESS)
	return ret;
    
    if (!api_entry_checkpoint())
	/* something went wrong, hopefully it isn't so bad that replay won't work */
	return LS_CRASHED;

    mf.len = endpos - savepos;
    mf.pos = 0;
    mf.buf = malloc(mf.len);
    lseek(fd, savepos, SEEK_SET);
    if (!read(fd, mf.buf, mf.len) == mf.len || !uptodate(&mf, NULL)) {
	free(mf.buf);
	api_exit();
	return LS_CRASHED; /* probably still a valid game */
    }
    
    mread(&mf, &sg_flags, sizeof(struct flag));
    flags.bypasses = 0;	/* never use the saved value of bypasses */

    role_init();	/* Reset the initial role, race, gender, and alignment */
    mread(&mf, &sg_you, sizeof(struct you));
    mread(&mf, &sg_youmonst, sizeof(youmonst));
    set_uasmon(); /* fix up youmonst.data */
    mread(&mf, &sg_moves, sizeof(moves));
    free(mf.buf);
    
    /* make sure topten_level_name can work correctly */
    if (!game_inited) {
	dlb_init();
	init_dungeons();
    }
    
    si->depth = depth(&sg_you.uz);
    si->moves = sg_moves;
    si->level_desc[0] = '\0';
    si->has_amulet = sg_you.uhave.amulet;
    topten_level_name(sg_you.uz.dnum, sg_you.uz.dlevel, si->level_desc);
    
    if (!game_inited) {
	    free_dungeons();
	    dlb_cleanup();
    }
    
    api_exit();
    
    return LS_SAVED;
}
