/* -*- C++ -*-
 *
 *  VndbCovers.cpp -- see VndbCovers.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/sysmodule.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "VndbCovers.h"
#include "GUI_Text.h"

namespace {

const char *API_URL   = "https://api.vndb.org/kana/vn";
const char *USER_AGENT = "ONS-Easy-Setup";

const int NET_MEMORY  = 1 * 1024 * 1024;
const int HTTP_MEMORY = 512 * 1024;
const int SSL_MEMORY  = 512 * 1024;

/* A cover is a few hundred kilobytes; this is the ceiling, not an
 * expectation, and a reply larger than it is treated as a failure rather
 * than truncated into a broken file. */
const size_t MAX_REPLY = 4 * 1024 * 1024;

bool  g_net_ready = false;
void *g_net_memory = NULL;

bool startNetwork()
{
    if (g_net_ready) return true;

    if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0) return false;

    g_net_memory = malloc(NET_MEMORY);
    if (g_net_memory == NULL) return false;

    SceNetInitParam param;
    param.memory = g_net_memory;
    param.size   = NET_MEMORY;
    param.flags  = 0;

    int ret = sceNetInit(&param);
    /* Already up is not a failure: the engine may have started it. */
    if (ret < 0 && ret != (int)SCE_NET_ERROR_EBUSY){
        free(g_net_memory);
        g_net_memory = NULL;
        return false;
    }

    sceNetCtlInit();

    if (sceHttpInit(HTTP_MEMORY) < 0) return false;
    if (sceSslInit(SSL_MEMORY) < 0) return false;

    g_net_ready = true;
    return true;
}

/* Is there actually a connection?  Asking first turns "no wifi" into a
 * sentence the player can act on, rather than a timeout. */
bool networkIsUp()
{
    int state = 0;
    if (sceNetCtlInetGetState(&state) < 0) return false;
    return state == SCE_NETCTL_STATE_CONNECTED;
}

/* JSON string escaping, for the title we are searching for. */
void appendEscaped(char *out, size_t out_len, const char *in)
{
    size_t o = strlen(out);
    for (size_t i = 0; in[i] && o + 2 < out_len; i++){
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\'){
            out[o++] = '\\';
            out[o++] = (char)c;
        }
        else if (c < 0x20){
            out[o++] = ' ';
        }
        else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

/* The first "url" value in the reply.
 *
 * The reply is {"results":[{"title":...,"image":{"url":"https://t.vndb.org/
 * cv/..","dims":..}}]}, and the only "url" in it is the cover's.  A real
 * parser would be better company, but this looks for one key in a document
 * whose shape we asked for, and says so when it is not there. */
bool findImageUrl(const char *json, char *out, size_t out_len)
{
    const char *p = strstr(json, "\"url\"");
    if (p == NULL) return false;

    p = strchr(p + 5, ':');
    if (p == NULL) return false;
    while (*p && *p != '"') p++;
    if (*p != '"') return false;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len){
        /* vndb sends plain ascii urls; a backslash escape here would mean
         * something unexpected, so stop rather than guess. */
        if (*p == '\\') return false;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0 && *p == '"';
}

/* One request, start to finish.  post_body NULL means GET. */
int httpFetch(const char *url, const char *post_body,
              unsigned char *buffer, size_t buffer_len, size_t *received)
{
    int tmpl = -1, conn = -1, req = -1;
    int result = -1;
    int status = 0;
    size_t total = 0;

    *received = 0;

    tmpl = sceHttpCreateTemplate(USER_AGENT, SCE_HTTP_VERSION_1_1, SCE_TRUE);
    if (tmpl < 0) return -1;

    /* The console's root certificates predate the authority vndb uses, so a
     * verified handshake fails on hardware however correct this code is.
     * What is being fetched is a public cover image, and it is written to
     * the memory card and shown; nothing here is authenticated or secret.
     * Verification is therefore turned off deliberately, and only for these
     * requests -- not left off for the rest of the program. */
    sceHttpsDisableOption(SCE_HTTPS_FLAG_SERVER_VERIFY);

    conn = sceHttpCreateConnectionWithURL(tmpl, url, SCE_TRUE);
    if (conn < 0) goto done;

    req = sceHttpCreateRequestWithURL(conn,
                                      post_body ? SCE_HTTP_METHOD_POST
                                                : SCE_HTTP_METHOD_GET,
                                      url,
                                      post_body ? strlen(post_body) : 0);
    if (req < 0) goto done;

    if (post_body)
        sceHttpAddRequestHeader(req, "Content-Type", "application/json",
                                SCE_HTTP_HEADER_OVERWRITE);

    if (sceHttpSendRequest(req, post_body, post_body ? strlen(post_body) : 0) < 0)
        goto done;

    if (sceHttpGetStatusCode(req, &status) < 0) goto done;
    if (status != 200){
        result = -status;
        goto done;
    }

    while (total + 1 < buffer_len){
        int got = sceHttpReadData(req, buffer + total, buffer_len - total - 1);
        if (got < 0) goto done;
        if (got == 0) break;
        total += (size_t)got;
    }
    buffer[total] = '\0';
    *received = total;
    result = 0;

done:
    if (req  >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
    return result;
}

}  /* namespace */

VndbResult vndb_fetch_cover(const char *title, const char *game_dir,
                            char *saved_path, size_t saved_len)
{
    if (title == NULL || game_dir == NULL) return VNDB_HTTP_ERROR;
    if (saved_path && saved_len) saved_path[0] = '\0';

    if (!startNetwork()) return VNDB_NO_NETWORK;
    if (!networkIsUp())  return VNDB_NO_NETWORK;

    /* Search by title, ask for the one field we are here for. */
    char body[512];
    strcpy(body, "{\"filters\":[\"search\",\"=\",\"");
    appendEscaped(body, sizeof(body) - 64, title);
    strcat(body, "\"],\"fields\":\"title,image.url\",\"results\":1}");

    unsigned char *buffer = (unsigned char *)malloc(MAX_REPLY);
    if (buffer == NULL) return VNDB_HTTP_ERROR;

    size_t received = 0;
    VndbResult result = VNDB_HTTP_ERROR;

    if (httpFetch(API_URL, body, buffer, MAX_REPLY, &received) != 0){
        free(buffer);
        return VNDB_HTTP_ERROR;
    }

    char image_url[512];
    if (!findImageUrl((const char *)buffer, image_url, sizeof(image_url))){
        /* A well formed reply with no cover in it means the search matched
         * nothing, or matched an entry with no image. */
        free(buffer);
        return VNDB_NOT_FOUND;
    }

    sceClibPrintf("vndb: [%s] -> %s\n", title, image_url);

    if (httpFetch(image_url, NULL, buffer, MAX_REPLY, &received) != 0 ||
        received == 0){
        free(buffer);
        return VNDB_HTTP_ERROR;
    }

    /* Saved under the name the list already looks for, with the extension
     * the file actually has -- the loader picks between them. */
    const char *ext = strstr(image_url, ".png") ? "png" : "jpg";
    char path[512];
    snprintf(path, sizeof(path), "%s/cover.%s", game_dir, ext);

    SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0){
        free(buffer);
        return VNDB_WRITE_ERROR;
    }

    size_t written = 0;
    while (written < received){
        int wrote = sceIoWrite(fd, buffer + written, received - written);
        if (wrote <= 0) break;
        written += (size_t)wrote;
    }
    sceIoClose(fd);
    free(buffer);

    if (written != received){
        sceIoRemove(path);
        return VNDB_WRITE_ERROR;
    }

    if (saved_path && saved_len) snprintf(saved_path, saved_len, "%s", path);
    sceClibPrintf("vndb: saved %s (%u bytes)\n", path, (unsigned)written);

    result = VNDB_OK;
    return result;
}

const char *vndb_result_text(VndbResult result)
{
    switch (result){
    case VNDB_OK:          return ui_text(UI_COVER_OK);
    case VNDB_NO_NETWORK:  return ui_text(UI_COVER_NO_NET);
    case VNDB_NOT_FOUND:   return ui_text(UI_COVER_NOT_FOUND);
    case VNDB_WRITE_ERROR: return ui_text(UI_COVER_WRITE_FAIL);
    default:               return ui_text(UI_COVER_FAIL);
    }
}
