# Guardrails for the eleven Storm! Engine v2 usage traps

**Status:** design, approved for planning
**Target release:** 1.4.0 (additive minor)
**Date:** 2026-08-30

## Problem

Eleven known ways to misuse the engine all share one shape: the wrong code
compiles cleanly, links, runs, and then misbehaves. Nothing in the build output
or the runtime log points at the cause. Eight of the eleven are already written
down in `KNOWN_ISSUES.md`, which is the record of what cannot be fixed inside
the 1.x compatibility promise — but a document only helps a reader who already
suspects the trap. A person or a code-generating model meeting the engine for
the first time does not.

The traps, as reported from the field:

1. `CollisionSystem` kills every colliding entity that has a
   `RigidBodyComponent`. There is no collision callback on it.
2. Systems must be registered before any entity is created. Membership is
   computed once, when `Registry::Update()` flushes.
3. `registry_.Update()` must be the first call in a state's `update()`.
4. `AddSystem<T>()` takes constructor arguments by lvalue reference, so
   `AddSystem<X>(5)` does not compile.
5. `GetSystem<T>()` throws `std::out_of_range` for a system never registered.
6. Entity ids are recycled and carry no generation counter.
7. `GetComponent<T>` returns a shared fallback on a miss, and two misses alias.
8. There is no keyboard abstraction and no engine-owned main loop; polling SDL
   events in two places drains a shared queue.
9. No namespaces — every engine type is a global symbol.
10. `SpriteComponent`'s `width`/`height` are the source rect, not screen size.
11. A wrong `AnimationComponent::vertical` flag draws nothing, silently.

## Constraint

No breaking changes. The 1.x promise in `KNOWN_ISSUES.md` rules out changing a
public signature, changing the layout of a type a game embeds or passes by
value, or deleting a public member. In particular `sizeof(Registry)` is ABI:
games embed a `Registry` by value in their states, so the design may not add a
data member to it.

One deliberate exception is taken, described under "The one judgment call".

## Approach

Extend the diagnostic convention the engine already has rather than introduce
new architecture. `common/ecs.h` already ships `EcsShouldReport`,
`EcsReportErr`, `EcsComponentIdIsValid` and `ECS_MAX_DIAGNOSTIC_REPORTS = 4`:
each diagnostic reports its first few occurrences through a call-site-owned
static counter and then goes quiet, so a per-entity-per-frame check costs a
counter comparison after the fourth report. Every runtime check below uses that
same throttle.

Three layers, applied per trap according to what can actually catch it:

- **Compile time** — `[[deprecated]]` attributes and an additive template
  signature, so the wrong call warns or the previously-impossible call starts
  working. Visible in build output.
- **Runtime** — throttled `Err` diagnostics that name the mistake and the fix.
  Catches the traps no compiler can see.
- **Documentation** — for the two traps that neither can address inside 1.x.

### The enabling trick

Almost every check is derivable from state `Registry` already holds:
`entitiesToBeAdded`, `entitiesToBeKilled`, `freeIds`, `numEntities`, `systems`
and `entityComponentSignatures`. No new member is needed for those.

Where genuinely per-instance diagnostic state is required — only trap 3 needs
it — it lives in a file-static `std::unordered_map<const Registry *, DiagState>`
in `ecs.cpp`, keyed on `this` and erased in `~Registry`. That keeps
`sizeof(Registry)` at 576 bytes and keeps the state out of the public header.
The map is touched on `CreateEntity` and `Update`, not per entity per frame.

## Design

### Trap 1 — `CollisionSystem` kills

Mark the class `[[deprecated("CollisionSystem kills both entities on overlap; "
"use ContactSystem for observable collisions")]]` and log one `Err` from its
constructor. `ContactSystem` has superseded it since 1.3.0 and no in-repo game
registers it. Behaviour is unchanged; only a warning is added. Deleting the
class stays a v3 item.

### Trap 2 — component added after the entity was admitted

In `Registry::AddComponent`, after the signature bit is set: if the entity is
alive and is *not* in `entitiesToBeAdded`, it has already been flushed, so its
system membership is fixed. Walk the registered systems and report only when
the newly set bit makes the entity satisfy a system signature it is not already
a member of. That last condition is what makes the check precise — adding a
component that changes no membership is legitimate and must stay silent.

The diagnostic names the system that will never see the entity, and states the
fix: add every component before the `Registry::Update()` that admits the
entity, or kill and re-create.

### Trap 2b — system registered after entities were admitted

In `Registry::AddSystem`, after the system is inserted (its constructor has by
then run `RequireComponent`, so its signature is final): scan admitted live
entities for any matching the new signature. Report the count. This is O(live
entities) once per `AddSystem`, and there are 107 `AddSystem` calls across the
whole repository, so the cost is irrelevant.

Add `Registry::AdmitExistingEntities<TSystem>()` as an explicit opt-in repair:
it back-fills matching already-admitted entities into the new system. Explicit,
never automatic — a silent back-fill would change behaviour for any game that
registers a system late on purpose.

### Trap 3 — `Registry::Update()` not called, or not called first

The one-frame-late case (calling `Update()` after the systems instead of
before) is benign and not worth a diagnostic. The fatal case — never calling
`Update()` at all, so no entity ever joins a system and nothing renders — is
worth catching and is cheap: the side table counts `Update()` calls, and
`CreateEntity` reports once when the pending set has grown past a small
threshold while that count is still zero.

### Trap 4 — `AddSystem` argument passing

Change `template <typename TSystem, typename... Targs> void AddSystem(Targs
&...)` to `Targs &&...`. The body already calls `std::forward<Targs>(args)...`,
which under the current `Targs &` signature deduces `Targs` as a non-reference
and therefore *moves out of the caller's lvalue*. That is a live silent bug as
well as the reason `AddSystem<X>(5)` fails to compile. The new signature fixes
both.

### Trap 5 — `GetSystem<T>` throws

Add `template <typename TSystem> TSystem *TryGetSystem() const`, returning
`nullptr` when absent. Also log an `Err` naming the system immediately before
`systems.at()` throws: under the Switch build's `-fno-exceptions` that throw
aborts, and an abort with no message is the worst possible failure.

`GetSystem` itself is unchanged.

### Trap 6 — recycled entity ids

Not fixable in 1.x; the fix is a generation counter and `sizeof(Entity)` is
ABI. Add `Registry::TryGetEntityByTag(const std::string &) const` returning a
pointer, so the guarded lookup is one call rather than the
`DoesTagExist`-then-`GetEntityByTag` pair that is easy to half-write.
`GetEntityByTag` keeps its precondition and its behaviour.

### Trap 7 — aliasing fallback component

Already diagnosed and logged. Extend the existing message with the component
type name so the log says which lookup missed.

### Traps 10 and 11 — nothing draws

One check in `RenderSystem::Update` catches both. Query the texture with
`SDL_QueryTexture` and compare the sprite's `srcRect` against its bounds. A
`srcRect` that falls outside the texture is the exact signature of a wrong
`AnimationComponent::vertical` flag (the frame offset is applied to the wrong
axis and walks off the sheet) and of `SpriteComponent` `width`/`height` that do
not match the sheet cell.

The message reports both rects and names both causes, for example:

    srcRect {0,192,32,32} outside texture 'player' 128x32 — nothing will draw.
    Check SpriteComponent width/height match the sheet cell, and
    AnimationComponent.vertical matches the sheet layout.

Throttled like every other diagnostic, so a permanently broken sprite costs
four log lines, not one per frame.

### Trap 8 — no keyboard abstraction

Add `common/input/keyboard.h`: an edge-triggered keyboard state, fed one
`SDL_Event` at a time by the caller. It deliberately does not poll, so it
cannot become a second consumer of the shared event queue — the trap it exists
to prevent. It mirrors the shape of the existing `common/input/gamepad.h`.

Frame pacing already exists as `GameState::CapFrameRate` and needs nothing.

### Trap 9 — no namespaces

Not fixable. Global type names are the 1.x ABI, and a namespace alias would not
prevent the collision it is meant to prevent, because the global names remain.
Documentation only. This is stated plainly rather than given a token fix.

## Testing

The engine's convention is that `specs/` mirrors the source tree. Each runtime
diagnostic gets a spec asserting both directions — that the diagnostic fires
for the misuse, and, more importantly, that it stays silent for the legitimate
neighbouring case. The false-positive assertions are the ones that matter: a
diagnostic that cries wolf on correct code is worse than no diagnostic.

New and extended spec files:

- `specs/ecs.spec.cpp` — component-after-admit fires; component-after-admit
  that changes no membership stays silent; `TryGetSystem` absent and present.
- `specs/registry.spec.cpp` — system-after-entities fires with the right count;
  `AdmitExistingEntities` back-fills; `TryGetEntityByTag` guarded lookup.
- `specs/systemMembership.spec.cpp` — `AddSystem` accepts an rvalue, and no
  longer moves out of a passed lvalue.
- `specs/systems/render.spec.cpp` — out-of-bounds `srcRect` reports; in-bounds
  does not; the report is throttled to four.
- `specs/input/keyboard.spec.cpp` — new; edge-triggered transitions.

All three platform targets must build, since the Switch build is the one that
catches throwing constructs reachable from headers:

    make -f Makefile.debian && make -f Makefile.debian test

## Compatibility

No layout changes. `sizeof(Registry)`, `sizeof(Entity)`, `sizeof(System)`,
`sizeof(Signature)` and `sizeof(Tile)` are all unchanged, so 1.4.0 is a relink,
not a rebuild — unlike 1.3.0.

No public signature is removed and no behaviour of an existing call changes,
with the single exception below.

### The one judgment call

The `AddSystem` change from `Targs &...` to `Targs &&...` is source-compatible
for every deduced call — all 107 in this repository continue to compile — but
breaks the explicit-template-argument form `AddSystem<Sys, int>(x)` with an
lvalue `x`. There are zero such calls in this repository. It is being taken
because it fixes a silent move-out-of-caller bug in the current signature, and
it will be recorded in `UPGRADING.md` and `CHANGELOG.md` as the exception it
is.

## Deliverables

1. The engine changes above, as 1.4.0.
2. `docs/UPGRADING.md` and a `CHANGELOG.md` entry.
3. `KNOWN_ISSUES.md` updated: each entry that now has a runtime diagnostic says
   so, since "cannot be fixed" and "cannot be detected" are different claims.
4. A GitHub issue on `SamsWebs/center-ice-hockey` giving that team advance
   notice. CIH is pinned at v1.2.8, so its upgrade spans two minors and
   includes the 1.3.0 `AssetStore` layout change, which requires a rebuild
   rather than a relink. The issue body is to be reviewed before it is posted.
   The `gh` CLI is not currently installed on this machine.
