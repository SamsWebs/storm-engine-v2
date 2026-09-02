#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "../components/boxCollider.h"
#include "../components/circleCollider.h"
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

  // Membership is TransformComponent ALONE, and that is not an oversight.
  //
  // A signature is an AND of required components, so there is no way to say
  // "transform, plus a box collider OR a circle collider" -- and a system that
  // required both collider types would match neither kind of body. Splitting
  // circles into a second system does not work either: a box/circle pair has
  // one side in each, and neither system can see the other's entities.
  //
  // So the requirement is widened to the one component both shapes need, and
  // Update() narrows to the entities that actually carry a collider. The cost
  // is a scan over every transform entity per frame -- for a game with
  // thousands of sprites and a handful of colliders, that scan is new work
  // that 2.1.x did not do. The output is unchanged: an entity with no collider
  // produces no contact, exactly as when it was not a member at all.
  //
  // GetSystemEntities() therefore reports transform entities, not colliders,
  // which is the one observable break. Read GetContacts() instead.
  ContactSystem() { RequireComponent<TransformComponent>(); }

  // Recomputes this frame's contacts and fires the begin/end callbacks.
  // Safe to call more than once per frame; the second call reports the same
  // contacts and fires nothing new.
  void Update() {
    contacts.clear();

    std::vector<Entity> &members = GetSystemEntities();

    // Narrow the membership to entities that actually carry a collider, and
    // resolve each one to world space once. Everything downstream indexes
    // `entities` and `shapes` together.
    std::vector<Entity> entities;
    std::vector<Collider> shapes;
    entities.reserve(members.size());
    shapes.reserve(members.size());

    for (const Entity &member : members) {
      const TransformComponent *transform =
          member.TryGetComponent<TransformComponent>();
      // Required, so present for every member -- unless the game removed it
      // from a live entity, which does not revoke membership.
      if (!transform)
        continue;

      // A box wins when an entity carries both. That combination is a game
      // bug, since the two shapes disagree about where the body is, but it has
      // to resolve the same way on every frame and in every build; preferring
      // the box is what leaves every pre-existing entity behaving identically.
      if (const BoxColliderComponent *box =
              member.TryGetComponent<BoxColliderComponent>()) {
        entities.push_back(member);
        shapes.push_back(Collider::FromBox(BoundsOf(*transform, *box)));
        continue;
      }
      if (const CircleColliderComponent *circle =
              member.TryGetComponent<CircleColliderComponent>()) {
        entities.push_back(member);
        shapes.push_back(Collider::FromCircle(CircleOf(*transform, *circle)));
      }
    }

    const std::size_t count = entities.size();

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
              [&shapes](std::size_t left, std::size_t right) {
                return shapes[left].bounds.minX < shapes[right].bounds.minX;
              });

    for (std::size_t i = 0; i < count; ++i) {
      const std::size_t first = order[i];
      for (std::size_t j = i + 1; j < count; ++j) {
        const std::size_t second = order[j];
        if (shapes[second].bounds.minX >= shapes[first].bounds.maxX)
          break;
        // Broadphase only. For a circle this box is a proxy that is wrong at
        // the corners -- deliberately conservative, since a circle contact
        // always implies its bounding box overlaps, so nothing real is
        // rejected here and the exact solver runs below.
        if (!storm::Overlaps(shapes[first].bounds, shapes[second].bounds))
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
        if (!ManifoldOf(shapes[low], shapes[high], contact.normal,
                        contact.depth))
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
      // Members, not the narrowed collider list. The guard below exists to
      // drop pairs whose entity DIED; an entity that merely had its collider
      // removed is still alive and its separation is real, so it should get
      // its end callback rather than being silently swallowed.
      std::vector<Entity> live(members.begin(), members.end());
      std::sort(live.begin(), live.end(), EntityOrder{});

      for (const PairKey &key : previous) {
        if (std::binary_search(current.begin(), current.end(), key,
                               PairKeyOrder{}))
          continue;

        // A killed entity has no meaningful "end", and its id may already
        // have been recycled onto a different, live entity by the time this
        // runs: Registry::Update() pushes a killed entity's id onto
        // `freeIds` in the same pass that drops it from this system, and the next CreateEntity() can hand that id straight
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
  // units scaled by the transform. Scaling the offset too would be more
  // consistent but would silently move every existing collider; that was ruled
  // out of scope for 2.0.0 and is deferred to a later breaking release.
  //
  // RenderColliderSystem calls this rather than repeating it, so the debug
  // outline and the swept shape cannot drift apart. That used to be a comment
  // pointing at a second copy of the arithmetic, which is exactly the
  // arrangement that lets two "identical" formulas disagree.
  static ContactAABB BoundsOf(const TransformComponent &transform,
                              const BoxColliderComponent &collider) {
    ContactAABB box;
    box.minX = transform.position.x + collider.offset.x;
    box.minY = transform.position.y + collider.offset.y;
    box.maxX = box.minX + collider.width * transform.scale.x;
    box.maxY = box.minY + collider.height * transform.scale.y;
    return box;
  }

  // PRECONDITION: the entity has both components. It reads them with
  // GetComponent, which hands back a shared zeroed fallback on a miss rather
  // than reporting one.
  static ContactAABB BoundsOf(const Entity &entity) {
    return BoundsOf(entity.GetComponent<TransformComponent>(),
                    entity.GetComponent<BoxColliderComponent>());
  }

  // World-space circle. `offset` is world pixels and unscaled, matching
  // BoundsOf; `radius` is scaled, matching a box's extents.
  //
  // A non-uniform scale has no correct answer here -- scaling a circle by
  // (1, 2) is an ellipse, and this shape cannot be one. The LARGER absolute
  // axis wins, so a body is never quietly smaller than the sprite it stands
  // for and cannot slip through something it visibly overlaps; picking x would
  // silently discard a deliberate y scale instead. `std::abs` because a radius
  // is a length: a mirrored sprite (scale.x = -1) keeps the collider it had,
  // where a signed multiply would hand the solvers a negative radius to clamp.
  //
  // If you need an ellipse, model the body as a box, or as two circles.
  static ContactCircle CircleOf(const TransformComponent &transform,
                                const CircleColliderComponent &collider) {
    ContactCircle circle;
    circle.x = transform.position.x + collider.offset.x;
    circle.y = transform.position.y + collider.offset.y;
    circle.radius =
        collider.radius * std::max(std::abs(transform.scale.x),
                                   std::abs(transform.scale.y));
    return circle;
  }

  // PRECONDITION: the entity has both components, same as BoundsOf.
  static ContactCircle CircleOf(const Entity &entity) {
    return CircleOf(entity.GetComponent<TransformComponent>(),
                    entity.GetComponent<CircleColliderComponent>());
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
  // It is a minimum translation only when `depth` is a true separation
  // distance, which for box vs box it is NOT whenever one box is contained
  // within the other along the chosen axis -- the overlap is then the inner
  // box's own extent, not the distance to a face. See the note on
  // storm::MinimumTranslation in <stormengine2/collision/shapes.h>.
  //
  // The system does not apply it: there is no scheduler, so resolution order is
  // the game's call.
  static glm::vec2 MinimumTranslation(const Contact &contact) {
    return storm::MinimumTranslation(contact.normal, contact.depth);
  }

private:
  // One collider resolved to world space for this frame, alongside the
  // broadphase box the sweep sorts and prunes on. For a box shape `bounds` IS
  // the shape -- which is why a box/box pair's broadphase test is already
  // exact, and the narrowphase then recomputes the same overlap to get its
  // normal, exactly as it did before circles existed.
  struct Collider {
    ContactAABB bounds;
    ContactCircle circle;
    bool isCircle = false;

    static Collider FromBox(const ContactAABB &box) {
      Collider collider;
      collider.bounds = box;
      return collider;
    }

    static Collider FromCircle(const ContactCircle &circle) {
      Collider collider;
      // Qualified: ContactSystem::BoundsOf would otherwise hide the free
      // function that takes a circle.
      collider.bounds = storm::BoundsOf(circle);
      collider.circle = circle;
      collider.isCircle = true;
      return collider;
    }
  };

  // Dispatches to the one solver for this shape pair. The normal comes back
  // pointing from `a` toward `b` in every branch, including the mixed ones --
  // storm::Manifold has an AABB-first overload that negates for exactly this,
  // so the caller never has to remember which side was round.
  static bool ManifoldOf(const Collider &a, const Collider &b,
                         glm::vec2 &normal, float &depth) {
    if (a.isCircle && b.isCircle)
      return storm::Manifold(a.circle, b.circle, normal, depth);
    if (a.isCircle)
      return storm::Manifold(a.circle, b.bounds, normal, depth);
    if (b.isCircle)
      return storm::Manifold(a.bounds, b.circle, normal, depth);
    return storm::Manifold(a.bounds, b.bounds, normal, depth);
  }

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
