#include "game.h"
#include <cstdlib>

Game::Game() { logger.Log("Game Constructor called"); }
Game::~Game() { logger.Log("Game Destructor called"); }

void Game::Initialize() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        logger.Err("Error initializing SDL.");
        return;
    }

    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    windowWidth  = displayMode.w;
    windowHeight = displayMode.h;

    window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              windowWidth, windowHeight, SDL_WINDOW_BORDERLESS);
    if (!window) { logger.Err("Error creating SDL window"); return; }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) { logger.Err("Error creating SDL renderer."); return; }

    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    isRunning = true;
}

void Game::LoadLevel(int level) {
    registry.AddSystem<MovementSystem>();
    registry.AddSystem<RenderSystem>();
    registry.AddSystem<AnimationSystem>();
    registry.AddSystem<CollisionSystem>();
    registry.AddSystem<RenderColliderSystem>();

    assetStore.AddTexture(renderer, "player",       "./assets/gfx/helicopter.png");
    assetStore.AddTexture(renderer, "enemy1",       "./assets/gfx/helicopter2.png");
    assetStore.AddTexture(renderer, "enemy2",       "./assets/gfx/enemy1.png");
    assetStore.AddTexture(renderer, "enemy3",       "./assets/gfx/enemy3.png");
    assetStore.AddTexture(renderer, "bullet",       "./assets/gfx/bullet1.png");
    assetStore.AddTexture(renderer, "explosion",    "./assets/gfx/smallexplosion.png");
    assetStore.AddTexture(renderer, "clouds",       "./assets/gfx/clouds.png");

    // Two tiled cloud backgrounds that scroll left; reset when off-screen
    float cloudScaleX = (float)windowWidth  / 640.0f;
    float cloudScaleY = (float)windowHeight / 480.0f;
    Entity clouds = registry.CreateEntity();
    clouds.Tag("clouds1");
    clouds.AddComponent<TransformComponent>(glm::vec2(0, 0), glm::vec2(cloudScaleX, cloudScaleY), 0.0);
    clouds.AddComponent<RigidBodyComponent>(glm::vec2(-20.0, 0.0));
    clouds.AddComponent<SpriteComponent>("clouds", 640, 480, 0);

    Entity clouds2 = registry.CreateEntity();
    clouds2.Tag("clouds2");
    clouds2.AddComponent<TransformComponent>(glm::vec2(windowWidth, 0), glm::vec2(cloudScaleX, cloudScaleY), 0.0);
    clouds2.AddComponent<RigidBodyComponent>(glm::vec2(-20.0, 0.0));
    clouds2.AddComponent<SpriteComponent>("clouds", 640, 480, 0);

    // Player helicopter (z=2, no BoxCollider so it can't be instantly killed)
    Entity player = registry.CreateEntity();
    player.Tag("player");
    player.AddComponent<TransformComponent>(
        glm::vec2(100.0, windowHeight / 2.0 - PLAYER_H),
        glm::vec2(1.0, 1.0), 0.0);
    player.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 0.0));
    player.AddComponent<SpriteComponent>("player", PLAYER_W, PLAYER_H, 2);
    player.GetComponent<SpriteComponent>().flip = SDL_FLIP_HORIZONTAL;
    player.AddComponent<AnimationComponent>(PLAYER_FRAMES, 10, false, true);
}

void Game::SpawnBullet() {
    auto &playerTransform = registry.GetEntityByTag("player").GetComponent<TransformComponent>();

    Entity bullet = registry.CreateEntity();
    bullet.Group("bullets");
    bullet.AddComponent<TransformComponent>(
        glm::vec2(playerTransform.position.x + PLAYER_W,
                  playerTransform.position.y + PLAYER_H * 0.4f),
        glm::vec2(1.5, 1.5), 0.0);
    bullet.AddComponent<RigidBodyComponent>(glm::vec2(BULLET_SPEED, 0.0));
    bullet.AddComponent<SpriteComponent>("bullet", BULLET_W, BULLET_H, 2);
    bullet.AddComponent<BoxColliderComponent>(BULLET_W, BULLET_H);
}

void Game::SpawnEnemy() {
    // Cycle through 3 enemy types, spawn at random vertical position
    int type = rand() % 3;
    int y    = rand() % (windowHeight - 80) + 20;

    Entity enemy = registry.CreateEntity();
    enemy.Group("enemies");

    switch (type) {
    case 0:
        // Green helicopter enemy
        enemy.AddComponent<TransformComponent>(
            glm::vec2(windowWidth, y), glm::vec2(1.0, 1.0), 0.0);
        enemy.AddComponent<RigidBodyComponent>(glm::vec2(-ENEMY_SPEED, 0.0));
        enemy.AddComponent<SpriteComponent>("enemy1", ENEMY1_W, ENEMY1_H, 1);
        enemy.AddComponent<AnimationComponent>(ENEMY1_FRAMES, 10, false, true);
        enemy.AddComponent<BoxColliderComponent>(ENEMY1_W, ENEMY1_H);
        break;
    case 1:
        // Small alien bug enemy (scale up so it's visible)
        enemy.AddComponent<TransformComponent>(
            glm::vec2(windowWidth, y), glm::vec2(2.0, 2.0), 0.0);
        enemy.AddComponent<RigidBodyComponent>(glm::vec2(-ENEMY_SPEED * 1.3f, 0.0));
        enemy.AddComponent<SpriteComponent>("enemy2", ENEMY2_W, ENEMY2_H, 1);
        enemy.AddComponent<BoxColliderComponent>(ENEMY2_W, ENEMY2_H);
        break;
    case 2:
        // Animated bat enemy (scale up so it's visible)
        enemy.AddComponent<TransformComponent>(
            glm::vec2(windowWidth, y), glm::vec2(2.0, 2.0), 0.0);
        enemy.AddComponent<RigidBodyComponent>(glm::vec2(-ENEMY_SPEED * 0.8f, 0.0));
        enemy.AddComponent<SpriteComponent>("enemy3", ENEMY3_W, ENEMY3_H, 1);
        enemy.AddComponent<AnimationComponent>(ENEMY3_FRAMES, 6, false, true);
        enemy.AddComponent<BoxColliderComponent>(ENEMY3_W, ENEMY3_H);
        break;
    }
}

void Game::DespawnOffscreen() {
    // Wrap cloud layers so they tile seamlessly
    for (const char *tag : {"clouds1", "clouds2"}) {
        try {
            auto &t = registry.GetEntityByTag(tag).GetComponent<TransformComponent>();
            if (t.position.x <= -windowWidth) {
                t.position.x += windowWidth * 2;
            }
        } catch (...) {}
    }

    // Kill bullets that fly off the right edge
    if (registry.DoesGroupExist("bullets")) {
        for (auto entity : registry.GetEntitiesByGroup("bullets")) {
            const auto &t = entity.GetComponent<TransformComponent>();
            if (t.position.x > windowWidth) {
                entity.Kill();
            }
        }
    }

    // Kill enemies that scroll off the left edge
    if (registry.DoesGroupExist("enemies")) {
        for (auto entity : registry.GetEntitiesByGroup("enemies")) {
            const auto &t = entity.GetComponent<TransformComponent>();
            if (t.position.x < -200) {
                entity.Kill();
            }
        }
    }
}

void Game::Setup() { LoadLevel(1); }

void Game::Run() {
    Setup();
    while (isRunning) {
        ProcessInput();
        Update();
        Render();
    }
}

void Game::ProcessInput() {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
        case SDL_QUIT:
            isRunning = false;
            break;
        case SDL_KEYDOWN:
            if (sdlEvent.key.keysym.sym == SDLK_ESCAPE)  isRunning = false;
            if (sdlEvent.key.keysym.sym == SDLK_d)       isDebugging = !isDebugging;
            break;
        default: break;
        }
    }

    // Smooth movement via keyboard state
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    if (!registry.EntityHasTag(registry.GetEntityByTag("player"), "player")) return;

    auto &vel = registry.GetEntityByTag("player").GetComponent<RigidBodyComponent>();
    vel.velocity = glm::vec2(0.0, 0.0);

    if (keys[SDL_SCANCODE_UP])    vel.velocity.y = -PLAYER_SPEED;
    if (keys[SDL_SCANCODE_DOWN])  vel.velocity.y =  PLAYER_SPEED;
    if (keys[SDL_SCANCODE_LEFT])  vel.velocity.x = -PLAYER_SPEED;
    if (keys[SDL_SCANCODE_RIGHT]) vel.velocity.x =  PLAYER_SPEED;

    // Shoot
    if (keys[SDL_SCANCODE_SPACE]) {
        int now = SDL_GetTicks();
        if (now - lastBulletTime >= BULLET_COOLDOWN_MS) {
            SpawnBullet();
            lastBulletTime = now;
        }
    }
}

void Game::Update() {
    int timeToWait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);
    if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME) SDL_Delay(timeToWait);

    double deltaTime = (SDL_GetTicks() - millisecondsPreviousFrame) / 1000.0;
    millisecondsPreviousFrame = SDL_GetTicks();

    // Clamp player within screen bounds
    if (registry.EntityHasTag(registry.GetEntityByTag("player"), "player")) {
        auto &t = registry.GetEntityByTag("player").GetComponent<TransformComponent>();
        t.position.x = std::max(0.0f, std::min(t.position.x, (float)(windowWidth  - PLAYER_W)));
        t.position.y = std::max(0.0f, std::min(t.position.y, (float)(windowHeight - PLAYER_H)));
    }

    // Spawn enemies on interval
    int now = SDL_GetTicks();
    if (now - lastEnemySpawn >= ENEMY_SPAWN_MS) {
        SpawnEnemy();
        lastEnemySpawn = now;
    }

    DespawnOffscreen();

    registry.Update();
    registry.GetSystem<MovementSystem>().Update(deltaTime);
    registry.GetSystem<AnimationSystem>().Update();
    registry.GetSystem<CollisionSystem>().Update();
}

void Game::Render() {
    SDL_SetRenderDrawColor(renderer, 100, 140, 200, 255);
    SDL_RenderClear(renderer);

    registry.GetSystem<RenderSystem>().Update(renderer, assetStore);
    if (isDebugging) {
        registry.GetSystem<RenderColliderSystem>().Update(renderer);
    }
    SDL_RenderPresent(renderer);
}

void Game::Destroy() {
    assetStore.ClearAssets();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}
