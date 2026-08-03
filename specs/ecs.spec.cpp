#include <igloo/igloo_alt.h>

#include "../common/ecs.h"

using namespace igloo;

struct SpecHealth {
  SpecHealth(int v = 0) : value(v) {}
  int value;
};

struct SpecArmor {
  int value = 0;
};

Describe(EcsSpec) {
  Describe(EntitySpec) {
    It(should_get_identifier_of_entity) {
      Entity entity = Entity(0);
      Assert::That(entity.GetId(), Equals(0));

      Entity entity2 = Entity(99);
      Assert::That(entity2.GetId(), Equals(99));
    };

    It(should_compare_entity) {
      Entity entity = Entity(0);
      Entity entity2 = Entity(0);
      Entity entity3 = Entity(1);

      Assert::That(entity == entity2, Equals(true));
      Assert::That(entity == entity3, Equals(false));

      Assert::That(entity != entity2, Equals(false));
      Assert::That(entity != entity3, Equals(true));

      Assert::That(entity > entity2, Equals(false));
      Assert::That(entity > entity3, Equals(false));
      Assert::That(entity3 > entity, Equals(true));

      Assert::That(entity < entity2, Equals(false));
      Assert::That(entity < entity3, Equals(true));
      Assert::That(entity3 < entity, Equals(false));
    };
  };

  Describe(SystemSpec) {

    It(should_add_entity_to_system) {
      System system;
      Entity entity = Entity(0);
      system.AddEntityToSystem(entity);
      Assert::That(system.GetSystemEntities().size(), Equals(1));
      Assert::That(system.GetSystemEntities()[0].GetId(), Equals(0));

      system.AddEntityToSystem(Entity(88));
      Assert::That(system.GetSystemEntities().size(), Equals(2));
      Assert::That(system.GetSystemEntities()[1].GetId(), Equals(88));
    };

    It(should_remove_entity_from_system) {
      System system;
      Entity entity = Entity(0);
      system.AddEntityToSystem(entity);
      Assert::That(system.GetSystemEntities().size(), Equals(1));
      Assert::That(system.GetSystemEntities()[0].GetId(), Equals(0));
      system.RemoveEntityFromSystem(entity);
      Assert::That(system.GetSystemEntities().size(), Equals(0));
    };

    It(should_get_system_entities) {
      System system;
      Entity entity = Entity(0);
      system.AddEntityToSystem(entity);
      Assert::That(system.GetSystemEntities().size(), Equals(1));
      Assert::That(system.GetSystemEntities()[0].GetId(), Equals(0));
    };
  };

  Describe(PoolSpec) {

    It(should_not_be_empty) {
      typedef Pool<int> Pool;
      Pool p(10);

      Assert::That(p.isEmpty(), Is().EqualTo(false));
    };

    It(should_get_size) {
      Pool<int> p(10);

      Assert::That(p.GetSize(), Is().EqualTo(10));
    };

    It(should_resize) {
      Pool<int> p(10);

      p.Resize(20);
      Assert::That(p.GetSize(), Is().EqualTo(20));
    };

    It(should_clear) {
      Pool<int> p(10);

      p.Clear();
      Assert::That(p.GetSize(), Is().EqualTo(0));
      Assert::That(p.isEmpty(), Is().EqualTo(true));
    };

    It(should_add) {
      Pool<int> p(0);
      p.Add(1);
      p.Add(2);
      p.Add(3);
      Assert::That(p.GetSize(), Is().EqualTo(3));
      Assert::That(p.Get(0), Is().EqualTo(1));
      Assert::That(p.Get(1), Is().EqualTo(2));
      Assert::That(p.Get(2), Is().EqualTo(3));
    };

    It(should_set) {
      Pool<int> p(3);
      p.Set(0, 4);
      p.Set(1, 5);
      p.Set(2, 6);
      Assert::That(p.Get(0), Is().EqualTo(4));
      Assert::That(p.Get(1), Is().EqualTo(5));
      Assert::That(p.Get(2), Is().EqualTo(6));
    };

    It(should_get) {
      Pool<int> p(10);
      p.Set(0, 4);
      p.Set(1, 5);
      p.Set(2, 6);
      Assert::That(p.Get(0), Is().EqualTo(4));
      Assert::That(p.Get(1), Is().EqualTo(5));
      Assert::That(p.Get(2), Is().EqualTo(6));
    };

    It(should_use_operator) {
      Pool<int> p(10);
      p[0] = 7;
      p[1] = 8;
      p[2] = 9;
      Assert::That(p[0], Is().EqualTo(7));
      Assert::That(p[1], Is().EqualTo(8));
      Assert::That(p[2], Is().EqualTo(9));
    };
  };

  Describe(RegistrySpec) {
    It(should_create_entity) {
      Registry registry;
      Entity entity = registry.CreateEntity();
      Assert::That(entity.GetId(), Equals(0));
    };

    It(should_get_component_an_entity_has) {
      Registry registry;
      Entity entity = registry.CreateEntity();
      registry.Update();
      registry.AddComponent<SpecHealth>(entity, 42);
      Assert::That(registry.GetComponent<SpecHealth>(entity).value, Equals(42));
    };

    It(should_return_default_component_when_entity_lacks_the_component) {
      Registry registry;
      Entity entity = registry.CreateEntity();
      registry.Update();
      registry.AddComponent<SpecHealth>(entity, 42);
      SpecArmor &got = registry.GetComponent<SpecArmor>(entity);
      Assert::That(got.value, Equals(0));
    };

    It(should_return_default_component_for_never_added_component_type) {
      Registry registry;
      Entity entity = registry.CreateEntity();
      registry.Update();
      SpecHealth &got = registry.GetComponent<SpecHealth>(entity);
      Assert::That(got.value, Equals(0));
    };

    It(should_not_read_out_of_bounds_for_a_stale_entity_id) {
      Registry registry;
      Entity entity = registry.CreateEntity();
      registry.Update();
      registry.AddComponent<SpecHealth>(entity, 7);
      Entity stale(150); // never created
      SpecHealth &got = registry.GetComponent<SpecHealth>(stale);
      Assert::That(got.value, Equals(0));
    };

    It(should_not_recycle_an_id_twice_when_entity_is_killed_twice) {
      Registry registry;
      Entity a = registry.CreateEntity();
      Entity b = registry.CreateEntity();
      registry.Update();
      Assert::That(a.GetId(), Equals(0));
      Assert::That(b.GetId(), Equals(1));

      registry.KillEntity(a);
      registry.Update();
      registry.KillEntity(a); // stale handle — double kill
      registry.Update();

      Entity c = registry.CreateEntity();
      Entity d = registry.CreateEntity();
      registry.Update();
      Assert::That(c.GetId(), Equals(0));
      Assert::That(d.GetId(), Equals(2)); // not aliased to c
      Assert::That(c.GetId(), Is().Not().EqualTo(d.GetId()));
    };

    It(should_ignore_kill_of_a_never_created_entity) {
      Registry registry;
      registry.KillEntity(Entity(7));
      registry.Update();
      Entity e = registry.CreateEntity();
      registry.Update();
      Assert::That(e.GetId(), Equals(0)); // 7 was never recycled
    };

    It(should_ignore_repeated_kill_in_the_same_frame) {
      Registry registry;
      Entity a = registry.CreateEntity();
      registry.Update();
      registry.KillEntity(a);
      registry.KillEntity(a);
      registry.Update();
      Entity b = registry.CreateEntity();
      Entity c = registry.CreateEntity();
      registry.Update();
      Assert::That(b.GetId(), Equals(0));
      Assert::That(c.GetId(), Equals(1));
    };

    It(should_initialize_registry_pointer_to_nullptr_on_bare_entity) {
      Entity entity = Entity(5);
      Assert::That(entity.registry, Equals((Registry *)nullptr));
    };

    It(should_not_crash_when_add_component_is_called_on_a_stale_entity) {
      Registry registry;
      Entity stale(150); // never created, registry pointer is nullptr
      registry.AddComponent<SpecHealth>(stale, 42);
      // Should not crash — bounds check rejects entity 150
      Entity e = registry.CreateEntity();
      registry.Update();
      Assert::That(e.GetId(), Equals(0));
    };

    It(should_not_crash_when_remove_component_is_called_on_a_stale_entity) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.Update();
      registry.AddComponent<SpecHealth>(e, 10);
      Entity stale(150); // never created
      registry.RemoveComponent<SpecHealth>(stale);
      // The live entity's component should be untouched
      Assert::That(registry.GetComponent<SpecHealth>(e).value, Equals(10));
    };

    It(should_find_entity_in_group_by_entity_not_by_id) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.Update();
      registry.GroupEntity(e, "players");
      Assert::That(registry.EntityBelongsToGroup(e, "players"), Equals(true));
      Entity other = registry.CreateEntity();
      registry.Update();
      Assert::That(registry.EntityBelongsToGroup(other, "players"),
                   Equals(false));
    };
  };
};