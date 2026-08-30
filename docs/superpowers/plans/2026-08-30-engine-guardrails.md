# Storm! Engine v2 2.0.0 Usage-Trap Guardrails Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make eleven known ways of misusing Storm! Engine v2 report themselves — at compile time or in the log — and take the four source-level breaks that let three of them be fixed outright rather than merely reported.

**Why 2.0.0 and not 2.0.0.** `KNOWN_ISSUES.md` states the 1.x promise plainly: a game that compiles and links against 1.2.x must keep compiling and linking against every later 1.x release, which rules out changing a public signature or deleting a public member. This release does both. Shipping it as a 1.x point release would make that promise decorative, so the release is 2.0.0 and the compatibility promise resets here.

**What is NOT in this release.** Every fix that requires a *layout* change stays out: the `Entity` generation counter, `Tile`'s animation fields, `MAX_COMPONENTS`, and `System`'s disabled latch. Those remain 3.0 items. The four breaks taken here are all compile-time, all loud, and all have zero in-tree call sites — a game either builds or tells you exactly where it does not. A silent ABI break is a different animal and none is taken.

**Architecture:** Extend the diagnostic convention already in `common/ecs.h` (`EcsShouldReport`, `EcsReportErr`, `EcsSuppressionNote`, `ECS_MAX_DIAGNOSTIC_REPORTS`) rather than add new machinery. Every check is derived from state `Registry` already holds; where per-instance diagnostic state is genuinely needed it lives in a file-static side table keyed on `this`, in `ecs.cpp` and again in `netServer.cpp`. Nothing is added to any public type's layout.

**Tech Stack:** C++17, SDL2, igloo (`igloo/igloo_alt.h`) for specs, GNU make (`Makefile.debian`, `base.mk`).

## Global Constraints

- **No layout changes, still.** `sizeof(Registry)` (576), `sizeof(Entity)` (16), `sizeof(System)` (32), `sizeof(Signature)` (8), `sizeof(Tile)` (80) and `sizeof(NetServer)` must be unchanged at the end. No new data member on any public type. Games allocate `Registry`, `AssetStore` and `NetServer` themselves, so a size change is emitted at their call site and overflows their allocation with no warning — that is the 1.3.0 `AssetStore` hazard, and this release does not repeat it. Where per-instance state is needed, use a file-static side table keyed on `this`, erased in the destructor.
- **Four source-level breaks are sanctioned, and only these four.** Each is compile-time, loud, and has zero in-tree call sites:
  1. `Registry::AddSystem` takes `Targs &&...` instead of `Targs &...` (Task 1). Breaks only the explicit-template-argument form `AddSystem<Sys, int>(x)` with an lvalue.
  2. `CollisionSystem` is deleted outright (Task 8).
  3. `NetServer`, `NetClient`, `NetConnection` and `NetSocket` get deleted copy operations (Task 15).
  4. `Entity(std::size_t)` becomes `explicit` (Task 16).
- **Anything not on that list keeps working.** No other public member is removed, no other signature changes, and no existing call changes behaviour — except Task 1's fix to argument forwarding, which stops `AddSystem` moving out of the caller's lvalue.
- Every runtime diagnostic uses `EcsShouldReport` with a call-site-owned `static thread_local unsigned int` counter, and appends `EcsSuppressionNote(counter)`.
- **Gate the whole computation, not just the message.** Check `counter < ECS_MAX_DIAGNOSTIC_REPORTS` before doing any work a diagnostic needs, so an exhausted diagnostic costs one integer comparison.
- Diagnostics log at `Err` level. Specs assert against the process-global `Logger::messages`.
- **Do not add a false positive.** Every diagnostic spec asserts both that it fires for the misuse and that it stays silent for the legitimate neighbouring case. The silent-case assertion is the one that matters.
- Build and test with `make -f Makefile.debian test`. Warnings are `-Wall` with no `-Werror` (`base.mk:60`).
- The engine has **no header dependency tracking**. Run `make -f Makefile.debian clean` before any build that follows a header edit.
- Engine types have no namespace. Do not introduce one — that is a 3.0 item and it would break every line of every consuming game.

## Execution order

Tasks do not run in numeric order. Run the engine tasks first, then the examples last, so Task 12 exercises the finished engine:

**1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 13, 14, 9, 10, 11, then 12.**

Task 12 must be last: it runs every example against the completed engine and fixes what the diagnostics find. Task 11 (documentation) is second-to-last so it can describe what actually shipped.

---

## File Structure

**Modified:**
- `common/ecs.h` — `AddSystem` signature, `AddComponent` diagnostic hook, `GetSystem` diagnostic, new `TryGetSystem` / `AdmitExistingEntities` templates, `GetComponent` message. Declarations of the new non-template helpers.
- `common/ecs.cpp` — the non-template helper bodies, the diagnostics side table, `Update`/`CreateEntity`/`~Registry` hooks, `TryGetEntityByTag`.
- `common/systems/collision.h` — **deleted**, along with `specs/systems/collision.spec.cpp`.
- `common/net/netServer.h`, `netClient.h`, `netConnection.h`, `netSocket.h` — deleted copy operations; `netServer.h`/`.cpp` also gain the per-address cap and the client-id iterator.
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

**Design revised after review.** The original design counted pending entities and reported once the count crossed a threshold inside `CreateEntity`. That was wrong: batch-spawning many entities and flushing once afterwards is what a level loader does, and the engine's own `specs/ecs.spec.cpp` creates 151 entities before a single `Update()`. Entity count is not a signal of misuse at all.

The revised trigger is the destructor. A registry that reaches the end of its life having **never** flushed is unambiguously misused — no entity ever joined a system, so nothing it owned ever rendered or moved. There is no legitimate program with that shape, so the diagnostic cannot false-positive. It fires at state exit rather than at the moment of the mistake, which is later than ideal but still puts an explanation in the log under the broken screen.

**Files:**
- Modify: `common/ecs.cpp` — side table, `Update`, `~Registry`
- Modify: `common/ecs.h` — move `~Registry` out of line
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Consumes: `EcsShouldReport`, `EcsReportErr`
- Produces: no public API.

- [ ] **Step 1: The side table must outlive every `Registry`**

`Registry::instance` (`common/ecs.cpp:4`) is a namespace-scope `std::unique_ptr<Registry>`, constant-initialised before `main`. A function-local `static` map inside `DiagnosticsTable()` is constructed during `main`, so reverse-order teardown destroys **the map first** and then runs `~Registry` for the singleton — which reaches into a destroyed `std::unordered_map`. That is undefined behaviour, and the editor hits it: it uses `Registry::Instance()` throughout and never resets it.

Give the table a lifetime that cannot end:

```cpp
std::unordered_map<const Registry *, RegistryDiagnostics> &DiagnosticsTable() {
  // Intentionally leaked. ~Registry reaches into this map, and the editor's
  // Registry::Instance() singleton is destroyed during static teardown — after
  // a function-local static would already have been destroyed. Leaking it
  // makes the destructor safe at any point in the program's life. One map,
  // freed by the OS at exit.
  static auto &table = *new std::unordered_map<const Registry *, RegistryDiagnostics>();
  return table;
}
```

The same reasoning applies to whatever `Logger` the destructor's report goes through: it must not be destroyed before the last `~Registry` runs. `EcsReportErr` uses a function-local `static Logger`; leak that one too, for the same reason and with a comment saying so. `Logger::messages` is a namespace-scope static in another translation unit, so its order relative to `Registry::instance` is unspecified — say plainly in the comment that a report from static teardown is best-effort.

- [ ] **Step 2: Write the failing tests**

Replace the three threshold-based cases. `ECS_PENDING_ENTITY_WARNING_THRESHOLD` is no longer needed — remove the constant and every reference to it.

```cpp
Describe(MissingUpdateSpec) {
  It(should_report_a_registry_destroyed_without_ever_flushing) {
    Logger::messages.clear();
    {
      Registry registry;
      (void)registry.CreateEntity();
      (void)registry.CreateEntity();
    } // destroyed here, Update() never called

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
    // address back readily. Without the erase in ~Registry, this second
    // registry inherits the first's updateCalls and the diagnostic silently
    // stops working for it. Deleting the erase(this) line MUST fail this case.
    const Registry *firstAddress = nullptr;
    {
      Registry first;
      firstAddress = &first;
      (void)first.CreateEntity();
      first.Update();          // marks this address as having flushed
    }

    Logger::messages.clear();
    {
      Registry second;
      // Assert the address was actually reused, so the case cannot pass by
      // testing nothing. If it was not, the test is inconclusive rather than
      // passing — say so loudly.
      Assert::That(&second == firstAddress, Equals(true));
      (void)second.CreateEntity();
    } // never flushed — must report, despite `first` having flushed

    Assert::That(SpecRegistryErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };
}
```

The last case is the one that matters, and it is the reason this task exists in the form it does — Task 13 reuses this side-table pattern for `NetServer`, so the erasure needs proven coverage here.

**If the address is not reused** and that assertion fails, do not weaken it into a no-op. Report it: the case needs a different construction (for instance, allocating both registries with `new`/`delete` at a controlled address) to exercise the same path deterministically.

- [ ] **Step 3: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: the first and last cases FAIL. The three silence cases pass already and are the regression net.

- [ ] **Step 4: Implement**

In `common/ecs.cpp`, inside the existing anonymous namespace:

```cpp
struct RegistryDiagnostics {
  unsigned long updateCalls = 0;
  unsigned long entitiesCreated = 0;
  unsigned int reports = 0;
};
```

`Registry::Update()` increments `updateCalls`. `Registry::CreateEntity()` increments `entitiesCreated` and does nothing else — no reporting on that path at all.

`~Registry()` reports and then erases:

```cpp
Registry::~Registry() {
  RegistryDiagnostics &diagnostics = DiagnosticsTable()[this];
  if (diagnostics.updateCalls == 0 && diagnostics.entitiesCreated > 0 &&
      EcsShouldReport(diagnostics.reports)) {
    EcsReportErr(
        "~Registry: this registry created " +
        std::to_string(diagnostics.entitiesCreated) +
        " entities and Registry::Update() was never called on it, so none of "
        "them ever joined a system — nothing it owned rendered or moved. Call "
        "registry.Update() once per frame, first, in your state's update()." +
        EcsSuppressionNote(diagnostics.reports));
  }
  DiagnosticsTable().erase(this);
  logger.Log("Registry destructor called.");
}
```

- [ ] **Step 5: Revert the workaround in `specs/ecs.spec.cpp`**

The previous design forced an `Update()` call into an unrelated pre-existing spec to dodge its false positive. That workaround is no longer needed and must be removed, restoring the case to what it was — it is an ASan repro for reading past a component pool, and it should read exactly as it did before this task touched it.

- [ ] **Step 6: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all five new cases PASS, `specs/ecs.spec.cpp` is back to its original form and still passes, and no other spec logs an error. Zero compiler warnings.

- [ ] **Step 7: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/registry.spec.cpp specs/ecs.spec.cpp
git commit -m "Report a registry destroyed without ever flushing"
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

### Task 8: Delete `CollisionSystem`

`ContactSystem` (`common/systems/contact.h`) superseded it in 1.3.0: it reports overlaps with a normal and penetration depth, fires begin/end callbacks once per pair, and never touches an entity. `CollisionSystem` only kills — on overlap it calls `Kill()` on both entities carrying a `RigidBodyComponent`, with no callback and no way to observe without acting. The two already share one copy of the bounds math via `ContactSystem::BoundsOf`, so nothing else depends on it.

It survived 1.x only because the compatibility promise forbade deleting a public member. 2.0.0 resets that promise, and `KNOWN_ISSUES.md` already names this deletion as the intended outcome.

Verified before this task was written: `grep -rn "AddSystem<CollisionSystem>" examples/ editor/` is empty. No example, and not the editor, registers it. `specs/systems/collision.spec.cpp` is the only thing in the tree that instantiates the class.

**Files:**
- Delete: `common/systems/collision.h`
- Delete: `specs/systems/collision.spec.cpp`
- Modify: `common/states/gameState.h` — remove the `#include "../systems/collision.h"` line
- Modify: `TUTORIAL.md` — remove the `CollisionSystem` row
- Modify: `common/systems/contact.h` — three comments reference `CollisionSystem` (lines 18, 176, 193)

**No Makefile edit is needed.** `Makefile.debian:70` builds the spec list with `$(shell find specs -name '*.cpp')`, so a deleted spec file drops out on its own.

**Interfaces:**
- Consumes: nothing
- Produces: nothing. This task only removes.

- [ ] **Step 1: Confirm nothing else depends on it**

```bash
grep -rn "CollisionSystem\|collision\.h" --include=*.cpp --include=*.h --include=*.mk --include=Makefile.* common editor examples specs template
```

Expected hits: the class's own header, its spec, the `#include` in `common/states/gameState.h`, and possibly a Makefile object list. `RenderColliderSystem` and `ContactSystem` are different types — do not touch them, and do not confuse `renderCollider.h` for `collision.h`.

If the grep turns up a dependency this list does not predict, **stop and report BLOCKED** rather than deleting it. An unexpected consumer means the deletion needs a decision, not a bulldozer.

- [ ] **Step 2: Delete the class and its spec**

```bash
git rm common/systems/collision.h specs/systems/collision.spec.cpp
```

- [ ] **Step 3: Drop the include and the tutorial row**

Remove the `#include "../systems/collision.h"` line from `common/states/gameState.h`. Leave every other include in that file alone — `gameState.h` deliberately pulls in the common engine surface, and trimming the rest is a separate, still-unmade decision.

`common/systems/contact.h` refers to the deleted class in three comments — line 18 ("CollisionSystem (../systems/collision.h) is the older kill-on-contact..."), line 176, and line 193 ("CollisionSystem::isCollision is inclusive and..."). Rewrite each to stand on its own without naming a class that no longer exists. Line 193's comment documents a real inclusivity detail of the bounds comparison; keep the technical content and drop only the cross-reference. Do not delete these comments wholesale — they explain why `ContactSystem` behaves as it does.

Remove the `CollisionSystem` row from `TUTORIAL.md`. If the surrounding prose refers to it, rewrite that sentence to name `ContactSystem` and what it does instead: reports overlaps with a normal and penetration depth, and never kills anything.

- [ ] **Step 4: Build and test**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: builds clean. The suite drops the collision spec's cases — **report the new total and the delta explicitly**, because a shrinking test count is exactly what a mistakenly deleted spec file also looks like. Confirm the count fell by the number of cases in the deleted file and by no more.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "Delete CollisionSystem, superseded by ContactSystem"
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
- No Makefile edit needed: `Makefile.debian:70` globs spec sources with `$(shell find specs -name '*.cpp')`, so a new file under `specs/` is picked up automatically.

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

- [ ] **Step 4: Confirm the spec was picked up**

No Makefile edit is needed — `Makefile.debian:70` is `TESTSRCS := $(shell find specs -name '*.cpp')`. Just confirm the new file's cases appear in the run by checking the total rose by five.

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

1. **2.0.0 is a rebuild, and some source will need editing.** No type changed *size* — state the measured sizes and how to confirm them — but four public source-level breaks are taken, so a game must recompile and may have to edit. Lead with the four, each with the before/after code and the one-line fix:
   - `Registry::AddSystem` takes a forwarding reference. Only `AddSystem<Sys, int>(x)` with an lvalue breaks; the deduced form is unaffected. Also fixes a silent move out of the caller's lvalue.
   - `CollisionSystem` is deleted. Migrate to `ContactSystem`, which reports overlaps with a normal and penetration depth and never kills anything.
   - `NetServer`, `NetClient`, `NetConnection`, `NetSocket` are no longer copyable or movable. Hold them by reference or `unique_ptr`, never by value in a reallocating container. They were always unsafe to copy — ~372 KB and ~188 KB, a duplicated descriptor, and callbacks pointing at the original.
   - `Entity(std::size_t)` is `explicit`. `registry.KillEntity(88)` no longer compiles; write `Entity(id)` where you meant a real entity.
2. **Argument forwarding, in detail:** `AddSystem` now takes a forwarding reference. Every deduced call is unaffected; the explicit-template-argument form `AddSystem<Sys, int>(x)` with an lvalue `x` no longer compiles. Give both forms as code. Say that the change also fixes a silent move out of the caller's lvalue under the old signature — a game that passed a container or string to a system constructor and got an empty one back was hitting this.
3. **New diagnostics and what each means**, one short section each: late component, late system, missing `Update()`, `srcRect` outside texture, `GetSystem` about to throw, `GetComponent` miss. For each, say what to change. Note that all are throttled to `ECS_MAX_DIAGNOSTIC_REPORTS` (4) occurrences.
4. **New API:** `TryGetSystem`, `TryGetEntityByTag`, `AdmitExistingEntities`, `IsPendingAdmission`, `CountEntitiesMissedBySystem`, `common/input/keyboard.h`.
5. **Networking, for internet play:** `NetServer::SetMaxClientsPerIp` (default unchanged at 4, which only bites over the internet, where every player behind one router shares an address) and `NetServer::GetConnectedClientIds`. State plainly that `GetClientCount()` is not a loop bound and why. Note that `kNetMaxClientsPerIp` stays 4 in `netTypes.h` on purpose: changing the constant would split the ABI between a game and the library.
6. **The 1.x line ends here.** State what 2.0.0 promises: no layout changes were taken, the four breaks are compile-time and loud, and the layout-changing fixes (`Entity` generation counter, `Tile` animation fields, `MAX_COMPONENTS`, `System`'s disabled latch, namespaces) remain 3.0 items.
7. **Coming from 1.2.x:** the hop crosses 1.3.0, which **requires a full rebuild** — `sizeof(AssetStore)` went 112 → 208 and games allocate the store themselves, so a 1.2.x binary against a 1.3.0 library overflows its allocation with no warning. Install the package and rebuild; do not swap the `.so`; run `make clean`, since the engine has no header dependency tracking.

- [ ] **Step 2: Add the `CHANGELOG.md` entry**

A `## [2.0.0]` section with `### Added`, `### Changed` and `### Deprecated`, in the prose style of the existing 1.3.0 entry — each item says what the problem was, not just what changed. The `AddSystem` signature change goes under `### Changed` and must name the explicit-template-argument break outright.

- [ ] **Step 3: Update `KNOWN_ISSUES.md`**

Three items are now **fixed, not merely detectable** — item 2 (implicit `Entity` conversion), item 6 (networking copy operations) and item 10's `CollisionSystem` deletion. Move them out of the open list into a resolved section that records what shipped and in which release; do not delete the entries, since the record of what was decided is the file's value.

Renumber the file's milestone: it refers to "Storm! Engine v3" as the compatibility reset in 8 places across `KNOWN_ISSUES.md` and `CHANGELOG.md`. That reset partly happened here, in 2.0.0. Rewrite those references to name **3.0** as the milestone for the remaining layout-changing items, and add a sentence at the top of `KNOWN_ISSUES.md` saying which promise now applies: 2.x keeps layouts and public signatures stable, and the items still listed are the ones that need a layout change to fix.

Items 1, 4, 5 and 7 now have a runtime diagnostic. Add one short paragraph to each saying so, in the shape of the existing "Resolved for new code in 1.3.0" notes. The defects themselves are unchanged and stay listed — "cannot be fixed in 1.x" and "cannot be detected in 1.x" are different claims, and only the second has changed.

Item 9 (no namespaces) explicitly gets nothing. Add a sentence saying a namespace alias was considered and rejected: the global names remain either way, so it would not prevent the collision it appears to address.

Also note under item 3 or in the preamble that `AssetStore_Ptr`, `Logger_Ptr` and the other `_Ptr` typedefs are `std::unique_ptr`, so they are move-only — hold the member, do not copy it.

- [ ] **Step 4: Purge the dangling references to deleted and changed API**

Task 8 deleted `CollisionSystem`, and several documents still describe it as present. These are user-facing and currently assert something false:

- `README.md:15` — "the older kill-on-contact `CollisionSystem` still works but is deprecated". It does not still work; it is gone. Rewrite the clause so the sentence describes `ContactSystem` alone.
- `PROJECT_REFERENCE.md:98` — a paragraph describing `CollisionSystem` as deprecated-but-behaviour-identical, including the detail that its overlap test is inclusive where `ContactSystem::Overlaps` is strict. Delete the paragraph, but check first whether the inclusive/strict distinction is documented anywhere else; if it is not, that is real information about `ContactSystem` and should be kept in a sentence of its own.
- `PROJECT_REFERENCE.md:197` — the `collision.h` row in the file table. Remove the row.

**Do not edit historical `CHANGELOG.md` entries.** The mentions at lines 158, 183, 189, 192, 377 and 669 describe what shipped in past releases and were true when written. A changelog that is retroactively edited stops being a record. Only the new 2.0.0 entry is yours.

`examples/strategy/README.md:69` also names "the deprecated `CollisionSystem`" in a comparative aside. Leave it to Task 12, which owns the examples.

- [ ] **Step 5: Update `README.md`'s feature list and add a Diagnostics section**

Add `keyboard.h` to whatever list enumerates `common/input/`, and add a short "Diagnostics" section: the engine reports a fixed number of occurrences of each misuse at `Err` level, they are on in every build, and a game seeing one has a real bug.

- [ ] **Step 6: Verify the whole tree once more**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: builds with no new warnings, every spec passes. Record the actual pass count in the commit message.

- [ ] **Step 7: Commit**

```bash
git add docs/UPGRADING.md CHANGELOG.md KNOWN_ISSUES.md README.md PROJECT_REFERENCE.md
git commit -m "Document the 2.0.0 guardrails and the upgrade path from 1.2.x"
```

---

### Task 12: Bring the examples and the template onto the 2.0.0 surface

The examples are how a new game learns the engine, and the template is what a new game is literally copied from. A diagnostic that fires while running `examples/platformer` is a bug the examples have been teaching.

This task runs **last**, after every engine task, so it exercises what actually shipped. It also absorbs the fallout of the four source-level breaks: `CollisionSystem` is gone, `Entity(std::size_t)` is explicit, and the net types are non-copyable. Those breaks surface as compile errors in the examples, and fixing each at its call site is part of this task.

Confirmed before this task was written: no example, and not the editor, registers `CollisionSystem` (`grep -rn "AddSystem<CollisionSystem>" examples/ editor/` is empty), so its deletion in Task 8 should cost the examples nothing. If an example nonetheless fails to build after Task 8, that is a finding worth reporting, not a silent fixup.

**Files:**
- Modify: whichever example sources the diagnostics implicate — discovered in Step 1, not guessed
- Modify: `template/src/states/playState.cpp`, `template/src/game.cpp`
- Modify: `template/README.md` if it documents input handling

**Interfaces:**
- Consumes: everything Tasks 1-11 produced — `Keyboard`, `TryGetSystem`, `TryGetEntityByTag`, `AdmitExistingEntities`, and the four runtime diagnostics
- Produces: no new API

- [ ] **Step 0: Settle whether `examples/shooter` was already broken**

Task 9 reported that `examples/shooter` does not build, citing a compile error at `examples/shooter/src/states/playState.cpp:439`:

```cpp
const bool aIsEnemy = c.a.HasComponent<EnemyComponent>();
```

It was described as a template-disambiguation error, but that diagnosis is doubtful: the enclosing `PlayState::CheckCollisions()` is not a template, so no `template` disambiguator is needed, and `Entity::HasComponent() const` is const-qualified so a `const Contact &` is not the problem either.

What is established: **this branch never touched `examples/shooter`** (`git log origin/main..HEAD -- examples/shooter/` is empty) and the line is byte-identical to `origin/main`.

Settle it decisively before doing anything else, because the answer changes what this task owns:

```sh
git stash list                     # confirm nothing pending
git worktree add /tmp/shooter-base origin/main
# build the engine and the shooter example from /tmp/shooter-base
```

- **If it fails on `origin/main` too**, it is inherited breakage. Record the real compiler error verbatim in your report, do **not** fix it as part of this task, and say so plainly — it needs its own decision.
- **If it builds on `origin/main` and fails here**, one of this release's changes broke it. That is a genuine finding and the most important thing this task will produce. Bisect to the responsible commit if you can, report it, and **stop** rather than patching the example to compile.

Either way, quote the actual compiler output rather than paraphrasing the error class. Remove the temporary worktree when done (`git worktree remove /tmp/shooter-base`).

- [ ] **Step 1: Run every example and collect what fires**

Build and run each example under `examples/` that builds on this machine, plus `editor/` and `template/`. Capture stderr. Then:

```bash
grep -n "RenderSystem: srcRect\|AddComponent: entity\|AddSystem:\|CreateEntity:\|GetSystem:\|GetComponent: entity" <captured-log>
```

Write the full findings to the task report file, one line per example: which diagnostic, which source line caused it. An example that fires nothing gets a line saying so — that is the result for most of them, and recording it is what makes the ones that do fire trustworthy.

The `nx-` and `android-` prefixed examples do not build on Linux. Say so in the report rather than implying the whole set was exercised.

- [ ] **Step 2: Fix each firing diagnostic at its cause**

For each hit, fix the example, not the diagnostic. The four causes and their fixes:

- `RenderSystem: srcRect ... outside texture` — the `SpriteComponent` `width`/`height` do not match the sheet cell, or `AnimationComponent.vertical` does not match the sheet layout. Correct whichever is wrong. Do not resize with `width`/`height`; those are the source rect. Screen size is `TransformComponent.scale`.
- `AddComponent: entity ... was already admitted` — move the `AddComponent` call before the `registry_.Update()` that admits the entity.
- `AddSystem: ... registered after N matching entities` — move the `AddSystem` call before the entity creation. Reach for `AdmitExistingEntities<T>()` only where the late registration is deliberate, and comment why.
- `CreateEntity: ... Registry::Update() has never been called` — the state never flushes. Add `registry_.Update()` as the first call in its `update()`.

Each fix is its own commit, named for the example.

- [ ] **Step 3: Re-run and confirm silence**

Re-run every example from Step 1. Expected: no diagnostic in any log. Paste the actual grep output — an empty result — into the report.

- [ ] **Step 4: Adopt `Keyboard` in the template**

The template is the canonical shape a new game copies, so it is where the input idiom has to be right. In `template/src/states/playState.cpp`, replace the raw `SDL_PollEvent` key handling with a `Keyboard` member fed from the state's existing poll loop:

```cpp
  keyboard_.BeginFrame();
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    keyboard_.HandleEvent(event);
    if (event.type == SDL_QUIT) {
      m_exiting = true;
    }
  }
  if (keyboard_.WasPressed(SDL_SCANCODE_ESCAPE)) {
    m_exiting = true;
  }
```

Keep the single poll loop the template already has. **Do not add a second `SDL_PollEvent` call, and do not move polling into `Game::ProcessInput` while the state also polls** — two consumers draining one queue is the exact trap `Keyboard` exists to prevent. If `template/src/game.cpp` already polls, the state must not; make the template show one owner and say which in a comment.

Leave the examples' own input code alone unless a diagnostic implicated it. Rewriting nine working examples to a new class is churn, and the plan is not a refactor.

- [ ] **Step 5: Update the template README**

If `template/README.md` documents input handling, update it to the `Keyboard` idiom and state the one-poll-owner rule in a sentence.

- [ ] **Step 6: Full build and test**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Then rebuild every example. Expected: everything builds, specs pass, no diagnostics in any example log.

- [ ] **Step 7: Commit**

```bash
git add examples/ template/
git commit -m "Fix what the 2.0.0 diagnostics found in the examples, and put the template on Keyboard"
```

---

### Task 13: Make the per-address connection cap configurable

**The trap:** `kNetMaxClientsPerIp = 4` (`common/net/netTypes.h:30`) is checked at `common/net/netServer.cpp:263`. On a LAN this is invisible — twelve machines have twelve addresses. Over the internet every player behind one router shares one public address, so a twelve-player game with two people in the same house is already refused, with the server sending `"too many connections"` and the game having no way to allow it.

**Why the two obvious fixes are wrong, and must not be attempted:**

- **Raising the constant** is a silent ABI split. It is `constexpr` in a header, so a game compiled against 4 and a `libstormenginev2.so` built with 8 disagree with no diagnostic of any kind.
- **Adding an `int` member to `NetServer`** changes `sizeof(NetServer)` (currently ~372 KB). Games allocate the server themselves via `std::make_unique<NetServer>()`, so the size is emitted in game code — this is precisely the 1.3.0 `AssetStore` heap overflow (112 → 208 bytes) repeated, and nothing warns about it.

The way through is the side table already introduced in Task 4: a file-static map in `netServer.cpp` keyed on `this`, erased by the destructor. Default behaviour is unchanged at 4.

**Files:**
- Modify: `common/net/netServer.h` — two public method declarations
- Modify: `common/net/netServer.cpp` — side table, the two bodies, the check at line 263, destructor cleanup
- Test: `specs/net/netLoopback.spec.cpp`

**Interfaces:**
- Consumes: `kNetMaxClientsPerIp` (`common/net/netTypes.h:30`), `NetServer::kMaxClients` (16), `NetServer::CountSlotsWithIp`
- Produces:
  - `void NetServer::SetMaxClientsPerIp(int limit)`
  - `int NetServer::GetMaxClientsPerIp() const`

- [ ] **Step 0: Write the multi-client pump helper**

Read `specs/net/netLoopback.spec.cpp:25-80` first — `PumpUntil`, `PumpUntil2` and `PumpServerUntil` are three copies of one loop differing only in how many clients they pump. Add one helper that covers any number, following their existing shape (same timeout parameter, same predicate-driven exit, same return convention):

```cpp
// Pumps the server and every client until `done` returns true or the timeout
// expires. Returns whether `done` became true. Generalises PumpUntil /
// PumpUntil2, which handle one and two clients.
static bool PumpUntilSettled(NetServer &server,
                             std::vector<std::unique_ptr<NetClient>> &clients,
                             int timeoutMs,
                             const std::function<bool()> &done);
```

Give it the same body shape the existing helpers use — poll and update the server, poll and update each client, sleep the same interval, check `done`, bail at the timeout. Do **not** delete or rewrite `PumpUntil`, `PumpUntil2` or `PumpServerUntil`; existing cases use them and this task is not a refactor of the net specs.

Where the cases below write `PumpUntilSettled(server, clients)`, call it with the file's usual timeout and a predicate that matches what the case is waiting for — for the connection cases, that the server's client count has stopped changing or has reached the expected value. A case that waits on "the fifth client is refused" must wait for a settled state, not for a count that never arrives; give it a predicate that becomes true once the four admitted clients are online, then assert the fifth never joins.

- [ ] **Step 1: Write the failing tests**

Every loopback client connects from `127.0.0.1`, so the default cap of 4 is directly exercisable. Append to `specs/net/netLoopback.spec.cpp`, following the connect/poll idiom already in that file — read the existing cases first and reuse their helpers rather than writing a second connect loop.

```cpp
Describe(MaxClientsPerIpSpec) {
  It(should_default_to_the_engine_wide_cap) {
    NetServer server;
    Assert::That(server.GetMaxClientsPerIp(), Equals(kNetMaxClientsPerIp));
  };

  It(should_refuse_the_fifth_client_from_one_address_by_default) {
    NetServer server;
    Assert::That(server.Start(0, 12), Equals(true));

    // Five clients, all from 127.0.0.1. The default cap is 4.
    std::vector<std::unique_ptr<NetClient>> clients;
    for (int i = 0; i < 5; ++i) {
      clients.push_back(std::unique_ptr<NetClient>(new NetClient()));
      clients.back()->Connect("127.0.0.1", server.GetPort());
    }
    PumpUntilSettled(server, clients);

    Assert::That(server.GetClientCount(), Equals(kNetMaxClientsPerIp));
  };

  It(should_admit_more_once_the_cap_is_raised) {
    NetServer server;
    server.SetMaxClientsPerIp(12);
    Assert::That(server.Start(0, 12), Equals(true));

    std::vector<std::unique_ptr<NetClient>> clients;
    for (int i = 0; i < 6; ++i) {
      clients.push_back(std::unique_ptr<NetClient>(new NetClient()));
      clients.back()->Connect("127.0.0.1", server.GetPort());
    }
    PumpUntilSettled(server, clients);

    Assert::That(server.GetClientCount(), Equals(6));
  };

  It(should_clamp_a_limit_above_the_slot_count) {
    NetServer server;
    server.SetMaxClientsPerIp(999);
    Assert::That(server.GetMaxClientsPerIp(), Equals(NetServer::kMaxClients));
  };

  It(should_reject_a_limit_below_one_and_keep_the_previous_value) {
    NetServer server;
    server.SetMaxClientsPerIp(12);
    server.SetMaxClientsPerIp(0);
    Assert::That(server.GetMaxClientsPerIp(), Equals(12));
  };

  It(should_not_leak_the_setting_to_a_later_server_at_the_same_address) {
    // The side table is keyed on `this`, and a destroyed server's address can
    // be handed straight back to the next allocation. If the destructor does
    // not erase its entry, the next NetServer silently inherits this one's cap.
    {
      NetServer first;
      first.SetMaxClientsPerIp(12);
    }
    NetServer second;
    Assert::That(second.GetMaxClientsPerIp(), Equals(kNetMaxClientsPerIp));
  };
}
```

**`PumpUntilSettled` does not exist yet — Step 0 below writes it.** The file's existing helpers are `PumpUntil` (one client, `specs/net/netLoopback.spec.cpp:25`), `PumpUntil2` (two clients, line 42) and `PumpServerUntil` (server only, line 61). None of them handles the five and six clients these cases need, and adding `PumpUntil5` and `PumpUntil6` to a file that already has three near-identical pump loops would be the wrong answer.

The last case is the one that matters most. A side table without destructor cleanup passes every other case in this list and then corrupts an unrelated server later in the same process.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: compile error — `'class NetServer' has no member named 'SetMaxClientsPerIp'`.

- [ ] **Step 3: Declare the methods**

In `common/net/netServer.h`, in the public section beside `GetClientCount()` (line 45):

```cpp
    // How many clients may connect from one address. Defaults to
    // kNetMaxClientsPerIp (4), which is an anti-flood cap sized for a LAN.
    //
    // Over the internet every player behind one router shares a public
    // address, so a twelve-player game with two people in one house is
    // refused at the default. Raise it for internet play; leave it alone for
    // a LAN, where it is doing real work.
    //
    // A limit below 1 is refused and logged; a limit above kMaxClients is
    // clamped to kMaxClients. Takes effect on the next connection attempt,
    // and never disconnects a client already admitted.
    //
    // The setting is held outside the object: sizeof(NetServer) is ABI,
    // because games allocate the server themselves and the size is emitted
    // at their call site.
    void SetMaxClientsPerIp(int limit);
    int GetMaxClientsPerIp() const;
```

- [ ] **Step 4: Implement**

In `common/net/netServer.cpp`, near the top:

```cpp
namespace {

// Per-NetServer settings that cannot live on NetServer itself: games allocate
// the server with std::make_unique<NetServer>(), so sizeof(NetServer) is
// emitted in game code and 1.x may not change it. Keyed on `this` and erased
// by ~NetServer — without that erase, a recycled address hands the next
// server this one's settings.
//
// Not thread-safe, in keeping with the rest of the networking layer.
std::unordered_map<const NetServer *, int> &MaxClientsPerIpTable() {
  static std::unordered_map<const NetServer *, int> table;
  return table;
}

} // namespace

void NetServer::SetMaxClientsPerIp(int limit) {
  if (limit < 1) {
    logger_.Err("NetServer::SetMaxClientsPerIp: limit " +
                std::to_string(limit) +
                " is below 1; ignoring. The per-address cap is unchanged.");
    return;
  }
  if (limit > kMaxClients) {
    limit = kMaxClients;
  }
  MaxClientsPerIpTable()[this] = limit;
}

int NetServer::GetMaxClientsPerIp() const {
  auto found = MaxClientsPerIpTable().find(this);
  return found == MaxClientsPerIpTable().end() ? kNetMaxClientsPerIp
                                               : found->second;
}
```

Add `#include <unordered_map>` to `netServer.cpp` if it is not already there.

In `~NetServer()`, as the first statement:

```cpp
  MaxClientsPerIpTable().erase(this);
```

And at line 263, replace the constant with the accessor:

```cpp
  if (CountSlotsWithIp(from) >= GetMaxClientsPerIp()) {
    SendControl(from, kNetControlClose, "too many connections", 21);
    return;
  }
```

Leave `kNetMaxClientsPerIp` in `netTypes.h` at 4, unchanged. It is now the default rather than the law, and changing its value would reintroduce the ABI split this task exists to avoid.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all six new cases PASS and every existing net spec still passes. The existing loopback cases connect fewer than four clients, so none of them should change behaviour — if one does, say so rather than adjusting it.

- [ ] **Step 6: Commit**

```bash
git add common/net/netServer.h common/net/netServer.cpp specs/net/netLoopback.spec.cpp
git commit -m "Make the per-address connection cap configurable for internet play"
```

---

### Task 14: A safe way to iterate connected clients

**The trap:** client ids are slot indices into `Slot slots_[kMaxClients]`, not a dense range. `for (int i = 0; i < server.GetClientCount(); i++)` works perfectly in a two-player test and silently stops sending to a player the moment anyone quits — client 3 remains connected in slot 3 while `GetClientCount()` returns 3, so the loop never reaches it. Nothing errors; a player simply stops receiving the world.

The correct loop already exists (`i < NetServer::kMaxClients` guarded by `IsClientConnected(i)`). This task makes the correct form the easy one and documents the trap at the method that baits it.

**Files:**
- Modify: `common/net/netServer.h` — one method declaration, doc comment on `GetClientCount`
- Modify: `common/net/netServer.cpp` — the body
- Test: `specs/net/netLoopback.spec.cpp`

**Interfaces:**
- Consumes: `NetServer::IsClientConnected`, `NetServer::kMaxClients`
- Produces: `int NetServer::GetConnectedClientIds(int *out, int maxOut) const` — writes the connected slot ids into `out` in ascending order, returns how many were written. Writes nothing and returns 0 when `out` is null or `maxOut` is below 1.

A plain array out-parameter rather than a returned `std::vector` or a `std::function` visitor: this is called every tick on the send path, and it must not allocate. It also has to compile under the Switch build's `-fno-exceptions`.

- [ ] **Step 1: Write the failing tests**

Append to `specs/net/netLoopback.spec.cpp`:

```cpp
Describe(ConnectedClientIdsSpec) {
  It(should_report_no_ids_for_a_server_with_no_clients) {
    NetServer server;
    int ids[NetServer::kMaxClients] = {};
    Assert::That(server.GetConnectedClientIds(ids, NetServer::kMaxClients),
                 Equals(0));
  };

  It(should_report_the_ids_of_connected_clients) {
    NetServer server;
    server.SetMaxClientsPerIp(12);
    Assert::That(server.Start(0, 12), Equals(true));

    std::vector<std::unique_ptr<NetClient>> clients;
    for (int i = 0; i < 3; ++i) {
      clients.push_back(std::unique_ptr<NetClient>(new NetClient()));
      clients.back()->Connect("127.0.0.1", server.GetPort());
    }
    PumpUntilSettled(server, clients);

    int ids[NetServer::kMaxClients] = {};
    const int count =
        server.GetConnectedClientIds(ids, NetServer::kMaxClients);

    Assert::That(count, Equals(3));
    for (int i = 0; i < count; ++i) {
      Assert::That(server.IsClientConnected(ids[i]), Equals(true));
    }
  };

  It(should_skip_a_hole_left_by_a_client_that_quit) {
    // The whole point: after a middle client leaves, the surviving ids are no
    // longer 0..count-1, which is what the naive GetClientCount loop assumes.
    NetServer server;
    server.SetMaxClientsPerIp(12);
    Assert::That(server.Start(0, 12), Equals(true));

    std::vector<std::unique_ptr<NetClient>> clients;
    for (int i = 0; i < 3; ++i) {
      clients.push_back(std::unique_ptr<NetClient>(new NetClient()));
      clients.back()->Connect("127.0.0.1", server.GetPort());
    }
    PumpUntilSettled(server, clients);

    int before[NetServer::kMaxClients] = {};
    const int countBefore =
        server.GetConnectedClientIds(before, NetServer::kMaxClients);
    Assert::That(countBefore, Equals(3));

    const int departing = before[1];
    server.DisconnectClient(departing, "spec");
    PumpUntilSettled(server, clients);

    int after[NetServer::kMaxClients] = {};
    const int countAfter =
        server.GetConnectedClientIds(after, NetServer::kMaxClients);

    Assert::That(countAfter, Equals(2));
    for (int i = 0; i < countAfter; ++i) {
      Assert::That(after[i] == departing, Equals(false));
      Assert::That(server.IsClientConnected(after[i]), Equals(true));
    }
  };

  It(should_write_no_more_than_the_caller_asked_for) {
    NetServer server;
    server.SetMaxClientsPerIp(12);
    Assert::That(server.Start(0, 12), Equals(true));

    std::vector<std::unique_ptr<NetClient>> clients;
    for (int i = 0; i < 3; ++i) {
      clients.push_back(std::unique_ptr<NetClient>(new NetClient()));
      clients.back()->Connect("127.0.0.1", server.GetPort());
    }
    PumpUntilSettled(server, clients);

    int ids[2] = {-1, -1};
    Assert::That(server.GetConnectedClientIds(ids, 2), Equals(2));
  };

  It(should_return_zero_for_a_null_buffer) {
    NetServer server;
    Assert::That(server.GetConnectedClientIds(nullptr, 4), Equals(0));
    int ids[1] = {};
    Assert::That(server.GetConnectedClientIds(ids, 0), Equals(0));
  };
}
```

Use the `PumpUntilSettled` helper Task 13 added to this file. It already exists by the time this task runs.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: compile error — `'class NetServer' has no member named 'GetConnectedClientIds'`.

- [ ] **Step 3: Declare it, and document the trap on `GetClientCount`**

In `common/net/netServer.h`, replace the bare `int GetClientCount() const;` (line 45) with:

```cpp
    // How many clients are connected. This is a count for display — "3/12
    // players" — and NOT a loop bound.
    //
    // Client ids are slot indices into a fixed array, not a dense range.
    // `for (int i = 0; i < GetClientCount(); i++)` works in a two-player test
    // and then silently stops sending to a player the moment anyone quits: a
    // client in slot 3 stays connected while the count reads 3. Nothing
    // errors; that player just stops receiving the world.
    //
    // Iterate with GetConnectedClientIds, or over kMaxClients guarded by
    // IsClientConnected.
    int GetClientCount() const;

    // Writes the connected client ids into `out` in ascending order and
    // returns how many were written, never more than `maxOut`. Pass
    // kMaxClients as `maxOut` to be sure of getting all of them.
    //
    //     int ids[NetServer::kMaxClients];
    //     const int count = server.GetConnectedClientIds(ids, NetServer::kMaxClients);
    //     for (int i = 0; i < count; ++i) { server.Send(ids[i], ...); }
    //
    // Takes an array rather than returning a container because this sits on
    // the per-tick send path and must not allocate.
    //
    // Returns 0 and writes nothing when `out` is null or `maxOut` is below 1.
    int GetConnectedClientIds(int *out, int maxOut) const;
```

- [ ] **Step 4: Implement**

In `common/net/netServer.cpp`, beside `GetClientCount`:

```cpp
int NetServer::GetConnectedClientIds(int *out, int maxOut) const {
  if (out == nullptr || maxOut < 1) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < kMaxClients && count < maxOut; i++) {
    if (IsClientConnected(i)) {
      out[count++] = i;
    }
  }
  return count;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all five new cases PASS.

- [ ] **Step 6: Commit**

```bash
git add common/net/netServer.h common/net/netServer.cpp specs/net/netLoopback.spec.cpp
git commit -m "Add GetConnectedClientIds and document why GetClientCount is not a loop bound"
```

---

### Task 15: Delete the networking copy operations

`NetServer`, `NetClient`, `NetConnection` and `NetSocket` all have implicit copy constructors and assignment operators. Each installs send callbacks capturing `this`, and each owns a socket file descriptor. Copying one gives you two objects whose callbacks point at whichever was copied *from*, and two destructors closing one descriptor. `NetServer` is ~372 KB and `NetClient` ~188 KB, so the copy is also a silent multi-hundred-kilobyte memcpy.

This is `KNOWN_ISSUES.md` item 6. It stayed unfixed through 1.x because `= delete` is a compile-time source break. 2.0.0 takes it.

Verified before this task was written: nothing in the tree copies one. Every use is by reference, a direct local, or a by-value *member* of a non-copied type — which is aggregation, not copying, and stays legal. `examples/netplay-checkers/src/states/playState.h:139-140` holds a `NetServer` and a `NetClient` by value as members and is unaffected; `examples/netchat`, `examples/netrepl` and `specs/net/netLoopback.spec.cpp` hold them by reference or `unique_ptr`.

**Files:**
- Modify: `common/net/netServer.h`, `common/net/netClient.h`, `common/net/netConnection.h`, `common/net/netSocket.h`
- Test: `specs/net/netLoopback.spec.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: four types that are no longer copyable. Whether they become movable is decided in Step 2.

- [ ] **Step 1: Write the failing tests**

These are compile-time properties, so the tests are `static_assert`s rather than runtime cases. Append to `specs/net/netLoopback.spec.cpp`, at file scope:

```cpp
// KNOWN_ISSUES item 6, fixed in 2.0.0: these four own a socket descriptor and
// install callbacks capturing `this`. A copy gives two objects whose callbacks
// point at the original and two destructors closing one descriptor.
static_assert(!std::is_copy_constructible<NetServer>::value,
              "NetServer must not be copy constructible");
static_assert(!std::is_copy_assignable<NetServer>::value,
              "NetServer must not be copy assignable");
static_assert(!std::is_copy_constructible<NetClient>::value,
              "NetClient must not be copy constructible");
static_assert(!std::is_copy_assignable<NetClient>::value,
              "NetClient must not be copy assignable");
static_assert(!std::is_copy_constructible<NetConnection>::value,
              "NetConnection must not be copy constructible");
static_assert(!std::is_copy_assignable<NetConnection>::value,
              "NetConnection must not be copy assignable");
static_assert(!std::is_copy_constructible<NetSocket>::value,
              "NetSocket must not be copy constructible");
static_assert(!std::is_copy_assignable<NetSocket>::value,
              "NetSocket must not be copy assignable");
```

Add `#include <type_traits>` to the file's includes.

- [ ] **Step 2: Run the tests to verify they fail, and learn what else breaks**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: eight `static_assert` failures, since all four types are currently copyable.

**This build is also your survey.** Note every other compile error it produces. There should be none beyond the `static_assert`s — but `NetServer` holds `Slot slots_[kMaxClients]` where `Slot` contains a `NetConnection` by value, and `NetClient` holds a `NetSocket` and a `NetConnection` by value, so deleting `NetConnection`'s and `NetSocket`'s copy operations propagates outward. Record what you find; it decides Step 3.

- [ ] **Step 3: Delete the copy operations**

In each of the four headers, in the public section immediately after the constructor declarations:

```cpp
    // Owns a socket descriptor and installs callbacks capturing `this`: a copy
    // would give two objects whose callbacks point at the original, and two
    // destructors closing one descriptor. KNOWN_ISSUES item 6, fixed in 2.0.0.
    NetServer(const NetServer &) = delete;
    NetServer &operator=(const NetServer &) = delete;
```

with the type's own name in each file.

**Do not add move operations unless Step 2 proved something needs them.** Declaring a copy constructor suppresses the implicit move, so these types become neither copyable nor movable — which is correct for something holding a descriptor and self-referential callbacks, and matches how every in-tree consumer already uses them (by reference or `unique_ptr`). If Step 2 turned up a real consumer that moves one, report it and stop rather than inventing move semantics for a callback-capturing type on your own.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: builds clean, every net spec passes, and the whole suite passes. If `NetServer` or `NetClient` now fails to compile because an aggregate member became non-copyable, that is the outward propagation Step 2 predicted — it means some code really was copying, and it is a finding to report, not a thing to work around by restoring a copy constructor.

- [ ] **Step 5: Commit**

```bash
git add common/net/netServer.h common/net/netClient.h common/net/netConnection.h common/net/netSocket.h specs/net/netLoopback.spec.cpp
git commit -m "Delete the networking copy operations"
```

---

### Task 16: Make `Entity(std::size_t)` explicit

`Entity(std::size_t id)` is not `explicit`, so any function taking an `Entity` silently accepts a bare number:

```cpp
registry.KillEntity(88);        // compiles. 88 is not an entity.
```

Every `Entity` member null-checks its registry pointer, so this no-ops and logs rather than dereferencing garbage — but it should never have compiled. This is `KNOWN_ISSUES.md` item 2, which records that `grep -rnE 'KillEntity\([0-9]|TagEntity\([0-9]' examples/ editor/ common/` is empty.

**That grep never covered `specs/`, and there is exactly one in-tree site.** `specs/systemMembership.spec.cpp:259` contains `registry.KillEntity(88);` — a spec that deliberately pins the current bad behaviour, exactly as `KNOWN_ISSUES.md` describes it. It will stop compiling, which is the point.

Do not delete that case. Read what it asserts first: it pins that a bare integer no-ops and logs rather than corrupting memory. Under `explicit` the mistake it guards against cannot be written at all, so the case's job is done by the compiler. Replace it with a `static_assert` in the same file recording that the conversion is now rejected, and carry the original comment's intent across so the history is not lost. If the case asserts anything *beyond* the implicit conversion — say, that `KillEntity` on a live-but-unowned entity no-ops — keep that part as a runtime case using `Entity(88)` explicitly.

**Files:**
- Modify: `common/ecs.h` — the constructor at line 108
- Test: `specs/ecs.spec.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `explicit Entity(std::size_t id)`

- [ ] **Step 1: Write the failing test**

The property is compile-time. Append to `specs/ecs.spec.cpp` at file scope:

```cpp
// KNOWN_ISSUES item 2, fixed in 2.0.0: a bare integer must not become an
// Entity. Direct initialisation — Entity(7) — stays legal and is how the
// Registry builds them; only the implicit conversion goes away.
static_assert(!std::is_convertible<std::size_t, Entity>::value,
              "a bare size_t must not implicitly convert to an Entity");
static_assert(std::is_constructible<Entity, std::size_t>::value,
              "Entity must still be constructible from an id");
```

Add `#include <type_traits>` to the file's includes.

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: the first `static_assert` fails — `a bare size_t must not implicitly convert to an Entity`.

- [ ] **Step 3: Add `explicit`**

In `common/ecs.h`, at the constructor (line 108):

```cpp
  // explicit since 2.0.0: without it any function taking an Entity silently
  // accepted a bare number, so registry.KillEntity(88) compiled.
  explicit Entity(std::size_t id) : id(id){};
```

- [ ] **Step 4: Run the tests, and read every error carefully**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`

Expected: clean. If anything fails to compile, the fix is almost always to write the construction explicitly — `Entity(id)` instead of a bare `id` — at the call site. **Do not fix a break by removing `explicit`.**

`Registry::CreateEntity` (`common/ecs.cpp:131`) and `Registry::GetEntityByTag` (`common/ecs.cpp:409`) both return `Entity` — check neither returns a bare integer or a braced initialiser. Beyond that, `common/ecs.cpp` and `common/ecs.h` construct `Entity` internally in several places, and the earlier tasks in this release added more (`Entity entity(id);` inside the membership scans). Direct initialisation is unaffected by `explicit`, so those are fine — but a `return 0;` or a braced `{id}` in a function returning `Entity` would not be. Report anything you had to change.

- [ ] **Step 5: Commit**

```bash
git add common/ecs.h specs/ecs.spec.cpp
git commit -m "Make Entity's id constructor explicit"
```

---

## Verification before calling this done

- [ ] `make -f Makefile.debian clean && make -f Makefile.debian test` — report the pass count printed, not a summary of it.
- [ ] `grep -rn "sizeof(Registry)\|sizeof(Entity)" specs/` — the layout assertions in the spec suite still pass. If none exist, add one to `specs/ecs.spec.cpp` pinning `sizeof(Registry) == 576`, `sizeof(Entity) == 16`, `sizeof(System) == 32`, `sizeof(Signature) == 8`, `sizeof(Tile) == 80` on x86-64, so the next layout change is argued rather than assumed.
- [ ] No `-Wdeprecated-declarations` warning outside `specs/systems/collision.spec.cpp`.
- [ ] Every example, the editor and the template run with no diagnostic in the log (Task 12 verifies this; this line is the final confirmation).
- [ ] `git log --oneline` shows one commit per task.
- [ ] Post a comment on https://github.com/SamsWebs/center-ice-hockey/issues/91 when 2.0.0 tags, with the release link and the `UPGRADING.md` path.
