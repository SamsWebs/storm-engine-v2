#include "../../common/input/actionMap.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

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

  It(should_map_every_virtual_gamepad_control) {
    ActionMap actions;
    const VPadControl controls[] = {
        VPadControl::Up, VPadControl::Down, VPadControl::Left,
        VPadControl::Right, VPadControl::A, VPadControl::B,
        VPadControl::X, VPadControl::Y};
    for (int i = 0; i < 8; ++i) {
      ActionBinding binding;
      binding.vpad = controls[i];
      actions.Bind(i, binding);
    }

    VPadState vpad;
    vpad.up = vpad.down = vpad.left = vpad.right = true;
    vpad.a = vpad.b = vpad.x = vpad.y = true;

    ActionSources sources;
    sources.vpad = &vpad;
    actions.Update(sources);

    for (int i = 0; i < 8; ++i) {
      Assert::That(actions.IsDown(i), Equals(true));
    }

    // And each one independently: only `x` held resolves only the x action.
    VPadState onlyX;
    onlyX.x = true;
    sources.vpad = &onlyX;
    actions.Update(sources);

    for (int i = 0; i < 8; ++i) {
      Assert::That(actions.IsDown(i), Equals(controls[i] == VPadControl::X));
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

  It(should_map_every_touch_control) {
    ActionMap actions;
    const TouchControl controls[] = {TouchControl::Left, TouchControl::Right,
                                     TouchControl::Jump};
    for (int i = 0; i < 3; ++i) {
      ActionBinding binding;
      binding.touch = controls[i];
      actions.Bind(i, binding);
    }

    TouchInput touch;
    touch.right = true;

    ActionSources sources;
    sources.touch = &touch;
    actions.Update(sources);

    Assert::That(actions.IsDown(0), Equals(false)); // left
    Assert::That(actions.IsDown(1), Equals(true));  // right
    Assert::That(actions.IsDown(2), Equals(false)); // jump
  };
};
