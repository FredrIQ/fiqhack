/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-12 */
/* Copyright (c) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include <string.h>
#include "xmalloc.h"

struct tm; /* no need to include time.h, we never inspect a struct tm */

/* Code for handling messages, a special case of strings designed to have
   particularly convenient memory management properties.

   A message is one of the following possibilities: a "const char *" that exists
   somewhere within the turnstate xmalloc chain (i.e. it can point to the start
   of the string part of the structure, or somewhere inside the string part); or
   a string literal.  The latter sort do not need allocation or deallocation;
   for the former sort, they are deallocated at neutral turnstate time.

   Thus, the only care needed when handling allocation for messages is that they
   do not try to persist past a neutral turnstate.  Typically, the code that
   messages replace was previously using an auto buffer of fixed size; so long
   as the code in question is not outside the main loop (hardly any code is
   outside the main loop), the code can be converted over directly.

   Messages are immutable once created. This means that when returning a
   message, if you make no changes to it, it's safe to return a message you were
   given as an argument directly. (They can be mutable during the creation
   process, but are immutable upon leaving this file.)

   Selections made from menus are not strings, but have the same lifetime
   properties as messages, and thus are also handled by this file. */

static char *msgcat_core(va_list args, size_t preceding);
static char charid(char c);

/* Produces a message that's a copy of the given string (which need not be a
   message, e.g. it might be on the stack somewhere). */
const char *
msg_from_string(const char *string)
{
    char *msg = xmalloc(&turnstate.message_chain, strlen(string) + 1);
    strcpy(msg, string);
    return msg;
}

/* Returns a message produced according to the given printf-style format string
   and varargs list. With sanitize_whitespace set, the only whitespace allowed
   in the output will be spaces (TODO: this seems not to be implemented?). */
const char *
msgvprintf(const char *fmt, va_list args, boolean sanitize_whitespace)
{
    return xmvasprintf(&turnstate.message_chain, fmt, args);
}

/* Returns a message produced according to the given printf-style format
   string. */
const char *
msgprintf(const char *fmt, ...)
{
    va_list args;
    const char *rv;
    
    va_start(args, fmt);
    rv = msgvprintf(fmt, args, FALSE);
    va_end(args);

    return rv;
}

/* Returns a message produced by formatting the given date according to the
   given format string. */
const char *
msgstrftime(const char *fmt, const struct tm *tm)
{
    /* Attempt to work around a deficiency in strftime's specification that
       causes xmastrftime to do bad things (= undefined behaviour) if it would
       produce zero bytes of output: barring crazy malicious input that uses
       platform-specific extensions to produce a zero-byte string, a zero-byte
       output can only come from a zero-byte input. */
    if (!*fmt)
        return "";

    return xmastrftime(&turnstate.message_chain, fmt, tm);
}

/* Returns the concatenation of two messages. */
const char *
msgcat(const char *first, const char *second)
{
    return msgcat_many(first, second, NULL);
}

/* Returns the concatenation of more than two messages; the messages are given
   as a NULL-terminated varargs list. */
const char *
msgcat_many(const char *first, ...)
{
    va_list args;
    size_t firstlen;
    char *rv;

    va_start(args, first);

    firstlen = strlen(first);
    rv = msgcat_core(args, firstlen); /* handles args, terminating NUL */
    memcpy(rv, first, firstlen);      /* handles first */

    va_end(args);
    return rv;
}

/* Allocates a string with length equal to "preceding" plus the total lengths
   of the "args" (plus a terminating NUL); then copies the concatenation of
   those args into characters from "preceding" onwards, leaving the earlier
   areas of the string uninitialized. */
static char *
msgcat_core(va_list args, size_t preceding)
{
    const char *next = va_arg(args, const char *);
    char *rv;
    if (!next) {

        rv = xmalloc(&turnstate.message_chain, preceding + 1);
        /* Write the NUL now, in the base case. */
        rv[preceding] = '\0';
        return rv;

    } else {

        size_t nextlen = strlen(next);
        /* Write the rest of the args and the NUL. */
        rv = msgcat_core(args, preceding + nextlen); 
        /* Write the current arg. */
        memcpy(rv + preceding, next, nextlen);
        return rv;

    }
}

/* Returns the concatenation of a message and a character. (This is to reduce
   code duplication from the callers having to create temporary arrays to
   stringify the character.) */
const char *
msgkitten(const char *first, char second)
{
    char kitten[2] = {second, '\0'};
    return msgcat(first, kitten);
}

/* Returns a message equal to the given message but with characters removed from
   the right end; throws an impossible() and returns an empty message if this
   would remove more characters than exist in the message, returns (most likely
   a clone of) the original message if it would remove a negative number of
   characters (due to "count" being positive and longer than the string). (To
   remove characters from the /left/, just add to the message pointer.) Count
   can be positive to be the number of characters to retain, or negative to be
   the number of characters to remove. */
const char *
msgchop(const char *message, int count)
{
    if (count < 0)
        count += strlen(message);
    if (count < 0) {
        impossible("msgchop() called with overlength negative count: "
                   "string '%s', over by '%d' chars", message, -count);
        return "";
    } else if (count == 0)
        return "";

    char *rv = xmalloc(&turnstate.message_chain, count + 1);
    strncpy(rv, message, count);
    rv[count] = '\0';
    return rv;
}

/* Performs mungspaces() on a message, returning the new munged message. */
const char *
msgmungspaces(const char *message)
{
    char msgcopy[strlen(message) + 1];
    strcpy(msgcopy, message);
    mungspaces(msgcopy);
    return msg_from_string(msgcopy);
}

/* Returns a message equal to the given message except with the first letter and
   all letters immediately after spaces capitalized. */
const char *
msgtitlecase(const char *message)
{
    return msgcaseconv(message, highc, charid, highc);
}

/* Returns a message equal to the given message except that the first character
   is capitalized. */
const char *
msgupcasefirst(const char *message)
{
    return msgcaseconv(message, highc, charid, charid);
}

/* Returns a message equal to the given message except that all characters are
   in lowercase.  */
const char *
msglowercase(const char *message)
{
    return msgcaseconv(message, lowc, lowc, lowc);
}

/* Applies the given case transformation to a message, producing a new
   message. */
const char *
msgcaseconv(const char *message,
            char (*firstcharcaseconv)(char),
            char (*insidewordcaseconv)(char),
            char (*wordstartcaseconv)(char))
{
    char *rv = xmalloc(&turnstate.message_chain, strlen(message) + 1);
    char (*caseconv)(char) = firstcharcaseconv;
    size_t i;
    for (i = 0; message[i]; i++) {
        rv[i] = caseconv(message[i]);
        caseconv = message[i] == ' ' ?
            wordstartcaseconv : insidewordcaseconv;
    }
    rv[i] = '\0';

    return rv;
}


/* No-op case transformation. This exists as a function so that pointers to it
   can be taken. (Other case transformations include highc and lowc, in
   hacklib.c.) */
static char
charid(char c)
{
    return c;
}


/* Callbacks that convert the API's allocation scheme into messages. */
void
msg_request_command_callback(const struct nh_cmd_and_arg *cmd,
                             void *ncaa_to_fill)
{
    struct nh_cmd_and_arg *ncaa = ncaa_to_fill;
    ncaa->cmd = msg_from_string(cmd->cmd);
    ncaa->arg = cmd->arg;
    if (ncaa->arg.argtype & CMD_ARG_STR)
        ncaa->arg.str = msg_from_string(ncaa->arg.str);
    else
        ncaa->arg.str = NULL;
}
void
msg_getlin_callback(const char *str, void *msg_to_fill)
{
    const char **msg = msg_to_fill;
    *msg = msg_from_string(str);
}

void
msg_display_menu_callback(const int *results, int nresults, void *dmcd_to_fill)
{
    struct display_menu_callback_data *dmcd = dmcd_to_fill;
    int *msg_results;

    dmcd->nresults = nresults;
    if (nresults <= 0)
        return;

    msg_results = xmalloc(&turnstate.message_chain, nresults * sizeof *results);
    memcpy(msg_results, results, nresults * sizeof *results);

    dmcd->results = msg_results;
    dmcd->nresults = nresults;
}

void
msg_display_objects_callback(const struct nh_objresult *results, int nresults,
                             void *docd_to_fill)
{
    struct display_objects_callback_data *docd = docd_to_fill;
    struct nh_objresult *msg_results;

    docd->nresults = nresults;
    if (nresults <= 0)
        return;

    msg_results = xmalloc(&turnstate.message_chain, nresults * sizeof *results);
    memcpy(msg_results, results, nresults * sizeof *results);

    docd->results = msg_results;
}
