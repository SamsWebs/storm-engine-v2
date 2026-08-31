#pragma once

#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/logger.h>

#include "../MouseControl.h"
#include "ICommand.h"

#include <optional>

using namespace storm;

class AddTileCommand : public ICommand {
private:
  std::shared_ptr<class MouseControl> mMouseControl;
// Stores the Entity rather than its id. Entity ids are recycled, so an id in
// an undo stack is not an identity: delete a tile, undo, delete it again, place
// a new tile that takes the recycled id, then redo -- and the redo matches the
// NEW tile and kills it. Entity::operator== compares id and generation, so a
// recycled id no longer matches a stale handle.
  std::optional<Entity> mTile;
  bool mCollider, mAnimated;
  BoxColliderComponent mBoxColliderComponent;
  TransformComponent mTransformComponent;
  SpriteComponent mSpriteComponent;
  AnimationComponent mAnimationComponent;
  Logger logger;

public:
  AddTileCommand(std::shared_ptr<class MouseControl> &mouseControl);
  virtual void Execute();
  virtual void Undo();
  virtual void Redo();
};