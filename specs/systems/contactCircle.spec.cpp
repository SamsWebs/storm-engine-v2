#include "../../common/systems/contact.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

Entity MakeBox(Registry &registry, glm::vec2 position, int width, int height,
               glm::vec2 scale = glm::vec2(1, 1),
               glm::vec2 offset = glm::vec2(0, 0)) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, scale, 0.0);
  entity.AddComponent<BoxColliderComponent>(width, height, offset);
  return entity;
}

// `position` is the transform; the circle's centre lands at position + offset,
// because a circle collider's offset places the centre rather than a corner.
Entity MakeCircle(Registry &registry, glm::vec2 position, float radius,
                  glm::vec2 scale = glm::vec2(1, 1),
                  glm::vec2 offset = glm::vec2(0, 0)) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, scale, 0.0);
  entity.AddComponent<CircleColliderComponent>(radius, offset);
  return entity;
}

} // namespace

Describe(ContactSystemCircleSpec) {

  It(reports_two_overlapping_circles) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeCircle(registry, {0, 0}, 10.0f);
    MakeCircle(registry, {15, 0}, 10.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    const Contact &contact = system.GetContacts()[0];
    Assert::That(contact.normal.x, EqualsWithDelta(1.0f, 1e-5f));
    Assert::That(contact.normal.y, EqualsWithDelta(0.0f, 1e-5f));
    Assert::That(contact.depth, EqualsWithDelta(5.0f, 1e-5f));
  }

  It(reports_no_contact_for_circles_that_only_touch) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeCircle(registry, {0, 0}, 10.0f);
    MakeCircle(registry, {20, 0}, 10.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    // Same strictness rule as boxes: a tangent touch has no meaningful normal.
    Assert::That(system.GetContacts().size(), Equals(0u));
  }

  It(pairs_a_circle_against_a_box) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Box spans x 0..10, y 0..10. Circle centred at (14, 5), radius 6, so it
    // reaches 2px past the box's right face.
    MakeBox(registry, {0, 0}, 10, 10);
    MakeCircle(registry, {14, 5}, 6.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    const Contact &contact = system.GetContacts()[0];
    // `a` is the box (lower id), so the normal points box -> circle: +X.
    Assert::That(contact.normal.x, EqualsWithDelta(1.0f, 1e-5f));
    Assert::That(contact.normal.y, EqualsWithDelta(0.0f, 1e-5f));
    Assert::That(contact.depth, EqualsWithDelta(2.0f, 1e-5f));
  }

  It(points_the_normal_from_the_lower_id_whichever_shape_it_is) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Same geometry as above with the creation order swapped, so the circle
    // now holds the lower id and the normal must flip.
    MakeCircle(registry, {14, 5}, 6.0f);
    MakeBox(registry, {0, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    const Contact &contact = system.GetContacts()[0];
    Assert::That(contact.normal.x, EqualsWithDelta(-1.0f, 1e-5f));
    Assert::That(contact.depth, EqualsWithDelta(2.0f, 1e-5f));
  }

  It(glances_off_a_box_corner_with_a_diagonal_normal) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // The whole reason circles exist here. A box in this position would report
    // an axis-aligned normal; a circle reports the line to the corner.
    MakeBox(registry, {0, 0}, 10, 10);
    MakeCircle(registry, {13, 13}, 6.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    const Contact &contact = system.GetContacts()[0];
    // Corner is (10, 10); the centre sits diagonally out at (13, 13).
    const float diagonal = 0.70710678f;
    Assert::That(contact.normal.x, EqualsWithDelta(diagonal, 1e-4f));
    Assert::That(contact.normal.y, EqualsWithDelta(diagonal, 1e-4f));
  }

  It(ignores_entities_that_carry_no_collider) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Membership is TransformComponent alone, so these two are in the system.
    // They must still produce nothing.
    Entity bare = registry.CreateEntity();
    bare.AddComponent<TransformComponent>(glm::vec2(0, 0), glm::vec2(1, 1),
                                          0.0);
    Entity alsoBare = registry.CreateEntity();
    alsoBare.AddComponent<TransformComponent>(glm::vec2(1, 1), glm::vec2(1, 1),
                                              0.0);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(0u));
  }

  It(places_the_circle_offset_at_the_centre_not_a_corner) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Transform at the origin, offset (100, 0): the centre is at (100, 0), so
    // a box sitting at the origin is far out of reach.
    MakeCircle(registry, {0, 0}, 10.0f, {1, 1}, {100, 0});
    MakeBox(registry, {0, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(0u));
  }

  It(scales_the_radius_by_the_larger_absolute_axis) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Radius 5 scaled by (1, 3) is 15, so the circle at x=20 reaches back to
    // x=5 and overlaps a box spanning 0..10. Scaling by x alone would not.
    MakeBox(registry, {0, 0}, 10, 10);
    MakeCircle(registry, {20, 5}, 5.0f, {1, 3});

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    Assert::That(system.GetContacts()[0].depth, EqualsWithDelta(5.0f, 1e-5f));
  }

  It(keeps_the_radius_positive_under_a_mirrored_scale) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // A flipped sprite must keep the collider it had, not lose it to a
    // negative radius the solvers would clamp to a point.
    MakeBox(registry, {0, 0}, 10, 10);
    MakeCircle(registry, {14, 5}, 6.0f, {-1, 1});

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    Assert::That(system.GetContacts()[0].depth, EqualsWithDelta(2.0f, 1e-5f));
  }

  It(prefers_the_box_when_an_entity_carries_both_colliders) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // The box spans 0..10; the circle would reach out to x=100. If the circle
    // won, the far probe would be contacted. It must not be.
    Entity both = registry.CreateEntity();
    both.AddComponent<TransformComponent>(glm::vec2(0, 0), glm::vec2(1, 1),
                                          0.0);
    both.AddComponent<BoxColliderComponent>(10, 10);
    both.AddComponent<CircleColliderComponent>(100.0f, glm::vec2(5, 5));

    MakeBox(registry, {50, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(0u));
  }

  // Widening the requirement to TransformComponent alone changed this, and it
  // is worth knowing: 2.1.x computed membership once against
  // Transform + BoxCollider, so a collider added to a live entity never got it
  // into the system. The entity is now a member on its transform alone, and
  // Update() re-reads the collider every frame, so a body can grow one late.
  It(picks_up_a_collider_added_after_the_entity_was_admitted) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    Entity box = registry.CreateEntity();
    box.AddComponent<TransformComponent>(glm::vec2(0, 0), glm::vec2(1, 1), 0.0);
    Entity circle = registry.CreateEntity();
    circle.AddComponent<TransformComponent>(glm::vec2(14, 5), glm::vec2(1, 1),
                                            0.0);
    registry.Update();

    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();
    Assert::That(system.GetContacts().size(), Equals(0u));

    box.AddComponent<BoxColliderComponent>(10, 10);
    circle.AddComponent<CircleColliderComponent>(6.0f);
    registry.Update();

    system.Update();
    Assert::That(system.GetContacts().size(), Equals(1u));
    Assert::That(system.GetContacts()[0].depth, EqualsWithDelta(2.0f, 1e-5f));
  }

  // The same property from the other side. Removing a collider does not revoke
  // membership -- the transform is still there -- so the narrowing in Update()
  // is what has to drop the body, and it does.
  It(drops_a_collider_removed_from_a_live_entity) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeBox(registry, {0, 0}, 10, 10);
    Entity circle = MakeCircle(registry, {14, 5}, 6.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();
    Assert::That(system.GetContacts().size(), Equals(1u));

    circle.RemoveComponent<CircleColliderComponent>();
    registry.Update();

    system.Update();
    Assert::That(system.GetContacts().size(), Equals(0u));
  }

  // The Entity-taking overload, which is public API the sweep itself does not
  // use -- it resolves components once inside Update() instead. Games call it,
  // so it is worth pinning that it agrees with the two-argument form and with
  // the offset-is-the-centre and larger-axis rules.
  It(resolves_a_world_circle_from_an_entity) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    Entity entity = MakeCircle(registry, {10, 20}, 3.0f, {2, -4}, {5, 7});
    registry.Update();

    const ContactCircle circle = ContactSystem::CircleOf(entity);
    Assert::That(circle.x, EqualsWithDelta(15.0f, 1e-5f)); // 10 + 5
    Assert::That(circle.y, EqualsWithDelta(27.0f, 1e-5f)); // 20 + 7
    // 3 * max(|2|, |-4|).
    Assert::That(circle.radius, EqualsWithDelta(12.0f, 1e-5f));

    const TransformComponent &transform =
        entity.GetComponent<TransformComponent>();
    const CircleColliderComponent &collider =
        entity.GetComponent<CircleColliderComponent>();
    const ContactCircle same = ContactSystem::CircleOf(transform, collider);
    Assert::That(circle.x, EqualsWithDelta(same.x, 1e-5f));
    Assert::That(circle.y, EqualsWithDelta(same.y, 1e-5f));
    Assert::That(circle.radius, EqualsWithDelta(same.radius, 1e-5f));
  }

  It(fires_begin_and_end_for_a_circle_pair) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    Entity moving = MakeCircle(registry, {15, 0}, 10.0f);
    MakeCircle(registry, {0, 0}, 10.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();

    int begins = 0;
    int ends = 0;
    system.SetOnBeginContact([&begins](const Contact &) { ++begins; });
    system.SetOnEndContact([&ends](const Entity &, const Entity &) { ++ends; });

    system.Update();
    Assert::That(begins, Equals(1));
    Assert::That(ends, Equals(0));

    // Still touching: no second begin.
    system.Update();
    Assert::That(begins, Equals(1));

    moving.GetComponent<TransformComponent>().position = glm::vec2(100, 0);
    system.Update();
    Assert::That(begins, Equals(1));
    Assert::That(ends, Equals(1));
  }
};
