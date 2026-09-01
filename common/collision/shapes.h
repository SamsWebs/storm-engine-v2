#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

// ── Collision shapes and the math over them ─────────────────────────────────
//
// Pure geometry. This header includes glm and nothing else from the engine --
// no ECS, no SDL, no components -- so a game that uses none of the engine's
// entity machinery can still use its collision math, and can include it without
// dragging `ecs.h` in behind it.
//
// That was already true of the math in `systems/contact.h`, but not obviously:
// the functions were statics on a class deriving from `System`, which reads like
// an ECS dependency even though nothing in them needs one. A consumer had to
// discover by experiment that it links clean. This header is where the math
// lives now; `systems/contact.h` includes it and keeps its own statics as
// forwarders, so existing calls like `ContactSystem::Overlaps(a, b)` are
// unchanged.
//
// Conventions, shared by every function here:
//
//   * Overlap is STRICT. A shared edge or a tangent touch is not a contact,
//     because a zero-area overlap has no meaningful normal.
//   * `Manifold` writes a UNIT normal pointing from `a` toward `b`, along the
//     axis of least penetration, and a `depth` that is always > 0. It returns
//     false and leaves the outputs untouched when the pair does not overlap.
//   * Shapes are world-space, with any collider offset and transform scale
//     already applied. Nothing here reads a component.

namespace storm {

// A world-space AABB with the collider offset and the transform scale already
// applied.
struct ContactAABB {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
};

// A world-space circle. `radius` has the transform scale already applied, the
// same way ContactAABB's extents do.
//
// Round bodies are not a niche: a puck kept off the boards by its edge, skaters
// pushed apart by a separation radius, and a shot glancing off a goal post are
// all circles, and modelling them as boxes changes how the game feels rather
// than merely how it computes.
struct ContactCircle {
  float x = 0.0f;      // centre
  float y = 0.0f;      // centre
  float radius = 0.0f;
};

// ── AABB vs AABB ────────────────────────────────────────────────────────────

// Strict: a shared edge is not a contact, because a zero-area overlap has no
// meaningful normal, unlike an inclusive comparison that would count a touching
// edge as a collision.
inline bool Overlaps(const ContactAABB &a, const ContactAABB &b) {
  return a.minX < b.maxX && a.maxX > b.minX && a.minY < b.maxY &&
         a.maxY > b.minY;
}

// Axis of least penetration. Returns false when the boxes do not overlap.
inline bool Manifold(const ContactAABB &a, const ContactAABB &b,
                     glm::vec2 &normal, float &depth) {
  const float overlapX = std::min(a.maxX, b.maxX) - std::max(a.minX, b.minX);
  const float overlapY = std::min(a.maxY, b.maxY) - std::max(a.minY, b.minY);
  if (overlapX <= 0.0f || overlapY <= 0.0f)
    return false;

  if (overlapX < overlapY) {
    const float centerA = (a.minX + a.maxX) * 0.5f;
    const float centerB = (b.minX + b.maxX) * 0.5f;
    normal = glm::vec2(centerA <= centerB ? 1.0f : -1.0f, 0.0f);
    depth = overlapX;
  } else {
    const float centerA = (a.minY + a.maxY) * 0.5f;
    const float centerB = (b.minY + b.maxY) * 0.5f;
    normal = glm::vec2(0.0f, centerA <= centerB ? 1.0f : -1.0f);
    depth = overlapY;
  }
  return true;
}

// ── Circle vs circle ────────────────────────────────────────────────────────

inline bool Overlaps(const ContactCircle &a, const ContactCircle &b) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float sum = a.radius + b.radius;
  // Compared squared to avoid the square root, and strict to match the AABB
  // rule: two circles exactly touching are not in contact.
  return (dx * dx + dy * dy) < (sum * sum);
}

// Normal points from `a`'s centre toward `b`'s centre; depth is how far they
// interpenetrate along it.
//
// Concentric centres are the one case with no direction to report. Unlike a
// shared edge -- which is a zero-area overlap and correctly returns false --
// two circles at the same point overlap *maximally*, so returning false there
// would hide a real collision. A stable +X is used instead, with the full
// penetration depth, so a caller separating along the normal still does
// something sane and deterministic rather than dividing by zero.
inline bool Manifold(const ContactCircle &a, const ContactCircle &b,
                     glm::vec2 &normal, float &depth) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float sum = a.radius + b.radius;
  const float distanceSquared = dx * dx + dy * dy;
  if (distanceSquared >= sum * sum)
    return false;

  const float distance = std::sqrt(distanceSquared);
  if (distance <= 0.0f) {
    normal = glm::vec2(1.0f, 0.0f);
    depth = sum;
    return true;
  }
  normal = glm::vec2(dx / distance, dy / distance);
  depth = sum - distance;
  return true;
}

// ── Circle vs AABB ──────────────────────────────────────────────────────────

// The closest point on `box` to (x, y), which is the point itself when it lies
// inside the box.
inline glm::vec2 ClosestPointOn(const ContactAABB &box, float x, float y) {
  return glm::vec2(std::max(box.minX, std::min(x, box.maxX)),
                   std::max(box.minY, std::min(y, box.maxY)));
}

inline bool Overlaps(const ContactCircle &circle, const ContactAABB &box) {
  const glm::vec2 closest = ClosestPointOn(box, circle.x, circle.y);
  const float dx = circle.x - closest.x;
  const float dy = circle.y - closest.y;
  const float distanceSquared = dx * dx + dy * dy;

  // Centre strictly outside the box: contact iff it is nearer than the radius.
  if (distanceSquared > 0.0f)
    return distanceSquared < circle.radius * circle.radius;

  // Centre on or inside the box. Any positive radius means a positive-area
  // overlap -- a circle centred exactly on an edge has half its area inside,
  // which IS a contact. Strictness rules out zero-area overlaps (a tangent
  // touch), not this.
  if (circle.radius > 0.0f)
    return true;

  // A zero-radius circle is a point, and a point has no area, so it only counts
  // when strictly inside -- a point resting on the edge is a zero-area touch.
  return circle.x > box.minX && circle.x < box.maxX && circle.y > box.minY &&
         circle.y < box.maxY;
}

inline bool Overlaps(const ContactAABB &box, const ContactCircle &circle) {
  return Overlaps(circle, box);
}

// Normal points from the circle toward the box.
//
// Two distinct cases, and they need different math rather than one formula:
//
//   * Centre OUTSIDE the box -- the contact is against the closest point on the
//     boundary, so the normal is along the line from the centre to that point.
//     This is what makes a round body glance off a corner as a round body
//     rather than snapping to a face axis, which is the whole reason circles
//     are worth having.
//   * Centre INSIDE the box -- there is no such line (the closest point IS the
//     centre), so the least-penetration face is used, matching how box-vs-box
//     picks its axis. Without this branch a deeply overlapping pair would
//     divide by zero.
inline bool Manifold(const ContactCircle &circle, const ContactAABB &box,
                     glm::vec2 &normal, float &depth) {
  const glm::vec2 closest = ClosestPointOn(box, circle.x, circle.y);
  const float dx = closest.x - circle.x;
  const float dy = closest.y - circle.y;
  const float distanceSquared = dx * dx + dy * dy;

  if (distanceSquared > 0.0f) {
    const float distance = std::sqrt(distanceSquared);
    if (distance >= circle.radius)
      return false;
    normal = glm::vec2(dx / distance, dy / distance);
    depth = circle.radius - distance;
    return true;
  }

  // Centre on or inside the box. Pick the nearest face and push out through it.
  //
  // "On" is included deliberately: a circle centred exactly on an edge has half
  // its area inside the box, so it is a real contact and Overlaps says so. An
  // earlier version returned false here, which made the two disagree -- caught
  // by mutation testing, because removing the guard broke nothing.
  //
  // A zero-radius point exactly on the boundary is the one case that is not a
  // contact, and it falls out: best is then 0 and depth is 0, which the caller
  // sees as no penetration.
  const float toMinX = circle.x - box.minX;
  const float toMaxX = box.maxX - circle.x;
  const float toMinY = circle.y - box.minY;
  const float toMaxY = box.maxY - circle.y;
  if (toMinX < 0.0f || toMaxX < 0.0f || toMinY < 0.0f || toMaxY < 0.0f)
    return false; // outside the box entirely

  float best = toMinX;
  normal = glm::vec2(1.0f, 0.0f); // circle is left of centre: box lies +X of it
  if (toMaxX < best) {
    best = toMaxX;
    normal = glm::vec2(-1.0f, 0.0f);
  }
  if (toMinY < best) {
    best = toMinY;
    normal = glm::vec2(0.0f, 1.0f);
  }
  if (toMaxY < best) {
    best = toMaxY;
    normal = glm::vec2(0.0f, -1.0f);
  }
  depth = best + circle.radius;
  return depth > 0.0f;
}

// Same contact, normal reversed, so a caller can order the pair whichever way
// suits it without having to remember to negate.
inline bool Manifold(const ContactAABB &box, const ContactCircle &circle,
                     glm::vec2 &normal, float &depth) {
  if (!Manifold(circle, box, normal, depth))
    return false;
  normal = -normal;
  return true;
}

// ── Resolution ──────────────────────────────────────────────────────────────

// The penetration vector: it points from `a` toward `b` with a magnitude equal
// to how deeply they overlap.
//
// Read the direction carefully, because the obvious reading is backwards. The
// normal points from `a` INTO `b`, so this vector separates the pair when it is
// applied to `b`, or when its negation is applied to `a`. Applying it to `a`
// unchanged drives them further together. Splitting it -- half to each, negated
// for `a` -- separates both.
//
// The engine does not apply it: there is no scheduler, so resolution order is
// the game's call.
//
// Takes the normal and depth rather than a Contact, because Contact holds two
// Entity values and a game with no ECS cannot build one -- which made the
// Contact-shaped version unreachable for exactly the callers this header is
// for.
inline glm::vec2 MinimumTranslation(const glm::vec2 &normal, float depth) {
  return normal * depth;
}

} // namespace storm
