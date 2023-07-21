#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

#include "../assetStore.h"
#include "../components/animation.h"
#include "../components/boxCollider.h"
#include "../components/rigidBody.h"
#include "../components/sprite.h"
#include "../components/transform.h"
#include "../ecs.h"
#include "../logger.h"
#include "../systems/animation.h"
#include "../systems/collision.h"
#include "../systems/movement.h"
#include "../systems/render.h"
#include "../systems/renderCollider.h"
#include "../tilemapLoader.h"

constexpr int FPS = 60;
constexpr int MILLISECS_PER_FRAME = 1000 / FPS;

class GameState {
public:
  virtual ~GameState() {}

  virtual void processInput() = 0;
  virtual void update() = 0;
  virtual void render() = 0;

  virtual bool onEnter() = 0;
  virtual bool onExit() = 0;

  virtual void resume() {}

  virtual std::string getStateID() const = 0;

protected:
  GameState() {}

  bool m_loadingComplete = false;
  bool m_exiting = false;
  int millisecondsPreviousFrame = 0;

  std::vector<std::string> m_textureIDList;

  static AssetStore assetStore;
};