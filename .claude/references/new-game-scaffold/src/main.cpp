#include "game.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Game game;
    game.Run();
    game.Destroy();
    return 0;
}
