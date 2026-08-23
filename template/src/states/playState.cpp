#include "playState.h"

PlayState::PlayState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     AssetStore_Ptr assetStore, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, assetStore_{std::move(assetStore)},
      isRunning_{isRunning} {}

bool PlayState::onEnter() {
  // A TTF_Font is rasterised at one point size, so register one id per size.
  assetStore_->AddFont("hud-24", "./assets/font.ttf", 24);

  // Register every system BEFORE creating entities. Membership is computed
  // once, when Registry::Update() admits an entity, so a system added later
  // starts empty and stays empty.
  registry_.AddSystem<MovementSystem>();
  registry_.AddSystem<RenderSystem>();
  registry_.AddSystem<ContactSystem>();

  Entity player = registry_.CreateEntity();
  player.Tag("player");
  // Add every component the entity will ever need before this Update().
  player.AddComponent<TransformComponent>(
      glm::vec2(windowWidth_ / 2.0f, windowHeight_ / 2.0f), glm::vec2(1, 1),
      0.0);
  player.AddComponent<RigidBodyComponent>(glm::vec2(0, 0));
  player.AddComponent<BoxColliderComponent>(32, 32);

  registry_.Update();
  millisecondsPreviousFrame_ = SDL_GetTicks();
  return true;
}

bool PlayState::onExit() {
  // Before TTF_Quit(): that call frees every open font itself, so a store torn
  // down afterwards hands already-freed pointers to TTF_CloseFont.
  assetStore_->ClearAssets();
  return true;
}

void PlayState::processInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      isRunning_ = false;
    }
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
      isRunning_ = false;
    }
  }
}

void PlayState::update() {
  // Variable timestep with a 60 FPS cap.
  const int wait =
      MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame_);
  if (wait > 0 && wait <= MILLISECS_PER_FRAME) {
    SDL_Delay(wait);
  }
  const double deltaTime =
      (SDL_GetTicks() - millisecondsPreviousFrame_) / 1000.0;
  millisecondsPreviousFrame_ = SDL_GetTicks();

  // Flush pending entity adds and kills FIRST, before any system runs.
  registry_.Update();

  registry_.GetSystem<MovementSystem>().Update(deltaTime);

  // ContactSystem reports overlaps; it never kills or moves anything. Read
  // GetContacts() and decide what a contact means.
  registry_.GetSystem<ContactSystem>().Update();
}

void PlayState::render() {
  SDL_SetRenderDrawColor(renderer_, 24, 26, 34, 255);
  SDL_RenderClear(renderer_);

  registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

  // The player has no SpriteComponent yet, so RenderSystem has nothing to draw
  // for it. Draw its collider box directly so the template shows something the
  // moment it runs. Give the entity a SpriteComponent and a texture from the
  // AssetStore when you have art.
  const auto &transform =
      registry_.GetEntityByTag("player").GetComponent<TransformComponent>();
  const SDL_Rect box{static_cast<int>(transform.position.x),
                     static_cast<int>(transform.position.y), 32, 32};
  SDL_SetRenderDrawColor(renderer_, 120, 200, 255, 255);
  SDL_RenderFillRect(renderer_, &box);

  Text::DrawCentred(renderer_, assetStore_->GetFont("hud-24"),
                    "Hello from Storm! Engine v2", windowWidth_ / 2, 24,
                    SDL_Color{255, 255, 255, 255});
  Text::DrawCentred(renderer_, assetStore_->GetFont("hud-24"),
                    "ESC to quit", windowWidth_ / 2, windowHeight_ - 48,
                    SDL_Color{150, 150, 160, 255});

  SDL_RenderPresent(renderer_);
}
