#include <igloo/igloo_alt.h>

#include "../../common/input/gamepad.h"

using namespace igloo;
using namespace storm;

namespace {

GamepadState WithButton(GamepadButton button, bool down) {
  GamepadState state;
  state.buttons[static_cast<int>(button)] = down;
  return state;
}

float Magnitude(float x, float y) { return std::sqrt(x * x + y * y); }

} // namespace

// Headless: no controller, no SDL_Init. Everything here is the pure half of
// gamepad.h - the edge detection and the deadzone rescale, which is where the
// hand-rolled copies in the examples actually differed.
Describe(GamepadSpec) {

  It(reports_a_held_button) {
    const GamepadState held = WithButton(GamepadButton::A, true);
    Assert::That(GamepadDown(held, GamepadButton::A), IsTrue());
    Assert::That(GamepadDown(held, GamepadButton::B), IsFalse());
  };

  It(fires_pressed_only_on_the_frame_the_button_goes_down) {
    const GamepadState up = WithButton(GamepadButton::A, false);
    const GamepadState down = WithButton(GamepadButton::A, true);

    Assert::That(GamepadPressed(down, up, GamepadButton::A), IsTrue());
    // Still held on the next frame - must not retrigger.
    Assert::That(GamepadPressed(down, down, GamepadButton::A), IsFalse());
  };

  It(fires_released_only_on_the_frame_the_button_comes_up) {
    const GamepadState up = WithButton(GamepadButton::Start, false);
    const GamepadState down = WithButton(GamepadButton::Start, true);

    Assert::That(GamepadReleased(up, down, GamepadButton::Start), IsTrue());
    Assert::That(GamepadReleased(up, up, GamepadButton::Start), IsFalse());
  };

  It(treats_a_resting_stick_inside_the_deadzone_as_centred) {
    float x = 9.0f, y = 9.0f;
    GamepadNormaliseStick(6000.0f, -4000.0f, Gamepad::kDefaultDeadzone, &x, &y);
    Assert::That(x, EqualsWithDelta(0.0, 0.0001));
    Assert::That(y, EqualsWithDelta(0.0, 0.0001));
  };

  It(ramps_continuously_from_the_deadzone_edge_rather_than_jumping) {
    // The bug in every hand-rolled copy: thresholding alone means the stick
    // jumps straight to ~24% of full travel the instant it leaves the
    // deadzone, so a slow walk cannot be asked for.
    float x = 0.0f, y = 0.0f;
    GamepadNormaliseStick(Gamepad::kDefaultDeadzone + 1.0f, 0.0f,
                          Gamepad::kDefaultDeadzone, &x, &y);
    Assert::That(x > 0.0f, IsTrue());
    Assert::That(x < 0.01f, IsTrue());
  };

  It(reaches_full_travel_at_the_axis_maximum) {
    float x = 0.0f, y = 0.0f;
    GamepadNormaliseStick(32767.0f, 0.0f, Gamepad::kDefaultDeadzone, &x, &y);
    Assert::That(x, EqualsWithDelta(1.0, 0.0001));
  };

  It(never_returns_a_vector_longer_than_one) {
    // A full diagonal is magnitude 46341 on a square gate, well past the
    // per-axis maximum. It must clamp, or diagonal movement outruns straight.
    float x = 0.0f, y = 0.0f;
    GamepadNormaliseStick(32767.0f, 32767.0f, Gamepad::kDefaultDeadzone, &x,
                          &y);
    Assert::That(Magnitude(x, y) <= 1.0001f, IsTrue());
  };

  It(clamps_the_extreme_negative_axis_value) {
    // Sint16 runs to -32768, one past the positive maximum.
    float x = 0.0f, y = 0.0f;
    GamepadNormaliseStick(-32768.0f, 0.0f, Gamepad::kDefaultDeadzone, &x, &y);
    Assert::That(x >= -1.0001f, IsTrue());
  };

  It(is_radial_so_a_diagonal_is_not_faster_than_a_straight_push) {
    float sx = 0.0f, sy = 0.0f, dx = 0.0f, dy = 0.0f;
    GamepadNormaliseStick(32767.0f, 0.0f, Gamepad::kDefaultDeadzone, &sx, &sy);
    GamepadNormaliseStick(32767.0f, 32767.0f, Gamepad::kDefaultDeadzone, &dx,
                          &dy);
    Assert::That(Magnitude(dx, dy) <= Magnitude(sx, sy) + 0.0001f, IsTrue());
  };

  It(starts_disconnected_and_reports_an_all_clear_state) {
    Gamepad pad;
    Assert::That(pad.Connected(), IsFalse());
    Assert::That(pad.Down(GamepadButton::A), IsFalse());
    Assert::That(pad.Pressed(GamepadButton::A), IsFalse());
    Assert::That(pad.Current().leftX, EqualsWithDelta(0.0, 0.0001));
  };

  It(is_safe_to_update_and_shut_down_with_no_device) {
    Gamepad pad;
    pad.Update();
    pad.Update();
    pad.Shutdown();
    pad.Shutdown();
    Assert::That(pad.Connected(), IsFalse());
  };
};
