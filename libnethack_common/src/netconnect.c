/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-05 */
/* Copyright (c) Daniel Thaler, 2012. */
/* Copyright (c) 2014 Alex Smith. */
/* This network connection library may be freely redistributed under the terms of
 * either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 * Note that you do not have a warranty under either license.
 */

#include "netconnect.h"

/* convert a string (address or hosname) into an address */
int
parse_ip_addr(const char *host, int port, int want_v4,
              struct sockaddr_storage *out, int *errcode)
{
    int res;
    char portstr[16];
    struct addrinfo *gai_res = NULL;
    struct addrinfo gai_hints;

    memset(&gai_hints, 0, sizeof (gai_hints));
#if defined(WIN32) || defined(AIMAKE_BUILDOS_darwin)
    gai_hints.ai_flags = 0;
#else
    gai_hints.ai_flags = AI_NUMERICSERV;
#endif
    gai_hints.ai_family = want_v4 ? AF_INET : AF_INET6;
    gai_hints.ai_socktype = SOCK_STREAM;

    snprintf(portstr, sizeof(portstr), "%d", port);

    res = getaddrinfo(host, portstr, &gai_hints, &gai_res);
    if (errcode)
        *errcode = res;

    if (res != 0 || !gai_res)
        return FALSE;
    if (want_v4)
        memcpy(out, gai_res->ai_addr, sizeof (struct sockaddr_in));
    else
        memcpy(out, gai_res->ai_addr, sizeof (struct sockaddr_in6));

    freeaddrinfo(gai_res);

    return TRUE;
}

int
connect_server(const char *host, int port, int want_v4, char *errmsg,
               int msglen)
{
    struct sockaddr_storage sa;
    int fd = -1;
    int errcode;

    errmsg[0] = '\0';
    if (parse_ip_addr(host, port, want_v4, &sa, &errcode)) {
        fd = socket(sa.ss_family, SOCK_STREAM, 0);
        if (fd == -1) {
            snprintf(errmsg, msglen, "failed to create a socket: %s\n",
                     strerror(errno));
            return -1;
        }

        if (connect(fd, (struct sockaddr *)&sa,
                    want_v4 ? sizeof (struct sockaddr_in) :
                    sizeof (struct sockaddr_in6)) == -1) {
            snprintf(errmsg, msglen, "could not connect: %s\n",
                     strerror(errno));
            close(fd);
            return -1;
        }
    } else {
        snprintf(errmsg, msglen, "could not parse hostname: %s\n",
                 gai_strerror(errcode));
    }

    return fd;
}
