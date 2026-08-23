#pragma once

#include <glm/glm.hpp>

struct PuckComponent {
  glm::vec2 velocity = {0.f, 0.f};
  int ownerTag = -1;     // -1 = free; 0 = player; 1 = AI skater; 2 = AI goalie
  float friction = 1.8f; // speed lost per second (units/s^2 style drag)
  // Seconds left before anyone may pick the puck up again. A shot puck
  // starts out exactly concentric with the shooter, so without this it is
  // back inside PICKUP_RADIUS on the very next frame and gets re-grabbed
  // before it can travel.
  float pickupLock = 0.f;

  PuckComponent() = default;
};
