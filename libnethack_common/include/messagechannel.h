/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Alex Smith, 2015. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MESSAGECHANNEL_H
# define MESSAGECHANNEL_H

/* "Message channel" definitions. Each message is assigned to a channel, which
   basically unifies messages that the UI might want to treat the same way.
   This allows for a MSGTYPE equivalent with considerably less configuration
   required.

   The order of the definitions is not 100% arbitrary: it groups together
   messages from a similar source, and orders the sources, and messages within
   each source, by importance (from most to least). However, it will necessarily
   be somewhat subjective, so relying on the order for anything but presenting
   lists to the user may be unwise.

   When adding entries to this, also add default rules to them in messages.c.

   Warning: this file is currently included from nethack_types.h (because it
   contains the definition of windowprocs), and thus should do nothing but
   define the one enum it's supposed to define. */

enum msg_channel {
    /* Outside the game */
    msgc_emergency,    /* error in the game engine, or similar meta-game info */
    msgc_impossible,                      /* message that should never happen */
    msgc_saveload,  /* saving, loading, replay messages, x-mode lifesave etc. */
    msgc_debug,                    /* debug messages: only show in debug mode */
    msgc_noidea,        /* in-game message but I have no idea what causes it;
                                  if you see this message, please let me know */

    /* Permanent or impending permanent changes */
    msgc_fatal,            /* player is dead or about to die (or quit/escape) */
    msgc_fatal_predone,     /* ditto, but with done() in the message sequence
                                    (separate to avoid duplicate force-Mores) */
    msgc_fatalavoid,        /* instadeath missed, or death was lifesaved from */
    msgc_petfatal,                  /* pet/ally died or is in serious trouble
                                      (separated from player, conflict, etc.) */
    msgc_npcanger,          /* major NPC (shopkeeper, quest, etc.) gets angry */
    msgc_intrloss,         /* permanent intrinsic/stat change, bad for player */
    msgc_intrloss_level,    /* ditto from level; avoids duplicate force-Mores */
    msgc_intrgain,       /* permanent intrinsic/stat change, good for player;
                                        also used for wishes, genocides, etc. */
    msgc_intrgain_level,    /* ditto from level; avoids duplicate force-Mores */
    msgc_itemloss,                /* items being destroyed / damaged / cursed */
    msgc_itemrepair,                  /* item repaired / uncursed / enchanted */

    /* Temporary conditions */
    msgc_statusbad,        /* player gains a bad temporary effect, e.g. stun,
                            or triggers a bad instant effect, e.g. aggravate,
                                or loses a good temporary effect unexpectedly */
    msgc_statusgood,       /* player gains a good or neutral temporary effect */
    msgc_statusextend,                /* player gains a redundant good effect */
    msgc_statusheal, /* a bad temporary effect, or damage, heals or times out
                             (Eyes of the Overworld instantly heal blindness) */
    msgc_statusend,  /* a good temporary effect is switched off or times out;
                         forcible rehumanization (golem instakill, explosion) */
    msgc_alignbad,     /* alignment and conduct violations, offending deities */
    msgc_aligngood,      /* message saying action was beneficial to alignment */
    msgc_alignchaos,        /* pseudo-channel for actions that violate lawful
                                  alignment by default-lawfuls, handling HoOA */

    /* Monster and dungeon actions and behaviour */
    msgc_interrupted,   /* action was cancelled midway due to a monster, etc. */
    msgc_nonmonbad,      /* non-monster source attacked player, and succeeded */
    msgc_moncombatbad,   /* monster did something combat-like, bad for player */
    msgc_nonmongood,        /* non-monster source attacked player, but missed */
    msgc_moncombatgood, /* monster did something combat-like, good for player */
    msgc_monneutral,  /* monster did/affected by something, player uninvolved */
    msgc_npcvoice,       /* monster says something (without doing something);
                                     used for "the monster says" prefixes too */

    /* Overrides for all the three previous categories */
    msgc_playerimmune,    /* player was immune to something (for fatal things
                             that can repeat, the immunity must be permanent) */
    msgc_notresisted,      /* player's resistance was pierced or didn't apply */

    /* Pet actions and behaviour */
    msgc_petkill,                                /* pet/ally killed a monster */
    msgc_petcombatbad,     /* pet/ally did something combat-like, bad for pet */
    msgc_petcombatgood,   /* pet/ally did something combat-like, good for pet */
    msgc_petneutral,    /* pet/ally did something non-combat (e.g. item drop) */
    msgc_petwarning, /* problem with pet relationship, less bad than petfatal */

    /* Response to player actions */
    msgc_substitute,    /* a different action happened instead (e.g. cursed);
                            or unexpected and bad side effects on player turn */
    msgc_badidea,              /* the action had predictable bad consequences */
    msgc_failcurse,    /* attempted action took time and accomplished nothing
                        due to spoiler information (curse, no charges, etc.);
                          the player didn't know this in advance but does now */
    msgc_combatimmune,                       /* combat action hit an immunity */
    msgc_unpaid,        /* shk charges for your action; monster demands money */
    msgc_mispaste,          /* msgc_cancelled variant for meaningless actions
                         (i.e. more likely a typo/mispaste than intentional);
                                              don't use for escaping a prompt */
    msgc_cancelled,      /* action is known impossible, and was not attempted */
    msgc_cancelled1,   /* action is known impossible but took time anyway :-( */
    msgc_combatalert,    /* player randomly activates artifact/weapon special */
    msgc_kill,                                     /* player killed a monster */
    msgc_failrandom,  /* action failed due to chance or 0% success rate, that
                           the player was already aware of (i.e. no spoiling) */
    msgc_trapescape,                   /* like failrandom, for escaping traps */
    msgc_nospoil,               /* action failed, the player doesn't know why */
    msgc_combatgood, /* player did something combat-like, it worked out (hit) */
    msgc_notarget,  /* player verifies the lack of a target / detects nothing */
    msgc_occstart,           /* description of what the player is about to do */
    msgc_consequence,            /* message about a side effect of an action,
                                       that doesn't majorly change its effect */
    msgc_noconsequence,      /* side effects were expected, but didn't happen */
    msgc_yafm,     /* the request had no real effect due to being ridiculous;
                      also used for actions that are aborted by known factors
                        (but may have accomplished useful things before that) */
    msgc_actionok,                   /* single-turn action completed normally */
    msgc_actionboring,                 /* like actionok, but less interesting */

    /* Informational about the dungeon ("voice of the DM") */
    msgc_discoverportal,             /* you find a portal or vibrating square */
    msgc_levelwarning, /* mind flayer warning, hostile monster summoned, etc. */
    msgc_intro,                          /* "Welcome to NetHack" and the like */
    msgc_outrobad,         /* flavour messages during endgame, bad for player */
    msgc_outrogood,       /* flavour messages during endgame, good for player */
    msgc_info,      /* output from informational commands, level names, etc.;
                                 also messages that exist to identify objects */
    msgc_youdiscover,                /* find a trap, discover a monster, etc. */
    msgc_levelsound,                /* hint about what's on the current level */
    msgc_branchchange,       /* message about changing branch / special level */

    /* Messages that are not useful to experienced players */
    msgc_controlhelp,                          /* "use the X command to do Y" */
    msgc_rumor,                                     /* a true or false rumour */
    msgc_hint,     /* message about a mistake or game rule (e.g. invocation);
                         verbose text for what a command does / message means */

    /* UI internals */
    msgc_uiprompt,        /* "messages" that inform you of how a prompt works */
    msgc_curprompt,       /* a currently active prompt (used by UI code only) */
    msgc_reminder,          /* "message" that shows the selection from a menu */

    /* Messages that are hidden inside pline() */
    msgc_offlevel,        /* something happened much too far away to perceive */
    msgc_mute,              /* always hidden, used to reduce code duplication */
};

/*
 * Channels that need special handling:
 * - msgc_emergency should be converted to a raw_print
 * - msgc_impossible causes an impossible(); the distinction from
 *   msgc_emergency is that it's an in-game message, just one that shouldn't
 *   appear in this context; and it's used much like msgc_mute in order to
 *   pass to a function that shouldn't be producing messages; just impossible()
 *   instead if it's convenient to do so.
 * - msgc_noidea should come with instructions to contact the developers (in
 *   beta versions, it outright puts up a dialog box; in release versions it
 *   puts an entry in the paniclog)
 * - msgc_debug is hidden by pline() outside debug mode
 * - msgc_alignchaos is converted to msgc_alignbad for lawful players
 *   (otherwise it's left as-is and will probably be given a neutral color)
 * - msgc_mispaste can lock the program if a high density is seen
 * - msgc_reminder has special --More-- handling (i.e. it doesn't)
 * - msgc_controlhelp, msgc_actionboring, msgc_trapescape are used for things
 *   that are suppressed by flags.verbose in 3.4.3
 * - msgc_offlevel messages are simply discarded in pline(), with an
 *   impossible() for the time being because we don't have multiplayer code yet
 * - msgc_mute messages are discarded in pline() full stop (this case is used
 *   for functions that conditionally produce messages; we code it as
 *   unconditionally producing the message and conditionalizing the channel)
 */

#endif
