/* Copyright (c) Daniel Thaler, 2012 */
/* NitroHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <fcntl.h>

enum menuids {
    NEWGAME = 1,
    LOAD,
    REPLAY,
    OPTIONS,
    TOPTEN,
    ACCOUNT,
    DISCONNECT
};

struct server_info {
    const char *hostname;
    const char *username;
    const char *password;
    int port;
};

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

static const unsigned char b64e[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static void base64_encode(const char* in, char *out)
{
    int i, pos = 0, rem;
    int len = strlen(in);
    
    for (i = 0; i < (len / 3) * 3; i += 3) {
	out[pos  ] = b64e[((unsigned)in[i  ]       ) >> 2];
	out[pos+1] = b64e[((unsigned)in[i  ] & 0x03) << 4 | (((unsigned)in[i+1]) & 0xf0) >> 4];
	out[pos+2] = b64e[((unsigned)in[i+1] & 0x0f) << 2 | (((unsigned)in[i+2]) & 0xc0) >> 6];
	out[pos+3] = b64e[ (unsigned)in[i+2] & 0x3f];
	pos += 4;
    }
    
    rem = len - i;
    if (rem > 0) {
	out[pos] = b64e[in[i] >> 2];
        out[pos+1] = b64e[(in[i  ] & 0x03) << 4 | (in[i+1] & 0xf0) >> 4];
	out[pos+2] = (rem == 1) ? '=' : b64e[(in[i+1] & 0x0f) << 2];
	out[pos+3] = '=';
	pos += 4;
    }
    
    out[pos] = '\0';
}


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


static void account_menu(struct server_info *server)
{
    int menuresult[1];
    int n = 1;
    char buf1[BUFSZ], buf2[BUFSZ];
    
    static struct nh_menuitem netmenu_items[] = {
	{1, MI_NORMAL, "change email address", 'e'},
	{2, MI_NORMAL, "change password", 'p'},
	{0, MI_NORMAL, "", 0},
	{3, MI_NORMAL, "back to main menu", 'x'}
    };
    
    while (n > 0) {
	menuresult[0] = 3; /* default action */
	n = curses_display_menu(netmenu_items, ARRAY_SIZE(netmenu_items),
				     "Account settings:", PICK_ONE, menuresult);
	
	switch (menuresult[0]) {
	    case 1:
		curses_getline("What email address do you want to use?", buf1);
		if (*buf1 != '\033' &&
		    (*buf1 || curses_yn_function("Remove current email address?",
						"yn", 'n') == 'y'))
		    nhnet_change_email(buf1);
		break;
		
	    case 2:
		curses_getline("Current password:", buf1);
		if (strcmp(server->password, buf1)) {
		    curses_msgwin("Incorrect password.");
		    break;
		}   
		
		curses_getline("Change password: (Beware - it is transmitted in plain text)", buf1);
		if (buf1[0] != '\033' && buf1[0] != '\0')
		    curses_getline("Confirm password:", buf2);
		
		if (buf2[0] == '\033' || buf2[0] == '\0')
		    curses_msgwin("Password change cancelled.");
		else if (strcmp(buf1, buf2))
		    curses_msgwin("The passwords didn't match. The password was not changed.");
		else {
		    nhnet_change_password(buf1);
		    free((void*)server->password);
		    server->password = strdup(buf1);
		}
		break;
		
	    case 3:
		return;
	}
	
	/* unrecoverable connection error? */
	if (!nhnet_connected())
	    break;
    }
}


static struct server_info *read_server_list(void)
{
    fnchar filename[BUFSZ];
    char hnbuf[256], unbuf_enc[256], pwbuf_enc[256], decbuf[256];
    char *data, *line;
    int size, scount, port, n;
    struct server_info *servlist;
    FILE *fp;
    
    scount = 0;
    servlist = malloc(sizeof(struct server_info));
    memset(&servlist[scount], 0, sizeof(struct server_info));
    
    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
	return servlist;
    fnncat(filename, FN("servers.conf"), BUFSZ);
    
    fp = fopen(filename, "rb");
    if (!fp)
	return servlist;

    fseek(fp , 0 , SEEK_END);
    size = ftell(fp);
    rewind(fp);
    
    data = malloc(size+1);
    fread(data, size, 1, fp);
    data[size] = '\0';
    fclose(fp);
    
    line = strtok(data, "\r\n");
    while (line) {
	n = sscanf(line, "%255s\t%d\t%255s\t%255s", hnbuf, &port, unbuf_enc, pwbuf_enc);
	if (n == 4) {
	    servlist[scount].hostname = strdup(hnbuf);
	    servlist[scount].port = port;
	    base64_decode(unbuf_enc, decbuf);
	    servlist[scount].username = strdup(decbuf);
	    base64_decode(pwbuf_enc, decbuf);
	    servlist[scount].password = strdup(decbuf);
	    scount++;
	    servlist = realloc(servlist, sizeof(struct server_info) * (scount+1));
	}
	
	line = strtok(NULL, "\r\n");
    }
    
    free(data);
    memset(&servlist[scount], 0, sizeof(struct server_info));
    return servlist;
}


static void write_server_list(struct server_info *servlist)
{
    fnchar filename[BUFSZ];
    int i;
    char un_enc[256], pw_enc[256];
    FILE *fp;
    
    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
	return;
    fnncat(filename, FN("servers.conf"), BUFSZ);
    
    fp = fopen(filename, "w+b");
    if (!fp)
	return;
    
    for (i = 0; servlist[i].hostname; i++) {
	base64_encode(servlist[i].username, un_enc);
	base64_encode(servlist[i].password, pw_enc);
	fprintf(fp, "%.255s\t%d\t%.255s\t%.255s\n", servlist[i].hostname,
		servlist[i].port, un_enc, pw_enc);
    }
    
    fclose(fp);
}


static void free_server_list(struct server_info *list)
{
    int i;
    
    for (i = 0; list[i].hostname; i++) {
	free((void*)list[i].hostname);
	free((void*)list[i].username);
	free((void*)list[i].password);
    }
    free(list);
}


static struct server_info *add_server_menu(struct server_info **servlist)
{
    int i, ret, port, hostok, passok, accountok;
    char hostbuf[BUFSZ], portbuf[BUFSZ], userbuf[BUFSZ], passbuf[BUFSZ], passbuf2[BUFSZ];
    
    do {
	curses_getline("Hostname or IP address:", hostbuf);
	if (hostbuf[0] == '\033' || hostbuf[0] == '\0')
	    return NULL;
	curses_getline("Port number (0 or empty = use default):", portbuf);
	if (portbuf[0] == '\033')
	    return NULL;
	
	port = atoi(portbuf);
	
	hostok = FALSE;
	if (nhnet_connect(hostbuf, port, "", "", NULL, 0) != NO_CONNECTION) {
	    hostok = TRUE;
	    nhnet_disconnect();
	} else
	    curses_msgwin("Connection test failed");
    } while (!hostok);

    do {
	curses_getline("Account name (It will be created if necessary):", userbuf);
	if (userbuf[0] == '\033' || userbuf[0] == '\0')
	    return NULL;
	
	do {
	    curses_getline("New password: (Beware - it is transmitted in plain text)", passbuf);
	    if (passbuf[0] == '\033' || passbuf[0] == '\0')
		return NULL;
	    curses_getline("Confirm password:", passbuf2);
	    if (passbuf2[0] == '\033' || passbuf2[0] == '\0')
		return NULL;
	    
	    passok = TRUE;
	    if (strcmp(passbuf, passbuf2)) {
		passok = FALSE;
		curses_msgwin("The passwords didn't match.");
	    }
	} while (!passok);
	
	ret = nhnet_connect(hostbuf, port, userbuf, passbuf, NULL, 0);
	if (ret == AUTH_FAILED_BAD_PASSWORD) {
	    accountok = FALSE;
	    curses_msgwin("The account exists but this is the wrong password.");
	} else
	    accountok = TRUE;
	nhnet_disconnect();

    } while (!accountok);

    
    for (i = 0; (*servlist)[i].hostname; i++);
    *servlist = realloc(*servlist, sizeof(struct server_info) * (i+2));
    memmove(&(*servlist)[1], &(*servlist)[0], sizeof(struct server_info) * (i+1));
    
    (*servlist)[0].hostname = strdup(hostbuf);
    (*servlist)[0].port = port;
    (*servlist)[0].username = strdup(userbuf);
    (*servlist)[0].password = strdup(passbuf);
    
    write_server_list(*servlist);
    
    return &(*servlist)[0];
}


static void list_servers(struct server_info *servlist,
			 struct nh_menuitem **items, int *size, int *icount)
{
    char buf[BUFSZ];
    int i;
    
    for (i = 0; servlist[i].hostname; i++) {
	if (servlist[i].port) {
	    if (!strchr(servlist[i].hostname, ':'))
		sprintf(buf, "%s on %s:%d", servlist[i].username,
			servlist[i].hostname, servlist[i].port);
	    else
		sprintf(buf, "%s on [%s]:%d", servlist[i].username,
			servlist[i].hostname, servlist[i].port);
	} else
	    sprintf(buf, "%s on %s", servlist[i].username,
		    servlist[i].hostname);
	
	add_menu_item(*items, *size, *icount, i+1, buf, 0, 0);
    }
}

static void delete_server_menu(struct server_info *servlist)
{
    struct nh_menuitem *items;
    int icount, size, n, id, selected[1];
    
    icount = 0;
    size = 10;
    items = malloc(size * sizeof(struct nh_menuitem));
    
    list_servers(servlist, &items, &size, &icount);
    
    n = curses_display_menu(items, icount, "Delete which server?", PICK_ONE, selected);
    free(items);
    
    if (n <= 0)
	return;
    
    id = selected[0] - 1;
    memmove(&servlist[id], &servlist[id+1], (icount - id) * sizeof(struct server_info));
    
    write_server_list(servlist);
}


static struct server_info *connect_server_menu(struct server_info **servlist)
{
    struct nh_menuitem *items;
    int icount, size, n, selected[1];
    struct server_info *server;
    
    while (1) {
	icount = 0;
	size = 10;
	items = malloc(size * sizeof(struct nh_menuitem));
	
	list_servers(*servlist, &items, &size, &icount);
	
	add_menu_txt(items, size, icount, "", MI_NORMAL);
	add_menu_item(items, size, icount, -1, "Add server", '!', 0);
	add_menu_item(items, size, icount, -2, "Delete server", '#', 0);
	
	n = curses_display_menu(items, icount, "Connect to which server?",
				PICK_ONE, selected);
	free(items);
	if (n <= 0)
	    break;

	if (selected[0] == -1) {
	    server = add_server_menu(servlist);
	    if (server)
		return server;
	} else if (selected[0] == -2) {
	    delete_server_menu(*servlist);
	} else
	    return &(*servlist)[selected[0]-1];

    }
    return NULL;
}


static int connect_server(struct server_info *server)
{
    int ret, i;
    char buf[BUFSZ];
    struct nh_option_desc *game_opts, *birth_opts;
    
    while (1) {
	ret = nhnet_connect(server->hostname, server->port, server->username,
			    server->password, NULL, 0);
	if (ret == AUTH_SUCCESS_NEW)
	    return TRUE;
	else if (ret == AUTH_SUCCESS_RECONNECT) {
	    nhnet_exit_game(EXIT_FORCE_SAVE);
	    if (!nhnet_connected()) /* disconnect due to an error while reconnecting */
		return FALSE;
	    return TRUE;
	} else if (ret == NO_CONNECTION) {
	    curses_msgwin("Connection attempt failed");
	    return FALSE;
	}
	else if (ret == AUTH_FAILED_BAD_PASSWORD) {
	    curses_msgwin("Authentication failed: Wrong password.");
	    curses_getline("Password:", buf);
	    if (buf[0] == '\033' || buf[0] == '\0')
		return FALSE;
	    free((void*)server->password);
	    server->password = strdup(buf);
	    continue;
	} else {/* AUTH_FAILED_UNKNOWN_USER */
	    nhnet_lib_exit(); /* need to acces the local options */
	    game_opts = nh_get_options(GAME_OPTIONS);
	    birth_opts = nh_get_options(CURRENT_BIRTH_OPTIONS);
	    nhnet_lib_init(&curses_windowprocs);
	    
	    sprintf(buf, "The account \"%s\" will be created for you.", server->username);
	    curses_msgwin(buf);
	    curses_getline("(Optional) You may give an email address for password resets:", buf);
	    ret = nhnet_connect(server->hostname, server->port, server->username,
				server->password, buf, TRUE);
	    if (ret != AUTH_SUCCESS_NEW) {
		curses_msgwin("Sorry, the registration failed.");
		return FALSE;
	    }
	    
	    /* upload current options */
	    if (curses_yn_function("Do you want to copy your current game "
		                   "options to the server?", "yn", 'y') == 'y') {
		for (i = 0; game_opts[i].name; i++)
		    nh_set_option(game_opts[i].name, game_opts[i].value, 0);
		for (i = 0; birth_opts[i].name; i++)
		    nh_set_option(birth_opts[i].name, birth_opts[i].value, 0);
	    }
	    
	    return TRUE;
	}
    }
}


static void netgame_mainmenu(struct server_info *server)
{
    int menuresult[1];
    char buf[BUFSZ];
    int n = 1, logoheight, i;
    const char **nhlogo;
    char verstr[32], server_verstr[32];

    static struct nh_menuitem netmenu_items[] = {
	{NEWGAME, MI_NORMAL, "new game", 'n'},
	{LOAD, MI_NORMAL, "load game", 'l'},
	{REPLAY, MI_NORMAL, "view replay", 'v'},
	{OPTIONS, MI_NORMAL, "set options", 'o'},
	{TOPTEN, MI_NORMAL, "show score list", 's'},
	{ACCOUNT, MI_NORMAL, "account settings", 'a'},
	{DISCONNECT, MI_NORMAL, "disconnect", 'q', 'x'}
    };
    
    sprintf(verstr, "Client version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
    sprintf(server_verstr, "Server version: %d.%d.%d", nhnet_server_ver.major,
	    nhnet_server_ver.minor, nhnet_server_ver.patchlevel);
    
    while (n > 0) {
	if (COLS >= 100)
	    nhlogo = nhlogo_large;
	else
	    nhlogo = nhlogo_small;
	logoheight = sizeof(nhlogo_small) / sizeof(nhlogo_small[0]);
	wclear(basewin);
	wattron(basewin, A_BOLD | COLOR_PAIR(4));
	for (i = 0; i < logoheight; i++) {
	    wmove(basewin, i, (COLS - strlen(nhlogo[0])) / 2);
	    waddstr(basewin, nhlogo[i]);
	}
	wattroff(basewin, A_BOLD | COLOR_PAIR(4));
	
	if (nhnet_server_ver.major > 0 || nhnet_server_ver.minor > 0)
	    mvwaddstr(basewin, LINES-1, 0, server_verstr);
	mvwaddstr(basewin, LINES-1, COLS - strlen(verstr), verstr);
	wrefresh(basewin);

	menuresult[0] = DISCONNECT; /* default action */
	snprintf(buf, BUFSZ, "%s on %s:", server->username, server->hostname);
	n = curses_display_menu_core(netmenu_items, ARRAY_SIZE(netmenu_items),
				     buf, PICK_ONE, menuresult, 0, logoheight,
				     COLS, ROWNO+3, NULL);
	
	switch (menuresult[0]) {
	    case NEWGAME:
		net_rungame();
		break;
		
	    case LOAD:
		net_loadgame();
		break;
		
	    case REPLAY:
		net_replay();
		break;
		
	    case OPTIONS:
		display_options(TRUE);
		break;
		
	    case TOPTEN:
		show_topten(NULL, -1, FALSE, FALSE);
		break;
		
	    case ACCOUNT:
		account_menu(server);
		break;
		
	    case DISCONNECT:
		return;
	}
	
	/* unrecoverable connection error? */
	if (!nhnet_connected())
	    break;
    }
}


void netgame(void)
{
    struct server_info *servlist, *server;
    
    nhnet_lib_init(&curses_windowprocs);
    servlist = read_server_list();
    
    server = connect_server_menu(&servlist);
    if (!server || !connect_server(server))
	goto finally;
    
    free_displaychars(); /* remove old display info */
    init_displaychars(); /* load new display info from the server */
    
    netgame_mainmenu(server);
    write_server_list(servlist);
    nhnet_disconnect();
    
finally:
    free_server_list(servlist);
    nhnet_lib_exit();
    free_displaychars(); /* remove server display info */
    init_displaychars(); /* go back to local object/monster/... lists */
}

/* netgame.c */