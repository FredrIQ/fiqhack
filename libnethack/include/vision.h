/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-02-27 */
/* Copyright (c) Dean Luick, with acknowledgements to Dave Cohrs, 1990. */
/* NetHack may be freely redistributed.  See license for details.       */

#ifndef VISION_H
# define VISION_H

# define COULD_SEE 0x1  /* location could be seen, if it were lit */
# define IN_SIGHT  0x2  /* location can be seen */
# define TEMP_LIT  0x4  /* location is temporarily lit */

/*
 * Light source sources
 */
# define LS_OBJECT 0
# define LS_MONSTER 1

/*
 *  cansee()    - Returns true if the hero can see the location.
 *
 *  couldsee()  - Returns true if the hero has a clear line of sight to
 *                the location.
 */
# define cansee(x,y)   (isok(x, y) && viz_array[y][x] & IN_SIGHT)
# define couldsee(x,y) (isok(x, y) && viz_array[y][x] & COULD_SEE)
# define templit(x,y)  (isok(x, y) && viz_array[y][x] & TEMP_LIT)

/*
 *  mcansee()   - Returns true if the monster has a clear line of sight to
 *                a given square. Do not use on a square that contains an
 *                engulfed player (due to ambiguity as to whether you're
 *                aiming for the player's location, or the square's).
 */
# define m_cansee(mtmp,x2,y2) \
    (mtmp->mcansee && \
     clear_path((mtmp)->mx,(mtmp)->my,(x2),(y2),viz_array))

/*
 *  Circle information
 */
# define MAX_RADIUS 15  /* this is in points from the source */

/* Use this macro to get a list of distances of the edges (see vision.c). */
# define circle_ptr(z) (&circle_data[(int)circle_start[z]])

extern const char circle_data[];
extern const char circle_start[];

#endif /* VISION_H */

