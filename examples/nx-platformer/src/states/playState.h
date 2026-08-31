#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>
#include <stormengine2/tilemapLoader.h>

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "../components/playerComponent.h"

#ifdef __SWITCH__
#include <switch.h>

using namespace storm;
#endif

// Switch screen is 1280x720; scale tiles up to fill it nicely
constexpr int TILE_SIZE = 16;
constexpr float TILE_SCALE = 2.5f;
constexpr int TILE_PX = static_cast<int>(TILE_SIZE * TILE_SCALE); // 40

constexpr int LEVEL_COLS = 40;
constexpr int LEVEL_ROWS = 28;

// Rabbit sprite strip: 10 idle frames then 8 walk, each PLAYER_W x PLAYER_H.
// Drawn 1:1 -- already the right size beside 40 px tiles.
constexpr int PLAYER_W = 37;
constexpr int PLAYER_H = 57;
constexpr float PLAYER_SCALE = 1.0f;

constexpr int ANIM_IDLE_OFFSET = 0;
constexpr int ANIM_IDLE_FRAMES = 10;
constexpr int ANIM_IDLE_FPS = 6;
constexpr int ANIM_WALK_OFFSET = 10;
constexpr int ANIM_WALK_FRAMES = 8;
constexpr int ANIM_WALK_FPS = 10;

// How long a jump press stays live waiting for the ground.
constexpr Uint32 JUMP_BUFFER_MS = 120;

// On Switch, assets are loaded from romfs
#ifdef __SWITCH__
constexpr const char *ASSET_ROOT = "romfs:/assets/";
#else
constexpr const char *ASSET_ROOT = "./assets/";
#endif

class PlayState : public GameState {
public:
  PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning);
  ~PlayState();

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_playID; }

private:
  void LoadAssets();
  void SpawnTiles();
  void SpawnPlayer();

  bool IsSolid(int col, int row) const;
  void ResolvePlayer(float dt);
  void SetPlayerAnimation(bool walking);

  static const std::string s_playID;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore_Ptr assetStore_;
  Logger logger_;
  bool &isRunning_;

  Registry registry_;
  glm::vec2 camera_ = {0.0f, 0.0f};

  std::vector<std::vector<bool>> solidGrid_;

  // Input state
  bool moveLeft_ = false;
  bool moveRight_ = false;
  bool jumpPress_ = false;
  Uint32 jumpBufferedAt_ = 0; // SDL ticks of the last unconsumed jump press
  bool animWalking_ = false;  // which run of the strip is playing

#ifdef __SWITCH__
  PadState pad_;
#endif
};
