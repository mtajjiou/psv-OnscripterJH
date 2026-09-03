/* Host tests for the game metadata cache.
 *
 * The manifest is a cache, so the interesting cases are all the ways a
 * cached answer could be wrong rather than absent: a stale stamp, a name
 * carrying the characters that end a JSON string, a file cut off halfway.
 * Being wrong here means a game that will not start; being absent costs a
 * second of rescanning.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "manifest.h"

static int checks = 0;
static int failures = 0;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    fwrite(text, 1, strlen(text), f);
    fclose(f);
}

static manifest_entry make(const char *folder, const char *root,
                           const char *name, const char *stamp, uint64_t size)
{
    manifest_entry e;
    memset(&e, 0, sizeof(e));
    snprintf(e.folder, sizeof(e.folder), "%s", folder);
    snprintf(e.root, sizeof(e.root), "%s", root);
    snprintf(e.name, sizeof(e.name), "%s", name);
    snprintf(e.stamp, sizeof(e.stamp), "%s", stamp);
    e.size = size;
    return e;
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : ".";
    char path[512];
    manifest m, back;
    manifest_entry e;
    const manifest_entry *found;

    snprintf(path, sizeof(path), "%s/manifest.json", dir);

    /* --- a round trip keeps every field ------------------------------- */
    manifest_init(&m);
    e = make("ux0:onsemu/Tsukihime", "ux0:onsemu/Tsukihime", "Tsukihime",
             "1756900000", 734003200ULL);
    check(manifest_put(&m, &e) == 1, "put returns success");
    e = make("ux0:onsemu/Narcissu", "ux0:onsemu/Narcissu/game", "Narcissu 2",
             "1756900001", 104857600ULL);
    manifest_put(&m, &e);
    check(m.count == 2, "two entries");
    check(manifest_save(&m, path) == 1, "save succeeds");

    manifest_init(&back);
    check(manifest_load(&back, path) == 1, "load succeeds");
    check(back.count == 2, "both entries came back");

    found = manifest_find(&back, "ux0:onsemu/Narcissu", "1756900001");
    check(found != NULL, "the second entry is found");
    if (found) {
        check(strcmp(found->root, "ux0:onsemu/Narcissu/game") == 0,
              "the resolved root survives");
        check(strcmp(found->name, "Narcissu 2") == 0, "the name survives");
        check(found->size == 104857600ULL, "the size survives");
    }
    manifest_free(&back);
    manifest_free(&m);

    /* --- a changed folder is a miss, not a stale hit ------------------- */
    manifest_init(&m);
    e = make("ux0:onsemu/Game", "ux0:onsemu/Game", "Game", "111", 1);
    manifest_put(&m, &e);
    manifest_save(&m, path);
    manifest_free(&m);

    manifest_init(&back);
    manifest_load(&back, path);
    check(manifest_find(&back, "ux0:onsemu/Game", "111") != NULL,
          "the same stamp hits");
    check(manifest_find(&back, "ux0:onsemu/Game", "222") == NULL,
          "a different stamp misses");
    check(manifest_find(&back, "ux0:onsemu/Other", "111") == NULL,
          "an unknown folder misses");
    manifest_free(&back);

    /* --- names carrying the characters that would end the string ------ */
    manifest_init(&m);
    e = make("ux0:onsemu/Odd", "ux0:onsemu/Odd",
             "He said \"hi\" \\ then \t left", "1", 0);
    manifest_put(&m, &e);
    manifest_save(&m, path);
    manifest_free(&m);

    manifest_init(&back);
    check(manifest_load(&back, path) == 1, "a quoted name still parses");
    found = manifest_find(&back, "ux0:onsemu/Odd", "1");
    check(found != NULL, "the odd entry is found");
    if (found)
        check(strcmp(found->name, "He said \"hi\" \\ then \t left") == 0,
              "quotes, backslashes and tabs come back unchanged");
    manifest_free(&back);

    /* --- utf-8 names are bytes, and stay bytes ------------------------ */
    manifest_init(&m);
    e = make("ux0:onsemu/JP", "ux0:onsemu/JP", "\xE6\x9C\x88\xE5\xA7\xAB", "1", 0);
    manifest_put(&m, &e);
    manifest_save(&m, path);
    manifest_free(&m);

    manifest_init(&back);
    manifest_load(&back, path);
    found = manifest_find(&back, "ux0:onsemu/JP", "1");
    check(found && strcmp(found->name, "\xE6\x9C\x88\xE5\xA7\xAB") == 0,
          "a japanese name survives the round trip");
    manifest_free(&back);

    /* --- replacing an entry rather than adding a second --------------- */
    manifest_init(&m);
    e = make("ux0:onsemu/Same", "a", "first", "1", 1);
    manifest_put(&m, &e);
    e = make("ux0:onsemu/Same", "b", "second", "2", 2);
    manifest_put(&m, &e);
    check(m.count == 1, "putting the same folder twice replaces it");
    found = manifest_find(&m, "ux0:onsemu/Same", "2");
    check(found && strcmp(found->name, "second") == 0, "the newer one wins");
    manifest_free(&m);

    /* --- anything unreadable is no cache, never half a cache ---------- */
    manifest_init(&back);
    check(manifest_load(&back, "/nonexistent/manifest.json") == 0,
          "a missing file loads nothing");
    check(back.count == 0, "and leaves the manifest empty");
    manifest_free(&back);

    write_file(path, "");
    manifest_init(&back);
    check(manifest_load(&back, path) == 0, "an empty file loads nothing");
    manifest_free(&back);

    write_file(path, "this is not json at all");
    manifest_init(&back);
    check(manifest_load(&back, path) == 0, "rubbish loads nothing");
    manifest_free(&back);

    write_file(path, "[{\"folder\": \"a\", \"name\": \"unterm");
    manifest_init(&back);
    check(manifest_load(&back, path) == 0, "a truncated string loads nothing");
    check(back.count == 0, "and leaves nothing behind");
    manifest_free(&back);

    write_file(path, "[{\"folder\": \"a\", \"stamp\": \"1\"},\n"
                     " {\"folder\": \"b\", \"stamp\": \"2\", \"cut\": ");
    manifest_init(&back);
    check(manifest_load(&back, path) == 0,
          "a file cut off mid-entry is refused whole");
    manifest_free(&back);

    /* --- a field this build does not know is skipped, not fatal ------- */
    write_file(path, "[{\"folder\": \"a\", \"root\": \"a\", \"name\": \"A\", "
                     "\"stamp\": \"1\", \"size\": 5, \"played\": \"tomorrow\"}]");
    manifest_init(&back);
    check(manifest_load(&back, path) == 1,
          "an unknown field does not spoil the entry");
    found = manifest_find(&back, "a", "1");
    check(found && found->size == 5, "and the fields it does know are right");
    manifest_free(&back);

    /* --- an empty manifest is a valid one ----------------------------- */
    manifest_init(&m);
    check(manifest_save(&m, path) == 1, "an empty manifest saves");
    manifest_init(&back);
    check(manifest_load(&back, path) == 1, "and loads");
    check(back.count == 0, "as empty");
    manifest_free(&back);
    manifest_free(&m);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
