#include "game.h"

#include <cstdlib>
#include <cstring>

// ── netplay-checkers: a 2D checkers game with a Teeworlds-style lobby ──────
//   ./bin/netplay-checkers host [port]   (host on the given port)
//   ./bin/netplay-checkers <ip> [port]   (join a host)
//
// The host runs an authoritative game server plus the full 2D scene; joiners
// click pieces and destinations on the shared board. See README.md for the
// full protocol and controls.

int main(int argc, char *argv[]) {
  bool host = true;
  std::string addr;
  uint16_t port = kDefaultPort;

  if (argc >= 2) {
    if (std::strcmp(argv[1], "host") == 0) {
      if (argc >= 3)
        port = (uint16_t)std::atoi(argv[2]);
    } else {
      host = false;
      addr = argv[1];
      if (argc >= 3)
        port = (uint16_t)std::atoi(argv[2]);
    }
  }

  Game game(host, addr, port);
  game.Run();
  game.Destroy();
  return 0;
}
