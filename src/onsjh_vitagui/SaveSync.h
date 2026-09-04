/* -*- C++ -*-
 *
 *  SaveSync.h -- copying saves to and from a server on the same network
 *
 *  The local backup (SITTINGS_BACKUP) copies a game's saves to another
 *  folder on the same card, which covers a game reinstalled and nothing
 *  else: the card that dies takes the backup with it.  This copies them
 *  off the console entirely, to whatever already serves files on the
 *  network -- a NAS, a desktop, a router with a disk in it.
 *
 *  FTP because it is the one thing all of those speak without an account
 *  or a client library, and because the console's own homebrew ecosystem
 *  already assumes it.  The protocol reading lives in
 *  src/common/ftpproto.c, where it is tested; this is the sockets and the
 *  files.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __SAVESYNC_H__
#define __SAVESYNC_H__

#include <string>

class SaveSync {
public:
    /* Where the saves go, as the config file holds it. */
    struct Server {
        std::string host;
        int         port;
        std::string user;
        std::string password;
        std::string path;   /* remote folder holding one folder per game */
    };

    struct Result {
        bool        ok;
        int         games;
        int         files;
        std::string message;   /* what went wrong, when something did */
    };

    /* Read from / written to ux0:data/onsemu/ONSConfig.ini, [FTP]. */
    static Server settings();
    static void   saveSettings(const Server &server);

    /* Every installed game's saves, up to the server, one folder per game.
     * Down again is the same walk in the other direction, and only for
     * games that are installed here: a save with no game is not useful and
     * a folder full of them is not a game list. */
    static Result upload();
    static Result download();

    /* True when a host has been set, i.e. there is something to sync to. */
    static bool configured();
};

#endif /* __SAVESYNC_H__ */
