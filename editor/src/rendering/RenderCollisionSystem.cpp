#include "RenderCollisionSystem.h"

RenderCollisionSystem::RenderCollisionSystem() {
  RequireComponent<BoxColliderComponent>();
  RequireComponent<TransformComponent>();
}

void RenderCollisionSystem::Update(
    std::unique_ptr<SDL_Renderer, Util::SDLDestroyer> &renderer,
    SDL_Rect &camera, const float &zoom) {
  for (const auto &entity : GetSystemEntities()) {
    const auto &transform = entity.GetComponent<TransformComponent>();
    const auto &collider = entity.GetComponent<BoxColliderComponent>();

    const SDL_Rect srcRect = {
        std::floor((transform.position.x + collider.offset.x) * zoom) -
            camera.x,
        std::floor((transform.position.y + collider.offset.y) * zoom) -
            camera.y,
        std::ceil(collider.width * transform.scale.x * zoom),
        std::ceil(collider.height * transform.scale.y * zoom)};

    SDL_SetRenderDrawColor(renderer.get(), 255, 0, 0, 125);
    SDL_RenderFillRect(renderer.get(), &srcRect);
    SDL_RenderDrawRect(renderer.get(), &srcRect);
  }
}
