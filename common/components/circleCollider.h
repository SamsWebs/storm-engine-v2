#pragma once

#include <glm/glm.hpp>

namespace storm {

// A round collider. `ContactSystem` accepts an entity carrying either this or
// a `BoxColliderComponent`, and pairs the two shapes against each other, so a
// puck can be a circle while the boards it rides against stay boxes.
//
// Two conventions differ from `BoxColliderComponent`, and both are deliberate:
//
//   * `offset` places the CENTRE, not a corner. A box has a natural top-left to
//     anchor at `transform.position`; a circle does not, and anchoring its
//     bounding box's corner would make the offset that centres a collider on a
//     sprite depend on the radius. A circle collider centred on a 32x32 sprite
//     drawn from `transform.position` therefore wants `offset = {16, 16}`.
//   * `radius` is a float. A box's `width`/`height` are ints because they are
//     usually a sprite cell; a radius is usually half of one, and rounding
//     3.5 px to 3 or 4 is a visible difference on a small body.
//
// Like `BoxColliderComponent`, `offset` is world pixels and is NOT scaled by
// the transform, while `radius` is. Non-uniform scale cannot make an ellipse:
// see `ContactSystem::CircleOf` for which axis wins and why.
struct CircleColliderComponent {
  float radius;
  glm::vec2 offset;

  CircleColliderComponent(float radius = 0.0f, glm::vec2 offset = glm::vec2(0))
      : radius{radius}, offset{offset} {}
};
} // namespace storm
