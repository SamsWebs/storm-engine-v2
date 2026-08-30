#pragma once

#include <SDL2/SDL.h>
#include <bitset>

// Edge-triggered keyboard state.
//
// It does not poll. The engine owns no main loop, so the game decides where
// SDL_PollEvent is called - and calling it in two places drains a queue that
// both share, which is how input goes missing. Feed this class the events you
// already pull:
//
//     keyboard.BeginFrame();
//     SDL_Event event;
//     while (SDL_PollEvent(&event)) {
//       keyboard.HandleEvent(event);
//       // ... your other event handling ...
//     }
//     if (keyboard.WasPressed(SDL_SCANCODE_SPACE)) { Jump(); }
//
// Header-only and holds no SDL resource, so it is safe to construct before
// SDL_Init and to keep by value in a state.
class Keyboard {
public:
  // Clears the press and release edges. Call once per frame, before feeding
  // the frame's events.
  void BeginFrame() {
    pressed_.reset();
    released_.reset();
  }

  void HandleEvent(const SDL_Event &event) {
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
      return;
    }

    const SDL_Scancode scancode = event.key.keysym.scancode;
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) {
      return;
    }
    const std::size_t index = static_cast<std::size_t>(scancode);

    if (event.type == SDL_KEYDOWN) {
      // SDL sends auto-repeat as further KEYDOWNs. A repeat is not a new
      // press, and treating it as one makes held keys fire every frame.
      if (event.key.repeat == 0 && !down_.test(index)) {
        pressed_.set(index);
      }
      down_.set(index);
    } else {
      if (down_.test(index)) {
        released_.set(index);
      }
      down_.reset(index);
    }
  }

  // Held right now.
  bool IsDown(SDL_Scancode scancode) const { return Test(down_, scancode); }

  // Went down during this frame.
  bool WasPressed(SDL_Scancode scancode) const {
    return Test(pressed_, scancode);
  }

  // Came up during this frame.
  bool WasReleased(SDL_Scancode scancode) const {
    return Test(released_, scancode);
  }

private:
  using KeyBits = std::bitset<SDL_NUM_SCANCODES>;

  static bool Test(const KeyBits &bits, SDL_Scancode scancode) {
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) {
      return false;
    }
    return bits.test(static_cast<std::size_t>(scancode));
  }

  KeyBits down_;
  KeyBits pressed_;
  KeyBits released_;
};
