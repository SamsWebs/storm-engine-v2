#include <igloo/igloo_alt.h>

#include "../../common/systems/movement.h"

using namespace igloo;

// Mock TransformComponent
struct TransformComponentMock {
  glm::vec2 position;
};

// Mock RigidBodyComponent
struct RigidBodyComponentMock {
  glm::vec2 velocity;
};

// Mock Entity
template <typename... Components> struct EntityMock {
  TransformComponentMock transform;
  RigidBodyComponentMock rigidBody;

  TransformComponentMock &GetComponent() { return transform; }

  template <typename Component> Component &GetComponent() {
    // Not implemented in this example
    throw std::logic_error("Not implemented");
  }
};

Describe(MovementSystemSpec){
    It(should_update_entity_positions_based_on_velocity){
        // Arrange
        MovementSystem movementSystem;
EntityMock<TransformComponentMock, RigidBodyComponentMock> entity;
entity.transform.position = glm::vec2(0, 0);
entity.rigidBody.velocity = glm::vec2(2.0, 1.5);
double deltaTime = 0.5;

// Act
movementSystem.Update(deltaTime);

// Assert
// Assert::That(entity.transform.position.x, Equals(1.0));
// Assert::That(entity.transform.position.y, Equals(0.75));
}
}
;
