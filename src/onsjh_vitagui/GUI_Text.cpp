/* -*- C++ -*-
 *
 *  GUI_Text.cpp -- see GUI_Text.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <string.h>

#include "GUI_Text.h"

namespace {

UILanguage g_language = UI_LANG_EN;

/* Indexed by UIStringId, then by UILanguage.  The Chinese column is
 * upstream's own wording wherever upstream had one, so a reader who used the
 * original launcher sees the labels they know. */
const char *g_strings[UI_STRING_COUNT][UI_LANG_COUNT] = {
    /* UI_SET_FULLSCREEN     */ { "Force full screen",      "强制全屏幕" },
    /* UI_SET_FONTCACHE      */ { "Cache font",             "缓存字体" },
    /* UI_SET_TEXTSHADOW     */ { "Text shadow",            "文字阴影" },
    /* UI_SET_TEXTBOX        */ { "Show text box",          "显示文字框" },
    /* UI_SET_ENCODING       */ { "Script encoding",        "文字编码" },
    /* UI_SET_TOUCH          */ { "Touch panels",           "触摸操作" },
    /* UI_SET_BACKUP         */ { "Back up saves",           "备份存档" },
    /* UI_SET_RESTORE        */ { "Restore saves",           "恢复存档" },
    /* UI_CFG_TEXT_SPEED_GAME*/ { "Text speed",             "文字速度" },
    /* UI_CFG_VOL_BGM_GAME   */ { "BGM volume",             "音乐音量" },
    /* UI_CFG_VOL_SE_GAME    */ { "SE volume",              "音效音量" },
    /* UI_CFG_VOL_VOICE_GAME */ { "Voice volume",           "语音音量" },
    /* UI_SET_RESET          */ { "Reset to defaults",      "恢复默认设置" },
    /* UI_SET_RETURN         */ { "Back",                   "返回" },

    /* UI_ON                 */ { "on",                     "开启" },
    /* UI_OFF                */ { "off",                    "关闭" },
    /* UI_AUTO               */ { "auto",                   "自动" },
    /* UI_ENC_SJIS           */ { "japanese",               "日文" },
    /* UI_ENC_GBK            */ { "chinese",                "中文" },

    /* UI_CFG_GRAPHIC_MODE   */ { "Game list style",        "显示" },
    /* UI_CFG_LIST           */ { "list",                   "列表" },
    /* UI_CFG_ICON           */ { "icons",                  "图标" },
    /* UI_CFG_ICON_ROW       */ { "Icon rows",              "图标行数" },
    /* UI_CFG_ICON_COL       */ { "Icon columns",           "图标列数" },
    /* UI_CFG_LIST_ROW       */ { "List rows",              "列表行数" },
    /* UI_CFG_TOUCH_MODE     */ { "Touch control",          "触摸控制" },
    /* UI_TOUCH_OFF          */ { "off",                    "关闭" },
    /* UI_TOUCH_FRONT        */ { "front only",             "仅前触屏" },
    /* UI_TOUCH_BOTH         */ { "front and back",         "前后触屏" },
    /* UI_TOUCH_BACK         */ { "back only",              "仅后触板" },
    /* UI_TOUCH_DEFAULT      */ { "default",                "默认" },
    /* UI_CFG_LANGUAGE       */ { "Language",               "界面语言" },

    /* UI_CFG_FETCH_COVERS   */ { "Fetch missing covers",   "\xE8\x8E\xB7\xE5\x8F\x96\xE7\xBC\xBA\xE5\xB0\x91\xE7\x9A\x84\xE5\xB0\x81\xE9\x9D\xA2" },
    /* UI_CFG_CLEAN          */ { "Clear temporary files",   "清理临时文件" },
    /* UI_CFG_TEXT_SPEED     */ { "Text speed (new games)",  "文字速度(新游戏)" },
    /* UI_CFG_VOL_BGM        */ { "BGM volume (new games)",  "音乐音量(新游戏)" },
    /* UI_CFG_VOL_SE         */ { "SE volume (new games)",   "音效音量(新游戏)" },
    /* UI_CFG_VOL_VOICE      */ { "Voice volume (new games)","语音音量(新游戏)" },
    /* UI_CFG_DEBUG_LOG      */ { "Write a debug log",       "写入调试日志" },
    /* UI_CFG_THEME          */ { "Theme",                   "界面主题" },
    /* UI_THEME_DARK         */ { "dark",                    "深色" },
    /* UI_THEME_LIGHT        */ { "light",                   "浅色" },
    /* UI_CFG_VIEW_LOG       */ { "View the log",            "查看日志" },
    /* UI_LOG_OPEN           */ { "open",                    "打开" },
    /* UI_LOG_EMPTY          */ { "This log is empty. Turn on \"Write a debug log\"\nand run the game again.",
                                  "日志为空。请先开启\"写入调试日志\"，\n然后重新运行游戏。" },
    /* UI_LOG_ENGINE         */ { "engine",                  "引擎" },
    /* UI_LOG_LAUNCHER       */ { "launcher",                "启动器" },
    /* UI_LOG_CRASH          */ { "last crash",              "崩溃报告" },
    /* UI_PROMPT_SWITCH      */ { "other log",               "切换日志" },

    /* UI_RETRY              */ { "retry",                   "重试" },
    /* UI_RETRY_RESUME       */ { "resume",                  "继续安装" },
    /* UI_CLEAN_RETRY        */ { "clear and retry",         "清理后重试" },
    /* UI_FAIL_SPACE_HINT    */ { "\n\nClearing temporary files may free enough to finish.",
                                  "\n\n清理临时文件可能释放足够空间。" },
    /* UI_FAIL_RESUME_HINT   */ { "\n\n%s is already installed; retrying carries on from there.",
                                  "\n\n已安装 %s，重试将从中断处继续。" },
    /* UI_SPEED_SLOW         */ { "slow",                    "慢" },
    /* UI_SPEED_NORMAL       */ { "normal",                  "普通" },
    /* UI_SPEED_FAST         */ { "fast",                    "快" },

    /* UI_CFG_SORT           */ { "Order",                  "\xE6\x8E\x92\xE5\xBA\x8F" },
    /* UI_SORT_NAME          */ { "name",                   "\xE5\x90\x8D\xE7\xA7\xB0" },
    /* UI_SORT_RECENT        */ { "recently played",        "\xE6\x9C\x80\xE8\xBF\x91\xE6\xB8\xB8\xE7\x8E\xA9" },
    /* UI_SORT_SIZE          */ { "size",                   "\xE5\xA4\xA7\xE5\xB0\x8F" },

    /* UI_BTN_START          */ { "start",                  "启动" },
    /* UI_BTN_CONFIG         */ { "config",                 "设置" },
    /* UI_BTN_INSTALL        */ { "install",                "安装" },
    /* UI_BTN_COVER          */ { "cover",                  "封面" },
    /* UI_BTN_DELETE         */ { "delete",                 "\xE5\x88\xA0\xE9\x99\xA4" },
    /* UI_PROMPT_NO          */ { "no",                     "取消" },
    /* UI_PROMPT_YES         */ { "yes",                    "确定" },
    /* UI_PROMPT_CLOSE       */ { "close",                  "关闭" },
    /* The footer is drawn as button glyphs with these labels beside them,
     * so the hints are the words alone now.  What is left of the old line
     * is the position, which is still a format. */
    /* UI_FOOTER_HINTS       */ { "%d / %d",                "%d / %d" },
    /* UI_HINT_SETTINGS      */ { "settings",               "设置" },
    /* UI_HINT_HELP          */ { "help",                   "帮助" },
    /* UI_HINT_ABOUT         */ { "about",                  "关于" },

    /* UI_INSTALLING         */ { "Installing...",          "安装中..." },
    /* UI_NOT_IMPLEMENTED    */ { "[not implemented yet]",  "[暂未开放的功能]" },
    /* UI_MAKE_PACKAGE_ASK   */ { "Create a shortcut bubble for this game?",
                                  "是否要生成快捷启动气泡？" },
    /* UI_MAKE_PACKAGE_RUN   */ { "Creating the bubble... do not touch anything...",
                                  "正在生成气泡中...请勿操作..." },
    /* UI_MAKE_PACKAGE_OK    */ { "Bubble created.",        "快捷启动气泡生成完毕！" },
    /* UI_MAKE_PACKAGE_FAIL  */ { "Could not create the bubble.",
                                  "快捷启动气泡生成失败..." },

    /* UI_COVER_ASK          */ { "Look this game up on vndb.org and fetch its cover?",
                                  "\xE4\xBB\x8E vndb.org \xE8\x8E\xB7\xE5\x8F\x96\xE6\x9C\xAC\xE6\xB8\xB8\xE6\x88\x8F\xE7\x9A\x84\xE5\xB0\x81\xE9\x9D\xA2\xEF\xBC\x9F" },
    /* UI_COVER_RUN          */ { "Asking vndb.org...",
                                  "\xE6\xAD\xA3\xE5\x9C\xA8\xE8\xAF\xB7\xE6\xB1\x82 vndb.org..." },
    /* UI_COVER_OK           */ { "Cover saved.",           "\xE5\xB0\x81\xE9\x9D\xA2\xE5\xB7\xB2\xE4\xBF\x9D\xE5\xAD\x98" },
    /* UI_COVER_NOT_FOUND    */ { "vndb.org has no cover under that name.",
                                  "vndb.org \xE4\xB8\x8A\xE6\xB2\xA1\xE6\x9C\x89\xE8\xBF\x99\xE4\xB8\xAA\xE5\x90\x8D\xE7\xA7\xB0" },
    /* UI_COVER_NO_NET       */ { "No network connection.", "\xE6\xB2\xA1\xE6\x9C\x89\xE7\xBD\x91\xE7\xBB\x9C\xE8\xBF\x9E\xE6\x8E\xA5" },
    /* UI_COVER_FAIL         */ { "Could not reach vndb.org.",
                                  "\xE6\x97\xA0\xE6\xB3\x95\xE8\xBF\x9E\xE6\x8E\xA5 vndb.org" },
    /* UI_COVER_WRITE_FAIL   */ { "The cover could not be saved.",
                                  "\xE5\xB0\x81\xE9\x9D\xA2\xE4\xBF\x9D\xE5\xAD\x98\xE5\xA4\xB1\xE8\xB4\xA5" },

    /* UI_COVERS_ALL_ASK     */ { "Look up every game without a cover on vndb.org?",
                                  "\xE4\xB8\xBA\xE6\x89\x80\xE6\x9C\x89\xE7\xBC\xBA\xE5\xB0\x91\xE5\xB0\x81\xE9\x9D\xA2\xE7\x9A\x84\xE6\xB8\xB8\xE6\x88\x8F\xE8\x8E\xB7\xE5\x8F\x96\xEF\xBC\x9F" },
    /* UI_COVERS_ALL_RUN     */ { "Fetching covers",        "\xE6\xAD\xA3\xE5\x9C\xA8\xE8\x8E\xB7\xE5\x8F\x96\xE5\xB0\x81\xE9\x9D\xA2" },
    /* UI_COVERS_ALL_DONE    */ { "%d fetched, %d not found, %d already had one",
                                  "\xE6\x88\x90\xE5\x8A\x9F %d\xEF\xBC\x8C\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0 %d\xEF\xBC\x8C\xE5\xB7\xB2\xE6\x9C\x89 %d" },
    /* UI_COVERS_START       */ { "start",                  "\xE5\xBC\x80\xE5\xA7\x8B" },

    /* UI_CLEAN_ASK          */ { "Remove temporary files?\n\n  the bubble installer's folder\n  leftover tmp.mus files\n  the launcher's scan cache\n\n  saves and games are not touched",
                                  "清理临时文件？\n\n  气泡安装临时目录\n  残留的 tmp.mus\n  启动器扫描缓存\n\n  存档与游戏不受影响" },
    /* UI_CLEAN_RUN          */ { "Clearing...",             "正在清理..." },
    /* UI_CLEAN_DONE         */ { "%d files removed, %s freed",
                                  "已删除 %d 个文件，释放 %s" },
    /* UI_CLEAN_NOTHING      */ { "Nothing to clear.",       "没有需要清理的文件" },
    /* UI_CLEAN_START        */ { "clear",                   "清理" },

    /* UI_SAVES_BACKED_UP    */ { "%d save files backed up.", "已备份 %d 个存档文件" },
    /* UI_SAVES_RESTORED     */ { "%d save files restored.",  "已恢复 %d 个存档文件" },
    /* UI_SAVES_NONE         */ { "This game has no saves yet.",
                                  "该游戏还没有存档" },
    /* UI_SAVES_NO_BACKUP    */ { "No backup for this game.", "没有该游戏的备份" },
    /* UI_SAVES_FAIL         */ { "The saves could not be copied.",
                                  "存档复制失败" },

    /* UI_DELETE_ASK         */ { "Delete %s?\n\n"
                                  "  %s in %u files\n"
                                  "  from ux0:onsemu\n\n"
                                  "  saved games in the folder go too",
                                  "\xE5\x88\xA0\xE9\x99\xA4 %s\xEF\xBC\x9F\n\n"
                                  "  %s\xEF\xBC\x8C%u \xE4\xB8\xAA\xE6\x96\x87\xE4\xBB\xB6\n"
                                  "  \xE4\xBD\x8D\xE4\xBA\x8E ux0:onsemu\n\n"
                                  "  \xE5\xAD\x98\xE6\xA1\xA3\xE4\xB9\x9F\xE4\xBC\x9A\xE4\xB8\x80\xE5\xB9\xB6\xE5\x88\xA0\xE9\x99\xA4" },
    /* UI_DELETE_ASK_ZIP     */ { "Delete the archive %s?\n\n"
                                  "  %s\n\n"
                                  "  the installed game is not touched",
                                  "\xE5\x88\xA0\xE9\x99\xA4\xE5\x8E\x8B\xE7\xBC\xA9\xE5\x8C\x85 %s\xEF\xBC\x9F\n\n  %s\n\n"
                                  "  \xE5\xB7\xB2\xE5\xAE\x89\xE8\xA3\x85\xE7\x9A\x84\xE6\xB8\xB8\xE6\x88\x8F\xE4\xB8\x8D\xE5\x8F\x97\xE5\xBD\xB1\xE5\x93\x8D" },
    /* UI_DELETE_RUN         */ { "Deleting...",            "\xE5\x88\xA0\xE9\x99\xA4\xE4\xB8\xAD..." },
    /* UI_DELETE_OK          */ { "Deleted. %s free now.",  "\xE5\x88\xA0\xE9\x99\xA4\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C\xE5\x89\xA9\xE4\xBD\x99 %s" },
    /* UI_DELETE_FAIL        */ { "Could not delete it. Some files may remain.",
                                  "\xE5\x88\xA0\xE9\x99\xA4\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x8C\xE9\x83\xA8\xE5\x88\x86\xE6\x96\x87\xE4\xBB\xB6\xE5\x8F\xAF\xE8\x83\xBD\xE4\xBB\x8D\xE5\x9C\xA8" },
    /* UI_FREE_SPACE         */ { "%s free",                "\xE5\x89\xA9\xE4\xBD\x99 %s" },

    /* UI_LAST_PLAYED        */ { "last played %s",        "\xE4\xB8\x8A\xE6\xAC\xA1\xE6\xB8\xB8\xE7\x8E\xA9 %s" },
    /* UI_NEVER_PLAYED       */ { "not played yet",         "\xE5\xB0\x9A\xE6\x9C\xAA\xE6\xB8\xB8\xE7\x8E\xA9" },
    /* UI_ZIP_INFO           */ { "archive %s, unpacks to about %s",
                                  "\xE5\x8E\x8B\xE7\xBC\xA9\xE5\x8C\x85 %s\xEF\xBC\x8C\xE8\xA7\xA3\xE5\x8E\x8B\xE7\xBA\xA6 %s" },
    /* UI_ZIP_INFO_TIME      */ { "about %d min %02d s to install",
                                  "\xE5\xAE\x89\xE8\xA3\x85\xE7\xBA\xA6\xE9\x9C\x80 %d \xE5\x88\x86 %02d \xE7\xA7\x92" },

    /* UI_SEARCH_TITLE       */ { "Find a game",            "\xE6\x9F\xA5\xE6\x89\xBE\xE6\xB8\xB8\xE6\x88\x8F" },
    /* UI_SEARCH_ACTIVE      */ { "\"%s\"",                   "\"%s\"" },
    /* UI_SEARCH_EMPTY       */ { "Nothing matches \"%s\".\n\nPress %s to clear the search.",
                                  "\xE6\xB2\xA1\xE6\x9C\x89\xE5\x8C\xB9\xE9\x85\x8D \"%s\" \xE7\x9A\x84\xE6\xB8\xB8\xE6\x88\x8F\xE3\x80\x82\n\n\xE6\x8C\x89 %s \xE6\xB8\x85\xE9\x99\xA4\xE6\x90\x9C\xE7\xB4\xA2" },
    /* UI_HINT_SEARCH        */ { "find",                   "\xE6\x9F\xA5\xE6\x89\xBE" },

    /* UI_UNFINISHED         */ { "unfinished",             "\xE6\x9C\xAA\xE5\xAE\x8C\xE6\x88\x90" },
    /* UI_RESUME_ASK         */ { "Finish installing this game?\n\n"
                                  "  %s\n"
                                  "  %s of %s already unpacked\n\n"
                                  "  %s free",
                                  "\xE7\xBB\xA7\xE7\xBB\xAD\xE5\xAE\x89\xE8\xA3\x85\xE6\xAD\xA4\xE6\xB8\xB8\xE6\x88\x8F\xEF\xBC\x9F\n\n"
                                  "  %s\n  \xE5\xB7\xB2\xE8\xA7\xA3\xE5\x8E\x8B %s / %s\n\n  \xE5\x89\xA9\xE4\xBD\x99 %s" },
    /* UI_RESUME_BLOCKED     */ { "This game was not finished installing.\n\n"
                                  "Install its .zip again to finish it, or delete it.",
                                  "\xE6\xAD\xA4\xE6\xB8\xB8\xE6\x88\x8F\xE5\xB0\x9A\xE6\x9C\xAA\xE5\xAE\x89\xE8\xA3\x85\xE5\xAE\x8C\xE6\x88\x90\xE3\x80\x82\n\n"
                                  "\xE8\xAF\xB7\xE9\x87\x8D\xE6\x96\xB0\xE5\xAE\x89\xE8\xA3\x85\xE5\x8E\x8B\xE7\xBC\xA9\xE5\x8C\x85\xEF\xBC\x8C\xE6\x88\x96\xE5\x88\xA0\xE9\x99\xA4\xE5\xAE\x83" },

    /* UI_HELP_TITLE         */ { "In-game controls",       "\xE4\xBD\xBF\xE7\x94\xA8\xE5\xB8\xAE\xE5\x8A\xA9(\xE6\xB8\xB8\xE6\x88\x8F\xE5\x86\x85)" },
    /* UI_HELP_CONFIRM       */ { "confirm, continue",      "\xE7\xA1\xAE\xE8\xAE\xA4/\xE7\xBB\xA7\xE7\xBB\xAD" },
    /* UI_HELP_SKIP          */ { "hold to fast-forward",   "\xE6\x8C\x89\xE4\xBD\x8F\xE5\xBF\xAB\xE8\xBF\x9B" },
    /* UI_HELP_AUTO          */ { "auto mode",              "\xE8\x87\xAA\xE5\x8A\xA8\xE6\xA8\xA1\xE5\xBC\x8F" },
    /* UI_HELP_MENU          */ { "menu, leaves the backlog",
                                  "\xE8\x8F\x9C\xE5\x8D\x95/\xE5\x85\xB3\xE9\x97\xAD\xE5\x9B\x9E\xE6\x83\xB3\xE6\xA8\xA1\xE5\xBC\x8F" },
    /* UI_HELP_SKIP_PAGE     */ { "skip this page",         "\xE5\xBF\xAB\xE8\xBF\x9B\xE5\xBD\x93\xE5\x89\x8D\xE9\xA1\xB5" },
    /* UI_HELP_TOGGLE_SKIP   */ { "start or stop skipping", "\xE5\xBC\x80\xE5\xA7\x8B/\xE5\x81\x9C\xE6\xAD\xA2\xE5\xBF\xAB\xE8\xBF\x9B" },
    /* UI_HELP_BACKLOG       */ { "backlog",                "\xE5\x9B\x9E\xE6\x83\xB3\xE6\xA8\xA1\xE5\xBC\x8F" },
    /* UI_HELP_CURSOR        */ { "move between choices",   "\xE9\x80\x89\xE9\xA1\xB9\xE9\x80\x89\xE6\x8B\xA9" },
    /* UI_HELP_STICK         */ { "same as the d-pad",      "\xE7\xAD\x89\xE5\x90\x8C\xE6\x96\xB9\xE5\x90\x91\xE9\x94\xAE" },
    /* UI_HELP_OVERLAY       */ { "show this list in game", "\xE6\xB8\xB8\xE6\x88\x8F\xE4\xB8\xAD\xE6\x98\xBE\xE7\xA4\xBA\xE6\x9C\xAC\xE5\x88\x97\xE8\xA1\xA8" },

    /* UI_LAUNCHER_TITLE     */ { "The launcher",            "启动器操作" },
    /* UI_LAUNCHER_START     */ { "start the game, install an archive",
                                  "启动游戏 / 安装压缩包" },
    /* UI_LAUNCHER_PANEL     */ { "settings, saves and cover for one game",
                                  "单个游戏的设置、存档与封面" },
    /* UI_LAUNCHER_SETTINGS  */ { "settings for everything",  "全局设置" },
    /* UI_LAUNCHER_HELP      */ { "this screen",              "本帮助界面" },
    /* UI_LAUNCHER_SEARCH    */ { "search the library",       "搜索游戏" },
    /* UI_LAUNCHER_ABOUT     */ { "about this build",         "关于本版本" },
    /* UI_LAUNCHER_MOVE      */ { "move through the library", "浏览游戏列表" },
    /* UI_WHERE_TITLE        */ { "Where games go",           "游戏放在哪里" },
    /* UI_WHERE_ZIP          */ { "ux0:data/game_zips/  -- archives to install",
                                  "ux0:data/game_zips/ -- 待安装的压缩包" },
    /* UI_WHERE_FOLDER       */ { "ux0:onsemu/  -- installed games, one folder each",
                                  "ux0:onsemu/ -- 已安装的游戏，每个一个文件夹" },

    /* UI_FIRST_RUN_TITLE    */ { "No games yet",             "还没有游戏" },
    /* UI_FIRST_RUN_BODY     */ { "Copy a game to the memory card and it appears here.\n\n"
                                  "  A .zip goes in       ux0:data/game_zips/\n"
                                  "  and is installed from this screen.\n\n"
                                  "  A game folder goes in ux0:onsemu/\n"
                                  "  and runs as it is.\n\n"
                                  "Both folders have been created for you.",
                                  "\xE5\xB0\x86\xE6\xB8\xB8\xE6\x88\x8F\xE5\xA4\x8D\xE5\x88\xB6\xE5\x88\xB0\xE5\xAD\x98\xE5\x82\xA8\xE5\x8D\xA1\xE5\x90\x8E\xE5\x8D\xB3\xE5\x8F\xAF\xE6\x98\xBE\xE7\xA4\xBA\xE3\x80\x82\n\n"
                                  "  \xE5\x8E\x8B\xE7\xBC\xA9\xE5\x8C\x85\xEF\xBC\x9A ux0:data/game_zips/\n"
                                  "  \xE5\x9C\xA8\xE6\x9C\xAC\xE7\x95\x8C\xE9\x9D\xA2\xE5\xAE\x89\xE8\xA3\x85\xE3\x80\x82\n\n"
                                  "  \xE6\xB8\xB8\xE6\x88\x8F\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xEF\xBC\x9A ux0:onsemu/\n"
                                  "  \xE5\x8F\xAF\xE7\x9B\xB4\xE6\x8E\xA5\xE8\xBF\x90\xE8\xA1\x8C\xE3\x80\x82\n\n"
                                  "\xE4\xB8\xA4\xE4\xB8\xAA\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE5\x88\x9B\xE5\xBB\xBA\xE3\x80\x82" },
    /* UI_PROMPT_LAUNCHER    */ { "launcher",                 "启动器" },

    /* UI_FORMATS_TITLE      */ { "Formats this build can open",
                                  "\xE6\x9C\xAC\xE7\x89\x88\xE6\x9C\xAC\xE6\x94\xAF\xE6\x8C\x81\xE7\x9A\x84\xE6\xA0\xBC\xE5\xBC\x8F" },
    /* UI_FORMATS_LEGEND     */ { "convert anything marked \"convert\" on a PC first",
                                  "\xE6\xA0\x87\xE4\xB8\xBA\xE9\x9C\x80\xE8\xBD\xAC\xE6\x8D\xA2\xE7\x9A\x84\xE8\xAF\xB7\xE5\x85\x88\xE5\x9C\xA8\xE7\x94\xB5\xE8\x84\x91\xE4\xB8\x8A\xE8\xBD\xAC\xE6\x8D\xA2" },
    /* UI_FORMATS_PLAYS      */ { "plays",                  "\xE6\x94\xAF\xE6\x8C\x81" },
    /* UI_FORMATS_SLOW       */ { "slow",                   "\xE8\xBE\x83\xE6\x85\xA2" },
    /* UI_FORMATS_CONVERT    */ { "convert",                "\xE9\x9C\x80\xE8\xBD\xAC\xE6\x8D\xA2" },
    /* UI_PROMPT_FORMATS     */ { "formats",                "\xE6\xA0\xBC\xE5\xBC\x8F" },
    /* UI_PROMPT_CONTROLS    */ { "controls",               "\xE6\x93\x8D\xE4\xBD\x9C" },

    /* UI_SET_PATCHES        */ { "Patches",                "补丁" },
    /* UI_PATCH_TITLE        */ { "Apply this patch to which game?",
                                  "将此补丁应用到哪个游戏?" },
    /* UI_PATCH_NO_GAMES     */ { "No installed game to apply it to.",
                                  "没有可应用的已安装游戏。" },
    /* UI_PATCH_ASK          */ { "Apply this patch?\n\n"
                                  "  %s\n"
                                  "  onto %s\n\n"
                                  "  writes %s; the files it replaces are kept",
                                  "应用此补丁?\n\n"
                                  "  %s\n"
                                  "  到 %s\n\n"
                                  "  写入 %s;被替换的文件会保留" },
    /* UI_PATCH_OK           */ { "Patch applied to\n%s\n\n"
                                  "Remove it again from the game's settings.",
                                  "补丁已应用到\n%s\n\n可在游戏设置中移除。" },
    /* UI_PATCH_EXISTS       */ { "This patch is already applied to that game.",
                                  "该游戏已应用此补丁。" },
    /* UI_PATCH_LIST_TITLE   */ { "Patches applied to %s", "%s 已应用的补丁" },
    /* UI_PATCH_NONE         */ { "No patches applied to this game.",
                                  "此游戏没有应用补丁。" },
    /* UI_PATCH_REMOVE_ASK   */ { "Remove this patch?\n\n"
                                  "  %s\n"
                                  "  from %s\n\n"
                                  "  the files it replaced come back",
                                  "移除此补丁?\n\n"
                                  "  %s\n"
                                  "  自 %s\n\n"
                                  "  被替换的文件将恢复" },
    /* UI_PATCH_REMOVED      */ { "Patch removed.", "补丁已移除。" },
    /* UI_PATCH_REMOVE_FAIL  */ { "That patch's record could not be read,\n"
                                  "so nothing was changed.",
                                  "无法读取该补丁的记录,未做任何更改。" },
    /* UI_PROMPT_REMOVE      */ { "remove",                 "移除" },

    /* UI_CFG_INSTALL_MODE   */ { "Install mode",           "安装方式" },
    /* UI_INSTALL_EXTRACT    */ { "extract",                "解压" },
    /* UI_INSTALL_COMPRESSED */ { "keep compressed",        "保留压缩包" },
    /* UI_INSTALL_COMPRESSED_OK */
                                { "Installed to\n%s\n\n"
                                  "The game runs from its archive: %s on the\n"
                                  "card instead of %s.",
                                  "已安装到\n%s\n\n"
                                  "游戏直接从压缩包运行:占用 %s,而非 %s。" },

    /* UI_CFG_WIFI_UPLOAD    */ { "Send a game over Wi-Fi",  "通过 Wi-Fi 发送游戏" },
    /* UI_WIFI_OPEN          */ { "open",                    "打开" },
    /* UI_WIFI_TITLE         */ { "Open this address in a browser",
                                  "在浏览器中打开此地址" },
    /* UI_WIFI_WAITING       */ { "waiting for a file",       "等待文件" },
    /* UI_WIFI_RECEIVING     */ { "receiving %s -- %s",       "正在接收 %s -- %s" },
    /* UI_WIFI_RECEIVED      */ { "%d archive(s) received into game_zips",
                                  "已接收 %d 个压缩包到 game_zips" },
    /* UI_WIFI_FAILED        */ { "Cannot listen: %s",        "无法监听:%s" },
    /* UI_WIFI_HINT          */ { "Any device on this network can send a game "
                                  "while this screen is open.",
                                  "此界面打开时,本网络上的任何设备都可以发送游戏。" },

    /* UI_ABOUT */
    { "About ONS Easy Setup\n\n"
      "ONScripter        <Ogapee>\n"
      "ONScripter-jh     <jh10001>\n"
      "vita-savemgr      <d3m3vilurr>\n"
      "ONS-jh-PSV        <wetor>\n\n"
      "maintained and extended by Yurisizuku,\n"
      "https://github.com/YuriSizuku/psv-Onscripter\n",

      "　　　关于ONS for PSV\n\n"
      "ONScripter 　　　<Ogapee>\n"
      "ONScripter-jh　　<jh10001>\n"
      "vita-savemgr 　　<d3m3vilurr>\n"
      "ONS-jh-PSV 　　 <wetor>\n\n"
      "　　　wetor(@依旧W如此)\n"
      "maintained and new features by Yurisiziku, \n"
      "https://github.com/YuriSizuku/psv-Onscripter\n" }
};

}  /* namespace */

/* --- japanese ---------------------------------------------------------
 *
 * Added as its own table rather than as a third column of the one above.
 * Two reasons, both about not breaking what already works: the rows above
 * are positional, so a third element mistyped in one of a hundred and
 * fifty of them shifts every string after it, while a wrong id here is one
 * wrong string; and pairing an id with its text lets this block be read as
 * a translation rather than as a diff.
 *
 * These are my translations and no native speaker has been over them. The
 * strings a player sees most -- the buttons, the settings rows, the
 * prompts -- are conventional enough to read normally; the longer
 * explanations are the ones worth a second pair of eyes. Anything missing
 * falls back to English, which is what ui_text() already does.
 */
struct JapaneseString {
    UIStringId  id;
    const char *text;
};

const JapaneseString g_japanese[] = {
    { UI_SET_PATCHES,              "パッチ" },
    { UI_PATCH_TITLE,              "このパッチをどのゲームに適用しますか?" },
    { UI_PATCH_NO_GAMES,           "適用できるゲームがインストールされていません。" },
    { UI_PATCH_ASK,                "このパッチを適用しますか?\n\n"
                                   "  %s\n"
                                   "  適用先 %s\n\n"
                                   "  %s を書き込みます。置き換えるファイルは保存されます" },
    { UI_PATCH_OK,                 "パッチを適用しました\n%s\n\n"
                                   "ゲームの設定から取り外せます。" },
    { UI_PATCH_EXISTS,             "このパッチはすでに適用されています。" },
    { UI_PATCH_LIST_TITLE,         "%s に適用中のパッチ" },
    { UI_PATCH_NONE,               "このゲームにパッチは適用されていません。" },
    { UI_PATCH_REMOVE_ASK,         "このパッチを取り外しますか?\n\n"
                                   "  %s\n"
                                   "  対象 %s\n\n"
                                   "  置き換えられたファイルが戻ります" },
    { UI_PATCH_REMOVED,            "パッチを取り外しました。" },
    { UI_PATCH_REMOVE_FAIL,        "パッチの記録を読み取れないため、何も変更していません。" },
    { UI_PROMPT_REMOVE,            "取り外す" },
    { UI_CFG_WIFI_UPLOAD,          "Wi-Fi でゲームを送る" },
    { UI_WIFI_OPEN,                "開く" },
    { UI_WIFI_TITLE,               "このアドレスをブラウザで開いてください" },
    { UI_WIFI_WAITING,             "ファイルを待っています" },
    { UI_WIFI_RECEIVING,           "受信中 %s -- %s" },
    { UI_WIFI_RECEIVED,            "%d 個のアーカイブを game_zips に受け取りました" },
    { UI_WIFI_FAILED,              "待ち受けできません:%s" },
    { UI_WIFI_HINT,                "この画面を開いている間、同じネットワーク上のどの機器からでも"
                                   "ゲームを送れます。" },
    { UI_CFG_INSTALL_MODE,         "インストール方式" },
    { UI_INSTALL_EXTRACT,          "展開する" },
    { UI_INSTALL_COMPRESSED,       "圧縮のまま" },
    { UI_INSTALL_COMPRESSED_OK,    "インストールしました\n%s\n\n"
                                   "ゲームはアーカイブから動作します。カードの使用量は %s で、"
                                   "展開した場合の %s ではありません。" },
    { UI_SET_FULLSCREEN,           "強制フルスクリーン" },
    { UI_SET_FONTCACHE,            "フォントをキャッシュ" },
    { UI_SET_TEXTSHADOW,           "文字の影" },
    { UI_SET_TEXTBOX,              "テキストボックスを表示" },
    { UI_SET_ENCODING,             "文字コード" },
    { UI_SET_TOUCH,                "タッチパネル" },
    { UI_SET_BACKUP,               "セーブをバックアップ" },
    { UI_SET_RESTORE,              "セーブを復元" },
    { UI_CFG_TEXT_SPEED_GAME,      "文字表示速度" },
    { UI_CFG_VOL_BGM_GAME,         "BGM音量" },
    { UI_CFG_VOL_SE_GAME,          "効果音の音量" },
    { UI_CFG_VOL_VOICE_GAME,       "音声の音量" },
    { UI_SET_RESET,                "初期設定に戻す" },
    { UI_SET_RETURN,               "戻る" },
    { UI_ON,                       "オン" },
    { UI_OFF,                      "オフ" },
    { UI_AUTO,                     "自動" },
    { UI_ENC_SJIS,                 "日本語" },
    { UI_ENC_GBK,                  "中国語" },
    { UI_CFG_GRAPHIC_MODE,         "一覧の表示方法" },
    { UI_CFG_LIST,                 "リスト" },
    { UI_CFG_ICON,                 "アイコン" },
    { UI_CFG_ICON_ROW,             "アイコンの行数" },
    { UI_CFG_ICON_COL,             "アイコンの列数" },
    { UI_CFG_LIST_ROW,             "リストの行数" },
    { UI_CFG_TOUCH_MODE,           "タッチ操作" },
    { UI_TOUCH_OFF,                "オフ" },
    { UI_TOUCH_FRONT,              "前面のみ" },
    { UI_TOUCH_BOTH,               "前面と背面" },
    { UI_TOUCH_BACK,               "背面のみ" },
    { UI_TOUCH_DEFAULT,            "既定" },
    { UI_CFG_LANGUAGE,             "表示言語" },
    { UI_CFG_FETCH_COVERS,         "未取得のカバーを取得" },
    { UI_CFG_CLEAN,                "一時ファイルを削除" },
    { UI_CFG_TEXT_SPEED,           "文字表示速度（新規のみ）" },
    { UI_CFG_VOL_BGM,              "BGM音量（新規のみ）" },
    { UI_CFG_VOL_SE,               "効果音の音量（新規のみ）" },
    { UI_CFG_VOL_VOICE,            "音声の音量（新規のみ）" },
    { UI_CFG_DEBUG_LOG,            "デバッグログを書き出す" },
    { UI_CFG_THEME,                "テーマ" },
    { UI_THEME_DARK,               "ダーク" },
    { UI_THEME_LIGHT,              "ライト" },
    { UI_CFG_VIEW_LOG,             "ログを表示" },
    { UI_LOG_OPEN,                 "開く" },
    { UI_LOG_EMPTY,                "ログは空です。「デバッグログを書き出す」を\n"
                                  "オンにしてから、もう一度実行してください。" },
    { UI_LOG_ENGINE,               "エンジン" },
    { UI_LOG_LAUNCHER,             "ランチャー" },
    { UI_LOG_CRASH,                "前回の異常終了" },
    { UI_PROMPT_SWITCH,            "ログを切替" },
    { UI_RETRY,                    "再試行" },
    { UI_RETRY_RESUME,             "続きから" },
    { UI_CLEAN_RETRY,              "削除して再試行" },
    { UI_FAIL_SPACE_HINT,          "\n\n"
                                  "一時ファイルを削除すれば足りるかもしれません。" },
    { UI_FAIL_RESUME_HINT,         "\n\n"
                                  "%s まで展開済みです。再試行すると続きから進みます。" },
    { UI_SPEED_SLOW,               "遅い" },
    { UI_SPEED_NORMAL,             "普通" },
    { UI_SPEED_FAST,               "速い" },
    { UI_CFG_SORT,                 "並び順" },
    { UI_SORT_NAME,                "名前" },
    { UI_SORT_RECENT,              "最近プレイした順" },
    { UI_SORT_SIZE,                "サイズ" },
    { UI_BTN_START,                "起動" },
    { UI_BTN_CONFIG,               "設定" },
    { UI_BTN_INSTALL,              "インストール" },
    { UI_BTN_COVER,                "カバー" },
    { UI_BTN_DELETE,               "削除" },
    { UI_PROMPT_NO,                "いいえ" },
    { UI_PROMPT_YES,               "はい" },
    { UI_PROMPT_CLOSE,             "閉じる" },
    { UI_FOOTER_HINTS,             "%d / %d" },
    { UI_HINT_SETTINGS,            "設定" },
    { UI_HINT_HELP,                "ヘルプ" },
    { UI_HINT_ABOUT,               "情報" },
    { UI_INSTALLING,               "インストール中..." },
    { UI_NOT_IMPLEMENTED,          "[未実装の機能です]" },
    { UI_MAKE_PACKAGE_ASK,         "このゲームの起動用バブルを作成しますか？" },
    { UI_MAKE_PACKAGE_RUN,         "バブルを作成しています...操作しないでください..." },
    { UI_MAKE_PACKAGE_OK,          "バブルを作成しました。" },
    { UI_MAKE_PACKAGE_FAIL,        "バブルを作成できませんでした。" },
    { UI_COVER_ASK,                "vndb.org でこのゲームを検索してカバーを取得しますか？" },
    { UI_COVER_RUN,                "vndb.org に問い合わせています..." },
    { UI_COVER_OK,                 "カバーを保存しました。" },
    { UI_COVER_NOT_FOUND,          "その名前のカバーは vndb.org にありません。" },
    { UI_COVER_NO_NET,             "ネットワークに接続していません。" },
    { UI_COVER_FAIL,               "vndb.org に接続できませんでした。" },
    { UI_COVER_WRITE_FAIL,         "カバーを保存できませんでした。" },
    { UI_COVERS_ALL_ASK,           "カバーのないゲームをすべて vndb.org で検索しますか？" },
    { UI_COVERS_ALL_RUN,           "カバーを取得しています" },
    { UI_COVERS_ALL_DONE,          "取得 %d 件、見つからず %d 件、取得済み %d 件" },
    { UI_COVERS_START,             "開始" },
    { UI_CLEAN_ASK,                "一時ファイルを削除しますか？\n\n"
                                  "  バブル作成用の一時フォルダ\n"
                                  "  残っている tmp.mus\n"
                                  "  ランチャーの一覧キャッシュ\n\n"
                                  "  セーブとゲームには影響しません" },
    { UI_CLEAN_RUN,                "削除しています..." },
    { UI_CLEAN_DONE,               "%d 個のファイルを削除し、%s を解放しました" },
    { UI_CLEAN_NOTHING,            "削除するものはありません。" },
    { UI_CLEAN_START,              "削除" },
    { UI_SAVES_BACKED_UP,          "%d 個のセーブをバックアップしました。" },
    { UI_SAVES_RESTORED,           "%d 個のセーブを復元しました。" },
    { UI_SAVES_NONE,               "このゲームにはまだセーブがありません。" },
    { UI_SAVES_NO_BACKUP,          "このゲームのバックアップはありません。" },
    { UI_SAVES_FAIL,               "セーブをコピーできませんでした。" },
    { UI_DELETE_ASK,               "%s を削除しますか？\n\n"
                                  "  %s、ファイル %u 個\n"
                                  "  ux0:onsemu 内\n\n"
                                  "  フォルダ内のセーブも一緒に消えます" },
    { UI_DELETE_ASK_ZIP,           "圧縮ファイル %s を削除しますか？\n\n"
                                  "  %s\n\n"
                                  "  インストール済みのゲームはそのままです" },
    { UI_DELETE_RUN,               "削除しています..." },
    { UI_DELETE_OK,                "削除しました。残り %s です。" },
    { UI_DELETE_FAIL,              "削除できませんでした。" },
    { UI_FREE_SPACE,               "残り %s" },
    { UI_LAST_PLAYED,              "最終プレイ %s" },
    { UI_NEVER_PLAYED,             "未プレイ" },
    { UI_ZIP_INFO,                 "圧縮ファイル %s、展開後およそ %s" },
    { UI_ZIP_INFO_TIME,            "インストールにおよそ %d 分 %02d 秒" },
    { UI_SEARCH_TITLE,             "ゲームを検索" },
    { UI_SEARCH_ACTIVE,            "\"%s\"" },
    { UI_SEARCH_EMPTY,             "\"%s\" に一致するゲームはありません。\n\n"
                                  "%s で検索を解除します。" },
    { UI_HINT_SEARCH,              "検索" },
    { UI_UNFINISHED,               "未完了" },
    { UI_RESUME_ASK,               "このゲームのインストールを続けますか？\n\n"
                                  "  %s\n"
                                  "  %s / %s を展開済み\n\n"
                                  "  残り %s" },
    { UI_RESUME_BLOCKED,           "このゲームはインストールが完了していません。\n\n"
                                  "圧縮ファイルからもう一度インストールするか、削除してください。" },
    { UI_HELP_TITLE,               "ゲーム中の操作" },
    { UI_HELP_CONFIRM,             "決定・進める" },
    { UI_HELP_SKIP,                "押している間だけ早送り" },
    { UI_HELP_AUTO,                "オートモード" },
    { UI_HELP_MENU,                "メニュー・履歴を閉じる" },
    { UI_HELP_SKIP_PAGE,           "このページを飛ばす" },
    { UI_HELP_TOGGLE_SKIP,         "スキップの開始・停止" },
    { UI_HELP_BACKLOG,             "履歴" },
    { UI_HELP_CURSOR,              "選択肢を移動" },
    { UI_HELP_STICK,               "方向キーと同じ" },
    { UI_HELP_OVERLAY,             "ゲーム中にこの一覧を表示" },
    { UI_LAUNCHER_TITLE,           "ランチャーの操作" },
    { UI_LAUNCHER_START,           "ゲームを起動・圧縮ファイルをインストール" },
    { UI_LAUNCHER_PANEL,           "このゲームの設定・セーブ・カバー" },
    { UI_LAUNCHER_SETTINGS,        "全体の設定" },
    { UI_LAUNCHER_HELP,            "この画面" },
    { UI_LAUNCHER_SEARCH,          "ゲームを検索" },
    { UI_LAUNCHER_ABOUT,           "このビルドについて" },
    { UI_LAUNCHER_MOVE,            "一覧を移動" },
    { UI_WHERE_TITLE,              "ゲームの置き場所" },
    { UI_WHERE_ZIP,                "ux0:data/game_zips/  -- インストール前の圧縮ファイル" },
    { UI_WHERE_FOLDER,             "ux0:onsemu/  -- インストール済みのゲーム、1つにつき1フォルダ" },
    { UI_FIRST_RUN_TITLE,          "ゲームがありません" },
    { UI_FIRST_RUN_BODY,           "メモリーカードにゲームを入れると、ここに表示されます。\n\n"
                                  "  圧縮ファイル：  ux0:data/game_zips/\n"
                                  "  この画面からインストールします。\n\n"
                                  "  ゲームのフォルダ： ux0:onsemu/\n"
                                  "  そのまま起動できます。\n\n"
                                  "どちらのフォルダも作成済みです。" },
    { UI_PROMPT_LAUNCHER,          "ランチャー" },
    { UI_FORMATS_TITLE,            "このビルドが開ける形式" },
    { UI_FORMATS_LEGEND,           "「変換」の形式はパソコンで変換してください" },
    { UI_FORMATS_PLAYS,            "対応" },
    { UI_FORMATS_SLOW,             "低速" },
    { UI_FORMATS_CONVERT,          "変換" },
    { UI_PROMPT_FORMATS,           "対応形式" },
    { UI_PROMPT_CONTROLS,          "操作" },
    { UI_ABOUT,                    "ONS Easy Setup について\n\n"
                                  "ONScripter        <Ogapee>\n"
                                  "ONScripter-jh     <jh10001>\n"
                                  "vita-savemgr      <d3m3vilurr>\n"
                                  "ONS-jh-PSV        <wetor>\n\n"
                                  "Yurisizuku による保守と拡張、\n"
                                  "https://github.com/YuriSizuku/psv-Onscripter\n" },
};

/* Read once into an array indexed by id: ui_text() is called for every
 * label on every frame, and a scan through a hundred and fifty pairs each
 * time would be a scan for nothing. */
const char *japanese_text(UIStringId id)
{
    static const char *table[UI_STRING_COUNT];
    static bool built = false;

    if (!built) {
        built = true;
        for (size_t i = 0; i < sizeof(g_japanese) / sizeof(g_japanese[0]); i++) {
            const JapaneseString &j = g_japanese[i];
            if (j.id >= 0 && j.id < UI_STRING_COUNT) table[j.id] = j.text;
        }
    }

    if (id < 0 || id >= UI_STRING_COUNT) return NULL;
    return table[id];
}

const char *ui_text(UIStringId id)
{
    if (id < 0 || id >= UI_STRING_COUNT) return "";

    const char *s = (g_language == UI_LANG_JA) ? japanese_text(id)
                                              : g_strings[id][g_language];
    /* A missing translation falls back to English rather than showing an
     * empty label. */
    if (s == NULL || s[0] == '\0') s = g_strings[id][UI_LANG_EN];
    return s ? s : "";
}

void ui_set_language(UILanguage lang)
{
    if (lang >= 0 && lang < UI_LANG_COUNT) g_language = lang;
}

UILanguage ui_get_language()
{
    return g_language;
}

const char *ui_language_name(UILanguage lang)
{
    switch (lang) {
    case UI_LANG_ZH: return "zh";
    case UI_LANG_JA: return "ja";
    default:         return "en";
    }
}

UILanguage ui_language_from_name(const char *name)
{
    if (name == NULL) return UI_LANG_EN;
    if (name[0] == 'z' || name[0] == 'Z') return UI_LANG_ZH;
    if (name[0] == 'j' || name[0] == 'J') return UI_LANG_JA;
    return UI_LANG_EN;
}
