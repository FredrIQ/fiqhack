/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include <sys/stat.h>
#include <wctype.h>


/* check various rules that apply to names:
 * - it must be a valid multibyte (UTF8) string
 * - no more than 50 multibyte characters long
 * - '/' and '\\' are not allowed because the user name will be used as part of
 *   the filename for saved games.
 * - it can't contain '<' or '>': names may be shown in web pages and this puts
 *   trivial mischief out of reach even if the name is not escaped properly.
 * - control characters are forbidden
 */
static int is_valid_username(const char *name)
{
    wchar_t converted[64];
    int i, mblen;
    
    mblen = mbstowcs(converted, name, 63);
    converted[63] = 0;
    if (mblen < 1 || mblen > 50)
	return FALSE;
    
    if (strchr(name, '/') || strchr(name, '\\') ||
	strchr(name, '<') || strchr(name, '>'))
	return FALSE;
    
    for (i = 0; i < mblen ; i++)
	if (!iswprint(converted[i]))
	    return FALSE;
    
    return TRUE;
}


int auth_user(char *authbuf)
{
    json_error_t err;
    json_t *obj, *cmd, *name, *pass, *email;
    const char *namestr, *passstr, *emailstr;
    int is_auth = 1;
    int userid = 0;
    
    obj = json_loads(authbuf, 0, &err);
    if (!obj)
	return 0;
    
    /* try 1: is it an auth command? */
    cmd = json_object_get(obj, "auth");
    if (!cmd) {
	/* try 2: register? */
	is_auth = 0;
	cmd = json_object_get(obj, "register");
    }
    if (!cmd) /* not a recognized command */
	goto err;
    
    name = json_object_get(cmd, "username");
    pass = json_object_get(cmd, "password");
    email = json_object_get(cmd, "email"); /* is null for auth */
    
    if (!name || !pass)
	goto err;
    
    namestr = json_string_value(name);
    passstr = json_string_value(pass);
    
    if (!namestr || !passstr || strlen(namestr) < 3 || strlen(passstr) < 3 ||
	!is_valid_username(namestr))
	goto err;
    
    if (is_auth) {
	/* authenticate against a user database */
	userid = db_auth_user(namestr, passstr);
    } else {
	/* register a new user */
	emailstr = email ? json_string_value(email) : "";
	if (strlen(emailstr) > 100)
	    goto err;
	
	userid = db_register_user(namestr, passstr, emailstr);
	if (userid) {
	    char savedir[1024];
	    snprintf(savedir, 1024, "%s/save/%s", settings.workdir, namestr);
	    mkdir(savedir, 0700);
	    log_msg("New user registered: %s", namestr);
	}
    }
    
    json_decref(obj);
    return userid;
    
err:
    json_decref(obj);
    return 0;
}


void auth_send_result(int sockfd, enum authresult result)
{
    int ret, written, len;
    json_t *jval = json_pack("{si}", "authresult", (int)result);
    char *jstr = json_dumps(jval, JSON_COMPACT);
    len = strlen(jstr);
    written = 0;
    do {
	ret = write(sockfd, jstr, len - written);
	if (ret > 0)
	    written += ret;
	/* don't care if it fails - if it does, the main event loop will be
	 * notified by epoll and perform cleanup later. */
    } while (ret && written < len);
    
    free(jstr);
    json_decref(jval);
}

/* auth.c */