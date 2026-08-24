#include "playState.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <glm/glm.hpp>

const std::string PlayState::s_playID = "PLAY";

// ─────────────────────────────────────────────────────────────────────────────
// Board model + rules (host authority)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

int Square(int col, int row) { return row * kBoardSize + col; }
int ColOf(int sq) { return sq % kBoardSize; }
int RowOf(int sq) { return sq / kBoardSize; }
bool IsDark(int col, int row) { return (col + row) % 2 == 1; }
bool IsRedPiece(uint8_t p) { return p == kPieceRed || p == kPieceRedKing; }
bool IsBlackPiece(uint8_t p) {
  return p == kPieceBlack || p == kPieceBlackKing;
}
bool IsKing(uint8_t p) { return p == kPieceRedKing || p == kPieceBlackKing; }
bool IsEnemy(uint8_t p, uint8_t mine) {
  return (IsRedPiece(mine) && IsBlackPiece(p)) ||
         (IsBlackPiece(mine) && IsRedPiece(p));
}
uint8_t Promote(uint8_t p) {
  return p == kPieceRed ? kPieceRedKing : kPieceBlackKing;
}
const char *PieceAsset(uint8_t p) {
  switch (p) {
  case kPieceRed:
    return "piece_red";
  case kPieceBlack:
    return "piece_black";
  case kPieceRedKing:
    return "piece_red_king";
  case kPieceBlackKing:
    return "piece_black_king";
  }
  return "piece_red";
}

void SetupBoard(CheckersState &g) {
  for (int c = 0; c < kBoardSize; c++) {
    for (int r = 0; r < kBoardSize; r++) {
      if (!IsDark(c, r))
        continue;
      if (r >= 5)
        g.board[Square(c, r)] = kPieceRed; // ranks 3..1 (bottom)
      else if (r <= 2)
        g.board[Square(c, r)] = kPieceBlack; // ranks 8..6 (top)
    }
  }
}

// One square in one direction: true if `to` is a legal non-capturing hop.
bool CanStep(const CheckersState &g, int from, int to, uint8_t piece) {
  if (to < 0 || to >= kBoardSize * kBoardSize || from < 0 ||
      from >= kBoardSize * kBoardSize)
    return false;
  int dc = ColOf(to) - ColOf(from);
  int dr = RowOf(to) - RowOf(from);
  if (std::abs(dc) != 1 || std::abs(dr) != 1)
    return false;
  if (!IsKing(piece)) {
    int dir = IsRedPiece(piece) ? -1 : 1; // red moves up the board
    if (dr != dir)
      return false;
  }
  return g.board[to] == kPieceNone;
}

// Two squares in one direction, jumping an enemy: true if legal.
bool CanJump(const CheckersState &g, int from, int to, uint8_t piece) {
  if (to < 0 || to >= kBoardSize * kBoardSize || from < 0 ||
      from >= kBoardSize * kBoardSize)
    return false;
  int dc = ColOf(to) - ColOf(from);
  int dr = RowOf(to) - RowOf(from);
  if (std::abs(dc) != 2 || std::abs(dr) != 2)
    return false;
  if (!IsKing(piece)) {
    int dir = IsRedPiece(piece) ? -1 : 1;
    if (dr != 2 * dir)
      return false;
  }
  int mid = Square(ColOf(from) + dc / 2, RowOf(from) + dr / 2);
  return IsEnemy(g.board[mid], piece) && g.board[to] == kPieceNone;
}

bool HasCapture(const CheckersState &g, int square) {
  uint8_t p = g.board[square];
  if (p == kPieceNone)
    return false;
  for (int dc = -2; dc <= 2; dc += 4) {
    for (int dr = -2; dr <= 2; dr += 4) {
      int tc = ColOf(square) + dc;
      int tr = RowOf(square) + dr;
      if (tc < 0 || tc >= kBoardSize || tr < 0 || tr >= kBoardSize)
        continue;
      if (CanJump(g, square, Square(tc, tr), p))
        return true;
    }
  }
  return false;
}

bool PlayerHasCapture(const CheckersState &g, int color) {
  for (int i = 0; i < kBoardSize * kBoardSize; i++) {
    uint8_t p = g.board[i];
    if (p != kPieceNone && IsRedPiece(p) == (color == kColorRed) &&
        HasCapture(g, i))
      return true;
  }
  return false;
}

// Validate and apply a move path on the authoritative board. Returns nullptr
// on success (and commits the change), otherwise an error string. `chainFrom`
// is the persistent "capture chain in progress" square: once a player makes a
// capturing hop with more captures available, the chain must continue from
// that landing square until it is played out.
const char *TryMove(CheckersState &g, int color, const std::vector<int> &path,
                    int &chainFrom) {
  if ((int)path.size() < 2)
    return "a move needs at least two squares";
  int from = path[0];
  if (from < 0 || from >= kBoardSize * kBoardSize)
    return "square out of range";
  uint8_t p = g.board[from];
  if (p == kPieceNone)
    return "no piece on that square";
  if (IsRedPiece(p) != (color == kColorRed))
    return "that is not your piece";
  if (g.turn != color)
    return "not your turn";
  if (g.winner >= 0)
    return "the game is over";
  if (chainFrom >= 0 && path[0] != chainFrom)
    return "complete your capture chain";

  CheckersState t = g; // work on a copy so a rejected chain changes nothing
  uint8_t piece = p;
  int cur = from;
  bool anyJump = false;
  bool promoted = false;
  int captured = 0;
  for (size_t i = 1; i < path.size(); i++) {
    int to = path[i];
    if (CanStep(t, cur, to, piece)) {
      t.board[cur] = kPieceNone;
      t.board[to] = piece;
    } else if (CanJump(t, cur, to, piece)) {
      int mid =
          Square((ColOf(cur) + ColOf(to)) / 2, (RowOf(cur) + RowOf(to)) / 2);
      t.board[mid] = kPieceNone;
      t.board[cur] = kPieceNone;
      t.board[to] = piece;
      anyJump = true;
      captured++;
    } else {
      return "illegal hop";
    }
    // Landing on the far rank promotes immediately; a king can keep jumping.
    int farRank = IsRedPiece(piece) ? 0 : kBoardSize - 1;
    if (!IsKing(piece) && RowOf(to) == farRank) {
      piece = Promote(piece);
      t.board[to] = piece;
      promoted = true;
    }
    cur = to;
  }
  // Forced capture: if the player had any capture before this move, the
  // move must capture (checked against the pre-move board — a position
  // reached by the move can legally expose a capture for later).
  if (!anyJump && PlayerHasCapture(g, color))
    return "you must capture";

  g = t;
  g.moveCount++;
  g.lastFrom = from;
  g.lastTo = cur;
  g.lastCaptured = captured;
  g.justPromoted = promoted;
  if (anyJump && HasCapture(g, cur)) {
    chainFrom = cur; // the chain continues: the same player moves again
  } else {
    chainFrom = -1;
    g.turn = 1 - color;
  }
  return nullptr;
}

// The player to move has no piece left or no legal move: they lose.
int WinnerOf(const CheckersState &g) {
  int red = 0, black = 0;
  for (int i = 0; i < kBoardSize * kBoardSize; i++) {
    if (IsRedPiece(g.board[i]))
      red++;
    else if (IsBlackPiece(g.board[i]))
      black++;
  }
  if (red == 0)
    return kColorBlack;
  if (black == 0)
    return kColorRed;
  for (int i = 0; i < kBoardSize * kBoardSize; i++) {
    uint8_t p = g.board[i];
    if (p == kPieceNone)
      continue;
    if (IsRedPiece(p) != (g.turn == kColorRed))
      continue;
    if (HasCapture(g, i))
      return -1;
    for (int dc = -1; dc <= 1; dc += 2) {
      for (int dr = -1; dr <= 1; dr += 2) {
        int tc = ColOf(i) + dc;
        int tr = RowOf(i) + dr;
        if (tc < 0 || tc >= kBoardSize || tr < 0 || tr >= kBoardSize)
          continue;
        if (CanStep(g, i, Square(tc, tr), p))
          return -1;
      }
    }
  }
  return 1 - g.turn; // the side to move is stuck
}

std::string SquareToNotation(int sq) {
  char buf[3];
  buf[0] = (char)('a' + ColOf(sq));
  buf[1] = (char)('1' + (kBoardSize - 1 - RowOf(sq)));
  buf[2] = '\0';
  return buf;
}

int ParseSquare(const char *s, int &sq) {
  if (!std::isalpha(s[0]) || !std::isdigit(s[1]))
    return 0;
  int col = std::tolower(s[0]) - 'a';
  int rank = s[1] - '0';
  if (col < 0 || col >= kBoardSize || rank < 1 || rank > kBoardSize)
    return 0;
  sq = Square(col, kBoardSize - rank);
  return 2;
}

// "a3b4", "a3 b4", or a capture chain like "a3c5e3" -> squares.
bool ParseMove(const std::string &in, std::vector<int> &path) {
  path.clear();
  std::string flat;
  for (char ch : in)
    if (!std::isspace((unsigned char)ch))
      flat += (char)std::tolower((unsigned char)ch);
  if (flat.size() < 4 || flat.size() % 2 != 0)
    return false;
  for (size_t i = 0; i < flat.size(); i += 2) {
    int sq = 0;
    if (ParseSquare(flat.c_str() + i, sq) != 2)
      return false;
    path.push_back(sq);
  }
  return true;
}

// UTF-8 safe truncation: keep at most maxBytes bytes that end on a character
// boundary. Used to fit HUD text into fixed-width panels.
static std::string TruncateUTF8(const std::string &s, int maxBytes) {
  if ((int)s.size() <= maxBytes)
    return s;
  std::string out;
  int n = 0;
  size_t i = 0;
  while (i < s.size()) {
    size_t len = 1;
    unsigned char c = (unsigned char)s[i];
    if ((c & 0xE0) == 0xC0)
      len = 2;
    else if ((c & 0xF0) == 0xE0)
      len = 3;
    else if ((c & 0xF8) == 0xF0)
      len = 4;
    if (n + (int)len > maxBytes)
      break;
    out.append(s, i, len);
    n += (int)len;
    i += len;
  }
  return out + "\xE2\x80\xA6"; // …
}

void WriteGame(NetMessageWriter &w, const CheckersState &g) {
  w.WriteInt(kMsgGame);
  w.WriteInt(g.turn);
  w.WriteInt(g.winner);
  w.WriteInt(g.moveCount);
  w.WriteInt(g.lastFrom);
  w.WriteInt(g.lastTo);
  w.WriteInt(g.lastCaptured);
  w.WriteInt(g.justPromoted ? 1 : 0);
  w.WriteInt(g.chainFrom);
  w.WriteRaw(g.board, kBoardSize * kBoardSize);
}

bool ReadGame(NetMessageReader &r, CheckersState &g) {
  int32_t promoted = 0;
  if (!r.ReadInt(g.turn) || !r.ReadInt(g.winner) || !r.ReadInt(g.moveCount) ||
      !r.ReadInt(g.lastFrom) || !r.ReadInt(g.lastTo) ||
      !r.ReadInt(g.lastCaptured) || !r.ReadInt(promoted) ||
      !r.ReadInt(g.chainFrom))
    return false;
  if (!r.ReadRaw(g.board, kBoardSize * kBoardSize))
    return false;
  g.justPromoted = promoted != 0;
  return true;
}

bool SameState(const CheckersState &a, const CheckersState &b) {
  return a.turn == b.turn && a.winner == b.winner &&
         a.moveCount == b.moveCount && a.lastFrom == b.lastFrom &&
         a.lastTo == b.lastTo && a.lastCaptured == b.lastCaptured &&
         a.justPromoted == b.justPromoted && a.chainFrom == b.chainFrom &&
         std::memcmp(a.board, b.board, sizeof(a.board)) == 0;
}

glm::vec2 SquareToPos(int col, int row) {
  return glm::vec2{BOARD_X + col * SQUARE_PX, BOARD_Y + row * SQUARE_PX};
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore,
                     bool &isRunning, bool host, const std::string &joinAddr,
                     uint16_t port)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{std::move(assetStore)},
      isRunning_{isRunning}, host_{host}, joinAddr_{joinAddr}, port_{port} {
  // Note: GameStateMachine::changeState calls onEnter() after pushing this
  // state, so initialization happens there (not here).
}

// GameStateMachine::clean() calls onExit() before deleting this state, so
// teardown happens there (not here — calling it from both places would
// double-free the audio device and SDL subsystems).
PlayState::~PlayState() {}

// ─────────────────────────────────────────────────────────────────────────────
// onEnter — load assets, register systems, start the network role
// ─────────────────────────────────────────────────────────────────────────────

bool PlayState::onEnter() {
  if (TTF_Init() != 0) {
    logger_.Err("TTF_Init failed: " + std::string(TTF_GetError()));
  }
  // One id per point size the panel draws at. DrawText used to open a font
  // from disk on every call for any size other than 18, then close it again.
  for (int pt : {14, 16, 18, 20, 40}) {
    assetStore_->AddFont("hud-" + std::to_string(pt), "assets/fonts/font.ttf",
                         pt);
  }

  // Load textures
  assetStore_->AddTexture(renderer_, "board", "assets/gfx/board.png");
  assetStore_->AddTexture(renderer_, "piece_red", "assets/gfx/piece_red.png");
  assetStore_->AddTexture(renderer_, "piece_black",
                          "assets/gfx/piece_black.png");
  assetStore_->AddTexture(renderer_, "piece_red_king",
                          "assets/gfx/piece_red_king.png");
  assetStore_->AddTexture(renderer_, "piece_black_king",
                          "assets/gfx/piece_black_king.png");
  assetStore_->AddTexture(renderer_, "highlight", "assets/gfx/highlight.png");
  assetStore_->AddTexture(renderer_, "moveMark", "assets/gfx/moveMark.png");

  // Register systems
  registry_.AddSystem<RenderSystem>();

  SpawnBoardEntities();
  InitAudio();
  SDL_StartTextInput();

  // Scripted input channel: when stdin is not a tty (automated tests),
  // commands and moves can be fed as text lines instead of mouse clicks.
  if (!isatty(STDIN_FILENO)) {
    scriptInput_ = true;
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0)
      fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  }

  if (host_) {
    server_.SetOnClientConnect([this](int clientId) {
      printf("== client %d connected ==\n", clientId);
      Seat seat;
      seat.clientId = clientId;
      seat.name = "player" + std::to_string(clientId);
      bool redTaken = false, blackTaken = false;
      for (const auto &s : seats_) {
        redTaken = redTaken || s.color == kColorRed;
        blackTaken = blackTaken || s.color == kColorBlack;
      }
      if (!redTaken)
        seat.color = kColorRed;
      else if (!blackTaken)
        seat.color = kColorBlack;
      seats_.push_back(seat);
      printf("== %s seated as %s ==\n", seat.name.c_str(),
             seat.color == kColorRed
                 ? "RED"
                 : seat.color == kColorBlack ? "BLACK" : "spectator");
      SendSeat(clientId);
      BroadcastLobby();
      if (started_ || game_.winner >= 0)
        BroadcastGame(); // late joiner: full sync on the first state
    });
    server_.SetOnClientDisconnect(
        [this](int clientId, const std::string &reason) {
          printf("== client %d left: %s ==\n", clientId, reason.c_str());
          for (auto it = seats_.begin(); it != seats_.end(); ++it) {
            if (it->clientId != clientId)
              continue;
            int leftColor = it->color;
            seats_.erase(it);
            if (started_ && game_.winner < 0 &&
                (leftColor == kColorRed || leftColor == kColorBlack)) {
              game_.winner = 1 - leftColor;
              game_.chainFrom = -1;
              started_ = false;
              Announce("the other player left — game over");
              RebuildPieces();
              BroadcastGame();
            }
            BroadcastLobby();
            return;
          }
        });
    server_.SetOnChunk([this](int clientId, const NetChunk &chunk) {
      NetMessageReader r(chunk.data, chunk.size);
      int32_t type = 0;
      if (!r.ReadInt(type))
        return;
      Seat *seat = FindSeat(clientId);
      const char *who = seat ? seat->name.c_str() : "unknown";
      switch (type) {
      case kMsgChat: {
        char text[256];
        if (!r.ReadString(text, sizeof(text)))
          return;
        printf("[%s] %s\n", who, text);
        chatLog_.push_back({seat ? seat->name : "unknown", text});
        NetMessageWriter w;
        w.WriteInt(kMsgChat);
        w.WriteString(who);
        w.WriteString(text);
        server_.Broadcast(w.Data(), w.Size(), true);
        break;
      }
      case kMsgCmd: {
        char cmd[256];
        if (!r.ReadString(cmd, sizeof(cmd)))
          return;
        HandleCommand(clientId, cmd);
        break;
      }
      case kMsgMove: {
        int32_t len = 0;
        if (!r.ReadInt(len) || len < 2 || len > 16)
          return;
        std::vector<int> path;
        for (int i = 0; i < len; i++) {
          int32_t sq = 0;
          if (!r.ReadInt(sq))
            return;
          path.push_back(sq);
        }
        HandleClientMove(clientId, path);
        break;
      }
      }
    });
    if (!server_.Start(port_, 8)) {
      fprintf(stderr, "netplay-checkers: cannot bind port %u\n", port_);
      isRunning_ = false;
      return false;
    }
    printf("checkers host on port %u — join with: netplay-checkers <ip> "
           "%u\n",
           server_.GetPort(), port_);
    printf("lobby commands: start, reset, quit\n");
  } else {
    client_.SetOnConnect([this]() {
      printf("== connected to %s:%u — in the lobby ==\n", joinAddr_.c_str(),
             port_);
      NetMessageWriter w;
      w.WriteInt(kMsgCmd);
      w.WriteString(
          ("name player" + std::to_string(SDL_GetTicks() % 1000)).c_str());
      client_.Send(w.Data(), w.Size(), true);
    });
    client_.SetOnDisconnect([this](const std::string &reason) {
      printf("== disconnected: %s ==\n", reason.c_str());
      quitting_ = true;
    });
    client_.SetOnChunk([this](const NetChunk &chunk) {
      NetMessageReader r(chunk.data, chunk.size);
      int32_t type = 0;
      if (!r.ReadInt(type))
        return;
      switch (type) {
      case kMsgChat: {
        char who[64], text[256];
        if (!r.ReadString(who, sizeof(who)) ||
            !r.ReadString(text, sizeof(text)))
          return;
        printf("[%s] %s\n", who, text);
        chatLog_.push_back({who, text});
        break;
      }
      case kMsgLobby: {
        int32_t n = 0;
        if (!r.ReadInt(n))
          return;
        seats_.clear();
        printf("== lobby: ");
        for (int i = 0; i < n; i++) {
          int32_t color = -1;
          char nm[64];
          if (!r.ReadInt(color) || !r.ReadString(nm, sizeof(nm)))
            return;
          seats_.push_back({-1, nm, color});
          if (i)
            printf(", ");
          printf("%s %s",
                 color == kColorRed ? "RED"
                                    : color == kColorBlack ? "BLACK" : "spec",
                 nm);
        }
        printf(" ==\n");
        break;
      }
      case kMsgSeat: {
        int32_t color = -1;
        if (!r.ReadInt(color))
          return;
        myColor_ = color;
        printf("== you are seated as %s ==\n",
               color == kColorRed
                   ? "RED"
                   : color == kColorBlack ? "BLACK" : "spectator");
        break;
      }
      case kMsgGame: {
        if (!ReadGame(r, game_))
          return;
        bool changed = !hasPrev_ || !SameState(prev_, game_);
        prev_ = game_;
        hasPrev_ = true;
        if (!changed)
          return;
        haveGame_ = true;
        RebuildPieces();
        PlayStateSounds();
        PrintBoard();
        break;
      }
      }
    });
    if (!client_.Connect(joinAddr_, port_)) {
      fprintf(stderr, "netplay-checkers: cannot reach %s:%u\n",
              joinAddr_.c_str(), port_);
      isRunning_ = false;
      return false;
    }
    printf("commands: start, reset, ff, a3b4 (a move), anything else is "
           "chat\n");
  }

  return true;
}

bool PlayState::onExit() {
  if (host_) {
    for (int i = 0; i < NetServer::kMaxClients; i++) {
      if (server_.IsClientConnected(i))
        server_.DisconnectClient(i, "host quit");
    }
    server_.Stop();
  } else {
    client_.Disconnect("bye");
  }

  SDL_StopTextInput();

  for (Entity e : pieces_)
    registry_.KillEntity(e);
  pieces_.clear();
  if (selMark_) {
    registry_.KillEntity(*selMark_);
    delete selMark_;
    selMark_ = nullptr;
  }
  if (fromMark_) {
    registry_.KillEntity(*fromMark_);
    delete fromMark_;
    fromMark_ = nullptr;
  }
  if (toMark_) {
    registry_.KillEntity(*toMark_);
    delete toMark_;
    toMark_ = nullptr;
  }
  registry_.Update();

  // Before TTF_Quit() and CloseAudio(): both free everything they own, so a
  // store cleared afterwards hands already-freed pointers to TTF_CloseFont and
  // Mix_FreeChunk. This state owns the AssetStore, so its destructor would
  // otherwise run the clear far too late.
  assetStore_->ClearAssets();
  TTF_Quit();
  CloseAudio();
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene setup
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::SpawnBoardEntities() {
  // Board sprite
  Entity board = registry_.CreateEntity();
  board.AddComponent<TransformComponent>(glm::vec2{BOARD_X, BOARD_Y},
                                         glm::vec2{1.f, 1.f}, 0.0);
  board.AddComponent<SpriteComponent>("board", BOARD_PX, BOARD_PX, 0);

  // Selection ring (above pieces) and last-move marks (below/above)
  selMark_ = new Entity(registry_.CreateEntity());
  selMark_->AddComponent<TransformComponent>(glm::vec2{-999.f, -999.f},
                                             glm::vec2{1.f, 1.f}, 0.0);
  selMark_->AddComponent<SpriteComponent>("highlight", SQUARE_PX, SQUARE_PX, 3);

  fromMark_ = new Entity(registry_.CreateEntity());
  fromMark_->AddComponent<TransformComponent>(glm::vec2{-999.f, -999.f},
                                              glm::vec2{1.f, 1.f}, 0.0);
  fromMark_->AddComponent<SpriteComponent>("moveMark", SQUARE_PX, SQUARE_PX, 1);

  toMark_ = new Entity(registry_.CreateEntity());
  toMark_->AddComponent<TransformComponent>(glm::vec2{-999.f, -999.f},
                                            glm::vec2{1.f, 1.f}, 0.0);
  toMark_->AddComponent<SpriteComponent>("moveMark", SQUARE_PX, SQUARE_PX, 3);

  registry_.Update();
}

void PlayState::RebuildPieces() {
  for (Entity e : pieces_)
    registry_.KillEntity(e);
  pieces_.clear();
  for (int sq = 0; sq < kBoardSize * kBoardSize; sq++) {
    uint8_t p = game_.board[sq];
    if (p == kPieceNone)
      continue;
    Entity e = registry_.CreateEntity();
    e.AddComponent<TransformComponent>(
        SquareToPos(ColOf(sq), RowOf(sq)) +
            glm::vec2{PIECE_MARGIN, PIECE_MARGIN},
        glm::vec2{1.f, 1.f}, 0.0);
    e.AddComponent<SpriteComponent>(PieceAsset(p), PIECE_PX, PIECE_PX, 2);
    pieces_.push_back(e);
  }
}

void PlayState::UpdateMarks() {
  auto place = [this](Entity *e, int sq) {
    if (!e)
      return;
    e->GetComponent<TransformComponent>().position =
        sq >= 0 ? SquareToPos(ColOf(sq), RowOf(sq)) : glm::vec2{-999.f, -999.f};
  };
  place(selMark_, selectedSquare_);
  bool showLast = (host_ && started_) || haveGame_;
  place(fromMark_, showLast && game_.lastFrom >= 0 ? game_.lastFrom : -1);
  place(toMark_, showLast && game_.lastFrom >= 0 ? game_.lastTo : -1);
}

// ─────────────────────────────────────────────────────────────────────────────
// processInput
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::processInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      isRunning_ = false;
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
        isRunning_ = false;
        break;
      case SDLK_BACKSPACE:
        if (!inputText_.empty())
          inputText_.pop_back();
        break;
      case SDLK_RETURN:
        SendInputLine();
        break;
      case SDLK_s:
        if (host_ && !started_)
          StartGame();
        break;
      case SDLK_r:
        if (host_)
          ResetToLobby();
        break;
      }
      break;
    case SDL_TEXTINPUT:
      if (inputText_.size() < 48)
        inputText_ += event.text.text;
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (event.button.button == SDL_BUTTON_LEFT)
        HandleClick(event.button.x, event.button.y);
      break;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::update() {
  if (host_) {
    server_.Update();
    server_.Poll();
  } else {
    client_.Update();
    client_.Poll();
    if (quitting_) {
      isRunning_ = false;
      return;
    }
  }

  HandleScriptInput();
  UpdateMarks();
  registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Host: lobby, commands, move validation
// ─────────────────────────────────────────────────────────────────────────────

Seat *PlayState::FindSeat(int clientId) {
  for (auto &s : seats_)
    if (s.clientId == clientId)
      return &s;
  return nullptr;
}

void PlayState::SendSeat(int clientId) {
  Seat *s = FindSeat(clientId);
  if (!s)
    return;
  NetMessageWriter w;
  w.WriteInt(kMsgSeat);
  w.WriteInt(s->color);
  server_.Send(clientId, w.Data(), w.Size(), true);
}

void PlayState::BroadcastLobby() {
  NetMessageWriter w;
  w.WriteInt(kMsgLobby);
  w.WriteInt((int)seats_.size());
  for (const auto &s : seats_) {
    w.WriteInt(s.color);
    w.WriteString(s.name.c_str());
  }
  server_.Broadcast(w.Data(), w.Size(), true);
}

void PlayState::BroadcastGame() {
  NetMessageWriter w;
  WriteGame(w, game_);
  server_.Broadcast(w.Data(), w.Size(), true);
}

void PlayState::Announce(const std::string &text) {
  NetMessageWriter w;
  w.WriteInt(kMsgChat);
  w.WriteString("server");
  w.WriteString(text.c_str());
  server_.Broadcast(w.Data(), w.Size(), true);
  printf("[server] %s\n", text.c_str());
  chatLog_.push_back({"server", text});
}

void PlayState::StartGame() {
  int red = 0, black = 0;
  for (const auto &s : seats_) {
    if (s.color == kColorRed)
      red++;
    if (s.color == kColorBlack)
      black++;
  }
  if (red < 1 || black < 1) {
    Announce("need RED and BLACK seated in the lobby to start");
    return;
  }
  game_ = CheckersState();
  SetupBoard(game_);
  started_ = true;
  RebuildPieces();
  Announce("game started — RED moves first");
  BroadcastGame();
}

void PlayState::ResetToLobby() {
  game_ = CheckersState();
  started_ = false;
  RebuildPieces();
  Announce("back to the lobby");
  BroadcastGame();
}

void PlayState::HandleCommand(int clientId, const std::string &cmd) {
  Seat *seat = FindSeat(clientId);
  std::string text = cmd;
  if (text.rfind("name ", 0) == 0 && text.size() > 5) {
    if (seat) {
      seat->name = text.substr(5);
      printf("== player %d is now '%s' ==\n", clientId, seat->name.c_str());
      BroadcastLobby();
    }
    return;
  }
  if (text == "start") {
    StartGame();
    return;
  }
  if (text == "reset") {
    ResetToLobby();
    return;
  }
  if (text == "ff") {
    if (!started_ && game_.winner < 0) {
      Announce("there is no game running");
      return;
    }
    int loser = seat ? seat->color : -1;
    if (loser == kColorRed || loser == kColorBlack) {
      game_.winner = 1 - loser;
      game_.chainFrom = -1;
      started_ = false;
      Announce("forfeit — " +
               std::string(loser == kColorRed ? "RED" : "BLACK") + " gave up");
      RebuildPieces();
      BroadcastGame();
    } else {
      Announce("spectators cannot forfeit");
    }
    return;
  }
  Announce("unknown command: " + text);
}

void PlayState::HandleClientMove(int clientId, const std::vector<int> &path) {
  Seat *seat = FindSeat(clientId);
  if (!seat) {
    Announce("unknown client tried to move");
    return;
  }
  if (seat->color < 0) {
    Announce(seat->name + " — spectators cannot move");
    return;
  }
  if (!started_) {
    Announce("the game has not started yet");
    return;
  }
  const char *err = TryMove(game_, seat->color, path, game_.chainFrom);
  if (err) {
    std::string from = path[0] >= 0 && path[0] < kBoardSize * kBoardSize
                           ? SquareToNotation(path[0])
                           : "?";
    Announce("illegal move (" + from + "): " + err);
    return;
  }
  int w = WinnerOf(game_);
  if (w >= 0)
    game_.winner = w;
  PlayStateSounds();
  printf("== %s moved %s -> %s%s ==\n", seat->name.c_str(),
         SquareToNotation(game_.lastFrom).c_str(),
         SquareToNotation(game_.lastTo).c_str(),
         game_.lastCaptured ? " (capture)" : "");
  if (game_.winner >= 0) {
    started_ = false;
    Announce("game over — " +
             std::string(game_.winner == kColorRed ? "RED" : "BLACK") +
             " wins (press S for a rematch)");
  }
  RebuildPieces();
  BroadcastGame();
}

// ─────────────────────────────────────────────────────────────────────────────
// Client: clicks, chat, scripted lines
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::TrySendMove(int from, int to) {
  if (from == to)
    return;
  NetMessageWriter w;
  w.WriteInt(kMsgMove);
  w.WriteInt(2);
  w.WriteInt(from);
  w.WriteInt(to);
  client_.Send(w.Data(), w.Size(), true);
  selectedSquare_ = -1;
  lastMsg_ = "sent " + SquareToNotation(from) + SquareToNotation(to);
}

void PlayState::HandleClick(int x, int y) {
  if (host_ || myColor_ < 0) {
    lastMsg_ = "you are spectating";
    return;
  }
  int col = (x - BOARD_X) / SQUARE_PX;
  int row = (y - BOARD_Y) / SQUARE_PX;
  if (col < 0 || col >= kBoardSize || row < 0 || row >= kBoardSize)
    return;
  int sq = Square(col, row);
  if (!haveGame_) {
    lastMsg_ = "waiting for the game to start";
    return;
  }
  if (game_.winner >= 0) {
    lastMsg_ = "the game is over — type /start for a rematch";
    return;
  }
  if (game_.turn != myColor_) {
    lastMsg_ = "it is not your turn";
    return;
  }
  uint8_t p = game_.board[sq];
  if (p != kPieceNone && IsRedPiece(p) == (myColor_ == kColorRed)) {
    if (game_.chainFrom >= 0 && sq != game_.chainFrom) {
      lastMsg_ =
          "continue your capture from " + SquareToNotation(game_.chainFrom);
      return;
    }
    selectedSquare_ = sq;
    lastMsg_ = SquareToNotation(sq) + " selected";
    return;
  }
  if (selectedSquare_ >= 0) {
    TrySendMove(selectedSquare_, sq);
    return;
  }
  lastMsg_ = "click one of your pieces first";
}

void PlayState::SendInputLine() {
  if (inputText_.empty())
    return;
  std::string text = inputText_;
  inputText_.clear();
  if (!text.empty() && text[0] == '/') {
    SendScriptLine(text.substr(1)); // "/start", "/ff", ...
  } else if (host_) {
    NetMessageWriter w;
    w.WriteInt(kMsgChat);
    w.WriteString("server");
    w.WriteString(text.c_str());
    server_.Broadcast(w.Data(), w.Size(), true);
    chatLog_.push_back({"server", text});
  } else {
    NetMessageWriter w;
    w.WriteInt(kMsgChat);
    w.WriteString(text.c_str());
    client_.Send(w.Data(), w.Size(), true);
  }
}

void PlayState::SendScriptLine(const std::string &text) {
  if (host_) {
    if (text == "quit") {
      isRunning_ = false;
      return;
    }
    if (text == "start") {
      StartGame();
      return;
    }
    if (text == "reset") {
      ResetToLobby();
      return;
    }
    printf("unknown command: %s\n", text.c_str());
    return;
  }
  if (text == "start" || text == "reset" || text == "ff" ||
      text.rfind("name ", 0) == 0) {
    NetMessageWriter w;
    w.WriteInt(kMsgCmd);
    w.WriteString(text.c_str());
    client_.Send(w.Data(), w.Size(), true);
    return;
  }
  std::vector<int> path;
  if (ParseMove(text, path)) {
    if (!haveGame_) {
      lastMsg_ = "the game has not started yet";
      return;
    }
    NetMessageWriter w;
    w.WriteInt(kMsgMove);
    w.WriteInt((int32_t)path.size());
    for (int sq : path)
      w.WriteInt(sq);
    client_.Send(w.Data(), w.Size(), true);
    selectedSquare_ = -1;
    return;
  }
  NetMessageWriter w;
  w.WriteInt(kMsgChat);
  w.WriteString(text.c_str());
  client_.Send(w.Data(), w.Size(), true);
}

void PlayState::HandleScriptInput() {
  if (!scriptInput_)
    return;
  char buf[256];
  ssize_t n;
  while ((n = ::read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    scriptBuf_.append(buf, n);
    size_t pos;
    while ((pos = scriptBuf_.find('\n')) != std::string::npos) {
      std::string line = scriptBuf_.substr(0, pos);
      scriptBuf_.erase(0, pos + 1);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty())
        continue;
      if (host_ && line == "quit") {
        isRunning_ = false;
        return;
      }
      SendScriptLine(line);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// render + HUD
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 20, 20, 40, 255);
  SDL_RenderClear(renderer_);

  // Board, marks, and pieces (Storm Engine RenderSystem)
  registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

  DrawStatusPanel();
  if (!host_ && myColor_ >= 0 && !haveGame_)
    DrawLobbyOverlay();
  if (game_.winner >= 0)
    DrawWinnerOverlay();

  SDL_RenderPresent(renderer_);
}

void PlayState::DrawText(const std::string &text, int x, int y, SDL_Color color,
                         int ptSize, int maxWidth) {
  TTF_Font *f = assetStore_->GetFont("hud-" + std::to_string(ptSize));
  if (!f)
    return;
  std::string use = text;
  if (maxWidth > 0 && !use.empty()) {
    int tw = 0, th = 0;
    if (TTF_SizeUTF8(f, use.c_str(), &tw, &th) == 0 && tw > maxWidth) {
      size_t lo = 0, hi = use.size();
      while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (TTF_SizeUTF8(f, TruncateUTF8(use, (int)mid).c_str(), &tw, &th) ==
                0 &&
            tw <= maxWidth)
          lo = mid;
        else
          hi = mid - 1;
      }
      use = TruncateUTF8(use, (int)lo) + "\xE2\x80\xA6"; // …
    }
  }
  Text::Draw(renderer_, f, use, x, y, color);
}

void PlayState::DrawStatusPanel() {
  SDL_Color white = {230, 230, 235, 255};
  SDL_Color dim = {160, 165, 180, 255};
  SDL_Color gold = {255, 210, 90, 255};
  SDL_Color red = {255, 110, 100, 255};
  SDL_Color cyan = {120, 220, 230, 255};

  SDL_Rect panel = {PANEL_X, 60, PANEL_W, 480};
  SDL_SetRenderDrawColor(renderer_, 30, 34, 48, 235);
  SDL_RenderFillRect(renderer_, &panel);
  SDL_SetRenderDrawColor(renderer_, 90, 96, 120, 255);
  SDL_RenderDrawRect(renderer_, &panel);

  DrawText("STORM CHECKERS", PANEL_TEXT_X, 66, gold, 20);

  int y = 96;
  if (game_.winner >= 0) {
    DrawText(std::string(game_.winner == kColorRed ? "RED" : "BLACK") +
                 " WINS!",
             PANEL_TEXT_X, y, gold, 22);
    y += 30;
  } else {
    DrawText(std::string(game_.turn == kColorRed ? "RED" : "BLACK") +
                 " to move",
             PANEL_TEXT_X, y, game_.turn == kColorRed ? red : white, 18);
    y += 24;
  }
  DrawText("move " + std::to_string(game_.moveCount + 1), PANEL_TEXT_X, y, dim,
           16);
  y += 22;
  if (game_.lastFrom >= 0) {
    DrawText("last: " + SquareToNotation(game_.lastFrom) + " -> " +
                 SquareToNotation(game_.lastTo) +
                 (game_.lastCaptured ? " (capture)" : ""),
             PANEL_TEXT_X, y, dim, 16);
    y += 22;
  }
  if (game_.chainFrom >= 0) {
    DrawText("capture from " + SquareToNotation(game_.chainFrom), PANEL_TEXT_X,
             y, cyan, 16);
    y += 22;
  }
  if (!lastMsg_.empty()) {
    DrawText(lastMsg_, PANEL_TEXT_X, y, dim, 14, PANEL_TEXT_W);
    y += 20;
  }

  y += 10;
  DrawText("LOBBY", PANEL_TEXT_X, y, gold, 16);
  y += 22;
  bool haveSeats = !seats_.empty();
  for (const auto &s : seats_) {
    std::string label = s.color == kColorRed
                            ? "RED  "
                            : s.color == kColorBlack ? "BLACK" : "spec ";
    DrawText(label + s.name, PANEL_TEXT_X, y,
             s.color == kColorRed ? red : s.color == kColorBlack ? white : dim,
             16, PANEL_TEXT_W);
    y += 20;
  }
  if (host_ && (!haveSeats || !started_))
    DrawText("press S to start (needs RED + BLACK)", PANEL_TEXT_X, y, dim, 14,
             PANEL_TEXT_W);

  // Chat log (last lines)
  int cy = 396;
  size_t start = chatLog_.size() > 6 ? chatLog_.size() - 6 : 0;
  for (size_t i = start; i < chatLog_.size(); i++) {
    DrawText(chatLog_[i].first + ": " + chatLog_[i].second, PANEL_TEXT_X, cy,
             dim, 14, PANEL_TEXT_W);
    cy += 18;
  }

  // Input box
  SDL_Rect input = {PANEL_X, 548, PANEL_W, 24};
  SDL_SetRenderDrawColor(renderer_, 18, 20, 30, 240);
  SDL_RenderFillRect(renderer_, &input);
  SDL_SetRenderDrawColor(renderer_, 90, 96, 120, 255);
  SDL_RenderDrawRect(renderer_, &input);
  DrawText("> " + inputText_ + "_", PANEL_TEXT_X + 6, 552, white, 15,
           PANEL_TEXT_W - 6);

  // Bottom hints
  DrawText(host_ ? "S: start  R: lobby  ESC: quit"
                 : "click your piece, then its destination  /ff: forfeit",
           60, 552, dim, 14);
  DrawText("ESC: quit", 60, 570, dim, 14);
}

void PlayState::DrawLobbyOverlay() {
  SDL_Color white = {230, 230, 235, 255};
  SDL_Color dim = {160, 165, 180, 255};
  SDL_Color red = {255, 110, 100, 255};
  SDL_Color gold = {255, 210, 90, 255};

  SDL_Rect box = {BOARD_X, BOARD_Y, BOARD_PX, BOARD_PX};
  SDL_SetRenderDrawColor(renderer_, 10, 12, 22, 200);
  SDL_RenderFillRect(renderer_, &box);

  int cx = BOARD_X + BOARD_PX / 2;
  auto centerText = [&](const std::string &t, int y, SDL_Color c, int pt) {
    // crude centering: 9px per char at 16pt (close enough for a HUD)
    DrawText(t, cx - (int)t.size() * pt / 4, y, c, pt);
  };
  centerText("LOBBY", BOARD_Y + 120, gold, 26);
  centerText("waiting for the host to start", BOARD_Y + 160, white, 18);
  int y = BOARD_Y + 200;
  for (const auto &s : seats_) {
    std::string label = s.color == kColorRed
                            ? "RED  "
                            : s.color == kColorBlack ? "BLACK" : "spec ";
    centerText(label + s.name, y, s.color == kColorRed ? red : white, 18);
    y += 26;
  }
  if (myColor_ >= 0) {
    y += 16;
    centerText("your seat: " + std::string(myColor_ == kColorRed
                                               ? "RED"
                                               : myColor_ == kColorBlack
                                                     ? "BLACK"
                                                     : "spectator"),
               y, dim, 16);
  }
}

void PlayState::DrawWinnerOverlay() {
  SDL_Color gold = {255, 210, 90, 255};
  SDL_Color dim = {160, 165, 180, 255};

  SDL_Rect box = {BOARD_X, BOARD_Y, BOARD_PX, BOARD_PX};
  SDL_SetRenderDrawColor(renderer_, 10, 12, 22, 180);
  SDL_RenderFillRect(renderer_, &box);

  int cx = BOARD_X + BOARD_PX / 2;
  std::string who = game_.winner == kColorRed
                        ? "RED"
                        : game_.winner == kColorBlack ? "BLACK" : "?";
  std::string msg = who + " WINS!";
  DrawText(msg, cx - (int)msg.size() * 40 / 4, BOARD_Y + 200, gold, 40);
  std::string hint =
      host_ ? "press S for a rematch" : "type /start for a rematch";
  DrawText(hint, cx - (int)hint.size() * 18 / 4, BOARD_Y + 260, dim, 18);
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio (SDL_mixer; the game runs fine with audio unavailable)
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::InitAudio() {
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    printf("(audio unavailable)\n");
    audioDisabled_ = true;
    return;
  }
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512) < 0) {
    printf("(audio unavailable: %s)\n", Mix_GetError());
    audioDisabled_ = true;
    return;
  }
  assetStore_->AddSound("move", "assets/sfx/move.wav");
  assetStore_->AddSound("capture", "assets/sfx/capture.wav");
  assetStore_->AddSound("win", "assets/sfx/win.wav");
  if (!assetStore_->GetSound("move") || !assetStore_->GetSound("capture") ||
      !assetStore_->GetSound("win")) {
    printf("(sound files missing — audio disabled)\n");
    audioDisabled_ = true;
  }
}

void PlayState::CloseAudio() {
  // The chunks live in the AssetStore now and are freed by ClearAssets, which
  // onExit() runs before this - Mix_CloseAudio frees every open chunk itself,
  // so clearing afterwards would double-free.
  Mix_CloseAudio();
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void PlayState::PlayStateSounds() {
  if (audioDisabled_)
    return;
  if (game_.winner >= 0) {
    if (Mix_Chunk *c = assetStore_->GetSound("win"))
      Mix_PlayChannel(-1, c, 0);
  } else if (game_.lastCaptured > 0) {
    if (Mix_Chunk *c = assetStore_->GetSound("capture"))
      Mix_PlayChannel(-1, c, 0);
  } else if (game_.lastFrom >= 0) {
    if (Mix_Chunk *c = assetStore_->GetSound("move"))
      Mix_PlayChannel(-1, c, 0);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// stdout transcript (kept for headless / test runs)
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::PrintBoard() {
  printf("\n  a b c d e f g h\n");
  for (int r = 0; r < kBoardSize; r++) {
    printf("%d ", kBoardSize - r);
    for (int c = 0; c < kBoardSize; c++) {
      uint8_t p = game_.board[Square(c, r)];
      char ch = IsDark(c, r) ? '.' : ':';
      if (p == kPieceRed)
        ch = 'r';
      else if (p == kPieceBlack)
        ch = 'b';
      else if (p == kPieceRedKing)
        ch = 'R';
      else if (p == kPieceBlackKing)
        ch = 'B';
      printf("%c ", ch);
    }
    printf("%d\n", kBoardSize - r);
  }
  printf("  a b c d e f g h\n");
  if (game_.winner >= 0) {
    printf("winner: %s after %d moves\n",
           game_.winner == kColorRed ? "RED" : "BLACK", game_.moveCount);
  } else {
    printf("turn: %s · move %d\n", game_.turn == kColorRed ? "RED" : "BLACK",
           game_.moveCount + 1);
  }
  if (game_.lastFrom >= 0) {
    printf(
        "last move: %s -> %s%s%s\n", SquareToNotation(game_.lastFrom).c_str(),
        SquareToNotation(game_.lastTo).c_str(),
        game_.lastCaptured ? " (capture" : "", game_.lastCaptured ? ")" : "");
  }
}
