/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-03-23 */
/* Copyright (c) 2015 Alex Smith. */
/* NetHack may be freely redistributed.  See license for details. */

#ifdef AIMAKE_BUILDOS_MSWin32
# error !AIMAKE_FAIL_SILENTLY! Testing on Windows is not yet supported.
#endif

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
    "adjust", "apply", "chat", "dip", "drop", "eat", "engrave", "farlook",
    "invoke", "kick", "namemon", "nameitem", "quiver", "read", "ride", "rub",
    "takeoff", "throw"
};

const int unused_objects[] = UNUSEDOBJECTS;

/* The idea behind the round-robin test is that, for each <item, monster,
   command> triple, we wish up the item, summon the monster, then use the
   command (specifying the item and the monster's position as arguments, if
   necessary). */
static void
round_robin_test(unsigned long long seed, unsigned long long skip,
                 unsigned long long limit, bool verbose)
{
    const int unuseditemcount = sizeof unused_objects / sizeof *unused_objects;

    /* We currently use hardcoded values for the first "abnormal" monster/item,
       so that we don't need full access to the game logic. */
    const int moncount = PM_LONG_WORM_TAIL - 0;
    const int itemcount = BLINDING_VENOM - 1 - unuseditemcount;
    const int cmdcount = sizeof testable_commands / sizeof *testable_commands;
    if (limit > moncount * itemcount * cmdcount - skip)
        limit = moncount * itemcount * cmdcount - skip;

    init_test_system(seed, "wgfn", skip + limit);

    /* We want to test all (cmd, mon, item) triples, but in an order that cycles
       through each individual command/monster/item as quickly as possible, and
       each pair of commands/monsters/items as quickly as possible. We do this
       by taking the next prime after *count, then taking the value of an
       increasing integer modulo those factors, skipping any entries which are
       out of range. */

    int monfactor = nextprime(moncount - 1);
    int itemfactor = nextprime(itemcount + unuseditemcount - 1);
    int cmdfactor = nextprime(cmdcount - 1);

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

        boolean skipping = false;
        if (skip) {
            --skip;
            skipping = true;
        } else if (limit)
            --limit;
        else
            break;

        char teststring[512];
        snprintf(teststring, sizeof teststring,
                 "levelteleport,\"%d\",genesis,\"monsndx #%d\","
                 "wish,\"Z - otyp #%d\",%s,wear,wield,fight,fight,cast,zap,"
                 "read,drink,fight,fight,wait,wait,%s,wait,wait,wait",
                 (int)((seed + nt) % 50), mon, item + 1,
                 testable_commands[cmd], testable_commands[cmd]);
        (skipping ? skip_test_game : play_test_game)(teststring, verbose);

    continue_main_loop:;
    }

    shutdown_test_system();
}

int
main(int argc, char **argv)
{
    unsigned long long seed = time(NULL);
    unsigned long long limit = -(1ULL);
    unsigned long long skip = 0;
    char *endptr;

    while (argc > 1) {
        if (argc == 2) {
            fprintf(stderr, "Usage:\n"
                    "  testmain [options]\n\n"
                    "Outputs a sequence of test results in TAP format.\n"
                    "Use a TAP parser like prove(1) to parse them.\n\n"
                    "Options:\n"
                    "  --seed seed\n"
                    "    Produce deterministic results; specifying the same\n"
                    "    seed will cause the same tests to be reproduced.\n\n"
                    "  --limit count\n"
                    "    Run only the given number of tests, then skip the\n"
                    "    remaining tests. If the limit is less than 10,\n"
                    "    produce verbose comments in the output.\n\n"
                    "  --skip count\n"
                    "    Skip the given number of tests at the start of the\n"
                    "    testsuite.\n\n"
                    "  --stdoutbuffer count\n"
                    "    Adjust the size of the buffer used on stdout (0 =\n"
                    "    use line buffering for stdout)\n");
            return (strcmp(argv[1], "--help") ? EXIT_FAILURE : 0);
        }

        unsigned long long parsevalue = strtoull(argv[2], &endptr, 10);
        if (!*argv[2] || *endptr) {
            fprintf(stderr, "Option value '%s' is not an integer\n", argv[2]);
            return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "--seed") == 0)
            seed = parsevalue;
        else if (strcmp(argv[1], "--limit") == 0)
            limit = parsevalue;
        else if (strcmp(argv[1], "--skip") == 0)
            skip = parsevalue;
        else if (strcmp(argv[1], "--stdoutbuffer") == 0)
            setvbuf(stdout, NULL, parsevalue ? _IOFBF : _IOLBF, parsevalue);
        else {
            fprintf(stderr, "Unknown option '%s'\n", argv[1]);
            return EXIT_FAILURE;
        }

        argv += 2;
        argc -= 2;
    }

    round_robin_test(seed, skip, limit, limit < 10);
    return 0;
}
