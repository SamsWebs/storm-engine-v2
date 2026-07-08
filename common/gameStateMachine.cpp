#include "gameStateMachine.h"
#include <iostream>

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
}

void GameStateMachine::processInput() {
  if (!m_gameStates.empty()) {
    m_gameStates.back()->processInput();
  }
}

void GameStateMachine::update() {
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
    delete m_gameStates.back(); // the machine owns the states it holds
    m_gameStates.pop_back();
  }

  if (!m_gameStates.empty()) {
    m_gameStates.back()->resume();
  }
}

void GameStateMachine::changeState(GameState *pState) {
  if (!m_gameStates.empty()) {
    if (m_gameStates.back()->getStateID() == pState->getStateID()) {
      delete pState; // we own what we're handed — don't leak the duplicate
      return;
    }

    m_gameStates.back()->onExit();
    delete m_gameStates.back(); // the machine owns the states it holds
    m_gameStates.pop_back();
  }

  // initialise it
  pState->onEnter();

  // push back our new state
  m_gameStates.push_back(pState);
}
