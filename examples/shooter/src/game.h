#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/glm.hpp>
#include <iostream>

#include <stormengine2/assetStore.h>
#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/rigidBody.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/collision.h>
#include <stormengine2/systems/movement.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>

constexpr int FPS                    = 60;
constexpr int MILLISECS_PER_FRAME    = 1000 / FPS;
constexpr float PLAYER_SPEED        = 200.0f;
constexpr float BULLET_SPEED        = 500.0f;
constexpr float ENEMY_SPEED         = 120.0f;
constexpr int BULLET_COOLDOWN_MS    = 250;
constexpr int ENEMY_SPAWN_MS        = 2500;

// Sprite frame sizes (helicopter.png = 640x55, 5 frames of 128x55)
constexpr int PLAYER_W = 128;  constexpr int PLAYER_H = 55;  constexpr int PLAYER_FRAMES = 5;
// helicopter2.png = 640x55, 5 frames of 128x55
constexpr int ENEMY1_W = 128;  constexpr int ENEMY1_H = 55;  constexpr int ENEMY1_FRAMES = 5;
constexpr int ENEMY2_W = 38;   constexpr int ENEMY2_H = 34;
constexpr int ENEMY3_W = 52;   constexpr int ENEMY3_H = 35;  constexpr int ENEMY3_FRAMES = 2;
constexpr int BULLET_W = 11;   constexpr int BULLET_H = 11;
constexpr int EXPL_W   = 20;   constexpr int EXPL_H   = 20;  constexpr int EXPL_FRAMES = 2;

class Game {
private:
    bool isRunning             = false;
    bool isDebugging           = false;
    int  millisecondsPreviousFrame = 0;
    int  lastBulletTime        = 0;
    int  lastEnemySpawn        = 0;

    SDL_Window   *window   = nullptr;
    SDL_Renderer *renderer = nullptr;

    Registry  registry;
    AssetStore assetStore;
    Logger     logger;

    void SpawnEnemy();
    void SpawnBullet();
    void DespawnOffscreen();

public:
    Game();
    ~Game();

    void Initialize();
    void ProcessInput();
    void Setup();
    void LoadLevel(int level);
    void Update();
    void Render();
    void Run();
    void Destroy();

    int windowWidth;
    int windowHeight;
};
