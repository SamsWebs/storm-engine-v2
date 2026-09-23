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
| 3 | Track 0.4 One canonical source list | pending | pending |
| 4 | Track 0.5 Release checklist (verification matrix half) | pending | pending |
| 5 | Track 0.6 Example layout/measurement harness | pending | pending |
| 6 | 2.4.1 `GameState::Present()` | pending | pending |
| 7 | 2.4.2 `ui/scale.h` | pending | pending |
| 8 | 2.4.3 `text.h` verbs | pending | pending |
| 9 | 2.4.4 `version.h` | pending | pending |
| 10 | 2.4.5 debug overlay | pending | pending |
| 11 | 2.5.1 Version the `.map` (+ editor writer) | pending | pending |
| 12 | 2.5.2 Asset path seam | pending | pending |
| 13 | 2.5.3 Blob lifetime rules (docs) | pending | pending |
| 14 | 2.6.1 Engine mixer | pending | pending |
| 15 | 2.6.2 `input/inputHub.h` | pending | pending |
| 16 | 2.6.3 Example adopts input hub | pending | pending |
| 17 | 2.7.1 Host address enumeration | pending | pending |
| 18 | 2.7.2 Switch halves (`common/net/` on nx-platformer, `socketInitializeDefault`) | pending | pending |

**Skipped (unscheduled):** 3.0.0 ECS wave — trigger-based, not in the queue.

## Carried TECH_DEBT (not scheduled releases; pick up opportunistically)

Tracked in `docs/ROADMAP.md` → "Carried from the TECH_DEBT notebook":
P19 residual, P28, P44 residual, P31, P65, P67, traps 10/11.
(P17 residual closed 2026-09-22, slice 2.)

## Rules

- Docs-only slice: suite must stay green (604), no new specs required.
- Code slice: failing BDD spec first; suite green after fix.
- Public API change: `TUTORIAL.md` + `CHANGELOG.md` in the same branch.
- New public name: `python3 scripts/generate-compat-probes.py --check`.
- Adversarial review + regression check before PR.
