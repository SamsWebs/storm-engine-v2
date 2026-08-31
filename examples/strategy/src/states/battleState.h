#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/ecs.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/states/gameState.h>

#include "../world.h"
#include <stormengine2/input/gamepad.h>

#include <string>
#include <vector>

using namespace storm;

// The side-on mass battle. Pushed on top of OverworldState, which stays alive
// underneath; this state pops itself and writes its outcome into
// Campaign::result for the overworld's resume() to apply.
class BattleState : public GameState {
public:
  enum class Command { Charge, Hold, Volley, Retreat };

  BattleState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
              bool isDebugging, AssetStore *assetStore,
              GameStateMachine *machine, Gamepad *gamepad,
              world::Campaign *campaign, int attacker, int defender, int castle,
              bool &isRunning);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_battleID; }

private:
  static const std::string s_battleID;

  static constexpr int VISIBLE_PER_SIDE = 8;
  static constexpr int UNIT_FRAME = 192;
  static constexpr Uint32 TICK_MS = 500;     // damage resolution
  static constexpr Uint32 COMMAND_MS = 4000; // how long a command lasts
  static constexpr Uint32 OUTCOME_MS = 2200; // banner dwell before pop

  struct Side {
    world::Owner side = world::Owner::Blue;
    world::Troop troop = world::Troop::Warrior;
    int general = -1;
    int troops = 0;
    int startTroops = 0;
    Command command = Command::Hold;
    Uint32 commandUntil = 0;
    std::vector<Entity> soldiers;
  };

  // The player is always Blue. Orders follow the colour, not the
  // attacker/defender role, so that defending a castle does not hand the
  // command bar to the enemy.
  Side &PlayerSide();
  Side &AiSide();

  void SpawnSoldiers(Side &s, bool facingRight);
  void SetAnimation(Side &s, const char *suffix, int frames, bool looped);
  void ResolveTick();
  void SyncSoldierCount(Side &s);
  void SpawnHitEffect(glm::vec2 pos);
  void CullFinishedEffects();
  void IssueCommand(Command c);
  void ConfirmRetreat();
  void CancelRetreat();
  void RenderRetreatPrompt();
  void Finish(world::Owner winner);
  void RenderHud();

  std::string UnitAsset(const Side &s, const char *action) const;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_;
  GameStateMachine *machine_;
  Gamepad *gamepad_;
  world::Campaign *campaign_;
  bool &isRunning_;
  Logger logger_;

  Registry registry_;

  Side attacker_;
  Side defender_;
  int castle_ = -1;

  // The player is always Blue, so Blue always forms up on the left whether it
  // is attacking or defending. Tying the sides to attacker/defender instead
  // would flip the screen depending on who moved, which reads as the armies
  // having swapped places.
  bool attackerOnLeft_ = true;

  // One-shot animations stop on their last frame but do NOT remove the
  // entity, so every hit effect has to be culled by hand or they accumulate
  // for the whole battle.
  std::vector<Entity> effects_;

  // Battle runs: closing -> melee -> over.
  bool closed_ = false; // the lines have met
  bool over_ = false;

  // Retreat is the one order that cannot be undone -- it concedes the castle
  // the moment it lands -- so it asks first. While this is set the battle is
  // modal: damage resolution pauses and every other order is ignored.
  //
  // It is a flag rather than a pushed state on purpose. GameStateMachine
  // renders only m_gameStates.back(), so a modal pushed on top of the battle
  // would draw over a blank screen with the fight gone; an overlay has to be
  // drawn by the state it belongs to.
  bool confirmRetreat_ = false;
  Uint32 pausedAtMs_ = 0;
  bool popped_ = false;
  world::Owner winner_ = world::Owner::Neutral;

  Uint32 tickAccumMs_ = 0;
  Uint32 lastTicks_ = 0;
  Uint32 overAtMs_ = 0;
  float closeT_ = 0.0f; // 0 = at the edges, 1 = in contact
};
