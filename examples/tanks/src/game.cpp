
#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <glm/glm.hpp>

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

glm::vec2 playerPosition;
glm::vec2 playerVelocity;

void Game::Setup() {
  playerPosition = glm::vec2(10.0, 20.0);
  playerVelocity = glm::vec2(100.0, 0.0);
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

  playerPosition.x += playerVelocity.x * deltaTime;
  playerPosition.y += playerVelocity.y * deltaTime;
}

void Game::Render() {
  SDL_SetRenderDrawColor(renderer, 21, 21, 21, 255);
  SDL_RenderClear(renderer);

  // Loads a PNG texture
  SDL_Surface *surface = IMG_Load("./assets/images/tank-tiger-right.png");
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);

  // What is the destination rectangle that we want to place our texture
  SDL_Rect dstRect = {static_cast<int>(playerPosition.x),
                      static_cast<int>(playerPosition.y), 32, 32};
  SDL_RenderCopy(renderer, texture, NULL, &dstRect);
  SDL_DestroyTexture(texture);

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
