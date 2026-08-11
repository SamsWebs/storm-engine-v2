#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>

#include "gamepad.h"

// The engine ships no Game class, no main loop and no window management --
// only GameStateMachine. Every game writes this file.
//
// Note the ownership change from the scaffold: the AssetStore is NOT moved
// into the first state. Three states share the same textures, and a state that
// owned the store would clear it on exit and leave the next state blank. Game
// owns it for the whole run and hands out a raw pointer.
class Game {
public:
    // Which state the game opens on. Anything but Menu is a testing shortcut.
    enum class StartState { Menu, Play, GameOver };

    Game();
    ~Game();

    void Initialize(StartState start = StartState::Menu);
    void ProcessInput();
    void Update();
    void Render();
    void Run(StartState start = StartState::Menu);
    void Destroy();

private:
    void LoadAssets();

    bool isRunning_   = false;
    bool isDebugging_ = false;

    SDL_Window   *window_   = nullptr;
    SDL_Renderer *renderer_ = nullptr;

    GameStateMachine gameStateMachine_;
    Gamepad          gamepad_;
    Logger_Ptr       logger_;
    AssetStore_Ptr   assetStore_;

    int windowWidth_  = 800;
    int windowHeight_ = 600;
};
