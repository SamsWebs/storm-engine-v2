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
//   * `Manifold` writes a UNIT normal pointing from `a` toward `b` and a
//     `depth` that is always > 0, and returns false leaving the outputs
//     untouched when the pair does not overlap.
//
//     The normal is the axis of least penetration only for box vs box, and for
//     a circle whose centre is inside a box. When a circle's centre is outside,
//     it runs along the line to the closest point on the other shape -- which
//     is the whole reason circles are here, since that is what makes a round
//     body glance off a corner instead of snapping to a face.
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

// The closest point on `box` to (x, y), which is the point itself when it lies
// inside the box. For an inverted box (minX > maxX) the clamp collapses to
// minX, which the solvers then reject rather than building a manifold from
// negative extents.
inline glm::vec2 ClosestPointOn(const ContactAABB &box, float x, float y) {
  return glm::vec2(std::max(box.minX, std::min(x, box.maxX)),
                   std::max(box.minY, std::min(y, box.maxY)));
}

// ── One source of truth per shape pair ──────────────────────────────────────
//
// Each pair has exactly ONE function that decides contact and computes the
// manifold; `Overlaps` and `Manifold` are both thin wrappers over it. That is
// structural, not stylistic. When the two were written separately they drifted
// apart in four different ways -- `Overlaps` compared squared distances while
// `Manifold` took a square root and compared against the radius, so `sqrt`
// rounding up to exactly the radius made them disagree on about 1.8% of
// near-tangent probes at box corners; a zero-size AABB overlapped by one
// measure and not the other; a negative radius was squared by one and compared
// signed by the other; and a NaN coordinate produced "yes" from one and "no"
// from the other. Every one of those is impossible now: there is nothing left
// to disagree.
//
// The wrappers also give the outputs-untouched guarantee for free. The solver
// writes only to its own locals, and `Manifold` copies them out after it knows
// the answer -- an earlier version assigned the normal on the way to deciding
// and then returned false, leaving a caller holding a normal for a contact
// that was never reported.
namespace detail {

// A radius is a length. Negative is not a smaller circle, it is nonsense, and
// the two measures used to disagree about which nonsense. Clamping to zero
// makes such a circle a point -- the geometric limit -- consistently
// everywhere.
inline float SaneRadius(float radius) { return radius > 0.0f ? radius : 0.0f; }

inline bool SolveBoxBox(const ContactAABB &a, const ContactAABB &b,
                        glm::vec2 &normal, float &depth) {
  const float overlapX = std::min(a.maxX, b.maxX) - std::max(a.minX, b.minX);
  const float overlapY = std::min(a.maxY, b.maxY) - std::max(a.minY, b.minY);
  // Not `<= 0`: NaN fails `> 0` too, so this rejects NaN rather than carrying
  // it into the outputs.
  if (!(overlapX > 0.0f) || !(overlapY > 0.0f))
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

inline bool SolveCircleCircle(const ContactCircle &a, const ContactCircle &b,
                              glm::vec2 &normal, float &depth) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float sum = SaneRadius(a.radius) + SaneRadius(b.radius);
  const float distanceSquared = dx * dx + dy * dy;
  const float distance = std::sqrt(distanceSquared);
  const float penetration = sum - distance;

  // Decided on the penetration itself, not on a squared comparison that a
  // later sqrt might contradict. Two circles overlapping by less than float
  // precision at their scale are reported as not touching -- which is the only
  // answer that can also honour "depth is always > 0". NaN fails this too.
  if (!(penetration > 0.0f))
    return false;

  if (distance > 0.0f) {
    normal = glm::vec2(dx / distance, dy / distance);
  } else {
    // Concentric: overlapping maximally, with no direction to report. A stable
    // +X keeps callers deterministic instead of dividing by zero.
    normal = glm::vec2(1.0f, 0.0f);
  }
  depth = penetration;
  return true;
}

inline bool SolveCircleBox(const ContactCircle &circle, const ContactAABB &box,
                           glm::vec2 &normal, float &depth) {
  const float radius = SaneRadius(circle.radius);
  const glm::vec2 closest = ClosestPointOn(box, circle.x, circle.y);
  const float dx = closest.x - circle.x;
  const float dy = closest.y - circle.y;
  const float distanceSquared = dx * dx + dy * dy;

  if (distanceSquared > 0.0f) {
    // Centre outside the box: the contact is against the closest point on the
    // boundary, so the normal runs along the line to it. This is what makes a
    // round body glance off a corner as a round body instead of snapping to a
    // face axis -- the reason circles exist here at all.
    const float distance = std::sqrt(distanceSquared);
    const float penetration = radius - distance;
    if (!(penetration > 0.0f))
      return false;
    normal = glm::vec2(dx / distance, dy / distance);
    depth = penetration;
    return true;
  }

  // Centre on or inside the box. There is no line to a closest point -- it IS
  // the centre -- so the least-penetration face is used, the same way box vs
  // box picks its axis.
  //
  // "On" is included deliberately. The rule across every shape pair here is
  // INTERIOR INTERSECTION: a contact exists when the shapes share a point that
  // is interior to at least one of them. That explains all five cases at once,
  // where the area argument this comment used to make did not -- it justified
  // the on-edge case by "half its area is inside" and then, three lines later,
  // made a zero-area point strictly inside a box a contact too.
  //
  //   tangent circles          shared boundary point only   -> no
  //   circle centred on an edge  half-disk interior to it   -> yes
  //   AABBs sharing an edge    shared boundary segment only -> no
  //   point on the boundary    shared boundary point only   -> no
  //   point strictly inside    interior to the box          -> yes
  const float toMinX = circle.x - box.minX;
  const float toMaxX = box.maxX - circle.x;
  const float toMinY = circle.y - box.minY;
  const float toMaxY = box.maxY - circle.y;
  // Reachable only for an inverted box (minX > maxX), where ClosestPointOn
  // clamps to minX and the centre can land "inside" a box with no interior, or
  // for NaN. Both must report nothing rather than a manifold built from
  // negative extents.
  if (!(toMinX >= 0.0f) || !(toMaxX >= 0.0f) || !(toMinY >= 0.0f) ||
      !(toMaxY >= 0.0f))
    return false;

  float best = toMinX;
  glm::vec2 candidate(1.0f, 0.0f); // nearest face is minX: the box lies +X
  if (toMaxX < best) {
    best = toMaxX;
    candidate = glm::vec2(-1.0f, 0.0f);
  }
  if (toMinY < best) {
    best = toMinY;
    candidate = glm::vec2(0.0f, 1.0f);
  }
  if (toMaxY < best) {
    best = toMaxY;
    candidate = glm::vec2(0.0f, -1.0f);
  }

  const float penetration = best + radius;
  // A zero-radius point resting exactly on the boundary: no area, no contact,
  // matching the tangent rule.
  if (!(penetration > 0.0f))
    return false;
  normal = candidate;
  depth = penetration;
  return true;
}

} // namespace detail

// ── Bounds ──────────────────────────────────────────────────────────────────

// The tight AABB around a circle.
//
// This is a broadphase proxy, not a shape you should test against: a circle
// and its bounding box disagree at all four corners, which is exactly the
// difference a round body exists to express. `ContactSystem` sweeps these and
// then runs the real circle solver on whatever survives.
//
// A negative radius is clamped the same way the solvers clamp it, so the box
// around a nonsense circle is a degenerate point rather than an inverted AABB
// the box solver would then reject for a second, unrelated reason.
inline ContactAABB BoundsOf(const ContactCircle &circle) {
  const float radius = detail::SaneRadius(circle.radius);
  ContactAABB box;
  box.minX = circle.x - radius;
  box.minY = circle.y - radius;
  box.maxX = circle.x + radius;
  box.maxY = circle.y + radius;
  return box;
}

// ── Finiteness ──────────────────────────────────────────────────────────────
//
// Every solver here already rejects a NaN deliberately -- each one tests
// `!(overlap > 0.0f)` rather than `<= 0.0f`, so a NaN falls out as "no
// contact" instead of propagating into a normal. That makes the SHAPES safe.
//
// What is not safe is ordering them. A broadphase that sorts bodies along an
// axis compares NaN against everything and gets `false` both ways, which reads
// as "equivalent" while the finite values still order among themselves -- not a
// strict weak ordering, and `std::sort` on one is undefined rather than merely
// wrong. So a body has to be rejected BEFORE it reaches a comparator, and that
// is what these are for.
//
// Infinity is included: an infinite extent orders fine but makes every overlap
// test against it inf - inf, which is NaN again one step later.
inline bool IsFinite(const ContactAABB &box) {
  return std::isfinite(box.minX) && std::isfinite(box.minY) &&
         std::isfinite(box.maxX) && std::isfinite(box.maxY);
}

inline bool IsFinite(const ContactCircle &circle) {
  return std::isfinite(circle.x) && std::isfinite(circle.y) &&
         std::isfinite(circle.radius);
}

// ── AABB vs AABB ────────────────────────────────────────────────────────────

inline bool Overlaps(const ContactAABB &a, const ContactAABB &b) {
  glm::vec2 normal(0.0f, 0.0f);
  float depth = 0.0f;
  return detail::SolveBoxBox(a, b, normal, depth);
}

inline bool Manifold(const ContactAABB &a, const ContactAABB &b,
                     glm::vec2 &normal, float &depth) {
  glm::vec2 solvedNormal(0.0f, 0.0f);
  float solvedDepth = 0.0f;
  if (!detail::SolveBoxBox(a, b, solvedNormal, solvedDepth))
    return false;
  normal = solvedNormal;
  depth = solvedDepth;
  return true;
}

// ── Circle vs circle ────────────────────────────────────────────────────────

inline bool Overlaps(const ContactCircle &a, const ContactCircle &b) {
  glm::vec2 normal(0.0f, 0.0f);
  float depth = 0.0f;
  return detail::SolveCircleCircle(a, b, normal, depth);
}

inline bool Manifold(const ContactCircle &a, const ContactCircle &b,
                     glm::vec2 &normal, float &depth) {
  glm::vec2 solvedNormal(0.0f, 0.0f);
  float solvedDepth = 0.0f;
  if (!detail::SolveCircleCircle(a, b, solvedNormal, solvedDepth))
    return false;
  normal = solvedNormal;
  depth = solvedDepth;
  return true;
}

// ── Circle vs AABB ──────────────────────────────────────────────────────────

inline bool Overlaps(const ContactCircle &circle, const ContactAABB &box) {
  glm::vec2 normal(0.0f, 0.0f);
  float depth = 0.0f;
  return detail::SolveCircleBox(circle, box, normal, depth);
}

inline bool Overlaps(const ContactAABB &box, const ContactCircle &circle) {
  return Overlaps(circle, box);
}

// Normal points from the circle toward the box.
inline bool Manifold(const ContactCircle &circle, const ContactAABB &box,
                     glm::vec2 &normal, float &depth) {
  glm::vec2 solvedNormal(0.0f, 0.0f);
  float solvedDepth = 0.0f;
  if (!detail::SolveCircleBox(circle, box, solvedNormal, solvedDepth))
    return false;
  normal = solvedNormal;
  depth = solvedDepth;
  return true;
}

// Same contact, normal reversed, so a caller can order the pair whichever way
// suits it without having to remember to negate. On a miss the outputs are
// untouched, like every other overload -- an earlier version returned early
// without negating and left the caller a normal pointing the wrong way.
inline bool Manifold(const ContactAABB &box, const ContactCircle &circle,
                     glm::vec2 &normal, float &depth) {
  glm::vec2 solvedNormal(0.0f, 0.0f);
  float solvedDepth = 0.0f;
  if (!detail::SolveCircleBox(circle, box, solvedNormal, solvedDepth))
    return false;
  normal = -solvedNormal;
  depth = solvedDepth;
  return true;
}

// ── Resolution ──────────────────────────────────────────────────────────────

// The penetration vector: it points from `a` toward `b` with a magnitude equal
// to `depth`.
//
// Read the direction carefully, because the obvious reading is backwards. The
// normal points from `a` INTO `b`, so this separates the pair when applied to
// `b`, or when its negation is applied to `a`. Applying it to `a` unchanged
// drives them further together.
//
// It is a minimum translation only when `depth` is a true separation distance.
// For circles it always is. For BOX vs BOX it is not, whenever one box is
// contained within the other along the chosen axis: `Manifold` computes the
// overlap as min(maxes) - max(mins), which for containment is the inner box's
// own extent rather than the distance to a face. A={0,0,10,10} against
// B={2,-5,4,15} reports depth 2 where 4 is needed, and applying it separates
// nothing. Resolve deep box overlaps iteratively, or keep bodies from reaching
// containment in the first place.
inline glm::vec2 MinimumTranslation(const glm::vec2 &normal, float depth) {
  return normal * depth;
}

} // namespace storm
