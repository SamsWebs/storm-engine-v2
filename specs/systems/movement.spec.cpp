#include <igloo/igloo_alt.h>

#include "../../common/systems/movement.h"

using namespace igloo;
using namespace storm;

namespace {

// glm::vec2 is float, and every value used below is exactly representable
// (halves and quarters), but the integration goes through a double multiply
// before narrowing — compare with a tolerance rather than for bit equality.
constexpr float kTolerance = 1e-5f;

} // namespace

Describe(MovementSystemSpec) {
  It(should_advance_position_by_velocity_times_delta_time) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(0, 0));
    entity.AddComponent<RigidBodyComponent>(glm::vec2(2.0, 1.5));

    registry.Update(); // admits the entity to the system
    registry.GetSystem<MovementSystem>().Update(0.5);

    const auto &transform = entity.GetComponent<TransformComponent>();
    Assert::That(transform.position.x, EqualsWithDelta(1.0f, kTolerance));
    Assert::That(transform.position.y, EqualsWithDelta(0.75f, kTolerance));
  };

  It(should_integrate_from_the_current_position_on_every_update) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(10, -4));
    entity.AddComponent<RigidBodyComponent>(glm::vec2(2.0, 1.5));

    registry.Update();
    registry.GetSystem<MovementSystem>().Update(0.5);
    registry.GetSystem<MovementSystem>().Update(0.5);

    const auto &transform = entity.GetComponent<TransformComponent>();
    Assert::That(transform.position.x, EqualsWithDelta(12.0f, kTolerance));
    Assert::That(transform.position.y, EqualsWithDelta(-2.5f, kTolerance));
  };

  It(should_move_an_entity_backwards_for_a_negative_velocity) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(8, 8));
    entity.AddComponent<RigidBodyComponent>(glm::vec2(-4.0, -2.0));

    registry.Update();
    registry.GetSystem<MovementSystem>().Update(0.25);

    const auto &transform = entity.GetComponent<TransformComponent>();
    Assert::That(transform.position.x, EqualsWithDelta(7.0f, kTolerance));
    Assert::That(transform.position.y, EqualsWithDelta(7.5f, kTolerance));
  };

  It(should_leave_a_resting_entity_where_it_is) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(3, 7));
    entity.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 0.0));

    registry.Update();
    registry.GetSystem<MovementSystem>().Update(1.0);

    const auto &transform = entity.GetComponent<TransformComponent>();
    Assert::That(transform.position.x, EqualsWithDelta(3.0f, kTolerance));
    Assert::That(transform.position.y, EqualsWithDelta(7.0f, kTolerance));
  };

  It(should_move_every_entity_the_system_holds) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity first = registry.CreateEntity();
    first.AddComponent<TransformComponent>(glm::vec2(0, 0));
    first.AddComponent<RigidBodyComponent>(glm::vec2(1.0, 0.0));

    Entity second = registry.CreateEntity();
    second.AddComponent<TransformComponent>(glm::vec2(0, 0));
    second.AddComponent<RigidBodyComponent>(glm::vec2(0.0, 3.0));

    registry.Update();
    registry.GetSystem<MovementSystem>().Update(2.0);

    Assert::That(
        registry.GetSystem<MovementSystem>().GetSystemEntities().size(),
        Equals(2u));
    Assert::That(first.GetComponent<TransformComponent>().position.x,
                 EqualsWithDelta(2.0f, kTolerance));
    Assert::That(second.GetComponent<TransformComponent>().position.y,
                 EqualsWithDelta(6.0f, kTolerance));
  };

  // The system requires both components, so a transform-only entity is never
  // in its list and is never integrated.
  It(should_not_move_an_entity_without_a_rigid_body) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity scenery = registry.CreateEntity();
    scenery.AddComponent<TransformComponent>(glm::vec2(5, 5));

    registry.Update();
    registry.GetSystem<MovementSystem>().Update(1.0);

    Assert::That(
        registry.GetSystem<MovementSystem>().GetSystemEntities().size(),
        Equals(0u));
    Assert::That(scenery.GetComponent<TransformComponent>().position.x,
                 EqualsWithDelta(5.0f, kTolerance));
    Assert::That(scenery.GetComponent<TransformComponent>().position.y,
                 EqualsWithDelta(5.0f, kTolerance));
  };

  // Registry::Update is what admits an entity to a system. Without it the
  // system's list is empty and Update integrates nothing — which is exactly
  // why the previous version of this file could not fail.
  It(should_move_nothing_before_the_registry_flush) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(0, 0));
    entity.AddComponent<RigidBodyComponent>(glm::vec2(9.0, 9.0));

    registry.GetSystem<MovementSystem>().Update(1.0); // no flush yet

    Assert::That(
        registry.GetSystem<MovementSystem>().GetSystemEntities().size(),
        Equals(0u));
    Assert::That(entity.GetComponent<TransformComponent>().position.x,
                 EqualsWithDelta(0.0f, kTolerance));
  };
};
