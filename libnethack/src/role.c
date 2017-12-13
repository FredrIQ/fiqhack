/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-12-13 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985-1999. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static boolean ok_race(int, int, int, int);
static boolean ok_gend(int, int, int, int);
static boolean ok_align(int, int, int, int);


/*** Table of all roles ***/
/* According to AD&D, HD for some classes (ex. Wizard) should be smaller
 * (4-sided for wizards).  But this is not AD&D, and using the AD&D
 * rule here produces an unplayable character.  Thus I have used a minimum
 * of an 10-sided hit die for everything.  Another AD&D change: wizards get
 * a minimum strength of 4 since without one you can't teleport or cast
 * spells. --KAA
 *
 * As the wizard has been updated (wizard patch 5 jun '96) their HD can be
 * brought closer into line with AD&D. This forces wizards to use magic more
 * and distance themselves from their attackers. --LSZ
 *
 * With the introduction of races, some hit points and energy
 * has been reallocated for each race.  The values assigned
 * to the roles has been reduced by the amount allocated to
 * humans.  --KMH
 *
 * God names use a leading underscore to flag goddesses.
 */
const struct Role roles[] = {
    {{"Archeologist", 0}, {
                           {"Digger", 0},
                           {"Field Worker", 0},
                           {"Investigator", 0},
                           {"Exhumer", 0},
                           {"Excavator", 0},
                           {"Spelunker", 0},
                           {"Speleologist", 0},
                           {"Collector", 0},
                           {"Curator", 0}},
     "Quetzalcoatl", "Camaxtli", "Huhetotl",    /* Central American */
     "Arc", "the College of Archeology", "the Tomb of the Toltec Kings",
     PM_ARCHEOLOGIST, NON_PM,
     PM_LORD_CARNARVON, PM_STUDENT, PM_MINION_OF_HUHETOTL,
     NON_PM, PM_HUMAN_MUMMY, S_SNAKE, S_MUMMY,
     ART_ORB_OF_DETECTION,
     MH_HUMAN | MH_DWARF | MH_GNOME | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL |
     ROLE_NEUTRAL,
     /* Str Int Wis Dex Con Cha */
     {7, 10, 10, 7, 7, 7},
     {20, 20, 20, 10, 20, 10},
     /* Init Lower Higher */
     {11, 0, 0, 8, 1, 0},       /* Hit points */
     {1, 0, 0, 1, 0, 1}, 14,    /* Energy */
     10, 5, 0, 2, 10, A_INT, SPE_MAGIC_MAPPING, -4},
    {{"Barbarian", 0}, {
                        {"Plunderer", "Plunderess"},
                        {"Pillager", 0},
                        {"Bandit", 0},
                        {"Brigand", 0},
                        {"Raider", 0},
                        {"Reaver", 0},
                        {"Slayer", 0},
                        {"Chieftain", "Chieftainess"},
                        {"Conqueror", "Conqueress"}},
     "Mitra", "Crom", "Set",    /* Hyborian */
     "Bar", "the Camp of the Duali Tribe", "the Duali Oasis",
     PM_BARBARIAN, NON_PM,
     PM_PELIAS, PM_CHIEFTAIN, PM_THOTH_AMON,
     PM_OGRE, PM_TROLL, S_OGRE, S_TROLL,
     ART_HEART_OF_AHRIMAN,
     MH_HUMAN | MH_ORC | ROLE_MALE | ROLE_FEMALE | ROLE_NEUTRAL | ROLE_CHAOTIC,
     /* Str Int Wis Dex Con Cha */
     {16, 7, 7, 15, 16, 6},
     {30, 6, 7, 20, 30, 7},
     /* Init Lower Higher */
     {14, 0, 0, 10, 2, 0},      /* Hit points */
     {1, 0, 0, 1, 0, 1}, 10,    /* Energy */
     10, 14, 0, 0, 8, A_INT, SPE_SPEED_MONSTER, -4},
    {{"Caveman", "Cavewoman"}, {
                                {"Troglodyte", 0},
                                {"Aborigine", 0},
                                {"Wanderer", 0},
                                {"Vagrant", 0},
                                {"Wayfarer", 0},
                                {"Roamer", 0},
                                {"Nomad", 0},
                                {"Rover", 0},
                                {"Pioneer", 0}},
     "Anu", "_Ishtar", "Anshar",        /* Babylonian */
     "Cav", "the Caves of the Ancestors", "the Dragon's Lair",
     PM_CAVEMAN, PM_LITTLE_DOG,
     PM_SHAMAN_KARNOV, PM_NEANDERTHAL, PM_CHROMATIC_DRAGON,
     PM_BUGBEAR, PM_HILL_GIANT, S_HUMANOID, S_GIANT,
     ART_SCEPTRE_OF_MIGHT,
     MH_HUMAN | MH_DWARF | MH_GNOME | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL |
     ROLE_NEUTRAL,
     /* Str Int Wis Dex Con Cha */
     {10, 7, 7, 7, 8, 6},
     {30, 6, 7, 20, 30, 7},
     /* Init Lower Higher */
     {14, 0, 0, 8, 2, 0},       /* Hit points */
     {1, 0, 0, 1, 0, 1}, 10,    /* Energy */
     10, 12, 0, 1, 8, A_INT, SPE_DIG, -4},
    {{"Healer", 0}, {
                     {"Rhizotomist", 0},
                     {"Empiric", 0},
                     {"Embalmer", 0},
                     {"Dresser", 0},
                     {"Medicus ossium", "Medica ossium"},
                     {"Herbalist", 0},
                     {"Magister", "Magistra"},
                     {"Physician", 0},
                     {"Chirurgeon", 0}},
     "_Athena", "Hermes", "Poseidon",   /* Greek */
     "Hea", "the Temple of Epidaurus", "the Temple of Coeus",
     PM_HEALER, NON_PM,
     PM_HIPPOCRATES, PM_ATTENDANT, PM_CYCLOPS,
     PM_GIANT_RAT, PM_SNAKE, S_RODENT, S_YETI,
     ART_STAFF_OF_AESCULAPIUS,
     MH_HUMAN | MH_GNOME | ROLE_MALE | ROLE_FEMALE | ROLE_NEUTRAL,
     /* Str Int Wis Dex Con Cha */
     {7, 7, 13, 7, 11, 16},
     {15, 20, 20, 15, 25, 5},
     /* Init Lower Higher */
     {11, 0, 0, 8, 1, 0},       /* Hit points */
     {1, 4, 0, 1, 0, 2}, 20,    /* Energy */
     10, 3, -3, 2, 10, A_WIS, SPE_CURE_SICKNESS, -4},
    {{"Knight", 0}, {
                     {"Gallant", 0},
                     {"Esquire", 0},
                     {"Bachelor", 0},
                     {"Sergeant", 0},
                     {"Knight", 0},
                     {"Banneret", 0},
                     {"Chevalier", "Chevaliere"},
                     {"Seignieur", "Dame"},
                     {"Paladin", 0}},
     "Lugh", "_Brigit", "Manannan Mac Lir",     /* Celtic */
     "Kni", "Camelot Castle", "the Isle of Glass",
     PM_KNIGHT, PM_PONY,
     PM_KING_ARTHUR, PM_PAGE, PM_IXOTH,
     PM_QUASIT, PM_OCHRE_JELLY, S_IMP, S_JELLY,
     ART_MAGIC_MIRROR_OF_MERLIN,
     MH_HUMAN | MH_DWARF | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL,
     /* Str Int Wis Dex Con Cha */
     {13, 7, 14, 8, 10, 17},
     {30, 15, 15, 10, 20, 10},
     /* Init Lower Higher */
     {14, 0, 0, 8, 2, 0},       /* Hit points */
     {1, 4, 0, 1, 0, 2}, 10,    /* Energy */
     10, 8, -2, 0, 9, A_WIS, SPE_TURN_UNDEAD, -4},
    {{"Monk", 0}, {
                   {"Candidate", 0},
                   {"Novice", 0},
                   {"Initiate", 0},
                   {"Student of Stones", 0},
                   {"Student of Waters", 0},
                   {"Student of Metals", 0},
                   {"Student of Winds", 0},
                   {"Student of Fire", 0},
                   {"Master", 0}},
     "Shan Lai Ching", "Chih Sung-tzu", "Huan Ti",      /* Chinese */
     "Mon", "the Monastery of Chan-Sune",
     "the Monastery of the Earth-Lord",
     PM_MONK, NON_PM,
     PM_GRAND_MASTER, PM_ABBOT, PM_MASTER_KAEN,
     PM_EARTH_ELEMENTAL, PM_XORN, S_ELEMENTAL, S_XORN,
     ART_EYES_OF_THE_OVERWORLD,
     MH_HUMAN | MH_ELF | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL | ROLE_NEUTRAL |
     ROLE_CHAOTIC,
     /* Str Int Wis Dex Con Cha */
     {10, 7, 8, 8, 7, 7},
     {25, 10, 20, 20, 15, 10},
     /* Init Lower Higher */
     {12, 0, 0, 8, 1, 0},       /* Hit points */
     {2, 2, 0, 2, 0, 2}, 10,    /* Energy */
     10, 8, -2, 2, 20, A_WIS, SPE_RESTORE_ABILITY, -4},
    {{"Priest", "Priestess"}, {
                               {"Aspirant", 0},
                               {"Acolyte", 0},
                               {"Adept", 0},
                               {"Priest", "Priestess"},
                               {"Curate", 0},
                               {"Canon", "Canoness"},
                               {"Lama", 0},
                               {"Patriarch", "Matriarch"},
                               {"High Priest", "High Priestess"}},
     0, 0, 0,   /* chosen randomly from among the other roles */
     "Pri", "the Great Temple", "the Temple of Nalzok",
     PM_PRIEST, NON_PM,
     PM_ARCH_PRIEST, PM_ACOLYTE, PM_NALZOK,
     PM_HUMAN_ZOMBIE, PM_WRAITH, S_ZOMBIE, S_WRAITH,
     ART_MITRE_OF_HOLINESS,
     MH_HUMAN | MH_ELF | MH_ORC | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL | ROLE_NEUTRAL |
     ROLE_CHAOTIC,
     /* Str Int Wis Dex Con Cha */
     {7, 7, 10, 7, 7, 7},
     {15, 10, 30, 15, 20, 10},
     /* Init Lower Higher */
     {12, 0, 0, 8, 1, 0},       /* Hit points */
     {4, 3, 0, 2, 0, 2}, 10,    /* Energy */
     10, 3, -2, 2, 10, A_WIS, SPE_REMOVE_CURSE, -4},
    /* Note: Rogue precedes Ranger so that use of `-R' on the command line
       retains its traditional meaning. */
    {{"Rogue", 0}, {
                    {"Footpad", 0},
                    {"Cutpurse", 0},
                    {"Rogue", 0},
                    {"Pilferer", 0},
                    {"Robber", 0},
                    {"Burglar", 0},
                    {"Filcher", 0},
                    {"Magsman", "Magswoman"},
                    {"Thief", 0}},
     "Issek", "Mog", "Kos",     /* Nehwon */
     "Rog", "the Thieves' Guild Hall", "the Assassins' Guild Hall",
     PM_ROGUE, NON_PM,
     PM_MASTER_OF_THIEVES, PM_THUG, PM_MASTER_ASSASSIN,
     PM_LEPRECHAUN, PM_GUARDIAN_NAGA, S_NYMPH, S_NAGA,
     ART_MASTER_KEY_OF_THIEVERY,
     MH_HUMAN | MH_ORC | ROLE_MALE | ROLE_FEMALE | ROLE_CHAOTIC,
     /* Str Int Wis Dex Con Cha */
     {7, 7, 7, 10, 7, 6},
     {20, 10, 10, 30, 20, 10},
     /* Init Lower Higher */
     {10, 0, 0, 8, 1, 0},       /* Hit points */
     {1, 0, 0, 1, 0, 1}, 11,    /* Energy */
     10, 8, 0, 1, 9, A_INT, SPE_DETECT_TREASURE, -4},
    {{"Ranger", 0}, {
                     {"Tenderfoot", 0},
                     {"Lookout", 0},
                     {"Trailblazer", 0},
                     {"Reconnoiterer", "Reconnoiteress"},
                     {"Scout", 0},
                     {"Arbalester", 0}, /* One skilled at crossbows */
                     {"Archer", 0},
                     {"Sharpshooter", 0},
                     {"Marksman", "Markswoman"}},
     "Mercury", "_Venus", "Mars",       /* Roman/planets */
     "Ran", "Orion's camp", "the cave of the wumpus",
     PM_RANGER, PM_LITTLE_DOG /* Orion & canis major */ ,
     PM_ORION, PM_HUNTER, PM_SCORPIUS,
     PM_FOREST_CENTAUR, PM_SCORPION, S_CENTAUR, S_SPIDER,
     ART_LONGBOW_OF_DIANA,
     MH_HUMAN | MH_ELF | MH_GNOME | MH_ORC | ROLE_MALE | ROLE_FEMALE |
     ROLE_NEUTRAL | ROLE_CHAOTIC,
     /* Str Int Wis Dex Con Cha */
     {13, 13, 13, 9, 13, 7},
     {30, 10, 10, 20, 20, 10},
     /* Init Lower Higher */
     {13, 0, 0, 6, 1, 0},       /* Hit points */
     {1, 0, 0, 1, 0, 1}, 12,    /* Energy */
     10, 9, 2, 1, 10, A_INT, SPE_INVISIBILITY, -4},
    {{"Samurai", 0}, {
                      {"Hatamoto", 0},  /* Banner Knight */
                      {"Ronin", 0},     /* no allegiance */
                      {"Ninja", "Kunoichi"},    /* secret society */
                      {"Joshu", 0},     /* heads a castle */
                      {"Ryoshu", 0},    /* has a territory */
                      {"Kokushu", 0},   /* heads a province */
                      {"Daimyo", 0},    /* a samurai lord */
                      {"Kuge", 0},      /* Noble of the Court */
                      {"Shogun", 0}},   /* supreme commander, warlord */
     "_Amaterasu Omikami", "Raijin", "Susanowo",        /* Japanese */
     "Sam", "the Castle of the Taro Clan", "the Shogun's Castle",
     PM_SAMURAI, PM_LITTLE_DOG,
     PM_LORD_SATO, PM_ROSHI, PM_ASHIKAGA_TAKAUJI,
     PM_WOLF, PM_STALKER, S_DOG, S_ELEMENTAL,
     ART_TSURUGI_OF_MURAMASA,
     MH_HUMAN | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL,
     /* Str Int Wis Dex Con Cha */
     {10, 8, 7, 10, 17, 6},
     {30, 10, 8, 30, 14, 8},
     /* Init Lower Higher */
     {13, 0, 0, 8, 1, 0},       /* Hit points */
     {1, 0, 0, 1, 0, 1}, 11,    /* Energy */
     10, 10, 0, 0, 8, A_INT, SPE_CLAIRVOYANCE, -4},
    {{"Tourist", 0}, {
                      {"Rambler", 0},
                      {"Sightseer", 0},
                      {"Excursionist", 0},
                      {"Peregrinator", "Peregrinatrix"},
                      {"Traveler", 0},
                      {"Journeyer", 0},
                      {"Voyager", 0},
                      {"Explorer", 0},
                      {"Adventurer", 0}},
     "Blind Io", "_The Lady", "Offler", /* Discworld */
     "Tou", "Ankh-Morpork", "the Thieves' Guild Hall",
     PM_TOURIST, NON_PM,
     PM_TWOFLOWER, PM_GUIDE, PM_MASTER_OF_THIEVES,
     PM_GIANT_SPIDER, PM_FOREST_CENTAUR, S_SPIDER, S_CENTAUR,
     ART_YENDORIAN_EXPRESS_CARD,
     MH_HUMAN | MH_GNOME | ROLE_MALE | ROLE_FEMALE | ROLE_NEUTRAL,
     /* Str Int Wis Dex Con Cha */
     {7, 10, 6, 7, 7, 10},
     {15, 10, 10, 15, 30, 20},
     /* Init Lower Higher */
     {8, 0, 0, 8, 0, 0},        /* Hit points */
     {1, 0, 0, 1, 0, 1}, 14,    /* Energy */
     10, 5, 1, 2, 10, A_INT, SPE_CHARM_MONSTER, -4},
    {{"Valkyrie", 0}, {
                       {"Stripling", 0},
                       {"Skirmisher", 0},
                       {"Fighter", 0},
                       {"Man-at-arms", "Woman-at-arms"},
                       {"Warrior", 0},
                       {"Swashbuckler", 0},
                       {"Hero", "Heroine"},
                       {"Champion", 0},
                       {"Lord", "Lady"}},
     "Tyr", "Odin", "Loki",     /* Norse */
     "Val", "the Shrine of Destiny", "the cave of Surtur",
     PM_VALKYRIE, NON_PM /* PM_WINTER_WOLF_CUB */ ,
     PM_NORN, PM_WARRIOR, PM_LORD_SURTUR,
     PM_FIRE_ANT, PM_FIRE_GIANT, S_ANT, S_GIANT,
     ART_ORB_OF_FATE,
     MH_HUMAN | MH_DWARF | ROLE_FEMALE | ROLE_LAWFUL | ROLE_NEUTRAL,
     /* Str Int Wis Dex Con Cha */
     {10, 7, 7, 7, 10, 7},
     {30, 6, 7, 20, 30, 7},
     /* Init Lower Higher */
     {14, 0, 0, 8, 2, 0},       /* Hit points */
     {1, 0, 0, 1, 0, 1}, 10,    /* Energy */
     10, 10, -2, 0, 9, A_WIS, SPE_CONE_OF_COLD, -4},
    {{"Wizard", 0}, {
                     {"Evoker", 0},
                     {"Conjurer", 0},
                     {"Thaumaturge", 0},
                     {"Magician", 0},
                     {"Enchanter", "Enchantress"},
                     {"Sorcerer", "Sorceress"},
                     {"Necromancer", 0},
                     {"Wizard", 0},
                     {"Mage", 0}},
     "Ptah", "Thoth", "Anhur",  /* Egyptian */
     "Wiz", "the Lonely Tower", "the Tower of Darkness",
     PM_WIZARD, PM_KITTEN,
     PM_NEFERET_THE_GREEN, PM_APPRENTICE, PM_DARK_ONE,
     PM_VAMPIRE_BAT, PM_XORN, S_BAT, S_WRAITH,
     ART_EYE_OF_THE_AETHIOPICA,
     MH_HUMAN | MH_ELF | MH_GNOME | MH_ORC | ROLE_MALE | ROLE_FEMALE |
     ROLE_NEUTRAL | ROLE_CHAOTIC,
     /* Str Int Wis Dex Con Cha */
     {7, 10, 7, 7, 7, 7},
     {10, 30, 10, 20, 20, 10},
     /* Init Lower Higher */
     {10, 0, 0, 8, 1, 0},       /* Hit points */
     {4, 3, 0, 2, 0, 3}, 12,    /* Energy */
     0, 1, 0, 3, 10, A_INT, SPE_MAGIC_MISSILE, -4},
/* Array terminator */
    {{0, 0}}
};


/* The player's role, created at runtime from initial
 * choices.  This will be munged in role_init().
 */
struct Role urole;



/* Table of all races */
const struct Race races[] = {
    {"human", "human", "humanity", "Hum",
     {"man", "woman"},
     PM_HUMAN, PM_HUMAN_MUMMY, PM_HUMAN_ZOMBIE,
     MH_HUMAN | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL | ROLE_NEUTRAL |
     ROLE_CHAOTIC,
     MH_HUMAN, 0, MH_GNOME | MH_ORC,
     /* Str Int Wis Dex Con Cha */
     {3, 3, 3, 3, 3, 3},
     {21, 18, 18, 18, 18, 18},
     /* Init Lower Higher */
     {2, 0, 0, 2, 1, 0},        /* Hit points */
     {1, 0, 2, 0, 2, 0} /* Energy */
     },
    {"elf", "elven", "elvenkind", "Elf",
     {0, 0},
     PM_ELF, PM_ELF_MUMMY, PM_ELF_ZOMBIE,
     MH_ELF | ROLE_MALE | ROLE_FEMALE | ROLE_CHAOTIC,
     MH_ELF, MH_ELF, MH_ORC,
     /* Str Int Wis Dex Con Cha */
     {3, 3, 3, 3, 3, 3},
     {18, 20, 20, 18, 16, 18},
     /* Init Lower Higher */
     {1, 0, 0, 1, 1, 0},        /* Hit points */
     {2, 0, 3, 0, 3, 0} /* Energy */
     },
    {"dwarf", "dwarven", "dwarvenkind", "Dwa",
     {0, 0},
     PM_DWARF, PM_DWARF_MUMMY, PM_DWARF_ZOMBIE,
     MH_DWARF | ROLE_MALE | ROLE_FEMALE | ROLE_LAWFUL,
     MH_DWARF, MH_DWARF | MH_GNOME, MH_ORC,
     /* Str Int Wis Dex Con Cha */
     {3, 3, 3, 3, 3, 3},
     {21, 16, 16, 20, 20, 16},
     /* Init Lower Higher */
     {4, 0, 0, 3, 2, 0},        /* Hit points */
     {0, 0, 0, 0, 0, 0} /* Energy */
     },
    {"gnome", "gnomish", "gnomehood", "Gno",
     {0, 0},
     PM_GNOME, PM_GNOME_MUMMY, PM_GNOME_ZOMBIE,
     MH_GNOME | ROLE_MALE | ROLE_FEMALE | ROLE_NEUTRAL,
     MH_GNOME, MH_DWARF | MH_GNOME, MH_HUMAN,
     /* Str Int Wis Dex Con Cha */
     {3, 3, 3, 3, 3, 3},
     {19, 19, 18, 18, 18, 18},
     /* Init Lower Higher */
     {1, 0, 0, 1, 0, 0},        /* Hit points */
     {2, 0, 2, 0, 2, 0} /* Energy */
     },
    {"orc", "orcish", "orcdom", "Orc",
     {0, 0},
     PM_ORC, PM_ORC_MUMMY, PM_ORC_ZOMBIE,
     MH_ORC | ROLE_MALE | ROLE_FEMALE | ROLE_CHAOTIC,
     MH_ORC, 0, MH_HUMAN | MH_ELF | MH_DWARF,
     /* Str Int Wis Dex Con Cha */
     {3, 3, 3, 3, 3, 3},
     {20, 16, 16, 18, 18, 16},
     /* Init Lower Higher */
     {1, 0, 0, 1, 0, 0},        /* Hit points */
     {1, 0, 1, 0, 1, 0} /* Energy */
     },
/* Array terminator */
    {0, 0, 0, 0}
};


/* The player's race, created at runtime from initial
 * choices.  This will be munged in role_init().
 */
struct Race urace;


/* Table of all genders */
const struct Gender genders[] = {
    {"male", "he", "him", "his", "Mal", ROLE_MALE},
    {"female", "she", "her", "her", "Fem", ROLE_FEMALE},
    {"neuter", "it", "it", "its", "Ntr", ROLE_NEUTER}
};


/* Table of all alignments */
const struct Align aligns[] = {
    {"law", "lawful", "Law", ROLE_LAWFUL, A_LAWFUL},
    {"balance", "neutral", "Neu", ROLE_NEUTRAL, A_NEUTRAL},
    {"chaos", "chaotic", "Cha", ROLE_CHAOTIC, A_CHAOTIC},
    {"evil", "unaligned", "Una", 0, A_NONE}
};

static char *promptsep(char *, int);
static int role_gendercount(int);
static int race_alignmentcount(int);

/* used by nh_str2XXX() */
static const char randomstr[] = "random";


boolean
validrole(int rolenum)
{
    return rolenum >= 0 && rolenum < SIZE(roles) - 1;
}


int
randrole(enum rng rng)
{
    return rn2_on_rng(SIZE(roles) - 1, rng);
}

short
role_quest_artifact(int pm)
{
    int i;
    for (i = 0; roles[i].name.m; i++) {
        if (roles[i].num == pm)
            return roles[i].questarti;
    }
    return 0;
}

int
str2role(char *str)
{
    int i, len;

    /* Is str valid? */
    if (!str || !str[0])
        return ROLE_NONE;

    /* Match as much of str as is provided */
    len = strlen(str);
    for (i = 0; roles[i].name.m; i++) {
        /* Does it match the male name? */
        if (!strncmpi(str, roles[i].name.m, len))
            return i;
        /* Or the female name? */
        if (roles[i].name.f && !strncmpi(str, roles[i].name.f, len))
            return i;
        /* Or the filecode? */
        if (!strcmpi(str, roles[i].filecode))
            return i;
    }

    if ((len == 1 && (*str == '*' || *str == '@')) ||
        !strncmpi(str, randomstr, len))
        return ROLE_RANDOM;

    /* Couldn't find anything appropriate */
    return ROLE_NONE;
}


boolean
validrace(int rolenum, int racenum)
{
    /* Assumes nh_validrole */
    return (racenum >= 0 && racenum < SIZE(races) - 1 &&
            (roles[rolenum].allow & races[racenum].allow & ROLE_RACEMASK));
}


int
str2race(char *str)
{
    int i, len;

    /* Is str valid? */
    if (!str || !str[0])
        return ROLE_NONE;

    /* Match as much of str as is provided */
    len = strlen(str);
    for (i = 0; races[i].noun; i++) {
        /* Does it match the noun? */
        if (!strncmpi(str, races[i].noun, len))
            return i;
        /* Or the filecode? */
        if (!strcmpi(str, races[i].filecode))
            return i;
    }

    if ((len == 1 && (*str == '*' || *str == '@')) ||
        !strncmpi(str, randomstr, len))
        return ROLE_RANDOM;

    /* Couldn't find anything appropriate */
    return ROLE_NONE;
}


boolean
validgend(int rolenum, int racenum, int gendnum)
{
    /* Assumes nh_validrole and nh_validrace */
    return (gendnum >= 0 && gendnum < ROLE_GENDERS &&
            (roles[rolenum].allow & races[racenum].allow &
             genders[gendnum].allow & ROLE_GENDMASK));
}


int
str2gend(char *str)
{
    int i, len;

    /* Is str valid? */
    if (!str || !str[0])
        return ROLE_NONE;

    /* Match as much of str as is provided */
    len = strlen(str);
    for (i = 0; i < ROLE_GENDERS; i++) {
        /* Does it match the adjective? */
        if (!strncmpi(str, genders[i].adj, len))
            return i;
        /* Or the filecode? */
        if (!strcmpi(str, genders[i].filecode))
            return i;
    }
    if ((len == 1 && (*str == '*' || *str == '@')) ||
        !strncmpi(str, randomstr, len))
        return ROLE_RANDOM;

    /* Couldn't find anything appropriate */
    return ROLE_NONE;
}


boolean
validalign(int rolenum, int racenum, int alignnum)
{
    /* Assumes nh_validrole and nh_validrace */
    return (alignnum >= 0 && alignnum < ROLE_ALIGNS &&
            (roles[rolenum].allow & races[racenum].allow &
             aligns[alignnum].allow & ROLE_ALIGNMASK));
}


int
str2align(char *str)
{
    int i, len;

    /* Is str valid? */
    if (!str || !str[0])
        return ROLE_NONE;

    /* Match as much of str as is provided */
    len = strlen(str);
    for (i = 0; i < ROLE_ALIGNS; i++) {
        /* Does it match the adjective? */
        if (!strncmpi(str, aligns[i].adj, len))
            return i;
        /* Or the filecode? */
        if (!strcmpi(str, aligns[i].filecode))
            return i;
    }
    if ((len == 1 && (*str == '*' || *str == '@')) ||
        !strncmpi(str, randomstr, len))
        return ROLE_RANDOM;

    /* Couldn't find anything appropriate */
    return ROLE_NONE;
}


/* is racenum compatible with any rolenum/gendnum/alignnum constraints? */
boolean
ok_race(int rolenum, int racenum, int gendnum, int alignnum)
{
    int i;
    short allow;

    if (racenum >= 0 && racenum < SIZE(races) - 1) {
        allow = races[racenum].allow;
        if (rolenum >= 0 && rolenum < SIZE(roles) - 1 &&
            !(allow & roles[rolenum].allow & ROLE_RACEMASK))
            return FALSE;
        if (gendnum >= 0 && gendnum < ROLE_GENDERS &&
            !(allow & genders[gendnum].allow & ROLE_GENDMASK))
            return FALSE;
        if (alignnum >= 0 && alignnum < ROLE_ALIGNS &&
            !(allow & aligns[alignnum].allow & ROLE_ALIGNMASK))
            return FALSE;
        return TRUE;
    } else {
        for (i = 0; i < SIZE(races) - 1; i++) {
            allow = races[i].allow;
            if (rolenum >= 0 && rolenum < SIZE(roles) - 1 &&
                !(allow & roles[rolenum].allow & ROLE_RACEMASK))
                continue;
            if (gendnum >= 0 && gendnum < ROLE_GENDERS &&
                !(allow & genders[gendnum].allow & ROLE_GENDMASK))
                continue;
            if (alignnum >= 0 && alignnum < ROLE_ALIGNS &&
                !(allow & aligns[alignnum].allow & ROLE_ALIGNMASK))
                continue;
            return TRUE;
        }
        return FALSE;
    }
}


/* is gendnum compatible with any rolenum/racenum/alignnum constraints? */
/* gender and alignment are not comparable (and also not constrainable) */
boolean
ok_gend(int rolenum, int racenum, int gendnum, int alignnum)
{
    int i;
    short allow;

    if (gendnum >= 0 && gendnum < ROLE_GENDERS) {
        allow = genders[gendnum].allow;
        if (rolenum >= 0 && rolenum < SIZE(roles) - 1 &&
            !(allow & roles[rolenum].allow & ROLE_GENDMASK))
            return FALSE;
        if (racenum >= 0 && racenum < SIZE(races) - 1 &&
            !(allow & races[racenum].allow & ROLE_GENDMASK))
            return FALSE;
        return TRUE;
    } else {
        for (i = 0; i < ROLE_GENDERS; i++) {
            allow = genders[i].allow;
            if (rolenum >= 0 && rolenum < SIZE(roles) - 1 &&
                !(allow & roles[rolenum].allow & ROLE_GENDMASK))
                continue;
            if (racenum >= 0 && racenum < SIZE(races) - 1 &&
                !(allow & races[racenum].allow & ROLE_GENDMASK))
                continue;
            return TRUE;
        }
        return FALSE;
    }
}


/* is alignnum compatible with any rolenum/racenum/gendnum constraints? */
/* alignment and gender are not comparable (and also not constrainable) */
boolean
ok_align(int rolenum, int racenum, int gendnum, int alignnum)
{
    int i;
    short allow;

    if (alignnum >= 0 && alignnum < ROLE_ALIGNS) {
        allow = aligns[alignnum].allow;
        if (rolenum >= 0 && rolenum < SIZE(roles) - 1 &&
            !(allow & roles[rolenum].allow & ROLE_ALIGNMASK))
            return FALSE;
        if (racenum >= 0 && racenum < SIZE(races) - 1 &&
            !(allow & races[racenum].allow & ROLE_ALIGNMASK))
            return FALSE;
        return TRUE;
    } else {
        for (i = 0; i < ROLE_ALIGNS; i++) {
            allow = races[i].allow;
            if (rolenum >= 0 && rolenum < SIZE(roles) - 1 &&
                !(allow & roles[rolenum].allow & ROLE_ALIGNMASK))
                continue;
            if (racenum >= 0 && racenum < SIZE(races) - 1 &&
                !(allow & races[racenum].allow & ROLE_ALIGNMASK))
                continue;
            return TRUE;
        }
        return FALSE;
    }
}


struct nh_roles_info *
nh_get_roles(void)
{
    int i, rolenum, racenum, gendnum, alignnum, arrsize;
    struct nh_roles_info *info;
    const char **names, **names2;
    nh_bool *tmpmatrix;

    xmalloc_cleanup(&api_blocklist);

    info = xmalloc(&api_blocklist, sizeof (struct nh_roles_info));

    /* number of choices */
    for (i = 0; roles[i].name.m; i++)
        ;
    info->num_roles = i;

    for (i = 0; races[i].noun; i++)
        ;
    info->num_races = i;

    info->num_genders = ROLE_GENDERS;
    info->num_aligns = ROLE_ALIGNS;

    /* names of choices */
    names = xmalloc(&api_blocklist, info->num_roles * sizeof (char *));
    names2 = xmalloc(&api_blocklist, info->num_roles * sizeof (char *));
    for (i = 0; i < info->num_roles; i++) {
        names[i] = roles[i].name.m;
        names2[i] = roles[i].name.f;
    }
    info->rolenames_m = names;
    info->rolenames_f = names2;

    names = xmalloc(&api_blocklist, info->num_races * sizeof (char *));
    for (i = 0; i < info->num_races; i++)
        names[i] = races[i].noun;
    info->racenames = names;

    names = xmalloc(&api_blocklist, info->num_genders * sizeof (char *));
    for (i = 0; i < info->num_genders; i++)
        names[i] = genders[i].adj;
    info->gendnames = names;

    names = xmalloc(&api_blocklist, info->num_aligns * sizeof (char *));
    for (i = 0; i < info->num_aligns; i++)
        names[i] = aligns[i].adj;
    info->alignnames = names;

    /* valid combinations of choices */
    arrsize =
        info->num_roles * info->num_races * info->num_genders *
        info->num_aligns;
    tmpmatrix = xmalloc(&api_blocklist, arrsize * sizeof (nh_bool));
    memset(tmpmatrix, FALSE, arrsize * sizeof (nh_bool));
    for (rolenum = 0; rolenum < info->num_roles; rolenum++) {
        for (racenum = 0; racenum < info->num_races; racenum++) {
            if (!ok_race(rolenum, racenum, ROLE_NONE, ROLE_NONE))
                continue;
            for (gendnum = 0; gendnum < info->num_genders; gendnum++) {
                if (!ok_gend(rolenum, racenum, gendnum, ROLE_NONE))
                    continue;
                for (alignnum = 0; alignnum < info->num_aligns; alignnum++) {
                    tmpmatrix[nh_cm_idx
                              ((*info), rolenum, racenum, gendnum, alignnum)] =
                        ok_align(rolenum, racenum, gendnum, alignnum);
                }
            }
        }
    }
    info->matrix = tmpmatrix;

    return info;
}


#define BP_ALIGN        0
#define BP_GEND         1
#define BP_RACE         2
#define BP_ROLE         3
#define NUM_BP          4

static char pa[NUM_BP], post_attribs;

static char *
promptsep(char *buf, int num_post_attribs)
{
    const char *conj = "and ";

    if (num_post_attribs > 1 && post_attribs < num_post_attribs &&
        post_attribs > 1)
        strcat(buf, ",");
    strcat(buf, " ");
    --post_attribs;
    if (!post_attribs && num_post_attribs > 1)
        strcat(buf, conj);
    return buf;
}

static int
role_gendercount(int rolenum)
{
    int gendcount = 0;

    if (validrole(rolenum)) {
        if (roles[rolenum].allow & ROLE_MALE)
            ++gendcount;
        if (roles[rolenum].allow & ROLE_FEMALE)
            ++gendcount;
        if (roles[rolenum].allow & ROLE_NEUTER)
            ++gendcount;
    }
    return gendcount;
}

static int
race_alignmentcount(int racenum)
{
    int aligncount = 0;

    if (racenum != ROLE_NONE && racenum != ROLE_RANDOM) {
        if (races[racenum].allow & ROLE_CHAOTIC)
            ++aligncount;
        if (races[racenum].allow & ROLE_LAWFUL)
            ++aligncount;
        if (races[racenum].allow & ROLE_NEUTRAL)
            ++aligncount;
    }
    return aligncount;
}


/* This uses a hardcoded BUFSZ, not the msg* functions, because it runs
   outside the main game sequence. */
const char *
nh_root_plselection_prompt(char *suppliedbuf, int buflen, int rolenum,
                           int racenum, int gendnum, int alignnum)
{
    int k, gendercount = 0, aligncount = 0;
    char buf[BUFSZ];
    static const char err_ret[] = " character's";
    boolean donefirst = FALSE;

    xmalloc_cleanup(&api_blocklist);

    if (!suppliedbuf || buflen < 1)
        return err_ret;

    /* initialize these static variables each time this is called */
    post_attribs = 0;
    for (k = 0; k < NUM_BP; ++k)
        pa[k] = 0;
    buf[0] = '\0';
    *suppliedbuf = '\0';

    /* How many alignments are allowed for the desired race? */
    if (racenum != ROLE_NONE && racenum != ROLE_RANDOM)
        aligncount = race_alignmentcount(racenum);

    if (alignnum != ROLE_NONE && alignnum != ROLE_RANDOM) {
        /* if race specified, and multiple choice of alignments for it */
        if (donefirst)
            strcat(buf, " ");
        strcat(buf, aligns[alignnum].adj);
        donefirst = TRUE;
    } else {
        /* if alignment not specified, but race is specified and only one
           choice of alignment for that race then don't include it in the later 
           list */
        if ((((racenum != ROLE_NONE && racenum != ROLE_RANDOM) &&
              ok_race(rolenum, racenum, gendnum, alignnum))
             && (aligncount > 1))
            || (racenum == ROLE_NONE || racenum == ROLE_RANDOM)) {
            pa[BP_ALIGN] = 1;
            post_attribs++;
        }
    }
    /* <your lawful> */

    /* How many genders are allowed for the desired role? */
    if (validrole(rolenum))
        gendercount = role_gendercount(rolenum);

    if (gendnum != ROLE_NONE && gendnum != ROLE_RANDOM) {
        if (validrole(rolenum)) {
            /* if role specified, and multiple choice of genders for it, and
               name of role itself does not distinguish gender */
            if ((rolenum != ROLE_NONE) && (gendercount > 1)
                && !roles[rolenum].name.f) {
                if (donefirst)
                    strcat(buf, " ");
                strcat(buf, genders[gendnum].adj);
                donefirst = TRUE;
            }
        } else {
            if (donefirst)
                strcat(buf, " ");
            strcat(buf, genders[gendnum].adj);
            donefirst = TRUE;
        }
    } else {
        /* if gender not specified, but role is specified and only one choice
           of gender then don't include it in the later list */
        if ((validrole(rolenum) && (gendercount > 1)) || !validrole(rolenum)) {
            pa[BP_GEND] = 1;
            post_attribs++;
        }
    }
    /* <your lawful female> */

    if (racenum != ROLE_NONE && racenum != ROLE_RANDOM) {
        if (validrole(rolenum) &&
            ok_race(rolenum, racenum, gendnum, alignnum)) {
            if (donefirst)
                strcat(buf, " ");
            strcat(buf,
                   (rolenum ==
                    ROLE_NONE) ? races[racenum].noun : races[racenum].adj);
            donefirst = TRUE;
        } else if (!validrole(rolenum)) {
            if (donefirst)
                strcat(buf, " ");
            strcat(buf, races[racenum].noun);
            donefirst = TRUE;
        } else {
            pa[BP_RACE] = 1;
            post_attribs++;
        }
    } else {
        pa[BP_RACE] = 1;
        post_attribs++;
    }
    /* <your lawful female gnomish> || <your lawful female gnome> */

    if (validrole(rolenum)) {
        if (donefirst)
            strcat(buf, " ");
        if (gendnum != ROLE_NONE) {
            if (gendnum == 1 && roles[rolenum].name.f)
                strcat(buf, roles[rolenum].name.f);
            else
                strcat(buf, roles[rolenum].name.m);
        } else {
            if (roles[rolenum].name.f) {
                strcat(buf, roles[rolenum].name.m);
                strcat(buf, "/");
                strcat(buf, roles[rolenum].name.f);
            } else
                strcat(buf, roles[rolenum].name.m);
        }
        donefirst = TRUE;
    } else if (rolenum == ROLE_NONE) {
        pa[BP_ROLE] = 1;
        post_attribs++;
    }

    if ((racenum == ROLE_NONE || racenum == ROLE_RANDOM) &&
        !validrole(rolenum)) {
        if (donefirst)
            strcat(buf, " ");
        strcat(buf, "character");
    }
    /* <your lawful female gnomish cavewoman> || <your lawful female gnome> ||
       <your lawful female character> */
    if (buflen > (int)(strlen(buf) + 1)) {
        strcpy(suppliedbuf, buf);
        return suppliedbuf;
    } else
        return err_ret;
}

/* This uses a hardcoded BUFSZ, not the msg* functions, because it runs
   outside the main game sequence. */
char *
nh_build_plselection_prompt(char *buf, int buflen, int rolenum, int racenum,
                            int gendnum, int alignnum)
{
    const char *defprompt = "Shall I pick a character for you?";
    int num_post_attribs = 0;
    char tmpbuf[BUFSZ];

    xmalloc_cleanup(&api_blocklist);

    if (buflen < QBUFSZ) {
        strncpy(buf, defprompt, buflen);
        buf[buflen-1] = '\0'; /* strncpy doesn't \0-terminate on overflow */
        return buf;
    }

    strcpy(tmpbuf, "Shall I pick ");
    if (racenum != ROLE_NONE || validrole(rolenum))
        strcat(tmpbuf, "your ");
    else {
        strcat(tmpbuf, "a ");
    }
    /* <your> */

    nh_root_plselection_prompt(
        tmpbuf + strlen(tmpbuf), buflen - strlen(tmpbuf),
        rolenum, racenum, gendnum, alignnum);
    /* A manual 's is used here because s_suffix will allocate onto the
     * turnstate chain, which leads to a leak. Furthermore, all races are
     * singular, so this is more grammatically correct.
     */
    sprintf(buf, "%s's", tmpbuf);

    /* buf should now be: < your lawful female gnomish cavewoman's> || <your
       lawful female gnome's> || <your lawful female character's> Now append
       the post attributes to it */

    num_post_attribs = post_attribs;
    if (post_attribs) {
        if (pa[BP_RACE]) {
            promptsep(buf + strlen(buf), num_post_attribs);
            strcat(buf, "race");
        }
        if (pa[BP_ROLE]) {
            promptsep(buf + strlen(buf), num_post_attribs);
            strcat(buf, "role");
        }
        if (pa[BP_GEND]) {
            promptsep(buf + strlen(buf), num_post_attribs);
            strcat(buf, "gender");
        }
        if (pa[BP_ALIGN]) {
            promptsep(buf + strlen(buf), num_post_attribs);
            strcat(buf, "alignment");
        }
    }
    strcat(buf, " for you?");
    return buf;
}

#undef BP_ALIGN
#undef BP_GEND
#undef BP_RACE
#undef BP_ROLE
#undef NUM_BP


/*
 * Special setup modifications here:
 *
 * Unfortunately, this is going to have to be done
 * on each newgame or restore, because you lose the permonst mods
 * across a save/restore.  :-)
 *
 *      1 - The Rogue Leader is the Tourist Nemesis.
 *      2 - Priests start with a random alignment - convert the leader and
 *          guardians here.
 *      3 - Elves can have one of two different leaders, but can't work it
 *          out here because it requires hacking the level file data (see
 *          sp_lev.c).
 *
 * This code also replaces quest_init().
 */
void
role_init(void)
{
    int alignmnt;

    alignmnt = aligns[u.initalign].value;

    /* Initialize urole and urace */
    urole = roles[u.initrole];
    urace = races[u.initrace];

    /* Fix up the quest leader */
    pm_leader = mons[urole.ldrnum];
    if (urole.ldrnum != NON_PM) {
        pm_leader.msound = MS_LEADER;
        pm_leader.mflags2 |= (M2_PEACEFUL);
        pm_leader.mflags3 |= M3_CLOSE;
        pm_leader.maligntyp = alignmnt * 3;
    }

    /* Fix up the quest guardians */
    pm_guardian = mons[urole.guardnum];
    if (urole.guardnum != NON_PM) {
        pm_guardian.mflags2 |= (M2_PEACEFUL);
        pm_guardian.maligntyp = alignmnt * 3;
    }

    /* Fix up the quest nemesis */
    pm_nemesis = mons[urole.neminum];
    if (urole.neminum != NON_PM) {
        pm_nemesis.msound = MS_NEMESIS;
        pm_nemesis.mflags2 &= ~(M2_PEACEFUL);
        pm_nemesis.mflags2 |= (M2_NASTY | M2_STALK | M2_HOSTILE);
        pm_nemesis.mflags3 |= M3_WANTSARTI | M3_WAITFORU;
    }

    pm_you_male = pm_you_female = mons[urole.num];

    /* Artifacts are fixed in hack_artifacts() */

    /* Success! */
    return;
}

/* Initialize the player's pantheon.
 *
 * This must come after both u and urole are initialized by role_init 
 * and by either u_init or restore_you
 */
void
pantheon_init(boolean newgame)
{
    if (newgame) {
        u.upantheon = u.initrole;            /* use own gods */
        while (!roles[u.upantheon].lgod)     /* unless they're missing */
            u.upantheon = randrole(rng_charstats_role);
    }

    if (!urole.lgod) {
        urole.lgod = roles[u.upantheon].lgod;
        urole.ngod = roles[u.upantheon].ngod;
        urole.cgod = roles[u.upantheon].cgod;
    }
}

const char *
Hello(struct monst *mtmp)
{
    switch (Role_switch) {
    case PM_KNIGHT:
        return "Salutations";   /* Olde English */
    case PM_SAMURAI:
        return (mtmp && mtmp->data == &mons[PM_SHOPKEEPER] ?
                "Irasshaimase" : "Konnichi wa");   /* Japanese */
    case PM_TOURIST:
        return "Aloha"; /* Hawaiian */
    case PM_VALKYRIE:
        return "Velkommen";     /* Norse */
    default:
        return "Hello";
    }
}

const char *
Goodbye(void)
{
    switch (Role_switch) {
    case PM_KNIGHT:
        return "Fare thee well";        /* Olde English */
    case PM_SAMURAI:
        return "Sayonara";      /* Japanese */
    case PM_TOURIST:
        return "Aloha"; /* Hawaiian */
    case PM_VALKYRIE:
        return "Farvel";        /* Norse */
    default:
        return "Goodbye";
    }
}

/* role.c */

