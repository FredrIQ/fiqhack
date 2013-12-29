/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2013-12-29 */
/*      Copyright 1991, M. Stephenson             */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "dlb.h"

/*  quest-specific pager routines. */

#include "qtext.h"

#define QTEXT_FILE      "quest.dat"

static void Fread(void *, int, int, dlb *);
static struct qtmsg *construct_qtlist(long);
static struct qtmsg *msg_in(struct qtmsg *, int);
static void convert_arg(char c, char *buf);
static char *convert_line(const char *in_line);
static void deliver_by_pline(struct qtmsg *);
static void deliver_by_window(struct qtmsg *);

static struct qtlists qt_list;
static dlb *msg_file;


static void
Fread(void *ptr, int size, int nitems, dlb * stream)
{
    int cnt;

    if ((cnt = dlb_fread(ptr, size, nitems, stream)) != nitems) {

        panic("PREMATURE EOF ON QUEST TEXT FILE! Expected %d bytes, got %d",
              (size * nitems), (size * cnt));
    }
}

static struct qtmsg *
construct_qtlist(long hdr_offset)
{
    struct qtmsg *msg_list;
    int n_msgs;

    dlb_fseek(msg_file, hdr_offset, SEEK_SET);
    Fread(&n_msgs, sizeof (int), 1, msg_file);
    msg_list = malloc((unsigned)(n_msgs + 1) * sizeof (struct qtmsg));

    /* 
     * Load up the list.
     */
    Fread(msg_list, n_msgs * sizeof (struct qtmsg), 1, msg_file);

    msg_list[n_msgs].msgnum = -1;
    return msg_list;
}

void
load_qtlist(void)
{
    int n_classes, i;
    char qt_classes[N_HDR][LEN_HDR];
    long qt_offsets[N_HDR];

    msg_file = dlb_fopen(QTEXT_FILE, RDBMODE);
    if (!msg_file)
        panic("CANNOT OPEN QUEST TEXT FILE %s.", QTEXT_FILE);

    /* 
     * Read in the number of classes, then the ID's & offsets for
     * each header.
     */
    Fread(&n_classes, sizeof (int), 1, msg_file);
    Fread(&qt_classes[0][0], sizeof (char) * LEN_HDR, n_classes, msg_file);
    Fread(qt_offsets, sizeof (long), n_classes, msg_file);

    /* 
     * Now construct the message lists for quick reference later
     * on when we are actually paging the messages out.
     */

    qt_list.common = qt_list.chrole = NULL;

    for (i = 0; i < n_classes; i++) {
        if (!strncmp(COMMON_ID, qt_classes[i], LEN_HDR))
            qt_list.common = construct_qtlist(qt_offsets[i]);
        else if (!strncmp(urole.filecode, qt_classes[i], LEN_HDR))
            qt_list.chrole = construct_qtlist(qt_offsets[i]);
    }

    if (!qt_list.common || !qt_list.chrole)
        impossible("load_qtlist: cannot load quest text.");
    return;     /* no ***DON'T*** close the msg_file */
}

/* called at program exit */
void
unload_qtlist(void)
{
    if (msg_file)
        dlb_fclose(msg_file), msg_file = 0;
    if (qt_list.common)
        free(qt_list.common), qt_list.common = 0;
    if (qt_list.chrole)
        free(qt_list.chrole), qt_list.chrole = 0;
    return;
}

short
quest_info(int typ)
{
    switch (typ) {
    case 0:
        return urole.questarti;
    case MS_LEADER:
        return urole.ldrnum;
    case MS_NEMESIS:
        return urole.neminum;
    case MS_GUARDIAN:
        return urole.guardnum;
    default:
        impossible("quest_info(%d)", typ);
    }
    return 0;
}

boolean
is_quest_artifact(struct obj * otmp)
{
    return (boolean) (otmp->oartifact == urole.questarti);
}

static struct qtmsg *
msg_in(struct qtmsg *qtm_list, int msgnum)
{
    struct qtmsg *qt_msg;

    for (qt_msg = qtm_list; qt_msg->msgnum > 0; qt_msg++)
        if (qt_msg->msgnum == msgnum)
            return qt_msg;

    return NULL;
}

static void
convert_arg(char c, char *buf)
{
    switch (c) {
    case 'p':
        strcpy(buf, u.uplname);
        break;
    case 'c':
        strcpy(buf, (u.ufemale && urole.name.f) ? urole.name.f : urole.name.m);
        break;
    case 'r':
        strcpy(buf, rank_of(u.ulevel, Role_switch, u.ufemale));
        break;
    case 'R':
        strcpy(buf, rank_of(MIN_QUEST_LEVEL, Role_switch, u.ufemale));
        break;
    case 's':
        strcpy(buf, (u.ufemale) ? "sister" : "brother");
        break;
    case 'S':
        strcpy(buf, (u.ufemale) ? "daughter" : "son");
        break;
    case 'l':
    case 'n': {
        int i = c == 'l' ? urole.ldrnum : urole.neminum;;
        if (snprintf(buf, BUFSZ, "%s%s", type_is_pname(&mons[i]) ? "" : "the ",
                     mons[i].mname) >= BUFSZ)
            impossible("Quest monster name exceeds BUFSZ");
        break;
    }
    case 'i':
        strcpy(buf, urole.intermed);
        break;
    case 'o':
        strcpy(buf, the(artiname(urole.questarti)));
        break;
        break;
    case 'g':
        strcpy(buf, mons[urole.guardnum].mname);
        break;
    case 'G':
        strcpy(buf, align_gtitle(u.ualignbase[A_ORIGINAL]));
        break;
    case 'H':
        strcpy(buf, urole.homebase);
        break;
    case 'a':
        strcpy(buf, align_str(u.ualignbase[A_ORIGINAL]));
        break;
    case 'A':
        strcpy(buf, align_str(u.ualign.type));
        break;
    case 'd':
        strcpy(buf, align_gname(u.ualignbase[A_ORIGINAL]));
        break;
    case 'D':
        strcpy(buf, align_gname(A_LAWFUL));
        break;
    case 'C':
        strcpy(buf, "chaotic");
        break;
    case 'N':
        strcpy(buf, "neutral");
        break;
    case 'L':
        strcpy(buf, "lawful");
        break;
    case 'x':
        strcpy(buf, Blind ? "sense" : "see");
        break;
    case 'Z':
        strcpy(buf, dungeons[0].dname);
        break;
    case '%':
        strcpy(buf, "%");
        break;
    default:
        strcpy(buf, "");
        break;
    }
}

static char *
convert_line(const char *in_line)
{
    char *c, *cc, *out_line = malloc(BUFSZ * 2);
    char xbuf[BUFSZ], buf[BUFSZ];

    cc = out_line;
    for (c = xcrypt(in_line, xbuf); *c; c++) {

        *cc = 0;
        switch (*c) {

        case '\r':
        case '\n':
            *(++cc) = 0;
            return out_line;

        case '%':
            if (*(c + 1)) {
                convert_arg(*(++c), buf);
                switch (*(++c)) {

                    /* insert "a"/"an" prefix */
                case 'A':
                    strcat(cc, An(buf));
                    cc += strlen(cc);
                    continue;   /* for */
                case 'a':
                    strcat(cc, an(buf));
                    cc += strlen(cc);
                    continue;   /* for */

                    /* capitalize */
                case 'C':
                    buf[0] = highc(buf[0]);
                    break;

                    /* pluralize */
                case 'P':
                    buf[0] = highc(buf[0]);
                case 'p':
                    strcpy(buf, makeplural(buf));
                    break;

                    /* append possessive suffix */
                case 'S':
                    buf[0] = highc(buf[0]);
                case 's':
                    strcpy(buf, s_suffix(buf));
                    break;

                    /* strip any "the" prefix */
                case 't':
                    if (!strncmpi(buf, "the ", 4)) {
                        strcat(cc, &buf[4]);
                        cc += strlen(cc);
                        continue;       /* for */
                    }
                    break;

                default:
                    --c;        /* undo switch increment */
                    break;
                }
                strcat(cc, buf);
                cc += strlen(buf);
                break;
            }
            /* else fall through */
        default:
            *cc++ = *c;
            break;
        }
    }
    if (cc >= out_line + sizeof out_line)
        panic("convert_line: overflow");
    *cc = 0;
    return out_line;
}

static void
deliver_by_pline(struct qtmsg *qt_msg)
{
    long size;
    char in_line[BUFSZ];

    for (size = 0; size < qt_msg->size; size += (long)strlen(in_line)) {
        dlb_fgets(in_line, 80, msg_file);
        char *out_line = convert_line(in_line);
        pline(out_line);
        free(out_line);
    }

}

static void
deliver_by_window(struct qtmsg *qt_msg)
{
    long size;
    char in_line[BUFSZ];
    struct menulist menu;

    init_menulist(&menu);

    for (size = 0; size < qt_msg->size; size += (long)strlen(in_line)) {
        dlb_fgets(in_line, 80, msg_file);
        char *out_line = convert_line(in_line);
        add_menutext(&menu, out_line);
        free(out_line);
    }
    display_menu(menu.items, menu.icount, NULL, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    free(menu.items);
}

void
com_pager(int msgnum)
{
    struct qtmsg *qt_msg;

    if (!(qt_msg = msg_in(qt_list.common, msgnum))) {
        impossible("com_pager: message %d not found.", msgnum);
        return;
    }

    dlb_fseek(msg_file, qt_msg->offset, SEEK_SET);
    if (qt_msg->delivery == 'p')
        deliver_by_pline(qt_msg);
    else
        deliver_by_window(qt_msg);
    return;
}

void
qt_pager(int msgnum)
{
    struct qtmsg *qt_msg;

    if (!(qt_msg = msg_in(qt_list.chrole, msgnum))) {
        impossible("qt_pager: message %d not found.", msgnum);
        return;
    }

    dlb_fseek(msg_file, qt_msg->offset, SEEK_SET);
    if (qt_msg->delivery == 'p')
        deliver_by_pline(qt_msg);
    else
        deliver_by_window(qt_msg);
    return;
}

const struct permonst *
qt_montype(const d_level * dlev)
{
    int qpm;

    if (rn2(5)) {
        qpm = urole.enemy1num;
        if (qpm != NON_PM && rn2(5) && !(mvitals[qpm].mvflags & G_GENOD))
            return &mons[qpm];
        return mkclass(dlev, urole.enemy1sym, 0);
    }
    qpm = urole.enemy2num;
    if (qpm != NON_PM && rn2(5) && !(mvitals[qpm].mvflags & G_GENOD))
        return &mons[qpm];
    return mkclass(dlev, urole.enemy2sym, 0);
}

/*questpgr.c*/
