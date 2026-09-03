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
    /* UI_CFG_USE_DPAD       */ { "Buttons only",           "仅按键" },
    /* UI_CFG_ICON_ROW       */ { "Icon rows",              "图标行数" },
    /* UI_CFG_ICON_COL       */ { "Icon columns",           "图标列数" },
    /* UI_CFG_LIST_ROW       */ { "List rows",              "列表行数" },
    /* UI_CFG_TOUCH_MODE     */ { "Touch control",          "触摸控制" },
    /* UI_TOUCH_OFF          */ { "off",                    "关闭" },
    /* UI_TOUCH_FRONT        */ { "front only",             "仅前触屏" },
    /* UI_TOUCH_BOTH         */ { "front and back",         "前后触屏" },
    /* UI_CFG_LANGUAGE       */ { "Language",               "界面语言" },

    /* UI_BTN_START          */ { "start",                  "启动" },
    /* UI_BTN_CONFIG         */ { "config",                 "设置" },
    /* UI_BTN_INSTALL        */ { "install",                "安装" },
    /* UI_PROMPT_NO          */ { "no",                     "取消" },
    /* UI_PROMPT_YES         */ { "yes",                    "确定" },
    /* UI_PROMPT_CLOSE       */ { "close",                  "关闭" },
    /* UI_FOOTER_HINTS       */ { "L settings   R help   Select about   |  %d/%d",
                                  "L 设置菜单   R 查看帮助   Select 关于   |  %d/%d" },

    /* UI_INSTALLING         */ { "Installing...",          "安装中..." },
    /* UI_NOT_IMPLEMENTED    */ { "[not implemented yet]",  "[暂未开放的功能]" },
    /* UI_MAKE_PACKAGE_ASK   */ { "Create a shortcut bubble for this game?",
                                  "是否要生成快捷启动气泡？" },
    /* UI_MAKE_PACKAGE_RUN   */ { "Creating the bubble... do not touch anything...",
                                  "正在生成气泡中...请勿操作..." },
    /* UI_MAKE_PACKAGE_OK    */ { "Bubble created.",        "快捷启动气泡生成完毕！" },
    /* UI_MAKE_PACKAGE_FAIL  */ { "Could not create the bubble.",
                                  "快捷启动气泡生成失败..." },

    /* UI_HELP */
    { "In-game controls\n\n"
      "\xE2\x97\x8B         confirm / continue\n"
      "\xC3\x97         hold to skip\n"
      "\xE2\x96\xA1         auto mode\n"
      "\xE2\x96\xB3         menu / leave backlog\n"
      "L         skip this page\n"
      "R         toggle skipping\n"
      "left right    backlog\n"
      "up down      move the cursor\n"
      "left stick    same as the d-pad\n"
      "hold Select   show this list in game\n",

      "使用帮助(游戏内)\n\n"
      "○　　　　确认/继续\n"
      "╳　　　　按住快进\n"
      "□　　　　自动模式\n"
      "△　　　　菜单/关闭回想模式\n"
      "Ｌ　　　　快进当前页\n"
      "Ｒ　　　　开启/停止快进\n"
      "←→　　　回想模式选择\n"
      "↑↓　　　选项/按钮选择\n"
      "左摇杆　　等同方向键\n"
      "长按 Select　游戏中显示本列表\n" },

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
