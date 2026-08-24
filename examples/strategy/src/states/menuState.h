#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/gameStateMachine.h>
#include <stormengine2/states/gameState.h>

#include "../world.h"
#include <stormengine2/input/gamepad.h>

#include <string>

class MenuState : public GameState {
public:
  MenuState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore *assetStore, GameStateMachine *machine,
            Gamepad *gamepad, world::Campaign *campaign, bool &isRunning);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_menuID; }

private:
  static const std::string s_menuID;
  static constexpr int MENU_COUNT = 2;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_;
  GameStateMachine *machine_;
  Gamepad *gamepad_;
  world::Campaign *campaign_;
  bool &isRunning_;
  Logger logger_;

  int selected_ = 0;
  bool leaving_ = false; // a changeState is already queued
  Uint32 enteredMs_ = 0;
};
