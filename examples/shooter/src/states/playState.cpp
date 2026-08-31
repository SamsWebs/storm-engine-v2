#include "playState.h"

#include "../ui.h"
#include "gameOverState.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr float PLAYER_SPEED = 180.0f;
constexpr float ENEMY_SPEED = 70.0f;
constexpr float BULLET_SPEED = 300.0f;
constexpr Uint32 FIRE_INTERVAL_MS = 167; // ~6 shots a second
constexpr Uint32 WAVE_INTERVAL_MS = 2000;
constexpr Uint32 GET_READY_MS = 1200;
constexpr int ROLL_FPS = 24; // 8 frames -> ~333 ms roll
constexpr int EXPLOSION_FPS = 20;
constexpr int SPRITE = 32;
constexpr int START_LIVES = 3;

// Marker components. Three custom types plus the five engine built-ins stay
// well under MAX_COMPONENTS, which is 64 per BINARY rather than per Registry.
struct PlayerComponent {};
struct EnemyComponent {};
struct BulletComponent {};

} // namespace

const std::string PlayState::s_playID = "PLAY_STATE";

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore *assetStore,
                     GameStateMachine *machine, Gamepad *gamepad,
                     bool &isRunning)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(assetStore), machine_(machine), gamepad_(gamepad),
      isRunning_(isRunning) {
  lives_ = START_LIVES;
  playerStart_ =
      glm::vec2((windowWidth_ - SPRITE) / 2.0f, windowHeight_ - 100.0f);
}

PlayState::~PlayState() {}

// Initialize in onEnter(), not the constructor. changeState() calls onEnter()
// after pushing the state; clean() calls onExit() before deleting it.
bool PlayState::onEnter() {
  // Register systems BEFORE creating entities. AddSystem<T>() never scans
  // existing entities, so a system added after the flush stays empty forever.
  registry_.AddSystem<MovementSystem>();
  registry_.AddSystem<RenderSystem>();
  registry_.AddSystem<AnimationSystem>();
  registry_.AddSystem<ContactSystem>();

  // Only bullet-vs-enemy and player-vs-enemy mean anything here. Rejecting
  // the rest in the filter rather than in CheckCollisions is what keeps a
  // dense volley cheap: without it every bullet pairs with every other
  // bullet, and all of those contacts are built and then thrown away.
  registry_.GetSystem<ContactSystem>().SetPairFilter(
      [](const Entity &a, const Entity &b) {
        const bool aEnemy = a.HasComponent<EnemyComponent>();
        const bool bEnemy = b.HasComponent<EnemyComponent>();
        if (aEnemy == bEnemy) {
          return false; // enemy-vs-enemy, or neither side is an enemy
        }
        const Entity &other = aEnemy ? b : a;
        return other.HasComponent<BulletComponent>() ||
               other.HasComponent<PlayerComponent>();
      });

  SpawnPlayer();
  SpawnWave();

  // Entity creation is deferred. changeState runs onEnter mid-update, so the
  // same frame's render() would otherwise draw an empty world for one frame.
  registry_.Update();

  lastShotMs_ = SDL_GetTicks();
  lastWaveMs_ = SDL_GetTicks();
  millisecondsPreviousFrame = SDL_GetTicks();
  return true;
}

// onExit() must be idempotent -- it can run twice (state machine call, then
// destructor). It does NOT clear the asset store: Game owns those textures and
// the menu and game-over states still need them.
bool PlayState::onExit() { return true; }

void PlayState::SpawnPlayer() {
  Entity player = registry_.CreateEntity();
  player.Tag("player");
  player.AddComponent<PlayerComponent>();
  player.AddComponent<TransformComponent>(playerStart_, glm::vec2(1, 1), 0.0);
  player.AddComponent<RigidBodyComponent>(glm::vec2(0, 0));
  // Sheet row 0 col 0 is the level flying pose. The sheet's aircraft are
  // drawn nose-DOWN, so the player -- which flies up the screen at the enemy
  // -- has to be flipped.
  player.AddComponent<SpriteComponent>("sheet", SPRITE, SPRITE, 1);
  player.GetComponent<SpriteComponent>().flip = SDL_FLIP_VERTICAL;
  // The 8 roll frames run ACROSS the sheet, so vertical = false. The engine
  // default is true, which would walk srcRect DOWN into a different aircraft
  // -- silently. Non-looped: the roll is started by resetting startTime and
  // ends on the last frame.
  player.AddComponent<AnimationComponent>(8, ROLL_FPS, false, false);
  // Inset from the 32px sprite: the aircraft does not fill its cell, and a
  // full-cell hitbox made near misses read as hits.
  player.AddComponent<BoxColliderComponent>(20, 20, glm::vec2(6, 6));

  player_ = player;
}

void PlayState::SpawnEnemy(const glm::vec2 &pos) {
  Entity enemy = registry_.CreateEntity();
  enemy.Group("enemies");
  enemy.AddComponent<EnemyComponent>();
  enemy.AddComponent<TransformComponent>(pos, glm::vec2(1, 1), 0.0);
  enemy.AddComponent<RigidBodyComponent>(glm::vec2(0, ENEMY_SPEED));
  // Green fighter, sheet row 2 col 0.
  enemy.AddComponent<SpriteComponent>("sheet", SPRITE, SPRITE, 1, false, 0,
                                      2 * SPRITE);
  enemy.AddComponent<BoxColliderComponent>(SPRITE, SPRITE);
  // No flip: the sheet's aircraft are drawn nose-down already, which is the
  // direction these fly.
}

void PlayState::SpawnBullet() {
  if (!player_) {
    return;
  }
  auto &tf = player_->GetComponent<TransformComponent>();
  Entity bullet = registry_.CreateEntity();
  bullet.Group("bullets");
  bullet.AddComponent<BulletComponent>();
  bullet.AddComponent<TransformComponent>(
      glm::vec2(tf.position.x, tf.position.y - 16.0f), glm::vec2(1, 1), 0.0);
  bullet.AddComponent<RigidBodyComponent>(glm::vec2(0, -BULLET_SPEED));
  // Orange bullet pair, sheet row 5 col 0.
  bullet.AddComponent<SpriteComponent>("sheet", SPRITE, SPRITE, 2, false, 0,
                                       5 * SPRITE);
  // The sprite cell is 32px but the drawn bullet pair is a narrow strip.
  bullet.AddComponent<BoxColliderComponent>(10, 16, glm::vec2(11, 8));
}

void PlayState::SpawnExplosion(glm::vec2 pos) {
  Entity explosion = registry_.CreateEntity();
  explosion.Group("explosions");
  explosion.AddComponent<TransformComponent>(pos, glm::vec2(1, 1), 0.0);
  // Small explosion runs sheet row 5, cols 2..7.
  explosion.AddComponent<SpriteComponent>("sheet", SPRITE, SPRITE, 2, false,
                                          2 * SPRITE, 5 * SPRITE);
  // 6 frames across, one shot, starting at col 2 (frameOffset). isLooped =
  // false stops on the last frame but does NOT remove the entity --
  // CullOffscreenEntities kills it once the run finishes.
  explosion.AddComponent<AnimationComponent>(6, EXPLOSION_FPS, false, false, 2);
}

// NOTE: pos is taken BY VALUE, not by const reference. Callers pass a
// reference into the TransformComponent pool (et->position, pt.position), and
// Registry::AddComponent resizes that same pool before constructing from the
// forwarded argument -- a std::vector resize invalidates every reference into
// it. Copying at the boundary makes the aliasing impossible.

// A wave is a formation that holds its shape as it descends. The previous
// version offset both x and y per enemy, which drew a diagonal staircase --
// not a formation at all.
void PlayState::SpawnWave() {
  const Formation shape =
      (waveCount_ % 2 == 0) ? Formation::Vee : Formation::LineAbreast;

  const int size = 5 + static_cast<int>(waveCount_ % 2); // 5 or 6
  const float spacing = 44.0f;

  // Deterministic, no rand(): the centre walks across the screen by wave
  // index, so every run spawns identically.
  const float span = static_cast<float>(windowWidth_) - 2.0f * spacing;
  const float centre = spacing + std::fmod(waveCount_ * 137.0f, span);
  const float mid = (size - 1) / 2.0f;

  // Shift the WHOLE formation to fit on screen rather than clamping each
  // aircraft. Per-aircraft clamping piles the outer wingmen onto the screen
  // edge and destroys the shape -- which is exactly what a formation is.
  const float halfSpan = mid * spacing;
  const float minX = 0.0f;
  const float maxX = static_cast<float>(windowWidth_ - SPRITE);
  float cx = centre;
  if (cx - halfSpan < minX) {
    cx = minX + halfSpan;
  }
  if (cx + halfSpan > maxX) {
    cx = maxX - halfSpan;
  }

  for (int i = 0; i < size; ++i) {
    const float offset = static_cast<float>(i) - mid;
    const float x = cx + offset * spacing;

    // Vee: the leader is lowest, wingmen step back symmetrically on both
    // sides. LineAbreast: one rank, all at the same height.
    float y = -static_cast<float>(SPRITE);
    if (shape == Formation::Vee) {
      y -= std::fabs(offset) * spacing;
    }

    SpawnEnemy(glm::vec2(x, y));
  }
  ++waveCount_;
}

bool PlayState::PlayerInvulnerable(Uint32 now) const {
  return rolling_ || now < getReadyUntil_;
}

void PlayState::LoseLife() {
  if (!player_ || leaving_) {
    return;
  }
  --lives_;
  logger_.Log("Hit! lives remaining: " + std::to_string(lives_));

  if (lives_ <= 0) {
    leaving_ = true;
    machine_->changeState(new GameOverState(
        renderer_, windowWidth_, windowHeight_, isDebugging_, assetStore_,
        machine_, gamepad_, isRunning_, score_, static_cast<int>(waveCount_)));
    return; // caller must not touch this state afterwards
  }

  auto &tf = player_->GetComponent<TransformComponent>();
  tf.position = playerStart_;
  auto &rb = player_->GetComponent<RigidBodyComponent>();
  rb.velocity = glm::vec2(0, 0);
  rolling_ = false;
  // Brief grace period so the player is not immediately killed again by the
  // same formation, and the GET READY! banner has time to be read.
  getReadyUntil_ = SDL_GetTicks() + GET_READY_MS;
}

// The active state owns ALL event polling. Never call SDL_PollEvent in both
// Game::ProcessInput and a state's processInput -- the queue is shared.
void PlayState::processInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    gamepad_->HandleEvent(event); // device add/remove only
    switch (event.type) {
    case SDL_QUIT:
      isRunning_ = false;
      return;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        isRunning_ = false;
        return;
      }
      if (event.key.keysym.sym == SDLK_LEFT)
        moveLeft_ = true;
      if (event.key.keysym.sym == SDLK_RIGHT)
        moveRight_ = true;
      if (event.key.keysym.sym == SDLK_UP)
        moveUp_ = true;
      if (event.key.keysym.sym == SDLK_DOWN)
        moveDown_ = true;
      if (event.key.keysym.sym == SDLK_SPACE)
        spaceHeld_ = true;
      if (event.key.keysym.sym == SDLK_z && !event.key.repeat) {
        rollPressed_ = true;
      }
      break;
    case SDL_KEYUP:
      if (event.key.keysym.sym == SDLK_LEFT)
        moveLeft_ = false;
      if (event.key.keysym.sym == SDLK_RIGHT)
        moveRight_ = false;
      if (event.key.keysym.sym == SDLK_UP)
        moveUp_ = false;
      if (event.key.keysym.sym == SDLK_DOWN)
        moveDown_ = false;
      if (event.key.keysym.sym == SDLK_SPACE)
        spaceHeld_ = false;
      break;
    default:
      break;
    }
  }
}

void PlayState::update() {
  // Variable dt with a 60 FPS cap. Nothing enforces a minimum frame rate, and
  // 0 keeps the delta unclamped, which is what this state has always done.
  const double deltaTime = CapFrameRate(0.0);
  const Uint32 now = SDL_GetTicks();

  // A changeState is queued; this object is off the stack and its deletion
  // is pending. Do no more work.
  if (leaving_) {
    return;
  }

  // Flush deferred entity adds/kills FIRST, before running any system.
  registry_.Update();

  // Sample the controller once per frame. Polled rather than accumulated
  // from events, so a pad unplugged mid-hold cannot latch a direction on.
  gamepad_->Update();
  if (gamepad_->Pressed(GamepadButton::Back)) {
    isRunning_ = false;
    return;
  }

  // Keyboard and controller are merged: either drives the game, and neither
  // disables the other.
  const bool left = moveLeft_ || gamepad_->Down(GamepadButton::Left);
  const bool right = moveRight_ || gamepad_->Down(GamepadButton::Right);
  const bool up = moveUp_ || gamepad_->Down(GamepadButton::Up);
  const bool down = moveDown_ || gamepad_->Down(GamepadButton::Down);
  const bool fire =
      spaceHeld_ || (gamepad_->Down(GamepadButton::A) ||
                     gamepad_->Current().triggerRight > 0.5f); // A / RT, held
  const bool roll = rollPressed_ || gamepad_->Pressed(GamepadButton::B) ||
                    gamepad_->Pressed(GamepadButton::X); // B / X, edge

  if (player_) {
    auto &rb = player_->GetComponent<RigidBodyComponent>();
    rb.velocity.x =
        (right ? PLAYER_SPEED : 0.0f) - (left ? PLAYER_SPEED : 0.0f);
    rb.velocity.y = (down ? PLAYER_SPEED : 0.0f) - (up ? PLAYER_SPEED : 0.0f);

    auto &anim = player_->GetComponent<AnimationComponent>();
    if (roll && !rolling_) {
      rolling_ = true;
      anim.startTime = static_cast<int>(now);
    }
    rollPressed_ = false;
    if (!rolling_) {
      // Pin to frame 0 (the level flying pose) while not rolling.
      anim.startTime = static_cast<int>(now);
    }

    if (fire && now - lastShotMs_ >= FIRE_INTERVAL_MS) {
      SpawnBullet();
      lastShotMs_ = now;
    }
  }

  registry_.GetSystem<MovementSystem>().Update(deltaTime);
  registry_.GetSystem<AnimationSystem>().Update();

  if (player_) {
    auto &tf = player_->GetComponent<TransformComponent>();
    tf.position.x =
        std::max(0.0f, std::min(static_cast<float>(windowWidth_ - SPRITE),
                                tf.position.x));
    tf.position.y =
        std::max(0.0f, std::min(static_cast<float>(windowHeight_ - SPRITE),
                                tf.position.y));

    auto &anim = player_->GetComponent<AnimationComponent>();
    if (rolling_ && anim.currentFrame >= anim.numFrames - 1) {
      rolling_ = false;
    }
  }

  if (now - lastWaveMs_ >= WAVE_INTERVAL_MS) {
    SpawnWave();
    lastWaveMs_ = now;
  }

  CullOffscreenEntities();
  CheckCollisions();
}

void PlayState::CullOffscreenEntities() {
  // Copy-then-iterate wherever the body might Kill(): a kill is deferred, so
  // a killed entity stays in its group until the next registry_.Update().
  if (registry_.DoesGroupExist("bullets")) {
    for (auto &b : registry_.GetEntitiesByGroup("bullets")) {
      auto *tf = registry_.TryGetComponent<TransformComponent>(b);
      if (tf && tf->position.y < -static_cast<float>(SPRITE)) {
        b.Kill();
      }
    }
  }

  if (registry_.DoesGroupExist("enemies")) {
    for (auto &e : registry_.GetEntitiesByGroup("enemies")) {
      auto *tf = registry_.TryGetComponent<TransformComponent>(e);
      if (tf && tf->position.y > windowHeight_ + SPRITE) {
        e.Kill();
      }
    }
  }

  // isLooped = false stops the animation on its last frame; it does not
  // remove the entity. Without this the corpses accumulate invisibly.
  if (registry_.DoesGroupExist("explosions")) {
    for (auto &x : registry_.GetEntitiesByGroup("explosions")) {
      auto *anim = registry_.TryGetComponent<AnimationComponent>(x);
      if (anim && anim->currentFrame >= anim->numFrames - 1) {
        x.Kill();
      }
    }
  }
}

// Overlap detection comes from the engine's ContactSystem, which reports
// contacts instead of acting on them. CollisionSystem, the older one, is wrong
// here twice over: it kills BOTH entities on contact -- deleting the player on
// any enemy touch -- and it offers no hook for scoring or spawning an
// explosion.
void PlayState::CheckCollisions() {
  if (!player_ || leaving_) {
    return;
  }
  const Uint32 now = SDL_GetTicks();

  auto &contacts = registry_.GetSystem<ContactSystem>();
  contacts.Update();

  // A killed entity stays in the system's entity vector until the next
  // registry_.Update(), so it can still turn up in a contact this frame.
  // Track what has died by id, so a second bullet cannot hit the same enemy
  // and score it twice.
  //
  // An earlier version guarded that by breaking out of the whole bullet loop
  // after the first hit, which silently capped the game at one kill per
  // frame: fire fast enough into a dense formation and bullets passed
  // straight through.
  std::set<std::size_t> deadEnemies;
  std::set<std::size_t> spentBullets;

  // Bullets get their own pass, ahead of the player's. The hand-rolled
  // version resolved every bullet before it looked at the player, so an
  // enemy shot down this frame could never also take a life. Contacts arrive
  // sorted by entity id, which would interleave the two.
  for (const auto &c : contacts.GetContacts()) {
    const bool aIsEnemy = c.a.HasComponent<EnemyComponent>();
    Entity enemy = aIsEnemy ? c.a : c.b;
    Entity other = aIsEnemy ? c.b : c.a;
    if (!other.HasComponent<BulletComponent>()) {
      continue;
    }
    if (deadEnemies.count(enemy.GetId()) || spentBullets.count(other.GetId())) {
      continue;
    }

    SpawnExplosion(enemy.GetComponent<TransformComponent>().position);
    enemy.Kill();
    other.Kill();
    deadEnemies.insert(enemy.GetId());
    spentBullets.insert(other.GetId());
    score_ += 100;
    logger_.Log("Score: " + std::to_string(score_));
  }

  if (PlayerInvulnerable(now)) {
    return;
  }

  for (const auto &c : contacts.GetContacts()) {
    const bool aIsEnemy = c.a.HasComponent<EnemyComponent>();
    const Entity &enemy = aIsEnemy ? c.a : c.b;
    const Entity &other = aIsEnemy ? c.b : c.a;
    if (!other.HasComponent<PlayerComponent>()) {
      continue;
    }
    if (deadEnemies.count(enemy.GetId())) {
      continue;
    }
    SpawnExplosion(player_->GetComponent<TransformComponent>().position);
    LoseLife(); // may have queued a changeState
    return;
  }
}

void PlayState::RenderHud() {
  SDL_Texture *digits = assetStore_->GetTexture("digits");
  const float s = 1.5f;

  // SCORE: <number>
  SDL_Texture *scoreLabel = assetStore_->GetTexture("scoreLabel");
  int lw = 0, lh = 0;
  if (scoreLabel) {
    SDL_QueryTexture(scoreLabel, nullptr, nullptr, &lw, &lh);
  }
  ui::DrawTexture(renderer_, scoreLabel, 12, 10, s);
  ui::DrawNumber(renderer_, digits, score_, 12 + static_cast<int>(lw * s) + 8,
                 12, s);

  // WAVE: <number>
  SDL_Texture *waveLabel = assetStore_->GetTexture("waveLabel");
  int ww = 0, wh = 0;
  if (waveLabel) {
    SDL_QueryTexture(waveLabel, nullptr, nullptr, &ww, &wh);
  }
  ui::DrawTexture(renderer_, waveLabel, 12, 40, s);
  ui::DrawNumber(renderer_, digits, static_cast<int>(waveCount_),
                 12 + static_cast<int>(ww * s) + 8, 40, s);

  // Lives, top-right, as small player fighters. There is no separate life
  // icon on the sheet -- the small planes in that row touch each other and
  // cannot be cleanly separated -- so this is sheet cell (0,0) at half size.
  SDL_Texture *sheet = assetStore_->GetTexture("sheet");
  const int icon = 20;
  for (int i = 0; i < lives_; ++i) {
    ui::DrawSheetCell(renderer_, sheet, 0, 0,
                      windowWidth_ - 12 - (i + 1) * (icon + 4), 12, icon);
  }

  // GET READY! while the post-death grace period runs.
  if (SDL_GetTicks() < getReadyUntil_) {
    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("getReady"),
                           windowWidth_ / 2, windowHeight_ / 2 - 40, 3.0f);
  }
}

void PlayState::render() {
  // Same sky blue as the menu and the 1945 logo.
  SDL_SetRenderDrawColor(renderer_, 0, 99, 191, 255);
  SDL_RenderClear(renderer_);

  registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

  // No debug collider overlay: RenderColliderSystem is not registered, and
  // GetSystem on an unregistered system goes through std::map::at -- it
  // would throw here and abort under the Switch build's -fno-exceptions.
  // The entities do carry BoxColliderComponent now, so registering it in
  // onEnter() is all an overlay would take.

  RenderHud();

  SDL_RenderPresent(renderer_);
}
