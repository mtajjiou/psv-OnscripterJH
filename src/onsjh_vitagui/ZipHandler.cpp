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
#include <stdlib.h>   /* atoi, for reading the resume journal */

#include "ZipHandler.h"
#include "filesystem.h"

extern "C" {
#include "zipreader.h"
}

/* C++ rather than C: it deals in std::string, like the installer does. */
#include "installname.h"

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

/* Collect the .zip files directly inside one directory. */
void ZipHandler::scanOneFolder(const char *folder,
                               std::vector<ZipEntryInfo> &zips) {
    SceUID dfd = sceIoDopen(folder);
    if (dfd < 0) return;

    SceIoDirent dir;
    int res;
    do {
        memset(&dir, 0, sizeof(SceIoDirent));
        res = sceIoDread(dfd, &dir);
        if (res <= 0) break;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (!install_has_zip_suffix(dir.d_name)) continue;

        ZipEntryInfo info;
        info.path = std::string(folder) + "/" + dir.d_name;
        info.display_name = dir.d_name;
        info.display_name.erase(info.display_name.size() - 4);  /* ".zip" */
        info.file_size = (uint64_t)dir.d_stat.st_size;
        zips.push_back(info);
    } while (res > 0);

    sceIoDclose(dfd);
}

std::vector<ZipEntryInfo> ZipHandler::scanZipFolder() {
    std::vector<ZipEntryInfo> zips;

    /* Create the drop folder so the first-run instructions are true even
     * before the user has put anything in it. */
    ensureDirectory(GAME_ZIP_FOLDER);
    scanOneFolder(GAME_ZIP_FOLDER, zips);

    /* Also pick up archives dropped straight into the game folders. That is
     * where people naturally put them -- next to the games they already
     * have -- and an archive sitting there was previously just invisible. */
    static const char *game_dirs[] = {
        "ux0:onsemu", "ur0:onsemu", "uma0:onsemu"
    };
    for (size_t i = 0; i < sizeof(game_dirs) / sizeof(game_dirs[0]); i++)
        scanOneFolder(game_dirs[i], zips);

    return zips;
}

std::string ZipHandler::destinationName(const std::string &zip_path) {
    return install_destination_name(zip_path);
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

/* The journal an interrupted install leaves behind.
 *
 * Two lines: the archive it came from, and the index of the last entry that
 * was written in full.  Anything after that index is unfinished business.
 * It lives in the destination folder, so a folder either has one -- and is
 * a half-installed game -- or does not, and is a game. */
static const char *JOURNAL_NAME = ".install.state";

static std::string journalPath(const std::string &dest) {
    return dest + "/" + JOURNAL_NAME;
}

static void writeJournal(const std::string &dest, const std::string &zip_path,
                         int last_done) {
    SceUID fd = sceIoOpen(journalPath(dest).c_str(),
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return;

    char line[ZIP_MAX_NAME + 32];
    int len = snprintf(line, sizeof(line), "%s\n%d\n", zip_path.c_str(),
                       last_done);
    sceIoWrite(fd, line, len);
    sceIoClose(fd);
}

/* Returns the last completed entry, or -1 if there is no journal here.
 * from_zip receives the archive it names. */
static int readJournal(const std::string &dest, std::string &from_zip) {
    SceUID fd = sceIoOpen(journalPath(dest).c_str(), SCE_O_RDONLY, 0777);
    if (fd < 0) return -1;

    char buffer[ZIP_MAX_NAME + 32];
    int got = sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);
    if (got <= 0) return -1;
    buffer[got] = '\0';

    char *newline = strchr(buffer, '\n');
    if (newline == NULL) return -1;
    *newline = '\0';

    from_zip = buffer;
    return atoi(newline + 1);
}

static void clearJournal(const std::string &dest) {
    sceIoRemove(journalPath(dest).c_str());
}

bool ZipHandler::isPartialInstall(const std::string &folder) {
    SceUID fd = sceIoOpen(journalPath(folder).c_str(), SCE_O_RDONLY, 0777);
    if (fd < 0) return false;
    sceIoClose(fd);
    return true;
}

uint64_t ZipHandler::resumableBytes(const std::string &zip_path) {
    const std::string dest =
        std::string(GAME_INSTALL_FOLDER) + "/" + destinationName(zip_path);

    std::string from_zip;
    const int last_done = readJournal(dest, from_zip);
    if (last_done < 0 || from_zip != zip_path) return 0;

    int err = 0;
    zip_reader *z = zip_open(zip_path.c_str(), &err);
    if (!z) return 0;

    uint64_t done = 0;
    for (int i = 0; i <= last_done && i < zip_count(z); i++)
        if (!zip_entry_is_dir(z, i)) done += zip_entry_size(z, i);

    zip_close(z);
    return done;
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

    /* A folder that is already there is either a game -- in which case there
     * is nothing to do -- or an install that stopped part way, which is
     * picked up where it left off rather than started again.  On a card and
     * an archive this size, starting again can mean many minutes. */
    int resume_from = 0;
    if (checkFolderExist(dest.c_str())) {
        std::string from_zip;
        const int last_done = readJournal(dest, from_zip);
        if (last_done < 0 || from_zip != zip_path) {
            zip_close(z);
            return ZIP_INSTALL_EXISTS;
        }
        resume_from = last_done + 1;
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
        /* Already on the card: counted so the bar starts where the last
         * attempt stopped rather than at zero. */
        if (i < resume_from) {
            if (!zip_entry_is_dir(z, i)) progress.bytes_done += zip_entry_size(z, i);
            continue;
        }

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
        else {
            /* Written in full, so an interrupted install can start after
             * it.  One small write per file is cheap beside the file. */
            writeJournal(dest, zip_path, i);
        }
    }

    zip_close(z);

    if (status != ZIP_INSTALL_OK) {
        /* What was written stays, with its journal, so the install can be
         * finished later instead of paid for twice.
         *
         * Deleting it was how a half-installed folder was kept from
         * appearing in the game list and failing confusingly at launch.
         * That job moves to the journal: a folder that has one is listed as
         * unfinished and refuses to start, so it is visible enough to
         * resume or delete without pretending to be a game.
         *
         * Nothing was written for a failure this early, so there is nothing
         * to keep and an empty folder would be litter. */
        if (resume_from == 0 && progress.bytes_done == 0)
            removePath(dest);
        return status;
    }

    clearJournal(dest);
    installed_path = dest;
    return ZIP_INSTALL_OK;
}
