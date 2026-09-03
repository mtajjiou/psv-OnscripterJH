/*
 *  manifest.c -- see manifest.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "manifest.h"

#define MANIFEST_MAX_FILE (256 * 1024)

void manifest_init(manifest *m)
{
    if (m == NULL) return;
    m->entries  = NULL;
    m->count    = 0;
    m->capacity = 0;
}

void manifest_free(manifest *m)
{
    if (m == NULL) return;
    free(m->entries);
    manifest_init(m);
}

static int manifest_grow(manifest *m)
{
    const int wanted = m->capacity ? m->capacity * 2 : 16;
    manifest_entry *bigger =
        (manifest_entry *)realloc(m->entries, (size_t)wanted * sizeof(*bigger));
    if (bigger == NULL) return 0;

    m->entries  = bigger;
    m->capacity = wanted;
    return 1;
}

const manifest_entry *manifest_find(const manifest *m, const char *folder,
                                    const char *stamp)
{
    int i;
    if (m == NULL || folder == NULL) return NULL;

    for (i = 0; i < m->count; i++){
        if (strcmp(m->entries[i].folder, folder) != 0) continue;
        /* A stamp that no longer matches means the folder changed under us,
         * which is exactly when the cached answer must not be believed. */
        if (stamp != NULL && strcmp(m->entries[i].stamp, stamp) != 0) return NULL;
        return &m->entries[i];
    }
    return NULL;
}

int manifest_put(manifest *m, const manifest_entry *entry)
{
    int i;
    if (m == NULL || entry == NULL) return 0;

    for (i = 0; i < m->count; i++){
        if (strcmp(m->entries[i].folder, entry->folder) == 0){
            m->entries[i] = *entry;
            return 1;
        }
    }

    if (m->count == m->capacity && !manifest_grow(m)) return 0;
    m->entries[m->count++] = *entry;
    return 1;
}

/* --- writing ---------------------------------------------------------- */

static void write_escaped(FILE *f, const char *text)
{
    size_t i;
    fputc('"', f);
    for (i = 0; text[i]; i++){
        const unsigned char c = (unsigned char)text[i];
        /* A game's name is whatever someone typed into caption.txt, so it
         * can contain the two characters that would end the string early,
         * and control bytes that would make the file unreadable. */
        if (c == '"' || c == '\\'){
            fputc('\\', f);
            fputc((int)c, f);
        }
        else if (c < 0x20){
            fprintf(f, "\\u%04x", c);
        }
        else {
            fputc((int)c, f);
        }
    }
    fputc('"', f);
}

int manifest_save(const manifest *m, const char *path)
{
    FILE *f;
    int i;

    if (m == NULL || path == NULL) return 0;

    f = fopen(path, "w");
    if (f == NULL) return 0;

    fputs("[\n", f);
    for (i = 0; i < m->count; i++){
        const manifest_entry *e = &m->entries[i];
        fputs("  {\"folder\": ", f);
        write_escaped(f, e->folder);
        fputs(", \"root\": ", f);
        write_escaped(f, e->root);
        fputs(", \"name\": ", f);
        write_escaped(f, e->name);
        fputs(", \"stamp\": ", f);
        write_escaped(f, e->stamp);
        fprintf(f, ", \"size\": %llu}%s\n",
                (unsigned long long)e->size,
                (i + 1 < m->count) ? "," : "");
    }
    fputs("]\n", f);

    fclose(f);
    return 1;
}

/* --- reading ---------------------------------------------------------- */

/* Copies a field, truncating rather than overrunning.  Fields are bounded
 * by the struct; a name longer than that is cut, which shows as a short
 * title and never as a corrupted heap. */
static void copy_field(char *dst, size_t size, const char *src)
{
    size_t length = strlen(src);
    if (length >= size) length = size - 1;
    memcpy(dst, src, length);
    dst[length] = '\0';
}

static const char *skip_space(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Reads a JSON string into out.  Returns where it stopped, or NULL. */
static const char *read_string(const char *p, char *out, size_t max)
{
    size_t o = 0;

    p = skip_space(p);
    if (*p != '"') return NULL;
    p++;

    while (*p != '"'){
        if (*p == '\0') return NULL;          /* truncated file */

        if (*p == '\\'){
            p++;
            switch (*p){
            case '"':  if (o + 1 < max) out[o++] = '"';  p++; break;
            case '\\': if (o + 1 < max) out[o++] = '\\'; p++; break;
            case 'n':  if (o + 1 < max) out[o++] = '\n'; p++; break;
            case 't':  if (o + 1 < max) out[o++] = '\t'; p++; break;
            case 'u': {
                /* Only the escapes this writer produces, which are the
                 * control bytes; anything else is not worth a decoder. */
                unsigned int value = 0;
                int digit;
                p++;
                for (digit = 0; digit < 4; digit++){
                    const char c = *p;
                    if (c >= '0' && c <= '9')      value = value * 16 + (unsigned)(c - '0');
                    else if (c >= 'a' && c <= 'f') value = value * 16 + (unsigned)(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') value = value * 16 + (unsigned)(c - 'A' + 10);
                    else return NULL;
                    p++;
                }
                if (value < 0x80 && o + 1 < max) out[o++] = (char)value;
                break;
            }
            default:
                return NULL;
            }
            continue;
        }

        if (o + 1 < max) out[o++] = *p;
        p++;
    }

    out[o] = '\0';
    return p + 1;
}

static const char *read_key(const char *p, char *out, size_t max)
{
    p = read_string(p, out, max);
    if (p == NULL) return NULL;
    p = skip_space(p);
    if (*p != ':') return NULL;
    return p + 1;
}

int manifest_load(manifest *m, const char *path)
{
    FILE *f;
    long length;
    char *text;
    const char *p;

    if (m == NULL || path == NULL) return 0;
    manifest_init(m);

    f = fopen(path, "rb");
    if (f == NULL) return 0;

    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (length <= 0 || length > MANIFEST_MAX_FILE){
        fclose(f);
        return 0;
    }

    text = (char *)malloc((size_t)length + 1);
    if (text == NULL){
        fclose(f);
        return 0;
    }
    if (fread(text, 1, (size_t)length, f) != (size_t)length){
        free(text);
        fclose(f);
        return 0;
    }
    text[length] = '\0';
    fclose(f);

    p = skip_space(text);
    if (*p != '['){
        free(text);
        return 0;
    }
    p++;

    while (1){
        manifest_entry entry;
        p = skip_space(p);
        if (*p == ']' || *p == '\0') break;
        if (*p == ','){ p++; continue; }
        if (*p != '{') break;
        p++;

        memset(&entry, 0, sizeof(entry));

        while (1){
            char key[32];
            p = skip_space(p);
            if (*p == '}'){ p++; break; }
            if (*p == ','){ p++; continue; }

            p = read_key(p, key, sizeof(key));
            if (p == NULL) goto give_up;

            if (strcmp(key, "size") == 0){
                p = skip_space(p);
                entry.size = strtoull(p, (char **)&p, 10);
            }
            else {
                char value[MANIFEST_PATH_MAX];
                p = read_string(p, value, sizeof(value));
                if (p == NULL) goto give_up;

                if      (strcmp(key, "folder") == 0)
                    copy_field(entry.folder, sizeof(entry.folder), value);
                else if (strcmp(key, "root") == 0)
                    copy_field(entry.root, sizeof(entry.root), value);
                else if (strcmp(key, "name") == 0)
                    copy_field(entry.name, sizeof(entry.name), value);
                else if (strcmp(key, "stamp") == 0)
                    copy_field(entry.stamp, sizeof(entry.stamp), value);
                /* An unknown key is skipped rather than refused: a newer
                 * launcher may have written a field this one predates. */
            }
        }

        if (entry.folder[0] != '\0' && !manifest_put(m, &entry)) goto give_up;
    }

    free(text);
    return 1;

give_up:
    /* A file we cannot read through is no cache at all.  Half a manifest is
     * worse than none: the entries after the damage would be missing, and
     * their games would look unfamiliar for no reason the player can see. */
    free(text);
    manifest_free(m);
    return 0;
}
