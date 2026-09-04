/* Host-side tests for the Wi-Fi upload page's parsing.
 *
 * The interesting failure is invisible over Wi-Fi: a body arrives in
 * whatever pieces the network chose, and a parser that mishandles a
 * boundary split across two of them writes a file that is subtly wrong.
 * So the same upload is fed here one byte at a time, in odd-sized pieces,
 * and whole, and the bytes that come out are compared with the bytes that
 * went in.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httpd.h"

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

/* --- what the parser collects ---------------------------------------- */

struct collector {
    char   name[256];
    char  *data;
    size_t len;
    int    parts;
};

static int on_part(void *user, const char *filename) {
    struct collector *c = (struct collector *)user;
    snprintf(c->name, sizeof(c->name), "%s", filename);
    c->parts++;
    return 0;
}

static int on_data(void *user, const void *data, size_t len) {
    struct collector *c = (struct collector *)user;
    c->data = (char *)realloc(c->data, c->len + len);
    memcpy(c->data + c->len, data, len);
    c->len += len;
    return 0;
}

static void test_request_head(void) {
    struct http_request r;
    const char *get =
        "GET /?after=install HTTP/1.1\r\n"
        "Host: 10.0.0.5:8080\r\n"
        "Accept: */*\r\n";
    const char *post =
        "POST /upload HTTP/1.1\r\n"
        "Host: 10.0.0.5:8080\r\n"
        "content-length: 4096\r\n"
        "Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryABC123\r\n";

    check(http_parse_request(get, &r) == 1, "a GET parses");
    check(r.method == HTTP_METHOD_GET, "as a GET");
    check_str(r.path, "/", "with its query string dropped");
    check(r.content_length == -1, "and no content length");
    check(r.boundary[0] == '\0', "and no boundary");

    check(http_parse_request(post, &r) == 1, "a POST parses");
    check(r.method == HTTP_METHOD_POST, "as a POST");
    check_str(r.path, "/upload", "with its path");
    check(r.content_length == 4096, "and its length, header name in any case");
    check_str(r.boundary, "----WebKitFormBoundaryABC123", "and its boundary");

    check(http_parse_request("PUT /x HTTP/1.1\r\n", &r) == 0,
          "a method the server does not answer is refused");
    check(http_parse_request("nonsense\r\n", &r) == 0, "so is nonsense");

    check(http_head_length("GET / HTTP/1.1\r\nHost: x\r\n\r\nbody", 30) == 27,
          "the body starts after the blank line");
    check(http_head_length("GET / HTTP/1.1\r\nHost: x\r\n", 25) == -1,
          "and there is no body until the blank line has arrived");
}

static void test_upload_names(void) {
    char name[128];

    check(http_upload_name("MyGame.zip", name, sizeof(name)) == 1,
          "a plain name is accepted");
    check_str(name, "MyGame.zip", "unchanged");

    check(http_upload_name("C:\\Users\\me\\Downloads\\My Game (v2).ZIP",
                           name, sizeof(name)) == 1,
          "a Windows path is accepted");
    check_str(name, "My Game (v2).zip", "as its last component");

    check(http_upload_name("../../ux0_data/evil.zip", name, sizeof(name)) == 1,
          "a name trying to climb out is accepted");
    check_str(name, "evil.zip", "with the climbing removed");

    check(http_upload_name("\xe6\x9c\x88\xe5\xa7\xab.zip", name, sizeof(name)) == 1,
          "a Japanese name is accepted");
    check_str(name, "upload.zip",
              "under a plain name, since nothing of it survives the fold");

    check(http_upload_name("notes.txt", name, sizeof(name)) == 0,
          "a file that is not an archive is refused");
    check(http_upload_name(".zip", name, sizeof(name)) == 0,
          "so is a name that is only a suffix");
    check(http_upload_name(NULL, name, sizeof(name)) == 0, "and no name at all");
}

/* Build one multipart body with a file part, a form field, and a payload
 * that contains something close to the boundary. */
static char *build_body(const char *boundary, const char *content,
                        size_t content_len, size_t *out_len) {
    char head[512];
    char tail[256];
    size_t hl, tl;
    char *body;

    hl = (size_t)snprintf(head, sizeof(head),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"note\"\r\n"
        "\r\n"
        "from a browser\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"My Game.zip\"\r\n"
        "Content-Type: application/zip\r\n"
        "\r\n", boundary, boundary);
    tl = (size_t)snprintf(tail, sizeof(tail), "\r\n--%s--\r\n", boundary);

    body = (char *)malloc(hl + content_len + tl);
    memcpy(body, head, hl);
    memcpy(body + hl, content, content_len);
    memcpy(body + hl + content_len, tail, tl);
    *out_len = hl + content_len + tl;
    return body;
}

/* Feed one body in pieces of the given size and check what came out. */
static void feed_in_pieces(const char *boundary, const char *body, size_t len,
                           const char *content, size_t content_len,
                           size_t piece, const char *what) {
    struct collector c;
    http_multipart *m;
    size_t at = 0;
    int rc = 0;

    memset(&c, 0, sizeof(c));
    m = http_multipart_begin(boundary, on_part, on_data, &c);
    if (m == NULL) { failures++; printf("FAIL: %s (no parser)\n", what); return; }

    while (at < len && rc == 0) {
        size_t take = len - at < piece ? len - at : piece;
        rc = http_multipart_feed(m, body + at, take);
        at += take;
    }

    checks++;
    if (rc != 0)
        { failures++; printf("FAIL: %s (parser refused the body)\n", what); }
    else if (!http_multipart_complete(m))
        { failures++; printf("FAIL: %s (upload not seen as finished)\n", what); }
    else if (c.len != content_len || memcmp(c.data, content, content_len) != 0)
        { failures++; printf("FAIL: %s (%zu bytes out of %zu in)\n", what,
                             c.len, content_len); }
    else if (strcmp(c.name, "My Game.zip") != 0)
        { failures++; printf("FAIL: %s (filename \"%s\")\n", what, c.name); }

    http_multipart_end(m);
    free(c.data);
}

static void test_multipart(void) {
    const char *boundary = "----WebKitFormBoundaryABC123";
    size_t content_len = 9000;
    char *content = (char *)malloc(content_len);
    size_t len, i;
    char *body;

    /* Binary, with runs that look like the start of the delimiter and a
     * CRLF pair just before the real one. */
    for (i = 0; i < content_len; i++) content[i] = (char)(i * 7);
    memcpy(content + 100, "\r\n----WebKitFormBoundaryABC12", 29);
    memcpy(content + 4090, "\r\n\r\n--", 6);
    content[content_len - 1] = '\r';

    body = build_body(boundary, content, content_len, &len);

    feed_in_pieces(boundary, body, len, content, content_len, HTTP_FEED_MAX,
                   "an upload fed in full-sized pieces");
    feed_in_pieces(boundary, body, len, content, content_len, 1,
                   "an upload fed one byte at a time");
    feed_in_pieces(boundary, body, len, content, content_len, 7,
                   "an upload fed in sevens");
    feed_in_pieces(boundary, body, len, content, content_len, 1500,
                   "an upload fed in packet-sized pieces");

    /* A body that stops in the middle is not a finished upload. */
    {
        struct collector c;
        http_multipart *m;
        memset(&c, 0, sizeof(c));
        m = http_multipart_begin(boundary, on_part, on_data, &c);
        check(http_multipart_feed(m, body, 2000) == 0,
              "a truncated body parses as far as it goes");
        check(!http_multipart_complete(m),
              "but is not reported as a finished upload");
        http_multipart_end(m);
        free(c.data);
    }

    check(http_multipart_begin("", on_part, on_data, NULL) == NULL,
          "a body with no boundary has no parser");

    free(body);
    free(content);
}

int main(void) {
    test_request_head();
    test_upload_names();
    test_multipart();

    printf("httpd: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
