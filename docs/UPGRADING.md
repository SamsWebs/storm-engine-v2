# Upgrading to 2.0.0

Written against what 2.0.0 actually shipped, not what was planned for it.

2.0.0 is the release that spends the whole major-version budget at once. Ten
breaking changes land together, deliberately, so that the traps they fix are
gone for good rather than one per release for the next two years.

**Read this first: 2.0.0 requires a rebuild, not a relink.** Four structs
changed size and one constant changed meaning without changing any size at all.
Swapping `libstormenginev2.so` underneath an already-built game corrupts memory
with no warning from the compiler, the linker or the loader.

| | 1.3.x | 2.0.0 |
|---|---|---|
| `sizeof(Registry)` | 576 | 488 |
| `sizeof(Entity)` | 16 | 24 |
| `sizeof(System)` | 32 | 40 |
| `sizeof(Tile)` | 80 | 104 |
| `sizeof(Signature)` | 8 | 8 |
| `MAX_COMPONENTS` | 32 | **64** |

`Signature` is the dangerous row. `sizeof(std::bitset<N>)` is 8 bytes for every
`N` from 1 to 64, so raising the component cap moved nothing — and nothing in
your build can detect a translation unit still compiled against 32. Two objects
disagreeing about `MAX_COMPONENTS` disagree about what type `Signature` *is*.
They link cleanly and misbehave at runtime.

The engine's own suite pins all of it in `specs/layout.spec.cpp`, including the
value of `MAX_COMPONENTS`, because the size assertions cannot see it.

---

## 1. Every engine type moved into `namespace storm`

The largest source break, and the cheapest to fix. Nothing needs editing if you
force-include the compatibility bridge from your build:

```make
CXXFLAGS += -include stormengine2/compat/global.h
```

Or include it per file:

```cpp
#include <stormengine2/compat/global.h>
```

`<stormengine2/compat/global.h>` emits a `using` declaration for every public
engine name, including the enumerators of the unscoped enums (`LOG_INFO`,
`kNetChunkVital`, `kNetControlConnect` and the rest) — a `using` on the enum
type alone does not bring those across.

**The bridge exists to be deleted.** It pulls every engine name back into the
global namespace, which is exactly the collision the namespace was added to
prevent, so a game that keeps it forever has taken none of the benefit. Use it
to get green, then remove it and do what the examples, the editor and the
starter template do:

```cpp
#include <stormengine2/ecs.h>

using namespace storm;
```

A future major removes the bridge.

## 2. `Entity` carries a generation

`sizeof(Entity)` is 16 → 24.

Entity ids are recycled. In 1.x a handle to a killed entity kept working against
whatever entity later took its id:

```cpp
Entity bullet = registry.CreateEntity();
bullet.Kill();
registry.Update();                     // id returns to the free list
Entity pickup = registry.CreateEntity(); // takes the same id
bullet.Kill();                         // 1.x: destroys `pickup`
```

Now `Entity` stamps a generation at creation and bumps it when the id is freed,
so `bullet == pickup` is `false` and the second `Kill()` is rejected with a
throttled error on the log.

**What this breaks in your code:**

- `Entity::operator<` and `operator>` are **deleted**. They compared id alone,
  which silently treats a stale handle as equal to a live one. If you keep
  entities in a `std::set` or call `std::sort` on them, order explicitly:

  ```cpp
  std::set<Entity, EntityOrder> pending;
  std::sort(entities.begin(), entities.end(), EntityOrder{});
  ```

- `Registry::GetEntitiesToBeKilled()` returns `std::vector<Entity>` rather than
  `std::set<Entity>`, since the set needed the ordering that is now gone.

- `IsAlive` is exact and O(1). A handle whose generation does not match is dead,
  full stop.

## 3. `Entity(std::size_t)` is `explicit`

```cpp
registry.KillEntity(88);              // 1.x: compiled, killed entity 88
registry.KillEntity(Entity(88));      // 2.0.0: say it on purpose
```

A bare integer no longer converts. This one is a compile error at every call
site, which is the point.

## 4. `CollisionSystem` is deleted

It only ever killed both entities on overlap, which is not collision *response*
and was the first thing every game had to work around. Use `ContactSystem`,
which reports begin/end contacts through callbacks and leaves the decision to
you:

```cpp
registry.AddSystem<ContactSystem>();
registry.GetSystem<ContactSystem>().SetOnBeginContact(
    [](const Contact &contact) {
      // contact.a, contact.b, contact.normal, contact.depth -- your response,
      // including killing them if that is what the game wants.
    });
registry.GetSystem<ContactSystem>().SetOnEndContact(
    [](const Entity &a, const Entity &b) { /* separation */ });
```

`Contact` carries the pair in a stable order along with the collision normal and
penetration depth, so a response can push entities apart rather than only
destroy them - which is the thing `CollisionSystem` could never do.

## 5. Networking objects are no longer copyable

`NetServer`, `NetClient`, `NetConnection` and `NetSocket` all `= delete` their
copy constructor and copy assignment.

Each installs send callbacks capturing `this` and each owns a socket descriptor,
so a copy gave you two objects whose callbacks pointed at the original and two
destructors closing one descriptor. Hold them by reference or `unique_ptr`,
never by value and never in a `std::vector` that can reallocate. (`NetServer` is
~372 KB and `NetClient` ~188 KB — both are far too large for the stack anyway.)

## 6. A system that overflows the component cap is latched off

`sizeof(System)` is 32 → 40.

Membership is `(entitySignature & systemSignature) == systemSignature`. In 1.x a
`RequireComponent<T>()` past the cap dropped the requirement, leaving an empty
signature — and an empty signature satisfies that test for **every** entity, so
a system that should have matched nothing ran on the whole world.

Now the system is latched off and matches nothing. `System::IsDisabled()` reports
it. The latch is one-way: the component id it wanted does not exist, so nothing
at runtime could make the system correct again.

You should never see this — it needs more than 64 component types — but if you
do, it is a bug to fix rather than a mode to ship.

## 7. `Tile` carries the editor's animation fields

`sizeof(Tile)` is 80 → 104. `Map` is `std::vector<Tile>` and games iterate it
directly, so **a game built against the old header walking a vector produced by
the new library reads misaligned garbage.** Nothing warns. Rebuild.

The tile editor has always written animation data into `.map` files;
`TileMapLoader` parsed it and threw it away, so animated tiles rendered static
and the editor's animation UI did nothing at runtime. `Tile` now carries it:

```cpp
for (const auto &tile : loader.getMap()) {
  Entity e = registry.CreateEntity();
  // ...
  if (tile.isAnimated)
    e.AddComponent<AnimationComponent>(tile.numFrames, tile.frameSpeedRate,
                                       tile.vertical, tile.isLooped,
                                       tile.frameOffset);
}
```

The fields are named to match `AnimationComponent` so the copy is direct. The
engine still does not build the component for you — `TileMapLoader` hands back a
`Map` and spawning entities stays your job.

The same pass found a second field being discarded: `colliderOffset`. The editor
has written collider offsets since colliders existed, so a tile whose collider
you nudged in the editor collided from its unnudged position. `Tile` carries it
now.

**The new fields are appended, not grouped with the fields they belong beside.**
That costs 8 bytes of padding and buys one thing: a game constructing a `Tile`
positionally still assigns the same eight fields. Reordering would have shifted a
`bool` onto `colliderW`, and `bool` converts to `int` with no diagnostic — it
would have compiled and misbehaved.

## 8. `MAX_COMPONENTS` is 64

No struct changed size. See the table at the top for why that is the dangerous
kind of change rather than the safe one.

64 is the last free step. At 65 `std::bitset` becomes 16 bytes and carries
`sizeof(Registry)` and `sizeof(System)` with it — a second ABI break rather than
a recompile.

## 9. `GameStateMachine` is no longer copyable

```cpp
GameStateMachine machine;
GameStateMachine copy = machine;   // 2.0.0: compile error
```

It owns raw `GameState` pointers in two vectors and frees them in `clean()`, so
a copy gave two machines owning the same pointers and the second `clean()` freed
what the first already had. The destructor is empty, so this never appeared at
scope exit — it needed both machines to tick, which is how it survived every
example.

Hold it by value as a member (what every game already does) or by reference.
Nothing in the engine, the examples or the starter template copied one, so this
is a compile error you are unlikely to hit.

## 10. `AddSystem<T>()` takes its arguments by forwarding reference

```cpp
registry.AddSystem<MySystem>(MyConfig{...});   // 1.x: would not compile
```

The old signature took lvalue references, so a temporary could not be passed. If
you worked around it by naming a local first, that still compiles — nothing to
do.

---

## What you get for the rebuild

- **`Keyboard`** (`<stormengine2/input/keyboard.h>`) — edge-triggered
  `IsDown`/`WasPressed`/`WasReleased` over the full scancode range. It is fed
  events rather than polling, because the engine owns no main loop and two
  `SDL_PollEvent` sites drain one shared queue.
- **`ActionMap`** (`<stormengine2/input/actionMap.h>`) — bind one game action
  across the keyboard, gamepad, virtual gamepad and touch at once. Every source
  is optional, so a desktop build and a phone build share one binding table.
- **Diagnostics for the traps that used to be silent.** A component added after
  `Registry::Update()` froze membership, a system registered after matching
  entities already existed, a registry destroyed having never flushed, a stale
  handle used for component access, a sprite whose source rect falls outside its
  texture. All throttled to a handful of reports per site, on in every build.
- **`TryGetSystem<T>()`, `TryGetComponent<T>()`, `TryGetEntityByTag()`** —
  the non-throwing halves of accessors that used to throw or return a shared
  fallback.
- **`AdmitExistingEntitiesTo(system)` / `AdmitExistingEntities<T>()` and
  `CountEntitiesMissedBySystem(system)`** — the retrofit for a system registered
  too late, and the way to find out whether that happened.
- **A stale handle passed to `System::AddEntityToSystem()` is rejected** rather
  than added. That method is public, so a game holding a `System&` bypassed the
  `IsAlive` gate on `Registry::AddEntityToSystems` — and nothing removes such an
  entry, so the system iterated it every frame forever.

- **`GetConnectedClientIds()`** and a configurable per-address connection cap
  (`kNetMaxClientsPerIp`) for internet play, where `GetClientCount()` was never a
  valid loop bound.

## Checklist

1. Rebuild the library, the editor and **every** game against the same headers,
   in one go. Do not relink.
2. Add the compat bridge to your build, or `using namespace storm;` to your
   files.
3. Fix the compile errors: `Entity(88)`, `CollisionSystem`, copied net objects,
   `std::set<Entity>` without `EntityOrder`.
4. Search for `GetEntitiesToBeKilled` and adjust for `std::vector`.
5. If you load editor maps, decide whether to use the animation fields now
   reaching `Tile`.
6. Run your game and read the log. The new diagnostics report real bugs, not
   warnings to suppress.
