/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-21 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#include "testgame.h"
#include "pm.h"
#include "onames.h"
#include "hacklib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Commands that take an item and/or monster as argument, are usable by a level
   1 wizard, and don't have specific requirements on the adjacent terrain or on
   items on the floor. */
const char *const testable_commands[] = {
    "adjust", "apply", "cast", "chat", "dip", "drink", "drop", "eat", "engrave",
    "farlook", "fight", "fire", "invoke", "kick", "name", "namemon", "nameitem",
    "nametype", "quiver", "read", "ride", "rub", "takeoff", "throw", "wear",
    "wield", "zap"
};

const int unused_objects[] = UNUSEDOBJECTS;

/* The idea behind the round-robin test is that, for each <item, monster,
   command> triple, we wish up the item, summon the monster, then use the
   command (specifying the item and the monster's position as arguments, if
   necessary). */
static void
round_robin_test(unsigned long long seed)
{
    const int unuseditemcount = sizeof unused_objects / sizeof *unused_objects;

    /* We currently use hardcoded values for the first "abnormal" monster/item,
       so that we don't need full access to the game logic. */
    const int moncount = PM_LONG_WORM_TAIL - 0;
    const int itemcount = BLINDING_VENOM - 1 - unuseditemcount;
    const int cmdcount = sizeof testable_commands / sizeof *testable_commands;
    init_test_system(seed, "wgfn", moncount * itemcount * cmdcount);

    /* We want to test all (cmd, mon, item) triples, but in an order that cycles
       through each individual command/monster/item as quickly as possible, and
       each pair of commands/monsters/items as quickly as possible. We do this
       by taking the next prime after *count, then taking the value of an
       increasing integer modulo those factors, skipping any entries which are
       out of range. */

    int monfactor = nextprime(moncount);
    int itemfactor = nextprime(itemcount + unuseditemcount);
    int cmdfactor = nextprime(cmdcount);

    long long nt;

    for (nt = 0; nt < monfactor * itemfactor * cmdfactor; nt++) {
        int cmd, mon, item;
        cmd = nt % cmdfactor;
        if (cmd >= cmdcount)
            continue;
        mon = nt % monfactor;
        if (mon >= moncount)
            continue;
        item = nt % itemfactor;
        if (item >= (itemcount + unuseditemcount))
            continue;
        int j;
        for (j = 0; j < unuseditemcount; j++)
            if (unused_objects[j] == item + 1)
                goto continue_main_loop;

        char teststring[512];
        snprintf(teststring, sizeof teststring,
                 "genesis,\"monsndx #%d\",wish,\"Z - otyp #%d\",%s,"
                 "wait,wait,wait,wait,wait", mon, item + 1,
                 testable_commands[cmd]);
        play_test_game(teststring);

    continue_main_loop:;
    }

    shutdown_test_system();
}

int
main(int argc, char **argv)
{
    round_robin_test(1);
    return 0;
}
