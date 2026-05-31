#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/assetStore.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>
#include <stormengine2/states/gameState.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/collision.h>
#include <stormengine2/systems/movement.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/systems/renderCollider.h>
#include <stormengine2/tilemapLoader.h>
#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/rigidBody.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <glm/glm.hpp>

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
    void SpawnEntities();

    static const std::string s_playID;

    SDL_Renderer  *renderer_;
    int            windowWidth_;
    int            windowHeight_;
    bool           isDebugging_;
    AssetStore_Ptr assetStore_;
    Logger         logger_;
    bool          &isRunning_;

    Registry registry_;
};
