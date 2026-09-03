#ifndef __GUI_UTILS_H__
#define __GUI_UTILS_H__

#include <vita2d.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vitasdk.h>

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
} configure;



class RomInfo {
private:
	char* temp;
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
	RomInfo() {
		touch_area = { 0,0,0,0 };
		is_zip = 0;
		size = 0;
		icon = NULL;
		w = h = 0;
	}
	/* An archive found in ux0:data/game_zips. */
	RomInfo(string zip_path, uint64_t zip_size, int) {
		touch_area = { 0,0,0,0 };
		is_zip = 1;
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
			
		FILE *fp = fopen((path_ + "/caption.txt").c_str(), "r");
		if (fp)
		{
			char* chs = new char[256];
			fgets(chs, 512, fp);
			name = chs;
			free(chs);
			fclose(fp);
		}
		else
		{
			name = path_.substr(path_.find_last_of("/\\") + 1);
		}
			
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
	char *char_path() {
		temp = new char[path.length() + 1];
		strcpy(temp, path.c_str());
		return temp;
	}
	char *char_name() {
		temp = new char[name.length() + 1];
		strcpy(temp, name.c_str());
		return temp;
	}
	char *char_last_date() {
		temp = new char[last_date.length() + 1];
		strcpy(temp, last_date.c_str());
		return temp;
	}
	char *char_icon_path() {
		temp = new char[icon_path.length() + 1];
		strcpy(temp, icon_path.c_str());
		return temp;
	}
};

extern vector<RomInfo> rom_list;
extern configure config;
extern vita2d_font* font;

void load_config();
void save_config();

void init_input();
void lock_psbutton();
void unlock_psbutton();
int read_buttons();
int read_touchscreen(point *p);

int load_rom_list();
int parseOption(string &cmdstr, int(&cmd)[10], char *cmd_str[10], int flag);
void sittings_file(string path, string &str, char mode, int nowrite = 0);

void convertUtcToLocalTime(SceDateTime *time_local, SceDateTime *time_utc);
void getDateString(char string[24], int date_format, SceDateTime *time);
void getTimeString(char string[16], int time_format, SceDateTime *time);
#endif // __GUI_UTILS_H__
