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
#include "installname.h"

extern "C" {
#include "logfile.h"
#include "logtail.h"
}

extern "C" {
#include "formats.h"
#include "patchplan.h"
}
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
/* Window, font cache on, no shadow, text box on, detected encoding, and
 * then the five that follow the launcher's own settings when they are 0. */
int cmd_default[CMD_OPTS] = { 0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0 };
int cmd[CMD_OPTS] = {0};
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
	sittings[SITTINGS_TOUCH] = ui_text(UI_SET_TOUCH);
	sittings[SITTINGS_TEXT_SPEED] = ui_text(UI_CFG_TEXT_SPEED_GAME);
	sittings[SITTINGS_VOL_BGM]    = ui_text(UI_CFG_VOL_BGM_GAME);
	sittings[SITTINGS_VOL_SE]     = ui_text(UI_CFG_VOL_SE_GAME);
	sittings[SITTINGS_VOL_VOICE]  = ui_text(UI_CFG_VOL_VOICE_GAME);
	sittings[SITTINGS_PATCHES] = ui_text(UI_SET_PATCHES);
	sittings[SITTINGS_BACKUP]  = ui_text(UI_SET_BACKUP);
	sittings[SITTINGS_RESTORE] = ui_text(UI_SET_RESTORE);
	sittings[SITTINGS_DEFAULT] = ui_text(UI_SET_RESET);
	sittings[SITTINGS_RETURN]  = ui_text(UI_SET_RETURN);
}

DrawListMode mainscreen_list_mode;

/* Advances one setting to its next value.  Most are plain on/off toggles;
 * the encoding cycles auto -> sjis -> gbk, and the touch panels cycle
 * default -> front -> both -> back -> off. */
static void cycle_sitting(int slot)
{
	if (slot == SITTINGS_ENCODING)
		cmd[slot] = (cmd[slot] + 1) % 3;
	else if (slot == SITTINGS_TOUCH)
		cmd[slot] = (cmd[slot] + 1) % 5;
	else if (slot == SITTINGS_TEXT_SPEED)
		cmd[slot] = (cmd[slot] + 1) % 4;
	else if (slot == SITTINGS_VOL_BGM || slot == SITTINGS_VOL_SE ||
		 slot == SITTINGS_VOL_VOICE)
		/* 0 follows the launcher, then 0% to 100% in tens. */
		cmd[slot] = (cmd[slot] + 1) % 12;
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

	/* Decoded here, the first time this game is drawn, rather than while
	 * the list was being built. */
	const RomIcon cover = rom_icon(rom_list[curr]);
	th_cover(cover.tex, cover.w, cover.h,
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
	const RomIcon cover = rom_icon(rom_list[curr]);
	th_cover(cover.tex, cover.w, cover.h,
		 x + 8, y + 4, thumb, thumb);

	const int text_left = x + 8 + thumb + TH_PAD;
	const int text_width = w - (text_left - x) - TH_PAD - 90;

	th_text(text_left, y + (h / 2) - 2,
		selected ? TH_TEXT : TH_TEXT_DIM, TH_FONT_M,
		th_fit(rom_list[curr].char_name(), TH_FONT_M, text_width));
	th_text(text_left, y + (h / 2) + TH_FONT_S + 2, TH_TEXT_FAINT, TH_FONT_S,
		th_fit(rom_list[curr].char_path(), TH_FONT_S, text_width));

	if (rom_list[curr].is_zip) {
		/* How big it is, before you pick it: a row that only says ".zip"
		 * makes you open the panel to find out whether it will fit. */
		const int right = x + w - TH_PAD;
		th_text_right(right, y + (h / 2) + 2, TH_ACCENT, TH_FONT_S, ".zip");

		const int tag_w = vita2d_font_text_width(font, TH_FONT_S, ".zip");
		char size_str[16];
		getSizeString(size_str, rom_list[curr].size);
		th_text_right(right - tag_w - 10, y + (h / 2) + 2, TH_TEXT_FAINT,
			      TH_FONT_S, size_str);
	}
	else if (rom_list[curr].is_partial) {
		th_text_right(x + w - TH_PAD, y + (h / 2) + 2, TH_DANGER, TH_FONT_S,
			ui_text(UI_UNFINISHED));
	}
}

/*
 * What the launcher shows before there is anything to show.
 *
 * An empty grid with a line of footer text was accurate and useless: the
 * one thing a new user needs is where a game has to be for it to appear,
 * and that is not something they can find by pressing buttons.  Both
 * folders exist by the time this is drawn -- the launcher creates them at
 * startup -- so it can say so rather than ask.
 */
void draw_first_run() {
	const int padding = 28;
	const int width   = 640;
	const int height  = 300;
	const int left    = (SCREEN_WIDTH - width) / 2;
	const int top     = HEADER_HEIGHT + (ITEMS_PANEL_HEIGHT - height) / 2;

	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);

	th_text(left + padding, top + padding + TH_FONT_L, TH_TEXT, TH_FONT_L,
		ui_text(UI_FIRST_RUN_TITLE));

	/* Drawn line by line: the body is a paragraph with two indented paths
	 * in it, and vita2d draws a string with newlines as one line. */
	const char *body = ui_text(UI_FIRST_RUN_BODY);
	int y = top + padding + TH_FONT_L + 30;
	while (*body) {
		char line[128];
		int n = 0;
		while (*body && *body != '\n' && n < (int)sizeof(line) - 1)
			line[n++] = *body++;
		line[n] = '\0';
		if (*body == '\n') body++;

		if (n > 0)
			th_text(left + padding, y, TH_TEXT_DIM, TH_FONT_S,
				th_fit(line, TH_FONT_S, width - padding * 2));
		y += 22;
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
		/* The panel in the middle of the screen says where games go;
		 * the footer says how to read more. */
		int hx = TH_PAD;
		hx += th_button(hx, baseline, th_glyph_r, "R", TH_FONT_S) + 5;
		th_hint(hx, baseline, NULL, ui_text(UI_HINT_HELP), TH_TEXT_DIM,
			TH_FONT_S);
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
	/* The string is already here; copying it onto the heap to get a
	 * const char * leaked one allocation per button per frame, and the
	 * launcher draws fifteen of them. */
	const char *label = text.c_str();

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

/* A number as text, for a settings row.
 *
 * draw_config runs every frame, so this must not allocate: RomInfo::to_char
 * returns a new[] that nobody frees, which on this screen was a steady leak
 * of the launcher's heap for as long as it was open.  A handful of buffers
 * used in turn is enough -- the caller draws them and does not keep them. */
static const char *number_text(int value) {
	static char buffers[8][12];
	static int next = 0;

	char *out = buffers[next];
	next = (next + 1) % 8;
	snprintf(out, sizeof(buffers[0]), "%d", value);
	return out;
}

/* How far the layer over the library has arrived: 0 when it has just been
 * opened, 1 once it has settled.  A panel that appears fully formed reads as
 * a screen swap; one that fades up and lifts the last few pixels reads as
 * something laid on top of what is still there. */
static float layer_anim = 0.0f;
static ScreenState layer_last = UNKNOWN;

static bool state_is_layer(ScreenState state) {
	return state >= PRINT_APPINFO || state == CONFIG_SCREEN ||
	       state == HELP_MSG || state == FORMATS_MSG ||
	       state == LAUNCHER_MSG || state == ABOUT_MSG;
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
static int config_scroll = 0;
static int config_visible_rows = CONFIG_NUM;
static int config_row_height = 44;
static int config_panel_left = 0;
static int config_panel_width = SCREEN_WIDTH;
static int config_panel_top = HEADER_HEIGHT;
static int config_panel_height = ITEMS_PANEL_HEIGHT;

void draw_config() {
	struct config_item items[] = {
		{ui_text(UI_CFG_GRAPHIC_MODE),
			strcmp(config.list_mode, "icon") ? ui_text(UI_CFG_LIST) : ui_text(UI_CFG_ICON)},
		{ui_text(UI_CFG_ICON_ROW),   number_text(config.icon_row)},
		{ui_text(UI_CFG_ICON_COL),   number_text(config.icon_col)},
		{ui_text(UI_CFG_LIST_ROW),   number_text(config.list_row)},
		{ui_text(UI_CFG_TOUCH_MODE),
			config.use_btouch == 0 ? ui_text(UI_TOUCH_OFF)
				: (config.use_btouch == 1 ? ui_text(UI_TOUCH_FRONT)
					: (config.use_btouch == 2 ? ui_text(UI_TOUCH_BOTH)
						: ui_text(UI_TOUCH_BACK)))},
		{ui_text(UI_CFG_SORT),
			config.sort_mode == SORT_RECENT ? ui_text(UI_SORT_RECENT)
				: (config.sort_mode == SORT_SIZE ? ui_text(UI_SORT_SIZE)
								 : ui_text(UI_SORT_NAME))},
		/* Each language named in itself, which is how a language
		 * picker is read by the person who needs it. */
		{ui_text(UI_CFG_LANGUAGE),
			config.language == UI_LANG_ZH ? "\xE4\xB8\xAD\xE6\x96\x87"
				: (config.language == UI_LANG_JA ? "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
								 : "English")},
		/* Not a setting but an action, and this is where a player looks for
		 * something that applies to the whole library rather than to the
		 * game under the cursor. */
		{ui_text(UI_CFG_FETCH_COVERS), ui_text(UI_COVERS_START)},
		{ui_text(UI_CFG_CLEAN), ui_text(UI_CLEAN_START)},
		/* What a game starts at the first time it is played.  A game
		 * that has been played keeps whatever it saved, so these are
		 * defaults rather than an override. */
		{ui_text(UI_CFG_TEXT_SPEED),
			config.text_speed == 0 ? ui_text(UI_SPEED_SLOW)
				: (config.text_speed == 2 ? ui_text(UI_SPEED_FAST)
							  : ui_text(UI_SPEED_NORMAL))},
		{ui_text(UI_CFG_VOL_BGM),   number_text(config.vol_bgm)},
		{ui_text(UI_CFG_VOL_SE),    number_text(config.vol_se)},
		{ui_text(UI_CFG_VOL_VOICE), number_text(config.vol_voice)},
		{ui_text(UI_CFG_DEBUG_LOG),
			config.debug_log ? ui_text(UI_ON) : ui_text(UI_OFF)},
		{ui_text(UI_CFG_THEME),
			config.theme == TH_MODE_LIGHT ? ui_text(UI_THEME_LIGHT)
						      : ui_text(UI_THEME_DARK)},
		{ui_text(UI_CFG_VIEW_LOG), ui_text(UI_LOG_OPEN)}
	};

	/* The settings sit on a card over a dimmed library rather than over a
	 * black panel, so it is clear they are a layer on top of it. */
	vita2d_draw_rectangle(0, HEADER_HEIGHT, SCREEN_WIDTH, ITEMS_PANEL_HEIGHT,
		th_alpha(TH_SCRIM, layer_anim));

	const int panel_w = 560;
	const int row_h   = 44;

	/* More settings than fit on a 544-pixel screen, so the list scrolls
	 * with the selection rather than the panel growing off the edge. */
	int visible = (ITEMS_PANEL_HEIGHT - TH_PAD * 2 - 34) / row_h;
	if (visible > CONFIG_NUM) visible = CONFIG_NUM;
	if (visible < 1) visible = 1;
	if (select_config < config_scroll) config_scroll = select_config;
	if (select_config > config_scroll + visible - 1)
		config_scroll = select_config - visible + 1;
	if (config_scroll > CONFIG_NUM - visible) config_scroll = CONFIG_NUM - visible;
	if (config_scroll < 0) config_scroll = 0;

	const int panel_h = visible * row_h + TH_PAD * 2 + 34;
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

	for (int v = 0; v < visible; v++) {
		const int i = config_scroll + v;
		const int y = first_row + v * row_h;

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

	/* A hint that there is more above or below, so a list that scrolls
	 * does not look like a list that ends. */
	if (config_scroll > 0)
		th_text_right(panel_x + panel_w - TH_PAD, panel_y + TH_PAD + TH_FONT_M - 2,
			TH_TEXT_FAINT, TH_FONT_S, "^");
	if (config_scroll + visible < CONFIG_NUM)
		th_text_right(panel_x + panel_w - TH_PAD, panel_y + panel_h - TH_PAD + 2,
			TH_TEXT_FAINT, TH_FONT_S, "v");

	/* Where the rows are, for the touch handling. */
	config_visible_rows = visible;
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
	const RomIcon cover = rom_icon(rom_list[curr]);
	th_cover(cover.tex, cover.w, cover.h,
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
		ui_text(UI_BTN_START), TH_FONT_S,
		(state == START_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 0));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(1),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		ui_text(UI_BTN_CONFIG), TH_FONT_S,
		(state == SETTING_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 1));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(2),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		ui_text(UI_BTN_INSTALL), TH_FONT_S,
		(state == SHORTCUT_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 2));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(3),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		ui_text(UI_BTN_DELETE), TH_FONT_S,
		(state == DELETE_MODE) ||
		(state == PRINT_APPINFO && select_appinfo_button == 3));
	draw_button(APPINFO_BUTTON_LEFT, APPINFO_BUTTON_TOP(4),
		APPINFO_BUTTON_WIDTH, APPINFO_BUTTON_HEIGHT,
		ui_text(UI_BTN_COVER), TH_FONT_S,
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
			if (i == SITTINGS_TEXT_SPEED) {
				static const UIStringId speed_word[] = {
					UI_TOUCH_DEFAULT, UI_SPEED_SLOW,
					UI_SPEED_NORMAL, UI_SPEED_FAST
				};
				tmp += ui_text(speed_word[cmd[i] % 4]);
				tmp += "]";
			}
			else if (i == SITTINGS_VOL_BGM || i == SITTINGS_VOL_SE ||
				 i == SITTINGS_VOL_VOICE) {
				if (cmd[i] == 0)
					tmp += ui_text(UI_TOUCH_DEFAULT);
				else
					tmp += number_text((cmd[i] - 1) * 10);
				tmp += "]";
			}
			else if (i == SITTINGS_TOUCH) {
				/* Five states, and the first of them defers to
				 * the launcher's own setting rather than being
				 * a value of its own. */
				static const UIStringId touch_word[] = {
					UI_TOUCH_DEFAULT, UI_TOUCH_FRONT,
					UI_TOUCH_BOTH, UI_TOUCH_BACK,
					UI_TOUCH_OFF
				};
				tmp += ui_text(touch_word[cmd[i] % 5]);
				tmp += "]";
			}
			else if (i == SITTINGS_ENCODING) {
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

	/* The second page is only findable if this says it is there. */
	int formats_w = th_hint_width(th_glyph_dpad, ui_text(UI_PROMPT_FORMATS),
				      TH_FONT_S);
	int close_w = th_hint_width(th_glyph_enter, ui_text(UI_PROMPT_CLOSE),
				    TH_FONT_S);
	int hx = left + (width - (formats_w + 20 + close_w)) / 2;
	hx += th_hint(hx, top + height - padding + 6, th_glyph_dpad,
		      ui_text(UI_PROMPT_FORMATS), TH_TEXT_DIM, TH_FONT_S);
	th_hint(hx + 20, top + height - padding + 6, th_glyph_enter,
		ui_text(UI_PROMPT_CLOSE), TH_TEXT_DIM, TH_FONT_S);
}

/*
 * The help screen's second page: which of a game's files this build can
 * open.  It is the question people hit after copying a game across and
 * finding a silent opening or a missing picture, and the answer used to
 * live only in a build script.
 *
 * The rows come from the shared table in src/common/formats.c, so this
 * screen and the README say the same thing.
 */
static const int kFormatRow    = 22;   /* one format */
static const int kFormatHeader = 26;   /* a category heading and its rule */
static const int kFormatName   = 168;  /* where the extensions start */

/* How tall a category's block is, so the panel can be sized to its contents
 * rather than to a number that was right when it was written. */
static int format_block_height(FormatCategory cat)
{
	int first = 0, count = 0;
	if (!formats_category_range(cat, &first, &count)) return 0;
	return kFormatHeader + count * kFormatRow + 8;
}

static int draw_format_block(int left, int top, int width, FormatCategory cat)
{
	int first = 0, count = 0;
	if (!formats_category_range(cat, &first, &count)) return 0;

	int y = top;

	th_text(left, y + TH_FONT_S, TH_TEXT, TH_FONT_S,
		formats_category_name(cat));
	y += kFormatHeader;
	vita2d_draw_rectangle(left, y - 8, width, 1, TH_LINE);

	for (int i = first; i < first + count; i++) {
		const FormatEntry *e = formats_get(i);
		const char *word;
		unsigned int color;

		switch (e->support) {
		case FORMAT_PLAYS:
			word = ui_text(UI_FORMATS_PLAYS);
			color = TH_ACCENT;
			break;
		case FORMAT_SLOW:
			word = ui_text(UI_FORMATS_SLOW);
			color = TH_TEXT_DIM;
			break;
		default:
			word = ui_text(UI_FORMATS_CONVERT);
			color = TH_DANGER;
			break;
		}

		/* The verdict is right-aligned, so the words line up as a column
		 * the eye can run down.  The two on the left are trimmed to the
		 * space they have rather than drawn over each other: a name and
		 * its extensions running together is worse than either being
		 * short. */
		int word_w = vita2d_font_text_width(font, TH_FONT_S, word);
		int ext_w  = width - kFormatName - word_w - 12;

		th_text(left, y + TH_FONT_S, TH_TEXT, TH_FONT_S,
			th_fit(e->name, TH_FONT_S, kFormatName - 10));
		if (ext_w > 20)
			th_text(left + kFormatName, y + TH_FONT_S, TH_TEXT_FAINT,
				TH_FONT_S, th_fit(e->extensions, TH_FONT_S, ext_w));
		th_text(left + width - word_w, y + TH_FONT_S, color, TH_FONT_S, word);

		y += kFormatRow;
	}

	return y - top + 8;
}

void draw_formats_screen() {
	const int padding = 24;
	const int col_gap = 24;
	const int col_w   = 384;
	const int width   = padding * 2 + col_w * 2 + col_gap;

	/* Heading, legend, then the tallest of the two columns, then the row
	 * of hints.  Sized from the table so a format added to it cannot push
	 * the last row under the footer. */
	const int head_h   = TH_FONT_M + 46;
	const int foot_h   = 44;
	const int left_h   = format_block_height(FORMAT_CATEGORY_VIDEO);
	const int right_h  = format_block_height(FORMAT_CATEGORY_AUDIO) + 12 +
			     format_block_height(FORMAT_CATEGORY_IMAGE);
	const int body_h   = left_h > right_h ? left_h : right_h;

	int height = padding * 2 + head_h + body_h + foot_h;
	if (height > SCREEN_HEIGHT - 8) height = SCREEN_HEIGHT - 8;

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

	th_text(left + padding, top + padding + TH_FONT_M, TH_TEXT, TH_FONT_M,
		ui_text(UI_FORMATS_TITLE));
	th_text(left + padding, top + padding + TH_FONT_M + 24, TH_TEXT_DIM,
		TH_FONT_S, ui_text(UI_FORMATS_LEGEND));

	/* Video on the left, the two shorter lists stacked on the right: the
	 * whole table at once, with nothing to scroll. */
	const int body = top + padding + head_h;
	draw_format_block(left + padding, body, col_w, FORMAT_CATEGORY_VIDEO);

	const int right = left + padding + col_w + col_gap;
	int used = draw_format_block(right, body, col_w, FORMAT_CATEGORY_AUDIO);
	draw_format_block(right, body + used + 12, col_w, FORMAT_CATEGORY_IMAGE);

	vita2d_draw_rectangle(left + padding, top + height - foot_h,
		width - padding * 2, 1, TH_LINE);

	/* Two ways out, side by side: back to the controls page, or done. */
	int back_w  = th_hint_width(th_glyph_dpad, ui_text(UI_PROMPT_CONTROLS),
				    TH_FONT_S);
	int close_w = th_hint_width(th_glyph_enter, ui_text(UI_PROMPT_CLOSE),
				    TH_FONT_S);
	const int hint_baseline = top + height - foot_h / 2 + 8;
	int x = left + (width - (back_w + 28 + close_w)) / 2;
	x += th_hint(x, hint_baseline, th_glyph_dpad,
		     ui_text(UI_PROMPT_CONTROLS), TH_TEXT_DIM, TH_FONT_S);
	th_hint(x + 28, hint_baseline, th_glyph_enter,
		ui_text(UI_PROMPT_CLOSE), TH_TEXT_DIM, TH_FONT_S);
}

/*
 * Reading a log on the console.
 *
 * Everything both binaries print goes to a file when the player turns
 * logging on, and until now the only way to read it was to take the memory
 * card out.  That makes a bug report something only someone with a PC can
 * file, which is the wrong way round: the person who hit the bug is the one
 * holding the console.
 *
 * The end of the log is what matters, so the view starts at the bottom and
 * scrolls up from there.
 */
static int  log_view_which = 0;    /* 0 engine, 1 launcher, 2 last crash */
#define LOG_VIEW_PAGES 3
static int  log_view_scroll = 0;   /* lines from the bottom */

static const char *log_view_path() {
	switch (log_view_which) {
	case 1:  return LAUNCHER_LOG_FILE;
	case 2:  return CRASH_REPORT_FILE;
	default: return ENGINE_LOG_FILE;
	}
}

static const char *log_view_name() {
	switch (log_view_which) {
	case 1:  return ui_text(UI_LOG_LAUNCHER);
	case 2:  return ui_text(UI_LOG_CRASH);
	default: return ui_text(UI_LOG_ENGINE);
	}
}

void draw_log_screen() {
	/* One window into the file, read fresh each frame: a log is small, the
	 * read is a few kilobytes, and a stale view of a log is worse than a
	 * slow one. */
	static char  buffer[8192];
	static char *lines[256];
	const int    max_lines = (int)(sizeof(lines) / sizeof(lines[0]));

	const int count = log_tail(log_view_path(), buffer, sizeof(buffer),
				   lines, max_lines);

	const int padding = 20;
	const int row_h   = 20;
	const int width   = SCREEN_WIDTH - 40;
	const int height  = SCREEN_HEIGHT - 40;
	const int left    = (SCREEN_WIDTH - width) / 2;
	const int top     = (SCREEN_HEIGHT - height) / 2;

	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TH_SCRIM);
	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);

	last_dialog_area.left = left;
	last_dialog_area.top = top;
	last_dialog_area.right = left + width;
	last_dialog_area.bottom = top + height;

	/* Which log, how big it is, and where in it we are looking. */
	char title[128];
	const long size = log_size(log_view_path());
	snprintf(title, sizeof(title), "%s  %ld bytes", log_view_name(), size);
	th_text(left + padding, top + padding + TH_FONT_M, TH_TEXT, TH_FONT_M,
		title);

	const int body_top = top + padding + TH_FONT_M + 16;
	const int body_h   = height - (body_top - top) - 44;
	const int visible  = body_h / row_h;

	if (count <= 0) {
		th_text(left + padding, body_top + TH_FONT_S, TH_TEXT_DIM,
			TH_FONT_S, ui_text(UI_LOG_EMPTY));
	}
	else {
		/* Clamped here rather than where the buttons are handled: the
		 * number of lines is only known once the file has been read. */
		int max_scroll = count - visible;
		if (max_scroll < 0) max_scroll = 0;
		if (log_view_scroll > max_scroll) log_view_scroll = max_scroll;
		if (log_view_scroll < 0) log_view_scroll = 0;

		const int first = count - visible - log_view_scroll;
		for (int i = 0; i < visible; i++) {
			const int index = (first < 0 ? 0 : first) + i;
			if (index >= count) break;
			th_text(left + padding, body_top + (i + 1) * row_h,
				TH_TEXT_DIM, TH_FONT_S,
				th_fit(lines[index], TH_FONT_S, width - padding * 2));
		}
	}

	vita2d_draw_rectangle(left + padding, top + height - 44,
		width - padding * 2, 1, TH_LINE);

	int switch_w = th_hint_width(th_glyph_square, ui_text(UI_PROMPT_SWITCH),
				     TH_FONT_S);
	int close_w  = th_hint_width(th_glyph_cancel, ui_text(UI_PROMPT_CLOSE),
				     TH_FONT_S);
	const int baseline = top + height - 22 + 6;
	int x = left + (width - (switch_w + 28 + close_w)) / 2;
	x += th_hint(x, baseline, th_glyph_square, ui_text(UI_PROMPT_SWITCH),
		     TH_TEXT_DIM, TH_FONT_S);
	th_hint(x + 28, baseline, th_glyph_cancel, ui_text(UI_PROMPT_CLOSE),
		TH_TEXT_DIM, TH_FONT_S);
}

ScreenState on_log_event() {
	while (1) {
		int btn = read_buttons();

		if (btn & SCE_CTRL_UP) {
			log_view_scroll += (btn & SCE_CTRL_HOLD) ? 3 : 1;
			return UNKNOWN;
		}
		if (btn & SCE_CTRL_DOWN) {
			log_view_scroll -= (btn & SCE_CTRL_HOLD) ? 3 : 1;
			if (log_view_scroll < 0) log_view_scroll = 0;
			return UNKNOWN;
		}
		if (btn & SCE_CTRL_HOLD) continue;

		if (btn & SCE_CTRL_LTRIGGER) {
			log_view_scroll += 15;
			return UNKNOWN;
		}
		if (btn & SCE_CTRL_RTRIGGER) {
			log_view_scroll -= 15;
			if (log_view_scroll < 0) log_view_scroll = 0;
			return UNKNOWN;
		}
		if (btn & SCE_CTRL_SQUARE) {
			log_view_which = (log_view_which + 1) % LOG_VIEW_PAGES;
			log_view_scroll = 0;   /* the end of the next one */
			return UNKNOWN;
		}
		if (btn & SCE_CTRL_CANCEL || btn & SCE_CTRL_ENTER) {
			return CONFIG_SCREEN;
		}
		{
			point p;
			if (read_touchscreen(&p)) return CONFIG_SCREEN;
		}
	}
}

/*
 * The help screen's third page: the launcher.
 *
 * The first two pages answer questions someone has while playing.  This one
 * answers the question they have before that -- where does a game go so
 * that it shows up at all -- which is the one thing a new user cannot work
 * out by pressing buttons.
 */
void draw_launcher_screen() {
	struct launcher_row {
		vita2d_texture **glyph;
		const char      *chip;
		UIStringId       description;
	};
	const launcher_row rows[] = {
		{ &th_glyph_enter,    NULL,     UI_LAUNCHER_START },
		{ &th_glyph_dpad,     NULL,     UI_LAUNCHER_MOVE },
		{ &th_glyph_triangle, "/\\",    UI_LAUNCHER_SEARCH },
		{ &th_glyph_l,        "L",      UI_LAUNCHER_SETTINGS },
		{ &th_glyph_r,        "R",      UI_LAUNCHER_HELP },
		{ &th_glyph_select,   "SELECT", UI_LAUNCHER_ABOUT },
	};
	const int count = (int)(sizeof(rows) / sizeof(rows[0]));

	const int padding = 28;
	const int row_h   = 30;
	const int col_w   = 150;

	int desc_w = 0;
	for (int i = 0; i < count; i++) {
		int w = vita2d_font_text_width(font, TH_FONT_S,
			ui_text(rows[i].description));
		if (w > desc_w) desc_w = w;
	}
	int where_w = vita2d_font_text_width(font, TH_FONT_S, ui_text(UI_WHERE_ZIP));
	int w2 = vita2d_font_text_width(font, TH_FONT_S, ui_text(UI_WHERE_FOLDER));
	if (w2 > where_w) where_w = w2;

	int width = padding * 2 + col_w + desc_w;
	if (padding * 2 + where_w > width) width = padding * 2 + where_w;
	const int height = padding * 2 + 40 + count * row_h + 34 + 76;
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
		ui_text(UI_LAUNCHER_TITLE));

	for (int i = 0; i < count; i++) {
		const int baseline = top + padding + 40 + (i + 1) * row_h;
		th_button(left + padding, baseline,
			  rows[i].glyph ? *rows[i].glyph : NULL, rows[i].chip,
			  TH_FONT_M);
		th_text(left + padding + col_w, baseline, TH_TEXT_DIM, TH_FONT_S,
			ui_text(rows[i].description));
	}

	/* Where the files go, which is the reason this page exists. */
	int y = top + padding + 40 + (count + 1) * row_h + 12;
	vita2d_draw_rectangle(left + padding, y - 16, width - padding * 2, 1, TH_LINE);
	th_text(left + padding, y + TH_FONT_S, TH_TEXT, TH_FONT_S,
		ui_text(UI_WHERE_TITLE));
	th_text(left + padding, y + TH_FONT_S + 24, TH_TEXT_DIM, TH_FONT_S,
		ui_text(UI_WHERE_ZIP));
	th_text(left + padding, y + TH_FONT_S + 46, TH_TEXT_DIM, TH_FONT_S,
		ui_text(UI_WHERE_FOLDER));

	vita2d_draw_rectangle(left + padding, top + height - 44,
		width - padding * 2, 1, TH_LINE);

	int next_w  = th_hint_width(th_glyph_dpad, ui_text(UI_PROMPT_CONTROLS),
				    TH_FONT_S);
	int close_w = th_hint_width(th_glyph_enter, ui_text(UI_PROMPT_CLOSE),
				    TH_FONT_S);
	int x = left + (width - (next_w + 28 + close_w)) / 2;
	x += th_hint(x, top + height - padding + 6, th_glyph_dpad,
		     ui_text(UI_PROMPT_LAUNCHER), TH_TEXT_DIM, TH_FONT_S);
	th_hint(x + 28, top + height - padding + 6, th_glyph_enter,
		ui_text(UI_PROMPT_CLOSE), TH_TEXT_DIM, TH_FONT_S);
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
static char clean_result_message[128] = { '\0' };
static char saves_result_message[160] = { '\0' };

static ZipInstallProgress install_progress;
static ZipInstallStatus  install_status = ZIP_INSTALL_OK;
static char install_message[512];

/* What can be done about the failure that just happened.
 *
 * An install that fails is not always a dead end, and which way out exists
 * depends on why it failed: a card that filled up can be cleared, a write
 * that failed can be tried again -- and, since an interrupted install
 * leaves a journal, trying again carries on from where it stopped rather
 * than starting the archive over.  An archive with no game in it, on the
 * other hand, will fail the same way however many times it is asked. */
enum InstallRecovery {
	RECOVER_NONE = 0,   /* nothing to offer; just say what happened */
	RECOVER_RETRY,
	RECOVER_CLEAN_RETRY /* clear temporary files first, then retry */
};
static InstallRecovery install_recovery = RECOVER_NONE;
/* Whether that retry would carry on rather than start over.  Worked out
 * once when the install fails, not while the dialog is on screen: it reads
 * the archive, and the dialog is drawn every frame. */
static bool install_resumable = false;
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
	install_recovery = RECOVER_NONE;
	install_resumable = false;

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

	/* Say what happened, then what can be done about it. */
	int written = snprintf(install_message, sizeof(install_message), "%s",
		ZipHandler::statusMessage(install_status));
	if (written < 0) written = 0;

	switch (install_status) {
	case ZIP_INSTALL_NO_SPACE:
		install_recovery = RECOVER_CLEAN_RETRY;
		snprintf(install_message + written, sizeof(install_message) - written,
			 "%s", ui_text(UI_FAIL_SPACE_HINT));
		break;
	case ZIP_INSTALL_WRITE_FAILED:
	case ZIP_INSTALL_CANCELED:
		install_recovery = RECOVER_RETRY;
		break;
	default:
		/* A corrupt archive, one with no game in it, or a folder that is
		 * already there: all of them fail the same way next time. */
		install_recovery = RECOVER_NONE;
		break;
	}

	/* An install that got part way leaves a journal, so a retry resumes.
	 * Worth saying: "retry" otherwise reads as "start the half hour
	 * again". */
	if (install_recovery != RECOVER_NONE) {
		const uint64_t done = ZipHandler::resumableBytes(rom_list[choose].path);
		install_resumable = (done > 0);
		if (done > 0) {
			char done_str[16];
			getSizeString(done_str, done);
			written = (int)strlen(install_message);
			snprintf(install_message + written,
				 sizeof(install_message) - written,
				 ui_text(UI_FAIL_RESUME_HINT), done_str);
		}
	}

	return INSTALL_FAIL;
}

/* ------------------------------------------------------------------ *
 *  Patches over a game that is already installed
 *
 *  A translation patch is an archive with no script in it, whose files
 *  belong on top of a game that is already there.  Selecting one used to
 *  offer to install it as a game, which failed twice over -- no script,
 *  and the destination exists -- and told the player nothing about what to
 *  do instead.  Selecting one now asks which game it goes on.
 * ------------------------------------------------------------------ */

struct PatchCandidate {
	std::string name;   /* what the list calls the game */
	std::string path;   /* its folder */
	int         score;  /* how well the archive's name matches it */
};

static std::vector<PatchCandidate> patch_candidates;
static int         patch_pick_index = 0;
static std::string patch_zip_path;
static std::string patch_target_path;
static std::string patch_target_name;
static char        patch_confirm_message[512];
static char        patch_message[512];

/* The patches on one game, and which of them is selected. */
static std::vector<std::string> patch_applied;
static int         patch_list_index = 0;
static std::string patch_list_game;      /* folder */
static std::string patch_list_name;      /* what to call it on screen */

/* A record file's name without ".mod", which is what the player is shown. */
static std::string patch_display_name(const std::string &record) {
	std::string name = record;
	const size_t suffix = strlen(PATCH_RECORD_SUFFIX);
	if (name.size() > suffix) name.erase(name.size() - suffix);
	return name;
}

/* Every installed game, likeliest first.  The archive is named after the
 * game often enough that the right answer is usually already at the top;
 * the rest are there because that guess is only a guess. */
static bool prepare_patch_pick(const std::string &zip_path) {
	patch_zip_path = zip_path;
	patch_candidates.clear();
	patch_pick_index = 0;

	const std::string archive = install_base_name(zip_path);

	for (size_t i = 0; i < rom_list.size(); i++) {
		if (rom_list[i].is_zip || rom_list[i].is_partial) continue;

		PatchCandidate c;
		c.name  = rom_list[i].char_name();
		c.path  = rom_list[i].path;
		c.score = patch_name_match(archive.c_str(), c.name.c_str());
		patch_candidates.push_back(c);
	}

	/* Small list, drawn once: an insertion sort keeps games of equal score
	 * in the order the list already had them. */
	for (size_t i = 1; i < patch_candidates.size(); i++) {
		PatchCandidate value = patch_candidates[i];
		size_t j = i;
		while (j > 0 && patch_candidates[j - 1].score < value.score) {
			patch_candidates[j] = patch_candidates[j - 1];
			j--;
		}
		patch_candidates[j] = value;
	}

	return !patch_candidates.empty();
}

/* One column of names in a card, with the selected row picked out.  Shared
 * by "which game does this patch go on" and "which patch comes off". */
static void draw_pick_list(const char *title, const std::vector<std::string> &rows,
			   int index, const char *empty_text,
			   const char *enter_label) {
	const int padding = 24;
	const int row_h   = 28;
	const int visible = 8;

	const int count = (int)rows.size();
	int first = index - visible / 2;
	if (first > count - visible) first = count - visible;
	if (first < 0) first = 0;

	const int shown  = count < visible ? count : visible;
	const int width  = SCREEN_WIDTH - 200;
	const int height = padding * 2 + 40 + (shown > 0 ? shown : 1) * row_h + 34;
	const int left   = (SCREEN_WIDTH - width) / 2;
	const int top    = (SCREEN_HEIGHT - height) / 2;

	vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TH_SCRIM);
	th_shadow(left, top, width, height);
	th_card(left, top, width, height, TH_SURFACE, TH_BG);
	th_border(left, top, width, height, 1, TH_LINE);

	last_dialog_area.left   = left;
	last_dialog_area.top    = top;
	last_dialog_area.right  = left + width;
	last_dialog_area.bottom = top + height;

	th_text(left + padding, top + padding + TH_FONT_M, TH_TEXT, TH_FONT_M,
		th_fit(title, TH_FONT_M, width - padding * 2));

	if (count == 0) {
		th_text(left + padding, top + padding + 40 + TH_FONT_S,
			TH_TEXT_DIM, TH_FONT_S, empty_text);
	}
	for (int i = 0; i < shown; i++) {
		const int item = first + i;
		const int y    = top + padding + 40 + i * row_h;

		if (item == index) {
			vita2d_draw_rectangle(left + padding - 8, y,
				width - padding * 2 + 16, row_h, TH_ACCENT_SOFT);
			vita2d_draw_rectangle(left + padding - 8, y, 3, row_h, TH_ACCENT);
		}
		th_text(left + padding, y + TH_FONT_S + 6,
			item == index ? TH_TEXT : TH_TEXT_DIM, TH_FONT_S,
			th_fit(rows[item].c_str(), TH_FONT_S, width - padding * 2));
	}

	/* Which of a longer list is being looked at, so the eight rows on
	 * screen do not read as the whole library. */
	if (count > visible) {
		char position[32];
		snprintf(position, sizeof(position), "%d/%d", index + 1, count);
		th_text_right(left + width - padding, top + padding + TH_FONT_M,
			      TH_TEXT_DIM, TH_FONT_S, position);
	}

	vita2d_draw_rectangle(left + padding, top + height - 44,
		width - padding * 2, 1, TH_LINE);

	const int baseline = top + height - 22 + 6;
	int enter_w = th_hint_width(th_glyph_enter, enter_label, TH_FONT_S);
	int close_w = th_hint_width(th_glyph_cancel, ui_text(UI_PROMPT_CLOSE),
				    TH_FONT_S);
	int x = left + (width - (enter_w + 28 + close_w)) / 2;
	x += th_hint(x, baseline, th_glyph_enter, enter_label, TH_TEXT, TH_FONT_S);
	th_hint(x + 28, baseline, th_glyph_cancel, ui_text(UI_PROMPT_CLOSE),
		TH_TEXT_DIM, TH_FONT_S);
}

void draw_patch_pick() {
	std::vector<std::string> rows;
	for (size_t i = 0; i < patch_candidates.size(); i++)
		rows.push_back(patch_candidates[i].name);

	draw_pick_list(ui_text(UI_PATCH_TITLE), rows, patch_pick_index,
		       ui_text(UI_PATCH_NO_GAMES), ui_text(UI_PROMPT_YES));
}

/* Moves the selection, or leaves this screen.  UNKNOWN means stay. */
static ScreenState on_pick_event(int &index, int count, ScreenState on_enter,
				 ScreenState on_cancel) {
	int btn = read_buttons();

	if (btn & SCE_CTRL_UP) {
		if (index > 0) index--;
		return UNKNOWN;
	}
	if (btn & SCE_CTRL_DOWN) {
		if (index + 1 < count) index++;
		return UNKNOWN;
	}
	if (!(btn & SCE_CTRL_HOLD) && (btn & SCE_CTRL_CANCEL)) return on_cancel;
	if (!(btn & SCE_CTRL_HOLD) && (btn & SCE_CTRL_ENTER))
		return count > 0 ? on_enter : on_cancel;
	return UNKNOWN;
}

ScreenState on_patch_pick_event() {
	ScreenState state = on_pick_event(patch_pick_index,
					  (int)patch_candidates.size(),
					  PATCH_CONFIRM, MAIN_SCREEN);
	if (state != PATCH_CONFIRM) return state;

	patch_target_path = patch_candidates[patch_pick_index].path;
	patch_target_name = patch_candidates[patch_pick_index].name;

	char size_str[16];
	getSizeString(size_str, ZipHandler::installedSize(patch_zip_path));
	snprintf(patch_confirm_message, sizeof(patch_confirm_message),
		 ui_text(UI_PATCH_ASK),
		 install_base_name(patch_zip_path).c_str(),
		 patch_target_name.c_str(), size_str);
	return PATCH_CONFIRM;
}

ScreenState run_patch() {
	install_progress.bytes_done  = 0;
	install_progress.bytes_total = 0;
	install_progress.percent     = 0;
	install_progress.current_file.clear();

	const ZipInstallStatus status =
		ZipHandler::installPatch(patch_zip_path, patch_target_path,
					 install_progress_callback, NULL);

	if (status == ZIP_INSTALL_OK)
		snprintf(patch_message, sizeof(patch_message),
			 ui_text(UI_PATCH_OK), patch_target_name.c_str());
	else if (status == ZIP_INSTALL_EXISTS)
		snprintf(patch_message, sizeof(patch_message), "%s",
			 ui_text(UI_PATCH_EXISTS));
	else
		snprintf(patch_message, sizeof(patch_message), "%s",
			 ZipHandler::statusMessage(status));

	return PATCH_DONE;
}

/* The patches on the game whose settings screen this was opened from. */
void prepare_patch_list(int choose) {
	patch_list_index = 0;
	patch_applied.clear();
	patch_list_game.clear();
	patch_list_name.clear();

	if (choose < 0 || choose >= (int)rom_list.size()) return;
	if (rom_list[choose].is_zip) return;

	patch_list_game = rom_list[choose].path;
	patch_list_name = rom_list[choose].char_name();
	patch_applied   = ZipHandler::appliedPatches(patch_list_game);
}

void draw_patch_list() {
	std::vector<std::string> rows;
	for (size_t i = 0; i < patch_applied.size(); i++)
		rows.push_back(patch_display_name(patch_applied[i]));

	char title[128];
	snprintf(title, sizeof(title), ui_text(UI_PATCH_LIST_TITLE),
		 patch_list_name.c_str());

	draw_pick_list(title, rows, patch_list_index, ui_text(UI_PATCH_NONE),
		       ui_text(UI_PROMPT_REMOVE));
}

ScreenState on_patch_list_event() {
	ScreenState state = on_pick_event(patch_list_index,
					  (int)patch_applied.size(),
					  PATCH_REMOVE_CONFIRM, SETTING_MODE);
	if (state != PATCH_REMOVE_CONFIRM) return state;

	snprintf(patch_confirm_message, sizeof(patch_confirm_message),
		 ui_text(UI_PATCH_REMOVE_ASK),
		 patch_display_name(patch_applied[patch_list_index]).c_str(),
		 patch_list_name.c_str());
	return PATCH_REMOVE_CONFIRM;
}

ScreenState run_patch_remove() {
	if (patch_list_index < 0 || patch_list_index >= (int)patch_applied.size())
		return PATCH_DONE;

	const bool ok = ZipHandler::removePatch(patch_list_game,
						patch_applied[patch_list_index]);
	snprintf(patch_message, sizeof(patch_message), "%s",
		 ui_text(ok ? UI_PATCH_REMOVED : UI_PATCH_REMOVE_FAIL));

	patch_applied = ZipHandler::appliedPatches(patch_list_game);
	if (patch_list_index >= (int)patch_applied.size())
		patch_list_index = (int)patch_applied.size() - 1;
	if (patch_list_index < 0) patch_list_index = 0;

	return PATCH_REMOVE_DONE;
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

		/* Nothing installed and nothing searched for: the one moment
		 * where the launcher has to say something rather than show
		 * something. */
		if (rom_list.size() == 0 && rom_search.empty()) {
			draw_first_run();
		}
		else switch (mainscreen_list_mode) {
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
	if (state == FORMATS_MSG) {
		draw_formats_screen();
	}
	if (state == LAUNCHER_MSG) {
		draw_launcher_screen();
	}
	if (state == ABOUT_MSG) {
		draw_alert((char*)about_msg, FONT_SIZE);
	}
	/* The batch fetch is started from the settings screen, so that is what
	 * belongs behind it -- the per-game panel has nothing to do with it,
	 * and these states sit past PRINT_APPINFO in the enum only because they
	 * were added last. */
	/* States that belong to the settings screen rather than to a game, so
	 * the settings are what is drawn behind them.  They sit past
	 * PRINT_APPINFO in the enum only because they were added last. */
	bool from_settings = (state == COVERS_ALL_CONFIRM || state == COVERS_ALL_RUN ||
			      state == COVERS_ALL_DONE ||
			      state == CLEAN_CONFIRM || state == CLEAN_DONE ||
			      state == LOG_VIEW);
	if (from_settings) {
		draw_config();
	}
	else if (state >= PRINT_APPINFO) {
		draw_appinfo(state, choose);
	}

	/* Drawn after the settings behind it, so it sits on top. */
	if (state == LOG_VIEW) {
		draw_log_screen();
	}

	switch (state) {
	case START_MODE:
		break;
	case INSTALL_CONFIRM:
		draw_message(install_confirm_message, choose, FONT_SIZE);
		break;
	case INSTALL_RUN:
	case PATCH_RUN:
		draw_install_progress(install_progress);
		break;
	case PATCH_PICK:
		draw_patch_pick();
		break;
	case PATCH_CONFIRM:
		draw_message(patch_confirm_message, choose, FONT_SIZE);
		break;
	case PATCH_DONE:
		draw_alert(patch_message, FONT_SIZE);
		break;
	case PATCH_LIST:
		/* Over the settings screen it was opened from, so it is clear
		 * whose patches these are. */
		draw_slots(choose, -1);
		draw_patch_list();
		break;
	case PATCH_REMOVE_CONFIRM:
		draw_slots(choose, -1);
		draw_message(patch_confirm_message, choose, FONT_SIZE);
		break;
	case PATCH_REMOVE_RUN:
		draw_slots(choose, -1);
		break;
	case PATCH_REMOVE_DONE:
		draw_slots(choose, -1);
		draw_alert(patch_message, FONT_SIZE);
		break;
	case INSTALL_DONE:
	case INSTALL_FAIL:
		if (install_recovery == RECOVER_NONE) {
			draw_alert(install_message, FONT_SIZE);
		}
		else {
			/* Close on the left, the way out on the right, which is
			 * the order every other dialog here reads in. */
			dialog_box(install_message, ui_text(UI_PROMPT_CLOSE),
				   install_recovery == RECOVER_CLEAN_RETRY
					   ? ui_text(UI_CLEAN_RETRY)
					   : (install_resumable ? ui_text(UI_RETRY_RESUME)
							        : ui_text(UI_RETRY)),
				   FONT_SIZE);
		}
		break;
	case SETTING_MODE:
		draw_slots(choose, -1);
		break;
	case SAVES_DONE:
		/* Over the screen it was started from, so it is clear which
		 * game the report is about. */
		draw_slots(choose, -1);
		draw_alert(saves_result_message, FONT_SIZE);
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
	case CLEAN_CONFIRM:
		draw_message((char*)ui_text(UI_CLEAN_ASK), choose, FONT_SIZE);
		break;
	case CLEAN_DONE:
		draw_alert(clean_result_message, FONT_SIZE);
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
/* Volumes move in tens and stop at the ends rather than wrapping: running
 * past 100 back to 0 is a way to mute a game by accident. */
/* Opens or closes the launcher's own log, so turning the setting on starts
 * logging now rather than at the next start.  The engine gets the same
 * treatment through --log when a game is launched. */
static void apply_debug_log() {
	if (config.debug_log) {
		sceIoMkdir("ux0:data/onsemu", 0777);
		if (log_open(LAUNCHER_LOG_FILE))
			log_printf("launcher %s %s\n", ONS_BUILD_VERSION, ONS_BUILD_COMMIT);
	}
	else {
		log_close();
	}
}

static int volume_step(int value, int direction) {
	value += direction * 10;
	if (value > 100) value = 100;
	if (value < 0) value = 0;
	return value;
}

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
		if (config.use_btouch > 3)
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
	case 8:
		return CLEAN_CONFIRM;
	case 9:
		config.text_speed = (config.text_speed + 1) % 3;
		break;
	case 10:
		config.vol_bgm = volume_step(config.vol_bgm, +1);
		break;
	case 11:
		config.vol_se = volume_step(config.vol_se, +1);
		break;
	case 12:
		config.vol_voice = volume_step(config.vol_voice, +1);
		break;
	case 13:
		config.debug_log = !config.debug_log;
		apply_debug_log();
		break;
	case 14:
		config.theme = (config.theme + 1) % TH_MODE_COUNT;
		th_set_theme((ThemeMode)config.theme);
		break;
	case 15:
		return LOG_VIEW;
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

	for (int v = 0; v < config_visible_rows; v++) {
		int top = config_rows_top + v * config_row_height;
		if (p.y >= top && p.y < top + config_row_height)
			return config_scroll + v;
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
		case 9:
			config.text_speed = (config.text_speed + -1 + 3) % 3;
			break;
		case 10:
			config.vol_bgm = volume_step(config.vol_bgm, -1);
			break;
		case 11:
			config.vol_se = volume_step(config.vol_se, -1);
			break;
		case 12:
			config.vol_voice = volume_step(config.vol_voice, -1);
			break;
		case 13:
			config.debug_log = !config.debug_log;
			apply_debug_log();
			break;
		case 14:
			config.theme = (config.theme + 1) % TH_MODE_COUNT;
			th_set_theme((ThemeMode)config.theme);
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
		case 9:
			config.text_speed = (config.text_speed + 1 + 3) % 3;
			break;
		case 10:
			config.vol_bgm = volume_step(config.vol_bgm, 1);
			break;
		case 11:
			config.vol_se = volume_step(config.vol_se, 1);
			break;
		case 12:
			config.vol_voice = volume_step(config.vol_voice, 1);
			break;
		case 13:
			config.debug_log = !config.debug_log;
			apply_debug_log();
			break;
		case 14:
			config.theme = (config.theme + 1) % TH_MODE_COUNT;
			th_set_theme((ThemeMode)config.theme);
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
			else if (slot == SITTINGS_BACKUP || slot == SITTINGS_RESTORE) {
				return SAVES_DONE;
			}
			else if (slot == SITTINGS_PATCHES) {
				return PATCH_LIST;
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
		else if (slot == SITTINGS_BACKUP || slot == SITTINGS_RESTORE) {
			return SAVES_DONE;
		}
		else if (slot == SITTINGS_PATCHES) {
			return PATCH_LIST;
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

/* The controls list and the format list are two pages of one screen:
 * SQUARE turns to the other, anything else closes, so neither page is a
 * dead end and the pair costs one button. */
ScreenState on_help_event(ScreenState other_page) {
	while (1) {
		int btn = read_buttons();
		if (btn & SCE_CTRL_HOLD) {
			continue;
		}
		/* Left and right turn the page, which is what a two-page screen
		 * reads as, with the shoulders and square accepted as well:
		 * this is one screen with nothing else to move between, so every
		 * key that is not "close" may as well turn it. */
		if (btn & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT | SCE_CTRL_SQUARE |
			   SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER)) {
			return other_page;
		}
		if (btn & SCE_CTRL_ENTER || btn & SCE_CTRL_CANCEL) {
			break;
		}
		{
			point p;
			if (read_touchscreen(&p)) break;
		}
	}
	return MAIN_SCREEN;
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

/* Copying a game's saves out of its folder and back in again.
 *
 * Deleting a game to free space takes its saves with it, and there is
 * nowhere else on the card they are kept.  This is what makes deleting a
 * game recoverable, and the way to carry progress across a reinstall. */
static void run_backup_saves(int choose) {
	if (choose < 0 || choose >= (int)rom_list.size()) return;

	int count = 0;
	if (backup_saves(rom_list[choose].path, &count))
		snprintf(saves_result_message, sizeof(saves_result_message),
			 ui_text(UI_SAVES_BACKED_UP), count);
	else
		snprintf(saves_result_message, sizeof(saves_result_message), "%s",
			 count > 0 ? ui_text(UI_SAVES_FAIL) : ui_text(UI_SAVES_NONE));
}

static void run_restore_saves(int choose) {
	if (choose < 0 || choose >= (int)rom_list.size()) return;

	int count = 0;
	if (restore_saves(rom_list[choose].path, &count))
		snprintf(saves_result_message, sizeof(saves_result_message),
			 ui_text(UI_SAVES_RESTORED), count);
	else
		snprintf(saves_result_message, sizeof(saves_result_message), "%s",
			 count > 0 ? ui_text(UI_SAVES_FAIL) : ui_text(UI_SAVES_NO_BACKUP));
}

/* Clears what nothing needs any more and says what that came to.  The
 * numbers matter more than the act: someone doing this is short of space
 * and wants to know whether it was worth it. */
static void run_clean() {
	int files = 0;
	uint64_t freed = clean_temp_files(&files);

	if (files == 0) {
		snprintf(clean_result_message, sizeof(clean_result_message), "%s",
			 ui_text(UI_CLEAN_NOTHING));
		return;
	}

	char freed_str[16];
	getSizeString(freed_str, freed);
	snprintf(clean_result_message, sizeof(clean_result_message),
		 ui_text(UI_CLEAN_DONE), files, freed_str);
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
					/* An archive with no script in it is a patch,
					 * not a game: ask which game it goes on
					 * rather than failing at the end of an
					 * extraction. */
					if (ZipHandler::archiveKind(rom_list[choose].path)
						== PATCH_KIND_PATCH &&
					    prepare_patch_pick(rom_list[choose].path)) {
						new_state = PATCH_PICK;
					}
					else {
						prepare_install_confirm(choose);
						new_state = INSTALL_CONFIRM;
					}
				}
				//printf("%d \n", curr);
				break;
			case INSTALL_CONFIRM:
				new_state = on_message_event(choose, NULL, INSTALL_RUN,
					MAIN_SCREEN, MAIN_SCREEN, 1);
				break;
			case PATCH_PICK:
				new_state = on_patch_pick_event();
				break;
			case PATCH_CONFIRM:
				new_state = on_message_event(choose, NULL, PATCH_RUN,
					MAIN_SCREEN, PATCH_PICK, 1);
				break;
			case PATCH_RUN:
				new_state = run_patch();
				break;
			case PATCH_DONE:
				on_alert_event(MAIN_SCREEN);
				/* A patch changes what is in a game folder, so the
				 * list is read again rather than trusted. */
				return 1;
			case PATCH_LIST:
				new_state = on_patch_list_event();
				break;
			case PATCH_REMOVE_CONFIRM:
				new_state = on_message_event(choose, NULL, PATCH_REMOVE_RUN,
					PATCH_LIST, PATCH_LIST, 1);
				break;
			case PATCH_REMOVE_RUN:
				new_state = run_patch_remove();
				break;
			case PATCH_REMOVE_DONE:
				on_alert_event(PATCH_LIST);
				new_state = PATCH_LIST;
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
				if (install_recovery == RECOVER_NONE) {
					on_alert_event(MAIN_SCREEN);
					return 1;
				}
				new_state = on_message_event(choose, NULL, INSTALL_RUN,
							     MAIN_SCREEN, MAIN_SCREEN, 1);
				if (new_state == INSTALL_RUN) {
					/* Clearing first is the whole point of the
					 * offer when the card filled up; the
					 * install then resumes from its journal. */
					if (install_recovery == RECOVER_CLEAN_RETRY)
						run_clean();
					install_recovery = RECOVER_NONE;
					break;
				}
				return 1;
			case CONFIG_SCREEN:
				new_state = on_config_event();
				break;
			case HELP_MSG:
				new_state = on_help_event(FORMATS_MSG);
				break;
			case FORMATS_MSG:
				new_state = on_help_event(LAUNCHER_MSG);
				break;
			case LAUNCHER_MSG:
				new_state = on_help_event(HELP_MSG);
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
				if (new_state == PATCH_LIST) prepare_patch_list(choose);
				if (new_state == SAVES_DONE) {
					if (slot == SITTINGS_BACKUP) run_backup_saves(choose);
					else                         run_restore_saves(choose);
				}
				break;
			case SAVES_DONE:
				new_state = on_alert_event(SETTING_MODE);
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
			case LOG_VIEW:
				new_state = on_log_event();
				break;
			case CLEAN_CONFIRM:
				new_state = on_message_event(choose, NULL, CLEAN_DONE,
							    CONFIG_SCREEN, CONFIG_SCREEN, 1);
				/* The sweep is quick -- a handful of small files and
				 * one folder -- so it runs on the way to the report
				 * rather than through a screen of its own. */
				if (new_state == CLEAN_DONE) run_clean();
				break;
			case CLEAN_DONE:
				on_alert_event(CONFIG_SCREEN);
				/* The scan cache is gone, so the list is rebuilt from
				 * the card. */
				return 1;
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
	/* The two folders a game can be put in, made before anything is drawn
	 * -- the first-run panel tells the player they are there, and a folder
	 * you are told about and cannot find is worse than no instruction.
	 * ux0:onsemu was previously created only when an install ran. */
	sceIoMkdir(GAME_INSTALL_FOLDER, 0777);
	sceIoMkdir(GAME_ZIP_FOLDER, 0777);
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
		/* Before anything else worth logging happens. */
		apply_debug_log();
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
		/* The game's own choice if it has made one, else the
		 * launcher's.  Both end up as the one argument the engine
		 * reads, so the two settings cannot contradict each other on
		 * the way in. */
		static const char *touch_arg[] = {
			"", "use_front_only_touch", "use_front_back_touch",
			"use_back_only_touch", "use_not_touch"
		};
		int touch_choice = cmd[SITTINGS_TOUCH];
		if (touch_choice <= 0 || touch_choice > 4) {
			static const int from_config[] = { 4, 1, 2, 3 };
			int c = config.use_btouch;
			if (c < 0 || c > 3) c = 1;
			touch_choice = from_config[c];
		}
		cmd_str[cmd_num++] = (char*)"--touch-mode";
		cmd_str[cmd_num++] = (char*)touch_arg[touch_choice];

		/* What a game starts at when it has no envdata yet.  Held in
		 * statics because sceAppMgrLoadExec reads the array after this
		 * function returns. */
		static char speed_str[8], volumes_str[16];

		/* Two different things, and the engine is told which is which.
		 *
		 * The launcher's own settings are defaults: they seed a game
		 * that has never been played and leave a played one alone.
		 * What the player set for this game in particular is a
		 * decision about this game, made after playing it, so it wins
		 * over what the game saved -- otherwise the per-game rows
		 * would appear to do nothing on every game worth setting them
		 * for.  cmd[] holds 0 for "follow the launcher", the speed as
		 * 1-3, and a volume as tens plus one. */
		const bool own_speed   = cmd[SITTINGS_TEXT_SPEED] != 0;
		const bool own_volumes = cmd[SITTINGS_VOL_BGM] != 0 ||
					 cmd[SITTINGS_VOL_SE] != 0 ||
					 cmd[SITTINGS_VOL_VOICE] != 0;

		int speed = own_speed ? cmd[SITTINGS_TEXT_SPEED] - 1
				      : config.text_speed;
		int bgm   = cmd[SITTINGS_VOL_BGM]   ? (cmd[SITTINGS_VOL_BGM]   - 1) * 10
						    : config.vol_bgm;
		int se    = cmd[SITTINGS_VOL_SE]    ? (cmd[SITTINGS_VOL_SE]    - 1) * 10
						    : config.vol_se;
		int voice = cmd[SITTINGS_VOL_VOICE] ? (cmd[SITTINGS_VOL_VOICE] - 1) * 10
						    : config.vol_voice;

		snprintf(speed_str, sizeof(speed_str), "%d", speed);
		snprintf(volumes_str, sizeof(volumes_str), "%d,%d,%d",
			 bgm, se, voice);
		cmd_str[cmd_num++] = (char*)(own_speed ? "--set-text-speed"
						       : "--text-speed");
		cmd_str[cmd_num++] = speed_str;
		cmd_str[cmd_num++] = (char*)(own_volumes ? "--set-volumes"
							 : "--volumes");
		cmd_str[cmd_num++] = volumes_str;
		/* Why the speed setting appeared to do nothing at all: most
		 * scripts set their own, which used to win for the whole game
		 * whatever the player chose. */
		cmd_str[cmd_num++] = (char*)"--force-text-speed";

		if (config.debug_log) {
			cmd_str[cmd_num++] = (char*)"--log";
			cmd_str[cmd_num++] = (char*)ENGINE_LOG_FILE;
		}
		/* What the launcher worked out, then what the game asked for --
		 * in that order, so a hand-written ons_args overrides a guess. */
		cmd_num = appendAutoArgs(rom_path, cmd_str, cmd_num, CMD_MAX);
		cmd_num = appendGameArgs(rom_path, cmd_str, cmd_num, CMD_MAX);
		cmd_str[cmd_num] = NULL;
		for(int i=0; i<cmd_num; i++)
		{
			printf("cmd_str[%d] %s\n", i, cmd_str[i]);
			/* The arguments the engine is about to be started with are
			 * the first thing to look at when a game does not start,
			 * and the launcher's process ends here. */
			log_printf("launch argv[%d] = %s\n", i, cmd_str[i]);
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
