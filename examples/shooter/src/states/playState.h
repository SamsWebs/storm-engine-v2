#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/gameStateMachine.h>

#include <stormengine2/input/gamepad.h>
#include <stormengine2/states/gameState.h>

#include <glm/glm.hpp>
#include <optional>
#include <string>

class PlayState : public GameState {
public:
  PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore *assetStore, GameStateMachine *machine,
            Gamepad *gamepad, bool &isRunning);
  ~PlayState();

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_playID; }

private:
  // Wave shapes. A 1942 wave is a formation, not a diagonal drift.
  enum class Formation { Vee, LineAbreast };

  void SpawnPlayer();
  void SpawnEnemy(const glm::vec2 &pos);
  void SpawnBullet();
  void SpawnExplosion(glm::vec2 pos); // by value on purpose -- see the .cpp
  void SpawnWave();
  void LoseLife();
  void CullOffscreenEntities();
  void CheckCollisions();
  void RenderHud();
  bool PlayerInvulnerable(Uint32 now) const;

  static const std::string s_playID;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_; // owned by Game, shared across states
  GameStateMachine *machine_;
  Gamepad *gamepad_;
  bool &isRunning_;
  Logger logger_;

  Registry registry_;

  // Held in an optional rather than a bare Entity member: Entity's default
  // constructor does not initialise its registry pointer, so a
  // default-constructed member is unusable until assigned. The player is
  // never killed -- losing a life only repositions it -- so this cached
  // handle stays valid for the whole state.
  std::optional<Entity> player_;

  bool moveLeft_ = false;
  bool moveRight_ = false;
  bool moveUp_ = false;
  bool moveDown_ = false;
  bool spaceHeld_ = false;
  bool rollPressed_ = false;
  bool rolling_ = false;

  Uint32 lastShotMs_ = 0;
  Uint32 lastWaveMs_ = 0;
  Uint32 getReadyUntil_ = 0; // invulnerable, and the banner is up
  Uint32 waveCount_ = 0;
  int score_ = 0;
  int lives_ = 0;        // set from START_LIVES in the ctor
  bool leaving_ = false; // a changeState is already queued

  glm::vec2 playerStart_;

  int millisecondsPreviousFrame_ = 0;
};
