/* -*- C++ -*-
 *
 *  ZipHandler.cpp -- installs a game from a .zip into ux0:onsemu/
 *
 *  See ZipHandler.h for the interface.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/appmgr.h>
#include <string.h>
#include <stdio.h>

#include "ZipHandler.h"
#include "filesystem.h"

extern "C" {
#include "zipreader.h"
}

/* State threaded through the extraction callbacks. */
namespace {

struct WriteContext {
    SceUID              fd;
    bool                write_failed;
    bool                canceled;
    ZipInstallProgress *progress;
    ZipProgressCallback callback;
    void               *user;
};

int writeChunk(void *user, const void *data, size_t len) {
    WriteContext *ctx = (WriteContext *)user;
    const char *p = (const char *)data;
    size_t left = len;

    while (left > 0) {
        int written = sceIoWrite(ctx->fd, p, left);
        if (written <= 0) {
            ctx->write_failed = true;
            return -1;
        }
        p    += written;
        left -= (size_t)written;
    }

    ctx->progress->bytes_done += len;
    if (ctx->progress->bytes_total > 0) {
        ctx->progress->percent = (int)((ctx->progress->bytes_done * 100) /
                                       ctx->progress->bytes_total);
        if (ctx->progress->percent > 100) ctx->progress->percent = 100;
    }

    if (ctx->callback && !ctx->callback(*ctx->progress, ctx->user)) {
        ctx->canceled = true;
        return -1;
    }
    return 0;
}

/* Case-insensitive ".zip" test. Spelled out rather than using strcasecmp,
 * which the vitasdk toolchain does not declare via <string.h>. */
bool hasZipSuffix(const char *name) {
    size_t len = strlen(name);
    if (len < 4) return false;

    const char *ext = name + len - 4;
    return ext[0] == '.' &&
           (ext[1] == 'z' || ext[1] == 'Z') &&
           (ext[2] == 'i' || ext[2] == 'I') &&
           (ext[3] == 'p' || ext[3] == 'P');
}

/* The engine opens game paths with plain byte strings and chokes on names
 * carrying non-ASCII bytes, so fold anything unusual down to '_'. */
std::string makeSafeFolderName(const std::string &raw) {
    std::string out;

    for (size_t i = 0; i < raw.size(); i++) {
        unsigned char c = (unsigned char)raw[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '(' || c == ')') {
            out += (char)c;
        } else if (c == ' ' || c == '+') {
            out += '_';
        } else if (!out.empty() && out[out.size() - 1] != '_') {
            out += '_';
        }
    }

    /* Trailing separators and dots confuse the filesystem. */
    while (!out.empty() && (out[out.size() - 1] == '_' ||
                            out[out.size() - 1] == '.'))
        out.erase(out.size() - 1);

    if (out.empty()) out = "game";
    if (out.size() > 64) out.erase(64);
    return out;
}

std::string baseName(const std::string &path) {
    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

} /* namespace */

bool ZipHandler::ensureDirectory(const std::string &path) {
    if (checkFolderExist(path.c_str())) return true;
    int res = sceIoMkdir(path.c_str(), 0777);
    /* Another entry may have created it between the check and the mkdir. */
    return res >= 0 || checkFolderExist(path.c_str());
}

bool ZipHandler::ensureParents(const std::string &base,
                               const std::string &relative) {
    size_t start = 0;

    while (true) {
        size_t slash = relative.find('/', start);
        if (slash == std::string::npos) break;
        if (!ensureDirectory(base + "/" + relative.substr(0, slash)))
            return false;
        start = slash + 1;
    }
    return true;
}

std::vector<ZipEntryInfo> ZipHandler::scanZipFolder() {
    std::vector<ZipEntryInfo> zips;

    /* Create the drop folder so the first-run instructions are true even
     * before the user has put anything in it. */
    ensureDirectory(GAME_ZIP_FOLDER);

    SceUID dfd = sceIoDopen(GAME_ZIP_FOLDER);
    if (dfd < 0) return zips;

    SceIoDirent dir;
    int res;
    do {
        memset(&dir, 0, sizeof(SceIoDirent));
        res = sceIoDread(dfd, &dir);
        if (res <= 0) break;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (!hasZipSuffix(dir.d_name)) continue;

        ZipEntryInfo info;
        info.path = std::string(GAME_ZIP_FOLDER) + "/" + dir.d_name;
        info.display_name = dir.d_name;
        info.display_name.erase(info.display_name.size() - 4);  /* ".zip" */
        info.file_size = (uint64_t)dir.d_stat.st_size;
        zips.push_back(info);
    } while (res > 0);

    sceIoDclose(dfd);
    return zips;
}

std::string ZipHandler::destinationName(const std::string &zip_path) {
    std::string name;
    int err = 0;

    zip_reader *z = zip_open(zip_path.c_str(), &err);
    if (z) {
        char root[ZIP_MAX_NAME];
        /* A game folder inside the archive names the game better than the
         * archive file does; fall back to the file name when it is at the
         * archive root. */
        if (zip_find_game_root(z, root, sizeof(root)) && root[0] != '\0')
            name = baseName(root);
        zip_close(z);
    }

    if (name.empty()) {
        name = baseName(zip_path);
        if (name.size() > 4 && hasZipSuffix(name.c_str()))
            name.erase(name.size() - 4);
    }
    return makeSafeFolderName(name);
}

uint64_t ZipHandler::installedSize(const std::string &zip_path) {
    int err = 0;
    zip_reader *z = zip_open(zip_path.c_str(), &err);
    if (!z) return 0;
    uint64_t total = zip_total_size(z);
    zip_close(z);
    return total;
}

uint64_t ZipHandler::freeSpace() {
    uint64_t max_size = 0, free_size = 0;
    if (sceAppMgrGetDevInfo("ux0:", &max_size, &free_size) < 0)
        return 0;
    return free_size;
}

const char *ZipHandler::statusMessage(ZipInstallStatus status) {
    switch (status) {
    case ZIP_INSTALL_OK:
        return "Installed";
    case ZIP_INSTALL_BAD_ARCHIVE:
        return "This archive cannot be read.\n"
               "It may be corrupt, password protected, or zip64.";
    case ZIP_INSTALL_NO_SCRIPT:
        return "No game script found in this archive.\n"
               "Expected 0.txt, nscript.dat, onscript.nt2 or similar.";
    case ZIP_INSTALL_NO_SPACE:
        return "Not enough free space on ux0: to install this game.";
    case ZIP_INSTALL_EXISTS:
        return "A game with this name is already installed.\n"
               "Delete it first, or rename the archive.";
    case ZIP_INSTALL_WRITE_FAILED:
        return "Writing to ux0: failed. The install was rolled back.";
    case ZIP_INSTALL_CANCELED:
        return "Install canceled. Partly extracted files were removed.";
    }
    return "Install failed.";
}

ZipInstallStatus ZipHandler::install(const std::string &zip_path,
                                     std::string &installed_path,
                                     ZipProgressCallback callback,
                                     void *user) {
    int err = 0;
    zip_reader *z = zip_open(zip_path.c_str(), &err);
    if (!z) return ZIP_INSTALL_BAD_ARCHIVE;

    char root[ZIP_MAX_NAME];
    if (!zip_find_game_root(z, root, sizeof(root))) {
        zip_close(z);
        return ZIP_INSTALL_NO_SCRIPT;
    }

    /* Only entries under the game root are installed; a sibling "readme"
     * or "patch" folder beside the game is left behind on purpose. */
    std::string prefix = root;
    if (!prefix.empty()) prefix += "/";

    const std::string dest =
        std::string(GAME_INSTALL_FOLDER) + "/" + destinationName(zip_path);
    if (checkFolderExist(dest.c_str())) {
        zip_close(z);
        return ZIP_INSTALL_EXISTS;
    }

    uint64_t needed = zip_total_size(z);
    uint64_t available = freeSpace();
    if (available > 0 && needed + INSTALL_SPACE_MARGIN > available) {
        zip_close(z);
        return ZIP_INSTALL_NO_SPACE;
    }

    if (!ensureDirectory(GAME_INSTALL_FOLDER) || !ensureDirectory(dest)) {
        zip_close(z);
        return ZIP_INSTALL_WRITE_FAILED;
    }

    ZipInstallProgress progress;
    progress.bytes_done  = 0;
    progress.bytes_total = needed;
    progress.percent     = 0;

    ZipInstallStatus status = ZIP_INSTALL_OK;
    const int count = zip_count(z);

    for (int i = 0; i < count && status == ZIP_INSTALL_OK; i++) {
        char clean[ZIP_MAX_NAME];
        if (zip_sanitize_name(zip_entry_name(z, i), clean, sizeof(clean)) != ZIP_OK)
            continue;   /* an unsafe name is skipped, never written */

        std::string relative = clean;
        if (!prefix.empty()) {
            if (relative.compare(0, prefix.size(), prefix) != 0) continue;
            relative.erase(0, prefix.size());
        }
        if (relative.empty()) continue;

        if (zip_entry_is_dir(z, i)) {
            relative.erase(relative.size() - 1);  /* trailing '/' */
            if (!relative.empty() && !ensureDirectory(dest + "/" + relative))
                status = ZIP_INSTALL_WRITE_FAILED;
            continue;
        }

        if (!ensureParents(dest, relative)) {
            status = ZIP_INSTALL_WRITE_FAILED;
            break;
        }

        const std::string out_path = dest + "/" + relative;
        SceUID fd = sceIoOpen(out_path.c_str(),
                              SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (fd < 0) {
            status = ZIP_INSTALL_WRITE_FAILED;
            break;
        }

        WriteContext ctx;
        ctx.fd           = fd;
        ctx.write_failed = false;
        ctx.canceled     = false;
        ctx.progress     = &progress;
        ctx.callback     = callback;
        ctx.user         = user;

        progress.current_file = relative;

        int rc = zip_extract_entry(z, i, writeChunk, &ctx);
        sceIoClose(fd);

        if (rc != ZIP_OK) {
            if (ctx.canceled)          status = ZIP_INSTALL_CANCELED;
            else if (ctx.write_failed) status = ZIP_INSTALL_WRITE_FAILED;
            else                       status = ZIP_INSTALL_BAD_ARCHIVE;
        }
    }

    zip_close(z);

    if (status != ZIP_INSTALL_OK) {
        /* Leave nothing half-installed: a partial folder would show up in
         * the game list and fail confusingly at launch. */
        removePath(dest);
        return status;
    }

    installed_path = dest;
    return ZIP_INSTALL_OK;
}
