#pragma once

// One scale factor, so a layout's halves cannot disagree about it.
//
// A game that scales its fonts but not its literal offsets gets a 4K layout
// whose two halves drift apart. A game that scales both, by factors it
// computed separately, gets the same thing more slowly. Neither bug announces
// itself: the layout looks correct on the resolution it was authored at, and
// wrong on a monitor nobody tested on. One function is what makes the halves
// agree by construction, which is why this is Px()/FontPt() and not a "scale"
// object a caller is expected to remember to apply consistently.
//
// PURE, SDL-FREE, HEADER-ONLY, AND NO .cpp IN THIS DIRECTORY. That last one is
// not style: Android's CMake reads engine-sources.txt, and a .cpp added here
// would have to be added to that list as well or it builds on desktop and
// silently vanishes on mobile. The flagship game that needed this first wrote
// its own copy, 9,367 lines under its src/ui/ of which the scale pair was the
// plainly generic part.
//
//     UiScale ui(windowHeight);
//     SDL_Rect r{ ui.Px(16), ui.Px(24), ui.Px(200), ui.Px(48) };
//     RenderText(fontAt(ui.FontPt(16)), "...", r.x, r.y);
//
// The deep-call-site form is a free function, and it agrees with the type
// exactly -- which is the property that matters, since a layout that picks up
// two different scalings depending on which call site wrote it has the
// original bug wearing a disguise.
//
//     SDL_RenderFillRect(r, &SDL_Rect{ Px(16, h), Px(24, h), ... });
//
// No exceptions: this header is reachable from Switch, which compiles with
// them disabled, so there is no std::stoi, no .at() and no throw.
#include <algorithm>
#include <cmath>

namespace storm {

// The height a layout is authored against. 720 is the smallest height at which
// a 16pt HUD label is comfortably legible, and every common laptop and phone
// resolution is a multiple or near-multiple of it, so scaling is close to
// exact rather than fractional everywhere.
inline constexpr int kUiReferenceHeight = 720;

namespace detail {

// Round half away from zero, and never let a POSITIVE input become 0.
//
// The clamp is the part a naive lround gets wrong and a player sees: a 1px
// border or a 1px gap at 480p rounds to zero, so the hairline disappears
// entirely at small window sizes while the layout is fine everywhere else.
// Zero stays zero, because an offset of 0 means "flush against the edge" and
// clamping it would move it -- and a negative offset (a margin on the left, a
// rect that runs off the edge) keeps its sign for the same reason. Clamping
// negatives into [1, inf) would push every off-screen edge on-screen.
inline int ScaleValue(int value, float factor) {
  if (value == 0 || factor <= 0.0f) {
    return 0;
  }
  const long long scaled =
      std::llround(static_cast<double>(value) * static_cast<double>(factor));
  if (value > 0 && scaled < 1) {
    return 1;
  }
  const long long limit = 2147483647LL;
  if (scaled > limit) {
    return static_cast<int>(limit);
  }
  if (scaled < -limit - 1) {
    return -static_cast<int>(limit) - 1;
  }
  return static_cast<int>(scaled);
}

} // namespace detail

// A layout scale bound to one window height.
//
// Default-constructed, it is the identity. A game that has a window before it
// has a scale object -- the normal case, since SDL hands you the size at
// window-creation time and the state is built first -- gets the reference
// rather than a factor of zero, so an unscaled layout is the failure mode and
// not a collapsed one.
class UiScale {
public:
  UiScale() = default;

  // A non-positive height is clamped to the reference rather than trusted: a
  // zero height makes the factor infinite and every Px() would come back as
  // INT_MIN, and a layout that cannot render is worse than one that does not
  // scale.
  explicit UiScale(int windowHeight, int referenceHeight = kUiReferenceHeight)
      : m_windowHeight(windowHeight > 0 ? windowHeight : 0),
        m_factor(windowHeight > 0 && referenceHeight > 0
                     ? static_cast<float>(windowHeight) /
                           static_cast<float>(referenceHeight)
                     : 1.0f) {}

  // The window height this scale was built for, or 0 if it was never told.
  int WindowHeight() const { return m_windowHeight; }

  float Factor() const { return m_factor; }

  // A literal offset, a margin, a border width, a rect dimension.
  int Px(int value) const { return detail::ScaleValue(value, m_factor); }

  // A font point size. Same arithmetic as Px(), deliberately: a 16pt label and
  // a 16px offset have to scale identically or the two halves of a layout
  // drift apart, which is the bug this whole header is for.
  //
  // Never returns 0 for a positive input, because TTF_OpenFont with size 0
  // fails and hands back a null font that every later call has to guard.
  int FontPt(int basePt) const { return detail::ScaleValue(basePt, m_factor); }

  // Convenience for the common pair: a font size for a scale.
  int operator()(int value) const { return Px(value); }

private:
  float m_factor = 1.0f;
  int m_windowHeight = 0;
};

// ── Free-function forms ──────────────────────────────────────────────────────
// For a draw call buried deep enough that constructing a scale object is noise.
// They MUST agree with the type above; specs/ui/scale.spec.cpp asserts it,
// because a layout that picks up two scalings depending on which call site
// wrote it has the original bug wearing a disguise.

inline int Px(int value, int windowHeight,
              int referenceHeight = kUiReferenceHeight) {
  return UiScale(windowHeight, referenceHeight).Px(value);
}

inline int FontPt(int basePt, int windowHeight,
                  int referenceHeight = kUiReferenceHeight) {
  return UiScale(windowHeight, referenceHeight).FontPt(basePt);
}

} // namespace storm
