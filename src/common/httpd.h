/* -*- C -*-
 *
 *  httpd.h -- the parsing half of the launcher's Wi-Fi upload page
 *
 *  Copying a game onto the console means FTP or taking the card out.  The
 *  launcher can instead answer a browser: open the address it shows, pick
 *  a .zip, and it lands in ux0:data/game_zips/ where the installer already
 *  looks for it.
 *
 *  Everything here is the part that has nothing to do with the console: a
 *  request line and its headers, the boundary a browser wraps an upload
 *  in, and the name a file is allowed to be given on the card.  A
 *  multipart body arrives in whatever sized pieces the network hands over,
 *  and a parser that mishandles a boundary split across two of them writes
 *  a corrupt archive that looks like a corrupt download -- which is why
 *  this is portable C with host tests rather than code that can only be
 *  exercised over Wi-Fi.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The port the launcher listens on. */
#define HTTPD_PORT 8080

#define HTTP_MAX_PATH     256
#define HTTP_MAX_BOUNDARY 80
/* Feed no more than this to http_multipart_feed() at a time: the parser
 * holds back at most one boundary's worth of bytes and works in a fixed
 * buffer sized from the pair. */
#define HTTP_FEED_MAX     4096

enum http_method {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST
};

struct http_request {
    int    method;
    char   path[HTTP_MAX_PATH];
    long   content_length;              /* -1 when the header is absent */
    char   boundary[HTTP_MAX_BOUNDARY]; /* empty unless multipart/form-data */
};

/* Parse a request's head -- the request line and its headers, up to but not
 * including the blank line.  Returns 1 on success, 0 if it is not a request
 * this server can answer. */
int http_parse_request(const char *head, struct http_request *out);

/* Offset of the end of the head within buffer, i.e. the first byte of the
 * body, or -1 while the blank line has not arrived yet. */
long http_head_length(const char *buffer, size_t len);

/* "200 OK", "404 Not Found", ... */
const char *http_status_text(int code);

/* The name an uploaded file may be given under ux0:data/game_zips/.
 *
 * A browser sends whatever the file was called, which can be a path, can
 * be in any alphabet, and can be chosen by whoever is on the network.  The
 * result keeps only characters that are safe in a name here, never
 * escapes the folder, and always ends in ".zip" -- the folder is scanned
 * for archives, so a file that is not one has no business in it.
 * Returns 1 on success, 0 if nothing usable is left. */
int http_upload_name(const char *filename, char *out, size_t n);

/*
 * The streaming multipart parser.
 */

/* Called once per file part, with the name the browser sent.  Return 0 to
 * accept the part, anything else to abort the upload. */
typedef int (*http_part_cb)(void *user, const char *filename);
/* Called with the file's bytes, in order.  Return 0 to continue. */
typedef int (*http_data_cb)(void *user, const void *data, size_t len);

typedef struct http_multipart http_multipart;

http_multipart *http_multipart_begin(const char *boundary,
                                     http_part_cb on_part,
                                     http_data_cb on_data,
                                     void *user);

/* Feed the next piece of the body, at most HTTP_FEED_MAX bytes.  Returns 0
 * while the upload is healthy, -1 if it is malformed or a callback asked
 * to stop. */
int http_multipart_feed(http_multipart *m, const void *data, size_t len);

/* True once the closing boundary has been seen, i.e. the file is whole.
 * A connection that stops before this delivered a partial file. */
int http_multipart_complete(const http_multipart *m);

void http_multipart_end(http_multipart *m);

#ifdef __cplusplus
}
#endif

#endif /* __HTTPD_H__ */
