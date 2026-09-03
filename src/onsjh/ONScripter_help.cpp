/* -*- C++ -*-
 *
 *  ONScripter_help.cpp -- the in-game controls overlay
 *
 *  Which button advances the text is not obvious on a Vita: the engine
 *  follows the japanese convention, where circle confirms and cross is the
 *  fast-forward modifier, and that is the opposite of every system dialog on
 *  a western console.  The launcher's help screen answers this, but only
 *  before the game starts, which is the one moment nobody needs it.
 *
 *  So the same list is available during the game.  It draws over the screen
 *  and touches nothing else: no engine state, no script variables, no
 *  rendering the game will later draw from.  The screen is put back from the
 *  accumulation surface on the way out, exactly as the video player does, so
 *  the scene continues from where it was.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "ONScripter.h"
#include "Utils.h"

namespace {

/* Kept in one place, in the order the buttons sit on the console. */
const char *const kLines[] = {
    "Controls",
    "",
    "Circle          advance, confirm",
    "Cross           hold to fast-forward",
    "Square          auto mode",
    "Triangle        menu, and leaves the backlog",
    "L               skip to the end of the page",
    "R               start or stop skipping",
    "Start           click through a wait",
    "Select          tap for text speed, hold for this",
    "Left, Right     backlog",
    "Up, Down        move between choices",
    "Left stick      same as the d-pad",
    "",
    "Any button closes this."
};
const int kNumLines = (int)(sizeof(kLines) / sizeof(kLines[0]));

const int kFontSize = 20;
const int kLineGap  = 26;
const int kPadding  = 24;

}  /* namespace */

void ONScripter::showControlsOverlay()
{
    if (renderer == NULL || font_file == NULL) return;
    if (!TTF_WasInit()) return;

    /* Our own font handle, so the engine's font cache is left alone -- the
     * size and hinting here are not the ones the script asked for. */
    TTF_Font *font = TTF_OpenFont(font_file, kFontSize);
    if (font == NULL){
        utils::printError("controls: no font (%s): %s\n", font_file, TTF_GetError());
        return;
    }

    SDL_Color fg = { 255, 255, 255, 255 };

    /* Wide enough for the longest line, tall enough for all of them. */
    int text_w = 0;
    for (int i = 0; i < kNumLines; i++){
        int w = 0, h = 0;
        if (kLines[i][0] != '\0' && TTF_SizeUTF8(font, kLines[i], &w, &h) == 0 && w > text_w)
            text_w = w;
    }
    int panel_w = text_w + kPadding * 2;
    int panel_h = kNumLines * kLineGap + kPadding * 2;
    if (panel_w > 960) panel_w = 960;
    if (panel_h > 544) panel_h = 544;

    SDL_Surface *panel = SDL_CreateRGBSurface(0, panel_w, panel_h, 32,
                                              0x00FF0000, 0x0000FF00,
                                              0x000000FF, 0xFF000000);
    if (panel == NULL){
        TTF_CloseFont(font);
        return;
    }
    SDL_FillRect(panel, NULL, SDL_MapRGBA(panel->format, 0, 0, 0, 220));

    for (int i = 0; i < kNumLines; i++){
        if (kLines[i][0] == '\0') continue;
        SDL_Surface *line = TTF_RenderUTF8_Blended(font, kLines[i], fg);
        if (line == NULL) continue;
        SDL_Rect dst = { kPadding, kPadding + i * kLineGap, line->w, line->h };
        SDL_SetSurfaceBlendMode(line, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(line, NULL, panel, &dst);
        SDL_FreeSurface(line);
    }

    SDL_Texture *panel_tex = SDL_CreateTextureFromSurface(renderer, panel);
    SDL_FreeSurface(panel);
    TTF_CloseFont(font);
    if (panel_tex == NULL) return;
    SDL_SetTextureBlendMode(panel_tex, SDL_BLENDMODE_BLEND);

    /* The game is not drawn to the whole window -- flushDirect letterboxes
     * it into the device rectangle unless the player asked for full screen.
     * Copying it to NULL instead would stretch the image for as long as the
     * list was up. */
    SDL_Rect game_rect = { 0, 0, 960, 544 };
    if (!fullscreen_mode)
        game_rect = (SDL_Rect){ screen_device_shiftx, screen_device_shifty,
                                screen_device_width, screen_device_height };

    if (panel_w > game_rect.w) panel_w = game_rect.w;
    if (panel_h > game_rect.h) panel_h = game_rect.h;

    SDL_Rect dst_rect = { game_rect.x + (game_rect.w - panel_w) / 2,
                          game_rect.y + (game_rect.h - panel_h) / 2,
                          panel_w, panel_h };

    /* The game stays exactly as it was underneath. */
    SDL_RenderClear(renderer);
    if (texture) SDL_RenderCopy(renderer, texture, NULL, &game_rect);
    SDL_RenderCopy(renderer, panel_tex, NULL, &dst_rect);
    SDL_RenderPresent(renderer);

    /* Wait for a press.  Every event is consumed here, so nothing that
     * happens while the list is up reaches the script -- which is the point:
     * reading the controls should not advance the text. */
    bool waiting = true;
    while (waiting){
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_KEYDOWN ||
                event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_FINGERDOWN)
                waiting = false;
            else if (event.type == SDL_QUIT){
                waiting = false;
                SDL_PushEvent(&event);   /* the main loop still needs this one */
            }
        }
        SDL_Delay(16);
    }

    /* Drain the release of whatever closed it, so the game does not see a
     * stray button up for a press it never got. */
    SDL_Delay(80);
    SDL_Event drain;
    while (SDL_PollEvent(&drain)){
        if (drain.type == SDL_QUIT) SDL_PushEvent(&drain);
    }

    SDL_DestroyTexture(panel_tex);

    /* Hand the screen back to the engine's own repaint, so the image
     * returns at the size and position the engine puts it at -- and so the
     * texture it draws from is the one it created, still streaming. */
    SDL_Rect whole = screen_rect;
    flushDirect(whole, refreshMode());
}
