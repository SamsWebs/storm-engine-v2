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

  // Sleeps out whatever is left of the frame budget, then returns how long the
  // frame actually took, in seconds, and rolls the timestamp forward. Call it
  // once at the top of update().
  //
  //     const double dt = CapFrameRate();
  //
  // Seven states had written this out by hand and five of them shadowed
  // `millisecondsPreviousFrame` with a member of their own to do it.
  //
  // `maxDeltaSeconds` clamps the result so one long hitch - a level load, a
  // breakpoint, a window drag - cannot teleport everything through a wall on
  // the next frame. Pass 0 to leave the delta unclamped.
  //
  // Non-virtual and adds no member, so it changes neither GameState's layout
  // nor its vtable.
  double CapFrameRate(double maxDeltaSeconds = 0.05) {
    const int remaining =
        MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);
    // The upper bound matters: a timestamp from the future, or one never
    // seeded, makes `remaining` enormous, and without the guard the state
    // would sleep for most of a minute.
    if (remaining > 0 && remaining <= MILLISECS_PER_FRAME) {
      SDL_Delay(remaining);
    }

    const Uint32 now = SDL_GetTicks();
    double delta = (now - millisecondsPreviousFrame) / 1000.0;
    millisecondsPreviousFrame = now;

    if (maxDeltaSeconds > 0.0 && delta > maxDeltaSeconds) {
      delta = maxDeltaSeconds;
    }
    return delta;
  }

  bool m_loadingComplete = false;
  bool m_exiting = false;
  int millisecondsPreviousFrame = 0;

  std::vector<std::string> m_textureIDList;
};