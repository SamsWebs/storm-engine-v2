#pragma once

#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>

#include "../components/puckComponent.h"

using namespace storm;

// HockeyPhysicsSystem: integrates the free puck and bleeds its speed off.
//
// It does NOT handle the boards. The rink walls are ordinary collider
// entities (PlayState::SpawnWalls) and the bounce is a reflection about the
// contact normal ContactSystem reports (PlayState::ResolvePuckContacts), so
// this system no longer needs to know where the rink is. That is what let the
// old RL/RT/RR/RB constants - which had to be kept in sync with PlayState by
// hand - go away.
//
// Goal detection is still PlayState's job; it reads the puck position.
class HockeyPhysicsSystem : public System {
public:
  HockeyPhysicsSystem() {
    RequireComponent<TransformComponent>();
    RequireComponent<PuckComponent>();
  }

  void Update(double dt) {
    for (auto &entity : GetSystemEntities()) {
      auto &transform = entity.GetComponent<TransformComponent>();
      auto &puck = entity.GetComponent<PuckComponent>();

      // If held by someone, skip physics
      if (puck.ownerTag != -1)
        continue;

      // Move puck
      transform.position.x += puck.velocity.x * static_cast<float>(dt);
      transform.position.y += puck.velocity.y * static_cast<float>(dt);

      // Apply friction (exponential decay each frame)
      float drag = 1.f - puck.friction * static_cast<float>(dt);
      if (drag < 0.f)
        drag = 0.f;
      puck.velocity *= drag;

      // Stop very slow puck to avoid endless micro-movement
      if (glm::length(puck.velocity) < 5.f)
        puck.velocity = {0.f, 0.f};
    }
  }
};
