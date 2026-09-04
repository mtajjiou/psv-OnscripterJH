/* Host-side tests for the plugin manifests.
 *
 * A plugin is text someone else wrote, applied to a game the launcher did
 * not write either. The failures worth catching are all quiet ones: a
 * plugin that matches every game when it was meant for one, a plugin that
 * matches none, an id that matches another id it merely starts like, and a
 * turned-on list that loses its other entries when one is turned off.
 */
#include <stdio.h>
#include <string.h>

#include "plugins.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

static void check_str(const char *got, const char *want, const char *what) {
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what, got, want);
    }
}

static void test_parsing(void) {
    struct plugin_info plugin;

    const char *manifest =
        "# a font for english patches\n"
        "[plugin]\n"
        "name = English font\n"
        "description = Uses the font this plugin brings\n"
        "match = *\n"
        "args = --font default_en.ttf --fontcache\n"
        "overlay = yes\n";

    check(plugin_parse(manifest, "font-en", &plugin) == 1, "a manifest parses");
    check_str(plugin.id, "font-en", "addressed by its folder");
    check_str(plugin.name, "English font", "named by its manifest");
    check_str(plugin.description, "Uses the font this plugin brings",
              "with its description");
    check(plugin.arg_count == 3, "with all three of its arguments");
    check_str(plugin.args[0], "--font", "the first as written");
    check_str(plugin.args[1], "default_en.ttf", "the second");
    check_str(plugin.args[2], "--fontcache", "and the third");
    check(plugin.overlay == 1, "and its files");

    /* The smallest thing that is still a plugin. */
    check(plugin_parse("args = --window\n", "window", &plugin) == 1,
          "one argument is enough to be a plugin");
    check_str(plugin.name, "window", "and it is named after its folder");
    check_str(plugin.match, "*", "and offered for every game");

    check(plugin_parse("name = does nothing\n", "empty", &plugin) == 0,
          "a plugin that adds nothing is not offered");
    check(plugin_parse("", "empty", &plugin) == 0, "nor is an empty manifest");
    check(plugin_parse("args = --window\n", "", &plugin) == 0,
          "nor one with no folder to be addressed by");

    /* A manifest written for a later launcher than this one. */
    check(plugin_parse("args = --window\nrequires_ondemand_shaders = 3\n",
                       "future", &plugin) == 1,
          "a key this version has never heard of is ignored, not refused");
    check(plugin.arg_count == 1, "and what it does understand still applies");

    /* Too many arguments: the ones that fit are kept rather than the
     * plugin being thrown away. */
    check(plugin_parse("args = -a -b -c -d -e -f -g -h -i -j\n",
                       "many", &plugin) == 1, "a long argument list parses");
    check(plugin.arg_count == PLUGIN_MAX_ARGS, "up to what is held");
}

static void test_matching(void) {
    struct plugin_info plugin;

    plugin_parse("match = *\nargs = --window\n", "any", &plugin);
    check(plugin_matches(&plugin, "MyGame"), "a * plugin is offered for a game");
    check(plugin_matches(&plugin, "Another"), "and for another");

    plugin_parse("match = higurashi\nargs = --window\n", "one", &plugin);
    check(plugin_matches(&plugin, "Higurashi_01"),
          "a named plugin matches its game whatever the case");
    check(!plugin_matches(&plugin, "Clannad"), "and not another game");
    check(!plugin_matches(&plugin, "higu"),
          "and not a name that is only part of what it asked for");
    check(!plugin_matches(&plugin, NULL), "and not nothing at all");
}

static void test_enabled_list(void) {
    char list[128];

    check(!plugin_enabled("", "font-en"), "nothing is turned on to begin with");

    check(plugin_list_set("", "font-en", 1, list, sizeof(list)) == 1,
          "turning one on works");
    check_str(list, "font-en", "and it is the list");
    check(plugin_enabled(list, "font-en"), "and it reads back as on");

    check(plugin_list_set(list, "widescreen", 1, list, sizeof(list)) == 1,
          "turning a second on works");
    check_str(list, "font-en widescreen", "and both are in the list");

    check(plugin_list_set(list, "font-en", 0, list, sizeof(list)) == 1,
          "turning the first off works");
    check_str(list, "widescreen", "and leaves the other alone");

    /* The failure this exists to prevent: an id that another id starts
     * with must not read as the same plugin. */
    check(!plugin_enabled("font-en", "font"),
          "an id is matched whole, not as a prefix");
    check(plugin_list_set("font-en", "font", 0, list, sizeof(list)) == 1,
          "turning off a plugin that is not on works");
    check_str(list, "font-en", "and takes nothing else with it");

    check(plugin_list_set("a b", "c", 1, list, 4) == 0,
          "a list that will not fit is refused rather than truncated");
}

int main(void) {
    test_parsing();
    test_matching();
    test_enabled_list();

    printf("plugins: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
