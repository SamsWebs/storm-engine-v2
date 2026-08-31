#pragma once

#include <SDL2/SDL.h>
#include <stormengine2/assetStore.h>

#include <cstdio>
#include <string>

using namespace storm;

// HUD, menu and banner drawing.
//
// Deliberately not ECS work, for the same reason as examples/shooter: component
// storage is a dense vector indexed by entity id, so an entity per glyph would
// grow the pools every frame and never give the ids back. These are
// immediate-mode SDL_RenderCopy calls made in render() after the systems have
// drawn the world.
namespace ui {

// Must match DW/DH in assets/gen-ui.sh, which renders each digit into its own
// fixed-size cell precisely so this arithmetic holds. Rendering the digits as
// one string would space them by the font's own advance widths -- '1' is much
// narrower than '0' -- and the pitch would drift across a multi-digit number.
constexpr int DIGIT_W = 24;
constexpr int DIGIT_H = 32;

inline void DrawTexture(SDL_Renderer *r, SDL_Texture *tex, int x, int y,
                        float scale = 1.0f) {
  if (!tex) {
    return;
  }
  int w = 0, h = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  SDL_Rect dst{x, y, static_cast<int>(w * scale), static_cast<int>(h * scale)};
  SDL_RenderCopy(r, tex, nullptr, &dst);
}

inline void DrawTextureCentred(SDL_Renderer *r, SDL_Texture *tex, int centreX,
                               int y, float scale = 1.0f) {
  if (!tex) {
    return;
  }
  int w = 0, h = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  DrawTexture(r, tex, centreX - static_cast<int>(w * scale) / 2, y, scale);
}

// Draws left-to-right and returns the width used, so a caller can lay a label
// out after the number without measuring it again.
inline int DrawNumber(SDL_Renderer *r, SDL_Texture *digits, int value, int x,
                      int y, float scale = 1.0f) {
  if (!digits) {
    return 0;
  }
  char buf[16];
  const int n = std::snprintf(buf, sizeof buf, "%d", value < 0 ? 0 : value);
  const int dw = static_cast<int>(DIGIT_W * scale);
  const int dh = static_cast<int>(DIGIT_H * scale);
  for (int i = 0; i < n; ++i) {
    SDL_Rect src{(buf[i] - '0') * DIGIT_W, 0, DIGIT_W, DIGIT_H};
    SDL_Rect dst{x + i * dw, y, dw, dh};
    SDL_RenderCopy(r, digits, &src, &dst);
  }
  return n * dw;
}

inline int NumberWidth(int value, float scale = 1.0f) {
  char buf[16];
  const int n = std::snprintf(buf, sizeof buf, "%d", value < 0 ? 0 : value);
  return n * static_cast<int>(DIGIT_W * scale);
}

// Troop-strength bar, drawn rather than blitted.
//
// Tiny Swords' SmallBar_Base is a three-slice asset -- left cap, middle, right
// cap, separated by gaps inside a 320x64 image -- and SmallBar_Fill is a single
// thin line. Stretching either as one texture produces a broken bar, and a
// nine-slice renderer is a lot of machinery for the one widget this game has.
// Rects cost nothing and size exactly.
inline void DrawBar(SDL_Renderer *r, int x, int y, int w, int h, float frac,
                    SDL_Color colour) {
  if (frac < 0.0f)
    frac = 0.0f;
  if (frac > 1.0f)
    frac = 1.0f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 18, 15, 26, 230);
  SDL_Rect trough{x, y, w, h};
  SDL_RenderFillRect(r, &trough);

  if (frac > 0.0f) {
    const int pad = 3;
    SDL_SetRenderDrawColor(r, colour.r, colour.g, colour.b, 255);
    SDL_Rect fill{x + pad, y + pad, static_cast<int>((w - 2 * pad) * frac),
                  h - 2 * pad};
    SDL_RenderFillRect(r, &fill);
  }

  SDL_SetRenderDrawColor(r, 222, 211, 176, 255);
  SDL_RenderDrawRect(r, &trough);
}

inline SDL_Color BlueColour() { return SDL_Color{90, 140, 230, 255}; }
inline SDL_Color RedColour() { return SDL_Color{210, 80, 80, 255}; }

// Flat translucent panel. Tiny Swords' paper and banner art is fixed-size and
// stretches badly, so anything that has to size itself to its contents is drawn
// as a plain rect instead of a nine-slice.
inline void DrawPanel(SDL_Renderer *r, int x, int y, int w, int h,
                      Uint8 alpha = 190) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 24, 20, 37, alpha);
  SDL_Rect box{x, y, w, h};
  SDL_RenderFillRect(r, &box);
  SDL_SetRenderDrawColor(r, 222, 211, 176, 255);
  SDL_RenderDrawRect(r, &box);
}

} // namespace ui
