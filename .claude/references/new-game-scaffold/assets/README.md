# Placeholder art

Generated for this scaffold, not sourced from a third party — no attribution or
license constraints, and safe to redistribute with the engine. Replace both
files with your own art whenever you like; the code reads them by path and does
not care what they look like.

## gfx/player.png — 128x32

A **horizontal** strip of 4 frames, 32x32 each.

The orientation is load-bearing. `AnimationSystem` does:

```cpp
if (animation.vertical) sprite.srcRect.y = sheetFrame * sprite.height;
else                    sprite.srcRect.x = sheetFrame * sprite.width;
```

so `vertical == false` (what the scaffold passes) advances along **x** and
needs frames laid left-to-right. Getting this backwards raises no error — the
sprite just samples outside the texture and draws nothing, or freezes on frame
zero. If you swap in a vertical sheet, pass `vertical = true`.

Note the engine's own `examples/platformer` uses a vertical sheet
(`rabbit.png`, 37x1026) and therefore passes `true`.

## gfx/tileset.png — 128x32

Four 32x32 tiles in a row, addressed by `SpriteComponent.srcRect.x`:

| Index | srcRect.x | Tile |
|-------|-----------|------|
| 0 | 0 | plain block — **opaque on purpose** |
| 1 | 32 | ground (grass top) |
| 2 | 64 | dirt |
| 3 | 96 | floating platform (partly transparent) |

**Cell 0 is deliberately not empty.** `SpriteComponent`'s `srcRectX` defaults to
`0`, so anything that does not think about cell selection lands there. When that
cell was transparent, such a sprite vanished with no error at any layer — it
cost a real debugging session. There is no "empty" cell: to draw nothing, give
the entity no `SpriteComponent`.

**`width`/`height` are the source rect, not just the draw size.** The
constructor builds `srcRect{srcRectX, srcRectY, width, height}`, and
`RenderSystem` computes the destination as `sprite.width * transform.scale.x`.
So to draw a 16x90 paddle from a 32x32 cell, pass `width=32, height=32` and set
`TransformComponent.scale` to `(0.5, 2.8)`. Passing `width=16, height=90`
instead makes `srcRect` reach past the bottom of a 32px-tall sheet; SDL clamps
it, so the sprite renders stretched rather than failing loudly.

`srcRect` is only written automatically by `AnimationSystem`. Tiles are not
animated, so set `srcRect.x` yourself when you spawn them.
