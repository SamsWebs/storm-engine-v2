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
  // Deletes states discarded by changeState/popState. Deferred to the next
  // processInput/update because those calls usually come from INSIDE the
  // state being discarded — an inline delete would free the caller's `this`
  // mid-call (use-after-free).
  void sweepDefunct();

  std::vector<GameState *> m_gameStates;
  std::vector<GameState *> m_defunctStates;
};