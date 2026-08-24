#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/gameStateMachine.h>
#include <stormengine2/states/gameState.h>

#include "../world.h"
#include <stormengine2/input/gamepad.h>

#include <string>

class GameOverState : public GameState {
public:
  GameOverState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                bool isDebugging, AssetStore *assetStore,
                GameStateMachine *machine, Gamepad *gamepad,
                world::Campaign *campaign, world::Owner winner,
                bool &isRunning);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_gameOverID; }

private:
  static const std::string s_gameOverID;
  static constexpr Uint32 AUTO_RETURN_MS = 8000;

  void ReturnToMenu();

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_;
  GameStateMachine *machine_;
  Gamepad *gamepad_;
  world::Campaign *campaign_;
  world::Owner winner_;
  bool &isRunning_;
  Logger logger_;

  Uint32 enteredMs_ = 0;
  bool leaving_ = false;
};
