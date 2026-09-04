/* -*- C++ -*-
 *
 *  SaveSync.cpp -- see SaveSync.h
 *
 *  A sync moves kilobytes: a game's saves are a handful of small files, so
 *  this is written the plain way -- one connection, one command at a time,
 *  with a timeout on every wait so a server that stops answering ends the
 *  sync instead of the session.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "SaveSync.h"
#include "GUI_Utils.h"
#include "GUI_common.h"

extern "C" {
#include "ftpproto.h"
#include "iniparser.h"
}

namespace {

const int NET_MEMORY = 512 * 1024;
/* Long enough for a server on the other side of a router, short enough
 * that a server that is not there stops being waited for. */
const unsigned int TIMEOUT_US = 8 * 1000 * 1000;
const int TRANSFER_CHUNK = 8 * 1024;

void  *g_net_memory = NULL;
bool   g_net_started = false;

bool startNetwork() {
    if (g_net_started) return true;
    if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0) return false;

    g_net_memory = malloc(NET_MEMORY);
    if (g_net_memory == NULL) return false;

    SceNetInitParam param;
    param.memory = g_net_memory;
    param.size   = NET_MEMORY;
    param.flags  = 0;

    /* Already up is the usual answer, and not a failure. */
    sceNetInit(&param);
    sceNetCtlInit();

    g_net_started = true;
    return true;
}

bool networkIsUp() {
    int state = 0;
    if (sceNetCtlInetGetState(&state) < 0) return false;
    return state == SCE_NETCTL_STATE_CONNECTED;
}

/* One FTP conversation. */
class Session {
public:
    Session() : control(-1), data(-1) {}
    ~Session() { close(); }

    bool open(const SaveSync::Server &server, std::string &error);
    void close();

    /* Send a command and read the reply, following continuation lines.
     * The reply's code is returned; reply holds its last line. */
    int  command(const char *format, ...);
    const std::string &lastReply() const { return reply; }

    /* Open a data connection for the next transfer. */
    bool openData();
    void closeData();

    bool storeFile(const std::string &local, const std::string &remote);
    bool retrieveFile(const std::string &remote, const std::string &local);
    bool listNames(const std::string &remote, std::vector<std::string> &names);

    void makeDirectory(const std::string &remote) {
        command("MKD %s", remote.c_str());   /* already there is fine */
    }

private:
    int         control;
    int         data;
    /* Kept for the extended passive reply, which names a port and leaves
     * the address implied. */
    std::string host;
    std::string reply;
    std::string pending;    /* bytes read past the end of a reply line */

    bool readLine(std::string &line);
    bool sendLine(const std::string &line);
};

int connectTo(const char *host, int port) {
    SceNetSockaddrIn address;
    int socket;

    memset(&address, 0, sizeof(address));
    address.sin_family = SCE_NET_AF_INET;
    address.sin_port   = sceNetHtons((unsigned short)port);

    if (sceNetInetPton(SCE_NET_AF_INET, host, &address.sin_addr) <= 0) {
        /* A name rather than an address: ask the console's resolver. */
        int rid = sceNetResolverCreate("ons_sync", NULL, 0);
        if (rid < 0) return -1;
        int res = sceNetResolverStartNtoa(rid, host, &address.sin_addr,
                                          (int)(TIMEOUT_US / 1000), 2, 0);
        sceNetResolverDestroy(rid);
        if (res < 0) return -1;
    }

    socket = sceNetSocket("ons_sync", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
    if (socket < 0) return -1;

    unsigned int timeout = TIMEOUT_US;
    sceNetSetsockopt(socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
                     &timeout, sizeof(timeout));
    sceNetSetsockopt(socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO,
                     &timeout, sizeof(timeout));

    if (sceNetConnect(socket, (SceNetSockaddr *)&address, sizeof(address)) < 0) {
        sceNetSocketClose(socket);
        return -1;
    }
    return socket;
}

bool Session::readLine(std::string &line) {
    char buffer[512];

    while (true) {
        size_t newline = pending.find('\n');
        if (newline != std::string::npos) {
            line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            while (!line.empty() && (line[line.size() - 1] == '\r'))
                line.erase(line.size() - 1);
            return true;
        }

        int got = sceNetRecv(control, buffer, sizeof(buffer), 0);
        if (got <= 0) return false;
        pending.append(buffer, (size_t)got);
    }
}

bool Session::sendLine(const std::string &line) {
    const std::string out = line + "\r\n";
    size_t sent = 0;

    while (sent < out.size()) {
        int wrote = sceNetSend(control, out.c_str() + sent,
                               (unsigned int)(out.size() - sent), 0);
        if (wrote <= 0) return false;
        sent += (size_t)wrote;
    }
    return true;
}

int Session::command(const char *format, ...) {
    char line[FTP_MAX_PATH + 64];
    va_list args;

    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if (control < 0) return -1;
    if (format[0] != '\0' && !sendLine(line)) return -1;

    /* A reply can run to several lines; only the last one answers. */
    while (true) {
        if (!readLine(reply)) return -1;
        if (ftp_reply_is_final(reply.c_str())) break;
    }
    return ftp_reply_code(reply.c_str());
}

bool Session::open(const SaveSync::Server &server, std::string &error) {
    host = server.host;
    control = connectTo(server.host.c_str(), server.port);
    if (control < 0) {
        error = "cannot reach the server";
        return false;
    }

    /* The greeting, which arrives unasked for: an empty command reads it. */
    if (!ftp_reply_ok(command(""))) {
        error = "the server refused the connection";
        return false;
    }

    int code = command("USER %s", server.user.empty() ? "anonymous"
                                                      : server.user.c_str());
    if (code == 331)
        code = command("PASS %s", server.password.c_str());
    if (!ftp_reply_ok(code) || code >= 400) {
        error = "the server refused the login";
        return false;
    }

    /* Binary: a save read as text on a server that translates newlines is
     * a save that no longer loads. */
    if (!ftp_reply_ok(command("TYPE I"))) {
        error = "the server refused binary mode";
        return false;
    }
    return true;
}

void Session::close() {
    closeData();
    if (control >= 0) {
        command("QUIT");
        sceNetSocketClose(control);
        control = -1;
    }
}

bool Session::openData() {
    char address[64];
    int port = 0;

    closeData();

    int code = command("PASV");
    if (code != 227 ||
        !ftp_parse_pasv(reply.c_str(), address, sizeof(address), &port)) {
        /* Some servers only offer the extended form. */
        code = command("EPSV");
        if (code != 229 || !ftp_parse_epsv(reply.c_str(), &port)) return false;
        /* An extended reply names no address: it is the same server. */
        snprintf(address, sizeof(address), "%s", host.c_str());
    }

    data = connectTo(address, port);
    return data >= 0;
}

void Session::closeData() {
    if (data >= 0) {
        sceNetSocketClose(data);
        data = -1;
    }
}

bool Session::storeFile(const std::string &local, const std::string &remote) {
    SceUID fd = sceIoOpen(local.c_str(), SCE_O_RDONLY, 0777);
    if (fd < 0) return false;

    if (!openData()) {
        sceIoClose(fd);
        return false;
    }
    if (!ftp_reply_ok(command("STOR %s", remote.c_str()))) {
        sceIoClose(fd);
        closeData();
        return false;
    }

    char buffer[TRANSFER_CHUNK];
    bool ok = true;
    int got;
    while ((got = sceIoRead(fd, buffer, sizeof(buffer))) > 0) {
        int sent = 0;
        while (sent < got) {
            int wrote = sceNetSend(data, buffer + sent,
                                   (unsigned int)(got - sent), 0);
            if (wrote <= 0) { ok = false; break; }
            sent += wrote;
        }
        if (!ok) break;
    }
    sceIoClose(fd);
    closeData();

    /* The server says whether it kept what it was sent. */
    return ok && ftp_reply_ok(command(""));
}

bool Session::retrieveFile(const std::string &remote, const std::string &local) {
    if (!openData()) return false;
    if (!ftp_reply_ok(command("RETR %s", remote.c_str()))) {
        closeData();
        return false;
    }

    SceUID fd = sceIoOpen(local.c_str(),
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
        closeData();
        command("");
        return false;
    }

    char buffer[TRANSFER_CHUNK];
    bool ok = true;
    int got;
    while ((got = sceNetRecv(data, buffer, sizeof(buffer), 0)) > 0) {
        int written = 0;
        while (written < got) {
            int wrote = sceIoWrite(fd, buffer + written, got - written);
            if (wrote <= 0) { ok = false; break; }
            written += wrote;
        }
        if (!ok) break;
    }
    sceIoClose(fd);
    closeData();

    if (!ok || !ftp_reply_ok(command(""))) {
        /* Half a save is worse than none: it loads and then does not. */
        sceIoRemove(local.c_str());
        return false;
    }
    return true;
}

bool Session::listNames(const std::string &remote,
                        std::vector<std::string> &names) {
    if (!openData()) return false;
    if (!ftp_reply_ok(command("LIST %s", remote.c_str()))) {
        closeData();
        return false;
    }

    std::string listing;
    char buffer[TRANSFER_CHUNK];
    int got;
    while ((got = sceNetRecv(data, buffer, sizeof(buffer), 0)) > 0)
        listing.append(buffer, (size_t)got);
    closeData();

    if (!ftp_reply_ok(command(""))) return false;

    size_t start = 0;
    while (start < listing.size()) {
        size_t end = listing.find('\n', start);
        if (end == std::string::npos) end = listing.size();

        const std::string line = listing.substr(start, end - start);
        start = end + 1;

        char name[256];
        if (ftp_parse_list_line(line.c_str(), name, sizeof(name)) &&
            ftp_is_save_name(name))
            names.push_back(name);
    }
    return true;
}

/* The folder name a game's saves live under on the server: the game's own
 * folder name, which is what the player recognises. */
std::string gameFolderName(const std::string &path) {
    std::string name = path;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.empty()) name = "game";
    return name;
}

} /* namespace */

SaveSync::Server SaveSync::settings() {
    Server server;
    server.port = FTP_DEFAULT_PORT;
    server.path = "/onsemu-saves";

    dictionary *ini = iniparser_load(CONFIG_FILE);
    if (ini == NULL) return server;

    server.host     = iniparser_getstring(ini, "FTP:host", "");
    server.port     = iniparser_getint(ini, "FTP:port", FTP_DEFAULT_PORT);
    server.user     = iniparser_getstring(ini, "FTP:user", "anonymous");
    server.password = iniparser_getstring(ini, "FTP:password", "ons@vita");
    server.path     = iniparser_getstring(ini, "FTP:path", "/onsemu-saves");
    iniparser_freedict(ini);
    return server;
}

void SaveSync::saveSettings(const Server &server) {
    dictionary *ini = iniparser_load(CONFIG_FILE);
    if (ini == NULL) return;

    char port[16];
    snprintf(port, sizeof(port), "%d", server.port);

    iniparser_set(ini, "FTP", NULL);
    iniparser_set(ini, "FTP:host", server.host.c_str());
    iniparser_set(ini, "FTP:port", port);
    iniparser_set(ini, "FTP:user", server.user.c_str());
    iniparser_set(ini, "FTP:password", server.password.c_str());
    iniparser_set(ini, "FTP:path", server.path.c_str());

    FILE *file = fopen(CONFIG_FILE, "w");
    if (file) {
        iniparser_dump_ini(ini, file);
        fclose(file);
    }
    iniparser_freedict(ini);
}

bool SaveSync::configured() {
    return !settings().host.empty();
}

/* Both directions are the same walk over the installed games, so they
 * share everything but what happens to each file. */
static SaveSync::Result run(bool sending) {
    SaveSync::Result result;
    result.ok    = false;
    result.games = 0;
    result.files = 0;

    const SaveSync::Server server = SaveSync::settings();
    if (server.host.empty()) {
        result.message = "no server set";
        return result;
    }
    if (!startNetwork() || !networkIsUp()) {
        result.message = "this console is not on a network";
        return result;
    }

    Session session;
    if (!session.open(server, result.message)) return result;

    session.makeDirectory(server.path);

    for (size_t i = 0; i < rom_list_all.size(); i++) {
        if (rom_list_all[i].is_zip || rom_list_all[i].is_partial) continue;

        const std::string local  = rom_list_all[i].path;
        const std::string folder = gameFolderName(local);

        char remote_dir[FTP_MAX_PATH];
        if (!ftp_join_path(server.path.c_str(), folder.c_str(), NULL,
                           remote_dir, sizeof(remote_dir)))
            continue;

        int copied = 0;

        if (sending) {
            SceUID dfd = sceIoDopen(local.c_str());
            if (dfd < 0) continue;

            int res = 0;
            do {
                SceIoDirent entry;
                memset(&entry, 0, sizeof(entry));
                res = sceIoDread(dfd, &entry);
                if (res <= 0) break;
                if (SCE_S_ISDIR(entry.d_stat.st_mode)) continue;
                if (!ftp_is_save_name(entry.d_name)) continue;

                char remote[FTP_MAX_PATH];
                if (!ftp_join_path(server.path.c_str(), folder.c_str(),
                                   entry.d_name, remote, sizeof(remote)))
                    continue;

                /* Made only once there is something to put in it, so a
                 * game never played leaves no folder on the server. */
                if (copied == 0) session.makeDirectory(remote_dir);

                if (session.storeFile(local + "/" + entry.d_name, remote))
                    copied++;
            } while (res > 0);
            sceIoClose(dfd);
        }
        else {
            std::vector<std::string> names;
            if (!session.listNames(remote_dir, names)) continue;

            for (size_t k = 0; k < names.size(); k++) {
                char remote[FTP_MAX_PATH];
                if (!ftp_join_path(server.path.c_str(), folder.c_str(),
                                   names[k].c_str(), remote, sizeof(remote)))
                    continue;
                if (session.retrieveFile(remote, local + "/" + names[k]))
                    copied++;
            }
        }

        if (copied > 0) {
            result.games++;
            result.files += copied;
        }
    }

    session.close();
    result.ok = true;
    return result;
}

SaveSync::Result SaveSync::upload()   { return run(true); }
SaveSync::Result SaveSync::download() { return run(false); }
