#pragma once

#include <SDL2/SDL.h>
#include <stormengine2/gameStateMachine.h>

#include <stormengine2/input/inputHub.h>
#include <stormengine2/states/gameState.h>

#include <string>

using namespace storm;

class MenuState : public GameState {
public:
  MenuState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore *assetStore, GameStateMachine *machine,
            InputHub *input, bool &isRunning);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;
  void resume() override;

  std::string getStateID() const override { return s_menuID; }

private:
  void SpawnAttractPlane(float x, float y, float speed, int row);

  static const std::string s_menuID;

  SDL_Renderer *renderer_;
  int windowWidth_;
  int windowHeight_;
  bool isDebugging_;
  AssetStore *assetStore_; // owned by Game, not by this state
  GameStateMachine *machine_;
  InputHub *input_;

  // This screen's bindings, and this screen's edges. Every state owns its own
  // ActionMap, which is what stops a new screen inheriting the previous
  // screen's "was down" -- a player holding RETURN through the menu -> game
  // change used to confirm instantly.
  enum class Action { Up, Down, Accept, AcceptKeypadEnter, Back };
  ActionMap actions_;
  bool &isRunning_;
  Logger logger_;

  Registry registry_;

  int selected_ = 0; // 0 = PLAY GAME ... 4 = QUIT
};
