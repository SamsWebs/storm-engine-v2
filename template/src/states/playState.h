#pragma once

#include <string>

#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/contact.h>
#include <stormengine2/text.h>

class PlayState : public GameState {
public:
  PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            AssetStore_Ptr assetStore, bool &isRunning);

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;
  std::string getStateID() const override { return "PLAY"; }

private:
  SDL_Renderer *renderer_;
  int windowWidth_, windowHeight_;
  AssetStore_Ptr assetStore_;
  bool &isRunning_;

  Registry registry_;
  Uint32 millisecondsPreviousFrame_ = 0;
};
