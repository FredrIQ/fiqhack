/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-09-01 */
/* Copyright (c) Daniel Thaler, 2012. */
/* Copyright (c) 2014 Alex Smith. */
/* This network connection library may be freely redistributed under the terms of
 * either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 * Note that you do not have a warranty under either license.
 */

#ifndef NETCONNECT_H
# define NETCONNECT_H

# include "nethack_types.h"
# include <sys/types.h>
# include <string.h>
# include <errno.h>
# include <stdio.h>

/* We don't have nethack_types to help us out with OS detection when
   calculating dependencies, so redo the calculation here. */
# if defined(UNIX) || (!defined(WIN32) && !defined(AIMAKE_BUILDOS_MSWin32))
#  include <unistd.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <netdb.h>
#  include <sys/un.h>
#  include <sys/select.h>
#  include <sys/time.h>
# else
#  include <winsock2.h>
#  include <ws2def.h>
#  include <ws2tcpip.h>

#  define snprintf(buf, len, fmt, ...) \
    _snprintf_s(buf, len, len-1, fmt, __VA_ARGS__)
#  define close closesocket

/* TODO: Find the header file this is defined in, and include it. */
extern int _snprintf_s(char *, size_t, size_t, const char *, ...);

# endif

extern int parse_ip_addr(const char *host, int port, int want_v4,
                         struct sockaddr_storage *out, int *errcode);
extern int connect_server(const char *host, int port, int want_v4,
                          char *errmsg, int msglen);

#endif
