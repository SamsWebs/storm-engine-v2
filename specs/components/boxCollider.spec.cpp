#include <glm/glm.hpp>
#include <igloo/igloo_alt.h>

#include "../../common/components/boxCollider.h"

using namespace igloo;

Describe(BoxColliderComponentSpec){
    It(should_initialize_with_default_values){// Arrange & Act
                                              BoxColliderComponent collider;

// Assert
Assert::That(collider.width, Equals(0));
Assert::That(collider.height, Equals(0));
Assert::That(collider.offset, Equals(glm::vec2(0)));
}

It(should_initialize_with_custom_values) {
  // Arrange & Act
  int width = 100;
  int height = 50;
  glm::vec2 offset(10, -5);
  BoxColliderComponent collider(width, height, offset);

  // Assert
  Assert::That(collider.width, Equals(width));
  Assert::That(collider.height, Equals(height));
  Assert::That(collider.offset, Equals(offset));
}
}
;
