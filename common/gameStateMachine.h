#pragma once

#include <iostream>
#include <vector>

// The slim interface, not the convenience header: this file uses only
// GameState *, so pulling the whole engine in behind it would negate
// gameStateBase.h entirely. A state that includes the slim header and this one
// would still get all 146,775 preprocessed lines back.
#include "states/gameStateBase.h"

namespace storm {

class GameStateMachine {
public:
  GameStateMachine() {}
  ~GameStateMachine() {}

  // Owns raw GameState pointers in two vectors and frees them in clean(). A
  // copy gives two machines holding the same pointers, and the second clean()
  // deletes what the first already freed. The destructor is empty, so this is
  // not a double free at scope exit -- it is a double free the moment both
  // machines tick. Same defect as the networking types (KNOWN_ISSUES item 6);
  // taken in 2.0.0 because it is a one-line fix that otherwise costs a major.
  GameStateMachine(const GameStateMachine &) = delete;
  GameStateMachine &operator=(const GameStateMachine &) = delete;

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
} // namespace storm
