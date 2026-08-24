#include <SDL2/SDL_main.h> // Android: SDL owns the JNI entry point

#include "game.h"

int main(int argc, char *argv[]) {
  Game game;
  game.Run();
  game.Destroy();
  return 0;
}
