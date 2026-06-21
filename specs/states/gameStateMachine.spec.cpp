
#include <igloo/igloo_alt.h>

#include "../../common/gameStateMachine.h"
#include "../../common/states/gameState.h"

using namespace igloo;

// Minimal mock used by the original push/pop tests.
class MockGameState : public GameState {
public:
  void processInput() override {}
  void update() override {}
  void render() override {}
  bool onEnter() override { return true; }
  bool onExit() override { return true; }
  std::string getStateID() const override { return ""; }
};

// Lifecycle-tracking mock: counters live in test scope so they survive the
// state being deleted by clean().
class TrackedState : public GameState {
public:
  TrackedState(std::string id, int *enters = nullptr, int *exits = nullptr,
               int *resumes = nullptr)
      : id_(std::move(id)), enters_(enters), exits_(exits), resumes_(resumes) {}

  void processInput() override {}
  void update() override {}
  void render() override {}
  bool onEnter() override { if (enters_) (*enters_)++; return true; }
  bool onExit() override  { if (exits_)  (*exits_)++;  return true; }
  void resume() override  { if (resumes_) (*resumes_)++; }
  std::string getStateID() const override { return id_; }

private:
  std::string id_;
  int *enters_, *exits_, *resumes_;
};

Describe(GameStateMachineSpec) {
  GameStateMachine gameStateMachine;

  It(should_push_state_to_game_states) {
    auto gameState = std::make_unique<MockGameState>();
    gameStateMachine.pushState(gameState.get());
    Assert::That(gameStateMachine.getGameStates().size(), Equals(1));
    Assert::That(gameStateMachine.getGameStates()[0], Equals(gameState.get()));
  };

  It(should_pop_state_from_game_states) {
    auto gameState1 = std::make_unique<MockGameState>();
    auto gameState2 = std::make_unique<MockGameState>();
    gameStateMachine.pushState(gameState1.get());
    gameStateMachine.pushState(gameState2.get());
    gameStateMachine.popState();
    Assert::That(gameStateMachine.getGameStates().size(), Equals(1));
    Assert::That(gameStateMachine.getGameStates()[0], Equals(gameState1.get()));
  };
};

Describe(GameStateMachineChangeStateSpec) {
  It(should_enter_the_new_state_when_changing) {
    GameStateMachine sm;
    int enters = 0;
    auto state = std::make_unique<TrackedState>("PLAY", &enters);
    sm.changeState(state.get());
    Assert::That(enters, Equals(1));
    Assert::That(sm.getGameStates().size(), Equals(1));
  };

  It(should_exit_the_old_state_and_enter_the_new_one) {
    GameStateMachine sm;
    int exitsA = 0, entersB = 0;
    auto a = std::make_unique<TrackedState>("A", nullptr, &exitsA);
    auto b = std::make_unique<TrackedState>("B", &entersB);
    sm.changeState(a.get());
    sm.changeState(b.get());
    Assert::That(exitsA, Equals(1));
    Assert::That(entersB, Equals(1));
    Assert::That(sm.getGameStates().size(), Equals(1));
    Assert::That(sm.getGameStates()[0], Equals(b.get()));
  };

  It(should_be_a_no_op_when_changing_to_the_same_state_id) {
    GameStateMachine sm;
    int exitsA = 0, entersB = 0;
    auto a = std::make_unique<TrackedState>("SAME", nullptr, &exitsA);
    auto b = std::make_unique<TrackedState>("SAME", &entersB);
    sm.changeState(a.get());
    sm.changeState(b.get());
    // The original state stays on top; new one is neither entered nor swapped in.
    Assert::That(exitsA, Equals(0));
    Assert::That(entersB, Equals(0));
    Assert::That(sm.getGameStates()[0], Equals(a.get()));
  };
};

Describe(GameStateMachinePopResumeSpec) {
  It(should_resume_the_underlying_state_when_popping) {
    GameStateMachine sm;
    int resumesA = 0;
    auto a = std::make_unique<TrackedState>("A", nullptr, nullptr, &resumesA);
    auto b = std::make_unique<TrackedState>("B");
    sm.pushState(a.get());
    sm.pushState(b.get());
    sm.popState();
    Assert::That(resumesA, Equals(1));
  };

  It(should_exit_the_popped_state) {
    GameStateMachine sm;
    int exitsB = 0;
    auto a = std::make_unique<TrackedState>("A");
    auto b = std::make_unique<TrackedState>("B", nullptr, &exitsB);
    sm.pushState(a.get());
    sm.pushState(b.get());
    sm.popState();
    Assert::That(exitsB, Equals(1));
  };
};

Describe(GameStateMachineCleanSpec) {
  It(should_exit_and_empty_the_stack) {
    GameStateMachine sm;
    int exits = 0;
    // clean() deletes the state, so allocate on the heap and let clean own it.
    sm.pushState(new TrackedState("A", nullptr, &exits));
    sm.clean();
    Assert::That(exits, Equals(1));
    Assert::That(sm.getGameStates().size(), Equals(0));
  };
};

Describe(GameStateMachineEmptyStackSpec) {
  It(should_be_safe_to_pop_an_empty_stack) {
    GameStateMachine sm;
    sm.popState();
    Assert::That(sm.getGameStates().size(), Equals(0));
  };

  It(should_be_safe_to_clean_an_empty_stack) {
    GameStateMachine sm;
    sm.clean();
    Assert::That(sm.getGameStates().size(), Equals(0));
  };

  It(should_be_safe_to_update_render_and_process_input_when_empty) {
    GameStateMachine sm;
    sm.processInput();
    sm.update();
    sm.render();
    Assert::That(sm.getGameStates().size(), Equals(0));
  };
};
