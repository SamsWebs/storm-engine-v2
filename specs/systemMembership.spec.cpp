#include <igloo/igloo_alt.h>

#include "../common/ecs.h"

#include <string>
#include <type_traits>
#include <vector>

using namespace igloo;
using namespace storm;

// Component ids come off one process-global counter with a hard ceiling of
// MAX_COMPONENTS (KNOWN_ISSUES.md §3), so every new type declared in the suite
// is spent budget. Two are enough for everything below.
struct SpecMembershipPosition {
  int value = 0;
};

struct SpecMembershipVelocity {
  int value = 0;
};

class SpecMembershipMoverSystem : public System {
public:
  SpecMembershipMoverSystem() {
    RequireComponent<SpecMembershipPosition>();
    RequireComponent<SpecMembershipVelocity>();
  }
};

class SpecMembershipPositionSystem : public System {
public:
  SpecMembershipPositionSystem() { RequireComponent<SpecMembershipPosition>(); }
};

// System::sortEntities is protected, so a subclass is the only way to reach
// it — which is exactly how RenderSystem orders by z-index
// (common/systems/render.h:25).
class SpecSortableSystem : public System {
public:
  SpecSortableSystem() { RequireComponent<SpecMembershipPosition>(); }

  void SortByDescendingId() {
    sortEntities(
        [](const Entity &a, const Entity &b) { return b.GetId() < a.GetId(); });
  }
};

// Systems used only by the AddSystem argument-passing cases below.
struct SpecCountingSystem : public System {
  int seed = 0;
  explicit SpecCountingSystem(int seed) : seed(seed) {}
};

struct SpecNameCarryingSystem : public System {
  std::vector<std::string> names;
  explicit SpecNameCarryingSystem(std::vector<std::string> names)
      : names(std::move(names)) {}
};

Describe(SystemMembershipSpec) {

  //////////////////////////////////////////////////////////////////////////
  // KNOWN_ISSUES.md §5 — "Adding or removing a component never changes
  // system membership".
  //
  // THESE CASES PIN A KNOWN LIMITATION, NOT DESIRED BEHAVIOUR. Membership is
  // computed once per entity, when Registry::Update() flushes
  // entitiesToBeAdded (common/ecs.cpp:231-237). AddComponent and
  // RemoveComponent only flip signature bits; nothing re-evaluates which
  // systems the entity belongs to. Fixing that is an ECS redesign that
  // changes System's layout, so it is frozen out of the 1.x line.
  //
  // A 2.0.0 that fixes it MUST make these fail. Flip them deliberately then —
  // do not delete them, and do not "repair" them here.
  //////////////////////////////////////////////////////////////////////////
  Describe(ComponentChangesAfterAdmission) {

    It(should_not_admit_an_entity_when_a_component_is_added_after_the_flush) {
      Registry registry;
      registry.AddSystem<SpecMembershipMoverSystem>();

      Entity entity = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(entity);
      registry.Update(); // membership is decided here, and only here

      registry.AddComponent<SpecMembershipVelocity>(entity);
      registry.Update(); // no re-evaluation happens

      // The entity now satisfies the system's signature in every way that
      // matters...
      Assert::That(registry.HasComponent<SpecMembershipPosition>(entity),
                   Equals(true));
      Assert::That(registry.HasComponent<SpecMembershipVelocity>(entity),
                   Equals(true));

      // ...and the system still does not have it. 1u would be correct; 0u is
      // what the engine does.
      Assert::That(registry.GetSystem<SpecMembershipMoverSystem>()
                       .GetSystemEntities()
                       .size(),
                   Equals(0u));
    };

    It(should_keep_an_entity_in_a_system_after_its_component_is_removed) {
      Registry registry;
      registry.AddSystem<SpecMembershipPositionSystem>();

      Entity entity = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(entity);
      registry.Update();

      Assert::That(registry.GetSystem<SpecMembershipPositionSystem>()
                       .GetSystemEntities()
                       .size(),
                   Equals(1u));

      registry.RemoveComponent<SpecMembershipPosition>(entity);
      registry.Update();

      Assert::That(registry.HasComponent<SpecMembershipPosition>(entity),
                   Equals(false));

      // PINS A KNOWN LIMITATION: the system keeps iterating an entity whose
      // component is gone, and GetComponent there hands back the shared
      // fallback instead of real data. 0u would be correct.
      Assert::That(registry.GetSystem<SpecMembershipPositionSystem>()
                       .GetSystemEntities()
                       .size(),
                   Equals(1u));
    };

    // Same single-evaluation mechanism seen from the other side: a system
    // registered after the flush never sees the entities that already exist.
    It(should_not_backfill_a_system_added_after_the_flush) {
      Registry registry;

      Entity entity = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(entity);
      registry.Update();

      registry.AddSystem<SpecMembershipPositionSystem>();
      registry.Update();

      // PINS A KNOWN LIMITATION: 1u would be correct.
      Assert::That(registry.GetSystem<SpecMembershipPositionSystem>()
                       .GetSystemEntities()
                       .size(),
                   Equals(0u));
    };

    // The documented workaround (KNOWN_ISSUES.md §5, "Meanwhile"): add every
    // component the entity will ever need before the admitting Update.
    It(should_admit_an_entity_that_was_complete_before_the_flush) {
      Registry registry;
      registry.AddSystem<SpecMembershipMoverSystem>();

      Entity entity = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(entity);
      registry.AddComponent<SpecMembershipVelocity>(entity);
      registry.Update();

      auto &entities =
          registry.GetSystem<SpecMembershipMoverSystem>().GetSystemEntities();
      Assert::That(entities.size(), Equals(1u));
      Assert::That(entities[0].GetId(), Equals(entity.GetId()));
    };

    // The other documented workaround: kill the entity and create a
    // replacement carrying the full component set.
    It(should_admit_a_replacement_entity_created_with_every_component) {
      Registry registry;
      registry.AddSystem<SpecMembershipMoverSystem>();

      Entity original = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(original);
      registry.Update();

      original.Kill();
      registry.Update();

      Entity replacement = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(replacement);
      registry.AddComponent<SpecMembershipVelocity>(replacement);
      registry.Update();

      Assert::That(registry.GetSystem<SpecMembershipMoverSystem>()
                       .GetSystemEntities()
                       .size(),
                   Equals(1u));
    };
  };

  //////////////////////////////////////////////////////////////////////////
  // System::sortEntities — protected, reached by subclassing, and the hook
  // RenderSystem's z-ordering depends on. Nothing else in the suite calls it.
  //////////////////////////////////////////////////////////////////////////
  Describe(SortEntitiesHook) {

    It(should_reorder_the_system_entity_list_in_place) {
      Registry registry;
      registry.AddSystem<SpecSortableSystem>();

      Entity first = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(first);
      Entity second = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(second);
      Entity third = registry.CreateEntity();
      registry.AddComponent<SpecMembershipPosition>(third);

      registry.Update();

      auto &entities =
          registry.GetSystem<SpecSortableSystem>().GetSystemEntities();
      Assert::That(entities.size(), Equals(3u));
      Assert::That(entities[0].GetId(), Equals(first.GetId()));
      Assert::That(entities[2].GetId(), Equals(third.GetId()));

      registry.GetSystem<SpecSortableSystem>().SortByDescendingId();

      Assert::That(entities.size(), Equals(3u));
      Assert::That(entities[0].GetId(), Equals(third.GetId()));
      Assert::That(entities[1].GetId(), Equals(second.GetId()));
      Assert::That(entities[2].GetId(), Equals(first.GetId()));
    };

    It(should_tolerate_sorting_an_empty_system) {
      Registry registry;
      registry.AddSystem<SpecSortableSystem>();
      registry.Update();

      registry.GetSystem<SpecSortableSystem>().SortByDescendingId();

      Assert::That(
          registry.GetSystem<SpecSortableSystem>().GetSystemEntities().size(),
          Equals(0u));
    };
  };

  //////////////////////////////////////////////////////////////////////////
  // KNOWN_ISSUES.md §2 — Entity(std::size_t) is not explicit, so a bare
  // integer converts to an Entity at any call site that takes one.
  //
  // FIXED in 2.0.0: Entity's id constructor is now explicit, so the implicit
  // conversion this block used to pin (registry.KillEntity(88) compiling) is
  // now a compile error — pinned below as a static_assert at file scope
  // instead. What survives here is the rest of the original case: KillEntity
  // on an id that was never allocated is a no-op rather than corrupting
  // memory. It now constructs the id explicitly, as callers must.
  //////////////////////////////////////////////////////////////////////////
  Describe(ImplicitEntityConversion) {

    It(should_leave_a_live_entity_alone_when_killing_an_unallocated_id) {
      Registry registry;
      Entity real = registry.CreateEntity();
      registry.Update();

      // 88 was never allocated by this registry. KillEntity rejects an id
      // that is not alive (common/ecs.cpp:328), so this is a no-op rather
      // than corrupting memory.
      registry.KillEntity(Entity(88));
      registry.Update();

      Assert::That(registry.IsAlive(real), Equals(true));
      Assert::That(registry.IsAlive(Entity(88)), Equals(false));
    };
  };

  Describe(AddSystemArgumentPassingSpec) {
    It(should_accept_an_rvalue_constructor_argument) {
      Registry registry;
      registry.AddSystem<SpecCountingSystem>(5);
      Assert::That(registry.HasSystem<SpecCountingSystem>(), Equals(true));
      Assert::That(registry.GetSystem<SpecCountingSystem>().seed, Equals(5));
    };

    It(should_not_move_out_of_a_caller_lvalue) {
      Registry registry;
      std::vector<std::string> names{"first", "second"};
      registry.AddSystem<SpecNameCarryingSystem>(names);

      // The system got its own copy; the caller's vector is untouched.
      Assert::That(names.size(), Equals(static_cast<std::size_t>(2)));
      Assert::That(names[0], Equals("first"));
      Assert::That(registry.GetSystem<SpecNameCarryingSystem>().names.size(),
                   Equals(static_cast<std::size_t>(2)));
    };
  };
};

// KNOWN_ISSUES.md §2, fixed in 2.0.0: a bare integer must not implicitly
// convert to an Entity. registry.KillEntity(88) used to compile; the
// ImplicitEntityConversion case above now constructs Entity(88) explicitly
// instead.
static_assert(!std::is_convertible<std::size_t, Entity>::value,
              "a bare size_t must not implicitly convert to an Entity");
