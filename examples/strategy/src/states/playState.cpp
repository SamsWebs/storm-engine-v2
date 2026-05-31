#include "playState.h"

const std::string PlayState::s_playID = "PLAY";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth}, windowHeight_{windowHeight},
      isDebugging_{isDebugging}, assetStore_{std::move(assetStore)},
      isRunning_{isRunning}
{
    registry_.AddSystem<MovementSystem>();
    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<AnimationSystem>();
    registry_.AddSystem<CollisionSystem>();
    registry_.AddSystem<RenderColliderSystem>();

    LoadAssets();
    SpawnEntities();
}

PlayState::~PlayState() { onExit(); }

bool PlayState::onEnter() {
    m_loadingComplete = true;
    return true;
}

bool PlayState::onExit() {
    assetStore_->ClearAssets();
    m_exiting = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Asset loading
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::LoadAssets() {
    assetStore_->AddTexture(renderer_, "tank-image",   "./assets/images/tank-panther-right.png");
    assetStore_->AddTexture(renderer_, "truck-image",  "./assets/images/truck-ford-right.png");
    assetStore_->AddTexture(renderer_, "tile-map",     "./assets/tilemaps/jungle.png");
    assetStore_->AddTexture(renderer_, "chopper-image","./assets/images/chopper.png");
    assetStore_->AddTexture(renderer_, "radar-image",  "./assets/images/radar.png");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entity setup
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::SpawnEntities() {
    constexpr int    tileSize  = 32;
    constexpr double tileScale = 2.5;

    // Load tilemap and create one entity per tile
    TileMapLoader tilemapLoader("./assets/tilemaps/jungle.map",
                                "./assets/tilemaps/jungle.png", tileSize);

    for (const auto &tile : tilemapLoader.getMap()) {
        Entity bg = registry_.CreateEntity();
        bg.AddComponent<TransformComponent>(
            glm::vec2(tileScale * tileSize * tile.relativePosition.x,
                      tileScale * tileSize * tile.relativePosition.y),
            glm::vec2(tileScale, tileScale));
        bg.AddComponent<SpriteComponent>("tile-map", tileSize, tileSize, 0,
                                         tile.pixelSrcPosition.x,
                                         tile.pixelSrcPosition.y);
    }

    // Animated scout chopper (player-controlled in future)
    Entity chopper = registry_.CreateEntity();
    chopper.Tag("chopper");
    chopper.AddComponent<TransformComponent>(glm::vec2(10.0, 100.0), glm::vec2(1.5, 1.5), 0.0);
    chopper.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 0.0));
    chopper.AddComponent<SpriteComponent>("chopper-image", tileSize, tileSize, 3);
    chopper.AddComponent<AnimationComponent>(2, 15, true);

    // Rotating radar dish (fixed to screen, always-on animation)
    Entity radar = registry_.CreateEntity();
    radar.AddComponent<TransformComponent>(glm::vec2(windowWidth_ - 100, 10.0), glm::vec2(1.5, 1.5), 0.0);
    radar.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 0.0));
    radar.AddComponent<SpriteComponent>("radar-image", 64, 64, 2);
    radar.AddComponent<AnimationComponent>(8, 5, true);

    // Enemy tank — moves left, has a box collider
    Entity tank = registry_.CreateEntity();
    tank.Group("enemies");
    tank.AddComponent<TransformComponent>(glm::vec2(500.0, 10.0), glm::vec2(1.5, 1.5), 0.0);
    tank.AddComponent<RigidBodyComponent>(glm::vec2(-30.0, 0.0));
    tank.AddComponent<SpriteComponent>("tank-image", tileSize, tileSize, 2);
    tank.AddComponent<BoxColliderComponent>(32, 32);

    // Friendly truck — moves right, has a box collider
    Entity truck = registry_.CreateEntity();
    truck.Group("friendlies");
    truck.AddComponent<TransformComponent>(glm::vec2(10.0, 10.0), glm::vec2(1.5, 1.5), 0.0);
    truck.AddComponent<RigidBodyComponent>(glm::vec2(20.0, 0.0));
    truck.AddComponent<SpriteComponent>("truck-image", tileSize, tileSize, 1);
    truck.AddComponent<BoxColliderComponent>(32, 32);

    registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// processInput
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::processInput() {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
        case SDL_QUIT:
            isRunning_ = false;
            return;
        case SDL_KEYDOWN:
            switch (sdlEvent.key.keysym.sym) {
            case SDLK_ESCAPE: isRunning_ = false; return;
            case SDLK_d:      isDebugging_ = !isDebugging_; break;
            }
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::update() {
    int timeToWait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);
    if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME)
        SDL_Delay(timeToWait);

    double deltaTime = (SDL_GetTicks() - millisecondsPreviousFrame) / 1000.0;
    millisecondsPreviousFrame = SDL_GetTicks();

    registry_.Update();
    registry_.GetSystem<MovementSystem>().Update(deltaTime);
    registry_.GetSystem<AnimationSystem>().Update();
    registry_.GetSystem<CollisionSystem>().Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::render() {
    SDL_SetRenderDrawColor(renderer_, 21, 21, 21, 255);
    SDL_RenderClear(renderer_);

    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

    if (isDebugging_)
        registry_.GetSystem<RenderColliderSystem>().Update(renderer_);

    SDL_RenderPresent(renderer_);
}
