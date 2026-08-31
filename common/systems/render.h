#pragma once

#include "../assetStore.h"
#include "../components/sprite.h"
#include "../components/transform.h"
#include "../ecs.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <string>

namespace storm {

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

      SDL_Texture *texture = assetStore.GetTexture(sprite.assetId);

      // A srcRect outside the texture makes SDL_RenderCopyEx draw nothing and
      // report nothing. The two ways to arrive here are a SpriteComponent
      // width/height that does not match the sheet cell, and an
      // AnimationComponent vertical flag that does not match the sheet layout
      // — the frame offset then walks off the wrong axis. Gate on the budget
      // before the query so an exhausted diagnostic costs one comparison.
      static thread_local unsigned int srcRectReports = 0;
      if (texture != nullptr && srcRectReports < ECS_MAX_DIAGNOSTIC_REPORTS) {
        int textureW = 0;
        int textureH = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);
        const bool outside =
            srcRect.x < 0 || srcRect.y < 0 || srcRect.w <= 0 ||
            srcRect.h <= 0 || srcRect.x + srcRect.w > textureW ||
            srcRect.y + srcRect.h > textureH;
        if (outside && EcsShouldReport(srcRectReports)) {
          EcsReportErr(
              "RenderSystem: srcRect {" + std::to_string(srcRect.x) + "," +
              std::to_string(srcRect.y) + "," + std::to_string(srcRect.w) +
              "," + std::to_string(srcRect.h) + "} is outside texture '" +
              sprite.assetId + "' (" + std::to_string(textureW) + "x" +
              std::to_string(textureH) +
              ") — nothing will draw. Check that SpriteComponent width/height "
              "match the sheet cell, and that AnimationComponent.vertical "
              "matches the sheet layout." +
              EcsSuppressionNote(srcRectReports));
        }
      }

      SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect,
                       transform.rotation, NULL, sprite.flip);
    }
  }
};
} // namespace storm
