/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Sean Hunt, 2014-01-27 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "epri.h"
#include "emin.h"
#include "edog.h"

static char *You_buf(int);


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
vpline(boolean nonblocking, boolean norepeat, const char *line, va_list the_args)
{
    char pbuf[BUFSZ], *c;
    boolean repeated;
    int lastline;

    lastline = curline - 1;
    if (lastline < 0)
        lastline += MSGCOUNT;

    if (!line || !*line)
        return;


    vsnprintf(pbuf, BUFSZ, line, the_args);

    /* Sanitize, otherwise the line can mess up the message window and message
       history. */
    for (c = pbuf; *c && c < pbuf + BUFSZ; c++) {
        if (*c == '\n' || *c == '\t')
            *c = ' ';
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


/* work buffer for You(), &c and verbalize() */
static char *you_buf = 0;
static int you_buf_siz = 0;

static char *
You_buf(int siz)
{
    if (siz > you_buf_siz) {
        if (you_buf)
            free(you_buf);
        you_buf_siz = siz + 10;
        you_buf = malloc((unsigned)you_buf_siz);
    }
    return you_buf;
}

void
free_youbuf(void)
{
    if (you_buf)
        free(you_buf), you_buf = NULL;
    you_buf_siz = 0;
}

/* `prefix' must be a string literal, not a pointer */
#define YouPrefix(pointer,prefix,text) \
 strcpy((pointer = You_buf((int)(strlen(text) + sizeof prefix))), prefix)

#define YouMessage(pointer,prefix,text) \
 strcat((YouPrefix(pointer, prefix, text), pointer), text)

/*VARARGS1*/
void
You_hear(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    va_start(the_args, line);
    if (Underwater)
        YouPrefix(tmp, "You barely hear ", line);
    else if (u.usleep)
        YouPrefix(tmp, "You dream that you hear ", line);
    else
        YouPrefix(tmp, "You hear ", line);
    vpline(FALSE, FALSE, strcat(tmp, line), the_args);
    va_end(the_args);
}

/*VARARGS1*/
void
verbalize(const char *line, ...)
{
    va_list the_args;
    char *tmp;

    if (!flags.soundok)
        return;
    va_start(the_args, line);
    tmp = You_buf((int)strlen(line) + sizeof "\"\"");
    strcpy(tmp, "\"");
    strcat(tmp, line);
    strcat(tmp, "\"");
    vpline(FALSE, FALSE, tmp, the_args);
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
        char pbuf[BUFSZ];

        vsprintf(pbuf, line, the_args);
        raw_print(pbuf);
    }
}


/*VARARGS1*/
void
impossible(const char *s, ...)
{
    nonfatal_dump_core();

    va_list the_args;

    va_start(the_args, s);
    if (program_state.in_impossible)
        panic("impossible called impossible");
    program_state.in_impossible = 1;
    {
        char pbuf[BUFSZ];

        vsprintf(pbuf, s, the_args);
        paniclog("impossible", pbuf);

        DEBUG_LOG_BACKTRACE("impossible() called: %s\n", pbuf);
    }
    va_end(the_args);
    va_start(the_args, s);
    vpline(FALSE, FALSE, s, the_args);
    pline("Program in disorder - perhaps you'd better save.");
    program_state.in_impossible = 0;
    va_end(the_args);
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
mstatusline(struct monst *mtmp)
{
    aligntyp alignment;
    char info[BUFSZ], monnambuf[BUFSZ];

    if (mtmp->ispriest || (mtmp->isminion && roamer_type(mtmp->data)))
        alignment = EPRI(mtmp)->shralign;
    else if (mtmp->isminion)
        alignment = EMIN(mtmp)->min_align;
    else {
        alignment = mtmp->data->maligntyp;
        alignment =
            (alignment > 0) ? A_LAWFUL : (alignment <
                                          0) ? A_CHAOTIC : A_NEUTRAL;
    }

    info[0] = 0;
    /* Identity theft */
    if (Role_if(PM_ROGUE) && carrying_questart())
        sprintf(eos(info), ", Social Security number %d", mtmp->m_id);
    if (mtmp->mtame) {
        strcat(info, ", tame");
        if (wizard) {
            sprintf(eos(info), " (%d", mtmp->mtame);
            if (!mtmp->isminion)
                sprintf(eos(info), "; hungry %u; apport %d",
                        EDOG(mtmp)->hungrytime, EDOG(mtmp)->apport);
            strcat(info, ")");
        }
    } else if (mtmp->mpeaceful)
        strcat(info, ", peaceful");
    if (mtmp->meating)
        strcat(info, ", eating");
    if (mtmp->mcan)
        strcat(info, ", cancelled");
    if (mtmp->mconf)
        strcat(info, ", confused");
    if (mtmp->mblinded || !mtmp->mcansee)
        strcat(info, ", blind");
    if (mtmp->mstun)
        strcat(info, ", stunned");
    if (mtmp->msleeping)
        strcat(info, ", asleep");
    else if (mtmp->mfrozen || !mtmp->mcanmove)
        strcat(info, ", can't move");
    /* [arbitrary reason why it isn't moving] */
    else if (mtmp->mstrategy & STRAT_WAITMASK)
        strcat(info, ", meditating");
    else if (mtmp->mflee)
        strcat(info, ", scared");
    if (mtmp->mtrapped)
        strcat(info, ", trapped");
    if (mtmp->mspeed)
        strcat(info,
               mtmp->mspeed == MFAST ? ", fast" : mtmp->mspeed ==
               MSLOW ? ", slow" : ", ???? speed");
    if (mtmp->mundetected)
        strcat(info, ", concealed");
    if (mtmp->minvis)
        strcat(info, ", invisible");
    if (mtmp == u.ustuck)
        strcat(info,
               (sticks(youmonst.data)) ? ", held by you" : Engulfed
               ? (is_animal(u.ustuck->data) ? ", swallowed you" :
                  ", engulfed you") : ", holding you");
    if (mtmp == u.usteed)
        strcat(info, ", carrying you");

    /* avoid "Status of the invisible newt ..., invisible" */
    /* and unlike a normal mon_nam, use "saddled" even if it has a name */
    strcpy(monnambuf,
           x_monnam(mtmp, ARTICLE_THE, NULL, (SUPPRESS_IT | SUPPRESS_INVISIBLE),
                    FALSE));

    pline("Status of %s (%s):  Level %d  HP %d(%d)  Def %d%s.", monnambuf,
          align_str(alignment), mtmp->m_lev, mtmp->mhp, mtmp->mhpmax,
          10 - find_mac(mtmp), info);
}

void
ustatusline(void)
{
    char info[BUFSZ];

    info[0] = '\0';
    if (Sick) {
        strcat(info, ", dying from");
        if (u.usick_type & SICK_VOMITABLE)
            strcat(info, " food poisoning");
        if (u.usick_type & SICK_NONVOMITABLE) {
            if (u.usick_type & SICK_VOMITABLE)
                strcat(info, " and");
            strcat(info, " illness");
        }
    }
    if (Stoned)
        strcat(info, ", solidifying");
    if (Slimed)
        strcat(info, ", becoming slimy");
    if (Strangled)
        strcat(info, ", being strangled");
    if (Vomiting)
        strcat(info, ", nauseated");    /* !"nauseous" */
    if (Confusion)
        strcat(info, ", confused");
    if (Blind) {
        strcat(info, ", blind");
        if (u.ucreamed) {
            if ((long)u.ucreamed < Blinded || Blindfolded ||
                !haseyes(youmonst.data))
                strcat(info, ", cover");
            strcat(info, "ed by sticky goop");
        }       /* note: "goop" == "glop"; variation is intentional */
    }
    if (Stunned)
        strcat(info, ", stunned");
    if (!u.usteed && Wounded_legs) {
        const char *what = body_part(LEG);

        if (LWounded_legs && RWounded_legs)
            what = makeplural(what);
        sprintf(eos(info), ", injured %s", what);
    }
    if (Glib)
        sprintf(eos(info), ", slippery %s", makeplural(body_part(HAND)));
    if (u.utrap)
        strcat(info, ", trapped");
    if (Fast)
        strcat(info, Very_fast ? ", very fast" : ", fast");
    if (u.uundetected)
        strcat(info, ", concealed");
    if (Invis)
        strcat(info, ", invisible");
    if (u.ustuck) {
        if (sticks(youmonst.data))
            strcat(info, ", holding ");
        else
            strcat(info, ", held by ");
        strcat(info, mon_nam(u.ustuck));
    }

    pline("Status of %s (%s%s):  Level %d  HP %d(%d)  Def %d%s.", u.uplname,
          (u.ualign.record >= 20) ? "piously " :
          (u.ualign.record > 13) ? "devoutly " :
          (u.ualign.record > 8) ? "fervently " :
          (u.ualign.record > 3) ? "stridently " :
          (u.ualign.record == 3) ? "" :
          (u.ualign.record >= 1) ? "haltingly " :
          (u.ualign.record == 0) ? "nominally " : "insufficiently ",
          align_str(u.ualign.type), Upolyd ? mons[u.umonnum].mlevel : u.ulevel,
          Upolyd ? u.mh : u.uhp, Upolyd ? u.mhmax : u.uhpmax, 10 - u.uac, info);
}

void
self_invis_message(void)
{
    pline("%s %s.",
          Hallucination ? "Far out, man!  You" : "Gee!  All of a sudden, you",
          See_invisible ? "can see right through yourself" :
          "can't see yourself");
}

/*pline.c*/
