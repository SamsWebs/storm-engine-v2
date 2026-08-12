#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/ecs.h>
#include <stormengine2/gameStateMachine.h>
#include <stormengine2/states/gameState.h>

#include "../gamepad.h"
#include "../world.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

// The campaign map. Pushes BattleState on top of itself when two armies meet
// and stays alive underneath it, which is the whole reason this example exists.
class OverworldState : public GameState {
public:
    OverworldState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                   bool isDebugging, AssetStore *assetStore,
                   GameStateMachine *machine, Gamepad *gamepad,
                   world::Campaign *campaign, bool &isRunning);

    void processInput() override;
    void update() override;
    void render() override;
    bool onEnter() override;
    bool onExit() override;

    // Fired by popState when the battle above finishes. onEnter() does NOT run
    // again, so everything that has to happen after a fight happens here.
    void resume() override;

    std::string getStateID() const override { return s_overworldID; }

private:
    static const std::string s_overworldID;

    static constexpr int TILE      = 64;
    static constexpr int MAP_W     = 16;
    static constexpr int MAP_H     = 12;
    static constexpr int MARCH_DAYS = 3;
    static constexpr int DECO_COUNT = 16;   // trees, bushes and rocks
    static constexpr int MAX_TREES  = 3;    // trees dominate; keep them rare
    static constexpr Uint32 DAY_MS = 1400;   // one day per 1.4s

    // Most real time a single frame may contribute to the day timer. Without a
    // ceiling, any stall in the event loop is spent all at once and every
    // marching army arrives simultaneously.
    static constexpr Uint32 MAX_DELTA_MS = 100;

    void SpawnTerrain();
    void SpawnDecorations();
    void SpawnCastles();
    void RefreshCastleSprite(int castle);
    void SyncArmyMarkers();
    void AdvanceDay();
    void RunEnemyAI();
    void CheckArrivals();
    void StartMarch(int general, int destination);
    void CycleSelection(int delta);
    void CycleDestination(int delta);
    void RebuildDestinations();
    bool CampaignOver() const;
    bool MaybeEndCampaign();
    void RenderHud();

    glm::vec2 CastlePixel(int castle) const;
    glm::vec2 GeneralPixel(int general) const;

    SDL_Renderer     *renderer_;
    int               windowWidth_;
    int               windowHeight_;
    bool              isDebugging_;
    AssetStore       *assetStore_;
    GameStateMachine *machine_;
    Gamepad          *gamepad_;
    world::Campaign  *campaign_;
    bool             &isRunning_;
    Logger            logger_;

    Registry registry_;

    // Held in optionals rather than bare Entity members: Entity's default
    // constructor leaves its registry pointer null, so a default-constructed
    // member is unusable until assigned.
    std::array<std::optional<Entity>, world::kCastleCount>  castleEntities_;
    std::array<std::optional<Entity>, world::kGeneralCount> armyEntities_;

    int selected_ = -1;              // index into campaign_->generals, or -1
    int destSlot_ = 0;               // index into destinations_
    std::vector<int> destinations_;  // castle indices reachable from selected_

    Uint32 dayAccumMs_ = 0;
    Uint32 lastTicks_  = 0;

    // True from the moment a battle is pushed until resume() consumes it.
    // Without it a resume() triggered by anything else would be read as a
    // battle result.
    bool awaitingBattle_ = false;
    bool leaving_        = false;
};
