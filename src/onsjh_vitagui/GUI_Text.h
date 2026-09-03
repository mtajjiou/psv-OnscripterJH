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
    UI_SET_TOUCH,
    UI_SET_BACKUP,
    UI_SET_RESTORE,
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
    UI_TOUCH_BACK,
    UI_TOUCH_DEFAULT,
    UI_CFG_LANGUAGE,
    UI_CFG_FETCH_COVERS,
    UI_CFG_CLEAN,
    UI_CFG_SORT,
    UI_SORT_NAME,
    UI_SORT_RECENT,
    UI_SORT_SIZE,

    /* Buttons and prompts */
    UI_BTN_START,
    UI_BTN_CONFIG,
    UI_BTN_INSTALL,
    UI_BTN_COVER,
    UI_BTN_DELETE,
    UI_PROMPT_NO,
    UI_PROMPT_YES,
    UI_PROMPT_CLOSE,
    UI_FOOTER_HINTS,
    UI_HINT_SETTINGS,
    UI_HINT_HELP,
    UI_HINT_ABOUT,

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
    UI_COVERS_ALL_ASK,
    UI_COVERS_ALL_RUN,
    UI_COVERS_ALL_DONE,
    UI_COVERS_START,

    /* Clearing what nothing needs any more */
    UI_CLEAN_ASK,
    UI_CLEAN_RUN,
    UI_CLEAN_DONE,
    UI_CLEAN_NOTHING,
    UI_CLEAN_START,

    /* Copying a game's saves out and back */
    UI_SAVES_BACKED_UP,
    UI_SAVES_RESTORED,
    UI_SAVES_NONE,
    UI_SAVES_NO_BACKUP,
    UI_SAVES_FAIL,

    /* Removing an installed game, and what is left on the card */
    UI_DELETE_ASK,
    UI_DELETE_ASK_ZIP,
    UI_DELETE_RUN,
    UI_DELETE_OK,
    UI_DELETE_FAIL,
    UI_FREE_SPACE,

    /* What the game panel knows about a row */
    UI_LAST_PLAYED,
    UI_NEVER_PLAYED,
    UI_ZIP_INFO,
    UI_ZIP_INFO_TIME,

    /* Searching the list */
    UI_SEARCH_TITLE,
    UI_SEARCH_ACTIVE,
    UI_SEARCH_EMPTY,
    UI_HINT_SEARCH,

    /* An install that stopped part way */
    UI_UNFINISHED,
    UI_RESUME_ASK,
    UI_RESUME_BLOCKED,

    /* The help screen is a table now: a button in the first column, what it
     * does in the second.  See draw_help_screen(). */
    UI_HELP_TITLE,
    UI_HELP_CONFIRM,
    UI_HELP_SKIP,
    UI_HELP_AUTO,
    UI_HELP_MENU,
    UI_HELP_SKIP_PAGE,
    UI_HELP_TOGGLE_SKIP,
    UI_HELP_BACKLOG,
    UI_HELP_CURSOR,
    UI_HELP_STICK,
    UI_HELP_OVERLAY,

    /* The help screen's second page: what this build can open, and how
     * to get between the two pages.  See draw_formats_screen(). */
    UI_FORMATS_TITLE,
    UI_FORMATS_LEGEND,
    UI_FORMATS_PLAYS,
    UI_FORMATS_SLOW,
    UI_FORMATS_CONVERT,
    UI_PROMPT_FORMATS,
    UI_PROMPT_CONTROLS,

    /* Long text */
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
