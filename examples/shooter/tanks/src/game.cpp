#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/glm.hpp>

#include <stormengine2/ecs.h>

#include "game.h"

Game::Game() {
  isRunning = false;
  logger.Log("Game constructor called!");
}

Game::~Game() { logger.Log("Game destructor called!"); }

void Game::Initialize() {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    logger.Err("Error initializing SDL.");
    return;
  }
  SDL_DisplayMode displayMode;
  SDL_GetCurrentDisplayMode(0, &displayMode);
  windowWidth = displayMode.w;
  windowHeight = displayMode.h;
  window =
      SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       windowWidth, windowHeight, SDL_WINDOW_BORDERLESS);
  if (!window) {
    logger.Err("Error creating SDL window.");
    return;
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    logger.Err("Error creating SDL renderer.");
    return;
  }
  SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
  isRunning = true;
}

void Game::ProcessInput() {
  SDL_Event sdlEvent;
  while (SDL_PollEvent(&sdlEvent)) {
    switch (sdlEvent.type) {
    case SDL_QUIT:
      isRunning = false;
      break;
    case SDL_KEYDOWN:
      if (sdlEvent.key.keysym.sym == SDLK_ESCAPE) {
        isRunning = false;
      }
      break;
    }
  }
}

void Game::Setup() {
  // TODO:
  // Entity tank = registry.CreateEntity();
  // tank.AddComponent<TransformComponent>();
  // tank.AddComponent<BoxColliderComponent>();
  // tank.AddComponent<SpriteComponent>("./assets/images/tank.png");
}

void Game::Update() {
  // If we are too fast, waste some time until we reach the MILLISECS_PER_FRAME
  int timeToWait =
      MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecsPreviousFrame);
  if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME) {
    SDL_Delay(timeToWait);
  }

  // The difference in ticks since the last frame, converted to seconds
  double deltaTime = (SDL_GetTicks() - millisecsPreviousFrame) / 1000.0;

  // Store the "previous" frame time
  millisecsPreviousFrame = SDL_GetTicks();

  // TODO:
  // MovementSystem.Update();
  // CollisionSystem.Update();
  // DamageSystem.Update();
}

void Game::Render() {
  SDL_SetRenderDrawColor(renderer, 21, 21, 21, 255);
  SDL_RenderClear(renderer);

  // TODO: Render game objects...

  SDL_RenderPresent(renderer);
}

void Game::Run() {
  Setup();
  while (isRunning) {
    ProcessInput();
    Update();
    Render();
  }
}

void Game::Destroy() {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}