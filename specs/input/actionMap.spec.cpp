#include "../../common/input/actionMap.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

SDL_Event ActionKeyEvent(Uint32 type, SDL_Scancode scancode, Uint8 repeat) {
  SDL_Event event{};
  event.type = type;
  event.key.type = type;
  event.key.repeat = repeat;
  event.key.keysym.scancode = scancode;
  return event;
}

enum SpecAction { Jump = 0, MoveLeft = 1, Unbound = 99 };

ActionBinding JumpOnEverything() {
  ActionBinding binding;
  binding.key = SDL_SCANCODE_SPACE;
  binding.pad = GamepadButton::A;
  binding.vpad = VPadControl::A;
  binding.touch = TouchControl::Jump;
  return binding;
}

} // namespace

// The gamepad is exercised through GamepadState rather than through Gamepad,
// so every path in this header is covered with no controller attached -- the
// same seam gamepad.h uses for its own specs.
Describe(ActionMapSpec) {

  It(should_report_nothing_for_an_action_that_was_never_bound) {
    ActionMap actions;
    ActionSources sources;
    actions.Update(sources);

    Assert::That(actions.IsDown(Unbound), Equals(false));
    Assert::That(actions.WasPressed(Unbound), Equals(false));
    Assert::That(actions.WasReleased(Unbound), Equals(false));
  };

  It(should_resolve_an_action_from_the_keyboard) {
    Keyboard keyboard;
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    ActionSources sources;
    sources.keyboard = &keyboard;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(true));
    Assert::That(actions.WasReleased(Jump), Equals(false));
  };

  It(should_resolve_the_same_action_from_the_gamepad) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    GamepadState previous;
    GamepadState current;
    current.buttons[static_cast<int>(GamepadButton::A)] = true;

    ActionSources sources;
    sources.gamepad = &current;
    sources.gamepadPrevious = &previous;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(true));
  };

  It(should_resolve_the_same_action_from_the_virtual_gamepad) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    VPadState vpad;
    vpad.a = true;

    ActionSources sources;
    sources.vpad = &vpad;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(true));
  };

  It(should_resolve_the_same_action_from_touch) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    TouchInput touch;
    touch.jump = true;

    ActionSources sources;
    sources.touch = &touch;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(true));
  };

  // A null source contributes nothing rather than being read. This is the
  // whole reason a desktop build and a phone build can share one binding
  // table.
  It(should_ignore_a_source_that_is_not_supplied) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    ActionSources sources; // every pointer null
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
    Assert::That(actions.WasPressed(Jump), Equals(false));

    // And no release either. The stateless sources have no edges of their
    // own, so a release derived from "not held" alone would fire on every
    // frame for an action nothing is touching -- which is the failure that
    // makes the previous-frame flag load-bearing.
    Assert::That(actions.WasReleased(Jump), Equals(false));
    actions.Update(sources);
    Assert::That(actions.WasReleased(Jump), Equals(false));
  };

  // A binding that names no control on a source must not pick up that
  // source's unrelated activity.
  It(should_ignore_a_source_the_binding_does_not_name) {
    ActionMap actions;
    ActionBinding keyboardOnly;
    keyboardOnly.key = SDL_SCANCODE_SPACE;
    actions.Bind(Jump, keyboardOnly);

    VPadState vpad;
    vpad.a = true;
    vpad.up = true;
    TouchInput touch;
    touch.jump = true;
    GamepadState previous;
    GamepadState current;
    current.buttons[static_cast<int>(GamepadButton::A)] = true;

    ActionSources sources;
    sources.vpad = &vpad;
    sources.touch = &touch;
    sources.gamepad = &current;
    sources.gamepadPrevious = &previous;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
  };

  It(should_bind_different_actions_to_different_controls) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    ActionBinding left;
    left.vpad = VPadControl::Left;
    left.touch = TouchControl::Left;
    actions.Bind(MoveLeft, left);

    VPadState vpad;
    vpad.left = true;

    ActionSources sources;
    sources.vpad = &vpad;
    actions.Update(sources);

    Assert::That(actions.IsDown(MoveLeft), Equals(true));
    Assert::That(actions.IsDown(Jump), Equals(false));
  };

  // The press edge is one frame wide. Holding must not retrigger it, or every
  // menu built on this fires on every frame.
  It(should_report_the_press_edge_only_once_while_held) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    VPadState vpad;
    vpad.a = true;
    ActionSources sources;
    sources.vpad = &vpad;

    actions.Update(sources);
    Assert::That(actions.WasPressed(Jump), Equals(true));

    actions.Update(sources); // still held
    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(false));
  };

  It(should_report_the_release_edge_when_a_stateless_source_lets_go) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    VPadState held;
    held.a = true;
    ActionSources sources;
    sources.vpad = &held;
    actions.Update(sources);

    VPadState idle;
    sources.vpad = &idle;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
    Assert::That(actions.WasReleased(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(false));
  };

  // The multi-source rule: down when the first source takes it, up when the
  // last one lets go. A second source arriving mid-hold is not a new press.
  It(should_not_report_a_second_press_when_another_source_joins_a_held_action) {
    Keyboard keyboard;
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    ActionSources sources;
    sources.keyboard = &keyboard;
    actions.Update(sources);
    Assert::That(actions.WasPressed(Jump), Equals(true));

    // Next frame: the key is still held, and touch joins it.
    keyboard.BeginFrame();
    TouchInput touch;
    touch.jump = true;
    sources.touch = &touch;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(false));
  };

  // And the mirror: one of two holders letting go is not a release.
  It(should_not_report_a_release_while_another_source_still_holds_the_action) {
    Keyboard keyboard;
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));
    TouchInput touch;
    touch.jump = true;

    ActionSources sources;
    sources.keyboard = &keyboard;
    sources.touch = &touch;
    actions.Update(sources);

    // The key comes up; touch is still holding.
    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE, 0));
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasReleased(Jump), Equals(false));
  };

  // A key pressed and released inside one frame never appears in the held
  // state. Deriving edges from held state alone would drop the tap entirely,
  // which is why keyboard edges are taken from Keyboard rather than recomputed.
  It(should_see_a_key_tapped_and_released_within_a_single_frame) {
    Keyboard keyboard;
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE, 0));

    ActionSources sources;
    sources.keyboard = &keyboard;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
    Assert::That(actions.WasPressed(Jump), Equals(true));
  };

  // Without gamepadPrevious there is nothing to take an edge against, so the
  // button reads as held with no press rather than reporting a press on every
  // frame.
  It(should_report_a_held_gamepad_button_without_a_previous_state) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    GamepadState current;
    current.buttons[static_cast<int>(GamepadButton::A)] = true;

    ActionSources sources;
    sources.gamepad = &current;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(false));
  };

  It(should_replace_a_binding_rather_than_adding_a_second) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    ActionBinding rebound;
    rebound.key = SDL_SCANCODE_W;
    actions.Bind(Jump, rebound);

    Assert::That(actions.Count(), Equals(1u));

    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    ActionSources sources;
    sources.keyboard = &keyboard;
    actions.Update(sources);

    // The old key no longer resolves the action.
    Assert::That(actions.IsDown(Jump), Equals(false));

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_W, 0));
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(true));
  };

  It(should_forget_an_unbound_action) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());
    actions.Bind(MoveLeft, JumpOnEverything());

    actions.Unbind(Jump);
    Assert::That(actions.Count(), Equals(1u));

    VPadState vpad;
    vpad.a = true;
    ActionSources sources;
    sources.vpad = &vpad;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
    Assert::That(actions.IsDown(MoveLeft), Equals(true));
  };

  // Every control held, then each control held ALONE in turn. The all-held
  // phase alone proves nothing about the mapping: with all eight flags set,
  // any permutation of the switch passes. Holding only `x` was barely better
  // -- it anchors X and leaves Up/Down, Left/Right, A/B and Y free to swap,
  // since every one of those reads false either way. Only the full sweep
  // fails when two cases in VPadHeld are exchanged.
  // A release edge arriving for an action that was never down must not be
  // reported. The realistic route is a source released across a state change:
  // the player holds SPACE, the new state binds it in OnEnter, the player lets
  // go, and the game would see a release for a press it never saw.
  It(should_not_report_a_release_for_an_action_that_was_never_down) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    // The button was down last frame and is up now, but this map has never
    // seen it held.
    GamepadState previous;
    previous.buttons[static_cast<int>(GamepadButton::A)] = true;
    GamepadState current;

    ActionSources sources;
    sources.gamepad = &current;
    sources.gamepadPrevious = &previous;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
    Assert::That(actions.WasPressed(Jump), Equals(false));
    Assert::That(actions.WasReleased(Jump), Equals(false));
  };

  // ... but the release half of a within-frame tap must still be reported.
  // The tap never appears in the held state, so gating the release on `down`
  // alone would swallow it -- which is why the gate is (down || pressed).
  It(should_still_report_the_release_half_of_a_within_frame_tap) {
    Keyboard keyboard;
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE, 0));

    ActionSources sources;
    sources.keyboard = &keyboard;
    actions.Update(sources);

    Assert::That(actions.WasPressed(Jump), Equals(true));
    Assert::That(actions.WasReleased(Jump), Equals(true));
    Assert::That(actions.IsDown(Jump), Equals(false));
  };

  // Rebinding must reset the edge state with the binding. Stale state
  // describes the OLD binding's sources.
  It(should_not_fabricate_a_release_when_rebinding_a_held_action) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    VPadState vpad;
    vpad.a = true;
    ActionSources sources;
    sources.vpad = &vpad;
    actions.Update(sources);
    Assert::That(actions.IsDown(Jump), Equals(true));

    // Rebound to keyboard only, while the player is still touching the
    // on-screen A. The vpad no longer contributes to this action at all.
    ActionBinding keyboardOnly;
    keyboardOnly.key = SDL_SCANCODE_SPACE;
    actions.Bind(Jump, keyboardOnly);

    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
    Assert::That(actions.WasReleased(Jump), Equals(false));
    Assert::That(actions.WasPressed(Jump), Equals(false));
  };

  // And the mirror: rebinding onto a source that IS held reports a fresh
  // press, the same as Unbind-then-Bind. The two spellings of the same
  // operation must not diverge.
  It(should_report_a_press_when_rebinding_onto_a_held_source) {
    VPadState vpad;
    vpad.a = true;
    ActionSources sources;
    sources.vpad = &vpad;

    ActionMap rebound;
    ActionBinding keyboardOnly;
    keyboardOnly.key = SDL_SCANCODE_SPACE;
    rebound.Bind(Jump, keyboardOnly);
    rebound.Update(sources);
    rebound.Bind(Jump, JumpOnEverything());
    rebound.Update(sources);

    ActionMap recreated;
    recreated.Bind(Jump, keyboardOnly);
    recreated.Update(sources);
    recreated.Unbind(Jump);
    recreated.Bind(Jump, JumpOnEverything());
    recreated.Update(sources);

    Assert::That(rebound.WasPressed(Jump), Equals(true));
    Assert::That(rebound.WasPressed(Jump), Equals(recreated.WasPressed(Jump)));
    Assert::That(rebound.IsDown(Jump), Equals(recreated.IsDown(Jump)));
  };

  // `pad` is a public field and GamepadDown indexes a fixed array with no
  // bounds check of its own, so a value outside the enum's range must be
  // rejected here rather than read out of bounds.
  It(should_ignore_a_gamepad_button_outside_the_valid_range) {
    ActionMap actions;
    ActionBinding bogus;
    bogus.pad = static_cast<GamepadButton>(50);
    actions.Bind(Jump, bogus);

    GamepadState previous;
    GamepadState current;
    ActionSources sources;
    sources.gamepad = &current;
    sources.gamepadPrevious = &previous;
    actions.Update(sources);

    Assert::That(actions.IsDown(Jump), Equals(false));
  };

  // The convenience overload forwards Current() and Previous() in that order.
  // Their VALUES are identical with no controller attached, which is what made
  // this look untestable -- but they are distinct objects, so pointer identity
  // distinguishes correct forwarding from swapped forwarding with no hardware.
  It(should_forward_the_gamepad_current_and_previous_states_in_order) {
    Keyboard keyboard;
    Gamepad gamepad;
    VPadState vpad;
    TouchInput touch;

    const ActionSources sources =
        ActionMap::SourcesFrom(&keyboard, &gamepad, &vpad, &touch);

    Assert::That(sources.gamepad == &gamepad.Current(), Equals(true));
    Assert::That(sources.gamepadPrevious == &gamepad.Previous(), Equals(true));
    Assert::That(sources.keyboard == &keyboard, Equals(true));
    Assert::That(sources.vpad == &vpad, Equals(true));
    Assert::That(sources.touch == &touch, Equals(true));
  };

  It(should_leave_the_gamepad_states_null_when_no_gamepad_is_passed) {
    const ActionSources sources =
        ActionMap::SourcesFrom(nullptr, nullptr, nullptr, nullptr);
    Assert::That(sources.gamepad == nullptr, Equals(true));
    Assert::That(sources.gamepadPrevious == nullptr, Equals(true));
  };

  It(should_map_every_virtual_gamepad_control) {
    ActionMap actions;
    const VPadControl controls[] = {
        VPadControl::Up, VPadControl::Down, VPadControl::Left,
        VPadControl::Right, VPadControl::A, VPadControl::B,
        VPadControl::X, VPadControl::Y};
    bool VPadState::*fields[] = {&VPadState::up, &VPadState::down,
                                 &VPadState::left, &VPadState::right,
                                 &VPadState::a,  &VPadState::b,
                                 &VPadState::x,  &VPadState::y};
    for (int i = 0; i < 8; ++i) {
      ActionBinding binding;
      binding.vpad = controls[i];
      actions.Bind(i, binding);
    }

    VPadState all;
    all.up = all.down = all.left = all.right = true;
    all.a = all.b = all.x = all.y = true;

    ActionSources sources;
    sources.vpad = &all;
    actions.Update(sources);
    for (int i = 0; i < 8; ++i) {
      Assert::That(actions.IsDown(i), Equals(true));
    }

    for (int held = 0; held < 8; ++held) {
      VPadState only;
      only.*fields[held] = true;
      sources.vpad = &only;
      actions.Update(sources);
      for (int i = 0; i < 8; ++i) {
        Assert::That(actions.IsDown(i), Equals(i == held));
      }
    }
  };

  // The convenience overload is the call most games will actually write, so
  // it needs its own case -- forwarding Current()/Previous() off the Gamepad
  // is code, and untested code that only appears in a comment example is how
  // an example that does not compile ships.
  It(should_accept_the_four_argument_convenience_overload) {
    Keyboard keyboard;
    Gamepad gamepad; // no device attached; contributes nothing
    VPadState vpad;
    TouchInput touch;

    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    keyboard.BeginFrame();
    keyboard.HandleEvent(ActionKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    actions.Update(&keyboard, &gamepad, &vpad, &touch);

    Assert::That(gamepad.Connected(), Equals(false));
    Assert::That(actions.IsDown(Jump), Equals(true));
    Assert::That(actions.WasPressed(Jump), Equals(true));
  };

  // A null Gamepad through the overload must not be dereferenced.
  It(should_accept_null_sources_through_the_convenience_overload) {
    ActionMap actions;
    actions.Bind(Jump, JumpOnEverything());

    actions.Update(nullptr, nullptr, nullptr, nullptr);

    Assert::That(actions.IsDown(Jump), Equals(false));
  };

  // Same sweep, same reason: setting only `right` left Left and Jump free to
  // swap in TouchHeld, because both read false either way.
  It(should_map_every_touch_control) {
    ActionMap actions;
    const TouchControl controls[] = {TouchControl::Left, TouchControl::Right,
                                     TouchControl::Jump};
    bool TouchInput::*fields[] = {&TouchInput::left, &TouchInput::right,
                                  &TouchInput::jump};
    for (int i = 0; i < 3; ++i) {
      ActionBinding binding;
      binding.touch = controls[i];
      actions.Bind(i, binding);
    }

    ActionSources sources;
    for (int held = 0; held < 3; ++held) {
      TouchInput only;
      only.*fields[held] = true;
      sources.touch = &only;
      actions.Update(sources);
      for (int i = 0; i < 3; ++i) {
        Assert::That(actions.IsDown(i), Equals(i == held));
      }
    }
  };
};
