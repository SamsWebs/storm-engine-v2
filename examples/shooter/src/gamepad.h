#pragma once

#include <SDL2/SDL.h>

#include <string>

// Physical game controller support (Xbox layout).
//
// The engine ships no gamepad abstraction at all -- `common/input/` is touch
// primitives only -- so this is plain SDL_GameController.
//
// Two things make the GameController API the right choice over SDL_Joystick:
// it maps any recognised pad onto a fixed Xbox-shaped model (A/B/X/Y, sticks,
// triggers) using SDL's built-in mapping database, and it reports face buttons
// by *meaning* rather than by index. A DualShock or a generic USB pad plugged
// into this game therefore lands on the same buttons without special cases.
//
// State is sampled once per frame rather than accumulated from events, so a
// disconnect mid-hold cannot latch a direction on forever.
class Gamepad {
public:
  ~Gamepad() { Close(); }

  // Must be called before SDL_QuitSubSystem/SDL_Quit. SDL_GameControllerQuit
  // force-closes and frees every open controller, so a destructor running
  // after SDL_Quit() calls SDL_GameControllerClose on freed memory -- and
  // takes SDL_LockJoysticks() after the joystick lock is gone. Game::Destroy
  // calls this explicitly; the destructor is then a no-op second call.
  void Shutdown() { Close(); }

  // Feed device add/remove events. Everything else is polled in Update().
  void HandleEvent(const SDL_Event &e) {
    if (e.type == SDL_CONTROLLERDEVICEADDED) {
      // Only take the first pad; a second one plugging in is ignored
      // rather than stealing control mid-game.
      if (!pad_) {
        Open(e.cdevice.which);
      }
    } else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
      if (pad_ && e.cdevice.which == instanceId_) {
        Close();
      }
    }
  }

  // Opens whatever is already plugged in at startup. SDL only emits
  // CONTROLLERDEVICEADDED for pads connected *after* init on some platforms,
  // so relying on the event alone misses a controller that was already there.
  void OpenFirstAttached() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
      if (SDL_IsGameController(i)) {
        Open(i);
        return;
      }
    }
  }

  void Update() {
    previous_ = current_;
    current_ = State{};

    if (!pad_) {
      return;
    }

    auto button = [&](SDL_GameControllerButton b) {
      return SDL_GameControllerGetButton(pad_, b) != 0;
    };

    // D-pad and left stick both drive movement, so either works.
    const Sint16 lx =
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 ly =
        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);

    current_.left = button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -DEADZONE;
    current_.right = button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > DEADZONE;
    current_.up = button(SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -DEADZONE;
    current_.down = button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > DEADZONE;

    current_.a = button(SDL_CONTROLLER_BUTTON_A);
    current_.b = button(SDL_CONTROLLER_BUTTON_B);
    current_.x = button(SDL_CONTROLLER_BUTTON_X);
    current_.y = button(SDL_CONTROLLER_BUTTON_Y);
    current_.start = button(SDL_CONTROLLER_BUTTON_START);
    current_.back = button(SDL_CONTROLLER_BUTTON_BACK);

    // Right trigger as an alternative fire, which is what most people
    // reach for in a shooter.
    current_.fire =
        current_.a || SDL_GameControllerGetAxis(
                          pad_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > DEADZONE;
  }

  bool Connected() const { return pad_ != nullptr; }
  const std::string &Name() const { return name_; }

  // Held
  bool Left() const { return current_.left; }
  bool Right() const { return current_.right; }
  bool Up() const { return current_.up; }
  bool Down() const { return current_.down; }
  bool Fire() const { return current_.fire; }

  // Edge-triggered: true only on the frame the button goes down. Menus and
  // the roll need this, or holding the button retriggers every frame.
  bool PressedA() const { return current_.a && !previous_.a; }
  bool PressedB() const { return current_.b && !previous_.b; }
  bool PressedX() const { return current_.x && !previous_.x; }
  bool PressedStart() const { return current_.start && !previous_.start; }
  bool PressedBack() const { return current_.back && !previous_.back; }
  bool PressedUp() const { return current_.up && !previous_.up; }
  bool PressedDown() const { return current_.down && !previous_.down; }

private:
  static constexpr Sint16 DEADZONE = 8000; // ~25% of 32767

  struct State {
    bool left = false, right = false, up = false, down = false;
    bool a = false, b = false, x = false, y = false;
    bool start = false, back = false;
    bool fire = false;
  };

  void Open(int deviceIndex) {
    SDL_GameController *c = SDL_GameControllerOpen(deviceIndex);
    if (!c) {
      return;
    }
    Close();
    pad_ = c;
    name_ = SDL_GameControllerName(c) ? SDL_GameControllerName(c)
                                      : "unknown controller";
    SDL_Joystick *js = SDL_GameControllerGetJoystick(c);
    instanceId_ = js ? SDL_JoystickInstanceID(js) : -1;
  }

  void Close() {
    if (pad_) {
      SDL_GameControllerClose(pad_);
      pad_ = nullptr;
    }
    instanceId_ = -1;
    name_.clear();
    current_ = State{};
    previous_ = State{};
  }

  SDL_GameController *pad_ = nullptr;
  SDL_JoystickID instanceId_ = -1;
  std::string name_;
  State current_;
  State previous_;
};
