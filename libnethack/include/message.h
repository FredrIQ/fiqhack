/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Steve Creps, 1988.                               */
/* Copyright (c) Alex Smith, 2015. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MESSAGE_H
# define MESSAGE_H

#include "compilers.h"
#include "messagechannel.h"
#include <stdarg.h>

/* 
 * This file contains two sets of message-handling code:
 *
 * - pline() and friends, which handle printing messages;
 * - msgcat() and friends, which handle allocating messages.
 *
 * All the declarations for the relevant functions, enums, etc. are here rather
 * than in extern.h (because extern.h should ideally get smaller over time).
 *
 * A message itself is a "const char *" that's either a string literal, or else
 * on the message chain, and is for most purposes treated as a read-only object
 * that spontaneously deallocates at the end of the turn (and should not be
 * deallocated manually).  Note that pointer arithmetic on a message, to remove
 * characters from the left hand side, is allowed and will work correctly. NULL
 * is not valid as a message (although you can certainly write a function that
 * takes a "message-or-NULL" argument if you want to).
 */

/******************************************************************************/

/* Message printing functions, from pline.c. Note that these are using printf's
   calling convention (with an extra argument for message channel), and in
   particular don't require the format string or "%s" arguments to be messages,
   any sort of string will do. (They'll be converted into messages internally if
   necessary.) */

/* Print a message to the player (in the message area). The caller is
   responsible for doing any checks that determine that the player should see
   the message in question. */
extern void pline(enum msg_channel, const char *, ...) PRINTFLIKE(2,3);

/* Like pline(), but will hide a message if it's identical to the previous
   message shown. */
extern void pline_once(enum msg_channel, const char *, ...) PRINTFLIKE(2,3);

/* Like pline(), but the message can be deduced from other messages shown (and
   thus experienced players may want to hide it). */
extern void pline_implied(enum msg_channel, const char *, ...) PRINTFLIKE(2,3);

/* Typically pline("You hear %s"), but with checks to ensure that the character
   actually can hear. */
extern void You_hear(enum msg_channel, const char *, ...) PRINTFLIKE(2,3);

/* pline() with extra double quotes added: for speech that an NPC is saying. */
extern void verbalize(enum msg_channel, const char *, ...) PRINTFLIKE(2,3);

/* pline() variant for emergency use. Does not rely on the existence of the
   message chain, and will attempt to output to both the player's screen and the
   original stdout/stderr. Useable when the game is not running. */
extern void raw_printf(const char *, ...) PRINTFLIKE(1,2);

/******************************************************************************/

/* Message allocation functions, from messages.c. These all return messages as
   their return value, and in most cases require messages as their arguments. */

/* Return a message, given a string as argument. Useful to supply arbitrary
   strings to functions that expect messages. */
extern const char *msg_from_string(const char *string) USE_RETVAL;

/* vsprintf variant that returns a message rather than outputting to a buffer.
   The arguments can be anything acceptable to vsprintf, and need not be
   messages. sanitize_whitespace is intended to replace all whtespace with
   spaces, but is currently unimplemented. */
extern const char *msgvprintf(const char *fmt, va_list args,
                              int sanitize_whitespace)
    USE_RETVAL PRINTFLIKE(1,0);

/* sprintf variant that returns a message rather than outputting to a buffer.
   The arguments can be anything acceptable to sprintf, and need not be
   messages. */
extern const char *msgprintf(const char *fmt, ...)
    USE_RETVAL PRINTFLIKE(1,2);

/* strftime variant that returns a message rather than outputting to a buffer.
   The format string can be any string, and need not be a message. */
struct tm;
extern const char *msgstrftime(const char *fmt, const struct tm *tm)
    USE_RETVAL STRFTIMELIKE(1,0);

/* Creates a new message as the concatenation of two messages. */
extern const char *msgcat(const char *first, const char *second)
    USE_RETVAL;

/* Creates a new message as the concatenation of any hardcoded positive number
   of messages (but typically three or more, otherwise you'd just use msgcat).
   Takes one argument per such message, and a NULL at the end of the list. */
extern const char *msgcat_many(const char *first, ...) USE_RETVAL SENTINEL;

/* Creates a new message as the concatenation of a message and a single
   character. (This is so that you don't need to make a temporary array to hold
   the single character in question.) */
extern const char *msgkitten(const char *first, char second) USE_RETVAL;

/* Creates a new message as a copy of an existing message, but with characters
   removed from the right. A positive count is the number of characters to
   retain; a negative count is the number of characters to remove. (Use pointer
   arithmetic to create a copy of a message with characters removed from the
   left.) */
extern const char *msgchop(const char *message, int count) USE_RETVAL;

/* Creates a new message as a copy of an existing message, removing horizontal
   whitespace at the start and end, and compacting runs of internal horizontal
   whitespace into a single space character. */
extern const char *msgmungspaces(const char *message) USE_RETVAL;

/* Creates a new message as a copy of an existing message, capitalizing each
   (ASCII) letter that comes after a space. */
extern const char *msgtitlecase(const char *message) USE_RETVAL;

/* Creates a new message as a copy of an existing message, capitalizing the
   first character if it's an ASCII letter. */
extern const char *msgupcasefirst(const char *message) USE_RETVAL;

/* Creates a new message as a copy of an existing message, lowercasing every
   (ASCII) letter. */
extern const char *msglowercase(const char *message) USE_RETVAL;

/* Creates a new message as a copy of an existing message, with each character
   in it transformed by one of three given transformation functions (based on
   whether it's the first character, after a nonspace, or after a space). */
extern const char *msgcaseconv(const char *message,
                               char (*firstcharcaseconv)(char),
                               char (*insidewordcaseconv)(char),
                               char (*wordstartcaseconv)(char)) USE_RETVAL;

#endif
