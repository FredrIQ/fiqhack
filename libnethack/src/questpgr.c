/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
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
static const char *convert_arg(char c);
static const char *convert_line(const char *in_line);
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

static const char *
convert_arg(char c)
{
    switch (c) {
    case 'p':
        return msg_from_string(u.uplname);
    case 'c':
        return (u.ufemale && urole.name.f) ? urole.name.f : urole.name.m;
    case 'r':
        return rank_of(u.ulevel, Role_switch, u.ufemale);
    case 'R':
        return rank_of(MIN_QUEST_LEVEL, Role_switch, u.ufemale);
    case 's':
        return (u.ufemale) ? "sister" : "brother";
    case 'S':
        return (u.ufemale) ? "daughter" : "son";
    case 'l':
    case 'n': {
        int i = c == 'l' ? urole.ldrnum : urole.neminum;;
        return msgcat(type_is_pname(&mons[i]) ? "" : "the ",
                      mons[i].mname);
    }
    case 'i':
        return urole.intermed;
    case 'o':
        return the(artiname(urole.questarti));
    case 'g':
        return mons[urole.guardnum].mname;
    case 'G':
        return align_gtitle(u.ualignbase[A_ORIGINAL]);
    case 'H':
        return urole.homebase;
    case 'a':
        return align_str(u.ualignbase[A_ORIGINAL]);
    case 'A':
        return align_str(u.ualign.type);
    case 'd':
        return align_gname(u.ualignbase[A_ORIGINAL]);
    case 'D':
        return align_gname(A_LAWFUL);
    case 'C':
        return "chaotic";
    case 'N':
        return "neutral";
    case 'L':
        return "lawful";
    case 'x':
        return Blind ? "sense" : "see";
    case 'Z':
        return msg_from_string(gamestate.dungeons[0].dname);
    case '%':
        return "%";
    default:
        return "";
    }
}

static const char *
convert_line(const char *in_line)
{
    /* xcrypt needs us to allocate a buffer for it */
    char decrypted_line[strlen(in_line)+1];
    xcrypt(in_line, decrypted_line);
    const char *rv = "";
    char *c;

    /* Tokenize the decrypted line; we stop at \r, \n, or \0, and do
       special handling of "%" characters.

       The algorithm used here is quadratic (when linear is possible), but
       given that the lines are only 80 characters long, I feel that a clear
       algorithm is superior to a low computational complexity algorithm. */

    for (c = xcrypt(in_line, decrypted_line);; c++) {

        switch (*c) {

        case '\r':
        case '\n':
        case '\0':
            return rv;

        case '%':
            if (c[1]) {
                const char *conversion = convert_arg(*(++c));
                switch (*(++c)) {

                    /* insert "a"/"an" prefix */
                case 'A':
                    rv = msgcat(rv, An(conversion));
                    break;
                case 'a':
                    rv = msgcat(rv, an(conversion));
                    break;

                    /* capitalize */
                case 'C':
                    rv = msgcat(rv, msgupcasefirst(conversion));
                    break;

                    /* pluralize */
                case 'P':
                    /* Note: makeplural doesn't work on arbitrarily capitalized
                       strings */
                    rv = msgcat(rv, msgupcasefirst(makeplural(conversion)));
                    break;
                case 'p':
                    rv = msgcat(rv, makeplural(conversion));
                    break;

                    /* append possessive suffix */
                case 'S':
                    conversion = msgupcasefirst(conversion);
                    /* fall through */
                case 's':
                    rv = msgcat(rv, s_suffix(conversion));
                    break;

                    /* strip any "the" prefix */
                case 't':
                    if (!strncmpi(conversion, "the ", 4))
                        rv = msgcat(rv, conversion + 4);
                    else
                        rv = msgcat(rv, conversion);
                    break;

                default:
                    --c;        /* undo switch increment */
                    rv = msgcat(rv, conversion);
                    break;
                }
                break;
            }
            /* else fall through */
        default:
            rv = msgkitten(rv, *c);
            break;
        }
    }
}

static void
deliver_by_pline(struct qtmsg *qt_msg)
{
    long size;
    char in_line[81]; /* to match the fgets call below */

    for (size = 0; size < qt_msg->size; size += (long)strlen(in_line)) {
        dlb_fgets(in_line, 80, msg_file);
        const char *out_line = convert_line(in_line);
        pline("%s", out_line);
    }

}

static void
deliver_by_window(struct qtmsg *qt_msg)
{
    char in_line[81];
    boolean new_para = TRUE;
    const char *msg = "";
    int size;

    /* Don't show this in replay mode, because it would require a keystroke to
       dismiss. (The other uses of display_buffer are #verhistory and #license,
       both of which are zero-time; thus, this is the only way to produce an
       /uninteractible/ buffer, which is something we want to avoid.) */
    if (program_state.followmode == FM_REPLAY)
        return;

    for (size = 0; size < qt_msg->size; size += (long)strlen(in_line)) {
        dlb_fgets(in_line, 80, msg_file);
        const char *out_line = convert_line(in_line);

        /* We want to strip lone newlines, but leave sequences intact, or
           special formatting.

           TODO: This is a huge kluge. Be better at this. */
        if (!*out_line) {
            if (!new_para)
                msg = msgcat(msg, "\n \n");
            new_para = TRUE;
        } else {
            if (out_line[0] == ' ' && !new_para)
                msg = msgkitten(msg, '\n');
            else if (!new_para)
                msg = msgkitten(msg, ' ');
            new_para = FALSE;
        }

        msg = msgcat(msg, out_line);
    }

    display_buffer(msg, TRUE);
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

/* Note: this now only returns a suggestion; it no longer takes genocide into
   account, so that the caller can handle the RNG implications */
const struct permonst *
qt_montype(const d_level *dlev, enum rng rng)
{
    int qpm;

    if (rn2_on_rng(5, rng)) {
        qpm = urole.enemy1num;
        if (qpm != NON_PM && rn2_on_rng(5, rng))
            return &mons[qpm];
        return mkclass(dlev, urole.enemy1sym, 0, rng);
    }
    qpm = urole.enemy2num;
    if (qpm != NON_PM && rn2_on_rng(5, rng))
        return &mons[qpm];
    return mkclass(dlev, urole.enemy2sym, 0, rng);
}

/*questpgr.c*/
