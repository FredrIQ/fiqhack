#ifndef WINCAP_H
#define WINCAP_H

/*
 * WINCAP
 * Window port preference capability bits.
 */
#define WC_COLOR	 0x01L		/* 01 Port can display things in color       */
#define WC_HILITE_PET	 0x02L		/* 02 supports hilite pet                    */
#define WC_ASCII_MAP	 0x04L		/* 03 supports an ascii map                  */
#define WC_TILED_MAP	 0x08L		/* 04 supports a tiled map                   */
#define WC_PRELOAD_TILES 0x10L		/* 05 supports pre-loading tiles             */
#define WC_TILE_WIDTH	 0x20L		/* 06 prefer this width of tile              */
#define WC_TILE_HEIGHT	 0x40L		/* 07 prefer this height of tile             */
#define WC_TILE_FILE	 0x80L		/* 08 alternative tile file name             */
#define WC_INVERSE	 0x100L		/* 09 Port supports inverse video            */
#define WC_ALIGN_MESSAGE 0x200L		/* 10 supports message alignmt top|b|l|r     */
#define WC_ALIGN_STATUS	 0x400L		/* 11 supports status alignmt top|b|l|r      */
#define WC_VARY_MSGCOUNT 0x800L		/* 12 supports varying message window        */
#define WC_FONT_MAP	 0x1000L	/* 13 supports specification of map win font */
#define WC_FONT_MESSAGE	 0x2000L	/* 14 supports specification of msg win font */
#define WC_FONT_STATUS	 0x4000L	/* 15 supports specification of sts win font */
#define WC_FONT_MENU	 0x8000L	/* 16 supports specification of mnu win font */
#define WC_FONT_TEXT	 0x10000L	/* 17 supports specification of txt win font */
#define WC_FONTSIZ_MAP	 0x20000L	/* 18 supports specification of map win font */
#define WC_FONTSIZ_MESSAGE 0x40000L	/* 19 supports specification of msg win font */
#define WC_FONTSIZ_STATUS 0x80000L	/* 20 supports specification of sts win font */
#define WC_FONTSIZ_MENU	 0x100000L	/* 21 supports specification of mnu win font */
#define WC_FONTSIZ_TEXT	 0x200000L	/* 22 supports specification of txt win font */
#define WC_SCROLL_MARGIN 0x400000L	/* 23 supports setting scroll margin for map */
#define WC_SPLASH_SCREEN 0x800000L	/* 24 supports display of splash screen      */
#define WC_POPUP_DIALOG	 0x1000000L	/* 25 supports queries in pop dialogs        */
#define WC_SCROLL_AMOUNT 0x2000000L	/* 26 scroll this amount at scroll margin    */
#define WC_EIGHT_BIT_IN	 0x4000000L	/* 27 8-bit character input                  */
#define WC_PERM_INVENT	 0x8000000L	/* 28 8-bit character input                  */
#define WC_MAP_MODE	 0x10000000L	/* 29 map_mode option                        */
#define WC_WINDOWCOLORS  0x20000000L	/* 30 background color for message window    */
#define WC_PLAYER_SELECTION  0x40000000L /* 31 background color for message window    */
#define WC_MOUSE_SUPPORT 0x80000000L	/* 32 mouse support                          */
					/* no free bits */

#define WC2_FULLSCREEN		0x01L	/* 01 display full screen                    */
#define WC2_SOFTKEYBOARD	0x02L	/* 02 software keyboard                      */
#define WC2_WRAPTEXT		0x04L	/* 04 wrap long lines of text                */
					/* 29 free bits */

#endif
