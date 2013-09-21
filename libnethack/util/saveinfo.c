/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* Copyright (c) Daniel Thaler, 2011.                             */
/* The code in this file is yours to do whatever you want with.   */

#error !AIMAKE_FAIL_SILENTLY! This file hasn't been tested for a while, produces lots of warnings, and likely doesn't work. Use at your peril.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

long cmdcount;
char **commands;


static const char *const directions[] = {
    "none", "west", "north-west", "north", "north-east", "east", "south-east",
    "south", "south-west", "up", "down", "self"
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
parse_option_token(char *token, long tnum)
{
    int optval;
    char *name, *val, *type;
    char namebuf[512], valbuf[512], *vb2;

    name = token + 1;
    type = strchr(name, ':');
    val = strchr(type + 1, ':');

    if (!type || !val) {
        printf("weird error on token %ld: %s\n", tnum, token);
        return;
    }
    *type++ = '\0';
    *val++ = '\0';

    base64_decode(name, namebuf);
    printf("%05ld: set option %s ", tnum, namebuf);
    valbuf[0] = 0;
    switch (type[0]) {
    case 's':
        base64_decode(val, valbuf);
        printf("(STRING) = \"%s\"\n", valbuf);
        break;

    case 'b':
        optval = strtol(val, NULL, 16);
        printf("(BOOL) = %d\n", optval);
        break;

    case 'e':
        optval = strtol(val, NULL, 16);
        printf("(ENUM) = %d\n", optval);
        break;

    case 'i':
        optval = strtol(val, NULL, 10);
        printf("(INT) = %d\n", optval);
        break;

    case 'a':
        vb2 = malloc(strlen(val) + 1);
        base64_decode(val, vb2);
        printf("(AP_RULES) = \"%s\"\n", vb2);
        free(vb2);
    }
}


static void
parse_command_token(char **tokens, long *tnum, long tcount)
{
    char *token, *pos, timebuf[256], *cmdname, invlet;
    long time;
    int cmd, rep, dir, x, y;

    token = tokens[*tnum];
    pos = token + 1;
    time = strtol(pos, NULL, 16);
    pos = strchr(pos, ':');
    if (!pos)
        return;
    pos++;
    cmd = strtol(pos, NULL, 16);
    pos = strchr(pos, ':');
    if (!pos)
        return;
    pos++;
    rep = strtol(pos, NULL, 10);

    if (!cmd && rep == -1)
        cmdname = "- break -";
    else if (!cmd)
        cmdname = "- continue -";
    else
        cmdname = commands[cmd];

    strftime(timebuf, 256, "%H:%M:%S", localtime(&time));
    if (rep > 1)
        printf("%05ld, %s: %dx %s", *tnum, timebuf, rep, cmdname);
    else
        printf("%05ld, %s: %s", *tnum, timebuf, cmdname);

    (*tnum)++;
    if (*tnum >= tcount)
        return;
    token = tokens[*tnum];
    switch (token[0]) {
    default:
    case 'n':
        printf("   ");
        break;

    case 'd':
        sscanf(token, "d:%d", &dir);
        printf("(%s)   ", directions[dir + 1]);
        break;

    case 'p':
        sscanf(token, "p:%x,%x", &x, &y);
        printf("(%d, %d)   ", x, y);
        break;

    case 'o':
        sscanf(token, "o:%c", &invlet);
        printf("obj:'%c'   ", invlet);
        break;
    }
}


static void
parse_command_list(char **tokens, long *tnum, long tcount)
{
    int i;
    int cmdcount = strtol(tokens[13], NULL, 16);
    char decbuf[256], *token;

    cmdcount++;
    commands = malloc(cmdcount * sizeof (char *));
    commands[0] = NULL;

    for (i = 1; i < cmdcount && *tnum < tcount; i++) {
        token = tokens[(*tnum)++];
        base64_decode(token, decbuf);
        commands[i] = strdup(decbuf);
    }
}


static void
parse_key(char *token)
{
    char key, keystr[16];
    int cnt = 0;

    sscanf(token, "k:%hhx:%x", &key, &cnt);
    if (key == '\033')
        sprintf(keystr, "<ESC>");
    else if (key == '\n')
        sprintf(keystr, "<RETURN>");
    else
        sprintf(keystr, "%c", key);

    if (cnt > 1)
        printf("key:%d%s ", cnt, keystr);
    else
        printf("key:%s ", keystr);
}


static void
parse_yesno(char *token)
{
    char key;

    sscanf(token, "y:%hhx", &key);
    printf("yes/no:'%c' ", key);
}


static void
parse_line(char *token)
{
    char outbuf[1024];
    char *encdata = strchr(token, ':');

    base64_decode(encdata + 1, outbuf);
    printf("line:\"%s\" ", outbuf);
}


static void
parse_dir(char *token)
{
    int dir;

    sscanf(token, "d:%d", &dir);
    printf("dir:%s ", directions[dir + 1]);
}


static void
parse_timezone(char *token, long tcount)
{
    int tz;

    sscanf(token, "TZ%d", &tz);
    printf("%05ld: Timezone offset: %d seconds\n", tcount, tz);
}


static void
parse_result(char *token)
{
    unsigned int state;

    sscanf(token, "<%04x", &state);
    printf("(%04x)", state);
}


int
main(int argc, char *argv[])
{
    FILE *fp;
    long size, tcount, nr_tokens, tnum, seed, mode, validlen;
    char *mem, *nexttoken, **tokens;
    long *toff;
    char namebuf[256], datebuf[256];

    if (argc != 2) {
        printf("Usage: %s <file.nhgame>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "r");
    if (!fp)
        return 2;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    mem = malloc(size + 1);
    mem[size] = 0;
    fread(mem, 1, size, fp);
    fclose(fp);

    tcount = 0;
    nr_tokens = 512;
    tokens = malloc(nr_tokens * sizeof (char *));
    toff = malloc(nr_tokens * sizeof (long));
    nexttoken = strtok(mem, " \r\n");
    while (nexttoken) {
        toff[tcount] = (long)nexttoken - (long)mem;
        tokens[tcount++] = nexttoken;
        nexttoken = strtok(NULL, " \r\n");
        if (tcount >= nr_tokens) {
            nr_tokens *= 2;
            tokens = realloc(tokens, nr_tokens * sizeof (char *));
            toff = realloc(toff, nr_tokens * sizeof (long));
        }
    }

    if (tcount < 13 || strcmp(tokens[0], "NHGAME") != 0) {
        printf("This does not look like a NetHack logfile.\n");
        return 3;
    }

    printf("This is a logfile for %s version %s.  ", tokens[3], tokens[4]);
    if (!strcmp(tokens[1], "inpr")) {
        printf("The game is either currently in progress or %s crashed.  ",
               tokens[3]);
    } else if (!strcmp(tokens[1], "save")) {
        printf("The game was saved.  ");
    } else if (!strcmp(tokens[1], "done")) {
        printf("The game has finished.  ");
    } else {
        printf("The file header is damaged.  ");
    }

    validlen = strtol(tokens[2], NULL, 16);
    if (!validlen)
        printf
            ("There is no information on how many bytes of data to expect in the log. This may result in garbage being printed after the last valid log entry\n");
    else
        printf("There should be %ld bytes of data in this logfile.\n",
               validlen);

    base64_decode(tokens[8], namebuf);
    seed = strtol(tokens[5], NULL, 16);
    mode = strtol(tokens[7], NULL, 16);

    strftime(datebuf, 256, "%A %F %H:%M", localtime(&seed));
    printf("On %s, %s the %s %s %s %s began %s adventure", datebuf, namebuf,
           tokens[12], tokens[11], tokens[10], tokens[9],
           tokens[11][0] == 'f' ? "her" : "his");

    if (mode > 0)
        printf(" in %s mode", mode == 1 ? "discover" : "wizard");
    printf("\n");

    tnum = 14;
    parse_command_list(tokens, &tnum, tcount);

    for (; tnum < tcount && (!validlen || tokens[tnum] - mem < validlen);
         tnum++) {
        printf("[%08lx] ", toff[tnum]);
        switch (tokens[tnum][0]) {
        case '!':
            parse_option_token(tokens[tnum], tnum);
            break;
        case '>':
            parse_command_token(tokens, &tnum, tcount);
            break;
        case 'p':
            printf("pos ");
            break;
        case 'k':
            parse_key(tokens[tnum]);
            break;
        case 'd':
            parse_dir(tokens[tnum]);
            break;
        case 'l':
            parse_line(tokens[tnum]);
            break;
        case 'y':
            parse_yesno(tokens[tnum]);
            break;
        case 'T':
            parse_timezone(tokens[tnum], tnum);
            break;
        case 'm':
            printf("menu ");
            break;
        case 'o':
            printf("objects ");
            break;
        case '<':
            parse_result(tokens[tnum]);
            printf("\n");
            break;
        default:
            printf("  [strange token]  ");
            break;
        }
    }

    return 0;
}
