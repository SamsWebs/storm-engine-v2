# Eval results

Newest first. Record conditions, not just scores — a number without its
conditions is not comparable to anything.

---

## 2026-08-11 — DeepSeek v4 Flash (free, via opencode zen), **MODE B**

Config: `opencode.modeB.json` — MCP disabled, no plugin, no `CLAUDE.md`,
`external_directory = allow`. First run under fully clean, intentional
conditions, with the opaque cell 0 and the documented `srcRect` rule in place.

| Task | Level | Notes |
|------|-------|-------|
| 01-pong | **3** | working Pong: both paddles + ball render, verified by screenshot |
| 02-collector | **3** | 8 coins + player render (counted from the screenshot); deferred-`Kill()` reasoning correct |
| 03-platformer | **3** | player rests on a platform; per-axis resolution, walls and ground all correct |

All three verified independently: build, headless run, windowed screenshot,
pixel distribution, and a read of the source — not the model's own summary.

`PASS: built, linked, ran 4s, 1 texture(s), 3 entity/entities, no errors; drew content (3136 non-background px)`

Pixel distribution across the window: left 1440, middle 256 (ball), right 1440.

### The documented rule was used

GLM 5.2 had passed the paddle's on-screen size as `SpriteComponent`'s
width/height, which made `srcRect` reach off the sheet, and took the default
`srcRectX = 0` which was then a transparent cell — an invisible paddle. Both
facts were added to the skill afterwards.

DeepSeek, reading the updated skill, wrote:

```cpp
left.AddComponent<SpriteComponent>("tiles", 32, 32, 1);   // the CELL size
// resized with TransformComponent.scale (scale.y = 2.8125)
```

Cell dimensions for the source rect, transform scale for the on-screen size —
exactly the documented fix, first try, no debugging round.

**This is the failure-to-rule loop closing for the first time**: a defect found
by one run became a rule that a later run consumed correctly.

### Other rules held

`CollisionSystem` not registered; `registry_.Update()` first in `update()`;
systems registered before entities. It also wrote its own simulation to check
the gameplay assertions — 5 x-velocity reversals on paddle overlap, paddles
clamped to `[0,510]` — which is level-4 evidence, self-reported rather than
independently scripted.


### Tasks 02 and 03 — the documented traps were all taken

**02, deferred `Kill()`.** Copied the group vector, then reasoned explicitly
about why no coin double-counts:

> `Kill()` is deferred — a killed coin stays alive and in the group until the
> next `registry_.Update()` ... That flush is exactly why no coin is ever
> counted twice.

Also guarded the completion message with a `completionLogged_` flag. Screenshot
shows exactly 8 coin sprites and 1 player, matching the 9 entities the verifier
counted.

**03, per-axis resolution.** The hardest task, and it took all three traps:

- resolved X before Y, commented `so a corner cannot shove the player`
- did **not** register `MovementSystem` for a hand-integrated player, avoiding
  double integration
- gravity plus an `onGround_` flag, with the jump press recorded in
  `processInput` and acted on in `update`

The screenshot is the strongest evidence in the run: the player is **resting on
a platform**, not sinking through it, with walls and ground rendering. That is
per-axis resolution working at runtime, not merely present in the source.

Every one of these is content added to the skill earlier in the session — the
ECS deferred-lifecycle trap and the platformer genre code block. This is the
second consecutive run where documented rules were consumed correctly on the
first attempt.

### Still unverified

Level 4 remains partly self-reported. Runtime *behaviour over time* — a ball
actually reversing on contact, a jump returning to the same resting height — is
not scripted. The screenshots prove a correct static frame; they do not prove
correct dynamics.

### Cost

Free tier. The paid NVIDIA endpoint had rate-limited (18 x 429) after the Mode A
sweep, producing no data for an hour.

---

## 2026-08-11 — GLM 5.2, **MODE A** (skill only, engine source denied)

Config: `opencode.modeA.json` — MCP disabled, no plugin, no `CLAUDE.md`, and
`permission.external_directory = deny` so nothing outside the task directory is
readable. The model confirmed it had the skill (`Got skill.`).

| Task | Level | Notes |
|------|-------|-------|
| 01-pong | **0** | nothing written |
| 02-collector | **0** | nothing written |
| 03-platformer | **0** | nothing written |

### The finding

**The skill alone is not sufficient for this model.** On all three tasks it
read `TASK.md`, consulted the skill, then went looking for exact API
signatures — `Kill()` semantics, `GetEntitiesByGroup`, component field names —
was denied, and abandoned the task rather than working from the documentation.

That is the question Mode A exists to answer, and the answer is no. It does not
mean the skill is bad; it means a model that *can* read headers prefers to, and
does not gracefully degrade when it cannot. Whether a model instructed to work
from the skill alone would succeed is a separate, untested question.

### How this run happened by accident

It was launched intending Mode B. opencode auto-rejects `Read`/`Grep` outside
the working directory in non-interactive `run` mode, which silently produced
Mode A. The earlier GLM run reached the same headers via `bash cat`, which that
permission does not cover — so the two runs differed in isolation purely by
which tool the model happened to reach for.

Both modes are now explicit configs (`opencode.modeA.json`,
`opencode.modeB.json`) rather than an accident of tool choice.

### The harness is readable by the thing it grades

The model read `verify.sh` and reasoned about the anti-cheat check:

> the verify.sh default PRISTINE is `~/.claude/skills/...` — that path doesn't
> exist there. So verify skips "did-anything-change" check with warning. Good —
> I just need to actually modify the scaffold files

It chose to do real work regardless, but a grader sitting inside the graded
directory is a design flaw. Move `verify.sh` outside the task directory, or
accept that scores are cooperative rather than adversarial.

---

## 2026-08-11 — GLM 5.2 (via NVIDIA, `nvidia/z-ai/glm-5.2`), skill @ ~1550 lines

**Conditions: MODE B (realistic).** MCP disabled and no access to
`~/Projects/storm-engine-v2`, but the model read
`/usr/local/include/stormengine2/` — the installed headers, byte-identical to
`common/`. Also, this run had the caveman plugin and `CLAUDE.md` injected via
the inherited config; both have since been stripped from `opencode.eval.json`.

| Task | Level | Notes |
|------|-------|-------|
| 01-pong | **3** | working Pong: both paddles and ball render, ball moves |
| 02-collector | not run | |
| 03-platformer | not run | |

### Rule compliance (checked by hand)

`CollisionSystem` correctly **not** registered; `registry_.Update()` before
systems; `AddSystem` before entity creation; `SDL_PollEvent` in one place.
It named the kill-on-contact trap explicitly and hand-rolled AABB bounce
resolution. It also self-corrected mid-run — wrote a debug branch calling
`RenderColliderSystem`, noticed it had never registered that system, and went
back to add it.

### The one visual defect was the harness's fault, not the model's

The left paddle rendered nothing. Cause: it used `SpriteComponent`'s default
`srcRectX = 0`, and cell 0 of the shipped `tileset.png` was **fully
transparent**. No error at any layer. Regenerating the tileset with an opaque
cell 0 fixed it with no code change — 0 px became 1440 px.

Two things were wrong on our side, both now fixed: the placeholder art had a
transparent default cell, and the skill never documented that
`SpriteComponent`'s `width`/`height` *are* the source rect.

### Two false readings this run produced, both mine

1. **"Renders nothing."** The draw check matched a window by title substring and
   grabbed **VS Code** (whose window title contained `storm-engine-v2`), then
   later returned 0 px on an occluded window. It now matches by `_NET_WM_PID`,
   raises the window with `wmctrl`, and samples three frames.
2. **A 1% luminance threshold.** One 32x32 sprite is 0.2% of an 800x600 window,
   and the scaffold's own player measures 441 px / 0.09%. The threshold now
   counts absolute non-background pixels with a floor of 100.

### Harness bug this run exposed

The first attempt stalled on the task instruction "Start from
`references/new-game-scaffold/`" — a path that does not exist in the run
directory. The model said so and stopped, correctly. Wording fixed; the scored
result is from the corrected version.

---

## 2026-08-11 — qwen3:4b (local, ollama), skill @ 1548 lines

**Conditions: CLEAN.** Run through opencode with all MCP servers disabled
(`opencode.eval.json`), cwd `~/eval-run/01-pong`, skill loaded from
`~/.config/opencode/skills`. Post-run audit scoped to the run id found **0**
references to `/home/wsams/Projects/storm-engine-v2` — the model did not read
the engine source.

| Task | Level | Notes |
|------|-------|-------|
| 01-pong | **0** | wrote no files at all; `playState.cpp` byte-identical to the scaffold |
| 02-collector | not run | |
| 03-platformer | not run | |

### What it actually produced

Three sentences of plan, no tool calls:

> Implement `PlayState` with fixed ball velocity. Check paddle collisions in
> update system without killing entities. Reset ball to center if off-screen.

### Confound — noted, not defended

This run used a task file that told the model to "start from
`references/new-game-scaffold/`", a path that does not exist in the run
directory (the scaffold's contents are copied to the cwd root). The wording was
fixed afterwards.

It probably did not cause the zero: the model produced a coherent plan naming
the correct engine traps and never attempted any file operation, which reads as
a failure to drive the agent loop rather than a failure to locate files. But it
is a confound, so re-run this task against the corrected wording before citing
the number as a local-tier baseline.

### Reading it

This is **not** a knowledge failure. "without killing entities" is the
`CollisionSystem` kill-on-contact trap straight out of the skill, and resetting
the ball off-screen is a task requirement — it read both and understood them.
It failed to drive the agent loop: it described the work instead of calling
write tools.

That distinction matters for what to fix. No amount of additional skill content
addresses it; the model cannot execute a multi-file edit. The local tier is
capability-bound, not documentation-bound.

---

## 2026-08-11 — Claude Opus 5, skill @ 1548 lines (post genre-code pass)

**Conditions: PRIVILEGED — smoke test, not a clean eval.** The generating model
had full repo access and had itself authored the skill revision under test. It
was reading its own recent work, not reading the skill cold. Treat the score as
an upper bound and a check that the harness works, not as evidence about the
skill.

| Task | Level | Notes |
|------|-------|-------|
| 01-pong | 3 | built, linked, ran 4s clean, 1 texture, 3 entities |
| 02-collector | not run | |
| 03-platformer | not run | |

Level 4 assertions were not machine-checked for any task.

### Harness validation

Run against deliberately broken candidates to confirm the scoring
discriminates rather than always passing:

| Candidate | Level | First error |
|-----------|-------|-------------|
| good scaffold | 3 | — |
| missing `player.png` | 2 | engine logged an error |
| syntax error in `main.cpp` | 0 | build failed |

### Observations

Writing Pong surfaced no new skill gaps, but two rules earned their place —
both were consulted while writing it:

- Not registering `CollisionSystem`, which kills on contact and would have
  deleted the ball on the first paddle hit.
- The `SpriteComponent` argument order: skipping `isFixed` binds `srcRectX` to
  the bool. The task needs `srcRectX` to pick a tileset cell, so this would
  have bitten immediately.

### What a real run needs

A model with the skill and **no repo access**, ideally not the one that wrote
the skill. Until then the honest answer to "is the skill good enough" is
unmeasured.
