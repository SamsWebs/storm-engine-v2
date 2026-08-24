#include "game.h"

#ifdef __ANDROID__
#include <unistd.h>
#endif

Game::Game() {
  assetStore = std::make_unique<AssetStore>();
  logger = std::make_unique<Logger>();
  logger->Log("Android platformer constructor called");
}

Game::~Game() { logger->Log("Android platformer destructor called"); }

void Game::Initialize() {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    logger->Err("Error initializing SDL.");
    return;
  }

#ifdef __ANDROID__
  // PlatformerActivity extracted the APK's assets/ into internal storage;
  // chdir there so the game's "./assets/..." paths (including the map file
  // read via std::ifstream) work unchanged.
  const char *internal = SDL_AndroidGetInternalStoragePath();
  if (internal)
    chdir(internal);
#endif

  window = SDL_CreateWindow("Storm Platformer", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight,
                            SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE);
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

  // Letterbox the fixed logical resolution onto whatever the phone has.
  SDL_RenderSetLogicalSize(renderer, windowWidth, windowHeight);

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
  SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
  SDL_Quit();
}
