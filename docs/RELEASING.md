# Releasing

The order to do it in, and — more to the point — **what a green run does not
prove.** That second half is the reason this file exists.

## Why this file exists

`docs/ROADMAP.md` → Track 0.5 asks for the verification matrix's remaining half,
so that "all targets build" can never again be written from intent. The
concrete failure behind it: a `getifaddrs` call reached the flagship game's
shared header and passed 5.5k specs on Linux while **Switch, Android and
Windows could not build at all.** The suite was green. Nothing in the repo
recorded which platforms had actually been built, so "it builds" was a claim
with no referent.

A second, quieter version of the same defect lives in the version number. It is
hand-written in **four** places, and the release workflow validates **one**:

| Site | Consumed by |
|---|---|
| `Makefile.debian` `VERSION ?=` | the workflow's pre-release gate, `pkg-config`, the `.deb` |
| `Makefile.win` `VERSION ?=` | `DISTNAME`, the `.zip` filename, `README.txt` in the zip |
| `web/index.html` — 7 `data-ver` elements | what every visitor to the project page is told |
| the pushed tag | which release GitHub builds |

So a tag can go out green with the website advertising the previous release.
`scripts/release-check.py` closes that, and the version sites are marked with
`data-ver` attributes precisely so the check can find them without regex-sweeping
for anything shaped like a version — the page also carries correct *historical*
versions (the 2.0.0 migration notes), and asserting on those would fail on the
truth.

## The matrix

| Target | Verified by | Covered |
|---|---|---|
| Engine library, specs, amd64 + arm64 `.deb` | `build-and-release.yml`, on a pushed tag | ✅ |
| `.deb` installs and loads on bookworm / 22.04 / 24.04 | `build-and-release.yml` (amd64 leg) | ✅ |
| Seven examples for Windows, plus `windows-platformer` against the packaged SDK zip | `build-and-release.yml` | ✅ |
| Nine desktop examples linked, `editor/` compiled to objects | `pr-validate.yml`, in the Docker image | ✅ |
| **`examples/nx-platformer`** (devkitPro, `-fno-exceptions`) | **nothing** | ❌ **ships green** |
| **`examples/android-platformer`** (NDK + six submodules) | **nothing** | ❌ **ships green** |

Two facts the table cannot show, and the files that record them:

- The editor is **compiled, never linked**. `editor/src/utilities/FileDialogWin.cpp`
  genuinely calls `NFD_*` and libnfd has no Debian package; `vendor/nfd` ships
  the header and a licence, no implementation.
- The two uncovered trees are uncovered because neither toolchain is in the
  image **and** `.dockerignore` keeps both out of the build context — so no
  script inside the image can build them even if one asked to.

`release-check.py` re-checks both claims rather than trusting this table. If a
workflow ever gains a devkitPro or NDK invocation, or the two job ids
`build-windows` / `build-linux` disappear, the check fails and **this table has
to be corrected in the same change.** An out-of-date matrix that understates
coverage is the defect, not a doc nit.

## Before you tag

Work on a branch, never on `main`. One commit per slice; the release itself is
a tag, not a merge.

### 1. Green the fast gates

```bash
python3 scripts/release-check.py     # version sites, source list, compat probe, coverage claims
```

Five checks, all of which have caught a real injected defect during
development: a phantom entry in the source list, a removed one, CRLF endings,
a `web/index.html` left on the old version, a `Makefile.win` version drift, a
Switch build landing in CI, and a deleted release job. It is stdlib-only and
takes under a second.

**It does not build anything and does not run the suite.** That is deliberate —
those are the two slow gates and they are steps 2 and 3.

### 2. Green the suite

```bash
make -f Makefile.debian test          # builds ./bin/tests, then runs it
```

There is no plain `Makefile` at the repo root; every invocation needs
`-f Makefile.debian`. 604 specs, and it **must be run from the repo root** —
several specs hardcode `./specs/assets/...` and the run segfaults elsewhere.

### 3. Build what CI will build, locally

```bash
make -f Makefile.debian target        # just the .so
sudo make -f Makefile.debian install  # examples link the INSTALLED library
```

`install` has **no prerequisites** and will happily install a stale `.so`, so
`target` first or you are testing the last build. It honours `DESTDIR`, so
`make install DESTDIR=/tmp/stage` previews the tree the `.deb` will carry
without touching the system.

### 4. Bump the version in all four sites

```bash
# Makefile.debian, Makefile.win, and the data-ver elements in web/index.html
```

Then re-run step 1. **Do not skip it** — it exists because this is the one
release chore that has silently shipped wrong.

A release number is a *tree* fact, not a build input. The workflow overrides
`VERSION` from the tag for the packaged artifact, which is correct — but that
also means a stale tree version produces a correct `.deb` and a wrong website,
and nothing in the build can see the difference.

### 5. Commit, sign, push

```bash
git add -A && git commit -S      # no --no-gpg-sign, ever
git push origin main
```

The remote enforces verified signatures. If GPG flakes, stop and ask; do not
work around it.

### 6. Tag deliberately

```bash
git tag -s v2.4.0 && git push origin v2.4.0
```

A tag is what triggers `build-and-release.yml`. It used to *also* fire on every
merge to `main`, bumping the patch from tag history and pushing a tag — which
published a set of BREAKING changes as a patch. It happened at v1.3.2 and had
to be deleted before anyone fetched it. **That trigger does not come back.**
Merges are not releases.

### 7. Read the workflow's output, do not infer it

`workflow_dispatch` on a commit that already carries a `vX.Y.Z` tag **rebuilds**
that release. It will not mint a new one, and that refusal is the point.

The workflow fails closed on the things that have actually shipped: a
pre-release version, a `.deb` that will not install, a `Depends:` that names
`tinyxml2` (compiled in since 2.1.0, so its presence means the build regressed
to the system copy), an SDL-family symbol outside the known ABI, and a
`.deb` that installs but does not load.

## After the tag

- `CHANGELOG.md`: the release gets a dated heading, and `[Unreleased]` empties.
  A public API change must be in the same branch that makes it — `ReadBlob`
  shipped on a branch and reached `CHANGELOG.md` **after** `v2.3.1`, with zero
  entries, which is this repo's own rule broken in the commit that added the
  API.
- `docs/ROADMAP.md` and `TODO.md`: the slice marked done, statuses corrected.
- If a Switch or Android build was attempted by hand, say so in the PR body.
  A hand-verified platform is worth recording; a hand-*believed* one is how the
  `getifaddrs` defect shipped.

## What this checklist cannot do

It cannot verify the two uncovered platforms, and no amount of reading will.
The only thing that closes that gap is putting devkitPro and the NDK in CI.
Until then, **a change to a header every target compiles is unverified on two of
three platforms until someone builds them by hand** — and the honest form of
that sentence belongs in the PR body of anything touching `common/`.
