/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-13 */
/* Copyright (c) 2014 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "netconnect.h"
#include "nhcurses.h"

/* Currently FIQHack does not have a motd server. */
/* #define MOTD_SERVER "motd.nethack4.org" */
/* #define MOTD_PORT   53401 */

int
network_motd(void)
{
#ifdef MOTD_SERVER
    char errmsg[256];
#endif
    char motdmsg[4096];
    int fd = -1;

    if (settings.show_motd == MOTD_TRUE) {

#ifndef MOTD_SERVER
        strcpy(motdmsg, "There is no Message of the Day server available.");
#else
        fd = connect_server(MOTD_SERVER, MOTD_PORT, FALSE,
                            errmsg, sizeof errmsg);
        if (fd == -1)
            fd = connect_server(MOTD_SERVER, MOTD_PORT, TRUE,
                                errmsg, sizeof errmsg);

        errmsg[sizeof errmsg - 1] = '\0';

        if (fd == -1) {
            strcpy(motdmsg, "Could not connect to <" MOTD_SERVER "> to "
                   "receive a Message of the Day: ");
            strcat(motdmsg, errmsg);
        } else {
            /* We want to receive until the connection closes (which causes
               either EPIPE on abnormal shutdown, or 0 on normal shutdown). So
               continue until we get an error message other than EINTR or the
               buffer fills (indicating a malicious connection; I'm not planning
               on sending malicious packets from motd.nethack4.org, especially
               as I wrote this code and so know it wouldn't work, but it's worth
               allowing for the possibility that someone else intercepts the
               connection). */
            int recvlen = 0;
            int rv;
            while (recvlen < sizeof motdmsg &&
                   (((rv = recv(fd, motdmsg + recvlen,
                                (sizeof motdmsg) - recvlen, 0))) > 0 ||
                    errno == EINTR))
                recvlen += rv < 0 ? 0 : rv;
            if (recvlen >= sizeof motdmsg)
                recvlen = -1 + sizeof motdmsg;

            motdmsg[recvlen] = '\0';
        }

        close(fd);
#endif
    } else if (settings.show_motd == MOTD_FALSE) {
        return 1;
    } else {
        /* It's a bad idea to do network connections without asking the user for
           permission first. (Arguably we could make an exception for
           connection-only mode, but that connects to localhost, which is not
           quite the same thing as connecting to the Internet, so I'd rather
           make absolutely sure we aren't doing connections unsolicited.)

           Note that nothing is sent (other than the fact that the connection
           exists); the nethack4 binary just creates the connection, then reads
           from it. */
#ifdef MOTD_SERVER
        strcpy(motdmsg, "The Message of the Day system connects to the "
               "Internet to receive gameplay tips and announcements (such "
               "as tournament information or release announcements). Do you "
               "want to turn it on? (You can change this later with the "
               "\x0enetwork_motd\x0f option.)");
#else
        return 1;
#endif
    }

    /* SI/SO in the output indicate bold text. This isn't implemented yet.  Also
       strip out all other unprintable characters for security reasons. We just
       use the ASCII space-to-tilde range for printables; we're not expecting
       any control characters but SI/SO, not even newlines. */
    char *f, *t;
    f = t = motdmsg;
    while (*f) {
        if (*f >= ' ' && *f <= '~')
            *(t++) = *f;
        f++;
    }
    *t = '\0';

    int outcount;
    char **outlines;
    wrap_text(COLNO-6, motdmsg, &outcount, &outlines);

    struct nh_menulist menu;
    int i;

    init_menulist(&menu);

    for (i = 0; i < outcount; i++)
        add_menu_txt(&menu, outlines[i], MI_TEXT);

    free_wrap(outlines);

    if (settings.show_motd == MOTD_ASK) {
        add_menu_txt(&menu, "", MI_TEXT);
        add_menu_item(&menu, 1,
                      "Yes, I'd like announcements and gameplay tips",
                      'y', FALSE);
        add_menu_item(&menu, 2,
                      "No, please don't connect to the MotD server",
                      'n', FALSE);

        curses_display_menu(&menu, "Message of the Day", PICK_ONE,
                            PLHINT_ANYWHERE, &i, curses_menu_callback);

        if (i == 1) {
            curses_set_option("networkmotd",
                              (union nh_optvalue){.e = MOTD_TRUE});
            write_ui_config();
            return network_motd();
        } else {
            curses_set_option("networkmotd",
                              (union nh_optvalue){.e = MOTD_FALSE});
            write_ui_config();
        }
    } else 
        curses_display_menu(&menu, "Message of the Day", PICK_NONE,
                            PLHINT_ANYWHERE, NULL, null_menu_callback);
    return fd != -1;
}
