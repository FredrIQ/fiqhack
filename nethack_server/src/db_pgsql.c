/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-06-21 */
/* Copyright (c) Daniel Thaler, 2011. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"

#if defined(LIBPQFE_IN_SUBDIR)
# include <postgresql/libpq-fe.h>
#else
# include <libpq-fe.h>
#endif

/* prepared statement names */
#define PREP_AUTH       "auth_user"
#define PREP_REGISTER   "register_user"

/* SQL statements used */
static const char SQL_init_user_table[] =
    "CREATE TABLE users(" "uid SERIAL PRIMARY KEY, "
    "name varchar(50) UNIQUE NOT NULL, " "pwhash text NOT NULL, "
    "email text NOT NULL default '', "
    "can_debug BOOLEAN NOT NULL DEFAULT FALSE, " "ts timestamp NOT NULL, "
    "reg_ts timestamp NOT NULL" ");";

static const char SQL_init_games_table[] =
    "CREATE TABLE games(" "gid SERIAL PRIMARY KEY, " "filename text NOT NULL, "
    "plname text NOT NULL, " "role text NOT NULL, " "race text NOT NULL, "
    "gender text NOT NULL, " "alignment text NOT NULL, "
    "mode integer NOT NULL, " "moves integer NOT NULL, "
    "depth integer NOT NULL, " "level_desc text NOT NULL, "
    "done boolean NOT NULL DEFAULT FALSE, "
    "owner integer NOT NULL REFERENCES users (uid), " "ts timestamp NOT NULL, "
    "start_ts timestamp NOT NULL" ");";

static const char SQL_init_topten_table[] =
    "CREATE TABLE topten(" "gid integer PRIMARY KEY REFERENCES games (gid), "
    "points integer NOT NULL, " "hp integer NOT NULL, "
    "maxhp integer NOT NULL, " "deaths integer NOT NULL, "
    "end_how integer NOT NULL, " "death text NOT NULL, "
    "entrytxt text NOT NULL" ");";

static const char SQL_check_table[] =
    "SELECT 1::integer " "FROM   pg_tables "
    "WHERE  schemaname = 'public' AND tablename = $1::text;";

/* Note: the regprocedure type check only succeeds it the function exists. */
static const char SQL_check_pgcrypto[] =
    "SELECT 'crypt(text,text)'::regprocedure;";

static const char SQL_register_user[] =
    "INSERT INTO users (name, pwhash, email, ts, reg_ts) "
    "VALUES ($1::varchar(50), crypt($2::text, gen_salt('bf', 8)), $3::text, "
    "'now', 'now');";

static const char SQL_last_reg_id[] = "SELECT currval('users_uid_seq');";

static const char SQL_auth_user[] =
    "SELECT uid, pwhash = crypt($2::text, pwhash) AS auth_ok " "FROM   users "
    "WHERE  name = $1::varchar(50);";

static const char SQL_get_user_info[] =
    "SELECT name, can_debug " "FROM   users " "WHERE  uid = $1::bigint";

static const char SQL_update_user_ts[] =
    "UPDATE users " "SET ts = 'now' " "WHERE uid = $1::integer;";

static const char SQL_set_user_email[] =
    "UPDATE users " "SET email = $2::text " "WHERE uid = $1::integer;";

static const char SQL_set_user_password[] =
    "UPDATE users " "SET pwhash = crypt($2::text, gen_salt('bf', 8)) "
    "WHERE uid = $1::integer;";

static const char SQL_add_game[] =
    "INSERT INTO games (filename, role, race, gender, alignment, mode, moves, "
    "depth, owner, plname, level_desc, ts, start_ts) "
    "VALUES ($1::text, $2::text, $3::text, $4::text, $5::text, "
    "$6::integer, 1, 1, $7::integer, $8::text, $9::text, 'now', 'now')";

static const char SQL_delete_game[] =
    "DELETE FROM games WHERE owner = $1::integer AND gid = $2::integer;";

static const char SQL_last_game_id[] = "SELECT currval('games_gid_seq');";

static const char SQL_update_game[] =
    "UPDATE games "
    "SET ts = 'now', moves = $2::integer, depth = $3::integer, level_desc = "
    "$4::text WHERE gid = $1::integer;";

static const char SQL_set_game_done[] =
    "UPDATE games " "SET done = TRUE " "WHERE gid = $1::integer;";

static const char SQL_list_games[] =
    "SELECT g.gid, g.filename, u.name "
    "FROM games AS g JOIN users AS u ON g.owner = u.uid "
    "WHERE (u.uid = $1::integer OR $1::integer = 0 OR"
    "       ($1::integer < 0 AND u.uid <> -($1::integer))) "
    "      AND (g.done = $2::boolean) "
    "      AND (g.gid = $3::integer OR $3::integer = 0) "
    "ORDER BY g.ts DESC LIMIT $4::integer;";

static const char SQL_add_topten_entry[] =
    "INSERT INTO topten (gid, points, hp, maxhp, deaths, end_how, death, "
    "entrytxt) VALUES ($1::integer, $2::integer, $3::integer, $4::integer, "
    "$5::integer, $6::integer, $7::text, $8::text);";


static PGconn *conn;


/*
 * init the database connection.
 */
int
init_database(void)
{
    if (conn)
        close_database();

    conn =
        PQsetdbLogin(settings.dbhost, settings.dbport, NULL, NULL,
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


static int
check_create_table(const char *tablename, const char *create_stmt)
{
    PGresult *res, *res2;
    const char *params[1];
    int paramFormats[1] = { 0 };

    params[0] = tablename;
    res2 =
        PQexecParams(conn, SQL_check_table, 1, NULL, params, NULL, paramFormats,
                     0);
    if (PQresultStatus(res2) != PGRES_TUPLES_OK || PQntuples(res2) == 0) {
        fprintf(stderr, "Table '%s' was not found. It will be created now.\n",
                tablename);
        PQclear(res2);

        res = PQexec(conn, create_stmt);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Failed to create table %s: %s", tablename,
                    PQerrorMessage(conn));
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
int
check_database(void)
{
    PGresult *res;

    /* 
     * Perform a quick check for the presence of the pgcrypto extension:
     * A function crypt(text, text) must exist.
     */
    res = PQexec(conn, SQL_check_pgcrypto);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "PostgreSQL pgcrypto check failed: %s",
                PQerrorMessage(conn));
        PQclear(res);
        goto err;
    }
    PQclear(res);

    /* 
     * Create the required set of tables if they don't exist
     */
    if (!check_create_table("users", SQL_init_user_table) ||
        !check_create_table("games", SQL_init_games_table) ||
        !check_create_table("topten", SQL_init_topten_table))
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


void
close_database(void)
{
    PQfinish(conn);
    conn = NULL;
}


int
db_auth_user(const char *name, const char *pass)
{
    PGresult *res;
    const char *const params[] = { name, pass };
    int uid, auth_ok, col;
    const char *uidstr;

    res = PQexecPrepared(conn, PREP_AUTH, 2, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        log_msg("db_auth_user failed: %s\n", PQerrorMessage(conn));
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


int
db_register_user(const char *name, const char *pass, const char *email)
{
    PGresult *res;
    const char *const params[] = { name, pass, email };
    int uid;
    const char *uidstr;

    res = PQexecPrepared(conn, PREP_REGISTER, 3, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        log_msg("db_register_user failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    PQclear(res);

    res = PQexec(conn, SQL_last_reg_id);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        log_msg("db_register_user get last id failed: %s",
                PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    uidstr = PQgetvalue(res, 0, 0);
    uid = atoi(uidstr);
    PQclear(res);

    return uid;
}


int
db_get_user_info(int uid, struct user_info *info)
{
    PGresult *res;
    char uidstr[16];
    const char *const params[] = { uidstr };
    const int paramFormats[] = { 0 };   /* text format */
    int col;

    snprintf(uidstr, sizeof(uidstr), "%d", uid);

    res =
        PQexecParams(conn, SQL_get_user_info, 1, NULL, params, NULL,
                     paramFormats, 0);
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


void
db_update_user_ts(int uid)
{
    PGresult *res;
    char uidstr[16];
    const char *const params[] = { uidstr };
    const int paramFormats[] = { 0 };   /* text format */

    snprintf(uidstr, sizeof(uidstr), "%d", uid);
    res =
        PQexecParams(conn, SQL_update_user_ts, 1, NULL, params, NULL,
                     paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
        log_msg("update_user_ts error: %s", PQerrorMessage(conn));
    PQclear(res);
}


int
db_set_user_email(int uid, const char *email)
{
    PGresult *res;
    char uidstr[16];
    const char *const params[] = { uidstr, email };
    const int paramFormats[] = { 0, 0 };
    const char *numrows;

    snprintf(uidstr, sizeof(uidstr), "%d", uid);

    res =
        PQexecParams(conn, SQL_set_user_email, 2, NULL, params, NULL,
                     paramFormats, 0);
    numrows = PQcmdTuples(res);
    if (PQresultStatus(res) == PGRES_COMMAND_OK && atoi(numrows) == 1) {
        PQclear(res);
        return TRUE;
    }
    PQclear(res);
    return FALSE;
}


int
db_set_user_password(int uid, const char *password)
{
    PGresult *res;
    char uidstr[16];
    const char *const params[] = { uidstr, password };
    const int paramFormats[] = { 0, 0 };
    const char *numrows;

    snprintf(uidstr, sizeof(uidstr), "%d", uid);

    res =
        PQexecParams(conn, SQL_set_user_password, 2, NULL, params, NULL,
                     paramFormats, 0);
    numrows = PQcmdTuples(res);
    if (PQresultStatus(res) == PGRES_COMMAND_OK && atoi(numrows) == 1) {
        PQclear(res);
        return TRUE;
    }
    PQclear(res);
    return FALSE;
}


long
db_add_new_game(int uid, const char *filename, const char *role,
                const char *race, const char *gend, const char *align, int mode,
                const char *plname, const char *levdesc)
{
    PGresult *res;
    char uidstr[16], modestr[16];

    const char *const params[] = { filename, role, race, gend,
        align, modestr, uidstr, plname, levdesc
    };
    const int paramFormats[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const char *gameid_str;
    int gid;

    snprintf(uidstr, sizeof(uidstr), "%d", uid);
    snprintf(modestr, sizeof(modestr), "%d", mode);

    res =
        PQexecParams(conn, SQL_add_game, 9, NULL, params, NULL, paramFormats,
                     0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        log_msg("db_add_new_game error while adding (%s - %s): %s", plname,
                filename, PQerrorMessage(conn));
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


void
db_update_game(int game, int moves, int depth, const char *levdesc)
{
    PGresult *res;
    char gidstr[16], movesstr[16], depthstr[16];
    const char *const params[] = { gidstr, movesstr, depthstr, levdesc };
    const int paramFormats[] = { 0, 0, 0, 0 };

    snprintf(gidstr, sizeof(gidstr), "%d", game);
    snprintf(movesstr, sizeof(movesstr), "%d", moves);
    snprintf(depthstr, sizeof(depthstr), "%d", depth);

    res =
        PQexecParams(conn, SQL_update_game, 4, NULL, params, NULL, paramFormats,
                     0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
        log_msg("update_game_ts error: %s", PQerrorMessage(conn));
    PQclear(res);
}

void
db_delete_game(int uid, int gid)
{
    PGresult *res;
    char uidstr[16], gidstr[16];
    const char *const params[] = { uidstr, gidstr };
    const int paramFormats[] = { 0, 0 };

    snprintf(uidstr, sizeof(uidstr), "%d", uid);
    snprintf(gidstr, sizeof(gidstr), "%d", gid);

    res =
        PQexecParams(conn, SQL_delete_game, 2, NULL, params, NULL, paramFormats,
                     0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
        log_msg("db_delete_game error: %s", PQerrorMessage(conn));

    PQclear(res);
}


static struct gamefile_info *
db_game_name_core(int completed, int uid, int gid, int limit, int *count)
{
    PGresult *res;
    int i, gidcol, fncol, ucol;
    struct gamefile_info *files;
    char uidstr[16], gidstr[16], complstr[16], limitstr[16];
    const char *const params[] = { uidstr, complstr, gidstr, limitstr };
    const int paramFormats[] = { 0, 0, 0, 0 };
    const char *const fmtstr = completed ? "%s/completed/%s/%s" :
        "%s/save/%s/%s";

    if (limit <= 0 || limit > 100)
        limit = 100;

    snprintf(uidstr, sizeof(uidstr), "%d", uid);
    snprintf(gidstr, sizeof(gidstr), "%d", gid);
    snprintf(complstr, sizeof(complstr), "%d", !!completed);
    snprintf(limitstr, sizeof(limitstr), "%d", limit);

    res =
        PQexecParams(conn, SQL_list_games, 4, NULL, params, NULL, paramFormats,
                     0);
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

    files = malloc(sizeof (struct gamefile_info) * (*count));
    for (i = 0; i < *count; i++) {
        files[i].gid = atoi(PQgetvalue(res, i, gidcol));
        files[i].filename = malloc(strlen(fmtstr) +
                                   strlen(settings.workdir) +
                                   strlen(PQgetvalue(res, i, fncol)) +
                                   strlen(PQgetvalue(res, i, ucol)) + 1);
        sprintf(files[i].filename, fmtstr, settings.workdir,
                PQgetvalue(res, i, ucol), PQgetvalue(res, i, fncol));
    }

    PQclear(res);
    return files;
}


enum getgame_result
db_get_game_filename(int gid, char *filenamebuf, int buflen)
{
    struct gamefile_info *gfi;
    int completed, count;
    
    for (completed = 0; completed <= 1; completed++) {
        gfi = db_game_name_core(completed, 0, gid, 1, &count);

        if (count) {
            strncpy(filenamebuf, gfi[0].filename, buflen);
            filenamebuf[buflen-1] = '\0';
            free(gfi[0].filename);
            free(gfi);
            return completed ? GGR_COMPLETED : GGR_INCOMPLETE;
        }
    }

    return GGR_NOT_FOUND;
}

struct gamefile_info *
db_list_games(int completed, int uid, int limit, int *count)
{
    return db_game_name_core(completed, uid, 0, limit, count);
}

void
db_add_topten_entry(int gid, int points, int hp, int maxhp, int deaths,
                    int end_how, const char *death, const char *entrytxt)
{
    PGresult *res;
    char gidstr[16], pointstr[16], hpstr[16], maxhpstr[16], dcountstr[16],
        endstr[16];
    const char *const params[] = { gidstr, pointstr, hpstr, maxhpstr,
        dcountstr, endstr, death, entrytxt
    };
    const int paramFormats[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    snprintf(gidstr, sizeof(gidstr), "%d", gid);
    snprintf(pointstr, sizeof(pointstr), "%d", points);
    snprintf(hpstr, sizeof(hpstr), "%d", hp);
    snprintf(maxhpstr, sizeof(maxhpstr), "%d", maxhp);
    snprintf(dcountstr, sizeof(dcountstr), "%d", deaths);
    snprintf(endstr, sizeof(endstr), "%d", end_how);

    res =
        PQexecParams(conn, SQL_add_topten_entry, 8, NULL, params, NULL,
                     paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
        log_msg("add_topten_entry error: %s", PQerrorMessage(conn));
    PQclear(res);

    /* note: the params and paramFormats arrays are re-used, but only the 1.
       entry matters */
    res =
        PQexecParams(conn, SQL_set_game_done, 1, NULL, params, NULL,
                     paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
        log_msg("set_game_done error: %s", PQerrorMessage(conn));
    PQclear(res);
    return;
}

/* db_pgsql.c */
