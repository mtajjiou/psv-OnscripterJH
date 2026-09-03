/* Host-side tests for the script encoding detector.
 *
 *   test_encoding_detect <fixture dir>
 *
 * The fixtures are real Japanese and Chinese prose encoded by python's own
 * cp932 and gbk codecs, so these are the byte patterns a shipped script
 * actually contains -- not something hand-built to match the detector.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoding_detect.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static const char *name(ScriptEncoding e)
{
    switch (e) {
    case SCRIPT_ENCODING_SJIS: return "sjis";
    case SCRIPT_ENCODING_GBK:  return "gbk";
    default:                   return "unknown";
    }
}

static char *slurp(const char *dir, const char *file, size_t *len)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, file);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("FAIL: cannot open fixture %s\n", path);
        failures++;
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        printf("FAIL: short read on %s\n", path);
        failures++;
    }
    fclose(fp);
    buf[n] = '\0';
    *len = (size_t)n;
    return buf;
}

static void expect(const char *dir, const char *file, ScriptEncoding want)
{
    size_t len = 0;
    char *buf = slurp(dir, file, &len);
    if (!buf) return;

    ScriptEncodingStats st;
    ScriptEncoding got = detectScriptEncodingStats(buf, len, &st);

    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s detected as %s, expected %s\n"
               "      sjis: %ld pairs, %ld kana, %ld hiragana, %ld invalid\n"
               "      gbk : %ld pairs, %ld hanzi, %ld invalid\n",
               file, name(got), name(want),
               st.sjis_pairs, st.katakana, st.hiragana, st.sjis_invalid,
               st.gbk_pairs, st.common_hanzi, st.gbk_invalid);
    }
    free(buf);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";

    expect(dir, "script_sjis.bin",  SCRIPT_ENCODING_SJIS);
    expect(dir, "script_gbk.bin",   SCRIPT_ENCODING_GBK);
    /* Nothing but ASCII: both code pages read it identically, so there is no
     * evidence either way and the caller should keep its default. */
    expect(dir, "script_ascii.bin", SCRIPT_ENCODING_UNKNOWN);

    /* Degenerate inputs must not crash or assert an encoding. */
    check(detectScriptEncoding(NULL, 0) == SCRIPT_ENCODING_UNKNOWN,
          "NULL buffer yields UNKNOWN");
    check(detectScriptEncoding("", 0) == SCRIPT_ENCODING_UNKNOWN,
          "empty buffer yields UNKNOWN");
    check(detectScriptEncoding("*define\ngame\n", 13) == SCRIPT_ENCODING_UNKNOWN,
          "tiny ASCII script yields UNKNOWN");

    /* A lone trailing lead byte at the very end must not read past the
     * buffer; run it under a sanitizer to mean anything. */
    {
        char tail[64];
        memset(tail, 'a', sizeof(tail));
        tail[sizeof(tail) - 1] = (char)0x82;
        detectScriptEncoding(tail, sizeof(tail));
        check(1, "truncated lead byte at end of buffer is handled");
    }

    /* Half-width katakana on its own is not evidence of Japanese: those
     * same bytes are ordinary GBK lead bytes, which is exactly what Chinese
     * text misread as Shift-JIS looks like.  Declining is correct. */
    {
        char kana[256];
        for (size_t i = 0; i < sizeof(kana); i++)
            kana[i] = (char)(0xB1 + (i % 20));
        check(detectScriptEncoding(kana, sizeof(kana)) != SCRIPT_ENCODING_SJIS,
              "half-width katakana alone is not called sjis");
    }

    /* Bytes that no code page allows must not be claimed by either. */
    {
        char junk[256];
        for (size_t i = 0; i < sizeof(junk); i++)
            junk[i] = (char)0xFF;
        ScriptEncoding e = detectScriptEncoding(junk, sizeof(junk));
        check(e == SCRIPT_ENCODING_UNKNOWN, "all-0xFF yields UNKNOWN");
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
