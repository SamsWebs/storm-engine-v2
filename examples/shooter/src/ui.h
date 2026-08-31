#pragma once

#include <SDL2/SDL.h>
#include <stormengine2/assetStore.h>

#include <cstdio>
#include <string>

using namespace storm;

// HUD and menu drawing helpers.
//
// These are deliberately NOT ECS work. Score digits, labels and centred
// messages are immediate-mode SDL_RenderCopy calls made in render() after the
// systems have drawn. Creating an entity per glyph would burn entity ids every
// frame -- component storage is a dense vector indexed by id, so the pools
// would grow forever -- and buy nothing. RenderSystem is for the world.
namespace ui {

// Digit n lives at x = n * 12 in ui_digits.png (see assets/SHEET.md). The
// source sheet's own spacing is uneven ('1' is 5px wide, '0' is 10px); the
// asset was repacked to a uniform pitch so this arithmetic is safe.
constexpr int DIGIT_W = 12;
constexpr int DIGIT_H = 14;

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

// Right-pads nothing and draws left-to-right; returns the width drawn so a
// caller can lay something out after it.
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

// One 32x32 cell of the main sheet, drawn at an arbitrary size. Used for the
// life icons in the HUD, which are the player fighter at half scale.
inline void DrawSheetCell(SDL_Renderer *r, SDL_Texture *sheet, int col, int row,
                          int x, int y, int size) {
  if (!sheet) {
    return;
  }
  SDL_Rect src{col * 32, row * 32, 32, 32};
  SDL_Rect dst{x, y, size, size};
  SDL_RenderCopy(r, sheet, &src, &dst);
}

} // namespace ui
