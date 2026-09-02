#include "../common/lighting.h"
#include "support/softwareRenderer.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

// Channels of one pixel, so a case can talk about warmth rather than about
// 0x00RRGGBB.
struct Rgb {
  int r;
  int g;
  int b;
};

Rgb PixelAt(const SpecSurfaceTarget &target, int x, int y) {
  const Uint32 packed = target.RgbAt(x, y);
  return Rgb{static_cast<int>((packed >> 16) & 0xFF),
             static_cast<int>((packed >> 8) & 0xFF),
             static_cast<int>(packed & 0xFF)};
}

} // namespace

Describe(LightingOverlaySpec) {

  It(builds_two_layers_and_reports_it) {
    SpecSurfaceTarget target(64, 64);
    Assert::That(target.IsUsable(), Equals(true));

    LightingOverlay lighting;
    Assert::That(lighting.IsBuilt(), Equals(false));

    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;

    Assert::That(lighting.Build(target.renderer, params), Equals(true));
    Assert::That(lighting.IsBuilt(), Equals(true));
  }

  // The pairing is the whole technique: warm where the key light reaches, cool
  // where it does not. A single-layer tint would make the centre and the corner
  // the same temperature, and this is the case that catches it.
  It(leaves_the_centre_warm_and_the_corner_cool) {
    SpecSurfaceTarget target(64, 64);
    Assert::That(target.IsUsable(), Equals(true));

    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));

    lighting.Draw(target.renderer);

    const Rgb centre = PixelAt(target, 32, 32);
    const Rgb corner = PixelAt(target, 1, 1);

    // Warm: red ahead of blue. The target was cleared to black, so every
    // channel here came from the overlay.
    Assert::That(centre.r, Is().GreaterThan(centre.b));
    // Cool: blue ahead of red.
    Assert::That(corner.b, Is().GreaterThan(corner.r));
    // And the two are genuinely different, not both a dark smudge.
    Assert::That(centre.r, Is().GreaterThan(corner.r));
  }

  // The falloff is radial, so warmth decreases monotonically outward along an
  // axis. A layer built from the texel corner rather than its centre still
  // passes the centre-vs-corner case above; it fails this one, because the
  // whole gradient shifts.
  It(falls_off_with_distance_from_the_centre) {
    SpecSurfaceTarget target(64, 64);
    Assert::That(target.IsUsable(), Equals(true));

    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));

    lighting.Draw(target.renderer);

    int previous = PixelAt(target, 32, 32).r;
    for (int x = 36; x <= 60; x += 4) {
      const int here = PixelAt(target, x, 32).r;
      Assert::That(here, Is().LessThanOrEqualTo(previous));
      previous = here;
    }
  }

  // A tighter radius pulls the lit pool in, so a point that was lit goes dark.
  // Without this, radius could be ignored entirely and every case above would
  // still pass.
  It(honours_a_smaller_radius) {
    const auto redAt = [](float radius, int x) {
      SpecSurfaceTarget target(64, 64);
      LightingOverlay lighting;
      LightingOverlay::Params params;
      params.width = 64;
      params.height = 64;
      params.radius = radius;
      lighting.Build(target.renderer, params);
      lighting.Draw(target.renderer);
      return PixelAt(target, x, 32).r;
    };

    // At x=48, 16px from the centre: inside a 32px radius, outside an 8px one.
    Assert::That(redAt(8.0f, 48), Is().LessThan(redAt(32.0f, 48)));
  }

  It(centres_the_key_light_where_it_is_told_to) {
    SpecSurfaceTarget target(64, 64);
    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;
    params.centre = glm::vec2(8, 8);
    params.radius = 16.0f;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));

    lighting.Draw(target.renderer);

    // Warm near the requested centre, not near the middle of the screen.
    Assert::That(PixelAt(target, 8, 8).r,
                 Is().GreaterThan(PixelAt(target, 32, 32).r));
  }

  It(draws_nothing_when_it_was_never_built) {
    SpecSurfaceTarget target(64, 64);
    Assert::That(target.IsUsable(), Equals(true));

    LightingOverlay lighting;
    lighting.Draw(target.renderer);

    // An unlit frame, not a black one and not a crash.
    Assert::That(target.DrawnPixelCount(), Equals(0));
  }

  It(refuses_a_null_renderer_or_a_non_positive_size) {
    SpecSurfaceTarget target(64, 64);

    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;

    Assert::That(lighting.Build(nullptr, params), Equals(false));
    Assert::That(lighting.IsBuilt(), Equals(false));

    params.width = 0;
    Assert::That(lighting.Build(target.renderer, params), Equals(false));

    params.width = 64;
    params.height = -10;
    Assert::That(lighting.Build(target.renderer, params), Equals(false));
    Assert::That(lighting.IsBuilt(), Equals(false));
  }

  // A resolution change rebuilds. The previous pair has to go, and the overlay
  // has to end up describing the new size rather than stretching the old one.
  It(rebuilds_over_a_previous_build) {
    SpecSurfaceTarget target(64, 64);

    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 16;
    params.height = 16;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));

    params.width = 64;
    params.height = 64;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));
    Assert::That(lighting.IsBuilt(), Equals(true));

    lighting.Draw(target.renderer);
    // The far corner is covered, which it would not be if the 16x16
    // destination rect had survived the rebuild.
    Assert::That(target.RgbAt(60, 60),
                 Is().Not().EqualTo(SpecSurfaceTarget::kNothing));
  }

  It(is_inert_after_release) {
    SpecSurfaceTarget target(64, 64);

    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));

    lighting.Release();
    Assert::That(lighting.IsBuilt(), Equals(false));

    lighting.Draw(target.renderer);
    Assert::That(target.DrawnPixelCount(), Equals(0));

    // Releasing twice is not a double free.
    lighting.Release();
    Assert::That(lighting.IsBuilt(), Equals(false));
  }

  // A zero or negative exponent would make std::pow(0, e) infinite or one, and
  // the whole layer would come out flat. It falls back to linear instead.
  It(treats_a_non_positive_falloff_as_linear) {
    SpecSurfaceTarget target(64, 64);

    LightingOverlay lighting;
    LightingOverlay::Params params;
    params.width = 64;
    params.height = 64;
    params.falloff = 0.0f;
    Assert::That(lighting.Build(target.renderer, params), Equals(true));

    lighting.Draw(target.renderer);
    Assert::That(PixelAt(target, 32, 32).r,
                 Is().GreaterThan(PixelAt(target, 60, 32).r));
  }
};
