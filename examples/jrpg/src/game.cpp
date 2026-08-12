#include "game.h"

Game::Game() {
    assetStore = std::make_unique<AssetStore>();
    logger     = std::make_unique<Logger>();
    logger->Log("Game constructor called");
}

Game::~Game() { logger->Log("Game destructor called"); }

bool Game::Initialize() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        // SDL_GetError() on every failure path: AssetStore and TileMapLoader
        // both report it, and "Error initializing SDL." on its own tells the
        // user nothing they can act on.
        logger->Err(std::string("Error initializing SDL: ") + SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Storm JRPG",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              windowWidth, windowHeight, 0);
    if (!window) {
        logger->Err(std::string("Error creating SDL window: ") + SDL_GetError());
        return false;
    }

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

    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        // Fall back to whatever SDL can give us, as netplay-checkers does.
        // Accelerated+vsync is unavailable on plenty of remote sessions and
        // headless-ish setups, and a 2D game like this runs fine on software.
        logger->Err(std::string("Accelerated renderer unavailable (")
                    + SDL_GetError() + "); falling back to software.");
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!renderer) {
        logger->Err(std::string("Error creating SDL renderer: ") + SDL_GetError());
        return false;
    }

    auto *play = new PlayState(renderer, windowWidth, windowHeight, isDebugging,
                               std::move(assetStore), isRunning);
    // PlayState reports which asset it could not load. Without this the game
    // opened on an empty green field and exited 0, which reads as success.
    if (!play->AssetsLoaded()) {
        delete play;
        return false;
    }
    gameStateMachine.changeState(play);

    isRunning = true;
    return true;
}

bool Game::Run() {
    if (!Initialize()) {
        return false;
    }
    while (isRunning) {
        ProcessInput();
        Update();
        Render();
    }
    return true;
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
