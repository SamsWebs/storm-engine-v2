#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sys/select.h>
#include <unistd.h>

#include <stormengine2/net/net.h>

using namespace storm;

// ── netchat: host or join a LAN chat over UDP ───────────────────────────────
//   ./bin/netchat host [port]
//   ./bin/netchat join <ip> <port> [name]
//
// Shows the hosting/joining loop, the connect/disconnect callbacks, and how
// game messages travel as reliable chunks (NetMessageWriter/NetMessageReader).
// The client prints its own messages only when the host echoes them back —
// proof the round trip works.

namespace {
constexpr int kMsgChat = 1; // game-defined message type ids start at 1
constexpr int kDefaultPort = 5000;
constexpr int kMaxNameLen = 32;
constexpr int kMaxTextLen = 256;
constexpr int kHostNameLen = 8;

// Non-blocking stdin read: returns false when nothing is waiting, so the
// network pump keeps running between lines of input.
bool ReadLine(std::string &out) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  timeval tv = {0, 0};
  if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
    return false;
  char buf[512];
  if (!fgets(buf, sizeof(buf), stdin))
    return false;
  out = buf;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return true;
}

// One chat message: [type id][sender name][text]
bool ParseChat(const NetChunk &chunk, char name[kMaxNameLen],
               char text[kMaxTextLen]) {
  NetMessageReader r(chunk.data, chunk.size);
  int32_t type = 0;
  if (!r.ReadInt(type) || type != kMsgChat)
    return false;
  return r.ReadString(name, kMaxNameLen) && r.ReadString(text, kMaxTextLen);
}

int RunHost(uint16_t port) {
  // NetServer is ~369 KB (16 connection slots inline), so it does not belong
  // on the stack. A desktop main() has 8 MB and hides this, but SDL runs the
  // game on its own thread on Android, where the stack is far smaller.
  auto serverOwner = std::make_unique<NetServer>();
  NetServer &server = *serverOwner;
  server.SetOnClientConnect(
      [&](int clientId) { printf("== client %d joined ==\n", clientId); });
  server.SetOnClientDisconnect([&](int clientId, const std::string &reason) {
    printf("== client %d left: %s ==\n", clientId, reason.c_str());
  });
  server.SetOnChunk([&](int clientId, const NetChunk &chunk) {
    char name[kMaxNameLen], text[kMaxTextLen];
    if (!ParseChat(chunk, name, text)) {
      printf("== bad message from client %d (%d bytes) ==\n", clientId,
             chunk.size);
      return;
    }
    printf("[%s] %s\n", name, text);
    server.Broadcast(chunk.data, chunk.size, true); // echo to everyone
  });
  if (!server.Start(port, 8)) {
    fprintf(stderr, "netchat: cannot bind port %u\n", port);
    return 1;
  }
  printf("chat host on port %u — type and press Enter; /quit exits\n",
         server.GetPort());
  for (;;) {
    server.Update();
    server.Poll();
    std::string line;
    if (ReadLine(line)) {
      if (line == "/quit")
        break;
      if (!line.empty()) {
        printf("[host] %s\n", line.c_str());
        NetMessageWriter msg;
        msg.WriteInt(kMsgChat);
        msg.WriteString("host");
        msg.WriteString(line.c_str());
        server.Broadcast(msg.Data(), msg.Size(), true);
      }
    }
    usleep(5000);
  }
  return 0;
}

int RunClient(const std::string &ip, uint16_t port, const std::string &name) {
  // ~200 KB — off the stack, same reasoning as RunHost above.
  auto clientOwner = std::make_unique<NetClient>();
  NetClient &client = *clientOwner;
  bool connected = false;
  bool sessionOver = false;
  client.SetOnConnect([&]() {
    connected = true;
    printf("== connected to %s:%u ==\n", ip.c_str(), port);
  });
  client.SetOnDisconnect([&](const std::string &reason) {
    connected = false;
    printf("== disconnected: %s ==\n", reason.c_str());
    sessionOver = true; // end the session (also on failed/timeout connect)
  });
  client.SetOnChunk([&](const NetChunk &chunk) {
    char sender[kMaxNameLen], text[kMaxTextLen];
    if (ParseChat(chunk, sender, text))
      printf("[%s] %s\n", sender, text);
  });
  if (!client.Connect(ip, port)) {
    fprintf(stderr, "netchat: cannot reach %s:%u\n", ip.c_str(), port);
    return 1;
  }
  printf("chatting as %s — type and press Enter; /quit exits\n", name.c_str());
  for (;;) {
    client.Update();
    client.Poll();
    if (sessionOver)
      break; // left the room, or the connect attempt failed/timed out
    std::string line;
    // Wait for the handshake: NetClient::Send drops chunks until the
    // connection is online, so nothing typed in the first few
    // milliseconds may be sent.
    if (connected && ReadLine(line)) {
      if (line == "/quit") {
        client.Disconnect("bye");
        break;
      }
      if (!line.empty()) {
        NetMessageWriter msg;
        msg.WriteInt(kMsgChat);
        msg.WriteString(name.c_str());
        msg.WriteString(line.c_str());
        if (!client.Send(msg.Data(), msg.Size(), true))
          printf("== send failed ==\n");
        // printed above when the host echoes it back
      }
    }
    usleep(5000);
  }
  return 0;
}
} // namespace

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string(argv[1]) == "host")
    return RunHost(argc >= 3 ? (uint16_t)atoi(argv[2]) : kDefaultPort);
  if (argc >= 3 && std::string(argv[1]) == "join") {
    uint16_t port = argc >= 4 ? (uint16_t)atoi(argv[3]) : kDefaultPort;
    std::string name = argc >= 5 ? argv[4] : "player";
    return RunClient(argv[2], port, name);
  }
  printf("usage:\n  netchat host [port]\n  netchat join <ip> <port> [name]\n");
  return 1;
}
