/* -*- C++ -*-
 *
 *  GUI_Text.h -- the launcher's interface text, in English and Chinese
 *
 *  Upstream's labels are Chinese with an English gloss in brackets, which
 *  works if you read Chinese and is cramped and second-class if you do not.
 *  Every string the launcher shows now lives here in both languages, chosen
 *  by one setting, so neither language is squeezed into a parenthesis.
 *
 *  Adding a language means adding a column: the ids are what the code refers
 *  to, so no call site changes.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef GUI_TEXT_H
#define GUI_TEXT_H

enum UILanguage {
    UI_LANG_EN = 0,
    UI_LANG_ZH = 1,
    UI_LANG_COUNT
};

enum UIStringId {
    /* Settings menu (per game) */
    UI_SET_FULLSCREEN = 0,
    UI_SET_FONTCACHE,
    UI_SET_TEXTSHADOW,
    UI_SET_TEXTBOX,
    UI_SET_ENCODING,
    UI_SET_RESET,
    UI_SET_RETURN,

    /* Values */
    UI_ON,
    UI_OFF,
    UI_AUTO,
    UI_ENC_SJIS,
    UI_ENC_GBK,

    /* Config menu (global) */
    UI_CFG_GRAPHIC_MODE,
    UI_CFG_LIST,
    UI_CFG_ICON,
    UI_CFG_ICON_ROW,
    UI_CFG_ICON_COL,
    UI_CFG_LIST_ROW,
    UI_CFG_TOUCH_MODE,
    UI_TOUCH_OFF,
    UI_TOUCH_FRONT,
    UI_TOUCH_BOTH,
    UI_CFG_LANGUAGE,

    /* Buttons and prompts */
    UI_BTN_START,
    UI_BTN_CONFIG,
    UI_BTN_INSTALL,
    UI_BTN_COVER,
    UI_PROMPT_NO,
    UI_PROMPT_YES,
    UI_PROMPT_CLOSE,
    UI_FOOTER_HINTS,

    /* Install and package flow */
    UI_INSTALLING,
    UI_NOT_IMPLEMENTED,
    UI_MAKE_PACKAGE_ASK,
    UI_MAKE_PACKAGE_RUN,
    UI_MAKE_PACKAGE_OK,
    UI_MAKE_PACKAGE_FAIL,

    /* Cover art from vndb */
    UI_COVER_ASK,
    UI_COVER_RUN,
    UI_COVER_OK,
    UI_COVER_NOT_FOUND,
    UI_COVER_NO_NET,
    UI_COVER_FAIL,
    UI_COVER_WRITE_FAIL,

    /* Long text */
    UI_HELP,
    UI_ABOUT,

    UI_STRING_COUNT
};

/* The text for the current language.  Never returns NULL. */
const char *ui_text(UIStringId id);

void      ui_set_language(UILanguage lang);
UILanguage ui_get_language();

/* For the config file: "en" / "zh". */
const char *ui_language_name(UILanguage lang);
UILanguage  ui_language_from_name(const char *name);

#endif
