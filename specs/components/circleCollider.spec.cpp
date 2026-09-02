#include <glm/glm.hpp>
#include <igloo/igloo_alt.h>

#include "../../common/components/circleCollider.h"

using namespace igloo;
using namespace storm;

Describe(CircleColliderComponentSpec) {

  It(should_initialize_with_default_values) {
    // Arrange & Act
    CircleColliderComponent collider;

    // Assert
    Assert::That(collider.radius, Equals(0.0f));
    Assert::That(collider.offset, Equals(glm::vec2(0)));
  }

  It(should_initialize_with_custom_values) {
    // Arrange & Act
    float radius = 12.5f;
    glm::vec2 offset(16, -4);
    CircleColliderComponent collider(radius, offset);

    // Assert
    Assert::That(collider.radius, Equals(radius));
    Assert::That(collider.offset, Equals(offset));
  }

  // The radius is a float on purpose -- a box's extents are ints because they
  // are usually a sprite cell, and a radius is usually half of one. Rounding
  // 3.5 px to 3 or 4 is a visible difference on a small body.
  It(should_keep_a_fractional_radius) {
    // Arrange & Act
    CircleColliderComponent collider(3.5f);

    // Assert
    Assert::That(collider.radius, Equals(3.5f));
  }
}
;
