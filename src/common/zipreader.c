/* -*- C -*-
 *
 *  zipreader.c -- minimal read-only ZIP archive reader
 *
 *  See zipreader.h for the interface and its limits.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "zipreader.h"

#define SIG_EOCD    0x06054b50
#define SIG_CENTRAL 0x02014b50
#define SIG_LOCAL   0x04034b50
#define SIG_ZIP64_EOCD_LOCATOR 0x07064b50

#define EOCD_SIZE      22
#define CENTRAL_SIZE   46
#define LOCAL_SIZE     30
#define MAX_COMMENT    65535

/* Deflate wants a decent window; these keep the Vita's heap use predictable. */
#define IN_CHUNK  (32 * 1024)
#define OUT_CHUNK (64 * 1024)

typedef struct {
    char    *name;
    uint32_t compressed_size;
    uint32_t size;
    uint32_t local_offset;
    uint32_t crc;
    uint16_t method;
    uint16_t flags;
    int      is_dir;
} zip_entry;

struct zip_reader {
    FILE      *fp;
    zip_entry *entries;
    int        count;
    uint64_t   total_size;
    /* The two buffers extraction works through, allocated once with the
     * reader rather than per entry.  A game is thousands of small files, so
     * per-entry buffers meant thousands of allocation pairs of 32K and 64K
     * -- churn that costs nothing to avoid, and that on a console with no
     * memory to spare is also the thing most likely to fail part way
     * through an install.  Holding them also keeps the stored path off the
     * stack, where a 32K frame is a lot to ask of a thread. */
    unsigned char *in_buf;
    unsigned char *out_buf;
    /* Entries the directory declared and this reader could not name, so a
     * caller can say so rather than silently install fewer files. */
    int skipped_names;
};

static uint16_t rd16(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

const char *zip_error_string(int err) {
    switch (err) {
    case ZIP_OK:             return "ok";
    case ZIP_ERR_IO:         return "cannot read the archive";
    case ZIP_ERR_FORMAT:     return "not a valid zip archive";
    case ZIP_ERR_ZIP64:      return "zip64 archives are not supported";
    case ZIP_ERR_METHOD:     return "unsupported compression method";
    case ZIP_ERR_ENCRYPTED:  return "the archive is password protected";
    case ZIP_ERR_MEMORY:     return "out of memory";
    case ZIP_ERR_DATA:       return "the archive is corrupt";
    case ZIP_ERR_CANCELED:   return "canceled";
    case ZIP_ERR_BADNAME:    return "the archive contains an unsafe file name";
    default:                 return "unknown error";
    }
}

/* Locate the end-of-central-directory record by scanning backwards over the
 * largest comment a zip may carry. */
static long find_eocd(FILE *fp, unsigned char *eocd) {
    long file_size, search_start, span, i;
    unsigned char *buf;
    long found = -1;

    if (fseek(fp, 0, SEEK_END) != 0) return -1;
    file_size = ftell(fp);
    if (file_size < EOCD_SIZE) return -1;

    span = EOCD_SIZE + MAX_COMMENT;
    if (span > file_size) span = file_size;
    search_start = file_size - span;

    buf = (unsigned char *)malloc((size_t)span);
    if (!buf) return -1;
    if (fseek(fp, search_start, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)span, fp) != (size_t)span) {
        free(buf);
        return -1;
    }

    for (i = span - EOCD_SIZE; i >= 0; i--) {
        if (rd32(buf + i) == SIG_EOCD) {
            memcpy(eocd, buf + i, EOCD_SIZE);
            found = search_start + i;
            break;
        }
    }
    free(buf);
    return found;
}

static int is_zip64(FILE *fp, long eocd_pos, const unsigned char *eocd) {
    unsigned char sig[4];

    if (rd16(eocd + 8) == 0xFFFF || rd16(eocd + 10) == 0xFFFF ||
        rd32(eocd + 12) == 0xFFFFFFFFu || rd32(eocd + 16) == 0xFFFFFFFFu)
        return 1;

    /* A zip64 locator sits immediately before the EOCD. */
    if (eocd_pos >= 20) {
        if (fseek(fp, eocd_pos - 20, SEEK_SET) == 0 &&
            fread(sig, 1, 4, fp) == 4 && rd32(sig) == SIG_ZIP64_EOCD_LOCATOR)
            return 1;
    }
    return 0;
}

static void free_entries(zip_entry *entries, int count) {
    int i;
    for (i = 0; i < count; i++) free(entries[i].name);
    free(entries);
}

static int read_central_directory(zip_reader *z, uint32_t cd_offset,
                                  uint32_t cd_size, int count) {
    unsigned char *cd;
    unsigned char *p, *end;
    int i, kept;

    cd = (unsigned char *)malloc(cd_size);
    if (!cd) return ZIP_ERR_MEMORY;
    if (fseek(z->fp, (long)cd_offset, SEEK_SET) != 0 ||
        fread(cd, 1, cd_size, z->fp) != cd_size) {
        free(cd);
        return ZIP_ERR_IO;
    }

    z->entries = (zip_entry *)calloc((size_t)count, sizeof(zip_entry));
    if (!z->entries) {
        free(cd);
        return ZIP_ERR_MEMORY;
    }

    p = cd;
    end = cd + cd_size;
    /* kept counts what is stored, which is not always what the directory
     * declares: an entry can be skipped without losing the rest. */
    kept = 0;
    for (i = 0; i < count; i++) {
        uint16_t name_len, extra_len, comment_len;
        zip_entry *e = &z->entries[kept];
        size_t len;

        if (p + CENTRAL_SIZE > end || rd32(p) != SIG_CENTRAL) {
            free_entries(z->entries, kept);
            z->entries = NULL;
            free(cd);
            return ZIP_ERR_FORMAT;
        }

        e->flags           = rd16(p + 8);
        e->method          = rd16(p + 10);
        e->crc             = rd32(p + 16);
        e->compressed_size = rd32(p + 20);
        e->size            = rd32(p + 24);
        name_len           = rd16(p + 28);
        extra_len          = rd16(p + 30);
        comment_len        = rd16(p + 32);
        e->local_offset    = rd32(p + 42);

        /* A name that runs past the end of the directory is a corrupt
         * archive and there is nothing sensible to do with the rest of it. */
        if (p + CENTRAL_SIZE + name_len > end) {
            free_entries(z->entries, kept);
            z->entries = NULL;
            free(cd);
            return ZIP_ERR_FORMAT;
        }

        /* A name longer than the reader will hold is one unusable entry,
         * not an unusable archive: it is skipped and the rest is read.
         * Refusing the whole archive meant one deep path -- a translation
         * patch nested a few folders down, say -- made a game impossible to
         * install, and the file that could not be named is one the console
         * could not have opened by that path either. */
        if (name_len >= ZIP_MAX_NAME) {
            p += CENTRAL_SIZE + name_len + extra_len + comment_len;
            z->skipped_names++;
            continue;
        }

        /* 0xFFFFFFFF in a size or an offset means "the real value is in the
         * zip64 extra field", which this reader does not parse.  Taken at
         * face value it asks for four gigabytes from a file that has not
         * got them, so it is refused as what it is. */
        if (e->compressed_size == 0xFFFFFFFFu || e->size == 0xFFFFFFFFu ||
            e->local_offset == 0xFFFFFFFFu) {
            free_entries(z->entries, kept);
            z->entries = NULL;
            free(cd);
            return ZIP_ERR_ZIP64;
        }

        len = name_len;
        e->name = (char *)malloc(len + 1);
        if (!e->name) {
            free_entries(z->entries, kept);
            z->entries = NULL;
            free(cd);
            return ZIP_ERR_MEMORY;
        }
        memcpy(e->name, p + CENTRAL_SIZE, len);
        e->name[len] = '\0';

        /* Some writers use backslashes; normalise so the rest of the code
         * only ever sees '/'. */
        {
            char *c;
            for (c = e->name; *c; c++)
                if (*c == '\\') *c = '/';
        }

        e->is_dir = (len > 0 && e->name[len - 1] == '/') ||
                    (e->size == 0 && e->compressed_size == 0 && len > 0 &&
                     e->name[len - 1] == '/');
        if (!e->is_dir) z->total_size += e->size;

        kept++;
        p += CENTRAL_SIZE + name_len + extra_len + comment_len;
    }

    z->count = kept;
    free(cd);
    return ZIP_OK;
}

zip_reader *zip_open(const char *path, int *err) {
    unsigned char eocd[EOCD_SIZE];
    zip_reader *z;
    long eocd_pos;
    uint32_t cd_offset, cd_size;
    int count, rc;

    if (err) *err = ZIP_OK;
    if (!path) {
        if (err) *err = ZIP_ERR_IO;
        return NULL;
    }

    z = (zip_reader *)calloc(1, sizeof(zip_reader));
    if (!z) {
        if (err) *err = ZIP_ERR_MEMORY;
        return NULL;
    }

    z->fp = fopen(path, "rb");
    if (!z->fp) {
        free(z);
        if (err) *err = ZIP_ERR_IO;
        return NULL;
    }

    z->in_buf  = (unsigned char *)malloc(IN_CHUNK);
    z->out_buf = (unsigned char *)malloc(OUT_CHUNK);
    if (!z->in_buf || !z->out_buf) {
        zip_close(z);
        if (err) *err = ZIP_ERR_MEMORY;
        return NULL;
    }

    eocd_pos = find_eocd(z->fp, eocd);
    if (eocd_pos < 0) {
        zip_close(z);
        if (err) *err = ZIP_ERR_FORMAT;
        return NULL;
    }
    if (is_zip64(z->fp, eocd_pos, eocd)) {
        zip_close(z);
        if (err) *err = ZIP_ERR_ZIP64;
        return NULL;
    }

    count     = rd16(eocd + 10);
    cd_size   = rd32(eocd + 12);
    cd_offset = rd32(eocd + 16);

    if (count == 0) {
        /* An empty archive is well formed but has nothing to install. */
        if (err) *err = ZIP_OK;
        return z;
    }
    if (cd_size == 0 || (long)cd_offset >= eocd_pos) {
        zip_close(z);
        if (err) *err = ZIP_ERR_FORMAT;
        return NULL;
    }

    rc = read_central_directory(z, cd_offset, cd_size, count);
    if (rc != ZIP_OK) {
        zip_close(z);
        if (err) *err = rc;
        return NULL;
    }
    return z;
}

void zip_close(zip_reader *z) {
    if (!z) return;
    free_entries(z->entries, z->count);
    if (z->fp) fclose(z->fp);
    free(z->in_buf);
    free(z->out_buf);
    free(z);
}

int zip_skipped_names(const zip_reader *z) {
    return z ? z->skipped_names : 0;
}

int zip_count(const zip_reader *z) {
    return z ? z->count : 0;
}

static const zip_entry *entry_at(const zip_reader *z, int i) {
    if (!z || i < 0 || i >= z->count) return NULL;
    return &z->entries[i];
}

const char *zip_entry_name(const zip_reader *z, int i) {
    const zip_entry *e = entry_at(z, i);
    return e ? e->name : NULL;
}

uint32_t zip_entry_size(const zip_reader *z, int i) {
    const zip_entry *e = entry_at(z, i);
    return e ? e->size : 0;
}

uint32_t zip_entry_compressed_size(const zip_reader *z, int i) {
    const zip_entry *e = entry_at(z, i);
    return e ? e->compressed_size : 0;
}

int zip_entry_is_dir(const zip_reader *z, int i) {
    const zip_entry *e = entry_at(z, i);
    return e ? e->is_dir : 0;
}

uint64_t zip_total_size(const zip_reader *z) {
    return z ? z->total_size : 0;
}

/* Seek past the local file header so the stream sits on the entry's data. */
static int seek_to_data(zip_reader *z, const zip_entry *e) {
    unsigned char hdr[LOCAL_SIZE];
    uint16_t name_len, extra_len;

    if (fseek(z->fp, (long)e->local_offset, SEEK_SET) != 0) return ZIP_ERR_IO;
    if (fread(hdr, 1, LOCAL_SIZE, z->fp) != LOCAL_SIZE) return ZIP_ERR_IO;
    if (rd32(hdr) != SIG_LOCAL) return ZIP_ERR_FORMAT;

    name_len  = rd16(hdr + 26);
    extra_len = rd16(hdr + 28);
    if (fseek(z->fp, (long)(name_len + extra_len), SEEK_CUR) != 0)
        return ZIP_ERR_IO;
    return ZIP_OK;
}

static int extract_stored(zip_reader *z, const zip_entry *e,
                          zip_write_cb cb, void *user, uLong *crc) {
    unsigned char *buf = z->in_buf;
    uint32_t left = e->compressed_size;

    while (left > 0) {
        size_t want = left < IN_CHUNK ? left : IN_CHUNK;
        size_t got  = fread(buf, 1, want, z->fp);
        if (got != want) return ZIP_ERR_IO;
        *crc = crc32(*crc, buf, (uInt)got);
        if (cb && cb(user, buf, got) != 0) return ZIP_ERR_CANCELED;
        left -= (uint32_t)got;
    }
    return ZIP_OK;
}

static int extract_deflated(zip_reader *z, const zip_entry *e,
                            zip_write_cb cb, void *user, uLong *crc) {
    z_stream strm;
    unsigned char *in  = z->in_buf;
    unsigned char *out = z->out_buf;
    uint32_t left = e->compressed_size;
    int rc = ZIP_OK;
    int zret;

    if (!in || !out) return ZIP_ERR_MEMORY;

    memset(&strm, 0, sizeof(strm));
    /* Negative window bits: raw deflate, no zlib header. */
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return ZIP_ERR_MEMORY;

    do {
        size_t want, got;

        if (strm.avail_in == 0) {
            if (left == 0) { rc = ZIP_ERR_DATA; break; }
            want = left < IN_CHUNK ? left : IN_CHUNK;
            got  = fread(in, 1, want, z->fp);
            if (got != want) { rc = ZIP_ERR_IO; break; }
            left -= (uint32_t)got;
            strm.next_in  = in;
            strm.avail_in = (uInt)got;
        }

        strm.next_out  = out;
        strm.avail_out = OUT_CHUNK;
        zret = inflate(&strm, Z_NO_FLUSH);
        if (zret != Z_OK && zret != Z_STREAM_END && zret != Z_BUF_ERROR) {
            rc = ZIP_ERR_DATA;
            break;
        }

        {
            size_t produced = OUT_CHUNK - strm.avail_out;
            if (produced > 0) {
                *crc = crc32(*crc, out, (uInt)produced);
                if (cb && cb(user, out, produced) != 0) {
                    rc = ZIP_ERR_CANCELED;
                    break;
                }
            } else if (zret == Z_BUF_ERROR && strm.avail_in == 0 && left == 0) {
                rc = ZIP_ERR_DATA;
                break;
            }
        }

        if (zret == Z_STREAM_END) break;
    } while (1);

    inflateEnd(&strm);
    return rc;
}

int zip_extract_entry(zip_reader *z, int i, zip_write_cb cb, void *user) {
    const zip_entry *e = entry_at(z, i);
    uLong crc = crc32(0L, Z_NULL, 0);
    int rc;

    if (!e) return ZIP_ERR_FORMAT;
    if (e->is_dir) return ZIP_OK;
    if (e->flags & 0x1) return ZIP_ERR_ENCRYPTED;
    if (e->method != 0 && e->method != 8) return ZIP_ERR_METHOD;

    rc = seek_to_data(z, e);
    if (rc != ZIP_OK) return rc;

    rc = (e->method == 0) ? extract_stored(z, e, cb, user, &crc)
                          : extract_deflated(z, e, cb, user, &crc);
    if (rc != ZIP_OK) return rc;

    /* Entries written with a streaming data descriptor carry a zero CRC in
     * the central directory; only check when we have something to check. */
    if (e->crc != 0 && (uint32_t)crc != e->crc) return ZIP_ERR_DATA;
    return ZIP_OK;
}

int zip_sanitize_name(const char *name, char *out, size_t n) {
    size_t len = 0;
    const char *p = name;
    int trailing_slash;

    if (!name || !out || n == 0) return ZIP_ERR_BADNAME;
    out[0] = '\0';

    if (*p == '/' || *p == '\\') return ZIP_ERR_BADNAME;         /* absolute */
    /* A colon before the first separator means a drive prefix: "c:\" on
     * Windows, "ux0:" on the Vita.  Neither may steer where we write. */
    {
        const char *q;
        for (q = name; *q && *q != '/' && *q != '\\'; q++)
            if (*q == ':') return ZIP_ERR_BADNAME;
    }

    trailing_slash = 0;
    while (*p) {
        const char *seg = p;
        size_t seg_len;

        while (*p && *p != '/' && *p != '\\') p++;
        seg_len = (size_t)(p - seg);

        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') return ZIP_ERR_BADNAME;

        if (seg_len > 0 && !(seg_len == 1 && seg[0] == '.')) {
            if (len + seg_len + (len ? 1 : 0) + 1 > n) return ZIP_ERR_BADNAME;
            if (len) out[len++] = '/';
            memcpy(out + len, seg, seg_len);
            len += seg_len;
        }

        if (*p) {
            trailing_slash = 1;
            p++;
        } else {
            trailing_slash = 0;
        }
    }

    if (len == 0) return ZIP_ERR_BADNAME;
    if (trailing_slash) {
        if (len + 2 > n) return ZIP_ERR_BADNAME;
        out[len++] = '/';
    }
    out[len] = '\0';
    return ZIP_OK;
}

int zip_common_root(const zip_reader *z, char *out, size_t n) {
    char root[ZIP_MAX_NAME];
    size_t root_len = 0;
    int i, have_root = 0;

    if (!out || n == 0) return 0;
    out[0] = '\0';
    if (!z || z->count == 0) return 0;

    for (i = 0; i < z->count; i++) {
        char clean[ZIP_MAX_NAME];
        const char *slash;
        size_t seg_len;

        if (zip_sanitize_name(z->entries[i].name, clean, sizeof(clean)) != ZIP_OK)
            continue;

        slash = strchr(clean, '/');
        if (!slash) return 0;   /* a file sits at the archive root */

        seg_len = (size_t)(slash - clean);
        if (!have_root) {
            if (seg_len + 1 > sizeof(root)) return 0;
            memcpy(root, clean, seg_len);
            root[seg_len] = '\0';
            root_len = seg_len;
            have_root = 1;
        } else if (seg_len != root_len || memcmp(root, clean, seg_len) != 0) {
            return 0;
        }
    }

    if (!have_root || root_len + 1 > n) return 0;
    memcpy(out, root, root_len + 1);
    return 1;
}

int zip_is_script_name(const char *base) {
    static const char *names[] = {
        "0.txt", "00.txt", "nscr_sec.dat", "nscript.___",
        "nscript.dat", "onscript.nt2", "onscript.nt3", NULL
    };
    int i;

    if (!base) return 0;
    for (i = 0; names[i]; i++) {
#ifdef _WIN32
        if (_stricmp(base, names[i]) == 0) return 1;
#else
        {
            const char *a = base, *b = names[i];
            while (*a && *b) {
                char ca = *a, cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
                if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
                if (ca != cb) break;
                a++; b++;
            }
            if (*a == '\0' && *b == '\0') return 1;
        }
#endif
    }
    return 0;
}

int zip_find_game_root(const zip_reader *z, char *out, size_t n) {
    int i;
    int best = -1;
    size_t best_depth = 0;
    char best_dir[ZIP_MAX_NAME];

    if (!out || n == 0) return 0;
    out[0] = '\0';
    if (!z) return 0;

    best_dir[0] = '\0';

    for (i = 0; i < z->count; i++) {
        char clean[ZIP_MAX_NAME];
        const char *slash, *base;
        size_t dir_len, depth = 0;
        const char *c;

        if (z->entries[i].is_dir) continue;
        if (zip_sanitize_name(z->entries[i].name, clean, sizeof(clean)) != ZIP_OK)
            continue;

        slash = strrchr(clean, '/');
        base  = slash ? slash + 1 : clean;
        if (!zip_is_script_name(base)) continue;

        dir_len = slash ? (size_t)(slash - clean) : 0;
        for (c = clean; c < clean + dir_len; c++)
            if (*c == '/') depth++;

        /* Prefer the shallowest script: a nested copy in a "backup" or
         * "patch" folder should not win over the real game root. */
        if (best < 0 || depth < best_depth) {
            if (dir_len + 1 > sizeof(best_dir)) continue;
            memcpy(best_dir, clean, dir_len);
            best_dir[dir_len] = '\0';
            best_depth = depth;
            best = i;
        }
    }

    if (best < 0) return 0;
    if (strlen(best_dir) + 1 > n) return 0;
    strcpy(out, best_dir);
    return 1;
}
