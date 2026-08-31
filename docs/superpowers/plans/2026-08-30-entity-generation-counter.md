# Entity Generation Counter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a stale `Entity` handle detectable, so it can no longer kill, tag, or read components from the live entity that recycled its id.

**Architecture:** `Entity` gains a `generation`; `Registry` keeps a `generations` vector indexed by id and bumps it when an id is freed. Equality becomes generation-aware and the relational operators are deleted, so ordered containers cannot match a stale handle either. Two reverse index maps are deleted, removing the duplicated state that made a recycled id able to inherit a stale tag.

**Tech Stack:** C++17, SDL2, igloo (`igloo/igloo_alt.h`), GNU make (`Makefile.debian`).

**Spec:** `docs/superpowers/specs/2026-08-30-entity-generation-counter-design.md`

## Global Constraints

- **Layout changes are expected and must be deliberate.** This is the wave where `sizeof` moves. Task 1 pins today's values; every later task that changes one updates the pin in the same commit, with the new number stated in the commit message. A layout change that arrives without a pin update is the defect this ordering exists to prevent.
- **Generation 0 is reserved and never valid.** `generations` is initialised to 1. A hand-built `Entity(id)` therefore carries generation 0 and is stale by construction, so no fabricated handle can match a live entity.
- Stale-handle use **no-ops and logs**; it never throws or aborts. The Switch build compiles `-fno-exceptions`.
- Diagnostics use `EcsShouldReport` with a call-site-owned `static thread_local unsigned int` counter and append `EcsSuppressionNote(counter)`, matching the convention established in wave one.
- **Test output must be pristine.** Zero compiler warnings.
- The engine has **no header dependency tracking**: run `make -f Makefile.debian clean` before any build following a header edit.
- Baseline at the start: **413 tests run, 413 succeeded, 0 failed, 0 warnings.**
- **A test asserting a stale handle still exists proves nothing.** It must assert the operation was *rejected*. Wave one shipped four checks that passed while the thing they checked was broken, each asserting a proxy — a count, a log line, a build succeeding — rather than the property.

---

## File Structure

- `common/ecs.h` — `Entity`'s new member, comparison operators, `friend class Registry`; `Registry`'s `generations` vector, the deleted reverse maps, the `EntityOrder` comparator, `GetEntitiesToBeKilled`'s return type.
- `common/ecs.cpp` — `CreateEntity` stamps, `Update` bumps, `IsAlive` becomes exact, the five tag/group methods lose their reverse-map probes.
- `common/systems/contact.h:132` — explicit comparator for `std::sort(live…)`.
- `specs/layout.spec.cpp` — **new**, the layout pin.
- `specs/ecs.spec.cpp`, `specs/registry.spec.cpp` — flip the two deliberately-wrong cases, add rejection coverage.

---

### Task 1: Pin the current layout

This lands **first**, with today's numbers, so that every later task has something to update deliberately. Without it there is nothing to notice a layout change against — which is exactly how `sizeof(AssetStore)` went 112 → 208 in 1.3.0 with no warning.

**Files:**
- Create: `specs/layout.spec.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: a spec that fails whenever a public type's size changes

- [ ] **Step 1: Write the pin**

`Makefile.debian:70` globs specs with `$(shell find specs -name '*.cpp')`, so a new file is picked up automatically — no Makefile edit.

```cpp
#include "../common/ecs.h"
#include "../common/tilemapLoader.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

// These sizes are ABI. Games allocate Registry and AssetStore themselves and
// iterate std::vector<Tile> directly, so a size change is emitted at *their*
// call site and overflows *their* allocation, with no diagnostic anywhere.
// That is how 1.3.0's AssetStore change (112 -> 208) reached users.
//
// A failure here is not a bug in this file. It means a public type changed
// size: decide whether that was intended, and if it was, update the number
// here in the same commit and say so in the commit message.
//
// Measured on x86-64, g++, -std=c++17.
Describe(LayoutSpec) {
  It(pins_the_sizes_that_are_abi) {
    Assert::That(sizeof(Registry), Equals(static_cast<std::size_t>(576)));
    Assert::That(sizeof(Entity), Equals(static_cast<std::size_t>(16)));
    Assert::That(sizeof(System), Equals(static_cast<std::size_t>(32)));
    Assert::That(sizeof(Signature), Equals(static_cast<std::size_t>(8)));
    Assert::That(sizeof(Tile), Equals(static_cast<std::size_t>(80)));
  };
};
```

- [ ] **Step 2: Run it and confirm it passes as written**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: **414 tests, 414 succeeded, 0 warnings.**

If any assertion fails, **do not adjust the number to match**. The pin's whole value is that it disagrees when something moved. Report the actual size and stop — it means the branch already changed a layout, which nothing so far should have.

- [ ] **Step 3: Prove the pin actually fires**

Temporarily add a `char pad_;` member to `Entity` in `common/ecs.h`, rebuild clean, and confirm `pins_the_sizes_that_are_abi` **fails**. Remove the member and rebuild. Paste both results into your report.

A pin that cannot fail is worse than no pin, and this release has already shipped four checks with that defect.

- [ ] **Step 4: Commit**

```bash
git add specs/layout.spec.cpp
git commit -m "Pin the sizes that are ABI

Games allocate Registry and AssetStore themselves, so a size change is
emitted at their call site with no diagnostic. Pinning the numbers makes
every later layout change a deliberate edit rather than a silent one."
```

---

### Task 2: `Entity` carries a generation

**Files:**
- Modify: `common/ecs.h` — `Entity`'s member, constructors, `operator==`/`!=`, `friend class Registry`; `Registry`'s `generations` member
- Modify: `common/ecs.cpp` — `CreateEntity`, `Update`'s kill pass, `IsAlive`
- Modify: `specs/layout.spec.cpp` — `sizeof(Entity)` 16 → 24, and `sizeof(Registry)` to its new value
- Test: `specs/ecs.spec.cpp`

**Interfaces:**
- Consumes: Task 1's `specs/layout.spec.cpp`
- Produces:
  - `Entity` with `std::uint32_t generation`, default 0
  - `bool Entity::operator==(const Entity &) const` comparing `(id, generation)`
  - `std::vector<std::uint32_t> Registry::generations` — private, initialised to 1 per id
  - `bool Registry::IsAlive(Entity) const` — now exact and O(1)

- [ ] **Step 1: Write the failing tests**

Append to `specs/ecs.spec.cpp`:

```cpp
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
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: all three FAIL — `IsAlive(first)` returns true, and `first == second` is true, because identity is still the id alone.

- [ ] **Step 3: Give `Entity` the generation**

In `common/ecs.h`:

```cpp
class Entity {
private:
  std::size_t id;
  // 0 is reserved and never valid: Registry::generations starts at 1, so an
  // Entity built from a bare id is stale by construction and cannot be
  // mistaken for a live entity.
  std::uint32_t generation = 0;

  friend class Registry;

public:
  explicit Entity(std::size_t id) : id(id){};
  // ...
  bool operator==(const Entity &other) const {
    return id == other.id && generation == other.generation;
  };
  bool operator!=(const Entity &other) const { return !(*this == other); };
```

Leave `operator<` and `operator>` alone for now — Task 3 removes them, and doing both at once makes two large diffs one unreviewable diff.

Add `#include <cstdint>` to `common/ecs.h`.

`friend class Registry` is needed because `generation` is private and `Registry` stamps it. `registry` is already a public member, so the friendship is only for the generation.

- [ ] **Step 4: Give `Registry` the generations vector**

In `common/ecs.h`, beside `entityComponentSignatures`:

```cpp
  // Generation per entity id, parallel to entityComponentSignatures. Starts
  // at 1 so that generation 0 can mean "never valid" — see Entity.
  std::vector<std::uint32_t> generations;
```

- [ ] **Step 5: Stamp, bump, and check**

`Registry::CreateEntity` — after `entityComponentSignatures.resize(entityId + 1)`, keep `generations` the same length, and stamp the entity:

```cpp
  if (entityId >= generations.size()) {
    generations.resize(entityId + 1, 1);   // 1, not 0: 0 means never valid
  }

  Entity entity(entityId);
  entity.generation = generations[entityId];
  entity.registry = this;
```

`Registry::Update`'s kill pass — bump when the id is freed, which is the only place an id becomes reusable:

```cpp
    // Bump before the id is reusable: every handle to the old entity becomes
    // detectably stale at exactly the moment the id can be handed out again.
    ++generations[entity.GetId()];

    // Make the entity id available to be reused
    freeIds.push_back(entity.GetId());
```

`Registry::IsAlive` — exact, and O(1) rather than a `freeIds` scan:

```cpp
bool Registry::IsAlive(Entity entity) const {
  const auto entityId = entity.GetId();
  if (entityId >= numEntities || entityId >= generations.size()) {
    return false;
  }
  return entity.generation == generations[entityId];
}
```

- [ ] **Step 6: Update the layout pin, deliberately**

`sizeof(Entity)` is now 24. `sizeof(Registry)` has grown by one `std::vector`. Build a one-off probe to read the real numbers rather than predicting them:

```sh
printf '#include "common/ecs.h"\n#include <cstdio>\nint main(){printf("Entity %%zu Registry %%zu\\n",sizeof(Entity),sizeof(Registry));}' > /tmp/p.cpp
g++ -std=c++17 -I. /tmp/p.cpp -o /tmp/p && /tmp/p
```

Put the measured values in `specs/layout.spec.cpp`. State both old and new numbers in the commit message.

- [ ] **Step 7: Run the tests**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: the three new cases PASS, the layout pin passes with its new numbers, zero warnings.

Some pre-existing cases may now fail — those are the ones that depend on a stale handle being indistinguishable. **Do not fix them here.** List them in your report; Task 5 owns them.

- [ ] **Step 8: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/ecs.spec.cpp specs/layout.spec.cpp
git commit -m "Give Entity a generation, so a recycled id no longer aliases a dead handle

sizeof(Entity) 16 -> 24, sizeof(Registry) grows by one vector; layout pin
updated in this commit. Generation 0 is reserved as never-valid, so an
Entity built from a bare id cannot match a live one."
```

---

### Task 3: Delete `operator<` and `operator>`

Equality alone does not close the hole: `std::set` finds elements through `<`, not `==`, so a stale handle would still locate a live entity's entry in `entitiesPerGroup`. Ordering by entity id also leaks allocation order into game logic and was never meaningful.

**Files:**
- Modify: `common/ecs.h` — remove both operators, add `EntityOrder`, retype the three sets and `GetEntitiesToBeKilled`
- Modify: `common/ecs.cpp` — `GetEntitiesToBeKilled`'s body, `entitiesPerGroup.emplace` at line 494
- Modify: `common/systems/contact.h:132` — explicit comparator
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Consumes: Task 2's generation-aware `Entity`
- Produces:
  - `struct EntityOrder` — strict weak ordering on `(id, generation)`
  - `std::vector<Entity> Registry::GetEntitiesToBeKilled() const`

- [ ] **Step 1: Write the failing test**

Append to `specs/registry.spec.cpp`:

```cpp
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
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: FAIL — `EntityBelongsToGroup(first, …)` returns true, because the set located the entry by id-only `<`.

- [ ] **Step 3: Add the comparator and delete the operators**

In `common/ecs.h`, after `Entity`:

```cpp
// Ordering for the containers that need one. Named for what it orders: this
// is not "by id", and calling it that is how the deleted operator< gets
// quietly reintroduced.
struct EntityOrder {
  bool operator()(const Entity &a, const Entity &b) const {
    if (a.GetId() != b.GetId()) {
      return a.GetId() < b.GetId();
    }
    return a.GetGeneration() < b.GetGeneration();
  }
};
```

This needs a public reader for the generation. Add to `Entity`:

```cpp
  std::uint32_t GetGeneration() const { return generation; }
```

Delete `operator<` and `operator>` from `Entity`.

- [ ] **Step 4: Retype the containers**

In `common/ecs.h`:

```cpp
  std::set<Entity, EntityOrder> entitiesToBeAdded;
  std::set<Entity, EntityOrder> entitiesToBeKilled;
  // ...
  std::unordered_map<std::string, std::set<Entity, EntityOrder>> entitiesPerGroup;
```

`GetEntitiesToBeKilled` returns a vector — the set-ness was an implementation detail of the flush queue leaking into the public API:

```cpp
  // The entities queued for reaping at the next Update(). Order is unspecified.
  std::vector<Entity> GetEntitiesToBeKilled() const;
```

with the body moved to `common/ecs.cpp`:

```cpp
std::vector<Entity> Registry::GetEntitiesToBeKilled() const {
  return std::vector<Entity>(entitiesToBeKilled.begin(),
                             entitiesToBeKilled.end());
}
```

`common/ecs.cpp:494`'s `entitiesPerGroup.emplace(group, std::set<Entity>())` becomes `std::set<Entity, EntityOrder>()`.

- [ ] **Step 5: Fix `ContactSystem`**

`common/systems/contact.h:132` is `std::sort(live.begin(), live.end());`, which used `operator<`. Give it the comparator:

```cpp
      std::sort(live.begin(), live.end(), EntityOrder{});
```

The three `.GetId()` comparisons at `contact.h:90,113,114` are explicit and unaffected — leave them.

- [ ] **Step 6: Run the tests, and read every compile error**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`

Every error is a place that was ordering entities implicitly. Fix each by supplying `EntityOrder` explicitly. **Never fix one by restoring `operator<`.** If you find a site where explicit ordering is genuinely wrong, that is a finding — report it and stop.

Expected once clean: the new case PASSES, zero warnings.

- [ ] **Step 7: Commit**

```bash
git add common/ecs.h common/ecs.cpp common/systems/contact.h specs/registry.spec.cpp
git commit -m "Delete Entity's relational operators; order explicitly where needed

Equality alone does not close the stale-handle hole: std::set finds
elements through <, not ==, so id-only ordering let a stale handle locate
a live entity's group entry. GetEntitiesToBeKilled now returns a vector."
```

---

### Task 4: Delete the reverse index maps

`tagPerEntity` and `groupPerEntity` duplicate state that must be kept in sync with `entityPerTag` and `entitiesPerGroup`, and both key on a raw `int` id — the one place a recycled id can inherit a stale tag. They are correct today only because kill-time cleanup runs on every path that frees an id.

**Files:**
- Modify: `common/ecs.h` — delete both members
- Modify: `common/ecs.cpp` — `TagEntity`, `EntityHasTag`, `RemoveEntityTag`, `GroupEntity`, `EntityBelongsToGroup`, `RemoveEntityGroup`
- Modify: `specs/layout.spec.cpp` — `sizeof(Registry)` shrinks
- Test: `specs/registry.spec.cpp`

**Interfaces:**
- Consumes: Tasks 2 and 3
- Produces: no public API change — only the internal representation

- [ ] **Step 1: Write the failing test**

```cpp
Describe(TagCleanupSpec) {
  It(should_not_let_a_recycled_id_inherit_a_tag) {
    Registry registry;
    Entity first = registry.CreateEntity();
    first.Tag("player");
    registry.Update();

    first.Kill();
    registry.Update();

    Entity second = registry.CreateEntity();   // same id
    Assert::That(registry.EntityHasTag(second, "player"), Equals(false));
    Assert::That(registry.DoesTagExist("player"), Equals(false));
  };
}
```

This should pass before the change too — the cleanup already works. It is here to stay green *through* the change, which is the point: it pins the invariant so the refactor cannot quietly break it.

- [ ] **Step 2: Confirm it passes now**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: PASS. If it fails, stop — the invariant is already broken and that is a separate finding.

- [ ] **Step 3: Rewrite the six methods without the reverse maps**

Read each method in `common/ecs.cpp` before editing. The shapes:

- `EntityHasTag(entity, tag)` — the tag is already in hand, so this is an `entityPerTag` probe and stays O(1): find `tag`, compare the stored `Entity` to `entity` with `==` (now generation-aware).
- `EntityBelongsToGroup(entity, group)` — `entitiesPerGroup.find(group)`, then `count(entity)` on the set. Also O(log n), no scan.
- `RemoveEntityTag(entity)` — scan `entityPerTag` for the value equal to `entity`, erase that key. Runs once per entity at kill time.
- `RemoveEntityGroup(entity)` — scan `entitiesPerGroup`, erase `entity` from whichever set holds it.
- `TagEntity(entity, tag)` — the "untag the previous holder" step becomes a `RemoveEntityTag(entity)` call plus erasing any existing holder of `tag`.
- `GroupEntity(entity, group)` — no reverse map to update.

Then delete both members from `common/ecs.h`.

- [ ] **Step 4: Update the layout pin**

`sizeof(Registry)` shrinks by two `unordered_map`s. Measure with the probe from Task 2 Step 6 and put the real number in `specs/layout.spec.cpp`. State old and new in the commit message.

- [ ] **Step 5: Run the tests**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: everything passes including the cleanup case from Step 1, zero warnings.

- [ ] **Step 6: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/layout.spec.cpp
git commit -m "Delete the reverse tag and group index maps

They duplicated state that had to be kept in sync and keyed on a raw id,
which is the one place a recycled id could inherit a stale tag. Correct by
construction now rather than by discipline. sizeof(Registry) shrinks."
```

---

### Task 4b: Reject stale handles at component access

Task 2 makes `IsAlive` exact, which is enough for `KillEntity` — it already calls
`IsAlive` and so rejects a stale kill for free. **Component access is not covered.**
`Registry::GetComponent` and friends index the component pools by id, so a stale
handle still reads the live entity's components. That is the same aliasing bug one
layer down.

**Files:**
- Modify: `common/ecs.h` — the `Entity` forwarders and `Registry::FindComponent`
- Test: `specs/ecs.spec.cpp`

**Interfaces:**
- Consumes: Task 2's `Registry::IsAlive`
- Produces: no API change — a stale handle now misses instead of aliasing

- [ ] **Step 1: Write the failing test**

```cpp
It(should_not_read_the_live_entitys_components_through_a_stale_handle) {
  Registry registry;
  Entity first = registry.CreateEntity();
  first.Kill();
  registry.Update();

  Entity second = registry.CreateEntity();     // same id, new generation
  registry.AddComponent<SpecMana>(second, 77);
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
```

Use whatever component type the surrounding file already defines rather than adding
one — each new type costs one of 32 process-wide component ids.

- [ ] **Step 2: Run to verify it fails**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: FAIL — the stale handle reads `77`, because the pool is indexed by id and
nothing consults the generation.

- [ ] **Step 3: Add the staleness check where the miss reasons already live**

`Registry::FindComponent` is the shared implementation behind `TryGetComponent` and
`GetComponent`, and it already reports a reason through the `ComponentMiss` enum.
**Read it before editing** — it has three existing bounds checks and a `ComponentMiss`
value for each.

Add a staleness check alongside them, with its own `ComponentMiss` value (`Stale`), so
the existing diagnostic names the real reason rather than reporting a generic miss.
`ComponentMissDescription` in `common/ecs.cpp` gains the matching string. Putting it
here rather than in each forwarder means one check covers `TryGetComponent`,
`GetComponent` and `HasComponent` at once.

- [ ] **Step 4: Run the tests**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: the new case PASSES, zero warnings.

Watch for pre-existing cases that construct an `Entity` by hand and then read a
component — those now correctly miss. Each is either a spec that needs a real entity,
or a genuine find. Report which, and do not "fix" one by weakening the check.

- [ ] **Step 5: Commit**

```bash
git add common/ecs.h common/ecs.cpp specs/ecs.spec.cpp
git commit -m "Reject component access through a stale handle

IsAlive covers KillEntity, but the component pools are indexed by id, so a
stale handle still read the live entity's components. The check goes in
FindComponent, which is the one place all three accessors share."
```

---

### Task 5: Flip the specs that pinned the old behaviour, and cover rejection

Two cases deliberately pin the bug and carry comments saying a breaking release must update them: `specs/ecs.spec.cpp:278` (`registry.KillEntity(a); // stale handle — double kill`) and the case at `specs/ecs.spec.cpp:551` (`should_still_destroy_the_new_entity_through_a_recycled_stale_handle`). A comment at `specs/registry.spec.cpp:341` says the same.

**Files:**
- Modify: `specs/ecs.spec.cpp`, `specs/registry.spec.cpp`

**Interfaces:**
- Consumes: Tasks 2, 3, 4
- Produces: no API change

- [ ] **Step 1: Read all three sites and decide, per case, what it should now assert**

Each case pins a behaviour that is now fixed. Rename it to say what it checks now — `should_still_destroy_the_new_entity_through_a_recycled_stale_handle` becomes something like `should_reject_a_kill_through_a_recycled_stale_handle` — and carry the original comment's intent across so the record of what was fixed survives.

**Do not delete these cases.** They are the record that the engine once behaved the other way.

- [ ] **Step 2: Make each assert rejection, not existence**

The failure this most invites: asserting that the live entity still exists. That passes whether or not the stale kill was rejected, because nothing else killed it. Each case must assert the *rejection* — that `IsAlive` reports false for the stale handle, that the live entity is still alive **and** still has its components, and that the diagnostic fired.

```cpp
It(should_reject_a_kill_through_a_recycled_stale_handle) {
  Registry registry;
  Entity first = registry.CreateEntity();
  first.Kill();
  registry.Update();

  Entity second = registry.CreateEntity();      // same id, new generation
  registry.AddComponent<SpecMana>(second, 42);
  registry.Update();

  Logger::messages.clear();
  first.Kill();                                  // stale handle
  registry.Update();

  Assert::That(registry.IsAlive(second), Equals(true));
  Assert::That(registry.IsAlive(first), Equals(false));
  // Not just alive — intact. A kill that half-succeeded would still be alive.
  Assert::That(registry.GetComponent<SpecMana>(second).value, Equals(42));
  Assert::That(SpecErrorCount(),
               Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
  Logger::messages.clear();
};
```

Adapt the component type to whatever the surrounding file already defines rather than declaring a new one — every new component type costs one of 32 process-wide ids.

- [ ] **Step 3: Prove each rewritten case fails against the old behaviour**

For each case, temporarily make `IsAlive` return `entityId < numEntities` (the pre-generation behaviour), rebuild, and confirm the case fails. Restore.

This is the step that distinguishes a test from a decoration, and it is not optional. Paste the before and after for each.

- [ ] **Step 4: Run the full suite**

Run: `make -f Makefile.debian clean && make -f Makefile.debian test`
Expected: everything passes, zero warnings. Report the final count and the delta from 413.

- [ ] **Step 5: Commit**

```bash
git add specs/ecs.spec.cpp specs/registry.spec.cpp
git commit -m "Flip the specs that pinned the stale-handle bug

They asserted the wrong behaviour deliberately and carried comments saying
a breaking release must update them. Each now asserts the operation is
rejected rather than that the live entity survived — surviving is true
either way."
```

---

## Verification before calling this done

- [ ] `make -f Makefile.debian clean && make -f Makefile.debian test` — report the count and the delta from 413.
- [ ] The layout pin's final numbers match a fresh probe build, and every change to it is explained in a commit message.
- [ ] `grep -rn "operator<\|operator>" common/ecs.h` shows neither on `Entity`.
- [ ] `grep -rn "tagPerEntity\|groupPerEntity" common/` is empty.
- [ ] A stale handle misses on component access as well as on kill — the two are separate paths and only one is covered by `IsAlive`.
- [ ] Every rewritten spec case was proven to fail against the old behaviour.
