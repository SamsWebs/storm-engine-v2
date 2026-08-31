# Known Issues

Defects that are **real, understood, and deliberately not fixed in the 1.x line**, because every one of them requires breaking source or binary compatibility for games already built on the engine.

The 1.x public API is frozen. A game that compiles and links against 1.2.x must keep compiling and linking against every later 1.x release, which rules out changing a public signature, changing the layout of a type a game embeds or passes by value, or deleting a public member. Each entry below explains the defect, why the fix cannot be made without breaking that promise, and what to do in the meantime.

Everything here is a candidate for **2.0.0**, where the compatibility promise resets. Items that *can* be fixed without a break live in the tech-debt ledger instead, not in this file.

**"2.0.0" is a RELEASE, not a new engine.** The product is Storm! Engine **v2** — that is the repo name, the package name (`libstormenginev2`) and the include path (`stormengine2/`), and none of them change. What changes is the semantic version, which is at 1.3.0 today. So the breaking release is *Storm! Engine v2, version 2.0.0*, and the two "2"s mean different things: the one in the name is the product generation, the one in the version is the compatibility epoch.

This file previously called that release "Storm! Engine v3", which implied a whole new product line and a new package to install alongside the old one. It is a version bump, not a fork: games upgrade in place by rebuilding, exactly as 1.2.x → 1.3.0 required.

**One deliberate exception has been taken.** 1.3.0 added font and sound caches to `AssetStore`, moving `sizeof(AssetStore)` from 112 to 208. Games allocate the store themselves via `std::make_unique<AssetStore>()`, so the size is emitted in game code and a 1.2.x binary relinked against a 1.3.0 `.so` overflows its allocation. It was taken knowingly: the `.deb` ships headers and library together, so the supported upgrade path - install the package, rebuild the game - is always consistent. It is recorded here so the next layout change is argued rather than assumed.

Measured on the current tree (x86-64, g++ 13, `-std=c++17`): `sizeof(Entity)` 24, `sizeof(Signature)` 8, `sizeof(System)` 40, `sizeof(Registry)` 488, `sizeof(Tile)` 104, `MAX_COMPONENTS` 64. These are pinned in `specs/layout.spec.cpp`; if this line and that file ever disagree, the file is right.

## 1. A stale `Entity` handle can kill a different, live entity

`Entity` was `{ std::size_t id; Registry *registry; }` and nothing more. Ids are recycled: `KillEntity` returns the id to a free list and the next `CreateEntity` hands it straight back out.

So a handle you kept past its entity's death was **bit-for-bit identical** to the new entity holding that id. `operator==` compared the id alone, the liveness check reported it alive, and the kill destroyed the wrong object.

```cpp
Entity bullet = registry.CreateEntity();   // id 7
bullet.Kill();
registry.Update();                          // id 7 returns to the free list
Entity pickup = registry.CreateEntity();   // id 7 again — a different entity
bullet.Kill();                              // destroys `pickup`
```

**Why it stays.** The fix is a generation counter — `{ id, generation, registry }` — checked on every access. That takes `sizeof(Entity)` from 16 to 24. Games store `Entity` by value in their own containers and every `System` holds a `std::vector<Entity>`, so the layout is thoroughly baked into compiled game code.

**Resolved in 2.0.0.** `Entity` now carries a generation, stamped at creation and bumped when its id is freed. `operator==` compares id and generation together, so `bullet == pickup` above is `false`; `operator<` is deleted outright rather than left comparing id alone, and callers order explicitly via `EntityOrder` where an ordering is needed. `IsAlive` checks the generation, so `bullet.Kill()` above now rejects the stale handle and logs a throttled error instead of destroying `pickup`. `sizeof(Entity)` moved from 16 to 24, as anticipated above. The two specs that used to pin this behaviour deliberately (`specs/ecs.spec.cpp`, `specs/registry.spec.cpp`) are flipped to assert the fix instead.

The gate covers every path that reads, writes or stores entity identity, not only the kill: `AddComponent`, `RemoveComponent`, the component reads, and `TagEntity`, `GroupEntity` and `AddEntityToSystems`. The last three were added after an adversarial review proved the gap was a live-state corruption rather than a lookup oddity — tagging through a stale handle silently stripped the live entity's tag, and a stale entity injected into a system was never removed, since removal only runs for entities the registry reaps.

The generation also skips 0 on wrap. Landing on the reserved value would make every hand-built `Entity(id)` compare equal to whatever live entity next held that id — this defect, in full, under this resolution note. The wrap was first dismissed as unreachable; measured, it is about 1.66 hours on a loop driving `Registry::Update()` flat out, and roughly 50 days for a 1 kHz headless server, because the counter advances once per id per `Update()` call rather than per kill.

## 2. A bare integer implicitly converts to an `Entity`

`Entity(std::size_t)` is not `explicit`, so any function taking an `Entity` silently accepts a number:

```cpp
registry.KillEntity(88);        // compiles. 88 is not an entity.
```

Every `Entity` member now null-checks its registry pointer, so this no-ops and logs loudly instead of dereferencing garbage — but it should never have compiled.

**Why it stays.** Adding `explicit` is a source break for any out-of-tree game that relies on the conversion. Nothing in this repo does (`grep -rnE 'KillEntity\([0-9]|TagEntity\([0-9]' examples/ editor/ common/` is empty), and it would break loudly at compile time rather than silently — but "loudly" is still a break, and 1.x promises not to.

**Resolved in 2.0.0.** `Entity(std::size_t)` is now `explicit`. `registry.KillEntity(88)` no longer compiles; a bare integer must be wrapped in an `Entity` explicitly.

## 3. Thirty-two component types, process-wide

`MAX_COMPONENTS` was 32 and `Signature` was `std::bitset<32>`. Type ids come from one process-wide counter, so the cap is per binary, not per `Registry`. See the README's *Component type limit* section for the full explanation and how to budget against it.

**Why it stayed.** `Signature` is `std::bitset<MAX_COMPONENTS>`; two translation units compiled with different values disagree about what type `Signature` *is*, which is an ODR violation. Worse, `sizeof(std::bitset<N>)` is 8 bytes for every N from 1 to 64, so raising 32 → 64 changes no layout and no size check catches a stale object file — the mismatch is silent. Shipping that as a 1.x point release would corrupt games that did not rebuild.

**Resolved in 2.0.0.** `MAX_COMPONENTS` is 64. No struct moved, which is precisely why it took a major: nothing about the build can detect a translation unit still compiled against 32, so the only safe upgrade is to rebuild the library, the editor and every game against one header in one go. `specs/layout.spec.cpp` pins the value itself alongside the sizes, because the size pins cannot see it.

64 is the last free step. At 65 `std::bitset` becomes 16 bytes and `sizeof(Registry)` and `sizeof(System)` move with it — a second ABI break rather than a recompile.

**Still true.** The cap remains per binary, not per `Registry`, and the five engine components count against it. Prefer widening a component (a `kind` enum) over declaring a new one. Overflow is reported on the error log and the type is ignored rather than throwing, so it will not abort under `-fno-exceptions` — and since 2.0.0 the system that lost the requirement is latched off rather than left matching everything (item 4).

## 4. A system that overflows the component cap matches *every* entity

Following from the above: when `RequireComponent<T>()` is called for a type past the cap, the requirement is dropped and the system's signature stays empty. Membership is `(entitySignature & systemSignature) == systemSignature`, which is true for **every** entity against an empty signature. A system that should have matched nothing instead runs on the whole world.

It is memory-safe and loudly logged, and only reachable once you are already past 32 types — but the failure direction is wrong.

**Why it stayed.** The clean fix is a latch on `System`, changing `sizeof(System)` from 32. Games subclass `System` and the `Registry` holds them by `shared_ptr`, so the layout is part of the ABI.

**Resolved in 2.0.0.** A `RequireComponent<T>()` call that overflows the cap now latches the system off rather than merely dropping the requirement, and `System::IsDisabled()` reports it. A latched system is skipped by entity admission and by the retrofit path (`AdmitExistingEntitiesTo`, `CountEntitiesMissedBySystem`), so it matches **nothing** instead of everything. The latch is one-way: the component id it wanted does not exist, so there is no runtime state that could make the system correct again. `sizeof(System)` is 32 → 40.

## 5. Adding or removing a component never changes system membership

System membership is computed **once per entity**, when `Registry::Update()` flushes `entitiesToBeAdded`. `AddComponent` and `RemoveComponent` only flip signature bits — nothing re-evaluates which systems the entity belongs to.

```cpp
Entity e = registry.CreateEntity();
e.AddComponent<TransformComponent>();
registry.Update();                       // membership decided here
e.AddComponent<SpriteComponent>();       // RenderSystem will never see it
```

This is the single largest correctness trap in the ECS, and it is not obvious from any one file.

**Why it stays.** Re-evaluating on every component change means every `System` needs stable add/remove, which means membership can no longer be a flat `std::vector<Entity>` — that is an ECS redesign, not a patch, and it changes `System`'s layout and behaviour.

**Meanwhile.** Add every component an entity will ever need *before* the `Registry::Update()` that admits it. To change an entity's component set afterwards, kill it and create a replacement.

## 6. Networking objects are copyable and must not be copied

`NetServer`, `NetClient`, `NetConnection` and `NetSocket` all have implicit copy constructors and assignment operators — none are `= delete`d. Each installs send callbacks capturing `this`, and each owns a socket file descriptor.

Copying one gives you two objects whose callbacks point at whichever was copied *from*, and two destructors closing one descriptor.

**Why it stays.** Deleting the copy operations is an API break for any game that stores one by value, returns one from a factory, or puts one in a resizing container. It breaks at compile time — which is exactly the point — but it still breaks.

**Meanwhile.** Hold them by reference or `unique_ptr`, never by value, and never in a `std::vector` that can reallocate. Also note `NetServer` is ~372 KB and `NetClient` ~188 KB — both are far too large for the stack regardless.

**Resolved in 2.0.0.** `NetServer`, `NetClient`, `NetConnection` and `NetSocket` all `= delete` their copy constructor and copy assignment operator. Copying one is now a compile error instead of a dangling callback and a double-close.

## 7. The engine discards the animation data the editor writes

The tile editor writes animation fields into `.map` files. `TileMapLoader` parses them and throws them away, because `Tile` has nowhere to put them:

```cpp
struct Tile {
  glm::ivec2 relativePosition, pixelSrcPosition;
  glm::vec2 scale; int zIndex; std::string assetId;
  bool hasCollider; int colliderW, colliderH;
};
```

Animated tiles therefore render as static ones, and the editor's animation UI has no effect at runtime.

**Why it stayed.** The fix adds fields to `Tile`, changing `sizeof(Tile)` from 80. `Map` is `std::vector<Tile>` and games iterate it directly, so a game built against the old header walking a vector produced by a new library reads misaligned garbage. It is an ABI break with no compile-time warning — the most dangerous kind.

**Resolved in 2.0.0.** `Tile` carries `isAnimated`, `numFrames`, `frameSpeedRate`, `vertical`, `isLooped` and `frameOffset`, named to match `AnimationComponent` so building one is a direct copy:

```cpp
if (tile.isAnimated)
  e.AddComponent<AnimationComponent>(tile.numFrames, tile.frameSpeedRate,
                                     tile.vertical, tile.isLooped,
                                     tile.frameOffset);
```

The same pass found a second field being discarded: `colliderOffset`. The editor has written collider offsets since colliders were added, and the loader read them off the line purely to advance the stream — so a tile whose collider the editor had nudged collided from its unnudged position. `Tile` now carries it.

The engine still does not build the component for you. `TileMapLoader` hands back a `Map` and nothing else; spawning entities stays the game's job.

`sizeof(Tile)` is 80 → 104. The new fields are appended rather than grouped beside the fields they belong with, which costs 8 bytes of padding — deliberately, so that a game constructing a `Tile` positionally still assigns the same eight fields. Reordering would have shifted a `bool` onto `colliderW`, which converts without a diagnostic.

## 8. Including a state header compiles the entire engine

`common/states/gameState.h` transitively pulls in SDL2, every component, every system, the asset store, the logger and the tilemap loader — roughly 713 headers and 145,000 preprocessed lines — to declare a 23-line interface. Every translation unit that touches a state pays it.

**Why it stays.** Trimming the includes to what the header actually needs is correct, but consuming games currently get SDL and the component headers *transitively* through it. Cutting them turns every game that relied on that into a wall of compile errors. Loud, fixable in a line or two per game — still a source break.

**Meanwhile.** Include what you use in your own headers rather than leaning on the transitive path; that also makes the 2.0.0 upgrade a no-op for you.

**A way out landed in 1.3.0.** `common/states/gameStateBase.h` is the same
`GameState` interface without the convenience includes - 80,265 preprocessed
lines against `gameState.h`'s 146,748, a 45% saving per translation unit.
`gameState.h` now includes it and adds the rest, so nothing existing changed
and the two cannot drift. A game that does not want the whole engine in every
state includes the base header and includes what it uses. The defect itself
stays: `gameState.h` still pulls everything, and trimming *that* was ruled out
of scope for 2.0.0 — this release adds an include to that header rather than
removing any. Trimming it is the source break a later breaking release is
for.


## 9. Every engine type is a global symbol

No public type in `common/` sits in a named namespace. `Entity`, `Registry`, `System`, `Logger`, `Tile`, `TouchZone` and the rest are all global, and the installed headers land in `/usr/local/include/stormengine2/` with the include path as the only qualification. (The one anonymous namespace in the tree, in `netPacket.cpp`, is file-local implementation detail and unrelated.)

A game that declares its own `Entity` or `Logger` collides.

**Why it stayed.** Introducing `namespace storm { }` breaks every single line of every consuming game.

**Resolved in 2.0.0.** Every engine type is in `namespace storm`. A game has three ways forward, cheapest first:

1. **Force-include the bridge from your build** — one line, no source edits:

   ```make
   CXXFLAGS += -include stormengine2/compat/global.h
   ```

2. **Include the bridge** in the files that need it: `#include <stormengine2/compat/global.h>`.

3. **Add `using namespace storm;`** after your engine includes, or qualify with `storm::`. This is what the examples, the editor and the starter template do.

`<stormengine2/compat/global.h>` emits a `using` declaration for every public engine name — including the enumerators of the unscoped enums (`LOG_INFO`, `kNetChunkVital` and the rest), which a `using` on the enum type alone does not bring across.

**The bridge exists to be deleted.** It pulls every engine name back into the global namespace, which is exactly the collision the namespace was added to prevent, so a game that keeps it forever has taken none of the benefit. Use it to get green, then remove it and fix the names. A future major will drop the header.

## 10. Collision only kills; there is no event bus

`CollisionSystem` responds to an overlap by calling `Kill()` on both entities when they carry a `RigidBodyComponent`. There is no callback, no event queue, no way to observe a collision without acting on it.

Any game needing collision *response* — bouncing, damage, triggers, pickups — has to hand-roll its own overlap pass. That is not a defect in those games; it is the documented state of the engine.

**Why it stays.** An event bus is new architecture, and changing what `CollisionSystem::Update()` does to entities is a silent behaviour break for anything relying on the current kill semantics.

**Resolved for new code in 1.3.0.** `ContactSystem` (`common/systems/contact.h`) is the observe-without-acting path: it reports overlaps with a normal and penetration depth, fires begin/end callbacks once per pair, and never touches an entity. `CollisionSystem` is unchanged and stays unchanged for the whole 1.x line - the two share one copy of the bounds math via `ContactSystem::BoundsOf`. Deleting `CollisionSystem` is a 2.0.0 item.

**Resolved in 2.0.0.** `CollisionSystem` is deleted. The kill-on-overlap behaviour this entry describes no longer exists in the engine at all; a game that wants entities to die on contact now writes that against `ContactSystem` itself. The entry's other half stays open — there is still no general event bus, only `ContactSystem`'s begin/end callbacks, which cover contacts and nothing else.

## Also on the 2.0.0 list

Not defects exactly — design decisions worth revisiting when compatibility is no longer binding:

- **Component storage is a dense `std::vector<T>` per type, indexed by entity id.** Memory per registered type is O(highest id ever used), not O(live entities), and every component type must be default-constructible. A sparse set would fix both.
- **No system scheduler.** Each system declares its own non-virtual `Update` with a bespoke signature, and the game calls each by name in an order it chooses. Ordering bugs are invisible until they bite.
- **Two registry idioms coexist** — the editor uses the `Registry::Instance()` singleton, games own a `Registry` per state. Pick one.
- ~~**`GameStateMachine` owns raw pointers** with implicitly generated copy operations, so copying one double-frees every state.~~ **Fixed in 2.0.0** — the copy constructor and copy assignment are `= delete`d, the same treatment the networking types got under item 6. It still owns raw pointers; what is gone is the way to duplicate them silently.
- **Frame pacing lives in game code**, not the engine — every state re-implements the same `SDL_Delay` budget against `MILLISECS_PER_FRAME`.
- **Three member-naming schemes** across the engine (bare, `m_`, trailing underscore) and two method casings (PascalCase in the ECS, camelCase in the state machine).
- **The collider offset is not scaled by the transform.** `ContactSystem::BoundsOf` and `RenderColliderSystem::Update` (`common/systems/renderCollider.h:22-26`) both compute `position + offset` while scaling the extents by `transform.scale`. So a collider with `offset = {4, 0}` on an entity at `scale = {2, 2}` starts 4 px from the origin, not 8. The two agree, so nothing is visibly broken today; scaling the offset would be more consistent but silently moves every collider a game has ever authored with a non-unit scale.
- **`ContactSystem`'s broadphase sweeps one axis.** It sorts by `minX` and breaks the inner loop on the first candidate starting past the current right edge, which degrades back to all-pairs for anything stacked in a single column. A uniform grid is the upgrade, and nothing in-repo is near the entity count where it would matter.

*Items that can be fixed without breaking compatibility are tracked separately and are not listed here.*
