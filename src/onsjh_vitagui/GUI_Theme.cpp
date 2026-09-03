/* -*- C++ -*-
 *
 *  GUI_Theme.cpp -- button glyphs and the easing the launcher animates with
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <psp2/ctrl.h>

#include "GUI_Theme.h"
#include "GUI_common.h"

vita2d_texture *th_glyph_circle   = NULL;
vita2d_texture *th_glyph_cross    = NULL;
vita2d_texture *th_glyph_square   = NULL;
vita2d_texture *th_glyph_triangle = NULL;
vita2d_texture *th_glyph_enter    = NULL;
vita2d_texture *th_glyph_cancel   = NULL;
vita2d_texture *th_glyph_l        = NULL;
vita2d_texture *th_glyph_r        = NULL;
vita2d_texture *th_glyph_start    = NULL;
vita2d_texture *th_glyph_select   = NULL;
vita2d_texture *th_glyph_dpad     = NULL;
vita2d_texture *th_glyph_lstick   = NULL;

void th_load_glyphs()
{
    th_glyph_circle   = vita2d_load_PNG_file("app0:btn_circle.png");
    th_glyph_cross    = vita2d_load_PNG_file("app0:btn_cross.png");
    th_glyph_square   = vita2d_load_PNG_file("app0:btn_square.png");
    th_glyph_triangle = vita2d_load_PNG_file("app0:btn_triangle.png");

    /* The named buttons -- L, R, START, SELECT -- are drawn as chips of
     * their letters instead; see th_chip.  The pointers stay so a build
     * that wants to put images back has somewhere to put them. */

    /* Which button confirms is a system setting, and the launcher already
     * follows it for the input.  The glyphs follow the same answer, so the
     * screen never disagrees with the button that works. */
    if (SCE_CTRL_ENTER == SCE_CTRL_CIRCLE){
        th_glyph_enter  = th_glyph_circle;
        th_glyph_cancel = th_glyph_cross;
    }
    else {
        th_glyph_enter  = th_glyph_cross;
        th_glyph_cancel = th_glyph_circle;
    }
}

/* How wide a glyph draws at a given text size.
 *
 * Keyed to height, not width: the shoulder and start/select icons are wide
 * shapes with a word inside them, and forcing those into a square would
 * shrink the word out of legibility.  So every glyph is drawn as tall as the
 * text beside it and as wide as it needs to be. */
int th_glyph_width(vita2d_texture *glyph, int size)
{
    if (glyph == NULL) return 0;
    unsigned int h = vita2d_texture_get_height(glyph);
    unsigned int w = vita2d_texture_get_width(glyph);
    if (h == 0) return 0;
    return (int)((float)w * (float)size / (float)h);
}

void th_glyph(vita2d_texture *glyph, int x, int baseline, int size,
              unsigned int color)
{
    if (glyph == NULL) return;

    unsigned int source_h = vita2d_texture_get_height(glyph);
    if (source_h == 0) return;
    float scale = (float)size / (float)source_h;

    /* Drawn as it is, not tinted: these are two-tone pictures -- a light
     * body with the symbol dark inside it -- rather than shapes in an alpha
     * channel, and multiplying them by the text colour would only muddy
     * them.  The colour argument is kept because the caller passes one for
     * the label beside it. */
    (void)color;

    /* Sat on the text's baseline rather than on the line box, so a glyph
     * and the word beside it look like one thing. */
    vita2d_draw_texture_scale(glyph, (float)x,
                              (float)(baseline - size + size / 6),
                              scale, scale);
}

/* Glyphs are drawn at the height of the letters beside them rather than at
 * the font size, which includes the space above and below them.  At full
 * size a filled button icon towers over the word it belongs to. */
static int glyph_size(int size)
{
    return size * 3 / 4;
}

int th_chip_width(const char *label, int size)
{
    return vita2d_font_text_width(font, size - 4, label) + 12;
}

int th_chip(int x, int baseline, const char *label, int size)
{
    const int text_size = size - 4;
    const int w = th_chip_width(label, size);
    const int h = size;
    const int top = baseline - h + 4;

    th_card(x, top, w, h, TH_SURFACE_HI, TH_SURFACE);
    th_border(x, top, w, h, 1, TH_LINE);
    th_text_center(x, w, baseline, TH_TEXT, text_size, label);

    return w;
}

int th_hint_width(vita2d_texture *glyph, const char *label, int size)
{
    int w = (glyph ? th_glyph_width(glyph, glyph_size(size)) + 5 : 0);
    if (label && label[0]) w += vita2d_font_text_width(font, size, label);
    return w;
}

int th_hint(int x, int baseline, vita2d_texture *glyph, const char *label,
            unsigned int color, int size)
{
    int start = x;

    if (glyph){
        th_glyph(glyph, x, baseline, glyph_size(size), color);
        x += th_glyph_width(glyph, glyph_size(size)) + 5;
    }
    if (label && label[0]){
        vita2d_font_draw_text(font, x, baseline, color, size, label);
        x += vita2d_font_text_width(font, size, label);
    }
    return x - start;
}
