#include "playState.h"
#include <stormengine2/xmlLoader.h>

const std::string PlayState::s_playID = "PLAY";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore,
                     bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{std::move(assetStore)}, isRunning_{isRunning} {
  registry_.AddSystem<MovementSystem>();
  registry_.AddSystem<RenderSystem>();
  registry_.AddSystem<AnimationSystem>();
  registry_.AddSystem<CollisionSystem>();
  registry_.AddSystem<RenderColliderSystem>();

  LoadAssets();
  SpawnPlayer();

  millisecondsPreviousFrame_ = SDL_GetTicks();
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
// Asset loading and initial entity setup
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::LoadAssets() {
  // Load textures defined in the PLAY state of attack.xml
  LoadTexturesFromXml("./assets/attack.xml", "PLAY", "./assets/gfx/", renderer_,
                      assetStore_.get(), &logger_);

  // These assets aren't in attack.xml — load them directly
  assetStore_->AddTexture(renderer_, "enemy2", "./assets/gfx/enemy1.png");
  assetStore_->AddTexture(renderer_, "enemy3", "./assets/gfx/enemy3.png");
  assetStore_->AddTexture(renderer_, "bullet", "./assets/gfx/bullet1.png");
  assetStore_->AddTexture(renderer_, "explosion",
                          "./assets/gfx/smallexplosion.png");
  assetStore_->AddTexture(renderer_, "clouds", "./assets/gfx/clouds.png");
}

void PlayState::SpawnPlayer() {
  // Two tiled cloud layers that scroll left and wrap around
  float cloudScaleX = (float)windowWidth_ / 640.0f;
  float cloudScaleY = (float)windowHeight_ / 480.0f;

  Entity clouds1 = registry_.CreateEntity();
  clouds1.Tag("clouds1");
  clouds1.AddComponent<TransformComponent>(
      glm::vec2(0, 0), glm::vec2(cloudScaleX, cloudScaleY), 0.0);
  clouds1.AddComponent<RigidBodyComponent>(glm::vec2(-20.0, 0.0));
  clouds1.AddComponent<SpriteComponent>("clouds", 640, 480, 0);

  Entity clouds2 = registry_.CreateEntity();
  clouds2.Tag("clouds2");
  clouds2.AddComponent<TransformComponent>(
      glm::vec2(windowWidth_, 0), glm::vec2(cloudScaleX, cloudScaleY), 0.0);
  clouds2.AddComponent<RigidBodyComponent>(glm::vec2(-20.0, 0.0));
  clouds2.AddComponent<SpriteComponent>("clouds", 640, 480, 0);

  // Spawn player and initial enemies from attack.xml PLAY state
  {
    XmlLoader xml("./assets/attack.xml");
    if (!xml.IsValid()) {
      logger_.Err("SpawnPlayer: failed to load attack.xml");
    } else {
      for (const auto &def : xml.GetObjects("PLAY")) {
        if (def.type == "Player") {
          Entity e = registry_.CreateEntity();
          e.Tag("player");
          e.AddComponent<TransformComponent>(glm::vec2(def.x, def.y),
                                             glm::vec2(1.f, 1.f), 0.0);
          e.AddComponent<RigidBodyComponent>(glm::vec2(0.f, 0.f));
          e.AddComponent<SpriteComponent>(def.textureId, def.width, def.height,
                                          def.zIndex > 0 ? def.zIndex : 2);
          e.GetComponent<SpriteComponent>().flip = SDL_FLIP_HORIZONTAL;
          if (def.numFrames > 1)
            e.AddComponent<AnimationComponent>(def.numFrames, 10, false, true);

        } else if (def.type == "Enemy") {
          Entity e = registry_.CreateEntity();
          e.Group("enemies");
          e.AddComponent<TransformComponent>(glm::vec2(def.x, def.y),
                                             glm::vec2(1.f, 1.f), 0.0);
          e.AddComponent<RigidBodyComponent>(glm::vec2(-120.f, 0.f));
          e.AddComponent<SpriteComponent>(def.textureId, def.width, def.height,
                                          def.zIndex > 0 ? def.zIndex : 1);
          if (def.numFrames > 1)
            e.AddComponent<AnimationComponent>(def.numFrames, 10, false, true);
          e.AddComponent<BoxColliderComponent>(def.width, def.height);

        } else {
          logger_.Log("SpawnPlayer: skipping unsupported type '" + def.type +
                      "'");
        }
      }
    }
  }

  registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Entity spawning helpers
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::SpawnBullet() {
  auto &playerT =
      registry_.GetEntityByTag("player").GetComponent<TransformComponent>();

  Entity bullet = registry_.CreateEntity();
  bullet.Group("bullets");
  bullet.AddComponent<TransformComponent>(
      glm::vec2(playerT.position.x + PLAYER_W,
                playerT.position.y + PLAYER_H * 0.4f),
      glm::vec2(1.5, 1.5), 0.0);
  bullet.AddComponent<RigidBodyComponent>(glm::vec2(BULLET_SPEED, 0.0));
  bullet.AddComponent<SpriteComponent>("bullet", BULLET_W, BULLET_H, 2);
  bullet.AddComponent<BoxColliderComponent>(BULLET_W, BULLET_H);
}

void PlayState::SpawnEnemy() {
  int type = rand() % 3;
  int y = rand() % (windowHeight_ - 80) + 20;

  Entity enemy = registry_.CreateEntity();
  enemy.Group("enemies");

  switch (type) {
  case 0: // Green helicopter — animated, same size as player
    enemy.AddComponent<TransformComponent>(glm::vec2(windowWidth_, y),
                                           glm::vec2(1.0, 1.0), 0.0);
    enemy.AddComponent<RigidBodyComponent>(glm::vec2(-ENEMY_SPEED, 0.0));
    enemy.AddComponent<SpriteComponent>("enemy1", ENEMY1_W, ENEMY1_H, 1);
    enemy.AddComponent<AnimationComponent>(ENEMY1_FRAMES, 10, false, true);
    enemy.AddComponent<BoxColliderComponent>(ENEMY1_W, ENEMY1_H);
    break;
  case 1: // Small alien bug — scaled up so it's visible
    enemy.AddComponent<TransformComponent>(glm::vec2(windowWidth_, y),
                                           glm::vec2(2.0, 2.0), 0.0);
    enemy.AddComponent<RigidBodyComponent>(glm::vec2(-ENEMY_SPEED * 1.3f, 0.0));
    enemy.AddComponent<SpriteComponent>("enemy2", ENEMY2_W, ENEMY2_H, 1);
    enemy.AddComponent<BoxColliderComponent>(ENEMY2_W, ENEMY2_H);
    break;
  case 2: // Animated bat — scaled up, slightly slower
    enemy.AddComponent<TransformComponent>(glm::vec2(windowWidth_, y),
                                           glm::vec2(2.0, 2.0), 0.0);
    enemy.AddComponent<RigidBodyComponent>(glm::vec2(-ENEMY_SPEED * 0.8f, 0.0));
    enemy.AddComponent<SpriteComponent>("enemy3", ENEMY3_W, ENEMY3_H, 1);
    enemy.AddComponent<AnimationComponent>(ENEMY3_FRAMES, 6, false, true);
    enemy.AddComponent<BoxColliderComponent>(ENEMY3_W, ENEMY3_H);
    break;
  }
}

void PlayState::DespawnOffscreen() {
  // Wrap cloud layers so they tile seamlessly
  for (const char *tag : {"clouds1", "clouds2"}) {
    try {
      auto &t =
          registry_.GetEntityByTag(tag).GetComponent<TransformComponent>();
      if (t.position.x <= -windowWidth_)
        t.position.x += windowWidth_ * 2;
    } catch (...) {
    }
  }

  if (registry_.DoesGroupExist("bullets")) {
    for (auto entity : registry_.GetEntitiesByGroup("bullets")) {
      if (entity.GetComponent<TransformComponent>().position.x > windowWidth_)
        entity.Kill();
    }
  }

  if (registry_.DoesGroupExist("enemies")) {
    for (auto entity : registry_.GetEntitiesByGroup("enemies")) {
      if (entity.GetComponent<TransformComponent>().position.x < -200)
        entity.Kill();
    }
  }
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
      if (sdlEvent.key.keysym.sym == SDLK_ESCAPE) {
        isRunning_ = false;
        return;
      }
      if (sdlEvent.key.keysym.sym == SDLK_d) {
        isDebugging_ = !isDebugging_;
      }
      break;
    }
  }

  // Continuous movement via keyboard state snapshot
  if (!registry_.EntityHasTag(registry_.GetEntityByTag("player"), "player"))
    return;

  auto &vel =
      registry_.GetEntityByTag("player").GetComponent<RigidBodyComponent>();
  vel.velocity = glm::vec2(0.0, 0.0);

  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  if (keys[SDL_SCANCODE_UP])
    vel.velocity.y = -PLAYER_SPEED;
  if (keys[SDL_SCANCODE_DOWN])
    vel.velocity.y = PLAYER_SPEED;
  if (keys[SDL_SCANCODE_LEFT])
    vel.velocity.x = -PLAYER_SPEED;
  if (keys[SDL_SCANCODE_RIGHT])
    vel.velocity.x = PLAYER_SPEED;

  if (keys[SDL_SCANCODE_SPACE]) {
    int now = SDL_GetTicks();
    if (now - lastBulletTime_ >= BULLET_COOLDOWN_MS) {
      SpawnBullet();
      lastBulletTime_ = now;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::update() {
  int timeToWait =
      MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame_);
  if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME)
    SDL_Delay(timeToWait);

  double deltaTime = (SDL_GetTicks() - millisecondsPreviousFrame_) / 1000.0;
  millisecondsPreviousFrame_ = SDL_GetTicks();

  // Clamp player within screen bounds
  if (registry_.EntityHasTag(registry_.GetEntityByTag("player"), "player")) {
    auto &t =
        registry_.GetEntityByTag("player").GetComponent<TransformComponent>();
    t.position.x = std::max(
        0.0f, std::min(t.position.x, (float)(windowWidth_ - PLAYER_W)));
    t.position.y = std::max(
        0.0f, std::min(t.position.y, (float)(windowHeight_ - PLAYER_H)));
  }

  // Periodic enemy spawning
  int now = SDL_GetTicks();
  if (now - lastEnemySpawn_ >= ENEMY_SPAWN_MS) {
    SpawnEnemy();
    lastEnemySpawn_ = now;
  }

  DespawnOffscreen();

  registry_.Update();
  registry_.GetSystem<MovementSystem>().Update(deltaTime);
  registry_.GetSystem<AnimationSystem>().Update();
  registry_.GetSystem<CollisionSystem>().Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 100, 140, 200, 255);
  SDL_RenderClear(renderer_);

  registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

  if (isDebugging_)
    registry_.GetSystem<RenderColliderSystem>().Update(renderer_);

  SDL_RenderPresent(renderer_);
}
