/*#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/shellutil.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <psp2/io/dirent.h>*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>   /* strcasecmp, for sorting the list */
#include <algorithm>    /* std::sort */

#include "iniparser.h"
#include "GUI_Utils.h"
#include "GUI_Text.h"
#include "GUI_common.h"
#include "ZipHandler.h"

int SCE_CTRL_ENTER;
int SCE_CTRL_CANCEL;
char ICON_ENTER[4];
char ICON_CANCEL[4];
int ICONS_ROW = 4;
int ICONS_COL = 7;
int LIST_ROW = 8;

char *confirm_msg;
int confirm_msg_width;
char *close_msg;
int close_msg_width;

std::vector<RomInfo> rom_list;
configure config;

char *strdup(const char *c)
{
    char *dup = (char *)malloc(strlen(c) + 1);

    if (dup != NULL)
       strcpy(dup, c);

    return dup;
}

void init_input() {
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

	int enter_button;

	SceAppUtilInitParam init_param = { 0 };
	SceAppUtilBootParam boot_param = { 0 };
	sceAppUtilInit(&init_param, &boot_param);

	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &enter_button);

	if (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) {
		SCE_CTRL_ENTER = SCE_CTRL_CIRCLE;
		SCE_CTRL_CANCEL = SCE_CTRL_CROSS;
		strcpy(ICON_ENTER, ICON_CIRCLE);
		strcpy(ICON_CANCEL, ICON_CROSS);
	}
	else {
		SCE_CTRL_ENTER = SCE_CTRL_CROSS;
		SCE_CTRL_CANCEL = SCE_CTRL_CIRCLE;
		strcpy(ICON_ENTER, ICON_CROSS);
		strcpy(ICON_CANCEL, ICON_CIRCLE);
	}
}

void lock_psbutton() {
	sceShellUtilLock(SceShellUtilLockType(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN |
		SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU));
}

void unlock_psbutton() {
	sceShellUtilUnlock(SceShellUtilLockType(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN |
		SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU));
}

int read_buttons() {
	SceCtrlData pad = { 0 };
	static int old;
	static int hold_times;
	int curr, btn;

	sceCtrlPeekBufferPositive(0, &pad, 1);

	if (pad.ly < 0x10) {
		pad.buttons |= SCE_CTRL_UP;
	}
	else if (pad.ly > 0xef) {
		pad.buttons |= SCE_CTRL_DOWN;
	}
	else if (pad.lx < 0x10) {
		pad.buttons |= SCE_CTRL_LEFT;
	}
	else if (pad.lx > 0xef) {
		pad.buttons |= SCE_CTRL_RIGHT;
	}

	curr = pad.buttons;
	btn = pad.buttons & ~old;

	if (curr && old == curr) {
		hold_times += 1;
		if (hold_times >= 10) {
			btn = curr;
			hold_times = 8;
			btn |= SCE_CTRL_HOLD;
		}
	}

	else {
		hold_times = 0;
		old = curr;
	}
	return btn;
}

#define lerp(value, from_max, to_max) \
    ((((value * 10) * (to_max * 10)) / (from_max * 10)) / 10)

int read_touchscreen(point *p) {
	SceTouchData touch = { 0 };
	static int old_report_num = 0;
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

	// prevent hold
	if (old_report_num == touch.reportNum) {
		return 0;
	}

	old_report_num = touch.reportNum;

	if (!touch.reportNum) {
		return 0;
	}

	p->x = lerp(touch.report[0].x, 1919, SCREEN_WIDTH);
	p->y = lerp(touch.report[0].y, 1087, SCREEN_HEIGHT);

	return 1;
}

void load_config() {
	FILE   *file;
	if ((file = fopen(CONFIG_FILE, "r")) == NULL) {
DEFAULT:
		FILE * tmp = fopen(CONFIG_FILE, "w");
		fprintf(tmp,
			"[GUI]\n"
			"version = %d\n"
			"list_mode = list\n"
			"language = en\n"
			"[GUI_icon]\n"
			"row = 4\n"
			"column = 7\n"
			"[GUI_list]\n"
			"row = 5\n"
			"[GAME]\n"
			"use_btouch = 1\n",
			GUI_VERSION
			);
		fclose(tmp);
	}
	if (file) {
		fclose(file);
	}
	dictionary  *   ini;
	ini = iniparser_load(CONFIG_FILE);
	if (ini == NULL) {
		printf("cannot parse file: %s\n", CONFIG_FILE);
		return ;
	}
	config.ver = iniparser_getint(ini, "GUI:version", 0);
	if (config.ver < GUI_VERSION) {
		iniparser_freedict(ini);
		goto DEFAULT;
	}
	config.list_mode = strdup(iniparser_getstring(ini, "GUI:list_mode", "icon"));
	/* English by default: this fork exists to make the setup understandable
	 * to someone who does not already know the tool. "zh" restores
	 * upstream's Chinese labels. */
	config.language = ui_language_from_name(
		iniparser_getstring(ini, "GUI:language", "en"));
	ui_set_language((UILanguage)config.language);

	ICONS_ROW = iniparser_getint(ini, "GUI_icon:row", ICONS_ROW);
	config.icon_row = ICONS_ROW;
	ICONS_COL = iniparser_getint(ini, "GUI_icon:column", ICONS_COL);
	config.icon_col = ICONS_COL;
	LIST_ROW = iniparser_getint(ini, "GUI_list:row", LIST_ROW);
	config.list_row = LIST_ROW;
	config.use_btouch = iniparser_getint(ini, "GAME:use_btouch", 1);
	iniparser_freedict(ini);
}

void save_config() {
	dictionary  *   ini;
	ini = iniparser_load(CONFIG_FILE);
	if (ini == NULL) {
		printf("cannot parse file: %s\n", CONFIG_FILE);
		return;
	}
	
	iniparser_set(ini, "GUI:list_mode", config.list_mode);
	iniparser_set(ini, "GUI:language", ui_language_name((UILanguage)config.language));
	char itc[10];
	sprintf(itc, "%d", GUI_VERSION);
	iniparser_set(ini, "GUI:version", itc);
	sprintf(itc, "%d", config.icon_row);
	iniparser_set(ini, "GUI_icon:row", itc);
	sprintf(itc, "%d", config.icon_col);
	iniparser_set(ini, "GUI_icon:column", itc);
	sprintf(itc, "%d", config.list_row);
	iniparser_set(ini, "GUI_list:row", itc);
	sprintf(itc, "%d", config.use_btouch);
	iniparser_set(ini, "GAME:use_btouch", itc);

	FILE *fp = fopen(CONFIG_FILE, "w");
	iniparser_dump_ini(ini, fp);
	iniparser_freedict(ini);
	fclose(fp);
}

/* Case-insensitive by the name the player sees. */
static bool by_display_name(const RomInfo &a, const RomInfo &b) {
	const string &x = a.name.empty() ? a.path : a.name;
	const string &y = b.name.empty() ? b.path : b.name;
	return strcasecmp(x.c_str(), y.c_str()) < 0;
}

int load_rom_list() {

	string drives[3] = { "ux0:/onsemu" ,"ur0:/onsemu" ,"uma0:/onsemu" };
	string file_name;
	string temp;
	SceUID dfd;

	/* Rebuilding means every row loads its image again, so let go of the
	 * ones already held -- the list is rebuilt after every install and every
	 * cover fetch, and a texture per row per reload adds up. */
	for (size_t i = 0; i < rom_list.size(); i++)
		if (rom_list[i].icon) vita2d_free_texture(rom_list[i].icon);
	rom_list.clear();
	for (int i = 0; i < 3; i++) {
		dfd = sceIoDopen(drives[i].c_str());
		if (dfd >= 0) {
			int res = 0;
			do {
				SceIoDirent dir;
				memset(&dir, 0, sizeof(SceIoDirent));
				res = sceIoDread(dfd, &dir);
				file_name = dir.d_name;
				if (res > 0) {
					temp = drives[i] + "/" + file_name;
					/* A card written on a mac or unpacked by a desktop
					 * tool carries bookkeeping folders -- .Trashes,
					 * __MACOSX, .Spotlight-V100 -- which are not games
					 * and should not take up a row. */
					bool junk = file_name.empty() ||
						file_name[0] == '.' ||
						file_name.compare(0, 9, "__MACOSX") == 0;
					if (SCE_S_ISDIR(dir.d_stat.st_mode) && !junk) {
						rom_list.push_back(RomInfo(temp));
					}
				}
			} while (res > 0);
			sceIoDclose(dfd);
		}
	}

	/* Alphabetical, so a game keeps its place in the list between runs.
	 * Directory order is whatever the filesystem happens to return, which
	 * moves as games are added and removed. */
	std::sort(rom_list.begin(), rom_list.end(), by_display_name);

	/* Archives waiting in ux0:data/game_zips are listed after the installed
	 * games, so dropping a .zip on the card is enough to see it here. */
	std::vector<ZipEntryInfo> zips = ZipHandler::scanZipFolder();
	for (size_t i = 0; i < zips.size(); i++)
		rom_list.push_back(RomInfo(zips[i].path, zips[i].file_size, 1));

	return rom_list.size();
}

int parseOption(string &cmdstr,int (&cmd)[10],char *cmd_str[10],int flag = 0) {
	if (!flag) {
		string tmp = "";
		int i = 0;
		for (int j = 0; j < 10; j++) cmd[j] = 0;
		while (i < cmdstr.length()) {
			if (cmdstr[i] == '-' && cmdstr[i + 1] == '-') {
				i += 2;
				while (cmdstr[i] != ' ' && i < cmdstr.length()) {
					tmp += cmdstr[i];
					i++;
				}
				if (tmp != "") {
					if (tmp == "fullscreen") {
						cmd[0] = 1;
					}
					else if (tmp == "window") {
						cmd[0] = 0;
					}
					else if (tmp == "fontcache") {
						cmd[1] = 1;
					}
					else if (tmp == "render-font-outline") {
						cmd[2] = 1;
					}
					else if (tmp == "textbox") {
						cmd[3] = 1;
					}
					/* Slot 4 is not a toggle but a choice:
					 * 0 auto (the engine detects), 1 shift-jis,
					 * 2 gbk.  Older settings files only ever
					 * contain enc:sjis, which still reads as 1. */
					else if (tmp == "enc:sjis") {
						cmd[4] = 1;
					}
					else if (tmp == "enc:gbk") {
						cmd[4] = 2;
					}
					else if (tmp == "enc:auto") {
						cmd[4] = 0;
					}
					else {
						printf(" unknown option %s\n", tmp.c_str());
					}
					tmp = "";
				}
			}
			i++;
		}
		return 0;
	}
	else {
		cmdstr = "";
		int index_ = 0;
		//cmd_str[index_] = (char*)"ons-jh-psvita";
		//index_++;
		if (cmd[0]){
			cmd_str[index_] = (char*)"--fullscreen";
			cmdstr += cmd_str[index_];
			index_++;
		}			
		else{
			cmd_str[index_] = (char*)"--window";
			cmdstr += cmd_str[index_];
			index_++;
		}			
		if (cmd[1]){
			cmd_str[index_] = (char*)"--fontcache";
			cmdstr += " ";
			cmdstr += cmd_str[index_];
			index_++;
		}	
		if (cmd[2]){
			cmd_str[index_] = (char*)"--render-font-outline";
			cmdstr += " ";
			cmdstr += cmd_str[index_];
			index_++;
		}
		if (cmd[3]){
			cmd_str[index_] = (char*)"--textbox";
			cmdstr += " ";
			cmdstr += cmd_str[index_];
			index_++;
		}
		if (cmd[4]) {
			cmd_str[index_] = (char*)(cmd[4] == 2 ? "--enc:gbk" : "--enc:sjis");
			cmdstr += " ";
			cmdstr += cmd_str[index_];
			index_++;
		}
			
		return index_;
	}
}

void sittings_file(string path,string &str,char mode,int nowrite) {
	string filename = path + "/" + SITTINGS_FILE;
	if (mode == 'r') {
		str = "";
		FILE *fp = fopen(filename.c_str(), "r");
		if (NULL == fp)
		{
			if (!nowrite) {
				str = "--window --fontcache --textbox";
				sittings_file(path, str, 'w');
			}
			else {
				str = "";
			}
			return;
		}
		char* chs=new char[512];
		fgets(chs,512,fp);
		str = chs;
		fclose(fp);
	}
	else if (mode == 'w') {
		FILE *fp = fopen(filename.c_str(), "w");
		fputs(str.c_str(),fp);
		fclose(fp);
	}

}

void convertUtcToLocalTime(SceDateTime *time_local, SceDateTime *time_utc) {
	SceRtcTick tick;
	sceRtcGetTick(time_utc, &tick);
	sceRtcConvertUtcToLocalTime(&tick, &tick);
	sceRtcSetTick(time_local, &tick);
}

void getDateString(char string[24], int date_format, SceDateTime *time) {
	SceDateTime time_local;
	convertUtcToLocalTime(&time_local, time);

	switch (date_format) {
	case SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD:
		snprintf(string, 24, "%04d/%02d/%02d", time_local.year, time_local.month, time_local.day);
		break;

	case SCE_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY:
		snprintf(string, 24, "%02d/%02d/%04d", time_local.day, time_local.month, time_local.year);
		break;

	case SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY:
		snprintf(string, 24, "%02d/%02d/%04d", time_local.month, time_local.day, time_local.year);
		break;
	}
}

//time_format is 12 or 24
void getTimeString(char string[16], int time_format, SceDateTime *time) {
	SceDateTime time_local;
	convertUtcToLocalTime(&time_local, time);

	switch (time_format) {
	case 12:
	{
		int hour = ((time_local.hour == 0) ? 12 : time_local.hour);
		snprintf(string, 16, "%02d:%02d %s", (time_local.hour > 12) ? (time_local.hour - 12) : hour,
			time_local.minute, time_local.hour >= 12 ? "PM" : "AM");
		break;
	}

	case 24:
		snprintf(string, 16, "%02d:%02d", time_local.hour, time_local.minute);
		break;
	}
}
