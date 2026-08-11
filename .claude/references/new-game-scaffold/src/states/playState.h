#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/states/gameState.h>

#include <optional>
#include <string>

// gameState.h already pulls in SDL2, ecs.h, assetStore.h, logger.h and every
// built-in component and system. Do not re-include them here.
//
// (That transitive include is a documented defect -- KNOWN_ISSUES #8, ~713
// headers to declare a 23-line interface -- and goes away in v3. Include what
// you use in headers of your own.)

class PlayState : public GameState {
public:
    PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
              bool isDebugging, AssetStore_Ptr assetStore, bool &isRunning);
    ~PlayState();

    void processInput() override;
    void update() override;
    void render() override;
    bool onEnter() override;
    bool onExit() override;

    std::string getStateID() const override { return s_playID; }

private:
    void LoadAssets();
    void SpawnPlayer();

    static const std::string s_playID;

    SDL_Renderer  *renderer_;
    int            windowWidth_;
    int            windowHeight_;
    bool           isDebugging_;
    AssetStore_Ptr assetStore_;
    Logger         logger_;

    // A state stops the loop by writing to the bool& the Game handed it.
    // There is no engine quit API.
    bool &isRunning_;

    Registry registry_;

    // Held in an optional rather than a bare Entity member: Entity's default
    // constructor does not initialise its registry pointer, so a
    // default-constructed member is unusable until assigned and any method call
    // on it is undefined behaviour. optional gives it a real empty state.
    //
    // This is safe here only because nothing in the scaffold kills the player.
    // Entity ids are recycled and Entity carries no generation, so once your
    // game can destroy the player, stop caching the handle and look it up by
    // tag each frame instead -- guarded with registry_.DoesTagExist("player"),
    // which needs v1.2.2 or newer.
    std::optional<Entity> player_;

    bool moveLeft_  = false;
    bool moveRight_ = false;

    int millisecondsPreviousFrame_ = 0;
};
