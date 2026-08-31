#include "EditorRenderSystem.h"

// Safe here and not in the header: a using-directive in a .cpp is scoped to
// this translation unit.
using namespace storm;

EditorRenderSystem::EditorRenderSystem() {
  RequireComponent<TransformComponent>();
  RequireComponent<SpriteComponent>();
}

void EditorRenderSystem::Update(SDL_Renderer *renderer,
                          std::unique_ptr<class AssetManager> &assetManager,
                          SDL_Rect &camera, const float &zoom) {
  // Create a struct for sorting entities
  struct RenderableEntity {
    TransformComponent transformComponent;
    SpriteComponent spriteComponent;
  };

  // Create a vector container for renderable entities
  std::vector<RenderableEntity> renderableEntities;

  for (const auto &entity : GetSystemEntities()) {
    RenderableEntity renderableEntity;
    renderableEntity.transformComponent =
        entity.GetComponent<TransformComponent>();
    renderableEntity.spriteComponent = entity.GetComponent<SpriteComponent>();

    bool isEntityOutsideCamera =
        (renderableEntity.transformComponent.position.x +
                 (renderableEntity.transformComponent.scale.x *
                  renderableEntity.spriteComponent.width) <
             camera.x ||
         renderableEntity.transformComponent.position.x > camera.x + camera.w ||
         renderableEntity.transformComponent.position.y +
                 (renderableEntity.transformComponent.scale.y *
                  renderableEntity.spriteComponent.height) <
             camera.y ||
         renderableEntity.transformComponent.position.y > camera.y + camera.h);

    // place the entity inside of the Renderable entities vector
    renderableEntities.emplace_back(renderableEntity);
  }

  // Sort the entities based on their layer (z-index)
  std::sort(renderableEntities.begin(), renderableEntities.end(),
            [](const RenderableEntity a, const RenderableEntity b) {
              return a.spriteComponent.zIndex < b.spriteComponent.zIndex;
            });

  // Draw all of the Entities
  for (const auto &entity : renderableEntities) {
    const auto &transform = entity.transformComponent;
    const auto &sprite = entity.spriteComponent;

    // Set the src Rect of our original sprite Texture
    SDL_Rect srcRect = sprite.srcRect;

    // Set the Destination rect with the x, y position to be rendered

    SDL_Rect dstRect = {(std::floor(transform.position.x * zoom) -
                         (sprite.isFixed ? 0 : camera.x)),
                        (std::floor(transform.position.y * zoom) -
                         (sprite.isFixed ? 0 : camera.y)),
                        std::ceil(sprite.width * transform.scale.x * zoom),
                        std::ceil(sprite.height * transform.scale.y * zoom)};

    SDL_RenderCopyEx(renderer, assetManager->GetTexture(sprite.assetId).get(),
                     &srcRect, &dstRect, transform.rotation, NULL, sprite.flip);
  }
}