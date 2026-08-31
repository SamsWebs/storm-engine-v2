#include "../../common/input/keyboard.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

namespace {

SDL_Event KeyEvent(Uint32 type, SDL_Scancode scancode, Uint8 repeat) {
  SDL_Event event{};
  event.type = type;
  event.key.type = type;
  event.key.repeat = repeat;
  event.key.keysym.scancode = scancode;
  return event;
}

} // namespace

Describe(KeyboardSpec) {
  It(should_report_a_key_as_down_after_a_keydown) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(true));
    Assert::That(keyboard.WasPressed(SDL_SCANCODE_SPACE), Equals(true));
  };

  It(should_clear_the_pressed_edge_on_the_next_frame) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    keyboard.BeginFrame();

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(true));
    Assert::That(keyboard.WasPressed(SDL_SCANCODE_SPACE), Equals(false));
  };

  It(should_report_the_released_edge_exactly_once) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE, 0));

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(false));
    Assert::That(keyboard.WasReleased(SDL_SCANCODE_SPACE), Equals(true));

    keyboard.BeginFrame();
    Assert::That(keyboard.WasReleased(SDL_SCANCODE_SPACE), Equals(false));
  };

  It(should_ignore_a_key_repeat_for_the_pressed_edge) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 1));

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(true));
    Assert::That(keyboard.WasPressed(SDL_SCANCODE_SPACE), Equals(false));
  };

  It(should_ignore_an_event_that_is_not_a_key_event) {
    Keyboard keyboard;
    keyboard.BeginFrame();

    SDL_Event quit{};
    quit.type = SDL_QUIT;
    keyboard.HandleEvent(quit);

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(false));
  };
};
