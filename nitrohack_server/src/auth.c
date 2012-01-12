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


int auth_user(char *authbuf, const char *peername, int *is_reg, int *reconnect_id)
{
    json_error_t err;
    json_t *obj, *cmd, *name, *pass, *email, *reconn;
    const char *namestr, *passstr, *emailstr;
    int userid = 0;
   
    obj = json_loads(authbuf, 0, &err);
    if (!obj)
	return 0;
    
    /* try 1: is it an auth command? */
    *is_reg = 0;
    cmd = json_object_get(obj, "auth");
    if (!cmd) {
	/* try 2: register? */
	*is_reg = 1;
	cmd = json_object_get(obj, "register");
    }
    if (!cmd) /* not a recognized command */
	goto err;
    
    name = json_object_get(cmd, "username");
    pass = json_object_get(cmd, "password");
    email = json_object_get(cmd, "email"); /* is null for auth */
    reconn = json_object_get(cmd, "reconnect");
    
    if (!name || !pass)
	goto err;
    
    namestr = json_string_value(name);
    passstr = json_string_value(pass);
    
    if (!namestr || !passstr || strlen(namestr) < 3 || strlen(passstr) < 3 ||
	!is_valid_username(namestr))
	goto err;
    
    *reconnect_id = 0;
    if (!*is_reg) {
	if (reconn && json_is_integer(reconn))
	    *reconnect_id = json_integer_value(reconn);
	
	/* authenticate against a user database */
	userid = db_auth_user(namestr, passstr);
	if (userid > 0)
	    log_msg("%s has logged in as \"%s\" (userid %d)",
		    peername, namestr, userid);
	else if (userid < 0)
	    log_msg("%s has failed to log in as \"%s\" (userid %d)",
		    peername, namestr, -userid);
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
	    log_msg("%s has registered as \"%s\" (userid: %d)",
		    peername, namestr, userid);
	}
    }
    
    json_decref(obj);
    return userid;
    
err:
    json_decref(obj);
    return 0;
}


void auth_send_result(int sockfd, enum authresult result, int is_reg, int connid)
{
    int ret, written, len;
    json_t *jval;
    char *jstr;
    const char *key;
    
    key = "auth";
    if (is_reg)
	key = "register";
    
    jval = json_pack("{s:{si,si}}", key, "return", result, "connection", connid);
    jstr = json_dumps(jval, JSON_COMPACT);
    len = strlen(jstr);
    written = 0;
    do {
	ret = write(sockfd, jstr, len - written);
	if (ret > 0)
	    written += ret;
	/* don't care if it fails - if it does, the main event loop will be
	 * notified by epoll and perform cleanup later. */
    } while (ret > 0 && written < len);
    
    free(jstr);
    json_decref(jval);
}

/* auth.c */