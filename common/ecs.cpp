#include "ecs.h"

std::size_t IComponent::nextId = 0;
std::unique_ptr<Registry> Registry::instance = nullptr;

const char *EcsSuppressionNote(unsigned int counter) {
  return counter >= ECS_MAX_DIAGNOSTIC_REPORTS
             ? " (further identical reports suppressed)"
             : "";
}

void EcsReportErr(const std::string &message) {
  static Logger ecsLogger;
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
  unsigned int missingUpdateReports = 0;
};

std::unordered_map<const Registry *, RegistryDiagnostics> &
DiagnosticsTable() {
  static std::unordered_map<const Registry *, RegistryDiagnostics> table;
  return table;
}

// The entities a system registered now would have to be told about: live,
// past admission, matching the signature, not already members.
template <typename TVisitor>
void ForEachMissedEntity(const Registry &registry, std::size_t numEntities,
                         const std::vector<Signature> &signatures,
                         const System &system, TVisitor &&visit) {
  const Signature &required = system.GetComponentSignature();
  const std::vector<Entity> &members =
      const_cast<System &>(system).GetSystemEntities();

  for (std::size_t id = 0; id < numEntities && id < signatures.size(); ++id) {
    Entity entity(id);
    if (!registry.IsAlive(entity)) {
      continue;
    }
    if (registry.IsPendingAdmission(entity)) {
      continue;
    }
    if ((signatures[id] & required) != required) {
      continue;
    }
    if (std::find(members.begin(), members.end(), entity) != members.end()) {
      continue;
    }
    visit(entity);
  }
}

} // namespace

Registry::~Registry() {
  DiagnosticsTable().erase(this);
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

  Entity entity(entityId);
  entity.registry = this;
  entitiesToBeAdded.insert(entity);

  RegistryDiagnostics &diagnostics = DiagnosticsTable()[this];
  if (diagnostics.updateCalls == 0 &&
      entitiesToBeAdded.size() >= ECS_PENDING_ENTITY_WARNING_THRESHOLD &&
      EcsShouldReport(diagnostics.missingUpdateReports)) {
    logger.Err(
        "CreateEntity: " + std::to_string(entitiesToBeAdded.size()) +
        " entities are waiting to be admitted and Registry::Update() has "
        "never been called on this registry. No entity has joined any system, "
        "so nothing will render or move. Call registry.Update() first in your "
        "state's update()." +
        EcsSuppressionNote(diagnostics.missingUpdateReports));
  }

  logger.Log("Entity created with id = " + std::to_string(entityId));

  return entity;
}

bool Registry::IsAlive(Entity entity) const {
  const auto entityId = entity.GetId();

  // An id is in use once CreateEntity has handed it out (id < numEntities) and
  // for as long as Update()'s kill flush has not parked it back in freeIds.
  // CreateEntity pops it off freeIds again when it recycles the id.
  if (entityId >= numEntities) {
    return false;
  }

  return std::find(freeIds.begin(), freeIds.end(),
                   static_cast<int>(entityId)) == freeIds.end();
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
  ForEachMissedEntity(*this, numEntities, entityComponentSignatures, system,
                      [&missed](Entity) { ++missed; });
  return missed;
}

std::size_t Registry::AdmitExistingEntitiesTo(System &system) {
  std::vector<Entity> toAdmit;
  ForEachMissedEntity(*this, numEntities, entityComponentSignatures, system,
                      [&toAdmit](Entity entity) { toAdmit.push_back(entity); });
  for (Entity entity : toAdmit) {
    system.AddEntityToSystem(entity);
  }
  return toAdmit.size();
}

void Registry::KillEntity(Entity entity) {
  const auto entityId = entity.GetId();

  // One liveness test covers both rejections this needs: an id that was never
  // created, and an id already recycled into freeIds (the double-kill that
  // used to alias two live entities onto one id).
  //
  // KNOWN GAP: ids are recycled, so a stale handle whose id has since been
  // handed to a new entity reads as alive and kills that new entity instead.
  // Closing it needs a generation counter inside Entity, which changes
  // sizeof(Entity) — an ABI break, tracked as P5 in docs/TECH_DEBT.md.
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