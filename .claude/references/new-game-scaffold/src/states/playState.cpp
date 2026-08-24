#include "playState.h"

const std::string PlayState::s_playID = "PLAY_STATE";

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore,
                     bool &isRunning)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(std::move(assetStore)), isRunning_(isRunning) {}

PlayState::~PlayState() {}

// Initialize in onEnter(), not the constructor. changeState() calls onEnter()
// after pushing the state; clean() calls onExit() before deleting it.
bool PlayState::onEnter() {
    LoadAssets();

    // Register systems BEFORE creating entities. AddSystem<T>() only constructs
    // and registers -- it never scans existing entities, so a system added
    // after the entities were flushed starts empty and stays empty.
    registry_.AddSystem<MovementSystem>();
    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<AnimationSystem>();

    SpawnPlayer();

    millisecondsPreviousFrame_ = SDL_GetTicks();
    return true;
}

// onExit() must be idempotent -- it can run twice (state machine call, then
// destructor).
bool PlayState::onExit() {
    if (assetStore_) {
        assetStore_->ClearAssets();
    }
    return true;
}

void PlayState::LoadAssets() {
    // Placeholder art ships with the scaffold: assets/gfx/player.png is a
    // HORIZONTAL strip of 4 frames, 32x32 each (128x32 total), which is what
    // AnimationComponent(4, 10, /*vertical=*/false, true) expects -- see
    // SpawnPlayer. GetTexture returns nullptr for a missing id rather than
    // throwing, so a missing file is silent at the point of use; check here,
    // where the path is still in scope.
    assetStore_->AddTexture(renderer_, "player", "./assets/gfx/player.png");
    if (!assetStore_->GetTexture("player")) {
        logger_.Err("Missing ./assets/gfx/player.png -- run from the game root.");
    }

    assetStore_->AddTexture(renderer_, "tiles", "./assets/gfx/tileset.png");
    if (!assetStore_->GetTexture("tiles")) {
        logger_.Err("Missing ./assets/gfx/tileset.png -- run from the game root.");
    }
}

void PlayState::SpawnPlayer() {
    Entity player = registry_.CreateEntity();
    player.Tag("player");
    player.AddComponent<TransformComponent>(glm::vec2(100.0f, 300.0f),
                                            glm::vec2(1.0f, 1.0f), 0.0);
    player.AddComponent<RigidBodyComponent>(glm::vec2(0.0f, 0.0f));
    player.AddComponent<SpriteComponent>("player", 32, 32, 1);
    // vertical=false advances srcRect.x, so the sheet must be a HORIZONTAL
    // strip. Passing true here with a horizontal sheet does not error -- it
    // just samples past the bottom of the texture and draws nothing.
    player.AddComponent<AnimationComponent>(4, 10, false, true);

    player_ = player;
}

// The active state owns ALL event polling. Never call SDL_PollEvent in both
// Game::ProcessInput and a state's processInput -- the queue is shared.
void PlayState::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            isRunning_ = false;
            return;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                isRunning_ = false;
                return;
            }
            if (event.key.keysym.sym == SDLK_LEFT)  moveLeft_  = true;
            if (event.key.keysym.sym == SDLK_RIGHT) moveRight_ = true;
            break;
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_LEFT)  moveLeft_  = false;
            if (event.key.keysym.sym == SDLK_RIGHT) moveRight_ = false;
            break;
        default:
            break;
        }
    }
}

void PlayState::update() {
    // Variable dt with a 60 FPS cap. Nothing enforces a minimum frame rate.
    //
    // On engine v1.3.0+ this whole block, and the millisecondsPreviousFrame_
    // member shadowing GameState's own, collapse to:
    //
    //     const double deltaTime = CapFrameRate();
    //
    // which also clamps a hitch. It is spelled out here because the scaffold
    // compiles against any 1.x install; see README.md, "Engine version".
    int wait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame_);
    if (wait > 0 && wait <= MILLISECS_PER_FRAME) {
        SDL_Delay(wait);
    }
    double deltaTime = (SDL_GetTicks() - millisecondsPreviousFrame_) / 1000.0;
    millisecondsPreviousFrame_ = SDL_GetTicks();

    // Flush deferred entity adds/kills FIRST, before running any system.
    // Entity creation and destruction are batched, not immediate.
    registry_.Update();

    if (player_) {
        auto &rb = player_->GetComponent<RigidBodyComponent>();
        rb.velocity.x = (moveRight_ ? 120.0f : 0.0f) - (moveLeft_ ? 120.0f : 0.0f);
    }

    // Each concrete system declares its own non-virtual Update with a bespoke
    // signature. There is no scheduler -- the state calls them by name, in an
    // order it chooses.
    registry_.GetSystem<MovementSystem>().Update(deltaTime);
    registry_.GetSystem<AnimationSystem>().Update();
}

void PlayState::render() {
    SDL_SetRenderDrawColor(renderer_, 21, 21, 21, 255);
    SDL_RenderClear(renderer_);

    // The camera is an optional third parameter defaulting to nullptr, which
    // renders in world coordinates. It is omitted rather than passed as an
    // explicit nullptr so this call also compiles against pre-v1.2.1 headers,
    // where Update took only two arguments. Pass &camera here to scroll.
    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

    // Note the asymmetry: RenderSystem::Update takes a camera, but
    // RenderColliderSystem::Update takes only the renderer -- the debug
    // overlay is not camera-aware.
    if (isDebugging_) {
        registry_.GetSystem<RenderColliderSystem>().Update(renderer_);
    }

    SDL_RenderPresent(renderer_);
}
