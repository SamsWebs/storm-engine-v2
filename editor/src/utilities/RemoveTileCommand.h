#pragma once

#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>

#include "../AssetManager.h"
#include "../MouseControl.h"
#include "ICommand.h"

class RemoveTileCommand : public ICommand {
private:
  std::shared_ptr<class MouseControl> mMouseControl;

  std::size_t mTileId;
  bool mCollider, mAnimated;

  BoxColliderComponent mBoxColliderComponent;
  TransformComponent mTransformComponent;
  SpriteComponent mSpriteComponent;
  AnimationComponent mAnimationComponent;
  Logger logger;

public:
  RemoveTileCommand(std::shared_ptr<class MouseControl> &mouseControl);
  virtual void Execute();
  virtual void Undo();
  virtual void Redo();
};