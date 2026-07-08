#include <igloo/igloo_alt.h>

#include "../../common/gameStateMachine.h"
#include "../../common/states/gameState.h"

using namespace igloo;

// OWNERSHIP: the machine owns every state it is handed. popState/changeState/
// clean delete states, so specs allocate with `new` and observe lifecycles via
// counters that live in test scope (they survive the state's destruction).
class TrackedState : public GameState {
public:
  TrackedState(std::string id, int *enters = nullptr, int *exits = nullptr,
               int *resumes = nullptr, int *dtors = nullptr)
      : id_(std::move(id)), enters_(enters), exits_(exits),
        resumes_(resumes), dtors_(dtors) {}

  ~TrackedState() override { if (dtors_) (*dtors_)++; }

  void processInput() override {}
  void update() override {}
  void render() override {}
  bool onEnter() override { if (enters_) (*enters_)++; return true; }
  bool onExit() override  { if (exits_)  (*exits_)++;  return true; }
  void resume() override  { if (resumes_) (*resumes_)++; }
  std::string getStateID() const override { return id_; }

private:
  std::string id_;
  int *enters_, *exits_, *resumes_, *dtors_;
};

Describe(GameStateMachineSpec) {

  It(should_push_state_to_game_states) {
    GameStateMachine sm;
    auto *state = new TrackedState("A");
    sm.pushState(state);
    Assert::That(sm.getGameStates().size(), Equals(1));
    Assert::That(sm.getGameStates()[0], Equals(state));
    sm.clean();
  };

  It(should_pop_state_from_game_states) {
    GameStateMachine sm;
    auto *a = new TrackedState("A");
    sm.pushState(a);
    sm.pushState(new TrackedState("B"));
    sm.popState();
    Assert::That(sm.getGameStates().size(), Equals(1));
    Assert::That(sm.getGameStates()[0], Equals(a));
    sm.clean();
  };
};

Describe(GameStateMachineChangeStateSpec) {
  It(should_enter_the_new_state_when_changing) {
    GameStateMachine sm;
    int enters = 0;
    sm.changeState(new TrackedState("PLAY", &enters));
    Assert::That(enters, Equals(1));
    Assert::That(sm.getGameStates().size(), Equals(1));
    sm.clean();
  };

  It(should_exit_the_old_state_and_enter_the_new_one) {
    GameStateMachine sm;
    int exitsA = 0, entersB = 0;
    auto *b = new TrackedState("B", &entersB);
    sm.changeState(new TrackedState("A", nullptr, &exitsA));
    sm.changeState(b);
    Assert::That(exitsA, Equals(1));
    Assert::That(entersB, Equals(1));
    Assert::That(sm.getGameStates().size(), Equals(1));
    Assert::That(sm.getGameStates()[0], Equals(b));
    sm.clean();
  };

  It(should_be_a_no_op_when_changing_to_the_same_state_id) {
    GameStateMachine sm;
    int exitsA = 0, entersB = 0;
    auto *a = new TrackedState("SAME", nullptr, &exitsA);
    sm.changeState(a);
    sm.changeState(new TrackedState("SAME", &entersB));
    // The original state stays on top; new one is neither entered nor swapped in.
    Assert::That(exitsA, Equals(0));
    Assert::That(entersB, Equals(0));
    Assert::That(sm.getGameStates()[0], Equals(a));
    sm.clean();
  };
};

Describe(GameStateMachinePopResumeSpec) {
  It(should_resume_the_underlying_state_when_popping) {
    GameStateMachine sm;
    int resumesA = 0;
    sm.pushState(new TrackedState("A", nullptr, nullptr, &resumesA));
    sm.pushState(new TrackedState("B"));
    sm.popState();
    Assert::That(resumesA, Equals(1));
    sm.clean();
  };

  It(should_exit_the_popped_state) {
    GameStateMachine sm;
    int exitsB = 0;
    sm.pushState(new TrackedState("A"));
    sm.pushState(new TrackedState("B", nullptr, &exitsB));
    sm.popState();
    Assert::That(exitsB, Equals(1));
    sm.clean();
  };
};

// The machine owns its states: discarding a state must free it — but NOT
// inline. changeState/popState are usually called from inside the discarded
// state, so deletion is deferred to the machine's next tick (the sweep at the
// start of processInput/update), after the old state's call frame unwinds.
Describe(GameStateMachineOwnershipSpec) {
  It(should_not_delete_the_discarded_state_until_the_next_tick) {
    GameStateMachine sm;
    int dtors = 0;
    sm.changeState(new TrackedState("A", nullptr, nullptr, nullptr, &dtors));
    sm.changeState(new TrackedState("B"));
    // The old state must survive the changeState call itself — it is usually
    // the caller, and an inline delete would free it mid-call.
    Assert::That(dtors, Equals(0));
    sm.update(); // next tick sweeps
    Assert::That(dtors, Equals(1));
    sm.clean();
  };

  It(should_delete_the_popped_state_on_the_next_tick) {
    GameStateMachine sm;
    int dtors = 0;
    sm.pushState(new TrackedState("A"));
    sm.pushState(new TrackedState("B", nullptr, nullptr, nullptr, &dtors));
    sm.popState();
    Assert::That(dtors, Equals(0)); // deferred
    sm.processInput(); // either tick entry sweeps
    Assert::That(dtors, Equals(1));
    sm.clean();
  };

  It(should_delete_a_duplicate_change_target_immediately) {
    GameStateMachine sm;
    int dtors = 0;
    sm.changeState(new TrackedState("SAME"));
    sm.changeState(new TrackedState("SAME", nullptr, nullptr, nullptr, &dtors));
    // The rejected duplicate was never entered and has no live call frames.
    Assert::That(dtors, Equals(1));
    sm.clean();
  };

  It(should_delete_every_state_on_clean_including_defunct_ones) {
    GameStateMachine sm;
    int dtors = 0;
    sm.changeState(new TrackedState("A", nullptr, nullptr, nullptr, &dtors));
    sm.changeState(new TrackedState("B", nullptr, nullptr, nullptr, &dtors)); // A defunct
    sm.pushState(new TrackedState("C", nullptr, nullptr, nullptr, &dtors));
    sm.clean(); // deletes the stack (B, C) and sweeps the defunct list (A)
    Assert::That(dtors, Equals(3));
    Assert::That(sm.getGameStates().size(), Equals(0));
  };
};

Describe(GameStateMachineCleanSpec) {
  It(should_exit_and_empty_the_stack) {
    GameStateMachine sm;
    int exits = 0;
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
