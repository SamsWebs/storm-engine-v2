#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdint>
#include <string>

#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>

#include "states/playState.h"

class Game {
public:
  Game(bool host, const std::string &joinAddr, uint16_t port);
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

  int windowWidth = 1100;
  int windowHeight = 600;

  bool host_;
  std::string joinAddr_;
  uint16_t port_;
};
