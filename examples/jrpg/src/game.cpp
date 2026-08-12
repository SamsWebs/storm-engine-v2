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

    window = SDL_CreateWindow("Storm JRPG",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              windowWidth, windowHeight, 0);
    if (!window) { logger->Err("Error creating SDL window."); return; }

    // Window icon: the player's idle-down frame, head and torso, cut from the
    // sprite sheet. SDL_SetWindowIcon copies the surface, so it is freed
    // straight away -- holding it would leak. Loaded through SDL_image rather
    // than the AssetStore, which deals in textures and cannot return a surface.
    if (SDL_Surface *icon = IMG_Load("./assets/gfx/icon.png")) {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    } else {
        // Not fatal -- the game runs fine with the window manager's default.
        logger->Err("Missing ./assets/gfx/icon.png -- run from the game root.");
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { logger->Err("Error creating SDL renderer."); return; }

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
