#include <algorithm>

#include "ecs.h"

int Entity::GetId() const { return id; }

void System::AddEntityToSystem(Entity entity) { entities.push_back(entity); }

void System::RemoveEntityFromSystem(Entity entity) {
  entities.erase(std::remove_if(entities.begin(), entities.end(),
                                [&](const Entity &e) {
                                  return e.GetId() == entity.GetId();
                                }),
                 entities.end());
}

std::vector<Entity> System::GetSystemEntities() const { return entities; }

const Signature &System::GetComponentSignature() const {
  return componentSignature;
}