/* Copyright (c) Daniel Thaler, 2011. */
/* The NitroHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#include <libpq-fe.h>

/* prepared statement names */
#define PREP_AUTH	"auth_user"
#define PREP_REGISTER	"register_user"

/* SQL statements used */
static const char SQL_init_user_table[] =
    "CREATE TABLE users("
       "uid SERIAL PRIMARY KEY, "
       "name varchar(50) UNIQUE NOT NULL, "
       "pwhash text NOT NULL, "
       "email text NOT NULL default '', "
       "can_debug BOOLEAN NOT NULL DEFAULT FALSE, "
       "lastactivity timestamp NOT NULL"
    ");";

static const char SQL_init_games_table[] =
    "CREATE TABLE games("
       "gid SERIAL PRIMARY KEY, "
       "filename text NOT NULL, "
       "plname text NOT NULL, "
       "role integer NOT NULL, "
       "race integer NOT NULL, "
       "gender integer NOT NULL, "
       "alignment integer NOT NULL, "
       "mode integer NOT NULL, "
       "done boolean NOT NULL DEFAULT FALSE, "
       "owner integer NOT NULL REFERENCES users (uid), "
       "end_how integer, "
       "ts timestamp NOT NULL"
    ");";

static const char SQL_init_options_table[] =
    "CREATE TABLE options("
	"uid integer NOT NULL REFERENCES users (uid), "
	"optname text NOT NULL, "
	"opttype integer NOT NULL, "
	"optvalue text NOT NULL, "
	"PRIMARY KEY(uid, optname)"
    ");";

static const char SQL_check_table[] =
    "SELECT 1::integer "
    "FROM   pg_tables "
    "WHERE  schemaname = 'public' AND tablename = $1::text;";

/* Note: the regprocedure type check only succeeds it the function exists. */
static const char SQL_check_pgcrypto[] =
    "SELECT 'crypt(text,text)'::regprocedure;";

static const char SQL_register_user[] =
    "INSERT INTO users (name, pwhash, email, lastactivity) "
    "VALUES ($1::varchar(50), crypt($2::text, gen_salt('bf', 8)), $3::text, 'now');";
    
static const char SQL_last_reg_id[] =
    "SELECT currval('users_uid_seq');";

static const char SQL_auth_user[] =
    "SELECT uid, pwhash = crypt($2::text, pwhash) AS auth_ok "
    "FROM   users "
    "WHERE  name = $1::varchar(50);";

static const char SQL_get_user_info[] =
    "SELECT name, can_debug "
    "FROM   users "
    "WHERE  uid = $1::bigint";

static const char SQL_update_user_ts[] =
    "UPDATE users "
    "SET lastactivity = 'now' "
    "WHERE uid = $1::integer;";

static const char SQL_add_game[] =
    "INSERT INTO games (filename, role, race, gender, alignment, mode, owner, plname, ts) "
    "VALUES ($1::text, $2::integer, $3::integer, $4::integer, "
            "$5::integer, $6::integer, $7::integer, $8::text, 'now')";
    
static const char SQL_last_game_id[] =
    "SELECT currval('games_gid_seq');";

static const char SQL_set_game_done[] =
    "UPDATE games "
    "SET end_how = $1::integer, done = TRUE "
    "WHERE gid = $2::integer;";

static const char SQL_update_game_ts[] =
    "UPDATE games "
    "SET ts = 'now' "
    "WHERE gid = $1::integer;";

static const char SQL_get_game_filename[] =
    "SELECT filename "
    "FROM games "
    "WHERE owner = $1::integer AND gid = $2::integer;";

static const char SQL_set_game_filename[] =
    "UPDATE games "
    "SET filename = $1::text "
    "WHERE gid = $2::integer;";

static const char SQL_list_games[] =
    "SELECT g.gid, g.filename, u.name "
    "FROM games AS g JOIN users AS u ON g.owner = u.uid "
    "WHERE (u.uid = $1::integer OR $1::integer = 0) AND g.done = $2::boolean "
    "ORDER BY g.ts DESC "
    "LIMIT $3::integer;";

static const char SQL_update_option[] =
    "UPDATE options "
    "SET optvalue = $1::text "
    "WHERE uid = $2::integer AND optname = $3::text;";

static const char SQL_insert_option[] =
    "INSERT INTO options (optvalue, uid, optname, opttype) "
    "VALUES ($1::text, $2::integer, $3::text, $4::integer);";

static const char SQL_get_options[] =
    "SELECT optname, optvalue "
    "FROM options "
    "WHERE uid = $1::integer;";


static PGconn *conn;


/*
 * init the database connection.
 */
int init_database(void)
{
    if (conn)
	close_database();

    conn = PQsetdbLogin(settings.dbhost, settings.dbport, NULL, NULL,
			settings.dbname, settings.dbuser, settings.dbpass);
    if (PQstatus(conn) == CONNECTION_BAD) {
	fprintf(stderr, "Database connection failed. Check your settings.\n");
	goto err;
    }
    
    return TRUE;
    
err:
    PQfinish(conn);
    return FALSE;
}


static int check_create_table(const char *tablename, const char *create_stmt)
{
    PGresult *res, *res2;
    const char *params[1];
    int paramFormats[1] = {0};
    
    params[0] = tablename;
    res2 = PQexecParams(conn, SQL_check_table, 1, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res2) != PGRES_TUPLES_OK || PQntuples(res2) == 0) {
	fprintf(stderr, "Table '%s' was not found. It will be created now.\n", tablename);
	PQclear(res2);
	
	res = PQexec(conn, create_stmt);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
	    fprintf(stderr, "Failed to create table %s: %s",
		    tablename, PQerrorMessage(conn));
	    PQclear(res);
	    return FALSE;
	}
	PQclear(res);
    } else
	PQclear(res2);
    
    return TRUE;
}


/*
 * check the database tables and create them if necessary. Also check for the
 * existence of the crypt function
 */
int check_database(void)
{
    PGresult *res;
    
    /*
     * Perform a quick check for the presence of the pgcrypto extension:
     * A function crypt(text, text) must exist.
     */
    res = PQexec(conn, SQL_check_pgcrypto);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "PostgreSQL pgcrypto check failed: %s", PQerrorMessage(conn));
        PQclear(res);
        goto err;
    }
    PQclear(res);
    
    /*
     * Create the required set of tables if they don't exist
     */
    if (!check_create_table("users", SQL_init_user_table) ||
	!check_create_table("games", SQL_init_games_table) ||
	!check_create_table("options", SQL_init_options_table))
	goto err;
    
    /*
     * Create prepared statements
     */
    res = PQprepare(conn, PREP_REGISTER, SQL_register_user, 0, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "prepare statement failed: %s", PQerrorMessage(conn));
        PQclear(res);
	goto err;
    }
    PQclear(res);
    
    res = PQprepare(conn, PREP_AUTH, SQL_auth_user, 0, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "prepare statement failed: %s", PQerrorMessage(conn));
        PQclear(res);
	goto err;
    }
    PQclear(res);
    
    return TRUE;
    
err:
    PQfinish(conn);
    return FALSE;
}


void close_database(void)
{
    PQfinish(conn);
    conn = NULL;
}


int db_auth_user(const char *name, const char *pass)
{
    PGresult *res;
    const char * const params[] = {name, pass};
    int uid, auth_ok, col;
    const char *uidstr;
    
    res = PQexecPrepared(conn, PREP_AUTH, 2, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
	return 0;
    }
    
    col = PQfnumber(res, "uid");
    uidstr = PQgetvalue(res, 0, col);
    uid = atoi(uidstr);
    
    col = PQfnumber(res, "auth_ok");
    auth_ok = (PQgetvalue(res, 0, col)[0] == 't');
    PQclear(res);
    
    return auth_ok ? uid : -uid;
}


int db_register_user(const char *name, const char *pass, const char *email)
{
    PGresult *res;
    const char * const params[] = {name, pass, email};
    int uid;
    const char *uidstr;
    
    res = PQexecPrepared(conn, PREP_REGISTER, 3, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
	return 0;
    }
    PQclear(res);
    
    res = PQexec(conn, SQL_last_reg_id);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
	return 0;
    }
    uidstr = PQgetvalue(res, 0, 0);
    uid = atoi(uidstr);
    PQclear(res);
    
    return uid;
}


int db_get_user_info(int uid, struct user_info *info)
{
    PGresult *res;
    char uidstr[16];
    const char * const params[] = {uidstr};
    const int paramFormats[] = {0}; /* text format */
    int col;
    
    sprintf(uidstr, "%d", uid);
    
    res = PQexecParams(conn, SQL_get_user_info, 1, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
	log_msg("db_get_user_info error: %s", PQerrorMessage(conn));
        PQclear(res);
	return FALSE;
    }
    
    col = PQfnumber(res, "can_debug");
    info->can_debug = (PQgetvalue(res, 0, col)[0] == 't');

    col = PQfnumber(res, "name");
    info->username = strdup(PQgetvalue(res, 0, col));
    
    info->uid = uid;
    
    PQclear(res);
    return TRUE;
}


void db_update_user_ts(int uid)
{
    PGresult *res;
    char uidstr[16];
    const char * const params[] = {uidstr};
    const int paramFormats[] = {0}; /* text format */
    
    sprintf(uidstr, "%d", uid);
    res = PQexecParams(conn, SQL_update_user_ts, 1, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
	log_msg("update_user_ts error: %s", PQerrorMessage(conn));
    PQclear(res);
}


long db_add_new_game(int uid, const char *filename, int role, int race,
			    int gend, int align, int mode, const char *plname)
{
    PGresult *res;
    char uidstr[16], rolestr[16], racestr[16], gendstr[16], alignstr[16], modestr[16];
    const char *const params[] = {filename, rolestr, racestr, gendstr,
                                  alignstr, modestr, uidstr, plname};
    const int paramFormats[] = {0, 0, 0, 0, 0, 0, 0, 0};
    const char *gameid_str;
    int gid;
    
    sprintf(uidstr, "%d", uid);
    sprintf(rolestr, "%d", role);
    sprintf(racestr, "%d", race);
    sprintf(gendstr, "%d", gend);
    sprintf(alignstr, "%d", align);
    sprintf(modestr, "%d", mode);
    
    res = PQexecParams(conn, SQL_add_game, 8, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
	log_msg("db_add_new_game error while adding (%s - %s): %s",
		plname, filename, PQerrorMessage(conn));
        PQclear(res);
	return 0;
    }
    
    res = PQexec(conn, SQL_last_game_id);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
	return 0;
    }
    
    gameid_str = PQgetvalue(res, 0, 0);
    gid = atoi(gameid_str);
    PQclear(res);
    
    return gid;
}


void db_set_game_done(int gameid, int end_how)
{
    PGresult *res;
    char gidstr[16], endstr[16], *numrows;
    const char * const params[] = {endstr, gidstr};
    const int paramFormats[] = {0, 0};
    
    sprintf(endstr, "%d", end_how);
    sprintf(gidstr, "%d", gameid);
    
    res = PQexecParams(conn, SQL_set_game_done, 2, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
	log_msg("set_game_done error: %s", PQerrorMessage(conn));
    numrows = PQcmdTuples(res);
    if (atoi(numrows) < 1)
	log_msg("set_game_done for game %d (status %d) failed", gameid, end_how);
    PQclear(res);
}


void db_update_game_ts(int gameid)
{
    PGresult *res;
    char gidstr[16];
    const char * const params[] = {gidstr};
    const int paramFormats[] = {0};
    
    sprintf(gidstr, "%d", gameid);
    
    res = PQexecParams(conn, SQL_update_game_ts, 1, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
	log_msg("update_game_ts error: %s", PQerrorMessage(conn));
    PQclear(res);
}


int db_get_game_filename(int uid, int gid, char *namebuf, int buflen)
{
    PGresult *res;
    char uidstr[16], gidstr[16];
    const char * const params[] = {uidstr, gidstr};
    const int paramFormats[] = {0, 0};
    
    sprintf(uidstr, "%d", uid);
    sprintf(gidstr, "%d", gid);
    
    res = PQexecParams(conn, SQL_get_game_filename, 2, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
	log_msg("get_game_filename error: %s", PQerrorMessage(conn));
	PQclear(res);
	return FALSE;
    }
    
    strncpy(namebuf, PQgetvalue(res, 0, 0), buflen);
    PQclear(res);
    return TRUE;
}


void db_set_game_filename(int gid, const char *filename)
{
    PGresult *res;
    char gidstr[16];
    const char * const params[] = {filename, gidstr};
    const int paramFormats[] = {0, 0};
    
    sprintf(gidstr, "%d", gid);
    res = PQexecParams(conn, SQL_set_game_filename, 2, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
	log_msg("db_set_game_filename error: %s", PQerrorMessage(conn));
    PQclear(res);
}


struct gamefile_info *db_list_games(int completed, int uid, int limit, int *count)
{
    PGresult *res;
    int i, gidcol, fncol, ucol;
    struct gamefile_info *files;
    char uidstr[16], complstr[16], limitstr[16];
    const char * const params[] = {uidstr, complstr, limitstr};
    const int paramFormats[] = {0, 0, 0};
    
    if (limit <= 0 || limit > 100)
	limit = 100;
    
    sprintf(uidstr, "%d", uid);
    sprintf(complstr, "%d", !!completed);
    sprintf(limitstr, "%d", limit);
    
    res = PQexecParams(conn, SQL_list_games, 3, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
	log_msg("list_games error: %s", PQerrorMessage(conn));
	PQclear(res);
	*count = 0;
	return NULL;
    }
    
    *count = PQntuples(res);
    gidcol = PQfnumber(res, "gid");
    fncol = PQfnumber(res, "filename");
    ucol = PQfnumber(res, "name");
    
    files = malloc(sizeof(struct gamefile_info) * (*count));
    for (i = 0; i < *count; i++) {
	files[i].gid = atoi(PQgetvalue(res, i, gidcol));
	files[i].filename = strdup(PQgetvalue(res, i, fncol));
	files[i].username = strdup(PQgetvalue(res, i, ucol));
    }
    
    PQclear(res);
    return files;
}


void db_set_option(int uid, const char *optname, int type, const char *optval)
{
    PGresult *res;
    char uidstr[16], typestr[16];
    const char * const params[] = {optval, uidstr, optname, typestr};
    const int paramFormats[] = {0, 0, 0, 0};
    const char *numrows;
    
    sprintf(uidstr, "%d", uid);
    sprintf(typestr, "%d", type);
    
    /* try to update first */
    res = PQexecParams(conn, SQL_update_option, 3, NULL, params, NULL, paramFormats, 0);
    numrows = PQcmdTuples(res);
    if (PQresultStatus(res) == PGRES_COMMAND_OK && atoi(numrows) == 1) {
	PQclear(res);
	return;
    }
    PQclear(res);
    
    /* update failed, try to insert */
    res = PQexecParams(conn, SQL_insert_option, 4, NULL, params, NULL, paramFormats, 0);
    numrows = PQcmdTuples(res);
    if (PQresultStatus(res) == PGRES_COMMAND_OK && atoi(numrows) == 1) {
	PQclear(res);
	return;
    }
    PQclear(res);
    
    /* insert failed too */
    log_msg("Failed to store an option. '%s = %s': %s", optname, optval,
	    PQerrorMessage(conn));
}


void db_restore_options(int uid)
{
    PGresult *res;
    char uidstr[16];
    const char * const params[] = {uidstr};
    const char *optname;
    const int paramFormats[] = {0};
    int i, count, ncol, vcol;
    union nh_optvalue value;
    
    sprintf(uidstr, "%d", uid);
    
    res = PQexecParams(conn, SQL_get_options, 1, NULL, params, NULL, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
	log_msg("get_options error: %s", PQerrorMessage(conn));
	PQclear(res);
	return;
    }
    
    count = PQntuples(res);
    ncol = PQfnumber(res, "optname");
    vcol = PQfnumber(res, "optvalue");
    for (i = 0; i < count; i++) {
	optname = PQgetvalue(res, i, ncol);
	value.s = PQgetvalue(res, i, vcol);
	nh_set_option(optname, value, 1);
    }
    PQclear(res);
}

/* db_pgsql.c */
