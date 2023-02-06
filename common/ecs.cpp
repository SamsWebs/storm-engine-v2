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

// template <typename T> Pool<T>::Pool(int size) { data.resize(size); }

// template <typename T> bool Pool<T>::isEmpty() const { return data.empty(); }

// template <typename T> int Pool<T>::GetSize() const { return data.size(); }

// template <typename T> void Pool<T>::Resize(int n) { data.resize(n); }

// template <typename T> void Pool<T>::Clear() { data.clear(); }

// template <typename T> void Pool<T>::Add(T object) { data.push_back(object); }

// template <typename T> void Pool<T>::Set(int index, T object) {
//   data[index] = object;
// }

// template <typename T> T &Pool<T>::Get(int index) {
//   return static_cast<T &>(data[index]);
// }

// template <typename T> T &Pool<T>::operator[](unsigned int index) {
//   return data[index];
// }
