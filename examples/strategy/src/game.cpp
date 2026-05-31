#include "game.h"

Game::Game() {
    assetStore = std::make_unique<AssetStore>();
    logger     = std::make_unique<Logger>();
    logger->Log("Game constructor called");
}

Game::~Game() { logger->Log("Game destructor called"); }

void Game::Initialize() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        logger->Err("Error initializing SDL.");
        return;
    }

    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    windowWidth  = displayMode.w;
    windowHeight = displayMode.h;

    window = SDL_CreateWindow(NULL,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              windowWidth, windowHeight, SDL_WINDOW_BORDERLESS);
    if (!window) { logger->Err("Error creating SDL window."); return; }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { logger->Err("Error creating SDL renderer."); return; }

    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    gameStateMachine.changeState(
        new PlayState(renderer, windowWidth, windowHeight, isDebugging,
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
void Game::Update()       { gameStateMachine.update(); }
void Game::Render()       { gameStateMachine.render(); }

void Game::Destroy() {
    gameStateMachine.clean();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}
