#include "playState.h"

#include <SDL2/SDL.h>
#include <algorithm>

const std::string PlayState::s_playID = "PLAY";

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore,
                     bool &isRunning)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(std::move(assetStore)), isRunning_(isRunning) {
  logger_.Log("PlayState constructor");

  registry_.AddSystem<RenderSystem>();
  registry_.AddSystem<RenderColliderSystem>();

  solidGrid_.assign(LEVEL_ROWS, std::vector<bool>(LEVEL_COLS, false));
  zones_ = MakeDefaultZones(static_cast<float>(windowWidth_),
                            static_cast<float>(windowHeight_));

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

void PlayState::LoadAssets() {
  assetStore_->AddTexture(renderer_, "16x16-platformer",
                          "./assets/tilemaps/16x16-platformer.png");
  assetStore_->AddTexture(renderer_, "player", "./assets/gfx/player.png");
}

void PlayState::SpawnTiles() {
  TileMapLoader loader("./assets/tilemaps/platformer.map", "", TILE_SIZE);

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
                                    tile.zIndex, false,
                                    tile.pixelSrcPosition.x,
                                    tile.pixelSrcPosition.y);
    e.Group("solid");
    e.AddComponent<BoxColliderComponent>(TILE_PX, TILE_PX);
  }
}

void PlayState::SpawnPlayer() {
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

bool PlayState::IsSolid(int col, int row) const {
  if (col < 0 || col >= LEVEL_COLS || row < 0 || row >= LEVEL_ROWS)
    return true;
  return solidGrid_[row][col];
}

// Reads every active finger (in logical-window coordinates) and merges the
// touch pads into the same flags the keyboard sets.
void PlayState::PollTouches() {
  TouchPoint points[10];
  int count = 0;

  int devices = SDL_GetNumTouchDevices();
  for (int d = 0; d < devices && count < 10; ++d) {
    SDL_TouchID id = SDL_GetTouchDevice(d);
    int fingers = SDL_GetNumTouchFingers(id);
    for (int f = 0; f < fingers && count < 10; ++f) {
      SDL_Finger *finger = SDL_GetTouchFinger(id, f);
      if (!finger) continue;
      // Normalized [0..1] → logical window pixels (logical size letterboxes,
      // so this matches the render coordinate space).
      points[count].x = finger->x * windowWidth_;
      points[count].y = finger->y * windowHeight_;
      ++count;
    }
  }

  TouchInput in = EvalTouches(zones_, points, count);
  moveLeft_  = moveLeft_  || in.left;
  moveRight_ = moveRight_ || in.right;
  if (in.jump && !prevTouchJump_) jumpPress_ = true; // edge, like a key press
  prevTouchJump_ = in.jump;
}

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
      case SDLK_AC_BACK: // Android back button
        isRunning_ = false;
        return;
      case SDLK_LEFT:  case SDLK_a: moveLeft_  = true; break;
      case SDLK_RIGHT: case SDLK_d: moveRight_ = true; break;
      case SDLK_SPACE: case SDLK_UP: case SDLK_w: jumpPress_ = true; break;
      default: break;
      }
      break;
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
      case SDLK_LEFT:  case SDLK_a: moveLeft_  = false; break;
      case SDLK_RIGHT: case SDLK_d: moveRight_ = false; break;
      default: break;
      }
      break;
    default:
      break;
    }
  }

  PollTouches();
}

void PlayState::ResolvePlayer(float dt) {
  Entity player = registry_.GetEntityByTag("player");
  auto &transform = registry_.GetComponent<TransformComponent>(player);
  auto &pc = registry_.GetComponent<PlayerComponent>(player);

  int pw = static_cast<int>(PLAYER_W * PLAYER_SCALE);
  int ph = static_cast<int>(PLAYER_H * PLAYER_SCALE);

  float vx = 0.0f;
  if (moveLeft_)  { vx = -pc.moveSpeed; pc.facingRight = false; }
  if (moveRight_) { vx =  pc.moveSpeed; pc.facingRight = true;  }

  if (jumpPress_ && pc.isOnGround) {
    pc.velocity.y = pc.jumpSpeed;
    pc.isOnGround = false;
  }
  jumpPress_ = false;

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

    if (vx < 0) {
      if (IsSolid(left, top) || IsSolid(left, bot))
        transform.position.x = (left + 1) * TILE_PX;
    } else if (vx > 0) {
      if (IsSolid(right, top) || IsSolid(right, bot))
        transform.position.x = right * TILE_PX - pw;
    }
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

  if (transform.position.y > LEVEL_ROWS * TILE_PX + 100) {
    transform.position = {2.0f * TILE_PX, 10.0f * TILE_PX};
    pc.velocity = {0.0f, 0.0f};
    pc.isOnGround = false;
  }
}

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

void PlayState::RenderTouchOverlay() {
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  auto drawZone = [&](const TouchZone &z, bool held) {
    SDL_Rect r = {static_cast<int>(z.x), static_cast<int>(z.y),
                  static_cast<int>(z.w), static_cast<int>(z.h)};
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, held ? 90 : 40);
    SDL_RenderFillRect(renderer_, &r);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 140);
    SDL_RenderDrawRect(renderer_, &r);
  };

  drawZone(zones_.left,  moveLeft_);
  drawZone(zones_.right, moveRight_);
  drawZone(zones_.jump,  prevTouchJump_);
}

void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 99, 155, 255, 255); // sky blue
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

    if (dstX + dstW < 0 || dstX > windowWidth_)  continue;
    if (dstY + dstH < 0 || dstY > windowHeight_) continue;

    SDL_Rect srcRect = sprite.srcRect;
    SDL_Rect dstRect = {dstX, dstY, dstW, dstH};
    SDL_RenderCopyEx(renderer_, assetStore_->GetTexture(sprite.assetId),
                     &srcRect, &dstRect, transform.rotation, NULL, sprite.flip);
  }

  RenderTouchOverlay();

  SDL_RenderPresent(renderer_);
}
