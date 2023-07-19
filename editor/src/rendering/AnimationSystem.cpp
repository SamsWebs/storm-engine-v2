#include "AnimationSystem.h"

AnimationSystem::AnimationSystem() {
  RequireComponent<AnimationComponent>();
  RequireComponent<SpriteComponent>();
  RequireComponent<TransformComponent>();
}

void AnimationSystem::Update() {
  for (const auto &entity : GetSystemEntities()) {
    const auto &transform = entity.GetComponent<TransformComponent>();

    auto &animation = entity.GetComponent<AnimationComponent>();
    auto &sprite = entity.GetComponent<SpriteComponent>();

    // Set the cuurent frame
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
