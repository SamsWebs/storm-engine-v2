# storm-engine-v2-web

The marketing and documentation landing page for
[Storm! Engine v2](https://github.com/SamsWebs/storm-engine-v2), built as a
single static page for GitHub Pages.

No build step, no dependencies, no framework. `index.html` carries its own
styles; everything else is an image.

```
index.html                 the whole page
assets/favicon.svg         tab icon — the band and a bolt
assets/og.png              1200x630 link-preview card
assets/img/*.png           screenshots, copied from the engine's examples/
.nojekyll                  stop Pages running Jekyll over the files
```

## Preview it

Any static server works, and a plain `file://` open works too:

```bash
python3 -m http.server 4173
# then open http://localhost:4173/
```

## Design notes

The palette is sampled directly from the Sams! Webs wordmark rather than
chosen alongside it — the band red is `#CB2026`, the letterform fill `#F1F2F2`
and the outline `#231F20`. The page neutral is warmed toward that outline
instead of being left a default cool grey.

**The wordmark is live text, not an image.** It is inline SVG using
`paint-order: stroke fill`, which puts the heavy stroke *outside* the glyph
instead of eating into it — the usual `-webkit-text-stroke` approach centres
the stroke and thins the letterform. It scales to any size, recolours with the
theme, and stays selectable. The face is Luckiest Guy; the fallback stack is
declared, so a failed font load degrades rather than disappearing.

The red band is the structural spine: behind the wordmark, again as a
full-bleed rule at every section boundary, and again on the left edge of every
code block. It is the same device doing the same job at three scales.

Screenshots use `image-rendering: pixelated`. These are pixel-art games and
smooth-scaling them misrepresents what the engine draws. Gallery thumbnails use
`object-fit: contain`, not `cover`: the frames run from 0.79 (a Tetris well) to
2.22 (an Android handset), and cropping them to a common ratio cut the
playfield out of one and most of the level out of the other.

`assets/img/puzzle.png` is cropped from the original 1920×1080 window
screenshot, which was 99% black background around a small playfield.

## Facts to keep in sync with the engine

Two numbers on this page are easy to get wrong, and one of them is wrong in the
engine's own README:

- **64 component types per binary**, not 32. `MAX_COMPONENTS` was raised from
  32 to 64 in 2.0.0 — it is one of the headline breaking changes, and the whole
  reason the upgrade is a rebuild rather than a relink. The engine README's
  feature list still says 32 while its own *Component type limit* section says
  64; `common/ecs.h` and `specs/layout.spec.cpp` are the authority.
- **Windows is supported**, via a MinGW-w64 cross-build from Linux. The library
  and spec suite cross-build; the examples are not wired into that build yet
  and a native cmd/PowerShell toolchain is not supported. Say all three, or the
  claim over-promises.

## Contrast

Footer colours are fixed literals rather than tokens, because the footer stays
the same near-black in every theme. Contrast was previously carried by
`opacity`, which is what made the link columns hard to read. Measured against
the footer ground `#1B1819`: links 17.6:1, body 14.0:1, column headings 7.1:1,
colophon 8.4:1 — all clear of AA.

## Themes

Three states are handled, not two: an explicit `data-theme` on the root, and
the default unstamped document where only `prefers-color-scheme` distinguishes
light from dark. Every colour is a token defined on bare `:root` and redefined
in both dark blocks.

The wordmark is deliberately *not* re-themed. A brand mark that recolours is a
different mark.

## Deploying to GitHub Pages

Settings → Pages → Build and deployment → **Deploy from a branch**, branch
`main`, folder `/ (root)`. `.nojekyll` is already present, so Pages serves the
files as they are.

Every asset path is relative, so the page works unchanged whether it is served
from a user site, a project site under `/storm-engine-v2-web/`, or dropped into
the engine repository's own `docs/` folder.
