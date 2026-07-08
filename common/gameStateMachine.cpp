#include "gameStateMachine.h"
#include <iostream>

// States discarded by changeState/popState land in m_defunctStates instead of
// being deleted inline: those calls usually come from inside the discarded
// state's own processInput/update, and deleting there would free the caller's
// `this` while it is still on the stack. The sweep runs at the start of the
// machine's next tick, after the old state's call frame has fully unwound.
void GameStateMachine::sweepDefunct() {
  for (GameState *state : m_defunctStates) {
    delete state;
  }
  m_defunctStates.clear();
}

void GameStateMachine::clean() {
  // The machine owns every state on the stack — exit the active one, then
  // delete them all (not just the top; a pushed stack would otherwise leak).
  if (!m_gameStates.empty()) {
    m_gameStates.back()->onExit();
  }
  for (GameState *state : m_gameStates) {
    delete state;
  }
  m_gameStates.clear();
  sweepDefunct();
}

void GameStateMachine::processInput() {
  sweepDefunct();
  if (!m_gameStates.empty()) {
    m_gameStates.back()->processInput();
  }
}

void GameStateMachine::update() {
  sweepDefunct();
  if (!m_gameStates.empty()) {
    m_gameStates.back()->update();
  }
}

void GameStateMachine::render() {
  if (!m_gameStates.empty()) {
    m_gameStates.back()->render();
  }
}

void GameStateMachine::pushState(GameState *pState) {
  m_gameStates.push_back(pState);
  m_gameStates.back()->onEnter();
}

void GameStateMachine::popState() {
  if (!m_gameStates.empty()) {
    m_gameStates.back()->onExit();
    m_defunctStates.push_back(m_gameStates.back()); // deleted on the next tick
    m_gameStates.pop_back();
  }

  if (!m_gameStates.empty()) {
    m_gameStates.back()->resume();
  }
}

void GameStateMachine::changeState(GameState *pState) {
  if (!m_gameStates.empty()) {
    if (m_gameStates.back()->getStateID() == pState->getStateID()) {
      delete pState; // never entered, no live call frames — safe to free now
      return;
    }

    m_gameStates.back()->onExit();
    m_defunctStates.push_back(m_gameStates.back()); // deleted on the next tick
    m_gameStates.pop_back();
  }

  // initialise it
  pState->onEnter();

  // push back our new state
  m_gameStates.push_back(pState);
}
