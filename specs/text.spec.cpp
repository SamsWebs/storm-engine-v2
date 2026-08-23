#include <igloo/igloo_alt.h>

#include "../common/text.h"
#include "support/softwareRenderer.h"

using namespace igloo;

// Headless. Measure and Draw both have to survive a null font, because
// AssetStore::GetFont returns nullptr for a missing id and the whole point of
// that contract is that callers may pass the result straight through.
Describe(TextSpec) {

  It(should_measure_a_null_font_as_zero) {
    const SDL_Point size = Text::Measure(nullptr, "anything");
    Assert::That(size.x, Equals(0));
    Assert::That(size.y, Equals(0));
  };

  It(should_measure_an_empty_string_as_zero) {
    const SDL_Point size = Text::Measure(nullptr, "");
    Assert::That(size.x, Equals(0));
    Assert::That(size.y, Equals(0));
  };

  It(should_draw_nothing_for_a_null_font) {
    SpecSurfaceTarget target(32, 32);
    const SDL_Point size = Text::Draw(target.renderer, nullptr, "hi", 0, 0,
                                      SDL_Color{255, 255, 255, 255});
    Assert::That(size.x, Equals(0));
  };

  It(should_draw_nothing_for_a_null_renderer) {
    const SDL_Point size =
        Text::Draw(nullptr, nullptr, "hi", 0, 0, SDL_Color{255, 255, 255, 255});
    Assert::That(size.x, Equals(0));
  };

  It(should_leave_the_surface_untouched_when_it_draws_nothing) {
    SpecSurfaceTarget target(16, 16);
    Text::Draw(target.renderer, nullptr, "hi", 0, 0, SDL_Color{255, 0, 0, 255});
    Assert::That(target.RgbAt(8, 8), Equals(SpecSurfaceTarget::kNothing));
  };

  It(should_centre_nothing_for_a_null_font) {
    SpecSurfaceTarget target(32, 32);
    const SDL_Point size = Text::DrawCentred(target.renderer, nullptr, "hi", 16,
                                             0, SDL_Color{255, 255, 255, 255});
    Assert::That(size.x, Equals(0));
  };
};
