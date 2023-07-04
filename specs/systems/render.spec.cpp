#include "../../common/systems/render.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(RenderSystemSpec){
    It(should_render_entities_in_correct_order){// Arrange
                                                Registry registry;
RenderSystem renderSystem;
SDL_Renderer *renderer = nullptr; // Replace with a valid SDL_Renderer
AssetStore assetStore;            // Create a valid instance of the AssetStore

Entity entityA = registry.CreateEntity();
Entity entityB = registry.CreateEntity();

// Add TransformComponent and SpriteComponent to entityA
entityA.AddComponent<TransformComponent>();
entityA.AddComponent<SpriteComponent>();

// Add TransformComponent and SpriteComponent to entityB
entityB.AddComponent<TransformComponent>();
entityB.AddComponent<SpriteComponent>();

// Set up the positions, scales, widths, heights, asset IDs, and z-indexes for
// entityA and entityB
entityA.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
entityA.GetComponent<TransformComponent>().scale = glm::vec2(1, 1);
entityA.GetComponent<SpriteComponent>().width = 10;
entityA.GetComponent<SpriteComponent>().height = 10;
entityA.GetComponent<SpriteComponent>().assetId = "assetA";
entityA.GetComponent<SpriteComponent>().zIndex = 1;

entityB.GetComponent<TransformComponent>().position = glm::vec2(20, 20);
entityB.GetComponent<TransformComponent>().scale = glm::vec2(1, 1);
entityB.GetComponent<SpriteComponent>().width = 20;
entityB.GetComponent<SpriteComponent>().height = 20;
entityB.GetComponent<SpriteComponent>().assetId = "assetB";
entityB.GetComponent<SpriteComponent>().zIndex = 0;

// Act
renderSystem.Update(renderer, assetStore);

// Assert
// Add your assertions here based on the expected behavior of the render system
// For example, you can check if the entities were rendered in the correct order
// based on their z-indexes
}

It(should_not_render_entities_when_no_entities_exist) {
  // Arrange
  Registry registry;
  RenderSystem renderSystem;
  SDL_Renderer *renderer = nullptr; // Replace with a valid SDL_Renderer
  AssetStore assetStore;            // Create a valid instance of the AssetStore

  // Act
  renderSystem.Update(renderer, assetStore);

  // Assert
  // Add your assertions here based on the expected behavior of the render
  // system For example, you can check if the rendering functions were not
  // called
}
}
;
