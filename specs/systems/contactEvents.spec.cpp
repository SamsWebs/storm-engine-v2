#include "../../common/systems/contact.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

Entity MakeEventCollider(Registry &registry, glm::vec2 position) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, glm::vec2(1, 1), 0.0);
  entity.AddComponent<BoxColliderComponent>(10, 10, glm::vec2(0, 0));
  return entity;
}

} // namespace

Describe(ContactEventsSpec){

    It(fires_begin_once_per_pair_not_once_per_frame){Registry registry;
registry.AddSystem<ContactSystem>();

MakeEventCollider(registry, {0, 0});
MakeEventCollider(registry, {5, 5});

registry.Update();
auto &system = registry.GetSystem<ContactSystem>();

int begins = 0;
system.SetOnBeginContact([&begins](const Contact &) { ++begins; });

system.Update();
system.Update();
system.Update();

// Still overlapping on frames 2 and 3, so begin must not fire again.
Assert::That(begins, Equals(1));
}

It(fires_end_when_a_pair_separates) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  int ends = 0;
  system.SetOnEndContact([&ends](const Entity &, const Entity &) { ++ends; });

  system.Update();
  Assert::That(ends, Equals(0));

  a.GetComponent<TransformComponent>().position = glm::vec2(500, 500);
  system.Update();
  Assert::That(ends, Equals(1));

  // Already apart - end must not repeat.
  system.Update();
  Assert::That(ends, Equals(1));
}

It(hands_end_callbacks_entities_that_can_still_reach_their_components) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  bool reachable = false;
  system.SetOnEndContact([&reachable](const Entity &x, const Entity &y) {
    // A bare Entity(id) has a null registry; these must be the real ones.
    reachable = x.HasComponent<TransformComponent>() &&
                y.HasComponent<TransformComponent>();
  });

  system.Update();
  a.GetComponent<TransformComponent>().position = glm::vec2(500, 500);
  system.Update();

  Assert::That(reachable, IsTrue());
}

It(does_not_fire_end_for_a_pair_whose_entity_was_killed) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  int ends = 0;
  system.SetOnEndContact([&ends](const Entity &, const Entity &) { ++ends; });

  system.Update();

  // Registry::Update()'s kill pass pushes the id onto `freeIds`, so firing an
  // end here would hand back a handle naming whatever entity now holds it.
  a.Kill();
  registry.Update();
  system.Update();

  Assert::That(ends, Equals(0));
}

It(does_not_fire_end_naming_a_recycled_entity) {
  // The stricter sibling of the case above: this time the freed id is
  // recycled into a brand-new contact participant before the next tick,
  // rather than left free. ContactSystem must not report the old contact as
  // having "ended" against that new, unrelated entity just because it now
  // holds the same id - see the PairKey/FindLive comments in
  // common/systems/contact.h.
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  Entity b = MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  system.Update(); // a and b overlap; this becomes "previous"

  b.Kill();
  registry.Update(); // id 1 freed, generation bumped

  // Recycle b's id into a new participant that does NOT overlap anyone - far
  // away from `a`. It is still a live ContactSystem member (Transform +
  // BoxCollider, admitted by Update()), which is exactly what lets the old
  // by-id-only FindLive find it: `live` is built from every system member,
  // not just this frame's overlapping pairs.
  Entity recycled = MakeEventCollider(registry, {1000, 1000});
  registry.Update(); // admits `recycled` into ContactSystem

  Assert::That(recycled.GetId(), Equals(b.GetId()));
  Assert::That(recycled.GetGeneration() != b.GetGeneration(), Equals(true));

  bool endedNamingRecycled = false;
  system.SetOnEndContact([&](const Entity &x, const Entity &y) {
    const bool namesRecycled =
        (x.GetId() == recycled.GetId() &&
         x.GetGeneration() == recycled.GetGeneration()) ||
        (y.GetId() == recycled.GetId() &&
         y.GetGeneration() == recycled.GetGeneration());
    if (namesRecycled)
      endedNamingRecycled = true;
  });

  // Nothing overlaps this tick, so `previous`'s (a, b) pair is not present in
  // `current` and must be evaluated through FindLive. Pre-fix, FindLive
  // matched `recycled` by id alone - a real, live, unrelated entity - and
  // fired onEnd(a, recycled) even though `recycled` was never in contact
  // with anything.
  system.Update();

  Assert::That(endedNamingRecycled, Equals(false));
  (void)a;
}

It(fires_begin_again_after_a_pair_separates_and_re_overlaps) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  int begins = 0;
  system.SetOnBeginContact([&begins](const Contact &) { ++begins; });

  system.Update();
  a.GetComponent<TransformComponent>().position = glm::vec2(500, 500);
  system.Update();
  a.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
  system.Update();

  Assert::That(begins, Equals(2));
}

It(carries_the_manifold_on_the_begin_callback) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  MakeEventCollider(registry, {0, 0});
  MakeEventCollider(registry, {8, 2});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  float depth = 0.0f;
  system.SetOnBeginContact([&depth](const Contact &c) { depth = c.depth; });

  system.Update();

  Assert::That(depth, EqualsWithDelta(2.0, 0.001));
}

It(fires_begin_for_a_recycled_entity_that_replaces_a_previous_participant) {
  // The PairKeyOrder half of the recycled-id fix: PairKey carries both
  // sides' generations, so a pair recorded last frame is told apart from a
  // same-id pair that only looks like it because one side's id was recycled
  // in between. Reducing PairKeyOrder to id-only comparison leaves the rest
  // of the suite green - specs/systems/contactEvents.spec.cpp otherwise only
  // covers the FindLive/end half of this fix - but it silently drops this
  // begin event: the recycled pair's key looks equal, by id alone, to the
  // one already sitting in `previous`, so the binary_search against
  // `previous` finds a "match" and Update() never calls onBegin for it.
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  Entity b = MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  int begins = 0;
  system.SetOnBeginContact([&begins](const Contact &) { ++begins; });

  system.Update(); // frame 1: a and b overlap, begin fires once
  Assert::That(begins, Equals(1));

  a.Kill();
  registry.Update(); // a's id freed, generation bumped

  // Recycle a's id onto a new participant that still overlaps b.
  Entity a2 = MakeEventCollider(registry, {0, 2});
  registry.Update(); // admits a2 into ContactSystem

  Assert::That(a2.GetId(), Equals(a.GetId()));
  Assert::That(a2.GetGeneration() != a.GetGeneration(), Equals(true));

  Entity begunA(0);
  Entity begunB(0);
  system.SetOnBeginContact([&begins, &begunA, &begunB](const Contact &c) {
    ++begins;
    begunA = c.a;
    begunB = c.b;
  });

  system.Update(); // frame 2: (a2, b) is a NEW pair - must fire begin again

  Assert::That(begins, Equals(2));
  Assert::That(begunA.GetId() == a2.GetId() &&
                   begunA.GetGeneration() == a2.GetGeneration(),
               Equals(true));
  Assert::That(begunB.GetId() == b.GetId() &&
                   begunB.GetGeneration() == b.GetGeneration(),
               Equals(true));
}

It(runs_without_callbacks_installed) {
  Registry registry;
  registry.AddSystem<ContactSystem>();

  Entity a = MakeEventCollider(registry, {0, 0});
  MakeEventCollider(registry, {5, 5});

  registry.Update();
  auto &system = registry.GetSystem<ContactSystem>();

  system.Update();
  a.GetComponent<TransformComponent>().position = glm::vec2(500, 500);
  system.Update();

  Assert::That(system.GetContacts().size(), Equals(0u));
}
}
;
