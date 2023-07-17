#include "playState.h"

Registry GameState::registry;
AssetStore GameState::assetStore;
Logger Engine::logger;

const std::string PlayState::s_playID = "PLAY";

PlayState::PlayState(SDL_Renderer *renderer, int windoWidth, bool isDebugging)
    : renderer_(renderer), windowWidth_(windoWidth), isDebugging_(isDebugging) {
  // Add the systems that need to be processed in our game
  registry.AddSystem<MovementSystem>();
  registry.AddSystem<RenderSystem>();
  registry.AddSystem<AnimationSystem>();
  registry.AddSystem<CollisionSystem>();
  registry.AddSystem<RenderColliderSystem>();

  // Add assets to the asset store
  const std::string tankSpriteId = "tank-image";
  assetStore.AddTexture(renderer, tankSpriteId,
                        "./assets/images/tank-panther-right.png");
  assetStore.AddTexture(renderer, "truck-image",
                        "./assets/images/truck-ford-right.png");
  const auto tileMapPng = std::string{"./assets/tilemaps/jungle.png"};
  const auto tileMapSpriteId = std::string{"tile-map"};
  assetStore.AddTexture(renderer, tileMapSpriteId, tileMapPng);
  assetStore.AddTexture(renderer, "chopper-image",
                        "./assets/images/chopper.png");
  assetStore.AddTexture(renderer, "radar-image", "./assets/images/radar.png");

  constexpr int tileSize = 32;
  constexpr double tileScale = 2.5;
  TileMapLoader tilemapLoader("./assets/tilemaps/jungle.map",
                              "./assets/tilemaps/jungle.png", tileSize);

  auto map = tilemapLoader.getMap();
  for (const auto &tile : map) {
    auto tileBackground = registry.CreateEntity();
    tileBackground.AddComponent<TransformComponent>(
        glm::vec2(tileScale * tileSize * tile.relativePosition.x,
                  tileScale * tileSize * tile.relativePosition.y),
        glm::vec2(tileScale, tileScale));
    tileBackground.AddComponent<SpriteComponent>(
        tileMapSpriteId, tileSize, tileSize, 0, tile.pixelSrcPosition.x,
        tile.pixelSrcPosition.y);
  }

  Entity chopper = registry.CreateEntity();
  chopper.AddComponent<TransformComponent>(glm::vec2(10.0, 100.0),
                                           glm::vec2(1.5, 1.5), 0.0);
  chopper.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 0.0));
  chopper.AddComponent<SpriteComponent>("chopper-image", tileSize, tileSize, 3);
  chopper.AddComponent<AnimationComponent>(2, 15, true);

  Entity radar = registry.CreateEntity();
  radar.AddComponent<TransformComponent>(glm::vec2(windowWidth_ - 100, 10.0),
                                         glm::vec2(1.5, 1.5), 0.0);
  radar.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 0.0));
  radar.AddComponent<SpriteComponent>("radar-image", 64, 64, 2);
  radar.AddComponent<AnimationComponent>(8, 5, true);

  Entity tank = registry.CreateEntity();
  tank.AddComponent<TransformComponent>(glm::vec2(500.0, 10.0),
                                        glm::vec2(1.5, 1.5), 0.0);
  tank.AddComponent<RigidBodyComponent>(glm::vec2(-30.0, 0.0));
  tank.AddComponent<SpriteComponent>(tankSpriteId, tileSize, tileSize, 2);
  tank.AddComponent<BoxColliderComponent>(32, 32);

  Entity truck = registry.CreateEntity();
  truck.AddComponent<TransformComponent>(glm::vec2(10.0, 10.0),
                                         glm::vec2(1.5, 1.5), 0.0);
  truck.AddComponent<RigidBodyComponent>(glm::vec2(20.0, 0.0));
  truck.AddComponent<SpriteComponent>("truck-image", tileSize, tileSize, 1);
  truck.AddComponent<BoxColliderComponent>(32, 32);
}

PlayState::~PlayState() { assetStore.ClearAssets(); }

void PlayState::update() {
  // If we are too fast, waste some time until we reach the MILLISECS_PER_FRAME
  int timeToWait =
      MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);
  if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME) {
    SDL_Delay(timeToWait);
  }

  // The difference in ticks since the last frame, converted to seconds.
  double deltaTime = (SDL_GetTicks() - millisecondsPreviousFrame) / 1000.0;

  // Store the current frame time
  millisecondsPreviousFrame = SDL_GetTicks();

  // Update the registry to process the entities that are waiting to be
  // created/deleted
  registry.Update();

  // Ask all the systems to update
  registry.GetSystem<MovementSystem>().Update(deltaTime);
  registry.GetSystem<AnimationSystem>().Update();
  registry.GetSystem<CollisionSystem>().Update();
}

void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 21, 21, 21, 255);
  SDL_RenderClear(renderer_);

  // Invoke all the systems that need to render
  registry.GetSystem<RenderSystem>().Update(renderer_, assetStore);
  if (isDebugging_) {
    registry.GetSystem<RenderColliderSystem>().Update(renderer_);
  }
  SDL_RenderPresent(renderer_);
}

void PlayState::processInput() {
  SDL_Event sdlEvent;
  while (SDL_PollEvent(&sdlEvent)) {
    switch (sdlEvent.type) {
    case SDL_QUIT:
      break;
    case SDL_KEYDOWN:
      if (sdlEvent.key.keysym.sym == SDLK_ESCAPE) {
      } else if (sdlEvent.key.keysym.sym == SDLK_d) {
      }
      break;
    }
  }
}

bool PlayState::onEnter() {
  // here we can set default variables for the game play
  // and load assets specific for the level

  m_loadingComplete = true;

  std::cout << "entering PlayState\n";
  return true;
}
bool PlayState::onExit() {
  // here we can free any resource we put on the stack
  // and set the exiting_state flag
  m_exiting = true;

  std::cout << "exiting PlayState\n";
  return true;
}