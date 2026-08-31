#include "ecs.h"

std::size_t IComponent::nextId = 0;
std::unique_ptr<Registry> Registry::instance = nullptr;

const char *EcsSuppressionNote(unsigned int counter) {
  return counter >= ECS_MAX_DIAGNOSTIC_REPORTS
             ? " (further identical reports suppressed)"
             : "";
}

void EcsReportErr(const std::string &message) {
  // Intentionally leaked. Registry::instance (below) is a namespace-scope
  // std::unique_ptr, so its ~Registry can run during static teardown, after
  // a function-local static Logger here would already have been destroyed.
  // Leaking it keeps EcsReportErr safe to call at any point in the program's
  // life, including from ~Registry at exit. Logger::messages itself is a
  // namespace-scope static in another translation unit, so its destruction
  // order relative to Registry::instance is unspecified — a report emitted
  // during static teardown is best-effort, not guaranteed to be observed.
  static Logger &ecsLogger = *new Logger();
  ecsLogger.Err(message);
}

bool EcsComponentIdIsValid(std::size_t componentId, const char *where,
                           unsigned int &counter) {
  if (componentId < MAX_COMPONENTS) {
    return true;
  }

  if (EcsShouldReport(counter)) {
    EcsReportErr(std::string(where) + ": component id " +
                 std::to_string(componentId) + " is past MAX_COMPONENTS (" +
                 std::to_string(MAX_COMPONENTS) +
                 "); this component type is ignored everywhere. Reduce the "
                 "number of distinct component types." +
                 EcsSuppressionNote(counter));
  }

  return false;
}

const char *ComponentMissDescription(ComponentMiss miss) {
  switch (miss) {
  case ComponentMiss::TooManyTypes:
    return "uses a component type past MAX_COMPONENTS";
  case ComponentMiss::NoPool:
    return "has no pool for the component type";
  case ComponentMiss::OutOfRange:
    return "is out of range for the component type";
  case ComponentMiss::NotOwned:
    return "does not have the component";
  default:
    return "has the component";
  }
}

std::size_t Entity::GetId() const { return id; }

// A bare Entity (one built as Entity(88) instead of handed out by
// Registry::CreateEntity) has a null registry pointer. Guard every forwarder:
// the call is a caller mistake either way, but an unguarded dereference turns
// it into a segfault with no diagnostic.
void Entity::Kill() {
  if (registry == nullptr) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr("Entity::Kill: entity " + std::to_string(id) +
                   " has no registry; ignoring" + EcsSuppressionNote(reports));
    }
    return;
  }
  registry->KillEntity(*this);
}

void Entity::Tag(const std::string &tag) {
  if (registry == nullptr) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr("Entity::Tag: entity " + std::to_string(id) +
                   " has no registry; ignoring" + EcsSuppressionNote(reports));
    }
    return;
  }
  registry->TagEntity(*this, tag);
}

bool Entity::HasTag(const std::string &tag) const {
  if (registry == nullptr) {
    return false;
  }
  return registry->EntityHasTag(*this, tag);
}

void Entity::Group(const std::string &group) {
  if (registry == nullptr) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      EcsReportErr("Entity::Group: entity " + std::to_string(id) +
                   " has no registry; ignoring" + EcsSuppressionNote(reports));
    }
    return;
  }
  registry->GroupEntity(*this, group);
}

bool Entity::BelongsToGroup(const std::string &group) const {
  if (registry == nullptr) {
    return false;
  }
  return registry->EntityBelongsToGroup(*this, group);
}

void System::AddEntityToSystem(Entity entity) { entities.push_back(entity); }

void System::RemoveEntityFromSystem(Entity entity) {
  entities.erase(
      std::remove_if(entities.begin(), entities.end(),
                     [&entity](Entity other) { return entity == other; }),
      entities.end());
}

std::vector<Entity> &System::GetSystemEntities() { return entities; }

void System::sortEntities(
    std::function<bool(const Entity &, const Entity &)> &&lambda) {
  std::sort(entities.begin(), entities.end(), lambda);
}

const Signature &System::GetComponentSignature() const {
  return componentSignature;
}

namespace {

// Per-Registry diagnostic state that cannot live on Registry itself: games
// embed a Registry by value in their states, so sizeof(Registry) is ABI and
// 1.x may not add a member to it. Keyed on `this` and erased by ~Registry.
//
// Not thread-safe, in keeping with the rest of the ECS.
struct RegistryDiagnostics {
  unsigned long updateCalls = 0;
  unsigned long entitiesCreated = 0;
};

std::unordered_map<const Registry *, RegistryDiagnostics> &
DiagnosticsTable() {
  // Intentionally leaked. ~Registry reaches into this map, and the editor's
  // Registry::Instance() singleton is destroyed during static teardown —
  // after a function-local static would already have been destroyed.
  // Leaking it makes the destructor safe at any point in the program's
  // life. One map, freed by the OS at exit.
  static auto &table =
      *new std::unordered_map<const Registry *, RegistryDiagnostics>();
  return table;
}

} // namespace

bool Registry::IsIdInUse(std::size_t id) const {
  if (id >= numEntities) {
    return false;
  }
  return std::find(freeIds.begin(), freeIds.end(),
                   static_cast<int>(id)) == freeIds.end();
}

// Occupancy is checked with IsIdInUse *before* the candidate is stamped with
// a generation. IsAlive cannot do this job: it requires the caller's
// generation to already match the id's current one, so stamping the current
// generation onto the candidate first — then asking IsAlive whether that
// same generation is current — would be trivially true even for an id that
// is presently sitting in freeIds.
//
// Once occupancy is confirmed, the candidate is stamped with its real,
// current generation (and left with a null `registry`) so it carries the
// live entity's actual identity into every == it takes part in below: the
// members-list scan, and anything IsPendingAdmission or a caller compares it
// against afterward. A bare Entity(id) (generation 0) would read as a
// mismatch against the real entity in every one of those.
//
// Anything that dereferences a component off a candidate must still stamp
// `entity.registry` first, as AdmitExistingEntitiesTo does. Skipping that
// stamp is silent — the null guards return a shared zeroed fallback rather
// than failing — and it shipped that way once already.
template <typename TVisitor>
void Registry::ForEachMissedEntity(const System &system,
                                   TVisitor &&visit) const {
  const Signature &required = system.GetComponentSignature();
  const std::vector<Entity> &members =
      const_cast<System &>(system).GetSystemEntities();

  for (std::size_t id = 0;
       id < numEntities && id < entityComponentSignatures.size(); ++id) {
    if (!IsIdInUse(id)) {
      continue;
    }

    Entity entity(id);
    entity.generation = generations[id];

    if (IsPendingAdmission(entity)) {
      continue;
    }
    if ((entityComponentSignatures[id] & required) != required) {
      continue;
    }
    if (std::find(members.begin(), members.end(), entity) != members.end()) {
      continue;
    }
    visit(entity);
  }
}

// Call-site-owned, like every other diagnostic throttle in this file —
// unlike updateCalls/entitiesCreated above, this counter must outlive any
// single registry, since ~Registry runs exactly once per registry and could
// never accumulate past 1 if it lived in the per-registry side table.
static thread_local unsigned int missingUpdateReports = 0;

Registry::~Registry() {
  // find, not operator[]: a registry that never called CreateEntity or
  // Update() has no entry in this table, and operator[] would insert one
  // just to immediately erase it below. With no entry, entitiesCreated reads
  // as 0 either way, so the guard's outcome is unchanged.
  auto found = DiagnosticsTable().find(this);
  if (found != DiagnosticsTable().end()) {
    const RegistryDiagnostics &diagnostics = found->second;
    if (diagnostics.updateCalls == 0 && diagnostics.entitiesCreated > 0 &&
        EcsShouldReport(missingUpdateReports)) {
      EcsReportErr(
          "~Registry: this registry created " +
          std::to_string(diagnostics.entitiesCreated) +
          " entities and Registry::Update() was never called on it, so none "
          "of them ever joined a system — nothing it owned rendered or "
          "moved. Call registry.Update() once per frame, first, in your "
          "state's update()." +
          EcsSuppressionNote(missingUpdateReports));
    }
    DiagnosticsTable().erase(found);
  }
  logger.Log("Registry destructor called.");
}

Entity Registry::CreateEntity() {
  std::size_t entityId;

  if (freeIds.empty()) {
    // if there are no free ids waiting to be reused
    entityId = numEntities++;
    if (entityId >= entityComponentSignatures.size()) {
      entityComponentSignatures.resize(entityId + 1);
    }
  } else {
    // Reuse and id from the list of previously remove entities
    entityId = freeIds.front();
    freeIds.pop_front();
  }

  if (entityId >= generations.size()) {
    generations.resize(entityId + 1, 1); // 1, not 0: 0 means never valid
  }

  Entity entity(entityId);
  entity.generation = generations[entityId];
  entity.registry = this;
  entitiesToBeAdded.insert(entity);

  // Counting only — see ~Registry for the diagnostic. A count-based report
  // here false-positives on batch-spawn-then-flush (a level loader), so the
  // actual misuse check happens at the registry's end of life instead.
  ++DiagnosticsTable()[this].entitiesCreated;

  logger.Log("Entity created with id = " + std::to_string(entityId));

  return entity;
}

bool Registry::IsAlive(Entity entity) const {
  const auto entityId = entity.GetId();
  if (entityId >= numEntities || entityId >= generations.size()) {
    return false;
  }
  return entity.generation == generations[entityId];
}

bool Registry::IsPendingAdmission(Entity entity) const {
  return entitiesToBeAdded.find(entity) != entitiesToBeAdded.end();
}

const char *
Registry::SystemMissedByLateComponent(Entity entity,
                                      std::size_t componentId) const {
  const auto entityId = entity.GetId();
  if (entityId >= entityComponentSignatures.size() ||
      componentId >= MAX_COMPONENTS) {
    return nullptr;
  }
  if (!IsAlive(entity)) {
    return nullptr;
  }
  // Still queued: Update() has not decided its membership yet, so adding a
  // component now is exactly the correct thing to do.
  if (IsPendingAdmission(entity)) {
    return nullptr;
  }
  // On its way out; its membership will never matter again.
  if (entitiesToBeKilled.find(entity) != entitiesToBeKilled.end()) {
    return nullptr;
  }

  const Signature &now = entityComponentSignatures[entityId];
  Signature asAdmitted = now;
  asAdmitted.reset(componentId);

  for (const auto &entry : systems) {
    const Signature &required = entry.second->GetComponentSignature();
    const bool matchedAtAdmission = (asAdmitted & required) == required;
    const bool matchesNow = (now & required) == required;
    if (matchedAtAdmission || !matchesNow) {
      continue;
    }
    // asAdmitted is a replay, not the recorded admission-time signature, so
    // it reads a bit that was already set before this call (a re-add of a
    // component the entity already had) as if it had just appeared. Guard
    // against that false positive with the one fact the system does record:
    // an entity Update() actually admitted is already in its entities list,
    // and nothing done after admission removes it from there.
    const auto &members = entry.second->GetSystemEntities();
    const bool alreadyMember =
        std::find(members.begin(), members.end(), entity) != members.end();
    if (!alreadyMember) {
      return entry.first.name();
    }
  }
  return nullptr;
}

std::size_t Registry::CountEntitiesMissedBySystem(const System &system) const {
  std::size_t missed = 0;
  ForEachMissedEntity(system, [&missed](Entity) { ++missed; });
  return missed;
}

std::size_t Registry::AdmitExistingEntitiesTo(System &system) {
  std::vector<Entity> toAdmit;
  ForEachMissedEntity(system,
                      [&toAdmit](Entity entity) { toAdmit.push_back(entity); });
  for (Entity entity : toAdmit) {
    // ForEachMissedEntity stamps each candidate's real generation but leaves
    // registry null. Stamp it before handing the entity to the system:
    // everything a system does with an Entity (GetComponent, Kill, Tag, ...)
    // routes through that pointer, and a null one silently poisons every
    // access instead of failing loudly. Entity::registry is public, so this
    // needs no friend declaration.
    entity.registry = this;
    system.AddEntityToSystem(entity);
  }
  return toAdmit.size();
}

void Registry::KillEntity(Entity entity) {
  const auto entityId = entity.GetId();

  // One liveness test covers every rejection this needs: an id that was never
  // created, an id already recycled into freeIds (the double-kill that used
  // to alias two live entities onto one id), and a stale handle whose id has
  // since been handed to a new entity — IsAlive checks the generation, so
  // that case no longer kills the new entity by mistake.
  if (!IsAlive(entity)) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      logger.Err("KillEntity: entity " + std::to_string(entityId) +
                 " is not alive (never created, or already killed); ignoring" +
                 EcsSuppressionNote(reports));
    }
    return;
  }

  if (entitiesToBeKilled.count(entity) > 0) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      logger.Err("KillEntity: entity " + std::to_string(entityId) +
                 " is already pending kill this frame; ignoring" +
                 EcsSuppressionNote(reports));
    }
    return;
  }

  entitiesToBeKilled.insert(entity);
}

void Registry::AddEntityToSystems(Entity entity) {
  const auto entityId = entity.GetId();

  if (entityId >= entityComponentSignatures.size()) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      logger.Err("AddEntityToSystems: entity " + std::to_string(entityId) +
                 " is out of range; ignoring" + EcsSuppressionNote(reports));
    }
    return;
  }

  const auto entityComponentSignature = entityComponentSignatures[entityId];

  // loop all the systems
  for (auto &system : systems) {
    const auto &systemComponentSignature =
        system.second->GetComponentSignature();

    bool isInterested = (entityComponentSignature & systemComponentSignature) ==
                        systemComponentSignature;
    if (isInterested) {
      system.second->AddEntityToSystem(entity);
    }
  }
}

void Registry::RemoveEntityFromSystems(Entity entity) {
  for (auto &system : systems) {
    system.second->RemoveEntityFromSystem(entity);
  }
}

void Registry::Update() {
  ++DiagnosticsTable()[this].updateCalls;

  // Add the entities that are waiting to be
  // created to the active systems
  for (auto entity : entitiesToBeAdded) {
    AddEntityToSystems(entity);
  }
  entitiesToBeAdded.clear();

  // Process the entities that are waiting to be
  // killed from the active systems
  for (auto entity : entitiesToBeKilled) {
    RemoveEntityFromSystems(entity);
    RemoveEntityGroup(entity);
    RemoveEntityTag(entity); // otherwise a recycled id inherits the stale tag

    entityComponentSignatures[entity.GetId()].reset();

    // Bump before the id is reusable: every handle to the old entity becomes
    // detectably stale at exactly the moment the id can be handed out again.
    ++generations[entity.GetId()];

    // Make the entity id available to be reused
    freeIds.push_back(entity.GetId());
  }
  entitiesToBeKilled.clear();
}
void Registry::TagEntity(Entity entity, const std::string &tag) {
  // One tag per entity, one entity per tag — last write wins on both sides.
  // (emplace would silently no-op and leave the two maps inconsistent.)
  RemoveEntityTag(entity); // drop this entity's previous tag, if any

  auto existing = entityPerTag.find(tag);
  if (existing != entityPerTag.end()) {
    tagPerEntity.erase(existing->second.GetId()); // untag the previous holder
  }

  entityPerTag.insert_or_assign(tag, entity);
  tagPerEntity.insert_or_assign(entity.GetId(), tag);
}

bool Registry::EntityHasTag(Entity entity, const std::string &tag) const {
  if (tagPerEntity.find(entity.GetId()) == tagPerEntity.end()) {
    return false;
  }

  auto it = entityPerTag.find(tag);
  if (it == entityPerTag.end()) {
    return false;
  }
  return it->second == entity;
}

bool Registry::DoesTagExist(const std::string &tag) const {
  return entityPerTag.find(tag) != entityPerTag.end();
}

// PRECONDITION: DoesTagExist(tag). Unlike GetEntitiesByGroup this cannot be
// softened to a silent miss — the return type is Entity, which has no "none"
// value, and inventing Entity(0) would hand back a live, unrelated entity.
// Callers guard with DoesTagExist; see P19 in docs/TECH_DEBT.md.
Entity Registry::GetEntityByTag(const std::string &tag) const {
  return entityPerTag.at(tag);
}

const Entity *Registry::TryGetEntityByTag(const std::string &tag) const {
  auto found = entityPerTag.find(tag);
  if (found == entityPerTag.end()) {
    return nullptr;
  }
  return &found->second;
}

void Registry::RemoveEntityTag(Entity entity) {
  auto taggedEntity = tagPerEntity.find(entity.GetId());
  if (taggedEntity != tagPerEntity.end()) {
    auto tag = taggedEntity->second;
    entityPerTag.erase(tag);
    tagPerEntity.erase(taggedEntity);
  }
}

void Registry::GroupEntity(Entity entity, const std::string &group) {
  // One group per entity — re-grouping moves the entity, so a later kill
  // can't leave it stranded in a group groupPerEntity no longer records.
  RemoveEntityGroup(entity);

  entitiesPerGroup.emplace(group, std::set<Entity>());
  entitiesPerGroup[group].emplace(entity);
  groupPerEntity.insert_or_assign(entity.GetId(), group);
}

bool Registry::EntityBelongsToGroup(Entity entity,
                                    const std::string &group) const {
  auto it = entitiesPerGroup.find(group);
  if (it == entitiesPerGroup.end()) {
    return false;
  }

  const auto &groupEntities = it->second; // by reference — don't copy the set

  return groupEntities.find(entity) != groupEntities.end();
}

std::vector<Entity>
Registry::GetEntitiesByGroup(const std::string &group) const {
  // find rather than .at: a group nobody has joined is not an error, and .at
  // throws, which under -fno-exceptions (the Switch build) terminates the
  // process. Matches AssetStore::GetTexture's miss behaviour.
  auto it = entitiesPerGroup.find(group);
  if (it == entitiesPerGroup.end()) {
    static unsigned int reports = 0;
    if (EcsShouldReport(reports)) {
      logger.Err("GetEntitiesByGroup: group '" + group +
                 "' does not exist; returning an empty list" +
                 EcsSuppressionNote(reports));
    }
    return std::vector<Entity>();
  }

  const auto &setOfEntities = it->second;

  return std::vector<Entity>(
      setOfEntities.begin(),
      setOfEntities.end()); // This create a new vector from the set
}

bool Registry::DoesGroupExist(const std::string &group) const {
  // If the group exists
  if (entitiesPerGroup.find(group) != entitiesPerGroup.end())
    return true;

  return false;
}

void Registry::RemoveEntityGroup(Entity entity) {
  // If in group, remove entity from group management
  auto groupedEntity = groupPerEntity.find(entity.GetId());
  if (groupedEntity != groupPerEntity.end()) {
    auto group = entitiesPerGroup.find(groupedEntity->second);
    if (group != entitiesPerGroup.end()) {
      auto entityInGroup = group->second.find(entity);
      if (entityInGroup != group->second.end()) {
        group->second.erase(entityInGroup);
      }
    }
    groupPerEntity.erase(groupedEntity);
  }
}

Registry &Registry::Instance() {
  if (instance == nullptr) {
    instance.reset(new Registry());
  }
  return *instance;
}