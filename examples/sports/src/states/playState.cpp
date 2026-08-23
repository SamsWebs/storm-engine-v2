#include "playState.h"
#include <algorithm>
#include <cstring>

const std::string PlayState::s_playID = "PLAY";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore,
                     bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{std::move(assetStore)}, isRunning_{isRunning} {}

PlayState::~PlayState() { onExit(); }

// ─────────────────────────────────────────────────────────────────────────────
// onEnter — load assets, register systems, spawn entities
// ─────────────────────────────────────────────────────────────────────────────

bool PlayState::onEnter() {
  if (TTF_Init() != 0) {
    logger_.Err("TTF_Init failed: " + std::string(TTF_GetError()));
  }

  font_ = TTF_OpenFont("assets/fonts/font.ttf", 22);
  if (!font_)
    logger_.Err("Failed to open font: " + std::string(TTF_GetError()));

  // Load textures
  assetStore_->AddTexture(renderer_, "rink", "assets/gfx/rink.png");
  assetStore_->AddTexture(renderer_, "player", "assets/gfx/player.png");
  assetStore_->AddTexture(renderer_, "ai_skater", "assets/gfx/ai_skater.png");
  assetStore_->AddTexture(renderer_, "goalie", "assets/gfx/goalie.png");
  assetStore_->AddTexture(renderer_, "puck", "assets/gfx/puck.png");

  // Register systems
  registry_.AddSystem<RenderSystem>();
  registry_.AddSystem<HockeyPhysicsSystem>();
  // Registered before SpawnEntities: Registry::Update() computes system
  // membership once, when an entity is admitted, so a system added after the
  // fact would start empty and stay empty.
  registry_.AddSystem<ContactSystem>();

  OpenController();

  SpawnEntities();
  lastTick_ = SDL_GetTicks();
  return true;
}

bool PlayState::onExit() {
  CloseController();

  if (font_) {
    TTF_CloseFont(font_);
    font_ = nullptr;
  }
  TTF_Quit();

  delete playerEnt_;
  delete aiSkaEnt_;
  delete aiGoalEnt_;
  delete puckEnt_;
  playerEnt_ = aiSkaEnt_ = aiGoalEnt_ = puckEnt_ = nullptr;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnEntities
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Gamepad
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::OpenController() {
  if (pad_)
    return;
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (!SDL_IsGameController(i))
      continue;
    pad_ = SDL_GameControllerOpen(i);
    if (pad_) {
      logger_.Log("Gamepad connected: " +
                  std::string(SDL_GameControllerName(pad_)));
      return;
    }
  }
}

void PlayState::CloseController() {
  if (!pad_)
    return;
  SDL_GameControllerClose(pad_);
  pad_ = nullptr;
  padAxis_ = {0.f, 0.f};
}

// Sticks and the d-pad are held state, not events, so they are sampled once a
// frame. The shoot button is NOT read here - it has to be edge triggered, or
// holding it would fire a shot every frame the puck is carried.
void PlayState::PollController() {
  padAxis_ = {0.f, 0.f};
  if (!pad_)
    return;

  float ax = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
  float ay = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
  if (std::abs(ax) < PAD_DEADZONE)
    ax = 0.f;
  if (std::abs(ay) < PAD_DEADZONE)
    ay = 0.f;
  padAxis_ = {ax / 32767.f, ay / 32767.f};

  // The d-pad is digital, so it deflects fully and overrides a resting stick.
  if (SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_DPAD_UP))
    padAxis_.y = -1.f;
  if (SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
    padAxis_.y = 1.f;
  if (SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
    padAxis_.x = -1.f;
  if (SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
    padAxis_.x = 1.f;
}

void PlayState::SpawnEntities() {
  // Rink background
  Entity rink = registry_.CreateEntity();
  rink.AddComponent<TransformComponent>(glm::vec2{0.f, 0.f},
                                        glm::vec2{1.f, 1.f}, 0.0);
  rink.AddComponent<SpriteComponent>("rink", 800, 600, 0);

  // Player (left side, center)
  Entity player = registry_.CreateEntity();
  player.Tag("player");
  player.AddComponent<TransformComponent>(
      glm::vec2{RINK_X + 120.f, RINK_Y + RINK_H / 2.f - PLAYER_SIZE / 2.f},
      glm::vec2{1.f, 1.f}, 0.0);
  player.AddComponent<SpriteComponent>("player", PLAYER_SIZE, PLAYER_SIZE, 2);
  player.AddComponent<SkaterComponent>(SkaterTeam::Player, PLAYER_SPEED);
  playerEnt_ = new Entity(player);

  // AI Skater (right side, near center)
  Entity aiSka = registry_.CreateEntity();
  aiSka.Tag("ai_skater");
  aiSka.AddComponent<TransformComponent>(
      glm::vec2{RINK_R - 160.f, RINK_Y + RINK_H / 2.f - AI_SIZE / 2.f},
      glm::vec2{1.f, 1.f}, 0.0);
  aiSka.AddComponent<SpriteComponent>("ai_skater", AI_SIZE, AI_SIZE, 2);
  aiSka.AddComponent<SkaterComponent>(SkaterTeam::AISkater, AI_SPEED);
  aiSkaEnt_ = new Entity(aiSka);

  // AI Goalie (right side, in front of goal)
  Entity aiGoal = registry_.CreateEntity();
  aiGoal.Tag("ai_goalie");
  aiGoal.AddComponent<TransformComponent>(
      glm::vec2{RINK_R - GOAL_DEPTH - GOALIE_SIZE - 4.f,
                RINK_Y + RINK_H / 2.f - GOALIE_SIZE / 2.f},
      glm::vec2{1.f, 1.f}, 0.0);
  aiGoal.AddComponent<SpriteComponent>("goalie", GOALIE_SIZE, GOALIE_SIZE, 2);
  aiGoal.AddComponent<SkaterComponent>(SkaterTeam::AIGoalie, GOALIE_SPEED);
  aiGoalEnt_ = new Entity(aiGoal);

  // Puck (center ice)
  Entity puck = registry_.CreateEntity();
  puck.Tag("puck");
  puck.AddComponent<TransformComponent>(
      glm::vec2{windowWidth_ / 2.f - PUCK_SIZE / 2.f,
                windowHeight_ / 2.f - PUCK_SIZE / 2.f},
      glm::vec2{1.f, 1.f}, 0.0);
  puck.AddComponent<SpriteComponent>("puck", PUCK_SIZE, PUCK_SIZE, 3);
  puck.AddComponent<PuckComponent>();
  puck.AddComponent<BoxColliderComponent>(PUCK_SIZE, PUCK_SIZE);
  puckEnt_ = new Entity(puck);

  SpawnWalls();

  registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnWalls - the boards, as collider entities
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::SpawnWalls() {
  // The left and right runs are split around the goal mouth so a shot on
  // target passes through instead of bouncing. With a solid wall there,
  // CheckGoal's `px < RINK_X` test was unreachable - the free puck got
  // clamped to RINK_X + 10 before it could ever cross the line, so the AI
  // could not score at all.
  const int mouthTop = GOAL_MOUTH_Y;
  const int mouthBottom = GOAL_MOUTH_Y + GOAL_MOUTH_H;

  const struct {
    int x, y, w, h;
  } boards[] = {
      {RINK_X, RINK_Y - WALL_T, RINK_W, WALL_T}, // top
      {RINK_X, RINK_B, RINK_W, WALL_T},          // bottom
      {RINK_X - WALL_T, RINK_Y, WALL_T, mouthTop - RINK_Y},
      {RINK_X - WALL_T, mouthBottom, WALL_T, RINK_B - mouthBottom},
      {RINK_R, RINK_Y, WALL_T, mouthTop - RINK_Y},
      {RINK_R, mouthBottom, WALL_T, RINK_B - mouthBottom},
  };

  for (const auto &board : boards) {
    Entity wall = registry_.CreateEntity();
    wall.Group("boards");
    wall.AddComponent<TransformComponent>(
        glm::vec2{static_cast<float>(board.x), static_cast<float>(board.y)},
        glm::vec2{1.f, 1.f}, 0.0);
    // No sprite - rink.png already draws the boards.
    wall.AddComponent<BoxColliderComponent>(board.w, board.h);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// ResolvePuckContacts - bounce the puck off whatever ContactSystem reported
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::ResolvePuckContacts() {
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  // A carried puck is placed by its carrier every frame, so bouncing it here
  // would only fight UpdatePuckCarry.
  if (puck.ownerTag != -1)
    return;

  auto &puckT = puckEnt_->GetComponent<TransformComponent>();
  const std::size_t puckId = puckEnt_->GetId();

  for (const auto &contact :
       registry_.GetSystem<ContactSystem>().GetContacts()) {
    const bool puckIsA = contact.a.GetId() == puckId;
    if (!puckIsA && contact.b.GetId() != puckId)
      continue;

    // Contact::normal points a -> b and `a` always holds the lower entity id,
    // so flip it when the puck happens to be b. After that it always points
    // from the puck into the board it hit.
    const glm::vec2 normal = puckIsA ? contact.normal : -contact.normal;

    // Push the puck back out, then mirror its velocity about that normal.
    // Which board it was stopped mattering - this replaced four hardcoded
    // axis-aligned bounce cases and the RL/RT/RR/RB constants behind them.
    puckT.position -= normal * contact.depth;
    puck.velocity = glm::reflect(puck.velocity, normal);
  }
}

void PlayState::ResetPositions() {
  auto &pt = playerEnt_->GetComponent<TransformComponent>();
  pt.position = {RINK_X + 120.f, RINK_Y + RINK_H / 2.f - PLAYER_SIZE / 2.f};

  auto &at = aiSkaEnt_->GetComponent<TransformComponent>();
  at.position = {RINK_R - 160.f, RINK_Y + RINK_H / 2.f - AI_SIZE / 2.f};

  auto &gt = aiGoalEnt_->GetComponent<TransformComponent>();
  gt.position = {RINK_R - GOAL_DEPTH - GOALIE_SIZE - 4.f,
                 RINK_Y + RINK_H / 2.f - GOALIE_SIZE / 2.f};

  auto &puckT = puckEnt_->GetComponent<TransformComponent>();
  puckT.position = {windowWidth_ / 2.f - PUCK_SIZE / 2.f,
                    windowHeight_ / 2.f - PUCK_SIZE / 2.f};

  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  puck.velocity = {0.f, 0.f};
  puck.ownerTag = -1;
  puck.pickupLock = 0.f;
}

// ─────────────────────────────────────────────────────────────────────────────
// processInput
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::processInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      isRunning_ = false;
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
        isRunning_ = false;
        break;
      case SDLK_w:
      case SDLK_UP:
        keyUp_ = true;
        break;
      case SDLK_s:
      case SDLK_DOWN:
        keyDown_ = true;
        break;
      case SDLK_a:
      case SDLK_LEFT:
        keyLeft_ = true;
        break;
      case SDLK_d:
      case SDLK_RIGHT:
        keyRight_ = true;
        break;
      case SDLK_SPACE:
        keyShoot_ = true;
        break;
      }
      break;
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
      case SDLK_w:
      case SDLK_UP:
        keyUp_ = false;
        break;
      case SDLK_s:
      case SDLK_DOWN:
        keyDown_ = false;
        break;
      case SDLK_a:
      case SDLK_LEFT:
        keyLeft_ = false;
        break;
      case SDLK_d:
      case SDLK_RIGHT:
        keyRight_ = false;
        break;
      case SDLK_SPACE:
        keyShoot_ = false;
        break;
      }
      break;

    // ── Gamepad ──────────────────────────────────────────────────────────
    case SDL_CONTROLLERDEVICEADDED:
      OpenController();
      break;
    case SDL_CONTROLLERDEVICEREMOVED:
      if (pad_ &&
          event.cdevice.which ==
              SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad_))) {
        CloseController();
        OpenController(); // fall back to another pad if one is still attached
      }
      break;
    case SDL_CONTROLLERBUTTONDOWN:
      if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
        keyShoot_ = true;
      if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START ||
          event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
        isRunning_ = false;
      break;
    case SDL_CONTROLLERBUTTONUP:
      if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
        keyShoot_ = false;
      break;
    }
  }

  PollController();
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────

glm::vec2 PlayState::Center(const Entity &e, int size) const {
  const auto &t = e.GetComponent<TransformComponent>();
  return t.position + glm::vec2{size / 2.f, size / 2.f};
}

void PlayState::update() {
  Uint32 now = SDL_GetTicks();
  double dt = (now - lastTick_) / 1000.0;
  lastTick_ = now;
  if (dt > 0.05)
    dt = 0.05; // cap at 50ms

  if (gameOver_)
    return;

  // ── Post-goal reset timer ─────────────────────────────────────────────
  if (goalScored_ || saveMade_) {
    resetTimer_ -= static_cast<float>(dt);
    if (resetTimer_ <= 0.f) {
      goalScored_ = false;
      saveMade_ = false;
      ResetPositions();
    }
    registry_.Update();
    return;
  }

  // ── Player movement ───────────────────────────────────────────────────
  UpdatePlayerMovement(dt);

  // ── AI logic ─────────────────────────────────────────────────────────
  UpdateAI(dt);

  // ── Puck carry (move puck with its owner) ─────────────────────────────
  UpdatePuckCarry();

  // ── Puck physics (free puck) ──────────────────────────────────────────
  registry_.GetSystem<HockeyPhysicsSystem>().Update(dt);

  // ── Boards ────────────────────────────────────────────────────────────
  registry_.GetSystem<ContactSystem>().Update();
  ResolvePuckContacts();

  // ── Pickup attempts ───────────────────────────────────────────────────
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  if (puck.pickupLock > 0.f)
    puck.pickupLock -= static_cast<float>(dt);
  if (puck.ownerTag == -1) {
    TryPickup(*playerEnt_, 0);
    TryPickup(*aiSkaEnt_, 1);
    TryPickup(*aiGoalEnt_, 2);
  }

  // ── Goalie save ───────────────────────────────────────────────────────
  // UpdateAI gives the goalie no clearing logic, so once it gains possession
  // nothing would ever release the puck and play would deadlock. Whistle it
  // dead instead and drop a fresh faceoff after SAVE_DELAY.
  if (puck.ownerTag == 2 && !saveMade_ && !goalScored_) {
    saveMade_ = true;
    resetTimer_ = SAVE_DELAY;
  }

  // ── Goal detection ────────────────────────────────────────────────────
  CheckGoal();

  registry_.Update();
}

void PlayState::UpdatePlayerMovement(double dt) {
  auto &t = playerEnt_->GetComponent<TransformComponent>();

  // Keyboard gives a unit push per axis; the stick gives its own magnitude,
  // so a light lean skates slowly. Both are summed, then clamped to length 1
  // -- which also stops a keyboard diagonal being sqrt(2) faster than a
  // straight line, as it used to be when each axis got the full step.
  glm::vec2 dir = {0.f, 0.f};
  if (keyUp_)
    dir.y -= 1.f;
  if (keyDown_)
    dir.y += 1.f;
  if (keyLeft_)
    dir.x -= 1.f;
  if (keyRight_)
    dir.x += 1.f;
  dir += padAxis_;

  const float len = glm::length(dir);
  if (len > 1.f)
    dir /= len;

  t.position += dir * (PLAYER_SPEED * static_cast<float>(dt));

  // Clamp to left half of rink (player stays on own side when not attacking)
  t.position.x =
      std::clamp(t.position.x, (float)RINK_X, (float)(RINK_R - PLAYER_SIZE));
  t.position.y =
      std::clamp(t.position.y, (float)RINK_Y, (float)(RINK_B - PLAYER_SIZE));

  // ── Shoot ─────────────────────────────────────────────────────────────
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  if (keyShoot_ && puck.ownerTag == 0) {
    // Shoot toward right goal
    puck.ownerTag = -1;
    puck.velocity = {SHOOT_SPEED, 0.f};
    puck.pickupLock = PICKUP_LOCKOUT;
    keyShoot_ = false;
  }
}

void PlayState::UpdateAI(double dt) {
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  auto puckC = Center(*puckEnt_, PUCK_SIZE);

  // ── AI Skater ─────────────────────────────────────────────────────────
  {
    auto &t = aiSkaEnt_->GetComponent<TransformComponent>();
    auto aPos = Center(*aiSkaEnt_, AI_SIZE);
    float spd = AI_SPEED * static_cast<float>(dt);

    if (puck.ownerTag == 1) {
      // Has puck — skate toward player goal (left net)
      glm::vec2 target = {(float)(RINK_X + GOAL_DEPTH + AI_SIZE),
                          (float)(GOAL_MOUTH_Y + GOAL_MOUTH_H / 2.f)};
      glm::vec2 dir = target - aPos;
      float len = glm::length(dir);
      if (len > 1.f) {
        dir /= len;
        t.position += dir * spd;
      }
      // Shoot when close enough to player goal
      if (aPos.x < RINK_X + AI_SHOOT_DIST) {
        puck.ownerTag = -1;
        puck.velocity = {-SHOOT_SPEED * 0.85f, 0.f};
        puck.pickupLock = PICKUP_LOCKOUT;
      }
    } else {
      // Chase the puck
      glm::vec2 dir = puckC - aPos;
      float len = glm::length(dir);
      if (len > 1.f) {
        dir /= len;
        t.position += dir * spd;
      }
    }

    t.position.x =
        std::clamp(t.position.x, (float)RINK_X, (float)(RINK_R - AI_SIZE));
    t.position.y =
        std::clamp(t.position.y, (float)RINK_Y, (float)(RINK_B - AI_SIZE));
  }

  // ── AI Goalie — tracks puck Y, stays on goal line ─────────────────────
  {
    auto &t = aiGoalEnt_->GetComponent<TransformComponent>();
    float spd = GOALIE_SPEED * static_cast<float>(dt);
    float goalX = (float)(RINK_R - GOAL_DEPTH - GOALIE_SIZE - 4);
    float targetY = puckC.y - GOALIE_SIZE / 2.f;

    targetY = std::clamp(targetY, (float)(GOAL_MOUTH_Y),
                         (float)(GOAL_MOUTH_Y + GOAL_MOUTH_H - GOALIE_SIZE));

    float diff = targetY - t.position.y;
    if (std::abs(diff) > spd)
      t.position.y += (diff > 0.f ? spd : -spd);
    else
      t.position.y = targetY;

    t.position.x = goalX; // always on goal line
  }
}

void PlayState::UpdatePuckCarry() {
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  if (puck.ownerTag == -1)
    return;

  // Move puck just in front of the carrier
  glm::vec2 carrierPos;
  int size = 0;
  if (puck.ownerTag == 0) {
    carrierPos = playerEnt_->GetComponent<TransformComponent>().position;
    size = PLAYER_SIZE;
  }
  if (puck.ownerTag == 1) {
    carrierPos = aiSkaEnt_->GetComponent<TransformComponent>().position;
    size = AI_SIZE;
  }
  if (puck.ownerTag == 2) {
    carrierPos = aiGoalEnt_->GetComponent<TransformComponent>().position;
    size = GOALIE_SIZE;
  }

  // Center puck on carrier
  auto &puckT = puckEnt_->GetComponent<TransformComponent>();
  puckT.position = carrierPos + glm::vec2{size / 2.f - PUCK_SIZE / 2.f,
                                          size / 2.f - PUCK_SIZE / 2.f};
  puck.velocity = {0.f, 0.f};
}

void PlayState::TryPickup(Entity &skater, int ownerTag) {
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  if (puck.ownerTag != -1)
    return;
  if (puck.pickupLock > 0.f)
    return;

  glm::vec2 pc = Center(*puckEnt_, PUCK_SIZE);
  int sz =
      (ownerTag == 2) ? GOALIE_SIZE : (ownerTag == 0 ? PLAYER_SIZE : AI_SIZE);
  glm::vec2 sc = Center(skater, sz);

  if (glm::length(pc - sc) < PICKUP_RADIUS) {
    puck.ownerTag = ownerTag;
  }
}

void PlayState::CheckGoal() {
  if (goalScored_)
    return;
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  auto &puckT = puckEnt_->GetComponent<TransformComponent>();

  float px = puckT.position.x;
  float py = puckT.position.y + PUCK_SIZE / 2.f;

  bool inGoalMouth = py > GOAL_MOUTH_Y && py < GOAL_MOUTH_Y + GOAL_MOUTH_H;

  // Puck crossed left goal line — AI scores
  if (px < RINK_X && inGoalMouth) {
    aiScore_++;
    goalScored_ = true;
    resetTimer_ = RESET_DELAY;
    puck.ownerTag = -1;
    puck.velocity = {0.f, 0.f};
    if (aiScore_ >= GOALS_TO_WIN)
      gameOver_ = true;
    return;
  }

  // Puck crossed right goal line — player scores
  if (px + PUCK_SIZE > RINK_R && inGoalMouth) {
    playerScore_++;
    goalScored_ = true;
    resetTimer_ = RESET_DELAY;
    puck.ownerTag = -1;
    puck.velocity = {0.f, 0.f};
    if (playerScore_ >= GOALS_TO_WIN)
      gameOver_ = true;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 10, 10, 30, 255);
  SDL_RenderClear(renderer_);

  // Draw entities (rink background + skaters + puck)
  registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

  // Draw rink markings on top of the background sprite
  DrawRink();

  // HUD
  DrawHUD();

  SDL_RenderPresent(renderer_);
}

void PlayState::DrawRink() {
  // Rink border
  SDL_SetRenderDrawColor(renderer_, 100, 170, 210, 255);
  SDL_Rect border = {RINK_X, RINK_Y, RINK_W, RINK_H};
  SDL_RenderDrawRect(renderer_, &border);

  // Center line
  SDL_SetRenderDrawColor(renderer_, 180, 60, 60, 255);
  SDL_RenderDrawLine(renderer_, RINK_X + RINK_W / 2, RINK_Y,
                     RINK_X + RINK_W / 2, RINK_B);

  // Left goal (player defends)
  SDL_SetRenderDrawColor(renderer_, 40, 40, 200, 180);
  SDL_Rect leftGoal = {RINK_X - GOAL_DEPTH, GOAL_MOUTH_Y, GOAL_DEPTH,
                       GOAL_MOUTH_H};
  SDL_RenderDrawRect(renderer_, &leftGoal);

  // Right goal (AI defends)
  SDL_SetRenderDrawColor(renderer_, 200, 40, 40, 180);
  SDL_Rect rightGoal = {RINK_R, GOAL_MOUTH_Y, GOAL_DEPTH, GOAL_MOUTH_H};
  SDL_RenderDrawRect(renderer_, &rightGoal);
}

void PlayState::DrawText(const std::string &text, int x, int y, SDL_Color color,
                         int ptSize) {
  if (!font_)
    return;
  SDL_Surface *surf = TTF_RenderText_Blended(font_, text.c_str(), color);
  if (!surf)
    return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer_, surf);
  SDL_FreeSurface(surf);
  if (!tex)
    return;

  int w, h;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  SDL_Rect dst = {x, y, w, h};
  SDL_RenderCopy(renderer_, tex, nullptr, &dst);
  SDL_DestroyTexture(tex);
}

void PlayState::DrawHUD() {
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gold = {255, 210, 50, 255};
  SDL_Color red = {255, 80, 80, 255};

  // Score
  std::string score =
      std::to_string(playerScore_) + "  —  " + std::to_string(aiScore_);
  DrawText(score, windowWidth_ / 2 - 40, 12, white);

  // Team labels
  DrawText("YOU", RINK_X + 10, 12, {100, 150, 255, 255});
  DrawText("CPU", RINK_R - 60, 12, {255, 100, 100, 255});

  // Controls hint
  DrawText(pad_ ? "STICK/DPAD: move   A: shoot   START: quit"
                : "WASD: move   SPACE: shoot   ESC: quit",
           RINK_X, RINK_B + 10, {160, 160, 160, 255});

  // Goal message
  if (goalScored_ && !gameOver_) {
    DrawText("GOAL!", windowWidth_ / 2 - 30, windowHeight_ / 2 - 20, gold, 28);
  }

  // Goalie save - play is dead until the faceoff
  if (saveMade_ && !gameOver_) {
    DrawText("SAVE!", windowWidth_ / 2 - 32, windowHeight_ / 2 - 20,
             {120, 220, 255, 255}, 28);
    DrawText("faceoff in " + std::to_string((int)resetTimer_ + 1),
             windowWidth_ / 2 - 52, windowHeight_ / 2 + 20,
             {200, 200, 200, 255}, 18);
  }

  // Game over
  if (gameOver_) {
    bool playerWon = playerScore_ >= GOALS_TO_WIN;
    std::string msg = playerWon ? "YOU WIN!" : "CPU WINS!";
    SDL_Color col = playerWon ? gold : red;
    DrawText(msg, windowWidth_ / 2 - 50, windowHeight_ / 2 - 20, col, 32);
    DrawText("Press ESC to exit", windowWidth_ / 2 - 70, windowHeight_ / 2 + 20,
             white, 20);
  }

  // Puck owner indicator
  auto &puck = puckEnt_->GetComponent<PuckComponent>();
  if (puck.ownerTag == 0)
    DrawText("* you have the puck *", RINK_X + 10, RINK_B + 28,
             {100, 200, 255, 255}, 18);
}
