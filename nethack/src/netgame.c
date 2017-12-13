/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
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
        /* decode blocks; padding '=' are converted to 0 in the decoding table
           */
        out[pos] = b64d[(int)in[i]] << 2 | b64d[(int)in[i + 1]] >> 4;
        out[pos + 1] = b64d[(int)in[i + 1]] << 4 | b64d[(int)in[i + 2]] >> 2;
        out[pos + 2] =
            ((b64d[(int)in[i + 2]] << 6) & 0xc0) | b64d[(int)in[i + 3]];
        pos += 3;
    }

    out[pos] = 0;
}

static void
confirm_email_change(const char *buf, void *unused)
{
    (void) unused;
    if (*buf != '\033' &&
        (*buf || curses_yn_function_internal(
            "Remove current email address?", "yn", 'n') == 'y'))
        nhnet_change_email(buf);
}

/* getline_pw callback; takes in a previously entered new password, compares it
   to the confirmation of the new password, NULLs out the old password if
   they don't match to signal to the caller not to change it */
static void
passwords_match_callback(const char *buf1, void *other_password_void)
{
    char **other_password_p = other_password_void;
    char *buf2 = *other_password_p;
    
    if (buf2[0] == '\033' || buf2[0] == '\0') {
        curses_msgwin("The password set request was cancelled.",
                      krc_notification);
        *other_password_p = NULL;
    } else if (strcmp(buf1, buf2)) {
        curses_msgwin("The passwords didn't match. No new password was "
                      "set.", krc_notification);
        *other_password_p = NULL;
    }
}

static void
confirm_set_password(const char *buf, void *server_void)
{
    struct server_info *server = server_void;

    if (!*buf || *buf == '\033')
        return;

    curses_getline_pw("Confirm password:", &buf, passwords_match_callback);

    if (buf) {
        nhnet_change_password(buf);
        free((void *)server->password);
        server->password = strdup(buf);
    }
}

static void
change_password_callback(const char *buf, void *server_void)
{
    struct server_info *server = server_void;

    if (strcmp(server->password, buf)) {
        if (*buf && *buf != '\033')
            curses_msgwin("Incorrect password.", krc_notification);
        return;
    }

    curses_getline_pw(
        "New password: (Beware - it is transmitted in plain text)",
        server, confirm_set_password);
}

static void
account_menu(struct server_info *server)
{
    int menuresult[1] = {0};

    static struct nh_menuitem netmenu_items[] = {
        {1, MI_NORMAL, 0, "change email address", 'e', 0, 0},
        {2, MI_NORMAL, 0, "change password", 'p', 0, 0},
        {0, MI_NORMAL, 0, "", 0, 0, 0},
        {3, MI_NORMAL, 0, "back to main menu", 'x', 0, 0}
    };

    while (*menuresult != CURSES_MENU_CANCELLED) {
        menuresult[0] = 3;      /* default action */
        curses_display_menu(
            STATIC_MENULIST(netmenu_items), "Account settings:",
            PICK_ONE, PLHINT_ANYWHERE, menuresult, curses_menu_callback);

        switch (menuresult[0]) {
        case 1:
            curses_getline("What email address do you want to use?",
                           NULL, confirm_email_change);
            break;

        case 2:
            curses_getline_pw("Current password:", server,
                              change_password_callback);
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
    char *line;
    int size, scount, port, n;
    struct server_info *servlist;
    FILE *fp;

    scount = 0;
    servlist = malloc(sizeof (struct server_info));
    memset(&servlist[scount], 0, sizeof (struct server_info));

    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
        return servlist;
    fnncat(filename, FN("servers.conf"), BUFSZ - fnlen(filename) - 1);

    fp = fopen(filename, "rb");
    if (!fp)
        return servlist;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    char data[size + 1];
    if (fread(data, 1, size, fp) < size) {
        fclose(fp);
        curses_msgwin("warning: servers.conf is corrupted", krc_notification);
        return servlist;
    }
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
    fnncat(filename, FN("servers.conf"), BUFSZ - fnlen(filename) - 1);

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


static void
getlin_strdup_callback(const char *buf, void *target_void)
{
    char **target = target_void;
    if (*buf == '\033' || !*buf)
        *target = NULL;
    else
        *target = strdup(buf);
}

static int
rangeclamp(long val, int min, int max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return (int)val;
}

static void
getlin_positive_int_callback(const char *buf, void *target_void)
{
    int *target = target_void;
    if (*buf == '\033' || !*buf)
        *target = -1;
    else
        *target = rangeclamp(strtol(buf, NULL, 0), 0, INT_MAX);
}

/* Given the host and port of a server, fills out the username and password. If
   returning 0 for failure, no allocations will be done; otherwise, the caller
   is responsible for freeing the username and password, which will be
   malloc'ed. */
static int
get_username_password(struct server_info *server)
{
    int passok, accountok = FALSE, ret;

    server->username = NULL;
    server->password = NULL;

    do {
        if (server->username) {
            free(server->username);
            server->username = NULL;
        }
        if (server->password) {
            free(server->password);
            server->password = NULL;
        }

        curses_getline("Username (new or existing account):",
                       &(server->username), getlin_strdup_callback);
        if (!server->username)
            return 0;

        do {
            curses_getline_pw(
                "Password: (beware - it is transmitted in plain text)",
                &(server->password), getlin_strdup_callback);
            if (!server->password) {
                free(server->username);
                server->username = NULL;
                return 0;
            }

            /* Don't re-ask password unless the account is new */
            ret = nhnet_connect(server->hostname, server->port,
                                server->username, server->password, NULL, 0);
            nhnet_disconnect();
            passok = TRUE;

            if (ret == AUTH_FAILED_UNKNOWN_USER) {
                char *pcopy = server->password;
                curses_getline_pw("Confirm password:", &pcopy,
                                  passwords_match_callback);
                if (!pcopy)
                    passok = FALSE;
            }
        } while (!passok);

        switch (ret) {
        case AUTH_FAILED_BAD_PASSWORD:
            curses_msgwin("The account exists but this is the wrong password.",
                          krc_notification);
            break;

        case NO_CONNECTION:
            curses_msgwin(
                "Error: could not establish a connection.", krc_notification);
            break;

        case AUTH_FAILED_UNKNOWN_USER:
        case AUTH_SUCCESS_NEW:
            accountok = TRUE;
            break;
        }
        nhnet_disconnect();    

    } while (!accountok);
    return 1;
}

static struct server_info *
add_server_menu(struct server_info **servlist)
{
    int i, hostok;
    struct server_info server = {0, 0, 0, 0};

    do {
        curses_getline("Hostname or IP address:", &(server.hostname),
                       getlin_strdup_callback);
        if (!server.hostname)
            return NULL;
        curses_getline("Port number (0 = use default):",
                       &(server.port), getlin_positive_int_callback);
        if (server.port < 0) {
            free(server.hostname);
            return NULL;
        }

        hostok = FALSE;
        if (nhnet_connect(server.hostname, server.port,
                          "", "", NULL, 0) != NO_CONNECTION) {
            hostok = TRUE;
            nhnet_disconnect();
        } else {
            curses_msgwin("Connection test failed", krc_notification);
            free(server.hostname);
        }
    } while (!hostok);

    if (!get_username_password(&server)) {
        free(server.hostname);
        return NULL;
    }

    for (i = 0; (*servlist)[i].hostname; i++)
        ;
    *servlist = realloc(*servlist, sizeof (struct server_info) * (i + 2));
    memmove(&(*servlist)[1], &(*servlist)[0],
            sizeof (struct server_info) * (i + 1));

    (*servlist)[0] = server;

    write_server_list(*servlist);

    return &(*servlist)[0];
}


static void
list_servers(struct server_info *servlist, struct nh_menulist *menu)
{
    char buf[BUFSZ];
    int i;

    for (i = 0; servlist[i].hostname; i++) {
        if (servlist[i].port) {
            if (!strchr(servlist[i].hostname, ':'))
                snprintf(buf, ARRAY_SIZE(buf), "%s on %s:%d", servlist[i].username,
                        servlist[i].hostname, servlist[i].port);
            else
                snprintf(buf, ARRAY_SIZE(buf), "%s on [%s]:%d", servlist[i].username,
                        servlist[i].hostname, servlist[i].port);
        } else
            snprintf(buf, ARRAY_SIZE(buf), "%s on %s", servlist[i].username,
                    servlist[i].hostname);

        add_menu_item(menu, i + 1, buf, 0, 0);
    }
}

static void
delete_server_menu(struct server_info *servlist)
{
    struct nh_menulist menu;
    int id, icount, selected[1];

    init_menulist(&menu);

    list_servers(servlist, &menu);

    icount = menu.icount;

    curses_display_menu(&menu, "Delete which server?", PICK_ONE,
                        PLHINT_ANYWHERE, selected, curses_menu_callback);

    if (*selected == CURSES_MENU_CANCELLED)
        return;

    id = selected[0] - 1;
    memmove(&servlist[id], &servlist[id + 1],
            (icount - id) * sizeof (struct server_info));

    write_server_list(servlist);
}


static struct server_info *
connect_server_menu(struct server_info **servlist)
{
    struct nh_menulist menu;
    int selected[1];
    struct server_info *server;

    while (1) {
        init_menulist(&menu);

        list_servers(*servlist, &menu);

        add_menu_txt(&menu, "", MI_NORMAL);
        add_menu_item(&menu, -1, "Add server", '!', 0);
        add_menu_item(&menu, -2, "Delete server", '#', 0);

        curses_display_menu(&menu, "Connect to which server?", PICK_ONE,
                            PLHINT_ANYWHERE, selected, curses_menu_callback);

        if (*selected == CURSES_MENU_CANCELLED)
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
    nh_bool ok;

    while (1) {
        ret =
            nhnet_connect(server->hostname, server->port, server->username,
                          server->password, NULL, 0);
        if (ret == AUTH_SUCCESS_NEW) {
            /* only copy into ui_flags.username once the connection has been
               accepted */
            strcpy(ui_flags.username, server->username);
            break;
        } else if (ret == NO_CONNECTION) {
            curses_msgwin("Connection attempt failed", krc_notification);
            return FALSE;
        } else if (ret == AUTH_FAILED_BAD_PASSWORD) {
            curses_msgwin("Authentication failed: Wrong password.",
                          krc_notification);
            char *newpw;
            curses_getline_pw("Password:", &newpw, getlin_strdup_callback);
            if (!newpw)
                return FALSE;
            free(server->password);
            server->password = newpw;
            continue;
        } else {        /* AUTH_FAILED_UNKNOWN_USER */
            char buf[sizeof "The account \"\" will be created for you." +
                     strlen(server->username) + 1];
            sprintf(buf, "The account \"%s\" will be created for you.",
                    server->username);
            curses_msgwin(buf, krc_notification);
            char *newemail;
            curses_getline("(Optional) You may give an email address for "
                           "password resets:", &newemail,
                           getlin_strdup_callback);
            ret = nhnet_connect(server->hostname, server->port,
                                server->username, server->password,
                                newemail, 1);
            free(newemail);
            if (ret != AUTH_SUCCESS_NEW) {
                curses_msgwin("Sorry, the registration failed.",
                              krc_notification);
                return FALSE;
            } else {
                strcpy(ui_flags.username, server->username);
                break;
            }
        }
    }

    /* Successful connection; reload options in case server has different
       perception of valid options than client does. */
    ok = read_nh_config();
    if (!ok) {
        nhnet_disconnect();
        curses_msgwin("The connection to the server was lost.",
                      krc_notification);
        return FALSE;
    }

    return TRUE;
}


static void
netgame_mainmenu(struct server_info *server)
{
    int menuresult[1];
    char buf[BUFSZ];
    int logoheight, i;
    const char **nhlogo;
    char verstr[32], server_verstr[32];
    const char *const *copybanner = nh_get_copyright_banner();

    static struct nh_menuitem netmenu_items[] = {
        {NEWGAME, MI_NORMAL, 0, "new game", 'n', 0, 0},
        {LOAD, MI_NORMAL, 0, "load game", 'l', 0, 0},
        {REPLAY, MI_NORMAL, 0, "view a game", 'v', 0, 0},
        {OPTIONS, MI_NORMAL, 0, "set options", 'o', 0, 0},
        {TOPTEN, MI_NORMAL, 0, "show score list", 's', 0},
        {ACCOUNT, MI_NORMAL, 0, "account settings", 'a', 0},
        {DISCONNECT, MI_NORMAL, 0, "disconnect", 'q', 'x', 0}
    };

    snprintf(verstr, ARRAY_SIZE(verstr), "Client version: %d.%d.%d",
             VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
    snprintf(server_verstr, ARRAY_SIZE(server_verstr),
             "Server version: %d.%d.%d", nhnet_server_ver.major,
             nhnet_server_ver.minor, nhnet_server_ver.patchlevel);

    /* In connection-only mode, we can't read the config file until we're
       already logged into the server. So do it now. */
    if (ui_flags.connection_only) {
        read_ui_config();
        read_nh_config();
    }

    while (1) {
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
        wnoutrefresh(basewin);


        menuresult[0] = DISCONNECT;     /* default action */
        snprintf(buf, BUFSZ, "%s on %s:", server->username, server->hostname);
        if (ui_flags.connection_only)
            snprintf(buf, BUFSZ, "Logged in as %s:", server->username);
        curses_display_menu_core(
            STATIC_MENULIST(netmenu_items), buf, PICK_ONE, menuresult,
            curses_menu_callback, 0, logoheight, COLS, LINES - 3,
            FALSE, NULL, TRUE);

        switch (menuresult[0]) {
        case CURSES_MENU_CANCELLED:
            return;

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
    int fd;

    nhnet_lib_init(&curses_windowprocs);

    if (ui_flags.connection_only) {
        servlist = NULL;
        server = &localserver;
        localserver.hostname = strdup("::1");
        if (!get_username_password(&localserver))
            goto finally;
    } else {
        servlist = read_server_list();
        server = connect_server_menu(&servlist);
    }
    if (!server || !connect_server(server))
        goto finally;

    fd = nhnet_get_socket_fd();
    if (fd == -1) {
        curses_msgwin("The connection to the server was lost.",
                      krc_notification);
        nhnet_disconnect();
        goto finally;
    }

    ui_flags.connected_to_server = 1;
    uncursed_watch_fd(fd);

    free_displaychars();        /* remove old display info */
    init_displaychars();        /* load new display info from the server */

    load_keymap();              /* load command info from the server */

    netgame_mainmenu(server);
    if (!ui_flags.connection_only)
        write_server_list(servlist);

    uncursed_unwatch_fd(fd);
    ui_flags.connected_to_server = 0;
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
