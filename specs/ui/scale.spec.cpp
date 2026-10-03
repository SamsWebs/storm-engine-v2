// ui/scale.h — 2.4.2.
//
// Pure and SDL-free on purpose, and the "no .cpp in a header-only directory"
// rule is not decoration here: Android globs a fixed directory list, so a
// .cpp added under common/ui/ would build on desktop and silently vanish on
// mobile. The whole point of a scaling helper is that it cannot go missing on
// the platform nobody tests.
//
// The defect this exists to prevent is a layout that drifts apart on a bigger
// screen. A game that scales its fonts but not its literal offsets gets a 4K
// layout whose halves disagree; a game that scales both by different factors
// gets the same thing more slowly. One function is what makes them agree by
// construction, which is why this is Px()/FontPt() and not a "scale" object a
// caller is expected to remember to use consistently.
#include "../../common/ui/scale.h"

#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

Describe(UiScaleSpec) {

  It(is_the_identity_at_the_reference_height) {
    // 720 is the reference because it is the smallest height at which a
    // 16pt HUD label is comfortably legible, and because every common laptop
    // and phone resolution is a multiple or near-multiple of it. At the
    // reference, scale must change NOTHING -- a helper that shifts a layout at
    // the resolution it was authored at is a helper nobody trusts.
    UiScale s(720);
    Assert::That(s.Px(100), Equals(100));
    Assert::That(s.FontPt(16), Equals(16));
    // Exact, not within-an-epsilon: snowhouse's EqualsConstraint has no
    // tolerance modifier, and none is wanted here anyway. 720/720 and 1080/1080
    // are exactly 1.0f in IEEE754, so an epsilon would only hide a genuine
    // division bug.
    Assert::That(s.Factor(), Equals(1.0f));
  };

  It(scales_proportionally_to_the_window_height) {
    Assert::That(UiScale(1080).Px(100), Equals(150));
    Assert::That(UiScale(2160).Px(100), Equals(300));
    Assert::That(UiScale(1080).FontPt(16), Equals(24));
    Assert::That(UiScale(480).FontPt(16), Equals(11)); // 10.67 rounds to 11
  };

  It(keeps_px_and_FontPt_agreeing_about_the_same_factor) {
    // The entire reason both functions exist. A caller who scales a 16pt font
    // and a 16px offset must get the same number out of both, or the halves of
    // the layout drift apart with no visible cause.
    UiScale s(1440);
    Assert::That(s.FontPt(16), Equals(s.Px(16)));
    Assert::That(s.FontPt(24), Equals(s.Px(24)));
  };

  It(rounds_half_away_from_zero) {
    // std::lround, not a truncation: truncation makes every odd pixel at 1.5x
    // lose half a pixel, and a column of them drifts visibly.
    Assert::That(UiScale(1080).Px(1), Equals(2)); // 1.5 -> 2
    Assert::That(UiScale(1080).Px(3), Equals(5)); // 4.5 -> 5
    Assert::That(UiScale(1080).Px(5), Equals(8)); // 7.5 -> 8
  };

  It(never_lets_a_positive_value_round_away_to_nothing) {
    // The case a naive lround gets wrong and a player sees: a 1px border or a
    // 1px gap at 480p becomes 0, so a hairline disappears entirely at small
    // window sizes -- and the layout is fine everywhere else.
    UiScale small(240);
    Assert::That(small.Px(1), Equals(1));
    Assert::That(small.Px(2), Is().GreaterThan(0));
    Assert::That(small.FontPt(1), Is().GreaterThan(0));
  };

  It(keeps_zero_at_zero) {
    // The clamp above is for POSITIVE values only. An offset of zero means
    // "flush against the edge", and clamping it to 1 would move it.
    UiScale s(240);
    Assert::That(s.Px(0), Equals(0));
    Assert::That(s.FontPt(0), Equals(0));
  };

  It(is_monotonic) {
    // Rounding must not reorder. If Px(2) could exceed Px(3), a pair of
    // adjacent columns would swap widths between resolutions.
    const int heights[] = {240, 480, 720, 900, 1080, 1440, 2160, 4320};
    for (int h : heights) {
      UiScale s(h);
      for (int v = 0; v < 200; v += 7) {
        Assert::That(s.Px(v + 1), Is().GreaterThanOrEqualTo(s.Px(v)));
        Assert::That(s.FontPt(v + 1), Is().GreaterThanOrEqualTo(s.FontPt(v)));
      }
    }
  };

  It(refuses_a_degenerate_window_height) {
    // A zero or negative height would make the factor infinite or negative, and
    // every Px() would come back as INT_MIN. A layout helper that cannot render
    // is worse than one that does not scale, so the window height is clamped
    // to the reference rather than trusted.
    UiScale zero(0);
    Assert::That(zero.Px(100), Equals(100));
    UiScale negative(-500);
    Assert::That(negative.Px(100), Equals(100));
    Assert::That(negative.Factor(), Equals(1.0f));
  };

  It(accepts_a_custom_reference_height) {
    // A game authored against 1080 rather than 720 gets an identity at 1080,
    // which is the same promise the default makes at 720.
    UiScale s(1080, 1080);
    Assert::That(s.Px(64), Equals(64));
    // Exact, not within-an-epsilon: snowhouse's EqualsConstraint has no
    // tolerance modifier, and none is wanted here anyway. 720/720 and 1080/1080
    // are exactly 1.0f in IEEE754, so an epsilon would only hide a genuine
    // division bug.
    Assert::That(s.Factor(), Equals(1.0f));
    Assert::That(UiScale(2160, 1080).Px(64), Equals(128));
  };

  It(is_the_default_constructible_identity) {
    // A UiScale default-constructed, before the window size is known, must not
    // multiply a layout by zero. It is the reference until told otherwise.
    UiScale s;
    Assert::That(s.Px(100), Equals(100));
    // Exact, not within-an-epsilon: snowhouse's EqualsConstraint has no
    // tolerance modifier, and none is wanted here anyway. 720/720 and 1080/1080
    // are exactly 1.0f in IEEE754, so an epsilon would only hide a genuine
    // division bug.
    Assert::That(s.Factor(), Equals(1.0f));
  };

  It(exposes_the_free_function_forms) {
    // The roadmap names both shapes, and the free functions are what a caller
    // reaches for deep inside a draw call where constructing a scale object is
    // noise. They must agree with the type exactly, or a layout picks up two
    // different scalings depending on which call site wrote it.
    Assert::That(Px(100, 1080), Equals(UiScale(1080).Px(100)));
    Assert::That(FontPt(16, 1080), Equals(UiScale(1080).FontPt(16)));
    Assert::That(Px(100, 1080), Equals(150));
    // And they must be safe on the same degenerate input.
    Assert::That(Px(100, 0), Equals(100));
    Assert::That(FontPt(16, -1), Equals(16));
  };
};
