#include "../../common/systems/renderCollider.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(RenderColliderSystemSpec){
    It(should_render_colliders_on_entities){// Arrange
                                            Registry registry;
RenderColliderSystem renderColliderSystem;
SDL_Renderer *renderer = nullptr; // Replace with a valid SDL_Renderer

Entity entityA = registry.CreateEntity();
Entity entityB = registry.CreateEntity();

// Add TransformComponent and BoxColliderComponent to entityA
entityA.AddComponent<TransformComponent>();
entityA.AddComponent<BoxColliderComponent>();

// Add TransformComponent and BoxColliderComponent to entityB
entityB.AddComponent<TransformComponent>();
entityB.AddComponent<BoxColliderComponent>();

// Set up the positions, scales, widths, heights, and offsets for entityA and
// entityB
entityA.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
entityA.GetComponent<TransformComponent>().scale = glm::vec2(1, 1);
entityA.GetComponent<BoxColliderComponent>().width = 10;
entityA.GetComponent<BoxColliderComponent>().height = 10;
entityA.GetComponent<BoxColliderComponent>().offset = glm::vec2(0, 0);

entityB.GetComponent<TransformComponent>().position = glm::vec2(20, 20);
entityB.GetComponent<TransformComponent>().scale = glm::vec2(1, 1);
entityB.GetComponent<BoxColliderComponent>().width = 20;
entityB.GetComponent<BoxColliderComponent>().height = 20;
entityB.GetComponent<BoxColliderComponent>().offset = glm::vec2(0, 0);

// Act
renderColliderSystem.Update(renderer);

// Assert
// Add your assertions here based on the expected behavior of the render
// collider system For example, you can check if the SDL_Renderer functions were
// called correctly to render the colliders
}

It(should_not_render_colliders_when_no_entities_exist) {
  // Arrange
  Registry registry;
  RenderColliderSystem renderColliderSystem;
  SDL_Renderer *renderer = nullptr; // Replace with a valid SDL_Renderer

  // Act
  renderColliderSystem.Update(renderer);

  // Assert
  // Add your assertions here based on the expected behavior of the render
  // collider system For example, you can check if the SDL_Renderer functions
  // were not called
}
}
;
