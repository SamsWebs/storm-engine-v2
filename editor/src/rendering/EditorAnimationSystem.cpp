#include "EditorAnimationSystem.h"

// Safe here and not in the header: a using-directive in a .cpp is scoped to
// this translation unit.
using namespace storm;

EditorAnimationSystem::EditorAnimationSystem() {
  RequireComponent<AnimationComponent>();
  RequireComponent<SpriteComponent>();
  RequireComponent<TransformComponent>();
}

void EditorAnimationSystem::Update() {
  for (const auto &entity : GetSystemEntities()) {
    const auto &transform = entity.GetComponent<TransformComponent>();

    auto &animation = entity.GetComponent<AnimationComponent>();
    auto &sprite = entity.GetComponent<SpriteComponent>();

    // The animation panel writes numFrames straight through, so zero reaches
    // here and the modulo below would raise SIGFPE and take the unsaved map
    // with it. common/systems/animation.h guards the same case.
    if (animation.numFrames <= 0)
      continue;

    // Set the current frame
    animation.currentFrame = ((SDL_GetTicks() - animation.startTime) *
                              animation.frameSpeedRate / 1000) %
                             animation.numFrames;

    // If the animation is a vertical scroll use this
    if (animation.vertical) {
      sprite.srcRect.y = animation.currentFrame * sprite.height;
    } else {
      sprite.srcRect.x =
          (animation.currentFrame * sprite.width) + animation.frameOffset;
    }
  }
}
