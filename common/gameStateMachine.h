#pragma once

#include <iostream>
#include <vector>

#include "states/gameState.h"

class GameStateMachine {
public:
  GameStateMachine() {}
  ~GameStateMachine() {}

  void processInput();
  void update();
  void render();

  void pushState(GameState *pState);
  void changeState(GameState *pState);
  void popState();

  void clean();

  std::vector<GameState *> &getGameStates() { return m_gameStates; }

private:
  std::vector<GameState *> m_gameStates;
};