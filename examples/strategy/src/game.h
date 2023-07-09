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
#include <stormengine2/logger.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/collision.h>
#include <stormengine2/systems/movement.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>
#include <stormengine2/tilemapLoader.h>

constexpr int FPS = 60;
constexpr int MILLISECS_PER_FRAME = 1000 / FPS;

class Game {
private:
  bool isRunning = false;
  bool isDebugging = false;
  int millisecondsPreviousFrame = 0;
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;

  Registry registry;
  AssetStore assetStore;
  Logger logger;

public:
  Game();
  ~Game();
  void Initialize();
  void ProcessInput();
  void Setup();
  void LoadLevel(int level);
  void Update();
  void Render();
  void Run();
  void Destroy();

  int windowWidth;
  int windowHeight;
};