# Generation eval — can a model build a game from this skill?

Everything else in `references/` improves the *input* to generation. This
measures the *output*. Without it, "the skill is good enough" is a guess.

## Method

For each task in `tasks/`:

1. Give a model the task file and the skill. **No repo access** — that is the
   condition being tested. A model that can grep `common/ecs.h` is not being
   scored on the skill.
2. It writes a game into a fresh directory, starting from
   `references/new-game-scaffold/`.
3. Run `./verify.sh` from that directory.
4. Record the result and, on failure, the first error.

`run-eval.sh` automates steps 3 and 4 over a directory of candidates.

## Scoring

Per task, the highest level reached:

| Level | Meaning |
|-------|---------|
| 0 | did not build |
| 1 | built, but failed to link |
| 2 | ran, but exited early or logged an engine error |
| 3 | ran clean for the full window with textures and entities loaded |
| 4 | level 3 **and** the task's gameplay assertions hold |

Levels 0–3 are what `verify.sh` measures automatically. Level 4 needs a human
or a scripted check per task; each task file lists its assertions.

## Reading the results

The number itself matters less than the *first error*. Every distinct failure
is a gap in the skill, and the loop is:

```
run eval -> read failures -> add a SKILL.md rule or a compile-errors.md row -> re-run
```

A failure that recurs across tasks is worth a genre code block. A failure that
appears once is worth an error-table row.

## Known bias

If the generating model wrote or reviewed the skill, or has repo access, its
score is inflated — it is remembering rather than reading. Note the conditions
next to any recorded score. A first data point produced under privileged
conditions is a smoke test, not an eval.

## Baseline

Record results in `results.md` as you go: date, model, conditions, engine
version, score per task, first error. Trends across skill revisions are the
point; a single run is noise.

Note the **engine version** explicitly from v1.3.0 onwards. It changes what a
correct answer looks like (`ContactSystem`, `Text`, `Gamepad` and
`CapFrameRate` only exist there), so a score against a 1.2.x install is not
comparable to one against 1.3.0. `pkg-config --modversion stormengine2`, or
`ls /usr/local/include/stormengine2/systems/contact.h`, tells you which you are
on.

---

## Running it locally through opencode

Set up one directory per task — scaffold plus the task file, outside the engine
repo:

```bash
SK=~/.claude/skills/storm-engine-ai-skills
for t in 01-pong 02-collector 03-platformer; do
  mkdir -p ~/eval-run/$t
  cp -r $SK/references/new-game-scaffold/. ~/eval-run/$t/
  cp $SK/references/eval/tasks/$t.md ~/eval-run/$t/TASK.md
done
```

The skill is already visible: `~/.config/opencode/skills/storm-engine-ai-skills`
is a symlink to `~/.claude/skills/storm-engine-ai-skills`, so anything synced
there is what the model sees.

### Task files assume the scaffold is already unpacked

`run-eval.sh` and the setup loop copy the scaffold's *contents* into each task
directory, so the model's working directory **is** the game. The task files say
so explicitly. Do not reword them to reference `references/new-game-scaffold/`
— that path does not exist from the model's cwd, and a model that tries to
follow it stalls before writing a line. (This bit a real run.)

### Two modes — say which one you ran

**Mode B (realistic, the default).** The model has the skill *and* whatever a
normal developer machine has, which includes the installed headers at
`/usr/local/include/stormengine2/`. This measures "can a model build a game on
this engine in a real environment". It is the more useful number for deciding
whether generation is viable.

**Mode A (skill-only).** The model has the skill and no engine source at all.
This measures whether the skill is *self-sufficient* — whether the writing is
good enough to work without the headers to fall back on.

Mode A is hard to achieve and easy to get wrong. Denying
`~/Projects/storm-engine-v2` is **not enough**: `make install` copies `common/*`
to `/usr/local/include/stormengine2/` and then deletes only `*.o`, `*.d` and
`*.cpp` from the copy, so every engine **header** is still on disk under a
second path, byte for byte. Templates and inline bodies live in the headers, so
that is most of the ECS. A model that cats them has bypassed the skill entirely
while your audit reports clean.

Real Mode A needs a container with neither the repo nor `/usr/local/include/
stormengine2` mounted. Anything short of that, label the run Mode B and move on
— an honest B beats a fake A.

### Cut off repo access

This is the whole point of the eval, and the default config defeats it — the
`filesystem` MCP server grants read access to the engine repo, so the model
would read `common/ecs.h` instead of the skill. Generate a config with the MCP
servers disabled:

```bash
python3 -c "import json;d=json.load(open('$HOME/.config/opencode/opencode.json'));d['mcp']={k:{**v,'enabled':False} for k,v in d.get('mcp',{}).items()};json.dump(d,open('$HOME/eval-run/opencode.eval.json','w'),indent=4)"
```

Then run a task:

```bash
cd ~/eval-run/01-pong && OPENCODE_CONFIG=~/eval-run/opencode.eval.json opencode run -m ollama/qwen3:4b "$(cat TASK.md)"
```

### Isolation is not airtight — audit afterwards

Disabling MCP removes the *granted* path list, but opencode's built-in `read`,
`grep` and `bash` tools still accept absolute paths, so a determined model can
reach the repo anyway. For a rigorous run, use a container that simply does not
have the repo mounted. For a practical run, check afterwards:

Match the repo **path**, and scope it to the run — the bare string
`storm-engine-v2` also appears in the skill's own name and in every skill
directory path, so grepping for it reports a false positive on every run:

```bash
# WRONG: matches the skill name and skill paths too
grep -c "storm-engine-v2" ~/.local/share/opencode/log/opencode.log

# RIGHT: the repo path only, within one run id
L=~/.local/share/opencode/log/opencode.log
RUN=$(grep -oE 'run=[a-f0-9]+' $L | tail -1 | cut -d= -f2)
awk "/run=$RUN/" $L | grep -c "/home/wsams/Projects/storm-engine-v2"
```

`0` means isolation held. Anything higher and the run is not a valid data
point — note it and discard.

### Score

```bash
~/.claude/skills/storm-engine-ai-skills/references/eval/run-eval.sh ~/eval-run 4
```

Each task directory already contains `verify.sh`, so the harness scores them
without further setup.
