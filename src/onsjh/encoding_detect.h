/* -*- C++ -*-
 *
 *  encoding_detect.h -- guesses whether a script is Shift-JIS or GBK
 *
 *  ONScripter games ship their script in the code page of the language they
 *  were written for, and nothing in the file says which one it is.  Guessing
 *  wrong is not subtle: every double-byte character decodes to the wrong
 *  glyph and the parser usually dies on the first line of dialogue with
 *  "text cannot be displayed in define section".
 *
 *  detectScriptEncoding() looks at the decrypted script and picks one, so a
 *  player does not have to know their game's language *and* which flag
 *  corresponds to it.  --enc:sjis and --enc:gbk still override it.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef __ENCODING_DETECT_H__
#define __ENCODING_DETECT_H__

#include <stddef.h>

enum ScriptEncoding {
    SCRIPT_ENCODING_UNKNOWN = 0,  /* no confident answer; keep the default */
    SCRIPT_ENCODING_SJIS,
    SCRIPT_ENCODING_GBK
};

/* Scores buf as Shift-JIS and as GBK and returns the better fit, or
 * SCRIPT_ENCODING_UNKNOWN when neither wins clearly -- an all-ASCII script,
 * say, which decodes identically either way. */
ScriptEncoding detectScriptEncoding(const char *buf, size_t len);

/* The counts behind one verdict.  Exposed for the tests and for the log line
 * the engine prints when it picks. */
struct ScriptEncodingStats {
    long sjis_invalid;   /* byte pairs that are not legal Shift-JIS */
    long gbk_invalid;    /* byte pairs that are not legal GBK */
    long sjis_pairs;     /* double-byte characters seen, Shift-JIS reading */
    long gbk_pairs;      /* ... GBK reading */
    long hiragana;       /* Shift-JIS hiragana, the Japanese giveaway */
    long katakana;       /* half-width katakana, single-byte in Shift-JIS */
    long common_hanzi;   /* GB2312 level-1 hanzi, the Chinese giveaway */
};

ScriptEncoding detectScriptEncodingStats(const char *buf, size_t len,
                                         ScriptEncodingStats *out);

#endif
