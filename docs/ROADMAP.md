# Roadmap

Planned work, in rough order. This is not a defect list — `KNOWN_ISSUES.md` holds
those, and every entry here that fixes one links to it.

The point of this file is the *reasoning*. Anyone can re-derive a task list; what
gets lost between sessions is why the order is what it is, and which decisions
were already argued and settled.

---

## 2.0.0, second wave

2.0.0 resets the 1.x compatibility promise. The first wave (PR #40) took the four
breaks that fail at compile time. This wave takes the five that need a **layout**
change, which is a different and more dangerous class: a game that relinks without
rebuilding does not get an error, it gets misaligned memory.

That already happened once. 1.3.0 changed `sizeof(AssetStore)` from 112 to 208, and
because games allocate the store themselves with `std::make_unique<AssetStore>()`,
the size is emitted at *their* call site. Nothing warned.

### 1. Pin the layout first — before any of the rest — **DONE**

Add a spec asserting `sizeof(Registry) == 576`, `sizeof(Entity) == 16`,
`sizeof(System) == 32`, `sizeof(Signature) == 8`, `sizeof(Tile) == 80` on x86-64.

This is deliberately the first task and not the last. Every subsequent item below
changes one of those numbers, and the pin forces each change to be a conscious edit
in a spec file rather than a silent consequence. It converts exactly the failure
mode the `AssetStore` incident came from.

### 2. `Entity` gains a generation counter — `KNOWN_ISSUES.md` item 1 — **DONE**

`sizeof(Entity)` 16 → 24. The highest-risk item in the release.

Ids are recycled, so a handle kept past its entity's death is bit-for-bit identical
to the new entity holding that id — a stale handle kills a live stranger and
`IsAlive` agrees it is fine.

The mechanical part is easy. The risk is in the places that key on a **raw id**
rather than an `Entity`: `Registry`'s tag and group maps are `unordered_map<int, …>`,
and that is where a dropped generation lets a recycled id inherit a stale tag,
silently. Any plan for this item should start by reading every one of those sites,
not by describing the struct change.

Two specs (`specs/ecs.spec.cpp`, `specs/registry.spec.cpp`) deliberately pin the
current wrong behaviour and carry comments saying a breaking release must flip them.

**Testing note.** A test that confirms a stale handle still *exists* proves nothing.
Only one that confirms it is *rejected* does. This release shipped four checks that
passed while the thing they checked was broken, every one of them asserting on a
proxy — a count, a log line, a build succeeding — instead of the property.

**Landed.** `sizeof(Entity)` 16 → 24, `sizeof(Registry)` 576 → 600 → **488** — it grew by the
generations vector and then shrank further when the two reverse index maps went away. Generation 0 is
reserved as never-valid, so a hand-built `Entity(id)` is stale by construction. `operator<` and `operator>`
were deleted, `IsAlive` became exact and O(1), and stale handles are now rejected at read *and* write.

Two things the whole-branch review caught that six task-scoped reviews could not, both worth remembering
when items 3-6 are planned:

- **`ContactSystem` kept its own id-keyed frame state.** Its `previous` pairs were raw ids, so a recycled id
  made `onEnd` fire naming an entity that never began that contact — `KNOWN_ISSUES` item 1's exact shape
  surviving inside the wave that closes it, behind a comment claiming it was guarded. Fixing `Entity` does
  not fix everything that stores entity identity; the next layout item should start by asking who else keeps
  a copy.
- **Reads were hardened and writes were not.** `AddComponent` through a dead handle silently overwrote the
  live occupant. Symmetry is not automatic — check every door, not the one the defect was reported through.

The layout pin earned itself immediately: it caught each size change as it happened and forced a measured
number into every commit message.

### 3. `System` gains a disabled latch — `KNOWN_ISSUES.md` item 4

`sizeof(System)` 32 → 40. Also fixes the wrong failure *direction* in the component
cap: a system whose `RequireComponent` overflowed ends up with an empty signature,
and an empty signature matches **every** entity — so a system that should have
matched nothing runs on the whole world.

### 4. `Tile` carries the editor's animation fields — `KNOWN_ISSUES.md` item 7

`sizeof(Tile)` 80 → larger. The tile editor writes animation data into `.map` files
and `TileMapLoader` parses and discards it, because `Tile` has nowhere to put it.
Animated tiles render as static ones and the editor's animation UI does nothing at
runtime.

### 5. `MAX_COMPONENTS` 32 → 64 — `KNOWN_ISSUES.md` item 3

No size change: `sizeof(std::bitset<N>)` is 8 bytes for every N up to 64. That is
precisely why it needs a major — no size check catches a stale object file, so a
mismatch between translation units is silent.

### 6. The engine moves into `namespace storm` — `KNOWN_ISSUES.md` item 9

Do this **last**. It rewrites nearly every line it touches, and doing it earlier
makes every other diff in the wave unreadable.

Ships with an opt-in `<stormengine2/compat/global.h>` emitting `using` declarations,
so an existing game keeps compiling by adding one include. That header exists to be
deleted in a future major — a bridge, not an API.

For a game whose engine includes are spread across many headers, the cheapest
migration is a force-include from the build rather than editing every file:

```make
CXXFLAGS += -include stormengine2/compat/global.h
```

### 7. Input action mapping

Belongs in this wave because it touches the input headers that namespacing is
already rewriting.

Four input sources now ship — `keyboard.h`, `gamepad.h`, `virtualGamepad.h`,
`touchControls.h` — with no way to bind them to a single action, so every game
writes `if (key || pad || touch)` by hand. An `ActionMap` resolving one action
across all four is what makes those four headers a system rather than four headers.

### 8. Documentation, written once at the end

`docs/UPGRADING.md` and the `CHANGELOG.md` 2.0.0 entry, written against what
actually shipped rather than what was planned. Then `VERSION` moves from
`2.0.0~dev` to `2.0.0`.

The tilde matters and should not be "tidied" to a hyphen: pkg-config sorts
`2.0.0-dev` *above* `2.0.0`, so a game gating on `--atleast-version=2.0.0` would
pass against a build missing most of the release. `2.0.0~dev` sorts below.

---

## After 2.0.0 ships

### A non-ECS collision entry point

The contact math is already ECS-free and does not know it. `ContactAABB` is four
floats with no engine types in it, and both `Overlaps(a, b)` and
`Manifold(a, b, normal, depth)` take nothing but `ContactAABB` — they are just
statics tucked inside `ContactSystem`. Only three things are actually ECS-bound:
`BoundsOf(Entity)`, `Contact` holding `Entity a, b`, and the sweep iterating
`GetSystemEntities()`.

This is worth doing because there is a real consumer: a game that uses none of the
ECS cannot reach `ContactSystem` at all, and ends up hand-rolling overlap tests the
engine already has.

**Hours — expose the primitives.** Move `Overlaps` and `Manifold` to public, or to
free functions beside `ContactAABB`. Purely additive, no break, ships in a 2.x minor.

**About a day — extract the sweep.** `ContactSystem::Update()` does three separable
jobs: build bounds from components, run the broadphase sweep and manifold, and diff
against the previous frame for begin/end callbacks. Jobs two and three need nothing
from the ECS but *an identity per body*. Parameterise on `std::size_t` and leave
`ContactSystem` a thin adapter.

Three behaviours must survive exactly, or this breaks `ContactSystem` while claiming
to be a refactor:

- the id-normalisation that makes `normal` independent of iteration order
- the final contact sort
- the filter running **before** `Manifold` — that ordering is what keeps a dense
  volley cheap in `examples/shooter`

The safety net is good: 498 lines across `contact.spec.cpp`, `contactEvents.spec.cpp`
and `contactFiltering.spec.cpp` pin current behaviour. The extracted core wants its
own non-ECS specs on top.

Settle two design questions before writing code: what identifies a body (an opaque
`std::size_t`, or `void*` userdata), and whether the core owns the begin/end state
or the caller does.

### A lighting overlay

Generalises a technique proven in a shipping game: two quarter-resolution RGBA
surfaces built once and cached — a warm key layer whose alpha follows a falloff
function, and a cool vignette layer whose alpha is the **inverse** of the same
value — both `SDL_BLENDMODE_BLEND`, upscaled to full screen with two
`SDL_RenderCopy` calls.

Quarter-res is why it is cheap; the inverse-alpha pairing is why it reads as
lighting rather than a tint. No shaders, so it runs on the SDL2 renderer on every
target including Switch and Android. That portability is what makes it engine
material rather than a desktop nicety.

The design question to settle first: does the engine own light *entities* in the
ECS, or is this a standalone overlay that a state drives? That choice is hard to
reverse.

### A debug overlay

FPS, frame time, entity count, per-system timings, and the last few `Err` lines,
toggled with a key.

The cheapest item on this list and the natural counterpart to 2.0.0's diagnostics,
which currently only reach a log nobody reads during play.

---

## Build and CI

### `examples/examples.mk` silently compiles against the installed engine

`examples.mk` links `-lstormenginev2` and passes no `-I` at the source tree, so a
default example build resolves `<stormengine2/…>` from `/usr/local/include` — that
is, against whatever version happens to be installed, not against the checkout.

This has already produced two wrong conclusions during development: examples were
declared clean after being exercised against an engine that did not contain the
feature under test.

**Fix this before the layout wave, not after.** Today a stale-header build is a
confusing compile error. Once `sizeof(Entity)`, `sizeof(System)` and `sizeof(Tile)`
change, the same mistake is silent memory corruption.

### Comments cite line numbers, and the line numbers rot

Several comments in `common/` point at specific lines — for example `common/systems/contact.h` citing
`common/ecs.cpp:439` for where a killed id returns to the free list. That line is `AddEntityToSystems`; the
free-list push is at 457.

This is not a one-off. Line citations drifted **four times in a single day's work**: 404/537 became 410/543,
then 546, and this one was wrong twice — including once where a fix report claimed it had been corrected
and it had not. Each edit above a cited line silently invalidates it, and nothing checks.

The fix is not to correct the numbers. Cite function names instead — `grep -n` finds them, and they do not
drift. This is worth doing as a sweep rather than opportunistically, because a half-swept file is exactly
the state that makes the remaining citations look trustworthy.

### `editor/` does not build under GCC 13

Vendored `ImGuiFileDialog.cpp` is missing `<cstdint>`. Pre-existing and unrelated to
any current work, but `pr-validate.yml` compiles the editor, so it will surface in
CI eventually.

---

## Carried, not blocking

Reviewed, ruled on, and deliberately left. Listed so they are not rediscovered as
though new.

- **`ForEachMissedEntity` does not exclude entities queued for death**, where
  `SystemMissedByLateComponent` does. The only false-positive path found in the six
  diagnostics; it needs kills queued *and* a system registered before the flush.
- **Signed overflow in `render.h`'s bounds arithmetic** on absurd sprite values.
  `srcRect.x > textureW - srcRect.w` avoids it at no cost.
- **Undocumented `const_cast`** in `common/ecs.cpp`'s missed-entity scan.
- **`IsAlive` is an O(|freeIds|) deque scan** called before the cheaper
  `IsPendingAdmission` short-circuit in `SystemMissedByLateComponent`. Swapping them
  helps churn-heavy games.
- **A `System` constructor runs before the duplicate-registration check** in
  `AddSystem`, so a subclass whose constructor had an observable side effect outside
  the `Registry` would fire it and then have the instance discarded. Not live: every
  current system's constructor only calls `RequireComponent`.

### Carried from the layout wave

Reviewed, ruled on, deliberately deferred.

- ~~**`generations[id]` can wrap to 0**~~ — **fixed.** Ruled "unreachable in practice" and that ruling was
  wrong. An adversarial review measured the rate rather than arguing it: **1.66 hours** on a harness driving
  `Update()` flat out, ~50 days at 1 kHz, because the constraint is one increment per id per `Update()` call,
  not per kill. At the boundary a hand-built `Entity(id)` compared equal to a live entity, read its
  components and killed it — `KNOWN_ISSUES` item 1 verbatim, under a "Resolved in 2.0.0" note. Clamped.
- **No spec recycles an id more than once.** Every stale-handle case goes generation 1 → 2. A case that
  recycles three or four times would cover the counter's actual behaviour rather than its first step.
- **`Entity::GetGeneration()` is asserted on by no spec** — it is a new public accessor with no direct
  coverage.
- ~~**`TagEntity`/`GroupEntity` accept stale handles**~~ — **fixed, and it was worse than recorded here.**
  Filed as a lookup oddity; proved to be a *regression this wave introduced*. Against base `93cb817`, tagging
  through a stale handle left the live entity's tag intact; on the branch the live entity **silently lost its
  tag**, zero log lines. The group half was caused by the wave's own change — the old `std::set<Entity>` with
  id-only `operator<` deduplicated by id, `EntityOrder` does not. `AddEntityToSystems` was a third such path
  and the worst: a stale member was never removed, so systems iterated it every frame forever. All three now
  carry the same `IsAlive` gate as `AddComponent`.

  The lesson worth keeping: the write paths were never enumerated. `AddComponent` and `RemoveComponent` were
  gated because someone noticed them individually. Before the next layout item, list every path that mutates
  or stores entity identity and check them as a set.
- **Five mutants survived the suite** when it was mutation-tested. Four remain open: `++generations[id]`
  → `+= 2` (no spec asserts a generation *value* or a second recycle), `System::RemoveEntityFromSystem`
  reduced to id-only, `EntityHasTag` reduced to id-only, and `GetEntitiesToBeKilled` stripping the generation
  off every returned handle — the last is asserted on only by `.size()`, which is exactly the proxy pattern.
  The fifth, `ContactSystem::PairKeyOrder` reduced to id-only, is closed: it silently dropped contact *begin*
  events for a recycled entity with the suite green, and now has a spec naming the entity by
  `(id, generation)`.
- **Cross-registry aliasing is untouched.** `operator==` and `IsAlive` both ignore `Entity::registry`, and
  every registry's generations start at 1, so one registry will report another's entity as alive, read its
  components and kill it. Pre-existing and identical on base — but the engine's own pattern is a `Registry`
  per `GameState` plus a singleton, the pointer is already in the struct, and `IsAlive` could close it with
  `&& entity.registry == this`.
- **`EntityOrder` is a new unqualified global symbol.** A game defining its own collides. Item 6
  (`namespace storm`) resolves it.
- **`RemoveEntityGroup` never erases an emptied group**, so `DoesGroupExist` stays true after every member
  dies. Pre-existing; the new scan now also walks those empty entries.
- **`Makefile.win` has no `-pthread`/`-mthreads`** while two spec files now use `std::thread`. Untested — no
  MinGW toolchain available here. `Makefile.debian` has it.
