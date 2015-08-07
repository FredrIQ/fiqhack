/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MAGIC_H
# define MAGIC_H

/* magic numbers to mark sections in the savegame */
# define LEVEL_MAGIC            0x4c56454c      /* "LEVL" */
# define STATE_MAGIC            0x54415453      /* "STAT" */
# define OBJ_MAGIC              0x004a424f      /* "OBJ\0" */
# define MON_MAGIC              0x004e4f4d      /* "MON\0" */
# define OBJCHAIN_MAGIC         0x4e48434f      /* "OCHN" */
# define MONCHAIN_MAGIC         0x4e48434d      /* "MCHN" */
# define FRUITCHAIN_MAGIC       0x48435246      /* "FRCH" */
# define TRAPCHAIN_MAGIC        0x53505254      /* "TRPS" */
# define REGION_MAGIC           0x49474552      /* "REGI" */
# define ROOMS_MAGIC            0x54424452      /* "RDAT" */
# define OCLASSES_MAGIC         0x4c4c434f      /* "OCLL" */
# define ENGRAVE_MAGIC          0x52474e45      /* "ENGR" */
# define HISTORY_MAGIC          0x54534948      /* "HIST" */
# define DIG_MAGIC              0x53474944      /* "DIGS" */
# define DGN_MAGIC              0x004e4744      /* "DGN\0" */

#endif

