#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>

#include "states/playState.h"

// Android shell: identical to the desktop platformer's Game except the window
// is fullscreen at the display's native size and the working directory is
// moved to internal storage (where PlatformerActivity extracted the assets).
class Game {
public:
    Game();
    ~Game();

    void Initialize();
    void ProcessInput();
    void Update();
    void Render();
    void Run();
    void Destroy();

private:
    bool isRunning   = false;
    bool isDebugging = false;

    SDL_Window   *window   = nullptr;
    SDL_Renderer *renderer = nullptr;

    GameStateMachine gameStateMachine;
    Logger_Ptr       logger;
    AssetStore_Ptr   assetStore;

    int windowWidth  = 800; // logical size; letterboxed onto the display
    int windowHeight = 480;
};
