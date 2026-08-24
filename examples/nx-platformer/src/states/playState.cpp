#include "playState.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <string>

#ifdef __SWITCH__
#include <switch.h>
#endif

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
  // Before any entity exists: Registry::Update() fixes membership once.
  registry_.AddSystem<AnimationSystem>();

  solidGrid_.assign(LEVEL_ROWS, std::vector<bool>(LEVEL_COLS, false));

#ifdef __SWITCH__
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad_);
#endif

  LoadAssets();
  SpawnTiles();
  SpawnPlayer();

  registry_.Update();

  millisecondsPreviousFrame = SDL_GetTicks();
}

PlayState::~PlayState() { logger_.Log("PlayState destructor"); }

bool PlayState::onEnter() {
  m_loadingComplete = true;
  return true;
}
bool PlayState::onExit() { return true; }

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::LoadAssets() {
  std::string tilesetPath =
      std::string(ASSET_ROOT) + "tilemaps/16x16-platformer.png";
  std::string playerPath = std::string(ASSET_ROOT) + "gfx/rabbit.png";

  logger_.Log("Loading tileset from: " + tilesetPath);
  logger_.Log("Loading player from: " + playerPath);

  assetStore_->AddTexture(renderer_, "16x16-platformer", tilesetPath.c_str());
  assetStore_->AddTexture(renderer_, "rabbit", playerPath.c_str());

  logger_.Log("Assets loaded");
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::SpawnTiles() {
  std::string mapPath = std::string(ASSET_ROOT) + "tilemaps/platformer.map";
  TileMapLoader loader(mapPath.c_str(), "", TILE_SIZE);

  for (const auto &tile : loader.getMap()) {
    int col = tile.relativePosition.x;
    int row = tile.relativePosition.y;

    if (col >= 0 && col < LEVEL_COLS && row >= 0 && row < LEVEL_ROWS)
      solidGrid_[row][col] = true;

    float worldX = static_cast<float>(col * TILE_PX);
    float worldY = static_cast<float>(row * TILE_PX);

    Entity e = registry_.CreateEntity();
    e.Group("tiles");
    e.AddComponent<TransformComponent>(glm::vec2(worldX, worldY),
                                       glm::vec2(TILE_SCALE, TILE_SCALE), 0.0);
    e.AddComponent<SpriteComponent>(tile.assetId, TILE_SIZE, TILE_SIZE,
                                    tile.zIndex, false, tile.pixelSrcPosition.x,
                                    tile.pixelSrcPosition.y);
    e.Group("solid");
    e.AddComponent<BoxColliderComponent>(TILE_PX, TILE_PX);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::SpawnPlayer() {
  float startX = 2 * TILE_PX;
  float startY = 22 * TILE_PX - PLAYER_H * PLAYER_SCALE;

  Entity player = registry_.CreateEntity();
  player.Tag("player");
  player.AddComponent<TransformComponent>(
      glm::vec2(startX, startY), glm::vec2(PLAYER_SCALE, PLAYER_SCALE), 0.0);
  player.AddComponent<SpriteComponent>("rabbit", PLAYER_W, PLAYER_H, 1, 0, 0);
  player.AddComponent<AnimationComponent>(ANIM_IDLE_FRAMES, ANIM_IDLE_FPS,
                                          /*vertical=*/true, /*isLooped=*/true,
                                          ANIM_IDLE_OFFSET);
  player.AddComponent<BoxColliderComponent>(
      static_cast<int>(PLAYER_W * PLAYER_SCALE),
      static_cast<int>(PLAYER_H * PLAYER_SCALE));
  player.AddComponent<PlayerComponent>();
}

// ─────────────────────────────────────────────────────────────────────────────
bool PlayState::IsSolid(int col, int row) const {
  if (col < 0 || col >= LEVEL_COLS || row < 0 || row >= LEVEL_ROWS)
    return true;
  return solidGrid_[row][col];
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::processInput() {
#ifdef __SWITCH__
  // Read Switch controller input
  padUpdate(&pad_);
  u64 kDown = padGetButtonsDown(&pad_);
  u64 kHeld = padGetButtons(&pad_);
  u64 kUp = padGetButtonsUp(&pad_);

  if (kDown & HidNpadButton_Plus) {
    isRunning_ = false;
    return;
  }

  moveLeft_ =
      (kHeld & HidNpadButton_Left) || (kHeld & HidNpadButton_StickLLeft);
  moveRight_ =
      (kHeld & HidNpadButton_Right) || (kHeld & HidNpadButton_StickLRight);
  if (kDown & (HidNpadButton_A | HidNpadButton_Up | HidNpadButton_StickLUp))
    jumpPress_ = true;
  (void)kUp;
#else
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
      case SDLK_LEFT:
      case SDLK_a:
        moveLeft_ = true;
        break;
      case SDLK_RIGHT:
      case SDLK_d:
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
      case SDLK_d:
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
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// The idle and walk runs share one strip, so switching is a change of
// frameOffset/numFrames. The animWalking_ guard keeps startTime from being
// reset every frame, which would pin the animation on frame 0.
void PlayState::SetPlayerAnimation(bool walking) {
  if (walking == animWalking_)
    return;
  animWalking_ = walking;

  Entity player = registry_.GetEntityByTag("player");
  auto &anim = registry_.GetComponent<AnimationComponent>(player);
  anim.frameOffset = walking ? ANIM_WALK_OFFSET : ANIM_IDLE_OFFSET;
  anim.numFrames = walking ? ANIM_WALK_FRAMES : ANIM_IDLE_FRAMES;
  anim.frameSpeedRate = walking ? ANIM_WALK_FPS : ANIM_IDLE_FPS;
  anim.currentFrame = 0;
  anim.startTime = SDL_GetTicks();
}

void PlayState::ResolvePlayer(float dt) {
  Entity player = registry_.GetEntityByTag("player");
  auto &transform = registry_.GetComponent<TransformComponent>(player);
  auto &pc = registry_.GetComponent<PlayerComponent>(player);

  int pw = static_cast<int>(PLAYER_W * PLAYER_SCALE);
  int ph = static_cast<int>(PLAYER_H * PLAYER_SCALE);

  float vx = 0.0f;
  if (moveLeft_) {
    vx = -pc.moveSpeed;
    pc.facingRight = false;
  }
  if (moveRight_) {
    vx = pc.moveSpeed;
    pc.facingRight = true;
  }

  // Jump buffer: remember a press briefly instead of discarding it on the
  // frame it arrives, so pressing just before landing still jumps.
  if (jumpPress_) {
    jumpBufferedAt_ = SDL_GetTicks();
    jumpPress_ = false;
  }
  if (pc.isOnGround && SDL_GetTicks() - jumpBufferedAt_ <= JUMP_BUFFER_MS) {
    pc.velocity.y = pc.jumpSpeed;
    pc.isOnGround = false;
    jumpBufferedAt_ = 0; // consumed
  }

  if (!pc.isOnGround)
    pc.velocity.y += pc.gravity * dt;

  transform.position.x += vx * dt;

  float levelWidth = LEVEL_COLS * TILE_PX;
  transform.position.x =
      std::max(0.0f, std::min(transform.position.x, levelWidth - pw));

  {
    int left = static_cast<int>(transform.position.x) / TILE_PX;
    int right = static_cast<int>(transform.position.x + pw - 1) / TILE_PX;
    int top = static_cast<int>(transform.position.y) / TILE_PX;
    int bot = static_cast<int>(transform.position.y + ph - 1) / TILE_PX;

    if (vx < 0 && (IsSolid(left, top) || IsSolid(left, bot)))
      transform.position.x = (left + 1) * TILE_PX;
    else if (vx > 0 && (IsSolid(right, top) || IsSolid(right, bot)))
      transform.position.x = right * TILE_PX - pw;
  }

  transform.position.y += pc.velocity.y * dt;
  pc.isOnGround = false;

  {
    int left = static_cast<int>(transform.position.x) / TILE_PX;
    int right = static_cast<int>(transform.position.x + pw - 1) / TILE_PX;
    int top = static_cast<int>(transform.position.y) / TILE_PX;
    int bot = static_cast<int>(transform.position.y + ph - 1) / TILE_PX;

    if (pc.velocity.y < 0) {
      if (IsSolid(left, top) || IsSolid(right, top)) {
        transform.position.y = (top + 1) * TILE_PX;
        pc.velocity.y = 0.0f;
      }
    } else {
      if (IsSolid(left, bot) || IsSolid(right, bot)) {
        transform.position.y = bot * TILE_PX - ph;
        pc.velocity.y = 0.0f;
        pc.isOnGround = true;
      }
    }
  }

  // Ground probe, separate from the overlap resolve. Resting snaps the feet
  // to exactly bot*TILE_PX, so the body's last pixel sits in the empty row
  // above the floor and the overlap test reads "airborne" -- isOnGround
  // oscillated to true on about one frame in four, refusing most jumps.
  {
    int left = static_cast<int>(transform.position.x) / TILE_PX;
    int right = static_cast<int>(transform.position.x + pw - 1) / TILE_PX;
    int below = static_cast<int>(transform.position.y + ph) / TILE_PX;
    if (pc.velocity.y >= 0.0f &&
        (IsSolid(left, below) || IsSolid(right, below)))
      pc.isOnGround = true;
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
  // Paces the frame and clamps a hitch to 50ms, as the hand-rolled version did.
  float dt = static_cast<float>(CapFrameRate());

  ResolvePlayer(dt);

  {
    Entity p = registry_.GetEntityByTag("player");
    const auto &pc = registry_.GetComponent<PlayerComponent>(p);
    auto &sprite = registry_.GetComponent<SpriteComponent>(p);
    SetPlayerAnimation(moveLeft_ || moveRight_);
    sprite.flip = pc.facingRight ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
  }
  registry_.GetSystem<AnimationSystem>().Update();

  Entity player = registry_.GetEntityByTag("player");
  auto &transform = registry_.GetComponent<TransformComponent>(player);

  float levelWidth = LEVEL_COLS * TILE_PX;
  float levelHeight = LEVEL_ROWS * TILE_PX;

  float targetX = transform.position.x - windowWidth_ / 2.0f +
                  (PLAYER_W * PLAYER_SCALE) / 2.0f;
  float targetY = transform.position.y - windowHeight_ / 2.0f +
                  (PLAYER_H * PLAYER_SCALE) / 2.0f;

  camera_.x = std::max(0.0f, std::min(targetX, levelWidth - windowWidth_));
  camera_.y = std::max(0.0f, std::min(targetY, levelHeight - windowHeight_));

  registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 99, 155, 255, 255);
  SDL_RenderClear(renderer_);

  auto &renderSys = registry_.GetSystem<RenderSystem>();
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

    if (dstX + dstW < 0 || dstX > windowWidth_)
      continue;
    if (dstY + dstH < 0 || dstY > windowHeight_)
      continue;

    SDL_Rect srcRect = sprite.srcRect;
    SDL_Rect dstRect = {dstX, dstY, dstW, dstH};
    SDL_RenderCopyEx(renderer_, assetStore_->GetTexture(sprite.assetId),
                     &srcRect, &dstRect, transform.rotation, NULL, sprite.flip);
  }

  SDL_RenderPresent(renderer_);
}
