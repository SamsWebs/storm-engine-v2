#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "../components/boxCollider.h"
#include "../components/transform.h"
#include "../collision/shapes.h"
#include "../ecs.h"

namespace storm {

// ContactSystem detects overlaps and reports them. It never kills an entity,
// never moves one, and never writes a component - the game decides what a
// contact means and in what order to respond, because the engine has no
// system scheduler.

// ContactAABB, ContactCircle and the math over them now live in
// <stormengine2/collision/shapes.h>, which includes glm and nothing else from
// the engine. They were always ECS-free; they did not look it, sitting as
// statics on a class deriving from System, and a consumer had to discover by
// experiment that they link without ecs.o. The types and every existing call
// are unchanged -- this header includes that one.

// One overlapping pair for the current frame. `a` always holds the lower
// entity id, so `normal` - a unit axis pointing from `a` toward `b` along the
// axis of least penetration - does not depend on iteration order. `depth` is
// the overlap on that axis and is always > 0.
struct Contact {
  Entity a;
  Entity b;
  glm::vec2 normal{0.0f, 0.0f};
  float depth = 0.0f;
};

class ContactSystem : public System {
public:
  using ContactCallback = std::function<void(const Contact &)>;
  using PairCallback = std::function<void(const Entity &, const Entity &)>;
  using PairFilter = std::function<bool(const Entity &, const Entity &)>;

  ContactSystem() {
    RequireComponent<TransformComponent>();
    RequireComponent<BoxColliderComponent>();
  }

  // Recomputes this frame's contacts and fires the begin/end callbacks.
  // Safe to call more than once per frame; the second call reports the same
  // contacts and fires nothing new.
  void Update() {
    contacts.clear();

    std::vector<Entity> &entities = GetSystemEntities();
    const std::size_t count = entities.size();

    std::vector<ContactAABB> bounds;
    bounds.reserve(count);
    for (const Entity &entity : entities)
      bounds.push_back(BoundsOf(entity));

    // Sweep and prune along X: visit entities in ascending minX so the inner
    // loop stops as soon as a candidate starts at or past the current right
    // edge. `order` is a local permutation - the system's own entity vector
    // is left alone, since games read GetSystemEntities() directly.
    //
    // ponytail: X axis only. Everything stacked in one column degrades to the
    // old all-pairs cost; a uniform grid is the upgrade if that ever matters.
    std::vector<std::size_t> order(count);
    for (std::size_t i = 0; i < count; ++i)
      order[i] = i;
    std::sort(order.begin(), order.end(),
              [&bounds](std::size_t left, std::size_t right) {
                return bounds[left].minX < bounds[right].minX;
              });

    for (std::size_t i = 0; i < count; ++i) {
      const std::size_t first = order[i];
      for (std::size_t j = i + 1; j < count; ++j) {
        const std::size_t second = order[j];
        if (bounds[second].minX >= bounds[first].maxX)
          break;
        if (!Overlaps(bounds[first], bounds[second]))
          continue;

        // Normalise on entity id so the reported normal is stable no matter
        // which way the sweep happened to walk the pair.
        const bool inIdOrder =
            entities[first].GetId() <= entities[second].GetId();
        const std::size_t low = inIdOrder ? first : second;
        const std::size_t high = inIdOrder ? second : first;

        Contact contact{entities[low], entities[high], glm::vec2(0.0f, 0.0f),
                        0.0f};

        // Filter before the manifold, not after. The whole point of the filter
        // is to make a crowd of like things cheap - a volley of bullets pairs
        // with itself O(n^2) times - and computing the manifold first would do
        // the expensive half of the work for every pair it then discards.
        if (filter && !filter(contact.a, contact.b))
          continue;
        if (!Manifold(bounds[low], bounds[high], contact.normal, contact.depth))
          continue;

        contacts.push_back(contact);
      }
    }

    std::sort(contacts.begin(), contacts.end(),
              [](const Contact &left, const Contact &right) {
                if (left.a.GetId() != right.a.GetId())
                  return left.a.GetId() < right.a.GetId();
                return left.b.GetId() < right.b.GetId();
              });

    // Sorted ascending by construction, which is what the set differences
    // below rely on.
    std::vector<PairKey> current;
    current.reserve(contacts.size());
    for (const Contact &contact : contacts)
      current.emplace_back(contact.a, contact.b);

    if (onBegin) {
      for (std::size_t i = 0; i < contacts.size(); ++i)
        if (!std::binary_search(previous.begin(), previous.end(), current[i],
                                PairKeyOrder{}))
          onBegin(contacts[i]);
    }

    if (onEnd) {
      std::vector<Entity> live(entities.begin(), entities.end());
      std::sort(live.begin(), live.end(), EntityOrder{});

      for (const PairKey &key : previous) {
        if (std::binary_search(current.begin(), current.end(), key,
                               PairKeyOrder{}))
          continue;

        // A killed entity has no meaningful "end", and its id may already
        // have been recycled onto a different, live entity by the time this
        // runs: Registry::Update() (common/ecs.cpp:439) returns a killed
        // entity's id to the free list in the same pass that drops it from
        // this system, and the next CreateEntity() can hand that id straight
        // back out. `key` carries each side's generation alongside its id
        // (PairKey), and FindLive compares the full Entity - id and
        // generation together - via operator==, so a key naming a recycled
        // id misses here instead of matching the new occupant. Drop the pair
        // silently either way.
        const Entity *a = FindLive(live, key.a);
        const Entity *b = FindLive(live, key.b);
        if (a && b)
          onEnd(*a, *b);
      }
    }

    previous.swap(current);
  }

  // This frame's contacts, ascending by (a.id, b.id). Invalidated by the next
  // Update().
  const std::vector<Contact> &GetContacts() const { return contacts; }

  // Fired from inside Update(). Begin carries the manifold; end does not,
  // because the pair has already separated by the time it runs.
  void SetOnBeginContact(ContactCallback callback) {
    onBegin = std::move(callback);
  }
  void SetOnEndContact(PairCallback callback) { onEnd = std::move(callback); }

  // Return false to skip a pair entirely. This is where layers, masks and
  // sensors live - a sensor is just an entity the game's filter treats
  // differently, which costs no component slot and changes no layout.
  void SetPairFilter(PairFilter predicate) { filter = std::move(predicate); }

  // World-space bounds. The offset is world pixels and the extents are local
  // units scaled by the transform - the same convention as
  // RenderColliderSystem (../systems/renderCollider.h:22-26). Scaling the
  // offset too would be more consistent but would silently move every
  // existing collider; that was ruled out of scope for 2.0.0 and is
  // deferred to a later breaking release.
  static ContactAABB BoundsOf(const Entity &entity) {
    const TransformComponent &transform =
        entity.GetComponent<TransformComponent>();
    const BoxColliderComponent &collider =
        entity.GetComponent<BoxColliderComponent>();

    ContactAABB box;
    box.minX = transform.position.x + collider.offset.x;
    box.minY = transform.position.y + collider.offset.y;
    box.maxX = box.minX + collider.width * transform.scale.x;
    box.maxY = box.minY + collider.height * transform.scale.y;
    return box;
  }

  // Kept so `ContactSystem::Overlaps(a, b)` keeps compiling: that spelling is
  // already in use outside this repo. New code should call storm::Overlaps and
  // storm::Manifold from <stormengine2/collision/shapes.h> directly, which also
  // cover circles.
  static bool Overlaps(const ContactAABB &a, const ContactAABB &b) {
    return storm::Overlaps(a, b);
  }

  static bool Manifold(const ContactAABB &a, const ContactAABB &b,
                       glm::vec2 &normal, float &depth) {
    return storm::Manifold(a, b, normal, depth);
  }

  // The penetration vector: from `a` toward `b`, with a magnitude equal to how
  // deeply they overlap.
  //
  // The direction is the opposite of what the old comment here claimed. It said
  // "how far to move `a` ... to separate the pair", but the normal points from
  // `a` INTO `b` -- so applying this to `a` unchanged drives them together. It
  // separates when applied to `b`, or negated and applied to `a`.
  // specs/systems/contact.spec.cpp pinned the correct value throughout; only
  // the prose was wrong, which is the kind of error a game pays for at runtime.
  //
  // The system does not apply it: there is no scheduler, so resolution order is
  // the game's call.
  static glm::vec2 MinimumTranslation(const Contact &contact) {
    return storm::MinimumTranslation(contact.normal, contact.depth);
  }

private:
  // Carries both sides' generations alongside their ids, so a pair recorded
  // last frame can be told apart from a same-id pair that only looks like it
  // because one side's id was recycled in between. Entity already bundles id
  // and generation with a generation-aware operator==, so a key is just a
  // pair of Entity values, not a pair of raw ids.
  struct PairKey {
    Entity a;
    Entity b;
    PairKey(Entity a, Entity b) : a(a), b(b) {}
  };

  // Orders PairKey by (a's identity, then b's identity), each compared the
  // same way EntityOrder compares a single Entity: id first, generation to
  // break ties. Explicit, because Entity's operator< is deleted - see
  // EntityOrder's own comment.
  struct PairKeyOrder {
    bool operator()(const PairKey &left, const PairKey &right) const {
      if (!(left.a == right.a))
        return EntityOrder{}(left.a, right.a);
      return EntityOrder{}(left.b, right.b);
    }
  };

  // Finds the live entity with the same id AND generation as `entity`. `live`
  // holds at most one entity per id, so locating by id via lower_bound always
  // lands on the right candidate (or none); the generation-aware
  // Entity::operator== is what actually rejects a recycled id naming a
  // different, newer entity.
  static const Entity *FindLive(const std::vector<Entity> &live,
                                const Entity &entity) {
    auto found =
        std::lower_bound(live.begin(), live.end(), entity, EntityOrder{});
    if (found == live.end() || !(*found == entity))
      return nullptr;
    return &(*found);
  }

  std::vector<Contact> contacts;
  std::vector<PairKey> previous;
  ContactCallback onBegin;
  PairCallback onEnd;
  PairFilter filter;
};

} // namespace storm
