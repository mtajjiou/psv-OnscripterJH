#ifndef __GUI_UTILS_H__
#define __GUI_UTILS_H__

#include <vita2d.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vitasdk.h>

#include "GUI_common.h"   /* CMD_OPTS, and the screen geometry */
#include "ZipHandler.h"   /* isPartialInstall, for a folder mid-install */

using namespace std;

typedef struct point {
	int x;
	int y;
} point;

typedef struct rectangle {
	int left;
	int top;
	int right;
	int bottom;
} rectangle;


typedef struct configure {
	int ver;
	char *list_mode;
	int use_btouch;
	int icon_row;
	int icon_col;
	int list_row;
	int language;      /* UILanguage; the launcher's interface language */
	int sort_mode;     /* SortMode: how the list is ordered */
	/* Defaults handed to a game the first time it is played, so a
	 * comfortable text speed and volume are set once rather than in every
	 * game's own menu.  A game that has been played keeps its own. */
	int text_speed;    /* 0 slow, 1 normal, 2 fast */
	int vol_bgm;       /* 0-100 */
	int vol_se;
	int vol_voice;
	/* Whether both binaries write what they print to ux0:data/onsemu/.
	 * Off by default: it costs a write per line, and a log nobody asked
	 * for is a log nobody reads. */
	int debug_log;
} configure;

/* How the game list is ordered.  Name is free -- it is already in memory.
 * Recent reads the stamp each game carries.  Size has to walk each folder,
 * so it is measured the first time it is asked for rather than at startup:
 * see measure_sizes(). */
enum SortMode {
	SORT_NAME = 0,
	SORT_RECENT,
	SORT_SIZE,
	SORT_COUNT
};



class RomInfo {
private:
public:
	string path;
	string name;
	string last_date;
	string icon_path;
	vita2d_texture *icon;
	point pos;
	rectangle touch_area;
	uint64_t size;
	int w;
	int h;
	/* A row can be either an installed game folder or a .zip waiting to be
	 * installed, so both show up in one list.  For an archive, path holds
	 * the .zip and is_zip is 1. */
	int is_zip;
	/* An install that stopped part way: the folder exists but is not a
	 * game yet.  It is listed so it can be finished or deleted, and
	 * refuses to launch. */
	int is_partial;
	RomInfo() {
		touch_area = { 0,0,0,0 };
		is_zip = 0;
		is_partial = 0;
		size = 0;
		icon = NULL;
		w = h = 0;
	}
	/* An archive found in ux0:data/game_zips. */
	RomInfo(string zip_path, uint64_t zip_size, int) {
		touch_area = { 0,0,0,0 };
		is_zip = 1;
		is_partial = 0;
		size = zip_size;
		path = zip_path;
		icon = vita2d_load_PNG_file("app0:/sce_sys/icon1.png");
		name = zip_path.substr(zip_path.find_last_of("/\\") + 1);
		if (name.length() > 4) name.erase(name.length() - 4);  /* ".zip" */
		w = icon ? sceGxmTextureGetWidth(&icon->gxm_tex) : 0;
		h = icon ? sceGxmTextureGetHeight(&icon->gxm_tex) : 0;
	}
	RomInfo(string path_) {
		touch_area = { 0,0,0,0 };
		is_zip = 0;
		is_partial = ZipHandler::isPartialInstall(path_) ? 1 : 0;
		size = 0;
		path = path_;
		/* A cover fetched from vndb comes first, then a hand-placed
		 * icon.png, then the launcher's own icon.  vndb serves jpeg for
		 * most covers and png for a few, so both are tried. */
		icon = NULL;
		const char *candidates[3] = { "/cover.png", "/cover.jpg", "/icon.png" };
		for (int c = 0; c < 3 && !icon; c++){
			string candidate = path_ + candidates[c];
			SceUID fd = sceIoOpen(candidate.c_str(), SCE_O_RDONLY, 0777);
			if (fd < 0) continue;
			sceIoClose(fd);

			icon_path = candidate;
			if (candidate.length() > 4 &&
			    candidate.compare(candidate.length() - 4, 4, ".jpg") == 0)
				icon = vita2d_load_JPEG_file(candidate.c_str());
			else
				icon = vita2d_load_PNG_file(candidate.c_str());
		}
		if (!icon){
			icon_path = path_ + "/icon.png";
			icon = vita2d_load_PNG_file("app0:/sce_sys/icon1.png");
		}
			
		/* Written by the launcher when the game is started; absent until
		 * it has been. */
		FILE *stamp = fopen((path_ + "/lastplayed.txt").c_str(), "r");
		if (stamp) {
			char line[64] = { 0 };
			if (fgets(line, sizeof(line), stamp)) {
				char *end = line + strlen(line);
				while (end > line && (end[-1] == '\n' || end[-1] == '\r'))
					*--end = '\0';
				last_date = line;
			}
			fclose(stamp);
		}

		/* A game can name itself in caption.txt.  This read had three
		 * faults worth naming, since all three are the kind that work
		 * until the day they do not: it read up to 511 bytes into a 256
		 * byte buffer, released a new[] allocation with free(), and kept
		 * the newline fgets leaves behind, so every game named this way
		 * carried one in its title. */
		name.clear();

		FILE *fp = fopen((path_ + "/caption.txt").c_str(), "r");
		if (fp) {
			char caption[256];
			if (fgets(caption, sizeof(caption), fp) != NULL) {
				char *end = caption + strlen(caption);
				while (end > caption &&
				       (end[-1] == '\n' || end[-1] == '\r' ||
					end[-1] == ' '  || end[-1] == '\t'))
					*--end = '\0';
				name = caption;
			}
			fclose(fp);
		}

		/* An empty or unreadable caption.txt leaves the folder's own name,
		 * rather than a game with no name at all. */
		if (name.empty())
			name = path_.substr(path_.find_last_of("/\\") + 1);
			
		w = sceGxmTextureGetWidth(&icon->gxm_tex);
		h = sceGxmTextureGetHeight(&icon->gxm_tex);
	}
	static char *to_char(string str) {
		char* temp1 = new char[str.length() + 1];
		strcpy(temp1, str.c_str());
		return temp1;
	}
	static char *to_char(int num) {
		char* temp1 = new char[9];
		sprintf(temp1, "%d", num);
		return temp1;
	}
	/* The string this row already holds, as a char *.
	 *
	 * These are called from the drawing code, which runs every frame, and
	 * they used to hand back a fresh new[] that nobody freed -- a leak of
	 * a path per row per frame for as long as the launcher was open, which
	 * is enough to run it out of heap while someone reads a game's panel.
	 *
	 * The pointer is into the row's own string and stays valid until the
	 * list is rebuilt.  Nothing writes through it: the callers draw it, or
	 * pass it to getPathInfo, which only reads. */
	char *char_path()      { return (char *)path.c_str(); }
	char *char_name()      { return (char *)name.c_str(); }
	char *char_last_date() { return (char *)last_date.c_str(); }
	char *char_icon_path() { return (char *)icon_path.c_str(); }
};

/* Every row found on the card.  rom_list is what is on screen: the same
 * rows filtered by the search and put in the chosen order.  The rows in it
 * are copies that share their texture with the master, so only rom_list_all
 * ever frees one. */
extern vector<RomInfo> rom_list_all;
extern vector<RomInfo> rom_list;

/* The active search, or empty.  Matching is on the name the player sees. */
extern string rom_search;

/* Rebuilds rom_list from rom_list_all for the current search and order. */
void apply_view();
/* Walks the folders once to fill in RomInfo::size, for sorting by it. */
void measure_sizes();

/* Sweeps the files nothing needs any more: the folder the bubble installer
 * builds in, the engine's tmp.mus in each game folder, and the launcher's
 * own scan cache, which is rebuilt on the next start.  Returns the bytes
 * freed and, through *files, how many things were removed.  Saves, scripts
 * and half-finished installs are never touched. */
uint64_t clean_temp_files(int *files);

/* Where a game's saves are copied to, one folder per game. */
#define SAVE_BACKUP_FOLDER "ux0:data/onsemu/saves"

/* Copies a game's saves out to SAVE_BACKUP_FOLDER, or back in again.  What
 * counts as a save is save<N>.dat and the three files the engine keeps
 * beside them: gloval.sav, envdata and kidoku.dat -- read flags and global
 * variables are as much a player's progress as a save slot is.
 *
 * Returns 1 on success, 0 if nothing was found or a copy failed, and puts
 * the number of files copied in *count. */
int backup_saves(const std::string &game_path, int *count);
int restore_saves(const std::string &game_path, int *count);
extern configure config;
extern vita2d_font* font;

void load_config();
void save_config();

void init_input();
void lock_psbutton();
void unlock_psbutton();
int read_buttons();
int read_touchscreen(point *p);
/* The live state, for telling a tap from a hold. */
int read_touch_raw(point *p);

int load_rom_list();
int parseOption(string &cmdstr, int(&cmd)[CMD_OPTS], char *cmd_str[], int flag);
/* Appends what the launcher works out for a game -- a font, mostly. */
int appendAutoArgs(const string &game_path, char *cmd_str[], int index, int max);
/* Appends a game's own ons_args, if it has one.  Returns the new count. */
int appendGameArgs(const string &game_path, char *cmd_str[], int index, int max);
void sittings_file(string path, string &str, char mode, int nowrite = 0);

void convertUtcToLocalTime(SceDateTime *time_local, SceDateTime *time_utc);
void getDateString(char string[24], int date_format, SceDateTime *time);
void getTimeString(char string[16], int time_format, SceDateTime *time);
#endif // __GUI_UTILS_H__
