/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-12-23 */
/* Copyright (c) Daniel Thaler, 2012 */
/* NetHack may be freely redistributed.  See license for details. */

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
    char *hostname;
    char *username;
    char *password;
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
    /* '[' - '\'' */ 0, 0, 0, 0, 0, 0,
    /* 'a' - 'z' */ 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

static const unsigned char b64e[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static void
base64_encode(const char *in, char *out)
{
    int i, pos = 0, rem;
    int len = strlen(in);

    for (i = 0; i < (len / 3) * 3; i += 3) {
        out[pos] = b64e[((unsigned)in[i]) >> 2];
        out[pos + 1] =
            b64e[((unsigned)in[i] & 0x03) << 4 | (((unsigned)in[i + 1]) & 0xf0)
                 >> 4];
        out[pos + 2] =
            b64e[((unsigned)in[i + 1] & 0x0f) << 2 |
                 (((unsigned)in[i + 2]) & 0xc0) >> 6];
        out[pos + 3] = b64e[(unsigned)in[i + 2] & 0x3f];
        pos += 4;
    }

    rem = len - i;
    if (rem > 0) {
        out[pos] = b64e[in[i] >> 2];
        out[pos + 1] = b64e[(in[i] & 0x03) << 4 | (in[i + 1] & 0xf0) >> 4];
        out[pos + 2] = (rem == 1) ? '=' : b64e[(in[i + 1] & 0x0f) << 2];
        out[pos + 3] = '=';
        pos += 4;
    }

    out[pos] = '\0';
}


static void
base64_decode(const char *in, char *out)
{
    int i, len = strlen(in), pos = 0;

    for (i = 0; i < len; i += 4) {
        /* decode blocks; padding '=' are converted to 0 in the decoding table */
        out[pos] = b64d[(int)in[i]] << 2 | b64d[(int)in[i + 1]] >> 4;
        out[pos + 1] = b64d[(int)in[i + 1]] << 4 | b64d[(int)in[i + 2]] >> 2;
        out[pos + 2] =
            ((b64d[(int)in[i + 2]] << 6) & 0xc0) | b64d[(int)in[i + 3]];
        pos += 3;
    }

    out[pos] = 0;
}


static void
account_menu(struct server_info *server)
{
    int menuresult[1];
    int n = 1;
    char buf1[BUFSZ], buf2[BUFSZ];

    static struct nh_menuitem netmenu_items[] = {
        {1, MI_NORMAL, "change email address", 'e', 0, 0},
        {2, MI_NORMAL, "change password", 'p', 0, 0},
        {0, MI_NORMAL, "", 0, 0, 0},
        {3, MI_NORMAL, "back to main menu", 'x', 0, 0}
    };

    while (n > 0) {
        menuresult[0] = 3;      /* default action */
        n = curses_display_menu(netmenu_items, ARRAY_SIZE(netmenu_items),
                                "Account settings:", PICK_ONE, PLHINT_ANYWHERE,
                                menuresult);

        switch (menuresult[0]) {
        case 1:
            curses_getline("What email address do you want to use?", buf1);
            if (*buf1 != '\033' &&
                (*buf1 ||
                 curses_yn_function("Remove current email address?", "yn",
                                    'n') == 'y'))
                nhnet_change_email(buf1);
            break;

        case 2:
            curses_getline_pw("Current password:", buf1);
            if (strcmp(server->password, buf1)) {
                curses_msgwin("Incorrect password.");
                break;
            }

            curses_getline_pw
                ("Change password: (Beware - it is transmitted in plain text)",
                 buf1);
            if (buf1[0] != '\033' && buf1[0] != '\0')
                curses_getline_pw("Confirm password:", buf2);

            if (buf2[0] == '\033' || buf2[0] == '\0')
                curses_msgwin("Password change cancelled.");
            else if (strcmp(buf1, buf2))
                curses_msgwin
                    ("The passwords didn't match. The password was not changed.");
            else {
                nhnet_change_password(buf1);
                free((void *)server->password);
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


static struct server_info *
read_server_list(void)
{
    fnchar filename[BUFSZ];
    char hnbuf[256], unbuf_enc[256], pwbuf_enc[256], decbuf[256];
    char *data, *line;
    int size, scount, port, n;
    struct server_info *servlist;
    FILE *fp;

    scount = 0;
    servlist = malloc(sizeof (struct server_info));
    memset(&servlist[scount], 0, sizeof (struct server_info));

    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
        return servlist;
    fnncat(filename, FN("servers.conf"), BUFSZ);

    fp = fopen(filename, "rb");
    if (!fp)
        return servlist;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    data = malloc(size + 1);
    fread(data, size, 1, fp);
    data[size] = '\0';
    fclose(fp);

    line = strtok(data, "\r\n");
    while (line) {
        n = sscanf(line, "%255s\t%d\t%255s\t%255s", hnbuf, &port, unbuf_enc,
                   pwbuf_enc);
        if (n == 4) {
            servlist[scount].hostname = strdup(hnbuf);
            servlist[scount].port = port;
            base64_decode(unbuf_enc, decbuf);
            servlist[scount].username = strdup(decbuf);
            base64_decode(pwbuf_enc, decbuf);
            servlist[scount].password = strdup(decbuf);
            scount++;
            servlist =
                realloc(servlist, sizeof (struct server_info) * (scount + 1));
        }

        line = strtok(NULL, "\r\n");
    }

    free(data);
    memset(&servlist[scount], 0, sizeof (struct server_info));
    return servlist;
}


static void
write_server_list(struct server_info *servlist)
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


static void
free_server_list(struct server_info *list)
{
    int i;

    for (i = 0; list[i].hostname; i++) {
        free((void *)list[i].hostname);
        free((void *)list[i].username);
        free((void *)list[i].password);
    }
    free(list);
}

static int
get_username_password(const char *hostbuf, int port, char *userbuf,
                      char *passbuf)
{
    int passok, accountok, ret;
    char passbuf2[BUFSZ];

    do {
        curses_getline("Username (new or existing account):", userbuf);
        if (userbuf[0] == '\033' || userbuf[0] == '\0')
            return 0;

        do {
            curses_getline_pw
                ("Password: (beware - it is transmitted in plain text)",
                 passbuf);
            if (passbuf[0] == '\033' || passbuf[0] == '\0')
                return 0;

            /* Don't re-ask password unless the account is new */
            ret = nhnet_connect(hostbuf, port, userbuf, passbuf, NULL, 0);
            nhnet_disconnect();
            passok = TRUE;

            if (ret == AUTH_FAILED_UNKNOWN_USER) {
                curses_getline_pw("Confirm password:", passbuf2);
                if (passbuf2[0] == '\033' || passbuf2[0] == '\0')
                    return 0;

                if (strcmp(passbuf, passbuf2)) {
                    passok = FALSE;
                    curses_msgwin("The passwords didn't match.");
                }
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
    return 1;
}

static struct server_info *
add_server_menu(struct server_info **servlist)
{
    int i, port, hostok;
    char hostbuf[BUFSZ], portbuf[BUFSZ], userbuf[BUFSZ], passbuf[BUFSZ];

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

    for (i = 0; (*servlist)[i].hostname; i++) ;
    *servlist = realloc(*servlist, sizeof (struct server_info) * (i + 2));
    memmove(&(*servlist)[1], &(*servlist)[0],
            sizeof (struct server_info) * (i + 1));

    if (!get_username_password(hostbuf, port, userbuf, passbuf))
        return NULL;

    (*servlist)[0].hostname = strdup(hostbuf);
    (*servlist)[0].port = port;
    (*servlist)[0].username = strdup(userbuf);
    (*servlist)[0].password = strdup(passbuf);

    write_server_list(*servlist);

    return &(*servlist)[0];
}


static void
list_servers(struct server_info *servlist, struct nh_menuitem **items,
             int *size, int *icount)
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

        add_menu_item(*items, *size, *icount, i + 1, buf, 0, 0);
    }
}

static void
delete_server_menu(struct server_info *servlist)
{
    struct nh_menuitem *items;
    int icount, size, n, id, selected[1];

    icount = 0;
    size = 10;
    items = malloc(size * sizeof (struct nh_menuitem));

    list_servers(servlist, &items, &size, &icount);

    n = curses_display_menu(items, icount, "Delete which server?", PICK_ONE,
                            PLHINT_ANYWHERE, selected);
    free(items);

    if (n <= 0)
        return;

    id = selected[0] - 1;
    memmove(&servlist[id], &servlist[id + 1],
            (icount - id) * sizeof (struct server_info));

    write_server_list(servlist);
}


static struct server_info *
connect_server_menu(struct server_info **servlist)
{
    struct nh_menuitem *items;
    int icount, size, n, selected[1];
    struct server_info *server;

    while (1) {
        icount = 0;
        size = 10;
        items = malloc(size * sizeof (struct nh_menuitem));

        list_servers(*servlist, &items, &size, &icount);

        add_menu_txt(items, size, icount, "", MI_NORMAL);
        add_menu_item(items, size, icount, -1, "Add server", '!', 0);
        add_menu_item(items, size, icount, -2, "Delete server", '#', 0);

        n = curses_display_menu(items, icount, "Connect to which server?",
                                PICK_ONE, PLHINT_ANYWHERE, selected);
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
            return &(*servlist)[selected[0] - 1];

    }
    return NULL;
}


static int
connect_server(struct server_info *server)
{
    int ret;
    char buf[BUFSZ];

    while (1) {
        ret =
            nhnet_connect(server->hostname, server->port, server->username,
                          server->password, NULL, 0);
        if (ret == AUTH_SUCCESS_NEW) {
            /* only copy into ui_flags.username once the connection has been
               accepted */
            strcpy(ui_flags.username, server->username);
            return TRUE;
        } else if (ret == AUTH_SUCCESS_RECONNECT) {
            /* TODO: This case should never happen, and probably /does/ never
               happen, in which case this is dead code. */
            nhnet_exit_game(EXIT_FORCE_SAVE);
            if (!nhnet_connected())     /* disconnect due to an error while
                                           reconnecting */
                return FALSE;
            strcpy(ui_flags.username, server->username);
            return TRUE;
        } else if (ret == NO_CONNECTION) {
            curses_msgwin("Connection attempt failed");
            return FALSE;
        } else if (ret == AUTH_FAILED_BAD_PASSWORD) {
            curses_msgwin("Authentication failed: Wrong password.");
            curses_getline("Password:", buf);
            if (buf[0] == '\033' || buf[0] == '\0')
                return FALSE;
            free((void *)server->password);
            server->password = strdup(buf);
            continue;
        } else {        /* AUTH_FAILED_UNKNOWN_USER */
            sprintf(buf, "The account \"%s\" will be created for you.",
                    server->username);
            curses_msgwin(buf);
            curses_getline
                ("(Optional) You may give an email address for password resets:",
                 buf);
            ret =
                nhnet_connect(server->hostname, server->port, server->username,
                              server->password, buf, TRUE);
            if (ret != AUTH_SUCCESS_NEW) {
                curses_msgwin("Sorry, the registration failed.");
                return FALSE;
            } else
                return TRUE;
        }
    }

    /* Successful connection; reload options in case server has different
       perception of valid options than client does. */
    read_nh_config();
}


static void
netgame_mainmenu(struct server_info *server)
{
    int menuresult[1];
    char buf[BUFSZ];
    int n = 1, logoheight, i;
    const char **nhlogo;
    char verstr[32], server_verstr[32];
    const char *const *copybanner = nh_get_copyright_banner();

    static struct nh_menuitem netmenu_items[] = {
        {NEWGAME, MI_NORMAL, "new game", 'n', 0, 0},
        {LOAD, MI_NORMAL, "load game", 'l', 0, 0},
        {REPLAY, MI_NORMAL, "view replay", 'v', 0, 0},
        {OPTIONS, MI_NORMAL, "set options", 'o', 0, 0},
        {TOPTEN, MI_NORMAL, "show score list", 's', 0},
        {ACCOUNT, MI_NORMAL, "account settings", 'a', 0},
        {DISCONNECT, MI_NORMAL, "disconnect", 'q', 'x', 0}
    };

    sprintf(verstr, "Client version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR,
            PATCHLEVEL);
    sprintf(server_verstr, "Server version: %d.%d.%d", nhnet_server_ver.major,
            nhnet_server_ver.minor, nhnet_server_ver.patchlevel);

    /* In connection-only mode, we can't read the config file until we're
       already logged into the server. So do it now. */
    if (ui_flags.connection_only)
        read_ui_config();

    while (n > 0) {
        if (COLS >= 100) {
            nhlogo = nhlogo_large;
            logoheight = sizeof (nhlogo_large) / sizeof (nhlogo_large[0]);
        } else {
            nhlogo = nhlogo_small;
            logoheight = sizeof (nhlogo_small) / sizeof (nhlogo_small[0]);
        }
        wclear(basewin);
        wattron(basewin, A_BOLD | COLOR_PAIR(4));
        for (i = 0; i < logoheight; i++) {
            wmove(basewin, i, (COLS - strlen(nhlogo[0])) / 2);
            waddstr(basewin, nhlogo[i]);
        }
        wattroff(basewin, A_BOLD | COLOR_PAIR(4));
        mvwaddstr(basewin, LINES - 3, 0, copybanner[0]);
        mvwaddstr(basewin, LINES - 2, 0, copybanner[1]);
        mvwaddstr(basewin, LINES - 1, 0, copybanner[2]);
        mvwaddstr(basewin, LINES - 5, COLS - strlen(verstr), verstr);
        mvwaddstr(basewin, LINES - 4, COLS - strlen(verstr), server_verstr);
        wrefresh(basewin);


        menuresult[0] = DISCONNECT;     /* default action */
        snprintf(buf, BUFSZ, "%s on %s:", server->username, server->hostname);
        if (ui_flags.connection_only)
            snprintf(buf, BUFSZ, "Logged in as %s:", server->username);
        n = curses_display_menu_core(netmenu_items, ARRAY_SIZE(netmenu_items),
                                     buf, PICK_ONE, menuresult, 0, logoheight,
                                     COLS, LINES - 3, NULL);

        switch (menuresult[0]) {
        case NEWGAME:
            rungame(TRUE);
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


void
netgame(void)
{
    struct server_info *servlist, *server;
    struct server_info localserver = { 0, 0, 0, 0 };

    nhnet_lib_init(&curses_windowprocs);

    if (ui_flags.connection_only) {
        char *username, *password;

        servlist = NULL;
        server = &localserver;
        localserver.hostname = strdup("::1");
        username = malloc(BUFSZ);
        password = malloc(BUFSZ);
        localserver.username = username;
        localserver.password = password;
        if (!get_username_password(localserver.hostname, 0, username, password))
            goto finally;
    } else {
        servlist = read_server_list();
        server = connect_server_menu(&servlist);
    }
    if (!server || !connect_server(server))
        goto finally;

    free_displaychars();        /* remove old display info */
    init_displaychars();        /* load new display info from the server */

    load_keymap();              /* load command info from the server */

    netgame_mainmenu(server);
    if (!ui_flags.connection_only)
        write_server_list(servlist);
    nhnet_disconnect();

    free_keymap();

finally:
    if (servlist)
        free_server_list(servlist);
    if (ui_flags.connection_only) {
        free(localserver.hostname);
        free(localserver.username);
        free(localserver.password);
    }
    nhnet_lib_exit();
    free_displaychars();        /* remove server display info */
    init_displaychars();        /* go back to local object/monster/... lists */
}

/* netgame.c */
