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
// PRECONDITION: pass the same set of sources every frame. "Optional" means
// chosen once at startup, not toggled per frame. Dropping a source mid-hold
// is not a release and is not reported as one consistently: a keyboard or
// gamepad that disappears takes its held state with it and no release edge is
// produced, while a vpad or touch that disappears DOES produce one, because
// the previous-frame flag it is diffed against is still set. If a game must
// switch input sets at runtime, call Bind() again for the affected actions --
// that resets the edge state deliberately rather than leaving it describing
// sources that are no longer being read.
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
        // Reset the edge state with the binding. Carrying it across a rebind
        // describes the OLD binding's sources: a player still holding the
        // on-screen A while the action is rebound to a keyboard-only binding
        // would otherwise get a fabricated release on the next Update (the
        // stale statelessPrev with nothing holding it now), and a rebind onto
        // a source already held would swallow the press. It also made
        // `Bind(a, b)` behave differently from `Unbind(a); Bind(a, b);`,
        // which is the same operation spelled two ways.
        //
        // Resetting `down` is what keeps the removed press term redundant:
        // the invariant `statelessPrev implies down` must hold after every
        // mutation, so these two are reset together or not at all.
        entry.down = false;
        entry.pressed = false;
        entry.released = false;
        entry.statelessPrev = false;
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

      // Range-checked, not just compared against the sentinel: GamepadDown
      // indexes a fixed array with no bounds check of its own, and `pad` is a
      // public field a caller can set to any value a cast produces. The
      // keyboard path is guarded inside Keyboard::Test and the two stateless
      // paths are switches that fall through to false, so this was the only
      // unvalidated index in the header.
      if (sources.gamepad != nullptr && binding.pad >= GamepadButton::Up &&
          binding.pad < GamepadButton::Count) {
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
      //
      // The release also requires that the action was actually down. Without
      // that, a release edge arriving while the action was never held reports
      // a release with no press before it -- a source released across a Bind()
      // is the realistic case: the player holds a key through a state change,
      // the new state binds it in OnEnter, the player lets go, and the game
      // sees WasReleased for a press it never saw.
      //
      // `|| entry.pressed` is not redundant with `entry.down`: a key pressed
      // AND released inside one frame never appears in the held state at all,
      // so `down` is false throughout while `pressed` is true. Gating on
      // `down` alone would swallow the release half of a fast tap.
      // Order matters: the release has to see the press computed for THIS
      // frame, not the previous frame's. A within-frame tap sets both edges in
      // one Update, and reading the stale entry.pressed here swallowed the
      // release half of exactly the case the (down || pressed) gate exists to
      // preserve.
      const bool pressedNow = edgePressed && !entry.down;
      entry.released = edgeReleased && !held && (entry.down || pressedNow);
      entry.pressed = pressedNow;
      entry.down = held;
    }
  }

  // Builds the ActionSources the convenience overload below passes on.
  //
  // Public because it is the seam that makes that overload testable. It was
  // once documented as untestable -- Gamepad::Update() samples a real device,
  // so with nothing attached Current() and Previous() hold identical values
  // and no assertion on their CONTENTS can tell correct forwarding from
  // swapped forwarding. That reasoning missed that the two are distinct
  // objects at distinct addresses whether or not a device is attached, so
  // asserting on pointer identity settles it with no hardware at all.
  static ActionSources SourcesFrom(const Keyboard *keyboard,
                                   const Gamepad *gamepad,
                                   const VPadState *vpad,
                                   const TouchInput *touch) {
    ActionSources sources;
    sources.keyboard = keyboard;
    if (gamepad != nullptr) {
      sources.gamepad = &gamepad->Current();
      sources.gamepadPrevious = &gamepad->Previous();
    }
    sources.vpad = vpad;
    sources.touch = touch;
    return sources;
  }

  // Convenience overload for the common case.
  void Update(const Keyboard *keyboard, const Gamepad *gamepad,
              const VPadState *vpad, const TouchInput *touch) {
    Update(SourcesFrom(keyboard, gamepad, vpad, touch));
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
