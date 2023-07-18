#pragma once

#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>

#include "../MouseControl.h"
#include "ICommand.h"

class RemoveTileCommand : public ICommand {
private:
  std::shared_ptr<class MouseControl> mMouseControl;

  int mTileId;
  bool mCollider, mAnimated;

  BoxColliderComponent mBoxColliderComponent;
  TransformComponent mTransformComponent;
  SpriteComponent mSpriteComponent;
  AnimationComponent mAnimationComponent;

public:
  RemoveTileCommand(std::shared_ptr<class MouseControl> &mouseControl);
  virtual void Execute() override;
  virtual void Undo() override;
  virtual void Redo() override;
};