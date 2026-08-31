#pragma once

#include "logger.h"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

constexpr unsigned int MAX_COMPONENTS = 32;

//////////////////////////////////////////
// Signature
/////////////////////////////////////////////
// We use a bitset (1s and 0s ) to keep track of which components and entity
// has, and also helps keep track of which entities a system is interested in.
/////////////////////////////////////////////
using Signature = std::bitset<MAX_COMPONENTS>;

// Budget for the ECS diagnostic paths. A component miss inside a system runs
// once per entity per frame, and Logger::Err costs a localtime() call plus a
// console write, so an ungated report there is dozens of writes a second.
// Every ECS diagnostic reports its first few occurrences, then goes quiet.
constexpr unsigned int ECS_MAX_DIAGNOSTIC_REPORTS = 4;

// True for the first ECS_MAX_DIAGNOSTIC_REPORTS calls made against `counter`.
// Gate a diagnostic on this *before* building its message — assembling the
// message is most of the cost. `counter` must be a static owned by a single
// call site, so that distinct failures throttle independently.
inline bool EcsShouldReport(unsigned int &counter) {
  if (counter >= ECS_MAX_DIAGNOSTIC_REPORTS) {
    return false;
  }
  ++counter;
  return true;
}

// Suffix explaining that a throttled diagnostic has just gone quiet; empty
// while `counter` still has budget left.
const char *EcsSuppressionNote(unsigned int counter);

// Error-level log for the ECS call sites that have no Registry to log through
// (System::RequireComponent and the bare-Entity forwarders).
void EcsReportErr(const std::string &message);

// std::bitset::set/test throw std::out_of_range for a position past
// MAX_COMPONENTS, and these templates are instantiated inside the *game's*
// translation unit, which may be built -fno-exceptions (the Switch build).
// Range-check the id first so the throw is unreachable. Returns false, and
// logs through `counter`'s budget, when the id is out of range.
bool EcsComponentIdIsValid(std::size_t componentId, const char *where,
                           unsigned int &counter);

// Why a component lookup failed. Deliberately not part of any public
// accessor's signature — it exists only so that GetComponent's throttled
// diagnostic can name the reason while the bounds checks live in one place.
enum class ComponentMiss : unsigned char {
  None = 0,
  // The handle's id is no longer held by the entity that created it — either
  // the id was recycled to a different entity after this one was killed, or
  // the handle was hand-built and never valid to begin with. Checked first:
  // without it, a stale handle whose id a live entity now holds would pass
  // every check below and read that entity's component instead of missing.
  Stale,
  TooManyTypes,
  NoPool,
  OutOfRange,
  NotOwned,
  Count
};

const char *ComponentMissDescription(ComponentMiss miss);

// One default-constructed instance per component type per thread, reset on
// every call. It backs the reference-returning accessors when there is no
// component to return, so a miss reads zeroes instead of out-of-bounds
// memory. Two references handed out by two misses still alias each other —
// use Registry::TryGetComponent whenever a miss is possible.
template <typename TComponent> TComponent &EcsFallbackComponent() {
  static thread_local TComponent fallback;
  // Components with a const or reference member, an atomic, a mutex, or a
  // deleted assignment are not assignable. Resetting unconditionally would
  // make GetComponent<T> fail to compile for them, which is an API break on
  // the most-called template in the engine. Those types keep a fallback that
  // is not re-zeroed per miss — exactly the previous behaviour, no worse.
  if constexpr (std::is_move_assignable<TComponent>::value) {
    fallback = TComponent{};
  }
  return fallback;
}

struct IComponent {
protected:
  static std::size_t nextId;
};

// Used to assign a unique id to a component type
template <typename T> class Component : public IComponent {
public:
  // Returns the unique id of Component<T>
  static std::size_t GetId() {
    static auto id = nextId++;
    return id;
  }
};

// Entity is a value type copied into every system list, set, and sort — keep
// it lean (an id, a generation, and a registry pointer, nothing else).
class Entity {
private:
  std::size_t id;
  // 0 is reserved and never valid: Registry::generations starts at 1, so an
  // Entity built from a bare id is stale by construction and cannot be
  // mistaken for a live entity.
  std::uint32_t generation = 0;

  friend class Registry;

public:
  // explicit since 2.0.0: without it any function taking an Entity silently
  // accepted a bare number, so registry.KillEntity(88) compiled.
  explicit Entity(std::size_t id) : id(id){};
  void Kill();
  std::size_t GetId() const;
  std::uint32_t GetGeneration() const { return generation; }

  // Manage entity tags and groups
  void Tag(const std::string &tag);
  bool HasTag(const std::string &tag) const;
  void Group(const std::string &group);
  bool BelongsToGroup(const std::string &group) const;

  Entity &operator=(const Entity &other) = default;
  bool operator==(const Entity &other) const {
    return id == other.id && generation == other.generation;
  };
  bool operator!=(const Entity &other) const { return !(*this == other); };

  template <typename TComponent, typename... TArgs>
  void AddComponent(TArgs &&... args);

  template <typename TComponent> void RemoveComponent();

  template <typename TComponent> bool HasComponent() const;

  // Returns nullptr when this entity has no TComponent (or has no registry).
  // Prefer this over GetComponent wherever a miss is possible.
  template <typename TComponent> TComponent *TryGetComponent() const;

  template <typename TComponent> TComponent &GetComponent() const;

  // Hold a pointer to the entity's owner registry
  class Registry *registry = nullptr; // Be careful for cyclic dependencies
};

// Ordering for the containers that need one. Named for what it orders: this
// is not "by id", and calling it that is how the deleted operator< gets
// quietly reintroduced.
struct EntityOrder {
  bool operator()(const Entity &a, const Entity &b) const {
    if (a.GetId() != b.GetId()) {
      return a.GetId() < b.GetId();
    }
    return a.GetGeneration() < b.GetGeneration();
  }
};

/*******************************************/
// System
/*******************************************/
// The system process entities that contains a specific signature
/*******************************************/
class System {
private:
  Signature componentSignature;
  using EntitiesContainer = std::vector<Entity>;
  EntitiesContainer entities;

protected:
  void
  sortEntities(std::function<bool(const Entity &, const Entity &)> &&lambda);

public:
  System() = default;
  ~System() = default;

  void AddEntityToSystem(Entity entity);
  void RemoveEntityFromSystem(Entity entity);
  std::vector<Entity> &GetSystemEntities();
  const Signature &GetComponentSignature() const;

  // Define the component type T that entities must have to be
  // considered by the system
  template <typename TComponent> void RequireComponent();
};

template <typename TComponent> void System::RequireComponent() {
  const auto componentId = Component<TComponent>::GetId();

  static thread_local unsigned int reports = 0;
  if (!EcsComponentIdIsValid(componentId, "System::RequireComponent",
                             reports)) {
    return; // the requirement is dropped rather than throwing out of a
            // -fno-exceptions translation unit
  }

  // operator[] rather than set(pos): set/test carry an out_of_range throw
  // that would be emitted into a -fno-exceptions game TU. The range check
  // above is the bounds check.
  componentSignature[componentId] = true;
}

///////////////////////////////////////////////////
// Pool
////////////////////////////////////////////////////
// A pool is just a vector (contiguos data) of objects of type T
////////////////////////////////////////////////////
class IPool {
public:
  virtual ~IPool() {}
};

template <typename T> class Pool : public IPool {
private:
  std::vector<T> data;

public:
  Pool(int size = 100) { data.resize(size); }

  virtual ~Pool() = default;

  bool isEmpty() const { return data.empty(); }

  std::size_t GetSize() const { return data.size(); }

  void Resize(int n) { data.resize(n); }

  void Clear() { data.clear(); }

  void Add(T object) { data.push_back(object); }

  void Set(std::size_t index, T object) { data[index] = object; }

  T &Get(std::size_t index) { return static_cast<T &>(data[index]); }

  T &operator[](std::size_t index) { return data[index]; }
};

///////////////////////////////////////////////////
// Registry
////////////////////////////////////////////////////
// The registry manaes the creation adn destruction of entities,
// add systems and components
////////////////////////////////////////////////////
class Registry {
private:
  std::size_t numEntities = 0;

  // Vector of component pools, each pool contains all the
  // data for a certain component type
  // Pool index = entity id
  std::vector<std::shared_ptr<IPool>> componentPools;

  // Vector of component signatures per entity,
  // saying which component is turned "on" for each entity
  // [ Vector index = entity id ]
  std::vector<Signature> entityComponentSignatures;

  // Generation per entity id, parallel to entityComponentSignatures. Starts
  // at 1 so that generation 0 can mean "never valid" — see Entity.
  std::vector<std::uint32_t> generations;

  std::unordered_map<std::type_index, std::shared_ptr<System>> systems;

  // Set of entities that are flagged to be added or removed the
  // next registry Update()
  std::set<Entity, EntityOrder> entitiesToBeAdded;
  std::set<Entity, EntityOrder> entitiesToBeKilled;

  // Entity tags (one tag name per entity)
  std::unordered_map<std::string, Entity> entityPerTag;

  // Entiy groups (a set of entities per group name)
  std::unordered_map<std::string, std::set<Entity, EntityOrder>>
      entitiesPerGroup;

  // List of free entity ids that were previously removed
  std::deque<int> freeIds;

  mutable Logger logger;
  static std::unique_ptr<Registry> instance;

  // Shared implementation of TryGetComponent and GetComponent, so the three
  // bounds checks exist once. Sets `miss` to the reason on failure.
  template <typename TComponent>
  TComponent *FindComponent(Entity entity, ComponentMiss &miss) const;

  // Whether `id` is currently held by any entity, independent of generation:
  // occupied means "in [0, numEntities) and not parked in freeIds". Named so
  // that "what does occupied mean" has one home. A liveness bitmap has been
  // floated to replace this scan (P49, tracked in docs/TECH_DEBT.md); an
  // unnamed, single-use reimplementation of occupancy is exactly what such a
  // change would miss.
  bool IsIdInUse(std::size_t id) const;

  // Shared implementation of CountEntitiesMissedBySystem and
  // AdmitExistingEntitiesTo: entities that are live, past admission, match
  // `system`'s signature, and are not already one of its members.
  //
  // A Registry member (not a free function) so it can stamp each candidate's
  // real, current generation before `visit` or the members-list comparison
  // below sees it — id occupancy and generation are both private state.
  // Handing `visit` a bare Entity(id) (generation 0) would make every == it
  // takes part in — the members-list scan, and any comparison a caller makes
  // afterward — read as a mismatch against the real, live entity holding
  // that id.
  template <typename TVisitor>
  void ForEachMissedEntity(const System &system, TVisitor &&visit) const;

public:
  Registry() { logger.Log("Registry constructor called"); }

  ~Registry();

  // The Registru Update finally process the entities that
  // are waiting to be added/killed
  void Update();

  // Entity management
  Entity CreateEntity();
  void KillEntity(Entity entity);

  // True while `entity`'s id is in use *and* its generation matches the
  // entity currently holding that id. A stale handle whose id has since been
  // recycled to a new entity reports false, not true: Entity carries a
  // generation stamped at creation and bumped when the id is freed, so the
  // two are never mistaken for each other. O(1): a generation compare, no
  // freeIds scan.
  bool IsAlive(Entity entity) const;

  // True while `entity` is queued for the next Registry::Update() and has not
  // yet been given to any system.
  bool IsPendingAdmission(Entity entity) const;

  // Names a registered system that `entity` would have joined had
  // `componentId` been present before Registry::Update() admitted it, or
  // nullptr when there is none. Call it *after* the signature bit is set.
  //
  // System membership is computed once, at admission, so a component added
  // afterwards never changes it (KNOWN_ISSUES.md item 5). This is how that
  // silent mistake is detected: replay the signature as the systems last saw
  // it and look for a requirement the new bit alone satisfies.
  //
  // The returned name comes from std::type_index::name() and is
  // implementation-mangled; it is for a log line, not for parsing.
  const char *SystemMissedByLateComponent(Entity entity,
                                          std::size_t componentId) const;

  // Component management
  template <typename TComponent, typename... Targs>
  void AddComponent(Entity entity, Targs &&... args);

  template <typename TComponent> void RemoveComponent(Entity entity);

  template <typename TComponent> bool HasComponent(Entity entity);

  // Returns a pointer to `entity`'s TComponent, or nullptr when there is
  // none: no pool for the type, an id out of range, or the signature bit
  // unset. Silent for those — a miss is a legitimate answer here, so this is
  // safe to call every frame, and it cannot alias. This is the correct
  // accessor whenever absence is possible.
  //
  // EXCEPTION: a stale handle (its id has since been recycled to a different
  // entity) is not a legitimate miss — it means the caller kept an Entity
  // past its death — so that one case logs (throttled) even here.
  template <typename TComponent>
  TComponent *TryGetComponent(Entity entity) const;

  // Returns `entity`'s TComponent by reference.
  //
  // PRECONDITION: the entity has the component. A reference cannot represent
  // absence, so on a miss this logs (throttled) and returns a freshly
  // default-constructed fallback instead of reading out of bounds.
  //
  // LIMITATION: that fallback is one object per component type per thread.
  // Resetting it on every miss stops one miss from reading back what an
  // earlier miss wrote, but two references obtained from two misses still
  // alias each other, and a write through one is visible through the other.
  // Use TryGetComponent when a miss is possible; this overload is only safe
  // when it is not.
  template <typename TComponent> TComponent &GetComponent(Entity entity) const;

  // (Registry::AddEntityToSystem was declared here with no definition
  // anywhere; calling it was a link error. AddEntityToSystems is the real
  // entry point.)

  // System Management
  template <typename TSystem, typename... Targs>
  void AddSystem(Targs &&... args);

  template <typename TSystem> void RemoveSystem();

  template <typename TSystem> bool HasSystem() const;

  // Prefer this over GetSystem wherever absence is possible: GetSystem calls
  // .at, which throws std::out_of_range — and aborts outright under the
  // Switch build's -fno-exceptions. The returned pointer is owned by the
  // registry and is invalidated by RemoveSystem<TSystem>() or the registry's
  // destruction.
  template <typename TSystem> TSystem *TryGetSystem() const;

  template <typename TSystem> TSystem &GetSystem() const;

  // How many live, already-admitted entities match `system`'s signature
  // without being members of it. Non-zero means the system was registered too
  // late to ever see them.
  std::size_t CountEntitiesMissedBySystem(const System &system) const;

  // Adds those entities to `system` and returns how many were added. Opt-in
  // and never automatic: a game may register a system late on purpose, and a
  // silent back-fill would change its behaviour.
  std::size_t AdmitExistingEntitiesTo(System &system);

  // AdmitExistingEntitiesTo for a system looked up by type. Returns 0 when
  // TSystem is not registered.
  template <typename TSystem> std::size_t AdmitExistingEntities();

  // Add and remove entities from their systems
  void AddEntityToSystems(Entity entity);
  void RemoveEntityFromSystems(Entity entity);

  // Tag Management
  void TagEntity(Entity entity, const std::string &tag);
  bool EntityHasTag(Entity entity, const std::string &tag) const;
  bool DoesTagExist(const std::string &tag) const;
  // PRECONDITION: DoesTagExist(tag). Entity has no "none" value, so this
  // cannot report a miss through its return type — guard the call.
  Entity GetEntityByTag(const std::string &tag) const;

  // The guarded lookup in one call. Returns nullptr when no entity holds the
  // tag, where GetEntityByTag has a precondition and throws.
  //
  // The pointer aliases the registry's tag map: it is invalidated by TagEntity,
  // RemoveEntityTag, and by the Update() that reaps a killed entity. Read it
  // and let it go; do not store it across a frame.
  const Entity *TryGetEntityByTag(const std::string &tag) const;

  void RemoveEntityTag(Entity entity);

  // Group Management
  void GroupEntity(Entity entity, const std::string &group);
  bool EntityBelongsToGroup(Entity entity, const std::string &group) const;
  std::vector<Entity> GetEntitiesByGroup(const std::string &group) const;
  bool DoesGroupExist(const std::string &group) const;
  void RemoveEntityGroup(Entity entity);

  // The entities queued for reaping at the next Update(). Order is
  // unspecified.
  std::vector<Entity> GetEntitiesToBeKilled() const;

  static Registry &Instance();
};

template <typename TSystem, typename... Targs>
void Registry::AddSystem(Targs &&... args) {
  std::shared_ptr<TSystem> newSystem =
      std::make_shared<TSystem>(std::forward<Targs>(args)...);
  const auto result = systems.insert(
      std::make_pair(std::type_index(typeid(TSystem)), newSystem));
  if (!result.second) {
    // Duplicate registration: unordered_map::insert is a no-op when the key
    // already exists, so `newSystem` above was never stored — the system
    // registered earlier is still the one in `systems`, untouched, and it
    // already has every matching entity as a member. Returning here (rather
    // than falling through) matters: CountEntitiesMissedBySystem below would
    // otherwise run against this stray, never-stored `newSystem`, whose
    // member list is empty, and report every live matching entity as
    // "missed" even though the real system already sees them — a false
    // positive the late-registration diagnostic exists specifically to
    // avoid.
    //
    // Silent, not its own diagnostic: a second AddSystem<T>() changes
    // nothing observable — the first registration keeps running exactly as
    // it was — so there is no state for a caller to have gotten wrong here,
    // unlike the late-registration case where entities really are left
    // stranded. The rest of Registry treats absence/duplication as
    // routine rather than newsworthy (RemoveSystem<T> on an unregistered
    // system, HasSystem<T> both no-op silently); adding a log here would be
    // the odd one out, and risks flagging a deliberately idempotent
    // AddSystem<T>() call in setup code as if it were a mistake.
    return;
  }

  // TSystem's constructor has run its RequireComponent calls by now, so the
  // signature is final and the scan is meaningful.
  static thread_local unsigned int lateSystemReports = 0;
  if (lateSystemReports < ECS_MAX_DIAGNOSTIC_REPORTS) {
    const std::size_t missed = CountEntitiesMissedBySystem(*newSystem);
    if (missed > 0 && EcsShouldReport(lateSystemReports)) {
      logger.Err("AddSystem: '" + std::string(typeid(TSystem).name()) +
                 "' was registered after " + std::to_string(missed) +
                 " matching entities were already admitted; it will never see "
                 "them. Register every system before creating entities, or "
                 "call Registry::AdmitExistingEntities<T>()." +
                 EcsSuppressionNote(lateSystemReports));
    }
  }
}

template <typename TSystem> void Registry::RemoveSystem() {
  auto system = systems.find(std::type_index(typeid(TSystem)));
  if (system != systems.end()) { // erasing end() is UB — no-op when absent
    systems.erase(system);
  }
}

template <typename TSystem> bool Registry::HasSystem() const {
  return systems.find(std::type_index(typeid(TSystem))) != systems.end();
}

template <typename TSystem> TSystem *Registry::TryGetSystem() const {
  auto found = systems.find(std::type_index(typeid(TSystem)));
  if (found == systems.end()) {
    return nullptr;
  }
  return std::static_pointer_cast<TSystem>(found->second).get();
}

template <typename TSystem> TSystem &Registry::GetSystem() const {
  auto found = systems.find(std::type_index(typeid(TSystem)));
  if (found != systems.end()) {
    return *(std::static_pointer_cast<TSystem>(found->second));
  }

  // .at is about to throw, and under -fno-exceptions that is an abort with
  // no message at all. Say which system first.
  static thread_local unsigned int reports = 0;
  if (EcsShouldReport(reports)) {
    logger.Err("GetSystem: system '" + std::string(typeid(TSystem).name()) +
               "' was never registered; this call is about to throw. Use "
               "TryGetSystem or HasSystem where absence is possible." +
               EcsSuppressionNote(reports));
  }
  // .at throws for a missing system — defined behavior instead of the UB of
  // dereferencing end(). Check HasSystem() first if absence is expected.
  return *(std::static_pointer_cast<TSystem>(
      systems.at(std::type_index(typeid(TSystem)))));
}

template <typename TSystem> std::size_t Registry::AdmitExistingEntities() {
  auto found = systems.find(std::type_index(typeid(TSystem)));
  if (found == systems.end()) {
    return 0;
  }
  return AdmitExistingEntitiesTo(*found->second);
}

template <typename TComponent, typename... Targs>
void Registry::AddComponent(Entity entity, Targs &&... args) {
  const auto componentId = Component<TComponent>::GetId();
  const auto entityId = entity.GetId();

  static thread_local unsigned int idReports = 0;
  if (!EcsComponentIdIsValid(componentId, "AddComponent", idReports)) {
    return;
  }

  if (entityId >= entityComponentSignatures.size()) {
    static thread_local unsigned int rangeReports = 0;
    if (EcsShouldReport(rangeReports)) {
      logger.Err("AddComponent: entity " + std::to_string(entityId) +
                 " is out of range; ignoring" +
                 EcsSuppressionNote(rangeReports));
    }
    return;
  }

  if (componentId >= componentPools.size()) {
    componentPools.resize(componentId + 1, nullptr);
  }

  if (!componentPools[componentId]) {
    std::shared_ptr<Pool<TComponent>> newComponentPool =
        std::make_shared<Pool<TComponent>>();
    componentPools[componentId] = newComponentPool;
  }

  std::shared_ptr<Pool<TComponent>> componentPool =
      std::static_pointer_cast<Pool<TComponent>>(componentPools[componentId]);

  if (entityId >= componentPool->GetSize()) {
    // Grow geometrically — resizing to exactly-n on every new entity would
    // re-copy the whole pool each time (O(n²) over n entities).
    std::size_t newSize = std::max(entityId + 1, componentPool->GetSize() * 2);
    componentPool->Resize(newSize);
  }

  TComponent newComponent(std::forward<Targs>(args)...);

  componentPool->Set(entityId, newComponent);
  entityComponentSignatures[entityId][componentId] = true; // see the note
                                                           // in
                                                           // RequireComponent

  logger.Log("Component id = " + std::to_string(componentId) +
             " was added to entity id " + std::to_string(entityId));

  // Gate on the budget before the search — an exhausted diagnostic must cost
  // one comparison, not a scan of every registered system.
  static thread_local unsigned int lateReports = 0;
  if (lateReports < ECS_MAX_DIAGNOSTIC_REPORTS) {
    if (const char *missed =
            SystemMissedByLateComponent(entity, componentId)) {
      if (EcsShouldReport(lateReports)) {
        logger.Err(
            "AddComponent: entity " + std::to_string(entityId) +
            " was already admitted by Registry::Update(), so adding this "
            "component will not put it in system '" + std::string(missed) +
            "'. Add every component before the Update() that admits the "
            "entity, or kill it and create a replacement." +
            EcsSuppressionNote(lateReports));
      }
    }
  }
}

template <typename TComponent> void Registry::RemoveComponent(Entity entity) {
  const auto componentId = Component<TComponent>::GetId();
  const auto entityId = entity.GetId();

  static thread_local unsigned int idReports = 0;
  if (!EcsComponentIdIsValid(componentId, "RemoveComponent", idReports)) {
    return;
  }

  if (entityId >= entityComponentSignatures.size()) {
    static thread_local unsigned int rangeReports = 0;
    if (EcsShouldReport(rangeReports)) {
      logger.Err("RemoveComponent: entity " + std::to_string(entityId) +
                 " is out of range; ignoring" +
                 EcsSuppressionNote(rangeReports));
    }
    return;
  }

  entityComponentSignatures[entityId][componentId] = false;

  logger.Log("Component id = " + std::to_string(componentId) +
             " was removed from entity id " + std::to_string(entityId));
}

template <typename TComponent> bool Registry::HasComponent(Entity entity) {
  // Routed through FindComponent — the same shared implementation
  // TryGetComponent and GetComponent use — so the staleness check lives in
  // exactly one place. A separate hand-rolled check here, alongside
  // FindComponent's, is exactly the kind of pair of related checks that has
  // already drifted apart twice in this codebase.
  ComponentMiss miss = ComponentMiss::None;
  return FindComponent<TComponent>(entity, miss) != nullptr;
}

template <typename TComponent>
TComponent *Registry::FindComponent(Entity entity, ComponentMiss &miss) const {
  const auto componentId = Component<TComponent>::GetId();
  const auto entityId = entity.GetId();

  // A stale handle's id may since have been recycled to a different, live
  // entity — every check below is indexed by id alone, so without this one
  // a stale handle would pass all of them and read that entity's component.
  // Unlike the other misses, a stale access is never a routine, expected
  // outcome, so it is reported here directly rather than left to whichever
  // accessor called in — that way TryGetComponent and HasComponent name it
  // too, not just GetComponent's own throttled diagnostic below.
  if (!IsAlive(entity)) {
    miss = ComponentMiss::Stale;
    static thread_local unsigned int staleReports = 0;
    if (EcsShouldReport(staleReports)) {
      logger.Err("FindComponent: entity " + std::to_string(entityId) + " " +
                 ComponentMissDescription(ComponentMiss::Stale) +
                 " for component type '" + typeid(TComponent).name() + "'" +
                 EcsSuppressionNote(staleReports));
    }
    return nullptr;
  }

  // Checked before any bitset::test below — past MAX_COMPONENTS that call
  // throws, and this template compiles into the game's TU.
  if (componentId >= MAX_COMPONENTS) {
    miss = ComponentMiss::TooManyTypes;
    return nullptr;
  }

  if (componentId >= componentPools.size() || !componentPools[componentId]) {
    miss = ComponentMiss::NoPool;
    return nullptr;
  }

  auto componentPool =
      std::static_pointer_cast<Pool<TComponent>>(componentPools[componentId]);

  if (entityId >= entityComponentSignatures.size() ||
      entityId >= componentPool->GetSize()) {
    miss = ComponentMiss::OutOfRange;
    return nullptr;
  }

  if (!entityComponentSignatures[entityId][componentId]) {
    miss = ComponentMiss::NotOwned;
    return nullptr;
  }

  miss = ComponentMiss::None;
  return &componentPool->Get(entityId);
}

template <typename TComponent>
TComponent *Registry::TryGetComponent(Entity entity) const {
  ComponentMiss miss = ComponentMiss::None;
  return FindComponent<TComponent>(entity, miss);
}

template <typename TComponent>
TComponent &Registry::GetComponent(Entity entity) const {
  ComponentMiss miss = ComponentMiss::None;
  TComponent *component = FindComponent<TComponent>(entity, miss);
  if (component != nullptr) {
    return *component;
  }

  // Stale is reported directly by FindComponent, unconditionally — logging
  // it again here would double the report for this one reason alone.
  if (miss != ComponentMiss::Stale) {
    // One budget per (component type, reason): the counters are static
    // inside a template instantiated per TComponent, so a game that misses
    // this lookup every frame logs a handful of lines and then stays
    // silent.
    static thread_local unsigned int
        reports[static_cast<std::size_t>(ComponentMiss::Count)] = {};
    unsigned int &counter = reports[static_cast<std::size_t>(miss)];
    if (EcsShouldReport(counter)) {
      logger.Err("GetComponent: entity " + std::to_string(entity.GetId()) +
                 " " + ComponentMissDescription(miss) +
                 " for component type '" + typeid(TComponent).name() +
                 "'; returning a default component" +
                 EcsSuppressionNote(counter));
    }
  }

  return EcsFallbackComponent<TComponent>();
}

// An Entity built directly (Entity(88)) rather than handed out by
// Registry::CreateEntity has a null registry pointer. Every forwarder below
// checks it: a bare handle is a caller mistake, but dereferencing null turns
// that mistake into a segfault with no diagnostic.
template <typename TComponent, typename... TArgs>
void Entity::AddComponent(TArgs &&... args) {
  if (registry == nullptr) {
    static thread_local unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr("Entity::AddComponent: entity " + std::to_string(id) +
                   " has no registry; ignoring" + EcsSuppressionNote(reports));
    }
    return;
  }
  registry->AddComponent<TComponent>(*this, std::forward<TArgs>(args)...);
}

template <typename TComponent> void Entity::RemoveComponent() {
  if (registry == nullptr) {
    static thread_local unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr("Entity::RemoveComponent: entity " + std::to_string(id) +
                   " has no registry; ignoring" + EcsSuppressionNote(reports));
    }
    return;
  }
  registry->RemoveComponent<TComponent>(*this);
}

template <typename TComponent> bool Entity::HasComponent() const {
  if (registry == nullptr) {
    return false;
  }
  return registry->HasComponent<TComponent>(*this);
}

template <typename TComponent> TComponent *Entity::TryGetComponent() const {
  if (registry == nullptr) {
    return nullptr;
  }
  return registry->TryGetComponent<TComponent>(*this);
}

template <typename TComponent> TComponent &Entity::GetComponent() const {
  if (registry == nullptr) {
    static thread_local unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr("Entity::GetComponent: entity " + std::to_string(id) +
                   " has no registry; returning a default component" +
                   EcsSuppressionNote(reports));
    }
    return EcsFallbackComponent<TComponent>();
  }
  return registry->GetComponent<TComponent>(*this);
}
