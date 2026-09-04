/* -*- C -*-
 *
 *  sevenzip.c -- see sevenzip.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sevenzip.h"

#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"

/* Entry names come out of the archive as UTF-16; everything above this
 * deals in bytes, and the console opens paths as bytes. */
struct entry {
    char    *name;
    uint64_t size;
    int      is_dir;
};

struct sevenzip_reader {
    CFileInStream  stream;
    CLookToRead2   look;
    CSzArEx        db;
    int            opened;

    struct entry  *entries;
    int            count;
    int            skipped;
    uint64_t       total;

    /* The SDK's solid-block cache, kept for the life of the reader: an
     * archive extracted in order then costs one decode per block rather
     * than one per file. */
    UInt32  block_index;
    Byte   *block;
    size_t  block_size;
};

static ISzAlloc g_alloc = { SzAlloc, SzFree };

/* UTF-16 to UTF-8.  Only what a file name can hold: anything outside the
 * basic plane is written as a surrogate pair and encoded as one code
 * point, and an unpaired surrogate is dropped rather than guessed at. */
static int utf16_to_utf8(const UInt16 *in, char *out, size_t n) {
    size_t o = 0;

    while (*in) {
        unsigned long c = *in++;

        if (c >= 0xD800 && c <= 0xDBFF) {
            const unsigned long low = *in;
            if (low >= 0xDC00 && low <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                in++;
            }
            else {
                continue;   /* half a character is not a character */
            }
        }
        else if (c >= 0xDC00 && c <= 0xDFFF) {
            continue;
        }

        if (c < 0x80) {
            if (o + 1 >= n) return 0;
            out[o++] = (char)c;
        }
        else if (c < 0x800) {
            if (o + 2 >= n) return 0;
            out[o++] = (char)(0xC0 | (c >> 6));
            out[o++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000) {
            if (o + 3 >= n) return 0;
            out[o++] = (char)(0xE0 | (c >> 12));
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (c & 0x3F));
        }
        else {
            if (o + 4 >= n) return 0;
            out[o++] = (char)(0xF0 | (c >> 18));
            out[o++] = (char)(0x80 | ((c >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (c & 0x3F));
        }
    }

    out[o] = '\0';
    return 1;
}

int sevenzip_is_sevenzip(const char *path) {
    static const unsigned char signature[6] =
        { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
    unsigned char head[6];
    size_t got;
    FILE *fp;

    if (path == NULL) return 0;
    fp = fopen(path, "rb");
    if (fp == NULL) return 0;

    got = fread(head, 1, sizeof(head), fp);
    fclose(fp);

    return got == sizeof(head) && memcmp(head, signature, sizeof(head)) == 0;
}

static void free_entries(sevenzip_reader *z) {
    int i;
    if (z->entries == NULL) return;
    for (i = 0; i < z->count; i++) free(z->entries[i].name);
    free(z->entries);
    z->entries = NULL;
}

/* Read every entry's name and size once, so the questions asked of an
 * archive before anything is extracted -- where the game root is, what it
 * would install as, how big it is -- cost no decoding. */
static int read_directory(sevenzip_reader *z) {
    UInt16 *utf16 = NULL;
    size_t utf16_len = 0;
    UInt32 i;

    z->count = (int)z->db.NumFiles;
    if (z->count <= 0) {
        z->count = 0;
        return 1;
    }

    z->entries = (struct entry *)calloc((size_t)z->count, sizeof(struct entry));
    if (z->entries == NULL) return 0;

    for (i = 0; i < z->db.NumFiles; i++) {
        char name[ZIP_MAX_NAME];
        const size_t needed = SzArEx_GetFileNameUtf16(&z->db, i, NULL);

        if (needed > utf16_len) {
            UInt16 *grown = (UInt16 *)realloc(utf16, needed * sizeof(UInt16));
            if (grown == NULL) { free(utf16); return 0; }
            utf16 = grown;
            utf16_len = needed;
        }
        SzArEx_GetFileNameUtf16(&z->db, i, utf16);

        z->entries[i].is_dir = SzArEx_IsDir(&z->db, i) ? 1 : 0;
        z->entries[i].size   = z->entries[i].is_dir
            ? 0 : (uint64_t)SzArEx_GetFileSize(&z->db, i);

        if (!utf16_to_utf8(utf16, name, sizeof(name))) {
            /* Longer than anything this can hold: counted, and left
             * without a name so nothing tries to write it. */
            z->skipped++;
            continue;
        }

        /* 7z holds names with the separator the archive was made with;
         * everything above here expects '/'. */
        {
            size_t k;
            for (k = 0; name[k]; k++)
                if (name[k] == '\\') name[k] = '/';
        }

        z->entries[i].name = (char *)malloc(strlen(name) + 1);
        if (z->entries[i].name == NULL) { free(utf16); return 0; }
        strcpy(z->entries[i].name, name);

        if (!z->entries[i].is_dir) z->total += z->entries[i].size;
    }

    free(utf16);
    return 1;
}

sevenzip_reader *sevenzip_open(const char *path, int *err) {
    sevenzip_reader *z;
    SRes res;

    if (err) *err = ZIP_ERR_IO;
    if (path == NULL) return NULL;

    z = (sevenzip_reader *)calloc(1, sizeof(sevenzip_reader));
    if (z == NULL) {
        if (err) *err = ZIP_ERR_MEMORY;
        return NULL;
    }

    if (InFile_Open(&z->stream.file, path) != 0) {
        free(z);
        if (err) *err = ZIP_ERR_IO;
        return NULL;
    }

    FileInStream_CreateVTable(&z->stream);
    LookToRead2_CreateVTable(&z->look, False);
    z->look.buf = (Byte *)ISzAlloc_Alloc(&g_alloc, 1 << 16);
    if (z->look.buf == NULL) {
        File_Close(&z->stream.file);
        free(z);
        if (err) *err = ZIP_ERR_MEMORY;
        return NULL;
    }
    z->look.bufSize    = 1 << 16;
    z->look.realStream = &z->stream.vt;
    LookToRead2_Init(&z->look);

    /* The table the SDK checks entries against; building it twice is
     * harmless and cheap, and doing it here means no caller has to know
     * this library exists. */
    CrcGenerateTable();

    SzArEx_Init(&z->db);
    res = SzArEx_Open(&z->db, &z->look.vt, &g_alloc, &g_alloc);
    if (res != SZ_OK) {
        sevenzip_close(z);
        if (err) {
            switch (res) {
            case SZ_ERROR_NO_ARCHIVE:
            case SZ_ERROR_ARCHIVE:    *err = ZIP_ERR_FORMAT;    break;
            case SZ_ERROR_UNSUPPORTED:*err = ZIP_ERR_METHOD;    break;
            case SZ_ERROR_MEM:        *err = ZIP_ERR_MEMORY;    break;
            case SZ_ERROR_CRC:        *err = ZIP_ERR_DATA;      break;
            /* A 7z whose header is encrypted cannot even be listed, which
             * is the case worth naming: the player set a password. */
            case SZ_ERROR_DATA:       *err = ZIP_ERR_ENCRYPTED; break;
            default:                  *err = ZIP_ERR_FORMAT;    break;
            }
        }
        return NULL;
    }
    z->opened = 1;

    if (!read_directory(z)) {
        sevenzip_close(z);
        if (err) *err = ZIP_ERR_MEMORY;
        return NULL;
    }

    if (err) *err = ZIP_OK;
    return z;
}

void sevenzip_close(sevenzip_reader *z) {
    if (z == NULL) return;

    free_entries(z);
    if (z->block) ISzAlloc_Free(&g_alloc, z->block);
    if (z->opened) SzArEx_Free(&z->db, &g_alloc);
    if (z->look.buf) ISzAlloc_Free(&g_alloc, z->look.buf);
    File_Close(&z->stream.file);
    free(z);
}

int sevenzip_count(const sevenzip_reader *z) {
    return z ? z->count : 0;
}

static const struct entry *at(const sevenzip_reader *z, int i) {
    if (z == NULL || i < 0 || i >= z->count) return NULL;
    return &z->entries[i];
}

const char *sevenzip_entry_name(const sevenzip_reader *z, int i) {
    const struct entry *e = at(z, i);
    return (e && e->name) ? e->name : "";
}

uint64_t sevenzip_entry_size(const sevenzip_reader *z, int i) {
    const struct entry *e = at(z, i);
    return e ? e->size : 0;
}

int sevenzip_entry_is_dir(const sevenzip_reader *z, int i) {
    const struct entry *e = at(z, i);
    return e ? e->is_dir : 0;
}

uint64_t sevenzip_total_size(const sevenzip_reader *z) {
    return z ? z->total : 0;
}

int sevenzip_skipped_names(const sevenzip_reader *z) {
    return z ? z->skipped : 0;
}

int sevenzip_extract_entry(sevenzip_reader *z, int i,
                           zip_write_cb cb, void *user) {
    const struct entry *e = at(z, i);
    size_t offset = 0, written = 0;
    SRes res;

    if (e == NULL) return ZIP_ERR_FORMAT;
    if (e->is_dir) return ZIP_OK;

    /* 7z decodes a whole solid block at a time; the block is kept, so the
     * next entry in the same block costs nothing but the copy. */
    res = SzArEx_Extract(&z->db, &z->look.vt, (UInt32)i,
                         &z->block_index, &z->block, &z->block_size,
                         &offset, &written, &g_alloc, &g_alloc);
    if (res != SZ_OK) {
        switch (res) {
        case SZ_ERROR_UNSUPPORTED: return ZIP_ERR_METHOD;
        case SZ_ERROR_MEM:         return ZIP_ERR_MEMORY;
        case SZ_ERROR_CRC:         return ZIP_ERR_DATA;
        case SZ_ERROR_DATA:        return ZIP_ERR_ENCRYPTED;
        default:                   return ZIP_ERR_FORMAT;
        }
    }

    /* Handed over in pieces the size the zip reader uses, so a caller that
     * draws a progress bar per chunk behaves the same for either kind of
     * archive. */
    {
        const size_t chunk = 64 * 1024;
        size_t at_byte = 0;

        while (at_byte < written) {
            size_t take = written - at_byte;
            if (take > chunk) take = chunk;

            if (cb && cb(user, z->block + offset + at_byte, take) != 0)
                return ZIP_ERR_CANCELED;
            at_byte += take;
        }
    }

    return ZIP_OK;
}
