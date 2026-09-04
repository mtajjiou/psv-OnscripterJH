/* -*- C -*-
 *
 *  httpd.c -- see httpd.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdlib.h>
#include <string.h>

#include "httpd.h"

/* --- request head ---------------------------------------------------- */

/* Case-insensitive prefix test, for header names. */
static int starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        char a = *s++, b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static const void *find(const void *haystack, size_t hlen,
                        const void *needle, size_t nlen) {
    const char *h = (const char *)haystack;
    const char *n = (const char *)needle;
    size_t i;

    if (nlen == 0 || hlen < nlen) return NULL;
    for (i = 0; i + nlen <= hlen; i++)
        if (h[i] == n[0] && memcmp(h + i, n, nlen) == 0) return h + i;
    return NULL;
}

long http_head_length(const char *buffer, size_t len) {
    const char *end = (const char *)find(buffer, len, "\r\n\r\n", 4);
    if (end == NULL) return -1;
    return (long)(end - buffer) + 4;
}

/* Copy one header's value into out, stopping at the end of the line. */
static void header_value(const char *line, char *out, size_t n) {
    size_t o = 0;

    while (*line == ' ' || *line == '\t') line++;
    while (*line && *line != '\r' && *line != '\n' && o + 1 < n)
        out[o++] = *line++;
    if (n > 0) out[o] = '\0';
}

int http_parse_request(const char *head, struct http_request *out) {
    const char *p = head;

    if (head == NULL || out == NULL) return 0;

    memset(out, 0, sizeof(*out));
    out->content_length = -1;

    if (starts_with_ci(p, "GET ")) {
        out->method = HTTP_METHOD_GET;
        p += 4;
    }
    else if (starts_with_ci(p, "POST ")) {
        out->method = HTTP_METHOD_POST;
        p += 5;
    }
    else {
        return 0;
    }

    {
        size_t o = 0;
        while (*p && *p != ' ' && *p != '\r' && *p != '\n' &&
               o + 1 < sizeof(out->path))
            out->path[o++] = *p++;
        out->path[o] = '\0';
        if (o == 0) return 0;
        /* A query string is not part of what is being asked for here. */
        {
            char *q = strchr(out->path, '?');
            if (q) *q = '\0';
        }
    }

    /* Walk the header lines. */
    while ((p = strchr(p, '\n')) != NULL) {
        p++;
        if (*p == '\r' || *p == '\0') break;

        if (starts_with_ci(p, "Content-Length:")) {
            char value[32];
            header_value(p + 15, value, sizeof(value));
            out->content_length = strtol(value, NULL, 10);
        }
        else if (starts_with_ci(p, "Content-Type:")) {
            char value[256];
            const char *mark;
            header_value(p + 13, value, sizeof(value));

            mark = strstr(value, "boundary=");
            if (mark) {
                size_t o = 0;
                mark += 9;
                if (*mark == '"') mark++;
                while (*mark && *mark != '"' && *mark != ';' && *mark != ' ' &&
                       o + 1 < sizeof(out->boundary))
                    out->boundary[o++] = *mark++;
                out->boundary[o] = '\0';
            }
        }
    }

    return 1;
}

const char *http_status_text(int code) {
    switch (code) {
    case 200: return "200 OK";
    case 303: return "303 See Other";
    case 400: return "400 Bad Request";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 413: return "413 Payload Too Large";
    case 500: return "500 Internal Server Error";
    }
    return "500 Internal Server Error";
}

int http_upload_name(const char *filename, char *out, size_t n) {
    const char *base;
    const char *p;
    size_t o = 0, len;
    int has_zip = 0;

    if (out == NULL || n < 6) return 0;
    out[0] = '\0';
    if (filename == NULL) return 0;

    /* Whatever the browser called it, only the last component is a name. */
    base = filename;
    for (p = filename; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':') base = p + 1;

    len = strlen(base);
    if (len > 4) {
        const char *ext = base + len - 4;
        has_zip = (ext[0] == '.' &&
                   (ext[1] == 'z' || ext[1] == 'Z') &&
                   (ext[2] == 'i' || ext[2] == 'I') &&
                   (ext[3] == 'p' || ext[3] == 'P'));
    }
    if (!has_zip) return 0;   /* the drop folder holds archives, nothing else */
    len -= 4;

    for (p = base; (size_t)(p - base) < len && o + 5 < n; p++) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ' ||
            c == '(' || c == ')' || c == '[' || c == ']')
            out[o++] = c;
        else if (o > 0 && out[o - 1] != '_')
            out[o++] = '_';
    }
    while (o > 0 && (out[o - 1] == '_' || out[o - 1] == ' ' || out[o - 1] == '.'))
        o--;
    /* A name written entirely in an alphabet that cannot survive the fold
     * -- which is most Japanese releases -- still names an archive, and
     * refusing the upload over it would be absurd.  It arrives under a
     * plain name instead; the installer names the game folder from the
     * archive's contents anyway. */
    if (o == 0) {
        const char *fallback = "upload";
        if (n < strlen(fallback) + 5) return 0;
        memcpy(out, fallback, strlen(fallback));
        o = strlen(fallback);
    }

    memcpy(out + o, ".zip", 5);
    return 1;
}

/* --- multipart ------------------------------------------------------- */

enum {
    MP_PREAMBLE = 0,   /* before the first boundary */
    MP_HEADERS,        /* a part's headers */
    MP_BODY,           /* a part's content */
    MP_DONE            /* the closing boundary has been seen */
};

struct http_multipart {
    char   marker[HTTP_MAX_BOUNDARY + 4];   /* "--" + boundary */
    size_t marker_len;

    int    state;
    int    complete;
    int    skip_part;      /* a part the caller refused, parsed but discarded */

    char   buf[HTTP_FEED_MAX + HTTP_MAX_BOUNDARY + 16];
    size_t len;

    http_part_cb on_part;
    http_data_cb on_data;
    void        *user;
};

http_multipart *http_multipart_begin(const char *boundary,
                                     http_part_cb on_part,
                                     http_data_cb on_data,
                                     void *user) {
    http_multipart *m;

    if (boundary == NULL || boundary[0] == '\0') return NULL;
    if (strlen(boundary) + 2 >= sizeof(m->marker)) return NULL;

    m = (http_multipart *)calloc(1, sizeof(http_multipart));
    if (m == NULL) return NULL;

    m->marker[0] = m->marker[1] = '-';
    strcpy(m->marker + 2, boundary);
    m->marker_len = strlen(m->marker);

    m->state   = MP_PREAMBLE;
    m->on_part = on_part;
    m->on_data = on_data;
    m->user    = user;
    return m;
}

void http_multipart_end(http_multipart *m) {
    free(m);
}

int http_multipart_complete(const http_multipart *m) {
    return m ? m->complete : 0;
}

/* Drop the first count bytes of the working buffer. */
static void consume(http_multipart *m, size_t count) {
    if (count >= m->len) { m->len = 0; return; }
    memmove(m->buf, m->buf + count, m->len - count);
    m->len -= count;
}

/* The name in a part's headers, or an empty string. */
static void part_filename(const char *headers, size_t len, char *out, size_t n) {
    const char *mark = (const char *)find(headers, len, "filename=\"", 10);
    size_t o = 0;

    out[0] = '\0';
    if (mark == NULL) return;

    mark += 10;
    while (mark < headers + len && *mark != '"' && o + 1 < n)
        out[o++] = *mark++;
    out[o] = '\0';
}

/* What follows a boundary: 1 another part, 2 the end, 0 not enough bytes
 * yet, -1 malformed. */
static int after_boundary(const char *p, size_t left) {
    if (left < 2) return 0;
    if (p[0] == '-' && p[1] == '-') return 2;
    if (p[0] == '\r' && p[1] == '\n') return 1;
    /* Some clients pad a boundary with spaces before the newline. */
    if (p[0] == ' ' || p[0] == '\t') return 0;
    return -1;
}

int http_multipart_feed(http_multipart *m, const void *data, size_t len) {
    if (m == NULL) return -1;
    if (len > HTTP_FEED_MAX) return -1;
    if (m->state == MP_DONE) return 0;
    if (m->len + len > sizeof(m->buf)) return -1;

    memcpy(m->buf + m->len, data, len);
    m->len += len;

    while (1) {
        if (m->state == MP_PREAMBLE) {
            const char *at = (const char *)find(m->buf, m->len,
                                                m->marker, m->marker_len);
            size_t offset;
            int what;

            if (at == NULL) {
                /* Keep only what could be the start of the boundary. */
                if (m->len > m->marker_len + 2)
                    consume(m, m->len - (m->marker_len + 2));
                return 0;
            }
            offset = (size_t)(at - m->buf) + m->marker_len;
            what = after_boundary(m->buf + offset, m->len - offset);
            if (what == 0) { consume(m, (size_t)(at - m->buf)); return 0; }
            if (what < 0)  return -1;
            if (what == 2) { m->state = MP_DONE; m->complete = 1; return 0; }

            consume(m, offset + 2);
            m->state = MP_HEADERS;
            continue;
        }

        if (m->state == MP_HEADERS) {
            const char *end = (const char *)find(m->buf, m->len, "\r\n\r\n", 4);
            char filename[HTTP_MAX_PATH];
            size_t head_len;

            if (end == NULL) {
                /* A part's headers are small; anything else is not a part. */
                if (m->len > HTTP_FEED_MAX) return -1;
                return 0;
            }
            head_len = (size_t)(end - m->buf) + 4;

            part_filename(m->buf, head_len, filename, sizeof(filename));
            m->skip_part = 0;
            if (filename[0] == '\0') {
                /* A form field rather than a file: parsed past, not kept. */
                m->skip_part = 1;
            }
            else if (m->on_part && m->on_part(m->user, filename) != 0) {
                return -1;
            }

            consume(m, head_len);
            m->state = MP_BODY;
            continue;
        }

        if (m->state == MP_BODY) {
            /* The delimiter that ends a part includes the CRLF before it,
             * which therefore belongs to the delimiter and not to the file. */
            char delimiter[HTTP_MAX_BOUNDARY + 8];
            size_t dlen = m->marker_len + 2;
            const char *at;

            delimiter[0] = '\r';
            delimiter[1] = '\n';
            memcpy(delimiter + 2, m->marker, m->marker_len);

            at = (const char *)find(m->buf, m->len, delimiter, dlen);
            if (at == NULL) {
                /* Hand over everything that cannot be the start of the
                 * delimiter, and hold the rest back for the next feed. */
                size_t keep = dlen + 2;
                if (m->len > keep) {
                    size_t give = m->len - keep;
                    if (!m->skip_part && m->on_data &&
                        m->on_data(m->user, m->buf, give) != 0)
                        return -1;
                    consume(m, give);
                }
                return 0;
            }
            else {
                size_t give   = (size_t)(at - m->buf);
                size_t offset = give + dlen;
                int what = after_boundary(m->buf + offset, m->len - offset);

                if (what == 0) {
                    /* The delimiter is there but what follows it is not yet:
                     * emit what is certainly file and wait. */
                    if (give > 0) {
                        if (!m->skip_part && m->on_data &&
                            m->on_data(m->user, m->buf, give) != 0)
                            return -1;
                        consume(m, give);
                    }
                    return 0;
                }
                if (what < 0) return -1;

                if (give > 0 && !m->skip_part && m->on_data &&
                    m->on_data(m->user, m->buf, give) != 0)
                    return -1;

                if (what == 2) {
                    m->state = MP_DONE;
                    m->complete = 1;
                    return 0;
                }
                consume(m, offset + 2);
                m->state = MP_HEADERS;
                continue;
            }
        }

        return 0;
    }
}
