#include "../../common/systems/collision.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(CollisionSystemSpec){
    It(should_kill_both_movable_entities_that_touch){Registry registry;
registry.AddSystem<CollisionSystem>();

Entity entityA = registry.CreateEntity();
entityA.AddComponent<TransformComponent>();
entityA.AddComponent<BoxColliderComponent>();
entityA.AddComponent<RigidBodyComponent>();
Entity entityB = registry.CreateEntity();
entityB.AddComponent<TransformComponent>();
entityB.AddComponent<BoxColliderComponent>();
entityB.AddComponent<RigidBodyComponent>();

entityA.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
entityB.GetComponent<TransformComponent>().position = glm::vec2(5, 5);
entityA.GetComponent<BoxColliderComponent>().width = 10;
entityA.GetComponent<BoxColliderComponent>().height = 10;
entityB.GetComponent<BoxColliderComponent>().width = 10;
entityB.GetComponent<BoxColliderComponent>().height = 10;

registry.Update(); // register the entities with the system
registry.GetSystem<CollisionSystem>().Update();
registry.Update(); // process the deferred kills

Assert::That(registry.GetSystem<CollisionSystem>().GetSystemEntities().size(),
             Equals(0));
}

It(should_kill_only_the_movable_entity_when_it_touches_a_static_one) {
  Registry registry;
  registry.AddSystem<CollisionSystem>();

  Entity moving = registry.CreateEntity();
  moving.AddComponent<TransformComponent>();
  moving.AddComponent<BoxColliderComponent>();
  moving.AddComponent<RigidBodyComponent>();
  Entity wall = registry.CreateEntity();
  wall.AddComponent<TransformComponent>();
  wall.AddComponent<BoxColliderComponent>();

  moving.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
  wall.GetComponent<TransformComponent>().position = glm::vec2(5, 5);
  moving.GetComponent<BoxColliderComponent>().width = 10;
  moving.GetComponent<BoxColliderComponent>().height = 10;
  wall.GetComponent<BoxColliderComponent>().width = 10;
  wall.GetComponent<BoxColliderComponent>().height = 10;

  registry.Update();
  registry.GetSystem<CollisionSystem>().Update();
  registry.Update();

  Assert::That(registry.GetSystem<CollisionSystem>().GetSystemEntities().size(),
               Equals(1));
}

It(should_leave_overlapping_static_entities_alone) {
  Registry registry;
  registry.AddSystem<CollisionSystem>();

  Entity wallA = registry.CreateEntity();
  wallA.AddComponent<TransformComponent>();
  wallA.AddComponent<BoxColliderComponent>();
  Entity wallB = registry.CreateEntity();
  wallB.AddComponent<TransformComponent>();
  wallB.AddComponent<BoxColliderComponent>();

  wallA.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
  wallB.GetComponent<TransformComponent>().position = glm::vec2(5, 5);
  wallA.GetComponent<BoxColliderComponent>().width = 10;
  wallA.GetComponent<BoxColliderComponent>().height = 10;
  wallB.GetComponent<BoxColliderComponent>().width = 10;
  wallB.GetComponent<BoxColliderComponent>().height = 10;

  registry.Update();
  registry.GetSystem<CollisionSystem>().Update();
  registry.Update();

  Assert::That(registry.GetSystem<CollisionSystem>().GetSystemEntities().size(),
               Equals(2));
}

It(should_not_kill_entities_that_do_not_touch) {
  Registry registry;
  registry.AddSystem<CollisionSystem>();

  Entity entityA = registry.CreateEntity();
  entityA.AddComponent<TransformComponent>();
  entityA.AddComponent<BoxColliderComponent>();
  entityA.AddComponent<RigidBodyComponent>();
  Entity entityB = registry.CreateEntity();
  entityB.AddComponent<TransformComponent>();
  entityB.AddComponent<BoxColliderComponent>();
  entityB.AddComponent<RigidBodyComponent>();

  entityA.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
  entityB.GetComponent<TransformComponent>().position = glm::vec2(50, 50);
  entityA.GetComponent<BoxColliderComponent>().width = 10;
  entityA.GetComponent<BoxColliderComponent>().height = 10;
  entityB.GetComponent<BoxColliderComponent>().width = 10;
  entityB.GetComponent<BoxColliderComponent>().height = 10;

  registry.Update();
  registry.GetSystem<CollisionSystem>().Update();
  registry.Update();

  Assert::That(registry.GetSystem<CollisionSystem>().GetSystemEntities().size(),
               Equals(2));
}
}
;
