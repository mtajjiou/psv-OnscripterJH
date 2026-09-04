/* Host-side tests for the launcher's interface text.
 *
 * The table is a hand-maintained grid, so the failure modes are a missing
 * entry, a row out of order, or a language that silently shows nothing.
 * None of those are visible until the launcher is on a screen, so check
 * them here.
 */
#include <stdio.h>
#include <string.h>

#include "GUI_Text.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

/* The conversions in a format string, in order, as one comparable string:
 * "%d%s" for "%d files removed, %s freed".  Flags and widths are kept,
 * since "%02d" and "%d" are not the same promise. */
static void format_specs(const char *s, char *out, size_t n)
{
    size_t used = 0;

    out[0] = '\0';
    for (const char *p = s; *p; p++) {
        if (*p != '%') continue;
        if (p[1] == '%') { p++; continue; }

        p++;
        while (used + 2 < n && *p && strchr("diouxXeEfgGaAcspn", *p) == NULL)
            out[used++] = *p++;
        if (*p && used + 2 < n) out[used++] = *p;
        out[used] = '\0';
    }
}

int main(void)
{
    /* Every id must have text in every language: an empty label is a blank
     * row in the menu, which reads as a broken build. */
    for (int lang = 0; lang < UI_LANG_COUNT; lang++) {
        ui_set_language((UILanguage)lang);
        for (int id = 0; id < UI_STRING_COUNT; id++) {
            const char *s = ui_text((UIStringId)id);
            checks++;
            if (s == NULL || s[0] == '\0') {
                failures++;
                printf("FAIL: string %d is empty in language %d\n", id, lang);
            }
        }
    }

    /* The English column must actually be English -- a copy-paste that left
     * a Chinese string in it would defeat the point. */
    ui_set_language(UI_LANG_EN);
    for (int id = 0; id < UI_STRING_COUNT; id++) {
        const char *s = ui_text((UIStringId)id);
        int high = 0;
        for (const unsigned char *p = (const unsigned char *)s; *p; p++)
            if (*p >= 0x80) high++;
        checks++;
        /* The help screen uses a few box-drawing glyphs for the buttons, so
         * allow a handful; a label full of high bytes is a mistake. */
        if (high > 12) {
            failures++;
            printf("FAIL: english string %d has %d non-ascii bytes\n", id, high);
        }
    }

    /* Every language's string must take exactly the arguments the English
     * one takes, in the same order.
     *
     * This is the one translation mistake that is not a cosmetic problem:
     * these strings are handed to snprintf with a fixed argument list, so a
     * %s the translator dropped reads whatever was next on the stack, and
     * one they added reads past the end of it.  A wrong word is a wrong
     * word; a wrong conversion is a crash in a language nobody testing the
     * build reads. */
    for (int id = 0; id < UI_STRING_COUNT; id++) {
        char want[64], got[64];

        ui_set_language(UI_LANG_EN);
        format_specs(ui_text((UIStringId)id), want, sizeof(want));

        for (int lang = 0; lang < UI_LANG_COUNT; lang++) {
            if (lang == UI_LANG_EN) continue;
            ui_set_language((UILanguage)lang);
            format_specs(ui_text((UIStringId)id), got, sizeof(got));

            checks++;
            if (strcmp(want, got) != 0) {
                failures++;
                printf("FAIL: string %d takes \"%s\" in english but \"%s\" "
                       "in language %d\n", id, want, got, lang);
            }
        }
    }

    /* The footer takes two numbers; a table edit that dropped a %d would
     * print garbage. */
    ui_set_language(UI_LANG_EN);
    check(strstr(ui_text(UI_FOOTER_HINTS), "%d") != NULL,
          "english footer keeps its format specifiers");
    ui_set_language(UI_LANG_ZH);
    check(strstr(ui_text(UI_FOOTER_HINTS), "%d") != NULL,
          "chinese footer keeps its format specifiers");

    /* Round-tripping through the config file must preserve the choice. */
    check(ui_language_from_name("en") == UI_LANG_EN, "\"en\" reads as english");
    check(ui_language_from_name("zh") == UI_LANG_ZH, "\"zh\" reads as chinese");
    check(ui_language_from_name(NULL) == UI_LANG_EN, "a missing setting is english");
    check(ui_language_from_name("klingon") == UI_LANG_EN,
          "an unknown language falls back to english");
    check(!strcmp(ui_language_name(UI_LANG_EN), "en"), "english writes as \"en\"");
    check(!strcmp(ui_language_name(UI_LANG_ZH), "zh"), "chinese writes as \"zh\"");

    /* Out of range ids must not read past the table. */
    check(ui_text((UIStringId)-1)[0] == '\0', "a negative id is empty, not a crash");
    check(ui_text((UIStringId)UI_STRING_COUNT)[0] == '\0',
          "an id past the end is empty, not a crash");

    /* Setting a nonsense language must leave the last good one in place. */
    ui_set_language(UI_LANG_ZH);
    ui_set_language((UILanguage)99);
    check(ui_get_language() == UI_LANG_ZH, "an invalid language is ignored");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
