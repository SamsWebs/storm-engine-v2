#include "../../common/components/rigidBody.h"
#include "../../common/systems/contact.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

// Builds an entity the ContactSystem will accept: transform + box collider.
Entity MakeCollider(Registry &registry, glm::vec2 position, int width,
                    int height, glm::vec2 scale = glm::vec2(1, 1),
                    glm::vec2 offset = glm::vec2(0, 0)) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, scale, 0.0);
  entity.AddComponent<BoxColliderComponent>(width, height, offset);
  return entity;
}

} // namespace

Describe(ContactSystemSpec){

    It(reports_no_contact_for_boxes_that_only_touch_at_an_edge){
        Registry registry;
registry.AddSystem<ContactSystem>();

MakeCollider(registry, {0, 0}, 10, 10);
MakeCollider(registry, {10, 0}, 10, 10);

registry.Update();
auto &system = registry.GetSystem<ContactSystem>();
system.Update();

// Deliberately stricter than CollisionSystem::isCollision, which is
// inclusive and counts a shared edge as a collision.
Assert::That(system.GetContacts().size(), Equals(0u));
}

It(reports_an_overlapping_pair_exactly_once) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  MakeCollider(registry, {0, 0}, 10, 10);
  MakeCollider(registry, {5, 5}, 10, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(1u));
}

It(scales_collider_extents_but_not_the_offset) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity entity = MakeCollider(registry, {0, 0}, 10, 10, {2, 2}, {4, 0});

  // The offset is world pixels, the extents are local units scaled by the
  // transform. RenderColliderSystem draws exactly this, by calling this
  // function rather than keeping its own copy of the arithmetic. Changing the
  // convention is a 2.0.0 item.
  const auto bounds = ContactSystem::BoundsOf(entity);
  Assert::That(bounds.minX, EqualsWithDelta(4.0, 0.001));
  Assert::That(bounds.maxX, EqualsWithDelta(24.0, 0.001));
}

It(picks_the_axis_of_least_penetration_for_the_normal) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // 2px of overlap on x, 8px on y -> the normal must be the x axis.
  MakeCollider(registry, {0, 0}, 10, 10);
  MakeCollider(registry, {8, 2}, 10, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(1u));
  const auto &contact = system.GetContacts()[0];
  Assert::That(contact.normal.x, EqualsWithDelta(1.0, 0.001));
  Assert::That(contact.normal.y, EqualsWithDelta(0.0, 0.001));
  Assert::That(contact.depth, EqualsWithDelta(2.0, 0.001));
}

It(reports_the_normal_pointing_from_the_lower_id_toward_the_higher) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // The higher-id entity sits to the LEFT, so a normal that simply followed
  // iteration order would come out +1. It must follow ids instead.
  MakeCollider(registry, {8, 0}, 10, 10);
  MakeCollider(registry, {0, 0}, 10, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(1u));
  const auto &contact = system.GetContacts()[0];
  Assert::That(contact.a.GetId() < contact.b.GetId(), IsTrue());
  Assert::That(contact.normal.x, EqualsWithDelta(-1.0, 0.001));
}

It(offers_the_minimum_translation_needed_to_separate_a_pair) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  MakeCollider(registry, {0, 0}, 10, 10);
  MakeCollider(registry, {8, 2}, 10, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();

  const auto mtv = ContactSystem::MinimumTranslation(system.GetContacts()[0]);
  Assert::That(mtv.x, EqualsWithDelta(2.0, 0.001));
  Assert::That(mtv.y, EqualsWithDelta(0.0, 0.001));
}

// The ContactSystem:: spellings are the compat promise made when the math
// moved to <stormengine2/collision/shapes.h>: they are public, they shipped in
// 2.0.0, and they had NO caller anywhere in the repo -- both forwarders could
// be deleted with the whole suite still green. These are the callers.
It(should_keep_forwarding_the_contact_system_statics) {
  const ContactAABB a{0.f, 0.f, 10.f, 10.f};
  const ContactAABB b{8.f, 2.f, 18.f, 12.f};

  Assert::That(ContactSystem::Overlaps(a, b), Equals(true));
  Assert::That(ContactSystem::Overlaps(a, ContactAABB{20.f, 0.f, 30.f, 10.f}),
               Equals(false));

  glm::vec2 normal(0.f, 0.f);
  float depth = 0.f;
  Assert::That(ContactSystem::Manifold(a, b, normal, depth), Equals(true));
  Assert::That(normal.x, EqualsWithDelta(1.0, 0.0001));
  Assert::That(depth, EqualsWithDelta(2.0, 0.0001));

  // Identical to the free functions they forward to.
  glm::vec2 freeNormal(0.f, 0.f);
  float freeDepth = 0.f;
  Assert::That(Manifold(a, b, freeNormal, freeDepth), Equals(true));
  Assert::That(normal.x, EqualsWithDelta(freeNormal.x, 0.0001));
  Assert::That(depth, EqualsWithDelta(freeDepth, 0.0001));

  // MinimumTranslation's forwarder is already covered by the
  // minimum-translation case above, which builds a real Contact through the
  // registry -- Contact is not default constructible, because Entity has no
  // default constructor.
};

It(never_kills_an_entity_however_deeply_it_overlaps) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // Both movable and fully overlapping: CollisionSystem would kill both.
  Entity a = MakeCollider(registry, {0, 0}, 10, 10);
  Entity b = MakeCollider(registry, {0, 0}, 10, 10);
  a.AddComponent<RigidBodyComponent>();
  b.AddComponent<RigidBodyComponent>();

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();
  registry.Update(); // would flush any deferred kills

  Assert::That(system.GetSystemEntities().size(), Equals(2u));
}

It(clears_the_previous_frames_contacts_on_every_update) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeCollider(registry, {0, 0}, 10, 10);
  MakeCollider(registry, {5, 5}, 10, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();
  Assert::That(system.GetContacts().size(), Equals(1u));

  a.GetComponent<TransformComponent>().position = glm::vec2(500, 500);
  system.Update();
  Assert::That(system.GetContacts().size(), Equals(0u));
}

It(reports_contacts_in_ascending_entity_id_order) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // Three overlapping boxes in a row -> pairs (0,1), (0,2), (1,2).
  MakeCollider(registry, {0, 0}, 10, 10);
  MakeCollider(registry, {4, 0}, 10, 10);
  MakeCollider(registry, {8, 0}, 10, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();

  const auto &contacts = system.GetContacts();
  Assert::That(contacts.size(), Equals(3u));
  for (std::size_t i = 1; i < contacts.size(); ++i) {
    const bool ordered = contacts[i - 1].a.GetId() < contacts[i].a.GetId() ||
                         (contacts[i - 1].a.GetId() == contacts[i].a.GetId() &&
                          contacts[i - 1].b.GetId() < contacts[i].b.GetId());
    Assert::That(ordered, IsTrue());
  }
}

It(finds_a_pair_the_x_axis_sweep_could_have_pruned_early) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // A wide box overlaps a far-right narrow one. If the sweep breaks on the
  // wrong bound, this pair goes missing.
  MakeCollider(registry, {0, 0}, 100, 10);
  MakeCollider(registry, {20, 0}, 5, 10);
  MakeCollider(registry, {90, 0}, 5, 10);

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(2u));
}
}
;
