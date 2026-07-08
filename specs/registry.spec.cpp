#include <igloo/igloo_alt.h>

#include "../common/ecs.h"

using namespace igloo;

// ─────────────────────────────────────────────────────────────────────────────
// Test fixtures: minimal components and a system that requires one of them.
// ─────────────────────────────────────────────────────────────────────────────
struct PositionComponent {
  float x = 0.f, y = 0.f;
  PositionComponent(float x = 0.f, float y = 0.f) : x(x), y(y) {}
};

struct VelocityComponent {
  float dx = 0.f, dy = 0.f;
  VelocityComponent(float dx = 0.f, float dy = 0.f) : dx(dx), dy(dy) {}
};

class PositionSystem : public System {
public:
  PositionSystem() { RequireComponent<PositionComponent>(); }
};

Describe(RegistrySpec) {

  Describe(ComponentManagement) {
    It(should_add_and_report_a_component) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.AddComponent<PositionComponent>(e, 1.f, 2.f);
      Assert::That(registry.HasComponent<PositionComponent>(e), Equals(true));
    };

    It(should_report_missing_component_as_absent) {
      Registry registry;
      Entity e = registry.CreateEntity();
      Assert::That(registry.HasComponent<PositionComponent>(e), Equals(false));
    };

    It(should_retrieve_a_component_with_its_values) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.AddComponent<PositionComponent>(e, 3.5f, 4.5f);
      auto &pos = registry.GetComponent<PositionComponent>(e);
      Assert::That(pos.x, Equals(3.5f));
      Assert::That(pos.y, Equals(4.5f));
    };

    It(should_remove_a_component) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.AddComponent<PositionComponent>(e, 1.f, 2.f);
      registry.RemoveComponent<PositionComponent>(e);
      Assert::That(registry.HasComponent<PositionComponent>(e), Equals(false));
    };

    It(should_track_multiple_component_types_independently) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.AddComponent<PositionComponent>(e, 1.f, 2.f);
      Assert::That(registry.HasComponent<PositionComponent>(e), Equals(true));
      Assert::That(registry.HasComponent<VelocityComponent>(e), Equals(false));
    };
  };

  Describe(SystemManagement) {
    It(should_add_and_report_a_system) {
      Registry registry;
      registry.AddSystem<PositionSystem>();
      Assert::That(registry.HasSystem<PositionSystem>(), Equals(true));
    };

    It(should_report_missing_system_as_absent) {
      Registry registry;
      Assert::That(registry.HasSystem<PositionSystem>(), Equals(false));
    };

    It(should_remove_a_system) {
      Registry registry;
      registry.AddSystem<PositionSystem>();
      registry.RemoveSystem<PositionSystem>();
      Assert::That(registry.HasSystem<PositionSystem>(), Equals(false));
    };

    It(should_treat_removing_an_absent_system_as_a_no_op) {
      Registry registry;
      registry.RemoveSystem<PositionSystem>(); // must not crash or corrupt
      Assert::That(registry.HasSystem<PositionSystem>(), Equals(false));
    };

    It(should_retrieve_a_system) {
      Registry registry;
      registry.AddSystem<PositionSystem>();
      PositionSystem &sys = registry.GetSystem<PositionSystem>();
      Assert::That(sys.GetSystemEntities().size(), Equals(0u));
    };
  };

  Describe(EntitySystemMatching) {
    It(should_add_a_matching_entity_to_the_system_on_update) {
      Registry registry;
      registry.AddSystem<PositionSystem>();
      Entity e = registry.CreateEntity();
      registry.AddComponent<PositionComponent>(e, 0.f, 0.f);
      registry.Update();
      Assert::That(
          registry.GetSystem<PositionSystem>().GetSystemEntities().size(),
          Equals(1u));
    };

    It(should_not_add_a_non_matching_entity_to_the_system) {
      Registry registry;
      registry.AddSystem<PositionSystem>();
      Entity e = registry.CreateEntity();
      registry.AddComponent<VelocityComponent>(e, 0.f, 0.f);
      registry.Update();
      Assert::That(
          registry.GetSystem<PositionSystem>().GetSystemEntities().size(),
          Equals(0u));
    };
  };

  Describe(TagManagement) {
    It(should_tag_an_entity_and_report_the_tag) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      Assert::That(registry.EntityHasTag(e, "player"), Equals(true));
    };

    It(should_retrieve_an_entity_by_tag) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      Entity found = registry.GetEntityByTag("player");
      Assert::That(found.GetId(), Equals(e.GetId()));
    };

    It(should_not_report_a_tag_the_entity_does_not_have) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      Assert::That(registry.EntityHasTag(e, "enemy"), Equals(false));
    };

    It(should_remove_a_tag) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      registry.RemoveEntityTag(e);
      Assert::That(registry.EntityHasTag(e, "player"), Equals(false));
    };

    It(should_replace_an_entitys_tag_when_retagged) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      registry.TagEntity(e, "goalie"); // last write wins
      Assert::That(registry.EntityHasTag(e, "goalie"), Equals(true));
      Assert::That(registry.EntityHasTag(e, "player"), Equals(false));
    };

    It(should_move_a_tag_between_entities_when_reused) {
      Registry registry;
      Entity a = registry.CreateEntity();
      Entity b = registry.CreateEntity();
      registry.TagEntity(a, "player");
      registry.TagEntity(b, "player"); // last write wins
      Assert::That(registry.EntityHasTag(b, "player"), Equals(true));
      Assert::That(registry.EntityHasTag(a, "player"), Equals(false));
      Assert::That(registry.GetEntityByTag("player").GetId(), Equals(b.GetId()));
    };
  };

  Describe(GroupManagement) {
    It(should_group_an_entity_and_report_membership) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "enemies");
      Assert::That(registry.EntityBelongsToGroup(e, "enemies"), Equals(true));
    };

    It(should_report_existing_group) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "enemies");
      Assert::That(registry.DoesGroupExist("enemies"), Equals(true));
    };

    It(should_report_unknown_group_as_nonexistent) {
      Registry registry;
      Assert::That(registry.DoesGroupExist("ghosts"), Equals(false));
    };

    It(should_return_all_entities_in_a_group) {
      Registry registry;
      Entity a = registry.CreateEntity();
      Entity b = registry.CreateEntity();
      registry.GroupEntity(a, "enemies");
      registry.GroupEntity(b, "enemies");
      auto entities = registry.GetEntitiesByGroup("enemies");
      Assert::That(entities.size(), Equals(2u));
    };

    It(should_not_report_membership_for_a_different_group) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "enemies");
      Assert::That(registry.EntityBelongsToGroup(e, "allies"), Equals(false));
    };

    It(should_remove_an_entity_from_its_group) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "enemies");
      registry.RemoveEntityGroup(e);
      Assert::That(registry.EntityBelongsToGroup(e, "enemies"), Equals(false));
    };

    It(should_move_an_entity_when_regrouped) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "enemies");
      registry.GroupEntity(e, "allies"); // one group per entity — a move
      Assert::That(registry.EntityBelongsToGroup(e, "allies"), Equals(true));
      Assert::That(registry.EntityBelongsToGroup(e, "enemies"), Equals(false));
    };
  };

  Describe(EntityLifecycle) {
    It(should_create_entities_with_sequential_ids) {
      Registry registry;
      Entity a = registry.CreateEntity();
      Entity b = registry.CreateEntity();
      Assert::That(a.GetId(), Equals(0u));
      Assert::That(b.GetId(), Equals(1u));
    };

    It(should_queue_an_entity_to_be_killed) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.Update();
      registry.KillEntity(e);
      Assert::That(registry.GetEntitiesToBeKilled().size(), Equals(1u));
    };

    It(should_clear_the_kill_queue_after_update) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.Update();
      registry.KillEntity(e);
      registry.Update();
      Assert::That(registry.GetEntitiesToBeKilled().size(), Equals(0u));
    };

    It(should_recycle_a_killed_entity_id) {
      Registry registry;
      Entity a = registry.CreateEntity(); // id 0
      registry.Update();
      registry.KillEntity(a);
      registry.Update(); // frees id 0
      Entity b = registry.CreateEntity(); // should reuse id 0
      Assert::That(b.GetId(), Equals(0u));
    };

    It(should_remove_a_killed_entity_from_its_system) {
      Registry registry;
      registry.AddSystem<PositionSystem>();
      Entity e = registry.CreateEntity();
      registry.AddComponent<PositionComponent>(e, 0.f, 0.f);
      registry.Update();
      registry.KillEntity(e);
      registry.Update();
      Assert::That(
          registry.GetSystem<PositionSystem>().GetSystemEntities().size(),
          Equals(0u));
    };

    It(should_remove_a_killed_entity_from_its_group) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "enemies");
      registry.Update();
      registry.KillEntity(e);
      registry.Update();
      Assert::That(registry.EntityBelongsToGroup(e, "enemies"), Equals(false));
    };

    It(should_release_a_killed_entitys_tag) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      registry.Update();
      registry.KillEntity(e);
      registry.Update();
      Assert::That(registry.EntityHasTag(e, "player"), Equals(false));
    };

    It(should_not_let_a_recycled_id_inherit_the_old_tag) {
      Registry registry;
      Entity a = registry.CreateEntity(); // id 0
      registry.TagEntity(a, "player");
      registry.Update();
      registry.KillEntity(a);
      registry.Update();                  // id 0 freed — tag must go with it
      Entity b = registry.CreateEntity(); // reuses id 0
      Assert::That(registry.EntityHasTag(b, "player"), Equals(false));
    };
  };
};
