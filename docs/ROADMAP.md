# Roadmap

Planned work, in rough order. This is not a defect list — `KNOWN_ISSUES.md` holds
those, and every entry here that fixes one links to it.

The point of this file is the *reasoning*. Anyone can re-derive a task list; what
gets lost between sessions is why the order is what it is, and which decisions
were already argued and settled.

**Current release:** 2.3.1. **Next:** 2.4.0. The forward-looking section is
[What's next — the 2.4 → 3.0 line](#whats-next--the-24--30-line), added
2026-09-17; everything below it is history and stays as it is.

---

## What's next — the 2.4 → 3.0 line

Added 2026-09-17. This is the first forward-looking section in this file. It was
written against the tree at 2.3.1-6-g2430bf9 plus an audit of Center Ice Hockey's
consumption of the engine.

### The audience decides the order

This phase is not ordered by risk to existing consumers, which is how 2.0.0 was
ordered. It is ordered by what a game that has not been written yet hits first,
because that is who it is for.

A new game hits, in its first week: a way to finish a frame, a way to scale its
UI, a way to draw and measure text, a way to play sound, and a way to read input.
It does not hit component-membership re-evaluation, storage layout or system
ordering until it is large enough to spawn entities mid-frame — by which point its
own code has accumulated enough that a breaking change is expensive for it. So the
additive work goes first and the ECS wave goes last, in a major, deliberately.

That is the complement of 2.0.0's own lesson, not a contradiction of it. 2.0.0's
rule was *spend the whole breaking budget at once*, and the same file records
taking `GameStateMachine`'s copy operations late "because the alternative was
spending a whole major on a one-line fix". Both hold at once: batch the breaks,
and do not smuggle an additive feature into a major because a major happens to be
coming.

### Why this list, and not a list someone preferred

Three sources, and an item has to come from one of them:

1. **The engine already knows it is wrong.** `KNOWN_ISSUES.md` and the carried
   lists in this file — the ECS membership trap, the `gameState.h` include cost,
   the unversioned `.map`, the Switch networking halves.
2. **A shipping game built it by hand because the engine had nothing.** Measured
   below. This is the strongest evidence available, because the cost has already
   been paid once by someone who had no choice.
3. **A release already fell through it.** The verification gaps, the dead source
   list, a stale plan read as a work queue.

What is deliberately *not* a source: what would be pleasant to have. A wish is
not a roadmap item, and the promotion test in "What we will not do" is what keeps
that honest.

### The evidence: one consumer, measured

Center Ice Hockey is the only consumer tracking the 2.x line. The other two games
on this engine (`Conan-the-Caveman-Android`, `etw2kxx`) are pinned at **v1.2.1**,
so the sample for "what a future game wants" is currently one game — stated here
as the section's main risk, not buried.

Measured on 2026-09-17:

```sh
# Which engine headers the game actually includes
grep -rhno "stormengine2/[a-zA-Z0-9_/.]*" src/ | sed 's/.*\(stormengine2[^"]*\)/\1/' \
  | sort | uniq -c | sort -rn
```

**Ten of the engine's 38 headers.** `assetStore.h`, `states/gameStateBase.h`,
`gameStateMachine.h`, `logger.h`, the net trio, `collision/shapes.h`,
`input/virtualGamepad.h`, `input/touchControls.h`. Zero ECS, zero components,
zero systems.

That number is not automatically a defect — a large game may legitimately use a
third of its library — but four of the ten findings below are places where the
*game wrote something the engine also ships*, and that is a defect whichever way
the usage number is read:

| # | The engine ships | The game wrote | Measured |
|---|---|---|---|
| 1 | A `GameState` with no way to finish a frame | `CihState::Present()` | **0** `SDL_RenderPresent` in `common/`, **36** across 18 example source files |
| 2 | `input/actionMap.h`, spec'd and bridged | 627-line `inputReader.h` | `actionMap.h` has **zero consumers** outside its own spec — no example, no game, nothing |
| 3 | `text.h` (`Text::Draw`/`Measure`) | 596-line `ui/text.h` with its own `DrawText`/`TextWidth`, which does not include the engine's | `text.h`'s own comment records four examples writing their own copy, and the copies diverging |
| 4 | Nothing at all for layout scale | `ui/scale.h` — `UiScale`/`Px`/`FontPt`, pure and SDL-free | 9,367 lines under the game's `src/ui/`, of which the scale pair is the plainly generic part |

And three more where the engine has no counterpart at all, each describable
without naming hockey, which is the test for engine-shaped:

- **Audio.** `AssetStore` caches a `Mix_Chunk`; there is no music, no channel
  owner, no volume model and no way to turn engine-side sound off. The game has
  607 lines of SDL_mixer glue over a pure volume header.
- **Host address enumeration.** The engine has a whole UDP net layer and nothing
  that answers "what is my LAN address", which is the first question the host of
  a LAN match has to answer in the UI. The game wrote it per platform
  (`getifaddrs` / `getaddrinfo` / a Switch guard), then split the *ranking* rule
  into a pure spec'd header.
- **Unversioned `.map`.** The editor format carries no version field, has two
  hand-rolled parsers, and no round-trip spec. 2.0.0 already had to change
  `sizeof(Tile)` once for data the editor had been writing all along.

### How an item here is read

Every item carries three things a bare task list drops: **effort**, **what a
consumer has to do**, and **how you know it landed**. The middle one is the cost
nobody prices — an engine change that forces every game to rebuild is not free
because it was additive.

**An item is done when a spec fails without it, or when an in-tree example uses
it** — never because the build is green and the header exists. This repo has the
scar for that: `actionMap.h` shipped spec'd, bridged into `compat/global.h` and
used by nobody, and nothing anywhere said so.

| Item | Effort | What a consumer must do |
|---|---|---|
| Track 0, all six | S each | nothing |
| 2.4.1 `GameState::Present()` | M | call it where `SDL_RenderPresent` is called today |
| 2.4.2 `ui/scale.h` | S | nothing — new header, opt in |
| 2.4.3 `text.h` verbs | S | nothing — additive statics |
| 2.4.4 `version.h` | S | nothing; examples adopt `--version` |
| 2.4.5 debug overlay | S | nothing — opt in |
| 2.5.1 version the `.map` | M | rebuild; **the editor must write the new version too** |
| 2.5.2 asset path seam | M | nothing, unless the game wants a writable base |
| 2.5.3 blob lifetime rules | S | nothing — documentation |
| 2.6.1 engine mixer | M | nothing |
| 2.6.2 `input/inputHub.h` | L | opt in; `ActionMap` unchanged |
| 2.6.3 an example adopts it | S | nothing |
| 2.7.1 host address enumeration | S | nothing |
| 2.7.2 the Switch halves | S | Switch consumers rebuild |
| 3.0.0 the ECS wave | L | rebuild + an `UPGRADING.md` entry per break |

**Effort is a band, not an estimate:** S is under a day, M is one to two, L is a
week or more of the kind of work this repo actually does — the 2.0.0 entries
carried "Hours" and "About a day" for the same reason, and one of them was wrong
in the optimistic direction. Treat the column as sequencing input, not as a
promise.

### Track 0 — the repo, not the library (continuous, unversioned)

These are not features and should not wait for a release. Each is a hole a
release already fell through.

1. **Finish retiring the guardrails plan.** ✅ **Done 2026-09-22.** The plan
   file was already deleted in PR #61. The disposition table (eight of eleven
   traps fully resolved, three half-done — re-verified 2026-09-22) now sits at
   the head of
   `docs/superpowers/specs/2026-08-30-engine-guardrails-design.md`, with the
   body left as history and the file marked retired. Traps 8, 10 and 11 remain
   half-done: field-site docs and no main loop, not architecture.
2. **`docs/TECH_DEBT.md` is gitignored** — listed in `.gitignore`, so a fresh
   clone has no ledger and CI cannot check it. ✅ **Durable half moved
   2026-09-22** into "Carried from the TECH_DEBT notebook" below. The notebook
   stays the reasoning file (it is still gitignored by design); every open item
   that matters now lives here. Verified still open on the move date:
   `loadFilemapEditor`'s third silent-failure mode (P17 residual — modes one
   and two fixed by `5758dfc`, the missing `eof()`/`fail()` check was not),
   unguarded `GetEntityByTag` in examples (P19 residual), Logger level filter
   and header `<iostream>` (P28), `netSocket::fd_` truncation (P44 residual),
   missing net unit specs (P31), `template/` out of CI (P65), no SONAME (P67),
   and guardrails traps 10/11 field-site docs.
3. **Fix this file's own stale statuses.** ✅ **Done 2026-09-22.** Headings for
   the non-ECS collision entry and the lighting overlay carry `— **DONE**`;
   circle colliders are marked done in the status block at the top of
   "After 2.0.0 ships". The stale "IsAlive walks freeIds" claim under
   "Carried, not blocking" was corrected the same day.
4. **One canonical source list.** `examples/nx-platformer/Makefile` and the
   Android CMake glob have both been non-recursive before, and the recurring fix
   written down twice was "emit the canonical source list once and have every
   build path read it". Nothing has. That is one file, and it retires a whole
   class of defect rather than an instance.
5. **State the verification matrix.** Done for this file — see "What CI
   actually verifies, and what ships green" under [Build and CI](#build-and-ci).
   The remaining half is the release checklist, so "all targets build" can never
   again be written from intent.
6. **A layout/measurement harness for the examples.** The engine ships 14
   examples with UI and no way to look at one without playing to it. The flagship
   game's `tools/layout_shot.cpp` renders the real font at the positions the
   layout code computes and writes a PNG under Xvfb. One tool serves every
   example, and the rule it enforces — *look at a screen you changed* — has caught
   four layout defects in one session in the game that adopted it.

### 2.4.0 — frame and presentation seams (additive)

Nothing here changes a public signature, a struct layout or a member. The layout
pins in `specs/layout.spec.cpp` are the gate: a size that moves in a minor is a
bug in this plan, not a licence.

1. **`GameState::Present()`.** A non-virtual method with no new member, which is
exactly the trick `CapFrameRate` already uses in that header — "Non-virtual and
adds no member, so it changes neither `GameState`'s layout nor its vtable." It
draws the registered overlays and then calls `SDL_RenderPresent` once. The
motivation is specific: on Android the touch overlay must be drawn *before* the
present, and a state that calls `SDL_RenderPresent` itself ships a menu with
invisible controls that looks perfect on every other platform. That defect
shipped once in the flagship game; the fix there was a base class rather than a
convention, and the engine is where it belongs.

   **Who owns the overlay list is a design decision this file deliberately does
   not make** — it belongs in the implementation plan, with two constraints
   already settled by the reasoning above: one call draws overlays *and*
   presents, so the ordering cannot be got wrong by a caller, and the ordering
   itself is spec'd rather than implied. What it must not become is a second
   convention a game can bypass by calling `SDL_RenderPresent` directly, because
   a convention is what failed the first time.
2. **`stormengine2/ui/scale.h`.** `UiScale(windowHeight)` / `Px(v, h)` /
`FontPt(basePt, h)` — pure, SDL-free, spec'd. A game that scales its fonts but
not its literal offsets gets a 4K layout whose halves drift apart; a game that
scales both by different factors gets the same thing more slowly. One function is
what makes them agree by construction.
3. **`text.h` grows the drawing verbs it is missing.** Centre and right
alignment, a measured fit that *marks* a truncated string rather than clipping it
silently, and a footer built from a list of parts so it can wrap between verbs.
All additive statics; the existing four keep their signatures. The engine's own
`text.h` comment already documents why this matters — three example copies
diverged until one of them re-opened the font per call.
4. **`stormengine2/version.h`.** A compile-time `kEngineVersion` and
`VersionString()`, so a binary can answer for itself which engine it was built
against, and the release number gets **one source** instead of a hand-bump in
`Makefile.debian`, `Makefile.win` and eight places in `web/index.html`. A compile
flag is not a property of the artifact; this is the smallest fix for that. Being
a library, the engine cannot print — the header is the answer, and the examples
adopt it in a `--version` switch so at least one built binary in the tree does.
The mechanism is a generated header, not a second hand-edited copy of the
number, or it has moved the problem rather than fixed it.
5. **A debug overlay.** The cheapest item on the previous list and still the
right one: FPS, frame time, entity count, per-system timings and the last few
`Err` lines, toggled with a key. It is also the only diagnostic surface a game
ships to a player, which the logger is not.

### 2.5.0 — data and asset seams (additive, plus one format change)

1. **Version the `.map`.** A header line carrying a format version, one parser
instead of two, and a round-trip spec. The rule being adopted is the flagship
game's config contract: *unknown keys are ignored* covers a key being added or
removed, and does **not** cover a key that still parses and now means something
else. A `.map` with no version cannot tell a reader which of those it is, and
2.0.0's `Tile` change is the proof that this format does move.

   Two consumers, and the second is the one that gets forgotten: the engine's
   `TileMapLoader`, and `editor/src/utilities/FileLoader.cpp`, which writes the
   format with its own stream of `<<` and is a separate binary that has to be
   rebuilt and repackaged in the same release. **A map written before the version
   existed must keep loading** — the absent header reads as version 1 — and a map
   declaring a version the build does not know must be refused with a
   diagnostic, which is the half that costs anything. Neither parser bounds the
   record it is reading beyond the stream's own conversions, so the version line
   is also the place to state what a malformed record does.
2. **The asset path seam, written down and completed.** `AssetPath` (the game
path a pack-aware loader consumes) and `AssetFilePath` (the path a real file is
opened at) are already two functions and already the source of a shipped defect —
`PackEntryName` gained a `./` strip because a game handed the second one's shape
to the first and the pack was skipped. Add the third piece: a writable base and a
readable-override resolver, so "shipped content is read-only, the player's
replacement lives beside the save" is one call rather than a per-game convention.
Document all three together in `docs/assets.md`, including the pack-vs-loose
fallback.
3. **Raw bytes for the loads the store does not cache.** `ReadBlob` is on the
branch and **unreleased** (`68b3940`, after the `v2.3.1` tag), and it is **absent
from `CHANGELOG.md` altogether** — zero hits, while `LoadPack` from the commit
before it has an entry. That is the repo's own rule broken in the commit that
added the API: "a public API change is documented in the same branch that makes
it." Add the entry, and add the second half this item is about: the statement of
which loads are synchronous and which are lazy, because the two have different
lifetime rules. A font opened over memory reads its bytes at render time; an
image decoded through `IMG_Load` does not. That rule currently lives in a game's
header comment. It belongs next to `ReadBlob`.

### 2.6.0 — audio and input (additive)

1. **An engine mixer.** Music versus sfx channel ownership, a pure volume model,
pack-aware loads, and a seam for a game to hold its own music. The engine already
links `SDL2_mixer` and already caches `Mix_Chunk`, so this adds a policy layer,
not a dependency.
2. **Make the input layer usable — add, do not rewrite.** `actionMap.h` has no
consumer anywhere in the repo, and it is not because nobody found it: the three
defects the flagship game's own reader documents are the reason a game wrote its
own 627 lines.
   - the event queue is drained only by the state on top, so a state pushed under
     freezes — a pad plugged in while a child screen is up is never enumerated;
   - hot-plug state must be process-wide, not per-state;
   - edges must be per-state, so a new screen does not inherit the previous
     screen's "was down".

   All three are **additive fixes** if the answer is a new header rather than a
   change to `ActionMap`: an `input/inputHub.h` that owns the device list and the
   poll, hands each state its own edge trackers, and leaves `ActionMap` as the
   binding table it already is. The repo rule applies — correct or extend the
   existing mechanism before adding one — and here extending it means putting the
   `ActionMap` per state *inside* a hub that owns what must be process-wide. That
   is why this item stays in a minor instead of drifting into 3.0, which the first
   draft of this section let it do by conflating "the abstraction is unused" with
   "the abstraction must break".
3. **Then make an example adopt it.** `examples/shooter` or `examples/sports`,
   whichever is smaller, moves to the hub and drops its hand-rolled polling
   (`grep -rn SDL_PollEvent examples/ --include=*.cpp` is **16 sites across nine
   examples**). This is the step that is easy to skip and is the whole point: an
   abstraction proven only by its own spec is not proven, which is exactly what
   happened to `actionMap.h`, and nothing anywhere said so, because an uncalled
   function is warning-free and the build is clean.

### 2.7.0 — net polish

1. **Host address enumeration**, engine-side and per platform, with the ranking
rule split into a pure header the way the flagship game already did it. A net
layer that cannot tell the host its own LAN address leaves every game to solve
it.
2. **The Switch halves.** `NetSocketsInit()` needs a `socketInitializeDefault()`
arm, and `examples/nx-platformer/Makefile` needs a recursive source glob
(`SOURCES := src src/states src/components include/stormengine2`, where
`include/stormengine2` is a symlink to `common/` and therefore reaches the six
top-level `.cpp` files and none of the seven under `common/net/`). These must land
together: the first is masked by the second and surfaces the moment it is fixed.
**Neither has been built here** — devkitPro is not on this machine — so both are
ledger claims verified only as *still present in the source*, which is a weaker
thing than verified as *broken*, and should be repeated to whoever builds it next
rather than asserted.

### 3.0.0 — the ECS wave (breaking, reserved, unscheduled)

Reserved, not dated. It happens when a game is large enough that one of these is
a measured cost, and the measurement is named at each item so "it felt slow" is
not the trigger.

1. **Component changes re-evaluate system membership.** `KNOWN_ISSUES.md` §5
calls this "the single largest correctness trap in the ECS": membership is
computed once per entity, so a component added after admission is invisible to
that system forever. The workaround is "kill it and create a replacement", which
is a workaround a game should not have to know. Trigger: any consumer that wants
it.
2. **A component-change hook, or membership the ECS can match as a union.** This
is what removes `ContactSystem`'s per-frame narrowing scan — and the previous
list's correction stands: a better broadphase does **not** remove it, because the
scan exists because nothing notifies a system when a component is added. Trigger:
a game with thousands of transform entities and a handful of bodies.
3. **Trim `gameState.h`'s includes.** `KNOWN_ISSUES.md` §8 — 145,947
preprocessed lines for a 23-line interface. `gameStateBase.h` took the 45% that
was free in 1.3.0; the remaining trim is a source break for anything leaning on
the transitive path. Trigger: scheduled with the wave, since it is free once a
major is already happening.
4. **Sparse-set component storage, and a system scheduler.** Both are on the
"also on the 2.0.0 list" in `KNOWN_ISSUES.md`. Storage is O(highest id ever
used) today and requires every component to be default-constructible; ordering is
the caller's problem with no way to declare it. Neither is a defect; both are
the shape of thing a mature game asks for.

### Versioning policy

Written down because "you pick the versioning" should not have to be asked
again.

| Change | Release |
|---|---|
| New header, new statics, new free function, new non-virtual method body | **Minor** (2.4, 2.5, 2.6, 2.7) |
| `= delete` of a copy operation on a type that cannot usefully be copied | **Minor**, with a CHANGELOG note |
| Public signature change, struct layout change, member deletion, include trim | **Major** (3.0) |
| `.map` or other on-disk format change | **Minor**, with a version field and a refusal path |

Three rules that go with the table:

- **A layout pin moves in the same commit that moves the layout**, and
  `specs/layout.spec.cpp` is the only place the number is allowed to change. A
  size that moves without a pin edit fails the suite, which is the point.
- **`~dev` sorts below the release; `-dev` does not.** A development version
  stays `2.4.0~dev`. The 2.0.0 entry explains why the tilde is not a hyphen: a
  game gating on `--atleast-version=2.4.0` would otherwise pass against a
development build missing most of the release.
- **Releases come from a pushed tag**, never from a merge. Already true; keep it.
  It stopped being true once and published a set of breaking changes as a patch.

### What we will not do, and why

This is the part that keeps the list a roadmap rather than a wish list.

**The promotion test.** Something moves into the engine when a second game wants
it, *or* when it is plainly engine-shaped — describable without naming a game.
Center Ice Hockey is one game, so the second test is doing most of the work here,
and it is stated as such rather than dressed up as demand.

**Stays in the game, deliberately:**

- **The hockey domain.** Rink geometry, 639 clubs across 16 tiers, the season
  and franchise simulations, trades, awards, the database and its migration
  contract. None of it is describable without naming hockey.
- **The screen models.** 30 per-screen column headers (`ls src/ui/*Columns.h`
  in the flagship game), the menu row lists, the draft board. A table model is a
  per-screen decision; the engine shipping one would be the engine shipping an
  opinion. `dataTable`-style *measurement* primitives are a different question and
  are in 2.4.0; the *content* is not.
- **The database contract.** A player's own rows never destroyed, a schema
  change migrated rather than discarded. The engine has no persistence and
  should not grow one to host this.
- **Anything whose only argument is that a dozen screens would use it.** That
  was the argument for `actionMap.h` too.

**Carried, and named so they are not rediscovered as new:**

- `Makefile.win` has no `-pthread` while two spec files use `std::thread`.
- `examples/examples.mk` links `-lstormenginev2` with no `-I` at the source tree,
  so an example build resolves the *installed* headers. Documented, not enforced;
  an automated tree-vs-install check was written and reverted because CI builds
  from a separate tree.
- Comments in `common/` cite line numbers and the line numbers rot. Cite
  function names instead; this is a sweep, and a half-swept file is worse than an
  unswept one.
- `editor/` links only where `libnfd` exists, which Debian and Ubuntu do not
  package.

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
pkg-config.

`examples/nx-platformer` was edited the same way and then actually **built**
with devkitPro, which turned up two bugs that had nothing to do with
namespacing and everything to do with the tree never being built:

- The Makefile passed `-I$(TOPDIR)/../../vendor` intending to supply glm, but
  glm is at `vendor/android/glm`, so `<glm/glm.hpp>` resolved to
  `vendor/glm/glm.hpp`, which does not exist — and devkitPro ships no glm
  portlib. The build failed on the first engine header it reached. The desktop
  build hid it completely: there glm comes from the system `/usr/include`,
  which a cross build must not use.
- `VPATH` searches for **targets** as well as prerequisites, and
  `include/stormengine2` is a symlink to `common/`, where the desktop build
  leaves its x86-64 `.o` files. Make found `common/ecs.o` while looking for the
  aarch64 `ecs.o`, decided the target was satisfied, never compiled it, and
  passed a bare name to the linker. Six engine translation units silently went
  unbuilt and the error blamed `ld`. Narrowed to per-pattern `vpath`, and
  verified by rebuilding with the desktop objects deliberately present.

The namespace edits made blind in that tree turned out to be correct — but that
was luck rather than verification, and the two bugs above are what "edited but
never built" actually costs.

`examples/android-platformer` was **confirmed working by the maintainer on a
machine with the NDK**, along with the Switch build. Neither toolchain is on the
machine this branch was developed on, so that confirmation is the only evidence
for the Android tree — it is not covered by CI and nothing here can re-check it.

Android was structurally immune to both bugs the Switch tree had, which is worth
knowing before assuming the two platforms fail alike:

- `app/jni/CMakeLists.txt` names `vendor/android/glm` on the include path
  directly, so the glm path bug could not occur.
- It uses `file(GLOB_RECURSE ENGINE_SRC "${REPO_ROOT}/common/*.cpp")` and CMake's
  own object layout, so there is no `VPATH` target lookup for a stale desktop
  `.o` to satisfy. It also picks up every engine translation unit, where the
  Switch Makefile's non-recursive glob picks up six.

**The standing gap is unchanged:** CI builds neither. Both trees can break on a
future engine change and nothing in the repo will say so.

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

**Status re-checked 2026-09-17, because three of these headings did not say they
were done and one of them reads as a queue it is not.**

| Item | State |
|---|---|
| A non-ECS collision entry point | ✅ **Done** — `common/collision/shapes.h`; its own entry says so further down. |
| A lighting overlay | ✅ **Done** — `common/lighting.h`, 2.3.0. |
| Circle colliders and mixed-shape sweeps | ✅ **Done** — `CircleColliderComponent`, uniform grid broadphase. |
| Extract the broadphase sweep | ⬜ **Open** — `common/collision/` holds `shapes.h` only; the sweep is still inside `ContactSystem`. |
| An example with sustained entity churn | ⬜ **Open** — nothing in `examples/` creates and destroys entities continuously. |
| A debug overlay | ⬜ **Open** — and moved into 2.4.0 above. |

### A non-ECS collision entry point — **DONE**

The contact math is already ECS-free and does not know it. `ContactAABB` is four
floats with no engine types in it, and both `Overlaps(a, b)` and
`Manifold(a, b, normal, depth)` take nothing but `ContactAABB` — they are just
statics tucked inside `ContactSystem`. Only three things are actually ECS-bound:
`BoundsOf(Entity)`, `Contact` holding `Entity a, b`, and the sweep iterating
`GetSystemEntities()`.

This is worth doing because there is a real consumer: a game that uses none of the
ECS cannot reach `ContactSystem` at all, and ends up hand-rolling overlap tests the
engine already has.

**~~Hours — expose the primitives.~~ Already done, and this entry was wrong to
imply otherwise.** `Overlaps`, `Manifold` and `MinimumTranslation` are already
`public` statics, above the `private:` in `contact.h`. Verified by compiling a
program against the installed 2.0.0 headers that calls them with no `Registry`,
no entities and no components: it builds, runs, returns the right numbers, and
`nm -u` reports **zero** undefined ECS symbols.

Center Ice Hockey's own design audit reached this independently and struck the
question from its list — *"Does the engine need a non-ECS entry point? No — it
already has one."* The one engine-side ask it kept was **documentation**: nothing
in `docs/` says the collision math is usable standalone, which is why a consumer
had to discover it by experiment.

**The real dependency is shape, not coupling.** A game reaching for this is
reaching for collision, and at the time this was written the engine had **box
colliders only** — a grep for `radius` or `Circle` across `common/components/`
and `common/systems/` returned nothing. (Both halves have since landed; see
**Done** and **Also done** below. The paragraph is kept as the case that was
made, not as a description of the tree.) Center Ice Hockey's physics is round throughout: puck-vs-boards keeps a
`radius` off the boards, skater separation takes a `radius`, and the net posts
are `postRadius = 3.f`. So `Overlaps`/`Manifold` are square math applied to round
bodies, which is *approximately* usable and quietly wrong at the edges — a puck
that dings a round post off a square corner has a different feel for no gain.

Circle support is therefore the item that actually unblocks a consumer, and it is
additive: no break, ships in a 2.x minor.

**Done.** `common/collision/shapes.h` — glm and nothing else from the engine.
`ContactAABB` moved there, `ContactCircle` joined it, `Overlaps` and `Manifold`
are overloaded for all three pairings, and `MinimumTranslation` gained a
`(normal, depth)` form a game with no ECS can call. `systems/contact.h`
forwards, so `ContactSystem::Overlaps(a, b)` is unchanged.

**Also done, in a follow-up.** `CircleColliderComponent` exists and
`ContactSystem` sweeps mixed shapes, so a circle contact now comes out of the
ECS sweep and pairs against boxes. Two consequences worth carrying forward:

* `ContactSystem` requires `TransformComponent` **alone**. A signature is an AND
  of required components, so it cannot express "box OR circle", and a second
  system for circles cannot see the box side of a mixed pair. The narrowing to
  actual colliders happens inside `Update()`, which is a per-frame scan over
  every transform entity — new work for a game with thousands of sprites and a
  handful of bodies. A component the ECS could match as a union, or a broadphase
  that indexes colliders once instead of rescanning, would remove that scan; it
  is not worth a break on its own.
* A collider added to a live entity now takes effect in both collider systems,
  because membership is decided on the transform and the collider is re-read
  each frame. That is a side effect of the widened requirement, not a fix for
  KNOWN_ISSUES #5 — every other system still freezes its component set at
  admission — but it is now observable behaviour with specs on it, so a later
  broadphase rework has to keep it.
* `RenderColliderSystem` draws both shapes now, through `ContactSystem`'s own
  statics rather than its own copy of the arithmetic — the overlay and the sweep
  agree by construction, including the box-wins rule. It took the same widened
  requirement, so both systems now report transform entities from
  `GetSystemEntities()`. The older gap there went with it: `Update` now takes an
  optional camera, so a scrolling game's outlines land on its sprites instead of
  at the raw world position.

### What the adversarial review of the follow-up found

Nothing that blocked the merge, and four things worth fixing, all in the debug
overlay or its surroundings rather than in the collision math — which is the
half that had already been reviewed hard.

- **`static_cast<int>` of a NaN, or of a float past `INT_MAX`, is undefined
  behaviour**, and the circle rasteriser costs one loop iteration per pixel of
  radius. So a `transform.scale` bug did not produce a wrong picture, it
  produced a per-frame hang or UB. Fixed by bounding what the overlay will draw
  — 16384 px of radius, 1e7 px from the origin, finite only — with the box path
  bounded the same way, which it had never been.
- **The draw colour was set per entity.** Harmless when membership was
  collider-carrying entities; wasteful and state-clobbering once it became every
  transform entity. Hoisted out of the loop.
- **`ContactSystem::CircleOf(const Entity &)` shipped with no spec.** It is
  public API the sweep itself does not use, carrying the same footgun as
  `BoundsOf`'s `Entity` overload: `GetComponent` hands back a shared zeroed
  fallback on a miss rather than reporting one. Specced.
- **Four line citations had rotted**, pointing at arithmetic that no longer
  lived where they said. Converted to function names, per the item below.

**Carried, not fixed.** A NaN in `transform.position` produces NaN bounds, and
`ContactSystem`'s sweep then sorts on a comparator that returns false both ways
for a NaN — which is not a strict weak ordering, so `std::sort` is undefined.
This predates circles: the box path always had it, and no spec has ever fed the
sweep a NaN. The overlay now guards itself against exactly this and the sweep
does not, which is the asymmetry to close. Carried below rather than filed in
`KNOWN_ISSUES.md`, which is for defects whose fix needs a compatibility break —
dropping a non-finite body from the sweep needs none.

### What the adversarial review of that branch found

The first commit claimed "eleven mutants, all killed". True, and worthless:
**23 of 29 survived**, because all eleven lived in the two cases already in the
author's head. Deleting *both* Y-axis conjuncts from `Overlaps(AABB, AABB)`
passed all 492 tests — nothing in the repo, across four spec files, had ever
tested a pair overlapping in X and separated in Y.

Four correctness bugs, all from the same root cause: `Overlaps` and `Manifold`
were two expressions answering one question, and the header promised they never
disagree.

- **Box corners** — the case circles exist for. `Overlaps` compared squared
  distances, `Manifold` took a square root that can round up to exactly the
  radius. Disagreed on ~1.8% of near-tangent probes.
- **Zero-size `ContactAABB`** — which is `BoxColliderComponent`'s *default*,
  since width and height both default to 0.
- **Negative radius** — squared by one, compared signed by the other;
  circle/circle agreed on a *negative* depth, which drives a pair together.
- **NaN** — "yes" from one, "no" from the other, with the NaN escaping into the
  caller's output.

Fixed structurally: one solver per shape pair, both entry points call it. The
class is impossible now rather than merely absent. That also fixed an
outputs-untouched violation the branch introduced and a `depth == 0` contact
that `MinimumTranslation` turns into `(0, 0)` — a collision no caller can
resolve.

**The lesson, again, and it is the same one.** A green check was read as
evidence about the code when it was evidence about the check. "Eleven mutants
killed" measured the mutants chosen, not the code covered. Choose mutants from
the code's branch structure — every early return, every tie-break, every clamp
in both directions — not from the cases that come to mind, because those are
the cases already tested.

**And a documentation correction became a documentation error.** The branch
correctly identified that `MinimumTranslation`'s comment was backwards, then
replaced it with a stronger claim that is also false: it is a minimum
translation only when `depth` is a true separation distance, which for box vs
box it is not when one box is contained within the other along the chosen axis.
`A={0,0,10,10}` against `B={2,-5,4,15}` reports depth 2 where 4 is needed.
Reachable in `examples/sports`, where a 16px puck can land inside a 24px board
after a frame hitch. Confidence in a correction is not evidence for it.

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

**A third measured problem, ahead of the sweep extraction — since fixed.**
`contact.h` used to record that the broadphase sorted on the X axis only, so
*"everything stacked in one column degrades to the old all-pairs cost"*. A hockey
rink puts ten skaters and a puck in a tall narrow space, which is close to that
degenerate case — so the consumer most likely to adopt the sweep was also the one
it served worst. The uniform grid landed first, exactly so the extraction would
not carry the one-axis assumption into a new public API.

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

### A lighting overlay — **DONE**

Generalises a technique proven in a shipping game: two quarter-resolution RGBA
surfaces built once and cached — a warm key layer whose alpha follows a falloff
function, and a cool vignette layer whose alpha is the **inverse** of the same
value — both `SDL_BLENDMODE_BLEND`, upscaled to full screen with two
`SDL_RenderCopy` calls.

Quarter-res is why it is cheap; the inverse-alpha pairing is why it reads as
lighting rather than a tint. No shaders, so it runs on the SDL2 renderer on every
target including Switch and Android. That portability is what makes it engine
material rather than a desktop nicety.

**Done.** `<stormengine2/lighting.h>` — `LightingOverlay::Build` once, `Draw`
last, over the finished frame.

The design question this listed as needing to be settled first — does the engine
own light *entities* in the ECS, or is this a standalone overlay a state drives?
— was settled as the **standalone overlay**, and the reasoning is worth keeping
because the choice is hard to reverse. The technique is one key light and its
complement, not a set of lights: there is nothing per-entity to store, and a
`LightComponent` would imply a compositing pass the engine does not have. It also
keeps the header ECS-free, like `text.h` and `collision/shapes.h`, so a game
using none of the entity machinery still gets it. Light entities remain
available later as an additive layer on top; the reverse would not have been.

### A debug overlay

FPS, frame time, entity count, per-system timings, and the last few `Err` lines,
toggled with a key.

The cheapest item on this list and the natural counterpart to 2.0.0's diagnostics,
which currently only reach a log nobody reads during play.

---

## Build and CI

### What CI actually verifies, and what ships green

The matrix, so that "all targets build" is never again written from intent. Read
off the three workflow files on 2026-09-17:

| Target | Verified by | Covered |
|---|---|---|
| Engine library, specs, amd64 + arm64 `.deb` | `build-and-release.yml` (on a pushed tag) | ✅ |
| Nine desktop examples linked, `editor/` compiled to objects | `pr-validate.yml`, in the Docker image | ✅ |
| Seven examples built for Windows, plus `windows-platformer` against the packaged SDK zip | `build-and-release.yml` | ✅ |
| `examples/nx-platformer` (devkitPro, `-fno-exceptions`) | nothing | ❌ **ships green** |
| `examples/android-platformer` (NDK + six submodules) | nothing | ❌ **ships green** |

The editor is compiled and never linked because `libnfd` has no Debian package;
the two uncovered trees are uncovered because neither toolchain is in the image
and `.dockerignore` keeps both out of the build context. Both facts are recorded
in `.github/scripts/ci-build-examples.sh`, which is the file to read before
changing this table.

The consequence is the one that matters: **a change to a header every target
compiles is unverified on two of the three platforms until a release is built.**
That is exactly how a `getifaddrs` call reached the flagship game's shared header
and passed 5.5k specs on Linux while Switch, Android and Windows could not build
at all. When a change touches `common/`, say which targets were built and which
were not; item 5 of Track 0 is to make that a checklist rather than a habit.

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

The circle-collider branch converted four of them — three citing
`renderCollider.h:22-26` for offset-and-scale arithmetic that had moved into
`ContactSystem::BoundsOf`, and one the branch had introduced itself. That is not
the sweep; the `common/ecs.cpp:439` citation this item names is still wrong, and
the point about half-swept files stands.

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

  **There are two recorded divergences from upstream imgui now, not one.** The
  cleanup pass dropped `imgui_demo.cpp`, which was being compiled by
  `editor/Makefile`'s `$(wildcard ../vendor/imgui/*.cpp)` and called by nothing,
  and then had to remove six declarations from `imgui.h` — `ShowDemoWindow`,
  `ShowAboutWindow`, `ShowStyleEditor`, `ShowStyleSelector`, `ShowFontSelector`,
  `ShowUserGuide` — because leaving them declared let a call compile and fail at
  link. `IMGUI_DISABLE_DEMO_WINDOWS` looks like the supported answer and is not:
  the empty stubs it selects lived in the deleted file. Both deltas are in
  `vendor/MANIFEST.md`; anyone taking the pin off 1.79 has to reapply or discard
  them deliberately.

Editor warnings that remain (narrowing, sign-compare, a duplicate `clean`
recipe between `editor/Makefile` and `base.mk`) are pre-existing and untouched.

---

## Carried, not blocking

Reviewed, ruled on, and deliberately left. Listed so they are not rediscovered as
though new.

### Carried from the TECH_DEBT notebook (moved 2026-09-22)

The gitignored `docs/TECH_DEBT.md` keeps the full evidence and reasoning; this
is the durable open half. Each row re-verified on the move date — fixed items
from that notebook are **not** repeated here.

| Item | What | State |
|---|---|---|
| P17 residual | `loadFilemapEditor` ends its read loop with no `fail()`/`eof()` check, so a malformed record mid-file truncates the level silently. Modes one and two (unopenable file report, `strtol` instead of `stoi`) fixed by `5758dfc`. | 🔴 Open |
| P19 residual | `GetEntityByTag` still `.at()`s (precondition documented; `TryGetEntityByTag` and `DoesTagExist` exist). **Zero** call sites under `examples/` use the safe forms — 17 raw `GetEntityByTag` calls remain. | 🟡 Partial |
| P28 | No `Logger::SetMinLevel` / no opt-out: every entity/component add still formats and writes `std::cout`. `<iostream>` still in three public headers: `logger.h`, `tilemapLoader.h`, `gameStateMachine.h`. | 🟡 Partial |
| P44 residual | `NetSocket::fd_` is `int`; a Win64 `SOCKET` truncates. MSVC compile break fixed (`_getpid`). Widening `fd_` changes `sizeof(NetSocket)` embedded in `NetServer`/`NetClient` — check the freeze first; else document the truncation and delete the unused `SocketHandle` alias. | 🟡 Partial |
| P31 | No unit specs for `netSocket` / `netServer` / `netClient` (loopback integration only). No hostile-input spec: forged handshake, `BanIp`, per-IP cap, timeout eviction. | 🟡 Partial |
| P65 | `template/` is compiled by no CI target and excluded from the pre-commit format path regex. Windows release job only *checks the file exists* in the zip. | 🔴 Open |
| P67 | `libstormenginev2.so` has no `-Wl,-soname`, so the AssetStore ABI break has no package guard behind it. (The tinyxml2 story in `base.mk` is about *its* soname in `NEEDED`, not this.) | 🔴 Open |
| Traps 10/11 | Field-site docs: no comment on `SpriteComponent::width/height` (source rect) or `AnimationComponent::vertical` (wrong flag draws nothing / wrong frames). Runtime diagnostic exists (`35877e1`). | 🟡 Partial |

Already scheduled elsewhere — do not re-file: P6/P7 → 2.7.2, P39 (`.map`
version) → 2.5.1, `Makefile.win` `-pthread` → "What we will not do" / layout
wave carry list.

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
- ~~**`IsAlive` is an O(|freeIds|) deque scan** called before the cheaper
  `IsPendingAdmission` short-circuit in `SystemMissedByLateComponent`.~~
  **Fixed** by the generation counter (2.0.0): `IsAlive` is a bounds check plus
  a generation compare. The comment in `SystemMissedByLateComponent` that still
  claimed the scan was corrected 2026-09-22. Residual: `IsIdInUse` still walks
  `freeIds` linearly; it is not on the per-frame diagnostic path.
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

### Carried from the 2.3.0 wave

- **The grid allocates per frame.** `CollectCandidates` builds a fresh
  `unordered_map` of cells, each holding a `vector` of body indices, every time
  `Update()` runs. The sweep it replaced allocated two vectors. Holding the map
  as a member and clearing it would keep the bucket array but not the per-cell
  vectors, which is half a fix; the whole one is a flat counting-sort layout —
  count bodies per cell, prefix-sum, fill one array — which allocates once and
  is reused. Not done here because the change already carries the output
  guarantees, and this is a profile question nothing in-repo is near.
- **`kMaxCellsPerBody` and `kBruteForceBelow` are reasoned, not measured.** 4096
  cells and 6 bodies. Both are the right shape — a body thousands of times
  larger than the grid should leave it, and all-pairs beats a hash table for a
  handful — but the exact numbers came from argument. A game that sits near
  either boundary would be the thing to measure against.

### Carried from the circle-collider wave

- ~~**A non-finite transform makes `ContactSystem`'s sweep undefined.**~~
  **Fixed.** The body is dropped before it reaches the comparator, per-frame
  rather than latched, and reported once through the usual throttled ECS
  diagnostic. Of the three options this list left open — silent drop, drop with
  a diagnostic, clamp — the middle one won on the grounds that the sweep decides
  gameplay, so a body vanishing from it is worth a line; `RenderColliderSystem`
  keeps its silent drop because it redraws every frame and would repeat that
  line 60 times a second. `storm::IsFinite` is exposed for a game running its
  own broadphase, which has the identical problem the moment it sorts.
- ~~**The broadphase still sweeps one axis.**~~ **Fixed** — it is a uniform
  grid now. **But this entry's other claim was wrong, and the correction is the
  part worth keeping:** it said a grid "would also remove the per-frame scan the
  widened membership requirement introduced — the two are the same rework". They
  are not. The scan exists because nothing notifies a system when a component is
  added, so the narrowing has to look every frame no matter what indexes the
  bodies afterwards. Removing it needs a change to the ECS — membership that
  re-evaluates, or a component-change hook — not a better broadphase. Bundling
  the two made the scan look like it had an owner when it did not.
- **`ContactSystem::BoundsOf(const Entity &)` and `CircleOf(const Entity &)` are
  public API the engine itself no longer calls.** `Update()` resolves components
  once and uses the `(transform, collider)` forms. Both `Entity` overloads read
  through `GetComponent`, which hands back a shared zeroed fallback on a miss
  rather than reporting one, so a caller passing an entity without the collider
  gets a silent degenerate shape. They are specced and documented as
  precondition-carrying, not deprecated.
