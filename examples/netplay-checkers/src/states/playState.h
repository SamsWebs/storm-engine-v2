#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#include <stormengine2/text.h>
#include <cstdint>
#include <string>
#include <vector>

#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/net/net.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/render.h>

// ─── Netplay ───────────────────────────────────────────────────────────────
constexpr uint16_t kDefaultPort = 51235;

// ─── Board layout (pixels) ─────────────────────────────────────────────────
constexpr int BOARD_X = 60;   // left edge of the board sprite
constexpr int BOARD_Y = 60;   // top edge
constexpr int BOARD_PX = 480; // board sprite size
constexpr int SQUARE_PX = BOARD_PX / 8;
constexpr int PIECE_PX = 52;
constexpr int PIECE_MARGIN = (SQUARE_PX - PIECE_PX) / 2;

// ─── HUD panel (pixels) ─────────────────────────────────────────────────────
constexpr int PANEL_X = 560; // left edge of the status/chat panel
constexpr int PANEL_W = 520; // panel width (fills the space beside the board)
constexpr int PANEL_TEXT_X = PANEL_X + 8;
constexpr int PANEL_TEXT_W = PANEL_W - 16;

// ─── Board model ───────────────────────────────────────────────────────────
constexpr int kBoardSize = 8;
constexpr int kColorRed = 0;
constexpr int kColorBlack = 1;
constexpr int kPieceNone = 0;
constexpr int kPieceRed = 1;
constexpr int kPieceBlack = 2;
constexpr int kPieceRedKing = 3;
constexpr int kPieceBlackKing = 4;

// Message ids (NetMessageWriter/Reader, varint ints)
constexpr int kMsgChat = 1;  // [type][name][text]             host -> all
constexpr int kMsgLobby = 2; // [type][num][color,name]...     host -> all
constexpr int kMsgGame = 3;  // [type][state fields][board]    host -> all
constexpr int kMsgMove = 4;  // [type][pathLen][square...]     client -> host
constexpr int kMsgCmd = 5;   // [type][string]                 client -> host
constexpr int kMsgSeat = 6;  // [type][color]                  host -> client

struct CheckersState {
  uint8_t board[kBoardSize * kBoardSize] = {};
  int turn = kColorRed;
  int winner = -1; // -1 none, else kColorRed / kColorBlack
  int moveCount = 0;
  int lastFrom = -1; // square index 0..63, -1 before the first move
  int lastTo = -1;
  int lastCaptured = 0; // pieces removed by the last move
  bool justPromoted = false;
  int chainFrom = -1; // square a capture chain must continue from
};

struct Seat {
  int clientId = -1;
  std::string name;
  int color = -1; // kColorRed / kColorBlack, -1 = spectator
};

class PlayState : public GameState {
public:
  PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
            bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning,
            bool host, const std::string &joinAddr, uint16_t port);
  ~PlayState();

  void processInput() override;
  void update() override;
  void render() override;
  bool onEnter() override;
  bool onExit() override;
  std::string getStateID() const override { return s_playID; }

private:
  static const std::string s_playID;

  // ── Host authority ────────────────────────────────────────────────────
  void BroadcastLobby();
  void BroadcastGame();
  void Announce(const std::string &text);
  void StartGame();
  void ResetToLobby();
  void HandleCommand(int clientId, const std::string &cmd);
  void HandleClientMove(int clientId, const std::vector<int> &path);
  Seat *FindSeat(int clientId);
  void SendSeat(int clientId);

  // ── Client ────────────────────────────────────────────────────────────
  void TrySendMove(int from, int to);
  void HandleClick(int x, int y);
  void SendInputLine();

  // ── Scene (Storm Engine ECS) ──────────────────────────────────────────
  void SpawnBoardEntities();
  void RebuildPieces();
  void UpdateMarks();

  // ── HUD ───────────────────────────────────────────────────────────────
  void DrawText(const std::string &text, int x, int y, SDL_Color color,
                int ptSize = 18, int maxWidth = 0);
  void DrawStatusPanel();
  void DrawLobbyOverlay();
  void DrawWinnerOverlay();

  // ── Audio ─────────────────────────────────────────────────────────────
  void InitAudio();
  void CloseAudio();
  void PlayStateSounds();

  // ── Scripted input (headless tests: stdin when it is not a tty) ───────
  void HandleScriptInput();
  void SendScriptLine(const std::string &text);

  void PrintBoard(); // stdout transcript

  SDL_Renderer *renderer_;
  int windowWidth_, windowHeight_;
  bool isDebugging_;
  AssetStore_Ptr assetStore_;
  Logger logger_;
  bool &isRunning_;

  Registry registry_;

  // ── Netplay ───────────────────────────────────────────────────────────
  bool host_;
  NetServer server_;
  NetClient client_;
  std::string joinAddr_;
  uint16_t port_;
  std::vector<Seat> seats_;
  CheckersState game_;
  bool started_ = false;  // host: game in progress
  bool haveGame_ = false; // client: first game state received
  int myColor_ = -1;      // client: seat color
  bool quitting_ = false; // client: disconnected

  // ── Entities ──────────────────────────────────────────────────────────
  std::vector<Entity> pieces_;
  Entity *selMark_ = nullptr;  // selection ring
  Entity *fromMark_ = nullptr; // last move origin
  Entity *toMark_ = nullptr;   // last move landing
  int selectedSquare_ = -1;

  // ── UI ────────────────────────────────────────────────────────────────
  std::string inputText_;
  std::vector<std::pair<std::string, std::string>> chatLog_;
  std::string lastMsg_;
  CheckersState prev_ = {};
  bool hasPrev_ = false;

  // ── Audio ─────────────────────────────────────────────────────────────
  bool audioDisabled_ = false;

  // ── Scripted input ────────────────────────────────────────────────────
  bool scriptInput_ = false;
  std::string scriptBuf_;
};
