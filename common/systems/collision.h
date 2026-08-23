#pragma once

#include "../components/boxCollider.h"
#include "../components/rigidBody.h"
#include "../components/transform.h"
#include "../ecs.h"
#include "contact.h"

// Deprecated as of 1.3.0. CollisionSystem can only respond to an overlap by
// killing both movable entities - there is no event, no manifold, and no way
// to observe a contact without acting on it (KNOWN_ISSUES.md #10).
// ContactSystem (../systems/contact.h) is the replacement.
//
// This stays, unchanged in behaviour, for source compatibility with games
// written against 1.0-1.2. Removing it is a v3 item.
class CollisionSystem : public System {
public:
  CollisionSystem() {
    RequireComponent<TransformComponent>();
    RequireComponent<BoxColliderComponent>();
  }

  void Update() {
    auto &entities = GetSystemEntities();
    for (auto it = entities.begin(); it != entities.end(); ++it) {
      auto entityA = *it;
      for (auto loopIt = it; loopIt != entities.end(); ++loopIt) {
        if (it == loopIt)
          continue;
        auto entityB = *loopIt;
        if (isCollision(entityA, entityB)) {
          // Only kill entities that can move: a wall or other static object
          // (no RigidBodyComponent) survives contact, so a moving entity
          // slamming into scenery dies instead of taking it with it.
          if (entityA.HasComponent<RigidBodyComponent>())
            entityA.Kill();
          if (entityB.HasComponent<RigidBodyComponent>())
            entityB.Kill();

          // Observing a contact without killing anything is what
          // ContactSystem is for.
        }
      }
    }
  }

  // AABB (axis-aligned bounding boxes) collision detection.
  //
  // The bounds math lives in ContactSystem::BoundsOf so there is one copy of
  // it. The overlap test below stays inclusive, unlike
  // ContactSystem::Overlaps: a shared edge has counted as a collision since
  // 1.0 and narrowing that now would be a silent behaviour break.
  bool isCollision(const Entity &entA, const Entity &entB) {
    const ContactAABB a = ContactSystem::BoundsOf(entA);
    const ContactAABB b = ContactSystem::BoundsOf(entB);

    return a.minX <= b.maxX && a.maxX >= b.minX && a.minY <= b.maxY &&
           a.maxY >= b.minY;
  }
};
