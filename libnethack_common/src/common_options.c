/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#define IN_LIBNETHACK_COMMON

#include "common_options.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct nh_option_desc *
nhlib_find_option(struct nh_option_desc *optlist, const char *name)
{
    int i;

    for (i = 0; optlist[i].name; i++)
        if (!strcmp(name, optlist[i].name))
            return &optlist[i];

    return NULL;
}

const struct nh_option_desc *
nhlib_const_find_option(const struct nh_option_desc *optlist, const char *name)
{
    int i;

    for (i = 0; optlist[i].name; i++)
        if (!strcmp(name, optlist[i].name))
            return &optlist[i];

    return NULL;
}

nh_bool *
nhlib_find_boolopt(const struct nhlib_boolopt_map *map, const char *name)
{
    int i;

    for (i = 0; map[i].optname; i++)
        if (!strcmp(name, map[i].optname))
            return map[i].addr;

    return NULL;
}

union nh_optvalue
nhlib_string_to_optvalue(const struct nh_option_desc *option, char *str)
{
    union nh_optvalue value;
    int i;

    value.i = -99999;

    switch (option->type) {
    case OPTTYPE_BOOL:
        if (!strcmp(str, "TRUE") || !strcmp(str, "true") || !strcmp(str, "1"))
            value.b = TRUE;
        else if (!strcmp(str, "FALSE") || !strcmp(str, "false") ||
                 !strcmp(str, "0"))
            value.b = FALSE;
        else
            value.i = 2;        /* intentionally invalid */

        break;

    case OPTTYPE_INT:
        sscanf(str, "%d", &value.i);
        break;

    case OPTTYPE_ENUM:
        for (i = 0; i < option->e.numchoices; i++)
            if (!strcmp(str, option->e.choices[i].caption))
                value.e = option->e.choices[i].id;
        break;

    case OPTTYPE_STRING:
        if (*str)
            value.s = str;
        else
            value.s = NULL;
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        value.ar = nhlib_parse_autopickup_rules(str);
        break;
    }

    return value;
}

static char *
autopickup_to_string(const struct nh_autopickup_rules *ar)
{
    int size, i;
    char *buf, *bp, pattern[40];

    if (!ar || !ar->num_rules) {
        buf = malloc(1);
        *buf = 0;
        return buf;
    }

    /* at this point, size is an upper bound on the stringified length of ar 3
       stringified small numbers + a pattern with up to 40 chars < 64 chars */
    size = 64 * ar->num_rules;
    buf = malloc(size);
    buf[0] = '\0';

    for (i = 0; i < ar->num_rules; i++) {
        strncpy(pattern, ar->rules[i].pattern, sizeof (pattern));
        pattern[(sizeof pattern) - 1] = 0;

        /* remove '"' and ';' from the pattern by replacing them by '?' (single 
           character wildcard), to simplify parsing */
        bp = pattern;
        while (*bp) {
            if (*bp == '"' || *bp == ';')
                *bp = '?';
            bp++;
        }

        snprintf(buf + strlen(buf), 64, "(\"%s\",%d,%u,%u);", pattern,
                 ar->rules[i].oclass, ar->rules[i].buc, ar->rules[i].action);
    }

    return buf;
}

char *
nhlib_optvalue_to_string(const struct nh_option_desc *option)
{
    char valbuf[10], *outstr;
    const char *valstr = NULL;
    char *valstr_malloced = NULL;
    int i;

    switch (option->type) {
    case OPTTYPE_BOOL:
        valstr = option->value.b ? "true" : "false";
        break;

    case OPTTYPE_ENUM:
        valstr = "(invalid)";
        for (i = 0; i < option->e.numchoices; i++)
            if (option->value.e == option->e.choices[i].id)
                valstr = option->e.choices[i].caption;
        break;

    case OPTTYPE_INT:
        snprintf(valbuf, sizeof(valbuf), "%d", option->value.i);
        valstr = valbuf;
        break;

    case OPTTYPE_STRING:
        if (!option->value.s)
            valstr = "";
        else
            valstr = option->value.s;
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        valstr_malloced = autopickup_to_string(option->value.ar);
        break;

    default:   /* custom option type defined by the client? */
        return NULL;
    }

    if (valstr_malloced)
        return valstr_malloced;

    outstr = malloc(strlen(valstr) + 1);
    strcpy(outstr, valstr);
    return outstr;
}


struct nh_autopickup_rules *
nhlib_parse_autopickup_rules(const char *str)
{
    struct nh_autopickup_rules *out;
    char *copy, *semi;
    const char *start;
    int i, rcount = 0;
    unsigned int action, buc;

    if (!str || !*str)
        return NULL;

    start = str;
    while ((semi = strchr(start, ';'))) {
        start = ++semi;
        rcount++;
    }

    if (!rcount)
        return NULL;

    out = malloc(sizeof(struct nh_autopickup_rules));
    out->rules = malloc(sizeof(struct nh_autopickup_rule) * rcount);
    out->num_rules = rcount;

    i = 0;
    start = copy = malloc(strlen(str) + 1);
    strcpy(copy, str);

    while ((semi = strchr(start, ';')) && i < rcount) {
        *semi++ = '\0';
        /* This memset is mostly unnecessary, but it ensures that pattern is
           fully initialized, thus allowing us to copy the spare bytes into
           the save file without reading uninitialized data. */
        memset(out->rules[i].pattern, 0, sizeof (out->rules[i].pattern));
        sscanf(start, "(\"%39[^,],%d,%u,%u);", out->rules[i].pattern,
               &out->rules[i].oclass, &buc, &action);
        /* since %[ in sscanf requires a nonempty match, we allowed it to match
           the closing '"' of the rule. Remove that now. */
        out->rules[i].pattern[strlen(out->rules[i].pattern) - 1] = '\0';
        out->rules[i].buc = (enum nh_bucstatus)buc;
        out->rules[i].action = (enum autopickup_action)action;
        i++;
        start = semi;
    }

    free(copy);
    return out;
}

nh_bool
nhlib_option_value_ok(const struct nh_option_desc *option,
                      union nh_optvalue value)
{
    int i;

    switch (option->type) {
    case OPTTYPE_BOOL:
        if (value.b == !!value.b)
            return TRUE;
        break;

    case OPTTYPE_INT:
        if (value.i >= option->i.min && value.i <= option->i.max)
            return TRUE;
        break;

    case OPTTYPE_ENUM:
        for (i = 0; i < option->e.numchoices; i++)
            if (value.e == option->e.choices[i].id)
                return TRUE;
        break;

    case OPTTYPE_STRING:
        if (!value.s)
            break;

        if (strlen(value.s) > option->s.maxlen)
            break;

        if (!*value.s)
            value.s = NULL;

        return TRUE;

    case OPTTYPE_AUTOPICKUP_RULES:
        if (value.ar && value.ar->num_rules > AUTOPICKUP_MAX_RULES)
            break;
        return TRUE;
    }

    return FALSE;
}

struct nh_autopickup_rules *
nhlib_copy_autopickup_rules(const struct nh_autopickup_rules *in)
{
    struct nh_autopickup_rules *out;
    int size;

    if (!in || !in->num_rules)
        return NULL;

    out = malloc(sizeof (struct nh_autopickup_rules));
    out->num_rules = in->num_rules;
    size = out->num_rules * sizeof (struct nh_autopickup_rule);
    out->rules = malloc(size);
    memcpy(out->rules, in->rules, size);

    return out;
}

/* copy values carefully: copying pointers to strings on the stack is not good
   return TRUE if the new value differs from the current value */
nh_bool
nhlib_copy_option_value(struct nh_option_desc *option, union nh_optvalue value)
{
    struct nh_autopickup_rules *aold, *anew;
    int i;

    switch (option->type) {
    case OPTTYPE_STRING:
        if (option->value.s == value.s ||
            (option->value.s && value.s && !strcmp(option->value.s, value.s)))
            return FALSE;       /* setting the option to it's current value;
                                   nothing to copy */

        if (option->value.s)
            free(option->value.s);
        option->value.s = NULL;
        if (value.s) {
            option->value.s = malloc(strlen(value.s) + 1);
            strcpy(option->value.s, value.s);
        }
        break;

    case OPTTYPE_AUTOPICKUP_RULES:
        aold = option->value.ar;
        anew = value.ar;

        if (!aold && !anew)
            return FALSE;

        /* check rule set equality */
        if (aold && anew && aold->num_rules == anew->num_rules) {
            /* compare each individual rule */
            for (i = 0; i < aold->num_rules; i++)
                if (strcmp(aold->rules[i].pattern, anew->rules[i].pattern) ||
                    aold->rules[i].oclass != anew->rules[i].oclass ||
                    aold->rules[i].buc != anew->rules[i].buc ||
                    aold->rules[i].action != anew->rules[i].action)
                    break;      /* rule difference found */
            if (i == aold->num_rules)
                return FALSE;
        }

        if (aold) {
            free(aold->rules);
            free(aold);
        }

        option->value.ar = nhlib_copy_autopickup_rules(anew);
        break;

    case OPTTYPE_BOOL:
        if (option->value.b == value.b)
            return FALSE;
        option->value.b = value.b;
        break;

    case OPTTYPE_ENUM:
        if (option->value.e == value.e)
            return FALSE;
        option->value.e = value.e;
        break;

    case OPTTYPE_INT:
        if (option->value.i == value.i)
            return FALSE;
        option->value.i = value.i;
        break;
    }

    return TRUE;
}


void
nhlib_free_optlist(struct nh_option_desc *opt)
{
    int i;

    if (!opt)
        return;

    for (i = 0; opt[i].name; i++) {
        if (opt[i].type == OPTTYPE_STRING && opt[i].value.s)
            free(opt[i].value.s);
        else if (opt[i].type == OPTTYPE_AUTOPICKUP_RULES && opt[i].value.ar) {
            free(opt[i].value.ar->rules);
            free(opt[i].value.ar);
        }
    }

    free(opt);
}


struct nh_option_desc *
nhlib_clone_optlist(const struct nh_option_desc *in)
{
    int i;
    struct nh_option_desc *out;

    for (i = 0; in[i].name; i++)
        ;
    i++;
    out = malloc(sizeof (struct nh_option_desc) * i);
    memcpy(out, in, sizeof (struct nh_option_desc) * i);

    for (i = 0; in[i].name; i++) {
        if (in[i].type == OPTTYPE_STRING && in[i].value.s)
            out[i].value.s = strdup(in[i].value.s);
        else if (in[i].type == OPTTYPE_AUTOPICKUP_RULES && in[i].value.ar)
            out[i].value.ar = nhlib_copy_autopickup_rules(in[i].value.ar);
    }

    return out;
}
