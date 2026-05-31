#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <random>

#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/render.h>
#include <glm/glm.hpp>

#include "../components/tetrisCell.h"
#include "../systems/tetrisSystem.h"

constexpr int BOARD_W       = 10;
constexpr int BOARD_H       = 20;
constexpr int CELL          = 32;
constexpr int PANEL_W       = 220;
constexpr int DAS_DELAY_MS  = 150;
constexpr int DAS_REPEAT_MS = 50;

// Tetromino shapes: [type][rotation] → 4 cells as {row, col} in 4×4 box
static const int SHAPES[7][4][4][2] = {
    {{{1,0},{1,1},{1,2},{1,3}},{{0,2},{1,2},{2,2},{3,2}},{{2,0},{2,1},{2,2},{2,3}},{{0,1},{1,1},{2,1},{3,1}}},
    {{{0,1},{0,2},{1,1},{1,2}},{{0,1},{0,2},{1,1},{1,2}},{{0,1},{0,2},{1,1},{1,2}},{{0,1},{0,2},{1,1},{1,2}}},
    {{{0,1},{1,0},{1,1},{1,2}},{{0,1},{1,1},{2,1},{1,2}},{{1,0},{1,1},{1,2},{2,1}},{{0,1},{1,0},{1,1},{2,1}}},
    {{{0,1},{0,2},{1,0},{1,1}},{{0,1},{1,1},{1,2},{2,2}},{{1,1},{1,2},{2,0},{2,1}},{{0,0},{1,0},{1,1},{2,1}}},
    {{{0,0},{0,1},{1,1},{1,2}},{{0,2},{1,1},{1,2},{2,1}},{{1,0},{1,1},{2,1},{2,2}},{{0,1},{1,0},{1,1},{2,0}}},
    {{{0,0},{1,0},{1,1},{1,2}},{{0,1},{0,2},{1,1},{2,1}},{{1,0},{1,1},{1,2},{2,2}},{{0,1},{1,1},{2,0},{2,1}}},
    {{{0,2},{1,0},{1,1},{1,2}},{{0,1},{1,1},{2,1},{2,2}},{{1,0},{1,1},{1,2},{2,0}},{{0,0},{0,1},{1,1},{2,1}}},
};

static const std::string BLOCK_IDS[8] = {
    "", "block_I", "block_O", "block_T", "block_S", "block_Z", "block_J", "block_L"
};

struct Piece { int type, rot, x, y; };

class PlayState : public GameState {
public:
    PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
              bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning);
    ~PlayState();

    void processInput() override;
    void update()       override;
    void render()       override;
    bool onEnter()      override;
    bool onExit()       override;
    std::string getStateID() const override { return s_playID; }

private:
    void SpawnPiece();
    bool CanPlace(const Piece &p) const;
    void LockPiece();
    int  ClearLines();
    int  GhostY() const;
    int  DropInterval() const;

    void CreateActivePieceEntities();
    void DestroyActivePieceEntities();
    void SyncGhostEntities();

    void RenderText(const std::string &text, int x, int y, SDL_Color color, int size = 24);
    void RenderPanel();
    void RenderBoardBackground();
    void RenderNextPiece();

    static const std::string s_playID;

    SDL_Renderer *renderer_;
    int windowWidth_;
    int windowHeight_;
    bool isDebugging_;
    AssetStore_Ptr assetStore_;
    Logger logger_;

    Registry registry_;

    // Board state: 0 = empty, 1-7 = locked piece color
    int board_[BOARD_H][BOARD_W] = {};

    Piece  current_  = {};
    int    nextType_ = 0;

    // The 4 active-piece entities and 4 ghost entities
    std::vector<Entity> activeEntities_;
    std::vector<Entity> ghostEntities_;
    bool entitiesSpawned_ = false;

    int score_ = 0;
    int lines_ = 0;
    int level_ = 1;

    Uint32 lastDrop_      = 0;
    int    dasDir_        = 0;
    Uint32 dasStartTime_  = 0;
    Uint32 dasLastRepeat_ = 0;

    bool  isPaused_  = false;
    bool  gameOver_  = false;
    bool &isRunning_;

    int boardOffX_ = 0;
    int boardOffY_ = 0;

    std::mt19937 rng_{std::random_device{}()};
};
