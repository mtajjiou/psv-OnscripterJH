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

void th_load_glyphs()
{
    th_glyph_circle   = vita2d_load_PNG_file("app0:btn_circle.png");
    th_glyph_cross    = vita2d_load_PNG_file("app0:btn_cross.png");
    th_glyph_square   = vita2d_load_PNG_file("app0:btn_square.png");
    th_glyph_triangle = vita2d_load_PNG_file("app0:btn_triangle.png");

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

void th_glyph(vita2d_texture *glyph, int x, int baseline, int size,
              unsigned int color)
{
    if (glyph == NULL) return;

    float scale = (float)size / (float)TH_GLYPH_SOURCE;
    /* Sat on the text's baseline rather than on the line box, so a glyph
     * and the word beside it look like one thing. */
    vita2d_draw_texture_tint_scale(glyph, (float)x,
                                   (float)(baseline - size + size / 6),
                                   scale, scale, color);
}

int th_hint_width(vita2d_texture *glyph, const char *label, int size)
{
    int w = (glyph ? size + 5 : 0);
    if (label && label[0]) w += vita2d_font_text_width(font, size, label);
    return w;
}

int th_hint(int x, int baseline, vita2d_texture *glyph, const char *label,
            unsigned int color, int size)
{
    int start = x;

    if (glyph){
        th_glyph(glyph, x, baseline, size, color);
        x += size + 5;
    }
    if (label && label[0]){
        vita2d_font_draw_text(font, x, baseline, color, size, label);
        x += vita2d_font_text_width(font, size, label);
    }
    return x - start;
}
