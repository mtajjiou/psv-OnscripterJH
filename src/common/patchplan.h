/* -*- C -*-
 *
 *  patchplan.h -- deciding what an archive that is not a game should do
 *
 *  Translation patches, voice packs and fan mods ship as archives that
 *  overlay a game that is already installed: a handful of files that
 *  replace the ones with the same names.  The installer refuses those --
 *  no script inside means "no game in here", and a destination that
 *  already exists means "already installed" -- which is correct for a game
 *  archive and useless for a patch.
 *
 *  These are the decisions made before anything is written: is this a
 *  patch at all, which folder inside it is the overlay root, which
 *  installed game does it most likely belong to, and what the record of an
 *  applied patch looks like so it can be taken off again.  All of it is
 *  pure functions of names and an open archive, so it is checked on a host
 *  rather than only by copying a patch onto a console.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __PATCHPLAN_H__
#define __PATCHPLAN_H__

#include <stddef.h>
#include "zipreader.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Folder inside a game holding the record of every patch applied to it,
 * and the files those patches replaced. */
#define PATCH_FOLDER ".mods"
/* Suffix of one patch's record file, inside PATCH_FOLDER. */
#define PATCH_RECORD_SUFFIX ".mod"

/* What an archive is, once it has been opened.
 *
 * A game carries a script; a patch does not.  An empty archive is neither
 * and is worth saying so about, since "this patch changes nothing" and
 * "this patch failed" look the same otherwise. */
enum patch_kind {
    PATCH_KIND_GAME = 0,    /* has a script: install it as a game */
    PATCH_KIND_PATCH,       /* no script: overlay it onto a game */
    PATCH_KIND_EMPTY        /* no files at all */
};

int patch_archive_kind(const zip_reader *z);

/* Directory inside the archive whose contents overlay the game folder.
 *
 * A patch is usually wrapped in one folder named after itself, and that
 * folder is not meant to appear inside the game -- its contents are.  When
 * every entry shares one top-level directory it is stripped; otherwise the
 * overlay root is the archive root.  Copies the root (no trailing '/',
 * empty for the archive root) into out and returns 1, or returns 0 when
 * the archive has nothing in it. */
int patch_overlay_root(const zip_reader *z, char *out, size_t n);

/* How well a patch's name matches an installed game's, 0 (unrelated) to
 * 100 (the same name).  Case, spaces, punctuation and the words patches
 * habitually add ("english", "patch", "v2", ...) are ignored, so
 * "Higurashi English Patch v2.zip" scores high against "higurashi".
 *
 * Used only to put the likely game at the top of the list the player
 * chooses from: a wrong guess costs a scroll, never a wrong write. */
int patch_name_match(const char *patch_name, const char *game_name);

/* How sure the launcher is that a patch belongs to a game, 0 (looks wrong)
 * to 100 (certain), from two pieces of evidence:
 *
 *   name_score      what patch_name_match() made of the two names
 *   files_total     files the patch would write
 *   files_matching  how many of those the game already has
 *
 * The second is the stronger signal and the one a name cannot fake: a
 * translation patch replaces the game's own files, so most of what it
 * carries is already there.  It is not proof on its own -- a voice pack
 * that only adds new archives legitimately overlaps nothing -- so a patch
 * whose files are all new is believed on its name alone, and less than it
 * would be otherwise.
 *
 * Below PATCH_CONFIDENCE_SURE the launcher warns before applying rather
 * than refusing: both halves of this are guesses, and a player who knows
 * what they downloaded should not be argued with. */
#define PATCH_CONFIDENCE_SURE 50

int patch_confidence(int name_score, int files_total, int files_matching);

/* One line of a patch record.  'R' -- this file replaced one that was
 * there, whose original is in the backup folder.  'N' -- this file was new,
 * so removing the patch deletes it. */
#define PATCH_LINE_REPLACED 'R'
#define PATCH_LINE_NEW      'N'

/* Parse one record line into kind and path.  Returns 1 on success.
 * Blank lines and lines with an unknown kind return 0. */
int patch_parse_line(const char *line, char *kind, char *path, size_t n);

/* Record file name for a patch installed from this archive path, e.g.
 * "english_patch.mod".  Reduced to characters a file name can hold. */
int patch_record_name(const char *zip_path, char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __PATCHPLAN_H__ */
