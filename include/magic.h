/* Copyright (c) Daniel Thaler, 2011 */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef MAGIC_H
#define MAGIC_H

/* magic numbers to mark sections in the savegame */
#define LEVEL_MAGIC		0x4c56454c /* "LEVL" */
#define STATE_MAGIC		0x54415453 /* "STAT" */
#define OBJ_MAGIC		0x004a424f /* "OBJ\0" */
#define MON_MAGIC		0x004e4f4d /* "MON\0" */
#define OBJCHAIN_MAGIC		0x4e48434f /* "OCHN" */
#define MONCHAIN_MAGIC		0x4e48434d /* "MCHN" */
#define FRUITCHAIN_MAGIC	0x48435246 /* "FRCH" */
#define TRAPCHAIN_MAGIC		0x53505254 /* "TRPS" */
#define REGION_MAGIC		0x49474552 /* "REGI" */
#define ROOMS_MAGIC		0x54424452 /* "RDAT" */
#define OCLASSES_MAGIC		0x4c4c434f /* "OCLL" */
#define ENGRAVE_MAGIC		0x52474e45 /* "ENGR" */

#endif
