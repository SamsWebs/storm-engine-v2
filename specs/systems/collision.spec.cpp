#include "../../common/systems/collision.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(CollisionSystemSpec){
    It(should_kill_entities_when_collision_detected){// Arrange
                                                     Registry registry;
CollisionSystem collisionSystem;
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

entityB.GetComponent<TransformComponent>().position = glm::vec2(5, 5);
entityB.GetComponent<TransformComponent>().scale = glm::vec2(1, 1);
entityB.GetComponent<BoxColliderComponent>().width = 10;
entityB.GetComponent<BoxColliderComponent>().height = 10;
entityB.GetComponent<BoxColliderComponent>().offset = glm::vec2(0, 0);

// Act
collisionSystem.Update();

// Assert
// TODO
}

It(should_not_kill_entities_when_no_collision_detected) {
  // Arrange
  Registry registry;
  CollisionSystem collisionSystem;
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

  entityB.GetComponent<TransformComponent>().position = glm::vec2(50, 50);
  entityB.GetComponent<TransformComponent>().scale = glm::vec2(1, 1);
  entityB.GetComponent<BoxColliderComponent>().width = 10;
  entityB.GetComponent<BoxColliderComponent>().height = 10;
  entityB.GetComponent<BoxColliderComponent>().offset = glm::vec2(0, 0);

  // Act
  collisionSystem.Update();

  // Assert
  // TODO
}
}
;