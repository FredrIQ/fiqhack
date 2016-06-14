#ifndef ZAP_H
# define ZAP_H

# include "monattk.h"

/* Defines for zap.c */

# define ZT_MAGIC_MISSILE        (AD_MAGM-1)
# define ZT_FIRE                 (AD_FIRE-1)
# define ZT_COLD                 (AD_COLD-1)
# define ZT_SLEEP                (AD_SLEE-1)
# define ZT_DEATH                (AD_DISN-1) /* or disintegration */
# define ZT_LIGHTNING            (AD_ELEC-1)
# define ZT_POISON_GAS           (AD_DRST-1)
# define ZT_ACID                 (AD_ACID-1)
# define ZT_STUN                 (AD_STUN-1)
/* 9 is currently unassigned */

# define ZT_WAND(x)              (x)
# define ZT_SPELL(x)             (10+(x))
# define ZT_BREATH(x)            (20+(x))

# define BHIT_NONE     0x0
# define BHIT_MON      0x1
# define BHIT_OBJ      0x2
# define BHIT_OBSTRUCT 0x4
# define BHIT_SHOPDAM  0x8

# define is_hero_spell(type) ((type) >= 10 && (type) < 20)

#endif /* ZAP_H */
