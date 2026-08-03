#pragma once

#include "logger.h"

#include <algorithm>
#include <bitset>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <type_traits>
#include <typeindex>
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
// it lean (an id + registry pointer, nothing else).
class Entity {
private:
  std::size_t id;

public:
  Entity(std::size_t id) : id(id){};
  void Kill();
  std::size_t GetId() const;

  // Manage entity tags and groups
  void Tag(const std::string &tag);
  bool HasTag(const std::string &tag) const;
  void Group(const std::string &group);
  bool BelongsToGroup(const std::string &group) const;

  Entity &operator=(const Entity &other) = default;
  bool operator==(const Entity &other) const { return id == other.id; };
  bool operator!=(const Entity &other) const { return id != other.id; };
  bool operator>(const Entity &other) const { return id > other.id; };
  bool operator<(const Entity &other) const { return id < other.id; };

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

  std::unordered_map<std::type_index, std::shared_ptr<System>> systems;

  // Set of entities that are flagged to be added or removed the
  // next registry Update()
  std::set<Entity> entitiesToBeAdded;
  std::set<Entity> entitiesToBeKilled;

  // Entity tags (one tag name per entity)
  std::unordered_map<std::string, Entity> entityPerTag;
  std::unordered_map<int, std::string>
      tagPerEntity; // Int is used to go by ID #

  // Entiy groups (a set of entities per group name)
  std::unordered_map<std::string, std::set<Entity>> entitiesPerGroup;
  std::unordered_map<int, std::string> groupPerEntity;
  ;

  // List of free entity ids that were previously removed
  std::deque<int> freeIds;

  mutable Logger logger;
  static std::unique_ptr<Registry> instance;

  // Shared implementation of TryGetComponent and GetComponent, so the three
  // bounds checks exist once. Sets `miss` to the reason on failure.
  template <typename TComponent>
  TComponent *FindComponent(Entity entity, ComponentMiss &miss) const;

public:
  Registry() { logger.Log("Registry constructor called"); }

  ~Registry() { logger.Log("Registry destructor called."); }

  // The Registru Update finally process the entities that
  // are waiting to be added/killed
  void Update();

  // Entity management
  Entity CreateEntity();
  void KillEntity(Entity entity);

  // True while `entity`'s id is in use. NOTE: ids are recycled, so a stale
  // handle whose id has since been handed to a new entity reports alive —
  // Entity carries no generation counter to tell the two apart.
  //
  // Derived from existing state (id < numEntities, and not parked in freeIds)
  // rather than a liveness bitmap, so it costs a scan of freeIds. A bitmap
  // would make this O(1) but adds a data member to Registry, and games embed
  // `Registry` by value, so sizeof(Registry) is ABI. Tracked as P49.
  bool IsAlive(Entity entity) const;

  // Component management
  template <typename TComponent, typename... Targs>
  void AddComponent(Entity entity, Targs &&... args);

  template <typename TComponent> void RemoveComponent(Entity entity);

  template <typename TComponent> bool HasComponent(Entity entity);

  // Returns a pointer to `entity`'s TComponent, or nullptr when there is
  // none: no pool for the type, an id out of range, or the signature bit
  // unset. Silent — a miss is a legitimate answer here, so this is safe to
  // call every frame, and it cannot alias. This is the correct accessor
  // whenever absence is possible.
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
  void AddSystem(Targs &... args);

  template <typename TSystem> void RemoveSystem();

  template <typename TSystem> bool HasSystem() const;

  template <typename TSystem> TSystem &GetSystem() const;

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
  void RemoveEntityTag(Entity entity);

  // Group Management
  void GroupEntity(Entity entity, const std::string &group);
  bool EntityBelongsToGroup(Entity entity, const std::string &group) const;
  std::vector<Entity> GetEntitiesByGroup(const std::string &group) const;
  bool DoesGroupExist(const std::string &group) const;
  void RemoveEntityGroup(Entity entity);
  std::set<Entity> GetEntitiesToBeKilled() const { return entitiesToBeKilled; }

  static Registry &Instance();
};

template <typename TSystem, typename... Targs>
void Registry::AddSystem(Targs &... args) {
  std::shared_ptr<TSystem> newSystem =
      std::make_shared<TSystem>(std::forward<Targs>(args)...);
  systems.insert(std::make_pair(std::type_index(typeid(TSystem)), newSystem));
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

template <typename TSystem> TSystem &Registry::GetSystem() const {
  // .at throws for a missing system — defined behavior instead of the UB of
  // dereferencing end(). Check HasSystem() first if absence is expected.
  return *(std::static_pointer_cast<TSystem>(
      systems.at(std::type_index(typeid(TSystem)))));
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
  const auto componentId = Component<TComponent>::GetId();
  const auto entityId = entity.GetId();

  static thread_local unsigned int idReports = 0;
  if (!EcsComponentIdIsValid(componentId, "HasComponent", idReports)) {
    return false;
  }

  return (entityId < entityComponentSignatures.size()) &&
         entityComponentSignatures[entityId][componentId];
}

template <typename TComponent>
TComponent *Registry::FindComponent(Entity entity, ComponentMiss &miss) const {
  const auto componentId = Component<TComponent>::GetId();
  const auto entityId = entity.GetId();

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

  // One budget per (component type, reason): the counters are static inside a
  // template instantiated per TComponent, so a game that misses this lookup
  // every frame logs a handful of lines and then stays silent.
  static thread_local unsigned int
      reports[static_cast<std::size_t>(ComponentMiss::Count)] = {};
  unsigned int &counter = reports[static_cast<std::size_t>(miss)];
  if (EcsShouldReport(counter)) {
    logger.Err("GetComponent: entity " + std::to_string(entity.GetId()) + " " +
               ComponentMissDescription(miss) + " (component id " +
               std::to_string(Component<TComponent>::GetId()) +
               "); returning a default component" +
               EcsSuppressionNote(counter));
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
