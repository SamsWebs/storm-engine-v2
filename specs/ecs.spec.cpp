#include <igloo/igloo_alt.h>

#include "../common/stormengine2/ecs.h"

using namespace igloo;

Describe(EcsSpec) {
  It(should_get_identifier_of_entity) {
    Entity entity = Entity(0);
    Assert::That(entity.GetId(), Equals(0));

    Entity entity2 = Entity(99);
    Assert::That(entity2.GetId(), Equals(99));
  };

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