/* -*- C++ -*-
 *
 *  onscripter_main.cpp -- main function of ONScripter
 *
 *  Copyright (c) 2001-2017 Ogapee. All rights reserved.
 *            (C) 2014-2017 jh10001 <jh10001@live.cn>
 *
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h> 
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/ime_dialog.h>
#include <stdio.h>
#include <string.h>
#include <vita2d.h>
#include <string>
#include <vector>
#include <iostream>

#include "vitaPackage.h"
#include "filesystem.h"
#include "ZipHandler.h"
#include "VndbCovers.h"

#include "GUI_common.h"
#include "GUI_Theme.h"
#include "build_version.h"
#include "GUI_Text.h"
#include "GUI_Utils.h"
#include "version.h"
#include "iniparser.h"

using namespace std;
// must use app0:xxx.bin
#define ONSJH_PATH "app0:onsjh.bin"

extern unsigned char _binary_res_logo_png_start;
extern unsigned char _binary_res_logo_png_size;

int _newlib_heap_size_user = 192 * 1024 * 1024;

extern int ons_main(int argc, char *argv[]);
//-----------------------------------
int select_row = 0;
int select_col = 0;
int select_appinfo_button = 0;
int select_slot = 0;
int select_config = 0;
int g_choose = -1;
vita2d_font* font;
/* The interface text lives in GUI_Text.cpp, in every language the launcher
 * speaks; these keep the old names so the drawing code reads the same. */
#define about_msg ui_text(UI_ABOUT)

int game_start_select = -1;
string startup_cmd;
int cmd_default[] = { 0,1,0,1,0,0,0,0,0,0 };
int cmd[10] = {0};
int cmd_num = 0;
char *cmd_str[CMD_MAX];
string sittings[SLOT_BUTTON];

/* Filled at startup, once the language is known. */
static void init_sittings_text()
{
	sittings[0] = ui_text(UI_SET_FULLSCREEN);
	sittings[1] = ui_text(UI_SET_FONTCACHE);
	sittings[2] = ui_text(UI_SET_TEXTSHADOW);
	sittings[3] = ui_text(UI_SET_TEXTBOX);
	sittings[4] = ui_text(UI_SET_ENCODING);
	sittings[5] = "";
	sittings[6] = "";
	sittings[7] = "";
	sittings[SITTINGS_DEFAULT] = ui_text(UI_SET_RESET);
	sittings[SITTINGS_RETURN]  = ui_text(UI_SET_RETURN);
}

DrawListMode mainscreen_list_mode;

/* Advances one setting to its next value.  Everything is a plain on/off
 * toggle except the encoding, which cycles auto -> sjis -> gbk. */
static void cycle_sitting(int slot)
{
	if (slot == SITTINGS_ENCODING)
		cmd[slot] = (cmd[slot] + 1) % 3;
	else
		cmd[slot] = !cmd[slot];
}

/* One game in the grid: its cover filling a card, with the name on a band
 * across the bottom.  The old grid drew the image letterboxed inside its box
 * and no name at all, which for a shelf of games is the wrong way round. */
/* Where the selection ring is, and where it is heading.
 *
 * The ring is not drawn by the card that owns it: the cards would overdraw
 * each other's rings, and the ring has to move between them rather than
 * jump.  Each card that is selected leaves its rectangle here, and the ring
 * is drawn after the grid, having moved part of the way there. */
static float ring_x, ring_y, ring_w, ring_h;
static int   ring_target_x, ring_target_y, ring_target_w, ring_target_h;
static bool  ring_visible = false;
static bool  ring_settled = false;

static void ring_aim(int x, int y, int w, int h) {
	ring_target_x = x; ring_target_y = y;
	ring_target_w = w; ring_target_h = h;
	ring_visible = true;
}

/* Called once per frame, after the cards are drawn. */
static void ring_draw() {
	if (!ring_visible) return;

	if (!ring_settled) {
		/* First appearance lands where it belongs rather than flying in
		 * from the corner it was left at. */
		ring_x = ring_target_x; ring_y = ring_target_y;
		ring_w = ring_target_w; ring_h = ring_target_h;
		ring_settled = true;
	}
	else {
		const float rate = 0.38f;
		ring_x = th_ease(ring_x, (float)ring_target_x, rate);
		ring_y = th_ease(ring_y, (float)ring_target_y, rate);
		ring_w = th_ease(ring_w, (float)ring_target_w, rate);
		ring_h = th_ease(ring_h, (float)ring_target_h, rate);
	}

	th_focus((int)ring_x, (int)ring_y, (int)ring_w, (int)ring_h);
}

void draw_icon(int curr, int row, int col) {
	const int x = ICON_LEFT(col);
	const int y = ICON_TOP(row);
	const int w = ICON_WIDTH;
	const int h = ICON_HEIGHT;
	const bool selected = (row == select_row && col == select_col);

	rom_list[curr].touch_area.left = x;
	rom_list[curr].touch_area.top = y;
	rom_list[curr].touch_area.right = x + w;
	rom_list[curr].touch_area.bottom = y + h;

	th_shadow(x, y, w, h);
	th_card(x, y, w, h, TH_SURFACE, TH_BG);

	th_cover(rom_list[curr].icon, rom_list[curr].w, rom_list[curr].h,
		 x, y, w, h);

	/* A band the name can be read on whatever the cover is doing behind
	 * it, taller than the text so nothing sits against the edge. */
	const int band = TH_FONT_S + 10;
	if (h > band * 2) {
		vita2d_draw_rectangle(x, y + h - band, w, band, TH_CAPTION);
		th_text(x + 6, y + h - band + TH_FONT_S + 1,
			selected ? TH_TEXT : TH_TEXT_DIM, TH_FONT_S,
			th_fit(rom_list[curr].char_name(), TH_FONT_S, w - 12));
	}

	/* What this row is, if it is not simply a game: an archive waiting to
	 * be installed, or an install that stopped part way.  Either way the
	 * player finds out here rather than by opening it. */
	const char *tag = rom_list[curr].is_zip ? ".zip"
			: (rom_list[curr].is_partial ? ui_text(UI_UNFINISHED) : NULL);
	if (tag) {
		const int tag_w = vita2d_font_text_width(font, TH_FONT_S, tag) + 12;
		vita2d_draw_rectangle(x + w - tag_w - 6, y + 6, tag_w, TH_FONT_S + 6,
			rom_list[curr].is_partial ? TH_DANGER : TH_ACCENT);
		th_text_center(x + w - tag_w - 6, tag_w, y + TH_FONT_S + 7,
			TH_BG, TH_FONT_S, tag);
	}

	if (selected) ring_aim(x, y, w, h);
}

void draw_icons(int curr) {
	vita2d_draw_rectangle(ITEMS_PANEL_LEFT, ITEMS_PANEL_TOP,
		ITEMS_PANEL_WIDTH, ITEMS_PANEL_HEIGHT, TH_BG);

	ring_visible = false;
	for (int i = 0; i + curr < rom_list.size() && i < (ICONS_COL * ICONS_ROW); i++) {
		draw_icon(i + curr, i / ICONS_COL, i % ICONS_COL);
	}
	ring_draw();
}

/* One game as a row: thumbnail, name, and where it lives underneath in the
 * quiet weight.  The selected row is a lighter surface with an accent edge,
 * rather than an inverted block. */
void draw_list_row(int curr, int row) {
	const int x = LIST_LEFT;
	const int y = LIST_TOP(row);
	const int w = LIST_WIDTH;
	const int h = LIST_HEIGHT;
	const bool selected = (row == select_row);

	if (selected) g_choose = curr;

	rom_list[curr].touch_area.left = x;
	rom_list[curr].touch_area.top = y;
	rom_list[curr].touch_area.right = x + w;
	rom_list[curr].touch_area.bottom = y + h;

	th_card(x, y, w, h, selected ? TH_SURFACE_HI : TH_SURFACE, TH_BG);
	if (selected) ring_aim(x, y, w, h);

	/* A square of cover at the left, cropped to fill it. */
	const int thumb = h - 8;
	th_cover(rom_list[curr].icon, rom_list[curr].w, rom_list[curr].h,
		 x + 8, y + 4, thumb, thumb);

	const int text_left = x + 8 + thumb + TH_PAD;
	const int text_width = w - (text_left - x) - TH_PAD - 90;

	th_text(text_left, y + (h / 2) - 2,
		selected ? TH_TEXT : TH_TEXT_DIM, TH_FONT_M,
		th_fit(rom_list[curr].char_name(), TH_FONT_M, text_width));
	th_text(text_left, y + (h / 2) + TH_FONT_S + 2, TH_TEXT_FAINT, TH_FONT_S,
		th_fit(rom_list[curr].char_path(), TH_FONT_S, text_width));

	if (rom_list[curr].is_zip) {
		th_text_right(x + w - TH_PAD, y + (h / 2) + 2, TH_ACCENT, TH_FONT_S,
			".zip");
	}
	else if (rom_list[curr].is_partial) {
		th_text_right(x + w - TH_PAD, y + (h / 2) + 2, TH_DANGER, TH_FONT_S,
			ui_text(UI_UNFINISHED));
	}
}

void draw_list(int curr) {
	// __________tm_bat
	// |__|___________|
	// |__|___________|
	// |__|___________|
	// |__|___________|
	// ------helps-----
	vita2d_draw_rectangle(ITEMS_PANEL_LEFT, ITEMS_PANEL_TOP,
		ITEMS_PANEL_WIDTH, ITEMS_PANEL_HEIGHT, TH_BG);

	ring_visible = false;
	for (int i = 0; i + curr < rom_list.size() && i < LIST_ROW; i++) {
		draw_list_row(i + curr, i);
	}
	ring_draw();
}

void draw_title() {
	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TH_SURFACE);
	/* One hairline under the header instead of a hard edge. */
	vita2d_draw_rectangle(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, 1, TH_LINE);

	th_text(TH_PAD, 27, TH_TEXT, TH_FONT_M, "ONS Easy Setup");

	/* The build identifies itself quietly, in the dim weight: it matters
	 * when something is wrong and should not compete with the library the
	 * rest of the time. */
	char build[64];
	snprintf(build, sizeof(build), "%s  %s", ONS_BUILD_VERSION, ONS_BUILD_COMMIT);
	int name_w = vita2d_font_text_width(font, TH_FONT_M, "ONS Easy Setup");
	th_text(TH_PAD + name_w + TH_GAP, 27, TH_TEXT_FAINT, TH_FONT_S, build);

	char time_str[16];
	SceDateTime time;
	sceRtcGetCurrentClock(&time, 0);
	getTimeString(time_str, 24, &time);
	th_text_right(SCREEN_WIDTH - TH_PAD, 27, TH_TEXT_DIM, TH_FONT_M, time_str);

	/* What is left on the card, beside the clock.
	 *
	 * Installing a game is the one thing here that can run the card out,
	 * and finding that out from a failed extraction is finding out too
	 * late.  Asked for every couple of seconds rather than every frame:
	 * it is a syscall, and the number does not move that fast. */
	static char free_str[16] = { '\0' };
	static int free_countdown = 0;
	if (free_countdown <= 0) {
		getSizeString(free_str, ZipHandler::freeSpace());
		free_countdown = 120;
	}
	free_countdown--;

	char free_line[32];
	snprintf(free_line, sizeof(free_line), ui_text(UI_FREE_SPACE), free_str);
	th_text_right(SCREEN_WIDTH - TH_PAD -
		      vita2d_font_text_width(font, TH_FONT_M, time_str) - TH_PAD,
		      27, TH_TEXT_FAINT, TH_FONT_S, free_line);
}

void draw_help() {
	vita2d_draw_rectangle(0, FOOTER_TOP, SCREEN_WIDTH, FOOTER_HEIGHT, TH_SURFACE);
	vita2d_draw_rectangle(0, FOOTER_TOP, SCREEN_WIDTH, 1, TH_LINE);

	const int baseline = FOOTER_TOP + 21;

	if (rom_list.size() == 0) {
		if (!rom_search.empty()) {
			static char empty[160];
			snprintf(empty, sizeof(empty), ui_text(UI_SEARCH_EMPTY),
				 rom_search.c_str(), ICON_CANCEL);
			th_text(TH_PAD, baseline, TH_TEXT_DIM, TH_FONT_S,
				th_fit(empty, TH_FONT_S, SCREEN_WIDTH - TH_PAD * 2));
			return;
		}
		th_text(TH_PAD, baseline, TH_TEXT_DIM, TH_FONT_S,
			"No games yet -- put a folder or a .zip in ux0:onsemu/");
		return;
	}

	/* Icon, word, gap.  A chip of the button's letters stands in for any
	 * icon that is not in the vpk, so a build without them still reads. */
	int x = TH_PAD;
	x += th_button(x, baseline, th_glyph_l, "L", TH_FONT_S) + 5;
	x += th_hint(x, baseline, NULL, ui_text(UI_HINT_SETTINGS), TH_TEXT_DIM,
		     TH_FONT_S) + TH_PAD;
	x += th_button(x, baseline, th_glyph_r, "R", TH_FONT_S) + 5;
	x += th_hint(x, baseline, NULL, ui_text(UI_HINT_HELP), TH_TEXT_DIM,
		     TH_FONT_S) + TH_PAD;
	x += th_button(x, baseline, th_glyph_select, "SELECT", TH_FONT_S) + 5;
	x += th_hint(x, baseline, NULL, ui_text(UI_HINT_ABOUT), TH_TEXT_DIM,
		     TH_FONT_S) + TH_PAD;
	x += th_button(x, baseline, th_glyph_enter, "O", TH_FONT_S) + 5;
	x += th_hint(x, baseline, NULL, ui_text(UI_BTN_START), TH_TEXT_DIM,
		     TH_FONT_S);

	static char position[64];
	snprintf(position, sizeof(position), ui_text(UI_FOOTER_HINTS),
		g_choose + 1, (int)rom_list.size());
	int right = SCREEN_WIDTH - TH_PAD;
	th_text_right(right, baseline, TH_TEXT, TH_FONT_S, position);

	/* A filtered list looks like a short list unless it says otherwise. */
	if (!rom_search.empty()) {
		char active[80];
		snprintf(active, sizeof(active), ui_text(UI_SEARCH_ACTIVE),
			 rom_search.c_str());
		right -= vita2d_font_text_width(font, TH_FONT_S, position) + TH_PAD;
		th_text_right(right, baseline, TH_ACCENT, TH_FONT_S,
			th_fit(active, TH_FONT_S, 260));
	}
}

/* Flat, with the accent as the pressed state rather than an inverted box. */
void draw_button(int left, int top, int width, int height, string text, int zoom, int pressed) {
	const char *label = RomInfo::to_char(text);

	th_card(left, top, width, height,
		pressed ? TH_ACCENT : TH_SURFACE_HI, TH_SURFACE);
	if (!pressed) th_border(left, top, width, height, 1, TH_LINE);

	int text_height = vita2d_font_text_height(font, zoom, label);
	th_text_center(left, width, top + (height + text_height) / 2 - 2,
		pressed ? TH_BG : TH_TEXT, zoom,
		th_fit(label, zoom, width - TH_PAD * 2));
}

struct config_item {
	const char *name;
	const char *value;
};

/* How far the layer over the library has arrived: 0 when it has just been
 * opened, 1 once it has settled.  A panel that appears fully formed reads as
 * a screen swap; one that fades up and lifts the last few pixels reads as
 * something laid on top of what is still there. */
static float layer_anim = 0.0f;
static ScreenState layer_last = UNKNOWN;

static bool state_is_layer(ScreenState state) {
	return state >= PRINT_APPINFO || state == CONFIG_SCREEN ||
	       state == HELP_MSG || state == ABOUT_MSG;
}

/* Defined with the install code further down, which is where the number it
 * reports is learned; the game panel needs it up here. */
static int estimated_install_seconds(const std::string &zip_path);

/* Where the last dialog was drawn, so a tap can be tested against it.  The
 * box is sized from its text every frame, so recording it keeps the touch
 * area and the drawing from drifting apart. */
static rectangle last_dialog_area = { 0, 0, 0, 0 };

/* Where draw_config() last put its rows.  The panel is centred and sized
 * from the number of rows, so the touch handling reads the geometry back
 * rather than recomputing it and drifting. */
static int config_rows_top = 0;
static int config_row_height = 44;
static int config_panel_left = 0;
static int config_panel_width = SCREEN_WIDTH;
static int config_panel_top = HEADER_HEIGHT;
static int config_panel_height = ITEMS_PANEL_HEIGHT;

void draw_config() {
	struct config_item items[] = {
		{ui_text(UI_CFG_GRAPHIC_MODE),
			strcmp(config.list_mode, "icon") ? ui_text(UI_CFG_LIST) : ui_text(UI_CFG_ICON)},
		{ui_text(UI_CFG_ICON_ROW),   RomInfo::to_char(config.icon_row)},
		{ui_text(UI_CFG_ICON_COL),   RomInfo::to_char(config.icon_col)},
		{ui_text(UI_CFG_LIST_ROW),   RomInfo::to_char(config.list_row)},
		{ui_text(UI_CFG_TOUCH_MODE),
			config.use_btouch == 0 ? ui_text(UI_TOUCH_OFF)
				: (config.use_btouch == 1 ? ui_text(UI_TOUCH_FRONT) : ui_text(UI_TOUCH_BOTH))},
		{ui_text(UI_CFG_SORT),
			config.sort_mode == SORT_RECENT ? ui_text(UI_SORT_RECENT)
				: (config.sort_mode == SORT_SIZE ? ui_text(UI_SORT_SIZE)
								 : ui_text(UI_SORT_NAME))},
		{ui_text(UI_CFG_LANGUAGE),
			config.language == UI_LANG_ZH ? "\xE4\xB8\xAD\xE6\x96\x87" : "English"},
		/* Not a setting but an action, and this is where a player looks for
		 * something that applies to the whole library rather than to the
		 * game under the cursor. */
		{ui_text(UI_CFG_FETCH_COVERS), ui_text(UI_COVERS_START)}
	};

	/* The settings sit on a card over a dimmed library rather than over a
	 * black panel, so it is clear they are a layer on top of it. */
	vita2d_draw_rectangle(0, HEADER_HEIGHT, SCREEN_WIDTH, ITEMS_PANEL_HEIGHT,
		th_alpha(TH_SCRIM, layer_anim));

	const int panel_w = 560;
	const int row_h   = 44;
	const int panel_h = CONFIG_NUM * row_h + TH_PAD * 2 + 34;
	const int panel_x = (SCREEN_WIDTH - panel_w) / 2;
	/* Settled position, which is what the touch handling is told about;
	 * the drawing is offset while it arrives so a tap during those few
	 * frames still lands on the row it looks like it landed on. */
	const int settled_y = HEADER_HEIGHT + (ITEMS_PANEL_HEIGHT - panel_h) / 2;
	const int panel_y = settled_y + (int)((1.0f - layer_anim) * 14.0f);

	th_shadow(panel_x, panel_y, panel_w, panel_h);
	th_card(panel_x, panel_y, panel_w, panel_h, TH_SURFACE, TH_BG);
	th_border(panel_x, panel_y, panel_w, panel_h, 1, TH_LINE);

	th_text(panel_x + TH_PAD, panel_y + TH_PAD + TH_FONT_M - 2, TH_TEXT,
		TH_FONT_M, ui_text(UI_BTN_CONFIG));

	const int first_row = panel_y + TH_PAD + 34;

	for (int i = 0; i < CONFIG_NUM; i++) {
		const int y = first_row + i * row_h;

		/* Rows that do not apply to the current list style are still shown,
		 * so the settings do not change shape as you move, but they are
		 * clearly not in play. */
		bool disabled = false;
		if (!strcmp(config.list_mode, "list")) disabled = (i == 1 || i == 2);
		else                                   disabled = (i == 3);

		if (i == select_config) {
			th_card(panel_x + 6, y, panel_w - 12, row_h - 4, TH_SURFACE_HI,
				TH_SURFACE);
			vita2d_draw_rectangle(panel_x + 6, y, TH_RING, row_h - 4, TH_ACCENT);
		}

		unsigned int label_color = disabled ? TH_TEXT_FAINT
					 : (i == select_config ? TH_TEXT : TH_TEXT_DIM);
		unsigned int value_color = disabled ? TH_TEXT_FAINT : TH_ACCENT;

		th_text(panel_x + TH_PAD + 8, y + (row_h - 4) / 2 + 7, label_color,
			TH_FONT_S, items[i].name);
		th_text_right(panel_x + panel_w - TH_PAD - 8, y + (row_h - 4) / 2 + 7,
			value_color, TH_FONT_S, items[i].value);
	}

	/* Where the rows are, for the touch handling. */
	config_rows_top = first_row + (settled_y - panel_y);
	config_row_height = row_h;
	config_panel_left = panel_x;
	config_panel_width = panel_w;
	config_panel_top = settled_y;
	config_panel_height = panel_h;
}

void draw_appinfo_icon(int curr) {
	/* The cover, large and cropped to fill, is the subject of this panel. */
	th_shadow(APPINFO_ICON_LEFT, APPINFO_ICON_TOP,
		APPINFO_ICON_WIDTH, APPINFO_ICON_HEIGHT);
	th_card(APPINFO_ICON_LEFT, APPINFO_ICON_TOP,
		APPINFO_ICON_WIDTH, APPINFO_ICON_HEIGHT, TH_SURFACE_HI, TH_SURFACE);
	th_cover(rom_list[curr].icon, rom_list[curr].w, rom_list[curr].h,
		APPINFO_ICON_LEFT, APPINFO_ICON_TOP,
		APPINFO_ICON_WIDTH, APPINFO_ICON_HEIGHT);
}

void draw_appinfo(ScreenState state, int choose) {
	/* The panel sits over a dimmed library, like the settings do.  It fades
	 * rather than lifting: its buttons are placed by the layout macros, and
	 * the touch areas are the same macros, so moving the drawing would put
	 * the two out of step for as long as it took to arrive. */
	vita2d_draw_rectangle(0, HEADER_HEIGHT, SCREEN_WIDTH, ITEMS_PANEL_HEIGHT,
		th_alpha(TH_SCRIM, layer_anim));

	th_card(APPINFO_PANEL_LEFT, APPINFO_PANEL_TOP,
		APPINFO_PANEL_WIDTH, APPINFO_PANEL_HEIGHT, TH_SURFACE, TH_BG);
	th_border(APPINFO_PANEL_LEFT, APPINFO_PANEL_TOP,
		APPINFO_PANEL_WIDTH, APPINFO_PANEL_HEIGHT, 1, TH_LINE);

	draw_appinfo_icon(choose);

	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(0),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		RomInfo::to_char(ui_text(UI_BTN_START)), TH_FONT_S,
		(state == START_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 0));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(1),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		RomInfo::to_char(ui_text(UI_BTN_CONFIG)), TH_FONT_S,
		(state == SETTING_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 1));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(2),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		RomInfo::to_char(ui_text(UI_BTN_INSTALL)), TH_FONT_S,
		(state == SHORTCUT_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 2));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(3),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		RomInfo::to_char(ui_text(UI_BTN_DELETE)), TH_FONT_S,
		(state == DELETE_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 3));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(4),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		RomInfo::to_char(ui_text(UI_BTN_COVER)), TH_FONT_S,
		(state == COVER_CONFIRM) ||
		(state == PRINT_APPINFO && select_appinfo_button == 4));

	/* The name, then the facts about the folder in the quiet weight.  The
	 * old panel put all of it in one sprintf with labels on every line;
	 * what a player wants first is which game this is. */
	static char detail_str[512];
	static char history_str[128];
	static int old_choose = -1;
	if (choose != old_choose) {
		old_choose = choose;
		char size_str[16];

		if (rom_list[choose].is_zip) {
			/* An archive has nothing installed to measure, so the panel
			 * says what it is and what it will cost: the archive's own
			 * size, what it unpacks to, and -- once there is anything to
			 * base it on -- how long that is likely to take. */
			char unpacked_str[16];
			getSizeString(size_str, rom_list[choose].size);
			getSizeString(unpacked_str,
				      ZipHandler::installedSize(rom_list[choose].path));
			snprintf(detail_str, sizeof(detail_str), ui_text(UI_ZIP_INFO),
				 size_str, unpacked_str);

			int seconds = estimated_install_seconds(rom_list[choose].path);
			if (seconds > 0)
				snprintf(history_str, sizeof(history_str),
					 ui_text(UI_ZIP_INFO_TIME), seconds / 60, seconds % 60);
			else
				history_str[0] = '\0';
		}
		else {
			uint64_t size = 0;
			uint32_t file_num = 0, floder_num = 0;
			getPathInfo(rom_list[choose].char_path(), &size, &floder_num, &file_num);
			getSizeString(size_str, size);
			snprintf(detail_str, sizeof(detail_str), "%s   %d files, %d folders",
				size_str, file_num, floder_num);

			if (rom_list[choose].last_date.empty())
				snprintf(history_str, sizeof(history_str), "%s",
					 ui_text(UI_NEVER_PLAYED));
			else
				snprintf(history_str, sizeof(history_str),
					 ui_text(UI_LAST_PLAYED),
					 rom_list[choose].last_date.c_str());
		}
	}

	const int text_left = APPINFO_DESC_LEFT;
	const int text_width = APPINFO_DESC_WIDTH;

	th_text(text_left, APPINFO_DESC_TOP + TH_FONT_L, TH_TEXT, TH_FONT_L,
		th_fit(rom_list[choose].char_name(), TH_FONT_L, text_width));
	th_text(text_left, APPINFO_DESC_TOP + TH_FONT_L + 26, TH_TEXT_DIM,
		TH_FONT_S, th_fit(rom_list[choose].char_path(), TH_FONT_S, text_width));
	th_text(text_left, APPINFO_DESC_TOP + TH_FONT_L + 50, TH_TEXT_FAINT,
		TH_FONT_S, detail_str);
	th_text(text_left, APPINFO_DESC_TOP + TH_FONT_L + 72, TH_TEXT_FAINT,
		TH_FONT_S, history_str);
}

void draw_slots(int index_, int slot) {
	// __________tm_bat
	// |name |=======|
	// |...  |=======|
	// |...  |..     |
	// |_____|_______|
	// ------helps-----

	// .--------------------------------------
	// | icon  | backup   | slot0
	// |       | restore  | slot1
	// |       | format   |
	// |       | ...      |  ..
	// |------------------|
	// | title id         |
	// | title            |
	// | cart /dl         |
	// | save position    |
	// | ...              | slot9
	// '---------------------------------------
	th_card(SLOT_PANEL_LEFT, SLOT_PANEL_TOP,
		SLOT_PANEL_WIDTH, SLOT_PANEL_HEIGHT, TH_SURFACE, TH_BG);
	th_border(SLOT_PANEL_LEFT, SLOT_PANEL_TOP,
		SLOT_PANEL_WIDTH, SLOT_PANEL_HEIGHT, 1, TH_LINE);

	for (int i = 0; i < SLOT_BUTTON; i++) {
		string tmp = sittings[i];
		if (i < SITTINGS_NUM) {
			while (tmp.length() < 24)
				tmp += " ";
			tmp += "[";
			if (i == SITTINGS_ENCODING) {
				/* Three states, not on/off: the engine guesses by
				 * default, and the other two force a code page for
				 * the rare script it cannot tell apart. */
				tmp += cmd[i] == 1 ? ui_text(UI_ENC_SJIS)
					: (cmd[i] == 2 ? ui_text(UI_ENC_GBK) : ui_text(UI_AUTO));
				tmp += "]";
			}
			else {
				tmp += cmd[i] ? ui_text(UI_ON) : ui_text(UI_OFF);
				tmp += "]";
				if (cmd[i])
					tmp += "●";
				else
					tmp += "○";
			}
		}
		draw_button(SLOT_BUTTON_LEFT, SLOT_BUTTON_TOP(i),
			SLOT_BUTTON_WIDTH, SLOT_BUTTON_HEIGHT,
			tmp, TH_FONT_S,
			(slot == i) || (slot < 0 && select_slot == i));
	}
	/*if (sittings) {
		free(sittings);
	}*/
}

/* Dialogs: a dark card over a dimmed screen, with the two answers written
 * where the box splits for touch -- cancel on the left, confirm on the
 * right, which is also the order the buttons are listed in. */
static void dialog_box(const char *msg, const char *no_label,
                       const char *yes_label, int fontsize)
{
	int text_width  = vita2d_font_text_width(font, fontsize, (char *)msg);
	int text_height = vita2d_font_text_height(font, fontsize, (char *)msg);

	const int padding = 28;
	int width  = text_width + padding * 2;
	int height = text_height + padding * 2 + 34;
	if (width  > SCREEN_WIDTH - 80)  width  = SCREEN_WIDTH - 80;
	if (height > SCREEN_HEIGHT - 80) height = SCREEN_HEIGHT - 80;

	const int left = (SCREEN_WIDTH - width) / 2;
	const int top  = (SCREEN_HEIGHT - height) / 2;

	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TH_SCRIM);
	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);

	last_dialog_area.left = left;
	last_dialog_area.top = top;
	last_dialog_area.right = left + width;
	last_dialog_area.bottom = top + height;

	vita2d_font_draw_text(font, left + padding, top + padding + fontsize,
		TH_TEXT, fontsize, (char *)msg);

	vita2d_draw_rectangle(left + padding, top + height - 44,
		width - padding * 2, 1, TH_LINE);

	/* The answers, as the buttons that give them.  Laid out from their
	 * measured width so the pair sits centred whatever the words are. */
	const int baseline = top + height - padding + 6;
	int total = th_hint_width(th_glyph_cancel, no_label, TH_FONT_S);
	if (yes_label) total += TH_PAD * 2 +
		th_hint_width(th_glyph_enter, yes_label, TH_FONT_S);

	int x = left + (width - total) / 2;
	x += th_hint(x, baseline, th_glyph_cancel, no_label, TH_TEXT_DIM,
		     TH_FONT_S);
	if (yes_label) {
		x += TH_PAD * 2;
		th_hint(x, baseline, th_glyph_enter, yes_label, TH_TEXT, TH_FONT_S);
	}
}

/* The controls, as a table: the button in the first column and what it does
 * in the second.  It used to be one string with the buttons written as
 * box-drawing characters and the columns lined up with spaces, which in a
 * proportional font lines up nothing. */
void draw_help_screen() {
	struct help_row {
		vita2d_texture **glyph;   /* a face button, drawn from its icon */
		const char *chip;         /* a named button, drawn as a chip */
		const char *text;         /* or plain words, for the sticks */
		UIStringId description;
	};
	const help_row rows[] = {
		{ &th_glyph_circle,   "O",      NULL,     UI_HELP_CONFIRM },
		{ &th_glyph_cross,    "X",      NULL,     UI_HELP_SKIP },
		{ &th_glyph_square,   "[]",     NULL,     UI_HELP_AUTO },
		{ &th_glyph_triangle, "/\\",    NULL,     UI_HELP_MENU },
		{ &th_glyph_l,        "L",      NULL,     UI_HELP_SKIP_PAGE },
		{ &th_glyph_r,        "R",      NULL,     UI_HELP_TOGGLE_SKIP },
		{ &th_glyph_dpad,     NULL,     NULL,     UI_HELP_BACKLOG },
		{ &th_glyph_dpad,     NULL,     NULL,     UI_HELP_CURSOR },
		{ &th_glyph_lstick,   NULL,     NULL,     UI_HELP_STICK },
		{ &th_glyph_select,   "SELECT", "hold",   UI_HELP_OVERLAY },
	};
	const int count = (int)(sizeof(rows) / sizeof(rows[0]));

	const int row_h  = 30;
	const int col_w  = 150;   /* the button column, wide enough for the words */
	const int padding = 28;

	/* Wide enough for the longest description, so nothing is trimmed. */
	int desc_w = 0;
	for (int i = 0; i < count; i++) {
		int w = vita2d_font_text_width(font, TH_FONT_S,
			ui_text(rows[i].description));
		if (w > desc_w) desc_w = w;
	}

	const int width  = padding * 2 + col_w + desc_w;
	const int height = padding * 2 + 40 + count * row_h + 34;
	const int left   = (SCREEN_WIDTH - width) / 2;
	const int top    = (SCREEN_HEIGHT - height) / 2;

	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TH_SCRIM);
	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);

	last_dialog_area.left = left;
	last_dialog_area.top = top;
	last_dialog_area.right = left + width;
	last_dialog_area.bottom = top + height;

	th_text(left + padding, top + padding + TH_FONT_M, TH_TEXT, TH_FONT_M,
		ui_text(UI_HELP_TITLE));

	for (int i = 0; i < count; i++) {
		const int baseline = top + padding + 40 + (i + 1) * row_h;

		int cx = left + padding;
		/* "hold" first where a row has it, so it reads as a sentence. */
		if (rows[i].text) {
			th_text(cx, baseline, TH_TEXT_DIM, TH_FONT_S, rows[i].text);
			cx += vita2d_font_text_width(font, TH_FONT_S, rows[i].text) + 6;
		}
		th_button(cx, baseline, rows[i].glyph ? *rows[i].glyph : NULL,
			  rows[i].chip, TH_FONT_M);

		th_text(left + padding + col_w, baseline, TH_TEXT_DIM, TH_FONT_S,
			ui_text(rows[i].description));
	}

	vita2d_draw_rectangle(left + padding, top + height - 44,
		width - padding * 2, 1, TH_LINE);
	int close_w = th_hint_width(th_glyph_enter, ui_text(UI_PROMPT_CLOSE),
				    TH_FONT_S);
	th_hint(left + (width - close_w) / 2, top + height - padding + 6,
		th_glyph_enter, ui_text(UI_PROMPT_CLOSE), TH_TEXT_DIM, TH_FONT_S);
}

void draw_message(char *msg, int choose, int fontsize) {
	/* Two answers: cancel on the left, confirm on the right -- the order
	 * the touch handling splits the box in. */
	dialog_box(msg, ui_text(UI_PROMPT_NO), ui_text(UI_PROMPT_YES), fontsize);
}

void draw_alert(char *msg, int fontsize) {
	dialog_box(msg, ui_text(UI_PROMPT_CLOSE), NULL, fontsize);
}

/*
 * Installing a game from a .zip.
 *
 * The extraction runs on this thread and repaints from its progress
 * callback, so the bar moves and CIRCLE stays responsive without the
 * launcher needing a worker thread.
 */
/* What the last cover fetch had to say, shown by COVER_DONE. */
static char cover_result_message[256] = { '\0' };

static ZipInstallProgress install_progress;
static ZipInstallStatus  install_status = ZIP_INSTALL_OK;
static char install_message[512];
static char install_confirm_message[512];
/* What the delete prompt says, built when the button is pressed so the
 * folder is measured once rather than every frame, and what it said
 * afterwards. */
static char delete_confirm_message[512];
static char delete_result_message[256];

void draw_install_progress(const ZipInstallProgress &progress) {
	const int width  = 700;
	const int height = 200;
	const int left   = (SCREEN_WIDTH - width) / 2;
	const int top    = (SCREEN_HEIGHT - height) / 2;
	const int padding = 25;

	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TH_SCRIM);
	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);
	th_text(left + padding, top + padding + TH_FONT_M, TH_TEXT, TH_FONT_M,
		ui_text(UI_INSTALLING));

	/* The file currently being written, trimmed to fit the box. */
	char line[96];
	snprintf(line, sizeof(line), "%s", progress.current_file.c_str());
	if (strlen(line) > 60) {
		memmove(line, line + strlen(line) - 60, 61);
		line[0] = line[1] = line[2] = '.';
	}
	th_text(left + padding, top + padding + TH_FONT_M * 2 + 8, TH_TEXT_DIM,
		TH_FONT_S, line);

	/* Progress bar. */
	const int bar_left   = left + padding;
	const int bar_top    = top + height - padding - 60;
	const int bar_width  = width - (padding * 2);
	const int bar_height = 24;
	th_card(bar_left, bar_top, bar_width, bar_height, TH_SURFACE_HI, TH_SURFACE);
	vita2d_draw_rectangle(bar_left + 2, bar_top + 2,
		((bar_width - 4) * progress.percent) / 100, bar_height - 4, TH_ACCENT);

	char done_size[16], total_size[16];
	getSizeString(done_size, progress.bytes_done);
	getSizeString(total_size, progress.bytes_total);
	snprintf(line, sizeof(line), "%d%%  (%s / %s)   %s cancel",
		progress.percent, done_size, total_size, ICON_CANCEL);
	th_text(bar_left, bar_top + bar_height + TH_FONT_S + 6, TH_TEXT_DIM,
		TH_FONT_S, line);
}

/* True to keep going, false to cancel.  Repaints and polls CIRCLE. */
static bool install_progress_callback(const ZipInstallProgress &progress, void *user) {
	static uint64_t last_drawn = 0;

	/* Repainting on every 64KB chunk would spend more time drawing than
	 * extracting; a frame every 512KB keeps the bar lively and cheap. */
	if (progress.bytes_done - last_drawn < 512 * 1024 &&
		progress.bytes_done < progress.bytes_total) {
		return true;
	}
	last_drawn = progress.bytes_done;

	vita2d_start_drawing();
	vita2d_clear_screen();
	draw_title();
	draw_help();
	draw_install_progress(progress);
	vita2d_end_drawing();
	vita2d_wait_rendering_done();
	vita2d_swap_buffers();

	SceCtrlData pad = { 0 };
	sceCtrlPeekBufferPositive(0, &pad, 1);
	if (pad.buttons & SCE_CTRL_CANCEL) return false;
	return true;
}

/* Text for the confirmation shown before an install starts. */
/* How fast the last install actually ran, in kilobytes per second.
 *
 * Rather than guess at a number for the card and the compression, the
 * launcher remembers what the last one managed and estimates from that.  It
 * says nothing at all until there is something to base it on: a made-up
 * figure that turns out to be half the real time is worse than no figure. */
static int install_rate_kbs() {
	dictionary *ini = iniparser_load(CONFIG_FILE);
	if (ini == NULL) return 0;
	int rate = iniparser_getint(ini, "GAME:install_rate_kbs", 0);
	iniparser_freedict(ini);
	return rate;
}

static void remember_install_rate(uint64_t bytes, uint32_t milliseconds) {
	if (bytes == 0 || milliseconds < 500) return;   /* too short to learn from */

	int rate = (int)(bytes / milliseconds);          /* bytes/ms == KB/s */
	if (rate <= 0) return;

	dictionary *ini = iniparser_load(CONFIG_FILE);
	if (ini == NULL) return;

	/* Averaged with what was there, so one unusual archive does not become
	 * the estimate for every archive after it. */
	int previous = iniparser_getint(ini, "GAME:install_rate_kbs", 0);
	if (previous > 0) rate = (previous + rate) / 2;

	char value[16];
	snprintf(value, sizeof(value), "%d", rate);
	iniparser_set(ini, "GAME:install_rate_kbs", value);

	FILE *file = fopen(CONFIG_FILE, "w");
	if (file) {
		iniparser_dump_ini(ini, file);
		fclose(file);
	}
	iniparser_freedict(ini);
}

static int estimated_install_seconds(const std::string &zip_path) {
	const int rate = install_rate_kbs();
	if (rate <= 0) return 0;

	const uint64_t bytes = ZipHandler::installedSize(zip_path);
	if (bytes == 0) return 0;

	return (int)(bytes / 1024 / rate);
}

void prepare_install_confirm(int choose) {
	const std::string &zip_path = rom_list[choose].path;
	uint64_t needed = ZipHandler::installedSize(zip_path);
	uint64_t available = ZipHandler::freeSpace();
	char needed_str[16], free_str[16];

	getSizeString(needed_str, needed);
	getSizeString(free_str, available);

	/* Picking up where a previous attempt stopped is a different question
	 * from installing, and worth asking differently: the answer decides
	 * whether the player waits for the whole archive or the rest of it. */
	const uint64_t already = ZipHandler::resumableBytes(zip_path);
	if (already > 0) {
		char done_str[16];
		getSizeString(done_str, already);
		snprintf(install_confirm_message, sizeof(install_confirm_message),
			ui_text(UI_RESUME_ASK), rom_list[choose].char_name(),
			done_str, needed_str, free_str);
		return;
	}
	snprintf(install_confirm_message, sizeof(install_confirm_message),
		"Install this game?\n\n"
		"  %s\n"
		"  to ux0:onsemu/%s\n\n"
		"  needs %s, %s free",
		rom_list[choose].char_name(),
		ZipHandler::destinationName(zip_path).c_str(),
		needed_str, free_str);
}

/* Runs the install; returns the screen to show next. */
ScreenState run_install(int choose) {
	std::string installed_path;

	install_progress.bytes_done  = 0;
	install_progress.bytes_total = 0;
	install_progress.percent     = 0;
	install_progress.current_file.clear();

	/* Timed, so the next archive can be given an estimate based on what
	 * this card and this console actually managed. */
	const uint64_t started_us = sceKernelGetProcessTimeWide();

	install_status = ZipHandler::install(rom_list[choose].path, installed_path,
		install_progress_callback, NULL);

	if (install_status == ZIP_INSTALL_OK)
		remember_install_rate(install_progress.bytes_done,
			(uint32_t)((sceKernelGetProcessTimeWide() - started_us) / 1000));

	if (install_status == ZIP_INSTALL_OK) {
		snprintf(install_message, sizeof(install_message),
			"Installed to\n%s\n\nThe archive was kept in\n" GAME_ZIP_FOLDER,
			installed_path.c_str());
		return INSTALL_DONE;
	}

	snprintf(install_message, sizeof(install_message), "%s",
		ZipHandler::statusMessage(install_status));
	return INSTALL_FAIL;
}

void draw_screen(ScreenState state, int curr, int choose, int slot) {

	/* Opening a layer restarts it; moving between two layers does not, so
	 * stepping from the game panel into its settings does not blink. */
	bool layered = state_is_layer(state);
	if (!layered) layer_anim = 0.0f;
	else if (!state_is_layer(layer_last)) layer_anim = 0.0f;
	else layer_anim = th_ease(layer_anim * 100.0f, 100.0f, 0.34f) / 100.0f;
	layer_last = state;

	vita2d_start_drawing();
	vita2d_clear_screen();

	if (state >= MAIN_SCREEN) {
		//draw header
		draw_title();
		//draw footer
		draw_help();

		switch (mainscreen_list_mode) {
		case USE_ICON:
			draw_icons(curr);
			break;
		case USE_LIST:
			draw_list(curr);
			break;
		}
	}

	if (state == CONFIG_SCREEN) {
		draw_config();
	}
	if (state == HELP_MSG) {
		draw_help_screen();
	}
	if (state == ABOUT_MSG) {
		draw_alert((char*)about_msg, FONT_SIZE);
	}
	/* The batch fetch is started from the settings screen, so that is what
	 * belongs behind it -- the per-game panel has nothing to do with it,
	 * and these states sit past PRINT_APPINFO in the enum only because they
	 * were added last. */
	bool covers_all = (state == COVERS_ALL_CONFIRM || state == COVERS_ALL_RUN ||
			   state == COVERS_ALL_DONE);
	if (covers_all) {
		draw_config();
	}
	else if (state >= PRINT_APPINFO) {
		draw_appinfo(state, choose);
	}

	switch (state) {
	case START_MODE:
		break;
	case INSTALL_CONFIRM:
		draw_message(install_confirm_message, choose, FONT_SIZE);
		break;
	case INSTALL_RUN:
		draw_install_progress(install_progress);
		break;
	case INSTALL_DONE:
	case INSTALL_FAIL:
		draw_alert(install_message, FONT_SIZE);
		break;
	case SETTING_MODE:
		draw_slots(choose, -1);
		break;
	case DELETE_MODE:
		draw_message(delete_confirm_message, choose, FONT_SIZE);
		break;
	case DELETE_RUN:
		draw_alert((char*)ui_text(UI_DELETE_RUN), FONT_SIZE);
		break;
	case DELETE_DONE:
		draw_alert(delete_result_message, FONT_SIZE);
		break;
	case SHORTCUT_MODE:
		draw_message((char*)ui_text(UI_MAKE_PACKAGE_ASK), choose, FONT_SIZE);
		break;
	case SHORTCUT_WAIT:
		draw_alert((char*)ui_text(UI_MAKE_PACKAGE_RUN), FONT_SIZE);
		break;
	case COVER_CONFIRM:
		draw_message((char*)ui_text(UI_COVER_ASK), choose, FONT_SIZE);
		break;
	case COVER_RUN:
		draw_alert((char*)ui_text(UI_COVER_RUN), FONT_SIZE);
		break;
	case COVER_DONE:
	case COVERS_ALL_DONE:
		draw_alert(cover_result_message, FONT_SIZE);
		break;
	case COVERS_ALL_CONFIRM:
		draw_message((char*)ui_text(UI_COVERS_ALL_ASK), choose, FONT_SIZE);
		break;
	case SHORTCUT_DONE_MODE:
		draw_alert((char*)ui_text(UI_MAKE_PACKAGE_OK), FONT_SIZE);
		break;
	case SHORTCUT_FAIL_MODE:
		draw_alert((char*)ui_text(UI_MAKE_PACKAGE_FAIL), FONT_SIZE);
		break;
	default:
		break;
	}

	/* Anything the system is showing over us -- the keyboard, mostly --
	 * is drawn here, in our frame, or it does not appear at all. */
	vita2d_common_dialog_update();

	vita2d_end_drawing();
	vita2d_wait_rendering_done();
	vita2d_swap_buffers();
}

#define IN_RANGE(start, end, value) (start < value && value < end)
#define IS_TOUCHED(rect, pt) \
    (IN_RANGE(rect.left, rect.right, pt.x) && IN_RANGE(rect.top, rect.bottom, pt.y))

/* Which row is under a point, or -1.  Only the rows on screen: a row that
 * has scrolled away still carries the touch area it last had, and would
 * otherwise answer for a tap somewhere it is no longer drawn. */
static int row_at(const point &p, int curr) {
	int visible = (mainscreen_list_mode == USE_LIST) ? LIST_ROW
						        : (ICONS_ROW * ICONS_COL);
	for (int i = curr; i < (int)rom_list.size() && i < curr + visible; i++)
		if (IS_TOUCHED(rom_list[i].touch_area, p))
			return i;
	return -1;
}

/* The touch half of the main screen: a tap opens the game, holding it opens
 * that game's settings.
 *
 * Both gestures start the same way, so neither can be decided on the way
 * down -- the hold fires at half a second, and the tap on release before
 * then.  A finger that wanders more than a thumb's width is neither, and is
 * dropped: on a screen this size, sliding off the row you meant is the
 * ordinary case, not a rare one.
 *
 * It reads no buttons: the dispatcher below has already done that, and
 * read_buttons only reports a press once. */
ScreenState mainscreen_touch(int curr, int &touched) {
	const uint64_t HOLD_US = 500 * 1000;
	const int      SLIP    = 24;          /* pixels before it is a drag */

	static bool     down = false;
	static bool     spent = false;        /* the hold already fired */
	static uint64_t began_us = 0;
	static point    began_at = { 0, 0 };
	static int      began_row = -1;

	point p;
	const bool touching = read_touch_raw(&p) != 0;

	if (touching && !down) {
		down = true;
		spent = false;
		began_us = sceKernelGetProcessTimeWide();
		began_at = p;
		began_row = row_at(p, curr);
		return UNKNOWN;
	}

	if (touching && down) {
		if (spent) return UNKNOWN;

		const int dx = p.x - began_at.x, dy = p.y - began_at.y;
		if (dx * dx + dy * dy > SLIP * SLIP) {
			spent = true;              /* a drag, not a press */
			return UNKNOWN;
		}

		if (began_row >= 0 && !rom_list[began_row].is_zip &&
		    sceKernelGetProcessTimeWide() - began_us >= HOLD_US) {
			spent = true;
			touched = began_row;
			select_appinfo_button = 0;
			return SETTING_MODE;       /* the options for that game */
		}
		return UNKNOWN;
	}

	/* Released. */
	down = false;
	if (spent || began_row < 0) return UNKNOWN;

	touched = began_row;
	select_appinfo_button = 0;
	began_row = -1;
	return PRINT_APPINFO;
}

int selectable_count(int curr, int row, int col) {
	int selectable_count = 0;
	while (curr < rom_list.size() && selectable_count < (row * col)) {
		selectable_count += 1;
		curr++;
	}
	return selectable_count;
}

#define IS_OVERFLOW() ( \
        select_row * max_col + select_col >= \
        selectable_count(curr, max_row, max_col) \
    )

ScreenState on_mainscreen_event_with_dpad(int steps, int &step, int &curr, int &touched) {
	int moves;
	int max_row;
	int max_col;
	switch (mainscreen_list_mode) {
	case USE_LIST:
		moves = 1;
		max_row = LIST_ROW;
		max_col = 1;
		break;
	case USE_ICON:
	default:
		moves = ICONS_COL;
		max_row = ICONS_ROW;
		max_col = ICONS_COL;
		break;
	}

	int btn = read_buttons();

	if (btn & SCE_CTRL_UP) {
		if (select_row == 0) {
			if (step == 0) {
				return UNKNOWN;
			}
			step -= 1;
			curr -= moves;
		}
		else {
			select_row -= 1;
		}
		return MAIN_SCREEN;
	}
	if (btn & SCE_CTRL_DOWN) {
		if (select_row + 1 == max_row) {
			if (step == steps) {
				return UNKNOWN;
			}
			step += 1;
			curr += moves;
		}
		else {
			select_row += 1;
		}
		if (IS_OVERFLOW()) {
			select_row -= 1;
		}
		return MAIN_SCREEN;
	}
	if (btn & SCE_CTRL_LEFT) {
		select_col -= 1;
		if (select_col < 0) {
			select_col = 0;
		}
		return MAIN_SCREEN;
	}
	if (btn & SCE_CTRL_RIGHT) {
		select_col += 1;
		if (select_col >= max_col) {
			select_col = max_col - 1;
		}
		if (IS_OVERFLOW()) {
			select_col -= 1;
		}
		return MAIN_SCREEN;
	}
	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_R2) {
		return HELP_MSG;
	}
	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_L2) {
		return CONFIG_SCREEN;
	}
	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_SELECT) {
		return ABOUT_MSG;
	}
	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_TRIANGLE) {
		return SEARCH_OPEN;
	}
	/* Cancel gets the whole list back, which is the only thing it has to
	 * do on this screen. */
	if (!(btn & SCE_CTRL_HOLD) && (btn & SCE_CTRL_CANCEL) && !rom_search.empty()) {
		return SEARCH_CLEAR;
	}

	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_ENTER) {
		int tmp_curr = curr;
		tmp_curr += (rom_list.size() - 1 < (select_row * max_col) + select_col) ? 
			rom_list.size() - 1 : (select_row * max_col) + select_col;
		//for (; curr<rom_list.size() && curr < (select_row * max_col) + select_col; curr++);
		touched = tmp_curr;
		select_appinfo_button = 0;
		return PRINT_APPINFO;
	}

	return UNKNOWN;
}
#undef IS_OVERFLOW

/* Buttons and touch both work, always.  They used to be alternatives,
 * chosen by a setting: the buttons moved a cursor and the screen did
 * nothing, or the screen worked and the cursor was not even drawn.  Nothing
 * about the hardware requires that choice, and having to visit the settings
 * to use the input you happen to have your thumb on is the sort of thing
 * this fork exists to remove.
 *
 * Buttons are handled first because read_buttons() reports a press once;
 * touch is looked at when they did nothing. */
ScreenState on_mainscreen_event(int steps, int &step, int &curr, int &touched) {
	ScreenState state = on_mainscreen_event_with_dpad(steps, step, curr, touched);
	if (state != UNKNOWN) return state;
	return mainscreen_touch(curr, touched);
}

/* What a row does when it is chosen, by button or by tap.  Most rows change
 * a setting and stay put; the covers row asks to leave for another screen,
 * which is what the return value is for. */
static ScreenState activate_config_row(int row) {
	switch (row) {
	case 0:
		if (strncmp(config.list_mode, "icon", 4) == 0) {
			strncpy(config.list_mode, "list", 4);
			mainscreen_list_mode = USE_LIST;
		}
		else {
			strncpy(config.list_mode, "icon", 4);
			mainscreen_list_mode = USE_ICON;
		}
		break;
	case 4:
		config.use_btouch++;
		if (config.use_btouch > 2)
			config.use_btouch = 0;
		break;
	case 5:
		/* Ordering by size has to measure every folder, which apply_view
		 * does the first time it is asked for. */
		config.sort_mode = (config.sort_mode + 1) % SORT_COUNT;
		apply_view();
		break;
	case 6:
		/* Cycling the language rewrites every label, so the settings menu
		 * strings are rebuilt here too rather than only at startup. */
		config.language = (config.language + 1) % UI_LANG_COUNT;
		ui_set_language((UILanguage)config.language);
		init_sittings_text();
		break;
	case 7:
		return COVERS_ALL_CONFIRM;
	default:
		break;
	}

	return UNKNOWN;
}

/* Which row a tap landed on, or -1, using the geometry draw_config() just
 * used. */
static int config_row_at(const point &p) {
	if (p.x < config_panel_left || p.x > config_panel_left + config_panel_width)
		return -1;

	for (int i = 0; i < CONFIG_NUM; i++) {
		int top = config_rows_top + i * config_row_height;
		if (p.y >= top && p.y < top + config_row_height)
			return i;
	}
	return -1;
}

ScreenState on_config_event() {
	static int need_refresh = 0;
	static int need_save = 0;
	int btn = read_buttons();

	if (btn & SCE_CTRL_HOLD) {
		return UNKNOWN;
	}

	if (btn & SCE_CTRL_CANCEL || btn & SCE_CTRL_L2) {
		if (need_save) {
			save_config();
		}
		if (need_refresh) {
			need_refresh = 0;
			return RELOAD_MAINSCREEN;
		}
		return MAIN_SCREEN;
	}

	if (btn & SCE_CTRL_UP) {
		select_config -= 1;
		if (strncmp(config.list_mode, "icon", 4) == 0) {
			for (; select_config == 3; select_config--);
		}
		else {
			for (; select_config == 1 || select_config == 2; select_config--);
		}
		if (select_config < 0) {
			select_config = 0;
		}
		return UNKNOWN;
	}

	if (btn & SCE_CTRL_DOWN) {
		select_config += 1;
		if (strncmp(config.list_mode, "icon", 4) == 0) {
			for (; select_config == 3; select_config++);
		}
		else {
			for (; select_config == 1 || select_config == 2; select_config++);
		}
		if (select_config > CONFIG_NUM - 1) {
			select_config = CONFIG_NUM - 1;
		}
		return UNKNOWN;
	}

	if (btn & SCE_CTRL_ENTER) {
		ScreenState action = activate_config_row(select_config);
		need_refresh = 1;
		need_save = 1;
		if (action != UNKNOWN) {
			if (need_save) save_config();
			return action;
		}
	}
	if (btn & SCE_CTRL_LEFT) {
		switch (select_config) {
		case 0:
			if (strncmp(config.list_mode, "icon", 4) == 0) {
				strncpy(config.list_mode, "list", 4);
				mainscreen_list_mode = USE_LIST;
			}
			else {
				strncpy(config.list_mode, "icon", 4);
				mainscreen_list_mode = USE_ICON;
			}
			break;
		case 1:
			config.icon_row--;
			if (config.icon_row < ICON_ROW_MIN)
				config.icon_row = ICON_ROW_MIN;
			ICONS_ROW = config.icon_row;
			break;
		case 2:
			config.icon_col--;
			if (config.icon_col < ICON_COL_MIN)
				config.icon_col = ICON_COL_MIN;
			ICONS_COL = config.icon_col;
			break;
		case 3:
			config.list_row--;
			if (config.list_row < LIST_ROW_MIN)
				config.list_row = LIST_ROW_MIN;
			LIST_ROW = config.list_row;
			break;
		case 5:
			config.sort_mode = (config.sort_mode + 1) % SORT_COUNT;
			apply_view();
			break;
		case 6:
			/* Cycling the language rewrites every label, so the settings
			 * menu strings are rebuilt here too rather than only at
			 * startup. */
			config.language = (config.language + 1) % UI_LANG_COUNT;
			ui_set_language((UILanguage)config.language);
			init_sittings_text();
			break;
		default:
			break;
		}
		need_refresh = 1;
		need_save = 1;
	}
	if (btn & SCE_CTRL_RIGHT) {
		switch (select_config) {
		case 0:
			if (strncmp(config.list_mode, "icon", 4) == 0) {
				strncpy(config.list_mode, "list", 4);
				mainscreen_list_mode = USE_LIST;
			}
			else {
				strncpy(config.list_mode, "icon", 4);
				mainscreen_list_mode = USE_ICON;
			}
			break;
		case 1:
			config.icon_row++;
			if (config.icon_row > ICON_ROW_MAX)
				config.icon_row = ICON_ROW_MAX;
			ICONS_ROW = config.icon_row;
			break;
		case 2:
			config.icon_col++;
			if (config.icon_col > ICON_COL_MAX)
				config.icon_col = ICON_COL_MAX;
			ICONS_COL = config.icon_col;
			break;
		case 3:
			config.list_row++;
			if (config.list_row > LIST_ROW_MAX)
				config.list_row = LIST_ROW_MAX;
			LIST_ROW = config.list_row;
			break;
		case 5:
			config.sort_mode = (config.sort_mode + 1) % SORT_COUNT;
			apply_view();
			break;
		case 6:
			/* Cycling the language rewrites every label, so the settings
			 * menu strings are rebuilt here too rather than only at
			 * startup. */
			config.language = (config.language + 1) % UI_LANG_COUNT;
			ui_set_language((UILanguage)config.language);
			init_sittings_text();
			break;
		default:
			break;
		}
		need_refresh = 1;
		need_save = 1;
	}

	/* A tap picks the row and applies it, the same as moving to it and
	 * pressing the enter button. */
	{
		point p;
		if (read_touchscreen(&p)) {
			if (p.x < config_panel_left ||
			    p.x > config_panel_left + config_panel_width ||
			    p.y < config_panel_top ||
			    p.y > config_panel_top + config_panel_height){
				/* Outside the panel is the way back, as cancel is. */
				if (need_save) save_config();
				if (need_refresh){
					need_refresh = 0;
					return RELOAD_MAINSCREEN;
				}
				return MAIN_SCREEN;
			}

			int row = config_row_at(p);
			if (row >= 0) {
				select_config = row;
				ScreenState action = activate_config_row(row);
				need_refresh = 1;
				need_save = 1;
				if (action != UNKNOWN) {
					save_config();
					return action;
				}
			}
		}
	}

	return UNKNOWN;
}

#define APPINFO_BUTTON_AREA(n) \
    { \
        .left = APPINFO_BUTTON_LEFT, \
        .top = APPINFO_BUTTON_TOP(n), \
        .right = APPINFO_BUTTON_LEFT + APPINFO_BUTTON_WIDTH, \
        .bottom = APPINFO_BUTTON_TOP(n) + APPINFO_BUTTON_HEIGHT, \
    }

ScreenState on_appinfo_button_event(point p) {
	static rectangle backup_button_area = APPINFO_BUTTON_AREA(0);
	static rectangle restore_button_area = APPINFO_BUTTON_AREA(1);
	static rectangle delete_button_area = APPINFO_BUTTON_AREA(2);
	static rectangle reset_button_area = APPINFO_BUTTON_AREA(3);
	static rectangle cover_button_area = APPINFO_BUTTON_AREA(4);
	if (IS_TOUCHED(backup_button_area, p)) {
		return START_MODE;
	}

	if (IS_TOUCHED(restore_button_area, p)) {
		return SETTING_MODE;
	}

	if (IS_TOUCHED(delete_button_area, p)) {
		return SHORTCUT_MODE;
	}

	if (IS_TOUCHED(reset_button_area, p)) {
		return DELETE_MODE;
	}

	if (IS_TOUCHED(cover_button_area, p)) {
		return COVER_CONFIRM;
	}

	return UNKNOWN;
}

#undef APPINFO_BUTTON_AREA

/* The touch half of the game panel: a tap on one of its buttons, or a tap
 * outside it to go back. */
ScreenState appinfo_touch() {
	static rectangle appinfo_area = {
		APPINFO_PANEL_LEFT,
		APPINFO_PANEL_TOP,
		APPINFO_PANEL_LEFT + APPINFO_PANEL_WIDTH,
		APPINFO_PANEL_TOP + APPINFO_PANEL_HEIGHT,
	};

	point p;
	if (!read_touchscreen(&p)) {
		return UNKNOWN;
	}

	if (!IS_TOUCHED(appinfo_area, p)) {
		return MAIN_SCREEN;
	}

	return on_appinfo_button_event(p);
}

ScreenState on_appinfo_event_with_dpad() {
	int btn = read_buttons();
	if (btn & SCE_CTRL_HOLD) {
		return UNKNOWN;
	}

	if (btn & SCE_CTRL_UP) {
		select_appinfo_button -= 1;
		if (select_appinfo_button < 0) {
			select_appinfo_button = 0;
		}
		return PRINT_APPINFO;
	}
	if (btn & SCE_CTRL_DOWN) {
		select_appinfo_button += 1;
		if (select_appinfo_button >= APPINFO_BUTTON) {
			select_appinfo_button = APPINFO_BUTTON - 1;
		}
		return PRINT_APPINFO;
	}
	if (btn & SCE_CTRL_CANCEL) {
		return MAIN_SCREEN;
	}
	if (btn & SCE_CTRL_ENTER) {
		select_slot = 0;
		switch (select_appinfo_button) {
		case 0:
			return START_MODE;
		case 1:
			return SETTING_MODE;
		case 2:
			return SHORTCUT_MODE;
		case 3:
			return DELETE_MODE;
		case 4:
			return COVER_CONFIRM;
		}
	}
	return UNKNOWN;
}

ScreenState on_appinfo_event() {
	ScreenState state = on_appinfo_event_with_dpad();
	if (state != UNKNOWN) return state;
	return appinfo_touch();
}

/* The touch half of the per-game settings list. */
ScreenState slot_touch(int &slot) {
	slot = -1;

	point p;
	if (!read_touchscreen(&p)) {
		return UNKNOWN;
	}

	ScreenState move_appinfo_action = on_appinfo_button_event(p);
	if (move_appinfo_action != UNKNOWN) {
		return move_appinfo_action;
	}

	for (int i = 0; i < SLOT_BUTTON; i++) {
		rectangle slot_area = {
			 SLOT_BUTTON_LEFT,
			 SLOT_BUTTON_TOP(i),
			 SLOT_BUTTON_LEFT + SLOT_BUTTON_WIDTH,
			 SLOT_BUTTON_TOP(i) + SLOT_BUTTON_HEIGHT,
		};
		if (IS_TOUCHED(slot_area, p)) {
			slot = i;
			if (slot == SITTINGS_DEFAULT) {
				for (int j = 0; j < SITTINGS_NUM; j++) cmd[j] = cmd_default[j];
			}
			else if (slot == SITTINGS_RETURN) {
				return PRINT_APPINFO;
			}
			else if (slot < SITTINGS_NUM) {
				cycle_sitting(slot);
			}
			return UNKNOWN;
		}
	}
	return UNKNOWN;
}

ScreenState on_slot_event_with_dpad(int &slot) {
	slot = -1;
	int btn = read_buttons();

	if (btn & SCE_CTRL_UP) {
		select_slot -= 1;
		if (select_slot < 0) {
			select_slot = 0;
		}
		return UNKNOWN;
	}
	if (btn & SCE_CTRL_DOWN) {
		select_slot += 1;
		if (select_slot >= SLOT_BUTTON) {
			select_slot = SLOT_BUTTON - 1;
		}
		return UNKNOWN;
	}
	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_CANCEL) {
		return PRINT_APPINFO;
	}
	if (!(btn & SCE_CTRL_HOLD) && btn & SCE_CTRL_ENTER) {
		slot = select_slot;
		if (slot == SITTINGS_DEFAULT) {
			for (int j = 0; j < SITTINGS_NUM; j++) cmd[j] = cmd_default[j];
		}
		else if (slot == SITTINGS_RETURN) {
			return PRINT_APPINFO;
		}
		else if (slot < SITTINGS_NUM) {
			cycle_sitting(slot);
		}
		return UNKNOWN;
	}
	return UNKNOWN;
}

ScreenState on_slot_event(int &slot) {
	ScreenState state = on_slot_event_with_dpad(slot);
	/* slot >= 0 means the buttons just acted on a row and returned UNKNOWN
	 * to stay on this screen; going on to the touch half would reset it. */
	if (state != UNKNOWN || slot >= 0) return state;
	return slot_touch(slot);
}

ScreenState on_message_event(int curr, int(*progress_func)(int), ScreenState state_done, ScreenState state_fail, ScreenState state_cancel,int norun_fun = 0) {
	while (1) {

		int btn = read_buttons();
		if (btn & SCE_CTRL_HOLD) {
			continue;
		}
		if (btn & SCE_CTRL_ENTER) {
			if(norun_fun)
				return state_done;
			if (progress_func(curr)) {
				return state_done;
			}
			else {
				return state_fail;
			}
		}
		if (btn & SCE_CTRL_CANCEL) {
			return state_cancel;
		}

		/* The footer reads "cancel no    confirm yes", so the box splits
		 * down the middle the way it is written: left is no, right is
		 * yes.  A tap outside the box cancels, like the cancel button. */
		{
			point p;
			if (read_touchscreen(&p)) {
				if (!IS_TOUCHED(last_dialog_area, p))
					return state_cancel;
				int middle = (last_dialog_area.left + last_dialog_area.right) / 2;
				if (p.x < middle)
					return state_cancel;
				if (norun_fun)
					return state_done;
				return progress_func(curr) ? state_done : state_fail;
			}
		}
	}
	return state_cancel;
}

ScreenState on_alert_event(ScreenState state) {
	while (1) {
		int btn = read_buttons();
		if (btn & SCE_CTRL_HOLD) {
			continue;
		}
		if (btn & SCE_CTRL_ENTER) {
			break;
		}
		/* Anywhere closes it; there is only the one thing to do. */
		{
			point p;
			if (read_touchscreen(&p)) break;
		}
	}
	return state;
}

void prepare_delete_confirm(int choose) {
	if (choose < 0 || choose >= (int)rom_list.size()) return;

	char size_str[16];

	if (rom_list[choose].is_zip) {
		getSizeString(size_str, rom_list[choose].size);
		snprintf(delete_confirm_message, sizeof(delete_confirm_message),
			ui_text(UI_DELETE_ASK_ZIP),
			rom_list[choose].char_name(), size_str);
		return;
	}

	uint64_t size = 0;
	uint32_t files = 0, folders = 0;
	getPathInfo(rom_list[choose].char_path(), &size, &folders, &files);
	getSizeString(size_str, size);
	snprintf(delete_confirm_message, sizeof(delete_confirm_message),
		ui_text(UI_DELETE_ASK),
		rom_list[choose].char_name(), size_str, (unsigned)files);
}

/* Deletes the game folder, or the archive if the row is one.
 *
 * The whole folder goes, saves included -- which the prompt says in as many
 * words, because a save is the one thing here that cannot be downloaded
 * again. */
int game_delete(int choose) {
	if (choose < 0 || choose >= (int)rom_list.size()) return 0;

	const std::string path = rom_list[choose].path;
	int ok;

	if (rom_list[choose].is_zip)
		ok = (sceIoRemove(path.c_str()) >= 0);
	else
		ok = (removePath(path) > 0);   /* negative is an errno, not a yes */

	if (!ok) {
		snprintf(delete_result_message, sizeof(delete_result_message), "%s",
			ui_text(UI_DELETE_FAIL));
		return 0;
	}

	char free_str[16];
	getSizeString(free_str, ZipHandler::freeSpace());
	snprintf(delete_result_message, sizeof(delete_result_message),
		ui_text(UI_DELETE_OK), free_str);
	return 1;
}

/* Fetching blocks for as long as the request takes; COVER_RUN is drawn
 * before this is called, so the screen says what is happening.  The list is
 * not rebuilt here -- the caller reloads it, which is what picks up the new
 * image. */
/* How the batch is going, for the progress screen. */
struct CoverBatch {
	int total;      /* games that need a cover */
	int done;       /* attempted so far */
	int fetched;
	int missing;    /* vndb had nothing under that name */
	int skipped;    /* already had a cover */
	string current;
};
static CoverBatch cover_batch;

void draw_cover_progress(const CoverBatch &batch) {
	const int width  = 700;
	const int height = 200;
	const int left   = (SCREEN_WIDTH - width) / 2;
	const int top    = (SCREEN_HEIGHT - height) / 2;
	const int padding = 25;

	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TH_SCRIM);
	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);

	char line[128];
	snprintf(line, sizeof(line), "%s  (%d/%d)", ui_text(UI_COVERS_ALL_RUN),
		 batch.done, batch.total);
	th_text(left + padding, top + padding + TH_FONT_M, TH_TEXT, TH_FONT_M, line);

	/* The game being looked up, trimmed to fit. */
	snprintf(line, sizeof(line), "%s", batch.current.c_str());
	if (strlen(line) > 60) {
		memmove(line, line + strlen(line) - 60, 61);
		line[0] = line[1] = line[2] = '.';
	}
	th_text(left + padding, top + padding + TH_FONT_M * 2 + 8, TH_TEXT_DIM,
		TH_FONT_S, line);

	const int bar_left   = left + padding;
	const int bar_top    = top + height - padding - 60;
	const int bar_width  = width - (padding * 2);
	const int bar_height = 24;
	int percent = batch.total > 0 ? (batch.done * 100) / batch.total : 0;
	th_card(bar_left, bar_top, bar_width, bar_height, TH_SURFACE_HI, TH_SURFACE);
	vita2d_draw_rectangle(bar_left + 2, bar_top + 2,
		((bar_width - 4) * percent) / 100, bar_height - 4, TH_ACCENT);

	snprintf(line, sizeof(line), "%d%%   %s cancel", percent, ICON_CANCEL);
	th_text(bar_left, bar_top + bar_height + TH_FONT_S + 6, TH_TEXT_DIM,
		TH_FONT_S, line);
}

/* Does this game already have a cover?  A game that has one is not asked
 * about again, so running this a second time only costs requests for the
 * ones still missing. */
static bool has_cover(const string &path) {
	const char *names[3] = { "/cover.png", "/cover.jpg", "/icon.png" };
	for (int i = 0; i < 3; i++) {
		SceUID fd = sceIoOpen((path + names[i]).c_str(), SCE_O_RDONLY, 0777);
		if (fd >= 0) {
			sceIoClose(fd);
			return true;
		}
	}
	return false;
}

/* Every game that has no cover, one after another.  Each request blocks, so
 * the bar is repainted before each one rather than from a callback. */
void fetch_all_covers() {
	cover_batch.total = cover_batch.done = 0;
	cover_batch.fetched = cover_batch.missing = cover_batch.skipped = 0;
	cover_batch.current.clear();

	for (size_t i = 0; i < rom_list.size(); i++) {
		if (rom_list[i].is_zip) continue;
		if (has_cover(rom_list[i].path)) { cover_batch.skipped++; continue; }
		cover_batch.total++;
	}

	for (size_t i = 0; i < rom_list.size(); i++) {
		if (rom_list[i].is_zip) continue;
		if (has_cover(rom_list[i].path)) continue;

		cover_batch.current = rom_list[i].name.empty() ? rom_list[i].path
							      : rom_list[i].name;

		vita2d_start_drawing();
		vita2d_clear_screen();
		draw_title();
		draw_help();
		draw_cover_progress(cover_batch);
		vita2d_end_drawing();
		vita2d_swap_buffers();

		char saved[512];
		VndbResult result = vndb_fetch_cover(rom_list[i].char_name(),
						     rom_list[i].char_path(),
						     saved, sizeof(saved));
		if (result == VNDB_OK) cover_batch.fetched++;
		else                   cover_batch.missing++;

		cover_batch.done++;

		/* Cancelling leaves what has been fetched so far in place. */
		if (read_buttons() & SCE_CTRL_CANCEL) break;
	}

	snprintf(cover_result_message, sizeof(cover_result_message),
		 ui_text(UI_COVERS_ALL_DONE),
		 cover_batch.fetched, cover_batch.missing, cover_batch.skipped);
}

int game_cover(int choose) {
	if (choose < 0 || choose >= (int)rom_list.size()) return 0;
	if (rom_list[choose].is_zip) return 0;

	char saved[512];
	VndbResult result = vndb_fetch_cover(rom_list[choose].char_name(),
					     rom_list[choose].char_path(),
					     saved, sizeof(saved));

	snprintf(cover_result_message, sizeof(cover_result_message), "%s",
		 vndb_result_text(result));

	return result == VNDB_OK;
}

/* Text in and out of the system keyboard.
 *
 * The dialog speaks UTF-16 and everything else here speaks UTF-8, so both
 * conversions live together where they can be read against each other.
 * Only the basic plane is handled, which covers every character that can be
 * typed into a game folder's name. */
static void utf8_to_utf16(const char *in, SceWChar16 *out, int max)
{
	int o = 0;
	for (int i = 0; in[i] && o < max - 1; ){
		unsigned char c = (unsigned char)in[i];
		unsigned int cp;
		if (c < 0x80)             { cp = c;                 i += 1; }
		else if ((c & 0xE0) == 0xC0) { cp = ((c & 0x1F) << 6) |
					       (in[i+1] & 0x3F);     i += 2; }
		else if ((c & 0xF0) == 0xE0) { cp = ((c & 0x0F) << 12) |
					       ((in[i+1] & 0x3F) << 6) |
					       (in[i+2] & 0x3F);     i += 3; }
		else                      { cp = '?';               i += 1; }
		out[o++] = (SceWChar16)cp;
	}
	out[o] = 0;
}

static void utf16_to_utf8(const SceWChar16 *in, char *out, int max)
{
	int o = 0;
	for (int i = 0; in[i] && o < max - 4; i++){
		unsigned int cp = in[i];
		if (cp < 0x80)        out[o++] = (char)cp;
		else if (cp < 0x800){ out[o++] = (char)(0xC0 | (cp >> 6));
				      out[o++] = (char)(0x80 | (cp & 0x3F)); }
		else                { out[o++] = (char)(0xE0 | (cp >> 12));
				      out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
				      out[o++] = (char)(0x80 | (cp & 0x3F)); }
	}
	out[o] = '\0';
}

/* Opens the console's own keyboard and puts what was typed in rom_search.
 *
 * The dialog draws itself over the application, which has to keep drawing
 * for it to appear at all -- hence the loop, which is the launcher's normal
 * frame with the dialog composited on top. */
static void run_search(ScreenState behind, int curr, int choose, int slot)
{
	static SceWChar16 title[64];
	static SceWChar16 initial[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
	static SceWChar16 buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];

	utf8_to_utf16(ui_text(UI_SEARCH_TITLE), title, 64);
	utf8_to_utf16(rom_search.c_str(), initial,
		      SCE_IME_DIALOG_MAX_TEXT_LENGTH);
	memset(buffer, 0, sizeof(buffer));

	SceImeDialogParam param;
	sceImeDialogParamInit(&param);
	param.supportedLanguages = 0;         /* whatever the console has */
	param.languagesForced    = SCE_FALSE;
	/* The console's ordinary keyboard, not the latin-only one: the
	 * folders on this card are as likely to be named in japanese or
	 * chinese as in english. */
	param.type               = SCE_IME_TYPE_DEFAULT;
	param.title              = title;
	param.maxTextLength      = 63;
	param.initialText        = initial;
	param.inputTextBuffer    = buffer;

	if (sceImeDialogInit(&param) < 0) return;

	while (1) {
		draw_screen(behind, curr, choose, slot);

		SceCommonDialogStatus status = sceImeDialogGetStatus();
		if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
			SceImeDialogResult result;
			memset(&result, 0, sizeof(result));
			sceImeDialogGetResult(&result);

			/* Closing the keyboard rather than confirming leaves the
			 * search as it was, which is what cancel should do. */
			if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {
				char typed[128];
				utf16_to_utf8(buffer, typed, sizeof(typed));
				rom_search = typed;
				apply_view();
			}
			sceImeDialogTerm();
			break;
		}
		if (status == SCE_COMMON_DIALOG_STATUS_NONE) break;
	}
}

void  game_start(int choose) {
	/* Half a game is not a game: launching one would fail somewhere inside
	 * the engine, with a missing-file message about whichever file the
	 * install had not reached. */
	if (choose >= 0 && choose < (int)rom_list.size() && rom_list[choose].is_partial)
		return;

	/* Stamp the folder so the panel can say when this was last played.
	 * A file beside the game rather than a list somewhere central: a game
	 * moved to another card, or deleted, takes its own history with it. */
	if (choose >= 0 && choose < (int)rom_list.size() && !rom_list[choose].is_zip) {
		SceDateTime now;
		char date_str[24], time_str[16];
		sceRtcGetCurrentClock(&now, 0);
		getDateString(date_str, SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD, &now);
		getTimeString(time_str, 24, &now);

		string stamp_path = rom_list[choose].path + "/lastplayed.txt";
		SceUID fd = sceIoOpen(stamp_path.c_str(),
				      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
		if (fd >= 0) {
			char line[48];
			int len = snprintf(line, sizeof(line), "%s %s", date_str, time_str);
			sceIoWrite(fd, line, len);
			sceIoClose(fd);
		}
	}

	game_start_select = choose;
}

char* get_title_id() {
	int ret;
	char *title_id;
	title_id = new char[32];
	int num = -1;
	do {
		num++;
		sprintf(title_id, "ux0:app/ONSVG0%03d/\0", num);
		ret = checkFolderExist(title_id);
		printf("%d %s\n",ret, title_id);
	} while (ret);
	sprintf(title_id, "ONSVG0%03d\0", num);
	return title_id;
}

int game_shortcut(int choose) {
	string path = PACKAGE_TEMP;
	int ret;
	if (checkFolderExist(path.c_str())) {
		ret = removePath(path);
		if (ret < 0) {
			printf("removePath() = 0x%08X\n", ret);
		}
	}

//temp:EBOOT
	sceIoMkdir(path.c_str(), 0777);
	copyFile("app0:eboot.bin", (path + "/eboot.bin").c_str());//EBOOT
	copyFile("app0:onsjh.bin", (path + "onsjh.bin").c_str());//ons
	char *src_path = new char[64];
	char *dst_path = new char[64];
	char *tmp_str = new char[512];
//sittings.txt
	sprintf(src_path, "%s/%s", rom_list[choose].char_path(), SITTINGS_FILE);
	sprintf(dst_path, "%s/%s", path.c_str(), SITTINGS_FILE);
	copyFile(src_path, dst_path);
//startup.ini
	sprintf(dst_path, "%s/%s", path.c_str(), "startup.ini");
	FILE * tmp = fopen(dst_path, "w");
	sprintf(tmp_str,
		"[AUTO_START]\n"
		"use_btouch = %d\n"
		"rom_path = %s",
		config.use_btouch,
		rom_list[choose].char_path()
	);
	fprintf(tmp, tmp_str);
	fclose(tmp);

	free(src_path);
	free(dst_path);
	free(tmp_str);
//sce_sys
	sceIoMkdir((path + "/sce_sys/").c_str(), 0777);
	//copyFile(rom_list[choose].char_icon_path(), (path + "/sce_sys/icon0.png").c_str());
	copyFile("app0:sce_sys/icon0.png", (path + "/sce_sys/icon0.png").c_str());
	copyFile("app0:sce_sys/param.sfo", (path + "/sce_sys/param.sfo").c_str());
	copyPath("app0:sce_sys/livearea", (path + "/sce_sys/livearea").c_str());
//TITLE ID
	SceDateTime time, time_local;
	sceRtcGetCurrentClock(&time, 0);
	convertUtcToLocalTime(&time_local,&time);
	char *title_id_tmp;
	title_id_tmp = new char[10];
	sprintf(title_id_tmp, "G%02d%02d%02d%02d", time_local.day, time_local.hour, time_local.minute, time_local.second);

//TITLE
	string title = getFileName(rom_list[choose].path);
//Install
	VitaPackage pkg(path);
	pkg.SetSFOString(title, title_id_tmp);
	ret = pkg.Install();
	if(!ret)
		printf("Shortcut fail\n", ret);
	return ret;
}

int mainloop() {
	int rows;
	int steps;
	switch (mainscreen_list_mode) {
	case USE_LIST:
		rows = rom_list.size();
		steps = rows - LIST_ROW;
		break;
	case USE_ICON:
	default:
		rows = (rom_list.size() / ICONS_COL) + ((rom_list.size() % ICONS_COL) ? 1 : 0);
		steps = rows - ICONS_ROW;
		break;
	}
	//printf("total: %d row: %d steps: %d\n", rom_list.size(), rows, steps);

	if (steps < 0) {
		steps = 0;
	}

	int curr = 0;
	int choose = 0;
	int step = 0;
	select_row = 0;
	select_col = 0;

	int need_load = 0;
	int need_save = 0;
	ScreenState state = MAIN_SCREEN;
	int slot = -1;
	while (1) {
		draw_screen(state, curr, choose, slot);
		ScreenState new_state = UNKNOWN;
		while (1) {
			switch (state) {
			case MAIN_SCREEN:
				if (!need_load) need_load = 1;
				new_state = on_mainscreen_event(steps, step, curr, choose);
				/* Both change which rows are on screen, so the list is
				 * rebuilt from what is already in memory -- 2 rather than
				 * 1, which would rescan the card and reload every icon. */
				/* A hold opens that game's settings without passing
				 * through its panel, which is where they are normally
				 * read -- so read them here, or the screen would show
				 * whichever game was opened last. */
				if (new_state == SETTING_MODE &&
				    choose >= 0 && choose < (int)rom_list.size()) {
					sittings_file(rom_list[choose].path, startup_cmd, 'r');
					parseOption(startup_cmd, cmd, NULL, 0);
					need_load = 0;
				}
				if (new_state == SEARCH_OPEN) {
					run_search(MAIN_SCREEN, curr, choose, slot);
					return 2;
				}
				if (new_state == SEARCH_CLEAR) {
					rom_search.clear();
					apply_view();
					return 2;
				}
				/* A .zip row has no game to configure yet -- offer to
				 * install it instead of opening the game info panel. */
				if (new_state == PRINT_APPINFO &&
					choose >= 0 && choose < (int)rom_list.size() &&
					rom_list[choose].is_zip) {
					prepare_install_confirm(choose);
					new_state = INSTALL_CONFIRM;
				}
				//printf("%d \n", curr);
				break;
			case INSTALL_CONFIRM:
				new_state = on_message_event(choose, NULL, INSTALL_RUN,
					MAIN_SCREEN, MAIN_SCREEN, 1);
				break;
			case INSTALL_RUN:
				new_state = run_install(choose);
				break;
			case INSTALL_DONE:
				on_alert_event(MAIN_SCREEN);
				/* Reload so the freshly installed game appears and the
				 * archive row is re-evaluated. */
				return 1;
			case INSTALL_FAIL:
				on_alert_event(MAIN_SCREEN);
				return 1;
			case CONFIG_SCREEN:
				new_state = on_config_event();
				break;
			case HELP_MSG:
				new_state = on_alert_event(MAIN_SCREEN);
				break;
			case ABOUT_MSG:
				new_state = on_alert_event(MAIN_SCREEN);
				break;
			case PRINT_APPINFO:
				if (need_save) {
					parseOption(startup_cmd, cmd, cmd_str, 1);
					sittings_file(rom_list[choose].path, startup_cmd, 'w');
					need_save = 0;
				}else if (need_load) {
					sittings_file(rom_list[choose].path, startup_cmd, 'r');
					parseOption(startup_cmd, cmd, NULL, 0);
					need_load = 0;
				}
				new_state = on_appinfo_event();
				/* Measuring the folder is a directory walk, so it happens
				 * once here rather than in every frame of the prompt. */
				if (new_state == DELETE_MODE) prepare_delete_confirm(choose);
				break;
			case START_MODE:
				if (choose >= 0 && choose < (int)rom_list.size() &&
				    rom_list[choose].is_partial) {
					snprintf(cover_result_message,
						 sizeof(cover_result_message), "%s",
						 ui_text(UI_RESUME_BLOCKED));
					new_state = COVER_DONE;   /* an alert, then back */
					break;
				}
				game_start(choose);
				new_state = PRINT_APPINFO;
				return -1;
				break;
			case SETTING_MODE:
				if (!need_save) need_save = 1;
				new_state = on_slot_event(slot);
				break;
			case DELETE_MODE:
				new_state = on_message_event(choose, NULL, DELETE_RUN,
							    PRINT_APPINFO, PRINT_APPINFO, 1);
				break;
			case DELETE_RUN:
				game_delete(choose);
				new_state = DELETE_DONE;
				break;
			case DELETE_DONE:
				on_alert_event(MAIN_SCREEN);
				/* The row that was just deleted has to go. */
				return 1;
			case SHORTCUT_MODE:
				new_state = on_message_event(choose, game_shortcut, SHORTCUT_WAIT, PRINT_APPINFO, PRINT_APPINFO,1);
				break;
			case SHORTCUT_WAIT:
				if(game_shortcut(choose))
					new_state = SHORTCUT_DONE_MODE;
				else
					new_state = SHORTCUT_FAIL_MODE;
				break;
			case COVERS_ALL_CONFIRM:
				new_state = on_message_event(choose, NULL, COVERS_ALL_RUN,
							    CONFIG_SCREEN, CONFIG_SCREEN, 1);
				break;
			case COVERS_ALL_RUN:
				fetch_all_covers();
				new_state = COVERS_ALL_DONE;
				break;
			case COVERS_ALL_DONE:
				on_alert_event(MAIN_SCREEN);
				/* Reload, so the covers just written are the icons the
				 * list draws. */
				return 1;
			case COVER_CONFIRM:
				new_state = on_message_event(choose, game_cover, COVER_RUN,
							    PRINT_APPINFO, PRINT_APPINFO, 1);
				break;
			case COVER_RUN:
				game_cover(choose);
				new_state = COVER_DONE;
				break;
			case COVER_DONE:
				on_alert_event(PRINT_APPINFO);
				/* Reload so the cover that was just written is the icon
				 * the list draws. */
				return 1;
			case SHORTCUT_DONE_MODE:
				new_state = on_alert_event(PRINT_APPINFO);
				break;
			case SHORTCUT_FAIL_MODE:
				new_state = on_alert_event(PRINT_APPINFO);
				break;
			case RELOAD_MAINSCREEN:
				return 1;
			default:
				break;
			}
			if (new_state == UNKNOWN) {
				break;
			}
			state = new_state;
			
			break;
		}
	}
}

int unload_ui_start(string &rom_path) {
	//return 0;

	dictionary *ini;
	ini = iniparser_load("app0:startup.ini");
	if (ini == NULL) {
		return 0;
	}
	config.use_btouch = iniparser_getint(ini, "AUTO_START:use_btouch", 1);
	rom_path = iniparser_getstring(ini, "AUTO_START:rom_path", "app0:");
	iniparser_freedict(ini);
	sittings_file("app0:", startup_cmd, 'r', 1);
	if (startup_cmd != "") {
		parseOption(startup_cmd, cmd, NULL, 0);
		return 1;
	}
	return 0;
}


int main() 
{
	printf("ONScripter-Jh for Vita version %s (Jh %s, ons %s, %d.%02d)\n", 
		ONS_JH_VITA_VERSION, ONS_JH_VERSION, ONS_VERSION, 
		NSC_VERSION / 100, NSC_VERSION % 100);
	/* Which build this actually is.  The launcher prints it too, since a
	 * crash before the engine starts leaves this as the only marker in the
	 * log of what was installed.  sceClibPrintf rather than printf: the
	 * launcher's stdout does not reach the console log, as its existing
	 * banner above demonstrates by never appearing there. */
	sceClibPrintf("%s\n", ONS_BUILD_STRING);

	sceIoMkdir("ux0:data/onsemu", 0777);
	vita2d_init();
	vita2d_set_clear_color(TH_BG);
	
	//draw LOGO began
	/*vita2d_texture *image = vita2d_load_PNG_buffer(&_binary_res_logo_png_start);
	vita2d_start_drawing();
	vita2d_draw_texture(image, 0, 0);
	vita2d_end_drawing();
	vita2d_swap_buffers();

	sceKernelDelayThread(1000 * 3000);
	vita2d_free_texture(image);*/
	//draw LOGO end
	
	string rom_path;
	int unload_ui = unload_ui_start(rom_path);

	if (!unload_ui) 
	{
		font = vita2d_load_font_file("app0:default.ttf");
		load_config();
		/* load_config() chose the language; the labels are built from it. */
		init_sittings_text();

		if (strcmp(config.list_mode, "icon") == 0) {
			mainscreen_list_mode = USE_ICON;
		}
		else if (strcmp(config.list_mode, "list") == 0) {
			mainscreen_list_mode = USE_LIST;
		}
		init_input();
		/* After init_input(), which decides which button confirms. */
		th_load_glyphs();

		/* The dialogs draw their answers as button glyphs now; these are
		 * kept only because the install screen still prints one. */
		confirm_msg = new char[256];
		sprintf(confirm_msg, "%s", ui_text(UI_PROMPT_YES));
		confirm_msg_width = vita2d_font_text_width(font, FONT_SIZE, confirm_msg);
		close_msg = new char[256];
		sprintf(close_msg, "%s", ui_text(UI_PROMPT_CLOSE));
		close_msg_width = vita2d_font_text_width(font, FONT_SIZE, close_msg);

		//load_config();
		load_rom_list();//\BC\D3\D4\D8ͼ\B1\EA
		/* mainloop() returns 1 when it did something the list should show --
		 * a game installed, a cover fetched -- and the list has to be built
		 * again for that to be true.  It used to just re-enter the loop with
		 * the same rows and the same icons, so a fresh install or a new
		 * cover only appeared after quitting the launcher. */
		while (1) {
			int again = mainloop();
			if (again < 0) break;
			/* 1 means the card changed -- an install, a delete, a cover
			 * written -- and the list has to be read again.  2 means only
			 * which rows are shown changed, and rereading the card would
			 * throw away every icon to arrive at the same rows. */
			if (again == 1) load_rom_list();
		}

		vita2d_start_drawing();
		vita2d_clear_screen();
		vita2d_end_drawing();

		vita2d_free_font(font);
	}
	vita2d_fini();

	if (game_start_select >= 0 || unload_ui) 
	{
		cmd_num = parseOption(startup_cmd, cmd, cmd_str, 1);
		if (!unload_ui) {
			rom_path = rom_list[game_start_select].path;
			sittings_file(rom_path, startup_cmd, 'w');
		}
		cmd_str[cmd_num++] = (char*)"--root";
		cmd_str[cmd_num++] = RomInfo::to_char(rom_path);
		cmd_str[cmd_num++] = (char*)"--touch-mode";
		cmd_str[cmd_num++] = (char*)(config.use_btouch == 0 ? 
			"use_not_touch" : (config.use_btouch == 1 
				? "use_front_only_touch" : "use_front_back_touch"));
		/* Last, so a game that carries its own arguments has the final
		 * word on anything the launcher also set. */
		cmd_num = appendGameArgs(rom_path, cmd_str, cmd_num, CMD_MAX);
		cmd_str[cmd_num] = NULL;
		for(int i=0; i<cmd_num; i++)
		{
			printf("cmd_str[%d] %s\n", i, cmd_str[i]);
		}

		printf("prepare sceAppMgrLoadExec %s\n", ONSJH_PATH);
		int res = sceAppMgrLoadExec(ONSJH_PATH, cmd_str, NULL);
		printf("sceAppMgrLoadExec %s res=0x%08x\n", ONSJH_PATH, res);

		// int status;
		// // SceUID res = sceKernelLoadStartModule("app0:module/onsjh.suprx", 0, NULL, 0, NULL , &status);
		// SceUID modid = sceKernelLoadModule("app0:module/onsjh2.suprx", 0, NULL);
		// printf("sceKernelLoadModule onsjh.suprx modid=0x%08x", modid);
		// ons_main(cmd_num, cmd_str);
		// if(modid > 0)
		// {
		// 	// int res = sceKernelStartModule(modid, 0, NULL, 0, NULL, &status);
		// 	// printf("sceKernelStartModule onsjh.suprx res=0x%08x, status=%d", modid, status);
		// }
	}
}

//--------------------------------
struct RomInfo_s 
{
	string path;
	string name;
};

void end() 
{
	sceKernelExitProcess(0);
	exit(0);
}
