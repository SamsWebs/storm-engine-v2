#include <igloo/igloo_alt.h>
#include <type_traits>
#include <typeinfo>

#include "../common/ecs.h"
#include "support/freshDiagnosticBudget.h"

using namespace igloo;

struct SpecHealth {
  SpecHealth(int v = 0) : value(v) {}
  int value;
};

struct SpecArmor {
  int value = 0;
};

struct SpecMana {
  int value = 0;
};

struct SpecStamina {
  int value = 0;
};

class SpecHealthSystem : public System {
public:
  SpecHealthSystem() { RequireComponent<SpecHealth>(); }
};

// Test seam for the MAX_COMPONENTS overflow cases. Component<T>::GetId() hands
// out ids off a process-global counter and caches each one in a function-local
// static, so a spec that simply declares 33 component types would push every
// type first used *after* it past MAX_COMPONENTS and break unrelated specs.
// Instead, borrow the counter, burn the ids inside one case, and put it back.
struct SpecComponentIdCounter : IComponent {
  static std::size_t Get() { return nextId; }
  static void Set(std::size_t value) { nextId = value; }
};

// Only ever used while the counter is parked past MAX_COMPONENTS.
struct SpecOverflowComponent {
  int value = 0;
};

class SpecOverflowSystem : public System {
public:
  SpecOverflowSystem() { RequireComponent<SpecOverflowComponent>(); }
};

// Test seam for the generation-wrap spec below. Reaching 2^32 kills for real
// is not something a test can do; this pre-seeds Registry's private
// generation table so the next CreateEntity for that id is stamped right up
// against the wrap, the same way SpecComponentIdCounter above borrows
// IComponent::nextId.
struct EcsGenerationTestSeam {
  static void SeedGeneration(Registry &registry, std::size_t id,
                             std::uint32_t generation) {
    if (id >= registry.generations.size()) {
      registry.generations.resize(id + 1, 1);
    }
    registry.generations[id] = generation;
  }
};

static std::size_t SpecErrorCount() {
  std::size_t errors = 0;
  for (const auto &entry : Logger::messages) {
    if (entry.type == LogType::LOG_ERROR) {
      ++errors;
    }
  }
  return errors;
}

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
      // Entity has no operator</operator> since 2.0.0: std::set found
      // elements through < alone, which let a stale handle (id-only equal)
      // alias a live entity's entry. Ordering, where genuinely needed, is
      // now explicit via EntityOrder.
    };
  };

  // P48 — the miss diagnostic's throttle counter is per-TComponent, static
  // thread_local, and never reset within a process. This case must run before
  // ComponentMissSpec exhausts the SpecMana/NoPool budget. Three tests share
  // one counter: this case (1 report), should_not_leak_a_miss_across_registries
  // (2 reports), and should_throttle_the_diagnostic_for_a_repeated_miss (needs
  // >= 1 remaining). SpecMana is reused deliberately rather than declaring a
  // fresh component type, because every type costs one of 32 process-wide ids.
  // If the budget exhausts first, nothing logs, named stays false, and the
  // assertion fails visibly — making order-position acceptable rather than
  // dangerous.
  It(should_name_the_component_type_in_the_miss_diagnostic) {
    Registry registry;
    Entity live = registry.CreateEntity();
    registry.Update();

    Logger::messages.clear();
    (void)registry.GetComponent<SpecMana>(live).value;

    bool named = false;
    for (const auto &entry : Logger::messages) {
      if (entry.type == LogType::LOG_ERROR &&
          entry.message.find(typeid(SpecMana).name()) != std::string::npos) {
        named = true;
      }
    }
    Assert::That(named, Equals(true));
    Logger::messages.clear();
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

  // P4 — GetComponent used to hand every miss the same shared static, so a
  // write through one miss was read back by an unrelated later one.
  Describe(ComponentMissSpec) {
    // Task 4b: this used to reach the pool via two hand-built, out-of-range
    // handles (Entity(999)/Entity(1000)). Since the staleness check now runs
    // ahead of OutOfRange, a hand-built id misses as Stale before ever
    // reaching the leak this case exists to catch — so both misses below are
    // now real, live entities that were simply never given SpecArmor
    // (a NotOwned miss), which still exercises the one property this case
    // cares about: a write through one miss must not be visible through a
    // different miss, nor through the live entity.
    It(should_not_leak_a_write_through_one_miss_into_another_miss) {
      Registry registry;
      Entity live = registry.CreateEntity();
      Entity firstMiss = registry.CreateEntity();  // never given SpecArmor
      Entity secondMiss = registry.CreateEntity(); // never given SpecArmor
      registry.Update();
      registry.AddComponent<SpecArmor>(live, SpecArmor{7});

      registry.GetComponent<SpecArmor>(firstMiss).value = 4242;

      // A different miss, and the live entity, must both be untouched.
      Assert::That(registry.GetComponent<SpecArmor>(secondMiss).value,
                   Equals(0));
      Assert::That(registry.GetComponent<SpecArmor>(live).value, Equals(7));
    };

    // Task 4b: this used to hit NoPool through a hand-built Entity(999) on
    // each registry, which is now Stale instead (never issued by
    // CreateEntity). Rebuilt with a real, live entity per registry that is
    // never given SpecMana at all — a genuine NoPool miss on each side —
    // so this still exercises what it is named for: the fallback static is
    // per component type per thread, not per Registry, and a write through
    // one registry's miss must not leak into another registry's.
    It(should_not_leak_a_miss_across_registries) {
      Registry first;
      Entity firstEntity = first.CreateEntity();
      first.Update();
      first.GetComponent<SpecMana>(firstEntity).value = 1234;

      Registry second;
      Entity secondEntity = second.CreateEntity();
      second.Update();
      Assert::That(second.GetComponent<SpecMana>(secondEntity).value,
                   Equals(0));
    };

    It(should_return_null_from_try_get_component_on_a_miss) {
      Registry registry;
      Entity live = registry.CreateEntity();
      registry.Update();

      // No pool for the type at all.
      Assert::That(registry.TryGetComponent<SpecStamina>(live) == nullptr,
                   Equals(true));

      registry.AddComponent<SpecStamina>(live, SpecStamina{3});

      // Present.
      SpecStamina *found = registry.TryGetComponent<SpecStamina>(live);
      Assert::That(found == nullptr, Equals(false));
      Assert::That(found->value, Equals(3));

      // Entity(150) here was never issued by CreateEntity, so it is now
      // rejected as Stale (Task 4b's staleness check runs ahead of
      // OutOfRange) rather than exercising the pool-bounds check this case
      // was originally written for. Genuine "real, live entity past the end
      // of a resized pool" OutOfRange coverage lives in
      // should_not_read_out_of_bounds_past_the_end_of_a_component_pool
      // below, which uses 151 real entities to get there.
      Assert::That(registry.TryGetComponent<SpecStamina>(Entity(150)) ==
                       nullptr,
                   Equals(true));

      // Live entity that simply does not have the type.
      Entity other = registry.CreateEntity();
      registry.Update();
      Assert::That(registry.TryGetComponent<SpecStamina>(other) == nullptr,
                   Equals(true));
    };

    It(should_not_read_out_of_bounds_past_the_end_of_a_component_pool) {
      // The pre-fix ASan repro: 151 entities, only entity 0 given the
      // component, then read entity 150 against a 100-slot pool.
      Registry registry;
      Entity first = registry.CreateEntity();
      registry.AddComponent<SpecHealth>(first, 7);
      Entity last = first;
      for (int i = 1; i < 151; ++i) {
        last = registry.CreateEntity();
      }
      registry.Update();

      Assert::That(last.GetId(), Equals(150u));
      Assert::That(registry.TryGetComponent<SpecHealth>(last) == nullptr,
                   Equals(true));
      Assert::That(registry.GetComponent<SpecHealth>(last).value, Equals(0));
      Assert::That(registry.GetComponent<SpecHealth>(first).value, Equals(7));
    };

    // P48 — the miss path must not do one flushed write plus a localtime()
    // call per entity per frame.
    It(should_throttle_the_diagnostic_for_a_repeated_miss) {
      Registry registry;
      Entity live = registry.CreateEntity();
      registry.Update();

      Logger::messages.clear();
      for (int i = 0; i < 200; ++i) {
        (void)registry.GetComponent<SpecMana>(live).value;
      }

      Assert::That(SpecErrorCount(),
                   Is().LessThanOrEqualTo(
                       static_cast<std::size_t>(ECS_MAX_DIAGNOSTIC_REPORTS)));
      Logger::messages.clear();
    };
  };

  // P12 — bitset::set/test throw past MAX_COMPONENTS, and these templates are
  // instantiated inside the game's -fno-exceptions translation unit.
  Describe(ComponentIdOverflowSpec) {
    It(should_reject_a_component_id_past_max_components) {
      unsigned int counter = 0;
      Assert::That(EcsComponentIdIsValid(0, "spec", counter), Equals(true));
      Assert::That(EcsComponentIdIsValid(MAX_COMPONENTS - 1, "spec", counter),
                   Equals(true));
      Assert::That(EcsComponentIdIsValid(MAX_COMPONENTS, "spec", counter),
                   Equals(false));
      Assert::That(
          EcsComponentIdIsValid(MAX_COMPONENTS + 1000, "spec", counter),
          Equals(false));
    };

    It(should_ignore_the_thirty_third_component_type_instead_of_throwing) {
      const std::size_t saved = SpecComponentIdCounter::Get();
      SpecComponentIdCounter::Set(MAX_COMPONENTS);

      // Caches an id of exactly MAX_COMPONENTS for the lifetime of the
      // process — SpecOverflowComponent is used nowhere else.
      Assert::That(Component<SpecOverflowComponent>::GetId(),
                   Equals(static_cast<std::size_t>(MAX_COMPONENTS)));

      Registry registry;
      registry.AddSystem<SpecOverflowSystem>(); // RequireComponent overflows
      Entity e = registry.CreateEntity();

      // None of these may throw; under -fno-exceptions a throw terminates.
      registry.AddComponent<SpecOverflowComponent>(e, SpecOverflowComponent{5});
      registry.Update();

      Assert::That(registry.HasComponent<SpecOverflowComponent>(e),
                   Equals(false));
      Assert::That(registry.TryGetComponent<SpecOverflowComponent>(e) ==
                       nullptr,
                   Equals(true));
      Assert::That(registry.GetComponent<SpecOverflowComponent>(e).value,
                   Equals(0));

      registry.RemoveComponent<SpecOverflowComponent>(e); // must be a no-op

      SpecComponentIdCounter::Set(saved);
    };

    // KNOWN_ISSUES.md §4 — PINS A KNOWN LIMITATION, NOT DESIRED BEHAVIOUR.
    // Was: "should match every entity when a system's requirement overflowed".
    // When RequireComponent<T>() overflows the cap the requirement cannot be
    // recorded, and membership is
    // (entitySignature & systemSignature) == systemSignature -- so a system
    // left holding an empty signature matched EVERY entity, and one that
    // should have seen nothing ran on the whole world. 2.0.0 latches such a
    // system off instead, so the failure direction is "matches nothing".
    It(should_match_no_entity_when_a_systems_requirement_overflowed) {
      // Order-independent: force SpecOverflowComponent's cached id to
      // MAX_COMPONENTS whether or not the case above has run yet.
      const std::size_t saved = SpecComponentIdCounter::Get();
      SpecComponentIdCounter::Set(MAX_COMPONENTS);
      Assert::That(Component<SpecOverflowComponent>::GetId(),
                   Equals(static_cast<std::size_t>(MAX_COMPONENTS)));
      SpecComponentIdCounter::Set(saved);

      Registry registry;
      registry.AddSystem<SpecOverflowSystem>();

      Assert::That(registry.GetSystem<SpecOverflowSystem>().IsDisabled(),
                   Equals(true));

      // A second, healthy system is the control. Without it, "the latched
      // system holds nothing" would also pass on a world that admitted
      // nothing at all -- the assertion has to distinguish "skipped" from
      // "there was never anything to skip".
      registry.AddSystem<SpecHealthSystem>();

      // Carries no component at all. Under the old behaviour it matched.
      Entity unrelated = registry.CreateEntity();

      // And one carrying a component the latched system never asked for.
      Entity carrier = registry.CreateEntity();
      registry.AddComponent<SpecHealth>(carrier, SpecHealth{7});

      registry.Update();

      Assert::That(registry.IsAlive(unrelated), Equals(true));

      Assert::That(registry.GetSystem<SpecOverflowSystem>()
                       .GetComponentSignature()
                       .none(),
                   Equals(true));
      Assert::That(
          registry.GetSystem<SpecOverflowSystem>().GetSystemEntities().size(),
          Equals(0u));

      // The control admitted normally, so admission ran and the latched
      // system was passed over specifically.
      Assert::That(
          registry.GetSystem<SpecHealthSystem>().GetSystemEntities().size(),
          Equals(1u));
      Assert::That(registry.GetSystem<SpecHealthSystem>()
                       .GetSystemEntities()[0]
                       .GetId(),
                   Equals(carrier.GetId()));
    };

    // The latch has to hold on the retrofit path too. AdmitExistingEntitiesTo
    // walks every live entity and admits the ones the system's signature
    // matches -- against an empty signature that is the entire world, handed
    // to the one system that must stay empty.
    It(should_not_admit_existing_entities_to_a_latched_system) {
      const std::size_t saved = SpecComponentIdCounter::Get();
      SpecComponentIdCounter::Set(MAX_COMPONENTS);
      Assert::That(Component<SpecOverflowComponent>::GetId(),
                   Equals(static_cast<std::size_t>(MAX_COMPONENTS)));
      SpecComponentIdCounter::Set(saved);

      Registry registry;
      Entity first = registry.CreateEntity();
      registry.AddComponent<SpecHealth>(first, SpecHealth{1});
      Entity second = registry.CreateEntity();
      registry.Update();

      // Both are live and already admitted, so the world genuinely holds
      // entities for the retrofit to find.
      Assert::That(registry.IsAlive(first), Equals(true));
      Assert::That(registry.IsAlive(second), Equals(true));

      registry.AddSystem<SpecOverflowSystem>();
      SpecOverflowSystem &system = registry.GetSystem<SpecOverflowSystem>();

      // A healthy system added just as late does find its entity, so the
      // retrofit path is working and the latched one is being skipped.
      registry.AddSystem<SpecHealthSystem>();
      SpecHealthSystem &control = registry.GetSystem<SpecHealthSystem>();
      Assert::That(registry.CountEntitiesMissedBySystem(control), Equals(1u));

      Assert::That(registry.CountEntitiesMissedBySystem(system), Equals(0u));

      registry.AdmitExistingEntitiesTo(system);

      Assert::That(system.GetSystemEntities().size(), Equals(0u));
    };

    // A system whose requirements all resolved is not latched -- the latch
    // must be reachable only through overflow.
    It(should_not_latch_a_system_whose_requirements_resolved) {
      Registry registry;
      registry.AddSystem<SpecHealthSystem>();

      Assert::That(registry.GetSystem<SpecHealthSystem>().IsDisabled(),
                   Equals(false));
    };
  };

  // P11 — a bare Entity has a null registry pointer; every forwarder used to
  // dereference it.
  Describe(BareEntitySpec) {
    It(should_treat_every_call_on_a_bare_entity_as_a_no_op) {
      Entity bare(88);
      Assert::That(bare.registry, Equals((Registry *)nullptr));

      // Each of these dereferenced the null registry pointer before the fix.
      bare.Kill();
      bare.Tag("player");
      bare.Group("enemies");
      bare.AddComponent<SpecHealth>(1);
      bare.RemoveComponent<SpecHealth>();

      Assert::That(bare.HasTag("player"), Equals(false));
      Assert::That(bare.BelongsToGroup("enemies"), Equals(false));
      Assert::That(bare.HasComponent<SpecHealth>(), Equals(false));
      Assert::That(bare.TryGetComponent<SpecHealth>() == nullptr, Equals(true));
      Assert::That(bare.GetComponent<SpecHealth>().value, Equals(0));
      Assert::That(bare.GetId(), Equals(88u));
    };
  };

  // P5 — the guards used to catch a double kill, but not a stale handle
  // whose id had already been recycled.
  Describe(RecycledIdStaleKillSpec) {
    It(should_reject_a_kill_through_a_recycled_stale_handle) {
      // FIXED, was pinned deliberately: closing this needed a generation
      // counter inside Entity, which changed sizeof(Entity) — an ABI break,
      // tracked as P5 in docs/TECH_DEBT.md. Before the fix, `doomed`'s stale
      // kill landed on `recycled` because the three KillEntity guards all
      // passed for a stale handle whose id had already been recycled; now
      // IsAlive checks the generation and the stale kill is rejected.
      //
      // KillEntity's "not alive" diagnostic is throttled by a counter shared
      // with every other spec that exercises this exact rejection on the
      // main thread — see OnFreshDiagnosticBudget (specs/support/), which
      // runs the stale kill below on a fresh thread so it gets its own,
      // untouched budget.
      Registry registry;
      registry.AddSystem<SpecHealthSystem>();

      Entity keeper = registry.CreateEntity(); // id 0
      Entity doomed = registry.CreateEntity(); // id 1
      registry.Update();

      registry.KillEntity(doomed);
      registry.Update(); // id 1 goes on the free list

      Entity recycled = registry.CreateEntity(); // takes id 1 back, new generation
      registry.AddComponent<SpecHealth>(recycled, 42);
      registry.Update();
      Assert::That(recycled.GetId(), Equals(doomed.GetId()));
      Assert::That(
          registry.GetSystem<SpecHealthSystem>().GetSystemEntities().size(),
          Equals(1u));

      Logger::messages.clear();
      OnFreshDiagnosticBudget([&] {
        registry.KillEntity(doomed); // stale handle, id already recycled
      });
      registry.Update();

      // The stale kill must not touch the new, live entity.
      Assert::That(
          registry.GetSystem<SpecHealthSystem>().GetSystemEntities().size(),
          Equals(1u));
      Assert::That(registry.IsAlive(recycled), Equals(true));
      Assert::That(registry.IsAlive(doomed), Equals(false));
      // Not just alive — intact. A kill that half-succeeded would still
      // report alive.
      Assert::That(registry.GetComponent<SpecHealth>(recycled).value,
                   Equals(42));
      Assert::That(SpecErrorCount(),
                   Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
      Logger::messages.clear();
      (void)keeper;
    };

    It(should_report_liveness_for_created_and_killed_entities) {
      Registry registry;
      Assert::That(registry.IsAlive(Entity(0)), Equals(false));

      Entity e = registry.CreateEntity();
      Assert::That(registry.IsAlive(e), Equals(true));

      registry.KillEntity(e);
      Assert::That(registry.IsAlive(e), Equals(true)); // not flushed yet

      registry.Update();
      Assert::That(registry.IsAlive(e), Equals(false));
      Assert::That(registry.IsAlive(Entity(4242)), Equals(false));
    };
  };
};
struct SpecLateA { int value = 0; };
struct SpecLateB { int value = 0; };

class SpecLateSystem : public System {
public:
  SpecLateSystem() {
    RequireComponent<SpecLateA>();
    RequireComponent<SpecLateB>();
  }
};

Describe(LateComponentSpec) {
  It(should_report_a_component_added_after_the_entity_was_admitted) {
    Registry registry;
    registry.AddSystem<SpecLateSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<SpecLateA>();
    registry.Update(); // membership decided here, without SpecLateB

    Logger::messages.clear();
    entity.AddComponent<SpecLateB>();

    Assert::That(SpecErrorCount(), Is().GreaterThanOrEqualTo(
                                       static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_stay_silent_when_the_component_is_added_before_admission) {
    Registry registry;
    registry.AddSystem<SpecLateSystem>();

    Entity entity = registry.CreateEntity();
    Logger::messages.clear();
    entity.AddComponent<SpecLateA>();
    entity.AddComponent<SpecLateB>();
    registry.Update();

    Assert::That(SpecErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_stay_silent_when_the_late_component_changes_no_membership) {
    Registry registry;
    registry.AddSystem<SpecLateSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<SpecLateA>();
    registry.Update();

    Logger::messages.clear();
    // SpecMana is required by no registered system, so adding it late costs
    // the entity nothing and must not be reported.
    entity.AddComponent<SpecMana>();

    Assert::That(SpecErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_report_nothing_for_an_entity_that_already_belongs_to_the_system) {
    Registry registry;
    registry.AddSystem<SpecLateSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<SpecLateA>();
    entity.AddComponent<SpecLateB>();
    registry.Update(); // already a member

    Logger::messages.clear();
    entity.AddComponent<SpecLateB>(); // re-adding changes no membership

    Assert::That(SpecErrorCount(), Equals(static_cast<std::size_t>(0)));
  };
};

// KNOWN_ISSUES.md item 2, fixed in 2.0.0: a bare integer must not become an
// Entity. Direct initialisation — Entity(7) — stays legal and is how the
// Registry builds them; only the implicit conversion goes away.
static_assert(!std::is_convertible<std::size_t, Entity>::value,
              "a bare size_t must not implicitly convert to an Entity");
static_assert(std::is_constructible<Entity, std::size_t>::value,
              "Entity must still be constructible from an id");

Describe(GenerationSpec) {
  It(should_not_report_a_stale_handle_as_alive) {
    Registry registry;
    Entity first = registry.CreateEntity();
    const std::size_t reusedId = first.GetId();
    first.Kill();
    registry.Update();                       // id returns to the free list

    Entity second = registry.CreateEntity(); // same id, new generation
    Assert::That(second.GetId(), Equals(reusedId));

    Assert::That(registry.IsAlive(second), Equals(true));
    Assert::That(registry.IsAlive(first), Equals(false));
  };

  It(should_not_compare_a_stale_handle_equal_to_the_live_entity) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Kill();
    registry.Update();
    Entity second = registry.CreateEntity();

    Assert::That(first == second, Equals(false));
    Assert::That(first != second, Equals(true));
  };

  It(should_treat_a_hand_built_entity_as_stale) {
    // Generation 0 is reserved: generations start at 1, so an Entity built
    // from a bare id can never match a live one.
    Registry registry;
    Entity live = registry.CreateEntity();
    registry.Update();

    Entity fabricated(live.GetId());
    Assert::That(registry.IsAlive(fabricated), Equals(false));
    Assert::That(fabricated == live, Equals(false));
  };

  // Task 4b — IsAlive alone is not enough: the component pools are indexed
  // by id, so without this check a stale handle whose id was recycled reads
  // the new, live entity's data as if it were its own.
  It(should_not_read_the_live_entitys_components_through_a_stale_handle) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Kill();
    registry.Update();

    Entity second = registry.CreateEntity(); // same id, new generation
    registry.AddComponent<SpecMana>(second, SpecMana{77});
    registry.Update();

    Assert::That(registry.HasComponent<SpecMana>(second), Equals(true));
    Assert::That(registry.HasComponent<SpecMana>(first), Equals(false));

    Logger::messages.clear();
    Assert::That(registry.TryGetComponent<SpecMana>(first) == nullptr,
                 Equals(true));
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  // Every read (FindComponent, and so HasComponent/TryGetComponent/
  // GetComponent) already gates on IsAlive. AddComponent and RemoveComponent
  // did not: a stale handle whose id had been recycled wrote straight into
  // the live occupant's pool slot and flipped its signature bit, silently.
  It(should_not_write_the_live_entitys_component_through_a_stale_add) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Kill();
    registry.Update();

    Entity second = registry.CreateEntity(); // same id, new generation
    registry.AddComponent<SpecMana>(second, SpecMana{42});
    registry.Update();

    Logger::messages.clear();
    // Stale: `first`'s generation no longer matches. Pre-fix this overwrote
    // `second`'s SpecMana in place.
    registry.AddComponent<SpecMana>(first, SpecMana{999});

    Assert::That(registry.GetComponent<SpecMana>(second).value, Equals(42));
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_not_remove_the_live_entitys_component_through_a_stale_remove) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Kill();
    registry.Update();

    Entity second = registry.CreateEntity(); // same id, new generation
    registry.AddComponent<SpecMana>(second, SpecMana{42});
    registry.Update();

    Logger::messages.clear();
    // Stale: pre-fix this cleared `second`'s SpecMana signature bit in place.
    registry.RemoveComponent<SpecMana>(first);

    Assert::That(registry.HasComponent<SpecMana>(second), Equals(true));
    Assert::That(registry.GetComponent<SpecMana>(second).value, Equals(42));
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  // KNOWN_ISSUES item 1: the increment that bumps a killed id's generation on
  // its way back to the free list must skip 0, the reserved never-valid
  // value. Without a clamp, an id killed while its generation sits at
  // 0xFFFFFFFF wraps the *next* recycle onto generation 0 — the exact value a
  // hand-built Entity(id) carries by default — so that fabricated handle
  // starts comparing equal to whatever live entity next takes the id.
  It(should_skip_generation_zero_on_wrap_at_uint32_max) {
    Registry registry;

    // First CreateEntity() with an empty registry always takes id 0.
    EcsGenerationTestSeam::SeedGeneration(registry, 0, 0xFFFFFFFFu);

    Entity first = registry.CreateEntity();
    Assert::That(first.GetId(), Equals(static_cast<std::size_t>(0)));
    Assert::That(first.GetGeneration(), Equals(0xFFFFFFFFu));
    registry.Update();

    first.Kill();
    registry.Update(); // ++generations[0] would wrap 0xFFFFFFFF -> 0

    Entity recycled = registry.CreateEntity(); // takes id 0 back
    registry.Update();

    Assert::That(recycled.GetId(), Equals(static_cast<std::size_t>(0)));
    // Must be 1 (skipping the reserved 0), not 0.
    Assert::That(recycled.GetGeneration(), Equals(1u));

    // A hand-built handle for this id still carries generation 0 by default,
    // and must stay stale — not compare equal to the live, recycled entity.
    Entity fabricated(0);
    Assert::That(registry.IsAlive(fabricated), Equals(false));
    Assert::That(fabricated == recycled, Equals(false));

    // The original stale handle (generation 0xFFFFFFFF) must also stay dead.
    Assert::That(registry.IsAlive(first), Equals(false));
    Assert::That(first == recycled, Equals(false));
  };
};

// TagEntity, GroupEntity and AddEntityToSystems used to be the three ECS
// mutators AddComponent/RemoveComponent's IsAlive gate did not cover. Each
// case here recycles an id onto a live entity, then drives the stale handle
// through the mutator and asserts the *live* entity's state is exactly what
// it was before — not merely that the call didn't crash.
Describe(StaleHandleMutationSpec) {
  It(should_not_steal_the_live_entitys_tag_through_a_stale_tag) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Kill();
    registry.Update(); // id freed, `first` is now stale

    Entity second = registry.CreateEntity(); // same id, new generation
    registry.TagEntity(second, "player");

    Logger::messages.clear();
    // Stale: `first`'s generation no longer matches `second`'s. Pre-fix,
    // TagEntity's entityPerTag.insert_or_assign overwrote "player" with the
    // stale handle regardless, so the live entity silently lost its tag.
    registry.TagEntity(first, "player");

    Assert::That(registry.EntityHasTag(second, "player"), Equals(true));
    Assert::That(registry.DoesTagExist("player"), Equals(true));
    Assert::That(registry.GetEntityByTag("player") == second, Equals(true));
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_not_duplicate_group_membership_through_a_stale_group) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Kill();
    registry.Update(); // id freed, `first` is now stale

    Entity second = registry.CreateEntity(); // same id, new generation
    registry.GroupEntity(second, "enemies");

    Logger::messages.clear();
    // Stale: pre-fix this emplaced `first` into the group's
    // set<Entity, EntityOrder> alongside `second` — keyed on (id,
    // generation), so it does not dedupe against the live occupant that
    // already holds the id, and a dead handle was handed to the game.
    registry.GroupEntity(first, "enemies");

    std::vector<Entity> members = registry.GetEntitiesByGroup("enemies");
    Assert::That(members.size(), Equals(static_cast<std::size_t>(1)));
    Assert::That(members[0] == second, Equals(true));
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_not_add_a_stale_entity_to_systems) {
    Registry registry;
    registry.AddSystem<SpecHealthSystem>();

    Entity first = registry.CreateEntity();
    registry.AddComponent<SpecHealth>(first, 1);
    registry.Update(); // admits `first` into SpecHealthSystem
    first.Kill();
    registry.Update(); // removes `first`, frees its id

    Entity second = registry.CreateEntity(); // same id, new generation
    registry.AddComponent<SpecHealth>(second, 2);
    registry.Update(); // admits `second` into SpecHealthSystem

    Logger::messages.clear();
    // Stale: pre-fix this push_backed `first` into the system's entity
    // vector unconditionally, permanently — removal only happens for
    // entities in entitiesToBeKilled, and a stale handle is never queued
    // there, so RenderSystem/ContactSystem/etc. would iterate it forever.
    registry.AddEntityToSystems(first);

    auto &system = registry.GetSystem<SpecHealthSystem>();
    Assert::That(system.GetSystemEntities().size(),
                 Equals(static_cast<std::size_t>(1)));
    Assert::That(system.GetSystemEntities()[0] == second, Equals(true));
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };
};
