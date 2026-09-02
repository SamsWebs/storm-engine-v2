#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include <SDL2/SDL.h>
#include <glm/glm.hpp>

// ── A two-layer lighting overlay ────────────────────────────────────────────
//
// Two quarter-resolution RGBA textures, built once and cached, drawn over the
// finished frame with two SDL_RenderCopy calls: a warm key layer whose alpha
// follows a radial falloff, and a cool vignette layer whose alpha is the
// INVERSE of the same falloff. Nothing else.
//
// The pairing is what makes it read as light rather than as a tint. A single
// warm layer looks like a colour filter; adding the cool layer where the warm
// one is absent gives the frame two colour temperatures with a boundary
// between them, and the eye reads that boundary as illumination.
//
// Quarter resolution is why it is cheap, and it costs nothing visually: the
// falloff has no detail to lose, and the upscale's bilinear filter is doing the
// same smoothing the full-resolution version would have computed per pixel.
// Build() is where the per-pixel work happens, once; Draw() is two textured
// quads regardless of the screen size.
//
// NO SHADERS, which is the whole reason this is engine material rather than a
// desktop nicety: it runs on the plain SDL2 renderer, so it works on Switch and
// Android as-is.
//
// Like `text.h` and `collision/shapes.h`, this includes no engine header. A
// game with no `Registry`, no entities and no components can use it.
//
//   LightingOverlay lighting;
//   lighting.Build(renderer, LightingOverlay::Params{});   // once, on enter
//   ...
//   registry.GetSystem<RenderSystem>().Update(renderer, assets, &camera);
//   lighting.Draw(renderer);                               // last, over the frame
//
// Draw it LAST. It is a post-pass over the finished frame; anything drawn
// afterwards is not lit, which is usually what you want for a HUD and never
// what you want for the world.

namespace storm {

class LightingOverlay {
public:
  struct Params {
    // Full-screen size in pixels. The cached textures are a quarter of this on
    // each axis, and are upscaled to it by Draw().
    int width = 0;
    int height = 0;

    // Key-light centre, in the same pixel space as width/height. Left
    // non-finite (the default), it becomes the middle of the screen.
    //
    // NaN is the sentinel rather than a negative value, because a negative
    // centre is legitimate -- a key light anchored off the top-left corner,
    // lighting one corner of the screen -- and no arithmetic here can produce
    // NaN from a real one. A "< 0 means default" test would have quietly
    // ignored (-5, -5) while honouring (-5, 10), which is worse than either.
    glm::vec2 centre{std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::quiet_NaN()};

    // Distance in pixels from `centre` at which the key light has fallen off
    // completely and the vignette is at full strength. 0 means "half the
    // screen's smaller dimension", which suits a centred key light.
    float radius = 0.0f;

    // The two colours. Warm key, cool shadow -- the defaults are a candle-ish
    // amber and a night-ish blue, chosen to read as a temperature difference
    // rather than as two hues.
    SDL_Color key{255, 176, 96, 0};
    SDL_Color vignette{24, 32, 72, 0};

    // Peak opacity of each layer, at the centre and at the edge respectively.
    // These are the knobs to reach for first: the shape rarely wants changing,
    // the strength always does.
    std::uint8_t keyOpacity = 72;
    std::uint8_t vignetteOpacity = 160;

    // Falloff exponent. 1 is linear, higher tightens the lit pool toward the
    // centre, below 1 spreads it. Values at or below zero are treated as 1.
    float falloff = 2.0f;
  };

  LightingOverlay() = default;
  ~LightingOverlay() { Release(); }

  // Owns two SDL_Texture pointers; copying one would destroy both twice. The
  // same treatment the networking types and GameStateMachine got, and for the
  // same reason.
  LightingOverlay(const LightingOverlay &) = delete;
  LightingOverlay &operator=(const LightingOverlay &) = delete;

  // Builds both layers. Safe to call again -- the previous pair is released
  // first -- which is what a resolution change or a relit scene does.
  //
  // Returns false and leaves the overlay unbuilt on a null renderer, a
  // non-positive size, or an SDL allocation failure. It never half-builds: a
  // failure on the second layer releases the first.
  bool Build(SDL_Renderer *renderer, const Params &params) {
    Release();

    if (renderer == nullptr || params.width <= 0 || params.height <= 0)
      return false;

    // Ceiling division, so a 3px screen still gets a 1px texture rather than a
    // zero-sized one SDL would refuse.
    const int lowWidth = std::max(1, (params.width + kScale - 1) / kScale);
    const int lowHeight = std::max(1, (params.height + kScale - 1) / kScale);

    // Non-finite means "use the middle", which also makes every other way a
    // NaN could arrive -- an uninitialised camera, a division by a zero
    // viewport -- degrade to a centred light rather than to a layer of
    // undefined alpha.
    const bool centreGiven =
        std::isfinite(params.centre.x) && std::isfinite(params.centre.y);
    const glm::vec2 centre =
        centreGiven ? params.centre
                    : glm::vec2(static_cast<float>(params.width) * 0.5f,
                                static_cast<float>(params.height) * 0.5f);

    const float radius =
        params.radius > 0.0f
            ? params.radius
            : static_cast<float>(std::min(params.width, params.height)) * 0.5f;

    const float exponent = params.falloff > 0.0f ? params.falloff : 1.0f;

    // Both layers share one falloff evaluation per pixel: the key takes it,
    // the vignette takes its complement. Computing them separately is where
    // the two layers would drift out of step and stop reading as one light.
    keyTexture = BuildLayer(renderer, lowWidth, lowHeight, params, centre,
                            radius, exponent, params.key, params.keyOpacity,
                            /*invert=*/false);
    if (keyTexture == nullptr)
      return false;

    vignetteTexture =
        BuildLayer(renderer, lowWidth, lowHeight, params, centre, radius,
                   exponent, params.vignette, params.vignetteOpacity,
                   /*invert=*/true);
    if (vignetteTexture == nullptr) {
      Release();
      return false;
    }

    destination.x = 0;
    destination.y = 0;
    destination.w = params.width;
    destination.h = params.height;
    return true;
  }

  // Two SDL_RenderCopy calls, vignette first so the key light sits over it.
  // A no-op on an unbuilt overlay, so a game that never called Build -- or
  // whose Build failed -- draws an unlit frame instead of nothing at all.
  void Draw(SDL_Renderer *renderer) const {
    if (renderer == nullptr || !IsBuilt())
      return;

    SDL_RenderCopy(renderer, vignetteTexture, nullptr, &destination);
    SDL_RenderCopy(renderer, keyTexture, nullptr, &destination);
  }

  void Release() {
    if (keyTexture != nullptr) {
      SDL_DestroyTexture(keyTexture);
      keyTexture = nullptr;
    }
    if (vignetteTexture != nullptr) {
      SDL_DestroyTexture(vignetteTexture);
      vignetteTexture = nullptr;
    }
  }

  bool IsBuilt() const {
    return keyTexture != nullptr && vignetteTexture != nullptr;
  }

private:
  // Quarter resolution on each axis, so a sixteenth of the pixels.
  static constexpr int kScale = 4;

  // The shared falloff: 1 at the centre, 0 at `radius` and beyond.
  static float Falloff(float x, float y, const glm::vec2 &centre, float radius,
                       float exponent) {
    const float dx = x - centre.x;
    const float dy = y - centre.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance >= radius)
      return 0.0f;
    return std::pow(1.0f - distance / radius, exponent);
  }

  // One layer. `invert` is the vignette: same falloff, complementary alpha.
  //
  // The surface is written in the low-resolution space and the falloff is
  // sampled at each texel's CENTRE in full-screen coordinates -- offsetting by
  // half a texel rather than sampling the corner, which would bias the whole
  // gradient up and to the left by two full-screen pixels.
  static SDL_Texture *BuildLayer(SDL_Renderer *renderer, int lowWidth,
                                 int lowHeight, const Params &params,
                                 const glm::vec2 &centre, float radius,
                                 float exponent, const SDL_Color &colour,
                                 std::uint8_t opacity, bool invert) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, lowWidth, lowHeight, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr)
      return nullptr;

    const float stepX = static_cast<float>(params.width) /
                        static_cast<float>(lowWidth);
    const float stepY = static_cast<float>(params.height) /
                        static_cast<float>(lowHeight);

    std::uint32_t *pixels = static_cast<std::uint32_t *>(surface->pixels);
    const int stride = surface->pitch / 4;

    for (int y = 0; y < lowHeight; ++y) {
      const float sampleY = (static_cast<float>(y) + 0.5f) * stepY;
      for (int x = 0; x < lowWidth; ++x) {
        const float sampleX = (static_cast<float>(x) + 0.5f) * stepX;
        const float lit = Falloff(sampleX, sampleY, centre, radius, exponent);
        const float weight = invert ? 1.0f - lit : lit;
        const std::uint32_t alpha = static_cast<std::uint32_t>(
            std::lround(weight * static_cast<float>(opacity)));

        pixels[y * stride + x] = (alpha << 24) |
                                 (static_cast<std::uint32_t>(colour.r) << 16) |
                                 (static_cast<std::uint32_t>(colour.g) << 8) |
                                 static_cast<std::uint32_t>(colour.b);
      }
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr)
      return nullptr;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    // Linear on the upscale is not a nicety here -- it is what stands in for
    // the per-pixel falloff that was never computed. Nearest would show the
    // quarter-resolution grid as banding.
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    return texture;
  }

  SDL_Texture *keyTexture = nullptr;
  SDL_Texture *vignetteTexture = nullptr;
  SDL_Rect destination{0, 0, 0, 0};
};

} // namespace storm
