/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright (c) Mike Stephenson 1991.                            */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef QUEST_H
# define QUEST_H

struct q_score {        /* Quest "scorecard" */
    unsigned first_start:1;     /* only set the first time */
    unsigned met_leader:1;      /* has met the leader */
    unsigned not_ready:3;       /* rejected due to alignment, etc. */
    unsigned pissed_off:1;      /* got the leader angry */
    unsigned got_quest:1;       /* got the quest assignment */

    unsigned first_locate:1;    /* only set the first time */
    unsigned met_intermed:1;    /* used if the locate is a person. */
    unsigned got_final:1;       /* got the final quest assignment */

    unsigned made_goal:3;       /* # of times on goal level */
    unsigned met_nemesis:1;     /* has met the nemesis before */
    unsigned killed_nemesis:1;  /* set when the nemesis is killed */
    unsigned in_battle:1;       /* set when nemesis fighting you */

    unsigned cheater:1; /* set if cheating detected */
    unsigned touched_artifact:1;        /* for a special message */
    unsigned offered_artifact:1;        /* offered to leader */
    unsigned got_thanks:1;      /* final message from leader */

    /* keep track of leader presence/absence even if leader is polymorphed,
       raised from dead, etc */
    unsigned leader_is_dead:1;
    unsigned leader_m_id;
};

# define MAX_QUEST_TRIES  7     /* exceed this and you "fail" */

/* at least this align.record to start */
# define MIN_QUEST_ALIGN 20
  /* note: align 20 matches "pious" as reported by enlightenment (cmd.c) */
# define MIN_QUEST_LEVEL 14     /* at least this u.ulevel to start */
  /* note: exp.lev. 14 is threshold level for 5th rank (class title, role.c) */

#endif /* QUEST_H */

