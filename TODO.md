# TODO

Cross-session roadmap slice tracker. One row per slice of
`docs/ROADMAP.md`. Check off when the PR merges. Conflict rule: if this
file conflicts after another agent's push, resolve by union (keep both
slices checked) and continue.

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
| 11 | 2.5.1 Version the `.map` — version header + editor writer landed; the 22-field record parser is still hand-rolled twice (P39) | `feat/map-format-version` | partial |
| 12 | 2.5.2 Asset path seam — `assetPath.h` + `docs/assets.md`; the `./` defect made public | `feat/asset-path-seam` | done |
| 13 | 2.5.3 Blob lifetime rules (docs) — missing `ReadBlob` entry + measured sync/lazy table | `docs/blob-lifetime` | done |
| 14 | 2.6.1 Engine mixer | pending | pending |
| 15 | 2.6.2 `input/inputHub.h` | pending | pending |
| 16 | 2.6.3 Example adopts input hub | pending | pending |
| 17 | 2.7.1 Host address enumeration | `feat/host-addresses` | done |
| 18 | 2.7.2 Switch halves — `socketInitializeDefault`, three-stage `Open` diagnostics, and the two latent Switch compile breaks it unmasked | `fix/switch-socket-init` | done |

**Skipped (unscheduled):** 3.0.0 ECS wave — trigger-based, not in the queue.

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
