/* Host-side tests for the format support table.
 *
 * The table is a claim made to the user about their files, so what is
 * checked here is the shape that makes it usable: every row filled in,
 * categories in contiguous blocks so a screen can draw them as sections,
 * no extension claimed by two rows, and a lookup that survives the shapes
 * real script paths come in.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "formats.h"

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

static void expect_lookup(const char *path, const char *want_name)
{
    const FormatEntry *e = formats_lookup(path);
    const char *got = e ? e->name : "(none)";

    checks++;
    if (want_name == NULL) {
        if (e != NULL) {
            failures++;
            printf("FAIL: %s matched %s, expected nothing\n", path, got);
        }
        return;
    }
    if (e == NULL || strcmp(e->name, want_name) != 0) {
        failures++;
        printf("FAIL: %s matched %s, expected %s\n", path, got, want_name);
    }
}

/* The README carries the same table, for people deciding what to convert
 * before they ever boot the launcher.  Two copies of a list drift, so the
 * one in the tree is checked against this one rather than trusted: every
 * row must appear in README.md, with the same extensions and verdict. */
static void check_readme(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char *text;
    long len;
    int i;

    if (fp == NULL) {
        printf("FAIL: cannot read %s\n", path);
        checks++; failures++;
        return;
    }
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    text = (char *)malloc((size_t)len + 1);
    if (text == NULL || len <= 0 || fread(text, 1, (size_t)len, fp) != (size_t)len) {
        printf("FAIL: cannot read %s\n", path);
        checks++; failures++;
        free(text);
        fclose(fp);
        return;
    }
    text[len] = '\0';
    fclose(fp);

    for (i = 0; i < formats_count(); i++) {
        const FormatEntry *e = formats_get(i);
        char row[256];

        snprintf(row, sizeof(row), "| %s | `%s` | %s |",
                 e->name, e->extensions, formats_support_word(e->support));
        checks++;
        if (strstr(text, row) == NULL) {
            failures++;
            printf("FAIL: README.md has no row for %s (expected \"%s\")\n",
                   e->name, row);
        }
    }

    free(text);
}

int main(int argc, char **argv)
{
    const int n = formats_count();
    int i, j;

    check(n > 0, "the table has rows");
    check(formats_get(-1) == NULL, "a negative index gives nothing");
    check(formats_get(n) == NULL, "one past the end gives nothing");

    for (i = 0; i < n; i++) {
        const FormatEntry *e = formats_get(i);
        check(e != NULL, "every index in range gives a row");
        if (e == NULL) continue;

        check(e->name && e->name[0], "every row is named");
        check(e->extensions != NULL, "every row lists its extensions");
        check(e->note != NULL, "every row has a note, even an empty one");
        check(e->category >= 0 && e->category < FORMAT_CATEGORY_COUNT,
              "every row is in a real category");
        check(formats_support_word(e->support)[0] != '\0',
              "every row's support level has a word for it");
        /* A row the user cannot act on is worse than no row: anything not
         * simply playable has to say what to do about it. */
        if (e->support == FORMAT_CONVERT)
            check(e->note[0] != '\0', "a row that needs converting says so");
    }

    /* Contiguous blocks, so a screen can draw one heading per category. */
    for (i = 0; i < FORMAT_CATEGORY_COUNT; i++) {
        int first = -1, count = -1, k;
        check(formats_category_range((FormatCategory)i, &first, &count) == 1,
              "every category is in the table");
        if (first < 0) continue;
        check(count > 0, "a category has rows");
        for (k = first; k < first + count; k++)
            check(formats_get(k)->category == (FormatCategory)i,
                  "a category's rows are contiguous");
    }
    {
        int first = -1, count = -1;
        check(formats_category_range((FormatCategory)FORMAT_CATEGORY_COUNT,
                                     &first, &count) == 0,
              "a category that is not in the table is refused");
        check(first == -1 && count == -1,
              "a refused range leaves the outputs alone");
    }

    /* No extension in two rows: whichever came first would win silently,
     * and the loser's note would never be shown. */
    for (i = 0; i < n; i++) {
        char list[128];
        char *tok;
        snprintf(list, sizeof(list), "%s", formats_get(i)->extensions);
        for (tok = strtok(list, " "); tok; tok = strtok(NULL, " ")) {
            const FormatEntry *found = formats_lookup(tok);
            checks++;
            if (found != formats_get(i)) {
                failures++;
                printf("FAIL: %s belongs to row %d but resolves to %s\n",
                       tok, i, found ? found->name : "(none)");
            }
            for (j = 0; j < i; j++)
                check(formats_lookup(tok) != formats_get(j),
                      "no extension is claimed by two rows");
        }
    }

    /* Lookup, on the shapes a script path actually takes. */
    expect_lookup("op.mpg", "MPEG-1/2");
    expect_lookup("OP.MPG", "MPEG-1/2");
    expect_lookup("video\\ending.WMV", "WMV / VC-1");
    expect_lookup("ux0:onsemu/game/bgm/track01.ogg", "Ogg Vorbis");
    expect_lookup("cg/face.PnG", "PNG");
    expect_lookup("readme.txt", NULL);
    expect_lookup("nscript.dat", NULL);
    expect_lookup("noextension", NULL);
    expect_lookup("", NULL);
    expect_lookup(NULL, NULL);
    /* A dot in a folder name is not the file's extension. */
    expect_lookup("my.games/opening", NULL);
    expect_lookup("my.games/opening.mp4", "H.264 in MP4");
    /* A trailing dot names nothing. */
    expect_lookup("weird.", NULL);

    check(strcmp(formats_category_name(FORMAT_CATEGORY_VIDEO), "Video") == 0,
          "categories have display names");
    check(formats_category_name((FormatCategory)FORMAT_CATEGORY_COUNT)[0] == '\0',
          "an unknown category has no name rather than a wrong one");

    /* Given a README to check against, check it. */
    if (argc > 1) check_readme(argv[1]);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
