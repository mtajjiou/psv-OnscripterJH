#ifndef __GUI_MAIN_H__
#define __GUI_MAIN_H__

#include <iostream>
#include <vector>
#include <vitasdk.h>

#define ICON_CIRCLE   "\xe2\x97\x8b"
#define ICON_CROSS    "\xe2\x95\xb3"
#define ICON_SQUARE   "\xe2\x96\xa1"
#define ICON_TRIANGLE "\xe2\x96\xb3"
#define ICON_UPDOWN   "\xe2\x86\x95"

#define SCREEN_WIDTH                960
#define SCREEN_HEIGHT               544
#define SCREEN_HALF_WIDTH           (SCREEN_WIDTH / 2)
#define SCREEN_HALF_HEIGHT          (SCREEN_HEIGHT / 2)
#define HEADER_HEIGHT               40
#define FOOTER_HEIGHT               30

#define ITEMS_PANEL_PADDING         5
#define ITEMS_PANEL_WIDTH           (SCREEN_WIDTH)
#define ITEMS_PANEL_INNER_WIDTH     (ITEMS_PANEL_WIDTH - (ITEMS_PANEL_PADDING * 2))
#define ITEMS_PANEL_HEIGHT          (SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT)
#define ITEMS_PANEL_INNER_HEIGHT    (ITEMS_PANEL_HEIGHT - (ITEMS_PANEL_PADDING * 2))
#define ITEMS_PANEL_TOP             (HEADER_HEIGHT)
#define ITEMS_PANEL_LEFT            (0)
#define ITEMS_INNER_TOP             (ITEMS_PANEL_TOP + ITEMS_PANEL_PADDING)
#define ITEMS_INNER_LEFT            (ITEMS_PANEL_LEFT + ITEMS_PANEL_PADDING)

#define FOOTER_TOP	                (HEADER_HEIGHT + ITEMS_PANEL_HEIGHT)

#define ITEM_BOX_MARGIN             5
#define ITEM_BOX_PADDING            0
#define ITEM_BOX_TOP(y, h)          (ITEMS_INNER_TOP + (ITEM_BOX_MARGIN * ((y) + 1)) + ((h) * (y)))
#define ITEM_BOX_LEFT(x, w)         (ITEMS_INNER_LEFT + (ITEM_BOX_MARGIN * ((x) + 1)) + ((w) * (x)))
#define ITEM_BOX_WIDTH(col)         (int)((ITEMS_PANEL_INNER_WIDTH - (ITEM_BOX_MARGIN * ((col) + 1))) / (col))
#define ITEM_BOX_HEIGHT(row)        (int)((ITEMS_PANEL_INNER_HEIGHT - (ITEM_BOX_MARGIN * ((row) + 1))) / (row))

//#define ICONS_ROW                   4
//#define ICONS_COL                   7
#define ICON_ROW_MAX				6
#define ICON_ROW_MIN				1
#define ICON_COL_MAX				10
#define ICON_COL_MIN				3
#define ICON_BOX_WIDTH              ITEM_BOX_WIDTH(ICONS_COL)
#define ICON_BOX_HEIGHT             ITEM_BOX_HEIGHT(ICONS_ROW)
#define ICON_WIDTH                  (ICON_BOX_WIDTH - (ITEM_BOX_PADDING * 2))
#define ICON_HEIGHT                 (ICON_BOX_HEIGHT - (ITEM_BOX_PADDING * 2))
#define ICON_TOP(y)                 (ITEM_BOX_TOP((y), ICON_BOX_HEIGHT) + ITEM_BOX_PADDING)
#define ICON_LEFT(x)                (ITEM_BOX_LEFT((x), ICON_BOX_WIDTH) + ITEM_BOX_PADDING)

//#define LIST_ROW                    8
#define LIST_ROW_MAX				10
#define LIST_ROW_MIN				3
#define LIST_BOX_WIDTH              ITEM_BOX_WIDTH(1)
#define LIST_BOX_HEIGHT             ITEM_BOX_HEIGHT(LIST_ROW)
#define LIST_WIDTH                  (LIST_BOX_WIDTH - (ITEM_BOX_PADDING * 2))
#define LIST_HEIGHT                 (LIST_BOX_HEIGHT - (ITEM_BOX_PADDING * 2))
#define LIST_TOP(y)                 (ITEM_BOX_TOP((y), LIST_BOX_HEIGHT) + ITEM_BOX_PADDING)
#define LIST_LEFT                   (ITEM_BOX_LEFT(0, LIST_BOX_WIDTH) + ITEM_BOX_PADDING)
#define LIST_TEXT_LEFT              LIST_LEFT + LIST_HEIGHT + ITEM_BOX_MARGIN
#define LIST_TEXT_WIDTH             LIST_WIDTH - LIST_HEIGHT - ITEM_BOX_MARGIN

#define APPINFO_PANEL_TOP           (ITEMS_PANEL_TOP)
#define APPINFO_PANEL_LEFT          (ITEMS_PANEL_LEFT)
#define APPINFO_PANEL_WIDTH         (ITEMS_PANEL_WIDTH / 2)
#define APPINFO_PANEL_HEIGHT        (ITEMS_PANEL_HEIGHT)
#define APPINFO_PANEL_PADDING       5

#define APPINFO_DESC_TOP            (int)(APPINFO_PANEL_TOP + (APPINFO_PANEL_HEIGHT / 2) + APPINFO_PANEL_PADDING)
#define APPINFO_DESC_LEFT           (APPINFO_PANEL_LEFT + APPINFO_PANEL_PADDING)
#define APPINFO_DESC_WIDTH          (APPINFO_PANEL_WIDTH - (APPINFO_PANEL_PADDING * 2))
#define APPINFO_DESC_HEIGHT         (int)((APPINFO_PANEL_HEIGHT / 2) - (APPINFO_PANEL_PADDING * 2))
#define APPINFO_DESC_PADDING        5

#define APPINFO_ICON_PADDING        40
#define APPINFO_ICON_TOP            (APPINFO_PANEL_TOP + APPINFO_PANEL_PADDING + APPINFO_ICON_PADDING)
#define APPINFO_ICON_LEFT           (APPINFO_PANEL_LEFT + APPINFO_PANEL_PADDING + APPINFO_ICON_PADDING)
#define APPINFO_ICON_WIDTH          (int)((APPINFO_PANEL_WIDTH / 2) - (APPINFO_PANEL_PADDING * 2) - (APPINFO_ICON_PADDING * 2))
#define APPINFO_ICON_HEIGHT         (int)((APPINFO_PANEL_HEIGHT / 2) - (APPINFO_PANEL_PADDING * 2) - (APPINFO_ICON_PADDING * 2))

#define APPINFO_BUTTONS_TOP         (APPINFO_PANEL_TOP + APPINFO_PANEL_PADDING)
#define APPINFO_BUTTONS_LEFT        (int)(APPINFO_PANEL_LEFT + (APPINFO_PANEL_WIDTH / 2) + APPINFO_PANEL_PADDING)
#define APPINFO_BUTTONS_WIDTH       (int)((APPINFO_PANEL_WIDTH / 2) - (APPINFO_PANEL_PADDING * 2))
#define APPINFO_BUTTONS_HEIGHT      (int)((APPINFO_PANEL_HEIGHT / 2) - (APPINFO_PANEL_PADDING * 2))

#define APPINFO_BUTTON              5
#define APPINFO_BUTTON_MARGIN       5
#define APPINFO_BUTTON_WIDTH        (int)(APPINFO_BUTTONS_WIDTH - (APPINFO_BUTTON_MARGIN * 2))
#define APPINFO_BUTTON_HEIGHT       (int)((APPINFO_BUTTONS_HEIGHT - (APPINFO_BUTTON_MARGIN * (APPINFO_BUTTON + 1))) / APPINFO_BUTTON)
#define APPINFO_BUTTON_LEFT         (APPINFO_BUTTONS_LEFT + APPINFO_BUTTON_MARGIN)
#define APPINFO_BUTTON_TOP(x)       (APPINFO_BUTTONS_TOP + APPINFO_BUTTON_MARGIN + (APPINFO_BUTTON_HEIGHT + APPINFO_BUTTON_MARGIN) * (x))

#define SLOT_PANEL_TOP              (ITEMS_PANEL_TOP)
#define SLOT_PANEL_LEFT             (APPINFO_PANEL_LEFT + APPINFO_PANEL_WIDTH)
#define SLOT_PANEL_WIDTH            (ITEMS_PANEL_WIDTH / 2)
#define SLOT_PANEL_HEIGHT           (ITEMS_PANEL_HEIGHT)
#define SLOT_PANEL_PADDING          5

#define SLOT_HEADER_TOP             (ITEMS_PANEL_TOP + SLOT_PANEL_PADDING)
#define SLOT_HEADER_LEFT            (ITEMS_PANEL_LEFT + SLOT_PANEL_PADDING)
#define SLOT_HEADER_HEIGHT          40

#define SLOT_BUTTONS_TOP            (SLOT_HEADER_TOP + SLOT_HEADER_HEIGHT)
#define SLOT_BUTTONS_HEIGHT         (SLOT_PANEL_HEIGHT - SLOT_HEADER_HEIGHT - (SLOT_PANEL_PADDING * 2))
#define SLOT_BUTTON                 10
#define SLOT_BUTTON_MARGIN          5
#define SLOT_BUTTON_WIDTH           (int)(SLOT_PANEL_WIDTH - (SLOT_BUTTON_MARGIN * 2))
#define SLOT_BUTTON_HEIGHT          (int)((SLOT_BUTTONS_HEIGHT - (SLOT_BUTTON_MARGIN * (SLOT_BUTTON + 1))) / SLOT_BUTTON)
#define SLOT_BUTTON_LEFT            (SLOT_PANEL_LEFT + SLOT_BUTTON_MARGIN)
#define SLOT_BUTTON_TOP(x)          (SLOT_BUTTONS_TOP + SLOT_BUTTON_MARGIN + (SLOT_BUTTON_HEIGHT + SLOT_BUTTON_MARGIN) * (x))

#define ICON_BUF_SIZE               (256 * 1024)

#define BLACK_1 			RGBA8(0x05, 0x05, 0x05, 0xFF)
#define BLACK_HALF_ALPHA    RGBA8(0x00, 0x00, 0x00, 0xCF)
#define BLACK_80_ALPHA      RGBA8(0x00, 0x00, 0x00, 0x32)
#define BLACK               RGBA8(0x20, 0x20, 0x20, 0xFF)
#define LIGHT_SLATE_GRAY    RGBA8(0x77, 0x88, 0x99, 0xFF)
#define LIGHT_GRAY          RGBA8(0xD3, 0xD3, 0xD3, 0xD3)
#define WHITE               RGBA8(0xFF, 0xFF, 0xFF, 0xFF)
#define LIGHT_SKY_BLUE      RGBA8(0x87, 0xCE, 0xEB, 0xFF)
#define RED                 RGBA8(0xFF, 0x00, 0x00, 0xFF)
#define GREEN               RGBA8(0x00, 0x80, 0x00, 0xFF)

#define CONFIG_FILE "ux0:data/onsemu/ONSConfig.ini"
/* What the launcher already worked out about the games on the card, so it
 * does not work it out again on every start.  Safe to delete. */
#define MANIFEST_FILE "ux0:data/onsemu/game_manifest.json"

/* Where each binary writes what it prints, when logging is on. */
#define LAUNCHER_LOG_FILE "ux0:data/onsemu/launcher.log"
#define ENGINE_LOG_FILE   "ux0:data/onsemu/onsjh.log"
/* Arguments handed to the engine.
 *
 * The five the settings can produce, --root and its path, --touch-mode and
 * its value, the terminating NULL -- that is ten, which is exactly what the
 * array used to hold. There was no room for an eleventh, and a per-game
 * ons_args file is nothing but eleventh arguments. */
#define CMD_MAX 32

#define CONFIG_NUM 14  /* ...the library actions, game defaults and logging */

#define PACKAGE_TEMP "ux0:onsemu/temp/"

#define SITTINGS_FILE "sittings.txt"
#define SITTINGS_NUM 6
/* The settings that are a choice rather than a toggle. */
#define SITTINGS_ENCODING 4
/* Touch, per game: 0 follows the launcher's setting, then front, both,
 * back, off.  Per game because the right answer depends on how a game is
 * played -- a game with a lot of tapping wants the front panel, one played
 * with the console propped up wants the back panel left alone. */
#define SITTINGS_TOUCH 5

/* Not settings but actions, on the same screen because this is the screen
 * that is about one game. */
#define SITTINGS_BACKUP 6
#define SITTINGS_RESTORE 7

#define SITTINGS_DEFAULT 8
#define SITTINGS_RETURN 9

#define SCE_CTRL_HOLD 0x80000000
#define FONT_SIZE 24

#define GUI_VERSION 102
#define GUI_VERSION_DATE ""

extern int ICONS_ROW;
extern int ICONS_COL;

extern int LIST_ROW;

extern int SCE_CTRL_ENTER;
extern int SCE_CTRL_CANCEL;
extern char ICON_ENTER[4];
extern char ICON_CANCEL[4];

extern char *confirm_msg;
extern int confirm_msg_width;
extern char *close_msg;
extern int close_msg_width;

typedef enum {
	UNKNOWN = 0,
	MAIN_SCREEN = 1,
	CONFIG_SCREEN,
	RELOAD_MAINSCREEN,
	HELP_MSG,
	/* The same screen's second page: what this build can open. */
	FORMATS_MSG,
	ABOUT_MSG,
	/* Installing a game from a .zip.  These sit before PRINT_APPINFO so the
	 * game list still draws behind them but the per-game info panel, which
	 * has no meaning for an archive, does not. */
	INSTALL_CONFIRM,
	INSTALL_RUN,
	INSTALL_DONE,
	INSTALL_FAIL,
	PRINT_APPINFO,
	START_MODE,
	SETTING_MODE,
	/* The report after copying a game's saves; drawn over the settings
	 * screen it was started from. */
	SAVES_DONE,
	DELETE_MODE,
	SHORTCUT_MODE,
	SHORTCUT_WAIT,
	SHORTCUT_DONE_MODE,
	SHORTCUT_FAIL_MODE,
	/* Fetching a cover from vndb: ask, run, then report. */
	COVER_CONFIRM,
	COVER_RUN,
	COVER_DONE,
	/* The same thing for the whole library, from the settings screen. */
	/* Clearing temporary files, also from the settings screen. */
	CLEAN_CONFIRM,
	CLEAN_DONE,
	COVERS_ALL_CONFIRM,
	COVERS_ALL_RUN,
	COVERS_ALL_DONE,
	/* Removing an installed game, or an archive. */
	DELETE_RUN,
	DELETE_DONE,
	/* Finding a game in a long list. */
	SEARCH_OPEN,
	SEARCH_CLEAR,
} ScreenState;
typedef enum {
	USE_ICON = 1,
	USE_LIST,
} DrawListMode;

#endif // __GUI_MAIN_H__
