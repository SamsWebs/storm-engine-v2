#include <igloo/igloo_alt.h>

#include "../common/ecs.h"

using namespace igloo;

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
      Pool<int> p = Pool<int>(10);

      // Test isEmpty()
      Assert::That(p.isEmpty(), Is().EqualTo(false));
    };

    It(should_get_size) {
      Pool<int> p = Pool<int>(10);

      Assert::That(p.GetSize(), Is().EqualTo(10));
    };

    It(should_resize) {
      Pool<int> p = Pool<int>(10);

      p.Resize(20);
      Assert::That(p.GetSize(), Is().EqualTo(20));
    };

    It(should_clear) {
      Pool<int> p = Pool<int>(10);

      p.Clear();
      Assert::That(p.GetSize(), Is().EqualTo(0));
      Assert::That(p.isEmpty(), Is().EqualTo(true));
    };

    It(should_add) {
      Pool<int> p = Pool<int>(0);
      p.Add(1);
      p.Add(2);
      p.Add(3);
      Assert::That(p.GetSize(), Is().EqualTo(3));
      Assert::That(p.Get(0), Is().EqualTo(1));
      Assert::That(p.Get(1), Is().EqualTo(2));
      Assert::That(p.Get(2), Is().EqualTo(3));
    };

    It(should_set) {
      Pool<int> p = Pool<int>(3);
      p.Set(0, 4);
      p.Set(1, 5);
      p.Set(2, 6);
      Assert::That(p.Get(0), Is().EqualTo(4));
      Assert::That(p.Get(1), Is().EqualTo(5));
      Assert::That(p.Get(2), Is().EqualTo(6));
    };

    It(should_get) {
      Pool<int> p = Pool<int>(10);
      p.Set(0, 4);
      p.Set(1, 5);
      p.Set(2, 6);
      Assert::That(p.Get(0), Is().EqualTo(4));
      Assert::That(p.Get(1), Is().EqualTo(5));
      Assert::That(p.Get(2), Is().EqualTo(6));
    };

    It(should_use_opertor) {
      Pool<int> p = Pool<int>(10);
      p[0] = 7;
      p[1] = 8;
      p[2] = 9;
      Assert::That(p[0], Is().EqualTo(7));
      Assert::That(p[1], Is().EqualTo(8));
      Assert::That(p[2], Is().EqualTo(9));
    };
  };
};