#include "../../common/systems/contact.h"
#include "../support/freshDiagnosticBudget.h"
#include <igloo/igloo_alt.h>
#include <limits>

using namespace igloo;
using namespace storm;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

Entity MakeBox(Registry &registry, glm::vec2 position, int width, int height,
               glm::vec2 scale = glm::vec2(1, 1)) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, scale, 0.0);
  entity.AddComponent<BoxColliderComponent>(width, height);
  return entity;
}

Entity MakeCircle(Registry &registry, glm::vec2 position, float radius,
                  glm::vec2 scale = glm::vec2(1, 1)) {
  Entity entity = registry.CreateEntity();
  entity.AddComponent<TransformComponent>(position, scale, 0.0);
  entity.AddComponent<CircleColliderComponent>(radius);
  return entity;
}

std::size_t SpecErrorCount() {
  std::size_t errors = 0;
  for (const auto &entry : Logger::messages) {
    if (entry.type == LogType::LOG_ERROR) {
      ++errors;
    }
  }
  return errors;
}

} // namespace

// A non-finite body is dropped from the sweep BEFORE it reaches the broadphase
// sort. That ordering is the whole point: a comparator involving a NaN returns
// false in both directions, which reads as "equivalent" while the finite values
// still order among themselves. That is not a strict weak ordering, and
// std::sort on one is undefined behaviour rather than merely a wrong order.
//
// So these cases cannot assert "the sort did the right thing" -- undefined is
// undefined, and a green run proves nothing on its own. What they assert is
// that the bad body never gets that far, and that every good body around it is
// unaffected. Run them under a sanitiser to see the difference the gate makes.
Describe(ContactSystemNonFiniteSpec) {

  It(drops_a_box_whose_position_is_not_a_number) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeBox(registry, {kNaN, 0}, 10, 10);
    MakeBox(registry, {5, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    Logger::messages.clear();
    OnFreshDiagnosticBudget([&] { system.Update(); });

    Assert::That(system.GetContacts().size(), Equals(0u));
    // Said once, not swallowed: a body vanishing from collision is a gameplay
    // bug somebody would otherwise chase upstream by hand.
    Assert::That(SpecErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  }

  It(drops_a_circle_whose_radius_is_not_a_number) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeCircle(registry, {0, 0}, kNaN);
    MakeCircle(registry, {5, 0}, 10.0f);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    Logger::messages.clear();
    OnFreshDiagnosticBudget([&] { system.Update(); });

    Assert::That(system.GetContacts().size(), Equals(0u));
    Logger::messages.clear();
  }

  // Infinity orders fine; it is the overlap test one step later that turns
  // inf - inf into a NaN, so it is rejected with the same gate.
  It(drops_a_body_whose_scale_overflowed_to_infinity) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeBox(registry, {0, 0}, 10, 10, {kInf, 1});
    MakeBox(registry, {5, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    Logger::messages.clear();
    OnFreshDiagnosticBudget([&] { system.Update(); });

    Assert::That(system.GetContacts().size(), Equals(0u));
    Logger::messages.clear();
  }

  // The case that used to be undefined: one bad body among enough good ones
  // that the sort actually has work to do.
  It(leaves_every_finite_body_pairing_normally_around_it) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    // Three overlapping pairs, laid out so the sweep must order them, plus one
    // NaN body dropped in the middle of the creation order.
    Entity a = MakeBox(registry, {0, 0}, 10, 10);
    Entity b = MakeBox(registry, {5, 0}, 10, 10);
    MakeBox(registry, {kNaN, kNaN}, 10, 10);
    Entity c = MakeBox(registry, {40, 0}, 10, 10);
    Entity d = MakeBox(registry, {45, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();
    Logger::messages.clear();
    OnFreshDiagnosticBudget([&] { system.Update(); });

    Assert::That(system.GetContacts().size(), Equals(2u));
    Assert::That(system.GetContacts()[0].a.GetId(), Equals(a.GetId()));
    Assert::That(system.GetContacts()[0].b.GetId(), Equals(b.GetId()));
    Assert::That(system.GetContacts()[1].a.GetId(), Equals(c.GetId()));
    Assert::That(system.GetContacts()[1].b.GetId(), Equals(d.GetId()));
    Logger::messages.clear();
  }

  It(fires_no_begin_callback_for_a_non_finite_body) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    MakeBox(registry, {kNaN, 0}, 10, 10);
    MakeBox(registry, {0, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();

    int begins = 0;
    system.SetOnBeginContact([&begins](const Contact &) { ++begins; });

    Logger::messages.clear();
    OnFreshDiagnosticBudget([&] { system.Update(); });

    Assert::That(begins, Equals(0));
    Logger::messages.clear();
  }

  // The gate is per-frame, not a latch: a body whose transform goes bad and
  // comes back collides again. Anything else would turn one bad frame into a
  // permanently ghosted entity.
  It(admits_the_body_again_once_its_transform_is_finite) {
    Registry registry;
    registry.AddSystem<ContactSystem>();

    Entity moving = MakeBox(registry, {kNaN, 0}, 10, 10);
    MakeBox(registry, {5, 0}, 10, 10);

    registry.Update();
    auto &system = registry.GetSystem<ContactSystem>();

    Logger::messages.clear();
    OnFreshDiagnosticBudget([&] { system.Update(); });
    Assert::That(system.GetContacts().size(), Equals(0u));
    Logger::messages.clear();

    moving.GetComponent<TransformComponent>().position = glm::vec2(0, 0);
    system.Update();

    Assert::That(system.GetContacts().size(), Equals(1u));
  }
};
