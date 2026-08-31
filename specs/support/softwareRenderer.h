#pragma once

#include <SDL2/SDL.h>
using namespace storm;

// A real SDL_Renderer with no window, no display and no SDL_Init: SDL's
// software renderer draws straight into an SDL_Surface, so RenderSystem and
// RenderColliderSystem can be driven for real in headless CI and the pixels
// they produce read back afterwards.
//
// The target is ARGB8888, so one pixel reads as 0xAARRGGBB. Compare with
// RgbAt(), which drops the alpha channel — the render systems do not control
// destination alpha and it is not what any of these specs are about.
struct SpecSurfaceTarget {
  // What RgbAt() returns for a pixel nothing has drawn to.
  static constexpr Uint32 kNothing = 0x000000;

  SDL_Surface *surface = nullptr;
  SDL_Renderer *renderer = nullptr;

  SpecSurfaceTarget(int width, int height) {
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                             SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
      return;
    }

    renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == nullptr) {
      return;
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(renderer);
  }

  ~SpecSurfaceTarget() {
    if (renderer != nullptr) {
      SDL_DestroyRenderer(renderer);
    }
    if (surface != nullptr) {
      SDL_FreeSurface(surface);
    }
  }

  // Owns an SDL_Renderer and an SDL_Surface; copying one would free both
  // twice.
  SpecSurfaceTarget(const SpecSurfaceTarget &) = delete;
  SpecSurfaceTarget &operator=(const SpecSurfaceTarget &) = delete;

  bool IsUsable() const { return renderer != nullptr; }

  // SDL queues draw commands and only executes them on a flush, so every read
  // below flushes first — otherwise a spec reads the surface as it was before
  // the system under test drew anything.
  Uint32 RgbAt(int x, int y) const {
    SDL_RenderFlush(renderer);
    const Uint32 *pixels = static_cast<const Uint32 *>(surface->pixels);
    return pixels[y * (surface->pitch / 4) + x] & 0x00FFFFFF;
  }

  // How many pixels are no longer the cleared background.
  int DrawnPixelCount() const {
    SDL_RenderFlush(renderer);
    int drawn = 0;
    for (int y = 0; y < surface->h; ++y) {
      for (int x = 0; x < surface->w; ++x) {
        if (RgbAt(x, y) != kNothing) {
          ++drawn;
        }
      }
    }
    return drawn;
  }
};

// 8x8 opaque white — the only image fixture the render specs need. Load it
// under two asset ids and colour-mod each to tell two sprites apart.
//
// The path is relative because the spec binary is run from the repo root
// (Makefile.debian's run-test target), the same assumption the tilemap and
// XML specs already make.
inline const char *SpecWhiteTexturePath() {
  return "./specs/assets/images/white8.bmp";
}
