#pragma once

#include "../components/boxCollider.h"
#include "../components/rigidBody.h"
#include "../components/transform.h"
#include "../ecs.h"

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

          // TODO: emit an event
        }
      }
    }
  }
  // AABB (axis-aligned bounding boxes) collision detection
  bool isCollision(const Entity &entA, const Entity &entB) {
    const auto &tComponentA = entA.GetComponent<TransformComponent>();
    const auto &tComponentB = entB.GetComponent<TransformComponent>();
    const auto &colliderComponentA = entA.GetComponent<BoxColliderComponent>();
    const auto &colliderComponentB = entB.GetComponent<BoxColliderComponent>();

    const auto entAXmin = tComponentA.position.x + colliderComponentA.offset.x;
    const auto entAXmax =
        entAXmin + colliderComponentA.width * tComponentA.scale.x;

    const auto entAYmin = tComponentA.position.y + colliderComponentA.offset.y;
    const auto entAYmax =
        entAYmin + colliderComponentA.height * tComponentA.scale.y;

    const auto entBXmin = tComponentB.position.x + colliderComponentB.offset.x;
    const auto entBXmax =
        entBXmin + colliderComponentB.width * tComponentB.scale.x;

    const auto entBYmin = tComponentB.position.y + colliderComponentB.offset.y;
    const auto entBYmax =
        entBYmin + colliderComponentB.height * tComponentB.scale.y;

    return entAXmin <= entBXmax && entAXmax >= entBXmin &&
           entAYmin <= entBYmax && entAYmax >= entBYmin;
  }
};