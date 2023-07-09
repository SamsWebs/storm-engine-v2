#include "game.h"

Game::Game() { logger.Log("Game Constructor called"); }

Game::~Game() { logger.Log("Game Destructor called"); }

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
    logger.Err("Error creating SDL window");
    return;
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    logger.Err("Error creating SDL renderer.");
  }
  SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

  gameStateMachine.changeState(
      new PlayState(renderer, windowWidth, isDebugging));

  isRunning = true;
}

void Game::Run() {
  while (isRunning) {
    ProcessInput();
    Update();
    Render();
  }
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
      } else if (sdlEvent.key.keysym.sym == SDLK_d) {
        isDebugging = !isDebugging;
      }
      break;
    default:
      break;
    }
  }
}

void Game::ProcessInput() { gameStateMachine.processInput(); }

void Game::Update() { gameStateMachine.update(); }

void Game::Render() { gameStateMachine.render(); }

void Game::Destroy() {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
  SDL_Quit();
}