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
#include <psp2/gxm.h>

#include "GUI_Theme.h"
#include "GUI_common.h"

/* The two palettes.
 *
 * The light one is not the dark one inverted: a light screen needs its text
 * darker than black-on-white would suggest to avoid glare, its shadows much
 * weaker (a hard shadow on white reads as dirt), and a slightly deeper
 * accent to hold its own against a bright ground.  The names mean the same
 * thing in both, so every drawing site reads correctly either way.
 */
static const ThemePalette kDarkPalette = {
    RGBA8(0x0E, 0x10, 0x16, 0xFF),   /* bg          */
    RGBA8(0x1A, 0x1D, 0x26, 0xFF),   /* surface     */
    RGBA8(0x26, 0x2B, 0x38, 0xFF),   /* surface_hi  */
    RGBA8(0x33, 0x39, 0x48, 0xFF),   /* line        */
    RGBA8(0x4C, 0x8D, 0xFF, 0xFF),   /* accent      */
    RGBA8(0x4C, 0x8D, 0xFF, 0x33),   /* accent_soft */
    RGBA8(0xEC, 0xEF, 0xF4, 0xFF),   /* text        */
    RGBA8(0x93, 0x9D, 0xB0, 0xFF),   /* text_dim    */
    RGBA8(0x5E, 0x67, 0x78, 0xFF),   /* text_faint  */
    RGBA8(0x00, 0x00, 0x00, 0x40),   /* shadow_1    */
    RGBA8(0x00, 0x00, 0x00, 0x22),   /* shadow_2    */
    RGBA8(0x00, 0x00, 0x00, 0xB0),   /* scrim       */
    RGBA8(0x0E, 0x10, 0x16, 0xD8),   /* caption     */
    RGBA8(0xE5, 0x6B, 0x6F, 0xFF)    /* danger      */
};

static const ThemePalette kLightPalette = {
    RGBA8(0xF4, 0xF5, 0xF8, 0xFF),   /* bg          */
    RGBA8(0xFF, 0xFF, 0xFF, 0xFF),   /* surface     */
    RGBA8(0xE8, 0xEB, 0xF2, 0xFF),   /* surface_hi  */
    RGBA8(0xD2, 0xD7, 0xE1, 0xFF),   /* line        */
    RGBA8(0x1F, 0x6F, 0xE5, 0xFF),   /* accent      */
    RGBA8(0x1F, 0x6F, 0xE5, 0x28),   /* accent_soft */
    RGBA8(0x14, 0x17, 0x1F, 0xFF),   /* text        */
    RGBA8(0x53, 0x5C, 0x6D, 0xFF),   /* text_dim    */
    RGBA8(0x8A, 0x93, 0xA3, 0xFF),   /* text_faint  */
    RGBA8(0x1B, 0x20, 0x2C, 0x1C),   /* shadow_1    */
    RGBA8(0x1B, 0x20, 0x2C, 0x0E),   /* shadow_2    */
    RGBA8(0x22, 0x26, 0x30, 0x80),   /* scrim       */
    RGBA8(0xFF, 0xFF, 0xFF, 0xD8),   /* caption     */
    RGBA8(0xC0, 0x39, 0x3D, 0xFF)    /* danger      */
};

ThemePalette th_pal = kDarkPalette;
static ThemeMode th_mode = TH_MODE_DARK;

void th_set_theme(ThemeMode mode)
{
    th_mode = (mode == TH_MODE_LIGHT) ? TH_MODE_LIGHT : TH_MODE_DARK;
    th_pal  = (th_mode == TH_MODE_LIGHT) ? kLightPalette : kDarkPalette;

    /* The colour the frame starts as, which is set once at startup and so
     * would otherwise keep the palette the launcher booted with. */
    vita2d_set_clear_color(th_pal.bg);
}

ThemeMode th_get_theme()
{
    return th_mode;
}

const char *th_theme_name(ThemeMode mode)
{
    return (mode == TH_MODE_LIGHT) ? "light" : "dark";
}

ThemeMode th_theme_from_name(const char *name)
{
    return (name && name[0] == 'l') ? TH_MODE_LIGHT : TH_MODE_DARK;
}

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

/* Loads one glyph, smoothly.
 *
 * vita2d leaves a new texture point-sampled, and these are drawn at a
 * fraction of the size they are stored at, so every drawn pixel would be
 * one texel picked out of several and the edges would crawl.  Asking for
 * linear sampling is the difference between an icon and a chewed icon. */
static vita2d_texture *load_glyph(const char *path)
{
    vita2d_texture *tex = vita2d_load_PNG_file(path);
    if (tex)
        vita2d_texture_set_filters(tex, SCE_GXM_TEXTURE_FILTER_LINEAR,
                                   SCE_GXM_TEXTURE_FILTER_LINEAR);
    return tex;
}

void th_load_glyphs()
{
    th_glyph_circle   = load_glyph("app0:btn_circle.png");
    th_glyph_cross    = load_glyph("app0:btn_cross.png");
    th_glyph_square   = load_glyph("app0:btn_square.png");
    th_glyph_triangle = load_glyph("app0:btn_triangle.png");

    th_glyph_l        = load_glyph("app0:btn_l.png");
    th_glyph_r        = load_glyph("app0:btn_r.png");
    th_glyph_start    = load_glyph("app0:btn_start.png");
    th_glyph_select   = load_glyph("app0:btn_select.png");
    th_glyph_dpad     = load_glyph("app0:btn_dpad.png");
    th_glyph_lstick   = load_glyph("app0:btn_lstick.png");

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
    return size * 7 / 8;
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

int th_button(int x, int baseline, vita2d_texture *glyph, const char *label,
              int size)
{
    if (glyph){
        const int h = glyph_size(size);
        th_glyph(glyph, x, baseline, h, TH_TEXT);
        return th_glyph_width(glyph, h);
    }
    if (label) return th_chip(x, baseline, label, size);
    return 0;
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
