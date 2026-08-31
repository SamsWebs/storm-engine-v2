#include <igloo/igloo_alt.h>

#include "../../common/states/gameState.h"

using namespace igloo;
using namespace storm;

namespace {

// GameState is abstract and CapFrameRate is protected, so a spec needs a
// concrete subclass that republishes both.
struct SpecPacedState : public GameState {
  void processInput() override {}
  void update() override {}
  void render() override {}
  bool onEnter() override { return true; }
  bool onExit() override { return true; }
  std::string getStateID() const override { return "SPEC_PACED"; }

  using GameState::CapFrameRate;
  using GameState::millisecondsPreviousFrame;
};

} // namespace

// No sleeping happens in any of these: each one starts the frame already well
// past the budget, so the delay branch is skipped and the specs stay fast and
// deterministic.
Describe(GameStateCapFrameRateSpec) {

  It(clamps_a_long_frame_to_the_default_maximum) {
    SpecPacedState state;
    state.millisecondsPreviousFrame = SDL_GetTicks() - 1000; // a second ago

    const double delta = state.CapFrameRate();

    Assert::That(delta, EqualsWithDelta(0.05, 0.0001));
  };

  It(honours_a_custom_maximum) {
    SpecPacedState state;
    state.millisecondsPreviousFrame = SDL_GetTicks() - 1000;

    Assert::That(state.CapFrameRate(0.25), EqualsWithDelta(0.25, 0.0001));
  };

  It(leaves_the_delta_unclamped_when_the_maximum_is_zero) {
    SpecPacedState state;
    state.millisecondsPreviousFrame = SDL_GetTicks() - 1000;

    // Roughly a second, and certainly not clamped down to 0.05.
    Assert::That(state.CapFrameRate(0.0) > 0.5, IsTrue());
  };

  It(rolls_the_timestamp_forward) {
    SpecPacedState state;
    state.millisecondsPreviousFrame = SDL_GetTicks() - 1000;

    state.CapFrameRate();

    // Within a few ms of now, rather than a second behind it.
    const int drift =
        static_cast<int>(SDL_GetTicks()) - state.millisecondsPreviousFrame;
    Assert::That(drift >= 0 && drift < 100, IsTrue());
  };

  It(does_not_sleep_for_a_minute_when_the_timestamp_was_never_seeded) {
    // millisecondsPreviousFrame defaults to 0, so the elapsed time reads as
    // the whole uptime and `remaining` goes hugely negative. Without the upper
    // bound on the guard this would be a long, silent hang.
    SpecPacedState state;
    const Uint32 before = SDL_GetTicks();

    state.CapFrameRate();

    Assert::That(SDL_GetTicks() - before < 100u, IsTrue());
  };
};
