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
    /* UI_CFG_LANGUAGE       */ { "Language",               "界面语言" },

    /* UI_CFG_FETCH_COVERS   */ { "Fetch missing covers",   "\xE8\x8E\xB7\xE5\x8F\x96\xE7\xBC\xBA\xE5\xB0\x91\xE7\x9A\x84\xE5\xB0\x81\xE9\x9D\xA2" },

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

const char *ui_text(UIStringId id)
{
    if (id < 0 || id >= UI_STRING_COUNT) return "";

    const char *s = g_strings[id][g_language];
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
    return lang == UI_LANG_ZH ? "zh" : "en";
}

UILanguage ui_language_from_name(const char *name)
{
    if (name && (name[0] == 'z' || name[0] == 'Z')) return UI_LANG_ZH;
    return UI_LANG_EN;
}
