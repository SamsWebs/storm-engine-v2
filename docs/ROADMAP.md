# Roadmap

Planned work, in rough order. This is not a defect list — `KNOWN_ISSUES.md` holds
those, and every entry here that fixes one links to it.

The point of this file is the *reasoning*. Anyone can re-derive a task list; what
gets lost between sessions is why the order is what it is, and which decisions
were already argued and settled.

---

## 2.0.0, second wave

**Status: complete.** All eight items done, plus an adversarial review of the
whole branch and the fixes it produced. Ten breaking changes shipped, not the
nine planned — `GameStateMachine`'s copy operations were taken late, because the
alternative was spending a whole major on a one-line fix.

2.0.0 resets the 1.x compatibility promise. The first wave (PR #40) took the four
breaks that fail at compile time. This wave takes the ones that need a **layout**
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

### 3. `System` gains a disabled latch — `KNOWN_ISSUES.md` item 4 — **DONE**

`sizeof(System)` 32 → 40, pinned in `specs/layout.spec.cpp`. An overflowing
`RequireComponent` now latches the system off, so it matches nothing rather than
everything, and `System::IsDisabled()` reports it.

Three paths compare signatures and all three had to be guarded, not just
admission: `AddEntityToSystems`, and `ForEachMissedEntity`, which backs both
`CountEntitiesMissedBySystem` and `AdmitExistingEntitiesTo`. The retrofit path
was the dangerous one — unguarded, `AdmitExistingEntitiesTo` would hand the
entire world to the one system the latch exists to keep empty.

A fourth site, `SystemMissedByLateComponent`, was left **without** a guard on
the reasoning that `matchedAtAdmission` is `(asAdmitted & required) == required`,
which an empty signature satisfies for every entity. **That reasoning was wrong
and an adversarial review caught it.** `RequireComponent` latches and returns
*without clearing `componentSignature`*, so a system whose first requirement
resolved and whose second overflowed keeps the first bit: its signature is not
empty, `matchedAtAdmission` is not trivially true, and the `alreadyMember`
escape is guaranteed to fail — the latch is what emptied the member list. Every
system in this engine has two or more requirements, so that is the ordinary
shape, not an exotic one. The guard is restored.

**The methodology failure is the part worth keeping.** The guard was removed
because a mutant survived. The mutant survived because `SpecOverflowSystem` has
exactly one requirement and nothing covered partial overflow — "no test
distinguishes this" meant *untested*, and it was read as *unreachable*. That
conclusion was then written into a code comment, this roadmap and a commit
message, where it looked like a considered decision. A surviving mutant is a
question about the tests first and the code second.

### 4. `Tile` carries the editor's animation fields — `KNOWN_ISSUES.md` item 7 — **DONE**

`sizeof(Tile)` 80 → 104, pinned in `specs/layout.spec.cpp`. All five animation
fields now reach `Tile`, named to match `AnimationComponent` so a game builds one
by copying across.

The pass found a **second** discarded field nobody had listed: `colliderOffset`.
The editor has written collider offsets since colliders existed and the loader
read them only to advance the stream, so a tile whose collider the editor had
nudged collided from its unnudged position. It is fixed here because 2.0.0 is
the one chance — carrying it later would cost a second ABI break.

The new fields are appended rather than grouped beside the collider fields, at a
cost of 8 bytes of padding. Reordering would have silently shifted a `bool` onto
`colliderW` for any game constructing a `Tile` positionally, and `bool` converts
to `int` without a diagnostic.

One test was written vacuous and caught before commit: "animation must not leak
between tiles" asserted on tiles that *preceded* the only animated tile in the
fixture, so it would have passed against any implementation. The fixture gained
a plain tile *after* the animated one, which is the only position where the leak
is observable. Mutation testing then killed all five mutants, the hoisted-variable
leak included.

**Not done here:** no example consumes the new fields yet, so the feature is
proven by specs rather than end to end. Wiring one would need the engine
installed over `/usr/local` to verify locally, since examples build against the
install prefix rather than the checkout.

### 5. `MAX_COMPONENTS` 32 → 64 — `KNOWN_ISSUES.md` item 3 — **DONE**

No size change: `sizeof(std::bitset<N>)` is 8 bytes for every N up to 64. That is
precisely why it needed a major — no size check catches a stale object file, so a
mismatch between translation units is silent.

Because the size pins in `specs/layout.spec.cpp` read identically at 32 and 64,
they cannot see this change at all. The spec now pins `MAX_COMPONENTS` itself
next to the sizes, with a comment saying why a value is being pinned in a file
that otherwise pins only layout.

64 is the last free step. At 65 `std::bitset` becomes 16 bytes and carries
`sizeof(Registry)` and `sizeof(System)` with it — a second ABI break rather than
a recompile. That is recorded at the constant, in the README and in
`KNOWN_ISSUES.md`, since the next person to want more types will not otherwise
know the ceiling has a cliff behind it.

### 6. The engine moves into `namespace storm` — `KNOWN_ISSUES.md` item 9 — **DONE**

Done last, as planned: it touches 46 engine files and would have made every other
diff in the wave unreadable.

Ships with `<stormengine2/compat/global.h>`, which emits a `using` declaration
for every public engine name. The cheapest migration for an existing game is a
force-include from the build (`CXXFLAGS += -include stormengine2/compat/global.h`)
rather than editing every file. The header exists to be deleted — it undoes the
namespace's entire benefit — and a future major drops it.

Four things this turned up that were not obvious from the plan:

- **The unscoped enums.** `using storm::LogType;` does not bring `LOG_INFO`
  across: the enumerators of an unscoped enum are names in `namespace storm` in
  their own right. The bridge needs a `using` per enumerator, and `LogType`,
  `NetChunkFlag`, `NetPacketFlag` and `NetControlMessage` all have them.
- **The `friend` declaration in `Registry`.** `friend struct EcsGenerationTestSeam;`
  now names `storm::EcsGenerationTestSeam`, so the spec's seam had to move into
  the namespace — a same-named struct in the global namespace is a different type
  and gets no access. The compiler said so plainly, but only for that one file.
- **`specs/main.cpp` includes no engine header**, so `using namespace storm;`
  there is an error rather than a no-op. A blanket edit across `specs/` has to
  account for it.
- **`INCLUDE +=` in `editor/Makefile` is discarded** the moment anyone passes
  `INCLUDE` on the command line, which is how the editor gets built against a
  staging prefix. It is `override INCLUDE +=` now.

`specs/compat/global.spec.cpp` is the one spec in the suite deliberately written
**without** `using namespace storm;`, since the directive would make it pass
whether the bridge exported anything or not.

Its original claim — "it names every type unqualified through the bridge alone,
so it fails if the bridge misses a name" — **was false**, and an adversarial
review caught it. The file named 33 of 133 exports, chosen from the same mental
list that produced the bridge, so a name forgotten in one was forgotten in the
other. Two were: `EcsSuppressionNote` and `ComponentMissDescription`, both
public since 1.x, both used by any game with its own throttled diagnostic.

The fix is that the list no longer comes from memory.
`scripts/generate-compat-probes.py` parses the engine headers and emits
`specs/compat/bridgedNames.h`, one `using ::Name;` per public name — a form
legal for every entity kind that fails to compile when the name is absent. CI
re-runs the generator with `--check` and fails if the committed file is stale,
because a generated file nobody regenerates is the same hole wearing a
different hat.

Verified against a staging install (`make install DESTDIR=…`) rather than by
overwriting `/usr/local`: all nine desktop examples build and link, the editor
compiles all 17 objects, and the starter template in `template/` builds through
pkg-config. `examples/nx-platformer` and `examples/android-platformer` were
edited the same way but **not** built — neither toolchain is on this machine,
and CI does not cover them either.

### 7. Input action mapping — **DONE**

`common/input/actionMap.h`, header-only and additive — nothing existing changed,
so this is the one item in the wave that breaks nothing.

`ActionMap::Bind(actionId, ActionBinding{key, pad, vpad, touch})`, then
`Update(sources)`, then `IsDown` / `WasPressed` / `WasReleased`. Every source is
optional, so a desktop build and a phone build share one binding table and differ
only in what they pass.

The design decision worth keeping: **keyboard and gamepad edges are taken from
those classes rather than recomputed from the held state.** Deriving all edges
centrally is simpler and was the first design, but a key pressed and released
inside a single frame never appears in the held state at all, so fast taps would
vanish — which is the exact reason `Keyboard` tracks presses separately. The
virtual gamepad and touch are stateless snapshots with no edges of their own, so
only those are derived against the previous frame.

Multi-source rule: down when the first source takes it, up when the last lets go.
A second source joining mid-hold is not a new press.

`Update` takes `ActionSources` holding two `GamepadState` snapshots rather than a
`Gamepad`, because `GamepadState` is a plain struct — that is what lets the whole
header be spec'd with no controller attached, the same seam `gamepad.h` uses.

**A claimed coverage gap that was not one.** The four-argument convenience
overload was documented three times — header, spec and here — as untestable,
because `Gamepad::Update()` samples a real device and with nothing attached
`Current()` and `Previous()` hold identical values. That reasoning only
considered their *contents*. They are distinct objects at distinct addresses
whether or not a device is attached, so asserting on pointer identity settles it
with no hardware at all. The forwarding is a public `SourcesFrom()` seam now,
and the swap mutant is killed.

The adversarial review also found four real defects in this header, all fixed:
a release edge that fired for an action that was never down; `Bind()` leaving
stale edge state that fabricated a release or swallowed a press; `binding.pad`
indexing a fixed array with no range check; and source-pointer stability being
an unstated precondition that failed in opposite directions for the stateful and
stateless sources. Both "map every control" specs were vacuous — they set every
flag, then one flag, which left every other control free to swap — and now sweep
each control held alone.

Mutation testing also removed one term and added one case. On the press side
`stateless && !statelessPrev` could never differ from `stateless`, because the
`!entry.down` gate already covers it, so it is gone rather than left as
unreachable-effect code. The same flag is load-bearing on the release side —
without it an idle action reports a release on every frame — so there is now a
case pinning exactly that.

### 8. Documentation, written once at the end — **DONE**

`docs/UPGRADING.md` and the `CHANGELOG.md` 2.0.0 entry, written against what
actually shipped. `VERSION` moved from `2.0.0~dev` to `2.0.0`, and the README's
banner with it.

The tilde mattered and was not tidied to a hyphen: pkg-config sorts `2.0.0-dev`
*above* `2.0.0`, so a game gating on `--atleast-version=2.0.0` would have passed
against a development build missing most of the release. `2.0.0~dev` sorts below.

Every code snippet in `UPGRADING.md` was compiled against a staging install
before the file was committed, which caught a wrong `ContactSystem` API in the
migration example — `SetOnBegin(Entity, Entity)` does not exist; it is
`SetOnBeginContact(const Contact &)`, and the `Contact` carries a normal and a
penetration depth, which is the whole reason `ContactSystem` can do what
`CollisionSystem` could not. A migration guide is exactly the document whose
examples nobody compiles.

One claim was walked back before commit: the changelog said eight of the ten
`KNOWN_ISSUES.md` entries were resolved. Seven are resolved outright; item 10 is
half resolved (`CollisionSystem` is gone, the event bus is still missing), and
counting it whole would have overstated the release.

### The adversarial review

Run against the whole branch before tagging, aimed at five claims rather than
at the diff. It found real defects in four of them and confirmed the fifth.

What it overturned, and what each cost:

- **The guard removed from `SystemMissedByLateComponent`.** Justified on the
  claim that a latched system's signature is empty; `RequireComponent` latches
  *without clearing the signature*, so a system whose second requirement
  overflowed keeps the first bit. Restored, with the two specs whose absence let
  the mutant survive.
- **"The `ActionMap` overload cannot be tested."** Asserted three times. Only
  ever considered the two `GamepadState`s' *contents*; they are distinct objects
  at distinct addresses, so pointer identity settles it with no hardware.
- **"The compat spec fails if the bridge misses a name."** It named 33 of 133,
  from the same list that produced the bridge. Two public names really were
  missing. The list is generated from the engine headers now.
- **Four `ActionMap` defects** and **two vacuous mapping specs** that passed
  against swapped controls.
- **The editor's undo stack** identified tiles by raw id — the exact failure
  class wave one closed everywhere else, in a first-party consumer nobody swept.

What it confirmed: the `Registry` size arithmetic reconciles to the byte
(576 − 56 − 56 + 24 = 488), the namespace wrap of `common/` is semantically
sound across every hazard class checked, and one redundancy removal in
`ActionMap` was *proved* correct rather than merely accepted.

**The lesson worth carrying.** Three of the overturned claims share a shape: a
check passed, and the passing was read as evidence about the code when it was
evidence about the check. A surviving mutant meant "untested", not
"unreachable". A green spec meant "the names I remembered are exported". A green
build meant "it parses". Each conclusion was then written somewhere durable —
a comment, this file, a commit message — where it read as a considered ruling.
Prefer checks whose input is generated from the thing under test, not from the
author's memory of it.

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

### An example with sustained entity churn

Genre coverage is decent — platformer (plus Switch and Android ports), JRPG,
Tetris, shooter, hockey, strategy, checkers, and three networking samples.
Racing, roguelike and tower-defense are unrepresented, but genre is not the gap.

The gap is **churn**. Only four of the eleven examples kill an entity at all
(`shooter`, `puzzle`, `strategy`, `netplay-checkers`), and none of them creates
and destroys entities continuously. So nothing in the tree exercises id
recycling at scale — which is exactly why the generation-counter wrap survived
every review and every one of 428 specs, and was found only by an adversarial
probe that drove `Registry::Update()` flat out for an hour.

A wave-survival or bullet-hell example would exercise it as a side effect of
being what it is: hundreds of entities spawning and dying per second, handles
held across frames, ids recycling constantly.

Worth being honest about what an example can and cannot show here, though. The
layout wave's changes are **invisible in correct code** — a stale handle is
rejected, and a game that never keeps one notices nothing. An example cannot
demonstrate that without deliberately misusing the engine. What it would do is
*exercise* the recycling path under real load, which is different and arguably
more valuable: a soak target rather than a teaching sample.

If the goal is coverage rather than a sample, a long-running stress spec would
buy more per line than an example does, and would not add a twelfth README to
sweep the next time something is deleted.

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

**Status: documented, not enforced.** An automated tree-vs-install mismatch
check was written and reverted — it broke CI, because CI builds the engine from
a separate tree at `/opt/library`, so tree-and-install equality is false there
by construction. Any future attempt has to account for that.

What landed instead is a comment in `base.mk` explaining the behaviour and what
it costs. That is weaker than a check: the layout wave has since shipped, so
`sizeof(Entity)`, `sizeof(System)` and `sizeof(Tile)` have already changed, and
a stale install is now silent memory corruption rather than a compile error.
`specs/layout.spec.cpp` pins the sizes but only inside the engine's own build —
it cannot see a game's stale headers.

Worth noting how nearly this was lost: the PR meant to land that comment
(#46) merged a **net-empty diff**. One commit added the reverted check, the
next removed it and never wrote the documentation its own message promised.
The comment reached `main` only when the omission was spotted afterwards.

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

### `editor/` does not build under GCC 13 — **DONE**

There were two breakages, not one, the second only visible once the first was
fixed:

1. Vendored `ImGuiFileDialog.cpp` used `intptr_t` with no header declaring it.
   Older toolchains supplied it transitively; GCC 13 stopped. Fixed by
   including `<cstdint>`.
2. `sol.hpp` includes `<lua.h>` unqualified, while `base.mk`'s `INCLUDE` only
   reaches `vendor/`, so only `<lua/lua.h>` resolved. The editor's Makefile now
   adds `-I$(ROOT_DIR)/vendor/lua`, editor-only because no example includes
   sol2.

All 25 editor translation units compile. The **link** still requires `libnfd`,
which Debian and Ubuntu do not package — that is why CI compiles the editor to
objects and stops short of linking. README now states the prerequisite, which
nothing did before: `cd editor && make` was the only instruction and it cannot
succeed without building libnfd from source.

Two lessons worth keeping:

- The `<cstdint>` patch would have shipped as a 4827-line diff. An editor pass
  had rewritten the whole CRLF file to LF, burying a three-line change. Caught
  at `git diff --stat`. Check line endings before committing to `vendor/`.
- Patching vendored source with no record of it creates a delta the next vendor
  update silently reverts, so `vendor/MANIFEST.md` landed *with* the patch
  rather than after it. It also surfaced that Dear ImGui is pinned at 1.79 WIP
  from around 2020 — now a decision someone can make on evidence.

Editor warnings that remain (narrowing, sign-compare, a duplicate `clean`
recipe between `editor/Makefile` and `base.mk`) are pre-existing and untouched.

---

## Carried, not blocking

Reviewed, ruled on, and deliberately left. Listed so they are not rediscovered as
though new.

- ~~**`ForEachMissedEntity` does not exclude entities queued for death**, where
  `SystemMissedByLateComponent` does.~~ **Fixed.** Ruled "needs kills queued *and*
  a system registered before the flush", which was true and turned out not to be
  the point: this wave added `AdmitExistingEntitiesTo` on top of it, so the
  false positive stopped being a miscount and started handing a system an entity
  that the next `Update()` reaps. A carried item can be made live by a later
  change without anyone re-reading it.
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
