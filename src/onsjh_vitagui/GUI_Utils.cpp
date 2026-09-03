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
#include "filesystem.h"   /* checkFileExist, getPathInfo */
#include "GUI_Utils.h"
#include "GUI_Text.h"
#include "GUI_common.h"
#include "ZipHandler.h"
#include "zipreader.h"
#include "manifest.h"   /* zip_is_script_name: one definition of "this is a game" */

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

std::vector<RomInfo> rom_list_all;
std::vector<RomInfo> rom_list;
std::string rom_search;
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

/* Where the finger is right now, or 0 if there is none.
 *
 * read_touchscreen() below reports a touch once, which is what a button-like
 * tap wants.  Telling a tap from a hold needs the opposite: the state as it
 * is, every frame, for as long as it lasts. */
int read_touch_raw(point *p) {
	SceTouchData touch = { 0 };
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

	if (!touch.reportNum) return 0;

	p->x = lerp(touch.report[0].x, 1919, SCREEN_WIDTH);
	p->y = lerp(touch.report[0].y, 1087, SCREEN_HEIGHT);
	return 1;
}

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
			"sort = name\n"
			"debug_log = 0\n"
			"[GUI_icon]\n"
			"row = 4\n"
			"column = 7\n"
			"[GUI_list]\n"
			"row = 5\n"
			"[GAME]\n"
			"use_btouch = 1\n"
			"text_speed = 1\n"
			"vol_bgm = 100\n"
			"vol_se = 100\n"
			"vol_voice = 100\n",
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

	const char *sort = iniparser_getstring(ini, "GUI:sort", "name");
	config.sort_mode = (sort && sort[0] == 'r') ? SORT_RECENT
			 : ((sort && sort[0] == 's') ? SORT_SIZE : SORT_NAME);

	ICONS_ROW = iniparser_getint(ini, "GUI_icon:row", ICONS_ROW);
	config.icon_row = ICONS_ROW;
	ICONS_COL = iniparser_getint(ini, "GUI_icon:column", ICONS_COL);
	config.icon_col = ICONS_COL;
	LIST_ROW = iniparser_getint(ini, "GUI_list:row", LIST_ROW);
	config.list_row = LIST_ROW;
	config.use_btouch = iniparser_getint(ini, "GAME:use_btouch", 1);
	config.text_speed = iniparser_getint(ini, "GAME:text_speed", 1);
	config.vol_bgm    = iniparser_getint(ini, "GAME:vol_bgm", 100);
	config.vol_se     = iniparser_getint(ini, "GAME:vol_se", 100);
	config.vol_voice  = iniparser_getint(ini, "GAME:vol_voice", 100);
	config.debug_log  = iniparser_getint(ini, "GUI:debug_log", 0);
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
	iniparser_set(ini, "GUI:sort",
		config.sort_mode == SORT_RECENT ? "recent"
			: (config.sort_mode == SORT_SIZE ? "size" : "name"));
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
	sprintf(itc, "%d", config.text_speed);
	iniparser_set(ini, "GAME:text_speed", itc);
	sprintf(itc, "%d", config.vol_bgm);
	iniparser_set(ini, "GAME:vol_bgm", itc);
	sprintf(itc, "%d", config.vol_se);
	iniparser_set(ini, "GAME:vol_se", itc);
	sprintf(itc, "%d", config.vol_voice);
	iniparser_set(ini, "GAME:vol_voice", itc);
	sprintf(itc, "%d", config.debug_log);
	iniparser_set(ini, "GUI:debug_log", itc);

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

/* Most recently played first.  A game that has never been played has no
 * stamp and sorts to the end rather than to the top, where it would push
 * what you actually play out of sight.  The stamps are written as
 * YYYY/MM/DD HH:MM, so comparing them as text compares them as dates. */
static bool by_recently_played(const RomInfo &a, const RomInfo &b) {
	if (a.last_date.empty() != b.last_date.empty())
		return b.last_date.empty();
	if (a.last_date != b.last_date)
		return a.last_date > b.last_date;
	return by_display_name(a, b);
}

/* Biggest first: the reason to sort by size is to find what to delete. */
static bool by_size(const RomInfo &a, const RomInfo &b) {
	if (a.size != b.size) return a.size > b.size;
	return by_display_name(a, b);
}

/* Does this row match what was searched for?  Case-insensitive, anywhere in
 * the name -- a player looking for "tsuki" should not have to know whether
 * the folder starts with it. */
static bool matches_search(const RomInfo &rom) {
	if (rom_search.empty()) return true;

	const string &name = rom.name.empty() ? rom.path : rom.name;
	const size_t hay = name.length(), needle = rom_search.length();
	if (needle > hay) return false;

	for (size_t i = 0; i + needle <= hay; i++)
		if (strncasecmp(name.c_str() + i, rom_search.c_str(), needle) == 0)
			return true;
	return false;
}

/* Does this folder hold a game, rather than hold a folder that does?
 *
 * The test is the same one the archive side uses -- zip_is_script_name --
 * so a layout that installs from a .zip is recognised identically when it
 * was copied across by hand instead.  A folder carrying only the archives
 * counts too: some releases keep the script inside arc.sar.
 */
static bool folder_has_script(const string &dir) {
	SceUID dfd = sceIoDopen(dir.c_str());
	if (dfd < 0) return false;

	bool found = false;
	int res = 0;
	do {
		SceIoDirent entry;
		memset(&entry, 0, sizeof(SceIoDirent));
		res = sceIoDread(dfd, &entry);
		if (res <= 0 || SCE_S_ISDIR(entry.d_stat.st_mode)) continue;

		if (zip_is_script_name(entry.d_name) ||
		    strcasecmp(entry.d_name, "arc.nsa") == 0 ||
		    strcasecmp(entry.d_name, "arc.sar") == 0) {
			found = true;
			break;
		}
	} while (res > 0);

	sceIoDclose(dfd);
	return found;
}

/* Where the game really is.  A folder copied across by hand often wraps the
 * game one or two levels deep -- onsemu/MyGame/MyGame/nscript.dat, or the
 * folder the archive was unpacked into inside the folder that was made for
 * it -- and until now none of those were found at all.
 *
 * Returns the folder holding the script, or an empty string if there is
 * none within reach.  Two levels: deeper than that and it is not a wrapped
 * game, it is a folder of games. */
static string resolve_game_root(const string &dir, int depth) {
	if (folder_has_script(dir)) return dir;
	if (depth <= 0) return string();

	SceUID dfd = sceIoDopen(dir.c_str());
	if (dfd < 0) return string();

	string found;
	int res = 0;
	do {
		SceIoDirent entry;
		memset(&entry, 0, sizeof(SceIoDirent));
		res = sceIoDread(dfd, &entry);
		if (res <= 0 || !SCE_S_ISDIR(entry.d_stat.st_mode)) continue;
		if (entry.d_name[0] == '.') continue;

		found = resolve_game_root(dir + "/" + entry.d_name, depth - 1);
		if (!found.empty()) break;
	} while (res > 0);

	sceIoDclose(dfd);
	return found;
}

void measure_sizes() {
	/* One directory walk per game, done when sorting by size is first
	 * asked for rather than at every startup: on a card full of games it
	 * is the slowest thing the launcher can do, and most of the time
	 * nobody wants it. */
	manifest cache;
	manifest_init(&cache);
	manifest_load(&cache, MANIFEST_FILE);

	bool learned = false;
	for (size_t i = 0; i < rom_list_all.size(); i++) {
		if (rom_list_all[i].is_zip || rom_list_all[i].size > 0) continue;

		uint64_t size = 0;
		uint32_t folders = 0, files = 0;
		getPathInfo(rom_list_all[i].char_path(), &size, &folders, &files);
		rom_list_all[i].size = size;

		/* Written down against the folder it was measured from, so the
		 * walk is paid for once rather than every time the list is
		 * ordered by size. */
		for (int j = 0; j < cache.count; j++) {
			if (rom_list_all[i].path.compare(0, strlen(cache.entries[j].folder),
							 cache.entries[j].folder) != 0)
				continue;
			cache.entries[j].size = size;
			learned = true;
			break;
		}
	}

	if (learned) manifest_save(&cache, MANIFEST_FILE);
	manifest_free(&cache);
}

void apply_view() {
	if (config.sort_mode == SORT_SIZE) measure_sizes();

	rom_list.clear();
	for (size_t i = 0; i < rom_list_all.size(); i++)
		if (matches_search(rom_list_all[i]))
			rom_list.push_back(rom_list_all[i]);

	switch (config.sort_mode) {
	case SORT_RECENT:
		std::sort(rom_list.begin(), rom_list.end(), by_recently_played);
		break;
	case SORT_SIZE:
		std::sort(rom_list.begin(), rom_list.end(), by_size);
		break;
	case SORT_NAME:
	default:
		std::sort(rom_list.begin(), rom_list.end(), by_display_name);
		break;
	}
}

/* What says a folder has not changed since it was written down.
 *
 * The modification time of the folder itself: adding, removing or renaming
 * anything directly inside it moves that time, which is exactly the set of
 * changes that would make a cached name or root wrong.  A file edited
 * deeper inside does not, and does not need to. */
static string folder_stamp(const string &dir) {
	SceIoStat stat;
	memset(&stat, 0, sizeof(stat));
	if (sceIoGetstat(dir.c_str(), &stat) < 0) return string();

	char text[MANIFEST_STAMP_MAX];
	snprintf(text, sizeof(text), "%04d%02d%02d%02d%02d%02d",
		 stat.st_mtime.year, stat.st_mtime.month, stat.st_mtime.day,
		 stat.st_mtime.hour, stat.st_mtime.minute, stat.st_mtime.second);
	return text;
}

/* How big a file is, or 0 if it is not there. */
static uint64_t file_size_of(const string &path)
{
	SceIoStat st;
	memset(&st, 0, sizeof(st));
	if (sceIoGetstat(path.c_str(), &st) < 0) return 0;
	return (uint64_t)st.st_size;
}

static uint64_t remove_file_counting(const string &path, int *files)
{
	uint64_t size = file_size_of(path);
	if (size == 0 && !checkFileExist(path.c_str())) return 0;
	if (sceIoRemove(path.c_str()) < 0) return 0;
	if (files) (*files)++;
	return size;
}

uint64_t clean_temp_files(int *files)
{
	uint64_t freed = 0;
	int removed = 0;

	/* The folder the bubble installer builds in.  It clears this on its
	 * next run, which is no help to someone who made one bubble a month
	 * ago and is now out of space. */
	if (checkFolderExist(PACKAGE_TEMP)) {
		uint64_t size = 0;
		uint32_t folders = 0, count = 0;
		char temp_path[64];
		snprintf(temp_path, sizeof(temp_path), "%s", PACKAGE_TEMP);
		getPathInfo(temp_path, &size, &folders, &count);
		if (removePath(PACKAGE_TEMP) > 0) {
			freed += size;
			removed += (int)count;
		}
	}

	/* tmp.mus: the engine writes a game's MIDI out to a real file to hand
	 * it to the mixer, and the copy stays behind when the game exits. */
	string drives[3] = { "ux0:/onsemu", "ur0:/onsemu", "uma0:/onsemu" };
	for (int i = 0; i < 3; i++) {
		SceUID dfd = sceIoDopen(drives[i].c_str());
		if (dfd < 0) continue;
		int res = 0;
		do {
			SceIoDirent dir;
			memset(&dir, 0, sizeof(SceIoDirent));
			res = sceIoDread(dfd, &dir);
			if (res <= 0) break;
			if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
			if (dir.d_name[0] == '.') continue;

			const string game = drives[i] + "/" + dir.d_name;
			freed += remove_file_counting(game + "/tmp.mus", &removed);
		} while (res > 0);
		sceIoClose(dfd);
	}

	/* The scan cache.  Removing it costs one slower start and is the one
	 * thing to try when the list shows a game that is no longer there. */
	freed += remove_file_counting(MANIFEST_FILE, &removed);

	if (files) *files = removed;
	return freed;
}

/* The names a save can have.  Slots first, then the three files the engine
 * keeps beside them. */
static bool is_save_name(const char *name)
{
	if (strncasecmp(name, "save", 4) == 0) {
		const char *p = name + 4;
		if (*p < '0' || *p > '9') return false;
		while (*p >= '0' && *p <= '9') p++;
		return strcasecmp(p, ".dat") == 0;
	}
	return strcasecmp(name, "gloval.sav") == 0 ||
	       strcasecmp(name, "envdata") == 0 ||
	       strcasecmp(name, "kidoku.dat") == 0;
}

/* Copies every save from one folder to the other, creating the destination.
 * Used both ways round: backing up and restoring differ only in which is
 * which. */
static int copy_saves(const string &from, const string &to, int *count)
{
	int copied = 0;
	bool failed = false;

	SceUID dfd = sceIoDopen(from.c_str());
	if (dfd < 0) {
		if (count) *count = 0;
		return 0;
	}

	int res = 0;
	do {
		SceIoDirent entry;
		memset(&entry, 0, sizeof(entry));
		res = sceIoDread(dfd, &entry);
		if (res <= 0) break;
		if (SCE_S_ISDIR(entry.d_stat.st_mode)) continue;
		if (!is_save_name(entry.d_name)) continue;

		/* The destination is made only once something is going into it,
		 * so a game with no saves leaves no empty folder behind. */
		if (copied == 0) {
			sceIoMkdir(SAVE_BACKUP_FOLDER, 0777);
			sceIoMkdir(to.c_str(), 0777);
		}

		const string src = from + "/" + entry.d_name;
		const string dst = to + "/" + entry.d_name;
		if (copyFile(src.c_str(), dst.c_str()) < 0) failed = true;
		else copied++;
	} while (res > 0);
	sceIoClose(dfd);

	if (count) *count = copied;
	return (copied > 0 && !failed) ? 1 : 0;
}

/* The backup folder for a game, named after the game's own folder. */
static string save_backup_path(const string &game_path)
{
	string name = game_path;
	size_t slash = name.find_last_of("/\\");
	if (slash != string::npos) name = name.substr(slash + 1);
	if (name.empty()) name = "game";
	return string(SAVE_BACKUP_FOLDER) + "/" + name;
}

int backup_saves(const string &game_path, int *count)
{
	return copy_saves(game_path, save_backup_path(game_path), count);
}

int restore_saves(const string &game_path, int *count)
{
	/* Straight back into the game folder: the engine reads its saves from
	 * where the script is, so there is nowhere else for them to go. */
	return copy_saves(save_backup_path(game_path), game_path, count);
}

int load_rom_list() {

	/* What was learned last time, and what is true now.  Building a second
	 * one rather than editing the first is what drops games that are no
	 * longer on the card. */
	manifest cache, fresh;
	manifest_init(&cache);
	manifest_init(&fresh);
	manifest_load(&cache, MANIFEST_FILE);

	string drives[3] = { "ux0:/onsemu" ,"ur0:/onsemu" ,"uma0:/onsemu" };
	string file_name;
	string temp;
	SceUID dfd;

	/* Rebuilding means every row loads its image again, so let go of the
	 * ones already held -- the list is rebuilt after every install and every
	 * cover fetch, and a texture per row per reload adds up.  Only the
	 * master frees: the rows on screen are copies sharing its textures. */
	for (size_t i = 0; i < rom_list_all.size(); i++)
		if (rom_list_all[i].icon) vita2d_free_texture(rom_list_all[i].icon);
	rom_list_all.clear();
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
						const string stamp = folder_stamp(temp);
						const manifest_entry *known =
							manifest_find(&cache, temp.c_str(),
								      stamp.empty() ? NULL : stamp.c_str());

						/* The game may be wrapped in a folder or two.
						 * Finding that out means walking directories, so
						 * it is remembered: the answer only changes when
						 * the folder itself does. */
						string root = known ? known->root
								    : resolve_game_root(temp, 2);
						if (root.empty()) root = temp;

						RomInfo rom(root);
						/* Named for the folder the player made, not the
						 * one inside it -- unless the game named itself
						 * with a caption. */
						if (root != temp &&
						    !checkFileExist((root + "/caption.txt").c_str()))
							rom.name = file_name;
						if (known && known->size > 0) rom.size = known->size;

						manifest_entry entry;
						memset(&entry, 0, sizeof(entry));
						snprintf(entry.folder, sizeof(entry.folder), "%s",
							 temp.c_str());
						snprintf(entry.root, sizeof(entry.root), "%s",
							 root.c_str());
						snprintf(entry.name, sizeof(entry.name), "%s",
							 rom.name.c_str());
						snprintf(entry.stamp, sizeof(entry.stamp), "%s",
							 stamp.c_str());
						entry.size = rom.size;
						manifest_put(&fresh, &entry);

						rom_list_all.push_back(rom);
					}
				}
			} while (res > 0);
			sceIoDclose(dfd);
		}
	}

	/* Archives waiting in ux0:data/game_zips, so dropping a .zip on the
	 * card is enough to see it here. */
	std::vector<ZipEntryInfo> zips = ZipHandler::scanZipFolder();
	for (size_t i = 0; i < zips.size(); i++)
		rom_list_all.push_back(RomInfo(zips[i].path, zips[i].file_size, 1));

	manifest_save(&fresh, MANIFEST_FILE);
	manifest_free(&cache);
	manifest_free(&fresh);

	/* Ordered and filtered into what is actually shown. */
	apply_view();

	return rom_list.size();
}

/* Arguments the launcher works out for itself.
 *
 * Deliberately short, and shorter than it once needed to be: the engine now
 * detects the script's code page, decodes the video formats the hardware
 * refuses, and understands backtick text, so none of those need a flag any
 * more. What is left is what the engine cannot recover from on its own.
 *
 * The font. A game with no default.ttf beside it gets a font path that does
 * not exist -- ONScripter's fallback to a system font is behind
 * USE_FONTCONFIG, which is a desktop build option and is not compiled in
 * here -- so it has nothing to draw text with. The launcher ships a font
 * for its own interface, and points the engine at it.
 *
 * These go on before a game's own ons_args, so anything written by hand
 * overrides what was guessed.
 */
int appendAutoArgs(const string &game_path, char *cmd_str[], int index, int max) {
	if (!checkFileExist((game_path + "/default.ttf").c_str()) && index + 2 < max) {
		printf("auto: no default.ttf in %s, using the launcher's font\n",
		       game_path.c_str());
		cmd_str[index++] = (char *)"--font";
		cmd_str[index++] = (char *)"app0:default.ttf";
	}

	return index;
}

/* A game's own arguments, from an ons_args file beside it.
 *
 * Some games need a flag the launcher has no setting for -- a font size, a
 * window mode, an option this fork has never heard of -- and the honest way
 * to allow that is to let the game carry its own. One argument per line, or
 * several separated by spaces; a line starting with # is a note to whoever
 * reads the file later.
 *
 * They go on last, so a game that insists on something overrides the
 * launcher's idea of it. */
int appendGameArgs(const string &game_path, char *cmd_str[], int index, int max) {
	FILE *file = fopen((game_path + "/ons_args").c_str(), "r");
	if (file == NULL) return index;

	char line[512];
	while (fgets(line, sizeof(line), file) != NULL) {
		char *cursor = line;

		while (*cursor == ' ' || *cursor == '\t') cursor++;
		if (*cursor == '#' || *cursor == '\n' || *cursor == '\r' || *cursor == '\0')
			continue;

		while (*cursor != '\0') {
			while (*cursor == ' ' || *cursor == '\t' ||
			       *cursor == '\n' || *cursor == '\r') cursor++;
			if (*cursor == '\0') break;

			char *start = cursor;
			while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' &&
			       *cursor != '\n' && *cursor != '\r') cursor++;

			const size_t length = (size_t)(cursor - start);
			if (*cursor != '\0') *cursor++ = '\0';

			/* One slot kept back for the NULL the argument list ends
			 * with; silently writing past it is how this array was one
			 * setting away from trouble already. */
			if (index >= max - 1) {
				printf("ons_args: too many arguments, ignoring the rest\n");
				fclose(file);
				return index;
			}

			char *copy = new char[length + 1];
			memcpy(copy, start, length);
			copy[length] = '\0';
			cmd_str[index++] = copy;
		}
	}

	fclose(file);
	return index;
}

int parseOption(string &cmdstr,int (&cmd)[10],char *cmd_str[],int flag = 0) {
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
					/* Touch, per game.  Absent means the
					 * launcher's own setting still decides,
					 * which is what every settings file
					 * written before this option says. */
					else if (tmp == "touch:front") {
						cmd[SITTINGS_TOUCH] = 1;
					}
					else if (tmp == "touch:both") {
						cmd[SITTINGS_TOUCH] = 2;
					}
					else if (tmp == "touch:back") {
						cmd[SITTINGS_TOUCH] = 3;
					}
					else if (tmp == "touch:off") {
						cmd[SITTINGS_TOUCH] = 4;
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
		if (cmd[SITTINGS_TOUCH]) {
			static const char *touch_opt[] = {
				"", "--touch:front", "--touch:both",
				"--touch:back", "--touch:off"
			};
			/* Written into the settings file so the choice comes
			 * back next time, but not handed to the engine: the
			 * engine takes it as --touch-mode, which the launcher
			 * appends once, after this. */
			cmdstr += " ";
			cmdstr += touch_opt[cmd[SITTINGS_TOUCH]];
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
