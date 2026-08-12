#include <cstring>

#include "game.h"

// An optional first argument picks the starting screen, which saves driving the
// map every time one of the later ones is being worked on.
//
//   ./bin/realms             menu (normal)
//   ./bin/realms overworld   straight to the campaign map
//   ./bin/realms battle      straight into a fight
//   ./bin/realms gameover    straight to the end screen
int main(int argc, char *argv[]) {
    Game::StartState start = Game::StartState::Menu;
    if (argc > 1) {
        if (std::strcmp(argv[1], "overworld") == 0) {
            start = Game::StartState::Overworld;
        } else if (std::strcmp(argv[1], "battle") == 0) {
            start = Game::StartState::Battle;
        } else if (std::strcmp(argv[1], "gameover") == 0) {
            start = Game::StartState::GameOver;
        }
    }

    Game game;
    game.Run(start);
    game.Destroy();
    return 0;
}
