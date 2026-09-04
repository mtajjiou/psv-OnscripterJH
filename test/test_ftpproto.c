/* Host-side tests for the FTP client's reading half.
 *
 * Every check here stands for a way a save sync fails in a manner the
 * player cannot diagnose: a multi-line greeting read as one reply leaves
 * every later reply off by one, a mis-parsed PASV connects to a port
 * nothing is listening on and hangs, and a remote path built from a game
 * folder's own name is a path chosen by whoever named the folder.
 */
#include <stdio.h>
#include <string.h>

#include "ftpproto.h"

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

static void test_replies(void) {
    check(ftp_reply_code("220 ready") == 220, "a reply's code is read");
    check(ftp_reply_code("331-more to come") == 331, "so is a continued one");
    check(ftp_reply_is_final("220 ready"), "a space ends a reply");
    check(!ftp_reply_is_final("220-hello there"),
          "a hyphen means another line follows");
    check(ftp_reply_code("-rw-r--r-- 1 me me 4096 x") == -1,
          "a listing line is not a reply");
    check(ftp_reply_code("2 ready") == -1, "nor is a short number");
    check(ftp_reply_code(NULL) == -1, "nor is nothing");

    check(ftp_reply_ok(226), "a transfer that completed is a success");
    check(ftp_reply_ok(150), "so is one that has started");
    check(ftp_reply_ok(331), "and a request for a password");
    check(!ftp_reply_ok(530), "a refused login is not");
    check(!ftp_reply_ok(550), "nor is a missing file");
}

static void test_pasv(void) {
    char host[32];
    int port = 0;

    check(ftp_parse_pasv("227 Entering Passive Mode (10,0,0,5,196,24).",
                         host, sizeof(host), &port) == 1,
          "a passive-mode reply parses");
    check_str(host, "10.0.0.5", "with the server's address");
    check(port == 196 * 256 + 24, "and the port its two halves make");

    check(ftp_parse_pasv("227 =192,168,1,20,4,1", host, sizeof(host), &port) == 1,
          "a reply without brackets parses too");
    check_str(host, "192.168.1.20", "with its address");
    check(port == 1025, "and its port");

    check(ftp_parse_pasv("227 Entering Passive Mode (10,0,0,5,196)",
                         host, sizeof(host), &port) == 0,
          "a reply short of a number is refused");
    check(ftp_parse_pasv("227 Entering Passive Mode (10,0,0,300,196,24)",
                         host, sizeof(host), &port) == 0,
          "so is one whose numbers are not bytes");

    port = 0;
    check(ftp_parse_epsv("229 Entering Extended Passive Mode (|||50000|)",
                         &port) == 1, "an extended passive reply parses");
    check(port == 50000, "with its port");
    check(ftp_parse_epsv("229 Entering Extended Passive Mode (|50000|)",
                         &port) == 0, "a malformed one is refused");
}

static void test_size(void) {
    long size = 0;

    check(ftp_parse_size("213 4096", &size) == 1, "a size reply parses");
    check(size == 4096, "as the number it carries");
    check(ftp_parse_size("550 No such file", &size) == 0,
          "a refusal carries no size");
}

static void test_paths(void) {
    char path[FTP_MAX_PATH];

    check(ftp_join_path("/saves", "MyGame", "save1.dat", path, sizeof(path)) == 1,
          "a path is built");
    check_str(path, "/saves/MyGame/save1.dat", "from its three parts");

    check(ftp_join_path("/saves/", "MyGame", NULL, path, sizeof(path)) == 1,
          "a trailing slash on the base is allowed");
    check_str(path, "/saves/MyGame", "and does not double up");

    check(ftp_join_path(NULL, "MyGame", "envdata", path, sizeof(path)) == 1,
          "no base means the root");
    check_str(path, "/MyGame/envdata", "which is where it goes");

    check(ftp_join_path("/saves", "..", "save1.dat", path, sizeof(path)) == 0,
          "a game folder cannot climb out of the base");
    check(ftp_join_path("/saves", "a/b", "save1.dat", path, sizeof(path)) == 0,
          "nor bring its own separator");
    check(ftp_join_path("/saves", "game\r\nQUIT", "x", path, sizeof(path)) == 0,
          "nor forge a second command");
    check(ftp_join_path("saves", "g", "x", path, sizeof(path)) == 0,
          "a relative base is refused");
}

static void test_save_names(void) {
    check(ftp_is_save_name("save1.dat"), "a slot is a save");
    check(ftp_is_save_name("save20.dat"), "however many slots in");
    check(ftp_is_save_name("SAVE3.DAT"), "whatever its case");
    check(ftp_is_save_name("gloval.sav"), "the global data is a save");
    check(ftp_is_save_name("envdata"), "so is the environment");
    check(ftp_is_save_name("kidoku.dat"), "and the read-text record");

    check(!ftp_is_save_name("save.dat"), "a slot needs a number");
    check(!ftp_is_save_name("saved.txt"), "a name that only starts alike is not");
    check(!ftp_is_save_name("nscript.dat"), "the script is not a save");
    check(!ftp_is_save_name("arc.nsa"), "nor is an archive");
    check(!ftp_is_save_name(NULL), "nor is nothing");
}

static void test_listing(void) {
    char name[128];

    check(ftp_parse_list_line(
              "-rw-r--r-- 1 user group 4096 Jan  1 00:00 save1.dat",
              name, sizeof(name)) == 1, "a unix listing line parses");
    check_str(name, "save1.dat", "as its file name");

    check(ftp_parse_list_line(
              "-rw-r--r-- 1 user group 4096 Jan  1 00:00 my save.dat",
              name, sizeof(name)) == 1, "a name with a space parses");
    check_str(name, "my save.dat", "whole");

    check(ftp_parse_list_line(
              "drwxr-xr-x 2 user group 4096 Jan  1 00:00 MyGame",
              name, sizeof(name)) == 0, "a directory is not a file");

    check(ftp_parse_list_line("01-01-24  12:00AM      4096 save1.dat",
                              name, sizeof(name)) == 1,
          "a DOS listing line parses");
    check_str(name, "save1.dat", "as its file name");
    check(ftp_parse_list_line("01-01-24  12:00AM      <DIR>      MyGame",
                              name, sizeof(name)) == 0,
          "a DOS directory is not a file");

    check(ftp_parse_list_line("total 24", name, sizeof(name)) == 0,
          "the total line is not a file");
}

int main(void) {
    test_replies();
    test_pasv();
    test_size();
    test_paths();
    test_save_names();
    test_listing();

    printf("ftpproto: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
