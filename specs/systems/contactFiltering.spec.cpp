#include "../../common/systems/contact.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

namespace {

Entity MakeFilterCollider(Registry &registry, glm::vec2 position,
                          const std::string &group) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, glm::vec2(1, 1), 0.0);
  entity.AddComponent<BoxColliderComponent>(10, 10, glm::vec2(0, 0));
  entity.Group(group);
  return entity;
}

} // namespace

Describe(ContactFilteringSpec){

    It(tests_every_pair_when_no_filter_is_installed){Registry registry;
registry.AddSystem<ContactSystem>();

MakeFilterCollider(registry, {0, 0}, "bullets");
MakeFilterCollider(registry, {5, 5}, "bullets");

registry.Update();
auto &system = registry.GetSystem<ContactSystem>();
system.Update();

Assert::That(system.GetContacts().size(), Equals(1u));
}

It(suppresses_a_contact_the_filter_rejects) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  MakeFilterCollider(registry, {0, 0}, "bullets");
  MakeFilterCollider(registry, {5, 5}, "bullets");

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.SetPairFilter([](const Entity &, const Entity &) { return false; });
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(0u));
}

It(suppresses_the_begin_event_for_a_rejected_pair) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  MakeFilterCollider(registry, {0, 0}, "bullets");
  MakeFilterCollider(registry, {5, 5}, "bullets");

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  int begins = 0;
  system.SetOnBeginContact([&begins](const Contact &) { ++begins; });
  system.SetPairFilter([](const Entity &, const Entity &) { return false; });
  system.Update();

  Assert::That(begins, Equals(0));
}

It(lets_bullets_hit_enemies_but_not_each_other) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // Three overlapping boxes: two bullets and one enemy. Without a filter
  // that is 3 contacts; with one it is 2 (each bullet vs the enemy).
  MakeFilterCollider(registry, {0, 0}, "bullets");
  MakeFilterCollider(registry, {4, 0}, "bullets");
  MakeFilterCollider(registry, {8, 0}, "enemies");

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.SetPairFilter([](const Entity &a, const Entity &b) {
    return !(a.BelongsToGroup("bullets") && b.BelongsToGroup("bullets"));
  });
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(2u));
}

It(consults_the_filter_only_for_pairs_that_actually_overlap) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // Two overlapping, one far away. The filter must see the overlapping pair
  // and nothing else - it is the cheap gate in front of the manifold, not a
  // visitor over every pair in the world.
  MakeFilterCollider(registry, {0, 0}, "a");
  MakeFilterCollider(registry, {5, 5}, "b");
  MakeFilterCollider(registry, {900, 900}, "c");

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  int consulted = 0;
  system.SetPairFilter([&consulted](const Entity &, const Entity &) {
    ++consulted;
    return true;
  });
  system.Update();

  Assert::That(consulted, Equals(1));
  Assert::That(system.GetContacts().size(), Equals(1u));
}

It(implements_a_sensor_as_a_filtered_pair_without_a_new_component) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  // Two trigger zones overlapping each other and a player. A sensor should
  // report against the player but never against another sensor.
  MakeFilterCollider(registry, {0, 0}, "sensors");
  MakeFilterCollider(registry, {4, 0}, "sensors");
  MakeFilterCollider(registry, {8, 0}, "player");

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();
  system.SetPairFilter([](const Entity &a, const Entity &b) {
    return !(a.BelongsToGroup("sensors") && b.BelongsToGroup("sensors"));
  });
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(2u));
}
}
;
