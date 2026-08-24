#pragma once

#include <cmath>
#include <string>

#include <SDL2/SDL.h>

// A physical game controller, polled once a frame.
//
// This is NOT virtualGamepad.h. That one is an on-screen touch pad: it maps
// finger coordinates onto button flags and is SDL-free. This is the real
// thing, wrapping SDL_GameController.
//
// Two examples had grown their own copy of this, and the copies were already
// diverging - one had shoulder buttons and five extra accessors the other did
// not, and the comments recording why the teardown order matters existed twice.
//
// The state struct and the three query functions below are SDL-free and pure,
// so the part with the actual logic in it - edge detection and the deadzone
// rescale - is spec'd without a device attached.

enum class GamepadButton {
  Up,
  Down,
  Left,
  Right,
  A,
  B,
  X,
  Y,
  Start,
  Back,
  LeftShoulder,
  RightShoulder,
  Count
};

// One frame's worth of input. Sticks and triggers are already normalised:
// sticks to -1..1 with the deadzone removed, triggers to 0..1.
struct GamepadState {
  bool buttons[static_cast<int>(GamepadButton::Count)] = {};
  float leftX = 0.0f, leftY = 0.0f;
  float rightX = 0.0f, rightY = 0.0f;
  float triggerLeft = 0.0f, triggerRight = 0.0f;
};

inline bool GamepadDown(const GamepadState &state, GamepadButton button) {
  return state.buttons[static_cast<int>(button)];
}

// True only on the frame the button goes down. Menus need this, or holding the
// button retriggers on every frame.
inline bool GamepadPressed(const GamepadState &current,
                           const GamepadState &previous, GamepadButton button) {
  return GamepadDown(current, button) && !GamepadDown(previous, button);
}

inline bool GamepadReleased(const GamepadState &current,
                            const GamepadState &previous,
                            GamepadButton button) {
  return !GamepadDown(current, button) && GamepadDown(previous, button);
}

// Radial deadzone with rescaling. `rawX`/`rawY` are the Sint16 axis values as
// floats; `deadzone` is in the same units.
//
// The rescale is the part the hand-rolled copies got wrong. Thresholding alone
// - zero inside the deadzone, raw value outside - means the stick jumps
// straight to 24% of full speed the instant it crosses, and there is no way to
// ask for a slow walk. Subtracting the deadzone and stretching what is left
// back over 0..1 gives a continuous ramp from a standstill.
//
// Radial, not per-axis, so a diagonal push is not privileged over a straight
// one and the reachable set is a circle rather than a square.
inline void GamepadNormaliseStick(float rawX, float rawY, float deadzone,
                                  float *outX, float *outY) {
  *outX = 0.0f;
  *outY = 0.0f;

  const float magnitude = std::sqrt(rawX * rawX + rawY * rawY);
  if (magnitude <= deadzone) {
    return;
  }

  constexpr float kAxisMax = 32767.0f;
  const float span = kAxisMax - deadzone;
  if (span <= 0.0f) {
    return;
  }

  // Clamp so an axis reading -32768, or a diagonal whose magnitude exceeds the
  // per-axis maximum, cannot produce a vector longer than 1.
  float scaled = (magnitude - deadzone) / span;
  if (scaled > 1.0f) {
    scaled = 1.0f;
  }

  *outX = (rawX / magnitude) * scaled;
  *outY = (rawY / magnitude) * scaled;
}

class Gamepad {
public:
  // ~24% of the Sint16 range, the usual starting point.
  static constexpr float kDefaultDeadzone = 8000.0f;

  Gamepad() = default;
  ~Gamepad() { Shutdown(); }

  // Holds an SDL handle, so copying one would close the same controller twice.
  Gamepad(const Gamepad &) = delete;
  Gamepad &operator=(const Gamepad &) = delete;

  // **Call this before SDL_QuitSubSystem or SDL_Quit.** SDL_GameControllerQuit
  // force-closes and frees every open controller, so a destructor running
  // afterwards calls SDL_GameControllerClose on freed memory - and takes
  // SDL_LockJoysticks() after the joystick lock is gone. The destructor is
  // then a no-op second call.
  void Shutdown() { Close(); }

  // SDL only emits CONTROLLERDEVICEADDED for pads connected *after* init on
  // some platforms, so relying on the event alone misses one that was already
  // plugged in.
  void OpenFirstAttached() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
      if (SDL_IsGameController(i)) {
        Open(i);
        return;
      }
    }
  }

  // Feed device add/remove events. Everything else is polled in Update().
  void HandleEvent(const SDL_Event &event) {
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
      // Only take the first pad; a second one plugging in is ignored rather
      // than stealing control mid-game.
      if (!pad_) {
        Open(event.cdevice.which);
      }
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
      if (pad_ && event.cdevice.which == instanceId_) {
        Close();
        OpenFirstAttached(); // fall back to another pad if one is attached
      }
    }
  }

  // Samples the device. Polled rather than accumulated from events, so a pad
  // unplugged mid-hold cannot latch a direction on.
  void Update() {
    previous_ = current_;
    current_ = GamepadState{};

    if (!pad_) {
      return;
    }

    auto button = [this](SDL_GameControllerButton b) {
      return SDL_GameControllerGetButton(pad_, b) != 0;
    };
    auto set = [this](GamepadButton b, bool value) {
      current_.buttons[static_cast<int>(b)] = value;
    };

    set(GamepadButton::A, button(SDL_CONTROLLER_BUTTON_A));
    set(GamepadButton::B, button(SDL_CONTROLLER_BUTTON_B));
    set(GamepadButton::X, button(SDL_CONTROLLER_BUTTON_X));
    set(GamepadButton::Y, button(SDL_CONTROLLER_BUTTON_Y));
    set(GamepadButton::Start, button(SDL_CONTROLLER_BUTTON_START));
    set(GamepadButton::Back, button(SDL_CONTROLLER_BUTTON_BACK));
    set(GamepadButton::LeftShoulder,
        button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    set(GamepadButton::RightShoulder,
        button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));

    GamepadNormaliseStick(
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY), deadzone_,
        &current_.leftX, &current_.leftY);
    GamepadNormaliseStick(
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_RIGHTX),
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_RIGHTY), deadzone_,
        &current_.rightX, &current_.rightY);

    current_.triggerLeft =
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_TRIGGERLEFT) /
        32767.0f;
    current_.triggerRight =
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) /
        32767.0f;

    // The d-pad and the left stick both drive the directions, so either works
    // and a game binds once.
    set(GamepadButton::Left,
        button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || current_.leftX < -0.5f);
    set(GamepadButton::Right,
        button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || current_.leftX > 0.5f);
    set(GamepadButton::Up,
        button(SDL_CONTROLLER_BUTTON_DPAD_UP) || current_.leftY < -0.5f);
    set(GamepadButton::Down,
        button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || current_.leftY > 0.5f);
  }

  bool Connected() const { return pad_ != nullptr; }
  const std::string &Name() const { return name_; }

  const GamepadState &Current() const { return current_; }
  const GamepadState &Previous() const { return previous_; }

  bool Down(GamepadButton button) const {
    return GamepadDown(current_, button);
  }
  bool Pressed(GamepadButton button) const {
    return GamepadPressed(current_, previous_, button);
  }
  bool Released(GamepadButton button) const {
    return GamepadReleased(current_, previous_, button);
  }

  // Set before the first Update() if the default feels wrong for your game.
  void SetDeadzone(float deadzone) { deadzone_ = deadzone; }

private:
  void Open(int deviceIndex) {
    SDL_GameController *opened = SDL_GameControllerOpen(deviceIndex);
    if (!opened) {
      return;
    }
    Close();
    pad_ = opened;
    name_ = SDL_GameControllerName(opened) ? SDL_GameControllerName(opened)
                                           : "unknown controller";
    SDL_Joystick *joystick = SDL_GameControllerGetJoystick(opened);
    instanceId_ = joystick ? SDL_JoystickInstanceID(joystick) : -1;
  }

  void Close() {
    if (pad_) {
      SDL_GameControllerClose(pad_);
      pad_ = nullptr;
    }
    instanceId_ = -1;
    name_.clear();
    current_ = GamepadState{};
    previous_ = GamepadState{};
  }

  SDL_GameController *pad_ = nullptr;
  SDL_JoystickID instanceId_ = -1;
  std::string name_;
  float deadzone_ = kDefaultDeadzone;
  GamepadState current_;
  GamepadState previous_;
};
