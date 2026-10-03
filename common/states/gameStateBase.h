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
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace storm {

constexpr int FPS = 60;
constexpr int MILLISECS_PER_FRAME = 1000 / FPS;

// A named, ordered list of things drawn ON TOP of the frame, immediately
// before it is presented.
//
// The motivation is a defect that shipped. On Android the touch overlay has to
// be drawn before the present; a state that calls SDL_RenderPresent itself
// ships a menu whose controls are invisible, and it looks perfect on every
// other platform, so nothing catches it. The fix in the game that hit this was
// a base class rather than a convention — and a convention is exactly what
// failed the first time, which is why this lives in the engine.
//
// WHY THE GAME OWNS THE LIST AND GameState ONLY BORROWS IT
//
// Present() takes the list as an argument instead of holding one as a member.
// That is the whole trick behind "adds no member": the storage lives in the
// derived class that actually has overlays, so GameState's size and vtable are
// untouched and a 2.4.x minor costs no game a layout change it did not ask for.
// A member here would have been simpler to write and would have made every
// GameState in every game bigger for a feature most of them never use.
//
// Draw order is REGISTRATION order, so a later overlay paints over an earlier
// one. That is the ordering a HUD wants (the debug readout on top of the
// vignette) and it is spec'd rather than left to the implementation, because
// the ordering is the part a caller can get wrong.
//
// Re-adding an existing name REPLACES THE CALLBACK IN PLACE and keeps its
// position. A HUD that re-registers itself every frame would otherwise climb
// to the top of the stack on every frame, making the visual order depend on how
// often something re-registered.
class OverlayList {
public:
  using Fn = std::function<void(SDL_Renderer *)>;

  // Returns false, changing nothing, for an empty name or a null callback. An
  // unnamed overlay cannot be removed or asserted about, and a null callback
  // would take the frame down on its first draw.
  bool Add(std::string name, Fn fn) {
    if (name.empty() || !fn) {
      return false;
    }
    for (auto &entry : m_items) {
      if (entry.first == name) {
        entry.second = std::move(fn);
        return true;
      }
    }
    m_items.emplace_back(std::move(name), std::move(fn));
    return true;
  }

  // Returns whether an overlay by that name was there to remove.
  bool Remove(const std::string &name) {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
      if (it->first == name) {
        m_items.erase(it);
        return true;
      }
    }
    return false;
  }

  bool Contains(const std::string &name) const {
    for (const auto &entry : m_items) {
      if (entry.first == name) {
        return true;
      }
    }
    return false;
  }

  void Clear() { m_items.clear(); }
  std::size_t Size() const { return m_items.size(); }
  bool IsEmpty() const { return m_items.empty(); }

  // In draw order. Out of range yields an empty string rather than throwing:
  // this header is reachable from Switch, which compiles with exceptions off.
  const std::string &NameAt(std::size_t index) const {
    static const std::string kNone;
    return index < m_items.size() ? m_items[index].first : kNone;
  }

  // Runs every overlay in registration order and returns how many ran. A null
  // renderer skips the calls rather than handing null to someone else's draw
  // code.
  std::size_t DrawAll(SDL_Renderer *renderer) const {
    if (renderer == nullptr) {
      return 0;
    }
    for (const auto &entry : m_items) {
      entry.second(renderer);
    }
    return m_items.size();
  }

private:
  std::vector<std::pair<std::string, Fn>> m_items;
};

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

  // Draws every overlay in registration order, then presents the frame — one
  // call, so the ordering cannot be got wrong by a caller:
  //
  //     void PlayState::render() {
  //       ...draw the world...
  //       Present(renderer_, overlays_);
  //     }
  //
  // The overlay list is the GAME's member, not GameState's; see OverlayList for
  // why that is what keeps this additive.
  //
  // WHAT THIS DOES NOT DO, stated plainly: it cannot stop a state calling
  // SDL_RenderPresent directly. Nothing in C++ can, short of owning the
  // renderer. What it does is make the correct call the shortest one and make
  // the overlay ordering a property of the call rather than of the caller's
  // memory. A state that keeps calling SDL_RenderPresent keeps getting exactly
  // the behaviour it has today, including the invisible Android menu — so
  // adopting this is the fix, and the sweep in tools/screen-sweep.py is what
  // tells you a state has not.
  //
  // Non-virtual and adds no member, so it changes neither GameState's layout
  // nor its vtable.
  void Present(SDL_Renderer *renderer,
               const OverlayList &overlays = OverlayList()) const {
    overlays.DrawAll(renderer);
    // The present is the last statement, and this is the ONLY SDL_RenderPresent
    // in common/ — `grep -rn SDL_RenderPresent common/` returned nothing before
    // this line was added, which is why the 2.4.1 audit starts by counting
    // them. That is the "exactly once" guarantee, and it is structural:
    // SDL_RenderPresent returns void and does nothing observable on a software
    // renderer, so no spec could assert it. See
    // specs/states/gameStatePresent.spec.cpp.
    SDL_RenderPresent(renderer);
  }

  bool m_loadingComplete = false;
  bool m_exiting = false;
  int millisecondsPreviousFrame = 0;

  std::vector<std::string> m_textureIDList;
};
} // namespace storm
