// This spec includes gameStateBase.h and NOTHING else from the engine, the same
// constraint specs/states/gameStateBase.spec.cpp holds: Present() must not drag
// the convenience includes back into the slim header.
#include "../../common/states/gameStateBase.h"

#include <igloo/igloo_alt.h>

#include "../support/softwareRenderer.h"

using namespace igloo;
using namespace storm;

// GameState::Present() — 2.4.1.
//
// The defect this exists to stop: on Android the touch overlay must be drawn
// BEFORE the present, and a state that calls SDL_RenderPresent itself ships a
// menu with invisible controls that looks perfect on every other platform. That
// shipped once in the flagship game; the fix there was a base class rather than
// a convention, and the engine is where it belongs.
//
// So the rule under test is not "Present calls SDL_RenderPresent". It is the
// ORDERING, because that is the part a caller can get wrong: every registered
// overlay runs, in registration order, before the frame is presented. Draw
// order is also what decides what a player sees when two overlays overlap, so
// it is observable in the pixels and not merely asserted.
//
// A note on what is deliberately NOT spec'd: "presents exactly once" is a
// property of the code shape, not of the output. SDL_RenderPresent returns void
// and on a software renderer it is a silent no-op, so no sequence of pixels can
// distinguish one call from two. The guarantee is structural -- the call is the
// last statement of Present(), and it is the only SDL_RenderPresent in common/
// (`grep -rn SDL_RenderPresent common/` was empty before this change, which is
// how the 2.4.1 audit found the gap) -- and this file does not pretend to
// check it.

namespace {

// A complete GameState built from the slim header alone: no renderer, no ECS,
// no components, no systems. It is the shape a game's own state has, and it is
// also the proof that Present() needs none of them.
class SpecPresentState : public GameState {
public:
  void processInput() override {}
  void update() override {}
  void render() override {}
  bool onEnter() override { return true; }
  bool onExit() override { return true; }
  std::string getStateID() const override { return "present-probe"; }

  // Present() is protected; expose exactly it and nothing else.
  using GameState::Present;
};

} // namespace

Describe(GameStatePresentSpec) {

  It(draws_a_registered_overlay_into_the_renderer) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));

    SpecPresentState state;
    OverlayList overlays;
    overlays.Add("fill", [](SDL_Renderer *r) {
      SDL_SetRenderDrawColor(r, 0x11, 0x22, 0x33, 0xFF);
      SDL_RenderFillRect(r, nullptr);
    });

    state.Present(target.renderer, overlays);

    Assert::That(target.RgbAt(4, 4), Equals(0x00112233u));
  };

  It(runs_every_registered_overlay_not_just_the_first) {
    SpecSurfaceTarget target(32, 32);
    SpecPresentState state;
    OverlayList overlays;

    // Two overlays painting disjoint halves. If only the first ran, the right
    // half would still be background.
    overlays.Add("left", [](SDL_Renderer *r) {
      SDL_SetRenderDrawColor(r, 0xFF, 0x00, 0x00, 0xFF);
      SDL_Rect l{0, 0, 16, 32};
      SDL_RenderFillRect(r, &l);
    });
    overlays.Add("right", [](SDL_Renderer *r) {
      SDL_SetRenderDrawColor(r, 0x00, 0xFF, 0x00, 0xFF);
      SDL_Rect rr{16, 0, 16, 32};
      SDL_RenderFillRect(r, &rr);
    });

    state.Present(target.renderer, overlays);

    Assert::That(target.RgbAt(4, 4), Equals(0x00FF0000u));
    Assert::That(target.RgbAt(24, 4), Equals(0x0000FF00u));
  };

  It(draws_a_later_registration_on_top_of_an_earlier_one) {
    SpecSurfaceTarget target(32, 32);
    SpecPresentState state;
    OverlayList overlays;

    // Same rect, two colours. Registration order is the whole contract here:
    // draw it back-to-front instead and the pixel comes out red, not blue.
    overlays.Add("under", [](SDL_Renderer *r) {
      SDL_SetRenderDrawColor(r, 0xFF, 0x00, 0x00, 0xFF);
      SDL_RenderFillRect(r, nullptr);
    });
    overlays.Add("over", [](SDL_Renderer *r) {
      SDL_SetRenderDrawColor(r, 0x00, 0x00, 0xFF, 0xFF);
      SDL_RenderFillRect(r, nullptr);
    });

    state.Present(target.renderer, overlays);

    Assert::That(target.RgbAt(16, 16), Equals(0x000000FFu));
  };

  It(replaces_a_callback_under_an_existing_name_and_keeps_its_position) {
    OverlayList overlays;
    overlays.Add("first", [](SDL_Renderer *) {});
    overlays.Add("second", [](SDL_Renderer *) {});
    overlays.Add("third", [](SDL_Renderer *) {});

    // A replace-IN-PLACE is what keeps a HUD that re-registers every frame from
    // jumping to the top of the stack every frame. Appending on replace would
    // make the stack order depend on how often something re-registered.
    Assert::That(overlays.Add("second", [](SDL_Renderer *) {}), Equals(true));
    Assert::That(overlays.Size(), Equals(static_cast<std::size_t>(3)));
    Assert::That(overlays.NameAt(0), Equals("first"));
    Assert::That(overlays.NameAt(1), Equals("second"));
    Assert::That(overlays.NameAt(2), Equals("third"));
  };

  It(rejects_an_empty_name_and_a_null_callback) {
    OverlayList overlays;
    // An unnamed overlay cannot be removed or asserted about, and a null
    // callback would take the frame down on first draw. Both refused at the
    // door, and neither leaves a gap in the list.
    Assert::That(overlays.Add("", [](SDL_Renderer *) {}), Equals(false));
    Assert::That(overlays.Add("null", nullptr), Equals(false));
    Assert::That(overlays.Size(), Equals(static_cast<std::size_t>(0)));
  };

  It(removes_by_name_and_reports_whether_anything_went) {
    OverlayList overlays;
    overlays.Add("keep", [](SDL_Renderer *) {});
    overlays.Add("drop", [](SDL_Renderer *) {});

    Assert::That(overlays.Remove("drop"), Equals(true));
    Assert::That(overlays.Remove("drop"), Equals(false));
    Assert::That(overlays.Contains("drop"), Equals(false));
    Assert::That(overlays.Contains("keep"), Equals(true));
    Assert::That(overlays.Size(), Equals(static_cast<std::size_t>(1)));
  };

  It(stops_drawing_a_removed_overlay) {
    SpecSurfaceTarget target(32, 32);
    SpecPresentState state;
    OverlayList overlays;
    overlays.Add("fill", [](SDL_Renderer *r) {
      SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xFF);
      SDL_RenderFillRect(r, nullptr);
    });
    overlays.Remove("fill");

    state.Present(target.renderer, overlays);

    Assert::That(target.RgbAt(4, 4), Equals(SpecSurfaceTarget::kNothing));
  };

  It(presents_with_no_overlays_at_all_and_with_the_default_argument) {
    SpecSurfaceTarget target(32, 32);
    SpecPresentState state;

    // The common case: a state with nothing to overlay. Present() must still be
    // safe, and the default argument must compile -- otherwise every adoption
    // is a second decision at every call site.
    state.Present(target.renderer);
    Assert::That(target.DrawnPixelCount(), Equals(0));
  };

  It(tolerates_a_null_renderer) {
    SpecPresentState state;
    OverlayList overlays;

    // Add() refuses a null callback, but a caller can still hand Present() a
    // renderer it never got from SDL -- on a teardown path, or a headless
    // build. Neither may crash: a menu that vanishes is the bug this item is
    // about, not the fix for it.
    state.Present(nullptr, overlays);
  };

  It(clears_the_list) {
    OverlayList overlays;
    overlays.Add("a", [](SDL_Renderer *) {});
    overlays.Add("b", [](SDL_Renderer *) {});
    overlays.Clear();
    Assert::That(overlays.Size(), Equals(static_cast<std::size_t>(0)));
    Assert::That(overlays.Contains("a"), Equals(false));
  };

  It(adds_no_member_to_GameState) {
    // The 2.4.x rule: nothing here may change GameState's size or its vtable,
    // or a game deriving from it pays for an additive convenience with a layout
    // change it never asked for. Present() is non-virtual and OverlayList is
    // passed in rather than stored, so the class is untouched. sizeof is the
    // load-bearing half of this assertion; the member-pointer line above it is
    // the readable one, since a virtual Present() would still compile here.
    Assert::That(std::is_base_of<GameState, SpecPresentState>::value,
                 Equals(true));
    Assert::That(sizeof(OverlayList) > sizeof(void *), Equals(true));
  };
};
