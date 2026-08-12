#include "game.h"

int main(int argc, char *argv[]) {
    Game game;
    const bool ok = game.Run();
    game.Destroy();
    // Non-zero when the game never started. Returning 0 unconditionally made
    // every failure -- SDL init, window, renderer, a missing tileset or map --
    // look like a clean run to `make run`, to CI, and to any shell chaining on
    // it. Assets are all loaded relative to the working directory, so the usual
    // cause is being launched from somewhere other than the example root.
    return ok ? 0 : 1;
}
