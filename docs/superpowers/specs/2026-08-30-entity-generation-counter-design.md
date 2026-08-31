# Entity generation counter

**Status:** approved for planning
**Target:** 2.0.0, layout wave
**Fixes:** `KNOWN_ISSUES.md` item 1

## Problem

`Entity` is `{ std::size_t id; Registry *registry; }`. Ids are recycled — `KillEntity`
returns the id to a free list and the next `CreateEntity` hands it straight back.

So a handle kept past its entity's death is **bit-for-bit identical** to the new
entity holding that id. `operator==` compares the id alone, the liveness check
reports it alive, and a kill through the stale handle destroys the wrong object:

```cpp
Entity bullet = registry.CreateEntity();   // id 7
bullet.Kill();
registry.Update();                          // id 7 returns to the free list
Entity pickup = registry.CreateEntity();   // id 7 again — a different entity
bullet.Kill();                              // destroys `pickup`
```

This needs a layout change, which is why it waited for a major.

## Design

### `Entity`

```cpp
{ std::size_t id; std::uint32_t generation; Registry *registry; }
```

`sizeof(Entity)` 16 → 24.

- `operator==` / `operator!=` compare **`(id, generation)`**. A stale handle is never
  equal to the live entity holding its id.
- **`operator<` and `operator>` are deleted.** Ordering entities by id leaks
  allocation order into game logic and was never meaningful. Where ordering is
  genuinely required it becomes explicit at the point of use.

Deleting the relational operators is what makes the fix structural rather than
partial. `std::set` lookups go through `<`, not `==`, so leaving id-only ordering
would let a stale handle still find a live entry in an ordered container — the bug
surviving one layer down.

### `Registry`

Add `std::vector<std::uint32_t> generations`, indexed by id, incremented when an id
is returned to `freeIds`. An entity is stale when
`entity.generation != generations[entity.id]`.

**Delete `tagPerEntity` and `groupPerEntity`.** Both are reverse indexes duplicating
state that must be kept in sync with `entityPerTag` / `entitiesPerGroup`, and both key
on a raw `int` id — the one place a recycled id can inherit a stale tag. Today they
are correct only because kill-time cleanup runs on every path that frees an id.
Removing them makes that correct by construction instead.

The cost is smaller than it looks, because most lookups were never reverse lookups:

| Operation | Before | After |
|---|---|---|
| `EntityHasTag(entity, tag)` | reverse map probe | `entityPerTag` probe — still O(1), the tag is already in hand |
| `EntityBelongsToGroup(entity, group)` | reverse map probe | `entitiesPerGroup[group]` lookup |
| `RemoveEntityTag(entity)` | reverse map probe | scan `entityPerTag` |
| `RemoveEntityGroup(entity)` | reverse map probe | scan `entitiesPerGroup` |

Only the two removal paths become scans, and each also runs on the *creation* side
(`TagEntity`/`GroupEntity` call the matching removal first, to drop any previous
tag/group), not just at kill time. `entityPerTag`/`entitiesPerGroup` hold one entry
per distinct tag/group name, not one per entity, so this is a scan of the tag/group
count. Exactly one in-tree consumer touches the reverse
direction at all (`examples/jrpg/src/states/playState.cpp:556`, `HasTag("player")`),
and that one is an O(1) probe after the change.

The three `std::set<Entity>` members — `entitiesToBeAdded`, `entitiesToBeKilled`,
`entitiesPerGroup` — take a named comparator ordering by
`(id, generation)`, so the ordering is visible rather than implied by a deleted
operator. Name it for what it orders — `EntityOrder`, not `EntityIdLess`, since it
is not ordering by id alone and a misleading name here is how the deleted operator
gets quietly reintroduced.

`sizeof(Registry)` changes: two members removed, one added. The layout pin is updated
deliberately as part of this work.

### Staleness detection

`IsAlive` becomes exact **and cheaper**: `id < numEntities && entity.generation ==
generations[id]`, which is O(1) where the current implementation scans `freeIds`.

Every `Entity` forwarder already null-checks `registry`; the generation check joins it
and reports through the throttled `EcsShouldReport` convention established in wave one
— no-op and log, never abort. The Switch build compiles `-fno-exceptions`, so throwing
is not available.

## Breaking changes

Four, all compile-time and loud:

1. `sizeof(Entity)` 16 → 24 — requires a rebuild, not a relink.
2. `operator<` / `operator>` deleted — any `std::set<Entity>`, `std::map<Entity, …>` or
   bare `std::sort` over entities in game code stops compiling.
3. `Registry::GetEntitiesToBeKilled()` returns `std::vector<Entity>` rather than
   `std::set<Entity>`. The set-ness was an implementation detail of the flush queue
   leaking into the public API; a vector says what callers actually get.
4. `ContactSystem`'s `std::sort(live.begin(), live.end())` (`common/systems/contact.h:132`)
   needs an explicit comparator.

`contact.h:90,113,114` compare `.GetId()` explicitly and are unaffected. Every example's
entity sort passes a lambda over component values (zIndex, y-position) and is unaffected.

## Testing

The two specs that deliberately pin the current wrong behaviour get flipped —
`specs/ecs.spec.cpp:278` and the case at `specs/ecs.spec.cpp:551`, both of which carry
comments saying a breaking release must update them.

**A test that asserts a stale handle still exists proves nothing.** It has to assert the
operation was *rejected*: that the live entity survives, that `IsAlive` reports false for
the stale handle, and that the diagnostic fired. Wave one shipped four checks that passed
while the thing they checked was broken, each asserting on a proxy — a count, a log line,
a build succeeding — rather than the property. This is the task where that mistake is
easiest to repeat.

Coverage required:

- kill through a stale handle after id recycling — the live entity survives
- `IsAlive` false for a stale handle, true for the live one holding the same id
- a stale handle does not compare equal to the live entity
- a stale handle does not find the live entity's entry in a group
- tag and group cleanup still correct with the reverse maps gone
- `generations` grows correctly as ids are recycled repeatedly

## Out of scope

The rest of the layout wave — `System`'s disabled latch, `Tile`'s animation fields,
`MAX_COMPONENTS` 32 → 64, and `namespace storm` — are separate items in
`docs/ROADMAP.md`. Namespacing goes last, because it rewrites nearly every line it
touches and would make every other diff in the wave unreadable.
