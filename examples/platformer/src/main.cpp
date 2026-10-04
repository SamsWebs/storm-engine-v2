// The engine is a library and cannot print, so "which engine was this built
// against" is a question the BINARY has to be able to answer. That is what
// <stormengine2/version.h> is for, and this switch is what makes it true in
// the tree rather than in theory: at least one built binary reports the
// version it was compiled against.
//
//     ./bin/platformer --version
//     v2.3.1
//
// Before this, a bug report could not say which engine produced it, and the
// answer lived in a makefile on the reporter's machine.
#include <cstring>
#include <iostream>

#include <stormengine2/version.h>

#include "game.h"

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--version") == 0 ||
        std::strcmp(argv[i], "-v") == 0) {
      // The engine's version, not this example's: the example is versioned
      // with the engine and has nothing separate to report.
      std::cout << storm::VersionString() << "\n";
      return 0;
    }
  }

  Game game;
  game.Run();
  game.Destroy();
  return 0;
}
