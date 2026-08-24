#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/logger.h>

#include "world.h"
#include <stormengine2/input/gamepad.h>

// The engine ships no Game class, no main loop and no window management -- only
// GameStateMachine. Every game writes this file.
//
// As in examples/shooter, the AssetStore is owned here for the whole run rather
// than moved into the first state: four states share the same textures, and a
// state that owned the store would clear it on exit and leave the next screen
// blank.
//
// The Campaign is owned here for a sharper reason. OverworldState pushes
// BattleState *on top of itself*, so both are alive at once; the battle reads
// which armies met and writes back who won. Handing the battle a pointer into
// the overworld would tie it to a state that may be mid-teardown, so the shared
// data sits above both of them.
class Game {
public:
  // Which screen the game opens on. Anything but Menu is a testing shortcut.
  enum class StartState { Menu, Overworld, Battle, GameOver };

  Game();
  ~Game();

  void Initialize(StartState start = StartState::Menu);
  void ProcessInput();
  void Update();
  void Render();
  // False when initialisation failed and the game never ran, so main() can
  // exit non-zero.
  bool Run(StartState start = StartState::Menu);
  void Destroy();

private:
  bool LoadAssets();

  bool isRunning_ = false;
  bool isDebugging_ = false;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;

  GameStateMachine gameStateMachine_;
  Gamepad gamepad_;
  Logger_Ptr logger_;
  AssetStore_Ptr assetStore_;
  world::Campaign campaign_;

  // 16x12 tiles of 64px. The overworld map is authored to exactly this size
  // so the whole campaign is visible without a camera.
  int windowWidth_ = 1024;
  int windowHeight_ = 768;
};
