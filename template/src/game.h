#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>

class Game {
public:
  Game();

  void Run();
  void Destroy();

private:
  void Initialize();

  bool isRunning_ = false;
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  GameStateMachine stateMachine_;
  AssetStore_Ptr assetStore_;
  int windowWidth_ = 960;
  int windowHeight_ = 540;
};
