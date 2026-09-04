/* Host-side integration test for the install decision chain.
 *
 * Not a unit test of any one function: it walks an archive through the
 * same questions the launcher asks before it writes anything, in the same
 * order, and checks the answers together.
 *
 *   1. does this archive open at all
 *   2. is there a game in it, and at what depth
 *   3. what folder will it install into
 *   4. how much space will it need
 *   5. does every entry's name survive the sanitising the writer applies
 *   6. does the content come out byte for byte
 *
 * The steps that touch the memory card -- creating folders, writing files,
 * the journal, the rollback -- are the Vita's and cannot run here.  What
 * can run here is every decision made before the first byte is written,
 * which is where an install goes wrong in a way the player cannot see.
 */
#include <stdio.h>
#include <string.h>
#include <string>

extern "C" {
#include "archive.h"
}
#include "installname.h"

static int failures = 0;
static int checks = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void check_str(const std::string &got, const std::string &want,
                      const char *what) {
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what, got.c_str(),
               want.c_str());
    }
}

/* What the installer would write, without writing it: every entry's name
 * sanitised, relative to the game root it found. */
static bool plan_install(archive *z, const std::string &root,
                         std::string *first_bad, int *files, uint64_t *bytes) {
    char clean[ZIP_MAX_NAME];
    bool ok = true;

    *files = 0;
    *bytes = 0;

    for (int i = 0; i < archive_count(z); i++) {
        const char *name = archive_entry_name(z, i);

        if (!root.empty()) {
            if (strncmp(name, root.c_str(), root.size()) != 0) continue;
            if (name[root.size()] != '/') continue;
            name += root.size() + 1;
        }
        if (name[0] == '\0') continue;

        if (zip_sanitize_name(name, clean, sizeof(clean)) != ZIP_OK) {
            if (first_bad->empty()) *first_bad = name;
            ok = false;
            continue;
        }
        if (archive_entry_is_dir(z, i)) continue;

        (*files)++;
        *bytes += archive_entry_size(z, i);
    }
    return ok;
}

struct sink {
    std::string data;
};

static int sink_write(void *user, const void *bytes, size_t len) {
    ((sink *)user)->data.append((const char *)bytes, len);
    return 0;
}

/* One archive, from "can it be opened" to "is the script's content right". */
static void install_flow(const char *dir, const char *file,
                         const char *want_folder, const char *want_root,
                         const char *want_script, const char *want_content) {
    char path[1024];
    char root_buf[ZIP_MAX_NAME];
    int err = 0;

    snprintf(path, sizeof(path), "%s/%s", dir, file);
    printf("%s\n", file);

    /* 1. It opens. */
    archive *z = archive_open(path, &err);
    check(z != NULL, "the archive opens");
    if (!z) {
        printf("  (%s)\n", zip_error_string(err));
        return;
    }

    /* 2. There is a game in it, and the root is where the script lives --
     *    not the archive's outermost folder, which for a nested release is
     *    a wrapper nobody wants installed. */
    check(archive_find_game_root(z, root_buf, sizeof(root_buf)) == 1,
          "a game is found in it");
    check_str(root_buf, want_root, "the game root is the folder with the script");

    /* 3. The folder it installs into comes from the archive's own name. */
    check_str(install_destination_name(path), want_folder,
              "the destination folder is named after the archive");

    /* 4. and 5. Everything it would write, and whether any name is refused. */
    std::string first_bad;
    int files = 0;
    uint64_t bytes = 0;
    check(plan_install(z, root_buf, &first_bad, &files, &bytes),
          "every entry's name is safe to write");
    if (!first_bad.empty()) printf("  first refused: %s\n", first_bad.c_str());
    check(files > 0, "there is something to write");
    check(bytes > 0, "and it has a size to check free space against");
    check(bytes <= archive_total_size(z),
          "no more than the archive's own total");

    /* 6. The script comes out as it went in.  An install that writes the
     *    right names and the wrong bytes is the worst of the failures,
     *    because everything looks installed. */
    int idx = -1;
    for (int i = 0; i < archive_count(z); i++)
        if (strcmp(archive_entry_name(z, i), want_script) == 0) idx = i;
    check(idx >= 0, "the script is one of the entries");
    if (idx >= 0) {
        sink s;
        check(archive_extract_entry(z, idx, sink_write, &s) == ZIP_OK,
              "the script extracts");
        check_str(s.data, want_content, "byte for byte");
    }

    archive_close(z);
}

int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : ".";

    /* A game at the archive root: nothing to strip. */
    install_flow(dir, "flat.zip", "flat", "",
                 "nscript.dat", "flat game script\n");

    /* The common shape: one folder, which is stripped so the game does not
     * install as onsemu/MyGame/MyGame. */
    install_flow(dir, "nested.zip", "nested", "MyGame",
                 "MyGame/nscript.dat", "nested game script\n");

    /* Two levels, with a decoy script in a backup folder that must not be
     * mistaken for the game. */
    install_flow(dir, "deep.zip", "deep", "outer/inner",
                 "outer/inner/0.txt", "deep game script\n");

    /* A game named in japanese: the folder inside keeps its name, the
     * folder it installs to is folded to ascii, because the engine opens
     * paths as plain bytes. */
    install_flow(dir, "names.zip", "names", "\346\234\210\345\247\253",
                 "\346\234\210\345\247\253/nscript.dat", "japanese folder\n");

    /* The naming rules on the shapes a download's file name takes. */
    printf("names an archive installs as\n");
    check_str(install_destination_name("ux0:data/game_zips/Tsukihime.zip"),
              "Tsukihime", "a plain name");
    check_str(install_destination_name("ux0:data/game_zips/My Game (2004).ZIP"),
              "My_Game_(2004)", "spaces become separators, case is kept");
    check_str(install_destination_name("/tmp/[Group] Title v1.2 [patched].zip"),
              "Group_Title_v1.2_patched",
              "brackets fold to one separator, never two in a row");
    check_str(install_destination_name("\346\234\210\345\247\253.zip"), "game",
              "a name with nothing ascii in it still installs somewhere");
    check_str(install_destination_name("game.zip.zip"), "game.zip",
              "only one suffix comes off");
    check_str(install_destination_name("noextension"), "noextension",
              "a name without one is left alone");
    check_str(install_destination_name(std::string(200, 'x') + ".zip"),
              std::string(64, 'x'), "a very long name is cut to fit a path");
    check(install_has_zip_suffix("a.ZIP"), "the suffix test ignores case");
    check(install_has_zip_suffix(".zip"), "\".zip\" ends in .zip");
    check_str(install_destination_name(".zip"), "game",
              "but a file called nothing but .zip installs as the fallback, "
              "not as a hidden folder");
    check_str(install_base_name("ux0:onsemu/game/thing.txt"), "thing.txt",
              "the last part of a path");
    check_str(install_base_name("a\\b\\c.txt"), "c.txt",
              "with backslashes too");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
