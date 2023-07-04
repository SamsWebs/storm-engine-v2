#include <glm/glm.hpp>
#include <igloo/igloo_alt.h>

#include "../../common/components/transform.h"

using namespace igloo;

Describe(TransformComponentSpec){
    It(should_initialize_with_default_values){// Arrange & Act
                                              TransformComponent transform;

// Assert
Assert::That(transform.position, Equals(glm::vec2(0, 0)));
Assert::That(transform.scale, Equals(glm::vec2(1, 1)));
Assert::That(transform.rotation, Equals(0.0));
}

It(should_initialize_with_custom_values) {
  // Arrange & Act
  glm::vec2 position(10, 20);
  glm::vec2 scale(2, 2);
  double rotation = 45.0;
  TransformComponent transform(position, scale, rotation);

  // Assert
  Assert::That(transform.position, Equals(position));
  Assert::That(transform.scale, Equals(scale));
  Assert::That(transform.rotation, Equals(rotation));
}
}
;
