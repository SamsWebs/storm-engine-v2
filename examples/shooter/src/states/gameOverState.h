#pragma once

#include <SDL2/SDL.h>
#include <stormengine2/gameStateMachine.h>

#include "../gamepad.h"
#include <stormengine2/states/gameState.h>

#include <string>

class GameOverState : public GameState {
public:
  GameOverState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                bool isDebugging, AssetStore *assetStore,
                GameStateMachine *machine, Gamepad *gamepad, bool &isRunning,
                int finalScore, int wavesSurvived);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_overID; }

private:
  void ToMenu();

  static const std::string s_overID;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_;
  GameStateMachine *machine_;
  Gamepad *gamepad_;
  bool &isRunning_;
  Logger logger_;

  int finalScore_;
  int wavesSurvived_;
  Uint32 enteredAt_ = 0;
  bool leaving_ = false;
};
