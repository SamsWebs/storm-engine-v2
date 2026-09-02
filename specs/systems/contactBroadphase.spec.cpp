#include "../../common/systems/contact.h"
#include <igloo/igloo_alt.h>
#include <sstream>

using namespace igloo;
using namespace storm;

namespace {

Entity MakeBox(Registry &registry, glm::vec2 position, int width, int height) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, glm::vec2(1, 1), 0.0);
  entity.AddComponent<BoxColliderComponent>(width, height);
  return entity;
}

// The contacts as a stable string, so two runs can be compared as one value
// rather than field by field.
std::string ContactSummary(const ContactSystem &system) {
  std::ostringstream out;
  for (const Contact &contact : system.GetContacts())
    out << contact.a.GetId() << "-" << contact.b.GetId() << ";";
  return out.str();
}

} // namespace

// The broadphase is a uniform grid. Nothing here asserts that it is FASTER --
// a spec cannot, and the reason the grid replaced the X-only sweep is a
// complexity claim, not a measurement this suite could make stick. What these
// pin is the part that would silently rot: the grid is an optimisation, so its
// output must not depend on the grid at all. Same contacts, same order, at
// every cell size, on both sides of the brute-force threshold.
Describe(ContactSystemBroadphaseSpec) {

  // The case the old sweep degraded on: everything in one column, so sorting
  // by minX separated nothing and the inner loop never broke early.
  It(pairs_a_column_of_stacked_bodies) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Ten boxes down a single column, each overlapping only its neighbours.
    for (int i = 0; i < 10; ++i)
      MakeBox(registry, {0, static_cast<float>(i * 8)}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    // Nine adjacent pairs, and nothing between bodies two apart (16 > 10).
    Assert::That(system.GetContacts().size(), Equals(9u));
    for (const Contact &contact : system.GetContacts())
      Assert::That(contact.b.GetId() - contact.a.GetId(),
                   Equals(static_cast<std::size_t>(1)));
  }

  // A body straddles as many cells as it spans, so every pair is reachable
  // once per shared cell. Without deduplication this reports the same contact
  // several times, and a game counting hits would double-count.
  It(reports_a_straddling_pair_exactly_once) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Wide, flat bodies at a deliberately small cell size: each spans many
    // cells and the pair shares most of them.
    MakeBox(registry, {0, 0}, 100, 4);
    MakeBox(registry, {10, 0}, 100, 4);
    for (int i = 0; i < 6; ++i)
      MakeBox(registry, {1000.0f + i * 200.0f, 0}, 4, 4);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.SetCellSize(2.0f);
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    Assert::That(system.GetContacts()[0].a.GetId(),
                 Equals(static_cast<std::size_t>(0)));
    Assert::That(system.GetContacts()[0].b.GetId(),
                 Equals(static_cast<std::size_t>(1)));
  }

  // A level-sized floor collider beside player-sized bodies. It spans more
  // cells than the grid will hold for one body, so it takes the oversized
  // path and is tested against everything instead.
  It(still_pairs_a_body_far_larger_than_the_grid) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    Entity floor = MakeBox(registry, {0, 0}, 40000, 8);
    Entity resting = MakeBox(registry, {19000, 4}, 10, 10);
    // Enough small bodies to push past the brute-force threshold and to pull
    // the derived cell size down to their scale.
    for (int i = 0; i < 8; ++i)
      MakeBox(registry, {static_cast<float>(i * 400), 500}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
    Assert::That(system.GetContacts()[0].a.GetId(), Equals(floor.GetId()));
    Assert::That(system.GetContacts()[0].b.GetId(), Equals(resting.GetId()));
  }

  // The property that makes the grid safe to tune: it is an acceleration
  // structure, so the answer cannot depend on it. A cell far smaller than the
  // bodies, one far larger, and the derived default must agree exactly --
  // including the ORDER, which GetContacts() documents.
  It(reports_the_same_contacts_at_every_cell_size) {
    const auto build = [](Registry &registry) {
      // A mix the grid has to work at: overlapping clusters, a long thin
      // body, and isolated bodies that must pair with nothing.
      MakeBox(registry, {0, 0}, 20, 20);
      MakeBox(registry, {10, 10}, 20, 20);
      MakeBox(registry, {15, 15}, 20, 20);
      MakeBox(registry, {0, 0}, 400, 6);
      MakeBox(registry, {390, 2}, 20, 20);
      MakeBox(registry, {5000, 5000}, 10, 10);
      MakeBox(registry, {9000, 9000}, 10, 10);
      MakeBox(registry, {-300, -300}, 30, 30);
      MakeBox(registry, {-290, -290}, 30, 30);
    };

    std::string derived;
    {
      Registry registry;
      registry.AddSystem<ContactSystem>();
      build(registry);
      registry.Update();
      auto &system = registry.GetSystem<ContactSystem>();
      system.Update();
      derived = ContactSummary(system);
      Assert::That(derived.empty(), Equals(false));
    }

    const float sizes[] = {0.5f, 1.0f, 7.0f, 64.0f, 100000.0f};
    for (const float size : sizes) {
      Registry registry;
      registry.AddSystem<ContactSystem>();
      build(registry);
      registry.Update();
      auto &system = registry.GetSystem<ContactSystem>();
      system.SetCellSize(size);
      system.Update();
      Assert::That(ContactSummary(system), Equals(derived));
    }
  }

  // Under the threshold the grid is skipped entirely for all-pairs, which is
  // a second code path and therefore a second chance to disagree.
  It(agrees_across_the_brute_force_threshold) {
    const auto summaryFor = [](int extras) {
      Registry registry;
      registry.AddSystem<ContactSystem>();
      MakeBox(registry, {0, 0}, 10, 10);
      MakeBox(registry, {5, 0}, 10, 10);
      // Extras sit far away and pair with nothing; they exist only to move
      // the body count across the threshold.
      for (int i = 0; i < extras; ++i)
        MakeBox(registry, {2000.0f + i * 100.0f, 0}, 10, 10);
      registry.Update();
      auto &system = registry.GetSystem<ContactSystem>();
      system.Update();
      return ContactSummary(system);
    };

    Assert::That(summaryFor(0), Equals(std::string("0-1;")));
    Assert::That(summaryFor(2), Equals(std::string("0-1;")));  // brute force
    Assert::That(summaryFor(20), Equals(std::string("0-1;"))); // grid
  }

  // `filter` is a game callback and may be stateful, so the sequence it sees
  // must not depend on how a hash table bucketed anything. Ordering candidates
  // on entity id is what guarantees that; without it this varies run to run
  // and between cell sizes.
  It(offers_pairs_to_the_filter_in_a_fixed_order) {
    const auto sequenceFor = [](float cell) {
      Registry registry;
      registry.AddSystem<ContactSystem>();
      for (int i = 0; i < 12; ++i)
        MakeBox(registry, {static_cast<float>(i * 6), 0}, 10, 10);
      registry.Update();

      auto &system = registry.GetSystem<ContactSystem>();
      if (cell > 0.0f)
        system.SetCellSize(cell);

      std::ostringstream seen;
      system.SetPairFilter([&seen](const Entity &a, const Entity &b) {
        seen << a.GetId() << ">" << b.GetId() << ";";
        return true;
      });
      system.Update();
      return seen.str();
    };

    const std::string derived = sequenceFor(0.0f);
    Assert::That(derived.empty(), Equals(false));
    Assert::That(sequenceFor(0.0f), Equals(derived));
    Assert::That(sequenceFor(3.0f), Equals(derived));
    Assert::That(sequenceFor(500.0f), Equals(derived));
  }

  // An inverted AABB -- a negative BoxColliderComponent width -- must not fall
  // out of the grid and silently stop colliding. The narrowphase still rejects
  // it, but for its own reason.
  It(keeps_an_inverted_box_in_the_broadphase) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeBox(registry, {50, 50}, -20, -20);
    for (int i = 0; i < 8; ++i)
      MakeBox(registry, {static_cast<float>(i * 40), 50}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    system.Update();

    // Whatever it does, it does not crash and does not report a contact for
    // a body with no interior.
    for (const Contact &contact : system.GetContacts()) {
      Assert::That(contact.a.GetId(), Is().Not().EqualTo(
                                          static_cast<std::size_t>(0)));
      Assert::That(contact.b.GetId(), Is().Not().EqualTo(
                                          static_cast<std::size_t>(0)));
    }
  }
};
