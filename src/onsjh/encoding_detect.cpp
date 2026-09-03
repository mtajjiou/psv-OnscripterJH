/* -*- C++ -*-
 *
 *  encoding_detect.cpp -- see encoding_detect.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "encoding_detect.h"

namespace {

/* Shift-JIS: 0x81-0x9F and 0xE0-0xFC lead a two-byte character; 0xA1-0xDF is
 * a half-width katakana all by itself. */
inline bool sjisLead(unsigned char c) {
    return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}
inline bool sjisKana(unsigned char c) { return c >= 0xA1 && c <= 0xDF; }
inline bool sjisTrail(unsigned char c) {
    return (c >= 0x40 && c <= 0x7E) || (c >= 0x80 && c <= 0xFC);
}

/* GBK: 0x81-0xFE leads, and every one of them does. */
inline bool gbkLead(unsigned char c) { return c >= 0x81 && c <= 0xFE; }
inline bool gbkTrail(unsigned char c) {
    return (c >= 0x40 && c <= 0x7E) || (c >= 0x80 && c <= 0xFE);
}

/* Japanese prose is mostly hiragana, which sits in one small Shift-JIS
 * block. */
inline bool sjisHiragana(unsigned char lead, unsigned char trail) {
    return lead == 0x82 && trail >= 0x9F && trail <= 0xF1;
}

/* Chinese prose is mostly GB2312 level-1 hanzi, likewise one block.  Note
 * these lead bytes are read as katakana under Shift-JIS, so the two tests
 * never both fire on the same byte. */
inline bool gbkCommonHanzi(unsigned char lead, unsigned char trail) {
    return lead >= 0xB0 && lead <= 0xD7 && trail >= 0xA1 && trail <= 0xFE;
}

/* Only the first stretch of the script is worth reading: it is enough to
 * decide, and a script can be several megabytes. */
const size_t SCAN_LIMIT = 256 * 1024;

}  /* namespace */

ScriptEncoding detectScriptEncodingStats(const char *buf, size_t len,
                                         ScriptEncodingStats *out)
{
    ScriptEncodingStats st;
    st.sjis_invalid = st.gbk_invalid = 0;
    st.sjis_pairs = st.gbk_pairs = 0;
    st.hiragana = st.katakana = st.common_hanzi = 0;

    if (buf == NULL) len = 0;
    if (len > SCAN_LIMIT) len = SCAN_LIMIT;

    const unsigned char *p = (const unsigned char *)buf;

    /* Walk once as Shift-JIS... */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = p[i];
        if (c < 0x80) continue;
        if (sjisKana(c)) { st.katakana++; continue; }
        if (sjisLead(c)) {
            if (i + 1 >= len) break;
            unsigned char t = p[++i];
            if (sjisTrail(t)) {
                st.sjis_pairs++;
                if (sjisHiragana(c, t)) st.hiragana++;
            }
            else st.sjis_invalid++;
            continue;
        }
        st.sjis_invalid++;  /* 0x80, 0xA0, 0xFD-0xFF are never Shift-JIS */
    }

    /* ...and once as GBK.  Separate walks, because the two disagree about
     * how many bytes a character takes, and a shared one would inherit the
     * other's mistakes. */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = p[i];
        if (c < 0x80) continue;
        if (gbkLead(c)) {
            if (i + 1 >= len) break;
            unsigned char t = p[++i];
            if (gbkTrail(t)) {
                st.gbk_pairs++;
                if (gbkCommonHanzi(c, t)) st.common_hanzi++;
            }
            else st.gbk_invalid++;
            continue;
        }
        st.gbk_invalid++;   /* 0x80 and 0xFF are never GBK */
    }

    if (out) *out = st;

    /* An ASCII-only script decodes the same either way, so say so rather
     * than flipping a coin. */
    long sjis_chars = st.sjis_pairs + st.katakana;
    if (sjis_chars < 16 && st.gbk_pairs < 16) return SCRIPT_ENCODING_UNKNOWN;

    /* Illegal byte pairs are the strongest evidence: a script that does not
     * decode cleanly as one encoding is not in that encoding.  Compare
     * error *rates*, since the two walks cover different numbers of
     * characters. */
    double sjis_rate = sjis_chars > 0
        ? (double)st.sjis_invalid / (double)(sjis_chars + st.sjis_invalid) : 1.0;
    double gbk_rate = st.gbk_pairs > 0
        ? (double)st.gbk_invalid / (double)(st.gbk_pairs + st.gbk_invalid) : 1.0;

    if (sjis_rate < 0.01 && gbk_rate > 0.05) return SCRIPT_ENCODING_SJIS;
    if (gbk_rate < 0.01 && sjis_rate > 0.05) return SCRIPT_ENCODING_GBK;

    /* Both decode cleanly, which is common -- the code pages overlap heavily.
     * Fall back to what the text is made of.  Japanese cannot be written
     * without hiragana, and Chinese prose is dominated by level-1 hanzi, so
     * whichever marker is dense wins. */
    long total = sjis_chars > st.gbk_pairs ? sjis_chars : st.gbk_pairs;
    if (total > 0) {
        /* Hiragana only.  Half-width katakana is not evidence of Japanese
         * here: Chinese text misread as Shift-JIS turns into a flood of it,
         * because GBK lead bytes land squarely in the katakana range, while
         * real scripts use it hardly at all. */
        double jp = (double)st.hiragana / (double)total;
        double cn = (double)st.common_hanzi / (double)total;
        if (jp > cn * 2.0 && jp > 0.10) return SCRIPT_ENCODING_SJIS;
        if (cn > jp * 2.0 && cn > 0.10) return SCRIPT_ENCODING_GBK;
    }

    /* Neither the byte rules nor the character mix committed.  Better to
     * leave the default in place than to guess and garble the script. */
    return SCRIPT_ENCODING_UNKNOWN;
}

ScriptEncoding detectScriptEncoding(const char *buf, size_t len)
{
    return detectScriptEncodingStats(buf, len, NULL);
}
