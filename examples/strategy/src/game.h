#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/rigidBody.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/collision.h>
#include <stormengine2/systems/movement.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>
#include <stormengine2/tilemapLoader.h>

//#include "states/gameOverState.h"
#include "states/mainMenuState.h"
#include "states/pauseState.h"
#include "states/playState.h"

class Game {
private:
  bool isRunning = false;
  bool isDebugging = false;
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;

  Logger logger;
  GameStateMachine gameStateMachine;

public:
  Game();
  ~Game();
  void Initialize();
  void ProcessInput();
  void Update();
  void Render();
  void Run();
  void Destroy();

  int windowWidth;
  int windowHeight;
};