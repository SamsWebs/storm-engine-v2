# Storm! Engine v2 1.4.0 Usage-Trap Guardrails Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make eleven known ways of misusing Storm! Engine v2 report themselves — at compile time or in the log — without breaking source or binary compatibility for games already built on 1.x.

**Architecture:** Extend the diagnostic convention already in `common/ecs.h` (`EcsShouldReport`, `EcsReportErr`, `EcsSuppressionNote`, `ECS_MAX_DIAGNOSTIC_REPORTS`) rather than add new machinery. Every check is derived from state `Registry` already holds, except one that lives in a file-static side table in `ecs.cpp` keyed on `this`. Nothing is added to any public type's layout.

**Tech Stack:** C++17, SDL2, igloo (`igloo/igloo_alt.h`) for specs, GNU make (`Makefile.debian`, `base.mk`).

## Global Constraints

- **No layout changes.** `sizeof(Registry)` (576), `sizeof(Entity)` (16), `sizeof(System)` (32), `sizeof(Signature)` (8) and `sizeof(Tile)` (80) must be unchanged at the end. No new data member on any public type. 1.4.0 must be a relink, not a rebuild.
- **No public member removed, no existing signature changed** — with the single sanctioned exception in Task 1.
- **No behaviour change to any existing call**, except Task 1's fix to argument forwarding.
- Every runtime diagnostic uses `EcsShouldReport` with a call-site-owned `static thread_local unsigned int` counter, and appends `EcsSuppressionNote(counter)`.
- **Gate the whole computation, not just the message.** Check `counter < ECS_MAX_DIAGNOSTIC_REPORTS` before doing any work a diagnostic needs, so an exhausted diagnostic costs one integer comparison.
- Diagnostics log at `Err` level. Specs assert against the process-global `Logger::messages`.
- **Do not add a false positive.** Every diagnostic spec asserts both that it fires for the misuse and that it stays silent for the legitimate neighbouring case. The silent-case assertion is the one that matters.
- Build and test with `make -f Makefile.debian test`. Warnings are `-Wall` with no `-Werror` (`base.mk:60`), so `[[deprecated]]` will not break the build.
- Engine types have no namespace. Do not introduce one.

---

## File Structure

**Modified:**
- `common/ecs.h` — `AddSystem` signature, `AddComponent` diagnostic hook, `GetSystem` diagnostic, new `TryGetSystem` / `AdmitExistingEntities` templates, `GetComponent` message. Declarations of the new non-template helpers.
- `common/ecs.cpp` — the non-template helper bodies, the diagnostics side table, `Update`/`CreateEntity`/`~Registry` hooks, `TryGetEntityByTag`.
- `common/systems/collision.h` — deprecation attribute.
- `common/systems/render.h` — `srcRect` bounds diagnostic.
- `KNOWN_ISSUES.md`, `CHANGELOG.md`, `README.md`, `TUTORIAL.md`.

**Created:**
- `common/input/keyboard.h` — edge-triggered keyboard state, header-only.
- `docs/UPGRADING.md`.
- `specs/input/keyboard.spec.cpp`.

**Spec files extended:** `specs/ecs.spec.cpp`, `specs/registry.spec.cpp`, `specs/systemMembership.spec.cpp`, `specs/systems/render.spec.cpp`, `specs/systems/collision.spec.cpp`.

The engine has no header dependency tracking. **Run `make -f Makefile.debian clean` before any build that follows an edit to `common/ecs.h`.**

---

### Task 1: `AddSystem` takes a forwarding reference

This is the sanctioned exception. It is first because Tasks 2 and 3 also edit `AddSystem`, and it fixes a live bug: under the current `Targs &...` signature, `Targs` deduces to a non-reference, so `std::forward<Targs>(args)...` in the body **moves out of the caller's lvalue**.

**Files:**
- Modify: `common/ecs.h` — declaration at line 335, definition at line 368
- Test: `specs/systemMembership.spec.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `template <typename TSystem, typename... Targs> void Registry::AddSystem(Targs &&... args)`

- [ ] **Step 1: Write the failing tests**

Append to `specs/systemMembership.spec.cpp`, inside the existing top-level `Describe`:

```cpp
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
```

Add `#include <string>` and `#include <vector>` to the file's includes if not already present.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`

Expected: `should_accept_an_rvalue_constructor_argument` fails to **compile** — `cannot bind non-const lvalue reference of type 'int&' to an rvalue of type 'int'`. Comment that case out, rebuild, and confirm `should_not_move_out_of_a_caller_lvalue` then fails at runtime with `names.size()` equal to 0. Uncomment before proceeding.

- [ ] **Step 3: Change the signature**

In `common/ecs.h`, the declaration (line 335):

```cpp
  template <typename TSystem, typename... Targs> void AddSystem(Targs &&... args);
```

and the definition (line 368):

```cpp
template <typename TSystem, typename... Targs>
void Registry::AddSystem(Targs &&... args) {
  std::shared_ptr<TSystem> newSystem =
      std::make_shared<TSystem>(std::forward<Targs>(args)...);
  systems.insert(std::make_pair(std::type_index(typeid(TSystem)), newSystem));
}
```

The body is unchanged; only the parameter pack's reference kind changes.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: both new cases PASS, and all 107 existing `AddSystem` call sites across `common/`, `editor/`, `examples/` and `specs/` still compile.

- [ ] **Step 5: Commit**

```bash
git add common/ecs.h specs/systemMembership.spec.cpp
git commit -m "Take AddSystem's constructor arguments by forwarding reference"
```

---

### Task 2: Report a component added after the entity was admitted

**Files:**
- Modify: `common/ecs.h` — new public method declaration on `Registry`, hook at the end of `AddComponent` (line 435-441)
- Modify: `common/ecs.cpp` — method body
- Test: `specs/ecs.spec.cpp`

**Interfaces:**
- Consumes: `Registry::IsAlive`, `System::GetComponentSignature`
- Produces: `const char *Registry::SystemMissedByLateComponent(Entity entity, std::size_t componentId) const` — returns the (mangled) type name of a registered system the entity would have joined had the component been added before admission, or `nullptr`. Public so specs can drive it directly.

- [ ] **Step 1: Write the failing tests**

Append to `specs/ecs.spec.cpp`. `SpecErrorCount()` already exists at line 49.

```cpp
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
```

`SpecMana` is the existing component used by the throttling case at `specs/ecs.spec.cpp:399`. If it is declared inside another `Describe`, hoist it to file scope alongside `SpecOverflowComponent` (line 40) rather than duplicating it.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: `should_report_a_component_added_after_the_entity_was_admitted` FAILS (0 errors logged). The three silence cases PASS already — they are the regression net, and they must keep passing after Step 3.

- [ ] **Step 3: Declare and implement the helper**

In `common/ecs.h`, in `Registry`'s public section next to `IsAlive` (around line 296):

```cpp
  // Names a registered system that `entity` would have joined had
  // `componentId` been present before Registry::Update() admitted it, or
  // nullptr when there is none. Call it *after* the signature bit is set.
  //
  // System membership is computed once, at admission, so a component added
  // afterwards never changes it (KNOWN_ISSUES.md item 5). This is how that
  // silent mistake is detected: replay the signature as the systems last saw
  // it and look for a requirement the new bit alone satisfies.
  //
  // The returned name comes from std::type_index::name() and is
  // implementation-mangled; it is for a log line, not for parsing.
  const char *SystemMissedByLateComponent(Entity entity,
                                          std::size_t componentId) const;
```

In `common/ecs.cpp`:

```cpp
const char *
Registry::SystemMissedByLateComponent(Entity entity,
                                      std::size_t componentId) const {
  const auto entityId = entity.GetId();
  if (entityId >= entityComponentSignatures.size() ||
      componentId >= MAX_COMPONENTS) {
    return nullptr;
  }
  if (!IsAlive(entity)) {
    return nullptr;
  }
  // Still queued: Update() has not decided its membership yet, so adding a
  // component now is exactly the correct thing to do.
  if (entitiesToBeAdded.find(entity) != entitiesToBeAdded.end()) {
    return nullptr;
  }
  // On its way out; its membership will never matter again.
  if (entitiesToBeKilled.find(entity) != entitiesToBeKilled.end()) {
    return nullptr;
  }

  const Signature &now = entityComponentSignatures[entityId];
  Signature asAdmitted = now;
  asAdmitted.reset(componentId);

  for (const auto &entry : systems) {
    const Signature &required = entry.second->GetComponentSignature();
    const bool matchedAtAdmission = (asAdmitted & required) == required;
    const bool matchesNow = (now & required) == required;
    if (!matchedAtAdmission && matchesNow) {
      return entry.first.name();
    }
  }
  return nullptr;
}
```

- [ ] **Step 4: Hook it into `AddComponent`**

In `common/ecs.h`, replace the closing `logger.Log(...)` of `AddComponent` (line 439-441) with:

```cpp
  logger.Log("Component id = " + std::to_string(componentId) +
             " was added to entity id " + std::to_string(entityId));

  // Gate on the budget before the search — an exhausted diagnostic must cost
  // one comparison, not a scan of every registered system.
  static thread_local unsigned int lateReports = 0;
  if (lateReports < ECS_MAX_DIAGNOSTIC_REPORTS) {
    if (const char *missed =
            SystemMissedByLateComponent(entity, componentId)) {
      if (EcsShouldReport(lateReports)) {
        logger.Err(
            "AddComponent: entity " + std::to_string(entityId) +
            " was already admitted by Registry::Update(), so adding this "
            "component will not put it in system '" + std::string(missed) +
            "'. Add every component before the Update() that admits the "
            "entity, or kill it and create a replacement." +
            EcsSuppressionNote(lateReports));
      }
    }
  }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all four new cases PASS, and every pre-existing case still passes. A failure in an unrelated ECS spec here means a false positive — fix the predicate, not the spec.

- [ ] **Step 6: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/ecs.spec.cpp
git commit -m "Report a component added after Registry::Update() froze membership"
```

---

### Task 3: Report a system registered after matching entities were admitted, and offer the repair

**Files:**
- Modify: `common/ecs.h` — `AddSystem` hook, new `AdmitExistingEntities` template, two helper declarations
- Modify: `common/ecs.cpp` — helper bodies
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Consumes: `Registry::SystemMissedByLateComponent` is *not* used here; this task uses `System::GetComponentSignature`, `System::AddEntityToSystem` (`common/ecs.h:167`), `Registry::IsAlive`
- Produces:
  - `std::size_t Registry::CountEntitiesMissedBySystem(const System &system) const`
  - `std::size_t Registry::AdmitExistingEntitiesTo(System &system)`
  - `template <typename TSystem> std::size_t Registry::AdmitExistingEntities()`

- [ ] **Step 1: Write the failing tests**

Append to `specs/registry.spec.cpp`:

```cpp
struct SpecLateSystemMarker { int value = 0; };

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
    Entity unrelated = registry.CreateEntity();
    registry.Update();

    Logger::messages.clear();
    registry.AddSystem<SpecLateRegisteredSystem>();

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_backfill_the_admitted_entities_on_request) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.AddComponent<SpecLateSystemMarker>();
    Entity second = registry.CreateEntity();
    second.AddComponent<SpecLateSystemMarker>();
    registry.Update();

    registry.AddSystem<SpecLateRegisteredSystem>();
    Assert::That(
        registry.GetSystem<SpecLateRegisteredSystem>().GetSystemEntities().size(),
        Equals(static_cast<std::size_t>(0)));

    const std::size_t admitted =
        registry.AdmitExistingEntities<SpecLateRegisteredSystem>();

    Assert::That(admitted, Equals(static_cast<std::size_t>(2)));
    Assert::That(
        registry.GetSystem<SpecLateRegisteredSystem>().GetSystemEntities().size(),
        Equals(static_cast<std::size_t>(2)));
    Logger::messages.clear();
  };

  It(should_admit_nothing_for_a_system_that_was_never_registered) {
    Registry registry;
    Assert::That(registry.AdmitExistingEntities<SpecLateRegisteredSystem>(),
                 Equals(static_cast<std::size_t>(0)));
  };
};
```

Add `#include "../common/logger.h"` to the file's includes if not already present.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: `should_report_when_matching_entities_were_already_admitted` FAILS, and the two `AdmitExistingEntities` cases fail to compile — `'class Registry' has no member named 'AdmitExistingEntities'`.

- [ ] **Step 3: Declare the helpers**

In `common/ecs.h`, in `Registry`'s public section beside `HasSystem` (line 339):

```cpp
  // How many live, already-admitted entities match `system`'s signature
  // without being members of it. Non-zero means the system was registered too
  // late to ever see them.
  std::size_t CountEntitiesMissedBySystem(const System &system) const;

  // Adds those entities to `system` and returns how many were added. Opt-in
  // and never automatic: a game may register a system late on purpose, and a
  // silent back-fill would change its behaviour.
  std::size_t AdmitExistingEntitiesTo(System &system);

  // AdmitExistingEntitiesTo for a system looked up by type. Returns 0 when
  // TSystem is not registered.
  template <typename TSystem> std::size_t AdmitExistingEntities();
```

And the template, next to the other out-of-class definitions (after `HasSystem`, around line 383):

```cpp
template <typename TSystem> std::size_t Registry::AdmitExistingEntities() {
  auto found = systems.find(std::type_index(typeid(TSystem)));
  if (found == systems.end()) {
    return 0;
  }
  return AdmitExistingEntitiesTo(*found->second);
}
```

- [ ] **Step 4: Implement the helpers**

In `common/ecs.cpp`:

```cpp
namespace {

// The entities a system registered now would have to be told about: live,
// past admission, matching the signature, not already members.
template <typename TVisitor>
void ForEachMissedEntity(const Registry &registry, std::size_t numEntities,
                         const std::vector<Signature> &signatures,
                         const System &system, TVisitor &&visit) {
  const Signature &required = system.GetComponentSignature();
  const std::vector<Entity> &members =
      const_cast<System &>(system).GetSystemEntities();

  for (std::size_t id = 0; id < numEntities && id < signatures.size(); ++id) {
    Entity entity(id);
    if (!registry.IsAlive(entity)) {
      continue;
    }
    if ((signatures[id] & required) != required) {
      continue;
    }
    if (std::find(members.begin(), members.end(), entity) != members.end()) {
      continue;
    }
    visit(entity);
  }
}

} // namespace
```

`Entity`'s `operator==` compares ids alone, which is exactly what is wanted here. `GetSystemEntities` has only a non-const overload, hence the cast; do not add a const overload, since that changes the public API.

The two members then read:

```cpp
std::size_t Registry::CountEntitiesMissedBySystem(const System &system) const {
  std::size_t missed = 0;
  ForEachMissedEntity(*this, numEntities, entityComponentSignatures, system,
                      [&missed](Entity) { ++missed; });
  return missed;
}

std::size_t Registry::AdmitExistingEntitiesTo(System &system) {
  std::vector<Entity> toAdmit;
  ForEachMissedEntity(*this, numEntities, entityComponentSignatures, system,
                      [&toAdmit](Entity entity) { toAdmit.push_back(entity); });
  for (Entity entity : toAdmit) {
    system.AddEntityToSystem(entity);
  }
  return toAdmit.size();
}
```

Collecting first and adding second keeps `AddEntityToSystem` from growing the vector `ForEachMissedEntity` is scanning.

Entities still queued in `entitiesToBeAdded` are deliberately not excluded here: `Registry::Update()` will admit them into every registered system, including this one, so they are not missed. They are also not yet in `members`, so excluding them would take an extra set lookup for no gain — but they *are* live and matching, so `CountEntitiesMissedBySystem` would count them. Add the exclusion to `ForEachMissedEntity` to keep the count honest:

```cpp
    if (registry.IsPendingAdmission(entity)) {
      continue;
    }
```

with, in `common/ecs.h` beside `IsAlive`:

```cpp
  // True while `entity` is queued for the next Registry::Update() and has not
  // yet been given to any system.
  bool IsPendingAdmission(Entity entity) const;
```

and in `common/ecs.cpp`:

```cpp
bool Registry::IsPendingAdmission(Entity entity) const {
  return entitiesToBeAdded.find(entity) != entitiesToBeAdded.end();
}
```

Use `IsPendingAdmission` in `SystemMissedByLateComponent` (Task 2) too, in place of the inline `entitiesToBeAdded.find`, so the condition exists once.

- [ ] **Step 5: Hook the diagnostic into `AddSystem`**

In `common/ecs.h`:

```cpp
template <typename TSystem, typename... Targs>
void Registry::AddSystem(Targs &&... args) {
  std::shared_ptr<TSystem> newSystem =
      std::make_shared<TSystem>(std::forward<Targs>(args)...);
  systems.insert(std::make_pair(std::type_index(typeid(TSystem)), newSystem));

  // TSystem's constructor has run its RequireComponent calls by now, so the
  // signature is final and the scan is meaningful.
  static thread_local unsigned int lateSystemReports = 0;
  if (lateSystemReports < ECS_MAX_DIAGNOSTIC_REPORTS) {
    const std::size_t missed = CountEntitiesMissedBySystem(*newSystem);
    if (missed > 0 && EcsShouldReport(lateSystemReports)) {
      logger.Err("AddSystem: '" + std::string(typeid(TSystem).name()) +
                 "' was registered after " + std::to_string(missed) +
                 " matching entities were already admitted; it will never see "
                 "them. Register every system before creating entities, or "
                 "call Registry::AdmitExistingEntities<T>()." +
                 EcsSuppressionNote(lateSystemReports));
    }
  }
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all five new cases PASS. Watch particularly that no pre-existing spec starts logging an error — several build a `Registry`, create entities, and register systems in various orders.

- [ ] **Step 7: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/registry.spec.cpp
git commit -m "Report a system registered after matching entities were admitted"
```

---

### Task 4: Report a `Registry` whose `Update()` is never called

**Files:**
- Modify: `common/ecs.cpp` — side table, `Update`, `CreateEntity`, `~Registry`
- Modify: `common/ecs.h` — the threshold constant, and move `~Registry` out of line
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Consumes: `EcsShouldReport`, `EcsReportErr`
- Produces: no public API. `ECS_PENDING_ENTITY_WARNING_THRESHOLD` is a new public constant.

- [ ] **Step 1: Write the failing tests**

Append to `specs/registry.spec.cpp`:

```cpp
Describe(MissingUpdateSpec) {
  It(should_report_a_registry_whose_update_was_never_called) {
    Registry registry;
    Logger::messages.clear();

    for (unsigned int i = 0; i < ECS_PENDING_ENTITY_WARNING_THRESHOLD + 1;
         ++i) {
      (void)registry.CreateEntity();
    }

    Assert::That(SpecRegistryErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_stay_silent_once_update_has_been_called) {
    Registry registry;
    registry.Update();
    Logger::messages.clear();

    for (unsigned int i = 0; i < ECS_PENDING_ENTITY_WARNING_THRESHOLD + 1;
         ++i) {
      (void)registry.CreateEntity();
    }

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_stay_silent_below_the_threshold) {
    Registry registry;
    Logger::messages.clear();

    for (unsigned int i = 0; i < ECS_PENDING_ENTITY_WARNING_THRESHOLD - 1;
         ++i) {
      (void)registry.CreateEntity();
    }

    Assert::That(SpecRegistryErrorCount(), Equals(static_cast<std::size_t>(0)));
  };
};
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: compile error — `'ECS_PENDING_ENTITY_WARNING_THRESHOLD' was not declared`.

- [ ] **Step 3: Add the constant and take the destructor out of line**

In `common/ecs.h`, beside `ECS_MAX_DIAGNOSTIC_REPORTS` (line 29):

```cpp
// A registry holding this many entities that have never been flushed has
// almost certainly never had Registry::Update() called on it — in which case
// no entity has joined any system and nothing renders. Well above any
// plausible single-frame spawn burst, so a game that flushes once a frame
// never reaches it.
constexpr unsigned int ECS_PENDING_ENTITY_WARNING_THRESHOLD = 64;
```

The destructor is currently defined inline at line 279:

```cpp
  ~Registry() { logger.Log("Registry destructor called."); }
```

Replace it with a declaration so it can clear the side table:

```cpp
  ~Registry();
```

This changes no layout and adds no virtual — `Registry` has no virtual functions and is not a base class anywhere in the tree. Confirm with `grep -rn "public Registry\|: Registry" common/ editor/ examples/` before proceeding; expect no hits.

- [ ] **Step 4: Implement the side table**

At the top of `common/ecs.cpp`, after the existing includes:

```cpp
namespace {

// Per-Registry diagnostic state that cannot live on Registry itself: games
// embed a Registry by value in their states, so sizeof(Registry) is ABI and
// 1.x may not add a member to it. Keyed on `this` and erased by ~Registry.
//
// Not thread-safe, in keeping with the rest of the ECS.
struct RegistryDiagnostics {
  unsigned long updateCalls = 0;
  unsigned int missingUpdateReports = 0;
};

std::unordered_map<const Registry *, RegistryDiagnostics> &
DiagnosticsTable() {
  static std::unordered_map<const Registry *, RegistryDiagnostics> table;
  return table;
}

} // namespace

Registry::~Registry() {
  DiagnosticsTable().erase(this);
  logger.Log("Registry destructor called.");
}
```

In `Registry::Update()` (line 231), as the first statement:

```cpp
  ++DiagnosticsTable()[this].updateCalls;
```

In `Registry::CreateEntity()` (line 126), after `entitiesToBeAdded.insert(entity);` (line 143):

```cpp
  RegistryDiagnostics &diagnostics = DiagnosticsTable()[this];
  if (diagnostics.updateCalls == 0 &&
      entitiesToBeAdded.size() >= ECS_PENDING_ENTITY_WARNING_THRESHOLD &&
      EcsShouldReport(diagnostics.missingUpdateReports)) {
    logger.Err(
        "CreateEntity: " + std::to_string(entitiesToBeAdded.size()) +
        " entities are waiting to be admitted and Registry::Update() has "
        "never been called on this registry. No entity has joined any system, "
        "so nothing will render or move. Call registry.Update() first in your "
        "state's update()." +
        EcsSuppressionNote(diagnostics.missingUpdateReports));
  }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all three new cases PASS.

- [ ] **Step 6: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/registry.spec.cpp
git commit -m "Report a registry whose Update() has never been called"
```

---

### Task 5: `TryGetSystem`, and a message before `GetSystem` throws

**Files:**
- Modify: `common/ecs.h` — declaration beside `HasSystem` (line 339), definition beside `GetSystem` (line 385)
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Produces: `template <typename TSystem> TSystem *Registry::TryGetSystem() const` — `nullptr` when absent. The pointer is owned by the registry and is invalidated by `RemoveSystem<TSystem>()` or the registry's destruction.

- [ ] **Step 1: Write the failing tests**

Append to `specs/registry.spec.cpp`:

```cpp
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
```

Add `#include <stdexcept>` to the file's includes.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: compile error — `'class Registry' has no member named 'TryGetSystem'`.

- [ ] **Step 3: Implement**

Declaration in `common/ecs.h` beside `HasSystem`:

```cpp
  // Prefer this over GetSystem wherever absence is possible: GetSystem calls
  // .at, which throws std::out_of_range — and aborts outright under the
  // Switch build's -fno-exceptions. The returned pointer is owned by the
  // registry and is invalidated by RemoveSystem<TSystem>().
  template <typename TSystem> TSystem *TryGetSystem() const;
```

Definitions:

```cpp
template <typename TSystem> TSystem *Registry::TryGetSystem() const {
  auto found = systems.find(std::type_index(typeid(TSystem)));
  if (found == systems.end()) {
    return nullptr;
  }
  return std::static_pointer_cast<TSystem>(found->second).get();
}

template <typename TSystem> TSystem &Registry::GetSystem() const {
  auto found = systems.find(std::type_index(typeid(TSystem)));
  if (found == systems.end()) {
    // .at is about to throw, and under -fno-exceptions that is an abort with
    // no message at all. Say which system first.
    static thread_local unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      logger.Err("GetSystem: system '" + std::string(typeid(TSystem).name()) +
                 "' was never registered; this call is about to throw. Use "
                 "TryGetSystem or HasSystem where absence is possible." +
                 EcsSuppressionNote(reports));
    }
  }
  // .at throws for a missing system — defined behavior instead of the UB of
  // dereferencing end(). Check HasSystem() first if absence is expected.
  return *(std::static_pointer_cast<TSystem>(
      systems.at(std::type_index(typeid(TSystem)))));
}
```

`GetSystem` is `const` and `logger` is `mutable` (line 270), so the `Err` call compiles.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all three PASS.

- [ ] **Step 5: Commit**

```bash
git add common/ecs.h specs/registry.spec.cpp
git commit -m "Add Registry::TryGetSystem and name the system before GetSystem throws"
```

---

### Task 6: `TryGetEntityByTag`

**Files:**
- Modify: `common/ecs.h` — declaration beside `GetEntityByTag` (line 353)
- Modify: `common/ecs.cpp` — body
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Produces: `const Entity *Registry::TryGetEntityByTag(const std::string &tag) const` — `nullptr` when the tag is unheld.

- [ ] **Step 1: Write the failing tests**

Append to `specs/registry.spec.cpp`:

```cpp
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
```

The third case pins existing behaviour: `Registry::Update` already calls `RemoveEntityTag` for killed entities (`common/ecs.cpp:244`) so a recycled id does not inherit a stale tag.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: compile error — `'class Registry' has no member named 'TryGetEntityByTag'`.

- [ ] **Step 3: Implement**

Declaration in `common/ecs.h` beside `GetEntityByTag`:

```cpp
  // The guarded lookup in one call. Returns nullptr when no entity holds the
  // tag, where GetEntityByTag has a precondition and throws.
  //
  // The pointer aliases the registry's tag map: it is invalidated by TagEntity,
  // RemoveEntityTag, and by the Update() that reaps a killed entity. Read it
  // and let it go; do not store it across a frame. Entity ids are recycled and
  // carry no generation counter (KNOWN_ISSUES.md item 1), so the same is true
  // of the Entity you copy out of it.
  const Entity *TryGetEntityByTag(const std::string &tag) const;
```

In `common/ecs.cpp`, beside `GetEntityByTag`:

```cpp
const Entity *Registry::TryGetEntityByTag(const std::string &tag) const {
  auto found = entityPerTag.find(tag);
  if (found == entityPerTag.end()) {
    return nullptr;
  }
  return &found->second;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all three PASS.

- [ ] **Step 5: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/registry.spec.cpp
git commit -m "Add Registry::TryGetEntityByTag for the guarded tag lookup"
```

---

### Task 7: Name the component type in the fallback diagnostic

**Files:**
- Modify: `common/ecs.h` — `Registry::GetComponent` (line 523-540)
- Test: `specs/ecs.spec.cpp`

**Interfaces:**
- Consumes: `ComponentMissDescription`
- Produces: no API change; message text only

- [ ] **Step 1: Write the failing test**

Append to `specs/ecs.spec.cpp`, inside the existing `Describe(EcsSpec)`:

```cpp
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
```

Add `#include <typeinfo>` to the file's includes.

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: FAIL — the current message names the entity and the reason, not the type.

- [ ] **Step 3: Extend the message**

In `common/ecs.h`, in the `logger.Err` call at line 537:

```cpp
    logger.Err("GetComponent: entity " + std::to_string(entity.GetId()) + " " +
               ComponentMissDescription(miss) + " for component type '" +
               typeid(TComponent).name() +
               "'; returning a shared fallback. Two misses alias each other — "
               "use TryGetComponent where a miss is possible." +
               EcsSuppressionNote(reports));
```

Keep the existing throttle counter and `EcsSuppressionNote` call exactly as they are; only the text changes. Read the surrounding lines before editing — the counter's name is whatever the current code uses.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: the new case PASSES and the existing throttling case at line 392 still passes.

- [ ] **Step 5: Commit**

```bash
git add common/ecs.h specs/ecs.spec.cpp
git commit -m "Name the component type in the GetComponent miss diagnostic"
```

---

### Task 8: Deprecate `CollisionSystem`

**Files:**
- Modify: `common/systems/collision.h`
- Modify: `specs/systems/collision.spec.cpp` — suppress the warning at the one place that must keep instantiating it
- Modify: `TUTORIAL.md` — the `CollisionSystem` row
- Test: `specs/systems/collision.spec.cpp`

**Interfaces:**
- Produces: no API change. `CollisionSystem` keeps its behaviour for the whole 1.x line.

- [ ] **Step 1: Add the attribute and the runtime notice**

In `common/systems/collision.h`:

```cpp
class [[deprecated(
    "CollisionSystem kills both entities on overlap and has no callback. Use "
    "ContactSystem (common/systems/contact.h) to observe collisions without "
    "acting on them.")]] CollisionSystem : public System {
public:
  CollisionSystem() {
    RequireComponent<TransformComponent>();
    RequireComponent<BoxColliderComponent>();

    static thread_local unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr(
          "CollisionSystem is registered. On overlap it calls Kill() on both "
          "entities that carry a RigidBodyComponent, and there is no collision "
          "callback. If you want to observe collisions, register ContactSystem "
          "instead." +
          std::string(EcsSuppressionNote(reports)));
    }
  }
```

Keep the rest of the class byte-identical. Confirm `collision.h` already includes `../ecs.h`; add it if not.

- [ ] **Step 2: Silence the warning in the spec**

`specs/systems/collision.spec.cpp` is the only place in the tree that instantiates the class, and it must keep doing so. Wrap the file's body:

```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// CollisionSystem is deprecated in 1.4.0 but its behaviour is frozen for the
// whole 1.x line, so it stays under spec until v3 deletes it.

// ... existing file contents ...

#pragma GCC diagnostic pop
```

- [ ] **Step 3: Add the notice case**

Append to `specs/systems/collision.spec.cpp`, inside the pragma-wrapped region:

```cpp
Describe(CollisionSystemDeprecationSpec) {
  It(should_log_a_notice_when_registered) {
    Registry registry;
    Logger::messages.clear();
    registry.AddSystem<CollisionSystem>();

    std::size_t errors = 0;
    for (const auto &entry : Logger::messages) {
      if (entry.type == LogType::LOG_ERROR) {
        ++errors;
      }
    }
    Assert::That(errors, Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };
};
```

This case is order-dependent on the diagnostic throttle: if another case in the same binary registers a `CollisionSystem` first, the counter may be exhausted. Register it here before any other `CollisionSystem` case in the file, or accept the case only asserting `>= 0` — prefer the ordering.

- [ ] **Step 4: Run the build and tests**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: PASS, and **no `-Wdeprecated-declarations` warning anywhere in the build output**. If one appears outside `collision.spec.cpp`, something else in the tree still registers `CollisionSystem` — find it and decide deliberately, do not blanket-suppress.

- [ ] **Step 5: Update the tutorial row**

In `TUTORIAL.md`, mark the `CollisionSystem` row deprecated and point at `ContactSystem`. Leave the row in place; the class still exists.

- [ ] **Step 6: Commit**

```bash
git add common/systems/collision.h specs/systems/collision.spec.cpp TUTORIAL.md
git commit -m "Deprecate CollisionSystem in favour of ContactSystem"
```

---

### Task 9: Report a `srcRect` that falls outside its texture

This is the task that catches the two silent-draw traps: a wrong `AnimationComponent::vertical` flag walks the frame offset off the wrong axis of the sheet, and `SpriteComponent` `width`/`height` that do not match the sheet cell push the rect past the texture edge. Both draw nothing today with no error.

**Files:**
- Modify: `common/systems/render.h` — `RenderSystem::Update`
- Test: `specs/systems/render.spec.cpp`

**Interfaces:**
- Consumes: `EcsShouldReport`, `EcsReportErr`, `EcsSuppressionNote` (via the existing `../ecs.h` include), `AssetStore::GetTexture`
- Produces: no API change

- [ ] **Step 1: Write the failing tests**

Append to `specs/systems/render.spec.cpp`. It already has `SpecSurfaceTarget`, `AddTintedTexture` and `SpecWhiteTexturePath()`; follow the declaration-order note at the top of the file — `SpecSurfaceTarget` before `AssetStore`.

```cpp
static std::size_t SpecRenderErrorCount() {
  std::size_t errors = 0;
  for (const auto &entry : Logger::messages) {
    if (entry.type == LogType::LOG_ERROR) {
      ++errors;
    }
  }
  return errors;
}

Describe(SrcRectBoundsSpec) {
  It(should_report_a_src_rect_past_the_bottom_of_the_texture) {
    SpecSurfaceTarget target;
    AssetStore assetStore;
    SDL_Texture *texture =
        AddTintedTexture(assetStore, target.renderer, "sheet", 255, 255, 255);
    Assert::That(texture == nullptr, Equals(false));

    int textureW = 0, textureH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);

    Registry registry;
    registry.AddSystem<RenderSystem>();
    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SpriteComponent>("sheet", textureW, textureH, 0, false,
                                         0, textureH * 4);
    registry.Update();

    Logger::messages.clear();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(SpecRenderErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_stay_silent_for_a_src_rect_inside_the_texture) {
    SpecSurfaceTarget target;
    AssetStore assetStore;
    SDL_Texture *texture =
        AddTintedTexture(assetStore, target.renderer, "sheet", 255, 255, 255);
    Assert::That(texture == nullptr, Equals(false));

    int textureW = 0, textureH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);

    Registry registry;
    registry.AddSystem<RenderSystem>();
    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SpriteComponent>("sheet", textureW, textureH, 0, false,
                                         0, 0);
    registry.Update();

    Logger::messages.clear();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(SpecRenderErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  It(should_throttle_the_report_for_a_permanently_broken_sprite) {
    SpecSurfaceTarget target;
    AssetStore assetStore;
    SDL_Texture *texture =
        AddTintedTexture(assetStore, target.renderer, "sheet", 255, 255, 255);
    Assert::That(texture == nullptr, Equals(false));

    int textureW = 0, textureH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);

    Registry registry;
    registry.AddSystem<RenderSystem>();
    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SpriteComponent>("sheet", textureW, textureH, 0, false,
                                         0, textureH * 4);
    registry.Update();

    Logger::messages.clear();
    for (int frame = 0; frame < 200; ++frame) {
      registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);
    }

    Assert::That(SpecRenderErrorCount(),
                 Is().LessThanOrEqualTo(
                     static_cast<std::size_t>(ECS_MAX_DIAGNOSTIC_REPORTS)));
    Logger::messages.clear();
  };
};
```

`SpriteComponent`'s constructor is `(assetId, width, height, zIndex, isFixed, srcRectX, srcRectY, offset)` — the seventh argument is `srcRectY`, which is what a wrong `vertical` flag drives.

The throttle case must run before the other two are able to exhaust the counter, or all three share one counter and the first case's report budget is spent. igloo runs cases in declaration order within a `Describe`, and the counter is a function-local static shared across all of them — so assert `>= 1` in the first case (already done), `== 0` in the silent case (unaffected by the throttle, since it never reports), and `<= ECS_MAX_DIAGNOSTIC_REPORTS` in the third (true regardless). All three assertions hold under any ordering.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: `should_report_a_src_rect_past_the_bottom_of_the_texture` FAILS with 0 errors logged. The other two PASS already and must keep passing.

- [ ] **Step 3: Add the bounds check**

In `common/systems/render.h`, inside the entity loop of `RenderSystem::Update`, replace the trailing `SDL_RenderCopyEx` call with:

```cpp
      SDL_Texture *texture = assetStore.GetTexture(sprite.assetId);

      // A srcRect outside the texture makes SDL_RenderCopyEx draw nothing and
      // report nothing. The two ways to arrive here are a SpriteComponent
      // width/height that does not match the sheet cell, and an
      // AnimationComponent vertical flag that does not match the sheet layout
      // — the frame offset then walks off the wrong axis. Gate on the budget
      // before the query so an exhausted diagnostic costs one comparison.
      static thread_local unsigned int srcRectReports = 0;
      if (texture != nullptr && srcRectReports < ECS_MAX_DIAGNOSTIC_REPORTS) {
        int textureW = 0;
        int textureH = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);
        const bool outside =
            srcRect.x < 0 || srcRect.y < 0 || srcRect.w <= 0 ||
            srcRect.h <= 0 || srcRect.x + srcRect.w > textureW ||
            srcRect.y + srcRect.h > textureH;
        if (outside && EcsShouldReport(srcRectReports)) {
          EcsReportErr(
              "RenderSystem: srcRect {" + std::to_string(srcRect.x) + "," +
              std::to_string(srcRect.y) + "," + std::to_string(srcRect.w) +
              "," + std::to_string(srcRect.h) + "} is outside texture '" +
              sprite.assetId + "' (" + std::to_string(textureW) + "x" +
              std::to_string(textureH) +
              ") — nothing will draw. Check that SpriteComponent width/height "
              "match the sheet cell, and that AnimationComponent.vertical "
              "matches the sheet layout." +
              EcsSuppressionNote(srcRectReports));
        }
      }

      SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect,
                       transform.rotation, NULL, sprite.flip);
```

Add `#include <string>` to the file's includes.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all three new cases PASS, every existing render case still passes, and **no existing spec starts logging a srcRect error**. If one does, that spec's fixture has a genuinely out-of-bounds rect and the finding is real — report it rather than loosening the check.

- [ ] **Step 5: Run the examples and check the log**

Run each example under `examples/` that builds on this machine and read the log for `RenderSystem: srcRect`. Any hit is a real, pre-existing, invisible rendering bug in that example. Note what you find in the commit message; fix it only if asked.

- [ ] **Step 6: Commit**

```bash
git add common/systems/render.h specs/systems/render.spec.cpp
git commit -m "Report a sprite srcRect that falls outside its texture"
```

---

### Task 10: `common/input/keyboard.h`

The trap is that the engine has no main loop and no keyboard abstraction, so games poll `SDL_PollEvent` in both `Game::ProcessInput` and the state, and the two drain a shared queue. This class deliberately **does not poll** — it is fed events by whoever owns the loop, so it cannot become a second consumer.

**Files:**
- Create: `common/input/keyboard.h`
- Create: `specs/input/keyboard.spec.cpp`
- Modify: `Makefile.debian` — add the new spec object if spec sources are listed explicitly rather than globbed (check `TESTOBJS` first)

**Interfaces:**
- Produces:
  - `void Keyboard::BeginFrame()`
  - `void Keyboard::HandleEvent(const SDL_Event &event)`
  - `bool Keyboard::IsDown(SDL_Scancode) const`
  - `bool Keyboard::WasPressed(SDL_Scancode) const`
  - `bool Keyboard::WasReleased(SDL_Scancode) const`

- [ ] **Step 1: Write the failing tests**

Create `specs/input/keyboard.spec.cpp`:

```cpp
#include "../../common/input/keyboard.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

namespace {

SDL_Event KeyEvent(Uint32 type, SDL_Scancode scancode, Uint8 repeat) {
  SDL_Event event{};
  event.type = type;
  event.key.type = type;
  event.key.repeat = repeat;
  event.key.keysym.scancode = scancode;
  return event;
}

} // namespace

Describe(KeyboardSpec) {
  It(should_report_a_key_as_down_after_a_keydown) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(true));
    Assert::That(keyboard.WasPressed(SDL_SCANCODE_SPACE), Equals(true));
  };

  It(should_clear_the_pressed_edge_on_the_next_frame) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    keyboard.BeginFrame();

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(true));
    Assert::That(keyboard.WasPressed(SDL_SCANCODE_SPACE), Equals(false));
  };

  It(should_report_the_released_edge_exactly_once) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE, 0));

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(false));
    Assert::That(keyboard.WasReleased(SDL_SCANCODE_SPACE), Equals(true));

    keyboard.BeginFrame();
    Assert::That(keyboard.WasReleased(SDL_SCANCODE_SPACE), Equals(false));
  };

  It(should_ignore_a_key_repeat_for_the_pressed_edge) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 0));

    keyboard.BeginFrame();
    keyboard.HandleEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE, 1));

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(true));
    Assert::That(keyboard.WasPressed(SDL_SCANCODE_SPACE), Equals(false));
  };

  It(should_ignore_an_event_that_is_not_a_key_event) {
    Keyboard keyboard;
    keyboard.BeginFrame();

    SDL_Event quit{};
    quit.type = SDL_QUIT;
    keyboard.HandleEvent(quit);

    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(false));
  };
};
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: compile error — `common/input/keyboard.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `common/input/keyboard.h`:

```cpp
#pragma once

#include <SDL2/SDL.h>
#include <bitset>

// Edge-triggered keyboard state.
//
// It does not poll. The engine owns no main loop, so the game decides where
// SDL_PollEvent is called — and calling it in two places drains a queue that
// both share, which is how input goes missing. Feed this class the events you
// already pull:
//
//     keyboard.BeginFrame();
//     SDL_Event event;
//     while (SDL_PollEvent(&event)) {
//       keyboard.HandleEvent(event);
//       // ... your other event handling ...
//     }
//     if (keyboard.WasPressed(SDL_SCANCODE_SPACE)) { Jump(); }
//
// Header-only and holds no SDL resource, so it is safe to construct before
// SDL_Init and to keep by value in a state.
class Keyboard {
public:
  // Clears the press and release edges. Call once per frame, before feeding
  // the frame's events.
  void BeginFrame() {
    pressed_.reset();
    released_.reset();
  }

  void HandleEvent(const SDL_Event &event) {
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
      return;
    }

    const SDL_Scancode scancode = event.key.keysym.scancode;
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) {
      return;
    }
    const std::size_t index = static_cast<std::size_t>(scancode);

    if (event.type == SDL_KEYDOWN) {
      // SDL sends auto-repeat as further KEYDOWNs. A repeat is not a new
      // press, and treating it as one makes held keys fire every frame.
      if (event.key.repeat == 0 && !down_.test(index)) {
        pressed_.set(index);
      }
      down_.set(index);
    } else {
      if (down_.test(index)) {
        released_.set(index);
      }
      down_.reset(index);
    }
  }

  // Held right now.
  bool IsDown(SDL_Scancode scancode) const { return Test(down_, scancode); }

  // Went down during this frame.
  bool WasPressed(SDL_Scancode scancode) const {
    return Test(pressed_, scancode);
  }

  // Came up during this frame.
  bool WasReleased(SDL_Scancode scancode) const {
    return Test(released_, scancode);
  }

private:
  using KeyBits = std::bitset<SDL_NUM_SCANCODES>;

  static bool Test(const KeyBits &bits, SDL_Scancode scancode) {
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) {
      return false;
    }
    return bits.test(static_cast<std::size_t>(scancode));
  }

  KeyBits down_;
  KeyBits pressed_;
  KeyBits released_;
};
```

`std::bitset::test` throws for an out-of-range position, which aborts under the Switch build's `-fno-exceptions` — hence the range guard in `Test` and in `HandleEvent` rather than relying on the enum's declared range.

- [ ] **Step 4: Make sure the spec is built**

Check how `Makefile.debian` collects spec sources (`TESTOBJS`, line 79 region). If they are globbed by directory, nothing to do — `specs/input/` is already covered. If they are listed, add `specs/input/keyboard.spec.o`.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all five PASS.

- [ ] **Step 6: Commit**

```bash
git add common/input/keyboard.h specs/input/keyboard.spec.cpp Makefile.debian
git commit -m "Add an edge-triggered Keyboard that is fed events rather than polling"
```

---

### Task 11: Documentation

**Files:**
- Create: `docs/UPGRADING.md`
- Modify: `CHANGELOG.md`, `KNOWN_ISSUES.md`, `README.md`

- [ ] **Step 1: Write `docs/UPGRADING.md`**

Cover, in this order:

1. **1.4.0 is a relink, not a rebuild.** No type changed size. State the measured sizes and how to confirm them.
2. **The one source-level change:** `AddSystem` now takes a forwarding reference. Every deduced call is unaffected; the explicit-template-argument form `AddSystem<Sys, int>(x)` with an lvalue `x` no longer compiles. Give both forms as code. Say that the change also fixes a silent move out of the caller's lvalue under the old signature — a game that passed a container or string to a system constructor and got an empty one back was hitting this.
3. **New diagnostics and what each means**, one short section each: late component, late system, missing `Update()`, `srcRect` outside texture, `GetSystem` about to throw, `GetComponent` miss. For each, say what to change. Note that all are throttled to `ECS_MAX_DIAGNOSTIC_REPORTS` (4) occurrences.
4. **New API:** `TryGetSystem`, `TryGetEntityByTag`, `AdmitExistingEntities`, `IsPendingAdmission`, `CountEntitiesMissedBySystem`, `common/input/keyboard.h`.
5. **`CollisionSystem` is deprecated.** Behaviour unchanged for the whole 1.x line. Migration to `ContactSystem`.
6. **Coming from 1.2.x:** the hop crosses 1.3.0, which **requires a full rebuild** — `sizeof(AssetStore)` went 112 → 208 and games allocate the store themselves, so a 1.2.x binary against a 1.3.0 library overflows its allocation with no warning. Install the package and rebuild; do not swap the `.so`; run `make clean`, since the engine has no header dependency tracking.

- [ ] **Step 2: Add the `CHANGELOG.md` entry**

A `## [1.4.0]` section with `### Added`, `### Changed` and `### Deprecated`, in the prose style of the existing 1.3.0 entry — each item says what the problem was, not just what changed. The `AddSystem` signature change goes under `### Changed` and must name the explicit-template-argument break outright.

- [ ] **Step 3: Update `KNOWN_ISSUES.md`**

Items 1, 4, 5, 7 and 10 now have a runtime diagnostic. Add one short paragraph to each saying so, in the shape of the existing "Resolved for new code in 1.3.0" notes. The defects themselves are unchanged and stay listed — "cannot be fixed in 1.x" and "cannot be detected in 1.x" are different claims, and only the second has changed.

Item 9 (no namespaces) explicitly gets nothing. Add a sentence saying a namespace alias was considered and rejected: the global names remain either way, so it would not prevent the collision it appears to address.

Also note under item 3 or in the preamble that `AssetStore_Ptr`, `Logger_Ptr` and the other `_Ptr` typedefs are `std::unique_ptr`, so they are move-only — hold the member, do not copy it.

- [ ] **Step 4: Update `README.md`**

Add `keyboard.h` to whatever list enumerates `common/input/`, and add a short "Diagnostics" section: the engine reports a fixed number of occurrences of each misuse at `Err` level, they are on in every build, and a game seeing one has a real bug.

- [ ] **Step 5: Verify the whole tree once more**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: builds with no new warnings, every spec passes. Record the actual pass count in the commit message.

- [ ] **Step 6: Commit**

```bash
git add docs/UPGRADING.md CHANGELOG.md KNOWN_ISSUES.md README.md
git commit -m "Document the 1.4.0 guardrails and the upgrade path from 1.2.x"
```

---

## Verification before calling this done

- [ ] `make -f Makefile.debian clean && make -f Makefile.debian test` — report the pass count printed, not a summary of it.
- [ ] `grep -rn "sizeof(Registry)\|sizeof(Entity)" specs/` — the layout assertions in the spec suite still pass. If none exist, add one to `specs/ecs.spec.cpp` pinning `sizeof(Registry) == 576`, `sizeof(Entity) == 16`, `sizeof(System) == 32`, `sizeof(Signature) == 8`, `sizeof(Tile) == 80` on x86-64, so the next layout change is argued rather than assumed.
- [ ] No `-Wdeprecated-declarations` warning outside `specs/systems/collision.spec.cpp`.
- [ ] Build and run each example under `examples/`; note any `RenderSystem: srcRect` or ECS diagnostic that fires. Each one is a real pre-existing bug in that example.
- [ ] `git log --oneline` shows one commit per task.
- [ ] Post a comment on https://github.com/SamsWebs/center-ice-hockey/issues/91 when 1.4.0 tags, with the release link and the `UPGRADING.md` path.
