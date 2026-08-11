#include "game.h"

#include <SDL2/SDL_image.h>

#include "states/gameOverState.h"
#include "states/menuState.h"
#include "states/playState.h"

Game::Game() {
    assetStore_ = std::make_unique<AssetStore>();
    logger_     = std::make_unique<Logger>();
}

Game::~Game() {}

void Game::LoadAssets() {
    struct Asset { const char *id; const char *path; };
    static const Asset kAssets[] = {
        {"sheet",       "./assets/gfx/sheet.png"},
        {"digits",      "./assets/gfx/ui_digits.png"},
        {"scoreLabel",  "./assets/gfx/ui_score_label.png"},
        {"waveLabel",   "./assets/gfx/ui_wave_label.png"},
        {"menu",        "./assets/gfx/ui_menu.png"},
        {"logo",        "./assets/gfx/ui_logo.png"},
        {"gameOver",    "./assets/gfx/ui_game_over.png"},
        {"getReady",    "./assets/gfx/ui_get_ready.png"},
    };
    // Load once, here, for every state. GetTexture returns nullptr for a
    // missing id rather than throwing, so a bad path is silent at the point of
    // use -- check at load time, where the path is still in scope.
    for (const auto &a : kAssets) {
        assetStore_->AddTexture(renderer_, a.id, a.path);
        if (!assetStore_->GetTexture(a.id)) {
            logger_->Err(std::string("Missing ") + a.path +
                         " -- run from the game root.");
        }
    }
}

void Game::Initialize(StartState start) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        logger_->Err("Error initializing SDL.");
        return;
    }

    window_ = SDL_CreateWindow("1945", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, windowWidth_,
                               windowHeight_, 0);
    if (!window_) {
        logger_->Err("Error creating SDL window.");
        return;
    }

    // Window icon. SDL_SetWindowIcon copies the surface, so it is freed
    // immediately -- holding it would leak. Loaded straight through
    // SDL_image rather than the AssetStore, which deals in textures and
    // cannot hand back a surface.
    if (SDL_Surface *icon = IMG_Load("./assets/gfx/icon.png")) {
        SDL_SetWindowIcon(window_, icon);
        SDL_FreeSurface(icon);
    } else {
        // Not fatal -- the game runs fine with the default icon.
        logger_->Err("Missing ./assets/gfx/icon.png -- run from the game root.");
    }

    renderer_ = SDL_CreateRenderer(
        window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        logger_->Err("Error creating SDL renderer.");
        return;
    }

    LoadAssets();

    // SDL only emits CONTROLLERDEVICEADDED for pads plugged in after init on
    // some platforms, so a controller that was already attached has to be
    // opened explicitly.
    gamepad_.OpenFirstAttached();
    if (gamepad_.Connected()) {
        logger_->Log("Controller: " + gamepad_.Name());
    }

    // The state machine owns every state pointer -- pass `new`-allocated
    // states and never delete them yourself.
    GameState *first = nullptr;
    switch (start) {
    case StartState::Play:
        first = new PlayState(renderer_, windowWidth_, windowHeight_,
                              isDebugging_, assetStore_.get(),
                              &gameStateMachine_, &gamepad_, isRunning_);
        break;
    case StartState::GameOver:
        first = new GameOverState(renderer_, windowWidth_, windowHeight_,
                                  isDebugging_, assetStore_.get(),
                                  &gameStateMachine_, &gamepad_, isRunning_,
                                  12300, 7);
        break;
    case StartState::Menu:
    default:
        first = new MenuState(renderer_, windowWidth_, windowHeight_,
                              isDebugging_, assetStore_.get(),
                              &gameStateMachine_, &gamepad_, isRunning_);
        break;
    }
    gameStateMachine_.changeState(first);

    isRunning_ = true;
}

void Game::Run(StartState start) {
    Initialize(start);
    while (isRunning_) {
        ProcessInput();
        Update();
        Render();
    }
}

void Game::ProcessInput() { gameStateMachine_.processInput(); }
void Game::Update()       { gameStateMachine_.update(); }
void Game::Render()       { gameStateMachine_.render(); }

void Game::Destroy() {
    gameStateMachine_.clean();
    // Before SDL_QuitSubSystem: SDL_GameControllerQuit frees every open
    // controller, so releasing the pad afterwards is a use-after-free.
    gamepad_.Shutdown();
    if (assetStore_) {
        assetStore_->ClearAssets();
    }
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}
