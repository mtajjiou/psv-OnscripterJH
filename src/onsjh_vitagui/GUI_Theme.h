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

/* --- button glyphs ----------------------------------------------------
 *
 * The face buttons, drawn rather than written: the font's box-drawing
 * characters read as punctuation next to the words they belong with.  White
 * with the shape in the alpha channel, so they take the colour of whatever
 * they sit in.  See script/make_button_glyphs.py.
 */
extern vita2d_texture *th_glyph_circle;
extern vita2d_texture *th_glyph_cross;
extern vita2d_texture *th_glyph_square;
extern vita2d_texture *th_glyph_triangle;
/* Optional, and NULL when the file is not in the vpk: the launcher writes
 * the name of the button instead.  Drop a png in asset/ under the matching
 * name and it is used with no code change, at whatever resolution it is --
 * the glyphs are scaled from their own size. */
extern vita2d_texture *th_glyph_l;
extern vita2d_texture *th_glyph_r;
extern vita2d_texture *th_glyph_start;
extern vita2d_texture *th_glyph_select;
extern vita2d_texture *th_glyph_dpad;
extern vita2d_texture *th_glyph_lstick;
/* Which of the two confirms is a system setting; these follow it. */
extern vita2d_texture *th_glyph_enter;
extern vita2d_texture *th_glyph_cancel;

void th_load_glyphs();
void th_glyph(vita2d_texture *glyph, int x, int baseline, int size,
              unsigned int color);
/* How wide that glyph will be: they are sized by height and keep their own
 * proportions, so a wide one is wider than it is tall. */
int  th_glyph_width(vita2d_texture *glyph, int size);
/* A glyph and its label, drawn together.  Returns how wide it was. */
int  th_hint(int x, int baseline, vita2d_texture *glyph, const char *label,
             unsigned int color, int size);
int  th_hint_width(vita2d_texture *glyph, const char *label, int size);

/* A button with a name rather than a shape -- L, R, SELECT -- drawn as a
 * bordered chip around its letters.
 *
 * The icon set has pictures for these, but they are pictures of the physical
 * buttons: a shoulder button seen at an angle, an oval with SELECT written
 * in it.  Those read at the size a manual prints them and turn to mush at
 * the height of a line of text, which is what they are sitting in here. */
int  th_chip(int x, int baseline, const char *label, int size);
int  th_chip_width(const char *label, int size);

/* A button: its icon if the vpk carries one, otherwise a chip of the
 * letters.  Returns how wide it was drawn. */
int  th_button(int x, int baseline, vita2d_texture *glyph, const char *label,
               int size);

/* --- motion -----------------------------------------------------------
 *
 * Everything that moves does it the same way: a value chases a target by a
 * fraction of the remaining distance each frame.  It is frame-rate bound,
 * which on a console that draws at a fixed 60Hz is exactly what is wanted,
 * and it needs no clock, no tweens and no state beyond the value itself.
 */
static inline float th_ease(float current, float target, float rate)
{
    float next = current + (target - current) * rate;
    /* Stop rather than approach forever, so a finished animation costs
     * nothing and lands exactly where it was going. */
    if (next > target - 0.35f && next < target + 0.35f) return target;
    return next;
}

/* A colour at a different alpha, for fading something in. */
static inline unsigned int th_alpha(unsigned int color, float amount)
{
    unsigned int a = (unsigned int)((float)((color >> 24) & 0xFF) * amount);
    if (amount <= 0.0f) a = 0;
    return (color & 0x00FFFFFF) | (a << 24);
}

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
