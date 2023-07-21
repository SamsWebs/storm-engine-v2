#include "ecs.h"

std::size_t IComponent::nextId = 0;
std::unique_ptr<Registry> Registry::instance = nullptr;

std::size_t Entity::GetId() const { return id; }

void Entity::Kill() { registry->KillEntity(*this); }

void Entity::Tag(const std::string &tag) { registry->TagEntity(*this, tag); }

bool Entity::HasTag(const std::string &tag) const {
  return registry->EntityHasTag(*this, tag);
}

void Entity::Group(const std::string &group) {
  registry->GroupEntity(*this, group);
}

bool Entity::BelongsToGroup(const std::string &group) const {
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

  logger.Log("Entity created with id = " + std::to_string(entityId));

  return entity;
}

void Registry::KillEntity(Entity entity) { entitiesToBeKilled.insert(entity); }

void Registry::AddEntityToSystems(Entity entity) {
  const auto entityId = entity.GetId();

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

    entityComponentSignatures[entity.GetId()].reset();

    // Make the entity id available to be reused
    freeIds.push_back(entity.GetId());
  }
  entitiesToBeKilled.clear();
}
void Registry::TagEntity(Entity entity, const std::string &tag) {
  entityPerTag.emplace(tag, entity);
  tagPerEntity.emplace(entity.GetId(), tag);
}

bool Registry::EntityHasTag(Entity entity, const std::string &tag) const {
  if (tagPerEntity.find(entity.GetId()) == tagPerEntity.end()) {
    return false;
  }

  return entityPerTag.find(tag)->second == entity;
}

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
  entitiesPerGroup.emplace(group, std::set<Entity>());
  entitiesPerGroup[group].emplace(entity);
  groupPerEntity.emplace(entity.GetId(), group);
}

bool Registry::EntityBelongsToGroup(Entity entity,
                                    const std::string &group) const {
  // Error checking and Validation

  if (entitiesPerGroup.find(group) == entitiesPerGroup.end()) {
    // Logger::Err("The Group [" + group + "]  Does Not Exist");
    return false;
  }

  auto groupEntities = entitiesPerGroup.at(group);

  return groupEntities.find(entity.GetId()) != groupEntities.end();
}

std::vector<Entity>
Registry::GetEntitiesByGroup(const std::string &group) const {
  auto &setOfEntities = entitiesPerGroup.at(group);

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