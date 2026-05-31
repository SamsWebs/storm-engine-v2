#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/ecs.h>
#include <stormengine2/graphics/engine.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/collision.h>
#include <stormengine2/systems/movement.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>

class PlayState : public GameState {
public:
  PlayState(SDL_Renderer *renderer, int windowWidth, bool isDebugging,
            AssetStore_Ptr assetStore);
  ~PlayState();

  virtual void processInput();
  virtual void update();
  virtual void render();

  virtual bool onEnter();
  virtual bool onExit();

  virtual std::string getStateID() const { return s_playID; }

private:
  static const std::string s_playID;
  SDL_Renderer *renderer_;
  int windowWidth_;
  bool isDebugging_;

  Logger_Ptr logger_;
  AssetStore_Ptr assetStore_;
};