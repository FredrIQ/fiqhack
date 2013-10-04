/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Daniel Thaler, 2011.                             */
/* NetHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <ctype.h>
#include <time.h>


#define LISTSZ 32

static int
is_valid_character(const struct nh_roles_info *ri, int rolenum, int racenum,
                   int gendnum, int alignnum)
{
    int i, j, k, l, ok;

    ok = 0;
    for (i = 0; i < ri->num_roles; i++) {
        if (rolenum > ROLE_NONE && i != rolenum)
            continue;

        for (j = 0; j < ri->num_races; j++) {
            if (racenum > ROLE_NONE && j != racenum)
                continue;

            for (k = 0; k < ri->num_genders; k++) {
                if (gendnum > ROLE_NONE && k != gendnum)
                    continue;

                for (l = 0; l < ri->num_aligns; l++) {
                    if (alignnum > ROLE_NONE && l != alignnum)
                        continue;

                    if (ri->matrix[nh_cm_idx((*ri), i, j, k, l)])
                        ok++;
                }
            }
        }
    }

    return ok > 0;
}


static int
get_valid_roles(const struct nh_roles_info *ri, int racenum, int gendnum,
                int alignnum, struct nh_listitem *list, int listlen)
{
    int i;
    int count = 0;
    char rolenamebuf[256];

    for (i = 0; i < ri->num_roles; i++) {
        if (!is_valid_character(ri, i, racenum, gendnum, alignnum))
            continue;

        if (list && count < listlen) {
            if (gendnum != ROLE_NONE && gendnum != ROLE_RANDOM) {
                if (gendnum == 1 && ri->rolenames_f[i])
                    strcpy(rolenamebuf, ri->rolenames_f[i]);
                else
                    strcpy(rolenamebuf, ri->rolenames_m[i]);
            } else {
                if (ri->rolenames_f[i]) {
                    strcpy(rolenamebuf, ri->rolenames_m[i]);
                    strcat(rolenamebuf, "/");
                    strcat(rolenamebuf, ri->rolenames_f[i]);
                } else
                    strcpy(rolenamebuf, ri->rolenames_m[i]);
            }
            strcpy(list[count].caption, rolenamebuf);
            list[count].id = i;
        }
        count++;
    }
    return count;
}


static int
get_valid_races(const struct nh_roles_info *ri, int rolenum, int gendnum,
                int alignnum, struct nh_listitem *list, int listlen)
{
    int i;
    int count = 0;

    for (i = 0; i < ri->num_races; i++) {
        if (!is_valid_character(ri, rolenum, i, gendnum, alignnum))
            continue;

        if (list && count < listlen) {
            strcpy(list[count].caption, ri->racenames[i]);
            list[count].id = i;
        }
        count++;
    }
    return count;
}


static int
get_valid_genders(const struct nh_roles_info *ri, int rolenum, int racenum,
                  int alignnum, struct nh_listitem *list, int listlen)
{
    int i;
    int count = 0;

    for (i = 0; i < ri->num_genders; i++) {
        if (!is_valid_character(ri, rolenum, racenum, i, alignnum))
            continue;

        if (list && count < listlen) {
            strcpy(list[count].caption, ri->gendnames[i]);
            list[count].id = i;
        }
        count++;
    }
    return count;
}


static int
get_valid_aligns(const struct nh_roles_info *ri, int rolenum, int racenum,
                 int gendnum, struct nh_listitem *list, int listlen)
{
    int i;
    int count = 0;

    for (i = 0; i < ri->num_aligns; i++) {
        if (!is_valid_character(ri, rolenum, racenum, gendnum, i))
            continue;

        if (list && count < listlen) {
            strcpy(list[count].caption, ri->alignnames[i]);
            list[count].id = i;
        }
        count++;
    }
    return count;
}


static void
validate_character_presets(const struct nh_roles_info *ri, int *role, int *race,
                           int *gend, int *align)
{
    struct nh_listitem list[1];
    char listbuffer[256];

    list[0].caption = listbuffer;

    /* throw out obviously invalid values */
    if (*role < ROLE_RANDOM || *role >= ri->num_roles)
        *role = ROLE_NONE;
    if (*race < ROLE_RANDOM || *race >= ri->num_races)
        *race = ROLE_NONE;
    if (*gend < ROLE_RANDOM || *gend >= ri->num_genders)
        *gend = ROLE_NONE;
    if (*align < ROLE_RANDOM || *align >= ri->num_aligns)
        *align = ROLE_NONE;

    /* catch mutually incompatible character options (male valkyrie) */
    if (!is_valid_character(ri, *role, *race, *gend, *align)) {
        /* always keep the role */
        if (is_valid_character(ri, *role, *race, *gend, ROLE_NONE)) {
            curses_msgwin("Incompatible alignment!");
            *align = ROLE_NONE;
        } else if (is_valid_character(ri, *role, *race, ROLE_NONE, *align)) {
            curses_msgwin("Incompatible gender!");
            *gend = ROLE_NONE;
        } else if (is_valid_character(ri, *role, ROLE_NONE, *gend, *align)) {
            curses_msgwin("Incompatible race!");
            *race = ROLE_NONE;
        } else {
            curses_msgwin("Incompatible character presets!");
            *race = ROLE_NONE;
            *gend = ROLE_NONE;
            *align = ROLE_NONE;
        }
    }

    /* if there is only one possible option for any attribute, choose it here */
    if ((*role == ROLE_NONE || *role == ROLE_RANDOM) &&
        get_valid_roles(ri, *race, *gend, *align, list, 1) == 1)
        *role = list[0].id;

    if ((*race == ROLE_NONE || *race == ROLE_RANDOM) &&
        get_valid_races(ri, *role, *gend, *align, list, 1) == 1)
        *race = list[0].id;

    if ((*gend == ROLE_NONE || *gend == ROLE_RANDOM) &&
        get_valid_genders(ri, *role, *race, *align, list, 1) == 1)
        *gend = list[0].id;

    if ((*align == ROLE_NONE || *align == ROLE_RANDOM) &&
        get_valid_aligns(ri, *role, *race, *gend, list, 1) == 1)
        *align = list[0].id;
}


nh_bool
player_selection(int *out_role, int *out_race, int *out_gend, int *out_align,
                 int randomall)
{
    struct nh_menuitem *items;
    int icount, size;
    int i, k, n, listlen, id;
    char pick4u = 'n', thisch, lastch = 0;
    char pbuf[QBUFSZ], plbuf[QBUFSZ];
    struct nh_listitem list[LISTSZ];    /* need enough space for lists of roles 
                                           or races */
    char listbuffers[LISTSZ][256];
    int pick_list[2];
    int role, race, gend, align;
    const struct nh_roles_info *ri = nh_get_roles();

    role = *out_role;
    race = *out_race;
    align = *out_align;
    gend = *out_gend;
    if (role == ROLE_NONE)
        role = ri->def_role;
    if (race == ROLE_NONE)
        race = ri->def_race;
    if (gend == ROLE_NONE)
        gend = ri->def_gend;
    if (align == ROLE_NONE)
        align = ri->def_align;

    validate_character_presets(ri, &role, &race, &gend, &align);

    for (i = 0; i < LISTSZ; i++) {
        listbuffers[i][0] = '\0';
        list[i].caption = listbuffers[i];
    }

    srand(time(NULL));

    /* Should we randomly pick for the player? */
    if (!randomall &&
        (role == ROLE_NONE || race == ROLE_NONE || gend == ROLE_NONE ||
         align == ROLE_NONE)) {
        char *prompt = nh_build_plselection_prompt(pbuf, QBUFSZ, role,
                                                   race, gend, align);

        pick4u = curses_yn_function(prompt, "ynq", 'y');
        if (pick4u != 'y' && pick4u != 'n')
            return FALSE;
    }

    nh_root_plselection_prompt(plbuf, QBUFSZ - 1, role, race, gend, align);
    icount = 0;
    size = 10;
    items = malloc(sizeof (struct nh_menuitem) * size);

    /* Select a role, if necessary */
    /* if pre-selected race/gender/alignment are still set after
       validate_character_presets we know they're OK */
    if (role < 0) {
        listlen = get_valid_roles(ri, race, gend, align, list, LISTSZ);

        /* Process the choice */
        if (pick4u == 'y' || role == ROLE_RANDOM || randomall) {
            /* Pick a random role */
            role = list[random() % listlen].id;
        } else {
            /* Prompt for a role */
            for (i = 0; i < listlen; i++) {
                id = list[i].id + 1;    /* list[i].id starts at 0 */
                thisch = tolower(*list[i].caption);
                if (thisch == lastch)
                    thisch = toupper(thisch);
                add_menu_item(items, size, icount, id, list[i].caption, thisch,
                              0);
                lastch = thisch;
            }
            pick_list[0] = id = list[random() % listlen].id + 1;
            add_menu_item(items, size, icount, id, "Random", '*', 0);
            add_menu_item(items, size, icount, -1, "Quit", 'q', 0);

            sprintf(pbuf, "Pick a role for your %s", plbuf);
            n = curses_display_menu(items, icount, pbuf, PICK_ONE,
                                    PLHINT_ANYWHERE, pick_list);

            /* Process the choice */
            if (n == -1 || pick_list[0] == -1)
                goto give_up;   /* Selected quit */

            role = pick_list[0] - 1;
            icount = 0;
        }
        nh_root_plselection_prompt(plbuf, QBUFSZ - 1, role, race, gend, align);
    }

    /* Select a race, if necessary */
    if (race < 0) {
        listlen = get_valid_races(ri, role, gend, align, list, LISTSZ);

        if (pick4u == 'y' || race == ROLE_RANDOM || randomall) {
            race = list[random() % listlen].id;
        } else {        /* pick4u == 'n' */
            /* Count the number of valid races */
            k = list[0].id;     /* valid race */

            /* Permit the user to pick, if there is more than one */
            if (listlen > 1) {
                for (i = 0; i < listlen; i++) {
                    id = list[i].id + 1;
                    add_menu_item(items, size, icount, id, list[i].caption,
                                  list[i].caption[0], 0);
                }
                pick_list[0] = id = list[random() % listlen].id + 1;
                add_menu_item(items, size, icount, id, "Random", '*', 0);
                add_menu_item(items, size, icount, -1, "Quit", 'q', 0);

                sprintf(pbuf, "Pick the race of your %s", plbuf);
                n = curses_display_menu(items, icount, pbuf, PICK_ONE,
                                        PLHINT_ANYWHERE, pick_list);

                if (n == -1 || pick_list[0] == -1)
                    goto give_up;       /* Selected quit */

                k = pick_list[0] - 1;
                icount = 0;
            }
            race = k;
        }
        nh_root_plselection_prompt(plbuf, QBUFSZ - 1, role, race, gend, align);
    }

    /* Select a gender, if necessary */
    if (gend < 0) {
        listlen = get_valid_genders(ri, role, race, align, list, LISTSZ);

        if (pick4u == 'y' || gend == ROLE_RANDOM || randomall) {
            gend = list[random() % listlen].id;
        } else {        /* pick4u == 'n' */
            /* Count the number of valid genders */
            k = list[0].id;     /* valid gender */

            /* Permit the user to pick, if there is more than one */
            if (listlen > 1) {
                for (i = 0; i < listlen; i++) {
                    id = list[i].id + 1;
                    add_menu_item(items, size, icount, id, list[i].caption,
                                  list[i].caption[0], 0);
                }
                pick_list[0] = id = list[random() % listlen].id + 1;
                add_menu_item(items, size, icount, id, "Random", '*', 0);
                add_menu_item(items, size, icount, -1, "Quit", 'q', 0);

                sprintf(pbuf, "Pick the gender of your %s", plbuf);
                n = curses_display_menu(items, icount, pbuf, PICK_ONE,
                                        PLHINT_ANYWHERE, pick_list);

                if (n == -1 || pick_list[0] == -1)
                    goto give_up;       /* Selected quit */

                k = pick_list[0] - 1;
                icount = 0;
            }
            gend = k;
        }
        nh_root_plselection_prompt(plbuf, QBUFSZ - 1, role, race, gend, align);
    }

    /* Select an alignment, if necessary */
    if (align < 0) {
        listlen = get_valid_aligns(ri, role, race, gend, list, LISTSZ);

        if (pick4u == 'y' || align == ROLE_RANDOM || randomall) {
            align = list[random() % listlen].id;
        } else {        /* pick4u == 'n' */
            /* Count the number of valid alignments */
            k = list[0].id;     /* valid alignment */

            /* Permit the user to pick, if there is more than one */
            if (listlen > 1) {
                for (i = 0; i < listlen; i++) {
                    id = list[i].id + 1;
                    add_menu_item(items, size, icount, id, list[i].caption,
                                  list[i].caption[0], 0);
                }
                pick_list[0] = id = list[random() % listlen].id + 1;
                add_menu_item(items, size, icount, id, "Random", '*', 0);
                add_menu_item(items, size, icount, -1, "Quit", 'q', 0);

                sprintf(pbuf, "Pick the alignment of your %s", plbuf);
                n = curses_display_menu(items, icount, pbuf, PICK_ONE,
                                        PLHINT_ANYWHERE, pick_list);

                if (n == -1 || pick_list[0] == -1)
                    goto give_up;       /* Selected quit */

                k = pick_list[0] - 1;
            }
            align = k;
        }
    }

    *out_role = role;
    *out_race = race;
    *out_gend = gend;
    *out_align = align;

    free(items);
    return TRUE;

give_up:
    free(items);
    return FALSE;
}
