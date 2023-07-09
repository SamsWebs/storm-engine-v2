
#include <igloo/igloo_alt.h>

#include "../../common/gameStateMachine.h"
#include "../../common/states/gameState.h"

using namespace igloo;

class MockGameState : public GameState {
public:
  void update() override {}
  void render() override {}
  bool onEnter() override { return true; }
  bool onExit() override { return true; }
  std::string getStateID() const override { return ""; }
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