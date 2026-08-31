#include <glm/glm.hpp>
#include <igloo/igloo_alt.h>

#include "../../common/components/rigidBody.h"

using namespace igloo;
using namespace storm;

Describe(RigidBodyComponentSpec){
    It(should_initialize_with_default_values){// Arrange & Act
                                              RigidBodyComponent rigidBody;

// Assert
Assert::That(rigidBody.velocity, Equals(glm::vec2(0.0, 0.0)));
}

It(should_initialize_with_custom_values) {
  // Arrange & Act
  glm::vec2 velocity(2.5, -1.0);
  RigidBodyComponent rigidBody(velocity);

  // Assert
  Assert::That(rigidBody.velocity, Equals(velocity));
}
}
;
