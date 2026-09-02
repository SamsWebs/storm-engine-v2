#pragma once

#include <cmath>

#include "../collision/shapes.h"
#include "../components/boxCollider.h"
#include "../components/circleCollider.h"
#include "../components/transform.h"
#include "../ecs.h"
#include "contact.h"

#include <SDL2/SDL.h>

namespace storm {

// The debug overlay. It draws the shape `ContactSystem` actually sweeps, which
// is why it includes contact.h and resolves every collider through
// `ContactSystem::BoundsOf` / `ContactSystem::CircleOf` rather than repeating
// the position, offset and scale arithmetic. A debug view that disagrees with
// the system it is drawing is worse than no debug view: the overlay looks
// right while the game does not, and the hunt starts in the wrong place.
//
// Membership is TransformComponent ALONE, and narrows inside Update(), for the
// same reason ContactSystem does it -- a signature is an AND, so it cannot say
// "a box collider OR a circle collider". See the comment on ContactSystem's
// constructor. The two consequences carry over unchanged: GetSystemEntities()
// reports transform entities rather than bodies, and a collider added to a live
// entity is drawn from the next frame.
class RenderColliderSystem : public System {
public:
  RenderColliderSystem() { RequireComponent<TransformComponent>(); }

  // `camera` pans the outlines exactly as RenderSystem pans a non-fixed
  // sprite, and defaults to nullptr so every existing `Update(renderer)` call
  // keeps compiling and keeps drawing in raw world coordinates.
  //
  // There is no `isFixed` equivalent here, unlike RenderSystem: a collider is a
  // body in the world, never a HUD element, so every outline pans. A game that
  // draws a screen-space box for debug purposes is not doing it through this
  // system.
  void Update(SDL_Renderer *renderer, const SDL_Rect *camera = nullptr) {
    const float cameraX = camera ? static_cast<float>(camera->x) : 0.0f;
    const float cameraY = camera ? static_cast<float>(camera->y) : 0.0f;

    // Set once, not per entity. Membership is every transform entity now, and
    // most of a real game's are sprites carrying no collider at all; setting
    // the draw colour inside the loop paid for all of them and clobbered the
    // caller's colour even on a frame that drew nothing.
    SDL_SetRenderDrawColor(renderer, 0, 0xFF, 0, 0xFF);

    for (auto &entity : GetSystemEntities()) {
      const TransformComponent *transform =
          entity.TryGetComponent<TransformComponent>();
      // Required, so present for every member -- unless the game removed it
      // from a live entity, which does not revoke membership.
      if (!transform)
        continue;

      // A box wins when an entity carries both, matching ContactSystem exactly.
      // The overlay has to agree with the sweep about which shape is live, or
      // it draws a collider the game does not have.
      if (const BoxColliderComponent *box =
              entity.TryGetComponent<BoxColliderComponent>()) {
        const ContactAABB bounds = ContactSystem::BoundsOf(*transform, *box);
        DrawBoxOutline(renderer, bounds.minX - cameraX, bounds.minY - cameraY,
                       bounds.maxX - bounds.minX, bounds.maxY - bounds.minY);
        continue;
      }

      if (const CircleColliderComponent *circle =
              entity.TryGetComponent<CircleColliderComponent>()) {
        const ContactCircle world = ContactSystem::CircleOf(*transform, *circle);
        DrawCircleOutline(renderer, world.x - cameraX, world.y - cameraY,
                          world.radius);
      }
    }
  }

private:
  // Whole-pixel limits, past which nothing is drawn.
  //
  // These are not tidiness. `static_cast<int>` of a float too large for an int
  // -- or of a NaN, which a zero-scaled or corrupt transform produces -- is
  // undefined behaviour, and the circle rasteriser below costs one loop
  // iteration per pixel of radius, so an absurd radius is a per-frame hang
  // rather than a wrong picture. Both bounds are far past any window on any
  // display, so no drawable outline is lost: a shape beyond them is a
  // transform.scale or position bug, and the overlay is how you find it, not
  // something it can usefully draw.
  static constexpr float kMaxOutlineRadius = 16384.0f;
  static constexpr float kMaxOutlineCoordinate = 1.0e7f;

  static bool IsDrawableAt(float x, float y) {
    return std::isfinite(x) && std::isfinite(y) &&
           std::abs(x) <= kMaxOutlineCoordinate &&
           std::abs(y) <= kMaxOutlineCoordinate;
  }

  static void DrawBoxOutline(SDL_Renderer *renderer, float x, float y,
                             float width, float height) {
    if (!IsDrawableAt(x, y) || !IsDrawableAt(width, height))
      return;

    SDL_Rect bbox = {static_cast<int>(x), static_cast<int>(y),
                     static_cast<int>(width), static_cast<int>(height)};
    SDL_RenderDrawRect(renderer, &bbox);
  }

  // Midpoint circle, eight-way symmetric, integer only -- SDL2 has no circle
  // primitive and pulling SDL2_gfx in for a debug overlay is not worth a new
  // dependency in the .deb.
  //
  // The radius is TRUNCATED to whole pixels, the same way a box's scaled
  // extents are truncated into an SDL_Rect. A radius that truncates to zero --
  // including a negative one, which is nonsense the solvers clamp to a point --
  // draws the centre pixel, because a body that exists should leave a mark
  // rather than vanish from the overlay it was turned on to explain.
  static void DrawCircleOutline(SDL_Renderer *renderer, float centreX,
                                float centreY, float radius) {
    if (!IsDrawableAt(centreX, centreY) || !std::isfinite(radius) ||
        radius > kMaxOutlineRadius)
      return;

    const int centreXi = static_cast<int>(centreX);
    const int centreYi = static_cast<int>(centreY);
    const int wholeRadius = static_cast<int>(radius);

    if (wholeRadius <= 0) {
      SDL_RenderDrawPoint(renderer, centreXi, centreYi);
      return;
    }

    int x = wholeRadius;
    int y = 0;
    // The decision variable of the midpoint algorithm: the error of the
    // candidate pixel one step in, kept in integers so no rounding creeps in.
    int error = 1 - wholeRadius;

    while (x >= y) {
      // The eight octants of one computed point. Adjacent octants share their
      // end pixels, which costs a handful of redundant SDL_RenderDrawPoint
      // calls per circle and keeps the loop free of special cases.
      SDL_RenderDrawPoint(renderer, centreXi + x, centreYi + y);
      SDL_RenderDrawPoint(renderer, centreXi + y, centreYi + x);
      SDL_RenderDrawPoint(renderer, centreXi - y, centreYi + x);
      SDL_RenderDrawPoint(renderer, centreXi - x, centreYi + y);
      SDL_RenderDrawPoint(renderer, centreXi - x, centreYi - y);
      SDL_RenderDrawPoint(renderer, centreXi - y, centreYi - x);
      SDL_RenderDrawPoint(renderer, centreXi + y, centreYi - x);
      SDL_RenderDrawPoint(renderer, centreXi + x, centreYi - y);

      ++y;
      if (error < 0) {
        error += 2 * y + 1;
      } else {
        --x;
        error += 2 * (y - x) + 1;
      }
    }
  }
};
} // namespace storm
