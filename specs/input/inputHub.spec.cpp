#include "../../common/input/inputHub.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

SDL_Event KeyEvent(Uint32 type, SDL_Scancode scancode, Uint8 repeat = 0) {
  SDL_Event event{};
  event.type = type;
  event.key.type = type;
  event.key.repeat = repeat;
  event.key.keysym.scancode = scancode;
  return event;
}

void PressKey(InputHub &hub, SDL_Scancode scancode) {
  hub.Feed(KeyEvent(SDL_KEYDOWN, scancode));
}

void ReleaseKey(InputHub &hub, SDL_Scancode scancode) {
  hub.Feed(KeyEvent(SDL_KEYUP, scancode));
}

enum SpecAction { Fire = 0, Thrust = 1 };

ActionBinding FireOnSpace() {
  ActionBinding binding;
  binding.key = SDL_SCANCODE_SPACE;
  return binding;
}

} // namespace

// The hub exists to fix three defects that all come from the same root: a game
// polling SDL from whichever state happens to be on top, keeping its own copy
// of the device list, and letting every state share one set of edge trackers.
//
// Every path here is driven with synthetic SDL_KEYDOWN/SDL_KEYUP events and a
// plain GamepadState seam, so the whole thing is spec'd with no keyboard
// attached and no controller plugged in -- the same seam keyboard.h and
// gamepad.h use for their own specs.
Describe(InputHubSpec){

    It(should_report_a_press_to_a_registered_map){InputHub hub;
ActionMap actions;
actions.Bind(Fire, FireOnSpace());
hub.RegisterMap(&actions);

hub.BeginFrame();
PressKey(hub, SDL_SCANCODE_SPACE);
hub.UpdateMaps();

Assert::That(actions.WasPressed(Fire), Equals(true));
}

It(should_keep_the_action_down_while_the_key_is_held) {
  InputHub hub;
  ActionMap actions;
  actions.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();
  Assert::That(actions.WasPressed(Fire), Equals(true));

  // Second frame, same key still down. The edge is gone; the hold is not.
  hub.BeginFrame();
  hub.UpdateMaps();
  Assert::That(actions.WasPressed(Fire), Equals(false));
  Assert::That(actions.IsDown(Fire), Equals(true));
}

It(should_report_a_release_only_after_a_press) {
  InputHub hub;
  ActionMap actions;
  actions.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  hub.BeginFrame();
  ReleaseKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  Assert::That(actions.WasReleased(Fire), Equals(true));
  Assert::That(actions.IsDown(Fire), Equals(false));
}

// ── DEFECT 3: a new screen must not inherit the previous screen's edges ──
//
// This is the one that a shared ActionMap cannot fix by itself, and the one
// a game hits constantly: a player holds SPACE through a state change, the
// next screen's map is built and bound, and the player lets go. The new
// screen must see nothing at all.

It(should_not_report_a_press_to_a_map_created_while_the_key_was_already_down) {
  InputHub hub;
  ActionMap oldScreen;
  oldScreen.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&oldScreen);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();
  Assert::That(oldScreen.WasPressed(Fire), Equals(true));

  // The player is STILL holding SPACE when the next screen appears.
  hub.BeginFrame();
  ActionMap newScreen;
  newScreen.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&newScreen);
  hub.UpdateMaps();

  Assert::That(newScreen.WasPressed(Fire), Equals(false));
}

It(should_not_report_a_release_to_a_map_that_never_saw_the_press) {
  InputHub hub;

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);

  // The state is pushed while the key is down, and the player lets go
  // before the new state's map is ever updated.
  ActionMap late;
  late.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&late);
  hub.BeginFrame();
  ReleaseKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  Assert::That(late.WasReleased(Fire), Equals(false));
  Assert::That(late.WasPressed(Fire), Equals(false));
}

It(should_give_two_maps_the_same_press_in_one_frame) {
  InputHub hub;
  ActionMap menus;
  ActionMap hud;
  menus.Bind(Fire, FireOnSpace());
  hud.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&menus);
  hub.RegisterMap(&hud);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  // Both live maps see it. The device state is shared; the EDGE bookkeeping
  // is per map, which is the whole point of the split.
  Assert::That(menus.WasPressed(Fire), Equals(true));
  Assert::That(hud.WasPressed(Fire), Equals(true));
}

It(should_isolate_edge_state_between_two_maps) {
  InputHub hub;
  ActionMap menus;
  ActionMap hud;
  menus.Bind(Fire, FireOnSpace());
  hud.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&menus);
  hub.RegisterMap(&hud);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  // Menus is unregistered mid-hold, exactly as a state being popped would.
  hub.UnregisterMap(&menus);
  hub.BeginFrame();
  ReleaseKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  // The surviving map still saw the press, so it is owed the release.
  Assert::That(hud.WasReleased(Fire), Equals(true));
}

It(should_stop_updating_a_map_that_was_unregistered) {
  InputHub hub;
  ActionMap gone;
  gone.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&gone);
  hub.UnregisterMap(&gone);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  Assert::That(gone.WasPressed(Fire), Equals(false));
}

It(should_ignore_a_map_registered_twice) {
  InputHub hub;
  ActionMap actions;
  actions.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&actions);
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  // A double registration would double-update the map and swallow its own
  // press edge on the second pass, so it is refused rather than tolerated.
  Assert::That(actions.WasPressed(Fire), Equals(true));
  Assert::That(hub.MapCount(), Equals(1u));
}

// ── DEFECT 1: the queue is drained whether or not a state wants it ──
//
// The freeze under the stack: a pad plugged in while a child screen is up was
// never enumerated, because only the top state polled. The hub polls; the
// stack is not consulted.

It(should_drain_events_with_no_map_registered_at_all) {
  InputHub hub;

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  // The key is down at the hub even though nobody was listening.
  Assert::That(hub.Keys().IsDown(SDL_SCANCODE_SPACE), Equals(true));
  Assert::That(hub.MapCount(), Equals(0u));
}

It(should_still_hold_a_key_pressed_while_no_state_was_listening) {
  InputHub hub;

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  // A state appears next frame. The key has been down the whole time, so the
  // new map must see a HOLD, not a press.
  hub.BeginFrame();
  ActionMap late;
  late.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&late);
  hub.UpdateMaps();

  Assert::That(late.IsDown(Fire), Equals(true));
  Assert::That(late.WasPressed(Fire), Equals(false));
}

It(should_count_frames_even_with_no_map_registered) {
  InputHub hub;
  Assert::That(hub.Frame(), Equals(0ull));

  hub.BeginFrame();
  Assert::That(hub.Frame(), Equals(1ull));
  hub.BeginFrame();
  Assert::That(hub.Frame(), Equals(2ull));
}

// ── a fast tap inside one frame ──
//
// Derived purely from the held state this would vanish, which is why
// Keyboard tracks presses separately at all. The hub must not lose it.

It(should_see_a_key_pressed_and_released_inside_one_frame) {
  InputHub hub;
  ActionMap actions;
  actions.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  ReleaseKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  Assert::That(actions.WasPressed(Fire), Equals(true));
  Assert::That(actions.WasReleased(Fire), Equals(true));
}

It(should_not_treat_auto_repeat_as_a_new_press) {
  InputHub hub;
  ActionMap actions;
  actions.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  hub.BeginFrame();
  hub.Feed(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, /*repeat=*/1));
  hub.UpdateMaps();

  Assert::That(actions.WasPressed(Fire), Equals(false));
  Assert::That(actions.IsDown(Fire), Equals(true));
}

// ── sources are optional, chosen once, as ActionMap already documents ──

It(should_carry_a_virtual_pad_and_touch_source) {
  VPadState vpad;
  TouchInput touch;
  vpad.a = true;
  touch.jump = true;

  InputHub hub;
  hub.SetVirtualPad(&vpad);
  hub.SetTouch(&touch);

  ActionBinding both;
  both.vpad = VPadControl::A;
  both.touch = TouchControl::Jump;

  ActionMap actions;
  actions.Bind(Fire, both);
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  hub.UpdateMaps();

  // An Android-shaped build uses the same hub with a different source set.
  Assert::That(actions.IsDown(Fire), Equals(true));
}

It(should_not_report_an_unbound_action) {
  InputHub hub;
  ActionMap actions;
  actions.Bind(Fire, FireOnSpace());
  hub.RegisterMap(&actions);

  hub.BeginFrame();
  PressKey(hub, SDL_SCANCODE_SPACE);
  hub.UpdateMaps();

  Assert::That(actions.WasPressed(Thrust), Equals(false));
  Assert::That(actions.IsDown(Thrust), Equals(false));
  Assert::That(actions.WasReleased(Thrust), Equals(false));
}
}
;
