#pragma once

#include <SDL2/SDL.h>
#include <stormengine2/gameStateMachine.h>

#include <stormengine2/input/gamepad.h>
#include <stormengine2/states/gameState.h>

#include <string>

class MenuState : public GameState {
public:
  MenuState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore *assetStore, GameStateMachine *machine,
            Gamepad *gamepad, bool &isRunning);

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
  Gamepad *gamepad_;
  bool &isRunning_;
  Logger logger_;

  Registry registry_;

  int selected_ = 0; // 0 = PLAY GAME ... 4 = QUIT
};
