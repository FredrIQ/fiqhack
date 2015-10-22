/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-10-22 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "epri.h"
#include "emin.h"
#include "edog.h"


/*VARARGS1*/
/* Note that these declarations rely on knowledge of the internals
 * of the variable argument handling stuff in "tradstdc.h"
 */

static void vpline(boolean nonblocking, boolean norepeat, const char *, va_list)
    PRINTFLIKE(3,0);


#define pline_body(line, nonblocking, norepeat) \
    do { \
    va_list the_args; \
    va_start (the_args, line); \
    vpline(nonblocking, norepeat, line, the_args); \
    va_end(the_args); \
    } while (0)


void
pline(const char *line, ...)
{
    pline_body(line, FALSE, FALSE);
}


void
pline_nomore(const char *line, ...)
{
    pline_body(line, TRUE, FALSE);
}


void
pline_once(const char *line, ...)
{
    pline_body(line, FALSE, TRUE);
}


void
pline_once_nomore(const char *line, ...)
{
    pline_body(line, TRUE, TRUE);
}


static void
vpline(boolean nonblocking, boolean norepeat, const char *line,
       va_list the_args)
{
    const char *pbuf;
    boolean repeated;
    int lastline;

    lastline = curline - 1;
    if (lastline < 0)
        lastline += MSGCOUNT;

    if (!line || !*line)
        return;

    pbuf = msgvprintf(line, the_args, TRUE);

    /* Remove all tabs from the string and replace them with spaces.  Some
       messages generated from menu use will have formatting tabs in them, but
       these look ugly in the message buffer (and also mess with the wrapping
       logic). */
    char *untabber = (char*)pbuf;
    while (*untabber != '\0') {
        if (*untabber == '\t')
            *untabber = ' ';
        untabber++;
    }

    line = pbuf;

    repeated = !strcmp(line, toplines[lastline]);
    if (norepeat && repeated)
        return;
    if (turnstate.vision_full_recalc)
        vision_recalc(0);
    if (u.ux)
        flush_screen();

    if (repeated) {
        toplines_count[lastline]++;
    } else {
        strcpy(toplines[curline], line);
        toplines_count[curline] = 1;
        curline++;
        curline %= MSGCOUNT;
    }
    if (nonblocking)
        (*windowprocs.win_print_message_nonblocking) (moves, line);
    else
        print_message(moves, line);
}


/*VARARGS1*/
void
You_hear(const char *line, ...)
{
    /* You can't hear while unconscious. */
    if (!canhear())
        return;

    va_list the_args;

    va_start(the_args, line);
    vpline(FALSE, FALSE, msgcat_many("You ", Underwater ? "barely " : "",
                                     "hear ", line, NULL), the_args);
    va_end(the_args);
}

/*VARARGS1*/
void
verbalize(const char *line, ...)
{
    va_list the_args;

    if (!canhear())
        return;

    va_start(the_args, line);
    vpline(FALSE, FALSE, msgcat_many("\"", line, "\"", NULL), the_args);
    va_end(the_args);
}

static void vraw_printf(const char *, va_list);

void
raw_printf(const char *line, ...)
{
    va_list the_args;

    va_start(the_args, line);
    vraw_printf(line, the_args);
    va_end(the_args);
}

static void
vraw_printf(const char *line, va_list the_args)
{
    if (!strchr(line, '%'))
        raw_print(line);
    else {
        /* We can't use msgvprintf here because the game might not be
           running. We use xmvasprintf instead (vasprintf would be a little more
           appropriate but might not be available), then transfer to the stack,
           so that there are no untracked allocations when we make the API
           call. */
        struct xmalloc_block *xm_temp = NULL;
        const char *fmtline = xmvasprintf(&xm_temp, line, the_args);
        char fmtline_onstack[strlen(fmtline) + 1];
        strcpy(fmtline_onstack, fmtline);
        xmalloc_cleanup(&xm_temp);

        raw_print(fmtline_onstack);
    }
}


/*VARARGS1*/
void
impossible_core(const char *file, int line, const char *s, ...)
{
    nonfatal_dump_core();

    va_list args;
    const char *pbuf;

    va_start(args, s);
    if (program_state.in_impossible)
        panic("impossible called impossible");
    program_state.in_impossible = 1;
    
    pbuf = msgvprintf(s, args, TRUE);
    paniclog("impossible", pbuf);
    DEBUG_LOG_BACKTRACE("impossible() called: %s\n", pbuf);

    va_end(args);
    program_state.in_impossible = 0;

    log_recover_core(get_log_start_of_turn_offset(), TRUE, pbuf, file, line);
}

const char *
align_str(aligntyp alignment)
{
    switch ((int)alignment) {
    case A_CHAOTIC:
        return "chaotic";
    case A_NEUTRAL:
        return "neutral";
    case A_LAWFUL:
        return "lawful";
    case A_NONE:
        return "unaligned";
    }
    return "unknown";
}

void
mstatusline(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    aligntyp alignment;
    const char *info, *monnambuf;

    if (you)
        alignment = u.ualign.type;
    else if (mon->ispriest || (mon->isminion && roamer_type(mon->data)))
        alignment = CONST_EPRI(mon)->shralign;
    else if (mon->isminion)
        alignment = EMIN(mon)->min_align;
    else {
        alignment = mon->data->maligntyp;
        alignment =
            (alignment > 0) ? A_LAWFUL :
            (alignment == A_NONE) ? A_NONE :
            (alignment < 0) ? A_CHAOTIC : A_NEUTRAL;
    }

    info = "";
    if (mon->mtame) {
        info = msgcat(info, ", tame");
        if (wizard) {
            info = msgprintf("%s (%d", info, mon->mtame);
            if (!mon->isminion)
                info = msgprintf("%s; hungry %u; apport %d", info,
                                 EDOG(mon)->hungrytime, EDOG(mon)->apport);
            info = msgcat(info, ")");
        }
    } else if (mon->mpeaceful)
        info = msgcat(info, ", peaceful");
    if (!you && mon->meating)
        info = msgcat(info, ", eating");
    if (sick(mon)) {
        if (mon == &youmonst) {
            info = msgcat(info, ", dying from");
            if (u.usick_type & SICK_VOMITABLE)
                info = msgcat(info, " food poisoning");
            if (u.usick_type & SICK_NONVOMITABLE) {
                if (u.usick_type & SICK_VOMITABLE)
                    info = msgcat(info, " and");
                info = msgcat(info, " illness");
            }
        } else
            info = msgcat(info, ", dying from illness");
    }
    if (petrifying(mon))
        info = msgcat(info, ", solidifying");
    if (sliming(mon))
        info = msgcat(info, ", becoming slimy");
    if (strangled(mon))
        info = msgcat(info, ", being strangled");
    if (vomiting(mon))
        info = msgcat(info, ", nauseated");    /* !"nauseous" */
    if (cancelled(mon))
        info = msgcat(info, ", cancelled");
    if (confused(mon))
        info = msgcat(info, ", confused");
    if (blind(mon))
        info = msgcat(info, ", blind");
    if (you && u.ucreamed) {
        if ((long)u.ucreamed < Blinded || Blindfolded ||
            !haseyes(mon->data) || !blind(mon))
            /* !blind means eyes of the overworld */
            info = msgcat(info, ", cover");
        info = msgcat(info, "ed by sticky goop");
    } /* note: "goop" == "glop"; variation is intentional */
    if (stunned(mon))
        info = msgcat(info, ", stunned");
    if (you) {
        if (!u.usteed && Wounded_legs) {
            const char *what = body_part(LEG);

            if (LWounded_legs && RWounded_legs)
                what = makeplural(what);
            info = msgcat_many(info, ", injured ", what, NULL);
        }
        if (Glib)
            info = msgcat_many(info, ", slippery ",
                               makeplural(body_part(HAND)), NULL);
    } else {
        if (mon->msleeping)
            info = msgcat(info, ", asleep");
        else if (mon->mfrozen || !mon->mcanmove)
            info = msgcat(info, ", can't move");
        /* [arbitrary reason why it isn't moving] */
        else if (mon->mstrategy & STRAT_WAITMASK)
            info = msgcat(info, ", meditating");
        else if (mon->mflee)
            info = msgcat(info, ", scared");
    }
    if ((you && u.utrap) || (!you && mon->mtrapped))
        info = msgcat(info, ", trapped");
    if (fast(mon))
        info = msgcat(info,
                      very_fast(mon) ? ", very fast" :
                      fast(mon) ? ", fast" : ", fast somehow");
    if (slow(mon))
        info = msgcat(info, ", slowed");
    if (m_mundetected(mon))
        info = msgcat(info, ", concealed");
    if (invisible(mon))
        info = msgcat(info, ", invisible");
    if (!you && mon == u.ustuck)
        info = msgcat(info,
                      (sticks(youmonst.data)) ? ", held by you" : Engulfed
                      ? (is_animal(u.ustuck->data) ? ", swallowed you" :
                         ", engulfed you") : ", holding you");
    else if (you && u.ustuck) {
        if (sticks(youmonst.data))
            info = msgcat(info, ", holding ");
        else
            info = msgcat(info, ", held by ");
        info = msgcat(info, mon_nam(u.ustuck));
    }
    if (mon == u.usteed)
        info = msgcat(info, ", carrying you");

    /* avoid "Status of the invisible newt ..., invisible" */
    /* and unlike a normal mon_nam, use "saddled" even if it has a name */
    monnambuf = (you ? u.uplname :
                 x_monnam(mon, ARTICLE_THE, NULL,
                          (SUPPRESS_IT | SUPPRESS_INVISIBLE), FALSE));

    pline("Status of %s (%s%s):  Level %d  HP %d(%d)  Def %d%s.", monnambuf,
          (!you || u.ualign.record == 3) ? "" :
          (u.ualign.record >= 20) ? "piously " :
          (u.ualign.record > 13) ? "devoutly " :
          (u.ualign.record > 8) ? "fervently " :
          (u.ualign.record > 3) ? "stridently " :
          (u.ualign.record >= 1) ? "haltingly " :
          (u.ualign.record == 0) ? "nominally " : "insufficiently ",
          align_str(alignment),
          you && Upolyd ? mons[u.umonnum].mlevel : you ? u.ulevel : mon->m_lev,
          you && Upolyd ? u.mh : m_mhp(mon),
          you && Upolyd ? u.mhmax : m_mhpmax(mon),
          10 - find_mac(mon), info);
}

/*pline.c*/
