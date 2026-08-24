// This spec includes gameStateBase.h and NOTHING else from the engine. That is
// the whole point of it: if the slim header ever grows a dependency on the
// convenience includes that gameState.h carries, this file stops compiling and
// the 45% saving quietly disappears.
#include "../../common/states/gameStateBase.h"

#include <igloo/igloo_alt.h>

using namespace igloo;

namespace {

// A complete GameState subclass built from the slim header alone - no
// AssetStore, no Registry, no components, no systems in sight.
struct SpecSlimState : public GameState {
  void processInput() override { inputs++; }
  void update() override { updates++; }
  void render() override { renders++; }
  bool onEnter() override { return true; }
  bool onExit() override { return true; }
  std::string getStateID() const override { return "SPEC_SLIM"; }

  int inputs = 0, updates = 0, renders = 0;

  using GameState::CapFrameRate;
  using GameState::millisecondsPreviousFrame;
};

} // namespace

Describe(GameStateBaseSpec) {

  It(defines_a_usable_state_without_the_convenience_includes) {
    SpecSlimState state;
    state.processInput();
    state.update();
    state.render();

    Assert::That(state.inputs, Equals(1));
    Assert::That(state.updates, Equals(1));
    Assert::That(state.renders, Equals(1));
    Assert::That(state.getStateID(), Equals("SPEC_SLIM"));
  };

  It(carries_the_frame_budget_constants) {
    Assert::That(FPS, Equals(60));
    Assert::That(MILLISECS_PER_FRAME, Equals(1000 / 60));
  };

  It(carries_CapFrameRate) {
    SpecSlimState state;
    state.millisecondsPreviousFrame = SDL_GetTicks() - 1000;
    Assert::That(state.CapFrameRate(), EqualsWithDelta(0.05, 0.0001));
  };
};
