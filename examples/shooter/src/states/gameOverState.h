#pragma once

#include <SDL2/SDL.h>
#include <stormengine2/gameStateMachine.h>

#include <stormengine2/input/inputHub.h>
#include <stormengine2/states/gameState.h>

#include <string>

using namespace storm;

class GameOverState : public GameState {
public:
  GameOverState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                bool isDebugging, AssetStore *assetStore,
                GameStateMachine *machine, InputHub *input, bool &isRunning,
                int finalScore, int wavesSurvived);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;

  std::string getStateID() const override { return s_overID; }

private:
  void ToMenu();

  static const std::string s_overID;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_;
  GameStateMachine *machine_;
  InputHub *input_;

  // This screen's own bindings and its own edges. Game Over is the screen that
  // motivated the split: SPACE is the fire button during play, so a player who
  // dies while holding it arrives here with SPACE still down. With edges shared
  // across screens that read as a press and the game left by itself a frame
  // later; with a per-screen map the key is simply already held, and the
  // dismiss needs a fresh press.
  enum class Action { Accept, AcceptKeypadEnter, Back };
  ActionMap actions_;
  bool &isRunning_;
  Logger logger_;

  int finalScore_;
  int wavesSurvived_;
  Uint32 enteredAt_ = 0;
  bool leaving_ = false;
};
