#include "RemoveTileCommand.h"

RemoveTileCommand::RemoveTileCommand(
    std::shared_ptr<MouseControl> &mouseControl)
    : mMouseControl(mouseControl), mTileId(kNoTile), mCollider(false),
      mAnimated(false), mBoxColliderComponent(), mTransformComponent(),
      mSpriteComponent(), mAnimationComponent() {}

void RemoveTileCommand::Execute() {
  mBoxColliderComponent = mMouseControl->GetRemovedBoxComponent();

  if (mBoxColliderComponent.width == 0 && mBoxColliderComponent.height == 0 &&
      mBoxColliderComponent.offset == glm::vec2(0))
    mCollider = false;
  else
    mCollider = true;

  mTransformComponent = mMouseControl->GetRemovedTransform();
  mSpriteComponent = mMouseControl->GetRemovedSpriteComponent();

  mAnimationComponent = mMouseControl->GetRemovedAnimationComponent();

  if (mAnimationComponent.numFrames > 1)
    mAnimated = true;
  else
    mAnimated = false;
}

void RemoveTileCommand::Undo() {
  // Create a new tile based on the removed tile
  Entity newEntity = Registry::Instance().CreateEntity();
  newEntity.Group("tiles");
  newEntity.AddComponent<TransformComponent>(mTransformComponent);
  newEntity.AddComponent<SpriteComponent>(mSpriteComponent);
  // If there is a collider, add the collider
  if (mCollider)
    newEntity.AddComponent<BoxColliderComponent>(mBoxColliderComponent);

  if (mAnimated)
    newEntity.AddComponent<AnimationComponent>(mAnimationComponent);

  // The Tile id is need for the redo so we can remove the tile
  mTileId = newEntity.GetId();
}

// Redo removes the tile again
void RemoveTileCommand::Redo() {
  // Undo never ran, so there is no tile to remove again. This compared
  // against 0 while the unset value was (size_t)-1, so it both failed to
  // catch the unset case and skipped the tile that really did have id 0.
  if (mTileId == kNoTile)
    return;

  auto entities = Registry::Instance().GetEntitiesByGroup("tiles");

  for (auto &entity : entities) {
    if (entity.GetId() == mTileId) {
      entity.Kill();
      logger.Log("REMOVE: Tile " + std::to_string(mTileId) +
                 " has been removed!");
      mTileId = kNoTile;
    }
  }
}
