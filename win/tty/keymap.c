/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nethack.h"
#include "wintty.h"

enum internal_commands {
    /* implicitly include enum nh_direction */
    TTYCMD_OPTIONS = DIR_SELF + 1,
    TTYCMD_EXTCMD,
    TTYCMD_REDO,
    TTYCMD_PREVMSG
};

static const char TTY_BUILTIN_CMD[] = "TTY_BUILTIN_CMD";
static struct nh_cmd_desc *keymap_hjkl[256];
static struct nh_cmd_desc *keymap_npad[256];
static struct nh_cmd_desc *commandlist;
static int cmdcount = 0;
static struct nh_cmd_desc *prev_cmd = NULL;
static struct nh_cmd_arg prev_arg = {CMD_ARG_NONE};
static int prev_count = 0;

#ifndef Ctrl
#define Ctrl(c)		(0x1f & (c))
#endif

struct nh_cmd_desc builtin_commands[] = {
	{TTY_BUILTIN_CMD, "", 'h', '4', DIR_W},
	{TTY_BUILTIN_CMD, "", 'y', '7', DIR_NW},
	{TTY_BUILTIN_CMD, "", 'k', '8', DIR_N},
	{TTY_BUILTIN_CMD, "", 'u', '9', DIR_NE},
	{TTY_BUILTIN_CMD, "", 'l', '6', DIR_E},
	{TTY_BUILTIN_CMD, "", 'n', '3', DIR_SE},
	{TTY_BUILTIN_CMD, "", 'j', '2', DIR_S},
	{TTY_BUILTIN_CMD, "", 'b', '1', DIR_SW},
	
	{TTY_BUILTIN_CMD, "", 'O', 0, TTYCMD_OPTIONS},
	{TTY_BUILTIN_CMD, "", '#', 0, TTYCMD_EXTCMD},
	{TTY_BUILTIN_CMD, "", '\001', 0, TTYCMD_REDO},
	{TTY_BUILTIN_CMD, "", Ctrl('p'), 0, TTYCMD_PREVMSG},
};


static struct nh_cmd_desc *doextcmd(void);


static struct nh_cmd_desc *find_command(const char *name)
{
    int i;
    for (i = 0; i < cmdcount; i++)
	if (!strcmp(name, commandlist[i].name))
	    return &commandlist[i];
	
    return NULL;
}


static void handle_internal_cmd(struct nh_cmd_desc **cmd, struct nh_cmd_arg *arg, int *count)
{
	switch ((*cmd)->flags) {
	    case DIR_W:
	    case DIR_NW:
	    case DIR_N:
	    case DIR_NE:
	    case DIR_E:
	    case DIR_SE:
	    case DIR_S:
	    case DIR_SW:
		arg->argtype = CMD_ARG_DIR;
		arg->d = (*cmd)->flags;
		*cmd = find_command("move");
		break;
		
	    case TTYCMD_OPTIONS:
		display_options(FALSE);
		*cmd = NULL;
		break;
	    
	    case TTYCMD_EXTCMD:
		*cmd = doextcmd();
		break;
		
	    case TTYCMD_REDO:
		*cmd = prev_cmd;
		*arg = prev_arg;
		*count = prev_count;
		break;
		
	    case TTYCMD_PREVMSG:
		tty_doprev_message();
		*cmd = NULL;
		break;
	}
}


const char *get_command(int *count, struct nh_cmd_arg *arg)
{
	int key, key2, multi = 0;
	char line[BUFSZ];
	const char *dirs, *dp;
	struct nh_cmd_desc *cmd;
	struct nh_cmd_desc **keymap;
	boolean bad_command;
	
	do {
	    /* the value of ui_flags.num_pad may change while the loop is running */
	    keymap = ui_flags.num_pad ? keymap_npad : keymap_hjkl;
	    dirs = ui_flags.num_pad ? ndir : sdir;

	    cmd = NULL;
	    arg->argtype = CMD_ARG_NONE;
	    bad_command = FALSE;
	    
	    tty_curs(player.x, player.y);
	    
	    if (!ui_flags.num_pad || (key = tty_nhgetch()) == 'n')
		for (;;) {
		    key = tty_nhgetch();
		    if (key >= '0' && key <= '9') {
			multi = 10 * multi + key - '0';
			if (multi < 0 || multi >= 0xffff) multi = 0xffff;
			if (multi > 9) {
			    tty_clear_nhwindow(NHW_MESSAGE);
			    sprintf(line, "Count: %d", multi);
			    tty_print_message(line);
			}
		    } else
			break;	/* not a digit */
		}
	    
	    if (key == '\033' || key == '\n') /* filter out ESC and enter */
		continue;
	    
	    *count = multi;
	    cmd = keymap[key];
	    
	    if (cmd != NULL) {
		/* handle internal commands. The command handler may alter
		 * cmd, arg and count (redo does this) */
		if (cmd->name == TTY_BUILTIN_CMD) {
		    handle_internal_cmd(&cmd, arg, count);
		    if (!cmd) /* command was fully handled internally */
			continue;
		}
		
		/* if the command requres an arg AND the arg isn't set yet (by handle_internal_cmd) */
		if (cmd->flags & CMD_ARG_DIR && arg->argtype != CMD_ARG_DIR) {
		    key2 = tty_nhgetch();
		    if (key2 == '\033') /* cancel silently */
			continue;
		    
		    dp = index(dirs, key2);
		    if (dp) {
			arg->argtype = CMD_ARG_DIR;
			arg->d = (enum nh_direction)(dp-dirs);
		    } else
			cmd = NULL;
		}
	    }
	    
	    if (!cmd) {
		tty_print_message("Bad command.");
	    }
	} while (!cmd);
	
	tty_clear_nhwindow(NHW_MESSAGE);
	tty_curs(player.x, player.y);
	
	prev_cmd = cmd;
	prev_arg = *arg;
	prev_count = *count;
	
	return cmd->name;
}


void load_keymap(boolean want_wizard)
{
	int count, i;
	struct nh_cmd_desc *cmdlist = nh_get_commands(&cmdcount, want_wizard);
	
	commandlist = malloc(cmdcount * sizeof(struct nh_cmd_desc));
	memcpy(commandlist, cmdlist, cmdcount * sizeof(struct nh_cmd_desc));
	
	count = sizeof(builtin_commands)/sizeof(struct nh_cmd_desc);
	for (i = 0; i < count; i++) {
	    if (builtin_commands[i].defkey && builtin_commands[i].altkey) {
		keymap_hjkl[(int)builtin_commands[i].defkey] = &builtin_commands[i];
		keymap_npad[(int)builtin_commands[i].altkey] = &builtin_commands[i];
	    } else if (builtin_commands[i].defkey) {
		keymap_hjkl[(int)builtin_commands[i].defkey] = &builtin_commands[i];
		keymap_npad[(int)builtin_commands[i].defkey] = &builtin_commands[i];
	    }
	}
	
	for (i = 0; i < cmdcount; i++) {
	    if (commandlist[i].defkey && commandlist[i].altkey) {
		keymap_hjkl[(int)commandlist[i].defkey] = &commandlist[i];
		keymap_npad[(int)commandlist[i].altkey] = &commandlist[i];
		
		if (!keymap_hjkl[(int)commandlist[i].altkey])
		    keymap_hjkl[(int)commandlist[i].altkey] = &commandlist[i];
		
		if (!keymap_npad[(int)commandlist[i].defkey])
		    keymap_npad[(int)commandlist[i].defkey] = &commandlist[i];
		
	    } else if (commandlist[i].defkey) {
		keymap_hjkl[(int)commandlist[i].defkey] = &commandlist[i];
		keymap_npad[(int)commandlist[i].defkey] = &commandlist[i];
	    }
	}
	
}

/* here after #? - now list all full-word commands */
int doextlist(const char **namelist, const char **desclist, int listlen)
{
	char buf[BUFSZ];
	winid datawin;
	int i;

	datawin = tty_create_nhwindow(NHW_TEXT);
	tty_putstr(datawin, 0, "");
	tty_putstr(datawin, 0, "            Extended Commands List");
	tty_putstr(datawin, 0, "");
	tty_putstr(datawin, 0, "    Press '#', then type:");
	tty_putstr(datawin, 0, "");

	for (i = 0; i < listlen; i++) {
		sprintf(buf, "    %-15s - %s.", namelist[i], desclist[i]);
		tty_putstr(datawin, 0, buf);
	}
	tty_display_nhwindow(datawin, FALSE);
	tty_destroy_nhwindow(datawin);
	return 0;
}


/* here after # - now read a full-word command */
static struct nh_cmd_desc *doextcmd(void)
{
    int i, idx, size;
    struct nh_cmd_desc *retval = NULL;
    const char **namelist, **desclist;
    static const char exthelp[] = "?";
    int *idxmap;
    
    size = 0;
    for (i = 0; i < cmdcount; i++)
	if (commandlist[i].flags & CMD_EXT)
	    size++;
    namelist = malloc((size+1) * sizeof(char*));
    desclist = malloc((size+1) * sizeof(char*));
    idxmap = malloc((size+1) * sizeof(int));
    
    /* add help */
    namelist[size] = exthelp;
    namelist[size] = "get this list of extended commands";
    idxmap[size] = 0;
       
    idx = 0;
    for (i = 0; i < cmdcount; i++) {
	if (commandlist[i].flags & CMD_EXT) {
	    namelist[idx] = commandlist[i].name;
	    desclist[idx] = commandlist[i].desc;
	    idxmap[idx] = i;
	    idx++;
	}
    }
    
    /* keep repeating until we don't run help or quit */
    do {
	idx = tty_get_ext_cmd(namelist, desclist, size);
	if (idx < 0)
	    goto freemem;
	
	i = idxmap[idx];
	
	if (i == 0 && namelist[i] == exthelp)
	    doextlist(namelist, desclist, size);
	else
	    retval = &commandlist[i];
    } while (!retval);

freemem:
    free(namelist);
    free(desclist);
    free(idxmap);
    
    return retval;
}

