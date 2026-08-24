// Guards the property that makes gameStateBase.h worth anything:
// gameStateMachine.h must NOT drag the ECS in.
//
// A state that includes the slim base and the state machine has to stay slim.
// If gameStateMachine.h ever goes back to including states/gameState.h, a file
// like this one silently jumps from 89,198 preprocessed lines to 146,775 and
// the saving disappears with no test failing.
//
// The guard below is a compile-time one rather than an assertion: defining
// Registry here collides with ecs.h's definition, so this file stops compiling
// the moment the ECS leaks back in. That is deliberate - the regression is a
// build error, not a silent cost.
#include "../common/gameStateMachine.h"
#include "../common/states/gameStateBase.h"

namespace {
// Not the engine's Registry. If ecs.h has been pulled in, this is a
// redefinition and the build fails - which is the whole point.
struct Registry {
  int canary = 0;
};
} // namespace

#include <igloo/igloo_alt.h>

using namespace igloo;

namespace {

struct SpecSlimMachineState : public GameState {
  explicit SpecSlimMachineState(std::string id) : id_(std::move(id)) {}
  void processInput() override {}
  void update() override {}
  void render() override {}
  bool onEnter() override {
    entered = true;
    return true;
  }
  bool onExit() override {
    exited = true;
    return true;
  }
  std::string getStateID() const override { return id_; }

  std::string id_;
  bool entered = false;
  bool exited = false;
};

} // namespace

Describe(GameStateMachineSlimSpec) {

  It(drives_a_state_without_the_ecs_in_scope) {
    GameStateMachine machine;
    // The machine owns the pointer; never delete it yourself.
    machine.changeState(new SpecSlimMachineState("A"));

    machine.processInput();
    machine.update();
    machine.render();

    machine.clean();
    Assert::That(true, IsTrue()); // reaching here without a crash is the check
  };

  It(keeps_the_ecs_out_of_scope) {
    // Resolves to the local Registry above, not the engine's. If ecs.h had
    // leaked in this file would not have compiled at all.
    Registry local;
    Assert::That(local.canary, Equals(0));
  };
};
