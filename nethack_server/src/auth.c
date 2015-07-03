/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-11-06 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include <wctype.h>


/* check various rules that apply to names:
 * - it must be a valid multibyte (UTF8) string
 * - no more than 50 multibyte characters long
 * - '/', '.' and '\\' are not allowed because the user name will be used as
 *   part of the filename for saved games.
 * - it can't contain '<' or '>': names may be shown in web pages and this puts
 *   trivial mischief out of reach even if the name is not escaped properly.
 * - control characters are forbidden
 * - ASCII letters, numbers, underscores only for now, so that people don't
 *   try to confuse shellscripts (this rather makes the other checks moot,
 *   but...)
 */
static int
is_valid_username(const char *name)
{
    wchar_t converted[64];
    int i, mblen;

    mblen = mbstowcs(converted, name, 63);
    converted[63] = 0;
    if (mblen < 1 || mblen > 50)
        return FALSE;

    if (strchr(name, '/') || strchr(name, '\\') || strchr(name, '<') ||
        strchr(name, '>') || strchr(name, '.'))
        return FALSE;

    if (strspn(name, "abcdefghijklmnopqrstuvwxyz"
               "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(name))
        return FALSE;

    for (i = 0; i < mblen; i++)
        if (!iswprint(converted[i]))
            return FALSE;

    return TRUE;
}


int
auth_user(char *authbuf, int *is_reg)
{
    json_error_t err;
    json_t *obj, *cmd, *name, *pass, *email;
    const char *namestr, *passstr, *emailstr;
    int userid = 0;

    obj = json_loads(authbuf, 0, &err);
    if (!obj) {
        log_msg("auth packet does not look like valid JSON");
        return 0;
    }

    /* try 1: is it an auth command? */
    *is_reg = 0;
    cmd = json_object_get(obj, "auth");
    if (!cmd) {
        /* try 2: register? */
        *is_reg = 1;
        cmd = json_object_get(obj, "register");
    }
    if (!cmd) {  /* not a recognized command */
        log_msg("auth packet is not 'auth' or 'register'");
        goto err;
    }

    name = json_object_get(cmd, "username");
    pass = json_object_get(cmd, "password");
    email = json_object_get(cmd, "email");      /* is null for auth */

    if (!name || !pass) {
        log_msg("auth packet is missing name or password");
        goto err;
    }

    namestr = json_string_value(name);
    passstr = json_string_value(pass);

    if (!namestr || !passstr || strlen(namestr) < 3 || strlen(passstr) < 3 ||
        !is_valid_username(namestr)) {
        log_msg("name or password is invalid");
        goto err;
    }

    if (!*is_reg) {

        /* authenticate against a user database */
        userid = db_auth_user(namestr, passstr);
        if (userid > 0)
            log_msg("User has logged in as \"%s\" (userid %d)", namestr,
                    userid);
        else if (userid < 0)
            log_msg("Someone has failed to log in as \"%s\" (userid %d)",
                    namestr, -userid);
    } else {
        /* register a new user */
        emailstr = email ? json_string_value(email) : "";
        if (strlen(emailstr) > 100) {
            log_msg("Rejecting registration attempt: email is too long");
            goto err;
        }

        userid = db_register_user(namestr, passstr, emailstr);
        if (userid) {
            char savedir[1024];

            snprintf(savedir, 1024, "%s/save/%s", settings.workdir, namestr);
            mkdir(savedir, 0700);
            snprintf(savedir, 1024, "%s/completed/%s",
                     settings.workdir, namestr);
            mkdir(savedir, 0700);
            log_msg("User has registered as \"%s\" (userid: %d)",
                    namestr, userid);
        } else {
            log_msg("User has failed to register as \"%s\"", namestr);
        }
    }

    json_decref(obj);
    return userid;

err:
    json_decref(obj);
    return 0;
}


void
auth_send_result(int sockfd, enum authresult result, int is_reg)
{
    int ret, written, len;
    json_t *jval;
    char *jstr;
    const char *key;

    key = "auth";
    if (is_reg)
        key = "register";

    jval =
        json_pack("{s:{si,s:[i,i,i]}}", key, "return", result,
                  "version", VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
    jstr = json_dumps(jval, JSON_COMPACT);
    len = strlen(jstr);
    written = 0;
    do {
        /* Remember to include a NUL; the client expects that. */
        ret = write(sockfd, jstr, len - written + 1);
        if (ret > 0)
            written += ret;
        /* don't care if it fails - if it does, we'll notice later */
    } while (ret > 0 && written < len);

    free(jstr);
    json_decref(jval);
}

/* auth.c */
