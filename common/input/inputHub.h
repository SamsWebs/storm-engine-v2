#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <SDL2/SDL.h>

#include "actionMap.h"

namespace storm {

// ── The one place SDL_PollEvent belongs ─────────────────────────────────────
//
// actionMap.h answers "what does this action mean". It deliberately does not
// answer "where does the event queue get drained", and the gap between those
// two questions is where every game in this repo went wrong:
//
//   - the queue is drained by whichever state is on top, so a state pushed
//     UNDERNEATH freezes. A pad plugged in while a child screen is up is never
//     enumerated, because the screen underneath is not the one polling.
//   - hot-plug state is per-state, so two states disagree about whether a pad
//     exists and the one that answers last wins.
//   - edges are shared, so a new screen inherits the previous screen's "was
//     down" and a player who holds a key through a screen change gets a
//     fabricated press or release in the screen they land in.
//
// None of that is fixable inside ActionMap without breaking it: its per-action
// edge state is correct for one map, and the bug is that several maps share
// one device. So the split the rest of the engine uses applies here too --
// common/net/hostAddress.h and common/audio/mixer.h both put the RULE in a pure
// header and leave the mechanism thin. What is process-wide lives in the hub;
// what is per-screen lives in a map the screen owns.
//
//     InputHub input;                       // one per process, in main()
//
//     ActionMap actions;                    // one per state, owned by it
//     actions.Bind(kFire, binding);
//     input.RegisterMap(&actions);
//
//     // once per frame, BEFORE any state's update():
//     input.Poll();
//     input.UpdateMaps();
//
// RegisterMap takes a POINTER and the map stays owned by the state. The hub
// must not own the maps: a std::vector<ActionMap> inside the hub would
// reallocate under a state holding a reference to its own map, and the hub
// outliving or underliving a screen is the lifetime bug this class exists to
// remove.
//
// WHY THE HUB CAN SHARE ONE Keyboard ACROSS MANY MAPS. Keyboard holds held
// state in `down_` and frame edges in `pressed_`/`released_`, and BeginFrame
// clears only the edges. Within a frame those bits are readable any number of
// times -- they are not consumed -- so N maps reading the same press is safe.
// The per-screen bookkeeping is each map's own `down`/`pressed`/`released`,
// which is exactly what gives a new screen a clean edge state. Sharing the
// device and separating the edges is the whole design; the other arrangement
// is the bug.
//
// Header-only, and safe to construct before SDL_Init -- the same guarantee
// actionMap.h makes.

class InputHub {
public:
  // Clears this frame's edges and starts a new frame. Call once per frame,
  // before feeding events.
  void BeginFrame() {
    keyboard_.BeginFrame();
    ++frame_;
  }

  // The seam every path in this header is spec'd through.
  //
  // Feed the event to every device the hub owns. Constructing an SDL_Event by
  // hand is enough to drive all of it, which is why the specs can cover the
  // whole hub with no keyboard attached. Poll() is the only thing here that
  // needs a real device, and it is three lines.
  void Feed(const SDL_Event &event) {
    keyboard_.HandleEvent(event);
    gamepad_.HandleEvent(event);

    // The window-close event belongs to no device, but it cannot simply be
    // dropped: once the hub owns the poll and no state drains the queue,
    // nothing else in the game ever sees it and the window becomes
    // unclosable. SDL_QUIT is not a binding -- it is not an action a player
    // presses -- so it is a latch on the hub rather than an ActionMap entry.
    if (event.type == SDL_QUIT) {
      quit_ = true;
    }
  }

  // True if a close request has arrived, latched until ClearQuit(). Latched
  // rather than per-frame so a quit that arrives between a state's input
  // passes cannot be missed by a screen that is mid-transition.
  bool SawQuit() const { return quit_; }
  void ClearQuit() { quit_ = false; }

  // Drains SDL's queue and samples the pad. This is the one call in the engine
  // that belongs in exactly one place, and having it be a method is what stops
  // a second state from adding a second drain.
  //
  // Returns the number of events consumed, which is zero on a frame where SDL
  // was never initialised -- a headless build, or a test.
  std::size_t Poll() {
    BeginFrame();

    std::size_t drained = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      Feed(event);
      ++drained;
    }

    // After the drain, not before: a device-added event in this frame's queue
    // should hand back a pad that is sampled this frame rather than next.
    gamepad_.Update();
    return drained;
  }

  // Feeds every registered map the same sources. Call once per frame, after
  // Poll().
  //
  // Iterating in registration order keeps the order stable, and a map
  // registered twice is updated once -- updating it twice would feed it the
  // same press edge twice, and its own `down` is already true on the second
  // pass, so the press would be swallowed. That is a silent lost input, so the
  // double registration is refused at the door instead.
  void UpdateMaps() {
    const ActionSources sources = Sources();
    for (ActionMap *map : maps_) {
      map->Update(sources);
    }
  }

  // The sources every map is fed from. Exposed for a game that keeps its own
  // ActionMap outside the hub, which is the same seam actionMap.h offers.
  ActionSources Sources() const {
    return ActionMap::SourcesFrom(&keyboard_, &gamepad_, vpad_, touch_);
  }

  // Maps are owned by their state, so the hub holds pointers. A null map is
  // refused rather than stored, because a null in this vector is a crash in
  // UpdateMaps one frame later.
  void RegisterMap(ActionMap *map) {
    if (map == nullptr || Contains(map)) {
      return;
    }
    maps_.push_back(map);
  }

  void UnregisterMap(ActionMap *map) {
    for (std::size_t i = 0; i < maps_.size(); ++i) {
      if (maps_[i] == map) {
        maps_.erase(maps_.begin() + static_cast<std::ptrdiff_t>(i));
        return;
      }
    }
  }

  std::size_t MapCount() const { return maps_.size(); }

  // The stateless sources, chosen once at startup rather than per frame --
  // the same precondition actionMap.h documents for its own optional sources.
  // Passing a null pointer detaches that source; it is not a "disable this
  // frame" switch.
  void SetVirtualPad(const VPadState *vpad) { vpad_ = vpad; }
  void SetTouch(const TouchInput *touch) { touch_ = touch; }

  // Read-only on purpose: events reach the keyboard through Feed, so a game
  // cannot fork the edge state by writing it directly.
  const Keyboard &Keys() const { return keyboard_; }

  // Process-wide, which is the point. Two states asking here get the same
  // answer, because there is only one pad and one hub.
  const Gamepad &Pad() const { return gamepad_; }
  Gamepad &MutablePad() { return gamepad_; }

  // Frames since construction. A game can compare this against a map's last
  // seen frame to tell "no input this frame" from "nobody polled this frame".
  std::uint64_t Frame() const { return frame_; }

private:
  bool Contains(const ActionMap *map) const {
    for (const ActionMap *entry : maps_) {
      if (entry == map) {
        return true;
      }
    }
    return false;
  }

  // A Gamepad holds an SDL handle, so copying one would close the same
  // controller twice. Holding it by value makes the hub non-copyable, which
  // is the same contract Gamepad itself states.
  Keyboard keyboard_;
  Gamepad gamepad_;
  const VPadState *vpad_ = nullptr;
  const TouchInput *touch_ = nullptr;
  std::vector<ActionMap *> maps_;
  std::uint64_t frame_ = 0;
  bool quit_ = false;
};

} // namespace storm
