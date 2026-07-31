#pragma once

#include "../assetStore.h"
#include "../components/sprite.h"
#include "../components/transform.h"
#include "../ecs.h"
#include <SDL2/SDL.h>
#include <algorithm>

class RenderSystem : public System {
public:
  RenderSystem() {
    RequireComponent<TransformComponent>();
    RequireComponent<SpriteComponent>();
  }

  void Update(SDL_Renderer *renderer, const AssetStore &assetStore,
              const SDL_Rect *camera = nullptr) {

    auto lambda = [](const Entity &entA, const Entity &entB) {
      const auto &spriteA = entA.GetComponent<SpriteComponent>();
      const auto &spriteB = entB.GetComponent<SpriteComponent>();
      return spriteA.zIndex < spriteB.zIndex;
    };
    sortEntities(lambda);

    // Loop all entities that system is interested in
    for (auto &entity : GetSystemEntities()) {
      const auto &transform = entity.GetComponent<TransformComponent>();
      const auto &sprite = entity.GetComponent<SpriteComponent>();

      // Set the source rectangle of our original sprite texture
      SDL_Rect srcRect = sprite.srcRect;

      // Fixed sprites are in screen space and ignore the camera pan.
      int camX = (camera && !sprite.isFixed) ? camera->x : 0;
      int camY = (camera && !sprite.isFixed) ? camera->y : 0;

      // Set the destination rectangle with the x, y position to be rendered
      SDL_Rect dstRect = {static_cast<int>(transform.position.x) +
                              static_cast<int>(sprite.offset.x) - camX,
                          static_cast<int>(transform.position.y) +
                              static_cast<int>(sprite.offset.y) - camY,
                          static_cast<int>(sprite.width * transform.scale.x),
                          static_cast<int>(sprite.height * transform.scale.y)};

      SDL_RenderCopyEx(renderer, assetStore.GetTexture(sprite.assetId),
                       &srcRect, &dstRect, transform.rotation, NULL,
                       sprite.flip);
    }
  }
};