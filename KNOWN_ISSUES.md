# Known Issues

Defects that are **real, understood, and deliberately not fixed in the 1.x line**, because every one of them requires breaking source or binary compatibility for games already built on the engine.

The 1.x public API is frozen. A game that compiles and links against 1.2.x must keep compiling and linking against every later 1.x release, which rules out changing a public signature, changing the layout of a type a game embeds or passes by value, or deleting a public member. Each entry below explains the defect, why the fix cannot be made without breaking that promise, and what to do in the meantime.

Everything here is a candidate for **Storm! Engine v3**, where the compatibility promise resets. Items that *can* be fixed without a break live in the tech-debt ledger instead, not in this file.

Measured on the current tree (x86-64, g++ 9, `-std=c++17`): `sizeof(Entity)` 16, `sizeof(Signature)` 8, `sizeof(System)` 32, `sizeof(Registry)` 576, `sizeof(Tile)` 80.

## 1. A stale `Entity` handle can kill a different, live entity

`Entity` is `{ std::size_t id; Registry *registry; }` and nothing more. Ids are recycled: `KillEntity` returns the id to a free list and the next `CreateEntity` hands it straight back out.

So a handle you kept past its entity's death is **bit-for-bit identical** to the new entity holding that id. `operator==` and `operator<` compare the id alone, the liveness check reports it alive, and the kill destroys the wrong object.

```cpp
Entity bullet = registry.CreateEntity();   // id 7
bullet.Kill();
registry.Update();                          // id 7 returns to the free list
Entity pickup = registry.CreateEntity();   // id 7 again — a different entity
bullet.Kill();                              // destroys `pickup`
```

**Why it stays.** The fix is a generation counter — `{ id, generation, registry }` — checked on every access. That takes `sizeof(Entity)` from 16 to 24. Games store `Entity` by value in their own containers and every `System` holds a `std::vector<Entity>`, so the layout is thoroughly baked into compiled game code.

**Meanwhile.** Do not keep an `Entity` past the frame in which it might die. Re-look it up by tag or group, or null your own references when you kill something. Two specs (`specs/ecs.spec.cpp`, `specs/registry.spec.cpp`) pin this wrong behaviour deliberately, each carrying a comment to flip them when v3 fixes it.

## 2. A bare integer implicitly converts to an `Entity`

`Entity(std::size_t)` is not `explicit`, so any function taking an `Entity` silently accepts a number:

```cpp
registry.KillEntity(88);        // compiles. 88 is not an entity.
```

Every `Entity` member now null-checks its registry pointer, so this no-ops and logs loudly instead of dereferencing garbage — but it should never have compiled.

**Why it stays.** Adding `explicit` is a source break for any out-of-tree game that relies on the conversion. Nothing in this repo does (`grep -rnE 'KillEntity\([0-9]|TagEntity\([0-9]' examples/ editor/ common/` is empty), and it would break loudly at compile time rather than silently — but "loudly" is still a break, and 1.x promises not to.

## 3. Thirty-two component types, process-wide

`MAX_COMPONENTS` is 32 and `Signature` is `std::bitset<32>`. Type ids come from one process-wide counter, so the cap is per binary, not per `Registry`. See the README's *Component type limit* section for the full explanation and how to budget against it.

**Why it stays.** `Signature` is `std::bitset<MAX_COMPONENTS>`; two translation units compiled with different values disagree about what type `Signature` *is*, which is an ODR violation. Worse, `sizeof(std::bitset<N>)` is 8 bytes for every N from 1 to 64, so raising 32 → 64 changes no layout and no size check catches a stale object file — the mismatch is silent. Shipping that as a 1.x point release would corrupt games that did not rebuild.

**Meanwhile.** Prefer widening a component (a `kind` enum) over declaring a new one. Overflow is reported on the error log and the type is ignored rather than throwing, so it will not abort under `-fno-exceptions`.

## 4. A system that overflows the component cap matches *every* entity

Following from the above: when `RequireComponent<T>()` is called for a type past the cap, the requirement is dropped and the system's signature stays empty. Membership is `(entitySignature & systemSignature) == systemSignature`, which is true for **every** entity against an empty signature. A system that should have matched nothing instead runs on the whole world.

It is memory-safe and loudly logged, and only reachable once you are already past 32 types — but the failure direction is wrong.

**Why it stays.** The clean fix is a `disabled_` latch on `System`, changing `sizeof(System)` from 32. Games subclass `System` and the `Registry` holds them by `shared_ptr`, so the layout is part of the ABI.

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

**Why it stays.** The fix adds fields to `Tile`, changing `sizeof(Tile)` from 80. `Map` is `std::vector<Tile>` and games iterate it directly, so a game built against the old header walking a vector produced by a new library reads misaligned garbage. It is an ABI break with no compile-time warning — the most dangerous kind.

**Meanwhile.** Drive tile animation from game code with `AnimationComponent`, not from the editor's fields.

## 8. Including a state header compiles the entire engine

`common/states/gameState.h` transitively pulls in SDL2, every component, every system, the asset store, the logger and the tilemap loader — roughly 713 headers and 145,000 preprocessed lines — to declare a 23-line interface. Every translation unit that touches a state pays it.

**Why it stays.** Trimming the includes to what the header actually needs is correct, but consuming games currently get SDL and the component headers *transitively* through it. Cutting them turns every game that relied on that into a wall of compile errors. Loud, fixable in a line or two per game — still a source break.

**Meanwhile.** Include what you use in your own headers rather than leaning on the transitive path; that also makes the v3 upgrade a no-op for you.

## 9. Every engine type is a global symbol

No public type in `common/` sits in a named namespace. `Entity`, `Registry`, `System`, `Logger`, `Tile`, `TouchZone` and the rest are all global, and the installed headers land in `/usr/local/include/stormengine2/` with the include path as the only qualification. (The one anonymous namespace in the tree, in `netPacket.cpp`, is file-local implementation detail and unrelated.)

A game that declares its own `Entity` or `Logger` collides.

**Why it stays.** Introducing `namespace storm { }` breaks every single line of every consuming game.

**Meanwhile.** Prefix your own types, or wrap yours in a namespace of your own.

## 10. Collision only kills; there is no event bus

`CollisionSystem` responds to an overlap by calling `Kill()` on both entities when they carry a `RigidBodyComponent`. There is no callback, no event queue, no way to observe a collision without acting on it. `common/systems/collision.h:32` carries the `// TODO: emit an event` marking the gap.

Any game needing collision *response* — bouncing, damage, triggers, pickups — has to hand-roll its own overlap pass. That is not a defect in those games; it is the documented state of the engine.

**Why it stays.** An event bus is new architecture, and changing what `CollisionSystem::Update()` does to entities is a silent behaviour break for anything relying on the current kill semantics.

## Also on the v3 list

Not defects exactly — design decisions worth revisiting when compatibility is no longer binding:

- **Component storage is a dense `std::vector<T>` per type, indexed by entity id.** Memory per registered type is O(highest id ever used), not O(live entities), and every component type must be default-constructible. A sparse set would fix both.
- **No system scheduler.** Each system declares its own non-virtual `Update` with a bespoke signature, and the game calls each by name in an order it chooses. Ordering bugs are invisible until they bite.
- **Two registry idioms coexist** — the editor uses the `Registry::Instance()` singleton, games own a `Registry` per state. Pick one.
- **`GameStateMachine` owns raw pointers** with implicitly generated copy operations, so copying one double-frees every state.
- **Frame pacing lives in game code**, not the engine — every state re-implements the same `SDL_Delay` budget against `MILLISECS_PER_FRAME`.
- **Three member-naming schemes** across the engine (bare, `m_`, trailing underscore) and two method casings (PascalCase in the ECS, camelCase in the state machine).

*Items that can be fixed without breaking compatibility are tracked separately and are not listed here.*
