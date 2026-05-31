#include "playState.h"
#include <algorithm>
#include <cstring>

const std::string PlayState::s_playID = "PLAY";

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth}, windowHeight_{windowHeight},
      isDebugging_{isDebugging}, assetStore_{std::move(assetStore)}, isRunning_{isRunning} {

    // Load colored block textures into the engine's AssetStore
    assetStore_->AddTexture(renderer_, "block_I", "./assets/gfx/block_I.png");
    assetStore_->AddTexture(renderer_, "block_O", "./assets/gfx/block_O.png");
    assetStore_->AddTexture(renderer_, "block_T", "./assets/gfx/block_T.png");
    assetStore_->AddTexture(renderer_, "block_S", "./assets/gfx/block_S.png");
    assetStore_->AddTexture(renderer_, "block_Z", "./assets/gfx/block_Z.png");
    assetStore_->AddTexture(renderer_, "block_J", "./assets/gfx/block_J.png");
    assetStore_->AddTexture(renderer_, "block_L", "./assets/gfx/block_L.png");
    assetStore_->AddTexture(renderer_, "block_ghost", "./assets/gfx/block_ghost.png");

    // Register engine systems
    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<TetrisSyncSystem>();

    // Center the board; panel sits to the right
    boardOffX_ = (windowWidth_ - BOARD_W * CELL - PANEL_W) / 2;
    boardOffY_ = (windowHeight_ - BOARD_H * CELL) / 2;

    registry_.GetSystem<TetrisSyncSystem>().boardOffX = boardOffX_;
    registry_.GetSystem<TetrisSyncSystem>().boardOffY = boardOffY_;

    memset(board_, 0, sizeof(board_));

    nextType_ = rng_() % 7;
    SpawnPiece();

    lastDrop_ = SDL_GetTicks();
}

PlayState::~PlayState() {
    assetStore_->ClearAssets();
    TTF_Quit();
}

bool PlayState::onEnter() {
    TTF_Init();
    m_loadingComplete = true;
    return true;
}

bool PlayState::onExit() {
    m_exiting = true;
    return true;
}

// ─── Piece logic ─────────────────────────────────────────────────────────────

bool PlayState::CanPlace(const Piece &p) const {
    for (int i = 0; i < 4; i++) {
        int r = p.y + SHAPES[p.type][p.rot][i][0];
        int c = p.x + SHAPES[p.type][p.rot][i][1];
        if (c < 0 || c >= BOARD_W || r >= BOARD_H) return false;
        if (r >= 0 && board_[r][c]) return false;
    }
    return true;
}

int PlayState::GhostY() const {
    Piece ghost = current_;
    while (CanPlace(ghost)) ghost.y++;
    return ghost.y - 1;
}

int PlayState::DropInterval() const {
    return std::max(50, 800 - (level_ - 1) * 75);
}

// ─── Entity management ───────────────────────────────────────────────────────

void PlayState::CreateActivePieceEntities() {
    const std::string &blockId = BLOCK_IDS[current_.type + 1];
    int ghostY = GhostY();
    activeEntities_.clear();
    ghostEntities_.clear();

    for (int i = 0; i < 4; i++) {
        int r = current_.y + SHAPES[current_.type][current_.rot][i][0];
        int c = current_.x + SHAPES[current_.type][current_.rot][i][1];

        Entity cell = registry_.CreateEntity();
        cell.AddComponent<TetrisCellComponent>(r, c, current_.type + 1, true);
        cell.AddComponent<TransformComponent>(
            glm::vec2(boardOffX_ + c * CELL, boardOffY_ + r * CELL),
            glm::vec2(1.0, 1.0), 0.0);
        cell.AddComponent<SpriteComponent>(blockId, CELL, CELL, 2);
        activeEntities_.push_back(cell);

        int gr = ghostY + SHAPES[current_.type][current_.rot][i][0];
        Entity ghost = registry_.CreateEntity();
        ghost.AddComponent<TetrisCellComponent>(gr, c, 0, false);
        ghost.AddComponent<TransformComponent>(
            glm::vec2(boardOffX_ + c * CELL, boardOffY_ + gr * CELL),
            glm::vec2(1.0, 1.0), 0.0);
        ghost.AddComponent<SpriteComponent>("block_ghost", CELL, CELL, 1);
        ghostEntities_.push_back(ghost);
    }
    entitiesSpawned_ = true;
}

void PlayState::DestroyActivePieceEntities() {
    if (!entitiesSpawned_) return;
    for (auto &e : activeEntities_) e.Kill();
    for (auto &e : ghostEntities_)  e.Kill();
    entitiesSpawned_ = false;
}

void PlayState::SyncGhostEntities() {
    int ghostY = GhostY();
    for (int i = 0; i < 4; i++) {
        int gr = ghostY  + SHAPES[current_.type][current_.rot][i][0];
        int gc = current_.x + SHAPES[current_.type][current_.rot][i][1];
        auto &cell = ghostEntities_[i].GetComponent<TetrisCellComponent>();
        cell.boardRow = gr;
        cell.boardCol = gc;
    }
}

void PlayState::SpawnPiece() {
    DestroyActivePieceEntities();
    current_ = { nextType_, 0, BOARD_W / 2 - 2, 0 };
    nextType_ = rng_() % 7;

    if (!CanPlace(current_)) {
        gameOver_ = true;
        return;
    }
    CreateActivePieceEntities();
}

void PlayState::LockPiece() {
    // Convert the 4 active entities into locked board cells
    const std::string &blockId = BLOCK_IDS[current_.type + 1];
    for (int i = 0; i < 4; i++) {
        int r = current_.y + SHAPES[current_.type][current_.rot][i][0];
        int c = current_.x + SHAPES[current_.type][current_.rot][i][1];
        if (r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W) {
            board_[r][c] = current_.type + 1;

            // Reuse the existing entity — just mark it as locked
            auto &cell = activeEntities_[i].GetComponent<TetrisCellComponent>();
            cell.isActive = false;
            cell.boardRow = r;
            cell.boardCol = c;
            // z-index stays at 2; ghost entities get killed below
        }
    }
    for (auto &e : ghostEntities_) e.Kill();
    entitiesSpawned_ = false; // activeEntities_ are now locked, not active

    int cleared = ClearLines();
    static const int scoreTable[5] = {0, 100, 300, 500, 800};
    score_ += scoreTable[cleared] * level_;
    lines_ += cleared;
    level_ = lines_ / 10 + 1;

    SpawnPiece();
}

int PlayState::ClearLines() {
    int cleared = 0;
    for (int r = BOARD_H - 1; r >= 0; ) {
        bool full = true;
        for (int c = 0; c < BOARD_W; c++)
            if (!board_[r][c]) { full = false; break; }

        if (full) {
            // Destroy all entities in this row
            for (auto &entity : registry_.GetSystem<RenderSystem>().GetSystemEntities()) {
                if (!entity.HasComponent<TetrisCellComponent>()) continue;
                auto &cell = entity.GetComponent<TetrisCellComponent>();
                if (!cell.isActive && cell.boardRow == r) entity.Kill();
            }
            // Shift board data down
            for (int rr = r; rr > 0; rr--)
                memcpy(board_[rr], board_[rr - 1], sizeof(board_[0]));
            memset(board_[0], 0, sizeof(board_[0]));

            // Shift locked cell entities down one row
            for (auto &entity : registry_.GetSystem<RenderSystem>().GetSystemEntities()) {
                if (!entity.HasComponent<TetrisCellComponent>()) continue;
                auto &cell = entity.GetComponent<TetrisCellComponent>();
                if (!cell.isActive && cell.boardRow < r) cell.boardRow++;
            }
            cleared++;
        } else {
            r--;
        }
    }
    return cleared;
}

// ─── Input ───────────────────────────────────────────────────────────────────

void PlayState::processInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { isRunning_ = false; return; }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) { isRunning_ = false; return; }
            if (e.key.keysym.sym == SDLK_p) { isPaused_ = !isPaused_; continue; }
            if (isPaused_ || gameOver_) continue;

            switch (e.key.keysym.sym) {
            case SDLK_UP: {
                Piece rotated = current_;
                rotated.rot = (rotated.rot + 1) % 4;
                if      (CanPlace(rotated))                  current_ = rotated;
                else { rotated.x--; if (CanPlace(rotated))  current_ = rotated;
                else { rotated.x+=2; if (CanPlace(rotated)) current_ = rotated; }}
                // Sync active entity board positions
                for (int i = 0; i < 4; i++) {
                    auto &cell = activeEntities_[i].GetComponent<TetrisCellComponent>();
                    cell.boardRow = current_.y + SHAPES[current_.type][current_.rot][i][0];
                    cell.boardCol = current_.x + SHAPES[current_.type][current_.rot][i][1];
                }
                SyncGhostEntities();
                break;
            }
            case SDLK_DOWN: {
                Piece down = current_; down.y++;
                if (CanPlace(down)) {
                    current_ = down;
                    score_++;
                    for (int i = 0; i < 4; i++) {
                        auto &cell = activeEntities_[i].GetComponent<TetrisCellComponent>();
                        cell.boardRow++;
                    }
                } else {
                    LockPiece();
                }
                lastDrop_ = SDL_GetTicks();
                break;
            }
            case SDLK_LEFT: {
                Piece left = current_; left.x--;
                if (CanPlace(left)) {
                    current_ = left;
                    for (int i = 0; i < 4; i++)
                        activeEntities_[i].GetComponent<TetrisCellComponent>().boardCol--;
                    SyncGhostEntities();
                }
                dasDir_ = -1; dasStartTime_ = dasLastRepeat_ = SDL_GetTicks();
                break;
            }
            case SDLK_RIGHT: {
                Piece right = current_; right.x++;
                if (CanPlace(right)) {
                    current_ = right;
                    for (int i = 0; i < 4; i++)
                        activeEntities_[i].GetComponent<TetrisCellComponent>().boardCol++;
                    SyncGhostEntities();
                }
                dasDir_ = 1; dasStartTime_ = dasLastRepeat_ = SDL_GetTicks();
                break;
            }
            case SDLK_SPACE: {
                int dropped = GhostY() - current_.y;
                score_ += dropped * 2;
                current_.y = GhostY();
                for (int i = 0; i < 4; i++) {
                    auto &cell = activeEntities_[i].GetComponent<TetrisCellComponent>();
                    cell.boardRow = current_.y + SHAPES[current_.type][current_.rot][i][0];
                    cell.boardCol = current_.x + SHAPES[current_.type][current_.rot][i][1];
                }
                LockPiece();
                lastDrop_ = SDL_GetTicks();
                break;
            }
            }
        }
        if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == SDLK_LEFT  && dasDir_ == -1) dasDir_ = 0;
            if (e.key.keysym.sym == SDLK_RIGHT && dasDir_ ==  1) dasDir_ = 0;
        }
    }
}

// ─── Update ──────────────────────────────────────────────────────────────────

void PlayState::update() {
    int timeToWait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);
    if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME) SDL_Delay(timeToWait);
    millisecondsPreviousFrame = SDL_GetTicks();

    if (isPaused_ || gameOver_) return;

    Uint32 now = SDL_GetTicks();

    // DAS auto-repeat
    if (dasDir_ != 0 && now - dasStartTime_ > (Uint32)DAS_DELAY_MS) {
        if (now - dasLastRepeat_ > (Uint32)DAS_REPEAT_MS) {
            Piece moved = current_; moved.x += dasDir_;
            if (CanPlace(moved)) {
                current_ = moved;
                for (int i = 0; i < 4; i++)
                    activeEntities_[i].GetComponent<TetrisCellComponent>().boardCol += dasDir_;
                SyncGhostEntities();
            }
            dasLastRepeat_ = now;
        }
    }

    // Gravity
    if (now - lastDrop_ >= (Uint32)DropInterval()) {
        Piece down = current_; down.y++;
        if (CanPlace(down)) {
            current_ = down;
            for (int i = 0; i < 4; i++)
                activeEntities_[i].GetComponent<TetrisCellComponent>().boardRow++;
        } else {
            LockPiece();
        }
        lastDrop_ = now;
    }

    // Flush entity creation/destruction, then sync screen positions
    registry_.Update();
    registry_.GetSystem<TetrisSyncSystem>().Update();
}

// ─── Render ──────────────────────────────────────────────────────────────────

void PlayState::RenderBoardBackground() {
    SDL_Rect bg = { boardOffX_, boardOffY_, BOARD_W * CELL, BOARD_H * CELL };
    SDL_SetRenderDrawColor(renderer_, 15, 15, 20, 255);
    SDL_RenderFillRect(renderer_, &bg);

    SDL_SetRenderDrawColor(renderer_, 35, 35, 45, 255);
    for (int r = 0; r <= BOARD_H; r++)
        SDL_RenderDrawLine(renderer_,
            boardOffX_,              boardOffY_ + r * CELL,
            boardOffX_ + BOARD_W * CELL, boardOffY_ + r * CELL);
    for (int c = 0; c <= BOARD_W; c++)
        SDL_RenderDrawLine(renderer_,
            boardOffX_ + c * CELL, boardOffY_,
            boardOffX_ + c * CELL, boardOffY_ + BOARD_H * CELL);

    SDL_SetRenderDrawColor(renderer_, 80, 80, 100, 255);
    SDL_RenderDrawRect(renderer_, &bg);
}

void PlayState::RenderText(const std::string &text, int x, int y, SDL_Color color, int size) {
    TTF_Font *f = TTF_OpenFont("./assets/fonts/font.ttf", size);
    if (!f) return;
    SDL_Surface *surf = TTF_RenderText_Blended(f, text.c_str(), color);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }
    TTF_CloseFont(f);
}

void PlayState::RenderNextPiece() {
    int panX = boardOffX_ + BOARD_W * CELL + 20;
    int panY = boardOffY_ + 80;
    SDL_Color white = {220, 220, 220, 255};
    RenderText("NEXT", panX, panY - 36, white, 20);

    SDL_Rect box = { panX, panY, 5 * CELL, 4 * CELL };
    SDL_SetRenderDrawColor(renderer_, 25, 25, 35, 255);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 70, 70, 90, 255);
    SDL_RenderDrawRect(renderer_, &box);

    // Draw the next piece preview using its sprite texture directly
    const std::string &id = BLOCK_IDS[nextType_ + 1];
    SDL_Texture *tex = assetStore_->GetTexture(id);
    for (int i = 0; i < 4; i++) {
        int r = SHAPES[nextType_][0][i][0];
        int c = SHAPES[nextType_][0][i][1];
        SDL_Rect dst = { panX + CELL + c * CELL, panY + CELL + r * CELL, CELL, CELL };
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    }
}

void PlayState::RenderPanel() {
    int panX = boardOffX_ + BOARD_W * CELL + 20;
    SDL_Color white  = {210, 210, 210, 255};
    SDL_Color yellow = {240, 200,   0, 255};
    SDL_Color cyan   = {  0, 210, 220, 255};
    SDL_Color dim    = {130, 130, 140, 255};

    RenderNextPiece();

    int y = boardOffY_ + 260;
    RenderText("SCORE",                    panX, y,       white,  18);
    RenderText(std::to_string(score_),     panX, y + 26,  yellow, 24);
    y += 80;
    RenderText("LINES",                    panX, y,       white,  18);
    RenderText(std::to_string(lines_),     panX, y + 26,  cyan,   24);
    y += 80;
    RenderText("LEVEL",                    panX, y,       white,  18);
    RenderText(std::to_string(level_),     panX, y + 26,  cyan,   24);
    y += 80;
    RenderText("CONTROLS",                 panX, y,       white,  16);
    RenderText("Arrows  Move / Rotate",    panX, y + 24,  dim,    14);
    RenderText("SPACE   Hard drop",        panX, y + 44,  dim,    14);
    RenderText("P       Pause",            panX, y + 64,  dim,    14);
    RenderText("ESC     Quit",             panX, y + 84,  dim,    14);

    if (isPaused_) {
        SDL_Color orange = {255, 140, 0, 255};
        RenderText("PAUSED", panX, boardOffY_ + BOARD_H * CELL - 50, orange, 26);
    }
    if (gameOver_) {
        SDL_Color red = {240, 40, 40, 255};
        RenderText("GAME OVER",
                   boardOffX_ + 10, boardOffY_ + BOARD_H * CELL / 2 - 20, red, 30);
    }
}

void PlayState::render() {
    SDL_SetRenderDrawColor(renderer_, 8, 8, 12, 255);
    SDL_RenderClear(renderer_);

    RenderBoardBackground();

    // Engine's RenderSystem draws all entities (locked cells, active piece, ghosts)
    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

    RenderPanel();

    SDL_RenderPresent(renderer_);
}
