#include "playState.h"

#include <SDL2/SDL.h>
#include <algorithm>

const std::string PlayState::s_playID = "PLAY";

// ─────────────────────────────────────────────────────────────────────────────
PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore,
                     bool &isRunning)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(std::move(assetStore)), isRunning_(isRunning) {
  logger_.Log("PlayState constructor");

  registry_.AddSystem<RenderSystem>();
  registry_.AddSystem<RenderColliderSystem>();

  // Initialise solid grid to all false
  solidGrid_.assign(LEVEL_ROWS, std::vector<bool>(LEVEL_COLS, false));

  LoadAssets();
  SpawnTiles();
  SpawnPlayer();


  registry_.Update();

  millisecondsPreviousFrame_ = SDL_GetTicks();
}

PlayState::~PlayState() { logger_.Log("PlayState destructor"); }

bool PlayState::onEnter() {
  m_loadingComplete = true;
  return true;
}
bool PlayState::onExit() { return true; }

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::LoadAssets() {
  // The asset ID stored in the map file is "16x16-platformer"
  assetStore_->AddTexture(renderer_, "16x16-platformer",
                          "./assets/tilemaps/16x16-platformer.png");
  assetStore_->AddTexture(renderer_, "player", "./assets/gfx/player.png");
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::SpawnTiles() {
  // The editor map is space-separated; TileMapLoader auto-detects the format.
  // No PNG needed — srcX/srcY are embedded in each map line.
  TileMapLoader loader("./assets/tilemaps/platformer.map", "", TILE_SIZE);

  for (const auto &tile : loader.getMap()) {
    int col = tile.relativePosition.x;
    int row = tile.relativePosition.y;

    // All tiles painted in the editor are treated as solid ground
    if (col >= 0 && col < LEVEL_COLS && row >= 0 && row < LEVEL_ROWS)
      solidGrid_[row][col] = true;

    float worldX = static_cast<float>(col * TILE_PX);
    float worldY = static_cast<float>(row * TILE_PX);

    Entity e = registry_.CreateEntity();
    e.Group("tiles");
    e.AddComponent<TransformComponent>(glm::vec2(worldX, worldY),
                                       glm::vec2(TILE_SCALE, TILE_SCALE), 0.0);
    e.AddComponent<SpriteComponent>(tile.assetId, TILE_SIZE, TILE_SIZE,
                                    tile.zIndex, false,
                                    tile.pixelSrcPosition.x,
                                    tile.pixelSrcPosition.y);
    e.Group("solid");
    e.AddComponent<BoxColliderComponent>(TILE_PX, TILE_PX);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::SpawnPlayer() {
  // Start just above the row-22 starting platform
  float startX = 2 * TILE_PX;
  float startY = 22 * TILE_PX - PLAYER_H * PLAYER_SCALE;

  Entity player = registry_.CreateEntity();
  player.Tag("player");
  player.AddComponent<TransformComponent>(
      glm::vec2(startX, startY), glm::vec2(PLAYER_SCALE, PLAYER_SCALE), 0.0);
  player.AddComponent<SpriteComponent>("player", PLAYER_W, PLAYER_H, 1, 0, 0);
  player.AddComponent<BoxColliderComponent>(
      static_cast<int>(PLAYER_W * PLAYER_SCALE),
      static_cast<int>(PLAYER_H * PLAYER_SCALE));
  player.AddComponent<PlayerComponent>();
}

// ─────────────────────────────────────────────────────────────────────────────
bool PlayState::IsSolid(int col, int row) const {
  if (col < 0 || col >= LEVEL_COLS || row < 0 || row >= LEVEL_ROWS)
    return true; // treat out-of-bounds as solid walls/floor
  return solidGrid_[row][col];
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::processInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      isRunning_ = false;
      return;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
        isRunning_ = false;
        return;
      case SDLK_d:
        isDebugging_ = !isDebugging_;
        break;
      case SDLK_LEFT:
      case SDLK_a:
        moveLeft_ = true;
        break;
      case SDLK_RIGHT:
        moveRight_ = true;
        break;
      case SDLK_SPACE:
      case SDLK_UP:
      case SDLK_w:
        jumpPress_ = true;
        break;
      default:
        break;
      }
      break;
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
      case SDLK_LEFT:
      case SDLK_a:
        moveLeft_ = false;
        break;
      case SDLK_RIGHT:
        moveRight_ = false;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::ResolvePlayer(float dt) {
  Entity player = registry_.GetEntityByTag("player");
  auto &transform = registry_.GetComponent<TransformComponent>(player);
  auto &pc = registry_.GetComponent<PlayerComponent>(player);

  int pw = static_cast<int>(PLAYER_W * PLAYER_SCALE); // 32
  int ph = static_cast<int>(PLAYER_H * PLAYER_SCALE); // 48

  // ── Horizontal input ──────────────────────────────────────────────────────
  float vx = 0.0f;
  if (moveLeft_) {
    vx = -pc.moveSpeed;
    pc.facingRight = false;
  }
  if (moveRight_) {
    vx = pc.moveSpeed;
    pc.facingRight = true;
  }

  // ── Jump ──────────────────────────────────────────────────────────────────
  if (jumpPress_ && pc.isOnGround) {
    pc.velocity.y = pc.jumpSpeed;
    pc.isOnGround = false;
  }
  jumpPress_ = false;

  // ── Gravity ───────────────────────────────────────────────────────────────
  if (!pc.isOnGround)
    pc.velocity.y += pc.gravity * dt;

  // ── Move horizontally then resolve ────────────────────────────────────────
  transform.position.x += vx * dt;

  // Clamp to level bounds
  float levelWidth = LEVEL_COLS * TILE_PX;
  transform.position.x =
      std::max(0.0f, std::min(transform.position.x, levelWidth - pw));

  // Horizontal tile collision
  {
    int left = static_cast<int>(transform.position.x) / TILE_PX;
    int right = static_cast<int>(transform.position.x + pw - 1) / TILE_PX;
    int top = static_cast<int>(transform.position.y) / TILE_PX;
    int bot = static_cast<int>(transform.position.y + ph - 1) / TILE_PX;

    if (vx < 0) { // moving left
      if (IsSolid(left, top) || IsSolid(left, bot))
        transform.position.x = (left + 1) * TILE_PX;
    } else if (vx > 0) { // moving right
      if (IsSolid(right, top) || IsSolid(right, bot))
        transform.position.x = right * TILE_PX - pw;
    }
  }

  // ── Move vertically then resolve ──────────────────────────────────────────
  transform.position.y += pc.velocity.y * dt;

  pc.isOnGround = false;

  {
    int left = static_cast<int>(transform.position.x) / TILE_PX;
    int right = static_cast<int>(transform.position.x + pw - 1) / TILE_PX;
    int top = static_cast<int>(transform.position.y) / TILE_PX;
    int bot = static_cast<int>(transform.position.y + ph - 1) / TILE_PX;

    if (pc.velocity.y < 0) { // moving up
      if (IsSolid(left, top) || IsSolid(right, top)) {
        transform.position.y = (top + 1) * TILE_PX;
        pc.velocity.y = 0.0f;
      }
    } else { // moving down / idle
      if (IsSolid(left, bot) || IsSolid(right, bot)) {
        transform.position.y = bot * TILE_PX - ph;
        pc.velocity.y = 0.0f;
        pc.isOnGround = true;
      }
    }
  }

  // Fall off bottom → respawn
  if (transform.position.y > LEVEL_ROWS * TILE_PX + 100) {
    transform.position = {2.0f * TILE_PX, 10.0f * TILE_PX};
    pc.velocity = {0.0f, 0.0f};
    pc.isOnGround = false;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::update() {
  int timeToWait =
      MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame_);
  if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME)
    SDL_Delay(timeToWait);

  float dt = (SDL_GetTicks() - millisecondsPreviousFrame_) / 1000.0f;
  if (dt > 0.05f)
    dt = 0.05f;
  millisecondsPreviousFrame_ = SDL_GetTicks();

  ResolvePlayer(dt);

  // ── Camera: follow player horizontally, clamped to level ─────────────────
  Entity player = registry_.GetEntityByTag("player");
  auto &transform = registry_.GetComponent<TransformComponent>(player);

  float levelWidth  = LEVEL_COLS * TILE_PX;
  float levelHeight = LEVEL_ROWS * TILE_PX;

  float targetX = transform.position.x - windowWidth_  / 2.0f + (PLAYER_W * PLAYER_SCALE) / 2.0f;
  float targetY = transform.position.y - windowHeight_ / 2.0f + (PLAYER_H * PLAYER_SCALE) / 2.0f;

  camera_.x = std::max(0.0f, std::min(targetX, levelWidth  - windowWidth_));
  camera_.y = std::max(0.0f, std::min(targetY, levelHeight - windowHeight_));

  registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 99, 155, 255, 255); // sky blue
  SDL_RenderClear(renderer_);

  // Camera-offset render: iterate entities sorted by zIndex
  auto &renderSys = registry_.GetSystem<RenderSystem>();

  // Sort manually the same way RenderSystem does
  auto entities = renderSys.GetSystemEntities();
  std::sort(entities.begin(), entities.end(),
            [](const Entity &a, const Entity &b) {
              return a.GetComponent<SpriteComponent>().zIndex <
                     b.GetComponent<SpriteComponent>().zIndex;
            });

  for (auto &entity : entities) {
    const auto &transform = entity.GetComponent<TransformComponent>();
    const auto &sprite = entity.GetComponent<SpriteComponent>();

    int dstX = static_cast<int>(transform.position.x - camera_.x);
    int dstY = static_cast<int>(transform.position.y - camera_.y);
    int dstW = static_cast<int>(sprite.width * transform.scale.x);
    int dstH = static_cast<int>(sprite.height * transform.scale.y);

    // Cull tiles that are completely off-screen
    if (dstX + dstW < 0 || dstX > windowWidth_)
      continue;
    if (dstY + dstH < 0 || dstY > windowHeight_)
      continue;

    SDL_Rect srcRect = sprite.srcRect;
    SDL_Rect dstRect = {dstX, dstY, dstW, dstH};
    SDL_RenderCopyEx(renderer_, assetStore_->GetTexture(sprite.assetId),
                     &srcRect, &dstRect, transform.rotation, NULL, sprite.flip);
  }

  // Debug colliders (offset by camera)
  if (isDebugging_) {
    SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
    for (auto &entity : registry_.GetEntitiesByGroup("solid")) {
      const auto &t = entity.GetComponent<TransformComponent>();
      const auto &c = entity.GetComponent<BoxColliderComponent>();
      SDL_Rect r = {static_cast<int>(t.position.x - camera_.x),
                    static_cast<int>(t.position.y - camera_.y), c.width,
                    c.height};
      SDL_RenderDrawRect(renderer_, &r);
    }
    // Player collider
    Entity player = registry_.GetEntityByTag("player");
    const auto &pt = registry_.GetComponent<TransformComponent>(player);
    const auto &pc = registry_.GetComponent<BoxColliderComponent>(player);
    SDL_SetRenderDrawColor(renderer_, 0, 255, 0, 255);
    SDL_Rect pr = {static_cast<int>(pt.position.x - camera_.x),
                   static_cast<int>(pt.position.y - camera_.y), pc.width,
                   pc.height};
    SDL_RenderDrawRect(renderer_, &pr);
  }

  SDL_RenderPresent(renderer_);
}
