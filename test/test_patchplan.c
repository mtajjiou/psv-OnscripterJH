/* Host-side tests for the patch (overlay) decisions.
 *
 * A translation patch is the archive the installer used to refuse twice
 * over: it has no script, and the game it belongs to is already there.
 * What is checked here is everything decided before a byte is written --
 * whether an archive is a patch at all, which folder inside it overlays
 * the game, which installed game it is offered against first, and that the
 * record written for it reads back the way it was written, since that
 * record is what removing a patch depends on.
 */
#include <stdio.h>
#include <string.h>

#include "patchplan.h"
#include "archive.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void check_str(const char *got, const char *want, const char *what) {
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what, got, want);
    }
}

static archive *open_fixture(const char *dir, const char *name) {
    char path[512];
    int err = 0;
    archive *z;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    z = archive_open(path, &err);
    if (z == NULL) {
        printf("FAIL: cannot open fixture %s (%s)\n", name,
               zip_error_string(err));
        failures++;
    }
    return z;
}

static void test_kinds(const char *dir) {
    archive *z;
    char root[ZIP_MAX_NAME];

    z = open_fixture(dir, "nested.zip");
    if (z) {
        check(patch_archive_kind(z) == PATCH_KIND_GAME,
              "an archive with a script is a game, not a patch");
        archive_close(z);
    }

    z = open_fixture(dir, "patch.zip");
    if (z) {
        check(patch_archive_kind(z) == PATCH_KIND_PATCH,
              "an archive with no script is a patch");
        check(patch_overlay_root(z, root, sizeof(root)) == 1,
              "a patch has an overlay root");
        check_str(root, "MyGame English Patch v2",
                  "the wrapping folder is stripped, not installed");
        archive_close(z);
    }

    z = open_fixture(dir, "patch_flat.zip");
    if (z) {
        check(patch_archive_kind(z) == PATCH_KIND_PATCH,
              "an unwrapped patch is a patch");
        check(patch_overlay_root(z, root, sizeof(root)) == 1,
              "an unwrapped patch has an overlay root");
        check_str(root, "", "an unwrapped patch overlays from the archive root");
        archive_close(z);
    }

    z = open_fixture(dir, "empty.zip");
    if (z) {
        check(patch_archive_kind(z) == PATCH_KIND_EMPTY,
              "an archive with no files is neither a game nor a patch");
        check(patch_overlay_root(z, root, sizeof(root)) == 0,
              "an empty archive has nothing to overlay");
        archive_close(z);
    }

    /* noscript.zip carries a readme and a data folder: no script, so a
     * patch, and its entries do not share one top-level folder. */
    z = open_fixture(dir, "noscript.zip");
    if (z) {
        check(patch_archive_kind(z) == PATCH_KIND_PATCH,
              "a script-less archive of loose files is a patch");
        patch_overlay_root(z, root, sizeof(root));
        check_str(root, "", "entries with no shared folder overlay from the root");
        archive_close(z);
    }
}

/* The same questions of the same contents in the other container: a mod
 * arrives as a .7z as often as a .zip, and every decision here has to come
 * out the same.  Skipped when the fixtures are absent, which is when py7zr
 * is not installed. */
static void test_kinds_7z(const char *dir) {
    char path[512];
    char root[ZIP_MAX_NAME];
    int err = 0;
    archive *z;

    snprintf(path, sizeof(path), "%s/patch.7z", dir);
    z = archive_open(path, &err);
    if (z == NULL) {
        printf("SKIPPED the 7z checks: fixtures need py7zr\n");
        return;
    }

    check(patch_archive_kind(z) == PATCH_KIND_PATCH,
          "a 7z with no script in it is a patch, as a zip would be");
    check(patch_overlay_root(z, root, sizeof(root)) == 1,
          "a 7z patch has an overlay root");
    check_str(root, "MyGame English Patch v2",
              "and the wrapping folder is stripped from it too");
    archive_close(z);

    snprintf(path, sizeof(path), "%s/nested.7z", dir);
    z = archive_open(path, &err);
    if (z) {
        check(patch_archive_kind(z) == PATCH_KIND_GAME,
              "and a 7z with a script in it is a game");
        archive_close(z);
    }
}

static void test_matching(void) {
    /* The case this exists for: the patch names the game and adds words. */
    check(patch_name_match("MyGame English Patch v2", "MyGame") >= 70,
          "a patch named after the game matches it");
    check(patch_name_match("higurashi_eng_patch_v1.03", "Higurashi") >= 70,
          "punctuation, case and version do not break the match");
    check(patch_name_match("Tsukihime full voice patch", "Tsukihime") >= 70,
          "a voice patch matches its game");

    /* Same name, different spelling of the same thing. */
    check(patch_name_match("MyGame", "my game") == 100,
          "spacing and case alone are the same name");

    /* And the case it must not get wrong: two unrelated games. */
    check(patch_name_match("MyGame English Patch", "Clannad") == 0,
          "an unrelated game does not match");
    check(patch_name_match("patch v2", "Clannad") == 0,
          "a patch that names no game matches nothing");
    check(patch_name_match("", "Clannad") == 0, "an empty name matches nothing");

    check(patch_name_match("Higurashi Patch", "Higurashi") >
          patch_name_match("Higurashi Patch", "Higu"),
          "the closer of two candidate games scores higher");
}

/* The check made before a mod is applied from the game panel: is this
 * archive for this game?  Both halves are guesses, so what is checked here
 * is that they add up the way the warning depends on. */
static void test_confidence(void) {
    /* A translation patch: named after the game, replacing its files. */
    check(patch_confidence(90, 10, 8) >= PATCH_CONFIDENCE_SURE,
          "a patch named after the game, replacing its files, is applied");
    /* The same patch under a release group's name that matches nothing. */
    check(patch_confidence(0, 10, 8) >= PATCH_CONFIDENCE_SURE,
          "what its files are is worth more than what it is called");

    /* The case the warning exists for: a patch for another game. */
    check(patch_confidence(0, 10, 0) < PATCH_CONFIDENCE_SURE,
          "a patch that shares no file and no name is warned about");
    check(patch_confidence(20, 10, 0) < PATCH_CONFIDENCE_SURE,
          "a weak name does not rescue it");

    /* A voice pack that only adds files is legitimate, and its name is
     * then the only evidence there is. */
    check(patch_confidence(100, 10, 0) >= PATCH_CONFIDENCE_SURE,
          "a patch that only adds files is believed on a name that matches");

    /* Some overlap is enough to act on. */
    check(patch_confidence(0, 10, 1) >= PATCH_CONFIDENCE_SURE,
          "one file in common is enough to stop warning");

    check(patch_confidence(80, 0, 0) == 80,
          "an archive with no files is judged on its name alone");
    check(patch_confidence(0, 0, 0) == 0, "and one with neither is not judged");
    check(patch_confidence(150, 10, 20) <= 100,
          "figures outside their range do not escape the scale");
}

static void test_records(void) {
    char kind = 0;
    char path[ZIP_MAX_NAME];
    char name[128];

    check(patch_parse_line("R arc.nsa", &kind, path, sizeof(path)) == 1,
          "a replaced-file line parses");
    check(kind == PATCH_LINE_REPLACED, "its kind is 'replaced'");
    check_str(path, "arc.nsa", "its path is the file");

    check(patch_parse_line("N extra/font.ttf\n", &kind, path, sizeof(path)) == 1,
          "a new-file line parses, newline and all");
    check(kind == PATCH_LINE_NEW, "its kind is 'new'");
    check_str(path, "extra/font.ttf", "a path with a folder survives");

    check(patch_parse_line("", &kind, path, sizeof(path)) == 0,
          "a blank line is not a record");
    check(patch_parse_line("X arc.nsa", &kind, path, sizeof(path)) == 0,
          "an unknown kind is not a record");
    check(patch_parse_line("R ", &kind, path, sizeof(path)) == 0,
          "a record with no path is not a record");

    check(patch_record_name("ux0:data/game_zips/MyGame English Patch v2.zip",
                            name, sizeof(name)) == 1,
          "a record name is derived from the archive");
    check_str(name, "mygame_english_patch_v2.mod",
              "and holds only what a file name can hold");
    check(patch_record_name("ux0:data/game_zips/\xe6\x9c\x88\xe5\xa7\xab.zip",
                            name, sizeof(name)) == 1,
          "a non-ASCII archive name still yields a record name");
    check_str(name, "patch.mod", "which falls back to a plain one");
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";

    test_kinds(dir);
    test_kinds_7z(dir);
    test_matching();
    test_confidence();
    test_records();

    printf("patchplan: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
