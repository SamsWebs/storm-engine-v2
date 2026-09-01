#include "../../common/collision/shapes.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

// <stormengine2/collision/shapes.h> includes glm and nothing else from the
// engine, so this spec deliberately includes no other engine header. If it ever
// needs one to compile, the header has grown a dependency it is not supposed to
// have.

Describe(CollisionShapesSpec) {

  // ── circle vs circle ──────────────────────────────────────────────────────

  It(should_report_overlapping_circles) {
    Assert::That(Overlaps(ContactCircle{0.f, 0.f, 5.f},
                          ContactCircle{6.f, 0.f, 5.f}),
                 Equals(true));
  };

  It(should_not_report_separated_circles) {
    Assert::That(Overlaps(ContactCircle{0.f, 0.f, 5.f},
                          ContactCircle{20.f, 0.f, 5.f}),
                 Equals(false));
  };

  // Strict, matching the AABB rule: exactly touching is not a contact, because
  // a zero-area overlap has no meaningful normal.
  It(should_not_report_circles_that_exactly_touch) {
    Assert::That(Overlaps(ContactCircle{0.f, 0.f, 5.f},
                          ContactCircle{10.f, 0.f, 5.f}),
                 Equals(false));
  };

  It(should_point_the_circle_normal_from_a_toward_b) {
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    // b sits to the right of a, overlapping by 4.
    Assert::That(Manifold(ContactCircle{0.f, 0.f, 5.f},
                          ContactCircle{6.f, 0.f, 3.f}, normal, depth),
                 Equals(true));
    Assert::That(normal.x, EqualsWithDelta(1.0, 0.0001));
    Assert::That(normal.y, EqualsWithDelta(0.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(2.0, 0.0001));

    // Swapping the pair flips the normal and reports the same depth.
    Assert::That(Manifold(ContactCircle{6.f, 0.f, 3.f},
                          ContactCircle{0.f, 0.f, 5.f}, normal, depth),
                 Equals(true));
    Assert::That(normal.x, EqualsWithDelta(-1.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(2.0, 0.0001));
  };

  It(should_normalise_a_diagonal_circle_normal) {
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    // Centres 5 apart on a 3-4-5 triangle, radii summing to 7.
    Assert::That(Manifold(ContactCircle{0.f, 0.f, 4.f},
                          ContactCircle{3.f, 4.f, 3.f}, normal, depth),
                 Equals(true));
    Assert::That(normal.x, EqualsWithDelta(0.6, 0.0001));
    Assert::That(normal.y, EqualsWithDelta(0.8, 0.0001));
    Assert::That(glm::length(normal), EqualsWithDelta(1.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(2.0, 0.0001));
  };

  // Concentric circles overlap maximally, so returning false would hide a real
  // collision -- unlike a shared edge, which is a genuine zero-area overlap.
  // There is no direction to report, so a stable one is chosen.
  It(should_give_concentric_circles_a_stable_normal_and_full_depth) {
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    Assert::That(Manifold(ContactCircle{7.f, 7.f, 4.f},
                          ContactCircle{7.f, 7.f, 3.f}, normal, depth),
                 Equals(true));
    Assert::That(glm::length(normal), EqualsWithDelta(1.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(7.0, 0.0001));
  };

  It(should_return_false_from_circle_manifold_when_they_do_not_overlap) {
    glm::vec2 normal(11.f, 22.f);
    float depth = 33.f;
    Assert::That(Manifold(ContactCircle{0.f, 0.f, 1.f},
                          ContactCircle{50.f, 0.f, 1.f}, normal, depth),
                 Equals(false));
    // Outputs untouched on a miss, so a caller reusing them cannot mistake a
    // stale value for a fresh contact.
    Assert::That(normal.x, EqualsWithDelta(11.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(33.0, 0.0001));
  };

  // ── circle vs AABB ────────────────────────────────────────────────────────

  It(should_report_a_circle_overlapping_a_box_face) {
    const ContactAABB box{0.f, 0.f, 10.f, 10.f};
    Assert::That(Overlaps(ContactCircle{15.f, 5.f, 6.f}, box), Equals(true));
    Assert::That(Overlaps(box, ContactCircle{15.f, 5.f, 6.f}), Equals(true));
    Assert::That(Overlaps(ContactCircle{15.f, 5.f, 4.f}, box), Equals(false));
  };

  It(should_not_report_a_circle_exactly_tangent_to_a_box) {
    Assert::That(Overlaps(ContactCircle{15.f, 5.f, 5.f},
                          ContactAABB{0.f, 0.f, 10.f, 10.f}),
                 Equals(false));
  };

  It(should_push_a_circle_out_through_the_nearest_face) {
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    // Circle to the right of the box, overlapping the +X face by 1.
    Assert::That(Manifold(ContactCircle{15.f, 5.f, 6.f},
                          ContactAABB{0.f, 0.f, 10.f, 10.f}, normal, depth),
                 Equals(true));
    Assert::That(normal.x, EqualsWithDelta(-1.0, 0.0001)); // circle toward box
    Assert::That(normal.y, EqualsWithDelta(0.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(1.0, 0.0001));
  };

  // The whole point of circles. A box would report an axis-aligned normal here;
  // a circle glances off the corner along the diagonal, which is what makes a
  // puck ding a round post instead of a square one.
  It(should_give_a_diagonal_normal_at_a_box_corner) {
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    Assert::That(Manifold(ContactCircle{13.f, 13.f, 5.f},
                          ContactAABB{0.f, 0.f, 10.f, 10.f}, normal, depth),
                 Equals(true));
    // Closest point is the corner (10,10); the centre is 3,3 away from it.
    const float expected = -3.0f / std::sqrt(18.0f);
    Assert::That(normal.x, EqualsWithDelta(expected, 0.0001));
    Assert::That(normal.y, EqualsWithDelta(expected, 0.0001));
    Assert::That(glm::length(normal), EqualsWithDelta(1.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(5.0 - std::sqrt(18.0), 0.0001));

    // And it is genuinely different from the axis-aligned answer a box gives.
    Assert::That(std::abs(normal.x) < 0.999f, Equals(true));
    Assert::That(std::abs(normal.y) > 0.001f, Equals(true));
  };

  // A centre inside the box has no line to a closest point -- that would be a
  // divide by zero -- so the least-penetration face is used instead.
  It(should_push_a_circle_whose_centre_is_inside_the_box_out_the_short_way) {
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    // Centre at x=2 in a 0..10 box: the -X face is nearest, 2 away.
    Assert::That(Manifold(ContactCircle{2.f, 5.f, 1.f},
                          ContactAABB{0.f, 0.f, 10.f, 10.f}, normal, depth),
                 Equals(true));
    Assert::That(normal.x, EqualsWithDelta(1.0, 0.0001));
    Assert::That(normal.y, EqualsWithDelta(0.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(3.0, 0.0001)); // 2 to the face + radius

    // Nearest face on the other axis.
    Assert::That(Manifold(ContactCircle{5.f, 9.f, 1.f},
                          ContactAABB{0.f, 0.f, 10.f, 10.f}, normal, depth),
                 Equals(true));
    Assert::That(normal.x, EqualsWithDelta(0.0, 0.0001));
    Assert::That(normal.y, EqualsWithDelta(-1.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(2.0, 0.0001));
  };

  // A circle centred exactly on an edge has half its area inside the box. That
  // is a positive-area overlap and therefore a contact -- strictness rules out
  // TANGENT touches, not this. Overlaps and Manifold have to agree about it,
  // and they did not: both said no contact until mutation testing showed the
  // guard could be deleted with nothing failing.
  It(should_treat_a_circle_centred_on_a_box_edge_as_a_contact) {
    const ContactAABB box{0.f, 0.f, 10.f, 10.f};
    const ContactCircle onEdge{10.f, 5.f, 5.f};

    Assert::That(Overlaps(onEdge, box), Equals(true));

    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    Assert::That(Manifold(onEdge, box, normal, depth), Equals(true));
    // Nearest face is the one it sits on, so it pushes straight out by exactly
    // the radius.
    Assert::That(depth, EqualsWithDelta(5.0, 0.0001));
    Assert::That(glm::length(normal), EqualsWithDelta(1.0, 0.0001));

    // A corner is the same story.
    Assert::That(Overlaps(ContactCircle{10.f, 10.f, 4.f}, box), Equals(true));
  };

  It(should_reverse_the_normal_when_the_box_is_given_first) {
    glm::vec2 circleFirst(0.f, 0.f), boxFirst(0.f, 0.f);
    float d1 = 0.f, d2 = 0.f;
    const ContactCircle circle{13.f, 13.f, 5.f};
    const ContactAABB box{0.f, 0.f, 10.f, 10.f};

    Assert::That(Manifold(circle, box, circleFirst, d1), Equals(true));
    Assert::That(Manifold(box, circle, boxFirst, d2), Equals(true));

    Assert::That(boxFirst.x, EqualsWithDelta(-circleFirst.x, 0.0001));
    Assert::That(boxFirst.y, EqualsWithDelta(-circleFirst.y, 0.0001));
    Assert::That(d2, EqualsWithDelta(d1, 0.0001));
  };

  It(should_treat_a_zero_radius_circle_as_a_point) {
    const ContactAABB box{0.f, 0.f, 10.f, 10.f};
    Assert::That(Overlaps(ContactCircle{5.f, 5.f, 0.f}, box), Equals(true));
    Assert::That(Overlaps(ContactCircle{10.f, 5.f, 0.f}, box),
                 Equals(false)); // on the boundary: a zero-area touch
    Assert::That(Overlaps(ContactCircle{11.f, 5.f, 0.f}, box), Equals(false));
  };

  // Overlaps and Manifold must never disagree. A zero-radius point resting on
  // the boundary is the case that separates "on the edge with area" (a contact,
  // above) from "on the edge with none" (not one) -- and it is the only input
  // that reaches the inside-the-box branch with zero penetration.
  It(should_not_report_a_contact_for_a_zero_radius_point_on_the_boundary) {
    const ContactAABB box{0.f, 0.f, 10.f, 10.f};
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;

    const ContactCircle onEdge{10.f, 5.f, 0.f};
    Assert::That(Overlaps(onEdge, box), Equals(false));
    Assert::That(Manifold(onEdge, box, normal, depth), Equals(false));

    const ContactCircle onCorner{0.f, 0.f, 0.f};
    Assert::That(Overlaps(onCorner, box), Equals(false));
    Assert::That(Manifold(onCorner, box, normal, depth), Equals(false));

    // ... while a point strictly inside is a contact in both.
    const ContactCircle inside{5.f, 5.f, 0.f};
    Assert::That(Overlaps(inside, box), Equals(true));
    Assert::That(Manifold(inside, box, normal, depth), Equals(true));
  };

  // ── resolution ────────────────────────────────────────────────────────────

  // The direction here is the one the old prose got backwards: the normal runs
  // from `a` INTO `b`, so this separates when applied to `b`.
  It(should_separate_the_pair_when_the_translation_is_applied_to_b) {
    ContactCircle a{0.f, 0.f, 5.f};
    ContactCircle b{6.f, 0.f, 3.f};
    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    Assert::That(Manifold(a, b, normal, depth), Equals(true));

    const glm::vec2 mtv = MinimumTranslation(normal, depth);
    Assert::That(mtv.x, EqualsWithDelta(2.0, 0.0001));

    b.x += mtv.x;
    b.y += mtv.y;
    Assert::That(Overlaps(a, b), Equals(false));

    // Applying it to `a` instead drives them further together, which is exactly
    // the mistake the corrected comment warns about.
    ContactCircle a2{0.f, 0.f, 5.f};
    ContactCircle b2{6.f, 0.f, 3.f};
    a2.x += mtv.x;
    Assert::That(Overlaps(a2, b2), Equals(true));
  };

  // ── the AABB math still behaves as it always did ──────────────────────────

  // Same cases as the ContactSystem statics, called as free functions, so the
  // move out of the class is pinned as behaviour-preserving rather than assumed.
  It(should_keep_the_box_math_unchanged) {
    const ContactAABB a{0.f, 0.f, 10.f, 10.f};
    const ContactAABB b{8.f, 2.f, 18.f, 12.f};
    Assert::That(Overlaps(a, b), Equals(true));
    Assert::That(Overlaps(a, ContactAABB{10.f, 0.f, 20.f, 10.f}),
                 Equals(false)); // shared edge is not a contact

    glm::vec2 normal(0.f, 0.f);
    float depth = 0.f;
    Assert::That(Manifold(a, b, normal, depth), Equals(true));
    Assert::That(normal.x, EqualsWithDelta(1.0, 0.0001));
    Assert::That(normal.y, EqualsWithDelta(0.0, 0.0001));
    Assert::That(depth, EqualsWithDelta(2.0, 0.0001));
  };
};
