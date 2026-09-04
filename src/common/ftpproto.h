/* -*- C -*-
 *
 *  ftpproto.h -- the reading half of an FTP client
 *
 *  Saves live on the card and nowhere else: a card that dies, or a console
 *  that is reset, takes a hundred hours of reading with it.  The launcher
 *  can copy them to a server on the same network -- a NAS, a desktop, a
 *  router with a disk in it -- which is what most people already have and
 *  what FTP is still the common denominator for.
 *
 *  What lives here is the part with no socket in it: reading a reply,
 *  telling a continuation line from the last one, taking an address out of
 *  a PASV reply, and building a remote path.  Those are where an FTP
 *  client goes wrong quietly -- a mis-parsed PASV connects to the wrong
 *  port and hangs, a multi-line greeting read as one reply leaves every
 *  later reply off by one -- and none of them need a network to check.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __FTPPROTO_H__
#define __FTPPROTO_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FTP_DEFAULT_PORT 21
#define FTP_MAX_PATH     512

/* The three-digit code a reply line starts with, or -1 when the line is
 * not a reply at all. */
int ftp_reply_code(const char *line);

/* A reply can run to several lines: "220-hello" continues, "220 hello"
 * ends.  Returns 1 for the last line of a reply. */
int ftp_reply_is_final(const char *line);

/* Whether a reply code means the command worked (2xx, and 1xx which means
 * "working on it"), or asks for more (3xx), or failed (4xx, 5xx). */
int ftp_reply_ok(int code);

/* Address out of "227 Entering Passive Mode (10,0,0,5,196,24)".  The host
 * is written into host as dotted quad and the port is composed from the
 * last two numbers.  Returns 1 on success. */
int ftp_parse_pasv(const char *line, char *host, size_t n, int *port);

/* Port out of "229 Entering Extended Passive Mode (|||50000|)", which is
 * what a server offers over IPv6 and some offer regardless. */
int ftp_parse_epsv(const char *line, int *port);

/* Size out of "213 4096", for showing progress and for telling a save
 * that changed from one that did not.  Returns 1 on success. */
int ftp_parse_size(const char *line, long *size);

/* Join a base directory, a game folder and a file into a remote path.
 * Slashes are not doubled, a missing base becomes "/", and a name that
 * tries to climb out of the base ("..") is refused -- the path is built
 * from a folder name on the card, and a game folder can be called
 * anything.  Returns 1 on success. */
int ftp_join_path(const char *base, const char *game, const char *file,
                  char *out, size_t n);

/* The names the engine keeps a game's progress in: the numbered slots and
 * the three files beside them.  The same rule as the local backup uses, so
 * "my saves" means one thing wherever they are being copied to. */
int ftp_is_save_name(const char *name);

/* The file name in one line of a LIST reply, whichever of the two layouts
 * the server speaks (unix "-rw-r--r-- 1 user group 4096 Jan 1 00:00 name"
 * or DOS "01-01-24 12:00AM 4096 name").  Returns 1 when a name was found;
 * directory entries return 0, since a save is never one. */
int ftp_parse_list_line(const char *line, char *name, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __FTPPROTO_H__ */
