/* -*- C++ -*-
 *
 *  WifiUpload.cpp -- see WifiUpload.h
 *
 *  One connection at a time, never blocking.  A frame's worth of work is
 *  "accept whoever is waiting, read what has arrived, write it to the
 *  card"; anything that would wait is left until the next frame, so the
 *  screen keeps its address and its progress bar on it while an archive
 *  comes in over Wi-Fi.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WifiUpload.h"
#include "ZipHandler.h"
#include "GUI_Utils.h"

extern "C" {
#include "httpd.h"
}

namespace {

const int NET_MEMORY = 512 * 1024;

/* How much is read from the socket per call.  One page of work per frame:
 * enough that a big archive still arrives at the speed of the network,
 * small enough that a frame is not spent inside this. */
const int READ_CHUNK = HTTP_FEED_MAX;
const int READS_PER_POLL = 16;

void  *g_net_memory = NULL;
bool   g_net_started = false;

int    g_listen = -1;
int    g_client = -1;

WifiUpload::Status g_status;

/* The request being read, and the upload it turned out to be. */
std::string     g_head;
http_multipart *g_multipart = NULL;
SceUID          g_file = -1;
std::string     g_file_path;
bool            g_file_failed = false;

/* Set when the reply has been sent and the connection should go away. */
bool g_closing = false;

void closeFile(bool keep) {
    if (g_file >= 0) {
        sceIoClose(g_file);
        g_file = -1;
    }
    if (!keep && !g_file_path.empty()) sceIoRemove(g_file_path.c_str());
    g_file_path.clear();
}

void closeClient() {
    if (g_multipart) {
        http_multipart_end(g_multipart);
        g_multipart = NULL;
    }
    /* A connection that went away mid-upload leaves half an archive, which
     * would sit in the drop folder failing to install. */
    closeFile(false);
    if (g_client >= 0) {
        sceNetSocketClose(g_client);
        g_client = -1;
    }
    g_head.clear();
    g_closing = false;
    g_file_failed = false;
    if (g_status.state == WifiUpload::RECEIVING)
        g_status.state = WifiUpload::WAITING;
}

bool startNetwork() {
    if (g_net_started) return true;

    if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0) return false;

    g_net_memory = malloc(NET_MEMORY);
    if (g_net_memory == NULL) return false;

    SceNetInitParam param;
    param.memory = g_net_memory;
    param.size   = NET_MEMORY;
    param.flags  = 0;

    /* Already initialised is not a failure, and is the usual answer once
     * covers have been fetched in the same session. */
    sceNetInit(&param);
    sceNetCtlInit();

    g_net_started = true;
    return true;
}

/* Everything the page says, built fresh each time it is asked for so it
 * shows what is installed now. */
std::string page() {
    std::string html =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>ONS Easy Setup</title><style>"
        "body{font:16px system-ui,sans-serif;margin:0;background:#15171c;color:#e8eaed}"
        "main{max-width:40em;margin:0 auto;padding:2em 1.2em}"
        "h1{font-size:1.3em}form{background:#1e2128;padding:1.2em;border-radius:8px}"
        "input[type=file]{width:100%;margin-bottom:1em}"
        "button{background:#4b8bf4;color:#fff;border:0;padding:.7em 1.4em;"
        "border-radius:6px;font-size:1em}"
        "li{margin:.2em 0}p.hint{color:#9aa0a6;font-size:.9em}"
        "</style></head><body><main>"
        "<h1>ONS Easy Setup</h1>"
        "<form method=\"post\" action=\"/upload\" enctype=\"multipart/form-data\">"
        "<input type=\"file\" name=\"file\" accept=\".zip\" required>"
        "<button type=\"submit\">Send to the console</button>"
        "<p class=\"hint\">A .zip goes to ux0:data/game_zips/. Install it from "
        "the game list when it has arrived.</p></form>";

    if (g_status.received > 0) {
        char line[256];
        snprintf(line, sizeof(line),
            "<p>Received: %s</p>", g_status.file.c_str());
        html += line;
    }

    html += "<h2>Installed</h2><ul>";
    bool any = false;
    for (size_t i = 0; i < rom_list_all.size(); i++) {
        if (rom_list_all[i].is_zip) continue;
        html += "<li>";
        /* The name as the list has it; angle brackets in a game's folder
         * name would otherwise be markup. */
        const std::string name = rom_list_all[i].char_name();
        for (size_t c = 0; c < name.size(); c++) {
            if      (name[c] == '<') html += "&lt;";
            else if (name[c] == '>') html += "&gt;";
            else if (name[c] == '&') html += "&amp;";
            else                     html += name[c];
        }
        html += "</li>";
        any = true;
    }
    if (!any) html += "<li>nothing yet</li>";
    html += "</ul></main></body></html>";

    return html;
}

/* Sends what it can and gives up on the rest: a reply is a few kilobytes
 * into a socket that has just been read from, and a browser that will not
 * take it is a browser that has gone away. */
void sendAll(int socket, const char *data, size_t len) {
    size_t sent = 0;
    int attempts = 0;

    while (sent < len && attempts < 200) {
        int wrote = sceNetSend(socket, data + sent, (unsigned int)(len - sent), 0);
        if (wrote > 0) { sent += (size_t)wrote; attempts = 0; }
        else           { attempts++; }
    }
}

void reply(int socket, int code, const std::string &body,
           const char *content_type) {
    char head[256];
    int len = snprintf(head, sizeof(head),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        http_status_text(code), content_type, (unsigned int)body.size());

    sendAll(socket, head, (size_t)len);
    if (!body.empty()) sendAll(socket, body.c_str(), body.size());
}

/* --- the multipart callbacks ----------------------------------------- */

int onPart(void *, const char *filename) {
    char safe[128];

    closeFile(false);
    g_file_failed = false;

    if (!http_upload_name(filename, safe, sizeof(safe))) {
        /* Not an archive: refused rather than written somewhere it would
         * never be found. */
        g_file_failed = true;
        return -1;
    }

    sceIoMkdir(GAME_ZIP_FOLDER, 0777);
    g_file_path = std::string(GAME_ZIP_FOLDER) + "/" + safe;
    g_file = sceIoOpen(g_file_path.c_str(),
                       SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (g_file < 0) {
        g_file_failed = true;
        g_file_path.clear();
        return -1;
    }

    g_status.file  = safe;
    g_status.bytes = 0;
    g_status.state = WifiUpload::RECEIVING;
    return 0;
}

int onData(void *, const void *data, size_t len) {
    const char *p = (const char *)data;
    size_t left = len;

    if (g_file < 0) return -1;

    while (left > 0) {
        int wrote = sceIoWrite(g_file, p, left);
        if (wrote <= 0) {
            g_file_failed = true;
            return -1;
        }
        p    += wrote;
        left -= (size_t)wrote;
    }
    g_status.bytes += len;
    return 0;
}

/* The head has arrived: decide what this connection is. */
void beginRequest() {
    struct http_request request;

    if (!http_parse_request(g_head.c_str(), &request)) {
        reply(g_client, 400, "bad request", "text/plain");
        g_closing = true;
        return;
    }

    if (request.method == HTTP_METHOD_GET) {
        reply(g_client, 200, page(), "text/html; charset=utf-8");
        g_closing = true;
        return;
    }

    if (request.boundary[0] == '\0') {
        reply(g_client, 400, "expected a file", "text/plain");
        g_closing = true;
        return;
    }

    g_multipart = http_multipart_begin(request.boundary, onPart, onData, NULL);
    if (g_multipart == NULL) {
        reply(g_client, 500, "out of memory", "text/plain");
        g_closing = true;
        return;
    }

    g_status.expected = request.content_length > 0
        ? (uint64_t)request.content_length : 0;
    g_status.bytes = 0;
}

/* An upload that ended, one way or the other. */
void finishUpload() {
    const bool ok = http_multipart_complete(g_multipart) && !g_file_failed &&
                    !g_file_path.empty();

    if (ok) {
        closeFile(true);
        g_status.received++;
        reply(g_client, 200, page(), "text/html; charset=utf-8");
    }
    else {
        closeFile(false);
        reply(g_client, 400,
              g_file_failed ? "that file was refused: send a .zip"
                            : "the upload did not finish",
              "text/plain");
    }
    g_closing = true;
}

} /* namespace */

bool WifiUpload::start() {
    stop();

    g_status = Status();
    g_status.state = FAILED;

    if (!startNetwork()) {
        g_status.message = "the network could not be started";
        return false;
    }

    int state = 0;
    if (sceNetCtlInetGetState(&state) < 0 ||
        state != SCE_NETCTL_STATE_CONNECTED) {
        g_status.message = "this console is not on a network";
        return false;
    }

    g_listen = sceNetSocket("ons_upload", SCE_NET_AF_INET,
                            SCE_NET_SOCK_STREAM, 0);
    if (g_listen < 0) {
        g_status.message = "no socket";
        return false;
    }

    int one = 1;
    sceNetSetsockopt(g_listen, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR,
                     &one, sizeof(one));
    sceNetSetsockopt(g_listen, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO,
                     &one, sizeof(one));

    SceNetSockaddrIn address;
    memset(&address, 0, sizeof(address));
    address.sin_family = SCE_NET_AF_INET;
    address.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
    address.sin_port = sceNetHtons(HTTPD_PORT);

    if (sceNetBind(g_listen, (SceNetSockaddr *)&address, sizeof(address)) < 0 ||
        sceNetListen(g_listen, 2) < 0) {
        sceNetSocketClose(g_listen);
        g_listen = -1;
        g_status.message = "the port is already in use";
        return false;
    }

    /* The address is the whole point of the screen: without it there is
     * nothing to type into a browser. */
    SceNetCtlInfo info;
    memset(&info, 0, sizeof(info));
    char url[64] = "http://?:8080";
    if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) >= 0)
        snprintf(url, sizeof(url), "http://%s:%d", info.ip_address, HTTPD_PORT);

    g_status.address = url;
    g_status.state   = WAITING;
    g_status.message.clear();
    return true;
}

void WifiUpload::stop() {
    closeClient();
    if (g_listen >= 0) {
        sceNetSocketClose(g_listen);
        g_listen = -1;
    }
    g_status.state = STOPPED;
}

void WifiUpload::poll() {
    if (g_listen < 0) return;

    if (g_client < 0) {
        SceNetSockaddrIn from;
        unsigned int from_len = sizeof(from);
        memset(&from, 0, sizeof(from));

        int accepted = sceNetAccept(g_listen, (SceNetSockaddr *)&from, &from_len);
        if (accepted < 0) return;   /* nobody waiting, which is the usual answer */

        g_client = accepted;
        int one = 1;
        sceNetSetsockopt(g_client, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO,
                         &one, sizeof(one));
        g_head.clear();
    }

    char buffer[READ_CHUNK];
    for (int i = 0; i < READS_PER_POLL; i++) {
        if (g_closing) { closeClient(); return; }

        int got = sceNetRecv(g_client, buffer, sizeof(buffer), 0);
        if (got == 0) {
            /* The browser finished sending.  For an upload that means the
             * body is complete or it never will be. */
            if (g_multipart) finishUpload();
            closeClient();
            return;
        }
        if (got < 0) return;   /* nothing more this frame */

        size_t at = 0;
        if (g_multipart == NULL) {
            /* Still reading the head. */
            g_head.append(buffer, (size_t)got);
            long head_len = http_head_length(g_head.c_str(), g_head.size());
            if (head_len < 0) {
                if (g_head.size() > 32 * 1024) {   /* not a request */
                    reply(g_client, 400, "bad request", "text/plain");
                    closeClient();
                    return;
                }
                continue;
            }

            /* Whatever came in after the blank line is body already. */
            const std::string body = g_head.substr((size_t)head_len);
            g_head.erase((size_t)head_len);
            beginRequest();
            if (g_closing) { closeClient(); return; }

            if (!body.empty() &&
                http_multipart_feed(g_multipart, body.data(), body.size()) != 0) {
                finishUpload();
                closeClient();
                return;
            }
            at = (size_t)got;   /* consumed above */
        }

        if (g_multipart && at == 0) {
            if (http_multipart_feed(g_multipart, buffer, (size_t)got) != 0) {
                finishUpload();
                closeClient();
                return;
            }
        }

        if (g_multipart && http_multipart_complete(g_multipart)) {
            finishUpload();
            closeClient();
            return;
        }
    }
}

const WifiUpload::Status &WifiUpload::status() {
    return g_status;
}
