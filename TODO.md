# TODO

Cross-session roadmap slice tracker. One row per slice of
`docs/ROADMAP.md`. Check off when the PR merges. Conflict rule: if this
file conflicts after another agent's push, resolve by union (keep both
slices checked) and continue.

**Released 2.7.0 (2026-10-04).** Slices 1–13 and 17–18 all shipped in that one
tag, so no 2.6 was ever cut and the numbering no longer matches the original
plan — see the note at the top of `docs/ROADMAP.md`.

**The remaining rows were re-versioned 2026-10-04, after that release.** The
old 2.6 line is now **2.8.0**, and the open half of row 11 (the `.map` record
parser, P39) is now its own slice **2.8.4** rather than a partial 2.5.1. So the
queue is 2.8.1 → 2.8.4, and 3.0.0 stays reserved and unscheduled. Row 11 below
is kept as the record of what 2.5.1 actually shipped; the open half is
tracked in row 19.

**Three unplanned repairs, not roadmap slices, all landed in the same window**
and recorded in `CHANGELOG.md` → 2.7.0 → Fixed: the PR gate's source-list
check was matching nothing (`$$` expanded to a PID in a `grep` pattern) which
had been failing every PR and hiding the next two; `VersionHeaderSpec` could
not read `Makefile.debian` in the CI image because `Dockerfile.debian` renames
it; and `ci-build-examples.sh` carried a second copy of `LIB` with
`-ltinyxml2`/`-llua` that `base.mk` had deliberately dropped. Worth knowing
because together they mean **the spec suite had not run in CI at all** since
the canonical-source-list slice — the pipeline was red from the top down.

## Slice order

| # | Slice | Branch | Status |
|---|-------|--------|--------|
| 1 | Track 0.1–0.3 docs hygiene (retire guardrails, move TECH_DEBT durable half, fix stale statuses) | `docs/track0-hygiene` | done (PR #79) |
| 2 | P17 residual: `loadFilemapEditor` third silent-failure mode (failing spec first) | `fix/editor-map-truncated` | done |
| 3 | Track 0.4 One canonical source list | `fix/engine-canonical-sources` | done (merged) |
| 4 | Track 0.5 Release checklist (verification matrix half) | `chore/release-checklist` | done |
| 5 | Track 0.6 Example layout/measurement harness | `chore/example-layout-harness` | done |
| 6 | 2.4.1 `GameState::Present()` | `feat/gamestate-present` | done |
| 7 | 2.4.2 `ui/scale.h` | `feat/ui-scale` | done |
| 8 | 2.4.3 `text.h` verbs | `feat/text-verbs` | done |
| 9 | 2.4.4 `version.h` | `feat/version-header` | done |
| 10 | 2.4.5 debug overlay | `feat/debug-overlay` | done |
| 11 | 2.5.1 Version the `.map` — version header + editor writer landed in 2.7.0; the 22-field record half moved to row 19 | `feat/map-format-version` | partial → split |
| 12 | 2.5.2 Asset path seam — `assetPath.h` + `docs/assets.md`; the `./` defect made public | `feat/asset-path-seam` | done |
| 13 | 2.5.3 Blob lifetime rules (docs) — missing `ReadBlob` entry + measured sync/lazy table | `docs/blob-lifetime` | done |
| 14 | 2.8.1 Engine mixer (was 2.6.1) — `audio/mixer.h` (pure policy) + `audio/soundMixer.h`, and `netplay-checkers` converted off `Mix_PlayChannel(-1, ...)` | `feat/engine-mixer` | done |
| 15 | 2.8.2 `input/inputHub.h` (was 2.6.2) — blocks row 16 | `feat/input-hub` | done |
| 16 | 2.8.3 Example adopts input hub (was 2.6.3) — `examples/shooter`, 3 poll loops + 6 key flags removed | `feat/input-hub` | done |
| 17 | 2.7.1 Host address enumeration | `feat/host-addresses` | done |
| 18 | 2.7.2 Switch halves — `socketInitializeDefault`, three-stage `Open` diagnostics, and the two latent Switch compile breaks it unmasked | `fix/switch-socket-init` | done |
| 19 | 2.8.4 The `.map` record parser (P39) — 22 fields parsed twice by hand, `SaveMap` is the only writer with no spec calling it. **Blocked on deciding how to make it observable**: the editor cannot be linked here (no libnfd), so a behaviour change in `LoadMap` has no test watching it. | pending | pending |

**Skipped (unscheduled):** 3.0.0 ECS wave — trigger-based, not in the queue.
2.6 is retired as a number: 2.7.0 shipped 2.4, 2.5 and 2.7 at once, so rows
14–16 moved to 2.8.x rather than leaving an empty line.

## Carried TECH_DEBT (not scheduled releases; pick up opportunistically)

Tracked in `docs/ROADMAP.md` → "Carried from the TECH_DEBT notebook":
P19 residual, P28, P44 residual, P65, P67, traps 10/11.
(P31 partially addressed 2026-10-04 in slice 18: the first `netSocket` spec file,
over the pure half only. The `netServer`/`netClient` and hostile-input cases — the
item's real content — are still open.)
(P17 residual closed 2026-09-22, slice 2.)

## Rules

- Docs-only slice: suite must stay green, no new specs required.
- Code slice: failing BDD spec first; suite green after fix.
- Public API change: `TUTORIAL.md` + `CHANGELOG.md` in the same branch.
- New public name: `python3 scripts/generate-compat-probes.py --check`.
- Adversarial review + regression check before PR. **The regression check is
  mechanical now:** `python3 tools/sabotage-<area>.py` proves the new specs can
  fail, and a `*** 0 ***` line means the spec is not doing its job. CLAUDE.md
  explains the tool and its two traps.
