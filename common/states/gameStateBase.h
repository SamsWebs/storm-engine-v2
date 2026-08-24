#pragma once

// The GameState interface on its own, without the convenience includes.
//
// <stormengine2/states/gameState.h> pulls the whole engine in behind this -
// every component, every system, the AssetStore, the Logger and the
// TileMapLoader - which is 145,947 preprocessed lines for a 23-line interface.
// That is a fine trade for a small game that uses most of it, and a bad one for
// a large game that does not: Center Ice Hockey includes it in 38 files and
// uses none of the ECS, paying the cost 38 times over.
//
// This header is that interface and nothing else: 80,211 preprocessed lines,
// almost all of it SDL2, which CapFrameRate needs for SDL_GetTicks/SDL_Delay.
// Include it instead when your state does not want the rest of the engine, and
// include what you do use explicitly.
//
// gameState.h includes this file, so the two cannot drift and existing code
// sees no change.

#include <SDL2/SDL.h>
#include <string>
#include <vector>

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