#include "game.h"

#include <cstring>

// Optional first argument selects the starting state, so the play and
// game-over screens can be reached without driving the menu by hand:
//
//   ./bin/shooter          -> menu (normal)
//   ./bin/shooter play     -> straight into the game
//   ./bin/shooter gameover -> straight to the game-over screen
int main(int argc, char *argv[]) {
  Game::StartState start = Game::StartState::Menu;
  if (argc > 1) {
    if (std::strcmp(argv[1], "play") == 0) {
      start = Game::StartState::Play;
    } else if (std::strcmp(argv[1], "gameover") == 0) {
      start = Game::StartState::GameOver;
    }
  }

  Game game;
  game.Run(start);
  game.Destroy();
  return 0;
}
