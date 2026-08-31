#pragma once

#include <cstddef>
#include <vector>

#include "gamepad.h"
#include "keyboard.h"
#include "touchControls.h"
#include "virtualGamepad.h"

namespace storm {

// ── Action mapping ──────────────────────────────────────────────────────────
// Four input sources ship in this directory - keyboard.h, gamepad.h,
// virtualGamepad.h and touchControls.h - and before this header there was no
// way to bind them to one thing, so every game wrote its own
//
//     if (keyboard.WasPressed(SDL_SCANCODE_SPACE) ||
//         gamepad.Pressed(GamepadButton::A) || vpad.a || touch.jump) Jump();
//
// by hand, in every state, for every action. ActionMap resolves one game action
// across all four:
//
//     enum class Action { Jump, Left, Right };
//
//     ActionBinding jump;
//     jump.key = SDL_SCANCODE_SPACE;
//     jump.pad = GamepadButton::A;
//     jump.vpad = VPadControl::A;
//     jump.touch = TouchControl::Jump;
//     actions.Bind(static_cast<int>(Action::Jump), jump);
//
//     // once per frame, after feeding the keyboard its events and calling
//     // gamepad.Update():
//     ActionSources sources;
//     sources.keyboard = &keyboard;
//     sources.gamepad = &gamepad.Current();
//     sources.gamepadPrevious = &gamepad.Previous();
//     actions.Update(sources);
//
//     if (actions.WasPressed(static_cast<int>(Action::Jump))) Jump();
//
// Every source is optional: leave a pointer null and that source contributes
// nothing. A desktop game passes keyboard and gamepad; a phone build passes
// vpad and touch; nothing has to change in between.
//
// Header-only, holds no SDL resource, and safe to construct before SDL_Init.

// Which control on the on-screen virtual gamepad a binding listens to.
// `None` means the binding ignores the virtual gamepad.
enum class VPadControl { None, Up, Down, Left, Right, A, B, X, Y };

// Which zone of the simple three-zone touch scheme a binding listens to.
// `None` means the binding ignores touch.
enum class TouchControl { None, Left, Right, Jump };

// One action's bindings. Each field is independently optional, so an action can
// be keyboard-only, or bound on all four sources at once.
struct ActionBinding {
  SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
  // GamepadButton::Count is the unbound sentinel -- it is one past the last
  // real button, so it can never name one.
  GamepadButton pad = GamepadButton::Count;
  VPadControl vpad = VPadControl::None;
  TouchControl touch = TouchControl::None;
};

// The frame's input, as far as ActionMap is concerned. Any member may stay
// null.
//
// The gamepad is taken as two GamepadState snapshots rather than as a Gamepad,
// because GamepadState is a plain struct with no device behind it -- which is
// what lets the whole of this header be spec'd with no controller attached.
// Pass gamepad.Current() and gamepad.Previous(); the overload below does it
// for you.
struct ActionSources {
  const Keyboard *keyboard = nullptr;
  const GamepadState *gamepad = nullptr;
  const GamepadState *gamepadPrevious = nullptr;
  const VPadState *vpad = nullptr;
  const TouchInput *touch = nullptr;
};

class ActionMap {
public:
  // Binds an action id. Binding the same id twice replaces the first binding
  // rather than adding a second, so a game can rebind at runtime without
  // accumulating stale entries. The id is the game's own -- cast an enum class
  // to int.
  void Bind(int action, const ActionBinding &binding) {
    for (Entry &entry : entries_) {
      if (entry.action == action) {
        entry.binding = binding;
        return;
      }
    }
    Entry entry;
    entry.action = action;
    entry.binding = binding;
    entries_.push_back(entry);
  }

  void Unbind(int action) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].action == action) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(i));
        return;
      }
    }
  }

  void Clear() { entries_.clear(); }

  std::size_t Count() const { return entries_.size(); }

  // Call once per frame, after the frame's events have been fed to the
  // keyboard and the gamepad has been updated.
  //
  // Edge semantics with more than one source bound to an action: the action
  // goes down when the *first* source takes it and comes up when the *last*
  // one lets go. Pressing a second source while the action is already held
  // reports no new press, and releasing one of two held sources reports no
  // release. That is what a game means by "jump was pressed".
  //
  // Keyboard and gamepad edges come from those classes rather than being
  // recomputed here, so a key pressed and released inside a single frame is
  // still seen -- deriving edges purely from the held state would silently
  // drop fast taps, which is the reason Keyboard tracks presses separately at
  // all. The virtual gamepad and touch are stateless snapshots with no edges
  // of their own, so those are derived here against the previous frame.
  void Update(const ActionSources &sources) {
    for (Entry &entry : entries_) {
      const ActionBinding &binding = entry.binding;

      bool held = false;
      bool edgePressed = false;
      bool edgeReleased = false;

      if (sources.keyboard != nullptr &&
          binding.key != SDL_SCANCODE_UNKNOWN) {
        held = held || sources.keyboard->IsDown(binding.key);
        edgePressed = edgePressed || sources.keyboard->WasPressed(binding.key);
        edgeReleased =
            edgeReleased || sources.keyboard->WasReleased(binding.key);
      }

      if (sources.gamepad != nullptr && binding.pad != GamepadButton::Count) {
        held = held || GamepadDown(*sources.gamepad, binding.pad);
        if (sources.gamepadPrevious != nullptr) {
          edgePressed = edgePressed || GamepadPressed(*sources.gamepad,
                                                      *sources.gamepadPrevious,
                                                      binding.pad);
          edgeReleased = edgeReleased ||
                         GamepadReleased(*sources.gamepad,
                                         *sources.gamepadPrevious, binding.pad);
        }
      }

      // The two stateless sources are pooled: they have no edges of their own,
      // so one previous-frame flag covers both.
      bool stateless = false;
      if (sources.vpad != nullptr && binding.vpad != VPadControl::None) {
        stateless = stateless || VPadHeld(*sources.vpad, binding.vpad);
      }
      if (sources.touch != nullptr && binding.touch != TouchControl::None) {
        stateless = stateless || TouchHeld(*sources.touch, binding.touch);
      }
      held = held || stateless;

      // Only the release side needs the previous flag. On the press side
      // `stateless && !statelessPrev` is redundant: statelessPrev being true
      // implies held was true last frame, which implies entry.down is true,
      // and the `!entry.down` gate below already suppresses it. Mutation
      // testing confirmed no test could tell the two apart, so the term is
      // gone rather than left as unreachable-effect code.
      //
      // The release side is not redundant. Without the previous flag,
      // `!stateless` is true on every frame for an action nothing is touching,
      // and an idle action would report a release every single frame.
      edgePressed = edgePressed || stateless;
      edgeReleased = edgeReleased || (!stateless && entry.statelessPrev);
      entry.statelessPrev = stateless;

      // A press only counts if nothing was already holding the action, and a
      // release only counts once nothing holds it any more.
      entry.pressed = edgePressed && !entry.down;
      entry.released = edgeReleased && !held;
      entry.down = held;
    }
  }

  // Convenience overload for the common case. Equivalent to filling an
  // ActionSources with gamepad->Current() and gamepad->Previous().
  //
  // Not covered by the specs, and it cannot be: Gamepad::Update() samples a
  // real device, so with no controller attached Current() and Previous() are
  // both zeroed and identical. A spec cannot tell correct forwarding from
  // swapped forwarding. The specs cover the ActionSources form instead, which
  // takes the two states directly; this overload is the two-line adapter onto
  // it. Test it on hardware if you change it.
  void Update(const Keyboard *keyboard, const Gamepad *gamepad,
              const VPadState *vpad, const TouchInput *touch) {
    ActionSources sources;
    sources.keyboard = keyboard;
    if (gamepad != nullptr) {
      sources.gamepad = &gamepad->Current();
      sources.gamepadPrevious = &gamepad->Previous();
    }
    sources.vpad = vpad;
    sources.touch = touch;
    Update(sources);
  }

  // All three return false for an action that was never bound, rather than
  // reporting anything about an id the game did not register.
  bool IsDown(int action) const {
    const Entry *entry = Find(action);
    return entry != nullptr && entry->down;
  }

  bool WasPressed(int action) const {
    const Entry *entry = Find(action);
    return entry != nullptr && entry->pressed;
  }

  bool WasReleased(int action) const {
    const Entry *entry = Find(action);
    return entry != nullptr && entry->released;
  }

private:
  struct Entry {
    int action = 0;
    ActionBinding binding;
    bool down = false;
    bool pressed = false;
    bool released = false;
    // Held state of the virtual gamepad and touch on the previous Update.
    bool statelessPrev = false;
  };

  // Linear: an action map holds a handful of actions, and a flat vector keeps
  // iteration order stable and construction allocation-free.
  const Entry *Find(int action) const {
    for (const Entry &entry : entries_) {
      if (entry.action == action) {
        return &entry;
      }
    }
    return nullptr;
  }

  static bool VPadHeld(const VPadState &state, VPadControl control) {
    switch (control) {
    case VPadControl::Up:
      return state.up;
    case VPadControl::Down:
      return state.down;
    case VPadControl::Left:
      return state.left;
    case VPadControl::Right:
      return state.right;
    case VPadControl::A:
      return state.a;
    case VPadControl::B:
      return state.b;
    case VPadControl::X:
      return state.x;
    case VPadControl::Y:
      return state.y;
    case VPadControl::None:
      break;
    }
    return false;
  }

  static bool TouchHeld(const TouchInput &input, TouchControl control) {
    switch (control) {
    case TouchControl::Left:
      return input.left;
    case TouchControl::Right:
      return input.right;
    case TouchControl::Jump:
      return input.jump;
    case TouchControl::None:
      break;
    }
    return false;
  }

  std::vector<Entry> entries_;
};

} // namespace storm
