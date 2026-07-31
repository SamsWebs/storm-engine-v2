#include "game.h"

Game::Game(bool host, const std::string &joinAddr, uint16_t port)
    : host_{host}, joinAddr_{joinAddr}, port_{port} {
  assetStore = std::make_unique<AssetStore>();
  logger = std::make_unique<Logger>();
  logger->Log("Checkers game constructor called");
}

Game::~Game() { logger->Log("Checkers game destructor called"); }

void Game::Initialize() {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    logger->Err("Error initializing SDL.");
    return;
  }

  window =
      SDL_CreateWindow("Storm Checkers", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, 0);
  if (!window) {
    logger->Err("Error creating SDL window.");
    return;
  }

  // Accelerated + vsync where available; fall back to the software
  // renderer so the game also runs headless (SDL_VIDEODRIVER=dummy).
  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, 0);
  }
  if (!renderer) {
    logger->Err("Error creating SDL renderer.");
    return;
  }

  gameStateMachine.changeState(
      new PlayState(renderer, windowWidth, windowHeight, isDebugging,
                    std::move(assetStore), isRunning, host_, joinAddr_, port_));

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
