#pragma once

#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Drawing one line of text with SDL_ttf is a five-call dance - render to a
// surface, make a texture from it, query its size, copy it, free both - and
// every step has a failure path. Four examples wrote their own copy of it and
// the copies diverged: two were correct, one skipped the null-texture check,
// and one re-opened the font from disk on every call.
//
// Header-only and free of engine types, so it costs nothing to anyone who does
// not include it and reaches every platform target. The statics are scoped on
// a struct rather than being free functions because the engine has no
// namespace (KNOWN_ISSUES.md #9) and `DrawText` is a name games already use.
//
// Fonts come from AssetStore::GetFont. Nothing here opens or closes one.
struct Text {
  // Pixel size the string would occupy. {0, 0} for a null font or a
  // measurement failure, which is also what an empty string gives.
  static SDL_Point Measure(TTF_Font *font, const std::string &text) {
    SDL_Point size{0, 0};
    if (!font || text.empty()) {
      return size;
    }
    if (TTF_SizeText(font, text.c_str(), &size.x, &size.y) != 0) {
      return SDL_Point{0, 0};
    }
    return size;
  }

  // Draws with the top-left corner at (x, y) and returns the size drawn.
  // {0, 0} means nothing was drawn - a null renderer or font, an empty
  // string, or an SDL failure. Never leaks the intermediate surface or
  // texture, including on the failure paths.
  static SDL_Point Draw(SDL_Renderer *renderer, TTF_Font *font,
                        const std::string &text, int x, int y,
                        SDL_Color color) {
    if (!renderer || !font || text.empty()) {
      return SDL_Point{0, 0};
    }

    SDL_Surface *surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) {
      return SDL_Point{0, 0};
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    const SDL_Point size{surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (!texture) {
      return SDL_Point{0, 0};
    }

    const SDL_Rect destination{x, y, size.x, size.y};
    SDL_RenderCopy(renderer, texture, nullptr, &destination);
    SDL_DestroyTexture(texture);
    return size;
  }

  // Same, horizontally centred on centreX. Replaces the hand-guessed offsets
  // ("windowWidth / 2 - 30") that every example was carrying, which drift the
  // moment the string or the point size changes.
  static SDL_Point DrawCentred(SDL_Renderer *renderer, TTF_Font *font,
                               const std::string &text, int centreX, int y,
                               SDL_Color color) {
    const SDL_Point size = Measure(font, text);
    if (size.x == 0) {
      return size;
    }
    return Draw(renderer, font, text, centreX - size.x / 2, y, color);
  }
};
