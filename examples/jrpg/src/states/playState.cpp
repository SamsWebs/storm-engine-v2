#include "playState.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

const std::string PlayState::s_playID = "PLAY";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth}, windowHeight_{windowHeight},
      isDebugging_{isDebugging}, assetStore_{std::move(assetStore)},
      isRunning_{isRunning}
{
    if (TTF_Init() != 0) {
        logger_.Err("PlayState: TTF_Init failed — " + std::string(TTF_GetError()));
        assetsLoaded_ = false;
    }
    font_ = TTF_OpenFont("./assets/fonts/font.ttf", 18);
    if (!font_) {
        logger_.Err("PlayState: failed to load font — " + std::string(TTF_GetError()));
        assetsLoaded_ = false;
    }

    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<RenderColliderSystem>();

    LoadAssets();
    SpawnTiles();
    LoadColliders();
    SpawnPlayer();

    // Shopkeeper outside the buildings
    SpawnNPC(400.0f, 200.0f, "Shopkeeper",
             "Welcome, traveler! Step inside and browse my wares.",
             Direction::Down);

    registry_.Update();
    millisecondsPreviousFrame_ = SDL_GetTicks();
}

PlayState::~PlayState() {
    onExit();
}

bool PlayState::onEnter() {
    m_loadingComplete = true;
    return true;
}

bool PlayState::onExit() {
    if (font_) { TTF_CloseFont(font_); font_ = nullptr; }
    // Matches the TTF_Init in the constructor. Without it SDL_ttf is still up
    // when SDL_Quit runs, which leaves the font cache and FreeType allocated.
    TTF_Quit();
    assetStore_->ClearAssets();
    m_exiting = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Asset loading
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::LoadAssets() {
    // Every miss here used to be silent: AddTexture logs and adds nothing,
    // GetTexture then returns nullptr, and RenderWorld skips the draw. The
    // result was an empty green window that exited 0. All three sheets are
    // required, so a miss is recorded and Game refuses to start.
    struct Asset { const char *id; const char *path; };
    static const Asset kAssets[] = {
        {"Buildings-Tileset", "./assets/gfx/Buildings-Tileset.png"},
        {"Outside-Tileset",   "./assets/gfx/Outside-Tileset.png"},
        {"NPC-Sprite-Sheet",  "./assets/gfx/NPC-Sprite-Sheet.png"},
    };
    for (const auto &a : kAssets) {
        assetStore_->AddTexture(renderer_, a.id, a.path);
        if (!assetStore_->GetTexture(a.id)) {
            logger_.Err(std::string("PlayState: missing ") + a.path);
            assetsLoaded_ = false;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tile spawning
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::SpawnTiles() {
    TileMapLoader loader("./assets/tilemaps/jrpg.map", "", LOADER_TILE_SIZE);
    for (const auto &tile : loader.getMap()) {
        float wx = tile.relativePosition.x * static_cast<float>(LOADER_CELL_PX);
        float wy = tile.relativePosition.y * static_cast<float>(LOADER_CELL_PX);

        Entity e = registry_.CreateEntity();
        e.Group("tiles");
        e.AddComponent<TransformComponent>(
            glm::vec2(wx, wy), glm::vec2(tile.scale.x, tile.scale.y), 0.0);
        e.AddComponent<SpriteComponent>(
            tile.assetId, TILE_SRC_W, TILE_SRC_H, tile.zIndex,
            false, tile.pixelSrcPosition.x, tile.pixelSrcPosition.y);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Collider loading
// Colliders map format: collider worldX worldY scaleX scaleY colW colH offX offY
// worldX/worldY are in editor pixel coordinates (same space as tile worldX/worldY)
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::LoadColliders() {
    std::ifstream f("./assets/tilemaps/jrpg_colliders.map");
    if (!f.is_open()) {
        logger_.Err("LoadColliders: cannot open jrpg_colliders.map");
        return;
    }

    std::string token;
    while (f >> token) {
        if (token == "collider") {
            float cx, cy, sx, sy, cw, ch, ox, oy;
            if (!(f >> cx >> cy >> sx >> sy >> cw >> ch >> ox >> oy))
                break;
            // Honour the size in the file. This used to read cw/ch and then
            // discard them, hardcoding every collider to one source tile, so
            // the file's own width/height columns meant nothing and a wall had
            // to be spelled out one 32px block at a time. Nobody did, which is
            // why the level had no perimeter and the player could walk off it.
            //
            // Zero still means "one tile" -- that is what all twenty of the
            // original hand-placed entries carry.
            colliderRects_.push_back({
                static_cast<int>(cx),
                static_cast<int>(cy),
                (cw > 0) ? static_cast<int>(cw) : TILE_SRC_W,
                (ch > 0) ? static_cast<int>(ch) : TILE_SRC_H
            });
        }
    }
    logger_.Log("Loaded " + std::to_string(colliderRects_.size()) + " colliders");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entity spawning
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::SpawnPlayer() {
    Entity player = registry_.CreateEntity();
    player.Tag("player");
    // On the plaza in front of the shop, not inside it. The old (100,200) put
    // the 32x64 sprite at y 200..264, squarely inside the wall band the shop
    // occupies at y 192..272 -- the player spawned embedded in the storefront.
    // Feet box here is x 104..128, y 328..344: clear of the shop (solid to
    // y=304) and of the fence run that starts at y=352.
    player.AddComponent<TransformComponent>(
        glm::vec2(100.0f, 280.0f), glm::vec2(1.0f, 1.0f), 0.0);
    // Start facing down (idle-down = frame 1, srcX = 32)
    player.AddComponent<SpriteComponent>(
        "NPC-Sprite-Sheet", PLAYER_FRAME_W, PLAYER_FRAME_H, CHARACTER_Z,
        false, 1 * PLAYER_FRAME_W, 0);
    player.AddComponent<PlayerComponent>();
}

void PlayState::SpawnNPC(float x, float y, const std::string &name,
                         const std::string &dialogue, Direction facing) {
    Entity npc = registry_.CreateEntity();
    npc.Group("npcs");

    // Standing idle frame based on facing direction
    int idleFrame = 1; // default: facing down
    switch (facing) {
        case Direction::Up:    idleFrame = 0; break;
        case Direction::Down:  idleFrame = 1; break;
        case Direction::Left:  idleFrame = 2; break;
        case Direction::Right: idleFrame = 3; break;
    }

    npc.AddComponent<TransformComponent>(
        glm::vec2(x, y), glm::vec2(1.0f, 1.0f), 0.0);
    npc.AddComponent<SpriteComponent>(
        "NPC-Sprite-Sheet", PLAYER_FRAME_W, PLAYER_FRAME_H, CHARACTER_Z,
        false, idleFrame * PLAYER_FRAME_W, 0);
    npc.AddComponent<NpcComponent>(NpcComponent{name, dialogue, facing});
    // NPC tinting happens per-draw in RenderWorld (the sheet is shared with
    // the player, so a spawn-time color mod would tint both).
}

// ─────────────────────────────────────────────────────────────────────────────
// processInput
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { isRunning_ = false; return; }
        // Ignore auto-repeat. These are all edge actions -- toggling the debug
        // overlay, advancing dialogue -- and the repeat stream made them strobe
        // for as long as the key was held.
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: isRunning_ = false; return;
            case SDLK_F1:    isDebugging_ = !isDebugging_; break;
            case SDLK_UP:    case SDLK_w: keyUp_    = true; break;
            case SDLK_DOWN:  case SDLK_s: keyDown_  = true; break;
            case SDLK_LEFT:  case SDLK_a: keyLeft_  = true; break;
            case SDLK_RIGHT: case SDLK_d: keyRight_ = true; break;
            case SDLK_SPACE: case SDLK_e: case SDLK_RETURN:
                keyInteract_ = true;
                break;
            default: break;
            }
        }
        if (event.type == SDL_KEYUP) {
            switch (event.key.keysym.sym) {
            case SDLK_UP:    case SDLK_w: keyUp_    = false; break;
            case SDLK_DOWN:  case SDLK_s: keyDown_  = false; break;
            case SDLK_LEFT:  case SDLK_a: keyLeft_  = false; break;
            case SDLK_RIGHT: case SDLK_d: keyRight_ = false; break;
            default: break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::update() {
    int timeToWait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame_);
    if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME)
        SDL_Delay(timeToWait);

    float dt = (SDL_GetTicks() - millisecondsPreviousFrame_) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    millisecondsPreviousFrame_ = SDL_GetTicks();

    if (dialogue_.active) {
        UpdateDialogue(dt);
    } else {
        UpdatePlayer(dt);
        UpdateAnimation(dt);
        CheckNpcInteraction();
    }

    // Camera: follow player, clamped to level bounds
    auto &pt = registry_.GetEntityByTag("player").GetComponent<TransformComponent>();
    float cx = pt.position.x - windowWidth_  / 2.0f + PLAYER_FRAME_W / 2.0f;
    float cy = pt.position.y - windowHeight_ / 2.0f + PLAYER_FRAME_H / 2.0f;
    camera_.x = std::max(0.0f, std::min(cx, LEVEL_W - windowWidth_));
    camera_.y = std::max(0.0f, std::min(cy, LEVEL_H - windowHeight_));

    registry_.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Player movement and collision
// ─────────────────────────────────────────────────────────────────────────────

bool PlayState::CollidesWithLevel(const SDL_Rect &r) const {
    for (const auto &c : colliderRects_) {
        if (r.x < c.x + c.w && r.x + r.w > c.x &&
            r.y < c.y + c.h && r.y + r.h > c.y)
            return true;
    }
    return false;
}

void PlayState::UpdatePlayer(float dt) {
    Entity player = registry_.GetEntityByTag("player");
    auto &transform = player.GetComponent<TransformComponent>();
    auto &pc        = player.GetComponent<PlayerComponent>();

    float vx = 0.0f, vy = 0.0f;

    if (keyUp_)    { vy = -pc.moveSpeed; pc.facing = Direction::Up;    }
    if (keyDown_)  { vy =  pc.moveSpeed; pc.facing = Direction::Down;  }
    if (keyLeft_)  { vx = -pc.moveSpeed; pc.facing = Direction::Left;  }
    if (keyRight_) { vx =  pc.moveSpeed; pc.facing = Direction::Right; }

    // Normalize diagonal movement
    if (vx != 0.0f && vy != 0.0f) {
        vx *= 0.7071f;
        vy *= 0.7071f;
    }

    pc.isMoving = (vx != 0.0f || vy != 0.0f);

    // Collision rect — use the lower half of the sprite as the "feet" box
    int fw = PLAYER_FRAME_W - 8;
    int fh = 16;
    int fx = static_cast<int>(transform.position.x) + 4;
    int fy = static_cast<int>(transform.position.y) + PLAYER_FRAME_H - fh;

    // Try X movement
    SDL_Rect testX = {static_cast<int>(fx + vx * dt), fy, fw, fh};
    if (!CollidesWithLevel(testX))
        transform.position.x += vx * dt;

    // Try Y movement from the post-X position, so a diagonal step can't clip
    // through a corner the X move just slid along.
    fx = static_cast<int>(transform.position.x) + 4;
    SDL_Rect testY = {fx, static_cast<int>(fy + vy * dt), fw, fh};
    if (!CollidesWithLevel(testY))
        transform.position.y += vy * dt;

    // Clamp to level bounds
    transform.position.x = std::max(0.0f, std::min(transform.position.x,
                                                     LEVEL_W - PLAYER_FRAME_W));
    transform.position.y = std::max(0.0f, std::min(transform.position.y,
                                                     LEVEL_H - PLAYER_FRAME_H));

    // Update sprite srcRect based on direction + idle/walk state
    auto &sprite = player.GetComponent<SpriteComponent>();
    sprite.srcRect.x = PlayerSrcX(pc);
    sprite.srcRect.y = 0;
}

int PlayState::PlayerSrcX(const PlayerComponent &pc) const {
    if (!pc.isMoving) {
        switch (pc.facing) {
        case Direction::Up:    return 0 * PLAYER_FRAME_W;
        case Direction::Down:  return 1 * PLAYER_FRAME_W;
        case Direction::Left:  return 2 * PLAYER_FRAME_W;
        case Direction::Right: return 3 * PLAYER_FRAME_W;
        }
    }
    int base;
    switch (pc.facing) {
    case Direction::Up:    base = 4;  break;
    case Direction::Down:  base = 8;  break;
    case Direction::Left:  base = 12; break;
    default:               base = 16; break; // Right
    }
    return (base + pc.walkFrame) * PLAYER_FRAME_W;
}

void PlayState::UpdateAnimation(float dt) {
    Entity player = registry_.GetEntityByTag("player");
    auto &pc = player.GetComponent<PlayerComponent>();

    if (!pc.isMoving) {
        pc.walkFrame = 0;
        pc.animTimer = 0.0f;
        return;
    }

    pc.animTimer += dt;
    if (pc.animTimer >= pc.animInterval) {
        pc.animTimer -= pc.animInterval;
        pc.walkFrame = (pc.walkFrame + 1) % 4;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// NPC interaction
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::CheckNpcInteraction() {
    if (!keyInteract_) return;
    keyInteract_ = false;

    auto &pt = registry_.GetEntityByTag("player").GetComponent<TransformComponent>();
    glm::vec2 playerCenter = {
        pt.position.x + PLAYER_FRAME_W / 2.0f,
        pt.position.y + PLAYER_FRAME_H / 2.0f
    };

    if (registry_.DoesGroupExist("npcs")) {
        for (auto &npc : registry_.GetEntitiesByGroup("npcs")) {
            auto &nt  = npc.GetComponent<TransformComponent>();
            auto &nc  = npc.GetComponent<NpcComponent>();

            glm::vec2 npcCenter = {
                nt.position.x + PLAYER_FRAME_W / 2.0f,
                nt.position.y + PLAYER_FRAME_H / 2.0f
            };

            float dx = playerCenter.x - npcCenter.x;
            float dy = playerCenter.y - npcCenter.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= nc.interactDist) {
                dialogue_.active       = true;
                dialogue_.speakerName  = nc.name;
                dialogue_.fullText     = nc.dialogue;
                dialogue_.visibleChars = 0;
                dialogue_.typeTimer    = 0.0f;
                dialogue_.complete     = false;
                return;
            }
        }
    }
}

void PlayState::UpdateDialogue(float dt) {
    if (!dialogue_.active) return;

    if (!dialogue_.complete) {
        dialogue_.typeTimer += dt;
        while (dialogue_.typeTimer >= dialogue_.typeInterval) {
            dialogue_.typeTimer -= dialogue_.typeInterval;
            if (dialogue_.visibleChars < static_cast<int>(dialogue_.fullText.size())) {
                dialogue_.visibleChars++;
            } else {
                dialogue_.complete = true;
                break;
            }
        }
    }

    // Space/Enter/E advances or dismisses
    if (keyInteract_) {
        keyInteract_ = false;
        if (!dialogue_.complete) {
            // Skip typewriter — show all text immediately
            dialogue_.visibleChars = static_cast<int>(dialogue_.fullText.size());
            dialogue_.complete     = true;
        } else {
            dialogue_.active = false;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::render() {
    SDL_SetRenderDrawColor(renderer_, 34, 85, 34, 255); // dark green (grass fallback)
    SDL_RenderClear(renderer_);

    RenderWorld();

    if (dialogue_.active)
        RenderDialogueBox();

    SDL_RenderPresent(renderer_);
}

void PlayState::RenderWorld() {
    // Collect all entities with Sprite + Transform, sort by zIndex, render with camera offset
    auto entities = registry_.GetSystem<RenderSystem>().GetSystemEntities();
    // zIndex first, then the feet line. zIndex alone is a spawn-time constant
    // -- player 5, NPC 4, tiles <= 3 -- so the player drew in front of every
    // NPC unconditionally, however far up the screen the other one stood. In a
    // top-down world the character lower on the screen is nearer the camera, so
    // position.y + height breaks the tie inside a layer.
    std::sort(entities.begin(), entities.end(), [](const Entity &a, const Entity &b) {
        const auto &sa = a.GetComponent<SpriteComponent>();
        const auto &sb = b.GetComponent<SpriteComponent>();
        if (sa.zIndex != sb.zIndex) {
            return sa.zIndex < sb.zIndex;
        }
        const float fa = a.GetComponent<TransformComponent>().position.y + sa.height;
        const float fb = b.GetComponent<TransformComponent>().position.y + sb.height;
        return fa < fb;
    });

    for (auto &e : entities) {
        const auto &t = e.GetComponent<TransformComponent>();
        const auto &s = e.GetComponent<SpriteComponent>();

        int dstX = static_cast<int>(t.position.x - camera_.x);
        int dstY = static_cast<int>(t.position.y - camera_.y);
        int dstW = static_cast<int>(s.width  * t.scale.x);
        int dstH = static_cast<int>(s.height * t.scale.y);

        // Frustum cull
        if (dstX + dstW < 0 || dstX > windowWidth_)  continue;
        if (dstY + dstH < 0 || dstY > windowHeight_) continue;

        SDL_Texture *tex = assetStore_->GetTexture(s.assetId);
        if (!tex) continue;

        // The NPC sprite sheet is tinted blue for NPCs; reset for the player
        if (s.assetId == "NPC-Sprite-Sheet") {
            if (e.HasTag("player"))
                SDL_SetTextureColorMod(tex, 255, 255, 255); // player: no tint
            else
                SDL_SetTextureColorMod(tex, 100, 180, 255); // NPC: blue tint
        }

        SDL_Rect srcRect = s.srcRect;
        SDL_Rect dstRect = {dstX, dstY, dstW, dstH};
        SDL_RenderCopyEx(renderer_, tex, &srcRect, &dstRect,
                         t.rotation, nullptr, s.flip);
    }

    // Debug: draw collision rects
    if (isDebugging_) {
        SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 180);
        for (const auto &r : colliderRects_) {
            SDL_Rect dr = {r.x - static_cast<int>(camera_.x),
                           r.y - static_cast<int>(camera_.y),
                           r.w, r.h};
            SDL_RenderDrawRect(renderer_, &dr);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dialogue box
// ─────────────────────────────────────────────────────────────────────────────

void PlayState::RenderDialogueBox() {
    int boxH   = 130;
    int boxY   = windowHeight_ - boxH - 10;
    int margin = 16;

    // Semi-transparent dark background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 210);
    SDL_Rect box = {10, boxY, windowWidth_ - 20, boxH};
    SDL_RenderFillRect(renderer_, &box);

    // White border
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);

    // Speaker name (gold)
    DrawText(dialogue_.speakerName, 10 + margin, boxY + 10,
             {255, 220, 0, 255});

    // Dialogue text (typewriter)
    std::string visible = dialogue_.fullText.substr(0, dialogue_.visibleChars);
    DrawText(visible, 10 + margin, boxY + 36, {255, 255, 255, 255});

    // Prompt when complete
    if (dialogue_.complete) {
        DrawText("[ Press E / Space ]",
                 windowWidth_ - 190, boxY + boxH - 24,
                 {200, 200, 200, 255});
    }

    // Put the blend mode back. It is renderer-wide state, not scoped to this
    // function, so leaving it on BLEND meant every later draw -- notably the
    // F1 collider overlay, which sets its own alpha -- rendered differently
    // after the first line of dialogue than before it.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

// Renders at the font's fixed point size (18) — the font is opened once in the
// constructor, so per-call sizes aren't supported.
void PlayState::DrawText(const std::string &text, int x, int y,
                         SDL_Color color) {
    if (!font_ || text.empty()) return;

    SDL_Surface *surf = TTF_RenderText_Blended(font_, text.c_str(), color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}
