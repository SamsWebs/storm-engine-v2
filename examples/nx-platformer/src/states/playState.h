#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>
#include <stormengine2/tilemapLoader.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>

#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "../components/playerComponent.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

// Switch screen is 1280x720; scale tiles up to fill it nicely
constexpr int   TILE_SIZE    = 16;
constexpr float TILE_SCALE   = 2.5f;
constexpr int   TILE_PX      = static_cast<int>(TILE_SIZE * TILE_SCALE); // 40

constexpr int   LEVEL_COLS   = 40;
constexpr int   LEVEL_ROWS   = 28;

constexpr int   PLAYER_W     = 16;
constexpr int   PLAYER_H     = 24;
constexpr float PLAYER_SCALE = 2.0f;

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
    void update()       override;
    void render()       override;
    bool onEnter()      override;
    bool onExit()       override;

    std::string getStateID() const override { return s_playID; }

private:
    void LoadAssets();
    void SpawnTiles();
    void SpawnPlayer();

    bool IsSolid(int col, int row) const;
    void ResolvePlayer(float dt);

    static const std::string s_playID;

    SDL_Renderer  *renderer_;
    int            windowWidth_;
    int            windowHeight_;
    bool           isDebugging_;
    AssetStore_Ptr assetStore_;
    Logger         logger_;
    bool          &isRunning_;

    Registry       registry_;
    glm::vec2      camera_ = {0.0f, 0.0f};

    std::vector<std::vector<bool>> solidGrid_;

    // Input state
    bool moveLeft_  = false;
    bool moveRight_ = false;
    bool jumpPress_ = false;

#ifdef __SWITCH__
    PadState pad_;
#endif

    int millisecondsPreviousFrame_ = 0;
};
