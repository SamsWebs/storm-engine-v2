#include <igloo/igloo_alt.h>

#include <new>
#include <stdexcept>

#include "../common/ecs.h"
#include "../common/logger.h"
#include "support/freshDiagnosticBudget.h"

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

// Forward-declared so EntityLifecycle (below) can use it; defined after
// RegistrySpec, alongside the other spec-local helpers.
static std::size_t SpecRegistryErrorCount();

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
      Assert::That(registry.GetEntityByTag("player").GetId(),
                   Equals(b.GetId()));
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
      registry.Update();                  // frees id 0
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

    It(should_not_recycle_an_id_twice_when_a_stale_handle_is_killed_again) {
      Registry registry;
      Entity a = registry.CreateEntity(); // id 0
      Entity b = registry.CreateEntity(); // id 1
      registry.Update();

      registry.KillEntity(a);
      registry.Update();
      registry.KillEntity(a); // stale handle, id 0 is on the free list
      registry.Update();

      Entity c = registry.CreateEntity(); // reuses id 0
      Entity d = registry.CreateEntity(); // must be a fresh id, not 0 again
      registry.Update();

      Assert::That(c.GetId(), Equals(0u));
      Assert::That(d.GetId(), Equals(2u));
      Assert::That(b.GetId(), Equals(1u));
    };

    It(should_reject_a_kill_through_a_recycled_stale_handle) {
      // FIXED, was pinned deliberately. The three KillEntity guards used to
      // all pass for a stale handle whose id had already been recycled, so
      // the kill landed on the new, live entity. Closing it needed a
      // generation counter inside Entity, which changed sizeof(Entity) — an
      // ABI break, tracked as P5 in docs/TECH_DEBT.md. Now IsAlive checks the
      // generation, so the stale kill below is rejected instead.
      //
      // KillEntity's "not alive" diagnostic is throttled by a counter shared
      // with every other spec that exercises this exact rejection on the
      // main thread — see OnFreshDiagnosticBudget (specs/support/), which
      // runs the stale kill below on a fresh thread so it gets its own,
      // untouched budget.
      Registry registry;
      registry.AddSystem<PositionSystem>();

      Entity doomed = registry.CreateEntity(); // id 0
      registry.Update();
      registry.KillEntity(doomed);
      registry.Update(); // id 0 freed

      Entity recycled = registry.CreateEntity(); // takes id 0 back, new generation
      registry.AddComponent<PositionComponent>(recycled, 1.f, 2.f);
      registry.Update();
      Assert::That(recycled.GetId(), Equals(doomed.GetId()));
      Assert::That(
          registry.GetSystem<PositionSystem>().GetSystemEntities().size(),
          Equals(1u));

      Logger::messages.clear();
      OnFreshDiagnosticBudget([&] {
        registry.KillEntity(doomed); // stale, but indistinguishable from
                                     // `recycled` by id alone
      });
      registry.Update();

      // The stale kill must not touch the new, live entity.
      Assert::That(
          registry.GetSystem<PositionSystem>().GetSystemEntities().size(),
          Equals(1u));
      Assert::That(registry.IsAlive(recycled), Equals(true));
      Assert::That(registry.IsAlive(doomed), Equals(false));
      // Not just alive — intact. A kill that half-succeeded would still
      // report alive.
      PositionComponent &pos =
          registry.GetComponent<PositionComponent>(recycled);
      Assert::That(pos.x, Equals(1.f));
      Assert::That(pos.y, Equals(2.f));
      Assert::That(SpecRegistryErrorCount(),
                   Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
      Logger::messages.clear();
    };

    It(should_ignore_a_kill_of_an_entity_that_was_never_created) {
      Registry registry;
      registry.KillEntity(Entity(7));
      registry.Update();

      Entity e = registry.CreateEntity();
      Assert::That(e.GetId(), Equals(0u)); // 7 was never put on the free list
    };
  };

  // P20 — GetEntitiesByGroup used .at(), which terminates the process under
  // -fno-exceptions when the group has never been created.
  Describe(MissingLookups) {
    It(should_return_an_empty_list_for_an_unknown_group) {
      Registry registry;
      Assert::That(registry.DoesGroupExist("colliders"), Equals(false));
      Assert::That(registry.GetEntitiesByGroup("colliders").size(), Equals(0u));
    };

    It(should_still_return_members_of_a_known_group) {
      Registry registry;
      Entity e = registry.CreateEntity();
      registry.GroupEntity(e, "colliders");
      registry.Update();
      Assert::That(registry.GetEntitiesByGroup("colliders").size(), Equals(1u));
    };

    It(should_report_whether_a_tag_exists) {
      // GetEntityByTag cannot report a miss through its return type (Entity
      // has no "none" value), so DoesTagExist is the guard callers need.
      Registry registry;
      Assert::That(registry.DoesTagExist("player"), Equals(false));

      Entity e = registry.CreateEntity();
      registry.TagEntity(e, "player");
      Assert::That(registry.DoesTagExist("player"), Equals(true));
      Assert::That(registry.GetEntityByTag("player").GetId(),
                   Equals(e.GetId()));

      registry.KillEntity(e);
      registry.Update();
      Assert::That(registry.DoesTagExist("player"), Equals(false));
    };
  };
};

struct SpecLateSystemMarker {
  int value = 0;
  SpecLateSystemMarker(int value = 0) : value(value) {}
};

class SpecLateRegisteredSystem : public System {
public:
  SpecLateRegisteredSystem() { RequireComponent<SpecLateSystemMarker>(); }
};

static std::size_t SpecRegistryErrorCount() {
  std::size_t errors = 0;
  for (const auto &entry : Logger::messages) {
    if (entry.type == LogType::LOG_ERROR) {
      ++errors;
    }
  }
  return errors;
}

Describe(LateSystemRegistrationSpec) {
  It(should_report_when_matching_entities_were_already_admitted) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.AddComponent<SpecLateSystemMarker>();
    Entity second = registry.CreateEntity();
    second.AddComponent<SpecLateSystemMarker>();
    registry.Update();

    Logger::messages.clear();
    registry.AddSystem<SpecLateRegisteredSystem>();

    Assert::That(SpecRegistryErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_stay_silent_when_registered_before_any_entity_exists) {
    Registry registry;
    Logger::messages.clear();
    registry.AddSystem<SpecLateRegisteredSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<SpecLateSystemMarker>();
    registry.Update();

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_stay_silent_when_no_existing_entity_matches) {
    Registry registry;
    registry.CreateEntity();
    registry.Update();

    Logger::messages.clear();
    registry.AddSystem<SpecLateRegisteredSystem>();

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_backfill_the_admitted_entities_on_request) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.AddComponent<SpecLateSystemMarker>(11);
    Entity second = registry.CreateEntity();
    second.AddComponent<SpecLateSystemMarker>(22);
    registry.Update();

    registry.AddSystem<SpecLateRegisteredSystem>();
    Assert::That(
        registry.GetSystem<SpecLateRegisteredSystem>().GetSystemEntities().size(),
        Equals(static_cast<std::size_t>(0)));

    const std::size_t admitted =
        registry.AdmitExistingEntities<SpecLateRegisteredSystem>();

    Assert::That(admitted, Equals(static_cast<std::size_t>(2)));
    const std::vector<Entity> &backfilled =
        registry.GetSystem<SpecLateRegisteredSystem>().GetSystemEntities();
    Assert::That(backfilled.size(), Equals(static_cast<std::size_t>(2)));

    // A count alone cannot tell a healthy back-filled entity from a poisoned
    // one: ForEachMissedEntity hands AdmitExistingEntitiesTo bare
    // Entity(id) candidates with a null `registry`, and every Entity
    // forwarder short-circuits a null registry to a shared, zeroed fallback
    // component instead of segfaulting. That would still pass a
    // size()-only assertion. Dereference a real component off each
    // back-filled entity and check its actual value: a poisoned entity
    // reads back value == 0 (the fallback), not the 11/22 that was really
    // stored.
    int total = 0;
    for (const Entity &entity : backfilled) {
      total += entity.GetComponent<SpecLateSystemMarker>().value;
    }
    Assert::That(total, Equals(11 + 22));

    Logger::messages.clear();
  };

  It(should_admit_nothing_for_a_system_that_was_never_registered) {
    Registry registry;
    Assert::That(registry.AdmitExistingEntities<SpecLateRegisteredSystem>(),
                 Equals(static_cast<std::size_t>(0)));
  };

  It(should_not_report_missed_entities_when_a_system_is_registered_twice) {
    Registry registry;
    registry.AddSystem<SpecLateRegisteredSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<SpecLateSystemMarker>();
    registry.Update();

    Logger::messages.clear();
    // Duplicate registration: unordered_map::insert no-ops, so this second
    // instance is discarded. Before the fix, the diagnostic ran
    // CountEntitiesMissedBySystem against that discarded instance (an empty
    // member list) instead of the real, already-populated one, and falsely
    // reported every matching entity as missed.
    registry.AddSystem<SpecLateRegisteredSystem>();

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
    Assert::That(
        registry.GetSystem<SpecLateRegisteredSystem>().GetSystemEntities().size(),
        Equals(static_cast<std::size_t>(1)));
  };
};

// The ~Registry throttle counter (missingUpdateReports, a file-scope static
// thread_local in common/ecs.cpp) is shared process-wide on whichever thread
// touches it and never resets — unlike the per-registry side table it used
// to (wrongly) live in. Plenty of other specs, in this file and elsewhere,
// create a Registry, create an entity, and destroy it without ever calling
// Update(); on the main thread that budget of ECS_MAX_DIAGNOSTIC_REPORTS (4)
// is normally spent well before this Describe runs. The two cases below
// exist specifically to exercise this diagnostic, so each does its
// create/destroy inside OnFreshDiagnosticBudget (specs/support/) — see that
// helper for why a fresh thread, not a reset seam, is the right shape here.
Describe(MissingUpdateSpec) {
  It(should_report_a_registry_destroyed_without_ever_flushing) {
    Logger::messages.clear();
    OnFreshDiagnosticBudget([] {
      Registry registry;
      (void)registry.CreateEntity();
      (void)registry.CreateEntity();
    }); // destroyed here, Update() never called

    Assert::That(SpecRegistryErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_stay_silent_when_update_was_called) {
    Logger::messages.clear();
    {
      Registry registry;
      (void)registry.CreateEntity();
      registry.Update();
      (void)registry.CreateEntity();
    }

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_stay_silent_for_a_registry_that_never_created_an_entity) {
    // A registry built and dropped without being used is not a mistake.
    Logger::messages.clear();
    { Registry registry; }

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_stay_silent_for_a_large_batch_flushed_once) {
    // The pattern the previous design got wrong: a level loader spawning a
    // burst and flushing once afterwards is correct, not a misuse.
    Logger::messages.clear();
    {
      Registry registry;
      for (int i = 0; i < 200; ++i) {
        (void)registry.CreateEntity();
      }
      registry.Update();
    }

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_not_inherit_diagnostic_state_from_a_destroyed_registry) {
    // The side table is keyed on `this`, and an allocator hands the same
    // address back readily — but not deterministically, so this places both
    // registries in the same raw storage via placement new rather than
    // relying on the heap to reuse a freed address. That controls the
    // address while still exercising the real ~Registry / CreateEntity code
    // paths. Without the erase in ~Registry, this second registry inherits
    // the first's updateCalls and the diagnostic silently stops working for
    // it. Deleting the erase(this) line MUST fail this case.
    //
    // Runs via OnFreshDiagnosticBudget (see the comment above
    // MissingUpdateSpec) so the shared missingUpdateReports throttle has its
    // own untouched budget, regardless of what the main thread has already
    // reported elsewhere in the suite. Assert::That stays on the main
    // thread: an uncaught assertion failure inside the spawned thread would
    // call std::terminate instead of failing the case.
    alignas(Registry) unsigned char storage[sizeof(Registry)];
    bool addressReused = false;

    OnFreshDiagnosticBudget([&] {
      Registry *first = new (storage) Registry();
      (void)first->CreateEntity();
      first->Update(); // marks this address as having flushed
      first->~Registry();

      Logger::messages.clear();

      Registry *second = new (storage) Registry();
      addressReused =
          static_cast<void *>(second) == static_cast<void *>(storage);
      (void)second->CreateEntity();
      second->~Registry(); // never flushed — must report, despite `first`
                           // having flushed
    });

    // Assert the address was actually reused, so the case cannot pass by
    // testing nothing.
    Assert::That(addressReused, Equals(true));
    Assert::That(SpecRegistryErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };
};

Describe(TryGetSystemSpec) {
  It(should_return_null_for_a_system_that_was_never_registered) {
    Registry registry;
    Assert::That(registry.TryGetSystem<SpecLateRegisteredSystem>() == nullptr,
                 Equals(true));
  };

  It(should_return_the_system_when_it_is_registered) {
    Registry registry;
    registry.AddSystem<SpecLateRegisteredSystem>();
    SpecLateRegisteredSystem *system =
        registry.TryGetSystem<SpecLateRegisteredSystem>();

    Assert::That(system == nullptr, Equals(false));
    Assert::That(system == &registry.GetSystem<SpecLateRegisteredSystem>(),
                 Equals(true));
  };

  It(should_log_before_get_system_throws) {
    Registry registry;
    Logger::messages.clear();

    bool threw = false;
    try {
      (void)registry.GetSystem<SpecLateRegisteredSystem>();
    } catch (const std::out_of_range &) {
      threw = true;
    }

    Assert::That(threw, Equals(true));
    Assert::That(SpecRegistryErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };
};

Describe(TryGetEntityByTagSpec) {
  It(should_return_null_for_a_tag_no_entity_holds) {
    Registry registry;
    Assert::That(registry.TryGetEntityByTag("player") == nullptr, Equals(true));
  };

  It(should_return_the_tagged_entity) {
    Registry registry;
    Entity player = registry.CreateEntity();
    player.Tag("player");
    registry.Update();

    const Entity *found = registry.TryGetEntityByTag("player");
    Assert::That(found == nullptr, Equals(false));
    Assert::That(found->GetId(), Equals(player.GetId()));
  };

  It(should_return_null_after_the_tagged_entity_is_killed) {
    Registry registry;
    Entity player = registry.CreateEntity();
    player.Tag("player");
    registry.Update();

    player.Kill();
    registry.Update();

    Assert::That(registry.TryGetEntityByTag("player") == nullptr,
                 Equals(true));
  };
};

Describe(StaleHandleContainerSpec) {
  It(should_not_find_a_live_entity_through_a_stale_handle_in_a_group) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Group("enemies");
    registry.Update();

    first.Kill();
    registry.Update();                       // id freed, generation bumped

    Entity second = registry.CreateEntity(); // same id, new generation
    second.Group("enemies");
    registry.Update();

    // The stale handle must not resolve to the live entity's group entry.
    Assert::That(registry.EntityBelongsToGroup(second, "enemies"),
                 Equals(true));
    Assert::That(registry.EntityBelongsToGroup(first, "enemies"),
                 Equals(false));
  };
};

Describe(TagCleanupSpec) {
  It(should_not_let_a_recycled_id_inherit_a_tag) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Tag("player");
    registry.Update();

    first.Kill();
    registry.Update();

    Entity second = registry.CreateEntity(); // same id
    Assert::That(registry.EntityHasTag(second, "player"), Equals(false));
    Assert::That(registry.DoesTagExist("player"), Equals(false));
  };
};
