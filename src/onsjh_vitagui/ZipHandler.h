/* -*- C++ -*-
 *
 *  ZipHandler.h -- installs a game from a .zip into ux0:onsemu/
 *
 *  Wraps the portable reader in src/common/zipreader.c with the Vita side
 *  of the job: free space checks, directory creation, progress reporting
 *  and cleaning up after a failed or canceled install.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __ZIPHANDLER_H__
#define __ZIPHANDLER_H__

#include <string>
#include <vector>
#include <stdint.h>

/* Where the launcher looks for archives waiting to be installed. */
#define GAME_ZIP_FOLDER "ux0:data/game_zips"
/* Where games live once installed. */
#define GAME_INSTALL_FOLDER "ux0:onsemu"
/* Where the launcher looks for mods: patches, voice packs, anything meant
 * to go on top of a game that is already installed.  Separate from the
 * archives waiting to be installed, because a mod is not a game and the
 * game list has nothing to show for it. */
#define GAME_MOD_FOLDER "ux0:data/game_mods"

/* Keep this much of ux0: free after an install rather than filling the
 * card completely -- saves and the font cache still need room. */
#define INSTALL_SPACE_MARGIN (16 * 1024 * 1024)

enum ZipInstallStatus {
    ZIP_INSTALL_OK = 0,
    ZIP_INSTALL_BAD_ARCHIVE,    /* unreadable, corrupt, zip64, encrypted */
    ZIP_INSTALL_NO_SCRIPT,      /* no ONScripter script inside */
    ZIP_INSTALL_NO_SPACE,
    ZIP_INSTALL_EXISTS,         /* destination folder already there */
    ZIP_INSTALL_WRITE_FAILED,
    ZIP_INSTALL_CANCELED
};

/* One archive found in GAME_ZIP_FOLDER. */
struct ZipEntryInfo {
    std::string path;           /* full path to the .zip */
    std::string display_name;   /* file name without the .zip suffix */
    uint64_t    file_size;      /* size of the archive on disk */
};

/* Progress of an install in flight, polled by the UI while it draws. */
struct ZipInstallProgress {
    uint64_t    bytes_done;
    uint64_t    bytes_total;
    std::string current_file;
    int         percent;        /* 0..100 */
};

/* Called after each chunk.  Return false to cancel the install. */
typedef bool (*ZipProgressCallback)(const ZipInstallProgress &progress,
                                    void *user);

class ZipHandler {
public:
    /* List the .zip files waiting to be installed: those in
     * GAME_ZIP_FOLDER, which is created if missing, and those dropped
     * straight into the ux0:/ur0:/uma0: onsemu game folders. */
    static std::vector<ZipEntryInfo> scanZipFolder();

    /* Folder name this archive would install to, under GAME_INSTALL_FOLDER.
     * Derived from the archive's own game folder when it has one, else from
     * the file name; always reduced to characters the engine can open. */
    static std::string destinationName(const std::string &zip_path);

    /* Install zip_path into GAME_INSTALL_FOLDER.  On success installed_path
     * receives the folder that was created.  A failed or canceled install
     * removes whatever it had written, so nothing half-extracted is left in
     * the game list. */
    static ZipInstallStatus install(const std::string &zip_path,
                                    std::string &installed_path,
                                    ZipProgressCallback callback = NULL,
                                    void *user = NULL);

    /* Uncompressed size of the archive's contents, for the space warning
     * shown before an install starts.  0 if the archive cannot be read. */
    static uint64_t installedSize(const std::string &zip_path);

    /* Free bytes on the partition holding GAME_INSTALL_FOLDER. */
    static uint64_t freeSpace();

    /* An install that stopped part way leaves a journal in the folder it was
     * writing, naming the archive and the last entry that was completed.
     * Its presence is what makes a folder resumable -- and what makes it
     * not yet a game. */
    static bool isPartialInstall(const std::string &folder);
    /* How many bytes of this archive are already on the card, or 0. */
    static uint64_t resumableBytes(const std::string &zip_path);

    /* Install zip_path without extracting all of it: the files the engine
     * opens as files are written out, the archive itself is kept as
     * <dest>/game.zip, and everything else is read from it while the game
     * runs.  Roughly halves what a game costs on the card. */
    static ZipInstallStatus installCompressed(const std::string &zip_path,
                                              std::string &installed_path,
                                              ZipProgressCallback callback = NULL,
                                              void *user = NULL);

    /* Can this archive be installed compressed?  Only a .zip can: the
     * engine mounts the archive it leaves behind, and that reader speaks
     * zip.  A .7z is extracted whichever mode the setting is in. */
    static bool canInstallCompressed(const std::string &zip_path);

    /* What a compressed install of this archive would take on the card,
     * against installedSize()'s answer for an ordinary one. */
    static uint64_t compressedInstallSize(const std::string &zip_path);

    /* --- patches (mods) applied over a game that is already installed ---
     *
     * An archive with no script in it is not a game; it is almost always a
     * translation patch, a voice pack or a mod, whose files are meant to
     * land on top of a game that is already there.  Installing one is the
     * same extraction with two differences: it writes into an existing
     * folder, and it keeps a record of what it did so it can be undone. */

    /* Mods on the card: everything in GAME_MOD_FOLDER, which is created if
     * missing, plus any archive in the drop folder that turned out to have
     * no game in it -- that is where a patch ends up when it is downloaded
     * beside the games. */
    static std::vector<ZipEntryInfo> scanModFolder();

    /* Does this archive look like it belongs on that game?
     *
     * Answers with patch_confidence(): the archive's name against the
     * game's, and how much of what it would write the game already has.
     * files_total and files_matching receive the counts behind it, so the
     * prompt can show its working. */
    static int patchFit(const std::string &zip_path,
                        const std::string &game_folder,
                        int *files_total = NULL, int *files_matching = NULL);

    /* Is this archive a game, a patch, or empty?  See patch_kind. */
    static int archiveKind(const std::string &zip_path);

    /* Apply zip_path over the installed game at game_folder, keeping the
     * originals of the files it replaces.  ZIP_INSTALL_EXISTS means this
     * patch is already applied to that game. */
    static ZipInstallStatus installPatch(const std::string &zip_path,
                                         const std::string &game_folder,
                                         ZipProgressCallback callback = NULL,
                                         void *user = NULL);

    /* Patches currently applied to a game, oldest first. */
    static std::vector<std::string> appliedPatches(const std::string &game_folder);

    /* Take a patch off again: the files it replaced come back, the files it
     * added go.  Returns false if the record cannot be read. */
    static bool removePatch(const std::string &game_folder,
                            const std::string &patch_name);

    /* Message to show the user for a failed install. */
    static const char *statusMessage(ZipInstallStatus status);

private:
    static void scanOneFolder(const char *folder,
                              std::vector<ZipEntryInfo> &zips);
    static bool ensureDirectory(const std::string &path);
    static bool ensureParents(const std::string &base,
                              const std::string &relative);
};

#endif /* __ZIPHANDLER_H__ */
