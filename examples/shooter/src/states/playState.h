#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/glm.hpp>
#include <cstdlib>
#include <algorithm>

#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/rigidBody.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/collision.h>
#include <stormengine2/systems/movement.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>

// ─── Constants ────────────────────────────────────────────────────────────────
constexpr float PLAYER_SPEED        = 200.0f;
constexpr float BULLET_SPEED        = 500.0f;
constexpr float ENEMY_SPEED         = 120.0f;
constexpr int   BULLET_COOLDOWN_MS  = 250;
constexpr int   ENEMY_SPAWN_MS      = 2500;

// Sprite frame sizes
// helicopter.png  = 640×55, 5 horizontal frames of 128×55
constexpr int PLAYER_W = 128; constexpr int PLAYER_H = 55; constexpr int PLAYER_FRAMES = 5;
// helicopter2.png = 640×55, 5 horizontal frames of 128×55
constexpr int ENEMY1_W = 128; constexpr int ENEMY1_H = 55; constexpr int ENEMY1_FRAMES = 5;
constexpr int ENEMY2_W = 38;  constexpr int ENEMY2_H = 34;
constexpr int ENEMY3_W = 52;  constexpr int ENEMY3_H = 35; constexpr int ENEMY3_FRAMES = 2;
constexpr int BULLET_W = 11;  constexpr int BULLET_H = 11;

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
    void SpawnPlayer();
    void SpawnEnemy();
    void SpawnBullet();
    void DespawnOffscreen();

    static const std::string s_playID;

    SDL_Renderer  *renderer_;
    int            windowWidth_, windowHeight_;
    bool          &isDebugging_;
    AssetStore_Ptr assetStore_;
    Logger         logger_;
    bool          &isRunning_;

    Registry registry_;

    int millisecondsPreviousFrame_ = 0;
    int lastBulletTime_            = 0;
    int lastEnemySpawn_            = 0;
};
