#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/tilemapLoader.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>

#include "../components/playerComponent.h"
#include "../components/npcComponent.h"

// ─── Sprite sheet layout (640×64, 20 frames of 32×64 each) ───────────────────
// Frame 0:   idle up        Frame 1:  idle down
// Frame 2:   idle left      Frame 3:  idle right
// Frames 4–7:  walk up      Frames 8–11:  walk down
// Frames 12–15: walk left   Frames 16–19: walk right
constexpr int PLAYER_FRAME_W = 32;
constexpr int PLAYER_FRAME_H = 64;

// ─── Tile constants ────────────────────────────────────────────────────────────
// The editor uses a 16px grid; passing tileSize=8 to TileMapLoader preserves
// the exact pixel coordinates from the map file (GCD of common worldX values).
constexpr int LOADER_TILE_SIZE = 8;
constexpr int LOADER_CELL_PX   = 8;   // relativePosition * LOADER_CELL_PX = world px
constexpr int TILE_SRC_W       = 32;  // source tile width in the tileset PNG
constexpr int TILE_SRC_H       = 32;  // source tile height in the tileset PNG

// ─── Level bounds (from editor canvas 1248×448) ───────────────────────────────
constexpr float LEVEL_W = 1248.0f;
constexpr float LEVEL_H = 448.0f;

// ─── NPC interact prompt ──────────────────────────────────────────────────────
constexpr float INTERACT_DIST = 56.0f;

// ─────────────────────────────────────────────────────────────────────────────
struct DialogueState {
    bool        active       = false;
    std::string speakerName;
    std::string fullText;
    int         visibleChars = 0;
    float       typeTimer    = 0.0f;
    float       typeInterval = 0.04f;  // seconds per character
    bool        complete     = false;
};

// ─────────────────────────────────────────────────────────────────────────────
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
    void LoadAssets();
    void SpawnTiles();
    void LoadColliders();
    void SpawnPlayer();
    void SpawnNPC(float x, float y, const std::string &name,
                  const std::string &dialogue, Direction facing);

    void UpdatePlayer(float dt);
    void UpdateAnimation(float dt);
    void CheckNpcInteraction();
    void UpdateDialogue(float dt);

    void RenderWorld();
    void RenderDialogueBox();
    void DrawText(const std::string &text, int x, int y, SDL_Color color, int ptSize = 18);

    // Returns the srcX for a given direction + idle/walk state
    int PlayerSrcX(const PlayerComponent &pc) const;

    bool CollidesWithLevel(const SDL_Rect &r) const;

    static const std::string s_playID;

    SDL_Renderer  *renderer_;
    int            windowWidth_, windowHeight_;
    bool           isDebugging_;
    AssetStore_Ptr assetStore_;
    Logger         logger_;
    bool          &isRunning_;

    Registry       registry_;
    glm::vec2      camera_ = {0.0f, 0.0f};

    std::vector<SDL_Rect> colliderRects_;

    // Input
    bool keyUp_ = false, keyDown_ = false, keyLeft_ = false, keyRight_ = false;
    bool keyInteract_ = false;

    DialogueState dialogue_;

    TTF_Font *font_ = nullptr;

    int millisecondsPreviousFrame_ = 0;
};
