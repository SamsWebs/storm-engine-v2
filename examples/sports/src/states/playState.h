#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <string>

#include <glm/glm.hpp>
#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/contact.h>
#include <stormengine2/systems/render.h>

#include "../components/puckComponent.h"
#include "../components/skaterComponent.h"
#include "../systems/hockeyPhysicsSystem.h"

// ─── Rink Layout (pixels)
// ─────────────────────────────────────────────────────
constexpr int RINK_X = 60;              // left  edge of ice surface
constexpr int RINK_Y = 60;              // top   edge
constexpr int RINK_W = 680;             // width
constexpr int RINK_H = 480;             // height
constexpr int RINK_R = RINK_X + RINK_W; // right edge
constexpr int RINK_B = RINK_Y + RINK_H; // bottom edge

// ─── Goal Mouths (centered vertically on rink)
// ────────────────────────────────
constexpr int GOAL_MOUTH_H = 120;
constexpr int GOAL_MOUTH_Y = RINK_Y + (RINK_H - GOAL_MOUTH_H) / 2;
constexpr int GOAL_DEPTH = 28;

// ─── Boards
// ─────────────────────────────────────────────────────────────────── The walls
// are collider entities sitting just outside the ice, so the puck bounces off
// whatever it hits without the physics system knowing the rink layout. The left
// and right boards are split either side of the goal mouth, which is what
// leaves the mouth open for a shot to go in.
constexpr int WALL_T = 24;

// ─── Sprite sizes ────────────────────────────────────────────────────────────
constexpr int PLAYER_SIZE = 32;
constexpr int AI_SIZE = 32;
constexpr int GOALIE_SIZE = 40;
constexpr int PUCK_SIZE = 16;

// ─── Gameplay
// ─────────────────────────────────────────────────────────────────
constexpr float PLAYER_SPEED = 220.f;
constexpr float AI_SPEED = 155.f;
constexpr float GOALIE_SPEED = 170.f;
constexpr float SHOOT_SPEED = 520.f;
constexpr float PICKUP_RADIUS = 22.f;   // how close to grab puck
constexpr float PICKUP_LOCKOUT = 0.35f; // no pickups for this long after a shot
constexpr int GOALS_TO_WIN = 3;
constexpr float RESET_DELAY = 2.0f;    // seconds to pause after a goal
constexpr float SAVE_DELAY = 3.0f;     // seconds to pause after a goalie save
constexpr float AI_SHOOT_DIST = 180.f; // AI shoots when this close to goal

// ─── Gamepad
// ────────────────────────────────────────────────────────────────── Analog
// sticks rest slightly off centre, so anything under this counts as centred.
// ~24% of the Sint16 range, the usual starting point.
constexpr int PAD_DEADZONE = 8000;

class PlayState : public GameState {
public:
  PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning);
  ~PlayState();

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;
  std::string getStateID() const override { return s_playID; }

private:
  // ── Helpers ──────────────────────────────────────────────────────────────
  void OpenController();
  void CloseController();
  void PollController();

  void SpawnEntities();
  void SpawnWalls();
  void ResolvePuckContacts();
  void ResetPositions();
  void CheckGoal();
  void UpdatePlayerMovement(double dt);
  void UpdateAI(double dt);
  void UpdatePuckCarry();
  void TryPickup(Entity &skater, int ownerTag);

  void DrawRink();
  void DrawHUD();
  void DrawText(const std::string &text, int x, int y, SDL_Color color,
                int ptSize = 22);
  glm::vec2 Center(const Entity &e, int size) const;

  static const std::string s_playID;

  SDL_Renderer *renderer_;
  int windowWidth_, windowHeight_;
  bool isDebugging_;
  AssetStore_Ptr assetStore_;
  Logger logger_;
  bool &isRunning_;

  Registry registry_;

  // Entities (stored so we can read their components each frame)
  Entity *playerEnt_ = nullptr;
  Entity *aiSkaEnt_ = nullptr;  // AI skater
  Entity *aiGoalEnt_ = nullptr; // AI goalie
  Entity *puckEnt_ = nullptr;

  // Input state (key-held tracking)
  bool keyUp_ = false, keyDown_ = false, keyLeft_ = false, keyRight_ = false;
  bool keyShoot_ = false;

  // Gamepad. The stick contributes a proportional direction, the d-pad a
  // full one; both are merged with the keyboard in UpdatePlayerMovement so
  // either input works at any time, and so does both at once.
  SDL_GameController *pad_ = nullptr;
  glm::vec2 padAxis_ = {0.f, 0.f};

  // Scores
  int playerScore_ = 0;
  int aiScore_ = 0;

  // Reset timer after goal
  float resetTimer_ = 0.f;
  bool goalScored_ = false;
  bool saveMade_ = false;
  bool gameOver_ = false;

  Uint32 lastTick_ = 0;

  TTF_Font *font_ = nullptr;
};
