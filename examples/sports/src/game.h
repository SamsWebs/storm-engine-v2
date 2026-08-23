#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>

#include "states/playState.h"

class Game {
public:
  Game();
  ~Game();

  void Initialize();
  void ProcessInput();
  void Update();
  void Render();
  void Run();
  void Destroy();

private:
  bool isRunning = false;
  bool isDebugging = false;

  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;

  GameStateMachine gameStateMachine;
  Logger_Ptr logger;
  AssetStore_Ptr assetStore;

  int windowWidth = 800;
  int windowHeight = 600;
};
