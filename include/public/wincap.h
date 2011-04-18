#ifndef WINCAP_H
#define WINCAP_H

/*
 * WINCAP
 * Window port preference capability bits.
 */
#define WC_HILITE_PET	 0x02L		/* 02 supports hilite pet                    */
#define WC_INVERSE	 0x100L		/* 09 Port supports inverse video            */
#define WC_EIGHT_BIT_IN	 0x4000000L	/* 27 8-bit character input                  */
#define WC_PERM_INVENT	 0x8000000L	/* 28 8-bit character input                  */
					/* no free bits */

#endif
