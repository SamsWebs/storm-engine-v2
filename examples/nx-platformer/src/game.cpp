#include "game.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

Game::Game() {
  assetStore = std::make_unique<AssetStore>();
  logger = std::make_unique<Logger>();
  logger->Log("Game constructor called");
}

Game::~Game() { logger->Log("Game destructor called"); }

void Game::Initialize() {
#ifdef __SWITCH__
  Result rc = romfsInit();
  if (R_FAILED(rc)) {
    logger->Err("romfsInit failed! Assets will not load.");
  } else {
    logger->Log("romfsInit succeeded.");
  }
#endif

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
    logger->Err("Error initializing SDL.");
    return;
  }

  Uint32 windowFlags = SDL_WINDOW_SHOWN;
#ifdef __SWITCH__
  windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif

  window = SDL_CreateWindow("Storm Platformer", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight,
                            windowFlags);
  if (!window) {
    logger->Err("Error creating SDL window.");
    return;
  }

  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    logger->Err("Error creating SDL renderer.");
    return;
  }

  gameStateMachine.changeState(new PlayState(renderer, windowWidth,
                                             windowHeight, isDebugging,
                                             std::move(assetStore), isRunning));

  isRunning = true;
}

void Game::Run() {
  Initialize();
  while (isRunning) {
    ProcessInput();
    Update();
    Render();
  }
}

void Game::ProcessInput() { gameStateMachine.processInput(); }
void Game::Update() { gameStateMachine.update(); }
void Game::Render() { gameStateMachine.render(); }

void Game::Destroy() {
  gameStateMachine.clean();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
  SDL_Quit();

#ifdef __SWITCH__
  romfsExit();
#endif
}
