/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include "common_options.h"
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#if !defined(WIN32)
# include <dirent.h>
# include <libgen.h>
#endif


#if defined(WIN32)

nh_bool
get_gamedir(enum game_dirs dirtype, wchar_t *buf)
{
    wchar_t *subdir;
    wchar_t appPath[MAX_PATH], nhPath[MAX_PATH];

    if (override_userdir) {
        /* TODO: make override_userdir a wchar_t on Windows. (This means using
           an alternative command line parser.) */
        wchar_t *r1 = nhPath;
        char *r2 = override_userdir;

        while (*r2)
            *r1++ = *r2++;
    } else {
        /* Get the location of "AppData\Roaming" (Vista, 7) or "Application
           Data" (XP). The returned Path does not include a trailing backslash. 
         */
        if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appPath)))
            return FALSE;
    }

    switch (dirtype) {
    case CONFIG_DIR:
        subdir = L"\\";
        break;
    case SAVE_DIR:
        subdir = L"\\save\\";
        break;
    case LOG_DIR:
        subdir = L"\\log\\";
        break;
    case DUMP_DIR:
        subdir = L"\\dumps\\";
        break;
    case TILESET_DIR:
        subdir = L"\\tilesets\\";
        break;
    default:
        return FALSE;
    }

    if (!override_userdir)
        snwprintf(nhPath, MAX_PATH, L"%s\\NetHack4", appPath);
    _wmkdir(nhPath);

    snwprintf(buf, BUFSZ, L"%s%s", nhPath, subdir);
    _wmkdir(buf);

    return TRUE;
}

/* Non-unicode version of the above. The code duplication is annoying, but
   deduplicating it would be even messier; there are too many subtle
   differences. */
nh_bool
get_gamedirA(enum game_dirs dirtype, char *buf)
{
    char *subdir;
    char appPath[MAX_PATH], nhPath[MAX_PATH];

    if (override_userdir) {
        strncpy(nhPath, override_userdir, MAX_PATH);
    } else {
        /* Get the location of "AppData\Roaming" (Vista, 7) or "Application
           Data" (XP). The returned Path does not include a trailing backslash. 
         */
        if (!SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appPath)))
            return FALSE;
    }

    switch (dirtype) {
    case CONFIG_DIR:
        subdir = "\\";
        break;
    case SAVE_DIR:
        subdir = "\\save\\";
        break;
    case LOG_DIR:
        subdir = "\\log\\";
        break;
    case DUMP_DIR:
        subdir = "\\dumps\\";
        break;
    case TILESET_DIR:
        subdir = "\\tilesets\\";
        break;
    default:
        return FALSE;
    }

    if (!override_userdir)
        snprintf(nhPath, MAX_PATH, "%s\\NetHack4", appPath);
    mkdir(nhPath);

    snprintf(buf, BUFSZ, "%s%s", nhPath, subdir);
    mkdir(buf);

    return TRUE;
}

#else /* defined(WIN32) */

nh_bool
get_gamedir(enum game_dirs dirtype, char *buf)
{
    const char *envval, *subdir;
    mode_t mask;

    switch (dirtype) {
    case CONFIG_DIR:
        subdir = "";
        break;
    case SAVE_DIR:
        subdir = "save/";
        break;
    case LOG_DIR:
        subdir = "log/";
        break;
    case DUMP_DIR:
        subdir = "dumps/";
        break;
    case TILESET_DIR:
        subdir = "tilesets/";
        break;
    default:
        return FALSE;
    }

    if (override_userdir && getgid() == getegid()) {
        snprintf(buf, BUFSZ, "%s/%s", override_userdir, subdir);
    } else {
        /* look in regular location */
        envval = getenv("XDG_CONFIG_HOME");
        if (envval)
            snprintf(buf, BUFSZ, "%s/NetHack4/%s", envval, subdir);
        else {
            envval = getenv("HOME");
            if (!envval)        /* HOME not set? just give up... */
                return FALSE;
            snprintf(buf, BUFSZ, "%s/.config/NetHack4/%s", envval, subdir);
        }
    }

    mask = umask(0);
    if (mkdir(buf, 0755) == -1 && errno != EEXIST) {
        /* try to create the parent directory too. This ist the only problem we
           can fix here - permission problems etc. all requre user intervention
           */
        char dirbuf[BUFSZ], *basedir;

        strcpy(dirbuf, buf);
        basedir = dirname(dirbuf);

        mkdir(basedir, 0755);   /* no need to check the return value: if it
                                   doesn't work, we're screwed */
        if (mkdir(buf, 0755) == -1) {
            umask(mask);
            return FALSE;
        }
    }
    umask(mask);

    return TRUE;
}

nh_bool
get_gamedirA(enum game_dirs dirtype, char *buf)
{
    return get_gamedir(dirtype, buf);
}

#endif


static int welcomed;
void
curses_request_command(
    nh_bool debug, nh_bool completed, nh_bool interrupted,
    void *callbackarg, void (*callback)(const struct nh_cmd_and_arg *, void *))
{
    struct nh_cmd_arg arg = {.argtype = 0};

    if (!welcomed) {
        callback(&(struct nh_cmd_and_arg){"welcome", arg}, callbackarg);
        welcomed = 1;
        return;
    }

    /*
     * The behaviour of multiple-turn commands in the client is as follows:
     * - If no count is given, we continue until the command is completed or
     *   interrupted.
     * - If a count is given, we continue for the given number of turns or
     *   until interrupted, ignoring the completed status.
     * - When interrupted, we set the count back to 0 /unless/ the next command
     *   entered by the user is "repeat", in which case we act as though we
     *   weren't interrupted.
     */
    if ((!interrupted && !completed && !repeats_remaining) ||
        (!interrupted && repeats_remaining && --repeats_remaining)) {
        if (settings.animation == ANIM_INSTANT ||
            get_map_key(TRUE, TRUE, krc_interrupt_long_action) == ERR) {
            callback(&(struct nh_cmd_and_arg){"repeat", arg}, callbackarg);
            return;
        }
    }

    /* This clears/sets repeats_remaining except when returning "repeat",
       where it maintains it. */
    get_command(callbackarg, callback, debug);
}


/* Called when detaching from a game. In network play, net is TRUE and filename
   is null; otherwise, filename is the game's filename and net is FALSE. */
void
game_ended(int status, fnchar *filename, nh_bool net)
{
#ifndef WIN32
    fnchar fncopy[1024], *fname;
#endif
    fnchar logname[1024], savedir[BUFSZ], *bp;

    switch (status) {
    case GAME_DETACHED:
    case GAME_ALREADY_OVER:
        /* Normal termination, due to saving a running game, or ceasing to
           load a completed game. */
        return;

    case GAME_OVER:
        /* Handled below. */
        break;

    /* Error statuses */
    case ERR_BAD_ARGS:
        curses_raw_print("Error: Could not find the save file.");
        return;
    case ERR_BAD_FILE:
        curses_raw_print("Error: This does not look like a NetHack 4 save "
                         "file.");
        return;
    case ERR_IN_PROGRESS:
        curses_raw_print("Error: Could not attach to the game file.");
        curses_raw_print(
            "(Maybe someone else is playing it, or a server restarted?)");
        return;
    case ERR_RESTORE_FAILED:
        curses_raw_print("Error: This game requires manual recovery.");
        if (net)
            curses_raw_print("Please contact the server administrator.");
        else
            curses_raw_print("If you cannot recover it yourself, contact "
                             "the NetHack 4 developers for advice.");
        return;
    case ERR_RECOVER_REFUSED:
        /* The user has declined recovery, so we've already had a message
           printed. */
        return;
    case ERR_CREATE_FAILED:
        curses_raw_print("Error: Could not create the save file.");
        return;
    case ERR_NETWORK_ERROR:
        curses_raw_print("Error: Could not re-establish connection to server.");
        return;

        /* Impossible statuses: internal-only values like ERR_CREATE_FAILED, and
           also RESTART_PLAY, CLIENT_RESTART and REPLAY_FINISHED (which are
           meaningful to the client, but should not reach this function). */
    default:
        curses_impossible("Game ended in an impossible way?");
        return;
    }

    if (status != GAME_OVER)
        return;

    show_topten(player.plname, settings.end_top, settings.end_around,
                settings.end_own);

    if (net)
        return;

    /* The game ended terminally. Now it would be nice to move the saved game
       out of the save/ dir and into the log/ dir, since its only use now is as
       a trophy. */
#if !defined(WIN32)
    /* dirname and basename may modify the input string, depending on the
       system */
    strncpy(fncopy, filename, sizeof (fncopy));
    bp = dirname(fncopy);

    get_gamedir(SAVE_DIR, savedir);
    savedir[strlen(savedir) - 1] = '\0';        /* remove the trailing '/' */
    if (strcmp(bp, savedir) != 0)
        return; /* file was not in savedir, so don't touch it */

    get_gamedir(LOG_DIR, logname);
    strncpy(fncopy, filename, sizeof (fncopy));
    fname = basename(fncopy);
    strncat(logname, fname, sizeof (logname) - strlen (logname) - 1);

    /* don't care about errors: rename is nice to have, not essential */
    rename(filename, logname);
#else
    bp = wcsrchr(filename, L'\\');
    get_gamedir(SAVE_DIR, savedir);
    if (!bp || wcsncmp(filename, savedir, wcslen(savedir)))
        return;

    get_gamedir(LOG_DIR, logname);
    wcsncat(logname, bp + 1, 1024);

    /* don't care about errors: rename is nice to have, not essential */
    _wrename(filename, logname);
#endif
}


static void
plname_handler(const char *str, void *plname_void)
{
    char **plname = plname_void;

    if (*str == '\033') /* cancelled */
        return;

    if (*str && strlen(str) < PL_NSIZ) { /* ok */
        *plname = strdup(str);
        return;
    }

    if (*str)
        curses_msgwin("That name is too long.", krc_notification);

    *plname = strdup("");
    return;
}

void
rungame(nh_bool net)
{
    int ret, role = cmdline_role, race = cmdline_race, gend = cmdline_gend,
        align = cmdline_align, playmode = ui_flags.playmode;
    int fd = -1;
    char prompt[BUFSZ] = "You are a ";
    fnchar filename[1024];
    fnchar savedir[BUFSZ];
    long t;
    struct nh_roles_info *info;

    struct nh_option_desc *old_opts = curses_get_nh_opts(),
                          *new_opts = nhlib_clone_optlist(old_opts),
                          *seedopt = nhlib_find_option(new_opts, "seed"),
                          *polyopt = nhlib_find_option(new_opts, "polyinit"),
                          *roleopt = nhlib_find_option(new_opts, "role"),
                          *raceopt = nhlib_find_option(new_opts, "race"),
                          *alignopt = nhlib_find_option(new_opts, "align"),
                          *gendopt = nhlib_find_option(new_opts, "gender"),
                          *nameopt = nhlib_find_option(new_opts, "name"),
                          *modeopt = nhlib_find_option(new_opts, "mode");
    curses_free_nh_opts(old_opts);
    if (!roleopt || !raceopt || !alignopt || !gendopt || !nameopt || !modeopt ||
        !seedopt || !polyopt) {
        curses_raw_print("Creation options not available?");
        goto cleanup;
    }

    if (role == ROLE_NONE)
        role = roleopt->value.e;
    if (race == ROLE_NONE)
        race = raceopt->value.e;
    if (align == ROLE_NONE)
        align = alignopt->value.e;
    if (gend == ROLE_NONE)
        gend = gendopt->value.e;
    if (cmdline_name[0]) {
        if (nameopt->value.s)
            free(nameopt->value.s);
        nameopt->value.s = strdup(cmdline_name);
    }
    if (playmode == MODE_NORMAL)
        playmode = modeopt->value.e;

    if (!net && !get_gamedir(SAVE_DIR, savedir)) {
        curses_raw_print
            ("Could not find where to put the logfile for a new game.");
        goto cleanup;
    }

    /* Check to make sure that this game will be eligible for the high score
       table. If not, print a warning, to prevent accidental nonscoring
       games. */
    if (playmode != MODE_NORMAL || (seedopt->value.s && *seedopt->value.s) ||
        polyopt->value.e != -1) {
        char prompt[BUFSZ];
        snprintf(prompt, BUFSZ, "This game is not eligible for the high score "
                 "table due to starting in %s mode. Continue?",
                 playmode == MODE_EXPLORE ? "explore" :
                 playmode == MODE_WIZARD ? "debug" :
                 (seedopt->value.s && *seedopt->value.s) ? "set seed" :
                 (polyopt->value.e != -1) ? "polyinit" : "[unknown]");
        if (curses_yn_function_internal(prompt, "yn", 'y') != 'y')
            goto cleanup;
    }

    if (!player_selection(&role, &race, &gend, &align, random_player))
        goto cleanup;

    /* 
     * Describe the player character for naming; see dowelcome() in cmd.c in
     * libnethack.
     */
    info = nh_get_roles();
    sprintf(prompt + strlen(prompt), "%s", info->alignnames[align]);

    /* 
     * assert(info->num_gend == 2) or else we lose a bunch of assumptions here,
     * like that 0 and 1 are the correct gender identifiers.
     */
    if (!info->rolenames_f[role] && nh_cm_idx(*info, role, race, 0, align) &&
        nh_cm_idx(*info, role, race, 1, align))
        sprintf(prompt + strlen(prompt), " %s", info->gendnames[gend]);

    sprintf(prompt + strlen(prompt), " %s %s. What is your name?",
            info->racenames[race], (gend &&
                                    info->
                                    rolenames_f[role] ? info->rolenames_f :
                                    info->rolenames_m)[role]);

    if (nameopt->value.s && !*nameopt->value.s) {
        free(nameopt->value.s);
        nameopt->value.s = NULL;
    }

    if (!nameopt->value.s) {
        /* In wizard mode, the name is provided automatically by the engine. */
        if (playmode != MODE_WIZARD)
            do {
                /* We don't allow the null string as a name. */
                if (nameopt->value.s) {
                    free(nameopt->value.s);
                    nameopt->value.s = NULL;
                }
                curses_getline(prompt, &(nameopt->value.s), plname_handler);
            } while (nameopt->value.s && !*(nameopt->value.s));

        /* Make sure we always have an allocated name. */
        if (!nameopt->value.s) {
            char *nullname = malloc(1);
            *nullname = '\0';
            nameopt->value.s = nullname;
        }

        if (!*nameopt->value.s && playmode != MODE_WIZARD)
            goto cleanup; /* cancelled */
    }

    if (!net) {
        t = (long)time(NULL);
#if defined(WIN32)
        snwprintf(filename, 1024, L"%ls%ld_%hs.nhgame",
                  savedir, t, nameopt->value.s);
#else
        snprintf(filename, 1024, "%s%ld_%s.nhgame",
                 savedir, t, nameopt->value.s);
#endif
        fd = sys_open(filename, O_TRUNC | O_CREAT | O_RDWR, FILE_OPEN_MASK);
        if (fd == -1) {
            curses_raw_print("Could not create the logfile.");
            goto cleanup;
        }
    }

    create_game_windows();

    roleopt->value.i = role;
    raceopt->value.i = race;
    alignopt->value.i = align;
    gendopt->value.i = gend;
    modeopt->value.e = playmode;
     
    /* Create the game, then immediately load it. */
    ret = ERR_CREATE_FAILED;
    if (net) {
        fd = nhnet_create_game(new_opts);
        if (fd >= 0)
            ret = playgame(fd, FM_PLAY);
    } else {
        if (nh_create_game(fd, new_opts) == NHCREATE_OK)
            ret = playgame(fd, FM_PLAY);
    }

    close(fd);

    destroy_game_windows();
    discard_message_history(0);

    game_ended(ret, net ? NULL : filename, net);

cleanup:
    nhlib_free_optlist(new_opts);
}


void
describe_game(char *buf, enum nh_log_status status, struct nh_game_info *gi)
{
    const char *mode_desc[] = { "", "\t[explore]", "\t[wizard]", "\t[set seed]",
                                "\t[unknown game mode]" };
    int pm = gi->playmode;
    const char *game_state = gi->game_state;

    if (pm < 0 || pm > (sizeof mode_desc / sizeof *mode_desc))
        pm = (sizeof mode_desc / sizeof *mode_desc) - 1;
    if (status == LS_CRASHED)
        game_state = "(crashed)";
    if (status == LS_IN_PROGRESS)
        game_state = "(status unavailable)";

    snprintf(buf, BUFSZ,
             "%s\t%3.3s-%3.3s-%3.3s-%3.3s\t%s%s", gi->name,
             gi->plrole, gi->plrace, gi->plgend, gi->plalign,
             game_state, mode_desc[pm]);
}

#if defined(WIN32)


static int
compare_filetime(const void *arg1, const void *arg2)
{
    const WIN32_FIND_DATA *file1 = (const WIN32_FIND_DATA *)arg1;
    const WIN32_FIND_DATA *file2 = (const WIN32_FIND_DATA *)arg2;

    if (file1->ftLastWriteTime.dwHighDateTime !=
        file2->ftLastWriteTime.dwHighDateTime)
        return file2->ftLastWriteTime.dwHighDateTime -
            file1->ftLastWriteTime.dwHighDateTime;
    return file2->ftLastWriteTime.dwLowDateTime -
        file1->ftLastWriteTime.dwLowDateTime;
}


wchar_t **
list_gamefiles(wchar_t * dir, int *count)
{
    wchar_t filepattern[1024], fullname[1024], **filenames;
    WIN32_FIND_DATA *find_data;
    HANDLE h_search;
    int fd, i;
    enum nh_log_status status;

    wcsncpy(filepattern, dir, 1024);
    wcsncat(filepattern, L"*.nhgame", 1024);
    *count = 0;
    find_data = malloc(1 * sizeof (WIN32_FIND_DATA));

    h_search = FindFirstFile(filepattern, &find_data[0]);
    if (h_search == INVALID_HANDLE_VALUE)
        return NULL;

    do {
        if (find_data[*count].nFileSizeHigh == 0 &&
            find_data[*count].nFileSizeLow < 64)
            continue;   /* too small to be valid */

        _snwprintf(fullname, 1024, L"%s%s", dir, find_data[*count].cFileName);
        fd = _wopen(fullname, O_RDWR, _S_IREAD | _S_IWRITE);
        if (fd == -1)
            continue;

        status = nh_get_savegame_status(fd, NULL);
        close(fd);

        if (status != LS_INVALID) {
            (*count)++;
            find_data =
                realloc(find_data, (*count + 1) * sizeof (WIN32_FIND_DATA));
        }
    } while (FindNextFile(h_search, &find_data[*count]));

    if (h_search != INVALID_HANDLE_VALUE)
        FindClose(h_search);

    qsort(find_data, *count, sizeof (WIN32_FIND_DATA), compare_filetime);

    filenames = malloc(*count * sizeof (wchar_t *));
    for (i = 0; i < *count; i++) {
        _snwprintf(fullname, 1024, L"%s%s", dir, find_data[i].cFileName);
        filenames[i] = _wcsdup(fullname);
    }
    free(find_data);

    return filenames;
}

#else

static int
compare_filetime(const void *arg1, const void *arg2)
{
    const char *file1 = *(const char *const *)arg1;
    const char *file2 = *(const char *const *)arg2;

    struct stat s1, s2;

    stat(file1, &s1);
    stat(file2, &s2);

    return s2.st_mtime - s1.st_mtime;
}


char **
list_gamefiles(char *dir, int *count)
{
    char filename[1024], **files;
    DIR *dirp;
    struct dirent *dp;
    int namelen, fd;
    enum nh_log_status status;

    files = NULL;
    *count = 0;

    dirp = opendir(dir);
    while ((dp = readdir(dirp)) != NULL) {
        namelen = strlen(dp->d_name);
        if (namelen > 7 &&      /* ".nhgame" */
            !strcmp(&dp->d_name[namelen - 7], ".nhgame")) {
            snprintf(filename, sizeof (filename), "%s%s", dir, dp->d_name);

            fd = open(filename, O_RDWR, 0660);
            status = nh_get_savegame_status(fd, NULL);
            close(fd);

            if (status == LS_INVALID)
                continue;

            (*count)++;
            files = realloc(files, (*count) * sizeof (char *));
            files[*count - 1] = strdup(filename);
        }
    }
    closedir(dirp);

    if (files)
        qsort(files, *count, sizeof (char *), compare_filetime);

    return files;
}
#endif


nh_bool
loadgame(void)
{
    char buf[BUFSZ];
    fnchar savedir[BUFSZ], filename[1024], **files;
    int fd, i, size, ret, pick[1];
    enum nh_log_status status;
    struct nh_game_info gi;
    struct nh_menulist menu;

    if (!get_gamedir(SAVE_DIR, savedir)) {
        curses_raw_print("Could not find or create the save directory.");
        return FALSE;
    }

    files = list_gamefiles(savedir, &size);
    if (!size) {
        curses_msgwin("No saved games found.", krc_notification);
        return FALSE;
    }

    init_menulist(&menu);

    for (i = 0; i < size; i++) {
        fd = sys_open(files[i], O_RDWR, FILE_OPEN_MASK);
        status = nh_get_savegame_status(fd, &gi);
        close(fd);

        describe_game(buf, status, &gi);
        add_menu_item(&menu, (status == LS_IN_PROGRESS) ? 0 : menu.icount + 1,
                      buf, 0, FALSE);
    }

    curses_display_menu(&menu, "saved games", PICK_ONE,
                        PLHINT_ANYWHERE, pick, curses_menu_callback);

    filename[0] = '\0';
    if (pick[0] != CURSES_MENU_CANCELLED)
        fnncat(filename, files[pick[0] - 1],
               sizeof (filename) / sizeof (fnchar) - fnlen(filename) - 1);

    for (i = 0; i < size; i++)
        free(files[i]);
    free(files);

    if (pick[0] == CURSES_MENU_CANCELLED)
        return FALSE;

    fd = sys_open(filename, O_RDWR, FILE_OPEN_MASK);
    create_game_windows();

    ret = playgame(fd, FM_PLAY);

    close(fd);

    destroy_game_windows();
    discard_message_history(0);
    game_ended(ret, filename, FALSE);

    return TRUE;
}


enum nh_play_status
playgame(int fd_or_gameno, enum nh_followmode followmode)
{
    enum nh_play_status ret;
    int reconnect_tries_upon_network_error = 3;

    ui_flags.current_followmode =
        ui_flags.available_followmode = followmode;
    ui_flags.gameload_message = NULL;

    game_is_running = TRUE;
    welcomed = 0;
    do {
        ui_flags.in_zero_time_command = FALSE;
        ret = nh_play_game(fd_or_gameno, ui_flags.current_followmode);

        /* Clean up any game windows that might be lying around.  This can
           happen if the server or client cancels a menu or prompt. */
        delete_all_gamewins();

        if (ret == CLIENT_RESTART) {
            /* We send this to ourself to change followmode.  In this case,
               current_followmode and available_followmode will both already
               be correct (and we already have a gameload_message). */
            ret = RESTART_PLAY;
        } else if (ret == REPLAY_FINISHED) {
            /* We're going to have to restart, and maybe change followmode. */
            if (ui_flags.current_followmode != FM_REPLAY) {
                curses_raw_print("Error: We reached the end of a replay, "
                                 "but weren't replaying?");
                ret = GAME_DETACHED;
            } else if (ui_flags.available_followmode != FM_REPLAY) {
                ui_flags.current_followmode = ui_flags.available_followmode;
                if (ui_flags.current_followmode == FM_PLAY)
                    ui_flags.gameload_message =
                        "The replay has caught up to the current situation of "
                        "the game.  You are now playing live.";
                else
                    ui_flags.gameload_message =
                        "The replay has caught up to the current situation of "
                        "the game.  You are now watching live.";
                ret = RESTART_PLAY;
            } else {
                ui_flags.gameload_message = "That was the end of the replay.";
                ret = RESTART_PLAY;
            }
        } else if (ret == GAME_DETACHED && ui_flags.current_followmode !=
                   ui_flags.available_followmode) {
            /* Exiting a replay that was nested inside a play or watch. */
            ui_flags.current_followmode = ui_flags.available_followmode;
            ret = RESTART_PLAY;
        }

        /* We reconnect if the server asked us to restart the connection; and we
           make a limited number of reconnection attempts if the network went
           down. TODO: Perhaps this behaviour should be in the client library
           rather than here. */
        if (ret != ERR_NETWORK_ERROR)
            reconnect_tries_upon_network_error = 3;
        else if (--reconnect_tries_upon_network_error > 0)
            ret = RESTART_PLAY;
    } while (ret == RESTART_PLAY);
    game_is_running = FALSE;

    return ret;
}
