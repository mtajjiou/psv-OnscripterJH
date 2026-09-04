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
#include "archive.h"
#include "patchplan.h"
#include "zipfs.h"
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
        /* ".zip" is four characters and ".7z" is three: asking is what
         * keeps a mod from being listed with a letter of its name gone. */
        info.display_name.erase(info.display_name.size() -
                                (size_t)archive_suffix_length(dir.d_name));
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
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return 0;
    uint64_t total = archive_total_size(z);
    archive_close(z);
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
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return 0;

    uint64_t done = 0;
    for (int i = 0; i <= last_done && i < archive_count(z); i++)
        if (!archive_entry_is_dir(z, i)) done += archive_entry_size(z, i);

    archive_close(z);
    return done;
}

ZipInstallStatus ZipHandler::install(const std::string &zip_path,
                                     std::string &installed_path,
                                     ZipProgressCallback callback,
                                     void *user) {
    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return ZIP_INSTALL_BAD_ARCHIVE;

    char root[ZIP_MAX_NAME];
    if (!archive_find_game_root(z, root, sizeof(root))) {
        archive_close(z);
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
            archive_close(z);
            return ZIP_INSTALL_EXISTS;
        }
        resume_from = last_done + 1;
    }

    uint64_t needed = archive_total_size(z);
    uint64_t available = freeSpace();
    if (available > 0 && needed + INSTALL_SPACE_MARGIN > available) {
        archive_close(z);
        return ZIP_INSTALL_NO_SPACE;
    }

    if (!ensureDirectory(GAME_INSTALL_FOLDER) || !ensureDirectory(dest)) {
        archive_close(z);
        return ZIP_INSTALL_WRITE_FAILED;
    }

    ZipInstallProgress progress;
    progress.bytes_done  = 0;
    progress.bytes_total = needed;
    progress.percent     = 0;

    ZipInstallStatus status = ZIP_INSTALL_OK;
    const int count = archive_count(z);

    for (int i = 0; i < count && status == ZIP_INSTALL_OK; i++) {
        /* Already on the card: counted so the bar starts where the last
         * attempt stopped rather than at zero. */
        if (i < resume_from) {
            if (!archive_entry_is_dir(z, i)) progress.bytes_done += archive_entry_size(z, i);
            continue;
        }

        char clean[ZIP_MAX_NAME];
        if (zip_sanitize_name(archive_entry_name(z, i), clean, sizeof(clean)) != ZIP_OK)
            continue;   /* an unsafe name is skipped, never written */

        std::string relative = clean;
        if (!prefix.empty()) {
            if (relative.compare(0, prefix.size(), prefix) != 0) continue;
            relative.erase(0, prefix.size());
        }
        if (relative.empty()) continue;

        if (archive_entry_is_dir(z, i)) {
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

        int rc = archive_extract_entry(z, i, writeChunk, &ctx);
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

    archive_close(z);

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


/* ------------------------------------------------------------------ *
 *  Patches applied over a game that is already installed
 * ------------------------------------------------------------------ */

/* Where a game keeps the record of the patches applied to it, and the
 * originals of the files those patches replaced. */
static std::string patchFolder(const std::string &game_folder) {
    return game_folder + "/" + PATCH_FOLDER;
}

static std::string patchRecordPath(const std::string &game_folder,
                                   const std::string &record_name) {
    return patchFolder(game_folder) + "/" + record_name;
}

/* Folder holding the originals this patch replaced: the record's name
 * without its suffix, beside the record. */
static std::string patchBackupFolder(const std::string &game_folder,
                                     const std::string &record_name) {
    std::string base = record_name;
    const size_t suffix = strlen(PATCH_RECORD_SUFFIX);
    if (base.size() > suffix) base.erase(base.size() - suffix);
    return patchFolder(game_folder) + "/" + base;
}

int ZipHandler::archiveKind(const std::string &zip_path) {
    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return -1;
    int kind = patch_archive_kind(z);
    archive_close(z);
    return kind;
}

std::vector<std::string> ZipHandler::appliedPatches(const std::string &game_folder) {
    std::vector<std::string> names;
    const std::string folder = patchFolder(game_folder);

    SceUID dfd = sceIoDopen(folder.c_str());
    if (dfd < 0) return names;

    SceIoDirent dir;
    int res;
    const size_t suffix = strlen(PATCH_RECORD_SUFFIX);
    do {
        memset(&dir, 0, sizeof(SceIoDirent));
        res = sceIoDread(dfd, &dir);
        if (res <= 0) break;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        std::string name = dir.d_name;
        if (name.size() <= suffix) continue;
        if (name.compare(name.size() - suffix, suffix, PATCH_RECORD_SUFFIX) != 0)
            continue;
        names.push_back(name);
    } while (res > 0);

    sceIoDclose(dfd);
    return names;
}

/* One line appended to the record as each file is written, so a patch
 * interrupted half way is still undoable: the record on the card always
 * describes what is on the card. */
static void appendPatchLine(const std::string &record, char kind,
                            const std::string &relative) {
    SceUID fd = sceIoOpen(record.c_str(),
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd < 0) return;
    std::string line;
    line += kind;
    line += ' ';
    line += relative;
    line += '\n';
    sceIoWrite(fd, line.c_str(), line.size());
    sceIoClose(fd);
}

ZipInstallStatus ZipHandler::installPatch(const std::string &zip_path,
                                          const std::string &game_folder,
                                          ZipProgressCallback callback,
                                          void *user) {
    if (!checkFolderExist(game_folder.c_str()))
        return ZIP_INSTALL_WRITE_FAILED;

    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return ZIP_INSTALL_BAD_ARCHIVE;

    /* Nothing in it is not a patch; saying "installed" for an archive that
     * changed no file is how a bad download passes for a good one. */
    const int kind = patch_archive_kind(z);
    if (kind == PATCH_KIND_EMPTY) {
        archive_close(z);
        return ZIP_INSTALL_BAD_ARCHIVE;
    }

    /* A full game archive can be overlaid too -- that is how a game is
     * updated in place -- and then it is its game folder that overlays,
     * not the wrapper around it. */
    char root[ZIP_MAX_NAME];
    if (kind == PATCH_KIND_GAME) {
        if (!archive_find_game_root(z, root, sizeof(root))) {
            archive_close(z);
            return ZIP_INSTALL_NO_SCRIPT;
        }
    }
    else if (!patch_overlay_root(z, root, sizeof(root))) {
        archive_close(z);
        return ZIP_INSTALL_BAD_ARCHIVE;
    }

    std::string prefix = root;
    if (!prefix.empty()) prefix += "/";

    char record_name[128];
    if (!patch_record_name(zip_path.c_str(), record_name, sizeof(record_name))) {
        archive_close(z);
        return ZIP_INSTALL_WRITE_FAILED;
    }

    const std::string record  = patchRecordPath(game_folder, record_name);
    const std::string backups = patchBackupFolder(game_folder, record_name);

    /* Applying the same patch twice would back up its own files over the
     * game's originals, and the game could never be got back. */
    if (checkFileExist(record.c_str())) {
        archive_close(z);
        return ZIP_INSTALL_EXISTS;
    }

    const uint64_t needed = archive_total_size(z);
    const uint64_t available = freeSpace();
    /* Twice over: the files going on, and the originals coming off. */
    if (available > 0 && needed * 2 + INSTALL_SPACE_MARGIN > available) {
        archive_close(z);
        return ZIP_INSTALL_NO_SPACE;
    }

    if (!ensureDirectory(patchFolder(game_folder)) ||
        !ensureDirectory(backups)) {
        archive_close(z);
        return ZIP_INSTALL_WRITE_FAILED;
    }

    ZipInstallProgress progress;
    progress.bytes_done  = 0;
    progress.bytes_total = needed;
    progress.percent     = 0;

    ZipInstallStatus status = ZIP_INSTALL_OK;
    const int count = archive_count(z);

    for (int i = 0; i < count && status == ZIP_INSTALL_OK; i++) {
        char clean[ZIP_MAX_NAME];
        if (zip_sanitize_name(archive_entry_name(z, i), clean, sizeof(clean)) != ZIP_OK)
            continue;

        std::string relative = clean;
        if (!prefix.empty()) {
            if (relative.compare(0, prefix.size(), prefix) != 0) continue;
            relative.erase(0, prefix.size());
        }
        if (relative.empty()) continue;

        if (archive_entry_is_dir(z, i)) {
            relative.erase(relative.size() - 1);
            if (!relative.empty() &&
                !ensureDirectory(game_folder + "/" + relative))
                status = ZIP_INSTALL_WRITE_FAILED;
            continue;
        }

        if (!ensureParents(game_folder, relative)) {
            status = ZIP_INSTALL_WRITE_FAILED;
            break;
        }

        const std::string out_path = game_folder + "/" + relative;
        const bool replacing = checkFileExist(out_path.c_str()) != 0;

        if (replacing) {
            /* The original goes to the backup folder before it is lost.
             * Failing to keep it is a reason to stop, not to carry on: a
             * patch that cannot be undone is worse than one not applied. */
            if (!ensureParents(backups, relative) ||
                copyFile(out_path.c_str(), (backups + "/" + relative).c_str()) < 0) {
                status = ZIP_INSTALL_WRITE_FAILED;
                break;
            }
        }

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

        /* Recorded before the write rather than after it: a file half
         * written is still a file that has to be put back. */
        appendPatchLine(record, replacing ? PATCH_LINE_REPLACED : PATCH_LINE_NEW,
                        relative);

        int rc = archive_extract_entry(z, i, writeChunk, &ctx);
        sceIoClose(fd);

        if (rc != ZIP_OK) {
            if (ctx.canceled)          status = ZIP_INSTALL_CANCELED;
            else if (ctx.write_failed) status = ZIP_INSTALL_WRITE_FAILED;
            else                       status = ZIP_INSTALL_BAD_ARCHIVE;
        }
    }

    archive_close(z);

    if (status != ZIP_INSTALL_OK) {
        /* Unlike a game install, which keeps what it wrote so it can be
         * resumed, a patch left half on is a game that behaves oddly with
         * no sign of why.  It comes straight back off. */
        removePatch(game_folder, record_name);
        return status;
    }

    return ZIP_INSTALL_OK;
}

bool ZipHandler::removePatch(const std::string &game_folder,
                             const std::string &patch_name) {
    const std::string record  = patchRecordPath(game_folder, patch_name);
    const std::string backups = patchBackupFolder(game_folder, patch_name);

    SceUID fd = sceIoOpen(record.c_str(), SCE_O_RDONLY, 0777);
    if (fd < 0) return false;

    /* The record is one line per file and small enough to read whole. */
    std::string text;
    char buffer[1024];
    int got;
    while ((got = sceIoRead(fd, buffer, sizeof(buffer))) > 0)
        text.append(buffer, (size_t)got);
    sceIoClose(fd);

    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();

        const std::string line = text.substr(start, end - start);
        start = end + 1;

        char kind = 0;
        char relative[ZIP_MAX_NAME];
        if (!patch_parse_line(line.c_str(), &kind, relative, sizeof(relative)))
            continue;

        const std::string target = game_folder + "/" + relative;
        if (kind == PATCH_LINE_REPLACED) {
            const std::string saved = backups + "/" + relative;
            if (checkFileExist(saved.c_str()))
                copyFile(saved.c_str(), target.c_str());
        }
        else {
            sceIoRemove(target.c_str());
        }
    }

    removePath(backups);
    sceIoRemove(record.c_str());
    return true;
}


/* ------------------------------------------------------------------ *
 *  Installing without extracting
 * ------------------------------------------------------------------ */

bool ZipHandler::canInstallCompressed(const std::string &zip_path) {
    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return false;

    const bool zip = (archive_kind_of(z) == ARCHIVE_ZIP);
    archive_close(z);
    return zip;
}

uint64_t ZipHandler::compressedInstallSize(const std::string &zip_path) {
    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return 0;

    /* The archive stays, whole, plus the files that cannot be read out of
     * it.  Those are the ones already compressed, so their size in the
     * archive is close enough to their size on the card to count once. */
    uint64_t total = 0;
    for (int i = 0; i < archive_count(z); i++) {
        if (archive_entry_is_dir(z, i)) continue;
        if (zipfs_needs_disk(archive_entry_name(z, i)))
            total += archive_entry_size(z, i);
    }
    archive_close(z);

    SceIoStat stat;
    memset(&stat, 0, sizeof(stat));
    if (sceIoGetstat(zip_path.c_str(), &stat) >= 0)
        total += (uint64_t)stat.st_size;

    return total;
}

ZipInstallStatus ZipHandler::installCompressed(const std::string &zip_path,
                                               std::string &installed_path,
                                               ZipProgressCallback callback,
                                               void *user) {
    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return ZIP_INSTALL_BAD_ARCHIVE;

    char root[ZIP_MAX_NAME];
    if (!archive_find_game_root(z, root, sizeof(root))) {
        archive_close(z);
        return ZIP_INSTALL_NO_SCRIPT;
    }

    std::string prefix = root;
    if (!prefix.empty()) prefix += "/";

    const std::string dest =
        std::string(GAME_INSTALL_FOLDER) + "/" + destinationName(zip_path);

    /* No journal and no resuming here: the expensive part is one file
     * copy, and half a copy is not something to pick up in the middle. */
    if (checkFolderExist(dest.c_str())) {
        archive_close(z);
        return ZIP_INSTALL_EXISTS;
    }

    const uint64_t needed = compressedInstallSize(zip_path);
    const uint64_t available = freeSpace();
    if (available > 0 && needed + INSTALL_SPACE_MARGIN > available) {
        archive_close(z);
        return ZIP_INSTALL_NO_SPACE;
    }

    if (!ensureDirectory(GAME_INSTALL_FOLDER) || !ensureDirectory(dest)) {
        archive_close(z);
        return ZIP_INSTALL_WRITE_FAILED;
    }

    ZipInstallProgress progress;
    progress.bytes_done  = 0;
    progress.bytes_total = needed;
    progress.percent     = 0;

    ZipInstallStatus status = ZIP_INSTALL_OK;
    const int count = archive_count(z);

    for (int i = 0; i < count && status == ZIP_INSTALL_OK; i++) {
        char clean[ZIP_MAX_NAME];
        if (zip_sanitize_name(archive_entry_name(z, i), clean, sizeof(clean)) != ZIP_OK)
            continue;

        std::string relative = clean;
        if (!prefix.empty()) {
            if (relative.compare(0, prefix.size(), prefix) != 0) continue;
            relative.erase(0, prefix.size());
        }
        if (relative.empty()) continue;
        if (archive_entry_is_dir(z, i)) continue;

        /* The rest stays in the archive: this is the whole point. */
        if (!zipfs_needs_disk(relative.c_str())) continue;

        if (!ensureParents(dest, relative)) {
            status = ZIP_INSTALL_WRITE_FAILED;
            break;
        }

        SceUID fd = sceIoOpen((dest + "/" + relative).c_str(),
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

        int rc = archive_extract_entry(z, i, writeChunk, &ctx);
        sceIoClose(fd);

        if (rc != ZIP_OK) {
            if (ctx.canceled)          status = ZIP_INSTALL_CANCELED;
            else if (ctx.write_failed) status = ZIP_INSTALL_WRITE_FAILED;
            else                       status = ZIP_INSTALL_BAD_ARCHIVE;
        }
    }

    archive_close(z);

    if (status == ZIP_INSTALL_OK) {
        /* The archive itself, which the engine will read the game out of.
         * Moved when it can be -- a rename within ux0: is instant, where
         * copying a four gigabyte archive is not -- and copied when it
         * cannot, which is when it lives on another partition. */
        const std::string archive = dest + "/" + ZIPFS_ARCHIVE_NAME;
        progress.current_file = ZIPFS_ARCHIVE_NAME;
        if (callback) callback(progress, user);

        if (sceIoRename(zip_path.c_str(), archive.c_str()) < 0 &&
            copyFile(zip_path.c_str(), archive.c_str()) < 0)
            status = ZIP_INSTALL_WRITE_FAILED;
    }

    if (status != ZIP_INSTALL_OK) {
        /* Nothing here is resumable, so nothing here is worth keeping. */
        removePath(dest);
        return status;
    }

    installed_path = dest;
    return ZIP_INSTALL_OK;
}


/* ------------------------------------------------------------------ *
 *  Mods: what is on the card, and whether one belongs on a game
 * ------------------------------------------------------------------ */

std::vector<ZipEntryInfo> ZipHandler::scanModFolder() {
    std::vector<ZipEntryInfo> mods;

    /* Created here so the folder exists to be told about before anyone has
     * put anything in it. */
    ensureDirectory("ux0:data");
    ensureDirectory(GAME_MOD_FOLDER);
    scanOneFolder(GAME_MOD_FOLDER, mods);

    /* A mod downloaded beside the games ends up in the drop folder, so
     * those are listed here too -- all of them, whatever is inside.
     *
     * Filtering by "has no script in it" was wrong: a 16:9 patch, a
     * retranslation, anything that changes what the game says all ship
     * a script of their own, and those are patches by any useful reading.
     * Nothing in an archive says which it is, and the list is not where
     * that gets decided: patchFit() weighs the archive against the game
     * the player picked it for, which is evidence rather than a guess. */
    scanOneFolder(GAME_ZIP_FOLDER, mods);

    return mods;
}

int ZipHandler::patchFit(const std::string &zip_path,
                         const std::string &game_folder,
                         int *files_total, int *files_matching) {
    if (files_total)    *files_total = 0;
    if (files_matching) *files_matching = 0;

    int err = 0;
    archive *z = archive_open(zip_path.c_str(), &err);
    if (!z) return 0;

    /* The same root the install would strip, so the paths compared are the
     * paths that would be written. */
    char root[ZIP_MAX_NAME];
    if (patch_archive_kind(z) == PATCH_KIND_GAME) {
        if (!archive_find_game_root(z, root, sizeof(root))) root[0] = '\0';
    }
    else if (!patch_overlay_root(z, root, sizeof(root))) {
        archive_close(z);
        return 0;
    }

    std::string prefix = root;
    if (!prefix.empty()) prefix += "/";

    int total = 0, matching = 0;
    for (int i = 0; i < archive_count(z); i++) {
        if (archive_entry_is_dir(z, i)) continue;

        char clean[ZIP_MAX_NAME];
        if (zip_sanitize_name(archive_entry_name(z, i), clean, sizeof(clean)) != ZIP_OK)
            continue;

        std::string relative = clean;
        if (!prefix.empty()) {
            if (relative.compare(0, prefix.size(), prefix) != 0) continue;
            relative.erase(0, prefix.size());
        }
        if (relative.empty()) continue;

        total++;
        if (checkFileExist((game_folder + "/" + relative).c_str())) matching++;
    }
    archive_close(z);

    if (files_total)    *files_total = total;
    if (files_matching) *files_matching = matching;

    /* The game's folder name is what the archive's name is matched
     * against, since that is the name the player sees in the list. */
    std::string game = game_folder;
    const size_t slash = game.find_last_of("/\\");
    if (slash != std::string::npos) game = game.substr(slash + 1);

    const int name_score =
        patch_name_match(install_base_name(zip_path).c_str(), game.c_str());

    return patch_confidence(name_score, total, matching);
}
