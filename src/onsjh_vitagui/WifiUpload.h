/* -*- C++ -*-
 *
 *  WifiUpload.h -- putting a game on the console from a browser
 *
 *  Copying an archive across otherwise means an FTP client, or taking the
 *  card out.  With this the launcher listens on the console's own address
 *  and serves one page: what is installed, and a box to send a .zip to.
 *  What arrives lands in ux0:data/game_zips/, where the installer already
 *  looks.
 *
 *  It is a server on a console, so it does two things deliberately: it
 *  never blocks -- every call returns immediately and the screen keeps
 *  drawing while a four gigabyte archive arrives -- and it accepts only
 *  .zip files into one folder, since anyone on the same network can reach
 *  it while it is running.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __WIFIUPLOAD_H__
#define __WIFIUPLOAD_H__

#include <string>
#include <stdint.h>

class WifiUpload {
public:
    enum State {
        STOPPED = 0,
        WAITING,     /* listening, nothing connected */
        RECEIVING,   /* a file is arriving */
        FAILED       /* could not start, or the connection broke */
    };

    struct Status {
        State       state;
        std::string address;    /* http://10.0.0.5:8080, to type into a browser */
        std::string file;       /* the archive arriving, or the last one */
        uint64_t    bytes;      /* of it, so far */
        uint64_t    expected;   /* 0 when the browser did not say */
        int         received;   /* archives accepted since starting */
        std::string message;    /* why it failed, when it did */
    };

    /* Bring up the network and start listening.  False if there is no
     * connection, or the port is taken. */
    static bool start();

    /* Do whatever can be done without waiting: accept a connection, read
     * what has arrived, write it out.  Called once per frame. */
    static void poll();

    static void stop();

    static const Status &status();
};

#endif /* __WIFIUPLOAD_H__ */
