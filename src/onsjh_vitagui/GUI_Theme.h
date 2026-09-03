/* -*- C++ -*-
 *
 *  GUI_Theme.h -- the launcher's visual language
 *
 *  The launcher drew white text and hard white boxes on black: legible, and
 *  from a decade before the console it runs on.  A library of games is
 *  something you pick from by eye, so this puts the covers first and gives
 *  everything else one palette, one spacing rule and one idea of what
 *  "selected" looks like.
 *
 *  Everything here is built from the two primitives vita2d gives us --
 *  rectangles and textures -- so there is nothing to load and nothing that
 *  can fail at runtime.  Depth comes from layering flat colours rather than
 *  from images: a shadow is three translucent rectangles, a border is four
 *  thin ones.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef GUI_THEME_H
#define GUI_THEME_H

#include <vita2d.h>
#include <string.h>
#include <stdio.h>

/* --- palette ----------------------------------------------------------
 *
 * A near-black ground with two lighter surfaces above it, one accent, and
 * text at two weights.  Nothing else: a screen that needs a sixth colour is
 * usually a screen that needs less on it.
 */
#define TH_BG            RGBA8(0x0E, 0x10, 0x16, 0xFF)  /* the page        */
#define TH_SURFACE       RGBA8(0x1A, 0x1D, 0x26, 0xFF)  /* cards, rows     */
#define TH_SURFACE_HI    RGBA8(0x26, 0x2B, 0x38, 0xFF)  /* hovered surface */
#define TH_LINE          RGBA8(0x33, 0x39, 0x48, 0xFF)  /* hairline border */
#define TH_ACCENT        RGBA8(0x4C, 0x8D, 0xFF, 0xFF)  /* selection       */
#define TH_ACCENT_SOFT   RGBA8(0x4C, 0x8D, 0xFF, 0x33)  /* selection wash  */
#define TH_TEXT          RGBA8(0xEC, 0xEF, 0xF4, 0xFF)
#define TH_TEXT_DIM      RGBA8(0x93, 0x9D, 0xB0, 0xFF)
#define TH_TEXT_FAINT    RGBA8(0x5E, 0x67, 0x78, 0xFF)
#define TH_SHADOW_1      RGBA8(0x00, 0x00, 0x00, 0x40)
#define TH_SHADOW_2      RGBA8(0x00, 0x00, 0x00, 0x22)
#define TH_SCRIM         RGBA8(0x00, 0x00, 0x00, 0xB0)  /* behind dialogs  */
#define TH_CAPTION       RGBA8(0x0E, 0x10, 0x16, 0xD8)  /* over cover art  */
#define TH_DANGER        RGBA8(0xE5, 0x6B, 0x6F, 0xFF)

/* --- metrics ---------------------------------------------------------- */
#define TH_GAP           10   /* between cards            */
#define TH_PAD           14   /* inside a card            */
#define TH_RADIUS        3    /* corner "bevel" in pixels */
#define TH_RING          3    /* selection ring thickness */
#define TH_FONT_S        18
#define TH_FONT_M        22
#define TH_FONT_L        28

extern vita2d_font *font;

/* A filled block with its corners nipped off.  vita2d has no rounded
 * rectangle, and four small notches read as a rounded card at this size
 * while costing four more rectangles of the colour behind it. */
static inline void th_card(int x, int y, int w, int h,
                           unsigned int fill, unsigned int behind)
{
    vita2d_draw_rectangle(x, y, w, h, fill);
    const int r = TH_RADIUS;
    vita2d_draw_rectangle(x,             y,             r, r, behind);
    vita2d_draw_rectangle(x + w - r,     y,             r, r, behind);
    vita2d_draw_rectangle(x,             y + h - r,     r, r, behind);
    vita2d_draw_rectangle(x + w - r,     y + h - r,     r, r, behind);
}

/* Two translucent bands under a card.  Not a real shadow, but enough to lift
 * the card off the page. */
static inline void th_shadow(int x, int y, int w, int h)
{
    vita2d_draw_rectangle(x + 2, y + 3, w, h, TH_SHADOW_2);
    vita2d_draw_rectangle(x + 1, y + 2, w, h, TH_SHADOW_1);
}

static inline void th_border(int x, int y, int w, int h, int t, unsigned int c)
{
    vita2d_draw_rectangle(x,         y,         w, t, c);
    vita2d_draw_rectangle(x,         y + h - t, w, t, c);
    vita2d_draw_rectangle(x,         y,         t, h, c);
    vita2d_draw_rectangle(x + w - t, y,         t, h, c);
}

/* What "selected" looks like, everywhere: an accent ring just outside the
 * thing, so the thing itself is never covered up. */
static inline void th_focus(int x, int y, int w, int h)
{
    th_border(x - TH_RING, y - TH_RING,
              w + TH_RING * 2, h + TH_RING * 2, TH_RING, TH_ACCENT);
}

/* Text trimmed to fit a width, with an ellipsis where it was cut.  Used
 * everywhere a name or a path is drawn, because a game called something long
 * should push nothing else off the screen. */
static inline const char *th_fit(const char *text, int size, int max_width)
{
    static char out[192];
    snprintf(out, sizeof(out), "%s", text ? text : "");

    if (vita2d_font_text_width(font, size, out) <= max_width) return out;

    size_t len = strlen(out);
    while (len > 1){
        len--;
        /* Do not cut a utf-8 sequence in half. */
        while (len > 1 && ((unsigned char)out[len] & 0xC0) == 0x80) len--;
        out[len] = '\0';
        char probe[192];
        snprintf(probe, sizeof(probe), "%s...", out);
        if (vita2d_font_text_width(font, size, probe) <= max_width){
            snprintf(out, sizeof(out), "%s...", out);
            return out;
        }
    }
    return out;
}

static inline void th_text(int x, int baseline, unsigned int color,
                           int size, const char *text)
{
    vita2d_font_draw_text(font, x, baseline, color, size, text);
}

static inline void th_text_right(int right, int baseline, unsigned int color,
                                 int size, const char *text)
{
    int w = vita2d_font_text_width(font, size, (char *)text);
    vita2d_font_draw_text(font, right - w, baseline, color, size, text);
}

static inline void th_text_center(int x, int w, int baseline,
                                  unsigned int color, int size, const char *text)
{
    int tw = vita2d_font_text_width(font, size, (char *)text);
    vita2d_font_draw_text(font, x + (w - tw) / 2, baseline, color, size, text);
}

/* Draws a texture filling a box, cropped rather than letterboxed.
 *
 * Covers come in whatever shape the publisher used, and a grid of images
 * floating in their own empty boxes is exactly the look this is replacing.
 * The middle of the image is kept, which for a cover is the part worth
 * seeing. */
static inline void th_cover(vita2d_texture *tex, int tw, int th_,
                            int x, int y, int w, int h)
{
    if (tex == NULL || tw <= 0 || th_ <= 0) return;

    float sx = (float)w / (float)tw;
    float sy = (float)h / (float)th_;
    float scale = sx > sy ? sx : sy;          /* cover, not contain */

    float src_w = (float)w / scale;
    float src_h = (float)h / scale;
    float src_x = ((float)tw - src_w) / 2.0f;
    float src_y = ((float)th_ - src_h) / 2.0f;

    vita2d_draw_texture_part_scale(tex, (float)x, (float)y,
                                   src_x, src_y, src_w, src_h,
                                   scale, scale);
}

#endif
