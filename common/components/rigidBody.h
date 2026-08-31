#pragma once

#include <glm/glm.hpp>

namespace storm {

struct RigidBodyComponent {
  glm::vec2 velocity;

  RigidBodyComponent(glm::vec2 velocity = glm::vec2(0.0, 0.0))
      : velocity{velocity} {}
};
} // namespace storm
